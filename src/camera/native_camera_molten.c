/* Camera slot handler 11 for area 05 (overlay 0x18D 0x80140B0C), see native_camera_molten.md. */
#include "native_camera_molten.h"

#include <string.h>

#include "native_camera_focus.h"

enum {
    SC_DESIRED_EYE    = 0x00,
    SC_DESIRED_TARGET = 0x08,
    SC_PLAYER         = 0x10
};

static int32_t s16_at(const uint8_t *p)
{
    uint32_t v = (uint32_t)p[0] | ((uint32_t)p[1] << 8);
    return v >= 0x8000u ? (int32_t)v - 0x10000 : (int32_t)v;
}

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

/* Halfword wrap of a 32 bit value (the original's sh of a subu / addu result). */
static int32_t wrap16(uint32_t v)
{
    v &= 0xFFFFu;
    return v >= 0x8000u ? (int32_t)v - 0x10000 : (int32_t)v;
}

/* Arithmetic right shift without relying on implementation-defined behavior. */
static int32_t sar32(int32_t v, unsigned n)
{
    return v >= 0 ? (int32_t)((uint32_t)v >> n) : ~(int32_t)((uint32_t)(~v) >> n);
}

static int32_t i32_of(uint32_t v)
{
    return v >= 0x80000000u ? (int32_t)(v - 0x80000000u) - 0x7FFFFFFF - 1 : (int32_t)v;
}

/* 32 bit wrapping product (mult / mflo). */
static int32_t mul32(int32_t a, int32_t b)
{
    return i32_of((uint32_t)a * (uint32_t)b);
}

/* The original's "bgez t,..; addiu t,t,3; sra 14" rounding step. */
static int32_t sra14_biased(int32_t t)
{
    return sar32(t < 0 ? i32_of((uint32_t)t + 3u) : t, 14);
}

int ae_camera_molten_update(AeCameraSlot *slot, const AeCameraMathTables *math,
                            const AeCameraQueries *queries, const int32_t player[3],
                            const uint8_t region_focus8[8], uint8_t scratch[0x80])
{
    AeCameraSlot work;
    uint8_t sc[0x80];
    uint8_t focus8[8];
    int16_t v[3];
    int16_t desired[3];
    int32_t div[3];
    uint32_t sq;
    int32_t px, py, pz, fx, fz, dx, dz, dist, mag, tx, tz;

    if (!slot || !math || !math->loaded || !queries || !queries->sqrt || !player ||
        !region_focus8 || !scratch)
        return 0;

    memcpy(&work, slot, sizeof work);
    memcpy(sc, scratch, sizeof sc);

    /* Player halfwords to scratch 0x10 / 0x12 / 0x14. */
    px = wrap16((uint32_t)player[0]);
    py = wrap16((uint32_t)player[1]);
    pz = wrap16((uint32_t)player[2]);
    put16(&sc[SC_PLAYER + 0], (uint32_t)px);
    put16(&sc[SC_PLAYER + 2], (uint32_t)py);
    put16(&sc[SC_PLAYER + 4], (uint32_t)pz);

    /* CameraSlotGetFocusPoint(slot, 2): copy of region_focus8; only x / z are used. */
    if (!ae_camera_get_focus_point(&work, 2, player, NULL, region_focus8, focus8))
        return 0;
    fx = s16_at(&focus8[0]);
    fz = s16_at(&focus8[4]);

    /* Normalized (focus - player) in the XZ plane; the result is not checked. */
    v[0] = (int16_t)wrap16((uint32_t)fx - (uint32_t)px);
    v[1] = 0;
    v[2] = (int16_t)wrap16((uint32_t)fz - (uint32_t)pz);
    if (!ae_camera_normalize(math, v, NULL))
        return 0;

    /* Distance player -> focus, scaled and clamped. */
    dx = wrap16((uint32_t)px - (uint32_t)fx);
    dz = wrap16((uint32_t)pz - (uint32_t)fz);
    dist = queries->sqrt(queries->user, (uint32_t)mul32(dx, dx) + (uint32_t)mul32(dz, dz));
    mag = 2000;
    if (dist > 1000)
        mag = i32_of((uint32_t)dist << 1);
    if (mag > 0xD48)
        mag = 0xD48;

    tx = mul32(v[0], mag);
    tz = mul32(v[2], mag);

    /* Desired target (scratch 0x08 / 0x0A / 0x0C) and eye (0x00 / 0x02 / 0x04). */
    put16(&sc[SC_DESIRED_TARGET + 0], (uint32_t)fx - (uint32_t)sra14_biased(tx));
    put16(&sc[SC_DESIRED_TARGET + 2], (uint32_t)py - 0x82u);
    put16(&sc[SC_DESIRED_EYE + 0], (uint32_t)fx - (uint32_t)sar32(tx, 12));
    put16(&sc[SC_DESIRED_EYE + 2], (uint32_t)py - 0x35Cu);
    put16(&sc[SC_DESIRED_TARGET + 4], (uint32_t)fz - (uint32_t)sra14_biased(tz));
    put16(&sc[SC_DESIRED_EYE + 4], (uint32_t)fz - (uint32_t)sar32(tz, 12));

    /* CameraSlotSmoothTarget(slot, 8, 8, 8), CameraSlotSmoothEye(slot, 8, 16, 8). */
    desired[0] = (int16_t)s16_at(&sc[SC_DESIRED_TARGET + 0]);
    desired[1] = (int16_t)s16_at(&sc[SC_DESIRED_TARGET + 2]);
    desired[2] = (int16_t)s16_at(&sc[SC_DESIRED_TARGET + 4]);
    div[0] = 8; div[1] = 8; div[2] = 8;
    if (!ae_camera_smooth_target(&work, desired, div))
        return 0;
    desired[0] = (int16_t)s16_at(&sc[SC_DESIRED_EYE + 0]);
    desired[1] = (int16_t)s16_at(&sc[SC_DESIRED_EYE + 2]);
    desired[2] = (int16_t)s16_at(&sc[SC_DESIRED_EYE + 4]);
    div[0] = 8; div[1] = 16; div[2] = 8;
    if (!ae_camera_smooth_eye(&work, desired, div))
        return 0;

    /* Normalized (smoothed eye - focus), XZ plane. */
    v[0] = (int16_t)wrap16((uint32_t)ae_camera_u16(&work, AE_CAM_EYE + 0) - (uint32_t)fx);
    v[1] = 0;
    v[2] = (int16_t)wrap16((uint32_t)ae_camera_u16(&work, AE_CAM_EYE + 4) - (uint32_t)fz);
    if (!ae_camera_normalize(math, v, &sq))
        return 0;

    if (i32_of(sq) <= 0x1DE83F) {
        /* focus + normalized * 350 / 1400, arithmetic >> 12, stored as halfwords. */
        ae_camera_set16(&work, AE_CAM_TARGET + 0, sar32(fx + mul32(v[0], 0x15E), 12));
        ae_camera_set16(&work, AE_CAM_TARGET + 4, sar32(fz + mul32(v[2], 0x15E), 12));
        ae_camera_set16(&work, AE_CAM_EYE + 0, sar32(fx + mul32(v[0], 0x578), 12));
        ae_camera_set16(&work, AE_CAM_EYE + 4, sar32(fz + mul32(v[2], 0x578), 12));
    }

    memcpy(slot, &work, sizeof work);
    memcpy(scratch, sc, sizeof sc);
    return 1;
}
