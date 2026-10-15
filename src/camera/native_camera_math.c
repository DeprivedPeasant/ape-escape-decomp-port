/* Original SDK math for the native camera (see native_camera_math.md). */
#include "native_camera_math.h"

#define EXE_BASE 0x80010000u
#define EXE_HDR 0x800u
#define SINE_ADDR 0x800A9DB0u
#define ATAN_ADDR 0x800B4430u
#define RECIP_ADDR 0x800A9C14u

static int16_t s16_from_u32(uint32_t u)
{
    u &= 0xFFFFu;
    return u >= 0x8000u ? (int16_t)((int32_t)u - 0x10000) : (int16_t)u;
}

/* Arithmetic (flooring) right shift without relying on implementation-defined
 * signed shifts. n is 0..31. */
static int32_t sar32(int32_t v, unsigned n)
{
    return v >= 0 ? (int32_t)((uint32_t)v >> n) : ~(int32_t)((uint32_t)(~v) >> n);
}

static int read_table(int16_t *dst, unsigned count, uint32_t addr,
                      const uint8_t *exe, size_t bytes)
{
    size_t off = (size_t)(addr - EXE_BASE + EXE_HDR);
    size_t len = (size_t)count * 2u;
    unsigned i;

    if (off > bytes || len > bytes - off)
        return 0;
    for (i = 0; i < count; i++) {
        const uint8_t *p = exe + off + (size_t)i * 2u;
        dst[i] = s16_from_u32((uint32_t)p[0] | ((uint32_t)p[1] << 8));
    }
    return 1;
}

int ae_camera_math_load(AeCameraMathTables *t, const uint8_t *exe, size_t bytes)
{
    if (!t)
        return 0;
    t->loaded = 0;
    if (!exe)
        return 0;
    if (!read_table(t->sine, 1025, SINE_ADDR, exe, bytes) ||
        !read_table(t->atan, 1025, ATAN_ADDR, exe, bytes) ||
        !read_table(t->reciprocal, 192, RECIP_ADDR, exe, bytes))
        return 0;
    if (t->sine[0] != 0 || t->sine[1024] != 4096 ||
        t->atan[0] != 0 || t->atan[1024] != 512)
        return 0;
    t->loaded = 1;
    return 1;
}

/* sin_1 (0x8002FB8C), angle already 0..0xFFF. Indices stay within 0..1024. */
static int32_t sin_core(const AeCameraMathTables *t, uint32_t a)
{
    if (a <= 0x800u) {
        if (a <= 0x400u)
            return t->sine[a];
        return t->sine[0x800u - a];
    }
    if (a <= 0xC00u)
        return -(int32_t)t->sine[a - 0x800u];
    return -(int32_t)t->sine[0x1000u - a];
}

int32_t ae_camera_rsin(const AeCameraMathTables *t, int32_t angle)
{
    if (!t || !t->loaded)
        return 0;
    if (angle < 0)
        return -sin_core(t, (0u - (uint32_t)angle) & 0xFFFu);
    return sin_core(t, (uint32_t)angle & 0xFFFu);
}

int32_t ae_camera_rcos(const AeCameraMathTables *t, int32_t angle)
{
    uint32_t a;

    if (!t || !t->loaded)
        return 0;
    a = (angle < 0 ? (0u - (uint32_t)angle) : (uint32_t)angle) & 0xFFFu;
    if (a <= 0x800u) {
        if (a <= 0x400u)
            return t->sine[0x400u - a];
        return -(int32_t)t->sine[a - 0x400u];
    }
    if (a <= 0xC00u)
        return -(int32_t)t->sine[0xC00u - a];
    return t->sine[a - 0xC00u];
}

/* Quotient step shared by the four ratan2 division paths. Original div traps
 * (divide by zero, INT32_MIN / -1) become failures. Returns table index or -1. */
static int32_t ratan_index(int32_t num, int32_t den)
{
    int32_t q;

    if (den == 0 || (den == -1 && num == INT32_MIN))
        return -1;
    q = num / den; /* truncating, like div */
    if (q < 0 || q > 1024)
        return -1;
    return q;
}

int ae_camera_atan2_safe(const AeCameraMathTables *t, int32_t x, int32_t z, int32_t *out)
{
    int32_t a0, a1, ang, idx;
    int xneg = 0, yneg = 0;

    if (!t || !t->loaded || !out)
        return 0;
    if (z == 0) {
        *out = x < 0 ? 0x800 : 0;
        return 1;
    }
    if (x == INT32_MIN || z == INT32_MIN)
        return 0;

    /* ratan2 (0x800145D0) with a0 = y = z, a1 = x */
    a0 = z;
    a1 = x;
    if (a1 < 0) {
        xneg = 1;
        a1 = -a1;
    }
    if (a0 < 0) {
        yneg = 1;
        a0 = -a0;
    }
    if (a1 == 0 && a0 == 0) {
        *out = 0;
        return 1;
    }
    if (a0 < a1) {
        if ((a0 & 0x7FE00000) != 0)
            idx = ratan_index(a0, sar32(a1, 10));
        else
            idx = ratan_index((int32_t)((uint32_t)a0 << 10), a1);
        if (idx < 0)
            return 0;
        ang = t->atan[idx];
    } else {
        if ((a1 & 0x7FE00000) != 0)
            idx = ratan_index(a1, sar32(a0, 10));
        else
            idx = ratan_index((int32_t)((uint32_t)a1 << 10), a0);
        if (idx < 0)
            return 0;
        ang = 0x400 - t->atan[idx];
    }
    if (xneg)
        ang = 0x800 - ang;
    if (yneg)
        ang = -ang;
    *out = ang;
    return 1;
}

int ae_camera_normalize(const AeCameraMathTables *t, int16_t io[3], uint32_t *squared_length)
{
    int32_t c[3], factor, prod;
    uint64_t sum64;
    uint32_t sum, lz, v1, tmp;
    unsigned t6, shift;
    int i;

    if (!t || !t->loaded || !io)
        return 0;
    for (i = 0; i < 3; i++)
        c[i] = io[i];
    /* SQR0: MACn = IRn * IRn, each at most 2^30 */
    sum64 = 0;
    for (i = 0; i < 3; i++)
        sum64 += (uint64_t)((int64_t)c[i] * c[i]);
    if (sum64 > (uint64_t)INT32_MAX)
        return 0; /* original add overflow exception */
    sum = (uint32_t)sum64;

    if (sum == 0) {
        io[0] = io[1] = io[2] = 0;
        if (squared_length)
            *squared_length = 0;
        return 1;
    }

    /* LZCR of a positive value, rounded down to even */
    lz = 0;
    for (tmp = sum; (tmp & 0x80000000u) == 0; tmp <<= 1)
        lz++;
    v1 = lz & ~1u;
    t6 = (31u - v1) >> 1;
    if (v1 >= 24u)
        tmp = sum << (v1 - 24u);
    else
        tmp = sum >> (24u - v1);
    if (tmp < 0x40u || tmp - 0x40u >= 192u)
        return 0;
    factor = t->reciprocal[tmp - 0x40u];

    shift = t6 & 31u;
    for (i = 0; i < 3; i++) {
        /* GPF sf=0: MAC = IR * IR0; a 16x16 product always fits 32 bits */
        prod = (int32_t)((int64_t)c[i] * factor);
        io[i] = s16_from_u32((uint32_t)sar32(prod, shift));
    }
    if (squared_length)
        *squared_length = sum;
    return 1;
}
