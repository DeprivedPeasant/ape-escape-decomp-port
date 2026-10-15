/* Camera slot handler 11 for area 05 (overlay 0x18D entry 0x80140B0C), see
 * native_camera_molten.md. Builds on native_camera.h / native_camera_focus.h /
 * native_camera_math.h / native_camera_collision.h (AeCameraQueries). No global
 * state; no dependencies beyond the C99 standard headers and those modules. */
#ifndef AE_NATIVE_CAMERA_MOLTEN_H
#define AE_NATIVE_CAMERA_MOLTEN_H

#include <stdint.h>

#include "native_camera.h"
#include "native_camera_collision.h"
#include "native_camera_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Overlay 0x18D FUN 0x80140B0C. One update of the area 05 slot handler.
 *   slot          : the 0xB0 byte camera slot (eye / target updated)
 *   math          : loaded tables (VectorNormalSS)
 *   queries       : only queries->sqrt (SquareRoot0) is used and must be non-NULL
 *   player        : g_Player.pos[3] (s32); only the low halfwords are used
 *   region_focus8 : scratch focus at 0x800E68C0 (CameraSlotGetFocusPoint mode 2)
 *   scratch       : PSX scratchpad image 0x1F800000..0x1F80007F
 *
 * Scratch bytes written: 0x00..0x05 (desired eye), 0x08..0x0D (desired target)
 * and the player halfwords 0x10..0x15. Every other scratch byte and every slot
 * byte not written by the original (including the slot padding) is preserved.
 *
 * Returns 1 on success. Returns 0 with slot and scratch completely unchanged if
 * a pointer, the loaded tables or queries->sqrt is missing, or the original
 * would trap (a VectorNormalSS square sum above INT32_MAX, or a smoothing
 * division trap). */
int ae_camera_molten_update(AeCameraSlot *slot, const AeCameraMathTables *math,
                            const AeCameraQueries *queries, const int32_t player[3],
                            const uint8_t region_focus8[8], uint8_t scratch[0x80]);

#ifdef __cplusplus
}
#endif

#endif
