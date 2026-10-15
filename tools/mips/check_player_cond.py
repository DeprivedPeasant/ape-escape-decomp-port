"""Differentially check the hand-ported PlayerCond_* predicates against SCUS_944.23.

Each call runs on matching MIPS and AeRam images containing the real EXE and area 1 data.
The checker compares the exact return word, all main RAM outside the top-of-RAM stack window,
the full scratchpad and ordered calls to the three out-of-batch player hooks and GetSolidHeight.

Usage: .venv/Scripts/python.exe tools/mips/check_player_cond.py [--coverage] [--mutate]
"""
import argparse
import ctypes as C
import json
import os
import random
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

M32 = 0xFFFFFFFF
STACK_LO = mr.STACK_WINDOW[0] - 0x80000000
PLAYER = 0x800EC1E0
SCRATCH = 0x1F800048
HISTORY_POS = 0x800E5490
HISTORY_DONE = 0x800E53C4
STATE_TABLE = 0x801D0000
SURFACE_TABLE = 0x800AEF60
PLAYER_COND_SEED = 0x800AE8F0
HOOKS = {
    "GetSolidHeight": 0x8004D8C0,
    "PlayerIsOnGround": 0x800652E4,
    "PlayerSplash": 0x8006B950,
    "PlayerFxJumpBurst": 0x8006C58C,
}

# name -> original address, byte size, C hook-table argument
FUNCS = {
    "PlayerCond_False": (0x80064AF0, 8, False),
    "PlayerCond_JumpHeld": (0x80064AF8, 16, False),
    "PlayerCond_JumpPressed": (0x80064B08, 28, False),
    "PlayerCond_RStickEvent02": (0x80064B24, 152, False),
    "PlayerCond_RStickEvent40": (0x80064BBC, 128, False),
    "PlayerCond_RStickBackHard": (0x80064C3C, 56, False),
    "PlayerCond_GadgetBusyOrGesture": (0x80064C74, 84, False),
    "PlayerCond_FacingDesiredDir": (0x80064CC8, 80, False),
    "PlayerCond_RStickSideNeg": (0x80064D18, 52, False),
    "PlayerCond_RStickSidePos": (0x80064D4C, 48, False),
    "PlayerCond_RStickFwd": (0x80064D7C, 48, False),
    "PlayerCond_RStickBack": (0x80064DAC, 52, False),
    "PlayerCond_AttachTimerDone": (0x80064DE0, 36, False),
    "PlayerCond_GroundBehindLow": (0x80064E04, 104, True),
    "PlayerCond_RStickGestureOrTravel": (0x80064E6C, 64, False),
    "PlayerCond_AnimDoneRandom": (0x80064EAC, 76, False),
    "PlayerCond_NoMoveInput": (0x80064EF8, 48, False),
    "PlayerCond_LStickFwd": (0x80064F28, 52, False),
    "PlayerCond_LStickBack": (0x80064F5C, 52, False),
    "PlayerCond_RStickEvent04": (0x80064F90, 140, False),
    "PlayerCond_RStickEvent08": (0x8006501C, 140, False),
    "PlayerCond_NoGestureR3Held": (0x800650A8, 48, False),
    "PlayerCond_RStickRotating": (0x800650E8, 68, False),
    "PlayerCond_Gadget1RStickFwdFlick": (0x8006512C, 76, False),
    "PlayerCond_Gadget3RStickBack": (0x80065178, 76, False),
    "PlayerCond_RStickGesture10": (0x800651C4, 16, False),
    "PlayerCond_RStickGesture20": (0x800651D4, 16, False),
    "PlayerCond_RStickNoGesture": (0x800651E4, 16, False),
    "PlayerCond_GestureEvent04": (0x800651F4, 16, False),
    "PlayerCond_GestureEvent08": (0x80065204, 16, False),
    "PlayerCond_LStickIdleL3Held": (0x80065214, 48, False),
    "PlayerCond_LStickIdle": (0x80065244, 16, False),
    "PlayerCond_LStickClass2": (0x80065254, 16, False),
    "PlayerCond_LStickClass4": (0x80065264, 16, False),
    "PlayerCond_LStickClass8": (0x80065274, 16, False),
    "PlayerCond_LStickFull": (0x80065284, 24, False),
    "PlayerCond_RStickFull": (0x8006529C, 24, False),
    "PlayerCond_LStickPartial": (0x800652B4, 24, False),
    "PlayerCond_RStickPartial": (0x800652CC, 24, False),
    "PlayerCond_LandingSoon": (0x80065318, 168, True),
    "PlayerCond_FacingContactWall": (0x800653C0, 124, False),
    "PlayerCond_ContactKind8": (0x8006543C, 28, False),
    "PlayerCond_NotContactKind8": (0x80065458, 28, False),
    "PlayerCond_FallingHigh": (0x80065474, 56, False),
    "PlayerCond_RStickEvent10": (0x800654AC, 140, False),
    "PlayerCond_RStickEvent20": (0x80065538, 140, False),
    "PlayerCond_FirstFrameRStickEvent10": (0x800655C4, 104, False),
    "PlayerCond_FirstFrameRStickEvent20": (0x8006562C, 104, False),
    "PlayerCond_JumpPressedAtApex": (0x800656DC, 108, True),
    "PlayerCond_LStickSidePos": (0x80065748, 24, False),
    "PlayerCond_LStickSideNeg": (0x80065760, 16, False),
    "PlayerCond_LStickSideAny": (0x80065770, 32, False),
    "PlayerCond_LStickIdleStillMoving": (0x80065790, 80, False),
    "PlayerCond_GestureEvent0C": (0x800657E0, 16, False),
    "PlayerCond_InWater": (0x800658E8, 28, False),
    "PlayerCond_WadingOnGround": (0x80065904, 124, True),
    "PlayerCond_ContactWall3B": (0x80065980, 44, False),
    "PlayerCond_JumpOutOfWater": (0x800659AC, 120, True),
    "PlayerCond_CanJump": (0x80065A24, 196, False),
    "PlayerCond_LStickIdleL3Pressed": (0x80065AE8, 56, False),
    "PlayerCond_Dead": (0x80065B20, 52, False),
    "PlayerCond_Gadget4Mode1": (0x80065B54, 24, False),
    "PlayerCond_AnimDone": (0x80065B6C, 60, False),
    "PlayerCond_False2": (0x80065BA8, 8, False),
    "PlayerCond_Still": (0x80065BB0, 112, False),
}

AREA_MEMBER = dump_areas.area_members()[1]
AREA_BLOB = (ROOT / "extract/members" / f"{AREA_MEMBER:03X}.bin").read_bytes()
AREA_HEADER = struct.unpack_from("<I", AREA_BLOB, 0x4D0)[0]
AREA_GRID = AREA_HEADER + 4 * struct.unpack_from("<I", AREA_BLOB, AREA_HEADER + 4)[0]
AREA_WIDTH, AREA_DEPTH, AREA_COLOR_COUNT = struct.unpack_from("<HHI", AREA_BLOB, AREA_GRID)
AREA_CELLS = AREA_GRID + 8
AREA_HEIGHTS = AREA_CELLS + AREA_WIDTH * AREA_DEPTH * 12 + AREA_COLOR_COUNT * 4


def build(source=None, tag="playercond"):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{os.getpid()}_{time.time_ns()}.dll"
    subprocess.run([
        shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-shared", "-static-libgcc",
        "-I", str(ROOT / "src/player"), str(ROOT / "src/core/ae_ram.c"),
        str(ROOT / "src/world/ae_world.c"), str(source or ROOT / "src/player/ae_player_cond.c"), "-o", str(dll),
    ], check=True)
    return dll


P, U32, I32, VP = C.c_void_p, C.c_uint32, C.c_int32, C.c_void_p
WorldHookFn = C.CFUNCTYPE(I32, VP, VP, U32, U32, U32, U32)
PlayerBoolHookFn = C.CFUNCTYPE(I32, VP, VP)
PlayerVoidHookFn = C.CFUNCTYPE(None, VP, VP)


class WorldHooks(C.Structure):
    _fields_ = [("user", VP), ("GetSolidHeight", WorldHookFn), ("PlaySfxAt", WorldHookFn),
                ("GimmickActivate", WorldHookFn), ("MeshInitFromAreaTable", WorldHookFn),
                ("FUN_8013d358", WorldHookFn), ("CameraShake", WorldHookFn), ("EventCamStart", WorldHookFn),
                ("ground_stale_x", C.c_uint16), ("ground_stale_z", C.c_uint16)]


class PlayerCondHooks(C.Structure):
    _fields_ = [("user", VP), ("world", C.POINTER(WorldHooks)), ("PlayerIsOnGround", PlayerBoolHookFn),
                ("PlayerSplash", PlayerVoidHookFn), ("PlayerFxJumpBurst", PlayerVoidHookFn)]


class CMemory:
    def __init__(self, side):
        self.side = side

    def _blob(self, address):
        if mr.SCRATCH_BASE <= address < mr.SCRATCH_BASE + mr.SCRATCH_SIZE:
            return self.side.scr, address - mr.SCRATCH_BASE
        return self.side.main, address & 0x1FFFFF

    def read(self, address, size):
        blob, off = self._blob(address)
        return int.from_bytes(bytes(blob[off:off + size]), "little")

    def write(self, address, size, value):
        blob, off = self._blob(address)
        blob[off:off + size] = (value & ((1 << (8 * size)) - 1)).to_bytes(size, "little")


class Model:
    """Same deterministic hook effects on the MIPS and C images."""
    def __init__(self, mem, ctx):
        self.mem, self.ctx, self.log = mem, ctx, []

    def call(self, name, args=()):
        if name == "GetSolidHeight":
            args = tuple(v & M32 for v in args[:3])
            self.log.append((name, *args))
            self.mem.write(0x800B1000, 4, (self.mem.read(0x800B1000, 4) + sum(args)) & M32)
            self.mem.write(0x1F8003F0, 2, 0x1234)
            self.mem.write(0x1F8003F8, 2, self.ctx["effect"])
            self.mem.write(0x1F8003FC, 2, 0x4321)
            self.mem.write(0x1F8003FE, 2, 0x8765)
            return self.ctx["solid"]
        self.log.append((name,))
        if name == "PlayerIsOnGround":
            return self.ctx["on_ground"]
        return 0


class Side:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        lib = self.lib
        lib.ae_ram_create.restype = P
        lib.ae_ram_destroy.argtypes = [P]
        lib.ae_ptr.argtypes, lib.ae_ptr.restype = [P, U32, U32], P
        lib.ae_ram_clear_error.argtypes = [P]
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_error.argtypes, lib.ae_ram_error.restype = [P], C.c_char_p
        self.ram = lib.ae_ram_create()
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scr = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.model = None
        self.mem = CMemory(self)
        self.keep = []

        def world_cb(user, ram, a, b, c, d):
            return self.model.call("GetSolidHeight", (a, b, c, d))
        world_get_solid = WorldHookFn(world_cb)
        self.keep.append(world_get_solid)
        self.world = WorldHooks()
        self.world.GetSolidHeight = world_get_solid

        def on_ground(user, ram):
            return self.model.call("PlayerIsOnGround")
        def splash(user, ram):
            self.model.call("PlayerSplash")
        def jump_burst(user, ram):
            self.model.call("PlayerFxJumpBurst")
        self_hooks = [PlayerBoolHookFn(on_ground), PlayerVoidHookFn(splash), PlayerVoidHookFn(jump_burst)]
        self.keep.extend(self_hooks)
        self.hooks = PlayerCondHooks(None, C.pointer(self.world), *self_hooks)
        for name, (_, _, takes_hooks) in FUNCS.items():
            fn = getattr(lib, "ae_" + name)
            fn.argtypes = [P] + ([C.POINTER(PlayerCondHooks)] if takes_hooks else [])
            fn.restype = I32

    def sync(self, m):
        C.memmove(self.main, bytes(m.ram), mr.RAM_SIZE)
        C.memmove(self.scr, bytes(m.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)

    def close(self):
        if self.ram:
            self.lib.ae_ram_destroy(self.ram)
            self.ram = None

    def call(self, name):
        if FUNCS[name][2]:
            return getattr(self.lib, "ae_" + name)(self.ram, C.byref(self.hooks))
        return getattr(self.lib, "ae_" + name)(self.ram)


class Harness:
    def __init__(self, source=None, tag="playercond"):
        area_path = ROOT / "extract/members" / f"{AREA_MEMBER:03X}.bin"
        self.m = mr.MipsRam.from_extract(area=area_path)
        self.m.coverage = set()
        self.base = self.m.snapshot()
        self.c = Side(build(source, tag))
        self.failures = 0
        self.counts = {name: 0 for name in FUNCS}
        self.target_mutation = None
        self.first_failure = None
        reg.declare("stack_window", "801FF000..80200000")

    def fail(self, name, label, *details):
        self.failures += 1
        if self.first_failure is None:
            self.first_failure = (name, label, details)
        print(f"FAIL {name} [{label}]")
        for line in details:
            print("   ", line)

    def run(self, name, case, ctx):
        if self.target_mutation and name != self.target_mutation:
            return False
        self.m.restore(self.base)
        setup_case(self.m, name, case, ctx)
        self.c.sync(self.m)
        self.c.world.ground_stale_x, self.c.world.ground_stale_z = ctx["stale"]
        mm = Model(self.m, ctx)
        cm = Model(self.c.mem, ctx)
        self.c.model = cm

        def callback(hook_name):
            def inner(cpu, regs):
                args = tuple(regs[4:8])
                if hook_name == "GetSolidHeight":
                    return mm.call(hook_name, args)
                mm.call(hook_name)
                return int(ctx["on_ground"]) if hook_name == "PlayerIsOnGround" else 0
            return inner

        mips_hooks = {address: callback(name_) for name_, address in HOOKS.items()}
        mips_result = None
        trap = None
        try:
            mips_result = self.m.call(FUNCS[name][0], hooks=mips_hooks)[2]
        except mr.MipsError as error:
            trap = str(error)
        c_result = self.c.call(name)
        self.counts[name] += 1
        if trap:
            self.fail(name, case, "MIPS harness error: " + trap)
            return False
        if self.c.lib.ae_ram_failed(self.c.ram):
            self.fail(name, case, "C error: " + self.c.lib.ae_ram_error(self.c.ram).decode())
            return False
        problems = []
        if (mips_result & M32) != (c_result & M32):
            problems.append(f"return MIPS {mips_result & M32:08X} C {c_result & M32:08X}")
        if mm.log != cm.log:
            problems.append(f"hook log differs; MIPS={mm.log} C={cm.log}")
        if bytes(self.m.ram[:STACK_LO]) != bytes(self.c.main[:STACK_LO]):
            left, right = self.m.ram[:STACK_LO], self.c.main[:STACK_LO]
            diffs = [i for i, (a, b) in enumerate(zip(left, right)) if a != b]
            problems.append(f"RAM differs ({len(diffs)} bytes) at " + ", ".join(
                f"{0x80000000+i:08X}(MIPS {left[i]:02X}, C {right[i]:02X})" for i in diffs[:8]))
        if bytes(self.m.scratch) != bytes(self.c.scr):
            diffs = [i for i, (a, b) in enumerate(zip(self.m.scratch, self.c.scr)) if a != b]
            problems.append("scratch differs at " + ", ".join(
                f"{0x1F800000+i:08X}(MIPS {self.m.scratch[i]:02X}, C {self.c.scr[i]:02X})" for i in diffs[:8]))
        if problems:
            self.fail(name, case, *problems)
            return False
        return True

    def finish(self, coverage=True, default_run=True):
        items = [(name, address, size, True) for name, (address, size, _) in FUNCS.items()]
        summary = reg.coverage_summary(items, self.m.coverage)
        if coverage:
            reg.print_coverage(summary)
        gaps = any(row["uncovered_count"] or row["stale_declared"] for row in summary.values())
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps, function_names=list(FUNCS))
        return summary, gaps

    def close(self):
        self.c.close()


def s16(value):
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def s32(value):
    value &= M32
    return value - 0x100000000 if value & 0x80000000 else value


def area_setup(m):
    """AreaLoad fixups used by check_world.py, with real area 1 terrain data."""
    m.write32(0x800F44E4, mr.AREA_BASE + AREA_HEADER)
    m.write16(0x800EBFA0, AREA_WIDTH)
    m.write16(0x800EBFA2, AREA_DEPTH)
    m.write32(0x800EBFA8, mr.AREA_BASE + AREA_CELLS)
    m.write32(0x800EBFB4, mr.AREA_BASE + AREA_HEIGHTS)


def setup_case(m, name, case, ctx):
    """Seed the condition's input fields and valid indirect tables for this scenario."""
    rng = random.Random(ctx["seed"])
    area_setup(m)
    m.write32(SCRATCH, STATE_TABLE)
    for state in range(128):
        entry = STATE_TABLE + state * 8
        m.write16(entry, rng.choice([0, 1, 5, 6, 20, 200, 0x7FFF]))
        m.write16(entry + 2, rng.choice([0, 1, 5, 6, 20, 200, 0x7FFF]))
    m.write32(HISTORY_POS, rng.randrange(40))
    for slot in range(40):
        m.write16(HISTORY_DONE + 2 * slot, rng.choice([0, 0, 0, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0xFFFF]))
    for entry in range(8):
        m.write16(SURFACE_TABLE + entry * 0x10 + 0x0C,
                  rng.choice([0, 1, 0x100, 0xB50, 0x7FFF, -1 & 0xFFFF]))

    # Player and scratch values that the predicates read.
    for offset in (0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x124, 0x12C):
        m.write32(PLAYER + offset, rng.choice([0, 1, -1, 4, -4, 0x1000, -0x1000, rng.randint(-20000, 20000)]))
    for offset in (0x50, 0x60, 0x64, 0x74, 0x78, 0x104, 0x106, 0x138, 0x13C, 0x5C, 0x5E,
                   0x70, 0x8A, 0xDE, 0xE8, 0xEC, 0xF2, 0x136):
        m.write16(PLAYER + offset, rng.choice([0, 1, 2, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200,
                                                0x1000, 0x3000, 0xA000, 0x7FFF, 0xFFFF, rng.getrandbits(16)]))
    m.write8(PLAYER + 0xF2, rng.choice([0, 1, 2, 3, 4, 0xFF]))
    for offset in (0x0C, 0x10, 0x14, 0x16, 0x1A, 0x1C, 0x1E, 0x28, 0x2A, 0x2C, 0x2E, 0x30, 0x34, 0x38):
        m.write16(SCRATCH + offset, rng.choice([0, 1, 2, 4, 8, 0x0C, 0x10, 0x20, 0x30, 0x6E, 0x7D,
                                                0xB50, -0xB50 & 0xFFFF, 0xDDB, -0xDDB & 0xFFFF,
                                                0x7FFF, 0x8000, rng.getrandbits(16)]))
    m.write32(SCRATCH + 0x0C, rng.choice([0, 1, 0x0A, 0x20A, 0x200, 0x400, 0xFFFFFFFF, rng.getrandbits(32)]))
    m.write32(SCRATCH + 0x10, rng.choice([0, 1, 0x0A, 0x20A, 0x200, 0x400, 0xFFFFFFFF, rng.getrandbits(32)]))
    m.write16(SCRATCH + 0x38, rng.choice([0, 0, 0, 1]))
    m.write16(PLAYER_COND_SEED, rng.choice([0, 0x33, 0xFFFF, rng.getrandbits(16)]))
    m.write16(0x800F5164, rng.choice([0, 1, 2, 0xFFFF]))
    m.write16(0x800F5124, rng.choice([0, 1, 2, 0xFFFF]))
    m.write32(0x1F800000, rng.choice([-0x1000, -0xB50, 0, 0xB50, 0x1000, rng.randint(-32768, 32767)]) & M32)
    m.write16(0x1F800004, rng.choice([0, 0x1000, 0x7FFF, 0x8000, rng.getrandbits(16)]))
    m.write32(0x1F800008, rng.choice([-0x1000, -0xB50, 0, 0xB50, 0x1000, rng.randint(-32768, 32767)]) & M32)
    m.write16(0x1F80000C, rng.choice([0, 0x1000, 0x7FFF, 0x8000, rng.getrandbits(16)]))
    m.write16(0x800E54D8, rng.choice([0, 0x14, 0x15, -0x14 & 0xFFFF, -0x15 & 0xFFFF, rng.getrandbits(16)]))
    m.write16(0x800E54A0, rng.choice([0, 0x6E, 0x6F, 0x7C, 0x7D, rng.getrandbits(16)]))
    m.write16(0x800E54A2, rng.choice([0, 0x6E, 0x6F, 0x7C, 0x7D, rng.getrandbits(16)]))
    m.write16(0x800E5498, rng.choice([0, 2, 4, 8]))
    m.write16(0x800E549A, rng.choice([0, 4, 8, 0x10, 0x20, 0x80]))
    m.write16(0x800E549E, rng.choice([0, 4, 8, 0x0C]))
    # Reasonable positions keep the real-area ground lookup inside its cell grid.
    m.write32(PLAYER + 0x24, rng.randrange(AREA_WIDTH * 512) - AREA_WIDTH * 256)
    m.write32(PLAYER + 0x28, rng.randint(-20000, 20000))
    m.write32(PLAYER + 0x2C, rng.randrange(AREA_DEPTH * 512) - AREA_DEPTH * 256)
    m.write16(PLAYER + 0x5E, rng.randrange(128))
    m.write16(PLAYER + 0x5C, rng.randrange(0x100))

    # Function-specific edge cases are mixed into every function's default case set.
    if case == "false":
        if name == "PlayerCond_AttachTimerDone": m.write16(PLAYER + 0xDE, 5)
        if name == "PlayerCond_AnimDone":
            m.write16(PLAYER + 0x5E, 3); m.write16(STATE_TABLE + 3 * 8 + 2, 20); m.write16(PLAYER + 0x5C, 10); m.write16(PLAYER + 0x8A, 0)
        if name == "PlayerCond_AnimDoneRandom": m.write16(PLAYER_COND_SEED, 0)
        if name in ("PlayerCond_RStickEvent02", "PlayerCond_RStickEvent40", "PlayerCond_RStickEvent04",
                    "PlayerCond_RStickEvent08", "PlayerCond_RStickEvent10", "PlayerCond_RStickEvent20"):
            for slot in range(40): m.write16(HISTORY_DONE + 2 * slot, 0x1000)
            m.write16(SCRATCH + 0x38, 0)
        if name == "PlayerCond_GadgetBusyOrGesture":
            m.write16(0x800F5164, 0); m.write16(SCRATCH + 0x16, 0); m.write16(SCRATCH + 0x28, 0x1000)
        if name == "PlayerCond_RStickGestureOrTravel": m.write16(SCRATCH + 0x16, 0); m.write16(SCRATCH + 0x34, 0)
        if name == "PlayerCond_RStickRotating": m.write16(SCRATCH + 0x16, 0); m.write16(SCRATCH + 0x30, 0)
        if name == "PlayerCond_NoMoveInput": m.write16(SCRATCH + 0x14, 1); m.write32(SCRATCH + 0x0C, 0)
        if name in ("PlayerCond_LStickFull", "PlayerCond_RStickFull"):
            m.write16(SCRATCH + (0x1C if "LStick" in name else 0x1E), 0)
        if name == "PlayerCond_NotContactKind8": m.write8(PLAYER + 0x53, 8)
        if name == "PlayerCond_LStickIdleStillMoving":
            m.write16(SCRATCH + 0x14, 0); m.write32(PLAYER + 0x30, 0); m.write32(PLAYER + 0x38, 0)
        if name in ("PlayerCond_FirstFrameRStickEvent10", "PlayerCond_FirstFrameRStickEvent20"):
            m.write16(PLAYER + 0x5E, 3); m.write16(PLAYER + 0x5C, 6); m.write16(STATE_TABLE + 3 * 8, 5)
            m.write16(HISTORY_DONE + 2 * m.read32(HISTORY_POS), 0)
        if name == "PlayerCond_FacingDesiredDir":
            m.write16(PLAYER + 0x60, 0x1000); m.write16(PLAYER + 0x64, 0)
            m.write16(PLAYER + 0x74, -0x1000); m.write16(PLAYER + 0x78, 0)
        if name == "PlayerCond_LandingSoon":
            m.write16(PLAYER + 0x136, 1); m.write32(PLAYER + 0x124, 0); m.write32(PLAYER + 0x28, -32768)
        if name == "PlayerCond_GroundBehindLow": m.write32(PLAYER + 0x28, -32768)
        if name == "PlayerCond_CanJump":
            m.write32(SCRATCH + 0x0C, 0); m.write32(SCRATCH + 0x10, 0); m.write16(PLAYER + 0x138, 0)
        if name in ("PlayerCond_JumpPressedAtApex", "PlayerCond_JumpOutOfWater"):
            m.write32(SCRATCH + 0x0C, 0); m.write32(SCRATCH + 0x10, 0)
    elif case == "true":
        if name == "PlayerCond_AttachTimerDone": m.write16(PLAYER + 0xDE, 1)
        if name in ("PlayerCond_AnimDone", "PlayerCond_AnimDoneRandom"):
            m.write16(PLAYER + 0x5E, 3); m.write16(STATE_TABLE + 3 * 8 + 2, 5); m.write16(PLAYER + 0x5C, 10); m.write16(PLAYER + 0x8A, 0)
            m.write16(PLAYER_COND_SEED, 0x33)
        if name == "PlayerCond_FacingDesiredDir":
            m.write16(PLAYER + 0x60, 0x1000); m.write16(PLAYER + 0x64, 0)
            m.write16(PLAYER + 0x74, 0x1000); m.write16(PLAYER + 0x78, 0)
        if name in ("PlayerCond_JumpHeld", "PlayerCond_JumpPressed", "PlayerCond_CanJump",
                    "PlayerCond_JumpPressedAtApex", "PlayerCond_JumpOutOfWater"):
            m.write32(SCRATCH + 0x0C, 0x0A); m.write32(SCRATCH + 0x10, 0x0A)
        if name == "PlayerCond_Still":
            m.write32(PLAYER + 0x30, 0); m.write32(PLAYER + 0x38, 0); m.write16(PLAYER + 0x104, 0); m.write16(PLAYER + 0x106, 0)
        if name == "PlayerCond_LStickIdleStillMoving":
            m.write16(SCRATCH + 0x14, 0); m.write32(PLAYER + 0x30, 0x100); m.write32(PLAYER + 0x38, 0)
        if name == "PlayerCond_FacingContactWall":
            m.write32(PLAYER + 0x50, 0xA0000000); m.write16(0x1F800000, 0x1000); m.write16(0x1F800008, 0)
            m.write16(PLAYER + 0x60, -0x1000 & 0xFFFF); m.write16(PLAYER + 0x64, 0)
        if name in ("PlayerCond_ContactKind8", "PlayerCond_NotContactKind8"):
            m.write8(PLAYER + 0x53, 8)
            if name == "PlayerCond_NotContactKind8": m.write8(PLAYER + 0x53, 0)
        if name == "PlayerCond_ContactWall3B": m.write32(PLAYER + 0x50, 0xA000003B)
        if name == "PlayerCond_WadingOnGround":
            m.write16(PLAYER + 0x138, 0x1002); m.write16(PLAYER + 0x104, 0x20); m.write16(PLAYER + 0x106, 0x30)
        if name == "PlayerCond_JumpOutOfWater":
            m.write32(SCRATCH + 0x0C, 0x0A); m.write32(SCRATCH + 0x10, 0x0A)
            m.write16(PLAYER + 0x13C, 100); m.write32(PLAYER + 0x28, 200)
        if name == "PlayerCond_CanJump":
            m.write16(PLAYER + 0x138, 0x1000); m.write32(PLAYER + 0x50, 0xA0000000)
        if name == "PlayerCond_JumpPressedAtApex": m.write32(PLAYER + 0x34, 0)
        if name == "PlayerCond_FirstFrameRStickEvent10":
            m.write16(PLAYER + 0x5E, 3); m.write16(PLAYER + 0x5C, 6); m.write16(STATE_TABLE + 3 * 8, 5)
            m.write16(HISTORY_DONE + 2 * m.read32(HISTORY_POS), 0x10)
        if name == "PlayerCond_FirstFrameRStickEvent20":
            m.write16(PLAYER + 0x5E, 3); m.write16(PLAYER + 0x5C, 6); m.write16(STATE_TABLE + 3 * 8, 5)
            m.write16(HISTORY_DONE + 2 * m.read32(HISTORY_POS), 0x20)
        if name in ("PlayerCond_RStickEvent02", "PlayerCond_RStickEvent40", "PlayerCond_RStickEvent04",
                    "PlayerCond_RStickEvent08", "PlayerCond_RStickEvent10", "PlayerCond_RStickEvent20"):
            event = {"PlayerCond_RStickEvent02": 2, "PlayerCond_RStickEvent40": 0x40, "PlayerCond_RStickEvent04": 4,
                     "PlayerCond_RStickEvent08": 8, "PlayerCond_RStickEvent10": 0x10, "PlayerCond_RStickEvent20": 0x20}[name]
            pos = m.read32(HISTORY_POS)
            m.write16(HISTORY_DONE + 2 * pos, event)
            m.write16(SCRATCH + 0x38, 0)
        if name == "PlayerCond_RStickRotating": m.write16(SCRATCH + 0x16, 0x10); m.write16(SCRATCH + 0x30, 0x15)
        if name == "PlayerCond_LStickIdleL3Pressed":
            m.write16(SCRATCH + 0x14, 0); m.write32(SCRATCH + 0x0C, 0x200); m.write32(SCRATCH + 0x10, 0x200)
        if name == "PlayerCond_LStickIdleL3Held":
            m.write16(SCRATCH + 0x14, 0); m.write32(SCRATCH + 0x0C, 0x200)
        if name == "PlayerCond_Dead": m.write16(PLAYER + 0xE8, 0); m.write32(PLAYER + 0x70, 0)
        if name == "PlayerCond_Gadget4Mode1": m.write16(0x800F5124, 1)
        if name == "PlayerCond_RStickGesture10": m.write16(SCRATCH + 0x16, 0x10)
        if name == "PlayerCond_RStickGesture20": m.write16(SCRATCH + 0x16, 0x20)
        if name == "PlayerCond_GestureEvent04": m.write16(SCRATCH + 0x1A, 4)
        if name == "PlayerCond_GestureEvent08": m.write16(SCRATCH + 0x1A, 8)
        if name == "PlayerCond_GestureEvent0C": m.write16(SCRATCH + 0x1A, 0x0C)
        if name == "PlayerCond_RStickNoGesture": m.write16(SCRATCH + 0x16, 0)
        if name == "PlayerCond_LStickClass2": m.write16(SCRATCH + 0x14, 2)
        if name == "PlayerCond_LStickClass4": m.write16(SCRATCH + 0x14, 4)
        if name == "PlayerCond_LStickClass8": m.write16(SCRATCH + 0x14, 8)
        if name in ("PlayerCond_LStickFull", "PlayerCond_RStickFull"): m.write16(SCRATCH + (0x1C if "LStick" in name else 0x1E), 0x7D)
        if name in ("PlayerCond_LStickPartial", "PlayerCond_RStickPartial"): m.write16(SCRATCH + (0x1C if "LStick" in name else 0x1E), 2)
        if name == "PlayerCond_LStickIdle": m.write16(SCRATCH + 0x14, 0)
        if name == "PlayerCond_NoMoveInput": m.write16(SCRATCH + 0x14, 0); m.write16(SCRATCH + 0x38, 0); m.write32(SCRATCH + 0x0C, 0)
        if name == "PlayerCond_NoGestureR3Held": m.write16(SCRATCH + 0x16, 0); m.write32(SCRATCH + 0x0C, 0x400)
        if name == "PlayerCond_GadgetBusyOrGesture": m.write16(0x800F5164, 1)
        if name == "PlayerCond_RStickGestureOrTravel": m.write16(SCRATCH + 0x16, 0x10)
        if name == "PlayerCond_RStickSideNeg": m.write16(SCRATCH + 0x1E, 1); m.write16(SCRATCH + 0x2A, -0xDDB - 1)
        if name == "PlayerCond_RStickSidePos": m.write16(SCRATCH + 0x1E, 1); m.write16(SCRATCH + 0x2A, 0xDDB + 1)
        if name == "PlayerCond_RStickFwd": m.write16(SCRATCH + 0x1E, 1); m.write16(SCRATCH + 0x28, 0xDDB + 1)
        if name == "PlayerCond_RStickBack": m.write16(SCRATCH + 0x1E, 1); m.write16(SCRATCH + 0x28, -0xDDB - 1)
        if name == "PlayerCond_RStickBackHard": m.write16(SCRATCH + 0x28, -0xB50 - 1); m.write16(SCRATCH + 0x1E, 0x6F)
        if name == "PlayerCond_Gadget1RStickFwdFlick":
            m.write16(SCRATCH + 0x28, 0xB51); m.write16(SCRATCH + 0x16, 4); m.write8(PLAYER + 0xF2, 1)
        if name == "PlayerCond_Gadget3RStickBack":
            m.write16(SCRATCH + 0x28, -0xB51); m.write16(SCRATCH + 0x1E, 0x6F); m.write8(PLAYER + 0xF2, 3)
        if name in ("PlayerCond_LStickSidePos", "PlayerCond_LStickSideNeg", "PlayerCond_LStickSideAny"):
            m.write16(SCRATCH + 0x2E, 0xB51 if name.endswith("Pos") else (-0xB51 & 0xFFFF))
        if name == "PlayerCond_LStickFwd": m.write16(SCRATCH + 0x2C, 0xB51); m.write16(SCRATCH + 0x14, 1)
        if name == "PlayerCond_LStickBack": m.write16(SCRATCH + 0x2C, -0xB51 & 0xFFFF); m.write16(SCRATCH + 0x14, 1)
        if name == "PlayerCond_FallingHigh": m.write32(PLAYER + 0x28, 0); m.write32(PLAYER + 0x12C, 100); m.write32(PLAYER + 0x34, 0x11)
        if name == "PlayerCond_InWater": m.write16(PLAYER + 0x138, 0x1000)
    elif case == "edge":
        if name == "PlayerCond_FacingDesiredDir":
            m.write16(PLAYER + 0x60, 0x1000); m.write16(PLAYER + 0x64, 0)
            m.write16(PLAYER + 0x74, 0x1000); m.write16(PLAYER + 0x78, 0)
        if name in ("PlayerCond_RStickEvent02", "PlayerCond_RStickEvent40", "PlayerCond_RStickEvent04",
                    "PlayerCond_RStickEvent08", "PlayerCond_RStickEvent10", "PlayerCond_RStickEvent20"):
            for slot in range(40): m.write16(HISTORY_DONE + 2 * slot, 0x1000)
            m.write16(SCRATCH + 0x38, 1 if name != "PlayerCond_RStickEvent40" else 0)
        if name == "PlayerCond_RStickRotating": m.write16(SCRATCH + 0x16, 0x10); m.write16(SCRATCH + 0x30, -0x15 & 0xFFFF)
        if name in ("PlayerCond_FirstFrameRStickEvent10", "PlayerCond_FirstFrameRStickEvent20"):
            m.write16(PLAYER + 0x5E, 3); m.write16(PLAYER + 0x5C, 6); m.write16(STATE_TABLE + 3 * 8, 5)
            m.write16(HISTORY_DONE + 2 * m.read32(HISTORY_POS), 0x10 if name.endswith("10") else 0x20)
        if name == "PlayerCond_JumpPressedAtApex":
            m.write32(SCRATCH + 0x0C, 0x0A); m.write32(SCRATCH + 0x10, 0x0A); m.write32(PLAYER + 0x34, -0x15)
        if name == "PlayerCond_WadingOnGround":
            m.write16(PLAYER + 0x138, 0x1002); m.write16(PLAYER + 0x104, -100); m.write16(PLAYER + 0x106, -100)
        if name == "PlayerCond_CanJump":
            m.write32(SCRATCH + 0x0C, 0x0A); m.write32(SCRATCH + 0x10, 0x0A)
            m.write16(PLAYER + 0x138, 0); m.write16(PLAYER + 0xEC, 0); m.write16(PLAYER + 0x104, -10); m.write16(PLAYER + 0x106, -10)
            m.write16(SURFACE_TABLE + 0x0C, 0x7FFF)
        if name == "PlayerCond_LandingSoon":
            m.write16(PLAYER + 0x136, 1); m.write32(PLAYER + 0x124, -0x10000); m.write32(PLAYER + 0x28, 30000)
        if name == "PlayerCond_NoMoveInput": m.write16(SCRATCH + 0x14, 0); m.write32(SCRATCH + 0x0C, 0)
        if name == "PlayerCond_LStickIdleStillMoving":
            m.write16(SCRATCH + 0x14, 0); m.write32(PLAYER + 0x30, 0x100); m.write32(PLAYER + 0x38, 0)
        if name == "PlayerCond_Still":
            m.write32(PLAYER + 0x30, 0); m.write32(PLAYER + 0x38, 0); m.write16(PLAYER + 0x104, 0x100); m.write16(PLAYER + 0x106, 0)

    if case == "false" and name == "PlayerCond_Still":
        m.write32(PLAYER + 0x30, 5)
    if case == "true" and name == "PlayerCond_GroundBehindLow":
        m.write32(PLAYER + 0x28, 30000)
    if case == "true" and name == "PlayerCond_LandingSoon":
        m.write16(PLAYER + 0x136, 1); m.write32(PLAYER + 0x124, 0x10000); m.write32(PLAYER + 0x28, 30000)
    if case == "true" and name == "PlayerCond_WadingOnGround":
        m.write16(PLAYER + 0x138, 0x1002); m.write16(PLAYER + 0x104, 0); m.write16(PLAYER + 0x106, 0)


def cases_for(name, count, seed):
    # The first two cases deliberately exercise opposite outcomes; the rest broaden field values,
    # loop exits, wraparound, and signed/unsigned boundaries deterministically.
    yield "true", {"seed": seed ^ 0x1111, "solid": -1000, "effect": 0x1357, "stale": (0x1234, 0x5678), "on_ground": 1}
    yield "false", {"seed": seed ^ 0x2222, "solid": 30000, "effect": 0x2468, "stale": (0xFEDC, 0xBA98), "on_ground": 0}
    yield "edge", {"seed": seed ^ 0x3333, "solid": 1000, "effect": 0x3579, "stale": (0xAAAA, 0x5555), "on_ground": 1}
    for i in range(count):
        rng = random.Random(seed * 131 + i * 17 + sum(name.encode("ascii")))
        yield f"random-{i:02}", {"seed": rng.getrandbits(32), "solid": rng.choice([-32768, -20000, -1, 0, 100, 16384, 16385, 32767]),
                                   "effect": rng.getrandbits(16), "stale": (rng.getrandbits(16), rng.getrandbits(16)),
                                   "on_ground": rng.randrange(2)}


def run_suite(h, random_cases, seed):
    for name in FUNCS:
        if h.target_mutation and name != h.target_mutation:
            continue
        for case, ctx in cases_for(name, random_cases, seed):
            failures_before = h.failures
            h.run(name, case, ctx)
            if h.target_mutation and h.failures != failures_before:
                return


def mutation_sources(original):
    """Two function-local wrong returns per original function; the MIPS checker must catch both."""
    for name in FUNCS:
        start = original.index("int32_t ae_" + name + "(")
        brace = original.index("{", start)
        replacements = ("1", "-1") if name in ("PlayerCond_False", "PlayerCond_False2") else ("0", "1")
        for ordinal, value in enumerate(replacements, 1):
            mutated = original[:brace + 1] + f"\n    return {value}; /* deliberate checker mutation {ordinal} */\n" + original[brace + 1:]
            yield name, ordinal, mutated


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--cases", type=int, default=12, help="random cases per function, in addition to three targeted cases")
    parser.add_argument("--seed", type=int, default=94423)
    parser.add_argument("--mutate", action="store_true")
    args = parser.parse_args()
    if args.cases < 0:
        parser.error("--cases must be non-negative")

    if args.mutate:
        original = (ROOT / "src/player/ae_player_cond.c").read_text(encoding="utf-8")
        mutations = list(mutation_sources(original))
        caught = 0
        survivors = []
        for index, (name, ordinal, source) in enumerate(mutations):
            path = ROOT / "extract/mips" / f"playercond_mutation_{index}.c"
            path.write_text(source, encoding="utf-8")
            h = Harness(path, tag=f"playercond_mutation_{index}")
            h.target_mutation = name
            run_suite(h, args.cases, args.seed)
            label = f"{name} mutation {ordinal}"
            if h.failures:
                caught += 1
                print("caught:", label)
            else:
                survivors.append(label)
                print("SURVIVED:", label)
            h.close()
        print(f"Mutations: {len(mutations)}; caught: {caught}; survivors: {len(survivors)}")
        return int(bool(survivors))

    started = time.time()
    h = Harness()
    run_suite(h, args.cases, args.seed)
    for name, cases in h.counts.items():
        print(f"{name:38} {cases}")
    summary, gaps = h.finish(coverage=True,
                             default_run=(args.cases == 12 and args.seed == 94423))
    if gaps:
        print("FAIL: undeclared coverage gaps or stale unreachable declarations")
    else:
        print(f"Instruction coverage: {sum(x['executed'] for x in summary.values())}/"
              f"{sum(x['total'] for x in summary.values())}")
    print(f"Differential cases: {sum(h.counts.values())}; failures: {h.failures}; {time.time()-started:.1f}s")
    h.close()
    return int(bool(h.failures or gaps))


if __name__ == "__main__":
    sys.exit(main())
