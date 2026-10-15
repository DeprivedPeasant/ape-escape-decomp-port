"""Differential checks for every function in stage-clear overlay member 187."""
import argparse
import ctypes as C
import math
import os
from pathlib import Path
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas
import exclusion_registry as reg
import mips_ram as mr

OVERLAY = ROOT / "extract/members/187.bin"
OVERLAY_BASE = 0x80136000
STACK_LO = 0x1FF000
GAME_STATE = 0x800F4350
UI = 0x80139B20
UI_STRINGS = 0x80139930
UI_LEVELS = 0x80139B00
UI_OBJECTS = 0x80139B80
UI_FLAGS = 0x8013995C
UI_FLAG_STRIDE = 8
UI_FLAG_KIND = 6
UI_FLAG_ACTIVE = 7
UI_TIME_STRINGS = 0x80139AB4
UI_CURSOR = 0x800EC354
UI_FRAME = 0x800EC350
OBJECT_COUNT = 0x800EC35C
WIPE_STATE = 0x80139540
LOAD_ACTIVE = 0x801399AC
WIPE_TICK = 0x801399AE
SCANLINE_TIMER = 0x80139AFC
SCANLINE_PHASE = 0x800AE8F0
WIPE_COUNTER = 0x80139B4A
ACTION_TABLE = 0x801394D8
TEXT_HANDLER_TABLE = 0x80139528
MODE_TABLE = 0x80139478
LEVEL_TABLE = 0x801D4000
OT_TEST_BASE = 0x801D0000
ALLOC_TEST_BASE = 0x801A0000
SCRATCH_TEST_BASE = 0x801D8000

FUNCS = {
    "StageClearUi_LookAt": (0x80136000, 472, "u", "uu"),
    "StageClearUi_StringTableIndex": (0x801361D8, 48, "u", "us"),
    "Mode_StageClear": (0x80136208, 1780, "s", ""),
    "StageClearUi_DrawWipe": (0x801368FC, 2516, "s", ""),
    "StageClearUi_DrawText": (0x801372D0, 296, "v", "u"),
    "StageClearUi_DrawMessage": (0x801373F8, 64, "v", "u"),
    "StageClearUi_RunScript": (0x801374C8, 2016, "v", ""),
    "StageClearUi_SetDrawStp": (0x80137CA8, 120, "v", ""),
    "StageClearUi_CopyStrip": (0x80137D20, 164, "v", "s"),
    "StageClearUi_DrawBlinkIcons": (0x80137DC4, 392, "v", ""),
    "StageClearUi_UpdateObjects": (0x80137F4C, 780, "v", "u"),
    "StageClearUi_DrawBox": (0x80138258, 276, "v", "ssss"),
    "StageClearUi_DrawBackdrop": (0x8013836C, 308, "v", ""),
    "StageClearUi_DrawTimes": (0x801384A0, 448, "v", ""),
    "StageClearUi_PlayXa": (0x80138660, 88, "s", ""),
    "StageClearUi_DrawScanlines": (0x801386B8, 996, "v", ""),
    "StageClearUi_CheckRequirements": (0x80138A9C, 112, "v", ""),
    "RippleGrid_Init": (0x80138B0C, 280, "u", ""),
    "RippleGrid_Smooth": (0x80138C24, 508, "v", ""),
    "RippleGrid_Draw": (0x80138E20, 280, "v", ""),
    "StageClearUi_Setup": (0x80138F38, 888, "v", ""),
    "StageClearUi_ObjMatrix": (0x801392B0, 236, "u", "su"),
    "StageClearUi_DrawStars": (0x8013939C, 208, "v", ""),
}

# Cases are per original function. The default scale runs every listed case;
# --cases N scales this table for quick local iterations.
RUNSCRIPT_CASES = [
    (0x3FC0, (), "end"),
]
for parameter in range(20):
    if parameter in (16, 17):
        payload = (0x100, 0x200, 0x300)
    elif parameter < 5 or parameter in (18, 19):
        payload = (0x100, 0x200, 0x300, 0x20)
    else:
        payload = ()
    RUNSCRIPT_CASES.append((0x0040 | parameter, payload, "action"))
RUNSCRIPT_CASES.extend([
    (0x0000, (1,), "opcode"),
    (0x0080, (10, 20, 30, 4), "opcode"),
    (0x00C0, (0,), "opcode"),
    (0x0100, (), "opcode"),
    (0x0140, (1,), "opcode"),
    (0x0180, (1,), "opcode"),
    (0x01C0, (1,), "opcode"),
    (0x0200, (), "opcode"),
    (0x0240, (), "opcode"),
    (0x0280, (), "restore"),
    (0x02C0, (2,), "opcode"),
    (0x0300, (), "opcode"),
    (0x0380, (1,), "wait_tick"),
    (0x0380, (0,), "opcode"),
    (0x03C0, (), "opcode"),
    (0x0400, (), "opcode"),
])
RUNSCRIPT_CASES.extend((0x3FC0, (), f"icon_{kind}") for kind in range(1, 6))
RUNSCRIPT_CASES.extend([
    (0x0200, (), "wait_count"),
    (0x0440, (), "opcode"),
    (0x3FC0, (), "icon_0"),
    (0x3FC0, (), "icon_5_times"),
])
RUNSCRIPT_CASES.extend([
    (0x3FC0, (), "ticks_65534"),
    (0x3FC0, (), "ticks_65535"),
])

DEFAULT_CASES = {
    "StageClearUi_LookAt": 2,
    "StageClearUi_StringTableIndex": 3,
    "Mode_StageClear": 10,
    "StageClearUi_DrawWipe": 19,
    "StageClearUi_DrawText": 3,
    "StageClearUi_DrawMessage": 1,
    "StageClearUi_RunScript": len(RUNSCRIPT_CASES),
    "StageClearUi_SetDrawStp": 1,
    "StageClearUi_CopyStrip": 1,
    "StageClearUi_DrawBlinkIcons": 2,
    "StageClearUi_UpdateObjects": 2,
    "StageClearUi_DrawBox": 1,
    "StageClearUi_DrawBackdrop": 4,
    "StageClearUi_DrawTimes": 6,
    "StageClearUi_PlayXa": 2,
    "StageClearUi_DrawScanlines": 3,
    "StageClearUi_CheckRequirements": 1,
    "RippleGrid_Init": 1,
    "RippleGrid_Smooth": 1,
    "RippleGrid_Draw": 1,
    "StageClearUi_Setup": 16,
    "StageClearUi_ObjMatrix": 2,
    "StageClearUi_DrawStars": 4,
}

# All other original code reached from the overlay is either intercepted as a
# named game/SDK call or has its own verified checker. Keep coverage scoped to
# the overlay member functions listed above.
EXTRA_COVERAGE = {}

# Original calls outside this overlay are intercepted on the MIPS side. Direct
# C calls to already-verified ports use checker-only adapters; the production
# module still calls those ports directly.
HOOKS = {
    0x8002FF50: ("CompMatrix (SDK)", 3),
    0x8003019C: ("VectorNormalS (SDK)", 2),
    0x800301B0: ("VectorNormal (SDK)", 2),
    0x800301E0: ("VectorNormalSS (SDK)", 2),
    0x800303C0: ("SquareRoot0 (SDK)", 1),
    0x80010CC0: ("AreaGetLevel", 1),
    0x80011194: ("ArcLoadEntry", 1),
    0x800116C0: ("LoadListStep", 0),
    0x80011F6C: ("XaPlay", 1),
    0x8001231C: ("CdMgrStopStream", 0),
    0x800125B8: ("CdMgrUpdate", 0),
    0x8001325C: ("PadUpdate", 1),
    0x80014750: ("MatrixRotYMul", 2),
    0x80015AC0: ("ModelInit", 2),
    0x80015C34: ("MeshInit", 2),
    0x80015D58: ("SetColorMatrixMode", 4),
    0x80015780: ("ModelPoseBones", 2),
    0x80016D60: ("MeshCalcEnvUVs", 1),
    0x80016F34: ("DrawAreaProps", 0),
    0x800161C0: ("ModelTransformRigid", 3),
    0x80016A60: ("ModelTransformSkinned", 1),
    0x80017CD4: ("ModelDraw", 3),
    0x8001CE54: ("ModelDrawEnvMap", 3),
    0x80028350: ("ResetGraph", 1),
    0x800286CC: ("ContextClear", 1),
    0x80028850: ("ContextSetup", 4),
    0x80028CC0: ("RenderSetDrawArea", 1),
    0x80028D30: ("RenderSetDrawOffset", 1),
    0x80028EFC: ("RenderClearOT", 1),
    0x8002B380: ("DrawMove", 4),
    0x8002B3E0: ("SetDrawStp", 2),
    0x8002FAF0: ("SetGeomScreen", 1),
    0x80030450: ("SetRotMatrix", 1),
    0x800305B0: ("StoreMatrix", 2),
    0x8002E520: ("VSync", 1),
    0x8005CFEC: ("CutsceneModeOff", 0),
    0x8005DF48: ("BeginFrame", 0),
    0x8005F68C: ("HudDrawTimeRankIcon", 4),
    0x8005F978: ("HudDrawTime", 3),
    0x80086170: ("SoundRestoreVolume", 0),
    0x80086FD0: ("SoundStop", 1),
    0x800877F8: ("PlaySfx", 1),
    0x8008C01C: ("HeapInit", 0),
    0x8008C09C: ("HeapAlloc", 1),
    0x8008DDB0: ("TextSetCursor", 2),
    0x8008DDC8: ("MessageBoxDrawString", 1),
    0x80043380: ("AreaSetZoneBoxes", 1),
    0x800963AC: ("ProgressGetByte", 1),
    0x800964B0: ("ProgressGetStageResult", 1),
    0x80096568: ("ProgressBitsGet", 1),
    0x80096580: ("LevelProgressGet", 1),
    0x80097E60: ("LevelTableGet", 1),
}

RENDER_ONLY = {
    "DrawMove", "SetDrawStp", "RenderSetDrawArea", "RenderSetDrawOffset",
    "RenderClearOT"
}


def build(source=None, tag="ovl187"):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{os.getpid()}_{time.time_ns()}.dll"
    subprocess.run([
        shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections", "-shared",
        "-static-libgcc", "-I" + str(ROOT / "src/core"),
        "-I" + str(ROOT / "src/world"), "-I" + str(ROOT / "src/player"),
        "-I" + str(ROOT / "src/ui"), "-I" + str(ROOT / "src/sound"),
        str(ROOT / "src/core/ae_ram.c"), str(ROOT / "src/core/ae_sdk.c"),
        str(ROOT / "src/core/ae_sdk_matrix.c"), str(source or ROOT / "src/modes/ae_ovl187.c"),
        str(ROOT / "extract/codex-task/ovl187_testwrap.c"),
        str(ROOT / "extract/codex-task/ovl187_teststubs.c"), "-o", str(dll),
    ], cwd=ROOT, check=True)
    return dll


class CMemory:
    def __init__(self, side):
        self.side = side

    def read(self, address, size):
        return int.from_bytes(bytes(self.side.main[address - 0x80000000:
                                                   address - 0x80000000 + size]), "little")

    def write(self, address, value, size):
        off = address - 0x80000000
        self.side.main[off:off + size] = (value & ((1 << (size * 8)) - 1)).to_bytes(size, "little")


class HookModel:
    def __init__(self, memory, context=None):
        self.memory = memory
        self.context = context or {}
        self.events = []
        self.render_events = []
        self.allocation_cursor = int(self.context.get("allocation", ALLOC_TEST_BASE))

    def call(self, name, args):
        args = tuple(x & 0xFFFFFFFF for x in args)
        event = (name, args)
        if (name.startswith("RenderStageClear") or name == "RenderRippleGrid"
                or name in RENDER_ONLY):
            self.render_events.append(event)
        elif name not in RENDER_ONLY:
            self.events.append(event)

        if name == "BeginFrame":
            if hasattr(self.memory, "side"):
                self.memory.write(UI_CURSOR, OT_TEST_BASE, 4)
            else:
                self.memory.write(UI_CURSOR, 4, OT_TEST_BASE)

        if name == "LoadListStep": return int(self.context.get("load_result", -1))
        if name == "HeapAlloc":
            address = self.allocation_cursor
            size = max(16, (args[0] + 15) & ~15)
            self.allocation_cursor += size
            return address
        if name == "AreaGetLevel": return int(self.context.get("area_level", 1))
        if name == "LevelProgressGet":
            return int(self.context.get("progress", {}).get(args[0], self.context.get("progress_default", 2)))
        if name == "ProgressGetByte":
            return int(self.context.get("progress_bytes", {}).get(args[0], 5))
        if name == "LevelTableGet": return int(self.context.get("level_table", LEVEL_TABLE))
        if name == "ProgressGetStageResult":
            return int(self.context.get("stage_results", {}).get(args[0], 0x19))
        if name == "ProgressBitsGet":
            return int(self.context.get("progress_bits", {}).get(args[0], 1))
        if name in ("RenderStageClearBox", "RenderStageClearBackdrop",
                    "RenderStageClearBlinkIcons", "RenderStageClearScanlines",
                    "RenderStageClearSetDrawStp", "RenderStageClearStrip",
                    "RenderStageClearText", "RenderStageClearWipeCurtain",
                    "RenderStageClearModeTransition", "RenderStageClearAreaModelSetup",
                    "RenderRippleGrid"):
            if "render_return" in self.context:
                return int(self.context["render_return"])
            if name == "RenderStageClearText": return args[2]
            if name == "RenderStageClearSetDrawStp": return args[0] + 12
            if name == "RenderStageClearStrip": return args[1] + 24
            if name == "RenderStageClearBlinkIcons":
                flags = args[2] & 0xFFFF
                flags = flags - 0x10000 if flags & 0x8000 else flags
                return args[0] + (max(0, flags) + 1) * 20
            if name == "RenderStageClearBox":
                width, height = (args[2], args[3])
                if width <= 0 or height <= 0: return args[4] + 8
                return args[4] + ((width + 7) // 8) * ((height + 7) // 8) * 16 + 8
            if name == "RenderStageClearBackdrop": return args[0] + 24
            if name == "RenderRippleGrid": return args[1] + 36 * 16 * 11
            if name == "RenderStageClearScanlines": return args[6]
            if name == "RenderStageClearWipeCurtain": return args[0] + 80
            return 0
        return int(self.context.get("returns", {}).get(name, 0))


class CSide:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        P, U = C.c_void_p, C.c_uint32
        self.lib.o187_ram_create.restype = P
        self.lib.o187_ram_destroy.argtypes = [P]
        self.lib.o187_ram_ptr.argtypes = [P, U, U]
        self.lib.o187_ram_ptr.restype = P
        self.lib.o187_ram_failed.argtypes = [P]
        self.lib.o187_ram_failed.restype = C.c_int
        self.lib.o187_ram_error.argtypes = [P]
        self.lib.o187_ram_error.restype = C.c_char_p
        self.lib.o187_ram_clear_error.argtypes = [P]
        self.lib.o187_test_sdk_compmatrix.argtypes = [P, U, U, U]
        self.lib.o187_test_sdk_compmatrix.restype = U
        for name in ("o187_test_sdk_vector_normal",
                     "o187_test_sdk_vector_normal_s",
                     "o187_test_sdk_vector_normal_ss"):
            fn = getattr(self.lib, name)
            fn.argtypes = [P, U, U]
            fn.restype = C.c_int32
        self.lib.o187_test_sdk_squareroot0.argtypes = [P, C.c_int32]
        self.lib.o187_test_sdk_squareroot0.restype = C.c_int32
        self.ram = self.lib.o187_ram_create()
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(
            self.lib.o187_ram_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(
            self.lib.o187_ram_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.CallFn = C.CFUNCTYPE(C.c_int32, P, P, C.c_char_p, U, U,
                                  C.POINTER(U))
        self.Hooks = type("AeOvl187Hooks", (C.Structure,), {
            "_fields_": [("user", P), ("stack_pointer", U), ("call", self.CallFn)] +
                        [(name, P) for name in ("pad", "area", "items", "misc",
                                                "stage", "stage_fade", "player1",
                                                "hud", "sound")]
        })
        self.hooks = self.Hooks()
        self.callbacks = []
        self.model = None
        self.lib.o187_call.argtypes = [P, C.POINTER(self.Hooks), U, U, U, U, U]
        self.lib.o187_call.restype = U

    def sync(self, cpu):
        C.memmove(self.main, bytes(cpu.ram), mr.RAM_SIZE)
        C.memmove(self.scratch, bytes(cpu.scratch), mr.SCRATCH_SIZE)
        self.lib.o187_ram_clear_error(self.ram)

    def configure(self, model):
        self.model = model
        callback = self.CallFn(self._call)
        self.callbacks = [callback]
        self.hooks = self.Hooks()
        self.hooks.stack_pointer = mr.DEFAULT_SP
        self.hooks.call = callback

    def _call(self, _user, _ram, name, address, argc, argv):
        decoded = name.decode("ascii")
        args = tuple(argv[i] for i in range(argc)) if argc else ()
        return self.model.call(decoded, args)

    def invoke(self, name, args):
        address, _size, ret, _arg_types = FUNCS[name]
        fn = self.lib.o187_call(self.ram, C.byref(self.hooks), address,
                                *(list(args) + [0] * (4 - len(args))))
        if self.lib.o187_ram_failed(self.ram):
            message = self.lib.o187_ram_error(self.ram).decode(errors="replace")
            raise AssertionError("C AeRam failure: " + message)
        return int(fn) if ret != "v" else None


def seed_u16(cpu, address, values):
    for index, value in enumerate(values): cpu.write16(address + index * 2, value)


def seed_u32(cpu, address, values):
    for index, value in enumerate(values): cpu.write32(address + index * 4, value)


def base_setup(cpu, area_id, member, name, seed=187):
    cpu.load_area(Path(ROOT / f"extract/members/{member:03X}.bin").read_bytes(), addr=0x80154000)
    cpu.load_area(OVERLAY.read_bytes(), addr=OVERLAY_BASE)
    cpu.write32(UI_CURSOR, OT_TEST_BASE)
    cpu.write32(UI + 0x08, 0x800F03D0)
    cpu.write32(0x800EC358, 0x800F03D0)
    cpu.write32(UI_FRAME, 7)
    cpu.write32(OBJECT_COUNT, 0)
    cpu.write32(GAME_STATE + 0x120, 12)
    cpu.write32(GAME_STATE + 0x124, 6)
    cpu.write16(GAME_STATE + 0x126, area_id)
    cpu.write16(GAME_STATE + 0x140, 500)
    cpu.write16(GAME_STATE + 0x144, 800)
    cpu.write16(GAME_STATE + 0x166, 0xFFFF)
    cpu.write32(UI + 0x10, 0x10002000)
    cpu.write32(UI + 0x18, 0x10002000)
    cpu.write16(UI + 0x28, 0)
    cpu.write16(UI + 0x2A, 0)
    cpu.write16(UI + 0x2C, 0)
    cpu.write32(UI + 0x30, 0)
    cpu.write32(UI + 0x58, 0)
    cpu.write32(UI + 0x5C, 0)
    cpu.write_bytes(UI_FLAGS, bytes(0x7F) + bytes([0xFF]))

    ptr = SCRATCH_TEST_BASE
    if name == "StageClearUi_LookAt":
        seed_u16(cpu, ptr, [10, 20, -30, 40, 60, 80])
        return (ptr, ptr + 6), {"area_level": 1}
    if name == "StageClearUi_StringTableIndex":
        cpu.write_bytes(ptr, b"first\0second\0third\0")
        return (ptr, 2), {}
    if name == "StageClearUi_DrawText":
        cpu.write_bytes(ptr, b"APE\0")
        cpu.write16(UI, 24)
        cpu.write16(UI + 2, 64)
        cpu.write32(UI + 0x0C, 32)
        for c in b"APE": cpu.write8(0x801399B0 + c, 8)
        return (ptr,), {}
    if name == "StageClearUi_DrawMessage":
        cpu.write_bytes(ptr, b"RESULT\0")
        cpu.write16(UI, 24); cpu.write16(UI + 2, 64)
        return (ptr,), {}
    if name == "StageClearUi_RunScript":
        cpu.write16(ptr, 0x3FC0)
        cpu.write32(UI + 0x20, ptr)
        cpu.write8(UI_FLAGS + 0x7F, 0xFF)
        return (), {}
    if name == "StageClearUi_SetDrawStp": return (), {}
    if name == "StageClearUi_CopyStrip": return (-2,), {}
    if name == "StageClearUi_DrawBlinkIcons":
        cpu.write16(GAME_STATE + 0x166, 0xFFFF)
        cpu.write32(UI + 0x48, 0x12345678)
        return (), {}
    if name == "StageClearUi_UpdateObjects":
        objects = UI_OBJECTS
        for i in range(5):
            obj = objects + i * 0x88
            cpu.write16(obj + 0x72, 1)
            cpu.write32(obj + 0x14, ptr + 0x100 + i * 0x40)
            cpu.write16(obj + 0x60, 0); cpu.write16(obj + 0x62, 0)
            cpu.write16(obj + 0x64, 0x1000)
            cpu.write32(obj + 0x74, ptr + 0x200)
            cpu.write16(obj + 0x5C, 0); cpu.write16(obj + 0x5E, 0)
            cpu.write16(obj + 0x70, 0)
            cpu.write16(ptr + 0x200, 0); cpu.write16(ptr + 0x202, 20)
            cpu.write16(ptr + 0x204, 0); cpu.write16(ptr + 0x206, 40)
        for i in range(5):
            record = 0x800F4C30 + i * 16
            cpu.write32(record, ptr + 0x500 + i * 0x100)
            cpu.write16(ptr + 0x500 + i * 0x100 + 10, 0)
        return (objects,), {}
    if name == "StageClearUi_DrawBox": return (12, 20, 32, 24), {}
    if name == "StageClearUi_DrawBackdrop": return (), {"progress": {6: 2}}
    if name == "StageClearUi_DrawTimes":
        cpu.write_bytes(UI_TIME_STRINGS, b"TIME\0BEST\0NEW RECORD\0GOLD\0BRONZE\0SILVER\0")
        seed_u16(cpu, LEVEL_TABLE, [400, 600, 900, 0, 0, 0, 0, 0])
        return (), {"level_table": LEVEL_TABLE}
    if name == "StageClearUi_PlayXa":
        cpu.write16(GAME_STATE + 0x126, 1)
        cpu.write32(0x80139B6C, 4)
        return (), {}
    if name == "StageClearUi_DrawScanlines": return (), {}
    if name == "StageClearUi_CheckRequirements":
        cpu.write_bytes(0x80139B00, bytes([1, 2, 3, 0]))
        return (), {"progress": {1: 2, 2: 2, 3: 1}}
    if name == "RippleGrid_Init": return (), {"allocation": ALLOC_TEST_BASE}
    if name == "RippleGrid_Smooth":
        cpu.write32(0x80139E28, ptr)
        rng = random.Random(seed)
        for i in range(16 * 12 * 8): cpu.write8(ptr + i, rng.randrange(256))
        return (), {}
    if name == "RippleGrid_Draw":
        cpu.write32(0x80139E28, ptr)
        for i in range(16 * 12 * 8): cpu.write8(ptr + i, i & 0xFF)
        return (), {}
    if name == "StageClearUi_Setup":
        cpu.write16(GAME_STATE + 0x120, 12)
        return (), {"progress": {0x11: 3, 6: 2}, "progress_bytes": {4: 5, 5: 4}}
    if name == "StageClearUi_ObjMatrix":
        cpu.write16(ptr + 4, 10); cpu.write16(ptr + 6, 20); cpu.write16(ptr + 8, 30)
        cpu.write8(ptr + 0x15, 4)
        record = 0x800F4C30 + 13 * 16
        cpu.write32(record, ptr + 0x500)
        cpu.write16(ptr + 0x500 + 10, 1)
        return (13, ptr), {}
    if name == "StageClearUi_DrawStars":
        cpu.write16(GAME_STATE + 0x126, 19)
        record = 0x800F4C30 + 13 * 16
        cpu.write32(record, ptr + 0x500)
        cpu.write16(ptr + 0x500 + 10, 1)
        return (), {"stage_results": {19: 0x19}}
    if name == "StageClearUi_DrawWipe":
        cpu.write16(WIPE_STATE, 1)
        cpu.write16(UI + 0x28, 0)
        cpu.write16(UI + 0x2A, 0)
        return (), {}
    if name == "Mode_StageClear":
        # Exercise the full entry/loop/exit path against real area records.
        # The script fixture terminates each mode path deterministically.
        cpu.write16(GAME_STATE + 0x126, 0xFF)
        cpu.write16(GAME_STATE + 0x120, 13)
        cpu.write32(UI_CURSOR, OT_TEST_BASE)
        cpu.write16(0x800E5478, 0x0860)
        cpu.write16(0x800E5480, 0x0860)
        return (), {"allocation": ALLOC_TEST_BASE, "progress": {0x11: 2}}
    raise AssertionError(name)


def scenario_area(name, case):
    if name == "Mode_StageClear":
        return (1, 19, 19, 1, 2, 1, 37, 19, 19, 19)[case % 10]
    if name == "StageClearUi_DrawStars":
        return (19, 36, 1, 19)[case % 4]
    if name == "StageClearUi_Setup":
        return 19 if case in (4, 5, 6, 7, 8, 10, 11, 12) else 1
    if name == "StageClearUi_PlayXa" and case == 1:
        return 255
    if name == "StageClearUi_DrawWipe" and case >= 6:
        if case == 16:
            return 1
        return (1, 19, 19, 2)[(case - 6) % 4]
    return None


def _script(cpu, words):
    ptr = SCRATCH_TEST_BASE
    for index, word in enumerate((*words, 0x3FC0)):
        cpu.write16(ptr + index * 2, word)
    cpu.write32(UI + 0x20, ptr)
    return ptr


def scenario_setup(cpu, name, case, args, context, seed=187):
    ptr = SCRATCH_TEST_BASE
    if name == "StageClearUi_LookAt" and case == 1:
        cpu.write16(args[1], cpu.read16(args[0]))
        cpu.write16(args[1] + 4, cpu.read16(args[0] + 4))
    elif name == "StageClearUi_StringTableIndex":
        return (args[0], (0, -1, 3)[case]), context
    elif name == "StageClearUi_DrawScanlines":
        cpu.write32(SCANLINE_TIMER, (5, 11, 0)[case])
        cpu.write16(SCANLINE_PHASE, (0x1234, 0xBEEF, 0x55AA)[case])
        if case == 1:
            cpu.write32(UI_FRAME, 280)
    elif name == "Mode_StageClear":
        cases = [
            (255, 13, 10, 1, {0xFF: 0x19}, {}),
            (19, 12, 6, 0, {19: 0x18}, {}),
            (19, 12, 13, 0, {19: 0x19}, {}),
            (1, 20, 17, 0, {}, {4: 0}),
            (2, 20, 24, 0, {}, {1: 0}),
            (1, 25, 7, 0, {}, {}),
            (37, 12, 6, 0, {19: 0x19}, {}),
            (19, 12, 13, 0, {19: 0x18, 36: 0x19}, {}),
            (19, 12, 24, 0, {}, {4: 0}),
            (19, 12, 17, 0, {}, {1: 0}),
        ]
        area_id, mode, level, override, results, bits = cases[case % len(cases)]
        cpu.write16(GAME_STATE + 0x126, area_id)
        cpu.write16(GAME_STATE + 0x120, mode)
        cpu.write16(GAME_STATE + 0x124, level)
        cpu.write16(GAME_STATE + 0x12C, 8 if case == 5 else 2)
        cpu.write16(WIPE_TICK, 119)
        cpu.write16(LOAD_ACTIVE, 0)
        cpu.write16(0x80139542, override)
        cpu.write16(0x800E5478, 0x0860)
        cpu.write16(0x800E5480, 0x0860)
        seed_u16(cpu, UI_LEVELS, [1, 0])
        seed_u16(cpu, LEVEL_TABLE, [400, 600, 900, 0, 0, 0, 0, 0])
        if case == 2:
            results = {19: 0x19, 36: 0x18}
        context["stage_results"] = results
        context["progress_bits"] = bits
        context["progress"] = {1: 2 if case == 0 else 0,
                                0x11: 2, 6: 3, 13: 3, 17: 3, 24: 3}
        context["progress_bytes"] = {4: 5, 5: 5}
        for script_address in (0x8013959C, 0x80139628, 0x80139774,
                               0x80139844, 0x801398BC):
            cpu.write16(script_address, 0x3FC0)
        if case == 7:
            cpu.write16(0x80139628, 0x01C0)
            cpu.write16(0x8013962A, 2)
            cpu.write16(0x8013962C, 0x0200)
            cpu.write16(0x8013962E, 0x3FC0)
            cpu.write32(0x800E5478, 0)
            cpu.write32(0x800E5480, 0)
    elif name == "StageClearUi_DrawWipe":
        if case in (0, 1):
            cpu.write16(WIPE_STATE, 1)
            cpu.write16(UI + 0x2A, 0 if case == 0 else 10)
            if case == 0:
                cpu.write16(WIPE_COUNTER, 4)
                cpu.write16(UI + 0x2C, 1)
        elif case in (2, 3):
            cpu.write16(WIPE_STATE, 2)
            cpu.write16(GAME_STATE + 0x120, 13 if case == 2 else 25)
            cpu.write16(0x800E5478, 0)
            cpu.write16(0x800E5480, 0)
            if case == 2:
                _script(cpu, (0x0380, 1))
                cpu.write32(UI + 0x5C, 0)
            else:
                _script(cpu, ())
                cpu.write32(0x80139E28, ptr + 0x400)
                for index in range(16 * 12 * 8):
                    cpu.write8(ptr + 0x400 + index, index & 0xFF)
        elif case in (4, 5):
            cpu.write16(WIPE_STATE, 3)
            cpu.write16(UI + 0x2A, 1 if case == 4 else 0)
            if case == 5:
                cpu.write16(WIPE_COUNTER, 6)
            cpu.write16(UI + 0x2C, 1)
        elif case == 6:
            cpu.write16(WIPE_STATE, 0)
            cpu.write16(LOAD_ACTIVE, 1)
            cpu.write16(WIPE_TICK, 0)
            context["load_result"] = 0
        elif case in (7, 8, 9):
            cpu.write16(WIPE_STATE, 0)
            cpu.write16(LOAD_ACTIVE, 1 if case == 7 else 0)
            cpu.write16(WIPE_TICK, 119)
            cpu.write16(GAME_STATE + 0x120, (12, 20, 25)[case - 7])
            cpu.write16(GAME_STATE + 0x12C, 8 if case == 9 else 2)
            _script(cpu, ())
            context.update({"load_result": -1, "progress": {0x11: 2, 6: 3},
                            "progress_bytes": {4: 5, 5: 5}})
        elif case in (10, 11):
            cpu.write16(WIPE_STATE, 4 if case == 10 else -1)
        elif case in (12, 13, 14, 15):
            mode, level, progress = ((13, 6, 3), (22, 6, 3),
                                     (12, 6, 1), (25, 6, 3))[case - 12]
            cpu.write16(WIPE_STATE, 0)
            cpu.write16(LOAD_ACTIVE, 0)
            cpu.write16(WIPE_TICK, 119)
            cpu.write16(GAME_STATE + 0x120, mode)
            cpu.write16(GAME_STATE + 0x124, level)
            cpu.write16(GAME_STATE + 0x12C, 2)
            _script(cpu, ())
            context.update({"load_result": -1, "progress": {0x11: 2, 6: progress},
                            "progress_bytes": {4: 5, 5: 5}})
        elif case == 16:
            cpu.write16(WIPE_STATE, 0)
            cpu.write16(LOAD_ACTIVE, 0)
            cpu.write16(WIPE_TICK, 119)
            cpu.write16(GAME_STATE + 0x120, 12)
            cpu.write16(GAME_STATE + 0x126, 255)
            _script(cpu, ())
            context.update({"load_result": -1, "progress": {0x11: 2, 6: 3},
                            "progress_bytes": {4: 5, 5: 5}})
        elif case in (17, 18):
            cpu.write16(WIPE_STATE, 3)
            cpu.write16(UI + 0x2A, 10 if case == 17 else 0)
            cpu.write16(UI + 0x2C, 1)
    elif name == "StageClearUi_DrawText" and case == 1:
        cpu.write16(UI + 0x0C, 0xFFFF)
        for character in b"APE": cpu.write8(0x801399B0 + character, 1)
    elif name == "StageClearUi_DrawText" and case == 2:
        cpu.write32(UI + 0x0C, 0xFFFFFFFF)
        for character in b"APE": cpu.write8(0x801399B0 + character, 1)
    elif name == "StageClearUi_DrawBlinkIcons":
        cpu.write16(GAME_STATE + 0x166, 1 if case == 0 else 7)
        cpu.write32(UI + 0x48, 0 if case == 0 else 3)
        cpu.write32(UI_FRAME, 0 if case == 0 else 8)
    elif name == "StageClearUi_UpdateObjects" and case == 1:
        for index in range(5):
            obj = UI_OBJECTS + index * 0x88
            cpu.write16(obj + 0x78, 120)
            cpu.write16(obj + 0x7A, -80)
            cpu.write16(obj + 0x7C, 40)
            cpu.write16(obj + 0x7E, 3)
            cpu.write16(obj + 0x80, 512)
            cpu.write16(obj + 0x82, -256)
            cpu.write16(obj + 0x84, 128)
            cpu.write16(obj + 0x86, 4)
            cpu.write16(obj + 0x5C, 20)
            cpu.write16(obj + 0x70, 0)
            cpu.write16(ptr + 0x200, 12)
            cpu.write16(ptr + 0x202, 8)
            cpu.write16(ptr + 0x204, 24)
            cpu.write16(ptr + 0x206, 4)
    elif name == "StageClearUi_DrawBackdrop":
        cpu.write32(0x80139AB0, 1 if case == 2 else 0)
        cpu.write16(GAME_STATE + 0x120, (12, 20, 25, 12)[case])
        context["progress"] = {6: (0, 2, 4, 2)[case]}
    elif name == "StageClearUi_DrawTimes":
        records = [
            (200, 1000, (400, 600, 900)),
            (500, 1000, (900, 300, 600)),
            (500, 1000, (900, 700, 300)),
            (500, 1000, (900, 300, 200)),
            (1100, 1200, (400, 600, 900)),
            (500, 400, (400, 600, 900)),
        ]
        current, best, thresholds = records[case]
        cpu.write32(GAME_STATE + 0x140, current)
        cpu.write32(GAME_STATE + 0x144, best)
        seed_u16(cpu, LEVEL_TABLE, (*thresholds, 0, 0, 0, 0, 0))
    elif name == "StageClearUi_PlayXa" and case == 1:
        cpu.write16(GAME_STATE + 0x126, 255)
        cpu.write32(0x80139B6C, 0)
    elif name == "StageClearUi_DrawScanlines":
        cpu.write32(0x80139AFC, (20, 5, 0)[case])
        cpu.write16(0x800AE8F0, (0x1200, 0x3300, 0x77)[case])
    elif name == "StageClearUi_Setup":
        cases = [
            (12, 6, 3, {4: 5, 5: 5}, {}, 200, 1000, (400, 600, 900), 2),
            (12, 6, 3, {4: 4, 5: 5}, {}, 200, 1000, (400, 600, 900), 2),
            (12, 6, 3, {4: 5, 5: 4}, {}, 200, 1000, (400, 600, 900), 2),
            (12, 6, 0, {4: 5, 5: 5}, {}, 200, 1000, (400, 600, 900), 2),
            (20, 6, 3, {4: 5, 5: 5}, {}, 450, 200, (300, 600, 900), 2),
            (20, 6, 3, {4: 5, 5: 5}, {}, 750, 200, (300, 600, 900), 2),
            (20, 6, 3, {4: 5, 5: 5}, {}, 950, 200, (300, 600, 900), 2),
            (20, 6, 3, {4: 5, 5: 5}, {}, 200, 250, (300, 600, 900), 2),
            (20, 6, 3, {4: 5, 5: 5}, {}, 300, 200, (300, 600, 900), 2),
            (25, 6, 3, {4: 5, 5: 5}, {}, 200, 1000, (400, 600, 900), 8),
            (20, 6, 3, {4: 5, 5: 5}, {}, 200, 1000, (300, 100, 900), 2),
            (20, 6, 3, {4: 5, 5: 5}, {}, 200, 1000, (300, 600, 100), 2),
            (20, 6, 3, {4: 5, 5: 4}, {}, 200, 1000, (300, 600, 900), 2),
            (25, 6, 3, {4: 5, 5: 5}, {}, 200, 1000, (400, 600, 900), 2),
            (13, 6, 3, {4: 5, 5: 5}, {}, 200, 1000, (400, 600, 900), 2),
            (22, 6, 3, {4: 5, 5: 5}, {}, 200, 1000, (400, 600, 900), 2),
        ]
        mode, level, progress, bytes_, bit_values, current, best, thresholds, exit_code = cases[case]
        cpu.write16(GAME_STATE + 0x120, mode)
        cpu.write16(GAME_STATE + 0x124, level)
        cpu.write16(GAME_STATE + 0x126, 19 if mode == 20 else 1)
        cpu.write16(GAME_STATE + 0x12C, exit_code)
        cpu.write32(GAME_STATE + 0x140, current)
        cpu.write32(GAME_STATE + 0x144, best)
        seed_u16(cpu, LEVEL_TABLE, (*thresholds, 0, 0, 0, 0, 0))
        context.update({"progress": {0x11: progress, level: progress},
                        "progress_bytes": bytes_, "progress_bits": bit_values})
    elif name == "StageClearUi_ObjMatrix":
        cpu.write16(ptr + 0x50A, 0 if case == 0 else 1)
    elif name == "StageClearUi_DrawStars":
        area = (19, 36, 1, 19)[case]
        cpu.write16(GAME_STATE + 0x126, area)
        context["stage_results"] = {19: 0x19, 36: (0x19 if case == 1 else 0x01)}

    if name == "StageClearUi_RunScript":
        command, payload, kind = RUNSCRIPT_CASES[case]
        words = (command, *payload)
        script = _script(cpu, words[:-1] if command == 0x3FC0 else words)
        cpu.write_bytes(UI_STRINGS, b"APE\0MESSAGE\0TIME\0BEST\0NEW RECORD\0GOLD\0BRONZE\0SILVER\0")
        for index in range(5):
            obj = UI_OBJECTS + index * 0x88
            cpu.write32(obj + 0x74, ptr + 0x200)
            cpu.write16(obj + 0x5C, 0)
            cpu.write16(obj + 0x5E, 0)
            cpu.write16(ptr + 0x200, 8)
            cpu.write16(ptr + 0x202, 20)
        if kind == "restore": cpu.write32(UI + 0x24, script + 2)
        elif kind == "wait_tick": cpu.write32(UI + 0x5C, 0)
        elif kind == "wait_count": cpu.write32(UI + 0x44, 2)
        elif kind == "ticks_65534":
            cpu.write32(UI + 0x58, 1); cpu.write32(UI + 0x5C, 65534)
        elif kind == "ticks_65535":
            cpu.write32(UI + 0x58, 1); cpu.write32(UI + 0x5C, 65535)
        elif kind.startswith("icon_"):
            icon_kind = int(kind.split("_")[1])
            is_stars = icon_kind == 5 and kind != "icon_5_times"
            cpu.write16(GAME_STATE + 0x120, 25 if is_stars else 20)
            cpu.write16(GAME_STATE + 0x126, 19)
            cpu.write16(GAME_STATE + 0x124, 6)
            cpu.write16(GAME_STATE + 0x140, 200)
            cpu.write32(GAME_STATE + 0x144, 1000)
            seed_u16(cpu, LEVEL_TABLE, [400, 600, 900, 0, 0, 0, 0, 0])
            cpu.write16(UI_FLAGS, 40 + icon_kind)
            cpu.write16(UI_FLAGS + 2, 60 + icon_kind)
            cpu.write16(UI_FLAGS + 4, 0)
            cpu.write8(UI_FLAGS + UI_FLAG_KIND, icon_kind)
            cpu.write8(UI_FLAGS + UI_FLAG_ACTIVE, 1)
            context["stage_results"] = {19: 0x19}
            context["progress"] = {6: 2}
        return (), context
    return args, context


class Harness:
    def __init__(self, source=None, tag="ovl187"):
        reg.declare("stack_window", "801FF000..80200000")
        self.dll = build(source, tag)
        self.c = CSide(self.dll)
        self.counts = {name: 0 for name in FUNCS}
        self.coverage = set()
        self.m = None
        self.failures = 0
        self.progress = False
        all_areas = dump_areas.area_members()
        selected_area_ids = [1, 2, 3, 4, 19, 36, 37]
        self.area_members = [(area, all_areas[area]) for area in selected_area_ids
                             if area in all_areas]
        self.area_to_member = all_areas
        if not self.area_members:
            raise AssertionError("no real area members found")
        gte_cpu = mr.MipsRam.from_extract()
        gte_cpu.attach_gte_oracle()
        self.gte_oracle = (gte_cpu._gte_lib, gte_cpu.gte_state,
                           gte_cpu.gte_backend)
        self.m = gte_cpu
        self.base_cpus = {}
        for _area, member in self.area_members:
            cpu = mr.MipsRam.from_extract()
            cpu.load_area(Path(ROOT / f"extract/members/{member:03X}.bin").read_bytes(), addr=0x80154000)
            cpu.load_area(OVERLAY.read_bytes(), addr=OVERLAY_BASE)
            self.base_cpus[member] = cpu.snapshot()

    def run(self, name, args=None, area=None, context=None, setup=None,
            case=0, seed=187, mips_limit=2_000_000):
        area_id, member = self.area_members[case % len(self.area_members)]
        if area is not None:
            area_id = area
            member = self.area_to_member.get(area, member)
        requested_area = scenario_area(name, case)
        if requested_area is not None:
            area_id = requested_area
            member = self.area_to_member.get(requested_area, member)
        self.m.gte_reset()
        self.m.restore(self.base_cpus[member])
        call_args, scenario_context = base_setup(self.m, area_id, member,
                                                 name, seed)
        if args is None: args = call_args
        args, scenario_context = scenario_setup(
            self.m, name, case, args, scenario_context, seed)
        if setup is not None: setup(self.m)
        if context: scenario_context.update(context)
        self.m.coverage = set()
        self.c.sync(self.m)
        c_model = HookModel(CMemory(self.c), scenario_context)
        m_model = HookModel(self.m, scenario_context)
        self.c.configure(c_model)
        c_initial_main = bytes(self.c.main)
        c_initial_scratch = bytes(self.c.scratch)

        mips_hooks = {}
        for address, (hook_name, argc) in HOOKS.items():
            def invoke(cpu, regs, n=hook_name, count=argc, model=m_model):
                values = tuple(regs[4:8])[:count]
                if count > 4:
                    values += tuple(cpu.read32(regs[29] + 16 + 4 * i)
                                    for i in range(count - 4))
                if n.endswith(" (SDK)"):
                    self.c.sync(cpu)
                    if n == "CompMatrix (SDK)":
                        result = self.c.lib.o187_test_sdk_compmatrix(
                            self.c.ram, *values)
                    elif n == "VectorNormalS (SDK)":
                        result = self.c.lib.o187_test_sdk_vector_normal_s(
                            self.c.ram, *values)
                    elif n == "VectorNormal (SDK)":
                        result = self.c.lib.o187_test_sdk_vector_normal(
                            self.c.ram, *values)
                    elif n == "VectorNormalSS (SDK)":
                        result = self.c.lib.o187_test_sdk_vector_normal_ss(
                            self.c.ram, *values)
                    else:
                        result = self.c.lib.o187_test_sdk_squareroot0(
                            self.c.ram, *values)
                    if self.c.lib.o187_ram_failed(self.c.ram):
                        message = self.c.lib.o187_ram_error(self.c.ram).decode(
                            errors="replace")
                        raise AssertionError("C SDK adapter failed: " + message)
                    cpu.ram[:] = bytes(self.c.main)
                    cpu.scratch[:] = bytes(self.c.scratch)
                    return result
                return model.call(n, values)
            mips_hooks[address] = invoke

        try:
            if self.progress:
                print("  MIPS start", flush=True)
            registers = self.m.call(FUNCS[name][0], args, hooks=mips_hooks,
                                    limit=mips_limit)
            if self.progress:
                print(f"  MIPS done ({self.m.steps} steps)", flush=True)
            self.coverage.update(self.m.coverage or ())
            self.c.main[:] = c_initial_main
            self.c.scratch[:] = c_initial_scratch
            self.c.lib.o187_ram_clear_error(self.c.ram)
            c_result = self.c.invoke(name, args)
            if self.progress:
                print("  C done", flush=True)
        except Exception as exc:
            state = getattr(self.m, "_last_machine", None)
            hook_tail = getattr(self.m, "hook_log", [])[-8:]
            raise AssertionError(
                f"{name}{tuple(args)} execution failed: {exc}; machine={state}; "
                f"hooks={hook_tail}") from exc

        self.counts[name] += 1
        ret_kind = FUNCS[name][2]
        if ret_kind != "v" and (int(c_result) & 0xFFFFFFFF) != registers[2]:
            self.fail(name, args, f"return differs C={int(c_result) & 0xFFFFFFFF:08X} MIPS={registers[2]:08X}")
        self.compare_ram(name, args)
        m_events = [event for event in m_model.events if event[0] not in RENDER_ONLY]
        c_events = [event for event in c_model.events if event[0] not in RENDER_ONLY]
        if any(event[0].startswith("RenderStageClear") or
               event[0] == "RenderRippleGrid"
               for event in c_model.render_events):
            reg.declare("ram_mask", "StageClear renderer hook request arguments")
        if any(event[0] in RENDER_ONLY
               for event in c_model.render_events + m_model.render_events):
            reg.declare("ram_mask", "low-level renderer hook arguments")
        if m_events != c_events:
            self.fail(name, args, f"hook sequence differs C={c_events!r}; MIPS={m_events!r}")
        if self.c.lib.o187_ram_failed(self.c.ram):
            self.fail(name, args, self.c.lib.o187_ram_error(self.c.ram).decode(errors="replace"))

    def compare_ram(self, name, args):
        expected = bytes(self.m.ram[:STACK_LO])
        actual = bytes(self.c.main[:STACK_LO])
        masks = [(0x800EC354 - 0x80000000, 4),
                 (OT_TEST_BASE - 0x80000000, 0x10000)]
        masks.extend((0x800F03CC - 0x80000000 + i * 0x4078, 8)
                     for i in range(32))
        masks.append((0x800F43CC - 0x80000000, 8))
        reg.check_ram_mask(masks)
        for lo, length in masks:
            expected = expected[:lo] + bytes(length) + expected[lo + length:]
            actual = actual[:lo] + bytes(length) + actual[lo + length:]
        if actual != expected:
            index = next(i for i, (a, b) in enumerate(zip(actual, expected)) if a != b)
            self.fail(name, args,
                      f"RAM differs at {0x80000000 + index:08X}: C={actual[index]:02X} MIPS={expected[index]:02X}")
        if bytes(self.c.scratch) != bytes(self.m.scratch):
            self.fail(name, args, "scratchpad differs")

    def fail(self, name, args, message):
        self.failures += 1
        raise AssertionError(f"FAIL {name}{tuple(args)}: {message}")

    def finish(self, extra_coverage=EXTRA_COVERAGE, default_run=False,
               show_coverage=False):
        items = [(name, address, size, True)
                 for name, (address, size, *_rest) in FUNCS.items()
                 if self.counts[name]]
        items.extend((name, address, size, False)
                     for name, (address, size) in extra_coverage.items())
        summary = reg.coverage_summary(items, self.coverage)
        if show_coverage: reg.print_coverage(summary)
        gaps = [(name, s["uncovered"]) for name, s in summary.items()
                if s["uncovered_count"] or s["stale_declared"]]
        if default_run:
            reg.write_results(self.counts, summary, not gaps,
                              function_names=list(FUNCS))
        if gaps and default_run:
            print("coverage gaps:", gaps)
            raise AssertionError("undeclared overlay instruction coverage gaps")
        return summary


def scenarios(h, scale=1.0, seed=187, only=None, progress=False,
              case_index=None, mips_limit=2_000_000):
    names = list(FUNCS) if only is None else [only]
    for name in names:
        count = max(1, min(DEFAULT_CASES[name],
                           math.ceil(DEFAULT_CASES[name] * scale)))
        case_indices = range(count) if case_index is None else (case_index,)
        for case in case_indices:
            if case < 0 or case >= DEFAULT_CASES[name]:
                raise ValueError(f"case index {case} out of range for {name}")
            if progress:
                print(f"checking {name} case {case + 1}/{DEFAULT_CASES[name]}", flush=True)
            h.run(name, case=case, seed=seed, mips_limit=mips_limit)


def run_mutations():
    source_path = ROOT / "src/modes/ae_ovl187.c"
    source = source_path.read_text(encoding="utf-8")
    insertions = []
    first_target = 0x801FE104
    second_target = 0x801FE108
    for index, name in enumerate(FUNCS):
        pattern = re.compile(
            rf"(?m)^(?:void|int32_t|uint32_t)\s+ae_{re.escape(name)}"
            rf"\s*\([^)]*\)\s*\{{"
        )
        matches = list(pattern.finditer(source))
        if len(matches) != 1:
            raise AssertionError(
                f"expected one definition of ae_{name}, found {len(matches)}"
            )
        selector_one = 1 + index * 2
        selector_two = selector_one + 1
        injection = (
            f"\n    if (ae_rd32(r, 0x801FE100u) == {selector_one}u) "
            f"ae_wr8(r, 0x{first_target:08X}u, 0x5Au);\n"
            f"    else if (ae_rd32(r, 0x801FE100u) == {selector_two}u) "
            f"ae_wr8(r, 0x{second_target:08X}u, 0xA5u);\n"
        )
        insertions.append((matches[0].end(), injection))

    for offset, insertion in sorted(insertions, reverse=True):
        source = source[:offset] + insertion + source[offset:]

    fd, temp_name = tempfile.mkstemp(prefix="ae_ovl187_mutations_",
                                     suffix=".c", dir=ROOT / "src/modes")
    mutant_path = Path(temp_name)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as mutant_file:
            mutant_file.write(source)
        h = Harness(source=mutant_path, tag="ovl187_mutations")
        caught = 0
        for index, name in enumerate(FUNCS):
            for selector, target in (
                (1 + index * 2, first_target),
                (2 + index * 2, second_target),
            ):
                def select_mutation(cpu, value=selector):
                    cpu.write32(0x801FE100, value)
                    cpu.write8(first_target, 0)
                    cpu.write8(second_target, 0)

                try:
                    h.run(name, case=0, setup=select_mutation)
                except AssertionError as exc:
                    expected = f"RAM differs at {target:08X}"
                    if expected not in str(exc):
                        raise AssertionError(
                            f"{name} mutation {selector} failed for the wrong "
                            f"reason: {exc}"
                        ) from exc
                    caught += 1
                else:
                    raise AssertionError(
                        f"{name} mutation {selector} was not detected"
                    )
        print(f"OK: caught {caught}/{2 * len(FUNCS)} mutations (2 per function)")
    finally:
        mutant_path.unlink(missing_ok=True)
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--cases", type=float, default=1.0)
    ap.add_argument("--seed", type=int, default=187)
    ap.add_argument("--function", choices=list(FUNCS))
    ap.add_argument("--mutate", action="store_true")
    ap.add_argument("--progress", action="store_true")
    ap.add_argument("--case-index", type=int,
                    help="run one zero-based scenario for --function")
    ap.add_argument("--mips-limit", type=int, default=2_000_000,
                    help="maximum oracle instructions for each case")
    args = ap.parse_args()
    if args.case_index is not None and args.function is None:
        ap.error("--case-index requires --function")
    if args.mutate:
        if args.function is not None or args.case_index is not None:
            ap.error("--mutate cannot be combined with --function or --case-index")
        if args.cases != 1.0 or args.seed != 187:
            ap.error("--mutate always uses the default checker scenarios")
        return run_mutations()
    h = Harness()
    h.progress = args.progress
    scenarios(h, args.cases, args.seed, args.function, args.progress,
              args.case_index, args.mips_limit)
    if args.function is None:
        h.finish(EXTRA_COVERAGE,
                 default_run=(args.cases == 1.0 and args.seed == 187),
                 show_coverage=args.coverage)
    else:
        h.finish(EXTRA_COVERAGE, show_coverage=args.coverage)
    print(" ".join(f"{n}={h.counts[n]}" for n in FUNCS))
    print(f"OK: {sum(h.counts.values())} cases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
