"""Differential check of the collision-body port (src/bodies/ae_bodies.c) against the original MIPS code.

For every ported function many scenarios are built in a byte-exact RAM image (SCUS_944.23 loaded at its real
addresses); the original instructions (tools/mips/mips_ram.py, GTE through the pinned PCSX-ReARMed oracle of
tools/codex/gte_oracle.py) and the C port (a DLL over AeRam) run on identical RAM images with identical hooks.  The
final RAM (main RAM without the top-of-RAM stack window, plus the scratchpad), the return value and the sequence of
hook calls (name + arguments) must be identical.  Needs extract/disc/SCUS_944.23 (skips if it is missing).

Usage: uv run python tools/mips/check_bodies.py [--cases PERCENT] [--seed S] [--coverage]
"""
import argparse
import ctypes as C
import os
import random
import shutil
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

SLOTS = 0x800BBDA0
POOL = 0x800BD1A0
POOL_FREE = 0x800BEDA0
LOOPS = 0x800BEEA0
HEAD, TAIL = 0x800B20D8, 0x800B20DC
PLAYER, RCCAR = 0x800EC1E0, 0x800F5A20
OBJ_BASE = 0x80120000
COLLIDERS = 0x80160000
SCR_V, SCR_N, SCR_A, SCR_B = 0x1F8000F0, 0x1F800100, 0x1F800108, 0x1F800110
M32 = 0xFFFFFFFF


def s32(v): return signed(v, 32)
def s16(v): return signed(v, 16)


# name -> (original address, size, C function, takes hooks, argument kinds: s = int32_t, u = uint32_t)
FUNCS = {
    "CollisionBodiesReset": (0x80071D3C, 264, "ae_CollisionBodiesReset", False, ""),
    "CollisionBodyRegister": (0x80071E44, 408, "ae_CollisionBodyRegister", True, "suu"),
    "CollisionBodyAllocSlot": (0x80071FDC, 60, "ae_CollisionBodyAllocSlot", False, ""),
    "CollisionBodyUnregister": (0x80072018, 344, "ae_CollisionBodyUnregister", True, "u"),
    "GetObjectByIndex": (0x80072170, 68, "ae_GetObjectByIndex", False, "u"),
    "CollisionBodyIndexOf": (0x800721B4, 108, "ae_CollisionBodyIndexOf", False, "s"),
    "CollisionBodyAlloc": (0x80072220, 108, "ae_CollisionBodyAlloc", False, ""),
    "CollisionBodyClear": (0x8007228C, 72, "ae_CollisionBodyClear", False, "u"),
    "CollisionBodyFree": (0x800722D4, 96, "ae_CollisionBodyFree", False, "u"),
    "LoopSlotAdd": (0x80072334, 76, "ae_LoopSlotAdd", False, "su"),
    "LoopSlotRef": (0x80072380, 104, "ae_LoopSlotRef", False, "su"),
    "LoopSlotSweep": (0x800723E8, 108, "ae_LoopSlotSweep", False, ""),
    "LoopSlotClearAll": (0x80072454, 40, "ae_LoopSlotClearAll", False, ""),
    "CollisionUpdate": (0x8007247C, 1200, "ae_CollisionUpdate", True, ""),
    "BodyCollideWithBodies": (0x800777AC, 1148, "ae_BodyCollideWithBodies", True, "u"),
    "CollisionBodyGetCenter": (0x80077C28, 240, "ae_CollisionBodyGetCenter", False, "suu"),
    "BodyFindOverlapType1": (0x80077D18, 464, "ae_BodyFindOverlapType1", True, "su"),
    "BodyFindOverlapType3to5": (0x80077EE8, 416, "ae_BodyFindOverlapType3to5", True, "su"),
    "BodyCollideBodiesDetailed": (0x80078088, 1920, "ae_BodyCollideBodiesDetailed", True, "su"),
    "BodyCheckPickupContact": (0x800788B8, 352, "ae_BodyCheckPickupContact", True, "su"),
    "BodyContactType": (0x800392E4, 12, "ae_BodyContactType", False, "u"),
    "BodyTypeOfActor": (0x80078864, 84, "ae_BodyTypeOfActor", False, "u"),
    "BodyContactSfx": (0x80078808, 92, "ae_BodyContactSfx", True, "ss"),
    "VectorNormalS": (0x8003019C, 20, "ae_VectorNormalS", False, "uu"),
}
# extra original code that is exercised through the functions above (for the coverage report)
EXTRA_COVERAGE = {"VectorNormalS core": (0x80030210, 0xC0), "VecLength3": (0x80046618, 0x4C)}


def build_dll(bodies_src=None, trapv=False):
    overrides = {"src/bodies/ae_bodies.c": Path(bodies_src)} if bodies_src else None
    import check_game as cg  # lazy: check_game imports this module through monkey_harness
    return cg.build_dll(overrides=overrides, tag="bodies", trapv=trapv)


class MoveHooks(C.Structure):
    _fields_ = [(name,C.c_void_p) for name in
                ('user','world','solid','misc','player_act','spawn','camupd','area_set','sound_hw','coll_probe')] + [
                ('stack_pointer',C.c_uint32),('LocalDirectionPadding',C.c_void_p),('SfxVoiceSetPitch',C.c_void_p),('XaPlay',C.c_void_p)]

class Hooks(C.Structure):
    _fields_ = [
        ("user", C.c_void_p),
        ("printf_error", C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_int32)),
        ("sound_loop_start", C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_int32, C.c_int32)),
        ("sound_loop_stop", C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32, C.c_int32)),
        ("reserved_body_move", C.c_void_p),
        ("build_probe_points", C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32, C.c_uint32)),
        ("body_stub_return_zero", C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p)),
        ("collision_ray_test", C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32, C.POINTER(C.c_int16))),
        ("gimmick_type_to_sfx", C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_int32)),
        ("play_sfx_at", C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32, C.POINTER(C.c_int16))),
        ("pickup_collect", C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32)),
        ("camera_nudge", C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32, C.c_int32, C.c_int32)),
        ("move", MoveHooks),
    ]


class CSide:
    """The C port: the DLL, an AeRam, views of its memory, and hook callbacks that forward to the current Model."""

    def __init__(self, dll):
        self.lib = lib = C.CDLL(str(dll))
        P, u32, i32 = C.c_void_p, C.c_uint32, C.c_int32
        lib.ae_ram_create.restype = P
        lib.ae_ptr.restype = P
        lib.ae_ptr.argtypes = [P, u32, u32]
        lib.ae_rd32.restype = u32
        lib.ae_rd32.argtypes = [P, u32]
        lib.ae_rds16.restype = i32
        lib.ae_rds16.argtypes = [P, u32]
        lib.ae_wr16.argtypes = [P, u32, u32]
        lib.ae_wr32.argtypes = [P, u32, u32]
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        lib.ae_ram_clear_error.argtypes = [P]
        lib.ae_player_vec_length3.argtypes = [i32, i32, i32]
        lib.ae_player_vec_length3.restype = i32
        for name, (_, _, cname, takes_hooks, kinds) in FUNCS.items():
            f = getattr(lib, cname)
            f.argtypes = [P] + ([P] if takes_hooks else []) + [i32 if k == "s" else u32 for k in kinds]
            f.restype = u32 if name in ("CollisionBodyAllocSlot", "GetObjectByIndex", "CollisionBodyAlloc",
                                        "BodyContactType") else i32
        self.ram = lib.ae_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scr = (C.c_uint8 * 0x400).from_address(lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.model = None
        self.hooks = Hooks()
        self._keep = []
        m = lambda: self.model  # noqa: E731
        fns = {
            "printf_error": lambda u, r, fmt, t: m().printf_error(fmt, t),
            "sound_loop_start": lambda u, r, a, i: m().sound_loop_start(a, i),
            "sound_loop_stop": lambda u, r, i, hd: m().sound_loop_stop(i, hd),
            "build_probe_points": lambda u, r, t, b: m().build_probe_points(t, b),
            "body_stub_return_zero": lambda u, r: m().stub_call(),
            "collision_ray_test": lambda u, r, arg, params, out: self._ray(arg, params, out),
            "gimmick_type_to_sfx": lambda u, r, t: m().gimmick(t),
            "play_sfx_at": lambda u, r, sfx, pos: m().play_sfx(sfx, [pos[0], pos[1], pos[2]]),
            "pickup_collect": lambda u, r, t: m().pickup(t),
            "camera_nudge": lambda u, r, dx, dy, dz: m().nudge(dx, dy, dz),
        }
        for name, fn in fns.items():
            cb = dict(Hooks._fields_)[name](fn)
            self._keep.append(cb)
            setattr(self.hooks, name, cb)
        self.null_hooks = Hooks()   # every function pointer NULL: fail-closed behaviour

    def _ray(self, arg, params, out):
        res, o0, o1 = self.model.ray(arg, params)
        out[0], out[1] = o0, o1
        return res

    def rd32(self, a): return self.lib.ae_rd32(self.ram, a)
    def wr32(self, a, v): self.lib.ae_wr32(self.ram, a, v & M32)
    def rds16(self, a): return self.lib.ae_rds16(self.ram, a)
    def wr16(self, a, v): self.lib.ae_wr16(self.ram, a, v & 0xFFFF)

    def call(self, name, args, hooks=None):
        _, _, cname, takes_hooks, kinds = FUNCS[name]
        conv = [s32(a) if k == "s" else a & M32 for a, k in zip(args, kinds)]
        f = getattr(self.lib, cname)
        if takes_hooks:
            return f(self.ram, C.byref(hooks if hooks is not None else self.hooks), *conv)
        return f(self.ram, *conv)


class MipsMem:
    def __init__(self, cpu): self.c = cpu
    def rd32(self, a): return self.c.read32(a)
    def wr32(self, a, v): self.c.write32(a, v & M32)
    def rds16(self, a): return self.c.reads16(a)
    def wr16(self, a, v): self.c.write16(a, v & 0xFFFF)


class Model:
    """Deterministic behaviour of every hook (identical on both sides).  Every call is logged."""

    def __init__(self, seed, mem, stub=0):
        self.seed, self.mem, self.stub, self.log, self.n = seed, mem, stub, [], {}

    def rng(self, name):
        k = self.n.get(name, 0)
        self.n[name] = k + 1
        return random.Random(f"{self.seed}/{name}/{k}")

    def printf_error(self, fmt, t): self.log.append(("printf", fmt & M32, s32(t)))

    def sound_loop_start(self, a, i):
        self.log.append(("loop_start", s32(a), s32(i)))
        return s32(self.rng("sls").getrandbits(32))

    def sound_loop_stop(self, i, h): self.log.append(("loop_stop", s32(i), s32(h)))

    def build_probe_points(self, t, body):
        self.log.append(("probe", s32(t), body & M32))
        g = self.rng("bp")
        if g.random() < 0.3:
            self.mem.wr16(body + 0x2A, g.randint(0, 3))
        self.mem.wr16(body + 0x34, g.getrandbits(16))

    def stub_call(self):
        self.log.append(("stub",))
        return self.stub

    def ray(self, arg, params):
        words = [self.mem.rd32(params + 4 * i) for i in range(5)]
        # the real CollisionRayTest reads g_CollWallProbe (0x800F45E6) and DAT_800B20CC (the type being tested)
        self.log.append(("ray", arg & M32, params & M32, tuple(words), self.mem.rds16(0x800F45E6) & 0xFFFF,
                         self.mem.rds16(0x800B20CC) & 0xFFFF))
        g = self.rng("ray")
        res = g.choice([-1, -1, 0, 1, 5, 77, 0x18000, 0x7FFF, 300])
        o0 = g.choice([0, 0, 1, -5, 7])
        return res, o0, s16(g.getrandbits(16))

    def gimmick(self, t):
        self.log.append(("gimmick", s32(t)))
        return self.rng("gim").choice([-1, 0x80, 0x81, 0x82, 0x29, 5, 0x40, 0x7F, 0x83, 0])

    def play_sfx(self, sfx, pos): self.log.append(("sfx", s32(sfx), tuple(s16(p & 0xFFFF) for p in pos)))
    def pickup(self, t): self.log.append(("pickup", s32(t)))
    def nudge(self, dx, dy, dz): self.log.append(("nudge", s32(dx), s32(dy), s32(dz)))


def mips_hooks(model):
    """Entry hooks for the original functions outside the port (their calls are intercepted)."""
    def ray(cpu, r):
        res, o0, o1 = model.ray(r[4], r[5])
        cpu.write16(r[6], o0 & 0xFFFF)
        cpu.write16(r[6] + 2, o1 & 0xFFFF)
        return res

    def sfx(cpu, r):
        model.play_sfx(s32(r[4]), [cpu.read16(r[5]), cpu.read16(r[5] + 2), cpu.read16(r[5] + 4)])
        return 0

    def void(fn):
        def inner(cpu, r):
            fn(r)
            return 0
        return inner

    return {
        0x800A3720: void(lambda r: model.printf_error(r[4], r[5])),
        0x800878E0: lambda cpu, r: model.sound_loop_start(r[4], r[5]),
        0x8008806C: void(lambda r: model.sound_loop_stop(r[4], r[5])),
        0x8007304C: void(lambda r: model.build_probe_points(r[4], r[5])),
        0x80098320: lambda cpu, r: model.stub_call(),
        0x80078C9C: ray,
        0x8005BB98: lambda cpu, r: model.gimmick(r[4]),
        0x80087A20: sfx,
        0x80078A18: void(lambda r: model.pickup(r[4])),
        0x80047690: void(lambda r: model.nudge(r[4], r[5], r[6])),
    }


class Harness:
    def __init__(self, dll, with_coverage):
        self.m = mr.MipsRam.from_extract()
        self.m.attach_gte_oracle()
        if with_coverage:
            self.m.coverage = set()
        self.base = self.m.snapshot()
        self.c = CSide(dll)
        self.lib = self.c.lib
        self.counts = {}
        self.stack_lo = mr.STACK_WINDOW[0] - 0x80000000
        reg.declare("stack_window", f"{mr.STACK_WINDOW[0]:08X}..{mr.STACK_WINDOW[1]:08X}")
        self.failures = 0
        self.hook_calls = 0
        self.traps_agreed = 0
        self.fail_closed_checked = 0

    def sync_to_c(self):
        C.memmove(self.c.main, bytes(self.m.ram), 0x200000)
        C.memmove(self.c.scr, bytes(self.m.scratch), 0x400)
        self.lib.ae_ram_clear_error(self.c.ram)

    def fail(self, name, label, *lines):
        self.failures += 1
        print(f"FAIL {name} [{label}]")
        for line in lines:
            print("   ", line)

    def compare(self, name, label, mret, cret, mlog, clog):
        problems = []
        if self.lib.ae_ram_failed(self.c.ram):
            problems.append("C error flag: " + self.lib.ae_ram_error(self.c.ram).decode())
        if (mret & M32) != (cret & M32):
            problems.append(f"return value MIPS {mret & M32:08X} C {cret & M32:08X}")
        if mlog != clog:
            problems.append(f"hook log differs\n      MIPS {mlog}\n      C    {clog}")
        lo = self.stack_lo
        a, b = bytes(self.m.ram)[:lo], bytes(self.c.main)[:lo]
        if a != b:
            diffs = [i for i in range(lo) if a[i] != b[i]]
            problems.append(f"RAM differs ({len(diffs)} bytes) at " +
                            ", ".join(f"{0x80000000 + i:08X}(mips {a[i]:02X} c {b[i]:02X})" for i in diffs[:8]))
        if bytes(self.m.scratch) != bytes(self.c.scr):
            diffs = [i for i in range(0x400) if self.m.scratch[i] != self.c.scr[i]]
            problems.append("scratchpad differs at " + ", ".join(
                f"{0x1F800000 + i:08X}(mips {self.m.scratch[i]:02X} c {self.c.scr[i]:02X})" for i in diffs[:8]))
        if problems:
            self.fail(name, label, *problems)
        return not problems

    def run(self, name, label, seed, setup, argsf, *, stub=0, null_hooks=False, expect_trap=False, allow_trap=False):
        """setup(m) builds the scenario in the MIPS RAM image and returns a context; argsf(context) gives the
        raw argument words.  The scenario is copied to the C RAM and both implementations run on it."""
        addr = FUNCS[name][0]
        if null_hooks:
            stub = 0        # the port's default BodyStub returns 0
        self.m.restore(self.base)
        self.m.gte_reset()
        ctx = setup(self.m)
        args = argsf(ctx)
        self.sync_to_c()
        mm, cm = Model(seed, MipsMem(self.m), stub), Model(seed, self.c, stub)
        self.c.model = cm
        self.m.hook_log.clear()
        self.counts[name] = self.counts.get(name, 0) + 1
        trap = None
        try:
            regs = self.m.call(addr, args, hooks=mips_hooks(mm))
            mret = regs[2]
        except mr.MipsError as e:
            trap, mret = str(e), None
        self.hook_calls += len(mm.log)
        cret = self.c.call(name, args, self.c.null_hooks if null_hooks else None)
        if null_hooks:
            # fail closed: if the original calls a hook (other than the constant BodyStub) the port must flag the
            # missing hook; if it does not, the port must behave exactly like the full run
            needs = any(entry[0] != "stub" for entry in mm.log)
            flagged = bool(self.lib.ae_ram_failed(self.c.ram))
            self.fail_closed_checked += 1
            if needs:
                if not flagged or b"not provided" not in self.lib.ae_ram_error(self.c.ram):
                    self.fail(name, label, f"NULL hook was needed ({mm.log[:2]}) but the port did not flag it")
                return
            if trap is None:
                self.compare(name, label, mret, cret, [e for e in mm.log], [e for e in mm.log])
            return
        if expect_trap:
            if trap is None or not self.lib.ae_ram_failed(self.c.ram):
                self.fail(name, label, f"expected the original to trap and the port to flag it (trap={trap}, "
                          f"port flagged={bool(self.lib.ae_ram_failed(self.c.ram))})")
            return
        if trap:
            # the original faulted (unmapped / unaligned address): the port must flag the same condition
            if allow_trap and self.lib.ae_ram_failed(self.c.ram):
                self.traps_agreed += 1
                return
            self.fail(name, label, "MIPS harness error: " + trap,
                      f"port flagged={bool(self.lib.ae_ram_failed(self.c.ram))}")
            return
        self.compare(name, label, mret, cret, mm.log, cm.log)


# ------------------------------------------------------------------------------------------ scenario building
class W:
    def __init__(self, m): self.m = m
    def w16(self, a, v): self.m.write16(a, v & 0xFFFF)
    def w32(self, a, v): self.m.write32(a, v & M32)
    def r32(self, a): return self.m.read32(a)


def clear_world(m):
    w = W(m)
    for i in range(256):
        s = SLOTS + 20 * i
        w.w16(s, 0xFFFF)
        w.w16(s + 2, 0xFFFF)
        for off in (4, 8, 12, 16):
            w.w32(s + off, 0)
    for i in range(128):
        w.w16(POOL_FREE + 2 * i, 0xFFFF)
    w.w32(HEAD, 0)
    w.w32(TAIL, 0)


TYPE_WEIGHTS = [(0, 1), (1, 4), (2, 3), (3, 8), (4, 5), (5, 5), (6, 6), (7, 6), (8, 7), (9, 4), (10, 1), (11, 1),
                (12, 5), (13, 5), (14, 3), (15, 3), (16, 4), (17, 4), (18, 1), (0x20, 1)]


def pick_type(rng, kinds=None):
    pop = [t for t, _ in TYPE_WEIGHTS if kinds is None or t in kinds]
    wts = [wgt for t, wgt in TYPE_WEIGHTS if kinds is None or t in kinds]
    return rng.choices(pop, wts)[0]


def rand_contact(rng):
    if rng.random() < 0.65:
        return 0
    return (rng.choice([0, 1, 2, 3, 4, 5, 6, 8, 10, 10, 0xB, 0xE, 0xF, 5, 7, 9]) << 28) | rng.getrandbits(28)


def write_body(w, rng, body, cx, cy, cz, spread, *, solid=None, radius=None, height=None):
    far = rng.random() < 0.05
    def pos(c):
        return rng.choice([-1, 1]) * rng.randint(20000, 3_000_000) if far else c + rng.randint(-spread, spread)
    w.w32(body + 0, pos(cx))
    w.w32(body + 4, rng.randint(-100000, 100000) if far else cy + rng.randint(-300, 300))
    w.w32(body + 8, pos(cz))
    for off in (0xC, 0x10, 0x14):
        w.w32(body + off, rng.randint(-200, 200))
    w.w32(body + 0x30, rng.getrandbits(32))
    for off in (0x18, 0x1A, 0x1C, 0x1E):
        w.w16(body + off, rng.getrandbits(16))
    w.w16(body + 0x20, radius if radius is not None else rng.choice(
        [rng.randint(0, 500), rng.randint(40, 300), rng.randint(0, 120), rng.randint(-50, 20)]))
    w.w16(body + 0x22, height if height is not None else rng.choice(
        [rng.randint(0, 700), rng.randint(100, 500), rng.randint(-100, 5), rng.randint(0x7000, 0x8100)]))
    w.w16(body + 0x24, rng.choice([rng.getrandbits(16), rng.getrandbits(16) | 0xC00, rng.getrandbits(16) & ~0xC00, 1, 0x100]))
    w.w16(body + 0x26, rng.choice([rng.getrandbits(16), 0x8000, 0x4000, 0xC000, 0x8103, 0]))
    w.w16(body + 0x28, rng.getrandbits(16))
    w.w16(body + 0x2A, solid if solid is not None else rng.choice([0, 0, 1, 1, 1, 2, 0xFFFF]))
    w.w32(body + 0x2C, rand_contact(rng))
    w.w16(body + 0x34, rng.getrandbits(16))



def half_s(h):
    """(h + (h < 0)) >> 1: the original's signed halving of an s16 height"""
    return (h + (1 if h < 0 else 0)) >> 1


def tighten(m, rng, info, a=None):
    """Place two bodies of the world exactly on (+-2 of) the thresholds the overlap tests compare against, so that
    off-by-one and sign mistakes show up (random positions almost never land on a boundary)."""
    nodes = [n for n in info if n[3]]
    if len(nodes) < 2:
        return
    if a is None or not a[3]:
        a = rng.choice(nodes)
    b = rng.choice([n for n in nodes if n is not a])
    A, B = a[3], b[3]
    w = W(m)
    ra, rb = m.reads16(A + 0x20), m.reads16(B + 0x20)
    ha, hb = m.reads16(A + 0x22), m.reads16(B + 0x22)
    ax, ay, az = m.reads32(A), m.reads32(A + 4), m.reads32(A + 8)
    d1, d2 = rng.randint(-2, 2), rng.randint(-2, 2)
    sgn = rng.choice([-1, 1])
    mode = rng.choice(["H", "HV", "HV", "ring", "ring_y"])
    if mode in ("H", "HV"):
        dist = ra + rb + d1
        if rng.random() < 0.5:
            bx, bz = ax + sgn * dist, az
        else:
            bx, bz = ax, az + sgn * dist
        w.w32(B + 0, bx)
        w.w32(B + 8, bz)
        if mode == "HV":
            t = abs(half_s(ha)) + abs(half_s(hb)) + d2
            w.w32(B + 4, ay + rng.choice([-1, 1]) * t + half_s(hb) - half_s(ha))
    elif mode == "ring":                       # BodyFindOverlapType1: | length - radius | < height of the ring body
        dist = ra + rng.choice([-1, 1]) * ha + d1
        w.w32(B + 0, ax + sgn * dist)
        w.w32(B + 8, az)
        w.w32(B + 4, ay + rng.choice([-1, 1]) * rng.randint(0, 40))
    else:                                      # ring y window: B.y - B.height - 64 < A.y < B.y + 64
        w.w32(B + 4, ay + hb + 64 - d2 if rng.random() < 0.5 else ay - 64 - d2)
        w.w32(B + 0, ax + sgn * max(0, ra + ha + rng.randint(-3, 3)) if rng.random() < 0.5 else ax)
        w.w32(B + 8, az)


def build_world(m, rng, n=None, kinds=None, spread=None, full=False, force_types=None):
    """A consistent body list: slot table, linked list, bodies (pool bodies or embedded in owner objects) and the
    globals the code reads.  Returns [(slot address, slot index, type, body, owner)] in list order."""
    clear_world(m)
    w = W(m)
    n = n if n is not None else rng.choice([0, 1, 2, 3, 4, 5, 6, 8, 10, 12])
    if full:
        n = 256
    free_pool = list(range(128))
    rng.shuffle(free_pool)
    slot_ids = rng.sample(range(256), n)
    width = rng.choice([0, 1, 2, 5, 10, 20, 40, rng.randint(1, 60)])
    depth = rng.choice([0, 1, 2, 5, 10, 20, 40, rng.randint(1, 60)])
    w.w16(0x800EBFA0, width)
    w.w16(0x800EBFA2, depth)
    cx = rng.randint(-(max(width, 1) - 1) * 256, (max(width, 1) - 1) * 256) if rng.random() < 0.8 else rng.randint(-5000, 5000)
    cz = rng.randint(-(max(depth, 1) - 1) * 256, (max(depth, 1) - 1) * 256) if rng.random() < 0.8 else rng.randint(-5000, 5000)
    cy = rng.randint(-500, 500)
    spread = spread if spread is not None else rng.choice([150, 300, 600, 1200])
    used_player = used_rc = False
    obj_i = 0
    info = []
    prev = 0
    for k, si in enumerate(slot_ids):
        s = SLOTS + 20 * si
        t = force_types[k % len(force_types)] if force_types else pick_type(rng, kinds)
        owner = body = 0
        if t == 1 and not used_player and rng.random() < 0.9:
            owner, body, used_player = PLAYER, PLAYER + 0x24, True
        elif t == 2 and not used_rc and rng.random() < 0.9:
            owner, body, used_rc = RCCAR, RCCAR + 0x24, True
        elif (rng.random() < 0.55 and t in (1, 2, 3, 4, 5, 9, 10, 11)) or rng.random() < 0.2:
            owner = OBJ_BASE + 0x100 * obj_i
            obj_i += 1
            body = owner + 0x24
        elif free_pool:
            pi = free_pool.pop()
            body = POOL + 0x38 * pi
            w.w16(POOL_FREE + 2 * pi, rng.choice([1, 1, 1, 0xFFFF, 0]))
            if rng.random() < 0.5:
                owner = OBJ_BASE + 0x100 * obj_i
                obj_i += 1
            elif rng.random() < 0.1:
                owner = rng.choice([PLAYER, RCCAR])
        else:
            t = 0
        w.w16(s + 0, t)
        w.w16(s + 2, si if rng.random() > 0.04 else rng.choice([0xFFFF, 0x8000, 0x8001, rng.getrandbits(16)]))
        w.w32(s + 4, body)
        w.w32(s + 8, owner)
        if body:
            write_body(w, rng, body, cx, cy, cz, spread)
        if owner:
            if owner in (PLAYER, RCCAR):
                if rng.random() < 0.7:
                    w.w16(owner + 0x4C, si)
            else:
                w.w16(owner + 0x4C, si if rng.random() < 0.85 else rng.randint(-3, 300))
            if body != owner + 0x24:
                w.w32(owner + 0x50, rand_contact(rng))
        w.w32(s + 0xC, 0)
        w.w32(s + 0x10, prev)
        if prev:
            w.w32(prev + 0xC, s)
        else:
            w.w32(HEAD, s)
        prev = s
        info.append((s, si, t, body, owner))
    w.w32(TAIL, prev)
    w.w16(PLAYER + 0x5E, 0x2D if rng.random() < 0.05 else rng.choice([0, 1, 0x10, 0x2C, 0x2E]))
    w.w32(PLAYER + 0x70, rng.choice([0x60, 0x80, 0x00, 0xE0, 0x61, 0x8F, rng.getrandbits(32)]))
    w.w16(PLAYER + 0xE8, rng.choice([1, 100, 0, -3, 5]))
    w.w32(0x800F4450, rng.choice([0, 0, 0, 1, 3]))
    ncol = rng.choice([0, 0, 1, 2, 3, 5])
    w.w16(0x800F45D4, ncol)
    w.w32(0x800F455C, COLLIDERS)
    for i in range(ncol):
        for off in range(0, 0x38, 4):
            w.w32(COLLIDERS + 0x38 * i + off, rng.getrandbits(32))
    for addr in (0x800F45E0, 0x800F45E6, 0x800B20C4, 0x800F5A78, 0x800B20D0):
        w.w16(addr, rng.getrandbits(16))
    w.w16(0x800B20D2, rng.choice([0, 1, 0xFFFF, 7]))
    w.w32(0x800B2130, rng.getrandbits(32))
    for i in range(16):
        e = LOOPS + 12 * i
        w.w16(e, rng.choice([0xFFFF, 0xFFFF, 0, 1, 2, 3, 5, 0x7FFF, 0x8000]))
        w.w16(e + 2, rng.getrandbits(16))
        w.w32(e + 4, rng.choice([0, 1, 2, rng.getrandbits(32)]))
        w.w16(e + 8, rng.choice([0, 0, 1, 2, 0xFFFF, 0x7FFF]))
        w.w16(e + 10, rng.getrandbits(16))
    for off in range(0xF0, 0x120, 4):       # stale scratchpad contents must not matter
        w.w32(0x1F800000 + off, rng.getrandbits(32))
    return info



class Mini:
    """A hand-built two-or-three body world (pool bodies, no stray flags) for the boundary sweeps: build_world(n=0)
    supplies the random globals, add() appends a slot to the list."""

    def __init__(self, m, rng):
        build_world(m, rng, n=0, spread=100)
        self.m, self.rng, self.w = m, rng, W(m)
        self.prev = 0
        self.next_slot = rng.randint(0, 200)
        self.pool = rng.randint(0, 100)
        self.obj = 0
        w = self.w
        w.w16(0x800EBFA0, 120)          # a big level so nothing is clamped
        w.w16(0x800EBFA2, 120)
        w.w32(PLAYER + 0x70, 0)
        w.w32(PLAYER + 0x50, 0)
        w.w32(RCCAR + 0x50, 0)

    def add(self, typ, pos, radius, height, *, owner=0, contact=0, flags=0, param=0, solid=1, idx=None):
        w, rng = self.w, self.rng
        si = self.next_slot
        self.next_slot += 1
        s = SLOTS + 20 * si
        body = POOL + 0x38 * self.pool
        w.w16(POOL_FREE + 2 * self.pool, 1)
        self.pool += 1
        write_body(w, rng, body, 0, 0, 0, 0, solid=solid, radius=radius, height=height)
        for off, v in zip((0, 4, 8), pos):
            w.w32(body + off, v)
        w.w32(body + 0x2C, contact)
        w.w16(body + 0x26, flags)
        w.w16(body + 0x24, param)
        w.w16(s, typ)
        w.w16(s + 2, si)
        w.w32(s + 4, body)
        w.w32(s + 8, owner)
        if owner and idx is not None:
            w.w16(owner + 0x4C, idx)
        w.w32(s + 0xC, 0)
        w.w32(s + 0x10, self.prev)
        if self.prev:
            w.w32(self.prev + 0xC, s)
        else:
            w.w32(HEAD, s)
        w.w32(TAIL, s)
        self.prev = s
        return s, body, si


def center_dy(typ, h, flags60=False):
    """The y offset GetCenter applies to a body of this type (center = y + offset)."""
    if typ in (6, 7, 8, 9, 12, 13, 16, 17):
        return 0
    return half_s(h) if (typ == 1 and flags60) else -half_s(h)


HEIGHTS = [-301, -300, -101, -100, -3, -2, -1, 0, 1, 2, 3, 100, 101, 300, 301, 0x7FFF - 5, -0x7FFF]
RADII = [0, 1, 2, 3, 40, 41, 100, 101, 300, 301, -2, -1]


def boundary_world(m, rng, family, i):
    """Two bodies placed exactly on (+-2 of) the thresholds of `family`; every (d1, d2) pair is swept through i."""
    mw = Mini(m, rng)
    d1, d2 = (i % 5) - 2, ((i // 5) % 5) - 2
    k = i // 25
    sgn1, sgn2 = (-1, 1)[k & 1], (-1, 1)[(k >> 1) & 1]
    axis = ymode = (k >> 2) & 1
    ax, ay, az = rng.randint(-3000, 3000), rng.randint(-2000, 2000), rng.randint(-3000, 3000)
    ra, rb = rng.choice(RADII), rng.choice(RADII)
    ha, hb = rng.choice(HEIGHTS), rng.choice(HEIGHTS)

    def place_xz(length):
        return (ax + (sgn1 * length if axis == 0 else 0), az + (sgn1 * length if axis == 1 else 0))

    if family == "BodyCollideWithBodies":
        ta = rng.choice([1, 3, 4, 5, 2])
        compat = {3: [1, 2, 3], 4: [1, 2, 5], 5: [1, 2, 4], 1: [1, 2, 3, 4, 5], 2: [1, 2, 3, 4, 5]}[ta]
        tb = rng.choice(compat)
        flags60 = rng.random() < 0.3
        m.write32(PLAYER + 0x70, 0x60 if flags60 else 0)
        sa, A, _ = mw.add(ta, (ax, ay, az), ra, ha, flags=rng.choice([0, 0x8000, 0x8000]))
        bx, bz = place_xz(ra + rb + d1)
        t = abs(half_s(ha)) + abs(half_s(hb)) + d2
        cya = ay + center_dy(ta, ha, flags60)
        by = cya + sgn2 * t - center_dy(tb, hb, flags60)
        mw.add(tb, (bx, by, bz), rb, hb, flags=rng.choice([0, 0x8000]))
        return sa, ta
    if family == "BodyFindOverlapType1":
        sa, A, _ = mw.add(rng.choice([16, 16, 6]), (ax, ay, az), ra, ha)
        rad = ra + sgn2 * ha + d1
        bx, bz = (ax + sgn1 * rad, az) if axis == 0 else (ax, az + sgn1 * rad)
        by = (ay + hb + 64 - d2) if ymode else (ay - 64 + d2)
        mw.add(1, (bx, by, bz), rb, hb, contact=rng.choice([0, 0, 0xA0000000]))
        return sa, 16
    if family == "BodyFindOverlapType3to5":
        sa, A, _ = mw.add(17, (ax, ay, az), ra, ha)
        tb = rng.choice([3, 4, 5])
        bx, bz = place_xz(ra + rb + d1)
        t = ra + abs(half_s(hb)) + d2
        by = ay + sgn2 * t + half_s(hb)
        mw.add(tb, (bx, by, bz), rb, hb, contact=rng.choice([0, 0, 0xA0000000]))
        return sa, 17
    if family == "BodyCheckPickupContact":
        sa, A, _ = mw.add(9, (ax, ay, az), ra, ha, param=rng.getrandbits(15))
        tb = rng.choice([1, 2, 8, 13])
        length = ra + rb + d1
        flags60 = False
        if axis == 0:
            bx, by, bz = ax + sgn1 * length, ay, az
        else:
            bx, bz, by = ax, az, ay + sgn1 * length
        by -= center_dy(tb, hb, flags60)
        mw.add(tb, (bx, by, bz), rb, hb)
        return sa, 9
    # BodyCollideBodiesDetailed
    p1 = rng.choice([6, 7, 8, 12, 13, 14, 15, 8, 8])
    owner = rng.choice([PLAYER, PLAYER, RCCAR])
    sa, A, ia = mw.add(p1, (ax, ay, az), ra, ha, owner=owner, idx=rng.randint(300, 400), param=rng.getrandbits(16))
    tb = rng.choice([1, 2, 3, 4, 5, 1, 3])
    crouch = tb == 1 and rng.random() < 0.4
    m.write32(PLAYER + 0x70, 0x80 if crouch else (0x60 if rng.random() < 0.2 else 0))
    flags60 = (m.read32(PLAYER + 0x70) & 0xF0) == 0x60
    bx, bz = place_xz(ra + rb + d1)
    hh = (hb // 3 if hb >= 0 else -((-hb) // 3)) if crouch else hb
    t = ra + abs(half_s(hh)) + d2
    by = ay + sgn2 * t - center_dy(tb, hb, flags60)
    mw.add(tb, (bx, by, bz), rb, hb, flags=rng.choice([0, 0x4000, 0x8000]))
    if rng.random() < 0.7:
        # a far-away body of type 1 / 3 / 4 / 5 whose slot index the owner points to: BodyTypeOfActor(owner) = that
        # type, which selects the collider ray test after the list walk
        tc = rng.choice([1, 3, 4, 5, 5, 3])
        _sc, _bc, ic = mw.add(tc, (ax + 90000, ay, az + 90000), 10, 10)
        m.write16(owner + 0x4C, ic)
        if tc == 4:
            m.write16(A + 0x24, rng.choice([0xC00, 0x400, 0x800, 0x0]) | rng.getrandbits(8))
    return sa, p1



def check_ram(h):
    """AeRam itself: loaders agree with the harness loader, mirrors, bounds, alignment, sticky first error."""
    lib = h.lib
    P, u32 = C.c_void_p, C.c_uint32
    lib.ae_ram_load_exe.argtypes = [P, C.c_char_p, C.c_size_t]
    lib.ae_ram_load.argtypes = [P, u32, C.c_char_p, C.c_size_t]
    for n in ("ae_rd8", "ae_rd16"):
        getattr(lib, n).argtypes = [P, u32]
        getattr(lib, n).restype = u32
    lib.ae_wr8.argtypes = [P, u32, u32]
    lib.ae_ptr.argtypes = [P, u32, u32]
    lib.ae_ptr.restype = P
    r = lib.ae_ram_create()
    bad = []
    exe = (ROOT / "extract/disc/SCUS_944.23").read_bytes()
    if not lib.ae_ram_load_exe(r, exe, len(exe)):
        bad.append("ae_ram_load_exe failed")
    main = (C.c_uint8 * 0x200000).from_address(lib.ae_ptr(r, 0x80000000, 0x200000))
    if bytes(main) != bytes(h.base[0]):
        bad.append("ae_ram_load_exe image differs from the harness loader")
    blob = random.Random(1).randbytes(0x3000)
    ref = mr.MipsRam()
    ref.load_area(blob)
    if not lib.ae_ram_load(r, 0x80154000, blob, len(blob)) or bytes(main[0x154000:0x154000 + len(blob)]) != bytes(ref.ram[0x154000:0x154000 + len(blob)]):
        bad.append("ae_ram_load")
    lib.ae_ram_clear_error(r)
    lib.ae_wr32(r, 0x80123450, 0xCAFEBABE)
    for mirror in (0x00123450, 0xA0123450):
        if lib.ae_rd32(r, mirror) != 0xCAFEBABE:
            bad.append(f"mirror {mirror:08X}")
    lib.ae_wr32(r, 0x1F800010, 0x12345678)
    for mirror in (0x9F800010, 0xBF800010):
        if lib.ae_rd32(r, mirror) != 0x12345678:
            bad.append(f"scratchpad mirror {mirror:08X}")
    if lib.ae_ram_failed(r):
        bad.append("unexpected error flag after valid accesses")
    if lib.ae_rd16(r, 0x80123450) != 0xBABE or lib.ae_rd8(r, 0x80123453) != 0xCA:
        bad.append("little endian layout")
    probes = [("rd32 past RAM", lambda: lib.ae_rd32(r, 0x80200000)), ("rd8 past scratchpad", lambda: lib.ae_rd8(r, 0x1F800400)),
              ("rd32 unaligned", lambda: lib.ae_rd32(r, 0x80000002)), ("rd16 unaligned", lambda: lib.ae_rd16(r, 0x80000001)),
              ("unmapped segment", lambda: lib.ae_rd8(r, 0xC0000000)), ("unmapped low", lambda: lib.ae_rd8(r, 0x20000000)),
              ("ae_ptr crossing the end", lambda: lib.ae_ptr(r, 0x801FFFFF, 2) or 0), ("ae_ptr too large", lambda: lib.ae_ptr(r, 0x80000000, 0x200001) or 0),
              ("scratchpad crossing", lambda: lib.ae_ptr(r, 0x1F8003FF, 2) or 0)]
    for name, fn in probes:
        lib.ae_ram_clear_error(r)
        res = fn()
        if not lib.ae_ram_failed(r) or res:
            bad.append(f"{name}: not flagged / returned {res}")
    lib.ae_ram_clear_error(r)
    lib.ae_wr32(r, 0x80000002, 0xFFFFFFFF)      # dropped
    if not lib.ae_ram_failed(r) or lib.ae_rd8(r, 0x80000002) != 0:
        bad.append("unaligned write was not dropped")
    lib.ae_ram_clear_error(r)
    lib.ae_rd32(r, 0x80200000)
    first = lib.ae_ram_error(r)
    lib.ae_rd8(r, 0xC0000000)
    if lib.ae_ram_error(r) != first or b"80200000" not in first:
        bad.append("the first error message is not kept")
    h.counts["AeRam self-test"] = len(probes) + 8
    for line in bad:
        h.fail("AeRam", "self-test", line)
    lib.ae_ram_destroy.argtypes = [P]
    lib.ae_ram_destroy(r)


# ------------------------------------------------------------------------------------------------- the checks
def check_all(h, scale, seed):
    t0 = time.time()

    def cases(count):
        return range(max(1, int(count * scale / 100)))

    def rngf(tag, i):
        return random.Random(f"{seed}/{tag}/{i}")

    def noargs(_):
        return ()

    # ---- CollisionBodiesReset: noise over every table it touches
    for i in cases(100):
        rng = rngf("reset", i)
        def setup(m, rng=rng):
            w = W(m)
            if rng.random() < 0.5:
                build_world(m, rng)
            if rng.random() < 0.7:
                for base, size in ((HEAD, 8), (SLOTS, 256 * 20), (POOL, 128 * 0x38), (POOL_FREE, 256), (LOOPS, 16 * 12),
                                   (0x800F45E0, 0x10)):
                    for off in range(0, size, 4):
                        w.w32(base + off, rng.getrandbits(32))
        h.run("CollisionBodiesReset", f"#{i}", i, setup, noargs)

    # ---- CollisionBodyRegister (free slots, a full table, every type, owners)
    for i in cases(500):
        rng = rngf("register", i)
        def setup(m, rng=rng):
            build_world(m, rng, full=rng.random() < 0.08, n=rng.choice([0, 1, 2, 6, 20]))
            w = W(m)
            typ = rng.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 14, 15, 16, 17, 0, 18, 100, -1, 0x10003, 0xFFFF, 3, 2, 1, 6, 4, 5])
            ok = rng.choice(["none", "player", "rc", "obj", "obj"])
            owner = {"none": 0, "player": PLAYER, "rc": RCCAR, "obj": OBJ_BASE + 0x100 * (300 + rng.randint(0, 3))}[ok]
            body = (owner + 0x24) if owner and rng.random() < 0.5 else POOL + 0x38 * rng.randint(0, 127)
            if rng.random() < 0.05:
                body = 0x800D0000
            if owner:
                w.w16(owner + 0x4C, rng.getrandbits(16))
            w.w16(body + 0x26, rng.getrandbits(16))
            w.w16(body + 0x2A, rng.getrandbits(16))
            return typ, body, owner
        h.run("CollisionBodyRegister", f"#{i}", i, setup, lambda c: c, null_hooks=(i % 6 == 5))

    # ---- fail-closed behaviour: NULL hooks set the error flag instead of calling anything
    for i in cases(40):
        rng = rngf("failclosed", i)
        def setup(m, rng=rng):
            build_world(m, rng, full=True)
            return (rng.choice([1, 2, 3]), POOL, 0)
        # full table -> printf hook; the original calls printf (hooked) and returns -1: the port must flag the missing hook
        h.m.restore(h.base)
        ctx = setup(h.m)
        h.sync_to_c()
        h.c.model = Model(0, h.c)
        ret = h.c.call("CollisionBodyRegister", ctx, h.c.null_hooks)
        h.counts["fail-closed hook check"] = h.counts.get("fail-closed hook check", 0) + 1
        if not (h.lib.ae_ram_failed(h.c.ram) and (ret & M32) == M32 and b"printf_error" in h.lib.ae_ram_error(h.c.ram)):
            h.fail("fail-closed", f"#{i}", "NULL printf_error hook did not set the error flag")

    # ---- CollisionBodyAllocSlot
    for i in cases(100):
        rng = rngf("allocslot", i)
        def setup(m, rng=rng):
            build_world(m, rng, n=rng.choice([0, 1, 5, 50, 200, 255, 256]))
            if rng.random() < 0.5:
                w = W(m)
                for k in range(256):
                    if rng.random() < 0.5:
                        w.w16(SLOTS + 20 * k, rng.choice([0xFFFF, 0, 1, 0x8000, 0x7FFF]))
        h.run("CollisionBodyAllocSlot", f"#{i}", i, setup, noargs)

    # ---- CollisionBodyUnregister (head / middle / tail / only / missing; type 2 loop sound)
    for i in cases(600):
        rng = rngf("unregister", i)
        def setup(m, rng=rng):
            info = build_world(m, rng, n=rng.choice([0, 1, 2, 3, 4, 5, 8, 20, 60]))
            if info and rng.random() < 0.3:
                W(m).w16(rng.choice(info)[0], 2)
            r = rng.random()
            if info and r < 0.8:
                return (rng.choice(info)[3],)
            if r < 0.85:
                return (0,)
            return (POOL + 0x38 * rng.randint(0, 127) + rng.choice([0, 8, 0x20]),)
        h.run("CollisionBodyUnregister", f"#{i}", i, setup, lambda c: c, null_hooks=(i % 6 == 5))

    # ---- GetObjectByIndex / CollisionBodyIndexOf
    for i in cases(250):
        rng = rngf("getobj", i)
        idx = rng.choice([0, 1, 5, 100, 255, 256, 257, 258, 0xFFFF, 0x8000, 0x10001, 0x1FFFF, M32, M32 - 1, rng.randint(0, 300),
                          rng.getrandbits(32), (-50 + rng.randint(0, 350)) & M32])
        h.run("GetObjectByIndex", f"#{i}", i, lambda m, rng=rng: build_world(m, rng), lambda c, idx=idx: (idx,), allow_trap=True)
        idx2 = rng.choice([0, 1, 5, 100, 255, 256, 257, 0xFFFF, 0x8000, M32, M32 - 1, (-300) & M32, rng.randint(-20, 300) & M32,
                           rng.randint(0, 255), rng.randint(0, 255), rng.randint(0, 255), 0x10005])
        h.run("CollisionBodyIndexOf", f"#{i}", i, lambda m, rng=rng: build_world(m, rng, n=rng.choice([3, 10, 40])),
              lambda c, idx2=idx2: (idx2,), allow_trap=True)

    # ---- body pool: Alloc / Free / Clear
    for i in cases(250):
        rng = rngf("pool", i)
        def setup(m, rng=rng):
            w = W(m)
            for k in range(128):
                w.w16(POOL_FREE + 2 * k, rng.choice([0xFFFF, 0xFFFF, 1, 0, 0x8000, 0x7FFF]))
                for off in range(0, 0x38, 4):
                    w.w32(POOL + 0x38 * k + off, rng.getrandbits(32))
            if rng.random() < 0.15:
                for k in range(128):
                    w.w16(POOL_FREE + 2 * k, 1)
            elif rng.random() < 0.3:
                for k in range(rng.randint(0, 127)):
                    w.w16(POOL_FREE + 2 * k, rng.choice([1, 0]))
        h.run("CollisionBodyAlloc", f"#{i}", i, setup, noargs)
        body = rng.choice([POOL + 0x38 * rng.randint(0, 127), POOL + 0x38 * rng.randint(0, 127), POOL, POOL + 0x38 * 127,
                           POOL + 0x38 * 128, 0, POOL + 8, OBJ_BASE + 0x24])
        h.run("CollisionBodyFree", f"#{i}", i, setup, lambda c, body=body: (body,))
        cb = rng.choice([POOL + 0x38 * rng.randint(0, 127), OBJ_BASE + 0x24 + 0x100 * rng.randint(0, 10), PLAYER + 0x24])
        h.run("CollisionBodyClear", f"#{i}", i, setup, lambda c, cb=cb: (cb,))

    # ---- loop slots
    for i in cases(300):
        rng = rngf("loop", i)
        def setup(m, rng=rng):
            w = W(m)
            for k in range(16):
                e = LOOPS + 12 * k
                w.w16(e, rng.choice([0xFFFF, 0xFFFF, 0, 1, 2, 3, 0x7FFF, 0x8000, rng.getrandbits(16)]))
                w.w16(e + 2, rng.getrandbits(16))
                w.w32(e + 4, rng.choice([0, 1, 2, 3, rng.getrandbits(32)]))
                w.w16(e + 8, rng.choice([0, 0, 1, 2, 3, 0xFFFF, 0x7FFF, 0x8000]))
                w.w16(e + 10, rng.getrandbits(16))
            if rng.random() < 0.1:
                for k in range(16):
                    w.w16(LOOPS + 12 * k, rng.choice([0, 1, 2]))
        ident = rng.choice([0, 1, 2, 3, 0x7FFF, 0x8000, 0xFFFF, M32, 0x10002, 0x18001, 0x12345, rng.getrandbits(32)])
        key = rng.choice([0, 1, 2, 3, rng.getrandbits(32)])
        h.run("LoopSlotAdd", f"#{i}", i, setup, lambda c: (ident, key))
        def ref_args(c, rng=rng, ident=ident, key=key):
            return c if c else (ident, key)
        def setup_ref(m, setup=setup, rng=rng):
            setup(m)
            if rng.random() < 0.7:          # pick an existing entry so the match path runs
                e = LOOPS + 12 * rng.randint(0, 15)
                return (m.read16(e) | (0xFFFF0000 if rng.random() < 0.3 else 0), m.read32(e + 4) if rng.random() < 0.85 else key)
            return None
        h.run("LoopSlotRef", f"#{i}", i, setup_ref, ref_args)
        h.run("LoopSlotSweep", f"#{i}", i, setup, noargs)
        h.run("LoopSlotClearAll", f"#{i}", i, setup, noargs)

    # ---- CollisionBodyGetCenter / BodyContactType
    for i in cases(500):
        rng = rngf("center", i)
        typ = rng.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 0, 18, M32, (-6) & M32, 0x10006, 0x1000C,
                          0xFFFF, 0x10001, 0x10010, 0xFFFA, rng.getrandbits(32), rng.randint(-5, 20) & M32, rng.randint(-5, 20) & M32])
        out = rng.choice([SCR_N, SCR_A, SCR_B, SCR_V, 0x1F800000, 0x1F8003FA])
        body = POOL + 0x38 * rng.randint(0, 5)
        def setup(m, rng=rng, body=body):
            w = W(m)
            write_body(w, rng, body, rng.choice([0, 1000, -70000]), rng.randint(-60000, 60000), 5, rng.choice([100, 70000, 4_000_000]),
                       height=rng.choice([None, 0, 1, -1, -2, 0x7FFF, -0x8000, 0x7000, 255, 256, -255]))
            w.w32(PLAYER + 0x70, rng.choice([0x60, 0x80, 0, 0x6F, 0xF0, rng.getrandbits(32)]))
            for k in range(0, 0x20, 4):
                w.w32(SCR_V + k, rng.getrandbits(32))
        h.run("CollisionBodyGetCenter", f"#{i}", i, setup, lambda c: (typ, body, out))
        actor = rng.choice([PLAYER, RCCAR, OBJ_BASE + 0x100 * rng.randint(0, 9), POOL + 0x38 * rng.randint(0, 5)])
        h.run("BodyContactType", f"#{i}", i, setup, lambda c: (actor,))

    # ---- BodyTypeOfActor
    for i in cases(300):
        rng = rngf("typeofactor", i)
        def setup(m, rng=rng):
            info = build_world(m, rng, n=rng.choice([0, 1, 2, 5, 12]))
            w = W(m)
            r = rng.random()
            if info and r < 0.7:
                actor = OBJ_BASE + 0x100 * 200
                w.w16(actor + 0x4C, rng.choice(info)[1] if rng.random() < 0.85 else rng.randint(-3, 300))
            elif r < 0.8:
                actor = 0
            else:
                actor = PLAYER
                w.w16(actor + 0x4C, rng.randint(-3, 300))
            return (actor,)
        h.run("BodyTypeOfActor", f"#{i}", i, setup, lambda c: c)

    # ---- BodyContactSfx
    for i in cases(300):
        rng = rngf("sfx", i)
        bt = rng.choice([3, 1, 0, 5, M32 - 2, 0x10003, 0xFFFF, 7, rng.randint(-5, 12) & M32])
        ct = rng.choice([0, 1, 5, M32, 0x18000, 0x7FFF, 300, rng.randint(-5, 60) & M32, 0x10000, 0xFFFF])
        h.run("BodyContactSfx", f"#{i}", i, lambda m: None, lambda c: (bt, ct), null_hooks=(i % 6 == 5))

    # ---- VectorNormalS and VecLength3
    lib = h.lib
    for i in cases(1500):
        rng = rngf("vec", i)
        kind = rng.random()
        if kind < 0.45:
            v = [rng.randint(-1000, 1000) for _ in range(3)]
        elif kind < 0.65:
            v = [rng.randint(-20000, 20000) for _ in range(3)]
        elif kind < 0.78:
            v = [rng.choice([0, 0, 1, -1, 2, 3, -3, 4, 5, 7, 8, 100, 255, 256, 1023, 1024, 4095, 4096, 32767, -32768, -32767]) for _ in range(3)]
        elif kind < 0.88:
            v = [rng.randint(-0x80000000, 0x7FFFFFFF) for _ in range(3)]
        else:
            v = [rng.randint(-70, 70) for _ in range(3)]
        q = [signed(x & 0xFFFF, 16) ** 2 for x in v]
        overflow = q[0] + q[1] > 0x7FFFFFFF or sum(q) > 0x7FFFFFFF
        def setup(m, v=v, rng=rng):
            w = W(m)
            for k in range(3):
                w.w32(SCR_V + 4 * k, v[k])
            for k in range(0, 0x10, 4):
                w.w32(SCR_N + k, rng.getrandbits(32))
        h.run("VectorNormalS", f"#{i} {v}", i, setup, lambda c: (SCR_V, SCR_N), expect_trap=overflow)
        h.m.restore(h.base)
        h.m.gte_reset()
        res = h.m.call(0x80046618, tuple(x & M32 for x in v))[2]
        got = lib.ae_player_vec_length3(*[s32(x & M32) for x in v]) & M32
        h.counts["VecLength3 (ae_player_vec_length3)"] = h.counts.get("VecLength3 (ae_player_vec_length3)", 0) + 1
        if res != got:
            h.fail("VecLength3", str(v), f"MIPS {res:08X} C {got:08X}")

    # ---- body-vs-body tests on random lists
    kind_sets = [None, {1, 2, 3, 4, 5}, {3, 4, 5, 1}, {1, 3, 4, 5, 6, 7, 8, 12, 13, 14, 15}, {1, 2, 3, 5, 8, 9, 13}]

    def world_setup(types, nmax):
        def setup(m, rng):
            info = build_world(m, rng, kinds=rng.choice(kind_sets), n=rng.choice([1, 2, 3, 4, 5, 6, 8, nmax]),
                               spread=rng.choice([100, 200, 400, 800]))
            if len(info) > 1 and rng.random() < 0.5:     # pull two bodies together so the overlap tests run
                w = W(m)
                a, b = rng.sample(info, 2)
                if a[3] and b[3]:
                    for off in (0, 4, 8):
                        w.w32(a[3] + off, w.r32(b[3] + off) + rng.randint(-250, 250))
            return info
        return setup

    families = [
        ("BodyCollideWithBodies", [3, 4, 5, 3, 4, 5, 1, 2], 1100, lambda s, t: (s,)),
        ("BodyFindOverlapType1", [16, 16, 17, 6], 450, lambda s, t: (t, s)),
        ("BodyFindOverlapType3to5", [17, 17, 16], 450, lambda s, t: (t, s)),
        ("BodyCollideBodiesDetailed", [6, 7, 8, 12, 13, 14, 15, 6, 7, 8, 5, 3], 1500, lambda s, t: (t, s)),
        ("BodyCheckPickupContact", [9, 9, 1], 450, lambda s, t: (t, s)),
    ]
    def targeted_detailed(m, rng):
        """An attack-style body (types 6, 7, 8, 12..15) owned by the player / RC car amid bodies of types 1..5 that
        sit close to it, so the contact branches of BodyCollideBodiesDetailed all run."""
        info = build_world(m, rng, kinds=rng.choice(kind_sets), n=rng.randint(2, 8), spread=rng.choice([100, 200, 400]))
        if not info:
            return SLOTS, 6
        w = W(m)
        a = rng.choice(info)
        typ = rng.choice([6, 7, 8, 12, 13, 14, 15, 8, 8, 6, 7])
        w.w16(a[0], typ)
        if a[3]:
            r = rng.random()
            owner = PLAYER if r < 0.45 else RCCAR if r < 0.6 else a[4]
            w.w32(a[0] + 8, owner)
            if owner in (PLAYER, RCCAR):
                others = [n for n in info if n is not a]
                w.w16(owner + 0x4C, rng.choice(others)[1] if others and rng.random() < 0.5 else
                      (a[1] if rng.random() < 0.5 else rng.randint(0, 255)))
                if rng.random() < 0.6:
                    w.w32(owner + 0x50, rand_contact(rng))
            if rng.random() < 0.7:
                w.w32(a[3] + 0x2C, 0)
            for b in info:
                if b is not a and b[3]:
                    if rng.random() < 0.7:
                        w.w16(b[0], rng.choice([1, 2, 3, 3, 3, 4, 4, 5, 5]))
                    if rng.random() < 0.8:
                        for off in (0, 4, 8):
                            w.w32(b[3] + off, w.r32(a[3] + off) + rng.randint(-150, 150))
                        if rng.random() < 0.5:
                            w.w32(b[3] + 0x2C, 0)
            if rng.random() < 0.6:
                tighten(m, rng, info, a)
        return a[0], typ

    for name, types, count, mk in families:
        for i in cases(count):
            rng = rngf(name, i)
            wsetup = world_setup(types, 10)
            def setup(m, rng=rng, wsetup=wsetup, types=types):
                if name == "BodyCollideBodiesDetailed" and rng.random() < 0.6:
                    return targeted_detailed(m, rng)
                info = wsetup(m, rng)
                cands = [n_ for n_ in info if n_[2] in types] if rng.random() < 0.8 else []
                node = rng.choice(cands or info) if info else None
                if node and rng.random() < 0.6:
                    tighten(m, rng, info, node)
                slot = node[0] if node else SLOTS
                if name == "BodyCollideBodiesDetailed":
                    typ = node[2] if node and rng.random() < 0.6 else rng.choice([6, 7, 8, 12, 13, 14, 15, 14, 15, 8, 6, 0, 9, 7])
                else:
                    typ = rng.choice(types)
                return slot, typ
            h.run(name, f"#{i}", i, setup, lambda c, mk=mk: mk(*c), stub=random.Random(i).choice([0, 0, 0, 1]),
                  null_hooks=(i % 8 == 7))

    # ---- boundary sweeps: two bodies exactly on (+-2 of) every threshold the tests compare against
    mkmap = {name: mk for name, _types, _count, mk in families}
    for name in ("BodyCollideWithBodies", "BodyFindOverlapType1", "BodyFindOverlapType3to5", "BodyCheckPickupContact",
                 "BodyCollideBodiesDetailed"):
        for i in cases(500):
            rng = rngf("boundary-" + name, i)
            h.run(name, f"boundary #{i}", i, lambda m, rng=rng, name=name, i=i: boundary_world(m, rng, name, i),
                  lambda c, mk=mkmap[name]: mk(*c), stub=random.Random(i).choice([0, 0, 1]))

    # ---- CollisionUpdate
    for i in cases(900):
        rng = rngf("update", i)
        def setup(m, rng=rng):
            # Types 1 / 2 are checked below against real area data by check_bodymove.py.
            # Keep this suite on CollisionUpdate's remaining cases and list-management paths.
            remaining_types = (0, *range(3, 18))
            build_world(m, rng, n=rng.choice([0, 1, 2, 3, 4, 5, 6, 8, 10, 14]),
                        spread=rng.choice([150, 300, 600]), force_types=remaining_types)
        h.run("CollisionUpdate", f"#{i}", i, setup, noargs, stub=random.Random(i).choice([0, 0, 0, 1]),
              null_hooks=(i % 8 == 7))
    print(f"scenario families done in {time.time() - t0:.1f}s")

    # The player and RC-car cases need the native platform adapters and area collision member. Reuse the
    # body-movement differential harness for one case of each body type in each real area at every scale.
    import check_bodymove
    import check_game as cg
    area_snapshot = h.m.snapshot()
    coverage = h.m.coverage
    h.m.coverage = None
    area_harness = check_bodymove.Harness(cg.build_dll(tag="bodies_area"), with_coverage=False, mips=h.m)
    try:
        for area in check_bodymove.AREAS:
            for typ in (1, 2):
                area_harness.run_collision_update(seed + 0xA0000 + area * 8 + typ, area, typ)
    finally:
        h.m.restore(area_snapshot)
        h.m.coverage = coverage
    if area_harness.failures:
        h.fail("CollisionUpdate", "real-area inline cases",
               f"check_bodymove reported {area_harness.failures} failure(s)")
    print(f"real-area inline type-1/type-2 CollisionUpdate cases: {len(check_bodymove.AREAS) * 2}")


def finish(h, show_coverage, default_run):
    """Coverage summary (own functions: the registry's unreachable rows are subtracted) and, for a run at the default scale and
    seed, extract/checks/results/check_bodies.json."""
    summary = None
    if h.m.coverage is not None:
        items = [(n, a, s, True) for n, (a, s, *_rest) in FUNCS.items()] + [(n, a, s, False) for n, (a, s) in EXTRA_COVERAGE.items()]
        summary = reg.coverage_summary(items, h.m.coverage)
        if show_coverage:
            reg.print_coverage(summary)
    if default_run:
        counts = dict(h.counts)
        counts["VecLength3"] = counts.pop("VecLength3 (ae_player_vec_length3)", 0)      # ported in src/player/native_player.c, checked here
        reg.write_results(counts, summary, not h.failures, function_names=list(FUNCS) + ["VecLength3"])
    return summary


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", type=float, default=40, help="scale of the case counts in percent (40 = about 6000 cases, 30 s; 100 = full)")
    ap.add_argument("--seed", type=int, default=0x944230)
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--trapv", action="store_true", help="build the port with -ftrapv (signed overflow aborts)")
    ap.add_argument("--bodies-src", help="test another copy of ae_bodies.c (mutation testing of the checker itself)")
    a = ap.parse_args()
    if not (ROOT / "extract/disc/SCUS_944.23").exists():
        print("SKIP: extract/disc/SCUS_944.23 not present")
        return 0
    t0 = time.time()
    h = Harness(build_dll(a.bodies_src, a.trapv), True)
    check_all(h, a.cases, a.seed)
    check_ram(h)
    print("cases per function:")
    for k, v in sorted(h.counts.items()):
        print(f"  {k:36s} {v}")
    print(f"original faults matched by a port error flag: {h.traps_agreed}; NULL-hook (fail closed) cases: {h.fail_closed_checked}")
    print(f"total cases {sum(h.counts.values())}; hook calls compared {h.hook_calls}; "
          f"MIPS instructions executed {h.m.steps}; {time.time() - t0:.1f}s")
    finish(h, a.coverage, default_run=(a.cases == ap.get_default("cases") and a.seed == ap.get_default("seed")
                                       and not a.trapv and not a.bodies_src))
    if h.failures:
        print(f"FAILED: {h.failures} mismatching cases")
        return 1
    print("OK: the C port matches the original instructions on every case")
    return 0


if __name__ == "__main__":
    sys.exit(main())
