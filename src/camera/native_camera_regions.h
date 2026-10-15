/* Area CAM region loader and CameraRegionTest (see native_camera_regions.md).
 * Source-backed: CameraResetAll (0x80047754) name linking and CameraRegionTest
 * (0x8004BD98). Region lifecycle, slots, scripts and blending are NOT here.
 * No dependencies beyond the C11 standard headers; no game data embedded. */
#ifndef AE_NATIVE_CAMERA_REGIONS_H
#define AE_NATIVE_CAMERA_REGIONS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AE_CAMERA_REGION_STRIDE 0x30u

typedef struct AeCameraRegions {
    uint8_t *records;     /* owned heap copy, count * 0x30 bytes, NULL if count == 0 */
    uint16_t count;       /* CAM header +4 */
    uint16_t named_count; /* name table entry count (0 when count == 0) */
    uint32_t cam_offset;  /* area blob +0x4D8, kept even when the section is empty */
} AeCameraRegions;

/* Release owned records and zero the struct. Safe on a zero-initialized struct
 * and on NULL. */
void ae_camera_regions_free(AeCameraRegions *r);

/* Load the CAM section of an area blob (u32 byte offset at +0x4D8). The blob
 * is never modified. Returns 1 on success, 0 on failure. On failure *r is left
 * unchanged and ae_camera_regions_error() describes the reason; on success the
 * previous records are released and the new copy is owned by *r. *r must be
 * zero-initialized or previously loaded. Offset 0 is an empty success. */
int ae_camera_regions_load(AeCameraRegions *r, const uint8_t *area_blob,
                           size_t size);

/* Last load error (static bounded string, not thread safe; "" after success). */
const char *ae_camera_regions_error(void);

/* CameraRegionTest (0x8004BD98) over 0x30-byte records (as produced by the
 * loader). point is x, y, z as signed 16 bit. Returns the first matching
 * index, -1 for no hit, -2 for invalid arguments (NULL point, NULL records with
 * count > 0, or bytes < count * 0x30). count 0 with records NULL / bytes 0 is
 * valid and returns -1. */
int32_t ae_camera_region_test(const uint8_t *records, size_t bytes,
                              uint16_t count, const int16_t point[3]);

#ifdef __cplusplus
}
#endif

#endif
