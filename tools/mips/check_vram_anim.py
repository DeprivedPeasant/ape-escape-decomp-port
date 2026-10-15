"""Check game VRAM animation setup with MoveImage intercepted at the public SDK API."""
import argparse
import ctypes as C
import os
import random
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas
import exclusion_registry as reg
import mips_ram as mr
OUT = ROOT / "extract/mips"
TASK_OUT = ROOT / "extract/codex-task"
M32 = 0xFFFFFFFF
AREA_BASE = 0x80154000
G_AREA_COUNT = 0x800B98F0
G_AREA_LIST = 0x800B98F4
FUNCS = {"AreaSetVramAnimList": (0x800690E8, 272, "ae_AreaSetVramAnimList")}
P, U32, I32 = C.c_void_p, C.c_uint32, C.c_int32
MoveHook = C.CFUNCTYPE(I32, P, P, U32, I32, I32)
class Hooks(C.Structure):
    _fields_ = [("user", P), ("MoveImage", MoveHook)]
def u32(value): return value & M32
@dataclass
class Spec:
    name: str
    root: int
    args: tuple
    c_name: str
    c_args: tuple
    writes: list = field(default_factory=list)  # (size, address, value)
    area_blob: bytes | None = None
    area_name: str = ""


class Mem:
    def __init__(self, side, cpu=None):
        self.side, self.cpu = side, cpu

    def read(self, address, size):
        if self.cpu is not None:
            return self.cpu.read(address, size)
        return int.from_bytes(bytes(self.side.main[(address & 0x1FFFFF):
                                                   (address & 0x1FFFFF) + size]), "little")

    def write(self, address, size, value):
        if self.cpu is not None:
            self.cpu.write(address, size, value)
        else:
            off = address & 0x1FFFFF
            self.side.main[off:off + size] = (u32(value) & ((1 << (size * 8)) - 1)).to_bytes(size, "little")

    def rd8(self, address): return self.read(address, 1)
    def rd16(self, address): return self.read(address, 2)
    def rd32(self, address): return self.read(address, 4)
    def wr8(self, address, value): self.write(address, 1, value)
    def wr16(self, address, value): self.write(address, 2, value)
    def wr32(self, address, value): self.write(address, 4, value)

class Model:
    def __init__(self, mem):
        self.mem = mem
        self.events = []
    def move_image(self, rect, x, y):
        # Snapshot the transient RECT at call time, including its address.
        words = tuple(self.mem.rd16(rect + i * 2) for i in range(4))
        self.events.append(("MoveImage", u32(rect), words, u32(x), u32(y)))
        return -7  # Deliberately nonzero host result, ignored by this game function.

class CSide:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        lib = self.lib
        lib.ae_ram_create.restype = P
        lib.ae_ram_destroy.argtypes = [P]
        lib.ae_ptr.restype = P
        lib.ae_ptr.argtypes = [P, U32, U32]
        lib.ae_rd8.argtypes = [P, U32]; lib.ae_rd8.restype = U32
        lib.ae_rd16.argtypes = [P, U32]; lib.ae_rd16.restype = U32
        lib.ae_rd32.argtypes = [P, U32]; lib.ae_rd32.restype = U32
        lib.ae_wr8.argtypes = [P, U32, U32]
        lib.ae_wr16.argtypes = [P, U32, U32]
        lib.ae_wr32.argtypes = [P, U32, U32]
        lib.ae_ram_failed.argtypes = [P]; lib.ae_ram_failed.restype = C.c_int
        lib.ae_ram_error.argtypes = [P]; lib.ae_ram_error.restype = C.c_char_p
        self.ram = lib.ae_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.model = None
        self.keepalive = []
        self.hooks = Hooks()
        self._install_callbacks()

    def _callback(self, typ, fn):
        cb = typ(fn)
        self.keepalive.append(cb)
        return cb

    def _install_callbacks(self):
        self.hooks.MoveImage = self._callback(MoveHook,
            lambda user, ram, rect, x, y: self.model.move_image(rect, x, y))
        self.lib.ae_AreaSetVramAnimList.restype = I32
        self.lib.ae_AreaSetVramAnimList.argtypes = [P, C.POINTER(Hooks), U32]
    def mem(self): return Mem(self)
    def reset(self, image, scratch):
        C.memmove(self.main, image, len(image))
        C.memmove(self.scratch, scratch, len(scratch))
        self.lib.ae_ram_clear_error.argtypes = [P]
        self.lib.ae_ram_clear_error(self.ram)
    def run(self, spec, model):
        self.model = model
        result = self.lib.ae_AreaSetVramAnimList(self.ram, C.byref(self.hooks), *spec.c_args)
        if self.lib.ae_ram_failed(self.ram):
            raise AssertionError(self.lib.ae_ram_error(self.ram).decode())
        return u32(result)
    def snapshot(self): return bytes(self.main), bytes(self.scratch)

def build(source=None, tag="vramanim"):
    OUT.mkdir(parents=True, exist_ok=True)
    TASK_OUT.mkdir(parents=True, exist_ok=True)
    dll = OUT / f"{tag}_{os.getpid()}_{time.time_ns()}.dll"
    src = Path(source) if source else ROOT / "src/render/ae_vram_anim.c"
    subprocess.run([
        shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-shared", "-static-libgcc", "-I" + str(ROOT / "src/render"),
        str(ROOT / "src/core/ae_ram.c"), str(src), "-o", str(dll),
    ], check=True)
    return dll

def area_feature(blob, area):
    header_off = int.from_bytes(blob[0x4D0:0x4D4], "little")
    if not 0 <= header_off < len(blob) - 0x54:
        raise ValueError(f"area {area:02X}: invalid header offset {header_off:#x}")
    header = header_off
    feature_word = None
    for index in range(8):
        word = int.from_bytes(blob[header + 0x34 + index * 4:header + 0x38 + index * 4], "little")
        if word & 0xFF == 0x20:
            feature_word = word
            break
    if feature_word is None:
        raise ValueError(f"area {area:02X}: member has no feature 0x20")
    return AREA_BASE + header + ((feature_word >> 8) * 4)

def spec(name, root, args, c_name, c_args, *, writes=(), area_blob=None, area_name=""):
    return Spec(name, root, tuple(args), c_name, tuple(c_args), list(writes), area_blob, area_name)


def fixed_specs():
    return [
        spec('area_null', 0x800690E8, (0,), 'ae_AreaSetVramAnimList', (0,)),
        spec('area_mixed', 0x800690E8, (0x800C2200,), 'ae_AreaSetVramAnimList', (0x800C2200,),
             writes=[(1, 0x800C2200, 1),
                     (1, 0x800C2201, 0),
                     (2, 0x800C2204, 10),
                     (2, 0x800C2206, 12),
                     (2, 0x800C2208, 44),
                     (2, 0x800C220A, 55),
                     (1, 0x800C220C, 8),
                     (1, 0x800C220D, 9),
                     (1, 0x800C2210, 2),
                     (1, 0x800C2211, 0),
                     (2, 0x800C2214, 20),
                     (2, 0x800C2216, 30),
                     (2, 0x800C2218, 80),
                     (2, 0x800C221A, 90),
                     (1, 0x800C221C, 8),
                     (1, 0x800C221D, 9),
                     (1, 0x800C2220, 3),
                     (1, 0x800C2230, 0)]),
        spec('area_feature_inline', 0x800690E8, (0x800C2300,), 'ae_AreaSetVramAnimList', (0x800C2300,),
             writes=[(1, 0x800C2300, 1),
                     (1, 0x800C2301, 1),
                     (2, 0x800C2304, 3),
                     (2, 0x800C2306, 4),
                     (2, 0x800C2308, 10),
                     (2, 0x800C230A, 20),
                     (1, 0x800C230C, 8),
                     (1, 0x800C230D, 9),
                     (1, 0x800C2310, 0)])
    ]

def area_specs():
    cases = []
    members = dump_areas.area_members()
    for area in (0x01, 0x02, 0x05, 0x20, 0x31, 0x45):
        member = members[area]
        blob = (ROOT / "extract/members" / f"{member:03X}.bin").read_bytes()
        feature = area_feature(blob, area)
        cases.append(spec(f"area_{area:02X}_member_{member:03X}", 0x800690E8,
                          (feature,), "ae_AreaSetVramAnimList", (feature,),
                          area_blob=blob, area_name=f"{area:02X}/{member:03X}"))
    return cases

def make_random_specs(count, seed):
    rng = random.Random(seed)
    cases = []
    for i in range(count):
        address = 0x800C2400
        writes = [(4, G_AREA_COUNT, rng.getrandbits(32))]
        length = rng.randrange(1, 12)
        for index in range(length):
            entry = address + index * 16
            writes += [(1, entry, rng.choice((1, 2, 3))), (1, entry + 1, rng.choice((0, 1, 2)))]
            writes += [(2, entry + offset, rng.getrandbits(16)) for offset in (4, 6, 8, 10)]
            writes += [(1, entry + offset, rng.getrandbits(8)) for offset in (12, 13)]
        writes += [(1, address + length * 16, 0)]
        cases.append(spec(f"random_list_{i:03d}", 0x800690E8, (address,),
                          "ae_AreaSetVramAnimList", (address,), writes=writes))
    return cases

def setup_memory(mem, item):
    mem.wr32(G_AREA_COUNT, 0)
    mem.wr32(G_AREA_LIST, 0)
    for size, address, value in item.writes:
        mem.write(address, size, value)

def setup_c(side, item, model, base_image, base_scratch):
    side.reset(base_image, base_scratch)
    model.mem = side.mem()
    setup_memory(model.mem, item)
    if item.area_blob is not None:
        off = AREA_BASE & 0x1FFFFF
        side.main[off:off+len(item.area_blob)] = item.area_blob

def call_spec(cpu, side, item, model, base_image, base_scratch, collect_coverage=False):
    cpu.restore((base_image, base_scratch))
    cpu.coverage = set()
    model.mem = Mem(None, cpu)
    model.events.clear()
    setup_memory(model.mem, item)
    if item.area_blob is not None: cpu.load_area(item.area_blob)
    hooks = {0x80028A38: lambda cpu, regs: model.move_image(*regs[4:7])}
    result = cpu.call(item.root, args=item.args, hooks=hooks)
    events = tuple(model.events)
    c_model = Model(None)
    setup_c(side, item, c_model, base_image, base_scratch)
    c_result = side.run(item, c_model)
    return {"mips_v0": result[2], "mips_image": bytes(cpu.ram), "mips_scratch": bytes(cpu.scratch),
            "mips_events": events, "coverage": set(cpu.coverage), "c_v0": c_result,
            "c_image": bytes(side.main), "c_scratch": bytes(side.scratch), "c_events": tuple(c_model.events)}

def compare(item, outcome):
    if outcome["mips_v0"] != outcome["c_v0"]:
        return f"return MIPS={outcome['mips_v0']:08X} C={outcome['c_v0']:08X}"
    lo, hi = mr.STACK_WINDOW[0] - 0x80000000, mr.STACK_WINDOW[1] - 0x80000000
    mips_main = outcome["mips_image"]
    c_main = outcome["c_image"]
    if mips_main[:lo] != c_main[:lo] or mips_main[hi:] != c_main[hi:]:
        for off, (a, b) in enumerate(zip(mips_main[:lo] + mips_main[hi:], c_main[:lo] + c_main[hi:])):
            if a != b:
                addr = off if off < lo else off + (hi - lo)
                start = max(0, addr - 8)
                return f"RAM differs at {0x80000000 + addr:08X}: MIPS={a:02X} C={b:02X}; MIPS bytes {mips_main[start:start+24].hex()} C {c_main[start:start+24].hex()}"
    if outcome["mips_scratch"] != outcome["c_scratch"]:
        return "scratchpad differs"
    if outcome["mips_events"] != outcome["c_events"]:
        mips_events, c_events = outcome["mips_events"], outcome["c_events"]
        limit = min(len(mips_events), len(c_events))
        index = next((i for i in range(limit) if mips_events[i] != c_events[i]), limit)
        mips = mips_events[index] if index < len(mips_events) else "<end>"
        cside = c_events[index] if index < len(c_events) else "<end>"
        return f"SDK hook events differ at #{index} (lengths {len(mips_events)}/{len(c_events)}): MIPS {mips}; C {cside}"
    return None

def _first_function_coverage(cases, name):
    address, size, _ = FUNCS[name]
    pcs = set(range(address, address + size, 4))
    return next((case for case in cases if case["coverage"] & pcs), None)

MUTATION_CASES = {"AreaSetVramAnimList": "area_mixed"}
def mutate_source(source, function, variant):
    text = source.read_text(encoding="utf-8")
    before, after = (("type == 2u", "type == 4u") if variant == 0 else
                     ("+ 1u);", "+ 2u);"))
    assert before in text
    mutated = text.replace(before, after, 1)
    path = TASK_OUT / f"vramanim_mutant_{os.getpid()}_{list(FUNCS).index(function):02d}_{variant}.c"
    path.write_text(mutated, encoding="utf-8")
    return path

def mutant_result(side, item, base_image, base_scratch):
    model = Model(None)
    setup_c(side, item, model, base_image, base_scratch)
    result = side.run(item, model)
    return (u32(result), *side.snapshot(), tuple(model.events))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=lambda s: int(s, 0), default=0xA3E2026)
    parser.add_argument("--cases", type=int, default=64, help="deterministic random rectangle cases")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--skip-mutations", action="store_true")
    mode.add_argument("--mutate", action="store_true", help="run the default suite and both game mutations")
    args = parser.parse_args()
    os.environ["MIPS_BACKEND"] = "py"
    reg.declare("stack_window", f"{mr.STACK_WINDOW[0]:08X}..{mr.STACK_WINDOW[1]:08X}")

    exe = (ROOT / "extract/disc/SCUS_944.23").read_bytes()
    cpu = mr.MipsRam()
    cpu.load_exe(exe)
    baseline = cpu.snapshot()
    cpu.coverage = set()
    base_image, base_scratch = baseline
    side = CSide(build())
    specs = fixed_specs() + make_random_specs(args.cases, args.seed) + area_specs()
    counts = {name: 0 for name in FUNCS}
    case_outcomes = {}
    failures = []
    union_coverage = set()
    mips_model = Model(None)
    started = time.perf_counter()

    for item in specs:
        try:
            outcome = call_spec(cpu, side, item, mips_model, base_image, base_scratch,
                                collect_coverage=True)
            mismatch = compare(item, outcome)
            if mismatch:
                failures.append((item.name, mismatch))
            case_outcomes[item.name] = outcome
            union_coverage.update(outcome["coverage"])
            for name, (address, size, _) in FUNCS.items():
                if any(pc in outcome["coverage"] for pc in range(address, address + size, 4)):
                    counts[name] += 1
            area_detail = f" [{item.area_name}]" if item.area_name else ""
            print(f"{'FAIL' if mismatch else 'ok  '} {item.name}{area_detail}" + (f": {mismatch}" if mismatch else ""))
        except Exception as exc:  # report a scenario and keep the rest of the batch running
            failures.append((item.name, f"{type(exc).__name__}: {exc}"))
            print(f"FAIL {item.name}: {type(exc).__name__}: {exc}")

    items = [(name, address, size, True) for name, (address, size, _) in FUNCS.items()]
    coverage = reg.coverage_summary(items, union_coverage)
    reg.print_coverage(coverage)
    if any(row["uncovered_count"] or row["stale_declared"] for row in coverage.values()):
        failures.append(("coverage", "undeclared or stale instruction coverage gaps"))
    for name in FUNCS:
        if counts[name] == 0:
            failures.append((name, "no default scenario reached this original function"))

    mutations_caught = 0
    mutation_total = 0
    if not args.skip_mutations and not failures:
        source = ROOT / "src/render/ae_vram_anim.c"
        clean_by_name = {}
        for item in specs:
            out = case_outcomes.get(item.name)
            if out is not None:
                clean_by_name[item.name] = (out["c_v0"], out["c_image"], out["c_scratch"], out["c_events"])
        for function in FUNCS:
            target_name = MUTATION_CASES.get(function)
            target = (case_outcomes.get(target_name) if target_name else
                      _first_function_coverage([case_outcomes[n] for n in case_outcomes], function))
            if target is None:
                failures.append((function, "mutation target has no exercised scenario"))
                continue
            item = next(spec for spec in specs if case_outcomes.get(spec.name) is target)
            expected = clean_by_name[item.name]
            for variant in (0, 1):
                mutation_total += 1
                path = mutate_source(source, function, variant)
                mutant_dll = build(path, tag=f"vramanim_mut{list(FUNCS).index(function):02d}_{variant}")
                mutant_side = CSide(mutant_dll)
                actual = mutant_result(mutant_side, item, base_image, base_scratch)
                if actual == expected:
                    failures.append((f"mutation {function}/{variant}", f"not detected in scenario {item.name}"))
                    print(f"MUTATION MISSED {function}/{variant} via {item.name}")
                else:
                    mutations_caught += 1
                    print(f"mutation caught {function}/{variant} via {item.name}")
                try:
                    path.unlink()
                except OSError:
                    pass
                try:
                    mutant_dll.unlink()
                except OSError:
                    pass

    print("\ncase counts:")
    for name, count in counts.items():
        print(f"  {name:28s} {count}")
    print(f"\nmutations caught: {mutations_caught}/{mutation_total}")
    print(f"elapsed: {time.perf_counter() - started:.1f}s")
    if failures:
        print("\nfailures:")
        for name, reason in failures:
            print(f"  {name}: {reason}")
    passed = not failures and (args.skip_mutations or mutations_caught == 2 * len(FUNCS))
    default_run = args.cases == 64 and args.seed == 0xA3E2026 and not args.skip_mutations
    if default_run:
        results = reg.write_results(counts, coverage, passed,
                                    extra={"mutations_caught": mutations_caught,
                                           "mutations_total": mutation_total,
                                           "case_names": [s.name for s in specs]},
                                    function_names=list(FUNCS))
        print(f"results: {results.relative_to(ROOT) if results else 'not written'}")
    side.lib.ae_ram_destroy(side.ram)
    if failures or not passed:
        return 1
    return 0

if __name__ == "__main__":
    raise SystemExit(main())

