#include "ae_ovl186c.h"

#include "ae_overlay_layout.h"

enum {
    AE_ADDR_MEMCARD_OP = 0x80010000u,
    AE_ADDR_VSYNC = 0x8002E520u,
    AE_ADDR_PAD_GET_STATE = 0x800877F8u,
    AE_ADDR_TEXT_DRAW = 0x8008211Cu,
    AE_ADDR_MEMCARD_SYNC = 0x80021EC8u,
    AE_ADDR_ENTER_CRITICAL = 0x8002F200u,
    AE_ADDR_GET_CURRENT_THREAD = 0x8002F210u,
    AE_ADDR_SET_THREAD_HOOK = 0x8002F220u,
    AE_ADDR_EXIT_CRITICAL = 0x8002F230u,
    AE_ADDR_CLOSE_THREAD = 0x8002F240u,
    AE_ADDR_CHANGE_THREAD = 0x8002F250u,
    AE_ADDR_CREATE_THREAD = 0x8002F260u,
    AE_ADDR_OPEN_THREAD = 0x8002F270u,
    AE_ADDR_START_THREAD = 0x8002F280u,

    AE_ADDR_SAVE_UI_DRAW_TEXT = 0x80136000u,
    AE_ADDR_SAVE_UI_DRAW_ICON = 0x8013946Cu,
    AE_ADDR_SAVE_UI_DRAW_WINDOW = 0x80136160u,
    AE_ADDR_SAVE_UI_DRAW_WINDOW_BORDER = 0x80136554u,
    AE_ADDR_SAVE_UI_MAIN_WINDOW = 0x80139A3Cu,
    AE_ADDR_SAVE_UI_BACKGROUND = 0x80139A68u,
    AE_ADDR_SAVE_UI_YES_NO_UPDATE = 0x80139F08u,
    AE_ADDR_SAVE_UI_CONFIRM_PRESSED = 0x8013A400u,
    AE_ADDR_SAVE_UI_SET_STATE = 0x8013784Cu,
    AE_ADDR_SAVE_UI_FADE_TICK = 0x8013A634u,
    AE_ADDR_FRAME_SOUND = 0x8001325Cu,
    AE_ADDR_BEGIN_FRAME = 0x8005DF48u,
    AE_ADDR_CLEAR_IMAGE = 0x80028850u,
    AE_ADDR_DRAW_SYNC = 0x800286CCu,
    AE_ADDR_PUT_DRAW_ENV = 0x80028D30u,
    AE_ADDR_PUT_DISP_ENV = 0x80028EFCu,
    AE_ADDR_DRAW_OTAG = 0x80028CC0u,
    AE_ADDR_SAVE_UI_TEXT_PAIR = 0x8013FEA0u,

    AE_ADDR_MCX_CARD_TYPE = 0x801444D4u,
    AE_ADDR_MCX_CARD_ALL_INFO = 0x8014464Cu,
    AE_ADDR_MCX_SYNC = 0x8014236Cu,
    AE_ADDR_MCX_HIDE_TRANS = 0x80144084u,
    AE_ADDR_MCX_CARD_FORMAT = 0x80143F54u,
    AE_ADDR_MCX_COM_INIT = 0x80145374u,
    AE_ADDR_MCX_THREAD_READY = 0x801424E0u,
    AE_ADDR_MCX_RUN_OPERATION = 0x80141EBCu,
    AE_ADDR_MCX_INIT_STEP = 0x80141FF0u,
    AE_ADDR_MCX_INIT_SHUTDOWN = 0x80145440u
};

enum {
    AE_OVL186_PAD_ANALOG_MODE = 0x800E5488u,
    AE_OVL186_UI_TIMEOUT_LIMIT = 90u,
    AE_OVL186_UI_YES_NO_ACCEPT = 2u
};

static int32_t ae_ovl186_external(AeRam *ram, const AeOverlayHooks *hooks,
                                  const char *name, uint32_t address,
                                  uint32_t count, uint32_t a0, uint32_t a1,
                                  uint32_t a2, uint32_t a3, uint32_t a4,
                                  uint32_t a5, uint32_t a6)
{
    const uint32_t arguments[7] = {a0, a1, a2, a3, a4, a5, a6};
    return ae_overlay_native_call(ram, hooks, name, address, count, arguments);
}

#define EXT0(r, h, n, a) \
    ae_ovl186_external((r), (h), (n), (a), 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u)
#define EXT1(r, h, n, a, p0) \
    ae_ovl186_external((r), (h), (n), (a), 1u, (uint32_t)(p0), 0u, 0u, 0u, 0u, 0u, 0u)
#define EXT2(r, h, n, a, p0, p1) \
    ae_ovl186_external((r), (h), (n), (a), 2u, (uint32_t)(p0), (uint32_t)(p1), 0u, 0u, 0u, 0u, 0u)
#define EXT3(r, h, n, a, p0, p1, p2) \
    ae_ovl186_external((r), (h), (n), (a), 3u, (uint32_t)(p0), (uint32_t)(p1), (uint32_t)(p2), 0u, 0u, 0u, 0u)
#define EXT4(r, h, n, a, p0, p1, p2, p3) \
    ae_ovl186_external((r), (h), (n), (a), 4u, (uint32_t)(p0), (uint32_t)(p1), (uint32_t)(p2), (uint32_t)(p3), 0u, 0u, 0u)
#define EXT5(r, h, n, a, p0, p1, p2, p3, p4) \
    ae_ovl186_external((r), (h), (n), (a), 5u, (uint32_t)(p0), (uint32_t)(p1), (uint32_t)(p2), (uint32_t)(p3), (uint32_t)(p4), 0u, 0u)
#define EXT6(r, h, n, a, p0, p1, p2, p3, p4, p5) \
    ae_ovl186_external((r), (h), (n), (a), 6u, (uint32_t)(p0), (uint32_t)(p1), (uint32_t)(p2), (uint32_t)(p3), (uint32_t)(p4), (uint32_t)(p5), 0u)
#define EXT7(r, h, n, a, p0, p1, p2, p3, p4, p5, p6) \
    ae_ovl186_external((r), (h), (n), (a), 7u, (uint32_t)(p0), (uint32_t)(p1), (uint32_t)(p2), (uint32_t)(p3), (uint32_t)(p4), (uint32_t)(p5), (uint32_t)(p6))

#define AE_SLOT_ADDRESS(base, index) \
    ((base) + (uint32_t)(index) * AE_OVL186_SLOT_RECORD_SIZE)

static int32_t ae_ovl186_draw_blinking_menu(AeRam *ram,
                                           const AeOverlayHooks *hooks,
                                           uint32_t variant);
static int32_t ae_ovl186_draw_progress_bar(AeRam *ram,
                                          const AeOverlayHooks *hooks);
static int32_t ae_ovl186_draw_file_list(AeRam *ram,
                                       const AeOverlayHooks *hooks);
static int32_t ae_ovl186_draw_icon_anim(AeRam *ram,
                                       const AeOverlayHooks *hooks);
static int32_t ae_ovl186_draw_selection_bars(AeRam *ram,
                                             const AeOverlayHooks *hooks);

static int32_t ae_ovl186_draw_text(AeRam *ram, const AeOverlayHooks *hooks,
                                  uint32_t string, int32_t x, int32_t y,
                                  uint32_t texture_page_row)
{
    return EXT4(ram, hooks, "SaveUi_DrawText", AE_ADDR_SAVE_UI_DRAW_TEXT,
                string, (uint32_t)x, (uint32_t)y, texture_page_row);
}

static int32_t ae_ovl186_text_draw(AeRam *ram, const AeOverlayHooks *hooks,
                                  int32_t x, int32_t y, uint32_t width)
{
    return EXT3(ram, hooks, "TextDraw", 0x8008211Cu,
                (uint32_t)x, (uint32_t)y, width);
}

static int32_t ae_ovl186_render(AeRam *ram, const AeOverlayHooks *hooks,
                               const char *name, uint32_t count,
                               const uint32_t *values)
{
    return ae_overlay_render(ram, hooks, name, count, values);
}

static int32_t ae_ovl186_render_selection_bars(AeRam *ram,
                                               const AeOverlayHooks *hooks)
{
    const uint32_t values[] = {
        AE_OVL186_UI_CARD_DIRECTIONS, AE_OVL186_UI_ANIMATION_RECORDS,
        ae_rd16(ram, AE_OVL186_UI_STATE),
        ae_rd32(ram, AE_OVL186_UI_MENU_CHOICE)
    };
    (void)ae_ovl186_render(ram, hooks, "CardMgr_DrawSelectionBars", 4u, values);
    return ae_CardMgr_AnimTick(ram, hooks, 1u);
}

static int32_t ae_ovl186_render_file_list(AeRam *ram,
                                         const AeOverlayHooks *hooks)
{
    const uint32_t values[] = {
        AE_OVL186_UI_FILE_ROWS, AE_OVL186_UI_RENDER_METADATA,
        AE_OVL186_UI_PRIMARY_VALUES, AE_OVL186_UI_OPERATION_VALUES,
        ae_rd16(ram, AE_OVL186_UI_STATE)
    };
    return ae_ovl186_render(ram, hooks, "CardMgr_DrawFileList", 5u, values);
}

static int32_t ae_ovl186_render_icon_animation(AeRam *ram,
                                               const AeOverlayHooks *hooks)
{
    const uint32_t values[] = {
        AE_OVL186_UI_CARD_DIRECTIONS, AE_OVL186_UI_ANIMATION_RECORDS,
        ae_rd16(ram, AE_OVL186_UI_STATE)
    };
    return ae_ovl186_render(ram, hooks, "CardMgr_DrawIconAnim", 3u, values);
}

static int32_t ae_ovl186_render_progress_bar(AeRam *ram,
                                            const AeOverlayHooks *hooks)
{
    const uint32_t sync = (uint32_t)EXT1(ram, hooks, "VSync", AE_ADDR_VSYNC,
                                        0xFFFFFFFFu);
    const uint32_t values[] = {
        sync, ae_rd16(ram, AE_OVL186_UI_STATE),
        ae_rd32(ram, AE_OVL186_UI_OPERATION), AE_OVL186_UI_CARD_FRAMES
    };
    return ae_ovl186_render(ram, hooks, "CardMgr_DrawProgressBar", 4u, values);
}

static int32_t ae_ovl186_render_message_lines(AeRam *ram,
                                             const AeOverlayHooks *hooks,
                                             uint32_t line_count)
{
    const uint32_t values[] = {
        line_count, ae_rd32(ram, AE_OVL186_UI_MENU_CHOICE),
        AE_OVL186_UI_FILE_ROWS, ae_rd16(ram, AE_OVL186_UI_STATE)
    };
    return ae_ovl186_render(ram, hooks, "CardMgr_DrawMessageLines", 4u, values);
}

static int32_t ae_ovl186_draw_icon(AeRam *ram, const AeOverlayHooks *hooks,
                                  uint32_t icon, uint32_t x, uint32_t y)
{
    return EXT3(ram, hooks, "SaveUi_DrawIcon", AE_ADDR_SAVE_UI_DRAW_ICON,
                icon, x, y);
}

static int32_t ae_ovl186_draw_state_overlay(AeRam *ram,
                                            const AeOverlayHooks *hooks)
{
    return ae_SaveUi_DrawStateOverlay(ram, hooks);
}

static int32_t ae_ovl186_draw_window_border(AeRam *ram,
                                            const AeOverlayHooks *hooks,
                                            uint32_t x, uint32_t y,
                                            uint32_t width, uint32_t height)
{
    return EXT4(ram, hooks, "SaveUi_DrawWindowBorder",
                AE_ADDR_SAVE_UI_DRAW_WINDOW_BORDER, x, y, width, height);
}

static int32_t ae_ovl186_draw_window(AeRam *ram,
                                     const AeOverlayHooks *hooks,
                                     uint32_t x, uint32_t y,
                                     uint32_t width, uint32_t height)
{
    return EXT4(ram, hooks, "SaveUi_DrawWindow", AE_ADDR_SAVE_UI_DRAW_WINDOW,
                x, y, width, height);
}

static int32_t ae_ovl186_draw_main_and_background(AeRam *ram,
                                                  const AeOverlayHooks *hooks)
{
    (void)EXT1(ram, hooks, "SaveUi_DrawMainWindow", AE_ADDR_SAVE_UI_MAIN_WINDOW,
               4u);
    return EXT0(ram, hooks, "SaveUi_DrawBackground3D",
                AE_ADDR_SAVE_UI_BACKGROUND);
}

static int32_t ae_ovl186_draw_state_window(AeRam *ram,
                                           const AeOverlayHooks *hooks)
{
    (void)ae_ovl186_draw_state_overlay(ram, hooks);
    (void)ae_ovl186_draw_window_border(ram, hooks, 96u, 80u, 192u, 132u);
    return ae_CardMgr_Draw(ram, hooks);
}

static void ae_ovl186_set_manager_mode(AeRam *ram)
{
    ae_wr16(ram, AE_OVL186_CARD_MODE, 11u);
}

static int32_t ae_ovl186_scale_by_timeout(int32_t value, int32_t timeout)
{
    int32_t product = (int32_t)((uint32_t)value * (uint32_t)timeout);
    return product / 90;
}

int32_t ae_CardMgr_SetState(AeRam *ram, const AeOverlayHooks *hooks,
                            uint32_t state, uint32_t option)
{
    ae_wr16(ram, AE_OVL186_UI_STATE, state);

    if ((state - 14u) >= 36u) {
        return 0;
    }

    switch (state) {
    case 18u:
    case 30u:
    case 36u:
    case 38u:
        ae_ovl186_set_manager_mode(ram);
        break;
    case 21u:
        ae_wr32(ram, AE_OVL186_UI_CARD_FRAMES,
                (uint32_t)EXT1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0xFFFFFFFFu));
        break;
    case 24u:
    case 31u:
    case 40u:
        ae_wr16(ram, AE_OVL186_UI_STATE_FLAG, 1u);
        break;
    case 25u:
        ae_wr16(ram, AE_OVL186_UI_REPEAT_COUNT, 0u);
        break;
    case 26u:
    case 37u:
    case 39u:
        ae_wr32(ram, AE_OVL186_UI_POLL_TIMER, 0u);
        ae_wr16(ram, AE_OVL186_UI_STATE_FLAG, 1u);
        break;
    case 29u:
        if (option != 0u) break;
        {
            static const uint8_t result_offsets[] = {24u, 0u, 16u, 12u, 8u, 4u};
            for (uint32_t index = 0; index < 6u; ++index) {
                ae_wr16(ram, AE_OVL186_UI_PRIMARY_VALUES + 12u + index * 2u,
                        ae_rd16(ram, AE_OVL186_CARD_RESULT + result_offsets[index]));
            }
            ae_wr32(ram, AE_OVL186_UI_OPERATION, 0u);
        }
        break;
    case 32u:
        if (ae_rd32(ram, AE_OVL186_UI_OPERATION) == 0u) {
            uint32_t manager = ae_rd32(ram, AE_OVL186_CARD_CONTEXT);
            ae_wr16(ram, AE_OVL186_UI_PRIMARY_VALUES +
                        AE_OVL186_UI_PRIMARY_LIMIT_OFFSET, 0u);
            for (uint32_t word = 0; word < 6u; ++word) {
                ae_wr32(ram, manager + AE_OVL186_CONTEXT_SLOT_ARRAY_OFFSET +
                            word * AE_OVL186_CONTEXT_WORD_SIZE,
                        ae_rd32(ram, AE_OVL186_CARD_RESULT + word *
                                    AE_OVL186_CONTEXT_WORD_SIZE));
            }
            ae_wr32(ram, manager + AE_OVL186_CONTEXT_CARD_TYPE_OFFSET,
                    (uint32_t)ae_rds16(ram, AE_OVL186_UI_PRIMARY_VALUES));
            break;
        }

        for (uint32_t index = 0; index < AE_OVL186_UI_PRIMARY_VALUE_COUNT;
             ++index) {
            ae_wr16(ram, AE_OVL186_UI_ADJUSTED_VALUES + index * 2u,
                    ae_rd16(ram, AE_OVL186_UI_PRIMARY_VALUES + index * 2u));
        }
        {
            int32_t used = ae_rds16(ram, AE_OVL186_UI_PRIMARY_VALUES + 2u);
            int32_t reserve = ae_rds16(ram, AE_OVL186_UI_PRIMARY_VALUES + 14u);
            uint32_t limit = used + reserve < 101
                ? (uint32_t)reserve : (uint32_t)(100 - used);
            ae_wr16(ram, AE_OVL186_UI_OPERATION_VALUES + 2u, limit);
        }
        ae_wr16(ram, AE_OVL186_UI_OPERATION_VALUES + 14u,
                ae_rd16(ram, AE_OVL186_UI_OPERATION_VALUES + 2u));
        for (uint32_t index = 2u; index < 4u; ++index) {
            int32_t used = ae_rds16(ram,
                AE_OVL186_UI_PRIMARY_VALUES + index * 2u);
            int32_t free_count = ae_rds16(ram,
                AE_OVL186_UI_PRIMARY_VALUES + (index + 6u) * 2u);
            uint32_t next_free = used + free_count < 100
                ? (uint32_t)free_count : (uint32_t)(99 - used);
            ae_wr16(ram, AE_OVL186_UI_OPERATION_VALUES + index * 2u,
                    next_free);
            ae_wr16(ram, AE_OVL186_UI_OPERATION_VALUES + (index + 6u) * 2u,
                    ae_rd16(ram, AE_OVL186_UI_PRIMARY_VALUES +
                                (index + 6u) * 2u));
        }
        for (uint32_t index = 0; index < 2u; ++index) {
            int32_t primary_mid = ae_rds16(ram,
                AE_OVL186_UI_PRIMARY_VALUES + (index + 2u) * 2u);
            int32_t secondary_mid = ae_rds16(ram,
                AE_OVL186_UI_OPERATION_VALUES + (index + 2u) * 2u);
            int32_t primary_current = ae_rds16(ram,
                AE_OVL186_UI_PRIMARY_VALUES + (index + 4u) * 2u);
            int32_t primary_future = ae_rds16(ram,
                AE_OVL186_UI_PRIMARY_VALUES + (index + 10u) * 2u);
            int32_t current_total = primary_mid + secondary_mid;
            int32_t future_total = primary_current + primary_future;
            uint32_t next;
            if (current_total < future_total) {
                next = (uint32_t)(current_total - primary_current);
            } else if (future_total < 100) {
                next = ae_rd16(ram,
                    AE_OVL186_UI_PRIMARY_VALUES + (index + 10u) * 2u);
            } else {
                next = (uint32_t)(99 -
                    ae_rd16(ram, AE_OVL186_UI_PRIMARY_VALUES +
                                (index + 4u) * 2u));
            }
            ae_wr16(ram, AE_OVL186_UI_OPERATION_VALUES +
                        (index + 4u) * 2u, next);
            ae_wr16(ram, AE_OVL186_UI_OPERATION_VALUES +
                        (index + 10u) * 2u, next);
        }

        {
            uint32_t manager = ae_rd32(ram, AE_OVL186_CARD_CONTEXT);
            uint32_t output = manager + AE_OVL186_CONTEXT_SLOT_ARRAY_OFFSET;
            static const uint8_t result_offsets[] = {14u, 22u, 20u, 18u, 16u};
            for (uint32_t index = 0; index < 5u; ++index) {
                ae_wr32(ram, output + index * 4u,
                    (uint32_t)(ae_rds16(ram, AE_OVL186_UI_ADJUSTED_VALUES +
                                             result_offsets[index]) -
                               ae_rds16(ram, AE_OVL186_UI_OPERATION_VALUES +
                                             result_offsets[index])));
            }
            ae_wr32(ram, output + 20u,
                    ae_rd32(ram, AE_OVL186_CARD_RESULT + 20u));
            ae_wr32(ram, output + 24u,
                    ae_rd32(ram, AE_OVL186_CARD_RESULT + 24u));
        }
        break;
    case 34u: {
        uint32_t manager = ae_rd32(ram, AE_OVL186_CARD_CONTEXT);
        ae_wr32(ram, AE_OVL186_UI_TIMEOUT, 0u);
        if (ae_rd32(ram, AE_OVL186_UI_OPERATION) != 0u) {
            ae_wr16(ram, AE_OVL186_MENU_PACKET_BASE_A +
                        AE_OVL186_MENU_PACKET_A_PROGRESS_0,
                    ae_rd16(ram, AE_OVL186_MENU_PACKET_BASE_A +
                                AE_OVL186_MENU_PACKET_A_PROGRESS_0) +
                    ae_rd16(ram, AE_OVL186_UI_OPERATION_VALUES + 2u));
            ae_wr16(ram, AE_OVL186_MENU_PACKET_BASE_A +
                        AE_OVL186_MENU_PACKET_A_PROGRESS_1,
                    ae_rd16(ram, AE_OVL186_MENU_PACKET_BASE_A +
                                AE_OVL186_MENU_PACKET_A_PROGRESS_1) +
                    ae_rd16(ram, AE_OVL186_UI_OPERATION_VALUES + 4u));
            ae_wr16(ram, AE_OVL186_MENU_PACKET_BASE_A +
                        AE_OVL186_MENU_PACKET_A_PROGRESS_2,
                    ae_rd16(ram, AE_OVL186_MENU_PACKET_BASE_A +
                                AE_OVL186_MENU_PACKET_A_PROGRESS_2) +
                    ae_rd16(ram, AE_OVL186_UI_OPERATION_VALUES + 6u));
            ae_wr8(ram, AE_OVL186_MENU_PACKET_BASE_B +
                       AE_OVL186_MENU_PACKET_B_PROGRESS_0,
                   ae_rd8(ram, AE_OVL186_MENU_PACKET_BASE_B +
                               AE_OVL186_MENU_PACKET_B_PROGRESS_0) +
                   ae_rd8(ram, AE_OVL186_UI_OPERATION_VALUES + 8u));
            ae_wr8(ram, AE_OVL186_MENU_PACKET_BASE_B +
                       AE_OVL186_MENU_PACKET_B_PROGRESS_1,
                   ae_rd8(ram, AE_OVL186_MENU_PACKET_BASE_B +
                               AE_OVL186_MENU_PACKET_B_PROGRESS_1) +
                   ae_rd8(ram, AE_OVL186_UI_OPERATION_VALUES + 10u));
        }
        for (uint32_t word = 0; word < 7u; ++word) {
            ae_wr32(ram, AE_OVL186_CARD_RESULT + word *
                        AE_OVL186_CONTEXT_WORD_SIZE,
                    ae_rd32(ram, manager + AE_OVL186_CONTEXT_SLOT_ARRAY_OFFSET +
                                word * AE_OVL186_CONTEXT_WORD_SIZE));
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

int32_t ae_CardMgr_StartOps(AeRam *ram, const AeOverlayHooks *hooks)
{
    ae_wr16(ram, AE_OVL186_UI_STATE, 19u);
    (void)EXT2(ram, hooks, "MemCardOp", AE_ADDR_MEMCARD_OP, 8u, 0u);

    ae_wr32(ram, AE_OVL186_CARD_OPERATION_BUFFER, 0x80158000u);
    ae_wr16(ram, AE_OVL186_CARD_OPERATION, 1u);
    ae_wr16(ram, AE_OVL186_CARD_OPERATION_STATUS, 0u);
    (void)EXT2(ram, hooks, "MemCardOp", AE_ADDR_MEMCARD_OP, 10u, 0u);
    return 0;
}

int32_t ae_SsGetMute(AeRam *ram, const AeOverlayHooks *hooks)
{
    return ae_CardMgr_StateMachine(ram, hooks) != 0;
}

int32_t ae_CardMgr_IsBusy(AeRam *ram, const AeOverlayHooks *hooks)
{
    return ae_CardMgr_StateMachine(ram, hooks) != 0;
}

int32_t ae_McxCardTypeSync(AeRam *ram, const AeOverlayHooks *hooks)
{
    uint32_t event = AE_OVL186_UI_CALL_WORK + AE_OVL186_CALL_WORK_EVENT_OFFSET;
    uint32_t result = AE_OVL186_UI_CALL_WORK + AE_OVL186_CALL_WORK_RESULT_OFFSET;
    (void)EXT3(ram, hooks, "MemCardSync", AE_ADDR_MEMCARD_SYNC, 0u, event, result);
    (void)EXT1(ram, hooks, "McxCardType", AE_ADDR_MCX_CARD_TYPE, 0u);
    (void)EXT3(ram, hooks, "McxSync", AE_ADDR_MCX_SYNC, 0u, event, result);
    ae_wr16(ram, AE_OVL186_UI_BUSY, ae_rd32(ram, result) != 0u ? 1u : 0u);
    return 0;
}

int32_t ae_McxAllInfoSync(AeRam *ram, const AeOverlayHooks *hooks)
{
    if (ae_rds16(ram, AE_OVL186_UI_BUSY) != 0) {
        return 0;
    }

    uint32_t event = AE_OVL186_UI_CALL_WORK + AE_OVL186_CALL_WORK_EVENT_OFFSET;
    uint32_t result = AE_OVL186_UI_CALL_WORK + AE_OVL186_CALL_WORK_RESULT_OFFSET;
    uint32_t card_info = AE_OVL186_UI_CALL_WORK + AE_OVL186_CALL_WORK_CARD_INFO_OFFSET;
    (void)EXT3(ram, hooks, "MemCardSync", AE_ADDR_MEMCARD_SYNC, 0u, event, result);
    (void)EXT2(ram, hooks, "McxAllInfo", AE_ADDR_MCX_CARD_ALL_INFO, 0u, card_info);
    (void)EXT3(ram, hooks, "McxSync", AE_ADDR_MCX_SYNC, 0u, event, result);
    return ae_rds16(ram, card_info);
}

int32_t ae_CardMgr_ResetSlots(AeRam *ram, const AeOverlayHooks *hooks,
                              uint32_t reset_type)
{
    (void)hooks;
    if (reset_type != 0u) {
        uint32_t manager = ae_rd32(ram, AE_OVL186_CARD_CONTEXT);
        ae_wr32(ram, manager + AE_OVL186_CONTEXT_CARD_TYPE_OFFSET,
                (uint32_t)ae_rds16(ram, AE_OVL186_UI_PRIMARY_VALUES));
        for (uint32_t slot = 0; slot < AE_OVL186_CONTEXT_SLOT_COUNT; ++slot) {
            uint32_t address = manager + AE_OVL186_CONTEXT_SLOT_ARRAY_OFFSET +
                               slot * AE_OVL186_CONTEXT_WORD_SIZE;
            ae_wr32(ram, address, 0u);
        }
    }

    if (ae_rds16(ram, AE_OVL186_UI_BUSY) != 0) {
        return 0;
    }

    uint32_t event = AE_OVL186_UI_CALL_WORK + AE_OVL186_CALL_WORK_EVENT_OFFSET;
    uint32_t result = AE_OVL186_UI_CALL_WORK + AE_OVL186_CALL_WORK_RESULT_OFFSET;
    (void)EXT3(ram, hooks, "MemCardSync", AE_ADDR_MEMCARD_SYNC, 0u, event, result);
    (void)EXT3(ram, hooks, "McxShowTrans", 0x80143F54u, 0u, 1u, 60u);
    (void)EXT3(ram, hooks, "McxSync", AE_ADDR_MCX_SYNC, 0u, event, result);
    return 0;
}

int32_t ae_McxHideTransSync(AeRam *ram, const AeOverlayHooks *hooks,
                            uint32_t operation)
{
    (void)operation;
    if (ae_rds16(ram, AE_OVL186_UI_BUSY) != 0) {
        return 0;
    }

    uint32_t event = AE_OVL186_UI_CALL_WORK + AE_OVL186_CALL_WORK_EVENT_OFFSET;
    uint32_t result = AE_OVL186_UI_CALL_WORK + AE_OVL186_CALL_WORK_RESULT_OFFSET;
    (void)EXT3(ram, hooks, "MemCardSync", AE_ADDR_MEMCARD_SYNC, 0u, event, result);
    (void)EXT1(ram, hooks, "McxHideTrans", AE_ADDR_MCX_HIDE_TRANS,
               0u);
    (void)EXT3(ram, hooks, "McxSync", AE_ADDR_MCX_SYNC, 0u, event, result);
    return 0;
}

int32_t ae_CardMgr_MenuUpdate(AeRam *ram, const AeOverlayHooks *hooks)
{
    if (ae_rds16(ram, AE_OVL186_MENU_FADE_STATE) != 1) {
        return 0;
    }

    uint32_t pad_state = ae_rd32(ram, 0x800E5488u);
    if (pad_state == 0x1000u) {
        (void)EXT1(ram, hooks, "PadGetState", AE_ADDR_PAD_GET_STATE, 0u);
        uint32_t remaining = ae_rd32(ram, AE_OVL186_UI_OPERATION) - 1u;
        ae_wr32(ram, AE_OVL186_UI_OPERATION, remaining);
        if ((int32_t)remaining < 0) {
            ae_wr32(ram, AE_OVL186_UI_OPERATION, 1u);
        }
    }
    if (pad_state == 0x4000u) {
        (void)EXT1(ram, hooks, "PadGetState", AE_ADDR_PAD_GET_STATE, 0u);
        uint32_t remaining = ae_rd32(ram, AE_OVL186_UI_OPERATION) + 1u;
        ae_wr32(ram, AE_OVL186_UI_OPERATION, remaining);
        if (remaining >= 2u) {
            ae_wr32(ram, AE_OVL186_UI_OPERATION, 0u);
        }
    }

    uint32_t pressed = ae_rd32(ram, AE_OVERLAY_PAD_HELD) &
                       ae_rd32(ram, AE_OVERLAY_PAD_CHANGED);
    if ((pressed & 0x800u) != 0u) {
        (void)EXT1(ram, hooks, "PadGetState", AE_ADDR_PAD_GET_STATE, 1u);
        return 1;
    }
    if ((pressed & 0x20u) == 0u) {
        return 0;
    }

    (void)EXT1(ram, hooks, "PadGetState", AE_ADDR_PAD_GET_STATE, 3u);
    if (ae_rd32(ram, AE_OVL186_UI_OPERATION) != 0u) {
        (void)ae_CardMgr_SetState(ram, hooks, 40u, 0u);
        return 0;
    }
    uint32_t next_state = ae_rds16(ram, AE_OVL186_UI_PRIMARY_VALUES) >
                                  ae_rds16(ram, AE_OVL186_UI_PRIMARY_VALUES +
                                           AE_OVL186_UI_PRIMARY_LIMIT_OFFSET)
                              ? 31u : 49u;
    (void)ae_CardMgr_SetState(ram, hooks, next_state, 0u);
    return 0;
}

int32_t ae_CardMgr_AnimTick(AeRam *ram, const AeOverlayHooks *hooks,
                            uint32_t ignored)
{
    (void)hooks;
    (void)ignored;
    for (uint32_t index = 0; index < AE_OVL186_UI_DRAW_ANIM_COUNT; ++index) {
        uint32_t animation = AE_OVL186_UI_ANIMATION_RECORDS +
                             index * AE_OVL186_ANIM_RECORD_SIZE;
        int32_t timer = ae_rds16(ram, animation + AE_OVL186_ANIM_TIMER_OFFSET) + 1;
        ae_wr16(ram, animation + AE_OVL186_ANIM_TIMER_OFFSET, (uint32_t)timer);
        if (ae_rds16(ram, animation + AE_OVL186_ANIM_TIMER_LIMIT_OFFSET) <
            (int16_t)timer) {
            uint32_t frame = ae_rd16(ram, animation + AE_OVL186_ANIM_FRAME_INDEX_OFFSET);
            ae_wr16(ram, animation + AE_OVL186_ANIM_TIMER_OFFSET, 0u);
            frame++;
            ae_wr16(ram, animation + AE_OVL186_ANIM_FRAME_INDEX_OFFSET, frame);
            if ((int16_t)frame >= ae_rds16(ram,
                    animation + AE_OVL186_ANIM_LIMIT_OFFSET)) {
                frame = 0u;
                ae_wr16(ram, animation + AE_OVL186_ANIM_FRAME_INDEX_OFFSET, 0u);
            }
            uint32_t draw_index = (uint32_t)ae_rds16(ram,
                animation + AE_OVL186_ANIM_ICON_OFFSET);
            uint32_t draw_frame = (uint32_t)ae_rd16(ram,
                animation + AE_OVL186_ANIM_FRAME_COUNT_OFFSET) * frame +
                ae_rd16(ram, animation + AE_OVL186_ANIM_FRAME_BASE_OFFSET);
            uint32_t output = AE_OVL186_UI_CARD_DIRECTIONS +
                              draw_index * AE_OVL186_ANIM_DRAW_FRAME_STRIDE +
                              AE_OVL186_ANIM_DRAW_FRAME_OFFSET;
            ae_wr16(ram, output, draw_frame);
        }
    }
    return 0;
}

int32_t ae_CardMgr_Draw(AeRam *ram, const AeOverlayHooks *hooks)
{
    (void)ae_ovl186_draw_selection_bars(ram, hooks);
    (void)ae_ovl186_draw_file_list(ram, hooks);
    if (ae_rds16(ram, AE_OVL186_UI_STATE) == 29) {
        (void)ae_ovl186_draw_icon_anim(ram, hooks);
        (void)ae_SaveUi_DrawStringPair(ram, hooks);
    }
    (void)ae_ovl186_draw_window(ram, hooks, 12u, 20u, 352u, 94u);
    (void)ae_ovl186_draw_window(ram, hooks, 12u, 110u, 352u, 106u);
    (void)EXT0(ram, hooks, "SaveUi_DrawBackground3D",
               AE_ADDR_SAVE_UI_BACKGROUND);
    return 0;
}

int32_t ae_CardMgr_StateMachine(AeRam *ram, const AeOverlayHooks *hooks)
{
    int16_t state_delta = (int16_t)(ae_rd16(ram, AE_OVL186_UI_STATE) - 14u);
    if ((uint16_t)state_delta >= 36u) {
        return 0;
    }

    uint32_t state = (uint16_t)(state_delta + 14);
    switch (state) {
    case 14u:
        (void)ae_ovl186_draw_main_and_background(ram, hooks);
        return 1;

    case 18u:
    case 30u:
        if (ae_rds16(ram, AE_OVL186_CARD_MODE) != 11) {
            (void)ae_ovl186_draw_blinking_menu(ram, hooks, 0u);
            (void)ae_ovl186_draw_main_and_background(ram, hooks);
            return 0;
        }
        (void)ae_ovl186_draw_state_overlay(ram, hooks);
        (void)ae_ovl186_draw_main_and_background(ram, hooks);
        if (EXT0(ram, hooks, "SaveUi_ConfirmPressed",
                 AE_ADDR_SAVE_UI_CONFIRM_PRESSED) == 0) return 0;
        (void)EXT1(ram, hooks, "PadGetState", AE_ADDR_PAD_GET_STATE, 3u);
        return 1;

    case 19u:
    case 28u:
        (void)ae_ovl186_draw_blinking_menu(ram, hooks, 0u);
        (void)ae_ovl186_draw_main_and_background(ram, hooks);
        return 0;

    case 29u:
        (void)ae_CardMgr_Draw(ram, hooks);
        return ae_CardMgr_MenuUpdate(ram, hooks) != 0;

    case 20u:
    case 23u:
    case 36u:
    case 38u:
    case 41u:
    case 42u:
    case 46u:
    case 47u:
        (void)ae_ovl186_draw_state_overlay(ram, hooks);
        (void)ae_ovl186_draw_main_and_background(ram, hooks);
        if (EXT0(ram, hooks, "SaveUi_ConfirmPressed",
                 AE_ADDR_SAVE_UI_CONFIRM_PRESSED) == 0) {
            return 0;
        }
        (void)EXT1(ram, hooks, "PadGetState", AE_ADDR_PAD_GET_STATE, 3u);
        return 1;

    case 21u:
    case 48u:
        (void)ae_ovl186_draw_state_overlay(ram, hooks);
        (void)ae_ovl186_draw_progress_bar(ram, hooks);
        (void)ae_ovl186_draw_main_and_background(ram, hooks);
        return 0;

    case 24u: {
        (void)ae_ovl186_draw_state_overlay(ram, hooks);
        int32_t choice = EXT0(ram, hooks, "SaveUi_YesNoUpdate",
                              AE_ADDR_SAVE_UI_YES_NO_UPDATE);
        (void)ae_ovl186_draw_main_and_background(ram, hooks);
        if (choice == 0) return 0;
        if (choice == AE_OVL186_UI_YES_NO_ACCEPT) return 1;
        (void)ae_CardMgr_SetState(ram, hooks, 25u, 0u);
        return 0;
    }

    case 25u:
        (void)ae_ovl186_draw_state_overlay(ram, hooks);
        (void)ae_ovl186_draw_main_and_background(ram, hooks);
        return 0;

    case 26u:
    case 37u:
    case 39u: {
        (void)ae_ovl186_draw_state_overlay(ram, hooks);
        int32_t choice = EXT0(ram, hooks, "SaveUi_YesNoUpdate",
                              AE_ADDR_SAVE_UI_YES_NO_UPDATE);
        (void)ae_ovl186_draw_main_and_background(ram, hooks);
        if (choice == 0 || ae_rd32(ram, AE_OVL186_UI_POLL_TIMER) != 0u) {
            return 0;
        }
        if (choice == AE_OVL186_UI_YES_NO_ACCEPT) return 1;
        ae_wr32(ram, AE_OVL186_UI_POLL_TIMER, 1u);
        return 0;
    }

    case 31u:
    case 40u: {
        (void)ae_ovl186_draw_state_overlay(ram, hooks);
        int32_t choice = EXT0(ram, hooks, "SaveUi_YesNoUpdate",
                      AE_ADDR_SAVE_UI_YES_NO_UPDATE);
        (void)ae_ovl186_draw_window_border(ram, hooks, 96u, 80u, 192u, 132u);
        (void)ae_CardMgr_Draw(ram, hooks);
        if (choice == 0) return 0;
        if (choice == AE_OVL186_UI_YES_NO_ACCEPT) {
            (void)ae_CardMgr_SetState(ram, hooks, 29u, 1u);
        } else {
            (void)ae_CardMgr_SetState(ram, hooks, 32u, 0u);
        }
        return 0;
    }

    case 32u:
    case 33u:
        (void)ae_ovl186_draw_state_window(ram, hooks);
        return 0;

    case 34u: {
        uint32_t timeout = ae_rd32(ram, AE_OVL186_UI_TIMEOUT) + 1u;
        ae_wr32(ram, AE_OVL186_UI_TIMEOUT, timeout);
        if ((int32_t)timeout > (int32_t)AE_OVL186_UI_TIMEOUT_LIMIT) {
            (void)ae_CardMgr_SetState(ram, hooks, 29u, 1u);
            (void)ae_ovl186_draw_state_window(ram, hooks);
            return 0;
        }
        if (ae_rd32(ram, AE_OVL186_UI_OPERATION) == 0u) {
            int32_t used = ae_rds16(ram, AE_OVL186_UI_PRIMARY_VALUES);
            ae_wr16(ram, AE_OVL186_UI_PRIMARY_VALUES +
                        AE_OVL186_UI_PRIMARY_LIMIT_OFFSET,
                    (uint32_t)ae_ovl186_scale_by_timeout(used,
                                                         (int32_t)timeout));
        } else {
            for (uint32_t index = 1u; index < 6u; ++index) {
                int32_t per_frame = ae_rds16(ram,
                    AE_OVL186_UI_SECONDARY_VALUES + index * 2u);
                int32_t added = ae_ovl186_scale_by_timeout(per_frame,
                                                            (int32_t)timeout);
                uint32_t displayed = ae_rd16(ram,
                    AE_OVL186_UI_ADJUSTED_VALUES + index * 2u);
                ae_wr16(ram, AE_OVL186_UI_PRIMARY_VALUES + index * 2u,
                        displayed + (uint32_t)added);
            }
            for (uint32_t index = 7u; index < AE_OVL186_UI_PRIMARY_VALUE_COUNT;
                 ++index) {
                int32_t per_frame = ae_rds16(ram,
                    AE_OVL186_UI_SECONDARY_VALUES + index * 2u);
                int32_t removed = ae_ovl186_scale_by_timeout(per_frame,
                                                              (int32_t)timeout);
                uint32_t remaining = ae_rd16(ram,
                    AE_OVL186_UI_ADJUSTED_VALUES + index * 2u);
                ae_wr16(ram, AE_OVL186_UI_PRIMARY_VALUES + index * 2u,
                        remaining - (uint32_t)removed);
            }
        }
        (void)ae_ovl186_draw_state_window(ram, hooks);
        return 0;
    }

    case 49u:
        (void)ae_ovl186_draw_state_window(ram, hooks);
        if (EXT0(ram, hooks, "SaveUi_ConfirmPressed",
                 AE_ADDR_SAVE_UI_CONFIRM_PRESSED) != 0) {
            (void)EXT1(ram, hooks, "PadGetState", AE_ADDR_PAD_GET_STATE, 3u);
            (void)ae_CardMgr_SetState(ram, hooks, 29u, 1u);
        }
        return 0;

    default:
        return 0;
    }
}

int32_t ae_CardMgr_DrawMenuLabels(AeRam *ram, const AeOverlayHooks *hooks,
                                  uint32_t menu)
{
    uint32_t selected = AE_OVL186_MENU_LABELS + menu * AE_OVL186_TEXT_ENTRY_SIZE;
    (void)ae_ovl186_draw_text(ram, hooks, selected,
        ae_rds16(ram, selected - 4u), ae_rds16(ram, selected - 2u), 1u);

    if (menu == 0u) {
        const uint32_t first = 0x801414E8u;
        (void)ae_ovl186_draw_text(ram, hooks, first,
            ae_rds16(ram, first - 4u), ae_rds16(ram, first - 2u), 0u);
        (void)ae_ovl186_draw_text(ram, hooks, first + AE_OVL186_TEXT_ENTRY_SIZE,
            ae_rds16(ram, first + AE_OVL186_TEXT_ENTRY_SIZE - 4u),
            ae_rds16(ram, first + AE_OVL186_TEXT_ENTRY_SIZE - 2u), 0u);
    } else if (menu == 1u) {
        const uint32_t text = 0x80141530u;
        (void)ae_ovl186_draw_text(ram, hooks, text,
            ae_rds16(ram, text - 4u), ae_rds16(ram, text - 2u), 0u);
    } else if (menu == 2u) {
        const uint32_t text = 0x80141554u;
        (void)ae_ovl186_draw_text(ram, hooks, text,
            ae_rds16(ram, text - 4u), ae_rds16(ram, text - 2u), 0u);
    }
    return 0;
}

int32_t ae_CardMgr_DrawBlinkingMenu(AeRam *ram,
                                    const AeOverlayHooks *hooks,
                                    uint32_t variant)
{
    const uint32_t first = variant == 1u ? 0u : 42u;
    const uint32_t frame = ae_rd32(ram, AE_OVL186_UI_FRAME_NUMBER);
    const int32_t visible_phase = (int32_t)(frame % 20u);

    for (uint32_t row = 0; row < 3u; ++row) {
        uint32_t record = AE_OVL186_STATE_TEXT_RECORDS +
            (first + row) * AE_OVL186_STATE_TEXT_RECORD_STRIDE;
        int32_t x = ae_rds16(ram, record + AE_OVL186_STATE_TEXT_X_OFFSET);
        if (x == -2 && visible_phase >= 11) continue;
        (void)ae_ovl186_draw_text(ram, hooks,
            record + AE_OVL186_STATE_TEXT_STRING_OFFSET, x,
            ae_rds16(ram, record + AE_OVL186_STATE_TEXT_Y_OFFSET), 0u);
    }
    return 0;
}

int32_t ae_SaveUi_DrawStateOverlay(AeRam *ram,
                                   const AeOverlayHooks *hooks)
{
    int32_t state = ae_rds16(ram, AE_OVL186_UI_STATE);
    if (state < (int32_t)AE_OVL186_STATE_TEXT_FIRST_STATE ||
        state > (int32_t)AE_OVL186_STATE_TEXT_LAST_STATE) {
        return 0;
    }

    uint32_t dispatch_index = (uint32_t)(state - 2);
    uint32_t handler = ae_rd32(ram, AE_OVL186_STATE_OVERLAY_DISPATCH_TABLE +
        dispatch_index * 4u);
    if (handler != AE_OVL186_STATE_OVERLAY_DRAW_HANDLER) return 0;

    uint32_t descriptor = AE_OVL186_STATE_TEXT_DESCRIPTORS +
        (uint32_t)state *
            AE_OVL186_STATE_TEXT_DESCRIPTOR_SIZE;
    int32_t first = ae_rds16(ram, descriptor);
    int32_t line_count = ae_rds16(ram,
        descriptor + AE_OVL186_STATE_TEXT_Y_OFFSET);
    if (line_count <= 0) return 0;

    for (int32_t line = 0; line < line_count; ++line) {
        uint32_t record = AE_OVL186_STATE_TEXT_RECORDS +
            (uint32_t)(first + line) * AE_OVL186_STATE_TEXT_RECORD_STRIDE;
        int32_t x = ae_rds16(ram, record + AE_OVL186_STATE_TEXT_X_OFFSET);
        int32_t y = ae_rds16(ram, record + AE_OVL186_STATE_TEXT_Y_OFFSET);
        uint32_t text = record + AE_OVL186_STATE_TEXT_STRING_OFFSET;
        if (x == -2 && ae_rd32(ram, AE_OVL186_UI_FRAME_NUMBER) % 20u >= 11u) {
            continue;
        }
        if (x != -3) {
            (void)ae_ovl186_draw_text(ram, hooks, text, x, y, 0u);
            continue;
        }

        int32_t card_variant = ae_rds16(ram, AE_OVL186_CARD_TEXT_VARIANT);
        uint32_t dynamic_text = AE_OVL186_CARD_TEXT_BUFFER +
            (uint32_t)(card_variant * 1952);
        int32_t dynamic_width = EXT1(ram, hooks, "SaveUi_TextWidth",
                                     0x80136120u, dynamic_text);
        int32_t static_width = EXT1(ram, hooks, "SaveUi_TextWidth",
                                    0x80136120u, text);
        int32_t centered_x = 192 - (dynamic_width + static_width) / 2;
        (void)ae_ovl186_draw_text(ram, hooks, dynamic_text,
                                  centered_x, y, 0u);
        dynamic_width = EXT1(ram, hooks, "SaveUi_TextWidth",
                             0x80136120u, dynamic_text);
        (void)ae_ovl186_draw_text(ram, hooks, text,
                                  centered_x + dynamic_width, y, 0u);
    }
    return 0;
}

uint32_t ae_SaveUi_GetColorBuffer(AeRam *ram, const AeOverlayHooks *hooks)
{
    (void)ram;
    (void)hooks;
    return AE_OVL186_UI_COLOR_BUFFER;
}

int32_t ae_CardMgr_DrawMenuItems(AeRam *ram, const AeOverlayHooks *hooks,
                                 uint32_t selected, uint32_t enabled)
{
    for (uint32_t row = 0; row < 5u; ++row) {
        if (ae_rds16(ram, enabled + row * 2u) == 1) {
            uint32_t text = AE_OVL186_MENU_ITEM_LABELS + row *
                            AE_OVL186_TEXT_ENTRY_SIZE;
            uint32_t coordinates = AE_OVL186_MENU_ITEM_LABELS - 4u + row *
                                  AE_OVL186_TEXT_ENTRY_SIZE;
            uint32_t highlighted = 0u;
            if (row == selected) {
                text = AE_OVL186_MENU_ITEM_LABELS + selected *
                       AE_OVL186_TEXT_ENTRY_SIZE;
                coordinates = text - 4u;
                highlighted = 1u;
            }
            (void)ae_ovl186_draw_text(ram, hooks, text,
                ae_rds16(ram, coordinates), ae_rds16(ram, coordinates + 2u),
                highlighted);
        }
    }

    uint32_t selected_item = AE_OVL186_MENU_ITEM_LABELS - 4u + selected *
                             AE_OVL186_TEXT_ENTRY_SIZE;
    return ae_ovl186_text_draw(ram, hooks,
        ae_rds16(ram, selected_item) + 28,
        ae_rds16(ram, selected_item + 2u), 80u);
}

uint32_t ae_SaveUi_GetBuffer2(AeRam *ram, const AeOverlayHooks *hooks)
{
    (void)ram;
    (void)hooks;
    return AE_OVL186_UI_BUFFER_2;
}

int32_t ae_CardMgr_DrawMenuTexts(AeRam *ram, const AeOverlayHooks *hooks,
                                 uint32_t enabled)
{
    for (uint32_t row = 0; row < 7u; ++row) {
        if (ae_rds16(ram, enabled + row * 2u) == 1) {
            uint32_t text = AE_OVL186_MENU_TEXT_LABELS + row *
                            AE_OVL186_TEXT_ENTRY_SIZE;
            uint32_t coordinates = AE_OVL186_MENU_TEXT_COORDINATES + row *
                                   AE_OVL186_TEXT_ENTRY_SIZE;
            (void)ae_ovl186_draw_text(ram, hooks, text,
                ae_rds16(ram, coordinates), ae_rds16(ram, coordinates + 2u),
                0u);
        }
    }
    return 0;
}

int32_t ae_CardMgr_DrawMenuItems2(AeRam *ram, const AeOverlayHooks *hooks,
                                  uint32_t selected, uint32_t enabled)
{
    for (uint32_t row = 0; row < 7u; ++row) {
        if (ae_rds16(ram, enabled + row * 2u) == 1) {
            uint32_t text = AE_OVL186_MENU_TEXT_LABELS_2 + row *
                            AE_OVL186_TEXT_ENTRY_SIZE;
            uint32_t coordinates = AE_OVL186_MENU_TEXT_STATUS + row *
                                  AE_OVL186_TEXT_ENTRY_SIZE;
            uint32_t highlighted = 0u;
            if (row == selected) {
                text = AE_OVL186_MENU_TEXT_LABELS_2 + selected *
                       AE_OVL186_TEXT_ENTRY_SIZE;
                coordinates = AE_OVL186_MENU_TEXT_STATUS + selected *
                              AE_OVL186_TEXT_ENTRY_SIZE;
                highlighted = 1u;
            }
            (void)ae_ovl186_draw_text(ram, hooks, text,
                ae_rds16(ram, coordinates), ae_rds16(ram, coordinates + 2u),
                highlighted);
        }
    }

    return 0;
}

int32_t ae_SaveUi_DrawStringPair(AeRam *ram, const AeOverlayHooks *hooks)
{
    const uint32_t first = 0x801418FCu;
    (void)ae_ovl186_draw_text(ram, hooks, first,
        ae_rds16(ram, first - 4u), ae_rds16(ram, first - 2u), 0u);
    (void)ae_ovl186_draw_text(ram, hooks, first + AE_OVL186_TEXT_ENTRY_SIZE,
        ae_rds16(ram, first + AE_OVL186_TEXT_ENTRY_SIZE - 4u),
        ae_rds16(ram, first + AE_OVL186_TEXT_ENTRY_SIZE - 2u), 0u);
    return 0;
}

int32_t ae_SaveUi_DrawYesNo(AeRam *ram, const AeOverlayHooks *hooks,
                            uint32_t selected)
{
    const uint32_t first = 0x80141434u;
    uint32_t first_highlighted = selected < 1u ? 1u : 0u;
    (void)ae_ovl186_draw_text(ram, hooks, first,
        ae_rds16(ram, first - 4u), ae_rds16(ram, first - 2u),
        first_highlighted);
    (void)ae_ovl186_draw_text(ram, hooks, first + AE_OVL186_TEXT_ENTRY_SIZE,
        ae_rds16(ram, first + AE_OVL186_TEXT_ENTRY_SIZE - 4u),
        ae_rds16(ram, first + AE_OVL186_TEXT_ENTRY_SIZE - 2u), selected);
    int32_t y = ae_rds16(ram, first - 2u) + (int32_t)(selected * 20u);
    return ae_ovl186_text_draw(ram, hooks, 192, y, 76u);
}

int32_t ae_SaveUi_DrawFileHeader(AeRam *ram, const AeOverlayHooks *hooks,
                                 uint32_t page)
{
    const uint32_t first = 0x80141650u;
    (void)ae_ovl186_draw_text(ram, hooks, first,
        ae_rds16(ram, first - 4u), ae_rds16(ram, first - 2u), 0u);
    uint32_t second = first + (page == 1u ? 2u : 1u) *
                      AE_OVL186_TEXT_ENTRY_SIZE;
    (void)ae_ovl186_draw_text(ram, hooks, second,
        ae_rds16(ram, second - 4u), ae_rds16(ram, second - 2u), 0u);
    return 0;
}

int32_t ae_SaveUi_DrawEmptySlotText(AeRam *ram, const AeOverlayHooks *hooks,
                                    uint32_t slot)
{
    uint32_t line_offset = slot * 48u;
    uint32_t first = 0x801416BCu;
    ae_wr8(ram, first + 4u, 107u + slot);
    (void)ae_ovl186_draw_text(ram, hooks, first,
        ae_rds16(ram, first - 4u),
        ae_rds16(ram, first - 2u) + (int32_t)line_offset, 1u);
    uint32_t second = first + AE_OVL186_TEXT_ENTRY_SIZE;
    (void)ae_ovl186_draw_text(ram, hooks, second,
        ae_rds16(ram, second - 4u),
        ae_rds16(ram, second - 2u) + (int32_t)line_offset, 0u);
    return 0;
}

static int32_t ae_ovl186_draw_blinking_menu(AeRam *ram,
                                           const AeOverlayHooks *hooks,
                                           uint32_t variant)
{
    return ae_CardMgr_DrawBlinkingMenu(ram, hooks, variant);
}

static int32_t ae_ovl186_draw_progress_bar(AeRam *ram,
                                          const AeOverlayHooks *hooks)
{
    return ae_ovl186_render_progress_bar(ram, hooks);
}

static int32_t ae_ovl186_draw_file_list(AeRam *ram,
                                       const AeOverlayHooks *hooks)
{
    return ae_ovl186_render_file_list(ram, hooks);
}

static int32_t ae_ovl186_draw_icon_anim(AeRam *ram,
                                       const AeOverlayHooks *hooks)
{
    return ae_ovl186_render_icon_animation(ram, hooks);
}

static int32_t ae_ovl186_draw_message_lines(AeRam *ram,
                                            const AeOverlayHooks *hooks,
                                            uint32_t line_count)
{
    return ae_ovl186_render_message_lines(ram, hooks, line_count);
}

static int32_t ae_ovl186_draw_selection_bars(AeRam *ram,
                                             const AeOverlayHooks *hooks)
{
    return ae_ovl186_render_selection_bars(ram, hooks);
}

static int32_t ae_ovl186_draw_message_overlay(AeRam *ram,
                                              const AeOverlayHooks *hooks)
{
    const uint32_t values[] = {
        ae_rd32(ram, AE_OVL186_UI_MENU_CHOICE), AE_OVL186_UI_FILE_ROWS,
        AE_OVL186_UI_RENDER_METADATA,
        ae_rd16(ram, AE_OVL186_UI_STATE)
    };
    (void)ae_ovl186_render(ram, hooks, "CardMgr_DrawMessageFrame", 4u, values);
    (void)ae_ovl186_draw_message_lines(ram, hooks,
        ae_rd32(ram, AE_OVL186_UI_MENU_CHOICE) - 2u);
    (void)EXT1(ram, hooks, "SaveUi_DrawMainWindow", AE_ADDR_SAVE_UI_MAIN_WINDOW,
               4u);
    return EXT0(ram, hooks, "SaveUi_DrawBackground3D",
                AE_ADDR_SAVE_UI_BACKGROUND);
}

int32_t ae_CardMgr_DrawMessage(AeRam *ram, const AeOverlayHooks *hooks,
                               uint32_t ignored_context)
{
    (void)ignored_context;
    uint32_t choice = ae_rd32(ram, AE_OVL186_UI_MENU_CHOICE);
    (void)ae_CardMgr_DrawMenuItems2(ram, hooks, choice,
                                    AE_OVL186_UI_MENU_CHOICE + 4u);
    (void)ae_ovl186_draw_icon(ram, hooks, 0u, 254u, 200u);
    (void)ae_ovl186_draw_icon(ram, hooks, 1u, 274u, 200u);
    (void)ae_ovl186_draw_message_overlay(ram, hooks);

    if (ae_rds16(ram, AE_OVL186_MENU_FADE_STATE) != 1) return 0;
    uint32_t analog_mode = ae_rd32(ram, AE_OVL186_PAD_ANALOG_MODE);
    if (analog_mode == 0x1000u) {
        (void)EXT1(ram, hooks, "PadGetState", AE_ADDR_PAD_GET_STATE, 0u);
        choice = ae_rd32(ram, AE_OVL186_UI_MENU_CHOICE) - 1u;
        ae_wr32(ram, AE_OVL186_UI_MENU_CHOICE, choice);
        if ((int32_t)choice < 2) {
            choice = 4u;
            ae_wr32(ram, AE_OVL186_UI_MENU_CHOICE, choice);
        }
    }
    if (analog_mode == 0x4000u) {
        (void)EXT1(ram, hooks, "PadGetState", AE_ADDR_PAD_GET_STATE, 0u);
        choice = ae_rd32(ram, AE_OVL186_UI_MENU_CHOICE) + 1u;
        ae_wr32(ram, AE_OVL186_UI_MENU_CHOICE, choice);
        if ((int32_t)choice >= 5) {
            choice = 2u;
            ae_wr32(ram, AE_OVL186_UI_MENU_CHOICE, choice);
        }
    }
    if (EXT0(ram, hooks, "SaveUi_ConfirmPressed",
             AE_ADDR_SAVE_UI_CONFIRM_PRESSED) == 0) return 0;
    if (choice == 2u) {
        (void)EXT1(ram, hooks, "PadGetState", AE_ADDR_PAD_GET_STATE, 3u);
        return -1;
    }
    if (choice == 3u) {
        (void)EXT1(ram, hooks, "PadGetState", AE_ADDR_PAD_GET_STATE, 3u);
        return 1;
    }
    if (choice == 4u) {
        (void)EXT1(ram, hooks, "PadGetState", AE_ADDR_PAD_GET_STATE, 3u);
        return 2;
    }
    return 0;
}

static uint32_t ae_ovl186_card_result_target(AeRam *ram, uint32_t callback)
{
    int32_t entry = (int16_t)(ae_rd16(ram, AE_OVL186_CARD_OPERATION) + 8u);
    if ((uint32_t)entry >= 10u) return 0u;
    uint32_t table = AE_OVL186_MCX_CALLBACK_TABLE + callback * 0x28u;
    return ae_rd32(ram, table + (uint32_t)entry * 4u);
}

static int32_t ae_ovl186_card_type_transition(AeRam *ram,
                                             const AeOverlayHooks *hooks,
                                             uint32_t busy_state,
                                             uint32_t ready_state)
{
    (void)ae_McxCardTypeSync(ram, hooks);
    uint32_t next_state = ae_rds16(ram, AE_OVL186_UI_BUSY) != 0
        ? busy_state : ready_state;
    return ae_CardMgr_SetState(ram, hooks, next_state, 0u);
}

static int32_t ae_ovl186_memcard_op(AeRam *ram, const AeOverlayHooks *hooks,
                                    uint32_t operation)
{
    return EXT2(ram, hooks, "MemCardOp", AE_ADDR_MEMCARD_OP, operation, 0u);
}

int32_t ae_CardMgr_OnCardResult_0(AeRam *ram,
                                  const AeOverlayHooks *hooks)
{
    switch (ae_ovl186_card_result_target(ram, 0u)) {
    case 0x8013D878u:
        (void)ae_ovl186_card_type_transition(ram, hooks, 36u, 37u);
        break;
    case 0x8013D89Cu:
        if (ae_rds16(ram, AE_OVL186_CARD_MODE) == 10) {
            (void)ae_ovl186_memcard_op(ram, hooks, 12u);
        } else {
            (void)ae_ovl186_card_type_transition(ram, hooks, 36u, 37u);
        }
        break;
    case 0x8013D8C0u:
        (void)ae_ovl186_card_type_transition(ram, hooks, 36u, 39u);
        break;
    case 0x8013D8E4u:
        (void)ae_CardMgr_SetState(ram, hooks, 18u, 0u);
        break;
    case 0x8013D8ECu:
        (void)ae_ovl186_card_type_transition(ram, hooks, 36u, 24u);
        break;
    case 0x8013D910u:
        (void)ae_CardMgr_SetState(ram, hooks, 41u, 0u);
        break;
    default:
        break;
    }
    return 0;
}

int32_t ae_CardMgr_OnCardResult_1(AeRam *ram,
                                  const AeOverlayHooks *hooks)
{
    switch (ae_ovl186_card_result_target(ram, 1u)) {
    case 0x8013D970u:
        if (ae_rds16(ram, AE_OVL186_CARD_MODE) == 10) {
            (void)ae_ovl186_memcard_op(ram, hooks, 12u);
        } else {
            (void)ae_ovl186_card_type_transition(ram, hooks, 36u, 29u);
        }
        break;
    case 0x8013D9B8u:
        (void)ae_CardMgr_SetState(ram, hooks, 18u, 0u);
        break;
    case 0x8013D9C0u:
        (void)ae_ovl186_card_type_transition(ram, hooks, 36u, 30u);
        break;
    case 0x8013D9E4u:
        (void)ae_CardMgr_SetState(ram, hooks, 41u, 0u);
        break;
    default:
        break;
    }
    return 0;
}

int32_t ae_CardMgr_OnCardResult_2(AeRam *ram,
                                  const AeOverlayHooks *hooks)
{
    if (ae_ovl186_card_result_target(ram, 2u) == 0x8013DA44u &&
        ae_rds16(ram, AE_OVL186_UI_STATE) != 38) {
        (void)ae_CardMgr_SetState(ram, hooks, 18u, 0u);
    }
    return 0;
}

int32_t ae_CardMgr_OnCardResult_3(AeRam *ram,
                                  const AeOverlayHooks *hooks)
{
    switch (ae_ovl186_card_result_target(ram, 3u)) {
    case 0x8013DAC0u:
        (void)ae_ovl186_card_type_transition(ram, hooks, 36u, 37u);
        break;
    case 0x8013DAE4u:
        (void)ae_ovl186_card_type_transition(ram, hooks, 36u, 39u);
        break;
    case 0x8013DB08u:
        (void)ae_ovl186_card_type_transition(ram, hooks, 36u, 24u);
        break;
    case 0x8013DB2Cu:
        (void)ae_CardMgr_SetState(ram, hooks, 41u, 0u);
        break;
    default:
        break;
    }
    return 0;
}

int32_t ae_CardMgr_OnCardResult_4(AeRam *ram,
                                  const AeOverlayHooks *hooks)
{
    switch (ae_ovl186_card_result_target(ram, 4u)) {
    case 0x8013DB8Cu:
        (void)ae_ovl186_card_type_transition(ram, hooks, 36u, 29u);
        break;
    case 0x8013DBB0u:
        (void)ae_ovl186_card_type_transition(ram, hooks, 36u, 30u);
        break;
    case 0x8013DBD4u:
        (void)ae_CardMgr_SetState(ram, hooks, 41u, 0u);
        break;
    default:
        break;
    }
    return 0;
}

int32_t ae_CardMgr_OnCardResult_5(AeRam *ram,
                                  const AeOverlayHooks *hooks)
{
    switch (ae_ovl186_card_result_target(ram, 5u)) {
    case 0x8013DC34u:
        (void)ae_McxHideTransSync(ram, hooks, 0u);
        (void)ae_CardMgr_SetState(ram, hooks, 38u, 0u);
        break;
    case 0x8013DC44u:
        (void)ae_McxHideTransSync(ram, hooks, 0xFFFFFFFCu);
        (void)ae_CardMgr_SetState(ram, hooks, 23u, 0u);
        break;
    case 0x8013DC54u:
        (void)ae_CardMgr_SetState(ram, hooks, 18u, 0u);
        break;
    case 0x8013DC5Cu:
        (void)ae_CardMgr_SetState(ram, hooks, 46u, 0u);
        break;
    default:
        break;
    }
    return 0;
}

static int32_t ae_ovl186_finish_slot_scan(AeRam *ram,
                                          const AeOverlayHooks *hooks)
{
    (void)ae_McxCardTypeSync(ram, hooks);
    if (ae_rds16(ram, AE_OVL186_UI_BUSY) != 0) {
        (void)ae_CardMgr_SetState(ram, hooks, 36u, 0u);
        return 1;
    }
    if (ae_McxAllInfoSync(ram, hooks) != 0) {
        (void)ae_CardMgr_SetState(ram, hooks, 20u, 0u);
        return 1;
    }
    (void)ae_CardMgr_ResetSlots(ram, hooks, 1u);
    return 0;
}

static int32_t ae_ovl186_transition_after_slot_scan(
    AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t operation = ae_rds16(ram, AE_OVL186_CARD_OPERATION);
    if (operation == 0) {
        ae_wr16(ram, AE_OVL186_CARD_OPERATION, 1u);
        return ae_CardMgr_SetState(ram, hooks, 21u, 0u);
    }
    if (operation == -4 || operation == -7) {
        (void)ae_McxHideTransSync(ram, hooks, (uint32_t)operation);
        return ae_CardMgr_SetState(ram, hooks,
            operation == -4 ? 23u : 18u, 0u);
    }
    if ((uint16_t)(operation + 2) < 2u) {
        (void)ae_McxHideTransSync(ram, hooks, 0xFFFFFFFFu);
        return ae_CardMgr_SetState(ram, hooks, 46u, 0u);
    }
    return ae_CardMgr_SetState(ram, hooks, 21u, 0u);
}

int32_t ae_CardMgr_OnCardResult_6(AeRam *ram,
                                  const AeOverlayHooks *hooks)
{
    if (ae_ovl186_card_result_target(ram, 6u) != 0x8013DCC0u ||
        ae_rd32(ram, AE_OVL186_UI_POLL_TIMER) == 0u) {
        return 0;
    }
    if (ae_rds16(ram, AE_OVL186_UI_STATE) == 39) {
        (void)ae_CardMgr_SetState(ram, hooks, 26u, 0u);
        return 0;
    }
    (void)ae_McxCardTypeSync(ram, hooks);
    if (ae_rds16(ram, AE_OVL186_UI_BUSY) != 0) {
        (void)ae_CardMgr_SetState(ram, hooks, 36u, 0u);
        return 0;
    }
    if (ae_McxAllInfoSync(ram, hooks) != 0) {
        (void)ae_CardMgr_SetState(ram, hooks, 20u, 0u);
        return 0;
    }
    (void)ae_CardMgr_ResetSlots(ram, hooks, 1u);
    if (ae_rds16(ram, AE_OVL186_UI_STATE) != 37) {
        (void)ae_ovl186_memcard_op(ram, hooks, 14u);
        (void)ae_CardMgr_SetState(ram, hooks, 21u, 0u);
        return 0;
    }
    (void)ae_CardMgr_SetState(ram, hooks, 48u, 0u);
    (void)ae_ovl186_memcard_op(ram, hooks, 13u);
    int32_t operation = ae_rds16(ram, AE_OVL186_CARD_OPERATION);
    if (operation == 0) {
        ae_wr16(ram, AE_OVL186_CARD_OPERATION, 1u);
        (void)ae_CardMgr_SetState(ram, hooks, 21u, 0u);
    } else if (operation == -4) {
        (void)ae_McxHideTransSync(ram, hooks, (uint32_t)operation);
        (void)ae_CardMgr_SetState(ram, hooks, 23u, 0u);
    } else if (operation == -7) {
        (void)ae_McxHideTransSync(ram, hooks, (uint32_t)operation);
        (void)ae_CardMgr_SetState(ram, hooks, 18u, 0u);
    } else if ((uint16_t)(operation + 2) < 2u) {
        (void)ae_McxHideTransSync(ram, hooks, 0xFFFFFFFFu);
        (void)ae_CardMgr_SetState(ram, hooks, 46u, 0u);
    } else {
        (void)ae_CardMgr_SetState(ram, hooks, 21u, 0u);
    }
    return 0;
}

int32_t ae_CardMgr_OnCardResult_7(AeRam *ram,
                                  const AeOverlayHooks *hooks)
{
    switch (ae_ovl186_card_result_target(ram, 7u)) {
    case 0x8013DE34u:
        (void)ae_McxCardTypeSync(ram, hooks);
        if (ae_rds16(ram, AE_OVL186_UI_BUSY) != 0) {
            (void)ae_CardMgr_SetState(ram, hooks, 36u, 0u);
            break;
        }
        if (ae_McxAllInfoSync(ram, hooks) != 0) {
            (void)ae_CardMgr_SetState(ram, hooks, 20u, 0u);
            break;
        }
        if (ae_rd32(ram, AE_OVL186_UI_OPERATION) == 0u) {
            (void)ae_CardMgr_ResetSlots(ram, hooks, 0u);
        }
        (void)ae_ovl186_memcard_op(ram, hooks, 15u);
        (void)ae_CardMgr_SetState(ram, hooks, 33u, 0u);
        break;
    case 0x8013DE90u:
        (void)ae_CardMgr_SetState(ram, hooks, 18u, 0u);
        break;
    case 0x8013DE98u:
        (void)ae_CardMgr_SetState(ram, hooks, 47u, 0u);
        break;
    default:
        break;
    }
    return 0;
}

int32_t ae_CardMgr_OnCardResult_8(AeRam *ram,
                                  const AeOverlayHooks *hooks)
{
    switch (ae_ovl186_card_result_target(ram, 8u)) {
    case 0x8013DEF8u:
        if (ae_rd32(ram, AE_OVL186_UI_OPERATION) == 0u) {
            (void)ae_McxHideTransSync(ram, hooks, 0u);
        }
        (void)ae_CardMgr_SetState(ram, hooks, 34u, 0u);
        break;
    case 0x8013DF1Cu:
        (void)ae_CardMgr_SetState(ram, hooks, 18u, 0u);
        break;
    case 0x8013DF24u:
        (void)ae_CardMgr_SetState(ram, hooks, 47u, 0u);
        break;
    default:
        break;
    }
    return 0;
}

int32_t ae_CardMgr_OnCardResult_9(AeRam *ram,
                                  const AeOverlayHooks *hooks)
{
    uint32_t retries = (ae_rd16(ram, AE_OVL186_UI_REPEAT_COUNT) + 1u) & 0xFFFFu;
    ae_wr16(ram, AE_OVL186_UI_REPEAT_COUNT, retries);
    if ((int16_t)retries < 3) return 0;

    (void)ae_ovl186_memcard_op(ram, hooks, 3u);
    int32_t operation = ae_rds16(ram, AE_OVL186_CARD_OPERATION);
    if ((uint16_t)(operation + 2) < 2u) {
        (void)EXT2(ram, hooks, "SaveUi_SetState", AE_ADDR_SAVE_UI_SET_STATE,
                   42u, 0u);
        return 0;
    }
    if (operation != 0) {
        (void)ae_CardMgr_SetState(ram, hooks, 18u, 0u);
        return 0;
    }
    if (ae_ovl186_finish_slot_scan(ram, hooks) != 0) return 0;
    (void)ae_CardMgr_SetState(ram, hooks, 48u, 0u);
    (void)ae_ovl186_memcard_op(ram, hooks, 13u);
    (void)ae_ovl186_transition_after_slot_scan(ram, hooks);
    return 0;
}

int32_t ae_CardMgr_RunMessage(AeRam *ram, const AeOverlayHooks *hooks)
{
    const uint32_t display_state = AE_OVL186_UI_FRAME_NUMBER;
    ae_wr8(ram, AE_OVL186_FRAME_RESET_BYTE, 1u);
    ae_wr8(ram, AE_OVL186_FRAME_DRAW_BYTE, 1u);
    for (uint32_t byte = 1u; byte <= 3u; ++byte) {
        ae_wr8(ram, AE_OVL186_FRAME_DRAW_BYTE + byte, 0u);
        ae_wr8(ram, AE_OVL186_FRAME_SECONDARY_BYTE + byte - 1u, 0u);
    }
    ae_wr16(ram, AE_OVL186_UI_STATE_AUX, 30u);
    ae_wr16(ram, AE_OVL186_MENU_FADE_STATE, 0u);
    ae_wr32(ram, display_state + AE_OVL186_UI_FRAME_NUMBER_OFFSET, 0u);
    ae_wr32(ram, AE_OVL186_UI_MENU_CHOICE, 2u);

    /* This original function leaves $s1 undefined until DrawMessage selects an
     * answer. The platform supplies the caller's saved value through this named
     * boundary so the early-fade return matches the actual caller. */
    int32_t selected = EXT0(ram, hooks, "CardMgr_RunMessage_InitialS1",
                            0x8013F630u);
    for (;;) {
        (void)EXT1(ram, hooks, "PlaySoundEffect", AE_ADDR_FRAME_SOUND, 2u);
        (void)EXT0(ram, hooks, "BeginFrame", AE_ADDR_BEGIN_FRAME);
        int32_t fade = EXT0(ram, hooks, "SaveUi_FadeTick",
                            AE_ADDR_SAVE_UI_FADE_TICK);
        if (fade < 0) break;

        int32_t draw_result = ae_CardMgr_DrawMessage(
            ram, hooks, AE_OVL186_UI_CALL_WORK + 0x100u);
        if (draw_result != 0) {
            selected = draw_result;
            ae_wr16(ram, AE_OVL186_MENU_FADE_STATE, 2u);
        }

        (void)EXT1(ram, hooks, "DrawSync", AE_ADDR_DRAW_SYNC, 0u);
        (void)EXT1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0u);
        uint32_t display_index = ae_rd32(ram,
            display_state + AE_OVL186_UI_FRAME_NUMBER_OFFSET);
        uint32_t frame = ae_rd32(ram, AE_OVL186_UI_FRAME_NUMBER) + 1u;
        ae_wr32(ram, AE_OVL186_UI_FRAME_NUMBER, frame);
        uint32_t frame_offset = display_index * 0x4078u;
        (void)EXT1(ram, hooks, "PutDispEnv", AE_ADDR_PUT_DISP_ENV,
            display_state + 0x6Cu + frame_offset);
        (void)EXT1(ram, hooks, "PutDrawEnv", AE_ADDR_PUT_DRAW_ENV,
            display_state + 0x10u + frame_offset);
        (void)EXT1(ram, hooks, "DrawOTag", AE_ADDR_DRAW_OTAG,
            display_state + 0x80u + frame_offset + 0x3FFCu);
    }

    (void)EXT4(ram, hooks, "ClearImage", AE_ADDR_CLEAR_IMAGE,
               AE_OVL186_RUNMESSAGE_CLEAR_IMAGE_LOCAL, 0u, 0u, 0u);
    (void)EXT1(ram, hooks, "DrawSync", AE_ADDR_DRAW_SYNC, 0u);
    if (selected >= 0) {
        ae_wr32(ram, AE_OVL186_UI_MESSAGE_RESULT, (uint32_t)selected);
    }
    return selected < 0 ? -1 : 0;
}

int32_t ae_SaveUi_ThreadSetHook(AeRam *ram, const AeOverlayHooks *hooks,
                                uint32_t function)
{
    int32_t current_thread = EXT0(ram, hooks, "EnterCriticalSection",
                                  AE_ADDR_ENTER_CRITICAL);
    (void)EXT0(ram, hooks, "GetCurrentThread", AE_ADDR_GET_CURRENT_THREAD);
    int32_t thread_handle = EXT3(ram, hooks, "SetThreadHook",
        AE_ADDR_SET_THREAD_HOOK, function, AE_OVL186_THREAD_HOOK_CONTEXT,
        (uint32_t)current_thread);
    ae_wr32(ram, AE_OVL186_UI_THREAD_HANDLE, (uint32_t)thread_handle);
    (void)EXT0(ram, hooks, "ExitCriticalSection", AE_ADDR_EXIT_CRITICAL);

    ae_wr16(ram, AE_OVL186_UI_THREAD_STATE, 1u);
    uint32_t thread_table = ae_rd32(ram, AE_OVL186_UI_THREAD_TABLE);
    uint32_t thread_index = ae_rd16(ram, AE_OVL186_UI_THREAD_HANDLE);
    uint32_t record = ae_rd32(ram, thread_table + 16u) +
                      thread_index * AE_OVL186_MCX_THREAD_RECORD_SIZE;
    ae_wr32(ram, record + AE_OVL186_MCX_THREAD_RECORD_FLAGS, 1028u);

    uint32_t entry_cell = ae_rd32(ram, thread_table + 8u);
    ae_wr32(ram, AE_OVL186_UI_THREAD_ENTRY_CELL, entry_cell);
    ae_wr32(ram, AE_OVL186_UI_THREAD_ENTRY, ae_rd32(ram, entry_cell));
    return 0;
}

int32_t ae_SaveUi_ThreadClose(AeRam *ram, const AeOverlayHooks *hooks)
{
    ae_wr16(ram, AE_OVL186_UI_THREAD_STATE, 0u);
    (void)EXT0(ram, hooks, "GetCurrentThread", AE_ADDR_GET_CURRENT_THREAD);
    (void)EXT1(ram, hooks, "CloseThread", AE_ADDR_CLOSE_THREAD,
               ae_rd32(ram, AE_OVL186_UI_THREAD_HANDLE));
    (void)EXT0(ram, hooks, "ExitCriticalSection", AE_ADDR_EXIT_CRITICAL);
    return 0;
}

int32_t ae_SaveUi_Nop(AeRam *ram, const AeOverlayHooks *hooks)
{
    (void)hooks;
    ae_wr16(ram, AE_OVL186_UI_THREAD_STATE, 0u);
    return 0;
}

int32_t ae_SaveUi_ThreadSwitch(AeRam *ram, const AeOverlayHooks *hooks)
{
    if (ae_rds16(ram, AE_OVL186_UI_THREAD_STATE) != 0) {
        (void)EXT1(ram, hooks, "ChangeThread", AE_ADDR_CHANGE_THREAD,
                   ae_rd32(ram, AE_OVL186_UI_THREAD_HANDLE));
    }
    return 0;
}

int32_t ae_SaveUi_ThreadSetHookReset(AeRam *ram,
                                     const AeOverlayHooks *hooks)
{
    (void)hooks;
    if (ae_rds16(ram, AE_OVL186_UI_THREAD_STATE) != 0) {
        ae_wr32(ram, ae_rd32(ram, AE_OVL186_UI_THREAD_ENTRY_CELL),
                ae_rd32(ram, AE_OVL186_UI_THREAD_ENTRY));
    }
    return 0;
}

int32_t ae_McxStartCom(AeRam *ram, const AeOverlayHooks *hooks)
{
    (void)EXT0(ram, hooks, "GetCurrentThread", AE_ADDR_GET_CURRENT_THREAD);
    (void)EXT2(ram, hooks, "CreateThread", AE_ADDR_CREATE_THREAD,
               2u, AE_OVL186_MCX_THREAD_CONFIG);
    (void)ae_MCXINIT_OBJ_B0(ram, hooks, 2u, AE_OVL186_MCX_THREAD_CONFIG);

    uint32_t thread_node = ae_rd32(ram, AE_OVL186_MCX_THREAD_LIST);
    ae_wr32(ram, thread_node + AE_OVL186_MCX_NODE_HEAD_OFFSET, 0xFFFFFFFEu);
    ae_wr32(ram, thread_node + AE_OVL186_MCX_NODE_FLAGS_OFFSET,
            ae_rd32(ram, thread_node + AE_OVL186_MCX_NODE_FLAGS_OFFSET) | 1u);
    (void)EXT2(ram, hooks, "OpenThread", AE_ADDR_OPEN_THREAD, 3u, 0u);
    (void)EXT0(ram, hooks, "ExitCriticalSection", AE_ADDR_EXIT_CRITICAL);
    return 0;
}

int32_t ae_MCXINIT_OBJ_B0(AeRam *ram, const AeOverlayHooks *hooks,
                          uint32_t index, uint32_t callback)
{
    (void)hooks;
    uint32_t callback_array = ae_rd32(ram,
        ae_rd32(ram, AE_OVL186_MCX_CALLBACK_ROOT));
    uint32_t entry = callback_array + index * AE_OVL186_MCX_THREAD_LINK_STRIDE;
    uint32_t tail = ae_rd32(ram, entry);
    uint32_t return_value;
    if (tail == 0u) {
        ae_wr32(ram, entry, callback);
        return_value = entry;
    } else {
        while (ae_rd32(ram, tail + AE_OVL186_MCX_LINK_NEXT_OFFSET) != 0u) {
            tail = ae_rd32(ram, tail + AE_OVL186_MCX_LINK_NEXT_OFFSET);
        }
        ae_wr32(ram, tail + AE_OVL186_MCX_LINK_NEXT_OFFSET, callback);
        return_value = 0u;
    }
    (void)ae_MCXINIT_OBJ_FC(ram, hooks, index, callback);
    return (int32_t)return_value;
}

int32_t ae_MCXINIT_OBJ_FC(AeRam *ram, const AeOverlayHooks *hooks,
                          uint32_t unused, uint32_t object)
{
    (void)unused;
    (void)hooks;
    ae_wr32(ram, object + AE_OVL186_MCX_LINK_NEXT_OFFSET, 0u);
    return 0;
}

int32_t ae_MCXINIT_OBJ_104(AeRam *ram, const AeOverlayHooks *hooks)
{
    (void)hooks;
    uint32_t node = ae_rd32(ram, AE_OVL186_MCX_THREAD_LIST);
    if ((ae_rd32(ram, node + AE_OVL186_MCX_NODE_FLAGS_OFFSET) & 1u) == 0u) {
        return 0;
    }
    return (ae_rd32(ram, node + AE_OVL186_MCX_NODE_HEAD_OFFSET) & 1u) != 0u;
}

int32_t ae_MCXINIT_OBJ_144(AeRam *ram, const AeOverlayHooks *hooks)
{
    (void)hooks;
    uint32_t node = ae_rd32(ram, AE_OVL186_MCX_THREAD_LIST);
    if ((ae_rd32(ram, node + AE_OVL186_MCX_NODE_FLAGS_OFFSET) & 0x80u) == 0u) {
        return 0;
    }
    return (ae_rd32(ram, node + AE_OVL186_MCX_NODE_HEAD_OFFSET) & 0x80u) != 0u;
}

int32_t ae_MCXINIT_OBJ_184(AeRam *ram, const AeOverlayHooks *hooks)
{
    uint32_t request = ae_rd32(ram, AE_OVL186_MCX_INIT_REQUEST);
    if (request == 0u) return ae_MCXINIT_OBJ_2BC(ram, hooks);

    uint32_t mode = ae_rd32(ram, AE_OVL186_MCX_INIT_MODE);
    uint32_t node = ae_rd32(ram, AE_OVL186_MCX_THREAD_LIST);
    if (mode == 17u) {
        uint32_t retry = ae_rd32(ram, AE_OVL186_MCX_INIT_RETRY) + 1u;
        ae_wr32(ram, AE_OVL186_MCX_INIT_RETRY, retry);
        if (retry < 3u) {
            ae_wr32(ram, AE_OVL186_MCX_INIT_RESULT, 17u);
            (void)EXT1(ram, hooks, "McxCheckThreadReady",
                       AE_ADDR_MCX_THREAD_READY,
                       ae_rd32(ram, AE_OVL186_MCX_INIT_ARGUMENT));
            ae_wr32(ram, AE_OVL186_MCX_INIT_RESULT, 2u);
            uint32_t objects = ae_rd32(ram, AE_OVL186_MCX_OBJECTS);
            ae_wr16(ram, objects + AE_OVL186_MCX_OBJECT_READY_OFFSET, 0u);
            ae_wr32(ram, node + AE_OVL186_MCX_NODE_HEAD_OFFSET, 0xFFFFFF7Fu);
            ae_wr32(ram, node + AE_OVL186_MCX_NODE_FLAGS_OFFSET,
                    ae_rd32(ram, node + AE_OVL186_MCX_NODE_FLAGS_OFFSET) &
                        0xFFFFFF7Fu);
            (void)EXT2(ram, hooks, "CreateThread", AE_ADDR_CREATE_THREAD,
                       1u, AE_OVL186_MCX_START_CONFIG);
            ae_wr32(ram, AE_OVL186_MCX_INIT_REQUEST, 0u);
            return ae_MCXINIT_OBJ_2BC(ram, hooks);
        }
        ae_wr32(ram, AE_OVL186_MCX_INIT_RESULT, 17u);
    } else {
        ae_wr32(ram, AE_OVL186_MCX_INIT_RESULT, 17u);
    }

    ae_wr32(ram, AE_OVL186_MCX_INIT_REQUEST, 0u);
    ae_wr32(ram, AE_OVL186_MCX_INIT_RETRY, 0u);
    ae_wr32(ram, node + AE_OVL186_MCX_NODE_HEAD_OFFSET, 0xFFFFFF7Fu);
    ae_wr32(ram, node + AE_OVL186_MCX_NODE_FLAGS_OFFSET,
            ae_rd32(ram, node + AE_OVL186_MCX_NODE_FLAGS_OFFSET) &
                0xFFFFFF7Fu);
    (void)EXT2(ram, hooks, "CreateThread", AE_ADDR_CREATE_THREAD,
               1u, AE_OVL186_MCX_START_CONFIG);

    uint32_t objects = ae_rd32(ram, AE_OVL186_MCX_OBJECTS);
    ae_wr16(ram, objects + AE_OVL186_MCX_OBJECT_READY_OFFSET, 0u);
    ae_wr16(ram, objects + AE_OVL186_MCX_OBJECT_READY_OFFSET, 64u);
    ae_wr16(ram, objects + AE_OVL186_MCX_OBJECT_READY_DETAIL_OFFSET, 136u);
    ae_wr16(ram, objects + AE_OVL186_MCX_OBJECT_CONFIG_OFFSET, 13u);
    ae_wr16(ram, objects + AE_OVL186_MCX_OBJECT_READY_OFFSET, 0u);
    return 0;
}

int32_t ae_MCXINIT_OBJ_2BC(AeRam *ram, const AeOverlayHooks *hooks)
{
    if ((ae_rd32(ram, AE_OVL186_MCX_INIT_RESULT) & 1u) != 0u) return 0;
    (void)EXT2(ram, hooks, "CreateThread", AE_ADDR_CREATE_THREAD,
               1u, AE_OVL186_MCX_START_CONFIG);
    (void)EXT2(ram, hooks, "StartThread", AE_ADDR_START_THREAD,
               1u, AE_OVL186_MCX_START_CONFIG);
    (void)EXT0(ram, hooks, "MCXINIT_OBJ_32C", AE_ADDR_MCX_RUN_OPERATION);

    uint32_t node = ae_rd32(ram, AE_OVL186_MCX_THREAD_LIST);
    ae_wr32(ram, node + AE_OVL186_MCX_NODE_HEAD_OFFSET, 0xFFFFFF7Fu);
    ae_wr32(ram, node + AE_OVL186_MCX_NODE_FLAGS_OFFSET,
            ae_rd32(ram, node + AE_OVL186_MCX_NODE_FLAGS_OFFSET) | 0x80u);
    return 0;
}

