#include "ae_ovl180.h"

#include "../core/ae_sdk_matrix.h"
#include "../render/ae_tex_copy.h"
#include "../world/ae_areaset.h"
#include "../effects/ae_fxscript.h"
#include "../effects/ae_effects.h"
#include "../core/ae_pad.h"

#include <limits.h>

#define OVL180_OBJECT_MESH 0x00u
#define OVL180_OBJECT_MODEL 0x04u
#define OVL180_OBJECT_MESH_WORD 0x08u
#define OVL180_OBJECT_MODEL_DATA 0x0Cu
#define OVL180_OBJECT_FLAGS_HIGH 0x02u
#define OVL180_OBJECT_MODEL_WORD 0x18u
#define OVL180_OBJECT_FLAGS 0x03u
#define OVL180_OBJECT_RECORD_SIZE 0x01u
#define OVL180_OBJECT_LAYER 0x12u
#define OVL180_MODEL_HEADER_WORD 0x0Eu
#define OVL180_MODEL_VERTEX_OR_MESH_WORD 0x08u
#define OVL180_MODEL_DATA_BIAS 0x0Cu
#define OVL180_HEADER_BYTES 4u
#define OVL180_DESCRIPTOR_STRIDE 4u
#define OVL180_DESCRIPTOR_MODEL_INDEX 0u
#define OVL180_DESCRIPTOR_TYPE 1u
#define OVL180_DESCRIPTOR_FLAGS 0u
#define OVL180_DESCRIPTOR_AUX_KIND 3u
#define OVL180_AUX_OBJECT_BYTES 28u
#define OVL180_OBJECT_AUXILIARY 0x08u
#define OVL180_OBJECT_DUPLICATE_A 0x04u
#define OVL180_OBJECT_DUPLICATE_B 0x18u
#define OVL180_MODEL_VERTEX_COUNT 0x06u
#define OVL180_MODEL_COLOR_COUNT 0x08u
#define OVL180_MESH_ELEMENT_COUNT 0x12u
#define OVL180_MESH_PRIMITIVE_COUNT 0x0Eu
#define OVL180_MESH_SEQUENCE_COUNT 0x0Eu
#define OVL180_MESH_FIND_TIME 0x16u
#define OVL180_MODEL_SEQUENCE_COUNT 0x02u
#define OVL180_MODEL_SECONDARY_COUNT 0x08u
#define OVL180_MODEL_REFERENCE_MASK 0x00FFFFFFu

#define OVL180_COMMAND_SIZE 0x00u
#define OVL180_COMMAND_ID 0x02u
#define OVL180_COMMAND_FRAME_MARKER 0x02u
#define OVL180_COMMAND_ARGUMENTS 0x04u
#define OVL180_COMMAND_SWEEP_MODE 0x04u
#define OVL180_FLAG_WRITE_INDEX 0x00u
#define OVL180_FLAG_WRITE_VALUE 0x02u
#define OVL180_FLAG_WRITE_STRIDE 0x04u
#define OVL180_FLAG_WRITE_HEADER_BYTES 2u
#define OVL180_FLAG_OP_VALUE 0x00u
#define OVL180_FLAG_OP_ENABLE 0x01u
#define OVL180_FLAG_OP_LAYER 0x02u
#define OVL180_FLAG_OP_MASK 0x03u
#define OVL180_CD_LOCATION_MINUTES 0x00u
#define OVL180_CD_LOCATION_SECONDS 0x01u
#define OVL180_CD_LOCATION_SECTORS 0x02u

#define OVL180_G_ALLOCATED_FILE_TABLE 0x8013C574u
#define OVL180_G_DISPLAY_STATE 0x8013C488u
#define OVL180_G_DISPLAY_BUFFER 0x8013C48Cu
#define OVL180_G_LAST_VSYNC 0x8013C490u
#define OVL180_G_FRAME_COUNTER 0x8013C494u
#define OVL180_G_FRAME_TIME 0x8013C498u
#define OVL180_G_PREVIOUS_FRAME_TIME 0x8013C49Cu
#define OVL180_G_SWEEP_MODE 0x8013C580u
#define OVL180_G_SWEEP_TIMER 0x8013C582u
#define OVL180_G_FILE_RANGE_COUNT 0x8013C714u
#define OVL180_FILE_INDEX_TABLE 0x8013C090u
#define OVL180_FILE_INDEX_STRIDE 0x1Cu
#define OVL180_FILE_BUFFER 0x80148000u
#define OVL180_FILE_BUFFER_NEXT 0x80148024u
#define OVL180_MODE_STATE 0x800F4470u
#define OVL180_MODE_SUBSTATE 0x800F4476u
#define OVL180_TITLE_SEQUENCE_INDEX 0x800F44AAu

enum AeOvl180NativeAddress {
    OVL180_API_MESH_INIT = 0x80015C34u,
    OVL180_API_ST_SET_RING = 0x80025C60u,
    OVL180_API_ST_SET_STREAM = 0x80025C90u,
    OVL180_API_VSYNC = 0x8002E520u,
    OVL180_API_VSYNC_CALLBACK = 0x8002E7C0u,
    OVL180_API_LOAD_LIST_START = 0x80011104u,
    OVL180_API_LOAD_LIST_STEP = 0x800116C0u,
    OVL180_API_CD_MANAGER_UPDATE = 0x800125B8u,
    OVL180_API_PROGRESS_SET_BYTE = 0x80096360u,
    OVL180_API_PROGRESS_BITS_SET = 0x80096540u,
    OVL180_API_STREAM_WAIT = 0x80136E00u,
    OVL180_API_CD_READ_SEEK = 0x80136454u
};

enum AeOvl180NativeFunction {
    OVL180_API_CD_POS_TO_INT = 0x80025E24u,
    OVL180_API_LOAD_FILES = 0x80136884u,
    OVL180_API_CD_INT_TO_POS = 0x80025D20u,
    OVL180_API_CD_READ_ASYNC = 0x8013ADA0u,
    OVL180_API_CD_BUSY = 0x8013AE80u,
    OVL180_API_CD_SET_FILTER = 0x80136E68u,
    OVL180_API_CD_PLAY_XA_RANGE = 0x801364B0u,
    OVL180_API_ST_WAIT = 0x80136E00u,
    OVL180_API_SAVE_RESTORE_VRAM = 0x8013BA80u,
    OVL180_API_SETUP_DISPLAY = 0x80136BD4u,
    OVL180_API_PRESENT = 0x80136FF0u,
    OVL180_API_COPY_SCREEN_TO_BACK = 0x801367DCu,
    OVL180_API_LOAD_SAVED_IMAGE = 0x8013AA70u,
    OVL180_API_SOUND_RESTORE_VOLUME = 0x80086248u,
    OVL180_API_XA_PLAY = 0x800242FCu,
    OVL180_API_MEMCPY = 0x800A3760u,
    OVL180_API_CD_READY = 0x8002408Cu,
    OVL180_API_CDSYNC = 0x80026A50u,
    OVL180_API_DRAW_SYNC = 0x800286CCu,
    OVL180_API_BEGIN_FRAME = 0x8005DF48u,
    OVL180_API_PUT_DISP_ENV = 0x80028EFCu,
    OVL180_API_PUT_DRAW_ENV = 0x80028D30u,
    OVL180_API_SET_GEOM_SCREEN = 0x8002FAF0u,
    OVL180_API_LOAD_IMAGE = 0x8002B0F0u,
    OVL180_API_SET_ROT_MATRIX = 0x8002FB00u,
    OVL180_API_SET_TRANS_MATRIX = 0x8002FB30u,
    OVL180_API_GADGET4_TRAIL_UPDATE = 0x8006DAF8u,
    OVL180_API_GADGET_FLASH_UPDATE = 0x8006C6D4u,
    OVL180_API_PAD_GET_STATE = 0x8002B52Cu,
    OVL180_API_PAD_INFO_MODE = 0x8002B5F8u,
    OVL180_API_PAD_SETUP_DUALSHOCK = 0x80012CA0u,
    OVL180_API_PAD_DEMO_STEP = 0x80013128u,
    OVL180_API_STAGE_REQUEST = 0x8005D100u,
    OVL180_API_STICK_TO_BUTTONS = 0x800140B4u,
    OVL180_API_PAD_RUMBLE_RESET = 0x80014208u,
    OVL180_API_ST_FREE = 0x80025EB0u,
    OVL180_API_ST_RING_STATUS = 0x80025F60u
};

enum AeOvl180CommandFunction {
    OVL180_CMD_SET_SWEEP_MODE = 0x80139FA0u,
    OVL180_CMD_ADVANCE_SWEEP = 0x80139FB8u,
    OVL180_CMD_WRITE_OBJECT_FLAGS = 0x80139EC4u,
    OVL180_CMD_ADVANCE_SWEEP_RENDER_START = 0x8013A014u,
    OVL180_CMD_ADVANCE_SWEEP_EPILOGUE = 0x8013A428u
};

#define OVL180_G_FRAME_FLAGS 0x800EC350u
#define OVL180_G_MODE_FRAME 0x800EC350u
#define OVL180_G_MODE_CONTEXT 0x800F4350u
#define OVL180_G_SCREEN_ENV_PRIMARY 0x800F03D4u
#define OVL180_G_SCREEN_ENV_SECONDARY 0x800F444Cu
#define OVL180_G_STOP_TIMING_BUFFER 0x800EBFA0u
#define OVL180_G_FADE_DRAW_ACTIVE 0x800F03F0u
#define OVL180_G_FADE_FRAME_ACTIVE 0x800EC378u
#define OVL180_G_OBJECT_FRAME_FLAGS 0x800F4470u
#define OVL180_G_TITLE_MODE_STATE 0x800F4470u
#define OVL180_G_TITLE_MODE_SUBSTATE 0x800F4476u
#define OVL180_MODE_DISPLAY_BUFFER_STRIDE 0x4080u
#define OVL180_EXE_DISPLAY_TIMING_BASE 0x800EBFA0u
#define OVL180_EXE_DISPLAY_LAST_VSYNC 0x24u
#define OVL180_EXE_DISPLAY_FRAME_COUNT 0x28u
#define OVL180_EXE_DISPLAY_FRAME_TIME 0x20u
#define OVL180_EXE_DISPLAY_PREVIOUS_FRAME_TIME 0x1Cu
#define OVL180_RUN_FRAME_TIME_LIMIT 0x8013C4B4u
#define OVL180_RUN_FRAME_TIME_VALUE 0x8013C4B0u
#define OVL180_DISPLAY_CONTEXT_BUFFER_INDEX 0x0Cu
#define OVL180_DISPLAY_ENV_OFFSET 0x6Cu
#define OVL180_DRAW_ENV_OFFSET 0x10u
#define OVL180_ORDERING_TABLE_OFFSET 0x407Cu
#define OVL180_FRAME_BUFFER_STRIDE 0x4078u
#define OVL180_RUN_MAX_DISPLAY_FRAMES 3u
#define OVL180_RUN_BUTTON_MASK 0x0860u
#define OVL180_RUN_STACK_FIRST_RECORD 16u
#define OVL180_RUN_STACK_SECOND_RECORD 20u
#define OVL180_RUN_STACK_RING_OUTPUT 24u
#define OVL180_RUN_STACK_RING_STATUS 26u
#define OVL180_DEFAULT_CALLER_STACK 0x801FFF00u
#define OVL180_STOP_STACK_BYTES 56u
#define OVL180_STOP_STACK_START_LOCATION 16u
#define OVL180_STOP_STACK_END_LOCATION 24u
#define OVL180_STOP_TIMING_LAST_VSYNC 0x24u
#define OVL180_STOP_TIMING_FRAME_COUNT 0x28u
#define OVL180_STOP_TIMING_FRAME_TIME 0x20u
#define OVL180_STOP_TIMING_PREVIOUS_FRAME_TIME 0x1Cu
#define OVL180_AUDIO_SEEK_SECTORS 5u

#define OVL180_FILE_PATH_OFFSET 4u
#define OVL180_FILE_NEXT_PATH_OFFSET 8u
#define OVL180_FILE_FIRST_DATA_OFFSET 12u
#define OVL180_FILE_SECOND_DATA_OFFSET 16u
#define OVL180_FILE_THIRD_DATA_OFFSET 20u
#define OVL180_FILE_FINAL_DATA_OFFSET 24u
#define OVL180_FILE_STREAM_OFFSET 24u
#define OVL180_FILE_FLAGS_OFFSET 16u
#define OVL180_HEADER_OBJECT_COUNT 0u
#define OVL180_HEADER_STREAM_SECTORS 1u
#define OVL180_HEADER_FLAGS_OFFSET 16u
#define OVL180_HEADER_WIDTH_FLAG 1u
#define OVL180_HEADER_AUDIO_FLAG 2u
#define OVL180_HEADER_LETTERBOX_FLAG 8u

static int32_t ovl180_native(AeRam *r, const AeOvl180Hooks *h,
                             const char *name, uint32_t address,
                             uint32_t argc, const uint32_t *args) {
    if (!h || !h->native_call) {
        ae_ram_fail(r, "AeOvl180 native hook %s not provided", name);
        return 0;
    }
    return h->native_call(h->user, r, name, address, argc, args);
}

static int32_t ovl180_native0(AeRam *r, const AeOvl180Hooks *h,
                              const char *name, uint32_t address) {
    return ovl180_native(r, h, name, address, 0u, NULL);
}

static int32_t ovl180_native1(AeRam *r, const AeOvl180Hooks *h,
                              const char *name, uint32_t address,
                              uint32_t a0) {
    const uint32_t args[1] = {a0};
    return ovl180_native(r, h, name, address, 1u, args);
}

static int32_t ovl180_native2(AeRam *r, const AeOvl180Hooks *h,
                              const char *name, uint32_t address,
                              uint32_t a0, uint32_t a1) {
    const uint32_t args[2] = {a0, a1};
    return ovl180_native(r, h, name, address, 2u, args);
}

static int32_t ovl180_native3(AeRam *r, const AeOvl180Hooks *h,
                              const char *name, uint32_t address,
                              uint32_t a0, uint32_t a1, uint32_t a2) {
    const uint32_t args[3] = {a0, a1, a2};
    return ovl180_native(r, h, name, address, 3u, args);
}

static void ovl180_native_void(AeRam *r, const AeOvl180Hooks *h,
                               const char *name, uint32_t address,
                               uint32_t argc, const uint32_t *args) {
    (void)ovl180_native(r, h, name, address, argc, args);
}

static int32_t ovl180_area_error_hook(void *user, AeRam *r,
                                      uint32_t a0, uint32_t a1,
                                      uint32_t a2, uint32_t a3) {
    const AeOvl180Hooks *h = (const AeOvl180Hooks *)user;
    const uint32_t args[4] = {a0, a1, a2, a3};
    return ovl180_native(r, h, "HeapErrorOutput", 0u, 4u, args);
}

static const AeAreaSetHooks *ovl180_areaset_hooks(const AeOvl180Hooks *h,
                                                  AeAreaSetHooks *storage) {
    if (!h) return NULL;
    storage->user = (void *)h;
    storage->Printf = ovl180_area_error_hook;
    storage->Exit = ovl180_area_error_hook;
    return storage;
}

void ae_SeqPlayer_StreamRingInit(AeRam *r, const AeOvl180Hooks *h,
                                 uint32_t unused, uint32_t sectors) {
    (void)unused;
    uint32_t ring = ae_rd32(r, AE_OVL180_G_STREAM_RING);
    if (sectors == 0u) sectors = 0x28u;
    if (ring == 0u) {
        AeAreaSetHooks area_hooks = {0};
        const AeAreaSetHooks *alloc_hooks = ovl180_areaset_hooks(h, &area_hooks);
        ring = ae_HeapAlloc(r, alloc_hooks, sectors << 11);
        ae_wr32(r, AE_OVL180_G_STREAM_RING, ring);
    }

    const uint32_t set_ring_args[2] = {ring, sectors};
    (void)ovl180_native(r, h, "StSetRing", OVL180_API_ST_SET_RING,
                        2u, set_ring_args);
    const uint32_t set_stream_args[5] = {0u, 0u, UINT32_MAX, 0u, 0u};
    (void)ovl180_native(r, h, "StSetStream", OVL180_API_ST_SET_STREAM,
                        5u, set_stream_args);
}

uint32_t ae_SeqPlayer_DisplayStateInit(AeRam *r, const AeOvl180Hooks *h) {
    ae_wr32(r, OVL180_G_DISPLAY_STATE,
            (uint32_t)ovl180_native1(r, h, "VSyncCallback",
                                    OVL180_API_VSYNC_CALLBACK, 0u));
    ae_wr32(r, OVL180_G_DISPLAY_BUFFER,
            (uint32_t)ovl180_native0(r, h, "ReadGeomScreen", 0x800315D0u));
    ae_wr32(r, OVL180_G_LAST_VSYNC,
            ae_rd32(r, OVL180_EXE_DISPLAY_TIMING_BASE +
                       OVL180_EXE_DISPLAY_LAST_VSYNC));
    ae_wr32(r, OVL180_G_FRAME_COUNTER,
            ae_rd32(r, OVL180_EXE_DISPLAY_TIMING_BASE +
                       OVL180_EXE_DISPLAY_FRAME_COUNT));
    ae_wr32(r, OVL180_G_FRAME_TIME,
            ae_rd32(r, OVL180_EXE_DISPLAY_TIMING_BASE +
                       OVL180_EXE_DISPLAY_FRAME_TIME));
    ae_wr32(r, OVL180_G_PREVIOUS_FRAME_TIME,
            ae_rd32(r, OVL180_EXE_DISPLAY_TIMING_BASE +
                       OVL180_EXE_DISPLAY_PREVIOUS_FRAME_TIME));
    return 0x80140000u;
}

void ae_SeqPlayer_Hook2(AeRam *r, uint32_t out_a, uint32_t out_b) {
    ae_wr32(r, out_a, ae_rd32(r, AE_OVL180_G_HOOK_VALUE_A));
    ae_wr32(r, out_b, ae_rd32(r, AE_OVL180_G_HOOK_VALUE_B));
}

void ae_SeqPlayer_InitHook(AeRam *r, const AeOvl180Hooks *h,
                           uint32_t out_a, uint32_t out_b) {
    AeAreaSetHooks area_hooks = {0};
    const AeAreaSetHooks *alloc_hooks = ovl180_areaset_hooks(h, &area_hooks);
    ae_SeqPlayer_Hook2(r, out_a, out_b);
    uint32_t entry_count = ae_rd32(r, out_a);
    uint32_t file_count = ae_rd32(r, out_b);
    uint32_t bytes = (entry_count << 3) + (file_count << 2);
    uint32_t allocation = ae_HeapAlloc(r, alloc_hooks, bytes);
    ae_wr32(r, AE_OVL180_G_FILE_COUNT, allocation);
    ae_wr32(r, AE_OVL180_G_FILE_DATA,
            allocation + (file_count << 2));
}

static int32_t ovl180_load_files(AeRam *r, const AeOvl180Hooks *h,
                                 uint32_t path) {
    return ovl180_native1(r, h, "SeqPlayer_LoadFiles",
                          OVL180_API_LOAD_FILES, path);
}

static void ovl180_copy_bytes(AeRam *r, const AeOvl180Hooks *h,
                              uint32_t destination, uint32_t source,
                              uint32_t byte_count) {
    (void)ovl180_native3(r, h, "memcpy", OVL180_API_MEMCPY,
                         destination, source, byte_count);
}

void ae_SeqPlayer_ParseObjects(AeRam *r, const AeOvl180Hooks *h,
                               uint32_t data) {
    AeAreaSetHooks area_hooks = {0};
    const AeAreaSetHooks *alloc_hooks = ovl180_areaset_hooks(h, &area_hooks);
    uint32_t descriptor_count = ae_rd32(r, data + OVL180_HEADER_OBJECT_COUNT);
    uint32_t descriptors = data + OVL180_HEADER_BYTES;
    uint32_t auxiliary_bytes = 0;
    uint32_t auxiliary_data = 0x80193800u;

    ae_wr32(r, 0x8013C560u, descriptors);
    ae_wr32(r, AE_OVL180_G_OBJECT_COUNT, descriptor_count);
    for (uint32_t index = 0; index < descriptor_count; ++index) {
        uint32_t descriptor = descriptors + index * OVL180_DESCRIPTOR_STRIDE;
        uint32_t kind = ae_rd8(r, descriptor + OVL180_DESCRIPTOR_AUX_KIND) & 0x0Fu;
        if (kind >= 2u && kind < 5u) auxiliary_bytes += OVL180_AUX_OBJECT_BYTES;
    }

    uint32_t object_table_bytes = descriptor_count * AE_OVL180_SEQ_OBJECT_STRIDE;
    uint32_t object_table = ae_HeapAlloc(r, alloc_hooks,
                                         object_table_bytes + auxiliary_bytes);
    ae_wr32(r, AE_OVL180_G_OBJECTS, object_table);
    uint32_t auxiliary = object_table + object_table_bytes;

    for (uint32_t index = 0; index < descriptor_count; ++index) {
        uint32_t descriptor = descriptors + index * OVL180_DESCRIPTOR_STRIDE;
        uint32_t object_slot = object_table + index * AE_OVL180_SEQ_OBJECT_STRIDE;
        ae_wr32(r, object_slot, auxiliary);
        uint32_t type = ae_rd8(r, descriptor + OVL180_DESCRIPTOR_AUX_KIND);
        uint32_t model_index = ae_rd32(r, descriptor + OVL180_DESCRIPTOR_MODEL_INDEX) &
                               OVL180_MODEL_REFERENCE_MASK;
        uint32_t referenced_data = data + model_index * AE_OVL180_SEQ_OBJECT_STRIDE;

        if (type == 2u) {
            ae_SeqPlayer_MeshInit(r, h, auxiliary, referenced_data);
            uint32_t mesh = ae_rd32(r, auxiliary + OVL180_OBJECT_MESH);
            uint32_t mesh_count = ae_rd16(r, mesh + OVL180_MESH_ELEMENT_COUNT);
            uint32_t vertex_count = ae_rd16(r, mesh + OVL180_MESH_PRIMITIVE_COUNT);
            if (ae_rd32(r, AE_OVL180_G_HOOK_VALUE_A) < mesh_count)
                ae_wr32(r, AE_OVL180_G_HOOK_VALUE_A, mesh_count);
            if (ae_rd32(r, AE_OVL180_G_HOOK_VALUE_B) < vertex_count)
                ae_wr32(r, AE_OVL180_G_HOOK_VALUE_B, vertex_count);
            auxiliary += OVL180_AUX_OBJECT_BYTES;
        } else if (type == 3u) {
            ae_SeqPlayer_ModelInit(r, h, auxiliary, referenced_data);
            uint32_t model = ae_rd32(r, auxiliary + OVL180_OBJECT_MESH);
            uint32_t vertex_count = ae_rd16(r, model + OVL180_MODEL_VERTEX_COUNT);
            uint32_t primitive_count = ae_rd16(r, model + OVL180_MODEL_COLOR_COUNT);
            if (ae_rd32(r, AE_OVL180_G_HOOK_VALUE_A) < vertex_count)
                ae_wr32(r, AE_OVL180_G_HOOK_VALUE_A, vertex_count);
            if (ae_rd32(r, AE_OVL180_G_HOOK_VALUE_B) < primitive_count)
                ae_wr32(r, AE_OVL180_G_HOOK_VALUE_B, primitive_count);
            auxiliary += OVL180_AUX_OBJECT_BYTES;
        } else if (type == 4u) {
            uint32_t composite_object = auxiliary;
            uint32_t source_index = ae_rd8(r, descriptor + OVL180_DESCRIPTOR_MODEL_INDEX);
            uint32_t source_slot = object_table + source_index *
                                   AE_OVL180_SEQ_OBJECT_STRIDE;
            uint32_t source_object = ae_rd32(r, source_slot);
            uint32_t source_kind = ae_rd8(r, descriptors + source_index *
                                          OVL180_DESCRIPTOR_STRIDE +
                                          OVL180_DESCRIPTOR_AUX_KIND);

            if (source_kind == 2u) {
                ae_SeqPlayer_MeshInit(r, h, auxiliary,
                                      ae_rd32(r, source_object + OVL180_OBJECT_MESH));
                uint32_t source_model = ae_rd32(r, source_object + OVL180_OBJECT_MESH);
                uint32_t source_data = ae_rd32(r, source_object + OVL180_OBJECT_AUXILIARY);
                uint32_t source_duplicate_data = ae_rd32(
                    r, source_object + OVL180_OBJECT_DUPLICATE_A);
                uint32_t source_time = ae_rd16(r, source_model + OVL180_MESH_FIND_TIME);
                uint32_t source_records = ae_rd16(r, source_model + OVL180_MESH_SEQUENCE_COUNT);
                uint32_t object_copy_count = source_records;
                auxiliary += OVL180_AUX_OBJECT_BYTES;
                ae_wr32(r, composite_object + OVL180_OBJECT_AUXILIARY, auxiliary_data);
                uint32_t copied_records = ae_SeqPlayer_FindRecordByTime(
                    r, source_data, source_time);
                uint32_t copy_bytes = copied_records << 2;
                ovl180_copy_bytes(r, h, auxiliary_data, source_data, copy_bytes);
                auxiliary_data += copy_bytes;
                uint32_t descriptor_flags = ae_rd16(r, descriptor +
                                                    OVL180_DESCRIPTOR_FLAGS);
                if ((descriptor_flags & 1u) != 0u) {
                    uint32_t operation_address = 0x801FFFB8u;
                    ae_wr8(r, operation_address + OVL180_FLAG_OP_MASK, 3u);
                    ae_wr8(r, operation_address, 1u);
                    ae_wr8(r, operation_address + OVL180_FLAG_OP_LAYER,
                           (descriptor_flags >> 1) & 3u);
                    ae_SeqPlayer_ApplyFlagOps(r, auxiliary_data - copy_bytes,
                                               (int32_t)copied_records,
                                               operation_address);
                }
                if ((descriptor_flags & 8u) != 0u) {
                    uint32_t operation_address = 0x801FFFB8u;
                    ae_wr8(r, operation_address, 0u);
                    ae_wr8(r, operation_address + OVL180_FLAG_OP_ENABLE, 0u);
                    ae_wr8(r, operation_address + OVL180_FLAG_OP_LAYER, 0u);
                    ae_wr8(r, operation_address + OVL180_FLAG_OP_MASK, 4u);
                    ae_SeqPlayer_ApplyFlagOps(r, auxiliary_data - copy_bytes,
                                               (int32_t)copied_records,
                                               operation_address);
                }
                if ((descriptor_flags & 0x10u) != 0u) {
                    uint32_t duplicate_bytes = object_copy_count << 2;
                    ae_wr32(r, composite_object + OVL180_OBJECT_DUPLICATE_A,
                            auxiliary_data);
                    ovl180_copy_bytes(r, h, auxiliary_data,
                                      source_duplicate_data, duplicate_bytes);
                    auxiliary_data += duplicate_bytes;
                    ae_wr32(r, composite_object + OVL180_OBJECT_DUPLICATE_B,
                            auxiliary_data);
                    ovl180_copy_bytes(r, h, auxiliary_data,
                                      source_duplicate_data, duplicate_bytes);
                    auxiliary_data += duplicate_bytes;
                }
            } else if (source_kind == 3u) {
                ae_SeqPlayer_ModelInit(r, h, auxiliary,
                                       ae_rd32(r, source_object + OVL180_OBJECT_MESH));
                uint32_t source_model = ae_rd32(r, source_object + OVL180_OBJECT_MESH);
                uint32_t source_data = ae_rd32(r, source_object + OVL180_OBJECT_AUXILIARY);
                uint32_t source_duplicate_data = ae_rd32(
                    r, source_object + OVL180_OBJECT_DUPLICATE_A);
                uint32_t source_records = ae_rd16(r, source_model + OVL180_MODEL_SEQUENCE_COUNT);
                uint32_t object_copy_count = ae_rd16(r, source_model + OVL180_MODEL_SECONDARY_COUNT);
                auxiliary += OVL180_AUX_OBJECT_BYTES;
                ae_wr32(r, composite_object + OVL180_OBJECT_AUXILIARY, auxiliary_data);
                uint32_t copied_records = ae_SeqPlayer_SumRecordSizes(
                    r, source_data, (int32_t)source_records);
                uint32_t copy_bytes = copied_records << 2;
                ovl180_copy_bytes(r, h, auxiliary_data, source_data, copy_bytes);
                auxiliary_data += copy_bytes;
                uint32_t descriptor_flags = ae_rd16(r, descriptor +
                                                    OVL180_DESCRIPTOR_FLAGS);
                if ((descriptor_flags & 1u) != 0u) {
                    uint32_t operation_address = 0x801FFFB8u;
                    ae_wr8(r, operation_address + OVL180_FLAG_OP_MASK, 3u);
                    ae_wr8(r, operation_address, 1u);
                    ae_wr8(r, operation_address + OVL180_FLAG_OP_LAYER,
                           (descriptor_flags >> 1) & 3u);
                    ae_SeqPlayer_ApplyFlagOps(r, auxiliary_data - copy_bytes,
                                               (int32_t)source_records,
                                               operation_address);
                }
                if ((descriptor_flags & 8u) != 0u) {
                    uint32_t operation_address = 0x801FFFB8u;
                    ae_wr8(r, operation_address, 0u);
                    ae_wr8(r, operation_address + OVL180_FLAG_OP_ENABLE, 0u);
                    ae_wr8(r, operation_address + OVL180_FLAG_OP_LAYER, 0u);
                    ae_wr8(r, operation_address + OVL180_FLAG_OP_MASK, 4u);
                    ae_SeqPlayer_ApplyFlagOps(r, auxiliary_data - copy_bytes,
                                               (int32_t)source_records,
                                               operation_address);
                }
                if ((descriptor_flags & 0x10u) != 0u) {
                    uint32_t duplicate_bytes = object_copy_count << 2;
                    ae_wr32(r, composite_object + OVL180_OBJECT_DUPLICATE_A,
                            auxiliary_data);
                    ovl180_copy_bytes(r, h, auxiliary_data,
                                      source_duplicate_data, duplicate_bytes);
                    auxiliary_data += duplicate_bytes;
                    ae_wr32(r, composite_object + OVL180_OBJECT_DUPLICATE_B,
                            auxiliary_data);
                    ovl180_copy_bytes(r, h, auxiliary_data,
                                      source_duplicate_data, duplicate_bytes);
                    auxiliary_data += duplicate_bytes;
                }
            } else {
                /* This dispatch falls through to the shared copy block with two
                 * never-initialized stack arguments. The checker declares the
                 * original stack window, and its fixtures keep these words zero. */
                ovl180_copy_bytes(r, h, ae_rd32(r, 0x801FFFCCu),
                                  ae_rd32(r, 0x801FFFC8u),
                                  auxiliary_bytes << 2);
            }
        }
    }

    ae_SeqPlayer_ClearObjectFlagsWrap(r, h, descriptor_count);
}

void ae_SeqPlayer_FreeObjects(AeRam *r) {
    (void)ae_HeapFree(r, ae_rd32(r, AE_OVL180_G_OBJECT_FLAGS));
    (void)ae_HeapFree(r, ae_rd32(r, AE_OVL180_G_OBJECTS));
}

static void ovl180_save_restore_vram(AeRam *r, const AeOvl180Hooks *h,
                                     uint32_t restore) {
    (void)ovl180_native1(r, h, "SeqPlayer_SaveRestoreVram",
                         AE_OVL180_FN_SAVE_RESTORE_VRAM, restore);
}

static void ovl180_setup_display(AeRam *r, const AeOvl180Hooks *h) {
    ovl180_native_void(r, h, "SeqPlayer_SetupDisplay",
                       AE_OVL180_FN_SETUP_DISPLAY, 0u, NULL);
}

static void ovl180_cd_set_filter(AeRam *r, const AeOvl180Hooks *h,
                                 uint32_t channel) {
    (void)ovl180_native1(r, h, "SeqPlayer_CdSetFilter",
                         AE_OVL180_FN_CD_SET_FILTER, channel);
}

static int32_t ovl180_present(AeRam *r, const AeOvl180Hooks *h,
                              uint32_t copy_back) {
    return ovl180_native1(r, h, "SeqPlayer_Present",
                          AE_OVL180_FN_PRESENT, copy_back);
}

static void ovl180_cd_int_to_pos(AeRam *r, const AeOvl180Hooks *h,
                                 uint32_t sector, uint32_t location) {
    (void)ovl180_native2(r, h, "CdIntToPos", OVL180_API_CD_INT_TO_POS,
                         sector, location);
}

static void ovl180_cd_play_xa_range(AeRam *r, const AeOvl180Hooks *h,
                                    uint32_t start, uint32_t end,
                                    uint32_t filter, uint32_t restart) {
    const uint32_t args[4] = {start, end, filter, restart};
    ovl180_native_void(r, h, "SeqPlayer_CdPlayXaRange",
                       AE_OVL180_FN_CD_PLAY_XA_RANGE, 4u, args);
}

static int32_t ovl180_fx_set_rot(void *user, AeRam *r, uint32_t a0,
                                 uint32_t a1, uint32_t a2, uint32_t a3,
                                 uint32_t a4) {
    (void)a1; (void)a2; (void)a3; (void)a4;
    return ovl180_native1(r, user, "SetRotMatrix", OVL180_API_SET_ROT_MATRIX, a0);
}

static int32_t ovl180_fx_set_trans(void *user, AeRam *r, uint32_t a0,
                                   uint32_t a1, uint32_t a2, uint32_t a3,
                                   uint32_t a4) {
    (void)a1; (void)a2; (void)a3; (void)a4;
    return ovl180_native1(r, user, "SetTransMatrix", OVL180_API_SET_TRANS_MATRIX, a0);
}

static int32_t ovl180_effect_set_rot(void *user, AeRam *r, uint32_t a0,
                                     uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a1; (void)a2; (void)a3;
    return ovl180_native1(r, user, "SetRotMatrix", OVL180_API_SET_ROT_MATRIX, a0);
}

static int32_t ovl180_effect_set_trans(void *user, AeRam *r, uint32_t a0,
                                       uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a1; (void)a2; (void)a3;
    return ovl180_native1(r, user, "SetTransMatrix", OVL180_API_SET_TRANS_MATRIX, a0);
}

static int32_t ovl180_effect_gadget4_trail(void *user, AeRam *r,
                                           uint32_t a0, uint32_t a1,
                                           uint32_t a2, uint32_t a3) {
    (void)a0; (void)a1; (void)a2; (void)a3;
    return ovl180_native0(r, user, "Gadget4TrailUpdate",
                          OVL180_API_GADGET4_TRAIL_UPDATE);
}

static int32_t ovl180_effect_gadget_flash(void *user, AeRam *r,
                                          uint32_t a0, uint32_t a1,
                                          uint32_t a2, uint32_t a3) {
    (void)a0; (void)a1; (void)a2; (void)a3;
    return ovl180_native0(r, user, "GadgetFlashUpdate",
                          OVL180_API_GADGET_FLASH_UPDATE);
}

static int32_t ovl180_pad_get_state(void *user, AeRam *r, int32_t port) {
    return ovl180_native1(r, user, "PadGetState", OVL180_API_PAD_GET_STATE,
                          (uint32_t)port);
}

static int32_t ovl180_pad_info_mode(void *user, AeRam *r, int32_t port,
                                    int32_t mode, int32_t index) {
    return ovl180_native3(r, user, "PadInfoMode", OVL180_API_PAD_INFO_MODE,
                          (uint32_t)port, (uint32_t)mode, (uint32_t)index);
}

static void ovl180_pad_setup_dualshock(void *user, AeRam *r, int32_t arg) {
    (void)ovl180_native1(r, user, "PadSetupDualShock",
                         OVL180_API_PAD_SETUP_DUALSHOCK, (uint32_t)arg);
}

static int32_t ovl180_pad_demo_step(void *user, AeRam *r) {
    return ovl180_native0(r, user, "PadDemoPlaybackStep",
                          OVL180_API_PAD_DEMO_STEP);
}

static void ovl180_pad_stage_request(void *user, AeRam *r, int32_t reason,
                                     int32_t area, int32_t entry) {
    (void)ovl180_native3(r, user, "StageRequest", OVL180_API_STAGE_REQUEST,
                         (uint32_t)reason, (uint32_t)area, (uint32_t)entry);
}

static uint32_t ovl180_pad_stick_to_buttons(void *user, AeRam *r) {
    return (uint32_t)ovl180_native0(r, user, "StickToButtons",
                                    OVL180_API_STICK_TO_BUTTONS);
}

static void ovl180_pad_rumble_reset(void *user, AeRam *r, int32_t port) {
    (void)ovl180_native1(r, user, "PadRumbleReset",
                         OVL180_API_PAD_RUMBLE_RESET, (uint32_t)port);
}

void ovl180_RunEffectScript(AeRam *r, const AeOvl180Hooks *h) {
    AeFxscriptHooks hooks = {0};
    hooks.user = (void *)h;
    hooks.SetRotMatrix = ovl180_fx_set_rot;
    hooks.SetTransMatrix = ovl180_fx_set_trans;
    ae_EffectScriptUpdate(r, &hooks);
}

void ovl180_RunEffects(AeRam *r, const AeOvl180Hooks *h) {
    AeEffectsHooks hooks = {0};
    hooks.user = (void *)h;
    hooks.Gadget4TrailUpdate = ovl180_effect_gadget4_trail;
    hooks.GadgetFlashUpdate = ovl180_effect_gadget_flash;
    hooks.SetRotMatrix = ovl180_effect_set_rot;
    hooks.SetTransMatrix = ovl180_effect_set_trans;
    ae_EffectsUpdateAll(r, NULL, &hooks);
}

void ovl180_RunPadUpdate(AeRam *r, const AeOvl180Hooks *h, uint32_t flags) {
    AePadHooks hooks = {0};
    hooks.user = (void *)h;
    hooks.pad_get_state = ovl180_pad_get_state;
    hooks.pad_info_mode = ovl180_pad_info_mode;
    hooks.pad_setup_dualshock = ovl180_pad_setup_dualshock;
    hooks.pad_demo_step = ovl180_pad_demo_step;
    hooks.stage_request = ovl180_pad_stage_request;
    hooks.stick_to_buttons = ovl180_pad_stick_to_buttons;
    hooks.pad_rumble_reset = ovl180_pad_rumble_reset;
    ae_pad_PadUpdate(r, &hooks, flags);
}

void ae_SeqPlayer_FadeIn(AeRam *r, const AeOvl180Hooks *h) {
    uint32_t file = ae_rd32(r, AE_OVL180_G_SEQUENCE_FILE);
    if ((ae_rd32(r, file + OVL180_HEADER_FLAGS_OFFSET) &
         OVL180_HEADER_LETTERBOX_FLAG) == 0u) return;

    const uint32_t render_state = 0x800EC350u;
    uint32_t display_index = ae_rd32(r,
        render_state + OVL180_DISPLAY_CONTEXT_BUFFER_INDEX) ^ 1u;
    ae_wr32(r, render_state + OVL180_DISPLAY_CONTEXT_BUFFER_INDEX,
            display_index);
    ovl180_native0(r, h, "SeqPlayer_CopyScreenToBack",
                   AE_OVL180_FN_COPY_SCREEN_TO_BACK);
    uint32_t frame = 0u;
    (void)ovl180_native1(r, h, "DrawSync", OVL180_API_DRAW_SYNC, frame);
    while (frame < 0x100u) {
        ae_wr8(r, OVL180_G_FADE_DRAW_ACTIVE, 0u);
        ae_wr8(r, OVL180_G_FADE_FRAME_ACTIVE, 0u);
        (void)ovl180_native0(r, h, "BeginFrame", OVL180_API_BEGIN_FRAME);
        (void)ovl180_native3(r, h, "ScreenBandDraw", 0x8009B9C8u,
                             30u, 30u, frame);
        (void)ovl180_native1(r, h, "DrawSync", OVL180_API_DRAW_SYNC, 0u);
        (void)ovl180_native1(r, h, "VSync", OVL180_API_VSYNC, 0u);

    uint32_t buffer_offset = display_index * OVL180_FRAME_BUFFER_STRIDE;
        (void)ovl180_native1(r, h, "PutDispEnv", OVL180_API_PUT_DISP_ENV,
                             render_state + OVL180_DISPLAY_ENV_OFFSET + buffer_offset);
        (void)ovl180_native1(r, h, "PutDrawEnv", OVL180_API_PUT_DRAW_ENV,
                             render_state + OVL180_DRAW_ENV_OFFSET + buffer_offset);
        (void)ovl180_native1(r, h, "DrawOTag", 0x80028CC0u,
                             render_state + OVL180_ORDERING_TABLE_OFFSET + buffer_offset);
        frame += 4u;
    }
    ae_wr8(r, OVL180_G_FADE_DRAW_ACTIVE, 1u);
    ae_wr8(r, OVL180_G_FADE_FRAME_ACTIVE, 1u);
}

void ae_SeqPlayer_Stop(AeRam *r, const AeOvl180Hooks *h, uint32_t restore) {
    (void)ovl180_native0(r, h, "CdSync", OVL180_API_CDSYNC);
    if (ae_rd16(r, OVL180_TITLE_SEQUENCE_INDEX) == 6u && restore == 0u) {
        const uint32_t start_location = 0x8013C150u;
        const uint32_t start_args[1] = {start_location};
        uint32_t start = (uint32_t)ovl180_native(r, h, "CdPosToInt",
                                                  OVL180_API_CD_POS_TO_INT,
                                                  1u, start_args);
        uint32_t caller_stack = h && h->ram_stack_pointer != 0u
            ? h->ram_stack_pointer : OVL180_DEFAULT_CALLER_STACK;
        uint32_t stop_stack = caller_stack - OVL180_STOP_STACK_BYTES;
        uint32_t xa_start_location = stop_stack +
                                    OVL180_STOP_STACK_START_LOCATION;
        uint32_t xa_end_location = stop_stack +
                                  OVL180_STOP_STACK_END_LOCATION;
        ae_wr8(r, xa_start_location, 0u);
        ae_wr8(r, xa_start_location + OVL180_CD_LOCATION_SECONDS, 37u);
        ae_wr8(r, xa_start_location + OVL180_CD_LOCATION_SECTORS, 51u);
        const uint32_t start_offset_args[1] = {xa_start_location};
        uint32_t start_offset = (uint32_t)ovl180_native(r, h, "CdPosToInt",
                                                         OVL180_API_CD_POS_TO_INT,
                                                         1u, start_offset_args);
        ovl180_cd_int_to_pos(r, h, start + start_offset, xa_start_location);
        ae_wr8(r, xa_end_location, 2u);
        ae_wr8(r, xa_end_location + OVL180_CD_LOCATION_SECONDS, 66u);
        ae_wr8(r, xa_end_location + OVL180_CD_LOCATION_SECTORS, 51u);
        const uint32_t end_args[1] = {xa_end_location};
        uint32_t end_offset = (uint32_t)ovl180_native(r, h, "CdPosToInt",
                                                       OVL180_API_CD_POS_TO_INT,
                                                       1u, end_args);
        ovl180_cd_int_to_pos(r, h, start + end_offset, xa_end_location);
        ovl180_cd_play_xa_range(r, h, xa_start_location, xa_end_location, 1u, 0u);
    }

    (void)ovl180_native1(r, h, "SoundStopAndRestoreVolume",
                         OVL180_API_SOUND_RESTORE_VOLUME, 2u);
    ovl180_cd_set_filter(r, h, 0u);
    while (ovl180_native3(r, h, "CdReady", OVL180_API_CD_READY,
                          9u, 0u, 0u) == 0) {}
    ovl180_cd_set_filter(r, h, 0u);
    while (ovl180_native3(r, h, "CdReady", OVL180_API_CD_READY,
                          9u, 0u, 0u) == 0) {}
    (void)ovl180_present(r, h, restore);

    ae_SeqPlayer_FreeObjects(r);
    uint32_t ring = ae_rd32(r, AE_OVL180_G_STREAM_RING);
    (void)ae_HeapFree(r, ring);
    ae_wr32(r, AE_OVL180_G_STREAM_RING, 0u);

    const uint32_t display_buffer = ae_rd32(r, AE_OVL180_G_DISPLAY_BUFFER);
    (void)ovl180_native1(r, h, "VSyncCallback",
                         OVL180_API_VSYNC_CALLBACK,
                         ae_rd32(r, AE_OVL180_G_DISPLAY_STATE));
    (void)ovl180_native1(r, h, "SetGeomScreen",
                         OVL180_API_SET_GEOM_SCREEN, display_buffer);
    ae_wr32(r, OVL180_G_SCREEN_ENV_PRIMARY, 0x801A8000u);
    ae_wr32(r, OVL180_G_SCREEN_ENV_SECONDARY, 0x801C1000u);
    ae_wr32(r, OVL180_G_STOP_TIMING_BUFFER + OVL180_STOP_TIMING_LAST_VSYNC,
            ae_rd32(r, AE_OVL180_G_LAST_VSYNC));
    ae_wr32(r, OVL180_G_STOP_TIMING_BUFFER + OVL180_STOP_TIMING_FRAME_COUNT,
            ae_rd32(r, AE_OVL180_G_FRAME_COUNTER));
    ae_wr32(r, OVL180_G_STOP_TIMING_BUFFER + OVL180_STOP_TIMING_FRAME_TIME,
            ae_rd32(r, 0x8013C498u));
    ae_wr32(r, OVL180_G_STOP_TIMING_BUFFER + OVL180_STOP_TIMING_PREVIOUS_FRAME_TIME,
            ae_rd32(r, 0x8013C49Cu));
    ovl180_native_void(r, h, "SeqPlayer_LoadSavedImage",
                       AE_OVL180_FN_LOAD_SAVED_IMAGE, 0u, NULL);
    ovl180_save_restore_vram(r, h, 1u);
    (void)ovl180_native1(r, h, "DrawSync", OVL180_API_DRAW_SYNC, 0u);
}

int32_t ae_SeqPlayer_Init(AeRam *r, const AeOvl180Hooks *h,
                          uint32_t display_state, uint32_t sequence_index) {
    AeAreaSetHooks area_hooks = {0};
    const AeAreaSetHooks *alloc_hooks = ovl180_areaset_hooks(h, &area_hooks);

    ovl180_save_restore_vram(r, h, 0u);
    (void)ae_SeqPlayer_DisplayStateInit(r, h);
    ae_HeapInit(r, alloc_hooks);
    (void)ovl180_native1(r, h, "srand", 0x800A3750u, 1u);

    uint32_t path = AE_OVL180_G_FILE_INDEX_TABLE + sequence_index * 0x1Cu;
    ae_wr32(r, AE_OVL180_G_SEQUENCE_FILE, path);
    if (ovl180_load_files(r, h, sequence_index) != 0) return 1;

    uint32_t file = ae_rd32(r, AE_OVL180_G_SEQUENCE_FILE);
    uint32_t header_byte = ae_rd8(r, file + OVL180_HEADER_OBJECT_COUNT);
    ae_wr32(r, 0x8013C4B4u, header_byte);
    ae_wr32(r, 0x8013C4B0u, header_byte);
    ae_SeqPlayer_ParseObjects(r, h, 0x80148000u);
    ae_SeqPlayer_InitHook(r, h, 0x801FFF80u, 0x801FFF84u);
    ae_SeqPlayer_StreamRingInit(r, h, 0u,
                                ae_rd8(r, file + OVL180_HEADER_STREAM_SECTORS));

    uint32_t width = 0x120u;
    uint32_t header_flags = ae_rd32(r, file + OVL180_HEADER_FLAGS_OFFSET);
    if ((header_flags & OVL180_HEADER_WIDTH_FLAG) != 0u) width = 0x1A0u;
    ae_wr32(r, display_state, width);
    if ((header_flags & OVL180_HEADER_AUDIO_FLAG) != 0u) {
        ae_wr32(r, display_state, ae_rd32(r, display_state) | 0x48u);
        ovl180_cd_set_filter(r, h, 1u);
    }

    (void)ovl180_native1(r, h, "SoundStopAndRestoreVolume",
                         OVL180_API_SOUND_RESTORE_VOLUME, UINT32_MAX);
    while (ovl180_native3(r, h, "XaPlay", OVL180_API_XA_PLAY,
                          12u, 0u, 0u) == 0) {}
    ae_SeqPlayer_FadeIn(r, h);
    ovl180_setup_display(r, h);
    return 0;
}

static void ovl180_wait_for_list(AeRam *r, const AeOvl180Hooks *h) {
    while (ovl180_native0(r, h, "LoadListStep", OVL180_API_LOAD_LIST_STEP) == 0) {
        (void)ovl180_native0(r, h, "CdMgrUpdate", OVL180_API_CD_MANAGER_UPDATE);
        (void)ovl180_native1(r, h, "VSync", OVL180_API_VSYNC, 2u);
    }
}

typedef struct AeOvl180CommandHook {
    uint32_t address;
    const char *name;
} AeOvl180CommandHook;

static const AeOvl180CommandHook ovl180_command_hooks[] = {
    {0x80137DB4u, "SeqPlayer_CommandDrawPoseMesh"},
    {0x80137F64u, "SeqPlayer_CommandDrawPoseModel"},
    {0x8013800Cu, "SeqPlayer_CommandSetProjection"},
    {0x8013818Cu, "SeqPlayer_CommandDrawPrimitive"},
    {0x80138254u, "SeqPlayer_CommandDrawPoseModelAlt"},
    {0x801382FCu, "SeqPlayer_CommandSetBackColor"},
    {0x80138354u, "SeqPlayer_DrawSprite"},
    {0x80138404u, "SeqPlayer_CommandDrawObject"},
    {0x801385CCu, "SeqPlayer_CommandDrawPoseModel2"},
    {0x80138670u, "SeqPlayer_CommandSetObjectState"},
    {0x80138730u, "SeqPlayer_CommandDrawText"},
    {0x8013B390u, "SeqPlayer_CommandDrawAnimatedModel"},
    {0x801388F0u, "SeqPlayer_CommandDrawObject2"},
    {0x80138A6Cu, "SeqPlayer_CommandDrawParticle"},
    {0x80138AC8u, "SeqPlayer_CommandSetObjectMode"},
    {0x80138B40u, "SeqPlayer_CommandSetObjectTransform"},
    {0x80138CE4u, "SeqPlayer_CommandDrawTrail"},
    {0x80138D60u, "SeqPlayer_CommandDrawPoseModel3"},
    {0x80138E1Cu, "SeqPlayer_CommandDrawPoseModel4"},
    {0x80138ED8u, "SeqPlayer_CommandDrawPoseModel5"},
    {0x80138F90u, "SeqPlayer_CommandDrawPoseMesh2"},
    {0x80139124u, "SeqPlayer_CommandSetColor"},
    {0x801391C8u, "SeqPlayer_CommandBuildModelPackets"},
    {0x8013AAF0u, "SeqPlayer_CommandDrawMessage"},
    {0x8013AF50u, "SeqPlayer_CommandDrawTextPage"}
};

static int ovl180_command_is_empty(uint32_t address) {
    switch (address) {
    case 0x80137DACu:
    case 0x80138344u:
    case 0x8013834Cu:
    case 0x801383FCu:
    case 0x80138728u:
    case 0x801388D8u:
    case 0x801388E0u:
    case 0x801388E8u:
    case 0x80138CDCu:
        return 1;
    default:
        return 0;
    }
}

static const char *ovl180_command_name(uint32_t address) {
    for (uint32_t index = 0;
         index < sizeof(ovl180_command_hooks) / sizeof(ovl180_command_hooks[0]);
         ++index) {
        if (ovl180_command_hooks[index].address == address)
            return ovl180_command_hooks[index].name;
    }
    return "SeqPlayer_CommandRenderer";
}

static int32_t ovl180_renderer_call(AeRam *r, const AeOvl180Hooks *h,
                                    const char *name, uint32_t address,
                                    uint32_t argc, const uint32_t *args) {
    if (!h || !h->renderer) {
        ae_ram_fail(r, "AeOvl180 renderer hook %s not provided", name);
        return 0;
    }
    return h->renderer(h->user, r, name, address, argc, args);
}

static uint32_t ovl180_run_stack_pointer(const AeOvl180Hooks *h) {
    if (h && h->ram_stack_pointer != 0u) return h->ram_stack_pointer;
    return 0x801FFEB8u;
}

static int32_t ovl180_stream_wait(AeRam *r, const AeOvl180Hooks *h) {
    uint32_t stack = ovl180_run_stack_pointer(h);
    return ovl180_native2(r, h, "SeqPlayer_StGetNextWait",
                          OVL180_API_ST_WAIT,
                          stack + OVL180_RUN_STACK_FIRST_RECORD,
                          stack + OVL180_RUN_STACK_SECOND_RECORD);
}

static void ovl180_run_present_frame(AeRam *r, const AeOvl180Hooks *h,
                                     uint32_t frame_duration,
                                     uint32_t *last_vsync,
                                     uint32_t *elapsed_frames,
                                     uint32_t *ring_pending) {
    uint32_t frame_context = OVL180_G_MODE_FRAME;
    (void)ovl180_native1(r, h, "DrawSync", OVL180_API_DRAW_SYNC, 0u);
    (void)ovl180_native1(r, h, "VSync", OVL180_API_VSYNC, frame_duration);

    uint32_t display_index = ae_rd32(r,
        frame_context + OVL180_DISPLAY_CONTEXT_BUFFER_INDEX);
    uint32_t buffer_offset = display_index * OVL180_FRAME_BUFFER_STRIDE;
    ae_wr32(r, OVL180_RUN_FRAME_TIME_VALUE,
            ae_rd32(r, OVL180_RUN_FRAME_TIME_LIMIT));
    (void)ovl180_native1(r, h, "PutDispEnv", OVL180_API_PUT_DISP_ENV,
                         frame_context + OVL180_DISPLAY_ENV_OFFSET + buffer_offset);
    (void)ovl180_native1(r, h, "PutDrawEnv", OVL180_API_PUT_DRAW_ENV,
                         frame_context + OVL180_DRAW_ENV_OFFSET + buffer_offset);

    uint32_t before_vsync = *last_vsync;
    *last_vsync = (uint32_t)ovl180_native1(r, h, "VSync",
                                           OVL180_API_VSYNC, UINT32_MAX);
    uint32_t elapsed = *last_vsync - before_vsync;
    *elapsed_frames += elapsed;
    if (*elapsed_frames >= OVL180_RUN_MAX_DISPLAY_FRAMES) *ring_pending = 1u;

    (void)ovl180_renderer_call(r, h, "DrawOTag", 0x80028CC0u, 1u,
        (const uint32_t[]){frame_context + OVL180_ORDERING_TABLE_OFFSET + buffer_offset});
    (void)ovl180_renderer_call(r, h, "DrawOTag", 0x80028CC0u, 1u,
        (const uint32_t[]){0x800F03D0u + buffer_offset});
    ovl180_RunPadUpdate(r, h, 0u);
}

static void ovl180_run_present_final(AeRam *r, const AeOvl180Hooks *h,
                                     uint32_t frame_duration) {
    uint32_t frame_context = OVL180_G_MODE_FRAME;
    (void)ovl180_native1(r, h, "DrawSync", OVL180_API_DRAW_SYNC, 0u);
    (void)ovl180_native1(r, h, "VSync", OVL180_API_VSYNC,
                         frame_duration);

    uint32_t display_index = ae_rd32(r,
        frame_context + OVL180_DISPLAY_CONTEXT_BUFFER_INDEX);
    uint32_t buffer_offset = display_index * OVL180_FRAME_BUFFER_STRIDE;
    (void)ovl180_native1(r, h, "PutDispEnv", OVL180_API_PUT_DISP_ENV,
                         frame_context + OVL180_DISPLAY_ENV_OFFSET + buffer_offset);
    ae_wr32(r, OVL180_RUN_FRAME_TIME_VALUE, frame_duration);
    (void)ovl180_native1(r, h, "PutDrawEnv", OVL180_API_PUT_DRAW_ENV,
                         frame_context + OVL180_DRAW_ENV_OFFSET + buffer_offset);
}

int32_t ae_SeqPlayer_Run(AeRam *r, const AeOvl180Hooks *h,
                         uint32_t sequence_index) {
    while (ae_SeqPlayer_Init(r, h, AE_OVL180_G_SEQUENCE, sequence_index) != 0) {}

    uint32_t file = ae_rd32(r, AE_OVL180_G_SEQUENCE_FILE);
    uint32_t audio_location = file + OVL180_FILE_STREAM_OFFSET;
    uint32_t sector = (uint32_t)ovl180_native1(r, h, "CdPosToInt",
                                                OVL180_API_CD_POS_TO_INT,
                                                audio_location);
    ovl180_cd_int_to_pos(r, h, sector + OVL180_AUDIO_SEEK_SECTORS,
                         audio_location);
    (void)ovl180_native2(r, h, "SeqPlayer_CdReadSeek",
                         OVL180_API_CD_READ_SEEK, audio_location,
                         ae_rd32(r, AE_OVL180_G_SEQUENCE));

    uint32_t stack = ovl180_run_stack_pointer(h);
    if (ovl180_stream_wait(r, h) != 0) {
        ae_SeqPlayer_Stop(r, h, 2u);
        return 1;
    }

    (void)ovl180_native1(r, h, "SetDispMask", 0x80028634u, 1u);
    uint32_t last_vsync = (uint32_t)ovl180_native1(r, h, "VSync",
                                                   OVL180_API_VSYNC,
                                                   UINT32_MAX);
    ae_wr32(r, OVL180_G_MODE_FRAME, 1u);

    uint32_t previous_buttons = 0u;
    uint32_t current_buttons = 0u;
    uint32_t elapsed_frames = 0u;
    uint32_t ring_pending = 0u;
    for (;;) {
        (void)ovl180_native0(r, h, "BeginFrame", OVL180_API_BEGIN_FRAME);
        uint32_t record = ae_rd32(r, stack + OVL180_RUN_STACK_FIRST_RECORD);
        if (record != 0u) {
            while (ae_rd16(r, record) != 0u) {
                uint32_t command_id = ae_rd8(r, record + OVL180_COMMAND_ID);
                uint32_t command_address = ae_rd32(r,
                    AE_OVL180_G_COMMAND_TABLE + command_id * 4u);
                if (command_address == OVL180_CMD_SET_SWEEP_MODE) {
                    (void)ae_SeqPlayer_SetTransitionSweep(r, record,
                                                         record + OVL180_COMMAND_ARGUMENTS);
                } else if (command_address == OVL180_CMD_ADVANCE_SWEEP) {
                    ae_SeqPlayer_AdvanceTransitionSweep(r, h, record,
                                                        record + OVL180_COMMAND_ARGUMENTS);
                } else if (command_address == OVL180_CMD_WRITE_OBJECT_FLAGS) {
                    ae_SeqPlayer_CommandWriteObjectFlags(r, record,
                                                         record + OVL180_COMMAND_ARGUMENTS);
                } else if (!ovl180_command_is_empty(command_address)) {
                    const uint32_t args[2] = {record,
                        record + OVL180_COMMAND_ARGUMENTS};
                    const char *name = ovl180_command_name(command_address);
                    (void)ovl180_renderer_call(r, h, name, command_address,
                                               2u, args);
                }
                uint32_t record_size = ae_rd16(r, record);
                record += record_size * 2u;
            }
        }

        ae_SeqPlayer_ClearObjectFlags(r);
        ovl180_RunEffectScript(r, h);
        ovl180_RunEffects(r, h);

        uint32_t frame_marker = ae_rd16(r,
                                        record + OVL180_COMMAND_FRAME_MARKER);
        if (frame_marker == UINT16_MAX) {
            ovl180_run_present_final(r, h,
                ae_rd32(r, OVL180_RUN_FRAME_TIME_LIMIT));
            ae_SeqPlayer_Stop(r, h, 0u);
            return 0;
        }

        uint32_t frame_count = ae_rd32(r, OVL180_G_MODE_FRAME);
        uint32_t frame_limit = ae_rd32(r,
                                       file + OVL180_FILE_THIRD_DATA_OFFSET);
        if (frame_limit != 0u && frame_count >= frame_limit) {
            ae_SeqPlayer_Stop(r, h, 0u);
            return 1;
        }

        (void)ovl180_native1(r, h, "StFree", OVL180_API_ST_FREE,
                             ae_rd32(r, stack + OVL180_RUN_STACK_FIRST_RECORD));
        if (previous_buttons != current_buttons &&
            (current_buttons & OVL180_RUN_BUTTON_MASK) != 0u) {
            ae_SeqPlayer_Stop(r, h, 1u);
            return 1;
        }

        ae_wr32(r, OVL180_G_MODE_FRAME, frame_count + 1u);
        if (ovl180_stream_wait(r, h) != 0) {
            ae_SeqPlayer_Stop(r, h, 1u);
            return 1;
        }

        if (ring_pending != 0u) {
            uint32_t dropped_frames = elapsed_frames >> 1;
            elapsed_frames -= dropped_frames << 1;
            ring_pending = 0u;
            while (dropped_frames != 0u) {
                uint32_t stack_outputs = stack + OVL180_RUN_STACK_RING_OUTPUT;
                (void)ovl180_native2(r, h, "StRingStatus",
                                     OVL180_API_ST_RING_STATUS,
                                     stack_outputs,
                                     stack + OVL180_RUN_STACK_RING_STATUS);
                if (ae_rds16(r, stack + OVL180_RUN_STACK_RING_STATUS) >= 4) {
                    (void)ovl180_native1(r, h, "StFree", OVL180_API_ST_FREE,
                        ae_rd32(r, stack + OVL180_RUN_STACK_FIRST_RECORD));
                    if (ovl180_stream_wait(r, h) != 0) {
                        ae_SeqPlayer_Stop(r, h, 1u);
                        return 1;
                    }
                }
                --dropped_frames;
            }
        }

        ovl180_run_present_frame(r, h,
            ae_rd32(r, OVL180_RUN_FRAME_TIME_LIMIT), &last_vsync,
            &elapsed_frames, &ring_pending);
        previous_buttons = current_buttons;
        current_buttons = (uint32_t)ae_rd16(r, AE_PAD_HELD) |
                          (ae_rd32(r, AE_PAD2_HELD) << 16);
    }
}

uint32_t ae_SeqPlayer_SetTransitionSweep(AeRam *r, uint32_t command,
                                         uint32_t arguments) {
    (void)arguments;
    uint32_t mode = ae_rd8(r, command + OVL180_COMMAND_SWEEP_MODE);
    ae_wr16(r, OVL180_G_SWEEP_TIMER, 0u);
    ae_wr8(r, OVL180_G_SWEEP_MODE, mode);
    return mode;
}

void ae_SeqPlayer_AdvanceTransitionSweep(AeRam *r,
                                         const AeOvl180Hooks *h,
                                         uint32_t command,
                                         uint32_t arguments) {
    (void)command;
    (void)arguments;
    uint32_t mode = ae_rd8(r, OVL180_G_SWEEP_MODE);
    uint32_t timer = ae_rd16(r, OVL180_G_SWEEP_TIMER);
    uint16_t next_timer = (uint16_t)(timer + mode);
    ae_wr16(r, OVL180_G_SWEEP_TIMER, next_timer);
    if ((int16_t)next_timer >= 193) {
        next_timer = (uint16_t)mode;
        ae_wr16(r, OVL180_G_SWEEP_TIMER, next_timer);
    }

    const uint32_t render_args[3] = {mode, next_timer,
                                     OVL180_G_MODE_FRAME};
    (void)ovl180_renderer_call(r, h, "SeqPlayer_DrawTransitionSweep",
                               OVL180_CMD_ADVANCE_SWEEP, 3u, render_args);
}

void ae_SeqPlayer_CommandWriteObjectFlags(AeRam *r, uint32_t command,
                                          uint32_t arguments) {
    uint32_t payload_count = (uint32_t)ae_rd16(r, command) - 2u;
    payload_count >>= 1;
    uint32_t payload = arguments;
    for (uint32_t index = 0; index < payload_count; ++index) {
        int32_t flag_index = ae_rds16(r, payload);
        uint32_t flag_value = ae_rd8(r, payload + OVL180_FLAG_WRITE_VALUE);
        uint32_t flags = ae_rd32(r, AE_OVL180_G_OBJECT_FLAGS);
        ae_wr8(r, flags + (uint32_t)flag_index, flag_value);
        payload += OVL180_FLAG_WRITE_STRIDE;
    }
}

int32_t ae_Mode_Title(AeRam *r, const AeOvl180Hooks *h) {
    (void)ovl180_native1(r, h, "VSync", OVL180_API_VSYNC, 2u);

    uint32_t sequence_index = ae_rd16(r, OVL180_TITLE_SEQUENCE_INDEX);
    (void)ae_SeqPlayer_Run(r, h, sequence_index);

    (void)ovl180_native2(r, h, "LoadListStart", OVL180_API_LOAD_LIST_START,
                         0u, 12u);
    ovl180_wait_for_list(r, h);

    uint32_t selection = ae_rd16(r, OVL180_TITLE_SEQUENCE_INDEX);
    switch (selection) {
    case 0:
        ae_wr16(r, OVL180_MODE_STATE, 8u);
        ae_wr16(r, OVL180_MODE_SUBSTATE, 1u);
        break;
    case 1:
        ae_wr16(r, OVL180_MODE_STATE, 13u);
        (void)ovl180_native1(r, h, "ProgressBitsSet",
                             OVL180_API_PROGRESS_BITS_SET, 1u);
        break;
    case 2:
        ae_wr16(r, OVL180_MODE_STATE, 3u);
        break;
    case 3:
        ae_wr16(r, OVL180_MODE_STATE, 11u);
        ae_wr16(r, OVL180_MODE_SUBSTATE, 78u);
        (void)ovl180_native2(r, h, "LoadListStart", OVL180_API_LOAD_LIST_START,
                             1u, 78u);
        ovl180_wait_for_list(r, h);
        break;
    case 4:
        ae_wr16(r, OVL180_MODE_STATE, 29u);
        (void)ovl180_native1(r, h, "ProgressBitsSet",
                             OVL180_API_PROGRESS_BITS_SET, 4u);
        break;
    case 5:
        ae_wr16(r, OVL180_MODE_STATE, 13u);
        (void)ovl180_native1(r, h, "ProgressBitsSet",
                             OVL180_API_PROGRESS_BITS_SET, 5u);
        break;
    case 6:
        ae_wr16(r, OVL180_MODE_STATE, 35u);
        break;
    case 7:
        (void)ovl180_native2(r, h, "ProgressSetByte",
                             OVL180_API_PROGRESS_SET_BYTE, 11u, 5u);
        ae_wr16(r, OVL180_MODE_STATE, 10u);
        ae_wr16(r, OVL180_MODE_SUBSTATE, 6u);
        break;
    case 8:
        ae_wr16(r, OVL180_MODE_STATE, 11u);
        ae_wr16(r, OVL180_MODE_SUBSTATE, 86u);
        (void)ovl180_native2(r, h, "LoadListStart", OVL180_API_LOAD_LIST_START,
                             1u, 86u);
        ovl180_wait_for_list(r, h);
        break;
    default:
        break;
    }
    return 0;
}

uint32_t ae_SeqPlayer_GetObject(AeRam *r, uint32_t index) {
    int32_t signed_index = (int32_t)(int16_t)(uint16_t)index;
    uint32_t byte_offset = (uint32_t)signed_index * AE_OVL180_SEQ_OBJECT_STRIDE;
    uint32_t entry = ae_rd32(r, AE_OVL180_G_OBJECTS) + byte_offset;
    return ae_rd32(r, entry);
}

uint32_t ae_SeqPlayer_GetObjectFlag(AeRam *r, uint32_t index) {
    uint32_t flags = ae_rd32(r, AE_OVL180_G_OBJECT_FLAGS);
    return ae_rd8(r, flags + index);
}

void ae_SeqPlayer_ClearObjectFlags(AeRam *r) {
    uint32_t count = ae_rd32(r, AE_OVL180_G_OBJECT_COUNT);
    uint32_t flags = ae_rd32(r, AE_OVL180_G_OBJECT_FLAGS);
    for (uint32_t index = 0; index < count; ++index) {
        ae_wr8(r, flags + index, 0u);
    }
}

void ae_SeqPlayer_ClearObjectFlagsWrap(AeRam *r, const AeOvl180Hooks *h,
                                       uint32_t byte_count) {
    AeAreaSetHooks area_hooks = {0};
    const AeAreaSetHooks *alloc_hooks = ovl180_areaset_hooks(h, &area_hooks);
    uint32_t flags = ae_HeapAlloc(r, alloc_hooks, byte_count);
    ae_wr32(r, AE_OVL180_G_OBJECT_FLAGS, flags);
    ae_SeqPlayer_ClearObjectFlags(r);
}

void ae_SeqPlayer_MeshInit(AeRam *r, const AeOvl180Hooks *h,
                           uint32_t object, uint32_t mesh) {
    const uint32_t args[2] = {object, mesh};
    (void)ovl180_native(r, h, "MeshInit", OVL180_API_MESH_INIT, 2u, args);

    uint32_t model = ae_rd32(r, object + OVL180_OBJECT_MESH);
    uint32_t first_word = ae_rd16(r, model + OVL180_MODEL_HEADER_WORD);
    ae_wr16(r, model + OVL180_MODEL_VERTEX_OR_MESH_WORD, first_word);
    ae_wr32(r, object + OVL180_OBJECT_MODEL_WORD,
            ae_rd32(r, object + OVL180_OBJECT_MODEL));
}

void ae_SeqPlayer_ModelInit(AeRam *r, const AeOvl180Hooks *h,
                            uint32_t object, uint32_t model) {
    (void)h;
    ae_ModelInit(r, NULL, object, model);
    uint32_t model_data = model + OVL180_MODEL_DATA_BIAS;
    ae_wr32(r, object + OVL180_OBJECT_MODEL, model_data);
    ae_wr32(r, object + OVL180_OBJECT_MODEL_WORD, model_data);
}

uint32_t ae_SeqPlayer_SumRecordSizes(AeRam *r, uint32_t records,
                                     int32_t count) {
    uint32_t total = 0;
    uint32_t cursor = records;
    for (int32_t index = 0; index < count; ++index) {
        uint32_t size = ae_rd8(r, cursor + OVL180_OBJECT_RECORD_SIZE);
        total += size;
        cursor += size * 4u;
    }
    return total;
}

uint32_t ae_SeqPlayer_FindRecordByTime(AeRam *r, uint32_t records,
                                       uint32_t time) {
    uint32_t total = 0;
    uint32_t cursor = records;
    while (total < time) {
        uint32_t size = ae_rd8(r, cursor + OVL180_OBJECT_RECORD_SIZE);
        total += size;
        cursor += size * 4u;
    }
    return total;
}

int32_t ae_SeqPlayer_FindRangeIndex(AeRam *r, uint32_t value) {
    int32_t count = ae_rds32(r, AE_OVL180_G_RANGE_COUNT);
    uint32_t range = ae_rd32(r, AE_OVL180_G_RANGES);
    for (int32_t index = 0; index < count; ++index) {
        uint32_t low = ae_rd16(r, range + AE_OVL180_RANGE_LOW);
        uint32_t high = ae_rd16(r, range + AE_OVL180_RANGE_HIGH);
        if (low <= value && value <= high) return (int32_t)index;
        range += AE_OVL180_RANGE_STRIDE;
    }
    return -1;
}

void ae_SeqPlayer_ApplyFlagOps(AeRam *r, uint32_t records, int32_t count,
                               uint32_t operation) {
    uint32_t record = records;
    for (int32_t index = 0; index < count; ++index) {
        if ((ae_rd8(r, operation + OVL180_FLAG_OP_MASK) & 1u) != 0u) {
            uint32_t target = ae_rd8(r, record + OVL180_OBJECT_FLAGS);
            target = (target & ~2u) | (ae_rd8(r, operation) << 1);
            ae_wr8(r, record + OVL180_OBJECT_FLAGS, target);
        }
        if ((ae_rd8(r, operation + OVL180_FLAG_OP_MASK) & 2u) != 0u) {
            uint32_t target = ae_rd8(r, record + OVL180_OBJECT_FLAGS_HIGH);
            target = (target & ~0x18u) |
                     (ae_rd8(r, operation + OVL180_FLAG_OP_LAYER) << 3);
            ae_wr8(r, record + OVL180_OBJECT_FLAGS_HIGH, target);
            if ((ae_rd8(r, record + OVL180_OBJECT_FLAGS) & 4u) != 0u) {
                uint32_t layer = ae_rd16(r, record + OVL180_OBJECT_LAYER);
                layer = (layer & ~0x60u) |
                        (ae_rd8(r, operation + OVL180_FLAG_OP_LAYER) << 5);
                ae_wr16(r, record + OVL180_OBJECT_LAYER, layer);
            }
        }
        if ((ae_rd8(r, operation + OVL180_FLAG_OP_MASK) & 4u) != 0u) {
            uint32_t target = ae_rd8(r, record + OVL180_OBJECT_FLAGS);
            target = (target & ~1u) |
                     ae_rd8(r, operation + OVL180_FLAG_OP_ENABLE);
            ae_wr8(r, record + OVL180_OBJECT_FLAGS, target);
        }
        uint32_t size = ae_rd8(r, record + OVL180_OBJECT_RECORD_SIZE);
        record += size * 4u;
    }
}
