"""Differential checks for the 42 shared Zako movement/state helpers.

The MIPS side executes the original EXE entries on real area images.  C calls
the hand-ported AeRam functions; only the already named native/BIOS boundaries
remain hooked.  Run from the repository venv:
  .venv/Scripts/python.exe tools/mips/check_zakohelpers.py [--coverage]
  .venv/Scripts/python.exe tools/mips/check_zakohelpers.py --mutate
"""
import argparse
import ctypes as C
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402
import check_zako1 as z1  # noqa: E402

P, U32, I32 = C.c_void_p, C.c_uint32, C.c_int32
STACK_LO, STACK_HI = 0x801FF000, 0x80200000
OBJECT, PLAYER, LINKED = 0x800E6960, 0x800EC1E0, 0x800E6B00
ANIM_TABLE = 0x80185000
MUTATION_SELECTOR, MUTATION_MARKER = 0x801F0000, 0x800F46F0
AREA_IDS = (1, 2, 3, 6)
EXTRA_COVERAGE = {}

# address, instruction bytes, return kind, takes AeZako1Hooks, C argument kinds
# (u = uint32_t, s = int32_t); the hook pointer is passed only to the C port.
FUNCS = {
    "ZakoWalkToTarget":             (0x80052D70, 140, "v", 1, "u"),
    "ZakoStateHurt":                (0x80052DFC, 216, "v", 1, "u"),
    "ZakoEnterIdleWait":            (0x80052ED4, 84,  "v", 0, "u"),
    "ObjFootstepFx":                (0x80052F28, 192, "v", 1, "u"),
    "ZakoChasePlayer":              (0x80052FE8, 232, "v", 1, "u"),
    "ZakoEnterDeath":               (0x800530D0, 132, "v", 1, "u"),
    "ObjTurnTowardsPlayerXZ":       (0x80053154, 264, "v", 1, "u"),
    "ObjSetSpeed96":                (0x8005325C, 8,   "v", 0, "us"),
    "ZakoStateWatch":               (0x80053264, 160, "v", 1, "u"),
    "Zako_WanderBounce":            (0x80053304, 120, "v", 1, "u"),
    "Zako_InitDefaultStats":        (0x8005337C, 196, "v", 0, "u"),
    "Zako_RecoverFromStun":         (0x80053440, 168, "v", 0, "u"),
    "ZakoReturnIfFar":              (0x800534E8, 104, "v", 0, "u"),
    "ObjTargetBeyondRange":         (0x80055370, 92,  "s", 0, "us"),
    "ObjWalkOnGround":              (0x80055460, 144, "v", 1, "u"),
    "ObjSetVelAlongDir":            (0x80055524, 180, "v", 1, "uuss"),
    "ObjSetVelTowardPlayer":         (0x800555D8, 156, "v", 1, "us"),
    "ObjSetVelTowardTarget":         (0x80055674, 164, "v", 1, "uss"),
    "ObjCooldown":                  (0x8005588C, 56,  "s", 0, "us"),
    "ObjWalkToPoint":               (0x80055AA0, 252, "s", 1, "usss"),
    "ObjChasePlayer":               (0x80055C00, 124, "v", 1, "us"),
    "ObjWanderStep":                (0x80055D28, 152, "v", 1, "uss"),
    "ObjWanderOrReturn":            (0x80055DC0, 172, "s", 1, "uss"),
    "ObjFleeFromPlayerInArea":       (0x80056128, 216, "v", 1, "us"),
    "ObjSetVelFromFacing":          (0x80056200, 56,  "v", 0, "us"),
    "ObjTryStepOnGround":           (0x80056344, 156, "s", 1, "us"),
    "ObjFallToGround":              (0x8005648C, 96,  "s", 1, "u"),
    "ObjPlayerOutOfYRange":         (0x800564EC, 96,  "s", 0, "us"),
    "ObjPlayerNearPointXZ":         (0x8005654C, 120, "s", 0, "ussss"),
    "ObjPlayerInCone":              (0x800565C4, 176, "s", 0, "ussss"),
    "ObjPlayerInConeFromPos2":      (0x800567B8, 164, "s", 0, "usss"),
    "ObjTurnToPlayer":              (0x8005696C, 88,  "v", 1, "u"),
    "ObjAccelerate":                (0x80056DC4, 140, "s", 0, "usss"),
    "ObjClampFrame":                (0x800570D4, 32,  "v", 0, "us"),
    "PlayerWithinRadiusXZ":         (0x800570F4, 76,  "s", 0, "sss"),
    "ObjAimAtPlayer":               (0x800571C4, 264, "v", 0, "u"),
    "ObjFallAndBounce":             (0x800572CC, 212, "s", 1, "u"),
    "ObjSpawnParticleAt":           (0x80057504, 84,  "v", 1, "ussss"),
    "Zako_Nop":                     (0x80058134, 8,   "v", 0, "us"),
    "ObjAddHitBody":                (0x8005813C, 228, "u", 1, "ussssss"),
    "ObjAddHitBodyIfPlayerNear":     (0x80058220, 204, "u", 1, "ussssss"),
    "ObjGetLinkedObjState":         (0x800582EC, 56,  "s", 0, "us"),
}

BASE_ARGS = {
    "ZakoWalkToTarget": (OBJECT,), "ZakoStateHurt": (OBJECT,),
    "ZakoEnterIdleWait": (OBJECT,), "ObjFootstepFx": (OBJECT,),
    "ZakoChasePlayer": (OBJECT,), "ZakoEnterDeath": (OBJECT,),
    "ObjTurnTowardsPlayerXZ": (OBJECT,), "ObjSetSpeed96": (OBJECT, -42),
    "ZakoStateWatch": (OBJECT,), "Zako_WanderBounce": (OBJECT,),
    "Zako_InitDefaultStats": (OBJECT,), "Zako_RecoverFromStun": (OBJECT,),
    "ZakoReturnIfFar": (OBJECT,), "ObjTargetBeyondRange": (OBJECT, 20),
    "ObjWalkOnGround": (OBJECT,),
    "ObjSetVelAlongDir": (OBJECT, OBJECT + 0x94, 12, 128),
    "ObjSetVelTowardPlayer": (OBJECT, 12),
    "ObjSetVelTowardTarget": (OBJECT, 12, 128),
    "ObjCooldown": (OBJECT, -1), "ObjWalkToPoint": (OBJECT, 120, 150, 8),
    "ObjChasePlayer": (OBJECT, 12), "ObjWanderStep": (OBJECT, 8, 128),
    "ObjWanderOrReturn": (OBJECT, 8, 128),
    "ObjFleeFromPlayerInArea": (OBJECT, 8),
    "ObjSetVelFromFacing": (OBJECT, 8), "ObjTryStepOnGround": (OBJECT, 64),
    "ObjFallToGround": (OBJECT,), "ObjPlayerOutOfYRange": (OBJECT, 100),
    "ObjPlayerNearPointXZ": (OBJECT, 100, 100, 140, 256),
    "ObjPlayerInCone": (OBJECT, 90, 1000, 20, 30),
    "ObjPlayerInConeFromPos2": (OBJECT, 90, 1000, 1536),
    "ObjTurnToPlayer": (OBJECT,), "ObjAccelerate": (OBJECT, 40, 8, -4),
    "ObjClampFrame": (OBJECT, 10), "PlayerWithinRadiusXZ": (100, 140, 256),
    "ObjAimAtPlayer": (OBJECT,), "ObjFallAndBounce": (OBJECT,),
    "ObjSpawnParticleAt": (OBJECT, 4, 0, 0, 0),
    "Zako_Nop": (OBJECT, 90),
    "ObjAddHitBody": (OBJECT, 120, 110, 80, 150, 1, 64),
    "ObjAddHitBodyIfPlayerNear": (OBJECT, 120, 100, 100, 140, 1, 64),
    "ObjGetLinkedObjState": (OBJECT, 0),
}

# These per-entry cases take distinct branch paths. Most functions have three
# cases: a normal state, a boundary/alternate state, and one signed edge input.
CASE_OVERRIDES = {
    "ZakoWalkToTarget": [
        {"target": (104, 144), "ground": 100},
        {"target": (500, -400), "ground": -100},
    ],
    "ZakoStateHurt": [{"link_state": 6}, {"link_state": 0, "player": (110, 100, 150)},
                       {"link_state": 6, "global_cooldown": 5}],
    "ZakoEnterIdleWait": [{"global_cooldown": 5}],
    "ObjFootstepFx": [{"frame": 0}, {"frame": 3, "type": 4}, {"frame": 13}],
    "ZakoChasePlayer": [{"player": (110, 100, 150)},
                         {"player": (900, 100, 900), "speed": 0xFFFF},
                         {"type": 0x49, "action": 1},
                         {"action": 0, "speed": 8}],
    "ZakoEnterDeath": [{"player": (100, 100, 140)}, {"player": (-100, 200, 32767)}],
    "ObjTurnTowardsPlayerXZ": [{"player": (100, 100, 140)}, {"player": (300, 100, -200)},
                                {"type": 0x49},
                                {"ground": 250, "pos_y": 100, "velocity_y": 0}],
    "ZakoStateWatch": [{"player": (110, 100, 150)}, {"player": (900, 100, 900), "watch_adjust": -6}],
    "Zako_WanderBounce": [{"target": (110, 150), "ground": 100}, {"target": (500, 500), "ground": -100}],
    "Zako_InitDefaultStats": [{"type": 14}, {"type": 3}, {"type": 0x49},
                               {"type": 4}, {"type": 15}],
    "Zako_RecoverFromStun": [{"flags": 0x00100100, "type": 0x49, "player": (300, 100, 100)},
                              {"flags": 0x00100100, "type": 3, "player": (900, 100, 900)}],
    "ZakoReturnIfFar": [{"target": (500, 500), "state": 4},
                        {"target": (500, 500), "state": 11}],
    "ObjTargetBeyondRange": [{"target": (500, -400)}, {"target": (110, 150), "velocity": (8, 0, -8)}],
    "ObjWalkOnGround": [{"ground": 100}, {"ground": -100}],
    "ObjSetVelAlongDir": [{"direction": (4096, 0, 0)}, {"direction": (-1200, 0, 3900)},
                           {"direction": (0, 0, 0), "speed": -32768}],
    "ObjSetVelTowardPlayer": [{"player": (100, 100, 140)}, {"player": (110, 100, 150)}],
    "ObjSetVelTowardTarget": [{"target": (110, 150)}, {"target": (-1200, 32767), "speed": -32768}],
    "ObjCooldown": [{"cooldown": 0}, {"cooldown": 12},
                     {"cooldown": -1, "cooldown_byte": 0}],
    "ObjWalkToPoint": [{"point": (100, 140), "radius": 8}, {"point": (120, 140), "radius": 32}],
    "ObjChasePlayer": [{"player": (100, 100, 140)}, {"player": (110, 100, 150), "speed": -32768}],
    "ObjWanderStep": [{"seed": 0}, {"seed": 0xFFFF, "speed": -32768}],
    "ObjWanderOrReturn": [{"target": (500, 500)}, {"target": (110, 150), "wander_seed": 0}],
    "ObjFleeFromPlayerInArea": [{"target": (110, 150), "player": (110, 100, 150)},
                                {"target": (500, 500), "player": (-300, 0, 700)}],
    "ObjSetVelFromFacing": [{"speed": -32768}, {"speed": 32767}],
    "ObjTryStepOnGround": [{"ground": 100}, {"ground": -100, "pos_y": 100}],
    "ObjFallToGround": [{"ground": 0, "pos_y": -10, "velocity_y": 5},
                        {"ground": 100, "pos_y": 100, "velocity_y": 5}],
    "ObjPlayerOutOfYRange": [{"player": (100, 1636, 140)}, {"player": (100, 1637, 140)},
                             {"flags": 0x104, "player": (100, 5000, 140)},
                             {"player": (100, 0, 140), "y": 2000}],
    "ObjPlayerNearPointXZ": [{"player": (100, 1636, 140)}, {"player": (100, 1637, 140)},
                             {"player": (100, 100, 140), "radius": 0}],
    "ObjPlayerInCone": [{"direction": (100, 0)}, {"direction": (-100, 0), "dot": 0}],
    "ObjPlayerInConeFromPos2": [{"player": (110, 100, 150)}, {"player": (100, 2000, 140)}],
    "ObjTurnToPlayer": [{"player": (100, 100, 140)}, {"player": (300, 100, -200)}],
    "ObjAccelerate": [{"action": 1, "speed": 0x100, "accel": 20},
                       {"action": 0, "speed": 8, "decel": -20},
                       {"action": 1, "speed": 0x7FFF, "accel": 0x7FFF},
                       {"action": 1, "speed": 100, "accel": 20, "target_speed": 1}],
    "ObjClampFrame": [{"frame": 9, "limit": 10}, {"frame": 10, "limit": 10}],
    "PlayerWithinRadiusXZ": [{"player": (100, 100, 140), "radius": 0},
                             {"player": (101, 100, 140), "radius": 2}],
    "ObjAimAtPlayer": [{"player": (100, 100, 140)},
                        {"player": (-100, 100, 500)},
                        {"player": (100, 100, -1000)}],
    "ObjFallAndBounce": [{"ground": 100, "pos_y": 100, "velocity_y": 0, "flags": 0x100},
                         {"ground": 100, "pos_y": 80, "velocity_y": 0, "flags": 0x120},
                         {"ground": 100, "pos_y": 80, "velocity_y": 0,
                          "flags": 0x120, "heading": -3000}],
    "ObjSpawnParticleAt": [{"offset": (0, 0, 0)}, {"offset": (-32768, 32767, 1024)}],
    "ObjAddHitBody": [{"player": (100, 100, 140), "pool_free": True}],
    "ObjAddHitBodyIfPlayerNear": [{"player": (100, 100, 140), "pool_free": True},
                                  {"player": (1000, 100, 1000)}],
    "ObjGetLinkedObjState": [{"link_state": 6}, {"link_state": 0}],
}


def build(source=None, tag="zakohelpers"):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{time.time_ns()}.dll"
    sources = [
        "src/core/ae_ram.c", "src/core/ae_sdk.c", "src/core/ae_sdk_matrix.c",
        "src/core/ae_pose.c", "src/player/native_player.c", "src/bodies/ae_bodies.c",
        "src/world/ae_world.c", "src/effects/ae_spawn.c", "src/sound/ae_sound.c",
        "src/world/ae_zako_init.c", "src/world/ae_zako1.c",
        str(source or ROOT / "src/world/ae_zakohelpers.c"),
    ]
    subprocess.run([shutil.which("gcc"), "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-Isrc/world", "-shared", "-static-libgcc", *sources,
                    "-o", str(dll), "-lm"], cwd=ROOT, check=True)
    return dll


def u32(value):
    return int(value) & 0xFFFFFFFF


def s16(value):
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def area_grid(cpu, member):
    """Point the EXE world globals at the real area's height grid."""
    blob = (ROOT / f"extract/members/{member:03X}.bin").read_bytes()
    area_offset = int.from_bytes(blob[0x4D0:0x4D4], "little")
    grid_offset = area_offset + 4 * int.from_bytes(blob[area_offset + 4:area_offset + 8], "little")
    width = int.from_bytes(blob[grid_offset:grid_offset + 2], "little")
    depth = int.from_bytes(blob[grid_offset + 2:grid_offset + 4], "little")
    collider_count = int.from_bytes(blob[grid_offset + 4:grid_offset + 8], "little")
    cells = grid_offset + 8
    heights = cells + width * depth * 12 + collider_count * 4
    cpu.write32(0x800F44E4, mr.AREA_BASE + area_offset)
    cpu.write16(0x800EBFA0, width)
    cpu.write16(0x800EBFA2, depth)
    cpu.write32(0x800EBFA8, mr.AREA_BASE + cells)
    cpu.write32(0x800EBFB4, mr.AREA_BASE + heights)


class Harness:
    def __init__(self, dll):
        members = dump_areas.area_members()
        self.coverage = set()
        self.cpus = {}
        self.bases = {}
        oracle_cpu = None
        for area in AREA_IDS:
            member = members[area]
            cpu = mr.MipsRam.from_extract(area=ROOT / f"extract/members/{member:03X}.bin")
            if oracle_cpu is None:
                cpu.attach_gte_oracle()
                oracle_cpu = cpu
            else:
                cpu.gte_backend = oracle_cpu.gte_backend
                cpu.gte_state = oracle_cpu.gte_state
                cpu._gte_lib = oracle_cpu._gte_lib
            cpu.coverage = self.coverage
            area_grid(cpu, member)
            self.cpus[area] = cpu
            self.bases[area] = cpu.snapshot()

        reg.declare("stack_window", "801FF000..80200000")
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0
        self.set_c(dll)

    def set_c(self, dll):
        self.c = z1.CSide(dll)
        self.c.spawn_hooks.world = C.cast(C.pointer(self.c.world_hooks), P)
        for name, (address, size, ret, takes_hooks, kinds) in FUNCS.items():
            fn = getattr(self.c.dll, "ae_" + name)
            fn.argtypes = [P] + ([C.POINTER(z1.Zako1Hooks)] if takes_hooks else []) + [
                I32 if kind == "s" else U32 for kind in kinds]
            fn.restype = {"v": None, "s": I32, "u": U32}[ret]

    def setup(self, mem, name, scenario, seed):
        rng = random.Random(seed)
        # Vary preserved object bytes, then replace pointer/control fields with
        # valid records so every branch observes a real, deterministic fixture.
        for base, size in ((OBJECT, 0xE4), (PLAYER, 0xE4), (LINKED, 0xE4)):
            for off in range(size):
                mem.wr8(base + off, rng.getrandbits(8))
        for address in range(STACK_LO, STACK_HI, 4):
            mem.wr32(address, 0)
        for slot in range(16):
            mem.wr16(ANIM_TABLE + slot * 4, 0)
            mem.wr16(ANIM_TABLE + slot * 4 + 2, 20 + slot)

        # Object and player state shared by the movement helpers.
        mem.wr32(OBJECT + 0x24, 100)
        mem.wr32(OBJECT + 0x28, 100)
        mem.wr32(OBJECT + 0x2C, 140)
        mem.wr16(OBJECT + 0x44, 90)
        mem.wr16(OBJECT + 0x46, 228)
        mem.wr16(OBJECT + 0x48, 1)
        mem.wr16(OBJECT + 0x4A, 0xC103)
        mem.wr16(OBJECT + 0x5C, 4)
        mem.wr16(OBJECT + 0x60, 0x1000)
        mem.wr16(OBJECT + 0x64, 0)
        mem.wr8(OBJECT + 0x7E, 3)
        mem.wr8(OBJECT + 0x7F, 0)
        mem.wr8(OBJECT + 0x80, 0)
        mem.wr8(OBJECT + 0x81, 5)
        mem.wr8(OBJECT + 0x82, 0)
        mem.wr8(OBJECT + 0x83, 1)
        mem.wr16(OBJECT + 0x86, 100)
        mem.wr32(OBJECT + 0x88, ANIM_TABLE)
        mem.wr8(OBJECT + 0x8F, 0)
        mem.wr16(OBJECT + 0x94, 4)
        mem.wr16(OBJECT + 0x96, 0)
        mem.wr16(OBJECT + 0x98, -4)
        mem.wr16(OBJECT + 0x9A, 120)
        mem.wr16(OBJECT + 0x9C, 100)
        mem.wr16(OBJECT + 0x9E, 150)
        mem.wr16(OBJECT + 0xA0, 50)
        mem.wr16(OBJECT + 0xA2, 0)
        mem.wr16(OBJECT + 0xA6, 256)
        mem.wr16(OBJECT + 0xA8, 0x2000)
        mem.wr16(OBJECT + 0xAC, 0)
        mem.wr16(OBJECT + 0xAE, 0)
        mem.wr16(OBJECT + 0xB0, 91)
        mem.wr32(OBJECT + 0xB4, LINKED)
        mem.wr32(OBJECT + 0xB8, 0)
        mem.wr8(OBJECT + 0xC8, 10)
        mem.wr8(OBJECT + 0xC9, 3)
        mem.wr8(OBJECT + 0xCC, 0xFF)
        mem.wr8(OBJECT + 0xCD, 2)
        mem.wr8(OBJECT + 0xDC, 1)
        mem.wr32(OBJECT + 0xE0, 0x100)
        mem.wr16(LINKED + 0x2C, 0)
        mem.wr32(LINKED + 0x2C, 0x60000000)

        mem.wr32(PLAYER + 0x24, 110)
        mem.wr32(PLAYER + 0x28, 100)
        mem.wr32(PLAYER + 0x2C, 150)
        mem.wr16(PLAYER + 0x60, 0x1000)
        mem.wr16(PLAYER + 0x64, 0)

        # Ground cached by default; scenarios below intentionally clear bit 8
        # for the real-area GetSolidHeight hook path.
        mem.wr16(OBJECT + 0x86, 100)
        mem.wr16(0x800F45E0, 0xFFFF)
        mem.wr16(0x800F45E8, 0)
        mem.wr8(0x800E77ED, 0)
        mem.wr8(0x800E77EA, 0)
        mem.wr8(0x800E77EB, 0)
        mem.wr8(0x800E77EC, 0)
        mem.wr8(0x800E77F1, 0)
        mem.wr16(0x800AE8F0, seed & 0xFFFF)
        mem.wr16(0x800AE8F2, (seed ^ 0xA55A) & 0xFFFF)

        # Sound requests remain on the original unavailable-bank return; no
        # SPU device callback is needed for this logic-only batch.
        for off in range(0x400): mem.wr8(0x800BF670 + off, 0xFF)
        for off in range(0x400): mem.wr8(0x800B2D08 + off, 0)

        # Valid free body slots/pool make body creation deterministic.
        z1.reset_collision_slots(mem)
        for index in range(128): mem.wr16(0x800BEDA0 + 2 * index, 0xFFFF)

        cfg = (CASE_OVERRIDES.get(name, [])[scenario - 1]
               if scenario > 0 and scenario - 1 < len(CASE_OVERRIDES.get(name, [])) else {})
        args = list(BASE_ARGS[name])
        if "flags" in cfg: mem.wr32(OBJECT + 0xE0, cfg["flags"])
        if "state" in cfg: mem.wr8(OBJECT + 0x80, cfg["state"])
        if "type" in cfg: mem.wr8(OBJECT + 0x7E, cfg["type"])
        if "action" in cfg: mem.wr16(OBJECT + 0xE2, cfg["action"])
        if "cooldown_byte" in cfg: mem.wr8(OBJECT + 0xC9, cfg["cooldown_byte"])
        if "global_cooldown" in cfg: mem.wr8(0x800E77ED, cfg["global_cooldown"])
        if "heading" in cfg: mem.wr16(OBJECT + 0xAC, cfg["heading"])
        if name == "ZakoChasePlayer" and "speed" in cfg:
            mem.wr16(OBJECT + 0xA8, cfg["speed"])
        if "link_state" in cfg: mem.wr32(LINKED + 0x2C, (cfg["link_state"] & 0xF) << 28)
        if "target" in cfg:
            mem.wr16(OBJECT + 0x9A, cfg["target"][0]); mem.wr16(OBJECT + 0x9E, cfg["target"][1])
        if "player" in cfg:
            for off, value in zip((0x24, 0x28, 0x2C), cfg["player"]): mem.wr32(PLAYER + off, value)
        if "ground" in cfg:
            mem.wr16(OBJECT + 0x86, cfg["ground"])
        if "pos_y" in cfg: mem.wr32(OBJECT + 0x28, cfg["pos_y"])
        if "velocity_y" in cfg: mem.wr16(OBJECT + 0x96, cfg["velocity_y"])
        if "velocity" in cfg:
            for off, value in zip((0x94, 0x96, 0x98), cfg["velocity"]): mem.wr16(OBJECT + off, value)
        if "frame" in cfg: mem.wr16(OBJECT + 0x5C, cfg["frame"])
        if "timer" in cfg: mem.wr16(OBJECT + 0xAE, cfg["timer"])
        if "watch_adjust" in cfg: mem.wr8(0x800E77EB, cfg["watch_adjust"])
        if "seed" in cfg: mem.wr16(0x800AE8F2, cfg["seed"])
        if "wander_seed" in cfg: mem.wr16(0x800AE8F0, cfg["wander_seed"])
        if "pool_free" in cfg:
            for index in range(128): mem.wr16(0x800BEDA0 + 2 * index, 0xFFFF)

        if name == "ObjSetVelAlongDir" and "direction" in cfg:
            direction = OBJECT + 0x94
            for off, value in zip((0, 2, 4), cfg["direction"]): mem.wr16(direction + off, value)
        if name == "ObjCooldown": args[1] = cfg.get("cooldown", args[1])
        if name == "ObjPlayerOutOfYRange": args[1] = cfg.get("y", args[1])
        if name == "ObjWalkToPoint":
            point = cfg.get("point", (args[1], args[2]))
            args[1], args[2] = point
            args[3] = cfg.get("radius", args[3])
        if name in ("ObjSetVelAlongDir", "ObjSetVelTowardPlayer", "ObjSetVelTowardTarget",
                    "ObjChasePlayer", "ObjWanderStep", "ObjWanderOrReturn",
                    "ObjFleeFromPlayerInArea", "ObjSetVelFromFacing"):
            if "speed" in cfg: args[-1 if name in ("ObjSetVelTowardPlayer", "ObjChasePlayer", "ObjSetVelFromFacing", "ObjFleeFromPlayerInArea") else -2] = cfg["speed"]
        if name == "ObjSetVelAlongDir" and "speed" in cfg: args[2] = cfg["speed"]
        if name in ("ObjSetVelAlongDir", "ObjSetVelTowardTarget", "ObjWanderStep", "ObjWanderOrReturn") and "turn" in cfg:
            args[-1] = cfg["turn"]
        if name in ("ObjPlayerOutOfYRange",) and "y" in cfg: args[1] = cfg["y"]
        if name == "ObjPlayerNearPointXZ":
            if "radius" in cfg: args[-1] = cfg["radius"]
        if name == "ObjPlayerInCone":
            if "dot" in cfg: args[2] = cfg["dot"]
            if "direction" in cfg: args[3], args[4] = cfg["direction"]
        if name == "ObjAccelerate":
            if "speed" in cfg: mem.wr16(OBJECT + 0xA8, cfg["speed"])
            if "target_speed" in cfg: args[1] = cfg["target_speed"]
            if "accel" in cfg: args[2] = cfg["accel"]
            if "decel" in cfg: args[3] = cfg["decel"]
        if name == "ObjClampFrame":
            if "frame" in cfg: mem.wr16(OBJECT + 0x5C, cfg["frame"])
            if "limit" in cfg: args[1] = cfg["limit"]
        if name == "PlayerWithinRadiusXZ":
            args[0], args[1] = 100, 140
            if "radius" in cfg: args[2] = cfg["radius"]
        if name == "ObjSpawnParticleAt" and "offset" in cfg:
            args[2:] = list(cfg["offset"])
        if name == "ObjAddHitBodyIfPlayerNear" and "player" in cfg:
            pass  # player state was set above
        mem.wr32(MUTATION_SELECTOR, 0)
        mem.wr32(MUTATION_MARKER, 0)
        return tuple(u32(v) for v in args)

    def mips_hooks(self, model):
        hooks = {}
        for name, dispatch, address in z1.HOOK_NAMES:
            if address is None or dispatch:
                continue
            def callback(cpu, regs, hook_name=name):
                model.mem = z1.MipsMem(cpu)
                return model.call(hook_name, tuple(regs[4:8]))
            hooks[address] = callback

        # Ground height is an API boundary in the world module, outside the
        # zako1 hook table used by the generic checker.
        def solid_height(cpu, regs):
            model.mem = z1.MipsMem(cpu)
            return model.call("GetSolidHeight", tuple(regs[4:8]))
        hooks[0x8004D8C0] = solid_height
        return hooks

    def run(self, name, area, scenario=0, *, selector=0, expected_failure=False):
        cpu = self.cpus[area]
        cpu.restore(self.bases[area])
        cpu.gte_reset()
        cpu.coverage = self.coverage
        mm = z1.MipsMem(cpu)
        args = self.setup(mm, name, scenario, 0x94423 + 0x100 * list(FUNCS).index(name) + scenario)
        mm.wr32(MUTATION_SELECTOR, selector)
        before = cpu.snapshot()
        m_model, c_model = z1.HookModel(mm), z1.HookModel(None)
        self.c.sync(before)
        self.c.set_model(c_model)
        hooks = self.mips_hooks(m_model)
        # ObjAimAtPlayer calls the already-ported Atan2Safe implementation;
        # let the original wrapper run, matching the direct SDK-backed C call.
        if name in ("ObjAimAtPlayer", "ZakoEnterDeath"): hooks.pop(0x80046340, None)

        try:
            original = cpu.call(FUNCS[name][0], args, hooks=hooks)
        except Exception as error:  # noqa: BLE001
            return self.fail(name, scenario, f"MIPS execution failed: {error}", expected_failure)
        fn = getattr(self.c.dll, "ae_" + name)
        if name in ("ObjSpawnParticleAt", "ObjFootstepFx", "ZakoChasePlayer", "ZakoEnterDeath"):
            reg.declare("shared_random", "ObjSpawnParticleAt SpawnParticle position pad")
            particle_pad_stack = {
                "ObjSpawnParticleAt": mr.DEFAULT_SP - 32 + 16 + 6,
                "ObjFootstepFx": mr.DEFAULT_SP - 32 - 32 + 16 + 6,
                "ZakoEnterDeath": mr.DEFAULT_SP - 32 - 32 + 16 + 6,
                "ZakoChasePlayer": mr.DEFAULT_SP - 32 - 32 - 32 + 16 + 6,
            }[name]
            self.c.position_padding_value = cpu.read16(particle_pad_stack)
        cargs = [self.c.ram]
        if FUNCS[name][3]: cargs.append(C.byref(self.c.hooks))
        kinds = FUNCS[name][4]
        cargs.extend(I32(v if v < 0x80000000 else v - 0x100000000) if kind == "s" else U32(v)
                     for v, kind in zip(args, kinds))
        result = fn(*cargs)
        self.counts[name] += 1

        problems = []
        ret_kind = FUNCS[name][2]
        if ret_kind != "v" and (u32(result) != u32(original[2])):
            problems.append(f"return MIPS={u32(original[2]):08X} C={u32(result):08X}")
        if self.c.dll.ae_ram_failed(self.c.ram):
            problems.append("C error: " + self.c.dll.ae_ram_error(self.c.ram).decode(errors="replace"))
        expected = bytes(cpu.ram[:STACK_LO - 0x80000000])
        got = bytes(self.c.main[:STACK_LO - 0x80000000])
        if got != expected:
            off = next(i for i, (left, right) in enumerate(zip(expected, got)) if left != right)
            problems.append(f"RAM differs at {0x80000000 + off:08X}: MIPS={expected[off:off+8].hex()} C={got[off:off+8].hex()}")
        if bytes(cpu.scratch) != bytes(self.c.scratch):
            off = next(i for i, (left, right) in enumerate(zip(cpu.scratch, self.c.scratch)) if left != right)
            problems.append(f"scratch differs at {mr.SCRATCH_BASE + off:08X}")
        me = [z1.hook_signature(e) for e in m_model.events]
        ce = [z1.hook_signature(e) for e in c_model.events]
        if me != ce: problems.append(f"hook events differ MIPS={me} C={ce}")

        if expected_failure:
            if not problems: return self.fail(name, scenario, "mutation survived", True)
            return True
        if problems: return self.fail(name, scenario, "; ".join(problems), False)
        return True

    def fail(self, name, scenario, message, expected_failure=False):
        if not expected_failure:
            self.failures += 1
            print(f"FAIL {name}[case={scenario}]: {message}", flush=True)
        return False

    def run_default_cases(self):
        passed = True
        for index, name in enumerate(FUNCS):
            cases = [{}] + CASE_OVERRIDES.get(name, [])
            for scenario in range(len(cases)):
                area = AREA_IDS[(index + scenario) % len(AREA_IDS)]
                passed = self.run(name, area, scenario) and passed
            print(f"{name}: {self.counts[name]} cases", flush=True)
        return passed

    def finish(self, extra=None, default_run=False, show_coverage=False):
        items = [(name, spec[0], spec[1], True) for name, spec in FUNCS.items()]
        items += [(name, addr, size, False) for name, (addr, size) in (extra or {}).items()]
        summary = reg.coverage_summary(items, self.coverage)
        if show_coverage: reg.print_coverage(summary)
        if default_run:
            reg.write_results(self.counts, summary, not self.failures,
                              extra={"areas": list(AREA_IDS), "seed": 0x94423},
                              function_names=list(FUNCS))
        return summary


def run_default(show_coverage):
    h = Harness(build())
    passed = h.run_default_cases()
    summary = h.finish(EXTRA_COVERAGE, default_run=True,
                       show_coverage=show_coverage)
    print(f"differential: {sum(h.counts.values())} cases; {'PASS' if passed else 'FAIL'}", flush=True)
    return 0 if passed else 1


def run_mutations():
    source_path = ROOT / "src/world/ae_zakohelpers.c"
    source = source_path.read_text(encoding="utf-8")
    names = list(FUNCS)
    caught = 0
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    harness = None
    with tempfile.TemporaryDirectory(prefix="zakohelpers_mut_", dir=out) as td:
        for variant, marker in ((1, 0x11), (2, 0x22)):
            mutated = source
            for index, name in enumerate(names, 1):
                pattern = re.compile(
                    r"(\b(?:void|int32_t|uint32_t)\s+ae_" + re.escape(name) + r"\s*\([^)]*\)\s*\{)", re.S)
                injection = (f"\n    if (ae_rd32(r, 0x{MUTATION_SELECTOR:08X}u) == {index}u) "
                             f"ae_wr8(r, 0x{MUTATION_MARKER:08X}u, {marker}u);\n")
                mutated, count = pattern.subn(r"\1" + injection, mutated, count=1)
                if count != 1: raise SystemExit(f"could not instrument {name}")
            mutant_source = Path(td) / f"variant{variant}.c"
            mutant_source.write_text(mutated, encoding="utf-8")
            mutant_dll = build(mutant_source, f"zakohelpers_mut{variant}")
            if harness is None:
                harness = Harness(mutant_dll)
            else:
                harness.set_c(mutant_dll)
            for index, name in enumerate(names, 1):
                area = AREA_IDS[(index - 1) % len(AREA_IDS)]
                before_failures = harness.failures
                if harness.run(name, area, 0, selector=index, expected_failure=True):
                    caught += 1
                elif harness.failures == before_failures:
                    print(f"MUTATION SURVIVED variant={variant} {name}", flush=True)
            print(f"mutant variant {variant}: checked {len(names)} functions", flush=True)
    print(f"mutations caught: {caught}/{len(names) * 2}", flush=True)
    return 0 if caught == len(names) * 2 else 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    args = parser.parse_args()
    if args.mutate:
        return run_mutations()
    return run_default(args.coverage)


if __name__ == "__main__":
    sys.exit(main())
