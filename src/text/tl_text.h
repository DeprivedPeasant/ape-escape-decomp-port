/* Portable C text layout of the game's font (FontDrawGlyph / TextNextGlyph / TextLineWidth, notes/26, notes/28).
 *
 * A string in the game's font codes becomes a list of textured quads in PlayStation VRAM terms (texture page, CLUT,
 * u / v / w / h) at screen positions, exactly as FontDrawGlyph emits its sprites. The tables (glyph widths 0x800B3640,
 * button icons 0x800B3738, portrait icons 0x800B37A0 / 0x800B37C0, blink frame 0x800B3798) are read from the EXE image
 * passed to tl_font_init; no game data is embedded.
 */
#ifndef TL_TEXT_H
#define TL_TEXT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t width[256];      /* glyph advance widths */
    uint16_t button[16][4];  /* button icon records {clut, uv, w, h} for 0A xx */
    uint16_t blink[4];       /* alternate record of button icon 0x0B while (frame & 8) */
    uint16_t face[2][4];     /* portrait records: [0] for 0B 01..0A, [1] for the others */
} TlFont;

enum { TL_GLYPH = 0, TL_BUTTON = 1, TL_FACE = 2 };

typedef struct {
    int kind;        /* TL_GLYPH / TL_BUTTON / TL_FACE */
    int code;        /* font code, or the icon index */
    int x, y;        /* screen position of the sprite's top left corner */
    int u, v, w, h;  /* texture rectangle inside the texture page */
    int tpage;       /* GPU texture page word (0x257 glyphs, 0x217 icons) */
    int clut;        /* GPU CLUT word */
    int bold;        /* 1: FontDrawGlyph draws the glyph twice, at x and x + 1 */
} TlQuad;

/* returns 0 on success */
int tl_font_init(TlFont *f, const uint8_t *exe, size_t exe_size);

/* Lay out a 0-terminated string from (x, y) like TextSetCursor + MessageBoxDrawString with CLUT row clut_row (the
 * game passes 0x1F0; codes >= 0x80 use the next row). frame = g_FrameCounter (blinking icon). Returns the number of
 * quads written (at most max); *end_x / *end_y receive the final cursor when not NULL. */
int tl_layout(const TlFont *f, const uint8_t *s, int x, int y, int clut_row, uint32_t frame, TlQuad *out, int max,
              int *end_x, int *end_y);

/* TextLineWidth: sum of the glyph widths up to 0 or a new line (no inter-glyph spacing, as the game centres text) */
int tl_line_width(const TlFont *f, const uint8_t *s);


/* Message box (MessageBoxOpen / SetText / DrawPrompt / DrawWindow, notes/32): pages separated by 0x0F appear whole;
 * a blinking prompt (button icon 4 at 0xB8, 0xA0, visible while (blink & 8) == 0) waits at a page end; buttons 0xEA
 * turn the page, at the text end they close the box; Start (0x800) closes it at any time. */
typedef struct {
    int state;               /* 0 closed, 1 opening, 2 page shown, 3 page end prompt, 4 text end prompt, 5 closing */
    const uint8_t *text, *page, *page_end;
    int anim;                /* slide counter, 0x80 halved every frame */
    int blink;
    int has_text;
} TlMsgBox;

void tl_msgbox_open(TlMsgBox *m);
void tl_msgbox_set_text(TlMsgBox *m, const uint8_t *text);
/* one frame; pressed = g_PadHeld & g_PadChanged. Returns 1 when the game plays SFX 0 (page turn / close). */
int tl_msgbox_update(TlMsgBox *m, unsigned pressed);
/* window x offset of this frame (MessageBoxDrawWindow: 16 when open, sliding while opening / closing) */
int tl_msgbox_window_x(const TlMsgBox *m);
/* quads of the visible page at (0x20, 0x28) plus the prompt icon when shown */
int tl_msgbox_layout(const TlFont *f, const TlMsgBox *m, uint32_t frame, TlQuad *out, int max);

#ifdef __cplusplus
}
#endif
#endif
