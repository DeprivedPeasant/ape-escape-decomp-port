#include "ae_ovl189.h"

#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "../math/native_sdk_apply.h"

#include <stdint.h>

#define OVL189_CALL(h, r, n, a, c, p) ovl189_call((h), (r), (n), (a), (c), (p))
#define OVL189_RENDER(h, r, n, c, p) ovl189_render((h), (r), (n), (c), (p))

#define OVL189_LEVEL_PROGRESS_ADDR       0x80096580u
#define OVL189_AREA_GET_LEVEL_ADDR       0x80010CC0u
#define OVL189_LEVEL_INFO_ADDR           0x80097D0Cu
#define OVL189_LEVEL_CAUGHT_COUNT_ADDR   0x80097FB0u
#define OVL189_STAGE_RESULT_ADDR         0x800964B0u
#define OVL189_SCRATCH_LEVEL_INFO        0x1F800000u
#define OVL189_SCRATCH_LEVEL_COUNT       0x000Eu

#define OVL189_LEVEL_ROUTE_FIELD          0x0001u
#define OVL189_AREA_INFO_PROGRESS_FIELD   0x012Cu
#define OVL189_NAME_WIDTH_TABLE           0x80139BF0u
#define AE_OVL189_DESTINATION_NAME_STRIDE 4u
#define OVL189_VIEW_MATRIX_ROTATION       0x0000u
#define OVL189_VIEW_MATRIX_TRANSLATION    0x0014u
#define OVL189_VIEW_MATRIX_BYTES          0x0020u
#define OVL189_VECTOR_X                   0x0000u
#define OVL189_VECTOR_Y                   0x0004u
#define OVL189_VECTOR_Z                   0x0008u
#define OVL189_VECTOR_COMPONENT_BYTES     0x0004u
#define OVL189_MODEL_POSITION_X           0x0000u
#define OVL189_MODEL_POSITION_Y           0x0002u
#define OVL189_MODEL_POSITION_Z           0x0004u
#define OVL189_ANGLE_COMPONENT_BYTES      0x0002u

#define OVL189_PAD_UP                     0x00001000u
#define OVL189_PAD_DOWN                   0x00004000u
#define OVL189_PAD_LEFT                   0x00008004u
#define OVL189_PAD_RIGHT                  0x00002008u
#define OVL189_PAD_OPEN_ROUTES            0x00000800u
#define OVL189_PAD_CANCEL_A               0x00000080u
#define OVL189_PAD_CANCEL_B               0x00000010u
#define OVL189_PAD_ACCEPT_A               0x00000020u
#define OVL189_PAD_ACCEPT_B               0x00000040u
#define OVL189_PAD_NEXT_ROUTE             0x00002000u
#define OVL189_PAD_PREVIOUS_ROUTE         0x00008000u
#define OVL189_PAD_SELECT_MODE            0x00000800u
#define OVL189_PAD_ROUTE_LIST             0x800E5488u
#define OVL189_DESTINATION_ROUTE_COUNT    0x0000u
#define OVL189_ROUTE_PROGRESS_STRIDE      4u
#define OVL189_ROUTE_INDEX_STRIDE         2u
#define OVL189_DESTINATION_LEVEL_ID       0x012Cu
#define OVL189_STAGE_CONTEXT_INITIAL_ROUTE       0x02CCu
#define OVL189_STAGE_CONTEXT_INITIAL_SLOT        0x02CDu
#define OVL189_STAGE_CONTEXT_SKIP_INTRO          0x02CEu
#define OVL189_STAGE_CONTEXT_MODE                0x0120u
#define OVL189_STAGE_CONTEXT_LEVEL_TIME          0x0144u
#define OVL189_AREA_RECORD_STRIDE                4u
#define OVL189_AREA_RECORD_KIND                  2u
#define OVL189_AREA_RECORD_MODEL_INDEX           0u
#define OVL189_AREA_INSTANCE_STRIDE              32u
#define OVL189_AREA_KIND_MESH                    2u
#define OVL189_AREA_KIND_STATIC_MODEL            1u
#define OVL189_AREA_KIND_VRAM_ANIM               14u
#define OVL189_MODE_MODEL_STRIDE                 0x4078u
#define OVL189_MODE_MODEL_PARTS                  0x80u
#define OVL189_MODEL_GROUP_FIRST_PART             0x120u
#define OVL189_LEVEL_INFO_SCRATCH                 0x1F800000u
#define OVL189_LEVEL_TIME_X                       44
#define OVL189_LEVEL_TIME_Y                       196
#define OVL189_RANK_ICON_X                        24
#define OVL189_RANK_ICON_Y                        180
#define OVL189_MODE_NO_RESULT                    0xFFFFFFFFu
#define OVL189_MODE_STAGE_SKIP_BIT                0x00002000u
#define OVL189_BADGE_PACKET_BYTES                 40u
#define OVL189_STAGE_PACKET_BYTES                 24u
#define OVL189_STATUS_ICON_PACKET_BYTES           48u
#define OVL189_MENU_PANEL_PACKET_COUNT            92u
#define OVL189_MENU_PANEL_PACKET_STRIDE           20u
#define OVL189_MENU_PANEL_LINK_BYTES               8u
#define OVL189_MENU_PANEL_ANIMATION_MULTIPLIER     5u
#define OVL189_G_GEOMETRY_STATE                  0x800EBFA0u
#define OVL189_GEOMETRY_STATE_TRANSLATION_X      0x001Cu
#define OVL189_GEOMETRY_STATE_TRANSLATION_Y      0x0028u
#define OVL189_GEOMETRY_STATE_TRANSLATION_Z      0x002Cu
#define OVL189_GEOMETRY_STATE_SCALE              0x0024u
#define OVL189_G_MODE_SWITCH                     0x800F4470u
#define OVL189_G_RENDER_CONTEXT_INDEX            0x800EC35Cu
#define OVL189_G_DISPLAY_ENVIRONMENTS            0x800EC3BCu
#define OVL189_DISPLAY_ENV_STRIDE                 0x4078u
#define OVL189_G_DRAW_ENVIRONMENTS               0x800EC360u
#define OVL189_INIT_DISPLAY_ADDR                 0x8005CFECu
#define OVL189_RESET_GRAPH_ADDR                  0x80028350u
#define OVL189_CLEAR_IMAGE_ADDR                  0x80028850u
#define OVL189_DRAW_SYNC_ADDR                    0x800286CCu
#define OVL189_G_DISPLAY_CONFIG                  0x801389D0u
#define OVL189_DISPLAY_CONFIG_BYTES              8u
#define OVL189_DISPLAY_CONFIG_RECT_BYTES         8u
#define OVL189_GRAPHICS_MODE_FLAG                0x801003F0u
#define OVL189_GRAPHICS_CONTEXT_BYTE             0x800FC378u
#define OVL189_RENDER_CONTEXT_INDEX              AE_OVL189_G_RENDER_CONTEXT_INDEX
#define OVL189_RENDER_TABLE_STRIDE               16504u
#define OVL189_ORDER_TABLE_TAG_OFFSET            0x3FFCu
#define OVL189_MODEL_PARTS_OFFSET               0x80u
#define OVL189_MODEL_GROUP_OFFSET               0x120u
#define OVL189_MODEL_INSTANCE_BYTES             0x20u
#define OVL189_MODEL_INSTANCE_MESH               0x0000u
#define OVL189_MODEL_INSTANCE_VERTICES            0x0004u
#define OVL189_MODEL_INSTANCE_PRIMITIVES          0x0008u
#define OVL189_MODEL_MESH_VERTEX_COUNT            0x000Eu
#define OVL189_MODEL_MESH_ENV_VERTEX_COUNT        0x0000u
#define OVL189_MODEL_MESH_VERTEX_OFFSET           0x0006u
#define OVL189_MODEL_MESH_VERTEX_BYTES            8u
#define OVL189_MODEL_VERTEX_OUTPUT                0x1F800000u
#define OVL189_MODEL_MATRIX_TRANSLATION_X         20u
#define OVL189_MODEL_MATRIX_TRANSLATION_Y         24u
#define OVL189_MODEL_MATRIX_TRANSLATION_Z         28u
#define OVL189_DESTINATION_NAME_STRIDE          4u
#define OVL189_G_DRAW_ORDER_TABLES               0x800EC3D0u
#define OVL189_G_LEVEL_INFO_TIME                 0x800F4494u
#define OVL189_LEVEL_INFO_TIME_FIELD             0x0000u
#define OVL189_G_LEVEL_TIME_BUFFER               0x800EC354u
#define OVL189_G_STAGE_TIME                      0x800EC350u
#define OVL189_G_UI_ANIMATION_COUNTER            0x800AE8F0u
#define OVL189_G_TEXT_RANDOM_SEED                  0x800AE8F2u
#define OVL189_MODE_EXIT_CODE             0x00000800
#define OVL189_DEBUG_ARGUMENT_FRAME       0x801FFD00u
#define OVL189_DEBUG_FORMAT_HEADER        0x80138A08u
#define OVL189_DEBUG_FORMAT_AREA_NAME     0x80138A18u
#define OVL189_RENDER_MAIN_MENU            "HubExitUi_RenderMainMenu"
#define OVL189_RENDER_ROUTE_MENU           "HubExitUi_RenderRouteMenu"

static uint32_t ovl189_render_context_address(AeRam *ram, uint32_t offset);
static int32_t ovl189_sra32(int32_t value, uint32_t shift);

static int32_t ovl189_call(const AeOvl189Hooks *hooks, AeRam *ram,
                           const char *name, uint32_t address,
                           uint32_t count, const uint32_t *args)
{
    if (!hooks || !hooks->call) {
        ae_ram_fail(ram, "AeOvl189Hooks.call missing for %s", name);
        return 0;
    }
    return hooks->call(hooks->user, ram, name, address, count, args);
}

static int32_t ovl189_render(const AeOvl189Hooks *hooks, AeRam *ram,
                             const char *name, uint32_t count,
                             const uint32_t *args)
{
    if (!hooks || !hooks->render) {
        ae_ram_fail(ram, "AeOvl189Hooks.render missing for %s", name);
        return 0;
    }
    return hooks->render(hooks->user, ram, name, count, args);
}

static uint32_t destination_route_address(int32_t destination, int32_t route)
{
    return AE_OVL189_DESTINATION_TABLE +
           (uint32_t)destination * AE_OVL189_EXIT_ROUTE_STRIDE +
           (uint32_t)route * AE_OVL189_EXIT_ROUTE_SIZE;
}

static uint32_t destination_area_info(int32_t destination)
{
    return AE_OVL189_DESTINATION_AREA_INFO +
           (uint32_t)destination * AE_OVL189_AREA_INFO_STRIDE;
}

static int32_t destination_level_area(AeRam *ram, int32_t destination,
                                      int32_t route)
{
    uint32_t route_record = destination_route_address(destination, route);
    uint32_t route_area = ae_rd8(ram, route_record + OVL189_LEVEL_ROUTE_FIELD);
    uint32_t area_record = destination_area_info(destination) +
                           route_area * AE_OVL189_EXIT_ROUTE_SIZE +
                           OVL189_AREA_INFO_PROGRESS_FIELD;
    return ae_rds16(ram, area_record);
}

static void debug_text(AeRam *ram, const AeOvl189Hooks *hooks,
                       uint32_t format, uint32_t arguments)
{
    ae_DebugTextPrintf(ram, hooks ? hooks->debug_text : NULL, format, arguments);
}

static void debug_text_with_string(AeRam *ram, const AeOvl189Hooks *hooks,
                                   uint32_t format, uint32_t string)
{
    ae_wr32(ram, OVL189_DEBUG_ARGUMENT_FRAME + 4u, string);
    debug_text(ram, hooks, format, OVL189_DEBUG_ARGUMENT_FRAME);
}

static int32_t menu_route_count(AeRam *ram, int32_t destination)
{
    return (int32_t)ae_rd8(ram, AE_OVL189_DESTINATION_TABLE +
                                (uint32_t)destination * AE_OVL189_EXIT_ROUTE_STRIDE);
}

int32_t ae_HubExitUi_Init(AeRam *ram, const AeOvl189Hooks *hooks,
                          uint32_t display_argument)
{
    const uint32_t reset_args[1] = {1u};
    const uint32_t sync_args[1] = {0u};
    uint32_t rectangle = AE_OVL189_TEMP_INIT_CLEAR_RECT;
    const uint32_t clear_args[4] = {rectangle, 0u, 0u, 0u};

    ae_wr32(ram, rectangle, ae_rd32(ram, OVL189_G_DISPLAY_CONFIG));
    ae_wr32(ram, rectangle + 4u,
            ae_rd32(ram, OVL189_G_DISPLAY_CONFIG + OVL189_DISPLAY_CONFIG_BYTES / 2u));
    (void)OVL189_CALL(hooks, ram, "InitDisplay", OVL189_INIT_DISPLAY_ADDR,
                      1u, &display_argument);
    (void)OVL189_CALL(hooks, ram, "ResetGraph", OVL189_RESET_GRAPH_ADDR,
                      1u, reset_args);
    (void)OVL189_CALL(hooks, ram, "ClearImage", OVL189_CLEAR_IMAGE_ADDR,
                      4u, clear_args);
    (void)OVL189_CALL(hooks, ram, "DrawSync", OVL189_DRAW_SYNC_ADDR,
                      1u, sync_args);
    return 0;
}

static uint32_t ovl189_build_model_matrix(AeRam *ram, uint32_t position,
                                          uint32_t rotation,
                                          uint8_t geometry_flag)
{
    uint32_t matrix = AE_OVL189_TEMP_MODEL_MATRIX;

    ae_wr8(ram, AE_OVL189_GRAPHICS_FLAG, geometry_flag);
    ae_wr16(ram, AE_OVL189_GRAPHICS_STATE + 2u, 0u);
    (void)ae_sdk_MatrixIdentity(ram, matrix);
    ae_wr16(ram, AE_OVL189_GRAPHICS_STATE, 0u);
    int32_t x = ae_rds16(ram, position + OVL189_MODEL_POSITION_X);
    int32_t y = ae_rds16(ram, position + OVL189_MODEL_POSITION_Y);
    int32_t z = ae_rds16(ram, position + OVL189_MODEL_POSITION_Z);
    (void)ae_sdk_RotMatrix(ram, rotation, matrix);
    ae_wr32(ram, matrix + OVL189_MODEL_MATRIX_TRANSLATION_X, (uint32_t)x);
    ae_wr32(ram, matrix + OVL189_MODEL_MATRIX_TRANSLATION_Y, (uint32_t)y);
    ae_wr32(ram, matrix + OVL189_MODEL_MATRIX_TRANSLATION_Z, (uint32_t)z);
    (void)ae_sdk_CompMatrix(ram, AE_OVL189_G_VIEW_MATRIX, matrix, matrix);
    return matrix;
}

static void ovl189_project_model_vertices(AeRam *ram,
                                         const AeOvl189Hooks *hooks,
                                         uint32_t model, uint32_t matrix,
                                         uint32_t model_flag)
{
    uint32_t mesh = ae_rd32(ram, model + OVL189_MODEL_INSTANCE_MESH);
    uint32_t vertex_count = ae_rd16(ram, mesh + OVL189_MODEL_MESH_VERTEX_COUNT);
    if (vertex_count == 0u) return;

    const uint32_t args[6] = {
        model,
        matrix,
        ae_rd32(ram, model + OVL189_MODEL_INSTANCE_VERTICES),
        vertex_count,
        model_flag,
        OVL189_MODEL_VERTEX_OUTPUT
    };
    (void)OVL189_RENDER(hooks, ram, "HubExitUi_ProjectModelVertices", 6u, args);
}

static void ovl189_emit_model_primitives(AeRam *ram,
                                         const AeOvl189Hooks *hooks,
                                         uint32_t model)
{
    const uint32_t args[3] = {
        ovl189_render_context_address(ram, 0u),
        ae_rd32(ram, model + OVL189_MODEL_INSTANCE_PRIMITIVES),
        AE_OVL189_G_COLOR_MATRIX
    };
    (void)OVL189_CALL(hooks, ram, "ModelEmitPrims", 0x800180B8u, 3u, args);
}

void ae_HubExitUi_DrawModelA(AeRam *ram, const AeOvl189Hooks *hooks,
                             uint32_t position, uint32_t rotation,
                             uint32_t model, uint32_t model_flag)
{
    uint32_t matrix = ovl189_build_model_matrix(ram, position, rotation, 0u);
    ae_MeshTransform(ram, hooks ? hooks->effects : NULL, model, matrix,
                     AE_OVL189_G_COLOR_MATRIX);
    ovl189_project_model_vertices(ram, hooks, model, matrix, model_flag);
    ovl189_emit_model_primitives(ram, hooks, model);
}

void ae_HubExitUi_DrawModelB(AeRam *ram, const AeOvl189Hooks *hooks,
                             uint32_t position, uint32_t rotation,
                             uint32_t model, uint32_t model_flag)
{
    uint32_t matrix = ovl189_build_model_matrix(ram, position, rotation, 1u);
    ae_MeshTransform(ram, hooks ? hooks->effects : NULL, model, matrix,
                     AE_OVL189_G_COLOR_MATRIX);
    ovl189_project_model_vertices(ram, hooks, model, matrix, model_flag);
    ovl189_emit_model_primitives(ram, hooks, model);
}

void ae_HubExitUi_DrawModelC(AeRam *ram, const AeOvl189Hooks *hooks,
                             uint32_t position, uint32_t rotation,
                             uint32_t model)
{
    uint32_t matrix = ovl189_build_model_matrix(ram, position, rotation, 1u);
    const uint32_t transform_args[3] = {
        model, matrix, AE_OVL189_G_COLOR_MATRIX
    };
    (void)OVL189_CALL(hooks, ram, "ModelTransformRigid", 0x800161C0u,
                      3u, transform_args);
    ae_MeshCalcEnvUVs(ram, hooks ? hooks->misc : NULL, model);

    const uint32_t draw_args[3] = {
        ovl189_render_context_address(ram, 0u),
        AE_OVL189_G_COLOR_MATRIX,
        model
    };
    (void)OVL189_CALL(hooks, ram, "ModelDrawEnvMap", 0x8001CE54u,
                      3u, draw_args);
}

int32_t ae_HubExitUi_GetProgress(AeRam *ram, const AeOvl189Hooks *hooks,
                                 int32_t destination, int32_t route)
{
    int32_t level_area = destination_level_area(ram, destination, route);
    int32_t level = ae_AreaGetLevel(ram, hooks ? hooks->area_set : NULL, level_area);
    return ae_LevelProgressGet(ram, (uint32_t)level);
}

int32_t ae_HubExitUi_IsDestinationUnlocked(AeRam *ram,
                                           const AeOvl189Hooks *hooks,
                                           int32_t destination, int32_t route)
{
    int32_t level_area = destination_level_area(ram, destination, route);
    int32_t level = ae_AreaGetLevel(ram, hooks ? hooks->area_set : NULL, level_area);
    if (level == 1) return 1;

    if (destination < (int32_t)AE_OVL189_SPECIAL_DESTINATION_COUNT) {
        int32_t progress_id = ae_rds8(
            ram, AE_OVL189_DESTINATION_PROGRESS_IDS +
                     (uint32_t)destination * OVL189_ROUTE_PROGRESS_STRIDE +
                     (uint32_t)route);
        return ae_LevelProgressGet(ram, (uint32_t)progress_id) > 0;
    }

    uint32_t progress_id = AE_OVL189_SPECIAL_PROGRESS_IDS;
    if (ae_rds8(ram, progress_id) <= 0) return 1;
    do {
        int32_t required_progress = ae_LevelProgressGet(
            ram, (uint32_t)ae_rds8(ram, progress_id));
        if (required_progress < 2) return 0;
        progress_id += 1u;
    } while (ae_rds8(ram, progress_id) > 0);
    return 1;
}

int32_t ae_HubExitUi_GetFlags(AeRam *ram, const AeOvl189Hooks *hooks,
                              int32_t destination, int32_t route)
{
    int32_t level_area = destination_level_area(ram, destination, route);
    int32_t level = ae_AreaGetLevel(ram, hooks ? hooks->area_set : NULL, level_area);
    uint32_t info_args[2] = {(uint32_t)level, OVL189_SCRATCH_LEVEL_INFO};
    (void)OVL189_CALL(hooks, ram, "LevelGetInfo", OVL189_LEVEL_INFO_ADDR, 2u, info_args);
    if (ae_ram_failed(ram)) return 0;

    if (level == 14 || level == 7) {
        uint32_t result_id = level == 14 ? 36u : 19u;
        uint32_t result_args[1] = {result_id};
        return OVL189_CALL(hooks, ram, "ProgressGetStageResult",
                           OVL189_STAGE_RESULT_ADDR, 1u, result_args) & 0x10;
    }
    if (level == 30) return 0;

    uint32_t caught_args[1] = {(uint32_t)level};
    int32_t caught = OVL189_CALL(hooks, ram, "LevelCaughtCount",
                                 OVL189_LEVEL_CAUGHT_COUNT_ADDR, 1u, caught_args);
    int32_t required = ae_rds16(ram, OVL189_SCRATCH_LEVEL_INFO +
                                    OVL189_SCRATCH_LEVEL_COUNT);
    return caught == required;
}

static int32_t screen_menu_update(AeRam *ram, const AeOvl189Hooks *hooks,
                                  uint32_t pressed)
{
    int32_t selection = ae_rds32(ram, AE_OVL189_G_SELECTION);

    if ((pressed & OVL189_PAD_OPEN_ROUTES) != 0u) {
        (void)ae_PlaySfx(ram, hooks ? hooks->sound_hw : NULL, 2);
        if (selection < (int32_t)AE_OVL189_DESTINATION_COUNT) {
            ae_wr32(ram, AE_OVL189_G_SCREEN_STATE, 3u);
            ae_wr32(ram, AE_OVL189_G_ROUTE_SELECTION, 0u);
            ae_wr32(ram, AE_OVL189_G_ROUTE_SCAN_INDEX, 0u);
            uint32_t area_ids = destination_area_info(selection) +
                                AE_OVL189_AREA_INFO_LEVEL_ID;
            int32_t route_index = 0;
            while (ae_rds16(ram, area_ids +
                                  (uint32_t)route_index * OVL189_ROUTE_INDEX_STRIDE) != -2) {
                ++route_index;
            }
            ae_wr32(ram, AE_OVL189_G_ROUTE_SCAN_INDEX,
                    (uint32_t)(route_index - 1));
        }
        return 0;
    }

    if (selection >= (int32_t)AE_OVL189_DESTINATION_COUNT) return 0;

    if ((pressed & OVL189_PAD_DOWN) != 0u) {
        selection += 3;
        if (selection >= (int32_t)AE_OVL189_DESTINATION_COUNT) selection %= 3;
        ae_wr8(ram, AE_OVL189_G_TEXT_ROW, 8u);
        (void)ae_PlaySfx(ram, hooks ? hooks->sound_hw : NULL, 0);
    } else if ((pressed & OVL189_PAD_UP) != 0u) {
        selection -= 3;
        if (selection < 0) {
            int32_t column = selection + 3;
            int32_t remainder = column % 3;
            selection = remainder + 9;
            if (selection >= (int32_t)AE_OVL189_DESTINATION_COUNT) selection -= 3;
        }
        ae_wr8(ram, AE_OVL189_G_TEXT_ROW, 8u);
        (void)ae_PlaySfx(ram, hooks ? hooks->sound_hw : NULL, 0);
    } else if ((pressed & OVL189_PAD_LEFT) != 0u) {
        int32_t row = selection / 3;
        int32_t column = (selection + 2) % 3;
        selection = row * 3 + column;
        if (selection >= (int32_t)AE_OVL189_DESTINATION_COUNT)
            selection = (int32_t)AE_OVL189_DESTINATION_COUNT - 1;
        ae_wr8(ram, AE_OVL189_G_TEXT_ROW, 8u);
        (void)ae_PlaySfx(ram, hooks ? hooks->sound_hw : NULL, 0);
    } else if ((pressed & OVL189_PAD_RIGHT) != 0u) {
        int32_t row = selection / 3;
        int32_t column = (selection + 4) % 3;
        selection = row * 3 + column;
        if (selection >= (int32_t)AE_OVL189_DESTINATION_COUNT)
            selection = (int32_t)AE_OVL189_DESTINATION_COUNT - 1;
        ae_wr8(ram, AE_OVL189_G_TEXT_ROW, 8u);
        (void)ae_PlaySfx(ram, hooks ? hooks->sound_hw : NULL, 0);
    }

    ae_wr32(ram, AE_OVL189_G_SELECTION, (uint32_t)selection);
    return 0;
}

static int32_t screen_route_update(AeRam *ram, const AeOvl189Hooks *hooks,
                                   uint32_t button)
{
    int32_t destination = ae_rds32(ram, AE_OVL189_G_SELECTION);
    int32_t route_slot = ae_rds32(ram, AE_OVL189_G_DESTINATION_SLOT);

    if (button == OVL189_PAD_CANCEL_A ||
        button == OVL189_PAD_CANCEL_B) {
        (void)ae_SoundStop(ram, hooks ? hooks->stage_fade : NULL, 1);
        (void)ae_PlaySfx(ram, hooks ? hooks->sound_hw : NULL, 1);
        return -7;
    }

    if (button == 4u) {
        if (destination != 0) {
            ae_wr32(ram, AE_OVL189_G_CAMERA_DISTANCE, 0u);
            ae_wr32(ram, AE_OVL189_G_TRANSITION_STATE, 1u);
            ae_wr32(ram, AE_OVL189_G_ANIMATION_FRAME, 0u);
            (void)ae_PlaySfx(ram, hooks ? hooks->sound_hw : NULL, 0);
        }
        return 0;
    }

    if (button == 8u) {
        if (destination < 9 &&
            ae_HubExitUi_IsDestinationUnlocked(ram, hooks, destination + 1, 0)) {
            ae_wr32(ram, AE_OVL189_G_TRANSITION_STATE, 0xFFFFFFFFu);
            ae_wr32(ram, AE_OVL189_G_CAMERA_DISTANCE, 0u);
            ae_wr32(ram, AE_OVL189_G_ANIMATION_FRAME, 0u);
            (void)ae_PlaySfx(ram, hooks ? hooks->sound_hw : NULL, 0);
        }
        return 0;
    }

    if (button == OVL189_PAD_NEXT_ROUTE) {
        int32_t route_count = menu_route_count(ram, destination);
        int32_t next_slot = route_slot + 1;
        if (next_slot < route_count &&
            ae_HubExitUi_IsDestinationUnlocked(ram, hooks, destination,
                                               next_slot)) {
            ae_wr32(ram, AE_OVL189_G_DESTINATION_SLOT, (uint32_t)next_slot);
            ae_wr32(ram, AE_OVL189_G_ANIMATION_FRAME, 0u);
            (void)ae_PlaySfx(ram, hooks ? hooks->sound_hw : NULL, 0);
        } else if (destination < 9 &&
                   ae_HubExitUi_IsDestinationUnlocked(ram, hooks,
                                                      destination + 1, 0)) {
            ae_wr32(ram, AE_OVL189_G_TRANSITION_STATE, 0xFFFFFFFFu);
            ae_wr32(ram, AE_OVL189_G_CAMERA_DISTANCE, 0u);
            ae_wr32(ram, AE_OVL189_G_ANIMATION_FRAME, 0u);
            (void)ae_PlaySfx(ram, hooks ? hooks->sound_hw : NULL, 0);
        }
        return 0;
    }

    if (button == OVL189_PAD_PREVIOUS_ROUTE) {
        if (route_slot > 0) {
            ae_wr32(ram, AE_OVL189_G_DESTINATION_SLOT,
                    (uint32_t)(route_slot - 1));
            ae_wr32(ram, AE_OVL189_G_ANIMATION_FRAME, 0u);
            (void)ae_PlaySfx(ram, hooks ? hooks->sound_hw : NULL, 0);
        } else if (destination != 0) {
            ae_wr32(ram, AE_OVL189_G_CAMERA_DISTANCE, 0u);
            ae_wr32(ram, AE_OVL189_G_TRANSITION_STATE, 1u);
            ae_wr32(ram, AE_OVL189_G_ANIMATION_FRAME, 0u);
            (void)ae_PlaySfx(ram, hooks ? hooks->sound_hw : NULL, 0);
        }
        return 0;
    }

    if (button == OVL189_PAD_ACCEPT_A || button == OVL189_PAD_ACCEPT_B ||
        button == OVL189_PAD_SELECT_MODE) {
        uint32_t route_record = destination_route_address(destination, route_slot);
        uint32_t route_area = ae_rd8(ram, route_record + OVL189_LEVEL_ROUTE_FIELD);
        int32_t area = ae_rds16(
            ram, destination_area_info(destination) +
                     route_area * OVL189_ROUTE_INDEX_STRIDE +
                     OVL189_AREA_INFO_PROGRESS_FIELD);
        ae_wr32(ram, AE_OVL189_G_ROUTE_SELECTION, route_area);
        ae_wr16(ram, AE_OVL189_G_CURRENT_AREA, (uint32_t)area);
        int32_t result = ae_HubDestinationState(
            ram, hooks ? hooks->progress : NULL, area, (int32_t)button);
        if (ae_ram_failed(ram)) return 0;

        if (button == OVL189_PAD_SELECT_MODE) {
            if (result != AE_PROGRESS_HUB_DESTINATION_READY) return 0;
            if (ae_rd16(ram, AE_OVL189_G_CURRENT_AREA) == 87u) return 0;
        }

        (void)ae_SoundStop(ram, hooks ? hooks->stage_fade : NULL, 1);
        (void)ae_PlaySfx(ram, hooks ? hooks->sound_hw : NULL, 3);
        ae_wr8(ram, AE_OVL189_G_SAVE_DESTINATION_SOURCE,
               (uint32_t)destination);
        ae_wr8(ram, AE_OVL189_G_SAVE_DESTINATION_ROUTE,
               (uint32_t)route_area);
        if (result == 0) return -3;
        if (result == 2) return -6;
        if (result == AE_PROGRESS_HUB_DESTINATION_READY) return -8;
        return -4;
    }

    return 0;
}

int32_t ae_HubExitUi_Run(AeRam *ram, const AeOvl189Hooks *hooks)
{
    int32_t screen_state = ae_rds32(ram, AE_OVL189_G_SCREEN_STATE);
    if (screen_state == 1) {
        int32_t loading = OVL189_CALL(hooks, ram, "LoadListStep",
                                      0x800116C0u, 0u, NULL);
        return loading >= 0 ? 0 : -2;
    }

    if (screen_state == 0) {
        uint32_t pressed = ae_rd32(ram, AE_OVL189_G_PAD_HELD) &
                           ae_rd32(ram, AE_OVL189_G_PAD_CHANGED);
        if ((pressed & 3u) == 3u) {
            (void)ae_SoundStop(ram, hooks ? hooks->stage_fade : NULL, 0);
            return -1;
        }

        (void)screen_menu_update(ram, hooks, pressed);
        if (ae_ram_failed(ram)) return 0;

        ae_ColorFadeSet(ram, 130, 10);
        debug_text(ram, hooks, OVL189_DEBUG_FORMAT_HEADER,
                   OVL189_DEBUG_ARGUMENT_FRAME);
        for (int32_t row = 0; row < (int32_t)AE_OVL189_DESTINATION_COUNT; ++row) {
            uint32_t selected = ae_rd32(ram, AE_OVL189_G_SELECTION);
            int32_t row_fade = row == (int32_t)selected
                                   ? 5 - (int32_t)ae_rd8(ram, AE_OVL189_G_TEXT_ROW)
                                   : 0;
            int32_t x = 130 + 80 * (row % 3);
            int32_t x_adjust = (row_fade + (row_fade < 0 ? 1 : 0)) >> 1;
            int32_t y = 28 + 15 * (row / 3);
            ae_ColorFadeSet(ram, x + x_adjust, y);
            debug_text_with_string(ram, hooks, OVL189_DEBUG_FORMAT_AREA_NAME,
                                   destination_area_info(row));
            if (ae_ram_failed(ram)) return 0;
        }

        const uint32_t render_args[8] = {
            ae_rd32(ram, AE_OVL189_G_SELECTION),
            ae_rd8(ram, AE_OVL189_G_TEXT_ROW),
            ae_rd32(ram, AE_OVL189_G_ROUTE_SELECTION),
            ae_rd32(ram, AE_OVL189_G_DESTINATION_SLOT),
            ae_rd32(ram, AE_OVL189_G_CAMERA_ANGLE),
            ae_rd32(ram, AE_OVL189_G_CAMERA_DISTANCE),
            ae_rd32(ram, AE_OVL189_G_ANIMATION_FRAME),
            AE_OVL189_DESTINATION_AREA_INFO
        };
        (void)OVL189_RENDER(hooks, ram, "HubExitUi_RenderRows", 8u, render_args);
        if (ae_ram_failed(ram)) return 0;
        ae_wr8(ram, AE_OVL189_G_TEXT_ROW,
               ae_rd8(ram, AE_OVL189_G_TEXT_ROW) >> 1);
        return 0;
    }

    if (screen_state != 3) return 0;

    int32_t selection = ae_rds32(ram, AE_OVL189_G_SELECTION);
    if (selection < 10) {
        uint32_t title = ae_SaveUi_StringTableIndex_189(
            ram, AE_OVL189_STAGE_NAME_DATA, selection);
        int32_t width = ae_HubExitUi_TextWidth(ram, title);
        int32_t half_width = (width + (width < 0 ? 1 : 0)) >> 1;
        ae_wr16(ram, AE_OVL189_G_TEXT_DRAW_STATE,
                (uint32_t)(192 - half_width));
        ae_wr16(ram, AE_OVL189_G_TEXT_DRAW_STATE + 2u, 43u);
        ae_wr16(ram, AE_OVL189_G_TEXT_DRAW_STATE + 4u, 0x3FA3u);
        uint32_t model_text = ovl189_render_context_address(ram, 0u);
        ae_wr32(ram, AE_OVL189_G_TEXT_DRAW_STATE + 8u, model_text);
        ae_HubExitUi_DrawText(ram, hooks, title);
        if (ae_ram_failed(ram)) return 0;
    }
    if (ae_rd32(ram, AE_OVL189_G_TRANSITION_STATE) != 0u) return 0;

    uint32_t button = ae_rd32(ram, OVL189_PAD_ROUTE_LIST);
    int32_t result = screen_route_update(ram, hooks, button);
    if (ae_ram_failed(ram)) return 0;
    return result;
}
#define OVL189_CALL_HEAP_INIT_ADDR             0x8008C01Cu
#define OVL189_CALL_SET_GEOM_SCREEN_ADDR       0x8002FAF0u
#define OVL189_CALL_SET_COLOR_MATRIX_ADDR      0x80015D58u
#define OVL189_CALL_ARC_LOAD_ENTRY_ADDR        0x80011194u
#define OVL189_CALL_LOAD_LIST_STEP_ADDR        0x800116C0u
#define OVL189_CALL_CD_MGR_UPDATE_ADDR         0x800125B8u
#define OVL189_CALL_VSYNC_ADDR                 0x8002E520u
#define OVL189_CALL_MESH_INIT_ADDR             0x80015C34u
#define OVL189_CALL_SOUND_RESTORE_ADDR         0x80086170u
#define OVL189_CALL_PLAYER_INIT_ADDR           0x8003272Cu
#define OVL189_CALL_BEGIN_FRAME_ADDR           0x8005DF48u
#define OVL189_CALL_ROT_MATRIX_Z_ADDR          0x800306F0u
#define OVL189_CALL_ROT_MATRIX_Y_ADDR          0x80030890u
#define OVL189_CALL_ROT_MATRIX_X_ADDR          0x80030A30u
#define OVL189_CALL_MODEL_EMIT_PRIMS_ADDR      0x8001A414u
#define OVL189_CALL_SET_DRAW_TPAGE_ADDR        0x8002B410u
#define OVL189_CALL_DRAW_SYNC_ADDR             0x800286CCu
#define OVL189_CALL_PUT_DISP_ENV_ADDR          0x80028EFCu
#define OVL189_CALL_PUT_DRAW_ENV_ADDR          0x80028D30u
#define OVL189_CALL_DRAW_OTAG_ADDR             0x80028CC0u
#define OVL189_CALL_LEVEL_GET_INFO_ADDR        0x80097D0Cu
#define OVL189_CALL_LEVEL_TABLE_GET_ADDR       0x80097E60u
#define OVL189_CALL_HUD_DRAW_RANK_ADDR         0x8005F68Cu
#define OVL189_CALL_PROGRESS_GET_BYTE_ADDR     0x80095E4Cu
#define OVL189_CALL_MODEL_TRANSFORM_ADDR       0x800160ECu
#define OVL189_CALL_PAD_UPDATE_FLAGS            2u
#define OVL189_AREA_DATA_KIND_MESH              2u
#define OVL189_AREA_DATA_KIND_STATIC_MODEL      1u
#define OVL189_AREA_DATA_KIND_VRAM_ANIM         14u
#define OVL189_DESTINATION_LABEL_NO_NAME        0xFFFFFFFFu
#define OVL189_DESTINATION_NAME_COPY_BYTES      16u
#define OVL189_DESTINATION_NAME_BUFFER          0x800F4458u
#define OVL189_STAGE_ROUTE_NAME_Y               64u
#define OVL189_STAGE_ROUTE_NAME_TPAGE           0x3F63u
#define OVL189_TRANSITION_STEP                  800
#define OVL189_TRANSITION_DISTANCE              8000
#define OVL189_FRAME_DECORATION_STEP             50
#define OVL189_FRAME_DECORATION_LIMIT           1400
#define OVL189_FRAME_COUNTER_STEP                1u
#define OVL189_STAGE_CONTEXT_INTRO_FLAG         0x02CEu
#define OVL189_GAME_INPUT_ROUTE_SKIP            0x800E5488u

static int32_t ovl189_mode_call(const AeOvl189Hooks *hooks, AeRam *ram,
                                const char *name, uint32_t address,
                                uint32_t count, const uint32_t *args)
{
    return OVL189_CALL(hooks, ram, name, address, count, args);
}

static int32_t ovl189_mode_call0(const AeOvl189Hooks *hooks, AeRam *ram,
                                 const char *name, uint32_t address)
{
    return ovl189_mode_call(hooks, ram, name, address, 0u, NULL);
}

static int32_t ovl189_mode_call1(const AeOvl189Hooks *hooks, AeRam *ram,
                                 const char *name, uint32_t address,
                                 uint32_t a0)
{
    const uint32_t args[1] = {a0};
    return ovl189_mode_call(hooks, ram, name, address, 1u, args);
}

static int32_t ovl189_mode_call2(const AeOvl189Hooks *hooks, AeRam *ram,
                                 const char *name, uint32_t address,
                                 uint32_t a0, uint32_t a1)
{
    const uint32_t args[2] = {a0, a1};
    return ovl189_mode_call(hooks, ram, name, address, 2u, args);
}

static int32_t ovl189_mode_call4(const AeOvl189Hooks *hooks, AeRam *ram,
                                 const char *name, uint32_t address,
                                 uint32_t a0, uint32_t a1,
                                 uint32_t a2, uint32_t a3)
{
    const uint32_t args[4] = {a0, a1, a2, a3};
    return ovl189_mode_call(hooks, ram, name, address, 4u, args);
}

static uint32_t ovl189_render_context_address(AeRam *ram, uint32_t offset)
{
    uint32_t context = ae_rd32(ram, AE_OVL189_G_RENDER_CONTEXT_INDEX);
    return AE_OVL189_G_ORDERING_TABLE_ROOT +
           context * OVL189_RENDER_TABLE_STRIDE + offset;
}

static uint32_t ovl189_destination_model(uint32_t instances, int32_t name_index)
{
    if (name_index < 0) return instances + OVL189_MODEL_PARTS_OFFSET;
    return instances + OVL189_MODEL_GROUP_OFFSET +
           (uint32_t)name_index * OVL189_MODEL_INSTANCE_BYTES;
}

static void ovl189_update_route_camera(AeRam *ram)
{
    int32_t target = -ae_rds32(ram, AE_OVL189_G_DESTINATION_SLOT) * 512;
    int32_t angle = ae_rds32(ram, AE_OVL189_G_CAMERA_ANGLE);
    if (angle < target - 128) angle += 128;
    else if (angle > target + 128) angle -= 128;
    else angle = target;
    ae_wr32(ram, AE_OVL189_G_CAMERA_ANGLE, (uint32_t)angle);
}

static void ovl189_draw_scene_decorations(AeRam *ram,
                                          const AeOvl189Hooks *hooks,
                                          uint32_t instances,
                                          int32_t *rotation_angle,
                                          int32_t *spin_step)
{
    uint32_t position = AE_OVL189_TEMP_ROUTE_POSITION;
    uint32_t rotation = AE_OVL189_TEMP_ROUTE_ROTATION;
    ae_wr16(ram, position, (uint32_t)-750);
    ae_wr16(ram, position + 2u, (uint32_t)-460);
    ae_wr16(ram, position + 4u, 7840u);
    ae_wr16(ram, rotation, 0u);
    ae_wr16(ram, rotation + 2u, (uint32_t)*rotation_angle);
    ae_wr16(ram, rotation + 4u, 0u);
    ae_HubExitUi_DrawModelB(ram, hooks, position, rotation,
                            instances + 0x100u, 4096u);

    *spin_step = ae_StepToward(
        ram, *spin_step,
        ae_rd32(ram, AE_OVL189_G_CAMERA_DISTANCE) != 0u ? 240 : 16, 32);
    *rotation_angle = (int32_t)((uint32_t)*rotation_angle +
                                (uint32_t)*spin_step);
    ae_wr16(ram, rotation, 0u);
    ae_wr16(ram, rotation + 2u, 0u);
    ae_wr16(ram, rotation + 4u, (uint32_t)*rotation_angle);
    ae_HubExitUi_DrawModelB(ram, hooks, position, rotation,
                            instances + 0xE0u, 4096u);

    ae_wr16(ram, rotation, (uint32_t)(*rotation_angle * 2));
    ae_wr16(ram, rotation + 2u, (uint32_t)(*rotation_angle * 3));
    ae_HubExitUi_DrawModelC(ram, hooks, position, rotation,
                            instances + 0x40u);

    ae_wr16(ram, position, 750u);
    ae_wr16(ram, position + 2u, (uint32_t)-560);
    ae_wr16(ram, rotation, 0u);
    ae_wr16(ram, rotation + 2u, (uint32_t)-*rotation_angle);
    ae_wr16(ram, rotation + 4u, 0u);
    ae_HubExitUi_DrawModelB(ram, hooks, position, rotation,
                            instances + 0xA0u, 4096u);

    ae_wr16(ram, position + 2u, (uint32_t)-360);
    ae_HubExitUi_DrawModelB(ram, hooks, position, rotation,
                            instances + 0xC0u, 4096u);

    ae_wr16(ram, position + 2u, (uint32_t)-460);
    ae_wr16(ram, rotation + 2u, (uint32_t)*rotation_angle);
    ae_HubExitUi_DrawModelB(ram, hooks, position, rotation,
                            instances + 0x80u, 4096u);

    (void)ovl189_mode_call1(hooks, ram, "SetGeomScreen",
                            OVL189_CALL_SET_GEOM_SCREEN_ADDR, 164u);

    int32_t destination = ae_rds32(ram, AE_OVL189_G_SELECTION);
    int32_t slot = ae_rds32(ram, AE_OVL189_G_DESTINATION_SLOT);
    int32_t cosine;
    int16_t z;
    if (slot == 0 && destination != 0 && destination != 9) {
        cosine = ae_sdk_rcos(
            ram, (int32_t)ae_rd32(ram, AE_OVL189_G_FRAME_INDEX) * 16);
        z = (int16_t)(2048 + cosine / 16);
        ae_wr16(ram, position, (uint32_t)-1548);
        ae_wr16(ram, position + 2u, 800u);
        ae_wr16(ram, position + 4u, (uint32_t)z);
        ae_wr16(ram, rotation, 0u);
        ae_wr16(ram, rotation + 2u, 2048u);
        ae_wr16(ram, rotation + 4u, 0u);
        ae_HubExitUi_DrawModelB(ram, hooks, position, rotation,
                                instances + 0x20u, 4096u);
    }

    if (destination < 10 &&
        slot == menu_route_count(ram, destination) - 1 &&
        ae_HubExitUi_IsDestinationUnlocked(ram, hooks, destination + 1, 0)) {
        cosine = ae_sdk_rcos(
            ram, (int32_t)ae_rd32(ram, AE_OVL189_G_FRAME_INDEX) * 16);
        z = (int16_t)(2048 + cosine / 16);
        ae_wr16(ram, position, 1548u);
        ae_wr16(ram, position + 2u, 800u);
        ae_wr16(ram, position + 4u, (uint32_t)z);
        ae_wr16(ram, rotation, 0u);
        ae_wr16(ram, rotation + 2u, 0u);
        ae_wr16(ram, rotation + 4u, 0u);
        ae_HubExitUi_DrawModelB(ram, hooks, position, rotation,
                                instances + 0x20u, 4096u);
    }
}

static void ovl189_render_progress_badge(AeRam *ram,
                                         const AeOvl189Hooks *hooks,
                                         int32_t destination, int32_t route,
                                         uint32_t model, uint32_t position,
                                         int32_t progress)
{
    const uint32_t args[8] = {
        (uint32_t)destination, (uint32_t)route, model,
        (uint32_t)ae_rds16(ram, position),
        (uint32_t)ae_rds16(ram, position + 2u),
        (uint32_t)ae_rds16(ram, position + 4u),
        (uint32_t)progress, ae_rd16(ram, AE_OVL189_G_UI_ANIMATION_COUNTER)
    };
    (void)OVL189_RENDER(hooks, ram, "HubExitUi_RenderProgressBadge", 8u, args);
    ae_wr32(ram, AE_OVL189_G_ORDER_TABLE_CURSOR,
            ae_rd32(ram, AE_OVL189_G_ORDER_TABLE_CURSOR) + OVL189_BADGE_PACKET_BYTES);
}

static void ovl189_render_collected_badge(AeRam *ram,
                                          const AeOvl189Hooks *hooks,
                                          int32_t destination, int32_t route,
                                          uint32_t model, uint32_t route_area)
{
    const uint32_t args[7] = {
        (uint32_t)destination, (uint32_t)route, model, route_area,
        (uint32_t)ae_rds32(ram, AE_OVL189_G_CAMERA_ANGLE),
        (uint32_t)ae_rds32(ram, AE_OVL189_G_CAMERA_DISTANCE),
        ae_rd16(ram, AE_OVL189_G_UI_ANIMATION_COUNTER)
    };
    (void)OVL189_RENDER(hooks, ram, "HubExitUi_RenderCollectedBadge", 7u, args);
    ae_wr32(ram, AE_OVL189_G_ORDER_TABLE_CURSOR,
            ae_rd32(ram, AE_OVL189_G_ORDER_TABLE_CURSOR) + OVL189_BADGE_PACKET_BYTES);
}

static void ovl189_update_transition(AeRam *ram)
{
    int32_t state = ae_rds32(ram, AE_OVL189_G_TRANSITION_STATE);
    int32_t distance = ae_rds32(ram, AE_OVL189_G_CAMERA_DISTANCE);
    int32_t destination = ae_rds32(ram, AE_OVL189_G_SELECTION);

    if (state == -1) {
        distance -= OVL189_TRANSITION_STEP;
        ae_wr32(ram, AE_OVL189_G_CAMERA_DISTANCE, (uint32_t)distance);
        if (distance < -OVL189_TRANSITION_DISTANCE) {
            destination = (destination + 1) % (int32_t)AE_OVL189_DESTINATION_COUNT;
            ae_wr32(ram, AE_OVL189_G_SELECTION, (uint32_t)destination);
            ae_wr32(ram, AE_OVL189_G_DESTINATION_SLOT, 0u);
            ae_wr32(ram, AE_OVL189_G_ROUTE_SELECTION, 0u);
            ae_wr32(ram, AE_OVL189_G_CAMERA_ANGLE, 0u);
            ae_wr32(ram, AE_OVL189_G_CAMERA_DISTANCE,
                    OVL189_TRANSITION_DISTANCE);
            ae_wr32(ram, AE_OVL189_G_TRANSITION_STATE, 0xFFFFFFFEu);
        }
    } else if (state == -2) {
        distance -= OVL189_TRANSITION_STEP;
        if (distance < 0) {
            distance = 0;
            ae_wr32(ram, AE_OVL189_G_TRANSITION_STATE, 0u);
        }
        ae_wr32(ram, AE_OVL189_G_CAMERA_DISTANCE, (uint32_t)distance);
    } else if (state == 1) {
        distance += OVL189_TRANSITION_STEP;
        ae_wr32(ram, AE_OVL189_G_CAMERA_DISTANCE, (uint32_t)distance);
        if (distance > OVL189_TRANSITION_DISTANCE) {
            destination = (destination + (int32_t)AE_OVL189_DESTINATION_COUNT - 1) %
                          (int32_t)AE_OVL189_DESTINATION_COUNT;
            int32_t last_slot = menu_route_count(ram, destination) - 1;
            ae_wr32(ram, AE_OVL189_G_SELECTION, (uint32_t)destination);
            ae_wr32(ram, AE_OVL189_G_DESTINATION_SLOT, (uint32_t)last_slot);
            ae_wr32(ram, AE_OVL189_G_CAMERA_ANGLE, (uint32_t)(-last_slot * 512));
            ae_wr32(ram, AE_OVL189_G_CAMERA_DISTANCE,
                    (uint32_t)-OVL189_TRANSITION_DISTANCE);
            ae_wr32(ram, AE_OVL189_G_TRANSITION_STATE, 2u);
        }
    } else if (state == 2) {
        distance += OVL189_TRANSITION_STEP;
        if (distance > 0) {
            distance = 0;
            ae_wr32(ram, AE_OVL189_G_TRANSITION_STATE, 0u);
        }
        ae_wr32(ram, AE_OVL189_G_CAMERA_DISTANCE, (uint32_t)distance);
    }
}

static void ovl189_render_stage(AeRam *ram, const AeOvl189Hooks *hooks,
                               int32_t level, int32_t progress,
                               int32_t time, int32_t rank)
{
    const uint32_t args[7] = {
        (uint32_t)level, (uint32_t)progress, (uint32_t)time,
        (uint32_t)rank, ae_rd32(ram, AE_OVL189_G_CAMERA_DISTANCE),
        ae_rd32(ram, AE_OVL189_G_ANIMATION_FRAME),
        AE_OVL189_G_STAGE_CONTEXT
    };
    (void)OVL189_RENDER(hooks, ram, "HubExitUi_RenderStageStatus", 7u, args);
    ae_wr32(ram, AE_OVL189_G_ORDER_TABLE_CURSOR,
            ae_rd32(ram, AE_OVL189_G_ORDER_TABLE_CURSOR) + OVL189_STAGE_PACKET_BYTES);
}

static void ovl189_render_status_icon(AeRam *ram,
                                      const AeOvl189Hooks *hooks)
{
    const uint32_t args[4] = {
        ae_rd32(ram, AE_OVL189_G_ORDER_TABLE_CURSOR),
        ae_rd32(ram, AE_OVL189_G_FRAME_INDEX),
        ae_rd32(ram, AE_OVL189_G_SELECTION),
        ae_rd32(ram, AE_OVL189_G_DESTINATION_SLOT)
    };
    (void)OVL189_RENDER(hooks, ram, "HubExitUi_RenderStatusIcon", 4u, args);
    ae_wr32(ram, AE_OVL189_G_ORDER_TABLE_CURSOR,
            ae_rd32(ram, AE_OVL189_G_ORDER_TABLE_CURSOR) +
                OVL189_STATUS_ICON_PACKET_BYTES);
}

static void ovl189_render_main_panel(AeRam *ram,
                                     const AeOvl189Hooks *hooks)
{
    uint32_t cursor = ae_rd32(ram, AE_OVL189_G_ORDER_TABLE_CURSOR);
    uint32_t context_index = ae_rd32(ram, AE_OVL189_G_RENDER_CONTEXT_INDEX);
    uint32_t animation = ae_rd16(ram, AE_OVL189_G_UI_ANIMATION_COUNTER);
    const uint32_t render_args[5] = {
        cursor, context_index, animation,
        ae_rd32(ram, AE_OVL189_G_SELECTION),
        ae_rd32(ram, AE_OVL189_G_ROUTE_SELECTION)
    };
    (void)OVL189_RENDER(hooks, ram, "HubExitUi_RenderMainPanel", 5u,
                        render_args);
    if (ae_ram_failed(ram)) return;

    uint16_t animation_state = (uint16_t)animation;
    for (uint32_t packet = 0; packet < OVL189_MENU_PANEL_PACKET_COUNT; ++packet)
        animation_state = (uint16_t)(animation_state * OVL189_MENU_PANEL_ANIMATION_MULTIPLIER + 1u);
    ae_wr16(ram, AE_OVL189_G_UI_ANIMATION_COUNTER, animation_state);

    uint32_t draw_args[4] = {cursor + OVL189_MENU_PANEL_PACKET_COUNT *
                                       OVL189_MENU_PANEL_PACKET_STRIDE,
                            0u, 1u, 0u};
    (void)ovl189_mode_call4(hooks, ram, "SetDrawTPage",
                            OVL189_CALL_SET_DRAW_TPAGE_ADDR,
                            draw_args[0], draw_args[1], draw_args[2],
                            draw_args[3]);
    if (ae_ram_failed(ram)) return;
    ae_wr32(ram, AE_OVL189_G_ORDER_TABLE_CURSOR, cursor + OVL189_MENU_PANEL_PACKET_COUNT *
                         OVL189_MENU_PANEL_PACKET_STRIDE +
                         OVL189_MENU_PANEL_LINK_BYTES);
}

int32_t ae_Mode_LeaveHub(AeRam *ram, const AeOvl189Hooks *hooks,
                         uint32_t display_argument)
{
    uint32_t area_records = AE_OVL189_G_AREA_DATA_RECORDS;
    uint32_t stage_context = AE_OVL189_G_STAGE_CONTEXT;
    int32_t area_count;
    uint32_t model_instances;
    uint32_t first_frame = 1u;
    int32_t rotation_angle = 0;
    int32_t spin_step = 16;
    int32_t route_look_at_y = 0;

    ae_HeapInit(ram, hooks ? hooks->area_set : NULL);
    if (ae_ram_failed(ram)) return 0;
    (void)ae_HubExitUi_Init(ram, hooks, display_argument);
    if (ae_ram_failed(ram)) return 0;
    ae_wr16(ram, AE_OVL189_G_TEXT_DRAW_STATE + 4u,
            OVL189_STAGE_ROUTE_NAME_TPAGE);
    ae_wr16(ram, AE_OVL189_G_TEXT_DRAW_STATE + 6u, 0x26u);

    (void)ovl189_mode_call1(hooks, ram, "SetGeomScreen",
                            OVL189_CALL_SET_GEOM_SCREEN_ADDR, 164u);
    ae_SetColorMatrixMode(ram, hooks ? hooks->items_init : NULL, 0u, 0u, 0u, 0u);
    ae_wr32(ram, OVL189_G_GEOMETRY_STATE + OVL189_GEOMETRY_STATE_TRANSLATION_X, 0u);
    ae_wr32(ram, OVL189_G_GEOMETRY_STATE + OVL189_GEOMETRY_STATE_TRANSLATION_Y, 4096u);
    ae_wr32(ram, OVL189_G_GEOMETRY_STATE + OVL189_GEOMETRY_STATE_TRANSLATION_Z, 4096u);
    ae_wr32(ram, OVL189_G_GEOMETRY_STATE + OVL189_GEOMETRY_STATE_SCALE, 0u);
    ae_ApplyLevelFog(ram, hooks ? hooks->items_init : NULL);
    if (ae_ram_failed(ram)) return 0;

    (void)ovl189_mode_call1(hooks, ram, "ArcLoadEntry",
                            OVL189_CALL_ARC_LOAD_ENTRY_ADDR, 0x80139BA0u);
    while (ovl189_mode_call0(hooks, ram, "LoadListStep",
                             OVL189_CALL_LOAD_LIST_STEP_ADDR) >= 0) {
        (void)ovl189_mode_call0(hooks, ram, "CdMgrUpdate",
                                OVL189_CALL_CD_MGR_UPDATE_ADDR);
        (void)ovl189_mode_call1(hooks, ram, "VSync", OVL189_CALL_VSYNC_ADDR, 2u);
        if (ae_ram_failed(ram)) return 0;
    }

    (void)ae_AreaSetVramAnimList(ram, hooks ? hooks->vram_anim : NULL, 0u);
    area_count = ae_rd8(ram, AE_OVL189_G_AREA_DATA_COUNT);
    model_instances = ae_HeapAlloc(
        ram, hooks ? hooks->area_set : NULL,
        (uint32_t)area_count * OVL189_AREA_INSTANCE_STRIDE);
    if (ae_ram_failed(ram)) return 0;

    for (int32_t area_index = 1; area_index <= area_count; ++area_index) {
        uint32_t record = area_records +
                          (uint32_t)(area_index - 1) * OVL189_AREA_RECORD_STRIDE;
        uint32_t kind = ae_rd8(ram, record + OVL189_AREA_RECORD_KIND);
        uint32_t model_index = ae_rd16(ram, record + OVL189_AREA_RECORD_MODEL_INDEX);
        uint32_t instance = model_instances +
                            (uint32_t)(area_index - 1) * OVL189_AREA_INSTANCE_STRIDE;
        uint32_t model_header = AE_OVL189_G_AREA_DATA_COUNT +
                                model_index * OVL189_AREA_RECORD_STRIDE;

        if (kind == OVL189_AREA_DATA_KIND_MESH) {
            (void)ovl189_mode_call2(hooks, ram, "MeshInitFromAreaTable",
                                    OVL189_CALL_MESH_INIT_ADDR,
                                    instance, model_header);
        } else if (kind == OVL189_AREA_DATA_KIND_STATIC_MODEL) {
            ae_StaticModelInit(ram, hooks ? hooks->area_set : NULL,
                               instance, model_header);
        } else if (kind == OVL189_AREA_DATA_KIND_VRAM_ANIM) {
            (void)ae_AreaSetVramAnimList(
                ram, hooks ? hooks->vram_anim : NULL, model_header);
        }
        if (ae_ram_failed(ram)) return 0;
    }

    uint32_t stage_ui_flags = AE_OVL189_G_STAGE_CONTEXT + AE_OVL189_STAGE_CONTEXT_UI_FLAGS;
    ae_wr16(ram, stage_ui_flags, ae_rd16(ram, stage_ui_flags) | 2u);
    (void)ovl189_mode_call0(hooks, ram, "SoundStopAndRestoreVolume",
                            OVL189_CALL_SOUND_RESTORE_ADDR);
    (void)ovl189_mode_call0(hooks, ram, "PlayerInit",
                            OVL189_CALL_PLAYER_INIT_ADDR);
    (void)ae_sdk_MatrixIdentity(ram, AE_OVL189_G_VIEW_MATRIX);
    ae_pad_PadUpdate(ram, hooks ? hooks->pad : NULL,
                     OVL189_CALL_PAD_UPDATE_FLAGS);
    if (ae_ram_failed(ram)) return 0;

    if (ae_rd8(ram, stage_context + OVL189_STAGE_CONTEXT_INTRO_FLAG) != 0u) {
        ae_wr8(ram, stage_context + OVL189_STAGE_CONTEXT_INTRO_FLAG, 0u);
        ae_wr32(ram, OVL189_GAME_INPUT_ROUTE_SKIP,
                ae_rd32(ram, OVL189_GAME_INPUT_ROUTE_SKIP) |
                OVL189_MODE_STAGE_SKIP_BIT);
    }
    (void)ovl189_mode_call0(hooks, ram, "BeginFrame",
                            OVL189_CALL_BEGIN_FRAME_ADDR);
    ae_ColorFadeReset(ram);

    const uint32_t eye = AE_OVL189_TEMP_VIEW_ANGLES;
    const uint32_t target = AE_OVL189_TEMP_VIEW_ANGLES + 8u;
    const uint32_t route_eye = AE_OVL189_TEMP_LOOK_AT_EYE;
    const uint32_t route_target = AE_OVL189_TEMP_LOOK_AT_TARGET;
    ae_wr16(ram, eye, 0u);
    ae_wr16(ram, eye + 2u, (uint32_t)route_look_at_y);
    ae_wr16(ram, eye + 4u, 0u);
    ae_wr16(ram, target, 0u);
    ae_wr16(ram, target + 2u, 0u);
    ae_wr16(ram, target + 4u, 2624u);
    ae_SaveUi_LookAt_189(ram, eye, target);

    ae_wr32(ram, AE_OVL189_G_SCROLL_POSITION, 0u);
    ae_wr32(ram, AE_OVL189_G_ROUTE_SELECTION, 0u);
    ae_wr32(ram, AE_OVL189_G_SELECTION, (uint32_t)(int32_t)ae_rds8(ram, stage_context + OVL189_STAGE_CONTEXT_INITIAL_ROUTE));
    ae_wr32(ram, AE_OVL189_G_DESTINATION_SLOT, (uint32_t)(int32_t)ae_rds8(ram, stage_context + OVL189_STAGE_CONTEXT_INITIAL_SLOT));
    ae_wr32(ram, AE_OVL189_G_SCREEN_STATE, 3u);
    ae_wr32(ram, AE_OVL189_G_CAMERA_ANGLE,
            (uint32_t)(-(int32_t)ae_rds8(
                ram, stage_context + OVL189_STAGE_CONTEXT_INITIAL_SLOT) * 512));
    ae_wr32(ram, AE_OVL189_G_CAMERA_DISTANCE, 0u);

    for (;;) {
        if (first_frame == 0u) {
            ae_pad_PadUpdate(ram, hooks ? hooks->pad : NULL,
                             OVL189_CALL_PAD_UPDATE_FLAGS);
            (void)ovl189_mode_call0(hooks, ram, "BeginFrame",
                                    OVL189_CALL_BEGIN_FRAME_ADDR);
            ae_ColorFadeReset(ram);
            ae_wr16(ram, eye, 0u);
            ae_wr16(ram, eye + 2u, 0u);
            ae_wr16(ram, eye + 4u, 0u);
            ae_SaveUi_LookAt_189(ram, eye, target);
            if (ae_ram_failed(ram)) return 0;
        }
        first_frame = 0u;

        int32_t screen_state = ae_rds32(ram, AE_OVL189_G_SCREEN_STATE);
        if (screen_state != 1) {
            ae_VramScrollUpdate(ram, hooks ? hooks->misc : NULL);
            (void)ovl189_mode_call1(hooks, ram, "SetGeomScreen",
                                    OVL189_CALL_SET_GEOM_SCREEN_ADDR, 164u);

            uint32_t rotation = AE_OVL189_TEMP_VIEW_ROTATION;
            (void)ae_sdk_MatrixIdentity(ram, rotation);
            int32_t camera_turn_sdk =
                (rotation_angle < 0 ? rotation_angle + 7 : rotation_angle) / 8;
            ae_wr16(ram, AE_OVL189_TEMP_VIEW_ANGLES,
                    (uint32_t)camera_turn_sdk);
            ae_wr16(ram, AE_OVL189_TEMP_VIEW_ANGLES + 2u, 0u);
            ae_wr16(ram, AE_OVL189_TEMP_VIEW_ANGLES + 4u, 0u);
            (void)ovl189_mode_call2(hooks, ram, "RotMatrixZ",
                                    OVL189_CALL_ROT_MATRIX_Z_ADDR,
                                    (uint32_t)camera_turn_sdk, rotation);
            (void)ovl189_mode_call2(hooks, ram, "RotMatrixY",
                                    OVL189_CALL_ROT_MATRIX_Y_ADDR,
                                    (uint32_t)camera_turn_sdk, rotation);
            (void)ovl189_mode_call2(hooks, ram, "RotMatrixX",
                                    OVL189_CALL_ROT_MATRIX_X_ADDR,
                                    (uint32_t)camera_turn_sdk, rotation);
            ae_MeshTransform(ram, hooks ? hooks->effects : NULL,
                             model_instances, rotation,
                             AE_OVL189_G_COLOR_MATRIX);
            (void)ovl189_mode_call1(
                hooks, ram, "ModelEmitPrims",
                OVL189_CALL_MODEL_EMIT_PRIMS_ADDR,
                ovl189_render_context_address(ram,
                                              OVL189_ORDER_TABLE_TAG_OFFSET));
            (void)ovl189_mode_call1(hooks, ram, "SetGeomScreen",
                                    OVL189_CALL_SET_GEOM_SCREEN_ADDR, 1312u);
            ovl189_draw_scene_decorations(ram, hooks, model_instances,
                                           &rotation_angle, &spin_step);
            if (ae_ram_failed(ram)) return 0;

            ae_wr16(ram, route_eye, 0u);
            ae_wr16(ram, route_eye + 2u, (uint32_t)route_look_at_y);
            ae_wr16(ram, route_eye + 4u, 0u);
            ae_wr16(ram, route_target, 0u);
            ae_wr16(ram, route_target + 2u, 0u);
            ae_wr16(ram, route_target + 4u, 2624u);
            ae_SaveUi_LookAt_189(ram, route_eye, route_target);
            if (ae_ram_failed(ram)) return 0;

            if (ae_rds32(ram, AE_OVL189_G_SELECTION) == 7 &&
                ae_rds32(ram, AE_OVL189_G_DESTINATION_SLOT) == 2) {
                if (route_look_at_y < -1399) route_look_at_y = -1400;
                else route_look_at_y -= 50;
            } else if (route_look_at_y < 0) {
                route_look_at_y += 50;
            } else {
                route_look_at_y = 0;
            }

            ovl189_update_route_camera(ram);
            int32_t selection = ae_rds32(ram, AE_OVL189_G_SELECTION);
            int32_t route_count = menu_route_count(ram, selection);
            uint32_t frame_index = ae_rd16(ram, AE_OVL189_G_FRAME_INDEX);
            uint32_t frame_counter = ae_rd32(ram, AE_OVL189_G_FRAME_INDEX);
            for (int32_t route = 0; route < route_count; ++route) {
                uint32_t route_area = ae_rd8(
                    ram, destination_route_address(selection, route) +
                             OVL189_LEVEL_ROUTE_FIELD);
                int32_t unlocked = ae_HubExitUi_IsDestinationUnlocked(
                    ram, hooks, selection, route);
                int32_t name_index = ae_rds8(
                    ram, AE_OVL189_DESTINATION_NAME_INDEX +
                             (uint32_t)selection * OVL189_DESTINATION_NAME_STRIDE);
                uint32_t model = ovl189_destination_model(model_instances,
                                                          name_index) +
                                 (uint32_t)route * OVL189_MODEL_INSTANCE_BYTES;
                int32_t angle = ae_rds32(ram, AE_OVL189_G_CAMERA_ANGLE) +
                                route * 512;
                int32_t sine = ae_sdk_rsin(ram, angle);
                int32_t cosine = ae_sdk_rcos(ram, angle);
                int32_t camera_distance = (int32_t)ae_rd16(
                    ram, AE_OVL189_G_CAMERA_DISTANCE);
                uint32_t position = AE_OVL189_TEMP_ROUTE_POSITION;
                uint32_t model_rotation = AE_OVL189_TEMP_ROUTE_ROTATION;
                ae_wr16(ram, position,
                        (uint32_t)(camera_distance + ovl189_sra32(sine, 1u)));
                ae_wr16(ram, position + 2u,
                        (uint32_t)(ovl189_sra32(cosine, 2u) - 300));
                ae_wr16(ram, position + 4u,
                        (uint32_t)(3800 - ovl189_sra32(cosine, 1u)));
                ae_wr16(ram, model_rotation, 128u);
                ae_wr16(ram, model_rotation + 2u,
                        (uint32_t)(-(int32_t)frame_index * 8));
                ae_wr16(ram, model_rotation + 4u, 0u);
                ae_HubExitUi_DrawModelA(ram, hooks, position, model_rotation,
                                        model, unlocked ? 4096u : 512u);

                int32_t progress = ae_HubExitUi_GetProgress(
                    ram, hooks, selection, route);
                if (progress == 1 ||
                    (progress == 2 && ((frame_counter / 15u) & 1u) != 0u)) {
                    ovl189_render_progress_badge(ram, hooks, selection, route,
                                                 model, position, progress);
                }

                int32_t flags = ae_HubExitUi_GetFlags(
                    ram, hooks, selection, route);
                if (flags != 0) {
                    ovl189_render_collected_badge(ram, hooks, selection, route,
                                                  model, route_area);
                }
                if (ae_ram_failed(ram)) return 0;
            }

            selection = ae_rds32(ram, AE_OVL189_G_SELECTION);
            int32_t slot = ae_rds32(ram, AE_OVL189_G_DESTINATION_SLOT);
            uint32_t name_index_address = AE_OVL189_DESTINATION_NAME_INDEX +
                                          (uint32_t)selection *
                                              OVL189_DESTINATION_NAME_STRIDE +
                                          (uint32_t)slot;
            int32_t name_index = ae_rds8(ram, name_index_address);
            if (name_index != -1) {
                uint32_t stage_name = ae_SaveUi_StringTableIndex_189(
                    ram, AE_OVL189_DESTINATION_NAME_DATA, name_index);
                int32_t width = ae_HubExitUi_TextWidth(ram, stage_name);
                int32_t center_x = 192 - ((width + (width < 0 ? 1 : 0)) >> 1);
                ae_wr16(ram, AE_OVL189_G_TEXT_DRAW_STATE, (uint32_t)center_x);
                ae_wr16(ram, AE_OVL189_G_TEXT_DRAW_STATE + 2u,
                        OVL189_STAGE_ROUTE_NAME_Y);
                ae_wr16(ram, AE_OVL189_G_TEXT_DRAW_STATE + 4u,
                        OVL189_STAGE_ROUTE_NAME_TPAGE);
                ae_wr32(ram, AE_OVL189_G_TEXT_DRAW_STATE + 8u,
                        ovl189_render_context_address(ram, 0u));
                ae_HubExitUi_DrawText(ram, hooks, stage_name);
            }

            ovl189_update_transition(ram);
            ovl189_render_status_icon(ram, hooks);

            selection = ae_rds32(ram, AE_OVL189_G_SELECTION);
            int32_t current_slot = ae_rds32(ram, AE_OVL189_G_DESTINATION_SLOT);
            int32_t level_area = destination_level_area(ram, selection,
                                                        current_slot);
            int32_t level = ae_AreaGetLevel(
                ram, hooks ? hooks->area_set : NULL, level_area);
            (void)ovl189_mode_call2(hooks, ram, "LevelGetInfo",
                                    OVL189_CALL_LEVEL_GET_INFO_ADDR,
                                    (uint32_t)level, OVL189_LEVEL_INFO_SCRATCH);
            int32_t current_time = ae_rds32(ram, OVL189_LEVEL_INFO_SCRATCH +
                                                   OVL189_LEVEL_INFO_TIME_FIELD);
            ae_wr32(ram, AE_OVL189_G_STAGE_CONTEXT +
                            OVL189_STAGE_CONTEXT_LEVEL_TIME,
                    (uint32_t)current_time);
            int32_t current_progress = ae_HubExitUi_GetProgress(ram, hooks, selection, current_slot);
            if (current_progress == 2 && level != 30) {
                uint32_t frame = ae_rd32(ram, AE_OVL189_G_ANIMATION_FRAME);
                ae_wr32(ram, AE_OVL189_G_ANIMATION_FRAME,
                        frame + OVL189_FRAME_COUNTER_STEP);
                ae_HudDrawTime(ram, hooks ? hooks->hud : NULL,
                               OVL189_LEVEL_TIME_X, OVL189_LEVEL_TIME_Y,
                               current_time);
                int32_t rank = ovl189_mode_call1(
                    hooks, ram, "LevelTableGet",
                    OVL189_CALL_LEVEL_TABLE_GET_ADDR, (uint32_t)level);
                (void)ovl189_mode_call4(
                    hooks, ram, "HudDrawTimeRankIcon",
                    OVL189_CALL_HUD_DRAW_RANK_ADDR,
                    OVL189_RANK_ICON_X, OVL189_RANK_ICON_Y,
                    (uint32_t)current_time, (uint32_t)rank);
                ovl189_render_stage(ram, hooks, level, current_progress,
                                    current_time, rank);
            }
            if (ae_ram_failed(ram)) return 0;
        }

        ovl189_render_main_panel(ram, hooks);
        if (ae_ram_failed(ram)) return 0;
        int32_t result = ae_HubExitUi_Run(ram, hooks);
        if (ae_ram_failed(ram)) return 0;

        if (result >= 0) {
            (void)ovl189_mode_call1(hooks, ram, "DrawSync",
                                    OVL189_CALL_DRAW_SYNC_ADDR, 0u);
            (void)ovl189_mode_call1(hooks, ram, "VSync",
                                    OVL189_CALL_VSYNC_ADDR, 2u);
            uint32_t display_index = ae_rd32(ram, OVL189_G_RENDER_CONTEXT_INDEX);
            uint32_t display = AE_OVL189_RENDER_PACKET_BASE + 0x6Cu +
                               display_index * OVL189_DISPLAY_ENV_STRIDE;
            ae_wr32(ram, AE_OVL189_G_FRAME_INDEX,
                    ae_rd32(ram, AE_OVL189_G_FRAME_INDEX) + 1u);
            (void)ovl189_mode_call1(hooks, ram, "PutDispEnv",
                                    OVL189_CALL_PUT_DISP_ENV_ADDR, display);
            uint32_t draw_environment = AE_OVL189_RENDER_PACKET_BASE + 0x10u +
                                        display_index * OVL189_DISPLAY_ENV_STRIDE;
            (void)ovl189_mode_call1(hooks, ram, "PutDrawEnv",
                                    OVL189_CALL_PUT_DRAW_ENV_ADDR,
                                    draw_environment);
            uint32_t order_tag = AE_OVL189_G_ORDERING_TABLE_ROOT +
                                 display_index * OVL189_DISPLAY_ENV_STRIDE +
                                 0x3FFCu;
            (void)ovl189_mode_call1(hooks, ram, "DrawOTag",
                                    OVL189_CALL_DRAW_OTAG_ADDR, order_tag);
            (void)ovl189_mode_call0(hooks, ram, "CdMgrUpdate",
                                    OVL189_CALL_CD_MGR_UPDATE_ADDR);
            if (ae_ram_failed(ram)) return 0;
            continue;
        }

        int32_t selection = ae_rds32(ram, AE_OVL189_G_SELECTION);
        int32_t slot = ae_rds32(ram, AE_OVL189_G_DESTINATION_SLOT);
        int32_t name_index = ae_rds8(
            ram, AE_OVL189_DESTINATION_NAME_INDEX +
                     (uint32_t)selection * 4u + (uint32_t)slot);
        uint32_t name = ae_SaveUi_StringTableIndex_189(
            ram, AE_OVL189_DESTINATION_NAME_DATA, name_index);
        for (uint32_t byte = 0u;
             byte < OVL189_DESTINATION_NAME_COPY_BYTES; ++byte) {
            ae_wr8(ram, OVL189_DESTINATION_NAME_BUFFER + byte,
                   ae_rd8(ram, name + byte));
        }
        (void)ovl189_mode_call1(hooks, ram, "DrawSync",
                                OVL189_CALL_DRAW_SYNC_ADDR, 0u);
        (void)ovl189_mode_call1(hooks, ram, "VSync",
                                OVL189_CALL_VSYNC_ADDR, 0u);
        int32_t mode_index = result + 8;
        switch (mode_index) {
        case 0:
            ae_wr16(ram, OVL189_G_MODE_SWITCH, 18u);
            break;
        case 1:
            ae_wr16(ram, OVL189_G_MODE_SWITCH, 13u);
            break;
        case 2:
            ae_wr16(ram, OVL189_G_MODE_SWITCH, 16u);
            break;
        case 3:
            break;
        case 4:
            ae_wr16(ram, OVL189_G_MODE_SWITCH, 10u);
            break;
        case 5:
            ae_wr16(ram, OVL189_G_MODE_SWITCH, 8u);
            break;
        case 6:
            ae_wr16(ram, OVL189_G_MODE_SWITCH, 11u);
            break;
        case 7:
            ae_wr16(ram, OVL189_G_MODE_SWITCH, 0u);
            break;
        default:
            break;
        }
        (void)ovl189_mode_call0(hooks, ram, "ProgressGetByte",
                                OVL189_CALL_PROGRESS_GET_BYTE_ADDR);
        return 0;
    }
}

static int32_t ovl189_sra32(int32_t value, uint32_t shift)
{
    if (shift == 0u) return value;
    uint32_t bits = (uint32_t)value;
    uint32_t shifted = bits >> shift;
    if ((bits & 0x80000000u) != 0u) shifted |= ~(0xFFFFFFFFu >> shift);
    return (int32_t)shifted;
}

static int32_t ovl189_low_product(int32_t left, int32_t right)
{
    return (int32_t)((uint32_t)left * (uint32_t)right);
}

void ae_SaveUi_LookAt_189(AeRam *ram, uint32_t eye, uint32_t target)
{
    uint32_t view = AE_OVL189_G_VIEW_MATRIX;
    uint32_t direction = AE_OVL189_TEMP_VECTOR;
    uint32_t normalized = AE_OVL189_TEMP_NORMALIZED;

    for (uint32_t component = 0u; component < 3u; ++component) {
        uint32_t source = eye + component * OVL189_ANGLE_COMPONENT_BYTES;
        uint32_t destination = view + OVL189_VIEW_MATRIX_TRANSLATION +
                               component * OVL189_VECTOR_COMPONENT_BYTES;
        ae_wr32(ram, destination, (uint32_t)(0u - (uint32_t)ae_rds16(ram, source)));
    }

    int32_t x = ae_rds16(ram, target) - ae_rds16(ram, eye);
    int32_t y = ae_rds16(ram, target + 2u) - ae_rds16(ram, eye + 2u);
    int32_t z = ae_rds16(ram, target + 4u) - ae_rds16(ram, eye + 4u);
    ae_wr32(ram, direction, (uint32_t)x);
    ae_wr32(ram, direction + 4u, 0u);
    ae_wr32(ram, direction + 8u,
            (uint32_t)((x == 0 && z == 0) ? 0x1000 : z));
    (void)ae_sdk_VectorNormal(ram, direction, normalized);
    if (ae_ram_failed(ram)) return;
    int32_t flat_normal_x = ae_rds32(ram, normalized);
    int32_t flat_normal_z = ae_rds32(ram, normalized + 8u);

    ae_wr32(ram, direction + 4u, (uint32_t)y);
    (void)ae_sdk_VectorNormal(ram, direction, normalized);
    if (ae_ram_failed(ram)) return;

    int32_t normalized_x = ae_rds32(ram, normalized);
    int32_t normalized_y = ae_rds32(ram, normalized + 4u);
    int32_t normalized_z = ae_rds32(ram, normalized + 8u);
    int32_t matrix_x = (int32_t)(0u - (uint32_t)flat_normal_x);
    int32_t horizontal_squared = (int32_t)((uint32_t)ovl189_low_product(
                                               normalized_z, normalized_z) +
                                           (uint32_t)ovl189_low_product(
                                               normalized_x, normalized_x));
    int32_t horizontal = ae_sdk_SquareRoot0(ram, horizontal_squared);
    if (ae_ram_failed(ram)) return;

    ae_wr16(ram, view, (uint32_t)flat_normal_z);
    ae_wr16(ram, view + 2u, 0u);
    ae_wr16(ram, view + 4u, (uint32_t)matrix_x);
    ae_wr16(ram, view + 6u,
            (uint32_t)ovl189_sra32(ovl189_low_product(matrix_x, normalized_y), 12u));
    ae_wr16(ram, view + 8u, (uint32_t)horizontal);
    ae_wr16(ram, view + 10u,
            (uint32_t)ovl189_sra32(
                (int32_t)(0u - (uint32_t)ovl189_low_product(normalized_y,
                                                              flat_normal_z)), 12u));
    ae_wr16(ram, view + 12u,
            (uint32_t)ovl189_sra32(
                (int32_t)(0u - (uint32_t)ovl189_low_product(horizontal, matrix_x)),
                12u));
    ae_wr16(ram, view + 14u, (uint32_t)normalized_y);
    ae_wr16(ram, view + 16u,
            (uint32_t)ovl189_sra32(ovl189_low_product(horizontal, flat_normal_z), 12u));

    uint8_t *matrix_bytes = ae_ptr(ram, view, OVL189_VIEW_MATRIX_BYTES);
    uint8_t *translation_bytes = ae_ptr(ram, view + OVL189_VIEW_MATRIX_TRANSLATION, 16u);
    if (ae_ram_failed(ram)) return;
    if (!ae_sdk_apply_matrix_lv(matrix_bytes, translation_bytes, translation_bytes)) {
        ae_ram_fail(ram, "ApplyMatrixLV failed in SaveUi_LookAt_189");
        return;
    }
    (void)ae_sdk_CompMatrix(ram, AE_OVL189_G_BASE_MATRIX, view, view);
}
void ae_HubExitUi_DrawText(AeRam *ram, const AeOvl189Hooks *hooks,
                           uint32_t string)
{
    uint32_t packet_cursor = ae_rd32(ram, AE_OVL189_G_ORDER_TABLE_CURSOR);

    while (ae_rd8(ram, string) != 0u) {
        uint32_t character = ae_rd8(ram, string);
        const uint32_t args[6] = {
            string,
            ae_rd16(ram, AE_OVL189_G_TEXT_DRAW_STATE),
            ae_rd16(ram, AE_OVL189_G_TEXT_DRAW_STATE + 2u),
            ae_rd16(ram, AE_OVL189_G_TEXT_DRAW_STATE + 4u),
            ae_rd32(ram, AE_OVL189_G_TEXT_DRAW_STATE + 8u),
            packet_cursor
        };
        (void)OVL189_RENDER(hooks, ram, "HubExitUi_DrawText", 6u, args);

        uint16_t random_seed = (uint16_t)ae_rd16(
            ram, OVL189_G_TEXT_RANDOM_SEED);
        random_seed = (uint16_t)((uint32_t)random_seed * 5u + 1u);
        ae_wr16(ram, OVL189_G_TEXT_RANDOM_SEED, random_seed);

        uint16_t text_x = (uint16_t)ae_rd16(
            ram, AE_OVL189_G_TEXT_DRAW_STATE);
        uint32_t character_width = ae_rd8(
            ram, OVL189_NAME_WIDTH_TABLE + character);
        ae_wr16(ram, AE_OVL189_G_TEXT_DRAW_STATE,
                (uint16_t)((uint32_t)text_x + character_width));

        string += 1u;
        packet_cursor += AE_OVL189_RENDER_PACKET_SIZE;
    }
    ae_wr32(ram, AE_OVL189_G_ORDER_TABLE_CURSOR, packet_cursor);
}

uint32_t ae_SaveUi_StringTableIndex_189(AeRam *ram, uint32_t strings,
                                        int32_t index)
{
    if (index <= 0) return strings;
    int32_t found = 0;
    do {
        if (ae_rd8(ram, strings) == 0u) found += 1;
        strings += 1u;
    } while (found < index);
    return strings;
}

int32_t ae_HubExitUi_TextWidth(AeRam *ram, uint32_t string)
{
    int32_t width = 0;
    while (ae_rd8(ram, string) != 0u) {
        uint32_t character = ae_rd8(ram, string);
        width += (int32_t)ae_rd8(ram, OVL189_NAME_WIDTH_TABLE + character);
        string += 1u;
    }
    return width;
}
