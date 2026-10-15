/* Camera slot bank services. See native_camera_bank.h / native_camera_bank.md. */
#include "native_camera_bank.h"
#include <string.h>

static uint16_t ctl_u16(const uint8_t *c, size_t off)
{
    return (uint16_t)(c[off] | ((uint16_t)c[off + 1] << 8));
}

static void ctl_set_u16(uint8_t *c, size_t off, uint32_t v)
{
    c[off] = (uint8_t)(v & 0xFFu);
    c[off + 1] = (uint8_t)((v >> 8) & 0xFFu);
}

static int32_t s32_from_u32(uint32_t v)
{
    if (v & 0x80000000u)
        return (int32_t)(v & 0x7FFFFFFFu) - 0x7FFFFFFF - 1;
    return (int32_t)v;
}

int ae_camera_slot_create(AeCameraSlot slots[12], uint8_t controls[0x50], int32_t kind,
                          const uint8_t initial_eye8[8], const uint8_t current_eye8[8],
                          const uint8_t current_target8[8], const int32_t player[3],
                          const int16_t facing[3], const AeCameraMathTables *tables,
                          uint32_t *index)
{
    uint32_t i;
    int32_t yaw = 0;
    AeCameraSlot temp;

    if (!slots || !index)
        return 0;
    for (i = 0; i < AE_CAMERA_BANK_SLOTS; i++)
        if (slots[i].raw[0] == 0)
            break;
    if (i == AE_CAMERA_BANK_SLOTS) {
        *index = 0;
        return 1;
    }
    if (!controls || !initial_eye8 || !current_eye8 || !current_target8)
        return 0;
    if (kind == 1 || kind == 3) {
        if (!player || !facing || !tables ||
            !ae_camera_atan2_safe(tables, -(int32_t)facing[0], -(int32_t)facing[2], &yaw))
            return 0;
    }

    temp = slots[i];
    temp.raw[0] = (uint8_t)((uint32_t)kind & 0xFFu);
    memcpy(temp.raw + AE_CAM_EYE, initial_eye8, 8);
    temp.raw[2] = (uint8_t)i;
    temp.raw[0x93] = 0xFF;
    ae_camera_init_follow(&temp, kind, current_eye8, current_target8, player, facing, yaw);

    ctl_set_u16(controls, AE_CAMCTL_COUNT, (uint32_t)ctl_u16(controls, AE_CAMCTL_COUNT) + 1u);
    slots[i] = temp;
    *index = i;
    return 1;
}

int ae_camera_slot_remove(AeCameraSlot slots[12], uint8_t controls[0x50], uint8_t *regions,
                          size_t regions_bytes, uint16_t region_count, uint32_t index)
{
    uint8_t region;
    uint8_t kind;
    uint8_t *rec = NULL;

    if (!slots || !controls || index >= AE_CAMERA_BANK_SLOTS)
        return 0;
    region = slots[index].raw[0x93];
    if (region != 0xFF) {
        if (!regions || region >= region_count ||
            (size_t)region + 1u > regions_bytes / AE_CAMERA_REGION_STRIDE)
            return 0;
        rec = regions + (size_t)region * AE_CAMERA_REGION_STRIDE;
    }

    if (controls[AE_CAMCTL_FLAG_41] != 0)
        controls[AE_CAMCTL_FLAG_41] = 0;
    if (rec)
        rec[2] = (uint8_t)(rec[2] & 0xF0u); /* u16 at +2 &= 0xFFF0, little-endian */
    ctl_set_u16(controls, AE_CAMCTL_COUNT, (uint32_t)ctl_u16(controls, AE_CAMCTL_COUNT) - 1u);
    kind = slots[index].raw[0];
    if ((uint8_t)(kind - 4u) < 2u && controls[AE_CAMCTL_ACTIVE_1B] == (uint8_t)index) {
        controls[AE_CAMCTL_ACTIVE_1B] = 0xFF;
        controls[AE_CAMCTL_CLEAR_C] = 0;
        controls[AE_CAMCTL_CLEAR_D] = 0xFF;
    }
    if (ctl_u16(controls, AE_CAMCTL_CURRENT) == index)
        controls[AE_CAMCTL_LAST_E] = (uint8_t)index;
    slots[index].raw[0] = 0;
    return 1;
}

int ae_camera_set_script(AeCameraSlot *slot, const uint8_t *table, size_t table_bytes,
                         uint32_t original_table_base, uint32_t script_index,
                         uint8_t region_flag)
{
    uint32_t count, rel;
    uint64_t entry, address;

    if (!slot || !table || (original_table_base & 1u) != 0 ||
        table_bytes < AE_CAMERA_SCRIPT_TABLE_HEADER)
        return 0;
    count = ctl_u16(table, 0);
    if (script_index >= 0x80000000u || script_index >= count)
        return 0;
    entry = AE_CAMERA_SCRIPT_TABLE_HEADER +
            (uint64_t)script_index * AE_CAMERA_SCRIPT_ENTRY_STRIDE;
    if (entry + AE_CAMERA_SCRIPT_ENTRY_STRIDE > (uint64_t)table_bytes)
        return 0;
    rel = ctl_u16(table, (size_t)entry + 2u);
    if ((rel & 1u) != 0 || (uint64_t)rel + 2u > (uint64_t)table_bytes)
        return 0;
    address = (uint64_t)original_table_base + rel;
    if (address > 0xFFFFFFFFu)
        return 0;

    slot->raw[0x8E] = region_flag;
    ae_camera_set16(slot, AE_CAM_SCRIPT_TIMER, 0);
    ae_camera_set16(slot, AE_CAM_SCRIPT_STEP, 0);
    ae_camera_set32(slot, AE_CAM_SCRIPT, s32_from_u32((uint32_t)address));
    return 1;
}
