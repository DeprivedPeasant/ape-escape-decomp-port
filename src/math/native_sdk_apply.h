#ifndef AE_NATIVE_SDK_APPLY_H
#define AE_NATIVE_SDK_APPLY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pure integer helper for original SDK ApplyMatrixLV (0x80030450).
 * Matrix bytes (32): 9 little-endian s16 rotation at 0..17 (row-major);
 * bytes 18..31 (padding, translation) are not read.
 * Vector bytes (16): 3 little-endian s32 at 0..11; bytes 12..15 are padding.
 * See native_sdk_apply.md for semantics (split high/low GTE pipeline).
 *
 * Returns 1 on success, 0 if any pointer is NULL (out untouched).
 * On success writes 3 little-endian s32 to out[0..11]; out[12..15] unchanged.
 * out may equal vector exactly; partial overlap is not supported.
 */
int ae_sdk_apply_matrix_lv(const uint8_t matrix[32], const uint8_t vector[16],
                           uint8_t out[16]);

#ifdef __cplusplus
}
#endif

#endif
