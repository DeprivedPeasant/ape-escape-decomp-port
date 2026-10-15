/* Original CameraSlotUpdate (0x8004CA70), see native_camera_motion.md.
 * All arithmetic is explicit wrapping uint32; signed conversions go through
 * portable helpers. No floating point. */
#include "native_camera_motion.h"

#include <string.h>

/* Offsets used here that native_camera.h does not name. */
enum {
    MOT_SCRIPT_RATE  = 0x8C,  /* s16 */
    MOT_SCRIPT_STATE = 0x90,  /* u8, switch value */
    MOT_FOCUS_MODE   = 0x91,  /* u8, CameraSlotGetFocusPoint mode */
    MOT_SCRIPT_FOCUS = 0x94,  /* s16[3] */
    MOT_SCRIPT_VEL   = 0x9C,  /* s16[3] */
    MOT_ANCHOR       = 0xA8   /* 8 bytes: s16 x, y, z + padding */
};

static int32_t mot_sext16(uint32_t v)
{
    v &= 0xFFFFu;
    return v >= 0x8000u ? (int32_t)v - 0x10000 : (int32_t)v;
}

static int32_t mot_i32(uint32_t v)
{
    return v <= 0x7FFFFFFFu ? (int32_t)v : -(int32_t)(~v) - 1;
}

/* Arithmetic shift right of a 32 bit pattern. */
static uint32_t mot_sra(uint32_t v, unsigned n)
{
    return (v & 0x80000000u) ? ~((~v) >> n) : v >> n;
}

static uint32_t mot_rd16(const uint8_t *b, size_t off)
{
    return (uint32_t)b[off] | ((uint32_t)b[off + 1] << 8);
}

static void mot_wr16(uint8_t *b, size_t off, uint32_t v)
{
    b[off] = (uint8_t)(v & 0xFFu);
    b[off + 1] = (uint8_t)((v >> 8) & 0xFFu);
}

/* Signed halfword at the slot offset, as a 32 bit pattern. */
static uint32_t mot_slot_s16(const AeCameraSlot *s, size_t off)
{
    return (uint32_t)mot_sext16(mot_rd16(s->raw, off));
}

/* Truncate-toward-zero halving of (signed16 + signed16), as the original's
 * srl 31 / addu / sra 1 sequence. The sum always fits an int32. */
static uint32_t mot_half_sum(int32_t a, int32_t b)
{
    int32_t sum = a + b;
    return (uint32_t)(sum / 2);
}

/* Shared prelude of states 8 and 9: F = normalize(player.lo16 - E, with
 * y + 0x80). F is the 8 byte focus buffer, E the 8 byte old-eye buffer. */
static int mot_player_direction(uint8_t F[8], const uint8_t E[8], const int32_t player[3],
                                const AeCameraMathTables *tables)
{
    int16_t v[3];
    uint32_t d;

    d = (uint32_t)player[0] - mot_rd16(E, 0);
    mot_wr16(F, 0, d);
    d = (uint32_t)player[1] - (mot_rd16(E, 2) + 0x80u);
    mot_wr16(F, 2, d);
    d = (uint32_t)player[2] - mot_rd16(E, 4);
    mot_wr16(F, 4, d);

    v[0] = (int16_t)mot_sext16(mot_rd16(F, 0));
    v[1] = (int16_t)mot_sext16(mot_rd16(F, 2));
    v[2] = (int16_t)mot_sext16(mot_rd16(F, 4));
    if (!tables || !ae_camera_normalize(tables, v, NULL))
        return 0;
    mot_wr16(F, 0, (uint32_t)(int32_t)v[0]);
    mot_wr16(F, 2, (uint32_t)(int32_t)v[1]);
    mot_wr16(F, 4, (uint32_t)(int32_t)v[2]);
    return 1;
}

/* F = base + (normalized + scriptFocus) / 2 per axis; base is E (8 bytes). */
static void mot_blend_focus(uint8_t F[8], const uint8_t base[8], const AeCameraSlot *s)
{
    size_t i;
    for (i = 0; i < 3; i++) {
        uint32_t h = mot_half_sum(mot_sext16(mot_rd16(F, i * 2)),
                                  mot_sext16(mot_rd16(s->raw, MOT_SCRIPT_FOCUS + i * 2)));
        mot_wr16(F, i * 2, mot_rd16(base, i * 2) + h);
    }
}

int ae_camera_slot_update(AeCameraSlot *slot, const int32_t player[3], uint8_t stereo,
                          const AeCameraMathTables *tables,
                          const AeCameraMotionQueries *queries)
{
    AeCameraSlot tmp;
    uint8_t F[8]; /* sp+0x10: focus / target */
    uint8_t E[8]; /* sp+0x18: eye */
    int32_t timer;
    uint8_t script_active;
    uint8_t mode;
    uint8_t state;

    if (!slot)
        return 0;
    tmp = *slot;

    tmp.raw[AE_CAM_FIXED_ACTIVE] = 0;
    timer = ae_camera_i16(&tmp, AE_CAM_SCRIPT_TIMER);
    if (timer == 0) {
        *slot = tmp;
        return 1;
    }
    if (timer > 0)
        ae_camera_set16(&tmp, AE_CAM_SCRIPT_TIMER, timer - 1);

    script_active = tmp.raw[AE_CAM_SCRIPT_ACTIVE];
    if (script_active != 0) {
        if (!queries || !queries->indexed ||
            !queries->indexed(queries->user, &tmp, script_active))
            return 0;
        *slot = tmp;
        return 1;
    }

    if (!queries || !queries->focus)
        return 0;
    mode = tmp.raw[MOT_FOCUS_MODE];
    if (!queries->focus(queries->user, &tmp, mode, F))
        return 0;
    memcpy(E, tmp.raw + AE_CAM_EYE, 8);

    state = tmp.raw[MOT_SCRIPT_STATE];
    switch (state) {
    case 2:
        /* Eye is a second focus query, same mode. */
        if (!queries->focus(queries->user, &tmp, mode, E))
            return 0;
        break;
    case 5: {
        int32_t yaw = ae_camera_i32(&tmp, AE_CAM_YAW);
        uint32_t dist = (uint32_t)ae_camera_i32(&tmp, AE_CAM_DIST);
        uint32_t v1;
        uint32_t q;
        uint32_t prod;

        if (!tables || !tables->loaded)
            return 0;
        prod = (uint32_t)ae_camera_rcos(tables, yaw) * dist;
        mot_wr16(E, 0, mot_rd16(F, 0) + mot_sra(prod, 12));
        prod = (uint32_t)ae_camera_rsin(tables, yaw) * dist;
        mot_wr16(E, 4, mot_rd16(F, 4) + mot_sra(prod, 12));

        v1 = (uint32_t)yaw + (mot_slot_s16(&tmp, MOT_SCRIPT_RATE) + 0x1000u);
        q = (v1 & 0x80000000u) ? v1 + 0xFFFu : v1;
        q = mot_sra(q, 12) << 12;
        ae_camera_set32(&tmp, AE_CAM_YAW, mot_i32(v1 - q));

        mot_wr16(E, 2, mot_rd16(E, 2) + mot_rd16(tmp.raw, MOT_SCRIPT_VEL + 2));
        ae_camera_set32(&tmp, AE_CAM_DIST,
                        mot_i32(dist + mot_slot_s16(&tmp, MOT_SCRIPT_VEL)));
        break;
    }
    case 7: {
        size_t i;
        for (i = 0; i < 3; i++)
            mot_wr16(E, i * 2, mot_rd16(E, i * 2) + mot_rd16(tmp.raw, MOT_SCRIPT_VEL + i * 2));
        break;
    }
    case 8: {
        uint32_t a;
        uint32_t d;

        if (!player || !mot_player_direction(F, E, player, tables))
            return 0;
        /* Full 32 bit player position against the sign-extended anchor. */
        a = mot_slot_s16(&tmp, MOT_ANCHOR);
        d = (uint32_t)player[0] - a;
        mot_wr16(E, 0, mot_rd16(tmp.raw, MOT_ANCHOR) + (uint32_t)(mot_i32(d) / 4));
        a = mot_slot_s16(&tmp, MOT_ANCHOR + 4);
        d = (uint32_t)player[2] - a;
        mot_wr16(E, 4, mot_rd16(tmp.raw, MOT_ANCHOR + 4) + (uint32_t)(mot_i32(d) / 4));
        /* E.y keeps the old eye value. */
        mot_blend_focus(F, E, &tmp);
        break;
    }
    case 9:
        if (!player || !mot_player_direction(F, E, player, tables))
            return 0;
        memcpy(E, tmp.raw + MOT_ANCHOR, 8);
        mot_blend_focus(F, E, &tmp);
        tmp.raw[AE_CAM_FIXED_ACTIVE] = 1;
        break;
    default:
        break;
    }

    if (stereo != 0) {
        mot_wr16(F, 2, mot_rd16(F, 2) + 3u);
        mot_wr16(E, 2, mot_rd16(E, 2) - 6u);
    }

    memcpy(tmp.raw + AE_CAM_TARGET, F, 8);
    memcpy(tmp.raw + AE_CAM_EYE, E, 8);
    *slot = tmp;
    return 1;
}
