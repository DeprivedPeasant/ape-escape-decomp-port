"""Differentially check the hand-ported HUD and message-box functions.

The oracle is SCUS_944.23 running on the byte-exact MIPS RAM image. Area
members are read from the shipped DAT/BNS archive in memory, so the checker
does not create extracted game-data files.

Usage: .venv/Scripts/python.exe tools/mips/check_hud.py [--coverage] [--cases PERCENT] [--seed N] [--mutate]
"""
from __future__ import annotations

import argparse
import ctypes as C
import json
import random
import re
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/mips"), str(ROOT / "tools")]
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

M32 = 0xFFFFFFFF
RAM_SIZE = mr.RAM_SIZE
SCRATCH_SIZE = mr.SCRATCH_SIZE
STACK_LO = 0x801FF000
GPU = 0x800EC350
PRIMITIVE = GPU + 4
AREA_HEADER_PTR = 0x800F44E4
AREA_BASE = 0x80154000
MESSAGE = 0x800F5A00
MESSAGE_DATA = 0x801D2000
PACKET_BUFFER = 0x801D7000
MUTATION_A = 0x80003000
MUTATION_B = 0x80003001
EXTRA_COVERAGE = {}

# address, size, C dispatcher index, return kind, original argument kinds
FUNCS = {
    "RadarUsable": (0x800468EC, 132, 0, "s", ""),
    "HudDrawIconSprite": (0x80046ACC, 248, 1, "v", "ssss"),
    "HudDrawGadgetIcons": (0x80046BC4, 444, 2, "v", ""),
    "GetDAT_800B01C2": (0x8005DD34, 16, 3, "s", ""),
    "StageDrawScreenOverlay": (0x8005E0F0, 828, 4, "v", "uu"),
    "HudDrawHealth": (0x8005E4A8, 332, 5, "v", ""),
    "AreaTexCopyDraw": (0x8005EAB0, 492, 6, "v", "s"),
    "HudPanelA_DrawIcon": (0x8005F2E0, 796, 7, "v", "ss"),
    "HudPanelB_DrawSprite": (0x8005F5FC, 144, 8, "v", "ss"),
    "HudDrawDigit": (0x8005F770, 268, 9, "v", "sss"),
    "HudDrawNumber": (0x8005F87C, 252, 10, "v", "sss"),
    "HudDrawTime": (0x8005F978, 812, 11, "v", "sss"),
    "HudDrawIcon16": (0x8005FCA4, 220, 12, "v", "sss"),
    "HudCounterC_Draw": (0x8005FD80, 596, 13, "s", ""),
    "HudDrawTimerGauge": (0x8005FFD4, 592, 14, "v", "s"),
    "HudDrawGadgetSlotsFrame": (0x80081180, 600, 15, "v", ""),
    "HudDrawGadgetSlotIcons": (0x800813D8, 652, 16, "v", ""),
    "HudDrawGadgetSlots": (0x80081664, 792, 17, "v", ""),
    "HudDrawGadgetIcon": (0x800819E8, 436, 18, "u", "sssu"),
    "FontDrawGlyph": (0x8008D9D0, 920, 19, "v", "ssu"),
    "TextNextGlyph": (0x8008DD68, 72, 20, "u", "uu"),
    "TextSetCursor": (0x8008DDB0, 24, 21, "v", "ss"),
    "MessageBoxUpdateDraw": (0x8008DF4C, 52, 22, "v", ""),
    "MessageBoxDrawText": (0x8008DF80, 124, 23, "v", ""),
    "MessageBoxDrawPrompt": (0x8008DFFC, 576, 24, "v", ""),
    "MessageBoxDrawWindow": (0x8008E23C, 888, 25, "v", ""),
    "HudPanelA_Update": (0x8009529C, 416, 26, "s", ""),
    "HudPanelB_Update": (0x80095690, 560, 27, "s", ""),
    "HudCounterC_Update": (0x80095924, 60, 28, "s", ""),
}

MIPSHOOKS = {
    "DR_MOVE": 0x8002B380,
    "ClearMemory": 0x800A3740,
    "ModelDrawScreenSpace": 0x800207B4,
    "PlaySfx": 0x800877F8,
    "Ovl_8013D424": 0x8013D424,
}
MIPS_HOOK_NAMES = {address: name for name, address in MIPSHOOKS.items()}

MASKS = [
    (0x801D7000 - 0x80000000, 0x4000),
    (0x800EC3D0 - 0x80000000, 0x4000),
    (0x800F03D0 - 0x80000000, 0x4000),
    (0x80003FFC - 0x80000000, 4),
]

DRAW_FIELDS = [
    ("kind", C.c_int32), ("primitive_address", C.c_uint32),
    ("ordering_entry", C.c_uint32), ("depth", C.c_uint32),
    ("packet_bytes", C.c_uint32), ("asset", C.c_uint32),
    ("flags", C.c_uint32), ("x", C.c_int32), ("y", C.c_int32),
    ("width", C.c_int32), ("height", C.c_int32),
    ("x2", C.c_int32), ("y2", C.c_int32),
    ("x3", C.c_int32), ("y3", C.c_int32),
    ("x4", C.c_int32), ("y4", C.c_int32),
    ("red", C.c_int32), ("green", C.c_int32), ("blue", C.c_int32),
    ("glyph", C.c_int32), ("u", C.c_uint16), ("v", C.c_uint16),
    ("clut", C.c_uint16), ("tpage", C.c_uint16),
]


class HudDraw(C.Structure):
    _fields_ = DRAW_FIELDS


DrawFn = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.POINTER(HudDraw))
OverlayFn = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_int32)
DrMoveFn = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p,
                        C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
ClearFn = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p,
                       C.c_uint32, C.c_uint32, C.c_uint32)
ModelFn = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                      C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)


class HudHooks(C.Structure):
    _fields_ = [
        ("user", C.c_void_p), ("stack_pointer", C.c_uint32),
        ("draw_sprite", DrawFn), ("draw_quad", DrawFn),
        ("draw_glyph", DrawFn), ("draw_stage_overlay", DrawFn),
        ("draw_gauge", DrawFn), ("draw_message_frame", DrawFn),
        ("Ovl_8013D424", OverlayFn), ("DR_MOVE", DrMoveFn),
        ("ClearMemory", ClearFn), ("ModelDrawScreenSpace", ModelFn),
        ("sound_hw", C.c_void_p),
    ]


def build_dll(source: Path | None = None, label: str = "hud") -> Path:
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{label}_{time.time_ns()}.dll"
    sources = [
        ROOT / "src/core/ae_ram.c", ROOT / "src/core/ae_sdk.c",
        source or ROOT / "src/ui/ae_hud.c", ROOT / "tools/mips/hud_test_api.c",
    ]
    subprocess.run([
        shutil.which("gcc") or "gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-fvisibility=hidden", "-ffunction-sections", "-fdata-sections",
        f"-I{ROOT / 'src/ui'}", f"-I{ROOT / 'src/core'}", f"-I{ROOT / 'src/player'}",
        f"-I{ROOT / 'src/sound'}", "-shared", "-static-libgcc", *map(str, sources),
        "-Wl,--gc-sections", "-lm", "-o", str(dll),
    ], check=True)
    return dll


class CSide:
    def __init__(self, dll_path: Path):
        self.lib = C.CDLL(str(dll_path))
        P, U, I = C.c_void_p, C.c_uint32, C.c_int32
        self.lib.ae_hud_test_create.restype = P
        self.lib.ae_hud_test_destroy.argtypes = [P]
        self.lib.ae_hud_test_main.argtypes = [P]
        self.lib.ae_hud_test_main.restype = P
        self.lib.ae_hud_test_scratch.argtypes = [P]
        self.lib.ae_hud_test_scratch.restype = P
        self.lib.ae_hud_test_pointer.argtypes = [P, U, U]
        self.lib.ae_hud_test_pointer.restype = P
        self.lib.ae_hud_test_clear_error.argtypes = [P]
        self.lib.ae_hud_test_failed.argtypes = [P]
        self.lib.ae_hud_test_failed.restype = C.c_int
        self.lib.ae_hud_test_error.argtypes = [P]
        self.lib.ae_hud_test_error.restype = C.c_char_p
        self.lib.ae_hud_test_clear_sfx.argtypes = []
        self.lib.ae_hud_test_sfx_count.argtypes = []
        self.lib.ae_hud_test_sfx_count.restype = U
        self.lib.ae_hud_test_sfx_id.argtypes = [U]
        self.lib.ae_hud_test_sfx_id.restype = I
        self.lib.ae_hud_test_call.argtypes = [P, C.POINTER(HudHooks), U, C.POINTER(U)]
        self.lib.ae_hud_test_call.restype = I

        self.ram = self.lib.ae_hud_test_create()
        if not self.ram:
            raise SystemExit("ae_hud_test_create failed")
        self.main = (C.c_uint8 * RAM_SIZE).from_address(self.lib.ae_hud_test_main(self.ram))
        self.scratch = (C.c_uint8 * SCRATCH_SIZE).from_address(self.lib.ae_hud_test_scratch(self.ram))
        self.draws: list[tuple[int, ...]] = []
        self.external: list[tuple[str, tuple[int, ...]]] = []
        self._keep = []
        draw_cbs = []
        for kind in range(1, 7):
            def append_draw(_user, _ram, draw_ptr, expected=kind):
                draw = draw_ptr.contents
                if draw.kind != expected:
                    self.external.append(("draw_kind_mismatch", (draw.kind, expected)))
                self.draws.append(tuple(int(getattr(draw, name)) for name, _ in DRAW_FIELDS))
            cb = DrawFn(append_draw)
            draw_cbs.append(cb)
            self._keep.append(cb)

        def overlay(_user, _ram, kind):
            self.external.append(("Ovl_8013D424", (int(kind) & M32,)))
            return 0

        def dr_move(_user, _ram, primitive, rect, source_x, source_y):
            self.external.append(("DR_MOVE", tuple(int(v) for v in (primitive, rect, source_x, source_y))))

        def clear_memory(_user, _ram, address, value, size):
            ptr = self.lib.ae_hud_test_pointer(self.ram, address, size)
            if not ptr:
                return
            C.memset(ptr, value & 0xFF, size)
            self.external.append(("ClearMemory", tuple(int(v) for v in (address, value, size))))

        def model_draw(_user, _ram, model, position, rotation, scale):
            self.external.append(("ModelDrawScreenSpace", tuple(int(v) for v in (model, position, rotation, scale))))
            return 0

        self._keep.extend([OverlayFn(overlay), DrMoveFn(dr_move), ClearFn(clear_memory), ModelFn(model_draw)])
        self.hooks = HudHooks(
            None, mr.DEFAULT_SP, *draw_cbs, self._keep[-4], self._keep[-3],
            self._keep[-2], self._keep[-1], None,
        )

    def sync(self, cpu: mr.MipsRam):
        C.memmove(self.main, bytes(cpu.ram), RAM_SIZE)
        C.memmove(self.scratch, bytes(cpu.scratch), SCRATCH_SIZE)
        self.lib.ae_hud_test_clear_error(self.ram)
        self.draws.clear()
        self.external.clear()
        self.lib.ae_hud_test_clear_sfx()

    def call(self, index: int, args: tuple[int, ...]) -> int:
        words = (C.c_uint32 * 6)(*(list(args) + [0] * (6 - len(args))))
        return int(self.lib.ae_hud_test_call(self.ram, C.byref(self.hooks), index, words)) & M32

    def sfx_ids(self) -> list[int]:
        return [int(self.lib.ae_hud_test_sfx_id(i)) & M32
                for i in range(self.lib.ae_hud_test_sfx_count())]


def signed(value: int, bits: int) -> int:
    value &= (1 << bits) - 1
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def area_source_data():
    index_path = ROOT / "extract/members/index.json"
    index = {int(item["index"], 16): item for item in json.loads(index_path.read_text(encoding="utf-8"))}
    stream = (ROOT / "extract/disc/KKIIDDZZ.DAT").read_bytes() + (ROOT / "extract/disc/KKIIDDZZ.BNS").read_bytes()
    result = {}
    for area, member in dump_areas.area_members().items():
        entry = index.get(member)
        if not entry or "sector_offset" not in entry:
            continue
        start = int(entry["sector_offset"]) * 2048
        end = (int(entry["sector_offset"]) + int(entry["sectors"])) * 2048
        blob = stream[start:end]
        if len(blob) >= 0x4D4:
            header = struct.unpack_from("<I", blob, 0x4D0)[0]
            if 0 < header < len(blob) - 0x40:
                result[area] = (member, blob, header)
    if len(result) < 3:
        raise SystemExit("check_hud requires real area blobs for at least three areas")
    return result


AREA_DATA = area_source_data()
AREA_IDS = sorted(AREA_DATA)


def initialize_case(cpu: mr.MipsRam, area_id: int, name: str, case: int, rng: random.Random):
    """Set shared game state while keeping the actual area member and EXE tables."""
    _, area_blob, header_offset = AREA_DATA[area_id]
    cpu.load_area(area_blob)
    cpu.write32(AREA_HEADER_PTR, AREA_BASE + header_offset)
    cpu.write32(PRIMITIVE, PACKET_BUFFER)
    cpu.write32(GPU + 0x0C, 0)
    cpu.write32(GPU, rng.randrange(0x10000))

    # Stage globals, area HUD context, mode and loaded player. The area member is
    # real; these state words select the call-tree paths exercised by this case.
    cpu.write8(0x800F447C, rng.choice([0, 3, 4, 5, 7, 9, 11]))
    cpu.write16(0x800F4470, rng.choice([0, 0x13, 0x13, 0x12]))
    cpu.write16(0x800F4472, rng.choice([0, 3, 7, 14, 30, 31]))
    cpu.write16(0x800F8126, rng.choice([0x57, 0x58, 0x5F, 0x63, 0x6A]))
    cpu.write16(0x800F8152, rng.choice([0, 3, 7, 11, 14]))
    cpu.write16(0x800F4350 + 0x124, rng.choice([0, 3, 7, 14, 17, 22, 30, 31]))
    cpu.write16(0x800F4350 + 0x12E, rng.choice([0, 1, 2]))
    cpu.write32(0x800F4350 + 0x140, rng.choice([0, 1, 59, 60, 3599, 3600, 0xFFFF]))
    cpu.write16(0x800F4350 + 0x150, rng.choice([0, 1, 3, 4, 8, 9, 11]))
    cpu.write16(0x800F4350 + 0x166, rng.choice([0, 1, 3, 10, 32]))
    cpu.write16(0x800F4350 + 0x186, rng.choice([0, 1, 3]))
    cpu.write16(0x800F4350 + 0x188, rng.choice([-1, 0, 1, 2, 5, 10, 200]))
    cpu.write16(0x800F4350 + 0x13C, rng.choice([0, 1, 9, 10, 42, 99, 100, 255]))
    cpu.write16(0x800F4350 + 0x12C, rng.choice([-1, 0, 3, 4, 5, 7, 9, 11]))
    cpu.write16(0x800E78CE, rng.choice([-32768, -31, -1, 0, 1, 15, 16, 120, 2048]))
    cpu.write8(0x800E6908, rng.choice([0, 1, 2]))
    cpu.write16(0x800F44A0, rng.choice([0, 1, 4, 8, 9, 11]))

    player = 0x800EC1E0
    cpu.write16(player + 0x5E, rng.choice([0x45, 0x46, 0x47, 0x49, 0x4A, 0x4C]))
    cpu.write32(player + 0x70, rng.choice([0, 0x20, 0xE0, 0x1234]))
    cpu.write8(player + 0xF2, rng.choice([2, 3, 4, 9, 0xFF]))
    cpu.write16(player + 0xE8, rng.choice([0, 1, 2, 3, 5, 8]))

    hud = 0x800F4DB0
    cpu.write32(hud + 0x20, rng.choice([0, 1, 2, 4, 8, 0x3F]))
    cpu.write16(hud + 0x08, rng.choice([0, 1, 2, 39, 40, 41]))
    cpu.write16(hud + 0x0A, rng.choice([0, 1, 2, 3, 4, 5, 0xFFFF]))
    cpu.write16(hud + 0x0C, rng.choice([0, 1, 2, 39, 40, 41]))
    cpu.write16(hud + 0x0E, rng.choice([0, 1, 2, 3, 4, 5, 0xFFFF]))
    cpu.write16(hud + 0x10, rng.choice([0, 1, 8, 9, 25, 26, 0xFFFF]))
    cpu.write8(hud + 2, rng.choice([0, 1, 2, 3]))
    cpu.write8(hud + 3, rng.choice([0, 1, 2, 3]))
    cpu.write16(hud + 0x168, rng.choice([0, 1, 30, 99, 100, 999]))

    panel = 0x800F5120
    cpu.write32(panel + 0x20, rng.choice([0, 8, 0x108]))
    cpu.write16(panel + 4, rng.choice([0, 8, 16, 40]))
    cpu.write16(panel + 0x80, rng.choice([0, 1, 2, 3, 4]))
    cpu.write16(panel + 0x88, rng.choice([0, 100, 150, 299, 300, 600]))
    cpu.write16(panel + 0x94, rng.choice([0, 80, 120, 200, 300, 400]))
    cpu.write16(panel + 0x96, rng.choice([0, 28, 60, 100, 180]))
    cpu.write16(panel + 0x98, rng.choice([0, 1, 2, 20, 300, 400]))
    cpu.write16(panel + 0x9A, rng.choice([0, 1, 2, 10]))
    cpu.write16(panel + 0x9C, rng.choice([-1, 0, 1, 2, 20]))
    cpu.write8(panel + 0xA0, rng.choice([0, 1, 2, 3, 4, 9, 0xFF]))
    cpu.write8(panel + 0xA1, rng.choice([0, 1, 2, 3, 4, 0xFF]))
    cpu.write8(panel + 0xA2, rng.choice([0, 1, 2, 3, 4, 0xFF]))
    cpu.write8(panel + 0xF2, rng.choice([0, 1, 2, 3, 0xFF]))
    for index in range(4):
        cpu.write8(0x800F51A8 + index, rng.choice([-1, 0, 1, 2, 3, 4, 9]))

    # Keep the sound oracle at the no-loaded-bank boundary; PlaySfx still runs
    # through the same call boundary and its requested IDs remain compared.
    for sound_id in (0, 0x7A):
        cpu.write16(0x800B2D08 + sound_id * 8, 0)
    cpu.write16(0x800BF670, 0xFFFF)

    # Each message scenario uses a real RAM string with escaped glyphs, page
    # markers, a terminal marker, and a NUL. The message pointers are reset per
    # case so malformed input cannot walk outside the fixture.
    message_bytes = bytes([0x10, 0x11, 0x0A, 0x04, 0x0B, 0x02, 0x0D, 0x12, 0x0F,
                           0x13, 0x14, 0, 0, 0, 0, 0])
    cpu.write_bytes(MESSAGE_DATA, message_bytes)
    cpu.write8(MESSAGE, rng.choice([0, 1, 2, 3, 4, 5]))
    cpu.write8(MESSAGE + 1, rng.choice([0, 1, 7, 8, 0xFF]))
    cpu.write16(MESSAGE + 2, rng.choice([0x1F0, 0x1F1, 0x1F2]))
    cpu.write32(MESSAGE + 4, MESSAGE_DATA + rng.choice([0, 2, 6, 8, 10]))
    cpu.write32(MESSAGE + 8, MESSAGE_DATA + rng.choice([0, 2, 6, 8, 10]))
    cpu.write16(MESSAGE + 0x10, rng.choice([0, 1, 2, 3, 0x40, 0x80, 0xFFFF]))
    cpu.write16(MESSAGE + 0x12, rng.choice([0, 1, 2, 13]))
    cpu.write16(MESSAGE + 0x14, rng.choice([0, 0x0D, 0x0F, 0x10]))
    cpu.write16(MESSAGE + 0x16, rng.choice([0, 1]))
    cpu.write32(0x800E5478, rng.choice([0, 0x10, 0x80, 0xEA, 0x800]))
    cpu.write32(0x800E5480, rng.choice([0, 0x10, 0x80, 0xEA, 0x800]))
    cpu.write16(0x800DF810, 0)
    cpu.write16(0x800DF812, 0)
    cpu.write16(0x800DF814, 0)

    # AreaTexCopyDraw's seven copies interpolate between real area texture
    # entries; vary the bounds slightly while keeping the source member real.
    area_header = AREA_BASE + header_offset
    cpu.write16(area_header + 0x30, rng.choice([0, 1, 64, 512]))
    cpu.write16(area_header + 0x32, rng.choice([0, 64, 512, 4096]))
    cpu.write8(area_header + 0x2A, rng.choice([0, 1]))

    if name == "HudPanelA_DrawIcon":
        cpu.write16(0x800F4350 + 0x12E, [0, 1, 2][case % 3])
        cpu.write16(0x800F4350 + 0x188, [0, 1, 2, 5, 10, 200, 0xFFFF][case % 7])
        cpu.write16(0x800F4350 + 0x124, [0x11, 0x16, 0x1E, 0x1F, 0][case % 5])
    if name == "HudDrawGadgetSlotsFrame":
        cpu.write16(hud + 0x18, [0, 1, 14, 15, 149, 150, 299, 300, 600][case % 9])
    if name in ("MessageBoxDrawPrompt", "MessageBoxUpdateDraw"):
        states = [0, 1, 2, 3, 4, 5]
        cpu.write8(MESSAGE, states[case % len(states)])
        cpu.write16(MESSAGE + 0x12, [0, 1, 2, 13][case % 4])
        if case % 3 == 1:
            cpu.write32(0x800E5478, 0xEA)
            cpu.write32(0x800E5480, 0xEA)
    if name == "HudDrawGadgetIcons":
        cpu.write8(player + 0xF2, [2, 3, 4][case % 3])
        cpu.write16(0x800F8126, [0x57, 0x58, 0x5F, 0x63][case % 4])
        cpu.write16(0x800F8152, [0, 3, 11][case % 3])
        scenario = case % 8
        cpu.write32(GPU, 4 if scenario in (0, 1, 2, 5) else 0)
        cpu.write32(hud + 0x20, 8 if scenario in (1, 3, 5, 7) else 0)
        cpu.write16(hud + 4, [0, 96, 128, 48][scenario % 4])
        cpu.write8(0x800E68D0 + 0x46, 1 if scenario in (2, 3) else 0)
        cpu.write8(0x800E68D0 + 0x32, 1 if scenario in (4, 5) else 0)
        cpu.write8(0x800E68D0 + 0x2F, [0, 1, 2, 3][scenario % 4])
        cpu.write8(player + 0xF2, 2 if scenario in (0, 2, 5) else [2, 3, 4][scenario % 3])
        cpu.write16(panel + 0x1C, 1 if scenario == 0 else 0)
        cpu.write32(panel + 0x18, 1 if scenario == 0 else 0)
        if scenario == 0:
            cpu.write16(0x800F8126, 0x70)
            cpu.write16(0x800F8152, 0)
            cpu.write8(0x800F447C, 3)
            cpu.write16(player + 0x5E, 0x45)
            cpu.write32(player + 0x70, 0)

    if name == "RadarUsable":
        cpu.write16(0x800F8126, 0x70)
        cpu.write16(0x800F8152, 0)
        cpu.write8(0x800F447C, 3)
        cpu.write16(player + 0x5E, 0x45)
        cpu.write8(player + 0xF2, 2)
        cpu.write32(player + 0x70, 0)

    if name == "StageDrawScreenOverlay":
        stage_cases = [
            (31, 2, 0, 0, 0), (32, 2, 0, 0, 0), (30, 1, 0x13, 0, 0),
            (0, 2, 0x13, 0, 1), (7, 2, 0, 0, 0), (14, 2, 0, 0, 0),
            (33, 1, 0, 0, 0), (0, 2, 0x12, 0, 0),
        ]
        kind, mode, stage_mode, stage_kind, gauge = stage_cases[case % len(stage_cases)]
        cpu.write16(0x800F4350 + 0x124, kind)
        cpu.write8(0x800E6908, mode)
        cpu.write16(0x800F4470, stage_mode)
        cpu.write16(0x800F8152, stage_kind)
        cpu.write16(0x800B01C2, gauge)
        cpu.write16(0x800E78CE, 2000 if case % 8 == 2 else [-32768, -1, 0, 15, 120, 2048, 2000, -31][case % 8])
        cpu.write32(hud + 0x20, [0x3F, 0x28, 0x1F, 0x3F, 0, 0, 0x3F, 0x3F][case % 8])
        if case % 8 in (4, 5):
            area_header = AREA_BASE + AREA_DATA[area_id][2]
            cpu.write8(area_header + 0x2A, 0)

    if name == "HudDrawGadgetSlotIcons":
        scenario = case % 6
        cpu.write16(panel + 0x98, [410, 400, 300, 310, 250, 401][scenario])
        cpu.write16(panel + 0x9C, [0, 0, 2, 1, 10, -1][scenario])
        cpu.write16(panel + 0x9A, [0, 1, 2, 0, 3, 10][scenario])
        cpu.write8(panel + 0xA0, [3, 2, 3, 4, 3, 2][scenario])

    if name == "HudCounterC_Draw":
        cpu.write16(hud, [0, 14, 15, 80, 81, 90, 91, 150][case % 8])
        cpu.write8(hud + 2, 2 if case % 8 == 0 else 1)
        cpu.write8(hud + 3, [1, 2, 3, 0][case % 4])
        cpu.write16(hud + 0x10, [0, 8, 9, 25, 26, 0xFFFF][case % 6])
        cpu.write16(0x800F4350 + 0x168, [0, 1, 30, 99, 100, 999][case % 6])

    if name == "HudPanelA_Update":
        panel_cases = [(0, 40), (1, 40), (1, 1), (2, 0), (3, 40), (3, 0),
                       (3, 45), (4, 0), (0, 0)]
        state, timer = panel_cases[case % len(panel_cases)]
        cpu.write16(0x800F4350 + 0x124, 0x1F if case % len(panel_cases) == 8 else 0)
        cpu.write16(hud + 0x08, timer)
        cpu.write16(hud + 0x0A, state)
        cpu.write16(0x800F4350 + 0x186, 1)

    if name == "HudPanelB_Update":
        panel_cases = [(0, 40), (1, 40), (1, 1), (2, 0), (3, 0), (3, 50), (4, 0)]
        state, timer = panel_cases[case % len(panel_cases)]
        cpu.write16(hud + 0x0C, timer)
        cpu.write16(hud + 0x0E, state)
        cpu.write16(hud + 0x10, [0, 8, 9, 25, 26, 26, 0xFFFF][case % len(panel_cases)])
        cpu.write16(0x800F4350 + 0x13C, [0, 1, 9, 10, 42, 99, 100][case % len(panel_cases)])

    if name == "HudCounterC_Update":
        cpu.write16(hud, 91 if case % 2 == 0 else 14)
        cpu.write8(hud + 2, 2 if case % 2 == 0 else 0)
        cpu.write8(hud + 3, 1)

    if name == "FontDrawGlyph":
        cpu.write32(GPU, 8 if case % 11 == 7 else 0)

    if name == "MessageBoxDrawPrompt":
        prompt_cases = [(0, 0, 1), (0, 8, 1), (3, 8, 0), (3, 6, 1),
                        (2, 0, 1), (4, 0, 1), (5, 8, 1), (0, 6, 1)]
        state, offset, has_text = prompt_cases[case % len(prompt_cases)]
        cpu.write8(MESSAGE, state)
        cpu.write8(MESSAGE + 1, 0 if case % 2 == 0 else 7)
        cpu.write16(MESSAGE + 0x12, [0, 1, 13, 15][case % 4])
        cpu.write16(MESSAGE + 0x14, [0, 0x0D, 0x0F, 0x10][case % 4])
        cpu.write16(MESSAGE + 0x16, has_text)
        cpu.write32(MESSAGE + 4, MESSAGE_DATA + offset)
        cpu.write32(MESSAGE + 8, MESSAGE_DATA + offset)
        if offset == 0:
            cpu.write8(MESSAGE_DATA, 0)
        cpu.write32(0x800E5478, 0xEA if case % 3 == 1 else 0)
        cpu.write32(0x800E5480, 0xEA if case % 3 == 1 else 0)

    # All sound requests route through a known invalid bank and never touch
    # mixer state. The checker compares the requested sound IDs on both sides.
    return area_blob


def arguments(name: str, case: int, rng: random.Random) -> tuple[int, ...]:
    xy = lambda: (rng.choice([-32768, -240, -1, 0, 31, 120, 239, 32767]),
                  rng.choice([-32768, -20, -1, 0, 32, 96, 200, 32767]))
    if name == "HudDrawIconSprite":
        x, y = xy(); return rng.randrange(16), x, y, case & 1
    if name == "StageDrawScreenOverlay": return rng.getrandbits(32), rng.getrandbits(32)
    if name == "AreaTexCopyDraw": return case % 7,
    if name in ("HudPanelA_DrawIcon", "HudPanelB_DrawSprite"):
        return xy()
    if name in ("HudDrawDigit", "HudDrawNumber", "HudDrawTime", "HudDrawIcon16"):
        x, y = xy()
        value = {
            "HudDrawDigit": [-1, 0, 1, 9, 10, 11, 12, 999][case % 8],
            "HudDrawNumber": [0, 1, 9, 10, 99, 100, 999, 9999, -1][case % 9],
            "HudDrawTime": [-1, 0, 1, 59, 60, 3599, 3600, 0x7FFFFFFF][case % 8],
            "HudDrawIcon16": [-8, -1, 0, 1, 7, 8, 31, 32, 255][case % 9],
        }[name]
        return x, y, value
    if name == "HudDrawTimerGauge": return [-1, 0, 1, 60, 100, 1000, 0x7FFFFFFF][case % 7],
    if name == "HudDrawGadgetIcon":
        x, y = xy(); return x, y, [-1, -2, -5, -9, -20, 0, 1, 2, 10, 12][case % 10], PACKET_BUFFER
    if name == "FontDrawGlyph":
        return [0, 1, 0x0D, 0x10, 0xCC, 0xCE, 0x10A, 0x10B, 0x201, 0x20A, 0x20B][case % 11], 0x1F0, 1
    if name == "TextNextGlyph": return MESSAGE_DATA + case % 10, 0x801FE000
    if name == "TextSetCursor": return xy()
    return ()


def c_renderer_names(draw):
    return draw[0]


def _memory_bytes(cpu: mr.MipsRam, side: CSide):
    m = bytearray(bytes(cpu.ram)[:STACK_LO - 0x80000000])
    c = bytearray(bytes(side.main)[:STACK_LO - 0x80000000])
    for offset, length in MASKS:
        reg.check_ram_mask([(offset, length)])
        m[offset:offset + length] = bytes(length)
        c[offset:offset + length] = bytes(length)
    return m, c


def compare_run(cpu: mr.MipsRam, side: CSide, name: str, args: tuple[int, ...],
                label: str, mips_hooks, *, require_equal=True):
    address, size, c_index, return_kind, arg_kinds = FUNCS[name]
    del size
    cpu.hook_log.clear()
    side.sync(cpu)
    before_primitive = cpu.read32(PRIMITIVE)
    cpu.gte_reset()
    mips_regs = cpu.call(address, args, hooks=mips_hooks)
    mips_ret = mips_regs[2] & M32
    mips_hooks_log = list(cpu.hook_log)
    mips_after = cpu.read32(PRIMITIVE)
    c_ret = side.call(c_index, args)

    problems = []
    if side.lib.ae_hud_test_failed(side.ram):
        problems.append("C error: " + side.lib.ae_hud_test_error(side.ram).decode("utf-8", "replace"))
    if return_kind != "v" and mips_ret != c_ret:
        problems.append(f"return MIPS {mips_ret:08X}, C {c_ret:08X}")

    cpu_hook_log = []
    for pc, a0, a1, a2, a3 in mips_hooks_log:
        hook_name = MIPS_HOOK_NAMES.get(pc)
        if hook_name is None or hook_name == "PlaySfx":
            continue
        count = {"Ovl_8013D424": 1, "DR_MOVE": 4,
                 "ClearMemory": 3, "ModelDrawScreenSpace": 4}[hook_name]
        cpu_hook_log.append((hook_name, tuple(int(v) & M32 for v in (a0, a1, a2, a3)[:count])))
    if cpu_hook_log != side.external:
        problems.append(f"external hook sequence MIPS {cpu_hook_log} C {side.external}")
    mips_sfx = [int(a0) & M32 for pc, a0, _a1, _a2, _a3 in mips_hooks_log
                if pc == MIPSHOOKS["PlaySfx"]]
    if mips_sfx != side.sfx_ids():
        problems.append(f"PlaySfx IDs MIPS {mips_sfx} C {side.sfx_ids()}")

    def packet_candidates(primitive):
        """Decode the original HUD's three PS1 sprite-packet layouts."""
        tpage = cpu.read16(primitive + 4)
        clut = cpu.read16(primitive + 18)
        x12 = signed(cpu.read16(primitive + 12), 16)
        y14 = signed(cpu.read16(primitive + 14), 16)
        return [
            (tpage, x12, y14, cpu.read8(primitive + 20),
             cpu.read8(primitive + 22), clut,
             cpu.read8(primitive + 16), cpu.read8(primitive + 17)),
            (tpage, x12, y14, cpu.read8(primitive + 16),
             cpu.read8(primitive + 17), clut,
             cpu.read16(primitive + 20), cpu.read16(primitive + 22)),
            (tpage, signed(cpu.read16(primitive + 20), 16),
             signed(cpu.read16(primitive + 22), 16),
             cpu.read8(primitive + 16), cpu.read8(primitive + 17), clut,
             cpu.read16(primitive + 12), cpu.read16(primitive + 14)),
        ]

    for draw in side.draws:
        if draw[0] == 6:
            expected_entry = GPU + 0x80 + draw[3] * 0x4078
            if draw[2] != expected_entry:
                problems.append(f'message frame ordering entry {draw[2]:08X} != original {expected_entry:08X}')
            expected_tail = (draw[1] + draw[4] - 40) & 0xFFFFFF
            if cpu.read32(expected_entry) & 0xFFFFFF != expected_tail:
                problems.append('original message frame ordering tag does not point to its final packet')
        if draw[0] != 1 or draw[4] != 24:
            continue
        primitive = draw[1]
        requested = (draw[24], draw[7], draw[8], draw[21], draw[22], draw[23], draw[9], draw[10])
        if requested not in packet_candidates(primitive):
            problems.append(f"sprite hook vs MIPS packet at {primitive:08X}: "
                            f"{requested} not in {packet_candidates(primitive)}")

    mips_primitive_delta = (mips_after - before_primitive) & M32
    c_primitive = C.c_uint32.from_buffer(side.main, (PRIMITIVE - 0x80000000)).value
    c_primitive_delta = (c_primitive - before_primitive) & M32
    if mips_primitive_delta != c_primitive_delta:
        problems.append(f"primitive cursor MIPS +{mips_primitive_delta}, C +{c_primitive_delta}")

    m_ram, c_ram = _memory_bytes(cpu, side)
    if m_ram != c_ram:
        diffs = [i for i, (a, b) in enumerate(zip(m_ram, c_ram)) if a != b]
        problems.append("RAM differs at " + ", ".join(
            f"{0x80000000 + i:08X}(MIPS {m_ram[i]:02X}, C {c_ram[i]:02X})" for i in diffs[:8]))
    if bytes(cpu.scratch) != bytes(side.scratch):
        diffs = [i for i, (a, b) in enumerate(zip(cpu.scratch, side.scratch)) if a != b]
        problems.append("scratch differs at " + ", ".join(f"{0x1F800000 + i:08X}" for i in diffs[:8]))

    if require_equal and problems:
        return False, problems, (mips_ret, c_ret, mips_primitive_delta, c_primitive_delta)
    return not problems, problems, (mips_ret, c_ret, mips_primitive_delta, c_primitive_delta)


def mips_hook_map(external_log):
    def dr_move(cpu, regs):
        external_log.append(("DR_MOVE", tuple(regs[index] & M32 for index in range(4, 8))))
        return 0

    def clear_memory(cpu, regs):
        address, value, size = (regs[i] & M32 for i in (4, 5, 6))
        for offset in range(size):
            cpu.write8(address + offset, value & 0xFF)
        external_log.append(("ClearMemory", (address, value, size)))
        return address

    def model_draw(_cpu, regs):
        args = tuple(regs[i] & M32 for i in range(4, 8))
        external_log.append(("ModelDrawScreenSpace", args))
        return 0

    def play_sfx(_cpu, regs):
        return -1

    def overlay(_cpu, regs):
        arg = regs[4] & M32
        external_log.append(("Ovl_8013D424", (arg,)))
        return 0

    return {
        MIPSHOOKS["DR_MOVE"]: dr_move,
        MIPSHOOKS["ClearMemory"]: clear_memory,
        MIPSHOOKS["ModelDrawScreenSpace"]: model_draw,
        MIPSHOOKS["PlaySfx"]: play_sfx,
        MIPSHOOKS["Ovl_8013D424"]: overlay,
    }


def case_count(percent: int, default: int = 20) -> int:
    return max(1, default * percent // 100)


class Checker:
    def __init__(self, *, coverage=False, cases=100, seed=7, dll=None):
        self.mips = mr.MipsRam.from_extract()
        self.mips.coverage = set() if coverage else None
        self.mips.attach_gte_oracle()
        self.exe_snapshot = self.mips.snapshot()
        self.side = CSide(dll or build_dll())
        self.cases = case_count(cases)
        self.seed = seed
        self.counts = {}
        self.failures = 0
        self.first_cases = {}
        reg.declare("stack_window", f"{STACK_LO:08X}..80200000")

    def run_case(self, name: str, case: int, *, setup_only=False):
        rng = random.Random(f"{self.seed}/{name}/{case}")
        self.mips.restore(self.exe_snapshot)
        area_id = AREA_IDS[(case + list(FUNCS).index(name)) % len(AREA_IDS)]
        initialize_case(self.mips, area_id, name, case, rng)
        args = arguments(name, case, rng)
        if len(args) != len(FUNCS[name][4]):
            raise AssertionError((name, args, FUNCS[name][4]))
        ext = []
        hooks = mips_hook_map(ext)
        self.counts[name] = self.counts.get(name, 0) + 1
        ok, problems, results = compare_run(self.mips, self.side, name, args,
                                             f"#{case} area {area_id:02X}", hooks)
        if setup_only:
            return ok, problems, results, area_id, args
        if not ok:
            self.failures += 1
            print(f"FAIL {name} [#{case} area {area_id:02X}]")
            for problem in problems:
                print("  " + problem)
        if case == 0:
            self.first_cases[name] = (area_id, args)
        return ok, problems, results, area_id, args

    def run_all(self):
        start = time.time()
        for name in FUNCS:
            for case in range(self.cases):
                self.run_case(name, case)
        print(f"\nHUD cases: {sum(self.counts.values())} in {time.time() - start:.2f}s; failures={self.failures}")

    def finish(self, extra_coverage=None, default_run=False, show_coverage=False):
        summary = None
        if self.mips.coverage is not None:
            items = [(name, spec[0], spec[1], True) for name, spec in FUNCS.items()]
            items.extend((name, address, size, False)
                         for name, (address, size) in (extra_coverage or {}).items())
            summary = reg.coverage_summary(items, self.mips.coverage)
            if show_coverage:
                reg.print_coverage(summary)
        if default_run:
            reg.write_results(self.counts, summary, not self.failures,
                              function_names=list(FUNCS))
        print("per-function cases: " + ", ".join(f"{name}={self.counts.get(name, 0)}" for name in FUNCS))
        return summary


def mutate_source(source: str, marker: int) -> str:
    for name in FUNCS:
        pat = re.compile(rf"\b(?:void|int32_t|uint32_t)\s+ae_{name}\s*\([^;]*?\)\s*\{{", re.S)
        match = pat.search(source)
        if not match:
            raise SystemExit(f"could not locate ae_{name} for mutation testing")
        insertion = f"\n    ae_wr8(r, 0x{marker:08X}u, 0x{(marker ^ 0xA5) & 0xFF:02X}u); /* checker mutation */"
        source = source[:match.end()] + insertion + source[match.end():]
    return source


def run_mutations(checker: Checker):
    build_dir = ROOT / "extract/mips"
    source = (ROOT / "src/ui/ae_hud.c").read_text(encoding="utf-8")
    variants = [(MUTATION_A, "hud_mut_a"), (MUTATION_B, "hud_mut_b")]
    for marker, label in variants:
        mutated = build_dir / f"{label}_source.c"
        mutated.write_text(mutate_source(source, marker), encoding="utf-8")
        side = CSide(build_dll(mutated, label=label))
        caught = []
        for name, (area_id, args) in checker.first_cases.items():
            rng = random.Random(f"{checker.seed}/{name}/0")
            checker.mips.restore(checker.exe_snapshot)
            initialize_case(checker.mips, area_id, name, 0, rng)
            ext = []
            hooks = mips_hook_map(ext)
            ok, _problems, _results = compare_run(checker.mips, side, name, args,
                                                   f"mutation {label}", hooks,
                                                   require_equal=False)
            marker_offset = marker - 0x80000000
            expected = (marker ^ 0xA5) & 0xFF
            actual = int(side.main[marker_offset])
            if ok or actual != expected:
                raise SystemExit(f"mutation was not caught for {name} ({label}): marker {actual:02X}")
            caught.append(name)
        print(f"{label}: caught {len(caught)}/{len(FUNCS)} entry mutations")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--cases", type=int, default=100)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--mutate", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.cases <= 100:
        raise SystemExit("--cases must be between 1 and 100 percent")
    checker = Checker(coverage=args.coverage, cases=args.cases, seed=args.seed)
    checker.run_all()
    summary = checker.finish(EXTRA_COVERAGE,
                             default_run=(args.cases == 100 and args.seed == 7 and
                                          not args.mutate), show_coverage=args.coverage)
    if args.mutate:
        if checker.failures:
            raise SystemExit("mutation run requires a clean baseline first")
        run_mutations(checker)
    if summary and any(item["uncovered"] for item in summary.values()):
        print("coverage gaps remain; register proven-unreachable renderer or input ranges before marking the ledger diffed")
    if checker.failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
