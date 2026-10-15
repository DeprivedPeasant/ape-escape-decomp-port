"""Differentially check gimmick batch A against the original SCUS_944.23 code.

Usage: .venv/Scripts/python.exe tools/mips/check_gimmick_a.py [--coverage] [--mutate]
"""
import argparse
import ctypes as C
import gc
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/mips"), str(ROOT / "tools")]
import check_gimmick as prior_gimmick
import exclusion_registry as reg
import mips_ram as mr

FUNCS = {
    "GimmickSaveTableReset": (0x8005AD90, 104, False, []),
    "GimmickSaveStates": (0x8005ADF8, 340, False, []),
    "GimmickRestoreStates": (0x8005AF4C, 484, False, []),
    "GimmickSaveEntryAdd": (0x8005B130, 488, False, [C.c_int32, C.c_uint32]),
    "GimmickInitFromProgress": (0x8005B318, 360, True, []),
    "GimmickSaveApplyProgress": (0x8005B480, 184, True, []),
    "GimmickSpawnAll": (0x8005B538, 340, True, []),
    "GimmickUpdateAll": (0x8005B68C, 304, True, []),
    "GimmickGetState": (0x8005BB5C, 60, False, [C.c_int32]),
    "GimmickTypeToSfx": (0x8005BB98, 120, False, [C.c_int32]),
    "GimmickPlayTypeSfx": (0x8008EAE4, 116, True, [C.c_uint32]),
    "GimmickAnyBodyInside": (0x8008EB58, 280, True,
                              [C.c_uint32, C.c_int32, C.c_uint32]),
    "GimmickCarryBodiesByDelta": (0x8008AA6C, 324, True,
                                   [C.c_uint32, C.c_uint32, C.c_int32]),
    "GimmickCarryBodiesSwing": (0x8008ABB0, 356, True,
                                 [C.c_uint32, C.c_uint32, C.c_int32]),
    "GimmickSwingCarryPoint": (0x8008AD14, 644, False,
                               [C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32]),
    "GimmickSwingAngle": (0x8008AF98, 200, False, [C.c_int32, C.c_int32]),
    "GimmickSwingFirstStep": (0x8008B060, 456, False, [C.c_uint32]),
    "GimmickSwingAdvance": (0x8008B228, 380, False, [C.c_uint32]),
    "GimmickSwingProbeWall": (0x8008B3A4, 1108, True,
                              [C.c_uint32, C.c_int32, C.c_int32, C.c_uint32]),
    "Gimmick_SwingDoor_Init": (0x8008E5C0, 840, True, [C.c_uint32]),
    "Gimmick_SwingDoor_Update": (0x8008E90C, 472, True, [C.c_uint32]),
    "Gimmick_LiftDoor_Init": (0x8008EF60, 348, False, [C.c_uint32]),
    "Gimmick_LiftDoor_Update": (0x8008F0BC, 588, True, [C.c_uint32]),
    "Gimmick_Init_ThresholdDoor": (0x8008F468, 212, False, [C.c_uint32]),
    "Gimmick_ThresholdDoor_Update": (0x8008F53C, 484, True, [C.c_uint32]),
    "Gimmick_SlideDoor_Init": (0x8008F720, 436, True, [C.c_uint32]),
    "Gimmick_SlideDoor_Update": (0x8008F8D4, 508, True, [C.c_uint32]),
    "Gimmick_Switch_Init": (0x80090370, 400, True, [C.c_uint32]),
    "Gimmick_Switch_Update": (0x80090500, 164, True, [C.c_uint32]),
    "Gimmick_Init_PairToggle": (0x800905A4, 188, True, [C.c_uint32]),
    "Gimmick_Hit_Update": (0x80090660, 244, True, [C.c_uint32]),
    "Gimmick_Init_Counter": (0x800909A8, 84, False, [C.c_uint32]),
    "Gimmick_SpinRoller_Update": (0x800909FC, 880, False, [C.c_uint32]),
}

STACK_LO = 0x1FF000
GIMMICKS = 0x801B0000
COLLIDERS = 0x801C0000
PROPS = 0x801D0000
GROUPS = 0x801B1000
HANDLERS = 0x801B2000
HANDLER_INIT_FN = 0x801E0000
HANDLER_UPDATE_FN = 0x801E0010
SAVES = 0x800B9030
HOOK_MARKER = 0x800B1000
BOX_HIT_ADDRESS = 0x800B1010
GIMMICK_TOUCH_ADDRESS = 0x800B1030
STAGE_ENTRY_FLAGS = 0x800F4450
PLAYER_POSITION = 0x800EC204
RC_CAR_POSITION = 0x800F5A44
MONKEYS = 0x800E5500
MONKEY_SIZE = 0x250
MONKEY_ACTIVITY = 0x7A
DELTA = 0x801B3000
SWING_POSITION = 0x801B4000
SWING_OFFSET = 0x801B4010
SWING_OUT = 0x801B4020
PROP_OBJECT = 0x801D1000
PROP_MATRIX = 0x801D2000
GROUND_CONTROL = 0x800B1020
GROUND_SURFACE_CONTROL = 0x800B1024
GROUND_CONTROL_MAGIC = 0x53575052
GROUND_VALUES = 0x801B5000
GROUND_HEADER = 0x801B5100
WALL_OUT = 0x801B5200
AE_GA_COLLIDER_GEOMETRY = 0x10
AE_GA_COLLIDER_SWING_RANGE_Y = 0x2C
AE_GA_COLLIDER_SWING_RANGE_X = 0x2A
AE_GA_COLLIDER_SWING_RANGE_Z = 0x2E
AE_GA_COLLIDER_SWING_STEP = 0x36
AE_GIMMICK_COLLIDER = 0x04
AE_COLLIDER_HALF_Y = 0x08

HOOKS = [
    ("CameraNudge", 0x80047690, 3),
    ("BodyInsideBox", 0x8008EC70, 4),
    ("GimmickTouchCheck", 0x80090754, 2),
    ("MeshInitFromAreaTable", 0x80015CC8, 2),
    ("PropSetMesh", 0x80090E54, 4),
    ("PlaySfx", 0x800877F8, 1),
    ("PlaySfxAt", 0x80087A20, 2),
    ("SoundLoopStart", 0x800878E0, 2),
    ("SoundEmitterPan", 0x80087B34, 4),
    ("SoundLoopStop", 0x8008806C, 2),
    ("ProgressFlagGet", 0x8009632C, 1),
    ("ProgressFlagSet", 0x800962F4, 1),
    ("ProgressGetByte", 0x800963AC, 1),
    ("ProgressSetByte", 0x80096360, 2),
    ("GetSolidHeight", 0x8004D8C0, 3),
    ("SwingDoorBodyDelta", 0x8008F308, 2),
    ("SlideDoorMove", 0x8008FAD0, 3),
]
HOOK_COUNTS = {name: count for name, _, count in HOOKS}
HOOK_TAGS = {name: (i + 1) * 0x1021 for i, (name, _, _) in enumerate(HOOKS)}
HookFn = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, *([C.c_uint32] * 4))
HandlerFn = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                        C.c_uint32, C.c_uint32, C.c_uint32)


class CHooks(C.Structure):
    _fields_ = [("user", C.c_void_p)] + [(name, HookFn) for name, _, _ in HOOKS] + [
        ("GimmickHandler", HandlerFn), ("ground_stale_x", C.c_uint16),
        ("ground_stale_z", C.c_uint16),
    ]


def build(trapv=False, source_override=None, out_dir=None, tag="main"):
    out_dir = out_dir or ROOT / "extract/mips"
    out_dir.mkdir(parents=True, exist_ok=True)
    dll = out_dir / f"gimmick_a_{tag}_{os.getpid()}_{time.time_ns()}.dll"
    gcc = shutil.which("gcc")
    if not gcc:
        raise SystemExit("gcc was not found on PATH")
    sources = [
        ROOT / "src/core/ae_ram.c", ROOT / "src/core/ae_sdk.c",
        ROOT / "src/core/ae_sdk_matrix.c", ROOT / "src/world/ae_gimmick.c",
        ROOT / "src/world/ae_world.c", source_override or ROOT / "src/world/ae_gimmick_a.c",
    ]
    subprocess.run([
        gcc, "-std=c11", "-O1" if trapv else "-O2", *( ["-ftrapv"] if trapv else []),
        "-Wall", "-Wextra", "-Werror", "-shared", "-static-libgcc",
        "-I" + str(ROOT / "src/world"), "-I" + str(ROOT / "src/core"),
        *(str(source) for source in sources), "-o", str(dll),
    ], check=True)
    return dll


class HookModel:
    def __init__(self, memory):
        self.memory = memory
        self.log = []
        self.ground_calls = 0

    def hook(self, name, *raw_args):
        args = tuple(value & 0xFFFFFFFF for value in raw_args[:HOOK_COUNTS[name]])
        self.log.append((name, *args))
        marker = self.memory.read32(HOOK_MARKER)
        mixed = sum(((word * (i + 1)) & 0xFFFFFFFF) for i, word in enumerate(args))
        self.memory.write32(HOOK_MARKER, marker + mixed + HOOK_TAGS[name])
        if name == "ProgressFlagGet":
            return raw_args[0] & 1
        if name == "ProgressGetByte":
            return (raw_args[0] * 3 + 7) & 0xFF
        if name == "BodyInsideBox":
            return int(args[0] == self.memory.read32(BOX_HIT_ADDRESS))
        if name == "GimmickTouchCheck":
            return int(self.memory.read32(GIMMICK_TOUCH_ADDRESS) != 0)
        if name == "SoundLoopStart":
            return 0x3456
        if name == "GetSolidHeight":
            if self.memory.read32(GROUND_CONTROL) == GROUND_CONTROL_MAGIC:
                sample = self.ground_calls
                self.ground_calls += 1
                self.memory.write16(0x1F8003F8,
                                    self.memory.read16(GROUND_SURFACE_CONTROL))
                return C.c_int16(self.memory.read16(GROUND_VALUES + sample * 2)).value
            return -0x200
        return 0

    def handler(self, function, a0, a1):
        # Area handler entries point to one-argument init/update functions. $a1 is a
        # caller-saved value left by the preceding internal calls and is not an argument.
        args = (function & 0xFFFFFFFF, a0 & 0xFFFFFFFF)
        self.log.append(("GimmickHandler", *args))
        marker = self.memory.read32(HOOK_MARKER)
        mixed = sum(((word * (i + 1)) & 0xFFFFFFFF) for i, word in enumerate(args))
        self.memory.write32(HOOK_MARKER, marker + mixed + 0xE771)
        return 0


class MipsMemory:
    def __init__(self, cpu): self.cpu = cpu
    def read16(self, address): return self.cpu.read16(address)
    def read32(self, address): return self.cpu.read32(address)
    def write16(self, address, value): self.cpu.write16(address, value)
    def write32(self, address, value): self.cpu.write32(address, value)


class SideMemory:
    def __init__(self, side): self.side = side
    def read16(self, address): return self.side.lib.ae_rd16(self.side.ram, address)
    def read32(self, address): return self.side.lib.ae_rd32(self.side.ram, address)
    def write16(self, address, value): self.side.lib.ae_wr16(self.side.ram, address, value & 0xFFFF)
    def write32(self, address, value): self.side.lib.ae_wr32(self.side.ram, address, value & 0xFFFFFFFF)


class Side:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        P, U, I = C.c_void_p, C.c_uint32, C.c_int32
        self.lib.ae_ram_create.restype = P
        self.lib.ae_ptr.argtypes = [P, U, U]
        self.lib.ae_ptr.restype = P
        self.lib.ae_ram_clear_error.argtypes = [P]
        self.lib.ae_ram_failed.argtypes = [P]
        self.lib.ae_ram_failed.restype = I
        self.lib.ae_ram_error.argtypes = [P]
        self.lib.ae_ram_error.restype = C.c_char_p
        self.lib.ae_rd16.argtypes = [P, U]
        self.lib.ae_rd16.restype = U
        self.lib.ae_rd32.argtypes = [P, U]
        self.lib.ae_rd32.restype = U
        self.lib.ae_wr16.argtypes = [P, U, U]
        self.lib.ae_wr32.argtypes = [P, U, U]
        self.ram = self.lib.ae_ram_create()
        if not self.ram:
            raise SystemExit("ae_ram_create failed")
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(
            self.lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(
            self.lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.hooks = CHooks()
        self.model = None
        self.callbacks = {}
        for name, _, _ in HOOKS:
            cb = HookFn(lambda user, ram, a0, a1, a2, a3, n=name:
                        self.model.hook(n, a0, a1, a2, a3))
            self.callbacks[name] = cb
            setattr(self.hooks, name, cb)
        self.handler_callback = HandlerFn(
            lambda user, ram, function, a0, a1: self.model.handler(function, a0, a1))
        self.hooks.GimmickHandler = self.handler_callback
        for name, (address, size, has_hooks, argtypes) in FUNCS.items():
            f = getattr(self.lib, "ae_" + name)
            f.argtypes = [P] + ([P] if has_hooks else []) + argtypes
            f.restype = I

    def sync_from(self, cpu):
        C.memmove(self.main, bytes(cpu.ram), mr.RAM_SIZE)
        C.memmove(self.scratch, bytes(cpu.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)

    def call(self, name, args, hooks=None):
        f = getattr(self.lib, "ae_" + name)
        has_hooks = FUNCS[name][2]
        if has_hooks:
            return f(self.ram, C.byref(self.hooks if hooks is None else hooks), *args)
        return f(self.ram, *args)


class Harness:
    def __init__(self, side):
        _, area_blob, *_ = prior_gimmick.area_layout(0x0E)
        self.cpu = mr.MipsRam.from_extract(area=area_blob)
        self.cpu.coverage = set()
        self.cpu.write32(GROUND_CONTROL, 0)
        self.base = self.cpu.snapshot()
        self.side = side
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0
        reg.declare("stack_window", "801FF000..80200000")

    def run(self, name, args, setup=lambda cpu: None, *, label="", side=None):
        side = side or self.side
        self.cpu.restore(self.base)
        setup(self.cpu)
        side.sync_from(self.cpu)
        mips_model = HookModel(MipsMemory(self.cpu))
        c_model = HookModel(SideMemory(self.side))
        side.model = c_model
        original_hooks = {address: (lambda cpu, regs, n=hook_name:
                                    mips_model.hook(n, *regs[4:8]))
                          for hook_name, address, _ in HOOKS}
        for function in (HANDLER_INIT_FN, HANDLER_UPDATE_FN):
            original_hooks[function] = (lambda cpu, regs, fn=function:
                                        mips_model.handler(fn, regs[4], regs[5]))
        self.cpu.hook_log.clear()
        registers = self.cpu.call(FUNCS[name][0], args, hooks=original_hooks)
        cret = side.call(name, args)
        self.counts[name] += 1
        problems = []
        if side.lib.ae_ram_failed(side.ram):
            problems.append("C error: " + side.lib.ae_ram_error(side.ram).decode())
        if (registers[2] & 0xFFFFFFFF) != (cret & 0xFFFFFFFF):
            problems.append(f"return MIPS {registers[2] & 0xFFFFFFFF:08X} C {cret & 0xFFFFFFFF:08X}")
        if mips_model.log != c_model.log:
            problems.append(f"hook calls differ MIPS={mips_model.log} C={c_model.log}")
        mram, cram = bytes(self.cpu.ram[:STACK_LO]), bytes(side.main[:STACK_LO])
        if mram != cram:
            diffs = [i for i, (a, b) in enumerate(zip(mram, cram)) if a != b]
            problems.append("RAM differs at " + ", ".join(
                f"{0x80000000 + i:08X}:{mram[i]:02X}/{cram[i]:02X}" for i in diffs[:8]))
        if bytes(self.cpu.scratch) != bytes(side.scratch):
            problems.append("scratchpad differs")
        if problems:
            self.failures += 1
            print(f"FAIL {name} [{label or args}]")
            for problem in problems:
                print("  " + problem)
            return False
        return True

    def finish(self, default_run=False, show_coverage=False):
        summary = reg.coverage_summary(
            [(name, spec[0], spec[1], True) for name, spec in FUNCS.items()], self.cpu.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        if default_run:
            reg.write_results(self.counts, summary, not self.failures,
                              function_names=list(FUNCS))
        return summary


def put16(cpu, addr, value): cpu.write16(addr, value)
def put32(cpu, addr, value): cpu.write32(addr, value)


def baseline(h, seed=94423):
    rng = random.Random(seed)
    for exit_code in (0, 1, 2, 7):
        def setup(cpu, code=exit_code):
            cpu.write8(0x800F447C, code)
            cpu.write_bytes(SAVES, rng.randbytes(64 * 0x1E))
            for i in range(64):
                cpu.write16(SAVES + i * 0x1E, i & 1)
        h.run("GimmickSaveTableReset", (), setup, label=f"reset carry-over exit {exit_code}")

    put32(h.cpu, 0x800F4570, GIMMICKS)
    for index in [-1, -32768, 0, 1, 7, 31]:
        def setup(cpu, ix=index):
            cpu.write32(0x800F4570, GIMMICKS)
            for i in range(32): cpu.write16(GIMMICKS + i * 0x18 + 2, i * 3 - 20)
        h.run("GimmickGetState", (index,), setup, label=f"state index {index}")

    for kind in list(range(-8, 264)) + [0x7FFFFFFF, -0x80000000]:
        h.run("GimmickTypeToSfx", (kind,), label=f"sound type {kind}")

    ranges = [-32768, -4096, -31, -2, -1, 1, 2, 31, 4096, 32767]
    steps = [-32768, -4096, -2, -1, 0, 1, 2, 4096, 32767]
    for range_value in ranges:
        for step in steps:
            h.run("GimmickSwingAngle", (range_value, step), label=f"angle {range_value}/{step}")
    for _ in range(180):
        h.run("GimmickSwingAngle", (rng.choice(ranges), rng.randint(-32768, 32767)),
              label="random swing angle")

    for _ in range(240):
        geom = rng.choice([0x3000, 0x3001, 0x2000, 0x7000, 0xFFFF])
        collider = COLLIDERS
        def setup(cpu, g=geom):
            cpu.write32(0x800F455C, COLLIDERS)
            cpu.write16(collider + 0x10, g)
            for off in (0x2A, 0x2C, 0x2E): cpu.write16(collider + off, rng.choice([-5, -1, 0, 1, 5]))
            cpu.write16(collider + 0x36, rng.choice([-32768, -5, -1, 0, 1, 5, 32767]))
        h.run("GimmickSwingAdvance", (collider,), setup, label="swing axis / step sweep")

    def setup_swing_carry(cpu, *, ranges=(0, 0, 5), step=1,
                          position=(31, -47, 59), offset=(13, -19, 23)):
        cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_X, ranges[0])
        cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_Y, ranges[1])
        cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_Z, ranges[2])
        cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_STEP, step)
        for axis, value in enumerate(position): cpu.write16(SWING_POSITION + axis * 2, value)
        for axis, value in enumerate(offset): cpu.write16(SWING_OFFSET + axis * 2, value)
        for axis, value in enumerate((0x12345678, 0x23456789, 0x3456789A)):
            cpu.write32(SWING_OUT + axis * 4, value)

    for axis, ranges in (("Y", (0, 5, 0)), ("X", (5, 0, 0)), ("Z", (0, 0, 5))):
        for step in (-3, 0, 2):
            h.run("GimmickSwingCarryPoint", (COLLIDERS, SWING_POSITION, SWING_OFFSET, SWING_OUT),
                  lambda cpu, rr=ranges, ss=step: setup_swing_carry(cpu, ranges=rr, step=ss),
                  label=f"swing carry axis {axis} step {step}")
    for ranges, offset in (((0, 1, 0), (7, 11, -13)),
                           ((1, 0, 0), (7, 11, -13)),
                           ((0, 0, 1), (7, 11, -13)),
                           ((0, 0, 1), (-7, 11, -13))):
        h.run("GimmickSwingCarryPoint", (COLLIDERS, SWING_POSITION, SWING_OFFSET, SWING_OUT),
              lambda cpu, rr=ranges, oo=offset: setup_swing_carry(
                  cpu, ranges=rr, step=0, offset=oo),
              label="swing carry exact axis endpoint")
    h.run("GimmickSwingCarryPoint", (COLLIDERS, SWING_POSITION, SWING_OFFSET, SWING_OUT),
          lambda cpu: setup_swing_carry(cpu, ranges=(5, 7, 5)),
          label="swing carry Y axis takes priority")
    h.run("GimmickSwingCarryPoint", (COLLIDERS, SWING_POSITION, SWING_OFFSET, SWING_OUT),
          lambda cpu: setup_swing_carry(cpu, ranges=(0, 0, 0)),
          label="swing carry without an active axis")
    for _ in range(24):
        ranges = rng.choice(((0, rng.choice((-23, -1, 1, 23)), 0),
                             (rng.choice((-23, -1, 1, 23)), 0, 0),
                             (0, 0, rng.choice((-23, -1, 1, 23)))))
        position = tuple(rng.randint(-32768, 32767) for _ in range(3))
        offset = tuple(rng.randint(-32768, 32767) for _ in range(3))
        step = rng.randint(-32768, 32767)
        h.run("GimmickSwingCarryPoint", (COLLIDERS, SWING_POSITION, SWING_OFFSET, SWING_OUT),
              lambda cpu, rr=ranges, pp=position, oo=offset, ss=step: setup_swing_carry(
                  cpu, ranges=rr, position=pp, offset=oo, step=ss),
              label="random swing carry point")

    def setup_swing_bodies(cpu, *, geometry=0x3000, player_index=7, rc_index=7,
                           ranges=(0, 5, 0), step=2):
        cpu.write16(COLLIDERS + AE_GA_COLLIDER_GEOMETRY, geometry)
        cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_X, ranges[0])
        cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_Y, ranges[1])
        cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_Z, ranges[2])
        cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_STEP, step)
        cpu.write16(0x800EC1E0 + 0x58, player_index)
        cpu.write16(0x800F5A20 + 0x58, rc_index)
        for base, coords in ((PLAYER_POSITION, (120, 240, 360)),
                             (RC_CAR_POSITION, (80, 160, 320))):
            for axis, value in enumerate(coords): cpu.write32(base + axis * 4, value)
        for axis, value in enumerate((100, 200, 300)):
            cpu.write16(SWING_POSITION + axis * 2, value)

    h.run("GimmickCarryBodiesSwing", (COLLIDERS, SWING_POSITION, 7), setup_swing_bodies,
          label="swing carries player and RC car")
    h.run("GimmickCarryBodiesSwing", (COLLIDERS, SWING_POSITION, 7),
          lambda cpu: setup_swing_bodies(cpu, rc_index=-1),
          label="swing carries player only")
    h.run("GimmickCarryBodiesSwing", (COLLIDERS, SWING_POSITION, 7),
          lambda cpu: setup_swing_bodies(cpu, player_index=-1),
          label="swing carries RC car only")
    h.run("GimmickCarryBodiesSwing", (COLLIDERS, SWING_POSITION, 8),
          lambda cpu: setup_swing_bodies(cpu, player_index=-1, rc_index=-1),
          label="swing carries neither body")
    h.run("GimmickCarryBodiesSwing", (COLLIDERS, SWING_POSITION, 7),
          lambda cpu: setup_swing_bodies(cpu, geometry=0x2000),
          label="swing carry rejects non-swing collider")

    def setup_swing_first_step(cpu, *, ranges=(0, 5, 0), step=1, kind=3, geometry=0x3000):
        cpu.write32(0x800B4D64, PROPS)
        cpu.write32(PROPS, PROP_OBJECT)
        cpu.write32(PROPS + 0x1C, PROP_MATRIX)
        cpu.write32(PROP_OBJECT, kind)
        cpu.write32(0x800F455C, COLLIDERS)
        cpu.write16(COLLIDERS + 0x0E, 0)
        cpu.write16(COLLIDERS + AE_GA_COLLIDER_GEOMETRY, geometry)
        cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_X, ranges[0])
        cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_Y, ranges[1])
        cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_Z, ranges[2])
        cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_STEP, step)
        cpu.write_bytes(PROP_MATRIX, bytes(32))

    for ranges, step in (((0, 5, 0), 1), ((0, -5, 0), 1), ((0, -1, 0), 1),
                         ((5, 0, 0), 1), ((-1, 0, 0), 1),
                         ((0, 0, -5), 1), ((0, 0, -1), 1), ((0, 0, 0), 1)):
        h.run("GimmickSwingFirstStep", (COLLIDERS,),
              lambda cpu, rr=ranges, ss=step: setup_swing_first_step(cpu, ranges=rr, step=ss),
              label=f"swing first-step axis {ranges}")
    h.run("GimmickSwingFirstStep", (COLLIDERS,),
          lambda cpu: setup_swing_first_step(cpu, kind=2),
          label="swing first-step rejects prop kind")
    h.run("GimmickSwingFirstStep", (COLLIDERS,),
          lambda cpu: setup_swing_first_step(cpu, geometry=0x2000),
          label="swing first-step rejects collider geometry")

    def setup_probe_wall(cpu, *, samples=(10, 20, 30, 40, 50), wall_y=-32768,
                         surface=0, controlled=True,
                         origin_xz=(100, 200), half=(20, 30, 16)):
        cpu.write16(COLLIDERS + 0x00, origin_xz[0])
        cpu.write16(COLLIDERS + 0x02, wall_y)
        cpu.write16(COLLIDERS + 0x04, origin_xz[1])
        cpu.write16(COLLIDERS + 0x06, half[0])
        cpu.write16(COLLIDERS + 0x08, half[1])
        cpu.write16(COLLIDERS + 0x0A, half[2])
        cpu.write16(WALL_OUT, 0x4567)
        if controlled:
            cpu.write32(0x800F44E4, GROUND_HEADER)
            cpu.write32(GROUND_HEADER, 2)
            cpu.write32(GROUND_CONTROL, GROUND_CONTROL_MAGIC)
            cpu.write16(GROUND_SURFACE_CONTROL, surface)
            for i, value in enumerate(samples): cpu.write16(GROUND_VALUES + i * 2, value)
        else:
            cpu.write32(GROUND_CONTROL, 0)

    for target in range(4):
        samples = [10, 10, 10, 10, 10]
        samples[target] = -10
        h.run("GimmickSwingProbeWall", (COLLIDERS, 4, -6, WALL_OUT),
              lambda cpu, vals=tuple(samples): setup_probe_wall(cpu, samples=vals, wall_y=0),
              label=f"wall probe early return at corner {target}")
    for samples in ((10, 20, 30, 40, 50), (50, 40, 30, 20, 10),
                    (30, 10, 50, 20, 40), (30, 40, 10, 20, 50),
                    (30, 40, 50, 10, 20)):
        h.run("GimmickSwingProbeWall", (COLLIDERS, 0x10001, -0x10006, WALL_OUT),
              lambda cpu, vals=samples: setup_probe_wall(cpu, samples=vals),
              label=f"wall probe select lowest of {samples}")
    h.run("GimmickSwingProbeWall", (COLLIDERS, 4, -6, WALL_OUT),
          lambda cpu: setup_probe_wall(cpu, samples=(0, 0, 0, 0, 0),
                                       wall_y=0, surface=1),
          label="wall probe special surface adjustment")
    h.run("GimmickSwingProbeWall", (COLLIDERS, 4, -6, WALL_OUT),
          lambda cpu: setup_probe_wall(cpu, controlled=False, origin_xz=(0, 0), half=(10, 10, 10)),
          label="wall probe against real area member")

    for i in range(100):
        gimmick = GIMMICKS + (i % 8) * 0x18
        def setup(cpu, ix=i % 8):
            cpu.write32(0x800F455C, COLLIDERS)
            cpu.write16(gimmick + 4, ix)
            cpu.write16(gimmick + 6, 0x5555)
            cpu.write16(gimmick + 8, -20 + ix * 4)
            cpu.write16(gimmick + 0x12, 0x7FFF)
            cpu.write16(gimmick + 0x14, 0x1234)
            cpu.write16(gimmick + 0x16, 0xABCD)
            cpu.write16(COLLIDERS + ix * 0x38 + 8, rng.randint(-32768, 32767))
        h.run("Gimmick_Init_Counter", (gimmick,), setup, label="counter init values")

    def setup_progress(cpu):
        cpu.write16(0x800F4474, 5)
        cpu.write16(0x800F4476, 5)
        cpu.write32(0x800F4570, GIMMICKS)
        cpu.write32(0x800F456C, GROUPS)
        cpu.write32(0x800F45D8, 2)
        cpu.write32(0x800AF868 + 5 * 4, HANDLERS)
        cpu.write32(HANDLERS, 0x80012340)
        cpu.write32(HANDLERS + 12, 0x80012340)
        cpu.write32(HANDLERS + 24, 0)
        for group, (count, first) in enumerate(((4, 0), (3, 4))):
            at = GROUPS + group * 8
            cpu.write16(at, count)
            cpu.write16(at + 4, first)
        for i, kind in enumerate((0x22, 0x32, 0x22, 2, 0x32, 0x22, 0x32)):
            record = GIMMICKS + i * 0x18
            cpu.write16(record, kind)
            cpu.write16(record + 2, rng.choice([-1, 0, 1, 5]))
            cpu.write16(record + 8, rng.randint(-20, 20))
            cpu.write16(record + 0x0A, i + 1)
            cpu.write16(record + 0x12, i + 10)
    h.run("GimmickInitFromProgress", (), setup_progress, label="progress-linked area groups")
    h.run("GimmickInitFromProgress", (), lambda cpu: cpu.write16(0x800F4474, 0x20),
          label="progress skip area 20")

    def setup_saved_progress(cpu):
        cpu.write16(0x800F4476, 5)
        cpu.write_bytes(SAVES, bytes(64 * 0x1E))
        for i in range(64):
            entry = SAVES + i * 0x1E
            cpu.write16(entry, 0 if i % 7 else 0xFFFF)
            cpu.write16(entry + 2, 5 if i % 9 else 4)
            cpu.write16(entry + 6, (0x22, 0x32, 0x03)[i % 3])
            cpu.write16(entry + 0x0E, i + 2)
            cpu.write16(entry + 0x10, i % 2)
            cpu.write16(entry + 0x18, i + 10)
    h.run("GimmickSaveApplyProgress", (), setup_saved_progress, label="saved progress application")

    def setup_save_entry(cpu, kind, state=1, subtype=0x1234, *, full=False):
        cpu.write16(0x800F4476, 5)
        cpu.write_bytes(SAVES, bytes(64 * 0x1E))
        for i in range(64): cpu.write16(SAVES + i * 0x1E, 1 if full else 0xFFFF)
        gimmick = GIMMICKS
        cpu.write16(gimmick, kind)
        cpu.write16(gimmick + 2, state)
        cpu.write16(gimmick + 8, subtype)
        for off, value in ((0x0A, 0x1234), (0x0C, 0x2345), (0x0E, 0x3456),
                           (0x10, 0x4567), (0x12, 0x5678), (0x14, 0x6789), (0x16, 0x789A)):
            cpu.write16(gimmick + off, value)

    for kind in range(2, 0x33):
        h.run("GimmickSaveEntryAdd", (kind, GIMMICKS),
              lambda cpu, t=kind: setup_save_entry(cpu, t),
              label=f"save-entry type {kind:02X}")
    for kind in (0x27, 0x32):
        for state in range(7):
            h.run("GimmickSaveEntryAdd", (kind, GIMMICKS),
                  lambda cpu, t=kind, s=state: setup_save_entry(cpu, t, s),
                  label=f"save-entry type {kind:02X} state {state}")
    h.run("GimmickSaveEntryAdd", (0x06, GIMMICKS),
          lambda cpu: setup_save_entry(cpu, 0x06, 3), label="save-entry type 06 state three")
    h.run("GimmickSaveEntryAdd", (0x27, GIMMICKS),
          lambda cpu: setup_save_entry(cpu, 0x27, 4), label="save-entry type 27 state four")
    h.run("GimmickSaveEntryAdd", (0x02, GIMMICKS),
          lambda cpu: setup_save_entry(cpu, 0x02, full=True), label="save-entry table full")

    def setup_save_states(cpu, type2_subtype):
        cpu.write16(0x800F4474, 5)
        cpu.write16(0x800F4476, 5)
        cpu.write32(0x800F4570, GIMMICKS)
        cpu.write32(0x800F456C, GROUPS)
        cpu.write32(0x800F45D8, 1)
        cpu.write16(GROUPS, 0x31)
        cpu.write16(GROUPS + 4, 0)
        cpu.write_bytes(SAVES, bytes(64 * 0x1E))
        for i in range(64): cpu.write16(SAVES + i * 0x1E, 0xFFFF)
        for i, kind in enumerate(range(2, 0x33)):
            gimmick = GIMMICKS + i * 0x18
            cpu.write16(gimmick, kind)
            cpu.write16(gimmick + 8, type2_subtype if kind == 2 else (i % 7))
    h.run("GimmickSaveStates", (), lambda cpu: setup_save_states(cpu, 0x37),
          label="save-state type dispatch and transient subtype")
    h.run("GimmickSaveStates", (), lambda cpu: setup_save_states(cpu, 0x36),
          label="save-state type 02 persistent subtype")

    def setup_restore_states(cpu, *, mismatch=False):
        cpu.write16(0x800F4476, 5)
        cpu.write32(0x800F4570, GIMMICKS)
        cpu.write_bytes(SAVES, rng.randbytes(64 * 0x1E))
        for i, kind in enumerate(range(2, 0x33)):
            entry = SAVES + i * 0x1E
            cpu.write16(entry, 1)
            cpu.write16(entry + 2, 4 if mismatch and i == 0 else 5)
            cpu.write16(entry + 4, i)
            cpu.write16(entry + 6, kind)
            gimmick = GIMMICKS + i * 0x18
            cpu.write_bytes(gimmick, rng.randbytes(0x18))
    h.run("GimmickRestoreStates", (), setup_restore_states, label="restore saved types")
    h.run("GimmickRestoreStates", (), lambda cpu: setup_restore_states(cpu, mismatch=True),
          label="restore unused and other-area entries")

    def setup_spawn(cpu, *, saved=False):
        cpu.write16(0x800F4474, 5)
        cpu.write16(0x800F4476, 5)
        cpu.write32(0x800AF868 + 5 * 4, HANDLERS)
        cpu.write_bytes(HANDLERS, bytes(3 * 12))
        cpu.write32(HANDLERS, HANDLER_INIT_FN)
        cpu.write32(HANDLERS + 12, HANDLER_INIT_FN)
        cpu.write32(0x800F4570, GIMMICKS)
        cpu.write32(0x800F456C, GROUPS)
        cpu.write32(0x800F45D8, 1)
        cpu.write16(GROUPS, 2)
        cpu.write16(GROUPS + 4, 0)
        cpu.write16(GIMMICKS, 2)
        cpu.write16(GIMMICKS + 0x18, 3)
        cpu.write_bytes(SAVES, bytes(64 * 0x1E))
        for i in range(64): cpu.write16(SAVES + i * 0x1E, 0xFFFF)
        if saved:
            cpu.write16(SAVES, 1)
            cpu.write16(SAVES + 2, 5)
            cpu.write16(SAVES + 4, 0)
            cpu.write16(SAVES + 6, 2)
            cpu.write16(SAVES + 0x10, 0x7777)
    h.run("GimmickSpawnAll", (), setup_spawn, label="spawn handler groups and sentinel")
    h.run("GimmickSpawnAll", (), lambda cpu: setup_spawn(cpu, saved=True),
          label="spawn restores saved progress first")
    def setup_spawn_without_handlers(cpu):
        setup_spawn(cpu)
        cpu.write32(0x800AF868 + 5 * 4, 0)
    h.run("GimmickSpawnAll", (), setup_spawn_without_handlers,
          label="spawn clears group count without area handlers")

    def setup_update(cpu, *, flags=0, no_update=False, no_area_handlers=False):
        cpu.write16(0x800F4476, 5)
        cpu.write32(0x800AF868 + 5 * 4, 0 if no_area_handlers else HANDLERS)
        cpu.write_bytes(HANDLERS, bytes(3 * 12))
        if not no_update:
            cpu.write32(HANDLERS + 4, HANDLER_UPDATE_FN)
            cpu.write32(HANDLERS + 12 + 4, HANDLER_UPDATE_FN)
        cpu.write32(HANDLERS + 8, GROUPS)
        cpu.write32(HANDLERS + 12 + 8, 0)
        cpu.write32(0x800F4570, GIMMICKS)
        cpu.write16(0x800F4450, flags)
        cpu.write16(GROUPS, 2)
        cpu.write16(GROUPS + 4, 0)
        cpu.write16(GIMMICKS, 2)
        cpu.write16(GIMMICKS + 0x18, 3)
    h.run("GimmickUpdateAll", (), setup_update, label="update grouped and null-state handlers")
    h.run("GimmickUpdateAll", (), lambda cpu: setup_update(cpu, flags=8),
          label="update area suppression flag")
    h.run("GimmickUpdateAll", (), lambda cpu: setup_update(cpu, no_update=True),
          label="update missing first callback")
    h.run("GimmickUpdateAll", (), lambda cpu: setup_update(cpu, no_area_handlers=True),
          label="update missing area handler table")

    def setup_play_sfx(cpu, *, flags=0, type_value=2):
        cpu.write16(COLLIDERS + 0x16, type_value)
        cpu.write32(STAGE_ENTRY_FLAGS, flags)
    h.run("GimmickPlayTypeSfx", (COLLIDERS,), setup_play_sfx,
          label="type sound with collider position")
    h.run("GimmickPlayTypeSfx", (COLLIDERS,), lambda cpu: setup_play_sfx(cpu, flags=1),
          label="type sound without collider position")
    h.run("GimmickPlayTypeSfx", (COLLIDERS,), lambda cpu: setup_play_sfx(cpu, type_value=8),
          label="type sound with invalid sound mapping")

    def setup_inside(cpu, *, hit=0, rc_active=False, radius=5):
        cpu.write32(BOX_HIT_ADDRESS, hit)
        cpu.write32(0x800F5A20 + 0x70, 1 if rc_active else 0)
        for slot in range(8):
            cpu.write8(MONKEYS + slot * MONKEY_SIZE + MONKEY_ACTIVITY, 4)
    h.run("GimmickAnyBodyInside", (COLLIDERS, 5, 0x12345678),
          lambda cpu: setup_inside(cpu, hit=PLAYER_POSITION), label="player inside")
    h.run("GimmickAnyBodyInside", (COLLIDERS, 5, 0x12345678),
          lambda cpu: setup_inside(cpu, hit=RC_CAR_POSITION, rc_active=True), label="RC car inside")
    h.run("GimmickAnyBodyInside", (COLLIDERS, 5, 0x12345678),
          lambda cpu: setup_inside(cpu, hit=MONKEYS + 0x24), label="active monkey inside")
    h.run("GimmickAnyBodyInside", (COLLIDERS, 0x10001, 0x12345678),
          lambda cpu: setup_inside(cpu, rc_active=True), label="all body probes miss with narrowed radius")

    def setup_delta(cpu, *, index=7, player_index=7, rc_index=7, delta=(12, -34, 56),
                    player_pos=(100, 200, 300), rc_pos=(-100, -200, -300)):
        cpu.write16(0x800EC1E0 + 0x58, player_index)
        cpu.write16(0x800F5A20 + 0x58, rc_index)
        for base, values in ((PLAYER_POSITION, player_pos), (RC_CAR_POSITION, rc_pos)):
            for axis, value in enumerate(values): cpu.write32(base + axis * 4, value)
        for axis, value in enumerate(delta): cpu.write16(DELTA + axis * 2, value)
    h.run("GimmickCarryBodiesByDelta", (GIMMICKS, DELTA, 7), setup_delta,
          label="carry player and RC car with camera nudge")
    h.run("GimmickCarryBodiesByDelta", (GIMMICKS, DELTA, 8),
          lambda cpu: setup_delta(cpu, player_index=-1, rc_index=8),
          label="carry RC car only")
    h.run("GimmickCarryBodiesByDelta", (GIMMICKS, DELTA, 7),
          lambda cpu: setup_delta(cpu, delta=(512, -1024, -512)),
          label="inclusive maximum carry delta")
    h.run("GimmickCarryBodiesByDelta", (GIMMICKS, DELTA, 7),
          lambda cpu: setup_delta(cpu, delta=(-512, 0, 0)),
          label="negative carry delta")
    for delta in ((513, 0, 0), (0, 1025, 0), (0, 0, -513)):
        h.run("GimmickCarryBodiesByDelta", (GIMMICKS, DELTA, 7),
              lambda cpu, d=delta: setup_delta(cpu, delta=d),
              label=f"reject carry delta {delta}")

    for direction in range(8):
        case = f"swing_init_{direction}"
        h.run("Gimmick_SwingDoor_Init", (GIMMICKS,), mutation_setup(case), label=case)
    for name in ("swing_init_invalid", "swing_init_type2_unlinked",
                 "swing_init_type1_unlinked", "swing_init_type1_linked"):
        h.run("Gimmick_SwingDoor_Init", (GIMMICKS,), mutation_setup(name), label=name)

    for name in ("swing_update_idle_empty", "swing_update_idle_inside",
                 "swing_update_opening_sound", "swing_update_open_boundary",
                 "swing_update_open_finish", "swing_update_open_finish_type1",
                 "swing_update_open_threshold_edge",
                 "swing_update_closing_sound", "swing_update_close_finish",
                 "swing_update_close_finish_type1", "swing_update_close_zero",
                 "swing_update_state1", "swing_update_state4"):
        h.run("Gimmick_SwingDoor_Update", (GIMMICKS,), mutation_setup(name), label=name)

    for name in ("lift_init_type10_linked", "lift_init_type12_linked",
                 "lift_init_type10_unlinked", "lift_init_other_linked", "lift_init_other_unlinked",
                 "lift_init_type1_other_linked"):
        h.run("Gimmick_LiftDoor_Init", (GIMMICKS,), mutation_setup(name), label=name)
    for name in ("lift_update_idle_empty", "lift_update_idle_inside",
                 "lift_update_opening_sound", "lift_update_opening_move",
                 "lift_update_open_boundary", "lift_update_open_finish",
                 "lift_update_open_finish_type1", "lift_update_closing_sound",
                 "lift_update_close_finish", "lift_update_close_sound_edge",
                 "lift_update_close_finish_type1", "lift_update_close_negative_half",
                 "lift_update_close_zero",
                 "lift_update_state1", "lift_update_state4"):
        h.run("Gimmick_LiftDoor_Update", (GIMMICKS,), mutation_setup(name), label=name)

    h.run("Gimmick_Init_ThresholdDoor", (GIMMICKS,), mutation_setup("threshold_init"),
          label="threshold door init")
    h.run("Gimmick_Init_ThresholdDoor", (GIMMICKS,), mutation_setup("threshold_init_unlinked"),
          label="threshold door unlinked init")
    for name in ("threshold_update_below", "threshold_update_equal",
                 "threshold_update_start_loop",
                 "threshold_update_pan_loop", "threshold_update_stop_loop",
                 "threshold_update_stop_loop_state5"):
        h.run("Gimmick_ThresholdDoor_Update", (GIMMICKS,), mutation_setup(name), label=name)

    for direction in range(8):
        case = f"slide_init_{direction}"
        h.run("Gimmick_SlideDoor_Init", (GIMMICKS,), mutation_setup(case), label=case)
    for name in ("slide_init_invalid", "slide_init_type1", "slide_init_unlinked"):
        h.run("Gimmick_SlideDoor_Init", (GIMMICKS,), mutation_setup(name), label=name)
    for name in ("slide_update_idle_empty", "slide_update_idle_inside",
                 "slide_update_opening_sound", "slide_update_open_boundary",
                 "slide_update_open_finish", "slide_update_open_finish_type1",
                 "slide_update_closing_sound", "slide_update_close_sound_edge",
                 "slide_update_close_negative_distance", "slide_update_close_finish",
                 "slide_update_close_finish_type1",
                 "slide_update_state1", "slide_update_state4"):
        h.run("Gimmick_SlideDoor_Update", (GIMMICKS,), mutation_setup(name), label=name)

    for name in ("switch_init_first_zero", "switch_init_first_one",
                 "switch_init_second_zero", "switch_init_second_one",
                 "switch_init_second_other", "switch_init_invalid"):
        h.run("Gimmick_Switch_Init", (GIMMICKS,), mutation_setup(name), label=name)
    for name in ("switch_update_touched", "switch_update_untouched", "switch_update_state1"):
        h.run("Gimmick_Switch_Update", (GIMMICKS,), mutation_setup(name), label=name)
    for name in ("pair_toggle_zero", "pair_toggle_one"):
        h.run("Gimmick_Init_PairToggle", (GIMMICKS,), mutation_setup(name), label=name)
    for name in ("hit_update_zero_touch", "hit_update_zero_miss",
                 "hit_update_one_release", "hit_update_one_touch", "hit_update_state2"):
        h.run("Gimmick_Hit_Update", (GIMMICKS,), mutation_setup(name), label=name)
    for name in ("roller_type37", "roller_type38", "roller_type39", "roller_type3a",
                 "roller_invalid", "roller_decay", "roller_stopped", "roller_touch_cap",
                 "roller_negative_restore", "roller_limit_reached", "roller_subtype_above",
                 "roller_type37_negative", "roller_type38_positive",
                 "roller_type39_positive", "roller_type3a_negative"):
        h.run("Gimmick_SpinRoller_Update", (GIMMICKS,), mutation_setup(name), label=name)


def mutation_sources():
    return [
        ("reset entry field", "GimmickSaveTableReset",
         "entry + AE_GA_SAVE_ENTRY_USED, 0xFFFFu", "entry + AE_GA_SAVE_ENTRY_STAGE, 0xFFFFu",
         (), "reset"),
        ("reset carry-over branch", "GimmickSaveTableReset",
         "!= 2u) {", "== 2u) {", (), "reset_exit"),
        ("save transient type 02", "GimmickSaveStates",
         "persistent = subtype < 0x37 || subtype > 0x3A;", "persistent = subtype < 0 || subtype >= 0;",
         (), "save_states"),
        ("save type 04", "GimmickSaveStates",
         "case 0x03: case 0x04: case 0x05:", "case 0x03: case 0x05:",
         (), "save_states"),
        ("restore area filter", "GimmickRestoreStates",
         "!= current_area) continue;", "== current_area) continue;",
         (), "restore"),
        ("restore type 02 field", "GimmickRestoreStates",
         "gimmick + AE_GIMMICK_PARAM_A,\n                    ae_rd16(r, entry + AE_GA_SAVE_ENTRY_FIELD_10)",
         "gimmick + AE_GIMMICK_SUBTYPE,\n                    ae_rd16(r, entry + AE_GA_SAVE_ENTRY_FIELD_10)",
         (), "restore"),
        ("save-entry field 10", "GimmickSaveEntryAdd",
         "case 0x23:\n            ae_wr16(r, entry + AE_GA_SAVE_ENTRY_FIELD_10, state);",
         "case 0x23:\n            ae_wr16(r, entry + AE_GA_SAVE_ENTRY_FIELD_12, state);",
         (2, GIMMICKS), "save_entry_used"),
        ("save-entry occupied test", "GimmickSaveEntryAdd",
         "AE_GA_SAVE_ENTRY_USED) >= 0", "AE_GA_SAVE_ENTRY_USED) > 0",
         (2, GIMMICKS), "save_entry_used"),
        ("spawn handler gate", "GimmickSpawnAll",
         "if (handlers == 0) {", "if (handlers != 0) {", (), "spawn"),
        ("spawn restore states", "GimmickSpawnAll",
         "ae_GimmickRestoreStates(r);", "(void)0;", (), "spawn_saved"),
        ("update group state reset", "GimmickUpdateAll",
         "ae_wr16(r, AE_GA_UPDATE_GROUP_STATE, 0xFFFFu);",
         "ae_wr16(r, AE_GA_UPDATE_GROUP_STATE, 0u);", (), "update"),
        ("update suppression bit", "GimmickUpdateAll",
         "AE_GA_STAGE_ENTRY_FLAGS) & 8u) != 0", "AE_GA_STAGE_ENTRY_FLAGS) & 0u) != 0",
         (), "update_flags"),
        ("type sound mode bit", "GimmickPlayTypeSfx",
         "AE_GA_STAGE_ENTRY_FLAGS) & 1u) != 0", "AE_GA_STAGE_ENTRY_FLAGS) & 2u) != 0",
         (COLLIDERS,), "play_type"),
        ("type sound field", "GimmickPlayTypeSfx",
         "collider + AE_GA_COLLIDER_SFX_TYPE", "collider + AE_GA_COLLIDER_GEOMETRY",
         (COLLIDERS,), "play_field"),
        ("inside radius narrowing", "GimmickAnyBodyInside",
         "int32_t radius = ga_s16((uint32_t)radius_arg);",
         "int32_t radius = radius_arg;", (COLLIDERS, 0x10001, 0x12345678), "inside_radius"),
        ("inside monkey activity", "GimmickAnyBodyInside",
         "AE_GA_MONKEY_ACTIVITY) != 4u", "AE_GA_MONKEY_ACTIVITY) != 3u",
         (COLLIDERS, 5, 0x12345678), "inside_monkey"),
        ("carry delta threshold", "GimmickCarryBodiesByDelta",
         "abs_dx >= 513", "abs_dx > 513",
         (GIMMICKS, DELTA, 7), "delta_range"),
        ("carry camera nudge", "GimmickCarryBodiesByDelta",
         "GA_CALL(CameraNudge, (uint32_t)dx, (uint32_t)dy, (uint32_t)dz, 0);",
         "GA_CALL(CameraNudge, (uint32_t)(dx + 1), (uint32_t)dy, (uint32_t)dz, 0);",
         (GIMMICKS, DELTA, 7), "delta_player"),
        ("swing body geometry", "GimmickCarryBodiesSwing",
         "& 0xF000u) != 0x3000u) return 0;", "& 0xF000u) != 0x2000u) return 0;",
         (COLLIDERS, SWING_POSITION, 7), "swing_body_geometry"),
        ("swing body camera nudge", "GimmickCarryBodiesSwing",
         "GA_CALL(CameraNudge, (uint32_t)moved_x, (uint32_t)moved_y, (uint32_t)moved_z, 0);",
         "GA_CALL(CameraNudge, (uint32_t)(moved_x + 1), (uint32_t)moved_y, (uint32_t)moved_z, 0);",
         (COLLIDERS, SWING_POSITION, 7), "swing_body_camera"),
        ("swing carry axis priority", "GimmickSwingCarryPoint",
         "AE_GA_COLLIDER_SWING_RANGE_Y);", "AE_GA_COLLIDER_SWING_RANGE_X);",
         (COLLIDERS, SWING_POSITION, SWING_OFFSET, SWING_OUT), "swing_carry_priority"),
        ("swing carry rotation sign", "GimmickSwingCarryPoint",
         "offset_x * cosine - offset_z * sine", "offset_x * cosine + offset_z * sine",
         (COLLIDERS, SWING_POSITION, SWING_OFFSET, SWING_OUT), "swing_carry_rotation"),
        ("swing first-step positive sign", "GimmickSwingFirstStep",
         "angle = range > 0 ? -quotient : quotient;\n        ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_X, 0);",
         "angle = range > 0 ? quotient : quotient;\n        ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_X, 0);",
         (COLLIDERS,), "swing_first_positive"),
        ("swing first-step Z sign", "GimmickSwingFirstStep",
         "angle = range < 0 ? -quotient : quotient;", "angle = range < 0 ? quotient : -quotient;",
         (COLLIDERS,), "swing_first_z_sign"),
        ("swing wall early hit", "GimmickSwingProbeWall",
         "i < 4 && ground < wall_y", "i < 4 && ground > wall_y",
         (COLLIDERS, 4, -6, WALL_OUT), "probe_wall_early"),
        ("swing wall minimum", "GimmickSwingProbeWall",
         "if (height < lowest) lowest = height;", "if (height > lowest) lowest = height;",
         (COLLIDERS, 4, -6, WALL_OUT), "probe_wall_min"),
        ("progress type 22", "GimmickInitFromProgress",
         "if (type == 0x22)", "if (type == 0x23)", (), "progress_init"),
        ("progress flag sense", "GimmickInitFromProgress",
         "if (enabled != 0) ae_wr16", "if (enabled == 0) ae_wr16", (), "progress_init"),
        ("apply progress type", "GimmickSaveApplyProgress",
         "type == 0x22 &&", "type == 0x23 &&", (), "progress_apply"),
        ("apply progress condition", "GimmickSaveApplyProgress",
         "AE_GA_SAVE_ENTRY_FIELD_10) != 0", "AE_GA_SAVE_ENTRY_FIELD_10) == 0",
         (), "progress_apply"),
        ("apply progress byte type", "GimmickSaveApplyProgress",
         "if (type == 0x32)", "if (type == 0x33)", (), "progress_apply_byte"),
        ("state rejects zero", "GimmickGetState",
         "if (index < 0) return -1;", "if (index <= 0) return -1;", (0,), "state"),
        ("state field", "GimmickGetState",
         "record + AE_GIMMICK_STATE", "record + AE_GIMMICK_TYPE", (0,), "state"),
        ("sound table value", "GimmickTypeToSfx",
         "        130, 130, 129, 129, 128, 128, -1, 42,",
         "        131, 130, 129, 129, 128, 128, -1, 42,", (2,), "none"),
        ("sound type mask", "GimmickTypeToSfx",
         "(uint32_t)type & 0x1Fu", "(uint32_t)type & 0x0Fu", (18,), "none"),
        ("swing angle first rounding", "GimmickSwingAngle",
         "if (adjusted_step < 0) adjusted_step += range;",
         "if (adjusted_step <= 0) adjusted_step += range;", (5, 1), "none"),
        ("swing angle negative sign", "GimmickSwingAngle",
         "if (range < 0) {\n        first_quotient = -first_quotient;",
         "if (range > 0) {\n        first_quotient = -first_quotient;", (-5, 1), "none"),
        ("swing axis order", "GimmickSwingAdvance",
         "collider + AE_GA_COLLIDER_SWING_RANGE_Y);",
         "collider + AE_GA_COLLIDER_SWING_RANGE_X);", (COLLIDERS,), "swing_advance"),
        ("swing boundary wrap", "GimmickSwingAdvance",
         "range > 0 && range < step", "range > 0 && range <= step", (COLLIDERS,), "swing_advance_edge"),
        ("counter initial state", "Gimmick_Init_Counter",
         "AE_GIMMICK_PARAM_12, 0xFFFFu", "AE_GIMMICK_PARAM_12, 0u", (GIMMICKS,), "counter"),
        ("counter travel", "Gimmick_Init_Counter",
         "ae_rds16(r, collider + AE_COLLIDER_HALF_Y) * 2",
         "ae_rds16(r, collider + AE_COLLIDER_HALF_Y)", (GIMMICKS,), "counter"),
        ("swing-door direction-zero hinge", "Gimmick_SwingDoor_Init",
         "case 0:\n        new_x = (uint16_t)(x - half_x);",
         "case 0:\n        new_x = (uint16_t)(x + half_x);",
         (GIMMICKS,), "swing_init_0"),
        ("swing-door initial pivot mode", "Gimmick_SwingDoor_Init",
         "ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE), 2);",
         "ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE), 5);",
         (GIMMICKS,), "swing_init_0"),
        ("swing-door sound frame", "Gimmick_SwingDoor_Update",
         "if (timer == 192)", "if (timer == 224)",
         (GIMMICKS,), "swing_update_opening_sound"),
        ("swing-door completed opening threshold", "Gimmick_SwingDoor_Update",
         "if (timer < 1025) return 0;", "if (timer <= 1025) return 0;",
         (GIMMICKS,), "swing_update_open_threshold_edge"),
        ("lift-door subtype-12 travel", "Gimmick_LiftDoor_Init",
         "subtype == 10 || subtype == 12", "subtype == 10 || subtype == 13",
         (GIMMICKS,), "lift_init_type12_linked"),
        ("lift-door linked initial state", "Gimmick_LiftDoor_Init",
         "ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);",
         "ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);",
         (GIMMICKS,), "lift_init_type10_linked"),
        ("lift-door exact height endpoint", "Gimmick_LiftDoor_Update",
         "if (timer > half_y)", "if (timer >= half_y)",
         (GIMMICKS,), "lift_update_open_boundary"),
        ("lift-door type-one completed state", "Gimmick_LiftDoor_Update",
         "ae_rds16(r, gimmick + AE_GIMMICK_TYPE) == 1 ? 0u : 1u",
         "ae_rds16(r, gimmick + AE_GIMMICK_TYPE) == 1 ? 1u : 1u",
         (GIMMICKS,), "lift_update_open_finish_type1"),
        ("threshold-door linked timer target", "Gimmick_Init_ThresholdDoor",
         "record + AE_GIMMICK_TIMER, (uint32_t)height",
         "record + AE_GIMMICK_STATE, (uint32_t)height",
         (GIMMICKS,), "threshold_init"),
        ("threshold-door unlinked state", "Gimmick_Init_ThresholdDoor",
         "ae_rds16(r, gimmick + AE_GIMMICK_LINK) == 0",
         "ae_rds16(r, gimmick + AE_GIMMICK_LINK) != 0",
         (GIMMICKS,), "threshold_init_unlinked"),
        ("threshold-door equality boundary", "Gimmick_ThresholdDoor_Update",
         "ae_rds16(r, collider + AE_COLLIDER_HALF_Y) < threshold;",
         "ae_rds16(r, collider + AE_COLLIDER_HALF_Y) <= threshold;",
         (GIMMICKS,), "threshold_update_equal"),
        ("threshold-door linked height sum", "Gimmick_ThresholdDoor_Update",
         "threshold += ae_rds16(r, record + AE_GIMMICK_PARAM_16);",
         "threshold -= ae_rds16(r, record + AE_GIMMICK_PARAM_16);",
         (GIMMICKS,), "threshold_update_start_loop"),
        ("slide-door direction-five group", "Gimmick_SlideDoor_Init",
         "case 0: case 5:", "case 0:",
         (GIMMICKS,), "slide_init_5"),
        ("slide-door initial movement distance", "Gimmick_SlideDoor_Init",
         "ae_rd16(r, gimmick + AE_GIMMICK_PARAM_12), 0);",
         "ae_rd16(r, gimmick + AE_GIMMICK_PARAM_E), 0);",
         (GIMMICKS,), "slide_init_2"),
        ("slide-door opening sound frame", "Gimmick_SlideDoor_Update",
         "if (timer == 64)", "if (timer == 96)",
         (GIMMICKS,), "slide_update_opening_sound"),
        ("slide-door exact travel endpoint", "Gimmick_SlideDoor_Update",
         "bool reached_end = timer > distance;",
         "bool reached_end = timer >= distance;",
         (GIMMICKS,), "slide_update_open_boundary"),
        ("switch subtype boundary", "Gimmick_Switch_Init",
         "subtype >= 0x2A && subtype <= 0x2D",
         "subtype >= 0x2A && subtype < 0x2D",
         (GIMMICKS,), "switch_init_first_zero"),
        ("switch mode-zero selection", "Gimmick_Switch_Init",
         "if (mode == 0) {\n            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);",
         "if (mode != 0) {\n            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);",
         (GIMMICKS,), "switch_init_first_zero"),
        ("switch touched-zero guard", "Gimmick_Switch_Update",
         "AE_GA_COLLIDER_TOUCHED_BY) < 0)",
         "AE_GA_COLLIDER_TOUCHED_BY) <= 0)",
         (GIMMICKS,), "switch_update_touched"),
        ("switch updated mesh argument", "Gimmick_Switch_Update",
         "ae_rd16(r, gimmick + AE_GIMMICK_PARAM_10),\n            ae_rd16(r, gimmick + AE_GIMMICK_PARAM_10), 0);",
         "ae_rd16(r, gimmick + AE_GIMMICK_PARAM_E),\n            ae_rd16(r, gimmick + AE_GIMMICK_PARAM_10), 0);",
         (GIMMICKS,), "switch_update_touched"),
        ("pair-toggle initial mode", "Gimmick_Init_PairToggle",
         "AE_GIMMICK_PARAM_A) == 0)", "AE_GIMMICK_PARAM_A) != 0)",
         (GIMMICKS,), "pair_toggle_zero"),
        ("pair-toggle initial mesh", "Gimmick_Init_PairToggle",
         "(uint32_t)mesh_a, (uint32_t)mesh_a, (uint32_t)mesh_b);",
         "(uint32_t)mesh_b, (uint32_t)mesh_a, (uint32_t)mesh_b);",
         (GIMMICKS,), "pair_toggle_zero"),
        ("hit update sound id", "Gimmick_Hit_Update",
         "GA_CALL(PlaySfxAt, 0x75, collider, 0, 0);",
         "GA_CALL(PlaySfxAt, 0x74, collider, 0, 0);",
         (GIMMICKS,), "hit_update_zero_touch"),
        ("hit update touch edge", "Gimmick_Hit_Update",
         "state == 0 && touched == 0", "state == 0 && touched != 0",
         (GIMMICKS,), "hit_update_zero_touch"),
        ("spinner speed decay", "Gimmick_SpinRoller_Update",
         "speed - ((speed / 100) * 8 + 8)", "speed - 8",
         (GIMMICKS,), "roller_decay"),
        ("spinner axis selection", "Gimmick_SpinRoller_Update",
         "(subtype == 0x37 || subtype == 0x38) ?",
         "(subtype == 0x37 || subtype == 0x39) ?",
         (GIMMICKS,), "roller_type38"),
    ]


GIMMICK_CASES = {}
for _direction in range(8):
    GIMMICK_CASES[f"swing_init_{_direction}"] = {
        "subtype": _direction, "gtype": 2, "link": -1,
        "half": (41, 55, 27),
    }
    GIMMICK_CASES[f"slide_init_{_direction}"] = {
        "subtype": _direction, "gtype": 2, "link": 1,
        "half": (41, 55, 27),
    }
GIMMICK_CASES.update({
    "swing_init_invalid": {"subtype": 8, "gtype": 2, "link": -1},
    "swing_init_type2_unlinked": {"subtype": 0, "gtype": 2, "link": 0},
    "swing_init_type1_unlinked": {"subtype": 2, "gtype": 1, "link": 0},
    "swing_init_type1_linked": {"subtype": 5, "gtype": 1, "link": 1},
    "swing_update_idle_empty": {"state": 0, "inside": False},
    "swing_update_idle_inside": {"state": 3, "inside": True},
    "swing_update_opening_sound": {"state": 5, "timer": 160},
    "swing_update_open_boundary": {"state": 5, "timer": 992},
    "swing_update_open_finish": {"state": 5, "timer": 1024},
    "swing_update_open_finish_type1": {"state": 5, "timer": 1024, "gtype": 1},
    "swing_update_open_threshold_edge": {"state": 5, "timer": 993},
    "swing_update_closing_sound": {"state": 2, "timer": 864},
    "swing_update_close_finish": {"state": 2, "timer": 16},
    "swing_update_close_finish_type1": {"state": 2, "timer": 16, "gtype": 1},
    "swing_update_close_zero": {"state": 2, "timer": 32},
    "swing_update_state1": {"state": 1},
    "swing_update_state4": {"state": 4},
    "lift_init_type10_linked": {"subtype": 10, "gtype": 1, "link": 1},
    "lift_init_type12_linked": {"subtype": 12, "gtype": 1, "link": 1},
    "lift_init_type10_unlinked": {"subtype": 10, "gtype": 1, "link": 0},
    "lift_init_other_linked": {"subtype": 2, "gtype": 2, "link": 1},
    "lift_init_other_unlinked": {"subtype": 2, "gtype": 2, "link": 0},
    "lift_init_type1_other_linked": {"subtype": 2, "gtype": 1, "link": 1},
    "lift_update_idle_empty": {"state": 0, "inside": False},
    "lift_update_idle_inside": {"state": 3, "inside": True},
    "lift_update_opening_sound": {"state": 5, "timer": 96},
    "lift_update_opening_move": {"state": 5, "timer": 0, "half": (32, 128, 16)},
    "lift_update_open_finish_type1": {"state": 5, "timer": 128, "gtype": 1,
                                       "half": (32, 128, 16)},
    "lift_update_open_finish": {"state": 5, "timer": 128, "half": (32, 128, 16)},
    "lift_update_open_boundary": {"state": 5, "timer": 96, "half": (32, 128, 16)},
    "lift_update_closing_sound": {"state": 2, "timer": 160, "half": (32, 128, 16)},
    "lift_update_close_finish": {"state": 2, "timer": 16},
    "lift_update_close_zero": {"state": 2, "timer": 32},
    "lift_update_close_sound_edge": {"state": 2, "timer": 32, "half": (32, 128, 16)},
    "lift_update_close_finish_type1": {"state": 2, "timer": 16, "gtype": 1},
    "lift_update_close_negative_half": {"state": 2, "timer": 160, "half": (32, -64, 16)},
    "lift_update_state1": {"state": 1},
    "lift_update_state4": {"state": 4},
    "threshold_init": {"param_e": 0, "param_10": 1, "link": 1},
    "threshold_init_unlinked": {"param_e": 0, "param_10": 1, "link": 0},
    "threshold_update_below": {
        "state": 5, "param_e": 0, "param_10": 1,
        "pair_heights": (20, 10), "half": (32, 15, 16), "loop": -1,
    },
    "threshold_update_equal": {
        "state": 5, "param_e": 0, "param_10": 1,
        "pair_heights": (10, 10), "half": (32, 20, 16), "loop": -1,
    },
    "threshold_update_start_loop": {
        "state": 5, "param_e": 0, "param_10": 1,
        "pair_heights": (20, 10), "half": (32, 40, 16), "loop": -1,
    },
    "threshold_update_pan_loop": {
        "state": 2, "param_e": 0, "param_10": 1,
        "pair_heights": (20, 10), "half": (32, 40, 16), "loop": 7,
    },
    "threshold_update_stop_loop": {
        "state": 2, "param_e": -1, "param_10": -1, "loop": 7,
    },
    "threshold_update_stop_loop_state5": {
        "state": 5, "param_e": -1, "param_10": -1, "loop": 7,
    },
    "slide_init_invalid": {"subtype": 8, "gtype": 2, "link": 1},
    "slide_init_type1": {"subtype": 0, "gtype": 1, "link": 1},
    "slide_init_unlinked": {"subtype": 3, "gtype": 2, "link": 0},
    "slide_update_idle_empty": {"state": 0, "inside": False},
    "slide_update_idle_inside": {"state": 3, "inside": True},
    "slide_update_opening_sound": {"state": 5, "timer": 32, "param_12": 256},
    "slide_update_open_finish": {"state": 5, "timer": 64, "param_12": 64},
    "slide_update_open_finish_type1": {"state": 5, "timer": 64, "param_12": 64, "gtype": 1},
    "slide_update_open_boundary": {"state": 5, "timer": 32, "param_12": 64},
    "slide_update_closing_sound": {"state": 2, "timer": 96, "param_12": 64},
    "slide_update_close_sound_edge": {"state": 2, "timer": 32, "param_12": 64},
    "slide_update_close_negative_distance": {"state": 2, "timer": 64, "param_12": -64},
    "slide_update_close_finish": {"state": 2, "timer": 16, "param_12": 64},
    "slide_update_close_finish_type1": {"state": 2, "timer": 16, "param_12": 64, "gtype": 1},
    "slide_update_state1": {"state": 1},
    "slide_update_state4": {"state": 4},
    "switch_init_first_zero": {"subtype": 0x2A, "param_a": 0},
    "switch_init_first_one": {"subtype": 0x2D, "param_a": 1},
    "switch_init_second_zero": {"subtype": 0x2E, "param_a": 0},
    "switch_init_second_one": {"subtype": 0x30, "param_a": 1},
    "switch_init_second_other": {"subtype": 0x31, "param_a": 2},
    "switch_init_invalid": {"subtype": 0x29, "param_a": 0},
    "switch_update_touched": {"state": 0, "touched": 0},
    "switch_update_untouched": {"state": 0, "touched": -1},
    "switch_update_state1": {"state": 1, "touched": 0},
    "pair_toggle_zero": {"param_a": 0},
    "pair_toggle_one": {"param_a": 1},
    "hit_update_zero_touch": {"state": 0, "touch": True},
    "hit_update_zero_miss": {"state": 0, "touch": False},
    "hit_update_one_release": {"state": 1, "touch": False},
    "hit_update_one_touch": {"state": 1, "touch": True},
    "hit_update_state2": {"state": 2, "touch": True},
    "roller_type37": {"subtype": 0x37, "timer": 0x400, "speed": 0x60, "angle": 1500},
    "roller_type38": {"subtype": 0x38, "timer": 0x400, "speed": 0x60, "angle": 1500},
    "roller_type39": {"subtype": 0x39, "timer": 0x400, "speed": 0x60, "angle": 1500},
    "roller_type3a": {"subtype": 0x3A, "timer": 0x400, "speed": 0x60, "angle": 1500},
    "roller_invalid": {"subtype": 0x36, "timer": 0x400, "speed": 0x60},
    "roller_decay": {"subtype": 0x37, "timer": 0x400, "speed": 0xB4, "touched": -1},
    "roller_stopped": {"subtype": 0x3A, "timer": 0x400, "speed": 0, "angle": 0},
    "roller_touch_cap": {"subtype": 0x37, "timer": 0x400, "speed": 0x90, "touched": 0},
    "roller_negative_restore": {"subtype": 0x37, "timer": 0x400, "speed": 0, "angle": 100},
    "roller_limit_reached": {"subtype": 0x37, "timer": 100, "speed": 0x90, "angle": 95,
                              "touched": 0},
    "roller_subtype_above": {"subtype": 0x3B, "timer": 0x400, "speed": 0x60, "angle": 1500},
    "roller_type37_negative": {"subtype": 0x37, "timer": 0x400, "speed": 0x60, "angle": -300},
    "roller_type38_positive": {"subtype": 0x38, "timer": 0x400, "speed": 0x60, "angle": -300},
    "roller_type39_positive": {"subtype": 0x39, "timer": 0x400, "speed": 0x60, "angle": -300},
    "roller_type3a_negative": {"subtype": 0x3A, "timer": 0x400, "speed": 0x60, "angle": -300},
})


def setup_gimmick_fixture(cpu, *, gtype=2, state=0, subtype=2, timer=0,
                          param_a=1, link=-1, param_e=1, param_10=2,
                          param_12=3, param_14=0, param_16=0,
                          half=(32, 64, 16), position=(100, 200, 300),
                          prop_index=0, touched=-1, inside=False, touch=False,
                          loop=None, pair_heights=None, speed=0, angle=0):
    cpu.write32(0x800F4570, GIMMICKS)
    cpu.write32(0x800F455C, COLLIDERS)
    cpu.write32(0x800B4D64, PROPS)
    cpu.write16(GIMMICKS + 0x00, gtype)
    cpu.write16(GIMMICKS + 0x02, state)
    cpu.write16(GIMMICKS + 0x04, 0)
    cpu.write16(GIMMICKS + 0x06, timer)
    cpu.write16(GIMMICKS + 0x08, subtype)
    cpu.write16(GIMMICKS + 0x0A, param_a)
    cpu.write16(GIMMICKS + 0x0C, link)
    cpu.write16(GIMMICKS + 0x0E, param_e)
    cpu.write16(GIMMICKS + 0x10, param_10)
    cpu.write16(GIMMICKS + 0x12, param_12 if loop is None else loop)
    cpu.write16(GIMMICKS + 0x14, speed if speed != 0 else param_14)
    cpu.write16(GIMMICKS + 0x16, angle if angle != 0 else param_16)
    for offset, value in zip((0, 2, 4, 6, 8, 10),
                             (position[0], position[1], position[2], half[0], half[1], half[2])):
        cpu.write16(COLLIDERS + offset, value)
    cpu.write16(COLLIDERS + 0x0E, prop_index)
    cpu.write16(COLLIDERS + 0x10, 0x3000)
    cpu.write16(COLLIDERS + 0x12, 2)
    cpu.write16(COLLIDERS + 0x14, 0)
    cpu.write16(COLLIDERS + 0x16, 2)
    cpu.write16(COLLIDERS + 0x18, 0x1234)
    cpu.write16(COLLIDERS + 0x1E, touched)
    for offset, value in zip((0x24, 0x26, 0x28), position):
        cpu.write16(COLLIDERS + offset, value)
    cpu.write16(0x800F5A20 + 0x70, 0)
    cpu.write16(0x800EC1E0 + 0x58, 0)
    cpu.write32(PLAYER_POSITION, 100)
    cpu.write32(PLAYER_POSITION + 4, 200)
    cpu.write32(PLAYER_POSITION + 8, 300)
    cpu.write32(BOX_HIT_ADDRESS, PLAYER_POSITION if inside else 0)
    cpu.write32(GIMMICK_TOUCH_ADDRESS, 1 if touch else 0)
    for slot in range(8):
        cpu.write8(MONKEYS + slot * MONKEY_SIZE + MONKEY_ACTIVITY, 0)
    for i in range(4):
        prop = PROPS + i * 0x20
        matrix = PROP_MATRIX + i * 0x20
        cpu.write32(prop + 0x1C, matrix)
        cpu.write_bytes(matrix, bytes(32))
    if pair_heights is not None:
        cpu.write16(GIMMICKS + 0x16, pair_heights[0])
        cpu.write16(GIMMICKS + 0x18 + 0x16, pair_heights[1])


def mutation_setup(case):
    def setup(cpu):
        if case in GIMMICK_CASES:
            setup_gimmick_fixture(cpu, **GIMMICK_CASES[case])
        elif case in ("reset", "reset_exit"):
            cpu.write16(0x800F447C, 0 if case == "reset" else 2)
            cpu.write_bytes(SAVES, bytes(64 * 0x1E))
            cpu.write16(0x800BFA20, 123)
        elif case == "save_states":
            cpu.write16(0x800F4474, 5)
            cpu.write32(0x800F4570, GIMMICKS)
            cpu.write32(0x800F456C, GROUPS)
            cpu.write32(0x800F45D8, 1)
            cpu.write16(GROUPS, 4)
            cpu.write16(GROUPS + 4, 0)
            cpu.write_bytes(SAVES, bytes(64 * 0x1E))
            for i in range(64): cpu.write16(SAVES + i * 0x1E, 0xFFFF)
            for i, kind in enumerate((2, 3, 4, 5)):
                cpu.write16(GIMMICKS + i * 0x18, kind)
                cpu.write16(GIMMICKS + i * 0x18 + 8, 0x37 if kind == 2 else 1)
        elif case == "restore":
            cpu.write16(0x800F4476, 5)
            cpu.write32(0x800F4570, GIMMICKS)
            cpu.write_bytes(SAVES, bytes(64 * 0x1E))
            for i in range(64): cpu.write16(SAVES + i * 0x1E, 0xFFFF)
            cpu.write16(SAVES, 1)
            cpu.write16(SAVES + 2, 5)
            cpu.write16(SAVES + 4, 0)
            cpu.write16(SAVES + 6, 2)
            cpu.write16(SAVES + 0x10, 0x1234)
        elif case in ("save_entry", "save_entry_used"):
            cpu.write16(0x800F4476, 5)
            cpu.write_bytes(SAVES, bytes(64 * 0x1E))
            for i in range(64): cpu.write16(SAVES + i * 0x1E, 0xFFFF)
            if case == "save_entry_used": cpu.write16(SAVES, 0)
            cpu.write16(GIMMICKS, 2)
            cpu.write16(GIMMICKS + 2, 1)
            cpu.write16(GIMMICKS + 0x0A, 0x1234)
        elif case == "progress_init":
            cpu.write16(0x800F4474, 5)
            cpu.write16(0x800F4476, 5)
            cpu.write32(0x800AF868 + 5 * 4, HANDLERS)
            cpu.write32(HANDLERS, 0x80012340)
            cpu.write32(HANDLERS + 12, 0)
            cpu.write32(0x800F4570, GIMMICKS)
            cpu.write32(0x800F456C, GROUPS)
            cpu.write32(0x800F45D8, 1)
            cpu.write16(GROUPS, 2)
            cpu.write16(GROUPS + 4, 0)
            cpu.write16(GIMMICKS, 0x22)
            cpu.write16(GIMMICKS + 0x12, 11)
            cpu.write16(GIMMICKS + 0x18, 0x32)
            cpu.write16(GIMMICKS + 0x18 + 0x0A, 6)
        elif case in ("progress_apply", "progress_apply_byte"):
            cpu.write16(0x800F4476, 5)
            cpu.write_bytes(SAVES, bytes(64 * 0x1E))
            for i in range(64): cpu.write16(SAVES + i * 0x1E, 0xFFFF)
            cpu.write16(SAVES, 1)
            cpu.write16(SAVES + 2, 4)
            cpu.write16(SAVES + 6, 0x32 if case == "progress_apply_byte" else 0x22)
            cpu.write16(SAVES + 0x10, 1)
            cpu.write16(SAVES + 0x18, 13)
        elif case in ("spawn", "spawn_saved"):
            cpu.write16(0x800F4474, 5)
            cpu.write16(0x800F4476, 5)
            cpu.write32(0x800AF868 + 5 * 4, HANDLERS)
            cpu.write_bytes(HANDLERS, bytes(2 * 12))
            cpu.write32(HANDLERS, HANDLER_INIT_FN)
            cpu.write32(0x800F4570, GIMMICKS)
            cpu.write32(0x800F456C, GROUPS)
            cpu.write32(0x800F45D8, 1 if case == "spawn" else 0)
            cpu.write16(GROUPS, 1)
            cpu.write16(GROUPS + 4, 0)
            cpu.write16(GIMMICKS, 2)
            cpu.write_bytes(SAVES, bytes(64 * 0x1E))
            for i in range(64): cpu.write16(SAVES + i * 0x1E, 0xFFFF)
            if case == "spawn_saved":
                cpu.write16(SAVES, 1)
                cpu.write16(SAVES + 2, 5)
                cpu.write16(SAVES + 4, 0)
                cpu.write16(SAVES + 6, 2)
                cpu.write16(SAVES + 0x10, 0x7777)
        elif case in ("update", "update_flags"):
            cpu.write16(0x800F4476, 5)
            cpu.write32(0x800AF868 + 5 * 4, HANDLERS)
            cpu.write_bytes(HANDLERS, bytes(2 * 12))
            cpu.write32(HANDLERS + 4, HANDLER_UPDATE_FN)
            cpu.write32(HANDLERS + 8, GROUPS)
            cpu.write32(0x800F4570, GIMMICKS)
            cpu.write16(0x800F4450, 8 if case == "update_flags" else 0)
            cpu.write16(GROUPS, 1)
            cpu.write16(GROUPS + 4, 0)
            cpu.write16(GIMMICKS, 2)
        elif case == "play_type":
            cpu.write16(COLLIDERS + 0x16, 2)
            cpu.write32(STAGE_ENTRY_FLAGS, 1)
        elif case == "play_field":
            cpu.write16(COLLIDERS + 0x16, 2)
            cpu.write16(COLLIDERS + 0x10, 0x3000)
        elif case in ("inside_radius", "inside_monkey"):
            cpu.write32(BOX_HIT_ADDRESS, 0 if case == "inside_radius" else MONKEYS + 0x24)
            cpu.write32(0x800F5A20 + 0x70, 0)
            for slot in range(8):
                cpu.write8(MONKEYS + slot * MONKEY_SIZE + MONKEY_ACTIVITY,
                           4 if case == "inside_monkey" and slot == 0 else
                           (0 if case == "inside_monkey" else 4))
        elif case in ("delta_range", "delta_player"):
            cpu.write16(0x800EC1E0 + 0x58, 7)
            cpu.write16(0x800F5A20 + 0x58, -1)
            for offset, value in ((0, 100), (4, 200), (8, 300)):
                cpu.write32(PLAYER_POSITION + offset, value)
            cpu.write16(DELTA, 513 if case == "delta_range" else 12)
            cpu.write16(DELTA + 2, -34)
            cpu.write16(DELTA + 4, 56)
        elif case == "state":
            cpu.write32(0x800F4570, GIMMICKS)
            cpu.write16(GIMMICKS + 2, 7)
        elif case in ("swing_advance", "swing_advance_edge"):
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_GEOMETRY, 0x3000)
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_Y, 5)
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_X, 0)
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_Z, 0)
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_STEP,
                        4 if case == "swing_advance_edge" else 0)
        elif case in ("swing_carry_priority", "swing_carry_rotation"):
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_X,
                        7 if case == "swing_carry_priority" else 0)
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_Y, 5)
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_Z, 0)
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_STEP, 2)
            for axis, value in enumerate((31, -47, 59)):
                cpu.write16(SWING_POSITION + axis * 2, value)
            for axis, value in enumerate((13, -19, 23)):
                cpu.write16(SWING_OFFSET + axis * 2, value)
            for axis, value in enumerate((0x12345678, 0x23456789, 0x3456789A)):
                cpu.write32(SWING_OUT + axis * 4, value)
        elif case in ("swing_body_geometry", "swing_body_camera"):
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_GEOMETRY, 0x3000)
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_Y, 5)
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_STEP, 2)
            cpu.write16(0x800EC1E0 + 0x58, 7)
            cpu.write16(0x800F5A20 + 0x58, 7 if case == "swing_body_geometry" else -1)
            for axis, value in enumerate((120, 240, 360)):
                cpu.write32(PLAYER_POSITION + axis * 4, value)
            for axis, value in enumerate((100, 200, 300)):
                cpu.write16(SWING_POSITION + axis * 2, value)
        elif case in ("swing_first_positive", "swing_first_z_sign"):
            cpu.write32(0x800B4D64, PROPS)
            cpu.write32(PROPS, PROP_OBJECT)
            cpu.write32(PROPS + 0x1C, PROP_MATRIX)
            cpu.write32(PROP_OBJECT, 3)
            cpu.write32(0x800F455C, COLLIDERS)
            cpu.write16(COLLIDERS + 0x0E, 0)
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_GEOMETRY, 0x3000)
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_X, 0)
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_Y,
                        0 if case == "swing_first_z_sign" else 5)
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_RANGE_Z,
                        -5 if case == "swing_first_z_sign" else 0)
            cpu.write16(COLLIDERS + AE_GA_COLLIDER_SWING_STEP, 1)
            cpu.write_bytes(PROP_MATRIX, bytes(32))
        elif case in ("probe_wall_early", "probe_wall_min"):
            cpu.write16(COLLIDERS + 0x00, 100)
            cpu.write16(COLLIDERS + 0x02, 0 if case == "probe_wall_early" else -32768)
            cpu.write16(COLLIDERS + 0x04, 200)
            cpu.write16(COLLIDERS + 0x06, 20)
            cpu.write16(COLLIDERS + 0x08, 30)
            cpu.write16(COLLIDERS + 0x0A, 16)
            cpu.write32(0x800F44E4, GROUND_HEADER)
            cpu.write32(GROUND_HEADER, 2)
            cpu.write32(GROUND_CONTROL, GROUND_CONTROL_MAGIC)
            cpu.write16(GROUND_SURFACE_CONTROL, 0)
            samples = (-10, 10, 10, 10, 10) if case == "probe_wall_early" else (10, 20, 30, 40, 50)
            for i, value in enumerate(samples): cpu.write16(GROUND_VALUES + i * 2, value)
        elif case == "counter":
            cpu.write32(0x800F455C, COLLIDERS)
            cpu.write16(GIMMICKS + AE_GIMMICK_COLLIDER, 0)
            cpu.write16(COLLIDERS + AE_COLLIDER_HALF_Y, 7)
    return setup


def run_mutations(h, trapv=False):
    variants = mutation_sources()
    source = (ROOT / "src/world/ae_gimmick_a.c").read_text(encoding="utf-8")
    caught, survivors = [], []
    counts = {name: 0 for name in FUNCS}
    original_coverage = h.cpu.coverage
    h.cpu.coverage = None
    with tempfile.TemporaryDirectory(prefix="gimmick-a-mutants-", dir=ROOT / "extract/mips",
                                     ignore_cleanup_errors=True) as temp_name:
        temp = Path(temp_name)
        for index, (label, function, old, new, args, case) in enumerate(variants, 1):
            try:
                mutated = mutate_function(source, function, old, new)
            except Exception as error:
                survivors.append(label + f" (bad anchor: {error})")
                continue
            mutation_source = temp / f"mutant_{index:02d}.c"
            mutation_source.write_text(mutated, encoding="utf-8")
            try:
                side = Side(build(trapv, mutation_source, temp, f"mutant_{index:02d}"))
            except Exception as error:
                survivors.append(label + f" (build: {error})")
                continue
            before = h.failures
            matched = h.run(function, args, mutation_setup(case), label=f"mutation {label}", side=side)
            h.failures = before
            counts[function] += 1
            if matched:
                survivors.append(label)
            else:
                caught.append(label)
            side.model = None
            side.callbacks.clear()
            side.hooks = CHooks()
            del side
            gc.collect()
    h.cpu.coverage = original_coverage
    print("mutation coverage: " + ", ".join(f"{name} {counts[name]}" for name in FUNCS))
    print(f"mutations caught: {len(caught)}/{len(variants)}")
    if survivors:
        print("mutation survivors: " + ", ".join(survivors))
    return not survivors and len(variants) >= 2 * len(FUNCS) and all(counts[name] >= 2 for name in FUNCS)


def mutate_function(source, function, old, new):
    signature = f"int32_t ae_{function}("
    start = source.index(signature)
    next_function = source.find("\nint32_t ae_", start + len(signature))
    end = next_function if next_function >= 0 else source.find("\n#undef GA_CALL", start)
    if end < 0: end = len(source)
    body = source[start:end]
    count = body.count(old)
    if count != 1:
        raise RuntimeError(f"anchor for {function} matched {count} times: {old!r}")
    return source[:start] + body.replace(old, new, 1) + source[end:]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--trapv", action="store_true")
    parser.add_argument("--seed", type=int, default=94423)
    args = parser.parse_args()

    side = Side(build(trapv=args.trapv))
    harness = Harness(side)
    baseline(harness, args.seed)
    default_run = args.seed == 94423 and not args.trapv
    summary = harness.finish(default_run=default_run,
                             show_coverage=args.coverage or default_run)
    for name in FUNCS:
        print(f"  {name:34s} {harness.counts[name]}")
    print(f"total differential cases: {sum(harness.counts.values())}")
    if args.mutate:
        mutation_ok = run_mutations(harness, trapv=args.trapv)
    else:
        mutation_ok = True
    gaps = [name for name, result in summary.items() if result["uncovered"] or result["stale_declared"]]
    if gaps:
        print("coverage gaps: " + ", ".join(gaps))
    if harness.failures or gaps or not mutation_ok:
        return 1
    print("PASS: all current gimmick A scenarios matched")
    if args.mutate:
        print("PASS: all required mutations were caught")
    return 0


if __name__ == "__main__":
    sys.exit(main())
