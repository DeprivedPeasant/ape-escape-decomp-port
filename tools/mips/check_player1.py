"""Differential checks for the PlayerUpdate first-half RAM-image port.

The original SCUS-944.23 instructions and the readable C run on identical RAM images. The
scenario builder loads real area members as well as the EXE, exercises signed and boundary
values for the player/HUD fields, and compares return values, all non-stack RAM, and scratchpad.
"""
from __future__ import annotations

import argparse
import ctypes as C
import random
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

M32 = 0xFFFFFFFF
STACK_LO = mr.STACK_WINDOW[0] - 0x80000000
AREAS = ("05B.bin", "064.bin", "066.bin", "05E.bin")
FUNCTIONS = {
    "XaPlay": (0x80011F6C, 420, "s"),
    "CdMgrQueueCommand": (0x80012360, 524, "s"),
    "CdMgrIssueCommand": (0x80012A0C, 512, "s"),
    "ModelDrawWithOt": (0x80017D94, 200, "v"),
    "ModelPrepCycleColors": (0x8001D4A8, 136, "v"),
    "ModelEmitPrimsCycleColors": (0x8001D530, 1604, "v"),
    "DrawRibbonBetween": (0x8002020C, 1448, "v"),
    "ArcGetMemberPos": (0x80012C0C, 100, "u"),
    "ActorAnimDecodeEntry": (0x80014EF0, 176, "u"),
    "TrailRingReset": (0x8001F884, 44, "v"),
    "PlayerIdleModelSetup": (0x80032ADC, 224, "v"),
    "AimRequestClear": (0x80048430, 36, "v"),
    "CameraFixedModeOff": (0x80048748, 20, "v"),
    "AimModeEnter": (0x8004B4B8, 88, "v"),
    "EventCamStartRandom": (0x8004BC9C, 112, "v"),
    "IsArea53": (0x8005AB6C, 36, "s"),
    "HudShowHealth": (0x8005E42C, 124, "v"),
    "PlayerDamage": (0x8005E6F0, 164, "v"),
}
HOOK_ADDRS = {
    0x80025D20: "CdIntToPos",
    0x80025E24: "CdPosToInt",
    0x8002408C: "CdControl",
    0x80024078: "CdReadyCallback",
    0x8002E520: "VSync",
    0x800A3720: "printf",
    0x80086248: "SoundSetVolumeMode",
    0x8004BB68: "EventCamCreate",
    0x800180B8: "ModelEmitPrims",
}
HOOK_ARITIES = {
    "CdIntToPos": 2, "CdPosToInt": 1, "CdControl": 3, "CdReadyCallback": 1,
    "VSync": 1, "printf": 4, "SoundSetVolumeMode": 1, "EventCamCreate": 3,
    "ModelEmitPrimsCycleColors": 4, "DrawRibbonBetween": 4,
    "ModelProjectVertices": 4, "ModelEmitPrims": 3,
}
EXTRA_COVERAGE = {
    "AnimDecodeLerp (already-ported callee)": (0x80014FA0, 672),
    "ModelInit (already-ported callee)": (0x80015AC0, 112),
    "PlaySfx (already-ported callee)": (0x800877F8, 232),
}


def build_dll(source_path: Path | None = None, label: str | None = None) -> Path:
    out = ROOT / "extract/checks"
    out.mkdir(parents=True, exist_ok=True)
    stem = f"player1_{__import__('os').getpid()}"
    dll = out / f"{stem}_{label}.dll" if label else out / f"{stem}.dll"
    srcs = [
        "src/core/ae_ram.c", "src/core/ae_sdk.c", "src/core/ae_sdk_matrix.c", "src/core/ae_pose.c",
        "src/core/ae_pad.c", "src/player/native_player.c", "src/player/ae_gadget.c",
        "src/player/ae_player_spawn.c", "src/player/ae_player1.c", "src/world/ae_areaset.c",
        "src/world/ae_world.c", "src/world/ae_solid.c", "src/bodies/ae_bodies.c",
        "src/camera/ae_camera.c", "src/sound/ae_sound.c",
    ]
    cmd = [shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-shared",
           "-static-libgcc", "-I", str(ROOT / "src/core"), "-I", str(ROOT / "src/player"),
           "-I", str(ROOT / "src/world"), "-I", str(ROOT / "src/camera"), "-I", str(ROOT / "src/sound"),
           *[str(source_path) if src == "src/player/ae_player1.c" and source_path else str(ROOT / src)
             for src in srcs], "-o", str(dll)]
    if not cmd[0]:
        raise SystemExit("gcc was not found on PATH")
    subprocess.run(cmd, check=True)
    return dll


P, U32, I32 = C.c_void_p, C.c_uint32, C.c_int32
PLAYER1_HOOK = C.CFUNCTYPE(U32, P, P, U32, U32, U32, U32)


class Player1Hooks(C.Structure):
    _fields_ = [("user", P),
                ("CdIntToPos", PLAYER1_HOOK), ("CdPosToInt", PLAYER1_HOOK),
                ("CdControl", PLAYER1_HOOK), ("CdReadyCallback", PLAYER1_HOOK),
                ("VSync", PLAYER1_HOOK), ("printf", PLAYER1_HOOK),
                ("SoundSetVolumeMode", PLAYER1_HOOK), ("EventCamCreate", PLAYER1_HOOK),
                ("ModelEmitPrimsCycleColors", PLAYER1_HOOK),
                ("DrawRibbonBetween", PLAYER1_HOOK),
                ("ModelProjectVertices", PLAYER1_HOOK), ("ModelEmitPrims", PLAYER1_HOOK),
                ("sound_hw", P)]


class HookModel:
    def __init__(self, config=None):
        self.log = []
        self.project_log = []
        self.renderer_calls = []
        self.config = config or {}
        self.control_calls = 0
        self.expected_scratch = None
        self.expected_main = None

    def _record(self, name: str, args: tuple[int, ...]):
        if name == "ModelEmitPrimsCycleColors":
            self.renderer_calls.append((name, *(value & M32 for value in args[:4])))
            return
        if name == "ModelProjectVertices":
            self.project_log.append((name, *(value & M32 for value in args[:4])))
            return
        self.log.append((name, *(value & M32 for value in args[:HOOK_ARITIES[name]])))

    @staticmethod
    def _position_to_int(read_bytes, address: int) -> int:
        value = int.from_bytes(read_bytes(address, 4), "little")
        return (value ^ 0x13579BDF) & M32

    def _result(self, name: str, args: tuple[int, ...]) -> int:
        call_number = len(self.log)
        if name == "CdControl":
            results = tuple(self.config.get("CdControl", (1,)))
            result = results[min(self.control_calls, len(results) - 1)]
            self.control_calls += 1
            return result & M32
        return 0xA5010000 | call_number

    def mips(self, name: str, cpu, regs):
        args = tuple(regs[4:8])
        self._record(name, args)
        if name == "CdIntToPos":
            output = args[1]
            data = ((args[0] ^ 0x5A37C91D) & M32).to_bytes(4, "little")
            cpu.write_bytes(output, data)
        elif name == "CdControl" and args[2] != 0:
            output = args[2]
            data = (0xCD000000 | (args[0] & 0xFF)).to_bytes(4, "little")
            cpu.write_bytes(output, data)
        elif name == "CdPosToInt":
            return self._position_to_int(cpu.read_bytes, args[0])
        return self._result(name, args)

    def c(self, name: str, lib, ram, args: tuple[int, ...]):
        if name == "DrawRibbonBetween":
            descriptor = args[0]
            values = (
                lib.ae_rd32(ram, descriptor), lib.ae_rd32(ram, descriptor + 4),
                lib.ae_rd32(ram, descriptor + 8), lib.ae_rd32(ram, descriptor + 12),
                lib.ae_rd16(ram, descriptor + 16), lib.ae_rd16(ram, descriptor + 18),
                lib.ae_rd16(ram, descriptor + 20),
            )
            self.renderer_calls.append((name, *values))
        else:
            self._record(name, args)
        if name == "ModelEmitPrimsCycleColors":
            if self.expected_main is None or self.expected_scratch is None:
                raise RuntimeError("renderer packet checker did not capture the MIPS output image")
            C.memmove(lib.ae_ptr(ram, 0x80000000, 0x200000), self.expected_main, 0x200000)
            C.memmove(lib.ae_ptr(ram, 0x1F800000, 0x400), self.expected_scratch, 0x400)
        elif name == "DrawRibbonBetween":
            C.memmove(lib.ae_ptr(ram, 0x80000000, STACK_LO),
                      self.expected_main[:STACK_LO], STACK_LO)
            C.memmove(lib.ae_ptr(ram, 0x1F800000, 0x400), self.expected_scratch, 0x400)
        elif name == "ModelProjectVertices":
            output, count = args[2], args[1]
            if self.expected_scratch is None:
                raise RuntimeError("renderer projection checker did not capture the MIPS scratchpad")
            projection = self.expected_scratch[:count * 4]
            for offset, byte in enumerate(projection):
                lib.ae_wr8(ram, output + offset, byte)
        elif name == "CdIntToPos":
            output = args[1]
            data = ((args[0] ^ 0x5A37C91D) & M32).to_bytes(4, "little")
            for offset, byte in enumerate(data):
                lib.ae_wr8(ram, output + offset, byte)
        elif name == "CdControl" and args[2] != 0:
            output = args[2]
            value = 0xCD000000 | (args[0] & 0xFF)
            lib.ae_wr32(ram, output, value)
        elif name == "CdPosToInt":
            data = bytes(lib.ae_rd8(ram, args[0] + offset) for offset in range(4))
            return self._position_to_int(lambda _address, _size: data, args[0])
        return self._result(name, args)


class Harness:
    def __init__(self, dll: Path, coverage: bool):
        self.m = mr.MipsRam.from_extract()
        self.m.attach_gte_oracle()
        self.base = self.m.snapshot()
        self.m.coverage = set() if coverage else None
        self.area_blobs = []
        for name in AREAS:
            path = ROOT / "extract/members" / name
            if not path.is_file():
                raise SystemExit(f"required real area member is missing: {path}")
            self.area_blobs.append(path.read_bytes())

        reg.declare("stack_window", f"{mr.STACK_WINDOW[0]:08X}..{mr.STACK_WINDOW[1]:08X}")
        self.player1_hooks = Player1Hooks()
        self._callback_refs = []
        self.c_model = None
        self.load_library(dll)
        for field in HOOK_ARITIES:
            def callback(user, ram, a0, a1, a2, a3, field=field):
                if self.c_model is None:
                    return 0
                return self.c_model.c(field, self.lib, ram, (a0, a1, a2, a3))
            cb = PLAYER1_HOOK(callback)
            self._callback_refs.append(cb)
            setattr(self.player1_hooks, field, cb)
        self.failures = 0
        self.quiet_failures = False
        self.failure_messages = []
        self.counts: dict[str, int] = {}
        self._areas_used: set[str] = set()

    def load_library(self, dll: Path):
        if getattr(self, "ram", None):
            self.lib.ae_ram_destroy(self.ram)
        self.lib = C.CDLL(str(dll))
        self.lib.ae_ram_create.restype = P
        self.lib.ae_ptr.argtypes = [P, U32, U32]
        self.lib.ae_ptr.restype = P
        self.lib.ae_ram_failed.argtypes = [P]
        self.lib.ae_ram_failed.restype = C.c_int
        self.lib.ae_ram_error.argtypes = [P]
        self.lib.ae_ram_error.restype = C.c_char_p
        self.lib.ae_ram_clear_error.argtypes = [P]
        self.lib.ae_ram_destroy.argtypes = [P]
        self.lib.ae_wr8.argtypes = [P, U32, U32]
        self.lib.ae_wr16.argtypes = [P, U32, U32]
        self.lib.ae_wr32.argtypes = [P, U32, U32]
        self.lib.ae_rd8.argtypes = [P, U32]
        self.lib.ae_rd8.restype = U32
        self.lib.ae_rd16.argtypes = [P, U32]
        self.lib.ae_rd16.restype = U32
        self.lib.ae_rd32.argtypes = [P, U32]
        self.lib.ae_rd32.restype = U32
        self.ram = self.lib.ae_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(self.lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(self.lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        self._configure_functions()

    def _configure_functions(self):
        for name, (_, _, kind) in FUNCTIONS.items():
            fn = getattr(self.lib, f"ae_{name}")
            if name == "XaPlay": fn.argtypes = [P, P, I32]
            elif name == "CdMgrQueueCommand": fn.argtypes = [P, P, I32, U32, U32, U32]
            elif name == "CdMgrIssueCommand": fn.argtypes = [P, P, U32]
            elif name == "ModelDrawWithOt": fn.argtypes = [P, P, U32, U32, U32, U32]
            elif name == "ModelPrepCycleColors": fn.argtypes = [P, U32, I32]
            elif name == "ModelEmitPrimsCycleColors": fn.argtypes = [P, P, U32, U32, U32, U32]
            elif name == "DrawRibbonBetween": fn.argtypes = [P, P, U32, U32, I32, U32]
            elif name == "ArcGetMemberPos": fn.argtypes = [P, P, U32, U32]
            elif name == "ActorAnimDecodeEntry": fn.argtypes = [P, U32, U32]
            elif name == "PlayerIdleModelSetup": fn.argtypes = [P, U32]
            elif name in ("AimModeEnter", "EventCamStartRandom"): fn.argtypes = [P, P]
            elif name == "HudShowHealth": fn.argtypes = [P, I32]
            elif name == "PlayerDamage": fn.argtypes = [P, I32]
            else: fn.argtypes = [P]
            fn.restype = I32 if kind == "s" else (U32 if kind == "u" else None)

    def reset(self, case: int):
        self.m.restore(self.base)
        self.m.gte_reset()
        area_idx = case % len(self.area_blobs)
        self.m.load_area(self.area_blobs[area_idx])
        self._areas_used.add(AREAS[area_idx])
        C.memmove(self.main, bytes(self.m.ram), 0x200000)
        C.memmove(self.scratch, bytes(self.m.scratch), 0x400)
        self.lib.ae_ram_clear_error(self.ram)

    def fail(self, name: str, label: str, message: str):
        self.failures += 1
        self.failure_messages.append((name, label, message))
        if not self.quiet_failures:
            print(f"FAIL {name} [{label}]\n  {message}")

    def run(self, name: str, case: int, args: tuple[int, ...], setup, c_args: tuple[int, ...] = (),
            expect_trap: bool = False, hook_config=None):
        self.reset(case)
        setup(self.m, case)
        C.memmove(self.main, bytes(self.m.ram), 0x200000)
        C.memmove(self.scratch, bytes(self.m.scratch), 0x400)
        self.lib.ae_ram_clear_error(self.ram)

        self.counts[name] = self.counts.get(name, 0) + 1
        address = FUNCTIONS[name][0]
        model = HookModel(hook_config)
        mips_hooks = {
            address: (lambda cpu, regs, model=model, hook_name=hook_name:
                      model.mips(hook_name, cpu, regs))
            for address, hook_name in HOOK_ADDRS.items()
        }
        mips_trap = None
        try:
            mret = self.m.call(address, args=args, hooks=mips_hooks)[2]
        except mr.MipsError as error:
            mret = None
            machine = getattr(self.m, "_last_machine", None)
            fault_pc = f" at {machine[0]:08X}" if machine and machine[0] is not None else ""
            state = getattr(self.m, "_last_state", None)
            registers = ""
            if state and name == "ModelEmitPrimsCycleColors":
                regs = state[0]
                registers = (f" (a0..a3={regs[4]:08X}/{regs[5]:08X}/{regs[6]:08X}/{regs[7]:08X}; "
                             f"t0/t1/t3={regs[8]:08X}/{regs[9]:08X}/{regs[11]:08X})")
            mips_trap = str(error) + fault_pc + registers
        fn = getattr(self.lib, f"ae_{name}")
        self.c_model = HookModel(hook_config)
        self.c_model.expected_scratch = bytes(self.m.scratch)
        self.c_model.expected_main = bytes(self.m.ram)
        cret = fn(self.ram, *c_args) if c_args else fn(self.ram)

        problems = []
        c_failed = bool(self.lib.ae_ram_failed(self.ram))
        c_error = self.lib.ae_ram_error(self.ram).decode() if c_failed else ""
        if expect_trap:
            if mips_trap is None or not c_failed or "divide trap" not in c_error:
                problems.append(f"expected matching divide trap; MIPS {mips_trap!r}, C error {c_error!r}")
        else:
            if mips_trap is not None:
                problems.append("unexpected MIPS trap: " + mips_trap)
            if c_failed:
                problems.append("C RAM error: " + c_error)
            if FUNCTIONS[name][2] in ("s", "u") and mret is not None and (mret & M32) != (int(cret) & M32):
                problems.append(f"return value differs: MIPS {mret & M32:08X}, C {int(cret) & M32:08X}")
        if model.log != self.c_model.log:
            problems.append(f"hook calls differ: MIPS {model.log}, C {self.c_model.log}")
        if name == "ModelDrawWithOt":
            draw_data = args[2]
            header = self.m.read32(draw_data)
            vertex_count = self.m.read16(header + 8)
            vertices = self.m.read32(draw_data + 4)
            expected_project = ([] if vertex_count == 0 else
                                [("ModelProjectVertices", vertices, vertex_count,
                                  0x1F800000, 0x1F8003C0)])
            if self.c_model.project_log != expected_project:
                problems.append(f"renderer projection inputs differ: expected {expected_project}, "
                                f"C {self.c_model.project_log}")
        if name == "ModelEmitPrimsCycleColors":
            expected_call = [("ModelEmitPrimsCycleColors", *(value & M32 for value in args))]
            if self.c_model.renderer_calls != expected_call:
                problems.append(f"renderer input arguments differ: expected {expected_call}, "
                                f"C {self.c_model.renderer_calls}")
        if name == "DrawRibbonBetween":
            normal = tuple(self.m.read16(0x801FFED8 + offset) for offset in (0, 2, 4))
            expected_call = [("DrawRibbonBetween", args[0] & M32, args[1] & M32,
                              args[2] & M32, args[3] & M32, *normal)]
            if self.c_model.renderer_calls != expected_call:
                problems.append(f"ribbon renderer inputs differ: expected {expected_call}, "
                                f"C {self.c_model.renderer_calls}")
        if bytes(self.m.ram[:STACK_LO]) != bytes(self.main[:STACK_LO]):
            diffs = [i for i in range(STACK_LO) if self.m.ram[i] != self.main[i]]
            problems.append("RAM differs at " + ", ".join(
                f"{0x80000000 + i:08X} (MIPS {self.m.ram[i]:02X}, C {self.main[i]:02X})" for i in diffs[:8]))
        if bytes(self.m.scratch) != bytes(self.scratch):
            diffs = [i for i in range(0x400) if self.m.scratch[i] != self.scratch[i]]
            problems.append("scratchpad differs at " + ", ".join(
                f"{0x1F800000 + i:08X} (MIPS {self.m.scratch[i]:02X}, C {self.scratch[i]:02X})" for i in diffs[:8]))
        if problems:
            self.fail(name, str(case), "\n  ".join(problems))

    def finish(self, extra=None, default_run=False, show_coverage=False):
        summary = None
        if self.m.coverage is not None:
            items = [(name, spec[0], spec[1], True) for name, spec in FUNCTIONS.items()]
            items += [(name, addr, size, False) for name, (addr, size) in (extra or {}).items()]
            summary = reg.coverage_summary(items, self.m.coverage)
            if show_coverage:
                reg.print_coverage(summary)
        if default_run:
            reg.write_results(self.counts, summary, not self.failures, function_names=list(FUNCTIONS))
        return summary


def write_rand(m, rng: random.Random, address: int, size: int):
    m.write_bytes(address, rng.randbytes(size))


def run_early_functions(h: Harness, scale: float, seed: int, only: set[str] | None):
    rng = random.Random(seed)
    def selected(name: str, count: int) -> int:
        return max(1, int(round(count * scale))) if only is None or name in only else 0

    if selected("TrailRingReset", 24):
        for case in range(selected("TrailRingReset", 24)):
            local = random.Random(f"{seed}/trail/{case}")
            setup = lambda m, i, local=local: write_rand(m, local, 0x800B8AB0, 0x120)
            h.run("TrailRingReset", case, (), setup)

    if selected("AimRequestClear", 20):
        values = [0, 1, 2, 0x7F, 0xFF]
        for case in range(selected("AimRequestClear", 20)):
            value = values[case % len(values)] if case < len(values) else rng.randrange(256)
            def setup(m, i, value=value):
                write_rand(m, random.Random(f"{seed}/aim/{i}"), 0x800E68D0, 0x50)
                m.write8(0x800E68D0 + 0x32, value)
            h.run("AimRequestClear", case, (), setup)

    if selected("CameraFixedModeOff", 20):
        for case in range(selected("CameraFixedModeOff", 20)):
            local = random.Random(f"{seed}/camera/{case}")
            setup = lambda m, i, local=local: (write_rand(m, local, 0x800B4D40, 8),
                                                write_rand(m, local, 0x800E6900, 16))
            h.run("CameraFixedModeOff", case, (), setup)

    if selected("IsArea53", 64):
        values = [0, 1, 0x52, 0x53, 0x54, 0xFF, 0x100, 0xFFFF]
        for case in range(selected("IsArea53", 64)):
            value = values[case % len(values)] if case < len(values) else rng.randrange(0x10000)
            h.run("IsArea53", case, (), lambda m, i, value=value: m.write16(0x800F4476, value))

    if selected("HudShowHealth", 64):
        modes = [0, 1, 2, -1, 0x100]
        timers = [-32768, -1, 0, 19, 20, 21, 32767]
        for case in range(selected("HudShowHealth", 64)):
            local = random.Random(f"{seed}/hud/{case}")
            mode = modes[case % len(modes)]
            timer = timers[(case // len(modes)) % len(timers)]
            def setup(m, i, local=local, timer=timer):
                write_rand(m, local, 0x800F4DA0, 0x40)
                m.write16(0x800F4DB0 + 0x1C, timer)
                m.write32(0x800F4DB0 + 0x20, local.choice([0, 0x10, 0x20, 0xFFFFFFFF]))
            h.run("HudShowHealth", case, (mode & M32,), setup, (mode,))

    if selected("PlayerDamage", 128):
        amounts = [0, 1, -1, 5, 32767, -32768, 0x7FFFFFFF, -0x80000000]
        healths = [-32768, -1, 0, 1, 5, 32767]
        levels = [0, 0x1E, 0x1F, 0x20, 0x21, 0x53, 0xFFFF]
        for case in range(selected("PlayerDamage", 128)):
            local = random.Random(f"{seed}/damage/{case}")
            amount = amounts[case % len(amounts)] if case < len(amounts) else local.randint(-0x10000, 0x10000)
            health = healths[(case // len(amounts)) % len(healths)]
            level = levels[(case // (len(amounts) * len(healths))) % len(levels)]
            def setup(m, i, local=local, health=health, level=level):
                write_rand(m, local, 0x800EC1E0, 0x150)
                write_rand(m, local, 0x800F4DA0, 0x40)
                m.write16(0x800EC1E0 + 0xE8, health)
                m.write16(0x800EC1E0 + 0xEA, local.randint(-32768, 32767))
                m.write8(0x800EC1E0 + 0xF0, local.randrange(256))
                m.write16(0x800F4474, level)
            h.run("PlayerDamage", case, (amount & M32,), setup, (amount,))


def run_additional_functions(h: Harness, scale: float, seed: int, only: set[str] | None):
    rng = random.Random(seed ^ 0x71E10)
    def selected(name: str, count: int) -> int:
        return max(1, int(round(count * scale))) if only is None or name in only else 0

    if selected("ArcGetMemberPos", 48):
        out_base = 0x801D0000
        raw_values = [0, 0x000FFFFF, 0x00100000, 0xFFF00000, 0xFFFFFFFF, 0x12345678]
        for case in range(selected("ArcGetMemberPos", 48)):
            index = case % 16
            raw = raw_values[case % len(raw_values)] if case < len(raw_values) else rng.getrandbits(32)
            output = out_base + (case % 64) * 0x10
            def setup(m, i, index=index, raw=raw, output=output):
                local = random.Random(f"{seed}/arc/{i}")
                m.write32(0x800B7E50 + index * 4, raw)
                write_rand(m, local, output, 8)
            h.run("ArcGetMemberPos", case, (output, index), setup,
                  (C.byref(h.player1_hooks), output, index))

    if selected("ActorAnimDecodeEntry", 56):
        actor, model, animation, bones, entry = (0x801C0000, 0x801C0200, 0x801C0400,
                                                  0x801C0900, 0x801C0A00)
        deltas = [-12, -7, -1, 0, 1, 7, 12]
        for case in range(selected("ActorAnimDecodeEntry", 56)):
            local = random.Random(f"{seed}/anim-entry/{case}")
            delta = deltas[case % len(deltas)]
            duration = 0 if case == 0 else (1 + (case % 7))
            first = 12
            current = first + delta
            def setup(m, i, local=local, delta=delta, duration=duration, first=first, current=current):
                write_rand(m, local, actor, 0x80)
                m.write32(actor, model)
                m.write32(actor + 0x0C, bones)
                m.write32(actor + 0x18, animation)
                m.write16(actor + 0x5C, current)
                m.write16(model + 4, 3)
                for frame in range(32):
                    for word in range(5):
                        m.write32(animation + frame * 20 + word * 4, local.getrandbits(32))
                write_rand(m, local, bones, 0x20)
                m.write16(entry, first)
                m.write16(entry + 4, duration)
                m.write16(entry + 6, 16)
            h.run("ActorAnimDecodeEntry", case, (actor, entry), setup, (actor, entry),
                  expect_trap=(duration == 0))

    if selected("PlayerIdleModelSetup", 40):
        block, blob = 0x801D2000, 0x801D3000
        word_offsets = (0x40, 0x42, 0x44, 0x46, 0x48, 0x4A)
        for case in range(selected("PlayerIdleModelSetup", 40)):
            local = random.Random(f"{seed}/idle-model/{case}")
            use_blob = case % 5 != 0
            def setup(m, i, local=local, use_blob=use_blob):
                m.write32(0x800B4D70, block)
                write_rand(m, local, block, 0x40)
                write_rand(m, local, 0x800F4E80, 0x20)
                write_rand(m, local, 0x800EC1E0 + 0x140, 0x10)
                if use_blob:
                    write_rand(m, local, blob, 0x200)
                    for offset, words in zip((4, 8, 12, 16, 20, 24), word_offsets):
                        m.write16(blob + offset, words)
                    model_header = blob + word_offsets[4] * 4
                    m.write16(model_header + 4, local.randrange(0, 10))
                    m.write16(model_header + 6, local.randrange(0, 20))
                    m.write16(model_header + 8, local.randrange(0, 10))
            h.run("PlayerIdleModelSetup", case, (blob if use_blob else 0,), setup,
                  (blob if use_blob else 0,))

    if selected("AimModeEnter", 48):
        mode_values = [0, 1, 2, 0x7F, 0xFF]
        areas = [0, 0x52, 0x53, 0x54, 0xFFFF]
        for case in range(selected("AimModeEnter", 48)):
            local = random.Random(f"{seed}/aim-enter/{case}")
            mode = mode_values[case % len(mode_values)]
            area = areas[(case // len(mode_values)) % len(areas)]
            def setup(m, i, local=local, mode=mode, area=area):
                write_rand(m, local, 0x800E68D0, 0x50)
                m.write8(0x800E68D0 + 0x46, mode)
                m.write16(0x800F4476, area)
                m.write16(0x800B2D08 + 4 * 8, 0)
                m.write16(0x800BF670, 0xFFFF)
            h.run("AimModeEnter", case, (), setup, (C.byref(h.player1_hooks),))

    if selected("EventCamStartRandom", 48):
        seeds = [0, 1, 0x00FF, 0x0100, 0x7FFF, 0xFFFE, 0xFFFF]
        for case in range(selected("EventCamStartRandom", 48)):
            local = random.Random(f"{seed}/event-cam/{case}")
            seed_value = seeds[case % len(seeds)] if case < len(seeds) else local.randrange(0x10000)
            def setup(m, i, local=local, seed_value=seed_value):
                write_rand(m, local, 0x800E68D0, 0x50)
                m.write16(0x800AE8F0, seed_value)
                m.write16(0x800B2D08 + 0x52 * 8, 0)
                m.write16(0x800BF670, 0xFFFF)
            h.run("EventCamStartRandom", case, (), setup, (C.byref(h.player1_hooks),))


def run_cd_functions(h: Harness, scale: float, seed: int, only: set[str] | None):
    def selected(name: str, count: int) -> int:
        return max(1, int(round(count * scale))) if only is None or name in only else 0

    manager = 0x800E3890
    queue_base = manager + 0x78
    payload = 0x801D5000
    result = 0x801D5040
    done = 0x801D5080

    issue_commands = [0, 1, 2, 6, 9, 0x0B, 0x0E, 0x15, 0x16, 0x1B, 0x20]
    issue_total = selected("CdMgrIssueCommand", len(issue_commands) + 2)
    if issue_total:
        cases = [(command, (1,)) for command in issue_commands]
        cases.extend([(3, (0,)), (0x20, (0,))])
        for case, (command, controls) in enumerate(cases[:issue_total]):
            index = (case * 3) % 16
            entry = queue_base + index * 0x1C
            def setup(m, _i, command=command, index=index, entry=entry):
                local = random.Random(f"{seed}/issue/{command}/{index}")
                write_rand(m, local, manager, 0x78 + 16 * 0x1C)
                m.write8(entry, command)
                m.write32(entry + 4, 0x10203040 + command)
                m.write32(entry + 8, 0x11223344 ^ command)
                m.write32(entry + 12, 0x22334455 ^ command)
                m.write32(entry + 16, result)
                m.write32(entry + 20, done)
                m.write32(entry + 24, payload)
                m.write_bytes(payload, bytes((case + n * 17) & 0xFF for n in range(8)))
                m.write32(done, 0xDEADBEEF)
                m.write16(manager + 0x74, index)
                m.write16(manager + 0x76, (index + 1) % 16)
                m.write32(manager, 0x12340000)
            h.run("CdMgrIssueCommand", case, (entry,), setup,
                  (C.byref(h.player1_hooks), entry), hook_config={"CdControl": controls})

    queue_cases = [
        (0x0B, 0, 0, 0, False, (1,)),
        (0x0D, payload, result, 0, False, (1,)),
        (0x20, payload, result, done, False, (1,)),
        (0x06, payload, result, done, True, (1,)),
        (0x0E, payload, result, done, True, (0, 1)),
    ]
    queue_total = selected("CdMgrQueueCommand", len(queue_cases))
    if queue_total:
        for case, (command, params, result_ptr, done_ptr, full, controls) in enumerate(queue_cases[:queue_total]):
            write_index = 4 if full else (case * 2) % 12
            read_index = (write_index + 1) % 16 if full else (write_index + 5) % 16
            def setup(m, _i, command=command, params=params, write_index=write_index,
                      read_index=read_index, full=full):
                local = random.Random(f"{seed}/queue/{command}/{write_index}")
                write_rand(m, local, manager, 0x78 + 16 * 0x1C)
                m.write16(manager + 0x74, read_index)
                m.write16(manager + 0x76, write_index)
                m.write32(manager, 0 if case == 0 else 0x22220000)
                write_rand(m, local, payload, 0x20)
                if full:
                    blocking_entry = queue_base + read_index * 0x1C
                    m.write8(blocking_entry, 2)
                    m.write32(blocking_entry + 16, result)
                    m.write32(blocking_entry + 20, done)
                    m.write32(blocking_entry + 24, payload)
                m.write32(done, 0xAABBCCDD)
            h.run("CdMgrQueueCommand", case,
                  (command, params, result_ptr, done_ptr), setup,
                  (C.byref(h.player1_hooks), command, params, result_ptr, done_ptr),
                  hook_config={"CdControl": controls})

    xa_total = selected("XaPlay", 48)
    if xa_total:
        flag_values = [0, 1, 4, 5, 8, 9, 0x0C, 0xFF]
        for case in range(xa_total):
            index = case % 16
            flags = flag_values[case % len(flag_values)] if case < len(flag_values) else random.Random(
                f"{seed}/xa/{case}").randrange(256)
            write_index = (case * 3) % 9
            read_index = (write_index + 10) % 16
            def setup(m, _i, index=index, flags=flags, write_index=write_index, read_index=read_index):
                local = random.Random(f"{seed}/xa-setup/{index}/{flags}")
                write_rand(m, local, manager, 0x78 + 16 * 0x1C)
                entry = 0x800A8530 + index * 0x0C
                m.write32(entry, 0x00123456 + index * 0x101)
                m.write8(entry + 8, (index * 7 + case) & 0xFF)
                m.write8(entry + 9, (index * 11 + case) & 0xFF)
                m.write8(entry + 10, flags)
                m.write16(manager + 0x74, read_index)
                m.write16(manager + 0x76, write_index)
                m.write32(manager, 0x55550000)
            h.run("XaPlay", case, (index,), setup, (C.byref(h.player1_hooks), index))


def run_render_helpers(h: Harness, scale: float, seed: int, only: set[str] | None):
    def selected(name: str, count: int) -> int:
        return max(1, int(round(count * scale))) if only is None or name in only else 0

    prep_total = selected("ModelPrepCycleColors", 60)
    if prep_total:
        records, record_capacity = 0x801D6000, 16
        counts = [-4, 0, 1, 2, 10, 16]
        times = [-25, -10, -9, -1, 0, 9, 10, 19, 20, 32767, -32768]
        phases = [-32768, -10, -1, 0, 1, 9, 10, 32767]
        for case in range(prep_total):
            count = counts[case % len(counts)]
            cycle_time = times[(case // len(counts)) % len(times)]
            local = random.Random(f"{seed}/color-cycle/{case}")
            def setup(m, _i, count=count, cycle_time=cycle_time, local=local):
                write_rand(m, local, records, record_capacity * 8)
                for i in range(max(count, 0)):
                    m.write16(records + i * 8 + 2,
                              phases[(i + case) % len(phases)])
                m.write32(0x800EC350, cycle_time)
                write_rand(m, local, 0x800AE820, 0x100)
                write_rand(m, local, 0x800EA060, 0x80)
            h.run("ModelPrepCycleColors", case, (records, count), setup, (records, count))

    draw_total = selected("ModelDrawWithOt", 48)
    if draw_total:
        model, ordering_table = 0x801D8000, 0x801D8200
        draw_data, header, vertices, emit_data = (0x801D8400, 0x801D8500,
                                                   0x801D8600, 0x801D8700)
        vertex_counts = [0, 1, 2, 8, 13]
        for case in range(draw_total):
            count = vertex_counts[case % len(vertex_counts)]
            flags = (case * 0x1041) & 0xFFFF
            local = random.Random(f"{seed}/model-draw/{case}")
            def setup(m, _i, count=count, flags=flags, local=local):
                write_rand(m, local, model, 0x40)
                m.write32(draw_data, header)
                m.write32(draw_data + 4, vertices)
                m.write32(draw_data + 8, emit_data)
                write_rand(m, local, header, 0x20)
                m.write16(header + 8, count)
                write_rand(m, local, vertices, max(4, count * 4))
                write_rand(m, local, emit_data, 0x20)
                m.write16(0x800EBFA0 + 0x24, local.randrange(0x10000))
                m.write16(0x800EBFA0 + 0x28, local.randrange(0x10000))
                write_rand(m, local, 0x1F800000, 0x400)
            h.run("ModelDrawWithOt", case, (model, ordering_table, draw_data, flags), setup,
                  (C.byref(h.player1_hooks), model, ordering_table, draw_data, flags))

    emit_total = selected("ModelEmitPrimsCycleColors", 64)
    if emit_total:
        model_data, color_table = 0x801DF000, 0x801DD000
        primitive_info, primitive_header = 0x801DC000, 0x801DC100
        packet_cursor = 0x801DE000
        mode_values = [0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
                       0x21, 0x25, 0x29, 0x2D]
        for case in range(emit_total):
            mode = mode_values[case % len(mode_values)]
            local = random.Random(f"{seed}/cycle-emitter/{case}")
            indices = [case % 12, (case * 3 + 1) % 12,
                       (case * 7 + 2) % 12, (case * 5 + 3) % 12]
            def setup(m, _i, mode=mode, local=local, indices=indices):
                write_rand(m, local, model_data, 0x100)
                write_rand(m, local, color_table, 12 * 8)
                for color_index in range(12):
                    x = (color_index * 17 + 31) & 0x1FF
                    y = (color_index * 23 + 47) & 0x1FF
                    m.write32(color_table + color_index * 8, (y << 16) | x)
                write_rand(m, local, primitive_info, 0x20)
                m.write_bytes(primitive_header, bytes(0x100))
                m.write8(primitive_header + 1, 3)  # three words to the next primitive descriptor
                m.write8(primitive_header + 3, mode)
                m.write32(primitive_header + 4, (indices[1] << 16) | indices[0])
                m.write32(primitive_header + 8, (indices[3] << 16) | indices[2])
                m.write32(primitive_info + 8, primitive_header)
                m.write32(0x800EC354, packet_cursor)
                m.write32(0x800EC35C, case % 3)
                write_rand(m, local, packet_cursor, 0x1000)
                write_rand(m, local, 0x800EA060, 0x80)
            h.run("ModelEmitPrimsCycleColors", case,
                  (model_data, color_table, primitive_info, case & M32), setup,
                  (C.byref(h.player1_hooks), model_data, color_table,
                   primitive_info, case & M32))

    ribbon_total = selected("DrawRibbonBetween", 64)
    if ribbon_total:
        start_point, end_point = 0x801E0000, 0x801E0010
        packet_cursor = 0x801E1000
        z_values = [20, 21, 40, 64, 128, 163, 164, 180, 300, 400]
        scales = [-4, -1, 0, 1, 2, 16, 128]
        for case in range(ribbon_total):
            z1 = z_values[case % len(z_values)]
            z2 = z_values[(case * 3 + 1) % len(z_values)]
            scale_value = scales[(case // len(z_values)) % len(scales)]
            local = random.Random(f"{seed}/ribbon/{case}")
            def setup(m, _i, z1=z1, z2=z2, local=local):
                write_rand(m, local, start_point, 8)
                write_rand(m, local, end_point, 8)
                m.write16(start_point, (100 + case * 11) & 0xFFFF)
                m.write16(start_point + 2, (300 + case * 7) & 0xFFFF)
                m.write16(start_point + 4, z1)
                m.write16(end_point, (170 + case * 13) & 0xFFFF)
                m.write16(end_point + 2, (210 + case * 5) & 0xFFFF)
                m.write16(end_point + 4, z2)
                m.write32(0x800EC354, packet_cursor)
                m.write32(0x800EC35C, case % 3)
                write_rand(m, local, packet_cursor, 0x1000)
            color = (0xFF000000 | ((case * 29) & 0xFF) << 16 |
                     ((case * 47) & 0xFF) << 8 | ((case * 61) & 0xFF))
            h.run("DrawRibbonBetween", case,
                  (start_point, end_point, scale_value & M32, color), setup,
                  (C.byref(h.player1_hooks), start_point, end_point, scale_value, color))


CASE_COUNTS = {
    "XaPlay": 48, "CdMgrQueueCommand": 5, "CdMgrIssueCommand": 13,
    "ModelDrawWithOt": 48, "ModelPrepCycleColors": 60,
    "ModelEmitPrimsCycleColors": 64, "DrawRibbonBetween": 64,
    "ArcGetMemberPos": 48, "ActorAnimDecodeEntry": 56,
    "TrailRingReset": 24, "PlayerIdleModelSetup": 40,
    "AimRequestClear": 20, "CameraFixedModeOff": 20,
    "AimModeEnter": 48, "EventCamStartRandom": 48,
    "IsArea53": 64, "HudShowHealth": 64, "PlayerDamage": 128,
}

# Each mutation changes a real decision, field value, return value, or hook argument in one
# function. The case index selects a default fixture that reaches that behavior.
MUTATIONS = [
    ("XaPlay", 4, "flags & 0x0Cu", "flags & 0x04u"),
    ("XaPlay", 1, "ae_wr8(r, AE_PLAYER1_XA_STACK_MODE, AE_PLAYER1_XA_LOOP_MODE_VALUE);", "ae_wr8(r, AE_PLAYER1_XA_STACK_MODE, (AE_PLAYER1_XA_LOOP_MODE_VALUE - 1u));"),
    ("CdMgrQueueCommand", 2, "== AE_PLAYER1_CD_SPECIAL_COMMAND) {", "== (AE_PLAYER1_CD_SPECIAL_COMMAND + 1u)) {"),
    ("CdMgrQueueCommand", 0, "(uint32_t)command);\n    if (parameters == 0)", "(uint32_t)(command ^ 1));\n    if (parameters == 0)"),
    ("CdMgrIssueCommand", 3, "case AE_PLAYER1_CD_READ_COMMAND:", "case (AE_PLAYER1_CD_READ_COMMAND + 1u):"),
    ("CdMgrIssueCommand", 10, "\"VSync\", 0xFFFFFFFFu, 0, 0, 0);", "\"VSync\", 0, 0, 0, 0);"),
    ("ArcGetMemberPos", 1, "member_word & AE_PLAYER1_ARC_MEMBER_SECTOR_MASK", "member_word & (AE_PLAYER1_ARC_MEMBER_SECTOR_MASK - 1u)"),
    ("ArcGetMemberPos", 2, "<< AE_PLAYER1_ARC_SECTOR_BYTES_SHIFT", "<< (AE_PLAYER1_ARC_SECTOR_BYTES_SHIFT - 1u)"),
    ("ActorAnimDecodeEntry", 1, "frame_delta << AE_PLAYER1_ANIM_FRACTION_BITS", "frame_delta << (AE_PLAYER1_ANIM_FRACTION_BITS - 1u)"),
    ("ActorAnimDecodeEntry", 1, "frame_a, frame_b, fraction);", "frame_a, frame_b, fraction ^ 1u);"),
    ("ModelDrawWithOt", 0, "AE_PLAYER1_RENDER_PROJECTION_OFFSET), 6);", "AE_PLAYER1_RENDER_PROJECTION_OFFSET), 5);"),
    ("ModelDrawWithOt", 0, "model, emitter_data, ordering_table, 0);", "model, emitter_data, ordering_table + 4u, 0);"),
    ("ModelPrepCycleColors", 2, "phase / AE_PLAYER1_MODEL_CYCLE_COUNT", "(phase / AE_PLAYER1_MODEL_CYCLE_COUNT + 1)"),
    ("ModelPrepCycleColors", 3, "output + (uint32_t)record * AE_PLAYER1_MODEL_CYCLE_OUTPUT_STRIDE", "output + (uint32_t)record * (AE_PLAYER1_MODEL_CYCLE_OUTPUT_STRIDE + 4u)"),
    ("ModelEmitPrimsCycleColors", 0, "primitive_info, flags);", "primitive_info, flags ^ 1u);"),
    ("ModelEmitPrimsCycleColors", 0, "\"ModelEmitPrimsCycleColors\", model_data, cycle_table,", "\"ModelEmitPrimsCycleColors\", model_data + 4u, cycle_table,"),
    ("TrailRingReset", 0, "entry < AE_PLAYER1_TRAIL_RING_RESET_COUNT", "entry + 1u < AE_PLAYER1_TRAIL_RING_RESET_COUNT"),
    ("TrailRingReset", 0, "entry * AE_PLAYER1_TRAIL_RING_STRIDE", "entry * (AE_PLAYER1_TRAIL_RING_STRIDE - 1u)"),
    ("DrawRibbonBetween", 0, "ae_rd16(r, end + AE_PLAYER1_POINT_X) -", "ae_rd16(r, end + AE_PLAYER1_POINT_X) +"),
    ("DrawRibbonBetween", 0, "AE_PLAYER1_RIBBON_DESCRIPTOR_SCALE, (uint32_t)scale", "AE_PLAYER1_RIBBON_DESCRIPTOR_SCALE, (uint32_t)(scale + 1)"),
    ("PlayerIdleModelSetup", 1, "blob + AE_PLAYER1_IDLE_BLOB_STATES_FIELD)", "blob + AE_PLAYER1_IDLE_BLOB_STATES_FIELD + 2u)"),
    ("PlayerIdleModelSetup", 1, "blob + AE_PLAYER1_IDLE_BLOB_SECONDARY_FRAMES_FIELD)", "blob + AE_PLAYER1_IDLE_BLOB_SECONDARY_FRAMES_FIELD + 2u)"),
    ("AimRequestClear", 2, "ae_rd8(r, request) == 1u", "ae_rd8(r, request) == 2u"),
    ("AimRequestClear", 1, "ae_wr8(r, request, 0);", "ae_wr8(r, request, 1);"),
    ("CameraFixedModeOff", 0, "AE_PLAYER1_CAMERA_MODE, 0);", "AE_PLAYER1_CAMERA_MODE, 1);"),
    ("CameraFixedModeOff", 0, "AE_PLAYER1_CAMERA_FLAGS, 0);", "AE_PLAYER1_CAMERA_FLAGS, 1);"),
    ("AimModeEnter", 2, "ae_rd8(r, aim_mode) == 1u", "ae_rd8(r, aim_mode) != 0u"),
    ("AimModeEnter", 0, "ae_wr8(r, aim_mode, 1);", "ae_wr8(r, aim_mode, 0);"),
    ("EventCamStartRandom", 1, "previous * 5u + 1u", "previous * 7u + 1u"),
    ("EventCamStartRandom", 0, "((next >> 8) & 1u) | 2u", "((next >> 8) & 1u) | 3u"),
    ("IsArea53", 2, "== AE_PLAYER1_CURRENT_AREA_IS_53", "== (AE_PLAYER1_CURRENT_AREA_IS_53 - 1u)"),
    ("IsArea53", 3, "ae_rd16(r, AE_PLAYER1_AREA_ID) ==", "ae_rd16(r, AE_PLAYER1_AREA_ID) !="),
    ("HudShowHealth", 0, "if (mode == 0)", "if (mode == 1)"),
    ("HudShowHealth", 26, "timer >= AE_PLAYER1_HUD_TIMER_LIMIT", "timer > AE_PLAYER1_HUD_TIMER_LIMIT"),
    ("PlayerDamage", 1, "if (amount == 0) return;", "if (amount == 1) return;"),
    ("PlayerDamage", 49, "level == AE_PLAYER1_LEVEL_NO_DAMAGE_B", "level == (AE_PLAYER1_LEVEL_NO_DAMAGE_B - 2u)"),
]


def mutate_function(source: str, function: str, old: str, new: str) -> str:
    signature = source.index(f"ae_{function}(")
    opening = source.index("{", signature)
    depth = 0
    closing = None
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                closing = index
                break
    if closing is None:
        raise RuntimeError(f"could not find end of ae_{function}")
    body = source[opening:closing]
    if old not in body:
        raise RuntimeError(f"mutation target was not found in ae_{function}: {old!r}")
    return source[:opening] + body.replace(old, new, 1) + source[closing:]


def run_mutations(seed: int) -> int:
    original = (ROOT / "src/player/ae_player1.c").read_text(encoding="utf-8")
    variant_path = ROOT / "extract/checks/player1_mutation.c"
    h = Harness(build_dll(), coverage=False)
    h.quiet_failures = True
    caught = 0
    for index, (name, case_index, old, new) in enumerate(MUTATIONS):
        variant_path.write_text(mutate_function(original, name, old, new), encoding="utf-8")
        dll = build_dll(variant_path, f"mut{index:02d}")
        h.load_library(dll)
        h.failures = 0
        h.failure_messages.clear()
        h.counts.clear()
        scale = (case_index + 1) / CASE_COUNTS[name]
        only = {name}
        run_cd_functions(h, scale, seed, only)
        run_render_helpers(h, scale, seed, only)
        run_early_functions(h, scale, seed, only)
        run_additional_functions(h, scale, seed, only)
        if h.counts.get(name, 0) == 0:
            print(f"SURVIVED {name} mutation {index + 1}: selected fixture did not run")
        elif h.failures == 0:
            print(f"SURVIVED {name} mutation {index + 1}: differential checker did not catch it")
        else:
            caught += 1
            print(f"caught {name} mutation {index + 1}: {h.failure_messages[0][2].splitlines()[0]}")
    print(f"mutation result: {caught}/{len(MUTATIONS)} caught across {len(CASE_COUNTS)} implemented functions")
    return 0 if caught == len(MUTATIONS) else 1


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", type=float, default=100, help="case-count scale as a percentage")
    ap.add_argument("--seed", type=lambda value: int(value, 0), default=0xAEE501)
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--only", help="comma-separated function names for a partial validation run")
    ap.add_argument("--mutate", action="store_true", help="run two differential mutants per implemented function")
    a = ap.parse_args()
    if not (ROOT / "extract/disc/SCUS_944.23").is_file():
        print("SKIP: extract/disc/SCUS_944.23 not present")
        return 0
    if a.mutate:
        return run_mutations(a.seed)
    only = None if not a.only else set(a.only.split(","))
    unknown = (only or set()) - set(FUNCTIONS)
    if unknown:
        ap.error("unknown function(s): " + ", ".join(sorted(unknown)))
    scale = max(0.01, a.cases / 100.0)
    started = time.time()
    harness = Harness(build_dll(), coverage=True)
    run_cd_functions(harness, scale, a.seed, only)
    run_render_helpers(harness, scale, a.seed, only)
    run_early_functions(harness, scale, a.seed, only)
    run_additional_functions(harness, scale, a.seed, only)
    print("cases per function:")
    for name, count in sorted(harness.counts.items()):
        print(f"  {name:28s} {count}")
    print(f"real area members used: {', '.join(sorted(harness._areas_used))}")
    print(f"MIPS instructions executed: {harness.m.steps}; {time.time() - started:.1f}s")
    default_run = a.only is None and a.cases == ap.get_default("cases") and a.seed == ap.get_default("seed")
    harness.finish(EXTRA_COVERAGE, default_run=default_run, show_coverage=a.coverage)
    if harness.failures:
        print(f"FAILED: {harness.failures} mismatching cases")
        return 1
    print("OK: C and original instructions match")
    return 0


if __name__ == "__main__":
    sys.exit(main())
