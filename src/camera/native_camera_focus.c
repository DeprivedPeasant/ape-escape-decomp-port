/* Camera player direction / slot focus point helpers (see native_camera_focus.md). */
#include "native_camera_focus.h"

#include <stddef.h>

static int32_t s16_at(const uint8_t *p)
{
    uint32_t v = (uint32_t)p[0] | ((uint32_t)p[1] << 8);
    return v >= 0x8000u ? (int32_t)v - 0x10000 : (int32_t)v;
}

/* Arithmetic right shift without relying on implementation-defined behavior. */
static int32_t sar32(int32_t v, unsigned n)
{
    return v >= 0 ? (int32_t)((uint32_t)v >> n) : ~(int32_t)((uint32_t)(~v) >> n);
}

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

int ae_camera_get_player_dir(int32_t mode, const uint8_t facing8[8], uint8_t out8[8])
{
    uint8_t tmp[8];
    size_t i;

    if (!out8)
        return 0;
    if (mode == 0) {
        if (!facing8)
            return 0;
        for (i = 0; i < 8; i++)
            tmp[i] = facing8[i];
        for (i = 0; i < 8; i++)
            out8[i] = tmp[i];
    } else if (mode == 1 || mode == 2) {
        for (i = 0; i < 6; i++)
            out8[i] = 0;
    }
    return 1;
}

int ae_camera_get_focus_point(const AeCameraSlot *slot, int32_t mode,
                              const int32_t player[3], const uint8_t facing8[8],
                              const uint8_t region_focus8[8], uint8_t out8[8])
{
    size_t i;

    if (!out8)
        return 0;
    if (mode == 0) {
        uint8_t dir[8];
        uint32_t px, py, pz, height;
        int32_t dist, dx, dz;
        uint32_t x, y, z;

        if (!slot || !player || !facing8)
            return 0;
        /* Snapshot every input before writing (out8 may alias any of them). */
        px = (uint32_t)player[0];
        py = (uint32_t)player[1];
        pz = (uint32_t)player[2];
        dist = s16_at(&slot->raw[AE_CAM_FOCUS_DIST]);
        height = (uint32_t)slot->raw[AE_CAM_FOCUS_HEIGHT] |
                 ((uint32_t)slot->raw[AE_CAM_FOCUS_HEIGHT + 1] << 8);
        /* The original calls CameraGetPlayerDir(slot, 0, local), a full copy. */
        if (!ae_camera_get_player_dir(0, facing8, dir))
            return 0;
        /* |product| <= 2^30, so the 32 bit multiply cannot overflow. */
        dx = sar32(s16_at(&dir[0]) * dist, 12);
        dz = sar32(s16_at(&dir[4]) * dist, 12);

        x = (px & 0xFFFFu) + (uint32_t)dx;
        y = ((py - 0x80u) & 0xFFFFu) + height;
        z = (pz & 0xFFFFu) + (uint32_t)dz;
        put16(&out8[0], x);
        put16(&out8[2], y);
        put16(&out8[4], z);
        /* out8[6..7] untouched. */
    } else if (mode == 2) {
        uint8_t tmp[8];

        if (!region_focus8)
            return 0;
        for (i = 0; i < 8; i++)
            tmp[i] = region_focus8[i];
        for (i = 0; i < 8; i++)
            out8[i] = tmp[i];
    }
    return 1;
}
