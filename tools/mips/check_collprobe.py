"""Differential checks for the RAM-image player collision-probe ports."""
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
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

M32 = 0xFFFFFFFF
STACK_LO = 0x801FF000
OUT_DIR = ROOT / "extract/mips"
COLLIDER = 0x801F1000
PROBE = 0x801F1100
OUTPUT = 0x801F1200
AREA_MEMBER = dump_areas.area_members()[0x01]
DIAGONAL_DIV_GAPS = (
    "CollProbe_DiagonalWall 80076638 80076648..80076650 8007667C 8007668C..80076694 "
    "800766BC 800766CC..800766D4 80076700 80076710..80076718 8007675C 8007676C..80076774 "
    "800767A0 800767B0..800767B8 800767E0 800767F0..800767F8 80076824 80076834..8007683C "
    "80076870 80076880..80076888 800768B4 800768C4..800768CC 80076900 80076910..80076918 "
    "80076944 80076954..8007695C 80076998 800769A8..800769B0 800769DC 800769EC..800769F4 "
    "80076A28 80076A38..80076A40 80076A6C 80076A7C..80076A84"
)

FUNCS = {
    "CollProbe_NoContact": (0x80077490, 40, "ae_CollProbe_NoContact", "uuu"),
    "CollProbe_Strip": (0x8007734C, 324, "ae_CollProbe_Strip", "uuu"),
    "CollProbe_ThinWall": (0x80075F48, 612, "ae_CollProbe_ThinWall", "uuu"),
    "CollProbe_EdgeWall": (0x800761AC, 732, "ae_CollProbe_EdgeWall", "uuu"),
    "CollProbe_Box": (0x80073BC8, 1036, "ae_CollProbe_Box", "uuu"),
    "CollProbe_DiagonalWall": (0x80076488, 1600, "ae_CollProbe_DiagonalWall", "uuu"),
    "CollProbe_Radial": (0x80074BBC, 2676, "ae_CollProbe_Radial", "uuu"),
    "CollProbe_HalfPipe": (0x80075630, 1040, "ae_CollProbe_HalfPipe", "uuu"),
    "CollProbe_Slope": (0x80076AC8, 952, "ae_CollProbe_Slope", "uuu"),
    "CollProbe_Ramp": (0x80073FD4, 1224, "ae_CollProbe_Ramp", "uuu"),
    "CollProbe_Trench": (0x80075A40, 1288, "ae_CollProbe_Trench", "uuu"),
    "CollProbe_Triangle": (0x8007449C, 1824, "ae_CollProbe_Triangle", "uuu"),
    "CollProbe_Rope": (0x80076E80, 1228, "ae_CollProbe_Rope", "uuu"),
}
HOOK_PRINTF = 0x800A3720
HOOK_POINT_IN_BOX_XZ = 0x80073B64
HOOK_SLOPE_CONTACT_DIR = 0x8004F884
HOOK_DIR_INDEX_FROM_FLAGS = 0x8007774C
HOOK_PLAYER_SET_CLIMB_TARGET = 0x80066E28
HOOK_PLAYER_SET_ROPE = 0x80066E8C
FORMAT_NO_CONTACT = 0x800A5E80
CURRENT_POSITION = 0x800B20B4
POSITION = 0x801F1300
AE_COLLPROBE_BODY_CUR_TYPE = 0x800B20CC
AE_COLLPROBE_PLAYER_ACTION_A = 0x800B20C6
AE_COLLPROBE_PLAYER_ACTION_B = 0x800B20CA
AE_COLLPROBE_ROPE_ACTION = 0x800B20C8
AE_COLLPROBE_ATTACH_KIND = 0x800F44A0

HOOKFN = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                     C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)


CLIMBFN = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, *([C.c_uint32] * 6))

class Hooks(C.Structure):
    _fields_ = [("user", C.c_void_p), ("stale_wall_axis", C.c_int32),
                ("stale_edge_x", C.c_int16), ("stale_edge_z", C.c_int16),
                ("PointInBoxXZ", HOOKFN), ("printf", HOOKFN), ("HalfPipeRangeTest", HOOKFN),
                ("ColliderSlopeContactDir", HOOKFN), ("DirIndexFromFlags", HOOKFN),
                ("PlayerSetClimbTarget", CLIMBFN), ("PlayerSetRope", HOOKFN)]


def build_dll(source=None):
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    dll = OUT_DIR / f"collprobe_{os.getpid()}_{time.time_ns()}.dll"
    srcs = [ROOT / "src/core/ae_ram.c", ROOT / "src/core/ae_sdk.c",
            ROOT / "src/player/native_player.c", ROOT / "src/world/ae_solid.c",
            ROOT / "src/world/ae_area02.c",
            Path(source) if source else ROOT / "src/collision/ae_collprobe.c"]
    subprocess.run([shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-shared", "-static-libgcc", f"-I{ROOT / 'src/collision'}", f"-I{ROOT / 'src/bodies'}",
                    f"-I{ROOT / 'src/world'}", f"-I{ROOT / 'src/core'}", *map(str, srcs), "-o", str(dll), "-lm"], check=True)
    return dll


class Side:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        P, U, I = C.c_void_p, C.c_uint32, C.c_int32
        self.lib.ae_ram_create.restype = P
        self.lib.ae_ram_destroy.argtypes = [P]
        self.lib.ae_ptr.restype = P
        self.lib.ae_ptr.argtypes = [P, U, U]
        self.lib.ae_ram_failed.argtypes = [P]
        self.lib.ae_ram_failed.restype = C.c_int
        self.lib.ae_ram_error.argtypes = [P]
        self.lib.ae_ram_error.restype = C.c_char_p
        self.lib.ae_CollProbe_NoContact.argtypes = [P, P, U, U, U]
        self.lib.ae_CollProbe_NoContact.restype = I
        self.lib.ae_CollProbe_Strip.argtypes = [P, P, U, U, U]
        self.lib.ae_CollProbe_Strip.restype = I
        self.lib.ae_CollProbe_ThinWall.argtypes = [P, P, U, U, U]
        self.lib.ae_CollProbe_ThinWall.restype = I
        self.lib.ae_CollProbe_EdgeWall.argtypes = [P, P, U, U, U]
        self.lib.ae_CollProbe_EdgeWall.restype = I
        self.lib.ae_CollProbe_Box.argtypes = [P, P, U, U, U]
        self.lib.ae_CollProbe_Box.restype = I
        self.lib.ae_CollProbe_DiagonalWall.argtypes = [P, P, U, U, U]
        self.lib.ae_CollProbe_DiagonalWall.restype = I
        self.lib.ae_CollProbe_HalfPipe.argtypes = [P, P, U, U, U]
        self.lib.ae_CollProbe_HalfPipe.restype = I
        self.lib.ae_CollProbe_Slope.argtypes = [P, P, U, U, U]
        self.lib.ae_CollProbe_Slope.restype = I
        self.lib.ae_CollProbe_Ramp.argtypes = [P, P, U, U, U]
        self.lib.ae_CollProbe_Ramp.restype = I
        self.lib.ae_CollProbe_Trench.argtypes = [P, P, U, U, U]
        self.lib.ae_CollProbe_Trench.restype = I
        self.lib.ae_CollProbe_Triangle.argtypes = [P, P, U, U, U]
        self.lib.ae_CollProbe_Triangle.restype = I
        self.lib.ae_CollProbe_Radial.argtypes = [P, P, U, U, U]
        self.lib.ae_CollProbe_Radial.restype = I
        self.lib.ae_CollProbe_Rope.argtypes = [P, P, U, U, U]
        self.lib.ae_CollProbe_Rope.restype = I
        self.ram = self.lib.ae_ram_create()
        if not self.ram:
            raise MemoryError("ae_ram_create failed")
        self.main = (C.c_uint8 * 0x200000).from_address(self.lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(self.lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.main_view = memoryview(self.main).cast("B")
        self.scratch_view = memoryview(self.scratch).cast("B")
        self.hook_calls = []
        self._keep = []
        def printf(user, ram, a0, a1, a2, a3):
            self.hook_calls.append((HOOK_PRINTF, a0, a1, a2, a3))
            return 0
        def point_in_box_xz(user, ram, a0, a1, a2, a3):
            self.hook_calls.append((HOOK_POINT_IN_BOX_XZ, a0, a1, a2, a3))
            pos = C.addressof(self.main) + (a0 - 0x80000000)
            box = C.addressof(self.main) + (a1 - 0x80000000)
            px = C.c_int32.from_address(pos).value
            pz = C.c_int32.from_address(pos + 8).value
            cx = C.c_int16.from_address(box).value
            hx = C.c_int16.from_address(box + 6).value
            cz = C.c_int16.from_address(box + 4).value
            hz = C.c_int16.from_address(box + 10).value
            return int(cx - hx <= px <= cx + hx and cz - hz <= pz <= cz + hz)
        def collider_slope_contact_dir(user, ram, a0, a1, a2, a3):
            self.hook_calls.append((HOOK_SLOPE_CONTACT_DIR, a0, a1, a2, a3))
            def pointer(address):
                if 0x1F800000 <= address < 0x1F800400:
                    return C.addressof(self.scratch) + (address - 0x1F800000)
                return C.addressof(self.main) + (address - 0x80000000)
            base = pointer(a1)
            out = pointer(a3)
            C.c_int32.from_address(out).value = C.c_int16.from_address(base + 2).value
            C.c_int32.from_address(out + 4).value = C.c_int16.from_address(base + 8).value
            return 1
        def dir_index_from_flags(user, ram, a0, a1, a2, a3):
            self.hook_calls.append((HOOK_DIR_INDEX_FROM_FLAGS, a0, a1, a2, a3))
            return ((a0 & 0x0C00) >> 10)
        def player_set_climb_target(user, ram, a0, a1, a2, a3, a4, a5):
            self.hook_calls.append((HOOK_PLAYER_SET_CLIMB_TARGET, a0, a1, a2, a3, a4, a5))
            return 0
        def player_set_rope(user, ram, a0, a1, a2, a3):
            self.hook_calls.append((HOOK_PLAYER_SET_ROPE, a0, a1, a2, a3))
            return 0
        printf_callback = HOOKFN(printf)
        point_callback = HOOKFN(point_in_box_xz)
        slope_contact_callback = HOOKFN(collider_slope_contact_dir)
        direction_callback = HOOKFN(dir_index_from_flags)
        climb_callback = CLIMBFN(player_set_climb_target)
        rope_callback = HOOKFN(player_set_rope)
        range_callback = HOOKFN()  # HalfPipeRangeTest is a direct port now.
        self._keep.extend((printf_callback, point_callback, slope_contact_callback,
                           direction_callback, climb_callback, rope_callback,
                           range_callback))
        self.hooks = Hooks(None, 0, 0, 0, point_callback, printf_callback, range_callback,
                           slope_contact_callback, direction_callback, climb_callback, rope_callback)


class Harness:
    oracle_cpu = None

    def __init__(self, source=None, cases=100, seed=0x94423, functions=None):
        self.cases = cases
        self.rng = random.Random(seed)
        self.only = None if functions is None else set(functions)
        area = ROOT / "extract/members" / f"{AREA_MEMBER:03X}.bin"
        self.m = mr.MipsRam.from_extract(area=area)
        if Harness.oracle_cpu is None:
            self.m.attach_gte_oracle()
            Harness.oracle_cpu = self.m
        else:
            self.m.gte_state = Harness.oracle_cpu.gte_state
            self.m.gte_backend = Harness.oracle_cpu.gte_backend
        self.m_main = (C.c_uint8 * 0x200000).from_buffer(self.m.ram)
        self.m_scratch = (C.c_uint8 * 0x400).from_buffer(self.m.scratch)
        self.m.coverage = set()
        reg.declare("stack_window", f"{STACK_LO:08X}..80200000")
        reg.declare("ram_mask", "hook PointInBoxXZ peek[2:4]")
        reg.declare("unreachable", "CollProbe_Box 80073DF8 80073E3C")
        reg.declare("unreachable", "CollProbe_Box 80073E08..80073E10 80073E4C..80073E54")
        reg.declare("unreachable", DIAGONAL_DIV_GAPS)
        self.base = self.m.snapshot()
        self.c = Side(build_dll(source))
        self.counts = {name: 0 for name in FUNCS}
        self.failures = []

    def compare(self, name, index, regs, c_ret):
        self.counts[name] += 1
        issues = []
        if self.c.lib.ae_ram_failed(self.c.ram):
            issues.append(self.c.lib.ae_ram_error(self.c.ram).decode("utf-8", "replace"))
        if regs[2] != (c_ret & M32):
            issues.append(f"return differs: MIPS {regs[2]:08X}, C {c_ret & M32:08X}")
        def box_bytes(address, from_c):
            if 0x80000000 <= address < 0x80200000:
                offset = address - 0x80000000
                memory = self.c.main_view if from_c else self.m.ram
                return bytes(memory[offset:offset + 12])
            if 0x1F800000 <= address < 0x1F800400:
                offset = address - 0x1F800000
                memory = self.c.scratch_view if from_c else self.m.scratch
                return bytes(memory[offset:offset + 12])
            return address

        def normalize_hook(entry, from_c):
            if entry[0] == HOOK_POINT_IN_BOX_XZ:
                reg.declare("ram_mask", "hook PointInBoxXZ peek[2:4]")
                reg.declare("ram_mask", "hook PointInBoxXZ box pointer contents")
                return (entry[0], entry[1], box_bytes(entry[2], from_c), 0, 0)
            if entry[0] == HOOK_DIR_INDEX_FROM_FLAGS:
                reg.declare("ram_mask", "hook DirIndexFromFlags peek[1:4]")
                return entry[:2] + (0, 0, 0)
            if entry[0] == HOOK_PLAYER_SET_ROPE:
                reg.declare("ram_mask", "hook PlayerSetRope peek[2:4]")
                return entry[:3] + (0, 0)
            return entry
        m_hooks = [normalize_hook(entry, False) for entry in self.m.hook_log]
        c_hooks = [normalize_hook(entry, True) for entry in self.c.hook_calls]
        if m_hooks != c_hooks:
            issues.append(f"hooks differ: MIPS {m_hooks}, C {c_hooks}")
        main_end = STACK_LO - 0x80000000
        if memoryview(self.m.ram)[:main_end] != self.c.main_view[:main_end]:
            issues.append("main RAM differs below stack window")
        if memoryview(self.m.scratch) != self.c.scratch_view:
            issues.append("scratchpad differs")
        if issues:
            self.failures.append((name, index, issues))

    def run_no_contact(self, index, word=None):
        self.m.restore(self.base)
        probe = bytearray(self.rng.getrandbits(8) for _ in range(0x20))
        if word is not None:
            probe[4:8] = (word & M32).to_bytes(4, "little")
        self.m.write_bytes(COLLIDER, bytes(self.rng.getrandbits(8) for _ in range(0x38)))
        self.m.write_bytes(PROBE, probe)
        self.m.write_bytes(OUTPUT, bytes(self.rng.getrandbits(8) for _ in range(0x20)))
        C.memmove(self.c.main, C.addressof(self.m_main), 0x200000)
        C.memmove(self.c.scratch, C.addressof(self.m_scratch), 0x400)
        self.c.hook_calls.clear()
        self.m.hook_log.clear()

        def printf(cpu, regs):
            return 0
        regs = self.m.call(0x80077490, (COLLIDER, PROBE, OUTPUT), hooks={HOOK_PRINTF: printf})
        c_ret = self.c.lib.ae_CollProbe_NoContact(self.c.ram, C.byref(self.c.hooks), COLLIDER, PROBE, OUTPUT)
        self.compare("CollProbe_NoContact", index, regs, c_ret)

    def run_strip(self, index, geometry, x, y, z, radius, body_height, half_x=256, half_y=128,
                  half_z=64, direction=0, range_top=0):
        self.m.restore(self.base)
        rng = self.rng
        self.m.write_bytes(COLLIDER, bytes(rng.getrandbits(8) for _ in range(0x38)))
        self.m.write8(COLLIDER + 0x10, geometry)
        self.m.write16(COLLIDER + 0x00, 0)
        self.m.write16(COLLIDER + 0x02, range_top)
        self.m.write16(COLLIDER + 0x04, 0)
        self.m.write16(COLLIDER + 0x06, half_x)
        self.m.write16(COLLIDER + 0x08, half_y)
        self.m.write16(COLLIDER + 0x0A, half_z)
        self.m.write16(COLLIDER + 0x2C, direction)
        self.m.write32(CURRENT_POSITION, POSITION)
        self.m.write32(POSITION + 0x00, x)
        self.m.write32(POSITION + 0x04, y)
        self.m.write32(POSITION + 0x08, z)
        probe = bytearray(rng.getrandbits(8) for _ in range(0x10))
        probe[0:4] = (radius & M32).to_bytes(4, "little")
        probe[4:6] = (geometry & 0xFFFF).to_bytes(2, "little")
        probe[8:12] = (body_height & M32).to_bytes(4, "little")
        self.m.write_bytes(PROBE, bytes(probe))
        self.m.write_bytes(OUTPUT, bytes(rng.getrandbits(8) for _ in range(0x18)))
        C.memmove(self.c.main, C.addressof(self.m_main), 0x200000)
        C.memmove(self.c.scratch, C.addressof(self.m_scratch), 0x400)
        self.c.hook_calls.clear()
        self.m.hook_log.clear()

        regs = self.m.call(0x8007734C, (COLLIDER, PROBE, OUTPUT),
                           hooks={})
        c_ret = self.c.lib.ae_CollProbe_Strip(self.c.ram, C.byref(self.c.hooks), COLLIDER, PROBE, OUTPUT)
        self.compare("CollProbe_Strip", index, regs, c_ret)

    def run_wall_band(self, name, index, material, orientation, x, z, radius, extent_x, extent_z,
                      stale_axis=1, stale_edge_x=-256, stale_edge_z=256):
        self.m.restore(self.base)
        rng = self.rng
        shape = bytearray(rng.getrandbits(8) for _ in range(0x38))
        shape[0:2] = (0).to_bytes(2, "little")
        shape[2:4] = (0).to_bytes(2, "little")
        shape[4:6] = (0).to_bytes(2, "little")
        shape[6:8] = (extent_x & 0xFFFF).to_bytes(2, "little")
        shape[8:10] = (128).to_bytes(2, "little")
        shape[10:12] = (extent_z & 0xFFFF).to_bytes(2, "little")
        self.m.write_bytes(COLLIDER, bytes(shape))
        self.m.write32(CURRENT_POSITION, POSITION)
        self.m.write32(POSITION + 0, x)
        self.m.write32(POSITION + 4, 128)
        self.m.write32(POSITION + 8, z)
        probe = bytearray(rng.getrandbits(8) for _ in range(0x10))
        probe[0:4] = (radius & M32).to_bytes(4, "little")
        probe[4:8] = ((material | orientation) & M32).to_bytes(4, "little")
        probe[8:12] = (241).to_bytes(4, "little")
        self.m.write_bytes(PROBE, bytes(probe))
        self.m.write_bytes(OUTPUT, bytes(rng.getrandbits(8) for _ in range(0x18)))
        C.memmove(self.c.main, C.addressof(self.m_main), 0x200000)
        C.memmove(self.c.scratch, C.addressof(self.m_scratch), 0x400)
        self.c.hook_calls.clear()
        self.m.hook_log.clear()

        address = FUNCS[name][0]
        self.c.hooks.stale_wall_axis = stale_axis
        self.c.hooks.stale_edge_x = stale_edge_x
        self.c.hooks.stale_edge_z = stale_edge_z
        regs = self.m.call(address, (COLLIDER, PROBE, OUTPUT),
                           hooks={},
                           initial_regs={17: stale_axis, 18: stale_edge_x, 19: stale_edge_z})
        c_fn = getattr(self.c.lib, FUNCS[name][2])
        c_ret = c_fn(self.c.ram, C.byref(self.c.hooks), COLLIDER, PROBE, OUTPUT)
        self.compare(name, index, regs, c_ret)

    def run_box(self, index, geometry, x, z, previous_x, previous_z, radius, half_x=128, half_z=64, player_y=128, height=241, range_reject_pc=None):
        self.m.restore(self.base)
        rng = self.rng
        shape = bytearray(rng.getrandbits(8) for _ in range(0x38))
        shape[0:2] = (0).to_bytes(2, "little")
        shape[2:4] = (0).to_bytes(2, "little")
        shape[4:6] = (0).to_bytes(2, "little")
        shape[6:8] = (half_x & 0xFFFF).to_bytes(2, "little")
        shape[8:10] = (128).to_bytes(2, "little")
        shape[10:12] = (half_z & 0xFFFF).to_bytes(2, "little")
        self.m.write_bytes(COLLIDER, bytes(shape))
        self.m.write32(CURRENT_POSITION, POSITION)
        self.m.write32(CURRENT_POSITION + 4, POSITION + 0x100)
        self.m.write32(POSITION + 0, x)
        self.m.write32(POSITION + 4, player_y)
        self.m.write32(POSITION + 8, z)
        self.m.write32(POSITION + 0x100, previous_x)
        self.m.write32(POSITION + 0x104, player_y)
        self.m.write32(POSITION + 0x108, previous_z)
        probe = bytearray(rng.getrandbits(8) for _ in range(0x10))
        probe[0:2] = (radius & 0xFFFF).to_bytes(2, "little")
        probe[4] = geometry
        probe[8:12] = (height & M32).to_bytes(4, "little")
        self.m.write_bytes(PROBE, bytes(probe))
        self.m.write_bytes(OUTPUT, bytes(rng.getrandbits(8) for _ in range(0x18)))
        C.memmove(self.c.main, C.addressof(self.m_main), 0x200000)
        C.memmove(self.c.scratch, C.addressof(self.m_scratch), 0x400)
        self.c.hook_calls.clear()
        self.m.hook_log.clear()

        def point_in_box_xz(cpu, regs):
            pos, box = regs[4], regs[5]
            px = self.m.reads32(pos)
            pz = self.m.reads32(pos + 8)
            cx, hx = self.m.reads16(box), self.m.reads16(box + 6)
            cz, hz = self.m.reads16(box + 4), self.m.reads16(box + 10)
            return int(cx - hx <= px <= cx + hx and cz - hz <= pz <= cz + hz)
        coverage_before = set(self.m.coverage)
        regs = self.m.call(FUNCS["CollProbe_Box"][0], (COLLIDER, PROBE, OUTPUT),
                           hooks={HOOK_POINT_IN_BOX_XZ: point_in_box_xz})
        c_ret = self.c.lib.ae_CollProbe_Box(self.c.ram, C.byref(self.c.hooks), COLLIDER, PROBE, OUTPUT)
        self.compare("CollProbe_Box", index, regs, c_ret)
        if range_reject_pc is not None:
            case_coverage = self.m.coverage - coverage_before
            assert range_reject_pc not in case_coverage, (
                f"HalfPipeRangeTest should reject before {range_reject_pc:08X}")
            assert regs[2] == 0, "vertical range rejection must return no contact"

    def run_diagonal_wall(self, index, orientation, x, z, radius=64, half_x=256, half_z=256):
        self.m.restore(self.base)
        rng = self.rng
        shape = bytearray(rng.getrandbits(8) for _ in range(0x38))
        shape[0:2] = (0).to_bytes(2, "little")
        shape[2:4] = (0).to_bytes(2, "little")
        shape[4:6] = (0).to_bytes(2, "little")
        shape[6:8] = (half_x & 0xFFFF).to_bytes(2, "little")
        shape[8:10] = (128).to_bytes(2, "little")
        shape[10:12] = (half_z & 0xFFFF).to_bytes(2, "little")
        self.m.write_bytes(COLLIDER, bytes(shape))
        self.m.write32(CURRENT_POSITION, POSITION)
        self.m.write32(POSITION + 0, x)
        self.m.write32(POSITION + 4, 128)
        self.m.write32(POSITION + 8, z)
        probe = bytearray(rng.getrandbits(8) for _ in range(0x10))
        probe[0:4] = (radius & M32).to_bytes(4, "little")
        probe[4:8] = ((0x8000 | orientation) & M32).to_bytes(4, "little")
        probe[8:12] = (241).to_bytes(4, "little")
        self.m.write_bytes(PROBE, bytes(probe))
        self.m.write_bytes(OUTPUT, bytes(rng.getrandbits(8) for _ in range(0x18)))
        C.memmove(self.c.main, C.addressof(self.m_main), 0x200000)
        C.memmove(self.c.scratch, C.addressof(self.m_scratch), 0x400)
        self.c.hook_calls.clear()
        self.m.hook_log.clear()

        self.m.gte_reset()
        regs = self.m.call(FUNCS["CollProbe_DiagonalWall"][0], (COLLIDER, PROBE, OUTPUT),
                           hooks={})
        c_ret = self.c.lib.ae_CollProbe_DiagonalWall(
            self.c.ram, C.byref(self.c.hooks), COLLIDER, PROBE, OUTPUT)
        self.compare("CollProbe_DiagonalWall", index, regs, c_ret)

    def run_halfpipe(self, index, geometry, x, y, z, radius, body_height,
                     half_x=256, half_y=128, half_z=192, previous=None):
        self.m.restore(self.base)
        rng = self.rng
        self.m.write_bytes(COLLIDER, bytes(rng.getrandbits(8) for _ in range(0x38)))
        self.m.write16(COLLIDER + 0, 0)
        self.m.write16(COLLIDER + 2, 0)
        self.m.write16(COLLIDER + 4, 0)
        self.m.write16(COLLIDER + 6, half_x)
        self.m.write16(COLLIDER + 8, half_y)
        self.m.write16(COLLIDER + 10, half_z)
        self.m.write32(CURRENT_POSITION, POSITION)
        self.m.write32(CURRENT_POSITION + 4, POSITION + 0x100)
        for off, value in ((0, x), (4, y), (8, z)):
            self.m.write32(POSITION + off, value)
        if previous is None:
            previous = (x, y, z)
        for off, value in zip((0, 4, 8), previous):
            self.m.write32(POSITION + 0x100 + off, value)
        probe = bytearray(rng.getrandbits(8) for _ in range(0x10))
        probe[0:2] = (radius & 0xFFFF).to_bytes(2, "little")
        probe[4] = geometry & 0xFF
        probe[8:12] = (body_height & M32).to_bytes(4, "little")
        self.m.write_bytes(PROBE, bytes(probe))
        self.m.write_bytes(OUTPUT, bytes(rng.getrandbits(8) for _ in range(0x20)))
        C.memmove(self.c.main, C.addressof(self.m_main), 0x200000)
        C.memmove(self.c.scratch, C.addressof(self.m_scratch), 0x400)
        self.c.hook_calls.clear()
        self.m.hook_log.clear()
        self.m.gte_reset()

        def point_in_box_xz(cpu, regs):
            pos, box = regs[4], regs[5]
            px, pz = self.m.reads32(pos), self.m.reads32(pos + 8)
            cx, hx = self.m.reads16(box), self.m.reads16(box + 6)
            cz, hz = self.m.reads16(box + 4), self.m.reads16(box + 10)
            return int(cx - hx <= px <= cx + hx and cz - hz <= pz <= cz + hz)
        regs = self.m.call(FUNCS["CollProbe_HalfPipe"][0], (COLLIDER, PROBE, OUTPUT),
                           hooks={HOOK_POINT_IN_BOX_XZ: point_in_box_xz})
        c_ret = self.c.lib.ae_CollProbe_HalfPipe(
            self.c.ram, C.byref(self.c.hooks), COLLIDER, PROBE, OUTPUT)
        self.compare("CollProbe_HalfPipe", index, regs, c_ret)

    def run_slope(self, index, geometry, x, y, z, radius, body_height,
                  half_x=256, half_y=200, half_z=256, inner_radius=64, low_height=64):
        self.m.restore(self.base)
        rng = self.rng
        shape = bytearray(rng.getrandbits(8) for _ in range(0x38))
        for off, value in ((0, 0), (2, 100), (4, 0), (6, half_x),
                           (8, half_y), (10, half_z), (0x10, geometry),
                           (0x24, inner_radius), (0x26, low_height)):
            if off == 0x10:
                shape[off] = value & 0xFF
            else:
                shape[off:off + 2] = (value & 0xFFFF).to_bytes(2, "little")
        self.m.write_bytes(COLLIDER, bytes(shape))
        self.m.write32(CURRENT_POSITION, POSITION)
        self.m.write32(POSITION + 0, x)
        self.m.write32(POSITION + 4, y)
        self.m.write32(POSITION + 8, z)
        probe = bytearray(rng.getrandbits(8) for _ in range(0x10))
        probe[0:4] = (radius & M32).to_bytes(4, "little")
        probe[8:12] = (body_height & M32).to_bytes(4, "little")
        self.m.write_bytes(PROBE, bytes(probe))
        self.m.write_bytes(OUTPUT, bytes(rng.getrandbits(8) for _ in range(0x20)))
        C.memmove(self.c.main, C.addressof(self.m_main), 0x200000)
        C.memmove(self.c.scratch, C.addressof(self.m_scratch), 0x400)
        self.c.hook_calls.clear()
        self.m.hook_log.clear()
        self.m.gte_reset()
        regs = self.m.call(FUNCS["CollProbe_Slope"][0], (COLLIDER, PROBE, OUTPUT))
        c_ret = self.c.lib.ae_CollProbe_Slope(
            self.c.ram, C.byref(self.c.hooks), COLLIDER, PROBE, OUTPUT)
        self.compare("CollProbe_Slope", index, regs, c_ret)

    def run_ramp(self, index, type_word, x, y, z, radius, body_height,
                 half_x=256, half_y=128, half_z=192, previous=None):
        self.m.restore(self.base)
        rng = self.rng
        shape = bytearray(rng.getrandbits(8) for _ in range(0x38))
        for off, value in ((0, 0), (2, 160), (4, 0), (6, half_x), (8, half_y), (10, half_z)):
            shape[off:off + 2] = (value & 0xFFFF).to_bytes(2, "little")
        self.m.write_bytes(COLLIDER, bytes(shape))
        self.m.write32(CURRENT_POSITION, POSITION)
        self.m.write32(CURRENT_POSITION + 4, POSITION + 0x100)
        for off, value in ((0, x), (4, y), (8, z)):
            self.m.write32(POSITION + off, value)
        if previous is None:
            previous = (x, y, z)
        for off, value in zip((0, 4, 8), previous):
            self.m.write32(POSITION + 0x100 + off, value)
        probe = bytearray(rng.getrandbits(8) for _ in range(0x10))
        probe[0:2] = (radius & 0xFFFF).to_bytes(2, "little")
        probe[4:8] = (type_word & M32).to_bytes(4, "little")
        probe[8:12] = (body_height & M32).to_bytes(4, "little")
        self.m.write_bytes(PROBE, bytes(probe))
        self.m.write_bytes(OUTPUT, bytes(rng.getrandbits(8) for _ in range(0x20)))
        C.memmove(self.c.main, C.addressof(self.m_main), 0x200000)
        C.memmove(self.c.scratch, C.addressof(self.m_scratch), 0x400)
        self.c.hook_calls.clear()
        self.m.hook_log.clear()

        def slope_contact_dir(cpu, regs):
            shape_addr, out_addr = regs[5], regs[7]
            cpu.write32(out_addr, cpu.reads16(shape_addr + 2))
            cpu.write32(out_addr + 4, cpu.reads16(shape_addr + 8))
            return 1
        def point_in_box_xz(cpu, regs):
            pos, box = regs[4], regs[5]
            px, pz = self.m.reads32(pos), self.m.reads32(pos + 8)
            cx, hx = self.m.reads16(box), self.m.reads16(box + 6)
            cz, hz = self.m.reads16(box + 4), self.m.reads16(box + 10)
            return int(cx - hx <= px <= cx + hx and cz - hz <= pz <= cz + hz)
        regs = self.m.call(FUNCS["CollProbe_Ramp"][0], (COLLIDER, PROBE, OUTPUT),
                           hooks={HOOK_SLOPE_CONTACT_DIR: slope_contact_dir,
                                  HOOK_POINT_IN_BOX_XZ: point_in_box_xz})
        c_ret = self.c.lib.ae_CollProbe_Ramp(
            self.c.ram, C.byref(self.c.hooks), COLLIDER, PROBE, OUTPUT)
        self.compare("CollProbe_Ramp", index, regs, c_ret)

    def run_trench(self, index, geometry, x, y, z, radius, body_height,
                   half_x=384, half_y=256, half_z=384, previous=None):
        self.m.restore(self.base)
        rng = self.rng
        shape = bytearray(rng.getrandbits(8) for _ in range(0x38))
        for off, value in ((0, 0), (2, 0), (4, 0), (6, half_x),
                           (8, half_y), (10, half_z)):
            shape[off:off + 2] = (value & 0xFFFF).to_bytes(2, "little")
        self.m.write_bytes(COLLIDER, bytes(shape))
        self.m.write32(CURRENT_POSITION, POSITION)
        self.m.write32(CURRENT_POSITION + 4, POSITION + 0x100)
        for off, value in ((0, x), (4, y), (8, z)):
            self.m.write32(POSITION + off, value)
        if previous is None:
            previous = (x, y, z)
        for off, value in zip((0, 4, 8), previous):
            self.m.write32(POSITION + 0x100 + off, value)
        probe = bytearray(rng.getrandbits(8) for _ in range(0x10))
        probe[0:2] = (radius & 0xFFFF).to_bytes(2, "little")
        probe[4] = geometry & 0xFF
        probe[8:12] = (body_height & M32).to_bytes(4, "little")
        self.m.write_bytes(PROBE, bytes(probe))
        self.m.write_bytes(OUTPUT, bytes(rng.getrandbits(8) for _ in range(0x20)))
        C.memmove(self.c.main, C.addressof(self.m_main), 0x200000)
        C.memmove(self.c.scratch, C.addressof(self.m_scratch), 0x400)
        self.c.hook_calls.clear()
        self.m.hook_log.clear()
        self.m.gte_reset()

        def point_in_box_xz(cpu, regs):
            pos, box = regs[4], regs[5]
            px, pz = self.m.reads32(pos), self.m.reads32(pos + 8)
            cx, hx = self.m.reads16(box), self.m.reads16(box + 6)
            cz, hz = self.m.reads16(box + 4), self.m.reads16(box + 10)
            return int(cx - hx <= px <= cx + hx and cz - hz <= pz <= cz + hz)
        regs = self.m.call(FUNCS["CollProbe_Trench"][0], (COLLIDER, PROBE, OUTPUT),
                           hooks={HOOK_POINT_IN_BOX_XZ: point_in_box_xz})
        c_ret = self.c.lib.ae_CollProbe_Trench(
            self.c.ram, C.byref(self.c.hooks), COLLIDER, PROBE, OUTPUT)
        self.compare("CollProbe_Trench", index, regs, c_ret)

    def run_triangle(self, index, orientation, x, y, z, radius=64,
                     body_height=128, half_x=256, half_y=160, half_z=256,
                     previous=None):
        self.m.restore(self.base)
        rng = self.rng
        shape = bytearray(rng.getrandbits(8) for _ in range(0x38))
        for off, value in ((0, 0), (2, 0), (4, 0), (6, half_x),
                           (8, half_y), (10, half_z)):
            shape[off:off + 2] = (value & 0xFFFF).to_bytes(2, "little")
        self.m.write_bytes(COLLIDER, bytes(shape))
        self.m.write32(CURRENT_POSITION, POSITION)
        self.m.write32(CURRENT_POSITION + 4, POSITION + 0x100)
        for off, value in ((0, x), (4, y), (8, z)):
            self.m.write32(POSITION + off, value)
        if previous is None:
            previous = (x, y, z)
        for off, value in zip((0, 4, 8), previous):
            self.m.write32(POSITION + 0x100 + off, value)
        probe = bytearray(rng.getrandbits(8) for _ in range(0x10))
        probe[0:4] = (radius & M32).to_bytes(4, "little")
        probe[4:8] = ((0x8000 | orientation) & M32).to_bytes(4, "little")
        probe[8:12] = (body_height & M32).to_bytes(4, "little")
        self.m.write_bytes(PROBE, bytes(probe))
        self.m.write_bytes(OUTPUT, bytes(rng.getrandbits(8) for _ in range(0x20)))
        C.memmove(self.c.main, C.addressof(self.m_main), 0x200000)
        C.memmove(self.c.scratch, C.addressof(self.m_scratch), 0x400)
        self.c.hook_calls.clear()
        self.m.hook_log.clear()
        self.m.gte_reset()

        def point_in_box_xz(cpu, regs):
            pos, box = regs[4], regs[5]
            px, pz = self.m.reads32(pos), self.m.reads32(pos + 8)
            cx, hx = self.m.reads16(box), self.m.reads16(box + 6)
            cz, hz = self.m.reads16(box + 4), self.m.reads16(box + 10)
            return int(cx - hx <= px <= cx + hx and cz - hz <= pz <= cz + hz)
        def dir_index_from_flags(cpu, regs):
            return (regs[4] & 0x0C00) >> 10
        regs = self.m.call(FUNCS["CollProbe_Triangle"][0], (COLLIDER, PROBE, OUTPUT),
                           hooks={HOOK_POINT_IN_BOX_XZ: point_in_box_xz,
                                  HOOK_DIR_INDEX_FROM_FLAGS: dir_index_from_flags})
        c_ret = self.c.lib.ae_CollProbe_Triangle(
            self.c.ram, C.byref(self.c.hooks), COLLIDER, PROBE, OUTPUT)
        self.compare("CollProbe_Triangle", index, regs, c_ret)

    def run_rope(self, index, ends, x, y, z, body_height=128,
                 player_mode=False, attach_mode=False):
        self.m.restore(self.base)
        rng = self.rng
        shape = bytearray(rng.getrandbits(8) for _ in range(0x38))
        for off, value in zip((0, 2, 4, 36, 38, 40), ends):
            shape[off:off + 2] = (value & 0xFFFF).to_bytes(2, "little")
        self.m.write_bytes(COLLIDER, bytes(shape))
        self.m.write32(CURRENT_POSITION, POSITION)
        self.m.write32(CURRENT_POSITION + 4, POSITION + 0x100)
        for off, value in ((0, x), (4, y), (8, z)):
            self.m.write32(POSITION + off, value)
            self.m.write32(POSITION + 0x100 + off, value)
        probe = bytearray(rng.getrandbits(8) for _ in range(0x10))
        probe[0:4] = (64).to_bytes(4, "little")
        probe[4] = 82
        probe[8:12] = (body_height & M32).to_bytes(4, "little")
        self.m.write_bytes(PROBE, bytes(probe))
        self.m.write16(AE_COLLPROBE_BODY_CUR_TYPE, 1 if player_mode else 0)
        self.m.write16(AE_COLLPROBE_ATTACH_KIND, 6 if attach_mode else 0)
        self.m.write16(AE_COLLPROBE_PLAYER_ACTION_A, 3)
        self.m.write16(AE_COLLPROBE_ROPE_ACTION, 3 if player_mode else 4)
        self.m.write16(AE_COLLPROBE_PLAYER_ACTION_B, 4)
        self.m.write_bytes(OUTPUT, bytes(rng.getrandbits(8) for _ in range(0x20)))
        C.memmove(self.c.main, C.addressof(self.m_main), 0x200000)
        C.memmove(self.c.scratch, C.addressof(self.m_scratch), 0x400)
        self.c.hook_calls.clear()
        self.m.hook_log.clear()
        regs = self.m.call(FUNCS["CollProbe_Rope"][0], (COLLIDER, PROBE, OUTPUT),
                           hooks={HOOK_PLAYER_SET_ROPE: lambda cpu, r: 0})
        c_ret = self.c.lib.ae_CollProbe_Rope(
            self.c.ram, C.byref(self.c.hooks), COLLIDER, PROBE, OUTPUT)
        self.compare("CollProbe_Rope", index, regs, c_ret)

    def run_radial(self, index, geometry, x, y, z, radius=64,
                   body_height=128, previous=None, half_y=160):
        self.m.restore(self.base)
        rng = self.rng
        shape = bytearray(rng.getrandbits(8) for _ in range(0x38))
        half_x = 1024 if geometry == 54 and index // 18 == 2 else 128
        center_x = 256 if geometry in (70, 71) else (-256 if geometry in (72, 73) else 0)
        center_z = 256 if geometry in (70, 73) else (-256 if geometry in (71, 72) else 0)
        for off, value in ((0, center_x), (2, 0), (4, center_z),
                           (6, half_x), (8, half_y), (10, 128),
                           (0x10, geometry),
                           (0x24, 48), (0x26, -48), (0x28, 48)):
            shape[off:off + 2] = (value & 0xFFFF).to_bytes(2, "little")
        self.m.write_bytes(COLLIDER, bytes(shape))
        self.m.write32(CURRENT_POSITION, POSITION)
        self.m.write32(CURRENT_POSITION + 4, POSITION + 0x100)
        for off, value in ((0, x), (4, y), (8, z)):
            self.m.write32(POSITION + off, value)
        if previous is None:
            previous = (x, y, z)
        for off, value in zip((0, 4, 8), previous):
            self.m.write32(POSITION + 0x100 + off, value)
        probe = bytearray(rng.getrandbits(8) for _ in range(0x10))
        probe[0:4] = (radius & M32).to_bytes(4, "little")
        probe[4] = geometry
        probe[8:12] = (body_height & M32).to_bytes(4, "little")
        self.m.write_bytes(PROBE, bytes(probe))
        group = index // 18
        body_mode = geometry in (59, 89)
        attach_mode = geometry in (59, 89) and group == 0
        self.m.write16(AE_COLLPROBE_BODY_CUR_TYPE, 1 if body_mode else 0)
        self.m.write16(AE_COLLPROBE_ATTACH_KIND, 5 if attach_mode else 0)
        self.m.write16(AE_COLLPROBE_PLAYER_ACTION_A, 3)
        self.m.write16(AE_COLLPROBE_PLAYER_ACTION_B, 3 if body_mode else 4)
        self.m.write_bytes(OUTPUT, bytes(rng.getrandbits(8) for _ in range(0x20)))
        C.memmove(self.c.main, C.addressof(self.m_main), 0x200000)
        C.memmove(self.c.scratch, C.addressof(self.m_scratch), 0x400)
        self.c.hook_calls.clear()
        self.m.hook_log.clear()
        self.m.gte_reset()

        def point_in_box_xz(cpu, regs):
            pos, box = regs[4], regs[5]
            px, pz = self.m.reads32(pos), self.m.reads32(pos + 8)
            cx, hx = self.m.reads16(box), self.m.reads16(box + 6)
            cz, hz = self.m.reads16(box + 4), self.m.reads16(box + 10)
            return int(cx - hx <= px <= cx + hx and cz - hz <= pz <= cz + hz)
        def climb(cpu, r):
            cpu.hook_log[-1] += (cpu.read32(r[29] + 16), cpu.read32(r[29] + 20))
            return 0
        regs = self.m.call(FUNCS["CollProbe_Radial"][0], (COLLIDER, PROBE, OUTPUT),
                           hooks={HOOK_POINT_IN_BOX_XZ: point_in_box_xz,
                                  HOOK_PLAYER_SET_CLIMB_TARGET: climb,
                                  HOOK_PLAYER_SET_ROPE: lambda cpu, r: 0})
        c_ret = self.c.lib.ae_CollProbe_Radial(
            self.c.ram, C.byref(self.c.hooks), COLLIDER, PROBE, OUTPUT)
        if half_y != 160:
            expected = (HOOK_PLAYER_SET_CLIMB_TARGET, center_x & M32, 0,
                        center_z & M32, half_x, half_y, int(geometry == 59))
            if expected not in self.m.hook_log:
                raise AssertionError(f"climb scenario did not reach original call: {self.m.hook_log}")
        self.compare("CollProbe_Radial", index, regs, c_ret)

    def run_cases(self):
        edge_words = [0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xA5A55A5A, 0x12345678, 0x87654321]
        for i, word in enumerate(edge_words):
            if self.only is None or "CollProbe_NoContact" in self.only:
                self.run_no_contact(i, word)
        for i in range(max(0, self.cases - len(edge_words))):
            if self.only is None or "CollProbe_NoContact" in self.only:
                self.run_no_contact(i + len(edge_words))

        strip_cases = []
        for geometry in (0x4A, 0x4B, 0x4C, 0x4D):
            for offset in (-20, 0, 20):
                for height in (60, 128):
                    if geometry in (0x4A, 0x4C):
                        strip_cases.append((geometry, 0, height, offset, 64, 240, 256, 128, 64, 0, height - 32))
                    else:
                        strip_cases.append((geometry, offset, height, 0, 64, 240, 256, 128, 64, 90, height - 32))
        while len(strip_cases) < self.cases:
            geometry = self.rng.choice((0x4A, 0x4B, 0x4C, 0x4D))
            x = self.rng.randint(-320, 320)
            z = self.rng.randint(-320, 320)
            y = self.rng.randint(-256, 256)
            range_top = self.rng.randint(-256, 256)
            direction = self.rng.choice((0, 90, 45, -1))
            strip_cases.append((geometry, x, y, z, self.rng.randint(-16, 512), self.rng.randint(-32, 600),
                                self.rng.randint(1, 512), self.rng.randint(-256, 512),
                                self.rng.randint(-128, 128), direction, range_top))
        for i, case in enumerate(strip_cases[:self.cases]):
            if self.only is None or "CollProbe_Strip" in self.only:
                self.run_strip(i, *case)

        coordinates = (-320, -280, -256, -192, -64, 0, 64, 192, 256, 280, 320)
        orientations = (0x000, 0x400, 0x800, 0xC00, 0x100, 0xD00)
        for name in ("CollProbe_ThinWall", "CollProbe_EdgeWall"):
            if self.only is not None and name not in self.only:
                continue
            materials = (0x1000, 0x5000) if name == "CollProbe_ThinWall" else (0x6000, 0x7000)
            cases = []
            for material in materials:
                for orientation in orientations:
                    for n in range(8):
                        x = coordinates[(n + orientation // 0x400) % len(coordinates)]
                        z = coordinates[(n * 3 + orientation // 0x400) % len(coordinates)]
                        cases.append((material, orientation, x, z, 64 + n, 256, 128,
                                      n % 3 + 1, (-256, 0, 256)[n % 3], (256, 0, -256)[n % 3]))
            if name == "CollProbe_ThinWall":
                cases.append((0x5000, 0x800, -320, 0, 64, 256, 128, 1, 0, 0))
            else:
                cases.append((0x6000, 0x100, 0, 0, 64, 256, 128, 3, 0, 0))
            while len(cases) < self.cases:
                material = self.rng.choice(materials)
                orientation = self.rng.choice(orientations)
                cases.append((material, orientation, self.rng.choice(coordinates), self.rng.choice(coordinates),
                              self.rng.randint(1, 300), self.rng.randint(32, 384), self.rng.randint(32, 192),
                              self.rng.randint(1, 3), self.rng.choice((-256, 0, 256)),
                              self.rng.choice((-256, 0, 256))))
            for i, case in enumerate(cases[:self.cases]):
                self.run_wall_band(name, i, *case)

        # The direct HalfPipeRangeTest port rejects these separated vertical
        # ranges before the later box response and returns no contact.
        if self.only is None or "CollProbe_Box" in self.only:
            self.run_box("above-top", 59, 0, 0, 0, 0, 32,
                         player_y=242, height=241, range_reject_pc=0x80073F48)
            self.run_box("below-bottom", 59, 0, 0, 0, 0, 32,
                         player_y=-129, height=240, range_reject_pc=0x80073F60)

        box_cases = []
        box_x = (-144, -128, -100, -64, 0, 64, 100, 128, 144)
        box_z = (-80, -64, -32, 0, 32, 64, 80)
        for geometry in (60, 61, 62, 63):
            for n in range(24):
                x = box_x[(n * 3 + n // 7) % len(box_x)]
                z = box_z[(n * 5 + n // 7) % len(box_z)]
                old_x, old_z = (x, z) if n % 2 == 0 else (300, -300)
                box_cases.append((geometry, x, z, old_x, old_z, 32 + n % 65))
        box_cases.extend([
            (59, 0, 0, 0, 0, 32), (64, 0, 0, 0, 0, 32),
            (60, -144, -80, -144, -80, 32, 0, 64),
            (59, 0, 0, 0, 0, 32),
        ])
        for i, case in enumerate(box_cases[:self.cases]):
            if self.only is None or "CollProbe_Box" in self.only:
                self.run_box(i, *case)

        if self.only is None or "CollProbe_DiagonalWall" in self.only:
            diagonal_coords = (-320, -256, -192, -64, 0, 64, 192, 256, 320)
            diagonal_cases = []
            for orientation in (0x000, 0x400, 0x800, 0xC00):
                for n in range(22):
                    x = diagonal_coords[n % len(diagonal_coords)]
                    z = diagonal_coords[(n * 3 + orientation // 0x400) % len(diagonal_coords)]
                    diagonal_cases.append((orientation, x, z))
            diagonal_cases.extend([
                (0x000, 64, -32), (0x000, 64, -96),
                (0x400, 64, 32), (0x400, 64, 96),
                (0x800, 64, -96), (0x800, 64, -32),
                (0xC00, 64, 96), (0xC00, 64, 32),
                (0x100, 0, 0), (0xD00, 64, 0),
                (0x000, 320, 320), (0x400, -320, 320),
            ])
            while len(diagonal_cases) < self.cases:
                diagonal_cases.append((self.rng.choice((0x000, 0x400, 0x800, 0xC00)),
                                       self.rng.choice(diagonal_coords), self.rng.choice(diagonal_coords)))
            for i, case in enumerate(diagonal_cases[:self.cases]):
                self.run_diagonal_wall(i, *case)

        if self.only is None or "CollProbe_HalfPipe" in self.only:
            halfpipe_cases = []
            for geometry in (36, 37, 83, 84):
                for n, offset in enumerate((-256, -192, -128, -64, 0, 64, 128, 192, 256)):
                    x = offset if geometry in (37, 84) else (n * 31) % 220 - 110
                    z = offset if geometry in (36, 83) else (n * 29) % 180 - 90
                    halfpipe_cases.append((geometry, x, 130 + n * 11, z, 48 + n, 128))
            for geometry in (83, 84):
                for x_sign in (-1, 1):
                    for z_sign in (-1, 1):
                        halfpipe_cases.append((geometry, x_sign * 300, 180,
                                               z_sign * 240, 64, 128))
            while len(halfpipe_cases) < self.cases:
                halfpipe_cases.append((self.rng.choice((36, 37, 83, 84)),
                                       self.rng.randint(-320, 320), self.rng.randint(-128, 512),
                                       self.rng.randint(-320, 320), self.rng.randint(1, 256),
                                       self.rng.randint(32, 300)))
            for i, case in enumerate(halfpipe_cases[:self.cases]):
                self.run_halfpipe(i, *case)

        if self.only is None or "CollProbe_Slope" in self.only:
            slope_cases = []
            for geometry in (70, 71, 72, 73):
                cx = 256 if geometry in (70, 71) else -256
                cz = 256 if geometry in (70, 73) else -256
                for n in range(23):
                    angle = n * 6.283185307179586 / 23
                    x = cx - int(300 * __import__("math").cos(angle))
                    z = cz - int(300 * __import__("math").sin(angle))
                    y = 180 + n % 5 * 8
                    radius = 32 + n % 48
                    low_height = 64
                    corner_x = 256 if geometry in (70, 71) else -256
                    corner_z = 256 if geometry in (70, 73) else -256
                    if n == 0:
                        y = -100
                    elif n == 21:
                        x, z = corner_x - (300 if geometry in (70, 71) else -300), corner_z
                        radius = 64
                        low_height = 240
                    elif n == 22:
                        x, z = corner_x, corner_z
                        radius = 128
                    elif n == 20:
                        x = corner_x - (550 if geometry in (70, 71) else -550)
                        z = corner_z
                        y = 180
                        radius = 64
                    elif n == 19:
                        x = corner_x
                        z = corner_z - (550 if geometry in (70, 73) else -550)
                        y = 180
                        radius = 64
                    elif geometry == 72 and n == 18:
                        x, z, y, radius = -192, -256, 180, 64
                    slope_cases.append((geometry, x, y, z, radius, 128,
                                        256, 200, 256, 64, low_height))
                slope_cases.extend([
                    (geometry, cx - (550 if geometry in (70, 71) else -550), 180, cz, 64, 128),
                    (geometry, cx - (100 if geometry in (70, 71) else -100), 180, cz, 64, 128),
                ])
            for i, case in enumerate(slope_cases[:self.cases]):
                self.run_slope(i, *case)

        if self.only is None or "CollProbe_Ramp" in self.only:
            ramp_cases = []
            for type_word in (7, 8, 9, 10, 32, 33, 34, 35,
                              64, 65, 66, 67, 0x0400 | 7, 0x0800 | 7,
                              0x0C00 | 7, 0x0100 | 7):
                for n, (x, z) in enumerate(((-280, -210), (-256, 0), (-128, 128),
                                             (0, 0), (128, -128), (256, 0), (280, 210))):
                    y = 180 + n * 16 + n % 2
                    if type_word == 7 and n == 0:
                        y = 181
                    if type_word == 7 and n == 5:
                        x, z = 256, 192
                    if type_word == 0x0407 and n == 1:
                        x, z = 256, 0
                    elif type_word == 0x0807 and n == 1:
                        x, z = 0, 256
                    elif type_word == 0x0C07 and n == 1:
                        x, z = -128, 96
                    ramp_cases.append((type_word, x, y, z, 64, 128))
            while len(ramp_cases) < self.cases:
                ramp_cases.append((self.rng.choice((7, 8, 9, 10, 64, 65, 66, 67,
                                                     0x0407, 0x0807, 0x0C07)),
                                   self.rng.randint(-320, 320), self.rng.randint(80, 320),
                                   self.rng.randint(-240, 240), self.rng.randint(16, 128), 128))
            for i, case in enumerate(ramp_cases[:self.cases]):
                self.run_ramp(i, *case)

        if self.only is None or "CollProbe_Trench" in self.only:
            trench_cases = []
            for geometry in (83, 84):
                for n in range(50):
                    along = (-500, -384, -256, -128, -64, 0, 64, 128, 256, 384, 500)[n % 11]
                    cross = (-400, -300, -128, 0, 128, 300, 400)[(n * 3) % 7]
                    if geometry == 83:
                        x, z = cross, along
                    else:
                        x, z = along, cross
                    y = (-160, -64, 0, 64, 128, 192, 320)[(n * 5) % 7]
                    trench_cases.append((geometry, x, y, z, 64 + n % 96,
                                         64 + n % 160))
            for i, case in enumerate(trench_cases[:self.cases]):
                self.run_trench(i, *case)

        if self.only is None or "CollProbe_Triangle" in self.only:
            triangle_cases = []
            for orientation in (0x000, 0x400, 0x800, 0xC00):
                for n in range(25):
                    coords = (-256, -192, -128, -64, 0, 64, 128, 192, 256)
                    x = coords[n % len(coords)]
                    z = coords[(n * 3 + orientation // 0x400) % len(coords)]
                    y = 65 + n % 7
                    if n == 0:
                        x, z = 0, 0
                    elif orientation == 0 and n == 1:
                        x, z, y = 64, 0, 65
                    elif orientation == 0 and n == 2:
                        x, z = -136, 0
                    elif n == 24:
                        y = 241
                    triangle_cases.append((orientation, x, y, z, 64, 128))
            for i, case in enumerate(triangle_cases[:self.cases]):
                self.run_triangle(i, *case)

        if self.only is None or "CollProbe_Radial" in self.only:
            radial_types = (36, 37, 38, 54, 59, 68, 70, 71, 72, 73, 78, 79,
                            80, 81, 82, 83, 84, 89)
            radial_cases = []
            for i in range(self.cases):
                geometry = radial_types[i % len(radial_types)]
                x = (i % 9 - 4) * 48
                z = ((i * 5) % 9 - 4) * 48
                group = i // len(radial_types)
                y = (i % 7 - 3) * 48 + group % 2
                radius = 64 + i % 96
                if geometry in (38, 54, 59, 89) and group == 1:
                    x, z = 0, 0
                if geometry == 38 and group == 1:
                    y = -47
                if geometry in (38, 59, 89) and group in (1, 2, 3):
                    radius = 5000
                if geometry == 38 and group == 2:
                    radius = 3968
                    y = 1
                if geometry == 54 and group == 2:
                    x, y, z = 1024, 48, 0
                radial_cases.append((geometry, x, y, z, radius, 128))
            for i, case in enumerate(radial_cases):
                self.run_radial(i, *case)
            for index, (kind, half_y) in enumerate(((59, 173), (89, 219)), self.cases + 18):
                self.run_radial(index, kind, 0, 0, 0, radius=5000, half_y=half_y)

        if self.only is None or "CollProbe_Rope" in self.only:
            rope_cases = []
            for i in range(self.cases):
                body_mode = i >= 5
                attach_mode = 5 <= i < 10
                form = (i - 5) % 4
                if form == 0:
                    ends = (0, -64, -192, 0, 64, 192)
                    x, z = ((i % 5) - 2) * 24, ((i % 7) - 3) * 48
                elif form == 1:
                    ends = (-192, -64, 0, 192, 64, 0)
                    x, z = ((i % 7) - 3) * 48, ((i % 5) - 2) * 24
                elif form == 2:
                    ends = (-192, -64, -192, 192, 64, 192)
                    x, z = ((i % 5) - 2) * 32, ((i % 5) - 2) * 32
                else:
                    ends = (-192, -64, 192, 192, 64, -192)
                    x, z = ((i % 5) - 2) * 32, (2 - (i % 5)) * 32
                mid_y = (ends[1] + ends[4]) // 2
                if i >= 10 and (i // 4) % 2 == 0:
                    ends = (ends[3], ends[4], ends[5], ends[0], ends[1], ends[2])
                    mid_y = (ends[1] + ends[4]) // 2
                if i >= 10 and i % 5 == 0:
                    if form == 0:
                        x, z = 0, 256
                    elif form == 1:
                        x, z = 256, 0
                    else:
                        x, z = 256, 256 if form == 2 else -256
                if i >= 10 and form == 1 and i % 5 == 1:
                    x, z = 0, 80
                player_y = mid_y + 64
                if i >= 10 and i % 7 == 0:
                    player_y += 129
                rope_cases.append((ends, x, player_y, z, 128,
                                   body_mode, attach_mode))
            if self.cases >= 100:
                rope_cases.append(((-192, -64, -192, 192, 64, 192), 400, 64, 0,
                                   128, True, False))
                rope_cases.append(((-192, -64, 0, 192, 64, 0), 0, 64, -48,
                                   128, True, False))
            for i, case in enumerate(rope_cases):
                self.run_rope(i, *case)

    def summary(self, default_run=False, show_coverage=False):
        result = reg.coverage_summary([(name, spec[0], spec[1], True) for name, spec in FUNCS.items()], self.m.coverage)
        if show_coverage:
            reg.print_coverage(result)
        if default_run:
            reg.write_results(self.counts, result, not self.failures, function_names=list(FUNCS))
        return result


def mutations(cases, seed):
    original = (ROOT / "src/collision/ae_collprobe.c").read_text(encoding="utf-8")
    variants = [
        ("climb fifth word dropped", "(uint32_t)half_y, type == 59 ? 1u : 0u);", "0u, type == 59 ? 1u : 0u);", "CollProbe_Radial"),
        ("climb sixth word dropped", "(uint32_t)half_y, type == 59 ? 1u : 0u);", "(uint32_t)half_y, 0u);", "CollProbe_Radial"),
        ("old four-word climb behavior", "(uint32_t)half_y, type == 59 ? 1u : 0u);", "0u, 0u);", "CollProbe_Radial"),
        ("NoContact return", "h->printf(h->user, r, AE_COLLPROBE_NO_CONTACT_FORMAT, probe_type_word, out, 0);\n    return 0;\n}",
         "h->printf(h->user, r, AE_COLLPROBE_NO_CONTACT_FORMAT, probe_type_word, out, 0);\n    return 1;\n}", "CollProbe_NoContact"),
        ("NoContact logged word", "uint32_t probe_type_word = ae_rd32(r, probe + AE_COLLPROBE_INPUT_TYPE);",
         "uint32_t probe_type_word = ae_rd32(r, probe + AE_COLLPROBE_INPUT_TYPE + 4u);", "CollProbe_NoContact"),
        ("Strip status", "response = 2;", "response = 1;", "CollProbe_Strip"),
        ("Strip radius margin", "int32_t radius_with_margin = add32(ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS), 12);",
         "int32_t radius_with_margin = add32(ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS), 13);", "CollProbe_Strip"),
        ("ThinWall diagonal selection", "case 0x000u: return dx < dz ? 1 : 2;",
         "case 0x000u: return dx < dz ? 2 : 1;", "CollProbe_ThinWall"),
        ("ThinWall radius margin", "int32_t radius = add32(ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS), 12);",
         "int32_t radius = add32(ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS), 13);", "CollProbe_ThinWall"),
        ("EdgeWall orientation center", "if (orientation == 0x000u) center_z = -256;",
         "if (orientation == 0x000u) center_z = -255;", "CollProbe_EdgeWall"),
        ("EdgeWall response status", "(uint32_t)sub32(add32(center_z, edge), dz));\n        return 2;",
         "(uint32_t)sub32(add32(center_z, edge), dz));\n        return 1;", "CollProbe_EdgeWall"),
        ("Box shifted center", "ae_rd16(r, shape + AE_COL_Z) - ae_rd16(r, shape + AE_COL_EXTENT_Z)",
         "ae_rd16(r, shape + AE_COL_Z) + ae_rd16(r, shape + AE_COL_EXTENT_Z)", "CollProbe_Box"),
        ("Box zero extent repair", "ae_wr16(r, shape + AE_COL_EXTENT_X, 1);",
         "ae_wr16(r, shape + AE_COL_EXTENT_X, 2);", "CollProbe_Box"),
        ("DiagonalWall side direction", "int32_t side_adjustment = distance < 0 ? 1 : -1;",
         "int32_t side_adjustment = distance < 0 ? -1 : 1;", "CollProbe_DiagonalWall"),
        ("DiagonalWall invalid orientation", "default: return 1;\n    }\n    int32_t side_adjustment",
         "default: return 0;\n    }\n    int32_t side_adjustment", "CollProbe_DiagonalWall"),
        ("HalfPipe axis bound", "if (check_axis_extent && abs32_wrapped(along) > arc_radius) return 0;",
         "if (check_axis_extent && abs32_wrapped(along) >= arc_radius) return 0;", "CollProbe_HalfPipe"),
        ("HalfPipe vertical response", "AE_COLLPROBE_OUTPUT_SLOPE_Z, (uint32_t)arc_height);",
         "AE_COLLPROBE_OUTPUT_SLOPE_Z, (uint32_t)(arc_height + 1));", "CollProbe_HalfPipe"),
        ("NoContact format address", "AE_COLLPROBE_NO_CONTACT_FORMAT, probe_type_word, out, 0)",
         "AE_COLLPROBE_NO_CONTACT_FORMAT + 4u, probe_type_word, out, 0)", "CollProbe_NoContact"),
        ("NoContact output pointer", "probe_type_word, out, 0);", "probe_type_word, out + 4u, 0);", "CollProbe_NoContact"),
        ("Strip geometry selection", "if (geometry == AE_COLLPROBE_STRIP_Z_A || geometry == AE_COLLPROBE_STRIP_Z_B)",
         "if (geometry == AE_COLLPROBE_STRIP_Z_A)", "CollProbe_Strip"),
        ("Strip side direction", "offset_z > 0", "offset_z < 0", "CollProbe_Strip"),
        ("ThinWall dispatch mode", "return wall_band_response(r, h, shape, probe, out, 0);",
         "return wall_band_response(r, h, shape, probe, out, 1);", "CollProbe_ThinWall"),
        ("ThinWall centered edge", "int32_t edge = dx <= center_x ? -radius : radius;",
         "int32_t edge = dx <= center_x ? radius : -radius;", "CollProbe_ThinWall"),
        ("EdgeWall dispatch mode", "return wall_band_response(r, h, shape, probe, out, 1);",
         "return wall_band_response(r, h, shape, probe, out, 0);", "CollProbe_EdgeWall"),
        ("EdgeWall centered edge", "int32_t edge = dz <= center_z ? -radius : radius;",
         "int32_t edge = dz < center_z ? -radius : radius;", "CollProbe_EdgeWall"),
        ("Box diagonal sector", "if (sector == 0 || sector == 3) {",
         "if (sector == 0 || sector == 2) {", "CollProbe_Box"),
        ("Box vertical reject return", "if (sub32(player_y, body_height) >= shape_top || shape_bottom >= player_y) return 1;",
         "if (sub32(player_y, body_height) >= shape_top || shape_bottom >= player_y) return 0;", "CollProbe_Box"),
        ("Box point inclusion", "if (point_in_box_xz(r, h, position, shape) == 0) return 1;",
         "if (point_in_box_xz(r, h, position, shape) != 0) return 1;", "CollProbe_Box"),
        ("DiagonalWall side magnitude", "int32_t side_adjustment = distance < 0 ? 1 : -1;",
         "int32_t side_adjustment = distance < 0 ? 1 : -2;", "CollProbe_DiagonalWall"),
        ("DiagonalWall C00 response", "case 0xC00u: sign_x = 1; sign_z = -1; break;",
         "case 0xC00u: sign_x = 1; sign_z = 1; break;", "CollProbe_DiagonalWall"),
        ("HalfPipe expanded X", "int32_t expanded_x = half_x_unsigned + radius;",
         "int32_t expanded_x = half_x_unsigned + radius + 1;", "CollProbe_HalfPipe"),
        ("Radial ring width", "int32_t width = divide1000_magic(mul32(half_x, 75));",
         "int32_t width = divide1000_magic(mul32(half_x, 76));", "CollProbe_Radial"),
        ("Radial result status", "(type >= 78 && type <= 81)) ? 2 : 1;",
         "(type >= 78 && type <= 81)) ? 1 : 2;", "CollProbe_Radial"),
        ("Slope X corner tie", "if (corner_x >= px) offset_x = add32(offset_x, 1);",
         "if (corner_x > px) offset_x = add32(offset_x, 1);", "CollProbe_Slope"),
        ("Slope Z corner tie", "if (corner_z >= pz) offset_z = add32(offset_z, 1);",
         "if (corner_z > pz) offset_z = add32(offset_z, 1);", "CollProbe_Slope"),
        ("Ramp X boundary", "if (player_x >= shape_right)", "if (player_x > shape_right)", "CollProbe_Ramp"),
        ("Ramp Z boundary", "if (player_z >= shape_front)", "if (player_z > shape_front)", "CollProbe_Ramp"),
        ("Trench type dispatch", "if (type == 83 || type == 84) {\n        int along_z = type == 83;",
         "if (type == 83) {\n        int along_z = type == 83;", "CollProbe_Trench"),
        ("Trench axis selection", "int along_z = type == 83;", "int along_z = type != 83;", "CollProbe_Trench"),
        ("Triangle distance boundary", "if (add32(radius, 32) < distance) return 0;",
         "if (add32(radius, 32) <= distance) return 0;", "CollProbe_Triangle"),
        ("Triangle zero distance corner", "if (corner_sector || distance == 0) sector = 4;",
         "if (corner_sector || distance < 0) sector = 4;", "CollProbe_Triangle"),
        ("Triangle zero response divisor", "if (distance > 0) {", "if (distance >= 0) {", "CollProbe_Triangle"),
        ("Rope axis dispatch", "if (x1 == x2) {", "if (x1 != x2) {", "CollProbe_Rope"),
        ("Rope contact gate", "if (contact) rope_attach(r, h, shape, 0);",
         "if (!contact) rope_attach(r, h, shape, 0);", "CollProbe_Rope"),
    ]
    failures = []
    for index, (label, before, after, target) in enumerate(variants):
        if before not in original:
            raise SystemExit(f"mutation source pattern not found: {label}")
        changed = original.replace(before, after, 1)
        path = OUT_DIR / f"collprobe_mutation_{index}.c"
        path.write_text(changed, encoding="utf-8")
        h = Harness(path, cases=max(cases, 100), seed=seed, functions={target})
        h.run_cases()
        caught = bool(h.failures)
        print(f"mutation {label}: {'caught' if caught else 'SURVIVED'}", flush=True)
        if not caught:
            failures.append(label)
    print(f"mutations: {len(variants)}; survivors: {len(failures)}", flush=True)
    return failures


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", type=int, default=100)
    ap.add_argument("--seed", type=int, default=0x94423)
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--mutate", action="store_true")
    args = ap.parse_args()
    if not (ROOT / "extract/disc/SCUS_944.23").exists():
        print("SKIP: extract/disc/SCUS_944.23 not present")
        return 0
    if args.mutate:
        failures = mutations(args.cases, args.seed)
        return 1 if failures else 0
    started = time.time()
    h = Harness(cases=args.cases, seed=args.seed)
    h.run_cases()
    result = h.summary(default_run=(args.cases == 100 and args.seed == 0x94423), show_coverage=args.coverage)
    for name, count in h.counts.items():
        print(f"{name}: {count} cases")
    print(f"elapsed: {time.time() - started:.2f}s; failures: {len(h.failures)}")
    for name, index, issues in h.failures[:8]:
        print(f"{name} case {index}: " + "; ".join(issues))
    gaps = [name for name, entry in result.items() if entry["uncovered_count"]]
    if gaps:
        print("coverage gaps: " + ", ".join(gaps))
    return 1 if h.failures or gaps else 0


if __name__ == "__main__":
    sys.exit(main())
