#ifndef AE_NATIVE_SDK_SQRT_H
#define AE_NATIVE_SDK_SQRT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pure integer reconstruction of the original SDK SquareRoot0 (0x800303C0).
 * The 192-entry s16 table is read by the caller-supplied original EXE image
 * (virtual 0x800AE5C0); no game data is embedded here.
 * See native_sdk_sqrt.md for semantics, domain and limits.
 */

#define AE_SDK_SQRT_TABLE_COUNT 192u
#define AE_SDK_SQRT_TABLE_VADDR 0x800AE5C0u

typedef struct AeSdkSqrtTables {
    int16_t values[192];
    int loaded;
} AeSdkSqrtTables;

/*
 * Loads the table from the EXE image (file offset vaddr - 0x80010000 + 0x800).
 * Returns 1 on success (loaded = 1). Returns 0 on NULL exe, short image or
 * malformed table; if `tables` is non-NULL it is then left with loaded = 0.
 * The table is staged and validated before it is copied into `tables`.
 */
int ae_sdk_sqrt_load(AeSdkSqrtTables *tables, const uint8_t *exe, size_t bytes);

/*
 * Original SquareRoot0: returns 1 and writes *out on success, else returns 0
 * and leaves *out untouched (NULL args, unloaded tables, value > INT32_MAX).
 */
int ae_sdk_sqrt0(const AeSdkSqrtTables *tables, uint32_t value, uint32_t *out);

#ifdef __cplusplus
}
#endif

#endif
