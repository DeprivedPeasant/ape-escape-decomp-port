/* Portable original CameraSlotUpdate (0x8004CA70) adapter (see
 * native_camera_motion.md). Pure function over the exact 0xB0-byte slot; the
 * original CameraSlotGetFocusPoint (0x8004B8F4) and CallIndexedHandler
 * (0x8004D880) are supplied by the caller. No dependencies beyond the C99
 * standard headers and the native_camera / native_camera_math modules. */
#ifndef AE_NATIVE_CAMERA_MOTION_H
#define AE_NATIVE_CAMERA_MOTION_H

#include <stdint.h>

#include "native_camera.h"
#include "native_camera_math.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AeCameraMotionQueries {
    void *user;
    /* Original CameraSlotGetFocusPoint(slot, mode, out): must write all 8
     * bytes (3 x s16 plus the 2 padding bytes). Return nonzero on success. */
    int (*focus)(void *user, const AeCameraSlot *slot, uint8_t mode, uint8_t out8[8]);
    /* Original CallIndexedHandler(slot, id): may modify the slot it is given.
     * Return nonzero if it ran to completion. */
    int (*indexed)(void *user, AeCameraSlot *slot, uint8_t id);
} AeCameraMotionQueries;

/* CameraSlotUpdate. player is g_Player.pos (full 32 bit, only read in script
 * states 8 and 9), stereo the byte at 0x800AF131, tables / queries are only
 * required where the executed path reaches them (see the .md).
 * Returns 1 if the update ran, 0 if a required dependency is missing, a
 * callback failed, or the SDK normalize left its supported domain. On 0 the
 * slot is unchanged. */
int ae_camera_slot_update(AeCameraSlot *slot, const int32_t player[3], uint8_t stereo,
                          const AeCameraMathTables *tables,
                          const AeCameraMotionQueries *queries);

#ifdef __cplusplus
}
#endif

#endif
