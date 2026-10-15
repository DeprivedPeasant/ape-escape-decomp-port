/* Portable original CameraApplySlot (0x80047FF0); see native_camera_blend.md.
 * No dependencies beyond native_camera.h and the C99 standard headers. */
#ifndef AE_NATIVE_CAMERA_BLEND_H
#define AE_NATIVE_CAMERA_BLEND_H

#include "native_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CameraApplySlot. controls: exact 0x50 bytes of the original control window
 * (0x800E68D0..0x800E691F); only the u16 blend counter at +0x30 is written,
 * and only when it was nonzero on entry. active: the slot the original reads
 * through control +0x08 (read-only; the control field itself is not used).
 * eye8 / target8: the exact current original SVECTOR global bytes (8 each,
 * including the 2 padding bytes at +6 / +7).
 *
 * Counter 0: all 8 bytes of slot +0x0A / +0x12 are copied to eye8 / target8.
 * Otherwise the counter is stepped by the blend style (control +0x2E), then
 * eye8 / target8 x, y, z (s16 at +0, +2, +4) are rewritten from the blend of
 * saved eye (+0x1C) / target (+0x24) and the slot values; padding is kept.
 *
 * Returns 1 on success; 0 if any argument is NULL, with nothing written. */
int ae_camera_apply_slot(uint8_t controls[0x50], const AeCameraSlot *active,
                         uint8_t eye8[8], uint8_t target8[8]);

#ifdef __cplusplus
}
#endif

#endif
