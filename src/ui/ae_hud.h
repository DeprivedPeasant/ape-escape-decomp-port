#ifndef AE_HUD_H
#define AE_HUD_H

#include "../core/ae_ram.h"
#include "../sound/ae_sound.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Renderer requests emitted by the original HUD code. These describe the
 * image-space result; the native renderer owns texture lookup and drawing. */
typedef enum AeHudDrawKind {
    AE_HUD_DRAW_SPRITE = 1,
    AE_HUD_DRAW_QUAD = 2,
    AE_HUD_DRAW_GLYPH = 3,
    AE_HUD_DRAW_STAGE_OVERLAY = 4,
    AE_HUD_DRAW_GAUGE = 5,
    AE_HUD_DRAW_MESSAGE_FRAME = 6
} AeHudDrawKind;

typedef struct AeHudDraw {
    AeHudDrawKind kind;
    uint32_t primitive_address;
    uint32_t ordering_entry;
    uint32_t depth;
    uint32_t packet_bytes;
    uint32_t asset;
    uint32_t flags;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    int32_t x2;
    int32_t y2;
    int32_t x3;
    int32_t y3;
    int32_t x4;
    int32_t y4;
    int32_t red;
    int32_t green;
    int32_t blue;
    int32_t glyph;
    uint16_t u;
    uint16_t v;
    uint16_t clut;
    uint16_t tpage;
} AeHudDraw;

typedef struct AeHudHooks {
    void *user;
    uint32_t stack_pointer; /* Original o32 entry SP for stack-backed SDK arguments. */
    /* Each draw hook receives the logical sprite / primitive and ordering
     * depth. Return is deliberately omitted: packet allocation is renderer
     * state and the port advances the original cursor by the command size. */
    void (*draw_sprite)(void *user, AeRam *r, const AeHudDraw *draw);
    void (*draw_quad)(void *user, AeRam *r, const AeHudDraw *draw);
    void (*draw_glyph)(void *user, AeRam *r, const AeHudDraw *draw);
    void (*draw_stage_overlay)(void *user, AeRam *r, const AeHudDraw *draw);
    void (*draw_gauge)(void *user, AeRam *r, const AeHudDraw *draw);
    void (*draw_message_frame)(void *user, AeRam *r, const AeHudDraw *draw);

    /* Original function outside this port: overlay code is loaded separately. */
    int32_t (*Ovl_8013D424)(void *user, AeRam *r, int32_t overlay_kind);
    /* Public SDK / hardware boundaries. */
    void (*DR_MOVE)(void *user, AeRam *r, uint32_t primitive, uint32_t rect,
                    uint32_t source_x, uint32_t source_y);
    void (*ClearMemory)(void *user, AeRam *r, uint32_t address,
                        uint32_t value, uint32_t size);
    int32_t (*ModelDrawScreenSpace)(void *user, AeRam *r, uint32_t model,
                                    uint32_t position, uint32_t rotation,
                                    uint32_t scale);
    /* Frame-count and area state are read by already-ported functions; these
     * callbacks represent only the SDK draw path used by the checker. */
    const AeSoundHw *sound_hw;
} AeHudHooks;

/* HUD / stage overlay functions from the task table. */
int32_t ae_RadarUsable(AeRam *r);
void ae_HudDrawIconSprite(AeRam *r, const AeHudHooks *h, int32_t index,
                          int32_t x, int32_t y, int32_t semi_transparent);
void ae_HudDrawGadgetIcons(AeRam *r, const AeHudHooks *h);
int32_t ae_GetDAT_800B01C2(AeRam *r);
void ae_StageDrawScreenOverlay(AeRam *r, const AeHudHooks *h,
                               uint32_t a0, uint32_t a1);
void ae_HudDrawHealth(AeRam *r, const AeHudHooks *h);
void ae_AreaTexCopyDraw(AeRam *r, const AeHudHooks *h, int32_t index);
void ae_HudPanelA_DrawIcon(AeRam *r, const AeHudHooks *h, int32_t x, int32_t y);
void ae_HudPanelB_DrawSprite(AeRam *r, const AeHudHooks *h, int32_t x, int32_t y);
void ae_HudDrawDigit(AeRam *r, const AeHudHooks *h, int32_t x, int32_t y,
                     int32_t digit);
void ae_HudDrawNumber(AeRam *r, const AeHudHooks *h, int32_t x, int32_t y,
                      int32_t number);
void ae_HudDrawTime(AeRam *r, const AeHudHooks *h, int32_t x, int32_t y,
                    int32_t frames);
void ae_HudDrawIcon16(AeRam *r, const AeHudHooks *h, int32_t x, int32_t y,
                      int32_t index);
int32_t ae_HudCounterC_Draw(AeRam *r, const AeHudHooks *h);
void ae_HudDrawTimerGauge(AeRam *r, const AeHudHooks *h, int32_t frames);
void ae_HudDrawGadgetSlotsFrame(AeRam *r, const AeHudHooks *h);
void ae_HudDrawGadgetSlotIcons(AeRam *r, const AeHudHooks *h);
void ae_HudDrawGadgetSlots(AeRam *r, const AeHudHooks *h);
uint32_t ae_HudDrawGadgetIcon(AeRam *r, const AeHudHooks *h, int32_t x,
                              int32_t y, int32_t gadget,
                              uint32_t primitive);

/* Font and message-box functions from the same task table. */
void ae_FontDrawGlyph(AeRam *r, const AeHudHooks *h, int32_t code,
                      int32_t clut_row, uint32_t unused);
uint32_t ae_TextNextGlyph(AeRam *r, uint32_t string, uint32_t out_code);
void ae_TextSetCursor(AeRam *r, int32_t x, int32_t y);
void ae_MessageBoxUpdateDraw(AeRam *r, const AeHudHooks *h);
void ae_MessageBoxDrawText(AeRam *r, const AeHudHooks *h);
void ae_MessageBoxDrawPrompt(AeRam *r, const AeHudHooks *h);
void ae_MessageBoxDrawWindow(AeRam *r, const AeHudHooks *h);

/* HUD state machines from the task table. */
int32_t ae_HudPanelA_Update(AeRam *r, const AeHudHooks *h);
int32_t ae_HudPanelB_Update(AeRam *r, const AeHudHooks *h);
int32_t ae_HudCounterC_Update(AeRam *r, const AeHudHooks *h);

#ifdef __cplusplus
}
#endif
#endif
