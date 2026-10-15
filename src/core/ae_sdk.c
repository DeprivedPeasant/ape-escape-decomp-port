/* PSX SDK maths on the AeRam image, translated from the original instructions (see ae_sdk.h). */
#include "ae_sdk.h"

int32_t ae_sdk_VectorNormalS(AeRam *r, uint32_t in, uint32_t out) {
    int32_t normalized[3];
    int32_t sum = ae_sdk_normalize_core(r, ae_rds32(r, in), ae_rds32(r, in + 4),
                                       ae_rds32(r, in + 8), normalized);
    if (ae_ram_failed(r)) return 0;
    for (uint32_t i = 0; i < 3; ++i) ae_wr16(r, out + 2 * i, normalized[i]);
    return sum;
}

static int32_t s16(uint32_t v) { return (int32_t)(int16_t)(uint16_t)v; }
static int32_t shl32(int32_t v, uint32_t n) { return (int32_t)((uint32_t)v << (n & 31u)); }
static int32_t sra32(int32_t v, uint32_t n) {
    n &= 31u;
    return v < 0 ? (int32_t)~((~(uint32_t)v) >> n) : (int32_t)((uint32_t)v >> n);
}

static int32_t mul_s(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }   /* mult + mflo / multu + mflo */

/* GTE LZCR: leading sign bits (zeros for a non-negative value, ones for a negative one), 32 for 0 / -1. */
static int32_t lzcr(int32_t v) {
    uint32_t u = v < 0 ? ~(uint32_t)v : (uint32_t)v;
    int32_t n = 0;
    for (uint32_t bit = 0x80000000u; bit && !(u & bit); bit >>= 1) ++n;
    return n;
}

/* ---- VectorNormal core @80030210 --------------------------------------------------------------- */
int32_t ae_sdk_normalize_core(AeRam *r, int32_t x, int32_t y, int32_t z, int32_t out[3]) {
    int32_t ir[3] = {s16((uint32_t)x), s16((uint32_t)y), s16((uint32_t)z)};
    int64_t sum = (int64_t)(ir[0] * ir[0]) + (int64_t)(ir[1] * ir[1]);   /* add t3, t3, t4 */
    if (sum > INT32_MAX) { ae_ram_fail(r, "VectorNormal: add overflow (the original traps)"); return 0; }
    sum += (int64_t)(ir[2] * ir[2]);                                      /* add v0, t3, t5 */
    if (sum > INT32_MAX) { ae_ram_fail(r, "VectorNormal: add overflow (the original traps)"); return 0; }
    int32_t s = (int32_t)sum;
    int32_t v1 = lzcr(s) & ~1;                                            /* mtc2 v0, LZCS ; mfc2 v1, LZCR */
    int32_t t6 = sra32(31 - v1, 1);
    int32_t t3 = v1 - 24;
    int32_t t4;
    if (t3 < 0) t4 = sra32(s, (uint32_t)(24 - v1));
    else t4 = shl32(s, (uint32_t)t3);
    t4 = shl32((int32_t)((uint32_t)t4 - 64u), 1);
    int32_t ir0 = ae_rds16(r, (uint32_t)((uint32_t)0x800B0000u + (uint32_t)t4 - 25580u));
    for (int i = 0; i < 3; ++i) out[i] = sra32(ir0 * ir[i], (uint32_t)t6);   /* GPF sf=0, then srav */
    return s;
}

int32_t ae_sdk_VectorNormal(AeRam *r, uint32_t in, uint32_t out) {
    int32_t v[3] = {ae_rds32(r, in), ae_rds32(r, in + 4), ae_rds32(r, in + 8)};
    int32_t o[3] = {0, 0, 0};
    int32_t s = ae_sdk_normalize_core(r, v[0], v[1], v[2], o);
    if (ae_ram_failed(r)) return 0;
    ae_wr32(r, out, (uint32_t)o[0]);
    ae_wr32(r, out + 4, (uint32_t)o[1]);
    ae_wr32(r, out + 8, (uint32_t)o[2]);
    return s;
}

int32_t ae_sdk_VectorNormalSS(AeRam *r, uint32_t in, uint32_t out) {
    int32_t v[3] = {ae_rds16(r, in), ae_rds16(r, in + 2), ae_rds16(r, in + 4)};
    int32_t o[3] = {0, 0, 0};
    int32_t s = ae_sdk_normalize_core(r, v[0], v[1], v[2], o);
    if (ae_ram_failed(r)) return 0;
    ae_wr16(r, out, (uint32_t)o[0]);
    ae_wr16(r, out + 2, (uint32_t)o[1]);
    ae_wr16(r, out + 4, (uint32_t)o[2]);
    return s;
}

int32_t ae_sdk_square0_lane(int32_t v) {
    int32_t ir = s16((uint32_t)v);
    return ir * ir;
}

/* ---- SquareRoot0 @800303C0 --------------------------------------------------------------------- */
int32_t ae_sdk_SquareRoot0(AeRam *r, int32_t a0) {
    int32_t v0 = lzcr(a0);
    if (v0 == 32) return 0;                    /* beq v0, 32: the delay slot of the jr sets v0 = 0 */
    int32_t t2 = v0 & ~1;
    int32_t t1 = sra32(31 - t2, 1);
    int32_t t3 = t2 - 24;
    int32_t t4;
    if (t3 >= 0) t4 = shl32(a0, (uint32_t)t3);
    else t4 = sra32(a0, (uint32_t)(24 - t2));
    t4 = (int32_t)((uint32_t)t4 - 64u);        /* addi t4, t4, -64 (no overflow: |t4| < 256) */
    t4 = shl32(t4, 1);
    int32_t t5 = ae_rds16(r, (uint32_t)((uint32_t)0x800B0000u + (uint32_t)t4 - 6720u));
    t5 = shl32(t5, (uint32_t)t1);
    return (int32_t)((uint32_t)t5 >> 12);
}

/* ---- ratan2 @800145D0 --------------------------------------------------------------------------- */
static int32_t div_trap(AeRam *r, int32_t a, int32_t b) {
    if (b == 0 || (b == -1 && a == INT32_MIN)) {
        ae_ram_fail(r, "ratan2: divide trap (div by zero / INT_MIN / -1; the original executes break)");
        return 0;
    }
    return a / b;
}

int32_t ae_sdk_ratan2(AeRam *r, int32_t a0, int32_t a1) {
    int32_t a2 = 0, a3 = 0;
    if (a1 < 0) { a2 = 1; a1 = (int32_t)(0u - (uint32_t)a1); }
    if (a0 < 0) { a3 = 1; a0 = (int32_t)(0u - (uint32_t)a0); }
    int32_t v1;
    if (a1 == 0 && a0 == 0) return 0;
    if (a0 < a1) {
        int32_t v0;
        if ((a0 & 0x7fe00000) != 0) {
            v0 = sra32(a1, 10);
            a0 = div_trap(r, a0, v0);
            v0 = shl32(a0, 1);
        } else {
            v0 = shl32(a0, 10);
            a0 = div_trap(r, v0, a1);
            v0 = shl32(a0, 1);
        }
        v1 = ae_rds16(r, (uint32_t)((uint32_t)0x800B0000u + (uint32_t)v0 + 17456u));
    } else {
        int32_t v0;
        if ((a1 & 0x7fe00000) != 0) {
            v0 = sra32(a0, 10);
            a0 = div_trap(r, a1, v0);
            v0 = shl32(a0, 1);
        } else {
            v0 = shl32(a1, 10);
            a0 = div_trap(r, v0, a0);
            v0 = shl32(a0, 1);
        }
        v1 = ae_rds16(r, (uint32_t)((uint32_t)0x800B0000u + (uint32_t)v0 + 17456u));
        v1 = (int32_t)(1024u - (uint32_t)v1);
    }
    if (a2 != 0) v1 = (int32_t)(2048u - (uint32_t)v1);
    if (a3 != 0) v1 = (int32_t)(0u - (uint32_t)v1);
    return v1;
}

/* ---- ApplyMatrixSV @800300B0 -------------------------------------------------------------------- */
uint32_t ae_sdk_ApplyMatrixSV(AeRam *r, uint32_t m, uint32_t v, uint32_t out) {
    uint32_t w[5];
    for (int i = 0; i < 5; ++i) w[i] = ae_rd32(r, m + 4u * (uint32_t)i);
    int32_t rt[3][3] = {
        {s16(w[0]), s16(w[0] >> 16), s16(w[1])},
        {s16(w[1] >> 16), s16(w[2]), s16(w[2] >> 16)},
        {s16(w[3]), s16(w[3] >> 16), s16(w[4])},
    };
    /* lwc2 VXY0 / VZ0 (word at +0 holds x, y; word at +4 holds z in its low half) */
    uint32_t vxy = ae_rd32(r, v), vz = ae_rd32(r, v + 4);
    int32_t vec[3] = {s16(vxy), s16(vxy >> 16), s16(vz)};
    for (int i = 0; i < 3; ++i) {
        int64_t mac = (int64_t)rt[i][0] * vec[0] + (int64_t)rt[i][1] * vec[1] + (int64_t)rt[i][2] * vec[2];
        mac >>= 12;                                 /* sf = 1 */
        int32_t mac32 = (int32_t)mac;               /* MAC1..3 are read as 32-bit */
        int32_t ir = mac32 > 32767 ? 32767 : (mac32 < -32768 ? -32768 : mac32);   /* lm = 0 */
        ae_wr16(r, out + 2u * (uint32_t)i, (uint32_t)ir);
    }
    return out;
}

/* ---- rsin @8002FB50 / rcos @8002FC20 -------------------------------------------------------------- */
/* 4096 = full turn; the sine quarter-wave table (s16, 1025 entries from 0x800A9DB0) and the neighbouring
   tables the original reads for the other quadrants are read from the RAM image. */
#define SDK_SIN_T1 0x800A9DB0u
#define SDK_SIN_T2 0x800A8DB0u   /* 800A8DB0 + 2 * a: a in 2049..3072 */
#define SDK_COS_T3 0x800A95B0u   /* 800A95B0 + 2 * a: a in 1025..2048 */
#define SDK_COS_T4 0x800A85B0u   /* 800A85B0 + 2 * a: a in 3073..4095 */

static int32_t neg32(int32_t v) { return (int32_t)(0u - (uint32_t)v); }

/* the part of rsin at 8002FB8C (a0 already masked with 0xFFF) */
static int32_t rsin_core(AeRam *r, uint32_t a) {
    if (a < 2049u) {
        if (a < 1025u) return ae_rds16(r, SDK_SIN_T1 + (a << 1));
        return ae_rds16(r, SDK_SIN_T1 + ((2048u - a) << 1));
    }
    if (a < 3073u) return neg32(ae_rds16(r, SDK_SIN_T2 + (a << 1)));
    return neg32(ae_rds16(r, SDK_SIN_T1 + ((4096u - a) << 1)));
}

int32_t ae_sdk_rsin(AeRam *r, int32_t a0) {
    if (a0 < 0) {
        a0 = neg32(a0);
        return neg32(rsin_core(r, (uint32_t)a0 & 0xFFFu));
    }
    return rsin_core(r, (uint32_t)a0 & 0xFFFu);
}

int32_t ae_sdk_rcos(AeRam *r, int32_t a0) {
    if (a0 < 0) a0 = neg32(a0);
    uint32_t a = (uint32_t)a0 & 0xFFFu;
    if (a < 2049u) {
        if (a < 1025u) return ae_rds16(r, SDK_SIN_T1 + ((1024u - a) << 1));
        return neg32(ae_rds16(r, SDK_COS_T3 + (a << 1)));
    }
    if (a < 3073u) return neg32(ae_rds16(r, SDK_SIN_T1 + ((3072u - a) << 1)));
    return ae_rds16(r, SDK_COS_T4 + (a << 1));
}

/* ---- RotMatrixYXZ @80030FA0 ------------------------------------------------------------------------- */
/* Rotation matrix (9 x s16 at m) from the angles (x, y, z) of the three s16 at `ang`; only the rotation part of the matrix
   is written.  The table at 0x800AA5C0 holds one word per angle (low half sin, high half cos).  Returns m. */
#define SDK_SINCOS_TABLE 0x800AA5C0u
static void sincos_of(AeRam *r, int32_t angle, int32_t *s, int32_t *c) {
    uint32_t i;
    int32_t lo, hi;
    if (angle >= 0) {
        i = (uint32_t)angle & 0xFFFu;
        uint32_t w = ae_rd32(r, SDK_SINCOS_TABLE + (i << 2));
        lo = s16(w); hi = (int32_t)w >> 16;
        *s = lo;
    } else {
        i = (uint32_t)neg32(angle) & 0xFFFu;
        uint32_t w = ae_rd32(r, SDK_SINCOS_TABLE + (i << 2));
        lo = s16(w); hi = (int32_t)w >> 16;
        *s = neg32(lo);
    }
    *c = hi;
}

uint32_t ae_sdk_RotMatrixYXZ(AeRam *r, uint32_t ang, uint32_t m) {
    int32_t sx, cx, sy, cy, sz, cz;
    int32_t ax = ae_rds16(r, ang), ay = ae_rds16(r, ang + 2);
    sincos_of(r, ax, &sx, &cx);                       /* t3 / t0 (t6 = -t3) */
    sincos_of(r, ay, &sy, &cy);                       /* t4 / t1 */
    int32_t t6 = neg32(sx);
    ae_wr16(r, m + 10, (uint32_t)t6);                 /* m[1][2] = -sin x */
    int32_t az = ae_rds16(r, ang + 4);
    ae_wr16(r, m + 4, (uint32_t)sra32(mul_s(sy, cx), 12));      /* m[0][2] */
    sincos_of(r, az, &sz, &cz);                       /* t5 / t2 */
    ae_wr16(r, m + 16, (uint32_t)sra32(mul_s(cy, cx), 12));     /* m[2][2] */
    ae_wr16(r, m + 6, (uint32_t)sra32(mul_s(sz, cx), 12));      /* m[1][0] */
    ae_wr16(r, m + 8, (uint32_t)sra32(mul_s(cz, cx), 12));      /* m[1][1] */
    int32_t A = sra32(mul_s(sy, sx), 12);                        /* t8 */
    int32_t t6a = sra32(mul_s(A, sz), 12);
    int32_t t9 = sra32(mul_s(cy, cz), 12);
    ae_wr16(r, m + 0, (uint32_t)((uint32_t)t9 + (uint32_t)t6a));             /* m[0][0] */
    t9 = sra32(mul_s(cy, sz), 12);
    int32_t t7 = neg32(t9);
    t9 = sra32(mul_s(A, cz), 12);
    ae_wr16(r, m + 2, (uint32_t)((uint32_t)t7 + (uint32_t)t9));              /* m[0][1] */
    int32_t B = sra32(mul_s(cy, sx), 12);                        /* t8 */
    int32_t t6b = sra32(mul_s(B, cz), 12);
    t9 = sra32(mul_s(sy, sz), 12);
    ae_wr16(r, m + 14, (uint32_t)((uint32_t)t9 + (uint32_t)t6b));            /* m[2][1] */
    t9 = sra32(mul_s(sy, cz), 12);
    t7 = neg32(t9);
    t9 = sra32(mul_s(B, sz), 12);
    ae_wr16(r, m + 12, (uint32_t)((uint32_t)t7 + (uint32_t)t9));             /* m[2][0] */
    return m;
}

/* ---- RotMatrix @80030BD0 ---------------------------------------------------------------------------- */
/* Rotation matrix (9 x s16 at m) from the angles (x, y, z) of the three s16 at `ang` (the PSY-Q RotMatrix: the same sine / cosine table as
   RotMatrixYXZ, the products shifted right by 12; only the rotation part of the matrix is written).  Returns m. */
uint32_t ae_sdk_RotMatrix(AeRam *r, uint32_t ang, uint32_t m) {
    int32_t sx, cx, sy, cy, sz, cz;
    sincos_of(r, ae_rds16(r, ang), &sx, &cx);                  /* t3 / t0 */
    sincos_of(r, ae_rds16(r, ang + 2), &sy, &cy);              /* t6 (t4 = -t6) / t1 */
    int32_t ny = neg32(sy);
    ae_wr16(r, m + 4, (uint32_t)sy);                           /* m[0][2] = sin y */
    ae_wr16(r, m + 10, (uint32_t)sra32(neg32(mul_s(cy, sx)), 12));    /* m[1][2] = -(cos y sin x) >> 12 */
    sincos_of(r, ae_rds16(r, ang + 4), &sz, &cz);              /* t5 / t2 */
    ae_wr16(r, m + 16, (uint32_t)sra32(mul_s(cy, cx), 12));    /* m[2][2] */
    ae_wr16(r, m + 0, (uint32_t)sra32(mul_s(cz, cy), 12));     /* m[0][0] */
    ae_wr16(r, m + 2, (uint32_t)sra32(neg32(mul_s(sz, cy)), 12));     /* m[0][1] */
    int32_t A = sra32(mul_s(cz, ny), 12);                      /* t8 */
    int32_t t6 = sra32(mul_s(A, sx), 12);
    int32_t t9 = sra32(mul_s(sz, cx), 12);
    ae_wr16(r, m + 6, (uint32_t)((uint32_t)t9 - (uint32_t)t6));       /* m[1][0] */
    int32_t t7 = sra32(mul_s(A, cx), 12);
    t9 = sra32(mul_s(sz, sx), 12);
    ae_wr16(r, m + 12, (uint32_t)((uint32_t)t9 + (uint32_t)t7));      /* m[2][0] */
    int32_t B = sra32(mul_s(sz, ny), 12);                      /* t8 */
    t6 = sra32(mul_s(B, sx), 12);
    t9 = sra32(mul_s(cz, cx), 12);
    ae_wr16(r, m + 8, (uint32_t)((uint32_t)t9 + (uint32_t)t6));       /* m[1][1] */
    t7 = sra32(mul_s(B, cx), 12);
    t9 = sra32(mul_s(cz, sx), 12);
    ae_wr16(r, m + 14, (uint32_t)((uint32_t)t9 - (uint32_t)t7));      /* m[2][1] */
    return m;
}
