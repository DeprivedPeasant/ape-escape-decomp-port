#ifndef AE_VECNORM_H
#define AE_VECNORM_H
/*
 * Core of the SDK normalise routines, translated instruction by instruction from the original code:
 *
 *   VectorNormalS  @8003019C   lw x3 / sh x3 (SVECTOR in, SVECTOR out; reads 32-bit words, low halves are used)
 *   VectorNormal   @800301B0   lw x3 / sw x3 (VECTOR in, VECTOR out)
 *   VectorNormalSS @800301E0   lh x3 / sh x3 (SVECTOR in, SVECTOR out)
 *   core           @80030210   (the three entry points above all `jal` it with the vector in $t0 / $t1 / $t2)
 *
 * Core, from the listing (GTE: mtc2 IR1..3, SQR sf=0, mfc2 MAC1..3, LZCS / LZCR, GPF sf=0):
 *     IR1..3 = low 16 bits of the three registers (sign extended)
 *     MACn   = IRn * IRn                      (SQR, sf = 0)
 *     v0     = (MAC1 + MAC2) + MAC3           (two `add`s: each traps on signed 32-bit overflow)
 *     v1     = LZCR(v0) & ~1                  (v0 >= 0 here, so LZCR = count of leading zeros, 32 for 0)
 *     t6     = (31 - v1) >> 1                 (sra)
 *     t3     = v1 - 24
 *     t4     = t3 < 0 ? v0 >>(sra) (24 - v1) : v0 <<(sllv) t3
 *     IR0    = (s16) lut[t4 - 64]             (lh at 0x800B0000 + 2 * (t4 - 64) - 25580 = 0x800A9C14 + 2 * (t4 - 64))
 *     MACn   = IR0 * IRn                      (GPF sf = 0)
 *     outn   = MACn >>(srav) t6               (arithmetic, shift amount t6 & 31)
 *     return v0                               (THE SUM OF SQUARES, not a length: $v0 is never overwritten)
 *
 * The lookup table has 192 s16 entries (t4 - 64 = 0..191 for every v0 >= 1); it is floor(sqrt(2^30 / (idx + 64))) (all 192
 * entries identical to the EXE bytes, checked by tools/mips/check_player_input.py).  For v0 == 0 the (out-of-table) read is
 * irrelevant because every IRn is 0.
 *
 * Header only so that both the player (native_player.c, no RAM image) and the RAM-image ports (ae_pad.c) share it.
 */
#include <stdint.h>

#define AE_VECNORM_LUT_ADDR 0x800A9C14u   /* g_NormTable, 192 x s16 */
#define AE_VECNORM_LUT_COUNT 192

typedef struct {
    int32_t sum;        /* v0 (valid when !trapped) */
    int trapped;        /* an `add` overflowed: the original raises an exception */
    int32_t index;      /* lut index t4 - 64, valid when sum != 0 */
    int32_t shift;      /* t6 (may be -1 for sum == 0: srav uses & 31) */
} AeVecNormPlan;

/* ir[] are the low 16 bits of the three input registers, sign extended. */
static inline AeVecNormPlan ae_vecnorm_plan(const int32_t ir[3]) {
    AeVecNormPlan p = {0, 0, -64, 0};
    int64_t s = (int64_t)ir[0] * ir[0] + (int64_t)ir[1] * ir[1];        /* add t3, t3, t4 */
    if (s > INT32_MAX) { p.trapped = 1; return p; }
    s += (int64_t)ir[2] * ir[2];                                        /* add v0, t3, t5 */
    if (s > INT32_MAX) { p.trapped = 1; return p; }
    int32_t v0 = (int32_t)s;
    int32_t lz = 32;
    if (v0 != 0) { lz = 0; for (uint32_t v = (uint32_t)v0; !(v & 0x80000000u); v <<= 1) ++lz; }
    int32_t v1 = lz & ~1;
    int32_t t3 = v1 - 24, t4;
    if (t3 < 0) { t3 = 24 - v1; t4 = v0 >> (t3 & 31); }
    else t4 = (int32_t)((uint32_t)v0 << (t3 & 31));
    p.sum = v0;
    p.shift = (31 - v1) >> 1;
    p.index = t4 - 64;
    return p;
}

/* out = (IR0 * ir) >> shift with IR0 the table entry; ir0 = 0 is used for sum == 0 (result 0 either way). */
static inline int32_t ae_vecnorm_apply(int32_t ir0, int32_t ir, int32_t shift) {
    return (ir0 * ir) >> (shift & 31);
}

#endif
