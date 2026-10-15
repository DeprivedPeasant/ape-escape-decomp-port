"""Differentially check the item loader and render-state setup ports against SCUS-94423."""
import argparse
import ctypes as C
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas
import exclusion_registry as reg
import mips_ram as mr

STACK_LO = 0x1FF000
STATE = 0x800F4350
ITEM_HEADER_FIELD = 0x248
ITEM_AREA_FIELD = 0x126
ITEM_LEVEL_FIELD = 0x124
ITEM_MASK_TABLE = 0x800BF2B8
ITEM_FLAGS = 0x800BF2D8
COLLECTED_TABLE = 0x800DFB70
PICKUP_TABLE = 0x800BEFA0
PICKUP_SIZE = 24
GTE_MATRIX = 0x800EBFF0
GTE_MATRIX_SOURCE = 0x800E78E0
LEVEL_GRID = 0x800EBFA0
FOG_SCRATCH = 0x1F8003C0
AREA_BASE = 0x80154000
M32 = 0xFFFFFFFF
EXTRA_COVERAGE = {}

FUNCS = {
    "SetColorMatrixMode": (0x80015D58, 460, "v", "uuuu", True),
    "MatrixCopy8": (0x80015F24, 68, "v", "uu", False),
    "ApplyLevelFog": (0x800323F8, 84, "v", "", True),
    "ItemsLoadForArea": (0x80083470, 624, "v", "", True),
    "PickupSpawnSlot": (0x80083844, 612, "v", "uuss", True),
    "CollectedItemsHas": (0x80084904, 96, "s", "u", False),
    "GetCollectedItemsTable": (0x800962E8, 12, "u", "", False),
}

def sdk_model(name, args, events):
    """Public SDK boundary: retain the exact argument words without SDK side effects."""
    events.append((name, tuple(value & M32 for value in args)))
    return 0


def sdk_trig(name, angle, ram, events):
    """Table-backed host model shared by the two SDK API interceptors."""
    sdk_model(name, (angle,), events)
    phase = angle & 0xFFF
    if name == "rcos": phase = (phase + 1024) & 0xFFF
    quadrant, offset = divmod(phase, 1024)
    index = offset if quadrant % 2 == 0 else 1024 - offset
    start = 0xA9DB0 + index * 2
    value = int.from_bytes(bytes(ram[start:start+2]), "little", signed=True)
    return -value if quadrant >= 2 else value


def build(source=None, tag="items_init"):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{os.getpid()}_{time.time_ns()}.dll"
    subprocess.run([
        shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections", "-shared",
        "-static-libgcc", "-I" + str(ROOT / "src/world"),
        str(ROOT / "src/core/ae_ram.c"),
        str(source or ROOT / "src/world/ae_items_init.c"), "-o", str(dll),
    ], check=True)
    return dll


class CSide:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        P, U = C.c_void_p, C.c_uint32
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
        self.external_events = []
        self.context = {}

        Call = C.CFUNCTYPE(C.c_int32, P, P, U, U, U, U)
        Color = C.CFUNCTYPE(None, P, P, U, U, U)
        Matrix = C.CFUNCTYPE(None, P, P, U)
        Trig = C.CFUNCTYPE(C.c_int32, P, P, C.c_int32)
        self.hooks_type = type("ItemsInitHooks", (C.Structure,), {
            "_fields_": [("user", P), ("GetLevelFirstArea", Call), ("printf", Call),
                         ("SetBackColor", Color),
                         ("SetFarColor", Color),
                         ("SetColorMatrix", Matrix), ("rsin", Trig), ("rcos", Trig)]
        })
        self.hooks = self.hooks_type()
        self.callbacks = []

        def get_level(_user, _ram, level, _a1, _a2, _a3):
            self.external_events.append(("GetLevelFirstArea", (level,)))
            return self.context.get("first_area", 1)

        def printf(_user, _ram, format_ptr, _a1, _a2, _a3):
            self.external_events.append(("printf", (format_ptr,)))
            return 0

        def background(_user, _ram, red, green, blue):
            sdk_model("SetBackColor", (red, green, blue), self.external_events)
        def far_color(_user, _ram, red, green, blue):
            sdk_model("SetFarColor", (red, green, blue), self.external_events)
        def color_matrix(_user, _ram, matrix):
            sdk_model("SetColorMatrix", (matrix,), self.external_events)

        for name, fn in (("GetLevelFirstArea", get_level), ("printf", printf),
                         ("SetBackColor", background),
                         ("SetFarColor", far_color),
                         ("SetColorMatrix", color_matrix)):
            callback_type = Call if name in ("GetLevelFirstArea", "printf") else (
                Color if name != "SetColorMatrix" else Matrix)
            callback = callback_type(fn)
            self.callbacks.append(callback)
            setattr(self.hooks, name, callback)

        for trig in ("rsin", "rcos"):
            callback = Trig(lambda _user, _ram, angle, label=trig:
                            sdk_trig(label, angle, self.main, self.external_events))
            self.callbacks.append(callback)
            setattr(self.hooks, trig, callback)

        self.argtypes = {name: (P, *([C.POINTER(self.hooks_type)] if data[4] else []),
                                *[C.c_int32 if kind == "s" else U for kind in data[3]])
                         for name, data in FUNCS.items()}
        for name, (_, _, ret, _, _) in FUNCS.items():
            fn = getattr(self.lib, "ae_" + name)
            fn.argtypes = list(self.argtypes[name])
            fn.restype = {"v": None, "s": C.c_int32, "u": U}[ret]

    def sync(self, mips):
        C.memmove(self.main, bytes(mips.ram), mr.RAM_SIZE)
        C.memmove(self.scratch, bytes(mips.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)

    def invoke(self, name, args):
        fn = getattr(self.lib, "ae_" + name)
        _, _, _, kinds, takes_hooks = FUNCS[name]
        converted = [C.c_int32(value) if kind == "s" else C.c_uint32(value & M32)
                     for value, kind in zip(args, kinds)]
        return fn(self.ram, *([C.byref(self.hooks)] if takes_hooks else []), *converted)


class CheckFailure(Exception):
    pass


class ItemsHarness:
    def __init__(self, source=None, tag="items_init"):
        reg.declare("stack_window", "801FF000..80200000")
        self.m = mr.MipsRam.from_extract()
        self.m.coverage = set()
        self.base = self.m.snapshot()
        self.c = CSide(build(source, tag))
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0
        self.members = dump_areas.area_members()
        self.area_blobs = {}

    def area_blob(self, area):
        if area not in self.area_blobs:
            member = self.members[area]
            self.area_blobs[area] = (ROOT / f"extract/members/{member:03X}.bin").read_bytes()
        return self.area_blobs[area]

    def run(self, name, args=(), setup=lambda _m: None, context=None, area=None):
        self.m.restore(self.base)
        if area is not None:
            self.m.load_area(self.area_blob(area))
        setup(self.m)
        self.c.sync(self.m)
        self.c.context = context or {"first_area": 1}
        self.c.external_events = []

        def get_level(cpu, regs):
            self.m_hook_events.append(("GetLevelFirstArea", (regs[4],)))
            return self.c.context.get("first_area", 1)

        def printf(cpu, regs):
            self.m_hook_events.append(("printf", (regs[4],)))
            return 0

        self.m_hook_events = []
        mips_hooks = {0x8009764C: get_level, 0x800A3720: printf}
        for sdk_name, address, arity in (("SetBackColor", 0x800315E0, 3),
                                         ("SetFarColor", 0x80031630, 3),
                                         ("SetColorMatrix", 0x80031600, 1)):
            mips_hooks[address] = lambda cpu, regs, label=sdk_name, count=arity: sdk_model(label, tuple(regs[4:4+count]), self.m_hook_events)
        for trig, address in (("rsin", 0x8002FB50), ("rcos", 0x8002FC20)):
            mips_hooks[address] = lambda cpu, regs, label=trig: sdk_trig(label, regs[4], cpu.ram, self.m_hook_events)
        registers = self.m.call(FUNCS[name][0], args, hooks=mips_hooks)
        result = self.c.invoke(name, args)
        self.counts[name] += 1
        self.compare(name, args, registers[2], result)

    def compare(self, name, args, original, result):
        ret_kind = FUNCS[name][2]
        if ret_kind != "v":
            expected = original & M32
            actual = result & M32
            if actual != expected:
                self.fail(name, args, f"return differs: C={actual:08X} MIPS={expected:08X}")
        expected = bytes(self.m.ram[:STACK_LO])
        actual = C.string_at(self.c.main, STACK_LO)
        if actual != expected:
            index = next(i for i, (a, b) in enumerate(zip(actual, expected)) if a != b)
            lo, hi = max(0, index - 8), min(len(actual), index + 16)
            self.fail(name, args, f"RAM differs at {0x80000000 + index:08X}: C={actual[index]:02X} MIPS={expected[index]:02X}; "
                      f"nearby C={actual[lo:hi].hex()} MIPS={expected[lo:hi].hex()}")
        if bytes(self.c.scratch) != bytes(self.m.scratch):
            self.fail(name, args, "scratchpad differs")
        if self.c.external_events != self.m_hook_events:
            self.fail(name, args, f"external hook sequence differs: C={self.c.external_events!r}; MIPS={self.m_hook_events!r}")
        if self.c.lib.ae_ram_failed(self.c.ram):
            self.fail(name, args, self.c.lib.ae_ram_error(self.c.ram).decode(errors="replace"))

    def fail(self, name, args, detail):
        self.failures += 1
        raise CheckFailure(f"FAIL {name}{tuple(args)}: {detail}")

    def finish(self, extra_coverage, default_run=False, show_coverage=False):
        items = [(name, data[0], data[1], True) for name, data in FUNCS.items()]
        summary = reg.coverage_summary(items, self.m.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        gaps = any(row["uncovered_count"] or row["stale_declared"] for row in summary.values())
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps,
                              extra=extra_coverage, function_names=list(FUNCS))
        if gaps:
            raise CheckFailure("FAIL: undeclared instruction coverage gaps")
        return summary


def _write_collected_table(m, entries=(), sentinel=True):
    for index in range(128):
        m.write16(COLLECTED_TABLE + index * 2, 0x00FE)
    if sentinel:
        m.write16(COLLECTED_TABLE + 127 * 2, 0x00FF)
    for index, value in entries:
        m.write16(COLLECTED_TABLE + index * 2, value)


def setup_collected(entries=(), sentinel=True):
    return lambda m: _write_collected_table(m, entries, sentinel)


def setup_matrix_copy(m, destination=0x801B1004, source=0x801B1000):
    m.write_bytes(destination, bytes([0xA5] * 40))
    m.write_bytes(source, bytes((index * 37 + 11) & 0xFF for index in range(32)))


def setup_color_matrix(m, state_word=0):
    m.write32(STATE + 0x27C, state_word)
    m.write_bytes(GTE_MATRIX_SOURCE,
                  bytes((index * 13 + 7) & 0xFF for index in range(32)))
    m.write_bytes(GTE_MATRIX, bytes([0x6D] * 32))


def setup_pickup(m, mode, slot=0, seed_a=0x2345, seed_b=0xA357):
    position = 0x801B5000
    if m is None:
        return 3, position, mode, slot
    for index in range(32):
        address = PICKUP_TABLE + index * PICKUP_SIZE
        m.write_bytes(address, bytes((index * 19 + byte * 11 + 3) & 0xFF
                                     for byte in range(PICKUP_SIZE)))
    m.write16(0x800AE8F0, seed_a)
    m.write16(0x800AE8F2, seed_b)
    m.write16(position, 0x8001)
    m.write16(position + 2, 0x7F01)
    m.write16(position + 4, 0xF234)
    if slot < 0:
        # The original negative-slot search checks the preceding entry before slot zero.
        m.write8(PICKUP_TABLE - PICKUP_SIZE + 0x0A, 1)
        for index in range(32):
            m.write8(PICKUP_TABLE + index * PICKUP_SIZE + 0x0A, 0)
    return (3, position, mode, slot)


def setup_items(area, first_area, mask=0, collected=False,
                sentinel_first=False, bad_magic=False):
    def setup(m):
        # This offset is supplied by the real member that ItemsLoadForArea consumes at 0x80154000.
        member = area
        area_blob = HARNESS.area_blob(member)
        header = int.from_bytes(area_blob[0x4DC:0x4E0], "little")
        item_header = AREA_BASE + header
        m.write32(STATE + ITEM_HEADER_FIELD, item_header)
        m.write16(STATE + ITEM_LEVEL_FIELD, 1)
        m.write16(STATE + ITEM_AREA_FIELD, member)
        delta = member - first_area
        m.write32(ITEM_MASK_TABLE, 0)
        if 0 <= delta < 8:
            m.write32(ITEM_MASK_TABLE + delta * 4, mask)
        for index in range(32):
            m.write8(ITEM_FLAGS + index, 0xA5)
            m.write8(PICKUP_TABLE + index * PICKUP_SIZE + 0x0A, 0xCC)
        _write_collected_table(m, ((0, member),) if collected else (), sentinel=True)
        if sentinel_first:
            m.write16(item_header + 8, 0x00FF)
        if bad_magic:
            m.write16(item_header, 0x1234)
    return setup


def setup_fog(area):
    def setup(m):
        member = HARNESS.area_blob(area)
        header = int.from_bytes(member[0x4D0:0x4D4], "little")
        color = member[header + 0x20:header + 0x23]
        m.write_bytes(LEVEL_GRID + 0x1C, color)
        m.write32(LEVEL_GRID + 0x24, 0x0000F000)
        near = int.from_bytes(member[header + 0x2C:header + 0x2E], "little")
        m.write16(LEVEL_GRID + 0x28, near)
    return setup


HARNESS = None


def scenarios(h, seed=94423, selected=None, scale=1.0):
    global HARNESS
    HARNESS = h
    runs = []
    runs += [("GetCollectedItemsTable", (), lambda _m: None, None, None)]
    runs += [
        ("CollectedItemsHas", (42,), setup_collected(((0, 42),)), None, None),
        ("CollectedItemsHas", (42,), setup_collected(((63, 42),)), None, None),
        ("CollectedItemsHas", (42,), setup_collected(((100, 42),)), None, None),
        ("CollectedItemsHas", (17,), setup_collected(), None, None),
        ("CollectedItemsHas", (17,), setup_collected(sentinel=False), None, None),
        ("CollectedItemsHas", (0,), setup_collected(((0, 0),)), None, None),
        ("CollectedItemsHas", (0x102,), setup_collected(((0, 2),)), None, None),
    ]
    runs += [
        ("MatrixCopy8", (0x801B1004, 0x801B1000), setup_matrix_copy, None, None),
        ("MatrixCopy8", (0x801B1000, 0x801B1000), setup_matrix_copy, None, None),
    ]
    mode_cases = [
        (0, 0, 0, 0, 0),
        (1, 0, 0, 0, 0x1000),
        (1, 0, 0, 0, 0),
        (2, 0x1234, 0xABCD, 0xFEDC, 0),
        (3, 0, 0, 0, 0),
        (4, 0x55, 0x66, 0x77, 0),
        (M32, 0, 0, 0, 0),
    ]
    for mode, red, green, blue, light_mode in mode_cases:
        runs.append(("SetColorMatrixMode", (mode, red, green, blue),
                     lambda m, value=light_mode: setup_color_matrix(m, value), None, None))
    for area in (1, 4, 10):
        runs.append(("ApplyLevelFog", (), setup_fog(area), None, area))

    pickup_cases = [
        (0, 0, 0x2345, 0xA357), (1, 0, 0xB333, 0xC000),
        (1, 4, 0x0000, 0x2000), (2, 31, 0xFFFF, 0x0001),
        (3, 3, 0x1000, 0xF000), (3, 8, 0x0000, 0x8000),
        (4, 5, 0x5555, 0xAAAA), (0, -1, 0x1234, 0xFEDC),
    ]
    for mode, slot, seed_a, seed_b in pickup_cases:
        args = setup_pickup(None, mode, slot, seed_a, seed_b)
        runs.append(("PickupSpawnSlot", args, lambda m, cfg=(mode, slot, seed_a, seed_b): setup_pickup(m, *cfg), None, None))

    full_search_args = setup_pickup(None, 0, -1)
    def setup_full_pickup_search(m):
        setup_pickup(m, 0, -1)
        for index in range(31):
            m.write8(PICKUP_TABLE + index * PICKUP_SIZE + 0x0A, 1)
    runs.append(("PickupSpawnSlot", full_search_args, setup_full_pickup_search, None, None))
    def setup_no_free_pickup(m):
        setup_pickup(m, 0, -1)
        for index in range(32):
            m.write8(PICKUP_TABLE + index * PICKUP_SIZE + 0x0A, 1)
    runs.append(("PickupSpawnSlot", full_search_args, setup_no_free_pickup, None, None))

    item_cases = [
        (1, 1, 0, False, False, False),
        (1, 1, 0, True, False, False),
        (2, 1, 0x00000002, False, False, False),
        (4, 1, 0x00000080, False, False, False),
        (8, 1, 0, False, False, False),
        (9, 1, 0, False, False, False),  # delta 8: warning hook and mask fallback
        (10, 10, 0x00000001, False, False, False),
        (18, 18, 0, False, False, False),
        (33, 33, 0, False, False, False),
        (1, 1, 0, False, True, False),
        (1, 1, 0, False, False, True),
    ]
    for area, first_area, mask, collected, stop, bad_magic in item_cases:
        runs.append(("ItemsLoadForArea", (),
                     setup_items(area, first_area, mask, collected, stop, bad_magic),
                     {"first_area": first_area}, area))
    def setup_zero_item_count(m):
        setup_items(1, 1)(m)
        header = int.from_bytes(HARNESS.area_blob(1)[0x4DC:0x4E0], "little")
        m.write16(AREA_BASE + header + 4, 0)
    runs.append(("ItemsLoadForArea", (), setup_zero_item_count,
                 {"first_area": 1}, 1))
    def setup_clamped_item_kind(m):
        setup_items(1, 1)(m)
        m.write16(STATE + ITEM_LEVEL_FIELD, 49)
    runs.append(("ItemsLoadForArea", (), setup_clamped_item_kind,
                 {"first_area": 1}, 1))

    if scale > 1.0:
        rng = random.Random(seed)
        for _ in range(round(scale) - 1):
            for area in (1, 2, 4, 10, 18, 33):
                first = area if area in (10, 18, 33) else 1
                mask = rng.getrandbits(32)
                runs.append(("ItemsLoadForArea", (), setup_items(area, first, mask,
                             bool(rng.getrandbits(1))), {"first_area": first}, area))

    for name, args, setup, context, area in runs:
        if selected is not None and name != selected:
            continue
        h.run(name, args, setup, context, area)


def _mutate_function(source, name, before, after, label):
    start = source.index(f"ae_{name}(")
    line_start = source.rfind("\n", 0, start) + 1
    end = source.index("\n}", start) + 2
    body = source[line_start:end]
    assert before in body, (name, label, before)
    return f"{name}: {label}", source[:line_start] + body.replace(before, after, 1) + source[end:]


def mutation_sources(original):
    specs = [
        ("SetColorMatrixMode", "wrong restore SDK matrix argument", "mode == 3u ? AE_ITEMS_G_RESTORE_MATRIX : AE_ITEMS_G_COLOR_MATRIX", "AE_ITEMS_G_COLOR_MATRIX"),
        ("ApplyLevelFog", "wrong SDK far-color channel", '"SetFarColor", red, green, blue', '"SetFarColor", red ^ 1u, green, blue'),
        ("SetColorMatrixMode", "tint green from red", "AE_ITEMS_MATRIX_R21, green", "AE_ITEMS_MATRIX_R21, (red + green - green)"),
        ("SetColorMatrixMode", "skip restore matrix mode", "} else if (mode == 3u) {", "} else if (mode == 4u) {"),
        ("SetColorMatrixMode", "drop final matrix coefficient", "AE_ITEMS_MATRIX_R33, 0", "AE_ITEMS_MATRIX_R33, AE_ITEMS_COLOR_MATRIX_UNIT"),
        ("MatrixCopy8", "copy wrong last word", "source + AE_ITEMS_COPY_WORD_7", "source + AE_ITEMS_COPY_WORD_6"),
        ("MatrixCopy8", "write wrong last word", "destination + AE_ITEMS_COPY_WORD_7", "destination + AE_ITEMS_COPY_WORD_6"),
        ("MatrixCopy8", "skip middle word", "ae_wr32(r, destination + AE_ITEMS_COPY_WORD_4, second);", "ae_wr32(r, destination + AE_ITEMS_COPY_WORD_4, first);"),
        ("ApplyLevelFog", "write parameter to second halfword", "ae_wr16(r, AE_ITEMS_FOG_SCRATCH,", "ae_wr16(r, AE_ITEMS_FOG_SCRATCH + sizeof(uint16_t),"),
        ("ApplyLevelFog", "wrong fog start source", "AE_ITEMS_LEVEL_GRID + AE_ITEMS_LEVEL_GRID_FOG_START", "AE_ITEMS_LEVEL_GRID + AE_ITEMS_LEVEL_GRID_FOG_PARAMETER"),
        ("ApplyLevelFog", "leave final scratch word nonzero", "ae_wr16(r, AE_ITEMS_FOG_SCRATCH_ZERO_B, 0);", "ae_wr16(r, AE_ITEMS_FOG_SCRATCH_ZERO_B, 1);"),
        ("ItemsLoadForArea", "wrong record stride", "record += AE_ITEMS_ITEM_RECORD_SIZE;", "record += AE_ITEMS_ITEM_RECORD_SIZE - 4u;"),
        ("ItemsLoadForArea", "invert collected mask bit", "if ((item_mask & 1u) == 0u)", "if ((item_mask & 1u) != 0u)"),
        ("ItemsLoadForArea", "check pickup kind as collected id", "ae_CollectedItemsHas(r, current_area)", "ae_CollectedItemsHas(r, kind)"),
        ("PickupSpawnSlot", "wrong seed multiplier", "(ae_rd16(r, AE_ITEMS_G_RANDOM_SEED_A) * 5u) + 1u", "(ae_rd16(r, AE_ITEMS_G_RANDOM_SEED_A) * 4u) + 1u"),
        ("PickupSpawnSlot", "use sine for x direction", "h->rcos(h->user, r, (int32_t)angle)", "h->rsin(h->user, r, (int32_t)angle)"),
        ("PickupSpawnSlot", "wrong final launch seed", "uint32_t final_seed_a = (ae_rd16(r, AE_ITEMS_G_RANDOM_SEED_A) * 5u) + 1u;", "uint32_t final_seed_a = (ae_rd16(r, AE_ITEMS_G_RANDOM_SEED_A) * 4u) + 1u;"),
        ("CollectedItemsHas", "wrong entry stride", "table + index * AE_ITEMS_COLLECTED_ENTRY_STRIDE", "table + index"),
        ("CollectedItemsHas", "treat terminator as collected", "if (value == AE_ITEMS_COLLECTED_EMPTY) return 0;", "if (value == AE_ITEMS_COLLECTED_EMPTY) return 1;"),
        ("CollectedItemsHas", "truncate ids to one byte", "if (value == id) return 1;", "if (value == (id & 0xFFu)) return 1;"),
        ("GetCollectedItemsTable", "wrong table start", "return AE_ITEMS_G_COLLECTED_TABLE;", "return AE_ITEMS_G_COLLECTED_TABLE + 2u;"),
        ("GetCollectedItemsTable", "return zero pointer", "return AE_ITEMS_G_COLLECTED_TABLE;", "return 0u;"),
        ("GetCollectedItemsTable", "return next progress table", "return AE_ITEMS_G_COLLECTED_TABLE;", "return AE_ITEMS_G_COLLECTED_TABLE + 0x100u;"),
    ]
    for name, label, before, after in specs:
        yield _mutate_function(original, name, before, after, label)


def mutations(args):
    original = (ROOT / "src/world/ae_items_init.c").read_text(encoding="utf-8")
    cases = list(mutation_sources(original))
    survivors = []
    for index, (label, source) in enumerate(cases):
        path = ROOT / f"extract/mips/items_init_mutation_{index}.c"
        path.write_text(source, encoding="utf-8")
        name = label.split(":", 1)[0]
        harness = ItemsHarness(path, f"items_mutation_{index}")
        try:
            scenarios(harness, args.seed, selected=name, scale=max(1.0, args.cases))
        except CheckFailure as exc:
            print(f"CAUGHT {label}: {exc}")
        else:
            print(f"SURVIVED {label}")
            survivors.append(label)
    print(f"Mutations: {len(cases)}; caught: {len(cases) - len(survivors)}; survivors: {survivors}")
    return int(bool(survivors))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--cases", type=float, default=1.0)
    parser.add_argument("--seed", type=int, default=94423)
    args = parser.parse_args()
    if args.mutate:
        return mutations(args)
    harness = ItemsHarness()
    scenarios(harness, args.seed, scale=args.cases)
    summary = harness.finish(EXTRA_COVERAGE,
                             default_run=args.cases == 1.0 and args.seed == 94423,
                             show_coverage=args.coverage)
    print(" ".join(f"{name}={harness.counts[name]}" for name in FUNCS))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except CheckFailure as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
