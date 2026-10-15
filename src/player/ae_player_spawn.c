#include "ae_player_spawn.h"

#include <string.h>

static int32_t spawn_s16(uint32_t value)
{
    value &= 0xFFFFu;
    return value >= 0x8000u ? (int32_t)value - 0x10000 : (int32_t)value;
}

static int32_t spawn_missing(AeRam *r, const char *name)
{
    ae_ram_fail(r, "%s hook not provided", name);
    return 0;
}

static uint32_t spawn_stack_pointer(const AePlayerSpawnHooks *h)
{
    return h != NULL && h->stack_pointer != 0u ? h->stack_pointer : 0x801FFF00u;
}

static void spawn_copy_bytes(AeRam *r, uint32_t destination, uint32_t source, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
        ae_wr8(r, destination + i, ae_rd8(r, source + i));
}

static int32_t spawn_printf(AeRam *r, const AePlayerSpawnHooks *h,
                            uint32_t format, uint32_t a1,
                            uint32_t a2, uint32_t a3)
{
    if (h == NULL || h->printf == NULL) {
        spawn_missing(r, "printf");
        return 0;
    }
    int32_t result = h->printf(h->user, r, format, a1, a2, a3);
    return ae_ram_failed(r) ? 0 : result;
}

static uint32_t spawn_area_alloc(AeRam *r, const AePlayerSpawnHooks *h, uint32_t bytes)
{
    if (h == NULL || h->AreaBumpAlloc == NULL) {
        spawn_missing(r, "AreaBumpAlloc");
        return 0;
    }
    uint32_t result = h->AreaBumpAlloc(h->user, r, bytes);
    return ae_ram_failed(r) ? 0 : result;
}

/* AreaGetEntryPoint @80010C3C. The active area's entry table pointer is
 * installed by AreaLoad; the area argument is retained for the original ABI. */
int32_t ae_AreaGetEntryPoint(AeRam *r, int32_t area, int32_t entry, uint32_t out)
{
    (void)area;
    uint32_t table = ae_rd32(r, AE_SPAWN_G_STAGE_STATE + AE_SPAWN_STATE_ENTRY_TABLE);
    uint32_t record = table + (uint32_t)entry * AE_SPAWN_ENTRY_SIZE;

    for (uint32_t word = 0; word < AE_SPAWN_ENTRY_SIZE / AE_SPAWN_ENTRY_WORD_SIZE - 1u; ++word)
        ae_wr32(r, out + word * AE_SPAWN_ENTRY_WORD_SIZE,
                ae_rd32(r, record + word * AE_SPAWN_ENTRY_WORD_SIZE));
    ae_wr32(r, out + AE_SPAWN_ENTRY_SIZE - AE_SPAWN_ENTRY_WORD_SIZE,
            ae_rd32(r, record + AE_SPAWN_ENTRY_SIZE - AE_SPAWN_ENTRY_WORD_SIZE));
    return 0;
}

/* SetProjection @80046D80. */
void ae_SetProjection(AeRam *r, const AePlayerSpawnHooks *h, int32_t distance)
{
    ae_wr16(r, AE_SPAWN_G_CAMERA_STATE + AE_SPAWN_CAMERA_PROJECTION, (uint32_t)distance);
    if (!h || !h->SetGeomScreen) { spawn_missing(r, "SetGeomScreen"); return; }
    h->SetGeomScreen(h->user, r, (uint16_t)distance);
}

/* CameraActivateSlot @80046DAC. */
void ae_CameraActivateSlot(AeRam *r, uint32_t index)
{
    uint32_t state = AE_SPAWN_G_CAMERA_STATE;
    uint32_t slot = ae_rd32(r, AE_SPAWN_G_CAMERA_SLOTS) + index * AE_SPAWN_CAMERA_SLOT_SIZE;
    uint32_t kind = ae_rd8(r, slot + AE_SPAWN_CAMERA_SLOT_KIND);

    ae_wr16(r, state + AE_SPAWN_CAMERA_PRIORITY, index);
    ae_wr32(r, state + AE_SPAWN_CAMERA_ACTIVE_SLOT, slot);
    ae_wr16(r, state + AE_SPAWN_CAMERA_ACTIVE_KIND, kind);
}

/* CameraShakeReset @800489EC. */
void ae_CameraShakeReset(AeRam *r)
{
    ae_wr8(r, AE_SPAWN_G_CAMERA_SHAKE + AE_SPAWN_SHAKE_COUNTER, 0);
    ae_wr8(r, AE_SPAWN_G_CAMERA_SHAKE + AE_SPAWN_SHAKE_ACTIVE, 0);
}

/* StageExitFlagsClear @8004D0F0. */
void ae_StageExitFlagsClear(AeRam *r)
{
    ae_wr16(r, AE_SPAWN_G_STAGE_EXIT_FLAGS + AE_SPAWN_STAGE_EXIT_FLAGS, 0);
    ae_wr8(r, AE_SPAWN_G_STAGE_EXIT_FLAGS + AE_SPAWN_STAGE_EXIT_PENDING, 0);
}

/* PlaySfx @800877F8. The queue and voice helpers are already verified
 * RAM-image ports; only SPU wait/diagnostic operations cross AeSoundHw. */
int32_t ae_PlaySfx(AeRam *r, const AeSoundHw *hw, int32_t id)
{
    uint32_t effect = AE_SOUND_EFFECTS + (uint32_t)id * AE_SE_SIZE;
    int32_t bank_slot = ae_rds16(r, effect + AE_SE_BANK);
    uint32_t loaded_bank = AE_SOUND_BANK_IDS + (uint32_t)bank_slot * 2u;
    int32_t bank = ae_rds16(r, loaded_bank);
    int32_t program;
    int32_t note;
    int32_t voice;

    if (bank < 0)
        return -1;

    if (ae_SfxIsQueued(r, (int16_t)id) != 0)
        return 0;
    if (ae_SoundDelayedQueueAdd(r, 0, (int16_t)id, 0) == 0)
        return 0;

    /* The original re-reads the bank slot after reserving the delayed entry. */
    bank_slot = ae_rds16(r, effect + AE_SE_BANK);
    loaded_bank = AE_SOUND_BANK_IDS + (uint32_t)bank_slot * 2u;
    bank = ae_rds16(r, loaded_bank);
    program = ae_rds16(r, effect + AE_SE_PROGRAM);
    note = ae_rds16(r, effect + AE_SE_NOTE);
    voice = ae_SfxVoiceStart(r, hw, (uint32_t)bank, (uint32_t)program, (uint32_t)note);
    if (ae_ram_failed(r))
        return 0;

    ae_wr16(r, effect + AE_SE_LAST_VOICE, (uint32_t)voice);
    if (voice < 0) {
        if (hw == NULL || hw->printf_error == NULL) {
            ae_ram_fail(r, "AeSoundHw.printf_error not provided");
            return 0;
        }
        hw->printf_error(hw->user, r, 0x800A622Cu, 3u, id, program, note);
        if (ae_ram_failed(r))
            return 0;
    }
    return ae_rds16(r, effect + AE_SE_LAST_VOICE);
}

/* AimModeLeave @8004B510. */
void ae_AimModeLeave(AeRam *r, const AePlayerSpawnHooks *h)
{
    uint32_t aim_mode = AE_SPAWN_G_CAMERA_STATE + AE_SPAWN_CAMERA_AIM_MODE;
    if (ae_rd8(r, aim_mode) == 0)
        return;

    ae_wr8(r, aim_mode, 0);
    (void)ae_PlaySfx(r, h != NULL ? h->sound : NULL, 5);
}

/* EventCamSetScript @8004BAD8. */
void ae_EventCamSetScript(AeRam *r, uint32_t slot_index, int32_t script_index,
                          uint32_t group, uint32_t script_set)
{
    uint32_t camera_state = AE_SPAWN_G_CAMERA_STATE;
    uint32_t camera_slot = ae_rd32(r, AE_SPAWN_G_CAMERA_SLOTS) +
                           slot_index * AE_SPAWN_CAMERA_SLOT_SIZE;
    uint32_t index = (uint32_t)script_index;
    uint32_t table;
    uint32_t script_data;

    ae_wr8(r, camera_slot + AE_SPAWN_CAMERA_SLOT_SCRIPT_GROUP, group);
    ae_wr16(r, camera_slot + AE_SPAWN_CAMERA_SLOT_SCRIPT_STEP, 0);
    ae_wr16(r, camera_slot + AE_SPAWN_CAMERA_SLOT_SCRIPT_TIMER, 0);

    if (script_set == 0) {
        table = ae_rd32(r, camera_state + AE_SPAWN_CAMERA_SCRIPT_INDEXES);
        script_data = ae_rd32(r, camera_state + AE_SPAWN_CAMERA_SCRIPT_BYTES);
    } else {
        table = ae_rd32(r, camera_state + AE_SPAWN_CAMERA_NAME_ENTRIES);
        script_data = ae_rd32(r, camera_state + AE_SPAWN_CAMERA_NAME_TABLE);
    }

    uint32_t record = table + index * AE_SPAWN_CAM_NAME_ENTRY_SIZE;
    uint32_t script_offset = ae_rd16(r, record + AE_SPAWN_CAM_NAME_SCRIPT_OFFSET);
    ae_wr32(r, camera_slot + AE_SPAWN_CAMERA_SLOT_SCRIPT_POINTER,
            script_data + script_offset);
}

/* CameraResetAll @80047754. This consumes the CAM section installed by
 * AreaLoad, resolves region names, creates its default and area camera slots,
 * selects the follow slot, then resets shake and exit state. */
void ae_CameraResetAll(AeRam *r, const AePlayerSpawnHooks *h)
{
    uint32_t camera_state = AE_SPAWN_G_CAMERA_STATE;
    uint32_t stage_state = AE_SPAWN_G_STAGE_STATE;
    uint32_t stack_pointer = spawn_stack_pointer(h);
    uint32_t frame_sp = stack_pointer - AE_SPAWN_CAMERA_FRAME_SIZE;
    uint32_t camera_data = ae_rd32(r, stage_state + AE_SPAWN_STATE_CAMERA_DATA);
    uint32_t script_data = ae_rd32(r, stage_state + AE_SPAWN_STATE_CAMERA_SCRIPTS);
    uint32_t region_count;
    uint32_t region_records;
    uint32_t name_table;
    uint32_t name_count;
    uint32_t slots;
    uint32_t entry_eye = frame_sp + AE_SPAWN_CAMERA_TEMP_EYE;

    for (uint32_t byte = 0; byte < AE_SPAWN_CAMERA_STATE_SIZE; ++byte)
        ae_wr8(r, camera_state + byte, 0);

    ae_SetProjection(r, h, AE_SPAWN_CAMERA_INITIAL_PROJECTION);
    if (ae_ram_failed(r))
        return;
    ae_wr8(r, camera_state + AE_CAM_G_REGION_SLOT,
           AE_SPAWN_CAMERA_REGION_SENTINEL);
    ae_wr8(r, camera_state + AE_CAM_G_REGION_OLD,
           AE_SPAWN_CAMERA_REGION_SENTINEL);
    ae_AimModeLeave(r, h);
    if (ae_ram_failed(r))
        return;

    ae_wr32(r, camera_state + AE_SPAWN_CAMERA_SCRIPT_BYTES, script_data);
    ae_wr32(r, camera_state + AE_SPAWN_CAMERA_SCRIPT_INDEXES,
            script_data + AE_SPAWN_CAM_SCRIPT_HEADER_SIZE);

    /* AreaLoad uses the area base as the no-CAM sentinel. Invalid signatures
     * select the EXE's built-in empty camera data at 0x800BF120. */
    if (camera_data == AE_AREA_BASE ||
        ae_rd16(r, camera_data + AE_SPAWN_CAM_MAGIC) != AE_SPAWN_CAMERA_NAME_MAGIC ||
        ae_rd8(r, camera_data + AE_SPAWN_CAM_MAGIC + AE_SPAWN_CAM_MAGIC_SUFFIX_OFFSET) !=
        AE_SPAWN_CAMERA_VALID_SIGNATURE) {
        camera_data = AE_SPAWN_G_CAMERA_FALLBACK_DATA;
    } else {
        ae_wr8(r, camera_data + AE_SPAWN_CAM_MAGIC, AE_SPAWN_CAMERA_MARKED_SIGNATURE);
    }

    region_count = ae_rd16(r, camera_data + AE_SPAWN_CAM_REGION_COUNT);
    region_records = ae_rd32(r, stage_state + AE_SPAWN_STATE_CAMERA_DATA) +
                     AE_SPAWN_CAM_HEADER_SIZE;
    name_table = ae_rd32(r, stage_state + AE_SPAWN_STATE_CAMERA_DATA) +
                 ae_rd16(r, camera_data + AE_SPAWN_CAM_NAME_TABLE_OFFSET);
    name_count = region_count != 0u ? ae_rd16(r, name_table + AE_SPAWN_CAM_NAME_COUNT) : 0u;

    ae_wr32(r, camera_state + AE_SPAWN_CAMERA_REGIONS, region_records);
    ae_wr16(r, camera_state + AE_SPAWN_CAMERA_REGION_COUNT, region_count);
    ae_wr32(r, camera_state + AE_SPAWN_CAMERA_NAME_TABLE, name_table);
    ae_wr32(r, camera_state + AE_SPAWN_CAMERA_NAME_ENTRIES,
            name_table + AE_SPAWN_CAM_NAME_ENTRIES);
    ae_wr32(r, camera_state + AE_SPAWN_CAMERA_SCRIPT_GROUP_A,
            ae_rd32(r, stage_state + AE_SPAWN_STATE_CAMERA_DATA) +
            ae_rd16(r, camera_data + AE_SPAWN_CAM_SCRIPT_GROUP_A_OFFSET));
    ae_wr32(r, camera_state + AE_SPAWN_CAMERA_SCRIPT_GROUP_B,
            ae_rd32(r, stage_state + AE_SPAWN_STATE_CAMERA_DATA) +
            ae_rd16(r, camera_data + AE_SPAWN_CAM_SCRIPT_GROUP_B_OFFSET));
    ae_wr16(r, camera_state + AE_SPAWN_CAMERA_SCRIPT_COUNT,
            ae_rd16(r, camera_data + AE_SPAWN_CAM_SCRIPT_GROUP_COUNT));
    ae_wr16(r, camera_state + AE_SPAWN_CAMERA_UNKNOWN_6A,
            ae_rd16(r, camera_data + AE_SPAWN_CAM_UNKNOWN_HEADER_FIELD));
    if (ae_ram_failed(r))
        return;

    if (region_count != 0 && name_count != 0) {
        for (uint32_t region_index = 0; region_index < region_count; ++region_index) {
            uint32_t region = region_records + region_index * AE_SPAWN_CAM_REGION_SIZE;
            uint32_t name_index;
            for (name_index = 0; name_index < name_count; ++name_index) {
                uint32_t name_record = name_table + AE_SPAWN_CAM_NAME_ENTRIES +
                                       name_index * AE_SPAWN_CAM_NAME_ENTRY_SIZE;
                int equal = 1;
                for (uint32_t word = 0; word < AE_SPAWN_CAM_NAME_LENGTH / 4u; ++word) {
                    if (ae_rd32(r, region + AE_SPAWN_CAM_REGION_NAME + word * 4u) !=
                        ae_rd32(r, name_record + AE_SPAWN_CAM_NAME_BYTES + word * 4u)) {
                        equal = 0;
                        break;
                    }
                }
                if (equal) {
                    ae_wr16(r, region + AE_SPAWN_CAM_REGION_NAME_INDEX, name_index);
                    ae_wr16(r, region + AE_SPAWN_CAM_REGION_FLAGS,
                            ae_rd16(r, name_record + AE_SPAWN_CAM_NAME_FLAGS));
                    break;
                }
            }
            if (name_index == name_count) {
                (void)spawn_printf(r, h, AE_SPAWN_G_CAMERA_NAME_ERROR,
                                   region + AE_SPAWN_CAM_REGION_NAME, 0, 0);
                if (ae_ram_failed(r))
                    return;
            }
        }
    }

    slots = spawn_area_alloc(r, h, 0x840u);
    if (ae_ram_failed(r))
        return;
    ae_wr32(r, AE_SPAWN_G_CAMERA_SLOTS, slots);
    ae_wr16(r, camera_state + AE_SPAWN_CAMERA_SLOT_COUNT, 0);
    for (uint32_t slot_index = 0; slot_index < AE_SPAWN_CAMERA_SLOT_COUNT_VALUE; ++slot_index)
        ae_wr8(r, slots + slot_index * AE_SPAWN_CAMERA_SLOT_SIZE + AE_SPAWN_CAMERA_SLOT_KIND, 0);

    spawn_copy_bytes(r, entry_eye, stage_state + AE_SPAWN_STATE_CAMERA_EYE, 8u);
    int32_t follow_index = ae_CameraSlotCreate(r, h != NULL ? h->camera : NULL, 1, entry_eye);
    if (ae_ram_failed(r))
        return;
    ae_wr8(r, camera_state + AE_SPAWN_CAMERA_FOLLOW_INDEX, (uint32_t)follow_index);
    int32_t aim_index = ae_CameraSlotCreate(r, h != NULL ? h->camera : NULL, 2, entry_eye);
    if (ae_ram_failed(r))
        return;
    ae_wr8(r, camera_state + AE_SPAWN_CAMERA_AIM_INDEX, (uint32_t)aim_index);
    int32_t fixed_index = ae_CameraSlotCreate(r, h != NULL ? h->camera : NULL, 3, entry_eye);
    if (ae_ram_failed(r))
        return;
    ae_wr8(r, camera_state + AE_SPAWN_CAMERA_FIXED_INDEX, (uint32_t)fixed_index);

    for (uint32_t region_index = 0; region_index < region_count; ++region_index) {
        uint32_t region = region_records + region_index * AE_SPAWN_CAM_REGION_SIZE;
        uint32_t flags = ae_rd16(r, region + AE_SPAWN_CAM_REGION_FLAGS);
        if ((flags & AE_SPAWN_CAMERA_REGION_KIND_MASK) != AE_SPAWN_CAMERA_REGION_KIND_AREA)
            continue;

        flags &= AE_SPAWN_CAMERA_REGION_FLAGS_MASK;
        ae_wr16(r, region + AE_SPAWN_CAM_REGION_FLAGS, flags);
        int32_t index = ae_CameraSlotCreate(r, h != NULL ? h->camera : NULL,
                                             5, region + AE_SPAWN_CAM_REGION_EYE);
        if (ae_ram_failed(r))
            return;
        flags |= (uint32_t)index & AE_SPAWN_CAMERA_REGION_SLOT_MASK;
        ae_wr16(r, region + AE_SPAWN_CAM_REGION_FLAGS, flags);
        ae_EventCamSetScript(r, (uint32_t)index,
                             ae_rds16(r, region + AE_SPAWN_CAM_REGION_NAME_INDEX),
                             (flags >> 13) & 3u, 1u);
    }

    uint32_t active_index = ae_rd8(r, camera_state + AE_SPAWN_CAMERA_FOLLOW_INDEX);
    ae_wr8(r, camera_state + AE_SPAWN_CAMERA_ACTIVE_INDEX, active_index);
    ae_CameraActivateSlot(r, active_index);

    ae_wr32(r, frame_sp + AE_SPAWN_CAMERA_TEMP_ROLL, 0);
    if (h == NULL || h->CameraLookAt == NULL) {
        spawn_missing(r, "CameraLookAt");
        return;
    }
    h->CameraLookAt(h->user, r,
                    stage_state + AE_SPAWN_STATE_CAMERA_EYE,
                    stage_state + AE_SPAWN_STATE_CAMERA_TARGET,
                    AE_G_CAMERA_MATRIX,
                    AE_G_CAMERA_ROT_MATRIX,
                    (int32_t)ae_rd32(r, frame_sp + AE_SPAWN_CAMERA_TEMP_ROLL));
    if (ae_ram_failed(r))
        return;

    ae_wr32(r, AE_SPAWN_G_CAMERA_RADAR_TARGET, 0);
    ae_CameraShakeReset(r);
    ae_StageExitFlagsClear(r);
}

/* PlayerSpawnAtEntry @80032BBC. */
int32_t ae_PlayerSpawnAtEntry(AeRam *r, const AePlayerSpawnHooks *h)
{
    uint32_t stage_state = AE_SPAWN_G_STAGE_STATE;
    uint32_t player = AE_SPAWN_G_PLAYER;
    uint32_t stack_pointer = spawn_stack_pointer(h);
    uint32_t frame_sp = stack_pointer - AE_SPAWN_PLAYER_FRAME_SIZE;
    uint32_t entry_record = frame_sp + AE_SPAWN_STACK_ENTRY_OFFSET;
    uint32_t facing = frame_sp + AE_SPAWN_STACK_FACING_OFFSET;
    int32_t area = ae_rds16(r, stage_state + AE_SPAWN_STATE_CURRENT_AREA);
    int32_t entry = ae_rds16(r, stage_state + AE_SPAWN_STATE_ENTRY_INDEX);

    (void)ae_AreaGetEntryPoint(r, area, entry, entry_record);
    if (ae_ram_failed(r))
        return 0;

    for (uint32_t axis = 0; axis < 3u; ++axis) {
        uint32_t offset = axis * AE_SPAWN_ENTRY_WORD_SIZE;
        uint32_t component = axis * AE_SPAWN_PACKED_VECTOR_COMPONENT_SIZE;
        ae_wr16(r, stage_state + AE_SPAWN_STATE_CAMERA_EYE + component,
                ae_rd16(r, entry_record + AE_SPAWN_ENTRY_CAMERA + offset));
        ae_wr16(r, stage_state + AE_SPAWN_STATE_CAMERA_TARGET + component,
                ae_rd16(r, entry_record + AE_SPAWN_ENTRY_POSITION + offset));
    }

    int32_t position_x = ae_rds32(r, entry_record + AE_SPAWN_ENTRY_POSITION);
    int32_t position_y = ae_rds32(r, entry_record + AE_SPAWN_ENTRY_POSITION_Y);
    int32_t position_z = ae_rds32(r, entry_record + AE_SPAWN_ENTRY_POSITION_Z);
    ae_wr32(r, player + AE_SPAWN_PLAYER_POSITION_X, (uint32_t)position_x);
    ae_wr16(r, player + AE_SPAWN_PLAYER_POSITION_X_LOW, (uint32_t)position_x);
    ae_wr32(r, player + AE_SPAWN_PLAYER_POSITION_Z, (uint32_t)position_z);
    ae_wr16(r, player + AE_SPAWN_PLAYER_POSITION_Z_LOW, (uint32_t)position_z);

    if (h == NULL || h->world == NULL) {
        spawn_missing(r, "GetGroundHeight dependencies");
        return 0;
    }
    int32_t ground_y = ae_GetGroundHeight(r, h->world, position_x, position_y, position_z);
    if (ae_ram_failed(r))
        return 0;
    ae_wr16(r, player + AE_SPAWN_PLAYER_POSITION_Y_LOW, (uint32_t)ground_y);
    ae_wr32(r, player + AE_SPAWN_PLAYER_GROUND_HEIGHT, (uint32_t)spawn_s16((uint32_t)ground_y));
    ae_wr32(r, player + AE_SPAWN_PLAYER_POSITION_Y, (uint32_t)spawn_s16((uint32_t)ground_y));

    ae_wr16(r, player + AE_SPAWN_PLAYER_BODY_RADIUS, 0x40u);
    ae_wr16(r, player + AE_SPAWN_PLAYER_BODY_TYPE, 1u);
    ae_wr16(r, player + AE_SPAWN_PLAYER_ANIM_FRAME, 0);
    ae_wr16(r, player + AE_SPAWN_PLAYER_STATE, 0);
    ae_wr16(r, player + AE_SPAWN_PLAYER_ANIM_LERP, 0);
    ae_wr16(r, player + AE_SPAWN_PLAYER_UP_X, 0);
    ae_wr16(r, player + AE_SPAWN_PLAYER_UP_Y, 0x1000u);
    ae_wr16(r, player + AE_SPAWN_PLAYER_UP_Z, 0);
    ae_wr32(r, player + AE_SPAWN_PLAYER_STATE_FLAGS, 0);
    for (uint32_t offset = AE_SPAWN_PLAYER_VELOCITY_X;
         offset <= AE_SPAWN_PLAYER_VELOCITY_Z; offset += AE_SPAWN_ENTRY_WORD_SIZE)
        ae_wr32(r, player + offset, 0);
    ae_wr32(r, player + AE_SPAWN_PLAYER_UNKNOWN_114, 0);
    ae_wr32(r, player + AE_SPAWN_PLAYER_UNKNOWN_11C, 0);
    ae_wr32(r, player + AE_SPAWN_PLAYER_UNKNOWN_108, 0);
    ae_wr32(r, player + AE_SPAWN_PLAYER_UNKNOWN_110, 0);
    ae_wr16(r, player + AE_SPAWN_PLAYER_BODY_FLAGS, 0x200u);
    ae_wr16(r, player + AE_SPAWN_PLAYER_UNKNOWN_58, 0xFFFFu);
    ae_wr16(r, player + AE_SPAWN_PLAYER_UNKNOWN_102, 0);
    ae_wr32(r, player + AE_SPAWN_PLAYER_DATA_POINTER,
            ae_rd32(r, stage_state + AE_SPAWN_STATE_PLAYER_DATA));
    ae_wr32(r, player + AE_SPAWN_PLAYER_BONE_BUFFER,
            AE_SPAWN_G_PLAYER_BONE_BUFFER);

    for (uint32_t axis = 0; axis < 3u; ++axis) {
        uint32_t source = entry_record + AE_SPAWN_ENTRY_FACING +
                          axis * AE_SPAWN_ENTRY_WORD_SIZE;
        uint32_t destination = facing + axis * AE_SPAWN_PACKED_VECTOR_COMPONENT_SIZE;
        ae_wr16(r, destination, ae_rd16(r, source));
    }
    (void)ae_sdk_VectorNormalSS(r, facing, facing);
    if (ae_ram_failed(r))
        return 0;
    spawn_copy_bytes(r, player + AE_SPAWN_PLAYER_FACING_X, facing, 6u);

    if (h == NULL || h->bodies == NULL) {
        spawn_missing(r, "CollisionBodyRegister dependencies");
        return 0;
    }
    (void)ae_CollisionBodyRegister(r, h->bodies, 1,
                                   player + AE_SPAWN_PLAYER_POSITION_X, player);
    if (ae_ram_failed(r))
        return 0;

    AePlayerSpawnHooks nested = h != NULL ? *h : (AePlayerSpawnHooks){0};
    nested.stack_pointer = frame_sp;
    ae_CameraResetAll(r, &nested);
    return 0;
}
