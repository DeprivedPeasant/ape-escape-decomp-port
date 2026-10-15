/* HUD, font and message-box game logic from the SCUS-94423 RAM image.
 * Renderer requests carry the chosen asset, image-space values and ordering
 * depth; the native renderer owns the GPU representation. */
#include "ae_hud.h"

#include "../core/ae_sdk.h"
#include "../effects/ae_effects.h"
#include "../player/ae_player_spawn.h"
#include "../player/native_player.h"
#include "../world/ae_misc.h"

#include <limits.h>
#include <stdint.h>

/* Original globals and record fields. Keep RAM-image addresses named here so
 * the function bodies describe game fields rather than pointer arithmetic. */
#define AE_HUD_GPU_STATE                 0x800EC350u
#define AE_HUD_GPU_PRIMITIVE             0x04u
#define AE_HUD_GPU_OT_DEPTH              0x0Cu
#define AE_HUD_GPU_ORDERING_TABLE_PTR    0x08u
#define AE_HUD_GPU_OT_BASE               0x4080u
#define AE_HUD_GPU_OT_STRIDE             0x4078u
#define AE_HUD_GPU_OT_FIRST_ENTRY          0x80u
#define AE_HUD_PLAYER                    0x800EC1E0u
#define AE_HUD_PLAYER_CURRENT_GADGET      0xF2u
#define AE_HUD_PLAYER_HEALTH              0xE8u
#define AE_HUD_PLAYER_STATE               0x5Eu
#define AE_HUD_STAGE_STATE                0x800F8000u
#define AE_HUD_STAGE_EXIT_CODE            0x12Cu
#define AE_HUD_STAGE_CURRENT_AREA         0x126u
#define AE_HUD_STAGE_AREA_KIND            0x152u
#define AE_HUD_FRAME_COUNTER              0x800EC350u
#define AE_HUD_STAGE_TRANSITION            0x800E78CEu
#define AE_HUD_GAME_MODE                   0x800E6908u
#define AE_HUD_ICON_TABLE                 0x800AF0E0u
#define AE_HUD_HEALTH_TABLE               0x800B01E0u
#define AE_HUD_SCREEN_TABLE               0x800A5A08u
#define AE_HUD_AREA_COPY_RECTS            0x800B98D0u
#define AE_HUD_AREA_HEADER_POINTER        0x800F44E4u
#define AE_HUD_AREA_CONTEXT               0x800F4350u
#define AE_HUD_AREA_CONTEXT_KIND           0x0124u
#define AE_HUD_AREA_CONTEXT_FRAMES         0x0140u
#define AE_HUD_AREA_CONTEXT_PLAYER_MODE    0x0150u
#define AE_HUD_AREA_TEXTURE_START          0x0030u
#define AE_HUD_AREA_TEXTURE_END            0x0032u
#define AE_HUD_AREA_TEXTURE_ENABLED        0x002Au
#define AE_HUD_AREA_PANEL_A_ENABLED        0x0186u
#define AE_HUD_AREA_PANEL_A_GATE           0x0188u
#define AE_HUD_AREA_PANEL_A_PROGRESS       0x0166u
#define AE_HUD_AREA_PANEL_MODE             0x012Eu
#define AE_HUD_AREA_PANEL_KIND             0x0124u
#define AE_HUD_MESSAGE_BOX                0x800F5A00u
#define AE_HUD_MESSAGE_STATE              0x00u
#define AE_HUD_MESSAGE_BLINK              0x01u
#define AE_HUD_MESSAGE_CLUT_ROW           0x02u
#define AE_HUD_MESSAGE_PAGE_END           0x04u
#define AE_HUD_MESSAGE_PAGE_START         0x08u
#define AE_HUD_MESSAGE_ANIMATION          0x10u
#define AE_HUD_MESSAGE_TERMINAL_DELAY     0x12u
#define AE_HUD_MESSAGE_CURRENT_CODE       0x14u
#define AE_HUD_MESSAGE_HAS_TEXT           0x16u
#define AE_HUD_PAD_HELD                   0x800E5478u
#define AE_HUD_PAD_CHANGED                0x800E5480u
#define AE_HUD_TEXT_CURSOR                0x800DF810u
#define AE_HUD_TEXT_CURSOR_Y              0x02u
#define AE_HUD_TEXT_START_X               0x04u
#define AE_HUD_GADGET_SLOT_TABLE          0x800B2728u
#define AE_HUD_HUD_STATE                  0x800F4DB0u
#define AE_HUD_HUD_FLAGS                  0x20u
#define AE_HUD_PANEL_A_TIMER              0x08u
#define AE_HUD_PANEL_A_STATE              0x0Au
#define AE_HUD_PANEL_B_TIMER              0x0Cu
#define AE_HUD_PANEL_B_STATE              0x0Eu
#define AE_HUD_COUNTER_TIMER              0x10u
#define AE_HUD_GADGET_ICON_STATE          0x800F5120u
#define AE_HUD_GADGET_ICON_TIMER          0x98u
#define AE_HUD_GADGET_ICON_HOLD           0x9Au
#define AE_HUD_GADGET_ICON_CURRENT        0xA0u
#define AE_HUD_GADGET_ICON_NEXT           0xA1u
#define AE_HUD_GADGET_ICON_PREVIOUS       0xA2u
#define AE_HUD_GADGET_ICON_TRANSITION      0x9Cu
#define AE_HUD_GADGET_ICON_POSITION_X      0x94u
#define AE_HUD_GADGET_ICON_POSITION_Y      0x96u
#define AE_HUD_GADGET_ICON_TABLE_A        0x800B2234u
#define AE_HUD_GADGET_ICON_TABLE_B        0x800B2254u
#define AE_HUD_COUNTER_POSITION_TABLE      0x800B0200u
#define AE_HUD_ICON_ATLAS                  0x800A5A00u
#define AE_HUD_GADGET_SLOT_EMPTY           0x800B0010u
#define AE_HUD_GADGET_SLOT_BASE            0x800B0008u
#define AE_HUD_GADGET_ICON_FRAME_ACTIVE    0x800B2258u
#define AE_HUD_HEALTH_PANEL_FILLED         0x800B01E0u
#define AE_HUD_HEALTH_PANEL_EMPTY          0x800B0220u
#define AE_HUD_DAT_800B01C2                0x800B01C2u
#define AE_HUD_SCREEN_TABLE_MESSAGE_FRAME  0x08u
#define AE_HUD_GADGET_SLOTS               0x800F51A8u
#define AE_HUD_FONT_WIDTHS                0x800B3640u
#define AE_HUD_FONT_BUTTONS               0x800B3738u
#define AE_HUD_FONT_BUTTON_BLINK          0x800B3798u
#define AE_HUD_FONT_FACE_A                0x800B37A0u
#define AE_HUD_FONT_FACE_B                0x800B37C0u
#define AE_HUD_ICON_RECORD_STRIDE         0x08u
#define AE_HUD_GLYPH_PAGE                 0x257u
#define AE_HUD_BUTTON_PAGE                0x217u
#define AE_HUD_RENDER_PACKET_SPRITE       0x18u
#define AE_HUD_MESSAGE_FRAME_PACKET_BYTES 0x220u
#define AE_HUD_GTE_COLOR_MATRIX_MODE       0x1F8003C8u
#define AE_HUD_DEFAULT_STACK_POINTER      0x801FFF00u
#define AE_HUD_TEMP_CODE                  0x801FFF10u
#define AE_HUD_COUNTER_MODEL              0x800F4D20u
#define AE_HUD_COUNTER_MODEL_DATA         0x800A5A20u
#define AE_HUD_TIMER_MODEL_DATA           0x800A5A10u
#define AE_HUD_TIMER_MODEL                0x800F4D20u
#define AE_HUD_GADGET_FRAME_PACKET_BYTES  0x18u
#define AE_HUD_GADGET_ICON_PACKET_BYTES   0x18u
#define AE_HUD_PANEL_STATE_BASE           0x800F4DB0u
#define AE_HUD_COUNTER_BASE               0x800F4DB0u
#define AE_HUD_COUNTER_STATE               0x00u
#define AE_HUD_COUNTER_ACTIVE             0x02u
#define AE_HUD_COUNTER_DELAY              0x03u
#define AE_HUD_GADGET_STATE_BASE          0x800F5120u
#define AE_HUD_AREA_HUD_B_VALUE           0x13Cu
#define AE_HUD_AREA_COUNTER_REMAINING      0x168u
#define AE_HUD_AREA_STAGE_TYPE              0x124u
#define AE_HUD_AREA_OVERLAY_X2              0x18u
#define AE_HUD_AREA_OVERLAY_Y2              0x1Au
#define AE_HUD_AREA_OVERLAY_X3              0x1Cu
#define AE_HUD_AREA_OVERLAY_Y3              0x1Eu
#define AE_HUD_TABLE_RECORD_HEIGHT          0x01u
#define AE_HUD_TABLE_RECORD_UV              0x02u
#define AE_HUD_TABLE_RECORD_V               0x03u
#define AE_HUD_TABLE_RECORD_CLUT            0x04u
#define AE_HUD_TABLE_RECORD_TPAGE           0x06u
#define AE_HUD_GADGET_ICON_CHARGE           0x18u
#define AE_HUD_GADGET_ICON_MODE             0x1Cu
#define AE_HUD_GADGET_TABLE_Y               0x02u
#define AE_HUD_GADGET_SLOT_RECORD_V         0x01u
#define AE_HUD_GADGET_SLOT_RECORD_CLUT      0x02u
#define AE_HUD_HEALTH_POINT_Y               0x01u
#define AE_HUD_AREA_COPY_RECT_X             0x00u
#define AE_HUD_AREA_COPY_RECT_Y             0x02u
#define AE_HUD_DR_MOVE_RECT_Y               0x02u
#define AE_HUD_PANEL_B_SLIDE                0x04u
#define AE_HUD_HUD_PANEL_B_Y                0x04u
#define AE_HUD_MODEL_POSITION               0x10u
#define AE_HUD_MODEL_ROTATION               0x18u
#define AE_HUD_MODEL_SCALE                  0x28u
#define AE_HUD_MODEL_TEMP                   0x20u
#define AE_HUD_MODEL_VECTOR_X               0x00u
#define AE_HUD_MODEL_VECTOR_Y               0x02u
#define AE_HUD_MODEL_VECTOR_Z               0x04u
#define AE_HUD_MODEL_ROTATION_FRAME         0x0Au
#define AE_HUD_COUNTER_POSITION_FIRST_Y     0x02u
#define AE_HUD_FONT_RECORD_CLUT              0x00u
#define AE_HUD_FONT_RECORD_UV                0x02u
#define AE_HUD_FONT_RECORD_WIDTH             0x04u
#define AE_HUD_FONT_RECORD_HEIGHT            0x06u
#define AE_HUD_PANEL_A_ICON_X               310
#define AE_HUD_PANEL_B_ICON_X               52
#define AE_HUD_PANEL_B_NUMBER_X             76
#define AE_HUD_PANEL_B_SPRITE_X             20
#define AE_HUD_PANEL_Y_BASE                 192
#define AE_HUD_PANEL_B_DIGIT_Y              200
#define AE_HUD_PANEL_B_NUMBER_Y             199
#define AE_HUD_PANEL_B_SPRITE_Y             192
#define AE_HUD_PANEL_A_LENGTH               40
#define AE_HUD_PANEL_B_LENGTH               40
#define AE_HUD_PANEL_GADGET_PROGRESS        0x18u
#define AE_HUD_STAGE_OVERLAY_OT_OFFSET     0x3FFCu
#define AE_HUD_PANEL_FLAGS_FIELD            0x20u
#define AE_HUD_GADGET_ICON_FLAG             0x08u
#define AE_HUD_GADGET_ICON_BASE_Y            0xB8
#define AE_HUD_PANEL_STATE_OPEN             1u
#define AE_HUD_PANEL_STATE_HOLD             2u
#define AE_HUD_PANEL_STATE_CLOSE            3u
#define AE_HUD_PANEL_STATE_OFF              4u
#define AE_HUD_PANEL_A_FLAG                 0x04u
#define AE_HUD_PANEL_B_FLAG                 0x08u
#define AE_HUD_COUNTER_FLAG                 0x01u
#define AE_HUD_FONT_LINE_HEIGHT              0x12u

#define AE_HUD_STATE_GADGET_MODE           0x800E68D0u
#define AE_HUD_GADGET_MODE_KIND            0x46u
#define AE_HUD_GADGET_MODE_FLAGS           0x32u
#define AE_HUD_GADGET_ICON_BLINK           0x2Fu

extern int32_t ae_IsTimeAttack(AeRam *r);

static int32_t hud_area_feature30(AeRam *r);
static void hud_play_sfx(AeRam *r, const AeHudHooks *h, int32_t id);

static int32_t hud_s16(int32_t value) {
    return (int16_t)(uint16_t)value;
}

static int32_t hud_sar32(int32_t value, uint32_t shift) {
    int32_t divisor = (int32_t)(1u << shift);
    int32_t quotient = value / divisor;
    if (value < 0 && value % divisor != 0) --quotient;
    return quotient;
}

static uint32_t hud_ot_entry(AeRam *r, uint32_t depth) {
    (void)r;
    uint32_t base = AE_HUD_GPU_STATE + AE_HUD_GPU_OT_BASE;
    return base + depth * AE_HUD_GPU_OT_STRIDE;
}

static void hud_draw(AeRam *r, const AeHudHooks *h, AeHudDraw *draw,
                     uint32_t packet_bytes, uint32_t primitive,
                     int update_global_cursor, uint32_t ordering_entry) {
    uint32_t depth = ae_rd32(r, AE_HUD_GPU_STATE + AE_HUD_GPU_OT_DEPTH);
    draw->primitive_address = primitive;
    draw->ordering_entry = ordering_entry != 0u ? ordering_entry : hud_ot_entry(r, depth);
    draw->depth = depth;
    draw->packet_bytes = packet_bytes;
    if (ae_ram_failed(r)) return;

    switch (draw->kind) {
    case AE_HUD_DRAW_SPRITE:
        if (h == NULL || h->draw_sprite == NULL) {
            ae_ram_fail(r, "AeHudHooks.draw_sprite not provided");
            return;
        }
        h->draw_sprite(h->user, r, draw);
        break;
    case AE_HUD_DRAW_QUAD:
        if (h == NULL || h->draw_quad == NULL) {
            ae_ram_fail(r, "AeHudHooks.draw_quad not provided");
            return;
        }
        h->draw_quad(h->user, r, draw);
        break;
    case AE_HUD_DRAW_GLYPH:
        if (h == NULL || h->draw_glyph == NULL) {
            ae_ram_fail(r, "AeHudHooks.draw_glyph not provided");
            return;
        }
        h->draw_glyph(h->user, r, draw);
        break;
    case AE_HUD_DRAW_STAGE_OVERLAY:
        if (h == NULL || h->draw_stage_overlay == NULL) {
            ae_ram_fail(r, "AeHudHooks.draw_stage_overlay not provided");
            return;
        }
        h->draw_stage_overlay(h->user, r, draw);
        break;
    case AE_HUD_DRAW_GAUGE:
        if (h == NULL || h->draw_gauge == NULL) {
            ae_ram_fail(r, "AeHudHooks.draw_gauge not provided");
            return;
        }
        h->draw_gauge(h->user, r, draw);
        break;
    case AE_HUD_DRAW_MESSAGE_FRAME:
        if (h == NULL || h->draw_message_frame == NULL) {
            ae_ram_fail(r, "AeHudHooks.draw_message_frame not provided");
            return;
        }
        h->draw_message_frame(h->user, r, draw);
        break;
    default:
        ae_ram_fail(r, "unknown HUD draw command %u", (unsigned)draw->kind);
        return;
    }
    if (!ae_ram_failed(r) && update_global_cursor)
        ae_wr32(r, AE_HUD_GPU_STATE + AE_HUD_GPU_PRIMITIVE, primitive + packet_bytes);
}

static void hud_draw_current(AeRam *r, const AeHudHooks *h, AeHudDraw *draw,
                             uint32_t packet_bytes) {
    uint32_t primitive = ae_rd32(r, AE_HUD_GPU_STATE + AE_HUD_GPU_PRIMITIVE);
    if (ae_ram_failed(r)) return;
    hud_draw(r, h, draw, packet_bytes, primitive, 1, 0u);
}

static uint32_t hud_emit_explicit(AeRam *r, const AeHudHooks *h,
                                  AeHudDraw *draw, uint32_t packet_bytes,
                                  uint32_t primitive) {
    hud_draw(r, h, draw, packet_bytes, primitive, 0, 0u);
    return primitive + packet_bytes;
}

static void hud_draw_to_entry(AeRam *r, const AeHudHooks *h, AeHudDraw *draw,
                              uint32_t packet_bytes, uint32_t primitive,
                              uint32_t ordering_entry) {
    hud_draw(r, h, draw, packet_bytes, primitive, 0, ordering_entry);
}

static int32_t hud_hook_missing(AeRam *r, const char *name) {
    ae_ram_fail(r, "AeHudHooks.%s not provided", name);
    return 0;
}

static uint32_t hud_stack_entry(const AeHudHooks *h) {
    return h != NULL && h->stack_pointer != 0u ? h->stack_pointer :
           AE_HUD_DEFAULT_STACK_POINTER;
}

static int32_t hud_model_draw(AeRam *r, const AeHudHooks *h, uint32_t model,
                              uint32_t position, uint32_t rotation,
                              uint32_t scale) {
    if (h == NULL || h->ModelDrawScreenSpace == NULL)
        return hud_hook_missing(r, "ModelDrawScreenSpace");
    return h->ModelDrawScreenSpace(h->user, r, model, position, rotation, scale);
}

static void hud_clear_memory(AeRam *r, const AeHudHooks *h, uint32_t address,
                             uint32_t value, uint32_t size) {
    if (h == NULL || h->ClearMemory == NULL) {
        ae_ram_fail(r, "AeHudHooks.ClearMemory not provided");
        return;
    }
    h->ClearMemory(h->user, r, address, value, size);
}

static void hud_copy_bytes(AeRam *r, uint32_t destination, uint32_t source,
                           uint32_t size) {
    for (uint32_t index = 0; index < size; ++index)
        ae_wr8(r, destination + index, ae_rd8(r, source + index));
}

static uint32_t hud_div_s32(int32_t numerator, int32_t denominator) {
    if (denominator == 0) return (numerator < 0) ? 1u : 0xFFFFFFFFu;
    if (numerator == INT32_MIN && denominator == -1) return 0x80000000u;
    return (uint32_t)(numerator / denominator);
}

static int32_t hud_div_s32_value(int32_t numerator, int32_t denominator) {
    return (int32_t)hud_div_s32(numerator, denominator);
}

void ae_HudDrawDigit(AeRam *r, const AeHudHooks *h, int32_t x, int32_t y,
                     int32_t digit) {
    int32_t cell;
    if (digit == -1) {
        cell = 10;
    } else {
        int32_t shifted = digit + 9;
        int32_t quotient = hud_div_s32_value(shifted, 10);
        cell = shifted - quotient * 10;
    }
    int32_t row = hud_div_s32_value(cell, 3);
    int32_t column = cell - row * 3;

    AeHudDraw draw = {0};
    draw.kind = AE_HUD_DRAW_SPRITE;
    draw.asset = (uint32_t)cell;
    draw.glyph = digit;
    draw.x = hud_s16(x);
    draw.y = hud_s16(y);
    draw.width = 24;
    draw.height = 24;
    draw.u = (uint16_t)(column * 24);
    draw.v = (uint16_t)(160 + row * 24);
    draw.clut = 0x7C37u;
    draw.tpage = 0x021Du;
    draw.red = 0x65;
    hud_draw_current(r, h, &draw, AE_HUD_RENDER_PACKET_SPRITE);
}

void ae_HudDrawNumber(AeRam *r, const AeHudHooks *h, int32_t x, int32_t y,
                      int32_t number) {
    int32_t place = 1000;
    for (;;) {
        int32_t digit = hud_div_s32_value(number, place);
        int32_t remainder = number - digit * place;
        if (number >= place || place == 1) {
            ae_HudDrawDigit(r, h, x, y, digit);
            x = hud_s16(x + 24);
        }
        if (ae_ram_failed(r) || place == 1) return;
        number = remainder;
        place /= 10;
    }
}

void ae_HudDrawIcon16(AeRam *r, const AeHudHooks *h, int32_t x, int32_t y,
                      int32_t index) {
    int32_t column_index = index >> 3;
    int32_t row = hud_div_s32_value(column_index, 4);
    int32_t column = column_index - row * 4;
    int32_t row8 = hud_div_s32_value(column_index, 8);
    int32_t row_remainder = column_index - row8 * 8;
    int32_t vertical_row = hud_div_s32_value(row_remainder, 4);
    AeHudDraw draw = {0};
    draw.kind = AE_HUD_DRAW_SPRITE;
    draw.asset = (uint32_t)index;
    draw.x = hud_s16(x);
    draw.y = hud_s16(y);
    draw.width = 16;
    draw.height = 16;
    draw.u = (uint16_t)(80 + column * 16);
    draw.v = (uint16_t)((vertical_row * 16 - 32) & 0xFF);
    draw.clut = 0x7CB7u;
    draw.tpage = 0x021Du;
    draw.red = 0x65;
    hud_draw_current(r, h, &draw, AE_HUD_RENDER_PACKET_SPRITE);
}

int32_t ae_HudCounterC_Draw(AeRam *r, const AeHudHooks *h) {
    uint32_t hud = AE_HUD_COUNTER_BASE;
    uint32_t state = ae_rd16(r, hud + AE_HUD_COUNTER_STATE);
    uint32_t active = ae_rd8(r, hud + AE_HUD_COUNTER_ACTIVE);
    if (active != 0u) {
        uint8_t delay = (uint8_t)(ae_rd8(r, hud + AE_HUD_COUNTER_DELAY) - 1u);
        ae_wr8(r, hud + AE_HUD_COUNTER_DELAY, delay);
        if (delay == 0u) {
            ae_wr8(r, hud + AE_HUD_COUNTER_DELAY, 3u);
            active = (active - 1u) & 0xFFu;
            ae_wr8(r, hud + AE_HUD_COUNTER_ACTIVE, active);
            if (active != 0u) hud_play_sfx(r, h, 0x7A);
        }
    }

    uint32_t frame = hud_stack_entry(h) - 80u;
    int32_t countdown = (int32_t)ae_rds16(r, AE_HUD_AREA_CONTEXT +
                                               AE_HUD_AREA_COUNTER_REMAINING) -
                        ((int32_t)ae_rd8(r, hud + AE_HUD_COUNTER_ACTIVE) - 100);
    int32_t quotient = countdown / 100;
    int32_t remainder = countdown - quotient * 100;
    if ((int16_t)state >= 91) return 1;

    int32_t first_y = 30;
    int32_t second_y = 30;
    if ((int16_t)state >= 81) {
        first_y = 30 - 4 * ((int32_t)(int16_t)state - 80);
        second_y = first_y;
    } else if ((int16_t)state < 15) {
        uint32_t position = AE_HUD_COUNTER_POSITION_TABLE + (uint16_t)state;
        second_y = ae_rds8(r, position);
        first_y = ae_rds8(r, position + AE_HUD_COUNTER_POSITION_FIRST_Y);
    }

    int32_t tens = remainder / 10;
    int32_t ones = remainder - tens * 10;
    ae_HudDrawDigit(r, h, 166, hud_s16(first_y + 8), tens);
    ae_HudDrawDigit(r, h, 186, hud_s16(second_y + 8), ones);
    if (ae_ram_failed(r)) return 0;

    hud_clear_memory(r, h, frame + AE_HUD_MODEL_POSITION, 0u, 8u);
    if (ae_ram_failed(r)) return 0;
    hud_clear_memory(r, h, frame + AE_HUD_MODEL_TEMP, 0u, 8u);
    if (ae_ram_failed(r)) return 0;
    ae_wr16(r, frame + AE_HUD_MODEL_POSITION + AE_HUD_MODEL_VECTOR_X, 150u);
    ae_wr16(r, frame + AE_HUD_MODEL_POSITION + AE_HUD_MODEL_VECTOR_Y,
            (uint32_t)hud_s16(first_y + 22));
    ae_wr16(r, frame + AE_HUD_MODEL_POSITION + AE_HUD_MODEL_VECTOR_Z, 328u);
    ae_wr16(r, frame + AE_HUD_MODEL_ROTATION + AE_HUD_MODEL_ROTATION_FRAME,
            (ae_rd16(r, AE_HUD_FRAME_COUNTER) << 5));
    hud_copy_bytes(r, frame + AE_HUD_MODEL_ROTATION, frame + AE_HUD_MODEL_TEMP, 8u);
    hud_copy_bytes(r, frame + AE_HUD_MODEL_SCALE, AE_HUD_TIMER_MODEL_DATA, 16u);
    if (ae_ram_failed(r)) return 0;

    ae_wr32(r, AE_HUD_GTE_COLOR_MATRIX_MODE, 2u);
    (void)hud_model_draw(r, h, AE_HUD_COUNTER_MODEL,
                         frame + AE_HUD_MODEL_POSITION,
                         frame + AE_HUD_MODEL_ROTATION,
                         frame + AE_HUD_MODEL_SCALE);
    if (ae_ram_failed(r)) return 0;
    ae_wr16(r, hud + AE_HUD_COUNTER_STATE, (uint16_t)(state + 1u));
    return 0;
}

void ae_HudDrawTimerGauge(AeRam *r, const AeHudHooks *h, int32_t frames) {
    int32_t scaled = (int32_t)((uint32_t)frames * 42u);
    int32_t width = hud_div_s32_value(scaled, 60);
    uint32_t tint = 0xFFu;
    if (width >= 97) {
        width = 96;
        tint = 0xFFFFu;
    }

    AeHudDraw draw = {0};
    draw.kind = AE_HUD_DRAW_GAUGE;
    draw.asset = AE_HUD_TIMER_MODEL_DATA;
    draw.flags = tint;
    draw.x = 160;
    draw.y = 160;
    draw.width = width;
    draw.height = 72;
    draw.red = 0x80;
    draw.green = 0x80;
    draw.blue = 0x80;
    hud_draw_current(r, h, &draw, 84u);
    if (ae_ram_failed(r)) return;

    uint32_t frame = hud_stack_entry(h) - 64u;
    hud_copy_bytes(r, frame + AE_HUD_MODEL_POSITION, AE_HUD_COUNTER_MODEL_DATA, 8u);
    hud_clear_memory(r, h, frame + AE_HUD_MODEL_TEMP, 0u, 8u);
    if (ae_ram_failed(r)) return;
    ae_wr16(r, frame + AE_HUD_MODEL_ROTATION + AE_HUD_MODEL_ROTATION_FRAME,
            (ae_rd16(r, AE_HUD_FRAME_COUNTER) << 6));
    hud_copy_bytes(r, frame + AE_HUD_MODEL_ROTATION, frame + AE_HUD_MODEL_TEMP, 8u);
    hud_copy_bytes(r, frame + AE_HUD_MODEL_SCALE, AE_HUD_TIMER_MODEL_DATA, 16u);
    if (ae_ram_failed(r)) return;
    ae_wr32(r, AE_HUD_GTE_COLOR_MATRIX_MODE, 2u);
    (void)hud_model_draw(r, h, AE_HUD_TIMER_MODEL,
                         frame + AE_HUD_MODEL_POSITION,
                         frame + AE_HUD_MODEL_ROTATION,
                         frame + AE_HUD_MODEL_SCALE);
}

void ae_HudDrawGadgetSlotsFrame(AeRam *r, const AeHudHooks *h) {
    int32_t progress = ae_rds16(r, AE_HUD_HUD_STATE + AE_HUD_PANEL_GADGET_PROGRESS);
    uint32_t frame_flags = 0x000000FFu;
    if (progress >= 150) {
        frame_flags = progress < 300 ? 0x00FF3030u : 0x0000FFFFu;
    }

    AeHudDraw background = {0};
    background.kind = AE_HUD_DRAW_SPRITE;
    background.asset = AE_HUD_ICON_ATLAS;
    background.flags = frame_flags;
    background.x = 280;
    background.y = 28;
    background.width = 64;
    background.height = 64;
    background.clut = 0x766Bu;
    background.v = 0xC0u;
    background.tpage = 0x023Au;
    background.red = 0x67;
    background.blue = 0xC0;
    hud_draw_current(r, h, &background, AE_HUD_RENDER_PACKET_SPRITE);
    if (ae_ram_failed(r) || progress <= 0) return;

    int32_t angle = -2048;
    int32_t outer_sine = ae_sdk_rsin(r, angle);
    int32_t outer_cosine = ae_sdk_rcos(r, angle);
    for (int32_t filled = 0; filled < progress; filled += 15) {
        int32_t next_angle = hud_s16(angle + 64);
        int32_t next_sine = ae_sdk_rsin(r, next_angle);
        int32_t next_cosine = ae_sdk_rcos(r, next_angle);
        AeHudDraw wedge = {0};
        wedge.kind = AE_HUD_DRAW_QUAD;
        wedge.asset = AE_HUD_ICON_ATLAS;
        wedge.flags = frame_flags;
        wedge.x = 312 + hud_sar32(outer_cosine * 28, 12);
        wedge.y = 60 - hud_sar32(outer_sine * 28, 12);
        wedge.x2 = 312 + hud_sar32(outer_cosine * 12, 12);
        wedge.y2 = 60 - hud_sar32(outer_sine * 12, 12);
        wedge.x3 = 312 + hud_sar32(next_cosine * 28, 12);
        wedge.y3 = 60 - hud_sar32(next_sine * 28, 12);
        wedge.x4 = 312 + hud_sar32(next_cosine * 12, 12);
        wedge.y4 = 60 - hud_sar32(next_sine * 12, 12);
        wedge.width = 40;
        wedge.height = 40;
        wedge.red = 0x67;
        wedge.green = 0x28;
        wedge.tpage = 0x023Au;
        wedge.clut = 0x766Bu;
        hud_draw_current(r, h, &wedge, AE_HUD_GADGET_FRAME_PACKET_BYTES);
        if (ae_ram_failed(r)) return;
        angle = next_angle;
        outer_sine = next_sine;
        outer_cosine = next_cosine;
    }
}

uint32_t ae_HudDrawGadgetIcon(AeRam *r, const AeHudHooks *h, int32_t x,
                              int32_t y, int32_t gadget,
                              uint32_t primitive) {
    if (gadget == -1) {
        AeHudDraw empty = {0};
        empty.kind = AE_HUD_DRAW_SPRITE;
        empty.asset = AE_HUD_GADGET_SLOT_EMPTY;
        empty.x = hud_s16(x);
        empty.y = hud_s16(y);
        empty.width = 16;
        empty.height = 11;
        empty.u = 16;
        empty.v = 150;
        empty.clut = 0x756Bu;
        empty.tpage = 0x021Au;
        empty.red = 0x65;
        return hud_emit_explicit(r, h, &empty, AE_HUD_GADGET_ICON_PACKET_BYTES,
                                 primitive);
    }

    int32_t tens = gadget / 10;
    int32_t ones = gadget - tens * 10;
    int32_t tens_row = tens / 4;
    int32_t ones_row = ones / 4;
    int32_t tens_column = tens - tens_row * 4;
    int32_t ones_column = ones - ones_row * 4;
    AeHudDraw draw = {0};
    draw.kind = AE_HUD_DRAW_SPRITE;
    draw.asset = AE_HUD_GADGET_SLOT_BASE;
    draw.x = hud_s16(x);
    draw.y = hud_s16(y);
    draw.width = 8;
    draw.height = 11;
    draw.u = (uint16_t)((tens_column * 8) & 0xFF);
    draw.v = (uint16_t)((tens_row * 11 - 128) & 0xFF);
    draw.clut = 0x756Bu;
    draw.tpage = 0x021Au;
    draw.red = 0x65;
    primitive = hud_emit_explicit(r, h, &draw,
                                  AE_HUD_GADGET_ICON_PACKET_BYTES, primitive);
    if (ae_ram_failed(r)) return primitive;

    draw.x = hud_s16(x + 8);
    draw.u = (uint16_t)((ones_column * 8) & 0xFF);
    draw.v = (uint16_t)((ones_row * 11 - 128) & 0xFF);
    return hud_emit_explicit(r, h, &draw,
                             AE_HUD_GADGET_ICON_PACKET_BYTES, primitive);
}

void ae_HudDrawGadgetSlotIcons(AeRam *r, const AeHudHooks *h) {
    uint32_t state = AE_HUD_GADGET_STATE_BASE;
    uint32_t primitive = ae_rd32(r, AE_HUD_GPU_STATE + AE_HUD_GPU_PRIMITIVE);
    int32_t position = ae_rds16(r, state + AE_HUD_GADGET_ICON_TIMER);
    int32_t selected = ae_rds8(r, state + AE_HUD_GADGET_ICON_CURRENT);
    int32_t cursor_y = 90;
    if (ae_ram_failed(r)) return;

    for (int32_t row = 0; row < 3; ++row) {
        uint32_t slot_table = AE_HUD_GADGET_SLOT_TABLE + (uint32_t)row * 4u;
        int32_t gadget = row == 1 ? (int32_t)ae_rd8(r, state + AE_HUD_GADGET_ICON_NEXT) :
                         row == 2 ? (int32_t)ae_rd8(r, state + AE_HUD_GADGET_ICON_PREVIOUS) : -1;
        AeHudDraw label = {0};
        label.kind = AE_HUD_DRAW_SPRITE;
        label.asset = ae_rd32(r, slot_table);
        label.x = hud_s16(position + 21);
        label.y = cursor_y;
        label.width = 16;
        label.height = 16;
        label.u = (uint16_t)(label.asset & 0xFFu);
        label.v = (uint16_t)((label.asset >> 8) & 0xFFu);
        label.clut = (uint16_t)(label.asset >> 16);
        label.tpage = 0x021Au;
        label.red = 0x65;
        primitive = hud_emit_explicit(r, h, &label,
                                      AE_HUD_GADGET_ICON_PACKET_BYTES, primitive);

        primitive = ae_HudDrawGadgetIcon(r, h, hud_s16(position + 37),
                                         cursor_y + 3, gadget, primitive);
        if (ae_ram_failed(r)) return;

        if (selected == row) {
            AeHudDraw selected_frame = {0};
            selected_frame.kind = AE_HUD_DRAW_SPRITE;
            selected_frame.asset = AE_HUD_GADGET_ICON_TABLE_B;
            selected_frame.x = hud_s16(position + 14);
            selected_frame.y = cursor_y;
            selected_frame.width = 48;
            selected_frame.height = 16;
            selected_frame.u = 0x40u;
            selected_frame.v = 0xA0u;
            selected_frame.clut = 0x786Bu;
            selected_frame.tpage = 0x021Au;
            selected_frame.red = 0x65;
            primitive = hud_emit_explicit(r, h, &selected_frame,
                                          AE_HUD_GADGET_ICON_PACKET_BYTES, primitive);
            selected_frame.asset = AE_HUD_GADGET_ICON_FRAME_ACTIVE;
            selected_frame.u = 0x40u;
            selected_frame.v = 0xB0u;
            selected_frame.clut = 0x78ABu;
            selected_frame.tpage = 0x025Au;
            primitive = hud_emit_explicit(r, h, &selected_frame,
                                          AE_HUD_GADGET_ICON_PACKET_BYTES, primitive);
        }
        cursor_y += 12;
    }
    int32_t transition_timer = ae_rds16(r, state + AE_HUD_GADGET_ICON_TRANSITION);
    if (transition_timer > 0) {
        int32_t old_position = position;
        position = hud_s16(position - 10);
        ae_wr16(r, state + AE_HUD_GADGET_ICON_TIMER, (uint32_t)position);
        if (old_position < 310) ae_wr16(r, state + AE_HUD_GADGET_ICON_TIMER, 300u);
    } else {
        int32_t next_position = hud_s16(ae_rd16(r, state + AE_HUD_GADGET_ICON_TIMER) + 10 + position);
        ae_wr16(r, state + AE_HUD_GADGET_ICON_TIMER, (uint32_t)next_position);
        if (position >= 401) {
            ae_wr16(r, state + AE_HUD_GADGET_ICON_TIMER, 400u);
            ae_wr16(r, state + AE_HUD_GADGET_ICON_TRANSITION, 0u);
        }
    }

    if (ae_rds8(r, AE_HUD_PLAYER + AE_HUD_PLAYER_CURRENT_GADGET) != 3)
        ae_wr16(r, state + AE_HUD_GADGET_ICON_TRANSITION, 0xFFFFu);
    int32_t hold_timer = ae_rds16(r, state + AE_HUD_GADGET_ICON_HOLD);
    if (hold_timer > 0)
        ae_wr16(r, state + AE_HUD_GADGET_ICON_HOLD, (uint32_t)(hold_timer - 1));
    ae_wr32(r, AE_HUD_GPU_STATE + AE_HUD_GPU_PRIMITIVE, primitive);
}

void ae_HudDrawGadgetSlots(AeRam *r, const AeHudHooks *h) {
    uint32_t player_state = ae_rd16(r, AE_HUD_PLAYER + AE_HUD_PLAYER_STATE);
    uint32_t player_mode = ae_rd16(r, AE_HUD_AREA_CONTEXT + AE_HUD_AREA_CONTEXT_PLAYER_MODE);
    int32_t current_gadget = ae_rds8(r, AE_HUD_PLAYER + AE_HUD_PLAYER_CURRENT_GADGET);
    if (ae_player_is_swimming((uint16_t)player_state)) {
        ae_HudDrawGadgetSlotsFrame(r, h);
        return;
    }
    if (!ae_player_can_switch_gadget((uint16_t)player_mode,
                                     (uint16_t)player_state) || current_gadget == 9)
        return;
    if (current_gadget == 3) ae_HudDrawGadgetSlotIcons(r, h);
    if (ae_ram_failed(r)) return;

    uint32_t primitive = ae_rd32(r, AE_HUD_GPU_STATE + AE_HUD_GPU_PRIMITIVE);
    int32_t follow_x = 0;
    int32_t follow_y = 0;
    for (int32_t slot_index = 0; slot_index < 4; ++slot_index) {
        uint32_t slot = AE_HUD_GADGET_SLOTS + (uint32_t)slot_index;
        int32_t gadget = ae_rds8(r, slot);
        uint32_t table_a = AE_HUD_GADGET_ICON_TABLE_A + (uint32_t)slot_index * 4u;
        uint32_t table_b = AE_HUD_GADGET_ICON_TABLE_B;

        if (gadget != -1) {
            uint32_t record = table_b + (uint32_t)gadget * 4u;
            AeHudDraw icon = {0};
            icon.kind = AE_HUD_DRAW_SPRITE;
            icon.asset = record;
            icon.x = hud_s16((int32_t)ae_rd16(r, table_a) - 16);
            icon.y = hud_s16((int32_t)ae_rd16(r, table_a + AE_HUD_GADGET_TABLE_Y) -
                             (gadget == current_gadget ? 14 : 17));
            icon.width = 32;
            icon.height = 32;
            icon.u = (uint16_t)ae_rd8(r, record);
            icon.v = (uint16_t)ae_rd8(r, record + AE_HUD_GADGET_SLOT_RECORD_V);
            icon.clut = (uint16_t)ae_rd16(r, record + AE_HUD_GADGET_SLOT_RECORD_CLUT);
            icon.tpage = 0x021Eu;
            icon.red = 0x65;
            primitive = hud_emit_explicit(r, h, &icon,
                                          AE_HUD_GADGET_ICON_PACKET_BYTES, primitive);
        }

        AeHudDraw slot_marker = {0};
        slot_marker.kind = AE_HUD_DRAW_SPRITE;
        slot_marker.asset = AE_HUD_GADGET_ICON_TABLE_A + (uint32_t)slot_index * 4u;
        slot_marker.x = hud_s16((int32_t)ae_rd16(r, table_a) - 16);
        slot_marker.y = hud_s16((int32_t)ae_rd16(r, table_a + AE_HUD_GADGET_TABLE_Y) - 16);
        slot_marker.width = 32;
        slot_marker.height = 32;
        slot_marker.u = (uint16_t)((slot_index % 2) * 64 +
                                   (gadget == current_gadget ? 32 : 0));
        slot_marker.v = (uint16_t)((slot_index / 2) * 32 + 64);
        slot_marker.clut = (uint16_t)((0x7840u + (uint32_t)slot_index * 64u) | 0x37u);
        slot_marker.tpage = 0x021Du;
        slot_marker.red = 0x67;
        primitive = hud_emit_explicit(r, h, &slot_marker,
                                      AE_HUD_GADGET_ICON_PACKET_BYTES, primitive);

        if (gadget == current_gadget) {
            follow_x = (int32_t)ae_rd16(r, table_a) - 24;
            follow_y = (int32_t)ae_rd16(r, table_a + AE_HUD_GADGET_TABLE_Y) - 23;
        }
        if (ae_ram_failed(r)) return;
    }

    if (follow_x != 0 && follow_y != 0) {
        int32_t center_x = hud_sar32(ae_rds16(r, AE_HUD_GADGET_STATE_BASE + AE_HUD_GADGET_ICON_POSITION_X) + follow_x, 1);
        int32_t center_y = hud_sar32(ae_rds16(r, AE_HUD_GADGET_STATE_BASE + AE_HUD_GADGET_ICON_POSITION_Y) + follow_y, 1);
        ae_wr16(r, AE_HUD_GADGET_STATE_BASE + AE_HUD_GADGET_ICON_POSITION_X, (uint32_t)center_x);
        ae_wr16(r, AE_HUD_GADGET_STATE_BASE + AE_HUD_GADGET_ICON_POSITION_Y, (uint32_t)center_y);
        AeHudDraw moving = {0};
        moving.kind = AE_HUD_DRAW_SPRITE;
        moving.asset = AE_HUD_GADGET_ICON_TABLE_A;
        moving.x = center_x;
        moving.y = center_y;
        moving.width = 48;
        moving.height = 48;
        moving.u = 0xC0u;
        moving.v = 0x50u;
        moving.clut = 0x363Fu;
        moving.tpage = 0x023Du;
        moving.red = 0x67;
        primitive = hud_emit_explicit(r, h, &moving,
                                      AE_HUD_GADGET_ICON_PACKET_BYTES, primitive);
    }
    ae_wr32(r, AE_HUD_GPU_STATE + AE_HUD_GPU_PRIMITIVE, primitive);
}

void ae_HudPanelB_DrawSprite(AeRam *r, const AeHudHooks *h, int32_t x,
                             int32_t y) {
    AeHudDraw draw = {0};
    draw.kind = AE_HUD_DRAW_SPRITE;
    draw.asset = AE_HUD_ICON_ATLAS;
    draw.flags = 1u;
    draw.x = hud_s16(x);
    draw.y = hud_s16(y);
    draw.width = 0x20;
    draw.height = 0x20;
    draw.u = 0xC0u;
    draw.v = 0x60u;
    draw.clut = 0x3AFFu;
    draw.tpage = 0x024Fu;
    draw.red = 0x65;
    hud_draw_current(r, h, &draw, AE_HUD_RENDER_PACKET_SPRITE);
}

void ae_AreaTexCopyDraw(AeRam *r, const AeHudHooks *h, int32_t index) {
    if (h == NULL || h->DR_MOVE == NULL) {
        ae_ram_fail(r, "AeHudHooks.DR_MOVE not provided");
        return;
    }
    uint32_t frame = hud_stack_entry(h) - 64u;
    uint32_t rect = frame + AE_HUD_MODEL_POSITION;
    uint32_t area = ae_rd32(r, AE_HUD_AREA_HEADER_POINTER);
    uint32_t area_rect = AE_HUD_AREA_COPY_RECTS + (uint32_t)index * 8u;
    uint16_t source_x = (uint16_t)ae_rd16(r, area_rect + AE_HUD_AREA_COPY_RECT_X);
    uint16_t source_y = (uint16_t)ae_rd16(r, area_rect + AE_HUD_AREA_COPY_RECT_Y);
    int32_t source_start = (int32_t)ae_rd16(r, area + AE_HUD_AREA_TEXTURE_START);
    int32_t source_end = (int32_t)ae_rd16(r, area + AE_HUD_AREA_TEXTURE_END);
    int32_t source_delta = source_end - source_start;
    int32_t initial_x = 624;
    int32_t initial_y = hud_s16(384 + index * 32);

    for (uint32_t word = 0; word < 2u; ++word)
        ae_wr32(r, rect + word * 4u,
                ae_rd32(r, AE_HUD_SCREEN_TABLE + word * 4u));
    ae_wr16(r, rect + AE_HUD_AREA_COPY_RECT_X, (uint32_t)initial_x);
    ae_wr16(r, rect + AE_HUD_DR_MOVE_RECT_Y, (uint32_t)initial_y);
    if (ae_ram_failed(r)) return;

    uint32_t primitive = ae_rd32(r, AE_HUD_GPU_STATE + AE_HUD_GPU_PRIMITIVE);
    h->DR_MOVE(h->user, r, primitive, rect, source_x, source_y);
    primitive += AE_HUD_RENDER_PACKET_SPRITE;

    uint32_t depth = ae_rd32(r, AE_HUD_GPU_STATE + AE_HUD_GPU_OT_DEPTH);
    for (int32_t copy = 0; copy < 7; ++copy) {
        int32_t column = copy & 3;
        int32_t row = copy / 4;
        int32_t x = 576 + column * 16;
        int32_t y = hud_s16(256 + index * 32 + row * 128);
        ae_wr16(r, rect + AE_HUD_AREA_COPY_RECT_X, (uint32_t)x);
        ae_wr16(r, rect + AE_HUD_DR_MOVE_RECT_Y, (uint32_t)y);
        h->DR_MOVE(h->user, r, primitive, rect, source_x, source_y);

        int32_t source = source_start + (copy * source_delta) / 6;
        uint32_t bucket = (uint32_t)source;
        if (bucket >= 4097u) bucket = 4096u;
        uint32_t entry = AE_HUD_GPU_STATE + AE_HUD_GPU_OT_FIRST_ENTRY +
                         depth * AE_HUD_GPU_OT_STRIDE + bucket * 4u;
        AeHudDraw draw = {0};
        draw.kind = AE_HUD_DRAW_STAGE_OVERLAY;
        draw.asset = area;
        draw.flags = (uint32_t)copy;
        draw.x = x;
        draw.y = y;
        draw.width = source_x;
        draw.height = source_y;
        draw.x2 = source;
        draw.packet_bytes = AE_HUD_RENDER_PACKET_SPRITE;
        hud_draw_to_entry(r, h, &draw, AE_HUD_RENDER_PACKET_SPRITE,
                          primitive, entry);
        primitive += AE_HUD_RENDER_PACKET_SPRITE;
        if (ae_ram_failed(r)) return;
    }
    ae_wr32(r, AE_HUD_GPU_STATE + AE_HUD_GPU_PRIMITIVE, primitive);
}

void ae_HudDrawHealth(AeRam *r, const AeHudHooks *h) {
    int32_t pip_count = ae_rds16(r, AE_HUD_PLAYER + AE_HUD_PLAYER_HEALTH) - 1;
    if (ae_ram_failed(r)) return;

    for (int32_t pip = pip_count; pip >= 0; --pip) {
        uint32_t point = AE_HUD_HEALTH_TABLE + (uint32_t)pip * 2u;
        AeHudDraw draw = {0};
        draw.kind = AE_HUD_DRAW_SPRITE;
        draw.asset = pip;
        draw.flags = 0x05u;
        draw.x = ae_rds8(r, point);
        draw.y = ae_rds8(r, point + AE_HUD_HEALTH_POINT_Y);
        draw.width = 32;
        draw.height = 32;
        draw.u = 0xE0u;
        draw.v = 0x60u;
        draw.clut = 0x367Fu;
        draw.tpage = 0x020Fu;
        draw.red = 0x67;
        hud_draw_current(r, h, &draw, AE_HUD_RENDER_PACKET_SPRITE);
        if (ae_ram_failed(r)) return;
    }

    AeHudDraw icon = {0};
    icon.kind = AE_HUD_DRAW_SPRITE;
    icon.asset = AE_HUD_ICON_ATLAS;
    icon.flags = 0x05u;
    icon.x = 71;
    icon.y = 63;
    icon.width = 24;
    icon.height = 38;
    icon.u = 0x80u;
    icon.v = 0x40u;
    icon.clut = 0x7A2Bu;
    icon.tpage = 0x021Au;
    icon.red = 0x67;
    hud_draw_current(r, h, &icon, AE_HUD_RENDER_PACKET_SPRITE);
}

static void hud_draw_time_cell(AeRam *r, const AeHudHooks *h, int32_t *x,
                               int32_t y, int32_t cell) {
    int32_t row = hud_sar32(cell, 2);
    int32_t column = cell - row * 4;
    uint32_t packed_uv_clut =
        ((uint32_t)(160 + row * 16) << 8) |
        (uint32_t)(80 + column * 8) | 0x7C770000u;
    AeHudDraw draw = {0};
    draw.kind = AE_HUD_DRAW_SPRITE;
    draw.asset = (uint32_t)cell;
    draw.x = hud_s16(*x);
    draw.y = hud_s16(y);
    draw.width = 8;
    draw.height = 16;
    draw.u = (uint16_t)(packed_uv_clut & 0xFFu);
    draw.v = (uint16_t)((packed_uv_clut >> 8) & 0xFFu);
    draw.clut = (uint16_t)(packed_uv_clut >> 16);
    draw.tpage = 0x025Du;
    draw.red = 0x65;
    hud_draw_current(r, h, &draw, AE_HUD_RENDER_PACKET_SPRITE);
    *x = hud_s16(*x + 8);
}

void ae_HudDrawTime(AeRam *r, const AeHudHooks *h, int32_t x, int32_t y,
                    int32_t frames) {
    uint32_t frame_count = (uint32_t)frames;
    uint32_t total_seconds = frame_count / 60u;
    uint32_t minutes = frame_count / 3600u;
    uint32_t seconds = total_seconds - minutes * 60u;
    uint32_t remainder_frames = frame_count - total_seconds * 60u;
    uint32_t centiseconds = (remainder_frames * 101u) / 60u;

    for (int32_t segment = 0; segment < 3; ++segment) {
        uint32_t value = segment == 0 ? minutes :
                         segment == 1 ? seconds : centiseconds;
        int32_t tens;
        int32_t ones;
        if (frames == -1) {
            tens = 12;
            ones = 12;
        } else {
            tens = (int32_t)(value / 10u);
            ones = (int32_t)(value - (uint32_t)tens * 10u);
        }

        hud_draw_time_cell(r, h, &x, y, tens);
        hud_draw_time_cell(r, h, &x, y, ones);
        if (segment == 0) hud_draw_time_cell(r, h, &x, y, 10);
        else if (segment == 1) hud_draw_time_cell(r, h, &x, y, 11);
        if (ae_ram_failed(r)) return;
    }
}

void ae_StageDrawScreenOverlay(AeRam *r, const AeHudHooks *h,
                               uint32_t a0, uint32_t a1) {
    uint8_t exit_code = (uint8_t)ae_rd8(r, AE_HUD_AREA_CONTEXT + AE_HUD_STAGE_EXIT_CODE);
    if (exit_code == 7u || exit_code == 9u || exit_code == 0x0Bu) return;

    uint32_t area = ae_rd32(r, AE_HUD_AREA_HEADER_POINTER);
    int32_t transition = ae_rds16(r, AE_HUD_STAGE_TRANSITION);
    int32_t adjusted = transition < 0 ? transition + 15 : transition;
    int32_t offset = 120 - hud_sar32(adjusted, 4);
    if (offset < 0) offset = 0;
    else if (offset >= 240) offset = 239;

    AeHudDraw overlay = {0};
    overlay.kind = AE_HUD_DRAW_STAGE_OVERLAY;
    overlay.asset = area;
    overlay.flags = (uint32_t)(uint16_t)transition;
    overlay.x = offset;
    overlay.y = 240 - offset;
    overlay.width = 512;
    overlay.height = 96;
    overlay.x2 = ae_rds16(r, area + AE_HUD_AREA_OVERLAY_X2);
    overlay.y2 = ae_rds16(r, area + AE_HUD_AREA_OVERLAY_Y2);
    overlay.x3 = ae_rds16(r, area + AE_HUD_AREA_OVERLAY_X3);
    overlay.y3 = ae_rds16(r, area + AE_HUD_AREA_OVERLAY_Y3);
    uint32_t overlay_primitive = ae_rd32(r, AE_HUD_GPU_STATE + AE_HUD_GPU_PRIMITIVE);
    uint32_t overlay_ot = ae_rd32(r, AE_HUD_GPU_STATE + AE_HUD_GPU_ORDERING_TABLE_PTR) +
                          AE_HUD_STAGE_OVERLAY_OT_OFFSET;
    hud_draw(r, h, &overlay, 0x20u, overlay_primitive, 1, overlay_ot);
    if (ae_ram_failed(r)) return;

    AeHudHooks nested;
    const AeHudHooks *nested_hooks = h;
    if (h != NULL) {
        nested = *h;
        nested.stack_pointer -= 32u;
        nested_hooks = &nested;
    }

    if (hud_area_feature30(r) != 0) return;
    uint16_t area_kind = (uint16_t)ae_rd16(r, AE_HUD_AREA_CONTEXT + AE_HUD_AREA_CONTEXT_KIND);
    int draw_health_and_slots = 1;
    if (area_kind >= 31u && area_kind < 33u) {
        ae_HudDrawGadgetSlots(r, nested_hooks);
        if (ae_ram_failed(r)) return;
        draw_health_and_slots = 0;
    } else if (area_kind < 31u) {
        uint8_t mode = (uint8_t)ae_rd8(r, AE_HUD_GAME_MODE);
        if (mode == 1u) {
            draw_health_and_slots = 0;
        }
        if (mode != 1u && ae_IsTimeAttack(r) != 0) {
            int32_t frames = ae_rds32(r, AE_HUD_AREA_CONTEXT + AE_HUD_AREA_CONTEXT_FRAMES);
            ae_HudDrawIcon16(r, h, 144, 32, frames);
            ae_HudDrawTime(r, h, 160, 32, frames);
            int32_t gauge = ae_GetDAT_800B01C2(r);
            if (gauge != 0) ae_HudDrawTimerGauge(r, nested_hooks, gauge);
        }
    } else if (ae_rd8(r, AE_HUD_GAME_MODE) == 1u) {
        draw_health_and_slots = 0;
    }

    uint32_t hud_flags = ae_rd32(r, AE_HUD_HUD_STATE + AE_HUD_HUD_FLAGS);
    if (draw_health_and_slots && (hud_flags & 0x10u) != 0u)
        ae_HudDrawHealth(r, nested_hooks);
    if (ae_ram_failed(r)) return;
    if (draw_health_and_slots && (hud_flags & 0x20u) != 0u)
        ae_HudDrawGadgetSlots(r, nested_hooks);
    if (ae_ram_failed(r)) return;
    if ((hud_flags & 0x08u) != 0u) ae_HudPanelB_Update(r, nested_hooks);
    if (ae_ram_failed(r)) return;
    if ((hud_flags & 0x02u) != 0u) {
        if (ae_rd8(r, AE_HUD_GAME_MODE) != 1u) ae_HudDrawGadgetIcons(r, nested_hooks);
    }
    if (ae_ram_failed(r)) return;
    if ((hud_flags & 0x04u) != 0u) ae_HudPanelA_Update(r, nested_hooks);
    if (ae_ram_failed(r)) return;
    if ((hud_flags & 0x01u) != 0u) ae_HudCounterC_Update(r, nested_hooks);
    if (ae_ram_failed(r)) return;

    uint16_t stage_kind = (uint16_t)ae_rd16(r, AE_HUD_AREA_CONTEXT + AE_HUD_AREA_CONTEXT_KIND);
    if (stage_kind == 7u || stage_kind == 14u) {
        if (h == NULL || h->Ovl_8013D424 == NULL) {
            ae_ram_fail(r, "AeHudHooks.Ovl_8013D424 not provided");
            return;
        }
        int32_t overlay_kind = stage_kind == 7u ? 0 : 1;
        (void)h->Ovl_8013D424(h->user, r, overlay_kind);
    }
    if (ae_ram_failed(r)) return;
    if (ae_rd16(r, area + AE_HUD_AREA_TEXTURE_ENABLED) != 0u)
        ae_AreaTexCopyDraw(r, nested_hooks, 0);
    (void)a0;
    (void)a1;
}

void ae_HudPanelA_DrawIcon(AeRam *r, const AeHudHooks *h, int32_t x,
                           int32_t y) {
    uint32_t context = AE_HUD_AREA_CONTEXT;
    uint16_t mode = (uint16_t)ae_rd16(r, context + AE_HUD_AREA_PANEL_MODE);
    int32_t gate = ae_rds16(r, context + AE_HUD_AREA_PANEL_A_GATE);
    uint16_t area_kind = (uint16_t)ae_rd16(r, context + AE_HUD_AREA_PANEL_KIND);
    if (mode == 1u || gate == 0) return;
    if (mode == 2u && area_kind == 0x1Eu) return;

    int special_area = area_kind == 0x11u ||
                       (area_kind >= 0x16u && area_kind <= 0x1Eu);
    if (special_area && mode != 2u) {
        uint32_t frame = ae_rd32(r, AE_HUD_FRAME_COUNTER);
        uint32_t frame_tens = frame / 10u;
        uint32_t blink = frame - frame_tens * 10u;
        AeHudDraw draw = {0};
        draw.kind = AE_HUD_DRAW_SPRITE;
        draw.asset = AE_HUD_HEALTH_PANEL_FILLED;
        draw.x = hud_s16(x);
        draw.y = hud_s16(y);
        draw.width = 32;
        draw.height = 32;
        draw.u = 0xE0u;
        draw.v = (uint16_t)(blink < 6u ? 0xA0u : 0x80u);
        draw.clut = 0x37FFu;
        draw.tpage = 0x020Fu;
        draw.red = 0x65;
        hud_draw_current(r, h, &draw, 20u);
        AeHudDraw end_marker = {0};
        end_marker.kind = AE_HUD_DRAW_SPRITE;
        end_marker.asset = 0x020Fu;
        end_marker.flags = 1u;
        end_marker.tpage = 0x020Fu;
        hud_draw_current(r, h, &end_marker, 8u);
        return;
    }

    int32_t spacing = hud_div_s32_value(200, gate);
    if (spacing >= 33) spacing = 32;
    if (gate > 0) {
        int32_t progress = ae_rds16(r, context + AE_HUD_AREA_PANEL_A_PROGRESS);
        uint32_t frame = ae_rd32(r, AE_HUD_FRAME_COUNTER);
        uint32_t frame_tens = frame / 10u;
        uint32_t blink = frame - frame_tens * 10u;
        for (int32_t segment = 0; segment < gate; ++segment) {
            AeHudDraw draw = {0};
            draw.kind = AE_HUD_DRAW_SPRITE;
            draw.asset = segment < progress ? AE_HUD_HEALTH_PANEL_FILLED :
                                              AE_HUD_HEALTH_PANEL_EMPTY;
            draw.flags = (uint32_t)segment;
            draw.x = hud_s16(x - segment * spacing);
            draw.y = hud_s16(y);
            draw.width = 32;
            draw.height = 32;
            draw.u = (uint16_t)(segment < progress ? 0x80u : 0xA0u);
            draw.v = (uint16_t)(blink < 6u ? 0x40u : 0x60u);
            draw.clut = (uint16_t)(segment < progress ? 0x3A3Fu : 0x3A7Fu);
            draw.tpage = 0x020Fu;
            draw.red = 0x67;
            hud_draw_current(r, h, &draw, 20u);
            if (ae_ram_failed(r)) return;
        }
    }

    AeHudDraw end_marker = {0};
    end_marker.kind = AE_HUD_DRAW_SPRITE;
    end_marker.asset = 0x020Fu;
    end_marker.flags = 1u;
    end_marker.tpage = 0x020Fu;
    hud_draw_current(r, h, &end_marker, 8u);
}

int32_t ae_HudPanelA_Update(AeRam *r, const AeHudHooks *h) {
    uint32_t hud = AE_HUD_PANEL_STATE_BASE;
    uint16_t area_kind = (uint16_t)ae_rd16(r, AE_HUD_AREA_CONTEXT + AE_HUD_AREA_STAGE_TYPE);
    if (area_kind == 0x1Fu) return 0;

    uint16_t state = (uint16_t)ae_rd16(r, hud + AE_HUD_PANEL_A_STATE);
    int32_t draw_y = 0;
    uint16_t timer;
    switch (state) {
    case 0:
        timer = AE_HUD_PANEL_A_LENGTH;
        ae_wr16(r, hud + AE_HUD_PANEL_A_TIMER, timer);
        ae_wr16(r, hud + AE_HUD_PANEL_A_STATE, AE_HUD_PANEL_STATE_OPEN);
        draw_y = AE_HUD_PANEL_A_LENGTH;
        break;
    case AE_HUD_PANEL_STATE_OPEN:
        timer = (uint16_t)(ae_rd16(r, hud + AE_HUD_PANEL_A_TIMER) - 2u);
        ae_wr16(r, hud + AE_HUD_PANEL_A_TIMER, timer);
        if ((int16_t)timer < 0) {
            ae_wr16(r, hud + AE_HUD_PANEL_A_STATE, AE_HUD_PANEL_STATE_HOLD);
            ae_wr16(r, hud + AE_HUD_PANEL_A_TIMER, AE_HUD_PANEL_A_LENGTH);
            draw_y = 0;
        } else draw_y = (int16_t)timer;
        break;
    case AE_HUD_PANEL_STATE_HOLD:
        timer = (uint16_t)(ae_rd16(r, hud + AE_HUD_PANEL_A_TIMER) - 1u);
        ae_wr16(r, hud + AE_HUD_PANEL_A_TIMER, timer);
        if ((int16_t)timer < 0) {
            ae_wr16(r, hud + AE_HUD_PANEL_A_STATE, AE_HUD_PANEL_STATE_CLOSE);
            ae_wr16(r, hud + AE_HUD_PANEL_A_TIMER, AE_HUD_PANEL_A_LENGTH);
        }
        break;
    case AE_HUD_PANEL_STATE_CLOSE:
        timer = (uint16_t)(ae_rd16(r, hud + AE_HUD_PANEL_A_TIMER) - 2u);
        ae_wr16(r, hud + AE_HUD_PANEL_A_TIMER, timer);
        if ((int16_t)timer < 0) {
            ae_wr16(r, hud + AE_HUD_PANEL_A_STATE, AE_HUD_PANEL_STATE_OFF);
        }
        draw_y = hud_s16(AE_HUD_PANEL_A_LENGTH - (int16_t)timer);
        if ((int16_t)draw_y < 0) draw_y = 0;
        break;
    case AE_HUD_PANEL_STATE_OFF:
        ae_wr32(r, hud + AE_HUD_HUD_FLAGS,
                ae_rd32(r, hud + AE_HUD_HUD_FLAGS) & ~AE_HUD_PANEL_A_FLAG);
        return 0;
    default:
        draw_y = 0;
        break;
    }

    if (ae_rds16(r, AE_HUD_AREA_CONTEXT + AE_HUD_AREA_PANEL_A_ENABLED) > 0)
        ae_HudPanelA_DrawIcon(r, h, AE_HUD_PANEL_A_ICON_X,
                              hud_s16(draw_y + AE_HUD_PANEL_Y_BASE));
    return 0;
}

int32_t ae_HudPanelB_Update(AeRam *r, const AeHudHooks *h) {
    uint32_t hud = AE_HUD_PANEL_STATE_BASE;
    uint16_t state = (uint16_t)ae_rd16(r, hud + AE_HUD_PANEL_B_STATE);
    int32_t slide = 0;
    switch (state) {
    case 0:
        slide = AE_HUD_PANEL_B_LENGTH;
        ae_wr16(r, hud + AE_HUD_PANEL_B_STATE, AE_HUD_PANEL_STATE_OPEN);
        ae_wr16(r, hud + AE_HUD_PANEL_B_TIMER, AE_HUD_PANEL_B_LENGTH);
        break;
    case AE_HUD_PANEL_STATE_OPEN: {
        uint16_t timer = (uint16_t)(ae_rd16(r, hud + AE_HUD_PANEL_B_TIMER) - 2u);
        ae_wr16(r, hud + AE_HUD_PANEL_B_TIMER, timer);
        if ((int16_t)timer < 0) {
            ae_wr16(r, hud + AE_HUD_PANEL_B_STATE, AE_HUD_PANEL_STATE_HOLD);
            ae_wr16(r, hud + AE_HUD_PANEL_B_TIMER, AE_HUD_PANEL_B_LENGTH);
            slide = 0;
        } else slide = (int16_t)timer;
        break;
    }
    case AE_HUD_PANEL_STATE_HOLD: {
        uint16_t timer = (uint16_t)(ae_rd16(r, hud + AE_HUD_PANEL_B_TIMER) - 1u);
        ae_wr16(r, hud + AE_HUD_PANEL_B_TIMER, timer);
        if ((int16_t)timer < 0) {
            ae_wr16(r, hud + AE_HUD_PANEL_B_STATE, AE_HUD_PANEL_STATE_CLOSE);
            ae_wr16(r, hud + AE_HUD_PANEL_B_TIMER, AE_HUD_PANEL_B_LENGTH);
            slide = 0;
        }
        break;
    }
    case AE_HUD_PANEL_STATE_CLOSE: {
        uint16_t timer = (uint16_t)(ae_rd16(r, hud + AE_HUD_PANEL_B_TIMER) - 2u);
        ae_wr16(r, hud + AE_HUD_PANEL_B_TIMER, timer);
        if ((int16_t)timer < 0) {
            ae_wr16(r, hud + AE_HUD_PANEL_B_STATE, AE_HUD_PANEL_STATE_OFF);
        }
        slide = hud_s16(AE_HUD_PANEL_B_LENGTH - (int16_t)timer);
        if ((int16_t)slide < 0) slide = 0;
        break;
    }
    case AE_HUD_PANEL_STATE_OFF:
        ae_wr32(r, hud + AE_HUD_HUD_FLAGS,
                ae_rd32(r, hud + AE_HUD_HUD_FLAGS) & ~AE_HUD_PANEL_B_FLAG);
        return 0;
    default:
        slide = 0;
        break;
    }

    ae_HudDrawDigit(r, h, AE_HUD_PANEL_B_ICON_X,
                    hud_s16(slide + AE_HUD_PANEL_B_DIGIT_Y), -1);
    uint32_t number = ae_rd16(r, AE_HUD_AREA_CONTEXT + AE_HUD_AREA_HUD_B_VALUE);
    int32_t number_y = hud_s16(slide + AE_HUD_PANEL_B_DIGIT_Y);
    uint16_t animation = (uint16_t)ae_rd16(r, hud + AE_HUD_COUNTER_TIMER);
    int32_t number_to_draw = (int32_t)number - 1;
    int draw_number = 1;
    if ((int16_t)animation != 0) {
        animation = (uint16_t)(animation - 1u);
        ae_wr16(r, hud + AE_HUD_COUNTER_TIMER, animation);
        if ((int16_t)animation < 9) {
            number_y = hud_s16(slide + AE_HUD_PANEL_B_NUMBER_Y +
                               (((int16_t)animation & 1) << 1));
            draw_number = 1;
        } else if ((int16_t)animation < 26) {
            draw_number = ((int16_t)animation & 2) != 0;
            number_to_draw = (int32_t)number - 2;
        } else {
            draw_number = 0;
        }
    }
    if (draw_number)
        ae_HudDrawNumber(r, h, AE_HUD_PANEL_B_NUMBER_X, number_y, number_to_draw);
    ae_HudPanelB_DrawSprite(r, h, AE_HUD_PANEL_B_SPRITE_X,
                            hud_s16(slide + AE_HUD_PANEL_B_SPRITE_Y));
    if (slide < 28) ae_wr16(r, hud + AE_HUD_PANEL_B_SLIDE, (uint32_t)(slide - 28));
    else ae_wr16(r, hud + AE_HUD_PANEL_B_SLIDE, 0u);
    return 0;
}

int32_t ae_HudCounterC_Update(AeRam *r, const AeHudHooks *h) {
    AeHudHooks nested;
    const AeHudHooks *nested_hooks = h;
    if (h != NULL) {
        nested = *h;
        nested.stack_pointer -= 24u;
        nested_hooks = &nested;
    }
    if (ae_HudCounterC_Draw(r, nested_hooks) != 0) {
        uint32_t flags = ae_rd32(r, AE_HUD_HUD_STATE + AE_HUD_HUD_FLAGS);
        ae_wr32(r, AE_HUD_HUD_STATE + AE_HUD_HUD_FLAGS, flags & ~AE_HUD_COUNTER_FLAG);
    }
    return 0;
}


static int32_t hud_area_feature30(AeRam *r) {
    return ae_AreaFeature30GetState(r);
}

int32_t ae_RadarUsable(AeRam *r) {
    uint32_t area = ae_rd16(r, AE_HUD_STAGE_STATE + AE_HUD_STAGE_CURRENT_AREA);
    uint32_t area_kind = ae_rd16(r, AE_HUD_STAGE_STATE + AE_HUD_STAGE_AREA_KIND);
    if (area - 0x58u < 0x0Bu || area_kind == 0x0Bu) return 0;

    uint16_t player_state = (uint16_t)ae_rd16(r, AE_HUD_PLAYER + AE_HUD_PLAYER_STATE);
    if (ae_player_is_swimming(player_state)) return 0;
    if (ae_rd8(r, AE_HUD_PLAYER + AE_HUD_PLAYER_CURRENT_GADGET) != 2u) return 0;
    return ae_PlayerCanExitStage(r) > 0;
}

void ae_HudDrawIconSprite(AeRam *r, const AeHudHooks *h, int32_t index,
                          int32_t x, int32_t y, int32_t semi_transparent) {
    AeHudDraw draw = {0};
    uint32_t entry = AE_HUD_ICON_TABLE + (uint32_t)index * AE_HUD_ICON_RECORD_STRIDE;
    draw.kind = AE_HUD_DRAW_SPRITE;
    draw.asset = (uint32_t)index;
    draw.flags = semi_transparent != 0;
    draw.x = hud_s16(x);
    draw.y = hud_s16(y);
    draw.width = (int32_t)ae_rd8(r, entry);
    draw.height = (int32_t)ae_rd8(r, entry + AE_HUD_TABLE_RECORD_HEIGHT);
    draw.u = (uint16_t)ae_rd8(r, entry + AE_HUD_TABLE_RECORD_UV);
    draw.v = (uint16_t)ae_rd8(r, entry + AE_HUD_TABLE_RECORD_V);
    draw.clut = (uint16_t)ae_rd16(r, entry + AE_HUD_TABLE_RECORD_CLUT);
    draw.tpage = (uint16_t)(((uint16_t)ae_rd16(r, entry + AE_HUD_TABLE_RECORD_TPAGE) &
                             0x09FFu) | 0x0200u);
    draw.red = 0x65;
    draw.green = 0;
    draw.blue = 0;
    if (ae_ram_failed(r)) return;
    hud_draw_current(r, h, &draw, AE_HUD_RENDER_PACKET_SPRITE);
}

void ae_HudDrawGadgetIcons(AeRam *r, const AeHudHooks *h) {
    int32_t base_y = (ae_rd32(r, AE_HUD_HUD_STATE + AE_HUD_PANEL_FLAGS_FIELD) &
                      AE_HUD_GADGET_ICON_FLAG) != 0u ?
                     (int32_t)ae_rd16(r, AE_HUD_HUD_STATE + AE_HUD_HUD_PANEL_B_Y) : 0;
    base_y = hud_s16(base_y + AE_HUD_GADGET_ICON_BASE_Y);
    int32_t first_icon = 1;
    if ((ae_rd32(r, AE_HUD_FRAME_COUNTER) & 4u) != 0u &&
        ae_rd8(r, AE_HUD_PLAYER + AE_HUD_PLAYER_CURRENT_GADGET) == 2u &&
        ae_rd32(r, AE_HUD_GADGET_ICON_STATE + AE_HUD_GADGET_ICON_MODE) == 1u &&
        ae_rd32(r, AE_HUD_GADGET_ICON_STATE + AE_HUD_GADGET_ICON_CHARGE) != 0u) {
        first_icon = 2;
    }
    ae_HudDrawIconSprite(r, h, first_icon, 24, base_y, 0);

    int32_t game_mode = ae_rd8(r, AE_HUD_STATE_GADGET_MODE + AE_HUD_GADGET_MODE_KIND);
    int32_t area_mode = ae_rd8(r, AE_HUD_STATE_GADGET_MODE + AE_HUD_GADGET_MODE_FLAGS);
    int32_t frame_sprite = (game_mode == 1 || area_mode == 1) ? 0x2D : 24;
    int32_t radar_blink = (game_mode == 1 || area_mode == 1) ? 1 : 0;
    ae_HudDrawIconSprite(r, h, 4, 25, hud_s16(base_y + 16), radar_blink);

    uint32_t blink = ae_rd8(r, AE_HUD_STATE_GADGET_MODE + AE_HUD_GADGET_ICON_BLINK);
    if (blink != 0u) {
        ae_wr8(r, AE_HUD_STATE_GADGET_MODE + AE_HUD_GADGET_ICON_BLINK, blink - 1u);
        if ((blink & 1u) != 0u)
            ae_HudDrawIconSprite(r, h, 3, 45, hud_s16(base_y + 18), 0);
    }

    int32_t radar = ae_RadarUsable(r);
    int32_t radar_icon = radar ? 6 : 5;
    if (ae_rd8(r, AE_HUD_PLAYER + AE_HUD_PLAYER_CURRENT_GADGET) == 3u)
        radar_icon = 7;
    ae_HudDrawIconSprite(r, h, radar_icon, 45, hud_s16(base_y + 18), radar_blink ^ 1);
    ae_HudDrawIconSprite(r, h, 0, hud_s16(frame_sprite - 2), hud_s16(base_y + 14), 1);
}

int32_t ae_GetDAT_800B01C2(AeRam *r) {
    return ae_rds16(r, AE_HUD_DAT_800B01C2);
}

uint32_t ae_TextNextGlyph(AeRam *r, uint32_t string, uint32_t out_code) {
    uint32_t code = ae_rd8(r, string);
    uint32_t next = string + 1u;
    if (code == 0x0Au || code == 0x0Bu) {
        uint32_t page = code == 0x0Au ? 0x100u : 0x200u;
        code = page | ae_rd8(r, next);
        ++next;
    }
    ae_wr16(r, out_code, code);
    return next;
}

void ae_TextSetCursor(AeRam *r, int32_t x, int32_t y) {
    ae_wr16(r, AE_HUD_TEXT_CURSOR, (uint32_t)x);
    ae_wr16(r, AE_HUD_TEXT_CURSOR + AE_HUD_TEXT_START_X, (uint32_t)x);
    ae_wr16(r, AE_HUD_TEXT_CURSOR + AE_HUD_TEXT_CURSOR_Y, (uint32_t)y);
}

void ae_MessageBoxUpdateDraw(AeRam *r, const AeHudHooks *h) {
    if (ae_rd8(r, AE_HUD_MESSAGE_BOX + AE_HUD_MESSAGE_STATE) == 0u) return;
    ae_MessageBoxDrawPrompt(r, h);
    if (ae_ram_failed(r)) return;
    ae_MessageBoxDrawWindow(r, h);
}

void ae_FontDrawGlyph(AeRam *r, const AeHudHooks *h, int32_t code,
                      int32_t clut_row, uint32_t unused) {
    (void)unused;
    uint32_t glyph_code = (uint32_t)code & 0xFFFFu;
    int32_t cursor_x = ae_rds16(r, AE_HUD_TEXT_CURSOR);
    int32_t cursor_y = ae_rds16(r, AE_HUD_TEXT_CURSOR + AE_HUD_TEXT_CURSOR_Y);
    if (glyph_code == 0x0Du) {
        ae_wr16(r, AE_HUD_TEXT_CURSOR, (uint32_t)ae_rds16(r, AE_HUD_TEXT_CURSOR + AE_HUD_TEXT_START_X));
        ae_wr16(r, AE_HUD_TEXT_CURSOR + AE_HUD_TEXT_CURSOR_Y,
                (uint32_t)hud_s16(cursor_y + AE_HUD_FONT_LINE_HEIGHT));
        return;
    }
    if ((glyph_code & 0xFF00u) == 0u && (glyph_code & 0x00FFu) < 0x10u) return;

    if ((glyph_code & 0x0100u) != 0u) {
        uint32_t button = glyph_code & 0xFFu;
        uint32_t record = AE_HUD_FONT_BUTTONS + button * AE_HUD_ICON_RECORD_STRIDE;
        int32_t draw_y = cursor_y;
        if (button == 0x0Bu) {
            draw_y = hud_s16(cursor_y - 5);
            if ((ae_rd32(r, AE_HUD_FRAME_COUNTER) & 8u) != 0u) {
                record = AE_HUD_FONT_BUTTON_BLINK;
            }
        }
        uint16_t width = (uint16_t)ae_rd16(r, record + AE_HUD_FONT_RECORD_WIDTH);
        uint16_t height = (uint16_t)ae_rd16(r, record + AE_HUD_FONT_RECORD_HEIGHT);
        uint16_t uv = (uint16_t)ae_rd16(r, record + AE_HUD_FONT_RECORD_UV);
        AeHudDraw draw = {0};
        draw.kind = AE_HUD_DRAW_SPRITE;
        draw.asset = 0x100u | button;
        draw.flags = 0;
        draw.x = cursor_x;
        draw.y = hud_s16(draw_y - (int32_t)((uint16_t)(height - 0x10u) >> 1));
        draw.width = width;
        draw.height = height;
        draw.u = uv & 0x00FFu;
        draw.v = uv >> 8;
        draw.clut = (uint16_t)ae_rd16(r, record + AE_HUD_FONT_RECORD_CLUT);
        draw.tpage = AE_HUD_BUTTON_PAGE;
        draw.red = 0x65;
        hud_draw_current(r, h, &draw, AE_HUD_RENDER_PACKET_SPRITE);
        ae_wr16(r, AE_HUD_TEXT_CURSOR,
                (uint32_t)hud_s16(cursor_x + (int32_t)width + 2));
        return;
    }

    if ((glyph_code & 0x0200u) != 0u) {
        uint32_t face = glyph_code & 0xFFu;
        uint32_t record = face - 1u < 10u ? AE_HUD_FONT_FACE_A : AE_HUD_FONT_FACE_B;
        uint16_t uv = (uint16_t)ae_rd16(r, record + AE_HUD_FONT_RECORD_UV);
        uint16_t width = (uint16_t)ae_rd16(r, record + AE_HUD_FONT_RECORD_WIDTH);
        uint16_t height = (uint16_t)ae_rd16(r, record + AE_HUD_FONT_RECORD_HEIGHT);
        AeHudDraw draw = {0};
        draw.kind = AE_HUD_DRAW_SPRITE;
        draw.asset = 0x200u | face;
        draw.x = hud_s16(cursor_x - 4);
        draw.y = hud_s16(cursor_y - 1);
        draw.width = width;
        draw.height = height;
        draw.u = uv & 0x00FFu;
        draw.v = uv >> 8;
        draw.clut = (uint16_t)ae_rd16(r, record + AE_HUD_FONT_RECORD_CLUT);
        draw.tpage = AE_HUD_BUTTON_PAGE;
        draw.red = 0x65;
        hud_draw_current(r, h, &draw, AE_HUD_RENDER_PACKET_SPRITE);
        ae_wr16(r, AE_HUD_TEXT_CURSOR,
                (uint32_t)hud_s16(cursor_x + (int32_t)width + 1));
        return;
    }

    uint32_t glyph = glyph_code & 0xFFu;
    int32_t width = (int32_t)ae_rd8(r, AE_HUD_FONT_WIDTHS + glyph);
    AeHudDraw draw = {0};
    draw.kind = AE_HUD_DRAW_GLYPH;
    draw.asset = glyph;
    draw.glyph = (int32_t)glyph_code;
    draw.x = cursor_x;
    draw.y = cursor_y;
    draw.width = width;
    draw.height = 16;
    draw.u = (uint16_t)((glyph & 0x0Fu) << 4);
    draw.v = (uint16_t)(glyph & 0x70u);
    draw.clut = (uint16_t)(((uint32_t)(clut_row + ((int32_t)glyph >> 7)) << 6) | 0x1Cu);
    draw.tpage = AE_HUD_GLYPH_PAGE;
    draw.flags = 1;
    draw.red = 0x67;
    hud_draw_current(r, h, &draw, AE_HUD_RENDER_PACKET_SPRITE);
    draw.x = hud_s16(cursor_x + 1);
    hud_draw_current(r, h, &draw, AE_HUD_RENDER_PACKET_SPRITE);

    cursor_x += width;
    if (glyph != 0xCCu && glyph != 0xCEu) ++cursor_x;
    ae_wr16(r, AE_HUD_TEXT_CURSOR, (uint32_t)hud_s16(cursor_x));
}

void ae_MessageBoxDrawText(AeRam *r, const AeHudHooks *h) {
    ae_TextSetCursor(r, 0x20, 0x28);
    uint32_t page_cursor = ae_rd32(r, AE_HUD_MESSAGE_BOX + AE_HUD_MESSAGE_PAGE_START);
    uint32_t page_end = ae_rd32(r, AE_HUD_MESSAGE_BOX + AE_HUD_MESSAGE_PAGE_END);
    uint16_t clut_row = (uint16_t)ae_rd16(r, AE_HUD_MESSAGE_BOX + AE_HUD_MESSAGE_CLUT_ROW);
    while (page_cursor < page_end) {
        page_cursor = ae_TextNextGlyph(r, page_cursor, AE_HUD_TEMP_CODE);
        ae_FontDrawGlyph(r, h, (int32_t)ae_rd16(r, AE_HUD_TEMP_CODE), clut_row, 1u);
        if (ae_ram_failed(r)) return;
    }
}

void ae_MessageBoxDrawWindow(AeRam *r, const AeHudHooks *h) {
    uint32_t message = AE_HUD_MESSAGE_BOX;
    uint32_t animation = (uint16_t)ae_rd16(r, message + AE_HUD_MESSAGE_ANIMATION) >> 1;
    ae_wr16(r, message + AE_HUD_MESSAGE_ANIMATION, animation);
    uint8_t state = (uint8_t)ae_rd8(r, message + AE_HUD_MESSAGE_STATE);
    int32_t x = 16;
    if (state == 1u) {
        if (animation < 2u && ae_rd8(r, message + AE_HUD_MESSAGE_HAS_TEXT) != 0u)
            ae_wr8(r, message + AE_HUD_MESSAGE_STATE, 2u);
        x = 16 - (int32_t)(2u * animation);
    } else if (state == 5u) {
        if (animation < 2u) ae_wr8(r, message + AE_HUD_MESSAGE_STATE, 0u);
        x = 16 - hud_div_s32_value(0x80, (int32_t)animation + 1);
    }

    AeHudDraw draw = {0};
    draw.kind = AE_HUD_DRAW_MESSAGE_FRAME;
    draw.asset = AE_HUD_SCREEN_TABLE + AE_HUD_SCREEN_TABLE_MESSAGE_FRAME;
    draw.flags = state;
    draw.x = x;
    draw.y = 0x10;
    draw.width = 0x110;
    draw.height = 0x68;
    draw.red = 0x7E;
    draw.green = 0xDD;
    draw.blue = 0xB0;
    uint32_t depth = ae_rd32(r, AE_HUD_GPU_STATE + AE_HUD_GPU_OT_DEPTH);
    uint32_t ordering_entry = AE_HUD_GPU_STATE + AE_HUD_GPU_OT_FIRST_ENTRY +
                              depth * AE_HUD_GPU_OT_STRIDE;
    hud_draw(r, h, &draw, AE_HUD_MESSAGE_FRAME_PACKET_BYTES,
             ae_rd32(r, AE_HUD_GPU_STATE + AE_HUD_GPU_PRIMITIVE), 1, ordering_entry);
}

static uint32_t hud_next_glyph(AeRam *r, uint32_t pointer, uint16_t *code) {
    uint32_t first = ae_rd8(r, pointer);
    uint32_t next = pointer + 1u;
    if (first == 0x0Au || first == 0x0Bu) {
        uint32_t prefix = first == 0x0Au ? 0x100u : 0x200u;
        *code = (uint16_t)(prefix | ae_rd8(r, next));
        return next + 1u;
    }
    *code = (uint16_t)first;
    return next;
}

static void hud_play_sfx(AeRam *r, const AeHudHooks *h, int32_t id) {
    const AeSoundHw *sound = h != NULL ? h->sound_hw : NULL;
    (void)ae_PlaySfx(r, sound, id);
}

void ae_MessageBoxDrawPrompt(AeRam *r, const AeHudHooks *h) {
    uint32_t message = AE_HUD_MESSAGE_BOX;
    uint8_t state = (uint8_t)ae_rd8(r, message + AE_HUD_MESSAGE_STATE);
    if (state == 1u || state == 5u) return;

    ae_MessageBoxDrawText(r, h);
    if (ae_ram_failed(r)) return;
    uint32_t pressed = ae_rd32(r, AE_HUD_PAD_HELD) & ae_rd32(r, AE_HUD_PAD_CHANGED);
    uint32_t any_page_button = pressed & 0xEAu;
    int any_page = any_page_button != 0u;

    if (state == 4u) {
        ae_TextSetCursor(r, 0xB8, 0xA0);
        ae_FontDrawGlyph(r, h, 0x104, 0x1F0, 1u);
        if (ae_ram_failed(r)) return;
        if (ae_rd8(r, message + AE_HUD_MESSAGE_STATE) == state &&
            (any_page || (pressed & 0x800u) != 0u)) {
            ae_wr8(r, message + AE_HUD_MESSAGE_STATE, 5u);
            ae_wr16(r, message + AE_HUD_MESSAGE_ANIMATION, 0x80u);
            hud_play_sfx(r, h, 0);
            return;
        }
    }

    if ((pressed & 0x800u) != 0u) {
        ae_wr8(r, message + AE_HUD_MESSAGE_STATE, 5u);
        ae_wr16(r, message + AE_HUD_MESSAGE_ANIMATION, 0x80u);
        hud_play_sfx(r, h, 0);
        return;
    }

    if (state == 3u) {
        uint8_t blink = (uint8_t)(ae_rd8(r, message + AE_HUD_MESSAGE_BLINK) + 1u);
        ae_wr8(r, message + AE_HUD_MESSAGE_BLINK, blink);
        if ((blink & 8u) == 0u) {
            ae_TextSetCursor(r, 0xB8, 0xA0);
            ae_FontDrawGlyph(r, h, 0x104, 0x1F0, 1u);
        }
        if (ae_ram_failed(r) || !any_page) return;
        uint32_t next_page = ae_rd32(r, message + AE_HUD_MESSAGE_PAGE_END) + 1u;
        ae_wr8(r, message + AE_HUD_MESSAGE_STATE, 2u);
        ae_wr32(r, message + AE_HUD_MESSAGE_PAGE_END, next_page);
        ae_wr32(r, message + AE_HUD_MESSAGE_PAGE_START, next_page);
        hud_play_sfx(r, h, 0);
        return;
    }

    uint32_t cursor = ae_rd32(r, message + AE_HUD_MESSAGE_PAGE_END);
    uint16_t code;
    do {
        cursor = hud_next_glyph(r, cursor, &code);
        ae_wr32(r, message + AE_HUD_MESSAGE_PAGE_END, cursor);
    } while (code != 0u && code != 0x0Fu && !ae_ram_failed(r));
    if (code == 0u || code == 0x0Fu)
        ae_wr32(r, message + AE_HUD_MESSAGE_PAGE_END, cursor - 1u);

    uint16_t delay = (uint16_t)(ae_rd16(r, message + AE_HUD_MESSAGE_TERMINAL_DELAY) - 1u);
    ae_wr16(r, message + AE_HUD_MESSAGE_TERMINAL_DELAY, delay);
    if (delay != 0u || ae_ram_failed(r)) return;
    ae_wr16(r, message + AE_HUD_MESSAGE_TERMINAL_DELAY, 1u);
    uint16_t previous_code = (uint16_t)ae_rd16(r, message + AE_HUD_MESSAGE_CURRENT_CODE);
    cursor = ae_rd32(r, message + AE_HUD_MESSAGE_PAGE_END);
    cursor = hud_next_glyph(r, cursor, &code);
    ae_wr32(r, message + AE_HUD_MESSAGE_PAGE_END, cursor);
    ae_wr16(r, message + AE_HUD_MESSAGE_CURRENT_CODE, code);
    if (code >= 0x10u) return;

    if (code == 0u) {
        ae_wr8(r, message + AE_HUD_MESSAGE_STATE, 4u);
        ae_wr32(r, message + AE_HUD_MESSAGE_PAGE_END, cursor - 1u);
    } else if (code == 0x0Fu) {
        ae_wr8(r, message + AE_HUD_MESSAGE_STATE, 3u);
        ae_wr32(r, message + AE_HUD_MESSAGE_PAGE_END, cursor - 1u);
        ae_wr8(r, message + AE_HUD_MESSAGE_BLINK, 0u);
    } else if (code == 0x0Du && previous_code == code) {
        ae_wr16(r, message + AE_HUD_MESSAGE_TERMINAL_DELAY, code);
    }
}

