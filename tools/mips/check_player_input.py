"""Differential check of the native player's input path against the original MIPS code.

Ported routines (C, on a byte-exact RAM image = AeRam; see src/core/ae_pad.c, src/core/ae_vecnorm.h,
src/player/native_player.c, src/player/native_gadget_slots.c) are run next to the ORIGINAL instructions of SCUS_944.23
(tools/mips/mips_ram.py, GTE through the pinned PCSX-ReARMed oracle) on identical RAM images and identical hook models.
The final RAM (without the top-of-RAM stack window), the scratchpad, the return value and the sequence of hook calls must
be byte-identical.

  VectorNormalSS 800301E0 / VectorNormal 800301B0 (core 80030210)   incl. the add-overflow traps and the player-side
                                                                    ae_player_normalize (EXE table and built-in table)
  ApplyMatrixSV 800300B0, QuadrantFromSigns 8001413C, PadNormalizeSticks 800130E0, PadSampleStickHistory 8001300C
  PadUpdate 8001325C  random RAM states (ring, done flags, gesture state, buffers, timers, camera matrix, flags) and long
                      stick sequences (flicks, pushes, spins both ways, ring wrap) compared after every frame
  CameraLookAt 80046670 (the g_CameraRotMatrix part; its other callees are hooked)
  CanSwitchGadget 80066F50 / IsPlayerSwimming 80068B7C / PlayerCheckGadgetSelect 80067974
  GadgetSlotsInitForExit 8007B16C / InitForArea 8007AFC4 / Save 8007C9D8 / Restore 8007CA10 (new-game defaults)

Hooked (not ported, identical model on both sides): PadGetState, PadInfoMode, PadSetupDualShock, PadDemoPlaybackStep,
StageRequest, StickToButtons, PadRumbleReset, PlayerSelectGadget, HudFindGadgetIconPos.
Needs extract/disc/SCUS_944.23 (skips if it is missing).

Usage: uv run python tools/mips/check_player_input.py [--cases PERCENT] [--seed S] [--coverage]
"""
import argparse
import ctypes as C
import math
import os
import random
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mips_ram as mr  # noqa: E402
from mips_ram import signed  # noqa: E402
import exclusion_registry as reg  # noqa: E402

OUT = ROOT / "extract/mips"
M32 = 0xFFFFFFFF
STACK_LO = mr.STACK_WINDOW[0] - 0x80000000

# ---- addresses
BUF1, BUF2 = 0x800E52D0, 0x800E52F2
B = 0x800E5320                      # stick history ring (40 x 4 bytes)
LATEST, DONE, LMAGH, FLICKH = 0x800E53C0, 0x800E53C4, 0x800E5414, 0x800E543C
PAD_ID1, PAD_ID2, RUMBLE0, RUMBLE1 = 0x800E5464, 0x800E5466, 0x800E546A, 0x800E546B
HELD1, HELD2, CHG1, CHG2, REP1, REP2, POS, DS_STATE = (0x800E5478, 0x800E547C, 0x800E5480, 0x800E5484, 0x800E5488,
                                                       0x800E548C, 0x800E5490, 0x800E5494)
PUSH, FLICK, EVENTS, RMAG, LMAG = 0x800E5498, 0x800E549A, 0x800E549E, 0x800E54A0, 0x800E54A2
RDIR, LDIR, RCAM, LCAM, LSPIN, RSPIN = 0x800E54A4, 0x800E54AC, 0x800E54B4, 0x800E54BC, 0x800E54C4, 0x800E54C8
CC_FWD, CC_SIDE, CR_FWD, CR_SIDE, ROT, TMR1, TMR2, TRAVL, TRAVR, AIM = (0x800E54CC, 0x800E54CE, 0x800E54D0, 0x800E54D2,
                                                                      0x800E54D4, 0x800E54D8, 0x800E54DA, 0x800E54DC,
                                                                      0x800E54DE, 0x800E54E0)
DEMO, FACING, CAMMAT, AREA, ENTRY = 0x800A8A58, 0x800EC240, 0x800EC330, 0x800F4476, 0x800F447A
PAD_STATE_WINDOW = (0x800E52C0, 0x800E5500)

ADDR = dict(PadUpdate=0x8001325C, PadNormalizeSticks=0x800130E0, PadSampleStickHistory=0x8001300C,
            QuadrantFromSigns=0x8001413C, ApplyMatrixSV=0x800300B0, VectorNormal=0x800301B0, VectorNormalSS=0x800301E0,
            CameraLookAt=0x80046670, CanSwitchGadget=0x80066F50, IsPlayerSwimming=0x80068B7C,
            PlayerCheckGadgetSelect=0x80067974, GadgetSlotsInitForExit=0x8007B16C, GadgetSlotsInitForArea=0x8007AFC4,
            GadgetSlotsSave=0x8007C9D8, GadgetSlotsRestore=0x8007CA10)
# (name, start, size) of the original code whose coverage is reported
COVER = [("PadUpdate", 0x8001325C, 3672), ("PadNormalizeSticks", 0x800130E0, 72), ("PadSampleStickHistory", 0x8001300C, 212),
         ("QuadrantFromSigns", 0x8001413C, 44), ("ApplyMatrixSV", 0x800300B0, 92),
         ("VectorNormal + SS + core", 0x800301B0, 0x120), ("CameraLookAt (rotMatrix part)", 0x80046670, 0x1F4),
         ("CanSwitchGadget", 0x80066F50, 120), ("IsPlayerSwimming", 0x80068B7C, 68),
         ("PlayerCheckGadgetSelect", 0x80067974, 168), ("GadgetSlotsInitForExit", 0x8007B16C, 492),
         ("GadgetSlotsInitForArea", 0x8007AFC4, 424), ("GadgetSlotsSave+Restore", 0x8007C9D8, 0x70)]


def s32(v): return signed(v, 32)
def s16(v): return signed(v, 16)
def s8(v): return signed(v, 8)


def build_dll(pad_src=None):
    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob("playerinput_*.dll"):
        try:
            if time.time() - old.stat().st_mtime > 600:
                old.unlink()
        except OSError:
            pass
    dll = OUT / f"playerinput_{os.getpid()}.dll"
    srcs = [ROOT / "src/core/ae_ram.c", Path(pad_src) if pad_src else ROOT / "src/core/ae_pad.c", ROOT / "src/player/native_player.c",
            ROOT / "src/player/native_gadget_slots.c"]
    subprocess.run([shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-shared", "-static-libgcc",
                    "-I", str(ROOT / "src/core"), *map(str, srcs), "-o", str(dll), "-lm"], check=True)
    return dll


P, U32, I32, VP = C.c_void_p, C.c_uint32, C.c_int32, C.c_void_p
CB = C.CFUNCTYPE


class PadHooks(C.Structure):
    _fields_ = [("user", VP),
                ("pad_get_state", CB(I32, VP, VP, I32)),
                ("pad_info_mode", CB(I32, VP, VP, I32, I32, I32)),
                ("pad_setup_dualshock", CB(None, VP, VP, I32)),
                ("pad_demo_step", CB(I32, VP, VP)),
                ("stage_request", CB(None, VP, VP, I32, I32, I32)),
                ("stick_to_buttons", CB(U32, VP, VP)),
                ("pad_rumble_reset", CB(None, VP, VP, I32))]


class GadgetHooks(C.Structure):
    _fields_ = [("user", VP), ("select_gadget", CB(None, VP, VP, I32, I32)), ("hud_find_icon_pos", CB(None, VP, VP))]


class Model:
    """Deterministic behaviour of every hook (identical on both sides); every call is logged."""

    def __init__(self, seed):
        self.seed, self.log, self.n = seed, [], {}

    def rng(self, name):
        k = self.n.get(name, 0)
        self.n[name] = k + 1
        return random.Random(f"{self.seed}/{name}/{k}")

    def get_state(self, port):
        self.log.append(("get_state", port & M32))
        return self.rng("gs").choice([0, 1, 2, 6, 6, 6, 6, 7, 3, -1, 5, 0, 1])

    def info_mode(self, a, b, c):
        self.log.append(("info_mode", a & M32, b & M32, c & M32))
        return self.rng("im").choice([7, 7, 0, 3, 1])

    def setup(self, arg): self.log.append(("setup", arg & M32))

    def demo(self):
        self.log.append(("demo",))
        return self.rng("demo").choice([0, 0, 0x20, 0x22, 0x20])

    def stage(self, a, b, c): self.log.append(("stage", a & M32, b & M32, c & M32))

    def sticks(self):
        self.log.append(("sticks",))
        return self.rng("st").getrandbits(32)

    def rumble(self, port): self.log.append(("rumble", port & M32))
    def select_gadget(self, g, old): self.log.append(("select", g & M32, old & M32))
    def hud(self): self.log.append(("hud",))


def mips_pad_hooks(model):
    def void(fn):
        def inner(cpu, r):
            fn(r)
            return 0
        return inner
    return {
        0x8002B52C: lambda cpu, r: model.get_state(r[4]),
        0x8002B5F8: lambda cpu, r: model.info_mode(r[4], r[5], r[6]),
        0x80012CA0: void(lambda r: model.setup(r[4])),
        0x80013128: lambda cpu, r: model.demo(),
        0x8005D100: void(lambda r: model.stage(r[4], r[5], r[6])),
        0x800140B4: lambda cpu, r: model.sticks(),
        0x80014208: void(lambda r: model.rumble(r[4])),
    }


def mips_gadget_hooks(model):
    def void(fn):
        def inner(cpu, r):
            fn(r)
            return 0
        return inner
    return {0x8007AC64: void(lambda r: model.select_gadget(r[4], r[5])), 0x8008197C: void(lambda r: model.hud())}


class Harness:
    def __init__(self, dll, coverage):
        self.m = mr.MipsRam.from_extract()
        self.m.attach_gte_oracle()
        if coverage:
            self.m.coverage = set()
        self.base = self.m.snapshot()
        reg.declare("stack_window", f"{mr.STACK_WINDOW[0]:08X}..{mr.STACK_WINDOW[1]:08X}")
        self.lib = lib = C.CDLL(str(dll))
        lib.ae_ram_create.restype = P
        lib.ae_ptr.restype = P
        lib.ae_ptr.argtypes = [P, U32, U32]
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        lib.ae_ram_clear_error.argtypes = [P]
        lib.ae_wr8.argtypes = [P, U32, U32]
        lib.ae_wr16.argtypes = [P, U32, U32]
        lib.ae_wr32.argtypes = [P, U32, U32]
        lib.ae_pad_VectorNormalSS.argtypes = [P, U32, U32]
        lib.ae_pad_VectorNormalSS.restype = I32
        lib.ae_pad_VectorNormal.argtypes = [P, U32, U32]
        lib.ae_pad_VectorNormal.restype = I32
        lib.ae_pad_ApplyMatrixSV.argtypes = [P, U32, U32, U32]
        lib.ae_pad_ApplyMatrixSV.restype = U32
        lib.ae_pad_QuadrantFromSigns.argtypes = [I32, I32]
        lib.ae_pad_QuadrantFromSigns.restype = I32
        lib.ae_pad_PadNormalizeSticks.argtypes = [P, U32]
        lib.ae_pad_PadSampleStickHistory.argtypes = [P]
        lib.ae_pad_PadUpdate.argtypes = [P, P, U32]
        lib.ae_pad_CameraRotMatrix.argtypes = [P, C.POINTER(C.c_int16), C.POINTER(C.c_int16), U32]
        lib.ae_player_data_init.argtypes = [P]
        lib.ae_player_data_load_exe.argtypes = [P, C.c_char_p, C.c_size_t]
        lib.ae_player_data_load_exe.restype = I32
        lib.ae_player_normalize.argtypes = [P, C.POINTER(C.c_int16), C.POINTER(C.c_int16)]
        lib.ae_player_normalize.restype = I32
        lib.ae_player_normalize_traps.restype = U32
        lib.ae_player_can_switch_gadget.argtypes = [C.c_uint16, C.c_uint16]
        lib.ae_player_can_switch_gadget.restype = I32
        lib.ae_player_is_swimming.argtypes = [C.c_uint16]
        lib.ae_player_is_swimming.restype = I32
        lib.ae_player_gadget_select_request.argtypes = [C.c_uint16, C.c_uint16, U32, U32, C.POINTER(C.c_int8), C.POINTER(I32)]
        lib.ae_player_gadget_select_request.restype = I32
        lib.ae_GadgetSlotsSave.argtypes = [P]
        lib.ae_GadgetSlotsRestore.argtypes = [P]
        lib.ae_GadgetSlotsInitForArea.argtypes = [P, I32]
        lib.ae_GadgetSlotsInitForArea.restype = I32
        lib.ae_GadgetSlotsInitForExit.argtypes = [P, P]
        self.ram = lib.ae_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scr = (C.c_uint8 * 0x400).from_address(lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        # player data (opaque to python): one with the built-in table, one loaded from the EXE
        self.d_builtin = C.create_string_buffer(1 << 16)
        self.d_exe = C.create_string_buffer(1 << 16)
        lib.ae_player_data_init(self.d_builtin)
        lib.ae_player_data_init(self.d_exe)
        exe = (ROOT / "extract/disc/SCUS_944.23").read_bytes()
        self.exe_ok = lib.ae_player_data_load_exe(self.d_exe, exe, len(exe))
        self.model = None
        self.pad_hooks = PadHooks()
        self.gadget_hooks = GadgetHooks()
        self._keep = []
        m = lambda: self.model  # noqa: E731
        pad = {
            "pad_get_state": lambda u, r, port: m().get_state(port),
            "pad_info_mode": lambda u, r, a, b, c: m().info_mode(a, b, c),
            "pad_setup_dualshock": lambda u, r, arg: m().setup(arg),
            "pad_demo_step": lambda u, r: m().demo(),
            "stage_request": lambda u, r, a, b, c: m().stage(a, b, c),
            "stick_to_buttons": lambda u, r: m().sticks(),
            "pad_rumble_reset": lambda u, r, port: m().rumble(port),
        }
        for name, fn in pad.items():
            cb = dict(PadHooks._fields_)[name](fn)
            self._keep.append(cb)
            setattr(self.pad_hooks, name, cb)
        gad = {"select_gadget": lambda u, r, g, old: m().select_gadget(g, old), "hud_find_icon_pos": lambda u, r: m().hud()}
        for name, fn in gad.items():
            cb = dict(GadgetHooks._fields_)[name](fn)
            self._keep.append(cb)
            setattr(self.gadget_hooks, name, cb)
        self.null_pad, self.null_gadget = PadHooks(), GadgetHooks()
        self.failures = 0
        self.counts = {}
        self.traps = 0
        self.compared_bytes = 0

    # ------------------------------------------------------------------ plumbing
    def sync_to_c(self):
        C.memmove(self.main, bytes(self.m.ram), 0x200000)
        C.memmove(self.scr, bytes(self.m.scratch), 0x400)
        self.lib.ae_ram_clear_error(self.ram)

    def reset(self):
        self.m.restore(self.base)
        self.m.gte_reset()

    def fail(self, name, label, *lines):
        self.failures += 1
        print(f"FAIL {name} [{label}]")
        for line in lines:
            print("   ", line)

    def window_equal(self, lo, hi):
        a = bytes(self.m.ram[lo & 0x1FFFFF:hi & 0x1FFFFF])
        b = bytes(self.main[lo & 0x1FFFFF:hi & 0x1FFFFF])
        return a == b, a, b

    def compare(self, name, label, mret, cret, mlog, clog, extra=()):
        problems = list(extra)
        if self.lib.ae_ram_failed(self.ram):
            problems.append("C error flag: " + self.lib.ae_ram_error(self.ram).decode())
        if mret is not None and cret is not None and (mret & M32) != (cret & M32):
            problems.append(f"return value MIPS {mret & M32:08X} C {cret & M32:08X}")
        if mlog != clog:
            problems.append(f"hook log differs\n      MIPS {mlog}\n      C    {clog}")
        a, b = bytes(self.m.ram)[:STACK_LO], bytes(self.main)[:STACK_LO]
        self.compared_bytes += len(a)
        if a != b:
            diffs = [i for i in range(STACK_LO) if a[i] != b[i]]
            problems.append(f"RAM differs ({len(diffs)} bytes) at " +
                            ", ".join(f"{0x80000000 + i:08X}(mips {a[i]:02X} c {b[i]:02X})" for i in diffs[:8]))
        if bytes(self.m.scratch) != bytes(self.scr):
            diffs = [i for i in range(0x400) if self.m.scratch[i] != self.scr[i]]
            problems.append("scratchpad differs at " + ", ".join(
                f"{0x1F800000 + i:08X}(mips {self.m.scratch[i]:02X} c {self.scr[i]:02X})" for i in diffs[:8]))
        if problems:
            self.fail(name, label, *problems)
        return not problems

    def count(self, name):
        self.counts[name] = self.counts.get(name, 0) + 1

    def run(self, name, label, setup, mips_call, c_call, seed=0, *, ret=True, expect_trap=False):
        """setup(m) builds the scenario in the MIPS image; both sides then run on identical copies."""
        self.reset()
        ctx = setup(self.m)
        self.sync_to_c()
        mm, cm = Model(f"{seed}/{name}"), Model(f"{seed}/{name}")
        self.model = cm
        self.count(name)
        trap = None
        mret = None
        try:
            regs = mips_call(ctx, mm)
            mret = regs[2]
        except mr.MipsError as e:
            trap = str(e)
        cret = c_call(ctx)
        if expect_trap or trap is not None:
            failed = bool(self.lib.ae_ram_failed(self.ram))
            if trap is None or not failed or "overflow" not in trap or "overflow" not in self.lib.ae_ram_error(self.ram).decode():
                self.fail(name, label, f"original trap {trap!r}, port flagged={failed} "
                          f"({self.lib.ae_ram_error(self.ram).decode()})")
            else:
                self.traps += 1
            return
        self.compare(name, label, mret if ret else None, cret if ret else None, mm.log, cm.log)


# ------------------------------------------------------------------------------------------------ random state
def rand16(rng):
    k = rng.random()
    if k < 0.15: return rng.choice([0, 1, -1, 2, -2, 0x7FFF, -0x8000, 0x4000, -0x4000, 0x1000, -0x1000, 0xB50, -0xB50])
    if k < 0.45: return rng.randint(-300, 300)
    if k < 0.75: return rng.randint(-0x1000, 0x1000)
    return rng.randint(-0x8000, 0x7FFF)


def rand_stick_byte(rng):
    k = rng.random()
    if k < 0.3: return rng.choice([0x80, 0x80, 0x7F, 0x81])
    if k < 0.5: return rng.choice([0, 0xFF, 1, 0xFE])
    if k < 0.7: return 0x80 + rng.randint(-24, 24)
    return rng.randint(0, 255)


def rand_ring_byte(rng):
    k = rng.random()
    if k < 0.2: return 0
    if k < 0.4: return rng.choice([0x7F, 0x80, 0x81, 0x7E, 0x40, 0xC0, 0x46, 0xBA, 0x45, 0xBB])
    if k < 0.6: return rng.randint(0, 255)
    return (rng.randint(-100, 100)) & 0xFF


def rand_yaw_matrix(rng):
    a = rng.random() * 2 * math.pi
    nx, nz = int(round(math.sin(a) * 4096)), int(round(math.cos(a) * 4096))
    return [nz, 0, nx, 0, 0x1000, 0, -nx, 0, nz]


def write_matrix(m, addr, vals):
    for i, v in enumerate(vals):
        m.write16(addr + 2 * i, v & 0xFFFF)


def setup_pad_state(m, rng, flavour):
    w8, w16, w32 = m.write8, m.write16, m.write32
    # raw buffers
    for buf in (BUF1, BUF2):
        w8(buf, rng.choice([0, 0, 0, 0, 0xFF]))
        w8(buf + 1, rng.choice([0x73, 0x73, 0x73, 0x41, 0x53, 0x23, 0x12, 0x70, 0x72, rng.randint(0, 255)]))
        for i in (2, 3):
            w8(buf + i, rng.choice([0xFF, 0xFF, 0xFF, 0, rng.randint(0, 255)]))
        for i in range(4, 8):
            w8(buf + i, rand_stick_byte(rng))
    pos = rng.randint(0, 39)
    w32(POS, pos)
    # ring + done flags
    for i in range(160):
        w8(B + i, rand_ring_byte(rng))
    w16(0x800E53C0, 0)
    k = rng.choice([0, 1, 1, 1, 1, 2, 3, 5, 12, 29, 30, 31, 39, 40, 40]) if flavour != "fuzz" else rng.randint(0, 40)
    for j in range(40):
        age = (pos - j) % 40            # 0 = newest
        done = 0 if age < k else rng.choice([1, 1, 1, 2, 4, 8, 0x10, 0x20, 0x40, rng.getrandbits(16)])
        if flavour == "fuzz" and rng.random() < 0.15:
            done = rng.choice([0, 1])
        w16(DONE + 2 * j, done)
    for j in range(40):
        w8(LMAGH + j, rng.getrandbits(8))
        w8(FLICKH + j, rng.getrandbits(8))
    # gesture / globals
    w16(FLICK, rng.choice([0, 0, 0, 4, 8, 0x10, 0x20, 0x80, 0x80, 0x80, 1, 0xC, 0x40, 0x100, 0x84, rng.getrandbits(16)]))
    w16(PUSH, rng.choice([0, 2, 4, 8, 1, 3, 5, 0x10, rng.getrandbits(16)]))
    for a in (EVENTS, RMAG, LMAG, ROT, CC_FWD, CC_SIDE, CR_FWD, CR_SIDE, TRAVL, TRAVR):
        w16(a, rng.getrandbits(16))
    for a in (LSPIN, RSPIN, HELD1, HELD2, CHG1, CHG2, REP1, REP2):
        w32(a, rng.getrandbits(32))
    for a in (RDIR, LDIR, RCAM, LCAM):
        for i in range(4):
            w16(a + 2 * i, rand16(rng))
    w16(TMR1, rng.choice([0, 1, 0x10, 0x11, 0x12, 0x20, 5, -1, -0x8000, 0x7FFF, rng.getrandbits(16)]))
    w16(TMR2, rng.choice([0, 1, 0x10, 0x11, 0x12, 0x20, 5, -1, -0x8000, 0x7FFF, rng.getrandbits(16)]))
    w32(DS_STATE, rng.choice([0, 1, 2, 3, 3, 7, rng.getrandbits(32)]))
    w8(RUMBLE0, rng.choice([0, 1, 2, 0xFF, rng.getrandbits(8)]))
    w8(RUMBLE1, rng.choice([0, 1, 2, 0xFF, rng.getrandbits(8)]))
    w32(AIM, rng.choice([0, 0, 0, 1, rng.getrandbits(32)]))
    w32(DEMO, rng.choice([0] * 12 + [1, 5]))
    w16(AREA, rng.getrandbits(16))
    w16(ENTRY, rng.getrandbits(16))
    fx, fz = rng.choice([(0, 0), (0x1000, 0), (0, 0x1000), (-0x1000, 0), (0, -0x1000)]) if rng.random() < 0.3 else (rand16(rng), rand16(rng))
    w16(FACING, fx)
    w16(FACING + 4, fz)
    w16(FACING + 2, rand16(rng))
    if rng.random() < 0.75:
        write_matrix(m, CAMMAT, rand_yaw_matrix(rng))
    else:
        write_matrix(m, CAMMAT, [rand16(rng) for _ in range(9)])
    for i in range(3):
        m.write32(CAMMAT + 0x14 + 4 * i, rng.getrandbits(32))
    return rng.choice([1, 1, 1, 1, 0, 3, 2])


def settle(m, rng):
    """A quiet pad: ring all zero and processed, nothing pressed, centred sticks (state of a running game)."""
    for i in range(160):
        m.write8(B + i, 0)
    for j in range(40):
        m.write16(DONE + 2 * j, 1)
        m.write8(LMAGH + j, 0)
        m.write8(FLICKH + j, 0)
    for a in (FLICK, PUSH, EVENTS, RMAG, LMAG, ROT, CC_FWD, CC_SIDE, CR_FWD, CR_SIDE, TRAVL, TRAVR, TMR1, TMR2):
        m.write16(a, 0)
    for a in (LSPIN, RSPIN, REP1, REP2, CHG1, CHG2):
        m.write32(a, 0)
    m.write32(HELD1, 0xFFFF0000)
    m.write32(HELD2, 0xFFFFFFFF)
    m.write32(POS, rng.randint(0, 39))
    m.write32(DS_STATE, 3)
    m.write8(RUMBLE0, 0)
    m.write8(RUMBLE1, 0)
    m.write32(AIM, 0)
    m.write32(DEMO, 0)
    write_matrix(m, CAMMAT, rand_yaw_matrix(rng))
    for buf in (BUF1, BUF2):
        m.write8(buf, 0)
        m.write8(buf + 1, 0x73)
        m.write8(buf + 2, 0xFF)
        m.write8(buf + 3, 0xFF)
        for i in range(4, 8):
            m.write8(buf + i, 0x80)
    m.write8(BUF2, 0xFF)


# ------------------------------------------------------------------------------------------------ stick sequences
def stick_sequence(kind, rng, n):
    """Per frame (rx, ry, lx, ly): raw bytes (0x80 centre, y down = larger)."""
    out = []
    centre = (0x80, 0x80)

    def byte(v): return max(0, min(255, int(round(v))))

    def pos(angle, radius): return (byte(0x80 + math.cos(angle) * radius), byte(0x80 + math.sin(angle) * radius))

    if kind == "spin":                      # gadget stick circles (either way, speed, radius), then releases
        a, sp, rad = rng.random() * 6.28, rng.choice([0.2, 0.35, 0.5, 0.8, 1.2, 1.7]) * rng.choice([1, -1]), rng.choice([127, 120, 100, 90, 70])
        for i in range(n):
            out.append(pos(a, rad) if i < n * 0.7 else centre)
            a += sp
    elif kind == "flick":                   # centre -> full in 1..3 frames in a random direction, hold, release, repeat
        i = 0
        while i < n:
            a, rad = rng.random() * 6.28, rng.choice([127, 127, 120, 110, 100, 90])
            ramp = rng.choice([1, 1, 2, 3, 4])
            hold = rng.randint(2, 14)
            for r_ in range(ramp):
                out.append(pos(a, rad * (r_ + 1) / ramp))
            out += [pos(a + rng.random() * 0.1, rad)] * hold
            out += [centre] * rng.randint(1, 8)
            i = len(out)
    elif kind == "hold_spin":               # push, hold, then start circling while held (0x80 -> 0x10 / 0x20 transitions)
        a, sp = rng.random() * 6.28, rng.choice([0.25, 0.4, 0.6, -0.25, -0.4, -0.6])
        out += [pos(a, 127)] * rng.randint(3, 10)
        for i in range(n):
            out.append(pos(a, rng.choice([127, 110, 90])))
            a += sp * (1 if i > 5 else 0)
    elif kind == "creep":                   # slow ramp to full tilt (a push, not a flick), hold, back, other direction
        a = rng.random() * 6.28
        steps = rng.choice([12, 20, 30])
        for i in range(n):
            ph = (i % (2 * steps + 10))
            rad = 127 * min(1.0, ph / steps) if ph < steps + 10 else 127 * (2 * steps + 10 - ph) / steps
            if ph == 0:
                a += rng.random() * 3
            out.append(pos(a, max(0, rad)))
    else:                                   # random walk with big jumps
        x, y = 0x80, 0x80
        for i in range(n):
            if rng.random() < 0.2:
                x, y = rand_stick_byte(rng), rand_stick_byte(rng)
            else:
                x, y = byte(x + rng.randint(-30, 30)), byte(y + rng.randint(-30, 30))
            out.append((x, y))
    out = out[:n] + [centre] * max(0, n - len(out))
    res = []
    mv = 0x80, 0x80
    for rx, ry in out:                      # the movement stick: independent slower sequence
        if rng.random() < 0.1:
            mv = (rand_stick_byte(rng), rand_stick_byte(rng))
        res.append((rx, ry) + mv)
    return res


# ------------------------------------------------------------------------------------------------ the checks
def check_vectors(h, scale, seed):
    lib = h.lib
    IN, OUT_, OUT2 = 0x80120000, 0x80120040, 0x80120080

    def rand_vec(rng):
        k = rng.random()
        if k < 0.1: return [0, 0, 0]
        if k < 0.25: return [rng.choice([0, 1, -1, 2, 3, 0x1000, -0x1000, 0x7FFF, -0x8000]) for _ in range(3)]
        if k < 0.4: return [rng.randint(-3, 3) for _ in range(3)]
        if k < 0.6: return [rng.randint(-128, 127), 0, rng.randint(-128, 127)]
        if k < 0.75: return [rng.randint(-0x1000, 0x1000) for _ in range(3)]
        if k < 0.85: return [rng.randint(-0x400, 0x400) for _ in range(3)]
        if k < 0.9:  # near the add-overflow boundary
            return [rng.choice([0x7FFF, -0x8000, 0x7FFF - rng.randint(0, 3), 0xB505, 0x5A82, -0x5A82]) for _ in range(3)]
        return [rng.randint(-0x8000, 0x7FFF) for _ in range(3)]

    for i in range(int(4000 * scale)):
        rng = random.Random(f"{seed}/vss/{i}")
        v = rand_vec(rng)
        inplace = rng.random() < 0.2

        def setup(m, v=v):
            for k in range(3):
                m.write16(IN + 2 * k, v[k] & 0xFFFF)
                m.write16(OUT_ + 2 * k, 0x5A5A)
            return None

        out = IN if inplace else OUT_
        h.run("VectorNormalSS", f"#{i} {v}", setup,
              lambda ctx, mm, out=out: h.m.call(ADDR["VectorNormalSS"], [IN, out]),
              lambda ctx, out=out: lib.ae_pad_VectorNormalSS(h.ram, IN, out), seed=i)
    for i in range(int(3000 * scale)):
        rng = random.Random(f"{seed}/vn/{i}")
        v = rand_vec(rng)
        hi = [rng.choice([0, 0, 0, 0xFFFF, rng.getrandbits(16)]) for _ in range(3)]    # the upper halves must be ignored

        def setup(m, v=v, hi=hi):
            for k in range(3):
                m.write32(IN + 4 * k, ((hi[k] << 16) | (v[k] & 0xFFFF)) & M32)
                m.write32(OUT_ + 4 * k, 0xA5A5A5A5)
            return None

        h.run("VectorNormal", f"#{i} {v}", setup,
              lambda ctx, mm: h.m.call(ADDR["VectorNormal"], [IN, OUT_]),
              lambda ctx: lib.ae_pad_VectorNormal(h.ram, IN, OUT_), seed=i)
    # the player-side VectorNormalSS (exact LUT from the EXE and the built-in table)
    exe_tab = h.m.read_bytes(0x800A9C14, 384)
    builtin = struct.pack("<192h", *[math.isqrt(2 ** 30 // (k + 64)) for k in range(192)])
    if exe_tab != builtin:
        h.fail("lut", "built-in table", "the built-in VectorNormal table differs from the EXE bytes")
    h.count("lut (192 entries, built-in vs EXE)")
    if not h.exe_ok:
        h.fail("lut", "load_exe", "ae_player_data_load_exe failed")
    n_player = int(3000 * scale)
    for i in range(n_player):
        rng = random.Random(f"{seed}/pn/{i}")
        v = rand_vec(rng)
        for tag, d in (("exe", h.d_exe), ("builtin", h.d_builtin)):
            h.reset()
            for k in range(3):
                h.m.write16(IN + 2 * k, v[k] & 0xFFFF)
            trap_expected = False
            try:
                regs = h.m.call(ADDR["VectorNormalSS"], [IN, OUT_])
                mret = regs[2]
                mout = [s16(h.m.read16(OUT_ + 2 * k)) for k in range(3)]
            except mr.MipsError:
                trap_expected = True
            vin = (C.c_int16 * 3)(*v)
            vout = (C.c_int16 * 3)(-1234, -1234, -1234)
            before = lib.ae_player_normalize_traps()
            cret = lib.ae_player_normalize(d, vin, vout)
            h.count(f"ae_player_normalize ({tag} table)")
            if trap_expected:
                if lib.ae_player_normalize_traps() != before + 1 or list(vout) != [-1234] * 3:
                    h.fail("ae_player_normalize", f"#{i} {v}", "the original traps; the port must count it and write nothing")
                else:
                    h.traps += 1
            elif cret != s32(mret) or list(vout) != mout:
                h.fail("ae_player_normalize", f"#{i} {tag} {v}", f"MIPS ret {s32(mret)} out {mout}; C ret {cret} out {list(vout)}")
    # ApplyMatrixSV
    MAT, VIN, VOUT = 0x80120100, 0x80120140, 0x80120180
    for i in range(int(3500 * scale)):
        rng = random.Random(f"{seed}/amsv/{i}")
        mat = rand_yaw_matrix(rng) if rng.random() < 0.5 else [rand16(rng) for _ in range(9)]
        vec = [rand16(rng), rand16(rng) if rng.random() < 0.5 else 0, rand16(rng)]
        if rng.random() < 0.15:
            vec = [rng.choice([0x7FFF, -0x8000]) for _ in range(3)]
            mat = [rng.choice([0x7FFF, -0x8000, 0x1000]) for _ in range(9)]

        def setup(m, mat=mat, vec=vec, rng=rng):
            write_matrix(m, MAT, mat)
            m.write16(MAT + 18, rng.getrandbits(16))
            for k in range(3):
                m.write16(VIN + 2 * k, vec[k] & 0xFFFF)
            m.write16(VIN + 6, rng.getrandbits(16))
            for k in range(3):
                m.write16(VOUT + 2 * k, 0x5A5A)
            return None

        h.run("ApplyMatrixSV", f"#{i}", setup, lambda ctx, mm: h.m.call(ADDR["ApplyMatrixSV"], [MAT, VIN, VOUT]),
              lambda ctx: lib.ae_pad_ApplyMatrixSV(h.ram, MAT, VIN, VOUT), seed=i)
    # QuadrantFromSigns
    vals = [0, 1, -1, 2, -2, 127, -128, 0x7FFFFFFF, -0x80000000]
    for a in vals:
        for b in vals:
            h.reset()
            mret = h.m.call(ADDR["QuadrantFromSigns"], [a, b])[2]
            h.count("QuadrantFromSigns")
            if s32(mret) != lib.ae_pad_QuadrantFromSigns(a, b):
                h.fail("QuadrantFromSigns", f"{a} {b}", f"MIPS {s32(mret)} C {lib.ae_pad_QuadrantFromSigns(a, b)}")


def check_pad_helpers(h, scale, seed):
    lib = h.lib
    for i in range(int(1500 * scale)):
        rng = random.Random(f"{seed}/pns/{i}")
        buf = rng.choice([BUF1, BUF2])

        def setup(m, rng=rng, buf=buf):
            for k in range(8):
                m.write8(buf + k, rng.choice([0xFF, 0x80, rng.getrandbits(8), 0, 0x73, 0x41]))
            m.write8(buf, rng.choice([0xFF, 0, 0xFF]))
            m.write8(buf + 1, rng.choice([0x73, 0x41, 0x12, 0x20, 0x53, rng.getrandbits(8)]))
        h.run("PadNormalizeSticks", f"#{i}", setup, lambda ctx, mm, buf=buf: h.m.call(ADDR["PadNormalizeSticks"], [buf]),
              lambda ctx, buf=buf: lib.ae_pad_PadNormalizeSticks(h.ram, buf), seed=i, ret=False)

        def setup2(m, rng=rng):
            setup_pad_state(m, rng, "sample")
        h.run("PadSampleStickHistory", f"#{i}", setup2, lambda ctx, mm: h.m.call(ADDR["PadSampleStickHistory"], []),
              lambda ctx: lib.ae_pad_PadSampleStickHistory(h.ram), seed=i, ret=False)


def run_pad_update(h, label, setup, seed, flags_from_setup=True, null_hooks=False):
    def mips(ctx, mm):
        return h.m.call(ADDR["PadUpdate"], [ctx], hooks=mips_pad_hooks(mm))

    def c(ctx):
        h.lib.ae_pad_PadUpdate(h.ram, C.byref(h.null_pad if null_hooks else h.pad_hooks), ctx)

    if null_hooks:
        # fail closed: if the original reaches a hook the port must flag it; otherwise both must agree exactly
        h.reset()
        ctx = setup(h.m)
        h.sync_to_c()
        mm = Model(f"{seed}/nh")
        h.model = Model(f"{seed}/nh")
        h.count("PadUpdate (NULL hooks)")
        try:
            h.m.call(ADDR["PadUpdate"], [ctx], hooks=mips_pad_hooks(mm))
        except mr.MipsError as e:
            h.fail("PadUpdate NULL", label, "MIPS error " + str(e))
            return
        c(ctx)
        flagged = bool(h.lib.ae_ram_failed(h.ram))
        if not mm.log:
            h.compare("PadUpdate NULL", label, None, None, [], [])
        elif not flagged or b"not provided" not in h.lib.ae_ram_error(h.ram):
            h.fail("PadUpdate NULL", label, f"a hook was needed ({mm.log[:2]}) but the port did not flag it")
        return

    def mips2(ctx, mm):
        return mips(ctx, mm)
    h.run("PadUpdate", label, setup, mips2, c, seed=seed, ret=False)


def check_pad_update(h, scale, seed):
    # random states
    for i in range(int(900 * scale)):
        flavour = "fuzz" if i % 4 == 3 else "run"

        def setup(m, i=i, flavour=flavour):
            rng = random.Random(f"{seed}/pu/{i}")
            return setup_pad_state(m, rng, flavour)
        run_pad_update(h, f"state #{i}", setup, i)
    for i in range(int(120 * scale)):
        def setup(m, i=i):
            rng = random.Random(f"{seed}/pun/{i}")
            return setup_pad_state(m, rng, "run")
        run_pad_update(h, f"NULL hooks #{i}", setup, i, null_hooks=True)
    # exact gesture-state transitions: every gesture state x every kind of newest sample, with the settled ring
    lib = h.lib
    samples = [(0, 0), (127, 0), (-127, 0), (0, 127), (0, -127), (90, 90), (-90, 90), (90, -90), (-90, -90), (30, 30),
               (45, 0), (0, 46), (-46, 0), (127, 127), (-128, -128), (70, 0), (69, 0), (35, 34), (35, 35)]
    i = 0
    for state in (0, 4, 8, 0x10, 0x20, 0x80, 0x40, 3):
        for (sx, sy) in samples:
            for prev in ((0, 0), (127, 0), (60, 60)):
                def setup(m, state=state, sx=sx, sy=sy, prev=prev, i=i):
                    rng = random.Random(f"{seed}/gs/{i}")
                    settle(m, rng)
                    m.write16(FLICK, state)
                    pos = m.read32(POS)
                    newest = (pos + 1) % 40                  # the sampler normally advances the position and clears done
                    m.write32(POS, newest)
                    m.write16(DONE + 2 * newest, 0)
                    # previous samples carry some direction so that the quadrant walk has something to count
                    for age in range(1, 40):
                        j = (newest - age) % 40
                        a = rng.random() * 6.28
                        mag = prev[0] if age < 3 else rng.choice([0, 30, 100])
                        m.write8(B + 4 * j, int(math.cos(a) * mag) & 0xFF)
                        m.write8(B + 4 * j + 1, int(math.sin(a) * mag) & 0xFF)
                    m.write8(BUF1 + 4, (0x80 + sx) & 0xFF)
                    m.write8(BUF1 + 5, (0x7F - sy) & 0xFF)
                    # put the (already normalised) sample into the ring the way the sampler does: raw bytes -> ring
                    m.write8(B + 4 * newest, (sx) & 0xFF)
                    m.write8(B + 4 * newest + 1, (sy) & 0xFF)
                    m.write8(B + 4 * newest + 2, rng.choice([0, 20, 127, 0x80]) & 0xFF)
                    m.write8(B + 4 * newest + 3, rng.choice([0, 20, 127, 0x80]) & 0xFF)
                    return 1
                run_pad_update(h, f"gesture state {state:#x} sample ({sx},{sy}) prev {prev}", setup, i)
                i += 1
    # sequences, compared after every frame
    nseq = max(4, int(26 * scale))
    frames = 90
    kinds = ["spin", "flick", "hold_spin", "walk", "creep"]
    for s in range(nseq):
        kind = kinds[s % len(kinds)]
        rng = random.Random(f"{seed}/seq/{s}")
        seq = stick_sequence(kind, rng, frames)
        h.reset()
        settle(h.m, rng)
        h.sync_to_c()
        mm, cm = Model(f"{seed}/seq/{s}"), Model(f"{seed}/seq/{s}")
        h.model = cm
        held = 0
        fx, fz = 0x1000, 0
        for f, (rx, ry, lx, ly) in enumerate(seq):
            if rng.random() < 0.1:
                held = rng.choice([0, 0x40, 0x10, 0x80, 0x20, 0x0A, 0x1000, 0xFFFF, rng.getrandbits(16)])
            if rng.random() < 0.15:
                a = rng.random() * 6.28
                fx, fz = int(math.sin(a) * 4096), int(math.cos(a) * 4096)
            if rng.random() < 0.05:
                write_matrix(h.m, CAMMAT, rand_yaw_matrix(rng))
                for k in range(9):
                    lib.ae_wr16(h.ram, CAMMAT + 2 * k, h.m.read16(CAMMAT + 2 * k))
            for side_write8, side_write16 in ((h.m.write8, h.m.write16), (lambda a, v: lib.ae_wr8(h.ram, a, v & 0xFF), lambda a, v: lib.ae_wr16(h.ram, a, v & 0xFFFF))):
                side_write8(BUF1 + 2, ((~held) >> 8) & 0xFF)
                side_write8(BUF1 + 3, (~held) & 0xFF)
                for k, v in enumerate((rx, ry, lx, ly)):
                    side_write8(BUF1 + 4 + k, v)
                side_write16(FACING, fx)
                side_write16(FACING + 4, fz)
            h.count("PadUpdate sequence frame")
            try:
                h.m.call(ADDR["PadUpdate"], [1], hooks=mips_pad_hooks(mm))
                lib.ae_pad_PadUpdate(h.ram, C.byref(h.pad_hooks), 1)
                ok, a, b = h.window_equal(*PAD_STATE_WINDOW)
                if not ok or mm.log != cm.log or lib.ae_ram_failed(h.ram):
                    diffs = [i for i in range(len(a)) if a[i] != b[i]]
                    h.fail("PadUpdate sequence", f"{kind} #{s} frame {f}",
                           f"window differs at {[hex(PAD_STATE_WINDOW[0] + d) for d in diffs[:6]]}" if diffs else "hook log / error flag differs",
                           lib.ae_ram_error(h.ram).decode())
                    break
                h.m.call(ADDR["PadSampleStickHistory"], [])
                lib.ae_pad_PadSampleStickHistory(h.ram)
                ok, a, b = h.window_equal(*PAD_STATE_WINDOW)
                if not ok:
                    h.fail("PadSampleStickHistory sequence", f"{kind} #{s} frame {f}")
                    break
            except mr.MipsError as e:
                h.fail("PadUpdate sequence", f"{kind} #{s} frame {f}", "MIPS error " + str(e))
                break
        else:
            if bytes(h.m.ram)[:STACK_LO] != bytes(h.main)[:STACK_LO] or bytes(h.m.scratch) != bytes(h.scr):
                h.fail("PadUpdate sequence", f"{kind} #{s}", "RAM / scratchpad differ after the last frame")


def py_normss(lut, x, z):
    """Python replica of the core (inputs |v| <= 128, never traps): (dir x, dir z, sum of squares)."""
    sm = x * x + z * z
    if sm == 0:
        return 0, 0, 0
    lz = 32 - sm.bit_length()
    v1 = lz & ~1
    t3 = v1 - 24
    t4 = (sm >> (24 - v1)) if t3 < 0 else (sm << t3)
    ir0 = lut[t4 - 64]
    sh = (31 - v1) >> 1
    return (ir0 * x) >> sh, (ir0 * z) >> sh, sm


def py_stored(lut, x, z):
    """The sample PadUpdate stores for a raw (x, z): direction * magnitude / 4096 (gadget stick)."""
    dx, dz, sm = py_normss(lut, x, z)
    v1 = sm >> 7
    mag = 0 if v1 < 0x15 else v1 - 0x15
    mag = min(0x7F, mag * 127 // 106)

    def q(v):
        v *= mag
        return ((v + 0xFFF) if v < 0 else v) >> 12
    return s8(q(dx) & 0xFF), s8(q(dz) & 0xFF), mag, dx, dz


def check_boundaries(h, seed):
    """Cases built to sit exactly on the comparison constants of the gesture code (found with a replica of the arithmetic)."""
    lut = struct.unpack("<192h", h.m.read_bytes(0x800A9C14, 384))
    cases = []

    def base(m, newest_raw, entries):
        settle(m, random.Random(seed))
        m.write32(POS, 7)
        m.write16(DONE + 14, 0)
        m.write8(B + 28, newest_raw[0] & 0xFF)
        m.write8(B + 29, newest_raw[1] & 0xFF)
        for age, (x, y) in entries.items():
            j = (7 - age) % 40
            m.write8(B + 4 * j, x & 0xFF)
            m.write8(B + 4 * j + 1, y & 0xFF)
        write_matrix(m, CAMMAT, [0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000])
        m.write16(FACING, 0x1000)
        m.write16(FACING + 4, 0)
        return 1

    cur = py_stored(lut, 127, 0)
    for dx, dy in ((30, 0), (0, 30), (18, 24), (24, 18), (30, 1), (1, 30), (26, 15), (15, 26), (29, 5), (31, 0), (5, 29), (29, 6)):
        for sgn in (1, -1):
            prev = (cur[0] - dx, cur[1] + sgn * dy)
            for pp in ((cur[0] - 3, cur[1]), (cur[0] - dx, cur[1] - sgn * dy), (prev[0] - 1, prev[1])):
                cases.append((f"flick boundary d=({dx},{sgn * dy})",
                              lambda m, prev=prev, pp=pp: base(m, (127, 0), {1: prev, 2: pp})))
    for rx, ry in ((40, 20), (60, -30), (-50, 40), (30, 70), (20, 0), (0, 25), (-35, -35), (52, 11)):
        c = py_stored(lut, rx, ry)
        for target in (0xB1D, 0xB1E, 0xB1F, -0xB1C, -0xB1D, -0xB1E):
            found = [(px, py) for px in range(-128, 128) for py in range(-128, 128) if c[0] * py - c[1] * px == target]
            for prev in found[:2]:
                cases.append((f"spin boundary raw=({rx},{ry}) target {target}",
                              lambda m, rx=rx, ry=ry, prev=prev: base(m, (rx, ry), {1: prev})))
    for target in (0xB4F, 0xB50, 0xB51, 0xB52):
        for sx in (1, -1):
            for axis in (0, 1):
                hits = []
                for x in range(-128, 128):
                    for z in range(-128, 128):
                        if x * x + z * z < 0x4000:
                            continue
                        dx, dz, _ = py_normss(lut, x, z)
                        if (dx if axis == 0 else dz) == sx * target:
                            hits.append((x, z))
                for x, z in hits[:2]:
                    cases.append((f"cam threshold {sx * target:#x} axis {axis}", lambda m, x=x, z=z: base(m, (x, z), {})))
    for i, (label, wr) in enumerate(cases):
        run_pad_update(h, f"{label} #{i}", wr, i)
        h.count("PadUpdate boundary cases")


def check_camera(h, scale, seed):
    lib = h.lib
    EYE, TGT, M3, M4 = 0x80120200, 0x80120210, 0x80120220, CAMMAT
    hooked = {0x80031830: lambda cpu, r: 0, 0x8002FF50: lambda cpu, r: 0, 0x80030450: lambda cpu, r: 0,
              0x800797F8: lambda cpu, r: 0, 0x800303C0: lambda cpu, r: 0, 0x8002FC20: lambda cpu, r: 0, 0x8002FB50: lambda cpu, r: 0,
              0x800A3740: lambda cpu, r: 0}
    cases = int(2500 * scale)
    for i in range(cases):
        rng = random.Random(f"{seed}/cam/{i}")
        k = rng.random()
        if k < 0.1:
            eye = [rand16(rng), rand16(rng), rand16(rng)]
            tgt = [eye[0], rand16(rng), eye[2]]
        elif k < 0.2:
            eye = [rng.randint(-100, 100), 0, rng.randint(-100, 100)]
            tgt = [eye[0] + rng.randint(-3, 3), rng.randint(-50, 50), eye[2] + rng.randint(-3, 3)]
        elif k < 0.3:
            eye = [rng.choice([-0x8000, 0x7FFF, 0, 0x4000]) for _ in range(3)]
            tgt = [rng.choice([-0x8000, 0x7FFF, 0, 0x4000]) for _ in range(3)]
        else:
            eye = [rng.randint(-0x3000, 0x3000) for _ in range(3)]
            tgt = [eye[j] + rng.randint(-0x2000, 0x2000) for j in range(3)]

        def setup(m, eye=eye, tgt=tgt):
            for j in range(3):
                m.write16(EYE + 2 * j, eye[j] & 0xFFFF)
                m.write16(TGT + 2 * j, tgt[j] & 0xFFFF)
            for j in range(9):
                m.write16(M4 + 2 * j, 0x5A5A)
            return None

        e3, t3 = (C.c_int16 * 3)(*[s16(v) for v in eye]), (C.c_int16 * 3)(*[s16(v) for v in tgt])

        def mips(ctx, mm):
            return h.m.call(ADDR["CameraLookAt"], [EYE, TGT, M3, M4, 0], hooks=hooked)

        def c(ctx):
            lib.ae_pad_CameraRotMatrix(h.ram, e3, t3, M4)

        # only the g_CameraRotMatrix part is ported: the view matrix (M3), the callees' scratch and 0x800F45CC are excluded
        reg.declare("ram_mask", "CameraLookAt: only the 18 matrix bytes at M4 are compared (not the RAM image)")
        h.reset()
        setup(h.m)
        h.sync_to_c()
        h.model = Model("cam")
        h.count("CameraLookAt (rotMatrix)")
        try:
            mips(None, None)
        except mr.MipsError as e:
            if "overflow" in str(e) and lib.ae_player_normalize_traps() >= 0:
                c(None)
                if not lib.ae_ram_failed(h.ram):
                    h.fail("CameraLookAt", f"#{i}", "the original traps but the port did not flag it")
                else:
                    h.traps += 1
                continue
            h.fail("CameraLookAt", f"#{i}", "MIPS error " + str(e))
            continue
        c(None)
        mm = bytes(h.m.read_bytes(M4, 32))
        cm = bytes(C.string_at(C.addressof(h.main) + (M4 & 0x1FFFFF), 32))
        if mm[:18] != cm[:18] or lib.ae_ram_failed(h.ram):
            h.fail("CameraLookAt", f"#{i} eye {eye} target {tgt}", f"matrix MIPS {mm[:18].hex()} C {cm[:18].hex()}",
                   lib.ae_ram_error(h.ram).decode())


def check_gadgets(h, scale, seed):
    lib = h.lib
    SW = ADDR["IsPlayerSwimming"]
    for state in list(range(0, 0x120)) + [0x1FF, 0xFFFF, 0x8046, 0x1046]:
        h.reset()
        h.m.write16(0x800EC23E, state)
        mret = h.m.call(SW, [])[2]
        h.count("IsPlayerSwimming")
        if lib.ae_player_is_swimming(state) != s32(mret):
            h.fail("IsPlayerSwimming", f"state {state:#x}", f"MIPS {s32(mret)} C {lib.ae_player_is_swimming(state)}")
    modes = list(range(0, 18)) + [0x100, 0x8009, 0xFFFF, 0x1000B]
    states = [0, 1, 0x24, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x80, 0x81, 0xC6, 0xFFFF]
    for mode in modes:
        for state in states:
            h.reset()
            h.m.write16(0x800F44A0, mode & 0xFFFF)
            h.m.write16(0x800EC23E, state)
            mret = h.m.call(ADDR["CanSwitchGadget"], [])[2]
            h.count("CanSwitchGadget")
            cret = lib.ae_player_can_switch_gadget(mode & 0xFFFF, state)
            if cret != s32(mret):
                h.fail("CanSwitchGadget", f"mode {mode:#x} state {state:#x}", f"MIPS {s32(mret)} C {cret}")
    # PlayerCheckGadgetSelect: the decision (which PlayerSelectGadget call, if any) on random inputs
    for i in range(int(2500 * scale)):
        rng = random.Random(f"{seed}/pcgs/{i}")
        mode = rng.choice([0, 0, 0, 0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12])
        state = rng.choice([0, 0x24, 0x46, 0x47, 0x49, 0x4A, 0x4C, 0x4B, 0x80, 0x81, 5])
        held = rng.choice([0, 0x10, 0x20, 0x40, 0x80, 0xF0, rng.getrandbits(16), 0xFFFFFFFF])
        changed = rng.choice([0, held, rng.getrandbits(16), 0xF0, 0x10, 0x20, 0x40, 0x80])
        slots = [rng.choice([-1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x7F, -128]) for _ in range(4)]
        cur = rng.choice([-1, 0, 1, 7, 9, 3])

        def setup(m, mode=mode, state=state, held=held, changed=changed, slots=slots, cur=cur):
            m.write16(0x800F44A0, mode)
            m.write16(0x800EC23E, state)
            m.write32(0x1F800058, held & M32)
            m.write32(0x1F800054, changed & M32)
            for k in range(4):
                m.write8(0x800F51A8 + k, slots[k] & 0xFF)
            m.write8(0x800EC2D2, cur & 0xFF)
            return None

        def mips(ctx, mm):
            return h.m.call(ADDR["PlayerCheckGadgetSelect"], [], hooks=mips_gadget_hooks(mm))

        def c(ctx):
            g = I32(0)
            arr = (C.c_int8 * 4)(*slots)
            if lib.ae_player_gadget_select_request(mode, state, held & M32, changed & M32, arr, C.byref(g)):
                h.model.select_gadget(g.value, cur)
            return 0

        # the C request function has no RAM: compare the hook log only (RAM is unchanged by the original except via hooks)
        h.reset()
        setup(h.m)
        h.sync_to_c()
        mm, cm = Model("pcgs"), Model("pcgs")
        h.model = cm
        h.count("PlayerCheckGadgetSelect")
        h.m.call(ADDR["PlayerCheckGadgetSelect"], [], hooks=mips_gadget_hooks(mm))
        c(None)
        if mm.log != cm.log:
            h.fail("PlayerCheckGadgetSelect", f"#{i} mode {mode} state {state:#x} held {held:#x} changed {changed:#x} slots {slots} cur {cur}",
                   f"MIPS {mm.log} C {cm.log}")
    # GadgetSlots*: full RAM diff
    def gs_setup(m, rng, code, area, flag, flag2, cur):
        m.write8(0x800F447C, code)
        m.write16(0x800F4476, area)
        m.write16(0x800F4472, flag)
        m.write16(0x800F447A, flag2)
        m.write8(0x800EC2D2, cur & 0xFF)
        for k in range(4):
            m.write8(0x800F51A8 + k, rng.choice([0xFF, 0, 1, 2, 3, 7, rng.getrandbits(8)]))
            m.write8(0x800F51AC + k, rng.choice([0xFF, 0, 5, rng.getrandbits(8)]))
        m.write32(0x800F51B0, rng.getrandbits(32))
        m.write32(0x800F51C4, rng.getrandbits(32))
        for a in (0x800F51C1, 0x800F51C2, 0x800F51C3):
            m.write8(a, rng.getrandbits(8))
        m.write16(0x800F51E8, rng.getrandbits(16))
        m.write16(0x800F51EA, rng.getrandbits(16))

    codes = list(range(0, 12)) + [0x80, 0xFF, 0x100 - 7]
    areas = [0x13, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x01, 0xFFFF, 0x7C, 0x5C - 1]
    n = 0
    for code in codes:
        for area in areas:
            for flag in (0, 3, 0xE, 0xF, 0x10, 0xFFFF, 4):
                for flag2 in (0, 1):
                    n += 1
                    if scale < 1 and random.Random(f"{seed}/gsk/{n}").random() > scale:
                        continue
                    cur = random.Random(f"{seed}/gsc/{n}").choice([-1, 0, 1, 2, 5, 9, 3])
                    h_rng = random.Random(f"{seed}/gsr/{n}")
                    h.reset()
                    gs_setup(h.m, h_rng, code, area, flag, flag2, cur)
                    h.sync_to_c()
                    mm, cm = Model("gsx"), Model("gsx")
                    h.model = cm
                    h.count("GadgetSlotsInitForExit")
                    h.m.call(ADDR["GadgetSlotsInitForExit"], [], hooks=mips_gadget_hooks(mm))
                    lib.ae_GadgetSlotsInitForExit(h.ram, C.byref(h.gadget_hooks))
                    h.compare("GadgetSlotsInitForExit", f"code {code} area {area:#x} f4472 {flag:#x} f447a {flag2} cur {cur}", None, None, mm.log, cm.log)
    for i in range(int(400 * scale)):
        rng = random.Random(f"{seed}/gsa/{i}")
        area = rng.choice(areas + [rng.getrandbits(16)])
        arg = rng.choice([-1, 0, 1, 2, 9, 5])
        for name, caddr, call_c in (("GadgetSlotsInitForArea", ADDR["GadgetSlotsInitForArea"], lambda a=arg: lib.ae_GadgetSlotsInitForArea(h.ram, a)),
                                    ("GadgetSlotsSave", ADDR["GadgetSlotsSave"], lambda: lib.ae_GadgetSlotsSave(h.ram)),
                                    ("GadgetSlotsRestore", ADDR["GadgetSlotsRestore"], lambda: lib.ae_GadgetSlotsRestore(h.ram))):
            h.reset()
            gs_setup(h.m, rng, 0, area, 0, 0, rng.choice([-1, 0, 5, 9]))
            h.sync_to_c()
            h.model = Model("x")
            h.count(name)
            regs = h.m.call(caddr, [arg & M32])
            cret = call_c()
            h.compare(name, f"#{i} area {area:#x}", regs[2] if name == "GadgetSlotsInitForArea" else None,
                      cret if name == "GadgetSlotsInitForArea" else None, [], [])
    # NULL hooks fail closed
    for code in (0, 1, 2, 4):
        h.reset()
        gs_setup(h.m, random.Random(code), code, 0x5C, 0xE, 0, 0)
        h.sync_to_c()
        lib.ae_GadgetSlotsInitForExit(h.ram, C.byref(h.null_gadget))
        h.count("GadgetSlotsInitForExit (NULL hooks)")
        if not lib.ae_ram_failed(h.ram) or b"not provided" not in lib.ae_ram_error(h.ram):
            h.fail("GadgetSlotsInitForExit", f"NULL hooks code {code}", "the missing hook was not flagged")
    # the documented new-game outcome, straight from the original instructions
    h.reset()
    gs_setup(h.m, random.Random(1), 0, 0xFFFF, 0, 0, 9)
    mm = Model("ng")
    h.m.call(ADDR["GadgetSlotsInitForExit"], [], hooks=mips_gadget_hooks(mm))
    slots = [s8(h.m.read8(0x800F51A8 + k)) for k in range(4)]
    mask = h.m.read32(0x800F51C4)
    print(f"original new game (g_StageExitCode 0): g_GadgetSlots = {slots} (Triangle, Square, Circle, Cross), "
          f"g_GadgetUnlockMask = {mask}, hook log {mm.log}")
    if slots != [0, -1, -1, 1] or mask != 3 or mm.log != [("select", 0, M32), ("hud",)]:
        h.fail("new game defaults", "documented values", f"slots {slots} mask {mask} log {mm.log}")


# result name (ledger function) -> the keys of Harness.counts that count it (the minimum over the keys is reported)
RESULT_KEYS = {"PadUpdate": ["PadUpdate"], "PadNormalizeSticks": ["PadNormalizeSticks"], "PadSampleStickHistory": ["PadSampleStickHistory"],
               "QuadrantFromSigns": ["QuadrantFromSigns"], "ApplyMatrixSV": ["ApplyMatrixSV"], "VectorNormal": ["VectorNormal"],
               "VectorNormalSS": ["VectorNormalSS"], "CameraLookAt": ["CameraLookAt (rotMatrix)"],
               "CanSwitchGadget": ["CanSwitchGadget"], "IsPlayerSwimming": ["IsPlayerSwimming"],
               "PlayerCheckGadgetSelect": ["PlayerCheckGadgetSelect"], "GadgetSlotsInitForExit": ["GadgetSlotsInitForExit"],
               "GadgetSlotsInitForArea": ["GadgetSlotsInitForArea"], "GadgetSlotsSave": ["GadgetSlotsSave"],
               "GadgetSlotsRestore": ["GadgetSlotsRestore"],
               "VectorNormal (player-side)": ["ae_player_normalize (builtin table)", "ae_player_normalize (exe table)"]}
# coverage entry (COVER name) of each ledger function
COVER_OF = {"VectorNormal": "VectorNormal + SS + core", "VectorNormalSS": "VectorNormal + SS + core",
            "CameraLookAt": "CameraLookAt (rotMatrix part)", "GadgetSlotsSave": "GadgetSlotsSave+Restore",
            "GadgetSlotsRestore": "GadgetSlotsSave+Restore"}


def finish(h, show_coverage, default_run):
    """Coverage summary (the registry's unreachable rows are subtracted) and, for a run at the default scale and seed,
    extract/checks/results/check_player_input.json."""
    summary = None
    if h.m.coverage is not None:
        summary = reg.coverage_summary([(n, a, s, True) for n, a, s in COVER], h.m.coverage)
        if show_coverage:
            reg.print_coverage(summary)
    if default_run:
        counts = {k: min(h.counts.get(key, 0) for key in keys) for k, keys in RESULT_KEYS.items()}
        view = {k: summary[COVER_OF.get(k, k)] for k in RESULT_KEYS if summary and COVER_OF.get(k, k) in summary}
        reg.write_results(counts, view or None, not h.failures, function_names=list(RESULT_KEYS))
    return summary


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", type=float, default=40, help="scale of the case counts in percent")
    ap.add_argument("--seed", type=int, default=0x944230)
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--pad-src", help="test another copy of ae_pad.c (mutation testing of the checker itself)")
    a = ap.parse_args()
    if not (ROOT / "extract/disc/SCUS_944.23").exists():
        print("SKIP: extract/disc/SCUS_944.23 not present")
        return 0
    scale = a.cases / 100.0
    t0 = time.time()
    h = Harness(build_dll(a.pad_src), True)
    steps = []
    for name, fn in (("vectors", check_vectors), ("pad helpers", check_pad_helpers), ("camera", check_camera),
                     ("gadgets", check_gadgets), ("PadUpdate", check_pad_update),
                     ("boundaries", lambda h_, sc, sd: check_boundaries(h_, sd))):
        t = time.time()
        fn(h, scale, a.seed)
        steps.append((name, time.time() - t))
        print(f"{name} done in {time.time() - t:.1f}s")
    print("cases per function:")
    for k, v in sorted(h.counts.items()):
        print(f"  {k:40s} {v}")
    print(f"original add-overflow traps matched by the port: {h.traps}")
    print(f"total cases {sum(h.counts.values())}; MIPS instructions executed {h.m.steps}; {time.time() - t0:.1f}s")
    finish(h, a.coverage, default_run=(a.cases == ap.get_default("cases") and a.seed == ap.get_default("seed") and not a.pad_src))
    if h.failures:
        print(f"FAILED: {h.failures} mismatching cases")
        return 1
    print("OK: the C port matches the original instructions on every case")
    return 0


if __name__ == "__main__":
    sys.exit(main())
