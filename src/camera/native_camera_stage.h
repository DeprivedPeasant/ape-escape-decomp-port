/* Stage camera composition (local CameraResetAll subset + the supported part of
 * CameraUpdate); see native_camera_stage.md. Composes the validated native_camera_*
 * modules. No game data embedded; the area blob is borrowed. */
#ifndef AE_NATIVE_CAMERA_STAGE_H
#define AE_NATIVE_CAMERA_STAGE_H

#include <stddef.h>
#include <stdint.h>

#include "native_camera_host.h"
#include "native_camera_lifecycle.h"
#include "native_camera_regions.h"

#ifdef __cplusplus
extern "C" {
#endif

/* AeCameraStage.status bits (OR of every failure of the last failed call, 0 after
 * a successful step / init). The low nibble equals the AE_CAMERA_FOLLOW_* bits. */
enum {
    AE_CAMERA_STAGE_MISSING             = 0x01, /* null / not ready / unbound host, allocation failure, unusable call */
    AE_CAMERA_STAGE_UNSUPPORTED_HANDLER = 0x02, /* indexed handler id != 3, or the follow gadget variant */
    AE_CAMERA_STAGE_MATH_DOMAIN         = 0x04, /* normalize / atan / division left the supported domain */
    AE_CAMERA_STAGE_UNSUPPORTED_MODE    = 0x08, /* selected kind 2 / 3, or a kind outside 0..5 */
    AE_CAMERA_STAGE_INVALID             = 0x10  /* invalid area data / ids / indices, or a service failure */
};

typedef struct AeCameraStage {
    AeCameraRegionFrame frame;   /* controls, region focus, the 12 slot bank */
    AeCameraRegions regions;     /* owned heap copy of the CAM records (mutable flags) */
    const uint8_t *blob;         /* borrowed area blob: must outlive the stage */
    size_t blob_size;
    const uint8_t *script_table; /* borrowed, inside blob; NULL when there are no regions */
    size_t script_table_bytes;   /* table start .. blob end */
    uint32_t original_table_base;/* opaque tag 0x80154000 + table offset, never dereferenced */
    uint8_t eye8[8];             /* current camera eye / target SVECTOR bytes (incl. padding) */
    uint8_t target8[8];
    uint32_t status;
    int ready;
} AeCameraStage;

/* Local CameraResetAll subset. The stage must be zero-initialized or freed (a
 * ready stage is rejected). host must be ready with queries.user == host; it is
 * only read (its slot seeds eye / target) and is never moved or rebound. blob
 * must stay valid and unmodified for the life of the stage. Returns 1 on success;
 * 0 leaves the stage unchanged except for status, with nothing leaked. */
int ae_camera_stage_init(AeCameraStage *stage, AeCameraHost *host, const uint8_t *blob,
                         size_t size, const AeCameraPlayerSnapshot *snapshot);

/* Releases the owned records and zeroes the stage. Safe on NULL / zeroed stages. */
void ae_camera_stage_free(AeCameraStage *stage);

/* One frame of the supported CameraUpdate subset. The caller supplies the
 * non-snapshot inputs in host->frame beforehand (state, flags, pad, stick,
 * projected_sxy of the previous view, ...). Returns 1 on success, with host->slot
 * = the selected bank slot whose eye / target are the blended stage vectors.
 * Returns 0 on failure with stage, regions, eye / target and host slot / frame /
 * shadow / status exactly as before the call, and the failure bits in stage->status. */
int ae_camera_stage_step(AeCameraStage *stage, AeCameraHost *host,
                         const AeCameraPlayerSnapshot *snapshot, int is_flight_area);

#ifdef __cplusplus
}
#endif

#endif
