#include "ae_missb.h"
#include "ae_hud.h"
#include "ae_pausemenu.h"
#include "../world/ae_progress.h"
#include "../world/ae_zako_init.h"
#include "../world/ae_zako2.h"
#include "../sound/ae_sound.h"
#include "../player/ae_player_spawn.h"
#include "../world/ae_misc.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"

#define AE_MISSB_UI_TEXT_ADVANCE_TABLE 0x800B2734u
#define AE_MISSB_UI_PAUSE_VOLUME_STATE 0x800BEF90u
#define AE_MISSB_UI_G_FRAME_COUNTER 0x800EC350u
#define AE_MISSB_UI_G_ENV_ANGLE_VECTOR 0x800E7D00u
#define AE_MISSB_UI_G_ENV_MATRIX 0x800F4D00u
#define AE_MISSB_UI_G_MODEL_INSTANCE 0x800EC358u
#define AE_MISSB_UI_G_VIEW_BASE_MATRIX 0x800BF0A0u
#define AE_MISSB_UI_ENV_VECTOR_LENGTH 12u
#define AE_MISSB_UI_TRANSFORM_LOCAL_OFFSET 0x10u
#define AE_MISSB_UI_MATRIX_TRANSLATION_X 0x14u
#define AE_MISSB_UI_MATRIX_TRANSLATION_Y 0x18u
#define AE_MISSB_UI_MATRIX_TRANSLATION_Z 0x1Cu
#define AE_MISSB_UI_SVECTOR_Y 0x02u
#define AE_MISSB_UI_SVECTOR_Z 0x04u
#define AE_MISSB_UI_HUD_STACK_SIZE 72u
#define AE_MISSB_UI_G_ENV_ACTIVE 0x1F8003F4u
#define AE_MISSB_UI_G_MESSAGE_BOX 0x800F5A00u
#define AE_MISSB_UI_MESSAGE_BOX_ACTIVE 0x00u
#define AE_MISSB_UI_MESSAGE_BOX_WIDTH 0x02u
#define AE_MISSB_UI_MESSAGE_BOX_STATE 0x10u
#define AE_MISSB_UI_MESSAGE_BOX_STYLE 0x12u
#define AE_MISSB_UI_MESSAGE_BOX_TIMER 0x14u
#define AE_MISSB_UI_MESSAGE_BOX_CLOSED 0x16u
#define AE_MISSB_UI_G_STAGE_STATE 0x800F4350u
#define AE_MISSB_UI_G_CAPTURE_TOTAL 0x186u
#define AE_MISSB_UI_G_CAPTURE_COUNT 0x166u
#define AE_MISSB_UI_G_CAPTURE_GOAL 0x188u
#define AE_MISSB_UI_G_STAGE_LEVEL 0x124u
#define AE_MISSB_UI_G_STAGE_GRADE 0x12Eu
#define AE_MISSB_UI_G_CAPTURE_BEST 0x18Au
#define AE_MISSB_UI_G_BEST_TIME 0x148u
#define AE_MISSB_UI_G_LAST_TIME 0x14Cu
#define AE_MISSB_UI_STAGE_LEVEL_TABLE_OFFSET 0x144u
#define AE_MISSB_UI_CAPTURE_OT 0x04u
#define AE_MISSB_UI_CAPTURE_OT_CURSOR 0x08u
#define AE_MISSB_UI_CAPTURE_PACKET_SIZE 20u
#define AE_MISSB_UI_CAPTURE_SCRATCH 0x1F800000u
#define AE_MISSB_UI_PAUSE_VOLUME_PAGE 0x800BEF80u
#define AE_MISSB_UI_PAUSE_SUBSCREEN 0x800BEF74u
#define AE_MISSB_UI_PAUSE_VOLUME_OUTPUT 0x04u
#define AE_MISSB_UI_G_PRIMITIVE_CURSOR 0x800EC354u
#define AE_MISSB_UI_G_PAUSE_SELECTION 0x800BEF90u
#define AE_MISSB_UI_PAUSE_SFX_VOLUME 0x04u
#define AE_MISSB_UI_PAUSE_BGM_VOLUME 0x08u
#define AE_MISSB_UI_PAUSE_SETTING_A 0x138u
#define AE_MISSB_UI_PAUSE_SETTING_B 0x13Au
#define AE_MISSB_UI_G_PAD_PRESSED 0x800E5478u
#define AE_MISSB_UI_G_PAD_HELD 0x800E5480u
#define AE_MISSB_UI_G_MENU_STRINGS 0x800B2864u
#define AE_MISSB_UI_MENU_RECORD_SIZE 8u
#define AE_MISSB_UI_MENU_RECORD_TABLE 0x800B264Cu
#define AE_MISSB_UI_MENU_RECORD_X 0u
#define AE_MISSB_UI_MENU_RECORD_Y 2u
#define AE_MISSB_UI_MENU_RECORD_WIDTH 4u
#define AE_MISSB_UI_MENU_RECORD_HEIGHT 6u
#define AE_MISSB_UI_CAPTURE_LEVEL_TYPE 0x2CCu
#define AE_MISSB_UI_CAPTURE_LEVEL_TYPE_TABLE 0x800B2640u
#define AE_MISSB_UI_CAPTURE_TIME_ATTACK_LEVEL_A 7u
#define AE_MISSB_UI_CAPTURE_TIME_ATTACK_LEVEL_B 14u
#define AE_MISSB_UI_CAPTURE_TIME_ATTACK_AREA_A 19
#define AE_MISSB_UI_CAPTURE_TIME_ATTACK_AREA_B 36
#define AE_MISSB_UI_CAPTURE_RESULT_FLAG 0x10u
#define AE_MISSB_UI_CAPTURE_GRID_PACKET_SIZE 20u
#define AE_MISSB_UI_CAPTURE_GRID_TERMINATOR_SIZE 8u
#define AE_MISSB_UI_SUBSCREEN_BASE_PACKET_SIZE 20u
#define AE_MISSB_UI_SUBSCREEN_ICON_COUNT 5u
#define AE_MISSB_UI_SUBSCREEN_ICON_PACKET_SIZE 16u
#define AE_MISSB_UI_ARROW_BASE_PACKET_SIZE 40u
#define AE_MISSB_UI_PAUSE_ARROW_BASE_PACKETS 2u
#define AE_MISSB_UI_PAUSE_ARROW_BAR_STEP 8u
#define AE_MISSB_UI_PAUSE_ARROW_BAR_PACKET_SIZE 36u

static uint8_t ui_digit_code(int32_t digit)
{
    return (uint8_t)(digit < 6 ? digit + 106 : digit + 116);
}

static void ui_write_counter_checksum(AeRam *r, uint32_t destination,
                                      int32_t value)
{
    for (uint32_t i = 4u; i != 0u; --i) {
        if (i == 1u) {
            ae_wr8(r, destination + 3u, 172u);
        }
        int32_t quotient = value / 10;
        int32_t remainder = value - quotient * 10;
        uint32_t output = i == 4u ? 4u : i - 1u;
        ae_wr8(r, destination + output, ui_digit_code(remainder));
        value = quotient;
    }
    uint8_t thousands = (uint8_t)ae_rd8(r, destination);
    uint8_t hundreds = (uint8_t)ae_rd8(r, destination + 1u);
    if (thousands == 106u) {
        /* The original's second BNE delay slot blanks the thousands glyph
         * even when the hundreds comparison branches around the next store. */
        ae_wr8(r, destination, 15u);
        if (hundreds == thousands) ae_wr8(r, destination + 1u, 15u);
    }
}

static const AeMissbHooks *ui_caller_frame_hooks(AeMissbHooks *child,
                                                 const AeMissbHooks *parent,
                                                 uint32_t frame_size)
{
    if (parent == NULL) return NULL;
    *child = *parent;
    uint32_t stack_pointer = parent->stack_pointer != 0u
        ? parent->stack_pointer : 0x801FFF00u;
    child->stack_pointer = stack_pointer - frame_size;
    return child;
}

static void ui_draw_text(AeRam *r, const AeMissbHooks *h, uint32_t text,
                         int32_t x, int32_t y, int32_t page)
{
    if (h == NULL || h->progress == NULL) {
        ae_ram_fail(r, "AeMissbHooks.progress not provided for DrawTextSprites");
        return;
    }
    ae_DrawTextSprites(r, h->progress, text, x, y, page);
}

static void ui_render_menu_arrows(AeRam *r, const AeMissbHooks *h,
                                  const char *name, int32_t x, int32_t y,
                                  int32_t width)
{
    (void)ae_missb_call(r, h, name, 0u, 3u, (uint32_t)x, (uint32_t)y,
                        (uint32_t)width, 0u);
    ae_wr32(r, AE_MISSB_UI_G_PRIMITIVE_CURSOR,
            ae_rd32(r, AE_MISSB_UI_G_PRIMITIVE_CURSOR) +
                AE_MISSB_UI_ARROW_BASE_PACKET_SIZE);
}

static void ui_render_pause_arrows(AeRam *r, const AeMissbHooks *h,
                                   int32_t x, int32_t y, int32_t bars)
{
    uint32_t packets = AE_MISSB_UI_PAUSE_ARROW_BASE_PACKETS;
    if (bars > 0) {
        packets += ((uint32_t)bars + AE_MISSB_UI_PAUSE_ARROW_BAR_STEP - 1u) /
                   AE_MISSB_UI_PAUSE_ARROW_BAR_STEP;
    }
    (void)ae_missb_call(r, h, "RendererPauseMenuDrawArrows", 0u, 3u,
                        (uint32_t)x, (uint32_t)y, (uint32_t)bars, 0u);
    ae_wr32(r, AE_MISSB_UI_G_PRIMITIVE_CURSOR,
            ae_rd32(r, AE_MISSB_UI_G_PRIMITIVE_CURSOR) +
                packets * AE_MISSB_UI_PAUSE_ARROW_BAR_PACKET_SIZE);
}

/* 80082230: sum glyph advances through NUL or carriage return. */
int32_t ae_HudTextWidth(AeRam *r, uint32_t text)
{
    uint32_t width = 0u;
    uint32_t cursor = text;
    uint32_t glyph = ae_rd8(r, cursor);

    while (glyph != 0u && glyph != 0x0Du) {
        width += ae_rd8(r, AE_MISSB_UI_TEXT_ADVANCE_TABLE + glyph);
        cursor += 1u;
        glyph = ae_rd8(r, cursor);
    }
    return (int32_t)width;
}

/* 80082398: keep the capture counts, labels, result, and time display in C;
 * the native renderer owns only the grid packet and ordering-table writes. */
void ae_PauseDrawCaptureGrid(AeRam *r, const AeMissbHooks *h)
{
    uint32_t stage = AE_MISSB_UI_G_STAGE_STATE;
    int32_t total = ae_rds16(r, stage + AE_MISSB_UI_G_CAPTURE_TOTAL);
    int32_t caught = ae_rds16(r, stage + AE_MISSB_UI_G_CAPTURE_COUNT);
    int32_t goal = ae_rds16(r, stage + AE_MISSB_UI_G_CAPTURE_GOAL);
    int32_t level = (int32_t)ae_rd16(r, stage + AE_MISSB_UI_G_STAGE_LEVEL);
    uint32_t primitive = ae_rd32(r, AE_MISSB_UI_G_PRIMITIVE_CURSOR);
    uint32_t packet_count = total > 0 ? (uint32_t)total : 0u;

    /* The loop emits one 20-byte grid cell per capture slot, followed by an
     * 8-byte OT terminator.  Keep the renderer cursor movement observable. */
    ae_wr32(r, AE_MISSB_UI_G_PRIMITIVE_CURSOR,
            primitive + packet_count * AE_MISSB_UI_CAPTURE_GRID_PACKET_SIZE +
                AE_MISSB_UI_CAPTURE_GRID_TERMINATOR_SIZE);
    (void)ae_missb_call(r, h, "RendererPauseDrawCaptureGrid", 0u, 4u,
                        (uint32_t)total, (uint32_t)caught,
                        (uint32_t)goal, (uint32_t)level);

    AeMissbHooks nested_hooks;
    const AeMissbHooks *draw_hooks = ui_caller_frame_hooks(
        &nested_hooks, h, 64u);
    ae_HudSetGeomOffsetThenDraw(r, draw_hooks, 276, 180);

    int32_t current_value;
    int32_t best_value;
    if ((uint32_t)level == AE_MISSB_UI_CAPTURE_TIME_ATTACK_LEVEL_A ||
        (uint32_t)level == AE_MISSB_UI_CAPTURE_TIME_ATTACK_LEVEL_B) {
        int32_t area = (uint32_t)level == AE_MISSB_UI_CAPTURE_TIME_ATTACK_LEVEL_A
            ? AE_MISSB_UI_CAPTURE_TIME_ATTACK_AREA_A
            : AE_MISSB_UI_CAPTURE_TIME_ATTACK_AREA_B;
        current_value = (ae_ProgressGetStageResult(r, area) &
                         AE_MISSB_UI_CAPTURE_RESULT_FLAG) != 0 ? 5 : 0;
        best_value = 5;
    } else {
        current_value = ae_LevelCaughtCount(r, level);
        best_value = ae_rds16(r, stage + AE_MISSB_UI_G_CAPTURE_BEST);
    }

    uint32_t scratch = AE_MISSB_UI_CAPTURE_SCRATCH;
    int32_t tens = current_value / 10;
    int32_t ones = current_value - tens * 10;
    ae_wr8(r, scratch, (uint8_t)(tens + 106));
    ae_wr8(r, scratch + 1u, ui_digit_code(ones));
    tens = best_value / 10;
    ones = best_value - tens * 10;
    ae_wr8(r, scratch + 2u, 141u);
    ae_wr8(r, scratch + 3u, (uint8_t)(tens + 106));
    ae_wr8(r, scratch + 4u, ui_digit_code(ones));
    ae_wr8(r, scratch + 5u, 0u);
    ui_draw_text(r, h, scratch, 241, 172, 1);

    ui_draw_text(r, h, 0x800B269Eu, 74, 180, 2);
    ui_draw_text(r, h, 0x800B26A8u, 54, 64, 2);
    ui_draw_text(r, h, 0x800B26B0u, 230, 64, 2);

    int32_t type_a = ae_rds8(r, stage + AE_MISSB_UI_CAPTURE_LEVEL_TYPE);
    int32_t type_b = ae_rds8(r, stage + AE_MISSB_UI_CAPTURE_LEVEL_TYPE + 1u);
    ae_wr8(r, scratch + 1u, 139u);
    ae_wr8(r, scratch, ae_rd8(r, AE_MISSB_UI_CAPTURE_LEVEL_TYPE_TABLE +
                              (uint32_t)type_a + 1u));
    ae_wr8(r, scratch + 2u,
           ae_rd8(r, AE_MISSB_UI_CAPTURE_LEVEL_TYPE_TABLE +
                        (uint32_t)type_b + 1u));
    ae_wr8(r, scratch + 3u, 0u);
    ui_draw_text(r, h, scratch, 124, 64, 2);

    ae_wr8(r, scratch, 10u);
    ae_wr8(r, scratch + 1u, 9u);
    ae_wr8(r, scratch + 2u, 0u);
    ae_TextSetCursor(r, 54, 180);
    if (h == NULL || h->pause_menu == NULL) {
        ae_ram_fail(r, "AeMissbHooks.pause_menu not provided for MessageBoxDrawString");
        return;
    }
    ae_MessageBoxDrawString(r, h->pause_menu, scratch);

    int32_t best_time = ae_rds32(r, stage + AE_MISSB_UI_G_BEST_TIME);
    int32_t last_time = ae_rds32(r, stage + AE_MISSB_UI_G_LAST_TIME);
    if (best_time != -1) {
        ui_draw_text(r, h, 0x800B26C4u, 214, 114, 2);
        if (h == NULL || h->hud == NULL) {
            ae_ram_fail(r, "AeMissbHooks.hud not provided for HudDrawTime");
            return;
        }
        ae_HudDrawTime(r, h->hud, 246, 140, best_time);
        uint32_t level_record = ae_LevelTableGet(r, level);
        (void)ae_missb_call(r, h, "HudDrawTimeRankIcon", 0x8005F68Cu, 4u,
                            222u, 132u, (uint32_t)best_time,
                            level_record);
    } else if (last_time != best_time) {
        ui_draw_text(r, h, 0x800B26F4u, 214, 114, 2);
        if (h == NULL || h->hud == NULL) {
            ae_ram_fail(r, "AeMissbHooks.hud not provided for HudDrawTime");
            return;
        }
        ae_HudDrawTime(r, h->hud, 246, 140, last_time);
    }

    ui_draw_text(r, h, 0x800F4458u, 54, 82, 1);
    if (h == NULL || h->hud == NULL) {
        ae_ram_fail(r, "AeMissbHooks.hud not provided for HudDrawTime");
        return;
    }
    int32_t stage_time = ae_rds32(r, stage + AE_MISSB_UI_STAGE_LEVEL_TABLE_OFFSET);
    ae_HudDrawTime(r, h->hud, 246, 90, stage_time);
    if ((uint32_t)level != AE_MISSB_UI_CAPTURE_TIME_ATTACK_LEVEL_A &&
        (uint32_t)level != AE_MISSB_UI_CAPTURE_TIME_ATTACK_LEVEL_B) {
        uint32_t level_record = ae_LevelTableGet(r, level);
        (void)ae_missb_call(r, h, "HudDrawTimeRankIcon", 0x8005F68Cu, 4u,
                            222u, 82u, (uint32_t)stage_time,
                            level_record);
    }
}

/* 800828F8: prepare the animated environment-map transform, then dispatch
 * projection and packet work to the native renderer. */
void ae_HudSetGeomOffsetThenDraw(AeRam *r, const AeMissbHooks *h,
                                 int32_t x, int32_t y)
{
    uint32_t caller_sp = h != NULL && h->stack_pointer != 0u
        ? h->stack_pointer : 0x801FFF00u;
    uint32_t frame_sp = caller_sp - AE_MISSB_UI_HUD_STACK_SIZE;
    uint32_t local_matrix = frame_sp + AE_MISSB_UI_TRANSFORM_LOCAL_OFFSET;
    uint32_t frame_angle = ae_rd32(r, AE_MISSB_UI_G_FRAME_COUNTER) << 6;

    (void)ae_missb_call(r, h, "RendererSetGeomOffset", 0u, 2u,
                        (uint32_t)x, (uint32_t)y, 0u, 0u);

    (void)ae_sdk_MatrixIdentity(r, local_matrix);
    ae_wr32(r, local_matrix + AE_MISSB_UI_MATRIX_TRANSLATION_X, 0u);
    ae_wr32(r, local_matrix + AE_MISSB_UI_MATRIX_TRANSLATION_Y, 0u);
    ae_wr32(r, local_matrix + AE_MISSB_UI_MATRIX_TRANSLATION_Z, 2048u);
    uint32_t sine = (uint32_t)ae_sdk_rsin(r, (int32_t)frame_angle);
    uint32_t cosine = (uint32_t)ae_sdk_rcos(r, (int32_t)frame_angle);
    uint32_t negative_sine = 0u - sine;
    uint32_t negative_cosine = 0u - cosine;
    uint32_t angle_x = (negative_sine >> 4) |
                       ((negative_sine & 0x80000000u) != 0u ? 0xF0000000u : 0u);
    uint32_t angle_y = (negative_cosine >> 4) |
                       ((negative_cosine & 0x80000000u) != 0u ? 0xF0000000u : 0u);
    ae_wr16(r, AE_MISSB_UI_G_ENV_ANGLE_VECTOR, angle_x);
    ae_wr16(r, AE_MISSB_UI_G_ENV_ANGLE_VECTOR + AE_MISSB_UI_SVECTOR_Y, angle_y);
    ae_wr16(r, AE_MISSB_UI_G_ENV_ANGLE_VECTOR + AE_MISSB_UI_SVECTOR_Z, 0u);
    (void)ae_sdk_RotMatrixZYX(r, AE_MISSB_UI_G_ENV_ANGLE_VECTOR, local_matrix);
    (void)ae_sdk_CompMatrix(r, AE_MISSB_UI_G_VIEW_BASE_MATRIX,
                            local_matrix, local_matrix);

    (void)ae_missb_call(r, h, "ModelTransformRigid", 0x800161C0u, 3u,
                        AE_MISSB_UI_G_ENV_MATRIX, local_matrix,
                        AE_MISSB_UI_G_ENV_ANGLE_VECTOR, 0u);
    if (h == NULL || h->misc == NULL) {
        ae_ram_fail(r, "AeMissbHooks.misc not provided for MeshCalcEnvUVs");
    } else {
        ae_MeshCalcEnvUVs(r, h->misc, AE_MISSB_UI_G_ENV_MATRIX);
    }

    ae_wr8(r, AE_MISSB_UI_G_ENV_ACTIVE, 2u);
    (void)ae_missb_call(r, h, "ModelDrawEnvMap", 0x8001CE54u, 3u,
                        ae_rd32(r, AE_MISSB_UI_G_MODEL_INSTANCE),
                        AE_MISSB_UI_G_ENV_ANGLE_VECTOR,
                        AE_MISSB_UI_G_ENV_MATRIX, 0u);
    (void)ae_missb_call(r, h, "RendererSetGeomOffset", 0u, 2u,
                        0x78u, 0xC0u, 0u, 0u);
}

/* 80082D40 */
void ae_PauseMenuInitVolumes(AeRam *r)
{
    ae_wr32(r, AE_MISSB_UI_PAUSE_VOLUME_STATE, 0u);
    (void)ae_SoundGetVolumes(r,
                             AE_MISSB_UI_PAUSE_VOLUME_STATE + AE_MISSB_UI_PAUSE_VOLUME_OUTPUT);
}

/* 800832FC: index a packed, NUL-separated string table. */
uint32_t ae_StringTableIndex_Main(AeRam *r, uint32_t strings, int32_t index)
{
    uint32_t cursor = strings;
    int32_t passed = 0;

    while (passed < index) {
        if (ae_rd8(r, cursor) == 0u) passed += 1;
        cursor += 1u;
    }
    return cursor;
}

/* 80082A04: format the pause counter and save checksum before requesting the
 * corresponding native text and window rendering. */
void ae_PauseDrawCounter(AeRam *r, const AeMissbHooks *h)
{
    uint32_t scratch = AE_MISSB_UI_CAPTURE_SCRATCH;
    AeMissbHooks nested_hooks;
    const AeMissbHooks *draw_hooks = ui_caller_frame_hooks(
        &nested_hooks, h, 32u);
    ae_HudSetGeomOffsetThenDraw(r, draw_hooks, 276, 112);
    int32_t count = ae_rds16(r, AE_PROGRESS_G_PICKUP_COUNTER);
    int32_t tens = count / 10;
    int32_t ones = count - tens * 10;
    uint32_t digits = tens == 0 ? 1u : 2u;
    ae_wr8(r, scratch, tens == 0 ? 15u : ui_digit_code(tens));
    ae_wr8(r, scratch + 1u, ui_digit_code(ones));
    if (ae_LevelProgressGet(r, 24u) > 0) {
        digits += 3u;
        ae_wr8(r, scratch + 2u, 141u);
        ae_wr8(r, scratch + 3u, 122u);
        ae_wr8(r, scratch + 4u, 106u);
        ae_wr8(r, scratch + 5u, 0u);
    } else {
        ae_wr8(r, scratch + 2u, 0u);
    }
    ui_draw_text(r, h, scratch, 277 - (int32_t)(digits << 3), 132, 1);

    if (h == NULL || h->pause_menu == NULL) {
        ae_ram_fail(r, "AeMissbHooks.pause_menu not provided for HudDrawWindowFrame");
        return;
    }
    ae_HudDrawWindowFrame(r, h->pause_menu, 30, 30, 304, 176);
    ui_draw_text(r, h, 0x800B26D0u, 80, 80, 2);

    int32_t checksum = ae_SaveBuildImage(r);
    ui_write_counter_checksum(r, scratch, checksum);
    ae_wr8(r, scratch + 5u, 188u);
    ae_wr8(r, scratch + 6u, 0u);
    ui_draw_text(r, h, scratch, 100, 100, 1);
    ui_draw_text(r, h, 0x800B269Eu, 74, 180, 2);
    ae_TextSetCursor(r, 54, 180);
    ae_wr8(r, scratch, 10u);
    ae_wr8(r, scratch + 1u, 9u);
    ae_wr8(r, scratch + 2u, 0u);
    if (h->pause_menu == NULL) {
        ae_ram_fail(r, "AeMissbHooks.pause_menu not provided for MessageBoxDrawString");
        return;
    }
    ae_MessageBoxDrawString(r, h->pause_menu, scratch);
}

/* 8008DEEC */
void ae_MessageBoxOpen(AeRam *r)
{
    ae_wr8(r, AE_MISSB_UI_G_MESSAGE_BOX + AE_MISSB_UI_MESSAGE_BOX_CLOSED, 0u);
    ae_wr8(r, AE_MISSB_UI_G_MESSAGE_BOX + AE_MISSB_UI_MESSAGE_BOX_ACTIVE, 1u);
    ae_wr16(r, AE_MISSB_UI_G_MESSAGE_BOX + AE_MISSB_UI_MESSAGE_BOX_WIDTH, 0x1F0u);
    ae_wr16(r, AE_MISSB_UI_G_MESSAGE_BOX + AE_MISSB_UI_MESSAGE_BOX_STATE, 0x80u);
    ae_wr16(r, AE_MISSB_UI_G_MESSAGE_BOX + AE_MISSB_UI_MESSAGE_BOX_STYLE, 1u);
    ae_wr16(r, AE_MISSB_UI_G_MESSAGE_BOX + AE_MISSB_UI_MESSAGE_BOX_TIMER, 0u);
}

/* 80082D6C: render the pause settings screen while preserving its selection,
 * option toggles, sound-volume changes, and renderer input values. */
int32_t ae_PauseMenuDrawSubScreen(AeRam *r, const AeMissbHooks *h)
{
    uint32_t stage = AE_MISSB_UI_G_STAGE_STATE;
    uint32_t selected = ae_rd32(r, AE_MISSB_UI_G_PAUSE_SELECTION);
    uint32_t frame = ae_rd32(r, AE_MISSB_UI_G_FRAME_COUNTER);
    int32_t phase = (int32_t)((frame >> 2) % 3u);
    int32_t packet_x = selected < 5u ? 28 : 220;
    int32_t packet_y = selected < 5u
        ? (int32_t)(selected * 26u + 28u) - phase
        : 158 - phase;
    uint32_t primitive = ae_rd32(r, AE_MISSB_UI_G_PRIMITIVE_CURSOR);
    uint32_t button_states = ae_rd32(r, AE_MISSB_UI_G_PAD_PRESSED) &
                             ae_rd32(r, AE_MISSB_UI_G_PAD_HELD);

    if (h == NULL || h->pause_menu == NULL) {
        ae_ram_fail(r, "AeMissbHooks.pause_menu not provided for pause subscreen");
        return 0;
    }
    ae_HudDrawWindowFrame(r, h->pause_menu, 30, 30, 304, 176);

    /* The native request replaces one 20-byte background and five 16-byte
     * icon packets; the original OT cursor still advances by their size. */
    ae_wr32(r, AE_MISSB_UI_G_PRIMITIVE_CURSOR,
            primitive + AE_MISSB_UI_SUBSCREEN_BASE_PACKET_SIZE +
                AE_MISSB_UI_SUBSCREEN_ICON_COUNT *
                    AE_MISSB_UI_SUBSCREEN_ICON_PACKET_SIZE);
    (void)ae_missb_call(r, h, "RendererPauseMenuDrawSubScreen", 0u, 4u,
                        selected, (uint32_t)phase, (uint32_t)packet_x,
                        (uint32_t)packet_y);

    int32_t bgm_volume = ae_rds32(r, AE_MISSB_UI_G_PAUSE_SELECTION +
                                  AE_MISSB_UI_PAUSE_BGM_VOLUME);
    int32_t sfx_volume = ae_rds32(r, AE_MISSB_UI_G_PAUSE_SELECTION +
                                  AE_MISSB_UI_PAUSE_SFX_VOLUME);
    ui_render_pause_arrows(r, h, 168, 104, bgm_volume);
    ui_render_pause_arrows(r, h, 168, 130, sfx_volume);
    ae_wr32(r, AE_MISSB_UI_G_PRIMITIVE_CURSOR,
            ae_rd32(r, AE_MISSB_UI_G_PRIMITIVE_CURSOR) + 8u);

    for (int32_t index = 0; index < 5; ++index) {
        uint32_t text = ae_StringTableIndex_Main(r,
                                                  AE_MISSB_UI_G_MENU_STRINGS,
                                                  index);
        ui_draw_text(r, h, text, 84, 52 + index * 26, 2);
    }
    ui_draw_text(r, h, ae_StringTableIndex_Main(r,
                                                AE_MISSB_UI_G_MENU_STRINGS, 5),
                 180, 52, 1);
    ui_draw_text(r, h, ae_StringTableIndex_Main(r,
                                                AE_MISSB_UI_G_MENU_STRINGS, 6),
                 240, 52, 1);

    int32_t setting_a = ae_rd8(r, stage + AE_MISSB_UI_PAUSE_SETTING_A);
    ui_render_menu_arrows(r, h, "RendererDrawMenuArrows",
                          setting_a != 0 ? 196 : 256, 52, 64);
    ui_draw_text(r, h, ae_StringTableIndex_Main(r,
                                                AE_MISSB_UI_G_MENU_STRINGS, 7),
                 180, 78, 1);
    ui_draw_text(r, h, ae_StringTableIndex_Main(r,
                                                AE_MISSB_UI_G_MENU_STRINGS, 8),
                 260, 78, 1);
    int32_t setting_b = (int32_t)ae_rd16(r,
                                         stage + AE_MISSB_UI_PAUSE_SETTING_B);
    ui_render_menu_arrows(r, h, "RendererDrawMenuArrows",
                          setting_b != 0 ? 212 : 292, 78, 88);
    ui_draw_text(r, h, ae_StringTableIndex_Main(r,
                                                AE_MISSB_UI_G_MENU_STRINGS, 9),
                 260, 182, 0);
    ae_TextSetCursor(r, 228, 182);
    ae_MessageBoxDrawString(r, h->pause_menu,
                            ae_StringTableIndex_Main(r,
                                AE_MISSB_UI_G_MENU_STRINGS, 11));

    uint32_t pressed = button_states;
    if (selected < 5u) {
        switch (selected) {
        case 0u:
            if ((pressed & 0xA0F0u) != 0u) {
                uint8_t value = (uint8_t)(ae_rd8(r, stage +
                    AE_MISSB_UI_PAUSE_SETTING_A) ^ 1u);
                ae_wr8(r, stage + AE_MISSB_UI_PAUSE_SETTING_A, value);
                if (h->sound_hw == NULL) {
                    ae_ram_fail(r, "AeMissbHooks.sound_hw not provided for PlaySfx");
                    return 0;
                }
                (void)ae_PlaySfx(r, h->sound_hw, 3);
            }
            break;
        case 1u:
            if ((pressed & 0xA0F0u) != 0u) {
                uint16_t value = (uint16_t)(ae_rd16(r, stage +
                    AE_MISSB_UI_PAUSE_SETTING_B) ^ 1u);
                ae_wr16(r, stage + AE_MISSB_UI_PAUSE_SETTING_B, value);
                if (h->sound_hw == NULL) {
                    ae_ram_fail(r, "AeMissbHooks.sound_hw not provided for PlaySfx");
                    return 0;
                }
                (void)ae_PlaySfx(r, h->sound_hw, 3);
            }
            break;
        case 2u:
        case 3u: {
            uint32_t volume_offset = selected == 2u
                ? AE_MISSB_UI_PAUSE_BGM_VOLUME
                : AE_MISSB_UI_PAUSE_SFX_VOLUME;
            int32_t volume = ae_rds32(r, AE_MISSB_UI_G_PAUSE_SELECTION +
                                      volume_offset);
            if ((pressed & 0x2000u) != 0u) volume += 8;
            if ((pressed & 0x8000u) != 0u) volume -= 8;
            if ((pressed & 0xA000u) != 0u) {
                if (h->sound_hw == NULL) {
                    ae_ram_fail(r, "AeMissbHooks.sound_hw not provided for PlaySfx");
                    return 0;
                }
                (void)ae_PlaySfx(r, h->sound_hw, 16);
            }
            if (volume < 0) volume = 0;
            if (volume >= 128) volume = 127;
            ae_wr32(r, AE_MISSB_UI_G_PAUSE_SELECTION + volume_offset,
                    (uint32_t)volume);
            if (h->progress == NULL) {
                ae_ram_fail(r, "AeMissbHooks.progress not provided for SoundSetVolumes");
                return 0;
            }
            if (selected == 2u) {
                (void)ae_SoundSetVolumes(r, h->progress,
                    ae_rds32(r, AE_MISSB_UI_G_PAUSE_SELECTION +
                                 AE_MISSB_UI_PAUSE_SFX_VOLUME), volume);
            } else {
                (void)ae_SoundSetVolumes(r, h->progress, volume,
                    ae_rds32(r, AE_MISSB_UI_G_PAUSE_SELECTION +
                                 AE_MISSB_UI_PAUSE_BGM_VOLUME));
            }
            break;
        }
        case 4u:
            if ((pressed & 0x00F0u) != 0u) {
                if (h->progress == NULL) {
                    ae_ram_fail(r, "AeMissbHooks.progress not provided for SoundVolumesDefault");
                    return 0;
                }
                (void)ae_SoundVolumesDefault(r, h->progress, 1);
                (void)ae_SoundGetVolumes(r, AE_MISSB_UI_G_PAUSE_SELECTION +
                                            AE_MISSB_UI_PAUSE_SFX_VOLUME);
                ae_wr8(r, stage + AE_MISSB_UI_PAUSE_SETTING_A, 1u);
                ae_wr16(r, stage + AE_MISSB_UI_PAUSE_SETTING_B, 1u);
                (void)ae_SoundSetStereo(r, 1);
                if (h->sound_hw == NULL) {
                    ae_ram_fail(r, "AeMissbHooks.sound_hw not provided for PlaySfx");
                    return 0;
                }
                (void)ae_PlaySfx(r, h->sound_hw, 3);
            }
            break;
        default:
            break;
        }
    }

    int32_t next_selection = (int32_t)selected;
    if ((pressed & 0x1000u) != 0u) {
        --next_selection;
        if (h->sound_hw == NULL) {
            ae_ram_fail(r, "AeMissbHooks.sound_hw not provided for PlaySfx");
            return 0;
        }
        (void)ae_PlaySfx(r, h->sound_hw, 0);
    }
    if ((pressed & 0x4000u) != 0u) {
        if (h->sound_hw == NULL) {
            ae_ram_fail(r, "AeMissbHooks.sound_hw not provided for PlaySfx");
            return 0;
        }
        (void)ae_PlaySfx(r, h->sound_hw, 0);
        ++next_selection;
    }
    next_selection %= 5;
    if (next_selection < 0) next_selection += 5;

    if ((pressed & 0x0800u) != 0u) {
        if (h->sound_hw == NULL) {
            ae_ram_fail(r, "AeMissbHooks.sound_hw not provided for PlaySfx");
            return 0;
        }
        (void)ae_PlaySfx(r, h->sound_hw, 1);
        return -1;
    }
    ae_PauseMenuDrawButtons(r, h->pause_menu);
    ae_wr32(r, AE_MISSB_UI_G_PAUSE_SELECTION, (uint32_t)next_selection);
    return 0;
}
