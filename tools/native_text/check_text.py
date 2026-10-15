"""Build src/text/tl_text.c, lay out game texts and render them to a PNG for a visual check.

Glyphs are copied from extract/font_clut496.png / font_clut497.png (the font page 0x257 decoded with CLUT rows
0x1F0 / 0x1F1); icons are drawn as grey boxes. Also checks tl_line_width against the same rule in Python
(TextLineWidth) for every hint text line.

Usage: uv run python tools/native_text/check_text.py [hint sector]   -> extract/native_text/text_check.png
"""
import ctypes
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
OUT = ROOT / "extract/native_text"
sys.path.insert(0, str(HERE.parent))
import render_seq as rs  # noqa: E402


class TlQuad(ctypes.Structure):
    _fields_ = [(n, ctypes.c_int) for n in ("kind", "code", "x", "y", "u", "v", "w", "h", "tpage", "clut", "bold")]


class TlFont(ctypes.Structure):
    _fields_ = [("width", ctypes.c_uint8 * 256), ("button", (ctypes.c_uint16 * 4) * 16), ("blink", ctypes.c_uint16 * 4),
                ("face", (ctypes.c_uint16 * 4) * 2)]


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    dll = OUT / "tl_text.dll"
    subprocess.run([shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-shared", str(ROOT / "src/text/tl_text.c"),
                    "-o", str(dll), "-static-libgcc"], check=True)
    lib = ctypes.CDLL(str(dll))
    lib.tl_font_init.argtypes = [ctypes.POINTER(TlFont), ctypes.c_char_p, ctypes.c_size_t]
    lib.tl_layout.argtypes = [ctypes.POINTER(TlFont), ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_uint32,
                              ctypes.POINTER(TlQuad), ctypes.c_int, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
    lib.tl_line_width.argtypes = [ctypes.POINTER(TlFont), ctypes.c_char_p]
    return lib


def main() -> None:
    from PIL import Image
    lib = build()
    exe = rs.exe_bytes()
    font = TlFont()
    assert lib.tl_font_init(ctypes.byref(font), exe, len(exe)) == 0
    pages = {0: Image.open(ROOT / "extract/font_clut496.png").convert("RGBA"), 1: Image.open(ROOT / "extract/font_clut497.png").convert("RGBA")}
    hints = (ROOT / "extract/members/014.bin").read_bytes()
    sector = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    texts = [hints[sector * 0x800:(sector + 1) * 0x800].split(b"\x0f")[0]]
    # the gadget menu descriptions (notes/28): pointer table 0x800B25A0, 8 gadgets x 5 lines
    import struct
    for g in range(8):
        for k in range(3):
            p = struct.unpack_from("<I", exe, 0xA2DA0 + (g * 5 + k) * 4)[0] - 0x80010000 + 0x800
            texts.append(exe[p:exe.index(0, p)])
    # width check: tl_line_width against the Python rule for every line of every hint text
    bad = 0
    for s in range(len(hints) // 0x800):
        sec = hints[s * 0x800:(s + 1) * 0x800]
        sec = sec[:sec.index(0)] if 0 in sec else sec
        for line in sec.split(b"\x0d"):
            ref = sum(font.width[b] for b in line)
            if lib.tl_line_width(ctypes.byref(font), line + b"\0") != ref:
                bad += 1
    img = Image.new("RGBA", (420, 40 + 20 * 40), (24, 24, 40, 255))
    y = 8
    quads = (TlQuad * 512)()
    total = 0
    for t in texts:
        ex, ey = ctypes.c_int(), ctypes.c_int()
        n = lib.tl_layout(ctypes.byref(font), t + b"\0", 8, y, 0x1F0, 0, quads, 512, ctypes.byref(ex), ctypes.byref(ey))
        for q in quads[:n]:
            if q.kind == 0:
                src = pages[q.code >> 7].crop((q.u, q.v, q.u + q.w, q.v + q.h))
                for dx in (0, 1):
                    img.alpha_composite(src, (q.x + dx, q.y))
            else:
                box = Image.new("RGBA", (max(q.w, 1), max(q.h, 1)), (150, 150, 150, 255))
                img.alpha_composite(box, (max(q.x, 0), max(q.y, 0)))
        total += n
        y = ey.value + 22
    img = img.crop((0, 0, 420, y))
    # message box: every hint text paged through with a button press 30 frames after each prompt
    class TlMsgBox(ctypes.Structure):
        _fields_ = [("state", ctypes.c_int), ("text", ctypes.c_void_p), ("page", ctypes.c_void_p), ("page_end", ctypes.c_void_p),
                    ("anim", ctypes.c_int), ("blink", ctypes.c_int), ("has_text", ctypes.c_int)]
    lib.tl_msgbox_open.argtypes = [ctypes.POINTER(TlMsgBox)]
    lib.tl_msgbox_set_text.argtypes = [ctypes.POINTER(TlMsgBox), ctypes.c_char_p]
    lib.tl_msgbox_update.argtypes = [ctypes.POINTER(TlMsgBox), ctypes.c_uint]
    pages_ok = 0
    for s in range(len(hints) // 0x800):
        sec = hints[s * 0x800:(s + 1) * 0x800]
        if not any(sec):
            continue
        buf = ctypes.create_string_buffer(sec)
        m = TlMsgBox()
        lib.tl_msgbox_open(ctypes.byref(m))
        lib.tl_msgbox_set_text(ctypes.byref(m), buf)
        shown, wait = 0, 0
        for _frame in range(5000):
            press = 0
            if m.state in (3, 4):
                wait += 1
                if wait == 30:
                    press, wait = 0x40, 0
                    shown += 1
            lib.tl_msgbox_update(ctypes.byref(m), press)
            if m.state == 0:
                break
        expected = sec[:sec.index(0)].count(bytes([0x0F])) + 1
        pages_ok += m.state == 0 and shown == expected
    print(f"message box: {pages_ok} hint texts paged through and closed with the expected page count")
    img.save(OUT / "text_check.png")
    print(f"{total} quads, {len(texts)} texts -> {OUT / 'text_check.png'}; line width mismatches: {bad}")


if __name__ == "__main__":
    main()
