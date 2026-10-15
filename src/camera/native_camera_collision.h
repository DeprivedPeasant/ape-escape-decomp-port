/* Camera slot ground clamp helpers (see native_camera_collision.md).
 * Extends native_camera.h; the original math / ground queries are supplied by
 * the caller as callbacks. No dependencies beyond the C99 standard headers. */
#ifndef AE_NATIVE_CAMERA_COLLISION_H
#define AE_NATIVE_CAMERA_COLLISION_H

#include <stdint.h>

#include "native_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AeCameraQueries {
    void *user;
    /* VectorNormalSS (0x800301E0) called in place: io[3] is normalized and the
     * original's returned squared-length bits are returned. The output may be
     * any signed 16 bit values. */
    uint32_t (*normalize)(void *user, int16_t io[3]);
    /* SquareRoot0 (0x800303C0), signed 32 bit result. */
    int32_t (*sqrt)(void *user, uint32_t value);
    /* GetGroundHeight (0x80043400). Coordinates are wrapped signed 32 bit; the
     * helpers truncate the result to signed 16 bit like the original. */
    int32_t (*ground)(void *user, int32_t x, int32_t y, int32_t z);
} AeCameraQueries;

/* CameraSlotGroundClamp (0x80048F6C). focus is the scratch halfword triple at
 * 0x1F800018 / 1A / 1C. Needs normalize and ground. Returns 1 if executed,
 * 0 (and no slot write, no callback call) if slot, focus, queries or a
 * required callback is missing. */
int ae_camera_ground_clamp(AeCameraSlot *s, const int16_t focus[3],
                           const AeCameraQueries *q);

/* CameraSlotGroundClampNear (0x8004906C). desired_eye is the scratch halfword
 * triple at 0x1F800000 / 02 / 04. Needs normalize, sqrt and ground. Same
 * return convention as above. */
int ae_camera_ground_clamp_near(AeCameraSlot *s, const int16_t desired_eye[3],
                                int near, const AeCameraQueries *q);

#ifdef __cplusplus
}
#endif

#endif
