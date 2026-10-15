"""Differential check for the CameraUpdate RAM-image port."""
from __future__ import annotations

import argparse
import ctypes as C
import contextlib
import gc
import io
import random
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mips_ram as mr  # noqa: E402
import exclusion_registry as reg  # noqa: E402

M32 = 0xFFFFFFFF
STACK_LO = 0x801FF000
AREA_MEMBERS = tuple(ROOT / f"extract/members/{member}.bin"
                     for member in ("091", "092", "093"))
OUT = ROOT / "extract/mips"

# name -> address, bytes, return-kind, C argument-kind string
FUNCS = {
    "PadRumble": (0x80014168, 160, "v", "suu", True),
    "CopySvecAt1C": (0x800154DC, 40, "v", "uu", False),
    "Atan2Safe": (0x80046340, 60, "s", "ss", False),
    "ScreenFlashDraw": (0x80046990, 232, "v", "u", True),
    "ScreenFadeUpdate": (0x80046A78, 84, "v", "", True),
    "CameraSelectMode": (0x80046DE8, 740, "v", "", True),
    "CameraApplySlot": (0x80047FF0, 720, "v", "", False),
    "CameraShakeApply": (0x80048BBC, 340, "v", "", False),
    "CameraRegionTest": (0x8004BD98, 392, "s", "u", False),
    "EventCamLoadRecord": (0x8004D134, 616, "v", "u", False),
    "EventCamStart": (0x8004D39C, 112, "v", "u", True),
    "EventCamCreate": (0x8004BB68, 136, "s", "suu", True),
    "EventCamTrigger": (0x8004BA0C, 204, "v", "u", True),
    "StageExitRequest": (0x8004D6F4, 72, "v", "", True),
    "EventCamUpdate": (0x8004D7A0, 212, "v", "", True),
    "StageExitSequenceUpdate": (0x8004D40C, 744, "v", "", True),
    "CameraTransitionFrameDraw": (0x8004CEB4, 568, "v", "", True),
    "CameraRegionUpdate": (0x8004BF20, 964, "v", "", True),
    "CameraGetEye": (0x800470CC, 56, "v", "u", False),
    "CameraSnapshotPlayer": (0x80047DA4, 232, "v", "", False),
    "CameraUpdate": (0x80047BD0, 444, "v", "", True),
    "CameraScriptRun": (0x8004C318, 1880, "v", "u", True),
    "IsFlightArea": (0x8004A940, 56, "s", "", False),
    "CameraGetPlayerDir": (0x8004B880, 116, "v", "uuu", False),
    "CutsceneModeOn": (0x8005DD18, 16, "v", "", False),
    "CutsceneModeOff": (0x8005DD28, 12, "v", "", False),
}

HOOKS = [("PadGetState", 0x8002B52C, 1),
         ("RenderScreenFlash", 0x80046990, 1),
         ("Printf", 0x800A3720, 1),
         ("memset", 0x800A3740, 3),
         ("SpawnParticle", 0x800698B8, 2),
         ("PlayerSetMode", 0x8007B374, 1),
         ("StageIntroStart", 0x8009546C, 0),
         ("XaPlay", 0x80011F6C, 1),
         ("StageTypeClearInit", 0x80094F48, 0),
         ("CameraAreaScriptDispatch", 0x8004B590, 2),
         ("CameraLookAt", 0x80046670, 5),
         ("RenderCameraTransition", 0x8004CEB4, 4)]
HOOKFN = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                     C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
LOOKATFN = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                       C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32,
                       C.c_uint32)
CAMERA_HOOK_NAMES = [
    "Atan2Safe", "VectorNormalSS", "SquareRoot0", "GetGroundHeight", "rsin", "rcos",
    "RotTransPers", "SetRotMatrix", "SetTransMatrix", "memset", "PlayerCanExitStage",
    "PlayerIsSubmerged", "CameraRcCarActive", "CameraRcCarFollow", "IsFlightArea",
    "FUN_8013d000", "BossUpdate", "CameraAreaScriptDispatch", "CallIndexedHandler",
    "RotMatrixY", "ApplyMatrixSV", "MatrixIdentity", "CameraGetPlayerDir", "AimRequestStart",
    "AimRequestClear", "AimModeEnter", "AimModeLeave", "CameraFixedModeOff", "StageExitPending",
    "BossAimSfx", "BossKindForArea", "ScreenFadeSet", "PlaySfx", "HudDrawMonkeyInfo",
    "RadarUsable", "MonkeyFlag248Set", "CameraRadarFocusUpdate", "CameraRadarFocusSelect",
]
CAMERA_HOOKS_C = type("AeCameraHooks", (C.Structure,), {
    "_fields_": [("user", C.c_void_p)] + [(n, HOOKFN) for n in CAMERA_HOOK_NAMES]
})
CAMUPD_HOOK_NAMES = ["PadGetState", "SpawnParticle", "Printf", "memset",
                     "CameraAreaScriptDispatch", "CameraLookAt",
                     "RenderScreenFlash", "RenderCameraTransition"]
CAMUPD_HOOKS_C = type("AeCamupdHooks", (C.Structure,), {
    "_fields_": [("user", C.c_void_p)] +
                [(n, LOOKATFN if n == "CameraLookAt" else HOOKFN)
                 for n in CAMUPD_HOOK_NAMES]
                 + [("camera", CAMERA_HOOKS_C), ("stage", C.c_void_p)]
})
STAGE_HEADER = (ROOT / "src/world/ae_stage.h").read_text(encoding="utf-8")
STAGE_HOOK_BLOCK = STAGE_HEADER.split("typedef struct AeStageHooks {", 1)[1].split(
    "} AeStageHooks", 1)[0]
STAGE_HOOK_NAMES = re.findall(r"AeStageHookFn\s+(\w+)\s*;", STAGE_HOOK_BLOCK)
STAGE_HOOKS_C = type("AeStageHooks", (C.Structure,), {
    "_fields_": [("user", C.c_void_p), ("stack_pointer", C.c_uint32)] +
                [(n, HOOKFN) for n in STAGE_HOOK_NAMES] +
                [("monkey_hooks", C.c_void_p), ("gadget_hooks", C.c_void_p)]
})


def u32(value):
    return value & M32


def s32(value):
    return mr.signed(value, 32)


def build_dll(source=None, tag="camupd"):
    OUT.mkdir(parents=True, exist_ok=True)
    dll = OUT / f"{tag}_{__import__('os').getpid()}.dll"
    sources = [
        ROOT / "src/core/ae_ram.c", ROOT / "src/core/ae_sdk.c",
        ROOT / "src/core/ae_sdk_matrix.c", ROOT / "src/core/ae_pad.c",
        ROOT / "src/player/native_player.c", ROOT / "src/bodies/ae_bodies.c",
        *sorted((ROOT / "src/monkeys").glob("ae_monkey*.c")),
        ROOT / "src/player/native_gadget_slots.c",
        ROOT / "src/camera/ae_camera.c", ROOT / "src/world/ae_world.c",
        ROOT / "src/sound/ae_sound.c", ROOT / "src/world/ae_stage.c",
        ROOT / "src/player/ae_player_spawn.c",
        Path(source or ROOT / "src/camera/ae_camupd.c"),
    ]
    subprocess.run([
        shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
        "-shared", "-static-libgcc", "-I", str(ROOT / "src/camera"),
        "-I", str(ROOT / "src/player"), "-I", str(ROOT / "src/world"),
        "-I", str(ROOT / "src/sound"), "-I", str(ROOT / "src/bodies"),
        "-I", str(ROOT / "src/monkeys"),
        *map(str, sources),
        "-o", str(dll),
    ], check=True)
    return dll


class Model:
    def __init__(self, seed):
        self.seed = seed
        self.log = []
        self.pad_result = 6

    def hook(self, name, args):
        argc = next(n for hname, _addr, n in HOOKS if hname == name)
        self.log.append((name,) + tuple(args[:argc]))
        if name == "PadGetState":
            return self.pad_result
        if name == "RenderScreenFlash":
            return 0
        if name == "RenderCameraTransition":
            return 0
        if name == "CameraLookAt":
            return 0
        if name == "CameraAreaScriptDispatch":
            return 0
        if name == "Printf":
            return 0
        if name == "memset":
            return args[0]
        if name in ("SpawnParticle", "PlayerSetMode", "StageIntroStart",
                    "XaPlay", "StageTypeClearInit"):
            return 0
        raise AssertionError(name)


class CSide:
    def __init__(self, dll):
        p, u32_t, i32_t = C.c_void_p, C.c_uint32, C.c_int32
        lib = C.CDLL(str(dll))
        for name, ctype, args in (
            ("ae_ram_create", p, []), ("ae_ram_destroy", None, [p]),
            ("ae_ptr", p, [p, u32_t, u32_t]),
            ("ae_rd8", u32_t, [p, u32_t]), ("ae_rd16", u32_t, [p, u32_t]),
            ("ae_rd32", u32_t, [p, u32_t]), ("ae_rds16", i32_t, [p, u32_t]),
            ("ae_wr8", None, [p, u32_t, u32_t]), ("ae_wr16", None, [p, u32_t, u32_t]),
            ("ae_wr32", None, [p, u32_t, u32_t]), ("ae_ram_failed", i32_t, [p]),
            ("ae_ram_error", C.c_char_p, [p]), ("ae_ram_clear_error", None, [p]),
        ):
            fn = getattr(lib, name)
            fn.restype, fn.argtypes = ctype, args
        lib.ae_sdk_VectorNormalSS.restype = i32_t
        lib.ae_sdk_VectorNormalSS.argtypes = [p, u32_t, u32_t]
        lib.ae_sdk_RotMatrixZYX.restype = u32_t
        lib.ae_sdk_RotMatrixZYX.argtypes = [p, u32_t, u32_t]
        lib.ae_StageRequest.restype = i32_t
        lib.ae_StageRequest.argtypes = [p, p, i32_t, i32_t, i32_t]
        for name, (_addr, _size, ret, kinds, with_hooks) in FUNCS.items():
            fn = getattr(lib, "ae_" + name)
            fn.argtypes = [p] + ([C.POINTER(CAMUPD_HOOKS_C)] if with_hooks else []) + [
                i32_t if k in "is" else u32_t for k in kinds
            ]
            fn.restype = {"v": None, "s": i32_t, "u": u32_t}[ret]
        self.lib = lib
        self.ram = lib.ae_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(
            lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(
            lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.oracle_ram = lib.ae_ram_create()
        self.oracle_main = (C.c_uint8 * 0x200000).from_address(
            lib.ae_ptr(self.oracle_ram, 0x80000000, 0x200000))
        self.oracle_scratch = (C.c_uint8 * 0x400).from_address(
            lib.ae_ptr(self.oracle_ram, 0x1F800000, 0x400))
        self.model = None
        self.hooks = CAMUPD_HOOKS_C()
        self.callbacks = []
        for name, _addr, _argc in HOOKS:
            if name == "CameraLookAt":
                cb = LOOKATFN(lambda _user, _ram, a0, a1, a2, a3, a4, n=name:
                              s32(self.host_hook(n, _ram, [a0, a1, a2, a3, a4])))
            else:
                cb = HOOKFN(lambda _user, _ram, a0, a1, a2, a3, n=name:
                            s32(self.host_hook(n, _ram, [a0, a1, a2, a3])))
            self.callbacks.append(cb)
            if name in CAMUPD_HOOK_NAMES:
                setattr(self.hooks, name, cb)
        self.stage_hooks = STAGE_HOOKS_C()
        self.stage_hooks.stack_pointer = 0x801FFF00
        self.stage_callbacks = []
        for name, _addr, _argc in HOOKS:
            if name in STAGE_HOOK_NAMES:
                cb = HOOKFN(lambda _user, _ram, a0, a1, a2, a3, n=name:
                            s32(self.host_hook(n, _ram, [a0, a1, a2, a3])))
                self.stage_callbacks.append(cb)
                setattr(self.stage_hooks, name, cb)
        self.hooks.stage = C.cast(C.pointer(self.stage_hooks), C.c_void_p)

    def host_hook(self, name, ram, args):
        result = self.model.hook(name, args)
        if name == "memset":
            for offset in range(args[2]):
                self.lib.ae_wr8(ram, (args[0] + offset) & M32, args[1])
        return result

    def mips_hook(self, cpu, regs, name, model):
        if name == "CameraLookAt":
            args = regs[4:8] + [cpu.read32((regs[29] + 16) & M32)]
            return model.hook(name, args)
        if name == "RenderCameraTransition":
            counter = cpu.read8(0x800AF130)
            if counter == 0:
                return 0
            counter = (counter - 1) & 0xFF
            cursor = cpu.read32(0x800EC354)
            display_index = cpu.read32(0x800EC35C)
            display_record = 0x800EC350 + display_index * 0x4078
            image = cpu.read32(display_record + 0x80)
            mode = cpu.read16(display_record + 0x12)
            model.hook(name, [counter, cursor, image, mode])
            cpu.write8(0x800AF130, counter)
            cpu.write32(0x800EC354, cursor + 0x68)
            return 0
        args = regs[4:8]
        result = model.hook(name, args)
        if name == "memset":
            for offset in range(args[2]):
                cpu.write((args[0] + offset) & M32, 1, args[1])
        return result

    def sync(self, cpu):
        C.memmove(self.main, bytes(cpu.ram), 0x200000)
        C.memmove(self.scratch, bytes(cpu.scratch), 0x400)
        self.lib.ae_ram_clear_error(self.ram)

    def sdk_vector_normalize(self, cpu, source, destination):
        C.memmove(self.oracle_main, bytes(cpu.ram), 0x200000)
        C.memmove(self.oracle_scratch, bytes(cpu.scratch), 0x400)
        self.lib.ae_ram_clear_error(self.oracle_ram)
        result = self.lib.ae_sdk_VectorNormalSS(self.oracle_ram, source, destination)
        if self.lib.ae_ram_failed(self.oracle_ram):
            raise RuntimeError(self.lib.ae_ram_error(self.oracle_ram).decode())
        cpu.ram[:] = bytes(self.oracle_main)
        cpu.scratch[:] = bytes(self.oracle_scratch)
        return result

    def sdk_rot_matrix_zyx(self, cpu, angles, matrix):
        C.memmove(self.oracle_main, bytes(cpu.ram), 0x200000)
        C.memmove(self.oracle_scratch, bytes(cpu.scratch), 0x400)
        self.lib.ae_ram_clear_error(self.oracle_ram)
        result = self.lib.ae_sdk_RotMatrixZYX(self.oracle_ram, angles, matrix)
        if self.lib.ae_ram_failed(self.oracle_ram):
            raise RuntimeError(self.lib.ae_ram_error(self.oracle_ram).decode())
        cpu.ram[:] = bytes(self.oracle_main)
        cpu.scratch[:] = bytes(self.oracle_scratch)
        return result

    def call(self, name, args, model):
        self.model = model
        _address, _size, _ret, kinds, with_hooks = FUNCS[name]
        cooked = [s32(v) if k in "is" else u32(v) for v, k in zip(args, kinds)]
        call_args = [self.ram]
        if with_hooks:
            call_args.append(C.byref(self.hooks))
        return getattr(self.lib, "ae_" + name)(*call_args, *cooked)


class Harness:
    def __init__(self, dll):
        self.coverage = set()
        self.area_models = {}
        self.area_bases = {}
        for area_member in AREA_MEMBERS:
            area_model = mr.MipsRam.from_extract(area=area_member)
            area_model.coverage = self.coverage
            self.area_models[area_member] = area_model
            self.area_bases[area_member] = area_model.snapshot()
        self.m = self.area_models[AREA_MEMBERS[0]]
        self.base = self.area_bases[AREA_MEMBERS[0]]
        self.c = CSide(dll)
        self.counts = {}
        self.failures = 0
        reg.declare("stack_window", f"{STACK_LO:08X}..80200000")

    def setup(self, name, case):
        area_member = AREA_MEMBERS[case % len(AREA_MEMBERS)]
        self.m = self.area_models[area_member]
        self.base = self.area_bases[area_member]
        rng = random.Random(f"camupd/{name}/{case}")
        self.m.restore(self.base)
        self.pad_result = rng.choice([0, 1, 5, 6, 7])
        if name == "PadRumble":
            self.pad_result = 6 if case < 3 else self.pad_result
            self.m.write8(0x800F4488, 1 if case < 3 else rng.choice([0, 1]))
            self.m.write8(0x800EA788, rng.randrange(256))
            self.m.write8(0x800EA789, rng.randrange(256))
            self.m.write8(0x800EA78A, rng.randrange(256))
            self.m.write8(0x800EA78B, rng.randrange(256))
            port = [0, 1, 2][case] if case < 3 else rng.choice([-1, 0, 1, 2])
            args = [port, rng.randrange(256), rng.randrange(256)]
        elif name == "CopySvecAt1C":
            args = [0x800EC1E0, 0x801F0101]
            for i in range(0x24, 0x2C):
                self.m.write8(args[0] + i, rng.randrange(256))
        elif name == "Atan2Safe":
            args = [rng.choice([-32768, -4096, -1, 0, 1, 4096, 32767]),
                    rng.choice([-32768, -1, 0, 1, 32767])]
        elif name == "CameraGetEye":
            args = [0x801F0101]
            for i in range(8): self.m.write8(0x800F45A8 + i, rng.randrange(256))
        elif name == "CameraSnapshotPlayer":
            args = []
            for i in range(0x250): self.m.write8(0x800EC1E0 + i, rng.randrange(256))
        elif name == "ScreenFlashDraw":
            args = [rng.randrange(256)]
        elif name == "ScreenFadeUpdate":
            args = []
            self.m.write8(0x800E690A, rng.choice([0, 0, 1, 2, 255]))
            self.m.write8(0x800E690B, rng.randrange(256))
            self.m.write8(0x800E690C, rng.randrange(256))
        elif name == "CameraSelectMode":
            slots = 0x801F0000
            state = 0x800E68D0
            scenario = case if case < 12 else rng.randrange(12)
            for i in range(12 * 0xB0):
                self.m.write8(slots + i, rng.randrange(256))
            self.m.write32(0x800B4D40, slots)
            for i in range(0x250):
                self.m.write8(0x800EC1E0 + i, rng.randrange(256))
            self.m.write16(0x800EC1E0 + 0x24, 1000)
            self.m.write16(0x800EC1E0 + 0x28, 500)
            self.m.write16(0x800EC1E0 + 0x2C, 0)
            for i in range(16):
                self.m.write8(0x800F45A8 + i, rng.randrange(256))
            self.m.write8(state + 0x0E, 1)
            self.m.write8(state + 0x0F, 1)
            self.m.write8(state + 0x18, 0)
            self.m.write8(state + 0x19, 2)
            self.m.write8(state + 0x1A, 3)
            self.m.write8(state + 0x1B, 0xFF)
            self.m.write8(state + 0x32, 0)
            self.m.write8(state + 0x38, 0)
            self.m.write8(state + 0x41, 0)
            self.m.write8(state + 0x42, 0)
            self.m.write8(state + 0x46, 0)
            self.m.write16(0x800F44A0, 1)
            self.m.write16(0x800F4476, 1)
            self.m.write16(slots + 0xB0 + 0x0A, 0)
            self.m.write16(slots + 0xB0 + 0x0E, 0)
            if scenario == 0:
                self.m.write8(state + 0x0E, 0)
            elif scenario == 1:
                self.m.write8(state + 0x1B, 4)
                self.m.write8(slots + 4 * 0xB0 + 0x8E, 0)
            elif scenario == 2:
                self.m.write8(state + 0x1B, 4)
                self.m.write8(slots + 4 * 0xB0 + 0x8E, 1)
                self.m.write8(slots + 4 * 0xB0 + 0x33, 1)
            elif scenario in (3, 4):
                self.m.write8(state + 0x1B, 4)
                self.m.write8(slots + 4 * 0xB0 + 0x8E, 1)
                self.m.write8(slots + 4 * 0xB0 + 0x33, 0)
                self.m.write16(slots + 4 * 0xB0 + 0x94,
                               0x1000 if scenario == 3 else -0x1000)
                self.m.write16(slots + 4 * 0xB0 + 0x98, 0)
            elif scenario == 5:
                self.m.write8(state + 0x42, 1)
                self.m.write8(state + 0x0F, 4)
            elif scenario == 6:
                self.m.write8(state + 0x38, 1)
            elif scenario == 7:
                self.m.write8(state + 0x32, 1)
            elif scenario == 8:
                self.m.write8(state + 0x32, 1)
                self.m.write16(0x800F44A0, 2)
                self.m.write16(0x800F4476, 0x48)
            elif scenario == 9:
                self.m.write8(state + 0x41, 1)
                self.m.write8(state + 0x1B, 4)
            elif scenario == 10:
                self.m.write8(state + 0x0F, 3)
            elif scenario == 11:
                self.m.write8(state + 0x0F, 5)
            args = []
        elif name == "CameraApplySlot":
            state = 0x800E68D0
            slot = 0x801F0000
            args = []
            self.m.write32(state + 0x08, slot)
            self.m.write16(state + 0x1C, rng.choice([-32768, -2048, -1, 0, 1, 2047, 32767]))
            self.m.write16(state + 0x1E, rng.choice([-1200, -1, 0, 1200]))
            self.m.write16(state + 0x20, rng.choice([-900, 0, 900]))
            self.m.write16(state + 0x24, rng.choice([-32768, -512, 0, 512, 32767]))
            self.m.write16(state + 0x26, rng.choice([-700, 0, 700]))
            self.m.write16(state + 0x28, rng.choice([-1000, 0, 1000]))
            self.m.write16(state + 0x30,
                           [0, 1, 31, 32, 47, 48, 128, 255, 0xFFFF][case % 9])
            self.m.write8(state + 0x2E, [0, 1, 2, 3, 4][case % 5])
            for i in range(0xB0): self.m.write8(slot + i, rng.randrange(256))
            for off in (0x0A, 0x0C, 0x0E, 0x12, 0x14, 0x16):
                self.m.write16(slot + off,
                               rng.choice([-32768, -2500, -1, 0, 1, 2500, 32767]))
        elif name == "CameraShakeApply":
            args = []
            amplitudes = [
                (0, 0, 0),
                (1, -1, 0x7FFF),
                (-32768, 32767, -12345),
                (rng.randrange(-32768, 32768),
                 rng.randrange(-32768, 32768),
                 rng.randrange(-32768, 32768)),
            ][case % 4]
            for axis, amplitude in enumerate(amplitudes):
                self.m.write16(0x800E6950 + axis * 2, amplitude)
            self.m.write8(0x800E6958, [0, 1, 2, 3, 4, 5, 255][case % 7])
            for offset in range(0x10):
                self.m.write8(0x800F45A8 + offset, rng.randrange(256))
        elif name == "CameraRegionTest":
            regions = 0x801F2000
            position = 0x801F0100
            args = [position]
            count = 3
            coords = (0, 0, 0)
            for index in range(3):
                region = regions + index * 0x30
                for offset in range(0x30):
                    self.m.write8(region + offset, 0)
                self.m.write16(region + 4, 0)
                self.m.write16(region + 6, 0)
                self.m.write16(region + 8, 0)
                self.m.write16(region + 0x0A, 400)
                self.m.write16(region + 0x0C, 400)
                self.m.write16(region + 0x0E, 400)
            self.m.write32(0x800E691C, regions)
            if case == 0:
                count = 0
            elif case == 2:
                self.m.write16(regions + 2, 0x6000)
            elif case == 3:
                self.m.write16(regions + 2, 0x6000)
                self.m.write16(regions + 0x30 + 2, 0x8100)
            elif 4 <= case <= 15:
                count = 1
                edges = [
                    (-200, 0, 0), (200, 0, 0), (-201, 0, 0), (201, 0, 0),
                    (0, -200, 0), (0, 200, 0), (0, -201, 0), (0, 201, 0),
                    (0, 0, -200), (0, 0, 200), (0, 0, -201), (0, 0, 201),
                ]
                coords = edges[case - 4]
            elif case >= 16:
                count = rng.choice([1, 2, 3])
                # asymmetric regions (different centre / extent per axis), so that comparing an axis against
                # another axis' bounds changes the result
                for index in range(3):
                    region = regions + index * 0x30
                    for axis in range(3):
                        self.m.write16(region + 4 + axis * 2, rng.randrange(-400, 401) & 0xFFFF)
                        self.m.write16(region + 0x0A + axis * 2, rng.randrange(100, 900))
                coords = tuple(rng.randrange(-700, 701) for _ in range(3))
                if case % 4 == 0:
                    self.m.write16(regions + 2, 0x6000)
                if case % 5 == 0:
                    self.m.write16(regions + 0x30 + 2, 0x8100)
            self.m.write16(0x800E68D0, count)
            for offset, coordinate in zip((0, 4, 2), coords):
                self.m.write16(position + offset, coordinate)
        elif name == "EventCamLoadRecord":
            record = 0x801F1000
            args = [record]
            for offset in range(0x30):
                self.m.write8(record + offset, rng.randrange(256))
            self.m.write16(record + 0x0E,
                           [0, 1, -1, 2, -3, 8, 32767, -32768][case % 8])
            for offset in (0, 2, 4, 8, 10, 12):
                self.m.write16(record + offset,
                               rng.choice([-32768, -10000, -1, 0, 1, 10000, 32767]))
            for offset in (0x10, 0x12, 0x14, 0x18, 0x1A, 0x1C):
                self.m.write16(record + offset,
                               rng.choice([-32768, -4096, -1, 0, 1, 4096, 32767]))
        elif name == "EventCamStart":
            state = 0x800E68D0
            records = 0x801F1000
            scenarios = [(0, 0), (1, 0), (2, 1), (2, 2),
                         (3, 0), (3, 2), (3, 3)]
            count, index = scenarios[case % len(scenarios)]
            if case >= len(scenarios):
                count = rng.randrange(0, 5)
                index = rng.randrange(0, 6)
            args = [index]
            self.m.write32(state + 0x60, records)
            self.m.write16(state + 0x68, count)
            self.m.write8(state + 0x46, rng.randrange(256))
            for record_index in range(5):
                record = records + record_index * 0x20
                for offset in range(0x20):
                    self.m.write8(record + offset, rng.randrange(256))
                self.m.write16(record + 0x0E,
                               rng.choice([-5, -1, 0, 1, 4, 15, 32767]))
                for offset in (0, 2, 4, 8, 10, 12):
                    self.m.write16(record + offset,
                                   rng.choice([-32768, -10000, -1, 0, 1, 10000, 32767]))
                for offset in (0x10, 0x12, 0x14, 0x18, 0x1A, 0x1C):
                    self.m.write16(record + offset,
                                   rng.choice([-32768, -4096, -1, 0, 1, 4096, 32767]))
        elif name in ("EventCamCreate", "EventCamTrigger"):
            slots = 0x801F0000
            state = 0x800E68D0
            indexes = 0x801F3000
            scripts = 0x801F4000
            names = 0x801F5000
            name_scripts = 0x801F6000
            for offset in range(12 * 0xB0):
                self.m.write8(slots + offset, 0)
            self.m.write32(0x800B4D40, slots)
            self.m.write16(state + 2, 0)
            self.m.write32(state + 0x50, scripts)
            self.m.write32(state + 0x54, name_scripts)
            self.m.write32(state + 0x58, indexes)
            self.m.write32(state + 0x5C, names)
            self.m.write32(state + 0x60, indexes)
            self.m.write32(state + 0x64, names)
            for index in range(8):
                self.m.write16(indexes + index * 0x28 + 2,
                               rng.randrange(0x100))
                self.m.write16(names + index * 0x28 + 2,
                               rng.randrange(0x100))
            for offset in range(8):
                self.m.write8(0x800F45A8 + offset, rng.randrange(256))
            if name == "EventCamCreate":
                args = [case % 4, case % 2, case % 2]
            else:
                trigger = [1, 2, 3, 4, 5, 6][case % 6]
                if case >= 6:
                    trigger = rng.randrange(0, 8)
                args = [trigger]
                self.m.write16(0x800F4476, rng.randrange(0x10000))
                self.m.write16(0x800F447A, rng.randrange(0x10000))
                self.m.write8(0x800F44A0, 1)
                self.m.write8(0x800F4488, 1)
                self.m.write8(0x800E5468, rng.randrange(256))
                self.m.write8(0x800E5469, rng.randrange(256))
                self.m.write8(0x800E546A, rng.randrange(256))
                self.m.write8(0x800E546B, rng.randrange(256))
                self.m.write8(0x800F4350 + 0x12C, 0)
        elif name == "StageExitRequest":
            args = []
            self.m.write16(0x800F4350 + 0x126,
                           rng.choice([1, 9, 24, 53, 78, 87]))
            self.m.write16(0x800F4350 + 0x12A, rng.randrange(8))
            self.m.write8(0x800F4350 + 0x12C,
                          [0, 32, 34, 1][case % 4])
            self.m.write8(0x800B8CC0 + 0x24, rng.randrange(256))
            self.m.write16(0x800B8CC0 + 0x20, rng.randrange(65536))
        elif name == "EventCamUpdate":
            state = 0x800E68D0
            event = 0x800B8CC0
            records = 0x801F1000
            args = []
            scenarios = [
                (0, 0, 0, 0), (1, 1, 0, 0), (2, 0, 0, 0),
                (1, 0, 0, 0), (3, 0, 1, 0), (1, 1, 0, 1),
                (2, 1, 0, 1), (1, 0, 1, 1),
            ]
            active, frames, kind, finish = scenarios[case % len(scenarios)]
            self.m.write32(state + 0x64, records)
            self.m.write32(state + 0x60, records)
            self.m.write8(event + 0x24, active)
            self.m.write16(event + 0x20, frames)
            self.m.write8(event + 0x25, case % 3)
            self.m.write8(event + 0x26, case % 3)
            self.m.write8(event + 0x28, kind)
            self.m.write8(event + 0x29, rng.randrange(256))
            for record_index in range(3):
                record = records + record_index * 0x20
                for offset in range(0x20):
                    self.m.write8(record + offset, rng.randrange(256))
                self.m.write16(record + 0x0E,
                               rng.choice([-5, -1, 0, 1, 4, 15, 32767]))
                for offset in (0, 2, 4, 8, 10, 12):
                    self.m.write16(record + offset,
                                   rng.choice([-32768, -10000, -1, 0, 1, 10000, 32767]))
                for offset in (0x10, 0x12, 0x14, 0x18, 0x1A, 0x1C):
                    self.m.write16(record + offset,
                                   rng.choice([-32768, -4096, -1, 0, 1, 4096, 32767]))
            if finish:
                self.m.write32(0x800E5478, 0x8E0)
                self.m.write32(0x800E5480, 0x8E0)
            else:
                self.m.write32(0x800E5478, rng.randrange(0x8E0))
                self.m.write32(0x800E5480, rng.randrange(0x8E0))
            self.m.write16(0x800F4350 + 0x126, rng.choice([1, 9, 24, 53, 78, 87]))
            self.m.write16(0x800F4350 + 0x12A, rng.randrange(8))
            self.m.write8(0x800F4350 + 0x12C, 0)
        elif name == "StageExitSequenceUpdate":
            event = 0x800B8CC0
            state = 0x800E68D0
            slots = 0x801F0000
            records = 0x801F1000
            scenarios = [
                (0, 0, 0, 0, 0), (1, 0, 0, 0, 0), (1, 0, 0, 2, 0),
                (1, 1, 1, 0, 0), (1, 1, 0, 0, 0), (2, 0, 0, 0, 0),
                (6, 0, 0, 0, 0), (8, 0, 0, 0, 2), (2, 0, 0, 1, 0),
                (5, 0, 0, 0, 0), (1, 0, 0, 3, 0), (1, 1, 1, 2, 0),
            ]
            frames, done, active, kind, fade = scenarios[case % len(scenarios)]
            if case >= len(scenarios):
                frames = rng.choice([0, 1, 2, 4, 5, 6, 12, 32, 255])
                done = rng.randrange(2)
                active = rng.choice([0, 0, 1, 2])
                kind = rng.randrange(4)
                fade = rng.choice([0, 0, 1, 2, 5, 255])
            args = []
            for offset in range(12 * 0xB0):
                self.m.write8(slots + offset, rng.randrange(256))
            self.m.write32(0x800B4D40, slots)
            self.m.write8(state + 0x18, 0)
            self.m.write32(state + 0x60, records)
            self.m.write16(state + 0x68, 8)
            for record_index in range(8):
                record = records + record_index * 0x20
                for offset in range(0x20):
                    self.m.write8(record + offset, rng.randrange(256))
                self.m.write16(record + 0x0E, rng.choice([0, 1, 2, 4, 8, 16]))
                for offset in (0, 2, 4, 8, 10, 12):
                    self.m.write16(record + offset,
                                   rng.choice([-32768, -10000, -1, 0, 1, 10000, 32767]))
                for offset in (0x10, 0x12, 0x14, 0x18, 0x1A, 0x1C):
                    self.m.write16(record + offset,
                                   rng.choice([-32768, -4096, -1, 0, 1, 4096, 32767]))
            self.m.write16(event + 0x20, frames)
            self.m.write16(event + 0x22, fade)
            self.m.write8(event + 0x24, active)
            self.m.write8(event + 0x28, kind)
            self.m.write8(event + 0x29, done)
            for offset in range(0x10):
                self.m.write8(0x800F45A8 + offset, rng.randrange(256))
            player = 0x800EC1E0
            for offset in (0x24, 0x28, 0x2C):
                self.m.write16(player + offset,
                               rng.choice([-32768, -10000, -1, 0, 1, 10000, 32767]))
            for offset in (0x60, 0x64):
                self.m.write16(player + offset,
                               rng.choice([-4096, -2048, 0, 2048, 4096]))
        elif name == "CameraTransitionFrameDraw":
            args = []
            display_index = case % 2
            self.m.write8(0x800AF130,
                          [0, 1, 2, 3, 8, 255][case % 6])
            self.m.write32(0x800EC354, 0x801F8000 + case * 0x100)
            self.m.write32(0x800EC35C, display_index)
            display_record = 0x800EC350 + display_index * 0x4078
            self.m.write16(display_record + 0x12,
                           rng.choice([0, 0x100, 0x200, 0x300, 0xFFFF]))
            self.m.write32(display_record + 0x80,
                           0x801F6000 + display_index * 0x1000)
        elif name == "CameraRegionUpdate":
            state = 0x800E68D0
            regions = 0x801F2000
            slots = 0x801F0000
            indexes = 0x801F3000
            name_scripts = 0x801F4000
            names = 0x801F5000
            script_bytes = 0x801F6000
            scenarios = [
                (0xFF, 0, 0, 0, 0, 0), (0xFF, 2, 0, 0, 0, 0),
                (0, 1, 0, 0, 0, 0), (1, 1, 0, 0, 0, 0),
                (0xFF, 1, 0x6000, 0x8100, 0, 0), (0, 1, 0, 0, 0, 1),
                (0, 1, 0x6000, 0, 0, 0), (0, 1, 0, 0, 2, 1),
                (0xFF, 1, 2, 0, 0, 0), (0xFF, 1, 0x2002, 0, 0, 0),
                (0xFF, 1, 0x4002, 0, 0, 0), (1, 1, 0, 0, 2, 0),
                (0, 1, 2, 0, 2, 2), (0, 1, 2, 0, 2, 1),
                (0, 1, 0x6002, 0, 2, 1), (0xFF, 1, 0x4000, 0, 0, 0),
            ]
            old, delay, flags0, flags1, old_slot, outside = scenarios[case % len(scenarios)]
            if case >= len(scenarios):
                old = rng.choice([0xFF, 0, 1])
                delay = rng.choice([0, 1, 2, 4])
                flags0 = rng.choice([0, 0, 2, 0x2000, 0x4000, 0x6000, 0x8100])
                flags1 = rng.choice([0, 0, 3, 0x2000, 0x4000, 0x6000, 0x8100])
                old_slot = rng.choice([0, 1, 2, 3])
                outside = rng.randrange(2)
            args = []
            for offset in range(12 * 0xB0):
                self.m.write8(slots + offset, 0)
            self.m.write32(0x800B4D40, slots)
            self.m.write16(state, 2)
            self.m.write16(state + 2, 1 if old_slot != 0 or (flags0 & 0xF) != 0 else 0)
            self.m.write32(state + 0x4C, regions)
            self.m.write8(state + 0x0C, delay)
            self.m.write8(state + 0x0D, old)
            self.m.write8(state + 0x0E, 0)
            self.m.write8(state + 0x0F, 1)
            self.m.write8(state + 0x18, 0)
            self.m.write8(state + 0x19, 1)
            self.m.write8(state + 0x1A, 2)
            self.m.write8(state + 0x1B, old_slot if old != 0xFF else 0xFF)
            self.m.write8(state + 0x32, 0)
            self.m.write8(state + 0x38, 0)
            self.m.write8(state + 0x41, 0)
            self.m.write8(state + 0x42, 0)
            self.m.write8(state + 0x46, 0)
            self.m.write32(state + 0x54, name_scripts)
            self.m.write32(state + 0x58, indexes)
            self.m.write32(state + 0x5C, names)
            self.m.write32(state + 0x60, indexes)
            self.m.write32(state + 0x64, names)
            self.m.write16(0x800F44A0, 1)
            self.m.write16(0x800F4476, 1)
            self.m.write16(0x800F45A8, 300)
            self.m.write16(0x800F45AA, 500)
            self.m.write16(0x800F45AC, -200)
            self.m.write16(0x800F45B0, 0)
            for index, flags in enumerate((flags0, flags1)):
                region = regions + index * 0x30
                for offset in range(0x30):
                    self.m.write8(region + offset, 0)
                self.m.write16(region, rng.randrange(4))
                self.m.write16(region + 2, flags)
                far_from_player = (outside == 1 or
                                   (outside == 2 and index == 0))
                self.m.write16(region + 4, 4000 if far_from_player else 300)
                self.m.write16(region + 6, 4000 if far_from_player else 372)
                self.m.write16(region + 8, 4000 if far_from_player else -200)
                self.m.write16(region + 0x0A, 4000)
                self.m.write16(region + 0x0C, 4000)
                self.m.write16(region + 0x0E, 4000)
            player = 0x800EC1E0
            self.m.write16(player + 0x24, 300)
            self.m.write16(player + 0x28, 500)
            self.m.write16(player + 0x2C, -200)
            self.m.write16(player + 0x60, 0)
            self.m.write16(player + 0x64, 4096)
            for index in range(12):
                slot = slots + index * 0xB0
                if index == old_slot or index in (flags0 & 0xF, flags1 & 0xF):
                    self.m.write8(slot, 4)
                self.m.write16(slot + 6, -370)
                self.m.write16(slot + 8, -110)
                self.m.write16(slot + 0x88, 0)
                self.m.write16(slot + 0x8A, 0)
                self.m.write16(indexes + index * 0x28 + 2, rng.randrange(0x100))
                self.m.write16(names + index * 0x28 + 2, rng.randrange(0x100))
        elif name == "CameraScriptRun":
            slots = 0x801F0000
            slot = slots
            script = 0x801F1000
            programs = [
                [3, 120, 4, 300, 5, 10, 11, 12, 13, 100, 200,
                 14, 15, 21, 1, 2, 3, 22, 0, 24, 300, 500, -200,
                 25, 4, 100, 200, 300, -50, 60, -70],
                [1, 2, 6, 4], [2], [3, 320, 6, 4], [4, 420, 6, 4],
                [5, 6, 4], [6, 5], [7, 6], [8, 7],
                [9, 5], [10, 6, 4], [11, 6, 4], [12, 6, 4],
                [13, -20, 30, 6, 4], [14, 6, 4], [15, 6, 4],
                [16, -900, 480, -50, 6, 4], [17, -60, 350, 80, 6, 4],
                [18, 12, 1024], [19, 12, -90, 32, -16],
                [20, 10, 100, 200, 300, -50, 60, -70],
                [21, 12, -14, 16, 6, 4], [22, 0, 6, 4],
                [23, 9, 2], [24, 200, 350, -120, 6, 4],
                [25, 6, 10, 20, 30, -10, -20, -30],
                [26, 420, 70, 6, 4], [27],
            ]
            program = programs[case % len(programs)]
            self.m.write32(0x800B4D40, slots)
            for offset in range(12 * 0xB0):
                self.m.write8(slots + offset, 0)
            self.m.write8(slot, 4)
            self.m.write8(slot + 2, 1)
            script_mode = 0 if case in (16, 17, 26) else case % 3
            self.m.write8(slot + 0x91, script_mode)
            self.m.write8(slot + 0x92, case % 2)
            self.m.write32(slot + 0x84, script)
            self.m.write16(slot + 0x88, 0)
            self.m.write32(0x800E68D8, slot)
            self.m.write8(0x800E68DE, 0)
            self.m.write8(0x800E68D8 + 0x0D, 0xFF)
            self.m.write32(0x800E691C, 0x801F2000)
            self.m.write16(0x800E68D0, 0)
            self.m.write16(0x800E68D2, 12)
            for offset in range(0x250):
                self.m.write8(0x800EC1E0 + offset, rng.randrange(256))
            for offset, value in ((0, 100), (4, 250), (8, -200),
                                  (0x60, 0), (0x64, 4096)):
                self.m.write32(0x800EC1E0 + 0x24 + offset, value)
            for offset, value in ((0, 200), (2, 300), (4, -100),
                                  (8, 20), (0xA, 30), (0xC, 40)):
                self.m.write16(0x800F45A8 + offset, value)
            for offset, value in ((0x10, 0), (0x12, 0), (0x14, 0),
                                  (0x18, 0), (0x1A, 0), (0x1C, 0)):
                self.m.write16(0x800E68D0 + offset, value)
            for offset, value in ((0x38, 600), (0x40, 0), (0x44, 0),
                                  (0x48, 0), (0x4C, 0), (0x50, 0),
                                  (0x54, 0), (0x58, 0), (0x5C, 0),
                                  (0x60, 0), (0x64, 0)):
                self.m.write32(0x800E68D0 + offset, value)
            for i, word in enumerate(program):
                self.m.write16(script + i * 2, word)
            args = [slot]
        elif name == "CameraUpdate":
            slots = 0x801F0000
            state = 0x800E68D0
            self.m.write32(0x800B4D40, slots)
            for offset in range(12 * 0xB0):
                self.m.write8(slots + offset, 0)
            self.m.write32(state + 8, slots)
            self.m.write16(state + 2, 12)
            self.m.write8(state + 0x0E, 0)
            self.m.write8(state + 0x0F, 1)
            self.m.write8(state + 0x18, 0)
            self.m.write8(state + 0x1B, 0xFF)
            self.m.write8(state + 0x3E, 1 if case % 4 else 0)
            self.m.write8(state + 0x3F, 0)
            self.m.write8(state + 0x40, 0)
            self.m.write8(state + 0x41, 0)
            self.m.write8(state + 0x42, 0)
            self.m.write8(state + 0x46, 0)
            self.m.write8(state + 0x47, 0)
            self.m.write8(state + 0x48, 0)
            self.m.write8(state + 0x32, 0)
            self.m.write8(state + 0x38, 0)
            self.m.write8(0x800AF130, 0)
            self.m.write8(0x800AF131, 0)
            self.m.write8(0x800B8CC0 + 0x24, 0)
            self.m.write8(0x800B8CC0 + 0x29, 0)
            self.m.write8(0x800E690A, 0)
            self.m.write16(state + 0x10, rng.choice([-32, -1, 0, 1, 32]))
            self.m.write16(state + 0x12, rng.choice([-24, -1, 0, 1, 24]))
            self.m.write16(state + 0x14, rng.choice([-16, -1, 0, 1, 16]))
            self.m.write32(0x800E691C, 0x801F2000)
            self.m.write16(state, 0)
            self.m.write8(state + 0x0D, 0xFF)
            for i in range(12):
                slot = slots + i * 0xB0
                self.m.write16(slot + 4, 600)
                self.m.write16(slot + 6, 300)
                self.m.write16(slot + 8, 80)
                self.m.write16(slot + 0x0A, 700 + i)
                self.m.write16(slot + 0x0C, 400)
                self.m.write16(slot + 0x0E, -100)
                self.m.write16(slot + 0x12, 200)
                self.m.write16(slot + 0x14, 300)
                self.m.write16(slot + 0x16, -200)
            kind = [0, 1, 2, 3, 4, 5, 6][case % 7]
            self.m.write8(slots, kind)
            self.m.write8(slots + 0x33, 1)
            self.m.write32(slots + 0x2C, 0)
            self.m.write32(0x800E78C0, 0)
            self.m.write32(0x800E78C4, 0)
            player = 0x800EC1E0
            for offset, value in ((0x24, 100), (0x28, 300), (0x2C, -200),
                                  (0x60, 0), (0x64, 4096)):
                self.m.write32(player + offset, value)
            for offset in range(16):
                self.m.write8(0x800F45A8 + offset, rng.randrange(256))
            self.m.write16(0x800F44A0, 1)
            self.m.write16(0x800F4476, 0x48)
            self.m.write16(0x800F4478, 0)
            self.m.write8(0x800E6902, 1)
            self.m.write16(0x800E6958, 0)
            self.m.write8(0x800AF130, 0)
            self.m.write16(state + 0x1C, 0)
            self.m.write16(state + 0x1E, 0)
            self.m.write16(state + 0x20, 0)
            args = []
        elif name == "IsFlightArea":
            self.m.write16(0x800F44A0, rng.choice([1, 2, 3, 0xFFFF]))
            self.m.write16(0x800F4476, rng.choice([0x47, 0x48, 0x4A, 0x4C, 0x4D, 0xFFFF]))
            args = []
        elif name == "CameraGetPlayerDir":
            mode = [0, 1, 2, 3, 0xFFFFFFFF][case] if case < 5 else rng.choice([0, 1, 2, 3, 0xFFFFFFFF])
            args = [0x801F0000, mode, 0x801F0100]
            for i in range(8): self.m.write8(0x800EC240 + i, rng.randrange(256))
            for i in range(8): self.m.write8(args[2] + i, rng.randrange(256))
        elif name in ("CutsceneModeOn", "CutsceneModeOff"):
            args = []
            self.m.write16(0x800B01C0, rng.randrange(65536))
        else:
            raise AssertionError(name)
        self.c.sync(self.m)
        return args

    def run(self, name, case, source=None):
        args = self.setup(name, case)
        mm = Model(f"camupd/{name}/{case}")
        cm = Model(f"camupd/{name}/{case}")
        mm.pad_result = cm.pad_result = self.pad_result
        self.m.hook_log.clear()
        self.counts[name] = self.counts.get(name, 0) + 1
        addr, _size, retkind, _kinds, _with_hooks = FUNCS[name]
        mhooks = {address: (lambda cpu, regs, n=hname, m=mm:
                            self.c.mips_hook(cpu, regs, n, m))
                  for hname, address, _argc in HOOKS}
        mhooks[0x800301E0] = lambda cpu, regs: self.c.sdk_vector_normalize(
            cpu, regs[4], regs[5])
        mhooks[0x8002FCC0] = lambda cpu, regs: self.c.sdk_rot_matrix_zyx(
            cpu, regs[4], regs[5])
        try:
            stack_pointer = (0x801FFF00 if name == "StageExitSequenceUpdate" else
                             0x801FFF28 if name == "CameraUpdate" else mr.DEFAULT_SP)
            mret = self.m.call(addr, args, hooks=mhooks, sp=stack_pointer)[2]
        except mr.MipsError as e:
            raise mr.MipsError(f"{name} case {case} args {args}: {e}") from e
        cret = self.c.call(name, args, cm)
        problems = []
        if self.c.lib.ae_ram_failed(self.c.ram):
            problems.append("C RAM error: " + self.c.lib.ae_ram_error(self.c.ram).decode())
        if retkind != "v" and u32(mret) != u32(cret):
            problems.append(f"return MIPS {u32(mret):08X}, C {u32(cret):08X}")
        if mm.log != cm.log:
            problems.append(f"hook calls differ: MIPS={mm.log}, C={cm.log}")
        ram_c = bytes(self.c.main)
        ram_m = self.m.ram
        end = STACK_LO - 0x80000000
        if bytes(ram_m[:end]) != ram_c[:end]:
            diffs = [i for i in range(end) if ram_m[i] != ram_c[i]]
            problems.append("RAM differs at " + ", ".join(
                f"{0x80000000+i:08X}: {ram_m[i]:02X}/{ram_c[i]:02X}" for i in diffs[:8]))
        if bytes(self.m.scratch) != bytes(self.c.scratch):
            diffs = [i for i in range(0x400) if self.m.scratch[i] != self.c.scratch[i]]
            problems.append("scratch differs at " + ", ".join(
                f"{0x1F800000+i:08X}: {self.m.scratch[i]:02X}/{self.c.scratch[i]:02X}" for i in diffs[:8]))
        if problems:
            self.failures += 1
            print(f"FAIL {name} case {case}: " + "; ".join(problems))
            return False
        return True

    def finish(self, default_run=False, show_coverage=False):
        items = [(n, a, s, True)
                 for n, (a, s, _ret, _args, _hooks) in FUNCS.items()
                 if n in self.counts]
        summary = reg.coverage_summary(items, self.m.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        if default_run:
            reg.write_results(self.counts, summary, not self.failures,
                              function_names=list(FUNCS))
        return summary


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=int, default=32)
    parser.add_argument("--only", nargs="*", default=[])
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutations", action="store_true")
    args = parser.parse_args()
    names = args.only or list(FUNCS)
    unknown = sorted(set(names) - set(FUNCS))
    if unknown: parser.error("unknown function(s): " + ", ".join(unknown))
    if args.mutations:
        source = ROOT / "src/camera/ae_camupd.c"
        original = source.read_text(encoding="utf-8")
        caught = 0
        missed = []
        for name in names:
            match = re.search(r"\bae_%s\s*\([^)]*\)\s*\{" % re.escape(name), original)
            if match is None:
                raise RuntimeError(f"cannot locate definition of ae_{name}")
            insert = match.end()
            for index, (offset, mask) in enumerate(((0x3F8, 0xA5), (0x3F9, 0x5A)), 1):
                mutation = (f"\n    ae_wr8(r, AE_SCRATCH_BASE + 0x{offset:X}u, "
                            f"ae_rd8(r, AE_SCRATCH_BASE + 0x{offset:X}u) ^ 0x{mask:02X}u);\n")
                variant = OUT / f"camupd_mut_{name}_{index}_{__import__('os').getpid()}.c"
                variant.write_text(original[:insert] + mutation + original[insert:], encoding="utf-8")
                dll = None
                try:
                    dll = build_dll(variant, tag=f"camupd_mut_{name}_{index}")
                    harness = Harness(dll)
                    with contextlib.redirect_stdout(io.StringIO()):
                        passed = harness.run(name, 0)
                    if passed:
                        missed.append((name, index))
                    else:
                        caught += 1
                    del harness
                    gc.collect()
                finally:
                    variant.unlink(missing_ok=True)
            print(f"{name:28s} {2 - sum(1 for n, _ in missed if n == name)}/2 mutations caught")
        print(f"mutation checks: {caught}/{len(names) * 2} caught")
        if missed:
            print("FAILED: escaped mutations " + ", ".join(f"{n}#{i}" for n, i in missed))
            return 1
        return 0
    if (not (ROOT / "extract/disc/SCUS_944.23").exists() or
            any(not area_member.exists() for area_member in AREA_MEMBERS)):
        print(f"SKIP: EXE or real area member missing ({AREA_MEMBERS})")
        return 0
    dll = build_dll()
    harness = Harness(dll)
    for name in names:
        for case in range(max(args.cases, 1)):
            harness.run(name, case)
    summary = harness.finish(default_run=args.cases == 32 and not args.only,
                             show_coverage=args.coverage)
    for name in names:
        print(f"{name:28s} {harness.counts.get(name, 0):4d} cases; "
              f"{summary[name]['executed']}/{summary[name]['total']} instructions")
    print(f"failures: {harness.failures}")
    return 1 if harness.failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
