#include "ae_ovl186a.h"

#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "../math/native_sdk_apply.h"

#include <stdint.h>

#define AE_OVL186A_API_RESET_GRAPH 0x80028350u
#define AE_OVL186A_API_CLEAR_IMAGE 0x80028850u
#define AE_OVL186A_API_DRAW_SYNC 0x800286CCu
#define AE_OVL186A_API_DRAW_OTAG 0x80028CC0u
#define AE_OVL186A_API_VSYNC 0x8002E520u
#define AE_OVL186A_API_BEGIN_FRAME 0x8005DF48u
#define AE_OVL186A_API_INIT_DISPLAY 0x8005CFECu
#define AE_OVL186A_API_PUT_DRAW_ENV 0x80028D30u
#define AE_OVL186A_API_PUT_DISPLAY_ENV 0x80028EFCu
#define AE_OVL186A_API_SET_GEOM_SCREEN 0x8002FAF0u
#define AE_OVL186A_API_MEMCARD_OP 0x80010000u
#define AE_OVL186A_API_MEMCARD_CHECK_FILE 0x8001078Cu
#define AE_OVL186A_API_PAD_SAMPLE_HISTORY 0x8001300Cu
#define AE_OVL186A_API_PAD_UPDATE 0x8001325Cu
#define AE_OVL186A_API_SOUND_DELAYED_RESET 0x8009BA90u
#define AE_OVL186A_API_SOUND_LOOP_STOP 0x80088578u
#define AE_OVL186A_API_HEAP_INIT 0x8008C01Cu
#define AE_OVL186A_API_HEAP_ALLOC 0x8008C09Cu
#define AE_OVL186A_API_MESH_INIT 0x80015C34u
#define AE_OVL186A_API_SET_COLOR_MATRIX 0x80015D58u
#define AE_OVL186A_API_APPLY_LEVEL_FOG 0x800323F8u
#define AE_OVL186A_API_MATRIX_IDENTITY 0x8004637Cu
#define AE_OVL186A_API_DELAYED_QUEUE_RESET 0x80088578u
#define AE_OVL186A_API_VSYNC_COUNTER 0x8002E520u
#define AE_OVL186A_API_SOUND_STOP_RESTORE 0x80086170u
#define AE_OVL186A_API_SOUND_STOP_ALL 0x80086FD0u
#define AE_OVL186A_API_PAD_SETUP_DUALSHOCK 0x80012CA0u
#define AE_OVL186A_API_STICK_TO_BUTTONS 0x8002B4A0u
#define AE_OVL186A_API_PAD_RUMBLE_RESET 0x80014208u
#define AE_OVL186A_API_PAD_GET_STATE 0x8002B4C0u
#define AE_OVL186A_API_PAD_DEMO_STEP 0x80013128u
#define AE_OVL186A_API_LOAD_LIST_STEP 0x800116C0u
#define AE_OVL186A_API_ARC_LOAD_ENTRY 0x80011194u
#define AE_OVL186A_API_CD_MANAGER_UPDATE 0x800125B8u
#define AE_OVL186A_API_SOUND_PLAY 0x800877F8u
#define AE_OVL186A_API_SAVE_RESTORE_STATE 0x80095E80u
#define AE_OVL186A_API_LEVEL_PROGRESS_GET 0x80096580u
#define AE_OVL186A_API_PROGRESS_CHECKSUM 0x800962B0u
#define AE_OVL186A_API_SAVE_SCORE 0x8009816Cu

#define AE_OVL186A_G_FRAME_DISPLAY_ENV 0x800E38B0u
#define AE_OVL186A_G_STAGE_AREA_DATA 0x80154000u
#define AE_OVL186A_G_STAGE_MESH_LIST 0x80154004u
#define AE_OVL186A_G_STAGE_MESH_COUNT 0x80154000u
#define AE_OVL186A_G_STAGE_FOG 0x800EBFA0u
#define AE_OVL186A_G_MUSIC_STATE 0x800E3880u
#define AE_OVL186A_G_SAVE_SLOT_INDEX 0x800E3886u
#define AE_OVL186A_G_SAVE_SLOT_COUNT 0x800E3884u
#define AE_OVL186A_G_FONT_TABLE 0x8013B018u
#define AE_OVL186A_G_VIEW_BASE_MATRIX 0x800AF0A0u
#define AE_OVL186A_G_MENU_STATE_WORD 0x8013AF56u
#define AE_OVL186A_G_CURRENT_MODE 0x800F4470u
#define AE_OVL186A_G_DISPLAY_TEMPLATE 0x8013A9CCu
#define AE_OVL186A_G_ARCHIVE_LIST_NO_MUSIC 0x8013AF7Cu
#define AE_OVL186A_G_ARCHIVE_LIST_WITH_MUSIC 0x8013AF84u
#define AE_OVL186A_G_PAD_STATE_BASE 0x80140000u
#define AE_OVL186A_G_FILE_ENTRIES 0x8013B218u
#define AE_OVL186A_G_SAVE_FILE_IMAGES 0x800E1A00u
#define AE_OVL186A_G_CARD_MENU_MODE 0x800E3884u
#define AE_OVL186A_G_CARD_MENU_SLOT 0x800E3886u
#define AE_OVL186A_G_CARD_DIRTY_FLAGS 0x8013AF64u
#define AE_OVL186A_G_CARD_CALLBACK_MODE 0x8013AF6Cu
#define AE_OVL186A_G_CARD_CALLBACK_TIMER 0x8013AF5Cu
#define AE_OVL186A_G_CARD_CALLBACK_FLAG 0x8013AF64u
#define AE_OVL186A_G_TEXT_COLOR_BUFFER 0x80141554u
#define AE_OVL186A_G_MENU_STRING_BUFFER 0x80141800u
#define AE_OVL186A_G_PAD_REPEAT 0x800E5488u
#define AE_OVL186A_G_STAGE_PROGRESS 0x800DFC70u
#define AE_OVL186A_G_OVERLAY_SLOT_INFO 0x800E1880u
#define AE_OVL186A_G_SLOT_SOURCE_TABLE 0x800E1A00u
#define AE_OVL186A_G_STAGE_FILE_LIST 0x800E1A00u
#define AE_OVL186A_G_SLOT_MENUS 0x8013AFE8u
#define AE_OVL186A_G_MODE_HANDLER_TABLE 0x8013A9D8u
#define AE_OVL186A_G_SET_STATE_TABLE 0x8013AA68u
#define AE_OVL186A_G_STATE_HANDLER_TABLE 0x8013AB20u
#define AE_OVL186A_G_CARD_HANDLER_TABLE 0x8013ACB8u
#define AE_OVL186A_G_CARD_HANDLER_TABLE_1 0x8013ACE0u
#define AE_OVL186A_G_CARD_HANDLER_TABLE_2 0x8013AD08u
#define AE_OVL186A_G_CARD_HANDLER_TABLE_3 0x8013AD30u
#define AE_OVL186A_G_CARD_HANDLER_TABLE_4 0x8013AD58u
#define AE_OVL186A_G_CARD_HANDLER_TABLE_5 0x8013AD80u
#define AE_OVL186A_G_SAVE_SLOT_RECORDS 0x800E1880u
#define AE_OVL186A_G_SLOT_VIEW_MODE 0x8013AF5Eu
#define AE_OVL186A_G_SAVE_MONKEY_VALUES 0x800E1A00u
#define AE_OVL186A_G_SAVE_PROGRESS_TABLE 0x800E1A00u
#define AE_OVL186A_TEMP_FORMAT_0 0x801FFB00u
#define AE_OVL186A_TEMP_FORMAT_1 0x801FFB10u
#define AE_OVL186A_TEMP_FORMAT_2 0x801FFB20u

static void ae_ovl186a_memcard_op(AeRam *ram, const AeOverlayHooks *hooks,
                                  uint32_t operation);

enum AeOvl186aFrameField {
    AE_OVL186A_FRAME_COUNTER = 0x00,
    AE_OVL186A_FRAME_CURSOR = 0x04,
    AE_OVL186A_FRAME_DISPLAY_INDEX = 0x0C,
    AE_OVL186A_FRAME_DRAW_ENV = 0x10,
    AE_OVL186A_FRAME_DISPLAY_ENV = 0x6C,
    AE_OVL186A_FRAME_OTAG = 0x80,
    AE_OVL186A_FRAME_DISPLAY_FLAGS = 0x28,
    AE_OVL186A_FRAME_PACKET_FLAGS = 0x40A0,
    AE_OVL186A_FRAME_DISPLAY_STRIDE = 16504
};

enum AeOvl186aFogField {
    AE_OVL186A_FOG_START = 28,
    AE_OVL186A_FOG_COLOR_R = 36,
    AE_OVL186A_FOG_COLOR_G = 40,
    AE_OVL186A_FOG_COLOR_B = 44
};

enum AeOvl186aSaveSlotField {
    AE_OVL186A_SAVE_SLOT_IMAGE_OFFSET = 0x180,
    AE_OVL186A_SAVE_SLOT_STATUS_OFFSET = 0x1C0,
    AE_OVL186A_SAVE_SLOT_MONKEYS_OFFSET = 0x1C8,
    AE_OVL186A_SAVE_SLOT_LIST_OFFSET = 0x1E8,
    AE_OVL186A_SAVE_SLOT_SCORE_OFFSET = 0x180,
    AE_OVL186A_SAVE_SLOT_FLAGS_OFFSET = 0x7E8,
    AE_OVL186A_SAVE_SLOT_STRIDE = 0x7A0,
    AE_OVL186A_SAVE_SLOT_IMAGE_STRIDE = 0x7A0,
    AE_OVL186A_SAVE_SLOT_COUNT = 4,
    AE_OVL186A_SAVE_SLOT_MONKEY_COUNT = 25
};

enum AeOvl186aFileEntryField {
    AE_OVL186A_FILE_ENTRY_STRIDE = 120,
    AE_OVL186A_FILE_ENTRY_MONKEYS = 0x00,
    AE_OVL186A_FILE_ENTRY_PROGRESS = 0x02,
    AE_OVL186A_FILE_ENTRY_SCORE_A = 0x04,
    AE_OVL186A_FILE_ENTRY_SCORE_B = 0x06,
    AE_OVL186A_FILE_ENTRY_LAST_USED = 0x0A,
    AE_OVL186A_FILE_ENTRY_TEXT_COLOR = 0x13,
    AE_OVL186A_FILE_ENTRY_DETAIL_COLOR = 0x14,
    AE_OVL186A_FILE_ENTRY_SCORE_TEXT = 0x70,
    AE_OVL186A_FILE_ENTRY_TEXT_MONKEYS = 0x50,
    AE_OVL186A_FILE_ENTRY_TEXT_PROGRESS = 0x58,
    AE_OVL186A_FILE_ENTRY_TEXT_SCORE_A = 0x60,
    AE_OVL186A_FILE_ENTRY_TEXT_SCORE_B = 0x68,
    AE_OVL186A_FILE_ENTRY_DETAIL_MONKEYS = 0x10,
    AE_OVL186A_FILE_ENTRY_DETAIL_PROGRESS_TERM = 0x17,
    AE_OVL186A_FILE_ENTRY_DETAIL_PROGRESS = 0x20,
    AE_OVL186A_FILE_ENTRY_NAME_LINE_1 = 0x10,
    AE_OVL186A_FILE_ENTRY_NAME_LINE_2 = 0x20,
    AE_OVL186A_FILE_ENTRY_DETAIL_SIZE = 0x40,
    AE_OVL186A_FILE_ENTRY_SECOND_TEXT_COLOR_2 = 0x40,
    AE_OVL186A_FILE_ENTRY_DETAIL_VALUE = 0x30,
    AE_OVL186A_FILE_ENTRY_DETAIL_LABEL = 0x40,
    AE_OVL186A_FILE_ENTRY_NAME_SCORE_TEXT = 0x70,
    AE_OVL186A_FILE_ENTRY_COLOR_SOURCE = 0x60,
    AE_OVL186A_FILE_ENTRY_COLOR_COUNT = 3,
    AE_OVL186A_FILE_ENTRY_SCORE_RESULT = 0x08,
    AE_OVL186A_FILE_ENTRY_SAVE_FLAGS = 0x0C,
    AE_OVL186A_FILE_ENTRY_SECOND_TEXT_COLOR = 0x23,
    AE_OVL186A_FILE_ENTRY_SECOND_DETAIL_COLOR = 0x24,
    AE_OVL186A_FILE_ENTRY_SECOND_COLOR_SOURCE = 0x68,
    AE_OVL186A_FILE_ENTRY_UNUSED_TEXT = 0x27,
    AE_OVL186A_FILE_ENTRY_BONUS_TEXT_COLOR = 0x30,
    AE_OVL186A_FILE_ENTRY_BONUS_COUNT_TEXT = 0x31,
    AE_OVL186A_FILE_ENTRY_BONUS_REMAINDER_TEXT = 0x41,
    AE_OVL186A_SAVE_SLOT_BONUS_COUNTS = 0x182,
    AE_OVL186A_SAVE_SLOT_FLAGS_WORD = 0x184,
    AE_OVL186A_SAVE_SLOT_NAME = 0x198,
    AE_OVL186A_DRAW_PACKET_HEADER_SIZE = 8,
    AE_OVL186A_DRAW_PACKET_SLOT_SIZE = 40,
    AE_OVL186A_DRAW_MENU_SLOT_COUNT = 4,
    AE_OVL186A_DRAW_DETAIL_ITEM_COUNT = 15
};

#define AE_OVL186A_AREA_MESH_COUNT 0x00u
#define AE_OVL186A_AREA_MESH_RECORDS 0x04u
#define AE_OVL186A_AREA_MESH_RECORD_SIZE 0x04u
#define AE_OVL186A_AREA_MESH_MODEL_INDEX 0x00u
#define AE_OVL186A_AREA_MESH_MODE 0x02u

static int32_t ae_ovl186a_call(AeRam *ram, const AeOverlayHooks *hooks,
                                const char *name, uint32_t address,
                                uint32_t count, uint32_t a0, uint32_t a1,
                                uint32_t a2, uint32_t a3, uint32_t a4)
{
    const uint32_t args[5] = {a0, a1, a2, a3, a4};
    return ae_overlay_call(ram, hooks, name, address, count, args);
}

static int32_t ae_ovl186a_later(AeRam *ram, const AeOverlayHooks *hooks,
                                const char *name, uint32_t address,
                                uint32_t count, uint32_t a0, uint32_t a1,
                                uint32_t a2, uint32_t a3)
{
    const uint32_t args[4] = {a0, a1, a2, a3};
    return ae_overlay_native_call(ram, hooks, name, address, count, args);
}

static int32_t ae_ovl186a_render(AeRam *ram, const AeOverlayHooks *hooks,
                                 const char *name, uint32_t count,
                                 uint32_t a0, uint32_t a1, uint32_t a2,
                                 uint32_t a3, uint32_t a4)
{
    const uint32_t values[5] = {a0, a1, a2, a3, a4};
    return ae_overlay_render(ram, hooks, name, count, values);
}

static int32_t ae_ovl186a_sign16(uint32_t value)
{
    return (value & 0x8000u) != 0u ? (int32_t)(value & 0xFFFFu) - 0x10000
                                    : (int32_t)(value & 0xFFFFu);
}

static int32_t ae_ovl186a_asr32(int32_t value, unsigned shift)
{
    uint32_t bits = (uint32_t)value;
    if (shift == 0u) return value;
    if (value >= 0) return (int32_t)(bits >> shift);
    return (int32_t)~((~bits) >> shift);
}

static uint32_t ae_ovl186a_wrap_add(uint32_t left, uint32_t right)
{
    return left + right;
}

int32_t ae_SaveUi_DrawText(AeRam *ram, const AeOverlayHooks *hooks,
                           uint32_t text, int32_t x, int32_t y,
                           int32_t texture_page_row)
{
    if (x < 0) {
        int32_t width = ae_SaveUi_TextWidth(ram, text);
        int32_t half_width = width / 2;
        x = 192 - half_width;
    }

    uint32_t cursor = ae_rd32(ram, AE_OVL186A_G_PRIMITIVE_CURSOR);
    uint32_t glyph = ae_rd8(ram, text);
    while (glyph != 0u) {
        (void)ae_ovl186a_render(ram, hooks, "SaveUi_DrawGlyph", 4u,
                                glyph, (uint32_t)ae_ovl186a_sign16((uint32_t)x),
                                (uint32_t)ae_ovl186a_sign16((uint32_t)y),
                                (uint32_t)texture_page_row, 0u);
        cursor = ae_ovl186a_wrap_add(cursor, 24u);
        x = (int32_t)ae_ovl186a_wrap_add((uint32_t)x,
                                        ae_rd8(ram, AE_OVL186A_G_TEXT_WIDTHS + glyph));
        text = ae_ovl186a_wrap_add(text, 1u);
        glyph = ae_rd8(ram, text);
    }
    ae_wr32(ram, AE_OVL186A_G_PRIMITIVE_CURSOR, cursor);
    return (int32_t)0x800F0000u;
}

int32_t ae_SaveUi_TextWidth(AeRam *ram, uint32_t text)
{
    int32_t width = 0;
    uint32_t character = ae_rd8(ram, text);
    while (character != 0u) {
        width = (int32_t)ae_ovl186a_wrap_add((uint32_t)width,
                                             ae_rd8(ram, AE_OVL186A_G_TEXT_WIDTHS + character));
        text = ae_ovl186a_wrap_add(text, 1u);
        character = ae_rd8(ram, text);
    }
    return width;
}

void ae_SaveUi_LookAt(AeRam *ram, uint32_t eye, uint32_t target)
{
    int32_t eye_x = ae_rds16(ram, eye + 0u);
    int32_t eye_y = ae_rds16(ram, eye + 2u);
    int32_t eye_z = ae_rds16(ram, eye + 4u);
    int32_t target_x = ae_rds16(ram, target + 0u);
    int32_t target_y = ae_rds16(ram, target + 2u);
    int32_t target_z = ae_rds16(ram, target + 4u);
    uint32_t view = AE_OVL186A_G_VIEW_MATRIX;

    ae_wr32(ram, view + 20u, (uint32_t)(-eye_x));
    ae_wr32(ram, view + 24u, (uint32_t)(-eye_y));
    ae_wr32(ram, view + 28u, (uint32_t)(-eye_z));

    int32_t horizontal[3] = {
        target_x - eye_x,
        0,
        target_z - eye_z
    };
    if (horizontal[0] == 0 && horizontal[2] == 0) horizontal[2] = 4096;
    int32_t first_normal[3];
    (void)ae_sdk_normalize_core(ram, horizontal[0], horizontal[1], horizontal[2], first_normal);

    int32_t direction[3] = {
        target_x - eye_x,
        target_y - eye_y,
        horizontal[2]
    };
    int32_t normal[3];
    (void)ae_sdk_normalize_core(ram, direction[0], direction[1], direction[2], normal);

    int32_t cross_x = -first_normal[0];
    int32_t cross_z = first_normal[2];
    int32_t radius_squared = (int32_t)((uint32_t)(normal[0] * normal[0]) +
                                       (uint32_t)(normal[2] * normal[2]));
    int32_t radius = ae_sdk_SquareRoot0(ram, radius_squared);

    ae_wr16(ram, view + 0u, (uint32_t)cross_z);
    ae_wr16(ram, view + 2u, 0u);
    ae_wr16(ram, view + 4u, (uint32_t)cross_x);
    ae_wr16(ram, view + 6u,
            (uint32_t)ae_ovl186a_asr32((int32_t)((uint32_t)cross_x * (uint32_t)normal[1]), 12u));
    ae_wr16(ram, view + 8u, (uint32_t)radius);
    ae_wr16(ram, view + 10u,
            (uint32_t)ae_ovl186a_asr32((int32_t)(0u - (uint32_t)((uint32_t)normal[1] * (uint32_t)cross_z)), 12u));
    ae_wr16(ram, view + 12u,
            (uint32_t)ae_ovl186a_asr32((int32_t)(0u - (uint32_t)((uint32_t)radius * (uint32_t)cross_x)), 12u));
    ae_wr16(ram, view + 14u, (uint32_t)normal[1]);
    ae_wr16(ram, view + 16u,
            (uint32_t)ae_ovl186a_asr32((int32_t)((uint32_t)radius * (uint32_t)cross_z), 12u));

    uint8_t *view_bytes = ae_ptr(ram, view, 32u);
    uint8_t *view_translation = ae_ptr(ram, view + 20u, 16u);
    if (view_bytes != NULL && view_translation != NULL)
        (void)ae_sdk_apply_matrix_lv(view_bytes, view_translation,
                                    view_translation);
    (void)ae_sdk_CompMatrix(ram, AE_OVL186A_G_VIEW_BASE_MATRIX, view, view);
}

uint32_t ae_SaveUi_StringTableIndex(AeRam *ram, uint32_t strings,
                                    int32_t index)
{
    uint32_t found = 0u;
    if (index > 0) {
        while ((int32_t)found < index) {
            if (ae_rd8(ram, strings) == 0u) ++found;
            strings = ae_ovl186a_wrap_add(strings, 1u);
        }
    }
    return strings;
}

int32_t ae_SaveUi_TotalMonkeyCount(AeRam *ram)
{
    int32_t total = 0;
    for (uint32_t level = 0u; level < 30u; ++level) {
        uint32_t entry = AE_OVL186A_G_LEVEL_TABLE + level * 16u;
        total = (int32_t)ae_ovl186a_wrap_add((uint32_t)total,
                                             ae_rd8(ram, entry + 1u));
    }
    return total;
}

int32_t ae_SaveUi_CountEntries(AeRam *ram, uint32_t list, uint32_t flags)
{
    int32_t count = 0;
    for (uint32_t i = 0u; i < 128u; ++i) {
        if (ae_rd8(ram, list + i * 2u) == 0xFFu) break;
        ++count;
    }
    if ((flags & 0x10u) != 0u) count += 5;
    if ((flags & 0x1000u) != 0u) count += 5;
    return count;
}

int32_t ae_SaveUi_FormatNumber3(AeRam *ram, int32_t value,
                                uint32_t output)
{
    int32_t number = ae_ovl186a_sign16((uint32_t)value);
    int32_t hundreds = number / 100;
    int32_t remainder = number - hundreds * 100;
    uint8_t hundreds_char = ae_rd8(ram, AE_OVL186A_G_DIGIT_TABLE + (uint32_t)hundreds);
    ae_wr8(ram, output, hundreds_char == (uint8_t)'j' ? 0xEFu : hundreds_char);

    int32_t tens = remainder / 10;
    int32_t ones = remainder - tens * 10;
    uint8_t tens_char = ae_rd8(ram, AE_OVL186A_G_DIGIT_TABLE + (uint32_t)tens);
    if (ae_rd8(ram, output) == 0xEFu && tens_char == (uint8_t)'j')
        tens_char = 0xEFu;
    ae_wr8(ram, output + 1u, tens_char);
    ae_wr8(ram, output + 2u,
           ae_rd8(ram, AE_OVL186A_G_DIGIT_TABLE + (uint32_t)ones));
    ae_wr8(ram, output + 3u, 0u);
    return 0;
}

int32_t ae_SaveUi_StateTick(AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t state_result = ae_SaveUi_StateMachine(ram, hooks);
    return state_result == 0 ? 0 : state_result;
}

int32_t ae_SaveUi_StateMachine(AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t state = ae_rds16(ram, AE_OVL186A_G_MENU_STATE);
    int32_t card_mode = ae_rds16(ram, AE_OVL186A_G_MEMCARD_STATE +
                                      AE_OVL186A_CARD_BUSY);
    int32_t card_result = ae_rds16(ram, AE_OVL186A_G_MEMCARD_STATE +
                                        AE_OVL186A_CARD_RESULT);
    int32_t action;

    switch (state) {
    case 0:
        if (card_mode == 1) {
            (void)ae_ovl186a_later(ram, hooks, "SaveUi_CardReady",
                                   0x8013FEFCu, 1u, 1u, 0u, 0u, 0u);
        }
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawMainWindow",
                               0x80139A3Cu, 1u, 4u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawBackground3D",
                               0x80139A68u, 0u, 0u, 0u, 0u, 0u);
        return 0;
    case 1:
        ae_SaveUi_DrawFileSelect(ram, hooks);
        action = ae_ovl186a_later(ram, hooks, "SaveUi_YesNoUpdate",
                                  0x80139FC8u, 0u, 0u, 0u, 0u, 0u);
        return action < 0 ? action : 0;
    case 2:
        if (card_mode == 4) {
            (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawStateOverlay",
                                   0x8014016Cu, 0u, 0u, 0u, 0u, 0u);
        } else {
            (void)ae_ovl186a_later(ram, hooks, "SaveUi_CardReady",
                                   0x8013FEFCu, 1u, 1u, 0u, 0u, 0u);
        }
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawMainWindow",
                               0x80139A3Cu, 1u, 4u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawBackground3D",
                               0x80139A68u, 0u, 0u, 0u, 0u, 0u);
        if (card_mode == 4 &&
            ae_ovl186a_later(ram, hooks, "SaveUi_ConfirmPressed",
                             0x8013A400u, 0u, 0u, 0u, 0u, 0u) != 0) {
            (void)ae_ovl186a_call(ram, hooks, "PlaySfx",
                                  AE_OVL186A_API_SOUND_PLAY, 1u,
                                  3u, 0u, 0u, 0u, 0u);
            return -4;
        }
        return 0;
    case 6:
    case 8:
    case 10:
    case 12:
    case 17:
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawStateOverlay",
                               0x8014016Cu, 0u, 0u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawMainWindow",
                               0x80139A3Cu, 1u, 4u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawBackground3D",
                               0x80139A68u, 0u, 0u, 0u, 0u, 0u);
        return 0;
    case 4:
        if (card_mode != 4 && card_result != 1) {
            (void)ae_ovl186a_later(ram, hooks, "SaveUi_CardReady",
                                   0x8013FEFCu, 1u, 1u, 0u, 0u, 0u);
            action = 0;
        } else {
            (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawStateOverlay",
                                   0x8014016Cu, 0u, 0u, 0u, 0u, 0u);
            action = ae_ovl186a_later(ram, hooks, "SaveUi_YesNoUpdate",
                                      0x80139F08u, 0u, 0u, 0u, 0u, 0u);
        }
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawMainWindow",
                               0x80139A3Cu, 1u, 4u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawBackground3D",
                               0x80139A68u, 0u, 0u, 0u, 0u, 0u);
        if (action == 2) {
            (void)ae_SaveUi_SetState(ram, hooks, 1, 2);
            ae_ovl186a_memcard_op(ram, hooks, 7u);
        } else if (action != 0) {
            (void)ae_SaveUi_SetState(ram, hooks, 10, 0);
        }
        return 0;
    case 5:
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawStateOverlay",
                               0x8014016Cu, 0u, 0u, 0u, 0u, 0u);
        action = ae_ovl186a_later(ram, hooks, "SaveUi_YesNoUpdate",
                                  0x80139F08u, 0u, 0u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawMainWindow",
                               0x80139A3Cu, 1u, 4u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawBackground3D",
                               0x80139A68u, 0u, 0u, 0u, 0u, 0u);
        if (action == 0) return 0;
        if (action == 1) {
            (void)ae_ovl186a_call(ram, hooks, "SaveRestoreState",
                                  AE_OVL186A_API_SAVE_RESTORE_STATE, 1u,
                                  1u, 0u, 0u, 0u, 0u);
            return -1;
        }
        (void)ae_SaveUi_SetState(ram, hooks, 1, 5);
        return 0;
    case 7:
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawStateOverlay",
                               0x8014016Cu, 0u, 0u, 0u, 0u, 0u);
        action = ae_ovl186a_later(ram, hooks, "SaveUi_YesNoUpdate",
                                  0x80139F08u, 0u, 0u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawMainWindow",
                               0x80139A3Cu, 1u, 4u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawBackground3D",
                               0x80139A68u, 0u, 0u, 0u, 0u, 0u);
        if (action == 0) return 0;
        if (action == 1) {
            int32_t slot = ae_rds16(ram, AE_OVL186A_G_CARD_MENU_SLOT);
            uint32_t save = AE_OVL186A_G_SAVE_SLOT_RECORDS +
                            (uint32_t)(slot * AE_OVL186A_SAVE_SLOT_STRIDE);
            (void)ae_SaveUi_SetState(ram, hooks, 12, 0);
            ae_wr16(ram, save + AE_OVL186A_SAVE_SLOT_IMAGE_OFFSET, 0xFFFFu);
            ae_wr8(ram, save + AE_OVL186A_SAVE_SLOT_STATUS_OFFSET, 0u);
            ae_ovl186a_memcard_op(ram, hooks, 2u);
        } else {
            (void)ae_SaveUi_SetState(ram, hooks, 1, 5);
        }
        return 0;
    case 9:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
        if (card_mode == 4 || card_result == 1) {
            (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawStateOverlay",
                                   0x8014016Cu, 0u, 0u, 0u, 0u, 0u);
            action = ae_ovl186a_later(ram, hooks, "SaveUi_ConfirmPressed",
                                      0x8013A400u, 0u, 0u, 0u, 0u, 0u);
        } else {
            (void)ae_ovl186a_later(ram, hooks, "SaveUi_CardReady",
                                   0x8013FEFCu, 1u, 1u, 0u, 0u, 0u);
            action = 0;
        }
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawMainWindow",
                               0x80139A3Cu, 1u, 4u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawBackground3D",
                               0x80139A68u, 0u, 0u, 0u, 0u, 0u);
        if (action == 0) return 0;
        (void)ae_ovl186a_call(ram, hooks, "PlaySfx",
                              AE_OVL186A_API_SOUND_PLAY, 1u,
                              3u, 0u, 0u, 0u, 0u);
        if (state == 41) {
            (void)ae_ovl186a_call(ram, hooks, "PlaySfx",
                                  AE_OVL186A_API_SOUND_PLAY, 1u,
                                  3u, 0u, 0u, 0u, 0u);
            return -4;
        }
        (void)ae_SaveUi_SetState(ram, hooks, 1, 2);
        ae_ovl186a_memcard_op(ram, hooks, 7u);
        return 0;
    case 11:
        if (ae_rds16(ram, AE_OVL186A_G_MODE_ARGUMENT) == 35) {
            (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawMainWindow",
                                   0x80139A3Cu, 1u, 4u, 0u, 0u, 0u);
            (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawBackground3D",
                                   0x80139A68u, 0u, 0u, 0u, 0u, 0u);
            return -5;
        }
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawStateOverlay",
                               0x8014016Cu, 0u, 0u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawDetail",
                               0x801391B0u, 0u, 0u, 0u, 0u, 0u);
        action = ae_ovl186a_later(ram, hooks, "SaveUi_YesNoUpdate",
                                  0x80139F08u, 0u, 0u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawMainWindow",
                               0x80139A3Cu, 1u, 4u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawBackground3D",
                               0x80139A68u, 0u, 0u, 0u, 0u, 0u);
        return action == 0 ? 0 : action == 1 ? -1 : -2;
    case 13:
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawStateOverlay",
                               0x8014016Cu, 0u, 0u, 0u, 0u, 0u);
        action = ae_ovl186a_later(ram, hooks, "SaveUi_YesNoUpdate",
                                  0x80139F08u, 0u, 0u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawMainWindow",
                               0x80139A3Cu, 1u, 4u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawBackground3D",
                               0x80139A68u, 0u, 0u, 0u, 0u, 0u);
        if (action == 0) return 0;
        if (action == 1) {
            int32_t slot = ae_rds16(ram, AE_OVL186A_G_CARD_MENU_SLOT);
            uint32_t file = AE_OVL186A_G_SAVE_FILE_IMAGES +
                            (uint32_t)(slot * AE_OVL186A_SAVE_SLOT_IMAGE_STRIDE);
            (void)ae_SaveUi_SetState(ram, hooks, 8, 0);
            (void)ae_ovl186a_call(ram, hooks, "MemCardCheckFile",
                                  AE_OVL186A_API_MEMCARD_CHECK_FILE, 1u,
                                  file, 0u, 0u, 0u, 0u);
            ae_ovl186a_memcard_op(ram, hooks, 2u);
            return 0;
        }
        (void)ae_SaveUi_SetState(ram, hooks, 1, 5);
        return 0;
    case 15:
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_NameEntryAccept",
                               0x8013BC64u, 0u, 0u, 0u, 0u, 0u);
        if (ae_ovl186a_later(ram, hooks, "SaveUi_NameEntryCheck",
                             0x8013B6C4u, 0u, 0u, 0u, 0u, 0u) != 0) {
            (void)ae_SaveUi_SetState(ram, hooks, 16,
                                     ae_rds16(ram, AE_OVL186A_G_MODE_FLAGS));
        }
        return 0;
    case 16:
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_NameEntryUpdate",
                               0x8013B47Cu, 0u, 0u, 0u, 0u, 0u);
        return 0;
    default:
        return 0;
    }
}

void ae_SaveUi_FrameReset(AeRam *ram, const AeOverlayHooks *hooks)
{
    (void)ae_ovl186a_call(ram, hooks, "PadSampleStickHistory",
                          AE_OVL186A_API_PAD_SAMPLE_HISTORY, 0u, 0u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "FUN_8009BA90", AE_OVL186A_API_SOUND_DELAYED_RESET,
                          0u, 0u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "SoundDelayedQueueReset",
                          AE_OVL186A_API_DELAYED_QUEUE_RESET, 0u,
                          0u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_later(ram, hooks, "SaveUi_ThreadSetHookReset",
                           0x80141B34u, 0u, 0u, 0u, 0u, 0u);
}

int32_t ae_SaveUi_SceneInit(AeRam *ram, const AeOverlayHooks *hooks)
{
    uint32_t area = AE_OVL186A_G_STAGE_AREA_DATA;
    (void)ae_ovl186a_call(ram, hooks, "HeapInit",
                          AE_OVL186A_API_HEAP_INIT, 0u,
                          0u, 0u, 0u, 0u, 0u);
    uint32_t mesh_count = ae_rd8(ram, area + AE_OVL186A_AREA_MESH_COUNT);
    uint32_t mesh_list_bytes = mesh_count * 32u;
    uint32_t mesh_list = (uint32_t)ae_ovl186a_call(ram, hooks, "HeapAlloc",
                                                   AE_OVL186A_API_HEAP_ALLOC, 1u,
                                                   mesh_list_bytes, 0u, 0u, 0u, 0u);
    ae_wr32(ram, AE_OVL186A_G_MENU_MESH_LIST, mesh_list);

    for (uint32_t mesh_index = 1u; mesh_index <= mesh_count; ++mesh_index) {
        uint32_t record = area + AE_OVL186A_AREA_MESH_RECORDS +
                          (mesh_index - 1u) * AE_OVL186A_AREA_MESH_RECORD_SIZE;
        if (ae_rd8(ram, record + AE_OVL186A_AREA_MESH_MODE) == 2u) {
            uint32_t output = mesh_list + (mesh_index - 1u) * 32u;
            uint32_t model_index = ae_rd16(ram, record + AE_OVL186A_AREA_MESH_MODEL_INDEX);
            uint32_t input = area + model_index * 4u;
            const uint32_t args[2] = {output, input};
            (void)ae_ovl186a_call(ram, hooks, "MeshInit",
                                  AE_OVL186A_API_MESH_INIT, 2u,
                                  args[0], args[1], 0u, 0u, 0u);
        }
    }

    (void)ae_ovl186a_call(ram, hooks, "SetGeomScreen",
                          AE_OVL186A_API_SET_GEOM_SCREEN, 1u,
                          164u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "SetColorMatrix",
                          AE_OVL186A_API_SET_COLOR_MATRIX, 4u,
                          0u, 0u, 0u, 0u, 0u);
    ae_wr32(ram, AE_OVL186A_G_STAGE_FOG + AE_OVL186A_FOG_COLOR_R, 0u);
    ae_wr32(ram, AE_OVL186A_G_STAGE_FOG + AE_OVL186A_FOG_COLOR_G, 4096u);
    ae_wr32(ram, AE_OVL186A_G_STAGE_FOG + AE_OVL186A_FOG_COLOR_B, 4096u);
    ae_wr32(ram, AE_OVL186A_G_STAGE_FOG + AE_OVL186A_FOG_START, 0u);
    (void)ae_ovl186a_call(ram, hooks, "ApplyLevelFog",
                          AE_OVL186A_API_APPLY_LEVEL_FOG, 0u,
                          0u, 0u, 0u, 0u, 0u);
    (void)ae_sdk_MatrixIdentity(ram, AE_OVL186A_G_VIEW_MATRIX);

    ae_wr32(ram, AE_OVL186A_G_FILE_LIST_COUNT, 0u);
    ae_wr32(ram, AE_OVL186A_G_FILE_LIST_CURSOR, 16u);
    ae_wr16(ram, AE_OVL186A_G_MENU_SELECTION, 0xFFFFu);
    ae_wr16(ram, AE_OVL186A_G_MENU_FADE_STATE, 0u);
    ae_wr16(ram, AE_OVL186A_G_MENU_FADE_TIMER, 30u);
    ae_wr16(ram, AE_OVL186A_G_MENU_MODE, 0u);
    return 0;
}

static void ae_ovl186a_mode_cleanup(AeRam *ram,
                                   const AeOverlayHooks *hooks,
                                   uint32_t pad_state_argument)
{
    (void)ae_ovl186a_call(ram, hooks, "PadGetState",
                          AE_OVL186A_API_PAD_GET_STATE, 1u,
                          pad_state_argument, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_later(ram, hooks, "SaveUi_McxStopCom",
                           0x80141C00u, 0u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "FUN_8002B940", 0x8002B940u, 2u,
                          0x800E52D0u, 0x800E52F2u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "PadInitDirect", 0x8002B4A0u, 1u,
                          0x800E52D0u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "StickToButtons", 0x8002F1F0u, 1u,
                          0u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "PadSetupDualShock",
                          AE_OVL186A_API_PAD_SETUP_DUALSHOCK, 1u,
                          0u, 0u, 0u, 0u, 0u);
    ae_wr32(ram, 0x800E54E0u, 0u);

    int32_t mode = ae_rds16(ram, AE_OVL186A_G_CURRENT_MODE);
    if (mode != 28) {
        (void)ae_ovl186a_call(ram, hooks, "SoundStopAllVoices",
                              AE_OVL186A_API_SOUND_STOP_ALL, 1u,
                              mode == 0 ? 0u : 1u, 0u, 0u, 0u, 0u);
    }
    uint32_t display = ae_rd32(ram, AE_OVL186A_G_MENU_SCENE);
    (void)ae_ovl186a_call(ram, hooks, "ResetGraphOrDisplay", 0x8002E7C0u, 1u,
                          display, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "ClearImage",
                          AE_OVL186A_API_CLEAR_IMAGE, 4u,
                          0x801FFEE8u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "DrawSync",
                          AE_OVL186A_API_DRAW_SYNC, 1u,
                          0u, 0u, 0u, 0u, 0u);
}

int32_t ae_Mode_SaveMenu(AeRam *ram, const AeOverlayHooks *hooks,
                         uint32_t caller_a0)
{
    uint8_t *stack_copy = ae_ptr(ram, 0x801FFEE8u, 8u);
    const uint8_t *display_template = ae_ptr(ram,
                                              AE_OVL186A_G_DISPLAY_TEMPLATE,
                                              8u);
    if (stack_copy != NULL && display_template != NULL) {
        for (uint32_t byte = 0u; byte < 8u; ++byte)
            stack_copy[byte] = display_template[byte];
    }

    (void)ae_ovl186a_call(ram, hooks, "PadGetState",
                          AE_OVL186A_API_PAD_GET_STATE, 1u,
                          caller_a0, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_later(ram, hooks, "SaveUi_McxStartCom",
                           0x80141B90u, 0u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "FUN_8002B940", 0x8002B940u, 2u,
                          0x800E52D0u, 0x800E52F2u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "PadInitDirect", 0x8002B4A0u, 1u,
                          0x800E52D0u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "StickToButtons", 0x8002F1F0u, 1u,
                          0u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "PadSetupDualShock",
                          AE_OVL186A_API_PAD_SETUP_DUALSHOCK, 1u,
                          0u, 0u, 0u, 0u, 0u);
    int32_t voice = ae_ovl186a_call(ram, hooks, "BgmGetVoice",
                                    0x80086408u, 0u,
                                    0u, 0u, 0u, 0u, 0u);
    ae_wr32(ram, 0x800E54E0u, 0u);

    uint32_t archive_list = voice < 0 ? AE_OVL186A_G_ARCHIVE_LIST_NO_MUSIC
                                      : AE_OVL186A_G_ARCHIVE_LIST_WITH_MUSIC;
    (void)ae_ovl186a_call(ram, hooks, "ArcLoadEntry",
                          AE_OVL186A_API_ARC_LOAD_ENTRY, 1u,
                          archive_list, 0u, 0u, 0u, 0u);
    int32_t load_result;
    do {
        load_result = ae_ovl186a_call(ram, hooks, "LoadListStep",
                                      AE_OVL186A_API_LOAD_LIST_STEP, 0u,
                                      0u, 0u, 0u, 0u, 0u);
        if (load_result >= 0) {
            (void)ae_ovl186a_call(ram, hooks, "CdMgrUpdate",
                                  AE_OVL186A_API_CD_MANAGER_UPDATE, 0u,
                                  0u, 0u, 0u, 0u, 0u);
            (void)ae_ovl186a_call(ram, hooks, "VSync",
                                  AE_OVL186A_API_VSYNC, 1u,
                                  2u, 0u, 0u, 0u, 0u);
        }
    } while (load_result >= 0);

    (void)ae_ovl186a_later(ram, hooks, "SaveUi_ThreadSetHook",
                           0x80141AF0u, 0u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "ResetGraphOrDisplay", 0x8002E7C0u,
                          1u, 0x80136D58u, 0u, 0u, 0u, 0u);
    int32_t display = ae_ovl186a_call(ram, hooks, "ResetGraph",
                                      AE_OVL186A_API_RESET_GRAPH, 1u,
                                      1u, 0u, 0u, 0u, 0u);
    ae_wr32(ram, AE_OVL186A_G_MENU_SCENE, (uint32_t)display);
    (void)ae_ovl186a_call(ram, hooks, "ClearImage",
                          AE_OVL186A_API_CLEAR_IMAGE, 4u,
                          0x801FFEE8u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "DrawSync",
                          AE_OVL186A_API_DRAW_SYNC, 1u,
                          0u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "SoundStopAndRestoreVolume",
                          AE_OVL186A_API_SOUND_STOP_RESTORE, 0u,
                          0u, 0u, 0u, 0u, 0u);
    (void)ae_SaveUi_SceneInit(ram, hooks);

    int32_t mode = ae_rds16(ram, AE_OVL186A_G_CURRENT_MODE);
    int32_t entry_mode = mode;
    uint32_t cleanup_pad_argument =
        entry_mode == 21 || entry_mode == 35
            ? 0u : AE_OVL186A_G_PAD_STATE_BASE;
    if (mode == 21) {
        ae_wr16(ram, AE_OVL186A_G_MODE_ARGUMENT, 21u);
        (void)ae_SaveUi_RunMenuA(ram, hooks);
    } else if (mode == 35) {
        ae_wr16(ram, AE_OVL186A_G_MODE_ARGUMENT, 35u);
        (void)ae_SaveUi_RunMenuB(ram, hooks);
    }

    for (;;) {
        mode = ae_rds16(ram, AE_OVL186A_G_CURRENT_MODE);
        if (mode < 0 || mode >= 36) continue;
        uint32_t handler = ae_rd32(ram, AE_OVL186A_G_MODE_DISPATCH +
                                        (uint32_t)mode * 4u);
        switch (handler) {
        case 0x80136C2Cu:
            continue;
        case 0x80136C58u:
            (void)ae_SaveUi_RunMenuC(ram, hooks,
                                      ae_rds16(ram, AE_OVL186A_G_MODE_ARGUMENT));
            cleanup_pad_argument = 9u;
            break;
        case 0x80136C6Cu:
            (void)ae_ovl186a_later(ram, hooks, "SaveUi_ModeStep",
                                   0x8013C7F0u, 0u, 0u, 0u, 0u, 0u);
            break;
        case 0x80136C7Cu:
            (void)ae_SaveUi_RunMenuA(ram, hooks);
            cleanup_pad_argument = 0u;
            break;
        case 0x80136C8Cu:
            (void)ae_SaveUi_RunMenuB(ram, hooks);
            cleanup_pad_argument = 0u;
            break;
        case 0x80136C9Cu:
            ae_ovl186a_mode_cleanup(
                ram, hooks, cleanup_pad_argument);
            return 0;
        default:
            break;
        }
    }
}

int32_t ae_SaveUi_MemCardStart(AeRam *ram, const AeOverlayHooks *hooks,
                               int32_t operation)
{
    (void)ae_ovl186a_call(ram, hooks, "MemCardOp",
                          AE_OVL186A_API_MEMCARD_OP, 2u, 8u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "MemCardOp",
                          AE_OVL186A_API_MEMCARD_OP, 2u, 7u, 0u, 0u, 0u, 0u);
    ae_wr16(ram, AE_OVL186A_G_MEMCARD_STATE + AE_OVL186A_CARD_PROGRESS, 0u);
    ae_wr16(ram, AE_OVL186A_G_MEMCARD_STATE + AE_OVL186A_CARD_RESULT_FLAGS, 0u);
    ae_wr16(ram, AE_OVL186A_G_MEMCARD_STATE + AE_OVL186A_CARD_RESULT, 1u);
    if (operation == 0) {
        ae_wr16(ram, AE_OVL186A_G_MENU_STATE, 0u);
    }
    (void)ae_ovl186a_call(ram, hooks, "MemCardOp",
                          AE_OVL186A_API_MEMCARD_OP, 2u,
                          operation == 0 ? 0u : 17u, 0u, 0u, 0u, 0u);
    return 0;
}

void ae_SaveUi_FileSelectInit(AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t has_progress = ae_ovl186a_call(ram, hooks, "LevelProgressGet",
                                           AE_OVL186A_API_LEVEL_PROGRESS_GET, 1u,
                                           24u, 0u, 0u, 0u, 0u) > 0;
    int32_t checksum = ae_ovl186a_call(ram, hooks, "ProgressChecksum",
                                       AE_OVL186A_API_PROGRESS_CHECKSUM, 1u,
                                       0u, 0u, 0u, 0u, 0u);
    ae_SaveUi_FormatNumber3(ram, checksum, AE_OVL186A_TEMP_FORMAT_0);

    int32_t card_count = checksum;
    if (has_progress) {
        card_count = ae_SaveUi_TotalMonkeyCount(ram);
        ae_SaveUi_FormatNumber3(ram, card_count, AE_OVL186A_TEMP_FORMAT_1);
    } else {
        ae_wr8(ram, AE_OVL186A_TEMP_FORMAT_1, 0x7Fu);
        ae_wr8(ram, AE_OVL186A_TEMP_FORMAT_1 + 1u, 0x7Fu);
        ae_wr8(ram, AE_OVL186A_TEMP_FORMAT_1 + 2u, 0u);
    }

    uint32_t color_buffer = (uint32_t)ae_ovl186a_later(ram, hooks,
                                                       "SaveUi_GetColorBuffer",
                                                       0x8013FC20u, 0u,
                                                       0u, 0u, 0u, 0u);
    for (uint32_t i = 0u; i < 3u; ++i) {
        ae_wr8(ram, color_buffer + i,
               ae_rd8(ram, AE_OVL186A_TEMP_FORMAT_0 + i));
        ae_wr8(ram, color_buffer + 4u + i,
               ae_rd8(ram, AE_OVL186A_TEMP_FORMAT_1 + i));
    }
    ae_wr8(ram, color_buffer + 7u, 0u);

    ae_wr16(ram, AE_OVL186A_G_SLOT_SELECTION, 1u);
    ae_wr32(ram, AE_OVL186A_G_MENU_SCREEN, 0xFFFFFE00u);
    ae_wr16(ram, AE_OVL186A_G_MENU_FADE_STATE, 0u);
    ae_wr16(ram, AE_OVL186A_G_MENU_FADE_TIMER, 30u);
    ae_wr16(ram, AE_OVL186A_G_FILE_LIST_FLAGS, 0u);

    (void)ae_SaveUi_MemCardStart(ram, hooks, 1);
    (void)ae_ovl186a_later(ram, hooks, "SaveUi_ThreadSetHook",
                           0x80141A10u, 1u, 0x80138370u, 0u, 0u, 0u);
}

int32_t ae_SaveUi_MenuUpdate(AeRam *ram, const AeOverlayHooks *hooks)
{
    (void)ae_ovl186a_later(ram, hooks, "CardMgr_DrawMenuLabels",
                           0x8013FB30u, 1u,
                           (uint32_t)ae_rds16(ram, AE_OVL186A_G_SLOT_SELECTION),
                           0u, 0u, 0u);
    (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawCursorAnim",
                           0x80139CF0u, 0u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawBackground3D",
                           0x80139A68u, 0u, 0u, 0u, 0u, 0u);
    if (ae_rds16(ram, AE_OVL186A_G_MENU_FADE_STATE) != 1) return -1;

    if (ae_ovl186a_later(ram, hooks, "SaveUi_CancelPressed",
                         0x8013A43Cu, 0u, 0u, 0u, 0u, 0u) != 0) {
        (void)ae_ovl186a_call(ram, hooks, "PlaySfx", AE_OVL186A_API_SOUND_PLAY,
                              1u, 1u, 0u, 0u, 0u, 0u);
        return 3;
    }
    if (ae_ovl186a_later(ram, hooks, "SaveUi_ConfirmPressed",
                         0x8013A400u, 0u, 0u, 0u, 0u, 0u) != 0) {
        (void)ae_ovl186a_call(ram, hooks, "PlaySfx", AE_OVL186A_API_SOUND_PLAY,
                              1u, 3u, 0u, 0u, 0u, 0u);
        return ae_rds16(ram, AE_OVL186A_G_SLOT_SELECTION);
    }

    uint32_t enabled = ae_rd16(ram, AE_OVL186A_G_FILE_LIST_FLAGS) < 1u ? 1u : 0u;
    if (ae_rd32(ram, AE_OVL186A_G_PAD_REPEAT) == 0x8000u) {
        uint32_t selection = (ae_rd16(ram, AE_OVL186A_G_SLOT_SELECTION) - 1u) & 0xFFFFu;
        int32_t signed_selection = ae_ovl186a_sign16(selection);
        ae_wr16(ram, AE_OVL186A_G_SLOT_SELECTION, selection);
        if (signed_selection < (int32_t)enabled) {
            ae_wr16(ram, AE_OVL186A_G_SLOT_SELECTION, enabled);
        } else {
            (void)ae_ovl186a_call(ram, hooks, "PlaySfx", AE_OVL186A_API_SOUND_PLAY,
                                  1u, 0u, 0u, 0u, 0u, 0u);
        }
    }
    if (ae_rd32(ram, AE_OVL186A_G_PAD_REPEAT) == 0x2000u) {
        uint32_t selection = (ae_rd16(ram, AE_OVL186A_G_SLOT_SELECTION) + 1u) & 0xFFFFu;
        ae_wr16(ram, AE_OVL186A_G_SLOT_SELECTION, selection);
        if (ae_ovl186a_sign16(selection) >= 3) {
            ae_wr16(ram, AE_OVL186A_G_SLOT_SELECTION, 2u);
        } else {
            (void)ae_ovl186a_call(ram, hooks, "PlaySfx", AE_OVL186A_API_SOUND_PLAY,
                                  1u, 0u, 0u, 0u, 0u, 0u);
        }
    }
    return -1;
}

static void ae_ovl186a_copy_display_template(AeRam *ram, uint32_t destination)
{
    ae_wr32(ram, destination,
            ae_rd32(ram, AE_OVL186A_G_DISPLAY_TEMPLATE));
    ae_wr32(ram, destination + 4u,
            ae_rd32(ram, AE_OVL186A_G_DISPLAY_TEMPLATE + 4u));
}

static void ae_ovl186a_runner_display_init(AeRam *ram,
                                           const AeOverlayHooks *hooks)
{
    (void)ae_ovl186a_call(ram, hooks, "InitDisplay",
                          AE_OVL186A_API_INIT_DISPLAY, 0u,
                          0u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "ResetGraph",
                          AE_OVL186A_API_RESET_GRAPH, 1u,
                          1u, 0u, 0u, 0u, 0u);

    uint32_t frame = AE_OVL186A_G_FRAME_COUNTER;
    ae_wr8(ram, frame + AE_OVL186A_FRAME_PACKET_FLAGS, 1u);
    ae_wr8(ram, frame + AE_OVL186A_FRAME_DISPLAY_FLAGS, 1u);
    for (uint32_t environment = 0u; environment < 2u; ++environment) {
        uint32_t base = frame + (environment == 0u ? AE_OVL186A_FRAME_DRAW_ENV
                                                    : 0x4088u);
        for (uint32_t byte = 25u; byte <= 27u; ++byte)
            ae_wr8(ram, base + byte, 0u);
    }
}

static void ae_ovl186a_runner_present(AeRam *ram,
                                      const AeOverlayHooks *hooks,
                                      uint32_t vsync_mode,
                                      int thread_switch)
{
    (void)ae_ovl186a_call(ram, hooks, "DrawSync",
                          AE_OVL186A_API_DRAW_SYNC, 1u,
                          0u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "VSync",
                          AE_OVL186A_API_VSYNC, 1u,
                          vsync_mode, 0u, 0u, 0u, 0u);
    if (thread_switch) {
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_ThreadSwitch",
                               0x80141B00u, 0u, 0u, 0u, 0u, 0u);
    }

    uint32_t frame = AE_OVL186A_G_FRAME_COUNTER;
    ae_wr32(ram, frame + AE_OVL186A_FRAME_COUNTER,
            ae_rd32(ram, frame + AE_OVL186A_FRAME_COUNTER) + 1u);
    uint32_t display_index = ae_rd32(ram, frame + AE_OVL186A_FRAME_DISPLAY_INDEX);
    uint32_t display_offset = display_index * AE_OVL186A_FRAME_DISPLAY_STRIDE;
    (void)ae_ovl186a_call(ram, hooks, "PutDispEnv",
                          AE_OVL186A_API_PUT_DISPLAY_ENV, 1u,
                          frame + AE_OVL186A_FRAME_DISPLAY_ENV + display_offset,
                          0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "PutDrawEnv",
                          AE_OVL186A_API_PUT_DRAW_ENV, 1u,
                          frame + AE_OVL186A_FRAME_DRAW_ENV + display_offset,
                          0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "DrawOTag",
                          AE_OVL186A_API_DRAW_OTAG, 1u,
                          frame + AE_OVL186A_FRAME_OTAG + display_offset + 0x3FFCu,
                          0u, 0u, 0u, 0u);
}

static int32_t ae_ovl186a_runner_finish(AeRam *ram,
                                       const AeOverlayHooks *hooks,
                                       int32_t next_mode)
{
    ae_wr16(ram, AE_OVL186A_G_CURRENT_MODE, (uint32_t)next_mode);
    (void)ae_ovl186a_later(ram, hooks, "SaveUi_ThreadClose",
                           0x80141AB4u, 0u, 0u, 0u, 0u, 0u);
    ae_ovl186a_memcard_op(ram, hooks, 9u);
    ae_ovl186a_copy_display_template(ram, AE_OVL186A_TEMP_FORMAT_2);
    (void)ae_ovl186a_call(ram, hooks, "ClearImage",
                          AE_OVL186A_API_CLEAR_IMAGE, 4u,
                          AE_OVL186A_TEMP_FORMAT_2, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "DrawSync",
                          AE_OVL186A_API_DRAW_SYNC, 1u,
                          0u, 0u, 0u, 0u, 0u);
    return 0;
}

static int32_t ae_ovl186a_runner_finish_basic(AeRam *ram,
                                             const AeOverlayHooks *hooks,
                                             int32_t next_mode)
{
    ae_wr16(ram, AE_OVL186A_G_CURRENT_MODE, (uint32_t)next_mode);
    ae_ovl186a_copy_display_template(ram, AE_OVL186A_TEMP_FORMAT_2);
    (void)ae_ovl186a_call(ram, hooks, "ClearImage",
                          AE_OVL186A_API_CLEAR_IMAGE, 4u,
                          AE_OVL186A_TEMP_FORMAT_2, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_call(ram, hooks, "DrawSync",
                          AE_OVL186A_API_DRAW_SYNC, 1u,
                          0u, 0u, 0u, 0u, 0u);
    return 0;
}

static int32_t ae_ovl186a_runner_finish_card(AeRam *ram,
                                             const AeOverlayHooks *hooks,
                                             int32_t next_mode)
{
    ae_wr16(ram, AE_OVL186A_G_CURRENT_MODE, (uint32_t)next_mode);
    (void)ae_ovl186a_later(ram, hooks, "SaveUi_ThreadClose",
                           0x80141AB4u, 0u, 0u, 0u, 0u, 0u);
    ae_ovl186a_memcard_op(ram, hooks, 9u);
    return 0;
}

int32_t ae_SaveUi_RunMenuA(AeRam *ram, const AeOverlayHooks *hooks)
{
    ae_ovl186a_copy_display_template(ram, AE_OVL186A_TEMP_FORMAT_2);
    ae_ovl186a_runner_display_init(ram, hooks);
    ae_SaveUi_FileSelectInit(ram, hooks);
    ae_wr32(ram, AE_OVL186A_G_FRAME_COUNTER + AE_OVL186A_FRAME_DISPLAY_INDEX, 0u);

    int32_t next_mode = 0;
    for (;;) {
        (void)ae_ovl186a_call(ram, hooks, "PadUpdate",
                              AE_OVL186A_API_PAD_UPDATE, 1u,
                              2u, 0u, 0u, 0u, 0u);
        (void)ae_ovl186a_call(ram, hooks, "BeginFrame",
                              AE_OVL186A_API_BEGIN_FRAME, 0u,
                              0u, 0u, 0u, 0u, 0u);
        int32_t fade = ae_ovl186a_later(ram, hooks, "SaveUi_FadeTick",
                                        0x8013A634u, 0u, 0u, 0u, 0u, 0u);
        if (fade < 0) break;

        int32_t selection = ae_SaveUi_MenuUpdate(ram, hooks);
        if (selection >= 0) {
            switch (selection) {
            case 0: next_mode = 22; ae_wr16(ram, AE_OVL186A_G_MENU_FADE_STATE, 2u); break;
            case 1: next_mode = 14; ae_wr16(ram, AE_OVL186A_G_MENU_FADE_STATE, 2u); break;
            case 2: next_mode = 28; ae_wr16(ram, AE_OVL186A_G_MENU_FADE_STATE, 2u); break;
            case 3: next_mode = 13; ae_wr16(ram, AE_OVL186A_G_MENU_FADE_STATE, 2u); break;
            default: ae_wr16(ram, AE_OVL186A_G_MENU_FADE_STATE, 2u); break;
            }
        }
        ae_ovl186a_runner_present(ram, hooks, 2u, 1);
    }
    return ae_ovl186a_runner_finish(ram, hooks, next_mode);
}

int32_t ae_SaveUi_RunMenuB(AeRam *ram, const AeOverlayHooks *hooks)
{
    ae_ovl186a_copy_display_template(ram, AE_OVL186A_TEMP_FORMAT_2);
    ae_ovl186a_runner_display_init(ram, hooks);
    uint32_t frame = AE_OVL186A_G_FRAME_COUNTER;
    ae_wr32(ram, frame + AE_OVL186A_FRAME_DISPLAY_INDEX, 0u);
    ae_wr16(ram, AE_OVL186A_G_MENU_FADE_STATE, 0u);
    ae_wr16(ram, AE_OVL186A_G_MENU_FADE_TIMER, 30u);
    ae_wr16(ram, AE_OVL186A_G_MENU_STATE, 35u);
    ae_wr32(ram, AE_OVL186A_G_FRAME_COUNTER + 12u, 0u);

    int32_t next_mode = 0;
    for (;;) {
        (void)ae_ovl186a_call(ram, hooks, "PadUpdate",
                              AE_OVL186A_API_PAD_UPDATE, 1u,
                              2u, 0u, 0u, 0u, 0u);
        (void)ae_ovl186a_call(ram, hooks, "BeginFrame",
                              AE_OVL186A_API_BEGIN_FRAME, 0u,
                              0u, 0u, 0u, 0u, 0u);
        int32_t fade = ae_ovl186a_later(ram, hooks, "SaveUi_FadeTick",
                                        0x8013A634u, 0u, 0u, 0u, 0u, 0u);
        if (fade < 0) break;

        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawStateOverlay",
                               0x8014016Cu, 0u, 0u, 0u, 0u, 0u);
        int32_t yes_no = ae_ovl186a_later(ram, hooks, "SaveUi_YesNoUpdate",
                                          0x80139F08u, 0u, 0u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawMainWindow",
                               0x80139A3Cu, 1u, 4u, 0u, 0u, 0u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawBackground3D",
                               0x80139A68u, 0u, 0u, 0u, 0u, 0u);
        if (yes_no != 0) {
            next_mode = yes_no == 2 ? 0 : 14;
            ae_wr16(ram, AE_OVL186A_G_MENU_FADE_STATE, 2u);
        }
        ae_ovl186a_runner_present(ram, hooks, 2u, 0);
    }
    return ae_ovl186a_runner_finish_basic(ram, hooks, next_mode);
}

int32_t ae_SaveUi_RunMenuC(AeRam *ram, const AeOverlayHooks *hooks,
                          int32_t argument)
{
    ae_ovl186a_copy_display_template(ram, AE_OVL186A_TEMP_FORMAT_2);
    ae_ovl186a_runner_display_init(ram, hooks);
    uint32_t frame = AE_OVL186A_G_FRAME_COUNTER;
    ae_wr16(ram, AE_OVL186A_G_SLOT_SELECTION, 0u);
    ae_wr16(ram, AE_OVL186A_G_MENU_MODE, 5u);
    ae_wr16(ram, AE_OVL186A_G_MODE_FLAGS, 0u);
    ae_wr16(ram, AE_OVL186A_G_MODE_SUBSTATE, 0u);
    ae_wr16(ram, AE_OVL186A_G_MODE_TIMER, 0u);
    ae_wr32(ram, frame + AE_OVL186A_FRAME_DISPLAY_INDEX, 0u);
    (void)ae_SaveUi_MemCardStart(ram, hooks, 0);
    (void)ae_ovl186a_later(ram, hooks, "SaveUi_ThreadSetHook",
                           0x80141A10u, 1u, 0x8013829Cu, 0u, 0u, 0u);
    ae_wr16(ram, AE_OVL186A_G_MENU_FADE_STATE, 0u);
    ae_wr16(ram, AE_OVL186A_G_MENU_FADE_TIMER, 30u);

    int32_t next_mode = 0;
    for (;;) {
        (void)ae_ovl186a_call(ram, hooks, "PadUpdate",
                              AE_OVL186A_API_PAD_UPDATE, 1u,
                              2u, 0u, 0u, 0u, 0u);
        (void)ae_ovl186a_call(ram, hooks, "BeginFrame",
                              AE_OVL186A_API_BEGIN_FRAME, 0u,
                              0u, 0u, 0u, 0u, 0u);
        int32_t fade = ae_ovl186a_later(ram, hooks, "SaveUi_FadeTick",
                                        0x8013A634u, 0u, 0u, 0u, 0u, 0u);
        if (fade < 0) break;

        int32_t state_result = ae_SaveUi_StateTick(ram, hooks);
        if (state_result < 0) {
            if (state_result == -4) {
                next_mode = argument < 0 ? 3 : argument == 35 ? 35 : 21;
                ae_wr16(ram, AE_OVL186A_G_MENU_FADE_STATE, 2u);
            } else if (state_result == -1) {
                next_mode = argument < 0 ? 13 : argument == 35 ? 0 : 21;
                ae_wr16(ram, AE_OVL186A_G_MENU_FADE_STATE, 2u);
            } else if (state_result == -2) {
                next_mode = 3;
                ae_wr16(ram, AE_OVL186A_G_MENU_FADE_STATE, 2u);
            } else if (state_result == -5) {
                next_mode = 0;
                ae_wr16(ram, AE_OVL186A_G_MENU_FADE_STATE, 2u);
            } else {
                next_mode = 2;
                ae_wr16(ram, AE_OVL186A_G_MENU_FADE_STATE, 2u);
            }
        }
        ae_ovl186a_runner_present(ram, hooks, 0u, 1);
    }
    return ae_ovl186a_runner_finish_card(ram, hooks, next_mode);
}

static void ae_ovl186a_build_file_entries(AeRam *ram,
                                           const AeOverlayHooks *hooks)
{
    for (uint32_t slot = 0u; slot < AE_OVL186A_SAVE_SLOT_COUNT; ++slot) {
        uint32_t entry = AE_OVL186A_G_FILE_ENTRIES +
                         slot * AE_OVL186A_FILE_ENTRY_STRIDE;
        uint32_t save_slot = AE_OVL186A_G_SAVE_SLOT_RECORDS +
                             slot * AE_OVL186A_SAVE_SLOT_STRIDE;

        ae_wr16(ram, entry + AE_OVL186A_FILE_ENTRY_MONKEYS, 0u);
        ae_wr16(ram, entry + AE_OVL186A_FILE_ENTRY_SCORE_A, 204u);
        ae_wr16(ram, entry + AE_OVL186A_FILE_ENTRY_SCORE_B, 60u);
        ae_wr16(ram, entry + AE_OVL186A_FILE_ENTRY_LAST_USED, 0xFFFFu);

        int32_t saved_status = ae_rds16(ram,
                                        save_slot + AE_OVL186A_SAVE_SLOT_IMAGE_OFFSET);
        if (saved_status >= 0) {
            int32_t last_used = ae_ovl186a_later(ram, hooks,
                                                 "SaveUi_LastUsedEntry",
                                                 0x801399E4u, 1u,
                                                 slot, 0u, 0u, 0u);
            ae_wr16(ram, entry + AE_OVL186A_FILE_ENTRY_LAST_USED,
                    (uint32_t)last_used);
            (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawFileIcons",
                                   0x8013A478u, 2u, slot,
                                   (uint32_t)saved_status, 0u, 0u);
        }

        int32_t monkey_total = 0;
        for (uint32_t monkey = 0u;
             monkey < AE_OVL186A_SAVE_SLOT_MONKEY_COUNT; ++monkey) {
            uint32_t count_address = save_slot +
                                     AE_OVL186A_SAVE_SLOT_MONKEYS_OFFSET + monkey;
            monkey_total = (int32_t)ae_ovl186a_wrap_add(
                (uint32_t)monkey_total, ae_rd8(ram, count_address));
        }
        ae_wr16(ram, entry + AE_OVL186A_FILE_ENTRY_MONKEYS,
                (uint32_t)monkey_total);

        uint32_t list = save_slot + AE_OVL186A_SAVE_SLOT_LIST_OFFSET;
        uint32_t flags_address = save_slot + AE_OVL186A_SAVE_SLOT_FLAGS_OFFSET;
        int32_t progress = ae_SaveUi_CountEntries(ram, list,
                                                   ae_rd32(ram, flags_address));
        ae_wr16(ram, entry + AE_OVL186A_FILE_ENTRY_PROGRESS, (uint32_t)progress);

        uint32_t score_address = save_slot + AE_OVL186A_SAVE_SLOT_SCORE_OFFSET;
        int32_t score = ae_ovl186a_call(ram, hooks, "SaveScore",
                                        AE_OVL186A_API_SAVE_SCORE, 1u,
                                        score_address, 0u, 0u, 0u, 0u);
        ae_wr16(ram, entry + AE_OVL186A_FILE_ENTRY_SCORE_RESULT, (uint32_t)score);
        if (score == 1000) {
            ae_wr8(ram, entry + AE_OVL186A_FILE_ENTRY_SCORE_TEXT, (uint8_t)'k');
            ae_wr8(ram, entry + AE_OVL186A_FILE_ENTRY_SCORE_TEXT + 1u,
                   (uint8_t)'j');
            ae_wr8(ram, entry + AE_OVL186A_FILE_ENTRY_SCORE_TEXT + 2u,
                   (uint8_t)'j');
            ae_wr8(ram, entry + AE_OVL186A_FILE_ENTRY_SCORE_TEXT + 3u, 0xBCu);
            ae_wr8(ram, entry + AE_OVL186A_FILE_ENTRY_SCORE_TEXT + 4u, 0u);
        } else {
            uint32_t score_text = entry + AE_OVL186A_FILE_ENTRY_SCORE_TEXT;
            (void)ae_SaveUi_FormatNumber3(ram, score, score_text);
            uint8_t hundreds = ae_rd8(ram, score_text + 2u);
            ae_wr8(ram, score_text + 2u, 0xACu);
            ae_wr8(ram, score_text + 4u, 0xBCu);
            ae_wr8(ram, score_text + 5u, 0u);
            ae_wr8(ram, score_text + 3u, hundreds);
        }

        (void)ae_SaveUi_FormatNumber3(ram, monkey_total,
                                       entry + AE_OVL186A_FILE_ENTRY_TEXT_MONKEYS);
        (void)ae_SaveUi_FormatNumber3(ram,
                                       ae_rds16(ram, entry + AE_OVL186A_FILE_ENTRY_SCORE_A),
                                       entry + AE_OVL186A_FILE_ENTRY_TEXT_SCORE_A);
        (void)ae_SaveUi_FormatNumber3(ram, progress,
                                       entry + AE_OVL186A_FILE_ENTRY_TEXT_PROGRESS);
        (void)ae_SaveUi_FormatNumber3(ram, progress,
                                       entry + AE_OVL186A_FILE_ENTRY_DETAIL_PROGRESS);
        ae_wr8(ram, entry + AE_OVL186A_FILE_ENTRY_DETAIL_PROGRESS_TERM, 0u);
        (void)ae_SaveUi_FormatNumber3(ram,
                                       ae_rds16(ram, entry + AE_OVL186A_FILE_ENTRY_SCORE_B),
                                       entry + AE_OVL186A_FILE_ENTRY_TEXT_SCORE_B);
        (void)ae_SaveUi_FormatNumber3(ram, monkey_total,
                                       entry + AE_OVL186A_FILE_ENTRY_DETAIL_MONKEYS);

        ae_wr8(ram, entry + AE_OVL186A_FILE_ENTRY_TEXT_COLOR, 141u);
        if (ae_rd8(ram, save_slot + AE_OVL186A_SAVE_SLOT_STATUS_OFFSET) != 0u) {
            for (uint32_t color = 0u;
                 color < AE_OVL186A_FILE_ENTRY_COLOR_COUNT; ++color) {
                ae_wr8(ram, entry + AE_OVL186A_FILE_ENTRY_DETAIL_COLOR + color,
                       ae_rd8(ram, entry + AE_OVL186A_FILE_ENTRY_COLOR_SOURCE + color));
            }
        } else {
            for (uint32_t color = 0u;
                 color < AE_OVL186A_FILE_ENTRY_COLOR_COUNT; ++color) {
                ae_wr8(ram, entry + AE_OVL186A_FILE_ENTRY_DETAIL_COLOR + color, 127u);
            }
        }

        ae_wr8(ram, entry + AE_OVL186A_FILE_ENTRY_SECOND_TEXT_COLOR, 141u);
        if (ae_rd8(ram, save_slot + AE_OVL186A_SAVE_SLOT_STATUS_OFFSET) != 0u) {
            for (uint32_t color = 0u;
                 color < AE_OVL186A_FILE_ENTRY_COLOR_COUNT; ++color) {
                ae_wr8(ram, entry + AE_OVL186A_FILE_ENTRY_SECOND_DETAIL_COLOR + color,
                       ae_rd8(ram, entry + AE_OVL186A_FILE_ENTRY_SECOND_COLOR_SOURCE + color));
            }
        } else {
            for (uint32_t color = 0u;
                 color < AE_OVL186A_FILE_ENTRY_COLOR_COUNT; ++color) {
                ae_wr8(ram, entry + AE_OVL186A_FILE_ENTRY_SECOND_DETAIL_COLOR + color, 127u);
            }
        }

        ae_wr8(ram, entry + AE_OVL186A_FILE_ENTRY_UNUSED_TEXT, 0u);
        ae_wr8(ram, entry + AE_OVL186A_FILE_ENTRY_BONUS_TEXT_COLOR, 140u);
        ae_wr8(ram, entry + AE_OVL186A_FILE_ENTRY_SECOND_TEXT_COLOR_2, 140u);
        uint32_t bonus_count = ae_rd16(ram,
                                       save_slot + AE_OVL186A_SAVE_SLOT_BONUS_COUNTS) >> 8;
        uint32_t bonus_count_text = entry + AE_OVL186A_FILE_ENTRY_BONUS_COUNT_TEXT;
        (void)ae_SaveUi_FormatNumber3(ram, (int32_t)bonus_count, bonus_count_text);
        uint8_t bonus_high = ae_rd8(ram, bonus_count_text + 1u);
        uint8_t bonus_low = ae_rd8(ram, bonus_count_text + 2u);
        ae_wr8(ram, bonus_count_text + 2u, 0u);
        ae_wr8(ram, bonus_count_text, bonus_high);
        ae_wr8(ram, bonus_count_text + 1u, bonus_low);

        uint32_t remaining_count =
            (uint16_t)(ae_rd16(ram, save_slot + AE_OVL186A_SAVE_SLOT_BONUS_COUNTS) - 1u) & 0xFFu;
        uint32_t remainder_text = entry + AE_OVL186A_FILE_ENTRY_BONUS_REMAINDER_TEXT;
        (void)ae_SaveUi_FormatNumber3(ram, (int32_t)remaining_count, remainder_text);
        uint8_t remainder_high = ae_rd8(ram, remainder_text + 1u);
        uint8_t remainder_low = ae_rd8(ram, remainder_text + 2u);
        ae_wr8(ram, remainder_text + 2u, 0u);
        ae_wr8(ram, remainder_text, remainder_high);
        ae_wr8(ram, remainder_text + 1u, remainder_low);

        ae_wr32(ram, entry + AE_OVL186A_FILE_ENTRY_SAVE_FLAGS,
                ae_rd32(ram, save_slot + AE_OVL186A_SAVE_SLOT_FLAGS_WORD));
    }
}

int32_t ae_SaveUi_SetState(AeRam *ram, const AeOverlayHooks *hooks,
                           int32_t state, int32_t argument)
{
    ae_wr16(ram, AE_OVL186A_G_MENU_STATE, (uint32_t)state);
    if ((uint32_t)state >= 46u) return 0;

    switch (state) {
    case 1: {
        ae_wr16(ram, AE_OVL186A_G_MENU_MODE, (uint32_t)argument);
        ae_wr16(ram, AE_OVL186A_G_MENU_MODE + 4u, 0u);
        for (uint32_t entry = 0u; entry < 5u; ++entry) {
            ae_wr16(ram, AE_OVL186A_G_SLOT_MENUS + entry * 2u, 1u);
        }
        int32_t selection_mode = ae_rds16(ram, AE_OVL186A_G_MENU_SELECTION);
        if (selection_mode < 0) {
            ae_wr16(ram, AE_OVL186A_G_SLOT_MENUS + 8u, 0xFFFFu);
            ae_wr16(ram, AE_OVL186A_G_SLOT_MENUS + 6u, 0xFFFFu);
            ae_wr16(ram, AE_OVL186A_G_SLOT_MENUS + 2u, 0xFFFFu);
        } else if (selection_mode == 35 || argument == 2) {
            ae_wr16(ram, AE_OVL186A_G_SLOT_MENUS + 8u, 0xFFFFu);
            ae_wr16(ram, AE_OVL186A_G_SLOT_MENUS + 6u, 0xFFFFu);
            ae_wr16(ram, AE_OVL186A_G_SLOT_MENUS + 4u, 0xFFFFu);
        }
        ae_wr16(ram, AE_OVL186A_G_MENU_ACTIVE, 0u);
        ae_ovl186a_build_file_entries(ram, hooks);
        ae_wr16(ram, AE_OVL186A_G_SLOT_SELECTION, 0u);
        break;
    }
    case 4:
    case 5:
    case 7:
    case 13:
        ae_wr16(ram, AE_OVL186A_G_MENU_ACTIVE, 1u);
        break;
    case 10:
        ae_wr16(ram, AE_OVL186A_G_CARD_CALLBACK_TIMER, 0u);
        break;
    case 11:
        ae_wr16(ram, AE_OVL186A_G_MENU_ACTIVE, 0u);
        ae_ovl186a_build_file_entries(ram, hooks);
        ae_wr16(ram, AE_OVL186A_G_SLOT_SELECTION, 0u);
        break;
    case 15:
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_NameEntryInit",
                               0x8013B550u, 0u, 0u, 0u, 0u, 0u);
        break;
    case 16:
        ae_wr16(ram, AE_OVL186A_G_MENU_ACTIVE, 0u);
        break;
    default:
        break;
    }
    return 0;
}

static void ae_ovl186a_memcard_op(AeRam *ram, const AeOverlayHooks *hooks,
                                 uint32_t operation)
{
    (void)ae_ovl186a_call(ram, hooks, "MemCardOp",
                          AE_OVL186A_API_MEMCARD_OP, 2u,
                          operation, 0u, 0u, 0u, 0u);
}

static int32_t ae_ovl186a_card_result_index(AeRam *ram)
{
    uint32_t result = ae_rd16(ram, AE_OVL186A_G_MEMCARD_STATE +
                                   AE_OVL186A_CARD_RESULT);
    return ae_ovl186a_sign16(result + 8u);
}

int32_t ae_SaveUi_OnCardResult_0(AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t index = ae_ovl186a_card_result_index(ram);
    switch (index) {
    case 0:
        ae_wr16(ram, AE_OVL186A_G_CARD_CALLBACK_FLAG, 1u);
        (void)ae_SaveUi_SetState(ram, hooks, 1, 2);
        break;
    case 1:
        ae_wr16(ram, AE_OVL186A_G_CARD_CALLBACK_FLAG, 0u);
        (void)ae_SaveUi_SetState(ram, hooks, 2, 0);
        break;
    case 3:
    case 4:
        ae_wr16(ram, AE_OVL186A_G_CARD_CALLBACK_FLAG, 0u);
        (void)ae_SaveUi_SetState(ram, hooks, 1, 2);
        break;
    case 6:
    case 7:
        ae_wr16(ram, AE_OVL186A_G_CARD_CALLBACK_FLAG, 0u);
        (void)ae_SaveUi_SetState(ram, hooks, 41, 0);
        break;
    case 8:
        ae_wr16(ram, AE_OVL186A_G_CARD_CALLBACK_FLAG, 0u);
        if (ae_rds16(ram, AE_OVL186A_G_CARD_MENU_MODE) == 0) {
            ae_ovl186a_memcard_op(ram, hooks, 1u);
        } else {
            (void)ae_SaveUi_SetState(ram, hooks, 1, 5);
        }
        break;
    default:
        break;
    }
    return 0;
}

int32_t ae_SaveUi_OnCardResult_1(AeRam *ram, const AeOverlayHooks *hooks)
{
    switch (ae_ovl186a_card_result_index(ram)) {
    case 1:
        (void)ae_SaveUi_SetState(ram, hooks, 2, 0);
        break;
    case 6:
    case 7:
        (void)ae_SaveUi_SetState(ram, hooks, 41, 0);
        break;
    default:
        break;
    }
    return 0;
}

int32_t ae_SaveUi_OnCardResult_2(AeRam *ram, const AeOverlayHooks *hooks)
{
    switch (ae_ovl186a_card_result_index(ram)) {
    case 0:
        (void)ae_SaveUi_SetState(ram, hooks, 4, 0);
        break;
    case 1:
        (void)ae_SaveUi_SetState(ram, hooks, 2, 0);
        break;
    case 2:
    case 3:
        break;
    case 4:
        (void)ae_SaveUi_SetState(ram, hooks, 9, 0);
        break;
    case 6:
    case 7:
        (void)ae_SaveUi_SetState(ram, hooks, 43, 0);
        break;
    case 8:
        (void)ae_SaveUi_SetState(ram, hooks, 8, 0);
        {
            int32_t slot = ae_rds16(ram, AE_OVL186A_G_CARD_MENU_SLOT);
            uint32_t file = AE_OVL186A_G_SAVE_FILE_IMAGES +
                            (uint32_t)(slot * AE_OVL186A_SAVE_SLOT_IMAGE_STRIDE);
            (void)ae_ovl186a_call(ram, hooks, "MemCardCheckFile",
                                  AE_OVL186A_API_MEMCARD_CHECK_FILE, 1u,
                                  file, 0u, 0u, 0u, 0u);
            ae_ovl186a_memcard_op(ram, hooks, 2u);
        }
        break;
    default:
        break;
    }
    return 0;
}

int32_t ae_SaveUi_OnCardResult_3(AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t state = ae_ovl186a_card_result_index(ram);
    switch (state) {
    case 0:
        (void)ae_SaveUi_SetState(ram, hooks, 4, 0);
        break;
    case 1:
        (void)ae_SaveUi_SetState(ram, hooks, 2, 0);
        break;
    case 2:
        break;
    case 3:
        ae_ovl186a_memcard_op(ram, hooks, 5u);
        break;
    case 4:
        (void)ae_SaveUi_SetState(ram, hooks, 9, 0);
        break;
    case 6:
    case 7:
        (void)ae_SaveUi_SetState(ram, hooks, 43, 0);
        break;
    case 8:
        if (ae_rds16(ram, AE_OVL186A_G_CARD_MENU_MODE) == 5) {
            int32_t slot = ae_rds16(ram, AE_OVL186A_G_CARD_MENU_SLOT);
            uint32_t slot_base = AE_OVL186A_G_SAVE_FILE_IMAGES +
                                 (uint32_t)(slot *
                                            AE_OVL186A_SAVE_SLOT_IMAGE_STRIDE);
            (void)ae_ovl186a_call(ram, hooks, "MemCardCheckFile",
                                  AE_OVL186A_API_MEMCARD_CHECK_FILE, 1u,
                                  slot_base, 0u, 0u, 0u, 0u);
            ae_ovl186a_memcard_op(ram, hooks, 2u);
        } else {
            (void)ae_SaveUi_SetState(ram, hooks, 11, 0);
        }
        break;
    default:
        break;
    }
    return 0;
}

int32_t ae_SaveUi_OnCardResult_4(AeRam *ram, const AeOverlayHooks *hooks)
{
    switch (ae_ovl186a_card_result_index(ram)) {
    case 0:
        break;
    case 1:
        (void)ae_SaveUi_SetState(ram, hooks, 2, 0);
        break;
    case 6:
    case 7:
        (void)ae_SaveUi_SetState(ram, hooks, 45, 0);
        break;
    case 8:
        (void)ae_SaveUi_SetState(ram, hooks, 1, 5);
        break;
    default:
        break;
    }
    return 0;
}

int32_t ae_SaveUi_OnCardResult_5(AeRam *ram, const AeOverlayHooks *hooks)
{
    switch (ae_ovl186a_card_result_index(ram)) {
    case 0:
        ae_wr16(ram, AE_OVL186A_G_CARD_CALLBACK_FLAG, 1u);
        (void)ae_SaveUi_SetState(ram, hooks, 1, 2);
        ae_ovl186a_memcard_op(ram, hooks, 7u);
        break;
    case 3:
        ae_wr16(ram, AE_OVL186A_G_CARD_CALLBACK_FLAG, 0u);
        (void)ae_SaveUi_SetState(ram, hooks, 1, 2);
        ae_ovl186a_memcard_op(ram, hooks, 7u);
        break;
    case 6:
    case 7:
        ae_wr16(ram, AE_OVL186A_G_CARD_CALLBACK_FLAG, 0u);
        (void)ae_SaveUi_SetState(ram, hooks, 41, 0);
        break;
    case 8:
        ae_wr16(ram, AE_OVL186A_G_CARD_CALLBACK_FLAG, 0u);
        (void)ae_SaveUi_SetState(ram, hooks, 1, 5);
        break;
    default:
        break;
    }
    return 0;
}

int32_t ae_SaveUi_OnCardResult_6(AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t timer = (int32_t)(uint16_t)(ae_rd16(ram, AE_OVL186A_G_CARD_CALLBACK_TIMER) + 1u);
    ae_wr16(ram, AE_OVL186A_G_CARD_CALLBACK_TIMER, (uint32_t)timer);
    if (ae_ovl186a_sign16((uint32_t)timer) < 3) return 0;

    ae_ovl186a_memcard_op(ram, hooks, 3u);
    uint32_t result = ae_rd16(ram, AE_OVL186A_G_MEMCARD_STATE +
                                   AE_OVL186A_CARD_RESULT);
    uint32_t result_plus_two = (result + 2u) & 0xFFFFu;
    if (result_plus_two < 2u) {
        (void)ae_SaveUi_SetState(ram, hooks, 42, 0);
    } else if (result != 0u) {
        (void)ae_SaveUi_SetState(ram, hooks, 2, 0);
    } else {
        ae_ovl186a_memcard_op(ram, hooks, 4u);
        (void)ae_SaveUi_SetState(ram, hooks, 6, 0);
    }
    return 0;
}

static void ae_ovl186a_draw_icon(AeRam *ram, const AeOverlayHooks *hooks,
                                 uint32_t icon, int32_t x, int32_t y)
{
    (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawIcon", 0x8013946Cu, 3u,
                           icon, (uint32_t)x, (uint32_t)y, 0u);
}

static int32_t ae_ovl186a_draw_slot_text(AeRam *ram,
                                         const AeOverlayHooks *hooks,
                                         uint32_t text, int32_t first,
                                         int32_t second, int32_t row)
{
    return ae_SaveUi_DrawText(ram, hooks, text, first, second, row);
}

void ae_SaveUi_DrawSlot(AeRam *ram, const AeOverlayHooks *hooks,
                        int32_t mode, int32_t slot)
{
    uint32_t slot_index = (uint32_t)slot;
    uint32_t entry = AE_OVL186A_G_FILE_ENTRIES +
                     slot_index * AE_OVL186A_FILE_ENTRY_STRIDE;
    int32_t y = mode == 0 ? 150 : 110;
    int32_t x = mode == 0
        ? (int32_t)(32u + slot_index * 48u)
        : 80;
    int32_t view_mode = ae_rds16(ram, AE_OVL186A_G_SLOT_VIEW_MODE);

    if (view_mode == 1) {
        ae_ovl186a_draw_icon(ram, hooks, 17u, y - 36, x);
        ae_ovl186a_draw_icon(ram, hooks, 16u, y - 36, x + 16);

        uint32_t cursor = ae_rd32(ram, AE_OVL186A_G_PRIMITIVE_CURSOR);
        (void)ae_ovl186a_render(ram, hooks, "SaveUi_SlotHeaderPacket", 4u,
                                cursor, (uint32_t)view_mode, slot_index,
                                (uint32_t)x, 0u);
        ae_wr32(ram, AE_OVL186A_G_PRIMITIVE_CURSOR,
                cursor + AE_OVL186A_DRAW_PACKET_HEADER_SIZE);

        (void)ae_ovl186a_draw_slot_text(
            ram, hooks, entry + AE_OVL186A_FILE_ENTRY_NAME_LINE_1,
            y + 30, x, 1);
        (void)ae_ovl186a_draw_slot_text(
            ram, hooks, entry + AE_OVL186A_FILE_ENTRY_NAME_LINE_2,
            y + 30, x + 16, 1);
        return;
    }

    if (view_mode == 2) {
        ae_ovl186a_draw_icon(ram, hooks, 15u, y - 36, x + 16);
        ae_ovl186a_draw_icon(ram, hooks, 18u, y + 52, x + 16);

        uint32_t flags = ae_rd32(ram, entry + AE_OVL186A_FILE_ENTRY_SAVE_FLAGS);
        static const struct {
            uint32_t flag;
            uint32_t icon;
            int32_t y_offset;
        } detail_icons[] = {
            {0x01u, 8u, -36}, {0x02u, 7u, -18}, {0x04u, 12u, 0},
            {0x08u, 11u, 18}, {0x10u, 9u, 36}, {0x40u, 14u, 54},
            {0x80u, 13u, 72}, {0x20u, 10u, 90}
        };
        for (uint32_t i = 0u;
             i < sizeof(detail_icons) / sizeof(detail_icons[0]); ++i) {
            if ((flags & detail_icons[i].flag) != 0u) {
                ae_ovl186a_draw_icon(ram, hooks, detail_icons[i].icon,
                                     y + detail_icons[i].y_offset, x);
            }
        }

        uint32_t cursor = ae_rd32(ram, AE_OVL186A_G_PRIMITIVE_CURSOR);
        (void)ae_ovl186a_render(ram, hooks, "SaveUi_SlotDetailPacket", 3u,
                                cursor,
                                slot_index * AE_OVL186A_FILE_ENTRY_STRIDE,
                                (uint32_t)x, 0u, 0u);
        ae_wr32(ram, AE_OVL186A_G_PRIMITIVE_CURSOR,
                cursor + AE_OVL186A_DRAW_PACKET_HEADER_SIZE);

        (void)ae_ovl186a_draw_slot_text(
            ram, hooks, entry + AE_OVL186A_FILE_ENTRY_DETAIL_VALUE,
            y + 72, x + 16, 1);
        (void)ae_ovl186a_draw_slot_text(
            ram, hooks, entry + AE_OVL186A_FILE_ENTRY_DETAIL_LABEL,
            y - 16, x + 16, 1);
        return;
    }

    int32_t last_used = ae_ovl186a_later(ram, hooks, "SaveUi_LastUsedEntry",
                                         0x801399E4u, 1u,
                                         slot_index, 0u, 0u, 0u);
    uint32_t name_index_address = 0x8013AF28u + (uint32_t)last_used;
    int32_t name_index = ae_rds8(ram, name_index_address);
    uint32_t name = ae_SaveUi_StringTableIndex(ram, 0x8013ADF0u, name_index);
    uint32_t saved_name = AE_OVL186A_G_SAVE_SLOT_RECORDS +
                          AE_OVL186A_SAVE_SLOT_NAME +
                          slot_index * AE_OVL186A_SAVE_SLOT_STRIDE;

    (void)ae_ovl186a_draw_slot_text(ram, hooks, saved_name, y - 46, x, 1);
    (void)ae_ovl186a_draw_slot_text(ram, hooks, name, y - 30, x + 16, 0);

    uint32_t score_text = entry + AE_OVL186A_FILE_ENTRY_NAME_SCORE_TEXT;
    int32_t text_width = ae_SaveUi_TextWidth(ram, score_text);
    int32_t aligned_y = (int32_t)((uint32_t)y -
                                  ((uint32_t)text_width - 126u));
    (void)ae_ovl186a_draw_slot_text(ram, hooks, score_text,
                                    aligned_y, x, 0);
}

void ae_SaveUi_DrawFileSelect(AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t selected_slot = ae_rds16(ram, AE_OVL186A_G_SLOT_SELECTION);
    (void)ae_ovl186a_later(ram, hooks, "CardMgr_DrawMenuItems",
                           0x8013FC2Cu, 2u, (uint32_t)selected_slot,
                           AE_OVL186A_G_SLOT_MENUS, 0u, 0u);

    for (uint32_t slot = 0u; slot < AE_OVL186A_DRAW_MENU_SLOT_COUNT; ++slot) {
        uint32_t save_slot = AE_OVL186A_G_SAVE_SLOT_RECORDS +
                             slot * AE_OVL186A_SAVE_SLOT_STRIDE;
        if (ae_rds16(ram, save_slot + AE_OVL186A_SAVE_SLOT_IMAGE_OFFSET) < 0) {
            (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawEmptySlotText",
                                   0x801400F4u, 1u, slot, 0u, 0u, 0u);
        } else {
            ae_SaveUi_DrawSlot(ram, hooks, 0, (int32_t)slot);
        }
    }

    int32_t page = ae_rds16(ram, AE_OVL186A_G_MODE_FLAGS);
    (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawFileHeader",
                           0x80140074u, 1u, (uint32_t)page, 0u, 0u, 0u);
    ae_ovl186a_draw_icon(ram, hooks, 0u, 25, 124);
    ae_ovl186a_draw_icon(ram, hooks, 1u, 45, 124);
    if (page == 1) {
        ae_ovl186a_draw_icon(ram, hooks, 2u, 25, 170);
        ae_ovl186a_draw_icon(ram, hooks, 3u, 45, 170);
    } else {
        ae_ovl186a_draw_icon(ram, hooks, 4u, 25, 170);
        ae_ovl186a_draw_icon(ram, hooks, 5u, 50, 170);
    }

    uint32_t cursor = ae_rd32(ram, AE_OVL186A_G_PRIMITIVE_CURSOR);
    uint32_t ot_index = ae_rd32(ram, AE_OVL186A_G_OT_INDEX);
    (void)ae_ovl186a_render(ram, hooks, "SaveUi_FilePanelPacket", 2u,
                            cursor, ot_index, 0u, 0u, 0u);
    ae_wr32(ram, AE_OVL186A_G_PRIMITIVE_CURSOR,
            cursor + AE_OVL186A_DRAW_PACKET_HEADER_SIZE);

    if (page == 1) {
        int32_t slot = ae_rds16(ram, AE_OVL186A_G_CARD_MENU_SLOT);
        int32_t portrait_y = (int32_t)(24u + (uint32_t)slot * 48u);
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawPortrait",
                               0x80139570u, 3u, 96u,
                               (uint32_t)portrait_y, 170u, 0u);
    }

    for (uint32_t slot = 0u; slot < AE_OVL186A_DRAW_MENU_SLOT_COUNT; ++slot) {
        uint32_t entry = AE_OVL186A_G_FILE_ENTRIES +
                         slot * AE_OVL186A_FILE_ENTRY_STRIDE;
        uint32_t save_slot = AE_OVL186A_G_SAVE_SLOT_RECORDS +
                             slot * AE_OVL186A_SAVE_SLOT_STRIDE;
        if (ae_rds16(ram, entry + AE_OVL186A_FILE_ENTRY_LAST_USED) < 0 ||
            ae_rds16(ram, save_slot + AE_OVL186A_SAVE_SLOT_IMAGE_OFFSET) < 0) {
            continue;
        }

        uint32_t cursor_now = ae_rd32(ram, AE_OVL186A_G_PRIMITIVE_CURSOR);
        (void)ae_ovl186a_render(ram, hooks, "SaveUi_FileSlotPacket", 4u,
                                cursor_now, slot, entry, save_slot, 0u);
        ae_wr32(ram, AE_OVL186A_G_PRIMITIVE_CURSOR,
                cursor_now + AE_OVL186A_DRAW_PACKET_SLOT_SIZE);
    }

    for (uint32_t slot = 0u; slot < AE_OVL186A_DRAW_DETAIL_ITEM_COUNT; ++slot) {
        uint32_t cursor_now = ae_rd32(ram, AE_OVL186A_G_PRIMITIVE_CURSOR);
        (void)ae_ovl186a_render(ram, hooks, "SaveUi_MonkeyListPacket", 3u,
                                cursor_now, slot, ot_index, 0u, 0u);
        ae_wr32(ram, AE_OVL186A_G_PRIMITIVE_CURSOR,
                cursor_now + AE_OVL186A_DRAW_PACKET_SLOT_SIZE);
    }

    (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawMainWindow",
                           0x80139A3Cu, 1u, 0u, 0u, 0u, 0u);
    (void)ae_ovl186a_later(ram, hooks, "SaveUi_DrawBackground3D",
                           0x80139A68u, 0u, 0u, 0u, 0u, 0u);
}

static void ae_ovl186a_card_state_thread_step(AeRam *ram,
                                               const AeOverlayHooks *hooks)
{
    int32_t state = ae_rds16(ram, AE_OVL186A_G_MENU_STATE);
    if (state < 0 || state >= 46) return;

    switch (state) {
    case 0:
        (void)ae_SaveUi_OnCardResult_0(ram, hooks);
        ae_ovl186a_memcard_op(ram, hooks, 4u);
        break;
    case 1:
    case 4:
    case 5:
    case 7:
    case 9:
    case 11:
    case 13:
    case 15:
    case 16:
        (void)ae_SaveUi_OnCardResult_1(ram, hooks);
        ae_ovl186a_memcard_op(ram, hooks, 4u);
        break;
    case 2:
        (void)ae_SaveUi_OnCardResult_5(ram, hooks);
        ae_ovl186a_memcard_op(ram, hooks, 4u);
        break;
    case 6:
        (void)ae_SaveUi_OnCardResult_2(ram, hooks);
        break;
    case 8:
        (void)ae_SaveUi_OnCardResult_3(ram, hooks);
        break;
    case 10:
        (void)ae_SaveUi_OnCardResult_6(ram, hooks);
        break;
    case 12:
        (void)ae_SaveUi_OnCardResult_4(ram, hooks);
        break;
    case 17:
        (void)ae_ovl186a_later(ram, hooks, "SaveUi_OnCardResult_7",
                               0x8013B400u, 0u, 0u, 0u, 0u, 0u);
        break;
    default:
        /* States 3, 14, and 18..45 return to the worker loop without dispatch. */
        break;
    }
}

void ae_SaveUi_CardStateThread(AeRam *ram, const AeOverlayHooks *hooks)
{
    for (;;) ae_ovl186a_card_state_thread_step(ram, hooks);
}

static void ae_ovl186a_card_result_thread_step(AeRam *ram,
                                                const AeOverlayHooks *hooks)
{
    int32_t index = ae_ovl186a_card_result_index(ram);
    if (index >= 0 && index < 10 &&
        (index == 0 || index == 1 || index == 3 || index == 4 ||
         index == 6 || index == 7 || index == 8)) {
        ae_wr16(ram, AE_OVL186A_G_CARD_CALLBACK_MODE, 0u);
    }
    ae_ovl186a_memcard_op(ram, hooks, 16u);
}

void ae_SaveUi_CardResultThread(AeRam *ram, const AeOverlayHooks *hooks)
{
    for (;;) ae_ovl186a_card_result_thread_step(ram, hooks);
}
