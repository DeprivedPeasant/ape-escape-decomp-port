#include "ae_stage.h"

#include "../monkeys/ae_monkey.h"
#include "../player/native_gadget_slots.h"
#include "../core/ae_sdk_matrix.h"

#define AE_STAGE_G_AREA_LOAD_TABLE 0x800A81B0u
#define AE_STAGE_G_STATE 0x800F4350u
#define AE_STAGE_STATE_CURRENT_AREA 0x126u
#define AE_STAGE_G_ARC_LOAD_STATE 0x800E3890u
#define AE_STAGE_ARC_RECORDS 0x1Cu
#define AE_STAGE_ARC_RECORD_INDEX 0x20u
#define AE_STAGE_ARC_LIST_INDEX 0x24u
#define AE_STAGE_ARC_SEEK_RESULT 0x18u
#define AE_STAGE_ARC_STATUS 0x44u
#define AE_STAGE_ARC_COMMAND 0x70u
#define AE_STAGE_ARC_LOAD_STATUS 0x04u
#define AE_STAGE_ARC_LOAD_READ_STATUS 0x08u
#define AE_STAGE_ARC_LOAD_LAST_STATUS 0x10u
#define AE_STAGE_ARC_LOAD_PARAMETER 0x0Cu
#define AE_STAGE_ARC_LOAD_RESULT 0x14u
#define AE_STAGE_ARC_LOAD_RETRY_INDEX 0x18u
#define AE_STAGE_ARC_LOAD_STEP 0x68u
#define AE_STAGE_STATE_LEVEL 0x124u
#define AE_STAGE_STATE_MODE 0x120u
#define AE_STAGE_STATE_AREA_KIND 0x122u
#define AE_STAGE_STATE_AREA_LOAD_SPECIAL_BYTE 0x138u
#define AE_STAGE_STATE_AREA_COUNT 0x128u
#define AE_STAGE_STATE_PROGRESS_TYPE 0x12Cu
#define AE_STAGE_STATE_PROGRESS_SELECTOR 0x12Eu
#define AE_STAGE_STATE_TRANSITION_MODE 0x130u
#define AE_STAGE_STATE_TRANSITION_STEP 0x132u
#define AE_STAGE_STATE_TRANSITION_ARGUMENT 0x134u
#define AE_STAGE_STATE_TRANSITION_STATUS 0x136u
#define AE_STAGE_STATE_AREA_TRANSITION_COUNT 0x13Cu
#define AE_STAGE_STATE_AREA_INIT_FLAG 0x140u
#define AE_STAGE_STATE_TIME_STATION_COUNT 0x158u
#define AE_STAGE_STATE_TIME_STATION_COUNTER 0x158u
#define AE_STAGE_STATE_TRANSITION_PROGRESS 0x100u
#define AE_STAGE_STATE_PROGRESS_RESET 0x160u
#define AE_STAGE_STATE_MONKEY_COUNT 0x186u
#define AE_STAGE_STATE_PROGRESS_KIND 0x188u
#define AE_STAGE_STATE_AREA_SCORE 0x18Au
#define AE_STAGE_STATE_AREA_DATA 0x18Cu
#define AE_STAGE_STATE_AREA_INDEX 0x15Cu
#define AE_STAGE_STATE_AREA_PROGRESS 0x166u
#define AE_STAGE_AREA_PROGRESS_BYTES 0x16Cu
#define AE_STAGE_PROGRESS_TABLE 0x800DFE00u
#define AE_STAGE_GIMMICK_STATE_TABLE 0x800DF820u
#define AE_STAGE_SPECIAL_PROGRESS 0x800DFCB0u
#define AE_STAGE_AREA_PROGRESS_TABLE 0x800DFC98u
#define AE_STAGE_TIME_ATTACK_PROGRESS 0x800DFF20u
#define AE_STAGE_AREA_ORDER 0x800DFFE0u
#define AE_STAGE_LEVEL_PROGRESS_MODE 0x800DFC70u
#define AE_STAGE_LEVEL_PROGRESS_COUNT 0x800DFC88u
#define AE_STAGE_AREA_HEADER_POINTER 0x801544CCu
#define AE_STAGE_AREA_BLOB 0x80154000u
#define AE_STAGE_LEVEL_TABLE 0x80102400u
#define AE_STAGE_HUB_MONKEY_COUNTS 0x800B38ACu
#define AE_STAGE_G_INIT_PROGRESS_FLAG 0x800B38B8u
#define AE_STAGE_G_PROGRESS_MIRROR 0x800E0010u
#define AE_STAGE_LEVEL_ROW_SIZE 0x10u
#define AE_STAGE_AREA_DATA_RECORDS_OFFSET 0x40u
#define AE_STAGE_PROGRESS_RECORD_SIZE 0x18u
#define AE_STAGE_PROGRESS_SLOT_COUNT 8u
#define AE_STAGE_G_LEVEL_GRID 0x800EBFA0u
#define AE_STAGE_G_AREA_PROPS 0x800B4D64u
#define AE_STAGE_G_ROTATION_MATRICES 0x800E7900u
#define AE_STAGE_G_AREA_MATRIX_BANK 0x800E9C60u
#define AE_STAGE_G_AREA_MATRIX_POOL 0x800F4730u
#define AE_STAGE_G_AREA_MATRIX_ROTATION 0x800E78C0u
#define AE_STAGE_G_AREA_MATRIX_TEMP 0x800EC330u
#define AE_STAGE_G_AREA_ALLOC_CURSOR 0x800E38E4u
#define AE_STAGE_AREA_MATRIX_COUNT 32u
#define AE_STAGE_STATE_AREA_HEADER 0x194u
#define AE_STAGE_STATE_AREA_GRID 0x198u
#define AE_STAGE_STATE_MESH_TABLE 0x224u
#define AE_STAGE_STATE_PROP_PLACEMENTS 0x228u
#define AE_STAGE_STATE_PROP_COUNT 0x280u
#define AE_STAGE_STATE_AREA_LOAD_PROGRESS_FLAG 0x100u
#define AE_STAGE_STATE_AREA_COLLISION_TABLE 0x244u
#define AE_STAGE_STATE_AREA_PROPS_TABLE 0x248u
#define AE_STAGE_STATE_AREA_ITEMS_TABLE 0x24Cu
#define AE_STAGE_STATE_AREA_MESHES_TABLE 0x250u
#define AE_STAGE_STATE_AREA_ITEM_DATA 0x254u
#define AE_STAGE_STATE_AREA_OBJECT_COUNT 0x288u
#define AE_STAGE_STATE_AREA_MONKEY_TABLE 0x240u
#define AE_STAGE_LEVEL_GRID_WIDTH 0x00u
#define AE_STAGE_LEVEL_GRID_DEPTH 0x02u
#define AE_STAGE_LEVEL_GRID_COLOR_COUNT 0x04u
#define AE_STAGE_LEVEL_GRID_CELLS 0x08u
#define AE_STAGE_LEVEL_GRID_COLORS 0x0Cu
#define AE_STAGE_LEVEL_GRID_CELL_TILES 0x10u
#define AE_STAGE_LEVEL_GRID_HEIGHTS 0x14u
#define AE_STAGE_LEVEL_GRID_FOG_COLOR 0x1Cu
#define AE_STAGE_LEVEL_GRID_FOG_MULTIPLIER 0x24u
#define AE_STAGE_LEVEL_GRID_FOG_START 0x28u
#define AE_STAGE_LEVEL_GRID_DRAW_DISTANCE 0x2Cu
#define AE_STAGE_GRID_CELL_DATA_OFFSET 0x08u
#define AE_STAGE_MESH_TABLE_ENTRY_SIZE 0x04u
#define AE_STAGE_MESH_INDEX_FIELD 0x04u
#define AE_STAGE_ROTATION_MATRIX_SIZE 0x20u
#define AE_STAGE_AREA_PROP_SIZE 0x20u
#define AE_STAGE_PROP_PLACEMENT_SIZE 0x0Cu
#define AE_STAGE_PROP_MESH 0x00u
#define AE_STAGE_PROP_POSITION 0x10u
#define AE_STAGE_PROP_ZONE_MASK 0x16u
#define AE_STAGE_PROP_ROTATION 0x1Cu
#define AE_STAGE_PROP_ROTATION_FIELD 0x02u
#define AE_STAGE_PROP_X 0x04u
#define AE_STAGE_PROP_Y 0x06u
#define AE_STAGE_PROP_Z 0x08u
#define AE_STAGE_MODEL_OVER_MESSAGE 0x800A48E0u
#define AE_STAGE_ROTATION_LIMIT 33u
#define AE_STAGE_G_AREA_SPECIAL_FLAGS 0x800F5120u
#define AE_STAGE_AREA_BASE 0x80154000u
#define AE_STAGE_AREA_HEADER_RECORD 0x801544D0u
#define AE_STAGE_AREA_OBJECT_TABLE_RECORD 0x801544D4u
#define AE_STAGE_AREA_MESHES_RECORD 0x801544D8u
#define AE_STAGE_AREA_PROPS_RECORD 0x801544DCu
#define AE_STAGE_AREA_COLLISION_RECORD 0x801544E0u
#define AE_STAGE_AREA_MONKEY_RECORD 0x801544E4u
#define AE_STAGE_AREA_ITEMS_RECORD 0x801544E8u
#define AE_STAGE_AREA_SOUND_SOURCE_RECORD 0x801544C4u
#define AE_STAGE_AREA_SOUND_SIZE_RECORD 0x80154504u
#define AE_STAGE_AREA_SPECIAL_INDEX_TABLE 0x800AE914u
#define AE_STAGE_AREA_SPECIAL_INDEX_MAGIC 0x38E38E39u
#define AE_STAGE_AREA_SPECIAL_INDEX_COUNT 9u
#define AE_STAGE_AREA_FEATURES 0x34u
#define AE_STAGE_AREA_FEATURE_COUNT 8u
#define AE_STAGE_AREA_FEATURE_SIZE 4u
#define AE_STAGE_AREA_FEATURE_ID_MASK 0xFFu
#define AE_STAGE_AREA_FOG_COLOR 0x20u
#define AE_STAGE_AREA_FOG_ENABLED 0x28u
#define AE_STAGE_AREA_FOG_NEAR 0x2Cu
#define AE_STAGE_AREA_FOG_FAR 0x2Eu
#define AE_STAGE_AREA_TEXCOPY_ENABLED 0x2Au
#define AE_STAGE_AREA_TEXCOPY_RGBA 0x24u
#define AE_STAGE_AREA_COLLISION_HEADER 0x10u
#define AE_STAGE_AREA_MESH_TABLE_OFFSET 0x08u
#define AE_STAGE_AREA_PROPS_OFFSET 0x0Cu
#define AE_STAGE_AREA_GRID_OFFSET 0x04u
#define AE_STAGE_AREA_OBJECT_COUNT_OFFSET 0x00u
#define AE_STAGE_AREA_OBJECTS_OFFSET 0x04u
#define AE_STAGE_AREA_OBJECT_RECORD_SIZE 0x08u
#define AE_STAGE_AREA_COLLISION_ENTRY_SIZE 0x04u
#define AE_STAGE_AREA_COLLISION_GRID_ENTRY 0x00u
#define AE_STAGE_AREA_COLLISION_SHAPE_ENTRY 0x04u
#define AE_STAGE_AREA_COLLISION_LIST_ENTRY 0x08u
#define AE_STAGE_AREA_COLLISION_GRID_AUX_ENTRY 0x0Cu
#define AE_STAGE_AREA_COLLISION_SHAPE_AUX_ENTRY 0x10u
#define AE_STAGE_AREA_COLLISION_ENTRY_DATA_OFFSET 0x02u
#define AE_STAGE_AREA_COLLISION_GRID_DATA_OFFSET 0x04u
#define AE_STAGE_AREA_COLLISION_KIND_OFFSET 0x00u
#define AE_STAGE_AREA_COLLISION_HEIGHT_OFFSET 0x02u
#define AE_STAGE_AREA_PROP_PLACEMENTS_OFFSET 0x04u
#define AE_STAGE_G_TEXCOPY_RECT_TABLE 0x800A47F0u
#define AE_STAGE_DEFAULT_STACK_TOP 0x801FFF00u
#define AE_STAGE_STACK_LOCAL_OFFSET 0x10u
#define AE_STAGE_STACK_COMMAND_OFFSET 0x28u
#define AE_STAGE_AREA_LOAD_START_SEEK_FRAME 48u
#define AE_STAGE_AREA_LOAD_QUEUE_READ_FRAME 72u
#define AE_STAGE_AREA_LOAD_START_FRAME 64u
#define AE_STAGE_AREA_LOAD_FRAME 64u
#define AE_STAGE_STAGE_REQUEST_FRAME 48u
#define AE_STAGE_STAGE_REQUEST_UPDATE_FRAME 40u
#define AE_STAGE_STAGE_REQUEST_RETRY_FRAME 24u
#define AE_STAGE_AREA_FEATURE_SPECIAL_ACTOR_A 0x40u
#define AE_STAGE_AREA_FEATURE_SPECIAL_ACTOR_B 0x41u
#define AE_STAGE_AREA_FEATURE_INDICATOR_A 0x10u
#define AE_STAGE_AREA_FEATURE_INDICATOR_B 0x11u
#define AE_STAGE_AREA_FEATURE_VRAM_ANIM 0x20u
#define AE_STAGE_AREA_FEATURE_ZONE_BOXES 0x08u
#define AE_STAGE_AREA_FEATURE_30 0x30u
#define AE_STAGE_AREA_FEATURE_31 0x31u
#define AE_STAGE_AREA_FEATURE_32 0x32u
#define AE_STAGE_AREA_BGM_PRESERVE_MODE 2u
#define AE_STAGE_G_AREA_LOAD_MODE 0x800A8A54u
#define AE_STAGE_G_AREA_LOAD_SPECIAL 0x800A8A58u
#define AE_STAGE_G_AREA_LOAD_RANDOM_SEED 0x800AE8F0u
#define AE_STAGE_G_AREA_LOAD_RANDOM_SEED_HI 0x800AE8F2u
#define AE_STAGE_G_AREA_LOAD_SPECIAL_INDEX 0x800F4DD0u
#define AE_STAGE_G_LEVEL_GRID_BLOCK_MASK (AE_STAGE_G_LEVEL_GRID + 0x18u)
#define AE_STAGE_STATE_AREA_KIND 0x122u
#define AE_STAGE_STATE_PROGRESS_SELECTOR 0x12Eu
#define AE_STAGE_STATE_AREA_ENTRY 0x12Au
#define AE_STAGE_STATE_AREA_RESET_A 0x150u
#define AE_STAGE_STATE_AREA_RESET_B 0x152u
#define AE_STAGE_STATE_AREA_RESET_C 0x268u
#define AE_STAGE_STATE_AREA_RESET_D 0x2B8u
#define AE_STAGE_STATE_AREA_RESET_E 0x2BAu
#define AE_STAGE_STATE_AREA_BASE 0x234u
#define AE_STAGE_STATE_AREA_AUDIO_FLAG 0x238u
#define AE_STAGE_STATE_AREA_OBJECTS_BEGIN 0x21Cu
#define AE_STAGE_STATE_AREA_OBJECTS_END 0x220u
#define AE_STAGE_STATE_AREA_COLLISION_A 0x208u
#define AE_STAGE_STATE_AREA_COLLISION_B 0x20Cu
#define AE_STAGE_STATE_AREA_COLLISION_C 0x210u
#define AE_STAGE_STATE_AREA_COLLISION_D 0x214u
#define AE_STAGE_STATE_AREA_COLLISION_E 0x218u
#define AE_STAGE_STATE_AREA_COLLISION_COUNT 0x284u
#define AE_STAGE_STATE_AREA_COLLISION_KIND 0x2A0u
#define AE_STAGE_STATE_AREA_COLLISION_HEIGHT 0x2A4u
#define AE_STAGE_STATE_AREA_COLLISION_KIND_SCALED 0x2A8u
#define AE_STAGE_STATE_AREA_COLLISION_HEIGHT_SCALED 0x2ACu
#define AE_STAGE_STATE_AREA_SPECIAL_LOOKUP 0x2C0u
#define AE_STAGE_STATE_AREA_AUDIO_POINTER 0x28Cu
#define AE_STAGE_STATE_AREA_MUSIC_BUFFER 0x230u
#define AE_STAGE_AREA_INDEX_SPECIAL_FIRST 99u
#define AE_STAGE_AREA_INDEX_SPECIAL_END 104u
#define AE_STAGE_AREA_INDEX_SPECIAL_SEED 0x3039u
#define AE_STAGE_AREA_INDEX_SPECIAL_SEED_HI 0xD432u
#define AE_STAGE_G_AREA_LOAD_FIELD_ZERO 0x800EC350u
#define AE_STAGE_G_AREA_AUDIO_DESCRIPTOR 0x80154240u
#define AE_STAGE_G_AREA_AUDIO_DESCRIPTOR_DATA 0x80154244u
#define AE_STAGE_G_AREA_ITEM_DATA 0x80102800u
#define AE_STAGE_G_CUTSCENE_MODE_FLAG 0x800B01C2u
#define AE_STAGE_G_AREA_STREAM_RETRY 0x800E38A0u
#define AE_STAGE_G_AREA_CHANGE_FLAGS 0x800EC1E0u
#define AE_STAGE_G_STAGE_REQUEST_FLAGS 0x800EC2C8u
#define AE_STAGE_G_STAGE_REQUEST_EXTENDED_FLAGS 0x800EC2C0u
#define AE_STAGE_G_STAGE_REQUEST_AREA_FLAG 0x800F03F0u
#define AE_STAGE_G_STAGE_REQUEST_MODE_FLAG 0x800EC378u
#define AE_STAGE_G_STAGE_REQUEST_TEMP_VECTOR_A (AE_STAGE_G_AREA_CHANGE_FLAGS + 0x30u)
#define AE_STAGE_G_STAGE_REQUEST_TEMP_VECTOR_B (AE_STAGE_G_AREA_CHANGE_FLAGS + 0x34u)
#define AE_STAGE_G_STAGE_REQUEST_TEMP_VECTOR_C (AE_STAGE_G_AREA_CHANGE_FLAGS + 0x38u)
#define AE_STAGE_G_STAGE_REQUEST_AREA_FLAG_OFFSET 0xF2u
#define AE_STAGE_G_STAGE_REQUEST_MODE_FLAG_OFFSET 0xF3u
#define AE_STAGE_REQUEST_AREA_COPY_SOURCE 0x800A58E0u
#define AE_STAGE_G_TRANSITION_CHECK_LEVEL 0x800EC23Eu
#define AE_STAGE_ARC_LOAD_NEXT_LIST_INDEX 0x68u
#define AE_STAGE_G_AREA_EXIT_RETRY_MARKER (AE_STAGE_G_ARC_LOAD_STATE + AE_STAGE_ARC_LOAD_STATUS)
#define AE_STAGE_G_AREA_EXIT_RECORD (AE_STAGE_G_ARC_LOAD_STATE + AE_STAGE_ARC_LOAD_RESULT)
#define AE_STAGE_G_AREA_EXIT_LAST_RESULT (AE_STAGE_G_ARC_LOAD_STATE + AE_STAGE_ARC_LOAD_LAST_STATUS)
#define AE_STAGE_G_AREA_EXIT_INPUT (AE_STAGE_G_ARC_LOAD_STATE + AE_STAGE_ARC_LOAD_PARAMETER)
#define AE_STAGE_AREA_TIME_STATION 0x45u
#define AE_STAGE_AREA_STAGE_INTRO 0x18u
#define AE_STAGE_AREA_STAGE_CLEAR 0x35u
#define AE_STAGE_AREA_STAGE_SPECIAL_A 0x19u
#define AE_STAGE_AREA_STAGE_SPECIAL_B 0x49u
#define AE_STAGE_AREA_PLAYER_SETUP_SKIP 0x56u
#define AE_STAGE_LEVEL_HUB 33u
#define AE_STAGE_LEVEL_CHALLENGE_A 31u
#define AE_STAGE_LEVEL_CHALLENGE_B 32u
#define AE_STAGE_AREA_TEXCOPY_RECT_WORD_COUNT 2u
#define AE_STAGE_TERRAIN_FOG_PARAM 0xF000u
#define AE_STAGE_FOG_FIXED_POINT_ONE 0x01000000u
#define AE_STAGE_DEFAULT_FOG_START 1200u
#define AE_STAGE_DEFAULT_DRAW_DISTANCE 0x400u
#define AE_STAGE_TERRAIN_GRID_HEADER_BYTES 8u
#define AE_STAGE_AREA_FOG_COLOR_BYTES 4u
#define AE_STAGE_AREA_SPECIAL_BGM 72u

static int32_t stage_hook(AeRam *r, const AeStageHooks *h, AeStageHookFn fn,
                          const char *name, uint32_t a0, uint32_t a1,
                          uint32_t a2, uint32_t a3) {
    if (!h || !fn) {
        ae_ram_fail(r, "AeStageHooks.%s not provided", name);
        return 0;
    }
    return fn(h->user, r, a0, a1, a2, a3);
}

#define STAGE_CALL(name, a0, a1, a2, a3) \
    stage_hook(r, h, h ? h->name : NULL, #name, \
               (uint32_t)(a0), (uint32_t)(a1), (uint32_t)(a2), (uint32_t)(a3))

static uint32_t stage_frame_sp(const AeStageHooks *h, uint32_t frame_size) {
    uint32_t entry_sp = h && h->stack_pointer != 0u ?
                        h->stack_pointer : AE_STAGE_DEFAULT_STACK_TOP;
    return entry_sp - frame_size;
}

static const AeStageHooks *stage_nested_hooks(const AeStageHooks *h,
                                              uint32_t frame_size,
                                              AeStageHooks *nested) {
    if (!h) return NULL;
    *nested = *h;
    nested->stack_pointer = stage_frame_sp(h, frame_size);
    return nested;
}

/* 80010D04: selects one of the two member fields then issues the CD seek. */
int32_t ae_AreaLoadStartSeek(AeRam *r, const AeStageHooks *h, int32_t list_index) {
    uint32_t seek_location = stage_frame_sp(h, AE_STAGE_AREA_LOAD_START_SEEK_FRAME) +
                             AE_STAGE_STACK_LOCAL_OFFSET;
    uint32_t record_slot = AE_STAGE_G_AREA_LOAD_TABLE + ((uint32_t)list_index << 3);
    uint32_t record_list = ae_rd32(r, record_slot);
    int32_t area = ae_rds16(r, AE_STAGE_G_STATE + AE_STAGE_STATE_CURRENT_AREA);
    int32_t alternate = STAGE_CALL(AreaUsesAltMember, area,
                                   (int16_t)list_index, 0, 0);
    if (ae_ram_failed(r)) return 0;

    uint32_t member_offset = alternate ? 0u : 8u;
    uint32_t member = ae_rd16(r, record_list + member_offset);
    STAGE_CALL(ArcGetMemberPos, seek_location, member, 0, 0);
    if (ae_ram_failed(r)) return 0;
    return STAGE_CALL(FUN_80025E24, seek_location, 0, 0, 0);
}

/* 80011208: queues the current record's CD commands; file access stays in hooks. */
int32_t ae_AreaLoadQueueRead(AeRam *r, const AeStageHooks *h) {
    uint32_t frame_sp = stage_frame_sp(h, AE_STAGE_AREA_LOAD_QUEUE_READ_FRAME);
    uint32_t location_buffer = frame_sp + AE_STAGE_STACK_LOCAL_OFFSET;
    uint32_t queue_parameters = frame_sp + AE_STAGE_STACK_COMMAND_OFFSET;
    uint32_t state = AE_STAGE_G_ARC_LOAD_STATE;
    uint32_t record_list = ae_rd32(r, state + AE_STAGE_ARC_RECORDS);
    uint32_t record_index = ae_rd32(r, state + AE_STAGE_ARC_RECORD_INDEX);
    uint32_t record = record_list + (record_index << 3);
    uint32_t member = ae_rd16(r, record);

    STAGE_CALL(ArcGetMemberPos, location_buffer, member, 0, 0);
    if (ae_ram_failed(r)) return 0;

    uint32_t location = ae_rd32(r, location_buffer);
    uint32_t member_bytes = ae_rd32(r, location_buffer + 4u);
    uint32_t record_bytes = ae_rd32(r, record + 4u);
    ae_wr32(r, queue_parameters, location);
    ae_wr32(r, queue_parameters + 4u, record_bytes);
    ae_wr32(r, queue_parameters + 8u, member_bytes >> 11);

    STAGE_CALL(CdMgrQueueCommand, 14u, state + AE_STAGE_ARC_COMMAND,
               state + AE_STAGE_ARC_STATUS, 0u);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(CdMgrQueueCommand, 2u, queue_parameters,
               state + AE_STAGE_ARC_STATUS, 0u);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(CdMgrQueueCommand, 1u, 0u, state + AE_STAGE_ARC_STATUS, 0u);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(CdMgrQueueCommand, 32u, queue_parameters,
               state + AE_STAGE_ARC_STATUS, 0u);
    return 0;
}

/* 800114D8: initialize the load-list cursor and queue its first five commands. */
int32_t ae_AreaLoadStart(AeRam *r, const AeStageHooks *h, int32_t list_index) {
    uint32_t frame_sp = stage_frame_sp(h, AE_STAGE_AREA_LOAD_START_FRAME);
    uint32_t location_buffer = frame_sp + AE_STAGE_STACK_LOCAL_OFFSET;
    uint32_t command_buffer = frame_sp + AE_STAGE_STACK_COMMAND_OFFSET;
    uint32_t record_slot = AE_STAGE_G_AREA_LOAD_TABLE + ((uint32_t)list_index << 3);
    uint32_t record_list = ae_rd32(r, record_slot);
    uint32_t state = AE_STAGE_G_ARC_LOAD_STATE;
    int32_t area = ae_rds16(r, AE_STAGE_G_STATE + AE_STAGE_STATE_CURRENT_AREA);

    ae_wr32(r, state + AE_STAGE_ARC_LIST_INDEX, (uint32_t)list_index);
    ae_wr32(r, state + AE_STAGE_ARC_RECORDS, record_list);
    int32_t alternate = STAGE_CALL(AreaUsesAltMember, area,
                                   (int16_t)list_index, 0, 0);
    if (ae_ram_failed(r)) return 0;

    uint32_t member_offset = alternate ? 0u : 8u;
    uint32_t member = ae_rd16(r, record_list + member_offset);
    if (alternate) ae_wr32(r, state + AE_STAGE_ARC_RECORD_INDEX, 0u);
    else ae_wr32(r, state + AE_STAGE_ARC_RECORD_INDEX, 1u);

    STAGE_CALL(ArcGetMemberPos, location_buffer, member, 0, 0);
    if (ae_ram_failed(r)) return 0;
    int32_t seek_result = STAGE_CALL(FUN_80025E24, location_buffer, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    ae_wr8(r, command_buffer, 0x80u);
    ae_wr32(r, state + AE_STAGE_ARC_SEEK_RESULT, (uint32_t)seek_result);

    STAGE_CALL(CdMgrQueueCommand, 14u, command_buffer,
               state + AE_STAGE_ARC_STATUS, 0u);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(CdMgrQueueCommand, 2u, location_buffer,
               state + AE_STAGE_ARC_STATUS, 0u);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(CdMgrQueueCommand, 1u, 0u, state + AE_STAGE_ARC_STATUS, 0u);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(CdMgrQueueCommand, 21u, 0u, state + AE_STAGE_ARC_STATUS, 0u);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(CdMgrQueueCommand, 1u, 0u, state + AE_STAGE_ARC_STATUS, 0u);
    return 0;
}

/* 80094F20: start the shared stage-exit sequence and return zero. */
int32_t ae_StageRequestExit(AeRam *r, const AeStageHooks *h) {
    STAGE_CALL(StageExitStart, 0, 0, 0, 0);
    return 0;
}

/* 80050AFC: jr $ra; nop. */
void ae_StageProgressRestore_Nop(AeRam *r) {
    (void)r;
}

/* 800848B0: jr $ra; nop. */
void ae_StageProgressRestore_Nop2(AeRam *r) {
    (void)r;
}

/* 80096E0C: persist the eight monkey-activity bytes and each area's gimmick
 * state. Time attack deliberately leaves the regular progress tables alone. */
int32_t ae_StageProgressStoreArea(AeRam *r, const AeStageHooks *h, int32_t area) {
    (void)area; /* The original receives this word but uses the live stage state. */
    if (STAGE_CALL(IsTimeAttack, 0, 0, 0, 0) != 0) return 0;
    if (ae_ram_failed(r)) return 0;

    ae_MonkeyStoreActivities(r);
    uint32_t stage_state = AE_STAGE_G_STATE;
    uint32_t level = ae_rd16(r, stage_state + AE_STAGE_STATE_LEVEL);
    uint32_t first_area = (uint32_t)STAGE_CALL(GetLevelFirstArea, level, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;

    uint32_t current_area = ae_rd16(r, stage_state + AE_STAGE_STATE_CURRENT_AREA);
    uint32_t current_record = AE_STAGE_PROGRESS_TABLE +
        (current_area - first_area) * AE_STAGE_PROGRESS_RECORD_SIZE;
    for (uint32_t slot = 0; slot < AE_STAGE_PROGRESS_SLOT_COUNT; ++slot) {
        ae_wr8(r, current_record + slot,
               ae_rd8(r, AE_STAGE_G_STATE + AE_STAGE_AREA_PROGRESS_BYTES + slot));
    }

    uint32_t area_count = ae_rd16(r, stage_state + AE_STAGE_STATE_AREA_COUNT);
    for (uint32_t area_index = 0; area_index < area_count; ++area_index) {
        for (uint32_t slot = 0; slot < AE_STAGE_PROGRESS_SLOT_COUNT; ++slot) {
            uint32_t area_base = (uint32_t)STAGE_CALL(GetLevelFirstArea, level, 0, 0, 0);
            if (ae_ram_failed(r)) return 0;
            uint32_t source = AE_STAGE_PROGRESS_TABLE + area_index * AE_STAGE_PROGRESS_RECORD_SIZE + slot;
            uint32_t destination = AE_STAGE_GIMMICK_STATE_TABLE +
                (area_base + area_index) * AE_STAGE_PROGRESS_SLOT_COUNT + slot;
            ae_wr8(r, destination, ae_rd8(r, source));
        }
    }

    uint8_t area_progress = ae_rd8(r, stage_state + AE_STAGE_STATE_AREA_PROGRESS);
    uint32_t progress_area = ae_rd16(r, stage_state + AE_STAGE_STATE_LEVEL);
    if (progress_area - 0x18u < 6u) {
        ae_wr8(r, AE_STAGE_SPECIAL_PROGRESS, area_progress);
    } else {
        ae_wr8(r, AE_STAGE_AREA_PROGRESS_TABLE + progress_area, area_progress);
    }
    return 0;
}

/* 80096FAC: commit ordinary stage state on exit and preserve the special
 * time-attack result tables when leaving the time station. */
int32_t ae_StageProgressOnExit(AeRam *r, const AeStageHooks *h, int32_t area) {
    STAGE_CALL(GimmickSaveStates, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(GimmickSaveApplyProgress, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;

    uint32_t state = AE_STAGE_G_STATE;
    uint32_t level = ae_rd16(r, state + AE_STAGE_STATE_LEVEL);
    uint32_t current_area = ae_rd16(r, state + AE_STAGE_STATE_CURRENT_AREA);
    uint32_t area_kind = ae_rd8(r, state + AE_STAGE_STATE_PROGRESS_TYPE);

    if (current_area == 0x45u && area_kind == 6u) {
        int32_t time_attack = STAGE_CALL(IsTimeAttack, 0, 0, 0, 0);
        if (ae_ram_failed(r) || time_attack != 0) return 0;
        ae_wr8(r, AE_STAGE_SPECIAL_PROGRESS, ae_rd8(r, state + AE_STAGE_STATE_AREA_PROGRESS));
        return 0;
    }

    STAGE_CALL(ItemSaveCollectedMask, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(ZakoSaveDefeatedMask, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    ae_MonkeyStoreActivities(r);

    uint32_t first_area = (uint32_t)STAGE_CALL(GetLevelFirstArea, level, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    int32_t area_delta = (int16_t)(uint16_t)(current_area - first_area);
    uint32_t current_record = AE_STAGE_PROGRESS_TABLE +
        (uint32_t)area_delta * AE_STAGE_PROGRESS_RECORD_SIZE;
    for (uint32_t slot = 0; slot < AE_STAGE_PROGRESS_SLOT_COUNT; ++slot) {
        ae_wr8(r, current_record + slot,
               ae_rd8(r, AE_STAGE_G_STATE + AE_STAGE_AREA_PROGRESS_BYTES + slot));
    }

    if (area != 0x45) return 0;
    first_area = (uint32_t)STAGE_CALL(GetLevelFirstArea, level, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    if (first_area != current_area || area_kind != 6u) return 0;

    int32_t time_attack = STAGE_CALL(IsTimeAttack, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    if (time_attack != 0) {
        if (level == 0x1Cu) {
            for (uint32_t slot = 0; slot < AE_STAGE_PROGRESS_SLOT_COUNT; ++slot) {
                ae_wr8(r, AE_STAGE_TIME_ATTACK_PROGRESS + slot,
                       ae_rd8(r, AE_STAGE_PROGRESS_TABLE + slot));
            }
        } else if (level == 0x1Bu) {
            for (uint32_t slot = 0; slot < AE_STAGE_PROGRESS_SLOT_COUNT; ++slot) {
                for (uint32_t row = 0; row < 5u; ++row) {
                    uint32_t source = AE_STAGE_PROGRESS_TABLE + row * AE_STAGE_PROGRESS_RECORD_SIZE + slot;
                    uint32_t destination = AE_STAGE_TIME_ATTACK_PROGRESS + (row + 1u) * AE_STAGE_PROGRESS_RECORD_SIZE + slot;
                    ae_wr8(r, destination, ae_rd8(r, source));
                }
            }
        }
        return 0;
    }

    uint32_t area_count = ae_rd16(r, state + AE_STAGE_STATE_AREA_COUNT);
    for (uint32_t area_index = 0; area_index < area_count; ++area_index) {
        for (uint32_t slot = 0; slot < AE_STAGE_PROGRESS_SLOT_COUNT; ++slot) {
            first_area = (uint32_t)STAGE_CALL(GetLevelFirstArea, level, 0, 0, 0);
            if (ae_ram_failed(r)) return 0;
            uint32_t source = AE_STAGE_PROGRESS_TABLE +
                area_index * AE_STAGE_PROGRESS_RECORD_SIZE + slot;
            uint32_t destination = AE_STAGE_GIMMICK_STATE_TABLE +
                (first_area + area_index) * AE_STAGE_PROGRESS_SLOT_COUNT + slot;
            ae_wr8(r, destination, ae_rd8(r, source));
        }
    }
    ae_wr8(r, AE_STAGE_SPECIAL_PROGRESS, ae_rd8(r, state + AE_STAGE_STATE_AREA_PROGRESS));
    return 0;
}

/* 800966B4: initialize stage-local progress from the selected area and the
 * saved per-level/per-area tables. The area blob remains data, not simulated IO. */
int32_t ae_StageProgressOnEnter(AeRam *r, const AeStageHooks *h, int32_t area) {
    uint32_t state = AE_STAGE_G_STATE;
    uint32_t requested_area = (uint32_t)area;
    int32_t area_kind = STAGE_CALL(AreaGetKind, requested_area, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    ae_wr16(r, state + AE_STAGE_STATE_AREA_KIND, (uint32_t)area_kind);
    int32_t level_result = STAGE_CALL(AreaGetLevel, requested_area, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    uint32_t level = (uint32_t)level_result;
    ae_wr16(r, state + AE_STAGE_STATE_LEVEL, level);

    if (ae_rd8(r, state + AE_STAGE_STATE_PROGRESS_TYPE) == 1u &&
        ae_rd16(r, state + AE_STAGE_STATE_MODE) == 19u) {
        ae_wr16(r, state + AE_STAGE_STATE_AREA_PROGRESS, 0u);
        for (uint32_t row = 0; row < 12u; ++row) {
            for (uint32_t slot = 0; slot < AE_STAGE_PROGRESS_SLOT_COUNT; ++slot) {
                ae_wr8(r, AE_STAGE_TIME_ATTACK_PROGRESS + row * AE_STAGE_PROGRESS_RECORD_SIZE + slot, 0u);
            }
        }
    }

    ae_wr16(r, state + AE_STAGE_STATE_PROGRESS_RESET, 0u);
    ae_MonkeyActivityReset(r);
    ae_wr16(r, AE_STAGE_G_INIT_PROGRESS_FLAG, 1u);
    STAGE_CALL(GimmickSaveTableReset, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(ItemsClearFlags, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(ZakoClearSlotTable, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(SoundSilence, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;

    uint32_t first_area = (uint32_t)STAGE_CALL(GetLevelFirstArea, level, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    if (level == 33u) {
        uint32_t monkey_area_index = ae_rd16(r, state + AE_STAGE_STATE_AREA_INDEX);
        uint32_t monkey_count_entry = AE_STAGE_HUB_MONKEY_COUNTS + monkey_area_index * 2u;
        uint16_t monkey_count = ae_rd16(r, monkey_count_entry);
        ae_wr16(r, state + AE_STAGE_STATE_AREA_COUNT, 1u);
        ae_wr16(r, state + AE_STAGE_STATE_MONKEY_COUNT, monkey_count);
        ae_wr16(r, state + AE_STAGE_STATE_PROGRESS_KIND, monkey_count);
        ae_wr16(r, state + AE_STAGE_STATE_AREA_SCORE, 0u);
        return 0;
    }
    if (first_area != requested_area || level == 31u || level == 32u || level == 34u ||
        ae_rd16(r, state + AE_STAGE_STATE_AREA_KIND) == 3u) {
        ae_wr16(r, state + AE_STAGE_STATE_AREA_COUNT, 0u);
        ae_wr16(r, state + AE_STAGE_STATE_MONKEY_COUNT, 0u);
        ae_wr16(r, state + AE_STAGE_STATE_PROGRESS_KIND, 0u);
        ae_wr16(r, state + AE_STAGE_STATE_AREA_SCORE, 0u);
        return 0;
    }

    {
        uint32_t level_entry = AE_STAGE_LEVEL_TABLE + level * AE_STAGE_LEVEL_ROW_SIZE;
        uint32_t area_data = AE_STAGE_AREA_BLOB + ae_rd32(r, AE_STAGE_AREA_HEADER_POINTER);
        uint32_t area_records = area_data + AE_STAGE_AREA_DATA_RECORDS_OFFSET;
        uint32_t area_count = ae_rd8(r, level_entry);
        ae_wr32(r, state + AE_STAGE_STATE_AREA_DATA, area_data);
        ae_wr16(r, state + AE_STAGE_STATE_AREA_COUNT, area_count);

        uint32_t progress_kind_offset =
            ae_rd16(r, state + AE_STAGE_STATE_MODE) == 19u ? 4u : 2u;
        ae_wr16(r, state + AE_STAGE_STATE_PROGRESS_KIND,
                ae_rd8(r, level_entry + progress_kind_offset));
        int32_t monkey_count = STAGE_CALL(LevelMonkeyCountGet, level, 0, 0, 0);
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, state + AE_STAGE_STATE_MONKEY_COUNT, (uint32_t)monkey_count);

        for (uint32_t index = 0; index < area_count * 2u; ++index) {
            uint32_t order = ae_rd16(r, area_records + index * 2u + 6u);
            ae_wr16(r, AE_STAGE_AREA_ORDER + index * 2u, order);
        }

        uint32_t area_score = 0;
        if (level - 24u < 6u) {
            for (uint32_t level_index = 24u; level_index < 30u; ++level_index) {
                area_score += ae_rd8(r, AE_STAGE_LEVEL_TABLE +
                                     level_index * AE_STAGE_LEVEL_ROW_SIZE + 3u);
            }
        } else {
            area_score = ae_rd8(r, level_entry + 3u);
        }
        ae_wr16(r, state + AE_STAGE_STATE_AREA_SCORE, area_score);

        if (first_area == 69u) {
            for (uint32_t row = 0; row < 12u; ++row) {
                for (uint32_t slot = 0; slot < AE_STAGE_PROGRESS_SLOT_COUNT; ++slot) {
                    ae_wr8(r, AE_STAGE_PROGRESS_TABLE +
                           row * AE_STAGE_PROGRESS_RECORD_SIZE + slot, 0u);
                }
            }
        } else if (ae_rd16(r, state + AE_STAGE_STATE_MODE) == 19u) {
            for (uint32_t row = 0; row < 12u; ++row) {
                for (uint32_t slot = 0; slot < AE_STAGE_PROGRESS_SLOT_COUNT; ++slot) {
                    ae_wr8(r, AE_STAGE_PROGRESS_TABLE +
                           row * AE_STAGE_PROGRESS_RECORD_SIZE + slot, 0u);
                }
            }
            if (level == 28u) {
                for (uint32_t slot = 0; slot < AE_STAGE_PROGRESS_SLOT_COUNT; ++slot) {
                    ae_wr8(r, AE_STAGE_PROGRESS_TABLE + slot,
                           ae_rd8(r, AE_STAGE_TIME_ATTACK_PROGRESS + slot));
                }
            } else if (level == 27u) {
                for (uint32_t row = 0; row < 5u; ++row) {
                    for (uint32_t slot = 0; slot < AE_STAGE_PROGRESS_SLOT_COUNT; ++slot) {
                        uint32_t source = AE_STAGE_TIME_ATTACK_PROGRESS +
                            (row + 1u) * AE_STAGE_PROGRESS_RECORD_SIZE + slot;
                        uint32_t destination = AE_STAGE_PROGRESS_TABLE +
                            row * AE_STAGE_PROGRESS_RECORD_SIZE + slot;
                        ae_wr8(r, destination, ae_rd8(r, source));
                    }
                }
            }
        } else {
            for (uint32_t area_index = 0; area_index < area_count; ++area_index) {
                for (uint32_t slot = 0; slot < AE_STAGE_PROGRESS_SLOT_COUNT; ++slot) {
                    uint32_t area_base = (uint32_t)STAGE_CALL(GetLevelFirstArea, level, 0, 0, 0);
                    if (ae_ram_failed(r)) return 0;
                    uint32_t source = AE_STAGE_GIMMICK_STATE_TABLE +
                        (area_base + area_index) * AE_STAGE_PROGRESS_SLOT_COUNT + slot;
                    uint32_t destination = AE_STAGE_PROGRESS_TABLE +
                        area_index * AE_STAGE_PROGRESS_RECORD_SIZE + slot;
                    ae_wr8(r, destination, ae_rd8(r, source));
                }
            }
        }
    }

    for (uint32_t slot = 0; slot < AE_STAGE_PROGRESS_SLOT_COUNT; ++slot) {
        uint32_t area_base = (uint32_t)STAGE_CALL(GetLevelFirstArea, level, 0, 0, 0);
        if (ae_ram_failed(r)) return 0;
        int32_t area_delta = (int16_t)(uint16_t)(requested_area - area_base);
        uint32_t source = AE_STAGE_PROGRESS_TABLE +
            (uint32_t)area_delta * AE_STAGE_PROGRESS_RECORD_SIZE + slot;
        ae_wr8(r, state + AE_STAGE_AREA_PROGRESS_BYTES + slot, ae_rd8(r, source));
    }

    uint32_t current_level = ae_rd16(r, state + AE_STAGE_STATE_LEVEL);
    if (ae_rd16(r, state + AE_STAGE_STATE_MODE) == 19u) {
        ae_wr16(r, state + AE_STAGE_STATE_PROGRESS_SELECTOR, 2u);
    } else if (current_level - 24u < 6u) {
        ae_wr16(r, state + AE_STAGE_STATE_PROGRESS_SELECTOR,
                ae_rd8(r, AE_STAGE_LEVEL_PROGRESS_COUNT));
        ae_wr16(r, state + AE_STAGE_STATE_AREA_PROGRESS,
                ae_rd8(r, AE_STAGE_SPECIAL_PROGRESS));
    } else {
        ae_wr16(r, state + AE_STAGE_STATE_PROGRESS_SELECTOR,
                ae_rd8(r, AE_STAGE_LEVEL_PROGRESS_MODE + current_level));
        ae_wr16(r, state + AE_STAGE_STATE_AREA_PROGRESS,
                ae_rd8(r, AE_STAGE_AREA_PROGRESS_TABLE + current_level));
    }

    ae_wr16(r, AE_STAGE_G_PROGRESS_MIRROR,
            ae_rd16(r, state + AE_STAGE_STATE_PROGRESS_SELECTOR));
    return 0;
}

/* 800972E0: restore the selected area's eight saved activity bytes and update
 * the one-time progress flag for the hub return path. */
int32_t ae_StageProgressRestore(AeRam *r, const AeStageHooks *h, int32_t area) {
    uint32_t state = AE_STAGE_G_STATE;
    uint32_t requested_area = (uint32_t)area;
    uint32_t progress_type = ae_rd8(r, state + AE_STAGE_STATE_PROGRESS_TYPE);

    if (requested_area == 0x45u && progress_type == 2u) {
        STAGE_CALL(GimmickSaveTableReset, 0, 0, 0, 0);
        return 0;
    }

    uint32_t current_area = ae_rd16(r, state + AE_STAGE_STATE_CURRENT_AREA);
    if (current_area == 0x45u) {
        int32_t level_for_area = STAGE_CALL(AreaGetLevel, requested_area, 0, 0, 0);
        if (ae_ram_failed(r)) return 0;
        uint32_t first_area = (uint32_t)STAGE_CALL(GetLevelFirstArea,
                                                   (uint32_t)level_for_area, 0, 0, 0);
        if (ae_ram_failed(r)) return 0;
        if (first_area == requested_area && progress_type == 2u) {
            ae_StageProgressOnEnter(r, h, area);
            return 0;
        }
    }

    ae_wr16(r, AE_STAGE_G_INIT_PROGRESS_FLAG, progress_type == 2u ? 0u : 1u);
    int32_t level_for_area = STAGE_CALL(AreaGetLevel, requested_area, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    uint32_t first_area = (uint32_t)STAGE_CALL(GetLevelFirstArea,
                                               (uint32_t)level_for_area, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;

    int32_t area_delta = (int16_t)(uint16_t)(requested_area - first_area);
    uint32_t saved_record = AE_STAGE_PROGRESS_TABLE +
        (uint32_t)area_delta * AE_STAGE_PROGRESS_RECORD_SIZE;
    for (uint32_t slot = 0; slot < AE_STAGE_PROGRESS_SLOT_COUNT; ++slot) {
        ae_wr8(r, state + AE_STAGE_AREA_PROGRESS_BYTES + slot,
               ae_rd8(r, saved_record + slot));
    }

    ae_StageProgressRestore_Nop2(r);
    ae_StageProgressRestore_Nop(r);
    return 0;
}

/* 80031B60: bind the area's data tables, features, audio and runtime systems. */
int32_t ae_AreaLoad(AeRam *r, const AeStageHooks *h, int32_t area, int32_t entry) {
    uint32_t state = AE_STAGE_G_STATE;
    uint32_t area_index = (uint32_t)area;
    ae_wr32(r, AE_STAGE_G_AREA_LOAD_MODE, 0u);
    if (area_index >= AE_STAGE_AREA_INDEX_SPECIAL_FIRST &&
        area_index < AE_STAGE_AREA_INDEX_SPECIAL_END) {
        ae_wr32(r, AE_STAGE_G_AREA_LOAD_SPECIAL, 1u);
        ae_wr16(r, AE_STAGE_G_AREA_LOAD_RANDOM_SEED, AE_STAGE_AREA_INDEX_SPECIAL_SEED);
        ae_wr16(r, AE_STAGE_G_AREA_LOAD_RANDOM_SEED_HI, AE_STAGE_AREA_INDEX_SPECIAL_SEED_HI);
        ae_wr32(r, AE_STAGE_G_AREA_LOAD_FIELD_ZERO, 0u);
        ae_wr8(r, state + AE_STAGE_STATE_AREA_LOAD_SPECIAL_BYTE, 0u);
    } else {
        ae_wr32(r, AE_STAGE_G_AREA_LOAD_SPECIAL, 0u);
    }

    if (ae_rd16(r, state + AE_STAGE_STATE_PROGRESS_SELECTOR) == 2u) {
        ae_wr16(r, AE_STAGE_G_AREA_LOAD_RANDOM_SEED, AE_STAGE_AREA_INDEX_SPECIAL_SEED);
        ae_wr16(r, AE_STAGE_G_AREA_LOAD_RANDOM_SEED_HI, AE_STAGE_AREA_INDEX_SPECIAL_SEED_HI);
        ae_wr32(r, AE_STAGE_G_AREA_LOAD_FIELD_ZERO, 0u);
    }
    ae_wr32(r, state + AE_STAGE_STATE_AREA_LOAD_PROGRESS_FLAG,
            ae_rd8(r, state + AE_STAGE_STATE_PROGRESS_TYPE) == 1u ? 1u : 0u);

    STAGE_CALL(HeapInit, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(AreaBumpAllocSet, ae_rd32(r, AE_STAGE_G_AREA_ALLOC_CURSOR), 0, 0, 0);
    if (ae_ram_failed(r)) return 0;

    uint32_t header = AE_STAGE_AREA_BASE + ae_rd32(r, AE_STAGE_AREA_HEADER_RECORD);
    ae_wr32(r, state + AE_STAGE_STATE_AREA_HEADER, header);
    int32_t area_kind = STAGE_CALL(AreaGetKind, area_index, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    ae_wr16(r, state + AE_STAGE_STATE_AREA_KIND, (uint32_t)area_kind);
    int32_t level = STAGE_CALL(AreaGetLevel, area_index, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    ae_wr16(r, state + AE_STAGE_STATE_LEVEL, (uint32_t)level);
    ae_wr16(r, state + AE_STAGE_STATE_CURRENT_AREA, area_index);
    ae_wr16(r, state + AE_STAGE_STATE_AREA_ENTRY, (uint32_t)entry);
    ae_wr16(r, state + AE_STAGE_STATE_AREA_RESET_A, 0u);
    ae_wr16(r, state + AE_STAGE_STATE_AREA_RESET_B, 0u);
    ae_wr16(r, state + AE_STAGE_STATE_AREA_RESET_C, 0u);
    ae_wr16(r, state + AE_STAGE_STATE_AREA_RESET_D, 0u);
    ae_wr16(r, state + AE_STAGE_STATE_AREA_RESET_E, 0u);
    ae_wr32(r, AE_STAGE_G_AREA_LOAD_SPECIAL_INDEX, 50u);

    uint32_t matrix = AE_STAGE_G_AREA_MATRIX_BANK;
    uint32_t matrix_pool = AE_STAGE_G_AREA_MATRIX_POOL;
    for (uint32_t index = 0; index < AE_STAGE_AREA_MATRIX_COUNT; ++index) {
        ae_sdk_MatrixIdentity(r, matrix);
        ae_sdk_MatrixIdentity(r, matrix_pool);
        matrix += AE_STAGE_ROTATION_MATRIX_SIZE;
        matrix_pool += AE_STAGE_ROTATION_MATRIX_SIZE;
    }
    ae_sdk_MatrixIdentity(r, AE_STAGE_G_AREA_MATRIX_ROTATION);
    ae_sdk_MatrixIdentity(r, AE_STAGE_G_AREA_MATRIX_TEMP);

    STAGE_CALL(AreaSpecialFlagsClear, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(AreaSetVramAnimList, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(AreaSetZoneBoxes, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(IndicatorsReset, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(AreaSetFeature30, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;

    for (uint32_t index = 0; index < AE_STAGE_AREA_FEATURE_COUNT; ++index) {
        uint32_t feature_address = header + AE_STAGE_AREA_FEATURES +
                                   index * AE_STAGE_AREA_FEATURE_SIZE;
        uint32_t feature = ae_rd32(r, feature_address);
        uint32_t feature_id = feature & 0xFFu;
        if (feature_id == 0u) continue;
        uint32_t feature_data = header + (feature >> 8) * 4u;
        switch (feature_id) {
        case AE_STAGE_AREA_FEATURE_VRAM_ANIM:
            STAGE_CALL(AreaSetVramAnimList, feature_data, 0, 0, 0);
            break;
        case AE_STAGE_AREA_FEATURE_ZONE_BOXES:
            STAGE_CALL(AreaSetZoneBoxes, feature_data, 0, 0, 0);
            break;
        case AE_STAGE_AREA_FEATURE_INDICATOR_A:
            STAGE_CALL(AreaSetIndicatorModelA, feature_data, 0, 0, 0);
            break;
        case AE_STAGE_AREA_FEATURE_INDICATOR_B:
            STAGE_CALL(AreaSetIndicatorModelB, feature_data, 0, 0, 0);
            break;
        case AE_STAGE_AREA_FEATURE_30:
            STAGE_CALL(AreaSetFeature30, feature_data, 0, 0, 0);
            break;
        case AE_STAGE_AREA_FEATURE_31:
            STAGE_CALL(AreaSetFeature31, feature_data, 0, 0, 0);
            break;
        case AE_STAGE_AREA_FEATURE_32:
            STAGE_CALL(AreaSetFeature32, feature_data, 0, 0, 0);
            break;
        case AE_STAGE_AREA_FEATURE_SPECIAL_ACTOR_A:
            STAGE_CALL(AreaSetSpecialActor, feature_data, 1u, 0, 0);
            ae_wr32(r, AE_STAGE_G_AREA_SPECIAL_FLAGS,
                    ae_rd32(r, AE_STAGE_G_AREA_SPECIAL_FLAGS) | 1u);
            break;
        case AE_STAGE_AREA_FEATURE_SPECIAL_ACTOR_B:
            STAGE_CALL(AreaSetSpecialActor, feature_data, 2u, 0, 0);
            ae_wr32(r, AE_STAGE_G_AREA_SPECIAL_FLAGS,
                    ae_rd32(r, AE_STAGE_G_AREA_SPECIAL_FLAGS) | 2u);
            break;
        default:
            break;
        }
        if (ae_ram_failed(r)) return 0;
    }

    uint32_t area_data_record = AE_STAGE_AREA_BASE +
                                ae_rd32(r, AE_STAGE_AREA_OBJECT_TABLE_RECORD);
    uint32_t area_object_count = ae_rd32(r, area_data_record +
                                         AE_STAGE_AREA_OBJECT_COUNT_OFFSET);
    uint32_t collision = header +
                         ae_rd32(r, header + AE_STAGE_AREA_COLLISION_HEADER) * 4u;
    uint32_t collision_grid_offset =
        ae_rd32(r, collision + AE_STAGE_AREA_COLLISION_GRID_ENTRY);
    uint32_t collision_shape_offset =
        ae_rd32(r, collision + AE_STAGE_AREA_COLLISION_SHAPE_ENTRY);
    uint32_t collision_list_offset =
        ae_rd32(r, collision + AE_STAGE_AREA_COLLISION_LIST_ENTRY);
    uint32_t collision_grid_aux_offset =
        ae_rd32(r, collision + AE_STAGE_AREA_COLLISION_GRID_AUX_ENTRY);
    uint32_t collision_shape_aux_offset =
        ae_rd32(r, collision + AE_STAGE_AREA_COLLISION_SHAPE_AUX_ENTRY);
    uint32_t mesh_table = header + ae_rd32(r, header + AE_STAGE_AREA_MESH_TABLE_OFFSET) * 4u;
    uint32_t props_table = header + ae_rd32(r, header + AE_STAGE_AREA_PROPS_OFFSET) * 4u;
    uint32_t grid_data = header + ae_rd32(r, header + AE_STAGE_AREA_GRID_OFFSET) * 4u;

    ae_wr32(r, state + AE_STAGE_STATE_AREA_BASE, AE_STAGE_AREA_BASE);
    ae_wr32(r, state + AE_STAGE_STATE_AREA_OBJECTS_BEGIN,
            area_data_record + AE_STAGE_AREA_OBJECT_COUNT_OFFSET +
                AE_STAGE_AREA_OBJECTS_OFFSET);
    ae_wr32(r, state + AE_STAGE_STATE_AREA_OBJECTS_END,
            area_data_record + AE_STAGE_AREA_OBJECT_COUNT_OFFSET + AE_STAGE_AREA_OBJECTS_OFFSET +
                area_object_count * AE_STAGE_AREA_OBJECT_RECORD_SIZE);
    ae_wr32(r, state + AE_STAGE_STATE_AREA_COLLISION_TABLE,
            AE_STAGE_AREA_BASE + ae_rd32(r, AE_STAGE_AREA_COLLISION_RECORD));
    ae_wr32(r, state + AE_STAGE_STATE_AREA_PROPS_TABLE,
            AE_STAGE_AREA_BASE + ae_rd32(r, AE_STAGE_AREA_PROPS_RECORD));
    ae_wr32(r, state + AE_STAGE_STATE_AREA_ITEMS_TABLE,
            AE_STAGE_AREA_BASE + ae_rd32(r, AE_STAGE_AREA_ITEMS_RECORD));
    ae_wr32(r, state + AE_STAGE_STATE_AREA_MESHES_TABLE,
            AE_STAGE_AREA_BASE + ae_rd32(r, AE_STAGE_AREA_MESHES_RECORD));
    ae_wr32(r, state + AE_STAGE_STATE_AREA_MONKEY_TABLE, AE_STAGE_AREA_BASE +
            ae_rd32(r, AE_STAGE_AREA_MONKEY_RECORD));
    ae_wr32(r, state + AE_STAGE_STATE_AREA_ITEM_DATA, AE_STAGE_G_AREA_ITEM_DATA);
    ae_wr32(r, state + AE_STAGE_STATE_AREA_OBJECT_COUNT, area_object_count);
    ae_wr32(r, state + AE_STAGE_STATE_AREA_COLLISION_A,
            collision + collision_shape_offset + AE_STAGE_AREA_COLLISION_ENTRY_DATA_OFFSET);
    ae_wr32(r, state + AE_STAGE_STATE_AREA_COLLISION_B,
            collision + collision_list_offset + AE_STAGE_AREA_COLLISION_ENTRY_DATA_OFFSET);
    ae_wr32(r, state + AE_STAGE_STATE_AREA_COLLISION_C,
            collision + collision_grid_offset + AE_STAGE_AREA_COLLISION_GRID_DATA_OFFSET);
    ae_wr32(r, state + AE_STAGE_STATE_AREA_COLLISION_D,
            collision + collision_grid_aux_offset + AE_STAGE_AREA_COLLISION_ENTRY_DATA_OFFSET);
    ae_wr32(r, state + AE_STAGE_STATE_AREA_COLLISION_E,
            collision + collision_shape_aux_offset + AE_STAGE_AREA_COLLISION_ENTRY_DATA_OFFSET);
    ae_wr16(r, state + AE_STAGE_STATE_AREA_COLLISION_COUNT,
            ae_rd16(r, collision + collision_list_offset));
    int32_t collision_kind = ae_rds16(
        r, collision + collision_grid_offset + AE_STAGE_AREA_COLLISION_KIND_OFFSET);
    int32_t collision_height = ae_rds16(
        r, collision + collision_grid_offset + AE_STAGE_AREA_COLLISION_HEIGHT_OFFSET);
    ae_wr32(r, state + AE_STAGE_STATE_AREA_COLLISION_KIND, (uint32_t)collision_kind);
    ae_wr32(r, state + AE_STAGE_STATE_AREA_COLLISION_HEIGHT, (uint32_t)collision_height);
    ae_wr32(r, state + AE_STAGE_STATE_AREA_COLLISION_KIND_SCALED,
            (uint32_t)collision_kind << 8);
    ae_wr32(r, state + AE_STAGE_STATE_AREA_COLLISION_HEIGHT_SCALED,
            (uint32_t)collision_height << 8);
    int32_t special_lookup_index = (int32_t)area % (int32_t)AE_STAGE_AREA_SPECIAL_INDEX_COUNT;
    ae_wr32(r, state + AE_STAGE_STATE_AREA_SPECIAL_LOOKUP,
            ae_rd32(r, AE_STAGE_AREA_SPECIAL_INDEX_TABLE +
                       (uint32_t)special_lookup_index * 4u));
    ae_wr32(r, state + AE_STAGE_STATE_MESH_TABLE, mesh_table);
    ae_wr32(r, state + AE_STAGE_STATE_PROP_PLACEMENTS,
            props_table + AE_STAGE_AREA_PROP_PLACEMENTS_OFFSET);
    ae_wr32(r, state + AE_STAGE_STATE_PROP_COUNT, ae_rd32(r, props_table));
    ae_wr32(r, state + AE_STAGE_STATE_AREA_GRID, grid_data);
    ae_wr32(r, state + AE_STAGE_STATE_AREA_AUDIO_POINTER,
            ae_rd32(r, AE_STAGE_G_AREA_AUDIO_DESCRIPTOR));
    if (ae_rd32(r, AE_STAGE_G_AREA_AUDIO_DESCRIPTOR) == 0u) {
        ae_wr32(r, state + AE_STAGE_STATE_AREA_AUDIO_FLAG, 0u);
    } else {
        ae_wr32(r, state + AE_STAGE_STATE_AREA_AUDIO_FLAG,
                AE_STAGE_G_AREA_AUDIO_DESCRIPTOR_DATA);
    }

    int32_t preserve_music = STAGE_CALL(SoundAreaKeepsMusic, area_index, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    if (preserve_music != 0) {
        uint32_t byte_count = ae_rd32(r, AE_STAGE_AREA_SOUND_SIZE_RECORD);
        uint32_t word_count = (byte_count + 3u) >> 2;
        uint32_t source = AE_STAGE_AREA_BASE + ae_rd32(r, AE_STAGE_AREA_SOUND_SOURCE_RECORD);
        uint32_t destination = ae_rd32(r, state + AE_STAGE_STATE_AREA_MUSIC_BUFFER);
        for (uint32_t word = 0; word < word_count; ++word) {
            ae_wr32(r, destination + word * 4u, ae_rd32(r, source + word * 4u));
        }
    }

    if (preserve_music != 0) {
        if (ae_rd8(r, state + AE_STAGE_STATE_PROGRESS_TYPE) == AE_STAGE_AREA_BGM_PRESERVE_MODE) {
            int keep_pair = area_index == 0x4Bu || area_index == 4u || area_index == 3u ||
                            area_index == 0x21u || area_index == 0x22u || area_index == 0x1Eu ||
                            area_index == 0x20u || area_index == 6u || area_index == 8u ||
                            area_index == 9u || area_index == 10u;
            if (keep_pair) {
                STAGE_CALL(BgmStart, 0, 0, 0, 0);
                if (ae_ram_failed(r)) return 0;
                STAGE_CALL(BgmUpdateVolume, 0, 0, 0, 0);
                if (ae_ram_failed(r)) return 0;
            } else if (area_index == AE_STAGE_AREA_SPECIAL_BGM) {
                STAGE_CALL(BgmStart, 0, 0, 0, 0);
                if (ae_ram_failed(r)) return 0;
            }
        }
    }

    uint32_t grid = AE_STAGE_G_LEVEL_GRID;
    /* the four colour bytes move from area header +0x20 to level grid +0x1C (unaligned lwl/lwr, swl/swr) */
    ae_wr32(r, grid + AE_STAGE_LEVEL_GRID_FOG_COLOR,
            ae_rd32(r, header + AE_STAGE_AREA_FOG_COLOR));
    if (ae_rd16(r, header + AE_STAGE_AREA_FOG_ENABLED) == 0u) {
        ae_wr32(r, grid + AE_STAGE_LEVEL_GRID_FOG_START, AE_STAGE_DEFAULT_FOG_START);
        ae_wr32(r, grid + AE_STAGE_LEVEL_GRID_FOG_MULTIPLIER, 0u);
    } else {
        uint32_t fog_near = ae_rd16(r, header + AE_STAGE_AREA_FOG_NEAR);
        uint32_t fog_far = ae_rd16(r, header + AE_STAGE_AREA_FOG_FAR);
        int32_t fog_range = (int32_t)fog_far - (int32_t)fog_near;
        if (fog_range <= 0) fog_range = 1;
        ae_wr32(r, grid + AE_STAGE_LEVEL_GRID_FOG_MULTIPLIER,
                AE_STAGE_FOG_FIXED_POINT_ONE / (uint32_t)fog_range);
        ae_wr32(r, grid + AE_STAGE_LEVEL_GRID_FOG_START, fog_near);
    }
    ae_wr32(r, grid + AE_STAGE_LEVEL_GRID_FOG_MULTIPLIER, AE_STAGE_TERRAIN_FOG_PARAM);
    ae_wr32(r, grid + AE_STAGE_LEVEL_GRID_DRAW_DISTANCE,
            ae_rd32(r, grid + AE_STAGE_LEVEL_GRID_FOG_START) * 4u +
                AE_STAGE_DEFAULT_DRAW_DISTANCE);
    STAGE_CALL(ApplyLevelFog, grid, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(SetColorMatrixMode, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(SoundLoopsStopAll, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;

    ae_AreaLoadGridAndProps(r, h);
    if (ae_ram_failed(r)) return 0;
    uint32_t terrain_mask = (uint32_t)STAGE_CALL(BuildTerrainBlockMask, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    ae_wr32(r, AE_STAGE_G_LEVEL_GRID_BLOCK_MASK, terrain_mask);
    STAGE_CALL(TableClearDAT_800BEF60, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(DebugMenuInitPlayerParams, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(DebugMenuInitCounters, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(DebugMenuInitCounters2, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(MessageBoxClose, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    ae_CollisionBodiesReset(r);
    STAGE_CALL(PlayerSpawnAtEntry, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(TargetListClear, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(ZakoInit, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    ae_MonkeySpawnAll(r, h ? h->monkey_hooks : NULL);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(MonkeyLodModelsInit, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(FxSlotsReset, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(ProjectilesReset, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(EffectsReset, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(ColliderZoneMasksUpdate, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(GimmickSpawnAll, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(BossInit, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    STAGE_CALL(ItemsLoadForArea, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;

    ae_GadgetSlotsInitForExit(r, h ? h->gadget_hooks : NULL);
    if (ae_ram_failed(r)) return 0;
    if (ae_rd16(r, header + AE_STAGE_AREA_TEXCOPY_ENABLED) != 0u) {
        uint32_t texcopy_rect = stage_frame_sp(h, AE_STAGE_AREA_LOAD_FRAME) +
                                AE_STAGE_STACK_LOCAL_OFFSET;
        for (uint32_t word = 0; word < AE_STAGE_AREA_TEXCOPY_RECT_WORD_COUNT; ++word) {
            ae_wr32(r, texcopy_rect + word * 4u,
                    ae_rd32(r, AE_STAGE_G_TEXCOPY_RECT_TABLE + word * 4u));
        }
        STAGE_CALL(AreaTexCopyInit, texcopy_rect, 0u,
                   ae_rd32(r, header + AE_STAGE_AREA_TEXCOPY_RGBA), 0u);
        if (ae_ram_failed(r)) return 0;
    }
    STAGE_CALL(RcCarDespawn, 0, 0, 0, 0);
    return 0;
}

/* 8003244C: bind the area's heightfield arrays and initialize its prop instances. */
void ae_AreaLoadGridAndProps(AeRam *r, const AeStageHooks *h) {
    uint32_t state = AE_STAGE_G_STATE;
    uint32_t header = ae_rd32(r, state + AE_STAGE_STATE_AREA_HEADER);
    uint32_t grid_data = ae_rd32(r, state + AE_STAGE_STATE_AREA_GRID);
    uint32_t level_grid = AE_STAGE_G_LEVEL_GRID;
    uint16_t width = ae_rd16(r, grid_data + AE_STAGE_LEVEL_GRID_WIDTH);
    uint16_t depth = ae_rd16(r, grid_data + AE_STAGE_LEVEL_GRID_DEPTH);
    ae_wr16(r, level_grid + AE_STAGE_LEVEL_GRID_WIDTH, width);
    ae_wr16(r, level_grid + AE_STAGE_LEVEL_GRID_DEPTH, depth);

    if ((ae_rd32(r, header) & 2u) == 0) {
        uint32_t color_count = ae_rd32(r, grid_data + AE_STAGE_LEVEL_GRID_COLOR_COUNT);
        uint32_t cell_count = (uint32_t)width * depth;
        uint32_t cells = grid_data + AE_STAGE_GRID_CELL_DATA_OFFSET;
        uint32_t colors = cells + cell_count * 4u;
        uint32_t cell_tiles = colors + color_count * 4u;
        uint32_t heights = cell_tiles + cell_count * 8u;
        ae_wr32(r, level_grid + AE_STAGE_LEVEL_GRID_COLOR_COUNT, color_count);
        ae_wr32(r, level_grid + AE_STAGE_LEVEL_GRID_CELLS, cells);
        ae_wr32(r, level_grid + AE_STAGE_LEVEL_GRID_COLORS, colors);
        ae_wr32(r, level_grid + AE_STAGE_LEVEL_GRID_CELL_TILES, cell_tiles);
        ae_wr32(r, level_grid + AE_STAGE_LEVEL_GRID_HEIGHTS, heights);
    }

    int32_t prop_count = (int32_t)ae_rd32(r, state + AE_STAGE_STATE_PROP_COUNT);
    uint32_t prop_bytes = (uint32_t)prop_count * AE_STAGE_AREA_PROP_SIZE;
    uint32_t props = (uint32_t)STAGE_CALL(AreaBumpAlloc, prop_bytes, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    ae_wr32(r, AE_STAGE_G_AREA_PROPS, props);
    if (prop_count <= 0) return;

    uint32_t mesh_table = ae_rd32(r, state + AE_STAGE_STATE_MESH_TABLE);
    uint32_t placements = ae_rd32(r, state + AE_STAGE_STATE_PROP_PLACEMENTS);
    uint32_t rotation_count = 0;
    uint32_t rotation_matrices = AE_STAGE_G_ROTATION_MATRICES;
    for (int32_t index = 0; index < prop_count; ++index) {
        uint32_t placement = placements + (uint32_t)index * AE_STAGE_PROP_PLACEMENT_SIZE;
        uint32_t prop = props + (uint32_t)index * AE_STAGE_AREA_PROP_SIZE;
        uint32_t mesh_id = ae_rd16(r, placement);
        uint32_t mesh_record = mesh_table + mesh_id * AE_STAGE_MESH_TABLE_ENTRY_SIZE;
        uint32_t mesh_index = ae_rd16(r, mesh_record + AE_STAGE_MESH_INDEX_FIELD);
        uint32_t mesh_entry = mesh_table + mesh_index * AE_STAGE_MESH_TABLE_ENTRY_SIZE;

        STAGE_CALL(MeshInit, prop, mesh_entry, 0, 0);
        if (ae_ram_failed(r)) return;
        ae_wr16(r, prop + AE_STAGE_PROP_POSITION,
                ae_rd16(r, placement + AE_STAGE_PROP_X));
        ae_wr16(r, prop + AE_STAGE_PROP_POSITION + 2u,
                ae_rd16(r, placement + AE_STAGE_PROP_Y));
        ae_wr16(r, prop + AE_STAGE_PROP_POSITION + 4u,
                ae_rd16(r, placement + AE_STAGE_PROP_Z));

        uint32_t mesh = ae_rd32(r, prop + AE_STAGE_PROP_MESH);
        uint8_t mesh_type = ae_rd8(r, mesh);
        uint16_t zone_mask;
        int32_t position_z = ae_rds16(r, placement + AE_STAGE_PROP_Z);
        if (mesh_type < 4u || mesh_type >= 6u) {
            zone_mask = (uint16_t)STAGE_CALL(
                GetZoneMask,
                (uint32_t)(int32_t)ae_rds16(r, placement + AE_STAGE_PROP_X),
                (uint32_t)(int32_t)ae_rds16(r, placement + AE_STAGE_PROP_Y),
                (uint32_t)position_z,
                0u);
            if (ae_ram_failed(r)) return;
        } else {
            zone_mask = 0xFFFFu;
        }
        ae_wr16(r, prop + AE_STAGE_PROP_ZONE_MASK, zone_mask);

        if ((ae_rd32(r, mesh) & 0x0Fu) == 3u) {
            uint32_t matrix = rotation_matrices + rotation_count * AE_STAGE_ROTATION_MATRIX_SIZE;
            ae_sdk_MatrixIdentity(r, matrix);
            ae_wr32(r, prop + AE_STAGE_PROP_ROTATION, matrix);
            STAGE_CALL(FUN_80030890,
                       ae_rd16(r, placement + AE_STAGE_PROP_ROTATION_FIELD), matrix,
                       (uint32_t)position_z, 0);
            if (ae_ram_failed(r)) return;
            ++rotation_count;
            if (rotation_count >= AE_STAGE_ROTATION_LIMIT) {
                STAGE_CALL(Printf, AE_STAGE_MODEL_OVER_MESSAGE, matrix,
                           (uint32_t)position_z, 0);
                if (ae_ram_failed(r)) return;
                STAGE_CALL(FUN_800A3700, 1u, 0, 0, 0);
                if (ae_ram_failed(r)) return;
            }
        } else {
            ae_wr32(r, prop + AE_STAGE_PROP_ROTATION,
                    ae_rd16(r, placement + AE_STAGE_PROP_ROTATION_FIELD));
        }

        ae_wr32(r, mesh, ae_rd32(r, mesh) & ~0xE000u);
    }
}

/* 8005D100: dispatch a requested stage transition by its low-byte action code. */
int32_t ae_StageRequest(AeRam *r, const AeStageHooks *h,
                        int32_t code, int32_t area, int32_t entry) {
    uint32_t state = AE_STAGE_G_STATE;
    AeStageHooks nested_storage;
    const AeStageHooks *nested_hooks =
        stage_nested_hooks(h, AE_STAGE_STAGE_REQUEST_FRAME, &nested_storage);
    uint32_t action = (uint32_t)code & 0xFFu;
    uint32_t current_area = ae_rd16(r, state + AE_STAGE_STATE_CURRENT_AREA);

    uint32_t active_transition = ae_rd8(r, state + AE_STAGE_STATE_PROGRESS_TYPE);
    if (active_transition == 32u || active_transition == 34u) return 0;
    ae_wr16(r, state + AE_STAGE_STATE_PROGRESS_TYPE, (uint32_t)code);
    if (action == 0u || action > 34u) return 0;
    switch (action) {
    case 1: {
        ae_wr32(r, AE_STAGE_G_ARC_LOAD_STATE + AE_STAGE_ARC_LOAD_STATUS, UINT32_MAX);
        ae_wr32(r, AE_STAGE_G_ARC_LOAD_STATE + AE_STAGE_ARC_LOAD_READ_STATUS, 0u);
        ae_wr32(r, AE_STAGE_G_ARC_LOAD_STATE + AE_STAGE_ARC_LOAD_LAST_STATUS, UINT32_MAX);
        STAGE_CALL(CutsceneModeOff, 0, 0, 0, 0);
        ae_wr16(r, AE_STAGE_G_CUTSCENE_MODE_FLAG, 0u);
        ae_wr32(r, state + AE_STAGE_STATE_AREA_INIT_FLAG, 0u);
        if (current_area != AE_STAGE_AREA_PLAYER_SETUP_SKIP) {
            ae_StageProgressOnEnter(r, h, area);
            if (ae_ram_failed(r)) return 0;
            STAGE_CALL(PlayerInit, 0, 0, 0, 0);
            if (ae_ram_failed(r)) return 0;
        }
        ae_AreaLoad(r, nested_hooks, area, entry);
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_MODE, 1u);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_STEP, 48u);
        if (ae_rd16(r, state + AE_STAGE_STATE_AREA_KIND) == 3u ||
            ae_rd16(r, state + AE_STAGE_STATE_CURRENT_AREA) == 78u ||
            ae_rd16(r, state + AE_STAGE_STATE_CURRENT_AREA) == AE_STAGE_AREA_PLAYER_SETUP_SKIP ||
            ae_rd16(r, state + AE_STAGE_STATE_LEVEL) == AE_STAGE_LEVEL_HUB) {
            return 0;
        }
        STAGE_CALL(PlayerSetMode, 12u, 0, 0, 0);
        return 0;
    }
    case 2: {
        uint32_t request_copy = stage_frame_sp(h, AE_STAGE_STAGE_REQUEST_FRAME) +
                                AE_STAGE_STACK_LOCAL_OFFSET;
        for (uint32_t word = 0; word < 2u; ++word) {
            ae_wr32(r, request_copy + word * 4u,
                    ae_rd32(r, AE_STAGE_REQUEST_AREA_COPY_SOURCE + word * 4u));
        }
        STAGE_CALL(FUN_80028850, request_copy, 0u, 0u, 0u);
        STAGE_CALL(FUN_800286CC, 0u, 0u, 0u, 0u);
        STAGE_CALL(CutsceneModeOn, 0, 0, 0, 0);
        ae_StageProgressRestore(r, h, area);
        if (ae_ram_failed(r)) return 0;
        if (area == 53 || area == 24) STAGE_CALL(SoundStop, 2u, 0, 0, 0);
        int32_t preserve_music = STAGE_CALL(SoundAreaKeepsMusic, (uint32_t)area, 0, 0, 0);
        if (ae_ram_failed(r)) return 0;
        if (preserve_music != 0) STAGE_CALL(SoundStop, 3u, 0, 0, 0);
        if ((uint32_t)area - 54u < 2u || area == AE_STAGE_AREA_STAGE_SPECIAL_A ||
            area == AE_STAGE_AREA_STAGE_SPECIAL_B) {
            STAGE_CALL(BgmUpdateVolume, 0, 0, 0, 0);
        }
        ae_AreaLoad(r, nested_hooks, area, entry);
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_MODE, 4u);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_STEP, 24u);
        STAGE_CALL(PlaySfx, 123u, 0, 0, 0);
        return 0;
    }
    case 3:
    case 12:
        ae_wr32(r, AE_STAGE_G_ARC_LOAD_STATE + AE_STAGE_ARC_LOAD_STATUS, UINT32_MAX);
        ae_wr32(r, AE_STAGE_G_ARC_LOAD_STATE + AE_STAGE_ARC_LOAD_READ_STATUS, 0u);
        ae_wr32(r, AE_STAGE_G_ARC_LOAD_STATE + AE_STAGE_ARC_LOAD_LAST_STATUS, UINT32_MAX);
        return 0;
    case 4:
        ae_wr32(r, AE_STAGE_G_AREA_STREAM_RETRY, UINT32_MAX);
        ae_AreaLoadStart(r, nested_hooks, area);
        return 0;
    case 6: {
        STAGE_CALL(CutsceneModeOff, 0, 0, 0, 0);
        ae_StageProgressOnExit(r, h, area);
        if (ae_ram_failed(r)) return 0;
        int32_t alternate = STAGE_CALL(AreaUsesAltMember, current_area,
                                       (int16_t)area, 0, 0);
        if (ae_ram_failed(r)) return 0;
        if (alternate != 0) STAGE_CALL(SoundStop, 1u, 0, 0, 0);
        STAGE_CALL(PlaySfx, 123u, 0, 0, 0);
        ae_AreaLoadQueueRead(r, nested_hooks);
        if (ae_ram_failed(r)) return 0;
        if (ae_rd16(r, state + AE_STAGE_STATE_AREA_RESET_A) == 1u) {
            uint8_t mode_flag = ae_rd8(r, AE_STAGE_G_AREA_CHANGE_FLAGS +
                                         AE_STAGE_G_STAGE_REQUEST_MODE_FLAG_OFFSET);
            ae_wr8(r, AE_STAGE_G_AREA_CHANGE_FLAGS +
                       AE_STAGE_G_STAGE_REQUEST_AREA_FLAG_OFFSET, mode_flag);
        }
        STAGE_CALL(WaitFrameFlip, 0, 0, 0, 0);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_STATUS, 1u);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_ARGUMENT,
                ae_rd16(r, AE_STAGE_G_ARC_LOAD_STATE + AE_STAGE_ARC_LOAD_PARAMETER));
        return 0;
    }
    case 7:
    case 21:
    case 23:
    case 27:
    case 28:
    case 30: {
        if (action == 27u) {
            STAGE_CALL(ProgressSetByte, 10u, 5u, 0, 0);
        }
        if (action == 7u || action == 27u) {
            ae_StageProgressStoreArea(r, h, area);
            if (ae_ram_failed(r)) return 0;
        }
        if (action == 27u) {
            STAGE_CALL(PlayerSetMode, 11u, 0, 0, 0);
        } else {
            STAGE_CALL(PlayerSetMode, 13u, 0, 0, 0);
            ae_wr32(r, state + AE_STAGE_STATE_TRANSITION_PROGRESS, 6u);
        }
        STAGE_CALL(BgmFadeOut, 0, 0, 0, 0);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_MODE, 2u);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_STEP, 0u);
        STAGE_CALL(AimRequestClear, 0, 0, 0, 0);
        STAGE_CALL(AimModeLeave, 0, 0, 0, 0);
        return 0;
    }
    case 8:
        ae_wr32(r, AE_STAGE_G_STAGE_REQUEST_TEMP_VECTOR_A, 0u);
        ae_wr32(r, AE_STAGE_G_STAGE_REQUEST_TEMP_VECTOR_B, 0u);
        ae_wr32(r, AE_STAGE_G_STAGE_REQUEST_TEMP_VECTOR_C, 0u);
        if (ae_rd16(r, state + AE_STAGE_STATE_LEVEL) == 33u) {
            ae_StageRequest(r, nested_hooks, 32, (int32_t)current_area,
                            ae_rd16(r, state + AE_STAGE_STATE_AREA_ENTRY));
            return 0;
        } else {
            STAGE_CALL(FUN_80096638,
                       ae_rd16(r, state + AE_STAGE_STATE_LEVEL), 0, 0, 0);
        }
        ae_StageProgressStoreArea(r, h, area);
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_MODE, 2u);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_STEP, 0u);
        return 0;
    case 9: {
        STAGE_CALL(BgmFadeOut, 0, 0, 0, 0);
        uint32_t count = (uint16_t)(ae_rd16(r, state + AE_STAGE_STATE_TIME_STATION_COUNTER) + 1u);
        if (count > 255u) count = 255u;
        ae_wr16(r, state + AE_STAGE_STATE_TIME_STATION_COUNTER, count);
        if (ae_rd16(r, state + AE_STAGE_STATE_LEVEL) == AE_STAGE_LEVEL_HUB) {
            ae_StageRequest(r, nested_hooks, 32, (int32_t)current_area,
                            ae_rd16(r, state + AE_STAGE_STATE_AREA_ENTRY));
            return 0;
        }
        ae_StageProgressStoreArea(r, h, area);
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_MODE, 2u);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_STEP, 0u);
        return 0;
    }
    case 10: {
        STAGE_CALL(CutsceneModeOff, 0, 0, 0, 0);
        if (ae_rd16(r, state + AE_STAGE_STATE_LEVEL) == AE_STAGE_LEVEL_HUB) {
            ae_StageRequest(r, nested_hooks, 32, (int32_t)current_area,
                            ae_rd16(r, state + AE_STAGE_STATE_AREA_ENTRY));
            return 0;
        }
        uint16_t area_kind = ae_rd16(r, state + AE_STAGE_STATE_AREA_KIND);
        uint16_t level = ae_rd16(r, state + AE_STAGE_STATE_LEVEL);
        if (area_kind == 3u) {
            ae_StageRequest(r, nested_hooks, 26, (int32_t)current_area,
                            ae_rd16(r, state + AE_STAGE_STATE_AREA_ENTRY));
            return 0;
        }
        if (ae_rd16(r, state + AE_STAGE_STATE_MODE) == 19u) {
            STAGE_CALL(PlayTimeAdd, 10u, 0, 0, 0);
        } else if ((uint32_t)level - AE_STAGE_LEVEL_CHALLENGE_A >= 2u) {
            uint16_t remaining = ae_rd16(r, state + AE_STAGE_STATE_AREA_TRANSITION_COUNT);
            ae_wr16(r, state + AE_STAGE_STATE_AREA_TRANSITION_COUNT,
                    (uint16_t)(remaining - 1u));
        }
        ae_wr8(r, AE_STAGE_G_STAGE_REQUEST_AREA_FLAG, 1u);
        ae_wr8(r, AE_STAGE_G_STAGE_REQUEST_MODE_FLAG, 1u);
        uint16_t remaining = ae_rd16(r, state + AE_STAGE_STATE_AREA_TRANSITION_COUNT);
        if (remaining == 0u) {
            ae_StageRequest(r, nested_hooks, 9, (int32_t)current_area,
                            ae_rd16(r, state + AE_STAGE_STATE_AREA_ENTRY));
        } else {
            ae_wr32(r, AE_STAGE_G_AREA_STREAM_RETRY + AE_STAGE_ARC_LOAD_READ_STATUS, 0u);
            ae_StageRequest(r, nested_hooks, 11, (int32_t)current_area,
                            ae_rd16(r, state + AE_STAGE_STATE_AREA_ENTRY));
        }
        return 0;
    }
    case 11:
        STAGE_CALL(CutsceneModeOff, 0, 0, 0, 0);
        ae_wr32(r, AE_STAGE_G_ARC_LOAD_STATE + AE_STAGE_ARC_LOAD_STATUS,
                UINT32_MAX);
        ae_wr32(r, AE_STAGE_G_ARC_LOAD_STATE + AE_STAGE_ARC_LOAD_READ_STATUS,
                0u);
        ae_wr32(r, AE_STAGE_G_ARC_LOAD_STATE + AE_STAGE_ARC_LOAD_LAST_STATUS,
                UINT32_MAX);
        ae_StageProgressOnExit(r, h, area);
        if (ae_ram_failed(r)) return 0;
        STAGE_CALL(BgmFadeOut, 0, 0, 0, 0);
        ae_wr16(r, AE_STAGE_G_STAGE_REQUEST_FLAGS, 0u);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_MODE, 3u);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_STEP, 0u);
        ae_AreaLoadStart(r, nested_hooks, area);
        return 0;
    case 13: {
        STAGE_CALL(CutsceneModeOn, 0, 0, 0, 0);
        ae_StageProgressRestore(r, h, area);
        if (ae_ram_failed(r)) return 0;
        if (area == 53 || area == 24) STAGE_CALL(SoundStop, 2u, 0, 0, 0);
        if (area != 53 && area != 24 && area != 87 && area != 72) {
            STAGE_CALL(BgmUpdateVolume, 0, 0, 0, 0);
        }
        STAGE_CALL(PlayerInit, 0, 0, 0, 0);
        ae_AreaLoad(r, nested_hooks, area, entry);
        if (ae_ram_failed(r)) return 0;
        ae_wr8(r, AE_STAGE_G_STAGE_REQUEST_AREA_FLAG, 0u);
        ae_wr8(r, AE_STAGE_G_STAGE_REQUEST_MODE_FLAG, 0u);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_MODE, 1u);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_STEP, 48u);
        return 0;
    }
    case 16:
        ae_wr16(r, state + AE_STAGE_STATE_AREA_PROGRESS,
                (uint16_t)(ae_rd16(r, state + AE_STAGE_STATE_AREA_PROGRESS) + 1u));
        STAGE_CALL(PlayerSetMode, 8u, 0, 0, 0);
        return 0;
    case 17:
        ae_StageRequest(r, nested_hooks, 3, (int32_t)current_area,
                        ae_rd16(r, state + AE_STAGE_STATE_AREA_ENTRY));
        return 0;
    case 18:
        STAGE_CALL(CutsceneModeOff, 0, 0, 0, 0);
        STAGE_CALL(StageGradeUpdate, 0, 0, 0, 0);
        STAGE_CALL(StageVictoryStart, 0, 0, 0, 0);
        return 0;
    case 19:
        STAGE_CALL(PlayerSetMode, 9u, 0, 0, 0);
        STAGE_CALL(StageIntroStart, 0, 0, 0, 0);
        STAGE_CALL(XaPlay, 57u, 0, 0, 0);
        return 0;
    case 20:
        STAGE_CALL(PlayerEnterCutsceneMode, 0, 0, 0, 0);
        return 0;
    case 22:
        ae_wr32(r, state + AE_STAGE_STATE_TRANSITION_PROGRESS, UINT32_MAX);
        ae_wr32(r, AE_STAGE_G_STAGE_REQUEST_TEMP_VECTOR_A, 0u);
        ae_wr32(r, AE_STAGE_G_STAGE_REQUEST_TEMP_VECTOR_B, 0u);
        ae_wr32(r, AE_STAGE_G_STAGE_REQUEST_TEMP_VECTOR_C, 0u);
        STAGE_CALL(BgmFadeOut, 0, 0, 0, 0);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_MODE, 2u);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_STEP, 0u);
        return 0;
    case 24:
        ae_StageRequestExit(r, h);
        return 0;
    case 25:
        STAGE_CALL(StageTypeClearInit, 0, 0, 0, 0);
        return 0;
    case 26:
    case 31:
    case 32:
    case 34:
        STAGE_CALL(BgmFadeOut, 0, 0, 0, 0);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_MODE, 2u);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_STEP, 0u);
        STAGE_CALL(AimRequestClear, 0, 0, 0, 0);
        STAGE_CALL(AimModeLeave, 0, 0, 0, 0);
        if (action == 32u || action == 34u) {
            STAGE_CALL(PlayerSetMode, 11u, 0, 0, 0);
        }
        return 0;
    case 29:
        STAGE_CALL(StageGradeUpdate, 0, 0, 0, 0);
        STAGE_CALL(FUN_80096638, ae_rd16(r, state + AE_STAGE_STATE_LEVEL), 0, 0, 0);
        ae_wr32(r, state + AE_STAGE_STATE_TRANSITION_PROGRESS, UINT32_MAX);
        ae_wr32(r, AE_STAGE_G_STAGE_REQUEST_TEMP_VECTOR_A, 0u);
        ae_wr32(r, AE_STAGE_G_STAGE_REQUEST_TEMP_VECTOR_B, 0u);
        ae_wr32(r, AE_STAGE_G_STAGE_REQUEST_TEMP_VECTOR_C, 0u);
        STAGE_CALL(BgmFadeOut, 0, 0, 0, 0);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_MODE, 2u);
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_STEP, 0u);
        return 0;
    case 33:
        STAGE_CALL(FUN_8013EA24, 0, 0, 0, 0);
        ae_wr32(r, state + AE_STAGE_STATE_TRANSITION_PROGRESS, 1u);
        return 0;
    default:
        return 0;
    }
}

/* 800955C8: reissue the current area's stage request with its live entry. */
int32_t ae_StageRequestRetry(AeRam *r, const AeStageHooks *h) {
    uint32_t state = AE_STAGE_G_STATE;
    AeStageHooks nested_storage;
    const AeStageHooks *nested_hooks =
        stage_nested_hooks(h, AE_STAGE_STAGE_REQUEST_RETRY_FRAME, &nested_storage);
    int32_t area = ae_rd16(r, state + AE_STAGE_STATE_CURRENT_AREA);
    int32_t entry = ae_rd16(r, state + AE_STAGE_STATE_AREA_ENTRY);
    ae_StageRequest(r, nested_hooks, 8, area, entry);
    return 0;
}

/* 8005D890: advance the current transition and route exit/load-list results. */
int32_t ae_StageRequestUpdate(AeRam *r, const AeStageHooks *h) {
    uint32_t state = AE_STAGE_G_STATE;
    uint32_t load_state = AE_STAGE_G_ARC_LOAD_STATE;
    AeStageHooks nested_storage;
    const AeStageHooks *nested_hooks =
        stage_nested_hooks(h, AE_STAGE_STAGE_REQUEST_UPDATE_FRAME, &nested_storage);
    uint8_t initial_type = ae_rd8(r, state + AE_STAGE_STATE_PROGRESS_TYPE);
    if ((uint32_t)initial_type - 3u < 3u) {
        uint16_t level = ae_rd16(r, AE_STAGE_G_TRANSITION_CHECK_LEVEL);
        if (level == 35u || level == 74u || level == 45u) {
            ae_StageRequest(r, nested_hooks, 10,
                            ae_rd16(r, state + AE_STAGE_STATE_CURRENT_AREA),
                            ae_rd16(r, state + AE_STAGE_STATE_AREA_ENTRY));
            if (ae_ram_failed(r)) return 0;
        }
    }

    STAGE_CALL(CdMgrUpdate, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return 0;
    uint16_t transition_type = ae_rd16(r, state + AE_STAGE_STATE_PROGRESS_TYPE);
    if ((transition_type & 0xC000u) != 0u && ae_rd32(r, load_state) == 0u) {
        ae_wr32(r, load_state + AE_STAGE_ARC_LOAD_STATUS, UINT32_MAX);
        ae_wr32(r, load_state + AE_STAGE_ARC_LOAD_LAST_STATUS, UINT32_MAX);
        ae_wr32(r, load_state + AE_STAGE_ARC_LOAD_READ_STATUS, 0u);
        ae_wr16(r, state + AE_STAGE_STATE_PROGRESS_TYPE, transition_type & 0x7FFFu);
    }

    transition_type = ae_rd16(r, state + AE_STAGE_STATE_PROGRESS_TYPE);
    uint32_t transition_flags = transition_type & 0x6000u;
    uint32_t action = transition_type & 0xFFu;
    if (action == 0u || action > 33u) return 0;

    switch (action) {
    case 3: {
        int32_t exit_info = STAGE_CALL(AreaCheckExits, 0u, 0, 0, 0);
        if (ae_ram_failed(r)) return 0;
        if (exit_info < 0) {
            ae_wr32(r, AE_STAGE_G_AREA_EXIT_RETRY_MARKER, UINT32_MAX);
            return 0;
        }
        uint32_t destination_area = (uint32_t)exit_info >> 8;
        if ((int32_t)ae_rd32(r, load_state + AE_STAGE_ARC_LOAD_STATUS) >= 0) return 0;
        ae_wr32(r, load_state + AE_STAGE_ARC_LOAD_STATUS, 0u);
        ae_wr32(r, AE_STAGE_G_AREA_EXIT_RECORD, (uint32_t)exit_info);
        int32_t seek_result = ae_AreaLoadStartSeek(r, nested_hooks,
                                                   (int32_t)destination_area);
        if (ae_ram_failed(r)) return 0;
        int32_t next_action = ae_rd32(r, AE_STAGE_G_AREA_EXIT_LAST_RESULT) ==
                              (uint32_t)seek_result ? 5 : 4;
        ae_StageRequest(r, nested_hooks, (int32_t)(transition_flags | (uint32_t)next_action),
                        (int32_t)destination_area, exit_info & 0xFF);
        return 0;
    }
    case 4: {
        int32_t exit_info = STAGE_CALL(AreaCheckExits, 0u, 0, 0, 0);
        if (ae_ram_failed(r)) return 0;
        if (exit_info < 0) {
            ae_wr32(r, AE_STAGE_G_AREA_EXIT_RETRY_MARKER, UINT32_MAX);
            return 0;
        }
        uint32_t destination_area = (uint32_t)exit_info >> 8;
        if (ae_rd32(r, AE_STAGE_G_AREA_EXIT_RECORD) != (uint32_t)exit_info) {
            ae_wr32(r, AE_STAGE_G_AREA_EXIT_RETRY_MARKER, UINT32_MAX);
            ae_StageRequest(r, nested_hooks, (int32_t)(transition_flags | 3u),
                            (int32_t)destination_area, exit_info & 0xFF);
            return 0;
        }

        int32_t secondary_exit = STAGE_CALL(AreaCheckExits, 1u, 0, 0, 0);
        if (ae_ram_failed(r)) return 0;
        if (secondary_exit >= 0) {
            ae_wr32(r, state + AE_STAGE_STATE_TRANSITION_PROGRESS, UINT32_MAX);
            ae_wr32(r, AE_STAGE_G_STAGE_REQUEST_TEMP_VECTOR_A, 0u);
            ae_wr32(r, AE_STAGE_G_STAGE_REQUEST_TEMP_VECTOR_B, 0u);
            ae_wr32(r, AE_STAGE_G_STAGE_REQUEST_TEMP_VECTOR_C, 0u);
        }
        if (ae_rd32(r, load_state) != 0u) return 0;
        uint32_t candidate_index = ae_rd32(r, load_state + AE_STAGE_ARC_LOAD_NEXT_LIST_INDEX);
        uint32_t current_index = ae_rd32(r, load_state + AE_STAGE_ARC_LOAD_RETRY_INDEX);
        if (candidate_index != current_index) {
            ae_wr32(r, load_state + AE_STAGE_ARC_LOAD_STATUS, 0u);
            ae_StageRequest(r, nested_hooks, (int32_t)(transition_flags | 4u),
                            (int32_t)destination_area, exit_info & 0xFF);
            return 0;
        }
        int32_t seek_result = ae_AreaLoadStartSeek(r, nested_hooks,
                                                   (int32_t)destination_area);
        if (ae_ram_failed(r)) return 0;
        ae_wr32(r, AE_STAGE_G_AREA_EXIT_LAST_RESULT, (uint32_t)seek_result);
        ae_StageRequest(r, nested_hooks, (int32_t)(transition_flags | 5u),
                        (int32_t)destination_area, exit_info & 0xFF);
        return 0;
    }
    case 5: {
        int32_t exit_info = STAGE_CALL(AreaCheckExits, 0u, 0, 0, 0);
        if (ae_ram_failed(r)) return 0;
        if (exit_info < 0) {
            ae_wr32(r, AE_STAGE_G_AREA_EXIT_RETRY_MARKER, UINT32_MAX);
            return 0;
        }
        uint32_t destination_area = (uint32_t)exit_info >> 8;
        if (ae_rd32(r, AE_STAGE_G_AREA_EXIT_RECORD) != (uint32_t)exit_info) {
            ae_wr32(r, AE_STAGE_G_AREA_EXIT_RETRY_MARKER, UINT32_MAX);
            ae_StageRequest(r, nested_hooks, (int32_t)(transition_flags | 3u),
                            (int32_t)destination_area, exit_info & 0xFF);
            return 0;
        }
        int32_t secondary_exit = STAGE_CALL(AreaCheckExits, 1u, 0, 0, 0);
        if (ae_ram_failed(r) || secondary_exit < 0) return 0;
        ae_wr32(r, AE_STAGE_G_AREA_EXIT_INPUT, (uint32_t)secondary_exit);
        ae_StageRequest(r, nested_hooks, 6,
                        (int32_t)((uint32_t)secondary_exit >> 8),
                        secondary_exit & 0xFF);
        return 0;
    }
    case 6: {
        ae_wr16(r, state + AE_STAGE_STATE_TRANSITION_STEP,
                (uint16_t)(ae_rd16(r, state + AE_STAGE_STATE_TRANSITION_STEP) + 1u));
        STAGE_CALL(FadeFrame, 0, 0, 0, 0);
        if (ae_ram_failed(r)) return 0;
        int32_t list_status = ae_rds32(r, load_state + AE_STAGE_ARC_LOAD_READ_STATUS);
        if (list_status < 0) return 0;
        if (list_status < 2) {
            int32_t step_result = STAGE_CALL(LoadListStep, 0, 0, 0, 0);
            if (ae_ram_failed(r)) return 0;
            if (step_result >= 0) return 0;
            ae_wr32(r, load_state + AE_STAGE_ARC_LOAD_READ_STATUS, 2u);
            return 0;
        }
        if (list_status == 2) {
            uint32_t next_exit = ae_rd32(r, AE_STAGE_G_AREA_EXIT_INPUT);
            ae_StageRequest(r, nested_hooks, 2, (int32_t)(next_exit >> 8),
                            (int32_t)(next_exit & 0xFFu));
            ae_wr32(r, load_state + AE_STAGE_ARC_LOAD_STATUS, UINT32_MAX);
            ae_wr32(r, load_state + AE_STAGE_ARC_LOAD_READ_STATUS, 0u);
            ae_wr32(r, load_state + AE_STAGE_ARC_LOAD_LAST_STATUS, UINT32_MAX);
        }
        return 0;
    }
    case 18:
        STAGE_CALL(GimmickNopUpdate4, 0, 0, 0, 0);
        return 0;
    case 19:
        STAGE_CALL(StageIntroUpdate, 0, 0, 0, 0);
        return 0;
    case 20: {
        uint32_t flags = ae_rd32(r, AE_STAGE_G_STAGE_REQUEST_EXTENDED_FLAGS);
        ae_wr32(r, AE_STAGE_G_STAGE_REQUEST_EXTENDED_FLAGS, flags | 0x20u);
        if (ae_rd32(r, load_state) == 0u) ae_StageRequestRetry(r, nested_hooks);
        return 0;
    }
    case 24:
        STAGE_CALL(GimmickNopUpdate3, 0, 0, 0, 0);
        return 0;
    case 25:
        STAGE_CALL(StageTypeClearUpdate, 0, 0, 0, 0);
        return 0;
    default:
        return 0;
    }
}
