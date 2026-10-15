"""Shared differential-test harness for the monkey port (src/monkeys/ae_monkey*.c).  Not a check itself (the
checks are tools/mips/check_monkey_<batch>.py, found by tools/run_checks.py); it provides:

  * parse_hooks(): the AeMonkeyHooks table, read from src/monkeys/ae_monkey.h (one `AeMonkeyHookFn Name;` line per
    hook, followed by a comment starting with the 8 digit address of the original function);
  * HOOK_SPECS: how each hook behaves in a test (arguments logged, stack buffers peeked, deterministic result);
    a later batch adds its hooks here only while they are still hooks;
  * MonkeyHarness: runs one original function on the MIPS harness (tools/mips/mips_ram.py, GTE through the pinned
    PCSX-ReARMed oracle) and the C port (a DLL over AeRam) on identical RAM images with identical hook behaviour and
    requires identical return value, RAM (outside the stack window), scratchpad and hook-call sequence;
  * RAM scenario helpers (build_monkey_world and friends) that lay out monkeys, nav nodes, the MonkeyInfo table and the
    globals the way MonkeySpawnAll leaves them.

The structure follows tools/mips/check_bodies.py (whose body hooks / model are reused for the collision-body ports that
the monkey code calls directly).
"""
import ctypes as C
import os
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
from mips_ram import signed  # noqa: E402
import check_bodies as cb  # noqa: E402
import exclusion_registry as reg  # noqa: E402

OUT = ROOT / "extract/mips"
HEADER = ROOT / "src/monkeys/ae_monkey.h"
M32 = 0xFFFFFFFF
STACK_LO, STACK_HI = 0x801FF000, 0x80200000


def s32(v): return signed(v, 32)
def s16(v): return signed(v, 16)


# ------------------------------------------------------------------------------------------ the hook table
def parse_hooks():
    """[(name, original address)] in struct order, from the header."""
    text = HEADER.read_text(encoding="utf-8")
    body = text[text.index("typedef struct AeMonkeyHooks"):]
    body = body[:body.index("} AeMonkeyHooks;")]
    return [(m.group(1), int(m.group(2), 16))
            for m in re.finditer(r"AeMonkeyHookFn\s+(\w+);\s*/\*\s*([0-9A-Fa-f]{8})", body)]


HOOKFN = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)


def hooks_struct(names):
    return type("MonkeyHooks", (C.Structure,), {"_fields_": [("user", C.c_void_p), ("body", C.c_void_p)] +
                                                [(n, HOOKFN) for n in names]})


# name -> dict(n = arguments that are logged, peek = {argument index: bytes dumped at that address (s16 words)},
#              effect = f(model, a0, a1, a2, a3) -> result (default: a deterministic random 32 bit value))
def _effect_gimmick(mod, a0, a1, a2, a3):
    g = mod.rng("gimmick-info")
    typ = g.choice([1, 1, 6, 9, 10, 17, 24, 49, 101, 101, 0, 2, 5, 11, 25, 50, -1, g.randint(-5, 130)])
    val = g.choice([0, 0, 1, 2, 3, 5, -1, -5, 0x7FFF, 0xFFFF, 4, g.getrandbits(16)])
    mod.mem.wr16(a1, typ & 0xFFFF)
    mod.mem.wr16(a1 + 2, val & 0xFFFF)
    return g.choice([-1, 0, 1, 7, 0, 3, -1, 0x1000]) if g.random() < 0.15 else 0


def _effect_matrix(mod, a0, a1, a2, a3):
    # ModelPoseBones / MatrixFromDirXZ rewrite bone matrices: perturb the matrix the scenario points ObjGetBonePos at
    g = mod.rng("pose")
    base = mod.ctx.get("matrix") if isinstance(mod.ctx, dict) else None
    if base is not None:
        for i in range(0, 0x20, 2):
            if g.random() < 0.5:
                mod.mem.wr16(base + i, g.choice([0, 0x1000, -0x1000 & 0xFFFF, 0x0B50, g.getrandbits(16)]))
    return g.getrandbits(32) & M32


def _effect_rtp(mod, a0, a1, a2, a3):
    # RotTransPers: screen xy (s16 x, s16 y) at a1, "p" word at a2, flag word at a3, returns OTZ (the interesting thresholds of
    # MonkeyProjectToScreen: x + 50 < 485, -25 <= y < 266, otz > 0)
    g = mod.rng("rtp")
    sx = g.choice([-51, -50, -49, 0, 100, 300, 434, 435, 436, -200, g.randint(-32768, 32767)])
    sy = g.choice([-26, -25, -24, 0, 100, 265, 266, 267, g.randint(-32768, 32767)])
    mod.mem.wr16(a1, sx & 0xFFFF)
    mod.mem.wr16(a1 + 2, sy & 0xFFFF)
    mod.mem.wr32(a2, g.getrandbits(32))
    mod.mem.wr32(a3, g.getrandbits(32))
    return g.choice([0, 1, -1, 5, 100, 0x3FFF, -300, 2])


def _effect_ground(mod, a0, a1, a2, a3):
    # GetGroundHeight: relative to the y argument (the MonkeyLineOfSight second test) or to the target y (the first test)
    g = mod.rng("ground")
    ty = mod.ctx.get("ty", 0) if isinstance(mod.ctx, dict) else 0
    r = g.random()
    if r < 0.55:
        return s32(a1 + g.choice([0, 0, 1, -1, -49, -50, -51, 10, -199, 50, -48]))
    if r < 0.9:
        return s32(ty + g.choice([-201, -200, -199, 0, 5, 100, -1000, -202]))
    return s32(g.getrandbits(32)) if r < 0.95 else s16(g.getrandbits(16))


def _effect_clear_words(mod, a0, a1, a2, a3):
    # ClearWords(ptr, n): zeroes n 32 bit words (the effect happens on both sides in the hook, like the original leaf routine)
    for i in range(max(0, min(a1 & M32, 4096))):
        mod.mem.wr32((a0 + 4 * i) & M32, 0)
    return 0


HOOK_SPECS = {
    "Printf": dict(n=1, n_by_a0={0x800A4964: 4, 0x800A4988: 3, 0x800A499C: 2, 0x800A4930: 2}),     # MonkeyPickGoalNode passes its debug values
    "GimmickGetInfo": dict(n=2, effect=_effect_gimmick),
    "PlaySfxAt": dict(n=2, peek={1: 6}),
    "ProjectileSpawn": dict(n=4, peek={0: 6, 3: 6}),
    "SpawnParticle": dict(n=2, peek={1: 6}),
    "TargetListAdd": dict(n=1),
    "TargetListRemove": dict(n=1),
    "MatrixFromDirXZ": dict(n=2, effect=_effect_matrix),
    "ModelPoseBones": dict(n=2, effect=_effect_matrix),
    "ModelTransformRigid": dict(n=3),
    "ModelDraw": dict(n=3),
    "MenuItemInitBound": dict(n=3),
    "MenuItemSetRange": dict(n=3),
    "MenuItemSetCallback": dict(n=2),
    "MenuItemSetStep": dict(n=2),
    # batch 2
    "EffectBigBurst": dict(n=3),
    "SparkAddRandom": dict(n=3, peek={0: 6, 1: 6}),
    "ModelInit": dict(n=2),
    "SetRotMatrix": dict(n=1),
    "SetTransMatrix": dict(n=1),
    "RotTransPers": dict(n=4, peek={0: 6}, effect=_effect_rtp),
    "GetGroundHeight": dict(n=3, effect=_effect_ground),
    # batch 3
    "ObjSlotGetAnimIndex": dict(n=1, effect=lambda mod, a0, a1, a2, a3: mod.rng("slot-anim").choice([0, 1, 1, 2, -1, 5])),
    "AlertIconDraw": dict(n=3, peek={0: 6}),
    "ObjSlotSetField_CF": dict(n=2),
    "ObjSlotGetTarget": dict(n=2),
    "SoundLoopStartEx": dict(n=3),
    "SoundLoopStopAndFree": dict(n=2),
    "SpawnPuff": dict(n=1, peek={0: 6}),
    "MatrixLookDir": dict(n=2),
    "ScaleMatrix": dict(n=2, peek={1: 12}),
    "ModelBuildBoneMatrices": dict(n=2),
    "ModelTransformSkinned": dict(n=1),
    "ModelDrawWithOt": dict(n=4),
    # batch 4
    "GimmickEvent": dict(n=3),
    "ActorAnimDecode": dict(n=1),
    "DrawShadow": dict(n=4),
    "ColorFadeSet": dict(n=2),
    "DebugTextPrintfWhite": dict(n=2),
    # batch 5
    "ClearWords": dict(n=2, effect=_effect_clear_words),
    "VSync": dict(n=1),
    "StageExitIsActive": dict(n=0, effect=lambda mod, a0, a1, a2, a3: mod.rng("stage-exit").choice([0, 1, 2, 0, 1])),
}


def norm(a):
    return "STK" if STACK_LO <= (a & M32) < STACK_HI else a & M32


class MipsMem:
    def __init__(self, cpu): self.c = cpu
    def rd8(self, a): return self.c.read8(a)
    def rd16(self, a): return self.c.read16(a)
    def rd32(self, a): return self.c.read32(a)
    def rds16(self, a): return self.c.reads16(a)
    def wr8(self, a, v): self.c.write8(a, v & 0xFF)
    def wr16(self, a, v): self.c.write16(a, v & 0xFFFF)
    def wr32(self, a, v): self.c.write32(a, v & M32)


class CMem:
    def __init__(self, side): self.s = side
    def _off(self, a): return a & 0x1FFFFFFF
    def rd8(self, a): return self.s.main[self._off(a)]
    def rd16(self, a): o = self._off(a); return self.s.main[o] | self.s.main[o + 1] << 8
    def rd32(self, a): return self.s.rd32(a)
    def rds16(self, a): return s16(self.rd16(a))
    def wr8(self, a, v): self.s.main[self._off(a)] = v & 0xFF
    def wr16(self, a, v): self.s.wr16(a, v)
    def wr32(self, a, v): self.s.wr32(a, v)


class Model(cb.Model):
    """Deterministic behaviour of every hook (identical on both sides), plus the logging of each call."""

    def __init__(self, seed, mem, stub=0):
        super().__init__(seed, mem, stub)
        self.ctx = None

    def hook(self, name, a0, a1, a2, a3):
        spec = HOOK_SPECS[name]
        n_args = spec.get("n_by_a0", {}).get(a0 & M32, spec["n"])
        args = [a0 & M32, a1 & M32, a2 & M32, a3 & M32][:n_args]
        entry = [name] + [norm(a) for a in args]
        for idx, nbytes in spec.get("peek", {}).items():
            a = args[idx]
            if STACK_LO <= a < STACK_HI or 0x80000000 <= a < 0x80200000:
                entry.append(tuple(s16(self.mem.rd16(a + i)) for i in range(0, nbytes, 2)))
        if name == "ProjectileSpawn" and args[2] in (5, 8) and isinstance(entry[-1], tuple):
            # MonkeyFireProjectileAimed / Fire8 store all three offset components into the same halfword (the original does), so the
            # other two halfwords the hook receives are leftover stack contents (depend on the call chain: not compared)
            reg.declare("ram_mask", "hook ProjectileSpawn peek[1:3] when a2 in (5,8)")
            entry[-1] = (entry[-1][0], None, None)
        if name == "Printf" and args[0] == 0x800A5D20:       # the collision-body error message: (fmt, type)
            self.log.append(("printf", a0 & M32, s32(a1)))
            return 0
        self.log.append(tuple(entry))
        eff = spec.get("effect")
        if eff:
            return eff(self, a0, a1, a2, a3)
        return s32(self.rng("ret-" + name).getrandbits(32))


class CSide:
    """The C port: the DLL, an AeRam, views of its memory and the hook callbacks forwarding to the current Model."""

    def __init__(self, dll, funcs, hook_names):
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
        self.funcs = funcs
        for name, spec in funcs.items():
            ret, takes_hooks, kinds = spec[2:5]
            f = getattr(lib, spec[5] if len(spec) > 5 else "ae_" + name)
            f.argtypes = [P] + ([P] if takes_hooks else []) + [i32 if k == "s" else u32 for k in kinds]
            f.restype = {"s": i32, "u": u32, "v": None}[ret]
        self.ram = lib.ae_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scr = (C.c_uint8 * 0x400).from_address(lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.model = None
        self.mem = CMem(self)
        m = lambda: self.model  # noqa: E731
        # collision-body hooks (the monkey code calls the body ports directly)
        self.body_hooks = cb.Hooks()
        self._keep = []
        bfns = {
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
        for name, fn in bfns.items():
            cbk = dict(cb.Hooks._fields_)[name](fn)
            self._keep.append(cbk)
            setattr(self.body_hooks, name, cbk)
        # monkey hooks
        self.Hooks = hooks_struct(hook_names)
        self.hooks = self.Hooks()
        self.hooks.body = C.cast(C.byref(self.body_hooks), C.c_void_p)
        for name in hook_names:
            cbk = HOOKFN(lambda u, r, a0, a1, a2, a3, name=name: s32(m().hook(name, a0, a1, a2, a3)))
            self._keep.append(cbk)
            setattr(self.hooks, name, cbk)
        self.null_hooks = self.Hooks()

    def _ray(self, arg, params, out):
        res, o0, o1 = self.model.ray(arg, params)
        out[0], out[1] = o0, o1
        return res

    def rd32(self, a): return self.lib.ae_rd32(self.ram, a)
    def wr32(self, a, v): self.lib.ae_wr32(self.ram, a, v & M32)
    def rds16(self, a): return self.lib.ae_rds16(self.ram, a)
    def wr16(self, a, v): self.lib.ae_wr16(self.ram, a, v & 0xFFFF)

    def call(self, name, args, hooks=None):
        spec = self.funcs[name]
        takes_hooks, kinds = spec[3], spec[4]
        conv = [s32(a) if k == "s" else a & M32 for a, k in zip(args, kinds)]
        f = getattr(self.lib, spec[5] if len(spec) > 5 else "ae_" + name)
        if takes_hooks:
            return f(self.ram, C.byref(hooks if hooks is not None else self.hooks), *conv)
        return f(self.ram, *conv)


def build_dll(extra_srcs=(), overrides=None, trapv=False, tag="monkeys"):
    """Builds the port DLL: AeRam, the body port, the SDK maths and every src/monkeys/ae_monkey*.c.  `overrides`
    maps a repository path (relative) to a replacement file (mutation testing of the checker)."""
    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob(f"{tag}_*.dll"):
        try:
            if time.time() - old.stat().st_mtime > 600:
                old.unlink()
        except OSError:
            pass
    dll = OUT / f"{tag}_{os.getpid()}.dll"
    rels = ["src/core/ae_ram.c", "src/core/ae_sdk.c", "src/bodies/ae_bodies.c", "src/player/native_player.c"]
    rels += sorted(str(p.relative_to(ROOT)).replace("\\", "/") for p in (ROOT / "src/monkeys").glob("ae_monkey*.c"))
    rels += list(extra_srcs)
    overrides = overrides or {}
    srcs = [Path(overrides.get(rel, ROOT / rel)) for rel in rels]
    flags = ["-O1", "-ftrapv"] if trapv else ["-O2"]
    subprocess.run([shutil.which("gcc"), "-std=c11", *flags, "-Wall", "-Wextra", "-Werror", "-shared", "-static-libgcc",
                    f"-I{ROOT / 'src/monkeys'}", f"-I{ROOT / 'src/core'}", f"-I{ROOT / 'src/bodies'}", *map(str, srcs), "-o", str(dll), "-lm"], check=True)
    return dll


# ------------------------------------------------------------------------------------------ the harness
def _memset_hook(cpu, r):
    """memset(dst, byte, count) of the console BIOS: fills the bytes, returns dst.  Not logged (the port does the fill inline)."""
    dst, val, cnt = r[4] & M32, r[5] & 0xFF, r[6] & M32
    for i in range(cnt):
        cpu.write8(dst + i, val)
    return dst


class MonkeyHarness:
    def __init__(self, funcs, with_coverage=False, overrides=None, trapv=False, area_member=None, tag="monkeys"):
        """funcs: name -> (original address, size in bytes, return kind 's' / 'u' / 'v', takes hooks, argument kinds)"""
        self.funcs = funcs
        self.hook_list = parse_hooks()
        self.hook_names = [n for n, _ in self.hook_list]
        missing = [n for n in self.hook_names if n not in HOOK_SPECS]
        if missing:
            raise SystemExit(f"monkey_harness.HOOK_SPECS has no entry for hook(s): {missing}")
        self.m = mr.MipsRam.from_extract(area=area_member)
        self.m.attach_gte_oracle()
        if with_coverage:
            self.m.coverage = set()
        self.base = self.m.snapshot()
        self.reload_c(overrides, trapv, tag)
        self.stack_lo = STACK_LO - 0x80000000
        reg.declare("stack_window", f"{STACK_LO:08X}..{STACK_HI:08X}")
        self.stop_after = None
        self._mask, self._mask_ok = [], set()
        self.mask = []          # [(RAM offset, length)] excluded from the RAM comparison (uninitialised-stack padding); each entry
                                # must be declared as a ram_mask row of this checker in tools/mips/check_exclusions.csv
        self.s2_funcs = set(re.findall(r"\bae_(\w+)\([^;]*\bs2_in\b", HEADER.read_text(encoding="utf-8")))

    @property
    def mask(self):
        return self._mask

    @mask.setter
    def mask(self, entries):
        key = tuple(entries)
        if key not in self._mask_ok:
            reg.check_ram_mask(key)             # exits (code 3) when an entry is not declared
            self._mask_ok.add(key)
        self._mask = list(entries)

    def reload_c(self, overrides=None, trapv=False, tag="monkeys"):
        """(Re)builds the port DLL, optionally with replaced source files (mutation testing), and resets the counters."""
        self.c = CSide(build_dll(overrides=overrides, trapv=trapv, tag=tag), self.funcs, self.hook_names)
        self.lib = self.c.lib
        self.counts = {}
        self.failures = 0
        self.hook_calls = 0
        self.traps_agreed = 0
        self.fail_closed_checked = 0

    # -- bookkeeping
    def sync_to_c(self):
        C.memmove(self.c.main, bytes(self.m.ram), 0x200000)
        C.memmove(self.c.scr, bytes(self.m.scratch), 0x400)
        self.lib.ae_ram_clear_error(self.c.ram)

    def fail(self, name, label, *lines):
        self.failures += 1
        print(f"FAIL {name} [{label}]")
        for line in lines:
            print("   ", line)
        if self.stop_after and self.failures >= self.stop_after:
            raise SystemExit(1)

    def mips_hooks(self, model):
        hooks = cb.mips_hooks(model)             # body hooks (printf is replaced below)
        def mk(name):
            return lambda cpu, r: model.hook(name, r[4], r[5], r[6], r[7]) & M32
        for name, addr in self.hook_list:
            hooks[addr] = mk(name)
        hooks[0x800A3740] = _memset_hook        # libc memset (a jump to the BIOS A0 table, absent from the image): zero fill as on the console
        return hooks

    def compare(self, name, label, mret, cret, mlog, clog, kind):
        problems = []
        if self.lib.ae_ram_failed(self.c.ram):
            problems.append("C error flag: " + self.lib.ae_ram_error(self.c.ram).decode())
        if kind != "v" and (mret & M32) != (cret & M32):
            problems.append(f"return value MIPS {mret & M32:08X} C {cret & M32:08X}")
        if mlog != clog:
            problems.append(f"hook log differs\n      MIPS {mlog}\n      C    {clog}")
        lo = self.stack_lo
        a, b = bytearray(bytes(self.m.ram)[:lo]), bytearray(bytes(self.c.main)[:lo])
        for off, ln in self.mask:                  # words that receive uninitialised stack and are never read (see the notes)
            a[off:off + ln] = bytes(ln)
            b[off:off + ln] = bytes(ln)
        a, b = bytes(a), bytes(b)
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

    def run(self, name, label, seed, setup, argsf, *, null_hooks=False, allow_trap=False, expect_trap=False,
            stub=0, patches=None):
        """setup(m) builds the scenario in the MIPS RAM image and returns a context; argsf(context) gives the raw
        argument words.  The scenario is copied to the C RAM and both implementations run on it.
        allow_trap: the original may fault / trap (the port must then have set its error flag); expect_trap: it must."""
        addr, kind = self.funcs[name][0], self.funcs[name][2]
        if name.split(" ")[0] in self.s2_funcs:
            reg.declare("shared_random", f"{name} $s2")      # the caller's $s2 (uninitialised register): the checks pass a constant
        self.m.restore(self.base)
        self.m.gte_reset()
        ctx = setup(self.m)
        args = argsf(ctx)
        self.sync_to_c()
        mm, cm = Model(seed, MipsMem(self.m), stub), Model(seed, self.c.mem, stub)
        mm.ctx = cm.ctx = ctx
        self.c.model = cm
        self.m.hook_log.clear()
        self.counts[name] = self.counts.get(name, 0) + 1
        trap = None
        try:
            regs = self.m.call(addr, args, hooks=self.mips_hooks(mm), patches=(patches(mm) if patches else {}))
            mret = regs[2]
        except mr.MipsError as e:
            trap, mret = str(e), None
        self.hook_calls += len(mm.log)
        cret = self.c.call(name, args, self.c.null_hooks if null_hooks else None)
        if cret is None:
            cret = 0
        if null_hooks:
            needs = any(entry[0] != "stub" for entry in mm.log)
            flagged = bool(self.lib.ae_ram_failed(self.c.ram))
            self.fail_closed_checked += 1
            if needs:
                if not flagged or b"not provided" not in self.lib.ae_ram_error(self.c.ram):
                    self.fail(name, label, f"NULL hook was needed ({mm.log[:2]}) but the port did not flag it")
                return
            if trap is None:
                self.compare(name, label, mret, cret, mm.log, mm.log, kind)
            return
        if expect_trap:
            if trap is None or not self.lib.ae_ram_failed(self.c.ram):
                self.fail(name, label, f"expected the original to trap and the port to flag it (trap={trap}, "
                          f"port flagged={bool(self.lib.ae_ram_failed(self.c.ram))})")
            return
        if trap:
            if allow_trap and self.lib.ae_ram_failed(self.c.ram):
                self.traps_agreed += 1
                return
            self.fail(name, label, "MIPS harness error: " + trap,
                      f"port flagged={bool(self.lib.ae_ram_failed(self.c.ram))}")
            return
        if self.lib.ae_ram_failed(self.c.ram):
            self.fail(name, label, "the original completed but the port flagged: " + self.lib.ae_ram_error(self.c.ram).decode())
            return
        self.compare(name, label, mret, cret, mm.log, cm.log, kind)

    def finish(self, extra=None, default_run=False, show_coverage=False):
        """End of a run: the coverage summary (own functions: the registry's unreachable rows are subtracted; `extra`: code the
        functions reach, informational) and, for a run at the default scale and seed, extract/checks/results/<checker>.json."""
        summary = None
        if self.m.coverage is not None:
            items = [(n, spec[0], spec[1], True) for n, spec in self.funcs.items()]
            items += [(n, a, s, False) for n, (a, s) in (extra or {}).items()]
            summary = reg.coverage_summary(items, self.m.coverage)
            if show_coverage:
                reg.print_coverage(summary)
        if default_run:
            reg.write_results(self.counts, summary, not self.failures, function_names=list(self.funcs))
        return summary


# ------------------------------------------------------------------------------------------ RAM scenarios
G_CUR, G_INFO, G_NAV, G_NAVCOUNT = 0x800B4D08, 0x800B4CF0, 0x800B4D00, 0x800B4D18
MONKEYS, MONKEY_SIZE = 0x800E5500, 0x250
INFO_BASE, NAV_BASE, LINK_BASE = 0x801E0000, 0x801D0000, 0x801D8000
OBJS = 0x800E6960
assert reg.PATTERNS["monkey"] == (MONKEYS, MONKEY_SIZE, 8)


class W:
    def __init__(self, m): self.m = m
    def w8(self, a, v): self.m.write8(a, v & 0xFF)
    def w16(self, a, v): self.m.write16(a, v & 0xFFFF)
    def w32(self, a, v): self.m.write32(a, v & M32)
    def r8(self, a): return self.m.read8(a)
    def r16(self, a): return self.m.read16(a)
    def r32(self, a): return self.m.read32(a)
    def rand_fill(self, rng, a, n, wild=True):
        data = rng.randbytes(n) if wild else bytes(rng.randint(0, 3) for _ in range(n))
        self.m.write_bytes(a, data)


def sane_or_wild(rng, lo, hi):
    """A value that is mostly moderate and sometimes an extreme (to exercise overflow / wraparound)."""
    r = rng.random()
    if r < 0.8:
        return rng.randint(lo, hi)
    if r < 0.9:
        return rng.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x8000, -0x8000, 0x7FFF, 0xFFFF])
    return rng.randint(-0x80000000, 0x7FFFFFFF)


def build_nav(m, rng, n=None, real_area=False):
    """Nav node array (0x18 bytes each) + sorted link lists.  Returns the node count."""
    w = W(m)
    n = n if n is not None else rng.choice([1, 2, 3, 5, 8, 20, 40])
    lp = LINK_BASE
    for i in range(n):
        a = NAV_BASE + 0x18 * i
        w.rand_fill(rng, a, 0x18)
        w.w32(a, rng.choice([0, 0, 0x400, 0x4, 0x8, 0xC, 0x800C, 0x1800C, 0x10000, 0x400 | 0x8000, 1, 1, 2, 3, 0x1000D, 0x20000, 0x80000000, rng.getrandbits(32)]))
        cnt = rng.choice([0, 1, 2, 3, 4, 5, 6, 8])
        w.w8(a + 0xA, cnt)
        ids = sorted(rng.sample(range(0, 40), cnt))
        w.w32(a + 0xC, lp)
        for j, lid in enumerate(ids):
            w.w16(lp + 2 * j, lid | rng.choice([0, 0, 0x4000, 0x8000 if rng.random() < 0.1 else 0]))
        lp += 2 * max(cnt, 1)
        w.w16(a + 0x10, rng.choice([rng.randint(-32768, 32767), rng.randint(0, 5000), rng.randint(-300, 300),
                                    rng.randint(0, 40) * 100 + rng.randint(0, 99)]))
        w.w16(a + 0x12, rng.choice([rng.randint(-32768, 32767), rng.randint(0, 5000), rng.randint(-300, 300),
                                    rng.randint(0, 40) * 100 + rng.randint(0, 99)]))
    w.w32(G_NAV, NAV_BASE)
    w.w16(G_NAVCOUNT, n)
    return n


def build_info(m, rng):
    """MonkeyInfo table: 16 entries of 0x88 bytes."""
    w = W(m)
    for i in range(16):
        e = INFO_BASE + 0x88 * i
        w.rand_fill(rng, e, 0x88)
        w.w16(e, rng.choice([0, 64, 192, 320, 0, 64, 1, 255, 65, 100, 191, 193, 63, 321, rng.getrandbits(16)]))
        w.w16(e + 4, rng.choice([0, 0, 1, -1, 500, rng.getrandbits(16)]))
        w.w16(e + 0x58, rng.choice([0, 1, 50, 200, rng.getrandbits(16)]))
    w.w32(G_INFO, INFO_BASE)


def build_monkey_world(m, rng, wild=None):
    """The globals the monkey code reads, 8 monkeys with random contents, the info table and the nav graph.
    Returns the current monkey's address (g_CurMonkey is set to it)."""
    w = W(m)
    wild = rng.random() < 0.35 if wild is None else wild
    for i in range(8):
        a = MONKEYS + MONKEY_SIZE * i
        w.rand_fill(rng, a, MONKEY_SIZE, wild=True)
        # structured fields (mostly sane values, so the common paths run)
        for off in (0x24, 0x28, 0x2C):
            w.w32(a + off, sane_or_wild(rng, -20000, 20000) if not wild else rng.getrandbits(32))
        w.w8(a + 0x78, rng.choice([rng.randint(0, 0x42)] * 3 + [rng.randint(0, 255)]))
        w.w8(a + 0x7A, rng.choice([0, 1, 2, 3, 4, 5]))
        w.w8(a + 0x7B, rng.randint(0, 7))
        w.w8(a + 0x7C, rng.randint(0, 15))
        w.w8(a + 0xD4, rng.choice([0, 1, 2, 3, 0xFF, 0x80, 0x7F, rng.getrandbits(8)]))
        for off in (0xF2, 0xF4, 0xF6, 0xF8, 0xFA, 0xFC):
            w.w16(a + off, rng.choice([0, 1, 2, 3, 5, 6, 19, 20, 151, rng.getrandbits(16)]))
        w.w16(a + 0x220, rng.choice([rng.randint(-300, 600), 200, 0, 1, -1, rng.getrandbits(16)]))
        for off in (0x20C, 0x214, 0x218):
            w.w32(a + off, rng.choice([rng.randint(0, 4000), rng.randint(-100, 3200), 800, 799, 2000, 2001, 260, 259,
                                       3000, 3001, rng.getrandbits(32)]))
        w.w32(a + 0x228, rng.choice([0, 2, 3, rng.getrandbits(32)]))
        w.w32(a + 0x248, rng.choice([0, 2, 0x08000000, 0xF7FFFFFF, rng.getrandbits(32)]))
        w.w32(a + 0x50, rng.getrandbits(32))
    cur = MONKEYS + MONKEY_SIZE * rng.randint(0, 7)
    w.w32(G_CUR, cur)
    w.w32(0x800B4D10, rng.choice([rng.getrandbits(16), rng.getrandbits(32), 0, 0xFFFF]))
    w.w32(0x800B4CF8, rng.choice([0x800EC1E0, 0x800F5A20, 0x800E6A00]))
    w.w8(0x800B4CF4, rng.choice([0, 0, 0, 1, 2]))
    w.w16(0x800AEB68, rng.choice([0, 1, 4, 8, 3]))
    w.w16(0x800AEB66, rng.choice([1, 2, 4]))
    w.w16(0x800AEB5A, rng.choice([0, 1, 3, 15, -1, rng.randint(-40, 40)]))
    w.w8(0x800AEBF4, rng.getrandbits(8))
    w.w32(0x800B4D44, rng.getrandbits(32))
    area = rng.choice([0x29, 0x43, 0x35, 0x46, 0x4D, 0x1A, rng.randint(0, 0x50), rng.getrandbits(16)])
    w.w16(0x800F4476, area)
    w.w16(0x800F4472, rng.choice([3, 6, 8, 0, 1, 2, 4, 5, 7, rng.getrandbits(16)]))
    for i in range(8):
        w.w8(0x800F44BC + i, rng.getrandbits(8))
    build_info(m, rng)
    build_nav(m, rng)
    return cur
