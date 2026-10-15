"""Differentially check the collision probe ports against SCUS-94423 and real area members.

Run `.venv/Scripts/python.exe tools/mips/check_probe.py --coverage` from the repository root; use `--mutate` for the
targeted 41-variant mutation suite.
"""
import argparse
import contextlib
import ctypes as C
import io
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
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

AREA_IDS = (0x01, 0x05, 0x17, 0x48, 0x65)
PROBE_BUFFER = 0x801F0000
PROBE_FLAGS = 0x801F0100
BODY_BUFFER = 0x801F0200
BOX_BUFFER = 0x801F0300
POINT_BUFFER = 0x801F0400
HOOK_COUNTER = 0x801F0800
HOOK_SCRATCH = 0x1F8003F0
COLLIDER_TYPE = 0x12
COLLIDER_FLAGS = 0x18
STACK_COMPARE_END = mr.STACK_WINDOW[0] - 0x80000000
MASK32 = 0xFFFFFFFF

AREA_HEADER = 0x800F44E4
LEVEL_GRID_WIDTH = 0x800EBFA0
LEVEL_GRID_DEPTH = 0x800EBFA2
LEVEL_GRID_CELLS = 0x800EBFA8
LEVEL_GRID_HEIGHTS = 0x800EBFB4

FUNCS = {
    "CollisionBuildProbePoints": (0x8007304C, 912, "ae_CollisionBuildProbePoints", "su"),
    "CollisionPointInGrid": (0x8004F6E8, 412, "ae_CollisionPointInGrid", "u"),
    "CollisionShiftProbePoints": (0x800733DC, 116, "ae_CollisionShiftProbePoints", "ss"),
    "PointInBoxXYZ": (0x80073AA0, 196, "ae_PointInBoxXYZ", "uu"),
    "BodyProbeGround": (0x80072D28, 156, "ae_BodyProbeGround", "u"),
}
EXTRA_COVERAGE = {}
BODY_TYPES = (6, 7, 12, 13)


def u16(blob, off):
    return struct.unpack_from("<H", blob, off)[0]


def s16(blob, off):
    return struct.unpack_from("<h", blob, off)[0]


def u32(blob, off):
    return struct.unpack_from("<I", blob, off)[0]


def wrap_s16(value):
    return ((value + 0x8000) & 0xFFFF) - 0x8000


def load_area_meta(area_id, member):
    path = ROOT / "extract" / "members" / f"{member:03X}.bin"
    blob = path.read_bytes()
    area_header = u32(blob, 0x4D0)
    terrain_grid = area_header + u32(blob, area_header + 4) * 4
    terrain_width, terrain_depth = struct.unpack_from("<HH", blob, terrain_grid)
    colors = u32(blob, terrain_grid + 4)
    terrain_cells = terrain_grid + 8
    terrain_heights = terrain_cells + terrain_width * terrain_depth * 4 + colors * 4 + terrain_width * terrain_depth * 8

    collision = area_header + u32(blob, area_header + 0x10) * 4
    collision_offsets = struct.unpack_from("<5I", blob, collision)
    collision_grid = collision + collision_offsets[0]
    shapes_record = collision + collision_offsets[1]
    colliders_record = collision + collision_offsets[2]
    collision_width, collision_depth = struct.unpack_from("<hh", blob, collision_grid)
    shape_count = u16(blob, shapes_record)
    collider_count = u16(blob, colliders_record)
    shape_data = shapes_record + 2
    collider_data = colliders_record + 2
    grid_data = collision_grid + 4

    cells = []
    empty_cells = []
    for z in range(collision_depth):
        for x in range(collision_width):
            index = s16(blob, grid_data + 2 * (z * collision_width + x))
            (cells if index >= 0 else empty_cells).append((x, z, index))

    if not cells:
        raise RuntimeError(f"area {area_id:02X} has no populated collision-grid cells")
    flags = u32(blob, area_header)
    if flags & 2:
        raise RuntimeError(f"area {area_id:02X} has a disabled terrain grid; choose a heightfield area")
    if terrain_width == 0 or terrain_depth == 0:
        raise RuntimeError(f"area {area_id:02X} has no terrain grid")
    return {
        "area": area_id,
        "member": member,
        "path": path,
        "blob": blob,
        "area_header": area_header,
        "flags": flags,
        "terrain_width": terrain_width,
        "terrain_depth": terrain_depth,
        "terrain_cells": terrain_cells,
        "terrain_heights": terrain_heights,
        "collision_grid": collision_grid,
        "collision_grid_data": grid_data,
        "collision_width": collision_width,
        "collision_depth": collision_depth,
        "shape_data": shape_data,
        "shape_count": shape_count,
        "collider_data": collider_data,
        "collider_count": collider_count,
        "cells": cells,
        "empty_cells": empty_cells,
        "half_x": collision_width * 256,
        "half_z": collision_depth * 256,
        "terrain_half_x": terrain_width * 256,
        "terrain_half_z": terrain_depth * 256,
    }


def area_metas():
    members = dump_areas.area_members()
    out = {}
    for area_id in AREA_IDS:
        member = members.get(area_id)
        if member is None:
            raise RuntimeError(f"no original area member for area {area_id:02X}")
        out[area_id] = load_area_meta(area_id, member)
    return out


def install_area(cpu, meta):
    blob = meta["blob"]
    base = mr.AREA_BASE
    ah = meta["area_header"]
    terrain_cells = meta["terrain_cells"]
    terrain_heights = meta["terrain_heights"]
    collision = ah + struct.unpack_from("<I", blob, ah + 0x10)[0] * 4
    shape_grid = collision + struct.unpack_from("<I", blob, collision)[0] + 4
    shapes = meta["shape_data"]
    colliders = meta["collider_data"]

    cpu.write32(AREA_HEADER, base + ah)
    cpu.write16(LEVEL_GRID_WIDTH, meta["terrain_width"])
    cpu.write16(LEVEL_GRID_DEPTH, meta["terrain_depth"])
    cpu.write32(LEVEL_GRID_CELLS, base + terrain_cells)
    cpu.write32(LEVEL_GRID_HEIGHTS, base + terrain_heights)

    cpu.write32(0x800F4558, base + shapes)
    cpu.write32(0x800F455C, base + colliders)
    cpu.write32(0x800F4560, base + shape_grid)
    cpu.write16(0x800F45D4, meta["collider_count"])
    cpu.write32(0x800F45F0, meta["collision_width"])
    cpu.write32(0x800F45F4, meta["collision_depth"])
    cpu.write32(0x800F45F8, meta["half_x"])
    cpu.write32(0x800F45FC, meta["half_z"])
    cpu.write32(0x800B20B0, PROBE_BUFFER)
    cpu.write32(0x800B20C0, PROBE_FLAGS)


def area_cell_center(meta, cell_x, cell_z):
    return cell_x * 512 + 256 - meta["half_x"], cell_z * 512 + 256 - meta["half_z"]


def shape_chain(meta, first):
    result = []
    visited = set()
    index = first
    while index >= 0 and index not in visited and len(result) <= meta["shape_count"]:
        visited.add(index)
        offset = meta["shape_data"] + index * 0x14
        result.append((index, s16(meta["blob"], offset + 2), s16(meta["blob"], offset + 8)))
        index = s16(meta["blob"], offset + 0x10)
    return result


def build_dll(source=None, tag="probe"):
    out = ROOT / "extract" / "mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{os.getpid()}_{time.time_ns()}.dll"
    compiler = shutil.which("gcc")
    if not compiler:
        raise RuntimeError("gcc is required for the RAM-image checker")
    subprocess.run([
        compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-shared", "-static-libgcc",
        "-I" + str(ROOT / "src" / "bodies"),
        str(ROOT / "src" / "core" / "ae_ram.c"),
        str(source or ROOT / "src" / "bodies" / "ae_probe.c"),
        str(ROOT / "src" / "world" / "ae_world.c"),
        "-o", str(dll),
    ], check=True)
    return dll


WorldHookFn = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                          C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)


class WorldHooks(C.Structure):
    _fields_ = [
        ("user", C.c_void_p),
        ("GetSolidHeight", WorldHookFn),
        ("PlaySfxAt", WorldHookFn),
        ("GimmickActivate", WorldHookFn),
        ("MeshInitFromAreaTable", WorldHookFn),
        ("FUN_8013d358", WorldHookFn),
        ("CameraShake", WorldHookFn),
        ("EventCamStart", WorldHookFn),
        ("ground_stale_x", C.c_uint16),
        ("ground_stale_z", C.c_uint16),
    ]


class CSide:
    def __init__(self, dll):
        self.lib = lib = C.CDLL(str(dll))
        P, U, I = C.c_void_p, C.c_uint32, C.c_int32
        lib.ae_ram_create.restype = P
        lib.ae_ptr.argtypes = [P, U, U]
        lib.ae_ptr.restype = P
        lib.ae_ram_clear_error.argtypes = [P]
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        lib.ae_ram_destroy.argtypes = [P]
        lib.ae_BodyProbeGround.argtypes = [P, P, U]
        lib.ae_BodyProbeGround.restype = I
        lib.ae_CollisionBuildProbePoints.argtypes = [P, I, U]
        lib.ae_CollisionBuildProbePoints.restype = I
        lib.ae_CollisionPointInGrid.argtypes = [P, U]
        lib.ae_CollisionPointInGrid.restype = U
        lib.ae_CollisionShiftProbePoints.argtypes = [P, I, I]
        lib.ae_CollisionShiftProbePoints.restype = I
        lib.ae_PointInBoxXYZ.argtypes = [P, U, U]
        lib.ae_PointInBoxXYZ.restype = I
        self.ram = lib.ae_ram_create()
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.model = None
        self.hooks = WorldHooks()
        self._keep = []

        def get_solid(_user, ram, x, y, z, _unused):
            return self.model.solid_call(CMemory(self), x, y, z)

        callback = WorldHookFn(get_solid)
        self._keep.append(callback)
        self.hooks.GetSolidHeight = callback

    def call(self, name, args):
        address, _size, cname, kinds = FUNCS[name]
        converted = [int(value) if kind == "s" else int(value) & MASK32 for value, kind in zip(args, kinds)]
        function = getattr(self.lib, cname)
        if name == "BodyProbeGround":
            return function(self.ram, C.byref(self.hooks), *converted)
        return function(self.ram, *converted)


class CMemory:
    def __init__(self, side):
        self.side = side

    def _view(self, address, size):
        physical = address & 0x1FFFFFFF
        if mr.SCRATCH_BASE <= physical < mr.SCRATCH_BASE + mr.SCRATCH_SIZE:
            offset = physical - mr.SCRATCH_BASE
            return self.side.scratch, offset
        return self.side.main, physical & 0x1FFFFF

    def read(self, address, size):
        view, offset = self._view(address, size)
        return int.from_bytes(bytes(view[offset:offset + size]), "little")

    def write(self, address, value, size):
        view, offset = self._view(address, size)
        view[offset:offset + size] = (value & ((1 << (size * 8)) - 1)).to_bytes(size, "little")


class MipsMemory:
    def __init__(self, cpu):
        self.cpu = cpu

    def read(self, address, size):
        return self.cpu.read(address, size)

    def write(self, address, value, size):
        self.cpu.write(address, size, value)


class SolidModel:
    """Deterministic GetSolidHeight hook with visible ordered side effects on both sides."""
    def __init__(self, seed, memory, result):
        self.seed = seed
        self.memory = memory
        self.result = result
        self.log = []

    def solid_call(self, memory, x, y, z):
        entry = (x & MASK32, y & MASK32, z & MASK32, self.result & MASK32)
        self.log.append(entry)
        counter = memory.read(HOOK_COUNTER, 4)
        memory.write(HOOK_COUNTER, counter + 1, 4)
        memory.write(HOOK_SCRATCH, (x ^ z ^ len(self.log) ^ self.seed) & 0xFFFF, 2)
        memory.write(HOOK_SCRATCH + 8, self.result, 2)
        return self.result


class ProbeHarness:
    def __init__(self, dll, metas):
        self.metas = metas
        self.bases = {}
        for area_id, meta in metas.items():
            cpu = mr.MipsRam.from_extract(meta["path"])
            self.bases[area_id] = cpu.snapshot()
        self.m = mr.MipsRam.from_extract(metas[AREA_IDS[0]]["path"])
        self.m.coverage = set()
        self.c = CSide(dll)
        self.counts = {}
        self.failures = 0
        self.hook_calls = 0
        reg.declare("stack_window", "801FF000..80200000")

    def sync_to_c(self):
        C.memmove(self.c.main, bytes(self.m.ram), mr.RAM_SIZE)
        C.memmove(self.c.scratch, bytes(self.m.scratch), mr.SCRATCH_SIZE)
        self.c.lib.ae_ram_clear_error(self.c.ram)

    def compare(self, name, label, mips_return, c_return, mips_log, c_log):
        problems = []
        if self.c.lib.ae_ram_failed(self.c.ram):
            problems.append("C error flag: " + self.c.lib.ae_ram_error(self.c.ram).decode())
        if (mips_return & MASK32) != (c_return & MASK32):
            problems.append(f"return differs: MIPS {mips_return & MASK32:08X}, C {c_return & MASK32:08X}")
        if mips_log != c_log:
            problems.append(f"GetSolidHeight hook log differs: MIPS {mips_log}, C {c_log}")
        mips_bytes = bytes(self.m.ram[:STACK_COMPARE_END])
        c_bytes = bytes(self.c.main[:STACK_COMPARE_END])
        if mips_bytes != c_bytes:
            differences = [i for i, (left, right) in enumerate(zip(mips_bytes, c_bytes)) if left != right]
            problems.append("RAM differs at " + ", ".join(
                f"{0x80000000 + off:08X}(MIPS {mips_bytes[off]:02X}, C {c_bytes[off]:02X})" for off in differences[:8]))
        if bytes(self.m.scratch) != bytes(self.c.scratch):
            differences = [i for i, (left, right) in enumerate(zip(self.m.scratch, self.c.scratch)) if left != right]
            problems.append("scratchpad differs at " + ", ".join(
                f"{mr.SCRATCH_BASE + off:08X}(MIPS {self.m.scratch[off]:02X}, C {self.c.scratch[off]:02X})"
                for off in differences[:8]))
        if problems:
            self.failures += 1
            print(f"FAIL {name} [{label}]")
            for problem in problems:
                print("   ", problem)
            return False
        return True

    def run(self, name, area_id, label, args, setup=None, solid=0, seed=0):
        meta = self.metas[area_id]
        self.m.restore(self.bases[area_id])
        install_area(self.m, meta)
        if setup:
            setup(self.m, meta)
        self.sync_to_c()

        mm = SolidModel(seed, MipsMemory(self.m), solid)
        cm = SolidModel(seed, CMemory(self.c), solid)
        self.c.model = cm
        self.m.hook_log.clear()
        hooks = {}
        if name == "BodyProbeGround":
            hooks[0x8004D8C0] = lambda cpu, regs: mm.solid_call(MipsMemory(cpu), regs[4], regs[5], regs[6])
        try:
            regs = self.m.call(FUNCS[name][0], args=args, hooks=hooks)
            mips_return = regs[2]
        except mr.MipsError as error:
            self.failures += 1
            print(f"FAIL {name} [{label}] original MIPS error: {error}")
            return False
        c_return = self.c.call(name, args)
        self.counts[name] = self.counts.get(name, 0) + 1
        self.hook_calls += len(mm.log)
        return self.compare(name, label, mips_return, c_return, mm.log, cm.log)


def write_body(cpu, *, x, y, z, vx=0, vy=0, vz=0, fill=None):
    if fill is not None:
        cpu.write_bytes(BODY_BUFFER, fill[:0x38])
    cpu.write32(BODY_BUFFER + 0x00, x)
    cpu.write32(BODY_BUFFER + 0x04, y)
    cpu.write32(BODY_BUFFER + 0x08, z)
    cpu.write32(BODY_BUFFER + 0x0C, vx)
    cpu.write32(BODY_BUFFER + 0x10, vy)
    cpu.write32(BODY_BUFFER + 0x14, vz)


def write_probe(cpu, x, y, z):
    cpu.write16(PROBE_BUFFER + 0x00, x)
    cpu.write16(PROBE_BUFFER + 0x02, y)
    cpu.write16(PROBE_BUFFER + 0x04, z)


def set_probe_storage(cpu, rng):
    cpu.write_bytes(PROBE_BUFFER, rng.randbytes(8 * 0x10))
    cpu.write_bytes(PROBE_FLAGS, rng.randbytes(256))


def default_scenarios(h, scale, seed, only=None):
    rng = random.Random(seed)

    def run(name, area, label, args, setup=None, solid=0):
        if only is None or name == only:
            h.run(name, area, label, args, setup, solid=solid, seed=seed ^ len(label))

    for area_id in AREA_IDS:
        meta = h.metas[area_id]
        hx, hz = meta["half_x"], meta["half_z"]
        sample_cells = []
        for candidate in (meta["cells"][0], meta["cells"][len(meta["cells"]) // 2], meta["cells"][-1]):
            if candidate not in sample_cells:
                sample_cells.append(candidate)

        # Direct grid lookup at actual cell centers, every available height band, and grid edges.
        for cell_index, (cx, cz, first_shape) in enumerate(sample_cells):
            x, z = area_cell_center(meta, cx, cz)
            chain = shape_chain(meta, first_shape)
            query_bands = {0, 1, 64}
            for _shape_index, band, height in chain:
                query_bands.add(max(0, band))
                if band > 0:
                    query_bands.add(band - 1)
                rounded_height = height + 511 if height < 0 else height
                span = rounded_height // 512
                query_bands.add(max(0, band + span - 1))
                query_bands.add(max(0, band + span))
            for band in sorted(query_bands):
                if band > 64:
                    continue
                y = -min(32768, band * 512)
                setup = lambda cpu, m, x=x, y=y, z=z: write_probe(cpu, x, y, z)
                run("CollisionPointInGrid", area_id, f"shape cell {cell_index} band {band}",
                    (PROBE_BUFFER,), setup)

        # Exercise the negative signed-height rounding arm with a real grid column and shape record.
        cx, cz, first_shape = sample_cells[0]
        x, z = area_cell_center(meta, cx, cz)
        first_band = max(0, shape_chain(meta, first_shape)[0][1])
        def negative_shape_height(cpu, m, x=x, z=z, band=first_band, shape=first_shape):
            shape_addr = cpu.read32(0x800F4558) + shape * 0x14
            cpu.write16(shape_addr + 0x08, -1)
            write_probe(cpu, x, -min(32768, (band + 1) * 512), z)
        run("CollisionPointInGrid", area_id, "negative shape half-height rounding",
            (PROBE_BUFFER,), negative_shape_height)

        edge_points = [
            (-hx, 0), (-hx + 1, 0), (hx - 1, 0), (hx, 0),
            (0, -hz), (0, -hz + 1), (0, hz - 1), (0, hz), (0, 0),
        ]
        for i, (x, z) in enumerate(edge_points):
            y = (1, 0, -1, -512)[i % 4]
            setup = lambda cpu, m, x=x, y=y, z=z: write_probe(cpu, x, y, z)
            run("CollisionPointInGrid", area_id, f"grid boundary {i}", (PROBE_BUFFER,), setup)

        empty = meta["empty_cells"]
        if empty:
            cx, cz, _ = empty[len(empty) // 2]
            x, z = area_cell_center(meta, cx, cz)
            for y in (-1, -512, -32768):
                setup = lambda cpu, m, x=x, y=y, z=z: write_probe(cpu, x, y, z)
                run("CollisionPointInGrid", area_id, f"empty grid column y={y}", (PROBE_BUFFER,), setup)

        # Build probes for all four body types at grid edges, around a real shape column,
        # outside the grid, and around real collider boxes.
        first_cell = sample_cells[0]
        cell_x, cell_z, first_shape = first_cell
        center_x, center_z = area_cell_center(meta, cell_x, cell_z)
        shape_band = shape_chain(meta, first_shape)[0][1]
        body_y = -max(0, shape_band) * 512 - 32
        positions = [
            (center_x, body_y, center_z),
            (-hx + 1, body_y, center_z), (-hx, body_y, center_z),
            (hx - 1, body_y, center_z), (hx + 1, body_y, center_z),
            (center_x, body_y, -hz + 1), (center_x, body_y, hz - 1),
            (-hx - 1, body_y, -hz - 1),
        ]
        for position_index, (x, y, z) in enumerate(positions):
            for type_index, body_type in enumerate(BODY_TYPES):
                vy = (0, 1, 64, 160)[(position_index + type_index) % 4]
                def setup(cpu, m, x=x, y=y, z=z, vy=vy, i=position_index, t=body_type):
                    local = random.Random(seed ^ (m["area"] << 16) ^ (i << 8) ^ t)
                    set_probe_storage(cpu, local)
                    write_body(cpu, x=x, y=y, z=z, vx=0, vy=vy, vz=0, fill=local.randbytes(0x38))
                run("CollisionBuildProbePoints", area_id, f"body type {body_type} position {position_index}",
                    (body_type, BODY_BUFFER), setup)

        # Exact collider box borders from the real area records.
        collider_count = meta["collider_count"]
        if collider_count:
            for collider_index in range(min(collider_count, 2)):
                record = meta["collider_data"] + collider_index * 0x38
                record_addr = mr.AREA_BASE + record
                cx = s16(meta["blob"], record + 0)
                cy = s16(meta["blob"], record + 2)
                cz = s16(meta["blob"], record + 4)
                ex = s16(meta["blob"], record + 6)
                ey = s16(meta["blob"], record + 8)
                ez = s16(meta["blob"], record + 10)
                for axis, extent in ((0, ex), (1, ey), (2, ez)):
                    for sign, border in ((-1, abs(extent) + 512), (1, abs(extent) + 511), (1, abs(extent) + 512)):
                        pos = [cx, cy, cz]
                        pos[axis] += sign * border
                        setup = lambda cpu, m, pos=tuple(pos): write_body(cpu, x=pos[0], y=pos[1], z=pos[2])
                        run("PointInBoxXYZ", area_id, f"collider {collider_index} axis {axis} border {sign}:{border}",
                            (record_addr, BODY_BUFFER), setup)
                setup = lambda cpu, m, cx=cx, cy=cy, cz=cz: write_body(cpu, x=cx, y=cy, z=cz)
                run("PointInBoxXYZ", area_id, f"collider {collider_index} center", (record_addr, BODY_BUFFER), setup)

        # One full Build case with collider count zero and one type-1 touched-marker path.
        def empty_collider_list(cpu, m):
            local = random.Random(seed ^ m["area"])
            set_probe_storage(cpu, local)
            cpu.write16(0x800F45D4, 0)
            write_body(cpu, x=0, y=-64, z=0, vy=64, fill=local.randbytes(0x38))
        run("CollisionBuildProbePoints", area_id, "empty collider list", (6, BODY_BUFFER), empty_collider_list)

        if collider_count:
            collider_addr = mr.AREA_BASE + meta["collider_data"]
            def marker_case(cpu, m, collider_addr=collider_addr, x=center_x, y=body_y, z=center_z):
                local = random.Random(seed ^ (m["area"] << 4))
                set_probe_storage(cpu, local)
                cpu.write16(collider_addr + COLLIDER_TYPE, 1)
                cpu.write16(collider_addr + COLLIDER_FLAGS, 0)
                write_body(cpu, x=x, y=y, z=z, vy=32, fill=local.randbytes(0x38))
            run("CollisionBuildProbePoints", area_id, "type-one collider marker", (1, BODY_BUFFER), marker_case)

            # The build routine has small body-kind dispatch blocks in addition to the four game types
            # that consume probes. Cover their remaining type arms against the same real area records.
            for body_type in (0, 2, 3, 4, 5):
                def dispatch_setup(cpu, m, body_type=body_type):
                    local = random.Random(seed ^ (m["area"] << 8) ^ body_type)
                    set_probe_storage(cpu, local)
                    write_body(cpu, x=center_x, y=body_y, z=center_z, vy=64, fill=local.randbytes(0x38))
                run("CollisionBuildProbePoints", area_id, f"body kind dispatch {body_type}",
                    (body_type, BODY_BUFFER), dispatch_setup)

        # Real heightfield data, near its bounds and outside it, with velocity thresholds and varied solid results.
        ground_points = [
            (0, -2048, 0),
            (-meta["terrain_half_x"] + 1, -4096, -meta["terrain_half_z"] + 1),
            (meta["terrain_half_x"] - 1, -1024, meta["terrain_half_z"] - 1),
            (-meta["terrain_half_x"] - 1, -128, 0),
            (0, -32, meta["terrain_half_z"] + 1),
        ]
        for point_index, (x, y, z) in enumerate(ground_points):
            for velocity_y in (0, 31, 32, 64, 160, 512):
                vx, vz = ((0, 0) if velocity_y == 0 else (256, -256))
                def setup(cpu, m, x=x, y=y, z=z, vy=velocity_y, vx=vx, vz=vz):
                    write_body(cpu, x=x, y=y, z=z, vx=vx, vy=vy, vz=vz)
                    cpu.write32(HOOK_COUNTER, 0)
                solid = (-32768, -2048, 0, 16384, 32767, 32767)[(velocity_y + point_index) % 6]
                run("BodyProbeGround", area_id, f"ground point {point_index} velocity {velocity_y}",
                    (BODY_BUFFER,), setup, solid=solid)

    # The shift primitive is independent of area data; seeded record contents make each write observable.
    for i in range(max(24, round(48 * scale))):
        dx, dz = ((512, -512), (-512, 512), (0x7FFF, -0x8000),
                  (rng.randint(-2048, 2048), rng.randint(-2048, 2048)))[i % 4]
        def shift_setup(cpu, meta, i=i):
            cpu.write_bytes(PROBE_BUFFER, random.Random(seed + i).randbytes(0x80))
        run("CollisionShiftProbePoints", AREA_IDS[0], f"shift #{i}", (dx, dz), shift_setup)

    # Synthetic strict-border cases complement the original collider boxes.
    for axis in range(3):
        for delta in (-513, -512, -511, 511, 512, 513):
            def box_setup(cpu, meta, axis=axis, delta=delta):
                cpu.write_bytes(BOX_BUFFER, bytes(0x38))
                for off, value in ((0, 0), (2, 0), (4, 0), (6, 0), (8, 0), (10, 0)):
                    cpu.write16(BOX_BUFFER + off, value)
                position = [0, 0, 0]
                position[axis] = delta
                write_body(cpu, x=position[0], y=position[1], z=position[2])
            run("PointInBoxXYZ", AREA_IDS[0], f"expanded box axis {axis} at {delta}",
                (BOX_BUFFER, BODY_BUFFER), box_setup)

    # Additional deterministic random query and box inputs broaden signed-coordinate coverage.
    for i in range(max(16, round(32 * scale))):
        area_id = AREA_IDS[i % len(AREA_IDS)]
        meta = h.metas[area_id]
        x = rng.randint(-meta["half_x"] - 1024, meta["half_x"] + 1024)
        z = rng.randint(-meta["half_z"] - 1024, meta["half_z"] + 1024)
        y = rng.choice([1, 0, -1, -511, -512, -513, -4096, -32768])
        setup = lambda cpu, m, x=x, y=y, z=z: write_probe(cpu, x, y, z)
        run("CollisionPointInGrid", area_id, f"random grid #{i}", (PROBE_BUFFER,), setup)

        def random_box(cpu, m, i=i):
            local = random.Random(seed + 0x9000 + i)
            cpu.write_bytes(BOX_BUFFER, local.randbytes(0x38))
            write_body(cpu, x=local.randint(-32768, 32767), y=local.randint(-32768, 32767),
                       z=local.randint(-32768, 32767))
        run("PointInBoxXYZ", area_id, f"random box #{i}", (BOX_BUFFER, BODY_BUFFER), random_box)


def replace_in_function(source, function_name, before, after):
    name_at = source.index(function_name + "(")
    opening = source.index("{", name_at)
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
        raise RuntimeError(f"could not find end of {function_name}")
    body = source[opening:closing]
    if before not in body:
        raise RuntimeError(f"mutation anchor not found in {function_name}: {before!r}")
    return source[:opening] + body.replace(before, after, 1) + source[closing:]


def replace_once(source, before, after):
    if before not in source:
        raise RuntimeError(f"mutation anchor not found: {before!r}")
    return source.replace(before, after, 1)


def grid_case_setup(area_id, band_delta=0, half_y=None, cell=None):
    def setup(cpu, meta):
        cell_x, cell_z, shape_index = cell or meta["cells"][0]
        x, z = area_cell_center(meta, cell_x, cell_z)
        band = s16(meta["blob"], meta["shape_data"] + shape_index * 0x14 + 2)
        if half_y is not None:
            shape = cpu.read32(0x800F4558) + shape_index * 0x14
            cpu.write16(shape + 0x08, half_y)
        query_band = max(0, band) + band_delta
        write_probe(cpu, x, -min(32768, query_band * 512), z)
    return setup


def adjacent_different_cell(meta):
    by_cell = {(x, z): shape for x, z, shape in meta["cells"]}
    for x, z, shape in meta["cells"]:
        if x + 1 < meta["collision_width"] and by_cell.get((x + 1, z), -1) != shape:
            return x, z, shape
    raise RuntimeError(f"area {meta['area']:02X} has no horizontal collision-cell boundary")


def mutation_specs(metas, seed):
    source = (ROOT / "src" / "bodies" / "ae_probe.c").read_text(encoding="utf-8")
    specs = []

    def add(label, target, source_text, area, args, setup, solid=0):
        specs.append(dict(label=label, target=target, source=source_text, area=area,
                          args=args, setup=setup, solid=solid))

    # CollisionShiftProbePoints: eight independently observable field / delta mutations.
    shift_first = "ae_rd16(r, probes + AE_PROBE_SHIFT_Z_PROBE_1) + (uint32_t)dz"
    add("shift first Z uses dx", "CollisionShiftProbePoints",
        replace_in_function(source, "ae_CollisionShiftProbePoints", shift_first,
                            "ae_rd16(r, probes + AE_PROBE_SHIFT_Z_PROBE_1) + (uint32_t)dx"),
        AREA_IDS[0], (0x1234, -0x2345), lambda cpu, m: cpu.write_bytes(PROBE_BUFFER, bytes(range(128))))
    add("shift first write targets wrong halfword", "CollisionShiftProbePoints",
        replace_in_function(source, "ae_CollisionShiftProbePoints", "probes + AE_PROBE_SHIFT_Z_PROBE_1,",
                            "probes + AE_PROBE_SHIFT_Z_PROBE_1 + 2u,"),
        AREA_IDS[0], (512, -512), lambda cpu, m: cpu.write_bytes(PROBE_BUFFER, bytes(range(128))))
    add("shift first Z store widened", "CollisionShiftProbePoints",
        replace_in_function(source, "ae_CollisionShiftProbePoints", "ae_wr16(r, probes + AE_PROBE_SHIFT_Z_PROBE_1,",
                            "ae_wr32(r, probes + AE_PROBE_SHIFT_Z_PROBE_1,"),
        AREA_IDS[0], (512, -512), lambda cpu, m: cpu.write_bytes(PROBE_BUFFER, bytes(range(128))))
    add("shift second Z uses dx", "CollisionShiftProbePoints",
        replace_in_function(source, "ae_CollisionShiftProbePoints",
            "ae_rd16(r, probes + AE_PROBE_SHIFT_Z_PROBE_2) + (uint32_t)dz",
            "ae_rd16(r, probes + AE_PROBE_SHIFT_Z_PROBE_2) + (uint32_t)dx"),
        AREA_IDS[0], (0x1234, -0x2345), lambda cpu, m: cpu.write_bytes(PROBE_BUFFER, bytes(range(128))))
    add("shift second X subtracts dx", "CollisionShiftProbePoints",
        replace_in_function(source, "ae_CollisionShiftProbePoints",
            "ae_rd16(r, probes + AE_PROBE_SHIFT_X_PROBE_2) + (uint32_t)dx",
            "ae_rd16(r, probes + AE_PROBE_SHIFT_X_PROBE_2) - (uint32_t)dx"),
        AREA_IDS[0], (0x1234, -0x2345), lambda cpu, m: cpu.write_bytes(PROBE_BUFFER, bytes(range(128))))
    add("shift third X overwrites second X", "CollisionShiftProbePoints",
        replace_in_function(source, "ae_CollisionShiftProbePoints", "probes + AE_PROBE_SHIFT_X_PROBE_3,",
                            "probes + AE_PROBE_SHIFT_X_PROBE_2,"),
        AREA_IDS[0], (-513, 257), lambda cpu, m: cpu.write_bytes(PROBE_BUFFER, bytes(range(128))))
    add("shift fifth X uses dz", "CollisionShiftProbePoints",
        replace_in_function(source, "ae_CollisionShiftProbePoints",
            "ae_rd16(r, probes + AE_PROBE_SHIFT_X_PROBE_5) + (uint32_t)dx",
            "ae_rd16(r, probes + AE_PROBE_SHIFT_X_PROBE_5) + (uint32_t)dz"),
        AREA_IDS[0], (0x1234, -0x2345), lambda cpu, m: cpu.write_bytes(PROBE_BUFFER, bytes(range(128))))
    add("shift return value changes", "CollisionShiftProbePoints",
        replace_in_function(source, "ae_CollisionShiftProbePoints", "    return 0;", "    return 1;"),
        AREA_IDS[0], (512, 512), lambda cpu, m: cpu.write_bytes(PROBE_BUFFER, bytes(range(128))))

    # PointInBoxXYZ: strict bounds, signed absolute difference, and axis-field selection.
    point_inside = lambda cpu, meta: (cpu.write_bytes(BOX_BUFFER, bytes(0x38)),
                                      write_body(cpu, x=0, y=0, z=0))
    def point_case(*, center=(0, 0, 0), half=(0, 0, 0), position=(0, 0, 0)):
        def setup(cpu, meta):
            cpu.write_bytes(BOX_BUFFER, bytes(0x38))
            for offset, value in zip((0, 2, 4, 6, 8, 10), (*center, *half)):
                cpu.write16(BOX_BUFFER + offset, value)
            write_body(cpu, x=position[0], y=position[1], z=position[2])
        return setup
    add("box expanded comparison becomes inclusive", "PointInBoxXYZ",
        replace_in_function(source, "probe_axis_inside", "return difference < expanded_half;",
                            "return difference <= expanded_half;"),
        AREA_IDS[0], (BOX_BUFFER, BODY_BUFFER), point_case(position=(512, 0, 0)))
    add("box expansion loses one unit", "PointInBoxXYZ",
        replace_in_function(source, "probe_axis_inside", "probe_wrap_add(half_extent, AE_PROBE_GRID_CELL_SIZE)",
                            "probe_wrap_add(half_extent, AE_PROBE_GRID_CELL_SIZE - 1)"),
        AREA_IDS[0], (BOX_BUFFER, BODY_BUFFER), point_case(position=(511, 0, 0)))
    add("box absolute difference keeps negative sign", "PointInBoxXYZ",
        replace_in_function(source, "probe_axis_inside", "if (difference < 0)", "if (difference > 0)"),
        AREA_IDS[0], (BOX_BUFFER, BODY_BUFFER), point_case(position=(600, 0, 0)))
    add("box X axis reads Y half extent", "PointInBoxXYZ",
        replace_in_function(source, "ae_PointInBoxXYZ", "AE_PROBE_COLLIDER_HALF_X,\n                           AE_PROBE_BODY_X",
                            "AE_PROBE_COLLIDER_HALF_Y,\n                           AE_PROBE_BODY_X"),
        AREA_IDS[0], (BOX_BUFFER, BODY_BUFFER), point_case(half=(0, 1024, 0), position=(600, 0, 0)))
    add("box X axis reads Y position", "PointInBoxXYZ",
        replace_in_function(source, "ae_PointInBoxXYZ", "AE_PROBE_BODY_X))",
                            "AE_PROBE_BODY_Y))"),
        AREA_IDS[0], (BOX_BUFFER, BODY_BUFFER), point_case(position=(600, 0, 0)))
    add("box Y axis reads Z center", "PointInBoxXYZ",
        replace_in_function(source, "ae_PointInBoxXYZ", "AE_PROBE_COLLIDER_Y, AE_PROBE_COLLIDER_HALF_Y",
                            "AE_PROBE_COLLIDER_Z, AE_PROBE_COLLIDER_HALF_Y"),
        AREA_IDS[0], (BOX_BUFFER, BODY_BUFFER),
        point_case(center=(0, 1000, 0), half=(1024, 0, 1024), position=(0, 1000, 0)))
    add("box X early reject condition reverses", "PointInBoxXYZ",
        replace_in_function(source, "ae_PointInBoxXYZ", "if (!probe_axis_inside(r, box, point, AE_PROBE_COLLIDER_X",
                            "if (probe_axis_inside(r, box, point, AE_PROBE_COLLIDER_X"),
        AREA_IDS[0], (BOX_BUFFER, BODY_BUFFER), point_inside)
    add("box overlap return changes", "PointInBoxXYZ",
        replace_in_function(source, "ae_PointInBoxXYZ", "    return 0;", "    return -1;"),
        AREA_IDS[0], (BOX_BUFFER, BODY_BUFFER), point_inside)

    # CollisionPointInGrid: grid bounds, rounding, bands, shape links and record scale.
    add("grid X upper bound becomes inclusive", "CollisionPointInGrid",
        replace_in_function(source, "ae_CollisionPointInGrid", "x < half_x", "x <= half_x"),
        0x01, (PROBE_BUFFER,), lambda cpu, m: write_probe(cpu, m["half_x"], 0, 0))
    add("grid Z upper bound becomes inclusive", "CollisionPointInGrid",
        replace_in_function(source, "ae_CollisionPointInGrid", "z < half_z", "z <= half_z"),
        0x01, (PROBE_BUFFER,), lambda cpu, m: write_probe(cpu, 0, 0, m["half_z"]))
    add("grid rejects zero height", "CollisionPointInGrid",
        replace_in_function(source, "ae_CollisionPointInGrid", "if (y > 0) return 0;", "if (y >= 0) return 0;"),
        0x01, (PROBE_BUFFER,), grid_case_setup(0x01))
    add("grid X cell rounds one cell too far", "CollisionPointInGrid",
        replace_in_function(source, "ae_CollisionPointInGrid",
            "probe_round_cell(probe_wrap_add(x, half_x))",
            "probe_round_cell(probe_wrap_add(probe_wrap_add(x, half_x), 512))"),
        0x65, (PROBE_BUFFER,), grid_case_setup(0x65, cell=adjacent_different_cell(metas[0x65])))
    add("grid X origin uses Z half-size", "CollisionPointInGrid",
        replace_in_function(source, "ae_CollisionPointInGrid",
            "int32_t half_x = ae_rds32(r, AE_G_COLL_HALF_X);",
            "int32_t half_x = ae_rds32(r, AE_G_COLL_HALF_Z);"),
        0x65, (PROBE_BUFFER,), grid_case_setup(0x65))
    add("grid exact shape band is rejected", "CollisionPointInGrid",
        replace_in_function(source, "ae_CollisionPointInGrid", "if (query_band < shape_band) return 0;",
                            "if (query_band <= shape_band) return 0;"),
        0x05, (PROBE_BUFFER,), grid_case_setup(0x05))
    add("grid top band loses lower endpoint", "CollisionPointInGrid",
        replace_in_function(source, "ae_CollisionPointInGrid",
            "probe_wrap_sub(probe_wrap_add(shape_band, probe_sra(rounded_height, 9)), 1)",
            "probe_wrap_sub(probe_wrap_add(shape_band, probe_sra(rounded_height, 9)), 0)"),
        0x01, (PROBE_BUFFER,), grid_case_setup(0x01, band_delta=1, half_y=512))
    add("grid treats shape index zero as empty", "CollisionPointInGrid",
        replace_in_function(source, "ae_CollisionPointInGrid", "if (shape_index < 0) return 0;",
                            "if (shape_index <= 0) return 0;"),
        0x05, (PROBE_BUFFER,), grid_case_setup(0x05))

    # CollisionBuildProbePoints: body velocity, probe loops, quadrant, marker, collider filtering.
    def build_setup(area_id, *, x=0, y=-64, z=0, vy=64, collider_type=None,
                    collider_flags=None, fill_seed=41):
        def setup(cpu, meta):
            local = random.Random(seed ^ (area_id << 12) ^ fill_seed)
            set_probe_storage(cpu, local)
            if collider_type is not None or collider_flags is not None:
                first = cpu.read32(0x800F455C)
                if collider_type is not None:
                    cpu.write16(first + COLLIDER_TYPE, collider_type)
                if collider_flags is not None:
                    cpu.write16(first + COLLIDER_FLAGS, collider_flags)
            write_body(cpu, x=x, y=y, z=z, vy=vy, fill=local.randbytes(0x38))
        return setup
    add("build drops positive Y velocity adjustment", "CollisionBuildProbePoints",
        replace_in_function(source, "ae_CollisionBuildProbePoints", "if (velocity_y > 0)",
                            "if (velocity_y < 0)"),
        0x01, (6, BODY_BUFFER), build_setup(0x01, x=0, y=-64, z=0, vy=64))
    add("build initializes only seven probes", "CollisionBuildProbePoints",
        replace_in_function(source, "ae_CollisionBuildProbePoints", "index < 8", "index < 7"),
        0x01, (6, BODY_BUFFER), build_setup(0x01, x=0, y=-64, z=0, vy=64))
    add("build selects wrong quadrant shift", "CollisionBuildProbePoints",
        replace_in_function(source, "ae_CollisionBuildProbePoints",
            "case 1: shift_x = 512; shift_z = -512; break;",
            "case 1: shift_x = -512; shift_z = -512; break;"),
        0x01, (6, BODY_BUFFER), build_setup(0x01, x=-4864, y=-64, z=-5120, vy=64))
    add("build resolves only seven probe records", "CollisionBuildProbePoints",
        replace_in_function(source, "ae_CollisionBuildProbePoints", "int32_t probe_index = 7;",
                            "int32_t probe_index = 6;"),
        0x01, (6, BODY_BUFFER), build_setup(0x01, x=0, y=-64, z=0, vy=64))
    add("build visits one collider past the count", "CollisionBuildProbePoints",
        replace_in_function(source, "ae_CollisionBuildProbePoints",
            "ae_rd16(r, AE_G_COLLIDER_COUNT) - 1u", "ae_rd16(r, AE_G_COLLIDER_COUNT)"),
        0x01, (6, BODY_BUFFER), build_setup(0x01, x=30000, y=30000, z=30000, vy=64))
    add("build skips collider type thirteen", "CollisionBuildProbePoints",
        replace_once(source, "#define AE_PROBE_COLLIDER_SKIP_TYPE 14",
                     "#define AE_PROBE_COLLIDER_SKIP_TYPE 13"),
        0x01, (6, BODY_BUFFER), build_setup(0x01, x=30000, y=30000, z=30000, vy=64, collider_type=13))
    add("build uses wrong disabled flag", "CollisionBuildProbePoints",
        replace_once(source, "#define AE_PROBE_COLLIDER_DISABLED 0x8000u",
                     "#define AE_PROBE_COLLIDER_DISABLED 0x4000u"),
        0x01, (6, BODY_BUFFER), build_setup(0x01, collider_flags=0x4000))
    add("build writes type-one marker to wrong field", "CollisionBuildProbePoints",
        replace_in_function(source, "ae_CollisionBuildProbePoints",
            "AE_PROBE_COLLIDER_BODY_MARKER, 0xFFFFu", "AE_PROBE_COLLIDER_BODY_MARKER - 2u, 0xFFFFu"),
        0x01, (1, BODY_BUFFER), build_setup(0x01, collider_type=1))
    add("build inverts non-overlap collider status", "CollisionBuildProbePoints",
        replace_in_function(source, "ae_CollisionBuildProbePoints",
            "ae_PointInBoxXYZ(r, box, body) != 0", "ae_PointInBoxXYZ(r, box, body) == 0"),
        0x01, (6, BODY_BUFFER), build_setup(0x01, x=30000, y=30000, z=30000, vy=64))

    # BodyProbeGround: threshold, step, query position, hit test, loop exit and write-back.
    ground_case = lambda cpu, meta: write_body(cpu, x=100000, y=0, z=-100000, vx=256, vy=64, vz=-256)
    ground_in_bounds = lambda cpu, meta: write_body(cpu, x=1000, y=0, z=-2000, vx=256, vy=64, vz=-256)
    def flat_ground_case(height):
        def setup(cpu, meta):
            vertex_count = (meta["terrain_width"] + 1) * (meta["terrain_depth"] + 1)
            center_count = meta["terrain_width"] * meta["terrain_depth"]
            heights = mr.AREA_BASE + meta["terrain_heights"]
            for index in range(vertex_count + center_count):
                cpu.write16(heights + index * 2, height)
            write_body(cpu, x=0, y=0, z=0, vx=256, vy=64, vz=-256)
        return setup
    add("ground velocity threshold moves down", "BodyProbeGround",
        replace_in_function(source, "ae_BodyProbeGround", "AE_PROBE_BODY_GROUND_MIN_VELOCITY) < 0",
                            "AE_PROBE_BODY_GROUND_MIN_VELOCITY - 1) < 0"),
        0x01, (BODY_BUFFER,), lambda cpu, m: write_body(cpu, x=1000, y=0, z=-2000, vy=31))
    add("ground starts at height thirty-one", "BodyProbeGround",
        replace_in_function(source, "ae_BodyProbeGround", "int32_t probe_height = AE_PROBE_BODY_GROUND_STEP;",
                            "int32_t probe_height = AE_PROBE_BODY_GROUND_STEP - 1;"),
        0x01, (BODY_BUFFER,), ground_case, solid=-32768)
    add("ground query omits X velocity", "BodyProbeGround",
        replace_in_function(source, "ae_BodyProbeGround",
            "ae_rds32(r, body + AE_PROBE_BODY_X),\n                                   ae_rds32(r, body + AE_PROBE_BODY_VELOCITY_X)",
            "ae_rds32(r, body + AE_PROBE_BODY_X),\n                                   0"),
        0x01, (BODY_BUFFER,), ground_in_bounds, solid=-32768)
    add("ground query subtracts Y step", "BodyProbeGround",
        replace_in_function(source, "ae_BodyProbeGround",
            "ae_rds32(r, body + AE_PROBE_BODY_Y), probe_height",
            "ae_rds32(r, body + AE_PROBE_BODY_Y), -probe_height"),
        0x01, (BODY_BUFFER,), ground_case, solid=-32768)
    add("ground query omits Z velocity", "BodyProbeGround",
        replace_in_function(source, "ae_BodyProbeGround",
            "ae_rds32(r, body + AE_PROBE_BODY_Z),\n                                   ae_rds32(r, body + AE_PROBE_BODY_VELOCITY_Z)",
            "ae_rds32(r, body + AE_PROBE_BODY_Z),\n                                   0"),
        0x01, (BODY_BUFFER,), ground_in_bounds, solid=-32768)
    add("ground hit boundary becomes inclusive", "BodyProbeGround",
        replace_in_function(source, "ae_BodyProbeGround", "if (ground < y)", "if (ground <= y)"),
        0x01, (BODY_BUFFER,), flat_ground_case(32), solid=100)
    add("ground loop exits at equal velocity", "BodyProbeGround",
        replace_in_function(source, "ae_BodyProbeGround",
            "probe_height) < 0) return 0;", "probe_height) <= 0) return 0;"),
        0x01, (BODY_BUFFER,), flat_ground_case(64), solid=100)
    add("ground write-back adds an extra step", "BodyProbeGround",
        replace_in_function(source, "ae_BodyProbeGround",
            "body + AE_PROBE_BODY_VELOCITY_Y, (uint32_t)probe_height",
            "body + AE_PROBE_BODY_VELOCITY_Y, (uint32_t)(probe_height + 32)"),
        0x01, (BODY_BUFFER,), ground_case, solid=-32768)

    return specs


def mutation_tests(metas, seed):
    specs = mutation_specs(metas, seed)
    output = ROOT / "extract" / "mips"
    output.mkdir(parents=True, exist_ok=True)
    caught = []
    survivors = []
    for index, spec in enumerate(specs, 1):
        source_path = output / f"probe_mutation_{os.getpid()}_{index:02d}.c"
        source_path.write_text(spec["source"], encoding="utf-8")
        dll = build_dll(source_path, tag=f"probe_mutation_{index:02d}")
        source_path.unlink()
        harness = ProbeHarness(dll, metas)
        with contextlib.redirect_stdout(io.StringIO()):
            harness.run(spec["target"], spec["area"], spec["label"], spec["args"],
                        spec["setup"], solid=spec["solid"], seed=seed ^ index)
        (caught if harness.failures else survivors).append(spec["label"])
        print(f"  {'CAUGHT' if harness.failures else 'SURVIVED'} {spec['label']}")
    print(f"mutation score: {len(caught)}/{len(specs)} caught")
    if survivors:
        print("surviving mutations: " + "; ".join(survivors))
    return len(specs) >= 40 and not survivors


def finish(h, show_coverage, default_run):
    items = [(name, addr, size, True) for name, (addr, size, _cname, _kinds) in FUNCS.items()]
    summary = reg.coverage_summary(items, h.m.coverage)
    if show_coverage:
        reg.print_coverage(summary)
    gaps = ([name for name, row in summary.items() if row["uncovered_count"] or row["stale_declared"]]
            if default_run else [])
    if gaps:
        print("FAIL: uncovered target instructions or stale unreachable declarations: " + ", ".join(gaps))
    if default_run:
        reg.write_results(h.counts, summary, not h.failures and not gaps, function_names=list(FUNCS))
    return not gaps


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=float, default=1.0, help="scale deterministic random-case groups")
    parser.add_argument("--seed", type=int, default=94423)
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true", help="compile and run the 41 deliberate wrong-code variants")
    parser.add_argument("--only", choices=tuple(FUNCS), help="run one ported function (no ledger result write)")
    args = parser.parse_args()
    if not (ROOT / "extract/disc/SCUS_944.23").exists():
        print("ERROR: extract/disc/SCUS_944.23 is required")
        return 2
    started = time.time()
    metas = area_metas()
    if args.mutate:
        mutation_ok = mutation_tests(metas, args.seed)
        print(f"mutation validation completed in {time.time() - started:.1f}s")
        return 0 if mutation_ok else 1
    h = ProbeHarness(build_dll(), metas)
    default_scenarios(h, args.cases, args.seed, args.only)
    print("cases per function:")
    for name in FUNCS:
        print(f"  {name:32s} {h.counts.get(name, 0)}")
    print(f"total cases {sum(h.counts.values())}, GetSolidHeight calls {h.hook_calls}, "
          f"original MIPS instructions {h.m.steps}, {time.time() - started:.1f}s")
    coverage_ok = finish(h, args.coverage, default_run=(args.only is None
                                                        and args.cases == 1.0 and args.seed == 94423))
    if h.failures:
        print(f"FAILED: {h.failures} differential mismatches")
        return 1
    if not coverage_ok:
        return 1
    print("OK: probe C matches the original instructions for every case")
    return 0


if __name__ == "__main__":
    sys.exit(main())
