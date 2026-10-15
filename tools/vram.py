"""VRAM reconstruction helpers: the VRAM contents after an area's TIM lists are loaded (see dump_tims.py).

    vram = area_vram(area)                      # bytearray 1024*512*2 (halfwords, little endian)
    w, h, rgba = texture(vram, tpage, clut)     # the 256x256 texture page seen through a CLUT
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dump_areas as da  # noqa: E402
import dump_tims as dt  # noqa: E402


LIST_TABLE = 0x800A7120  # kind-0 load lists: {u16 member, s16 postprocess, u32 dest}, terminator postprocess < 0


def global_lists() -> list[list[tuple[int, int, int]]]:
    """The kind-0 load lists (boot / player data / gadget textures): [(member, postprocess, dest), ...] each."""
    out, cur, a = [], [], LIST_TABLE
    while a < 0x800A7200:
        member, post = struct.unpack_from("<Hh", da.EXE, a - 0x80010000 + 0x800)
        if post < 0:
            out.append(cur)
            cur = []
        else:
            cur.append((member, post, da.exe_u32(a + 4)))
        a += 8
    return out


def global_vram() -> bytearray:
    """VRAM after the global lists 0-2 (system, player and gadget textures)."""
    vram = bytearray(1024 * 512 * 2)
    for lst in global_lists()[:3]:
        for member, post, _ in lst:
            if post == 1:
                for t in dt.parse_tims((da.MEMBERS / f"{member:03X}.bin").read_bytes()):
                    dt.upload(vram, t)
    return vram


def area_vram(area: int) -> bytearray:
    vram = global_vram()
    rec = da.exe_u32(da.AREA_PTRS + area * 8)
    while True:
        member, post = struct.unpack_from("<Hh", da.EXE, rec - 0x80010000 + 0x800)
        if post < 0:
            break
        if post == 1:
            for t in dt.parse_tims((da.MEMBERS / f"{member:03X}.bin").read_bytes()):
                dt.upload(vram, t)
        rec += 8
    return vram


def texture(vram: bytes, tpage: int, clut: int):
    """256x256 RGBA of a texture page: tpage bits 0-3 x/64, bit 4 y/256, bits 7-8 mode (0 4bpp, 1 8bpp, 2 15bpp);
    clut bits 0-5 x/16, bits 6-14 y."""
    px, py, mode = (tpage & 15) * 64, (tpage >> 4 & 1) * 256, tpage >> 7 & 3
    cx, cy = (clut & 63) * 16, clut >> 6 & 511
    half = lambda x, y: struct.unpack_from("<H", vram, ((y * 1024) + x) * 2)[0]
    out = bytearray()
    for y in range(256):
        for x in range(256):
            if mode == 2:
                v = half(px + x, py + y)
            elif mode == 1:
                word = half(px + x // 2, py + y)
                v = half(cx + (word >> (8 * (x & 1)) & 255), cy)
            else:
                word = half(px + x // 4, py + y)
                v = half(cx + (word >> (4 * (x & 3)) & 15), cy)
            out += bytes(dt.c16(v))
    return 256, 256, bytes(out)


class Materials:
    """Texture pages seen through CLUTs, written as PNGs and referenced from one MTL."""

    def __init__(self, folder: Path, vram: bytes, rel: str = ""):
        self.folder, self.vram, self.rel, self.names = folder, vram, rel, {}

    def get(self, tpage: int, clut: int) -> str:
        key = (tpage & 0x19F, clut)  # tpage: x/y base + colour mode only (ignore blend bits)
        if key not in self.names:
            name = f"tp{key[0]:03X}_c{key[1]:04X}"
            f = self.folder / f"tex_{name}.png"
            if not f.exists():  # textures of one VRAM image are identical across models: render once
                w, h, rgba = texture(self.vram, key[0], key[1])
                dt.png(f, w, h, rgba)
            self.names[key] = name
        return self.names[key]

    def mtl(self) -> str:
        out = []
        for name in self.names.values():
            out.append(f"newmtl {name}\nKd 1 1 1\nmap_Kd tex_{name}.png\nmap_d tex_{name}.png\n")
        return "\n".join(out)
