#ifndef AE_NATIVE_SDK_MATRIX_H
#define AE_NATIVE_SDK_MATRIX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pure integer helpers for the original SDK matrix routines.
 * Matrix bytes (32): 9 little-endian s16 rotation at 0..17 (row-major),
 * 2 padding bytes at 18..19, 3 little-endian s32 translation at 20..31.
 * See native_sdk_matrix.md for semantics, domain and limits.
 *
 * Both return 1 on success, 0 on failure. On failure `out` is untouched.
 * Exactly equal input/output pointers are supported; partial overlap is not.
 */

/* Original MulMatrix0 (0x80031830): rotation only; out[20..31] unchanged. */
int ae_sdk_mul_matrix0(const uint8_t left[32], const uint8_t right[32],
                       uint8_t out[32]);

/* Original CompMatrix (0x8002FF50): rotation plus translation, all 32 bytes. */
int ae_sdk_comp_matrix(const uint8_t left[32], const uint8_t right[32],
                       uint8_t out[32]);

#ifdef __cplusplus
}
#endif

#endif
