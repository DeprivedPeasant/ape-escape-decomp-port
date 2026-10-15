"""Diff the AreaLoad setup/reset ports against the original SCUS-94423 instructions."""
import argparse
import ctypes as C
import random
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/mips"), str(ROOT / "tools")]
import dump_areas
import exclusion_registry as reg
import mips_ram as mr

FUNCS = {
    "AreaGetKind": (0x80010CA4, 28, "s", ("s",)),
    "AreaGetLevel": (0x80010CC0, 28, "s", ("s",)),
    "ModelInit": (0x80015AC0, 112, "v", ("u", "u")),
    "StaticModelInit": (0x80015D10, 72, "v", ("u", "u")),
    "AreaSetZoneBoxes": (0x80043380, 48, "v", ("u",)),
    "ProjectilesReset": (0x800592E0, 36, "v", ()),
    "BossKindForArea": (0x8005AB00, 108, "s", ()),
    "BossInit": (0x8005AB90, 152, "v", ()),
    "EffectsReset": (0x80069500, 72, "v", ()),
    "EffectsResetSubsystems": (0x8006A9B0, 72, "v", ()),
    "PlayerFxBlinkReset": (0x8006B7DC, 36, "v", ()),
    "SparkTableClear": (0x8006E02C, 40, "v", ()),
    "TrailsReset": (0x8006ED4C, 44, "v", ()),
    "EffectObjsReset": (0x8006FE54, 40, "v", ()),
    "RandomParticlesReset": (0x80070B48, 40, "v", ()),
    "FxTableClear12": (0x800710F8, 40, "v", ()),
    "IndicatorsReset": (0x800715A0, 20, "v", ()),
    "AreaSetIndicatorModelA": (0x8007163C, 88, "v", ("u",)),
    "AreaSetIndicatorModelB": (0x80071694, 88, "v", ("u",)),
    "AreaSpecialFlagsClear": (0x8007AFB8, 12, "v", ()),
    "MonkeyLodModelsInit": (0x8007BCB4, 132, "v", ()),
    "TargetListClear": (0x8007CD60, 40, "v", ()),
    "TableClearDAT_800BEF60": (0x8007D0F0, 32, "v", ()),
    "FxSlotsReset": (0x80084A60, 64, "v", ()),
    "ProjectileGroupsClear": (0x80085230, 36, "v", ()),
    "HeapInit": (0x8008C01C, 60, "v", ()),
    "AreaBumpAllocSet": (0x8008C058, 12, "v", ("u",)),
    "HeapAlloc": (0x8008C09C, 284, "u", ("u",)),
    "AreaBumpAlloc": (0x8008C1B8, 92, "u", ("u",)),
    "SmallocError": (0x8008C348, 72, "s", ("u",)),
    "MessageBoxClose": (0x8008DEE0, 12, "v", ()),
    "RcCarDespawn": (0x8009946C, 60, "v", ()),
    "AreaSetSpecialActor": (0x8009B3CC, 428, "v", ("u", "u")),
    "AreaSetFeature30": (0x8009B7C8, 28, "v", ("u",)),
    "AreaSetFeature31": (0x8009B7E4, 12, "v", ("u",)),
    "AreaSetFeature32": (0x8009B7F0, 12, "v", ("u",)),
    "exit": (0x800A3700, 12, "s", ("u",)),
    "printf": (0x800A3720, 12, "s", ("u", "u", "u", "u")),
    "FUN_8013d1ac": (0x8013D1AC, 0x174, "v", ()),
    "FUN_8013da10": (0x8013DA10, 0x180, "v", ()),
    "FUN_8013dae8": (0x8013DAE8, 0x258, "v", ()),
    "FUN_8013dc68": (0x8013DC68, 0x1BC, "v", ()),
    "FUN_8013df90": (0x8013DF90, 0x1BC, "v", ()),
}

AREA_HOOKS = (
    "Printf", "Exit", "ProgressGetByte",
    "FUN_8013d1f0", "Area44_Boss_SetAnim", "FUN_8013f68c",
    "FUN_8013f130", "FUN_8013ecf4", "FUN_8013d52c", "FUN_8013db98",
    "FUN_8013dc00", "FUN_8013d440", "Area46_Boss_SetAnim",
    "Area46_Boss_ClearFlag", "FUN_8013f544", "FUN_8013e14c",
    "FUN_8013ec5c", "FUN_8013d8bc", "FUN_8013df28", "FUN_8013d10c",
    "FUN_8013f4f0", "FUN_8013e8c0", "FUN_8013de14", "FUN_8013e4d8",
    "FUN_8013d804", "FUN_8013d000", "Area56_Boss_SetCameraObjAnim",
    "FUN_8013f86c", "FUN_8013e4a8", "FUN_8013d47c", "FUN_8013e070",
    "FUN_8013e7f8", "FUN_8013f080", "Area57_Boss_SetAnim",
    "Area57_Boss_ClearFlag", "FUN_8013e020", "FUN_8013d33c",
    "FUN_801407bc", "FUN_8013d9a8", "FUN_8013ee9c",
)
HOOK_ADDRESSES = {
    0x800963AC: "ProgressGetByte",
    0x8008806C: "SoundLoopStop",
}
HOOK_ARITY = {
    "SoundLoopStop": 2,
    "ProgressGetByte": 1, "Exit": 1, "Printf": 4,
    "FUN_8013d1f0": 3, "Area44_Boss_SetAnim": 2,
    "FUN_8013f68c": 0, "FUN_8013f130": 0, "FUN_8013ecf4": 0,
    "FUN_8013d52c": 0, "FUN_8013db98": 0, "FUN_8013dc00": 0,
    "FUN_8013d440": 3, "Area46_Boss_SetAnim": 2,
    "Area46_Boss_ClearFlag": 1, "FUN_8013f544": 0,
    "FUN_8013e14c": 0, "FUN_8013ec5c": 0, "FUN_8013d8bc": 0,
    "FUN_8013df28": 0, "FUN_8013d10c": 3, "FUN_8013f4f0": 0,
    "FUN_8013e8c0": 0, "FUN_8013de14": 0, "FUN_8013e4d8": 0,
    "FUN_8013d804": 0, "FUN_8013d000": 3,
    "Area56_Boss_SetCameraObjAnim": 1, "FUN_8013f86c": 0,
    "FUN_8013e4a8": 0, "FUN_8013d47c": 0, "FUN_8013e070": 0,
    "FUN_8013e7f8": 0, "FUN_8013f080": 0,
    "Area57_Boss_SetAnim": 2, "Area57_Boss_ClearFlag": 1,
    "FUN_8013e020": 0, "FUN_8013d33c": 0, "FUN_801407bc": 0,
    "FUN_8013d9a8": 0, "FUN_8013ee9c": 0,
}
OVERLAY_FOR_AREA = {0x44: 0x19E, 0x46: 0x1A0, 0x53: 0x1A5,
                    0x56: 0x1A6, 0x57: 0x1A8}
OVERLAY_HELPERS = {
    0x19E: {0x8013D1F0: "FUN_8013d1f0", 0x80140948: "Area44_Boss_SetAnim",
            0x8013F68C: "FUN_8013f68c", 0x8013F130: "FUN_8013f130",
            0x8013ECF4: "FUN_8013ecf4", 0x8013D52C: "FUN_8013d52c",
            0x8013DB98: "FUN_8013db98", 0x8013DC00: "FUN_8013dc00"},
    0x1A0: {0x8013D440: "FUN_8013d440", 0x8013EF0C: "Area46_Boss_SetAnim",
            0x8013EF00: "Area46_Boss_ClearFlag", 0x8013F544: "FUN_8013f544",
            0x8013E14C: "FUN_8013e14c", 0x8013EC5C: "FUN_8013ec5c",
            0x8013D8BC: "FUN_8013d8bc", 0x8013DF28: "FUN_8013df28"},
    0x1A5: {0x8013D10C: "FUN_8013d10c", 0x8013F4F0: "FUN_8013f4f0",
            0x8013E8C0: "FUN_8013e8c0", 0x8013DE14: "FUN_8013de14",
            0x8013E4D8: "FUN_8013e4d8", 0x8013D804: "FUN_8013d804"},
    0x1A6: {0x8013D000: "FUN_8013d000", 0x801406F8: "Area56_Boss_SetCameraObjAnim",
            0x8013F86C: "FUN_8013f86c", 0x8013E4A8: "FUN_8013e4a8",
            0x8013D47C: "FUN_8013d47c", 0x8013E070: "FUN_8013e070",
            0x8013E7F8: "FUN_8013e7f8", 0x8013F080: "FUN_8013f080"},
    0x1A8: {0x8013D000: "FUN_8013d000", 0x8013F3A4: "Area57_Boss_SetAnim",
            0x8013F398: "Area57_Boss_ClearFlag", 0x8013E020: "FUN_8013e020",
            0x8013D33C: "FUN_8013d33c", 0x801407BC: "FUN_801407bc",
            0x8013D9A8: "FUN_8013d9a8", 0x8013EE9C: "FUN_8013ee9c"},
}
BIOS_PRINT = 0xA0
BIOS_EXIT = 0xB0
STACK_LO = 0x1FF000
AREA_BASE = 0x80154000
AREA_STATE = 0x800F4350
STATE_LEVEL = 0x124
STATE_CURRENT_AREA = 0x126
STATE_AREA_COLLISION_TABLE = 0x244
STATE_MONKEY_LOD_MODEL = 0x1A4
HEAP_BLOCKS = 0x800BF690
HEAP_COUNT = 0x800BF790
HEAP_CURSOR = 0x800DF798
RC_CAR = 0x800F5A20
COLLISION_SLOTS = 0x800BBDA0
COLLISION_SLOT_STRIDE = 0x14
COLLISION_HEAD = 0x800B20D8
COLLISION_TAIL = 0x800B20DC
COLLISION_SLOT_COUNT = 256
RC_BODY = RC_CAR + 0x24
DEFAULT_SEED = 94423
AREA_IDS = (0x01, 0x07, 0x08, 0x09, 0x16, 0x21, 0x30, 0x33, 0x42,
            0x44, 0x46, 0x53, 0x56, 0x57)

P = C.c_void_p
U = C.c_uint32
I = C.c_int32
HOOK_FN = C.CFUNCTYPE(I, P, P, U, U, U, U)
SOUND_STOP_FN = C.CFUNCTYPE(None, P, P, I, I)


class AeBodyHooks(C.Structure):
    _fields_ = [("user", P), ("printf_error", P), ("sound_loop_start", P),
                ("sound_loop_stop", SOUND_STOP_FN), ("body_move", P),
                ("build_probe_points", P), ("body_stub_return_zero", P),
                ("collision_ray_test", P), ("gimmick_type_to_sfx", P),
                ("play_sfx_at", P), ("pickup_collect", P), ("camera_nudge", P)]


class AeAreaSetHooks(C.Structure):
    _fields_ = ([("user", P), ("body_hooks", P)] +
                [(name, HOOK_FN) for name in AREA_HOOKS])


class Memory:
    def __init__(self, ram=None, side=None):
        self.ram = ram
        self.side = side

    def read(self, address, size):
        if self.ram is not None:
            return self.ram.read(address, size)
        buf, offset = self._buffer(address)
        return int.from_bytes(bytes(buf[offset:offset + size]), "little")

    def write(self, address, value, size):
        if self.ram is not None:
            return self.ram.write(address, size, value)
        buf, offset = self._buffer(address)
        buf[offset:offset + size] = (value & ((1 << (size * 8)) - 1)).to_bytes(size, "little")

    def _buffer(self, address):
        if mr.SCRATCH_BASE <= address < mr.SCRATCH_BASE + mr.SCRATCH_SIZE:
            return self.side.scratch, address - mr.SCRATCH_BASE
        return self.side.main, address & 0x1FFFFF


class HookModel:
    def __init__(self, memory, context=None):
        self.memory = memory
        self.context = context or {}
        self.events = []

    def call(self, name, args):
        if name == "Printf" and args:
            # These are the only original printf calls in this function.
            # The first passes only a format string; the second also passes
            # the requested size and current heap-block count.
            count = {0x800A6340: 1, 0x800A6350: 3}.get(args[0] & 0xFFFFFFFF, 4)
        else:
            count = HOOK_ARITY.get(name, len(args))
        args = tuple(value & 0xFFFFFFFF for value in args[:count])
        self.events.append((name, args))
        return self.context.get(name, 0)

    def bios_call(self, cpu, regs):
        syscall = regs[9] & 0xFFFFFFFF
        name = "Printf" if syscall == 0x3F else "Exit" if syscall == 0x38 else None
        if name is None:
            raise AssertionError(f"unexpected BIOS syscall {syscall:08X}")
        return self.call(name, regs[4:8])


def build(source=None, tag="areaset", include_mutations=False):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{__import__('os').getpid()}_{time.time_ns()}.dll"
    exports = dll.with_suffix(".def")
    symbols = [
        "ae_ram_create", "ae_ram_destroy", "ae_ram_clear_error",
        "ae_ram_failed", "ae_ram_error", "ae_ptr",
    ] + ["ae_" + name for name in FUNCS]
    if include_mutations:
        symbols += [f"ae_mut_{name}_{variant}" for name in FUNCS for variant in range(2)]
    exports.write_text("EXPORTS\n" + "\n".join(symbols) + "\n", encoding="ascii")
    compiler = shutil.which("gcc")
    if not compiler:
        raise SystemExit("gcc not found on PATH")
    inputs = [
        str(ROOT / "src/core/ae_ram.c"), str(ROOT / "src/core/ae_sdk.c"),
        str(ROOT / "src/core/ae_sdk_matrix.c"), str(ROOT / "src/player/native_player.c"),
        str(ROOT / "src/player/ae_gadget.c"),
        str(ROOT / "src/world/ae_world.c"), str(ROOT / "src/world/ae_solid.c"),
        str(ROOT / "src/bodies/ae_bodies.c"), str(source or ROOT / "src/world/ae_areaset.c"),
    ]
    subprocess.run([
        compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
        "-Wl,--exclude-all-symbols",
        "-shared", "-static-libgcc", "-I" + str(ROOT / "src/world"), *inputs,
        str(exports), "-o", str(dll),
    ], check=True)
    return dll


class CSide:
    def __init__(self, dll, include_mutations=False):
        self.lib = C.CDLL(str(dll))
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
        self.hooks = AeAreaSetHooks()
        self.hooks.user = P(1)
        self.body_hooks = AeBodyHooks()
        self.body_hooks.user = P(1)
        self.sound_stop = SOUND_STOP_FN(
            lambda _user, _ram, sound_id, handle:
                self.model.call("SoundLoopStop", (sound_id, handle)))
        self.callbacks.append(self.sound_stop)
        self.body_hooks.sound_loop_stop = self.sound_stop
        self.hooks.body_hooks = C.cast(C.pointer(self.body_hooks), P)
        for name in AREA_HOOKS:
            callback = HOOK_FN(
                lambda _user, _ram, a0, a1, a2, a3, n=name:
                    self.model.call(n, (a0, a1, a2, a3)))
            self.callbacks.append(callback)
            setattr(self.hooks, name, callback)
        for name, (_address, _size, result_kind, arg_kinds) in FUNCS.items():
            self._set_signature("ae_" + name, result_kind, arg_kinds)
            if include_mutations:
                for variant in range(2):
                    self._set_signature(f"ae_mut_{name}_{variant}", result_kind, arg_kinds)

    def _set_signature(self, symbol, result_kind, arg_kinds):
        fn = getattr(self.lib, symbol)
        fn.argtypes = [P, C.POINTER(AeAreaSetHooks)] + [I if k == "s" else U for k in arg_kinds]
        fn.restype = None if result_kind == "v" else I if result_kind == "s" else U

    def sync(self, mips):
        C.memmove(self.main, bytes(mips.ram), mr.RAM_SIZE)
        C.memmove(self.scratch, bytes(mips.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)

    def invoke(self, name, args, symbol=None):
        fn = getattr(self.lib, symbol or ("ae_" + name))
        return fn(self.ram, C.byref(self.hooks), *args)


class AreaSetHarness:
    def __init__(self, dll, include_mutations=False):
        reg.declare("stack_window", "801FF000..80200000")
        self.area_members = dump_areas.area_members()
        self.blobs = {}
        self.cpus = {}
        self.coverage = set()
        for area in AREA_IDS:
            if area not in self.area_members:
                continue
            member = self.area_members[area]
            blob = (ROOT / f"extract/members/{member:03X}.bin").read_bytes()
            self.blobs[area] = blob
            cpu = mr.MipsRam.from_extract(ROOT / f"extract/members/{member:03X}.bin")
            overlay = OVERLAY_FOR_AREA.get(area)
            if overlay is not None:
                overlay_path = ROOT / f"extract/members/{overlay:03X}.bin"
                cpu.load_area(overlay_path.read_bytes(), 0x8013D000)
            cpu.coverage = self.coverage
            self.cpus[area] = cpu
        self.c = CSide(dll, include_mutations)
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0
        self.mips_steps = 0
        self.hook_calls = 0

    def fail(self, name, args, detail):
        self.failures += 1
        raise AssertionError(f"{name}{tuple(args)}: {detail}")

    def run_and_compare(self, name, area, args=(), setup=lambda _m: None,
                        context=None, symbol=None):
        if area not in self.cpus:
            raise AssertionError(f"no real area fixture for area {area:#x}")
        m = self.cpus[area]
        if not hasattr(m, "_area_set_base"):
            m._area_set_base = m.snapshot()
        m.restore(m._area_set_base)
        setup(m)
        self.c.sync(m)
        context = context or {}
        original_model = HookModel(Memory(ram=m), context)
        native_model = HookModel(Memory(side=self.c), context)
        self.c.model = native_model
        mips_hooks = {
            address: (lambda cpu, regs, n=hook: original_model.call(n, regs[4:8]))
            for address, hook in HOOK_ADDRESSES.items()
        }
        mips_hooks[BIOS_PRINT] = original_model.bios_call
        mips_hooks[BIOS_EXIT] = original_model.bios_call
        overlay = OVERLAY_FOR_AREA.get(area)
        for address, hook_name in OVERLAY_HELPERS.get(overlay, {}).items():
            mips_hooks[address] = (
                lambda _cpu, regs, name=hook_name:
                    original_model.call(name, regs[4:8]))
        # PsyQ exit is non-returning. The syscall oracle returns to the veneer,
        # so stop at SmallocError's post-exit return address instead of running
        # into the next function in the EXE image.
        mips_patches = {0x8008C384: lambda _cpu, _regs: mr.SENTINEL}
        m.hook_log.clear()
        before_steps = m.steps
        try:
            registers = m.call(FUNCS[name][0], args, hooks=mips_hooks,
                              patches=mips_patches)
        except mr.MipsError as error:
            machine = getattr(m, "_last_machine", None)
            pc = machine[0] if machine else FUNCS[name][0]
            tail = "\n".join(mr.listing(m, max(0x80010000, pc - 16), 32))
            raise mr.MipsError(f"{name}{tuple(args)} on area {area:#x}: {error}; last={machine}\n{tail}") from error
        self.mips_steps += m.steps - before_steps
        result = self.c.invoke(name, args, symbol)
        self.counts[name] += 1
        self.hook_calls += len(original_model.events)
        if FUNCS[name][2] != "v" and (int(result) & 0xFFFFFFFF) != (registers[2] & 0xFFFFFFFF):
            self.fail(name, args,
                      f"return differs: C={int(result) & 0xFFFFFFFF:08X} MIPS={registers[2]:08X}")
        expected = bytes(m.ram[:STACK_LO])
        actual = C.string_at(self.c.main, STACK_LO)
        if actual != expected:
            offset = next(i for i, (a, b) in enumerate(zip(actual, expected)) if a != b)
            self.fail(name, args,
                      f"area {area:#x}: RAM differs at {0x80000000 + offset:08X}: "
                      f"C={actual[offset]:02X} MIPS={expected[offset]:02X}")
        if bytes(self.c.scratch) != bytes(m.scratch):
            self.fail(name, args, "scratchpad differs")
        if original_model.events != native_model.events:
            self.fail(name, args,
                      f"hook calls differ: C={native_model.events!r}; MIPS={original_model.events!r}")
        if self.c.lib.ae_ram_failed(self.c.ram):
            detail = self.c.lib.ae_ram_error(self.c.ram).decode(errors="replace")
            self.fail(name, args, detail)

    def finish(self, show_coverage=False, default_run=False):
        items = [(name, address, size, True)
                 for name, (address, size, _result, _args) in FUNCS.items()]
        summary = reg.coverage_summary(items, self.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        gaps = {name: state for name, state in summary.items()
                if state["uncovered_count"] or state["stale_declared"]}
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps,
                              function_names=list(FUNCS))
        if gaps:
            for name, state in gaps.items():
                print(f"COVERAGE GAP {name}: {state['uncovered']}; stale={state['stale_declared']}")
            raise SystemExit("FAIL: undeclared area-setup instruction coverage gaps")
        return summary


def u16(blob, offset):
    return int.from_bytes(blob[offset:offset + 2], "little")


def u32(blob, offset):
    return int.from_bytes(blob[offset:offset + 4], "little")


def features_for(area, harness):
    blob = harness.blobs[area]
    header = u32(blob, 0x4D0)
    features = {}
    for index in range(8):
        entry = u32(blob, header + 0x34 + index * 4)
        feature_id = entry & 0xFF
        if feature_id:
            features[feature_id] = AREA_BASE + header + (entry >> 8) * 4
    return features


def all_features(harness, feature_id):
    for area in sorted(harness.blobs):
        address = features_for(area, harness).get(feature_id)
        if address:
            yield area, address


def model_header(memory, feature):
    return feature + memory.read(feature + 4, 2) * 4


def static_model_header(memory, feature):
    return feature + memory.read(feature + 0x0C, 2) * 4


def monkey_model_header(area, harness):
    blob = harness.blobs[area]
    monkey_data = u32(blob, 0x4E4)
    if not monkey_data or monkey_data + 20 > len(blob) or u32(blob, monkey_data) != 0x11:
        return None
    return AREA_BASE + monkey_data + u32(blob, monkey_data + 8)


def dirty(m, address, size, seed):
    rng = random.Random(seed)
    m.write_bytes(address, bytes(rng.getrandbits(8) for _ in range(size)))


def clean_collision_bodies(m):
    m.write32(COLLISION_HEAD, 0)
    m.write32(COLLISION_TAIL, 0)
    for index in range(COLLISION_SLOT_COUNT):
        slot = COLLISION_SLOTS + index * COLLISION_SLOT_STRIDE
        m.write16(slot, 0xFFFF)
        m.write16(slot + 2, 0xFFFF)
        m.write32(slot + 4, 0)
        m.write32(slot + 8, 0)
        m.write32(slot + 0x0C, 0)
        m.write32(slot + 0x10, 0)


def random_reset_setup(m, regions, seed):
    for index, (address, size) in enumerate(regions):
        dirty(m, address, size, seed + index)


def clean_heap(m, cursor=0x80102800, count=0, occupied=()):
    for index in range(64):
        m.write32(HEAP_BLOCKS + index * 4, 0)
    for index, value in occupied:
        m.write32(HEAP_BLOCKS + index * 4, value)
    m.write32(HEAP_COUNT, count)
    m.write32(HEAP_CURSOR, cursor)


def real_cases(h):
    runs = {name: [] for name in FUNCS}

    def add(name, area=0x01, args=(), setup=lambda _m: None, context=None):
        runs[name].append((area, args, setup, context))

    # Area lookup rows are read from the actual EXE table, including several worlds and levels.
    for area in AREA_IDS:
        add("AreaGetKind", area, (area,))
        add("AreaGetLevel", area, (area,))
    add("AreaGetKind", 0x01, (0x100,))
    add("AreaGetLevel", 0x01, (0x100,))

    # Model pointers come from the real feature records and model headers in each loaded member.
    for area, feature in all_features(h, 0x10):
        add("AreaSetIndicatorModelA", area, (feature,))
        add("ModelInit", area,
            (0x800E8000, model_header(Memory(ram=h.cpus[area]), feature)))
    for area, feature in all_features(h, 0x11):
        add("AreaSetIndicatorModelB", area, (feature,))
    actor40 = next(all_features(h, 0x40))
    actor41 = next(all_features(h, 0x41))
    area40, feature40 = actor40
    area41, feature41 = actor41
    add("StaticModelInit", area40,
        (0x800E8100, static_model_header(Memory(ram=h.cpus[area40]), feature40)))

    zone_area, zone_feature = next(all_features(h, 0x08))
    add("AreaSetZoneBoxes", zone_area, (zone_feature,))
    add("AreaSetZoneBoxes", zone_area, (0,))

    # Area feature 0x40 has both skinned and static model offsets; 0x41 has the skinned model only.
    for level in (4, 12, 21, 22, 33, 19, 3, 34):
        def actor_setup(m, level=level):
            clean_heap(m, cursor=0x80152000)
            dirty(m, 0x800B4DC0, 0x18, 1000 + level)
            m.write16(AREA_STATE + STATE_LEVEL, level)
        add("AreaSetSpecialActor", area40, (feature40, 1), actor_setup)
    for level in (12, 22):
        def actor41_setup(m, level=level):
            clean_heap(m, cursor=0x80152000)
            dirty(m, 0x800B4DC0, 0x18, 2000 + level)
            m.write16(AREA_STATE + STATE_LEVEL, level)
        add("AreaSetSpecialActor", area41, (feature41, 2), actor41_setup)

    # The monkey LOD model pointer is based on the real monkey subsection and its model offset.
    for area in (0x01, 0x07, 0x09, 0x16, 0x21):
        header = monkey_model_header(area, h)
        if header is not None:
            def lod_setup(m, header=header):
                m.write32(AREA_STATE + STATE_MONKEY_LOD_MODEL, header)
                m.write16(AREA_STATE + STATE_LEVEL, 1)
            add("MonkeyLodModelsInit", area, (), lod_setup)
    def lod_skip(m):
        m.write16(AREA_STATE + STATE_LEVEL, 0x20)
    add("MonkeyLodModelsInit", 0x01, (), lod_skip)

    # Boss tags execute the matching real code overlay through the EXE dispatch table.
    boss_areas = {0x44: 0x44, 0x53: 0x53, 0x56: 0x56,
                  0x46: 0x46, 0x57: 0x57}
    for area_tag in (0x44, 0x53, 0x56, 0x46, 0x57, 0x45):
        fixture_area = boss_areas.get(area_tag, 0x01)
        def boss_setup(m, area_tag=area_tag, fixture_area=fixture_area):
            m.write16(AREA_STATE + STATE_CURRENT_AREA, area_tag)
            m.write8(0x800AF840, 0xFE)
            if area_tag in boss_areas:
                clean_collision_bodies(m)
                collision_offset = u32(h.blobs[fixture_area], 0x4E0)
                m.write32(AREA_STATE + STATE_AREA_COLLISION_TABLE,
                          AREA_BASE + collision_offset)
        add("BossKindForArea", 0x01, (), boss_setup)
        add("BossInit", fixture_area, (), boss_setup)

    for name, area, progress in (
            ("FUN_8013d1ac", 0x53, None),
            ("FUN_8013da10", 0x57, 0), ("FUN_8013da10", 0x57, 5),
            ("FUN_8013dae8", 0x56, 0), ("FUN_8013dae8", 0x56, 5),
            ("FUN_8013dc68", 0x44, 0), ("FUN_8013dc68", 0x44, 5),
            ("FUN_8013df90", 0x46, 0), ("FUN_8013df90", 0x46, 5)):
        def boss_overlay_setup(m, area=area):
            clean_collision_bodies(m)
            collision_offset = u32(h.blobs[area], 0x4E0)
            m.write32(AREA_STATE + STATE_AREA_COLLISION_TABLE,
                      AREA_BASE + collision_offset)
            m.write8(0x800AF840, 0xFE)
        context = None if progress is None else {"ProgressGetByte": progress}
        add(name, area, (), boss_overlay_setup, context)

    # All other setup and reset functions are dirtied before clearing, across real member images.
    reset_regions = {
        "ProjectilesReset": (0x800B8D30, 16 * 0x30),
        "EffectsReset": (0x800B9900, 100 * 0x34),
        "EffectsResetSubsystems": (0x800BB920, 0x400),
        "PlayerFxBlinkReset": (0x800BAD70, 16),
        "SparkTableClear": (0x800BAEA0, 8 * 0x0C),
        "TrailsReset": (0x800F5010, 8 * 0x10),
        "EffectObjsReset": (0x800BAF00, 16 * 0x54),
        "RandomParticlesReset": (0x800BB840, 8 * 0x1C),
        "FxTableClear12": (0x800BB920, 12 * 0x44),
        "IndicatorsReset": (0x800BBC50, 0x150),
        "AreaSpecialFlagsClear": (0x800F5120, 4),
        "TargetListClear": (0x800F51F0, 32 * 4),
        "TableClearDAT_800BEF60": (0x800BEF60, 6),
        "FxSlotsReset": (0x800F5380, 32 * 0x34),
        "ProjectileGroupsClear": (0x800BF300, 4 * 0x1C),
        "HeapInit": (0x800BF690, 0x104),
        "MessageBoxClose": (0x800F5A00, 1),
    }
    for name, (base, size) in reset_regions.items():
        for fixture_area in (0x01, 0x07, 0x42):
            def reset_setup(m, base=base, size=size, is_heap=(name == "HeapInit"),
                            seed=0x94423 + fixture_area + base):
                dirty(m, base, size, seed)
                if is_heap:
                    m.write32(HEAP_COUNT, 0xABCD1234)
                    m.write32(HEAP_CURSOR, 0x80105555)
            add(name, fixture_area, (), reset_setup)

    # Shared leaf resets are also called directly so their individual coverage is recorded.
    add("IndicatorsReset", 0x09, (), lambda m: dirty(m, 0x800BBC50, 0x150, 73))
    area30, feature30 = next(all_features(h, 0x30))
    add("AreaSetFeature30", area30, (feature30,))
    add("AreaSetFeature30", area30, (0,))
    area31, feature31 = next(all_features(h, 0x31))
    area32, feature32 = next(all_features(h, 0x32))
    add("AreaSetFeature31", area31, (feature31,))
    add("AreaSetFeature31", area31, (0,))
    add("AreaSetFeature32", area32, (feature32,))
    add("AreaSetFeature32", area32, (0,))

    # Heap boundaries, fragmentation, invalid sizes, and the fallback from the area bump pool.
    for byte_count in (0, 1, 2048, 2049):
        add("HeapAlloc", 0x01, (byte_count,), lambda m: clean_heap(m))
    add("HeapAlloc", 0x07, (1,), lambda m: clean_heap(m, count=3,
        occupied=((3, 0x80104000), (5, 0x80105000))))
    add("HeapAlloc", 0x09, (4097,), lambda m: clean_heap(m, count=0,
        occupied=((1, 0x80104000), (3, 0x80105000))))
    add("HeapAlloc", 0x16, (1,), lambda m: clean_heap(m, count=63))
    add("HeapAlloc", 0x21, (1,), lambda m: clean_heap(m, count=64))
    add("HeapAlloc", 0x21, (2049,), lambda m: clean_heap(m, count=63))
    add("HeapAlloc", 0x42, (0x7FFFFFFF,), lambda m: clean_heap(m))
    add("HeapAlloc", 0x42, (0xFFFFF800,), lambda m: clean_heap(m))

    for cursor, byte_count in ((0x80110000, 1), (0x801027FC, 3),
                               (0x801027FE, 1), (0x801027FF, 1),
                               (0x80104000, 0x1000)):
        def bump_setup(m, cursor=cursor):
            clean_heap(m, cursor=cursor)
        add("AreaBumpAlloc", 0x07, (byte_count,), bump_setup)
    add("AreaBumpAllocSet", 0x01, (0x80123450,))
    add("SmallocError", 0x01, (0x321,), lambda m: m.write32(HEAP_COUNT, 7))

    # RC-car teardown covers inactive, missing slot, and a registered list-head body.
    add("RcCarDespawn", 0x01, (), lambda m: m.write32(RC_CAR + 0x70, 0))
    def rc_missing(m):
        m.write32(RC_CAR + 0x70, 1)
        m.write32(COLLISION_HEAD, 0)
        m.write32(COLLISION_TAIL, 0)
    add("RcCarDespawn", 0x07, (), rc_missing)
    def rc_registered(m):
        slot = COLLISION_SLOTS
        m.write32(RC_CAR + 0x70, 1)
        m.write16(slot, 1)
        m.write16(slot + 2, 0)
        m.write32(slot + 4, RC_BODY)
        m.write32(slot + 8, 0)
        m.write32(slot + 0x0C, 0)
        m.write32(slot + 0x10, 0)
        m.write32(COLLISION_HEAD, slot)
        m.write32(COLLISION_TAIL, slot)
    add("RcCarDespawn", 0x09, (), rc_registered)

    for name, area, size in (("ProjectilesReset", 0x01, 0x30),
                             ("EffectsReset", 0x07, 0x34),
                             ("EffectsResetSubsystems", 0x42, 0x400),
                             ("SparkTableClear", 0x09, 0x60),
                             ("TrailsReset", 0x16, 0x80),
                             ("EffectObjsReset", 0x21, 0x540),
                             ("RandomParticlesReset", 0x33, 0xE0),
                             ("FxTableClear12", 0x2D, 0x330),
                             ("FxSlotsReset", 0x42, 0x680),
                             ("ProjectileGroupsClear", 0x09, 0x70),
                             ("TargetListClear", 0x16, 0x80)):
        if area in h.cpus and name in reset_regions:
            base, _ = reset_regions[name]
            add(name, area, (), lambda m, base=base, size=size, seed=area + base:
                dirty(m, base, size, seed))

    add("exit", 0x01, (1,))
    add("printf", 0x01, (0x800A6340, 0x1234, 0x5678, 0x9ABC))
    return runs


def add_reset_leaf_runs(runs):
    for name, base, size in (
        ("PlayerFxBlinkReset", 0x800BAD70, 16),
        ("AreaSpecialFlagsClear", 0x800F5120, 4),
        ("TargetListClear", 0x800F51F0, 128),
        ("TableClearDAT_800BEF60", 0x800BEF60, 6),
        ("ProjectileGroupsClear", 0x800BF300, 112),
        ("HeapInit", 0x800BF690, 260),
        ("MessageBoxClose", 0x800F5A00, 1),
    ):
        if not runs[name]:
            runs[name].append((0x01, (), lambda m, base=base, size=size:
                               dirty(m, base, size, base)))


def suite(h, scale=1.0, seed=DEFAULT_SEED):
    runs = real_cases(h)
    add_reset_leaf_runs(runs)
    for name in FUNCS:
        if not runs[name]:
            raise AssertionError(f"no cases defined for {name}")
    if scale > 1.0:
        rng = random.Random(seed)
        reps = int((scale - 1.0) * 8)
        random_reset_regions = {
            "ProjectilesReset": ((0x800B8D30, 16 * 0x30),),
            "EffectsReset": (
                (0x800B9900, 100 * 0x34), (0x800BAD50, 4),
                (0x800BAD70, 16), (0x800BAEA0, 8 * 0x0C),
                (0x800BAF00, 16 * 0x54), (0x800BB840, 8 * 0x1C),
                (0x800BB920, 12 * 0x44), (0x800F5010, 8 * 0x10),
            ),
            "TargetListClear": ((0x800F51F0, 32 * 4),),
        }
        for index in range(reps):
            for name in ("ProjectilesReset", "EffectsReset", "TargetListClear",
                         "AreaGetKind", "AreaGetLevel"):
                area = rng.choice(AREA_IDS)
                args = (area,) if name in ("AreaGetKind", "AreaGetLevel") else ()
                if name in random_reset_regions:
                    regions = random_reset_regions[name]
                    seed_value = rng.getrandbits(32)
                    setup = lambda m, ranges=regions, seed=seed_value: \
                        random_reset_setup(m, ranges, seed)
                else:
                    setup = lambda _m: None
                runs[name].append((area, args, setup, None))
            heap_size = rng.getrandbits(32)
            heap_count = rng.randrange(65)
            occupied = tuple((slot, 0x80104000 + slot * 0x1000)
                             for slot in range(heap_count, 64)
                             if rng.randrange(5) == 0)
            runs["HeapAlloc"].append((rng.choice((0x01, 0x07, 0x42)),
                                      (heap_size,),
                                      lambda m, count=heap_count, blocks=occupied:
                                          clean_heap(m, count=count, occupied=blocks),
                                      None))
            bump_cursor = rng.choice((0x801027FC, 0x80102800, 0x80104000,
                                      0x80110000, rng.getrandbits(32)))
            bump_size = rng.getrandbits(16)
            runs["AreaBumpAlloc"].append((0x07, (bump_size,),
                                          lambda m, cursor=bump_cursor:
                                              clean_heap(m, cursor=cursor), None))

        for name, area, progress_id, row_count, active_kind in (
                ("FUN_8013d1ac", 0x53, None, 8, 0xD4),
                ("FUN_8013da10", 0x57, 12, 0, 0),
                ("FUN_8013dae8", 0x56, 3, 7, 0xD4),
                ("FUN_8013dc68", 0x44, 0, 2, 200),
                ("FUN_8013df90", 0x46, 8, 0, 0)):
            for index in range(reps):
                random_kinds = tuple(active_kind if rng.randrange(2) else rng.choice((0, 1, 0xFFFF))
                                     for _ in range(row_count))
                random_position = tuple(rng.randrange(-32768, 32768) for _ in range(3))

                def random_boss_setup(m, area=area, kinds=random_kinds,
                                      position=random_position, row_count=row_count):
                    clean_collision_bodies(m)
                    blob = h.blobs[area]
                    collision_offset = u32(blob, 0x4E0)
                    collision = AREA_BASE + collision_offset
                    m.write32(AREA_STATE + STATE_AREA_COLLISION_TABLE, collision)
                    if row_count:
                        records_offset = collision_offset + u32(blob, collision_offset + 8)
                        if records_offset + row_count * 0x20 > len(blob):
                            raise AssertionError(f"boss record table outside area {area:#x}")
                        for row, kind in enumerate(kinds):
                            m.write16(AREA_BASE + records_offset + row * 0x20, kind)
                    if area == 0x57:
                        for axis, value in enumerate(position):
                            m.write16(0x80141CC8 + axis * 2, value)

                context = None if progress_id is None else {
                    "ProgressGetByte": rng.choice((0, 1, 4, 5, 0xFF))}
                runs[name].append((area, (), random_boss_setup, context))
    return runs


def function_span(source, name):
    match = re.search(rf"(?m)^(?:int32_t|uint32_t|void)\s+(ae_{re.escape(name)})\s*\(", source)
    if not match:
        raise AssertionError(f"missing production function ae_{name}")
    opening = source.index("{", match.start())
    depth = 1
    index = opening + 1
    while depth:
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
        index += 1
    return match.start(), opening, index - 1


def mutation_source(original):
    clones = []
    for index, name in enumerate(FUNCS):
        start, opening, end = function_span(original, name)
        signature = original[start:opening].replace("ae_" + name,
                                                   f"ae_mut_{name}_0", 1)
        body = original[opening + 1:end]
        clones.append(signature + "{\n    ae_wr32(r, 0x800EFFFCu, 0xA5A50001u);" + body + "}\n")
        signature = original[start:opening].replace("ae_" + name,
                                                   f"ae_mut_{name}_1", 1)
        clones.append(signature + "{\n    ae_wr32(r, 0x800EFFFCu, 0x5A5A0002u);" + body + "}\n")
    return '#include "../../src/world/ae_areaset.c"\n\n' + "\n".join(clones)


def run_mutations(h, runs):
    original = (ROOT / "src/world/ae_areaset.c").read_text(encoding="utf-8")
    mutant_source = ROOT / "extract/mips/areaset_mutants.c"
    mutant_source.write_text(mutation_source(original), encoding="utf-8")
    h.c = CSide(build(mutant_source, "areaset_mutants", include_mutations=True),
                include_mutations=True)
    tested = caught = 0
    survivors = []
    for name in FUNCS:
        area, args, setup, context = runs[name][0]
        for variant in range(2):
            tested += 1
            try:
                h.run_and_compare(name, area, args, setup, context,
                                  symbol=f"ae_mut_{name}_{variant}")
            except AssertionError as error:
                if "RAM differs" not in str(error):
                    raise
                caught += 1
            else:
                survivors.append(f"{name} mutation {variant}")
    print(f"Mutations: {tested}; caught: {caught}; survivors: {len(survivors)}")
    for survivor in survivors:
        print("SURVIVED", survivor)
    return int(bool(survivors) or tested < 30)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--cases", type=float, default=1.0)
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    args = parser.parse_args()
    if not (ROOT / "extract/disc/SCUS_944.23").is_file():
        raise SystemExit("missing extract/disc/SCUS_944.23")
    t0 = time.time()
    harness = AreaSetHarness(build(), include_mutations=False)
    runs = real_cases(harness)
    add_reset_leaf_runs(runs)
    for name in FUNCS:
        if not runs[name]:
            raise SystemExit(f"no cases defined for {name}")
    if args.mutate:
        return run_mutations(harness, runs)
    if args.cases > 1.0:
        runs = suite(harness, args.cases, args.seed)
    for name, cases in runs.items():
        for area, call_args, setup, context in cases:
            harness.run_and_compare(name, area, call_args, setup, context)
    print("cases per function:")
    for name, count in harness.counts.items():
        print(f"  {name:34s} {count}")
    print(f"total cases {sum(harness.counts.values())}; MIPS steps {harness.mips_steps}; "
          f"hook calls {harness.hook_calls}; elapsed {time.time() - t0:.1f}s")
    harness.finish(show_coverage=args.coverage,
                   default_run=(args.cases == 1.0 and args.seed == DEFAULT_SEED))
    print("OK: the C ports match the original instructions for all configured cases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
