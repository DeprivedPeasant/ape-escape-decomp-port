/* Pure host-side render adapter for the native camera slot (see native_camera_render.md).
 * Converts the original camera slot eye / target (PSX units, Y negative-up) into the
 * viewer's orbit parameters, and gives an APPROXIMATE float projection of one point.
 * No global state, no dependencies beyond native_camera.h and the C standard headers. */
#ifndef AE_NATIVE_CAMERA_RENDER_H
#define AE_NATIVE_CAMERA_RENDER_H

#include <stdint.h>

#include "native_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Viewer orbit for the slot's eye / target. With t = (x, -y, z) / 256 of the slot target
 * and e likewise for the eye, delta = e - t:
 *   target   = t
 *   distance = |delta|
 *   yaw      = atan2(-delta.x, delta.z)            (degrees)
 *   pitch    = atan2(delta.y, hypot(delta.x, delta.z)) (degrees, inside (-90, 90))
 * so that the viewer modelview  Scale(-1,1,1) * Translate(0,0,-distance) * Rx(pitch) *
 * Ry(yaw) * Translate(-target)  places the eye exactly at e and keeps the original X
 * handedness. Returns 1 on success. Returns 0 with NO output written on a null argument,
 * a zero distance, a zero horizontal distance (looking straight up / down) or a
 * non-finite result. */
int ae_camera_render_orbit(const AeCameraSlot *slot, float target[3], float *yaw,
                           float *pitch, float *distance);

/* APPROXIMATE screen XY of a PSX point (s16, Y negative-up) for the slot's eye / target.
 * Uses the float basis of the original CameraLookAt (rows [hz,0,-hx],
 * [-hx*vy,h,-vy*hz], [h*hx,vy,h*hz]) without roll, the root matrix, SDK normalisation
 * or the GTE fixed point reciprocal. X = offset_x + focal * viewX / depth,
 * Y = offset_y + focal * viewY / depth, each truncated toward zero and clamped to the
 * original screen range [-1024, 1023]; *sxy = (X & 0xFFFF) | (Y & 0xFFFF) << 16.
 * Coordinates are already in PSX units: there is no 256 scale. Returns 1 on success.
 * Returns 0 with *sxy unchanged on a null argument, a non-positive / non-finite focal,
 * non-finite offsets, or a point whose depth is not greater than 1e-3. */
int ae_camera_project_approx(const AeCameraSlot *slot, const int16_t point[3], float focal,
                             float offset_x, float offset_y, uint32_t *sxy);

#ifdef __cplusplus
}
#endif

#endif
