# Native text layout (`tools/native_text/`)

Portable C99 (`tl_text.c` / `tl_text.h`, no game data inside) for the port's message boxes and menus: a string in
the game's font codes (notes/26, notes/28) becomes textured sprites exactly as `FontDrawGlyph` emits them.

Rules (from `FontDrawGlyph`, `TextNextGlyph`, `TextLineWidth`):

* glyph code c >= 0x10: sprite of the font page (tpage 0x257 = VRAM x 448, y 256, 4 bit), u = (c & 15) * 16,
  v = c & 0x70, width = the table at 0x800B3640, height 16, CLUT row `clut_row + (c >> 7)` at x 0x1C0 (the game passes
  0x1F0, so codes >= 0x80 use row 0x1F1); drawn **twice, at x and x + 1** (bold); the cursor advances by the width + 1
  (no extra pixel after 0xCC / 0xCE);
* `0A xx`: button icon, record xx of the 8 byte table 0x800B3738 {CLUT, uv, w, h}, tpage 0x217, vertically centred on
  the 16 pixel line (y - (h - 16) / 2); icon 0x0B sits 5 pixels higher and blinks to the record at 0x800B3798 while
  (frame & 8); the cursor advances by w + 2;
* `0B xx`: portrait icon (record 0x800B37A0 for xx 1..10, else 0x800B37C0) drawn at (x - 4, y - 1); advance w + 1;
* `0D`: new line (y + 0x12, x back to the start); other codes below 0x10 draw nothing; 0 ends the string;
* `TextLineWidth` (used to centre text) adds the raw table widths of every byte up to 0 / 0x0D, without the +1
  spacing and including escape bytes: `tl_line_width` reproduces it.

`check_text.py` builds the DLL, checks `tl_line_width` against the rule for every line of the 91 hint texts
(0 mismatches) and renders a hint and the gadget menu texts from the decoded font page
(`extract/native_text/text_check.png`; icons as grey boxes): spacing, bold and line breaks look as in the game.

## Message box (`tl_msgbox_*`)

From `MessageBoxOpen` / `SetText` / `DrawPrompt` / `DrawWindow` (the page logic checked on the raw MIPS of
0x8008DFFC): states 0 closed, 1 opening, 2 page, 3 page-end prompt, 4 text-end prompt, 5 closing. A page (text up to
0x0F) appears **whole**, there is no typewriter effect. The prompt is button icon 4 at (0xB8, 0xA0), blinking (shown
while (counter & 8) == 0) at a page end and steady at the text end. Buttons 0xEA turn the page / close at the end, Start
(0x800) closes at any time; both play SFX 0. The window slides with a counter starting at 0x80 halved every frame
(x = 16 - 2 * counter while opening, 16 - 0x80 / (counter + 1) while closing); text starts at (0x20, 0x28).
`check_text.py` pages through all 91 hint texts and checks the page counts (91 / 91).

