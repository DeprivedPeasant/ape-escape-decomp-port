"""Check game texture-copy functions with public SDK GPU calls intercepted."""
import argparse
import ctypes as C
import hashlib
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

FUNCS = {
    "AreaTexCopyInit": (0x8005E7EC, 708, "i", "uuuu"),
    "HeapFree": (0x8008C064, 56, "i", "u"),
}

EXTRA_COVERAGE = {}
HOOKS = {
    "HeapAlloc": (0x8008C09C, 1),
    "DrawSync": (0x800286CC, 1),
    "LoadImage": (0x80028978, 2),
    "StoreImage": (0x800289D8, 2),
}
AREA_BASE = 0x80154000
AREA_TEXCOPY_ENABLED = 0x2A
AREA_TEXCOPY_COLOR = 0x24
AREA_RECT_TABLE = 0x800A47F0
HEAP_POINTER_TABLE = 0x800BF690
HEAP_POINTER_COUNT = 64
GPU_CONTEXT = 0x801F4000
STACK_LOW = mr.STACK_WINDOW[0] - 0x80000000
IMAGE_ALLOCATIONS = (0x801D0000, 0x801D1000)
AREA_LOAD_RECT = mr.DEFAULT_SP - 48
M32 = 0xFFFFFFFF


def signed32(value):
    value &= M32
    return value - 0x100000000 if value & 0x80000000 else value


def active_area_members():
    result = []
    for area, member in sorted(dump_areas.area_members().items()):
        blob = (dump_areas.MEMBERS / f"{member:03X}.bin").read_bytes()
        header = struct.unpack_from("<I", blob, 0x4D0)[0]
        enabled = struct.unpack_from("<H", blob, header + AREA_TEXCOPY_ENABLED)[0]
        if enabled != 0:
            color = struct.unpack_from("<I", blob, header + AREA_TEXCOPY_COLOR)[0]
            result.append((area, member, blob, header, color))
    if len(result) < 3:
        raise SystemExit("need real area members with AreaTexCopyInit enabled")
    return result


class CMemory:
    def __init__(self, main, scratch):
        self.main = main
        self.scratch = scratch

    @staticmethod
    def _resolve(address, size):
        address &= M32
        if 0x1F800000 <= address < 0x1F800400:
            offset = address - 0x1F800000
            if offset + size > 0x400:
                raise ValueError(f"scratch read out of range at {address:08X}")
            return "scratch", offset
        segment = address >> 29
        physical = address & 0x1FFFFFFF
        if segment in (0, 4, 5) and physical + size <= 0x200000:
            return "main", physical
        raise ValueError(f"unmapped RAM address {address:08X}")

    def read_bytes(self, address, size):
        region, offset = self._resolve(address, size)
        buf = self.scratch if region == "scratch" else self.main
        return bytes(buf[offset:offset + size])

    def write_bytes(self, address, data):
        region, offset = self._resolve(address, len(data))
        buf = self.scratch if region == "scratch" else self.main
        buf[offset:offset + len(data)] = data

    def read8(self, address):
        return self.read_bytes(address, 1)[0]

    def read16(self, address):
        return int.from_bytes(self.read_bytes(address, 2), "little", signed=False)

    def reads16(self, address):
        return struct.unpack("<h", self.read_bytes(address, 2))[0]

    def read32(self, address):
        return int.from_bytes(self.read_bytes(address, 4), "little", signed=False)

    def write8(self, address, value):
        self.write_bytes(address, bytes((value & 0xFF,)))

    def write16(self, address, value):
        self.write_bytes(address, struct.pack("<H", value & 0xFFFF))

    def write32(self, address, value):
        self.write_bytes(address, struct.pack("<I", value & M32))


class HookModel:
    def __init__(self, memory, seed, context):
        self.memory = memory
        self.seed = seed
        self.context = context
        self.events = []
        self.alloc_index = 0
        self.wait_index = 0

    def call(self, name, raw_args):
        arity = HOOKS[name][1]
        args = tuple(value & M32 for value in raw_args[:arity])
        extra = None
        result = 0

        if name == "HeapAlloc":
            if self.alloc_index >= len(IMAGE_ALLOCATIONS):
                raise AssertionError("unexpected HeapAlloc call count")
            pointer = IMAGE_ALLOCATIONS[self.alloc_index]
            self.alloc_index += 1
            free_slot = None
            for index in range(HEAP_POINTER_COUNT):
                slot = HEAP_POINTER_TABLE + index * 4
                if self.memory.read32(slot) == 0:
                    free_slot = slot
                    break
            if free_slot is None:
                raise AssertionError("test HeapAlloc table is full")
            self.memory.write32(free_slot, pointer)
            result = pointer
            extra = (free_slot, pointer)
        elif name == "DrawSync":
            result = self.context.get("draw_result", 0)
        elif name == "StoreImage":
            rect, image = args
            count = self.memory.reads16(rect + 4) * self.memory.reads16(rect + 6)
            if count > 2048:
                raise AssertionError(f"unsafe StoreImage test dimensions: {count}")
            count = max(count, 0)
            rng = random.Random(f"{self.seed}/{self.context.get('case_name')}/store")
            for pixel in range(count):
                value = 0 if pixel % 23 == 0 else rng.getrandbits(16)
                self.memory.write16(image + pixel * 2, value)
            extra = (count, hashlib.sha256(self.memory.read_bytes(image, count * 2)).hexdigest())
        elif name == "LoadImage":
            rect, image = args
            count = self.memory.reads16(rect + 4) * self.memory.reads16(rect + 6)
            if count > 2048:
                raise AssertionError(f"unsafe LoadImage test dimensions: {count}")
            count = max(count, 0)
            extra = (count, hashlib.sha256(self.memory.read_bytes(image, count * 2)).hexdigest())

        self.events.append((name, args, extra))
        return result & M32


def build_dll(source=None, tag="tex_copy"):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{time.time_ns()}.dll"
    subprocess.run([
        shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-shared", "-static-libgcc", "-I" + str(ROOT / "src/render"),
        str(ROOT / "src/core/ae_ram.c"),
        str(source or ROOT / "src/render/ae_tex_copy.c"), "-o", str(dll),
    ], check=True)
    return dll


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
        self.main = (C.c_uint8 * 0x200000).from_address(
            self.lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(
            self.lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.memory = CMemory(self.main, self.scratch)
        self.model = None
        self.callbacks = []
        callback_type = C.CFUNCTYPE(U, P, P, U, U, U, U)
        callback_types = {"HeapAlloc": callback_type,
                          "DrawSync": C.CFUNCTYPE(C.c_int32, P, P, C.c_int32),
                          "LoadImage": C.CFUNCTYPE(C.c_int32, P, P, U, U),
                          "StoreImage": C.CFUNCTYPE(C.c_int32, P, P, U, U)}
        self.hooks_type = type("AeTexCopyHooks", (C.Structure,), {
            "_fields_": [("user", P), ("stack_pointer", U)] +
                        [(name, callback_types[name]) for name in HOOKS]
        })
        self.hooks = self.hooks_type()
        for name in HOOKS:
            callback = callback_types[name](
                lambda _user, _ram, *args, hook=name: self.model.call(hook, args))
            self.callbacks.append(callback)
            setattr(self.hooks, name, callback)

        self.functions = {}
        specs = {
            "AreaTexCopyInit": (I, [P, C.POINTER(self.hooks_type), U, U, U, U]),
            "HeapFree": (I, [P, U]),
        }
        for name, (restype, argtypes) in specs.items():
            function = getattr(self.lib, "ae_" + name)
            function.restype = restype
            function.argtypes = argtypes
            self.functions[name] = function

    def sync(self, cpu):
        C.memmove(self.main, bytes(cpu.ram), len(cpu.ram))
        C.memmove(self.scratch, bytes(cpu.scratch), len(cpu.scratch))
        self.lib.ae_ram_clear_error(self.ram)

    def invoke(self, name, args, initial_regs=None):
        self.hooks.stack_pointer = mr.DEFAULT_SP
        if name == "HeapFree":
            return self.functions[name](self.ram, *args)
        return self.functions[name](self.ram, C.byref(self.hooks), *args)


class CheckFailure(Exception):
    pass


class TexCopyHarness:
    def __init__(self, source=None, tag="tex_copy"):
        reg.declare("stack_window", "801FF000..80200000")
        self.areas = active_area_members()
        first_blob = self.areas[0][2]
        self.m = mr.MipsRam.from_extract(first_blob)
        self.m.coverage = set()
        self.base_by_area = {}
        for area, _member, blob, _header, _color in self.areas:
            cpu = mr.MipsRam.from_extract(blob)
            self.base_by_area[area] = cpu.snapshot()
        self.c = CSide(build_dll(source, tag))
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0

    def area_base(self, area):
        if area not in self.base_by_area:
            raise ValueError(f"area {area:02X} is not an enabled real texcopy area")
        return self.base_by_area[area]

    def run(self, name, args=(), setup=None, context=None, area=None, initial_regs=None):
        if area is None:
            area = self.areas[self.counts[name] % len(self.areas)][0]
        self.m.restore(self.area_base(area))
        if setup:
            setup(self.m)
        context = dict(context or {})
        context.setdefault("case_name", f"{name}/{self.counts[name]}")
        if name == "AreaTexCopyInit":
            for index in range(HEAP_POINTER_COUNT):
                self.m.write32(HEAP_POINTER_TABLE + index * 4, 0)

        self.c.sync(self.m)
        mips_model = HookModel(CMemory(self.m.ram, self.m.scratch), 94423, context)
        c_model = HookModel(self.c.memory, 94423, context)
        self.c.model = c_model
        mips_hooks = {address: (lambda cpu, regs, hook=name: mips_model.call(hook, regs[4:8]))
                      for name, (address, _arity) in HOOKS.items()}
        self.m.hook_log.clear()
        pc, _size, _ret, kinds = FUNCS[name]
        cooked_args = tuple(signed32(arg) if kind == "s" else arg & M32
                            for arg, kind in zip(args, kinds))
        registers = self.m.call(pc, cooked_args, hooks=mips_hooks,
                                limit=2_000_000, initial_regs=initial_regs)
        c_result = self.c.invoke(name, cooked_args, initial_regs)
        self.counts[name] += 1
        self.compare(name, cooked_args, registers[2], c_result,
                     mips_model.events, c_model.events)

    def compare(self, name, args, original, result, original_events, c_events):
        if (result & M32) != (original & M32):
            self.fail(name, args, f"return differs: C={result & M32:08X} MIPS={original & M32:08X}")
        expected = bytes(self.m.ram[:STACK_LOW])
        actual = C.string_at(self.c.main, STACK_LOW)
        if actual != expected:
            index = next(i for i, (left, right) in enumerate(zip(actual, expected))
                         if left != right)
            self.fail(name, args,
                      f"RAM differs at {0x80000000 + index:08X}: "
                      f"C={actual[index]:02X} MIPS={expected[index]:02X}")
        if bytes(self.c.scratch) != bytes(self.m.scratch):
            self.fail(name, args, "scratchpad differs")
        if original_events != c_events:
            self.fail(name, args,
                      f"hook sequence differs: C={c_events!r}; MIPS={original_events!r}")
        if self.c.lib.ae_ram_failed(self.c.ram):
            error = self.c.lib.ae_ram_error(self.c.ram).decode(errors="replace")
            self.fail(name, args, f"AeRam error: {error}")

    def fail(self, name, args, detail):
        self.failures += 1
        raise CheckFailure(f"FAIL {name}{tuple(args)}: {detail}")

    def finish(self, names=None, show_coverage=False, default_run=False):
        names = names or list(FUNCS)
        items = [(name, *FUNCS[name][:2], True) for name in names]
        if names == list(FUNCS):
            items.extend((name, address, size, False)
                         for name, (address, size) in EXTRA_COVERAGE.items())
        summary = reg.coverage_summary(items, self.m.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        gaps = any(item["uncovered_count"] or item["stale_declared"]
                   for item in summary.values())
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps,
                              function_names=list(FUNCS))
        if gaps and names == list(FUNCS):
            raise CheckFailure("FAIL: undeclared texture-copy instruction coverage gaps")
        return summary


def put_rect(cpu, address, x, y, width, height):
    cpu.write_bytes(address, struct.pack("<hhhh", x, y, width, height))


def scenarios(h, seed=94423, only=None, repeat=1):
    rng = random.Random(seed)
    selected = set(only or FUNCS)

    def add(name, args=(), setup=None, context=None, area=None, initial_regs=None):
        if name in selected:
            h.run(name, args, setup, context, area, initial_regs)

    tex_areas = h.areas
    for i, (area, _member, _blob, _header, color) in enumerate(tex_areas):
        def real_texcopy_setup(m, rgba=color):
            m.write_bytes(AREA_LOAD_RECT, m.read_bytes(AREA_RECT_TABLE, 8))
            m.write32(HEAP_POINTER_TABLE, 0)
            m.write32(HEAP_POINTER_TABLE + 4, 0)
        add("AreaTexCopyInit", (AREA_LOAD_RECT, i, color, 0), real_texcopy_setup,
            {"case_name": f"AreaTexCopyInit/area_{area:02X}"}, area)

    for i, (width, height, color) in enumerate(((0, 128, 0), (-2, 16, 0x00FFFFFF),
                                                (16, 128, 0x00FFFFFF), (16, 128, 0x004080C0))):
        area, _member, _blob, _header, _color = tex_areas[i % len(tex_areas)]
        def edge_texcopy_setup(m, w=width, hgt=height):
            m.write_bytes(AREA_LOAD_RECT, m.read_bytes(AREA_RECT_TABLE, 8))
            m.write16(AREA_LOAD_RECT + 4, w)
            m.write16(AREA_LOAD_RECT + 6, hgt)
            m.write32(HEAP_POINTER_TABLE, 0)
            m.write32(HEAP_POINTER_TABLE + 4, 0)
        add("AreaTexCopyInit", (AREA_LOAD_RECT, 4 + i, color, 0), edge_texcopy_setup,
            {"case_name": f"AreaTexCopyInit/edge_{i}"}, area)

    for i in range(5):
        pointer = 0x81230000 + i * 4
        def free_setup(m, target=pointer, case=i):
            rng_local = random.Random(f"{seed}/HeapFree/{case}")
            for slot in range(HEAP_POINTER_COUNT):
                value = rng_local.getrandbits(32) | 1
                m.write32(HEAP_POINTER_TABLE + slot * 4, value)
            if case == 0:
                for slot in (2, 15, 63):
                    m.write32(HEAP_POINTER_TABLE + slot * 4, target)
            elif case == 1:
                m.write32(HEAP_POINTER_TABLE + 63 * 4, target)
            elif case == 2:
                m.write32(HEAP_POINTER_TABLE + 0, 0)
                m.write32(HEAP_POINTER_TABLE + 1 * 4, 0)
                target = 0
        add("HeapFree", (pointer,), free_setup, area=h.areas[i % len(h.areas)][0])

    if repeat > 1:
        for iteration in range(repeat - 1):
            area, _member, _blob, _header, color = tex_areas[iteration % len(tex_areas)]
            rect = (rng.randrange(1024), rng.randrange(512), rng.choice((1, 4, 16)),
                    rng.choice((1, 8, 32, 128)))
            def repeat_texcopy_setup(m, dims=rect):
                m.write_bytes(AREA_LOAD_RECT, m.read_bytes(AREA_RECT_TABLE, 8))
                put_rect(m, AREA_LOAD_RECT, *dims)
                m.write32(HEAP_POINTER_TABLE, 0)
                m.write32(HEAP_POINTER_TABLE + 4, 0)
            add("AreaTexCopyInit", (AREA_LOAD_RECT, iteration & 7, color, 0),
                repeat_texcopy_setup, {"case_name": f"repeat_tex/{iteration}"}, area)


def mutation_sources(original):
    specs = [
        ("AreaTexCopyInit", "wrong SDK sync argument", "tex_draw_sync(r, hooks, 0)", "tex_draw_sync(r, hooks, 1)"),
        ("AreaTexCopyInit", "wrong SDK variant image argument", "tex_load_image(r, hooks, variant_rect, variant_image)", "tex_load_image(r, hooks, variant_rect, source_image)"),
        ("AreaTexCopyInit", "wrong color weight", "#define AE_TEX_VARIANT_DIVISOR 8\n", "#define AE_TEX_VARIANT_DIVISOR 4\n"),
        ("AreaTexCopyInit", "drop semi-transparency", "#define AE_TEX_COLOR_ALPHA_MASK 0x8000u", "#define AE_TEX_COLOR_ALPHA_MASK 0x0000u"),
        ("AreaTexCopyInit", "wrong variant count", "#define AE_TEX_VARIANT_COUNT 8u", "#define AE_TEX_VARIANT_COUNT 7u"),
        ("AreaTexCopyInit", "wrong rectangle table stride", "(table_index << 3)", "(table_index << 2)"),
        ("AreaTexCopyInit", "wrong pixel stride", "#define AE_TEX_PIXEL_SIZE 2u", "#define AE_TEX_PIXEL_SIZE 4u"),
        ("HeapFree", "skip last heap slot", "index < AE_TEX_HEAP_POINTER_COUNT", "index < AE_TEX_HEAP_POINTER_COUNT - 1u"),
        ("HeapFree", "clear only first duplicate", "if (ae_rd32(r, slot) == pointer) ae_wr32(r, slot, 0u);", "if (ae_rd32(r, slot) == pointer && index == 0u) ae_wr32(r, slot, 0u);"),
        ("HeapFree", "wrong pointer table base", "#define AE_TEX_HEAP_POINTER_TABLE 0x800BF690u", "#define AE_TEX_HEAP_POINTER_TABLE 0x800BF68Cu"),
        ("HeapFree", "invert pointer match", "if (ae_rd32(r, slot) == pointer) ae_wr32(r, slot, 0u);", "if (ae_rd32(r, slot) != pointer) ae_wr32(r, slot, 0u);"),
    ]
    for name, label, before, after in specs:
        if before == after:
            raise AssertionError(f"equivalent mutation: {name}: {label}")
        if before not in original:
            raise AssertionError(f"mutation source not found: {name}: {label}: {before!r}")
        yield name, label, original.replace(before, after, 1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--seed", type=int, default=94423)
    parser.add_argument("--cases", type=int, default=1)
    parser.add_argument("--only", choices=tuple(FUNCS))
    args = parser.parse_args()

    if args.mutate:
        original = (ROOT / "src/render/ae_tex_copy.c").read_text(encoding="utf-8")
        mutations = list(mutation_sources(original))
        survivors = []
        for index, (function, label, source) in enumerate(mutations):
            path = ROOT / "extract/mips" / f"tex_copy_mutation_{index}.c"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(source, encoding="utf-8")
            harness = TexCopyHarness(path, f"tex_copy_mutation_{index}")
            try:
                scenarios(harness, args.seed, {function})
            except CheckFailure:
                continue
            print(f"SURVIVED {function}: {label}")
            survivors.append((function, label))
        caught = len(mutations) - len(survivors)
        print(f"Mutations: {len(mutations)}; caught: {caught}; survivors: {len(survivors)}")
        for function, label in survivors:
            print(f"  survivor: {function}: {label}")
        return int(bool(survivors))

    harness = TexCopyHarness()
    selected = [args.only] if args.only else list(FUNCS)
    scenarios(harness, args.seed, set(selected), max(args.cases, 1))
    summary = harness.finish(names=selected if args.only else None,
                             show_coverage=args.coverage,
                             default_run=args.only is None and args.cases == 1 and args.seed == 94423)
    print(" ".join(f"{name}={harness.counts[name]}" for name in selected))
    if not args.only:
        print(f"OK: {sum(harness.counts.values())} differential cases")
    return int(any(item["uncovered_count"] or item["stale_declared"]
                   for item in summary.values()))


if __name__ == "__main__":
    sys.exit(main())
