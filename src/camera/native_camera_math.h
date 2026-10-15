/* Original SDK math used by the native camera follow code (see
 * native_camera_math.md). The tables are read from the caller's PSX-EXE image;
 * no game data is embedded. No dependencies beyond the C99 standard headers. */
#ifndef AE_NATIVE_CAMERA_MATH_H
#define AE_NATIVE_CAMERA_MATH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AeCameraMathTables {
    int16_t sine[1025];       /* 0x800A9DB0, quarter wave, 4096 = 1.0 */
    int16_t atan[1025];       /* 0x800B4430, ratan table, 1024 ratio steps -> 0..512 */
    int16_t reciprocal[192];  /* 0x800A9C14, VectorNormalSS factor table */
    int loaded;               /* 1 only after a fully verified ae_camera_math_load */
} AeCameraMathTables;

/* Loads the three tables from a PSX-EXE image (file offset = addr - 0x80010000
 * + 0x800). Returns 1 and sets loaded = 1 on success. On any failure (null
 * argument, image too short, table sanity check failed) returns 0 and sets
 * loaded = 0. Checked: sine[0] == 0, sine[1024] == 4096, atan[0] == 0,
 * atan[1024] == 512. */
int ae_camera_math_load(AeCameraMathTables *t, const uint8_t *exe, size_t bytes);

/* rsin (0x8002FB50) / rcos (0x8002FC20): 12 bit angles (4096 = full turn), any
 * int32 angle (reduced like the original), result in -4096..4096. Return 0 if
 * t is null or not loaded. */
int32_t ae_camera_rsin(const AeCameraMathTables *t, int32_t angle);
int32_t ae_camera_rcos(const AeCameraMathTables *t, int32_t angle);

/* Atan2Safe (0x80046340) over ratan2 (0x800145D0): original argument order
 * (x, z). Writes the 12 bit angle to *out and returns 1. Returns 0 with *out
 * unchanged if t/out is null or not loaded, if z != 0 and x or z is INT32_MIN
 * (abs wraps in the original; outside the supported domain), or on a table
 * index outside 0..1024 / division trap. */
int ae_camera_atan2_safe(const AeCameraMathTables *t, int32_t x, int32_t z, int32_t *out);

/* VectorNormalSS (0x800301E0) in place. Stores the original squared length
 * (sum of the three squares) in *squared_length (may be NULL). Returns 1 on
 * success. Returns 0 with nothing changed if t/io is null or not loaded, or the
 * sum exceeds INT32_MAX (the original's add traps). A zero vector returns 1
 * with io zero and length 0. */
int ae_camera_normalize(const AeCameraMathTables *t, int16_t io[3], uint32_t *squared_length);

#ifdef __cplusplus
}
#endif

#endif
