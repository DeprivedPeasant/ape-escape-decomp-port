"""Differential checks for the nine requested area-02 movement helpers.

The original SCUS_944.23 instructions run with one of several real area
members loaded. C runs the AeRam ports from src/world/ae_area02*.c against the
same snapshot. The only intercepted API is the BIOS word-clear routine used
by ObjSeekPoint; its four argument registers and memory effects are compared.
"""
import argparse
import ctypes as C
import random
import re
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/mips"), str(ROOT / "tools")]
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402
import check_game  # noqa: E402
import check_zako1 as z1  # noqa: E402

P, U32, I32 = C.c_void_p, C.c_uint32, C.c_int32
STACK_LO, STACK_HI = 0x801FF000, 0x80200000
BIOS_CLEAR = 0x800A3740
OBJ = 0x801F2000
PATH = 0x80180000
AREA_IDS = (1, 2, 4, 5)
DEFAULT_CASES = 32
DEFAULT_SEED = 0xA202

# Each entry is (MIPS address, byte size, C name, return kind, C argument
# kinds after AeRam / optional AeZako1Hooks). The root MIPS arguments are the
# original function arguments only; hooks are checker-side boundaries.
FUNCS = {
    "HalfPipeRangeTest": (0x80050490, 236, "ae_HalfPipeRangeTest", "s", "ssss", False),
    "ObjMove": (0x800554F0, 52, "ae_ObjMove", "v", "u", False),
    "ObjSeekPoint": (0x80055718, 372, "ae_ObjSeekPoint", "s", "ussss", True),
    "ZakoHooksAdd": (0x80057588, 132, "ae_ZakoHooksAdd", "s", "u", False),
    "ObjSeekPathPoint": (0x80058400, 108, "ae_ObjSeekPathPoint", "s", "uuss", True),
    "ObjAdvancePath": (0x800584C8, 264, "ae_ObjAdvancePath", "v", "us", False),
    "ObjFollowPath": (0x800585D0, 104, "ae_ObjFollowPath", "s", "usss", True),
    "ObjSetPosToPathPoint": (0x80058638, 52, "ae_ObjSetPosToPathPoint", "v", "us", False),
    "ObjFacePathPoint": (0x8005866C, 148, "ae_ObjFacePathPoint", "v", "us", False),
}

# Each mutation changes observable game logic and names a directed case that
# must distinguish the original function from its mutant.
MUTATIONS = {
    "HalfPipeRangeTest": (
        (8, "if (body_height < 96)", "if (body_height < 97)"),
        (9, "clearance_bottom < y_below_margin && y_below_margin < lower_y",
         "clearance_bottom <= y_below_margin && y_below_margin < lower_y"),
    ),
    "ObjMove": (
        (0, "position_x + (uint32_t)velocity_x",
         "position_x - (uint32_t)velocity_x"),
        (1, "position_y + (uint32_t)velocity_y",
         "position_y - (uint32_t)velocity_y"),
    ),
    "ObjSeekPoint": (
        (0, "AE_AREA02_SEEK_POINT_CLEAR_DIRECT",
         "AE_AREA02_SEEK_POINT_CLEAR_DIRECT + 4u"),
        (8, "AE_AREA02_SEEK_POINT_CLEAR_DIRECT",
         "AE_AREA02_SEEK_POINT_CLEAR_DIRECT + 8u"),
    ),
    "ZakoHooksAdd": (
        (2, "count >= AE_AREA02_HOOK_TABLE_CAPACITY",
         "count > AE_AREA02_HOOK_TABLE_CAPACITY"),
        (4, "(int32_t)AE_AREA02_HOOK_TABLE_CAPACITY - 1;",
         "(int32_t)AE_AREA02_HOOK_TABLE_CAPACITY - 2;"),
    ),
    "ObjSeekPathPoint": (
        (0, "AE_AREA02_SEEK_POINT_CLEAR_PATH",
         "AE_AREA02_SEEK_POINT_CLEAR_PATH + 4u"),
        (1, "AE_AREA02_SEEK_POINT_CLEAR_PATH",
         "AE_AREA02_SEEK_POINT_CLEAR_PATH + 8u"),
    ),
    "ObjAdvancePath": (
        (0, "next_index % count", "(next_index + 1u) % count"),
        (1, "(int32_t)count - 1", "(int32_t)count - 2"),
    ),
    "ObjFollowPath": (
        (0, "area02_s16((uint32_t)reached) == 0",
         "area02_s16((uint32_t)reached) != 0"),
        (1, "area02_s16((uint32_t)reached) == 0",
         "area02_s16((uint32_t)reached) == 1"),
    ),
    "ObjSetPosToPathPoint": (
        (0, "(uint32_t)ae_rds16(r, point + AE_AREA02_PATH_POINT_X)",
         "0u - (uint32_t)ae_rds16(r, point + AE_AREA02_PATH_POINT_X)"),
        (1, "obj + AE_OBJ_POS_Y,\n            (uint32_t)ae_rds16(r, point + AE_AREA02_PATH_POINT_Y)",
         "obj + AE_OBJ_POS_Z,\n            (uint32_t)ae_rds16(r, point + AE_AREA02_PATH_POINT_Y)"),
    ),
    "ObjFacePathPoint": (
        (0, "ae_rds16(r, obj + AE_OBJ_FACING_X),\n        ae_rds16(r, obj + AE_OBJ_FACING_Z)",
         "ae_rds16(r, obj + AE_OBJ_FACING_Z),\n        ae_rds16(r, obj + AE_OBJ_FACING_X)"),
        (1, "obj + AE_OBJ_HEADING, (uint32_t)heading",
         "obj + AE_OBJ_TURN_SPEED, (uint32_t)heading"),
    ),
}

# AeZako object fields used by the test fixture, matching the named offsets in
# src/world/ae_zako_init.h. Keeping fixture names explicit makes cases legible.
OBJ_POS_X = 0x024
OBJ_POS_Y = 0x028
OBJ_POS_Z = 0x02C
OBJ_FACING_X = 0x060
OBJ_FACING_Y = 0x062
OBJ_FACING_Z = 0x064
OBJ_PATH_COUNT = 0x08C
OBJ_PATH_INDEX = 0x08E
OBJ_PATH = 0x090
OBJ_VELOCITY_X = 0x094
OBJ_VELOCITY_Y = 0x096
OBJ_VELOCITY_Z = 0x098
OBJ_HEADING = 0x0AC
OBJ_TURN_SPEED = 0x0B0
OBJ_FLAGS = 0x0E0
OBJ_FLAG_PING_PONG = 0x00000040
PATH_POINT_SIZE = 8
HOOKS = 0x800B8D20
HOOK_COUNT = 0x800E77A5


def u32(value):
    return int(value) & 0xFFFFFFFF


def s32(value):
    value &= 0xFFFFFFFF
    return value - 0x100000000 if value & 0x80000000 else value


def s16(value):
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


class MipsMem:
    def __init__(self, cpu):
        self.cpu = cpu

    def rd8(self, address): return self.cpu.read8(address)
    def rd16(self, address): return self.cpu.read16(address)
    def rd32(self, address): return self.cpu.read32(address)
    def wr8(self, address, value): self.cpu.write8(address, value)
    def wr16(self, address, value): self.cpu.write16(address, value)
    def wr32(self, address, value): self.cpu.write32(address, value)
    def write_bytes(self, address, data): self.cpu.write_bytes(address, data)


class Harness:
    def __init__(self, dll, seed=DEFAULT_SEED):
        reg.declare("stack_window", "801FF000..80200000")
        members = dump_areas.area_members()
        missing = [area for area in AREA_IDS if area not in members]
        if missing:
            raise SystemExit(f"real area members are missing for area ids {missing}")
        self.bases = {
            area: mr.MipsRam.from_extract(
                ROOT / f"extract/members/{members[area]:03X}.bin").snapshot()
            for area in AREA_IDS
        }
        self.cpu = mr.MipsRam.from_extract(
            ROOT / f"extract/members/{members[AREA_IDS[0]]:03X}.bin")
        self.cpu.attach_gte_oracle()
        self.coverage = set()
        self.cpu.coverage = self.coverage
        self.c = z1.CSide(dll)
        self.seed = seed
        self.rng = random.Random(seed)
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0
        self._bind_c_functions()

    def _bind_c_functions(self):
        for name, (_address, _size, symbol, result, kinds, takes_hooks) in FUNCS.items():
            fn = getattr(self.c.dll, symbol)
            fn.argtypes = [P]
            if takes_hooks:
                fn.argtypes.append(C.POINTER(z1.Zako1Hooks))
            fn.argtypes.extend(I32 if kind == "s" else U32 for kind in kinds)
            fn.restype = None if result == "v" else I32

    def _bind_symbol(self, symbol, name):
        _address, _size, _normal, result, kinds, takes_hooks = FUNCS[name]
        fn = getattr(self.c.dll, symbol)
        fn.argtypes = [P]
        if takes_hooks:
            fn.argtypes.append(C.POINTER(z1.Zako1Hooks))
        fn.argtypes.extend(I32 if kind == "s" else U32 for kind in kinds)
        fn.restype = None if result == "v" else I32
        return fn

    def _seed_fixture(self, mem, rng):
        mem.write_bytes(OBJ, bytes(0x140))
        mem.wr32(OBJ + OBJ_POS_X, rng.getrandbits(32))
        mem.wr32(OBJ + OBJ_POS_Y, rng.getrandbits(32))
        mem.wr32(OBJ + OBJ_POS_Z, rng.getrandbits(32))
        for offset in (OBJ_VELOCITY_X, OBJ_VELOCITY_Y, OBJ_VELOCITY_Z,
                       OBJ_FACING_X, OBJ_FACING_Y, OBJ_FACING_Z,
                       OBJ_HEADING, OBJ_TURN_SPEED):
            mem.wr16(OBJ + offset, rng.getrandbits(16))
        mem.wr32(OBJ + OBJ_PATH, PATH)
        mem.wr16(OBJ + OBJ_PATH_COUNT, 5)
        mem.wr8(OBJ + OBJ_PATH_INDEX, 0)
        mem.wr32(OBJ + OBJ_FLAGS, 0)
        for index in range(256):
            point = PATH + index * PATH_POINT_SIZE
            for axis in (0, 2, 4):
                mem.wr16(point + axis, rng.getrandbits(16))
        mem.wr8(HOOK_COUNT, 0)
        mem.wr32(HOOKS, 0x80040000)
        mem.wr32(HOOKS + 4, 0x80040004)

    def _point(self, mem, index, x, y, z):
        signed_index = s16(index)
        address = PATH + signed_index * PATH_POINT_SIZE
        mem.wr16(address, x)
        mem.wr16(address + 2, y)
        mem.wr16(address + 4, z)
        return address

    def _near_seek(self, mem, dx, dy, dz, radius):
        x = mem.rd16(OBJ + OBJ_POS_X)
        y = mem.rd16(OBJ + OBJ_POS_Y)
        z = mem.rd16(OBJ + OBJ_POS_Z)
        return [OBJ, s32(x + dx), s32(y + dy), s32(z + dz), radius]

    def _case(self, name, index, mem, rng):
        """Seed one edge or randomized call and return (args, expected-trap)."""
        if name == "HalfPipeRangeTest":
            edge = [
                (200, 95, 300, 100), (500, 400, 350, 100),
                (500, 200, 350, 100), (500, 300, 320, 20),
                (500, 100, 400, 200), (100, 240, 150, -100),
                (0x7FFFFFF0, 0x7FFFFFFF, -0x7FFFFFF0, 0x7FFFFFFF),
                (-0x80000000, -96, 0x7FFFFFFF, -0x80000000),
                (260, 96, 300, 100), (296, 95, 300, 100),
            ]
            if index < len(edge):
                return list(edge[index]), False
            return [s32(rng.getrandbits(32)), s32(rng.getrandbits(32)),
                    s32(rng.getrandbits(32)), s32(rng.getrandbits(32))], False

        if name == "ObjMove":
            mem.wr16(OBJ + OBJ_VELOCITY_X, 10 + (index & 1))
            mem.wr16(OBJ + OBJ_VELOCITY_Y, -20 - (index & 1))
            mem.wr16(OBJ + OBJ_VELOCITY_Z, 3)
            return [OBJ], False

        if name == "ObjSeekPoint":
            seeks = [
                ((1, 2, 3), 100), ((0, 10, 0), 500),
                ((20, 0, 0), 10), ((-5, -6, -7), 0),
                ((7, 8, 9), -20), ((3, -4, 5), 0x10000),
                ((0x7FFFFFFF, -0x80000000, 0x7FFF), 32767),
                ((-32768, 32767, -1), 46341),
            ]
            if index < len(seeks):
                (dx, dy, dz), radius = seeks[index]
            elif index == 8:
                (dx, dy, dz), radius = (3, 4, 0), 100
            else:
                (dx, dy, dz) = (rng.randint(-32768, 32767) for _ in range(3))
                radius = rng.choice((0, 1, 8, 64, 256, 4096, 32767, -200, 46341))
            return self._near_seek(mem, dx, dy, dz, radius), False

        if name == "ZakoHooksAdd":
            callbacks = [0x80050100, 0x80050104, 0x80050108,
                         0x8005010C, 0x80050110, 0x80050114]
            count = (0, 1, 2, 1, 0, 255)[index] if index < 6 else rng.choice((0, 1, 2, 3, 255))
            callback = callbacks[index] if index < len(callbacks) else 0x80050200 + index * 4
            mem.wr8(HOOK_COUNT, count)
            mem.wr32(HOOKS, 0x80050100)
            mem.wr32(HOOKS + 4, 0x80050104)
            if index == 3:
                callback = 0x80050100  # duplicate in the first slot
            elif index == 4:
                callback = 0x80050104  # duplicate in the second, even at count zero
            elif index == 5:
                callback = 0x80050114  # full table returns zero without mutation
            return [callback], False

        if name == "ObjSeekPathPoint":
            mem.wr32(OBJ + OBJ_POS_X, 1000)
            mem.wr32(OBJ + OBJ_POS_Y, 2000)
            mem.wr32(OBJ + OBJ_POS_Z, 3000)
            indices = (0, 1, 2, 255, 257, 0xFFFFFFFF)
            point_index = indices[index % len(indices)]
            slot = point_index & 0xFF
            x = mem.rd16(OBJ + OBJ_POS_X)
            y = mem.rd16(OBJ + OBJ_POS_Y)
            z = mem.rd16(OBJ + OBJ_POS_Z)
            if index == 1:
                dx, dz, path_y = 1, 1, y + 2
            else:
                dx, dz = ((1, 3) if index % 2 == 0 else (40, -70))
                path_y = y + 200 if index % 2 == 0 else y + 2
            self._point(mem, slot, x + dx, path_y, z + dz)
            mode = (1, 0, 2, -255, 257, -1)[index % 6]
            radius = (100, 10, 500, 0, -64, 32767)[index % 6]
            return [OBJ, point_index, mode, radius], False

        if name == "ObjAdvancePath":
            states = (
                (5, 0, 0, 0), (5, 3, 0, OBJ_FLAG_PING_PONG),
                (5, 2, 1, OBJ_FLAG_PING_PONG), (5, 0, 1, OBJ_FLAG_PING_PONG),
                (5, 4, 1, 0), (5, 1, 1, 0),
                (0, 0, 0, 0), (0, 0, 0, OBJ_FLAG_PING_PONG),
                (258, 1, 1, 0), (1, 0, 1, 0),
            )
            if index < len(states):
                count, path_index, reverse, flags = states[index]
            else:
                count = rng.choice((0, 1, 2, 5, 17, 255, 256, 258))
                path_index = rng.randrange(256)
                reverse = rng.randrange(2)
                flags = OBJ_FLAG_PING_PONG if rng.randrange(2) else 0
            mem.wr16(OBJ + OBJ_PATH_COUNT, count)
            mem.wr8(OBJ + OBJ_PATH_INDEX, path_index)
            mem.wr32(OBJ + OBJ_FLAGS, flags)
            return [OBJ, reverse], count == 0 and reverse == 0

        if name == "ObjFollowPath":
            mem.wr32(OBJ + OBJ_POS_X, 1000)
            mem.wr32(OBJ + OBJ_POS_Y, 2000)
            mem.wr32(OBJ + OBJ_POS_Z, 3000)
            states = (
                (0, 5, 100, 0, 1, 0, (1, 2, 3)),
                (1, 5, 1, 0, 0, 0, (50, 20, 40)),
                (2, 5, 100, 1, 1, 0x101, (1, 1, 1)),
                (0, 5, 100, 1, OBJ_FLAG_PING_PONG, 1, (2, 2, 2)),
                (4, 5, 100, 1, 0, 1, (1, 0, 1)),
                (0, 5, 100, 1, 0, 0, (1, 1, 1)),
            )
            if index < len(states):
                path_index, count, radius, mode, flags, reverse, delta = states[index]
            else:
                path_index, count = rng.randrange(5), rng.choice((3, 5, 8))
                radius = rng.choice((1, 10, 100, 1000, 32767))
                mode = rng.choice((0, 1, -1, 2, 257))
                flags = OBJ_FLAG_PING_PONG if rng.randrange(2) else 0
                reverse = rng.randrange(2)
                delta = tuple(rng.randint(-100, 100) for _ in range(3))
            mem.wr16(OBJ + OBJ_PATH_COUNT, count)
            mem.wr8(OBJ + OBJ_PATH_INDEX, path_index)
            mem.wr32(OBJ + OBJ_FLAGS, flags)
            px = mem.rd16(OBJ + OBJ_POS_X)
            py = mem.rd16(OBJ + OBJ_POS_Y)
            pz = mem.rd16(OBJ + OBJ_POS_Z)
            self._point(mem, path_index, px + delta[0], py + delta[1], pz + delta[2])
            return [OBJ, radius, reverse, mode], False

        if name == "ObjSetPosToPathPoint":
            mem.wr32(OBJ + OBJ_POS_X, 1000)
            mem.wr32(OBJ + OBJ_POS_Y, 2000)
            mem.wr32(OBJ + OBJ_POS_Z, 3000)
            indices = (0, 1, 7, 255, -1, 0x8000, 0x7FFF, 0xFFFF)
            point_index = indices[index % len(indices)]
            self._point(mem, point_index, -123, 456, -789)
            return [OBJ, point_index], False

        if name == "ObjFacePathPoint":
            mem.wr32(OBJ + OBJ_POS_X, 1000)
            mem.wr32(OBJ + OBJ_POS_Y, 2000)
            mem.wr32(OBJ + OBJ_POS_Z, 3000)
            deltas = ((100, 0), (0, 100), (-100, 0), (0, -100),
                      (0, 0), (32767, -32768), (-32768, 32767),
                      (1, 1), (-1, -1))
            point_index = (0, 1, 2, 3, 4, 5, 6, -1, 0x8000)[index % 9]
            dx, dz = deltas[index % len(deltas)]
            x = mem.rd16(OBJ + OBJ_POS_X)
            y = mem.rd16(OBJ + OBJ_POS_Y)
            z = mem.rd16(OBJ + OBJ_POS_Z)
            self._point(mem, point_index, x + dx, y, z + dz)
            return [OBJ, point_index], False

        raise KeyError(name)

    def _bios_hooks(self, model):
        def clear_words(cpu, regs):
            model.mem = MipsMem(cpu)
            return model.call("FUN_800A3740", tuple(regs[4:8]))
        return {BIOS_CLEAR: clear_words}

    def _c_atan2_hook(self):
        atan2 = self.c.dll.ae_Atan2Safe
        atan2.argtypes = [P, I32, I32]
        atan2.restype = I32
        def callback(_user, ram, x, z, _a2, _a3):
            return atan2(ram, s32(x), s32(z))
        hook = z1.HOOKFN(callback)
        self.c.hooks.Atan2Safe = hook
        self.c.callbacks.append(hook)

    def run(self, name, index, record=True, report=True, symbol=None):
        area = AREA_IDS[index % len(AREA_IDS)]
        self.cpu.restore(self.bases[area])
        self.cpu.gte_reset()
        mem = MipsMem(self.cpu)
        rng = random.Random(self.seed + index * 101 + list(FUNCS).index(name) * 10007)
        self._seed_fixture(mem, rng)
        args, expected_trap = self._case(name, index, mem, rng)
        before = self.cpu.snapshot()
        mm_model = z1.HookModel(MipsMem(self.cpu))
        c_model = z1.HookModel(None)
        self.c.sync(before)
        self.c.set_model(c_model)
        self._c_atan2_hook()
        if record:
            self.counts[name] += 1

        mips_result = None
        mips_error = None
        try:
            address = FUNCS[name][0]
            mips_result = self.cpu.call(address, args,
                                        hooks=self._bios_hooks(mm_model))
        except mr.MipsError as exc:
            mips_error = str(exc)

        symbol = symbol or FUNCS[name][2]
        c_fn = self._bind_symbol(symbol, name)
        prefix = [self.c.ram]
        if FUNCS[name][5]:
            prefix.append(C.byref(self.c.hooks))
        c_result = c_fn(*prefix, *args)
        c_failed = bool(self.c.dll.ae_ram_failed(self.c.ram))
        c_error = (self.c.dll.ae_ram_error(self.c.ram).decode(errors="replace")
                   if c_failed else "")
        matching_vector_overflow = (
            name == "ObjSeekPoint" and mips_error is not None and
            "add overflow at 80030234" in mips_error.lower() and
            c_failed and "VectorNormal: add overflow" in c_error)

        problems = []
        if expected_trap:
            if mips_error is None or "break at 8005850c" not in mips_error.lower():
                problems.append(f"expected original divide break at 8005850C, got {mips_error!r}")
            if not c_failed:
                problems.append("C did not report the matching zero-divisor failure")
        elif matching_vector_overflow:
            pass
        else:
            if mips_error is not None:
                problems.append(f"MIPS execution failed: {mips_error}")
            if c_failed:
                problems.append(f"C RAM error: {c_error}")
            if mips_error is None and FUNCS[name][3] != "v":
                if u32(c_result) != mips_result[2]:
                    problems.append(f"return C={u32(c_result):08X} MIPS={mips_result[2]:08X}")
        if mm_model.events != c_model.events:
            problems.append(f"BIOS hook calls differ C={c_model.events!r} MIPS={mm_model.events!r}")

        stack_offset = STACK_LO - 0x80000000
        expected = bytes(self.cpu.ram[:stack_offset])
        actual = bytes(self.c.main[:stack_offset])
        if expected != actual:
            offset = next(i for i, (left, right) in enumerate(zip(expected, actual))
                          if left != right)
            problems.append(
                f"RAM differs at {0x80000000 + offset:08X}: "
                f"C={actual[offset:offset + 8].hex()} MIPS={expected[offset:offset + 8].hex()}")
        if bytes(self.cpu.scratch) != bytes(self.c.scratch):
            problems.append("scratchpad differs")

        if problems:
            if record:
                self.failures += 1
            if report:
                print(f"FAIL {name}[case={index},area={area:02X}] args={args}: " +
                      " | ".join(problems), flush=True)
            return False, " | ".join(problems)
        return True, ""

    def all(self, cases=DEFAULT_CASES):
        self.coverage.clear()
        passed = True
        for name in FUNCS:
            for index in range(cases):
                ok, _ = self.run(name, index)
                passed = ok and passed
            print(f"{name}: {self.counts[name]} cases", flush=True)
        return passed

    def finish(self, default_run=False, show_coverage=False):
        items = [(name, data[0], data[1], True)
                 for name, data in FUNCS.items()]
        summary = reg.coverage_summary(items, self.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        gaps = {name: row for name, row in summary.items()
                if row["uncovered_count"] or row["stale_declared"]}
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps,
                              extra={"areas": list(AREA_IDS), "seed": self.seed},
                              function_names=list(FUNCS))
        for name, row in gaps.items():
            print(f"COVERAGE GAP {name}: {row['uncovered']}; "
                  f"stale={row['stale_declared']}", flush=True)
        return not gaps and not self.failures


def _function_span(source, name):
    pattern = re.compile(r"(?m)^(?:int32_t|uint32_t|void)\s+(ae_" +
                         re.escape(name) + r")\s*\(")
    match = pattern.search(source)
    if not match:
        raise AssertionError(f"missing ae_{name} for mutation")
    opening = source.index("{", match.start())
    depth, cursor = 1, opening + 1
    while depth:
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
        cursor += 1
    return match.start(), opening, cursor - 1


def _mutant_source(path, names):
    source = path.read_text(encoding="utf-8")
    clones = []
    for name in names:
        start, opening, end = _function_span(source, name)
        signature, body = source[start:opening], source[opening + 1:end]
        for variant in range(2):
            _case_index, old, new = MUTATIONS[name][variant]
            if old not in body:
                raise AssertionError(f"mutation source not found in {name} variant={variant}")
            mutated_body = body.replace(old, new, 1)
            symbol = f"ae_mut_{name}_{variant}"
            clone_signature = signature.replace("ae_" + name, symbol, 1)
            clones.append(clone_signature + "{" + mutated_body + "}\n")
    include = f'#include "../../src/world/{path.name}"\n'
    return include + "\n" + "\n".join(clones)


def run_mutations():
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    original_area = ROOT / "src/world/ae_area02.c"
    original_paths = ROOT / "src/world/ae_area02_paths.c"
    area_mutant = out / "ae_area02_mutants.c"
    paths_mutant = out / "ae_area02_paths_mutants.c"
    area_mutant.write_text(_mutant_source(original_area, ["HalfPipeRangeTest"]),
                           encoding="utf-8")
    path_names = [name for name in FUNCS if name != "HalfPipeRangeTest"]
    paths_mutant.write_text(_mutant_source(original_paths, path_names),
                            encoding="utf-8")
    try:
        dll = check_game.build_dll({
            "src/world/ae_area02.c": area_mutant,
            "src/world/ae_area02_paths.c": paths_mutant,
        }, tag="area02_mutants")
        harness = Harness(dll)
        caught = 0
        for name in FUNCS:
            for variant in range(2):
                case_index = MUTATIONS[name][variant][0]
                ok, reason = harness.run(name, case_index,
                                         record=False, report=False,
                                         symbol=f"ae_mut_{name}_{variant}")
                if ok:
                    print(f"MUTATION SURVIVED {name} variant={variant}", flush=True)
                else:
                    caught += 1
                    print(f"mutation caught: {name} variant={variant}: {reason}",
                          flush=True)
        print(f"mutations caught: {caught}/{2 * len(FUNCS)}", flush=True)
        return 0 if caught == 2 * len(FUNCS) else 1
    finally:
        area_mutant.unlink(missing_ok=True)
        paths_mutant.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--cases", type=int, default=DEFAULT_CASES)
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    args = parser.parse_args()
    if args.cases < 1:
        raise SystemExit("--cases must be positive")
    if args.mutate:
        return run_mutations()
    if not (ROOT / "extract/disc/SCUS_944.23").is_file():
        raise SystemExit("missing extract/disc/SCUS_944.23")
    started = time.time()
    dll = check_game.build_dll(tag="area02")
    harness = Harness(dll, seed=args.seed)
    differential_ok = harness.all(args.cases)
    coverage_ok = harness.finish(
        default_run=(args.cases == DEFAULT_CASES and args.seed == DEFAULT_SEED),
        show_coverage=args.coverage)
    print(f"differential: {sum(harness.counts.values())} cases; "
          f"{'PASS' if differential_ok and coverage_ok else 'FAIL'}; "
          f"elapsed {time.time() - started:.1f}s", flush=True)
    return 0 if differential_ok and coverage_ok else 1


if __name__ == "__main__":
    sys.exit(main())
