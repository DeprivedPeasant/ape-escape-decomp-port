#ifndef AE_NATIVE_CAMERA_SELECT_H
#define AE_NATIVE_CAMERA_SELECT_H
#include "native_camera.h"
#include "native_camera_math.h"

typedef struct AeCameraSelection {
    uint32_t snapshot_follow, activate;
    int32_t slot;
} AeCameraSelection;

/* CameraSelectMode (0x80046DE8) local control mutations and ordered service
 * requests. controls: exact 0x50 bytes at 0x800E68D0. slots are read-only.
 * eye8/target8: current original SVECTOR bytes (including padding).
 * is_flight_area supplies IsFlightArea's result. snapshot_follow, if set,
 * requests CameraSlotSnapshotEye before CameraActivateSlot(slot).
 * Returns 1, or 0 without writes for missing arguments, invalid slot indices
 * or normalization domain failure. Tables are only needed by kind5's dot test.
 * Does not execute snapshot/activation or camera blending/rendering. */
int ae_camera_select_mode(uint8_t controls[0x50], const AeCameraSlot *slots,
                           uint32_t count, const int32_t player[3],
                           const uint8_t eye8[8], const uint8_t target8[8],
                           int is_flight_area, const AeCameraMathTables *tables,
                           AeCameraSelection *events);

/* Complete CameraSlotSnapshotEye (0x80049514) for the follow slot. Copies all
 * eight eye bytes, resets follow fields, normalizes wrapped XZ delta and sets
 * yaw using original SDK tables. Failure returns0 without any slot writes. */
int ae_camera_snapshot_eye(AeCameraSlot *follow, const int32_t player[3],
                            const uint8_t eye8[8], const AeCameraMathTables *tables);

/* CameraActivateSlot (0x80046DAC) control window fields. original_slot_base is
 * an opaque 32-bit address tag, never dereferenced: +08 stores base+index*0xB0.
 * Native hosts must resolve slots by +04 index, not dereference that tag.
 * Failure (null/index/address overflow) returns0 without control writes. */
int ae_camera_activate_slot(uint8_t controls[0x50], const AeCameraSlot *slots,
                             uint32_t count, uint32_t index, uint32_t original_slot_base);
#endif
