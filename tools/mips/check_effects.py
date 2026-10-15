"""Differential checks for the self-contained effects leaf functions.

The oracle is the original SCUS_944.23 code on real area-member RAM images.
Run with .venv/Scripts/python.exe tools/mips/check_effects.py.
"""
from __future__ import annotations

import argparse
import ctypes as C
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/mips"), str(ROOT / "tools")]
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

M32 = 0xFFFFFFFF
RAM_SIZE = 0x200000
SCRATCH_SIZE = 0x400
STACK_START = 0x801FF000
PLAYER = 0x800EC1E0
TRAIL_POINTS = 0x800F4E90
TRAIL_SEED = 0x800AE8F0
SPRITE_QUEUE = 0x800BADA0
SPRITE_CURSOR = 0x800B1B40
POSITION = 0x801D0000
SPEC = 0x801D0100
RANDOM_PARTICLES = 0x800BB840

FUNCS = {
    "PlayerCanExitStage": (0x8005DC78, 108, "s", "ae_PlayerCanExitStage"),
    "EffectsUpdateAll": (0x8006A9F8, 104, "v", "ae_EffectsUpdateAll"),
    "TrailPointAdd": (0x8006EF08, 128, "v", "ae_TrailPointAdd"),
    "SpriteEffectQueue": (0x8006D4C0, 136, "v", "ae_SpriteEffectQueue"),
    "SpriteEffectQueue2": (0x8006D588, 32, "v", "ae_SpriteEffectQueue2"),
    "PlayerFxTrailUpdate": (0x8006B168, 436, "v", "ae_PlayerFxTrailUpdate"),
    "PlayerEffectsUpdate": (0x8006B800, 72, "v", "ae_PlayerEffectsUpdate"),
    "PlayerWaterEffectsUpdate": (0x8006BA2C, 1252, "v", "ae_PlayerWaterEffectsUpdate"),
    "SpriteEffectsDraw": (0x8006D5A8, 1312, "v", "ae_SpriteEffectsDraw"),
    "PlayerSplash": (0x8006B950, 168, "v", "ae_PlayerSplash"),
    "PlayerTintEffectUpdate": (0x8006C334, 600, "v", "ae_PlayerTintEffectUpdate"),
    "TrailsUpdate": (0x8006EE3C, 204, "v", "ae_TrailsUpdate"),
    "MeshTransform": (0x800160EC, 212, "v", "ae_MeshTransform"),
    "MeshDraw": (0x80017E5C, 160, "v", "ae_MeshDraw"),
    "FlyingMeshesUpdateDraw": (0x80071370, 548, "v", "ae_FlyingMeshesUpdateDraw"),
    "RandomParticlesUpdate": (0x80070D94, 868, "v", "ae_RandomParticlesUpdate"),
    "TrailPointsDraw": (0x8006EF88, 748, "v", "ae_TrailPointsDraw"),
    "EffectObjDraw": (0x8006FA10, 1092, "v", "ae_EffectObjDraw"),
    "EffectObjsUpdate": (0x8006FEE8, 584, "v", "ae_EffectObjsUpdate"),
    "FxPlayerState6Update": (0x8006E1C0, 1012, "v", "ae_FxPlayerState6Update"),
    "GroundSparksUpdateDraw": (0x80070560, 1512, "v", "ae_GroundSparksUpdateDraw"),
}

_MUTATION_OBJECT_DIR: Path | None = None
_BASE_OBJECTS: list[Path] | None = None
_GTE_LIBRARY = None
_GTE_NATIVE_FUNCTION = None

PaddingFn = C.CFUNCTYPE(C.c_uint16, C.c_void_p, C.c_void_p, C.c_uint32)
SpawnHookFn = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                          C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)


class SpawnHooks(C.Structure):
    _fields_ = [("user", C.c_void_p), ("world", C.c_void_p),
                ("PlaySfx", SpawnHookFn), ("MatrixLookDir", SpawnHookFn),
                ("MatrixFromDirXZ", SpawnHookFn), ("position_padding", PaddingFn)]


class SoundHw(C.Structure):
    _fields_ = [("user", C.c_void_p), ("wait_idle", C.c_void_p), ("wait_loop", C.c_void_p),
                ("set_voice_adsr", C.c_void_p), ("voice_key_off", C.c_void_p),
                ("printf_error", C.c_void_p), ("set_voice_volume", C.c_void_p)]


EffectsCallFn = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                            C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
EffectsPaddingFn = C.CFUNCTYPE(C.c_uint16, C.c_void_p, C.c_void_p,
                               C.c_uint32, C.c_int32)


class EffectsHooks(C.Structure):
    _fields_ = [("user", C.c_void_p), ("EffectRingDraw", EffectsCallFn),
                ("SpawnRandomParticle", EffectsCallFn), ("GpuProjectMesh", EffectsCallFn),
                ("GpuDrawMesh", EffectsCallFn), ("Gadget4TrailUpdate", EffectsCallFn),
                ("GadgetFlashUpdate", EffectsCallFn)]
    _fields_.extend([("sound", C.c_void_p), ("LocalPositionPadding", EffectsPaddingFn),
                     ("GpuDrawSprite", EffectsCallFn), ("RibbonDraw", EffectsCallFn),
                     ("SetRotMatrix", EffectsCallFn), ("SetTransMatrix", EffectsCallFn),
                     ("GpuDrawTrailPoint", EffectsCallFn)])
    _fields_.append(("GpuDrawEffectObj", EffectsCallFn))
    _fields_.extend([("SparkAddRandom", EffectsCallFn),
                     ("GpuDrawFxPlayerState6", EffectsCallFn),
                     ("GpuDrawGroundSpark", EffectsCallFn)])

class AeRam(C.Structure):
    _fields_ = [("main", C.c_uint8 * RAM_SIZE), ("scratch", C.c_uint8 * SCRATCH_SIZE),
                ("error", C.c_int), ("error_text", C.c_char * 192)]


def build(core_source: Path | None = None, player_source: Path | None = None,
          trails_source: Path | None = None, mesh_source: Path | None = None,
          flying_source: Path | None = None, dispatch_source: Path | None = None,
          water_source: Path | None = None, sprite_source: Path | None = None,
          random_source: Path | None = None,
          trail_draw_source: Path | None = None,
          objects_source: Path | None = None,
          player_state_source: Path | None = None,
          ground_sparks_source: Path | None = None,
          label: str = "effects") -> Path:
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{label}_{os.getpid()}_{time.time_ns()}.dll"
    base_sources = [
        ROOT / "src/core/ae_ram.c", ROOT / "src/core/ae_sdk.c", ROOT / "src/core/ae_sdk_matrix.c",
        ROOT / "src/core/ae_pad.c", ROOT / "src/player/native_player.c", ROOT / "src/bodies/ae_bodies.c",
        ROOT / "src/camera/ae_camera.c", ROOT / "src/world/ae_world.c", ROOT / "src/sound/ae_sound.c",
        ROOT / "src/player/ae_player_spawn.c", ROOT / "src/effects/ae_spawn.c",
        ROOT / "src/effects/ae_effects_core.c",
        ROOT / "src/effects/ae_effects_player.c",
        ROOT / "src/effects/ae_effects_trails.c",
        ROOT / "src/effects/ae_effects_mesh.c",
        flying_source or ROOT / "src/effects/ae_effects_flying.c",
        ROOT / "src/effects/ae_effects_dispatch.c",
        water_source or ROOT / "src/effects/ae_effects_water.c",
        sprite_source or ROOT / "src/effects/ae_effects_sprite_draw.c",
        random_source or ROOT / "src/effects/ae_effects_random.c",
        trail_draw_source or ROOT / "src/effects/ae_effects_trail_draw.c",
        ROOT / "tools/mips/effects_test_api.c",
        ROOT / "src/math/native_sdk_matrix.c",
        ROOT / "src/world/ae_solid.c",
        objects_source or ROOT / "src/effects/ae_effects_objects.c",
        player_state_source or ROOT / "src/effects/ae_effects_player_state.c",
        ground_sparks_source or ROOT / "src/effects/ae_effects_ground_sparks.c",
    ]
    changed = [(index, source) for index, source in (
        (11, core_source), (12, player_source), (13, trails_source),
        (14, mesh_source), (15, flying_source), (16, dispatch_source),
        (17, water_source), (18, sprite_source), (19, random_source),
        (20, trail_draw_source),
        (24, objects_source),
        (25, player_state_source),
        (26, ground_sparks_source),
    ) if source is not None]
    gcc = shutil.which("gcc") or "gcc"
    if not changed:
        subprocess.run([
            gcc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-DAE_EFFECTS_TEST",
            "-shared", "-static-libgcc", "-I", str(ROOT / "src/effects"),
            *map(str, base_sources), "-o", str(dll),
        ], check=True)
        return dll

    global _MUTATION_OBJECT_DIR, _BASE_OBJECTS
    if _MUTATION_OBJECT_DIR is None:
        _MUTATION_OBJECT_DIR = out / f"effects_obj_{os.getpid()}"
        _MUTATION_OBJECT_DIR.mkdir(parents=True, exist_ok=True)
        _BASE_OBJECTS = []
        for index, source in enumerate(base_sources):
            obj = _MUTATION_OBJECT_DIR / f"base_{index:02d}.o"
            subprocess.run([
                gcc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-DAE_EFFECTS_TEST",
                "-fPIC", "-I", str(ROOT / "src/effects"), "-c", str(source), "-o", str(obj),
            ], check=True)
            _BASE_OBJECTS.append(obj)

    link_objects = list(_BASE_OBJECTS or [])
    for index, source in changed:
        obj = _MUTATION_OBJECT_DIR / f"mut_{index:02d}_{time.time_ns()}.o"
        subprocess.run([
            gcc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-DAE_EFFECTS_TEST",
            "-fPIC", "-I", str(ROOT / "src/effects"), "-c", str(source), "-o", str(obj),
        ], check=True)
        link_objects[index] = obj
    subprocess.run([gcc, "-shared", "-static-libgcc", *map(str, link_objects), "-o", str(dll)], check=True)
    return dll


def bind(dll: Path):
    lib = C.CDLL(str(dll))
    lib.ae_ram_clear_error.argtypes = [C.POINTER(AeRam)]
    lib.ae_ram_failed.argtypes = [C.POINTER(AeRam)]
    lib.ae_ram_failed.restype = C.c_int
    lib.ae_PlayerCanExitStage.argtypes = [C.POINTER(AeRam)]
    lib.ae_PlayerCanExitStage.restype = C.c_int32
    lib.ae_TrailPointAdd.argtypes = [C.POINTER(AeRam), C.c_uint32]
    lib.ae_SpriteEffectQueue.argtypes = [C.POINTER(AeRam), C.c_uint32, C.c_int32]
    lib.ae_SpriteEffectQueue2.argtypes = [C.POINTER(AeRam), C.c_uint32]
    lib.ae_PlayerFxTrailUpdate.argtypes = [C.POINTER(AeRam), C.POINTER(SpawnHooks)]
    lib.ae_PlayerSplash.argtypes = [C.POINTER(AeRam), C.POINTER(SpawnHooks), C.POINTER(SoundHw)]
    lib.ae_PlayerTintEffectUpdate.argtypes = [C.POINTER(AeRam), C.POINTER(SpawnHooks), C.POINTER(EffectsHooks)]
    lib.ae_TrailsUpdate.argtypes = [C.POINTER(AeRam), C.POINTER(EffectsHooks)]
    lib.ae_MeshTransform.argtypes = [C.POINTER(AeRam), C.POINTER(EffectsHooks), C.c_uint32, C.c_uint32, C.c_uint32]
    lib.ae_MeshDraw.argtypes = [C.POINTER(AeRam), C.POINTER(EffectsHooks), C.c_uint32, C.c_uint32, C.c_uint32]
    lib.ae_FlyingMeshesUpdateDraw.argtypes = [C.POINTER(AeRam), C.POINTER(SpawnHooks), C.POINTER(EffectsHooks)]
    lib.ae_PlayerEffectsUpdate.argtypes = [C.POINTER(AeRam), C.POINTER(SpawnHooks), C.POINTER(EffectsHooks)]
    lib.ae_EffectsUpdateAll.argtypes = [C.POINTER(AeRam), C.POINTER(SpawnHooks), C.POINTER(EffectsHooks)]
    lib.ae_PlayerWaterEffectsUpdate.argtypes = [C.POINTER(AeRam), C.POINTER(SpawnHooks), C.POINTER(EffectsHooks)]
    lib.ae_SpriteEffectsDraw.argtypes = [C.POINTER(AeRam), C.POINTER(EffectsHooks)]
    lib.ae_RandomParticlesUpdate.argtypes = [C.POINTER(AeRam), C.POINTER(EffectsHooks)]
    lib.ae_TrailPointsDraw.argtypes = [C.POINTER(AeRam), C.POINTER(EffectsHooks)]
    lib.ae_EffectObjDraw.argtypes = [C.POINTER(AeRam), C.POINTER(EffectsHooks), C.c_uint32]
    lib.ae_EffectObjsUpdate.argtypes = [C.POINTER(AeRam), C.POINTER(EffectsHooks)]
    lib.ae_FxPlayerState6Update.argtypes = [C.POINTER(AeRam), C.POINTER(EffectsHooks)]
    lib.ae_GroundSparksUpdateDraw.argtypes = [C.POINTER(AeRam), C.POINTER(SpawnHooks), C.POINTER(EffectsHooks)]
    lib.effects_test_reset.argtypes = []
    lib.effects_test_sfx_count.argtypes = []
    lib.effects_test_sfx_count.restype = C.c_uint32
    lib.effects_test_sfx_id.argtypes = [C.c_uint32]
    lib.effects_test_sfx_id.restype = C.c_int32
    lib.effects_test_spawn_count.argtypes = []
    lib.effects_test_spawn_count.restype = C.c_uint32
    lib.effects_test_spawn_type.argtypes = [C.c_uint32]
    lib.effects_test_spawn_type.restype = C.c_int32
    lib.effects_test_spawn_position.argtypes = [C.c_uint32]
    lib.effects_test_spawn_position.restype = C.c_uint32
    return lib


def copy_to_c(cpu: mr.MipsRam) -> AeRam:
    ram = AeRam()
    C.memmove(C.addressof(ram), bytes(cpu.ram), RAM_SIZE)
    C.memmove(C.addressof(ram) + RAM_SIZE, bytes(cpu.scratch), SCRATCH_SIZE)
    return ram


def area_ids() -> list[int]:
    member_map = dump_areas.area_members()
    preferred = [0x01, 0x02, 0x03, 0x05, 0x12, 0x1D, 0x2F, 0x40, 0x5C]
    result = [a for a in preferred if a in member_map]
    if len(result) < 3:
        result = sorted(member_map)[:max(3, len(result))]
    if len(result) < 3:
        raise RuntimeError("need real area-member data from at least three areas")
    return result


def area_cpu(area: int) -> mr.MipsRam:
    member = dump_areas.area_members()[area]
    path = ROOT / "extract" / "members" / f"{member:03X}.bin"
    cpu = mr.MipsRam.from_extract(area=path)
    cpu.coverage = set()
    return cpu


def attach_gte_oracle(cpu: mr.MipsRam):
    global _GTE_LIBRARY, _GTE_NATIVE_FUNCTION
    if _GTE_NATIVE_FUNCTION is None:
        cpu.attach_gte_oracle()
        _GTE_LIBRARY = cpu._gte_lib
        _GTE_NATIVE_FUNCTION = cpu.gte_backend._native_function
        return

    state = (C.c_uint32 * 64)()
    cpu._gte_lib = _GTE_LIBRARY
    cpu.gte_state = state
    def backend(instruction, value):
        return _GTE_NATIVE_FUNCTION(state, instruction, value & M32)
    backend._native_function = _GTE_NATIVE_FUNCTION
    backend._native_state = state
    cpu.gte_backend = backend


def load_gte_control_matrix(cpu: mr.MipsRam, matrix: int,
                            first_register: int, first_offset: int,
                            word_count: int):
    """Apply the MIPS CTC2 side effects of PsyQ's matrix-load helpers."""
    for index in range(word_count):
        register = first_register + index
        value = cpu.read32(matrix + first_offset + index * 4)
        cpu.gte_backend(0x48C00000 | (register << 11), value)


def configure_flying_meshes(cpu: mr.MipsRam, case: int, rng: random.Random):
    model_base = 0x801D3000
    cpu.write32(0x800B4D64, model_base)
    cpu.write32(0x800EC35C, case % 4)
    cpu.write_bytes(0x800E78C0, bytes(32))
    for axis in range(3):
        cpu.write16(0x800E78C0 + 2 * (axis * 3 + axis), 4096)

    states = [0, 1, 2, 3, 0xFF]
    angular_velocities = [-7, -1, 0, 1, 9]
    vertical_velocities = [-15, -3, 0, 5, 18]
    for index in range(12):
        record = 0x800BB920 + index * 0x44
        cpu.write_bytes(record, bytes(0x44))
        for axis in range(3):
            cpu.write16(record + axis * 8, 4096)
        cpu.write16(record + 0x20, rng.randrange(0x1000))
        cpu.write16(record + 0x22, rng.randrange(0x1000))
        cpu.write16(record + 0x24, rng.randrange(0x1000))
        for axis, offset in enumerate((0x28, 0x2A, 0x2C)):
            cpu.write16(record + offset, angular_velocities[(case + index + axis) % len(angular_velocities)])
        x = rng.randrange(-200, 201)
        y = rng.randrange(-40, 81)
        z = rng.randrange(-200, 201)
        vx = rng.randrange(-8, 9)
        vy = vertical_velocities[(case + index) % len(vertical_velocities)]
        vz = rng.randrange(-8, 9)
        cpu.write16(record + 0x30, x)
        cpu.write16(record + 0x32, y)
        cpu.write16(record + 0x34, z)
        cpu.write16(record + 0x36, vx)
        cpu.write16(record + 0x38, vy)
        cpu.write16(record + 0x3A, vz)
        cpu.write8(record + 0x3C, states[(case + index) % len(states)])
        cpu.write8(record + 0x3D, (case + index) & 3)
        after_y = y + vy
        ground_y = after_y - 1 if (case + index) % 3 == 0 else after_y + 24
        cpu.write16(record + 0x3E, ground_y)
        cpu.write16(record + 0x40, index % 4)


def configure_dispatch(cpu: mr.MipsRam, name: str, case: int, rng: random.Random):
    cpu.write8(0x800F447C, 5)
    cpu.write16(PLAYER + 0x5E, 0x45)
    cpu.write32(PLAYER + 0x70, 0)
    cpu.write16(PLAYER + 0x138, 0)
    cpu.write32(0x800BAD70, 1 if name == "PlayerEffectsUpdate" and case % 4 == 0 else 0)
    cpu.write_bytes(0x800BAD78, bytes(8))

    for index in range(8):
        trail = 0x800F5010 + index * 0x10
        cpu.write_bytes(trail, bytes(0x10))
    if name == "EffectsUpdateAll" and case % 3 == 1:
        trail = 0x800F5010
        cpu.write8(trail + 0x0A, 2)
        cpu.write16(trail + 0x0C, 1234)
        cpu.write16(trail + 0x0E, 500)

    if name == "EffectsUpdateAll":
        configure_flying_meshes(cpu, case, rng)
        for index in range(12):
            record = 0x800BB920 + index * 0x44
            cpu.write8(record + 0x3C, 0)
            cpu.write8(record + 0x3D, 0)
        if case % 2 == 0:
            record = 0x800BB920
            cpu.write8(record + 0x3C, 2)
            cpu.write8(record + 0x3D, 0)
            cpu.write16(record + 0x30, 20)
            cpu.write16(record + 0x32, 10)
            cpu.write16(record + 0x34, 80)
            cpu.write16(record + 0x36, 1)
            cpu.write16(record + 0x38, 0)
            cpu.write16(record + 0x3A, 1)
            cpu.write16(record + 0x3E, 100)
        configure_sprite_draw(cpu, case, case + 17)
        configure_random_particles(cpu, case, rng)
        configure_effect_objects(cpu, case, rng)
        saved_player_state = cpu.read16(PLAYER + 0x5E)
        saved_player_motion = cpu.read_bytes(PLAYER + 0x24, 0x1A)
        configure_fx_player_state6(cpu, case, rng)
        cpu.write16(PLAYER + 0x5E, saved_player_state)
        cpu.write_bytes(PLAYER + 0x24, saved_player_motion)
        configure_ground_sparks(cpu, case, rng)
        for index in range(32):
            sprite = SPRITE_QUEUE + index * 8
            cpu.write8(sprite + 6, 8 if index == 0 and case % 2 == 0 else 0)
            cpu.write8(sprite + 7, 2)
    if name in ("PlayerEffectsUpdate", "EffectsUpdateAll"):
        configure_trail_points(cpu, case, rng)
    cpu.effects_padding = {
        (0x8006BA2C, -50): 0, (0x8006BA2C, -42): 0, (0x8006BA2C, -34): 0,
        0x8006B950: 0, 0x800696EC: 0,
    }
    for offset in (-50, -42, -34, -90, -122, -170):
        cpu.write16(mr.DEFAULT_SP + offset, 0)


def configure_water(cpu: mr.MipsRam, case: int):
    # Nonzero surface input exposes the unconditional height store at 8006BAC0
    # for slow entries. Zero scratch and zero old height hid that delay slot.
    cpu.write16(0x1F8003DE, 0xFAD6)
    cpu.write16(0x1F8003E4, 0)
    cpu.write16(0x800B4D90, 0x1357)
    player = PLAYER
    splash = 0x800BAD78
    scenario = case % 5
    cpu.write_bytes(splash, bytes(8))
    cpu.write16(player + 0x138, 0x1000 if scenario != 3 else 0)
    cpu.write32(player + 0x24, 1000)
    cpu.write32(player + 0x28, 1000)
    cpu.write32(player + 0x2C, 1000)
    cpu.write32(player + 0x30, 0)
    cpu.write32(player + 0x34, 0)
    cpu.write32(player + 0x38, 0)
    cpu.write32(player + 0x12C, 1096 if scenario == 0 else 900 if scenario in (2, 4) else 800)
    # 8006BAC4 reads the full ground height at +12C for entry depth. The
    # later wake checks read the separate halfword at +13C; don't equate them.
    cpu.write16(player + 0x13C, 32767 if scenario == 0 else 900 if scenario in (2, 4) else 800)
    cpu.write32(player + 0x70, 0x40 if scenario in (1, 2) else 0)
    cpu.write32(0x800FC250, 0x40 if scenario in (1, 2) else 0)
    cpu.write32(0x800FC350, 0)
    if scenario == 0:
        cpu.write32(player + 0x34, [16, 17, 200, 0xFFFFFFFF][case % 4])
    elif scenario == 1:
        cpu.write8(splash + 3, 1)
        cpu.write8(splash + 0, 2 + case % 3)
        cpu.write8(splash + 1, 10 + case % 5)
    elif scenario == 2:
        cpu.write8(splash + 3, 1)
        cpu.write32(player + 0x34, 0xFFFFFFE2)
        cpu.write32(player + 0x30, 1 if case & 1 else 0)
    elif scenario == 4:
        cpu.write8(splash + 3, 1)
        cpu.write8(splash + 0, 2)
        cpu.write8(splash + 1, 10 + case % 5)
        cpu.write32(player + 0x30, 1 if case & 1 else 0)
        cpu.write32(player + 0x70, 0x20)
        cpu.write32(0x800FC250, 0x20)
        for index in range(8):
            cpu.write8(TRAIL_POINTS + index * 8 + 6, 1)
        cpu.write16(TRAIL_SEED, 0)
        cpu.write16(0x800AE8F2, 0)

    water_padding = {
        (0x8006BA2C, -50): 0,
        (0x8006BA2C, -42): 0,
        (0x8006BA2C, -34): 0,
        0x8006B950: 0,
        0x800696EC: 0,
    }
    cpu.effects_padding = water_padding
    for offset in (-50, -42, -34, -90, -122, -170):
        cpu.write16(mr.DEFAULT_SP + offset, 0)


def configure_sprite_draw(cpu: mr.MipsRam, case: int, seed: int):
    rng = random.Random(f"sprite-draw/{case}/{seed}")
    camera = 0x800E78C0
    cpu.write_bytes(camera, bytes(32))
    for axis in range(3):
        cpu.write16(camera + 2 * (axis * 3 + axis), 4096)
    cpu.write32(0x800FC354, 0x800EC3D0)
    cpu.write32(0x800FC35C, 0x800EC3D0)
    cpu.write16(TRAIL_SEED, rng.getrandbits(16))
    for index in range(32):
        sprite = SPRITE_QUEUE + index * 8
        cpu.write_bytes(sprite, bytes(8))
        active = ((case + index) % 5 != 0) if case < 24 else rng.choice([False, True])
        age = [1, 2, 7, 16, 255][(case + index) % 5]
        kind = [0, 1, 2, 3, 4, 255][(case + index) % 6]
        cpu.write16(sprite, rng.randrange(0x10000))
        cpu.write16(sprite + 2, rng.randrange(0x10000))
        cpu.write16(sprite + 4, rng.randrange(0x10000))
        cpu.write8(sprite + 6, age if active else 0)
        cpu.write8(sprite + 7, kind)


def configure_random_particles(cpu: mr.MipsRam, case: int, rng: random.Random):
    count_values = [0, 1, 2, 3, 6, 7, 0xFF, 0xFD]
    cpu.write_bytes(SPEC, bytes([case % 7, (case // 3) & 0xFF, 0, 0]))
    cpu.write16(SPEC + 2, [0, 1, 127, 256, 1024, 4095, 0xFFFF][case % 7])
    for axis in range(3):
        cpu.write16(POSITION + axis * 4, rng.randrange(-1600, 1601))
    for slot in range(8):
        entry = RANDOM_PARTICLES + slot * 28
        cpu.write_bytes(entry, bytes(28))
        cpu.write32(entry, SPEC)
        cpu.write8(entry + 25, (case + slot) & 1)
        cpu.write32(entry + 4, POSITION)
        for axis in range(3):
            cpu.write16(entry + 8 + axis * 2, rng.randrange(-300, 301))
            cpu.write16(entry + 16 + axis * 2, rng.randrange(-300, 301))
        cpu.write8(entry + 24, count_values[(case + slot) % len(count_values)])
        cpu.write8(entry + 26, [0, 1, 0x100, 0x2345, 0xFFFF][(case + slot) % 5])


def configure_trail_points(cpu: mr.MipsRam, case: int, rng: random.Random):
    camera = 0x800E78C0
    cpu.write_bytes(camera, bytes(32))
    for axis in range(3):
        cpu.write16(camera + 2 * (axis * 3 + axis), 4096)
    cpu.write32(0x800EC354, 0x800EC3D0)
    cpu.write32(0x800EC35C, 8)
    ages = [0, 1, 2, 8, 31, 64, 128, 255]
    for index in range(8):
        point = TRAIL_POINTS + index * 8
        cpu.write_bytes(point, rng.randbytes(8))
        for axis in range(3):
            cpu.write16(point + axis * 2, rng.randrange(-2000, 2001))
        age = ages[(index + case) % len(ages)] if case < 24 else rng.randrange(256)
        cpu.write8(point + 6, age)


def configure_effect_object(cpu: mr.MipsRam, object: int, case: int, rng: random.Random,
                           active: int = 1, timer: int = 0, spawn_body: int = 0):
    cpu.write_bytes(object, rng.randbytes(0x54))
    for offset in (0, 2, 4, 8, 10, 12, 16, 18, 20, 22, 68, 70, 72):
        value = rng.randrange(-700, 701)
        if offset in (16, 18):
            value = abs(value) if case % 2 == 0 else -abs(value)
        cpu.write16(object + offset, value)
    cpu.write8(object + 14, 1 + (case % 8))
    cpu.write32(object + 28, rng.getrandbits(32))
    cpu.write32(object + 32, 0)
    cpu.write16(object + 32, 4096)
    cpu.write16(object + 40, 4096)
    cpu.write16(object + 48, 4096)
    cpu.write32(object + 64, rng.getrandbits(32))
    cpu.write8(object + 74, active)
    cpu.write8(object + 75, spawn_body)
    cpu.write8(object + 78, timer)
    cpu.write32(object + 80, 0)


def configure_effect_objects(cpu: mr.MipsRam, case: int, rng: random.Random):
    for index in range(16):
        object = 0x800BAF00 + index * 0x54
        alive = (case + index) % 3 != 0
        active = 1 + ((case + index) % 15) if alive else 0
        timer = (case + index) % 4 if alive else 0
        spawn_body = 1 if alive and (case + index) % 4 == 0 else 0
        configure_effect_object(cpu, object, case + index, rng, active, timer, spawn_body)

    # Start with an empty collision list and all pool entries free so a marked
    # object exercises the real allocator and registration code deterministically.
    cpu.write32(0x800B20D8, 0)
    cpu.write32(0x800B20DC, 0)
    for index in range(256):
        slot = 0x800BBDA0 + index * 0x14
        cpu.write16(slot, 0xFFFF)
    for index in range(128):
        cpu.write16(0x800BEDA0 + index * 2, 0xFFFF)

    # One live body with type nibble 6 covers the contact suppression path.
    if case % 4 == 1:
        object = 0x800BAF00 + 15 * 0x54
        body = 0x800BD1A0 + 127 * 0x38
        cpu.write32(object + 80, body)
        cpu.write32(body + 0x2C, 0x60000000)
        cpu.write8(object + 75, 1)


def configure_fx_player_state6(cpu: mr.MipsRam, case: int, rng: random.Random):
    cpu.write16(PLAYER + 0x5E, 6 if case % 3 == 0 else 0x45)
    for offset in (0x24, 0x28, 0x2C, 0x30, 0x34, 0x38):
        cpu.write16(PLAYER + offset, rng.randrange(-2000, 2001))
    cpu.write32(0x800EC354, 0x800EC3D0)
    cpu.write32(0x800EC35C, 8)
    for index in range(8):
        record = 0x800BAEA0 + index * 12
        cpu.write_bytes(record, rng.randbytes(12))
        for offset in (0, 2, 4):
            cpu.write16(record + offset, rng.randrange(-1600, 1601))
        life_values = [0, 1, 2, 7, 32, 127, 128, 255]
        cpu.write8(record + 6, life_values[(case + index) % len(life_values)])
        for offset in (8, 9, 10):
            cpu.write8(record + offset, rng.randrange(256))


def configure_ground_sparks(cpu: mr.MipsRam, case: int, rng: random.Random):
    camera = 0x800E78C0
    cpu.write_bytes(camera, bytes(32))
    for axis in range(3):
        cpu.write16(camera + 2 * (axis * 3 + axis), 4096)
    cpu.write32(0x800EC354, 0x800EC3D0)
    cpu.write32(0x800EC35C, 8)
    for index in range(32):
        record = 0x800BB440 + index * 32
        cpu.write_bytes(record, rng.randbytes(32))
        active = (case + index) % 4 != 0
        life = [0, 1, 2, 6, 12, 64, 128, 255][(case + index) % 8] if active else 0
        cpu.write8(record + 6, life)
        cpu.write8(record + 7, 1 + ((case + index) % 4))
        cpu.write16(record + 0, rng.randrange(-400, 401))
        cpu.write16(record + 2, rng.randrange(-120, 121))
        cpu.write16(record + 4, rng.randrange(-400, 401))
        cpu.write8(record + 8, rng.randrange(-16, 17))
        cpu.write8(record + 9, rng.randrange(-16, 17))
        cpu.write16(record + 10, rng.randrange(-80, 81))
        for offset in (20, 22, 24):
            cpu.write16(record + offset, rng.randrange(-1024, 1025))
        cpu.write16(record + 28, -121 if (case + index) % 2 == 0 else 100)
        cpu.write8(record + 30, rng.randrange(-8, 9))
        cpu.write8(record + 31, rng.randrange(-8, 9))
    if case == 0:
        # Force the visible bounce's lower-velocity clamp and particle spawn path.
        record = 0x800BB440 + 32
        cpu.write8(record + 6, 2)
        cpu.write16(record + 2, 0)
        cpu.write16(record + 10, 200)
        cpu.write16(record + 28, -121)


def configure(cpu: mr.MipsRam, name: str, case: int, seed: int):
    rng = random.Random(f"effects/{name}/{case}/{seed}")
    if name == "PlayerCanExitStage":
        exit_codes = [0, 1, 2, 3, 4, 5, 6, 7, 0x1C]
        states = [0x23, 0x4A, 0x2D, 0x22, 0x45, 0x80]
        classes = [0, 0xD0, 0xE0, 0xF0]
        cpu.write8(0x800F447C, exit_codes[case % len(exit_codes)] if case < 24 else rng.choice(exit_codes))
        cpu.write16(PLAYER + 0x5E, states[(case // len(exit_codes)) % len(states)] if case < 24 else rng.choice(states))
        cpu.write32(PLAYER + 0x70, rng.choice(classes) | rng.getrandbits(24) << 8)
        return ()

    if name == "PlayerWaterEffectsUpdate":
        configure_water(cpu, case)
        return ()

    if name == "SpriteEffectsDraw":
        configure_sprite_draw(cpu, case, seed)
        return ()

    if name == "RandomParticlesUpdate":
        configure_random_particles(cpu, case, rng)
        return ()

    if name == "TrailPointsDraw":
        configure_trail_points(cpu, case, rng)
        return ()

    if name == "EffectObjDraw":
        object = 0x800BAF00
        configure_effect_object(cpu, object, case, rng)
        return (object,)

    if name == "EffectObjsUpdate":
        configure_effect_objects(cpu, case, rng)
        return ()

    if name == "FxPlayerState6Update":
        configure_fx_player_state6(cpu, case, rng)
        return ()

    if name == "GroundSparksUpdateDraw":
        configure_ground_sparks(cpu, case, rng)
        return ()

    if name == "PlayerEffectsUpdate":
        configure_dispatch(cpu, name, case, rng)
        if case % 4 == 1:
            cpu.write8(0x800F447C, 1)
        elif case % 4 == 2:
            cpu.write16(PLAYER + 0x5E, 0x23)
        elif case % 4 == 3:
            cpu.write32(PLAYER + 0x70, 0xE0)
        return ()

    if name == "EffectsUpdateAll":
        configure_dispatch(cpu, name, case, rng)
        return ()

    if name == "PlayerSplash":
        velocities = [0, 16, 17, -16, -17, 200, -200, 0x80000000]
        deltas = [191, 192, 193, -1, 400, 0, 250, -192]
        velocity = velocities[case % len(velocities)]
        delta = deltas[(case // len(velocities) + case % len(deltas)) % len(deltas)]
        cpu.write16(PLAYER + 0x24, rng.getrandbits(16))
        cpu.write16(PLAYER + 0x2C, rng.getrandbits(16))
        cpu.write32(PLAYER + 0x28, 1000)
        cpu.write32(PLAYER + 0x34, velocity & M32)
        cpu.write32(PLAYER + 0x12C, (1000 + delta) & M32)
        cpu.write16(0x800BAD7C, rng.getrandbits(16))
        cpu.write8(0x800BAD78, rng.getrandbits(8))
        cpu.write8(0x800BAD79, rng.getrandbits(8))
        # Both sounds take PlaySfx's early return because sound bank 0 is not loaded.
        cpu.write16(0x800BF670, -1)
        for sound_id in (0x74, 0x9D):
            cpu.write16(0x800B2D08 + sound_id * 8, 0)
        splash_padding = rng.getrandbits(16)
        group_padding = rng.getrandbits(16)
        cpu.write16(mr.DEFAULT_SP - 10, splash_padding)
        cpu.write16(mr.DEFAULT_SP - 90, group_padding)
        cpu.effects_padding = {0x8006B950: splash_padding, 0x800696EC: group_padding}
        return ()

    if name == "PlayerFxTrailUpdate":
        timers = [0, 1, 2, 3, 4, 5, 8, 9, 12, 13, 16, 17]
        cpu.write_bytes(0x800BAD68, rng.randbytes(8))
        cpu.write32(0x800BAD70, timers[case % len(timers)] if case < 24 else rng.getrandbits(32))
        cpu.write16(TRAIL_SEED, [0, 1, 0x00FF, 0x0100, 0xFFFF][case % 5] if case < 10 else rng.getrandbits(16))
        cpu.write8(0x800B1BD0, rng.randrange(16))
        cpu.write8(0x800BAD50, rng.randrange(100))
        return ()

    if name == "TrailsUpdate":
        for index in range(8):
            trail = 0x800F5010 + index * 0x10
            life = [0, 1, 2, 10, 255][(case + index) % 5]
            delay = [0, 1, 2][(case // 2 + index) % 3]
            cpu.write8(trail + 0x0A, life)
            cpu.write8(trail + 0x0B, delay)
            cpu.write16(trail + 0x0C, rng.randrange(0x10000))
            cpu.write16(trail + 0x0E, rng.randrange(0x10000))
            cpu.write16(trail + 0x08, rng.randrange(0x10000))
        return ()

    if name == "PlayerTintEffectUpdate":
        modes = [0, 1, 2, 3, 4]
        timers = [0, 1, 2, 3, 4, 5, 8, 12, 15]
        mode = modes[case % len(modes)]
        timer = timers[(case // len(modes) + case % len(timers)) % len(timers)]
        if mode == 3:
            timer = (case // len(modes)) % 4 + 1
        cpu.write8(0x800BAD7A, mode)
        cpu.write16(0x800BAD7E, timer)
        cpu.write16(TRAIL_SEED, [0, 1, 0x00FF, 0x0100, 0xFFFF][case % 5] if case < 20 else rng.getrandbits(16))
        for offset in (0x24, 0x28, 0x2C):
            cpu.write16(PLAYER + offset, rng.getrandbits(16))
        return ()

    if name == "FlyingMeshesUpdateDraw":
        configure_flying_meshes(cpu, case, rng)
        return ()

    if name == "TrailPointAdd":
        point_states = [1] * 8
        free_index = [7, 6, 0, 3, None, 4, 1, 2][case % 8]
        if free_index is not None:
            point_states[free_index] = 0
        else:
            point_states = [1] * 8
        if case >= 8:
            point_states = [rng.choice([0, 0, 0, 1, 7, 255]) for _ in range(8)]
        for i, active in enumerate(point_states):
            record = TRAIL_POINTS + i * 8
            cpu.write_bytes(record, rng.randbytes(8))
            cpu.write8(record + 6, active)
        cpu.write16(TRAIL_SEED, [0, 1, 0x00FF, 0x0100, 0xFFFF][case % 5] if case < 10 else rng.getrandbits(16))
        cpu.write_bytes(POSITION, rng.randbytes(8))
        return (POSITION,)

    if name == "MeshTransform":
        return (0x801D1000, 0x801D1100, 0x801D1200)

    if name == "MeshDraw":
        return (0x801D1300, 0x801D1400, 0x801D1500)

    cpu.write_bytes(SPEC, rng.randbytes(8))
    cursors = [0, 1, 30, 31, 32, 63, 127, 254, 255]
    cpu.write8(SPRITE_CURSOR, cursors[case % len(cursors)] if case < 18 else rng.randrange(256))
    if name == "SpriteEffectQueue":
        kind = [0, 1, 2, 0x7F, 0xFF][case % 5] if case < 15 else rng.getrandbits(32)
        return (SPEC, mr.signed(kind, 32))
    return (SPEC,)


def invoke(lib, ram: AeRam, name: str, args: tuple[int, ...], context=None) -> int:
    fn = getattr(lib, FUNCS[name][3])
    sfx_ids = []
    spawn_events = []
    if name == "PlayerFxTrailUpdate":
        spawn = SpawnHooks()
        result = fn(C.byref(ram), C.byref(spawn))
    elif name == "PlayerWaterEffectsUpdate":
        padding = context or {}
        spawn_padding = PaddingFn(lambda _user, _ram, function: padding.get(function, 0))
        effects_padding = EffectsPaddingFn(
            lambda _user, _ram, function, offset: padding.get((function, offset), 0))
        spawn = SpawnHooks()
        spawn.position_padding = spawn_padding
        sound = SoundHw()
        effects = EffectsHooks()
        effects.sound = C.cast(C.byref(sound), C.c_void_p)
        effects.LocalPositionPadding = effects_padding
        lib.effects_test_reset()
        result = fn(C.byref(ram), C.byref(spawn), C.byref(effects))
        sfx_ids = [int(lib.effects_test_sfx_id(i)) for i in range(lib.effects_test_sfx_count())]
    elif name == "PlayerSplash":
        padding = context or {}
        callback = PaddingFn(lambda _user, _ram, function: padding.get(function, 0))
        spawn = SpawnHooks()
        spawn.position_padding = callback
        sound = SoundHw()
        lib.effects_test_reset()
        result = fn(C.byref(ram), C.byref(spawn), C.byref(sound))
        sfx_ids = [int(lib.effects_test_sfx_id(i)) for i in range(lib.effects_test_sfx_count())]
    elif name == "TrailsUpdate":
        ring_events = []
        callback = EffectsCallFn(lambda _user, _ram, trail, step, color, intensity:
                                 ring_events.append((trail, step, color, intensity)) or 0)
        effects = EffectsHooks()
        effects.EffectRingDraw = callback
        result = fn(C.byref(ram), C.byref(effects))
        sfx_ids = ring_events
    elif name == "PlayerTintEffectUpdate":
        random_particle_events = []
        callback = EffectsCallFn(lambda _user, _ram, a0, a1, a2, a3:
                                 random_particle_events.append((a0, a1, a2, a3)) or 0)
        effects = EffectsHooks()
        effects.SpawnRandomParticle = callback
        spawn = SpawnHooks()
        result = fn(C.byref(ram), C.byref(spawn), C.byref(effects))
        sfx_ids = random_particle_events
    elif name == "FlyingMeshesUpdateDraw":
        mesh_events = []
        def c_main_bytes(ram_pointer, address, size):
            c_ram = C.cast(ram_pointer, C.POINTER(AeRam)).contents
            offset = address & 0x1FFFFF
            return bytes(c_ram.main[offset:offset + size])
        project_callback = EffectsCallFn(
            lambda _user, c_ram, mesh, matrix, output, _a3:
            mesh_events.append(("GpuProjectMesh", mesh, c_main_bytes(c_ram, matrix, 32), output)) or 0)
        draw_callback = EffectsCallFn(
            lambda _user, _c_ram, ordering_table, vertices, mesh, _a3:
            mesh_events.append(("GpuDrawMesh", ordering_table, vertices, mesh)) or 0)
        effects = EffectsHooks()
        effects.GpuProjectMesh = project_callback
        effects.GpuDrawMesh = draw_callback
        spawn = SpawnHooks()
        lib.effects_test_reset()
        result = fn(C.byref(ram), C.byref(spawn), C.byref(effects))
        sfx_ids = mesh_events
        spawn_events = [(int(lib.effects_test_spawn_type(i)),
                         int(lib.effects_test_spawn_position(i)))
                        for i in range(lib.effects_test_spawn_count())]
    elif name in ("PlayerEffectsUpdate", "EffectsUpdateAll"):
        events = []
        callback_refs = []
        effects = EffectsHooks()
        hook_names = ("Gadget4TrailUpdate", "GadgetFlashUpdate") if name == "EffectsUpdateAll" else ()
        for hook_name in hook_names:
            callback = EffectsCallFn(
                lambda _user, _ram, _a0, _a1, _a2, _a3, event_name=hook_name:
                events.append(event_name) or 0)
            callback_refs.append(callback)
            setattr(effects, hook_name, callback)

        def c_main_bytes(ram_pointer, address, size):
            c_ram = C.cast(ram_pointer, C.POINTER(AeRam)).contents
            offset = address & 0x1FFFFF
            return bytes(c_ram.main[offset:offset + size])

        project_callback = EffectsCallFn(
            lambda _user, c_ram, mesh, matrix, output, _a3:
            events.append(("GpuProjectMesh", mesh, c_main_bytes(c_ram, matrix, 32), output)) or 0)
        draw_callback = EffectsCallFn(
            lambda _user, _c_ram, ordering_table, vertices, mesh, _a3:
            events.append(("GpuDrawMesh", ordering_table, vertices, mesh)) or 0)
        ring_callback = EffectsCallFn(
            lambda _user, _c_ram, trail, step, color, intensity:
            events.append(("EffectRingDraw", trail, step, color, intensity)) or 0)
        callback_refs.extend((project_callback, draw_callback, ring_callback))
        effects.GpuProjectMesh = project_callback
        effects.GpuDrawMesh = draw_callback
        effects.EffectRingDraw = ring_callback
        sprite_callback = EffectsCallFn(
            lambda _user, _ram, sprite, age, kind, _a3:
            events.append(("GpuDrawSprite", sprite, age, kind, 0)) or 0)
        callback_refs.append(sprite_callback)
        effects.GpuDrawSprite = sprite_callback
        def draw_ribbon(_user, c_ram, points, count, style, group):
            vertices = []
            if count <= 8:
                blob = c_main_bytes(c_ram, points, count * 8)
                for index in range(count):
                    offset = index * 8
                    vertices.append(tuple(int.from_bytes(blob[offset + axis * 2:offset + axis * 2 + 2],
                                                         "little", signed=True)
                                          for axis in range(3)))
            events.append(("RibbonDraw", count, style, group, tuple(vertices)))
            return 0
        ribbon_callback = EffectsCallFn(draw_ribbon)
        callback_refs.append(ribbon_callback)
        effects.RibbonDraw = ribbon_callback
        def matrix_callback(event_name):
            callback = EffectsCallFn(lambda _user, _ram, matrix, _a1, _a2, _a3:
                                      events.append((event_name, matrix)) or 0)
            callback_refs.append(callback)
            return callback
        effects.SetRotMatrix = matrix_callback("SetRotMatrix")
        effects.SetTransMatrix = matrix_callback("SetTransMatrix")
        if name == "EffectsUpdateAll":
            def draw_effect_object(_user, c_ram, object, matrix, vertices, _a3):
                c_ram = C.cast(c_ram, C.POINTER(AeRam)).contents
                matrix_offset = (matrix - 0x1F800000) & 0x3FF
                vertex_offset = (vertices - 0x1F800000) & 0x3FF
                events.append(("GpuDrawEffectObj", object,
                               bytes(c_ram.scratch[matrix_offset:matrix_offset + 32]),
                               bytes(c_ram.scratch[vertex_offset:vertex_offset + 16])))
                return 0
            object_draw_callback = EffectsCallFn(draw_effect_object)
            callback_refs.append(object_draw_callback)
            effects.GpuDrawEffectObj = object_draw_callback

            def spark_add_random(_user, c_ram, position, velocity, mode, _a3):
                c_ram = C.cast(c_ram, C.POINTER(AeRam)).contents
                position_offset = (position - 0x1F800000) & 0x3FF
                velocity_offset = (velocity - 0x1F800000) & 0x3FF
                events.append(("SparkAddRandom",
                               bytes(c_ram.scratch[position_offset:position_offset + 8]),
                               bytes(c_ram.scratch[velocity_offset:velocity_offset + 8]),
                               mode))
                return 0
            spark_callback = EffectsCallFn(spark_add_random)
            callback_refs.append(spark_callback)
            effects.SparkAddRandom = spark_callback

            def draw_fx_player_state6(_user, c_ram, record, geometry, triangle, radius):
                c_ram = C.cast(c_ram, C.POINTER(AeRam)).contents
                geometry_offset = (geometry - 0x1F800000) & 0x3FF
                events.append(("GpuDrawFxPlayerState6", record, triangle, radius,
                               bytes(c_ram.scratch[geometry_offset:geometry_offset + 24])))
                return 0
            fx_draw_callback = EffectsCallFn(draw_fx_player_state6)
            callback_refs.append(fx_draw_callback)
            effects.GpuDrawFxPlayerState6 = fx_draw_callback

            padding_context = context or {}
            ground_visibility = iter(padding_context.get("ground_visibility", ()))
            def draw_ground_spark(_user, c_ram, record, matrix, ot_state, scale):
                c_ram = C.cast(c_ram, C.POINTER(AeRam)).contents
                matrix_offset = (matrix - 0x1F800000) & 0x3FF
                matrix_bytes = bytes(c_ram.scratch[matrix_offset:matrix_offset + 32])
                visible = next(ground_visibility, 0)
                events.append(("GpuDrawGroundSpark", record, matrix_bytes,
                               ot_state, scale, visible))
                return visible
            ground_callback = EffectsCallFn(draw_ground_spark)
            callback_refs.append(ground_callback)
            effects.GpuDrawGroundSpark = ground_callback
        def draw_trail_point(_user, c_ram, point, age, geometry, index):
            c_ram = C.cast(c_ram, C.POINTER(AeRam)).contents
            offset = (geometry - 0x1F800000) & 0x3FF
            blob = bytes(c_ram.scratch[offset:offset + 4 * 8])
            vertices = []
            for vertex in range(4):
                base = vertex * 8
                vertices.append(tuple(int.from_bytes(blob[base + axis * 2:base + axis * 2 + 2],
                                                     "little", signed=True)
                                      for axis in range(3)))
            events.append(("GpuDrawTrailPoint", point, age, index, tuple(vertices)))
            return 0
        trail_draw_callback = EffectsCallFn(draw_trail_point)
        callback_refs.append(trail_draw_callback)
        effects.GpuDrawTrailPoint = trail_draw_callback
        padding = ((context or {}).get("padding", {}) if name == "EffectsUpdateAll"
                   else (context or {}))
        spawn_padding = PaddingFn(lambda _user, _ram, function: padding.get(function, 0))
        effects_padding = EffectsPaddingFn(
            lambda _user, _ram, function, offset: padding.get((function, offset), 0))
        spawn = SpawnHooks()
        spawn.position_padding = spawn_padding
        sound = SoundHw()
        effects.sound = C.cast(C.byref(sound), C.c_void_p)
        effects.LocalPositionPadding = effects_padding
        lib.effects_test_reset()
        result = fn(C.byref(ram), C.byref(spawn), C.byref(effects))
        sfx_ids = events
        spawn_events = [(int(lib.effects_test_spawn_type(i)),
                         int(lib.effects_test_spawn_position(i)))
                        for i in range(lib.effects_test_spawn_count())]
    elif name == "SpriteEffectsDraw":
        sprite_events = []
        callback = EffectsCallFn(
            lambda _user, _ram, sprite, age, kind, _a3:
            sprite_events.append(("GpuDrawSprite", sprite, age, kind, 0)) or 0)
        effects = EffectsHooks()
        effects.GpuDrawSprite = callback
        rot_callback = EffectsCallFn(
            lambda _user, _ram, matrix, _a1, _a2, _a3:
            sprite_events.append(("SetRotMatrix", matrix)) or 0)
        trans_callback = EffectsCallFn(
            lambda _user, _ram, matrix, _a1, _a2, _a3:
            sprite_events.append(("SetTransMatrix", matrix)) or 0)
        effects.SetRotMatrix = rot_callback
        effects.SetTransMatrix = trans_callback
        result = fn(C.byref(ram), C.byref(effects))
        sfx_ids = sprite_events
    elif name == "RandomParticlesUpdate":
        ribbon_events = []
        def c_main_bytes(ram_pointer, address, size):
            c_ram = C.cast(ram_pointer, C.POINTER(AeRam)).contents
            offset = address & 0x1FFFFF
            return bytes(c_ram.main[offset:offset + size])
        def draw_ribbon(_user, c_ram, points, count, style, group):
            vertices = []
            if count <= 8:
                blob = c_main_bytes(c_ram, points, count * 8)
                for index in range(count):
                    offset = index * 8
                    vertices.append(tuple(int.from_bytes(blob[offset + axis * 2:offset + axis * 2 + 2],
                                                         "little", signed=True)
                                          for axis in range(3)))
            ribbon_events.append((count, style, group, tuple(vertices)))
            return 0
        callback = EffectsCallFn(draw_ribbon)
        effects = EffectsHooks()
        effects.RibbonDraw = callback
        result = fn(C.byref(ram), C.byref(effects))
        sfx_ids = ribbon_events
    elif name == "TrailPointsDraw":
        draw_events = []
        effects = EffectsHooks()
        def matrix_callback(event_name):
            return EffectsCallFn(lambda _user, _ram, matrix, _a1, _a2, _a3:
                                 draw_events.append((event_name, matrix)) or 0)
        rot_callback = matrix_callback("SetRotMatrix")
        trans_callback = matrix_callback("SetTransMatrix")
        def draw_trail_point(_user, c_ram, point, age, geometry, index):
            c_ram = C.cast(c_ram, C.POINTER(AeRam)).contents
            offset = (geometry - 0x1F800000) & 0x3FF
            blob = bytes(c_ram.scratch[offset:offset + 4 * 8])
            vertices = []
            for vertex in range(4):
                base = vertex * 8
                vertices.append(tuple(int.from_bytes(blob[base + axis * 2:base + axis * 2 + 2],
                                                     "little", signed=True)
                                      for axis in range(3)))
            draw_events.append(("GpuDrawTrailPoint", point, age, index, tuple(vertices)))
            return 0
        draw_callback = EffectsCallFn(draw_trail_point)
        effects.SetRotMatrix = rot_callback
        effects.SetTransMatrix = trans_callback
        effects.GpuDrawTrailPoint = draw_callback
        result = fn(C.byref(ram), C.byref(effects))
        sfx_ids = draw_events
    elif name in ("EffectObjDraw", "EffectObjsUpdate"):
        draw_events = []
        effects = EffectsHooks()
        def matrix_callback(event_name):
            return EffectsCallFn(lambda _user, _ram, matrix, _a1, _a2, _a3:
                                 draw_events.append((event_name, matrix)) or 0)
        rot_callback = matrix_callback("SetRotMatrix")
        trans_callback = matrix_callback("SetTransMatrix")
        def draw_effect_object(_user, c_ram, object, matrix, vertices, _a3):
            c_ram = C.cast(c_ram, C.POINTER(AeRam)).contents
            matrix_offset = (matrix - 0x1F800000) & 0x3FF
            vertex_offset = (vertices - 0x1F800000) & 0x3FF
            draw_events.append(("GpuDrawEffectObj", object,
                                bytes(c_ram.scratch[matrix_offset:matrix_offset + 32]),
                                bytes(c_ram.scratch[vertex_offset:vertex_offset + 16])))
            return 0
        draw_callback = EffectsCallFn(draw_effect_object)
        effects.SetRotMatrix = rot_callback
        effects.SetTransMatrix = trans_callback
        effects.GpuDrawEffectObj = draw_callback
        if name == "EffectObjDraw":
            result = fn(C.byref(ram), C.byref(effects), *args)
        else:
            result = fn(C.byref(ram), C.byref(effects))
        sfx_ids = draw_events
    elif name == "FxPlayerState6Update":
        draw_events = []
        effects = EffectsHooks()
        def mips_data_callback(event_name):
            return EffectsCallFn(lambda _user, _ram, matrix, _a1, _a2, _a3:
                                 draw_events.append((event_name, matrix)) or 0)
        rot_callback = mips_data_callback("SetRotMatrix")
        trans_callback = mips_data_callback("SetTransMatrix")
        def spark_add_random(_user, c_ram, position, velocity, mode, _a3):
            c_ram = C.cast(c_ram, C.POINTER(AeRam)).contents
            position_offset = (position - 0x1F800000) & 0x3FF
            velocity_offset = (velocity - 0x1F800000) & 0x3FF
            draw_events.append(("SparkAddRandom",
                                bytes(c_ram.scratch[position_offset:position_offset + 8]),
                                bytes(c_ram.scratch[velocity_offset:velocity_offset + 8]),
                                mode))
            return 0
        spark_callback = EffectsCallFn(spark_add_random)
        def draw_fx_player_state6(_user, c_ram, record, geometry, triangle, radius):
            c_ram = C.cast(c_ram, C.POINTER(AeRam)).contents
            geometry_offset = (geometry - 0x1F800000) & 0x3FF
            draw_events.append(("GpuDrawFxPlayerState6", record, triangle, radius,
                                bytes(c_ram.scratch[geometry_offset:geometry_offset + 24])))
            return 0
        draw_callback = EffectsCallFn(draw_fx_player_state6)
        effects.SetRotMatrix = rot_callback
        effects.SetTransMatrix = trans_callback
        effects.SparkAddRandom = spark_callback
        effects.GpuDrawFxPlayerState6 = draw_callback
        result = fn(C.byref(ram), C.byref(effects))
        sfx_ids = draw_events
    elif name == "GroundSparksUpdateDraw":
        draw_events = []
        effects = EffectsHooks()
        spawn = SpawnHooks()
        def matrix_callback(event_name):
            return EffectsCallFn(lambda _user, _ram, matrix, _a1, _a2, _a3:
                                 draw_events.append((event_name, matrix)) or 0)
        rot_callback = matrix_callback("SetRotMatrix")
        trans_callback = matrix_callback("SetTransMatrix")
        visible_results = iter(context or [])
        def draw_ground_spark(_user, c_ram, record, matrix, ot_state, scale):
            c_ram = C.cast(c_ram, C.POINTER(AeRam)).contents
            matrix_offset = (matrix - 0x1F800000) & 0x3FF
            matrix_bytes = bytes(c_ram.scratch[matrix_offset:matrix_offset + 32])
            visible = next(visible_results, 0)
            draw_events.append(("GpuDrawGroundSpark", record, matrix_bytes,
                                ot_state, scale, visible))
            return visible
        draw_callback = EffectsCallFn(draw_ground_spark)
        effects.SetRotMatrix = rot_callback
        effects.SetTransMatrix = trans_callback
        effects.GpuDrawGroundSpark = draw_callback
        lib.effects_test_reset()
        result = fn(C.byref(ram), C.byref(spawn), C.byref(effects))
        sfx_ids = draw_events
        spawn_events = [(int(lib.effects_test_spawn_type(i)),
                         int(lib.effects_test_spawn_position(i)))
                        for i in range(lib.effects_test_spawn_count())]
    elif name in ("MeshTransform", "MeshDraw"):
        mesh_events = []
        callback = EffectsCallFn(lambda _user, _ram, a0, a1, a2, _a3:
                                 mesh_events.append((a0, a1, a2, 0)) or 0)
        effects = EffectsHooks()
        if name == "MeshTransform":
            effects.GpuProjectMesh = callback
        else:
            effects.GpuDrawMesh = callback
        result = fn(C.byref(ram), C.byref(effects), *args)
        sfx_ids = mesh_events
    else:
        result = fn(C.byref(ram), *args)
    if lib.ae_ram_failed(C.byref(ram)):
        raise AssertionError(f"{name}: C RAM access failed: {bytes(ram.error_text).split(bytes([0]), 1)[0]!r}")
    return (int(result) if FUNCS[name][2] != "v" else 0), sfx_ids, spawn_events


def compare(name: str, case: int, area: int, cpu: mr.MipsRam, lib, args: tuple[int, ...]):
    ram = copy_to_c(cpu)
    initial_ram, initial_scratch = bytes(cpu.ram), bytes(cpu.scratch)
    cpu.coverage.clear()
    mips_sfx_ids = []
    mips_ring_events = []
    mips_spawn_events = []
    hooks = {}
    patches = {}
    if name == "PlayerSplash":
        def play_sfx(_cpu, registers):
            mips_sfx_ids.append(mr.signed(registers[4], 32))
            return -1
        hooks[0x800877F8] = play_sfx
    elif name == "PlayerWaterEffectsUpdate":
        def play_sfx(_cpu, registers):
            mips_sfx_ids.append(mr.signed(registers[4], 32))
            return -1
        hooks[0x800877F8] = play_sfx
    elif name == "TrailsUpdate":
        def ring_draw(_cpu, registers):
            mips_ring_events.append(tuple(registers[index] for index in range(4, 8)))
            return 0
        hooks[0x8006E5B4] = ring_draw
    elif name == "PlayerTintEffectUpdate":
        def spawn_random_particle(_cpu, registers):
            mips_ring_events.append(tuple(registers[index] for index in range(4, 8)))
            return 0
        hooks[0x80070B70] = spawn_random_particle
    elif name in ("MeshTransform", "MeshDraw"):
        def mesh_renderer(_cpu, registers):
            mips_ring_events.append(tuple(registers[index] for index in range(4, 8)))
            return 0
        hooks[FUNCS[name][0]] = mesh_renderer
    elif name == "FlyingMeshesUpdateDraw":
        attach_gte_oracle(cpu)
        def project_mesh(cpu, registers):
            mips_ring_events.append(("GpuProjectMesh", registers[4],
                                     cpu.read_bytes(registers[5], 32), registers[6]))
            return 0
        def draw_mesh(_cpu, registers):
            mips_ring_events.append(("GpuDrawMesh", registers[4], registers[5], registers[6]))
            return 0
        hooks[0x800160EC] = project_mesh
        hooks[0x80017E5C] = draw_mesh
        def spawn_particle_patch(cpu, registers):
            mips_spawn_events.append((mr.signed(registers[4], 32), registers[5]))
            cpu.write16((registers[16] - 6) & M32, registers[3])
            registers[31] = 0x80071520
            return 0x800698B8
        patches[0x80071518] = spawn_particle_patch
    elif name == "SpriteEffectsDraw":
        attach_gte_oracle(cpu)
        hooks[0x8002FB00] = lambda _cpu, registers: mips_ring_events.append(
            ("SetRotMatrix", registers[4])) or 0
        hooks[0x8002FB30] = lambda _cpu, registers: mips_ring_events.append(
            ("SetTransMatrix", registers[4])) or 0
        def draw_sprite_patch(cpu, registers):
            sprite = (registers[11] - 6) & M32
            mips_ring_events.append(("GpuDrawSprite", sprite,
                                     cpu.read8(sprite + 6), cpu.read8(sprite + 7), 0))
            seed = cpu.read32(0x1F800000)
            for _ in range(3):
                seed = (seed * 5 + 1) & M32
            cpu.write32(0x1F800000, seed)
            return 0x8006DA70
        patches[0x8006D7C0] = draw_sprite_patch
    elif name == "RandomParticlesUpdate":
        def draw_ribbon(cpu, registers):
            count = registers[5]
            vertices = []
            if count <= 8:
                blob = cpu.read_bytes(registers[4], count * 8)
                for index in range(count):
                    offset = index * 8
                    vertices.append(tuple(int.from_bytes(blob[offset + axis * 2:offset + axis * 2 + 2],
                                                         "little", signed=True)
                                          for axis in range(3)))
            mips_ring_events.append((count, registers[6], registers[7], tuple(vertices)))
            return 0
        hooks[0x8006F3D0] = draw_ribbon
    elif name == "TrailPointsDraw":
        hooks[0x8002FB00] = lambda _cpu, registers: mips_ring_events.append(
            ("SetRotMatrix", registers[4])) or 0
        hooks[0x8002FB30] = lambda _cpu, registers: mips_ring_events.append(
            ("SetTransMatrix", registers[4])) or 0
        def draw_trail_point(cpu, registers):
            point = registers[6]
            age = (cpu.read8(point + 6) + 1) & 0xFF
            blob = cpu.read_bytes(0x1F800008, 4 * 8)
            vertices = []
            for vertex in range(4):
                base = vertex * 8
                vertices.append(tuple(int.from_bytes(blob[base + axis * 2:base + axis * 2 + 2],
                                                     "little", signed=True)
                                      for axis in range(3)))
            mips_ring_events.append(("GpuDrawTrailPoint", point, age, (point - TRAIL_POINTS) // 8,
                                     tuple(vertices)))
            return 0x8006F24C
        patches[0x8006F0D8] = draw_trail_point
    elif name in ("EffectObjDraw", "EffectObjsUpdate"):
        attach_gte_oracle(cpu)
        hooks[0x8002FB00] = lambda _cpu, registers: mips_ring_events.append(
            ("SetRotMatrix", registers[4])) or 0
        hooks[0x8002FB30] = lambda _cpu, registers: mips_ring_events.append(
            ("SetTransMatrix", registers[4])) or 0
        def draw_effect_object(cpu, registers):
            object = registers[19]
            matrix = 0x1F80018C
            mips_ring_events.append(("GpuDrawEffectObj", object,
                                     cpu.read_bytes(matrix, 32),
                                     cpu.read_bytes(0x1F8001AC, 16)))
            registers[9] = cpu.read32(0x800EC354)
            return 0x8006FE1C
        patches[0x8006FBC4] = draw_effect_object
    elif name == "FxPlayerState6Update":
        def spark_add_random(cpu, registers):
            mips_ring_events.append(("SparkAddRandom",
                                     cpu.read_bytes(registers[4], 8),
                                     cpu.read_bytes(registers[5], 8),
                                     registers[6]))
            return 0
        hooks[0x8006E0DC] = spark_add_random
        hooks[0x8002FB00] = lambda _cpu, registers: mips_ring_events.append(
            ("SetRotMatrix", registers[4])) or 0
        hooks[0x8002FB30] = lambda _cpu, registers: mips_ring_events.append(
            ("SetTransMatrix", registers[4])) or 0
        def draw_fx_player_state6(cpu, registers):
            record = registers[6]
            geometry = registers[19]
            mips_ring_events.append(("GpuDrawFxPlayerState6", record,
                                     registers[11], registers[7],
                                     cpu.read_bytes(geometry, 24)))
            return 0x8006E528
        patches[0x8006E450] = draw_fx_player_state6
    elif name == "GroundSparksUpdateDraw":
        attach_gte_oracle(cpu)
        def set_rot_matrix(cpu, registers):
            matrix = registers[4]
            mips_ring_events.append(("SetRotMatrix", matrix))
            if getattr(cpu, "gte_backend", None) is not None:
                load_gte_control_matrix(cpu, matrix, 0, 0, 5)
            return 0
        def set_trans_matrix(cpu, registers):
            matrix = registers[4]
            mips_ring_events.append(("SetTransMatrix", matrix))
            if getattr(cpu, "gte_backend", None) is not None:
                load_gte_control_matrix(cpu, matrix, 5, 20, 3)
            return 0
        hooks[0x8002FB00] = set_rot_matrix
        hooks[0x8002FB30] = set_trans_matrix
        visibility_results = []
        def draw_ground_spark(cpu, registers):
            record = (registers[16] - 6) & M32
            matrix = registers[19]
            visible = 1 if registers[2] != 0 else 0
            mips_ring_events.append(("GpuDrawGroundSpark", record,
                                     cpu.read_bytes(matrix, 32), registers[22],
                                     cpu.read8(record + 7), visible))
            visibility_results.append(visible)
            # The native renderer owns both projection and OT packet output.
            # Restore the GTE result words so only the transform scratch remains.
            cpu.scratch[0x40:0x4C] = initial_scratch[0x40:0x4C]
            cpu.scratch[0x60:0x64] = initial_scratch[0x60:0x64]
            if visible:
                registers[11] = cpu.read32(registers[22] + 4)
                return 0x80070A08
            return 0x80070B00
        patches[0x80070764] = draw_ground_spark
        def spawn_particle_patch(cpu, registers):
            position = registers[18]
            registers[5] = position
            mips_spawn_events.append((mr.signed(registers[4], 32), position))
            registers[31] = 0x80070B00
            return 0x800698B8
        patches[0x80070AF8] = spawn_particle_patch
        cpu.ground_spark_visibility = visibility_results
    elif name in ("PlayerEffectsUpdate", "EffectsUpdateAll"):
        dispatch_calls = {
            0x8006DAF8: "Gadget4TrailUpdate",
            0x8006C6D4: "GadgetFlashUpdate",
        }
        active_calls = dispatch_calls.values() if name == "EffectsUpdateAll" else ()
        addresses = {event: address for address, event in dispatch_calls.items()}
        for event in active_calls:
            def dispatch_call(_cpu, _registers, event_name=event):
                mips_ring_events.append(event_name)
                return 0
            hooks[addresses[event]] = dispatch_call

        def ring_draw(_cpu, registers):
            mips_ring_events.append(("EffectRingDraw", *registers[4:8]))
            return 0
        hooks[0x8006E5B4] = ring_draw

        def mesh_renderer(cpu, registers):
            mips_ring_events.append(("GpuProjectMesh", registers[4],
                                     cpu.read_bytes(registers[5], 32), registers[6]))
            return 0
        def mesh_draw(_cpu, registers):
            mips_ring_events.append(("GpuDrawMesh", registers[4], registers[5], registers[6]))
            return 0
        hooks[0x800160EC] = mesh_renderer
        hooks[0x80017E5C] = mesh_draw
        def set_rot_matrix(cpu, registers):
            matrix = registers[4]
            mips_ring_events.append(("SetRotMatrix", matrix))
            if getattr(cpu, "gte_backend", None) is not None:
                load_gte_control_matrix(cpu, matrix, 0, 0, 5)
            return 0
        def set_trans_matrix(cpu, registers):
            matrix = registers[4]
            mips_ring_events.append(("SetTransMatrix", matrix))
            if getattr(cpu, "gte_backend", None) is not None:
                load_gte_control_matrix(cpu, matrix, 5, 20, 3)
            return 0
        hooks[0x8002FB00] = set_rot_matrix
        hooks[0x8002FB30] = set_trans_matrix
        def draw_trail_point(cpu, registers):
            point = registers[6]
            age = (cpu.read8(point + 6) + 1) & 0xFF
            blob = cpu.read_bytes(0x1F800008, 4 * 8)
            vertices = []
            for vertex in range(4):
                base = vertex * 8
                vertices.append(tuple(int.from_bytes(blob[base + axis * 2:base + axis * 2 + 2],
                                                     "little", signed=True)
                                      for axis in range(3)))
            mips_ring_events.append(("GpuDrawTrailPoint", point, age,
                                     (point - TRAIL_POINTS) // 8, tuple(vertices)))
            return 0x8006F24C
        patches[0x8006F0D8] = draw_trail_point
        if name == "EffectsUpdateAll":
            attach_gte_oracle(cpu)
            def draw_effect_object(cpu, registers):
                object = registers[19]
                mips_ring_events.append(("GpuDrawEffectObj", object,
                                         cpu.read_bytes(0x1F80018C, 32),
                                         cpu.read_bytes(0x1F8001AC, 16)))
                # The renderer owns OT packet emission; preserve the current
                # OT cursor in the original function's post-render store.
                registers[9] = cpu.read32(0x800EC354)
                return 0x8006FE1C
            patches[0x8006FBC4] = draw_effect_object

            def spark_add_random(cpu, registers):
                mips_ring_events.append(("SparkAddRandom",
                                         cpu.read_bytes(registers[4], 8),
                                         cpu.read_bytes(registers[5], 8),
                                         registers[6]))
                return 0
            hooks[0x8006E0DC] = spark_add_random

            def draw_fx_player_state6(cpu, registers):
                record = registers[6]
                geometry = registers[19]
                mips_ring_events.append(("GpuDrawFxPlayerState6", record,
                                         registers[11], registers[7],
                                         cpu.read_bytes(geometry, 24)))
                return 0x8006E528
            patches[0x8006E450] = draw_fx_player_state6

            ground_visibility_results = []
            def draw_ground_spark(cpu, registers):
                record = (registers[16] - 6) & M32
                matrix = registers[19]
                visible = 1 if registers[2] != 0 else 0
                mips_ring_events.append(("GpuDrawGroundSpark", record,
                                         cpu.read_bytes(matrix, 32), registers[22],
                                         cpu.read8(record + 7), visible))
                ground_visibility_results.append(visible)
                cpu.scratch[0x40:0x4C] = initial_scratch[0x40:0x4C]
                cpu.scratch[0x60:0x64] = initial_scratch[0x60:0x64]
                if visible:
                    registers[11] = cpu.read32(registers[22] + 4)
                    return 0x80070A08
                return 0x80070B00
            patches[0x80070764] = draw_ground_spark
            def ground_spawn_patch(cpu, registers):
                position = registers[18]
                registers[5] = position
                mips_spawn_events.append((mr.signed(registers[4], 32), position))
                registers[31] = 0x80070B00
                return 0x800698B8
            patches[0x80070AF8] = ground_spawn_patch
            cpu.ground_spark_visibility = ground_visibility_results

            def draw_ribbon(cpu, registers):
                count = registers[5]
                vertices = []
                if count <= 8:
                    blob = cpu.read_bytes(registers[4], count * 8)
                    for index in range(count):
                        offset = index * 8
                        vertices.append(tuple(int.from_bytes(blob[offset + axis * 2:offset + axis * 2 + 2],
                                                             "little", signed=True)
                                              for axis in range(3)))
                mips_ring_events.append(("RibbonDraw", count, registers[6], registers[7],
                                         tuple(vertices)))
                return 0
            hooks[0x8006F3D0] = draw_ribbon
            def spawn_particle_patch(cpu, registers):
                mips_spawn_events.append((mr.signed(registers[4], 32), registers[5]))
                cpu.write16((registers[16] - 6) & M32, registers[3])
                registers[31] = 0x80071520
                return 0x800698B8
            patches[0x80071518] = spawn_particle_patch
            def draw_sprite_patch(cpu, registers):
                sprite = (registers[11] - 6) & M32
                mips_ring_events.append(("GpuDrawSprite", sprite,
                                         cpu.read8(sprite + 6), cpu.read8(sprite + 7), 0))
                seed = cpu.read32(0x1F800000)
                for _ in range(3):
                    seed = (seed * 5 + 1) & M32
                cpu.write32(0x1F800000, seed)
                return 0x8006DA70
            patches[0x8006D7C0] = draw_sprite_patch
    regs = cpu.call(FUNCS[name][0], args, hooks=hooks, patches=patches)
    invoke_context = getattr(cpu, "effects_padding", None)
    if name == "GroundSparksUpdateDraw":
        invoke_context = cpu.ground_spark_visibility
    elif name == "EffectsUpdateAll":
        invoke_context = {"padding": getattr(cpu, "effects_padding", {}),
                          "ground_visibility": cpu.ground_spark_visibility}
    c_result, c_sfx_ids, c_spawn_events = invoke(lib, ram, name, args, invoke_context)
    if name in ("PlayerSplash", "PlayerWaterEffectsUpdate") and mips_sfx_ids != c_sfx_ids:
        raise AssertionError(f"{name} case {case} area {area:02X}: PlaySfx calls {c_sfx_ids} != {mips_sfx_ids}")
    if name in ("TrailsUpdate", "PlayerTintEffectUpdate", "MeshTransform", "MeshDraw",
                "FlyingMeshesUpdateDraw", "RandomParticlesUpdate", "TrailPointsDraw") and mips_ring_events != c_sfx_ids:
        raise AssertionError(f"{name} case {case} area {area:02X}: renderer/hook calls {c_sfx_ids} != {mips_ring_events}")
    if name == "FlyingMeshesUpdateDraw" and mips_spawn_events != c_spawn_events:
        raise AssertionError(f"{name} case {case} area {area:02X}: SpawnParticle calls "
                             f"{c_spawn_events} != {mips_spawn_events}")
    if name in ("PlayerEffectsUpdate", "EffectsUpdateAll") and mips_ring_events != c_sfx_ids:
        raise AssertionError(f"{name} case {case} area {area:02X}: ordered hook/renderer calls "
                             f"{c_sfx_ids} != {mips_ring_events}")
    if name in ("EffectObjDraw", "EffectObjsUpdate") and mips_ring_events != c_sfx_ids:
        raise AssertionError(f"{name} case {case} area {area:02X}: renderer calls "
                             f"{c_sfx_ids} != {mips_ring_events}")
    if name == "FxPlayerState6Update" and mips_ring_events != c_sfx_ids:
        raise AssertionError(f"{name} case {case} area {area:02X}: hook/renderer calls "
                             f"{c_sfx_ids} != {mips_ring_events}")
    if name == "GroundSparksUpdateDraw" and mips_ring_events != c_sfx_ids:
        raise AssertionError(f"{name} case {case} area {area:02X}: renderer calls "
                             f"{c_sfx_ids} != {mips_ring_events}")
    if name == "EffectsUpdateAll" and mips_spawn_events != c_spawn_events:
        raise AssertionError(f"{name} case {case} area {area:02X}: SpawnParticle calls "
                             f"{c_spawn_events} != {mips_spawn_events}")
    if name == "GroundSparksUpdateDraw" and mips_spawn_events != c_spawn_events:
        raise AssertionError(f"{name} case {case} area {area:02X}: SpawnParticle calls "
                             f"{c_spawn_events} != {mips_spawn_events}")
    if name == "SpriteEffectsDraw" and mips_ring_events != c_sfx_ids:
        raise AssertionError(f"{name} case {case} area {area:02X}: renderer calls "
                             f"{c_sfx_ids} != {mips_ring_events}")
    mips_result = mr.signed(regs[2], 32) if FUNCS[name][2] != "v" else 0
    if c_result != mips_result:
        detail = ""
        if name == "PlayerCanExitStage":
            detail = (f" (exit={cpu.read8(0x800F447C):02X}, state={cpu.read16(PLAYER + 0x5E):04X}, "
                      f"flags={cpu.read32(PLAYER + 0x70):08X}, a0={regs[4]:08X}, v1={regs[3]:08X}, "
                      f"path={[f'{pc:08X}' for pc in sorted(cpu.coverage) if 0x8005DCA0 <= pc <= 0x8005DCE0]})")
        raise AssertionError(f"{name} case {case} area {area:02X}: return {c_result} != {mips_result}{detail}")
    mips_final = bytes(cpu.ram)
    c_final = bytes(ram.main)
    stack_offset = STACK_START - 0x80000000
    if mips_final[:stack_offset] != c_final[:stack_offset]:
        limit = next(i for i, (a, b) in enumerate(zip(mips_final[:stack_offset], c_final[:stack_offset])) if a != b)
        diff_addresses = [0x80000000 + i for i, (a, b) in enumerate(
            zip(mips_final[:stack_offset], c_final[:stack_offset])) if a != b][:24]
        detail = (f"MIPS={mips_final[limit:limit + 24].hex()} C={c_final[limit:limit + 24].hex()}")
        detail += f"; diffs={[f'{address:08X}' for address in diff_addresses]}"
        if name == "FlyingMeshesUpdateDraw":
            seed_offset = TRAIL_SEED - 0x80000000
            particle_offset = 0x800B9900 - 0x80000000
            detail += (f"; seed={int.from_bytes(mips_final[seed_offset:seed_offset + 2], 'little'):04X}/"
                       f"{int.from_bytes(c_final[seed_offset:seed_offset + 2], 'little'):04X}"
                       f"; particles MIPS={mips_final[particle_offset:particle_offset + 0x150].hex()}"
                       f" C={c_final[particle_offset:particle_offset + 0x150].hex()}")
        raise AssertionError(f"{name} case {case} area {area:02X}: RAM differs at {0x80000000 + limit:08X}; {detail}")
    # The original MIPS stack occupies the final 4 KiB of main RAM, which is excluded.
    if bytes(cpu.scratch) != bytes(ram.scratch):
        raise AssertionError(f"{name} case {case} area {area:02X}: scratchpad differs")
    return set(cpu.coverage)


def compile_mutation(original: Path, old: str, new: str, label: str) -> Path:
    source = original.read_text(encoding="utf-8")
    if old not in source:
        raise AssertionError(f"mutation {label}: source anchor not found")
    with tempfile.TemporaryDirectory(prefix="effects_mut_") as td:
        # The compiler consumes the source before the temporary directory exits;
        # copy the DLL out to the repository's ignored build-output directory.
        mutated_source = Path(td) / original.name
        mutated_source.write_text(source.replace(old, new, 1), encoding="utf-8")
        if original.name == "ae_effects_core.c":
            return build(core_source=mutated_source, label=f"effects_mut_{label}")
        if original.name == "ae_effects_trails.c":
            return build(trails_source=mutated_source, label=f"effects_mut_{label}")
        if original.name == "ae_effects_mesh.c":
            return build(mesh_source=mutated_source, label=f"effects_mut_{label}")
        if original.name == "ae_effects_flying.c":
            return build(flying_source=mutated_source, label=f"effects_mut_{label}")
        if original.name == "ae_effects_dispatch.c":
            return build(dispatch_source=mutated_source, label=f"effects_mut_{label}")
        if original.name == "ae_effects_water.c":
            return build(water_source=mutated_source, label=f"effects_mut_{label}")
        if original.name == "ae_effects_sprite_draw.c":
            return build(sprite_source=mutated_source, label=f"effects_mut_{label}")
        if original.name == "ae_effects_random.c":
            return build(random_source=mutated_source, label=f"effects_mut_{label}")
        if original.name == "ae_effects_trail_draw.c":
            return build(trail_draw_source=mutated_source, label=f"effects_mut_{label}")
        if original.name == "ae_effects_objects.c":
            return build(objects_source=mutated_source, label=f"effects_mut_{label}")
        if original.name == "ae_effects_player_state.c":
            return build(player_state_source=mutated_source, label=f"effects_mut_{label}")
        if original.name == "ae_effects_ground_sparks.c":
            return build(ground_sparks_source=mutated_source, label=f"effects_mut_{label}")
        return build(player_source=mutated_source, label=f"effects_mut_{label}")


def mutation_case(name: str, mutation: int, seed: int):
    cpu = area_cpu(area_ids()[0])
    if name == "PlayerCanExitStage":
        cpu.write8(0x800F447C, 5 if mutation == 0 else 3)
        cpu.write16(PLAYER + 0x5E, 0x45 if mutation == 0 else 0x23)
        cpu.write32(PLAYER + 0x70, 0)
    elif name == "TrailPointAdd":
        for i in range(8):
            point = TRAIL_POINTS + i * 8
            cpu.write_bytes(point, bytes([0xA5 + i]) * 8)
            cpu.write8(point + 6, 0 if (mutation == 0 or i == 7) else 1)
        cpu.write16(TRAIL_SEED, 0x1234)
        cpu.write_bytes(POSITION, bytes.fromhex("0102030405060708"))
    elif name == "PlayerSplash":
        cpu.write16(PLAYER + 0x24, 0x1234)
        cpu.write16(PLAYER + 0x2C, 0x5678)
        cpu.write32(PLAYER + 0x28, 1000)
        cpu.write32(PLAYER + 0x34, 16 if mutation == 0 else 17)
        cpu.write32(PLAYER + 0x12C, 1191)
        cpu.write16(0x800BAD7C, 0xABCD)
        cpu.write8(0x800BAD78, 0xA5)
        cpu.write8(0x800BAD79, 0x5A)
        splash_padding, group_padding = 0x4567, 0x89AB
        cpu.write16(mr.DEFAULT_SP - 10, splash_padding)
        cpu.write16(mr.DEFAULT_SP - 90, group_padding)
        cpu.effects_padding = {0x8006B950: splash_padding, 0x800696EC: group_padding}
    elif name == "PlayerFxTrailUpdate":
        cpu.write_bytes(0x800BAD68, bytes.fromhex("6400C8002C01EFBE"))
        cpu.write32(0x800BAD70, 1 if mutation == 0 else 2)
        cpu.write16(TRAIL_SEED, 0x1234)
        cpu.write8(0x800B1BD0, 7)
        cpu.write8(0x800BAD50, 0)
    elif name == "TrailsUpdate":
        trail = 0x800F5010
        cpu.write8(trail + 0x0A, 10 + mutation)
        cpu.write8(trail + 0x0B, 0)
        cpu.write16(trail + 0x0C, 1234)
        cpu.write16(trail + 0x0E, 500)
    elif name == "PlayerTintEffectUpdate":
        cpu.write8(0x800BAD7A, 1 if mutation == 0 else 2)
        cpu.write16(0x800BAD7E, 8)
        cpu.write16(TRAIL_SEED, 0x1234)
        cpu.write16(PLAYER + 0x24, 0x1234)
        cpu.write16(PLAYER + 0x28, 0x5678)
        cpu.write16(PLAYER + 0x2C, 0x9ABC)
    elif name in ("EffectObjDraw", "EffectObjsUpdate"):
        configure_effect_objects(cpu, mutation + 1, random.Random(seed + mutation))
    elif name in ("MeshTransform", "MeshDraw"):
        pass
    elif name == "FlyingMeshesUpdateDraw":
        pass
    else:
        cpu.write_bytes(SPEC, bytes.fromhex("1020304050607080"))
        cpu.write8(SPRITE_CURSOR, 30 if name == "SpriteEffectQueue" and mutation == 1 else 31)
    args = configure(cpu, name, 3 + mutation, seed)
    # The deterministic mutation fixture takes precedence over the generic setup.
    if name == "PlayerCanExitStage":
        cpu.write8(0x800F447C, 5 if mutation == 0 else 3)
        cpu.write16(PLAYER + 0x5E, 0x45 if mutation == 0 else 0x23)
        cpu.write32(PLAYER + 0x70, 0)
        args = ()
    elif name == "TrailPointAdd":
        for i in range(8):
            point = TRAIL_POINTS + i * 8
            cpu.write_bytes(point, bytes([0xA5 + i]) * 8)
            cpu.write8(point + 6, 0 if (mutation == 0 or i == 7) else 1)
        cpu.write16(TRAIL_SEED, 0x1234)
        cpu.write_bytes(POSITION, bytes.fromhex("0102030405060708"))
        args = (POSITION,)
    elif name == "PlayerSplash":
        cpu.write16(PLAYER + 0x24, 0x1234)
        cpu.write16(PLAYER + 0x2C, 0x5678)
        cpu.write32(PLAYER + 0x28, 1000)
        cpu.write32(PLAYER + 0x34, 16 if mutation == 0 else 17)
        cpu.write32(PLAYER + 0x12C, 1191)
        cpu.write16(0x800BAD7C, 0xABCD)
        cpu.write8(0x800BAD78, 0xA5)
        cpu.write8(0x800BAD79, 0x5A)
        splash_padding, group_padding = 0x4567, 0x89AB
        cpu.write16(mr.DEFAULT_SP - 10, splash_padding)
        cpu.write16(mr.DEFAULT_SP - 90, group_padding)
        cpu.effects_padding = {0x8006B950: splash_padding, 0x800696EC: group_padding}
        args = ()
    elif name == "PlayerFxTrailUpdate":
        cpu.write_bytes(0x800BAD68, bytes.fromhex("6400C8002C01EFBE"))
        cpu.write32(0x800BAD70, 1 if mutation == 0 else 2)
        cpu.write16(TRAIL_SEED, 0x1234)
        cpu.write8(0x800B1BD0, 7)
        cpu.write8(0x800BAD50, 0)
        args = ()
    elif name == "TrailsUpdate":
        trail = 0x800F5010
        cpu.write8(trail + 0x0A, 10 + mutation)
        cpu.write8(trail + 0x0B, 0)
        cpu.write16(trail + 0x0C, 1234)
        cpu.write16(trail + 0x0E, 500)
        args = ()
    elif name == "PlayerTintEffectUpdate":
        cpu.write8(0x800BAD7A, 1 if mutation == 0 else 2)
        cpu.write16(0x800BAD7E, 8)
        cpu.write16(TRAIL_SEED, 0x1234)
        cpu.write16(PLAYER + 0x24, 0x1234)
        cpu.write16(PLAYER + 0x28, 0x5678)
        cpu.write16(PLAYER + 0x2C, 0x9ABC)
        args = ()
    elif name == "EffectObjDraw":
        object = 0x800BAF00
        configure_effect_object(cpu, object, mutation + 1,
                                random.Random(seed + mutation), active=1)
        args = (object,)
    elif name == "EffectObjsUpdate":
        configure_effect_objects(cpu, mutation + 1,
                                 random.Random(seed + mutation))
        args = ()
    elif name in ("MeshTransform", "MeshDraw"):
        args = (0x801D1000, 0x801D1100, 0x801D1200) if name == "MeshTransform" else \
               (0x801D1300, 0x801D1400, 0x801D1500)
    elif name == "FlyingMeshesUpdateDraw":
        record = 0x800BB920 + 0x44
        cpu.write8(record + 0x3C, 0xFF if mutation == 0 else 2)
        cpu.write8(record + 0x3D, 1 if mutation == 0 else 0)
        cpu.write16(record + 0x32, 20)
        cpu.write16(record + 0x38, 5)
        cpu.write16(record + 0x3E, 25)
        args = ()
    elif name in ("PlayerEffectsUpdate", "EffectsUpdateAll"):
        configure_dispatch(cpu, name, 0, random.Random(seed + mutation))
        cpu.write8(0x800F447C, 5)
        cpu.write16(PLAYER + 0x5E, 0x45)
        cpu.write32(PLAYER + 0x70, 0)
        if name == "PlayerEffectsUpdate":
            cpu.write32(0x800BAD70, 1)
        args = ()
    elif name == "FxPlayerState6Update":
        configure_fx_player_state6(cpu, 0, random.Random(seed + mutation))
        args = ()
    elif name == "GroundSparksUpdateDraw":
        configure_ground_sparks(cpu, 0, random.Random(seed + mutation))
        args = ()
    elif name == "PlayerWaterEffectsUpdate":
        configure_water(cpu, 0 if mutation in (0, 2, 3) else 4)
        if mutation == 0:
            cpu.write32(PLAYER + 0x34, 20)
        elif mutation == 1:
            cpu.write8(0x800BAD79, 10)
        elif mutation == 2:
            cpu.write32(PLAYER + 0x34, 16)
        else:
            cpu.write32(PLAYER + 0x34, 20)
        args = ()
    elif name == "SpriteEffectsDraw":
        configure_sprite_draw(cpu, 2 + mutation, seed)
        sprite = SPRITE_QUEUE
        cpu.write8(sprite + 6, 12)
        cpu.write8(sprite + 7, 1 if mutation == 0 else 2)
        args = ()
    elif name == "RandomParticlesUpdate":
        configure_random_particles(cpu, 0, random.Random(seed + mutation))
        entry = RANDOM_PARTICLES
        cpu.write8(entry + 24, 3)
        cpu.write8(entry + 25, mutation)
        args = ()
    elif name == "SpriteEffectQueue":
        cpu.write_bytes(SPEC, bytes.fromhex("1020304050607080"))
        cpu.write8(SPRITE_CURSOR, 30 if mutation == 1 else 31)
        args = (SPEC, 4)
    else:
        cpu.write_bytes(SPEC, bytes.fromhex("1020304050607080"))
        cpu.write8(SPRITE_CURSOR, 31)
        args = (SPEC,)
    return cpu, args


MUTATIONS = {
    "PlayerCanExitStage": [
        ("if (exit_code < 3u || exit_code > 5u)", "if (exit_code < 3u || exit_code > 4u)"),
        ("player_state == 0x23u", "player_state == 0x22u"),
    ],
    "TrailPointAdd": [
        ("AE_TRAIL_POINT_LIFE_BASE + ((seed >> 8) & 3u)", "AE_TRAIL_POINT_LIFE_BASE + 1u"),
        ("AE_TRAIL_POINT_COUNT - 1; index >= 0", "AE_TRAIL_POINT_COUNT - 2; index >= 0"),
    ],
    "SpriteEffectQueue": [
        ("AE_SPRITE_FX_INITIAL_AGE);", "AE_SPRITE_FX_INITIAL_AGE - 1u);"),
        ("(cursor + 1u) & (AE_SPRITE_FX_COUNT - 1u)", "(cursor + 1u) & (AE_SPRITE_FX_COUNT - 2u)"),
    ],
    "SpriteEffectQueue2": [
        ("ae_SpriteEffectQueue(r, spec, 2);", "ae_SpriteEffectQueue(r, spec, 1);"),
        ("ae_SpriteEffectQueue(r, spec, 2);", "ae_SpriteEffectQueue(r, spec, 0);"),
    ],
    "PlayerFxTrailUpdate": [
        ("((seed & 0xFFFFu) >> 9)", "((seed & 0xFFFFu) >> 8)"),
        ("if ((timer & 3u) == 0u)", "if ((timer & 3u) == 1u)"),
    ],
    "PlayerSplash": [
        ("velocity_y < 0x80000000u && velocity_y >= 17u", "velocity_y < 0x80000000u && velocity_y >= 16u"),
        ("depth_delta < 192u", "depth_delta < 191u"),
    ],
    "TrailsUpdate": [
        ("(life * 6u) & 0xFFu", "(life * 5u) & 0xFFu"),
        ("step * 7u + width", "step * 6u + width"),
    ],
    "PlayerTintEffectUpdate": [
        ("timer--;\n    ae_wr16(r, state + AE_SPLASH_STATE_TINT_TIMER, timer);",
         "timer -= 2u;\n    ae_wr16(r, state + AE_SPLASH_STATE_TINT_TIMER, timer);"),
        ("- horizontal_radius;", "- horizontal_radius + 1u;"),
    ],
    "MeshTransform": [
        ("mesh, matrix, output);", "mesh, output, matrix);"),
        ("GpuProjectMesh\", mesh, matrix, output", "GpuProjectMesh\", mesh, output, matrix"),
    ],
    "MeshDraw": [
        ("\"GpuDrawMesh\", ordering_table,\n                                     screen_vertices, mesh",
         "\"GpuDrawMesh\", screen_vertices,\n                                     ordering_table, mesh"),
        ("screen_vertices, mesh);", "mesh, screen_vertices);"),
    ],
    "FlyingMeshesUpdateDraw": [
        ("if (state == 0) continue;", "if (state <= 0) continue;"),
        ("+ AE_FM_GRAVITY", "+ (AE_FM_GRAVITY + 1u)"),
    ],
    "PlayerEffectsUpdate": [
        ("ae_PlayerFxTrailUpdate(r, spawn);", "(void)spawn;"),
        ("if (!ae_PlayerCanExitStage(r)) return;", "if (ae_PlayerCanExitStage(r)) return;"),
    ],
    "EffectsUpdateAll": [
        ("    ae_EffectObjsUpdate(r, hooks);", "    ae_RandomParticlesUpdate(r, hooks);"),
        ("    ae_PlayerEffectsUpdate(r, spawn, hooks);", "    return;"),
    ],
    "PlayerWaterEffectsUpdate": [
        ("ae_SpawnParticle(r, spawn, 0x101, AE_G_WATER_TEMP_POSITION);",
         "ae_SpawnParticle(r, spawn, 0x100, AE_G_WATER_TEMP_POSITION);"),
        ("player_class != 0x40u && splash_timer >= 10u",
         "player_class != 0x40u && splash_timer > 10u"),
        ("ae_wr16(r, AE_G_WATER_ENTRY_EFFECT_POSITION, surface_y);",
         "if (water_signed32(velocity_y) >= 17) ae_wr16(r, AE_G_WATER_ENTRY_EFFECT_POSITION, surface_y);"),
        ("player + WATER_PLAYER_GROUND_HEIGHT", "player + WATER_PLAYER_GROUND_Y"),
    ],
    "SpriteEffectsDraw": [
        ("if (kind != 1u)", "if (kind == 1u)"),
        ("sprite + AE_SPRITE_FX_AGE, age - 1u", "sprite + AE_SPRITE_FX_AGE, age - 2u"),
    ],
    "RandomParticlesUpdate": [
        ("if (step > 0 && step < life)", "if (step > 0 && step <= life)"),
        ("ae_wr8(r, life_address, life_byte - 1u);", "ae_wr8(r, life_address, life_byte - 2u);"),
    ],
    "TrailPointsDraw": [
        ("age - 1u);", "age - 2u);"),
        ("AE_TRAIL_DRAW_RADIUS_PER_LIFE = 12", "AE_TRAIL_DRAW_RADIUS_PER_LIFE = 13"),
    ],
    "EffectObjDraw": [
        ("ae_rds16(r, object + AE_EFFECT_OBJECT_ROTATION));",
         "ae_rds16(r, object + AE_EFFECT_OBJECT_ROTATION) + 1);") ,
        ("ae_rd16(r, object + AE_EFFECT_OBJECT_RADIUS_Y));",
         "ae_rd16(r, object + AE_EFFECT_OBJECT_RADIUS_X));"),
    ],
    "EffectObjsUpdate": [
        ("ae_wr8(r, object + AE_EFFECT_OBJECT_ACTIVE, active - 1u);",
         "ae_wr8(r, object + AE_EFFECT_OBJECT_ACTIVE, active - 2u);"),
        ("int32_t damped_x = x_velocity * 7;", "int32_t damped_x = x_velocity * 6;"),
    ],
    "FxPlayerState6Update": [
        ("ae_rd8(r, record + AE_FX_PLAYER_STATE6_LIFE) - 1u);",
         "ae_rd8(r, record + AE_FX_PLAYER_STATE6_LIFE) - 2u);"),
        ("position[axis] + velocity[axis] * 6", "position[axis] + velocity[axis] * 5"),
    ],
    "GroundSparksUpdateDraw": [
        ("ae_rd16(r, record + AE_GROUND_SPARK_ANGLE_X) + angular_step);",
         "ae_rd16(r, record + AE_GROUND_SPARK_ANGLE_X) + angular_step + 1);") ,
        ("AE_GROUND_SPARK_RENDER_SCALE = 64", "AE_GROUND_SPARK_RENDER_SCALE = 32"),
    ],
}


def run_mutations(seed: int) -> int:
    source_names = {"PlayerSplash": "ae_effects_player.c", "PlayerFxTrailUpdate": "ae_effects_player.c",
                    "PlayerTintEffectUpdate": "ae_effects_player.c",
                    "TrailsUpdate": "ae_effects_trails.c",
                    "MeshTransform": "ae_effects_mesh.c", "MeshDraw": "ae_effects_mesh.c",
                    "FlyingMeshesUpdateDraw": "ae_effects_flying.c",
                    "PlayerEffectsUpdate": "ae_effects_dispatch.c",
                    "EffectsUpdateAll": "ae_effects_dispatch.c",
                    "PlayerWaterEffectsUpdate": "ae_effects_water.c",
                    "SpriteEffectsDraw": "ae_effects_sprite_draw.c",
                    "RandomParticlesUpdate": "ae_effects_random.c",
                    "TrailPointsDraw": "ae_effects_trail_draw.c",
                    "EffectObjDraw": "ae_effects_objects.c",
                    "EffectObjsUpdate": "ae_effects_objects.c",
                    "FxPlayerState6Update": "ae_effects_player_state.c",
                    "GroundSparksUpdateDraw": "ae_effects_ground_sparks.c"}
    caught = 0
    for name, variants in MUTATIONS.items():
        source = ROOT / "src/effects" / source_names.get(name, "ae_effects_core.c")
        for i, (old, new) in enumerate(variants):
            dll = compile_mutation(source, old, new, f"{name}_{i}")
            mutant = bind(dll)
            cpu, args = mutation_case(name, i, seed)
            if name in ("EffectsUpdateAll", "FxPlayerState6Update", "GroundSparksUpdateDraw"):
                try:
                    compare(name, i, area_ids()[0], cpu, mutant, args)
                except AssertionError:
                    caught += 1
                    continue
                raise AssertionError(f"mutation escaped: {name} mutation {i + 1}")
            ram = copy_to_c(cpu)
            before_ram, before_scratch = bytes(cpu.ram), bytes(cpu.scratch)
            original_sfx_ids = []
            original_ring_events = []
            original_spawn_events = []
            hooks = {}
            patches = {}
            if name == "PlayerSplash":
                def play_sfx(_cpu, registers):
                    original_sfx_ids.append(mr.signed(registers[4], 32))
                    return -1
                hooks[0x800877F8] = play_sfx
            elif name == "PlayerWaterEffectsUpdate":
                def play_sfx(_cpu, registers):
                    original_sfx_ids.append(mr.signed(registers[4], 32))
                    return -1
                hooks[0x800877F8] = play_sfx
            elif name == "SpriteEffectsDraw":
                attach_gte_oracle(cpu)
                hooks[0x8002FB00] = lambda _cpu, registers: original_ring_events.append(
                    ("SetRotMatrix", registers[4])) or 0
                hooks[0x8002FB30] = lambda _cpu, registers: original_ring_events.append(
                    ("SetTransMatrix", registers[4])) or 0
                def draw_sprite_patch(cpu, registers):
                    sprite = (registers[11] - 6) & M32
                    original_ring_events.append(("GpuDrawSprite", sprite,
                                                 cpu.read8(sprite + 6), cpu.read8(sprite + 7), 0))
                    seed = cpu.read32(0x1F800000)
                    for _ in range(3):
                        seed = (seed * 5 + 1) & M32
                    cpu.write32(0x1F800000, seed)
                    return 0x8006DA70
                patches[0x8006D7C0] = draw_sprite_patch
            elif name == "RandomParticlesUpdate":
                def draw_ribbon(cpu, registers):
                    count = registers[5]
                    vertices = []
                    if count <= 8:
                        blob = cpu.read_bytes(registers[4], count * 8)
                        for index in range(count):
                            offset = index * 8
                            vertices.append(tuple(int.from_bytes(blob[offset + axis * 2:offset + axis * 2 + 2],
                                                                 "little", signed=True)
                                                  for axis in range(3)))
                    original_ring_events.append((count, registers[6], registers[7], tuple(vertices)))
                    return 0
                hooks[0x8006F3D0] = draw_ribbon
            elif name == "TrailPointsDraw":
                hooks[0x8002FB00] = lambda _cpu, registers: original_ring_events.append(
                    ("SetRotMatrix", registers[4])) or 0
                hooks[0x8002FB30] = lambda _cpu, registers: original_ring_events.append(
                    ("SetTransMatrix", registers[4])) or 0
                def draw_trail_point(cpu, registers):
                    point = registers[6]
                    age = (cpu.read8(point + 6) + 1) & 0xFF
                    blob = cpu.read_bytes(0x1F800008, 4 * 8)
                    vertices = []
                    for vertex in range(4):
                        base = vertex * 8
                        vertices.append(tuple(int.from_bytes(blob[base + axis * 2:base + axis * 2 + 2],
                                                             "little", signed=True)
                                              for axis in range(3)))
                    original_ring_events.append(("GpuDrawTrailPoint", point, age,
                                                 (point - TRAIL_POINTS) // 8, tuple(vertices)))
                    return 0x8006F24C
                patches[0x8006F0D8] = draw_trail_point
            elif name in ("EffectObjDraw", "EffectObjsUpdate"):
                attach_gte_oracle(cpu)
                hooks[0x8002FB00] = lambda _cpu, registers: original_ring_events.append(
                    ("SetRotMatrix", registers[4])) or 0
                hooks[0x8002FB30] = lambda _cpu, registers: original_ring_events.append(
                    ("SetTransMatrix", registers[4])) or 0
                def draw_effect_object(cpu, registers):
                    object = registers[19]
                    original_ring_events.append(("GpuDrawEffectObj", object,
                                                 cpu.read_bytes(0x1F80018C, 32),
                                                 cpu.read_bytes(0x1F8001AC, 16)))
                    registers[9] = cpu.read32(0x800EC354)
                    return 0x8006FE1C
                patches[0x8006FBC4] = draw_effect_object
            elif name == "PlayerTintEffectUpdate":
                def spawn_random_particle(_cpu, registers):
                    original_ring_events.append(tuple(registers[index] for index in range(4, 8)))
                    return 0
                hooks[0x80070B70] = spawn_random_particle
            elif name == "TrailsUpdate":
                def ring_draw(_cpu, registers):
                    original_ring_events.append(tuple(registers[index] for index in range(4, 8)))
                    return 0
                hooks[0x8006E5B4] = ring_draw
            elif name in ("MeshTransform", "MeshDraw"):
                def mesh_renderer(_cpu, registers):
                    original_ring_events.append(tuple(registers[index] for index in range(4, 8)))
                    return 0
                hooks[FUNCS[name][0]] = mesh_renderer
            elif name == "FlyingMeshesUpdateDraw":
                attach_gte_oracle(cpu)
                def project_mesh(cpu, registers):
                    original_ring_events.append(("GpuProjectMesh", registers[4],
                                                 cpu.read_bytes(registers[5], 32), registers[6]))
                    return 0
                def draw_mesh(_cpu, registers):
                    original_ring_events.append(("GpuDrawMesh", registers[4], registers[5], registers[6]))
                    return 0
                hooks[0x800160EC] = project_mesh
                hooks[0x80017E5C] = draw_mesh
                def spawn_particle_patch(cpu, registers):
                    original_spawn_events.append((mr.signed(registers[4], 32), registers[5]))
                    cpu.write16((registers[16] - 6) & M32, registers[3])
                    registers[31] = 0x80071520
                    return 0x800698B8
                patches[0x80071518] = spawn_particle_patch
            elif name in ("PlayerEffectsUpdate", "EffectsUpdateAll"):
                dispatch_calls = {
                    0x8006FEE8: "EffectObjsUpdate",
                    0x8006DAF8: "Gadget4TrailUpdate",
                    0x8006E1C0: "FxPlayerState6Update",
                    0x80070560: "GroundSparksUpdateDraw",
                    0x8006C6D4: "GadgetFlashUpdate",
                }
                active_calls = dispatch_calls.values() if name == "EffectsUpdateAll" else ()
                addresses = {event: address for address, event in dispatch_calls.items()}
                for event in active_calls:
                    def dispatch_call(_cpu, _registers, event_name=event):
                        original_ring_events.append(event_name)
                        return 0
                    hooks[addresses[event]] = dispatch_call
                def ring_draw(_cpu, registers):
                    original_ring_events.append(("EffectRingDraw", *registers[4:8]))
                    return 0
                hooks[0x8006E5B4] = ring_draw
                def project_mesh(cpu, registers):
                    original_ring_events.append(("GpuProjectMesh", registers[4],
                                                 cpu.read_bytes(registers[5], 32), registers[6]))
                    return 0
                def draw_mesh(_cpu, registers):
                    original_ring_events.append(("GpuDrawMesh", registers[4], registers[5], registers[6]))
                    return 0
                hooks[0x800160EC] = project_mesh
                hooks[0x80017E5C] = draw_mesh
                hooks[0x8002FB00] = lambda _cpu, registers: original_ring_events.append(
                    ("SetRotMatrix", registers[4])) or 0
                hooks[0x8002FB30] = lambda _cpu, registers: original_ring_events.append(
                    ("SetTransMatrix", registers[4])) or 0
                def draw_trail_point(cpu, registers):
                    point = registers[6]
                    age = (cpu.read8(point + 6) + 1) & 0xFF
                    blob = cpu.read_bytes(0x1F800008, 4 * 8)
                    vertices = []
                    for vertex in range(4):
                        base = vertex * 8
                        vertices.append(tuple(int.from_bytes(blob[base + axis * 2:base + axis * 2 + 2],
                                                             "little", signed=True)
                                              for axis in range(3)))
                    original_ring_events.append(("GpuDrawTrailPoint", point, age,
                                                 (point - TRAIL_POINTS) // 8, tuple(vertices)))
                    return 0x8006F24C
                patches[0x8006F0D8] = draw_trail_point
                if name == "EffectsUpdateAll":
                    attach_gte_oracle(cpu)
                    def draw_ribbon(cpu, registers):
                        count = registers[5]
                        vertices = []
                        if count <= 8:
                            blob = cpu.read_bytes(registers[4], count * 8)
                            for index in range(count):
                                offset = index * 8
                                vertices.append(tuple(int.from_bytes(blob[offset + axis * 2:offset + axis * 2 + 2],
                                                                     "little", signed=True)
                                                      for axis in range(3)))
                        original_ring_events.append(("RibbonDraw", count, registers[6], registers[7],
                                                     tuple(vertices)))
                        return 0
                    hooks[0x8006F3D0] = draw_ribbon
                    def draw_sprite_patch(cpu, registers):
                        sprite = (registers[11] - 6) & M32
                        original_ring_events.append(("GpuDrawSprite", sprite,
                                                     cpu.read8(sprite + 6), cpu.read8(sprite + 7), 0))
                        seed = cpu.read32(0x1F800000)
                        for _ in range(3):
                            seed = (seed * 5 + 1) & M32
                        cpu.write32(0x1F800000, seed)
                        return 0x8006DA70
                    patches[0x8006D7C0] = draw_sprite_patch
            original_regs = cpu.call(FUNCS[name][0], args, hooks=hooks, patches=patches)
            original_result = mr.signed(original_regs[2], 32) if FUNCS[name][2] != "v" else 0
            mutant_result, mutant_sfx_ids, mutant_spawn_events = invoke(
                mutant, ram, name, args, getattr(cpu, "effects_padding", None))
            stack_offset = STACK_START - 0x80000000
            event_names = {"PlayerTintEffectUpdate", "TrailsUpdate", "MeshTransform", "MeshDraw",
                           "SpriteEffectsDraw",
                           "FlyingMeshesUpdateDraw", "PlayerEffectsUpdate", "EffectsUpdateAll",
                           "RandomParticlesUpdate", "TrailPointsDraw",
                           "EffectObjDraw", "EffectObjsUpdate"}
            differs = (original_result != mutant_result or
                       bytes(cpu.ram[:stack_offset]) != bytes(ram.main[:stack_offset]) or
                       bytes(cpu.scratch) != bytes(ram.scratch) or
                       original_sfx_ids != mutant_sfx_ids or
                       (name in event_names and original_ring_events != mutant_sfx_ids) or
                       original_spawn_events != mutant_spawn_events)
            if not differs:
                raise AssertionError(f"mutation escaped: {name} mutation {i + 1}")
            caught += 1
    return caught


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=int, default=40, help="deterministic cases per function")
    parser.add_argument("--seed", type=int, default=94423)
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--no-mutate", action="store_true")
    args = parser.parse_args()
    if args.cases < 1:
        parser.error("--cases must be positive")

    started = time.time()
    reg.declare("stack_window", "801FF000..80200000")
    reg.declare("unreachable", "SpriteEffectsDraw 8006D7C0..8006DA6C")
    reg.declare("unreachable", "TrailPointsDraw 8006F0D8..8006F248")
    reg.declare("unreachable", "EffectObjDraw 8006FBC4..8006FE18")
    reg.declare("unreachable", "EffectObjDraw 8006FB58")
    reg.declare("unreachable", "EffectObjDraw 8006FBA8")
    reg.declare("unreachable", "FxPlayerState6Update 8006E450..8006E524")
    reg.declare("unreachable", "GroundSparksUpdateDraw 80070764..80070A04")
    reg.declare("unreachable", "GroundSparksUpdateDraw 80070AF8..80070AFC")
    areas = area_ids()
    library = bind(build())
    bases = {}
    for area in areas:
        cpu = area_cpu(area)
        bases[area] = cpu.snapshot()

    counts = {name: 0 for name in FUNCS}
    coverage: set[int] = set()
    failures = []
    for name in FUNCS:
        for case in range(args.cases):
            area = areas[(case + list(FUNCS).index(name)) % len(areas)]
            cpu = area_cpu(area)
            cpu.restore(bases[area])
            function_args = configure(cpu, name, case, args.seed)
            try:
                coverage.update(compare(name, case, area, cpu, library, function_args))
            except Exception as error:  # noqa: BLE001
                failures.append(f"{name} case {case}: {error}")
                break
            counts[name] += 1

    items = [(name, addr, size, True) for name, (addr, size, _kind, _cname) in FUNCS.items()]
    summary = reg.coverage_summary(items, coverage)
    for name, info in summary.items():
        if info["uncovered_count"] or info["stale_declared"]:
            failures.append(f"{name}: uncovered={info['uncovered']} stale={info['stale_declared']}")
    caught = 0
    if not args.no_mutate and not failures:
        try:
            caught = run_mutations(args.seed)
        except Exception as error:  # noqa: BLE001
            failures.append(f"mutation testing: {error}")

    passed = not failures
    for failure in failures:
        print("FAIL:", failure)
    if args.coverage or failures:
        reg.print_coverage(summary)
    print(f"effects leaf checks: {sum(counts.values())} cases across {len(areas)} real areas; "
          f"mutations caught={caught}/{sum(map(len, MUTATIONS.values()))}; "
          f"elapsed={time.time() - started:.1f}s")
    if args.cases == 40 and args.seed == 94423 and not args.no_mutate:
        reg.write_results(counts, summary, passed, extra={"mutations_caught": caught,
                                                        "area_ids": [f"{a:02X}" for a in areas]},
                          function_names=list(FUNCS))
    for name, n in counts.items():
        print(f"  {name:24s} {n:4d} cases, {summary[name]['executed']}/{summary[name]['total']} instructions")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
