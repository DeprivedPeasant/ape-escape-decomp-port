#include "ae_ovl187.h"
#include "../math/native_sdk_apply.h"

#include "../player/ae_player_spawn.h"
#include "../sound/ae_bgm.h"
#include "../ui/ae_hud.h"
#include "../world/ae_areaset.h"
#include "../world/ae_items_init.h"
#include "../world/ae_misc.h"
#include "../world/ae_stage.h"
#include "../world/ae_stagefade.h"

#include <limits.h>

/* Overlay member 187 is loaded at 0x80136000. These globals and object
 * records are outside the member and retain their original RAM addresses. */
#define O187_GAME_STATE             0x800F4350u
#define O187_UI                     0x80139B20u
#define O187_UI_STRINGS             0x80139930u
#define O187_UI_TIME_STRINGS        0x80139AB4u
#define O187_UI_LEVELS              0x80139B00u
#define O187_UI_OBJECTS              0x80139B80u
#define O187_UI_OBJECT_FLAGS        0x8013995Cu
#define O187_UI_RIPPLE_POINTER       0x80139E28u
#define O187_UI_XA_ID                0x80139B6Cu
#define O187_UI_VIEW_MATRIX          0x800E78C0u
#define O187_UI_RENDER_OBJECTS       0x800F4C30u
#define O187_UI_MODEL_OT_BASE         0x800EC3D0u
#define O187_UI_ORDER_CURSOR         0x800EC354u
#define O187_UI_ORDER_TABLE          0x800EC358u
#define O187_UI_FRAME_COUNTER        0x800EC350u
#define O187_UI_VIEW_VERTICES        0x800E7D00u
#define O187_GAME_OBJECT_COUNT       0x800EC35Cu
#define O187_WIPE_STATE              0x80139540u
#define O187_LOAD_ACTIVE             0x801399ACu
#define O187_WIPE_TICK               0x801399AEu
#define O187_MODE_DISPATCH_TABLE     0x80139478u
#define O187_SCRIPT_ACTION_TABLE_ADDRESS 0x801394D8u
#define O187_UI_TEXT_HANDLER_TABLE   0x80139528u
#define O187_AREA_DATA               0x80154000u
#define O187_WIPE_COUNTER            0x80139B4Au
#define O187_WIPE_FIRST_DRAW         0x80139AB0u
#define O187_UI_GLYPH_WIDTHS         0x801399B0u
#define O187_SCANLINE_TIMER          0x80139AFCu
#define O187_SCANLINE_PHASE          0x800AE8F0u
#define O187_DISPLAY_STATE_FLAGS     0x800EC378u
#define O187_DISPLAY_MODEL_FLAGS     0x800F03F0u
#define O187_TEMP_MODEL_COUNT        0x800B4D64u
#define O187_MODE_DESTINATION        0x800B4D6Cu
#define O187_CAMERA_MATRIX            0x800EBFA0u
#define O187_MODE_CURSOR_BASE         0x800F03D0u
#define O187_MODE_BUFFER_INDEX        O187_GAME_OBJECT_COUNT
#define O187_AREA_MODEL_TABLE_PTR     (O187_GAME_STATE + 0x224u)
#define O187_AREA_OBJECT_TABLE_PTR    (O187_GAME_STATE + 0x228u)
#define O187_AREA_MODEL_COUNT         (O187_GAME_STATE + 0x280u)
#define O187_MODE_CLEAR_OT_BASE       0x800EC3BCu
#define O187_MODE_DRAW_OFFSET_BASE    0x800EC360u
#define O187_MODE_DRAW_AREA_BASE      0x800F03CCu
#define O187_MODE_AREA_OVERRIDE       0x80139542u
#define O187_CAMERA_TRANSLATION_X     36u
#define O187_CAMERA_TRANSLATION_Y     40u
#define O187_CAMERA_TRANSLATION_Z     44u
#define O187_AREA_MODELS_COUNT        0x002Cu
#define O187_AREA_MODELS_TABLE        0x0030u
#define O187_AREA_OBJECTS_TABLE       0x0034u
#define O187_AREA_OBJECTS_COUNT       0x0036u
#define O187_MODE_OT_ENTRY_STRIDE     0x4078u
#define O187_SCRATCH_VERTICES        0x1F800080u
#define O187_SCRATCH_ENV_MAP_MODE    0x1F8003F4u

#define O187_DEFAULT_STACK_POINTER    0x801FFF00u

enum O187UiTextHandler {
    O187_UI_TEXT_HANDLER_NONE = 0,
    O187_UI_TEXT_HANDLER_SPACING_32 = 0x80137BD8u,
    O187_UI_TEXT_HANDLER_MESSAGE = 0x80137BE0u,
    O187_UI_TEXT_HANDLER_BLINK = 0x80137BFCu,
    O187_UI_TEXT_HANDLER_BACKDROP = 0x80137C0Cu,
    O187_UI_TEXT_HANDLER_SPACING_16 = 0x80137C1Cu,
    O187_UI_TEXT_HANDLER_TIMES_OR_STARS = 0x80137C40u
};

enum O187ScriptAddress {
    O187_SCRIPT_AREA_255 = 0x801397F8u,
    O187_SCRIPT_MODE_12_READY = 0x80139628u,
    O187_SCRIPT_MODE_12_INCOMPLETE = 0x8013959Cu,
    O187_SCRIPT_MODE_20 = 0x80139774u,
    O187_SCRIPT_MODE_25_EXIT_8 = 0x80139844u,
    O187_SCRIPT_MODE_25_OTHER_EXIT = 0x801398BCu
};

enum O187GameStateField {
    O187_STATE_MODE = 0x120,
    O187_STATE_LEVEL = 0x124,
    O187_STATE_AREA = 0x126,
    O187_STATE_ENTRY = 0x12A,
    O187_STATE_EXIT = 0x12C,
    O187_STATE_CURRENT_TIME = 0x140,
    O187_STATE_BEST_TIME = 0x144,
    O187_STATE_RACE_AREA = 0x14C,
    O187_STATE_RACE_TARGET = 0x15A,
    O187_STATE_STAGE_FLAGS = 0x166,
    O187_STATE_INCOMPLETE_MARKER = 0x2CE,
    O187_STATE_CLEAR_NEXT_KIND = 0x2CC,
    O187_STATE_CLEAR_NEXT_FLAGS = 0x2CD
};

enum O187UiField {
    O187_UI_X = 0x00,
    O187_UI_Y = 0x02,
    O187_UI_COLOR = 0x04,
    O187_UI_DATA = 0x08,
    O187_UI_TEXT_SPACING = 0x0C,
    O187_UI_START_X = 0x10,
    O187_UI_START_Y = 0x12,
    O187_UI_START_Z = 0x14,
    O187_UI_TARGET_X = 0x18,
    O187_UI_TARGET_Y = 0x1A,
    O187_UI_TARGET_Z = 0x1C,
    O187_UI_SCRIPT = 0x20,
    O187_UI_SCRIPT_RESUME = 0x24,
    O187_UI_PHASE = 0x28,
    O187_UI_PHASE_TIMER = 0x2A,
    O187_UI_PHASE_SUBSTATE = 0x2C,
    O187_UI_MODEL_LIST = 0x30,
    O187_UI_VALUE_34 = 0x34,
    O187_UI_VALUE_36 = 0x36,
    O187_UI_VALUE_38 = 0x38,
    O187_UI_TIMER_A = 0x3A,
    O187_UI_VALUE_3C = 0x3C,
    O187_UI_VALUE_3E = 0x3E,
    O187_UI_VALUE_40 = 0x40,
    O187_UI_TIMER_B = 0x42,
    O187_UI_WAIT = 0x44,
    O187_UI_REQUIREMENTS = 0x48,
    O187_UI_RANK_TEXT = 0x4C,
    O187_UI_RANK = 0x50,
    O187_UI_SCRIPT_FLAGS = 0x58,
    O187_UI_SCRIPT_TICKS = 0x5C,
    O187_UI_TRANSITION = 0x60,
    O187_UI_TRANSITION_TIMER = 0x62,
    O187_UI_TRANSITION_TARGET = 0x64,
    O187_UI_TRANSITION_DURATION = 0x66
};

enum O187UiObjectFlagField {
    O187_UI_OBJECT_FLAG_X = 0,
    O187_UI_OBJECT_FLAG_Y = 2,
    O187_UI_OBJECT_FLAG_VALUE = 4,
    O187_UI_OBJECT_FLAG_KIND = 6,
    O187_UI_OBJECT_FLAG_STRIDE = 8,
    O187_UI_OBJECT_FLAG_ACTIVE = 7
};

#define O187_SCRIPT_ACTION_OBJECT_ROTATION    0x80137694u
#define O187_SCRIPT_ACTION_UI_START           0x801376D8u
#define O187_SCRIPT_ACTION_UI_TARGET          0x80137700u
#define O187_SCRIPT_ACTION_UI_GOAL            0x80137728u
#define O187_SCRIPT_ACTION_UI_GOAL2           0x8013775Cu
#define O187_SCRIPT_ACTION_OBJECT_TARGET       0x80137790u
#define O187_SCRIPT_ACTION_OBJECT_FLAGS        0x801377E4u
#define O187_SCRIPT_ACTION_OBJECT_FLAGS_NOW    0x80137814u
#define O187_SCRIPT_ACTION_OBJECT_RANK         0x8013783Cu
#define O187_SCRIPT_ACTION_OBJECT_SEQUENCE     0x80137868u
#define O187_SCRIPT_ACTION_WAIT                0x801378B4u
#define O187_SCRIPT_ACTION_WAIT_COUNTER        0x801378C4u
#define O187_SCRIPT_ACTION_WAIT_TICK            0x801378DCu
#define O187_SCRIPT_ACTION_ICON_FLAG           0x801378FCu
#define O187_SCRIPT_ACTION_SAVE_POINTER        0x80137918u
#define O187_SCRIPT_ACTION_RESTORE_POINTER     0x80137920u
#define O187_SCRIPT_ACTION_COPY_STRIP          0x8013792Cu
#define O187_SCRIPT_ACTION_PLAY_XA             0x80137944u
#define O187_SCRIPT_ACTION_ADVANCE              0x8013795Cu

enum O187ObjectField {
    O187_OBJ_ROTATION = 0x14,
    O187_OBJ_MODEL_INSTANCE = 0x18,
    O187_OBJ_ROT_X = 0x24,
    O187_OBJ_ROT_Y = 0x28,
    O187_OBJ_ROT_Z = 0x2C,
    O187_OBJ_POSITION_X = 0x60,
    O187_OBJ_POSITION_Y = 0x62,
    O187_OBJ_POSITION_Z = 0x64,
    O187_OBJ_ROTATION_STEP = 0x5C,
    O187_OBJ_SEQUENCE_FRAME = 0x5C,
    O187_OBJ_SEQUENCE_INDEX = 0x5E,
    O187_OBJ_SEQUENCE_CURRENT = 0x70,
    O187_OBJ_SEQUENCE_FLAGS = 0x72,
    O187_OBJ_SEQUENCE_POINTER = 0x74,
    O187_OBJ_TARGET_X = 0x78,
    O187_OBJ_TARGET_Y = 0x7A,
    O187_OBJ_TARGET_Z = 0x7C,
    O187_OBJ_MOVE_FRAMES = 0x7E,
    O187_OBJ_TARGET_ROT_X = 0x80,
    O187_OBJ_TARGET_ROT_Y = 0x82,
    O187_OBJ_TARGET_ROT_Z = 0x84,
    O187_OBJ_ROT_FRAMES = 0x86,
    O187_OBJ_STRIDE = 0x88,
    O187_OBJ_SLOTS = 5,
    O187_SEQUENCE_ENTRY_STRIDE = 4,
    O187_SEQUENCE_ENTRY_DURATION = 0,
    O187_SEQUENCE_ENTRY_FRAME = 2
};

enum O187PoseField {
    O187_POSE_X = 4,
    O187_POSE_Y = 6,
    O187_POSE_Z = 8,
    O187_POSE_ANGLE = 0x15
};

enum O187LookAtStackField {
    O187_LOOKAT_INPUT_X = 16,
    O187_LOOKAT_INPUT_Y = 20,
    O187_LOOKAT_INPUT_Z = 24,
    O187_LOOKAT_NORMAL_X = 32,
    O187_LOOKAT_NORMAL_Y = 36,
    O187_LOOKAT_NORMAL_Z = 40,
    O187_LOOKAT_IDENTITY = 48,
    O187_LOOKAT_STORE_TRANSLATION = 80
};

enum O187ModelField {
    O187_MODEL_RECORD_HEADER = 0,
    O187_MODEL_HEADER_ENV_FLAG = 10,
    O187_MODEL_RECORD_STRIDE = 16,
    O187_MODEL_INSTANCE_RECORD = 0,
    O187_MODEL_INSTANCE_POSITION_X = 0x10,
    O187_MODEL_INSTANCE_POSITION_Y = 0x12,
    O187_MODEL_INSTANCE_POSITION_Z = 0x14,
    O187_MODEL_INSTANCE_FLAGS = 0x16,
    O187_MODEL_INSTANCE_ORDER_VALUE = 0x1C,
    O187_MODEL_INSTANCE_STRIDE = 0x20,
    O187_AREA_MODEL_DESCRIPTOR_MESH = 0,
    O187_AREA_MODEL_DESCRIPTOR_VALUE = 2,
    O187_AREA_MODEL_DESCRIPTOR_X = 4,
    O187_AREA_MODEL_DESCRIPTOR_Y = 6,
    O187_AREA_MODEL_DESCRIPTOR_Z = 8,
    O187_AREA_MODEL_DESCRIPTOR_SEQUENCE = 0x18,
    O187_AREA_MODEL_DESCRIPTOR_STRIDE = 12,
    O187_MATRIX_TRANSLATION_X = 20,
    O187_MATRIX_TRANSLATION_Y = 24,
    O187_MATRIX_TRANSLATION_Z = 28
};

enum O187ScriptOpcode {
    O187_SCRIPT_SET_OBJECT_FLAGS = 0x0000,
    O187_SCRIPT_ACTION_TABLE = 0x0040,
    O187_SCRIPT_SET_OBJECT_TARGET = 0x0080,
    O187_SCRIPT_SET_ICON_FLAGS = 0x00C0,
    O187_SCRIPT_SET_OBJECT_RANK_IMMEDIATE = 0x0140,
    O187_SCRIPT_UNUSED_100 = 0x0100,
    O187_SCRIPT_SET_OBJECT_SEQUENCE = 0x0180,
    O187_SCRIPT_SET_WAIT = 0x01C0,
    O187_SCRIPT_WAIT = 0x0200,
    O187_SCRIPT_SAVE_POINTER = 0x0240,
    O187_SCRIPT_RESTORE_POINTER = 0x0280,
    O187_SCRIPT_COPY_STRIP = 0x02C0,
    O187_SCRIPT_SET_FLAGS = 0x0300,
    O187_SCRIPT_WAIT_FOR_TICK = 0x0380,
    O187_SCRIPT_PLAY_XA = 0x03C0,
    O187_SCRIPT_SET_OBJECT_RANK = 0x0400,
    O187_SCRIPT_END = 0x3FC0,
    O187_SCRIPT_OPCODE_MASK = 0x3FC0,
    O187_SCRIPT_PARAMETER_MASK = 0x003F
};

enum O187AreaField {
    O187_LEVEL_RECORD_TIME_GOLD = 0x00,
    O187_LEVEL_RECORD_TIME_SILVER = 0x02,
    O187_LEVEL_RECORD_TIME_BRONZE = 0x04,
    O187_LEVEL_RECORD_BYTES = 0x10
};

enum O187InputField {
    O187_PAD_HELD = 0x800E5478u,
    O187_PAD_CHANGED = 0x800E5480u,
    O187_PAD_CONFIRM_MASK = 0x0860u
};

enum O187RippleField {
    O187_RIPPLE_COLUMNS = 16,
    O187_RIPPLE_ROWS = 12,
    O187_RIPPLE_VERTEX_STRIDE = 8,
    O187_RIPPLE_ROW_BYTES = 0x80,
    O187_RIPPLE_VERTEX_BYTES = 0x600,
    O187_RIPPLE_COLOR = 4,
    O187_RIPPLE_COLOR_RED = 4,
    O187_RIPPLE_COLOR_GREEN = 5,
    O187_RIPPLE_COLOR_BLUE = 6
};

enum O187Address {
    O187_ADDR_CUTSCENE_MODE_OFF = 0x8005CFECu,
    O187_ADDR_RESET_GRAPH = 0x80028350u,
    O187_ADDR_CONTEXT_SETUP = 0x80028850u,
    O187_ADDR_SET_GEOM_SCREEN = 0x8002FAF0u,
    O187_ADDR_CONTEXT_CLEAR = 0x800286CCu,
    O187_ADDR_DRAW_AREA_PROPS = 0x80016F34u,
    O187_ADDR_LOAD_LIST_STEP = 0x800116C0u,
    O187_ADDR_SOUND_RESTORE_VOLUME = 0x80086170u,
    O187_ADDR_CD_MGR_STOP_STREAM = 0x8001231Cu,
    O187_ADDR_CD_MGR_UPDATE = 0x800125B8u,
    O187_ADDR_LEVEL_PROGRESS_GET = 0x80096580u,
    O187_ADDR_LEVEL_TABLE_GET = 0x80097E60u,
    O187_ADDR_LEVEL_RESULT = 0x800964B0u,
    O187_ADDR_PROGRESS_FIELD = 0x80096568u,
    O187_ADDR_ARC_LOAD_ENTRY = 0x80011194u,
    O187_ADDR_BEGIN_FRAME = 0x8005DF48u,
    O187_ADDR_VSYNC = 0x8002E520u,
    O187_ADDR_SET_DRAW_STP = 0x8002B3E0u,
    O187_ADDR_DRAW_MOVE = 0x8002B380u,
    O187_ADDR_MODEL_TRANSFORM_RIGID = 0x800161C0u,
    O187_ADDR_MODEL_TRANSFORM_SKINNED = 0x80016A60u,
    O187_ADDR_MODEL_DRAW = 0x80017CD4u,
    O187_ADDR_MODEL_DRAW_ENV_MAP = 0x8001CE54u,
    O187_ADDR_MODEL_INIT = 0x80015AC0u,
    O187_ADDR_MESH_INIT = 0x80015C34u,
    O187_ADDR_RENDER_SET_DRAW_AREA = 0x80028CC0u,
    O187_ADDR_RENDER_SET_DRAW_OFFSET = 0x80028D30u,
    O187_ADDR_RENDER_CLEAR_OT = 0x80028EFCu,
    O187_ADDR_MESSAGE_BOX_DRAW_STRING = 0x8008DDC8u,
    O187_ADDR_HUD_DRAW_TIME_RANK = 0x8005F68Cu,
    O187_ADDR_LEVEL_TABLE = 0x80097E60u,
    O187_ADDR_GTE_SET_MATRIX = 0x80030450u,
    O187_ADDR_GTE_STORE_MATRIX = 0x800305B0u,
    O187_ADDR_GTE_SET_TRANSFORM = 0x8002FF50u
};

/* The call boundary is deliberately fail-closed: a missing native service is
 * visible on the AeRam image instead of turning into guessed behavior. */
static int32_t ovl187_call(AeRam *r, const AeOvl187Hooks *h,
                           const char *name, uint32_t address,
                           uint32_t argc, const uint32_t *argv)
{
    if (h == NULL || h->call == NULL) {
        ae_ram_fail(r, "ovl187 hook %s (%08X) is not provided", name, address);
        return 0;
    }
    return h->call(h->user, r, name, address, argc, argv);
}

static int32_t ovl187_call0(AeRam *r, const AeOvl187Hooks *h,
                            const char *name, uint32_t address)
{
    return ovl187_call(r, h, name, address, 0u, NULL);
}

static int32_t ovl187_call1(AeRam *r, const AeOvl187Hooks *h,
                            const char *name, uint32_t address,
                            uint32_t a0)
{
    const uint32_t args[] = {a0};
    return ovl187_call(r, h, name, address, 1u, args);
}

static int32_t ovl187_call2(AeRam *r, const AeOvl187Hooks *h,
                            const char *name, uint32_t address,
                            uint32_t a0, uint32_t a1)
{
    const uint32_t args[] = {a0, a1};
    return ovl187_call(r, h, name, address, 2u, args);
}

static int32_t ovl187_call3(AeRam *r, const AeOvl187Hooks *h,
                            const char *name, uint32_t address,
                            uint32_t a0, uint32_t a1, uint32_t a2)
{
    const uint32_t args[] = {a0, a1, a2};
    return ovl187_call(r, h, name, address, 3u, args);
}

static int32_t ovl187_call4(AeRam *r, const AeOvl187Hooks *h,
                            const char *name, uint32_t address,
                            uint32_t a0, uint32_t a1, uint32_t a2,
                            uint32_t a3)
{
    const uint32_t args[] = {a0, a1, a2, a3};
    return ovl187_call(r, h, name, address, 4u, args);
}

static uint32_t ovl187_stack_pointer(const AeOvl187Hooks *h)
{
    return h != NULL && h->stack_pointer != 0u ? h->stack_pointer :
                                                   O187_DEFAULT_STACK_POINTER;
}

static uint32_t ovl187_frame_address(const AeOvl187Hooks *h,
                                     uint32_t frame_bytes, uint32_t offset)
{
    return ovl187_stack_pointer(h) - frame_bytes + offset;
}

static void ovl187_nested_hooks(AeOvl187Hooks *nested,
                                const AeOvl187Hooks *current,
                                uint32_t current_frame_bytes)
{
    if (current != NULL) *nested = *current;
    else {
        const AeOvl187Hooks empty = {0};
        *nested = empty;
    }
    nested->stack_pointer = ovl187_stack_pointer(current) -
                            current_frame_bytes;
}

static int32_t ovl187_s32(uint32_t value)
{
    return (int32_t)value;
}

static uint32_t ovl187_object(uint32_t objects, int32_t index)
{
    return objects + (uint32_t)index * O187_OBJ_STRIDE;
}

static uint32_t ovl187_run_script_action(AeRam *r,
                                         const AeOvl187Hooks *h,
                                         uint32_t script,
                                         uint32_t parameter,
                                         int *suspended)
{
    uint32_t action = ae_rd32(r, O187_SCRIPT_ACTION_TABLE_ADDRESS + parameter * 4u);
    uint32_t object = ovl187_object(O187_UI_OBJECTS, (int32_t)parameter);
    uint32_t ui = O187_UI;
    uint32_t next = script + 2u;

    switch (action) {
    case O187_SCRIPT_ACTION_OBJECT_ROTATION:
        ae_wr16(r, object + O187_OBJ_TARGET_ROT_X, ae_rd16(r, script + 2u));
        ae_wr16(r, object + O187_OBJ_TARGET_ROT_Y, ae_rd16(r, script + 4u));
        ae_wr16(r, object + O187_OBJ_TARGET_ROT_Z, ae_rd16(r, script + 6u));
        ae_wr16(r, object + O187_OBJ_ROT_FRAMES, ae_rd16(r, script + 8u));
        next = script + 10u;
        break;
    case O187_SCRIPT_ACTION_UI_START:
        ae_wr16(r, ui + O187_UI_START_X, ae_rd16(r, script + 2u));
        ae_wr16(r, ui + O187_UI_START_Y, ae_rd16(r, script + 4u));
        ae_wr16(r, ui + O187_UI_START_Z, ae_rd16(r, script + 6u));
        next = script + 8u;
        break;
    case O187_SCRIPT_ACTION_UI_TARGET:
        ae_wr16(r, ui + O187_UI_TARGET_X, ae_rd16(r, script + 2u));
        ae_wr16(r, ui + O187_UI_TARGET_Y, ae_rd16(r, script + 4u));
        ae_wr16(r, ui + O187_UI_TARGET_Z, ae_rd16(r, script + 6u));
        next = script + 8u;
        break;
    case O187_SCRIPT_ACTION_UI_GOAL:
        ae_wr16(r, ui + O187_UI_VALUE_34, ae_rd16(r, script + 2u));
        ae_wr16(r, ui + O187_UI_VALUE_36, ae_rd16(r, script + 4u));
        ae_wr16(r, ui + O187_UI_VALUE_38, ae_rd16(r, script + 6u));
        ae_wr16(r, ui + O187_UI_TIMER_A, ae_rd16(r, script + 8u));
        next = script + 10u;
        break;
    case O187_SCRIPT_ACTION_UI_GOAL2:
        ae_wr16(r, ui + O187_UI_VALUE_3C, ae_rd16(r, script + 2u));
        ae_wr16(r, ui + O187_UI_VALUE_3E, ae_rd16(r, script + 4u));
        ae_wr16(r, ui + O187_UI_VALUE_40, ae_rd16(r, script + 6u));
        ae_wr16(r, ui + O187_UI_TIMER_B, ae_rd16(r, script + 8u));
        next = script + 10u;
        break;
    case O187_SCRIPT_ACTION_OBJECT_TARGET:
        if (parameter < O187_OBJ_SLOTS) {
            ae_wr16(r, object + O187_OBJ_TARGET_X, ae_rd16(r, script + 2u));
            ae_wr16(r, object + O187_OBJ_TARGET_Y, ae_rd16(r, script + 4u));
            ae_wr16(r, object + O187_OBJ_TARGET_Z, ae_rd16(r, script + 6u));
            ae_wr16(r, object + O187_OBJ_MOVE_FRAMES, ae_rd16(r, script + 8u));
            next = script + 10u;
        }
        break;
    case O187_SCRIPT_ACTION_OBJECT_FLAGS:
    case O187_SCRIPT_ACTION_OBJECT_FLAGS_NOW:
        if (parameter < O187_OBJ_SLOTS) {
            ae_wr16(r, object + O187_OBJ_SEQUENCE_FLAGS, ae_rd16(r, script + 2u));
            next = script + 4u;
        }
        break;
    case O187_SCRIPT_ACTION_OBJECT_RANK:
        if (parameter < O187_OBJ_SLOTS)
            ae_wr16(r, object + O187_OBJ_SEQUENCE_CURRENT,
                    ae_rd16(r, ui + O187_UI_RANK));
        break;
    case O187_SCRIPT_ACTION_OBJECT_SEQUENCE:
        if (parameter < O187_OBJ_SLOTS) {
            uint32_t sequence_index = ae_rd16(r, script + 2u);
            uint32_t sequence = ae_rd32(r, object + O187_OBJ_SEQUENCE_POINTER);
            uint32_t entry = sequence + sequence_index * O187_SEQUENCE_ENTRY_STRIDE;
            ae_wr16(r, object + O187_OBJ_SEQUENCE_INDEX, sequence_index);
            ae_wr16(r, object + O187_OBJ_SEQUENCE_CURRENT, sequence_index);
            ae_wr16(r, object + O187_OBJ_SEQUENCE_FRAME,
                    ae_rd16(r, entry + O187_SEQUENCE_ENTRY_DURATION));
        }
        if (parameter < O187_OBJ_SLOTS) next = script + 4u;
        break;
    case O187_SCRIPT_ACTION_WAIT:
        ae_wr32(r, ui + O187_UI_WAIT, (uint32_t)ae_rds16(r, script + 2u));
        next = script + 4u;
        break;
    case O187_SCRIPT_ACTION_WAIT_COUNTER: {
        int32_t wait = ae_rds32(r, ui + O187_UI_WAIT);
        if (wait != 0) {
            ae_wr32(r, ui + O187_UI_WAIT, (uint32_t)(wait - 1));
            *suspended = 1;
        }
        break;
    }
    case O187_SCRIPT_ACTION_WAIT_TICK: {
        uint32_t requested = (uint32_t)ae_rds16(r, script + 2u);
        if (ae_rd32(r, ui + O187_UI_SCRIPT_TICKS) < requested)
            *suspended = 1;
        else
            next = script + 4u;
        break;
    }
    case O187_SCRIPT_ACTION_ICON_FLAG: {
        uint32_t icon = ae_rd16(r, script + 2u);
        ae_wr8(r, O187_UI_OBJECT_FLAGS + icon * O187_UI_OBJECT_FLAG_STRIDE +
                  O187_UI_OBJECT_FLAG_KIND, 1u);
        next = script + 4u;
        break;
    }
    case O187_SCRIPT_ACTION_SAVE_POINTER:
        ae_wr32(r, ui + O187_UI_SCRIPT_RESUME, script + 2u);
        break;
    case O187_SCRIPT_ACTION_RESTORE_POINTER:
        next = ae_rd32(r, ui + O187_UI_SCRIPT_RESUME);
        *suspended = 1;
        break;
    case O187_SCRIPT_ACTION_COPY_STRIP:
        ae_StageClearUi_CopyStrip(r, h, ae_rds16(r, script + 2u));
        next = script + 4u;
        break;
    case O187_SCRIPT_ACTION_PLAY_XA:
        (void)ae_StageClearUi_PlayXa(r, h);
        break;
    case O187_SCRIPT_ACTION_ADVANCE:
        break;
    default:
        ae_ram_fail(r, "StageClear script action table entry %u has unsupported target %08X",
                    parameter, action);
        return script;
    }
    return next;
}

static int32_t ovl187_divide(int32_t numerator, int32_t denominator);
static int32_t ovl187_sub32(int32_t left, int32_t right);

/* 80136000: build the view rotation from the eye/target vectors, store the
 * negated eye translation, then transform it through the view basis. */
uint32_t ae_StageClearUi_LookAt(AeRam *r, const AeOvl187Hooks *h,
                                uint32_t eye, uint32_t target)
{
    uint32_t stack_input = ovl187_frame_address(h, 120u, O187_LOOKAT_INPUT_X);
    uint32_t stack_normal = ovl187_frame_address(h, 120u, O187_LOOKAT_NORMAL_X);
    uint32_t view_matrix = O187_UI_VIEW_MATRIX;
    int32_t eye_x = ae_rds16(r, eye);
    int32_t eye_y = ae_rds16(r, eye + 2u);
    int32_t eye_z = ae_rds16(r, eye + 4u);
    int32_t target_x = ae_rds16(r, target);
    int32_t target_y = ae_rds16(r, target + 2u);
    int32_t target_z = ae_rds16(r, target + 4u);
    if (ae_ram_failed(r)) return 0u;

    ae_wr32(r, view_matrix + O187_MATRIX_TRANSLATION_X, (uint32_t)-eye_x);
    ae_wr32(r, view_matrix + O187_MATRIX_TRANSLATION_Y, (uint32_t)-eye_y);
    ae_wr32(r, view_matrix + O187_MATRIX_TRANSLATION_Z, (uint32_t)-eye_z);

    int32_t delta_x = target_x - eye_x;
    int32_t delta_y = target_y - eye_y;
    int32_t delta_z = target_z - eye_z;
    ae_wr32(r, stack_input + (O187_LOOKAT_INPUT_X - O187_LOOKAT_INPUT_X),
            (uint32_t)delta_x);
    ae_wr32(r, stack_input + (O187_LOOKAT_INPUT_Y - O187_LOOKAT_INPUT_X), 0u);
    ae_wr32(r, stack_input + (O187_LOOKAT_INPUT_Z - O187_LOOKAT_INPUT_X),
            (uint32_t)delta_z);
    if (delta_x == 0 && delta_z == 0)
        ae_wr32(r, stack_input + (O187_LOOKAT_INPUT_Z - O187_LOOKAT_INPUT_X),
                0x1000u);
    (void)ae_sdk_VectorNormal(r, stack_input, stack_normal);
    if (ae_ram_failed(r)) return 0u;

    int32_t horizontal_x = ae_rds32(r, stack_normal +
                                     (O187_LOOKAT_NORMAL_X - O187_LOOKAT_NORMAL_X));
    int32_t horizontal_z = ae_rds32(r, stack_normal +
                                     (O187_LOOKAT_NORMAL_Z - O187_LOOKAT_NORMAL_X));
    ae_wr32(r, stack_input + (O187_LOOKAT_INPUT_Y - O187_LOOKAT_INPUT_X),
            (uint32_t)delta_y);
    (void)ae_sdk_VectorNormal(r, stack_input, stack_normal);
    if (ae_ram_failed(r)) return 0u;

    uint32_t normalized_x = (uint32_t)ae_rds32(
        r, stack_normal + (O187_LOOKAT_NORMAL_X - O187_LOOKAT_NORMAL_X));
    uint32_t normalized_y = (uint32_t)ae_rds32(
        r, stack_normal + (O187_LOOKAT_NORMAL_Y - O187_LOOKAT_NORMAL_X));
    uint32_t normalized_z = (uint32_t)ae_rds32(
        r, stack_normal + (O187_LOOKAT_NORMAL_Z - O187_LOOKAT_NORMAL_X));
    uint32_t horizontal_length_squared = normalized_x * normalized_x +
                                         normalized_z * normalized_z;
    int32_t horizontal_length = ae_sdk_SquareRoot0(
        r, (int32_t)horizontal_length_squared);
    if (ae_ram_failed(r)) return 0u;

    /* Build the 3x3 view basis from the horizontal and full look vectors. */
    int32_t negative_horizontal_x = -horizontal_x;
    int32_t matrix_cross_x = (negative_horizontal_x * (int32_t)normalized_y) >> 12;
    int32_t matrix_cross_y = -((int32_t)normalized_y * horizontal_z) >> 12;
    int32_t matrix_cross_z = -(horizontal_length * negative_horizontal_x) >> 12;
    int32_t matrix_up_z = (horizontal_length * horizontal_z) >> 12;
    ae_wr16(r, view_matrix + 0u, (uint32_t)horizontal_z);
    ae_wr16(r, view_matrix + 2u, 0u);
    ae_wr16(r, view_matrix + 4u, (uint32_t)negative_horizontal_x);
    ae_wr16(r, view_matrix + 6u, (uint32_t)matrix_cross_x);
    ae_wr16(r, view_matrix + 8u, (uint32_t)horizontal_length);
    ae_wr16(r, view_matrix + 10u, (uint32_t)matrix_cross_y);
    ae_wr16(r, view_matrix + 12u, (uint32_t)matrix_cross_z);
    ae_wr16(r, view_matrix + 14u, normalized_y);
    ae_wr16(r, view_matrix + 16u, (uint32_t)matrix_up_z);
    if (ae_ram_failed(r)) return 0u;

    (void)ae_sdk_apply_matrix_lv(ae_ptr(r, view_matrix, 32u),
        ae_ptr(r, view_matrix + O187_MATRIX_TRANSLATION_X, 12u),
        ae_ptr(r, view_matrix + O187_MATRIX_TRANSLATION_X, 12u));
    if (ae_ram_failed(r)) return 0u;

    uint32_t view = ae_sdk_CompMatrix(r, 0x800AF0A0u,
                                      view_matrix, view_matrix);
    if (ae_ram_failed(r)) return 0u;

    uint32_t saved_matrix = ovl187_frame_address(h, 120u,
                                                 O187_LOOKAT_IDENTITY);
    uint32_t stored_matrix = ovl187_frame_address(
        h, 120u, O187_LOOKAT_STORE_TRANSLATION);
    ae_wr32(r, stored_matrix, 0x2000u);
    ae_wr32(r, stored_matrix + 4u, 0x2000u);
    ae_wr32(r, stored_matrix + 8u, 0x2000u);
    (void)ae_sdk_MatrixIdentity(r, saved_matrix);
    if (ae_ram_failed(r)) return 0u;
    uint32_t store_args[] = {saved_matrix, stored_matrix};
    (void)ovl187_call(r, h, "ScaleMatrixL", O187_ADDR_GTE_STORE_MATRIX,
                      2u, store_args);
    if (ae_ram_failed(r)) return 0u;
    return ae_sdk_CompMatrix(r, saved_matrix, view, view);
}

static uint32_t ovl187_rank_for_time(AeRam *r, uint32_t table,
                                     uint32_t time)
{
    uint32_t gold = (uint32_t)ae_rds16(r, table + O187_LEVEL_RECORD_TIME_GOLD);
    uint32_t silver = (uint32_t)ae_rds16(r, table + O187_LEVEL_RECORD_TIME_SILVER);
    uint32_t bronze = (uint32_t)ae_rds16(r, table + O187_LEVEL_RECORD_TIME_BRONZE);

    if (gold < time) return 0u;
    if (silver < time) return 1u;
    if (bronze < time) return 2u;
    return 3u;
}

/* 801361D8: index is zero-based; each preceding string contributes one NUL. */
uint32_t ae_StageClearUi_StringTableIndex(AeRam *r, uint32_t strings,
                                          int32_t index)
{
    if (index <= 0) return strings;

    int32_t skipped = 0;
    while (skipped < index) {
        if (ae_rd8(r, strings) == 0u) ++skipped;
        ++strings;
        if (ae_ram_failed(r)) return 0u;
    }
    return strings;
}

/* 80138A9C: store one iff every positive entry in the requirement list has
 * reached level-progress value two. Entries are signed bytes and stop at 0. */
void ae_StageClearUi_CheckRequirements(AeRam *r, const AeOvl187Hooks *h)
{
    int32_t all_met = 1;
    int32_t level = ae_rds8(r, O187_UI_LEVELS);
    uint32_t address = O187_UI_LEVELS;

    while (level > 0) {
        int32_t progress = ovl187_call1(r, h, "LevelProgressGet",
                                        O187_ADDR_LEVEL_PROGRESS_GET,
                                        (uint32_t)level);
        if (ae_ram_failed(r)) return;
        if (progress < 2) {
            all_met = 0;
            break;
        }
        ++address;
        level = ae_rds8(r, address);
    }

    ae_wr16(r, 0x80139542u, (uint32_t)all_met);
}

/* 80138660: choose the XA index from the area sentinel or the overlay global. */
int32_t ae_StageClearUi_PlayXa(AeRam *r, const AeOvl187Hooks *h)
{
    int32_t area = ae_rds16(r, O187_GAME_STATE + O187_STATE_AREA);
    int32_t index = area == 0xFF ? 0x17 : ae_rds32(r, O187_UI_XA_ID);
    if (index == 0) return 0;
    return ae_XaPlay(r, h != NULL ? h->player1 : NULL, index);
}

/* 801372D0: the native font renderer receives the resolved string and the
 * cursor/spacing values that the game selected for this draw request. */
void ae_StageClearUi_DrawText(AeRam *r, const AeOvl187Hooks *h,
                              uint32_t string)
{
    uint32_t cursor = ae_rd32(r, O187_UI_ORDER_CURSOR);
    int32_t start_x = ae_rds16(r, O187_UI + O187_UI_X);
    int32_t x = start_x;
    int32_t y = ae_rds16(r, O187_UI + O187_UI_Y);
    int32_t spacing = ae_rds32(r, O187_UI + O187_UI_TEXT_SPACING);
    uint32_t character_address = string;

    for (;;) {
        uint32_t character = ae_rd8(r, character_address);
        if (character == 0u) break;

        uint32_t glyph_width = ae_rd8(r, O187_UI_GLYPH_WIDTHS + character);
        int32_t product = (int32_t)(glyph_width * (uint32_t)spacing);
        if (product < 0) product = (int32_t)((uint32_t)product + 15u);
        x = (int32_t)((uint32_t)x + (uint32_t)(product >> 4));
        cursor += 40u;
        ++character_address;
        if (ae_ram_failed(r)) return;
    }

    ae_wr16(r, O187_UI + O187_UI_X, (uint32_t)x);
    uint32_t args[] = {string, ae_rd32(r, O187_UI_ORDER_CURSOR), cursor,
                       (uint32_t)start_x, (uint32_t)y, (uint32_t)spacing};
    (void)ovl187_call(r, h, "RenderStageClearText", 0u, 6u, args);
    if (!ae_ram_failed(r)) ae_wr32(r, O187_UI_ORDER_CURSOR, cursor);
}

/* 801373F8: cursor setup is game state; glyph emission is a renderer call. */
void ae_StageClearUi_DrawMessage(AeRam *r, const AeOvl187Hooks *h,
                                 uint32_t string)
{
    ae_TextSetCursor(r, ae_rds16(r, O187_UI + O187_UI_X),
                     ae_rds16(r, O187_UI + O187_UI_Y));
    if (ae_ram_failed(r)) return;
    (void)ovl187_call1(r, h, "MessageBoxDrawString",
                       O187_ADDR_MESSAGE_BOX_DRAW_STRING, string);
}

/* GPU-only mode functions stay named renderer requests at the native edge. */
void ae_StageClearUi_SetDrawStp(AeRam *r, const AeOvl187Hooks *h)
{
    uint32_t cursor = ae_rd32(r, O187_UI_ORDER_CURSOR);
    uint32_t frame = ae_rd32(r, O187_UI_FRAME_COUNTER);
    (void)ovl187_call2(r, h, "SetDrawStp", O187_ADDR_SET_DRAW_STP,
                       cursor, 1u);
    if (ae_ram_failed(r)) return;
    uint32_t args[] = {cursor, 1u, frame};
    int32_t next = ovl187_call(r, h, "RenderStageClearSetDrawStp", 0u,
                               3u, args);
    if (!ae_ram_failed(r)) ae_wr32(r, O187_UI_ORDER_CURSOR, (uint32_t)next);
}

void ae_StageClearUi_CopyStrip(AeRam *r, const AeOvl187Hooks *h,
                               int32_t index)
{
    uint32_t cursor = ae_rd32(r, O187_UI_ORDER_CURSOR);
    uint32_t args[] = {(uint32_t)index, cursor, 16u, 64u, 560u,
                       (uint32_t)(index * 64 + 64)};
    int32_t next = ovl187_call(r, h, "RenderStageClearStrip", 0u,
                               6u, args);
    if (!ae_ram_failed(r)) ae_wr32(r, O187_UI_ORDER_CURSOR, (uint32_t)next);
}

void ae_StageClearUi_DrawBlinkIcons(AeRam *r, const AeOvl187Hooks *h)
{
    uint32_t args[] = {
        ae_rd32(r, O187_UI_ORDER_CURSOR),
        ae_rd32(r, O187_UI_FRAME_COUNTER),
        ae_rd16(r, O187_GAME_STATE + O187_STATE_STAGE_FLAGS),
        ae_rd32(r, O187_UI + O187_UI_REQUIREMENTS),
        ae_rd32(r, O187_UI + O187_UI_DATA),
        ae_rd32(r, O187_UI + O187_UI_COLOR)
    };
    int32_t next = ovl187_call(r, h, "RenderStageClearBlinkIcons", 0u,
                               6u, args);
    if (!ae_ram_failed(r)) ae_wr32(r, O187_UI_ORDER_CURSOR, (uint32_t)next);
}

void ae_StageClearUi_DrawBox(AeRam *r, const AeOvl187Hooks *h,
                             int32_t x, int32_t y, int32_t width,
                             int32_t height)
{
    uint32_t args[] = {(uint32_t)x, (uint32_t)y, (uint32_t)width,
                       (uint32_t)height, ae_rd32(r, O187_UI_ORDER_CURSOR)};
    int32_t next = ovl187_call(r, h, "RenderStageClearBox", 0u, 5u, args);
    if (!ae_ram_failed(r)) ae_wr32(r, O187_UI_ORDER_CURSOR, (uint32_t)next);
}

void ae_StageClearUi_DrawBackdrop(AeRam *r, const AeOvl187Hooks *h)
{
    uint32_t first_draw = O187_WIPE_FIRST_DRAW;
    if (ae_rd32(r, first_draw) == 0u) ae_wr32(r, first_draw, 1u);
    int32_t level = ae_rds16(r, O187_GAME_STATE + O187_STATE_LEVEL);
    int32_t progress = ovl187_call1(r, h, "LevelProgressGet",
                                    O187_ADDR_LEVEL_PROGRESS_GET,
                                    (uint32_t)level);
    if (ae_ram_failed(r)) return;
    uint32_t args[] = {
        ae_rd32(r, O187_UI_ORDER_CURSOR),
        ae_rd32(r, O187_UI_FRAME_COUNTER),
        ae_rd16(r, O187_GAME_STATE + O187_STATE_MODE),
        (uint32_t)level,
        (uint32_t)progress
    };
    int32_t next = ovl187_call(r, h, "RenderStageClearBackdrop", 0u,
                               5u, args);
    if (ae_ram_failed(r)) return;
    ae_wr32(r, O187_UI_ORDER_CURSOR, (uint32_t)next);
    ae_StageClearUi_DrawBox(r, h, 24, 22, 272, 48);
}

void ae_StageClearUi_DrawScanlines(AeRam *r, const AeOvl187Hooks *h)
{
    uint32_t cursor = ae_rd32(r, O187_UI_ORDER_CURSOR);
    uint32_t frame = ae_rd32(r, O187_UI_FRAME_COUNTER);
    int32_t old_timer = ae_rds32(r, O187_SCANLINE_TIMER);
    uint16_t phase = ae_rd16(r, O187_SCANLINE_PHASE);
    int32_t brightness_limit = 0;
    int32_t duration = 4;

    ae_wr32(r, O187_SCANLINE_TIMER, (uint32_t)(old_timer - 1));
    if (old_timer < 11) brightness_limit = 10;
    if (old_timer <= 0) {
        uint16_t advanced = (uint16_t)((uint32_t)phase * 5u + 1u);
        phase = advanced;
        uint32_t high_byte = (uint32_t)phase >> 8;
        duration = (int32_t)(high_byte % 200u) + 20;
        ae_wr32(r, O187_SCANLINE_TIMER, (uint32_t)duration);
        ae_wr16(r, O187_SCANLINE_PHASE, phase);
    }

    uint32_t dynamic_packets = 0;
    uint32_t object_count = ae_rd32(r, O187_GAME_OBJECT_COUNT);
    for (uint32_t line = 0; line < 120u; ++line) {
        uint16_t advanced = (uint16_t)((uint32_t)phase * 5u + 1u);
        phase = advanced;
        uint32_t high_byte = (uint32_t)phase >> 8;
        if (brightness_limit < (int32_t)(high_byte % 100u)) continue;

        advanced = (uint16_t)((uint32_t)phase * 5u + 1u);
        phase = advanced;
        high_byte = (uint32_t)phase >> 8;
        int32_t half_width = (int32_t)(high_byte % (uint32_t)duration) +
                             duration / 2;
        if (half_width > 0) dynamic_packets += 2u;
        (void)object_count;
    }
    ae_wr16(r, O187_SCANLINE_PHASE, phase);
    if (ae_ram_failed(r)) return;

    uint32_t next = cursor + 120u * 168u + dynamic_packets * 24u;
    uint32_t args[] = {
        cursor, frame, (uint32_t)old_timer, (uint32_t)duration,
        phase, object_count, next
    };
    (void)ovl187_call(r, h, "RenderStageClearScanlines", 0u,
                      7u, args);
    if (!ae_ram_failed(r)) ae_wr32(r, O187_UI_ORDER_CURSOR, next);
}

static void ovl187_interpolate_ui_group(AeRam *r, uint32_t ui,
                                        uint32_t current_fields,
                                        uint32_t target_fields,
                                        uint32_t frame_field)
{
    int32_t frames = ae_rds16(r, ui + frame_field);
    if (frames <= 0) return;

    for (uint32_t axis = 0; axis < 3u; ++axis) {
        uint32_t current = current_fields + axis * 2u;
        uint32_t target = target_fields + axis * 2u;
        int32_t delta = ovl187_sub32(ae_rds16(r, ui + target),
                                     ae_rds16(r, ui + current));
        int32_t step = ovl187_divide(delta, frames);
        uint32_t value = ae_rd16(r, ui + current) + (uint32_t)step;
        ae_wr16(r, ui + current, value);
    }
    ae_wr16(r, ui + frame_field, (uint32_t)(frames - 1));
}

/* 801374C8: execute the stage-clear script until a wait, a tick gate, a
 * restore-pointer command, or the terminator suspends this frame. */
void ae_StageClearUi_RunScript(AeRam *r, const AeOvl187Hooks *h)
{
    uint32_t ui = O187_UI;
    AeOvl187Hooks child_hooks;
    ovl187_nested_hooks(&child_hooks, h, 48u);
    uint32_t flags = ae_rd32(r, ui + O187_UI_SCRIPT_FLAGS);
    int32_t ticks = ae_rds32(r, ui + O187_UI_SCRIPT_TICKS);
    if ((flags & 1u) != 0u) {
        /* The original loads 0xFFFE with ori, so signed slt compares the
         * positive value 65534 against the 32-bit tick counter. */
        if (!(65534 < ticks)) {
            ++ticks;
            ae_wr32(r, ui + O187_UI_SCRIPT_TICKS, (uint32_t)ticks);
        }
        ae_wr32(r, ui + O187_UI_SCRIPT_FLAGS, flags | 2u);
    }

    uint32_t script = ae_rd32(r, ui + O187_UI_SCRIPT);
    int suspended = 0;
    for (;;) {
        int32_t command_word = ae_rds16(r, script);
        if (ae_ram_failed(r)) return;
        if (command_word == O187_SCRIPT_END) break;

        uint32_t encoded = (uint32_t)command_word;
        uint32_t opcode = encoded & O187_SCRIPT_OPCODE_MASK;
        uint32_t parameter = encoded & O187_SCRIPT_PARAMETER_MASK;
        uint32_t next = script + 2u;

        switch (opcode) {
        case O187_SCRIPT_SET_OBJECT_FLAGS:
            if (parameter < O187_OBJ_SLOTS) {
                uint32_t object = ovl187_object(O187_UI_OBJECTS,
                                                (int32_t)parameter);
                ae_wr16(r, object + O187_OBJ_SEQUENCE_FLAGS,
                        ae_rd16(r, script + 2u));
                next += 2u;
            }
            break;

        case O187_SCRIPT_ACTION_TABLE: {
            if (parameter < 20u)
                next = ovl187_run_script_action(r, &child_hooks, script, parameter,
                                                &suspended);
            break;
        }

        case O187_SCRIPT_SET_OBJECT_TARGET:
            if (parameter < O187_OBJ_SLOTS) {
                uint32_t object = ovl187_object(O187_UI_OBJECTS,
                                                (int32_t)parameter);
                ae_wr16(r, object + O187_OBJ_TARGET_X,
                        ae_rd16(r, script + 2u));
                ae_wr16(r, object + O187_OBJ_TARGET_Y,
                        ae_rd16(r, script + 4u));
                ae_wr16(r, object + O187_OBJ_TARGET_Z,
                        ae_rd16(r, script + 6u));
                ae_wr16(r, object + O187_OBJ_MOVE_FRAMES,
                        ae_rd16(r, script + 8u));
                next += 8u;
            }
            break;

        case O187_SCRIPT_SET_ICON_FLAGS: {
            uint32_t item = ae_rd16(r, script + 2u);
            ae_wr8(r, O187_UI_OBJECT_FLAGS +
                      item * O187_UI_OBJECT_FLAG_STRIDE +
                      O187_UI_OBJECT_FLAG_ACTIVE, 1u);
            next += 2u;
            break;
        }

        case O187_SCRIPT_SET_OBJECT_RANK_IMMEDIATE:
            if (parameter < O187_OBJ_SLOTS) {
                uint32_t object = ovl187_object(O187_UI_OBJECTS,
                                                (int32_t)parameter);
                ae_wr16(r, object + O187_OBJ_SEQUENCE_CURRENT,
                        ae_rd16(r, script + 2u));
                next += 2u;
            }
            break;

        case O187_SCRIPT_SET_OBJECT_SEQUENCE:
            if (parameter < O187_OBJ_SLOTS) {
                uint32_t object = ovl187_object(O187_UI_OBJECTS,
                                                (int32_t)parameter);
                uint32_t sequence_index = ae_rd16(r, script + 2u);
                uint32_t sequence = ae_rd32(r,
                    object + O187_OBJ_SEQUENCE_POINTER);
                uint32_t entry = sequence + sequence_index *
                                 O187_SEQUENCE_ENTRY_STRIDE;
                ae_wr16(r, object + O187_OBJ_SEQUENCE_INDEX, sequence_index);
                ae_wr16(r, object + O187_OBJ_SEQUENCE_CURRENT, sequence_index);
                ae_wr16(r, object + O187_OBJ_SEQUENCE_FRAME,
                        ae_rd16(r, entry + O187_SEQUENCE_ENTRY_DURATION));
                next += 2u;
            }
            break;

        case O187_SCRIPT_SET_WAIT:
            ae_wr32(r, ui + O187_UI_WAIT,
                    (uint32_t)ae_rds16(r, script + 2u));
            next += 2u;
            break;

        case O187_SCRIPT_WAIT: {
            int32_t wait = ae_rds32(r, ui + O187_UI_WAIT);
            if (wait != 0) {
                ae_wr32(r, ui + O187_UI_WAIT, (uint32_t)(wait - 1));
                suspended = 1;
            } else {
                next += 0u;
            }
            break;
        }

        case O187_SCRIPT_SAVE_POINTER:
            ae_wr32(r, ui + O187_UI_SCRIPT_RESUME, next);
            break;

        case O187_SCRIPT_RESTORE_POINTER:
            script = ae_rd32(r, ui + O187_UI_SCRIPT_RESUME);
            suspended = 1;
            break;

        case O187_SCRIPT_COPY_STRIP:
            ae_StageClearUi_CopyStrip(r, &child_hooks,
                                      ae_rds16(r, script + 2u));
            next += 2u;
            break;

        case O187_SCRIPT_SET_FLAGS:
            ae_wr32(r, ui + O187_UI_SCRIPT_FLAGS, 1u);
            break;

        case O187_SCRIPT_WAIT_FOR_TICK: {
            uint32_t requested_tick = (uint32_t)ae_rds16(r, script + 2u);
            uint32_t script_ticks = ae_rd32(r, ui + O187_UI_SCRIPT_TICKS);
            if (script_ticks < requested_tick) suspended = 1;
            else next += 2u;
            break;
        }

        case O187_SCRIPT_PLAY_XA:
            (void)ae_StageClearUi_PlayXa(r, &child_hooks);
            break;

        case O187_SCRIPT_SET_OBJECT_RANK:
            if (parameter < O187_OBJ_SLOTS) {
                uint32_t object = ovl187_object(O187_UI_OBJECTS,
                                                (int32_t)parameter);
                ae_wr16(r, object + O187_OBJ_SEQUENCE_CURRENT,
                        ae_rd16(r, ui + O187_UI_RANK));
            }
            break;

        default:
            break;
        }
        if (ae_ram_failed(r)) return;
        script = suspended ? script : next;
        if (suspended) break;
    }

    ae_wr32(r, ui + O187_UI_SCRIPT, script);
    ovl187_interpolate_ui_group(r, ui, O187_UI_START_X, O187_UI_VALUE_34,
                                O187_UI_TIMER_A);
    ovl187_interpolate_ui_group(r, ui, O187_UI_TARGET_X, O187_UI_VALUE_3C,
                                O187_UI_TIMER_B);
    if (ae_ram_failed(r)) return;

    uint32_t flag = O187_UI_OBJECT_FLAGS;
    for (;;) {
        int32_t active = ae_rds8(r, flag + O187_UI_OBJECT_FLAG_ACTIVE);
        if (active < 0) break;
        if (active == 0) {
            flag += O187_UI_OBJECT_FLAG_STRIDE;
            continue;
        }
        int32_t kind = ae_rds8(r, flag + O187_UI_OBJECT_FLAG_KIND);
        uint32_t x = ae_rd16(r, flag + O187_UI_OBJECT_FLAG_X);
        uint32_t y = ae_rd16(r, flag + O187_UI_OBJECT_FLAG_Y);
        int32_t string_index = ae_rds16(r, flag + O187_UI_OBJECT_FLAG_VALUE);
        uint32_t handler = (uint32_t)kind < 6u ?
            ae_rd32(r, O187_UI_TEXT_HANDLER_TABLE + (uint32_t)kind * 4u) : 0u;
        ae_wr16(r, ui + O187_UI_X, x);
        ae_wr16(r, ui + O187_UI_Y, y);
        switch (handler) {
        case O187_UI_TEXT_HANDLER_SPACING_32:
            ae_wr32(r, ui + O187_UI_TEXT_SPACING, 32u);
            ae_StageClearUi_DrawText(r, &child_hooks,
                ae_StageClearUi_StringTableIndex(r, O187_UI_STRINGS,
                                                  string_index));
            break;
        case O187_UI_TEXT_HANDLER_MESSAGE:
            ae_StageClearUi_DrawMessage(r, &child_hooks,
                ae_StageClearUi_StringTableIndex(r, O187_UI_STRINGS,
                                                  string_index));
            break;
        case O187_UI_TEXT_HANDLER_BLINK:
            ae_StageClearUi_DrawBlinkIcons(r, &child_hooks);
            break;
        case O187_UI_TEXT_HANDLER_BACKDROP:
            ae_StageClearUi_DrawBackdrop(r, &child_hooks);
            break;
        case O187_UI_TEXT_HANDLER_SPACING_16:
            ae_wr32(r, ui + O187_UI_TEXT_SPACING, 16u);
            ae_StageClearUi_DrawText(r, h,
                ae_StageClearUi_StringTableIndex(r, O187_UI_STRINGS,
                                                  string_index));
            break;
        case O187_UI_TEXT_HANDLER_TIMES_OR_STARS:
            if (ae_rd16(r, O187_GAME_STATE + O187_STATE_MODE) == 25u)
                ae_StageClearUi_DrawStars(r, &child_hooks);
            else
                ae_StageClearUi_DrawTimes(r, &child_hooks);
            break;
        default:
            break;
        }
        if (ae_ram_failed(r)) return;
        flag += O187_UI_OBJECT_FLAG_STRIDE;
    }
    return;
}

/* 80138B0C: create a 16 by 12 polar grid in the original area heap. */
uint32_t ae_RippleGrid_Init(AeRam *r, const AeOvl187Hooks *h)
{
    uint32_t grid = ae_HeapAlloc(r, h != NULL ? h->area : NULL,
                                 O187_RIPPLE_VERTEX_BYTES);
    if (ae_ram_failed(r)) return 0u;
    ae_wr32(r, O187_UI_RIPPLE_POINTER, grid);

    for (int32_t row = 0; row < O187_RIPPLE_ROWS; ++row) {
        uint32_t radial_product = (uint32_t)(300 * row * row);
            int32_t radius = (int32_t)(radial_product / 144u);
        uint32_t angle = (uint32_t)row * 128u;
        for (int32_t column = 0; column < O187_RIPPLE_COLUMNS; ++column) {
            uint32_t vertex = grid + (uint32_t)(row * O187_RIPPLE_COLUMNS + column) *
                                     O187_RIPPLE_VERTEX_STRIDE;
            int32_t x = (ae_sdk_rcos(r, (int32_t)angle) * radius >> 12) + 192;
            int32_t y = (ae_sdk_rsin(r, (int32_t)angle) * radius >> 12) + 120;
            ae_wr16(r, vertex, (uint32_t)x);
            ae_wr16(r, vertex + 2u, (uint32_t)y);
            ae_wr32(r, vertex + O187_RIPPLE_COLOR, 0u);
            angle += 256u;
        }
    }
    return 0u;
}

/* 80138C24: blur each odd row from its adjacent even row, wrap columns,
 * clear row zero, then add three phase-shifted sine lanes to that row. */
void ae_RippleGrid_Smooth(AeRam *r)
{
    uint32_t grid = ae_rd32(r, O187_UI_RIPPLE_POINTER);
    for (int32_t row = O187_RIPPLE_ROWS - 1; row >= 1; row -= 2) {
        uint32_t source_row = grid + (uint32_t)(row - 1) *
                                      O187_RIPPLE_ROW_BYTES;
        uint32_t output_row = grid + (uint32_t)row * O187_RIPPLE_ROW_BYTES;
        for (int32_t column = 0; column < O187_RIPPLE_COLUMNS; ++column) {
            int32_t next_column = column == 15 ? 0 : column + 1;
            int32_t previous_column = column == 0 ? 15 : column - 1;
            uint32_t next_vertex = source_row + (uint32_t)next_column *
                                              O187_RIPPLE_VERTEX_STRIDE;
            uint32_t previous_vertex = source_row + (uint32_t)previous_column *
                                                  O187_RIPPLE_VERTEX_STRIDE;
            uint32_t source_vertex = source_row + (uint32_t)column *
                                                O187_RIPPLE_VERTEX_STRIDE;
            uint32_t output_vertex = output_row + (uint32_t)column *
                                                O187_RIPPLE_VERTEX_STRIDE;
            for (uint32_t lane = 0; lane < 3u; ++lane) {
                uint32_t average = ae_rd8(r, next_vertex + O187_RIPPLE_COLOR_RED + lane) +
                                   ae_rd8(r, previous_vertex + O187_RIPPLE_COLOR_RED + lane) +
                                   2u * ae_rd8(r, source_vertex + O187_RIPPLE_COLOR_RED + lane);
                ae_wr8(r, output_vertex + O187_RIPPLE_COLOR_RED + lane,
                       average >> 2);
            }
        }
    }

    uint32_t first_row_last_vertex = grid + 15u * O187_RIPPLE_VERTEX_STRIDE;
    for (int32_t column = O187_RIPPLE_COLUMNS - 1; column >= 0; --column) {
        ae_wr32(r, first_row_last_vertex + O187_RIPPLE_COLOR, 0u);
        first_row_last_vertex -= O187_RIPPLE_VERTEX_STRIDE;
    }

    uint32_t frame = ae_rd32(r, O187_UI_FRAME_COUNTER);
    if ((frame & 0xFFu) == 0u) return;

    uint32_t vertex = grid;
    uint32_t angle = frame << 5;
    for (int32_t column = 0; column < O187_RIPPLE_COLUMNS; ++column) {
        int32_t wave = ae_sdk_rsin(r, (int32_t)angle) + 4096;
        uint32_t contribution = ((uint32_t)wave << 2) & 0xFF00u;
        ae_wr32(r, vertex + O187_RIPPLE_COLOR,
                ae_rd32(r, vertex + O187_RIPPLE_COLOR) + contribution);
        angle += 256u;
        vertex += O187_RIPPLE_VERTEX_STRIDE;
    }

    vertex = grid;
    angle = (frame << 5) + 1365u;
    for (int32_t column = 0; column < O187_RIPPLE_COLUMNS; ++column) {
        int32_t wave = ae_sdk_rsin(r, (int32_t)angle) + 4096;
        uint32_t contribution = ((uint32_t)(wave >> 6)) & 0xFFu;
        ae_wr32(r, vertex + O187_RIPPLE_COLOR,
                ae_rd32(r, vertex + O187_RIPPLE_COLOR) + contribution);
        angle += 256u;
        vertex += O187_RIPPLE_VERTEX_STRIDE;
    }

    vertex = grid;
    angle = (frame << 5) + 2730u;
    for (int32_t column = 0; column < O187_RIPPLE_COLUMNS; ++column) {
        int32_t wave = ae_sdk_rsin(r, (int32_t)angle) + 4096;
        uint32_t contribution = (((uint32_t)(wave >> 6)) & 0xFFu) << 16;
        ae_wr32(r, vertex + O187_RIPPLE_COLOR,
                ae_rd32(r, vertex + O187_RIPPLE_COLOR) + contribution);
        angle += 256u;
        vertex += O187_RIPPLE_VERTEX_STRIDE;
    }
}

/* Ripple vertices are game-computed; only the quad packet construction and
 * ordering-table insertion cross the renderer boundary. */
void ae_RippleGrid_Draw(AeRam *r, const AeOvl187Hooks *h)
{
    ae_RippleGrid_Smooth(r);
    if (ae_ram_failed(r)) return;
    uint32_t args[] = {
        ae_rd32(r, O187_UI_RIPPLE_POINTER),
        ae_rd32(r, O187_UI_ORDER_CURSOR),
        ae_rd32(r, O187_UI_FRAME_COUNTER),
        O187_RIPPLE_COLUMNS,
        O187_RIPPLE_ROWS - 1u,
        ae_rd32(r, O187_UI + O187_UI_DATA)
    };
    int32_t next = ovl187_call(r, h, "RenderRippleGrid", 0u, 6u, args);
    if (!ae_ram_failed(r) && (uint32_t)next != 0u) {
        ae_wr32(r, O187_UI_ORDER_CURSOR, (uint32_t)next);
    }
}

/* 80138F38: seed result fields and select the text/rank for modes 12, 20, 25. */
void ae_StageClearUi_Setup(AeRam *r, const AeOvl187Hooks *h)
{
    ae_wr32(r, O187_UI + O187_UI_REQUIREMENTS, 0u);
    ae_wr32(r, O187_UI + O187_UI_RANK_TEXT, 0u);
    ae_wr32(r, O187_UI + O187_UI_RANK, 0u);

    uint32_t mode = ae_rd16(r, O187_GAME_STATE + O187_STATE_MODE);
    if (mode == 12u || mode == 20u) {
        if (ovl187_call1(r, h, "LevelProgressGet",
                         O187_ADDR_LEVEL_PROGRESS_GET, 0x11u) > 0) {
            if (ae_ProgressGetByte(r, 4u) != 5u) {
                ae_wr32(r, O187_UI + O187_UI_REQUIREMENTS,
                        ae_rd32(r, O187_UI + O187_UI_REQUIREMENTS) | 1u);
            }
        }
        if (ovl187_call1(r, h, "LevelProgressGet",
                         O187_ADDR_LEVEL_PROGRESS_GET, 0x11u) > 0) {
            if (ae_ProgressGetByte(r, 5u) != 5u) {
                ae_wr32(r, O187_UI + O187_UI_REQUIREMENTS,
                        ae_rd32(r, O187_UI + O187_UI_REQUIREMENTS) | 2u);
            }
        }
    }
    if (ae_ram_failed(r)) return;

    if (mode == 12u) {
        uint32_t requirements = ae_rd32(r, O187_UI + O187_UI_REQUIREMENTS);
        if ((requirements & 2u) != 0u) return;
        int32_t progress = ovl187_call1(
            r, h, "LevelProgressGet", O187_ADDR_LEVEL_PROGRESS_GET,
            ae_rd16(r, O187_GAME_STATE + O187_STATE_LEVEL));
        if (ae_ram_failed(r)) return;
        ae_wr32(r, O187_UI + O187_UI_RANK_TEXT,
                progress < 2 ? 31u : 32u);
        return;
    }

    if (mode == 20u) {
        int32_t area = ae_rds16(r, O187_GAME_STATE + O187_STATE_AREA);
        int32_t level = ae_AreaGetLevel(r, h != NULL ? h->area : NULL, area);
        uint32_t table = (uint32_t)ovl187_call1(
            r, h, "LevelTableGet", O187_ADDR_LEVEL_TABLE_GET,
            (uint32_t)level);
        if (ae_ram_failed(r)) return;

        uint32_t current = ae_rd32(r, O187_GAME_STATE +
                                      O187_STATE_CURRENT_TIME);
        uint32_t best = ae_rd32(r, O187_GAME_STATE + O187_STATE_BEST_TIME);
        uint32_t gold_time = (uint32_t)ae_rds16(
            r, table + O187_LEVEL_RECORD_TIME_GOLD);
        if (gold_time < current) {
            if ((ae_rd32(r, O187_UI + O187_UI_REQUIREMENTS) & 2u) == 0u)
                ae_wr32(r, O187_UI + O187_UI_RANK, 4u);
            ae_wr32(r, O187_UI + O187_UI_RANK_TEXT, 37u);
            return;
        }
        uint32_t current_rank = ovl187_rank_for_time(r, table, current);
        uint32_t best_rank = ovl187_rank_for_time(r, table, best);

        if (best_rank < current_rank) {
            if (current_rank == 1u) {
                ae_wr32(r, O187_UI + O187_UI_RANK_TEXT, 33u);
                ae_wr32(r, O187_UI + O187_UI_RANK, 5u);
            } else if (current_rank == 2u) {
                ae_wr32(r, O187_UI + O187_UI_RANK_TEXT, 34u);
                ae_wr32(r, O187_UI + O187_UI_RANK, 6u);
            } else {
                ae_wr32(r, O187_UI + O187_UI_RANK_TEXT, 35u);
                ae_wr32(r, O187_UI + O187_UI_RANK, 7u);
            }
        } else {
            if (current < best) {
                if ((ae_rd32(r, O187_UI + O187_UI_REQUIREMENTS) & 2u) == 0u) {
                    ae_wr32(r, O187_UI + O187_UI_RANK_TEXT, 36u);
                }
                ae_wr32(r, O187_UI + O187_UI_RANK, 3u);
            } else {
                ae_wr32(r, O187_UI + O187_UI_RANK_TEXT, 0u);
                ae_wr32(r, O187_UI + O187_UI_RANK, 8u);
            }
        }
        if ((ae_rd32(r, O187_UI + O187_UI_REQUIREMENTS) & 2u) != 0u)
            ae_wr32(r, O187_UI + O187_UI_RANK_TEXT, 0u);
        return;
    }

    if (mode == 25u) {
        uint32_t exit = ae_rd16(r, O187_GAME_STATE + O187_STATE_EXIT);
        ae_wr32(r, O187_UI_XA_ID, exit == 8u ? 42u : 41u);
    }
}

/* Signed division in MIPS truncates toward zero; the positive denominators
 * below make the C99 operation equivalent for all reachable interpolation. */
static int32_t ovl187_divide(int32_t numerator, int32_t denominator)
{
    return numerator / denominator;
}

static int32_t ovl187_sub32(int32_t left, int32_t right)
{
    return (int32_t)((uint32_t)left - (uint32_t)right);
}

/* 80137F4C: interpolate five scripted object poses, advance their animation
 * records, build their matrices, and submit renderer requests. */
void ae_StageClearUi_UpdateObjects(AeRam *r, const AeOvl187Hooks *h,
                                   uint32_t objects)
{
    uint32_t requirements = ae_rd32(r, O187_UI + O187_UI_REQUIREMENTS);

    for (int32_t index = 0; index < O187_OBJ_SLOTS; ++index) {
        uint32_t object = ovl187_object(objects, index);
        uint32_t active = ae_rd16(r, object + O187_OBJ_SEQUENCE_FLAGS) & 1u;
        if (active == 0u || (requirements & (1u << index)) != 0u) continue;

        int32_t frames = ae_rds16(r, object + O187_OBJ_MOVE_FRAMES);
        if (frames > 0) {
            int32_t dx = ovl187_divide(
                ovl187_sub32(ae_rds16(r, object + O187_OBJ_TARGET_X),
                             ae_rds16(r, object + O187_OBJ_POSITION_X)), frames);
            int32_t dy = ovl187_divide(
                ovl187_sub32(ae_rds16(r, object + O187_OBJ_TARGET_Y),
                             ae_rds16(r, object + O187_OBJ_POSITION_Y)), frames);
            int32_t dz = ovl187_divide(
                ovl187_sub32(ae_rds16(r, object + O187_OBJ_TARGET_Z),
                             ae_rds16(r, object + O187_OBJ_POSITION_Z)), frames);
            --frames;
            ae_wr16(r, object + O187_OBJ_MOVE_FRAMES, (uint32_t)frames);
            ae_wr16(r, object + O187_OBJ_POSITION_X,
                    ae_rd16(r, object + O187_OBJ_POSITION_X) + (uint32_t)dx);
            ae_wr16(r, object + O187_OBJ_POSITION_Y,
                    ae_rd16(r, object + O187_OBJ_POSITION_Y) + (uint32_t)dy);
            ae_wr16(r, object + O187_OBJ_POSITION_Z,
                    ae_rd16(r, object + O187_OBJ_POSITION_Z) + (uint32_t)dz);
        }

        int32_t rotation_frames = ae_rds16(r, object + O187_OBJ_ROT_FRAMES);
        if (rotation_frames > 0) {
            int32_t dx = ovl187_divide(
                ovl187_sub32(ae_rds16(r, object + O187_OBJ_TARGET_ROT_X),
                             ae_rds32(r, object + O187_OBJ_ROT_X)),
                rotation_frames);
            int32_t dy = ovl187_divide(
                ovl187_sub32(ae_rds16(r, object + O187_OBJ_TARGET_ROT_Y),
                             ae_rds32(r, object + O187_OBJ_ROT_Y)),
                rotation_frames);
            int32_t dz = ovl187_divide(
                ovl187_sub32(ae_rds16(r, object + O187_OBJ_TARGET_ROT_Z),
                             ae_rds32(r, object + O187_OBJ_ROT_Z)),
                rotation_frames);
            --rotation_frames;
            ae_wr16(r, object + O187_OBJ_ROT_FRAMES,
                    (uint32_t)rotation_frames);
            ae_wr32(r, object + O187_OBJ_ROT_X,
                    ae_rd32(r, object + O187_OBJ_ROT_X) +
                    (uint32_t)dx);
            ae_wr32(r, object + O187_OBJ_ROT_Y,
                    ae_rd32(r, object + O187_OBJ_ROT_Y) +
                    (uint32_t)dy);
            ae_wr32(r, object + O187_OBJ_ROT_Z,
                    ae_rd32(r, object + O187_OBJ_ROT_Z) +
                    (uint32_t)dz);
        }

        uint32_t direction_out = ovl187_frame_address(h, 48u, 16u);
        (void)ae_sdk_VectorNormalSS(r, object + O187_OBJ_POSITION_X,
                                    direction_out);
        uint32_t rotation = ae_rd32(r, object + O187_OBJ_ROTATION);
        (void)ae_sdk_MatrixFromDirXZ(r, direction_out, rotation);

        uint32_t sequence_frame = ae_rd16(r, object + O187_OBJ_SEQUENCE_FRAME) + 1u;
        ae_wr16(r, object + O187_OBJ_SEQUENCE_FRAME, sequence_frame);
        uint32_t sequence = ae_rd32(r, object + O187_OBJ_SEQUENCE_POINTER);
        uint32_t sequence_index = ae_rd16(r, object + O187_OBJ_SEQUENCE_INDEX);
        uint32_t next_frame_address = sequence +
            sequence_index * O187_SEQUENCE_ENTRY_STRIDE +
            O187_SEQUENCE_ENTRY_FRAME;
        int32_t frame_limit = ae_rds16(r, next_frame_address);
        if (frame_limit < (int32_t)sequence_frame) {
            uint32_t current_index = ae_rd16(r, object + O187_OBJ_SEQUENCE_CURRENT);
            uint32_t entry = sequence + current_index *
                             O187_SEQUENCE_ENTRY_STRIDE;
            uint32_t duration = ae_rd16(r, entry + O187_SEQUENCE_ENTRY_DURATION);
            ae_wr16(r, object + O187_OBJ_SEQUENCE_INDEX, current_index);
            ae_wr16(r, object + O187_OBJ_SEQUENCE_FRAME, duration);
        }

        ae_ModelPoseBones(r, object, 1u);
        if (ae_ram_failed(r)) return;
        (void)ovl187_call1(r, h, "ModelTransformSkinned",
                           O187_ADDR_MODEL_TRANSFORM_SKINNED, object);
        if (ae_ram_failed(r)) return;

        uint32_t draw_index = ae_rd32(r, O187_GAME_OBJECT_COUNT);
        uint32_t ordering_entry = O187_UI_MODEL_OT_BASE +
                                  draw_index * O187_MODE_OT_ENTRY_STRIDE;
        uint32_t vertices = O187_UI_VIEW_VERTICES;
        (void)ovl187_call3(r, h, "ModelDraw", O187_ADDR_MODEL_DRAW,
                           ordering_entry, vertices, object);
        if (ae_ram_failed(r)) return;
    }
}

/* 801392B0: form the translation/rotation transform, then pass the logical
 * model and vertex buffer to the renderer's rigid projection/draw boundary. */
uint32_t ae_StageClearUi_ObjMatrix(AeRam *r, const AeOvl187Hooks *h,
                                   int32_t index, uint32_t pose)
{
    const uint32_t matrix = ovl187_frame_address(h, 64u, 16u);
    (void)ae_sdk_MatrixIdentity(r, matrix);
    ae_wr32(r, matrix + O187_MATRIX_TRANSLATION_X,
            (uint32_t)ae_rds16(r, pose + O187_POSE_X));
    ae_wr32(r, matrix + O187_MATRIX_TRANSLATION_Y,
            (uint32_t)ae_rds16(r, pose + O187_POSE_Y));
    ae_wr32(r, matrix + O187_MATRIX_TRANSLATION_Z,
            (uint32_t)ae_rds16(r, pose + O187_POSE_Z));

    uint32_t angle = ae_rd8(r, pose + O187_POSE_ANGLE) << 6;
    ae_MatrixRotYMul(r, h != NULL ? h->misc : NULL,
                     (int32_t)angle, matrix);
    if (ae_ram_failed(r)) return 0u;
    (void)ae_sdk_CompMatrix(r, O187_UI_VIEW_MATRIX, matrix, matrix);
    if (ae_ram_failed(r)) return 0u;

    uint32_t model_record = O187_UI_RENDER_OBJECTS +
                            (uint32_t)index * O187_MODEL_RECORD_STRIDE;
    uint32_t model = ae_rd32(r, model_record + O187_MODEL_RECORD_HEADER);
    uint32_t vertices = ae_rd16(r, model + O187_MODEL_HEADER_ENV_FLAG) != 0u ?
                        O187_SCRATCH_VERTICES : O187_UI_VIEW_VERTICES;
    (void)ovl187_call3(r, h, "ModelTransformRigid",
                       O187_ADDR_MODEL_TRANSFORM_RIGID,
                       model_record, matrix, vertices);
    if (ae_ram_failed(r)) return 0u;
    ae_MeshCalcEnvUVs(r, h != NULL ? h->misc : NULL, model_record);
    if (ae_ram_failed(r)) return 0u;
    ae_wr8(r, O187_SCRATCH_ENV_MAP_MODE, 2u);
    return (uint32_t)ovl187_call3(r, h, "ModelDrawEnvMap",
                                  O187_ADDR_MODEL_DRAW_ENV_MAP,
                                  ae_rd32(r, O187_UI_ORDER_TABLE), vertices,
                                  model_record);
}

/* 8013939C: the area 19 and 36 paths check the result bits before drawing.
 * The original branch layout sends other areas straight to the draw loop. */
void ae_StageClearUi_DrawStars(AeRam *r, const AeOvl187Hooks *h)
{
    AeOvl187Hooks child_hooks;
    ovl187_nested_hooks(&child_hooks, h, 64u);
    uint32_t area = ae_rd16(r, O187_GAME_STATE + O187_STATE_AREA);
    if (area == 19u || area == 36u) {
        int32_t result = ovl187_call1(r, h, "ProgressGetStageResult",
                                      O187_ADDR_LEVEL_RESULT, area);
        if (((uint32_t)result & 0x18u) != 0x18u || ae_ram_failed(r)) return;
    }

    for (int32_t star = 0; star < 5; ++star) {
        uint32_t pose = ovl187_frame_address(h, 64u, 16u);
        uint32_t frame = ae_rd8(r, O187_UI_FRAME_COUNTER);
        uint32_t angle = (frame + (uint32_t)star * 8u) & 0xFFu;
        ae_wr16(r, pose + O187_POSE_X, (uint32_t)(int16_t)(-512 + star * 256));
        ae_wr16(r, pose + O187_POSE_Y, 0u);
        ae_wr16(r, pose + O187_POSE_Z, 0x500u);
        ae_wr8(r, pose + O187_POSE_ANGLE, angle);
        (void)ae_StageClearUi_ObjMatrix(r, &child_hooks, 13, pose);
        if (ae_ram_failed(r)) return;
    }
}

/* 801384A0: compute the clear-time labels, timer values, rank text and medal
 * request. The existing HUD time port owns digit formatting and glyph draws. */
void ae_StageClearUi_DrawTimes(AeRam *r, const AeOvl187Hooks *h)
{
    int32_t level = ae_rds16(r, O187_GAME_STATE + O187_STATE_LEVEL);
    uint32_t table = (uint32_t)ovl187_call1(
        r, h, "LevelTableGet", O187_ADDR_LEVEL_TABLE_GET, (uint32_t)level);
    if (ae_ram_failed(r)) return;

    uint32_t current = ae_rd32(r, O187_GAME_STATE +
                                  O187_STATE_CURRENT_TIME);
    uint32_t best = ae_rd32(r, O187_GAME_STATE + O187_STATE_BEST_TIME);
    uint32_t strings = O187_UI_TIME_STRINGS;

    ae_wr16(r, O187_UI + O187_UI_X, 32u);
    ae_wr16(r, O187_UI + O187_UI_Y, 80u);
    ae_wr32(r, O187_UI + O187_UI_TEXT_SPACING, 16u);
    ae_StageClearUi_DrawText(r, h,
        ae_StageClearUi_StringTableIndex(r, strings, 0));
    ae_HudDrawTime(r, h != NULL ? h->hud : NULL, 136, 80,
                   ovl187_s32(current));
    if (ae_ram_failed(r)) return;

    ae_wr16(r, O187_UI + O187_UI_X, 32u);
    ae_wr16(r, O187_UI + O187_UI_Y, 104u);
    ae_StageClearUi_DrawText(r, h,
        ae_StageClearUi_StringTableIndex(r, strings, 1));
    ae_HudDrawTime(r, h != NULL ? h->hud : NULL, 136, 104,
                   ovl187_s32(best));
    if (ae_ram_failed(r)) return;

    if (current < best) {
        ae_wr16(r, O187_UI + O187_UI_X, 64u);
        ae_wr16(r, O187_UI + O187_UI_Y, 208u);
        ae_StageClearUi_DrawText(r, h,
            ae_StageClearUi_StringTableIndex(r, strings, 6));
    }

    int32_t current_time = ovl187_s32(current);
    int32_t gold = ae_rds16(r, table + O187_LEVEL_RECORD_TIME_GOLD);
    int32_t silver = ae_rds16(r, table + O187_LEVEL_RECORD_TIME_SILVER);
    int32_t bronze = ae_rds16(r, table + O187_LEVEL_RECORD_TIME_BRONZE);
    if (gold >= current_time) {
        ae_wr16(r, O187_UI + O187_UI_X, 64u);
        ae_wr16(r, O187_UI + O187_UI_Y, 144u);
        ae_StageClearUi_DrawText(r, h,
            ae_StageClearUi_StringTableIndex(r, strings, 2));
        ae_wr16(r, O187_UI + O187_UI_X, 120u);
        ae_wr16(r, O187_UI + O187_UI_Y, 176u);
        uint32_t label;
        if (bronze >= current_time) label = 3u;
        else if (silver < current_time) label = 5u;
        else label = 4u;
        ae_StageClearUi_DrawText(r, h,
            ae_StageClearUi_StringTableIndex(r, strings, (int32_t)label));
        if (ae_ram_failed(r)) return;
        (void)ovl187_call4(r, h, "HudDrawTimeRankIcon",
                           O187_ADDR_HUD_DRAW_TIME_RANK, 104u, 168u,
                           current, table);
    }
}

static uint32_t ovl187_mode_buffer_cursor(AeRam *r)
{
    uint32_t index = ae_rd32(r, O187_MODE_BUFFER_INDEX);
    return O187_MODE_CURSOR_BASE + index * O187_MODE_OT_ENTRY_STRIDE;
}

static void ovl187_draw_wipe_curtain(AeRam *r, const AeOvl187Hooks *h,
                                     int32_t opening)
{
    uint32_t cursor = ae_rd32(r, O187_UI_ORDER_CURSOR);
    uint32_t count = ae_rd32(r, O187_GAME_OBJECT_COUNT);
    uint32_t ui = O187_UI;
    uint32_t args[] = {
        cursor,
        ae_rd16(r, O187_WIPE_COUNTER),
        ae_rd16(r, ui + O187_UI_PHASE_TIMER),
        ae_rd16(r, ui + O187_UI_DATA),
        count,
        (uint32_t)opening
    };
    int32_t next = ovl187_call(r, h, "RenderStageClearWipeCurtain", 0u,
                               6u, args);
    if (!ae_ram_failed(r)) ae_wr32(r, O187_UI_ORDER_CURSOR, (uint32_t)next);
}

static void ovl187_initialize_area_models(AeRam *r,
                                          const AeOvl187Hooks *h)
{
    uint32_t ui = O187_UI;
    uint32_t area = ae_rd16(r, O187_GAME_STATE + O187_STATE_AREA);
    uint32_t mode = ae_rd16(r, O187_GAME_STATE + O187_STATE_MODE);
    uint32_t area_data = O187_AREA_DATA;
    uint32_t mesh_table = area_data +
                          (uint32_t)ae_rd16(r, area_data +
                                                   O187_AREA_MODELS_COUNT) * 4u;
    uint32_t object_table = area_data +
                            (uint32_t)ae_rd16(r, area_data +
                                                     O187_AREA_MODELS_TABLE) * 4u +
                            4u;

    ae_wr32(r, O187_AREA_MODEL_TABLE_PTR, mesh_table);
    ae_wr32(r, O187_AREA_OBJECT_TABLE_PTR, object_table);
    int32_t model_count = ae_rds32(r, object_table - 4u);
    ae_wr32(r, O187_AREA_MODEL_COUNT, (uint32_t)model_count);
    uint32_t model_instances = ae_HeapAlloc(
        r, h != NULL ? h->area : NULL, (uint32_t)model_count << 5);
    ae_wr32(r, O187_TEMP_MODEL_COUNT, model_instances);

    for (int32_t i = 0; i < model_count; ++i) {
        uint32_t descriptor = object_table +
            (uint32_t)i * O187_AREA_MODEL_DESCRIPTOR_STRIDE;
        uint32_t instance = model_instances +
            (uint32_t)i * O187_MODEL_INSTANCE_STRIDE;
        uint32_t mesh_index = ae_rd16(
            r, descriptor + O187_AREA_MODEL_DESCRIPTOR_MESH);
        uint32_t mesh_entry = mesh_table + mesh_index * 4u;
        uint32_t model_header = mesh_table +
                                (uint32_t)ae_rd16(r, mesh_entry + 4u) * 4u;

        ae_wr16(r, instance + O187_MODEL_INSTANCE_POSITION_X,
                ae_rd16(r, descriptor + O187_AREA_MODEL_DESCRIPTOR_X));
        ae_wr16(r, instance + O187_MODEL_INSTANCE_POSITION_Z,
                ae_rd16(r, descriptor + O187_AREA_MODEL_DESCRIPTOR_Z));
        ae_wr16(r, instance + O187_MODEL_INSTANCE_FLAGS, 0u);
        ae_wr16(r, instance + O187_MODEL_INSTANCE_POSITION_Y,
                ae_rd16(r, descriptor + O187_AREA_MODEL_DESCRIPTOR_Y));
        (void)ovl187_call2(r, h, "MeshInit", O187_ADDR_MESH_INIT,
                           instance, model_header);

        uint32_t model_record = ae_rd32(
            r, instance + O187_MODEL_INSTANCE_RECORD);
        uint32_t model_flags = ae_rd32(r, model_record) & 0xFFFF1FFFu;
        ae_wr32(r, model_record, model_flags);
        ae_wr32(r, instance + O187_MODEL_INSTANCE_ORDER_VALUE,
                ae_rd16(r, descriptor + O187_AREA_MODEL_DESCRIPTOR_VALUE));
        ae_wr16(r, instance + O187_MODEL_INSTANCE_FLAGS, 0xFFFFu);
    }

    for (uint32_t i = 0; i < 2u; ++i) {
        uint32_t descriptor = area_data + i * 4u;
        uint32_t object = O187_UI_OBJECTS + i * O187_OBJ_STRIDE;
        uint32_t model_header = area_data +
                                (uint32_t)ae_rd16(r, descriptor + 4u) * 4u;
        ae_ModelInit(r, h != NULL ? h->area : NULL, object, model_header);

        uint32_t sequence_index = ae_rd16(
            r, descriptor + O187_AREA_MODEL_DESCRIPTOR_SEQUENCE);
        uint32_t sequence_header = area_data + sequence_index * 4u;
        uint32_t sequence_count = ae_rd32(r, sequence_header);
        ae_wr32(r, object + O187_OBJ_MODEL_INSTANCE,
                sequence_header + sequence_count * 4u + 4u);
        ae_wr16(r, object + O187_OBJ_SEQUENCE_FRAME, 0u);
        ae_wr16(r, object + O187_OBJ_SEQUENCE_INDEX, 0u);
        ae_wr16(r, object + O187_OBJ_SEQUENCE_CURRENT, 0u);
        ae_wr16(r, object + O187_OBJ_SEQUENCE_FLAGS, 0u);
        ae_wr32(r, object + O187_OBJ_SEQUENCE_POINTER,
                sequence_header + 4u);
    }
    if (ae_ram_failed(r)) return;

    if (area == 0xFFu) {
        ae_wr32(r, ui + O187_UI_SCRIPT, O187_SCRIPT_AREA_255);
        ae_wr32(r, ui + O187_UI_MODEL_LIST,
                area_data + (uint32_t)ae_rd16(
                                r, area_data + O187_AREA_OBJECTS_TABLE) * 4u);
        return;
    }

    if (mode == 12u) {
        int32_t progress = ovl187_call1(
            r, h, "LevelProgressGet", O187_ADDR_LEVEL_PROGRESS_GET,
            ae_rd16(r, O187_GAME_STATE + O187_STATE_LEVEL));
        if (ae_ram_failed(r)) return;
        if (progress >= 2) {
            ae_wr32(r, ui + O187_UI_SCRIPT, O187_SCRIPT_MODE_12_READY);
            ae_StageClearUi_CheckRequirements(r, h);
        } else {
            ae_wr32(r, ui + O187_UI_SCRIPT,
                    O187_SCRIPT_MODE_12_INCOMPLETE);
            ae_wr8(r, O187_GAME_STATE + O187_STATE_INCOMPLETE_MARKER, 1u);
        }
        return;
    }

    if (mode == 20u) {
        ae_wr32(r, ui + O187_UI_SCRIPT, O187_SCRIPT_MODE_20);
        return;
    }

    if (mode == 25u) {
        uint32_t exit = ae_rd16(r, O187_GAME_STATE + O187_STATE_EXIT);
        ae_wr32(r, ui + O187_UI_SCRIPT,
                exit == 8u ? O187_SCRIPT_MODE_25_EXIT_8 :
                             O187_SCRIPT_MODE_25_OTHER_EXIT);
        return;
    }

    /* The remaining modes use the script pointer selected by their loaded
     * area data, which is already present in the UI object. */
}

/* 801368FC: wait for the stage-clear resources, prepare area objects, then
 * drive the wipe and result-screen state machine one frame at a time. */
int32_t ae_StageClearUi_DrawWipe(AeRam *r, const AeOvl187Hooks *h)
{
    uint32_t ui = O187_UI;
    AeOvl187Hooks child_hooks;
    ovl187_nested_hooks(&child_hooks, h, 56u);
    int32_t state = ae_rds16(r, O187_WIPE_STATE);

    if (state == 0) {
        if (ae_rds16(r, O187_LOAD_ACTIVE) != 0) {
            int32_t load_result = ovl187_call0(
                r, h, "LoadListStep", O187_ADDR_LOAD_LIST_STEP);
            if (load_result < 0) {
                (void)ovl187_call0(r, h, "SoundRestoreVolume",
                                   O187_ADDR_SOUND_RESTORE_VOLUME);
                ae_wr16(r, O187_LOAD_ACTIVE, 0u);
            }
        }

        int32_t tick = (int16_t)(ae_rd16(r, O187_WIPE_TICK) + 2u);
        ae_wr16(r, O187_WIPE_TICK, (uint32_t)tick);
        if (tick < 121 || ae_rds16(r, O187_LOAD_ACTIVE) != 0) return 0;

        ae_wr16(r, O187_WIPE_TICK, 0u);
        ae_AreaSetZoneBoxes(r, h != NULL ? h->area : NULL, 0u);
        ovl187_initialize_area_models(r, &child_hooks);
        if (ae_ram_failed(r)) return 0;

        ae_wr32(r, ui + O187_UI_SCRIPT_TICKS, 0u);
        (void)ae_StageClearUi_RunScript(r, &child_hooks);
        if (ae_ram_failed(r)) return 0;
        (void)ae_StageClearUi_LookAt(r, &child_hooks, ui + O187_UI_START_X,
                                     ui + O187_UI_TARGET_X);
        if (ae_ram_failed(r)) return 0;
        (void)ovl187_call0(r, h, "DrawAreaProps",
                           O187_ADDR_DRAW_AREA_PROPS);
        ae_StageClearUi_UpdateObjects(r, &child_hooks, O187_UI_OBJECTS);
        if (ae_ram_failed(r)) return 0;
        (void)ae_PlaySfx(r, h != NULL ? h->sound : NULL, 0xAC);
        ae_wr16(r, O187_WIPE_STATE, 1u);
        ae_wr16(r, ui + O187_UI_PHASE, 0u);
        ae_wr16(r, ui + O187_UI_PHASE_TIMER, 0u);
        return 0;
    }

    if (state == 1) {
        ovl187_draw_wipe_curtain(r, h, 1);
        if (ae_ram_failed(r)) return 0;
        int32_t timer = (int16_t)(ae_rd16(r, ui + O187_UI_PHASE_TIMER) + 1u);
        ae_wr16(r, ui + O187_UI_PHASE_TIMER, (uint32_t)timer);
        if (timer >= 11) ae_wr16(r, ui + O187_UI_PHASE, 2u);
        return 0;
    }

    if (state == 2) {
        (void)ae_StageClearUi_RunScript(r, &child_hooks);
        if (ae_ram_failed(r)) return 0;
        (void)ae_StageClearUi_LookAt(r, &child_hooks, ui + O187_UI_START_X,
                                     ui + O187_UI_TARGET_X);
        if (ae_ram_failed(r)) return 0;
        (void)ovl187_call0(r, h, "DrawAreaProps",
                           O187_ADDR_DRAW_AREA_PROPS);
        ae_StageClearUi_UpdateObjects(r, &child_hooks, O187_UI_OBJECTS);
        if (ae_ram_failed(r)) return 0;
        if (ae_rd16(r, O187_GAME_STATE + O187_STATE_MODE) == 25u)
            ae_RippleGrid_Draw(r, &child_hooks);
        else
            ae_StageClearUi_DrawScanlines(r, &child_hooks);
        if (ae_ram_failed(r)) return 0;

        uint32_t confirm = ae_rd32(r, O187_PAD_HELD) &
                           ae_rd32(r, O187_PAD_CHANGED) &
                           O187_PAD_CONFIRM_MASK;
        uint32_t script = ae_rd32(r, ui + O187_UI_SCRIPT);
        if (confirm != 0u || ae_rd16(r, script) == O187_SCRIPT_END) {
            (void)ae_PlaySfx(r, h != NULL ? h->sound : NULL, 0xAD);
            ae_wr16(r, O187_WIPE_STATE, 3u);
            ae_wr16(r, ui + O187_UI_PHASE, 0u);
            ae_wr16(r, ui + O187_UI_PHASE_TIMER, 10u);
        }
        return 0;
    }

    if (state == 3) {
        ovl187_draw_wipe_curtain(r, h, 0);
        if (ae_ram_failed(r)) return 0;
        int32_t timer = (int16_t)(ae_rd16(r, ui + O187_UI_PHASE_TIMER) - 1u);
        ae_wr16(r, ui + O187_UI_PHASE_TIMER, (uint32_t)timer);
        if (timer < 0) ae_wr16(r, ui + O187_UI_PHASE, 2u);
    }
    return 0;
}

static void ovl187_render_clear_ot(AeRam *r, const AeOvl187Hooks *h,
                                   uint32_t index)
{
    uint32_t ot = O187_MODE_CLEAR_OT_BASE +
                  index * O187_MODE_OT_ENTRY_STRIDE;
    (void)ovl187_call1(r, h, "RenderClearOT", O187_ADDR_RENDER_CLEAR_OT,
                       ot);
}

static void ovl187_render_set_draw_offset(AeRam *r,
                                          const AeOvl187Hooks *h,
                                          uint32_t index)
{
    uint32_t offset = O187_MODE_DRAW_OFFSET_BASE +
                      index * O187_MODE_OT_ENTRY_STRIDE;
    (void)ovl187_call1(r, h, "RenderSetDrawOffset",
                       O187_ADDR_RENDER_SET_DRAW_OFFSET, offset);
}

static void ovl187_update_display_transition(AeRam *r,
                                             const AeOvl187Hooks *h,
                                             int32_t wipe_state)
{
    uint32_t ui = O187_UI;
    int32_t phase = ae_rds16(r, ui + O187_UI_PHASE);
    uint32_t main_index = ae_rd32(r, O187_MODE_BUFFER_INDEX);

    if (wipe_state == 1) {
        if (phase == 0) {
            ae_StageClearUi_SetDrawStp(r, h);
            ovl187_render_clear_ot(r, h, main_index);
            ovl187_render_set_draw_offset(r, h, main_index);
            ae_wr16(r, ui + O187_UI_PHASE, 1u);
            ae_wr16(r, ui + O187_UI_PHASE_SUBSTATE, main_index);
        } else if (phase == 1) {
            uint32_t substate = ae_rd16(r, ui + O187_UI_PHASE_SUBSTATE);
            ae_wr8(r, O187_DISPLAY_MODEL_FLAGS, 0u);
            ae_wr8(r, O187_DISPLAY_STATE_FLAGS, 0u);
            ae_wr16(r, ui + O187_UI_PHASE, 0u);
            ovl187_render_clear_ot(r, h, substate);
            substate = (uint16_t)(substate ^ 1u);
            ovl187_render_set_draw_offset(r, h, substate);
        } else if (phase == 2) {
            uint32_t substate = ae_rd16(r, ui + O187_UI_PHASE_SUBSTATE);
            ovl187_render_clear_ot(r, h, substate);
            substate = (uint16_t)(substate ^ 1u);
            ovl187_render_set_draw_offset(r, h, substate);
            ae_wr8(r, O187_DISPLAY_MODEL_FLAGS, 1u);
            ae_wr8(r, O187_DISPLAY_STATE_FLAGS, 1u);
            ae_wr16(r, O187_WIPE_STATE, 2u);
        }
    } else if (wipe_state == 3) {
        if (phase == 0) {
            ovl187_render_clear_ot(r, h, main_index);
            ovl187_render_set_draw_offset(r, h, main_index);
            ae_wr8(r, O187_DISPLAY_MODEL_FLAGS, 0u);
            ae_wr8(r, O187_DISPLAY_STATE_FLAGS, 0u);
            ae_wr16(r, ui + O187_UI_PHASE, 1u);
            ae_wr16(r, ui + O187_UI_PHASE_SUBSTATE, main_index);
        } else if (phase == 1) {
            uint32_t substate = ae_rd16(r, ui + O187_UI_PHASE_SUBSTATE);
            ae_wr8(r, O187_DISPLAY_STATE_FLAGS, 0u);
            ovl187_render_clear_ot(r, h, substate);
            ovl187_render_set_draw_offset(r, h,
                                          (uint16_t)(substate ^ 1u));
        } else if (phase == 2) {
            uint32_t substate = ae_rd16(r, ui + O187_UI_PHASE_SUBSTATE);
            ovl187_render_clear_ot(r, h, substate);
            ovl187_render_set_draw_offset(r, h,
                                          (uint16_t)(substate ^ 1u));
            ae_wr16(r, O187_WIPE_STATE, 4u);
        }
    }

    uint32_t draw_area = O187_MODE_DRAW_AREA_BASE +
                         main_index * O187_MODE_OT_ENTRY_STRIDE;
    (void)ovl187_call1(r, h, "RenderSetDrawArea",
                       O187_ADDR_RENDER_SET_DRAW_AREA, draw_area);
    (void)ovl187_call0(r, h, "CdMgrUpdate", O187_ADDR_CD_MGR_UPDATE);
}

static void ovl187_finish_mode(AeRam *r, const AeOvl187Hooks *h)
{
    (void)ovl187_call1(r, h, "ContextClear", O187_ADDR_CONTEXT_CLEAR, 0u);
    (void)ovl187_call1(r, h, "VSync", O187_ADDR_VSYNC, 0u);
    (void)ovl187_call0(r, h, "CdMgrStopStream",
                       O187_ADDR_CD_MGR_STOP_STREAM);
    (void)ae_SoundStop(r, h != NULL ? h->stage_fade : NULL, 1);
    if (ae_ram_failed(r)) return;

    if (ae_rds16(r, O187_MODE_AREA_OVERRIDE) != 0) {
        ae_wr16(r, O187_GAME_STATE + O187_STATE_AREA, 0xFFu);
        return;
    }

    uint32_t mode = ae_rd16(r, O187_GAME_STATE + O187_STATE_MODE);
    if (mode == 20u) {
        ae_wr16(r, O187_GAME_STATE + O187_STATE_MODE, 9u);
        return;
    }
    ae_wr16(r, O187_GAME_STATE + O187_STATE_MODE, 13u);
    uint32_t area = ae_rd16(r, O187_GAME_STATE + O187_STATE_AREA);
    uint32_t level = ae_rd16(r, O187_GAME_STATE + O187_STATE_LEVEL);
    if (area == 0xFFu || level < 6u || level >= 30u) return;

    uint32_t target = ae_rd32(r, O187_MODE_DISPATCH_TABLE +
                                  (level - 6u) * 4u);
    switch (target) {
    case 0x801367E8u:
    case 0x80136824u: {
        uint32_t special_area = target == 0x801367E8u ? 19u : 36u;
        int32_t result = ovl187_call1(r, h, "ProgressGetStageResult",
                                      O187_ADDR_LEVEL_RESULT, special_area);
        if (((uint32_t)result & 1u) == 0u) {
            ae_wr16(r, O187_GAME_STATE + O187_STATE_AREA, special_area);
            ae_wr16(r, O187_GAME_STATE + O187_STATE_MODE, 23u);
            ae_wr8(r, O187_GAME_STATE + O187_STATE_CLEAR_NEXT_KIND,
                   target == 0x801367E8u ? 2u : 5u);
            ae_wr8(r, O187_GAME_STATE + O187_STATE_CLEAR_NEXT_FLAGS, 0u);
        }
        break;
    }
    case 0x80136868u: {
        int32_t bits = ovl187_call1(r, h, "ProgressBitsGet",
                                    O187_ADDR_PROGRESS_FIELD, 4u);
        if (bits == 0) {
            ae_wr16(r, O187_GAME_STATE + O187_STATE_MODE, 26u);
            ae_wr16(r, O187_GAME_STATE + O187_STATE_RACE_TARGET, 4u);
        }
        break;
    }
    case 0x80136898u: {
        int32_t bits = ovl187_call1(r, h, "ProgressBitsGet",
                                    O187_ADDR_PROGRESS_FIELD, 1u);
        if (bits == 0) {
            ae_wr16(r, O187_GAME_STATE + O187_STATE_MODE, 26u);
            ae_wr16(r, O187_GAME_STATE + O187_STATE_RACE_TARGET, 1u);
        }
        break;
    }
    default:
        break;
    }
}

/* 80136208: initialize the stage-clear display, then update it until the wipe
 * closes and the stage-clear result selects the next game mode. */
int32_t ae_Mode_StageClear(AeRam *r, const AeOvl187Hooks *h)
{
    uint32_t mode_stack = ovl187_frame_address(h, 64u, 16u);
    AeOvl187Hooks child_hooks;
    ovl187_nested_hooks(&child_hooks, h, 64u);
    uint32_t ui = O187_UI;
    uint32_t area = ae_rd16(r, O187_GAME_STATE + O187_STATE_AREA);

    (void)ovl187_call0(r, h, "CutsceneModeOff",
                       O187_ADDR_CUTSCENE_MODE_OFF);
    (void)ovl187_call1(r, h, "ResetGraph", O187_ADDR_RESET_GRAPH, 1u);
    ae_wr32(r, mode_stack, ae_rd32(r, 0x80139470u));
    ae_wr32(r, mode_stack + 4u, ae_rd32(r, 0x80139474u));
    (void)ovl187_call4(r, h, "ContextSetup", O187_ADDR_CONTEXT_SETUP,
                       mode_stack, 0u, 0u, 0u);
    (void)ovl187_call1(r, h, "SetGeomScreen", O187_ADDR_SET_GEOM_SCREEN,
                       328u);
    ae_SetColorMatrixMode(r, h != NULL ? h->items : NULL, 0, 0, 0, 0);
    ae_wr32(r, O187_CAMERA_MATRIX + O187_CAMERA_TRANSLATION_X, 0u);
    ae_wr32(r, O187_CAMERA_MATRIX + O187_CAMERA_TRANSLATION_Y, 4096u);
    ae_wr32(r, O187_CAMERA_MATRIX + O187_CAMERA_TRANSLATION_Z, 4096u);
    ae_HeapInit(r, h != NULL ? h->area : NULL);
    (void)ovl187_call1(r, h, "ContextClear", O187_ADDR_CONTEXT_CLEAR, 0u);
    ae_wr16(r, ui + O187_UI_COLOR, 0x3FDFu);

    if (area == 19u || area == 36u) {
        (void)ovl187_call1(r, h, "ArcLoadEntry", O187_ADDR_ARC_LOAD_ENTRY,
                           0x80139584u);
        ae_wr16(r, ui + O187_UI_COLOR, 0x1Bu);
    } else if (area == 0xFFu) {
        (void)ovl187_call1(r, h, "ArcLoadEntry", O187_ADDR_ARC_LOAD_ENTRY,
                           0x80139564u);
    } else {
        (void)ovl187_call1(r, h, "ArcLoadEntry", O187_ADDR_ARC_LOAD_ENTRY,
                           0x80139544u);
    }
    if (ae_ram_failed(r)) return 0;

    ae_StageClearUi_Setup(r, &child_hooks);
    ae_wr32(r, O187_GAME_OBJECT_COUNT, 0u);
    ae_wr16(r, ui + 6u, 38u);
    ae_wr32(r, ui + O187_UI_TEXT_SPACING, 32u);
    ae_wr16(r, ui + O187_UI_START_Z, (uint32_t)-4096);
    ae_wr16(r, O187_WIPE_STATE, 0u);
    ae_wr16(r, ui + O187_UI_START_X, 0u);
    ae_wr16(r, ui + O187_UI_START_Y, 0u);
    ae_wr16(r, ui + O187_UI_TARGET_X, 0u);
    ae_wr16(r, ui + O187_UI_TARGET_Y, 0u);
    ae_wr16(r, ui + O187_UI_TARGET_Z, 0u);
    ae_wr32(r, ui + O187_UI_SCRIPT_FLAGS, 0u);
    ae_wr8(r, O187_DISPLAY_MODEL_FLAGS, 1u);
    ae_wr8(r, O187_DISPLAY_STATE_FLAGS, 1u);
    ae_wr8(r, O187_DISPLAY_MODEL_FLAGS + 1u, 0u);
    ae_wr8(r, O187_DISPLAY_STATE_FLAGS + 1u, 0u);
    (void)ae_RippleGrid_Init(r, &child_hooks);
    if (ae_ram_failed(r)) return 0;

    for (;;) {
        ae_pad_PadUpdate(r, child_hooks.pad, 0u);
        if (ae_ram_failed(r)) return 0;
        ae_wr32(r, O187_UI_FRAME_COUNTER,
                ae_rd32(r, O187_UI_FRAME_COUNTER) + 1u);
        (void)ovl187_call0(r, h, "BeginFrame", O187_ADDR_BEGIN_FRAME);
        ae_wr32(r, ui + O187_UI_DATA, ovl187_mode_buffer_cursor(r));
        if (ae_ram_failed(r)) return 0;

        int32_t draw_result = ae_StageClearUi_DrawWipe(r, &child_hooks);
        if (ae_ram_failed(r)) return 0;
        if (draw_result < 0) {
            ovl187_finish_mode(r, h);
            return 0;
        }
        (void)ovl187_call1(r, h, "ContextClear", O187_ADDR_CONTEXT_CLEAR, 0u);
        (void)ovl187_call1(r, h, "VSync", O187_ADDR_VSYNC, 2u);
        if (ae_ram_failed(r)) return 0;

        int32_t wipe_state = ae_rds16(r, O187_WIPE_STATE);
        ovl187_update_display_transition(r, h, wipe_state);
        if (ae_ram_failed(r)) return 0;
        if (ae_rds16(r, O187_WIPE_STATE) == 4) {
            ovl187_finish_mode(r, h);
            return 0;
        }
    }
}


