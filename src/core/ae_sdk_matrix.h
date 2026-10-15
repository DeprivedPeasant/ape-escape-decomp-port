#ifndef AE_SDK_MATRIX_H
#define AE_SDK_MATRIX_H

#include "ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Matrix helpers whose original routines live outside ae_sdk.c (the latter is owned by another task). */
uint32_t ae_sdk_RotMatrixZYX(AeRam *r, uint32_t angles, uint32_t matrix); /* 8002FCC0 */
uint32_t ae_sdk_RotMatrixX(AeRam *r, int32_t angle, uint32_t matrix); /* 80030A30 */
uint32_t ae_sdk_RotMatrixY(AeRam *r, int32_t angle, uint32_t matrix); /* 80030890 */
uint32_t ae_sdk_RotMatrixZ(AeRam *r, int32_t angle, uint32_t matrix); /* 800306F0 */
uint32_t ae_sdk_CompMatrix(AeRam *r, uint32_t left, uint32_t right, uint32_t out); /* 8002FF50 */
uint32_t ae_sdk_ScaleMatrix(AeRam *r, uint32_t matrix, uint32_t scale); /* 80030E60 */
uint32_t ae_sdk_MatrixIdentity(AeRam *r, uint32_t matrix); /* 8004637C */
uint32_t ae_sdk_MatrixFromDirXZ(AeRam *r, uint32_t direction, uint32_t matrix); /* 80014804 */
uint32_t ae_sdk_MatrixLookDir(AeRam *r, uint32_t direction, uint32_t matrix); /* 8001484C */

#ifdef __cplusplus
}
#endif
#endif
