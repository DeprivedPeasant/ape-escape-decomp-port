"""Differentially check the readable Zako initialization port against the USA EXE."""
import argparse
import ctypes as C
import gc
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas
import exclusion_registry as reg
import mips_ram as mr

STACK_LO = 0x1FF000
AREA_MEMBER = dump_areas.area_members()[1]

# name: (original address, byte size, return kind, signed argument string, has hooks)
FUNCS = {
    "ModelInitAuto": (0x80015BA0, 148, "v", "uu", False),
    "ZakoTypeIsIgnored": (0x80050580, 72, "s", "s", False),
    "ZakoInit": (0x800505C8, 752, "v", "", True),
    "ZakoClearSlotTable": (0x800509A4, 40, "v", "", False),
    "ZakoSpawnFromEntry": (0x800508B8, 236, "v", "uus", True),
    "ObjUnregisterBody": (0x80051E34, 100, "v", "u", True),
    "ObjKill": (0x80051E98, 132, "v", "us", True),
    "ZakoSetupType": (0x80050D04, 1184, "v", "uu", True),
    "ZakoSetAreaMode": (0x80057FD0, 356, "v", "uus", True),
    "ZakoFindEntryByType": (0x80051FE8, 80, "s", "u", False),
    "ObjInitModel": (0x80052038, 112, "v", "uu", False),
    "ZakoGetAnimTable": (0x800520A8, 140, "u", "uu", False),
    "ZakoSetAnimFromEntry": (0x80052134, 56, "v", "uu", False),
    "ZakoHooksReset": (0x80057558, 48, "v", "", False),
    "ObjSetPathFromTable": (0x800583BC, 68, "v", "us", False),
    "ZakoDebugMenuInit": (0x800587EC, 360, "v", "", True),
    "MenuItemInitToggle": (0x8007D23C, 84, "v", "uuu", True),
    "GetLevelFirstArea": (0x8009764C, 28, "s", "s", False),
}

EXTRA_COVERAGE = {}
U32 = C.c_uint32
I32 = C.c_int32
HOOKS = [
    ("Printf", 0x800A3720, 1),
    ("MenuItemInitBound", 0x8007D1E0, 3),
    ("MenuItemSetCallback", 0x8007D29C, 2),
    ("ReadSizedValue", 0x8007D118, 2),
]
HOOKFN = C.CFUNCTYPE(I32, C.c_void_p, C.c_void_p, U32, U32, U32, U32)


class ZakoHooks(C.Structure):
    _fields_ = [("user", C.c_void_p)] + [(name, HOOKFN) for name in (
        "Printf", "MenuItemInitBound", "MenuItemSetCallback", "ReadSizedValue")] + [
            ("body_hooks", C.c_void_p)]


def build(source=None, tag="zako_init"):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{time.time_ns()}.dll"
    subprocess.run([
        shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-shared", "-static-libgcc", "-I", str(ROOT / "src/world"),
        str(ROOT / "src/core/ae_ram.c"),
        str(ROOT / "src/player/native_player.c"), str(ROOT / "src/bodies/ae_bodies.c"),
        str(ROOT / "src/world/ae_world.c"),
        str(source or ROOT / "src/world/ae_zako_init.c"), "-o", str(dll), "-lm",
    ], check=True)
    return dll


class CSide:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        P = C.c_void_p
        self.lib.ae_ram_create.restype = P
        self.lib.ae_ptr.argtypes = [P, U32, U32]
        self.lib.ae_ptr.restype = P
        self.lib.ae_ram_clear_error.argtypes = [P]
        self.lib.ae_rd32.argtypes = [P, U32]
        self.lib.ae_rd32.restype = U32
        self.lib.ae_ram_failed.argtypes = [P]
        self.lib.ae_ram_failed.restype = C.c_int
        self.lib.ae_ram_error.argtypes = [P]
        self.lib.ae_ram_error.restype = C.c_char_p
        self.lib.ae_wr8.argtypes = [P, U32, U32]
        self.lib.ae_wr16.argtypes = [P, U32, U32]
        self.lib.ae_wr32.argtypes = [P, U32, U32]
        self.ram = self.lib.ae_ram_create()
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(
            self.lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(
            self.lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.funcs = {}
        for name, (_address, _size, ret, args, _has_hooks) in FUNCS.items():
            fn = getattr(self.lib, "ae_" + name)
            fn.argtypes = [P] + ([C.POINTER(ZakoHooks)] if _has_hooks else []) + [
                I32 if kind == "s" else U32 for kind in args]
            fn.restype = None if ret == "v" else I32
            self.funcs[name] = fn
        self.hooks = ZakoHooks()
        self.callbacks = []
        self.model = None
        for name, _address, _nargs in HOOKS:
            callback = HOOKFN(lambda _user, _ram, a0, a1, a2, a3, n=name:
                               self.model.call(n, (a0, a1, a2, a3)))
            self.callbacks.append(callback)
            setattr(self.hooks, name, callback)

    def sync(self, mips):
        C.memmove(self.main, bytes(mips.ram), mr.RAM_SIZE)
        C.memmove(self.scratch, bytes(mips.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)

    def read(self, address, size):
        if mr.SCRATCH_BASE <= address < mr.SCRATCH_BASE + mr.SCRATCH_SIZE:
            buffer, offset = self.scratch, address - mr.SCRATCH_BASE
        else:
            buffer, offset = self.main, address & 0x1FFFFF
        return int.from_bytes(bytes(buffer[offset:offset + size]), "little")

    def write(self, address, value, size):
        if mr.SCRATCH_BASE <= address < mr.SCRATCH_BASE + mr.SCRATCH_SIZE:
            buffer, offset = self.scratch, address - mr.SCRATCH_BASE
        else:
            buffer, offset = self.main, address & 0x1FFFFF
        buffer[offset:offset + size] = (value & ((1 << (size * 8)) - 1)).to_bytes(size, "little")


class HookModel:
    def __init__(self, context, memory):
        self.context = context
        self.memory = memory
        self.events = []

    def read_sized_value(self, pointer, size):
        if "read_value" in self.context:
            return int(self.context["read_value"])
        if size == 0:
            value = self.memory.read(pointer, 1)
            return value - 0x100 if value & 0x80 else value
        if size == 1:
            value = self.memory.read(pointer, 2)
            return value - 0x10000 if value & 0x8000 else value
        if size == 2:
            value = self.memory.read(pointer, 4)
            return value - 0x100000000 if value & 0x80000000 else value
        if size == 3:
            return self.memory.read(pointer, 1)
        return 0

    def call(self, name, args):
        count = next(n for hook, _address, n in HOOKS if hook == name)
        args = tuple(value & 0xFFFFFFFF for value in args[:count])
        self.events.append((name, args))
        if name == "ReadSizedValue":
            return self.read_sized_value(args[0], args[1])
        if name == "MenuItemInitBound":
            item, variable, size = args
            self.memory.write(item + 0x20, size, 1)
            self.memory.write(item + 0x0C, 0x8007D110, 4)
            self.memory.write(item + 0x08, variable, 4)
            self.memory.write(item + 0x18,
                              self.read_sized_value(variable, size) & 0xFFFFFFFF, 4)
            self.memory.write(item + 0x10, -9999, 4)
            self.memory.write(item + 0x14, 9999, 4)
            self.memory.write(item + 0x1C, 1, 4)
            return 0
        if name == "MenuItemSetCallback":
            item, callback = args
            self.memory.write(item + 0x0C, callback, 4)
            return 0
        if name == "Printf":
            return 0
        raise AssertionError(name)


class MipsMemory:
    def __init__(self, cpu):
        self.cpu = cpu

    def read(self, address, size):
        return self.cpu.read(address, size)

    def write(self, address, value, size):
        self.cpu.write(address, size, value)


class Harness:
    def __init__(self, source=None, tag="zako_init"):
        reg.declare("stack_window", "801FF000..80200000")
        members = dump_areas.area_members()
        self.area_bases = {
            area: mr.MipsRam.from_extract(ROOT / f"extract/members/{members[area]:03X}.bin").snapshot()
            for area in (1, 2, 3, 6)
        }
        self.m = mr.MipsRam.from_extract(ROOT / f"extract/members/{members[1]:03X}.bin")
        self.m.coverage = set()
        self.base = self.area_bases[1]
        self.c = CSide(build(source, tag))
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0

    def run(self, name, args, setup=lambda _m: None, context=None, area_id=None):
        self.m.restore(self.area_bases.get(area_id, self.base))
        setup(self.m)
        self.c.sync(self.m)
        context = context or {}
        original_model = HookModel(context, MipsMemory(self.m))
        c_model = HookModel(context, self.c)
        self.c.model = c_model
        mips_hooks = {address: (lambda _cpu, regs, n=hook:
                                original_model.call(n, regs[4:8]))
                      for hook, address, _nargs in HOOKS}
        original = self.m.call(FUNCS[name][0], args, hooks=mips_hooks)
        call_args = [self.c.ram]
        if FUNCS[name][4]:
            call_args.append(C.byref(self.c.hooks))
        result = self.c.funcs[name](*call_args, *args)
        self.counts[name] += 1
        if FUNCS[name][2] in ("s", "u") and (int(result) & 0xFFFFFFFF) != original[2]:
            self.fail(name, args, f"return C={int(result) & 0xFFFFFFFF:08X} MIPS={original[2]:08X}")
        expected = bytes(self.m.ram[:STACK_LO])
        actual = C.string_at(self.c.main, STACK_LO)
        if actual != expected:
            index = next(i for i, (left, right) in enumerate(zip(actual, expected)) if left != right)
            start = max(0, index - 4)
            self.fail(name, args,
                      f"RAM differs at {0x80000000 + index:08X}: C={actual[start:start + 12].hex()} "
                      f"MIPS={expected[start:start + 12].hex()}"
                      + (f" flags C={self.c.lib.ae_rd32(self.c.ram, 0x801780E0):08X} "
                         f"MIPS={self.m.read32(0x801780E0):08X}"
                         f" target_call={0x8007CD88 in self.m.coverage}"
                         if name == "ZakoSetupType" else "")
                      + (f" hooks C={c_model.events!r} MIPS={original_model.events!r}"
                         if name == "ZakoDebugMenuInit" else ""))
        if bytes(self.c.scratch) != bytes(self.m.scratch):
            self.fail(name, args, "scratchpad differs")
        if original_model.events != c_model.events:
            self.fail(name, args,
                      f"hook sequence differs C={c_model.events!r} MIPS={original_model.events!r}")
        if self.c.lib.ae_ram_failed(self.c.ram):
            self.fail(name, args, self.c.lib.ae_ram_error(self.c.ram).decode(errors="replace"))

    def fail(self, name, args, detail):
        self.failures += 1
        raise SystemExit(f"FAIL {name}{tuple(args)}: {detail}")

    def finish(self, show_coverage=False, default_run=False, only=None):
        items = [(name, address, size, True) for name, (address, size, *_rest) in FUNCS.items()]
        if only is not None:
            items = [item for item in items if item[0] == only]
        summary = reg.coverage_summary(items, self.m.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        gaps = any(row["uncovered_count"] or row["stale_declared"] for row in summary.values())
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps,
                              function_names=list(FUNCS))
        if gaps:
            raise SystemExit("FAIL: undeclared Zako instruction coverage gaps")
        return summary


def scenarios(h, scale=1.0, seed=94423, only=None):
    rng = random.Random(seed)

    def run(name, args, setup=lambda _m: None, context=None, area_id=None):
        if only is None or name == only:
            h.run(name, args, setup, context, area_id)

    for bones, verts, colors, normals in ((0, 0, 0, 0), (1, 2, 3, 0), (4, 9, 7, 1)):
        def model(m, bones=bones, verts=verts, colors=colors, normals=normals):
            header = 0x80170000
            m.write16(header + 4, bones)
            m.write16(header + 6, verts)
            m.write16(header + 8, colors)
            m.write16(header + 10, normals)
        run("ModelInitAuto", (0x80171000, 0x80170000), model)

    for value in [0, 0x60, 0x61, 0x62, 0x63, 0x6C, 0x6D, 0x6E, 0x6F,
                  0x71, 0x72, 0x84, 0xB4, 0xB5, 0xB6, 0xFF, -1]:
        run("ZakoTypeIsIgnored", (value,))

    def progress(m):
        for i in range(8):
            m.write32(0x800B8D00 + i * 4, rng.getrandbits(32))
    run("ZakoClearSlotTable", (), progress)

    obj = 0x80178000
    entry_table = 0x80170000

    def object_state(m, flags, spawn_index, register_body, add_target=False, kills=0):
        m.call(0x80071D3C, ())
        if register_body:
            m.call(0x80071E44, (4, obj + 0x24, obj))
        m.write32(obj + 0x1C, obj + 0x1C)
        m.write32(obj + 0xE0, flags)
        m.write8(obj + 0xCC, spawn_index)
        m.write16(0x800F44B0, kills)
        m.write32(0x800E77D4, entry_table)
        m.write_bytes(entry_table, bytes(0x100))
        if spawn_index != 0xFF:
            m.write8(entry_table + spawn_index * 0x20 + 0x16, 1)
        if add_target:
            m.write32(0x800F51F0, obj + 0x1C)
            for i in range(1, 32):
                m.write32(0x800F51F0 + i * 4, 0)
        else:
            for i in range(32):
                m.write32(0x800F51F0 + i * 4, 0)

    for flags, register_body, add_target in [
            (0, False, False), (0x4002, False, False),
            (0x4000, True, False), (0x08004000, True, True)]:
        run("ObjUnregisterBody", (obj,),
            lambda m, f=flags, b=register_body, t=add_target:
                object_state(m, f, 0xFF, b, t))

    for count_as_kill, flags, spawn_index, register_body, add_target, kills in [
            (0, 0, 0xFF, False, False, 0),
            (1, 0x08004010, 2, True, True, 0xFFFF),
            (1, 0x4002, 0, False, False, 0x1234),
            (0, 0x08004000, 0xFF, True, True, 0)]:
        run("ObjKill", (obj, count_as_kill),
            lambda m, f=flags, si=spawn_index, b=register_body, t=add_target, k=kills:
                object_state(m, f, si, b, t, k))

    for kind, current_area, index in [
            (0x3D, 1, 4), (0x23, 1, 5), (0x57, 1, 6),
            (0x2E, 1, 7), (0x45, 1, 8), (0x40, 1, 9),
            (0x1A, 1, -1), (0x6F, 1, 10), (0x6F, 0x4C, 11),
            (0x10, 1, 12), (0x30, 1, 13), (0x3E, 1, 14)]:
        def area_mode(m, kind=kind, current_area=current_area):
            m.write32(obj + 0xE0, 0x12345678)
            m.write8(obj + 0xCC, 0xFF)
            m.write16(0x800F4476, current_area)
            m.write32(0x800E77D4, entry_table)
            m.write32(entry_table, 0)
            m.write8(0x800E77A1, 0xFF)
            m.write8(0x800E77A2, 0xEE)
        run("ZakoSetAreaMode", (obj, kind, index), area_mode)

    def model_entries(m, kinds, clip_counts=(0, 1, 3)):
        table = 0x80170000
        zed = 0x80190000
        m.write16(0x800E77DC, len(kinds))
        m.write32(0x800E77D4, table)
        m.write32(0x800F4594, zed)
        for i, kind in enumerate(kinds):
            entry = table + i * 0x20
            model_offset = 0x1000 + i * 0x100
            anim_offset = 0x3000 + i * 0x100
            m.write16(entry, kind)
            m.write32(entry + 0x18, model_offset)
            m.write32(entry + 0x1C, anim_offset)
            model = zed + model_offset
            m.write16(model + 4, i + 1)
            m.write16(model + 6, 3 + i)
            m.write16(model + 8, 2 + i)
            m.write16(model + 10, i & 1)
            animation = zed + anim_offset
            m.write16(animation, clip_counts[i % len(clip_counts)])

    for kind in list(range(0xB7)) + [0xFF]:
        def setup_type(m, kind=kind):
            m.call(0x80071D3C, ())
            for i in range(32):
                m.write32(0x800F51F0 + i * 4, 0)
            model_entries(m, [kind])
            m.write32(obj + 0xE0, rng.choice([0, 0xFFFFFFFF, 0x08004000, 0x4002]))
            m.write8(obj + 0x7E, kind)
            m.write8(obj + 0xCB, kind & 0x0F)
            for offset in (0x24, 0x28, 0x2C):
                m.write32(obj + offset, rng.choice([-32768, -1, 0, 32767, rng.getrandbits(32)]))
        run("ZakoSetupType", (obj, kind), setup_type)

    for kind, path_index, current_area, entry_index in [
            (3, 0xFFFF, 1, 0), (3, 0, 1, 1),
            (0x23, 1, 1, 2), (0x6F, 0xFFFF, 0x4C, 3)]:
        def spawn_entry(m, kind=kind, path_index=path_index, current_area=current_area):
            m.call(0x80071D3C, ())
            for i in range(32):
                m.write32(0x800F51F0 + i * 4, 0)
            model_entries(m, [kind])
            entry = 0x80170000
            zed = 0x80190000
            m.write16(entry + 0x02, -1234)
            m.write16(entry + 0x04, 2345)
            m.write16(entry + 0x06, -32768)
            m.write16(entry + 0x08, 120)
            m.write16(entry + 0x0A, 45)
            m.write16(entry + 0x0C, 0xFFFF)
            m.write16(entry + 0x0E, path_index)
            m.write8(entry + 0x16, 0)
            path_table = zed + 0x4000
            m.write32(0x800E77D8, path_table)
            m.write16(path_table, 3)
            m.write32(path_table + 4, 0x5000)
            m.write16(path_table + 8, 2)
            m.write32(path_table + 12, 0x5040)
            m.write16(0x800F4476, current_area)
            m.write32(obj + 0xE0, 0x10000008)
            m.write8(obj + 0xCB, 4)
        run("ZakoSpawnFromEntry", (obj, 0x80170000, entry_index), spawn_entry)

    for kinds, requested in [([], 3), ([3], 3), ([3, 7, 11], 7), ([3, 7, 11], 11),
                             ([3, 7, 11], 12)]:
        run("ObjInitModel", (obj, requested), lambda m, ks=kinds: model_entries(m, ks))
        run("ZakoGetAnimTable", (obj, requested),
            lambda m, ks=kinds: model_entries(m, ks))
        run("ZakoSetAnimFromEntry", (obj, requested),
            lambda m, ks=kinds, requested=requested:
                (model_entries(m, ks), m.write8(obj + 0x7E, requested)))

    entry_base = 0x80170000
    for count, match in [(0, 0), (1, 1), (4, 2), (4, 7), (4, 15), (-1, 1)]:
        def entries(m, count=count):
            m.write16(0x800E77DC, count)
            m.write32(0x800E77D4, entry_base)
            for i, kind in enumerate([3, 7, 11, 15]):
                m.write16(entry_base + i * 0x20, kind)
        run("ZakoFindEntryByType", (match,), entries)

    def hooks_reset(m):
        m.write8(0x800E77A5, 7)
        m.write32(0x800B8D20, 0x80123456)
        m.write32(0x800B8D24, 0x80123458)
    run("ZakoHooksReset", (), hooks_reset)

    def paths(m, index):
        table = 0x80170000
        zed = 0x80154000 + 0x1000
        m.write32(0x800E77D8, table)
        m.write32(0x800F4594, zed)
        entry = table + (index & 0xFFFF) * 8
        m.write16(entry, 4)
        m.write32(entry + 4, 0x500 + (index & 0xFFFF) * 8)
    for index in [0, 1, -1]:
        run("ObjSetPathFromTable", (0x80172000, index), lambda m, index=index: paths(m, index))

    for level in [-1, 0, 1, 2, 33, 69]:
        run("GetLevelFirstArea", (level,))

    for size in [0, 1, 3, 0xFF]:
        for value in [0, 1, -1, 0x12345678]:
            run("MenuItemInitToggle", (0x80173000, 0x80174000, size),
                lambda m: m.write32(0x80174000, value & 0xFFFFFFFF),
                {"read_value": value})

    def debug_values(m):
        for offset, value in enumerate([0x80, 0x7F, 0xFF, 0x01, 0xFE, 0x00,
                                        0x42, 0x81, 0x10, 0x20, 0x30, 0x40,
                                        0x50, 0x60]):
            m.write8(0x800E77E4 + offset, value)
    run("ZakoDebugMenuInit", (), debug_values)

    for area_id, area_kind, defeated_mask, grid_limit in [
            (1, 0, 0x00000005, 1365),
            (2, 0, 0x0000001F, 3000),
            (3, 0, 0x00003FFF, -12),
            (6, 0, 0x0000FFFF, 2356),
            (3, 13, 0xFFFFFFFF, 2048),
            (2, 0, 0x00000001, 1366),
            (1, 0, 0x00000003, 2355),
    ]:
        def zako_init_setup(m, area_id=area_id, area_kind=area_kind,
                            defeated_mask=defeated_mask, grid_limit=grid_limit):
            m.call(0x80071D3C, ())
            for i in range(32):
                m.write32(0x800F51F0 + i * 4, 0)
            zed_offset = m.read32(0x80154000 + 0x4E0)
            m.write32(0x800F4594, 0x80154000 + zed_offset)
            m.write16(0x800F4350 + 0x124, 0)
            m.write16(0x800F4350 + 0x126, area_id)
            m.write8(0x800F4350 + 0x12C, area_kind)
            m.write32(0x800EBFC8, grid_limit)
            for i in range(8):
                m.write32(0x800B8D00 + i * 4,
                          defeated_mask if i == 0 else (0xA5A50000 | i))
            for i in range(16):
                obj_address = 0x800E6960 + i * 0xE4
                m.write32(obj_address + 0xE0,
                          rng.choice([0, 0xFFFFFFFF, 0x08004000, 0x4002]))
                m.write8(obj_address + 0x7E, rng.randrange(0xB7))
            for offset in range(0x24):
                m.write8(0x800E77D0 + offset, rng.randrange(256))
        run("ZakoInit", (), zako_init_setup, area_id=area_id)

    def zako_bad_zed(m):
        m.call(0x80071D3C, ())
        for i in range(32):
            m.write32(0x800F51F0 + i * 4, 0)
        m.write32(0x800F4594, 0x80170000)
        m.write8(0x80170000, 0x5A)
        m.write16(0x800F4350 + 0x124, 0)
        m.write16(0x800F4350 + 0x126, 1)
        m.write8(0x800F4350 + 0x12C, 0)
        m.write32(0x800EBFC8, 2000)
    run("ZakoInit", (), zako_bad_zed)

    def zako_overflow_area(m):
        m.call(0x80071D3C, ())
        for i in range(32):
            m.write32(0x800F51F0 + i * 4, 0)
        zed_offset = m.read32(0x80154000 + 0x4E0)
        m.write32(0x800F4594, 0x80154000 + zed_offset)
        m.write16(0x800F4350 + 0x124, 0)
        m.write16(0x800F4350 + 0x126, 9)
        m.write8(0x800F4350 + 0x12C, 0)
        m.write32(0x800EBFC8, 2731)
        for i in range(8):
            m.write32(0x800B8D00 + i * 4, 0)
    run("ZakoInit", (), zako_overflow_area, area_id=1)
    _ = scale


def mutation_sources():
    """Two focused, behavior-changing mistakes for every ported function."""
    return [
        ("model bone stride", "ModelInitAuto", "bone_count * 12u", "bone_count * 16u"),
        ("model normal vertex stride", "ModelInitAuto",
         "has_normals != 0 ? AE_MODEL_NORMAL_VERTEX_STRIDE : AE_MODEL_VERTEX_STRIDE",
         "has_normals != 0 ? AE_MODEL_NORMAL_VERTEX_STRIDE : AE_MODEL_NORMAL_VERTEX_STRIDE"),
        ("ignored exact type", "ZakoTypeIsIgnored", "case 0x61u:", "case 0x62u:"),
        ("ignored range end", "ZakoTypeIsIgnored", "< 0x44u", "< 0x43u"),
        ("init grid clamp", "ZakoInit", "grid_limit_source < AE_ZAKO_GRID_LIMIT_X_CLAMP",
         "grid_limit_source < (AE_ZAKO_GRID_LIMIT_X_CLAMP + 1)"),
        ("init sparse defeat mask", "ZakoInit", "if ((defeated_mask & 1u) != 0)",
         "if (defeated_mask != 0)"),
        ("spawn x coordinate", "ZakoSpawnFromEntry",
         "entry + AE_ZED_ENTRY_X", "entry + AE_ZED_ENTRY_Y"),
        ("spawn activation byte", "ZakoSpawnFromEntry",
         "entry + AE_ZED_ENTRY_ACTIVE, 1u", "entry + AE_ZED_ENTRY_ACTIVE, 0u"),
        ("slot table final word", "ZakoClearSlotTable",
         "i < AE_ZAKO_PROGRESS_WORD_COUNT", "i + 1u < AE_ZAKO_PROGRESS_WORD_COUNT"),
        ("slot table contents", "ZakoClearSlotTable", "i * AE_ZAKO_PROGRESS_WORD_SIZE, 0)",
         "i * AE_ZAKO_PROGRESS_WORD_SIZE, 1u)"),
        ("setup spawn flag", "ZakoSetupType", "flags |= AE_OBJ_FLAG_SPAWNED;",
         "flags &= ~AE_OBJ_FLAG_SPAWNED;"),
        ("setup body radius", "ZakoSetupType", "obj + AE_OBJ_BODY_RADIUS, 0x30u",
         "obj + AE_OBJ_BODY_RADIUS, 0x31u"),
        ("unregister marker", "ObjUnregisterBody",
         "ae_rd32(r, obj + AE_OBJ_FLAGS) | AE_OBJ_FLAG_BODY_UNREGISTERED",
         "ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_BODY_UNREGISTERED"),
        ("unregister target condition", "ObjUnregisterBody",
         "(flags & AE_OBJ_FLAG_TARGET_TRACKED) != 0",
         "(flags & AE_OBJ_FLAG_TARGET_TRACKED) == 0"),
        ("kill animation", "ObjKill", "obj + AE_OBJ_ANIM_INDEX, 1u",
         "obj + AE_OBJ_ANIM_INDEX, 2u"),
        ("kill spawn test", "ObjKill", "spawn_index != AE_ZAKO_NO_SPAWN_INDEX",
         "spawn_index == AE_ZAKO_NO_SPAWN_INDEX"),
        ("find type condition", "ZakoFindEntryByType",
         "ae_rd16(r, entry + AE_ZED_ENTRY_TYPE) == type",
         "ae_rd16(r, entry + AE_ZED_ENTRY_TYPE) != type"),
        ("find entry stride", "ZakoFindEntryByType", "(uint32_t)i * AE_ZED_ENTRY_SIZE",
         "(uint32_t)i * (AE_ZED_ENTRY_SIZE + 2u)"),
        ("model entry zero", "ObjInitModel", "if (entry_index < 0) return;",
         "if (entry_index < 1) return;"),
        ("model header source", "ObjInitModel", "AE_ZED_ENTRY_MODEL_OFFSET",
         "AE_ZED_ENTRY_ANIM_OFFSET"),
        ("animation returned pointer", "ZakoGetAnimTable",
         "return animation_header + AE_ANIM_HEADER_CLIP_TABLE;", "return anim_table;"),
        ("animation clip stride", "ZakoGetAnimTable",
         "clip_count * AE_ANIM_CLIP_RECORD_SIZE",
         "clip_count * (AE_ANIM_CLIP_RECORD_SIZE + 4u)"),
        ("animation frame reset", "ZakoSetAnimFromEntry", "AE_OBJ_ANIM_FRAME, 0",
         "AE_OBJ_ANIM_FRAME, 1u"),
        ("animation slot reset", "ZakoSetAnimFromEntry", "AE_OBJ_ANIM_SLOT, 0)",
         "AE_OBJ_ANIM_SLOT, 1u)"),
        ("hook count reset", "ZakoHooksReset", "AE_G_ZAKO_HOOK_COUNT, 0",
         "AE_G_ZAKO_HOOK_COUNT, 1u"),
        ("hook pointer reset", "ZakoHooksReset", "AE_G_ZAKO_HOOKS + i * AE_ZAKO_HOOK_POINTER_SIZE, 0)",
         "AE_G_ZAKO_HOOKS + i * AE_ZAKO_HOOK_POINTER_SIZE, 1u)"),
        ("area mode mapping", "ZakoSetAreaMode", "AE_G_ZAKO_MODE_KIND, 3u",
         "AE_G_ZAKO_MODE_KIND, 2u"),
        ("area mode counter", "ZakoSetAreaMode", "AE_G_ZAKO_MODE_COUNTER) + 1u",
         "AE_G_ZAKO_MODE_COUNTER) + 2u"),
        ("path pointer base", "ObjSetPathFromTable", "area_zed + path_offset",
         "area_zed + path_offset + AE_ZED_PATH_ENTRY_SIZE"),
        ("path table stride", "ObjSetPathFromTable", "index * AE_ZED_PATH_ENTRY_SIZE",
         "index * (AE_ZED_PATH_ENTRY_SIZE + 1u)"),
        ("debug variable offset", "ZakoDebugMenuInit", "{1, 1, 0, 0}", "{1, 2, 0, 0}"),
        ("debug last item index", "ZakoDebugMenuInit", "{15, 9, 0, 0}", "{14, 9, 0, 0}"),
        ("toggle size field", "MenuItemInitToggle",
         "AE_MENU_ITEM_SIZE_KIND, size)", "AE_MENU_ITEM_SIZE_KIND, size + 1u)"),
        ("toggle range", "MenuItemInitToggle", "AE_MENU_ITEM_RANGE, 1u",
         "AE_MENU_ITEM_RANGE, 2u"),
        ("first-area stride", "GetLevelFirstArea", "(uint32_t)level * AE_LEVEL_FIRST_AREA_ENTRY_SIZE",
         "(uint32_t)level * (AE_LEVEL_FIRST_AREA_ENTRY_SIZE + 1u)"),
        ("first-area halfword address", "GetLevelFirstArea", "return ae_rds16(r, entry);",
         "return ae_rds16(r, entry + 1u);"),
    ]


def mutate_function(source, function, old, new):
    definition = re.search(rf"(?m)^(?:void|int32_t|uint32_t)\s+ae_{re.escape(function)}\s*\(", source)
    if definition is None:
        raise RuntimeError(f"cannot locate ae_{function} definition for mutation testing")
    opening = source.find("{", definition.end())
    depth = 0
    closing = None
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                closing = index + 1
                break
    if closing is None:
        raise RuntimeError(f"cannot find end of ae_{function} for mutation testing")
    body = source[definition.start():closing]
    count = body.count(old)
    if count != 1:
        raise RuntimeError(f"mutation anchor for {function} matched {count} times: {old!r}")
    start = definition.start()
    return source[:start] + body.replace(old, new, 1) + source[closing:]


def run_mutations():
    source = (ROOT / "src/world/ae_zako_init.c").read_text(encoding="utf-8")
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    variants = mutation_sources()
    counts = {name: 0 for name in FUNCS}
    caught = []
    survivors = []
    for index, (label, function, old, new) in enumerate(variants, 1):
        mutated = mutate_function(source, function, old, new)
        mutation_source = None
        try:
            with tempfile.NamedTemporaryFile(
                    mode="w", encoding="utf-8", suffix=".c", prefix="zako_mutation_",
                    dir=out, delete=False) as file:
                file.write(mutated)
                mutation_source = Path(file.name)
            harness = Harness(mutation_source, f"zako_mutant_{index:02d}")
            mutation_source.unlink()
            mutation_source = None
            try:
                scenarios(harness, only=function)
            except SystemExit:
                caught.append(label)
                counts[function] += 1
                print(f"CAUGHT {label}", flush=True)
            else:
                survivors.append(label)
                counts[function] += 1
                print(f"SURVIVED {label}", flush=True)
            del harness
            gc.collect()
        finally:
            if mutation_source is not None:
                mutation_source.unlink(missing_ok=True)
    print("mutation coverage: " + ", ".join(
        f"{name}={counts[name]}" for name in FUNCS), flush=True)
    print(f"mutations caught: {len(caught)}/{len(variants)}", flush=True)
    if survivors:
        print("mutation survivors: " + ", ".join(survivors), flush=True)
    return int(bool(survivors) or len(variants) < 30 or
               any(counts[name] < 2 for name in FUNCS))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--cases", type=float, default=1.0)
    ap.add_argument("--seed", type=int, default=94423)
    ap.add_argument("--function")
    ap.add_argument("--mutate", action="store_true")
    args = ap.parse_args()
    if args.mutate:
        return run_mutations()
    h = Harness()
    scenarios(h, args.cases, args.seed, args.function)
    summary = h.finish(show_coverage=args.coverage,
                       default_run=args.cases == 1.0 and args.seed == 94423 and args.function is None,
                       only=args.function)
    print(" ".join(f"{name}={h.counts[name]}" for name in FUNCS))
    print(f"OK: {sum(h.counts.values())} differential cases")
    return int(any(row["uncovered_count"] or row["stale_declared"] for row in summary.values()))


if __name__ == "__main__":
    sys.exit(main())
