"""Disjoint opaque / semi-transparent RGBA layers of one texture page seen through a CLUT.

Pure function over a VRAM image; it reads nothing and writes nothing. The page and CLUT addressing, and the 5-bit
to 8-bit expansion, match tools/vram.texture so the opaque layer agrees with the existing PNG colours. Unlike that
function, the texel's bit 15 (the STP bit of the actual CLUT entry, or of the raw 15bpp texel) is preserved:

    texel 0x0000            -> transparent (alpha 0) in both layers
    texel bit15 = 0, != 0   -> opaque layer only, alpha 255
    texel bit15 = 1         -> semi layer only, alpha 255 (0x8000 is a valid black)

Rows are returned bottom-left first, matching pack_area.png_rgba and the flipped OBJ UVs.
"""
import struct

VRAM_HALFWORDS = 1024 * 512
SIZE = 256
_CLEAR = bytes(4)


def split(vram, tpage, clut):
    """Return (opaque_rgba, semi_rgba), each 256*256*4 bytes, for tpage bits 0-4 / 7-8 and clut bits 0-14."""
    if len(vram) != VRAM_HALFWORDS * 2:
        raise ValueError(f'VRAM image must be {VRAM_HALFWORDS * 2} bytes, got {len(vram)}')
    mode = tpage >> 7 & 3
    if mode == 3:
        raise ValueError(f'tpage 0x{tpage:04X}: colour mode 3 is not a valid texture page mode')
    halves = struct.unpack(f'<{VRAM_HALFWORDS}H', vram)
    px, py = (tpage & 15) * 64, (tpage >> 4 & 1) * 256
    cx, cy = (clut & 63) * 16, clut >> 6 & 511

    def half(x, y):
        # Same linear indexing as vram.texture: an x past 1023 continues on the next VRAM row.
        index = y * 1024 + x
        if not 0 <= index < VRAM_HALFWORDS:
            raise ValueError(f'tpage 0x{tpage:04X} clut 0x{clut:04X}: VRAM address outside the image')
        return halves[index]

    cache = {}

    def texel(v):
        pair = cache.get(v)
        if pair is None:
            if v == 0:
                pair = (_CLEAR, _CLEAR)
            else:
                color = bytes(((v & 31) << 3, (v >> 5 & 31) << 3, (v >> 10 & 31) << 3, 255))
                pair = (_CLEAR, color) if v & 0x8000 else (color, _CLEAR)
            cache[v] = pair
        return pair

    opaque, semi = [], []
    for y in range(SIZE):
        row_o, row_s = [], []
        for x in range(SIZE):
            if mode == 2:
                v = half(px + x, py + y)
            elif mode == 1:
                word = half(px + x // 2, py + y)
                v = half(cx + (word >> (8 * (x & 1)) & 255), cy)
            else:
                word = half(px + x // 4, py + y)
                v = half(cx + (word >> (4 * (x & 3)) & 15), cy)
            o, s = texel(v)
            row_o.append(o)
            row_s.append(s)
        opaque.append(b''.join(row_o))
        semi.append(b''.join(row_s))
    return b''.join(reversed(opaque)), b''.join(reversed(semi))
