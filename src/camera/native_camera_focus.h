/* Camera player direction / slot focus point helpers (see native_camera_focus.md).
 * Extends native_camera.h (slot type only; native_camera.c is not needed).
 * The player facing vector, region focus scratch and output are 8 byte
 * little-endian buffers (three s16 plus two padding bytes). No dependencies
 * beyond the C99 standard headers. */
#ifndef AE_NATIVE_CAMERA_FOCUS_H
#define AE_NATIVE_CAMERA_FOCUS_H

#include <stdint.h>

#include "native_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Slot offsets read by the focus helper. */
enum {
    AE_CAM_FOCUS_DIST   = 0xA4, /* s16 */
    AE_CAM_FOCUS_HEIGHT = 0xA6  /* s16 */
};

/* CameraGetPlayerDir (0x8004B880). facing8 is g_Player.facing (0x800EC1E0 +
 * 0x60 .. 0x67 in the original), out8 the destination.
 *   mode 0      : out8[0..7] = facing8[0..7] (needs facing8)
 *   mode 1, 2   : out8[0..5] = 0, out8[6..7] preserved
 *   any other   : no write
 * Modes are compared as signed 32 bit values. Only out8 is required for
 * modes other than 0; facing8 is required (and read) only for mode 0.
 * Returns 1 on success, 0 (and no write) if a required pointer is NULL.
 * out8 may alias facing8. */
int ae_camera_get_player_dir(int32_t mode, const uint8_t facing8[8], uint8_t out8[8]);

/* CameraSlotGetFocusPoint (0x8004B8F4). player is g_Player.pos[3] (s32),
 * facing8 as above, region_focus8 the scratch focus at 0x800E68C0.
 *   mode 0 : out8[0..5] = s16 triple
 *              x = (s16)player[0] + (s16)((s16 facing[0] * s16 slot[0xA4]) >> 12)
 *              y = (s16)(player[1] - 0x80) + slot s16 [0xA6]
 *              z = (s16)player[2] + (s16)((s16 facing[4] * s16 slot[0xA4]) >> 12)
 *            out8[6..7] preserved. Needs slot, player, facing8.
 *   mode 2 : out8[0..7] = region_focus8[0..7]. Needs region_focus8.
 *   mode 1, any other : no write, success. Needs only out8.
 * Returns 1 on success, 0 (and no write) if a required pointer is NULL.
 * All inputs are snapshotted before the first write, so out8 may alias any
 * input buffer (including the slot). */
int ae_camera_get_focus_point(const AeCameraSlot *slot, int32_t mode,
                              const int32_t player[3], const uint8_t facing8[8],
                              const uint8_t region_focus8[8], uint8_t out8[8]);

#ifdef __cplusplus
}
#endif

#endif
