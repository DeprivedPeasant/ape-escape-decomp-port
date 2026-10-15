/* Camera slot bank services: CameraSlotCreate (0x800471E4), CameraSlotRemove
 * (0x80047290) and EventCamSetScript (0x8004BAD8). See native_camera_bank.md.
 * The bank is the exact 12 x 0xB0 byte original slot array, the controls are the
 * 0x50 byte block at 0x800E68D0 (little-endian, byte accessed). No game data is
 * embedded. Depends on native_camera.h and native_camera_math.h only. */
#ifndef AE_NATIVE_CAMERA_BANK_H
#define AE_NATIVE_CAMERA_BANK_H

#include <stddef.h>
#include <stdint.h>
#include "native_camera.h"
#include "native_camera_math.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AE_CAMERA_BANK_SLOTS 12u
#define AE_CAMERA_BANK_CONTROLS_BYTES 0x50u
#define AE_CAMERA_REGION_STRIDE 0x30u
#define AE_CAMERA_SCRIPT_TABLE_HEADER 8u
#define AE_CAMERA_SCRIPT_ENTRY_STRIDE 0x28u

/* Control offsets used by these services. */
enum {
    AE_CAMCTL_COUNT      = 0x02, /* u16 live slot count (0x800E68D2) */
    AE_CAMCTL_CURRENT    = 0x04, /* u16 (0x800E68D4) */
    AE_CAMCTL_CLEAR_C    = 0x0C, /* u8  */
    AE_CAMCTL_CLEAR_D    = 0x0D, /* u8  */
    AE_CAMCTL_LAST_E     = 0x0E, /* u8  (0x800E68DE) */
    AE_CAMCTL_ACTIVE_1B  = 0x1B, /* u8  (0x800E68EB) */
    AE_CAMCTL_FLAG_41    = 0x41, /* u8  (0x800E6911) */
    AE_CAMCTL_REGION_PTR = 0x4C  /* u32 region table pointer, never dereferenced */
};

/* Takes the first slot (0..11) whose kind byte is 0. Returns 1 on success and
 * stores the index in *index; when all 12 are used the original returns 0, which
 * is reported as success with *index = 0 and nothing changed (slots, controls,
 * player, facing, tables, eyes are then not even looked at except slots / index).
 * Returns 0 (all outputs untouched, *index preserved) on invalid input: null
 * required pointer, kind 1 / 3 without loaded tables, or an atan2 failure.
 * Only the kind byte, the initial eye bytes, the linked index, the region byte
 * 0x93 = 0xFF and what ae_camera_init_follow writes change; controls u16 at 0x02
 * is incremented with wrap. The initial eye is overwritten by current_eye8 inside
 * init_follow (as in the original), but is validated and written first. */
int ae_camera_slot_create(AeCameraSlot slots[12], uint8_t controls[0x50], int32_t kind,
                          const uint8_t initial_eye8[8], const uint8_t current_eye8[8],
                          const uint8_t current_target8[8], const int32_t player[3],
                          const int16_t facing[3], const AeCameraMathTables *tables,
                          uint32_t *index);

/* Frees slot index. Returns 1 on success, 0 (nothing changed) if index >= 12, a
 * pointer is null, or the slot has a region byte != 0xFF and the 0x30 byte region
 * record is not within regions / region_count. Only the kind byte of the slot is
 * cleared (PC and other bytes stay). */
int ae_camera_slot_remove(AeCameraSlot slots[12], uint8_t controls[0x50], uint8_t *regions,
                          size_t regions_bytes, uint16_t region_count, uint32_t index);

/* EventCamSetScript (local group 1 adapter). table starts at the entry table
 * (u16 count at 0, entries at 8, stride 0x28, u16 relative script offset at entry
 * + 2) and extends table_bytes. original_table_base is the original address that
 * relative offsets are added to (2 byte aligned). The relative offset must be
 * 2 byte aligned and leave at least 2 bytes inside the table window. Writes slot
 * 0x8E = region_flag, 0x88 = 0x8A = 0, 0x84 = base + offset. Returns 1 on
 * success, 0 (slot unchanged) on any validation failure or address overflow. */
int ae_camera_set_script(AeCameraSlot *slot, const uint8_t *table, size_t table_bytes,
                         uint32_t original_table_base, uint32_t script_index,
                         uint8_t region_flag);

#ifdef __cplusplus
}
#endif

#endif
