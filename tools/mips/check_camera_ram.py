"""Differential check for the 1:1 gameplay-camera RAM-image port.

The original functions run from the SCUS-94423 EXE with area 0x01 loaded at
0x80154000. Original functions outside the camera batch are intercepted on
both sides by the same deterministic callback model. Every test compares the
return value, hook sequence, all non-stack RAM, and all scratchpad bytes.
"""
from __future__ import annotations

import argparse
import contextlib
import ctypes as C
import gc
import io
import random
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mips_ram as mr  # noqa: E402
import exclusion_registry as reg  # noqa: E402

M32 = 0xFFFFFFFF
SLOTS = 0x801F0000
REGIONS = 0x801F2000
DRAW_PRIMITIVES = 0x801F5000
STACK_LO = 0x801FF000
AREA_MEMBER = ROOT / "extract/members/091.bin"  # area 0x01, from dump_areas.area_members()
OUT = ROOT / "extract/mips"


def u32(value):
    return value & M32


def s32(value):
    return mr.signed(value, 32)


def s16(value):
    return mr.signed(value, 16)


# name -> original address, size, C return kind, and argument kinds
FUNCS = {
    "CameraKindFixed_Nop": (0x80046970, 8, "v", "u"),
    "CameraSlotInitFollow": (0x80047104, 224, "v", "us"),
    "CameraSlotCreate": (0x800471E4, 172, "s", "su"),
    "CameraSlotRemove": (0x80047290, 208, "v", "u"),
    "CameraSlotAimAtFocus": (0x80047360, 260, "v", "u"),
    "CameraSlotSmoothEye": (0x80047464, 244, "v", "usss"),
    "CameraSlotSmoothTarget": (0x80047558, 252, "v", "usss"),
    "CameraKindAim_SnapToFocus": (0x80047654, 60, "v", "u"),
    "CameraFollowDispatch": (0x800482C0, 216, "v", "u"),
    "CameraKindAimUpdate": (0x80048454, 756, "v", "u"),
    "CameraKindFixedUpdate": (0x8004875C, 560, "v", "u"),
    "CameraSlotPlaceBehindPlayer": (0x80048D10, 180, "v", "uui"),
    "CameraSlotUpdateYaw": (0x80048DD0, 412, "v", "u"),
    "CameraSlotGroundClamp": (0x80048F6C, 256, "v", "u"),
    "CameraSlotGroundClampNear": (0x8004906C, 464, "v", "ui"),
    "CameraSlotStickOrbit": (0x8004923C, 408, "v", "u"),
    "CameraSlotResetFollow": (0x800493D4, 320, "v", "u"),
    "CameraSlotSnapshotEye": (0x80049514, 224, "v", ""),
    "CameraFollowUpdate": (0x800495F4, 4940, "v", "ui"),
    "CameraSlotFlagBit81": (0x8004AA24, 52, "v", ""),
    "CameraFollowAimedVariant": (0x8004AC84, 756, "v", "u"),
    "CameraFollowGadgetVariant": (0x8004AF78, 1344, "v", "u"),
    "CameraKind3FollowUpdate": (0x8004B5FC, 576, "v", "u"),
    "CameraSlotGetFocusPoint": (0x8004B8F4, 280, "v", "us u".replace(" ", "")),
    "CameraSlotClearScript": (0x8004C2E4, 52, "v", "u"),
    "CameraSlotUpdate": (0x8004CA70, 1080, "v", "u"),
}

TARGET_ADDRS = {spec[0] for spec in FUNCS.values()}

# This is the hook order in AeCameraHooks. Each entry also records the original
# function address and the number of ABI arguments that belong to its signature.
HOOKS = [
    ("Atan2Safe", 0x80046340, 2),
    ("VectorNormalSS", 0x800301E0, 2),
    ("SquareRoot0", 0x800303C0, 1),
    ("GetGroundHeight", 0x80043400, 3),
    ("rsin", 0x8002FB50, 1),
    ("rcos", 0x8002FC20, 1),
    ("RotTransPers", 0x80031330, 4),
    ("SetRotMatrix", 0x8002FB00, 1),
    ("SetTransMatrix", 0x8002FB30, 1),
    ("memset", 0x800A3740, 3),
    ("PlayerCanExitStage", 0x8005DC78, 0),
    ("PlayerIsSubmerged", 0x8006B9F8, 0),
    ("CameraRcCarActive", 0x8004A978, 0),
    ("CameraRcCarFollow", 0x8004AA58, 1),
    ("IsFlightArea", 0x8004A940, 0),
    ("FUN_8013d000", 0x8013D000, 1),
    ("BossUpdate", 0x8005AC28, 1),
    ("CameraAreaScriptDispatch", 0x8004B590, 2),
    ("CallIndexedHandler", 0x8004D880, 2),
    ("RotMatrixY", 0x80030890, 2),
    ("ApplyMatrixSV", 0x800300B0, 3),
    ("MatrixIdentity", 0x8004637C, 1),
    ("CameraGetPlayerDir", 0x8004B880, 3),
    ("AimRequestStart", 0x80048398, 0),
    ("AimRequestClear", 0x80048430, 0),
    ("AimModeEnter", 0x8004B4B8, 0),
    ("AimModeLeave", 0x8004B510, 0),
    ("CameraFixedModeOff", 0x80048748, 0),
    ("StageExitPending", 0x8004D104, 0),
    ("BossAimSfx", 0x8004898C, 0),
    ("BossKindForArea", 0x8005AB00, 0),
    ("ScreenFadeSet", 0x80046978, 3),
    ("PlaySfx", 0x800877F8, 1),
    ("HudDrawMonkeyInfo", 0x80060448, 1),
    ("RadarUsable", 0x800468EC, 0),
    ("MonkeyFlag248Set", 0x8009A17C, 1),
    ("CameraRadarFocusUpdate", 0x80047EE0, 0),
    ("CameraRadarFocusSelect", 0x80047E8C, 0),
]
HOOK_BY_ADDR = {address: name for name, address, _ in HOOKS}
HOOKFN = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                     C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
HOOK_STRUCT = type("AeCameraHooks", (C.Structure,), {
    "_fields_": [("user", C.c_void_p)] + [(name, HOOKFN) for name, _, _ in HOOKS]
})


def build_dll(source=None, tag="camera"):
    OUT.mkdir(parents=True, exist_ok=True)
    dll = OUT / f"{tag}_{__import__('os').getpid()}.dll"
    subprocess.run([
        shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-shared", "-static-libgcc", "-I", str(ROOT / "src/camera"),
        str(ROOT / "src/core/ae_ram.c"),
        str(source or ROOT / "src/camera/ae_camera.c"),
        "-o", str(dll),
    ], check=True)
    return dll


class MipsMem:
    def __init__(self, cpu):
        self.cpu = cpu

    def rd8(self, address): return self.cpu.read8(address)
    def rd16(self, address): return self.cpu.read16(address)
    def rds16(self, address): return self.cpu.reads16(address)
    def rd32(self, address): return self.cpu.read32(address)
    def wr8(self, address, value): self.cpu.write8(address, value)
    def wr16(self, address, value): self.cpu.write16(address, value)
    def wr32(self, address, value): self.cpu.write32(address, value)


class CMem:
    def __init__(self, side): self.side = side
    def rd8(self, address): return self.side.lib.ae_rd8(self.side.ram, address)
    def rd16(self, address): return self.side.lib.ae_rd16(self.side.ram, address)
    def rds16(self, address): return self.side.lib.ae_rds16(self.side.ram, address)
    def rd32(self, address): return self.side.lib.ae_rd32(self.side.ram, address)
    def wr8(self, address, value): self.side.lib.ae_wr8(self.side.ram, address, value & 0xFF)
    def wr16(self, address, value): self.side.lib.ae_wr16(self.side.ram, address, value & 0xFFFF)
    def wr32(self, address, value): self.side.lib.ae_wr32(self.side.ram, address, value & M32)


def norm_arg(value):
    value &= M32
    if STACK_LO <= value < 0x80200000:
        return "STACK"
    return value


class Model:
    """Deterministic, address-aware effects for the same external hooks."""

    def __init__(self, seed, mem):
        self.seed = seed
        self.mem = mem
        self.counts = {}
        self.log = []
        self.overrides = {}

    def rng(self, name):
        index = self.counts.get(name, 0)
        self.counts[name] = index + 1
        return random.Random(f"{self.seed}/{name}/{index}")

    def hook(self, name, a0, a1, a2, a3):
        args = [a0, a1, a2, a3]
        argc = next(n for hook_name, _, n in HOOKS if hook_name == name)
        self.log.append((name,) + tuple(norm_arg(v) for v in args[:argc]))
        rng = self.rng(name)
        mem = self.mem

        if name == "memset":
            for offset in range(min(a2 & M32, 0x10000)):
                mem.wr8((a0 + offset) & M32, a1)
            return a0
        if name == "StageExitPending":
            return 1 if mem.rd32(0x800B8CE0) != 0 else 0
        if name == "AimRequestStart":
            mem.wr8(0x800E6902, 1)
            return 0
        if name == "AimRequestClear":
            mem.wr8(0x800E6902, 0)
            return 0
        if name == "AimModeEnter":
            mem.wr8(0x800E6916, 1)
            return 0
        if name == "AimModeLeave":
            mem.wr8(0x800E6916, 0)
            return 0
        if name == "CameraFixedModeOff":
            mem.wr32(0x800B4D44, 0)
            mem.wr8(0x800E6908, 0)
            return 0
        if name == "CameraRadarFocusSelect":
            mem.wr32(0x800B4D44, self.overrides.get(
                name, rng.choice([0, 0x801E9000, 0x801E9040])))
            return 0
        if name == "CameraRadarFocusUpdate":
            for off in (0x10, 0x12, 0x14, 0x18, 0x1A, 0x1C):
                mem.wr16(0x1F800000 + off, rng.randint(-2200, 2200))
            return 0
        if name == "CameraGetPlayerDir":
            facing = 0x800EC1E0 + 0x60
            mem.wr16(a2, mem.rds16(facing))
            mem.wr16(a2 + 2, 0)
            mem.wr16(a2 + 4, mem.rds16(facing + 4))
            mem.wr16(a2 + 6, 0)
            return 0
        if name == "MatrixIdentity":
            for offset in range(32):
                mem.wr8(a0 + offset, 0)
            for offset in (0, 8, 16):
                mem.wr16(a0 + offset, 0x1000)
            return 0
        if name == "RotMatrixY":
            for offset in range(32):
                mem.wr8(a1 + offset, 0)
            mem.wr16(a1, 0x1000)
            mem.wr16(a1 + 8, 0x1000)
            mem.wr16(a1 + 16, 0x1000)
            mem.wr16(a1 + 20, a0)
            return 0
        if name == "ApplyMatrixSV":
            for axis in range(3):
                mem.wr16(a2 + axis * 2, mem.rd16(a1 + axis * 2))
            return 0
        if name == "VectorNormalSS":
            x, y, z = (mem.rds16(a0 + i) for i in (0, 2, 4))
            scale = max(1, abs(x), abs(y), abs(z))
            for offset, value in zip((0, 2, 4), (x, y, z)):
                mem.wr16(a1 + offset, int(value * 4096 / scale))
            return rng.choice([0, 1, 16, 63, 64, 65, 1024, 4096])
        if name == "SquareRoot0":
            if name in self.overrides:
                return self.overrides[name]
            return rng.choice([0, 1, 16, 63, 64, 65, 256, 1024])
        if name == "Atan2Safe":
            if name in self.overrides:
                return s32(self.overrides[name])
            return rng.choice([-4096, -3072, -2048, -1, 0, 1, 1024, 2048, 4095])
        if name in ("rsin", "rcos"):
            return rng.choice([-4096, -3072, -2048, -1024, 0, 1024, 2048, 3072, 4096])
        if name == "GetGroundHeight":
            if name in self.overrides:
                return s16(self.overrides[name])
            y = s16(a1)
            return s16(y + rng.choice([-600, -300, -192, -128, -1, 0, 1, 128, 300, 600]))
        if name == "RotTransPers":
            x, y, z = (mem.rds16(a0 + i) for i in (0, 2, 4))
            mem.wr16(a1, x + rng.choice([-50, 0, 50]))
            projected_y = self.overrides.get("RotTransPersY")
            mem.wr16(a1 + 2, y + rng.choice([-50, 0, 50])
                     if projected_y is None else projected_y)
            mem.wr32(a2, z & M32)
            mem.wr32(a3, rng.getrandbits(32))
            return rng.choice([0, 1, 32, 64, 128, 0x7FFF])
        if name == "PlayerCanExitStage":
            return rng.choice([0, 0, 1])
        if name in ("PlayerIsSubmerged", "CameraRcCarActive", "IsFlightArea",
                    "BossUpdate", "RadarUsable", "MonkeyFlag248Set", "BossKindForArea"):
            if name in self.overrides:
                return self.overrides[name]
            return rng.choice([0, 0, 1])
        if name == "PlayerCanExitStage" and name in self.overrides:
            return self.overrides[name]
        if name == "CallIndexedHandler":
            return rng.choice([0, 1, 0xFFFFFFFF])
        return 0


class CSide:
    def __init__(self, dll):
        self.lib = lib = C.CDLL(str(dll))
        P, u32, i32 = C.c_void_p, C.c_uint32, C.c_int32
        lib.ae_ram_create.restype = P
        lib.ae_ptr.restype = P
        lib.ae_ptr.argtypes = [P, u32, u32]
        for name, ctype, args in (
            ("ae_rd8", u32, [P, u32]), ("ae_rd16", u32, [P, u32]),
            ("ae_rd32", u32, [P, u32]), ("ae_rds8", i32, [P, u32]),
            ("ae_rds16", i32, [P, u32]), ("ae_wr8", None, [P, u32, u32]),
            ("ae_wr16", None, [P, u32, u32]), ("ae_wr32", None, [P, u32, u32]),
            ("ae_ram_failed", i32, [P]), ("ae_ram_error", C.c_char_p, [P]),
            ("ae_ram_clear_error", None, [P]),
        ):
            fn = getattr(lib, name)
            fn.restype = ctype
            fn.argtypes = args

        for name, (_, _, ret, kinds) in FUNCS.items():
            fn = getattr(lib, "ae_" + name)
            fn.argtypes = [P, P] + [i32 if kind in "is" else u32 for kind in kinds]
            fn.restype = {"v": None, "s": i32, "u": u32}[ret]

        self.ram = lib.ae_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(
            lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scr = (C.c_uint8 * 0x400).from_address(
            lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.mem = CMem(self)
        self.model = None
        self.hooks = HOOK_STRUCT()
        self._callbacks = []
        for name, _, _ in HOOKS:
            cb = HOOKFN(lambda user, ram, a0, a1, a2, a3, n=name:
                        s32(self.model.hook(n, a0, a1, a2, a3)))
            self._callbacks.append(cb)
            setattr(self.hooks, name, cb)

    def call(self, name, args):
        _, _, ret, kinds = FUNCS[name]
        cooked = [s32(v) if k in "is" else u32(v) for v, k in zip(args, kinds)]
        return getattr(self.lib, "ae_" + name)(self.ram, C.byref(self.hooks), *cooked)


class Harness:
    def __init__(self, dll):
        self.m = mr.MipsRam.from_extract(area=AREA_MEMBER)
        self.m.coverage = set()
        self.base = self.m.snapshot()
        self.c = CSide(dll)
        self.stack_lo = STACK_LO - 0x80000000
        self.counts = {}
        self.failures = 0
        self.hook_calls = 0
        self.seconds = 0.0
        reg.declare("stack_window", f"{STACK_LO:08X}..80200000")

    def sync(self):
        C.memmove(self.c.main, bytes(self.m.ram), 0x200000)
        C.memmove(self.c.scr, bytes(self.m.scratch), 0x400)
        self.c.lib.ae_ram_clear_error(self.c.ram)

    def setup(self, name, case_seed):
        rng = random.Random(case_seed)
        case_no = int(case_seed.rsplit("/", 1)[1])
        self.hook_overrides = {}
        m = self.m
        m.restore(self.base)
        m.write32(0x800B4D40, SLOTS)
        m.write32(0x800E68DC, 0)
        m.write32(0x800E691C, 0)
        m.write32(0x800E68DC, 0)
        m.write8(0x800E68D2, 12)
        m.write8(0x800E68E8, rng.randrange(12))
        m.write8(0x800E68E6, rng.choice([0, 1, 2, 3]))
        m.write8(0x800E68E7, rng.randrange(2))
        m.write8(0x800E6902, rng.choice([0, 1]))
        m.write8(0x800E6908, rng.choice([0, 1, 2]))
        m.write8(0x800E690F, rng.randrange(2))
        m.write8(0x800E6910, rng.randrange(2))
        m.write8(0x800E6916, rng.choice([0, 0, 1]))
        m.write8(0x800E6917, 0)
        m.write8(0x800E6918, rng.randrange(2))
        m.write32(0x800E6904, rng.randrange(0x10000))
        m.write16(0x800E6906, rng.randrange(0x1000))
        m.write32(0x800E6918 + 0x34, rng.getrandbits(32))
        m.write8(0x800E68E8, rng.randrange(12))
        m.write32(0x800E691C, REGIONS)
        m.write32(0x800B8CE0, rng.choice([0, 0, 0, 1]))
        m.write32(0x800E5478, rng.choice([0, 1, 4, 5, 0x1000, 0x2000, 0x4000, 0x8000, 0xE000]))
        m.write32(0x800E5480, rng.choice([0, 1, 4, 5, 0x1000, 0x2000, 0x4000, 0x8000, 0xE000]))
        m.write16(0x800E54A0, rng.choice([0, 50, 64, 100, 101, 256]))
        m.write8(0x800E53C2, rng.choice([-127, -81, -50, -1, 0, 1, 50, 81, 127]))
        m.write8(0x800E53C3, rng.choice([-127, -81, -50, -1, 0, 1, 50, 81, 127]))
        m.write16(0x800F44A0, rng.choice([0, 1, 2, 3, 10]))
        m.write16(0x800F4476, rng.choice([1, 5, 0x13, 0x24, 0x38, 0x42, 0x48, 0x58]))
        m.write32(0x800F4450, rng.choice([0, 0x2000, 0x8000, 0xA000]))
        m.write32(0x800B4D44, rng.choice([0, 0x801E9000, 0x801E9040]))
        m.write32(0x800FC354, DRAW_PRIMITIVES)
        m.write32(0x800FC3D4, DRAW_PRIMITIVES)
        m.write32(0x800FC3DC, rng.randrange(32))
        m.write32(0x800EC354, DRAW_PRIMITIVES)
        m.write32(0x800EC35C, rng.randrange(32))
        m.write8(0x800B8CB0, rng.randrange(32))

        for i in range(0x1000):
            m.write8(REGIONS + i, rng.randrange(256))
        for i in range(12 * 0xB0):
            m.write8(SLOTS + i, rng.randrange(256))
        for i in range(12):
            m.write8(SLOTS + i * 0xB0, rng.choice([0, 1, 2, 3, 4, 5]))
            m.write8(SLOTS + i * 0xB0 + 0x93, rng.choice([0xFF, 0, 1, 2, 3]))
        for i in range(0x200):
            m.write8(0x800EC1E0 + i, rng.randrange(256))
        for i in range(0x200):
            m.write8(0x800E68C0 + i, rng.randrange(256))
        for i in range(0x180):
            m.write8(0x1F800000 + i, rng.randrange(256))

        # Reassert fields used as pointers or small indices after the noisy data fill.
        m.write32(0x800B4D40, SLOTS)
        m.write32(0x800E691C, REGIONS)
        m.write8(0x800E68E8, rng.randrange(12))
        m.write8(0x800E68D2, 12)
        m.write8(0x800E6917, 0)
        m.write8(0x800E6902, rng.choice([0, 1]))
        m.write8(0x800E6916, rng.choice([0, 0, 1]))
        m.write8(0x800E6908, rng.choice([0, 1, 2]))
        m.write16(0x800F44A0, rng.choice([0, 1, 2, 3, 10]))
        m.write16(0x800F4476, rng.choice([1, 5, 0x13, 0x24, 0x38, 0x42, 0x48, 0x58]))
        m.write32(0x800EC354, DRAW_PRIMITIVES)
        m.write32(0x800EC35C, rng.randrange(32))
        m.write16(0x800EC1E0 + 0x5E, rng.choice([0, 1, 3, 5, 10, 0x2D, 0x46, 0x50, 0x82, 0x83, 0x84, 0x86]))
        m.write32(0x800EC1E0 + 0x70, rng.choice([0, 0x80, 0xF0]))
        m.write8(0x800EC1E0 + 0xF2, rng.choice([-1, 0, 1, 2, 3, 4, 7]))
        m.write16(0x800EC1E0 + 0x24, rng.randint(-2400, 2400))
        m.write16(0x800EC1E0 + 0x28, rng.randint(-1000, 1000))
        m.write16(0x800EC1E0 + 0x2C, rng.randint(-2400, 2400))
        m.write16(0x800EC1E0 + 0x60, rng.randint(-4096, 4096))
        m.write16(0x800EC1E0 + 0x64, rng.randint(-4096, 4096))

        index = rng.randrange(12)
        slot = SLOTS + index * 0xB0
        follow_slot = SLOTS + m.read8(0x800E68E8) * 0xB0
        m.write8(slot + 0x93, rng.choice([0xFF, 0, 1, 2, 3]))
        m.write8(follow_slot + 0x93, 0xFF)
        m.write8(0x800E68EB, 0xFF)
        m.write8(0x800E68E8, index)
        m.write8(0x800E68D2, rng.randrange(13))
        m.write8(0x800E68D8, index)
        m.write8(0x800E68D9, 0xFF)

        args = {
            "CameraKindFixed_Nop": (slot,),
            "CameraSlotInitFollow": (slot, rng.choice([0, 1, 2, 3, 4, 5])),
            "CameraSlotCreate": (rng.choice([1, 2, 3, 4, 5]), 0x800F45A8),
            "CameraSlotRemove": (index,),
            "CameraSlotAimAtFocus": (slot,),
            "CameraSlotSmoothEye": (slot, rng.choice([1, 2, 3, 4, 5, 8]), rng.choice([1, 2, 3, 4, 5, 8]), rng.choice([1, 2, 3, 4, 5, 8])),
            "CameraSlotSmoothTarget": (slot, rng.choice([1, 2, 3, 4, 5, 8]), rng.choice([1, 2, 3, 4, 5, 8]), rng.choice([1, 2, 3, 4, 5, 8])),
            "CameraKindAim_SnapToFocus": (slot,),
            "CameraFollowDispatch": (slot,),
            "CameraKindAimUpdate": (slot,),
            "CameraKindFixedUpdate": (slot,),
            "CameraSlotPlaceBehindPlayer": (slot, 0x1F800010, rng.randint(-2048, 2048)),
            "CameraSlotUpdateYaw": (slot,),
            "CameraSlotGroundClamp": (slot,),
            "CameraSlotGroundClampNear": (slot, rng.choice([0, 1])),
            "CameraSlotStickOrbit": (slot,),
            "CameraSlotResetFollow": (slot,),
            "CameraSlotSnapshotEye": (),
            "CameraFollowUpdate": (slot, rng.choice([0, 1])),
            "CameraSlotFlagBit81": (),
            "CameraFollowAimedVariant": (slot,),
            "CameraFollowGadgetVariant": (slot,),
            "CameraKind3FollowUpdate": (slot,),
            "CameraSlotGetFocusPoint": (slot, rng.choice([0, 1, 2]), 0x801FF180),
            "CameraSlotClearScript": (slot,),
            "CameraSlotUpdate": (slot,),
        }[name]

        if name == "CameraSlotCreate":
            for i in range(12):
                m.write8(SLOTS + i * 0xB0, 1)
            if case_no % 2 == 0:
                m.write8(SLOTS + (case_no // 2 % 12) * 0xB0, 0)
        if name == "CameraSlotRemove":
            remove_case = case_no % 6
            m.write8(slot, [3, 4, 5, 4, 5, 1][remove_case])
            m.write8(slot + 0x93, [0xFF, 0, 1, 2, 3, 0xFF][remove_case])
            m.write8(0x800E68D0 + 0x41, remove_case & 1)
            m.write8(0x800E68D0 + 0x1B,
                     index if remove_case in (1, 2, 3, 4) else (index + 1) % 12)
            m.write16(0x800E68D0 + 4,
                      index if remove_case == 5 else (index + 1) % 12)
        if name == "CameraSlotInitFollow":
            m.write8(slot, 0)
        if name == "CameraFollowUpdate":
            follow_case = case_no % 32
            mode = 1 if follow_case in (3, 5, 21, 23, 29, 30, 31) else 0
            args = (slot, mode)
            player_flags = [0, 0x20, 0x20, 0x30, 0x40, 0x50, 0, 0x20,
                            0, 0x50, 0x50, 0x40, 0, 0, 0x20, 0x20,
                            0, 0x80, 0xA0, 0x20, 0x40, 0x30, 0x20, 0x50,
                            0x30, 0, 0, 0, 0x20, 0x50, 0, 0x20][follow_case]
            direction_x = {1: 3000, 2: 3000, 3: 500, 4: 3000, 5: 1500,
                           6: 500, 7: 500, 8: 0, 9: 500, 10: 500, 11: -500,
                           12: 3000, 13: 3000, 14: 3000, 15: 3000,
                           16: 500, 17: 500, 18: 3000, 19: 500, 20: 500,
                           21: 500, 22: 3000, 23: 500,
                           24: 3000, 25: 3000, 26: 3000, 27: 3000,
                           28: 3000, 29: 3000, 30: 3000, 31: -16000}.get(follow_case, 0)
            player_mode = 10 if follow_case in (8, 28) else 0
            player_state = (0x2D if follow_case in (19, 31) else
                            (6 if follow_case in (18, 25) else 0))
            gadget = 3 if follow_case in (12, 13) else 0
            m.write8(0x800E6917, 1 if follow_case == 0 else 0)
            m.write8(0x800E6918, 1 if follow_case == 21 else 0)
            m.write8(0x800E690F, 1 if follow_case in (9, 10) else 0)
            m.write8(0x800E6910, 1 if follow_case in (9, 10) else 0)
            m.write16(0x800E6904, -100)
            m.write16(0x800E6906, 0x400)
            m.write16(0x800F44A0, player_mode)
            m.write16(0x800F4476, 7 if follow_case == 20 else 0)
            m.write32(0x800F4450, 1 if follow_case == 20 else 0)
            m.write32(0x800EC1E0 + 0x70, player_flags)
            m.write16(0x800EC1E0 + 0x5E, player_state)
            m.write8(0x800EC1E0 + 0xF2, gadget)
            player_x = -1000 if follow_case in (4, 25) else (100 if follow_case == 31 else 1000)
            player_z = 1000 if follow_case == 24 else (-1000 if follow_case in (4, 25) else 0)
            m.write16(0x800EC1E0 + 0x24, player_x)
            m.write16(0x800EC1E0 + 0x28, 1000)
            m.write16(0x800EC1E0 + 0x2C, player_z)
            m.write16(0x1F800010, player_x)
            m.write16(0x1F80001A, 1000)
            m.write16(0x1F800014, player_z)
            m.write32(0x1F80002C, 1000 if follow_case == 24 else 0)
            m.write16(0x1F800040, direction_x)
            m.write16(0x1F800044, -16000 if follow_case == 31 else
                      (1500 if follow_case in (5, 30) else 0))
            m.write32(0x1F800068, 10 if follow_case in (5, 30) else 0)
            m.write32(0x1F80006C, (64 if follow_case in (29, 30) else
                                   (-32 if follow_case == 31 else
                                    (32 if follow_case == 10 else 0))))
            m.write32(0x1F800070, -10 if follow_case in (5, 30) else 0)
            if follow_case == 5:
                m.write32(0x1F800068, -10)
            for offset, value in ((0x0A, 0), (0x0C, 0), (0x0E, 0),
                                  (0x12, 0), (0x14, 0), (0x16, 0),
                                  (0x1A, 1000), (0x1C, 1000), (0x1E, 0),
                                  (0x38, 0x500), (0x40, 0), (0x44, 0),
                                  (0x48, 0), (0x4C, 0), (0x50, 0),
                                  (0x58, 0), (0x5A, 0), (0x5C, 0),
                                  (0x60, 0), (0x64, 0), (0x68, 0),
                                  (0x6C, 0), (0x6E, 0), (0x80, 0)):
                if offset in (0x38, 0x40, 0x44, 0x48, 0x4C, 0x58, 0x60):
                    m.write32(slot + offset, value)
                elif offset == 0x80:
                    m.write8(slot + offset, value)
                else:
                    m.write16(slot + offset, value)
            m.write8(slot + 0x30, 17 if follow_case == 7 else 0)
            m.write8(slot + 0x31, 0)
            m.write8(slot + 0x32, 2 if follow_case == 14 else (1 if follow_case == 21 else 0))
            m.write8(slot + 0x33, 1 if follow_case == 16 else 0)
            m.write8(slot + 0x34, 1 if follow_case in (22, 26, 27) else 0)
            m.write8(slot + 0x35, 2 if follow_case == 14 else 0)
            m.write8(slot + 0x36, 5 if follow_case == 14 else 0)
            m.write8(slot + 0x37, 13 if follow_case == 15 else 0)
            m.write8(slot + 0x80, 1 if follow_case == 6 else 0)
            m.write16(slot + 0x04, -10000 if follow_case in (29, 31) else
                      (0 if follow_case == 30 else (-1000 if follow_case == 23 else 0x400)))
            m.write32(slot + 0x38, (0x100 if follow_case in (2, 14) else
                                    (-1000 if follow_case == 28 else
                                     (-10000 if follow_case == 30 else
                                      (0 if follow_case == 8 else 0x500)))))
            m.write16(slot + 0x68, 1000 if follow_case == 1 else 0)
            if follow_case in (1, 24, 25):
                m.write16(slot + 0x1C, 950 if follow_case == 1 else 1050)
            if follow_case == 14:
                m.write16(slot + 0x52, -8)
            if follow_case in (19, 31):
                m.write16(slot + 0x50, -8)
                m.write16(slot + 0x52, -16)
            if follow_case in (26, 27):
                m.write32(slot + 0x40, 0 if follow_case == 26 else -5120)
                m.write32(slot + 0x48, 0)
                m.write32(slot + 0x4C, 0)
                m.write32(slot + 0x70, 0 if follow_case == 26 else 0)
                m.write32(slot + 0x74, 0 if follow_case == 26 else -5000)
            if follow_case == 31:
                m.write16(slot + 0x12, -1000)
                m.write16(slot + 0x16, -1000)
                m.write16(slot + 0x5A, -100)
                m.write8(slot + 0x37, 13)
            if follow_case == 27:
                m.write32(slot + 0x40, -5120)
                m.write32(slot + 0x70, -5000)
                m.write32(slot + 0x74, -5000)
            if follow_case == 26:
                m.write32(slot + 0x40, 0)
                m.write32(slot + 0x70, 0)
                m.write32(slot + 0x74, 0)
            if follow_case == 14:
                m.write16(slot + 0x04, 0)
                m.write32(slot + 0x38, 0x100)
            if follow_case == 24:
                m.write16(slot + 0x0C, 1000)
                m.write32(0x1F80002C, 0)
                m.write16(slot + 0x0E, 0)
                m.write16(slot + 0x14, 0)
                m.write8(0x800E53C2, 100)
            if follow_case == 25:
                m.write16(slot + 0x0C, 0)
                m.write16(slot + 0x0E, 0)
                m.write16(slot + 0x14, 0)
                m.write8(0x800E53C2, 100)
            if follow_case in (26, 27):
                m.write8(slot + 0x32, 1)
                m.write8(slot + 0x34, 1)
            if follow_case == 29:
                m.write8(0x800E6910, 1)
                m.write16(0x800E6906, -10000)
                m.write32(0x1F80002C, 3000)
                self.hook_overrides["RotTransPersY"] = 0
            if follow_case == 30:
                m.write8(0x800E6910, 1)
                m.write16(0x800E6906, 0)
                self.hook_overrides["RotTransPersY"] = 300
                m.write16(slot + 0x5A, 100)
            if follow_case == 31:
                m.write8(slot + 0x33, 1)
                m.write8(slot + 0x37, 13)
                self.hook_overrides["RotTransPersY"] = 0
            if follow_case == 25:
                m.write8(slot + 0x31, 1)
            if follow_case == 28:
                m.write16(slot + 0x38, -1000)
            if follow_case == 27:
                self.hook_overrides["Atan2Safe"] = -4096
            if follow_case == 26:
                self.hook_overrides["Atan2Safe"] = 0
            m.write16(0x800E53C2, 100 if follow_case in (1, 2, 4, 6, 10, 14, 16, 24, 25) else
                      (-100 if follow_case == 3 else 0))
            stick_mag = 0 if follow_case in (7, 26, 27) else 101
            m.write32(0x800E54A0, stick_mag)
            m.write32(0x800E5478, 0)
            if follow_case == 7:
                for scratch_off, slot_off in ((0x10, 0x1A), (0x1A, 0x1C), (0x14, 0x1E)):
                    m.write16(slot + slot_off, m.read16(0x1F800000 + scratch_off))
            self.hook_overrides["PlayerCanExitStage"] = 1
            self.hook_overrides["PlayerIsSubmerged"] = 1 if follow_case == 9 else 0
            self.hook_overrides["CameraRcCarActive"] = 1 if follow_case == 6 else 0
            self.hook_overrides["SquareRoot0"] = 0 if follow_case == 11 else 63
            if follow_case in (1, 3, 5, 7, 10, 14, 15, 18, 19, 20, 21, 23, 24, 25, 26, 27, 28, 29, 30, 31):
                self.hook_overrides["GetGroundHeight"] = 1000
            elif follow_case == 2:
                self.hook_overrides["GetGroundHeight"] = 0
            elif follow_case == 4:
                self.hook_overrides["GetGroundHeight"] = -1000
            if follow_case == 25:
                self.hook_overrides["GetGroundHeight"] = -1000
            if follow_case in (12, 13):
                m.write8(0x800E6916, 1 if follow_case == 13 else 0)
        elif name == "CameraSlotUpdate":
            m.write32(slot + 0x84, REGIONS)
            m.write16(slot + 0x88, rng.choice([1, 2, 5, 20, -1]))
            m.write8(slot + 0x90, 2 + case_no % 8)
            m.write8(slot + 0x91, (case_no % 2) * 2)
            m.write8(slot + 0x92, rng.choice([0, 1, 2]))
            m.write8(slot + 0x03, 1 if case_no % 10 == 0 else 0)
            m.write8(0x800AF131, 1 if case_no % 10 == 1 else 0)
        elif name == "CameraFollowAimedVariant":
            m.write8(0x800E6908, rng.choice([0, 2]))
            m.write8(0x800E6916, rng.choice([0, 1]))
            m.write8(0x800E68E8, index)
            m.write16(0x800F4476, rng.choice([1, 5]))
        elif name == "CameraFollowGadgetVariant":
            gadget_case = case_no % 5
            m.write8(0x800E6916, 1 if gadget_case in (3, 4) else 0)
            m.write16(0x800E54A0, 100 if gadget_case in (0, 1) else 50)
            gadget_state = 0x85 if gadget_case == 2 else (0x86 if gadget_case == 3 else
                            (0x81 if gadget_case == 4 else 0x82))
            m.write16(0x800EC1E0 + 0x5E, gadget_state)
            m.write16(0x800E6904, rng.randrange(0x10000))
            m.write8(0x800E53C3, -100 if gadget_case == 0 else (100 if gadget_case == 1 else 0))
            m.write32(0x800EC1E0 + 0x28, 0 if gadget_case == 0 else 1000)
            m.write16(0x1F800002, 0)
            m.write16(slot + 0x38, 100)
            m.write32(0x800B8CE0, 0)
            self.hook_overrides["BossKindForArea"] = 0
        elif name == "CameraKindAimUpdate":
            aim_case = case_no % 20
            m.write8(0x800E6902, 1 if aim_case == 0 else 0)
            pad = 1 if aim_case < 12 else 5
            m.write32(0x800E5478, pad)
            m.write32(0x800E5480, pad)
            player_states = [0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 16, 18,
                             1, 1, 70, 0, 19, 18, 20, 0x88]
            gadgets = [3] * 12 + [4, 4, 4, 3, 3, 3, 3, 3]
            m.write16(0x800EC1E0 + 0x5E, player_states[aim_case])
            m.write8(0x800EC1E0 + 0xF2, gadgets[aim_case])
            m.write8(0x800E6916, 1 if aim_case in (1, 12) else 0)
            m.write8(0x800E690E, 0)
            m.write8(0x800E6911, 0)
            m.write16(0x800F4476, 0)
            m.write16(0x800F44A0, 2 if aim_case == 15 else 0)
            m.write8(0x800E68E8, index)
            follow_slot = SLOTS + index * 0xB0
            m.write8(follow_slot + 0x32, 0)
            m.write32(0x800B8CE0, 0)
            if aim_case >= 8:
                self.hook_overrides["PlayerCanExitStage"] = 1
        elif name == "CameraKindFixedUpdate":
            fixed_case = case_no % 4
            m.write8(0x800E6908, [0, 1, 2, 3][fixed_case])
            pressed = 1
            m.write32(0x800E5478, pressed)
            m.write32(0x800E5480, pressed)
            m.write8(0x800E6902, rng.choice([0, 1]))
            if fixed_case == 0:
                self.hook_overrides.update({
                    "RadarUsable": 1,
                    "CameraRadarFocusSelect": 0x801E9000,
                })
            elif fixed_case == 1:
                self.hook_overrides.update({
                    "RadarUsable": 0 if case_no % 8 == 1 else 1,
                    "MonkeyFlag248Set": 1 if case_no % 8 == 5 else 0,
                })
            elif fixed_case in (2, 3):
                self.hook_overrides["RadarUsable"] = 1
        elif name == "CameraFollowDispatch":
            m.write16(0x800F44A0, rng.choice([0, 1, 2, 3]))
            m.write8(0x800E6908, rng.choice([0, 1]))

        if name == "CameraFollowAimedVariant":
            m.write16(0x800E68D6, 2 if case_no % 2 == 0 else 0)
            m.write8(0x800E6916, 1 if case_no % 4 == 0 else 0)
            m.write32(0x800EC250, 0x80 if case_no % 8 == 0 else 0)
            m.write8(0x800E53C2, [-127, -81, -1, 0, 1, 50, 81, 127][case_no % 8])
            m.write8(0x800E53C3, [-127, -81, -1, 0, 1, 50, 81, 127][(case_no // 2) % 8])
            m.write32(0x800E5478,
                      [0, 0x8000, 0x2000, 0xA000, 0x1000, 0x4000, 0x5000, 0][(case_no // 2) % 8])
            m.write16(0x800F44A0, 1 if case_no % 8 == 0 else 2)
        elif name == "CameraSlotGetFocusPoint":
            args = (slot, case_no % 4, args[2])
        elif name == "CameraSlotUpdateYaw" and case_no % 2 == 0:
            m.write32(0x800E54A0, 0)
            for scratch_off, slot_off, value in ((0x10, 0x1A, 123),
                                                 (0x1A, 0x1C, -456),
                                                 (0x14, 0x1E, 789)):
                m.write16(0x1F800000 + scratch_off, value)
                m.write16(slot + slot_off, value)
        elif name == "CameraSlotGroundClampNear" and case_no % 4 == 0:
            args = (slot, 1)
            m.write8(slot + 0x34, 0)
            m.write16(slot + 0x0C, 0)
            m.write32(slot + 0x44, 0x12345678)
            m.write16(0x1F800002, 1000)
            self.hook_overrides["SquareRoot0"] = 1024
            self.hook_overrides["GetGroundHeight"] = 0
        elif name == "CameraSlotGroundClampNear" and case_no % 4 == 2:
            args = (slot, 0)
            m.write8(slot + 0x34, 0)
            m.write8(slot + 0x35, 0)
            m.write8(slot + 0x30, 0)
            m.write8(slot + 0x37, 0)
            m.write16(0x1F800002, 1000)
            self.hook_overrides["SquareRoot0"] = 1024
            self.hook_overrides["GetGroundHeight"] = 2000
        elif name == "CameraSlotStickOrbit" and case_no % 2 == 0:
            m.write8(slot + 0x34, 0)
            m.write8(slot + 0x32, 1)
            m.write8(0x800E6908, 1)
            m.write32(0x800E54A0, 101)
            m.write8(0x800E53C2, 0)
            self.hook_overrides["PlayerCanExitStage"] = 1
        if name == "CameraSlotStickOrbit":
            orbit_case = case_no % 4
            self.hook_overrides["PlayerCanExitStage"] = 1
            m.write8(0x800E53C2, 0)
            if orbit_case == 1:
                m.write32(0x800E5478, 0x1000)
                m.write16(0x1F80001A, 1000)
                m.write16(slot + 0x0C, 0)
            elif orbit_case == 2:
                m.write32(0x800E5478, 0x4000)
                m.write16(0x1F80001A, 0)
                m.write16(slot + 0x0C, 1000)

        return args

    def mips_hooks(self, model):
        out = {}
        for name, address, _ in HOOKS:
            out[address] = lambda cpu, regs, n=name: model.hook(n, *regs[4:8])
        return out

    def run(self, name, case, source=None):
        self.m.restore(self.base)
        args = self.setup(name, f"camera/{name}/{case}")
        self.sync()
        mm = Model(f"camera/{name}/{case}", MipsMem(self.m))
        cm = Model(f"camera/{name}/{case}", self.c.mem)
        mm.overrides.update(self.hook_overrides)
        cm.overrides.update(self.hook_overrides)
        self.c.model = cm
        self.m.hook_log.clear()
        self.counts[name] = self.counts.get(name, 0) + 1
        start = time.perf_counter()
        try:
            regs = self.m.call(FUNCS[name][0], args, hooks=self.mips_hooks(mm))
        except mr.MipsError as error:
            raise mr.MipsError(f"{name} case {case} args {args}: {error}") from error
        mret = regs[2]
        cret = self.c.call(name, args)
        self.seconds += time.perf_counter() - start
        self.hook_calls += len(mm.log)

        problems = []
        if self.c.lib.ae_ram_failed(self.c.ram):
            problems.append("C RAM error: " + self.c.lib.ae_ram_error(self.c.ram).decode())
        retkind = FUNCS[name][2]
        if retkind != "v" and (mret & M32) != (u32(cret) if cret is not None else 0):
            problems.append(f"return MIPS {mret & M32:08X}, C {u32(cret):08X}")
        if mm.log != cm.log:
            problems.append(f"hook calls differ\n      MIPS {mm.log}\n      C    {cm.log}")
        left, right = self.m.ram, self.c.main
        if bytes(left[:self.stack_lo]) != bytes(right[:self.stack_lo]):
            diffs = [i for i in range(self.stack_lo) if left[i] != right[i]]
            problems.append("RAM differs at " + ", ".join(
                f"{0x80000000 + i:08X} MIPS={left[i]:02X} C={right[i]:02X}" for i in diffs[:8]))
        if bytes(self.m.scratch) != bytes(self.c.scr):
            diffs = [i for i in range(0x400) if self.m.scratch[i] != self.c.scr[i]]
            problems.append("scratch differs at " + ", ".join(
                f"{0x1F800000 + i:08X} MIPS={self.m.scratch[i]:02X} C={self.c.scr[i]:02X}" for i in diffs[:8]))
        if problems:
            self.failures += 1
            print(f"FAIL {name} case {case}")
            for problem in problems:
                print("  " + problem)
        return not problems

    def finish(self, default_run=False, show_coverage=False):
        items = [(name, addr, size, True)
                 for name, (addr, size, _ret, _kinds) in FUNCS.items()
                 if name in self.counts]
        summary = reg.coverage_summary(items, self.m.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        if default_run:
            reg.write_results(self.counts, summary, not self.failures,
                              function_names=list(FUNCS))
        return summary

    def fail(self, message):
        self.failures += 1
        print("FAIL " + message)


def audit_calls():
    cpu = mr.MipsRam.from_extract(area=AREA_MEMBER)
    missing = []
    for name, (address, size, _ret, _kinds) in FUNCS.items():
        for pc in range(address, address + size, 4):
            word = cpu.read32(pc)
            if word >> 26 == 3:
                target = ((pc + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)
                if target not in TARGET_ADDRS and target not in HOOK_BY_ADDR:
                    missing.append((name, pc, target))
    if missing:
        for name, pc, target in missing:
            print(f"unmapped camera call: {name} at {pc:08X} -> {target:08X}")
        return 1
    print(f"all direct calls mapped: {len(HOOKS)} hooks, {len(FUNCS)} camera functions")
    return 0


def run_mutations():
    """Compile two independently corrupted copies of every function and require both to fail."""
    source = ROOT / "src/camera/ae_camera.c"
    original = source.read_text(encoding="utf-8")
    scratch_bytes = (0x1F8003F8, 0x1F8003F9)
    caught = 0
    misses = 0
    for name in FUNCS:
        function = re.search(r"\bae_%s\s*\([^)]*\)\s*\{" % re.escape(name), original)
        if function is None:
            raise RuntimeError(f"cannot locate definition of ae_{name} for mutation testing")
        function_caught = 0
        for index, address in enumerate(scratch_bytes, 1):
            mutation = (f"\n    ae_wr8(r, 0x{address:08X}u, "
                        f"ae_rd8(r, 0x{address:08X}u) ^ 0xA5u);\n")
            mutated = original[:function.end()] + mutation + original[function.end():]
            variant = OUT / f"camera_mut_{name}_{index}_{__import__('os').getpid()}.c"
            variant.write_text(mutated, encoding="utf-8")
            dll = build_dll(variant, tag=f"camera_mut_{name}_{index}")
            harness = Harness(dll)
            with contextlib.redirect_stdout(io.StringIO()):
                passed = harness.run(name, 0)
            if passed:
                misses += 1
                print(f"  MISSED {name} mutation {index}")
            else:
                caught += 1
                function_caught += 1
            del harness
            gc.collect()
            try:
                variant.unlink()
                dll.unlink()
            except OSError:
                pass
        print(f"  {name:36s} {function_caught}/2 caught")
    print(f"mutation checks: {caught}/52 caught")
    if misses:
        print(f"FAILED: {misses} mutation(s) escaped the checker")
        return 1
    print("OK: all 52 deliberate mutations were detected")
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=int, default=32,
                        help="number of cases per function (default 32)")
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--only", nargs="*", default=[])
    parser.add_argument("--audit-calls", action="store_true")
    parser.add_argument("--mutations", action="store_true",
                        help="compile and check two independent mutations per function")
    args = parser.parse_args()
    if args.audit_calls:
        return audit_calls()
    if args.mutations:
        return run_mutations()
    if not (ROOT / "extract/disc/SCUS_944.23").exists() or not AREA_MEMBER.exists():
        print(f"SKIP: EXE or real area member missing ({AREA_MEMBER})")
        return 0

    names = args.only or list(FUNCS)
    unknown = sorted(set(names) - set(FUNCS))
    if unknown:
        parser.error("unknown function(s): " + ", ".join(unknown))
    dll = build_dll()
    harness = Harness(dll)
    t0 = time.perf_counter()
    for name in names:
        count = max(1, args.cases)
        for case in range(count):
            harness.run(name, case)

    print("cases per function:")
    for name, count in sorted(harness.counts.items()):
        print(f"  {name:36s} {count}")
    print(f"total cases {sum(harness.counts.values())}; hooks compared {harness.hook_calls}; "
          f"MIPS instructions {harness.m.steps}; {time.perf_counter() - t0:.1f}s")
    default = not args.only and args.cases == parser.get_default("cases")
    summary = harness.finish(default_run=default, show_coverage=args.coverage)
    uncovered = {name: ent["uncovered"] for name, ent in summary.items()
                 if ent["own"] and ent["uncovered"]}
    if uncovered:
        print("coverage gaps (declare only proven-unreachable PCs):")
        for name, gaps in uncovered.items():
            print(f"  {name}: {gaps}")
    if harness.failures:
        print(f"FAILED: {harness.failures} mismatch(es)")
        return 1
    if uncovered:
        print("DIFFS PASS; coverage gaps remain")
        return 2
    print("OK: all camera functions match the original instructions with complete coverage")
    return 0


if __name__ == "__main__":
    sys.exit(main())
