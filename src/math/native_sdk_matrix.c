#include "native_sdk_matrix.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Portable floor(v / 4096) for any int64 (C division truncates toward zero). */
static int64_t floor_div_4096(int64_t v)
{
    int64_t q = v / 4096;
    if (v % 4096 < 0) {
        q -= 1;
    }
    return q;
}

static int64_t decode_s16(const uint8_t *p)
{
    uint32_t u = (uint32_t)p[0] | ((uint32_t)p[1] << 8);
    return u >= 0x8000u ? (int64_t)u - 65536 : (int64_t)u;
}

static uint32_t decode_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int64_t decode_s32(const uint8_t *p)
{
    uint32_t u = decode_u32(p);
    return u >= 0x80000000u ? (int64_t)u - 4294967296LL : (int64_t)u;
}

/* Low 16 bits of a little-endian 32-bit word, as a signed value. */
static int64_t low16_signed_of_u32(uint32_t u)
{
    uint32_t low = u & 0xFFFFu;
    return low >= 0x8000u ? (int64_t)low - 65536 : (int64_t)low;
}

static int64_t saturate_s16(int64_t v)
{
    if (v < -32768) {
        return -32768;
    }
    if (v > 32767) {
        return 32767;
    }
    return v;
}

/* v must be within [-32768, 32767]. */
static void encode_s16(uint8_t *p, int64_t v)
{
    uint32_t u = v < 0 ? (uint32_t)(v + 65536) : (uint32_t)v;
    p[0] = (uint8_t)(u & 0xFFu);
    p[1] = (uint8_t)((u >> 8) & 0xFFu);
}

/* v must be within the int32 range. */
static void encode_s32(uint8_t *p, int64_t v)
{
    uint32_t u = v < 0 ? (uint32_t)(v + 4294967296LL) : (uint32_t)v;
    p[0] = (uint8_t)(u & 0xFFu);
    p[1] = (uint8_t)((u >> 8) & 0xFFu);
    p[2] = (uint8_t)((u >> 16) & 0xFFu);
    p[3] = (uint8_t)((u >> 24) & 0xFFu);
}

/*
 * Computes the rotation block into res[0..19] (padding included) from local
 * copies of the inputs. Result row r, column c =
 * sat16(floor(sum_k L[r][k] * R[k][c] / 4096)).
 */
static void compute_rotation(const uint8_t left[32], const uint8_t right[32],
                             uint8_t res[20])
{
    int64_t l[3][3];
    int64_t r[3][3];
    int64_t last = 0;
    int row;
    int col;
    int k;

    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++) {
            l[row][col] = decode_s16(left + 2 * (3 * row + col));
            r[row][col] = decode_s16(right + 2 * (3 * row + col));
        }
    }

    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++) {
            int64_t sum = 0;
            int64_t v;
            for (k = 0; k < 3; k++) {
                sum += l[row][k] * r[k][col];
            }
            v = saturate_s16(floor_div_4096(sum));
            encode_s16(res + 2 * (3 * row + col), v);
            last = v;
        }
    }

    /* Original stores the 32-bit IR3 of the last column: padding is the sign
     * extension of m33. */
    res[18] = last < 0 ? 0xFF : 0x00;
    res[19] = res[18];
}

int ae_sdk_mul_matrix0(const uint8_t left[32], const uint8_t right[32],
                       uint8_t out[32])
{
    uint8_t res[20];

    if (left == NULL || right == NULL || out == NULL) {
        return 0;
    }
    compute_rotation(left, right, res);
    memcpy(out, res, 20); /* out[20..31] intentionally untouched */
    return 1;
}

int ae_sdk_comp_matrix(const uint8_t left[32], const uint8_t right[32],
                       uint8_t out[32])
{
    uint8_t res[32];
    int64_t l[3][3];
    int64_t v[3];
    int64_t lt[3];
    int64_t t[3];
    int row;
    int col;
    int k;

    if (left == NULL || right == NULL || out == NULL) {
        return 0;
    }

    /* Read everything before any write (aliasing support). */
    compute_rotation(left, right, res);
    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++) {
            l[row][col] = decode_s16(left + 2 * (3 * row + col));
        }
        lt[row] = decode_s32(left + 20 + 4 * row);
        v[row] = low16_signed_of_u32(decode_u32(right + 20 + 4 * row));
    }

    for (row = 0; row < 3; row++) {
        int64_t dot = 0;
        for (k = 0; k < 3; k++) {
            dot += l[row][k] * v[k];
        }
        /* MAC result, unclamped; |value| <= 786432. */
        t[row] = floor_div_4096(dot) + lt[row];
        if (t[row] < (int64_t)INT32_MIN || t[row] > (int64_t)INT32_MAX) {
            return 0; /* original would trap (MIPS ADD); fail closed */
        }
    }

    for (row = 0; row < 3; row++) {
        encode_s32(res + 20 + 4 * row, t[row]);
    }
    memcpy(out, res, 32);
    return 1;
}
