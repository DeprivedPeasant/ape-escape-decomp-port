"""Instruction differential check for the 10 game entry-spawn functions.

Run with .venv/Scripts/python.exe tools/mips/check_player_spawn.py [--coverage] [--mutate].
The oracle is the extracted EXE loaded beside real area members at 0x80154000.
"""
from __future__ import annotations

import argparse
import ctypes as C
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/mips"), str(ROOT / "tools")]
import dump_areas  # noqa: E402
import mips_ram as mr  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import check_sound  # noqa: E402

M32 = 0xFFFFFFFF
STACK_LO = 0x801FF000
AREA_BASE = 0x80154000
SLOT_POOL = 0x801D0000
ENTRY_OUT = 0x801E0000
CAMERA_STATE = 0x800E68D0
STAGE_STATE = 0x800F4350
PLAYER = 0x800EC1E0
COLLISION_SLOTS = 0x800BBDA0
CAMERA_SLOT_GLOBAL = 0x800B4D40

FUNCS = {
    "AreaGetEntryPoint": (0x80010C3C, 104, "s", "s u u", False),
    "PlayerSpawnAtEntry": (0x80032BBC, 380, "s", "", True),
    "SetProjection": (0x80046D80, 44, "v", "s", True),
    "CameraActivateSlot": (0x80046DAC, 60, "v", "u", False),
    "CameraResetAll": (0x80047754, 1148, "v", "", True),
    "CameraShakeReset": (0x800489EC, 20, "v", "", False),
    "AimModeLeave": (0x8004B510, 60, "v", "", True),
    "EventCamSetScript": (0x8004BAD8, 144, "v", "u s u u", False),
    "StageExitFlagsClear": (0x8004D0F0, 20, "v", "", False),
    "PlaySfx": (0x800877F8, 232, "s", "s", False),
}
KIND_NAMES = {
    "GetSolidHeight": 0x8004D8C0,
    "Atan2Safe": 0x80046340,
    "AreaBumpAlloc": 0x8008C1B8,
    "CameraLookAt": 0x80046670,
    "printf": 0x800A3720,
    "SpuWaitIdle": 0x800A1398,
    "SpuWaitLoop": 0x8009D79C,
    "SoundLoopStart": 0x800878E0,
    "SoundLoopStop": 0x8008806C,
}
EXTRA_COVERAGE = {}
HOOKFN = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                     C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
WORLD_FIELDS = ("GetSolidHeight", "PlaySfxAt", "GimmickActivate", "MeshInitFromAreaTable",
                "FUN_8013d358", "CameraShake", "EventCamStart")
BODY_FIELDS = ("printf_error", "sound_loop_start", "sound_loop_stop", "body_move",
               "build_probe_points", "body_stub_return_zero", "collision_ray_test",
               "gimmick_type_to_sfx", "play_sfx_at", "camera_nudge")
CAMERA_HOOKS = [
    "Atan2Safe", "VectorNormalSS", "SquareRoot0", "GetGroundHeight", "rsin", "rcos",
    "RotTransPers", "SetRotMatrix", "SetTransMatrix", "memset", "PlayerCanExitStage",
    "PlayerIsSubmerged", "CameraRcCarActive", "CameraRcCarFollow", "IsFlightArea",
    "FUN_8013d000", "BossUpdate", "CameraAreaScriptDispatch", "CallIndexedHandler",
    "RotMatrixY", "ApplyMatrixSV", "MatrixIdentity", "CameraGetPlayerDir",
    "AimRequestStart", "AimRequestClear", "AimModeEnter", "AimModeLeave",
    "CameraFixedModeOff", "StageExitPending", "BossAimSfx", "BossKindForArea",
    "ScreenFadeSet", "PlaySfx", "HudDrawMonkeyInfo", "RadarUsable", "MonkeyFlag248Set",
    "CameraRadarFocusUpdate", "CameraRadarFocusSelect",
]


def prepare_gte_build_output():
    """Stage the pinned GTE inputs beside local build artifacts.

    The shared dependency cache can be a junction outside a writable worktree.
    gte_oracle.build() writes its manifest and wrapper beside the inputs, so use
    an ignored local build directory while retaining the pinned source manifest.
    """
    sys.path.insert(0, str(ROOT / "tools/codex"))
    import gte_oracle

    source_dir = gte_oracle.OUT
    build_dir = ROOT / "extract/mips/gte-reference"
    build_dir.mkdir(parents=True, exist_ok=True)
    for relative in [*gte_oracle.FILES, "oracle-source.json"]:
        source = source_dir / relative
        target = build_dir / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists():
            shutil.copy2(source, target)
    gte_oracle.OUT = build_dir
    original_build = gte_oracle.build
    cached = {}

    def build_once():
        if "library" not in cached:
            cached["library"] = original_build()
        return cached["library"]

    gte_oracle.build = build_once


def u32(value):
    return value & M32


def s32(value):
    return mr.signed(value, 32)


def build(source=None, tag="playerspawn"):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{os.getpid()}_{time.time_ns()}.dll"
    sources = [
        ROOT / "src/core/ae_ram.c", ROOT / "src/core/ae_sdk.c",
        ROOT / "src/core/ae_sdk_matrix.c", ROOT / "src/core/ae_pad.c",
        ROOT / "src/player/native_player.c", ROOT / "src/bodies/ae_bodies.c",
        ROOT / "src/camera/ae_camera.c", ROOT / "src/world/ae_world.c",
        ROOT / "src/sound/ae_sound.c", Path(source or ROOT / "src/player/ae_player_spawn.c"),
    ]
    subprocess.run([
        shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-shared", "-static-libgcc", "-I", str(ROOT / "src/camera"),
        "-I", str(ROOT / "src/player"),
        *map(str, sources), "-o", str(dll),
    ], check=True)
    return dll


class WorldHooks(C.Structure):
    _fields_ = [("user", C.c_void_p)] + [(name, C.c_void_p) for name in WORLD_FIELDS] + [
        ("ground_stale_x", C.c_uint16), ("ground_stale_z", C.c_uint16)]


class BodyHooks(C.Structure):
    _fields_ = [("user", C.c_void_p)] + [(name, C.c_void_p) for name in BODY_FIELDS]


class CameraHooks(C.Structure):
    _fields_ = [("user", C.c_void_p)] + [(name, C.c_void_p) for name in CAMERA_HOOKS]


class SoundHw(C.Structure):
    _fields_ = [("user", C.c_void_p), ("wait_idle", C.c_void_p), ("wait_loop", C.c_void_p),
                ("set_voice_adsr", C.c_void_p), ("voice_key_off", C.c_void_p),
                ("printf_error", C.c_void_p)]


class PlayerSpawnHooks(C.Structure):
    _fields_ = [("user", C.c_void_p), ("stack_pointer", C.c_uint32),
                ("world", C.c_void_p), ("bodies", C.c_void_p),
                ("camera", C.c_void_p), ("sound", C.c_void_p),
                ("AreaBumpAlloc", C.c_void_p), ("printf", C.c_void_p),
                ("CameraLookAt", C.c_void_p), ("SetGeomScreen", C.c_void_p)]


class Memory:
    def __init__(self, cpu=None, side=None):
        self.cpu = cpu
        self.side = side

    def read(self, address, size):
        if self.cpu is not None:
            return self.cpu.read(address, size)
        fn = {1: self.side.lib.ae_rd8, 2: self.side.lib.ae_rd16, 4: self.side.lib.ae_rd32}[size]
        return int(fn(self.side.ram, address))

    def write(self, address, value, size):
        if self.cpu is not None:
            return self.cpu.write(address, size, value)
        fn = {1: self.side.lib.ae_wr8, 2: self.side.lib.ae_wr16, 4: self.side.lib.ae_wr32}[size]
        fn(self.side.ram, address, value & ((1 << (size * 8)) - 1))


class HookModel:
    def __init__(self, context, memory):
        self.context = context
        self.memory = memory
        self.events = []

    def call(self, name, args):
        args = tuple(u32(value) for value in args)
        if name == "GetSolidHeight":
            self.events.append((name, *args[:3]))
            return s32(self.context["solid"])
        if name == "Atan2Safe":
            self.events.append((name, *args[:2]))
            return s32(self.context["atan2"])
        if name == "AreaBumpAlloc":
            self.events.append((name, *args[:1]))
            return self.context["slots"]
        if name == "printf":
            # CameraName Error %s consumes only a0/a1; its a2/a3 are stale
            # caller registers. Other exercised printf formats consume 3 values.
            used = 2 if args[0] == 0x800A4CC0 else 4
            self.events.append((name, *args[:used]))
            return 0
        if name == "CameraLookAt":
            self.events.append((name, *args[:4], self.context["roll"]))
            return 0
        if name == "SpuWaitIdle":
            self.events.append((name,))
            return self.context["idle"]
        if name == "SpuWaitLoop":
            self.events.append((name, *args[:1]))
            return 0
        if name == "SoundLoopStart":
            self.events.append((name, *args[:2]))
            return self.context["loop_handle"]
        if name == "SoundLoopStop":
            self.events.append((name, *args[:2]))
            return 0
        raise AssertionError(f"unexpected hook {name}: {args!r}")


class CSide:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        P, U, I = C.c_void_p, C.c_uint32, C.c_int32
        lib = self.lib
        lib.ae_ram_create.restype = P
        lib.ae_ram_destroy.argtypes = [P]
        lib.ae_ptr.argtypes = [P, U, U]
        lib.ae_ptr.restype = P
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_failed.restype = I
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        lib.ae_ram_clear_error.argtypes = [P]
        lib.ae_rd8.argtypes = [P, U]; lib.ae_rd8.restype = U
        lib.ae_rd16.argtypes = [P, U]; lib.ae_rd16.restype = U
        lib.ae_rd32.argtypes = [P, U]; lib.ae_rd32.restype = U
        lib.ae_wr8.argtypes = [P, U, U]
        lib.ae_wr16.argtypes = [P, U, U]
        lib.ae_wr32.argtypes = [P, U, U]

        signatures = {
            "ae_AreaGetEntryPoint": ([P, I, I, U], I),
            "ae_PlayerSpawnAtEntry": ([P, P], I),
            "ae_SetProjection": ([P, P, I], None),
            "ae_CameraActivateSlot": ([P, U], None),
            "ae_CameraResetAll": ([P, P], None),
            "ae_CameraShakeReset": ([P], None),
            "ae_AimModeLeave": ([P, P], None),
            "ae_EventCamSetScript": ([P, U, I, U, U], None),
            "ae_StageExitFlagsClear": ([P], None),
            "ae_PlaySfx": ([P, P, I], I),
        }
        for name, (args, result) in signatures.items():
            fn = getattr(lib, name)
            fn.argtypes = args
            fn.restype = result

        self.ram = lib.ae_ram_create()
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(
            lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(
            lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.mem = Memory(side=self)
        self.events = None
        self.context = None
        self.callbacks = []

        self.world = WorldHooks()
        self.body = BodyHooks()
        self.camera = CameraHooks()
        self.sound = SoundHw()
        self.hooks = PlayerSpawnHooks()
        self.hooks.stack_pointer = mr.DEFAULT_SP
        for obj in (self.world, self.body, self.camera, self.sound, self.hooks):
            obj.user = None

        def retain(cb):
            self.callbacks.append(cb)
            return C.cast(cb, P).value

        world_cb = HOOKFN(lambda user, ram, a0, a1, a2, a3:
                          self.callback("GetSolidHeight", (a0, a1, a2, a3)))
        self.world.GetSolidHeight = retain(world_cb)
        self.hooks.world = C.cast(C.pointer(self.world), P).value

        alloc_cb = C.CFUNCTYPE(U, P, P, U)(
            lambda user, ram, size: self.callback("AreaBumpAlloc", (size,)))
        printf_cb = C.CFUNCTYPE(I, P, P, U, U, U, U)(
            lambda user, ram, fmt, a1, a2, a3:
            self.callback("printf", (fmt, a1, a2, a3)))
        look_cb = C.CFUNCTYPE(None, P, P, U, U, U, U, I)(
            lambda user, ram, eye, target, view, rotation, roll:
            self.callback("CameraLookAt", (eye, target, view, rotation, roll)))
        screen_cb = C.CFUNCTYPE(None, P, P, U)(
            lambda user, ram, distance: sdk_screen(self.model.events, distance))
        self.hooks.AreaBumpAlloc = retain(alloc_cb)
        self.hooks.printf = retain(printf_cb)
        self.hooks.CameraLookAt = retain(look_cb)
        self.hooks.SetGeomScreen = retain(screen_cb)

        camera_cb = HOOKFN(lambda user, ram, a0, a1, a2, a3:
                           self.callback("Atan2Safe", (a0, a1, a2, a3)))
        self.camera.Atan2Safe = retain(camera_cb)
        self.hooks.camera = C.cast(C.pointer(self.camera), P).value

        body_printf = C.CFUNCTYPE(None, P, P, U, I)(
            lambda user, ram, fmt, value:
            self.callback("printf", (fmt, value, 0, 0)))
        body_loop = C.CFUNCTYPE(I, P, P, I, I)(
            lambda user, ram, mode, sound_id:
            self.callback("SoundLoopStart", (mode, sound_id)))
        body_stop = C.CFUNCTYPE(None, P, P, I, I)(
            lambda user, ram, sound_id, handle:
            self.callback("SoundLoopStop", (sound_id, handle)))
        self.body.printf_error = retain(body_printf)
        self.body.sound_loop_start = retain(body_loop)
        self.body.sound_loop_stop = retain(body_stop)
        self.hooks.bodies = C.cast(C.pointer(self.body), P).value

        idle_cb = C.CFUNCTYPE(I, P, P)(lambda user, ram: self.callback("SpuWaitIdle", ()))
        wait_cb = C.CFUNCTYPE(None, P, P, U)(
            lambda user, ram, units: self.callback("SpuWaitLoop", (units,)))
        sound_printf = C.CFUNCTYPE(None, P, P, U, U, I, I, I)(
            lambda user, ram, fmt, argc, a1, a2, a3:
            self.callback("printf", (fmt, a1, a2, a3)))
        self.sound.wait_idle = retain(idle_cb)
        self.sound.wait_loop = retain(wait_cb)
        self.sound.printf_error = retain(sound_printf)
        self.hooks.sound = C.cast(C.pointer(self.sound), P).value

    def callback(self, name, args):
        return self.model.call(name, args)

    def sync(self, cpu):
        C.memmove(self.main, bytes(cpu.ram), mr.RAM_SIZE)
        C.memmove(self.scratch, bytes(cpu.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)

    def invoke(self, name, args):
        self.hooks.stack_pointer = mr.DEFAULT_SP
        fn = getattr(self.lib, "ae_" + name)
        if name == "AreaGetEntryPoint":
            return fn(self.ram, *args)
        if name == "CameraActivateSlot":
            return fn(self.ram, *args)
        if name == "CameraShakeReset" or name == "StageExitFlagsClear":
            return fn(self.ram)
        if name == "EventCamSetScript":
            return fn(self.ram, *args)
        if name == "PlaySfx":
            return fn(self.ram, C.byref(self.sound), *args)
        return fn(self.ram, C.byref(self.hooks), *args)


def read_area_blob(area, member_map):
    member = member_map[area]
    return member, (ROOT / f"extract/members/{member:03X}.bin").read_bytes()


def camera_metadata(blob):
    if len(blob) < 0x4DC:
        return 0, 0, []
    offset = struct.unpack_from("<I", blob, 0x4D8)[0]
    if offset == 0 or offset + 0x10 > len(blob):
        return offset, 0, []
    if blob[offset:offset + 3] != b"CAM":
        return offset, 0, []
    count = struct.unpack_from("<H", blob, offset + 4)[0]
    name_offset = struct.unpack_from("<H", blob, offset + 0xE)[0]
    names = []
    if offset + name_offset + 2 <= len(blob):
        name_count = struct.unpack_from("<H", blob, offset + name_offset)[0]
        for i in range(name_count):
            rec = offset + name_offset + 8 + 0x28 * i
            if rec + 0x28 > len(blob):
                break
            names.append(struct.unpack_from("<H", blob, rec)[0])
    area_flags = []
    for i in range(count):
        rec = offset + 0x10 + 0x30 * i
        if rec + 0x30 > len(blob):
            break
        name_index = struct.unpack_from("<h", blob, rec)[0]
        flags = struct.unpack_from("<H", blob, rec + 2)[0]
        if 0 <= name_index < len(names):
            flags = names[name_index]
        area_flags.append(flags)
    return offset, count, area_flags


def camera_area_ids(member_map):
    nonempty = []
    has_area_script = []
    for area in sorted(member_map):
        _member, blob = read_area_blob(area, member_map)
        offset, count, flags = camera_metadata(blob)
        if offset and count:
            nonempty.append(area)
            if any((flag & 0x6000) == 0x2000 for flag in flags):
                has_area_script.append(area)
    return nonempty, has_area_script


class Harness:
    def __init__(self, source=None):
        self.m = mr.MipsRam.from_extract()
        self.m.coverage = set()
        self.m.attach_gte_oracle()
        self.base = self.m.snapshot()
        self.c = CSide(build(source))
        self.counts = {name: 0 for name in FUNCS}
        self.member_map = dump_areas.area_members()
        reg.declare("stack_window", "801FF000..80200000")
        self.current_name = None
        self.current_context = None
        self.c.model = None
        self.mips_model = None
        self.failures = 0

    def setup_area(self, area, entry=0):
        member, blob = read_area_blob(area, self.member_map)
        self.m.load_area(blob, AREA_BASE)
        stage = STAGE_STATE
        self.m.write32(stage + 0x234, AREA_BASE)
        self.m.write16(stage + 0x126, area)
        self.m.write16(stage + 0x12A, entry)
        self.m.write32(stage + 0x254, 0x80102800)
        cam_offset = struct.unpack_from("<I", blob, 0x4D8)[0]
        self.m.write32(stage + 0x250, AREA_BASE + cam_offset if cam_offset else AREA_BASE)

        header_offset = struct.unpack_from("<I", blob, 0x4D0)[0]
        self.m.write32(0x800F44E4, AREA_BASE + header_offset)
        if header_offset + 8 < len(blob):
            grid_words = struct.unpack_from("<I", blob, header_offset + 4)[0]
            grid = header_offset + grid_words * 4
            if grid + 8 <= len(blob):
                width, depth, color_count = struct.unpack_from("<HHI", blob, grid)
                cells = grid + 8
                heights = cells + width * depth * 12 + color_count * 4
                if heights + 2 * (width + 1) * (depth + 1) <= len(blob):
                    self.m.write16(0x800EBFA0, width)
                    self.m.write16(0x800EBFA2, depth)
                    self.m.write32(0x800EBFA8, AREA_BASE + cells)
                    self.m.write32(0x800EBFB4, AREA_BASE + heights)

        self.m.write32(0x800E6900, 0)
        self.m.write_bytes(PLAYER, bytes(0x250))
        self.m.write32(PLAYER + 0x24, 0)
        self.m.write32(PLAYER + 0x28, 0)
        self.m.write32(PLAYER + 0x2C, 0)
        self.m.write32(PLAYER + 0x24 + 0xC, 0)
        self.m.write32(PLAYER + 0x24 + 0x10, 0)
        self.m.write32(PLAYER + 0x24 + 0x14, 0)

        # AreaLoad calls CollisionBodiesReset before PlayerSpawnAtEntry.
        self.m.write32(0x800B20D8, 0)
        self.m.write32(0x800B20DC, 0)
        for i in range(256):
            self.m.write16(COLLISION_SLOTS + 0x14 * i, -1)

        self.m.write32(CAMERA_SLOT_GLOBAL, SLOT_POOL)
        self.m.write_bytes(SLOT_POOL, bytes(0x840))
        self.m.write32(0x800B4D44, 0)
        self.m.write32(0x800B8CE0, 0)
        self.m.write32(0x800B8CE4, 0)
        self.m.write_bytes(0x800BF610, b"\xFF" * (12 * 8))
        self.m.write16(0x800BF670, -1)
        self.m.gte_reset()
        return member, blob

    def setup_function(self, name, area, entry, args, variant):
        if name == "AreaGetEntryPoint":
            self.m.write32(STAGE_STATE + 0x234, AREA_BASE)
        elif name == "SetProjection":
            pass
        elif name == "CameraActivateSlot":
            self.m.write32(CAMERA_SLOT_GLOBAL, SLOT_POOL)
            for i in range(12):
                self.m.write8(SLOT_POOL + i * 0xB0, i + 1)
        elif name == "CameraResetAll":
            if variant == "unmatched_name":
                offset, count, _flags = camera_metadata(read_area_blob(area, self.member_map)[1])
                if offset and count:
                    region = AREA_BASE + offset + 0x10
                    self.m.write8(region + 0x10, self.m.read8(region + 0x10) ^ 0x7F)
            elif variant == "force_area_camera":
                offset, count, _flags = camera_metadata(read_area_blob(area, self.member_map)[1])
                if offset and count:
                    region_flags = AREA_BASE + offset + 0x10 + 2
                    self.m.write16(region_flags, (self.m.read16(region_flags) & 0x1FFF) | 0x2000)
            self.m.write_bytes(CAMERA_STATE, bytes((0xA5 ^ i) & 0xFF for i in range(0x7C)))
        elif name == "AimModeLeave":
            self.m.write8(CAMERA_STATE + 0x46, 1 if variant == "active" else 0)
            if variant == "active":
                check_sound.bank_setup(self.m, random.Random(94423))
                self.m.write16(0x800BF670, 2)
                self.m.write16(0x800BF672, 3)
                for sound_id, program, note in ((5, 1, 1), (6, 2, 3)):
                    effect = 0x800B2D08 + sound_id * 8
                    self.m.write16(effect, 0)
                    self.m.write16(effect + 2, program)
                    self.m.write16(effect + 4, note)
                for i in range(12):
                    self.m.write16(0x800BF610 + i * 8, -1)
            else:
                self.m.write16(0x800BF670, -1)
        elif name == "EventCamSetScript":
            self.m.write32(CAMERA_SLOT_GLOBAL, SLOT_POOL)
            self.m.write32(CAMERA_STATE + 0x50, 0x801D4000)
            self.m.write32(CAMERA_STATE + 0x58, 0x801D4008)
            self.m.write32(CAMERA_STATE + 0x54, 0x801D5000)
            self.m.write32(CAMERA_STATE + 0x5C, 0x801D5008)
            for index in range(8):
                self.m.write16(0x801D4008 + index * 0x28 + 2, 0x100 + index * 8)
                self.m.write16(0x801D5008 + index * 0x28 + 2, 0x180 + index * 8)
        elif name == "PlaySfx":
            sound_id = args[0]
            effect = 0x800B2D08 + 8 * (sound_id & M32)
            mode = variant
            if mode == "bank_missing":
                self.m.write16(0x800BF670, -1)
            else:
                check_sound.bank_setup(self.m, random.Random(94423))
                self.m.write16(0x800BF670, 2)
                self.m.write16(effect, 0)
                self.m.write16(effect + 2, 1)
                self.m.write16(effect + 4, 1)
                if mode == "already_queued":
                    self.m.write16(0x800BF610, 0)
                    self.m.write16(0x800BF612, sound_id)
                elif mode == "queue_full":
                    for i in range(12):
                        self.m.write16(0x800BF610 + i * 8, 0)
                        self.m.write16(0x800BF612 + i * 8, i + 100)
                elif mode == "queue_success":
                    for i in range(12):
                        self.m.write16(0x800BF610 + i * 8, -1)
        elif name == "CameraShakeReset":
            self.m.write8(0x800E6958, 0x99)
            self.m.write8(0x800E6959, 0x77)
        elif name == "StageExitFlagsClear":
            self.m.write16(0x800B8CE0, 0x1234)
            self.m.write8(0x800B8CE4, 0x56)

        return args

    def run(self, name, args=(), area=1, entry=0, variant="", scenario=""):
        self.m.restore(self.base)
        member, blob = self.setup_area(area, entry)
        args = self.setup_function(name, area, entry, args, variant)
        self.c.sync(self.m)
        context = {"solid": -120, "atan2": -1024, "slots": SLOT_POOL,
                   "roll": 0, "idle": int(name == "PlaySfx" and variant == "voice_busy"),
                   "loop_handle": 6}
        self.current_name = name
        self.current_context = context
        self.mips_model = HookModel(context, Memory(cpu=self.m))
        self.c.model = HookModel(context, self.c.mem)
        self.c.events = self.c.model.events
        original_hooks = {}
        for hook_name, address in KIND_NAMES.items():
            def make_hook(label):
                def callback(cpu, registers):
                    a0, a1, a2, a3 = registers[4:8]
                    if label == "CameraLookAt":
                        context["roll"] = cpu.read32(registers[29] + 16)
                        return self.mips_model.call(label, (a0, a1, a2, a3))
                    if label == "printf":
                        return self.mips_model.call(label, (a0, a1, a2, a3))
                    if label == "GetSolidHeight":
                        return self.mips_model.call(label, (a0, a1, a2, a3))
                    if label == "Atan2Safe":
                        return self.mips_model.call(label, (a0, a1, a2, a3))
                    if label == "SpuWaitIdle":
                        return self.mips_model.call(label, ())
                    return self.mips_model.call(label, (a0, a1, a2, a3))
                return callback
            original_hooks[address] = make_hook(hook_name)

        original_hooks[0x8002FAF0] = lambda cpu, regs: sdk_screen(self.mips_model.events, regs[4])
        self.m.hook_log.clear()
        mips_result = self.m.call(FUNCS[name][0], args, hooks=original_hooks)
        c_result = self.c.invoke(name, args)
        self.counts[name] += 1

        if FUNCS[name][2] == "s" and u32(c_result) != mips_result[2]:
            self.fail(name, scenario, f"return C={u32(c_result):08X} MIPS={mips_result[2]:08X}")
        original_events = self.mips_model.events
        c_events = self.c.model.events
        if original_events != c_events:
            self.fail(name, scenario, f"hook sequence C={c_events!r} MIPS={original_events!r}")

        expected = bytes(self.m.ram[:STACK_LO - 0x80000000])
        actual = C.string_at(self.c.main, STACK_LO - 0x80000000)
        if actual != expected:
            off = next(i for i, (left, right) in enumerate(zip(actual, expected)) if left != right)
            self.fail(name, scenario,
                      f"RAM {0x80000000 + off:08X}: C={actual[off]:02X} MIPS={expected[off]:02X}")
        if bytes(self.c.scratch) != bytes(self.m.scratch):
            self.fail(name, scenario, "scratchpad differs")
        if self.c.lib.ae_ram_failed(self.c.ram):
            self.fail(name, scenario, self.c.lib.ae_ram_error(self.c.ram).decode(errors="replace"))
        return member, blob

    def fail(self, name, scenario, detail):
        self.failures += 1
        raise AssertionError(f"{name} {scenario}: {detail}")

    def scenarios(self, scale=1.0, seed=94423, only=None):
        rng = random.Random(seed)
        nonempty, scripted = camera_area_ids(self.member_map)
        camera_area = scripted[0] if scripted else (nonempty[0] if nonempty else 1)
        extra_camera = next((area for area in nonempty if area != camera_area), camera_area)
        player_areas = [area for area in (1, 5, camera_area, extra_camera) if area in self.member_map]

        def run(name, args=(), area=1, entry=0, variant="", label=""):
            if only is not None and name != only:
                return
            self.run(name, args, area, entry, variant, label or variant or f"area=0x{area:02X},entry={entry}")

        for area in player_areas:
            for entry in (0, 1, 15):
                run("PlayerSpawnAtEntry", area=area, entry=entry,
                    label=f"real area 0x{area:02X}, entry {entry}")
        for entry in (0, 1, 15):
            run("AreaGetEntryPoint", (1, entry, ENTRY_OUT), area=1, entry=entry,
                label=f"entry index {entry}")
        for value in (0x148, 0x12345, -32768):
            run("SetProjection", (value,), label=f"projection {value}")
        for index in (0, 7, 11):
            run("CameraActivateSlot", (index,), label=f"slot {index}")
        run("CameraResetAll", area=0x04, label="real area 0x04 without CAM")
        run("CameraResetAll", area=5, label="real area 0x05")
        if nonempty:
            run("CameraResetAll", area=camera_area, label=f"real CAM area 0x{camera_area:02X}")
            run("CameraResetAll", area=camera_area, variant="unmatched_name",
                label=f"real CAM area 0x{camera_area:02X}, one unmatched name")
            run("CameraResetAll", area=camera_area, variant="force_area_camera",
                label=f"real CAM area 0x{camera_area:02X}, area camera slot")
        run("CameraShakeReset", label="nonzero shake fields")
        run("AimModeLeave", variant="inactive", label="inactive aim")
        run("AimModeLeave", variant="active", label="active aim calls PlaySfx")
        for table_set in (0, 1):
            run("EventCamSetScript", (3, 2, 1, table_set), area=camera_area,
                label=f"script set {table_set}")
        run("StageExitFlagsClear", label="nonzero exit flags")
        for mode in ("bank_missing", "already_queued", "queue_full", "queue_success", "voice_busy"):
            run("PlaySfx", (9,), variant=mode, label=mode)

        # A small seeded edge sweep complements the data-backed path cases.
        reps = max(0, round(4 * scale) - 4)
        for i in range(reps):
            run("AreaGetEntryPoint", (rng.randint(-32768, 32767), rng.randrange(16), ENTRY_OUT),
                area=1, label=f"seeded entry edge {i}")

    def finish(self, show_coverage=False, default_run=False):
        items = [(name, address, size, True) for name, (address, size, *_rest) in FUNCS.items()]
        summary = reg.coverage_summary(items, self.m.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        gaps = any(item["uncovered_count"] or item["stale_declared"] for item in summary.values())
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps,
                              function_names=list(FUNCS))
        if gaps:
            raise SystemExit("FAIL: undeclared player-spawn instruction coverage gaps")
        return summary


def sdk_screen(events, distance):
    events.append(("SetGeomScreen", (distance & M32,)))
    return 0


def mutation_sources(original):
    specs = [
        ("AreaGetEntryPoint", "wrong record stride", "(uint32_t)entry * AE_SPAWN_ENTRY_SIZE", "(uint32_t)entry * (AE_SPAWN_ENTRY_SIZE - 4u)"),
        ("AreaGetEntryPoint", "drop final copied word", "ae_wr32(r, out + AE_SPAWN_ENTRY_SIZE - AE_SPAWN_ENTRY_WORD_SIZE,\n            ae_rd32(r, record + AE_SPAWN_ENTRY_SIZE - AE_SPAWN_ENTRY_WORD_SIZE));", "ae_wr32(r, out + AE_SPAWN_ENTRY_SIZE - AE_SPAWN_ENTRY_WORD_SIZE, 0);"),
        ("AreaGetEntryPoint", "copy from next record", "record + word * AE_SPAWN_ENTRY_WORD_SIZE", "record + word * AE_SPAWN_ENTRY_WORD_SIZE + 4u"),
        ("PlayerSpawnAtEntry", "wrong camera eye component stride", "axis * AE_SPAWN_PACKED_VECTOR_COMPONENT_SIZE", "axis * AE_SPAWN_ENTRY_WORD_SIZE"),
        ("PlayerSpawnAtEntry", "preserve entry y instead of ground y", "(uint32_t)spawn_s16((uint32_t)ground_y));\n    ae_wr32(r, player + AE_SPAWN_PLAYER_POSITION_Y", "(uint32_t)position_y);\n    ae_wr32(r, player + AE_SPAWN_PLAYER_POSITION_Y"),
        ("PlayerSpawnAtEntry", "skip body registration", "(void)ae_CollisionBodyRegister(r, h->bodies, 1,\n                                   player + AE_SPAWN_PLAYER_POSITION_X, player);", "(void)0;"),
        ("SetProjection", "store projection in wrong field", "AE_SPAWN_CAMERA_PROJECTION", "AE_SPAWN_CAMERA_ACTIVE_KIND"),
        ("SetProjection", "pass altered projection", "(uint16_t)distance);", "(uint16_t)(distance - 1));"),
        ("SetProjection", "skip projection store", "ae_wr16(r, AE_SPAWN_G_CAMERA_STATE + AE_SPAWN_CAMERA_PROJECTION, (uint32_t)distance);", "ae_wr16(r, AE_SPAWN_G_CAMERA_STATE + AE_SPAWN_CAMERA_PROJECTION, 0);"),
        ("CameraActivateSlot", "wrong slot size", "index * AE_SPAWN_CAMERA_SLOT_SIZE", "index * (AE_SPAWN_CAMERA_SLOT_SIZE - 2u)"),
        ("CameraActivateSlot", "store the pointer as the index", "state + AE_SPAWN_CAMERA_PRIORITY, index", "state + AE_SPAWN_CAMERA_PRIORITY, slot"),
        ("CameraActivateSlot", "read the wrong slot kind", "slot + AE_SPAWN_CAMERA_SLOT_KIND", "slot + AE_SPAWN_CAMERA_SLOT_KIND + 1u"),
        ("CameraResetAll", "shorten camera state clear", "byte < AE_SPAWN_CAMERA_STATE_SIZE", "byte < AE_SPAWN_CAMERA_STATE_SIZE - 1u"),
        ("CameraResetAll", "do not mark parsed CAM header", "AE_SPAWN_CAMERA_MARKED_SIGNATURE", "AE_SPAWN_CAMERA_VALID_SIGNATURE"),
        ("CameraResetAll", "disable area script slot branch", "AE_SPAWN_CAMERA_REGION_KIND_AREA", "0x4000u"),
        ("CameraShakeReset", "leave shake counter set", "ae_wr8(r, AE_SPAWN_G_CAMERA_SHAKE + AE_SPAWN_SHAKE_COUNTER, 0);", "ae_wr8(r, AE_SPAWN_G_CAMERA_SHAKE + AE_SPAWN_SHAKE_COUNTER, 0x99u);"),
        ("CameraShakeReset", "clear wrong shake byte", "AE_SPAWN_SHAKE_ACTIVE", "AE_SPAWN_SHAKE_COUNTER"),
        ("AimModeLeave", "do not clear aim mode", "ae_wr8(r, aim_mode, 0);", "ae_wr8(r, aim_mode, 1);"),
        ("AimModeLeave", "play wrong sound", "NULL, 5);", "NULL, 6);"),
        ("AimModeLeave", "skip sound request", "(void)ae_PlaySfx(r, h != NULL ? h->sound : NULL, 5);", "(void)h;"),
        ("EventCamSetScript", "wrong script table for set zero", "camera_state + AE_SPAWN_CAMERA_SCRIPT_INDEXES", "camera_state + AE_SPAWN_CAMERA_NAME_ENTRIES"),
        ("EventCamSetScript", "wrong script offset field", "record + AE_SPAWN_CAM_NAME_SCRIPT_OFFSET", "record + AE_SPAWN_CAM_NAME_FLAGS"),
        ("EventCamSetScript", "skip timer reset", "camera_slot + AE_SPAWN_CAMERA_SLOT_SCRIPT_TIMER, 0", "camera_slot + AE_SPAWN_CAMERA_SLOT_SCRIPT_TIMER, 1"),
        ("StageExitFlagsClear", "retain transition flags", "AE_SPAWN_G_STAGE_EXIT_FLAGS + AE_SPAWN_STAGE_EXIT_FLAGS, 0", "AE_SPAWN_G_STAGE_EXIT_FLAGS + AE_SPAWN_STAGE_EXIT_FLAGS, 1"),
        ("StageExitFlagsClear", "clear halfword as a byte", "ae_wr16(r, AE_SPAWN_G_STAGE_EXIT_FLAGS + AE_SPAWN_STAGE_EXIT_FLAGS, 0);", "ae_wr8(r, AE_SPAWN_G_STAGE_EXIT_FLAGS + AE_SPAWN_STAGE_EXIT_FLAGS, 0);"),
        ("StageExitFlagsClear", "leave pending byte set", "AE_SPAWN_G_STAGE_EXIT_FLAGS + AE_SPAWN_STAGE_EXIT_PENDING, 0", "AE_SPAWN_G_STAGE_EXIT_FLAGS + AE_SPAWN_STAGE_EXIT_PENDING, 1"),
        ("PlaySfx", "wrong sound bank slot", "AE_SOUND_BANK_IDS + (uint32_t)bank_slot * 2u", "AE_SOUND_BANK_IDS + (uint32_t)(bank_slot + 1) * 2u"),
        ("PlaySfx", "reverse duplicate queue test", "ae_SfxIsQueued(r, (int16_t)id) != 0", "ae_SfxIsQueued(r, (int16_t)id) == 0"),
        ("PlaySfx", "return voice plus one", "return ae_rds16(r, effect + AE_SE_LAST_VOICE);", "return ae_rds16(r, effect + AE_SE_LAST_VOICE) + 1;"),
    ]
    for name, label, before, after in specs:
        start = original.index(f"ae_{name}(")
        end = original.index("\n}\n", start) + 2
        body = original[start:end]
        if before not in body:
            raise AssertionError(f"mutation {name}: missing anchor {before!r}")
        yield f"{name}: {label}", original[:start] + body.replace(before, after, 1) + original[end:]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--cases", type=float, default=1.0)
    parser.add_argument("--seed", type=int, default=94423)
    args = parser.parse_args()
    prepare_gte_build_output()

    if args.mutate:
        original_path = ROOT / "src/player/ae_player_spawn.c"
        original = original_path.read_text(encoding="utf-8")
        mutations = list(mutation_sources(original))
        survivors = []
        for index, (label, source) in enumerate(mutations):
            path = ROOT / f"extract/mips/player_spawn_mutation_{index}.c"
            path.write_text(source, encoding="utf-8")
            function = label.split(":", 1)[0]
            h = Harness(path)
            try:
                h.scenarios(max(args.cases, 1.0), args.seed, only=function)
            except AssertionError:
                if not h.failures:
                    raise
            else:
                print("SURVIVED", label)
                survivors.append(label)
        print(f"Mutations: {len(mutations)}; caught: {len(mutations)-len(survivors)}; survivors: {len(survivors)}")
        return int(bool(survivors))

    harness = Harness()
    harness.scenarios(args.cases, args.seed)
    summary = harness.finish(show_coverage=args.coverage,
                             default_run=args.cases == 1.0 and args.seed == 94423)
    print(" ".join(f"{name}={harness.counts[name]}" for name in FUNCS))
    print(f"OK: {sum(harness.counts.values())} differential cases")
    return int(any(item["uncovered_count"] or item["stale_declared"] for item in summary.values()))


if __name__ == "__main__":
    sys.exit(main())
