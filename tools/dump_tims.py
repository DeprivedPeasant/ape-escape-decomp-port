"""Parse the TIM lists loaded by LoadTimList (load-list postprocess 1) and export them as PNG.

Format (LoadTimList 0x80068D10, notes/11-model-engine.md): a member is a concatenation of TIMs, each starting
with a type word: 0x10 = plain TIM, 0x10010 / 0x20010 = TIM whose image block is RLE-compressed 4-bit
(TimRleDecode1 / TimRleDecode2). After the type word the layout is the normal TIM:
    u32 flags (low nibble pmode: 2 = 16bpp, 8 = 4bpp+CLUT, 9 = 8bpp+CLUT)
    [pmode 8/9] CLUT block: u32 bnum, u16 x, y, w, h, u16 data[w*h]
    image block:           u32 bnum, u16 x, y, w, h, data (w halfwords wide)
The list ends at the first word that is not a TIM type. The decoders write the first nibble of each pair into the high half of a byte (the decoded buffer is uploaded
as is; the GPU reads the low nibble first).

Outputs: extract/tim/<member>_<n>.png (first CLUT row applied), extract/tim/vram_area_XX.png (VRAM after the area's
list loaded), notes/20-textures.md (generated rect table).

Usage: uv run python tools/dump_tims.py
"""
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dump_areas as da  # noqa: E402

ROOT = da.ROOT
OUT = ROOT / "extract/tim"


def rle1(data: bytes, nbytes: int) -> bytes:
    """TimRleDecode1: byte = (value << 4 | count-1) -> value repeated count times; nibbles stored high first."""
    nibs: list[int] = []
    i = 0
    while len(nibs) < nbytes * 2 and i < len(data):
        b = data[i]
        nibs += [b >> 4] * ((b & 15) + 1)
        i += 1
    return pack_nibbles(nibs[:nbytes * 2])


def rle2(data: bytes, nbytes: int) -> bytes:
    """TimRleDecode2: equal nibble pair (v,v) + n -> v repeated n+2 times; otherwise two literal nibbles."""
    nibs: list[int] = []
    i = 0
    while len(nibs) < nbytes * 2 and i < len(data):
        b = data[i]
        hi, lo = b >> 4, b & 15
        if hi == lo:
            nibs += [hi] * (data[i + 1] + 2)
            i += 2
        else:
            nibs += [hi, lo]
            i += 1
    return pack_nibbles(nibs[:nbytes * 2])


def pack_nibbles(nibs: list[int]) -> bytes:
    """Exactly what TimRleDecode1/2 write: the first nibble of each pair goes into the HIGH half of the byte
    (the GPU then reads the low half first, so the decoded stream is pair-swapped on screen)."""
    if len(nibs) % 2:
        nibs.append(0)
    return bytes((nibs[i] << 4) | nibs[i + 1] for i in range(0, len(nibs), 2))


def parse_tims(b: bytes):
    """Yield dicts {kind, pmode, clut:(x,y,w,h,data)|None, img:(x,y,w,h,data)}; stops at a non-TIM word."""
    p = 0
    while p + 8 <= len(b):
        kind = struct.unpack_from("<I", b, p)[0]
        if kind not in (0x10, 0x10010, 0x20010):
            break
        pmode = struct.unpack_from("<I", b, p + 4)[0] & 15
        q, clut = p + 8, None
        if pmode in (8, 9):
            bn = struct.unpack_from("<I", b, q)[0]
            x, y, w, h = struct.unpack_from("<4H", b, q + 4)
            clut = (x, y, w, h, b[q + 12:q + 12 + w * h * 2])
            q += bn & ~3
        bn = struct.unpack_from("<I", b, q)[0]
        x, y, w, h = struct.unpack_from("<4H", b, q + 4)
        raw = b[q + 12:q + bn] if bn else b[q + 12:]
        nbytes = w * h * 2
        if kind == 0x10010:
            raw = rle1(raw, nbytes)
        elif kind == 0x20010:
            raw = rle2(raw, nbytes)
        yield {"kind": kind, "pmode": pmode, "clut": clut, "img": (x, y, w, h, raw[:nbytes])}
        p = q + (bn & ~3)


def png(path: Path, w: int, h: int, rgba: bytes) -> None:
    raw = b"".join(b"\x00" + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def c16(v: int) -> tuple[int, int, int, int]:
    r, g, b = (v & 31) << 3, (v >> 5 & 31) << 3, (v >> 10 & 31) << 3
    return r, g, b, 0 if v == 0 else 255


def render(t) -> tuple[int, int, bytes]:
    x, y, w, h, data = t["img"]
    pm = t["pmode"]
    if pm == 2:
        px = struct.unpack_from(f"<{w * h}H", data)
        return w, h, b"".join(bytes(c16(v)) for v in px)
    pal = struct.unpack_from(f"<{len(t['clut'][4]) // 2}H", t["clut"][4]) if t["clut"] else [0] * 256
    pal = [c16(v) for v in pal]
    if pm == 8:
        wp = w * 4
        idx = [n for byte in data for n in (byte & 15, byte >> 4)]
    else:
        wp = w * 2
        idx = list(data)
    out = bytearray()
    for v in idx[:wp * h]:
        out += bytes(pal[v] if v < len(pal) else (255, 0, 255, 255))
    return wp, h, bytes(out)


def upload(vram: bytearray, t) -> None:
    """Copy a parsed TIM's CLUT and image into a VRAM buffer."""
    x, y, w, h, data = t["img"]
    if t["clut"]:
        cx, cy, cw, ch, cdata = t["clut"]
        for r in range(ch):
            put(vram, cx, cy + r, cdata[r * cw * 2:(r + 1) * cw * 2])
    for r in range(h):
        put(vram, x, y + r, data[r * w * 2:(r + 1) * w * 2])


def put(vram: bytearray, x: int, y: int, row: bytes) -> None:
    """Write one row into VRAM, clipped to the 1024x512 area (never resizes the buffer)."""
    if not (0 <= y < 512 and 0 <= x < 1024):
        return
    row = row[:(1024 - x) * 2]
    o = (y * 1024 + x) * 2
    vram[o:o + len(row)] = row


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    rows = ["# Textures (TIM lists, generated)", "",
            "Generated by `tools/dump_tims.py`. Rects are VRAM halfword coordinates (x y w h); an image's pixel width "
            "is w*4 (4bpp) / w*2 (8bpp). Compressed = RLE variants 0x10010 / 0x20010.", "",
            "Each area's load list uploads its TIM members into VRAM (postprocess 1); `tools/vram.py` rebuilds that "
            "VRAM and renders any (TPAGE, CLUT) pair as a 256x256 texture (TPAGE bits 0-3 x/64, bit 4 y/256, bits "
            "7-8 mode; CLUT bits 0-5 x/16, bits 6-14 y). Terrain: TPAGE 0x18 + CLUT row 0xF0+tile (notes/12). "
            "Models / meshes carry their own tpage and clut per primitive (notes/11). Actor-model textures come "
            "from other load lists (not covered here yet).", "",
            "| Member | # | Kind | pmode | CLUT rect | Image rect | Pixels |", "|---|---|---|---|---|---|---|"]
    members = da.area_members  # noqa: F841 (area blobs are post 0; TIM members come from the area lists)
    seen: set[int] = set()
    vram_by_area = {}
    for area, ptr in da.area_chains():
        rec, vram = ptr, bytearray(1024 * 512 * 2)
        while True:
            member, post = struct.unpack_from("<Hh", da.EXE, rec - 0x80010000 + 0x800)
            if post < 0:
                break
            if post == 1:
                b = (da.MEMBERS / f"{member:03X}.bin").read_bytes()
                for n, t in enumerate(parse_tims(b)):
                    x, y, w, h, data = t["img"]
                    if t["clut"]:
                        cx, cy, cw, ch, cdata = t["clut"]
                        for r in range(ch):
                            put(vram, cx, cy + r, cdata[r * cw * 2:(r + 1) * cw * 2])
                    for r in range(h):
                        put(vram, x, y + r, data[r * w * 2:(r + 1) * w * 2])
                    if member not in seen:
                        wp, hp, rgba = render(t)
                        png(OUT / f"{member:03X}_{n}.png", wp, hp, rgba)
                        c = t["clut"]
                        rows.append(f"| 0x{member:03X} | {n} | 0x{t['kind']:X} | {t['pmode']} | "
                                    f"{(c[0], c[1], c[2], c[3]) if c else ''} | {(x, y, w, h)} | {wp}x{hp} |")
                if member not in seen:
                    seen.add(member)
            rec += 8
        vram_by_area[area] = vram
        px = struct.unpack(f"<{1024 * 512}H", bytes(vram))
        png(OUT / f"vram_area_{area:02X}.png", 1024, 512, b"".join(bytes(c16(v)) for v in px))
    (ROOT / "notes/20-textures.md").write_text("\n".join(rows) + "\n", encoding="utf-8")
    print(f"{len(seen)} TIM members, {len(vram_by_area)} areas -> {OUT}")


if __name__ == "__main__":
    main()
