#ifndef AE_SDK_H
#define AE_SDK_H
/*
 * PSX SDK maths used by the ported gameplay code, translated from the original instruction listings and run on
 * the byte-exact RAM image (AeRam).  The lookup tables are read from the image (no game data in the repository).
 * Every function names its original address; they are diffed against the original code by the check scripts of
 * the modules that use them (tools/mips/check_monkey_*.py), GTE parts through the PCSX-ReARMed oracle.
 *
 * Failure model (as in ae_ram.h): where the original traps (signed add overflow, divide by zero, INT_MIN / -1)
 * the port sets the AeRam error flag and returns 0.
 */
#include "ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AE_SDK_TABLE_NORMAL 0x800A9C14u  /* s16[192], VectorNormal* core */
#define AE_SDK_TABLE_SQRT 0x800AE5C0u    /* s16[192], SquareRoot0 */
#define AE_SDK_TABLE_ATAN 0x800B4430u    /* s16[1025], ratan2 */

/* Core of VectorNormal / VectorNormalSS @80030210 on the three raw register values (only the low 16 bits of each
   enter the GTE as IR1..3): out[i] = (IR0 * s16(v[i])) >> shift, before the store narrows it.  Returns the sum of
   squares (what the original leaves in $v0).  The original `add`s trap on signed overflow: error flag, returns 0. */
int32_t ae_sdk_normalize_core(AeRam *r, int32_t x, int32_t y, int32_t z, int32_t out[3]);
/* VectorNormal @800301B0: three s32 in -> three s32 out (RAM addresses). Returns the sum of squares. */
int32_t ae_sdk_VectorNormal(AeRam *r, uint32_t in, uint32_t out);
/* VectorNormalSS @800301E0: three s16 in -> three s16 out. Returns the sum of squares. */
int32_t ae_sdk_VectorNormalSS(AeRam *r, uint32_t in, uint32_t out);
/* VectorNormalS @8003019C: three s32 in, three s16 out. */
int32_t ae_sdk_VectorNormalS(AeRam *r, uint32_t in, uint32_t out);
/* One lane of Square0 @80014498 (GTE SQR, sf = 0): IR = s16(v), MAC = IR * IR. */
int32_t ae_sdk_square0_lane(int32_t v);
/* SquareRoot0 @800303C0 (integer square root through LZCR and the table; 0 and -1 return 0). */
int32_t ae_sdk_SquareRoot0(AeRam *r, int32_t a0);
/* ratan2 @800145D0: (y, x) -> angle in 4096 = full circle, as the original table lookup. */
int32_t ae_sdk_ratan2(AeRam *r, int32_t y, int32_t x);
/* ApplyMatrixSV @800300B0: out(3 x s16) = saturate((RT(m) * v(3 x s16)) >> 12) (GTE MVMVA sf=1, no translation, lm=0: MAC = sum >> 12).
   The first 20 bytes of the matrix are the rotation, as in the original; returns `out`. */
uint32_t ae_sdk_ApplyMatrixSV(AeRam *r, uint32_t m, uint32_t v, uint32_t out);

/* rsin @8002FB50 / rcos @8002FC20: 4096 = full turn, result +-4096 (tables read from the RAM image). */
int32_t ae_sdk_rsin(AeRam *r, int32_t a0);
int32_t ae_sdk_rcos(AeRam *r, int32_t a0);
/* RotMatrixYXZ @80030FA0: ang = three s16 (x, y, z); writes the nine s16 of the rotation part of the matrix at m, returns m. */
uint32_t ae_sdk_RotMatrixYXZ(AeRam *r, uint32_t ang, uint32_t m);
/* RotMatrix @80030BD0: like RotMatrixYXZ for the order of the PSY-Q RotMatrix; ang = three s16 (x, y, z), writes the nine s16 of the rotation, returns m. */
uint32_t ae_sdk_RotMatrix(AeRam *r, uint32_t ang, uint32_t m);

#ifdef __cplusplus
}
#endif
#endif
