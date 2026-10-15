/* Portable camera slot leaf routines. See native_camera.h / native_camera.md. */
#include "native_camera.h"

uint16_t ae_camera_u16(const AeCameraSlot *s, size_t off)
{
    return (uint16_t)(s->raw[off] | ((uint16_t)s->raw[off + 1] << 8));
}

int16_t ae_camera_i16(const AeCameraSlot *s, size_t off)
{
    uint16_t v = ae_camera_u16(s, off);
    return v >= 0x8000u ? (int16_t)((int32_t)v - 0x10000) : (int16_t)v;
}

static uint32_t slot_u32(const AeCameraSlot *s, size_t off)
{
    return (uint32_t)s->raw[off] | ((uint32_t)s->raw[off + 1] << 8) |
           ((uint32_t)s->raw[off + 2] << 16) | ((uint32_t)s->raw[off + 3] << 24);
}

static int32_t i32_from_u32(uint32_t v)
{
    if (v & 0x80000000u)
        return (int32_t)(v & 0x7FFFFFFFu) - 0x7FFFFFFF - 1;
    return (int32_t)v;
}

int32_t ae_camera_i32(const AeCameraSlot *s, size_t off)
{
    return i32_from_u32(slot_u32(s, off));
}

void ae_camera_set16(AeCameraSlot *s, size_t off, int32_t value)
{
    uint32_t v = (uint32_t)value;
    s->raw[off] = (uint8_t)(v & 0xFFu);
    s->raw[off + 1] = (uint8_t)((v >> 8) & 0xFFu);
}

void ae_camera_set32(AeCameraSlot *s, size_t off, int32_t value)
{
    uint32_t v = (uint32_t)value;
    s->raw[off] = (uint8_t)(v & 0xFFu);
    s->raw[off + 1] = (uint8_t)((v >> 8) & 0xFFu);
    s->raw[off + 2] = (uint8_t)((v >> 16) & 0xFFu);
    s->raw[off + 3] = (uint8_t)((v >> 24) & 0xFFu);
}

/* Arithmetic right shift of a possibly negative value (floor), without
 * relying on implementation-defined behaviour. */
static int32_t floor_shr(int32_t v, int n)
{
    return v >= 0 ? (v >> n) : (-1 - ((-1 - v) >> n));
}

void ae_camera_clear_script(AeCameraSlot *s)
{
    s->raw[AE_CAM_SCRIPT_STATE] = 0;
    s->raw[AE_CAM_SCRIPT_STATE + 1] = 0;
    s->raw[AE_CAM_SCRIPT_STATE + 2] = 0;
    s->raw[AE_CAM_SCRIPT_ACTIVE] = 0;
    s->raw[AE_CAM_FIXED_ACTIVE] = 0;
    ae_camera_set16(s, AE_CAM_SCRIPT_STEP, 0);
    ae_camera_set16(s, AE_CAM_SCRIPT_TIMER, 0);
    ae_camera_set32(s, AE_CAM_SCRIPT, 0);
}

void ae_camera_reset_follow(AeCameraSlot *s, const int32_t pos[3],
                            const int16_t facing[3], int32_t original_yaw)
{
    int i;
    int32_t fx = facing[0];
    int32_t fz = facing[2];

    ae_camera_set32(s, AE_CAM_YAW, original_yaw);
    ae_camera_set32(s, AE_CAM_DIST, 0x460);
    ae_camera_set16(s, AE_CAM_ORBIT_Y, 0);
    s->raw[AE_CAM_STILL_FRAMES] = 0;
    s->raw[AE_CAM_YAW_HOLD] = 0;
    s->raw[AE_CAM_MODE_TIMER] = 0;
    s->raw[AE_CAM_CLAMP_STATE] = 0;
    s->raw[AE_CAM_CLAMP_FLAG] = 0;
    s->raw[AE_CAM_CLAMP_COUNT] = 0;
    s->raw[AE_CAM_CLAMP_TIMER] = 0;
    s->raw[AE_CAM_FOLLOW_FLAG_A] = 0;
    s->raw[AE_CAM_FOLLOW_FLAG_B] = 1;
    ae_camera_set32(s, AE_CAM_SAVED_YAW, original_yaw);
    for (i = 0; i < 3; i++) {
        ae_camera_set16(s, AE_CAM_LAST_POS + 2 * i, pos[i]);
        ae_camera_set16(s, AE_CAM_EYE_VEL + 2 * i, 0);
        ae_camera_set16(s, AE_CAM_TARGET_VEL + 2 * i, 0);
    }
    /* Low 16 bits of each sum are stored. */
    ae_camera_set16(s, AE_CAM_EYE + 0, (int32_t)((uint32_t)pos[0] - (uint32_t)floor_shr(fx, 1)));
    ae_camera_set16(s, AE_CAM_EYE + 2, (int32_t)((uint32_t)pos[1] + (uint32_t)-0x172));
    ae_camera_set16(s, AE_CAM_EYE + 4, (int32_t)((uint32_t)pos[2] - (uint32_t)floor_shr(fz, 1)));
    ae_camera_set16(s, AE_CAM_TARGET + 0, (int32_t)((uint32_t)pos[0] + (uint32_t)floor_shr(fx, 2)));
    ae_camera_set16(s, AE_CAM_TARGET + 2, (int32_t)((uint32_t)pos[1] + (uint32_t)-0x6E));
    ae_camera_set16(s, AE_CAM_TARGET + 4, (int32_t)((uint32_t)pos[2] + (uint32_t)floor_shr(fz, 2)));
    s->raw[AE_CAM_FIXED_ACTIVE] = 1;
}

void ae_camera_init_follow(AeCameraSlot *s, int32_t kind, const uint8_t eye8[8],
                           const uint8_t target8[8], const int32_t pos[3],
                           const int16_t facing[3], int32_t original_yaw)
{
    int i;

    ae_camera_set16(s, AE_CAM_EYE_HEIGHT, -0x172);
    ae_camera_set16(s, AE_CAM_TARGET_HEIGHT, -0x6E);
    ae_camera_set16(s, AE_CAM_FOLLOW_DIST_H, 0x460);
    for (i = 0; i < 8; i++) {
        s->raw[AE_CAM_EYE + i] = eye8[i];
        s->raw[AE_CAM_TARGET + i] = target8[i];
    }
    ae_camera_set32(s, AE_CAM_DIST, 0x460);
    ae_camera_set32(s, AE_CAM_UNK_2C, 0);
    ae_camera_set16(s, AE_CAM_ORBIT_X, 0);
    if (kind == 1 || kind == 3)
        ae_camera_reset_follow(s, pos, facing, original_yaw);
    else if (kind == 4 || kind == 5)
        ae_camera_clear_script(s);
}

/* One axis: (cur * (div - 1) + desired) / div with 32-bit wraparound.
 * Returns 0 (and writes nothing) where the original traps. */
static int smooth_axis(AeCameraSlot *s, size_t off, int16_t desired, int32_t div)
{
    int32_t cur = ae_camera_i16(s, off);
    uint32_t sum = (uint32_t)cur * ((uint32_t)div - 1u) + (uint32_t)(int32_t)desired;
    int32_t n = i32_from_u32(sum);
    int32_t q;

    if (div == 0)
        return 0;
    if (div == -1) {
        if (n == -0x7FFFFFFF - 1)
            return 0;
        q = -n;
    } else {
        q = n / div;
    }
    ae_camera_set16(s, off, q);
    return 1;
}

static int smooth3(AeCameraSlot *s, size_t base, const int16_t desired[3], const int32_t div[3])
{
    int i;
    for (i = 0; i < 3; i++)
        if (!smooth_axis(s, base + 2 * (size_t)i, desired[i], div[i]))
            return 0;
    return 1;
}

int ae_camera_smooth_eye(AeCameraSlot *s, const int16_t desired[3], const int32_t div[3])
{
    return smooth3(s, AE_CAM_EYE, desired, div);
}

int ae_camera_smooth_target(AeCameraSlot *s, const int16_t desired[3], const int32_t div[3])
{
    return smooth3(s, AE_CAM_TARGET, desired, div);
}

static int16_t wrap_s16(int32_t v)
{
    uint32_t u = (uint32_t)v & 0xFFFFu;
    return u >= 0x8000u ? (int16_t)((int32_t)u - 0x10000) : (int16_t)u;
}

void ae_camera_stick_orbit(AeCameraSlot *s, int can_exit, int fixed_active,
                           uint16_t stick_mag, int8_t stick_x, int8_t stick_y,
                           uint32_t held, int16_t player_y)
{
    int32_t cx = 0, cy = 0;
    int32_t eye_y;

    if (!can_exit)
        return;
    if (fixed_active == 1 && stick_mag > 100) {
        cx = stick_x;
        cy = stick_y;
    }

    ae_camera_set16(s, AE_CAM_ORBIT_Y, 0);
    if (cx != 0 || (s->raw[AE_CAM_CLAMP_STATE] == 0 && s->raw[AE_CAM_MODE_TIMER] == 0)) {
        int32_t v = 0;
        int apply = 1;
        if ((held & 0x2000u) || cx > 0)
            v = 0x22;
        else if ((held & 0x8000u) || cx < 0)
            v = -0x22;
        else
            apply = 0;
        if (apply) {
            ae_camera_set16(s, AE_CAM_ORBIT_Y, v);
            s->raw[AE_CAM_STILL_FRAMES] = 0;
            s->raw[AE_CAM_CLAMP_FLAG] = 0;
        }
    }

    ae_camera_set16(s, AE_CAM_ORBIT_X, 0);
    eye_y = ae_camera_i16(s, AE_CAM_EYE + 2);
    if ((held & 0x1000u) || cy > 0) {
        uint32_t lim = ((uint32_t)(int32_t)player_y - 0x404u) & 0xFFFFu;
        ae_camera_set16(s, AE_CAM_ORBIT_X, -0x30);
        if (eye_y - 0x30 < wrap_s16((int32_t)lim))
            ae_camera_set16(s, AE_CAM_ORBIT_X, (int32_t)(lim - ((uint32_t)eye_y & 0xFFFFu)));
    }
    if ((held & 0x4000u) || cy < 0) {
        uint32_t lim = ((uint32_t)(int32_t)player_y + 0x100u) & 0xFFFFu;
        ae_camera_set16(s, AE_CAM_ORBIT_X, 0x30);
        if (wrap_s16((int32_t)lim) < eye_y + 0x30)
            ae_camera_set16(s, AE_CAM_ORBIT_X, (int32_t)(lim - ((uint32_t)eye_y & 0xFFFFu)));
    }
}
