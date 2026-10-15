#include "ae_menuwin.h"

#include <stdint.h>

#include "../core/ae_pad.h"
#include "../core/ae_sdk_matrix.h"
#include "../player/ae_player_spawn.h"
#include "../sound/ae_sound.h"
#include "../world/ae_stage.h"
#include "../world/ae_zako2.h"
#include "../world/ae_zones.h"

int32_t ae_WriteSizedValue(AeRam *r, uint32_t address, uint32_t value, int32_t size)
{
    if (size == 1) {
        ae_wr16(r, address, value);
        return 1;
    }
    if (size < 2) {
        if (size == 0) {
            ae_wr8(r, address, value);
            return 1;
        }
        return 1;
    }
    if (size == 2) {
        ae_wr32(r, address, value);
        return 3;
    }
    if (size == 3) {
        ae_wr8(r, address, value);
        return 3;
    }
    return 3;
}

uint32_t ae_MenuStrLen(AeRam *r, uint32_t string)
{
    uint32_t start = string;
    uint32_t cursor = string;

    if (ae_rd8(r, cursor) == (uint32_t)'`') {
        cursor += 2u;
        start = cursor;
    }
    while (ae_rd8(r, cursor) != 0u) {
        ++cursor;
    }
    /* The branch delay slot increments the original $a1 even on the terminating NUL. */
    return cursor - start + 1u;
}

int32_t ae_MenuWindowAdd(AeRam *r, uint32_t definition)
{
    uint32_t window_count = ae_rd16(r, AE_G_MENU_WINDOW_COUNT);
    if (window_count >= AE_MENU_WINDOW_COUNT_MAX) {
        return -1;
    }

    /* The input is the definition record; its +4 field supplies the item-list pointer. */
    uint32_t item_definition = ae_rd32(r, definition + AE_MENU_DEF_ITEMS);
    uint32_t window_offset = window_count * AE_MENU_WINDOW_SIZE;
    uint32_t window = AE_MENU_WINDOWS + window_offset;

    ae_wr32(r, window + AE_MENU_WINDOW_POSITION,
            ae_rd32(r, definition + AE_MENU_DEF_POSITION));
    ae_wr32(r, window + AE_MENU_WINDOW_ITEMS, item_definition);
    ae_wr8(r, window + AE_MENU_WINDOW_DRAW_COLOR_A, 1u);
    ae_wr8(r, window + AE_MENU_WINDOW_DRAW_COLOR_B, 2u);
    ae_wr16(r, window + AE_MENU_WINDOW_FRAME_WIDTH, 17u);
    ae_wr16(r, window + AE_MENU_WINDOW_FRAME_HEIGHT, 15u);
    ae_wr8(r, window + AE_MENU_WINDOW_SELECTED_ITEM, 0xFFu);
    ae_wr8(r, window + AE_MENU_WINDOW_STYLE_BYTE, 0xFFu);
    ae_wr16(r, window + AE_MENU_WINDOW_DRAW_COLOR_C, 0u);
    ae_wr16(r, window + AE_MENU_WINDOW_DRAW_COLOR_D, 0u);
    ae_wr8(r, window + AE_MENU_WINDOW_STYLE_FLAG, 0u);
    ae_wr16(r, window + AE_MENU_WINDOW_STYLE, 1474u);
    ae_wr8(r, window + AE_MENU_WINDOW_ITEM_COUNT, 0u);

    int32_t max_text_length = 0;
    uint32_t definition_item_index = item_definition;
    while (ae_rd16(r, definition_item_index + AE_MENU_DEF_ITEM_FLAGS) != 0u) {
        uint32_t flags = ae_rd16(r, definition_item_index + AE_MENU_DEF_ITEM_FLAGS);
        uint32_t text = ae_rd32(r, definition_item_index + AE_MENU_DEF_ITEM_TEXT);
        int32_t text_length = (int32_t)ae_MenuStrLen(r, text);

        if ((flags & 0x6000u) != 0u) {
            text_length += 5;
        }
        if (max_text_length < text_length) {
            max_text_length = text_length;
        }

        if (ae_rds8(r, window + AE_MENU_WINDOW_SELECTED_ITEM) == -1 && (flags & 0x8000u) != 0u) {
            ae_wr8(r, window + AE_MENU_WINDOW_SELECTED_ITEM,
                   ae_rd8(r, window + AE_MENU_WINDOW_ITEM_COUNT));
        }

        ae_wr8(r, window + AE_MENU_WINDOW_ITEM_COUNT,
               ae_rd8(r, window + AE_MENU_WINDOW_ITEM_COUNT) + 1u);
        definition_item_index += AE_MENU_DEF_ITEM_SIZE;
    }

    if (ae_rds8(r, window + AE_MENU_WINDOW_SELECTED_ITEM) == -1) {
        ae_wr8(r, window + AE_MENU_WINDOW_DRAW_COLOR_A, 2u);
    } else {
        ae_wr16(r, window + AE_MENU_WINDOW_FRAME_WIDTH,
                ae_rd16(r, window + AE_MENU_WINDOW_FRAME_WIDTH) + 18u);
        ae_wr16(r, AE_G_MENU_WINDOW_SELECTION, window_count);
    }

    uint32_t item_count = ae_rd8(r, window + AE_MENU_WINDOW_ITEM_COUNT);
    uint32_t left = ae_rd16(r, window + AE_MENU_WINDOW_WIDTH);
    uint32_t top = ae_rd16(r, window + AE_MENU_WINDOW_HEIGHT);
    uint32_t frame_width = ae_rd16(r, window + AE_MENU_WINDOW_FRAME_WIDTH);
    uint32_t frame_height_unsigned = ae_rd16(r, window + AE_MENU_WINDOW_FRAME_HEIGHT);
    ae_wr16(r, window + AE_MENU_WINDOW_RIGHT_EDGE, left + frame_width);
    ae_wr16(r, window + AE_MENU_WINDOW_BOTTOM_EDGE, top + frame_height_unsigned);

    int32_t signed_frame_height = ae_rds16(r, window + AE_MENU_WINDOW_FRAME_HEIGHT);
    uint32_t item_step_width = ((uint32_t)max_text_length + 1u) << 3;
    ae_wr16(r, window + AE_MENU_WINDOW_CONTENT_WIDTH,
            frame_width + item_step_width);

    int32_t item_text_width = (int16_t)((signed_frame_height + (int32_t)(item_count * 5u)) * 2 - 5);
    ae_wr16(r, window + AE_MENU_WINDOW_ITEM_WIDTH, (uint32_t)item_text_width);

    int32_t y = ae_rds16(r, window + AE_MENU_WINDOW_HEIGHT);
    if (y + item_text_width >= 240) {
        ae_wr16(r, window + AE_MENU_WINDOW_HEIGHT, (uint32_t)(232 - item_text_width));
    }

    ae_wr16(r, window + AE_MENU_WINDOW_TEXT_HEIGHT, 0u);
    if (item_text_width < 0) {
        item_text_width += 3;
    }
    int32_t row_height = item_text_width >> 2;
    if (row_height < 16) {
        row_height = 16;
    }
    ae_wr16(r, window + AE_MENU_WINDOW_ROW_HEIGHT, (uint32_t)row_height);

    uint32_t next_count = (window_count + 1u) & 0xFFFFu;
    ae_wr16(r, AE_G_MENU_WINDOW_COUNT, next_count);
    return (int32_t)((next_count & 0xFFFFu) - 1u);
}

static int32_t menu_s16(uint32_t value)
{
    return (int16_t)(uint16_t)value;
}

static int32_t menu_divide_by_ten_magic(int32_t value)
{
    int64_t product = (int64_t)value * (int32_t)0xB60B60B7u;
    int32_t high = (int32_t)(product >> 32);
    return ((high + value) >> 8) - (value >> 31);
}

static int32_t menu_missing_hook(AeRam *r, const char *name)
{
    ae_ram_fail(r, "AeMenuWinHooks.%s not provided", name);
    return 0;
}

static int32_t menu_call0(AeRam *r, const AeMenuWinHooks *hooks,
                          AeMenuWinHook0Fn function, const char *name)
{
    if (function == NULL) return menu_missing_hook(r, name);
    return (int32_t)function(hooks->user, r);
}

static int32_t menu_call1(AeRam *r, const AeMenuWinHooks *hooks,
                          AeMenuWinHook1Fn function, const char *name,
                          uint32_t a0)
{
    if (function == NULL) return menu_missing_hook(r, name);
    return (int32_t)function(hooks->user, r, a0);
}

static int32_t menu_call2(AeRam *r, const AeMenuWinHooks *hooks,
                          AeMenuWinHook2Fn function, const char *name,
                          uint32_t a0, uint32_t a1)
{
    if (function == NULL) return menu_missing_hook(r, name);
    return (int32_t)function(hooks->user, r, a0, a1);
}

static int32_t menu_call3(AeRam *r, const AeMenuWinHooks *hooks,
                          AeMenuWinHook3Fn function, const char *name,
                          uint32_t a0, uint32_t a1, uint32_t a2)
{
    if (function == NULL) return menu_missing_hook(r, name);
    return (int32_t)function(hooks->user, r, a0, a1, a2);
}

static int32_t menu_call4(AeRam *r, const AeMenuWinHooks *hooks,
                          AeMenuWinHook4Fn function, const char *name,
                          uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    if (function == NULL) return menu_missing_hook(r, name);
    return (int32_t)function(hooks->user, r, a0, a1, a2, a3);
}

static void menu_debug_printf(AeRam *r, const AeMenuWinHooks *hooks,
                              uint32_t format, uint32_t first, uint32_t second,
                              uint32_t argument_count)
{
    for (uint32_t index = 0; index < AE_MENU_ARGUMENT_FRAME_WORDS; ++index)
        ae_wr32(r, AE_MENU_ARGUMENT_FRAME + index * 4u, 0u);
    ae_wr32(r, AE_MENU_ARGUMENT_FRAME, format);
    if (argument_count > 0u)
        ae_wr32(r, AE_MENU_ARGUMENT_FRAME + 4u, first);
    if (argument_count > 1u)
        ae_wr32(r, AE_MENU_ARGUMENT_FRAME + 8u, second);
    ae_DebugTextPrintf(r, hooks != NULL ? hooks->debug_text : NULL,
                       format, AE_MENU_ARGUMENT_FRAME);
}

static void menu_draw_hook(AeRam *r, const AeMenuWinHooks *hooks,
                           AeMenuWinHook4Fn function, const char *name,
                           uint32_t window, uint32_t item, uint32_t item_index,
                           uint32_t context)
{
    (void)menu_call4(r, hooks, function, name, window, item, item_index, context);
}

static int32_t menu_callback(AeRam *r, const AeMenuWinHooks *hooks,
                             uint32_t target, uint32_t value)
{
    if (target == 0u) return 0;
    if (hooks == NULL || hooks->item_callback == NULL) {
        ae_ram_fail(r, "AeMenuWinHooks.item_callback not provided for %08X", target);
        return 0;
    }
    return (int32_t)hooks->item_callback(hooks->user, r, target, value);
}

static uint32_t menu_item_at(AeRam *r, uint32_t window, uint32_t index)
{
    return ae_rd32(r, window + AE_MENU_WINDOW_ITEMS) + index * AE_MENU_DEF_ITEM_SIZE;
}

static void menu_sync_displayed_window(AeRam *r, uint32_t window)
{
    int32_t displayed_index = ae_rds8(r, window + AE_MENU_WINDOW_DISPLAY_FLAGS);
    if (displayed_index == -1) return;

    uint32_t displayed = AE_MENU_WINDOWS +
        (uint32_t)displayed_index * AE_MENU_WINDOW_SIZE;
    ae_wr8(r, displayed + AE_MENU_WINDOW_STYLE_FLAG, 1u);

    int32_t selected = ae_rds8(r, window + AE_MENU_WINDOW_ACTIVE_ITEM);
    if (selected == -1) return;
    uint32_t item = menu_item_at(r, window, (uint32_t)selected);
    ae_wr32(r, displayed + AE_MENU_WINDOW_SELECTED_VALUE,
            ae_rd32(r, item + AE_MENU_ITEM_VALUE));
}

static int menu_update_input_repeat(AeRam *r, const AeMenuWinHooks *hooks,
                                    uint32_t window, uint32_t held,
                                    uint32_t changed)
{
    if ((held & 0xF000u) == 0u) {
        ae_wr16(r, AE_G_MENU_INPUT_REPEAT,
                ae_rd8(r, window + AE_MENU_WINDOW_OPEN));
        return 0;
    }

    uint16_t repeat = (uint16_t)(ae_rd16(r, AE_G_MENU_INPUT_REPEAT) - 1u);
    ae_wr16(r, AE_G_MENU_INPUT_REPEAT, repeat);
    if (repeat != 0u) return 0;

    (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, 1);
    ae_wr16(r, AE_G_MENU_INPUT_REPEAT, (held & changed) != 0u ? 20u : 3u);
    return 1;
}

static void menu_draw_item_text(AeRam *r, const AeMenuWinHooks *hooks,
                                uint32_t window, uint32_t item, uint32_t index)
{
    uint32_t type = ae_rd16(r, item + AE_MENU_ITEM_FLAGS);
    uint32_t label = ae_rd32(r, item + AE_MENU_ITEM_LABEL);
    uint32_t variable = ae_rd32(r, item + AE_MENU_ITEM_VARIABLE);
    if (type == 0xC005u || type == 0xC008u) {
        int32_t value = ae_ReadSizedValue(r, ae_rd32(r, item + AE_MENU_ITEM_VARIABLE),
                                         (int32_t)ae_rd8(r, item + AE_MENU_ITEM_VALUE_SIZE));
        menu_debug_printf(r, hooks, AE_MENU_DEBUG_FORMAT_VALUE, label, (uint32_t)value,
                          2u);
    } else if (type == 0x2006u) {
        menu_debug_printf(r, hooks, AE_MENU_DEBUG_FORMAT_VALUE, label,
                          ae_rd32(r, window + AE_MENU_WINDOW_SELECTED_VALUE),
                          2u);
    } else {
        uint32_t format = variable == 0u && (type & 0x8000u) != 0u
            ? AE_MENU_DEBUG_FORMAT_LABEL_SELECTED : AE_MENU_DEBUG_FORMAT_LABEL;
        menu_debug_printf(r, hooks, format, label, 0u, 1u);
    }
    (void)index;
}

static void menu_draw_animated_item_label(AeRam *r, const AeMenuWinHooks *hooks,
                                         uint32_t item)
{
    uint32_t flags = ae_rd16(r, item + AE_MENU_ITEM_FLAGS);
    uint32_t variable = ae_rd32(r, item + AE_MENU_ITEM_VARIABLE);
    uint32_t format = variable == 0u && (flags & 0x8000u) != 0u
        ? AE_MENU_DEBUG_FORMAT_LABEL_SELECTED : AE_MENU_DEBUG_FORMAT_LABEL;
    menu_debug_printf(r, hooks, format, ae_rd32(r, item + AE_MENU_ITEM_LABEL),
                      0u, 1u);
}

static void menu_update_selected(AeRam *r, const AeMenuWinHooks *hooks,
                                 uint32_t window, uint32_t held,
                                 uint32_t changed,
                                 int repeat_ready,
                                 uint32_t *deferred_close)
{
    uint32_t item_count = ae_rd8(r, window + AE_MENU_WINDOW_ITEM_TOTAL);
    uint32_t selected = ae_rd8(r, window + AE_MENU_WINDOW_ACTIVE_ITEM);
    if (item_count == 0u || selected >= item_count) return;

    uint32_t item = menu_item_at(r, window, selected);
    uint32_t type = ae_rd16(r, item + AE_MENU_ITEM_FLAGS);

    if (repeat_ready && (held & 0xA000u) != 0u) {
        if (type == 0xC005u || type == 0xC008u) {
            int32_t step = ae_rds32(r, item + AE_MENU_ITEM_STEP);
            if ((held & 8u) != 0u) step = (int32_t)((uint32_t)step * 10u);
            if ((held & 0x2000u) == 0u) step = (int32_t)(0u - (uint32_t)step);
            int32_t current = ae_ReadSizedValue(r, ae_rd32(r, item + AE_MENU_ITEM_VARIABLE),
                                                (int32_t)ae_rd8(r, item + AE_MENU_ITEM_VALUE_SIZE));
            if (step == 0 || (step == -1 && current == INT32_MIN)) {
                ae_ram_fail(r, "MenuWindowsRun item division trap");
                return;
            }
            int32_t snapped = (int32_t)((uint32_t)(current / step) * (uint32_t)step);
            int32_t value = (int32_t)((uint32_t)step + (uint32_t)snapped);
            int32_t minimum = ae_rds32(r, item + AE_MENU_ITEM_MINIMUM);
            int32_t maximum = ae_rds32(r, item + AE_MENU_ITEM_MAXIMUM);
            if (value < minimum) value = maximum;
            if (maximum < value) value = minimum;
            (void)ae_WriteSizedValue(r, ae_rd32(r, item + AE_MENU_ITEM_VARIABLE),
                                     (uint32_t)value,
                                     (int32_t)ae_rd8(r, item + AE_MENU_ITEM_VALUE_SIZE));
            if (type != 0xC008u &&
                menu_callback(r, hooks, ae_rd32(r, item + AE_MENU_ITEM_CALLBACK),
                              (uint32_t)value) == 1) {
                ae_wr8(r, window + AE_MENU_WINDOW_ACTIVE_ITEM,
                       item_count == 1u ? 0u : 1u);
                ae_wr8(r, window + AE_MENU_WINDOW_ANIMATION, 1u);
            }
            menu_sync_displayed_window(r, window);
        }
    }

    if ((held & changed & 0x8A0u) != 0u) {
        switch (type) {
        case 0x8004u:
            (void)ae_StageRequest(r, hooks != NULL ? hooks->stage : NULL, 7,
                                  ae_rds16(r, AE_G_MENU_STAGE_STATE + AE_G_MENU_STAGE_AREA),
                                  ae_rds16(r, AE_G_MENU_STAGE_STATE + AE_G_MENU_STAGE_ENTRY));
            (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, 3);
            break;
        case 0x8002u: {
            uint32_t definition = ae_rd32(r, item + AE_MENU_ITEM_VARIABLE);
            if (definition == 0u) {
                (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, 36);
                (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, 33);
            } else {
                uint32_t child_definition = AE_MENU_ARGUMENT_FRAME +
                                           AE_MENU_SUBMENU_DEFINITION_OFFSET;
                uint32_t x = ae_rd16(r, window + AE_MENU_WINDOW_WIDTH) +
                             ae_rd16(r, window + AE_MENU_WINDOW_CONTENT_WIDTH) + 4u;
                int32_t y = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_HEIGHT)) - 4;
                ae_wr32(r, child_definition,
                        ((uint32_t)(uint16_t)x) | ((uint32_t)(uint16_t)y << 16));
                ae_wr32(r, child_definition + AE_MENU_DEF_ITEMS, definition);
                (void)ae_MenuWindowAdd(r, child_definition);
                (void)menu_callback(r, hooks, ae_rd32(r, item + AE_MENU_ITEM_CALLBACK), 0u);
            }
            break;
        }
        case 0x8003u: {
            uint32_t definition = ae_rd32(r, item + AE_MENU_ITEM_VARIABLE);
            if (definition == 0u) {
                (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, 36);
                (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, 33);
                break;
            }
            uint32_t child_definition = AE_MENU_ARGUMENT_FRAME + AE_MENU_SUBMENU_DEFINITION_OFFSET;
            uint32_t x = ae_rd16(r, window + AE_MENU_WINDOW_WIDTH) +
                         ae_rd16(r, window + AE_MENU_WINDOW_CONTENT_WIDTH) + 4u;
            int32_t y = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_HEIGHT)) - 4;
            ae_wr32(r, child_definition,
                    ((uint32_t)(uint16_t)x) | ((uint32_t)(uint16_t)y << 16));
            ae_wr32(r, child_definition + AE_MENU_DEF_ITEMS, definition);
            int32_t first_child_index = ae_MenuWindowAdd(r, child_definition);
            uint32_t child_x = AE_MENU_ARGUMENT_FRAME + AE_MENU_SUBMENU_DEFINITION_OFFSET;
            uint32_t second_x = ae_rd16(r, window + AE_MENU_WINDOW_WIDTH) + 4u;
            int32_t second_y = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_HEIGHT)) +
                               menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_MAX_TEXT_HEIGHT)) + 8;
            ae_wr32(r, child_x,
                    ((uint32_t)(uint16_t)second_x) | ((uint32_t)(uint16_t)second_y << 16));
            ae_wr32(r, child_x + AE_MENU_DEF_ITEMS, AE_MENU_CHILD_EXIT_ITEMS);
            int32_t overlay_index = ae_MenuWindowAdd(r, child_x);
            uint32_t first_child = AE_MENU_WINDOWS +
                (uint32_t)first_child_index * AE_MENU_WINDOW_SIZE;
            uint32_t second_child = AE_MENU_WINDOWS +
                (uint32_t)overlay_index * AE_MENU_WINDOW_SIZE;
            ae_wr8(r, first_child + AE_MENU_WINDOW_DISPLAY_FLAGS, (uint32_t)overlay_index);
            ae_wr8(r, second_child + AE_MENU_WINDOW_STYLE_FLAG, 1u);
            int32_t displayed_child_index = ae_rds8(r, first_child + AE_MENU_WINDOW_DISPLAY_FLAGS);
            uint32_t displayed_child = AE_MENU_WINDOWS +
                (uint32_t)displayed_child_index * AE_MENU_WINDOW_SIZE;
            uint32_t displayed_items = ae_rd32(r, displayed_child + AE_MENU_WINDOW_ITEMS);
            ae_wr32(r, second_child + AE_MENU_WINDOW_SELECTED_VALUE,
                    ae_rd32(r, displayed_items + AE_MENU_ITEM_VALUE));
            (void)menu_callback(r, hooks, ae_rd32(r, item + AE_MENU_ITEM_CALLBACK), 0u);
            break;
        }
        case 0x8007u: {
            int32_t current = ae_ReadSizedValue(r, ae_rd32(r, item + AE_MENU_ITEM_VARIABLE),
                                                (int32_t)ae_rd8(r, item + AE_MENU_ITEM_VALUE_SIZE));
            if (menu_callback(r, hooks, ae_rd32(r, item + AE_MENU_ITEM_CALLBACK),
                              (uint32_t)current) == 1) {
                ae_wr8(r, window + AE_MENU_WINDOW_ACTIVE_ITEM,
                       item_count == 1u ? 0u : 1u);
                ae_wr8(r, window + AE_MENU_WINDOW_ANIMATION, 1u);
            }
            if ((held & 0x80u) == 0u) {
                (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, 2);
            }
            break;
        }
        case 0xC005u:
            *deferred_close = 1u;
            break;
        case 0xC008u: {
            int32_t current = ae_ReadSizedValue(r, ae_rd32(r, item + AE_MENU_ITEM_VARIABLE),
                                                (int32_t)ae_rd8(r, item + AE_MENU_ITEM_VALUE_SIZE));
            int32_t result = menu_callback(r, hooks, ae_rd32(r, item + AE_MENU_ITEM_CALLBACK),
                                           (uint32_t)current);
            if (result == 1) {
                ae_wr8(r, window + AE_MENU_WINDOW_ACTIVE_ITEM, 1u);
                ae_wr8(r, window + AE_MENU_WINDOW_ANIMATION, 1u);
            }
            if ((held & 0x80u) == 0u) (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, 2);
            break;
        }
        default:
            if (type < 0x8002u) {
                *deferred_close = 1u;
                if (ae_rd32(r, item + AE_MENU_ITEM_VARIABLE) == 0u) {
                    (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, 36);
                    (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, 33);
                } else {
                    (void)menu_callback(r, hooks, ae_rd32(r, item + AE_MENU_ITEM_CALLBACK), 0u);
                }
            } else if (type > 0x8004u) {
                *deferred_close = 1u;
            }
            break;
        }
    }
    if ((held & changed & 0x2u) != 0u) {
        (void)ae_WriteSizedValue(r, ae_rd32(r, item + AE_MENU_ITEM_VARIABLE),
                                 ae_rd32(r, item + AE_MENU_ITEM_VALUE),
                                 (int32_t)ae_rd8(r, item + AE_MENU_ITEM_VALUE_SIZE));
    }
}

static void menu_move_selection(AeRam *r, uint32_t window, uint32_t window_index,
                                uint32_t held, int repeat_ready)
{
    uint32_t directions = held & 0xF000u;
    if (directions == 0u) {
        ae_wr16(r, AE_G_MENU_WINDOW_SELECTION, (uint16_t)window_index);
        return;
    }

    if (!repeat_ready) return;

    uint32_t count = ae_rd8(r, window + AE_MENU_WINDOW_ITEM_TOTAL);
    if (count == 0u) return;
    int32_t selected = ae_rds8(r, window + AE_MENU_WINDOW_ACTIVE_ITEM);
    if ((directions & 0x1000u) != 0u) {
        do {
            --selected;
            if (selected < 0) selected = (int32_t)count - 1;
        } while ((ae_rd16(r, menu_item_at(r, window, (uint32_t)selected) + AE_MENU_ITEM_FLAGS) & 0x8000u) == 0u);
    } else if ((directions & 0x4000u) != 0u) {
        do {
            ++selected;
            if (selected >= (int32_t)count) selected = 0;
        } while ((ae_rd16(r, menu_item_at(r, window, (uint32_t)selected) + AE_MENU_ITEM_FLAGS) & 0x8000u) == 0u);
    }
    ae_wr8(r, window + AE_MENU_WINDOW_ACTIVE_ITEM, (uint32_t)selected);
    ae_wr16(r, AE_G_MENU_WINDOW_SELECTION, (uint16_t)window_index);
}

static void menu_draw_cursor(AeRam *r, const AeMenuWinHooks *hooks,
                             uint32_t window, uint32_t context)
{
    uint32_t stack = AE_MENU_ARGUMENT_FRAME - 0x10u;
    uint32_t angles = stack + AE_MENU_STACK_CURSOR_ANGLES_OFFSET;
    uint32_t matrix = stack + AE_MENU_STACK_CURSOR_MATRIX_OFFSET;
    uint32_t scale = stack + AE_MENU_STACK_CURSOR_SCALE_OFFSET;
    uint32_t animation_x = ae_rd16(r, window + AE_MENU_WINDOW_ANIMATION_X);
    uint32_t window_x = ae_rd16(r, window + AE_MENU_WINDOW_WIDTH);
    uint32_t window_y = ae_rd16(r, window + AE_MENU_WINDOW_HEIGHT);
    uint32_t frame_width = ae_rd16(r, window + AE_MENU_WINDOW_RENDER_WIDTH);
    uint32_t frame_height = ae_rd16(r, window + AE_MENU_WINDOW_RENDER_HEIGHT);
    int32_t selected = ae_rds8(r, window + AE_MENU_WINDOW_ACTIVE_ITEM);

    ae_wr16(r, window + AE_MENU_WINDOW_RIGHT_EDGE,
            window_x + frame_width - 14u);
    ae_wr16(r, window + AE_MENU_WINDOW_BOTTOM_EDGE,
            window_y + frame_height + (uint32_t)(selected * 10));

    int32_t half_phase = (int32_t)((animation_x + 80u) >> 1);
    int32_t first_input = (int32_t)((uint32_t)half_phase << 12);
    int32_t angle = menu_divide_by_ten_magic(first_input);
    int32_t wrap_1024 = angle < 0 ? angle + 1023 : angle;
    int32_t wrapped_angle = angle - (wrap_1024 >> 10) * 1024;
    if (menu_s16((uint32_t)wrapped_angle) >= 513) {
        wrapped_angle = 1024 - wrapped_angle;
    }
    int32_t first_total = wrapped_angle + 3840;
    int32_t first_adjusted = first_total < 0 ? wrapped_angle + 7935 : first_total;
    int32_t first_sector = first_adjusted >> 12;
    int32_t first_angle = first_total - first_sector * 4096;

    int32_t second_input = (int32_t)((animation_x * 3u) << 12);
    int32_t second_angle = menu_divide_by_ten_magic(second_input);
    int32_t second_adjusted = second_angle < 0 ? second_angle + 4095 : second_angle;
    int32_t second_sector = second_adjusted >> 12;
    second_angle -= second_sector * 4096;

    ae_wr16(r, angles, (uint32_t)first_angle);
    ae_wr16(r, angles + 2u, (uint32_t)second_angle);
    ae_wr16(r, angles + 4u, 0u);
    ae_wr16(r, angles + 6u, 0u);
    (void)ae_sdk_RotMatrixZYX(r, angles, matrix);

    uint32_t size = (uint32_t)ae_rd16(r, window + AE_MENU_WINDOW_STYLE);
    ae_wr32(r, scale, size);
    ae_wr32(r, scale + 4u, size);
    ae_wr32(r, scale + 8u, size);
    (void)ae_sdk_ScaleMatrix(r, matrix, scale);

    ae_wr32(r, stack + AE_MENU_STACK_CURSOR_TRANSLATE_X_OFFSET,
            (uint32_t)(menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_RIGHT_EDGE)) - 192));
    ae_wr32(r, stack + AE_MENU_STACK_CURSOR_TRANSLATE_Y_OFFSET,
            (uint32_t)(menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_BOTTOM_EDGE)) - 118));
    ae_wr32(r, stack + AE_MENU_STACK_CURSOR_TRANSLATE_Z_OFFSET, 384u);
    (void)menu_call3(r, hooks, hooks != NULL ? hooks->ModelTransformRigid : NULL,
                    "ModelTransformRigid", AE_MENU_CURSOR_MODEL_TRANSFORM,
                    matrix, AE_MENU_CURSOR_MODEL);
    uint32_t buffer_index = ae_rd32(r, context + AE_MENU_RENDER_BUFFER_INDEX);
    uint32_t model = context + AE_MENU_RENDER_BUFFER_DATA_OFFSET +
                     buffer_index * AE_MENU_RENDER_BUFFER_STRIDE;
    (void)menu_call3(r, hooks, hooks != NULL ? hooks->ModelDraw : NULL,
                    "ModelDraw", model, AE_MENU_CURSOR_MODEL,
                    AE_MENU_CURSOR_MODEL_TRANSFORM);
    uint32_t cursor_value = (animation_x & 0x7Fu) << 2;
    if (cursor_value >= 256u) cursor_value = 511u - cursor_value;
    menu_draw_hook(r, hooks, hooks != NULL ? hooks->DrawMenuCursor : NULL,
                   "DrawMenuCursor", window, ae_rd32(r, context + AE_MENU_RENDER_PRIMITIVE),
                   selected, cursor_value);

    /* The original builds one cursor primitive in the active buffer. Its packet bytes belong
     * to the renderer; the selected item and transformed model above remain menu state. */
}

int32_t ae_MenuWindowsRun(AeRam *r, const AeMenuWinHooks *hooks,
                          uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    if (ae_rd16(r, AE_G_MENU_WINDOW_COUNT) == 0u) return 0;

    uint32_t context = AE_MENU_RENDER_CONTEXT;
    uint32_t stack = AE_MENU_ARGUMENT_FRAME - 0x10u;
    uint32_t base_buffer = ae_rd32(r, context + AE_MENU_RENDER_BUFFER_INDEX);
    uint32_t buffer = context + base_buffer * AE_MENU_RENDER_BUFFER_STRIDE;
    uint32_t area = stack + AE_MENU_STACK_ARGUMENT_AREA_OFFSET;
    int32_t draw_mode = menu_s16(ae_rd16(r, buffer + AE_MENU_RENDER_BUFFER_DRAW_MODE));
    uint16_t saved_debug = (uint16_t)ae_rd16(r, AE_G_MENU_DEBUG_ENABLED);

    ae_wr16(r, area, 0u);
    ae_wr16(r, area + 2u, ae_rd16(r, buffer + AE_MENU_RENDER_BUFFER_AREA_Y));
    ae_wr16(r, area + 4u, 383u);
    ae_wr16(r, area + 6u, 239u);
    (void)menu_call4(r, hooks, hooks != NULL ? hooks->SetDrawMove : NULL,
                    "SetDrawMove", ae_rd32(r, context + AE_MENU_RENDER_PRIMITIVE),
                    area, 0u, (uint32_t)draw_mode);
    (void)menu_call1(r, hooks, hooks != NULL ? hooks->DrawPrim : NULL,
                     "DrawPrim", ae_rd32(r, context + AE_MENU_RENDER_PRIMITIVE));
    ae_wr32(r, context + AE_MENU_RENDER_PRIMITIVE,
            ae_rd32(r, context + AE_MENU_RENDER_PRIMITIVE) + AE_MENU_RENDER_PRIMITIVE_ADVANCE);
    ae_wr16(r, AE_G_MENU_DEBUG_ENABLED, 1u);

    uint32_t deferred_close = 0u;
    for (;;) {
        ae_pad_PadUpdate(r, hooks != NULL ? hooks->pad : NULL, 0u);
        if (ae_ram_failed(r)) return 0;
        uint32_t held = ae_rd32(r, AE_G_MENU_INPUT_HELD);
        uint32_t changed = ae_rd32(r, AE_G_MENU_INPUT_CHANGED);
        if ((held & changed & 0x140u) != 0u ||
            ae_rd8(r, AE_G_MENU_STAGE_STATE + AE_G_MENU_STAGE_MODE) == 7u ||
            deferred_close != 0u) {
            break;
        }

        ae_wr32(r, context, ae_rd32(r, context) + 1u);
        (void)menu_call0(r, hooks, hooks != NULL ? hooks->BeginFrame : NULL,
                         "BeginFrame");
        (void)menu_call0(r, hooks, hooks != NULL ? hooks->ColorFadeReset : NULL,
                         "ColorFadeReset");
        (void)ae_StageRequestUpdate(r, hooks != NULL ? hooks->stage : NULL);
        if (ae_ram_failed(r)) return 0;

        for (uint32_t window_index = 0;
             window_index < ae_rd16(r, AE_G_MENU_WINDOW_COUNT); ++window_index) {
            uint32_t window = AE_MENU_WINDOWS + window_index * AE_MENU_WINDOW_SIZE;
            uint32_t items = ae_rd32(r, window + AE_MENU_WINDOW_ITEMS);
            uint32_t item_count = ae_rd8(r, window + AE_MENU_WINDOW_ITEM_TOTAL);
            uint32_t animation = ae_rd8(r, window + AE_MENU_WINDOW_ANIMATION);
            uint32_t is_open = ae_rd8(r, window + AE_MENU_WINDOW_OPEN);
            if (is_open == 0u) continue;

            ae_wr16(r, window + AE_MENU_WINDOW_ANIMATION_X,
                    ae_rd16(r, window + AE_MENU_WINDOW_ANIMATION_X) + 1u);
            uint32_t next_y = ae_rd16(r, window + AE_MENU_WINDOW_ANIMATION_Y) + 1u;
            if (next_y > 0xFFFDu) next_y = 0xFFFEu;
            ae_wr16(r, window + AE_MENU_WINDOW_ANIMATION_Y, next_y);
            if (animation == 2u) ae_wr8(r, window + AE_MENU_WINDOW_ANIMATION, 1u);

            uint32_t draw_area = stack + AE_MENU_STACK_ARGUMENT_AREA_OFFSET;
            ae_wr16(r, draw_area, 0u);
            ae_wr16(r, draw_area + 2u, 0u);
            ae_wr16(r, draw_area + 4u, 384u);
            ae_wr16(r, draw_area + 6u, 512u);
            int32_t old_content = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_CONTENT_HEIGHT));
            int32_t maximum = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_MAX_TEXT_HEIGHT));
            if (animation == 2u) {
                ae_wr8(r, window + AE_MENU_WINDOW_ANIMATION, 1u);
                (void)menu_call2(r, hooks, hooks != NULL ? hooks->SetDrawArea : NULL,
                                "SetDrawArea", ae_rd32(r, context + AE_MENU_RENDER_PRIMITIVE),
                                draw_area);
                (void)menu_call4(r, hooks, hooks != NULL ? hooks->SetDrawTPage : NULL,
                                "SetDrawTPage", ae_rd32(r, context + AE_MENU_RENDER_PRIMITIVE) +
                                    AE_MENU_RENDER_TPAGE_PRIMITIVE_OFFSET,
                                0u, 1u, 28u);
                menu_draw_hook(r, hooks, hooks != NULL ? hooks->DrawMenuChrome : NULL,
                               "DrawMenuChrome", window, items, window_index, context);
            }

            int32_t scaled_height = (old_content * 96) / maximum;
            int32_t draw_x = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_WIDTH)) +
                             menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_CONTENT_WIDTH)) / 2 +
                             scaled_height - 118;
            if (draw_x > 0) {
                int32_t draw_y = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_HEIGHT)) +
                                 scaled_height - 106 + draw_mode;
                ae_wr16(r, draw_area, (uint32_t)draw_x);
                ae_wr16(r, draw_area + 2u, (uint32_t)draw_y);
                ae_wr16(r, draw_area + 4u, ae_rd16(r, window + AE_MENU_WINDOW_CONTENT_WIDTH));
                ae_wr16(r, draw_area + 6u, ae_rd16(r, window + AE_MENU_WINDOW_MAX_TEXT_HEIGHT));
                (void)menu_call4(r, hooks, hooks != NULL ? hooks->SetDrawMove : NULL,
                                "SetDrawMove", ae_rd32(r, context + AE_MENU_RENDER_PRIMITIVE),
                                draw_area, (uint32_t)draw_x, (uint32_t)draw_y);
            }

            int32_t row_step = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_ROW_STEP));
            row_step = (row_step * 9) / 10;
            if (row_step < 2) row_step = 2;
            ae_wr16(r, window + AE_MENU_WINDOW_ROW_STEP, (uint32_t)row_step);
            uint32_t content_height = (ae_rd16(r, window + AE_MENU_WINDOW_CONTENT_HEIGHT) +
                                       (uint32_t)row_step) & 0xFFFFu;
            uint32_t max_height = ae_rd16(r, window + AE_MENU_WINDOW_MAX_TEXT_HEIGHT);
            if (content_height > max_height) {
                content_height = max_height;
            }
            ae_wr16(r, window + AE_MENU_WINDOW_CONTENT_HEIGHT, content_height);

            if (ae_rd8(r, window + AE_MENU_WINDOW_ANIMATION) != 0u) {
                (void)menu_call4(r, hooks, hooks != NULL ? hooks->SetDrawMove : NULL,
                                "SetDrawMove", ae_rd32(r, context + AE_MENU_RENDER_PRIMITIVE),
                                draw_area,
                                (uint32_t)menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_WIDTH)),
                                (uint32_t)(menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_HEIGHT)) + draw_mode));
                menu_draw_hook(r, hooks, hooks != NULL ? hooks->DrawMenuRows : NULL,
                               "DrawMenuRows", window, items, item_count, context);
                int32_t render_height = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_RENDER_HEIGHT));
                int32_t content = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_CONTENT_HEIGHT));
                int32_t maximum = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_MAX_TEXT_HEIGHT));
                for (uint32_t item_index = 0; item_index < item_count; ++item_index) {
                    int32_t scaled_row = ((render_height + (int32_t)item_index * 10) * content) / maximum;
                    ae_ColorFadeSet(r,
                                    menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_WIDTH)) +
                                        menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_RENDER_WIDTH)),
                                    menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_HEIGHT)) + scaled_row);
                    uint32_t item = items + item_index * AE_MENU_DEF_ITEM_SIZE;
                    menu_draw_animated_item_label(r, hooks, item);
                    if (ae_ram_failed(r)) return 0;
                }
            }

            int32_t content_width = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_CONTENT_WIDTH));
            int32_t panel_content = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_CONTENT_HEIGHT));
            int32_t panel_maximum = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_MAX_TEXT_HEIGHT));
            int32_t panel_height = ((panel_content * 96) / panel_maximum);
            int32_t panel_left = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_WIDTH)) + content_width / 2;
            int32_t panel_top = panel_left + panel_height - 118;
            if (panel_top > 0) {
                (void)menu_call4(r, hooks, hooks != NULL ? hooks->SetDrawTPage : NULL,
                                "SetDrawTPage", ae_rd32(r, context + AE_MENU_RENDER_PRIMITIVE),
                                0u, 1u, 28u);
            }

            if (ae_rd16(r, window + AE_MENU_WINDOW_CONTENT_HEIGHT) == max_height) {
                ae_wr8(r, window + AE_MENU_WINDOW_ANIMATION, 0u);
            }

            uint32_t selected_item = ae_rd8(r, window + AE_MENU_WINDOW_ACTIVE_ITEM);
            uint32_t draw_selected_rows = ae_rd8(r, window + AE_MENU_WINDOW_STYLE_FLAG) != 0u ||
                (ae_rd16(r, AE_G_MENU_WINDOW_SELECTION) == window_index &&
                 ae_rd16(r, window + AE_MENU_WINDOW_OPEN) == 1u);
            if (draw_selected_rows) {
                menu_draw_hook(r, hooks, hooks != NULL ? hooks->DrawMenuHighlight : NULL,
                               "DrawMenuHighlight", window, items, selected_item, context);
                int32_t right_edge = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_RIGHT_EDGE));
                int32_t bottom_edge = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_BOTTOM_EDGE));
                int32_t window_x = menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_WIDTH));
                int32_t first_panel_y = bottom_edge + draw_mode - 1;
                (void)menu_call4(r, hooks, hooks != NULL ? hooks->SetDrawMove : NULL,
                                "SetDrawMove", ae_rd32(r, context + AE_MENU_RENDER_PRIMITIVE),
                                draw_area, (uint32_t)window_x, (uint32_t)first_panel_y);
                (void)menu_call4(r, hooks, hooks != NULL ? hooks->SetDrawMove : NULL,
                                "SetDrawMove", ae_rd32(r, context + AE_MENU_RENDER_PRIMITIVE),
                                draw_area, (uint32_t)(right_edge - 12),
                                (uint32_t)(bottom_edge + draw_mode - 13));
                for (uint32_t item_index = 0; item_index < item_count; ++item_index) {
                    uint32_t item = items + item_index * AE_MENU_DEF_ITEM_SIZE;
                    ae_ColorFadeSet(r,
                                    menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_WIDTH)) +
                                        menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_RENDER_WIDTH)),
                                    menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_HEIGHT)) +
                                        menu_s16(ae_rd16(r, window + AE_MENU_WINDOW_RENDER_HEIGHT)) +
                                        (int32_t)item_index * 10);
                    menu_draw_item_text(r, hooks, window, item, item_index);
                    if (ae_ram_failed(r)) return 0;
                }
                menu_draw_hook(r, hooks, hooks != NULL ? hooks->DrawMenuSelection : NULL,
                               "DrawMenuSelection", window, items, selected_item, context);
                if (ae_rd8(r, window + AE_MENU_WINDOW_OPEN) == 1u) {
                    int repeat_ready = menu_update_input_repeat(r, hooks, window,
                                                                held, changed);
                    if ((held & 0xF000u) == 0u) {
                        ae_wr16(r, AE_G_MENU_WINDOW_SELECTION, (uint16_t)window_index);
                    } else {
                        menu_move_selection(r, window, window_index,
                                            held, repeat_ready);
                    }
                    menu_update_selected(r, hooks, window, held, changed,
                                         repeat_ready, &deferred_close);
                    if (ae_ram_failed(r)) return 0;
                    menu_draw_cursor(r, hooks, window, context);
                }
            }

            menu_draw_hook(r, hooks, hooks != NULL ? hooks->DrawMenuItem : NULL,
                           "DrawMenuItem", window, items,
                           ae_rd8(r, window + AE_MENU_WINDOW_ACTIVE_ITEM), context);
            if (ae_ram_failed(r)) return 0;
        }

        uint32_t buffer_index = ae_rd32(r, context + AE_MENU_RENDER_BUFFER_INDEX);
        uint32_t order_table = context + AE_MENU_RENDER_BUFFER_DATA_OFFSET +
                               buffer_index * AE_MENU_RENDER_BUFFER_STRIDE;
        (void)menu_call1(r, hooks, hooks != NULL ? hooks->OtLinkSkipEmpty : NULL,
                         "OtLinkSkipEmpty", order_table);
        (void)menu_call1(r, hooks, hooks != NULL ? hooks->DrawSync : NULL,
                         "DrawSync", 0u);
        (void)menu_call1(r, hooks, hooks != NULL ? hooks->VSync : NULL,
                         "VSync", 0u);
        uint32_t draw_environment = context + AE_MENU_RENDER_BUFFER_DRAWENV_OFFSET +
                                    ((buffer_index ^ 1u) * AE_MENU_RENDER_BUFFER_STRIDE);
        uint32_t display_environment = context + AE_MENU_RENDER_BUFFER_DISPENV_OFFSET +
                                      buffer_index * AE_MENU_RENDER_BUFFER_STRIDE;
        (void)menu_call1(r, hooks, hooks != NULL ? hooks->PutDrawEnv : NULL,
                         "PutDrawEnv", draw_environment);
        (void)menu_call1(r, hooks, hooks != NULL ? hooks->PutDispEnv : NULL,
                         "PutDispEnv", display_environment);
        (void)menu_call1(r, hooks, hooks != NULL ? hooks->DrawOTag : NULL,
                        "DrawOTag", ae_rd32(r, context + AE_MENU_RENDER_ORDERING_TABLE) +
                            AE_MENU_RENDER_OT_TERMINATOR_OFFSET);
        if (ae_ram_failed(r)) return 0;
    }

    ae_wr16(r, AE_G_MENU_WINDOW_COUNT, 0u);
    ae_wr16(r, AE_G_MENU_WINDOW_SELECTION, 0u);
    ae_wr16(r, AE_G_MENU_DEBUG_ENABLED, saved_debug);
    ae_pad_PadUpdate(r, hooks != NULL ? hooks->pad : NULL, 0u);
    if (ae_ram_failed(r)) return 0;
    return 0;
}
