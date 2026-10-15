"""Check member 186's MCX native boundary.

The 80141EBC..801455B0 functions are the MCX serial/accessory driver and its
private command/interrupt handlers.  The host replaces that hardware-facing
module at its public Mcx* entry points.  This checker loads the real EXE,
member 186, and real area data, then compares each C hook dispatch with an
instruction-budgeted MIPS call intercepted at the same native boundary.
"""
from __future__ import annotations

import argparse
import csv
import ctypes as C
import math
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/mips"), str(ROOT / "tools")]
import dump_areas as da  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

OVERLAY = ROOT / "extract/members/186.bin"
OVERLAY_BASE = 0x80136000
AREA_BASE = 0x80154000
FUNCTION_START = 0x80141EBC
FUNCTION_END = 0x80145800
FUNCTION_BYTES = 12192
FUNCTION_COUNT = 197
ORACLE_INSTRUCTION_BUDGET = 64
EXPECTED_EXTERNAL_CALLS = {0x8002F260, 0x8002F290}
TEST_DATA = 0x80100000
M32 = 0xFFFFFFFF
OUT = ROOT / "extract/mips"
SOURCE = ROOT / "src/modes/ae_ovl186d.c"
HEADER = ROOT / "src/modes/ae_ovl186d.h"
INVENTORY = ROOT / "notes/inventory/functions.csv"

# Public MCX calls that cross the native system boundary.  Private MCXCMD and
# MCXDSR callbacks are internal to the native driver's protocol implementation.
EXPECTED_APIS = (
    ("McxSync", 0x8014236C),
    ("McxGetApl", 0x8014275C),
    ("McxExecApl", 0x801428E0),
    ("McxGetTime", 0x80142A90),
    ("McxGetMem", 0x80142BE4),
    ("McxSetMem", 0x80142E7C),
    ("McxReadDev", 0x8014311C),
    ("McxWriteDev", 0x801434CC),
    ("McxSetTime", 0x80143884),
    ("McxGetUIFS", 0x801439A4),
    ("McxSetUIFS", 0x80143A64),
    ("McxShowTrans", 0x80143F54),
    ("McxHideTrans", 0x80144084),
    ("McxCurrCtrl", 0x80144128),
    ("McxFlashAcs", 0x80144264),
    ("McxGetSerial", 0x80144344),
    ("McxSetLED", 0x80144400),
    ("McxCardType", 0x801444D4),
    ("McxGetInfo", 0x80144DF4),
    ("McxGetMcxInfo", 0x80144500),
    ("McxAllInfo", 0x8014464C),
    ("McxExecFlag", 0x80144850),
)
DEFAULT_CASES = 4
ARG_CASES = (
    (),
    ("output",),
    ("output", 0x00000000),
    ("output", 0xFFFF8000, 0x12345678, 0x80000000),
)
AREAS = (1, 2, 5)
EXTRA_COVERAGE = {}


@dataclass(frozen=True)
class NativeApi:
    name: str
    address: int


class C_AeRam(C.Structure):
    _fields_ = [
        ("main", C.c_uint8 * 0x200000),
        ("scratch", C.c_uint8 * 0x400),
        ("error", C.c_int),
        ("error_text", C.c_char * 192),
    ]


NativeCall = C.CFUNCTYPE(
    C.c_int32, C.c_void_p, C.POINTER(C_AeRam), C.c_char_p,
    C.c_uint32, C.c_uint32, C.POINTER(C.c_uint32),
)


class C_Hooks(C.Structure):
    _fields_ = [("user", C.c_void_p), ("native_call", NativeCall)]


def parse_api_table(source: str, header: str) -> list[NativeApi]:
    macros = {
        suffix: int(address, 16)
        for suffix, address in re.findall(
            r"(?m)^#define AE_OVL186D_FN_([A-Z0-9_]+)\s+0x([0-9A-Fa-f]+)u$",
            header,
        )
    }
    apis = []
    for name, macro in re.findall(
        r'\{"([A-Za-z0-9_]+)",\s*AE_OVL186D_FN_([A-Z0-9_]+)\}', source
    ):
        if macro not in macros:
            raise AssertionError(f"{name} references missing address macro {macro}")
        apis.append(NativeApi(name, macros[macro]))
    if tuple((api.name, api.address) for api in apis) != EXPECTED_APIS:
        raise AssertionError(
            "native API table differs from the expected member-186 entry points: "
            f"{[(api.name, hex(api.address)) for api in apis]!r}"
        )
    return apis


def load_function_manifest() -> list[tuple[str, int, int]]:
    functions = []
    with INVENTORY.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            entry = row["entry"]
            if not entry.startswith("ovl_186::"):
                continue
            address = int(entry.split("::", 1)[1], 16)
            if FUNCTION_START <= address < FUNCTION_END:
                functions.append((row["name"], address, int(row["size"])))
    functions.sort(key=lambda row: row[1])
    if len(functions) != FUNCTION_COUNT:
        raise AssertionError(f"expected {FUNCTION_COUNT} original functions, found {len(functions)}")
    if sum(size for _name, _address, size in functions) != FUNCTION_BYTES:
        raise AssertionError("member-186 native function byte count changed")
    for name, address, size in functions:
        if address + size > FUNCTION_END:
            raise AssertionError(f"function extends beyond member 186: {name} {address:08X}")
    return functions


def inventory_overlaps(functions: list[tuple[str, int, int]]) -> list[str]:
    overlaps = []
    for index, (address, size, name) in enumerate(
        (address, size, name) for name, address, size in functions
    ):
        for other_name, other_address, other_size in functions[:index]:
            if address < other_address + other_size:
                overlaps.append(
                    f"{other_name} {other_address:08X}..{other_address + other_size:08X} "
                    f"overlaps {name} {address:08X}..{address + size:08X}"
                )
    return overlaps


def audit_direct_calls(cpu: mr.MipsRam, functions: list[tuple[str, int, int]]) -> tuple[int, set[int]]:
    entries = {address for _name, address, _size in functions}
    calls = 0
    external_targets = set()
    seen_pcs = set()
    for name, start, size in functions:
        for pc in range(start, start + size, 4):
            if pc in seen_pcs:
                continue
            seen_pcs.add(pc)
            instruction = cpu.read32(pc)
            if instruction >> 26 != 3:  # JAL
                continue
            target = ((pc + 4) & 0xF0000000) | ((instruction & 0x03FFFFFF) << 2)
            if FUNCTION_START <= target < FUNCTION_END:
                if target not in entries:
                    raise AssertionError(
                        f"{name} calls unlisted member-186 code at {target:08X} (from {pc:08X})"
                    )
                calls += 1
            else:
                external_targets.add(target)
    if external_targets != EXPECTED_EXTERNAL_CALLS:
        raise AssertionError(
            "member-186 direct external calls changed: "
            f"{sorted(f'{target:08X}' for target in external_targets)!r}"
        )
    return calls, external_targets


def area_blob(area: int) -> bytes:
    member = da.area_members().get(area)
    if member is None:
        raise AssertionError(f"no real area member is mapped for area {area}")
    path = ROOT / f"extract/members/{member:03X}.bin"
    if not path.is_file():
        raise FileNotFoundError(path)
    return path.read_bytes()


def result_for(api_index: int, address: int, args: tuple[int, ...]) -> int:
    value = (address ^ ((api_index + 1) * 0x00010001) ^ (len(args) * 0x11111111)) & M32
    for index, argument in enumerate(args):
        value = ((value * 33) ^ argument ^ ((index + 1) * 0x9E3779B9)) & M32
    return value


def write_native_result(main: bytearray | None, args: tuple[int, ...], value: int) -> None:
    if not args:
        return
    output = args[0]
    if 0x80000000 <= output <= 0x801FFFFC:
        if main is not None:
            offset = output - 0x80000000
            for index, byte in enumerate(((value ^ 0xA5A55A5A) & M32).to_bytes(4, "little")):
                main[offset + index] = byte


def make_arguments(api_index: int, case_index: int) -> tuple[int, ...]:
    words = []
    for value in ARG_CASES[case_index]:
        if value == "output":
            words.append(TEST_DATA + api_index * 0x20 + case_index * 4)
        else:
            words.append(int(value) & M32)
    return tuple(words)


def compile_library(source: Path, output: Path) -> Path:
    compiler = shutil.which("gcc")
    if compiler is None:
        raise RuntimeError("gcc is required to build the MCX hook adapter")
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
         "-shared", "-I" + str(ROOT / "src/core"),
         "-I" + str(ROOT / "src/modes"),
         "-I" + str(source.parent),
         str(ROOT / "src/core/ae_ram.c"), str(source), "-o", str(output)],
        cwd=ROOT, check=True,
    )
    return output


class CAdapter:
    def __init__(self, dll: Path):
        self.lib = C.CDLL(str(dll))
        self.lib.ae_ram_create.restype = C.POINTER(C_AeRam)
        self.lib.ae_ram_destroy.argtypes = [C.POINTER(C_AeRam)]
        self.lib.ae_ram_failed.argtypes = [C.POINTER(C_AeRam)]
        self.lib.ae_ram_failed.restype = C.c_int
        self.lib.ae_ram_error.argtypes = [C.POINTER(C_AeRam)]
        self.lib.ae_ram_error.restype = C.c_char_p
        self.lib.ovl186d_call_native.argtypes = [
            C.POINTER(C_AeRam), C.POINTER(C_Hooks), C.c_uint32,
            C.c_uint32, C.POINTER(C.c_uint32),
        ]
        self.lib.ovl186d_call_native.restype = C.c_int32

    def new_ram(self, main: bytes, scratch: bytes) -> C.POINTER(C_AeRam):
        ram = self.lib.ae_ram_create()
        if not ram:
            raise MemoryError("ae_ram_create failed")
        base = C.addressof(ram.contents)
        C.memmove(base + C_AeRam.main.offset, main, len(main))
        C.memmove(base + C_AeRam.scratch.offset, scratch, len(scratch))
        return ram

    @staticmethod
    def main_bytes(ram: C.POINTER(C_AeRam)) -> bytes:
        return bytes(ram.contents.main)

    @staticmethod
    def scratch_bytes(ram: C.POINTER(C_AeRam)) -> bytes:
        return bytes(ram.contents.scratch)


class Harness:
    def __init__(self, apis: list[NativeApi], functions: list[tuple[str, int, int]],
                 c_adapter: CAdapter, mips_limit: int):
        self.apis = apis
        self.functions = functions
        self.c = c_adapter
        self.mips_limit = mips_limit
        self.counts = {api.name: 0 for api in apis}
        self.direct_calls = 0
        self.oracle_steps = 0

    def run(self, api_index: int, case_index: int, seed: int = 186) -> None:
        api = self.apis[api_index]
        args = make_arguments(api_index, case_index)
        area = AREAS[(api_index + case_index + seed) % len(AREAS)]
        cpu = mr.MipsRam.from_extract()
        cpu.load_area(OVERLAY.read_bytes(), OVERLAY_BASE)
        cpu.load_area(area_blob(area), AREA_BASE)
        initial_main, initial_scratch = bytes(cpu.ram), bytes(cpu.scratch)
        mips_events = []

        def mips_native_hook(machine, registers):
            actual_args = tuple(registers[4:4 + len(args)])
            if actual_args != args:
                raise AssertionError(
                    f"{api.name} o32 words differ: expected {args!r}, got {actual_args!r}"
                )
            value = result_for(api_index, api.address, actual_args)
            mips_events.append((api.name, api.address, len(actual_args), actual_args))
            write_native_result(machine.ram, actual_args, value)
            return value

        registers = cpu.call(
            api.address, args=args, hooks={api.address: mips_native_hook},
            limit=self.mips_limit,
        )
        self.oracle_steps += cpu.steps
        mips_result = registers[2] & M32

        c_events = []

        def c_native_hook(_user, ram_ptr, raw_name, address, argc, raw_args):
            name = raw_name.decode("ascii")
            call_args = tuple(int(raw_args[index]) for index in range(argc)) if argc else ()
            value = result_for(api_index, int(address), call_args)
            c_events.append((name, int(address), int(argc), call_args))
            write_native_result(ram_ptr.contents.main, call_args, value)
            return C.c_int32(value).value

        callback = NativeCall(c_native_hook)
        hooks = C_Hooks(None, callback)
        c_ram = self.c.new_ram(initial_main, initial_scratch)
        c_args = (C.c_uint32 * len(args))(*args) if args else None
        try:
            c_result = self.c.lib.ovl186d_call_native(
                c_ram, C.byref(hooks), api.address, len(args), c_args
            ) & M32
            if c_result != mips_result:
                raise AssertionError(
                    f"{api.name} return differs C={c_result:08X} MIPS={mips_result:08X}"
                )
            if c_events != mips_events:
                raise AssertionError(
                    f"{api.name} hook differs C={c_events!r} MIPS={mips_events!r}"
                )
            if self.c.main_bytes(c_ram) != bytes(cpu.ram):
                raise AssertionError(f"{api.name} RAM differs after native hook dispatch")
            if self.c.scratch_bytes(c_ram) != bytes(cpu.scratch):
                raise AssertionError(f"{api.name} scratchpad differs after native hook dispatch")
            if self.c.lib.ae_ram_failed(c_ram):
                error = self.c.lib.ae_ram_error(c_ram).decode("utf-8", "replace")
                raise AssertionError(f"{api.name} C adapter failed: {error}")
        finally:
            self.c.lib.ae_ram_destroy(c_ram)
        self.counts[api.name] += 1

    def finish(self, extra_coverage=EXTRA_COVERAGE, default_run=False,
               show_coverage=False):
        if extra_coverage:
            raise AssertionError("this native-only range has no original C coverage rows")
        if show_coverage:
            print(f"native-only member 186 functions: {len(self.functions)}")
            print("original instruction coverage: not applicable; the MCX module is a native boundary")
        if default_run:
            reg.write_results(
                self.counts, None, True,
                extra={
                    "native_only_functions": len(self.functions),
                    "native_api_count": len(self.apis),
                    "instruction_budget_per_oracle_call": self.mips_limit,
                    "oracle_instructions_executed": self.oracle_steps,
                    "direct_external_calls": [f"{address:08X}" for address in sorted(self.external_targets)],
                    "internal_direct_call_sites": self.direct_calls,
                    "inventory_overlaps": inventory_overlaps(self.functions),
                    "coverage_note": "native-only MCX code is intercepted at the public API boundary",
                },
                function_names=list(self.counts),
            )


def build_cpu_for_audit() -> mr.MipsRam:
    cpu = mr.MipsRam.from_extract()
    cpu.load_area(OVERLAY.read_bytes(), OVERLAY_BASE)
    cpu.load_area(area_blob(1), AREA_BASE)
    return cpu


def run_mutations(apis: list[NativeApi], base_source: str) -> int:
    """Mutate each public hook mapping in two observable ways."""
    OUT.mkdir(parents=True, exist_ok=True)
    mutations = (
        ("name", re.sub(
            r'\{"([A-Za-z0-9_]+)",\s*(AE_OVL186D_FN_[A-Z0-9_]+)\}',
            lambda match: '{"Mutated' + match.group(1) + '", ' + match.group(2) + '}',
            base_source,
        )),
        ("address", re.sub(
            r'(\{"[A-Za-z0-9_]+",\s*)(AE_OVL186D_FN_[A-Z0-9_]+)(\})',
            r'\1\2 + 4u\3', base_source,
        )),
    )
    caught = 0
    total = len(apis) * len(mutations)
    for mutation_name, mutated_source in mutations:
        if mutated_source == base_source:
            raise AssertionError(f"{mutation_name} mutation did not change the source")
        source_path = OUT / f"check_ovl186d_mutant_{mutation_name}.c"
        dll_path = OUT / f"check_ovl186d_mutant_{mutation_name}.dll"
        source_path.write_text(mutated_source, encoding="utf-8")
        try:
            adapter = CAdapter(compile_library(source_path, dll_path))
            for api_index, api in enumerate(apis):
                args = make_arguments(api_index, 3)
                events = []

                def mutation_hook(_user, _ram, raw_name, address, argc, raw_args):
                    name = raw_name.decode("ascii")
                    call_args = tuple(int(raw_args[i]) for i in range(argc)) if argc else ()
                    events.append((name, int(address), int(argc), call_args))
                    return 0x13572468

                callback = NativeCall(mutation_hook)
                hooks = C_Hooks(None, callback)
                ram = adapter.lib.ae_ram_create()
                c_args = (C.c_uint32 * len(args))(*args)
                try:
                    result = adapter.lib.ovl186d_call_native(
                        ram, C.byref(hooks), api.address, len(args), c_args
                    )
                    if mutation_name == "name":
                        if (len(events) != 1 or events[0][0] == api.name or
                                (result & M32) != 0x13572468):
                            raise AssertionError(f"{api.name} name mutation escaped detection")
                    else:
                        if events or not adapter.lib.ae_ram_failed(ram):
                            raise AssertionError(f"{api.name} address mutation escaped detection")
                    caught += 1
                finally:
                    adapter.lib.ae_ram_destroy(ram)
        finally:
            source_path.unlink(missing_ok=True)
    print(f"OK: caught {caught}/{total} native API mapping mutations (2 per public API)")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--cases", type=float, default=1.0)
    ap.add_argument("--seed", type=int, default=186)
    ap.add_argument("--function", choices=[name for name, _address in EXPECTED_APIS])
    ap.add_argument("--mutate", action="store_true")
    ap.add_argument("--mips-limit", type=int, default=ORACLE_INSTRUCTION_BUDGET,
                    help="maximum MIPS instructions for every oracle call")
    args = ap.parse_args()
    if args.cases <= 0:
        ap.error("--cases must be positive")
    if args.mips_limit <= 0:
        ap.error("--mips-limit must be positive")
    if args.mutate and (args.function is not None or args.cases != 1.0 or args.seed != 186):
        ap.error("--mutate cannot be combined with --function or non-default scale/seed")

    source_text = SOURCE.read_text(encoding="utf-8")
    header_text = HEADER.read_text(encoding="utf-8")
    apis = parse_api_table(source_text, header_text)
    functions = load_function_manifest()

    # This audit uses disassembly only, not MIPS execution.
    audit_cpu = build_cpu_for_audit()
    for _name, address, size in functions:
        if len(audit_cpu.read_bytes(address, size)) != size:
            raise AssertionError(f"unreadable code range at {address:08X}")
    direct_calls, external_targets = audit_direct_calls(audit_cpu, functions)

    if args.mutate:
        return run_mutations(apis, source_text)

    dll = compile_library(SOURCE, OUT / "check_ovl186d.dll")
    harness = Harness(apis, functions, CAdapter(dll), args.mips_limit)
    selected = list(range(len(apis)))
    if args.function:
        selected = [next(i for i, api in enumerate(apis) if api.name == args.function)]
    for api_index in selected:
        count = max(1, min(DEFAULT_CASES, math.ceil(DEFAULT_CASES * args.cases)))
        for case_index in range(count):
            harness.run(api_index, case_index, args.seed)
    harness.direct_calls = direct_calls
    harness.external_targets = external_targets
    harness.finish(
        EXTRA_COVERAGE,
        default_run=(args.function is None and args.cases == 1.0 and args.seed == 186),
        show_coverage=args.coverage,
    )
    print(" ".join(f"{name}={harness.counts[name]}" for name, _address in EXPECTED_APIS))
    print(f"native-only functions={len(functions)} internal direct calls={direct_calls}")
    print(f"MIPS oracle instructions executed={harness.oracle_steps}; per-call budget={args.mips_limit}")
    print("external native calls=" + ", ".join(f"{address:08X}" for address in sorted(external_targets)))
    for overlap in inventory_overlaps(functions):
        print(f"inventory span overlap: {overlap}")
    print(f"OK: {sum(harness.counts.values())} cases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
