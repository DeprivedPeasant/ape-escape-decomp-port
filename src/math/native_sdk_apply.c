#include "native_sdk_apply.h"

#include <stddef.h>

static int32_t s16_from_u16(uint32_t v)
{
    v &= 0xFFFFu;
    return (v >= 0x8000u) ? (int32_t)v - 0x10000 : (int32_t)v;
}

static int32_t rd_s16(const uint8_t *p)
{
    return s16_from_u16((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

static int64_t rd_s32(const uint8_t *p)
{
    uint32_t v = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                 ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return (v >= 0x80000000u) ? (int64_t)v - 4294967296LL : (int64_t)v;
}

static void wr_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static int64_t floor_div_4096(int64_t s)
{
    int64_t q = s / 4096;
    if (s % 4096 < 0)
        q -= 1;
    return q;
}

/* Low 32 bits of a signed 64-bit value, as a defined uint32 encoding. */
static uint32_t low32(int64_t v)
{
    return (uint32_t)((uint64_t)v & 0xFFFFFFFFu);
}

int ae_sdk_apply_matrix_lv(const uint8_t matrix[32], const uint8_t vector[16],
                           uint8_t out[16])
{
    int32_t m[9];
    int32_t high[3];
    int64_t low[3];
    uint8_t tmp[12];
    int i, j;

    if (matrix == NULL || vector == NULL || out == NULL)
        return 0;

    for (i = 0; i < 9; i++)
        m[i] = rd_s16(matrix + 2 * i);

    for (i = 0; i < 3; i++) {
        int64_t x = rd_s32(vector + 4 * i);
        /* C division/remainder truncate toward zero, matching the original
         * negate-split-negate; the high part is then truncated to s16 by the
         * IR load. */
        high[i] = s16_from_u16((uint32_t)((uint64_t)(x / 32768) & 0xFFFFu));
        low[i] = x % 32768;
    }

    for (i = 0; i < 3; i++) {
        int64_t hs = 0;
        int64_t ls = 0;
        for (j = 0; j < 3; j++) {
            hs += (int64_t)m[3 * i + j] * (int64_t)high[j];
            ls += (int64_t)m[3 * i + j] * low[j];
        }
        wr_u32(tmp + 4 * i, (low32(hs) << 3) + low32(floor_div_4096(ls)));
    }

    for (i = 0; i < 12; i++)
        out[i] = tmp[i];
    return 1;
}
