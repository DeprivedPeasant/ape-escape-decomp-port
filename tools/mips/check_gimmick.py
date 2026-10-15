"""Differentially check the RAM-image gimmick activation port against SCUS_944.23.

Usage: .venv/Scripts/python.exe tools/mips/check_gimmick.py [--coverage] [--mutate] [--trapv]
"""
import argparse
import ctypes as C
import gc
import os
from pathlib import Path
import random
import shutil
import struct
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

FUNCS = {
    "GimmickActivate": (0x8008FC30, 1536, "s", True, "su"),
    "GimmickDeactivateChain": (0x80090230, 320, "s", False, "u"),
    "SwingDoorSetPivot": (0x8008ECFC, 612, "s", False, "usi"),
}
EXTRA_COVERAGE = {}
HOOKS = [
    ("PlaySfx", 0x800877F8, 1),
    ("PlaySfxAt", 0x80087A20, 2),
    ("PropSetMesh", 0x80090E54, 4),
    ("FUN_801414a0", 0x801414A0, 1),
]
HOOK_COUNTS = {name: count for name, _, count in HOOKS}
HOOK_TAGS = {name: (i + 1) * 0x1021 for i, (name, _, _) in enumerate(HOOKS)}
HOOK_MARKER = 0x800B1000
STACK_LO = 0x1FF000

GIMMICKS = 0x801B0000
COLLIDERS = 0x801C0000
PROPS = 0x801D0000
SYNTH_COUNT = 8
REAL_AREAS = (0x0E, 0x29, 0x41, 0x52, 0x66)

HookFn = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, *([C.c_uint32] * 4))


class CHooks(C.Structure):
    _fields_ = [("user", C.c_void_p)] + [(name, HookFn) for name, _, _ in HOOKS]


def build(source=None, trapv=False, out_dir=None, tag="gimmick"):
    out_dir = Path(out_dir or ROOT / "extract/mips")
    out_dir.mkdir(parents=True, exist_ok=True)
    dll = out_dir / f"{tag}_{os.getpid()}_{time.time_ns()}.dll"
    gcc = shutil.which("gcc")
    if not gcc:
        raise SystemExit("gcc was not found on PATH")
    subprocess.run([
        gcc, "-std=c11", "-O1" if trapv else "-O2", *(["-ftrapv"] if trapv else []),
        "-Wall", "-Wextra", "-Werror", "-shared", "-static-libgcc",
        "-I" + str(ROOT / "src/world"), "-I" + str(ROOT / "src/core"),
        str(ROOT / "src/core/ae_ram.c"), str(source or ROOT / "src/world/ae_gimmick.c"),
        "-o", str(dll),
    ], check=True)
    return dll


class Side:
    """One C implementation and its AeRam image."""

    def __init__(self, dll):
        self.lib = lib = C.CDLL(str(dll))
        P, U, I = C.c_void_p, C.c_uint32, C.c_int32
        lib.ae_ram_create.restype = P
        lib.ae_ptr.argtypes = [P, U, U]
        lib.ae_ptr.restype = P
        lib.ae_ram_clear_error.argtypes = [P]
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_failed.restype = C.c_int
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        lib.ae_rd16.argtypes = [P, U]
        lib.ae_rd16.restype = U
        lib.ae_rd32.argtypes = [P, U]
        lib.ae_rd32.restype = U
        lib.ae_wr32.argtypes = [P, U, U]
        lib.ae_GimmickActivate.argtypes = [P, P, I, U]
        lib.ae_GimmickActivate.restype = I
        lib.ae_GimmickDeactivateChain.argtypes = [P, U]
        lib.ae_GimmickDeactivateChain.restype = I
        lib.ae_SwingDoorSetPivot.argtypes = [P, U, I, I]
        lib.ae_SwingDoorSetPivot.restype = I

        self.ram = lib.ae_ram_create()
        if not self.ram:
            raise SystemExit("ae_ram_create failed")
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(
            lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scr = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(
            lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.hooks = CHooks()
        self.model = None
        self.callbacks = {}
        for name, _, _ in HOOKS:
            callback = HookFn(lambda user, ram, a0, a1, a2, a3, n=name:
                              self.model.hook(n, a0, a1, a2, a3))
            self.callbacks[name] = callback
            setattr(self.hooks, name, callback)

    def sync_from(self, m):
        C.memmove(self.main, bytes(m.ram), mr.RAM_SIZE)
        C.memmove(self.scr, bytes(m.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)

    def read16(self, address):
        return self.lib.ae_rd16(self.ram, address)

    def read32(self, address):
        return self.lib.ae_rd32(self.ram, address)

    def write32(self, address, value):
        self.lib.ae_wr32(self.ram, address, value & 0xFFFFFFFF)

    def call(self, name, args, hooks=None):
        f = getattr(self.lib, "ae_" + name)
        if name == "GimmickActivate":
            return f(self.ram, C.byref(hooks if hooks is not None else self.hooks), *args)
        return f(self.ram, *args)


class MipsMemory:
    def __init__(self, m):
        self.m = m

    def read16(self, address):
        return self.m.read16(address)

    def read32(self, address):
        return self.m.read32(address)

    def write32(self, address, value):
        self.m.write32(address, value)


class SideMemory:
    def __init__(self, side):
        self.side = side

    def read16(self, address):
        return self.side.read16(address)

    def read32(self, address):
        return self.side.read32(address)

    def write32(self, address, value):
        self.side.write32(address, value)


class HookModel:
    """Deterministic sound / render / overlay stubs shared by both executions."""

    def __init__(self, mem):
        self.mem = mem
        self.log = []

    def hook(self, name, a0, a1, a2, a3):
        words = [a0 & 0xFFFFFFFF, a1 & 0xFFFFFFFF, a2 & 0xFFFFFFFF, a3 & 0xFFFFFFFF]
        args = words[:HOOK_COUNTS[name]]
        entry = [name, *args]
        if name == "PlaySfxAt":
            entry.append(tuple(self.mem.read16(a1 + 2 * k) for k in range(3)))
        self.log.append(tuple(entry))
        marker = self.mem.read32(HOOK_MARKER)
        mixed = sum((word * (i + 1)) & 0xFFFFFFFF for i, word in enumerate(args))
        self.mem.write32(HOOK_MARKER, (marker + mixed + HOOK_TAGS[name]) & 0xFFFFFFFF)
        return 0


class GimmickHarness:
    def __init__(self, side, with_coverage=True):
        self.funcs = FUNCS
        self.m = mr.MipsRam.from_extract()
        self.m.coverage = set() if with_coverage else None
        self.base = self.m.snapshot()
        self.side = side
        self.stack_lo = STACK_LO
        self.counts = {}
        self.failures = 0
        self.stop_after = 1
        reg.declare("stack_window", "801FF000..80200000")

    def failure(self, name, label, problems, *, intentional=False):
        if intentional:
            return False
        self.failures += 1
        print(f"FAIL {name} [{label}]")
        for problem in problems:
            print("   ", problem)
        return False

    def compare(self, name, label, mret, cret, mm, cm, side=None, expected=None,
                require_error=False, intentional=False):
        side = side or self.side
        problems = []
        if require_error:
            if not side.lib.ae_ram_failed(side.ram):
                problems.append("missing hook did not set AeRam's error flag")
            elif b"not provided" not in side.lib.ae_ram_error(side.ram):
                problems.append("missing hook error did not identify the missing callback")
        elif side.lib.ae_ram_failed(side.ram):
            problems.append("C error flag: " + side.lib.ae_ram_error(side.ram).decode())

        if expected is None:
            if mret is not None and (mret & 0xFFFFFFFF) != (cret & 0xFFFFFFFF):
                problems.append(f"return MIPS {mret & 0xFFFFFFFF:08X} C {cret & 0xFFFFFFFF:08X}")
            if mm.log != cm.log:
                problems.append(f"hook log differs\n      MIPS {mm.log}\n      C    {cm.log}")
            mram = bytes(self.m.ram[:STACK_LO])
            cram = bytes(side.main[:STACK_LO])
            if mram != cram:
                diffs = [i for i, (a, b) in enumerate(zip(mram, cram)) if a != b]
                problems.append(f"RAM differs ({len(diffs)} bytes) at " + ", ".join(
                    f"{0x80000000 + i:08X}(mips {mram[i]:02X} c {cram[i]:02X})" for i in diffs[:8]))
            if bytes(self.m.scratch) != bytes(side.scr):
                diffs = [i for i, (a, b) in enumerate(zip(self.m.scratch, side.scr)) if a != b]
                problems.append("scratchpad differs at " + ", ".join(
                    f"{mr.SCRATCH_BASE + i:08X}(mips {self.m.scratch[i]:02X} c {side.scr[i]:02X})" for i in diffs[:8]))
        else:
            expected_ram, expected_scr, expected_log = expected
            cram = bytes(side.main[:STACK_LO])
            if cram != expected_ram:
                diffs = [i for i, (a, b) in enumerate(zip(expected_ram, cram)) if a != b]
                problems.append("missing hook continued RAM writes at " + ", ".join(
                    f"{0x80000000 + i:08X}(checkpoint {expected_ram[i]:02X} c {cram[i]:02X})" for i in diffs[:8]))
            if bytes(side.scr) != expected_scr:
                problems.append("missing hook continued scratchpad writes")
            if cm.log != expected_log:
                problems.append(f"missing hook changed callback log\n      checkpoint {expected_log}\n      C {cm.log}")

        if problems:
            return self.failure(name, label, problems, intentional=intentional)
        return True

    def run(self, name, args, setup, *, side=None, label="", verify_missing=False,
            count=True, intentional=False):
        side = side or self.side
        self.m.restore(self.base)
        setup(self.m)
        initial = self.m.snapshot()
        side.sync_from(self.m)
        mm = HookModel(MipsMemory(self.m))
        cm = HookModel(SideMemory(side))
        side.model = cm
        checkpoints = {}

        def mips_hook(name_, registers):
            if name_ not in checkpoints:
                checkpoints[name_] = (bytes(self.m.ram[:STACK_LO]), bytes(self.m.scratch), list(mm.log))
            return mm.hook(name_, *registers[4:8])

        original_hooks = {address: (lambda cpu, registers, n=name_: mips_hook(n, registers))
                          for name_, address, _ in HOOKS}
        self.m.hook_log.clear()
        mips_error = None
        try:
            registers = self.m.call(FUNCS[name][0], args, hooks=original_hooks)
            mret = registers[2]
        except mr.MipsError as error:
            mret, mips_error = None, str(error)
        cret = side.call(name, args)
        if count:
            self.counts[name] = self.counts.get(name, 0) + 1

        if mips_error:
            okay = self.failure(name, label, ["MIPS harness error: " + mips_error], intentional=intentional)
            if not okay:
                return False
        okay = self.compare(name, label or str(args), mret, cret or 0, mm, cm, side=side,
                            intentional=intentional)
        if not verify_missing or not mm.log:
            return okay

        for missing in sorted({entry[0] for entry in mm.log}):
            expected = checkpoints[missing]
            self.m.restore(initial)
            side.sync_from(self.m)
            side.model = HookModel(SideMemory(side))
            null_callback = HookFn()
            setattr(side.hooks, missing, null_callback)
            side.call(name, args)
            setattr(side.hooks, missing, side.callbacks[missing])
            okay = self.compare(name, f"NULL {missing}", None, 0, mm, side.model, side=side,
                                expected=expected, require_error=True, intentional=intentional) and okay
        return okay

    def finish(self, extra=None, default_run=False, show_coverage=False):
        summary = None
        if self.m.coverage is not None:
            items = [(name, spec[0], spec[1], True) for name, spec in self.funcs.items()]
            items += [(name, addr, size, False) for name, (addr, size) in (extra or {}).items()]
            summary = reg.coverage_summary(items, self.m.coverage)
            if show_coverage:
                reg.print_coverage(summary)
        if default_run:
            reg.write_results(self.counts, summary, not self.failures,
                              function_names=list(self.funcs))
        return summary


def make_synthetic(m, cfg=None, seed=94423):
    """Build a small backed gimmick/collider/prop set, including unused bytes with varied values."""
    cfg = cfg or {}
    rng = random.Random(seed)
    m.write32(0x800F4570, GIMMICKS)
    m.write32(0x800F455C, COLLIDERS)
    m.write32(0x800B4D64, PROPS)
    m.write16(0x800F4474, cfg.get("level", 1))
    m.write16(0x800F4476, cfg.get("area", 1))

    for index in range(SYNTH_COUNT):
        gimmick = GIMMICKS + index * 0x18
        m.write_bytes(gimmick, rng.randbytes(0x18))
        m.write16(gimmick + 0x00, 0)
        m.write16(gimmick + 0x02, 0)
        m.write16(gimmick + 0x04, index)
        m.write16(gimmick + 0x06, 0)
        m.write16(gimmick + 0x08, 0)
        m.write16(gimmick + 0x0A, 0)
        m.write16(gimmick + 0x0C, -1)
        m.write16(gimmick + 0x0E, -1)
        m.write16(gimmick + 0x10, -1)
        m.write16(gimmick + 0x12, -1)
        m.write16(gimmick + 0x14, -1)
        m.write16(gimmick + 0x16, -1)

        collider = COLLIDERS + index * 0x38
        m.write_bytes(collider, rng.randbytes(0x38))
        m.write16(collider + 0x00, 101 + index * 17)
        m.write16(collider + 0x02, -203 - index * 11)
        m.write16(collider + 0x04, 307 + index * 13)
        m.write16(collider + 0x06, 19 + index)
        m.write16(collider + 0x08, 23 + index)
        m.write16(collider + 0x0A, 31 + index * 2)
        m.write16(collider + 0x0E, index)
        m.write16(collider + 0x10, 0xABCD)
        m.write16(collider + 0x12, 0xA50F)
        m.write16(collider + 0x14, 0x1357)
        m.write16(collider + 0x18, 0xD5A3)

        prop = PROPS + index * 0x20
        m.write_bytes(prop, rng.randbytes(0x20))
        m.write16(prop + 0x16, 0x1234 + index)

    target_index = cfg.get("target_index", 1)
    source_index = cfg.get("source_index", 0 if target_index != 0 else 1)
    target = GIMMICKS + target_index * 0x18
    source = GIMMICKS + source_index * 0x18
    m.write16(target + 0x00, cfg.get("type", 2))
    m.write16(target + 0x02, cfg.get("state", 0))
    m.write16(target + 0x04, cfg.get("collider_index", target_index))
    m.write16(target + 0x08, cfg.get("subtype", 0))
    m.write16(target + 0x0A, cfg.get("param_a", 0))
    m.write16(target + 0x0C, cfg.get("link", -1))
    m.write16(target + 0x0E, cfg.get("param_e", 4))
    m.write16(target + 0x10, cfg.get("param_10", 5))
    m.write16(target + 0x12, cfg.get("param_12", 6))
    m.write16(source + 0x00, cfg.get("source_type", 2))
    m.write16(source + 0x02, cfg.get("source_state", 0))

    collider_index = cfg.get("collider_index", target_index)
    collider = COLLIDERS + collider_index * 0x38
    if "prop_index" in cfg:
        m.write16(collider + 0x0E, cfg["prop_index"])
    if "collider_flags" in cfg:
        m.write16(collider + 0x18, cfg["collider_flags"])
    if "collider_type" in cfg:
        m.write16(collider + 0x12, cfg["collider_type"])
    if "following_state" in cfg:
        m.write16(GIMMICKS + (target_index + 1) * 0x18 + 0x02, cfg["following_state"])

    for index in cfg.get("child_types", {}):
        record = GIMMICKS + index * 0x18
        values = cfg["child_types"][index]
        m.write16(record + 0x00, values.get("type", 0))
        m.write16(record + 0x02, values.get("state", 0))
        m.write16(record + 0x08, values.get("subtype", 0))
        m.write16(record + 0x0C, values.get("link", -1))
        m.write16(record + 0x0E, values.get("param_e", -1))
        m.write16(record + 0x10, values.get("param_10", -1))
        m.write16(record + 0x12, values.get("param_12", -1))
    return source


def run_activate(h, cfg, *, arg_index=None, side=None, label="", verify_missing=False,
                 count=True, intentional=False):
    cfg = dict(cfg)
    target_index = cfg.get("target_index", 1)
    source_index = cfg.get("source_index", 0 if target_index != 0 else 1)
    raw_index = target_index if arg_index is None else arg_index
    source = GIMMICKS + source_index * 0x18
    return h.run("GimmickActivate", (raw_index, source),
                 lambda m: make_synthetic(m, cfg), side=side, label=label,
                 verify_missing=verify_missing, count=count, intentional=intentional)


def run_deactivate(h, cfg=None, *, side=None, label="", count=True, intentional=False):
    cfg = dict(cfg or {})
    target_index = cfg.get("target_index", 1)

    def setup(m):
        make_synthetic(m, cfg)
        target = GIMMICKS + target_index * 0x18
        for field, off in (("child_e", 0x0E), ("child_10", 0x10), ("child_12", 0x12)):
            m.write16(target + off, cfg.get(field, -1))
        for index, values in cfg.get("links", {}).items():
            record = GIMMICKS + index * 0x18
            m.write16(record + 0x04, values.get("collider_index", index))
            m.write16(record + 0x0E, values.get("child_e", -1))
            m.write16(record + 0x10, values.get("child_10", -1))
            m.write16(record + 0x12, values.get("child_12", -1))
            collider = COLLIDERS + values.get("collider_index", index) * 0x38
            m.write16(collider + 0x0E, values.get("prop_index", index))
            m.write16(collider + 0x00, values.get("x", 200 + index))
            m.write16(collider + 0x02, values.get("y", 300 + index))
            m.write16(collider + 0x04, values.get("z", 400 + index))
        collider_index = cfg.get("collider_index", target_index)
        collider = COLLIDERS + collider_index * 0x38
        if "prop_index" in cfg:
            m.write16(collider + 0x0E, cfg["prop_index"])
        if "collider_flags" in cfg:
            m.write16(collider + 0x18, cfg["collider_flags"])
        m.write16(target + 0x04, collider_index)
        m.write16(collider + 0x00, cfg.get("x", 111))
        m.write16(collider + 0x02, cfg.get("y", -222))
        m.write16(collider + 0x04, cfg.get("z", 333))

    return h.run("GimmickDeactivateChain", (GIMMICKS + target_index * 0x18,), setup,
                 side=side, label=label, count=count, intentional=intentional)


def run_pivot(h, direction, mode=2, *, values=None, side=None, label="", count=True, intentional=False):
    values = values or {}

    def setup(m):
        make_synthetic(m, seed=values.get("seed", 94423))
        collider = COLLIDERS + 1 * 0x38
        for off, key, default in ((0x00, "x", 111), (0x02, "y", -222), (0x04, "z", 333),
                                  (0x06, "half_x", 19), (0x08, "half_y", 23),
                                  (0x0A, "half_z", 37), (0x18, "flags", 0xFFFF)):
            m.write16(collider + off, values.get(key, default))

    return h.run("SwingDoorSetPivot", (COLLIDERS + 0x38, direction, mode), setup,
                 side=side, label=label, count=count, intentional=intentional)


def area_layout(area):
    member = dump_areas.area_members()[area]
    blob_path = ROOT / "extract/members" / f"{member:03X}.bin"
    blob = blob_path.read_bytes()
    u32 = lambda offset: struct.unpack_from("<I", blob, offset)[0]
    s16 = lambda offset: struct.unpack_from("<h", blob, offset)[0]
    area_header = u32(0x4D0)
    gimmick_table = u32(0x4D4)
    group_count = u32(gimmick_table)
    gimmick_records = gimmick_table + 4 + 8 * group_count
    gimmick_count = sum(s16(gimmick_table + 4 + 8 * i) for i in range(group_count))
    collision = area_header + 4 * u32(area_header + 0x10)
    colliders = collision + u32(collision + 8) + 2
    if gimmick_count < 0 or gimmick_count > 512:
        raise SystemExit(f"area {area:02X}: implausible gimmick count {gimmick_count}")
    return member, blob, gimmick_records, gimmick_count, colliders


def run_real_area_object(h, area, index, *, label=""):
    member, blob, gimmick_records, count, colliders = area_layout(area)
    target = mr.AREA_BASE + gimmick_records + index * 0x18

    def setup(m):
        m.load_area(blob)
        m.write32(0x800F4570, mr.AREA_BASE + gimmick_records)
        m.write32(0x800F455C, mr.AREA_BASE + colliders)
        m.write32(0x800B4D64, PROPS)
        m.write16(0x800F4474, 0)
        m.write16(0x800F4476, area)
        for prop_index in range(128):
            prop = PROPS + prop_index * 0x20
            m.write_bytes(prop, bytes((prop_index * 13 + byte * 7) & 0xFF for byte in range(0x20)))
            m.write16(prop + 0x16, 0x0357 + prop_index)

    return h.run("GimmickActivate", (index, target), setup, label=label or f"area {area:02X} gimmick {index}")


def baseline_scenarios(h, scale, seed):
    rng = random.Random(seed)
    # Every table slot from type 2 through 0x65 is dispatched at least once.
    for gimmick_type in range(2, 102):
        cfg = {"type": gimmick_type, "state": 0, "subtype": 46, "param_a": 2,
               "link": 2, "area": 0x10, "level": 6,
               "child_types": {2: {"type": 0, "state": 0}}}
        if gimmick_type in (6, 10):
            cfg.update(child_e=2, child_10=3, child_12=4)
        if gimmick_type == 30:
            cfg.update(subtype=2, param_a=3)
        if gimmick_type == 36:
            cfg.update(following_state=0)
        if gimmick_type == 101:
            cfg.update(state=1, subtype=0, source_state=1)
        run_activate(h, cfg, label=f"dispatch type {gimmick_type:02X}")

    # Branch and state transitions not represented by a single table-dispatch case.
    for subtype, state in ((45, 0), (46, 0), (49, 0), (50, 0), (46, 2), (46, 1)):
        run_activate(h, {"type": 2, "subtype": subtype, "state": state, "param_e": 3,
                         "param_12": 6}, verify_missing=(subtype == 46 and state == 0),
                     label=f"type 02 subtype {subtype} state {state}")
    for gimmick_type in (3, 20):
        for state in (0, 1, 2):
            run_activate(h, {"type": gimmick_type, "state": state}, label=f"type {gimmick_type:02X} state {state}")

    for area, param_a in ((0x10, 0), (0x10, 2), (1, 0xE)):
        run_activate(h, {"type": 5, "state": 0, "param_a": param_a, "link": 2, "area": area,
                         "child_types": {2: {"type": 0}}}, verify_missing=(area == 0x10 and param_a == 2),
                     label=f"type 05 area {area:02X} mode {param_a}")
    for level, area in ((6, 1), (0, 0x65), (0, 1)):
        run_activate(h, {"type": 22, "level": level, "area": area, "prop_index": 1},
                     verify_missing=(level == 6), label=f"type 16 level {level} area {area:02X}")

    for state in (0, 1):
        for area in (0x15, 1):
            run_activate(h, {"type": 26, "state": state, "source_state": state, "area": area},
                         verify_missing=(state == 0 and area == 0x15), label=f"type 1A source {state} area {area:02X}")
            run_activate(h, {"type": 29, "state": state, "source_state": state, "area": area},
                         label=f"type 1D source {state} area {area:02X}")

    for source_state, source_type in ((0, 2), (1, 0x1B), (0, 0x1B), (1, 2)):
        child = {2: {"type": 2, "subtype": 46, "state": 0}}
        run_activate(h, {"type": 27, "source_state": source_state, "source_type": source_type,
                         "link": 2, "child_types": child}, verify_missing=(source_state == 0 and source_type == 2),
                     label=f"type 1B source state {source_state} type {source_type:02X}")
    for source_state in (0, 1):
        run_activate(h, {"type": 28, "source_state": source_state, "param_e": 3, "param_10": 6},
                     verify_missing=(source_state == 0), label=f"type 1C source state {source_state}")

    for source_state, target_state in ((0, 0), (0, 1), (1, 0), (1, 1)):
        run_activate(h, {"type": 31, "source_state": source_state, "state": target_state},
                     label=f"type 1F source/target state {source_state}/{target_state}")
    for source_state, target_state, following_state in ((0, 0, 0), (0, 1, 1), (1, 0, 0)):
        run_activate(h, {"type": 36, "source_state": source_state, "state": target_state,
                         "following_state": following_state}, label="type 24 paired state")

    for gimmick_type in (38, 39):
        for state in (1, 4, 0):
            run_activate(h, {"type": gimmick_type, "state": state}, label=f"type {gimmick_type:02X} state {state}")
    for source_state, source_type in ((0, 0x1B), (1, 0x1B), (0, 2), (1, 2)):
        run_activate(h, {"type": 40, "source_state": source_state, "source_type": source_type,
                         "link": 2, "child_types": {2: {"type": 0}}}, label="type 28 source condition")
    for state in (0, 1):
        run_activate(h, {"type": 41, "state": state}, verify_missing=(state == 0),
                     label=f"type 29 state {state}")

    # Type 0x65 is the hinged-door activation path; all of its states and unsigned direction bounds matter.
    for state, direction, source_state in ((0, 0, 0), (1, 0, 0), (1, 8, 1), (1, 8, 0),
                                           (2, 0, 0), (2, 8, 0), (2, 8, 1),
                                           (4, 0, 0), (4, 8, 0), (4, 8, 1),
                                           (5, 0, 0), (5, 8, 5), (5, 8, 1), (3, 0, 0)):
        run_activate(h, {"type": 101, "state": state, "subtype": direction,
                         "source_state": source_state}, label=f"type 65 state {state} dir {direction} source {source_state}")

    # A real table sweep uses several areas with doors, platforms, switches and grouped gimmicks.
    for area in REAL_AREAS:
        member, blob, records, count, _ = area_layout(area)
        types = sorted({struct.unpack_from("<H", blob, records + 0x18 * i)[0] for i in range(count)})
        print(f"real area {area:02X}: member {member:03X}, {count} records, types " +
              ",".join(f"{kind:02X}" for kind in types))
        for index in range(count):
            run_real_area_object(h, area, index)

    # Randomised supported types and field words; all pointers and records stay backed.
    random_types = (2, 3, 5, 6, 7, 10, 20, 22, 25, 26, 27, 28, 29, 30, 31, 36, 38, 39, 40, 41, 44, 101)
    for i in range(max(8, round(32 * scale))):
        kind = rng.choice(random_types)
        cfg = {"type": kind, "state": rng.choice([-1, 0, 1, 2, 3, 4, 5]),
               "subtype": rng.choice([-1, 0, 2, 7, 8, 45, 46, 49, 50, 0x7FFF, 0x8000]),
               "param_a": rng.choice([-1, 0, 2, 8, 0xE, 0x7FFF]),
               "source_state": rng.choice([-1, 0, 1, 2, 5]),
               "source_type": rng.choice([0, 2, 0x1B, 0x1C]),
               "link": rng.choice([-1, 2]), "area": rng.choice([1, 0x10, 0x15, 0x65]),
               "level": rng.choice([0, 6, 0x7FFF]),
               "child_types": {2: {"type": 0}}}
        if kind in (6, 10):
            cfg.update(child_e=2, child_10=3, child_12=4)
        if kind == 30:
            cfg.update(subtype=2, param_a=3)
        run_activate(h, cfg, label=f"random {i} type {kind:02X}")

    # The recursive deactivation helper's three links, a nested link, empty collider, and no prop.
    run_deactivate(h, {"collider_index": -1}, label="deactivate without collider")
    run_deactivate(h, {"prop_index": -1}, label="deactivate without prop")
    run_deactivate(h, {"child_e": 2, "child_10": 3, "child_12": 4,
                       "links": {2: {"child_e": 5, "x": -91},
                                3: {"prop_index": -1, "x": 32},
                                4: {"child_12": 5, "z": -73},
                                5: {"collider_index": -1}}}, label="three-link nested chain")
    run_deactivate(h, {"child_e": 2, "child_10": -1, "child_12": -1,
                       "links": {2: {"prop_index": -1}}}, label="one linked record")

    # Every pivot direction is exercised in both relevant mode classes, plus signed / narrowed edges.
    for direction in range(8):
        run_pivot(h, direction, 2, values={"x": 123, "z": -456, "half_x": 17, "half_z": 29},
                  label=f"pivot direction {direction} mode 2")
        run_pivot(h, direction, 5, values={"x": -123, "z": 456, "half_x": 21, "half_z": 35},
                  label=f"pivot direction {direction} mode 5")
    for direction in (-32768, -1, 8, 0x10001):
        run_pivot(h, direction, 2, label=f"pivot direction edge {direction}")
    for values in ((32767, -32768, 32767, -32768), (-32768, 32767, -32768, 32767)):
        x, z, half_x, half_z = values
        run_pivot(h, 6, 2, values={"x": x, "z": z, "half_x": half_x, "half_z": half_z},
                  label="pivot s16 edge arithmetic")


def mutation_sources():
    """Small, function-scoped mistakes. Each probe below must fail against the original instructions."""
    return [
        # GimmickActivate: bounds, dispatch behavior, field widths, hook calls, and recursion.
        ("activate signed index", "GimmickActivate", "int32_t target_index = gimmick_s16((uint32_t)index);", "int32_t target_index = index;", {"type": 2, "subtype": 46}, 0x10001),
        ("activate rejects zero", "GimmickActivate", "if (target_index < 0) return 0;", "if (target_index <= 0) return 0;", {"type": 2, "subtype": 46, "target_index": 0}, 0),
        ("activate type upper bound", "GimmickActivate", "if (type < 2 || type > 101) return 0;", "if (type < 2 || type >= 101) return 0;", {"type": 101, "state": 1, "subtype": 0, "source_state": 0}, None),
        ("type 02 lower subtype", "GimmickActivate", "if (subtype < 46 || subtype > 49) return 0;", "if (subtype < 47 || subtype > 49) return 0;", {"type": 2, "subtype": 46}, None),
        ("type 02 upper subtype", "GimmickActivate", "if (subtype < 46 || subtype > 49) return 0;", "if (subtype < 46 || subtype >= 49) return 0;", {"type": 2, "subtype": 49}, None),
        ("type 02 sound id", "GimmickActivate", "GIMMICK_CALL(PlaySfxAt, 0x75u, collider, 0, 0);", "GIMMICK_CALL(PlaySfxAt, 0x76u, collider, 0, 0);", {"type": 2, "subtype": 46}, None),
        ("type 02 opening mesh source", "GimmickActivate", "mesh_index = ae_rds16(r, target + AE_GIMMICK_PARAM_12);", "mesh_index = ae_rds16(r, target + AE_GIMMICK_PARAM_E);", {"type": 2, "subtype": 46, "state": 0, "param_e": 3, "param_12": 6}, None),
        ("type 02 closing state", "GimmickActivate", "ae_wr16(r, target + AE_GIMMICK_STATE, 0);", "ae_wr16(r, target + AE_GIMMICK_STATE, 1);", {"type": 2, "subtype": 46, "state": 2}, None),
        ("type 03 first transition", "GimmickActivate", "if (state == 0) ae_wr16(r, target + AE_GIMMICK_STATE, 1);", "if (state == 0) ae_wr16(r, target + AE_GIMMICK_STATE, 2);", {"type": 3, "state": 0}, None),
        ("type 03 second transition", "GimmickActivate", "else if (state == 2) ae_wr16(r, target + AE_GIMMICK_STATE, 3);", "else if (state == 1) ae_wr16(r, target + AE_GIMMICK_STATE, 3);", {"type": 3, "state": 2}, None),
        ("type 03 collider disable", "GimmickActivate", "| AE_COLLIDER_DISABLED);\n        return 0;\n    }\n\n    case 5", "& ~AE_COLLIDER_DISABLED);\n        return 0;\n    }\n\n    case 5", {"type": 3, "state": 0, "collider_flags": 0x8123}, None),
        ("type 05 collider mode", "GimmickActivate", "ae_wr16(r, collider + AE_COLLIDER_MODE, (uint32_t)mode);", "ae_wr16(r, collider + AE_COLLIDER_TYPE, (uint32_t)mode);", {"type": 5, "param_a": 2, "link": 2}, None),
        ("type 05 mode bits", "GimmickActivate", "((ae_rd16(r, target + AE_GIMMICK_PARAM_A) & 0x000Eu) ? 0x0400u : 0x0500u)", "((ae_rd16(r, target + AE_GIMMICK_PARAM_A) & 0x000Eu) ? 0x0500u : 0x0400u)", {"type": 5, "param_a": 2, "link": 2}, None),
        ("type 05 recursion source", "GimmickActivate", "int32_t link = ae_rds16(r, target + AE_GIMMICK_LINK);\n        ae_GimmickActivate(r, h, link, target);\n        if (ae_ram_failed(r)) return 0;\n        ae_wr16(r, target + AE_GIMMICK_STATE, 2);", "int32_t link = ae_rds16(r, target + AE_GIMMICK_LINK);\n        ae_GimmickActivate(r, h, link, source_gimmick);\n        if (ae_ram_failed(r)) return 0;\n        ae_wr16(r, target + AE_GIMMICK_STATE, 2);", {"type": 5, "param_a": 2, "link": 2, "child_types": {2: {"type": 27, "link": -1}}}, None),
        ("type 05 state", "GimmickActivate", "ae_wr16(r, target + AE_GIMMICK_STATE, 2);\n\n        if (ae_rd16(r, AE_G_CURRENT_AREA)", "ae_wr16(r, target + AE_GIMMICK_STATE, 3);\n\n        if (ae_rd16(r, AE_G_CURRENT_AREA)", {"type": 5, "param_a": 2, "link": 2}, None),
        ("type 05 area sound gate", "GimmickActivate", "ae_rd16(r, AE_G_CURRENT_AREA) == 0x10u", "ae_rd16(r, AE_G_CURRENT_AREA) == 0x11u", {"type": 5, "param_a": 2, "link": 2, "area": 0x10}, None),
        ("type 16 level gate", "GimmickActivate", "ae_rd16(r, AE_G_CURRENT_LEVEL) != 6u", "ae_rd16(r, AE_G_CURRENT_LEVEL) != 7u", {"type": 22, "level": 6, "area": 1}, None),
        ("type 16 sound id", "GimmickActivate", "GIMMICK_CALL(PlaySfxAt, 0xB0u, collider, 0, 0);", "GIMMICK_CALL(PlaySfxAt, 0xAFu, collider, 0, 0);", {"type": 22, "level": 6, "area": 1}, None),
        ("type 16 prop mask", "GimmickActivate", "ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0xFFFFu);", "ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0);", {"type": 22, "level": 6, "area": 1}, None),
        ("type 19 saved y", "GimmickActivate", "target + AE_GIMMICK_PARAM_10, ae_rd16(r, collider + AE_COLLIDER_POS_Y)", "target + AE_GIMMICK_PARAM_10, ae_rd16(r, collider + AE_COLLIDER_POS_X)", {"type": 25}, None),
        ("type 1a source state", "GimmickActivate", "source_state == 0 ? 1u : 2u", "source_state == 0 ? 2u : 1u", {"type": 26, "source_state": 0}, None),
        ("type 1a area sound gate", "GimmickActivate", "ae_rd16(r, AE_G_CURRENT_AREA) == 0x15u", "ae_rd16(r, AE_G_CURRENT_AREA) == 0x14u", {"type": 26, "area": 0x15}, None),
        ("type 1b source type", "GimmickActivate", "source_state == 0 && source_type == 2", "source_state == 0 && source_type == 3", {"type": 27, "source_state": 0, "source_type": 2, "link": 2}, None),
        ("type 1b source state", "GimmickActivate", "source_state == 0 && source_type == 2", "source_state != 0 && source_type == 2", {"type": 27, "source_state": 0, "source_type": 2, "link": 2}, None),
        ("type 1b linked type", "GimmickActivate", "ae_GimmickActivate(r, h, link, target);\n            if (ae_ram_failed(r)) return 0;\n            ae_wr16(r, target + AE_GIMMICK_STATE, 1);", "ae_GimmickActivate(r, h, link + 1, target);\n            if (ae_ram_failed(r)) return 0;\n            ae_wr16(r, target + AE_GIMMICK_STATE, 1);", {"type": 27, "source_state": 0, "source_type": 2, "link": 2, "child_types": {2: {"type": 2, "subtype": 46}}}, None),
        ("type 1c mesh select", "GimmickActivate", "source_state == 0 ? AE_GIMMICK_PARAM_10 : AE_GIMMICK_PARAM_E", "source_state == 0 ? AE_GIMMICK_PARAM_E : AE_GIMMICK_PARAM_10", {"type": 28, "source_state": 0, "param_e": 3, "param_10": 6}, None),
        ("type 1c mesh argument", "GimmickActivate", "GIMMICK_CALL(PropSetMesh, prop_index, mesh_index, mesh_index, 0);\n        return 0;\n    }\n\n    case 30", "GIMMICK_CALL(PropSetMesh, prop_index, mesh_index, 0, 0);\n        return 0;\n    }\n\n    case 30", {"type": 28, "source_state": 0, "param_e": 3, "param_10": 6}, None),
        ("type 1e second link", "GimmickActivate", "target + AE_GIMMICK_PARAM_A);\n        ae_wr16(r, gimmick_record(gimmicks, first_index)", "target + AE_GIMMICK_PARAM_E);\n        ae_wr16(r, gimmick_record(gimmicks, first_index)", {"type": 30, "subtype": 2, "param_a": 3}, None),
        ("type 1f source guard", "GimmickActivate", "case 31: {\n        if (ae_rds16(r, source_gimmick + AE_GIMMICK_STATE) != 0 ||\n            ae_rds16(r, target + AE_GIMMICK_STATE) != 0) return 0;", "case 31: {\n        if (ae_rds16(r, source_gimmick + AE_GIMMICK_STATE) == 0 ||\n            ae_rds16(r, target + AE_GIMMICK_STATE) != 0) return 0;", {"type": 31, "source_state": 0, "state": 0}, None),
        ("type 1f target guard", "GimmickActivate", "case 31: {\n        if (ae_rds16(r, source_gimmick + AE_GIMMICK_STATE) != 0 ||\n            ae_rds16(r, target + AE_GIMMICK_STATE) != 0) return 0;", "case 31: {\n        if (ae_rds16(r, source_gimmick + AE_GIMMICK_STATE) != 0 ||\n            ae_rds16(r, target + AE_GIMMICK_STATE) == 0) return 0;", {"type": 31, "source_state": 0, "state": 0}, None),
        ("type 24 following record", "GimmickActivate", "gimmick_record(gimmicks, target_index + 1)", "gimmick_record(gimmicks, target_index)", {"type": 36, "state": 0, "following_state": 0}, None),
        ("type 24 source guard", "GimmickActivate", "if (ae_rds16(r, source_gimmick + AE_GIMMICK_STATE) != 0) return 0;\n        if (ae_rds16(r, target + AE_GIMMICK_STATE)", "if (ae_rds16(r, source_gimmick + AE_GIMMICK_STATE) == 0) return 0;\n        if (ae_rds16(r, target + AE_GIMMICK_STATE)", {"type": 36, "source_state": 0, "state": 0, "following_state": 0}, None),
        ("type 26 door state", "GimmickActivate", "if (state == 4) ae_wr16(r, target + AE_GIMMICK_STATE, 5);", "if (state == 4) ae_wr16(r, target + AE_GIMMICK_STATE, 4);", {"type": 38, "state": 4}, None),
        ("type 26 door flag", "GimmickActivate", "ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);\n        return 0;\n    }\n\n    case 40", "ae_rd16(r, collider + AE_COLLIDER_FLAGS) & ~AE_COLLIDER_DISABLED);\n        return 0;\n    }\n\n    case 40", {"type": 38, "state": 4, "collider_flags": 0x8123}, None),
        ("type 28 target state gate", "GimmickActivate", "case 40: {\n        if (ae_rds16(r, source_gimmick + AE_GIMMICK_STATE) != 0 ||\n            ae_rds16(r, target + AE_GIMMICK_STATE) != 0) return 0;", "case 40: {\n        if (ae_rds16(r, source_gimmick + AE_GIMMICK_STATE) != 0 ||\n            ae_rds16(r, target + AE_GIMMICK_STATE) == 0) return 0;", {"type": 40, "state": 0, "source_state": 0, "link": 2}, None),
        ("type 29 overlay gate", "GimmickActivate", "if (ae_rds16(r, target + AE_GIMMICK_STATE) != 0) return 0;", "if (ae_rds16(r, target + AE_GIMMICK_STATE) == 0) return 0;", {"type": 41, "state": 0}, None),
        ("type 2c state", "GimmickActivate", "case 44:\n        ae_wr16(r, target + AE_GIMMICK_STATE, 2);", "case 44:\n        ae_wr16(r, target + AE_GIMMICK_STATE, 1);", {"type": 44}, None),
        ("type 65 unsigned direction", "GimmickActivate", "if (state == 1) {\n            if (direction < 8) {", "if (state == 1) {\n            if (direction <= 8) {", {"type": 101, "state": 1, "subtype": 8, "source_state": 1}, None),
        ("type 65 pivot mode", "GimmickActivate", "ae_SwingDoorSetPivot(r, collider, (int32_t)direction, 5);", "ae_SwingDoorSetPivot(r, collider, (int32_t)direction, 2);", {"type": 101, "state": 2, "subtype": 0}, None),
        ("type 65 state 4", "GimmickActivate", "ae_wr16(r, target + AE_GIMMICK_STATE, 5);\n                ae_wr16(r, collider + AE_COLLIDER_FLAGS,", "ae_wr16(r, target + AE_GIMMICK_STATE, 4);\n                ae_wr16(r, collider + AE_COLLIDER_FLAGS,", {"type": 101, "state": 4, "subtype": 0}, None),
        ("type 65 state 2 source", "GimmickActivate", "if (state == 2) {\n            if (direction < 8) {\n                ae_wr16(r, collider + AE_COLLIDER_FLAGS,\n                        ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);\n                ae_wr16(r, target + AE_GIMMICK_STATE, 5);\n                ae_SwingDoorSetPivot(r, collider, (int32_t)direction, 5);\n                return 0;\n            }\n            if (source_state == 0)", "if (state == 2) {\n            if (direction < 8) {\n                ae_wr16(r, collider + AE_COLLIDER_FLAGS,\n                        ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);\n                ae_wr16(r, target + AE_GIMMICK_STATE, 5);\n                ae_SwingDoorSetPivot(r, collider, (int32_t)direction, 5);\n                return 0;\n            }\n            if (source_state != 0)", {"type": 101, "state": 2, "subtype": 8, "source_state": 0}, None),

        # GimmickDeactivateChain: invalid collider, prop, flags, links, and saved position.
        ("deactivate collider guard", "GimmickDeactivateChain", "if (collider_index < 0) return 0;", "if (collider_index < -1) return 0;", {"collider_index": -1}, None),
        ("deactivate state", "GimmickDeactivateChain", "ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);", "ae_wr16(r, gimmick + AE_GIMMICK_STATE, 2);", {}, None),
        ("deactivate prop bound", "GimmickDeactivateChain", "if (prop_index >= 0) {", "if (prop_index > 0) {", {"prop_index": 0}, None),
        ("deactivate prop mask", "GimmickDeactivateChain", "ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0xFFFFu);", "ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0);", {}, None),
        ("deactivate collider state bits", "GimmickDeactivateChain", "(ae_rd16(r, collider + AE_COLLIDER_FLAGS) & ~AE_COLLIDER_STATE_MASK) |", "(ae_rd16(r, collider + AE_COLLIDER_FLAGS) & 0xFFF8u) |", {"collider_flags": 0xD5AB}, None),
        ("deactivate first link", "GimmickDeactivateChain", "gimmick + AE_GIMMICK_PARAM_E);\n    if (linked_index >= 0) ae_GimmickDeactivateChain", "gimmick + AE_GIMMICK_PARAM_10);\n    if (linked_index >= 0) ae_GimmickDeactivateChain", {"child_e": 2, "child_10": 3, "child_12": 4, "links": {2: {}, 3: {"collider_index": -1}, 4: {}}}, None),
        ("deactivate second link", "GimmickDeactivateChain", "linked_index = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_10);", "linked_index = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12);", {"child_e": 2, "child_10": 3, "child_12": 4, "links": {2: {}, 3: {}, 4: {"collider_index": -1}}}, None),
        ("deactivate third link", "GimmickDeactivateChain", "linked_index = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12);", "linked_index = -1;", {"child_e": 2, "child_10": 3, "child_12": 4, "links": {2: {}, 3: {}, 4: {}}}, None),
        ("deactivate saved x", "GimmickDeactivateChain", "gimmick + AE_GIMMICK_PARAM_E, ae_rd16(r, collider + AE_COLLIDER_POS_X)", "gimmick + AE_GIMMICK_PARAM_E, ae_rd16(r, collider + AE_COLLIDER_POS_Y)", {"child_e": -1, "child_10": -1, "child_12": -1}, None),
        ("deactivate saved z", "GimmickDeactivateChain", "gimmick + AE_GIMMICK_PARAM_12, ae_rd16(r, collider + AE_COLLIDER_POS_Z)", "gimmick + AE_GIMMICK_PARAM_12, ae_rd16(r, collider + AE_COLLIDER_POS_X)", {"child_e": -1, "child_10": -1, "child_12": -1}, None),

        # SwingDoorSetPivot: direction narrowing, both formula modes, flags, and half extents.
        ("pivot signed direction", "SwingDoorSetPivot", "int32_t dir = gimmick_s16((uint32_t)direction);", "int32_t dir = direction;", {"direction": 0x10001, "mode": 2}, None),
        ("pivot direction 7", "SwingDoorSetPivot", "if (dir >= 0 && dir < 8) {", "if (dir >= 0 && dir < 7) {", {"direction": 7, "mode": 2}, None),
        ("pivot clears disable", "SwingDoorSetPivot", "ae_rd16(r, collider + AE_COLLIDER_FLAGS) & ~AE_COLLIDER_DISABLED);", "ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);", {"direction": 0, "mode": 2, "values": {"flags": 0xFFFF}}, None),
        ("pivot mode two", "SwingDoorSetPivot", "if (mode == 2) {", "if (mode == 5) {", {"direction": 1, "mode": 2}, None),
        ("pivot dir 0 x", "SwingDoorSetPivot", "case 0: pivot_x = pos_x - half_z + half_x; pivot_z = pos_z + half_z + half_x; break;", "case 0: pivot_x = pos_x + half_z + half_x; pivot_z = pos_z + half_z + half_x; break;", {"direction": 0, "mode": 5}, None),
        ("pivot dir 1 z", "SwingDoorSetPivot", "case 1: pivot_x = pos_x - half_x + half_z; pivot_z = pos_z - half_x - half_z; break;", "case 1: pivot_x = pos_x - half_x + half_z; pivot_z = pos_z + half_x + half_z; break;", {"direction": 1, "mode": 2}, None),
        ("pivot dir 2 x", "SwingDoorSetPivot", "case 2: pivot_x = pos_x - half_z - half_x; pivot_z = pos_z - half_z + half_x; break;", "case 2: pivot_x = pos_x + half_x + half_z; pivot_z = pos_z - half_x - half_z; break;", {"direction": 2, "mode": 2}, None),
        ("pivot dir 4 z", "SwingDoorSetPivot", "case 4: pivot_x = pos_x + half_z - half_x; pivot_z = pos_z - half_z - half_x; break;", "case 4: pivot_x = pos_x + half_z - half_x; pivot_z = pos_z + half_z + half_x; break;", {"direction": 4, "mode": 5}, None),
        ("pivot z half size", "SwingDoorSetPivot", "int32_t half_z = ae_rds16(r, collider + AE_COLLIDER_HALF_Z);", "int32_t half_z = ae_rds16(r, collider + AE_COLLIDER_HALF_X);", {"direction": 6, "mode": 2}, None),
        ("pivot swaps extents", "SwingDoorSetPivot", "ae_wr16(r, collider + AE_COLLIDER_HALF_X, (uint32_t)half_z);", "ae_wr16(r, collider + AE_COLLIDER_HALF_X, (uint32_t)half_x);", {"direction": 6, "mode": 2}, None),
    ]


def mutate_function(source, function, old, new):
    signature = f"int32_t ae_{function}("
    start = source.index(signature)
    next_function = source.find("\nint32_t ae_", start + len(signature))
    end = next_function if next_function >= 0 else source.find("\n#undef GIMMICK_CALL", start)
    if end < 0:
        end = len(source)
    prefix, body, suffix = source[:start], source[start:end], source[end:]
    count = body.count(old)
    if count != 1:
        raise RuntimeError(f"mutation anchor for {function} matched {count} times: {old!r}")
    return prefix + body.replace(old, new, 1) + suffix


def mutation_probe(h, name, function, cfg, arg_index, side):
    if function == "GimmickActivate":
        return run_activate(h, cfg, arg_index=arg_index, side=side, label=name,
                            count=False, intentional=True)
    if function == "GimmickDeactivateChain":
        return run_deactivate(h, cfg, side=side, label=name, count=False, intentional=True)
    return run_pivot(h, cfg["direction"], cfg["mode"], values=cfg.get("values"), side=side,
                     label=name, count=False, intentional=True)


def run_mutations(h, trapv=False):
    source = (ROOT / "src/world/ae_gimmick.c").read_text(encoding="utf-8")
    variants = mutation_sources()
    mutated_sources = [mutate_function(source, function, old, new)
                       for _, function, old, new, _, _ in variants]
    counts = {name: 0 for name in FUNCS}
    caught = []
    survivors = []
    # A ctypes-loaded DLL can remain mapped by Windows until process exit.  Keep this
    # build-only directory if the platform refuses to remove a mapped mutant image.
    with tempfile.TemporaryDirectory(prefix="gimmick-mutations-", dir=ROOT / "extract/mips",
                                     ignore_cleanup_errors=True) as temp_name:
        temp = Path(temp_name)
        h.m.coverage = None
        for index, ((label, function, old, new, cfg, arg_index), mutated) in enumerate(
                zip(variants, mutated_sources), 1):
            mutation_source = temp / f"mutant_{index:02d}.c"
            mutation_source.write_text(mutated, encoding="utf-8")
            dll = build(mutation_source, trapv=trapv, out_dir=temp, tag=f"mutant_{index:02d}")
            side = Side(dll)
            counts[function] += 1
            if function == "SwingDoorSetPivot":
                cfg = {"direction": cfg["direction"], "mode": cfg["mode"], "values": cfg.get("values")}
            try:
                okay = mutation_probe(h, label, function, cfg, arg_index, side)
            except OSError:
                # A mutant can turn a rejected index into recursive out-of-range accesses.
                # The original instruction path remains backed; a host C stack failure is
                # therefore a caught mutation, not a checker crash.
                okay = False
            finally:
                side.model = None
                side.callbacks.clear()
                side.hooks = CHooks()
                del side
                gc.collect()
            if okay:
                survivors.append(label)
            else:
                caught.append(label)
    h.m.coverage = set()
    print("mutation coverage: " + ", ".join(f"{name} {counts[name]}" for name in FUNCS))
    print(f"mutations caught: {len(caught)}/{len(variants)}")
    if survivors:
        print("mutation survivors: " + ", ".join(survivors))
    return not survivors and len(variants) >= 40 and all(counts[name] > 0 for name in FUNCS), len(caught), len(variants)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--coverage", action="store_true", help="print original instruction coverage")
    parser.add_argument("--mutate", action="store_true", help="run the function-scoped mutation suite")
    parser.add_argument("--trapv", action="store_true", help="compile with -ftrapv and exercise signed edge inputs")
    parser.add_argument("--cases", type=float, default=1.0, help="scale randomized cases (default 1)")
    parser.add_argument("--seed", type=int, default=94423)
    args = parser.parse_args()

    side = Side(build(trapv=args.trapv))
    h = GimmickHarness(side, with_coverage=True)
    baseline_scenarios(h, args.cases, args.seed)
    default_run = args.cases == 1.0 and args.seed == 94423 and not args.trapv
    summary = h.finish(EXTRA_COVERAGE, default_run=default_run,
                       show_coverage=args.coverage or default_run)
    print("per-function differential cases:")
    for name in FUNCS:
        print(f"  {name:28s} {h.counts.get(name, 0)}")
    print(f"total differential cases: {sum(h.counts.values())}")

    coverage_gaps = []
    if summary:
        coverage_gaps = [name for name, item in summary.items() if item["uncovered"] or item["stale_declared"]]
    if coverage_gaps:
        print("coverage gaps: " + ", ".join(coverage_gaps))

    mutation_ok = True
    if args.mutate:
        mutation_ok, _, _ = run_mutations(h, trapv=args.trapv)

    if h.failures or coverage_gaps or not mutation_ok:
        return 1
    print("PASS: all gimmick differential scenarios matched")
    if args.mutate:
        print("PASS: all required mutations were caught")
    return 0


if __name__ == "__main__":
    sys.exit(main())
