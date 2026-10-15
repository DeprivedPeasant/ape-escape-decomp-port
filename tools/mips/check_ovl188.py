"""Differential check for stage-start overlay member 188.

Run with ``.venv/Scripts/python.exe tools/mips/check_ovl188.py``.  The MIPS side
loads member 188 at 0x80136000 and real area members at 0x80154000.  SDK and
platform calls cross named hooks; the native renderer callback records the
semantic drawing requests while MIPS packet memory is treated as renderer-owned.
"""
from __future__ import annotations

import argparse
import ctypes as C
import json
import random
import shutil
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/mips"), str(ROOT / "tools")]
import dump_areas as da  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

OUT = ROOT / "extract/mips"
OVERLAY = ROOT / "extract/members/188.bin"
AREA_BASE = 0x80154000
STACK_LO = 0x801FF000
STACK_POINTER = 0x801FFF00
TEST_DATA = 0x80100000
LEVEL_TABLE = TEST_DATA + 0x1000
PACKET_BASE = 0x80118000
ORDER_BASE = 0x80128000
UI = 0x8013B998
MODE = 0x800F4350
FRAME = 0x800EC350
M32 = 0xFFFFFFFF
_GTE_LIB = None

# name, address, byte length, C dispatch id, returns a value, default cases
FUNCS = [
    ("StageStartUi_LookAt",       0x80136000, 472, 0, False, 4),
    ("StageStartUi_StringTableIndex",0x801361D8, 48, 1, True, 4),
    ("Mode_StageStart",           0x8013683C, 480, 2, True, 2),
    ("StageStartUi_RunA",          0x80136A1C,1168, 3, True, 2),
    ("StageStartUi_CopyStrip",     0x80136EAC, 748, 4, False, 2),
    ("StageStartUi_DrawWipeA",     0x80137198,1052, 5, True, 5),
    ("StageStartUi_RunB",          0x801375B4,1748, 6, True, 2),
    ("StageStartUi_DrawWipeB",     0x80137C88,1108, 7, True, 5),
    ("StageStartUi_MeshInit",      0x801380DC, 60, 8, False, 2),
    ("StageStartUi_SetAreaMeshTable",0x80138118, 32, 9, False, 2),
    ("StageStartUi_AreaInit",      0x80138138, 568,10, False, 2),
    ("StageStartUi_RunScript",     0x801385B0,2132,11, False, 5),
    ("StageStartUi_DrawNumber",    0x80138E04, 808,12, False, 2),
    ("StageStartUi_DrawTitleLines",0x8013912C, 284,13, False, 3),
    ("StageStartUi_DrawObjectives",0x80139248, 648,14, False, 4),
    ("StageStartUi_DrawStageIcon", 0x801394D0, 344,15, False, 2),
    ("StageStartUi_DrawText",      0x80139628, 232,16, False, 4),
    ("StageStartUi_SetDrawStp",    0x80139710, 228,17, False, 2),
    ("StageStartUi_DrawCounter",   0x801397F4, 432,18, False, 4),
    ("StageStartUi_UpdateObjects", 0x801399A4,1128,19, False, 4),
    ("StageStartUi_CopyBuffer",    0x80139E0C, 212,20, False, 4),
    ("StageStartUi_ClampVerts",    0x80139EE0, 216,21, False, 6),
    ("StageStartUi_WipeQuadA",     0x80139FB8, 340,22, False, 4),
    ("StageStartUi_WipeQuadB",     0x8013A10C, 552,23, False, 4),
    ("StageStartUi_DrawScanlines", 0x8013A3C4, 996,24, False, 4),
    ("RippleGrid_Init2",           0x8013A7A8, 280,25, False, 2),
    ("RippleGrid_Smooth2",         0x8013A8C0, 508,26, False, 4),
    ("RippleGrid_Draw2",           0x8013AABC, 280,27, False, 4),
]
BY_NAME = {row[0]: row for row in FUNCS}

# All non-native calls made by the port are intercepted at the same entry on
# the MIPS side.  Their exact argument words are compared with the C hook log.
HOOKS = {
    0x000000B0: ("Exit", 1, 0),
    0x8002B52C: ("PadGetState", 1, 2),
    0x8002B5F8: ("PadInfoMode", 3, 7),
    0x80012CA0: ("PadSetupDualShock", 1, 0),
    0x80013128: ("PadDemoPlaybackStep", 0, 0),
    0x800140B4: ("StickToButtons", 0, 0),
    0x800A1398: ("SpuWaitIdle", 0, 0),
    0x8009D79C: ("SpuWaitLoop", 1, 0),
    0x800A3720: ("printf", 4, 0),
    0x8002E520: ("VSync", 1, 0),
    0x800A1F3C: ("SeqTrackClose", 2, 0),
    0x80011F6C: ("XaPlay", 1, 0),
    0x800A2B6C: ("SeqTrackSetPlayState", 2, 0),
    0x800A1CCC: ("SoundBankFree", 1, 0),
    0x800A342C: ("SoundStopAllVoices", 1, 0),
    0x8002B380: ("DR_MOVE", 4, 0),
    0x80028EFC: ("PutDispEnv", 1, 0),
    0x80028D30: ("PutDrawEnv", 1, 0),
    0x800305B0: ("ScaleMatrixL", 2, 0),
    0x80015C34: ("MeshInit", 2, 0),
    0x80028850: ("ClearImage", 4, 0),
    0x800286CC: ("DrawSync", 1, 0),
    0x8005CFEC: ("InitDisplay", 0, 0),
    0x80028350: ("ResetGraph", 1, 0),
    0x8002FAF0: ("SetGeomScreen", 1, 0),
    0x80015D58: ("SetColorMatrixMode", 4, 0),
    0x80011194: ("ArcLoadEntry", 1, 0),
    0x80011104: ("LoadListStart", 3, 0),
    0x8005DF48: ("BeginFrame", 0, 0),
    0x800964DC: ("AreaKindVisitedSet", 1, 0),
    0x80028CC0: ("DrawOTag", 1, 0),
    0x800125B8: ("CdMgrUpdate", 0, 0),
    0x80097FB0: ("LevelCaughtCount", 1, 5),
    0x80097D0C: ("LevelGetInfo", 2, 0),
    0x8005F68C: ("HudDrawTimeRankIcon", 4, 0),
    0x800116C0: ("LoadListStep", 0, 0),
    0x80086170: ("SoundStopAndRestoreVolume", 0, 0),
    0x80016F34: ("DrawAreaProps", 1, 0),
    0x80016A60: ("ModelTransformSkinned", 1, 0),
    0x80017CD4: ("ModelDraw", 3, 0),
    0x8001C918: ("ModelEmitPrimsScratchColors", 3, 0),
    0x800160EC: ("ModelTransform", 3, 0),
    0x80017EFC: ("ModelDrawTransformed", 4, 0),
    0x80097E60: ("LevelTableGet", 1, LEVEL_TABLE),
    0x80096580: ("LevelProgressGet", 1, 0),
}

RENDER_ONLY = {
    "HudDrawTimeCell", "StageStartUi_DrawGlyph", "RippleGrid_Draw2",
    "StageStartUi_DrawStageIcon", "StageStartUi_SetDrawStp",
    "StageStartUi_DrawCounterDigits", "StageStartUi_CopyStrip",
    "StageStartUi_CopyBuffer", "StageStartUi_WipeQuadA",
    "StageStartUi_WipeQuadB", "StageStartUi_DrawScanlines",
    "StageStartUi_DrawWipeBackground", "ModelTransformSkinned",
    "ModelDraw", "ModelEmitPrimsScratchColors", "ModelTransform",
    "ModelDrawTransformed",
}

RENDER_HOOKS = {
    "ModelTransformSkinned": 0x80016A60,
    "ModelDraw": 0x80017CD4,
    "ModelEmitPrimsScratchColors": 0x8001C918,
    "ModelTransform": 0x800160EC,
    "ModelDrawTransformed": 0x80017EFC,
}


@dataclass(frozen=True)
class Case:
    label: str
    area: int
    args: tuple[int, ...]
    variant: int = 0
    pad_after: int = 0
    press_on_wipe_kind: int = 0
    load_after: int = 0
    mode_value: int | None = None
    wipe_mode: int | None = None
    ui_timer: int | None = None
    frame_timer: int | None = None
    animation_frame: int | None = None
    animation_timer: int | None = None
    wipe_a_timer: int | None = None
    wipe_b_timer: int | None = None
    pending_wipe_a: int = 0
    pending_wipe_b: int = 0
    level_info_progress: int = 0
    level_info_kind: int = 0
    followup_info_kind: int | None = None
    level_info_total: int = 1
    level_info_required: int = 3
    progress_byte_11: int | None = None
    level_progress: int = 0
    area_kind_override: int | None = None
    ui_hide_objectives: bool = False
    level_table_values: tuple[int, int, int, int] = (10, 20, 30, 40)
    number_first: int | None = None
    number_second: int | None = None
    number_suffix: int | None = None
    caught_count: int = 5
    object_flags: int | None = None
    object_position_time: int = 4
    object_translation_time: int = 0
    object_end_frame: int | None = None
    object_loop_index: int | None = None
    stage_model_active: bool = False
    script_pointer: int | None = None
    script_flags: int | None = None
    script_tick: int | None = None
    script_command: int | None = None
    objective_line_type: int | None = None


class CSide:
    def __init__(self, dll: Path):
        self.lib = C.CDLL(str(dll))
        self.P, self.U = C.c_void_p, C.c_uint32
        self.Call = C.CFUNCTYPE(C.c_int32, self.P, self.P, C.c_char_p,
                                self.U, self.U, C.POINTER(self.U))
        self.Render = C.CFUNCTYPE(None, self.P, self.P, C.c_char_p,
                                  self.U, C.POINTER(self.U))
        lib = self.lib
        lib.ovl188_ram_create.restype = self.P
        lib.ovl188_ram_destroy.argtypes = [self.P]
        lib.ovl188_ram_main.argtypes = [self.P]
        lib.ovl188_ram_main.restype = self.P
        lib.ovl188_ram_scratch.argtypes = [self.P]
        lib.ovl188_ram_scratch.restype = self.P
        lib.ovl188_ram_clear_error.argtypes = [self.P]
        lib.ovl188_ram_failed.argtypes = [self.P]
        lib.ovl188_ram_failed.restype = C.c_int
        lib.ovl188_ram_error.argtypes = [self.P]
        lib.ovl188_ram_error.restype = C.c_char_p
        lib.ovl188_call.argtypes = [self.P, self.U, C.POINTER(self.U),
                                    self.U, self.Call, self.Render, self.P]
        lib.ovl188_call.restype = C.c_int32
        self.ram = lib.ovl188_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(lib.ovl188_ram_main(self.ram))
        self.scratch = (C.c_uint8 * 0x400).from_address(lib.ovl188_ram_scratch(self.ram))
        self.calls: list[tuple] = []
        self.renders: list[tuple] = []
        self.events: list[tuple] = []
        self.current_selector = 0
        self.pad_after = 0
        self.pad_calls = 0
        self.press_on_wipe_kind = 0
        self.input_sent = False
        self.load_after = 0
        self.load_steps = 0
        self.case: Case | None = None

    def sync(self, cpu: mr.MipsRam, selector: int = 0, pad_after: int = 0,
             press_on_wipe_kind: int = 0, load_after: int = 0,
             case: Case | None = None):
        C.memmove(self.main, bytes(cpu.ram), len(self.main))
        C.memmove(self.scratch, bytes(cpu.scratch), len(self.scratch))
        C.c_uint32.from_address(C.addressof(self.scratch) + 0x3F0).value = selector & M32
        self.calls.clear()
        self.renders.clear()
        self.events.clear()
        self.current_selector = selector
        self.pad_after = pad_after
        self.pad_calls = 0
        self.press_on_wipe_kind = press_on_wipe_kind
        self.input_sent = False
        self.load_after = load_after
        self.load_steps = 0
        self.case = case
        self.lib.ovl188_ram_clear_error(self.ram)

    def c_call(self, _user, ram, name, address, argc, args):
        arg_words = tuple(args[i] for i in range(argc)) if args else ()
        event = (name.decode("ascii"), address, argc, arg_words)
        self.calls.append(event)
        self.events.append(event)
        if address == 0x80011194 and argc:
            self.load_arc_entries(arg_words[0])
        if address == 0x8002B52C:
            self.pad_calls += 1
            if self.pad_after and self.pad_calls >= self.pad_after:
                pressed = (self.pad_calls - self.pad_after) % 2 == 0
                self.write8(0x800E52D2, 0xF7 if pressed else 0xFF)
                self.write8(0x800E52D3, 0x9F if pressed else 0xFF)
            if self.press_on_wipe_kind:
                wipe_kind = self.read32(0x8013AE60)
                if wipe_kind == self.press_on_wipe_kind and not self.input_sent:
                    self.write8(0x800E52D2, 0xF7)
                    self.write8(0x800E52D3, 0x9F)
                    self.input_sent = True
                elif wipe_kind != self.press_on_wipe_kind:
                    self.write8(0x800E52D2, 0xFF)
                    self.write8(0x800E52D3, 0xFF)
        if address == 0x80097D0C and argc >= 2:
            output = arg_words[1]
            fixture = self.case or Case("default", 1, ())
            self.write32(output, fixture.level_info_progress)
            self.write16(output + 4, fixture.level_info_required)
            self.write16(output + 6, 12)
            self.write16(output + 8, 8)
            self.write16(output + 10, 0)
            kind = fixture.level_info_kind
            if arg_words[0] == 24 and fixture.followup_info_kind is not None:
                kind = fixture.followup_info_kind
            self.write16(output + 0x10, kind)
            self.write16(output + 0x24, fixture.level_info_total)
        if address == 0x800116C0 and self.load_after:
            self.load_steps += 1
            return -1 if self.load_steps >= self.load_after else 0
        if address == 0x80096580:
            return (self.case.level_progress if self.case is not None else 0)
        if address == 0x80097FB0:
            return (self.case.caught_count if self.case is not None else 5)
        return dict((addr, ret) for addr, (_n, _a, ret) in HOOKS.items()).get(address, 0)

    def c_render(self, _user, _ram, name, argc, args):
        arg_words = tuple(args[i] for i in range(argc)) if args else ()
        render_name = name.decode("ascii")
        self.renders.append((render_name, argc, arg_words))
        address = RENDER_HOOKS.get(render_name)
        if address is not None:
            self.events.append((render_name, address, argc, arg_words))

    def write16(self, address, value):
        C.c_uint16.from_address(self._memory_address(address, 2)).value = value & 0xFFFF

    def write32(self, address, value):
        C.c_uint32.from_address(self._memory_address(address, 4)).value = value & M32

    def write8(self, address, value):
        C.c_uint8.from_address(self._memory_address(address, 1)).value = value & 0xFF

    def _memory_address(self, address, size):
        if 0x1F800000 <= address and address + size <= 0x1F800000 + len(self.scratch):
            return C.addressof(self.scratch) + address - 0x1F800000
        if 0x80000000 <= address and address + size <= 0x80200000:
            return C.addressof(self.main) + address - 0x80000000
        raise ValueError(f"hook write outside main RAM and scratchpad: {address:08X}:{size}")

    def read16(self, address):
        return C.c_uint16.from_address(C.addressof(self.main) + address - 0x80000000).value

    def read32(self, address):
        return C.c_uint32.from_address(C.addressof(self.main) + address - 0x80000000).value

    def load_arc_entries(self, entry):
        cursor = entry
        for _ in range(16):
            member = self.read16(cursor)
            flags = self.read16(cursor + 2)
            destination = self.read32(cursor + 4)
            if flags == 0xFFFF:
                return
            if member and 0x80000000 <= destination < 0x80200000:
                path = ROOT / f"extract/members/{member:03X}.bin"
                if not path.is_file():
                    raise FileNotFoundError(path)
                data = path.read_bytes()
                offset = destination - 0x80000000
                if offset + len(data) > len(self.main):
                    raise ValueError(f"archive member {member:03X} exceeds RAM at {destination:08X}")
                C.memmove(C.addressof(self.main) + offset, data, len(data))
            cursor += 8
        raise ValueError(f"unterminated archive list at {entry:08X}")

    def run(self, function_id, args):
        words = (self.U * 6)(*(list(args[:6]) + [0] * (6 - len(args[:6]))))
        result = self.lib.ovl188_call(self.ram, function_id, words,
                                      STACK_POINTER, self.Call(self.c_call),
                                      self.Render(self.c_render), None)
        failed = self.lib.ovl188_ram_failed(self.ram)
        error = self.lib.ovl188_ram_error(self.ram).decode("utf-8", "replace")
        return result, bool(failed), error

    def result(self):
        return bytes(self.main), bytes(self.scratch)


def build_dll(source: Path | None = None, tag="base") -> Path:
    OUT.mkdir(parents=True, exist_ok=True)
    dll = OUT / f"ovl188_{tag}.dll"
    src = source or ROOT / "src/modes/ae_ovl188.c"
    sources = [
        ROOT / "src/core/ae_ram.c", ROOT / "src/core/ae_sdk.c",
        ROOT / "src/core/ae_sdk_matrix.c", ROOT / "src/core/ae_pad.c",
        ROOT / "src/core/ae_pose.c", ROOT / "src/bodies/ae_bodies.c",
        ROOT / "src/math/native_sdk_apply.c",
        ROOT / "src/world/ae_areaset.c", ROOT / "src/world/ae_misc_batch.c",
        ROOT / "src/world/ae_stagefade.c", ROOT / "src/sound/ae_bgm.c",
        ROOT / "src/sound/ae_sound.c", ROOT / "src/ui/ae_hud.c",
        ROOT / "src/player/ae_player_spawn.c", ROOT / "src/player/ae_player1.c",
        ROOT / "src/player/ae_player_act_state.c", src,
        ROOT / "tools/mips/ovl188_test_support.c",
        ROOT / "tools/mips/ovl188_test_api.c",
    ]
    gcc = shutil.which("gcc")
    if not gcc:
        raise RuntimeError("gcc is required to build the overlay test DLL")
    cmd = [gcc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
           "-fvisibility=hidden", "-ffunction-sections", "-fdata-sections",
           "-I", str(ROOT / "src/core"), "-I", str(ROOT / "src/ui"),
           "-I", str(ROOT / "src/world"), "-I", str(ROOT / "src/sound"),
           "-I", str(ROOT / "src/modes"), "-I", str(ROOT / "src/bodies"),
           "-I", str(ROOT / "src/player"), "-shared", "-static-libgcc",
           "-Wl,--gc-sections", *map(str, sources), "-lm", "-o", str(dll)]
    subprocess.run(cmd, check=True)
    return dll


def area_blob(area: int) -> bytes | None:
    member = da.area_members().get(area)
    if member is None:
        return None
    path = ROOT / f"extract/members/{member:03X}.bin"
    if not path.is_file():
        raise FileNotFoundError(path)
    return path.read_bytes()


def setup_base(cpu: mr.MipsRam, area: int):
    cpu.load_area(OVERLAY.read_bytes(), 0x80136000)
    blob = area_blob(area)
    if blob is not None:
        cpu.load_area(blob, AREA_BASE)
    cpu.write32(0x800EC354, PACKET_BASE)
    cpu.write32(0x800EC358, ORDER_BASE)
    cpu.write32(0x800EC350, 0)
    cpu.write32(0x800EC35C, 0)
    cpu.write32(0x800F4350 + 0x120, 10)
    cpu.write16(0x800F4350 + 0x126, area)
    cpu.write32(UI + 0x20, 0x8013B008)
    cpu.write16(0x8013B478, 0)
    cpu.write16(0x8013B47A, 0)
    cpu.write16(0x8013B480, 0)
    cpu.write16(0x8013B482, 0)
    cpu.write16(0x800E5480, 0)
    cpu.write16(0x800E5478, 0)
    cpu.write8(0x800E5470, 0x79)  # PadGetState fixture: connected, no analog report.
    cpu.write8(0x800E5471, 0x05)
    cpu.write8(0x800E52D2, 0xFF)
    cpu.write8(0x800E52D3, 0xFF)
    cpu.write8(0x800E52F4, 0xFF)
    cpu.write8(0x800E52F5, 0xFF)
    cpu.write32(TEST_DATA + 0x100, 0)
    cpu.write16(LEVEL_TABLE, 10)
    cpu.write16(LEVEL_TABLE + 2, 20)
    cpu.write16(LEVEL_TABLE + 4, 30)
    cpu.write16(LEVEL_TABLE + 6, 40)
    # Give both allocators a valid, repeatable heap before functions that allocate.
    cpu.call(0x8008C01C, hooks={}, limit=2_000_000)


def scenario_cases(name: str) -> list[Case]:
    # Every case runs from a fresh EXE + overlay + area-member image.
    if name == "StageStartUi_LookAt":
        return [Case("view axes and translation", 1, (TEST_DATA, TEST_DATA + 0x20), 0),
                Case("vertical direction", 2, (TEST_DATA, TEST_DATA + 0x20), 1),
                Case("coincident XZ", 5, (TEST_DATA, TEST_DATA + 0x20), 2),
                Case("negative coordinates", 8, (TEST_DATA, TEST_DATA + 0x20), 3)]
    if name == "StageStartUi_StringTableIndex":
        return [Case("first", 1, (TEST_DATA, 0)), Case("middle", 2, (TEST_DATA, 2)),
                Case("last", 5, (TEST_DATA, 3)), Case("negative index", 8, (TEST_DATA, -1))]
    if name == "Mode_StageStart":
        return [Case("special-area handoff", 6, ()),
                Case("normal area", 1, (), 0, 0, 2, 3),
                Case("mode 8 special-area handoff", 6, (), pad_after=1,
                     mode_value=8, progress_byte_11=0),
                Case("mode 18 score handoff", 1, (), 0, 0, 2, 3,
                     mode_value=18, wipe_mode=0, animation_frame=0,
                     animation_timer=0),
                Case("mode 23 return", 45, (), 0, 0, 2, 3,
                     mode_value=23, level_progress=1)]
    if name == "StageStartUi_RunA":
        return [Case("wipe completes", 1, (), wipe_mode=3),
                Case("area kind cadence", 8, (), wipe_mode=3),
                Case("area kind 4 cadence", 20, (), wipe_mode=3),
                Case("area kind 5 cadence", 29, (), wipe_mode=3),
                Case("area kind 7 cadence", 37, (), wipe_mode=3),
                Case("area kind 10 cadence", 53, (), wipe_mode=3),
                Case("area kind 11 cadence", 69, (), wipe_mode=3),
                Case("opening and input exit", 1, (), wipe_mode=0,
                     press_on_wipe_kind=2)]
    if name == "StageStartUi_CopyStrip":
        return [Case("timer zero", 1, ()), Case("timer active", 2, ())]
    if name == "StageStartUi_DrawWipeA":
        return [Case(f"wipe mode {mode}", 1, (), mode) for mode in range(4)] + [
            Case("invalid wipe kind", 1, (), wipe_mode=4),
            Case("negative wipe kind", 1, (), wipe_mode=-1),
            Case("load completes", 1, (), 4, 0, 0, 1),
            Case("opening area setup", 1, (), wipe_mode=0, wipe_a_timer=120),
            Case("kind ten opening setup", 2, (), wipe_mode=0,
                 wipe_a_timer=120, area_kind_override=10),
            Case("kind ten non-level ten scene", 53, (), wipe_mode=2),
            Case("kind ten textured scene", 2, (), wipe_mode=2,
                 area_kind_override=10),
            Case("hidden objectives model setup", 69, (), wipe_mode=0,
                 wipe_a_timer=120, ui_hide_objectives=True),
            Case("opening geometry timer six", 1, (), wipe_mode=1,
                 animation_timer=6),
            Case("opening geometry timer ten", 1, (), wipe_mode=1,
                 animation_timer=10),
            Case("closing geometry timer six", 1, (), wipe_mode=3,
                 animation_frame=0, animation_timer=6),
            Case("closing geometry timer ten", 1, (), wipe_mode=3,
                 animation_frame=0, animation_timer=10),
            Case("opening sound load", 1, (), wipe_mode=0, wipe_a_timer=120,
                 pending_wipe_a=1, load_after=1),
            Case("closing wipe late frame", 1, (), wipe_mode=3,
                 animation_frame=2, animation_timer=4)]
    if name == "StageStartUi_RunB":
        return [Case("area 19 start", 19, (), 0, 0, 2, 3),
                Case("mode 18 start", 1, (), 18, 0, 2, 3),
                Case("progression area zero", 45, (), 0, 0, 2, 3,
                     level_progress=0),
                Case("progression area unlocked", 45, (), 0, 0, 2, 3,
                     level_progress=1),
                Case("progression area 63", 63, (), 0, 0, 2, 3,
                     level_progress=1),
                Case("progression area 69", 69, (), 0, 0, 2, 3,
                     level_progress=1),
                Case("progression area 87", 87, (), 0, 0, 2, 3,
                     level_progress=1),
                Case("ordinary area below 70", 46, (), 0, 0, 2, 3),
                Case("ordinary area above 70", 88, (), 0, 0, 2, 3),
                Case("special area 36", 36, (), 0, 0, 2, 3),
                Case("mode 18 completed score", 45, (), 0, 0, 2, 3,
                     mode_value=18, level_info_progress=-1),
                Case("mode 18 third threshold", 45, (), 0, 0, 2, 3,
                     mode_value=18, level_info_progress=35,
                     level_table_values=(50, 40, 30, 20)),
                Case("mode 18 second threshold", 45, (), 0, 0, 2, 3,
                     mode_value=18, level_info_progress=45,
                     level_table_values=(50, 40, 30, 20)),
                Case("mode 18 no threshold", 45, (), 0, 0, 2, 3,
                     mode_value=18, level_info_progress=55,
                     level_table_values=(50, 40, 30, 20))]
    if name == "StageStartUi_DrawWipeB":
        return [Case(f"wipe mode {mode}", 1, (), mode) for mode in range(4)] + [
            Case("invalid wipe kind", 1, (), wipe_mode=4),
            Case("negative wipe kind", 1, (), wipe_mode=-1),
            Case("special variant", 1, (), 5),
            Case("opening area setup", 1, (), wipe_mode=0, wipe_b_timer=120),
            Case("opening sound load", 1, (), wipe_mode=0, wipe_b_timer=120,
                 pending_wipe_b=1, load_after=1),
            Case("closing wipe late frame", 1, (), wipe_mode=3,
                 animation_frame=2, animation_timer=4),
            Case("closing load completes", 1, (), wipe_mode=3,
                 animation_frame=2, animation_timer=6,
                 pending_wipe_b=1, load_after=1),
            Case("closing geometry timer six", 1, (), wipe_mode=3,
                 animation_frame=0, animation_timer=6),
            Case("closing geometry timer ten", 1, (), wipe_mode=3,
                 animation_frame=0, animation_timer=10),
            Case("opening animation timer six", 1, (), wipe_mode=1,
                 animation_timer=6),
            Case("opening animation timer ten", 1, (), wipe_mode=1,
                 animation_timer=10),
            Case("mode 18 area setup", 1, (), wipe_mode=0, wipe_b_timer=120,
                 mode_value=18),
            Case("mode 19 area setup", 1, (), wipe_mode=0, wipe_b_timer=120,
                 mode_value=19),
            Case("mode 23 variant 7", 1, (), wipe_mode=0, wipe_b_timer=120,
                 mode_value=23),
            Case("mode 23 variant 14", 1, (), wipe_mode=0, wipe_b_timer=120,
                 mode_value=23),
            Case("mode 23 variant 9", 1, (), wipe_mode=0, wipe_b_timer=120,
                 mode_value=23)]
    if name == "StageStartUi_MeshInit":
        return [Case("area 1", 1, ()), Case("area 2", 2, ())]
    if name == "StageStartUi_SetAreaMeshTable":
        return [Case("area 1", 1, ()), Case("area 8", 8, ())]
    if name == "StageStartUi_AreaInit":
        return [Case("area 1, one static model", 1, (1,)), Case("area 2, two static models", 2, (2,))]
    if name == "StageStartUi_RunScript":
        return [Case("terminal", 1, (), 0), Case("vector command", 2, (), 1),
                Case("wait command", 8, (), 2), Case("soundtrack command", 19, (), 3),
                Case("object flags command", 5, (), 4),
                Case("object and vector commands", 1, (), 5),
                Case("wait pending", 1, (), 6),
                Case("wait ready", 1, (), 7),
                Case("return address", 1, (), 8),
                Case("wait flag", 1, (), 9),
                Case("count gate", 1, (), 10),
                Case("invalid object indexes", 1, (), 11),
                Case("unknown command", 1, (), 12),
                Case("ordinary soundtrack", 1, (), 13),
                Case("script line draws", 1, (), 14),
                Case("script tick active", 1, (), script_flags=1,
                     script_tick=7),
                Case("script tick saturated", 1, (), script_flags=1,
                     script_tick=0xFFFF),
                Case("unsupported 0x100 command", 1, (),
                     script_command=0x0100),
                Case("unsupported 0x2c0 command", 1, (),
                     script_command=0x02C0),
                Case("script line type one", 1, (), variant=14,
                     objective_line_type=1),
                Case("script line type three", 1, (), variant=14,
                     objective_line_type=3),
                Case("script line negative type", 1, (), variant=14,
                     objective_line_type=0xFF)] + [
                Case(f"soundtrack area kind {kind}", 8, (), variant=13,
                     area_kind_override=kind) for kind in range(2, 12)] + [
                Case("soundtrack unknown area kind", 8, (), variant=13,
                     area_kind_override=12)]
    if name == "StageStartUi_DrawNumber":
        return [Case("small values", 1, (), 0),
                Case("two-digit values", 2, (), 1),
                Case("negative values", 5, (), 2),
                Case("larger values", 8, (), 3),
                Case("middle digits", 19, (), 4, caught_count=18),
                Case("suffix past glyph six", 19, (), 5, number_suffix=8)]
    if name == "StageStartUi_DrawTitleLines":
        return [Case("one line", 1, (TEST_DATA + 0x20, 1)),
                Case("two lines", 2, (TEST_DATA + 0x20, 2)),
                Case("three lines", 8, (TEST_DATA + 0x20, 3))]
    if name == "StageStartUi_DrawObjectives":
        return [Case("normal objectives", 1, ()), Case("time attack", 2, (), 18),
                Case("special area objectives", 19, ()), Case("collected count", 36, ()),
                Case("collectible objective", 1, (), level_info_kind=1,
                     level_info_total=8),
                Case("empty collectible objective", 1, (), level_info_kind=1,
                     level_info_total=0, followup_info_kind=0),
                Case("special objective", 1, (), level_info_kind=2),
                Case("unsupported objective", 1, (), level_info_kind=3),
                Case("negative objective kind", 1, (), level_info_kind=-1),
                Case("optional count objective", 1, (), level_info_required=0),
                Case("late level count objective", 63, ()),
                Case("unsupported mode", 1, (), mode_value=11)]
    if name == "StageStartUi_DrawStageIcon":
        return [Case("mode 10", 1, ()), Case("mode 18", 2, (), 18),
                Case("special area 19", 19, ()),
                Case("special area 36", 36, ()),
                Case("other mode", 1, (), mode_value=23)]
    if name == "StageStartUi_DrawText":
        return [Case("three glyphs", 1, (TEST_DATA,)), Case("empty text", 2, (TEST_DATA + 0x20,)),
                Case("one glyph", 8, (TEST_DATA + 0x40,)), Case("high glyph ids", 19, (TEST_DATA + 0x60,))]
    if name == "StageStartUi_SetDrawStp":
        return [Case("timer zero", 1, ()), Case("timer active", 2, ())]
    if name == "StageStartUi_DrawCounter":
        return [Case("zero", 1, (128, 88, 0)), Case("middle", 2, (128, 88, 47)),
                Case("clamped", 8, (128, 88, 120)), Case("negative", 19, (128, 88, -3))]
    if name == "StageStartUi_UpdateObjects":
        return [Case("empty objects", 1, (0x8013B6F0,)), Case("active object", 2, (0x8013B6F0,)),
                Case("scratch model", 8, (0x8013B6F0,)),
                Case("alternate mode", 19, (0x8013B6F0,), 23),
                Case("animated active object", 1, (0x8013B6F0,),
                     object_flags=3, mode_value=10, script_pointer=0x8013B008,
                     object_translation_time=4),
                Case("animation loop", 1, (0x8013B6F0,),
                     object_end_frame=0, object_loop_index=0),
                Case("animation stop", 2, (0x8013B6F0,),
                     object_end_frame=0, object_loop_index=-1),
                Case("area prop transform", 8, (0x8013B6F0,),
                     stage_model_active=True),
                Case("position only animation", 2, (0x8013B6F0,),
                     object_flags=1, object_position_time=1)]
    if name == "StageStartUi_CopyBuffer":
        return [Case("row 0", 1, (0,), ui_timer=0, frame_timer=0),
                Case("row 30", 2, (30,)),
                Case("animated row", 8, (120,), frame_timer=1),
                Case("negative row", 19, (-2,))]
    if name == "StageStartUi_ClampVerts":
        return [Case(f"mode {mode}", 1, (mode, 12, 120)) for mode in (-1, 0, 1, 2)] + [
            Case("empty", 2, (0, 0, -40)), Case("full range", 8, (1, 12, 80))]
    if name in ("StageStartUi_WipeQuadA", "StageStartUi_WipeQuadB"):
        return [Case("early wipe", 1, (0, 2)), Case("wide wipe", 2, (192, 128)),
                Case("negative X", 8, (-32, 30)), Case("late wipe", 19, (192, 150))]
    if name == "StageStartUi_DrawScanlines":
        return [Case("frame zero", 1, ()), Case("frame 1", 2, ()),
                Case("frame 316", 8, ()), Case("timer rollover", 19, ()),
                Case("negative scanline offset", 1, (), frame_timer=300)]
    if name == "RippleGrid_Init2":
        return [Case("area 1", 1, ()), Case("area 2", 2, ())]
    if name == "RippleGrid_Smooth2":
        return [Case("frame zero", 1, ()), Case("frame one", 2, ()),
                Case("frame 255", 8, ()), Case("frame wrap", 19, ())]
    if name == "RippleGrid_Draw2":
        return [Case("frame zero", 1, ()), Case("frame one", 2, ()),
                Case("frame 255", 8, ()), Case("frame wrap", 19, ())]
    raise KeyError(name)


def setup_case(cpu: mr.MipsRam, name: str, case: Case):
    # Common draw/environment state and data strings/records.
    cpu.write32(FRAME, 0)
    cpu.write32(FRAME + 0x0C, 0)
    cpu.write16(UI + 0x00, 32)
    cpu.write16(UI + 0x02, 48)
    cpu.write16(UI + 0x04, 0x123)
    cpu.write16(UI + 0x06, 0x42)
    default_mode = 18 if case.variant == 18 or name == "StageStartUi_DrawStageIcon" else 10
    cpu.write16(MODE + 0x120,
                default_mode if case.mode_value is None else case.mode_value)
    cpu.write16(MODE + 0x126, case.area)
    if case.area_kind_override is not None:
        cpu.write16(0x800A81B0 + case.area * 8 + 6,
                    case.area_kind_override)
    cpu.write32(UI + 0x20, TEST_DATA + 0x100)
    cpu.write32(UI + 0x28, 1)
    cpu.write16(UI + 0x2A, 0)
    cpu.write32(UI + 0x2C, 12 if case.ui_timer is None else case.ui_timer)
    if case.ui_hide_objectives:
        cpu.write32(UI + 0x48, 1)
    cpu.write16(UI + 0x3A, 4)
    cpu.write16(UI + 0x42, 4)
    cpu.write32(0x8013B6E0, 50)
    cpu.write32(0x8013B6E4, 30)
    cpu.write32(0x8013B9F4, 0)
    cpu.write32(0x8013B9F8, TEST_DATA + 0x400)
    cpu.write16(0x800AE8F0, 0x1234)
    cpu.write16(0x800AE8F2, 0x5678)
    cpu.write32(0x800F4350 + 0x148, 300)
    cpu.write32(0x800F4350 + 0x14C, 400)
    cpu.write_bytes(TEST_DATA, b"Ape Escape\0Stage two\0\0X\0")
    cpu.write16(TEST_DATA + 0x20, 0)
    cpu.write16(TEST_DATA + 0x22, 40)
    cpu.write16(TEST_DATA + 0x24, 48)
    cpu.write16(TEST_DATA + 0x26, 1)
    cpu.write16(TEST_DATA + 0x28, 64)
    cpu.write16(TEST_DATA + 0x2A, 56)
    cpu.write16(TEST_DATA + 0x2C, 2)
    cpu.write16(TEST_DATA + 0x2E, 80)
    cpu.write16(TEST_DATA + 0x30, 64)
    cpu.write16(TEST_DATA + 0x32, 0xFFFF)
    cpu.write_bytes(TEST_DATA + 0x80, b"\x00\x00\x00\x00\x00\x00\x00\x00")
    for index, value in enumerate(case.level_table_values):
        cpu.write16(LEVEL_TABLE + index * 2, value)
    cpu.write_bytes(TEST_DATA + 0x400, bytes((index * 7 + case.variant * 13) & 0xFF for index in range(0x600)))
    for glyph in range(256):
        cpu.write8(0x8013B5E0 + glyph, 6 + (glyph % 7))
    for index in range(12):
        vert = 0x800E7D00 + index * 8
        cpu.write16(vert, index * 11 - 20)
        cpu.write16(vert + 2, index * 31 - 80)
        cpu.write16(vert + 4, 0x100 + index)
    for index in range(0, 5):
        cpu.write16(0x8013B6F0 + index * 0x88 + 0x72, 0)
    if name == "StageStartUi_LookAt":
        eye, target = case.args
        vectors = [((0, 0, -4096), (0, 0, 0)), ((0, -4096, 0), (0, 0, 0)),
                   ((100, 200, 300), (100, -300, 300)), ((-32768, 32767, 32767), (32767, -32768, -32768))]
        e, t = vectors[case.variant]
        cpu.write_bytes(eye, __import__("struct").pack("<3h", *e))
        cpu.write_bytes(target, __import__("struct").pack("<3h", *t))
    elif name == "StageStartUi_StringTableIndex":
        cpu.write_bytes(case.args[0], b"one\0two\0\0last\0")
    elif name == "StageStartUi_DrawText":
        payload = {"three glyphs": b"AB3\0", "empty text": b"\0", "one glyph": b"Z\0", "high glyph ids": bytes((0xA0, 0xF1, 0))}[case.label]
        cpu.write_bytes(case.args[0], payload)
    elif name == "StageStartUi_DrawTitleLines":
        records, count = case.args
        cpu.write16(records, 0)
        cpu.write16(records + 6, 1 if count > 1 else 0xFFFF)
        cpu.write16(records + 12, 2 if count > 2 else 0xFFFF)
    elif name == "StageStartUi_RunScript":
        script = TEST_DATA + 0x180
        programs = [b"\xc0\x3f", b"\x50\x00\x64\x00\xc8\x00\x2c\x01\xf4\x01\xc0\x3f",
                    b"\xc0\x01\x03\x00\x40\x02\xc0\x3f", b"\xc0\x03\xc0\x3f",
                    b"\x00\x00\x01\x00\xc0\x3f"]
        command_words = {
            5: [0x0000, 0x0003, 0x0040, 100, 200, 300, 4,
                0x0080, 110, 220, 330, 0, 0x00C0, 0,
                0x0140, 1, 0x0180, 0,
                0x0050, 10, 20, 30, 0x0051, 40, 50, 60,
                0x0052, 70, 80, 90, 4, 0x0053, 100, 110, 120, 4,
                0x0240, 0x0340, 0x0300, 0x0380, 0, 0x03C0, 0x3FC0],
            6: [0x01C0, 3, 0x0200, 0x3FC0],
            7: [0x01C0, 0, 0x0200, 0x3FC0],
            8: [0x0280, 0x3FC0],
            9: [0x0340, 0x3FC0],
            10: [0x0380, 2, 0x3FC0],
            11: [0x0005, 0, 0x0045, 0x0085, 0x0145, 0x0185, 0x3FC0],
            12: [0x0400, 0x3FC0],
            13: [0x03C0, 0x3FC0],
            14: [0x3FC0],
        }
        if case.script_command is not None:
            program = struct.pack("<HH", case.script_command, 0x3FC0)
        elif case.variant < 5:
            program = programs[case.variant]
        else:
            program = struct.pack("<" + "H" * len(command_words[case.variant]),
                                  *command_words[case.variant])
            if case.variant == 8:
                cpu.write32(UI + 0x24, script + 2)
            if case.variant == 9:
                cpu.write32(UI + 0x58, 0)
                cpu.write32(UI + 0x44, 3)
            if case.variant == 10:
                cpu.write32(UI + 0x5C, 0)
            if case.variant == 5:
                cpu.write32(UI + 0x58, 2)
                cpu.write32(UI + 0x5C, 1)
                cpu.write32(0x8013B6F0 + 0x74, TEST_DATA + 0x500)
                cpu.write16(TEST_DATA + 0x500, 12)
                cpu.write16(TEST_DATA + 0x502, 16)
            if case.variant == 14:
                if case.objective_line_type is None:
                    lines = bytes((16, 0, 20, 0, 0, 0, 0, 1,
                                   24, 0, 30, 0, 0, 0, 2, 1,
                                   32, 0, 40, 0, 0, 0, 1, 2,
                                   0, 0, 0, 0, 0, 0, 0, 0xFF))
                else:
                    lines = bytes((16, 0, 20, 0, 0, 0,
                                   case.objective_line_type, 1,
                                   0, 0, 0, 0, 0, 0, 0, 0xFF))
                cpu.write_bytes(0x8013AEA4, lines)
        cpu.write_bytes(script, program)
        cpu.write32(UI + 0x20, script)
        if case.variant != 9:
            cpu.write32(UI + 0x44, 0)
        if case.script_flags is not None:
            cpu.write32(UI + 0x58, case.script_flags)
        if case.script_tick is not None:
            cpu.write32(UI + 0x5C, case.script_tick)
    elif name in ("StageStartUi_DrawWipeA", "StageStartUi_DrawWipeB"):
        if name.endswith("WipeA") and case.label == "kind ten opening setup":
            cpu.load_area((ROOT / "extract/members/026.bin").read_bytes(), AREA_BASE)
        if case.wipe_mode is not None:
            cpu.write32(0x8013AE60, case.wipe_mode)
        elif case.variant < 4:
            cpu.write32(0x8013AE60, case.variant)
        else:
            cpu.write32(0x8013AE60, 0)
        timer = case.wipe_a_timer if name.endswith("WipeA") else case.wipe_b_timer
        default_timer = 120 if case.variant == 4 else 0
        timer_address = 0x8013B47A if name.endswith("WipeA") else 0x8013B482
        cpu.write16(timer_address, default_timer if timer is None else timer)
        pending = case.pending_wipe_a if name.endswith("WipeA") else case.pending_wipe_b
        if name.endswith("WipeA") and case.variant == 4:
            pending = 1
        cpu.write16(0x8013B478 if name.endswith("WipeA") else 0x8013B480, pending)
        default_frame = 0 if case.variant in (1, 3) else 2
        cpu.write16(UI + 0x28, default_frame if case.animation_frame is None else case.animation_frame)
        cpu.write16(UI + 0x2A, 0 if case.animation_timer is None else case.animation_timer)
        cpu.write32(UI + 0x20, TEST_DATA + 0x180)
        cpu.write16(TEST_DATA + 0x180, 0x3FC0)
        if name.endswith("WipeB") and case.label.startswith("mode 23 variant"):
            cpu.write16(MODE + 0x120, 23)
            cpu.write16(MODE + 0x124, int(case.label.rsplit(" ", 1)[1]))
    elif name == "Mode_StageStart":
        if case.mode_value is not None:
            cpu.write16(MODE + 0x120, case.mode_value)
        if case.wipe_mode is not None:
            cpu.write32(0x8013AE60, case.wipe_mode)
        if case.animation_frame is not None:
            cpu.write16(UI + 0x28, case.animation_frame)
        if case.animation_timer is not None:
            cpu.write16(UI + 0x2A, case.animation_timer)
        if case.pending_wipe_b:
            cpu.write16(0x8013B480, case.pending_wipe_b)
        if case.progress_byte_11 is not None:
            cpu.write8(0x800DFDE3, case.progress_byte_11)
    elif name == "StageStartUi_RunA":
        cpu.write32(0x8013AE60, 3 if case.wipe_mode is None else case.wipe_mode)
        cpu.write16(UI + 0x28, 0)
        cpu.write16(UI + 0x2A, 0)
    elif name == "StageStartUi_RunB":
        mode = 18 if case.variant == 18 else 10
        cpu.write16(MODE + 0x120, mode if case.mode_value is None else case.mode_value)
        cpu.write32(0x8013AE60, 0)
        cpu.write16(UI + 0x28, 0)
        cpu.write16(UI + 0x2A, 0)
        cpu.write16(0x8013B480, 0)
    elif name == "StageStartUi_AreaInit":
        bundle = 0x025 if case.area == 1 else 0x026
        cpu.load_area((ROOT / f"extract/members/{bundle:03X}.bin").read_bytes(), AREA_BASE)
    elif name in ("StageStartUi_MeshInit", "StageStartUi_SetAreaMeshTable"):
        bundle = 0x025 if case.area == 1 else 0x026
        cpu.load_area((ROOT / f"extract/members/{bundle:03X}.bin").read_bytes(), AREA_BASE)
    elif name == "StageStartUi_DrawNumber":
        number_values = {
            0: (7, 3, 5), 1: (42, 15, 99),
            2: (-1, -1, -8), 3: (123, 108, 100),
            4: (6, 8, 19), 5: (6, 8, 19)
        }
        first, second, suffix = number_values[case.variant]
        cpu.write16(0x1F800004, first if case.number_first is None else case.number_first)
        cpu.write16(0x1F800008, second if case.number_second is None else case.number_second)
        cpu.write16(0x1F80000E, suffix if case.number_suffix is None else case.number_suffix)
    elif name == "StageStartUi_DrawObjectives":
        default_mode = 18 if case.variant == 18 else 10
        cpu.write16(MODE + 0x120, default_mode if case.mode_value is None else case.mode_value)
        cpu.write16(0x1F800004, 3)
        cpu.write16(0x1F800006, 5)
        cpu.write16(0x1F800008, 8)
        cpu.write16(0x1F80000A, 10)
    elif name == "StageStartUi_DrawStageIcon":
        mode = 18 if case.variant == 18 else 10
        cpu.write16(MODE + 0x120,
                    mode if case.mode_value is None else case.mode_value)
        cpu.write32(FRAME, 7 if case.frame_timer is None else case.frame_timer)
    elif name == "StageStartUi_DrawCounter":
        cpu.write32(0x8013B6E0, 40 if case.label == "clamped" else 100)
    elif name == "StageStartUi_CopyBuffer":
        if case.frame_timer is not None:
            cpu.write32(FRAME + 0x0C, case.frame_timer)
    elif name == "StageStartUi_UpdateObjects":
        if case.script_pointer is not None:
            cpu.write32(UI + 0x20, case.script_pointer)
        if case.label != "empty objects":
            obj = case.args[0]
            model = TEST_DATA + 0x600
            anim = TEST_DATA + 0x700
            bones = TEST_DATA + 0x800
            matrices = TEST_DATA + 0x900
            cpu.write_bytes(model, bytes(0x100))
            cpu.write_bytes(anim, bytes(0x40))
            cpu.write_bytes(bones, bytes(12))
            cpu.write_bytes(matrices, bytes(32))
            flags = 1 | (2 if case.variant == 23 else 0)
            cpu.write16(obj + 0x72, flags if case.object_flags is None else case.object_flags)
            cpu.write16(obj + 0x60, 100)
            cpu.write16(obj + 0x62, 200)
            cpu.write16(obj + 0x64, 300)
            cpu.write16(obj + 0x78, 120)
            cpu.write16(obj + 0x7A, 220)
            cpu.write16(obj + 0x7C, 320)
            cpu.write16(obj + 0x7E, case.object_position_time)
            cpu.write32(obj + 0x0C, bones)
            cpu.write32(obj + 0x14, matrices)
            cpu.write32(obj + 0x18, anim)
            cpu.write32(obj + 0x74, anim)
            cpu.write16(obj + 0x5C, 0)
            cpu.write16(obj + 0x5E, 0)
            cpu.write16(obj + 0x70,
                        0xFFFF if case.object_loop_index is None
                        else case.object_loop_index)
            cpu.write16(obj + 0x86, case.object_translation_time)
            cpu.write32(obj + 0x00, model)
            cpu.write16(model + 4, 0)
            cpu.write16(model + 6, 4)
            cpu.write16(anim, 0)
            cpu.write16(anim + 2,
                        5 if case.object_end_frame is None
                        else case.object_end_frame)
            cpu.write32(UI + 0x48, 1 if case.stage_model_active else 0)
    elif name == "StageStartUi_DrawScanlines":
        frame = {"frame zero": 0, "frame 1": 1,
                 "frame 316": 316, "timer rollover": 0x100}.get(
                     case.label, case.frame_timer)
        cpu.write32(FRAME, frame)
        cpu.write32(0x8013B6E4, -1 if case.label == "timer rollover" else 12)
    elif name in ("RippleGrid_Smooth2", "RippleGrid_Draw2"):
        cpu.write32(0x8013B9F8, TEST_DATA + 0x400)
        cpu.write32(FRAME, {"frame zero": 0, "frame one": 1, "frame 255": 255, "frame wrap": 0x100}[case.label])
    if name == "StageStartUi_ClampVerts":
        mode, count, y = case.args
        for index in range(12):
            vert = 0x800E7D00 + index * 8
            cpu.write16(vert + 2, (index - 6) * 35)
    # The mode 18 branch is selected by cases whose variant carries it.
    if case.variant == 18:
        cpu.write16(MODE + 0x120, 18)


def make_cpu(area: int, name: str, case: Case, seed: int = 0x18894423) -> mr.MipsRam:
    cpu = mr.MipsRam.from_extract()
    global _GTE_LIB
    if _GTE_LIB is None:
        cpu.attach_gte_oracle()
        _GTE_LIB = cpu._gte_lib
    else:
        state = (C.c_uint32 * 64)()
        cpu.gte_state = state
        cpu._gte_lib = _GTE_LIB
        cpu.gte_backend = lambda instruction, value, state=state: _GTE_LIB.oracle_gte_step(
            state, instruction, value & M32)
        cpu.gte_backend._native_function = _GTE_LIB.oracle_gte_step
        cpu.gte_backend._native_state = state
    setup_base(cpu, area)
    setup_case(cpu, name, case)
    cpu.coverage = set()
    return cpu


def load_arc_entries_mips(cpu: mr.MipsRam, entry: int):
    cursor = entry
    for _ in range(16):
        member = cpu.read16(cursor)
        flags = cpu.read16(cursor + 2)
        destination = cpu.read32(cursor + 4)
        if flags == 0xFFFF:
            return
        if member and 0x80000000 <= destination < 0x80200000:
            path = ROOT / f"extract/members/{member:03X}.bin"
            if not path.is_file():
                raise FileNotFoundError(path)
            cpu.write_bytes(destination, path.read_bytes())
        cursor += 8
    raise ValueError(f"unterminated archive list at {entry:08X}")


def mips_hook_table(log: list[tuple], output, pad_after: int = 0,
                    press_on_wipe_kind: int = 0, load_after: int = 0,
                    case: Case | None = None):
    hooks = {}
    pad_calls = 0
    load_steps = 0
    input_sent = False
    for address, (name, argc, result) in HOOKS.items():
        def invoke(cpu, regs, address=address, name=name, argc=argc, result=result):
            nonlocal pad_calls, load_steps, input_sent
            args = tuple(regs[4 + i] & M32 for i in range(4))
            log.append((name, address, argc, args[:min(argc, 4)]))
            if address == 0x80011194 and argc:
                load_arc_entries_mips(cpu, args[0])
            if address == 0x8002B52C:
                pad_calls += 1
                if pad_after and pad_calls >= pad_after:
                    pressed = (pad_calls - pad_after) % 2 == 0
                    cpu.write8(0x800E52D2, 0xF7 if pressed else 0xFF)
                    cpu.write8(0x800E52D3, 0x9F if pressed else 0xFF)
                if press_on_wipe_kind:
                    wipe_kind = cpu.read32(0x8013AE60)
                    if wipe_kind == press_on_wipe_kind and not input_sent:
                        cpu.write8(0x800E52D2, 0xF7)
                        cpu.write8(0x800E52D3, 0x9F)
                        input_sent = True
                    elif wipe_kind != press_on_wipe_kind:
                        cpu.write8(0x800E52D2, 0xFF)
                        cpu.write8(0x800E52D3, 0xFF)
            if address == 0x80097D0C and argc >= 2:
                out = regs[5]
                fixture = case or Case("default", 1, ())
                cpu.write32(out, fixture.level_info_progress)
                cpu.write16(out + 4, fixture.level_info_required)
                cpu.write16(out + 6, 12)
                cpu.write16(out + 8, 8)
                cpu.write16(out + 10, 0)
                kind = fixture.level_info_kind
                if regs[4] == 24 and fixture.followup_info_kind is not None:
                    kind = fixture.followup_info_kind
                cpu.write16(out + 0x10, kind)
                cpu.write16(out + 0x24, fixture.level_info_total)
            if address == 0x800116C0 and load_after:
                load_steps += 1
                return -1 if load_steps >= load_after else 0
            if address == 0x80096580:
                return case.level_progress if case is not None else 0
            if address == 0x80097FB0:
                return case.caught_count if case is not None else 5
            return result
        hooks[address] = invoke
    return hooks


def c_api_log(calls):
    return [(name, address, argc, tuple(args[:min(argc, 4)]))
            for name, address, argc, args in calls]


def compare_case(side: CSide, name: str, case: Case, seed: int,
                 mutations=False, selector=0) -> tuple[bool, str, set[int]]:
    cpu = make_cpu(case.area, name, case, seed)
    if selector:
        cpu.scratch[0x3F0:0x3F4] = (selector & M32).to_bytes(4, "little")
    args = case.args
    if name == "StageStartUi_LookAt":
        # Input records are filled by setup_case after common initialization.
        args = case.args
    if name == "StageStartUi_ClampVerts":
        args = case.args
    side.sync(cpu, selector, case.pad_after, case.press_on_wipe_kind,
              case.load_after, case)
    mips_calls: list[tuple] = []
    try:
        mips_regs = cpu.call(BY_NAME[name][1], args=args,
                             hooks=mips_hook_table(mips_calls, None, case.pad_after,
                                                   case.press_on_wipe_kind,
                                                   case.load_after, case), limit=20_000_000)
    except BaseException as exc:  # report MIPS traps in the same format as a mismatch
        state = (cpu.read32(0x800E5478), cpu.read32(0x800E5480),
                 cpu.read32(0x8013AE60), cpu.read16(UI + 0x28), cpu.read16(UI + 0x2A),
                 cpu.read16(0x8013B480), cpu.read16(0x8013B482),
                 cpu.read8(0x800E52D2), cpu.read8(0x800E52D3),
                 cpu.read8(0x800DFDE3), cpu.read16(MODE + 0x126))
        return False, f"MIPS execution failed: {exc}; hooks={len(mips_calls)} state={state} tail={mips_calls[-8:]}", set(cpu.coverage or ())
    mips_ram, mips_scratch = bytes(cpu.ram), bytes(cpu.scratch)
    port_id = BY_NAME[name][3]
    c_ret, c_failed, c_error = side.run(port_id, args)
    c_ram, c_scratch = side.result()
    if c_failed:
        return False, f"C port error: {c_error}", set(cpu.coverage or ())
    ret = BY_NAME[name][4]
    mips_ret = mips_regs[2] & M32
    if ret and mips_ret != (c_ret & M32):
        return False, f"return: MIPS {mips_ret:08X}, C {c_ret & M32:08X}", set(cpu.coverage or ())
    # DR_MOVE is a GPU ordering-table primitive.  The C side replaces the
    # original packet-building blocks with semantic renderer requests; its
    # low-level DR_MOVE calls therefore belong to that native boundary.
    mips_events = [event for event in mips_calls if event[1] != 0x8002B380]
    c_events = [event for event in side.events if event[1] != 0x8002B380]
    if mips_events != c_events:
        mismatch = next((i for i, pair in enumerate(zip(mips_events, c_events))
                         if pair[0] != pair[1]),
                        min(len(mips_events), len(c_events)))
        first = max(0, mismatch - 2)
        last = mismatch + 3
        return False, (f"call sequence mismatch at event {mismatch} "
                       f"(MIPS {len(mips_events)}, C {len(c_events)})\n"
                       f"  MIPS {mips_events[first:last]}\n"
                       f"  C    {c_events[first:last]}"), set(cpu.coverage or ())
    # Every renderer request must be named and have a compact argument list.
    bad_render = [entry for entry in side.renders if entry[0] not in RENDER_ONLY]
    if bad_render:
        return False, f"unnamed renderer request(s): {bad_render[:3]}", set(cpu.coverage or ())
    # Renderer-owned packets and the stack are the two intentional comparison
    # windows. Specific spans are declared in check_exclusions.csv.
    masked = [(0, 4), (0xEC354, 8), (0xEC360, 0x80F0),
              (0x118000, 0x8000), (0x128000, 0x8000)]
    for off, length in masked:
        reg.check_ram_mask([(off, length)])
    for index, (off, length) in enumerate(masked):
        del index
        mips_ram = mips_ram[:off] + b"\x00" * length + mips_ram[off + length:]
        c_ram = c_ram[:off] + b"\x00" * length + c_ram[off + length:]
    ram_diff = first_diff(mips_ram, c_ram, STACK_LO - 0x80000000,
                          0x200000)
    scratch_diff = first_diff(mips_scratch, c_scratch, 0x400, 0x400,
                              base=0x1F800000)
    if ram_diff or scratch_diff:
        return False, f"RAM diff {ram_diff}; scratch diff {scratch_diff}", set(cpu.coverage or ())
    return True, "", set(cpu.coverage or ())


def first_diff(left: bytes, right: bytes, stop: int, end: int,
               base: int = 0x80000000):
    for i in range(end):
        if stop <= i < 0x200000:
            continue
        if left[i] != right[i]:
            return f"{base + i:08X}: {left[i]:02X}!={right[i]:02X}"
    return None


def instruction_coverage(cases_coverage: dict[str, set[int]]):
    items = [(name, address, size, True)
             for name, address, size, _dispatch, _returns, _count in FUNCS]
    total_cov = set().union(*cases_coverage.values()) if cases_coverage else set()
    return reg.coverage_summary(items, total_cov)


def mutation_source(source: str) -> str:
    import re
    out = source
    for i, (name, *_rest) in enumerate(FUNCS):
        c_name = "ae_" + name
        pattern = re.compile(r"(\b(?:void|uint32_t|int32_t)\s+" + re.escape(c_name) +
                             r"\s*\([^)]*\)\s*\{)", re.S)
        match = pattern.search(out)
        if not match:
            raise RuntimeError(f"cannot find function definition {c_name} for mutations")
        injections = []
        for variant in range(2):
            selector = 0x1880000 + i * 2 + variant + 1
            value = 0x188A0000 + i * 2 + variant + 1
            injections.append(f"\n    if (ae_rd32(ram, 0x1F8003F0u) == 0x{selector:08X}u) "
                              f"ae_wr32(ram, 0x1F8003F4u, 0x{value:08X}u);")
        out = out[:match.end()] + "".join(injections) + out[match.end():]
    return out


def run_mutations(side: CSide, source_path: Path, seed: int) -> int:
    source = source_path.read_text(encoding="utf-8")
    mutant_path = OUT / "ovl188_mutations.c"
    mutant_path.write_text(mutation_source(source), encoding="utf-8")
    mutant_dll = build_dll(mutant_path, "mutations")
    mutant_side = CSide(mutant_dll)
    caught = 0
    survivors = []
    for i, (name, *_rest) in enumerate(FUNCS):
        case = scenario_cases(name)[0]
        for variant in range(2):
            selector = 0x1880000 + i * 2 + variant + 1
            ok, message, _cov = compare_case(mutant_side, name, case,
                                              seed + i * 17 + variant,
                                              mutations=True, selector=selector)
            if ok:
                survivors.append(f"{name}#{variant + 1}")
                print(f"MUTATION SURVIVED {name} #{variant + 1}", flush=True)
            elif "scratch diff" in message:
                caught += 1
                print(f"mutation caught {name} #{variant + 1}", flush=True)
            else:
                # A baseline scenario must itself be valid before a mutation
                # can count as detected.
                survivors.append(f"{name}#{variant + 1} (fixture: {message})")
                print(f"mutation fixture failed {name} #{variant + 1}: {message}", flush=True)
    print(f"mutation results: {caught}/{len(FUNCS) * 2} caught", flush=True)
    if survivors:
        print("mutation survivors/invalid cases: " + "; ".join(survivors), flush=True)
    return caught == len(FUNCS) * 2


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=lambda x: int(x, 0), default=0x18894423)
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--only", choices=[row[0] for row in FUNCS])
    args = parser.parse_args()
    reg.declare("stack_window", f"{STACK_LO:08X}..80200000")

    source = ROOT / "src/modes/ae_ovl188.c"
    dll = build_dll()
    side = CSide(dll)
    cases_seen = {name: 0 for name, *_rest in FUNCS}
    coverage = {name: set() for name, *_rest in FUNCS}
    failures = []
    for name, _addr, _size, _id, _returns, _count in FUNCS:
        if args.only and name != args.only:
            continue
        cases = scenario_cases(name)
        for case_index, case in enumerate(cases):
            ok, message, cov = compare_case(side, name, case,
                                             args.seed + case_index * 0x1F123,
                                             selector=0)
            cases_seen[name] += 1
            coverage[name].update(cov)
            if not ok:
                failures.append(f"{name} [{case.label}]: {message}")
                print("FAIL " + failures[-1], flush=True)
            else:
                print(f"PASS {name} [{case.label}]", flush=True)
    summary = instruction_coverage(coverage)
    reg.print_coverage(summary)
    mutations_ok = True
    if args.mutate:
        mutations_ok = run_mutations(side, source, args.seed + 0x50000)
    passed = not failures and mutations_ok
    default_run = args.only is None and args.seed == 0x18894423 and not args.mutate
    if args.only is None:
        reg.write_results(cases_seen, summary, passed,
                          extra={"mutations_caught": 56 if mutations_ok and args.mutate else 0,
                                 "mutations_required": 56,
                                 "failures": failures},
                          function_names=[row[0] for row in FUNCS])
    if failures:
        print("\nFailures:\n  " + "\n  ".join(failures), flush=True)
    if not mutations_ok:
        return 1
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
