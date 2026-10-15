/* CameraRegionUpdate (0x8004BF20) complete per-frame region lifecycle; see
 * native_camera_lifecycle.md. Composes the validated native_camera_regions
 * (CameraRegionTest), native_camera_bank (CameraSlotCreate / Remove,
 * EventCamSetScript) and native_camera_select (CameraSelectMode, SnapshotEye,
 * ActivateSlot) services. No game data embedded; C11 standard headers only. */
#ifndef AE_NATIVE_CAMERA_LIFECYCLE_H
#define AE_NATIVE_CAMERA_LIFECYCLE_H

#include <stddef.h>
#include <stdint.h>

#include "native_camera.h"
#include "native_camera_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exact original layout, no padding:
 *   controls[0x50] = 0x800E68D0..0x800E691F
 *   focus[16]      = 0x800E68C0..0x800E68CF (region focus scratch)
 *   slots[12]      = the 12 x 0xB0 byte slot bank */
typedef struct AeCameraRegionFrame {
    uint8_t controls[0x50];
    uint8_t focus[16];
    AeCameraSlot slots[12];
} AeCameraRegionFrame;

/* One CameraRegionUpdate call.
 * regions: caller-owned mutable 0x30-stride records (original region table);
 * regions_bytes >= region_count * 0x30 and region_count <= 255 (0xFF is the
 * "no region" sentinel). regions may be NULL only when region_count == 0.
 * player: g_Player.pos (s32[3]); the query point is the low 16 bits of x, y - 0x80, z.
 * facing / tables: only forwarded to the bank / selector services (the kind 4
 * create path never reads player or facing; tables are needed only by the
 * selector's kind 5 test). eye8 / target8: current camera eye / target SVECTOR bytes.
 * script_table / _bytes / _base: EventCamSetScript group 1 window (see bank header).
 * original_slot_base: opaque tag stored by ActivateSlot, never dereferenced.
 * Control 0x4C (region pointer) is never read or written; the caller sets the
 * slot count at control 0x02 itself.
 * Returns 1 on success, 0 on invalid input, service failure (selector, math,
 * script domain, bad previous region / slot index) or allocation failure. On 0,
 * all of frame and regions are unchanged. */
int ae_camera_region_update(AeCameraRegionFrame *frame, uint8_t *regions,
                            size_t regions_bytes, uint16_t region_count,
                            const int32_t player[3], const int16_t facing[3],
                            const uint8_t eye8[8], const uint8_t target8[8],
                            const AeCameraMathTables *tables,
                            const uint8_t *script_table, size_t script_table_bytes,
                            uint32_t script_table_base, int is_flight_area,
                            uint32_t original_slot_base);

#ifdef __cplusplus
}
#endif

#endif
