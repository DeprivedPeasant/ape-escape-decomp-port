#include "../core/ae_missa.h"

#include "ae_pausemenu.h"
#include "../world/ae_progress.h"

#define AE_MISSA_HUD_STAGE_FLAGS          0x800F4350u
#define AE_MISSA_HUD_MONKEY_INFO_TABLE    0x800B4CF0u
#define AE_MISSA_HUD_MONKEY_INFO_STRIDE   0x88u
#define AE_MISSA_HUD_MONKEY_INDEX         0x7Cu
#define AE_MISSA_HUD_MONKEY_SCORE         0x1Cu
#define AE_MISSA_HUD_MONKEY_NAME          0x5Eu
#define AE_MISSA_HUD_MONKEY_TITLE          0x6Au
#define AE_MISSA_HUD_TEXT_AREA_TITLE      0x800B0214u
#define AE_MISSA_HUD_TEXT_STAT_SPEED      0x800B021Cu
#define AE_MISSA_HUD_TEXT_STAT_ATTACK     0x800B0224u
#define AE_MISSA_HUD_TEXT_STAT_ALERT      0x800B022Cu
#define AE_MISSA_HUD_TEXT_TIME_RANK       0x800B0234u
#define AE_MISSA_HUD_TITLE_WIDTH          384
#define AE_MISSA_HUD_TITLE_Y              200
#define AE_MISSA_HUD_TEXT_PAGE            2
#define AE_MISSA_HUD_BADGE_FLAG           0x10u

static int32_t missa_hud_s16(uint32_t value)
{
    uint32_t low = value & 0xFFFFu;
    return low < 0x8000u ? (int32_t)low : (int32_t)low - 0x10000;
}

static int32_t missa_hud_s32(uint32_t value)
{
    if ((value & 0x80000000u) == 0u) return (int32_t)value;
    return (int32_t)((int64_t)value - 0x100000000LL);
}

static int32_t missa_hud_div_trunc(int32_t numerator, int32_t denominator)
{
    if (denominator == 0 ||
        (numerator == INT32_MIN && denominator == -1)) {
        return 0;
    }
    return numerator / denominator;
}

static int32_t missa_hud_call(AeRam *r, const AeMissaHooks *hooks,
                              AeMissaHookFn callback, const char *name,
                              uint32_t a0, uint32_t a1,
                              uint32_t a2, uint32_t a3)
{
    if (hooks == NULL || callback == NULL) {
        ae_ram_fail(r, "HudDrawMonkeyInfo: %s hook missing", name);
        return 0;
    }
    return callback(hooks->user, r, a0, a1, a2, a3);
}

static void missa_hud_draw_text(AeRam *r, const AeMissaHooks *hooks,
                                uint32_t text, int32_t x, int32_t y)
{
    ae_DrawTextSprites(r, hooks != NULL ? hooks->progress : NULL,
                       text, x, y, AE_MISSA_HUD_TEXT_PAGE);
}

/* 8005F68C. The medal choice is game logic; the native renderer owns packet
 * allocation, ordering-table links, and GPU packet writes. */
void ae_HudDrawTimeRankIcon(AeRam *r, const AeMissaHooks *hooks,
                            int32_t x, int32_t y, uint32_t time,
                            uint32_t thresholds)
{
    uint32_t rank = 0u;
    uint32_t threshold0 = (uint32_t)missa_hud_s16(ae_rd16(r, thresholds));
    uint32_t threshold1 = (uint32_t)missa_hud_s16(ae_rd16(r, thresholds + 2u));
    uint32_t threshold2 = (uint32_t)missa_hud_s16(ae_rd16(r, thresholds + 4u));

    if (threshold0 >= time) rank = 501u;
    if (threshold1 >= time) --rank;
    if (threshold2 >= time) --rank;
    if (rank == 0u) return;

    (void)missa_hud_call(r, hooks,
        hooks != NULL ? hooks->HudDrawTimeRankIcon : NULL,
        "HudDrawTimeRankIcon", (uint32_t)x, (uint32_t)y, rank, time);
}

/* 80060448. Strings, centering, rank digits and stat values are game state;
 * DrawTextSprites and the named packet builders cross the renderer boundary. */
void ae_HudDrawMonkeyInfo(AeRam *r, const AeMissaHooks *hooks,
                          uint32_t monkey)
{
    if (monkey == 0u) return;

    ae_SetDAT_800BEF80(r, 0u);
    missa_hud_draw_text(r, hooks, AE_MISSA_HUD_TEXT_AREA_TITLE, 56, 24);

    uint32_t info_index = ae_rd8(r, monkey + AE_MISSA_HUD_MONKEY_INDEX);
    uint32_t info = ae_rd32(r, AE_MISSA_HUD_MONKEY_INFO_TABLE) +
                    info_index * AE_MISSA_HUD_MONKEY_INFO_STRIDE;
    missa_hud_draw_text(r, hooks, info + AE_MISSA_HUD_MONKEY_NAME, 32, 48);

    int32_t score = missa_hud_s16(ae_rd16(r, info + AE_MISSA_HUD_MONKEY_SCORE));
    int32_t thousands = missa_hud_div_trunc(score, 1000);
    int32_t last_digit = thousands -
                         missa_hud_div_trunc(thousands, 10) * 10;
    uint32_t rank_text_character = (uint32_t)(last_digit < 6
        ? last_digit + 106 : last_digit + 116);
    ae_wr8(r, AE_MISSA_HUD_TEXT_TIME_RANK + 13u, rank_text_character);
    missa_hud_draw_text(r, hooks, AE_MISSA_HUD_TEXT_TIME_RANK, 60, 64);

    missa_hud_draw_text(r, hooks, AE_MISSA_HUD_TEXT_STAT_SPEED, 224, 32);
    missa_hud_draw_text(r, hooks, AE_MISSA_HUD_TEXT_STAT_ATTACK, 224, 48);
    missa_hud_draw_text(r, hooks, AE_MISSA_HUD_TEXT_STAT_ALERT, 224, 64);

    uint32_t title = info + AE_MISSA_HUD_MONKEY_TITLE;
    int32_t title_width = missa_hud_call(r, hooks,
        hooks != NULL ? hooks->HudTextWidth : NULL, "HudTextWidth",
        title, 0, 0, 0);
    int32_t centered_x = missa_hud_div_trunc(
        missa_hud_s32((uint32_t)AE_MISSA_HUD_TITLE_WIDTH -
                      (uint32_t)title_width), 2);
    missa_hud_draw_text(r, hooks, title, centered_x, AE_MISSA_HUD_TITLE_Y);

    uint32_t stage_flag = ae_rd32(r, AE_MISSA_HUD_STAGE_FLAGS) &
                          AE_MISSA_HUD_BADGE_FLAG;
    (void)missa_hud_call(r, hooks,
        hooks != NULL ? hooks->RendererHudMonkeyBadge : NULL,
        "RendererHudMonkeyBadge", stage_flag, 0u, 0u, 0u);

    int32_t hundreds = missa_hud_div_trunc(score, 100);
    int32_t tens_value = missa_hud_div_trunc(score, 10);
    uint32_t pip_values[3] = {
        (uint32_t)(hundreds - missa_hud_div_trunc(hundreds, 10) * 10),
        (uint32_t)(tens_value - hundreds * 10),
        (uint32_t)(score - tens_value * 10)
    };
    for (uint32_t row = 0; row < 3u; ++row) {
        (void)missa_hud_call(r, hooks,
            hooks != NULL ? hooks->HudDrawStatPips : NULL, "HudDrawStatPips",
            row, pip_values[row], 0u, 0u);
    }

    (void)missa_hud_call(r, hooks,
        hooks != NULL ? hooks->HudDrawFrameBox : NULL, "HudDrawFrameBox",
        28u, 20u, 80u, 24u);
    (void)missa_hud_call(r, hooks,
        hooks != NULL ? hooks->HudDrawFrameBox : NULL, "HudDrawFrameBox",
        28u, 44u, 176u, 40u);
    (void)missa_hud_call(r, hooks,
        hooks != NULL ? hooks->HudDrawFrameBox : NULL, "HudDrawFrameBox",
        220u, 28u, 128u, 56u);
    (void)missa_hud_call(r, hooks,
        hooks != NULL ? hooks->HudDrawFrameBox : NULL, "HudDrawFrameBox",
        28u, 196u, 320u, 24u);
}
