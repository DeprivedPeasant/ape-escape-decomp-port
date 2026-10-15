#include "native_camera_blend.h"

#include <string.h>

enum {
    CTL_SAVED_EYE    = 0x1C, /* s16[3] */
    CTL_SAVED_TARGET = 0x24, /* s16[3] */
    CTL_STYLE        = 0x2E, /* u8 */
    CTL_COUNTER      = 0x30  /* u16 */
};

static uint32_t rd_u16(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

/* Sign-extended halfword as a 32-bit two's complement bit pattern. */
static uint32_t rd_s16_u32(const uint8_t *p)
{
    uint32_t v = rd_u16(p);
    return (v ^ 0x8000u) - 0x8000u;
}

/* One axis: ((saved * c + cur * (0x80 - c)) with the original's low-word
 * wraparound, +0x7F when negative, arithmetic >> 7), as the low 16 bits. */
static uint16_t blend_axis(uint32_t saved, uint32_t cur, uint32_t c, uint32_t inv)
{
    uint32_t sum = saved * c + cur * inv;
    uint32_t neg = sum >> 31;
    uint32_t r;

    if (neg)
        sum += 0x7Fu;
    r = sum >> 7;
    if (neg)
        r |= 0xFE000000u; /* arithmetic shift: bit 31 is still the sign */
    return (uint16_t)(r & 0xFFFFu);
}

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

int ae_camera_apply_slot(uint8_t controls[0x50], const AeCameraSlot *active,
                         uint8_t eye8[8], uint8_t target8[8])
{
    uint32_t c;
    uint32_t inv;
    uint8_t cur[16];
    int i;

    if (!controls || !active || !eye8 || !target8)
        return 0;

    /* Snapshot the slot bytes first (the original copies them to the stack). */
    memcpy(cur, active->raw + AE_CAM_EYE, 8);
    memcpy(cur + 8, active->raw + AE_CAM_TARGET, 8);

    c = rd_u16(controls + CTL_COUNTER);
    if (c == 0) {
        memcpy(eye8, cur, 8);
        memcpy(target8, cur + 8, 8);
        return 1;
    }

    switch (controls[CTL_STYLE]) {
    case 1:
        c = (c < 0x30u) ? ((c * 7u) >> 3) : (c - 8u);
        break;
    case 2:
        c = (c * 3u) >> 2;
        break;
    case 3:
        c = 0;
        break;
    default:
        c = (c < 0x20u) ? (c - 1u) : (c - 3u);
        break;
    }
    c &= 0xFFFFu;
    put16(controls + CTL_COUNTER, (uint16_t)c);

    inv = 0x80u - c; /* wraps like the original's 32-bit subu */

    for (i = 0; i < 3; i++) {
        put16(eye8 + 2 * i,
              blend_axis(rd_s16_u32(controls + CTL_SAVED_EYE + 2 * i),
                         rd_s16_u32(cur + 2 * i), c, inv));
        put16(target8 + 2 * i,
              blend_axis(rd_s16_u32(controls + CTL_SAVED_TARGET + 2 * i),
                         rd_s16_u32(cur + 8 + 2 * i), c, inv));
    }
    return 1;
}
