/* Stage-start mode overlay (member 188, loaded at 0x80136000). */
#include "ae_ovl188.h"

#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "../core/ae_pose.h"
#include "../math/native_sdk_apply.h"
#include "../core/ae_pad.h"
#include "../sound/ae_bgm.h"
#include "../ui/ae_hud.h"
#include "../world/ae_areaset.h"
#include "../world/ae_misc_batch.h"
#include "../world/ae_stage.h"
#include "../world/ae_stagefade.h"
#include "../player/ae_player_spawn.h"
#include "../player/ae_player_act.h"
#include "../player/ae_player1.h"

#include <stdint.h>

/* Overlay globals and records.  Keep the original fields named here so the function bodies
   describe the state they manipulate instead of scattering unexplained offsets. */
#define O188_AREA_DATA                    0x80154000u
#define O188_AREA_MESH_TABLE              0x800EC574u
#define O188_AREA_PROP_PLACEMENT_TABLE    0x800EC578u
#define O188_AREA_MESHES                  0x800EC574u
#define O188_AREA_PROP_PLACEMENTS         0x800EC578u
#define O188_AREA_PROPS                   0x800B4D64u
#define O188_STAGE_UI_STATE               0x8013B998u
#define O188_STAGE_UI_RIPPLE_GRID          0x8013B9F8u
#define O188_STAGE_UI_STATIC_MODELS        0x8013B6F0u
#define O188_FRAME_COUNTER                 0x800EC350u
#define O188_FRAME_TIMER_OFFSET             0x0Cu
#define O188_FRAME_DISPLAY_ENV              0x800EC3BCu
#define O188_FRAME_DRAW_ENV                 0x800EC360u
#define O188_FRAME_ALTERNATE_ENV            0x800F03D0u
#define O188_FRAME_DISPLAY_ENV_OFFSET       0x3FFCu
#define O188_FRAME_DRAW_ENV_OFFSET          0x407Cu
#define O188_FRAME_DRAW_ENV_SECOND_OFFSET   0x4080u
#define O188_FRAME_BUFFER_STRIDE            0x4078u
#define O188_FRAME_DRAWENV_ISBG             (O188_FRAME_COUNTER + 0x28u)
#define O188_FRAME_DRAWENV_BG_RED           (O188_FRAME_COUNTER + 0x29u)
#define O188_FRAME_DRAWENV_BG_GREEN         (O188_FRAME_COUNTER + 0x2Au)
#define O188_FRAME_DRAWENV_BG_BLUE          (O188_FRAME_COUNTER + 0x2Bu)
#define O188_DISPLAY_READY                 0x800F03F0u
#define O188_RUN_A_STACK_BYTES              0x40u
#define O188_RUN_B_STACK_BYTES              0x40u
#define O188_RUN_SCRIPT_STACK_BYTES         0x30u
#define O188_DRAW_WIPE_A_STACK_BYTES         0x20u
#define O188_DRAW_WIPE_B_STACK_BYTES         0x28u
#define O188_DEFAULT_STACK_POINTER           0x801FFF00u
#define O188_CLEAR_RECT_TEMPLATE             0x8013ADB0u
#define O188_PLAYER_EYE_RECT_STACK_TEMP     0x801FFEE8u
#define O188_TEXT_WIDTHS                   0x8013B5E0u
#define O188_LOOK_AT_MATRIX                0x800E78C0u
#define O188_LOOK_AT_TRANSLATION           0x800E78D4u
#define O188_MATRIX_M00                     0x00u
#define O188_MATRIX_M01                     0x02u
#define O188_MATRIX_M02                     0x04u
#define O188_MATRIX_M10                     0x06u
#define O188_MATRIX_M11                     0x08u
#define O188_MATRIX_M12                     0x0Au
#define O188_MATRIX_M20                     0x0Cu
#define O188_MATRIX_M21                     0x0Eu
#define O188_MATRIX_M22                     0x10u
#define O188_MATRIX_TRANSLATION              0x14u
#define O188_VECTOR_X                       0x00u
#define O188_VECTOR_Y                       0x04u
#define O188_VECTOR_Z                       0x08u
#define O188_SVECTOR_X                      0x00u
#define O188_SVECTOR_Y                      0x02u
#define O188_SVECTOR_Z                      0x04u
#define O188_SCALE_X                        0x00u
#define O188_SCALE_Y                        0x04u
#define O188_SCALE_Z                        0x08u
#define O188_LOOK_AT_FRAME_BYTES           120u
#define O188_LOOK_AT_VECTOR_IN             16u
#define O188_LOOK_AT_VECTOR_OUT            32u
#define O188_LOOK_AT_IDENTITY_TEMP          48u
#define O188_LOOK_AT_SCALE_TEMP             80u
#define O188_STAGE_UI_TEXT_X               0x00u
#define O188_STAGE_UI_TEXT_Y               0x02u
#define O188_STAGE_UI_TEXT_CLUT            0x04u
#define O188_STAGE_UI_TEXT_TPAGE           0x06u

#define O188_AREA_MESH_TABLE_OFFSET        0x2Cu
#define O188_AREA_PROP_TABLE_OFFSET        0x30u
#define O188_AREA_ZONE_TABLE_OFFSET        0x34u
#define O188_AREA_STATIC_MODEL_COUNT       0x36u
#define O188_AREA_MESH_COUNT               0x38u
#define O188_STAGE_AREA_STATE               0x800F4350u
#define O188_STAGE_AREA_MESH_POINTER        0x224u
#define O188_STAGE_AREA_PROP_POINTER        0x228u
#define O188_STAGE_AREA_PROP_COUNT          0x280u

#define O188_AREA_PROP_RECORD_BYTES        0x0Cu
#define O188_AREA_PROP_TABLE_COUNT_BYTES   4u
#define O188_AREA_PROP_INSTANCE_BYTES      0x20u
#define O188_AREA_PROP_FIRST_INSTANCE       0x80u
#define O188_AREA_MESH_HEADER_ANIMATION     0x04u
#define O188_AREA_ANIMATION_TABLE_ENTRIES   0x04u
#define O188_ANIMATION_RECORD_BYTES         0x04u
#define O188_AREA_PROP_INSTANCE_MESH       0x10u
#define O188_AREA_PROP_INSTANCE_ANIM       0x14u
#define O188_AREA_PROP_INSTANCE_FRAME      0x12u
#define O188_AREA_PROP_INSTANCE_MODEL      0x1Cu
#define O188_AREA_PROP_INSTANCE_MODEL_DATA 0x00u
#define O188_AREA_PROP_INSTANCE_ANIM_DATA  0x18u
#define O188_AREA_PROP_INSTANCE_FLAGS      0x00u
#define O188_AREA_PROP_INSTANCE_HIDDEN     0x16u

#define O188_STATIC_MODEL_RECORD_BYTES     4u
#define O188_STATIC_MODEL_INSTANCE_BYTES   0x88u
#define O188_STATIC_MODEL_HEADER           0x04u
#define O188_STATIC_MODEL_ANIM_INDEX       0x18u
#define O188_STATIC_MODEL_ANIM_HEADER       0x18u
#define O188_STATIC_MODEL_ANIM_TABLE        0x74u
#define O188_STATIC_MODEL_FRAME             0x5Cu
#define O188_STATIC_MODEL_FRAME_ALT         0x5Eu
#define O188_STATIC_MODEL_LOOP_FRAME        0x70u
#define O188_STATIC_MODEL_LOOP_FRAME_ALT    0x72u
#define O188_STATIC_MODEL_ANGLE             0x64u

#define O188_SCREEN_VERTS                  0x800E7D00u
#define O188_STAGE_PROP_MATRIX              0x800E9C80u
#define O188_CLAMPED_SCREEN_VERTS          0x800EA060u
#define O188_SCREEN_VERT_BYTES             8u
#define O188_SCREEN_VERT_X                 0x00u
#define O188_SCREEN_VERT_Y                 0x02u
#define O188_SCREEN_VERT_Z                 0x04u

#define O188_RIPPLE_ROW_BYTES              0x80u
#define O188_RIPPLE_VERTEX_BYTES           8u
#define O188_RIPPLE_X                      0x00u
#define O188_RIPPLE_Y                      0x02u
#define O188_RIPPLE_COLOUR                 0x04u
#define O188_RIPPLE_COLUMNS                16
#define O188_RIPPLE_ROWS                   12
#define O188_RIPPLE_LAST_ROW_OFFSET        0x580u
#define O188_RIPPLE_LAST_COLUMN_OFFSET     ((O188_RIPPLE_COLUMNS - 1) * O188_RIPPLE_VERTEX_BYTES)
#define O188_MODEL_TRANSFORM_SCALE_X       0x24u
#define O188_MODEL_TRANSFORM_SCALE_Y       0x28u
#define O188_MODEL_TRANSFORM_SCALE_Z       0x2Cu
#define O188_FRAME_PACKET_TABLE_OFFSET     0x80u
#define O188_MODEL_HEADER_VERTEX_COUNT     0x06u
#define O188_ANIMATION_RECORD_END_FRAME    0x02u

#define O188_MODE_STATE                    0x800F4350u
#define O188_MODE_STAGE_START_STACK_BYTES  0x28u
#define O188_MODE_VALUE                    0x120u
#define O188_MODE_AREA                     0x126u
#define O188_MODE_AREA_VARIANT             0x124u
#define O188_MODE_RETRY_FLAG               0x2CCu
#define O188_MODE_RETRY_FLAG_SECOND_BYTE   0x01u
#define O188_MODE_OBJECTIVES_STATE         0x144u
#define O188_MODE_TIMER                    0x0Cu
#define O188_MODE_FLAGS                    0x58u
#define O188_MODE_SCRIPT_POINTER           0x20u
#define O188_MODE_SCRIPT_RETURN            0x24u
#define O188_MODE_SCRIPT_WAIT              0x44u
#define O188_MODE_SCRIPT_OBJECTIVE_COUNT   0x3Au
#define O188_MODE_CAMERA_FROM_X            0x10u
#define O188_MODE_CAMERA_TO_X              0x34u
#define O188_MODE_CAMERA_SECOND_FROM_X     0x18u
#define O188_MODE_CAMERA_SECOND_TO_X       0x3Cu
#define O188_MODE_CAMERA_TIME              0x3Au
#define O188_MODE_CAMERA_SECOND_TIME       0x42u
#define O188_MODE_SCRIPT_TABLE             0x8013AEA4u
#define O188_MODE_STAGE_INDEX              0x800EC350u
#define O188_MODE_WIPE_KIND                0x8013AE60u
#define O188_UI_TEXTURE_SEED                0x800AE8F0u
#define O188_UI_TEXTURE_SEED_2              (O188_UI_TEXTURE_SEED + 2u)
#define O188_MODE_WIPE_A_TIMER              0x8013B47Au
#define O188_MODE_WIPE_B_TIMER              0x8013B482u
#define O188_MODE_WIPE_A_SOUND_PENDING      0x8013B478u
#define O188_MODE_WIPE_B_SOUND_PENDING      0x8013B480u
#define O188_MODE_COUNTER                   0x8013B6E0u
#define O188_MODE_SCANLINE_TIMER            0x8013B6E4u
#define O188_MODE_PROGRESS_CURRENT          0x148u
#define O188_MODE_PROGRESS_NEXT             0x14Cu
#define O188_MODE_STATE_HANDOFF_PARAMETER   0x15Au
#define O188_MODE_UI_POSITION_X             0x00u
#define O188_MODE_UI_POSITION_Y             0x02u
#define O188_MODE_UI_TEXT_CLUT              0x04u
#define O188_MODE_UI_TEXT_TPAGE             0x06u
#define O188_MODE_UI_SCRIPT_POINTER         0x20u
#define O188_MODE_UI_SCRIPT_RETURN          0x24u
#define O188_MODE_UI_ANIMATION_FRAME        0x28u
#define O188_MODE_UI_ANIMATION_FRAME_LIMIT  0x2Cu
#define O188_MODE_UI_ANIMATION_TIMER        0x2Au
#define O188_MODE_UI_TIMER                  0x0Cu
#define O188_MODE_UI_VECTOR_A_X             0x10u
#define O188_MODE_UI_VECTOR_A_Y             0x12u
#define O188_MODE_UI_VECTOR_A_Z             0x14u
#define O188_MODE_UI_VECTOR_B_X             0x18u
#define O188_MODE_UI_VECTOR_B_Y             0x1Au
#define O188_MODE_UI_VECTOR_B_Z             0x1Cu
#define O188_MODE_UI_VECTOR_A_TARGET_X      0x34u
#define O188_MODE_UI_VECTOR_A_TARGET_Y      0x36u
#define O188_MODE_UI_VECTOR_A_TARGET_Z      0x38u
#define O188_MODE_UI_VECTOR_B_TARGET_X      0x3Cu
#define O188_MODE_UI_VECTOR_B_TARGET_Y      0x3Eu
#define O188_MODE_UI_VECTOR_B_TARGET_Z      0x40u
#define O188_MODE_UI_VECTOR_A_TIME          0x3Au
#define O188_MODE_UI_VECTOR_B_TIME          0x42u
#define O188_MODE_UI_HIDE_OBJECTIVES        0x48u
#define O188_MODE_UI_SCRIPT_FLAGS           0x58u
#define O188_MODE_UI_SCRIPT_TICK            0x5Cu
#define O188_MODE_UI_SCRIPT_COUNT           0x5Cu
#define O188_MODE_UI_SCRIPT_OBJECT_COUNT    0x3Au
#define O188_MODE_UI_SCRIPT_LINE_COUNT      0x3Au
#define O188_MODE_UI_ENV_POINTER             0x08u
#define O188_SCRIPT_OPERAND_OFFSET           0x02u
#define O188_SCRIPT_OBJECTIVE_LINE_STRIDE    0x08u
#define O188_SCRIPT_OBJECTIVE_LINE_X         0x00u
#define O188_SCRIPT_OBJECTIVE_LINE_Y         0x02u
#define O188_SCRIPT_OBJECTIVE_LINE_TYPE      0x06u
#define O188_SCRIPT_OBJECTIVE_LINE_FLAGS     0x07u
#define O188_LEVEL_INFO_REQUIRED             0x04u
#define O188_LEVEL_INFO_RESULT               0x06u
#define O188_LEVEL_INFO_CURRENT              0x08u
#define O188_LEVEL_INFO_COUNTER              0x0Au
#define O188_LEVEL_INFO_KIND                 0x10u
#define O188_LEVEL_INFO_FOLLOWUP             0x14u
#define O188_LEVEL_INFO_TOTAL                0x24u
#define O188_NUMBER_SCRATCH_FIRST            0x04u
#define O188_NUMBER_SCRATCH_SECOND           0x08u
#define O188_NUMBER_SCRATCH_SUFFIX           0x0Eu
#define O188_NUMBER_DIGITS_BUFFER            0x10u
#define O188_NUMBER_SEPARATOR_OFFSET        0x02u
#define O188_NUMBER_SUFFIX_OFFSET           0x03u
#define O188_NUMBER_TERMINATOR_OFFSET       0x05u
#define O188_NUMBER_NEXT_DIGIT_OFFSET       0x01u
#define O188_LEVEL_TABLE_SECOND_OFFSET      0x02u
#define O188_LEVEL_TABLE_THIRD_OFFSET       0x04u
#define O188_LEVEL_TABLE_FOURTH_OFFSET      0x06u
#define O188_STAGE_TITLE_RECORD_X           0x02u
#define O188_STAGE_TITLE_RECORD_Y           0x04u
#define O188_STAGE_TITLE_RECORD_LIMIT       0x06u
#define O188_STAGE_TITLE_RECORD_STRIDE      0x06u
#define O188_ANIMATION_RECORD_FIRST_FRAME   0x00u

#define O188_OBJECT_BYTES                   0x88u
#define O188_OBJECT_FLAGS                   0x72u
#define O188_OBJECT_POSITION_X             0x60u
#define O188_OBJECT_POSITION_Y             0x62u
#define O188_OBJECT_POSITION_Z             0x64u
#define O188_OBJECT_TARGET_X               0x78u
#define O188_OBJECT_TARGET_Y               0x7Au
#define O188_OBJECT_TARGET_Z               0x7Cu
#define O188_OBJECT_POSITION_TIME          0x7Eu
#define O188_OBJECT_TRANSLATION_X          0x24u
#define O188_OBJECT_TRANSLATION_Y          0x28u
#define O188_OBJECT_TRANSLATION_Z          0x2Cu
#define O188_OBJECT_TRANSLATION_TARGET_X   0x80u
#define O188_OBJECT_TRANSLATION_TARGET_Y   0x82u
#define O188_OBJECT_TRANSLATION_TARGET_Z   0x84u
#define O188_OBJECT_TRANSLATION_TIME       0x86u
#define O188_OBJECT_DIRECTION_MATRIX       0x14u
#define O188_OBJECT_ANIMATION_HEADER       0x74u
#define O188_OBJECT_ANIMATION_FRAME        0x5Cu
#define O188_OBJECT_ANIMATION_INDEX        0x5Eu
#define O188_OBJECT_ANIMATION_LOOP          0x70u
#define O188_OBJECT_ANIMATION_RENDER_FLAGS 0x72u
#define O188_OBJECT_MODEL_MESH              0x00u
#define O188_OBJECT_MODEL_ACTIVE             0x48u

#define O188_AREA_TABLE_PTR                 0x8013B9C8u
#define O188_RENDER_PACKET_CURSOR            0x800EC354u
#define O188_RENDER_ORDER_TABLE              0x800EC358u
#define O188_DRAW_STP_PACKET_BYTES             0x1Cu
#define O188_WIPE_QUAD_A_PACKET_BYTES          0x50u
#define O188_WIPE_QUAD_B_PACKET_BYTES          0xF0u
#define O188_RIPPLE_DRAW_PACKET_BYTES          0x18C0u
#define O188_LOAD_LIST_START_WIDTH             240u
#define O188_GLYPH_PACKET_BYTES              24u
#define O188_TEXT_WIDTH_TABLE                0x8013B5E0u
#define O188_STAGE_NAME_BY_LEVEL             0x8013B450u
#define O188_STAGE_NAME_TABLE                0x8013AED0u
#define O188_STAGE_NAME_BUFFER               0x800F4458u
#define O188_LEVEL_SCRATCH                   0x1F800000u
#define O188_OBJECTIVE_COUNTER               0x8013B7EAu
#define O188_STAGE_UI_OBJECTS                0x8013B6F0u
#define O188_SCRIPT_STAGE_OBJECTIVE_LINES    0x8013AEA4u
#define O188_STAGE_UI_DETAILS                0x8013B998u
#define O188_STAGE_UI_WIPE_DATA              0x8013B478u
#define O188_STAGE_UI_WIPE_DATA_B            0x8013B480u
#define O188_STAGE_STATUS_PARAMETER          0x8013B9E0u
#define O188_STAGE_SCRIPT_DEFAULT             0x8013B228u
#define O188_STAGE_SCRIPT_KIND_2              0x8013B290u
#define O188_STAGE_SCRIPT_KIND_4              0x8013B2D0u
#define O188_STAGE_SCRIPT_KIND_5              0x8013B310u
#define O188_STAGE_SCRIPT_KIND_7              0x8013B350u
#define O188_STAGE_SCRIPT_KIND_10             0x8013B390u
#define O188_STAGE_SCRIPT_KIND_11             0x8013B408u
#define O188_STAGE_SCRIPT_MODE_18             0x8013B008u
#define O188_STAGE_SCRIPT_AREA_19_36          0x8013B148u
#define O188_STAGE_SCRIPT_LOCKED_PROGRESS     0x8013B0A8u
#define O188_STAGE_AREA_KIND_COUNTER_A        0x8013AE8Cu
#define O188_STAGE_AREA_KIND_COUNTER_B        0x8013AE94u
#define O188_STAGE_ARC_LIST                   0x8013AE84u
#define O188_STAGE_AREA_KIND_VARIANT          0x8013AE64u
#define O188_STAGE_AREA_KIND_ANIMATION        0x8013AE74u
#define O188_STAGE_AREA_KIND_HEADER_A         0x8013AEA8u
#define O188_STAGE_AREA_KIND_HEADER_B         0x8013AEB0u
#define O188_STAGE_AREA_KIND_HEADER_C         0x8013AEB8u
#define O188_STAGE_AREA_KIND_HEADER_FLAGS     0x8013AEC2u
#define O188_STAGE_NAME_STRINGS               0x8013B490u
#define O188_STAGE_TEXT_HUB_TITLE             0x8013B5CCu
#define O188_STAGE_TEXT_MODE18_MAIN           0x8013B53Cu
#define O188_STAGE_TEXT_MODE18_RANK           0x8013B5BAu
#define O188_STAGE_TEXT_OBJECTIVE_CAPTURE     0x8013B572u
#define O188_STAGE_TEXT_OBJECTIVE_EMPTY       0x8013B560u
#define O188_STAGE_TEXT_OBJECTIVE_SPECIAL     0x8013B584u
#define O188_STAGE_TEXT_OBJECTIVE_LEVELS      0x8013B596u
#define O188_STAGE_TEXT_OBJECTIVE_COUNT       0x8013B54Eu
#define O188_STAGE_UI_DRAW_MODE               0x8013B9F4u
#define O188_STAGE_UI_LOOK_AT_EYE             0x8013B9A8u
#define O188_STAGE_UI_LOOK_AT_TARGET          0x8013B9B0u
#define O188_STAGE_UI_MODEL_TRANSFORM          0x800EBFA0u
#define O188_GTE_MATRIX_TEMP                  0x800AF0A0u
#define O188_DISPLAY_READY_SECONDARY          (O188_DISPLAY_READY + 1u)
#define O188_DISPLAY_READY_TERTIARY           (O188_DISPLAY_READY + 2u)
#define O188_DISPLAY_READY_QUATERNARY         (O188_DISPLAY_READY + 3u)

#define O188_PROP_RECORD_MESH_INDEX          0x00u
#define O188_PROP_RECORD_MODEL_INDEX         0x02u
#define O188_PROP_RECORD_MESH_ANIMATION      0x04u
#define O188_PROP_RECORD_FRAME               0x06u
#define O188_PROP_RECORD_ANIMATION           0x08u
#define O188_MODEL_INSTANCE_POSITION_X       0x60u
#define O188_MODEL_INSTANCE_POSITION_Y       0x62u
#define O188_MODEL_INSTANCE_POSITION_Z       0x64u
#define O188_MODEL_INSTANCE_YAW              0x14u
#define O188_MODEL_INSTANCE_DRAW_FLAGS       0x72u
#define O188_MODEL_INSTANCE_FRAME            0x5Cu
#define O188_MODEL_INSTANCE_FRAME_INDEX      0x5Eu
#define O188_MODEL_INSTANCE_LOOP_INDEX       0x70u
#define O188_MODEL_INSTANCE_ANIM_HEADER      0x74u

#define O188_INPUT_BUTTONS                   0x800E5480u
#define O188_INPUT_BUTTONS_PRESSED           0x800E5478u
#define O188_INPUT_ACCEPT_MASK                0x00000860u
#define O188_SOUND_SELECT                    0xACu
#define O188_SOUND_CONFIRM                   0xADu
#define O188_SOUND_STAGE_START               0xAFu

static int32_t o188_call(AeRam *ram, const AeOvl188Hooks *hooks,
                         const char *name, uint32_t address, uint32_t argc,
                         const uint32_t *args);
static void o188_render(AeRam *ram, const AeOvl188Hooks *hooks,
                        const char *name, uint32_t argc,
                        const uint32_t *args);

static int32_t o188_signed_div(AeRam *ram, int32_t dividend, int32_t divisor) {
    if (divisor == 0 || (dividend == INT32_MIN && divisor == -1)) {
        ae_ram_fail(ram, "overlay 188 signed division trap (%d / %d)",
                    dividend, divisor);
        return 0;
    }
    return dividend / divisor;
}

static int32_t o188_level_info(AeRam *ram, const AeOvl188Hooks *hooks,
                               int32_t level, uint32_t output);

static int32_t o188_get_area_kind(AeRam *ram, uint32_t area) {
    return ae_AreaGetKind(ram, NULL, (int32_t)area);
}

static int32_t o188_get_area_level(AeRam *ram, uint32_t area) {
    return ae_AreaGetLevel(ram, NULL, (int32_t)area);
}

static void o188_hud_sprite(void *user, AeRam *ram,
                            const AeHudDraw *draw) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    const uint32_t args[13] = {
        (uint32_t)draw->kind, draw->asset, draw->flags,
        (uint32_t)draw->x, (uint32_t)draw->y,
        (uint32_t)draw->width, (uint32_t)draw->height,
        draw->u, draw->v, draw->clut, draw->tpage,
        (uint32_t)draw->red, (uint32_t)draw->green
    };
    o188_render(ram, hooks, "HudDrawTimeCell", 13u, args);
}

static int32_t o188_call(AeRam *ram, const AeOvl188Hooks *hooks,
                         const char *name, uint32_t address, uint32_t argc,
                         const uint32_t *args) {
    if (hooks == NULL || hooks->call == NULL) {
        ae_ram_fail(ram, "missing overlay 188 hook: %s (%08X)", name, address);
        return 0;
    }
    return hooks->call(hooks->user, ram, name, address, argc, args);
}

static void o188_render(AeRam *ram, const AeOvl188Hooks *hooks,
                        const char *name, uint32_t argc,
                        const uint32_t *args) {
    if (hooks == NULL || hooks->render == NULL) {
        ae_ram_fail(ram, "missing overlay 188 renderer hook: %s", name);
        return;
    }
    hooks->render(hooks->user, ram, name, argc, args);
}

static int32_t o188_area_printf(void *user, AeRam *ram, uint32_t a0,
                                uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    const uint32_t args[4] = { a0, a1, a2, a3 };
    return o188_call(ram, hooks, "printf", 0x800A3720u, 4u, args);
}

static int32_t o188_area_exit(void *user, AeRam *ram, uint32_t a0,
                              uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    const uint32_t args[1] = { a0 };
    (void)a1; (void)a2; (void)a3;
    return o188_call(ram, hooks, "Exit", 0x000000B0u, 1u, args);
}

static AeAreaSetHooks o188_area_set_hooks(const AeOvl188Hooks *hooks) {
    AeAreaSetHooks area_hooks = {0};
    area_hooks.user = (void *)hooks;
    area_hooks.Printf = o188_area_printf;
    area_hooks.Exit = o188_area_exit;
    return area_hooks;
}

static uint32_t o188_temp_stack(const AeOvl188Hooks *hooks,
                                uint32_t frame_size, uint32_t offset) {
    const uint32_t entry_sp = hooks != NULL && hooks->stack_pointer != 0u ?
        hooks->stack_pointer : O188_DEFAULT_STACK_POINTER;
    return entry_sp - frame_size + offset;
}

static const AeOvl188Hooks *o188_hooks_below_frame(
    const AeOvl188Hooks *hooks, uint32_t frame_size,
    AeOvl188Hooks *adjusted) {
    if (hooks == NULL) return NULL;
    *adjusted = *hooks;
    const uint32_t entry_sp = hooks->stack_pointer != 0u ?
        hooks->stack_pointer : O188_DEFAULT_STACK_POINTER;
    adjusted->stack_pointer = entry_sp - frame_size;
    return adjusted;
}

static int32_t o188_pad_get_state(void *user, AeRam *ram, int32_t port) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    return o188_call(ram, hooks, "PadGetState", 0x8002B52Cu, 1u,
                     (const uint32_t[]){ (uint32_t)port });
}

static int32_t o188_pad_info_mode(void *user, AeRam *ram, int32_t port,
                                  int32_t mode, int32_t index) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    return o188_call(ram, hooks, "PadInfoMode", 0x8002B5F8u, 3u,
                     (const uint32_t[]){ (uint32_t)port, (uint32_t)mode,
                                         (uint32_t)index });
}

static void o188_pad_setup_dualshock(void *user, AeRam *ram, int32_t arg) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    (void)o188_call(ram, hooks, "PadSetupDualShock", 0x80012CA0u, 1u,
                    (const uint32_t[]){ (uint32_t)arg });
}

static int32_t o188_pad_demo_step(void *user, AeRam *ram) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    return o188_call(ram, hooks, "PadDemoPlaybackStep", 0x80013128u, 0u,
                     NULL);
}

static void o188_pad_stage_request(void *user, AeRam *ram, int32_t mode,
                                   int32_t area, int32_t entry) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    if (hooks == NULL || hooks->stage_hooks == NULL) {
        ae_ram_fail(ram, "missing overlay 188 StageRequest hooks");
        return;
    }
    (void)ae_StageRequest(ram, hooks->stage_hooks, mode, area, entry);
}

static uint32_t o188_pad_stick_to_buttons(void *user, AeRam *ram) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    return (uint32_t)o188_call(ram, hooks, "StickToButtons", 0x800140B4u,
                               0u, NULL);
}

static void o188_pad_rumble_reset(void *user, AeRam *ram, int32_t port) {
    (void)user;
    ae_PadRumbleReset(ram, port);
}

static void o188_pad_update(AeRam *ram, const AeOvl188Hooks *hooks) {
    AePadHooks pad = {0};
    pad.user = (void *)hooks;
    pad.pad_get_state = o188_pad_get_state;
    pad.pad_info_mode = o188_pad_info_mode;
    pad.pad_setup_dualshock = o188_pad_setup_dualshock;
    pad.pad_demo_step = o188_pad_demo_step;
    pad.stage_request = o188_pad_stage_request;
    pad.stick_to_buttons = o188_pad_stick_to_buttons;
    pad.pad_rumble_reset = o188_pad_rumble_reset;
    ae_pad_PadUpdate(ram, &pad, 0u);
}

static int32_t o188_sound_wait_idle(void *user, AeRam *ram) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    return o188_call(ram, hooks, "SpuWaitIdle", 0x800A1398u, 0u, NULL);
}

static void o188_sound_wait_loop(void *user, AeRam *ram, uint32_t units) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    (void)o188_call(ram, hooks, "SpuWaitLoop", 0x8009D79Cu, 1u,
                    (const uint32_t[]){ units });
}

static void o188_sound_set_voice_adsr(void *user, AeRam *ram,
                                      uint32_t channel, uint16_t adsr1,
                                      uint16_t adsr2) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    const uint32_t args[3] = { channel, adsr1, adsr2 };
    o188_render(ram, hooks, "SpuSetVoiceADSR", 3u, args);
}

static void o188_sound_voice_key_off(void *user, AeRam *ram, uint32_t half,
                                     uint16_t mask) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    const uint32_t args[2] = { half, mask };
    o188_render(ram, hooks, "SpuVoiceKeyOff", 2u, args);
}

static void o188_sound_printf(void *user, AeRam *ram, uint32_t format,
                              uint32_t argc, int32_t a, int32_t b, int32_t c) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    const uint32_t args[4] = { format, argc, (uint32_t)a, (uint32_t)b };
    (void)c;
    (void)o188_call(ram, hooks, "printf", 0x800A3720u, 4u, args);
}

static void o188_sound_set_voice_volume(void *user, AeRam *ram,
                                        uint32_t channel, uint32_t side,
                                        uint16_t volume) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    const uint32_t args[3] = { channel, side, volume };
    o188_render(ram, hooks, "SpuSetVoiceVolume", 3u, args);
}

static AeSoundHw o188_sound_hw(const AeOvl188Hooks *hooks) {
    AeSoundHw sound = {0};
    sound.user = (void *)hooks;
    sound.wait_idle = o188_sound_wait_idle;
    sound.wait_loop = o188_sound_wait_loop;
    sound.set_voice_adsr = o188_sound_set_voice_adsr;
    sound.voice_key_off = o188_sound_voice_key_off;
    sound.printf_error = o188_sound_printf;
    sound.set_voice_volume = o188_sound_set_voice_volume;
    return sound;
}

static int32_t o188_stage_vsync(void *user, AeRam *ram,
                                uint32_t a0, uint32_t a1,
                                uint32_t a2, uint32_t a3) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    const uint32_t args[1] = { a0 };
    (void)a1; (void)a2; (void)a3;
    return o188_call(ram, hooks, "VSync", 0x8002E520u, 1u, args);
}

static int32_t o188_fade_seq_close(void *user, AeRam *ram,
                                   uint32_t a0, uint32_t a1,
                                   uint32_t a2, uint32_t a3) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    (void)a1; (void)a2; (void)a3;
    /* The original SoundStop leaves $a1 from SeqTrackSetPlayState live here. */
    return o188_call(ram, hooks, "SeqTrackClose", 0x800A1F3Cu, 2u,
                     (const uint32_t[]){ a0, 1u });
}

static int32_t o188_fade_seq_play_state(void *user, AeRam *ram,
                                        uint32_t a0, uint32_t a1,
                                        uint32_t a2, uint32_t a3) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    (void)a2; (void)a3;
    return o188_call(ram, hooks, "SeqTrackSetPlayState", 0x800A2B6Cu, 2u,
                     (const uint32_t[]){ a0, a1 });
}

static int32_t o188_fade_bank_free(void *user, AeRam *ram,
                                   uint32_t a0, uint32_t a1,
                                   uint32_t a2, uint32_t a3) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    (void)a1; (void)a2; (void)a3;
    return o188_call(ram, hooks, "SoundBankFree", 0x800A1CCCu, 1u,
                     (const uint32_t[]){ a0 });
}

static int32_t o188_fade_stop_voices(void *user, AeRam *ram,
                                    uint32_t a0, uint32_t a1,
                                    uint32_t a2, uint32_t a3) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    (void)a1; (void)a2; (void)a3;
    return o188_call(ram, hooks, "SoundStopAllVoices", 0x800A342Cu, 1u,
                     (const uint32_t[]){ a0 });
}

static int32_t o188_sound_stop(AeRam *ram, const AeOvl188Hooks *hooks,
                               int32_t mode) {
    const AeSoundHw sound = o188_sound_hw(hooks);
    AeStageHooks stage = {0};
    stage.user = (void *)hooks;
    stage.VSync = o188_stage_vsync;
    AeStageFadeHooks fade = {0};
    fade.user = (void *)hooks;
    fade.stack_pointer = hooks != NULL ? hooks->stack_pointer : 0u;
    fade.stage = &stage;
    fade.sound_hw = &sound;
    fade.SeqTrackClose = o188_fade_seq_close;
    fade.SeqTrackSetPlayState = o188_fade_seq_play_state;
    fade.SoundBankFree = o188_fade_bank_free;
    fade.SoundStopAllVoices = o188_fade_stop_voices;
    return ae_SoundStop(ram, &fade, mode);
}

static void o188_play_sfx(AeRam *ram, const AeOvl188Hooks *hooks,
                          uint32_t sound_id) {
    const AeSoundHw sound = o188_sound_hw(hooks);
    (void)ae_PlaySfx(ram, &sound, (int32_t)sound_id);
}

static int32_t o188_xa_play(AeRam *ram, const AeOvl188Hooks *hooks,
                            int32_t soundtrack) {
    /* XaPlay queues complete words from byte-sized locals. Preserve the
     * caller's untouched bytes at this nested call, as in member 18A. */
    const uint32_t filter_padding = hooks && hooks->xa_padding
        ? hooks->xa_padding(hooks->user, ram, 0u) : 0u;
    const uint32_t mode_padding = hooks && hooks->xa_padding
        ? hooks->xa_padding(hooks->user, ram, 1u) : 0u;
    for (uint32_t byte = 2u; byte < 4u; ++byte)
        ae_wr8(ram, AE_PLAYER1_XA_STACK_PARAMS + byte,
               filter_padding >> (byte * 8u));
    for (uint32_t byte = 1u; byte < 4u; ++byte)
        ae_wr8(ram, AE_PLAYER1_XA_STACK_MODE + byte,
               mode_padding >> (byte * 8u));
    return o188_call(ram, hooks, "XaPlay", 0x80011F6Cu, 1u,
                     (const uint32_t[]){ (uint32_t)soundtrack });
}

static void o188_player_dr_move(void *user, AeRam *ram, uint32_t command,
                                uint32_t rectangle, int32_t x, int32_t y) {
    const AeOvl188Hooks *hooks = (const AeOvl188Hooks *)user;
    if (rectangle == O188_PLAYER_EYE_RECT_STACK_TEMP) {
        const uint32_t nested_rectangle = rectangle - O188_RUN_B_STACK_BYTES;
        for (uint32_t byte = 0; byte < 8u; ++byte)
            ae_wr8(ram, nested_rectangle + byte,
                   ae_rd8(ram, rectangle + byte));
        rectangle = nested_rectangle;
    }
    const uint32_t args[4] = { command, rectangle, (uint32_t)x, (uint32_t)y };
    (void)o188_call(ram, hooks, "DR_MOVE", 0x8002B380u, 4u, args);
}

static void o188_player_set_eye_frame(AeRam *ram,
                                      const AeOvl188Hooks *hooks,
                                      int32_t frame) {
    AePlayerActHooks player = {0};
    player.user = (void *)hooks;
    player.DR_MOVE = o188_player_dr_move;
    ae_PlayerSetEyeFrame(ram, &player, frame);
}

#undef O188_PLAYER1_ADAPTER

static uint32_t o188_env_offset(uint32_t timer, uint32_t base) {
    uint32_t scaled = (timer << 7) + timer;
    scaled = (scaled << 4) - timer;
    scaled <<= 3;
    return base + scaled;
}

static void o188_restore_display(AeRam *ram, const AeOvl188Hooks *hooks,
                                 uint32_t display_base,
                                 uint32_t draw_base) {
    const uint32_t timer = ae_rd32(ram, O188_FRAME_COUNTER + O188_FRAME_TIMER_OFFSET);
    const uint32_t display = o188_env_offset(timer, display_base);
    const uint32_t draw = o188_env_offset(timer, draw_base);
    (void)o188_call(ram, hooks, "PutDispEnv", 0x80028EFCu, 1u,
                    (const uint32_t[]){ display });
    (void)o188_call(ram, hooks, "PutDrawEnv", 0x80028D30u, 1u,
                    (const uint32_t[]){ draw });
}

void ae_StageStartUi_LookAt(AeRam *ram, const AeOvl188Hooks *hooks,
                            uint32_t eye, uint32_t target) {
    const uint32_t input = o188_temp_stack(hooks, O188_LOOK_AT_FRAME_BYTES,
                                           O188_LOOK_AT_VECTOR_IN);
    const uint32_t output = o188_temp_stack(hooks, O188_LOOK_AT_FRAME_BYTES,
                                            O188_LOOK_AT_VECTOR_OUT);
    const uint32_t identity = o188_temp_stack(hooks, O188_LOOK_AT_FRAME_BYTES,
                                              O188_LOOK_AT_IDENTITY_TEMP);
    const uint32_t scale = o188_temp_stack(hooks, O188_LOOK_AT_FRAME_BYTES,
                                           O188_LOOK_AT_SCALE_TEMP);

    const int32_t eye_x = ae_rds16(ram, eye + O188_SVECTOR_X);
    const int32_t eye_y = ae_rds16(ram, eye + O188_SVECTOR_Y);
    const int32_t eye_z = ae_rds16(ram, eye + O188_SVECTOR_Z);
    int32_t dx = ae_rds16(ram, target + O188_SVECTOR_X) - eye_x;
    int32_t dz = ae_rds16(ram, target + O188_SVECTOR_Z) - eye_z;

    ae_wr32(ram, O188_LOOK_AT_TRANSLATION + O188_VECTOR_X, (uint32_t)-eye_x);
    ae_wr32(ram, O188_LOOK_AT_TRANSLATION + O188_VECTOR_Y, (uint32_t)-eye_y);
    ae_wr32(ram, O188_LOOK_AT_TRANSLATION + O188_VECTOR_Z, (uint32_t)-eye_z);
    ae_wr32(ram, input + O188_VECTOR_X, (uint32_t)dx);
    ae_wr32(ram, input + O188_VECTOR_Y, 0u);
    ae_wr32(ram, input + O188_VECTOR_Z, (uint32_t)dz);
    if (dx == 0 && dz == 0) ae_wr32(ram, input + O188_VECTOR_Z, 4096u);

    (void)ae_sdk_VectorNormal(ram, input, output);
    const int32_t horizontal_x = ae_rds32(ram, output + O188_VECTOR_X);
    const int32_t horizontal_z = ae_rds32(ram, output + O188_VECTOR_Z);
    const int32_t target_y = ae_rds16(ram, target + O188_SVECTOR_Y);
    ae_wr32(ram, input + O188_VECTOR_Y, (uint32_t)(target_y - eye_y));
    (void)ae_sdk_VectorNormal(ram, input, output);

    const int32_t normal_x = -horizontal_x;
    const int32_t normal_y = ae_rds32(ram, output + O188_VECTOR_Y);
    const int32_t normal_z = horizontal_z;
    const int32_t look_x = ae_rds32(ram, output + O188_VECTOR_X);
    const int32_t look_z = ae_rds32(ram, output + O188_VECTOR_Z);
    const int32_t horizontal_squared =
        (int32_t)((uint32_t)(look_z * look_z) +
                  (uint32_t)(look_x * look_x));
    const int32_t horizontal_length =
        ae_sdk_SquareRoot0(ram, horizontal_squared);

    const uint32_t m_xy = (uint32_t)normal_x * (uint32_t)normal_y;
    const uint32_t m_yz = (uint32_t)normal_y * (uint32_t)normal_z;
    const uint32_t m_xh = (uint32_t)horizontal_length * (uint32_t)normal_x;
    const uint32_t m_zh = (uint32_t)horizontal_length * (uint32_t)normal_z;
    ae_wr16(ram, O188_LOOK_AT_MATRIX + O188_MATRIX_M00, (uint16_t)normal_z);
    ae_wr16(ram, O188_LOOK_AT_MATRIX + O188_MATRIX_M01, 0u);
    ae_wr16(ram, O188_LOOK_AT_MATRIX + O188_MATRIX_M02, (uint16_t)normal_x);
    ae_wr16(ram, O188_LOOK_AT_MATRIX + O188_MATRIX_M10,
            (uint16_t)((int32_t)m_xy >> 12));
    ae_wr16(ram, O188_LOOK_AT_MATRIX + O188_MATRIX_M11, (uint16_t)horizontal_length);
    ae_wr16(ram, O188_LOOK_AT_MATRIX + O188_MATRIX_M12,
            (uint16_t)(-(int32_t)m_yz >> 12));
    ae_wr16(ram, O188_LOOK_AT_MATRIX + O188_MATRIX_M20,
            (uint16_t)(-(int32_t)m_xh >> 12));
    ae_wr16(ram, O188_LOOK_AT_MATRIX + O188_MATRIX_M21, (uint16_t)normal_y);
    ae_wr16(ram, O188_LOOK_AT_MATRIX + O188_MATRIX_M22,
            (uint16_t)((int32_t)m_zh >> 12));

    (void)ae_sdk_apply_matrix_lv(ae_ptr(ram, O188_LOOK_AT_MATRIX, 32u),
        ae_ptr(ram, O188_LOOK_AT_MATRIX + O188_MATRIX_TRANSLATION, 12u),
        ae_ptr(ram, O188_LOOK_AT_MATRIX + O188_MATRIX_TRANSLATION, 12u));
    (void)ae_sdk_MatrixIdentity(ram, identity);
    ae_wr32(ram, scale + O188_SCALE_X, 8192u);
    ae_wr32(ram, scale + O188_SCALE_Y, 8192u);
    ae_wr32(ram, scale + O188_SCALE_Z, 8192u);
    (void)o188_call(ram, hooks, "ScaleMatrixL", 0x800305B0u, 2u,
                    (const uint32_t[]){ identity, scale });
    (void)ae_sdk_CompMatrix(ram, O188_GTE_MATRIX_TEMP,
                            O188_LOOK_AT_MATRIX, O188_LOOK_AT_MATRIX);
    (void)ae_sdk_CompMatrix(ram, identity,
                            O188_LOOK_AT_MATRIX, O188_LOOK_AT_MATRIX);
}

uint32_t ae_StageStartUi_StringTableIndex(AeRam *ram, uint32_t strings,
                                          int32_t index) {
    uint32_t cursor = strings;
    int32_t skipped = 0;
    while (skipped < index) {
        if (ae_rd8(ram, cursor) == 0u) ++skipped;
        ++cursor;
    }
    return cursor;
}

void ae_StageStartUi_SetAreaMeshTable(AeRam *ram) {
    const uint32_t mesh_table = O188_AREA_DATA +
        (uint32_t)ae_rd16(ram, O188_AREA_DATA + O188_AREA_ZONE_TABLE_OFFSET) * 4u;
    ae_wr32(ram, O188_AREA_TABLE_PTR, mesh_table);
}

void ae_StageStartUi_MeshInit(AeRam *ram, const AeOvl188Hooks *hooks) {
    const uint32_t mesh_index = ae_rd16(ram, O188_AREA_DATA + O188_AREA_MESH_COUNT);
    const uint32_t mesh_header = O188_AREA_DATA + mesh_index * 4u;
    const uint32_t prop_instance = ae_rd32(ram, O188_AREA_PROPS) +
                                   O188_AREA_PROP_FIRST_INSTANCE;
    const uint32_t args[2] = { prop_instance, mesh_header };
    (void)o188_call(ram, hooks, "MeshInit", 0x80015C34u, 2u, args);
}

void ae_StageStartUi_AreaInit(AeRam *ram, const AeOvl188Hooks *hooks,
                              int32_t static_model_count) {
    const AeAreaSetHooks area_hooks = o188_area_set_hooks(hooks);
    const uint32_t area_mesh_table = O188_AREA_DATA +
        (uint32_t)ae_rd16(ram, O188_AREA_DATA + O188_AREA_MESH_TABLE_OFFSET) * 4u;
    const uint32_t area_prop_table = O188_AREA_DATA +
        (uint32_t)ae_rd16(ram, O188_AREA_DATA + O188_AREA_PROP_TABLE_OFFSET) * 4u;
    const uint32_t prop_count = ae_rd32(ram, area_prop_table);

    ae_wr32(ram, O188_STAGE_AREA_STATE + O188_STAGE_AREA_MESH_POINTER,
            area_mesh_table);
    ae_wr32(ram, O188_STAGE_AREA_STATE + O188_STAGE_AREA_PROP_POINTER,
            area_prop_table + O188_AREA_PROP_TABLE_COUNT_BYTES);
    ae_AreaSetZoneBoxes(ram, &area_hooks, 0u);
    ae_wr32(ram, O188_STAGE_AREA_STATE + O188_STAGE_AREA_PROP_COUNT,
            prop_count);
    ae_wr32(ram, O188_AREA_PROPS,
            ae_HeapAlloc(ram, &area_hooks,
                         (prop_count + 1u) * O188_AREA_PROP_INSTANCE_BYTES));

    for (uint32_t index = 0; index < prop_count; ++index) {
        const uint32_t placement = ae_rd32(ram,
            O188_STAGE_AREA_STATE + O188_STAGE_AREA_PROP_POINTER) +
                                   index * O188_AREA_PROP_RECORD_BYTES;
        const uint32_t instance = ae_rd32(ram, O188_AREA_PROPS) +
                                  index * O188_AREA_PROP_INSTANCE_BYTES;
        const uint32_t mesh_id = ae_rd16(ram, placement + O188_PROP_RECORD_MESH_INDEX);
        const uint32_t mesh_table = ae_rd32(ram,
            O188_STAGE_AREA_STATE + O188_STAGE_AREA_MESH_POINTER);
        const uint32_t mesh = mesh_table + mesh_id * 4u;
        const uint32_t mesh_header = mesh_table +
            (uint32_t)ae_rd16(ram, mesh + O188_AREA_MESH_HEADER_ANIMATION) * 4u;

        ae_wr16(ram, instance + O188_AREA_PROP_INSTANCE_MESH,
                ae_rd16(ram, placement + O188_PROP_RECORD_MESH_ANIMATION));
        ae_wr16(ram, instance + O188_AREA_PROP_INSTANCE_ANIM,
                ae_rd16(ram, placement + O188_PROP_RECORD_ANIMATION));
        ae_wr16(ram, instance + O188_AREA_PROP_INSTANCE_FRAME,
                ae_rd16(ram, placement + O188_PROP_RECORD_FRAME));
        (void)o188_call(ram, hooks, "MeshInit", 0x80015C34u, 2u,
                        (const uint32_t[]){ instance, mesh_header });
        ae_wr32(ram, instance + O188_AREA_PROP_INSTANCE_MODEL,
                ae_rd16(ram, placement + O188_PROP_RECORD_MODEL_INDEX));
        const uint32_t object_data = ae_rd32(ram, instance +
                                              O188_AREA_PROP_INSTANCE_MODEL_DATA);
        ae_wr32(ram, object_data,
                ae_rd32(ram, object_data) & 0xFFFF1FFFu);
        ae_wr16(ram, instance + O188_AREA_PROP_INSTANCE_HIDDEN, 0xFFFFu);
    }

    for (int32_t index = 0; index < static_model_count; ++index) {
        const uint32_t placement = O188_AREA_DATA + index * O188_STATIC_MODEL_RECORD_BYTES;
        const uint32_t instance = O188_STAGE_UI_STATIC_MODELS +
                                  index * O188_STATIC_MODEL_INSTANCE_BYTES;
        const uint32_t model_header = O188_AREA_DATA +
            (uint32_t)ae_rd16(ram, placement + O188_STATIC_MODEL_HEADER) * 4u;
        ae_ModelInit(ram, &area_hooks, instance, model_header);
        const uint32_t animation_index = ae_rd16(ram, placement +
                                                 O188_STATIC_MODEL_ANIM_INDEX);
        const uint32_t animation_table = O188_AREA_DATA +
                                         animation_index * O188_ANIMATION_RECORD_BYTES;
        const uint32_t animation_header = animation_table +
            (uint32_t)ae_rd32(ram, animation_table) * O188_ANIMATION_RECORD_BYTES +
            O188_AREA_ANIMATION_TABLE_ENTRIES;
        ae_wr32(ram, instance + O188_STATIC_MODEL_ANIM_HEADER,
                animation_header);
        ae_wr16(ram, instance + O188_STATIC_MODEL_FRAME, 0u);
        ae_wr16(ram, instance + O188_STATIC_MODEL_FRAME_ALT, 0u);
        ae_wr16(ram, instance + O188_STATIC_MODEL_LOOP_FRAME, 0u);
        ae_wr16(ram, instance + O188_STATIC_MODEL_LOOP_FRAME_ALT, 0u);
        ae_wr16(ram, instance + O188_MODEL_INSTANCE_POSITION_X, 0u);
        ae_wr16(ram, instance + O188_MODEL_INSTANCE_POSITION_Y, 0u);
        ae_wr16(ram, instance + O188_STATIC_MODEL_ANGLE, (uint16_t)-4096);
        ae_wr32(ram, instance + O188_STATIC_MODEL_ANIM_TABLE,
                O188_AREA_DATA +
                (uint32_t)ae_rd16(ram, placement + O188_STATIC_MODEL_ANIM_INDEX) *
                O188_ANIMATION_RECORD_BYTES +
                O188_AREA_ANIMATION_TABLE_ENTRIES);
    }
}

void ae_StageStartUi_ClampVerts(AeRam *ram, int32_t mode, int32_t count,
                                int32_t y) {
    if (mode != 0 && mode != 1) return;
    for (int32_t index = 0; index < count; ++index) {
        const uint32_t source = O188_SCREEN_VERTS +
                                (uint32_t)index * O188_SCREEN_VERT_BYTES;
        const uint32_t destination = O188_CLAMPED_SCREEN_VERTS +
                                     (uint32_t)index * O188_SCREEN_VERT_BYTES;
        const int32_t source_y = ae_rds16(ram, source + O188_SCREEN_VERT_Y);
        int32_t clamped_y = source_y;
        if (mode == 0 && source_y < y) clamped_y = y;
        if (mode == 1 && y < source_y) clamped_y = y;
        ae_wr16(ram, destination + O188_SCREEN_VERT_X,
                ae_rd16(ram, source + O188_SCREEN_VERT_X));
        ae_wr16(ram, destination + O188_SCREEN_VERT_Y, (uint16_t)clamped_y);
        ae_wr16(ram, destination + O188_SCREEN_VERT_Z,
                ae_rd16(ram, source + O188_SCREEN_VERT_Z));
    }
}

void ae_RippleGrid_Init2(AeRam *ram, const AeOvl188Hooks *hooks) {
    const AeAreaSetHooks area_hooks = o188_area_set_hooks(hooks);
    const uint32_t grid = ae_HeapAlloc(ram, &area_hooks, 0x600u);
    ae_wr32(ram, O188_STAGE_UI_RIPPLE_GRID, grid);

    for (int32_t row = 0; row < O188_RIPPLE_ROWS; ++row) {
        const int32_t radius = (300 * row * row) / 144;
        for (int32_t column = 0; column < O188_RIPPLE_COLUMNS; ++column) {
            const int32_t angle = row * 128 + column * 256;
            const uint32_t vertex = grid + (uint32_t)row * O188_RIPPLE_ROW_BYTES +
                                    (uint32_t)column * O188_RIPPLE_VERTEX_BYTES;
            const int32_t x = (ae_sdk_rcos(ram, angle) * radius) >> 12;
            const int32_t y = (ae_sdk_rsin(ram, angle) * radius) >> 12;
            ae_wr16(ram, vertex + O188_RIPPLE_X, (uint16_t)(x + 192));
            ae_wr16(ram, vertex + O188_RIPPLE_Y, (uint16_t)(y + 120));
            ae_wr32(ram, vertex + O188_RIPPLE_COLOUR, 0u);
        }
    }
}

int32_t ae_Mode_StageStart(AeRam *ram, const AeOvl188Hooks *hooks) {
    const uint32_t rectangle = o188_temp_stack(hooks, 40u, 16u);
    ae_wr32(ram, rectangle, ae_rd32(ram, O188_CLEAR_RECT_TEMPLATE));
    ae_wr32(ram, rectangle + 4u, ae_rd32(ram, O188_CLEAR_RECT_TEMPLATE + 4u));
    ae_wr32(ram, O188_MODE_WIPE_KIND, 0u);
    const uint32_t mode_state = O188_MODE_STATE;
    const uint32_t mode_address = mode_state + O188_MODE_VALUE;
    const uint32_t area_address = mode_state + O188_MODE_AREA;
    const int32_t mode = ae_rds16(ram, mode_address);
    AeOvl188Hooks mode_hooks_storage;
    const AeOvl188Hooks *mode_hooks = o188_hooks_below_frame(
        hooks, O188_MODE_STAGE_START_STACK_BYTES, &mode_hooks_storage);

    if (mode == 8) {
        (void)ae_StageStartUi_RunA(ram, mode_hooks);
        ae_wr16(ram, mode_address, 10u);
        ae_wr32(ram, O188_MODE_WIPE_KIND, 0u);
        ae_wr32(ram, O188_STAGE_STATUS_PARAMETER, 0u);
    }

    const uint32_t area = ae_rd16(ram, area_address);
    if (area == 6u && ae_ProgressGetByte(ram, 11u) != 5u) {
        const uint32_t vector = o188_temp_stack(hooks, 40u, 16u);
        (void)o188_call(ram, hooks, "ClearImage", 0x80028850u, 4u,
                        (const uint32_t[]){ vector, 0u, 0u, 0u });
        (void)o188_call(ram, hooks, "DrawSync", 0x800286CCu, 1u,
                        (const uint32_t[]){ 0u });
        ae_wr16(ram, mode_address, 26u);
        ae_wr16(ram, mode_state + O188_MODE_STATE_HANDOFF_PARAMETER, 7u);
        return 0;
    }

    (void)ae_StageStartUi_RunB(ram, mode_hooks);
    const int32_t after_mode = ae_rds16(ram, mode_address);
    if (after_mode == 18) ae_wr16(ram, mode_address, 19u);
    else if (after_mode == 23) ae_wr16(ram, mode_address, 24u);
    else ae_wr16(ram, mode_address, 11u);

    const int32_t level = o188_get_area_level(ram, area);
    const int32_t name_index = ae_rds8(ram,
        O188_STAGE_NAME_BY_LEVEL + (uint32_t)level);
    if (name_index != -1) {
        uint32_t source = ae_StageStartUi_StringTableIndex(
            ram, O188_STAGE_NAME_TABLE, name_index);
        uint32_t target = O188_STAGE_NAME_BUFFER;
        uint8_t byte;
        do {
            byte = ae_rd8(ram, source++);
            ae_wr8(ram, target++, byte);
        } while (byte != 0u);
    }

    (void)o188_level_info(ram, hooks, level, O188_LEVEL_SCRATCH);
    ae_wr32(ram, mode_state + O188_MODE_OBJECTIVES_STATE,
            ae_rd32(ram, O188_LEVEL_SCRATCH));
    (void)o188_call(ram, hooks, "DrawSync", 0x800286CCu, 1u,
                    (const uint32_t[]){ 0u });
    (void)o188_call(ram, hooks, "VSync", 0x8002E520u, 1u,
                    (const uint32_t[]){ 0u });
    return 0;
}

int32_t ae_StageStartUi_RunA(AeRam *ram, const AeOvl188Hooks *hooks) {
    const uint32_t temp_vector = o188_temp_stack(hooks, 64u, 16u);
    ae_wr32(ram, temp_vector, ae_rd32(ram, O188_CLEAR_RECT_TEMPLATE));
    ae_wr32(ram, temp_vector + 4u, ae_rd32(ram, O188_CLEAR_RECT_TEMPLATE + 4u));
    const uint32_t transform = O188_STAGE_UI_MODEL_TRANSFORM;
    const uint32_t ui = O188_STAGE_UI_STATE;
    const uint32_t mode_state = O188_MODE_STATE;
    const uint32_t area = ae_rd16(ram, mode_state + O188_MODE_AREA);
    AeOvl188Hooks run_hooks_storage;
    const AeOvl188Hooks *run_hooks = o188_hooks_below_frame(
        hooks, O188_RUN_A_STACK_BYTES, &run_hooks_storage);

    (void)o188_call(ram, hooks, "InitDisplay", 0x8005CFECu, 0u, NULL);
    (void)o188_call(ram, hooks, "ResetGraph", 0x80028350u, 1u,
                    (const uint32_t[]){ 1u });
    (void)o188_call(ram, hooks, "ClearImage", 0x80028850u, 4u,
                    (const uint32_t[]){ temp_vector, 0u, 0u, 0u });
    (void)o188_call(ram, hooks, "SetGeomScreen", 0x8002FAF0u, 1u,
                    (const uint32_t[]){ 328u });
    (void)o188_call(ram, hooks, "SetColorMatrixMode", 0x80015D58u, 4u,
                    (const uint32_t[]){ 0u, 0u, 0u, 0u });
    ae_wr32(ram, transform + O188_MODEL_TRANSFORM_SCALE_X, 0u);
    ae_wr32(ram, transform + O188_MODEL_TRANSFORM_SCALE_Y, 4096u);
    ae_wr32(ram, transform + O188_MODEL_TRANSFORM_SCALE_Z, 4096u);
    (void)o188_call(ram, hooks, "DrawSync", 0x800286CCu, 1u,
                    (const uint32_t[]){ 0u });
    ae_HeapInit(ram, NULL);

    ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_POINTER, O188_STAGE_SCRIPT_DEFAULT);
    ae_wr32(ram, ui + O188_MODE_UI_HIDE_OBJECTIVES, 0u);
    const int32_t area_kind = o188_get_area_kind(ram, area);
    uint32_t cadence = 0u;
    switch (area_kind) {
    case 2:
        cadence = 2u;
        ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_POINTER, O188_STAGE_SCRIPT_KIND_2);
        break;
    case 4:
        cadence = 4u;
        ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_POINTER, O188_STAGE_SCRIPT_KIND_4);
        break;
    case 5:
        cadence = 6u;
        ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_POINTER, O188_STAGE_SCRIPT_KIND_5);
        break;
    case 7:
        cadence = 8u;
        ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_POINTER, O188_STAGE_SCRIPT_KIND_7);
        break;
    case 10:
        cadence = 10u;
        ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_POINTER, O188_STAGE_SCRIPT_KIND_10);
        break;
    case 11:
        cadence = 12u;
        ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_POINTER, O188_STAGE_SCRIPT_KIND_11);
        ae_wr32(ram, ui + O188_MODE_UI_HIDE_OBJECTIVES, 1u);
        break;
    default:
        break;
    }

    ae_wr16(ram, O188_STAGE_AREA_KIND_COUNTER_A,
            (uint16_t)(ae_rd16(ram, O188_STAGE_AREA_KIND_COUNTER_A) + cadence));
    ae_wr16(ram, O188_STAGE_AREA_KIND_COUNTER_B,
            (uint16_t)(ae_rd16(ram, O188_STAGE_AREA_KIND_COUNTER_B) + cadence));
    (void)o188_call(ram, hooks, "ArcLoadEntry", 0x80011194u, 1u,
                    (const uint32_t[]){ O188_STAGE_ARC_LIST });

    ae_wr16(ram, ui + O188_MODE_UI_TEXT_CLUT, 0x3FDFu);
    ae_wr16(ram, ui + O188_MODE_UI_TEXT_TPAGE, 6u);
    ae_wr32(ram, ui + O188_MODE_UI_TIMER, 16u);
    ae_wr8(ram, O188_DISPLAY_READY, 1u);
    ae_wr8(ram, O188_FRAME_DRAWENV_ISBG, 1u);
    ae_wr8(ram, O188_DISPLAY_READY_SECONDARY, 0u);
    ae_wr8(ram, O188_FRAME_DRAWENV_BG_RED, 0u);
    ae_wr8(ram, O188_DISPLAY_READY_TERTIARY, 0u);
    ae_wr8(ram, O188_FRAME_DRAWENV_BG_GREEN, 0u);
    ae_wr8(ram, O188_DISPLAY_READY_QUATERNARY, 8u);
    ae_wr8(ram, O188_FRAME_DRAWENV_BG_BLUE, 8u);
    o188_pad_update(ram, hooks);

    for (;;) {
        (void)o188_call(ram, hooks, "BeginFrame", 0x8005DF48u, 0u, NULL);
        ae_ColorFadeReset(ram);
        if (ae_StageStartUi_DrawWipeA(ram, run_hooks) < 0) {
            (void)o188_call(ram, hooks, "AreaKindVisitedSet", 0x800964DCu,
                            1u, (const uint32_t[]){ area });
            return 0;
        }

        (void)o188_call(ram, hooks, "DrawSync", 0x800286CCu, 1u,
                        (const uint32_t[]){ 0u });
        (void)o188_call(ram, hooks, "VSync", 0x8002E520u, 1u,
                        (const uint32_t[]){ 2u });
        ae_wr32(ram, O188_FRAME_COUNTER,
                ae_rd32(ram, O188_FRAME_COUNTER) + 1u);

        const uint32_t wipe_kind = ae_rd32(ram, O188_MODE_WIPE_KIND);
        if (wipe_kind == 1u || wipe_kind == 3u) {
            const int32_t phase = ae_rds16(ram,
                ui + O188_MODE_UI_ANIMATION_FRAME);
            if (phase == 0) {
                ae_StageStartUi_SetDrawStp(ram, hooks);
                const uint32_t timer = ae_rd32(ram,
                    O188_FRAME_COUNTER + O188_FRAME_TIMER_OFFSET);
                (void)o188_call(ram, hooks, "PutDispEnv", 0x80028EFCu,
                    1u, (const uint32_t[]){ o188_env_offset(timer,
                                                              O188_FRAME_DISPLAY_ENV) });
                (void)o188_call(ram, hooks, "PutDrawEnv", 0x80028D30u,
                    1u, (const uint32_t[]){ o188_env_offset(timer,
                                                              O188_FRAME_DRAW_ENV) });
                ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_FRAME, 1u);
                ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_FRAME_LIMIT,
                        (uint16_t)timer);
            } else if (phase == 1 || phase == 2) {
                int32_t display_phase = ae_rds16(ram,
                    ui + O188_MODE_UI_ANIMATION_FRAME_LIMIT);
                if (phase == 1) {
                    ae_wr8(ram, O188_DISPLAY_READY, 0u);
                    ae_wr8(ram, O188_FRAME_DRAWENV_ISBG, 0u);
                }
                (void)o188_call(ram, hooks, "PutDispEnv", 0x80028EFCu,
                    1u, (const uint32_t[]){ o188_env_offset(
                        (uint32_t)display_phase, O188_FRAME_DISPLAY_ENV) });
                display_phase = (int16_t)(display_phase ^ 1);
                (void)o188_call(ram, hooks, "PutDrawEnv", 0x80028D30u,
                    1u, (const uint32_t[]){ o188_env_offset(
                        (uint32_t)display_phase, O188_FRAME_DRAW_ENV) });
                if (wipe_kind == 1u && phase == 2) {
                    ae_wr8(ram, O188_DISPLAY_READY, 1u);
                    ae_wr8(ram, O188_FRAME_DRAWENV_ISBG, 1u);
                    ae_wr32(ram, O188_MODE_WIPE_KIND, 2u);
                }
            }
        } else {
            o188_restore_display(ram, hooks, O188_FRAME_DISPLAY_ENV, O188_FRAME_DRAW_ENV);
        }

        const uint32_t timer = ae_rd32(ram, O188_FRAME_COUNTER + O188_FRAME_TIMER_OFFSET);
        const uint32_t otag = o188_env_offset(timer, O188_FRAME_COUNTER +
                                             O188_FRAME_PACKET_TABLE_OFFSET) + O188_FRAME_DISPLAY_ENV_OFFSET;
        (void)o188_call(ram, hooks, "DrawOTag", 0x80028CC0u, 1u,
                        (const uint32_t[]){ otag });
        (void)o188_call(ram, hooks, "CdMgrUpdate", 0x800125B8u, 0u, NULL);
        o188_pad_update(ram, hooks);
    }
}

static void o188_smooth_channel(AeRam *ram, uint32_t write_address,
                                uint32_t left_address, uint32_t right_address,
                                uint32_t center_address, uint32_t shift) {
    const uint32_t left = ae_rd8(ram, left_address);
    const uint32_t right = ae_rd8(ram, right_address);
    const uint32_t center = ae_rd8(ram, center_address);
    const uint32_t value = (left + right + center * 2u) >> 2;
    const uint32_t old = ae_rd32(ram, write_address & ~3u);
    const uint32_t lane_mask = 0xFFu << shift;
    const uint32_t updated = (old & ~lane_mask) | (value << shift);
    ae_wr32(ram, write_address & ~3u, updated);
}

void ae_RippleGrid_Smooth2(AeRam *ram) {
    const uint32_t grid = ae_rd32(ram, O188_STAGE_UI_RIPPLE_GRID);

    /* The source walks the work rows by 0x100 bytes.  Preserve that two-row stride,
       including its first and final edge rows, exactly as the overlay does. */
    uint32_t row_pointer = grid + O188_RIPPLE_LAST_ROW_OFFSET;
    for (int32_t rows_left = 12; rows_left >= 2; --rows_left) {
        for (int32_t column = 0; column < O188_RIPPLE_COLUMNS; ++column) {
            const uint32_t target = row_pointer +
                (uint32_t)column * O188_RIPPLE_VERTEX_BYTES;
            const uint32_t input_row = row_pointer - O188_RIPPLE_ROW_BYTES;
            const int32_t next = column == O188_RIPPLE_COLUMNS - 1 ? 0 : column + 1;
            const int32_t previous = column == 0 ? O188_RIPPLE_COLUMNS - 1 : column - 1;
            for (uint32_t channel = 0; channel < 3u; ++channel) {
                const uint32_t shift = channel * 8u;
                const uint32_t center_color = input_row +
                    (uint32_t)column * O188_RIPPLE_VERTEX_BYTES +
                    O188_RIPPLE_COLOUR + channel;
                const uint32_t left_color = input_row +
                    (uint32_t)previous * O188_RIPPLE_VERTEX_BYTES +
                    O188_RIPPLE_COLOUR + channel;
                const uint32_t right_color = input_row +
                    (uint32_t)next * O188_RIPPLE_VERTEX_BYTES +
                    O188_RIPPLE_COLOUR + channel;
                o188_smooth_channel(ram,
                    target + O188_RIPPLE_COLOUR + channel,
                    left_color, right_color, center_color, shift);
            }
        }
        row_pointer -= O188_RIPPLE_ROW_BYTES;
    }

    uint32_t clear_row = row_pointer + O188_RIPPLE_LAST_COLUMN_OFFSET;
    for (int32_t column = 0; column < O188_RIPPLE_COLUMNS; ++column) {
        ae_wr32(ram, clear_row + O188_RIPPLE_COLOUR, 0u);
        clear_row -= O188_RIPPLE_VERTEX_BYTES;
    }

    const uint32_t frame = ae_rd32(ram, O188_FRAME_COUNTER);
    if ((frame & 0xFFu) == 0u) return;

    uint32_t vertex = grid;
    int32_t angle = (int32_t)(frame << 5);
    for (int32_t column = 0; column < O188_RIPPLE_COLUMNS; ++column) {
        const uint32_t wave = (uint32_t)(ae_sdk_rsin(ram, angle) + 4096);
        ae_wr32(ram, vertex + O188_RIPPLE_COLOUR,
                ae_rd32(ram, vertex + O188_RIPPLE_COLOUR) +
                ((wave << 2) & 0xFF00u));
        angle += 256;
        vertex += O188_RIPPLE_VERTEX_BYTES;
    }

    vertex = grid;
    angle = (int32_t)((frame << 5) + 0x555u);
    for (int32_t column = 0; column < O188_RIPPLE_COLUMNS; ++column) {
        const uint32_t wave = (uint32_t)(ae_sdk_rsin(ram, angle) + 4096);
        ae_wr32(ram, vertex + O188_RIPPLE_COLOUR,
                ae_rd32(ram, vertex + O188_RIPPLE_COLOUR) +
                ((wave >> 6) & 0xFFu));
        angle += 256;
        vertex += O188_RIPPLE_VERTEX_BYTES;
    }

    vertex = grid;
    angle = (int32_t)((frame << 5) + 0xAAAu);
    for (int32_t column = 0; column < O188_RIPPLE_COLUMNS; ++column) {
        const uint32_t wave = (uint32_t)(ae_sdk_rsin(ram, angle) + 4096);
        ae_wr32(ram, vertex + O188_RIPPLE_COLOUR,
                ae_rd32(ram, vertex + O188_RIPPLE_COLOUR) +
                (((wave >> 6) & 0xFFu) << 16));
        angle += 256;
        vertex += O188_RIPPLE_VERTEX_BYTES;
    }
}

void ae_StageStartUi_DrawText(AeRam *ram, const AeOvl188Hooks *hooks,
                              uint32_t text) {
    int32_t x = ae_rds16(ram, O188_STAGE_UI_STATE + O188_STAGE_UI_TEXT_X);
    const int32_t y = ae_rds16(ram, O188_STAGE_UI_STATE + O188_STAGE_UI_TEXT_Y);
    const uint32_t clut = ae_rd16(ram,
        O188_STAGE_UI_STATE + O188_STAGE_UI_TEXT_CLUT);
    const uint32_t tpage = (ae_rd16(ram,
        O188_STAGE_UI_STATE + O188_STAGE_UI_TEXT_TPAGE) & 0x09FFu) |
        0xE1000200u;

    for (uint32_t cursor = text; ; ++cursor) {
        const uint32_t glyph = ae_rd8(ram, cursor);
        if (glyph == 0u) break;
        const uint32_t u = (glyph & 0x0Fu) << 4;
        const uint32_t v = glyph & 0xF0u;
        const uint32_t draw[11] = {
            glyph, (uint32_t)x, (uint32_t)y, tpage, clut,
            u, v, 16u, 16u, 5u, 102u
        };
        o188_render(ram, hooks, "StageStartUi_DrawGlyph", 11u, draw);
        x = (int16_t)(x + (int32_t)ae_rd8(ram, O188_TEXT_WIDTHS + glyph));
    }

    ae_wr16(ram, O188_STAGE_UI_STATE + O188_STAGE_UI_TEXT_X,
            (uint16_t)x);
}

void ae_RippleGrid_Draw2(AeRam *ram, const AeOvl188Hooks *hooks) {
    ae_RippleGrid_Smooth2(ram);
    const uint32_t packet_cursor = ae_rd32(ram,
        O188_RENDER_PACKET_CURSOR);
    const uint32_t args[3] = {
        ae_rd32(ram, O188_STAGE_UI_RIPPLE_GRID),
        O188_RIPPLE_COLUMNS,
        O188_RIPPLE_ROWS
    };
    o188_render(ram, hooks, "RippleGrid_Draw2", 3u, args);
    ae_wr32(ram, O188_RENDER_PACKET_CURSOR,
            packet_cursor + O188_RIPPLE_DRAW_PACKET_BYTES);
}

void ae_StageStartUi_DrawTitleLines(AeRam *ram, const AeOvl188Hooks *hooks,
                                    uint32_t records, int32_t count) {
    if (count <= 0 || ae_rds16(ram, records) < 0) return;
    const uint32_t ui = O188_STAGE_UI_STATE;
    const uint32_t record = records;
    const uint32_t index = (uint32_t)ae_rds16(ram, record);
    const uint32_t text = ae_StageStartUi_StringTableIndex(
        ram, O188_STAGE_NAME_STRINGS, (int32_t)index);
    ae_wr16(ram, ui + O188_STAGE_UI_TEXT_X,
            ae_rd16(ram, record + O188_STAGE_TITLE_RECORD_X));
    ae_wr16(ram, ui + O188_STAGE_UI_TEXT_Y,
            ae_rd16(ram, record + O188_STAGE_TITLE_RECORD_Y));
    ae_StageStartUi_DrawText(ram, hooks, text);

    if (ae_rds16(ram, record + O188_STAGE_TITLE_RECORD_LIMIT) < 0) return;
    const uint32_t second = record + O188_STAGE_TITLE_RECORD_STRIDE;
    const uint32_t second_index = (uint32_t)ae_rds16(ram, second);
    const uint32_t second_text = ae_StageStartUi_StringTableIndex(
        ram, O188_STAGE_NAME_STRINGS, (int32_t)second_index);
    ae_wr16(ram, ui + O188_STAGE_UI_TEXT_X,
            ae_rd16(ram, second + O188_STAGE_TITLE_RECORD_X));
    ae_wr16(ram, ui + O188_STAGE_UI_TEXT_Y,
            ae_rd16(ram, second + O188_STAGE_TITLE_RECORD_Y));
    ae_StageStartUi_DrawText(ram, hooks, second_text);

    if (ae_rds16(ram, record + 2u * O188_STAGE_TITLE_RECORD_STRIDE) < 0) return;
    const uint32_t third = record + 2u * O188_STAGE_TITLE_RECORD_STRIDE;
    const uint32_t third_index = (uint32_t)ae_rds16(ram, third);
    const uint32_t third_text = ae_StageStartUi_StringTableIndex(
        ram, O188_STAGE_NAME_STRINGS, (int32_t)third_index);
    ae_wr16(ram, ui + O188_STAGE_UI_TEXT_X,
            ae_rd16(ram, third + O188_STAGE_TITLE_RECORD_X));
    ae_wr16(ram, ui + O188_STAGE_UI_TEXT_Y,
            ae_rd16(ram, third + O188_STAGE_TITLE_RECORD_Y));
    ae_StageStartUi_DrawText(ram, hooks, third_text);
}

void ae_StageStartUi_DrawStageIcon(AeRam *ram,
                                    const AeOvl188Hooks *hooks) {
    const uint32_t mode = ae_rd16(ram, O188_MODE_STATE + O188_MODE_VALUE);
    const uint32_t area = ae_rd16(ram, O188_MODE_STATE + O188_MODE_AREA);
    const uint32_t frame = ae_rd32(ram, O188_MODE_STAGE_INDEX);
    const uint32_t args[5] = {
        area, mode, frame,
        ae_rd32(ram, O188_RENDER_PACKET_CURSOR),
        ae_rd32(ram, O188_RENDER_ORDER_TABLE)
    };
    o188_render(ram, hooks, "StageStartUi_DrawStageIcon", 5u, args);
}

void ae_StageStartUi_SetDrawStp(AeRam *ram,
                                 const AeOvl188Hooks *hooks) {
    const uint32_t packet_cursor = ae_rd32(ram, O188_RENDER_PACKET_CURSOR);
    const uint32_t args[4] = {
        ae_rd32(ram, O188_FRAME_COUNTER + O188_FRAME_TIMER_OFFSET),
        packet_cursor,
        ae_rd32(ram, O188_RENDER_ORDER_TABLE),
        O188_STAGE_UI_STATE
    };
    o188_render(ram, hooks, "StageStartUi_SetDrawStp", 4u, args);
    ae_wr32(ram, O188_RENDER_PACKET_CURSOR,
            packet_cursor + O188_DRAW_STP_PACKET_BYTES);
}

void ae_StageStartUi_DrawCounter(AeRam *ram, const AeOvl188Hooks *hooks,
                                 int32_t x, int32_t y, int32_t value) {
    int32_t shown = value;
    const int32_t stored = ae_rds32(ram, O188_MODE_COUNTER);
    const int32_t maximum = stored >> 1;
    if (maximum < shown) {
        shown = maximum;
        ae_wr32(ram, O188_MODE_COUNTER, (uint32_t)(stored + 1));
    }

    int32_t tens = o188_signed_div(ram, shown, 10);
    int32_t ones = shown - tens * 10;
    const uint32_t first = tens == 0 ? 10u : (uint32_t)(tens - 1);
    const uint32_t second = ones == 0 ? 9u : (uint32_t)(ones - 1);
    const uint32_t args[5] = {
        (uint32_t)x, (uint32_t)y, first, second,
        ae_rd32(ram, O188_RENDER_PACKET_CURSOR)
    };
    o188_render(ram, hooks, "StageStartUi_DrawCounterDigits", 5u, args);
}

void ae_StageStartUi_CopyStrip(AeRam *ram,
                                const AeOvl188Hooks *hooks) {
    const uint32_t type = ae_rd32(ram, O188_STAGE_UI_DRAW_MODE);
    const uint32_t frame = ae_rd32(ram, O188_FRAME_COUNTER + O188_FRAME_TIMER_OFFSET);
    const uint32_t args[5] = {
        type, frame, ae_rd32(ram, O188_RENDER_PACKET_CURSOR),
        ae_rd32(ram, O188_RENDER_ORDER_TABLE), O188_STAGE_UI_STATE
    };
    o188_render(ram, hooks, "StageStartUi_CopyStrip", 5u, args);
}

void ae_StageStartUi_CopyBuffer(AeRam *ram, const AeOvl188Hooks *hooks,
                                int32_t row) {
    const uint32_t source_y = (uint32_t)row +
        (ae_rd32(ram, O188_FRAME_COUNTER + O188_FRAME_TIMER_OFFSET) != 0u ? 0x100u : 0u);
    const uint32_t args[5] = {
        source_y, ae_rd32(ram, O188_FRAME_COUNTER + O188_FRAME_TIMER_OFFSET),
        ae_rd32(ram, O188_RENDER_PACKET_CURSOR),
        ae_rd32(ram, O188_RENDER_ORDER_TABLE), 0x02000000u
    };
    o188_render(ram, hooks, "StageStartUi_CopyBuffer", 5u, args);
}

void ae_StageStartUi_WipeQuadA(AeRam *ram, const AeOvl188Hooks *hooks,
                               int32_t x, int32_t height) {
    const uint32_t args[6] = {
        (uint32_t)x, (uint32_t)height,
        (uint32_t)ae_rds16(ram, O188_STAGE_UI_STATE +
                           O188_MODE_UI_ANIMATION_FRAME_LIMIT),
        ae_rd32(ram, O188_FRAME_COUNTER + O188_FRAME_TIMER_OFFSET),
        ae_rd32(ram, O188_RENDER_PACKET_CURSOR),
        ae_rd32(ram, O188_RENDER_ORDER_TABLE)
    };
    o188_render(ram, hooks, "StageStartUi_WipeQuadA", 6u, args);
    ae_wr32(ram, O188_RENDER_PACKET_CURSOR,
            args[4] + O188_WIPE_QUAD_A_PACKET_BYTES);
}

void ae_StageStartUi_WipeQuadB(AeRam *ram, const AeOvl188Hooks *hooks,
                               int32_t x, int32_t height) {
    const uint32_t args[6] = {
        (uint32_t)x, (uint32_t)height,
        (uint32_t)ae_rds16(ram, O188_STAGE_UI_STATE +
                           O188_MODE_UI_ANIMATION_FRAME_LIMIT),
        ae_rd32(ram, O188_FRAME_COUNTER + O188_FRAME_TIMER_OFFSET),
        ae_rd32(ram, O188_RENDER_PACKET_CURSOR),
        ae_rd32(ram, O188_RENDER_ORDER_TABLE)
    };
    o188_render(ram, hooks, "StageStartUi_WipeQuadB", 6u, args);
    ae_wr32(ram, O188_RENDER_PACKET_CURSOR,
            args[4] + O188_WIPE_QUAD_B_PACKET_BYTES);
}

void ae_StageStartUi_DrawScanlines(AeRam *ram,
                                   const AeOvl188Hooks *hooks) {
    const uint32_t frame = ae_rd32(ram, O188_FRAME_COUNTER);
    const uint32_t quotient = (uint32_t)(((uint64_t)frame *
        0x1B4E81B5u) >> 37);
    const uint32_t phase = 260u - (frame - quotient * 316u);

    int32_t timer = ae_rds32(ram, O188_MODE_SCANLINE_TIMER);
    ae_wr32(ram, O188_MODE_SCANLINE_TIMER, (uint32_t)(timer - 1));
    uint32_t bank = timer < 11 ? 10u : 0u;
    if (timer <= 0) {
        const uint32_t texture_phase = (uint16_t)(ae_rd16(ram, O188_UI_TEXTURE_SEED) * 5u + 1u);
        ae_wr16(ram, O188_UI_TEXTURE_SEED, (uint16_t)texture_phase);
        const uint32_t high = (texture_phase >> 8) & 0xFFu;
        const uint32_t texture_remainder = high - (high / 200u) * 200u;
        timer = (int32_t)(texture_remainder + 20u);
        ae_wr32(ram, O188_MODE_SCANLINE_TIMER, (uint32_t)timer);
    }

    uint16_t texture_seed = ae_rd16(ram, O188_UI_TEXTURE_SEED);
    const uint32_t args[5] = {
        frame, phase, (uint32_t)timer,
        texture_seed | (bank << 16), O188_RENDER_PACKET_CURSOR
    };
    o188_render(ram, hooks, "StageStartUi_DrawScanlines", 5u, args);
    for (uint32_t row = 0; row < 120u; ++row) {
        texture_seed = (uint16_t)(texture_seed * 5u + 1u);
        const uint32_t remainder = ((texture_seed >> 8) % 100u);
        if (bank >= remainder)
            texture_seed = (uint16_t)(texture_seed * 5u + 1u);
    }
    ae_wr16(ram, O188_UI_TEXTURE_SEED, texture_seed);
}

static uint32_t o188_number_glyph(int32_t value) {
    return (uint32_t)(value < 6 ? value + 106 : value + 116);
}

static void o188_number_pair(AeRam *ram, uint32_t output, int32_t value) {
    const int32_t tens = o188_signed_div(ram, value, 10);
    const int32_t ones = value - tens * 10;
    ae_wr8(ram, output, tens == 0 ? 0xFFu : (uint32_t)(tens + 106));
    ae_wr8(ram, output + O188_NUMBER_NEXT_DIGIT_OFFSET,
           o188_number_glyph(ones));
}

static void o188_number_suffix(AeRam *ram, uint32_t output, int32_t value) {
    if (value < 10) {
        ae_wr8(ram, output, o188_number_glyph(value));
        ae_wr8(ram, output + O188_NUMBER_NEXT_DIGIT_OFFSET, 0u);
        return;
    }
    o188_number_pair(ram, output, value);
}

void ae_StageStartUi_DrawNumber(AeRam *ram,
                                const AeOvl188Hooks *hooks) {
    const uint32_t scratch = O188_LEVEL_SCRATCH;
        const uint32_t digits = scratch + O188_NUMBER_DIGITS_BUFFER;
    const int32_t first = ae_rds16(ram, scratch + O188_NUMBER_SCRATCH_FIRST);
    const int32_t second = ae_rds16(ram, scratch + O188_NUMBER_SCRATCH_SECOND);
    o188_number_pair(ram, digits, first);
    ae_wr8(ram, digits + O188_NUMBER_SEPARATOR_OFFSET, 141u);
    o188_number_suffix(ram, digits + O188_NUMBER_SUFFIX_OFFSET, second);
    ae_wr8(ram, digits + O188_NUMBER_TERMINATOR_OFFSET, 0u);
    ae_wr16(ram, O188_STAGE_UI_STATE + O188_STAGE_UI_TEXT_X, 64u);
    ae_wr16(ram, O188_STAGE_UI_STATE + O188_STAGE_UI_TEXT_Y, 184u);
    ae_StageStartUi_DrawText(ram, hooks, digits);

    const uint32_t area = ae_rd16(ram, O188_MODE_STATE + O188_MODE_AREA);
    const int32_t level = o188_get_area_level(ram, area);
    const int32_t caught = o188_call(ram, hooks, "LevelCaughtCount",
                                      0x80097FB0u, 1u,
                                      (const uint32_t[]){ (uint32_t)level });
    o188_number_pair(ram, digits, caught);
    ae_wr8(ram, digits + O188_NUMBER_SEPARATOR_OFFSET, 141u);
    o188_number_suffix(ram, digits + O188_NUMBER_SUFFIX_OFFSET,
                       ae_rds16(ram, scratch + O188_NUMBER_SCRATCH_SUFFIX));
    ae_wr8(ram, digits + O188_NUMBER_TERMINATOR_OFFSET, 0u);
    ae_wr16(ram, O188_STAGE_UI_STATE + O188_STAGE_UI_TEXT_X, 180u);
    ae_wr16(ram, O188_STAGE_UI_STATE + O188_STAGE_UI_TEXT_Y, 184u);
    ae_StageStartUi_DrawText(ram, hooks, digits);

}

static int32_t o188_level_info(AeRam *ram, const AeOvl188Hooks *hooks,
                               int32_t level, uint32_t output) {
    return o188_call(ram, hooks, "LevelGetInfo", 0x80097D0Cu, 2u,
                     (const uint32_t[]){ (uint32_t)level, output });
}

static void o188_draw_time(AeRam *ram, const AeOvl188Hooks *hooks,
                           int32_t x, int32_t y, int32_t frames) {
    AeHudHooks hud_hooks = {0};
    hud_hooks.user = (void *)hooks;
    hud_hooks.stack_pointer = hooks != NULL ? hooks->stack_pointer : 0u;
    hud_hooks.draw_sprite = o188_hud_sprite;
    ae_HudDrawTime(ram, &hud_hooks, x, y, frames);
}

void ae_StageStartUi_DrawObjectives(AeRam *ram,
                                    const AeOvl188Hooks *hooks) {
    const uint32_t area = ae_rd16(ram, O188_MODE_STATE + O188_MODE_AREA);
    if (area == 19u || area == 36u) {
        ae_StageStartUi_DrawTitleLines(ram, hooks, O188_STAGE_TEXT_HUB_TITLE, 3);
        return;
    }

    const int32_t level = o188_get_area_level(ram, area);
    (void)o188_level_info(ram, hooks, level, O188_LEVEL_SCRATCH);
    const int32_t mode = ae_rds16(ram, O188_MODE_STATE + O188_MODE_VALUE);
    if (mode == 18) {
        const int32_t counter = ae_rds16(ram,
                                         O188_LEVEL_SCRATCH + O188_LEVEL_INFO_COUNTER);
        ae_StageStartUi_DrawTitleLines(ram, hooks, O188_STAGE_TEXT_MODE18_MAIN, 3);
        ae_StageStartUi_DrawCounter(ram, hooks, 128, 88, counter);

        const int32_t table_value = o188_call(ram, hooks, "LevelTableGet",
                                               0x80097E60u, 1u,
                                               (const uint32_t[]){ (uint32_t)level });
        const int32_t frames = ae_rds32(ram,
                                        O188_MODE_STATE + O188_MODE_PROGRESS_CURRENT);
        if (frames != -1) {
            o188_draw_time(ram, hooks, 260, 192, frames);
            (void)o188_call(ram, hooks, "HudDrawTimeRankIcon", 0x8005F68Cu,
                            4u, (const uint32_t[]){ 236u, 184u,
                                (uint32_t)frames, (uint32_t)table_value });
        }
        ae_StageStartUi_DrawTitleLines(ram, hooks, O188_STAGE_TEXT_MODE18_RANK, 1);
        return;
    }

    if (mode != 10) return;
    const int32_t objective_kind = ae_rds16(ram,
                                            O188_LEVEL_SCRATCH + O188_LEVEL_INFO_KIND);
    if (objective_kind == 1) {
        (void)o188_level_info(ram, hooks, 24,
                              O188_LEVEL_SCRATCH + O188_LEVEL_INFO_FOLLOWUP);
        const int32_t total = ae_rds16(ram,
                                       O188_LEVEL_SCRATCH + O188_LEVEL_INFO_TOTAL);
        if (total > 0) {
            const int32_t current = ae_rds16(ram,
                                             O188_LEVEL_SCRATCH + O188_LEVEL_INFO_CURRENT);
            const int32_t target = ae_rds16(ram,
                                            O188_LEVEL_SCRATCH + O188_LEVEL_INFO_REQUIRED);
            ae_StageStartUi_DrawTitleLines(ram, hooks, O188_STAGE_TEXT_OBJECTIVE_CAPTURE, 3);
            ae_StageStartUi_DrawCounter(ram, hooks, 144, 88, current - target);
        } else {
            ae_StageStartUi_DrawTitleLines(ram, hooks, O188_STAGE_TEXT_OBJECTIVE_EMPTY, 2);
        }
        ae_StageStartUi_DrawNumber(ram, hooks);
        return;
    }
    if (objective_kind == 2) {
        ae_StageStartUi_DrawTitleLines(ram, hooks, O188_STAGE_TEXT_OBJECTIVE_SPECIAL, 2);
        ae_wr16(ram, O188_OBJECTIVE_COUNTER, 0u);
        ae_StageStartUi_DrawNumber(ram, hooks);
        return;
    }
    if (objective_kind == 0) {
        if (level >= 22 && level <= 30)
            ae_StageStartUi_DrawTitleLines(ram, hooks, O188_STAGE_TEXT_OBJECTIVE_LEVELS, 2);
        const int32_t required = ae_rds16(ram,
                                          O188_LEVEL_SCRATCH + O188_LEVEL_INFO_REQUIRED);
        const int32_t result = ae_rds16(ram,
                                        O188_LEVEL_SCRATCH + O188_LEVEL_INFO_RESULT);
        if (required > 0) {
            ae_StageStartUi_DrawTitleLines(ram, hooks, O188_STAGE_TEXT_OBJECTIVE_COUNT, 3);
            ae_StageStartUi_DrawCounter(ram, hooks, 48, 88,
                                        result - required);
        } else {
            ae_StageStartUi_DrawTitleLines(ram, hooks, O188_STAGE_TEXT_MODE18_MAIN, 3);
            ae_StageStartUi_DrawCounter(ram, hooks, 48, 88, result);
        }
    }
    ae_StageStartUi_DrawNumber(ram, hooks);
}

static int32_t o188_load_step(AeRam *ram, const AeOvl188Hooks *hooks) {
    return o188_call(ram, hooks, "LoadListStep", 0x800116C0u, 0u, NULL);
}

static void o188_sound_restore(AeRam *ram, const AeOvl188Hooks *hooks) {
    (void)o188_call(ram, hooks, "SoundStopAndRestoreVolume", 0x80086170u,
                    0u, NULL);
}

static void o188_draw_area_props(AeRam *ram, const AeOvl188Hooks *hooks,
                                 uint32_t area_props) {
    (void)o188_call(ram, hooks, "DrawAreaProps", 0x80016F34u, 1u,
                    (const uint32_t[]){ area_props });
}

static void o188_run_ui_scene(AeRam *ram, const AeOvl188Hooks *hooks,
                              uint32_t eye, uint32_t target,
                              uint32_t objects) {
    (void)ae_StageStartUi_RunScript(ram, hooks);
    ae_StageStartUi_LookAt(ram, hooks, eye, target);
    const uint32_t area_props = o188_temp_stack(
        hooks, O188_LOOK_AT_FRAME_BYTES, O188_LOOK_AT_IDENTITY_TEMP);
    o188_draw_area_props(ram, hooks, area_props);
    ae_StageStartUi_UpdateObjects(ram, hooks, objects);
}

static void o188_draw_wipe_a_scene(AeRam *ram,
                                   const AeOvl188Hooks *hooks,
                                   const AeOvl188Hooks *frame_hooks) {
    const uint32_t ui = O188_STAGE_UI_STATE;
    o188_run_ui_scene(ram, frame_hooks,
                      ui + O188_MODE_UI_VECTOR_A_X,
                      ui + O188_MODE_UI_VECTOR_B_X,
                      O188_STAGE_UI_OBJECTS);
    const uint32_t area = ae_rd16(ram,
        O188_MODE_STATE + O188_MODE_AREA);
    if (o188_get_area_kind(ram, area) == 10)
        ae_StageStartUi_CopyStrip(ram, frame_hooks);
    const uint32_t args[3] = {
        ae_rd32(ram, O188_FRAME_COUNTER + O188_FRAME_TIMER_OFFSET), area,
        ae_rd32(ram, O188_RENDER_PACKET_CURSOR)
    };
    o188_render(ram, hooks, "StageStartUi_DrawWipeBackground", 3u, args);
    ae_StageStartUi_DrawScanlines(ram, hooks);
}

static void o188_wipe_coordinates(int32_t frame, int32_t *x,
                                  int32_t *height) {
    if (frame < 6) {
        *x = (frame * 3 * 64) / 5;
        *height = 2;
        return;
    }
    *x = 192;
    *height = (frame - 5) * 30;
    if (*height > 128) *height = 128;
}

int32_t ae_StageStartUi_DrawWipeA(AeRam *ram,
                                  const AeOvl188Hooks *hooks) {
    const uint32_t ui = O188_STAGE_UI_STATE;
    const uint32_t wipe_mode = ae_rd32(ram, O188_MODE_WIPE_KIND);
    AeOvl188Hooks frame_hooks_storage;
    const AeOvl188Hooks *frame_hooks = o188_hooks_below_frame(
        hooks, O188_DRAW_WIPE_A_STACK_BYTES, &frame_hooks_storage);
    if (wipe_mode == 0u) {
        if (ae_rds16(ram, O188_MODE_WIPE_A_SOUND_PENDING) != 0 &&
            o188_load_step(ram, hooks) < 0) {
            o188_sound_restore(ram, hooks);
            ae_wr16(ram, O188_MODE_WIPE_A_SOUND_PENDING, 0u);
        }
        int32_t timer = (int16_t)(ae_rd16(ram, O188_MODE_WIPE_A_TIMER) + 2u);
        ae_wr16(ram, O188_MODE_WIPE_A_TIMER, (uint16_t)timer);
        if (timer < 121 || ae_rds16(ram, O188_MODE_WIPE_A_SOUND_PENDING) != 0)
            return 0;
        ae_wr16(ram, O188_MODE_WIPE_A_TIMER, 0u);

        const uint32_t area = ae_rd16(ram, O188_MODE_STATE + O188_MODE_AREA);
        const int32_t area_kind = o188_get_area_kind(ram, area);
        ae_StageStartUi_AreaInit(ram, frame_hooks,
                                 area_kind == 10 ? 2 : 1);
        if (ae_rd32(ram, ui + O188_MODE_UI_HIDE_OBJECTIVES) != 0u)
            ae_StageStartUi_MeshInit(ram, frame_hooks);
        ae_StageStartUi_SetAreaMeshTable(ram);
        o188_run_ui_scene(ram, frame_hooks,
                          ui + O188_MODE_UI_VECTOR_A_X,
                          ui + O188_MODE_UI_VECTOR_B_X,
                          O188_STAGE_UI_OBJECTS);
        ae_wr32(ram, O188_MODE_WIPE_KIND, 1u);
        ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_FRAME, 0u);
        ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_TIMER, 0u);
        o188_play_sfx(ram, hooks, O188_SOUND_SELECT);
        return 0;
    }

    if (wipe_mode == 1u) {
        int32_t x, height;
        o188_wipe_coordinates(ae_rds16(ram, ui + O188_MODE_UI_ANIMATION_TIMER),
                              &x, &height);
        ae_StageStartUi_WipeQuadA(ram, hooks, x, height);
        int32_t timer = (int16_t)(ae_rd16(ram, ui +
                                         O188_MODE_UI_ANIMATION_TIMER) + 1u);
        ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_TIMER, (uint16_t)timer);
        if (timer >= 11)
            ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_FRAME, 2u);
        return 0;
    }

    if (wipe_mode == 2u) {
        const uint32_t input = ae_rd32(ram, O188_INPUT_BUTTONS_PRESSED) &
                               ae_rd32(ram, O188_INPUT_BUTTONS) &
                               O188_INPUT_ACCEPT_MASK;
        const uint32_t script = ae_rd32(ram, O188_STAGE_UI_STATE +
                                        O188_MODE_UI_SCRIPT_POINTER);
        const int32_t script_done = ae_rds16(ram, script) == 0x3FC0;
        if (input != 0u || script_done) {
            o188_play_sfx(ram, hooks, O188_SOUND_CONFIRM);
            ae_wr16(ram, O188_MODE_WIPE_A_SOUND_PENDING,
                    1u);
            ae_wr32(ram, O188_MODE_WIPE_KIND, 3u);
            ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_FRAME, 0u);
            ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_TIMER, 10u);
        }
        o188_draw_wipe_a_scene(ram, hooks, frame_hooks);
        return 0;
    }

    if (wipe_mode == 3u) {
        if (ae_rds16(ram, ui + O188_MODE_UI_ANIMATION_FRAME) < 2) {
            int32_t x, height;
            o188_wipe_coordinates(ae_rds16(ram, ui +
                                           O188_MODE_UI_ANIMATION_TIMER),
                                  &x, &height);
            ae_StageStartUi_WipeQuadB(ram, hooks, x, height);
        }
        int32_t timer = (int16_t)(ae_rd16(ram, ui +
                                         O188_MODE_UI_ANIMATION_TIMER) - 1u);
        ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_TIMER, (uint16_t)timer);
        if (timer < 0) {
            (void)o188_sound_stop(ram, hooks, 1);
            return -1;
        }
        return 0;
    }
    o188_draw_wipe_a_scene(ram, hooks, frame_hooks);
    return 0;
}

int32_t ae_StageStartUi_DrawWipeB(AeRam *ram,
                                  const AeOvl188Hooks *hooks) {
    const uint32_t ui = O188_STAGE_UI_STATE;
    const uint32_t wipe_mode = ae_rd32(ram, O188_MODE_WIPE_KIND);
    AeOvl188Hooks frame_hooks_storage;
    const AeOvl188Hooks *frame_hooks = o188_hooks_below_frame(
        hooks, O188_DRAW_WIPE_B_STACK_BYTES, &frame_hooks_storage);
    if (wipe_mode == 0u) {
        if (ae_rds16(ram, O188_MODE_WIPE_B_SOUND_PENDING) != 0 &&
            o188_load_step(ram, hooks) < 0) {
            o188_sound_restore(ram, hooks);
            ae_wr16(ram, O188_MODE_WIPE_B_SOUND_PENDING, 0u);
        }
        int32_t timer = (int16_t)(ae_rd16(ram, O188_MODE_WIPE_B_TIMER) + 2u);
        ae_wr16(ram, O188_MODE_WIPE_B_TIMER, (uint16_t)timer);
        if (timer < 121 || ae_rds16(ram, O188_MODE_WIPE_B_SOUND_PENDING) != 0)
            return 0;
        ae_wr16(ram, O188_MODE_WIPE_B_TIMER, 0u);
        ae_StageStartUi_AreaInit(ram, frame_hooks, 2);
        const int32_t mode = ae_rds16(ram, O188_MODE_STATE + O188_MODE_VALUE);
        if (mode == 18) {
            ae_wr16(ram, O188_STAGE_AREA_KIND_HEADER_A, 3u);
        } else if (mode == 23) {
            ae_wr16(ram, O188_STAGE_AREA_KIND_HEADER_A, 4u);
            ae_wr16(ram, O188_STAGE_AREA_KIND_HEADER_C, 6u);
            ae_wr16(ram, O188_STAGE_AREA_KIND_HEADER_B, 5u);
            ae_wr8(ram, O188_STAGE_AREA_KIND_HEADER_FLAGS, 0xFFu);
            const uint32_t variant = ae_rd16(ram,
                O188_MODE_STATE + O188_MODE_AREA_VARIANT);
            if (variant == 7u) {
                ae_wr8(ram, O188_MODE_STATE + O188_MODE_RETRY_FLAG, 2u);
                ae_wr8(ram, O188_MODE_STATE + O188_MODE_RETRY_FLAG +
                       O188_MODE_RETRY_FLAG_SECOND_BYTE, 0u);
            } else if (variant == 14u) {
                ae_wr8(ram, O188_MODE_STATE + O188_MODE_RETRY_FLAG, 5u);
                ae_wr8(ram, O188_MODE_STATE + O188_MODE_RETRY_FLAG +
                       O188_MODE_RETRY_FLAG_SECOND_BYTE, 0u);
            }
        }
        (void)ae_StageStartUi_RunScript(ram, frame_hooks);
        ae_StageStartUi_LookAt(ram, frame_hooks, O188_STAGE_UI_LOOK_AT_EYE,
                               O188_STAGE_UI_LOOK_AT_TARGET);
        ae_StageStartUi_UpdateObjects(ram, frame_hooks,
                                      O188_STAGE_UI_OBJECTS);
        ae_wr32(ram, O188_MODE_WIPE_KIND, 1u);
        ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_FRAME, 0u);
        ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_TIMER, 0u);
        o188_play_sfx(ram, hooks, O188_SOUND_SELECT);
        return 0;
    }

    if (wipe_mode == 1u) {
        int32_t x, height;
        o188_wipe_coordinates(ae_rds16(ram, ui + O188_MODE_UI_ANIMATION_TIMER),
                              &x, &height);
        ae_StageStartUi_WipeQuadA(ram, hooks, x, height);
        int32_t timer = (int16_t)(ae_rd16(ram, ui +
                                         O188_MODE_UI_ANIMATION_TIMER) + 1u);
        ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_TIMER, (uint16_t)timer);
        if (timer >= 11)
            ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_FRAME, 2u);
        return 0;
    }

    if (wipe_mode == 2u) {
        const uint32_t input = ae_rd32(ram, O188_INPUT_BUTTONS_PRESSED) &
                               ae_rd32(ram, O188_INPUT_BUTTONS) &
                               O188_INPUT_ACCEPT_MASK;
        const uint32_t script = ae_rd32(ram, O188_STAGE_UI_STATE +
                                        O188_MODE_UI_SCRIPT_POINTER);
        if (input != 0u || ae_rds16(ram, script) == 0x3FC0) {
            o188_play_sfx(ram, hooks, O188_SOUND_CONFIRM);
            ae_wr16(ram, O188_MODE_WIPE_B_SOUND_PENDING,
                    (uint16_t)wipe_mode);
            ae_wr32(ram, O188_MODE_WIPE_KIND, 3u);
            ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_FRAME, 0u);
            ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_TIMER, 10u);
        }
        (void)ae_StageStartUi_RunScript(ram, frame_hooks);
        ae_StageStartUi_LookAt(ram, frame_hooks,
                               ui + O188_MODE_UI_VECTOR_A_X,
                               ui + O188_MODE_UI_VECTOR_B_X);
        ae_StageStartUi_UpdateObjects(ram, frame_hooks,
                                      O188_STAGE_UI_OBJECTS);
        ae_RippleGrid_Draw2(ram, frame_hooks);
        return 0;
    }

    if (wipe_mode == 3u) {
        if (ae_rds16(ram, O188_MODE_WIPE_B_SOUND_PENDING) == 1 &&
            o188_load_step(ram, hooks) < 0) {
            ae_wr16(ram, O188_MODE_WIPE_B_SOUND_PENDING, 0u);
        }
        if (ae_rds16(ram, ui + O188_MODE_UI_ANIMATION_FRAME) < 2) {
            int32_t x, height;
            o188_wipe_coordinates(ae_rds16(ram, ui +
                                           O188_MODE_UI_ANIMATION_TIMER),
                                  &x, &height);
            ae_StageStartUi_WipeQuadB(ram, hooks, x, height);
            int32_t timer = (int16_t)(ae_rd16(ram, ui +
                                             O188_MODE_UI_ANIMATION_TIMER) - 1u);
            ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_TIMER, (uint16_t)timer);
            if (timer < 0) {
                const uint32_t area = ae_rd16(ram,
                    O188_MODE_STATE + O188_MODE_AREA);
                (void)o188_call(ram, hooks, "LoadListStart", 0x80011104u,
                                3u, (const uint32_t[]){ 1u, area,
                                                       O188_LOAD_LIST_START_WIDTH });
                ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_FRAME, 2u);
                ae_wr16(ram, O188_MODE_WIPE_B_SOUND_PENDING, 1u);
                if (area != 19u && area != 36u)
                    (void)o188_sound_stop(ram, hooks, 1);
            }
            if (ae_rds16(ram, O188_MODE_WIPE_B_SOUND_PENDING) == 0) {
                ae_wr16(ram, O188_MODE_WIPE_B_SOUND_PENDING, 1u);
                return -1;
            }
            return 0;
        }
        if (ae_rds16(ram, O188_MODE_WIPE_B_SOUND_PENDING) != 0) return 0;
        ae_wr16(ram, O188_MODE_WIPE_B_SOUND_PENDING, 1u);
        return -1;
    }

    (void)ae_StageStartUi_RunScript(ram, frame_hooks);
    ae_StageStartUi_LookAt(ram, frame_hooks,
                           ui + O188_MODE_UI_VECTOR_A_X,
                           ui + O188_MODE_UI_VECTOR_B_X);
    ae_StageStartUi_UpdateObjects(ram, frame_hooks,
                                  O188_STAGE_UI_OBJECTS);
    ae_RippleGrid_Draw2(ram, frame_hooks);
    return 0;
}

static void o188_interpolate_vector(AeRam *ram, uint32_t ui,
                                    uint32_t current_offset,
                                    uint32_t target_offset,
                                    uint32_t timer_offset) {
    int32_t remaining = ae_rds16(ram, ui + timer_offset);
    if (remaining <= 0) return;
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        const uint32_t current_address = ui + current_offset + axis * 2u;
        const uint32_t target_address = ui + target_offset + axis * 2u;
        const int32_t current = ae_rds16(ram, current_address);
        const int32_t target = ae_rds16(ram, target_address);
        const int32_t step = o188_signed_div(ram, target - current, remaining);
        ae_wr16(ram, current_address, (uint16_t)(current + step));
    }
    ae_wr16(ram, ui + timer_offset, (uint16_t)(remaining - 1));
}

static void o188_script_read_vector(AeRam *ram, uint32_t ui,
                                    uint32_t offset, uint32_t *script,
                                    uint32_t count) {
    *script += 2u;
    for (uint32_t axis = 0; axis < count; ++axis) {
        ae_wr16(ram, ui + offset + axis * 2u, ae_rd16(ram, *script));
        *script += 2u;
    }
}

static void o188_script_tick(AeRam *ram, uint32_t ui) {
    uint32_t flags = ae_rd32(ram, ui + O188_MODE_UI_SCRIPT_FLAGS);
    if ((flags & 1u) == 0u) return;
    uint32_t tick = ae_rd32(ram, ui + O188_MODE_UI_SCRIPT_TICK);
    if (tick <= 0xFFFEu) ++tick;
    flags |= 2u;
    ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_TICK, tick);
    ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_FLAGS, flags);
}

static int32_t o188_script_command(AeRam *ram, const AeOvl188Hooks *hooks,
                                   uint32_t ui, uint32_t *script) {
    const uint16_t command = (uint16_t)ae_rd16(ram, *script);
    if (command == 0x3FC0u) return 1;
    const uint32_t encoded = command & 0x3FC0u;
    const uint32_t index = command & 0x3Fu;
    switch (encoded) {
    case 0x0000u: {
        if (index < 5u) {
            const uint32_t object = O188_STAGE_UI_OBJECTS + index * O188_OBJECT_BYTES;
            *script += 2u;
            ae_wr16(ram, object + O188_OBJECT_FLAGS, ae_rd16(ram, *script));
            *script += 2u;
        } else {
            *script += 2u;
        }
        break;
    }
    case 0x0040u: {
        if (index < 5u) {
            const uint32_t object = O188_STAGE_UI_OBJECTS + index * O188_OBJECT_BYTES;
            *script += 2u;
            for (uint32_t axis = 0; axis < 4u; ++axis) {
                ae_wr16(ram, object + O188_OBJECT_TRANSLATION_TARGET_X + axis * 2u,
                        ae_rd16(ram, *script));
                *script += 2u;
            }
        } else if (index == 16u) {
            o188_script_read_vector(ram, ui, O188_MODE_UI_VECTOR_B_X,
                                    script, 3u);
        } else if (index == 17u) {
            o188_script_read_vector(ram, ui, O188_MODE_UI_VECTOR_A_X,
                                    script, 3u);
        } else if (index == 18u) {
            o188_script_read_vector(ram, ui, O188_MODE_UI_VECTOR_B_TARGET_X,
                                    script, 4u);
        } else if (index == 19u) {
            o188_script_read_vector(ram, ui, O188_MODE_UI_VECTOR_A_TARGET_X,
                                    script, 4u);
        } else {
            *script += 2u;
        }
        break;
    }
    case 0x0080u: {
        if (index < 5u) {
            const uint32_t object = O188_STAGE_UI_OBJECTS + index * O188_OBJECT_BYTES;
            *script += 2u;
            for (uint32_t axis = 0; axis < 4u; ++axis) {
                ae_wr16(ram, object + O188_OBJECT_TARGET_X + axis * 2u,
                        ae_rd16(ram, *script));
                *script += 2u;
            }
        } else {
            *script += 2u;
        }
        break;
    }
    case 0x00C0u: {
        const uint32_t line_index = ae_rd16(ram,
                                             *script + O188_SCRIPT_OPERAND_OFFSET);
        ae_wr8(ram, O188_SCRIPT_STAGE_OBJECTIVE_LINES +
               line_index * O188_SCRIPT_OBJECTIVE_LINE_STRIDE +
               O188_SCRIPT_OBJECTIVE_LINE_FLAGS,
               1u);
        *script += 4u;
        break;
    }
    case 0x0140u: {
        if (index < 5u) {
            const uint32_t object = O188_STAGE_UI_OBJECTS + index * O188_OBJECT_BYTES;
            ae_wr16(ram, object + O188_OBJECT_ANIMATION_LOOP,
                    ae_rd16(ram, *script + O188_SCRIPT_OPERAND_OFFSET));
            *script += 4u;
        } else {
            *script += 2u;
        }
        break;
    }
    case 0x0180u: {
        if (index < 5u) {
            const uint32_t object = O188_STAGE_UI_OBJECTS + index * O188_OBJECT_BYTES;
            const int32_t animation = ae_rds16(ram,
                                                *script + O188_SCRIPT_OPERAND_OFFSET);
            const uint32_t table = ae_rd32(ram,
                object + O188_OBJECT_ANIMATION_HEADER);
            const uint32_t animation_record = table +
                (uint32_t)animation * O188_ANIMATION_RECORD_BYTES;
            ae_wr16(ram, object + O188_OBJECT_ANIMATION_INDEX,
                    (uint16_t)animation);
            ae_wr16(ram, object + O188_OBJECT_ANIMATION_LOOP,
                    (uint16_t)animation);
            ae_wr16(ram, object + O188_OBJECT_ANIMATION_FRAME,
                    ae_rd16(ram, animation_record));
            *script += 4u;
        } else {
            *script += 2u;
        }
        break;
    }
    case 0x01C0u:
        ae_wr32(ram, ui + O188_MODE_SCRIPT_WAIT,
                (uint32_t)ae_rds16(ram, *script + O188_SCRIPT_OPERAND_OFFSET));
        *script += 4u;
        break;
    case 0x0200u: {
        const int32_t wait = ae_rds32(ram, ui + O188_MODE_SCRIPT_WAIT);
        if (wait > 0) {
            ae_wr32(ram, ui + O188_MODE_SCRIPT_WAIT, (uint32_t)(wait - 1));
            return 2;
        }
        *script += 2u;
        break;
    }
    case 0x0240u:
        ae_wr32(ram, ui + O188_MODE_SCRIPT_RETURN,
                *script + O188_SCRIPT_OPERAND_OFFSET);
        *script += 2u;
        break;
    case 0x0280u:
        *script = ae_rd32(ram, ui + O188_MODE_SCRIPT_RETURN);
        return 2;
    case 0x0300u:
        ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_FLAGS, 1u);
        *script += 2u;
        break;
    case 0x0340u: {
        const uint32_t flags = ae_rd32(ram, ui + O188_MODE_UI_SCRIPT_FLAGS);
        if ((flags & 2u) == 0u) {
            const int32_t wait = ae_rds32(ram, ui + O188_MODE_SCRIPT_WAIT);
            ae_wr32(ram, ui + O188_MODE_SCRIPT_WAIT, (uint32_t)(wait - 1));
            return 2;
        }
        *script += 2u;
        break;
    }
    case 0x0380u: {
        const uint32_t limit = (uint32_t)ae_rds16(
            ram, *script + O188_SCRIPT_OPERAND_OFFSET);
        const uint32_t count = ae_rd32(ram, ui + O188_MODE_UI_SCRIPT_COUNT);
        if (count < limit) return 2;
        *script += 4u;
        break;
    }
    case 0x03C0u: {
        const uint32_t area = ae_rd16(ram, O188_MODE_STATE + O188_MODE_AREA);
        int32_t soundtrack = 0;
        if (area == 19u || area == 36u) {
            soundtrack = 40;
        } else {
            const int32_t kind = o188_get_area_kind(ram, area);
            switch (kind) {
            case 2: soundtrack = 17; break;
            case 4: soundtrack = 18; break;
            case 5: soundtrack = 19; break;
            case 7: soundtrack = 20; break;
            case 10: soundtrack = 21; break;
            case 11: soundtrack = 22; break;
            default: soundtrack = 16; break;
            }
        }
        (void)o188_xa_play(ram, hooks, soundtrack);
        *script += 2u;
        break;
    }
    default:
        *script += 2u;
        break;
    }
    return 0;
}

void ae_StageStartUi_RunScript(AeRam *ram,
                               const AeOvl188Hooks *hooks) {
    const uint32_t ui = O188_STAGE_UI_STATE;
    AeOvl188Hooks script_hooks_storage;
    const AeOvl188Hooks *script_hooks = o188_hooks_below_frame(
        hooks, O188_RUN_SCRIPT_STACK_BYTES, &script_hooks_storage);
    o188_script_tick(ram, ui);
    uint32_t script = ae_rd32(ram, ui + O188_MODE_SCRIPT_POINTER);
    for (;;) {
        const int32_t result = o188_script_command(ram, script_hooks, ui, &script);
        if (result != 0 || ae_ram_failed(ram)) break;
    }
    ae_wr32(ram, ui + O188_MODE_SCRIPT_POINTER, script);
    o188_interpolate_vector(ram, ui, O188_MODE_UI_VECTOR_A_X,
                            O188_MODE_UI_VECTOR_A_TARGET_X,
                            O188_MODE_UI_VECTOR_A_TIME);
    o188_interpolate_vector(ram, ui, O188_MODE_UI_VECTOR_B_X,
                            O188_MODE_UI_VECTOR_B_TARGET_X,
                            O188_MODE_UI_VECTOR_B_TIME);

    uint32_t line = O188_SCRIPT_STAGE_OBJECTIVE_LINES;
    for (;;) {
        const int32_t line_flag = ae_rds8(ram,
                                          line + O188_SCRIPT_OBJECTIVE_LINE_FLAGS);
        if (line_flag < 0) break;
        if (line_flag != 0) {
            ae_wr16(ram, ui + O188_STAGE_UI_TEXT_X, ae_rd16(ram, line));
            ae_wr16(ram, ui + O188_STAGE_UI_TEXT_Y,
                    ae_rd16(ram, line + O188_SCRIPT_OBJECTIVE_LINE_Y));
            const uint32_t line_type = ae_rd8(ram,
                                               line + O188_SCRIPT_OBJECTIVE_LINE_TYPE);
            if (line_type == 0u) {
                ae_StageStartUi_DrawObjectives(ram, script_hooks);
            } else if (line_type == 2u) {
                const uint32_t stage = ae_rd32(ram, O188_MODE_STAGE_INDEX);
                const uint32_t cycle = stage - (stage / 30u) * 30u;
                if (cycle < 15u) {
                    ae_StageStartUi_DrawStageIcon(ram, script_hooks);
                    const uint32_t area = ae_rd16(ram,
                        O188_MODE_STATE + O188_MODE_AREA);
                    if (cycle == 0u && area != 19u && area != 36u)
                        o188_play_sfx(ram, hooks, O188_SOUND_STAGE_START);
                }
            }
        }
        line += O188_SCRIPT_OBJECTIVE_LINE_STRIDE;
    }
}

static void o188_interpolate_object_position(AeRam *ram, uint32_t object) {
    int32_t remaining = ae_rds16(ram,
        object + O188_OBJECT_POSITION_TIME);
    if (remaining <= 0) return;
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        const uint32_t position = object + O188_OBJECT_POSITION_X + axis * 2u;
        const uint32_t target = object + O188_OBJECT_TARGET_X + axis * 2u;
        const int32_t current_value = ae_rds16(ram, position);
        const int32_t target_value = ae_rds16(ram, target);
        const int32_t step = o188_signed_div(ram,
            target_value - current_value, remaining);
        ae_wr16(ram, position, (uint16_t)(current_value + step));
    }
    ae_wr16(ram, object + O188_OBJECT_POSITION_TIME,
            (uint16_t)(remaining - 1));
}

static void o188_interpolate_object_translation(AeRam *ram,
                                                uint32_t object) {
    int32_t remaining = ae_rds16(ram,
        object + O188_OBJECT_TRANSLATION_TIME);
    if (remaining <= 0) return;
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        const uint32_t position = object + O188_OBJECT_TRANSLATION_X + axis * 4u;
        const uint32_t target = object + O188_OBJECT_TRANSLATION_TARGET_X + axis * 2u;
        const int32_t current = ae_rds32(ram, position);
        const int32_t target_value = ae_rds16(ram, target);
        const int32_t difference = (int32_t)((uint32_t)target_value -
                                             (uint32_t)current);
        const int32_t step = o188_signed_div(ram, difference, remaining);
        ae_wr32(ram, position, (uint32_t)current + (uint32_t)step);
    }
    ae_wr16(ram, object + O188_OBJECT_TRANSLATION_TIME,
            (uint16_t)(remaining - 1));
}

static int32_t o188_render_call(AeRam *ram, const AeOvl188Hooks *hooks,
                                const char *name, uint32_t address,
                                uint32_t argc, const uint32_t *args) {
    (void)address;
    o188_render(ram, hooks, name, argc, args);
    return 0;
}

void ae_StageStartUi_UpdateObjects(AeRam *ram,
                                   const AeOvl188Hooks *hooks,
                                   uint32_t objects) {
    const uint32_t vector = o188_temp_stack(hooks, 64u, 16u);
    const uint32_t game_mode = ae_rd16(ram,
        O188_MODE_STATE + O188_MODE_VALUE);
    const uint32_t draw_context = O188_FRAME_COUNTER;
    for (uint32_t index = 0; index < 5u; ++index) {
        const uint32_t object = objects + index * O188_OBJECT_BYTES;
        const uint32_t flags = ae_rd16(ram,
            object + O188_OBJECT_FLAGS);
        if ((flags & 1u) == 0u) continue;

        o188_interpolate_object_position(ram, object);
        o188_interpolate_object_translation(ram, object);

        (void)ae_sdk_VectorNormalSS(ram,
            object + O188_OBJECT_POSITION_X, vector);
        const uint32_t direction_matrix = ae_rd32(ram,
            object + O188_OBJECT_DIRECTION_MATRIX);
        (void)ae_sdk_MatrixFromDirXZ(ram, vector, direction_matrix);

        const uint32_t frame_address = object + O188_OBJECT_ANIMATION_FRAME;
        const uint32_t index_address = object + O188_OBJECT_ANIMATION_INDEX;
        const uint32_t header = ae_rd32(ram,
            object + O188_OBJECT_ANIMATION_HEADER);
        const int32_t frame = (int16_t)(ae_rd16(ram, frame_address) + 1u);
        ae_wr16(ram, frame_address, (uint16_t)frame);
        const uint32_t animation_index = ae_rd16(ram, index_address);
        const int32_t last_frame = ae_rds16(ram,
            header + animation_index * O188_ANIMATION_RECORD_BYTES +
            O188_ANIMATION_RECORD_END_FRAME);
        if (last_frame < frame) {
            const int32_t loop_index = ae_rds16(ram,
                object + O188_OBJECT_ANIMATION_LOOP);
            if (loop_index != -1) {
                const uint32_t loop_frame = ae_rd16(ram,
                    header + (uint32_t)loop_index * O188_ANIMATION_RECORD_BYTES +
                    O188_ANIMATION_RECORD_FIRST_FRAME);
                ae_wr16(ram, frame_address, loop_frame);
                ae_wr16(ram, index_address, (uint16_t)loop_index);
            } else {
                ae_wr16(ram, frame_address, (uint16_t)(frame - 1));
            }
        }

        ae_ModelPoseBones(ram, object, 1u);
        const uint32_t transform_args[1] = { object };
        o188_render_call(ram, hooks, "ModelTransformSkinned", 0x80016A60u,
                         1u, transform_args);

        if ((flags & 2u) != 0u) {
            if (game_mode == 23u) continue;
            const uint32_t script = ae_rd32(ram,
                O188_STAGE_UI_STATE + O188_MODE_UI_SCRIPT_POINTER);
            int32_t offset;
            if (script == O188_STAGE_SCRIPT_MODE_18) {
                const int32_t wave = ae_sdk_rsin(ram,
                    (int32_t)(ae_rd32(ram, O188_FRAME_COUNTER) << 4));
                offset = o188_signed_div(ram, wave, 80) + 150;
            } else {
                const int32_t wave = ae_sdk_rsin(ram,
                    (int32_t)(ae_rd32(ram, O188_FRAME_COUNTER) << 4));
                offset = o188_signed_div(ram, wave, 50) + 140;
            }
            const uint32_t model = ae_rd32(ram,
                object + O188_OBJECT_MODEL_MESH);
            const int32_t vertex_count = ae_rd16(
                ram, model + O188_MODEL_HEADER_VERTEX_COUNT);
            ae_StageStartUi_ClampVerts(ram, 1, vertex_count, offset);
            const uint32_t args[3] = {
                ae_rd32(ram, O188_RENDER_ORDER_TABLE),
                O188_CLAMPED_SCREEN_VERTS, object
            };
            o188_render_call(ram, hooks, "ModelDraw", 0x80017CD4u,
                             3u, args);
            ae_StageStartUi_ClampVerts(ram, 0, vertex_count, offset);
            o188_render_call(ram, hooks, "ModelEmitPrimsScratchColors",
                             0x8001C918u, 3u, args);
            ae_StageStartUi_CopyBuffer(ram, hooks, offset);
        } else {
            const uint32_t animation_slot = ae_rd32(ram,
                draw_context + O188_MODE_UI_TIMER);
            const uint32_t slot_context = draw_context + O188_FRAME_PACKET_TABLE_OFFSET +
                                          animation_slot * O188_FRAME_BUFFER_STRIDE;
            const uint32_t args[3] = {
                slot_context, O188_SCREEN_VERTS, object
            };
            o188_render_call(ram, hooks, "ModelDraw", 0x80017CD4u,
                             3u, args);
            if (ae_rd32(ram, O188_STAGE_UI_STATE + O188_OBJECT_MODEL_ACTIVE) != 0u) {
                const uint32_t area_prop = ae_rd32(ram, O188_AREA_PROPS) +
                                           O188_AREA_PROP_FIRST_INSTANCE;
                const uint32_t transform_args[3] = {
                    area_prop, O188_STAGE_PROP_MATRIX, O188_SCREEN_VERTS
                };
                o188_render_call(ram, hooks, "ModelTransform", 0x800160ECu,
                                 3u, transform_args);
                o188_render_call(ram, hooks, "ModelDrawTransformed",
                                 0x80017EFCu, 4u,
                                 (const uint32_t[]){
                                     ae_rd32(ram, O188_RENDER_ORDER_TABLE),
                                     O188_SCREEN_VERTS, area_prop, 0u });
            }
        }
    }
}

int32_t ae_StageStartUi_RunB(AeRam *ram, const AeOvl188Hooks *hooks) {
    const uint32_t temp_vector = o188_temp_stack(hooks, 64u, 16u);
    ae_wr32(ram, temp_vector, ae_rd32(ram, O188_CLEAR_RECT_TEMPLATE));
    ae_wr32(ram, temp_vector + 4u, ae_rd32(ram, O188_CLEAR_RECT_TEMPLATE + 4u));
    const uint32_t transform = O188_STAGE_UI_MODEL_TRANSFORM;
    const uint32_t ui = O188_STAGE_UI_STATE;
    const uint32_t mode_state = O188_MODE_STATE;
    const uint32_t area = ae_rd16(ram, mode_state + O188_MODE_AREA);
    AeOvl188Hooks run_hooks_storage;
    const AeOvl188Hooks *run_hooks = o188_hooks_below_frame(
        hooks, O188_RUN_B_STACK_BYTES, &run_hooks_storage);

    (void)o188_call(ram, hooks, "InitDisplay", 0x8005CFECu, 0u, NULL);
    (void)o188_call(ram, hooks, "ResetGraph", 0x80028350u, 1u,
                    (const uint32_t[]){ 1u });
    (void)o188_call(ram, hooks, "ClearImage", 0x80028850u, 4u,
                    (const uint32_t[]){ temp_vector, 0u, 0u, 0u });
    (void)o188_call(ram, hooks, "SetGeomScreen", 0x8002FAF0u, 1u,
                    (const uint32_t[]){ 328u });
    (void)o188_call(ram, hooks, "SetColorMatrixMode", 0x80015D58u, 4u,
                    (const uint32_t[]){ 0u, 0u, 0u, 0u });
    ae_wr32(ram, transform + O188_MODEL_TRANSFORM_SCALE_X, 0u);
    ae_wr32(ram, transform + O188_MODEL_TRANSFORM_SCALE_Y, 4096u);
    ae_wr32(ram, transform + O188_MODEL_TRANSFORM_SCALE_Z, 4096u);
    (void)o188_call(ram, hooks, "DrawSync", 0x800286CCu, 1u,
                    (const uint32_t[]){ 0u });
    ae_HeapInit(ram, NULL);
    ae_RippleGrid_Init2(ram, hooks);

    if (area == 19u || area == 36u) {
        ae_wr16(ram, O188_STAGE_AREA_KIND_VARIANT, 27u);
        ae_wr16(ram, O188_STAGE_AREA_KIND_ANIMATION, 38u);
        ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_POINTER, O188_STAGE_SCRIPT_AREA_19_36);
    } else if (area == 45u || area == 63u || area == 69u || area == 87u) {
        if (ae_rds16(ram, mode_state + O188_MODE_VALUE) == 18) {
            ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_POINTER, O188_STAGE_SCRIPT_MODE_18);
        } else {
            const int32_t level = o188_get_area_level(ram, area);
            const int32_t progress = o188_call(ram, hooks,
                "LevelProgressGet", 0x80096580u, 1u,
                (const uint32_t[]){ (uint32_t)level });
            if (progress > 0) {
                ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_POINTER, O188_STAGE_SCRIPT_MODE_18);
            } else {
                ae_wr16(ram, O188_STAGE_AREA_KIND_ANIMATION, 39u);
                ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_POINTER, O188_STAGE_SCRIPT_LOCKED_PROGRESS);
            }
        }
    } else {
        ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_POINTER, O188_STAGE_SCRIPT_MODE_18);
    }

    (void)o188_call(ram, hooks, "ArcLoadEntry", 0x80011194u, 1u,
                    (const uint32_t[]){ O188_STAGE_AREA_KIND_VARIANT });
    ae_wr32(ram, O188_FRAME_COUNTER + O188_FRAME_TIMER_OFFSET, 0u);
    ae_wr16(ram, ui + O188_MODE_UI_TEXT_CLUT, 27u);
    ae_wr16(ram, ui + O188_MODE_UI_TEXT_TPAGE, 38u);
    ae_wr32(ram, ui + O188_MODE_UI_TIMER, 16u);
    ae_wr32(ram, ui + O188_MODE_UI_SCRIPT_FLAGS, 0u);
    ae_wr8(ram, O188_DISPLAY_READY, 1u);
    ae_wr8(ram, O188_FRAME_DRAWENV_ISBG, 1u);
    ae_wr8(ram, O188_DISPLAY_READY_SECONDARY, 0u);
    ae_wr8(ram, O188_FRAME_DRAWENV_BG_RED, 0u);
    ae_wr8(ram, O188_DISPLAY_READY_TERTIARY, 0u);
    ae_wr8(ram, O188_FRAME_DRAWENV_BG_GREEN, 0u);
    ae_wr8(ram, O188_DISPLAY_READY_QUATERNARY, 8u);
    ae_wr8(ram, O188_FRAME_DRAWENV_BG_BLUE, 8u);
    ae_wr32(ram, mode_state + O188_MODE_PROGRESS_NEXT, 0xFFFFFFFFu);
    if (ae_rds16(ram, mode_state + O188_MODE_VALUE) == 18) {
        const int32_t level = o188_get_area_level(ram, area);
        const uint32_t table = (uint32_t)o188_call(ram, hooks,
            "LevelTableGet", 0x80097E60u, 1u,
            (const uint32_t[]){ (uint32_t)level });
        (void)o188_level_info(ram, hooks, level, O188_LEVEL_SCRATCH);
        const int32_t progress = ae_rds32(ram, O188_LEVEL_SCRATCH);
        const int32_t first = ae_rds16(ram, table);
        ae_wr32(ram, mode_state + O188_MODE_PROGRESS_CURRENT,
                (uint32_t)first);
        if (progress != -1) {
            const int32_t second = ae_rds16(ram,
                                            table + O188_LEVEL_TABLE_SECOND_OFFSET);
            const int32_t third = ae_rds16(ram,
                                           table + O188_LEVEL_TABLE_THIRD_OFFSET);
            if (third >= progress) {
                ae_wr32(ram, mode_state + O188_MODE_PROGRESS_CURRENT,
                        0xFFFFFFFFu);
                ae_wr32(ram, mode_state + O188_MODE_PROGRESS_NEXT,
                        (uint32_t)ae_rds16(ram,
                                           table + O188_LEVEL_TABLE_FOURTH_OFFSET));
            } else if (second >= progress) {
                ae_wr32(ram, mode_state + O188_MODE_PROGRESS_CURRENT,
                        (uint32_t)third);
            } else if (first >= progress) {
                ae_wr32(ram, mode_state + O188_MODE_PROGRESS_CURRENT,
                        (uint32_t)second);
            }
        }
    } else {
        ae_wr32(ram, mode_state + O188_MODE_PROGRESS_CURRENT, 0xFFFFFFFFu);
    }

    for (;;) {
        o188_pad_update(ram, hooks);
        ae_wr16(ram, O188_UI_TEXTURE_SEED,
                (uint16_t)(ae_rd16(ram, O188_UI_TEXTURE_SEED) * 5u + 1u));
        ae_wr16(ram, O188_UI_TEXTURE_SEED_2,
                (uint16_t)(ae_rd16(ram, O188_UI_TEXTURE_SEED_2) * 5u + 1u));
        (void)o188_call(ram, hooks, "BeginFrame", 0x8005DF48u, 0u, NULL);

        const uint32_t timer = ae_rd32(ram, O188_FRAME_COUNTER + O188_FRAME_TIMER_OFFSET);
        ae_wr32(ram, ui + O188_MODE_UI_ENV_POINTER, o188_env_offset(timer, O188_FRAME_ALTERNATE_ENV));
        ae_wr32(ram, O188_FRAME_COUNTER,
                ae_rd32(ram, O188_FRAME_COUNTER) + 1u);
        ae_ColorFadeReset(ram);
        o188_player_set_eye_frame(ram, hooks, 0);

        if (ae_StageStartUi_DrawWipeB(ram, run_hooks) < 0) {
            (void)o188_call(ram, hooks, "DrawSync", 0x800286CCu, 1u,
                            (const uint32_t[]){ 0u });
            (void)o188_call(ram, hooks, "VSync", 0x8002E520u, 1u,
                            (const uint32_t[]){ 0u });
            return 0;
        }

        (void)o188_call(ram, hooks, "DrawSync", 0x800286CCu, 1u,
                        (const uint32_t[]){ 0u });
        (void)o188_call(ram, hooks, "VSync", 0x8002E520u, 1u,
                        (const uint32_t[]){ 2u });
        const uint32_t wipe_kind = ae_rd32(ram, O188_MODE_WIPE_KIND);
        if (wipe_kind == 1u || wipe_kind == 3u) {
            const int32_t phase = ae_rds16(ram,
                ui + O188_MODE_UI_ANIMATION_FRAME);
            if (phase == 0) {
                if (wipe_kind == 1u) ae_StageStartUi_SetDrawStp(ram, hooks);
                const uint32_t env_timer = ae_rd32(ram,
                    O188_FRAME_COUNTER + O188_FRAME_TIMER_OFFSET);
                (void)o188_call(ram, hooks, "PutDispEnv", 0x80028EFCu,
                    1u, (const uint32_t[]){ o188_env_offset(env_timer,
                                                              O188_FRAME_DISPLAY_ENV) });
                (void)o188_call(ram, hooks, "PutDrawEnv", 0x80028D30u,
                    1u, (const uint32_t[]){ o188_env_offset(env_timer,
                                                              O188_FRAME_DRAW_ENV) });
                if (wipe_kind == 3u) {
                    ae_wr8(ram, O188_DISPLAY_READY, 0u);
                    ae_wr8(ram, O188_FRAME_DRAWENV_ISBG, 0u);
                }
                ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_FRAME, 1u);
                ae_wr16(ram, ui + O188_MODE_UI_ANIMATION_FRAME_LIMIT,
                        (uint16_t)env_timer);
            } else if (phase == 1 || phase == 2) {
                int32_t display_phase = ae_rds16(ram,
                    ui + O188_MODE_UI_ANIMATION_FRAME_LIMIT);
                if (wipe_kind == 1u && phase == 1) {
                    ae_wr8(ram, O188_DISPLAY_READY, 0u);
                    ae_wr8(ram, O188_FRAME_DRAWENV_ISBG, 0u);
                }
                (void)o188_call(ram, hooks, "PutDispEnv", 0x80028EFCu,
                    1u, (const uint32_t[]){ o188_env_offset(
                        (uint32_t)display_phase, O188_FRAME_DISPLAY_ENV) });
                display_phase = (int16_t)(display_phase ^ 1);
                (void)o188_call(ram, hooks, "PutDrawEnv", 0x80028D30u,
                    1u, (const uint32_t[]){ o188_env_offset(
                        (uint32_t)display_phase, O188_FRAME_DRAW_ENV) });
                if (wipe_kind == 1u && phase == 2) {
                    ae_wr8(ram, O188_DISPLAY_READY, 1u);
                    ae_wr8(ram, O188_FRAME_DRAWENV_ISBG, 1u);
                    ae_wr32(ram, O188_MODE_WIPE_KIND, 2u);
                }
            }
        } else {
            o188_restore_display(ram, hooks, O188_FRAME_DISPLAY_ENV, O188_FRAME_DRAW_ENV);
        }

        const uint32_t draw_timer = ae_rd32(ram,
            O188_FRAME_COUNTER + O188_FRAME_TIMER_OFFSET);
        const uint32_t first_otag = o188_env_offset(draw_timer,
            O188_FRAME_COUNTER) + O188_FRAME_DRAW_ENV_OFFSET;
        const uint32_t second_otag = o188_env_offset(draw_timer,
            O188_FRAME_COUNTER) + O188_FRAME_DRAW_ENV_SECOND_OFFSET;
        (void)o188_call(ram, hooks, "DrawOTag", 0x80028CC0u, 1u,
                        (const uint32_t[]){ first_otag });
        (void)o188_call(ram, hooks, "DrawOTag", 0x80028CC0u, 1u,
                        (const uint32_t[]){ second_otag });
        (void)o188_call(ram, hooks, "CdMgrUpdate", 0x800125B8u, 0u, NULL);
    }
}

