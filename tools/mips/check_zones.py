"""Differentially check the area terrain/collider-zone and debug-menu ports."""
import argparse
import ctypes as C
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas
import exclusion_registry as reg
import mips_ram as mr

SOURCE = ROOT / "src/world/ae_zones.c"
HEADER = ROOT / "src/world/ae_zones.h"
STACK_LO = 0x1FF000
AREA_BASE = 0x80154000
PROPS_BASE = 0x801B0000
MATRIX_BASE = 0x801B5000
TERRAIN_MASK_BASE = 0x801B8000
SYNTHETIC_COLLIDERS = 0x80170000
AREA_IDS = (0x01, 0x02, 0x04, 0x05)

FUNCS = {
    "SvecToDiagMatrix": (0x800147D0, 52, "v", "uu", False),
    "BuildTerrainBlockMask": (0x800438F8, 404, "u", "", True),
    "DebugMenuInitFlags": (0x8005CA34, 80, "v", "", False),
    "DebugMenuInitPlayerParams": (0x8005CB1C, 128, "v", "", False),
    "DebugMenuInitCounters": (0x8005CC58, 132, "v", "", False),
    "DebugMenuInitCounters2": (0x8005CCDC, 108, "v", "", False),
    "ReadSizedValue": (0x8007D118, 112, "s", "us", False),
    "MenuItemInitBound": (0x8007D1E0, 92, "s", "uus", False),
    "MenuItemSetCallback": (0x8007D29C, 8, "v", "uu", False),
    "ColliderZoneMasksUpdate": (0x80088670, 820, "s", "", True),
    "ColliderComputeQuadrantMask": (0x800889EC, 204, "s", "u", True),
    "FUN_8013fc00": (0x8013FC00, 1, "v", "s", True),
}
HOOKS = {
    "AreaBumpAlloc": 0x8008C1B8,
    "GetColliderQuadrantMaskXZ": 0x800889B4,
    "FUN_8013fc00": 0x8013FC00,
}
RETURNS = {name for name, spec in FUNCS.items() if spec[2] != "v"}
EXTRA_COVERAGE = {}


class DiffFailure(AssertionError):
    pass


def build(source=None, tag="zones", trapv=False):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{os.getpid()}_{time.time_ns()}.dll"
    gcc = shutil.which("gcc")
    if not gcc:
        raise SystemExit("gcc is required to build the RAM-image port")
    command = [
        gcc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-shared", "-static-libgcc",
        "-I" + str(ROOT / "src/world"), str(ROOT / "src/core/ae_ram.c"),
        str(source or SOURCE), "-o", str(dll),
    ]
    if trapv:
        command.insert(2, "-ftrapv")
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
    return dll


class HookFn:
    alloc = C.CFUNCTYPE(C.c_uint32, C.c_void_p, C.c_void_p, C.c_uint32)
    quadrant = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_int32, C.c_int32)
    overlay = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32)


ZonesHooks = type("AeZonesHooks", (C.Structure,), {
    "_fields_": [("user", C.c_void_p), ("area_bump_alloc", HookFn.alloc),
                 ("get_collider_quadrant_mask_xz", HookFn.quadrant), ("FUN_8013fc00", HookFn.overlay)]
})


class Memory:
    def __init__(self, cpu=None, side=None):
        self.cpu = cpu
        self.side = side

    def read(self, address, size):
        if self.cpu is not None:
            return self.cpu.read(address, size)
        if mr.SCRATCH_BASE <= address < mr.SCRATCH_BASE + mr.SCRATCH_SIZE:
            raw, offset = self.side.scratch, address - mr.SCRATCH_BASE
        else:
            raw, offset = self.side.main, address & 0x1FFFFF
        return int.from_bytes(bytes(raw[offset:offset + size]), "little")


class HookModel:
    def __init__(self, memory, context):
        self.memory = memory
        self.context = context
        self.events = []

    @staticmethod
    def _u32(value):
        return int(value) & 0xFFFFFFFF

    def call(self, name, args):
        args = tuple(self._u32(value) for value in args)
        if name == "AreaBumpAlloc":
            result = self.context.get("alloc_result", TERRAIN_MASK_BASE)
        elif name == "GetColliderQuadrantMaskXZ":
            x = mr.signed(args[0], 16)
            z = mr.signed(args[1], 16)
            if x < 0:
                result = 1 if z < 0 else 0x1000
            else:
                result = 0x10 if z < 0 else 0x100
        elif name == "FUN_8013fc00":
            result = 0
        else:
            raise AssertionError(f"unexpected hook {name}")
        self.events.append((name, args))
        return result


class CSide:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        P, U, I = C.c_void_p, C.c_uint32, C.c_int32
        self.lib.ae_ram_create.restype = P
        self.lib.ae_ram_destroy.argtypes = [P]
        self.lib.ae_ptr.argtypes = [P, U, U]
        self.lib.ae_ptr.restype = P
        self.lib.ae_ram_failed.argtypes = [P]
        self.lib.ae_ram_failed.restype = C.c_int
        self.lib.ae_ram_error.argtypes = [P]
        self.lib.ae_ram_error.restype = C.c_char_p
        self.lib.ae_ram_clear_error.argtypes = [P]
        self.ram = self.lib.ae_ram_create()
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(
            self.lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(
            self.lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.model = None
        self.callbacks = []
        self.hooks = ZonesHooks()
        self._bind_hooks()
        for name, (address, _size, ret, args, has_hooks) in FUNCS.items():
            fn = getattr(self.lib, "ae_" + name)
            fn.argtypes = [P] + ([C.POINTER(ZonesHooks)] if has_hooks else []) + [
                I if kind == "s" else U for kind in args
            ]
            fn.restype = {"v": None, "s": I, "u": U}[ret]

    def _bind_hooks(self):
        alloc = HookFn.alloc(lambda _user, _ram, size: self.model.call("AreaBumpAlloc", (size,)))
        quadrant = HookFn.quadrant(
            lambda _user, _ram, x, z: self.model.call("GetColliderQuadrantMaskXZ", (x, z)))
        overlay = HookFn.overlay(
            lambda _user, _ram, index: self.model.call("FUN_8013fc00", (index,)))
        self.callbacks.extend((alloc, quadrant, overlay))
        self.hooks.area_bump_alloc = alloc
        self.hooks.get_collider_quadrant_mask_xz = quadrant
        self.hooks.FUN_8013fc00 = overlay

    def sync(self, cpu):
        C.memmove(self.main, bytes(cpu.ram), mr.RAM_SIZE)
        C.memmove(self.scratch, bytes(cpu.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)

    def invoke(self, name, args):
        fn = getattr(self.lib, "ae_" + name)
        has_hooks = FUNCS[name][4]
        prefix = [self.ram]
        if has_hooks:
            prefix.append(C.byref(self.hooks))
        return fn(*prefix, *args)

    def failed(self):
        return bool(self.lib.ae_ram_failed(self.ram))

    def error(self):
        return self.lib.ae_ram_error(self.ram).decode(errors="replace")


def area_blob(area_id):
    member = dump_areas.area_members()[area_id]
    path = ROOT / "extract/members" / f"{member:03X}.bin"
    if not path.is_file():
        raise SystemExit(f"missing real area member for area {area_id:02X}: {path}")
    return member, path.read_bytes()


def install_area(cpu, area_id):
    """Recreate the relevant AreaLoad pointer fixups over an actual area member."""
    member, blob = area_blob(area_id)
    cpu.load_area(blob)
    u32 = lambda offset: struct.unpack_from("<I", blob, offset)[0]
    u16 = lambda offset: struct.unpack_from("<H", blob, offset)[0]
    header = u32(0x4D0)
    grid = header + 4 * u32(header + 4)
    width, depth, color_count = struct.unpack_from("<HHI", blob, grid)
    cells = grid + 8
    heights = cells + width * depth * 12 + color_count * 4
    collision = header + 4 * u32(header + 0x10)
    collision_grid = collision + u32(collision)
    shapes = collision + u32(collision + 4)
    collider_table = collision + u32(collision + 8)
    collider_count = u16(collider_table)
    collider_base = AREA_BASE + collider_table + 2
    cpu.write32(0x800F44E4, AREA_BASE + header)
    cpu.write16(0x800EBFA0, width)
    cpu.write16(0x800EBFA2, depth)
    cpu.write32(0x800EBFA8, AREA_BASE + cells)
    cpu.write32(0x800EBFB4, AREA_BASE + heights)
    cpu.write32(0x800F4558, AREA_BASE + shapes + 2)
    cpu.write32(0x800F4560, AREA_BASE + collision_grid + 4)
    cpu.write32(0x800F455C, collider_base)
    cpu.write16(0x800F45D4, collider_count)
    cpu.write32(0x800B4D64, PROPS_BASE)

    max_prop_index = -1
    for index in range(collider_count):
        collider = collider_base + index * 0x38
        prop_index = cpu.read(collider + 0x0E, 2)
        prop_index = mr.signed(prop_index, 16)
        max_prop_index = max(max_prop_index, prop_index)
    # MeshInit normally supplies MATRIX* values in AreaProp.rot. Give type-12
    # colliders valid backing matrices while preserving each collider's real fields.
    for prop_index in range(-1, max_prop_index + 1):
        prop = PROPS_BASE + prop_index * 0x20
        matrix = MATRIX_BASE + prop_index * 0x14
        cpu.write32(prop + 0x1C, matrix)
    return {"area": area_id, "member": member, "width": width, "depth": depth,
            "colliders": collider_count, "collider_base": collider_base,
            "cells": AREA_BASE + cells}


def seed_menu(cpu, seed):
    rng = random.Random(seed)
    for address, count, stride in ((0x800AFC1C, 18, 0x24), (0x800AFB68, 5, 0x24),
                                   (0x800AFF10, 6, 0x24), (0x800AFEC8, 1, 0x24)):
        for index in range(count * stride):
            cpu.write(address + index, 1, rng.randrange(256))
    for index in range(18):
        cpu.write32(0x800EC010 + index * 4, rng.getrandbits(32))
    for address, size in ((0x800B01BC, 4), (0x800F4610, 8), (0x800F4614, 8)):
        for offset in range(size):
            cpu.write(address + offset, 1, rng.randrange(256))


def setup_terrain_bits(cpu, meta):
    width, depth = meta["width"], meta["depth"]
    if width < 4 or depth < 4:
        return
    cells = meta["cells"]
    # One empty 2x2 block and one blocked block; the member remains real but
    # this directed case forces both mask outcomes independently of the area.
    for address in (cells, cells + 4, cells + width * 8, cells + width * 8 + 4):
        cpu.write32(address, 0)
    blocked = cells + 8
    cpu.write32(blocked, cpu.read(blocked, 4) | 1)


def setup_synthetic_colliders(cpu, _meta=None):
    types = [0, 9, 1, 1, 3, 4, 5, 6, 8, 12, 14, 2, 13, 3, 2]
    for index, kind in enumerate(types):
        collider = SYNTHETIC_COLLIDERS + index * 0x38
        for offset in range(0x38):
            cpu.write(collider + offset, 1, 0)
        cpu.write16(collider + 0x00, (-1800 + index * 250) & 0xFFFF)
        cpu.write16(collider + 0x02, (320 + index * 3) & 0xFFFF)
        cpu.write16(collider + 0x04, (1400 - index * 230) & 0xFFFF)
        cpu.write16(collider + 0x06, (120 + index * 5) & 0xFFFF)
        cpu.write16(collider + 0x0A, (180 + index * 7) & 0xFFFF)
        cpu.write16(collider + 0x0C, (720 + index * 10) & 0xFFFF)
        cpu.write16(collider + 0x0E, 0 if index != 2 else 0xFFFF)
        cpu.write8(collider + 0x12, kind)
        cpu.write16(collider + 0x18, 0x8000 if index == 3 else (3 if index == 4 else 0))
        cpu.write16(collider + 0x1C, (540 + index * 6) & 0xFFFF)
        cpu.write16(collider + 0x2A, 120 + index)
        cpu.write16(collider + 0x2C, -80 + index)
        cpu.write16(collider + 0x2E, 60 - index)
    cpu.write32(0x800F455C, SYNTHETIC_COLLIDERS)
    cpu.write16(0x800F45D4, len(types))
    # Type 12 accesses a MATRIX* through AreaProp.rot.
    cpu.write32(PROPS_BASE + 0x1C, MATRIX_BASE)
    cpu.write32(PROPS_BASE - 0x20 + 0x1C, MATRIX_BASE - 0x14)


def setup_random_synthetic_colliders(cpu, meta=None, seed=94423):
    setup_synthetic_colliders(cpu, meta)
    rng = random.Random(seed)
    edge_values = (-32768, -30001, -1, 0, 1, 30001, 32767)
    for index, kind in enumerate((3, 4, 5, 6, 2, 13)):
        collider = SYNTHETIC_COLLIDERS + index * 0x38
        first = rng.choice(edge_values) if index % 2 == 0 else rng.randint(-32768, 32767)
        second = rng.choice(edge_values) if index % 2 == 1 else rng.randint(-32768, 32767)
        cpu.write16(collider + 0x0C, first & 0xFFFF)
        cpu.write16(collider + 0x1C, second & 0xFFFF)
        for offset in (0x2A, 0x2C, 0x2E):
            cpu.write16(collider + offset, rng.choice(edge_values) & 0xFFFF)
        cpu.write8(collider + 0x12, kind)


def setup_zero_count(cpu, _meta=None):
    cpu.write16(0x800F45D4, 0)


def case(name, args=(), area=None, setup=None, context=None):
    return {"name": name, "args": tuple(args), "area": area, "setup": setup, "context": context or {}}


def all_cases(seed=94423):
    cases = []
    for area in AREA_IDS:
        cases.append(case("BuildTerrainBlockMask", area=area))
    cases.append(case("BuildTerrainBlockMask", area=0x01, setup=setup_terrain_bits))
    cases.append(case("BuildTerrainBlockMask", area=0x01,
                      setup=lambda m, meta: (m.write16(0x800EBFA0, 0), m.write16(0x800EBFA2, 4))))
    cases.append(case("BuildTerrainBlockMask", area=0x01,
                      setup=lambda m, meta: m.write16(0x800EBFA2, 0)))

    for index, value in enumerate((0x80, 0xFF, 0x7F)):
        def setup_flags(m, meta, value=value, index=index):
            seed_menu(m, seed + 10 + index)
            m.write16(0x800B01BC, value | ((0xFF - value) << 8))
        cases.append(case("DebugMenuInitFlags", setup=setup_flags))
    for index in range(3):
        cases.append(case("DebugMenuInitPlayerParams", setup=lambda m, meta, i=index: seed_menu(m, seed + 20 + i)))
        cases.append(case("DebugMenuInitCounters", setup=lambda m, meta, i=index: seed_menu(m, seed + 30 + i)))
        cases.append(case("DebugMenuInitCounters2", setup=lambda m, meta, i=index: seed_menu(m, seed + 40 + i)))

    read_sizes = ((0, 0x800E3000), (1, 0x800E3002), (2, 0x800E3004), (3, 0x800E3008),
                  (-1, 0x800E300C), (4, 0x800E3010), (0x7FFFFFFF, 0x800E3014))
    for index, (size, address) in enumerate(read_sizes):
        def setup_read(m, meta, address=address, index=index):
            m.write32(address & ~3, (0x8000FF81 ^ (index * 0x10204081)) & 0xFFFFFFFF)
        cases.append(case("ReadSizedValue", (address, size), setup=setup_read))

    for index, size in enumerate((0, 1, 2, 3, -1, 4)):
        address = 0x800E7200
        variable = 0x800E7100
        def setup_bound(m, meta, address=address, variable=variable, index=index):
            m.write32(variable, (0x80FF017F ^ index * 0x11001100) & 0xFFFFFFFF)
            for byte in range(0x24):
                m.write(address + byte, 1, (byte * 17 + index * 31) & 0xFF)
        cases.append(case("MenuItemInitBound", (address, variable, size), setup=setup_bound))
    for index in range(4):
        cases.append(case("MenuItemSetCallback", (0x800E7300, 0x80010000 + index * 4),
                          setup=lambda m, meta, i=index: m.write32(0x800E730C, 0xDEADBEEF ^ i)))

    for index, vector in enumerate(((0xF001, 0xFFFF, 0x8000), (1, 2, 3), (0x7FFF, 0xFFFF, 0))):
        def setup_vector(m, meta, vector=vector, index=index):
            address = 0x800E7400 + index * 0x20
            for axis, value in enumerate(vector):
                m.write16(address + axis * 2, value)
            for byte in range(0x12):
                m.write(address + 0x100 + byte, 1, 0xA5 ^ byte ^ index)
        cases.append(case("SvecToDiagMatrix", (0x800E7400 + index * 0x20, 0x800E7500 + index * 0x20), setup=setup_vector))
    def setup_overlap_vector(m, _meta):
        for index, value in enumerate((0x1234, 0x2345, 0x3456)):
            m.write16(0x800E7600 + index * 2, value)
    cases.append(case("SvecToDiagMatrix", (0x800E7600, 0x800E7600), setup=setup_overlap_vector))

    for index, (x, z, hx, hz) in enumerate(((32760, -32768, 20, 7), (-1, 1, 12, 12),
                                            (1200, -800, 1700, 900), (0, 0, 0, 0))):
        address = 0x800E7700
        def setup_quadrant(m, meta, x=x, z=z, hx=hx, hz=hz):
            for off, value in ((0, x), (4, z), (6, hx), (10, hz)):
                m.write16(address + off, value & 0xFFFF)
        cases.append(case("ColliderComputeQuadrantMask", (address,), setup=setup_quadrant))

    for index in range(3):
        cases.append(case("ColliderZoneMasksUpdate", area=AREA_IDS[index]))
    cases.append(case("ColliderZoneMasksUpdate", area=0x01, setup=setup_synthetic_colliders))
    for index in range(8):
        cases.append(case("ColliderZoneMasksUpdate", area=0x01,
                          setup=lambda m, meta, seed=seed + 100 + index:
                          setup_random_synthetic_colliders(m, meta, seed)))
    cases.append(case("ColliderZoneMasksUpdate", area=0x01, setup=setup_zero_count))
    cases.append(case("ColliderZoneMasksUpdate", area=0x01,
                      setup=lambda m, meta: m.write16(0x800F45D4, 0xFFFF)))

    for index in (0, 1, 17, 65535):
        cases.append(case("FUN_8013fc00", (index,)))
    return cases


class ZonesHarness:
    def __init__(self, source=None, tag="zones", trapv=False):
        reg.declare("stack_window", "801FF000..80200000")
        self.m = mr.MipsRam.from_extract()
        self.m.coverage = set()
        self.exe_base = self.m.snapshot()
        self.c = CSide(build(source, tag, trapv=trapv))
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0

    def run(self, item):
        name, args = item["name"], item["args"]
        self.m.restore(self.exe_base)
        meta = install_area(self.m, item["area"]) if item["area"] is not None else None
        if item["setup"] is not None:
            item["setup"](self.m, meta)
        self.c.sync(self.m)

        original_model = HookModel(Memory(cpu=self.m), item["context"])
        c_model = HookModel(Memory(side=self.c), item["context"])
        self.c.model = c_model
        mips_hooks = {
            address: (lambda cpu, regs, n=hook: original_model.call(
                n, tuple(regs[4 + arg] for arg in range(1 if n != "GetColliderQuadrantMaskXZ" else 2))))
            for hook, address in HOOKS.items()
        }
        try:
            original_regs = self.m.call(FUNCS[name][0], args, hooks=mips_hooks)
        except Exception as exc:  # print the case before surfacing the original emulator fault
            raise DiffFailure(f"{name}{args}: original MIPS execution failed: {exc}") from exc
        original_main = bytes(self.m.ram[:STACK_LO])
        original_scratch = bytes(self.m.scratch)
        original_events = list(original_model.events)

        c_result = self.c.invoke(name, args)
        c_main = C.string_at(self.c.main, STACK_LO)
        c_scratch = bytes(self.c.scratch)
        c_events = list(c_model.events)
        self.counts[name] += 1
        if name in RETURNS and (int(c_result) & 0xFFFFFFFF) != original_regs[2]:
            self.fail(name, args, f"return differs: C={int(c_result) & 0xFFFFFFFF:08X} MIPS={original_regs[2]:08X}")
        if c_main != original_main:
            index = next(i for i, (left, right) in enumerate(zip(c_main, original_main)) if left != right)
            self.fail(name, args, f"RAM differs at {0x80000000 + index:08X}: C={c_main[index]:02X} MIPS={original_main[index]:02X}")
        if c_scratch != original_scratch:
            self.fail(name, args, "scratchpad differs")
        if c_events != original_events:
            self.fail(name, args, f"hook events differ: C={c_events!r}; MIPS={original_events!r}")
        if self.c.failed():
            self.fail(name, args, self.c.error())

    @staticmethod
    def fail(name, args, detail):
        raise DiffFailure(f"{name}{tuple(args)}: {detail}")

    def finish(self, default_run=False, show_coverage=True, extra=None):
        items = [(name, addr, size, True) for name, (addr, size, *_rest) in FUNCS.items()]
        summary = reg.coverage_summary(items, self.m.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        gaps = any(entry["uncovered_count"] or entry["stale_declared"] for entry in summary.values())
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps,
                              extra=extra, function_names=list(FUNCS))
        if gaps:
            raise SystemExit("FAIL: undeclared zone instruction coverage gaps")
        return summary


def mutation_case(key):
    """One directed case for each changed expression in MUTATIONS."""
    mapped = {
        "svec": "SvecToDiagMatrix", "svec_z": "SvecToDiagMatrix", "terrain": "BuildTerrainBlockMask",
        "flags": "DebugMenuInitFlags", "player": "DebugMenuInitPlayerParams",
        "counters": "DebugMenuInitCounters", "counters2": "DebugMenuInitCounters2",
        "read0": "ReadSizedValue", "read1": "ReadSizedValue", "read3": "ReadSizedValue",
        "bound": "MenuItemInitBound", "callback": "MenuItemSetCallback",
        "zone": "ColliderZoneMasksUpdate", "quadrant": "ColliderComputeQuadrantMask",
        "overlay": "FUN_8013fc00",
    }
    if key == "svec":
        return case("SvecToDiagMatrix", (0x800E7600, 0x800E7600), setup=lambda m, meta: setup_overlap_vector(m, meta))
    if key == "svec_z":
        return case("SvecToDiagMatrix", (0x800E7400, 0x800E7500), setup=lambda m, meta: (
            m.write16(0x800E7400, 0x1234), m.write16(0x800E7402, 0x2345), m.write16(0x800E7404, 0x3456)))
    if key == "terrain":
        return case("BuildTerrainBlockMask", area=0x01, setup=setup_terrain_bits)
    if key == "flags":
        return case("DebugMenuInitFlags", setup=lambda m, meta: (seed_menu(m, 991), m.write16(0x800B01BC, 0x80FF)))
    if key == "player":
        return case("DebugMenuInitPlayerParams", setup=lambda m, meta: seed_menu(m, 992))
    if key == "counters":
        return case("DebugMenuInitCounters", setup=lambda m, meta: seed_menu(m, 993))
    if key == "counters2":
        return case("DebugMenuInitCounters2", setup=lambda m, meta: seed_menu(m, 994))
    if key == "read0":
        return case("ReadSizedValue", (0x800E3000, 0), setup=lambda m, meta: m.write8(0x800E3000, 0xFF))
    if key == "read1":
        return case("ReadSizedValue", (0x800E3002, 1), setup=lambda m, meta: m.write16(0x800E3002, 0xFFFF))
    if key == "read3":
        return case("ReadSizedValue", (0x800E3000, 3), setup=lambda m, meta: m.write8(0x800E3000, 0xFF))
    if key == "bound":
        return case("MenuItemInitBound", (0x800E7200, 0x800E7100, 2),
                    setup=lambda m, meta: m.write32(0x800E7100, 0x8000FF81))
    if key == "callback":
        return case("MenuItemSetCallback", (0x800E7300, 0x80012344))
    if key == "zone":
        return case("ColliderZoneMasksUpdate", area=0x01, setup=setup_synthetic_colliders)
    if key == "quadrant":
        return case("ColliderComputeQuadrantMask", (0x800E7700,), setup=lambda m, meta: (
            m.write16(0x800E7700, 0xFFFF), m.write16(0x800E7704, 0xFFFF),
            m.write16(0x800E7706, 2), m.write16(0x800E770A, 2)))
    if key == "overlay":
        return case("FUN_8013fc00", (-1,))
    raise AssertionError(key)


def setup_overlap_vector(cpu, _meta=None):
    for index, value in enumerate((0x1234, 0x2345, 0x3456)):
        cpu.write16(0x800E7600 + index * 2, value)


def mutations():
    return [
        ("SvecToDiagMatrix", "svec", "AE_ZONES_MATRIX_DIAGONAL_X, x", "AE_ZONES_MATRIX_DIAGONAL_X, x + 1u"),
        ("SvecToDiagMatrix", "svec", "AE_ZONES_MATRIX_OFF_DIAGONAL_12, 0);", "AE_ZONES_MATRIX_OFF_DIAGONAL_12, 1);"),
        ("SvecToDiagMatrix", "svec_z", "vector + AE_ZONES_SVECTOR_Z);", "vector + AE_ZONES_SVECTOR_Y);"),
        ("BuildTerrainBlockMask", "terrain", "mask_size = (width >> 1) * (depth >> 1);", "mask_size = width * (depth >> 1);"),
        ("BuildTerrainBlockMask", "terrain", "((ae_rd32(r, cell_address) & 1u) == 0u)", "((ae_rd32(r, cell_address) & 1u) != 0u)"),
        ("BuildTerrainBlockMask", "terrain", "is_empty ? 0xFFu : 0u", "is_empty ? 0u : 0xFFu"),
        ("DebugMenuInitFlags", "flags", "AE_ZONES_DEBUG_FLAGS_ITEM_GLOBAL, AE_ZONES_DEBUG_FLAGS_ITEM", "AE_ZONES_DEBUG_FLAGS_ITEM_GLOBAL, AE_ZONES_DEBUG_FLAGS_ITEM + 4u"),
        ("DebugMenuInitFlags", "flags", "AE_ZONES_DEBUG_FLAGS_ITEM, AE_ZONES_DEBUG_FLAGS_VARIABLE, 1", "AE_ZONES_DEBUG_FLAGS_ITEM, AE_ZONES_DEBUG_FLAGS_VARIABLE, 2"),
        ("DebugMenuInitFlags", "flags", "AE_ZONES_DEBUG_FLAGS_CALLBACK);", "AE_ZONES_DEBUG_FLAGS_CALLBACK + 4u);"),
        ("DebugMenuInitPlayerParams", "player", "index < AE_ZONES_DEBUG_PLAYER_ITEM_COUNT", "index < AE_ZONES_DEBUG_PLAYER_ITEM_COUNT - 1u"),
        ("DebugMenuInitPlayerParams", "player", "AE_ZONES_DEBUG_PLAYER_VARIABLES + index * 4u", "AE_ZONES_DEBUG_PLAYER_VARIABLES + index * 2u"),
        ("DebugMenuInitPlayerParams", "player", "AE_ZONES_DEBUG_PLAYER_CALLBACK);", "AE_ZONES_DEBUG_PLAYER_CALLBACK + 4u);"),
        ("DebugMenuInitCounters", "counters", "index < AE_ZONES_DEBUG_COUNTER_COUNT", "index < AE_ZONES_DEBUG_COUNTER_COUNT - 1u"),
        ("DebugMenuInitCounters", "counters", "AE_ZONES_DEBUG_COUNTER_VARIABLES + index;", "AE_ZONES_DEBUG_COUNTER_VARIABLES + index * 2u;"),
        ("DebugMenuInitCounters", "counters", "AE_ZONES_DEBUG_COUNTERS_CALLBACK);", "AE_ZONES_DEBUG_COUNTERS_CALLBACK + 4u);"),
        ("DebugMenuInitCounters2", "counters2", "index < AE_ZONES_DEBUG_COUNTER2_COUNT", "index < AE_ZONES_DEBUG_COUNTER2_COUNT - 1u"),
        ("DebugMenuInitCounters2", "counters2", "AE_ZONES_DEBUG_COUNTER2_VARIABLES + index;", "AE_ZONES_DEBUG_COUNTER2_VARIABLES + index * 2u;"),
        ("DebugMenuInitCounters2", "counters2", "AE_ZONES_DEBUG_COUNTER2_TABLE_GLOBAL, AE_ZONES_DEBUG_COUNTER2_ITEMS", "AE_ZONES_DEBUG_COUNTER2_TABLE_GLOBAL, AE_ZONES_DEBUG_COUNTER2_ITEMS + 4u"),
        ("ReadSizedValue", "read0", "if (size == 0)", "if (size == 4)"),
        ("ReadSizedValue", "read1", "return ae_rds16(r, address);", "return (int32_t)ae_rd16(r, address);"),
        ("ReadSizedValue", "read3", "return (int32_t)ae_rd8(r, address);", "return ae_rds8(r, address);"),
        ("MenuItemInitBound", "bound", "item + AE_ZONES_MENU_ITEM_VALUE_SIZE", "item + AE_ZONES_MENU_ITEM_STEP"),
        ("MenuItemInitBound", "bound", "AE_ZONES_MENU_ITEM_DEFAULT_CALLBACK);", "AE_ZONES_MENU_ITEM_DEFAULT_CALLBACK + 4u);"),
        ("MenuItemInitBound", "bound", "(uint32_t)-9999);", "(uint32_t)-10000);"),
        ("MenuItemSetCallback", "callback", "item + AE_ZONES_MENU_ITEM_CALLBACK, callback);", "item + AE_ZONES_MENU_ITEM_VARIABLE, callback);"),
        ("MenuItemSetCallback", "callback", "item + AE_ZONES_MENU_ITEM_CALLBACK, callback);", "item + AE_ZONES_MENU_ITEM_CALLBACK, callback + 1u);"),
        ("MenuItemSetCallback", "callback", "ae_wr32(r, item + AE_ZONES_MENU_ITEM_CALLBACK, callback);", "ae_wr16(r, item + AE_ZONES_MENU_ITEM_CALLBACK, callback);"),
        ("ColliderZoneMasksUpdate", "zone", "(flags & AE_ZONES_COLLIDER_DISABLED) != 0u", "(flags & 0x4000u) != 0u"),
        ("ColliderZoneMasksUpdate", "zone", "zone_mask = 0x1111u;", "zone_mask = 0x1110u;"),
        ("ColliderZoneMasksUpdate", "zone", "if (type == 5u)", "if (type == 4u)"),
        ("ColliderComputeQuadrantMask", "quadrant", "(uint16_t)center_x + (uint16_t)half_x", "(uint16_t)center_x - (uint16_t)half_x"),
        ("ColliderComputeQuadrantMask", "quadrant", "x_negative, z_positive);", "x_negative, z_negative);"),
        ("ColliderComputeQuadrantMask", "quadrant", "return (int16_t)(uint16_t)mask;", "return (int16_t)(uint16_t)(mask ^ 1);"),
        ("FUN_8013fc00", "overlay", "(h->user, r, collider_index);", "(h->user, r, collider_index + 1);"),
        ("FUN_8013fc00", "overlay", "h->FUN_8013fc00(h->user, r, collider_index);", "(void)collider_index;"),
        ("FUN_8013fc00", "overlay", "if (h == NULL || h->FUN_8013fc00 == NULL)", "if (h == NULL || h->FUN_8013fc00 == NULL || collider_index == -1)"),
    ]


def run_mutations():
    rows = mutations()
    caught = 0
    for index, (function_name, key, before, after) in enumerate(rows, 1):
        source_text = SOURCE.read_text(encoding="utf-8")
        if before not in source_text:
            raise SystemExit(f"mutation {index} ({function_name}) could not find source text: {before!r}")
        mutated_text = source_text.replace(before, after, 1)
        mutated_source = ROOT / "extract/mips" / f"zones_mutation_{index}.c"
        mutated_source.write_text(mutated_text, encoding="utf-8")
        h = ZonesHarness(mutated_source, tag=f"zones_mut{index}")
        try:
            h.run(mutation_case(key))
        except DiffFailure:
            caught += 1
            print(f"mutation {index:02d}/{len(rows)} caught: {function_name}", flush=True)
            continue
        raise SystemExit(f"mutation {index} was NOT caught: {function_name} / {before}")
    print(f"mutation testing: {caught}/{len(rows)} caught across {len({row[0] for row in rows})} target functions")
    if len(rows) < 30 or caught != len(rows):
        raise SystemExit("FAIL: mutation requirement not met")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mutate", action="store_true", help="run all deliberate source mutations")
    parser.add_argument("--trapv", action="store_true", help="compile the C port with signed-overflow traps")
    parser.add_argument("--seed", type=int, default=94423)
    args = parser.parse_args()
    if args.mutate:
        run_mutations()
        return

    h = ZonesHarness(trapv=args.trapv)
    all_items = all_cases(args.seed)
    for index, item in enumerate(all_items, 1):
        h.run(item)
        if index % 10 == 0:
            print(f"checked {index}/{len(all_items)} function cases", flush=True)
    summary = h.finish(default_run=True, show_coverage=True,
                       extra={"mutation_tests_required": len(mutations()),
                              "real_areas": [f"{area:02X}" for area in AREA_IDS]})
    print("per-function case counts: " + " ".join(f"{name}={h.counts[name]}" for name in FUNCS))
    print(f"total cases: {sum(h.counts.values())}")
    return summary


if __name__ == "__main__":
    main()
