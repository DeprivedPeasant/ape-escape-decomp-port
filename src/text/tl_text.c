/* Text layout of the game's font; see tl_text.h. Mirrors FontDrawGlyph (0x8008DB..), TextNextGlyph and TextLineWidth. */
#include "tl_text.h"

#include <string.h>

#define EXE_BASE 0x80010000u
#define EXE_HDR 0x800u

static const uint8_t *exe_ptr(const uint8_t *exe, size_t size, uint32_t addr, size_t n) {
    size_t o = addr - EXE_BASE + EXE_HDR;
    return o + n <= size ? exe + o : NULL;
}

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static void rd_record(const uint8_t *p, uint16_t r[4]) {
    for (int i = 0; i < 4; i++) r[i] = rd16(p + 2 * i);
}

int tl_font_init(TlFont *f, const uint8_t *exe, size_t exe_size) {
    const uint8_t *w = exe_ptr(exe, exe_size, 0x800B3640, 256), *b = exe_ptr(exe, exe_size, 0x800B3738, 0xA8);
    if (!w || !b) return -1;
    memcpy(f->width, w, 256);
    for (int i = 0; i < 12; i++) rd_record(b + 8 * i, f->button[i]);
    for (int i = 12; i < 16; i++) memset(f->button[i], 0, sizeof f->button[i]);
    rd_record(b + 0x60, f->blink);    /* 0x800B3798 */
    rd_record(b + 0x68, f->face[0]);  /* 0x800B37A0 */
    rd_record(b + 0x88, f->face[1]);  /* 0x800B37C0 */
    return 0;
}

int tl_line_width(const TlFont *f, const uint8_t *s) {
    int w = 0;
    while (*s && *s != 0x0D) w += f->width[*s++];
    return w;
}

int tl_layout(const TlFont *f, const uint8_t *s, int x, int y, int clut_row, uint32_t frame, TlQuad *out, int max,
              int *end_x, int *end_y) {
    int n = 0, x0 = x;
    for (;;) {
        unsigned code = *s++;
        if (code == 0x0A) code = 0x100 | *s++;      /* button icon */
        else if (code == 0x0B) code = 0x200 | *s++; /* portrait icon */
        if (code == 0) break;
        if (code == 0x0D) {
            y += 0x12;
            x = x0;
            continue;
        }
        if (!(code & 0xFF00) && code <= 0x0F) continue; /* other control codes draw nothing */
        TlQuad q;
        memset(&q, 0, sizeof q);
        if (!(code & 0x300)) {
            q.kind = TL_GLYPH;
            q.code = (int)code;
            q.x = x;
            q.y = y;
            q.u = (code & 0x0F) << 4;
            q.v = code & 0x70;
            q.w = f->width[code];
            q.h = 16;
            q.tpage = 0x257;
            q.clut = (clut_row + (int)(code >> 7)) * 0x40 | 0x1C;
            q.bold = 1;
            x += f->width[code];
            if (code != 0xCC && code != 0xCE) x += 1;
        } else if (code & 0x200) {
            unsigned i = code & 0xFF;
            const uint16_t *r = (i - 1 < 10) ? f->face[0] : f->face[1];
            q.kind = TL_FACE;
            q.code = (int)i;
            q.x = (int16_t)(x - 4);
            q.y = (int16_t)(y - 1);
            q.clut = r[0];
            q.u = r[1] & 0xFF;
            q.v = r[1] >> 8;
            q.w = r[2];
            q.h = r[3];
            q.tpage = 0x217;
            x += r[2] + 1;
        } else {
            unsigned i = code & 0xFF;
            const uint16_t *r = f->button[i & 15];
            int yy = y;
            if (i == 0x0B) {
                yy = y - 5;
                if (frame & 8) r = f->blink;
            }
            q.kind = TL_BUTTON;
            q.code = (int)i;
            q.x = x;
            q.y = (int16_t)(yy - (((uint16_t)(r[3] - 0x10)) >> 1));
            q.clut = r[0];
            q.u = r[1] & 0xFF;
            q.v = r[1] >> 8;
            q.w = r[2];
            q.h = r[3];
            q.tpage = 0x217;
            x += r[2] + 2;
        }
        if (n < max) out[n] = q;
        n++;
    }
    if (end_x) *end_x = x;
    if (end_y) *end_y = y;
    return n < max ? n : max;
}

/* ------------------------------------------------------------------ message box */
void tl_msgbox_open(TlMsgBox *m) {
    memset(m, 0, sizeof *m);
    m->state = 1;
    m->anim = 0x80;
}

void tl_msgbox_set_text(TlMsgBox *m, const uint8_t *text) {
    m->text = m->page = m->page_end = text;
    m->has_text = 1;
}

static const uint8_t *next_glyph(const uint8_t *p, unsigned *code) {
    unsigned c = *p++;
    if (c == 0x0A || c == 0x0B) c = (c == 0x0A ? 0x100 : 0x200) | *p++;
    *code = c;
    return p;
}

int tl_msgbox_update(TlMsgBox *m, unsigned pressed) {
    int sfx = 0;
    int any = (pressed & 0xEA) != 0;
    /* MessageBoxDrawPrompt */
    if (m->state != 0 && m->state != 1 && m->state != 5) {
        if (m->state == 4 && any) {
            m->state = 5;
            m->anim = 0x80;
            sfx = 1;
        } else if (pressed & 0x800) {
            m->state = 5;
            m->anim = 0x80;
            sfx = 1;
        } else if (m->state == 3) {
            m->blink++;
            if (any) {
                m->state = 2;
                m->page_end++;
                m->page = m->page_end;
                sfx = 1;
            }
        } else if (m->state == 2) {
            unsigned c;
            const uint8_t *p = m->page_end;
            do { /* the whole page at once */
                p = next_glyph(p, &c);
            } while (c != 0 && c != 0x0F);
            m->page_end = p - 1; /* on the 0 / 0x0F */
            m->state = c == 0 ? 4 : 3;
            m->blink = 0;
        }
    }
    /* MessageBoxDrawWindow */
    m->anim >>= 1;
    if (m->state == 1 && m->anim < 2 && m->has_text) m->state = 2;
    else if (m->state == 5 && m->anim < 2) m->state = 0;
    return sfx;
}

int tl_msgbox_window_x(const TlMsgBox *m) {
    if (m->state == 1) return 0x10 - 2 * m->anim;
    if (m->state == 5) return 0x10 - 0x80 / (m->anim + 1);
    return 0x10;
}

int tl_msgbox_layout(const TlFont *f, const TlMsgBox *m, uint32_t frame, TlQuad *out, int max) {
    if (m->state < 2 || m->state == 5 || !m->page) return 0;
    uint8_t buf[0x800];
    size_t n = (size_t)(m->page_end - m->page);
    if (n >= sizeof buf) n = sizeof buf - 1;
    memcpy(buf, m->page, n);
    buf[n] = 0;
    int k = tl_layout(f, buf, 0x20, 0x28, 0x1F0, frame, out, max, NULL, NULL);
    if ((m->state == 4 || (m->state == 3 && !(m->blink & 8))) && k < max) {
        static const uint8_t prompt[] = {0x0A, 0x04, 0};
        k += tl_layout(f, prompt, 0xB8, 0xA0, 0x1F0, frame, out + k, max - k, NULL, NULL);
    }
    return k;
}
