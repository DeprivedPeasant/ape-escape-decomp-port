"""Differential checks for the three newly named Zako dispatch handlers.

The source is compiled with the previously verified Zako helpers. Remaining
EXE helper calls and overlay entries are intercepted by their original names
on both sides. The default cases use real area 01 / 02 members, vary all three
target functions, and exercise the state-table and stage-level branches.
"""
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
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402
import check_zako1 as z1  # noqa: E402

U32, I32, P = C.c_uint32, C.c_int32, C.c_void_p
STACK_LO, STACK_HI = 0x801FF000, 0x80200000
MUTATION_SELECTOR, MUTATION_MARKER = 0x801FEF00, 0x800B4DE4
OBJECT = 0x800E6960
OBJECT_SIZE = 0xE4
ANIM_TABLE = 0x80185000
PLAYER = 0x800EC1E0
LINKED = 0x800E6B00
SOUND_EFFECTS = 0x800B2D08
SOUND_BANK_IDS = 0x800BF670
STAGE_STATE = 0x800F4350

# name -> (entry, instruction bytes, return kind)
FUNCS = {
    "ZakoUpdateGenericEnemy": (0x80053550, 0x464, "v"),
    "ZakoUpdateStageSpecificEnemy": (0x800546DC, 0xA4, "v"),
    "ZakoAreaModeNoop": (0x80057680, 8, "v"),
}

# Overlay callees stay behind named hooks. The zako helpers in this batch now
# run as C ports on the native side and original instructions on the MIPS side.
AI_HOOKS = {
    "FUN_8013DF54": (0x8013DF54, 1),
    "FUN_80140880": (0x80140880, 1),
    "FUN_8013F504": (0x8013F504, 1),
    "FUN_8014116C": (0x8014116C, 1),
}

AI_HOOK_FIELDS = [
    "FUN_8013DF54", "FUN_80140880", "FUN_8013F504", "FUN_8014116C",
]
AIHOOKFN = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32, U32, U32, U32)


class ZakoAiHooks(C.Structure):
    _fields_ = [("user", P), ("zako1_hooks", C.POINTER(z1.Zako1Hooks))] + [
        (name, AIHOOKFN) for name in AI_HOOK_FIELDS
    ]


def build(source=None, tag="zakoai"):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{time.time_ns()}.dll"
    sources = [
        "src/core/ae_ram.c", "src/core/ae_sdk.c", "src/core/ae_sdk_matrix.c",
        "src/core/ae_pose.c", "src/player/native_player.c", "src/bodies/ae_bodies.c",
        "src/world/ae_world.c",
        "src/effects/ae_spawn.c", "src/sound/ae_sound.c",
        "src/world/ae_zako_init.c", "src/world/ae_zako1.c",
        "src/world/ae_zakohelpers.c",
        str(source or ROOT / "src/world/ae_zakoai.c"),
    ]
    subprocess.run([shutil.which("gcc"), "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-Isrc/world", "-shared", "-static-libgcc", *sources, "-o", str(dll), "-lm"],
                   cwd=ROOT, check=True)
    return dll


class ScenarioModel(z1.HookModel):
    def __init__(self, mem, returns=None):
        super().__init__(mem)
        self.returns = returns or {}

    @staticmethod
    def _s32(value):
        value &= 0xFFFFFFFF
        return value - 0x100000000 if value & 0x80000000 else value

    @staticmethod
    def _s16(value):
        value &= 0xFFFF
        return value - 0x10000 if value & 0x8000 else value

    @staticmethod
    def _trunc_div(numerator, denominator):
        quotient = abs(numerator) // abs(denominator)
        return -quotient if (numerator < 0) != (denominator < 0) else quotient

    def atan2_safe(self, args):
        x = self._s32(args[0])
        z = self._s32(args[1])
        if z == 0:
            return 0x800 if x < 0 else 0

        # ae_Atan2Safe forwards (z, x) to the PsyQ ratan2 table routine.
        a0, a1 = z, x
        sign_x = 0
        sign_z = 0
        if a1 < 0:
            sign_x = 1
            a1 = self._s32(-a1)
        if a0 < 0:
            sign_z = 1
            a0 = self._s32(-a0)
        if a0 == 0 and a1 == 0:
            return 0

        if a0 < a1:
            if a0 & 0x7FE00000:
                denominator = a1 >> 10
                ratio = self._trunc_div(a0, denominator)
            else:
                ratio = self._trunc_div(a0 << 10, a1)
            angle = self._s16(self.mem.rd16(0x800B0000 + (ratio << 1) + 17456))
        else:
            if a1 & 0x7FE00000:
                denominator = a0 >> 10
                ratio = self._trunc_div(a1, denominator)
            else:
                ratio = self._trunc_div(a1 << 10, a0)
            angle = 1024 - self._s16(
                self.mem.rd16(0x800B0000 + (ratio << 1) + 17456))
        if sign_x:
            angle = 2048 - angle
        if sign_z:
            angle = -angle
        return self._s32(angle)

    def call(self, name, args):
        if name == "Atan2Safe":
            args = tuple(x & 0xFFFFFFFF for x in args[:2])
            self.events.append((name, args, ()))
            return self.returns.get(name, self.atan2_safe(args))
        result = super().call(name, args)
        return self.returns.get(name, result)


class Harness:
    def __init__(self, dll):
        reg.declare("stack_window", "801FF000..80200000")
        members = dump_areas.area_members()
        self.bases = {
            area: mr.MipsRam.from_extract(
                ROOT / f"extract/members/{members[area]:03X}.bin").snapshot()
            for area in (1, 2)
        }
        self.cpu = mr.MipsRam.from_extract(ROOT / f"extract/members/{members[1]:03X}.bin")
        self.cpu.attach_gte_oracle()
        self.cpu.coverage = set()
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0
        self.ai_hooks = None
        self._ai_callbacks = []
        self.set_c(dll)

    def set_c(self, dll):
        """Swap C builds without rebuilding the process-shared GTE oracle."""
        self.c = z1.CSide(dll)
        self.ai_fn = self.c.dll.ae_ZakoUpdateGenericEnemy
        self.ai_fn.argtypes = [P, C.POINTER(ZakoAiHooks), U32]
        self.ai_fn.restype = None
        self.stage_fn = self.c.dll.ae_ZakoUpdateStageSpecificEnemy
        self.stage_fn.argtypes = [P, C.POINTER(ZakoAiHooks), U32]
        self.stage_fn.restype = None
        self.noop_fn = self.c.dll.ae_ZakoAreaModeNoop
        self.noop_fn.argtypes = [P]
        self.noop_fn.restype = None

    def _bind_ai_hooks(self, model):
        hooks = ZakoAiHooks()
        hooks.zako1_hooks = C.pointer(self.c.hooks)
        callbacks = []
        for name in AI_HOOK_FIELDS:
            argc = AI_HOOKS[name][1]
            def callback(_user, ram, *args, hook_name=name, arg_count=argc):
                model.mem = z1.CMem(model.dll, ram)
                return model.call(hook_name, args[:arg_count])
            cb = AIHOOKFN(callback)
            setattr(hooks, name, cb)
            callbacks.append(cb)
        self.ai_hooks = hooks
        self._ai_callbacks = callbacks

    def _mips_hooks(self, model):
        hooks = {}
        for name, (address, argc) in AI_HOOKS.items():
            def callback(cpu, regs, hook_name=name, arg_count=argc):
                model.mem = z1.MipsMem(cpu)
                args = list(regs[4:8])
                for index in range(max(0, arg_count - 4)):
                    args.append(cpu.read32((regs[29] + 16 + 4 * index) & 0xFFFFFFFF))
                return model.call(hook_name, args[:arg_count])
            hooks[address] = callback

        for name, dispatch, address in z1.HOOK_NAMES:
            if address is None or dispatch:
                continue
            def callback(cpu, regs, hook_name=name):
                model.mem = z1.MipsMem(cpu)
                if hook_name == "Atan2Safe" and regs[31] == 0x800571FC:
                    return model.atan2_safe(tuple(regs[4:8]))
                return model.call(hook_name, tuple(regs[4:8]))
            hooks[address] = callback

        def solid_height(cpu, regs):
            model.mem = z1.MipsMem(cpu)
            return model.call("GetSolidHeight", tuple(regs[4:8]))
        hooks[0x8004D8C0] = solid_height
        return hooks

    def _setup(self, mem, name, case):
        area = case.get("area", 1)
        del area  # selected before setup; the RAM view is already backed by that member
        rng = random.Random(case.get("seed", 0x94423))
        for offset in range(OBJECT_SIZE):
            mem.wr8(OBJECT + offset, rng.getrandbits(8))
        for address in range(STACK_LO, STACK_HI, 4):
            mem.wr32(address, 0)

        # A valid cache bit avoids the world-height hook while still exercising
        # the already ported ObjGroundHeightCached call from state 6.
        flags = (0x00000100 |
                 (0x00000008 if case.get("initialized", True) else 0) |
                 case.get("flags", 0))
        mem.wr32(OBJECT + 0xE0, flags)
        mem.wr16(OBJECT + 0xE2, case.get("action_flags", 0))
        mem.wr8(OBJECT + 0x7E, case.get("type", 3))
        mem.wr8(OBJECT + 0x7F, case.get("anim_slot", 0))
        mem.wr8(OBJECT + 0x80, case.get("state", 0))
        mem.wr8(OBJECT + 0x81, 1)
        mem.wr8(OBJECT + 0x82, 0)
        mem.wr32(OBJECT + 0xB4, LINKED)
        mem.wr32(OBJECT + 0xB8, 0)
        mem.wr16(OBJECT + 0x1C, case.get("hit_x", -20))
        mem.wr16(OBJECT + 0x1E, case.get("hit_y", 35))
        mem.wr16(OBJECT + 0x20, case.get("hit_z", 90))
        mem.wr32(OBJECT + 0x24, case.get("x", 100))
        mem.wr32(OBJECT + 0x28, case.get("y", 100))
        mem.wr32(OBJECT + 0x2C, case.get("z", -100))
        mem.wr16(OBJECT + 0x44, 50)
        mem.wr16(OBJECT + 0x46, 100)
        mem.wr16(OBJECT + 0x48, 1)
        mem.wr16(OBJECT + 0x4A, 0xC103)
        mem.wr16(OBJECT + 0x60, case.get("facing_x", 2896))
        mem.wr16(OBJECT + 0x64, case.get("facing_z", 2896))
        mem.wr16(OBJECT + 0x5C, case.get("frame", 5))
        mem.wr32(OBJECT + 0x88, ANIM_TABLE)
        mem.wr16(OBJECT + 0x86, 250)
        mem.wr32(LINKED + 0x2C, (case.get("link_state", 0) & 0xF) << 28)
        mem.wr16(OBJECT + 0x94, 0)
        mem.wr16(OBJECT + 0x96, case.get("vertical_velocity", 0))
        mem.wr16(OBJECT + 0x98, 0)
        mem.wr16(OBJECT + 0x9A, case.get("target_x", 100))
        mem.wr16(OBJECT + 0x9C, case.get("home_y", 0))
        mem.wr16(OBJECT + 0x9E, case.get("target_z", -100))
        mem.wr16(OBJECT + 0xA8, case.get("speed", 0x2000))
        mem.wr16(OBJECT + 0xA6, case.get("radius", 256))
        mem.wr16(OBJECT + 0xB0, 91)
        mem.wr8(OBJECT + 0x8F, case.get("sub_state", 0))
        mem.wr8(OBJECT + 0xC9, case.get("cooldown_byte", 0))
        mem.wr8(OBJECT + 0xCC, 0xFF)
        mem.wr8(OBJECT + 0xDC, 99)
        mem.wr8(OBJECT + 0xE3, 0)
        for slot in range(16):
            mem.wr16(ANIM_TABLE + slot * 4, 0)
            mem.wr16(ANIM_TABLE + slot * 4 + 2, case.get("last_frame", 10))
        mem.wr8(0x800E77EC, case.get("accel_adjust", 0))
        mem.wr32(0x800E77D4, 0x80185100)
        mem.wr16(0x800F44B0, 0)
        player_x, player_y, player_z = case.get("player", (110, 100, 150))
        mem.wr32(PLAYER + 0x24, player_x)
        mem.wr32(PLAYER + 0x28, player_y)
        mem.wr32(PLAYER + 0x2C, player_z)
        z1.reset_collision_slots(mem)
        for index in range(128): mem.wr16(0x800BEDA0 + 2 * index, 0xFFFF)

        # Make the two direct ObjPlaySfx calls take the original unloaded-bank
        # return before the SPU boundary.
        mem.wr16(SOUND_BANK_IDS, 0xFFFF)
        for sfx in (0x45, 0x69):
            mem.wr16(SOUND_EFFECTS + 8 * sfx, 0)

        if name == "ZakoUpdateStageSpecificEnemy":
            mem.wr16(STAGE_STATE + 0x124, case.get("level", 0))
        return case.get("args", (OBJECT,))

    def run(self, name, area, case, *, selector=0, expected_failure=False):
        self.cpu.restore(self.bases[area])
        self.cpu.gte_reset()
        args = self._setup(z1.MipsMem(self.cpu), name, case)
        self.cpu.write32(MUTATION_SELECTOR, selector)
        before = self.cpu.snapshot()
        returns = case.get("returns", {})
        mm = ScenarioModel(z1.MipsMem(self.cpu), returns)
        cm = ScenarioModel(None, returns)
        self.c.sync(before)
        self.c.set_model(cm)
        self._bind_ai_hooks(cm)
        mips_hooks = self._mips_hooks(mm)

        addr, _size, kind = FUNCS[name]
        try:
            original = self.cpu.call(addr, args, hooks=mips_hooks)
        except Exception as error:  # noqa: BLE001
            return self.fail(name, case, f"MIPS execution failed: {error}", expected_failure)

        if name == "ZakoUpdateGenericEnemy":
            self.ai_fn(self.c.ram, C.byref(self.ai_hooks), *args)
        elif name == "ZakoUpdateStageSpecificEnemy":
            self.stage_fn(self.c.ram, C.byref(self.ai_hooks), *args)
        else:
            self.noop_fn(self.c.ram)
        self.counts[name] += 1

        problems = []
        if kind != "v" and (int(original[2]) & 0xFFFFFFFF) != 0:
            problems.append("unexpected MIPS return value")
        if self.c.dll.ae_ram_failed(self.c.ram):
            problems.append("C error flag: " + self.c.dll.ae_ram_error(self.c.ram).decode(errors="replace"))

        expected = bytearray(self.cpu.ram[:STACK_LO - 0x80000000])
        got = bytearray(self.c.main[:STACK_LO - 0x80000000])
        if got != expected:
            different = [i for i, (left, right) in enumerate(zip(expected, got)) if left != right]
            problems.append("RAM differs at " + ", ".join(
                f"{0x80000000 + i:08X}(MIPS {expected[i]:02X}, C {got[i]:02X})"
                for i in different[:10]))
        if bytes(self.cpu.scratch) != bytes(self.c.scratch):
            problems.append("scratchpad differs")
        m_events = [z1.hook_signature(event) for event in mm.events]
        c_events = [z1.hook_signature(event) for event in cm.events]
        if m_events != c_events:
            problems.append(f"hook order / arguments differ\n MIPS {m_events}\n C    {c_events}")
        if problems:
            if expected_failure:
                return True
            return self.fail(name, case, "\n".join(problems), expected_failure)
        if expected_failure:
            return False
        return True

    def fail(self, name, case, message, expected_failure=False):
        if not expected_failure:
            self.failures += 1
            print(f"FAIL {name} {case}: {message}", flush=True)
        return False


def generic_cases():
    cases = []
    for state in range(13):
        cases.append({"state": state, "seed": 0x94423 + state})
    cases.extend([
        {"state": 1, "initialized": False, "seed": 0xA001},
        {"state": 5, "seed": 0xA002},
        {"state": 6, "action_flags": 1, "cooldown_byte": 1,
         "accel_adjust": 7, "speed": 0xFFFF, "seed": 0xA003},
        {"state": 7, "seed": 0xA004},
        {"state": 10, "sub_state": 0, "seed": 0xA005},
        {"state": 10, "sub_state": 1, "seed": 0xA006},
        {"state": 10, "sub_state": 0, "y": 300, "seed": 0xA00F},
        {"state": 10, "sub_state": 1, "y": 300, "seed": 0xA011},
        {"state": 11, "link_state": 6, "seed": 0xA007},
        {"state": 11, "link_state": 0, "seed": 0xA008},
        {"state": 11, "link_state": 0, "action_flags": 1, "seed": 0xA009},
        {"state": 11, "link_state": 0, "y": 300, "flags": 0x400,
         "seed": 0xA010},
        {"state": 12, "action_flags": 1, "frame": 10,
         "seed": 0xA00A},
        {"state": 0, "type": 0x49, "y": 1000,
         "seed": 0xA00B},
        {"state": 11, "type": 0x49, "y": 1000, "seed": 0xA00C},
        {"state": 0x80, "type": 0x49, "y": 1000, "seed": 0xA00D},
        {"state": 6, "speed": 0xFFFF, "facing_x": -32768,
         "facing_z": 32767, "accel_adjust": 0x80, "cooldown_byte": 1,
         "seed": 0xA00E},
    ])
    return cases


def stage_cases():
    return [{"level": level, "seed": 0xB000 + level}
            for level in (0, 2, 3, 4, 9, 10)]


def run_default(args):
    harness = Harness(build())
    passed = True
    cases = generic_cases()
    for index, case in enumerate(cases):
        passed = harness.run("ZakoUpdateGenericEnemy", 1 + (index & 1), case) and passed
    for index, case in enumerate(stage_cases()):
        passed = harness.run("ZakoUpdateStageSpecificEnemy", 1 + (index & 1), case) and passed
    for area in (1, 2):
        passed = harness.run("ZakoAreaModeNoop", area, {"args": ()}) and passed

    summary = reg.coverage_summary(
        [(name, address, size, True) for name, (address, size, _kind) in FUNCS.items()],
        harness.cpu.coverage)
    if args.coverage:
        reg.print_coverage(summary)
    reg.write_results(harness.counts, summary, passed,
                      extra={"area_ids": [1, 2], "seed": 94423},
                      function_names=list(FUNCS))
    print("cases: " + ", ".join(f"{name}={count}" for name, count in harness.counts.items()), flush=True)
    print(f"differential: {sum(harness.counts.values())} cases; {'PASS' if passed else 'FAIL'}", flush=True)
    return 0 if passed else 1


def run_mutations():
    source_path = ROOT / "src/world/ae_zakoai.c"
    source = source_path.read_text(encoding="utf-8")
    names = list(FUNCS)
    mutations_by_function = {name: [] for name in names}
    for mutation_index in range(1, 31):
        name = names[(mutation_index - 1) % len(names)]
        mutations_by_function[name].append(mutation_index)

    mutated = source
    for function_index, name in enumerate(names, 1):
        pattern = re.compile(
            r"(void\s+ae_" + re.escape(name) + r"\s*\([^)]*\)\s*\{)", re.S)
        injection = "\n".join(
            f"    if (ae_rd32(r, 0x{MUTATION_SELECTOR:08X}u) == {mutation_index}u) "
            f"ae_wr32(r, 0x{MUTATION_MARKER:08X}u, 0x6B000000u | {mutation_index}u);"
            for mutation_index in mutations_by_function[name])
        mutated, count = pattern.subn(r"\1\n" + injection + "\n", mutated, count=1)
        if count != 1:
            raise SystemExit(f"could not insert mutations into {name}")

    temp_source = ROOT / "extract/mips" / f"zakoai_mutants_{time.time_ns()}.c"
    temp_source.parent.mkdir(parents=True, exist_ok=True)
    temp_source.write_text(mutated, encoding="utf-8")
    mutant_dll = build(temp_source, "zakoai_mutants")
    temp_source.unlink(missing_ok=True)

    cases = {
        "ZakoUpdateGenericEnemy": {"state": 1, "seed": 0xC001},
        "ZakoUpdateStageSpecificEnemy": {"level": 2, "seed": 0xC002},
        "ZakoAreaModeNoop": {"args": (), "seed": 0xC003},
    }
    caught = 0
    harness = Harness(mutant_dll)
    for mutation_index in range(1, 31):
        name = names[(mutation_index - 1) % len(names)]
        case = cases[name]
        area = 1 + (mutation_index & 1)
        if harness.run(name, area, case, selector=mutation_index,
                       expected_failure=True):
            caught += 1
        else:
            print(f"MUTATION SURVIVED {mutation_index}: {name}", flush=True)
    print(f"mutations caught: {caught}/30", flush=True)
    return 0 if caught == 30 else 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    args = parser.parse_args()
    return run_mutations() if args.mutate else run_default(args)


if __name__ == "__main__":
    sys.exit(main())
