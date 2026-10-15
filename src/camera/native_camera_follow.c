/* Portable CameraFollowUpdate / CameraSlotUpdateYaw / CameraSlotAimAtFocus.
 * See native_camera_follow.h / native_camera_follow.md. Written from the
 * original instruction listings (0x800495F4, 0x80048DD0, 0x80047360). */
#include "native_camera_follow.h"

#include <string.h>

/* Scratchpad offsets (0x1F800000 + n). */
enum {
    SC_DESIRED_EYE   = 0x00, /* s16 x, y, z (+ pad at 0x06) */
    SC_DESIRED_TGT   = 0x08, /* s16 x at 0x08, y at 0x0A, z at 0x0C */
    SC_PLAYER_X      = 0x10,
    SC_PLAYER_Z      = 0x14,
    SC_FOCUS         = 0x18, /* s16 x, y, z */
    SC_PLAYER_Y      = 0x1A,
    SC_GROUND_REF    = 0x2C, /* s32 */
    SC_DIR           = 0x40, /* s16 x at 0x40, z at 0x44 (words copied whole) */
    SC_PROJ_X        = 0x60,
    SC_PROJ_Y        = 0x64,
    SC_V68           = 0x68, /* s32 (original g_PlayerScratch + 0x20) */
    SC_V6C           = 0x6C, /* s32 (+ 0x24) */
    SC_V70           = 0x70, /* s32 (+ 0x28) */
    SC_AIM_CROSS     = 0x74,
    SC_AIM_DOT       = 0x78
};

/* Control globals (0x800E68D0 + n). */
enum {
    CTL_EYE_HEIGHT   = 0x34, /* u16 at 0x800E6904 */
    CTL_FOLLOW_DIST  = 0x36, /* u16 at 0x800E6906 */
    CTL_FIXED_ACTIVE = 0x38, /* 0x800E6908 */
    CTL_EYE_ON       = 0x3F, /* 0x800E690F */
    CTL_DIST_ON      = 0x40, /* 0x800E6910 */
    CTL_AIM_MODE     = 0x46, /* 0x800E6916 */
    CTL_GUARD        = 0x47, /* 0x800E6917 */
    CTL_STAGE_MODE   = 0x48  /* 0x800E6918 */
};

/* Slot offsets not named in native_camera.h. */
enum {
    SL_EYE_X = AE_CAM_EYE, SL_EYE_Y = AE_CAM_EYE + 2, SL_EYE_Z = AE_CAM_EYE + 4,
    SL_TGT_X = AE_CAM_TARGET, SL_TGT_Y = AE_CAM_TARGET + 2, SL_TGT_Z = AE_CAM_TARGET + 4,
    SL_TARGET_YAW = 0x48,
    SL_YAW_DELTA = 0x4C,
    SL_EYE_VEL_Y = AE_CAM_EYE_VEL + 2,
    SL_TGT_VEL_Y = AE_CAM_TARGET_VEL + 2,
    SL_SIDE = 0x60,
    SL_DIR_X = 0x64,
    SL_DIR_Z = 0x68,
    SL_HEIGHT_A = 0x70,
    SL_HEIGHT_B = 0x74
};

typedef struct FollowCtx {
    AeCameraSlot *s;
    AeCameraFollowFrame *f;
    const AeCameraMathTables *t;
    const AeCameraQueries *q;
} FollowCtx;

/* ---- wrapped 32 bit / 16 bit helpers ---- */

static int32_t i32_from_u32(uint32_t v)
{
    if (v & 0x80000000u)
        return (int32_t)(v & 0x7FFFFFFFu) - 0x7FFFFFFF - 1;
    return (int32_t)v;
}

/* Low 16 bits of v as a signed value in -32768..32767. */
static int32_t s16_of(uint32_t v)
{
    v &= 0xFFFFu;
    return v >= 0x8000u ? (int32_t)v - 0x10000 : (int32_t)v;
}

static int16_t to_s16(uint32_t v)
{
    return (int16_t)s16_of(v);
}

static int32_t s8_of(uint32_t v)
{
    v &= 0xFFu;
    return v >= 0x80u ? (int32_t)v - 0x100 : (int32_t)v;
}

static int32_t add32(int32_t a, int32_t b)
{
    return i32_from_u32((uint32_t)a + (uint32_t)b);
}

static int32_t sub32(int32_t a, int32_t b)
{
    return i32_from_u32((uint32_t)a - (uint32_t)b);
}

static int32_t mul32(int32_t a, int32_t b)
{
    return i32_from_u32((uint32_t)a * (uint32_t)b);
}

static int32_t neg32(int32_t a)
{
    return i32_from_u32(0u - (uint32_t)a);
}

/* Arithmetic right shift (floor) without implementation-defined behaviour. */
static int32_t floor_shr(int32_t v, int n)
{
    return v >= 0 ? (v >> n) : (-1 - ((-1 - v) >> n));
}

/* The original's "bgez; addiu (2^n - 1); sra n": division by 2^n truncating
 * toward zero (the bias add cannot overflow, it is only applied to negatives). */
static int32_t trunc_shr(int32_t v, int n)
{
    if (v < 0)
        v = add32(v, (1 << n) - 1);
    return floor_shr(v, n);
}

/* High 32 bits of the signed 64 bit product (mult / mfhi). */
static int32_t mulhi(int32_t a, int32_t b)
{
    int64_t p = (int64_t)a * (int64_t)b;
    int64_t h;
    if (p >= 0)
        h = p / 4294967296LL;
    else
        h = -1 - ((-1 - p) / 4294967296LL);
    return (int32_t)h;
}

/* Signed division truncating toward zero; 0 where the original traps. */
static int checked_div(int32_t a, int32_t b, int32_t *out)
{
    if (b == 0)
        return 0;
    if (b == -1 && a == -0x7FFFFFFF - 1)
        return 0;
    *out = a / b;
    return 1;
}

/* v % 4096 as the original computes it ("bgez; addiu 0xfff; sra 12; sll 12; subu"). */
static int32_t mod4096(int32_t v)
{
    int32_t t = v < 0 ? add32(v, 0xFFF) : v;
    return sub32(v, mul32(floor_shr(t, 12), 0x1000));
}

/* Yaw error wrap: ((d + 0x1000) % 4096) as in the original (no re-centering). */
static int32_t wrap_yaw(int32_t d)
{
    return mod4096(add32(d, 0x1000));
}

/* ---- frame accessors (little endian) ---- */

static uint16_t sc_u16(const AeCameraFollowFrame *f, size_t off)
{
    return (uint16_t)(f->scratch[off] | ((uint16_t)f->scratch[off + 1] << 8));
}

static int32_t sc_s16(const AeCameraFollowFrame *f, size_t off)
{
    return s16_of(sc_u16(f, off));
}

static int32_t sc_i32(const AeCameraFollowFrame *f, size_t off)
{
    return i32_from_u32((uint32_t)f->scratch[off] | ((uint32_t)f->scratch[off + 1] << 8) |
                        ((uint32_t)f->scratch[off + 2] << 16) |
                        ((uint32_t)f->scratch[off + 3] << 24));
}

static void sc_set16(AeCameraFollowFrame *f, size_t off, uint32_t v)
{
    f->scratch[off] = (uint8_t)(v & 0xFFu);
    f->scratch[off + 1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void sc_set32(AeCameraFollowFrame *f, size_t off, uint32_t v)
{
    f->scratch[off] = (uint8_t)(v & 0xFFu);
    f->scratch[off + 1] = (uint8_t)((v >> 8) & 0xFFu);
    f->scratch[off + 2] = (uint8_t)((v >> 16) & 0xFFu);
    f->scratch[off + 3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint16_t ctl_u16(const AeCameraFollowFrame *f, size_t off)
{
    return (uint16_t)(f->controls[off] | ((uint16_t)f->controls[off + 1] << 8));
}

static void sset16(AeCameraSlot *s, size_t off, uint32_t v)
{
    ae_camera_set16(s, off, i32_from_u32(v));
}

static void sset32(AeCameraSlot *s, size_t off, uint32_t v)
{
    ae_camera_set32(s, off, i32_from_u32(v));
}

static int32_t sl_s16(const AeCameraSlot *s, size_t off)
{
    return ae_camera_i16(s, off);
}

static int32_t sl_i32(const AeCameraSlot *s, size_t off)
{
    return ae_camera_i32(s, off);
}

/* GetGroundHeight result truncated to signed 16 bit (sll / sra by 16). */
static int32_t ground16(const AeCameraQueries *q, int32_t x, int32_t y, int32_t z)
{
    return s16_of((uint32_t)q->ground(q->user, x, y, z));
}

/* Saturating byte increment: b + (b != 0xFF). */
static void sat_inc(uint8_t *b)
{
    if (*b != 0xFFu)
        (*b)++;
}

/* ---- CameraSlotAimAtFocus (0x80047360) ---- */

static void aim_at_focus(const AeCameraSlot *s, AeCameraFollowFrame *f, const AeCameraQueries *q)
{
    int16_t v[3];
    int32_t dx, dz, vx, vz, dot, cross;

    v[0] = to_s16((uint32_t)sc_u16(f, SC_PLAYER_X) - (uint32_t)ae_camera_u16(s, SL_EYE_X));
    v[1] = 0;
    v[2] = to_s16((uint32_t)sc_u16(f, SC_PLAYER_Z) - (uint32_t)ae_camera_u16(s, SL_EYE_Z));
    (void)q->normalize(q->user, v);

    dx = sc_s16(f, SC_DIR);
    dz = sc_s16(f, SC_DIR + 4);
    vx = v[0];
    vz = v[2];
    dot = neg32(add32(mul32(dx, vx), mul32(dz, vz)));
    sc_set32(f, SC_AIM_DOT, (uint32_t)trunc_shr(dot, 12));
    cross = sub32(mul32(dx, vz), mul32(dz, vx));
    sc_set32(f, SC_AIM_CROSS, (uint32_t)trunc_shr(cross, 12));
}

/* ---- CameraSlotUpdateYaw (0x80048DD0). Returns 0 only if atan fails, before
 * anything is written. ---- */

static int update_yaw(const FollowCtx *c)
{
    AeCameraSlot *s = c->s;
    AeCameraFollowFrame *f = c->f;
    int32_t target_yaw, yd, side;
    uint32_t i;

    if (!ae_camera_atan2_safe(c->t, neg32(sc_s16(f, SC_DIR)), neg32(sc_s16(f, SC_DIR + 4)),
                              &target_yaw))
        return 0;
    sset32(s, SL_TARGET_YAW, (uint32_t)target_yaw);

    yd = wrap_yaw(sub32(sl_i32(s, AE_CAM_YAW), target_yaw));
    sset32(s, SL_YAW_DELTA, (uint32_t)yd);
    if (yd >= 0x801)
        sset32(s, SL_YAW_DELTA, (uint32_t)sub32(yd, 0x1000));

    aim_at_focus(s, f, c->q);

    if ((f->right_stick_word & 0xFFFFu) < 0x65u)
        s->raw[AE_CAM_YAW_HOLD] = 0;
    else
        sat_inc(&s->raw[AE_CAM_YAW_HOLD]);

    if (f->right_stick_word == 0 &&
        sl_s16(s, AE_CAM_LAST_POS) == sc_s16(f, SC_PLAYER_X) &&
        sl_s16(s, AE_CAM_LAST_POS + 2) == sc_s16(f, SC_PLAYER_Y) &&
        sl_s16(s, AE_CAM_LAST_POS + 4) == sc_s16(f, SC_PLAYER_Z))
        sat_inc(&s->raw[AE_CAM_STILL_FRAMES]);
    else
        s->raw[AE_CAM_STILL_FRAMES] = 0;

    side = sub32(mul32(sc_s16(f, SC_DIR + 4), sl_s16(s, SL_DIR_X)),
                 mul32(sc_s16(f, SC_DIR), sl_s16(s, SL_DIR_Z)));
    sset32(s, SL_SIDE, (uint32_t)floor_shr(side, 12));

    for (i = 0; i < 4; i++) {
        s->raw[SL_DIR_X + i] = f->scratch[SC_DIR + i];
        s->raw[SL_DIR_Z + i] = f->scratch[SC_DIR + 4 + i];
    }
    return 1;
}

/* ---- CameraFollowUpdate body (0x800495F4 after the guard). Returns 1 on
 * success, 0 with *fail set to AE_CAMERA_FOLLOW_* bits. ---- */

static int follow_body(const FollowCtx *c, int mode, uint32_t *fail)
{
    AeCameraSlot *s = c->s;
    AeCameraFollowFrame *f = c->f;
    const AeCameraQueries *q = c->q;
    const AeCameraMathTables *t = c->t;
    uint32_t flags4 = f->state_flags & 0xF0u;
    uint32_t player_mode = f->player_mode & 0xFFFFu;
    uint32_t state = f->state & 0xFFFFu;
    int32_t th, eh, fd;       /* local target height / eye height / follow dist (s16 values) */
    int32_t w78, dist, yd;
    int hit = 0;
    int16_t de[3];
    int i;

    /* Projected screen XY of the player (previous view), supplied by the caller. */
    sc_set32(f, SC_PROJ_X, f->projected_sxy & 0xFFFFu);
    sc_set32(f, SC_PROJ_Y, (uint32_t)s16_of(f->projected_sxy >> 16));

    th = sl_s16(s, AE_CAM_TARGET_HEIGHT);
    eh = sl_s16(s, AE_CAM_EYE_HEIGHT);
    fd = sl_s16(s, AE_CAM_FOLLOW_DIST_H);
    if (mode == 0) {
        if (f->controls[CTL_EYE_ON] != 0)
            eh = s16_of(ctl_u16(f, CTL_EYE_HEIGHT));
        if (f->controls[CTL_DIST_ON] != 0)
            fd = s16_of(ctl_u16(f, CTL_FOLLOW_DIST));
        if (flags4 == 0x80) {
            eh = -0xDC;
            fd = 0x26C;
        }
        if (player_mode == 10) {
            fd = 0x2F8;
            th = -0x32;
            eh = -100;
        } else if (f->submerged != 0) {
            uint32_t v = (uint32_t)sc_i32(f, SC_V6C);
            fd = 0x294;
            th = s16_of(v << 5);
            eh = s16_of(0u - (v << 3));
        }
    } else if (f->controls[CTL_STAGE_MODE] == 1) {
        eh = 0;
        th = -0x8C;
    }

    if (!update_yaw(c)) {
        *fail |= AE_CAMERA_FOLLOW_MATH_DOMAIN;
        return 0;
    }

    if (mode == 1) {
        if ((sc_i32(f, SC_V68) | sc_i32(f, SC_V6C) | sc_i32(f, SC_V70)) == 0) {
            s->raw[AE_CAM_YAW_HOLD] = 0;
            sat_inc(&s->raw[AE_CAM_STILL_FRAMES]);
        } else {
            s->raw[AE_CAM_STILL_FRAMES] = 0;
            sat_inc(&s->raw[AE_CAM_YAW_HOLD]);
        }
    }

    ae_camera_stick_orbit(s, f->can_exit, f->controls[CTL_FIXED_ACTIVE],
                          (uint16_t)(f->right_stick_word & 0xFFFFu), f->stick_x, f->stick_y,
                          f->pad_held, to_s16(sc_u16(f, SC_PLAYER_Y)));

    w78 = sc_i32(f, SC_AIM_DOT);
    if ((uint32_t)w78 + 3999u < 0x1EDBu) {
        int32_t side = sl_i32(s, SL_SIDE);
        if (side < 0)
            side = neg32(side);
        if (side < 0x640 && s->raw[AE_CAM_YAW_HOLD] != 0 && s->raw[AE_CAM_MODE_TIMER] == 0)
            sset32(s, SL_YAW_DELTA, (uint32_t)trunc_shr(mul32(sl_i32(s, SL_YAW_DELTA), 0x3F), 6));
    }

    /* Yaw delta damping. */
    {
        int damp = 0;
        int32_t a1 = 0;

        if (s->raw[AE_CAM_FOLLOW_FLAG_A] != 0 && f->rccar_active != 0) {
            int32_t a0 = s->raw[AE_CAM_FOLLOW_FLAG_A];
            a1 = a0 < 0x31 ? a0 : 0x30;
            damp = 1;
        } else if (s->raw[AE_CAM_STILL_FRAMES] >= 0x12 && w78 < 0xF3C && player_mode != 10) {
            int32_t a0 = (int32_t)s->raw[AE_CAM_STILL_FRAMES] - 0x12;
            if (a0 <= 0)
                a1 = 1;
            else
                a1 = a0 < 0x31 ? a0 : 0x30;
            damp = 1;
        }
        if (damp) {
            int32_t d = sub32(0x40, a1);
            int32_t quo;
            if (!checked_div(mul32(sl_i32(s, SL_YAW_DELTA), d), add32(d, 1), &quo)) {
                *fail |= AE_CAMERA_FOLLOW_MATH_DOMAIN;
                return 0;
            }
            sset32(s, SL_YAW_DELTA, (uint32_t)quo);
        }
    }

    if (s->raw[AE_CAM_CLAMP_STATE] != 0) {
        s->raw[AE_CAM_CLAMP_STATE]--;
        yd = trunc_shr(mul32(sl_i32(s, SL_YAW_DELTA), 15), 4);
        sset32(s, SL_YAW_DELTA, (uint32_t)yd);
        if (yd < 0)
            yd = neg32(yd);
        if (yd < 0xB)
            s->raw[AE_CAM_CLAMP_STATE] = 0;
    }

    /* Follow distance smoothing. */
    if (s->raw[AE_CAM_MODE_TIMER] == 0 || flags4 == 0x80) {
        if (player_mode == 10) {
            sset32(s, AE_CAM_DIST,
                   (uint32_t)trunc_shr(add32(mul32(sl_i32(s, AE_CAM_DIST), 7), fd), 3));
        } else {
            dist = sl_i32(s, AE_CAM_DIST);
            if (add32(fd, 0x100) < dist) {
                sset32(s, AE_CAM_DIST,
                       (uint32_t)trunc_shr(add32(add32(mul32(dist, 3), 0x100), fd), 2));
            }
            dist = sl_i32(s, AE_CAM_DIST);
            if (dist < add32(fd, -0x100)) {
                sset32(s, AE_CAM_DIST,
                       (uint32_t)trunc_shr(add32(add32(mul32(dist, 3), -0x100), fd), 2));
            }
        }
    } else {
        sset32(s, AE_CAM_DIST,
               (uint32_t)trunc_shr(add32(add32(sl_i32(s, AE_CAM_DIST), -0x80), fd), 1));
    }

    /* Yaw. */
    sset32(s, AE_CAM_YAW,
           (uint32_t)mod4096(add32(add32(sl_i32(s, SL_YAW_DELTA), 0x1000),
                                   add32(sl_i32(s, SL_TARGET_YAW), sl_s16(s, AE_CAM_ORBIT_Y)))));
    if (s->raw[AE_CAM_MODE_TIMER] != 0) {
        int32_t r, adj, h;

        s->raw[AE_CAM_MODE_TIMER]--;
        r = wrap_yaw(sub32(sl_i32(s, SL_HEIGHT_A), sl_i32(s, SL_HEIGHT_B)));
        if (r >= 0x801)
            r = sub32(r, 0x1000);
        adj = trunc_shr(mul32(r, 7), 3);
        {
            int32_t m = adj < 0 ? neg32(adj) : adj;
            if (m < 0xB)
                s->raw[AE_CAM_MODE_TIMER] = 0;
        }
        h = mod4096(add32(sl_i32(s, SL_HEIGHT_B), add32(adj, 0x1000)));
        sset32(s, SL_HEIGHT_A, (uint32_t)h);
        sset32(s, AE_CAM_YAW, (uint32_t)h);
    }

    /* Eye-to-ground probe along the eye side offset (3 steps). */
    if (w78 < 1000) {
        int32_t ey = sl_s16(s, SL_EYE_Y);
        int32_t ref = sc_i32(f, SC_GROUND_REF);
        int32_t d = sub32(ref, ey);
        int in_range = d >= 0 ? d < 0x800 : sub32(ey, ref) < 0x800;
        int32_t sx = f->stick_x;

        if (in_range && (sx < 0 ? -sx : sx) > 0x32) {
            int16_t v[3];
            int32_t p20, p24, q1, q2;
            uint32_t s4, s3, s6, s8, s7, l30;
            int32_t dxe, dye, dze;

            v[0] = to_s16((uint32_t)ae_camera_u16(s, SL_EYE_X) - (uint32_t)sc_u16(f, SC_PLAYER_X));
            v[1] = 0;
            v[2] = to_s16((uint32_t)ae_camera_u16(s, SL_EYE_Z) - (uint32_t)sc_u16(f, SC_PLAYER_Z));
            (void)q->normalize(q->user, v);
            if (f->stick_x > 0) {
                p20 = s16_of(0u - (uint32_t)(int32_t)v[2]);
                p24 = v[0];
            } else {
                p20 = v[2];
                p24 = s16_of(0u - (uint32_t)(int32_t)v[0]);
            }
            q1 = sub32(floor_shr(mulhi(p20, 0x2AAAAAAB), 1), p20 < 0 ? -1 : 0);
            q2 = sub32(floor_shr(mulhi(p24, 0x2AAAAAAB), 1), p24 < 0 ? -1 : 0);
            s4 = (uint32_t)sc_u16(f, SC_PLAYER_X) + (uint32_t)q1;
            s3 = (uint32_t)sc_u16(f, SC_PLAYER_Z) + (uint32_t)q2;
            s6 = (uint32_t)sc_u16(f, SC_PLAYER_Y) - 0x80u;

            /* Only the low 16 bits of the step values are ever used (the
             * original's logical vs arithmetic shift does not matter). */
            dxe = sub32(sl_s16(s, SL_EYE_X), sc_s16(f, SC_PLAYER_X));
            l30 = (uint32_t)trunc_shr(dxe, 2) & 0xFFFFu;
            dye = sub32(sl_s16(s, SL_EYE_Y), s16_of(s6));
            s8 = (uint32_t)trunc_shr(dye, 2);
            dze = sub32(sl_s16(s, SL_EYE_Z), sc_s16(f, SC_PLAYER_Z));
            s7 = (uint32_t)trunc_shr(dze, 2);

            for (i = 0; i < 3; i++) {
                int32_t ay = s16_of(s6);
                int32_t g = ground16(q, s16_of(s4), ay, s16_of(s3));
                if (g < ay - 0xC0) {
                    hit = 1;
                    break;
                }
                s6 += s8;
                s3 += s7;
                s4 += l30;
            }
        }
    }

    /* Held camera behind a wall: look back at the player and re-clamp. */
    if (w78 > 2000 && s->raw[AE_CAM_YAW_HOLD] != 0) {
        int16_t v[3];
        int32_t g;

        v[0] = to_s16((uint32_t)sc_u16(f, SC_PLAYER_X) - (uint32_t)ae_camera_u16(s, SL_EYE_X));
        v[1] = 0;
        v[2] = to_s16((uint32_t)sc_u16(f, SC_PLAYER_Z) - (uint32_t)ae_camera_u16(s, SL_EYE_Z));
        (void)q->normalize(q->user, v);
        g = ground16(q, sub32(sl_s16(s, SL_EYE_X), trunc_shr(v[0], 5)), sl_s16(s, SL_EYE_Y),
                     sub32(sl_s16(s, SL_EYE_Z), trunc_shr(v[2], 5)));
        if (g < sl_s16(s, SL_EYE_Y)) {
            s->raw[AE_CAM_CLAMP_STATE] = 5;
            s->raw[AE_CAM_CLAMP_COUNT] = 0;
            s->raw[AE_CAM_CLAMP_TIMER] = 0;
            s->raw[AE_CAM_CLAMP_FLAG] = 0xFF;
        }
    }

    if (hit) {
        if (w78 < 0xDAC) {
            int32_t d;
            s->raw[AE_CAM_CLAMP_STATE] = 8;
            s->raw[AE_CAM_CLAMP_COUNT] = 0;
            s->raw[AE_CAM_CLAMP_TIMER] = 0;
            d = sub32(sl_i32(s, AE_CAM_DIST), 0x258);
            sset32(s, AE_CAM_DIST, (uint32_t)d);
            if (d < 0)
                sset32(s, AE_CAM_DIST, 0);
            d = sl_i32(s, AE_CAM_DIST);
            sset32(s, AE_CAM_DIST, (uint32_t)add32(trunc_shr(mul32(d, 7), 3), 0x258));
        }
    } else {
        uint32_t d = (uint32_t)sc_u16(f, SC_PLAYER_Y) - (uint32_t)ae_camera_u16(s, AE_CAM_LAST_POS + 2);
        int32_t sv = s16_of(d);

        if (sv != 0 && (sv < 0 ? -sv : sv) < 0x60 &&
            (flags4 == 0x20 || flags4 == 0x30 || flags4 == 0xA0 || state == 6)) {
            uint32_t a0 = d;
            int32_t v6, hi;

            if (w78 > 0 && sv < 0)
                a0 = (uint32_t)(-sv) << 2;
            v6 = s16_of(a0);
            sset16(s, SL_EYE_VEL_Y, (uint32_t)trunc_shr(neg32(v6), 1));
            hi = mulhi(v6, 0x55555556);
            sset16(s, SL_TGT_VEL_Y, (uint32_t)sub32(hi, v6 < 0 ? -1 : 0));
        }
    }

    /* Desired target. */
    {
        int32_t root, a0, a2, a1v, a3;
        int32_t v68 = sc_i32(f, SC_V68), v70 = sc_i32(f, SC_V70);
        int32_t v6c = sc_i32(f, SC_V6C);

        sc_set16(f, SC_DESIRED_TGT + 2,
                 (uint32_t)sc_u16(f, SC_PLAYER_Y) + (uint32_t)th - 0x80u);
        root = q->sqrt(q->user, (uint32_t)add32(mul32(v68, v68), mul32(v70, v70)));
        a0 = root >= 0x21 ? 0x20 : root;

        a2 = trunc_shr(mul32(trunc_shr(sc_s16(f, SC_DIR), 4), a0), 5);
        a1v = trunc_shr(mul32(trunc_shr(sc_s16(f, SC_DIR + 4), 4), a0), 5);
        a3 = a2;
        if (mode == 1) {
            a3 = neg32(a2);
            a1v = neg32(a1v);
        }
        sc_set16(f, SC_DESIRED_TGT,
                 (uint32_t)sc_u16(f, SC_PLAYER_X) + (uint32_t)a3 + (uint32_t)mul32(v68, a0));
        sc_set16(f, SC_DESIRED_TGT + 4,
                 (uint32_t)sc_u16(f, SC_PLAYER_Z) + (uint32_t)a1v + (uint32_t)mul32(v70, a0));
        if (flags4 == 0x50 && v6c > 0)
            sc_set16(f, SC_DESIRED_TGT + 2,
                     (uint32_t)sc_u16(f, SC_DESIRED_TGT + 2) + ((uint32_t)v6c << 6));
        if (sc_i32(f, SC_PROJ_Y) > 0xD0 && v6c > 0x20 && flags4 != 0x40 && w78 < 0) {
            int32_t nd = trunc_shr(mul32(sl_i32(s, AE_CAM_DIST), 7), 3);
            sset32(s, AE_CAM_DIST, (uint32_t)nd);
            if (nd < 0x200)
                sset32(s, AE_CAM_DIST, 0x200);
        }
        if (sc_s16(f, SC_DESIRED_TGT + 2) > 0 && (f->current_area & 0xFFFFu) != 7)
            sc_set16(f, SC_DESIRED_TGT + 2, 0);
    }

    /* Desired eye. */
    sc_set16(f, SC_DESIRED_EYE,
             (uint32_t)ae_camera_u16(s, SL_TGT_X) +
             (uint32_t)floor_shr(mul32(ae_camera_rcos(t, sl_i32(s, AE_CAM_YAW)),
                                       sl_i32(s, AE_CAM_DIST)), 12));
    {
        int32_t sn = ae_camera_rsin(t, sl_i32(s, AE_CAM_YAW));
        sc_set16(f, SC_DESIRED_EYE + 2,
                 (uint32_t)sc_u16(f, SC_PLAYER_Y) + (uint32_t)eh - 0x80u);
        sc_set16(f, SC_DESIRED_EYE + 4,
                 (uint32_t)ae_camera_u16(s, SL_TGT_Z) +
                 (uint32_t)floor_shr(mul32(sn, sl_i32(s, AE_CAM_DIST)), 12));
    }

    /* Gadget variant (original gadget 3 with controls[0x46] != 1) was rejected
     * in the preflight. */

    if (s->raw[AE_CAM_CLAMP_TIMER] != 0)
        s->raw[AE_CAM_CLAMP_TIMER]--;
    for (i = 0; i < 3; i++)
        de[i] = to_s16(sc_u16(f, SC_DESIRED_EYE + 2 * (size_t)i));
    if (!ae_camera_ground_clamp_near(s, de, 0, q)) {
        *fail |= AE_CAMERA_FOLLOW_MISSING_CALLBACK;
        return 0;
    }

    if (s->raw[AE_CAM_CLAMP_TIMER] != 0) {
        int32_t sn;

        if (s->raw[AE_CAM_CLAMP_COUNT] >= 5) {
            int32_t d = sub32(sl_i32(s, AE_CAM_DIST), 0x30);
            sset32(s, AE_CAM_DIST, (uint32_t)d);
            if (d < 0x180) {
                sset32(s, AE_CAM_DIST, 0x180);
                if (sl_s16(s, SL_EYE_Y) < sub32(sc_s16(f, SC_PLAYER_Y), 0x60))
                    sset16(s, SL_EYE_VEL_Y, 8);
            }
            sset16(s, AE_CAM_FOLLOW_DIST_H, (uint32_t)sl_i32(s, AE_CAM_DIST));
        }
        sc_set16(f, SC_DESIRED_EYE,
                 (uint32_t)ae_camera_u16(s, SL_TGT_X) +
                 (uint32_t)floor_shr(mul32(ae_camera_rcos(t, sl_i32(s, AE_CAM_YAW)),
                                           sl_i32(s, AE_CAM_DIST)), 12));
        sn = ae_camera_rsin(t, sl_i32(s, AE_CAM_YAW));
        sc_set16(f, SC_DESIRED_EYE + 4,
                 (uint32_t)ae_camera_u16(s, SL_TGT_Z) +
                 (uint32_t)floor_shr(mul32(sn, sl_i32(s, AE_CAM_DIST)), 12));
        for (i = 0; i < 3; i++)
            de[i] = to_s16(sc_u16(f, SC_DESIRED_EYE + 2 * (size_t)i));
        if (!ae_camera_ground_clamp_near(s, de, 1, q)) {
            *fail |= AE_CAMERA_FOLLOW_MISSING_CALLBACK;
            return 0;
        }
    }

    if (s->raw[AE_CAM_CLAMP_STATE] == 0 && sl_s16(s, AE_CAM_FOLLOW_DIST_H) < 0x460 &&
        s->raw[AE_CAM_CLAMP_FLAG] >= 0xD) {
        int32_t v3 = mul32(sl_s16(s, AE_CAM_FOLLOW_DIST_H), 3);
        sset16(s, AE_CAM_FOLLOW_DIST_H, (uint32_t)trunc_shr(add32(v3, 0x460), 2));
    }

    if ((f->stage_entry_flags & 1u) == 0 && s->raw[AE_CAM_CLAMP_STATE] == 0) {
        sc_set16(f, SC_DESIRED_EYE,
                 (uint32_t)sc_u16(f, SC_DESIRED_EYE) +
                 (uint32_t)trunc_shr(mul32(sc_i32(f, SC_V68), 3), 1));
        sc_set16(f, SC_DESIRED_EYE + 4,
                 (uint32_t)sc_u16(f, SC_DESIRED_EYE + 4) +
                 (uint32_t)trunc_shr(mul32(sc_i32(f, SC_V70), 3), 1));
    }

    if (flags4 == 0x50 && sc_i32(f, SC_V6C) > 0)
        sc_set16(f, SC_DESIRED_EYE + 2,
                 (uint32_t)sc_u16(f, SC_DESIRED_EYE + 2) - ((uint32_t)sc_i32(f, SC_V6C) << 6));

    if (sc_s16(f, SC_DESIRED_EYE + 2) > 0)
        sc_set16(f, SC_DESIRED_EYE + 2, 0);

    if (state == 0x2D) {
        sc_set16(f, SC_DESIRED_EYE,
                 (uint32_t)sc_u16(f, SC_PLAYER_X) - (uint32_t)trunc_shr(sc_s16(f, SC_DIR), 6));
        sc_set16(f, SC_DESIRED_EYE + 4,
                 (uint32_t)sc_u16(f, SC_PLAYER_Z) - (uint32_t)trunc_shr(sc_s16(f, SC_DIR + 4), 6));
    }

    if (s->raw[AE_CAM_FIXED_ACTIVE] != 0) {
        sset16(s, SL_TGT_X,
               (uint32_t)sc_u16(f, SC_PLAYER_X) + (uint32_t)trunc_shr(sc_s16(f, SC_DIR), 4));
        sset16(s, SL_TGT_Y, (uint32_t)sc_u16(f, SC_PLAYER_Y) + (uint32_t)th - 0x80u);
        sset16(s, SL_TGT_Z,
               (uint32_t)sc_u16(f, SC_PLAYER_Z) + (uint32_t)trunc_shr(sc_s16(f, SC_DIR + 4), 4));
    }

    /* Target smoothing. */
    sset16(s, SL_TGT_X,
           (uint32_t)trunc_shr(add32(sc_s16(f, SC_DESIRED_TGT), mul32(sl_s16(s, SL_TGT_X), 31)), 5));
    sset16(s, SL_TGT_Z,
           (uint32_t)trunc_shr(add32(sc_s16(f, SC_DESIRED_TGT + 4), mul32(sl_s16(s, SL_TGT_Z), 31)), 5));
    if (flags4 == 0x40)
        sset16(s, SL_TGT_Y,
               (uint32_t)trunc_shr(add32(sc_s16(f, SC_DESIRED_TGT + 2),
                                         mul32(sl_s16(s, SL_TGT_Y), 31)), 5));
    else
        sset16(s, SL_TGT_Y,
               (uint32_t)trunc_shr(add32(sc_s16(f, SC_DESIRED_TGT + 2),
                                         mul32(sl_s16(s, SL_TGT_Y), 15)), 4));

    {
        int32_t pv = sc_i32(f, SC_PROJ_Y);
        int32_t v6c = sc_i32(f, SC_V6C);
        int skip = flags4 == 0x50 ? pv >= 0x30 : pv >= 0x20;
        if (!skip && v6c < 0)
            sset16(s, SL_TGT_VEL_Y, (uint32_t)v6c - 8u);
        if (pv > 0xD0 && v6c > 0) {
            uint32_t nv = (uint32_t)ae_camera_u16(s, SL_TGT_VEL_Y) + (uint32_t)trunc_shr(v6c, 1);
            sset16(s, SL_TGT_VEL_Y, nv);
            if (s16_of(nv) > 0x60)
                sset16(s, SL_TGT_VEL_Y, 0x60);
        }
    }
    {
        int32_t tv = sl_s16(s, SL_TGT_VEL_Y);
        sset16(s, SL_TGT_Y,
               (uint32_t)ae_camera_u16(s, SL_TGT_Y) + (uint32_t)ae_camera_u16(s, SL_TGT_VEL_Y));
        sset16(s, SL_TGT_VEL_Y, (uint32_t)trunc_shr(mul32(tv, 7), 3));
    }

    /* Eye smoothing / fixed placement. */
    if (s->raw[AE_CAM_FIXED_ACTIVE] == 0) {
        int32_t ev, y;

        sset16(s, SL_EYE_X,
               (uint32_t)trunc_shr(add32(sc_s16(f, SC_DESIRED_EYE), sl_s16(s, SL_EYE_X)), 1));
        sset16(s, SL_EYE_Z,
               (uint32_t)trunc_shr(add32(sc_s16(f, SC_DESIRED_EYE + 4), sl_s16(s, SL_EYE_Z)), 1));
        if (flags4 == 0x40)
            y = trunc_shr(add32(sc_s16(f, SC_DESIRED_EYE + 2), mul32(sl_s16(s, SL_EYE_Y), 63)), 6);
        else
            y = trunc_shr(add32(sc_s16(f, SC_DESIRED_EYE + 2), mul32(sl_s16(s, SL_EYE_Y), 31)), 5);
        sset16(s, SL_EYE_Y, (uint32_t)y);
        ev = sl_s16(s, SL_EYE_VEL_Y);
        sset16(s, SL_EYE_Y, (uint32_t)ae_camera_u16(s, SL_EYE_Y) +
                            (uint32_t)ae_camera_u16(s, AE_CAM_ORBIT_X));
        sset16(s, SL_EYE_Y, (uint32_t)ae_camera_u16(s, SL_EYE_Y) +
                            (uint32_t)ae_camera_u16(s, SL_EYE_VEL_Y));
        sset16(s, SL_EYE_VEL_Y, (uint32_t)trunc_shr(mul32(ev, 7), 3));
    } else {
        int16_t v[3];
        int32_t ya;

        for (i = 0; i < 8; i++)
            s->raw[AE_CAM_EYE + i] = f->scratch[SC_DESIRED_EYE + i];
        v[0] = to_s16((uint32_t)ae_camera_u16(s, SL_TGT_X) - (uint32_t)ae_camera_u16(s, SL_EYE_X));
        v[1] = 0;
        v[2] = to_s16((uint32_t)ae_camera_u16(s, SL_TGT_Z) - (uint32_t)ae_camera_u16(s, SL_EYE_Z));
        (void)q->normalize(q->user, v);
        if (!ae_camera_atan2_safe(t, neg32(v[0]), neg32(v[2]), &ya)) {
            *fail |= AE_CAMERA_FOLLOW_MATH_DOMAIN;
            return 0;
        }
        sset32(s, AE_CAM_YAW, (uint32_t)ya);
    }

    if (state != 0x2D) {
        int16_t focus[3];
        for (i = 0; i < 3; i++)
            focus[i] = to_s16(sc_u16(f, SC_FOCUS + 2 * (size_t)i));
        if (!ae_camera_ground_clamp(s, focus, q)) {
            *fail |= AE_CAMERA_FOLLOW_MISSING_CALLBACK;
            return 0;
        }
    }

    {
        int32_t dx = sub32(sl_s16(s, SL_TGT_X), sl_s16(s, SL_EYE_X));
        int32_t dz = sub32(sl_s16(s, SL_TGT_Z), sl_s16(s, SL_EYE_Z));
        int32_t r;

        sset16(s, AE_CAM_LAST_POS, sc_u16(f, SC_PLAYER_X));
        sset16(s, AE_CAM_LAST_POS + 2, sc_u16(f, SC_PLAYER_Y));
        sset16(s, AE_CAM_LAST_POS + 4, sc_u16(f, SC_PLAYER_Z));
        r = q->sqrt(q->user, (uint32_t)add32(mul32(dx, dx), mul32(dz, dz)));
        sset32(s, AE_CAM_DIST, (uint32_t)r);
    }
    s->raw[AE_CAM_FIXED_ACTIVE] = 0;
    sset32(s, AE_CAM_SAVED_YAW, (uint32_t)sl_i32(s, AE_CAM_YAW));
    return 1;
}

/* ---- public entry points ---- */

int ae_camera_follow_update(AeCameraSlot *slot, AeCameraFollowFrame *frame,
                            const AeCameraMathTables *tables,
                            const AeCameraQueries *queries, int mode)
{
    FollowCtx c;
    AeCameraSlot saved_slot;
    uint8_t saved_scratch[sizeof frame->scratch];
    uint8_t saved_controls[sizeof frame->controls];
    uint32_t bad = 0, fail = 0;

    if (!frame)
        return 0;
    if (!slot || !tables || !tables->loaded || !queries || !queries->normalize ||
        !queries->sqrt || !queries->ground)
        bad |= AE_CAMERA_FOLLOW_MISSING_CALLBACK;
    if (mode != 0 && mode != 1)
        bad |= AE_CAMERA_FOLLOW_UNSUPPORTED_MODE;
    if (bad) {
        frame->status |= bad;
        return 0;
    }

    if (frame->controls[CTL_GUARD] != 0)
        return 1;
    if (s8_of(frame->current_gadget) == 3 && frame->controls[CTL_AIM_MODE] != 1) {
        frame->status |= AE_CAMERA_FOLLOW_UNSUPPORTED_GADGET;
        return 0;
    }

    saved_slot = *slot;
    memcpy(saved_scratch, frame->scratch, sizeof saved_scratch);
    memcpy(saved_controls, frame->controls, sizeof saved_controls);

    frame->controls[CTL_GUARD] = 1;
    c.s = slot;
    c.f = frame;
    c.t = tables;
    c.q = queries;
    if (!follow_body(&c, mode, &fail)) {
        *slot = saved_slot;
        memcpy(frame->scratch, saved_scratch, sizeof saved_scratch);
        memcpy(frame->controls, saved_controls, sizeof saved_controls);
        frame->status |= fail ? fail : AE_CAMERA_FOLLOW_MATH_DOMAIN;
        return 0;
    }
    return 1;
}

int ae_camera_slot_update_yaw(AeCameraSlot *slot, AeCameraFollowFrame *frame,
                              const AeCameraMathTables *tables,
                              const AeCameraQueries *queries)
{
    FollowCtx c;

    if (!frame)
        return 0;
    if (!slot || !tables || !tables->loaded || !queries || !queries->normalize) {
        frame->status |= AE_CAMERA_FOLLOW_MISSING_CALLBACK;
        return 0;
    }
    c.s = slot;
    c.f = frame;
    c.t = tables;
    c.q = queries;
    if (!update_yaw(&c)) { /* atan is the first operation: nothing was written */
        frame->status |= AE_CAMERA_FOLLOW_MATH_DOMAIN;
        return 0;
    }
    return 1;
}

int ae_camera_slot_aim_at_focus(const AeCameraSlot *slot, AeCameraFollowFrame *frame,
                                const AeCameraQueries *queries)
{
    if (!frame)
        return 0;
    if (!slot || !queries || !queries->normalize) {
        frame->status |= AE_CAMERA_FOLLOW_MISSING_CALLBACK;
        return 0;
    }
    aim_at_focus(slot, frame, queries);
    return 1;
}
