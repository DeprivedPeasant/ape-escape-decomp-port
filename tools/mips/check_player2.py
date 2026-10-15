"""Instruction-level differential checker for the second PlayerUpdate batch."""
from __future__ import annotations

import argparse
import ctypes as C
import os
import random
import re
import shutil
import struct
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

PLAYER = 0x800EC1E0
PLAYER_STATE_SETS = 0x800F4DE0
GADGET4 = 0x800F5120
SPU_SHADOW_BUFFER = 0x80150000
WALL_NORMAL = 0x801FF200
AREA2_DUST_OVERLAY_ADDRESS = 0x8013D000
AREA2_DUST_OVERLAY = ROOT / "extract/members/18C.bin"
AREA2_DUST_PADDING_DIRECT = 0x801FFEF6
AREA2_DUST_PADDING_NESTED = 0x801FFECE
SPAWN_GROUP_PADDING_SLOT = 0x801FFEAE
SPLASH_GROUP_PADDING_SLOT = {
    "PlayerSplashStart": 0x801FFE8E,
    "PlayerCheckWaterEntry": 0x801FFE6E,
}
GROUND_IMPACT_PADDING_SLOT = {
    "EffectGroundImpact": 0x801FFEDE,
    "EffectGroundImpactAtPlayer": 0x801FFEC6,
}
PLAYER_TARGET_MOVE_VEL = (0x114, 0x118, 0x11C)
PLAYER_MOVE_VEL = (0x120, 0x124, 0x128)
STACK_LO = mr.STACK_WINDOW[0] - 0x80000000
DEFAULT_SEED = 0x944232
AREA_IDS = (1, 5, 0x25)
PADDING_SLOT = {
    "PlayerSpawnDustBehind": 0x801FFEF6,
    "PlayerSparkleBurst": 0x801FFEEE,
}
DRAW_GROUP_PADDING_SLOT = {
    "PlayerDrawSquashedWithFx": 0x801FFE6E,
    "PlayerDrawSquashedFlattened": 0x801FFE6E,
}
GADGET_SELECT_GROUP_PADDING_SLOT = 0x801FFE86
PLAYER_STATE_TABLE = 0x80151000
PLAYER_BONE_MATRICES = 0x80152000
RANDOM_PARTICLE_CURSOR = 0x800B205C
TRANSITION_CONDITION_FUNCTIONS = {0: 0x801FFE08, 1: 0x801FFE00, 2: 0x801FFE04}
TRANSITION_ACTION_FUNCTIONS = {1: 0x801FFE10}
HELD_OBJECT_BODY = 0x80150000
EMPTY_TRANSITIONS = 0x801FF600
FX_GROUP_PADDING_SLOT = 0x801FFE86
PLAYER_FX_FUNCTIONS = {"PlayerFxSquashParticles", "PlayerFxFlattenedParticles"}
DRAW_FUNCTIONS = {"PlayerDrawSquashed", "PlayerDrawSquashedWithFx", "PlayerDrawSquashedFlattened"}
DRAW_EFFECT_FUNCTIONS = {"PlayerDrawSquashedWithFx", "PlayerDrawSquashedFlattened"}
SELECT_GADGET_HOOKS = {"PlayerStopGadgetSounds", "AimModeEnter", "TrailRingReset"}
DRAW_RENDER_HOOKS = {
    0x80014D70: ("AnimDecodeFrame", 1),
    0x80014A84: ("MatrixFromFrontUp", 4),
    0x800305B0: ("ScaleMatrixL", 2),
    0x80016A60: ("ModelTransformSkinned", 1),
    0x8001D4A8: ("ModelPrepCycleColors", 2),
    0x8001D530: ("ModelEmitPrimsCycleColors", 3),
    0x80017CD4: ("ModelDraw", 3),
    0x8002020C: ("DrawRibbonBetween", 4),
}
_BASE_OBJECTS = None

# name -> (entry point, byte size, C symbol, return value is observable)
FUNCS = {
    "PlayerIsOnGround": (0x800652E4, 52, "ae_PlayerIsOnGround", True),
    "PlayerCheckWaterEntry": (0x800657F0, 248, "ae_PlayerCheckWaterEntry", True),
    "PlayerAccelXZ": (0x8006699C, 320, "ae_PlayerAccelXZ", True),
    "PlayerAccelXYZ": (0x80066ADC, 436, "ae_PlayerAccelXYZ", True),
    "PlayerSpawnDust": (0x8006B70C, 208, "ae_PlayerSpawnDust", False),
    "FUN_8013f0c0": (0x8013F0C0, 244, "ae_FUN_8013f0c0", False),
    "PlayerFxSpawn10C": (0x8006C2C0, 40, "ae_PlayerFxSpawn10C", False),
    "EffectGroundImpact": (0x8006B570, 320, "ae_EffectGroundImpact", False),
    "EffectGroundImpactAtPlayer": (0x8006BF84, 52, "ae_EffectGroundImpactAtPlayer", False),
    "RcCarSpawn": (0x80099360, 268, "ae_RcCarSpawn", False),
    "PlayerStartHurtBlink": (0x80066018, 16, "ae_PlayerStartHurtBlink", False),
    "PlayerEnterSwimIfInWater": (0x80066094, 52, "ae_PlayerEnterSwimIfInWater", False),
    "PlayerOnWallBump": (0x800668A0, 252, "ae_PlayerOnWallBump", False),
    "PlayPlayerVoice": (0x8006896C, 132, "ae_PlayPlayerVoice", False),
    "PlayerApplyGadgetStance": (0x80067A1C, 212, "ae_PlayerApplyGadgetStance", False),
    "PlayerResetAir": (0x800665E0, 24, "ae_PlayerResetAir", False),
    "PlayerFxResetBlink": (0x8006CFA4, 24, "ae_PlayerFxResetBlink", False),
    "PlayerSpawnDustBehind": (0x8006BF10, 116, "ae_PlayerSpawnDustBehind", False),
    "PlayerSparkleBurst": (0x8006C204, 188, "ae_PlayerSparkleBurst", False),
    "AimTableClear3": (0x8007A304, 40, "ae_AimTableClear3", False),
    "PlayerFxClearDAT_800F5144": (0x8007AA64, 52, "ae_PlayerFxClearDAT_800F5144", False),
    "IsPauseMenuOpen": (0x8007F5D4, 16, "ae_IsPauseMenuOpen", True),
    "RadarResetState": (0x80099D20, 80, "ae_RadarResetState", False),
    "Max16": (0x800A1174, 40, "ae_Max16", True),
    "SpuSetMainVolume": (0x800A2A14, 92, "ae_SpuSetMainVolume", False),
    "SoundSetVolumeMode": (0x80086248, 156, "ae_SoundSetVolumeMode", True),
    "SpawnRandomParticle": (0x80070B70, 548, "ae_SpawnRandomParticle", False),
    "PlayerSplashStart": (0x8006B848, 264, "ae_PlayerSplashStart", False),
    "PlayerRunTransitions": (0x800649D8, 280, "ae_PlayerRunTransitions", False),
    "PlayerUpdateHeldByObject": (0x80068634, 420, "ae_PlayerUpdateHeldByObject", False),
    "PlayerFxSquashParticles": (0x8006C92C, 764, "ae_PlayerFxSquashParticles", False),
    "PlayerFxFlattenedParticles": (0x8006CC28, 440, "ae_PlayerFxFlattenedParticles", False),
    "PlayerDrawSquashed": (0x800689F0, 236, "ae_PlayerDrawSquashed", False),
    "PlayerDrawSquashedWithFx": (0x80068ADC, 64, "ae_PlayerDrawSquashedWithFx", False),
    "PlayerDrawSquashedFlattened": (0x80068B1C, 96, "ae_PlayerDrawSquashedFlattened", False),
    "PlayerUpdateIdleAnim": (0x800687D8, 404, "ae_PlayerUpdateIdleAnim", False),
    "PlayerSelectGadget": (0x8007AC64, 852, "ae_PlayerSelectGadget", True),
    "PlayerUpdateClimb": (0x80067CAC, 1336, "ae_PlayerUpdateClimb", False),
    "PlayerUpdateRope": (0x800681E4, 1104, "ae_PlayerUpdateRope", False),
    "PlayerOnContact": (0x80066FC8, 2476, "ae_PlayerOnContact", True),
    "MonkeyNetCatchDraw": (0x8007BD38, 1384, "ae_MonkeyNetCatchDraw", False),
    "MonkeyNetCatchUpdate": (0x8007C2A0, 456, "ae_MonkeyNetCatchUpdate", False),
}

CASES = {
    "PlayerIsOnGround": [
        ((), {"ground_y": 100, "position_y": 100, "has_ground": 1}),
        ((), {"ground_y": 101, "position_y": 100, "has_ground": 1}),
        ((), {"ground_y": 99, "position_y": 100, "has_ground": 0}),
        ((), {"ground_y": -200, "position_y": -100, "has_ground": -1}),
        ((), {"ground_y": 0x70000000, "position_y": -0x70000000, "has_ground": 1}),
        ((), {"ground_y": -1, "position_y": 0, "has_ground": 2}),
    ],
    "PlayerCheckWaterEntry": [
        ((), {"state_flags": 0xC0, "ground_type": 0x1000, "gadget": 2, "aim_mode": 1}),
        ((), {"state_flags": 0xE0, "ground_type": 0x1000, "gadget": 2, "aim_mode": 1}),
        ((), {"state_flags": 0, "ground_type": 0x1000, "gadget": 0, "aim_mode": 0}),
        ((), {"state_flags": 0x40, "ground_type": 0x1000, "gadget": 2, "aim_mode": 1}),
        ((), {"state_flags": 0, "ground_type": 0x1001, "gadget": 2, "aim_mode": 1}),
        ((), {"state_flags": 0, "ground_type": 0x2000, "gadget": 2, "aim_mode": 1}),
        ((), {"state_flags": 0xA0, "ground_type": 0x1000, "gadget": 0xFE, "aim_mode": 1}),
        ((), {"state_flags": 0, "ground_type": 0x1000, "gadget": 0xFF, "aim_mode": 0}),
    ],
    "PlayerAccelXZ": [
        ((0,), {"move": (0, 0, 0), "target": (0, 0, 0)}),
        ((64,), {"move": (64, 0, 0), "target": (0, 0, 0)}),
        ((1,), {"move": (65, 0, -65), "target": (0, 0, 0)}),
        ((1,), {"move": (-63, 0, 64), "target": (0, 0, 0)}),
        ((1000,), {"move": (4096, 0, 3072), "target": (0, 0, 0)}),
        ((32,), {"move": (1024, 0, -1024), "target": (2048, 0, 2048)}),
    ],
    "PlayerAccelXYZ": [
        ((0,), {"move": (0, 0, 0), "target": (0, 0, 0)}),
        ((64,), {"move": (64, 0, 0), "target": (0, 0, 0)}),
        ((1,), {"move": (65, -65, 128), "target": (0, 0, 0)}),
        ((0,), {"move": (-64, 64, -128), "target": (0, 0, 0)}),
        ((1000,), {"move": (4096, -2048, 3072), "target": (0, 0, 0)}),
        ((32,), {"move": (1024, -1024, 512), "target": (2048, 2048, -512)}),
    ],
    "PlayerStartHurtBlink": [((), {})] * 3,
    "PlayerEnterSwimIfInWater": [
        ((), {"ground_type": 0x1000, "old_state": 0x20}),
        ((), {"ground_type": 0x2000, "old_state": 0x20}),
        ((), {"ground_type": 0x1FFF, "old_state": 0x40}),
        ((), {"ground_type": 0x0000, "old_state": 0x10}),
        ((), {"ground_type": 0x1001, "old_state": 0x20}),
        ((), {"ground_type": 0xF000, "old_state": 0x30}),
    ],
    "PlayerApplyGadgetStance": [
        ((), {"gadget": 0, "state": 4}),
        ((), {"gadget": 0, "state": 1}),
        ((), {"gadget": 0, "state": 3}),
        ((), {"gadget": 0, "state": 7}),
        ((), {"gadget": 0, "state": 0x14}),
        ((), {"gadget": 0, "state": 0x13}),
        ((), {"gadget": 2, "state": 3}),
        ((), {"gadget": 5, "state": 0}),
        ((), {"gadget": 7, "state": 2}),
        ((), {"gadget": 2, "state": 5}),
        ((), {"gadget": 5, "state": 0x13}),
        ((), {"gadget": 7, "state": 0x14}),
    ],
    "PlayerResetAir": [((), {})] * 3,
    "PlayerFxResetBlink": [((), {})] * 3,
    "PlayerSpawnDustBehind": [
        ((), {"position": (100, 400, -200), "facing": (0x1000, -0x1800)}),
        ((), {"position": (-32768, -1, 32767), "facing": (-32768, 32767)}),
        ((), {"position": (0x12345, -0x23456, 0x34567), "facing": (65, -65)}),
    ],
    "PlayerSparkleBurst": [
        ((), {"position": (100, 400, -200), "rand_seed": 0x1234}),
        ((), {"position": (-32768, -1, 32767), "rand_seed": 0xFFF0}),
        ((), {"position": (0x12345, -0x23456, 0x34567), "rand_seed": 0xABCD}),
    ],
    "PlayPlayerVoice": [
        ((voice, 0x1234), {}) for voice in range(12)
    ] + [((12, 0xFFFFFFFF), {}), ((0xFFFFFFFF, 0x80000001), {})],
    "PlayerOnWallBump": [
        ((WALL_NORMAL,), {"health": 0, "facing": (0, 0), "normal": (0, 0), "move": (-9, -11, -13)}),
        ((WALL_NORMAL,), {"health": 1, "facing": (0x1000, 0), "normal": (-0x1000, 0), "move": (-9, -5, -13)}),
        ((WALL_NORMAL,), {"health": 100, "facing": (-0x1000, 0), "normal": (-0x1000, 0), "move": (-9, -5, -13)}),
        ((WALL_NORMAL,), {"health": 100, "facing": (0x1000, 0x1000), "normal": (-0x1000, 0x1000), "move": (-1, 2, -3)}),
        ((WALL_NORMAL,), {"health": 100, "facing": (0x1000, 0), "normal": (0, 0), "move": (-7, -1, -15)}),
        ((WALL_NORMAL,), {"health": -1, "facing": (0x7FFF, 0x7FFF), "normal": (0x7FFF, 0x7FFF), "move": (1, -1, 1)}),
        ((WALL_NORMAL,), {"health": 100, "facing": (-0x8000, -0x8000), "normal": (-0x8000, -0x8000), "move": (0x7FFFFFFF, -1, -0x80000000)}),
    ],
    "PlayerSpawnDust": [
        ((), {"current_area": 2, "frame": 0, "position": (100, 400, -200), "velocity": (1, 2, 3),
              "primary_seed": 0, "secondary_seed": 0}),
        ((), {"current_area": 1, "frame": 2, "position": (100, 400, -200), "velocity": (1, 2, 3)}),
        ((), {"current_area": 1, "frame": 1, "position": (100, 400, -200), "velocity": (1, 2, 3)}),
        ((), {"current_area": 5, "frame": 3, "position": (-32768, -1, 32767), "velocity": (-0x80000000, 0x7FFFFFFF, -17)}),
        ((), {"current_area": 0x25, "frame": 0xFFFFFFFF, "position": (0x12345, -0x23456, 0x34567), "velocity": (0x12345678, -0x2345678, 0x76543210)}),
        ((), {"current_area": 2, "frame": 1, "position": (-32768, -1, 32767), "velocity": (-7, 11, -13),
              "primary_seed": 0xFFFF, "secondary_seed": 0xFFF0}),
        ((), {"current_area": 2, "frame": 0x12345678, "position": (0x12345, -0x23456, 0x34567),
              "velocity": (1, 2, 3), "primary_seed": 0x1234, "secondary_seed": 0xABCD}),
    ],
    "FUN_8013f0c0": [
        ((), {"position": (100, 400, -200), "primary_seed": 0x0000, "secondary_seed": 0x0000}),
        ((), {"position": (-32768, -1, 32767), "primary_seed": 0xFFFF, "secondary_seed": 0xFFF0}),
        ((), {"position": (0x12345, -0x23456, 0x34567), "primary_seed": 0x1234, "secondary_seed": 0xABCD}),
        ((), {"position": (-0x70000, 0x70000, -0x7FFFF), "primary_seed": 0x5555, "secondary_seed": 0x8001}),
    ],
    "PlayerFxSpawn10C": [
        ((), {"position": (100, 400, -200), "rand_seed": 0x1234}),
        ((), {"position": (-32768, -1, 32767), "rand_seed": 0xFFF0}),
        ((), {"position": (0x12345, -0x23456, 0x34567), "rand_seed": 0xABCD}),
        ((), {"position": (-1, -32768, 0), "rand_seed": 0x0001}),
        ((), {"position": (0x70000, -0x70000, 0x7FFFF), "rand_seed": 0x8000}),
        ((), {"position": (17, 34, 51), "rand_seed": 0x5555}),
    ],
    "EffectGroundImpact": [
        ((0, 100, 0), {"solid": -1000, "rand_seed": 0x1234}),
        ((100, -500, -100), {"solid": 1000, "rand_seed": 0xFFF0}),
        ((-32768, 32767, 32767), {"solid": -32768, "rand_seed": 0xABCD}),
        ((32767, -32768, -32768), {"solid": 32767, "rand_seed": 0x0001}),
        ((0x12345, -0x23456, 0x34567), {"solid": 500, "rand_seed": 0x8000}),
        ((-256, 512, 256), {"solid": 0, "rand_seed": 0x5555}),
    ],
    "EffectGroundImpactAtPlayer": [
        ((), {"position": (0, 100, 0), "solid": -1000, "rand_seed": 0x1234}),
        ((), {"position": (100, -500, -100), "solid": 1000, "rand_seed": 0xFFF0}),
        ((), {"position": (-32768, 32767, 32767), "solid": -32768, "rand_seed": 0xABCD}),
        ((), {"position": (32767, -32768, -32768), "solid": 32767, "rand_seed": 0x0001}),
        ((), {"position": (0x12345, -0x23456, 0x34567), "solid": 500, "rand_seed": 0x8000}),
        ((), {"position": (-256, 512, 256), "solid": 0, "rand_seed": 0x5555}),
    ],
    "RcCarSpawn": [
        ((1, 2, 3), {"can_activate": 0, "active": 0, "cooldown": 0, "sound_handle": 0x1234}),
        ((100, 200, -300), {"can_activate": 1, "active": 0, "cooldown": 0, "sound_handle": 0x1234}),
        ((-1, -2, -3), {"can_activate": 1, "active": 1, "cooldown": 4, "sound_handle": -2}),
        ((32767, -32768, 0x12345678), {"can_activate": 1, "active": 1, "cooldown": 0, "sound_handle": 0x3456}),
        ((-32768, 32767, -0x7654321), {"can_activate": 1, "active": 1, "cooldown": -1, "sound_handle": 7}),
        ((0x12345678, -0x2345678, 0x3456789A), {"can_activate": 1, "active": 0, "cooldown": -1, "sound_handle": 0x7FFF}),
    ],
    "AimTableClear3": [((), {})] * 3,
    "PlayerFxClearDAT_800F5144": [((), {})] * 3,
    "IsPauseMenuOpen": [((), {"pause_mode": value}) for value in (0, 1, 0x80)],
    "RadarResetState": [((), {})] * 3,
    "Max16": [
        ((-1, 1), {}),
        ((1, -1), {}),
        ((0xFFFF, 0x7FFF), {}),
        ((0x8000, 0x7FFF), {}),
        ((0x12348001, 0xFFFF8002), {}),
        ((0x80008000, 0x7FFF7FFF), {}),
    ],
    "SpuSetMainVolume": [
        ((0x7F, 0x20), {"mono": 0}),
        ((0x20, 0x7F), {"mono": 0}),
        ((0x7F, 0x20), {"mono": 1}),
        ((0x80, 0x7F), {"mono": 1}),
        ((0xFF, 0x01), {"mono": 1}),
        ((-1, 0x80000002), {"mono": 0}),
    ],
    "SoundSetVolumeMode": [
        ((-2,), {"reverb": 75, "main": 42, "mono": 0}),
        ((-1,), {"reverb": -32768, "main": -1, "mono": 0}),
        ((0,), {"reverb": 75, "main": 42, "mono": 0}),
        ((0,), {"reverb": 32767, "main": 42, "mono": 1}),
        ((1,), {"reverb": 20, "main": 0x1234, "mono": 0}),
        ((2,), {"reverb": 20, "main": 0x4321, "mono": 1}),
        ((3,), {"reverb": 0, "main": 0, "mono": 0}),
        ((0x7FFFFFFF,), {"reverb": -137, "main": -30000, "mono": 1}),
    ],
    "SpawnRandomParticle": [
        ((0x801FF100, 0, 0x00, 0), {"rand_seed": 0x0000, "cursor": 0}),
        ((0x801FF110, 0xFF, 0x7F, 1), {"rand_seed": 0x1234, "cursor": 1}),
        ((0x801FF120, 0x100, 0x100, 0x60), {"rand_seed": 0xFFF0, "cursor": 7}),
        ((0x801FF130, 0x1FF, 0x101, -0x60), {"rand_seed": 0xABCD, "cursor": 6}),
        ((0x801FF140, 0x80, 0x1A5, 0x7FFFFFFF), {"rand_seed": 0x8000, "cursor": 0xFF}),
        ((0x801FF150, 0x35, 0x2FE, -0x80000000), {"rand_seed": 0x5555, "cursor": 0xF8}),
        ((0x801FF160, 0x100, 0x123, -257), {"rand_seed": 0x0001, "cursor": 3}),
        ((0x801FF170, 0, 0x100, 257), {"rand_seed": 0xFFFE, "cursor": 5}),
    ],
    "PlayerSplashStart": [
        ((), {"fade": 0, "position": (100, 400, -200), "move_y": -41}),
        ((), {"fade": 1, "position": (-32768, -1, 32767), "move_y": 41}),
        ((), {"fade": 0, "position": (0x12345, -0x23456, 0x34567), "move_y": -1}),
        ((), {"fade": 0, "position": (-1, -32768, 0), "move_y": 0x7FFFFFFF}),
    ],
    "PlayerRunTransitions": [
        ((0x801FF200,), {"state": 0, "entries": ((1, 1, 2, 0), (0xFF, 0, 0, 0)),
                         "condition_results": {}, "actions": ()}),
        ((0x801FF200,), {"state": 2, "entries": ((1, 1, 2, 0), (2, 1, 3, 0), (0xFF, 0, 0, 0)),
                         "condition_results": {1: 1}, "actions": ()}),
        ((0x801FF200,), {"state": 1, "entries": ((1, 1, 2, 1), (0xFF, 0, 0, 0)),
                         "condition_results": {1: 1}, "actions": (1,)}),
        ((0x801FF200,), {"state": 1, "entries": ((1, 1, 2, 0), (1, -2, 3, 0), (0xFF, 0, 0, 0)),
                         "condition_results": {1: 0, 2: 0}, "actions": ()}),
        ((0x801FF200,), {"state": 1, "entries": ((1, -1, 2, 0), (0xFF, 0, 0, 0)),
                         "condition_results": {1: 1}, "actions": ()}),
        ((0x801FF200,), {"state": 1, "entries": ((1, 0, 2, 0), (0xFF, 0, 0, 0)),
                         "condition_results": {0: 1}, "actions": ()}),
        ((0x801FF200,), {"state": 1, "entries": ((1, -2, 3, 1), (0xFF, 0, 0, 0)),
                         "condition_results": {2: 1}, "actions": ()}),
    ],
    "PlayerUpdateHeldByObject": [
        ((), {"state": 0, "gadget": 0, "contact": 0, "travel": (0, 0), "anim_frame": 0xFFFF,
              "body_type": 0x0B00, "body_index": 3, "body_pos": (100, 200, -300),
              "body_facing": (0x1000, 0, 0, 0x1000)}),
        ((), {"state": 1, "gadget": 0, "contact": 0xB0000003, "travel": (100, 0), "anim_frame": 0x100,
              "body_type": 0x0B00, "body_index": 3, "body_pos": (-1, -2, -3),
              "body_facing": (-0x1000, 0, 0, 0x1000)}),
        ((), {"state": 2, "gadget": 0, "contact": 0xB0000004, "travel": (101, 0), "anim_frame": 0xFFFF,
              "body_type": 0x0B00, "body_index": 4, "body_pos": (0x12345678, -0x2345678, 0x3456789A),
              "body_facing": (0x7FFF, -0x8000, 0x1234, -0x2345)}),
        ((), {"state": 0, "gadget": 0, "contact": 0xB0000003, "travel": (70, 30), "anim_frame": 0,
              "body_type": 0x0A00, "body_index": 3, "body_pos": (17, 34, 51),
              "body_facing": (0x1000, 0, 0, 0x1000)}),
        ((), {"state": 0, "gadget": 0, "contact": 0xA0000003, "travel": (0, 101), "anim_frame": 7,
              "body_type": 0x0B00, "body_index": 3, "body_pos": (-100, 200, -300),
              "body_facing": (-0x1000, 0, 0, 0x1000)}),
        ((), {"state": 0, "gadget": 0, "contact": 0xB0000FFF, "travel": (-50, -50), "anim_frame": 8,
              "body_type": 0x0B00, "body_index": 3, "body_pos": (100, 200, 300),
              "body_facing": (0, 0x1000, 0, 0x1000)}),
        ((), {"state": 0x80, "gadget": 2, "contact": 0xB0000005, "travel": (-20, -30), "anim_frame": 9,
              "body_type": 0x0B00, "body_index": 5, "body_pos": (-32768, 32767, -1),
              "body_facing": (-1, -2, -3, -4)}),
    ],
    "PlayerFxSquashParticles": [
        ((), {"frame": frame, "rand_seed": seed, "player_pos": (100, 400, -200),
              "fx_pos": (100, 400, -200)})
        for frame, seed in ((0, 0x0000), (1, 0x1234), (6, 0xFFF0), (10, 0xABCD),
                            (17, 0x8000), (18, 0x0001), (20, 0x5555), (29, 0xFFFE))
    ],
    "PlayerFxFlattenedParticles": [
        ((), {"frame": frame, "rand_seed": seed, "fx_pos": (100, 400, -200)})
        for frame, seed in ((0, 0x0000), (1, 0x1234), (4, 0xFFF0), (7, 0xABCD),
                            (8, 0x8000), (0xFFFF, 0x0001), (0x8000, 0x5555))
    ],
    "PlayerDrawSquashed": [
        ((), {"frame": frame, "rand_seed": 0x1234}) for frame in (-1, 0, 7, 8, 0x7FFF)
    ],
    "PlayerDrawSquashedWithFx": [
        ((), {"frame": frame, "rand_seed": seed})
        for frame, seed in ((-1, 0x0001), (1, 0x1234), (6, 0xFFF0), (20, 0xBEEF),
                            (10, 0xABCD), (29, 0x8000), (30, 0x0000), (0x7FFF, 0x5555))
    ],
    "PlayerDrawSquashedFlattened": [
        ((), {"frame": frame, "rand_seed": seed})
        for frame, seed in ((-1, 0x0001), (0, 0x1234), (1, 0xFFF0),
                            (7, 0xABCD), (15, 0x8000), (16, 0x0000), (0x7FFF, 0x5555))
    ],
    "PlayerUpdateIdleAnim": [
        ((), {"frame": frame, "state": state, "draw_buffer": draw, "player_frame": player_frame})
        for frame, state, draw, player_frame in ((0, 5, 0, 0), (16, 5, 1, 0xFFFE),
                                                (17, 5, 0, 9), (31, 4, 1, 0xFFFF))
    ],
    "PlayerSelectGadget": [
        ((2, 0), {"current": 0, "state": 0, "state_flags": 0, "aim_mode": 0,
                  "pause_mode": 1, "position": (100, 400, -200), "ground_y": 500,
                  "can_activate": 0, "car_active": 0, "car_cooldown": 0, "sound_handle": 0}),
        ((3, 3), {"current": 3, "state": 0x20, "state_flags": 0, "aim_mode": 0,
                  "pause_mode": 1, "position": (100, 400, -200), "ground_y": 500,
                  "can_activate": 0, "car_active": 0, "car_cooldown": 0, "sound_handle": 0}),
        ((0, 2), {"current": 2, "state": 0, "state_flags": 0, "aim_mode": 1,
                  "pause_mode": 1, "position": (100, 400, -200), "ground_y": 500,
                  "can_activate": 0, "car_active": 0, "car_cooldown": 0, "sound_handle": 0}),
        ((4, 4), {"current": 4, "state": 0, "state_flags": 0, "aim_mode": 0,
                  "pause_mode": 1, "position": (100, 400, -200), "ground_y": 500,
                  "can_activate": 0, "car_active": 0, "car_cooldown": 0, "sound_handle": 0}),
        ((5, 3), {"current": 3, "state": 0, "state_flags": 0, "aim_mode": 0,
                  "pause_mode": 1, "position": (100, 400, -200), "ground_y": 500,
                  "can_activate": 0, "car_active": 0, "car_cooldown": 0, "sound_handle": 0}),
        ((6, 5), {"current": 5, "state": 0, "state_flags": 0, "aim_mode": 0,
                  "pause_mode": 0, "position": (100, 400, -200), "ground_y": 500,
                  "can_activate": 0, "car_active": 0, "car_cooldown": 0, "sound_handle": 0}),
        ((7, 7), {"current": 7, "state": 0, "state_flags": 0, "aim_mode": 0,
                  "pause_mode": 1, "position": (100, 400, -200), "ground_y": 500,
                  "can_activate": 1, "car_active": 0, "car_cooldown": 0, "sound_handle": 0}),
        ((3, 2), {"current": 2, "state": 0, "state_flags": 0, "aim_mode": 1,
                  "aim_request": 1,
                  "pause_mode": 1, "position": (-32768, -1, 32767), "ground_y": -100,
                  "can_activate": 0, "car_active": 0, "car_cooldown": 0, "sound_handle": 0}),
        ((9, 6), {"current": 6, "state": 0x80, "state_flags": 0, "aim_mode": 0,
                  "pause_mode": 1, "position": (0x12345, 400, -0x23456), "ground_y": 0,
                  "can_activate": 0, "car_active": 0, "car_cooldown": 0, "sound_handle": 0}),
    ],
    "PlayerUpdateClimb": [
        ((), {"state": state, "position": (100, 100, -200), "desired": (0x1000, 0, 0),
              "facing": (0x1000, 0, 0, 0x1000), "target": (120, 150, -180, 0, 0, 20),
              "stick_x": 0, "stick_y": stick_y, "held": held, "changed": changed,
              "frame": frame, "table_start": 0, "table_end": 20})
        for state, stick_y, held, changed, frame in (
            (0x38, 0, 0, 0, 0), (0x39, 0, 0, 0, 4), (0x39, 0xF0, 0, 0, 6),
            (0x3A, 0, 0, 0, 6), (0x3B, 0, 0, 0, 9), (0x20, 0, 0, 0, 2),
            (0x39, 0, 0x0A, 0x0A, 8))
    ],
    "PlayerUpdateRope": [
        ((), {"state": state, "position": (10, 20, 30), "facing": (0x1000, 0, 0, 0x1000),
              "rope": rope, "attach_flags": 4, "held": held, "changed": changed,
              "aim_mode": 0, "gesture": gesture, "jump_allowed": allowed})
        for state, rope, held, changed, gesture, allowed in (
            (0x3D, (0, 0, 0, 100, 200, 300, 0x800, 0), 0, 0, 0, 0),
            (0x3E, (-100, 50, 20, 100, 200, 300, 0x400, 1), 0, 0, 1, 0),
            (0x3F, (10, -50, 20, 100, 200, 300, 0x1000, 1), 0, 0, 1, 0),
            (0x40, (10, 20, 30, 100, 200, 300, 0x800, 1), 0, 0, 0, 0),
            (0x41, (-32768, 20, 30, 32767, 200, 300, 0x1000, 1), 0, 0, 0, 0),
            (0x20, (10, 20, 30, 100, 200, 300, 0x800, 1), 0, 0, 0, 0),
            (0x3E, (10, 20, 30, 100, 200, 300, 0x800, 1), 0x0A, 0x0A, 0, 1))
    ],
    "PlayerOnContact": [
        ((), {"state": 0x20, "state_flags": 0, "contact": 0x30000000,
              "health": 10, "hurt": 1, "hurt_blink": 0, "invuln": 0,
              "mode": 0, "aim_mode": 0, "aim_request": 0, "camera_fixed": 0,
              "ground_type": 0, "ground_y": 0, "position": (10, 20, 30),
              "body_type": 0, "body_flags": 0, "body_pos": (100, 0, -100),
              "cooldown": 2, "contact_frames": 0}),
        ((), {"state": 0x10, "state_flags": 0, "contact": 0x40000003,
              "health": 10, "hurt": 0, "hurt_blink": 0, "invuln": 2,
              "mode": 0, "aim_mode": 0, "aim_request": 0, "camera_fixed": 0,
              "ground_type": 0, "ground_y": 200, "position": (10, 100, 30),
              "body_type": 0, "body_flags": 0, "body_pos": (110, 0, -90),
              "cooldown": 2, "contact_frames": 0}),
        ((), {"state": 0x10, "state_flags": 0, "contact": 0x40000003,
              "health": 10, "hurt": 0, "hurt_blink": 0, "invuln": 1,
              "mode": 0, "aim_mode": 0, "aim_request": 0, "camera_fixed": 0,
              "ground_type": 0x1000, "ground_y": 200, "position": (10, 100, 30),
              "body_type": 0, "body_flags": 0, "body_pos": (110, 0, -90),
              "cooldown": 2, "contact_frames": 0}),
        ((), {"state": 0x12, "state_flags": 0, "contact": 0xB0000003,
              "health": 10, "hurt": 0, "hurt_blink": 0, "invuln": 0,
              "mode": 0, "aim_mode": 0, "aim_request": 0, "camera_fixed": 0,
              "ground_type": 0, "ground_y": 0, "position": (10, 20, 30),
              "body_type": 0x0605, "body_flags": 0x0200, "body_pos": (100, 0, -100),
              "cooldown": 0, "contact_frames": 0}),
        ((), {"state": 0x12, "state_flags": 0, "contact": 0xB0000003,
              "health": 10, "hurt": 0, "hurt_blink": 0, "invuln": 0,
              "mode": 0, "aim_mode": 0, "aim_request": 0, "camera_fixed": 0,
              "ground_type": 0, "ground_y": 0, "position": (10, 20, 30),
              "body_type": 0x0205, "body_flags": 0x0100, "body_pos": (100, 0, -100),
              "cooldown": 0, "contact_frames": 0}),
        ((), {"state": 0x12, "state_flags": 0, "contact": 0xB0000003,
              "health": 0, "hurt": 0, "hurt_blink": 0, "invuln": 0,
              "mode": 0, "aim_mode": 0, "aim_request": 0, "camera_fixed": 0,
              "ground_type": 0, "ground_y": 0, "position": (10, 20, 30),
              "body_type": 0x0605, "body_flags": 0x0100, "body_pos": (100, 0, -100),
              "cooldown": 0, "contact_frames": 0}),
    ],
    "MonkeyNetCatchDraw": [
        ((), {"state": state, "level": level, "draw_buffer": draw, "frame": frame})
        for state, level, draw, frame in (
            (0, 0x20, 0, 0), (0, 1, 0, 0), (0, 5, 1, 4), (0, 0x25, 1, 9),
            (0x46, 0x20, 0, 0), (0x47, 1, 1, 8), (0x49, 0x25, 0, 9),
            (0x4A, 2, 1, -1), (0x4C, 5, 0, 1),
        )
    ] + [
        ((), {"state": 0, "level": 1, "draw_buffer": draw, "frame": 0,
              "anim_frame": anim_frame, "first_frame": first, "last_frame": last})
        for draw, anim_frame, first, last in ((0, 0, 5, 40), (1, 50, 0, 40))
    ],
    "MonkeyNetCatchUpdate": [
        ((), {"state": state, "level": level, "draw_buffer": draw, "frame": frame})
        for state, level, draw, frame in (
            (0, 0x20, 0, 0), (0, 1, 1, 1), (0, 5, 0, 3), (0x46, 0x25, 1, 4),
            (0, 0x20, 0, 9), (0, 1, 1, 12), (0, 5, 0, 13), (0, 0x25, 1, 25),
            (0, 0x20, 0, 35), (0, 1, 1, 37), (0x49, 5, 1, 38), (0x4C, 0x25, 0, 40),
        )
    ],
}

# Exercise the state subgraphs with combinations that reach each transition and
# movement arm while keeping all non-state inputs deterministic.
_climb_base = CASES["PlayerUpdateClimb"][0][1]
for _state in (0x20, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x80, 0x81):
    for _index, (_stick_y, _position_y, _flags) in enumerate((
        (0, 100, 0), (0x80, 100, 0), (0x7F, 200, 0),
        (0, 200, 1), (0x80, 100, 1), (0x7F, 100, 1),
    )):
        _fields = dict(_climb_base)
        _fields.update({
            "state": _state,
            "position": (100, _position_y, -200),
            "target": (120, 150, -180, 1 if _index >= 4 else 0, _flags, 20),
            "desired": ((-0x1000 if (_state + _index) % 2 else 0x1000), 0,
                        (-0x1000 if (_state + _index) % 3 == 0 else 0x1000), 0),
            "facing": ((-0x1000 if _index % 2 else 0x1000), 0,
                       (-0x1000 if _index % 3 == 0 else 0x1000), 0x1000),
            "stick_x": (-128, 0, 127)[_index % 3],
            "stick_y": _stick_y,
            "frame": (4, 14, 5, 15, 0xFFFF, 0)[_index],
            "table_start": 10 if _index % 2 else 0,
            "table_end": 5 if _index % 2 else 20,
            "held": (0, 2, 8, 10, 2, 8)[_index],
            "changed": (0, 2, 8, 10, 8, 2)[_index],
            "input_block": 1 if _index == 5 else 0,
            "gadget": 1 if _state >= 0x80 else 0,
        })
        CASES["PlayerUpdateClimb"].append(((), _fields))

_rope_base = CASES["PlayerUpdateRope"][0][1]
_rope_inputs = (
    (0, 0, 0, 0, 0, 0, 0, 4),
    (0x400, 1, 2, 2, 0, 1, 0, 4),
    (0x800, 0x800, 8, 8, 1, 1, 1, 4),
    (0x1000, 1, 10, 10, 0, 1, 1, 0),
    (0x1001, 0, 0, 0, 0, 0, 0, 0),
    (-1, 0, 10, 10, 0, 0, 1, 4),
)
for _state in (0x20, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x80):
    for _index, (_fraction, _gesture, _held, _changed, _aim, _stick, _allowed, _attach) in enumerate(_rope_inputs):
        _fields = dict(_rope_base)
        _fields.update({
            "state": _state,
            "position": (10, 20, 30),
            "rope": (0, 0, 0, 100, 200, 300, _fraction, 2000),
            "attach_flags": _attach,
            "held": _held,
            "changed": _changed,
            "aim_mode": _aim,
            "gesture": _gesture,
            "jump_allowed": _allowed,
            "rope_stick": _stick,
            "rope_normal_x": 0x800 if _index % 2 else -0x800,
            "rope_normal_z": -0x400 if _index % 2 else 0x400,
            "facing": (0x1000, 0, -0x800, 0x1000),
            "input_block": 1 if _index == 5 else 0,
            "gadget": 1 if _state >= 0x80 else 0,
        })
        CASES["PlayerUpdateRope"].append(((), _fields))

_contact_base = CASES["PlayerOnContact"][3][1]
for _contact_type in range(13):
    for _ground_type, _ground_y, _position_y in ((0, 0, 100), (0x1000, 200, 100), (0, 1000, 100)):
        _fields = dict(_contact_base)
        _fields.update({
            "contact": (_contact_type << 28) | 3,
            "ground_type": _ground_type,
            "ground_y": _ground_y,
            "position": (10, _position_y, 30),
        })
        CASES["PlayerOnContact"].append(((), _fields))

for _object_type in (0x0A00, 0x0600, 0x0B00, 0x0300, 0x0200, 0x0400, 0x0500, 0x0100):
    for _body_flags in (0, 0x100, 0x101, 0x102, 0x200, 0x202, 0x400, 0x401, 0x1234):
        for _state_flags in (0, 0xC0, 0x30, 0xD0):
            _fields = dict(_contact_base)
            _fields.update({
                "body_type": _object_type | 5,
                "body_flags": _body_flags,
                "state_flags": _state_flags,
                "solid": 100,
            })
            CASES["PlayerOnContact"].append(((), _fields))

for _mode in (0, 5, 6, 10):
    for _state in (0x12, 0x1B, 0x1C, 0x24, 0x4C, 0x22, 0x4D):
        for _health in (0, 1, 10):
            _fields = dict(_contact_base)
            _fields.update({
                "mode": _mode,
                "state": _state,
                "health": _health,
                "body_type": 0x0201,
                "body_flags": 0x0102,
                "solid": 100,
            })
            CASES["PlayerOnContact"].append(((), _fields))

for _state, _cooldown, _hurt_blink, _state_flags in (
    (0x4D, 1, 0, 0), (0x4D, 2, 0, 0), (0x4C, 0, 0, 0),
    (0x22, 0, 0, 0), (0x12, 0, 1, 0), (0x12, 0, 0, 0xD0),
):
    _fields = dict(_contact_base)
    _fields.update({"state": _state, "cooldown": _cooldown,
                    "hurt_blink": _hurt_blink, "state_flags": _state_flags})
    CASES["PlayerOnContact"].append(((), _fields))

for _mode in (0, 5, 6):
    for _health in (0, 1, 10):
        _fields = dict(_contact_base)
        _fields.update({"body_type": 0x0A01, "mode": _mode, "health": _health,
                        "ground_y": 100, "position": (10, 20, 30), "solid": 100})
        CASES["PlayerOnContact"].append(((), _fields))

for _state, _frames, _body_pos, _body_flags, _camera, _aim, _aim_request in (
    (0x1B, 6, (10, 0, 30), 0x0400, 0, 0, 0),
    (0x1C, 7, (-100, 0, -100), 0x0400, 0, 0, 0),
    (0x12, 0, (10, 0, 30), 0x0100, 0, 0, 0),
    (0x12, 0, (-100, 0, -100), 0x0100, 0, 0, 0),
    (0x12, 0, (10, 0, 30), 0x0200, 1, 1, 1),
    (0x12, 0, (-100, 0, -100), 0x0400, 0, 1, 1),
):
    _fields = dict(_contact_base)
    _fields.update({"state": _state, "contact_frames": _frames,
                    "body_type": 0x0201, "body_flags": _body_flags,
                    "body_pos": _body_pos, "camera_fixed": _camera,
                    "aim_mode": _aim, "aim_request": _aim_request})
    CASES["PlayerOnContact"].append(((), _fields))

for _body_type in (0x0C01, 0x0F01):
    _fields = dict(_contact_base)
    _fields.update({"body_type": _body_type, "body_flags": 0x0202})
    CASES["PlayerOnContact"].append(((), _fields))

_gadget_case = dict(CASES["PlayerSelectGadget"][0][1])
_gadget_case.update({
    "current": 7,
    "state": 0x80,
    "state_flags": 0xC0,
    "aim_mode": 1,
    "aim_request": 1,
    "pause_mode": 0,
    "position": (100, 100, -200),
    "ground_y": 500,
    "can_activate": 0,
    "car_active": 0,
    "car_cooldown": 0,
    "sound_handle": 0,
})
CASES["PlayerSelectGadget"].append(((7, -1), _gadget_case))


def u32(value):
    return int(value) & 0xFFFFFFFF


def build(source: Path | None = None, tag: str = "player2") -> Path:
    import check_game as game
    return game.build_dll({"src/player/ae_player2_leaf.c": source} if source else None, tag)


P, U32, I32, VP = C.c_void_p, C.c_uint32, C.c_int32, C.c_void_p
PositionPaddingFn = C.CFUNCTYPE(C.c_uint16, P, P, U32)
PlaySfxFn = C.CFUNCTYPE(None, P, P, U32)
SimpleHookFn = C.CFUNCTYPE(None, P, P)
StopGadgetSoundsFn = C.CFUNCTYPE(None, P, P, I32)
CanActivateGadgetFn = C.CFUNCTYPE(I32, P, P, I32)
PlayerConditionFn = C.CFUNCTYPE(I32, P, P, U32)
PlayerActionFn = C.CFUNCTYPE(None, P, P, U32)
AddressHookFn = C.CFUNCTYPE(None, P, P, U32, U32, U32, U32)
RumbleFn = C.CFUNCTYPE(None, P, P, I32, I32, I32)
DamageFn = C.CFUNCTYPE(None, P, P, I32)
AnimFramePtrFn = C.CFUNCTYPE(U32, P, P, U32, I32)
PlayerSelectActFn = C.CFUNCTYPE(None, P, P, I32, I32)
PlayerActAddressFn = C.CFUNCTYPE(None, P, P, U32)


class Player2Hooks(C.Structure):
    _fields_ = [("user", P), ("position_padding", PositionPaddingFn), ("PlaySfx", PlaySfxFn),
                ("AimModeLeave", SimpleHookFn),
                ("PlayerStopGadgetSounds", StopGadgetSoundsFn),
                ("PlayerCanActivateGadget", CanActivateGadgetFn),
                ("PlayerCondition", PlayerConditionFn), ("PlayerAction", PlayerActionFn),
                ("AnimDecodeFrame", AddressHookFn), ("MatrixFromFrontUp", AddressHookFn),
                ("ScaleMatrixL", AddressHookFn), ("ModelTransformSkinned", AddressHookFn),
                ("ModelPrepCycleColors", AddressHookFn), ("ModelEmitPrimsCycleColors", AddressHookFn),
                ("ModelDraw", AddressHookFn), ("DrawRibbonBetween", AddressHookFn),
                ("AimModeEnter", SimpleHookFn), ("TrailRingReset", SimpleHookFn),
                ("CameraFixedModeOff", SimpleHookFn), ("AimRequestClear", SimpleHookFn),
                ("TurnTowardsY", AddressHookFn),
                ("PadRumble", RumbleFn), ("MatrixLookDir", AddressHookFn),
                ("ModelTransformRigid", AddressHookFn), ("ModelDrawWithOt", AddressHookFn),
                ("ActorAnimDecodeEntry", AddressHookFn), ("SetRotMatrix", AddressHookFn),
                ("SetTransMatrix", AddressHookFn), ("RotTransSV", AddressHookFn),
                ("RotTransPers", AddressHookFn), ("RotMatrixY", AddressHookFn),
                ("EventCamStartRandom", SimpleHookFn)]


class PlayerActHooks(C.Structure):
    _fields_ = [("user", P), ("AimRequestClear", P), ("PlayerCond_JumpHeld", P),
                ("PlayerInput_StickGesture", P), ("PadRumble", P), ("Gadget3AimUpdateAmmo", P),
                ("AimModeEnter", P), ("AimModeLeave", P), ("PlayPlayerVoice", P),
                ("EffectGroundImpactAtPlayer", P), ("PlayerDamage", P), ("PlaySfx", P),
                ("StageRequest", P), ("EffectBigBurst", P), ("StageGoalReached", P),
                ("HudPanelA_Start", P), ("HeapFree", P), ("CameraAreaScriptDispatch", P),
                ("PlayerSelectGadget", P), ("PlayerIdleModelSetup", P),
                ("AnimFramePtr", AnimFramePtrFn), ("DR_MOVE", P), ("PlayerModeCallback", P),
                ("incoming_a1", U32), ("incoming_a2", U32), ("incoming_a3", U32)]


SpawnHookFn = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32)


class SpawnHooks(C.Structure):
    _fields_ = [("user", P), ("world", P), ("PlaySfx", SpawnHookFn),
                ("MatrixLookDir", SpawnHookFn), ("MatrixFromDirXZ", SpawnHookFn),
                ("position_padding", PositionPaddingFn)]


class WorldHooks(C.Structure):
    _fields_ = [("user", P), ("GetSolidHeight", SpawnHookFn), ("PlaySfxAt", SpawnHookFn),
                ("GimmickActivate", SpawnHookFn), ("MeshInitFromAreaTable", SpawnHookFn),
                ("FUN_8013d358", SpawnHookFn), ("CameraShake", SpawnHookFn),
                ("EventCamStart", SpawnHookFn), ("ground_stale_x", C.c_uint16),
                ("ground_stale_z", C.c_uint16)]


BodyPrintfFn = C.CFUNCTYPE(None, P, P, U32, I32)
BodySoundStartFn = C.CFUNCTYPE(I32, P, P, I32, I32)
BodySoundStopFn = C.CFUNCTYPE(None, P, P, I32, I32)
BodyMoveFn = C.CFUNCTYPE(None, P, P, I32, U32, U32)
BodyProbeFn = C.CFUNCTYPE(None, P, P, I32, U32)
BodyStubFn = C.CFUNCTYPE(I32, P, P)
BodyRayFn = C.CFUNCTYPE(I32, P, P, U32, U32, C.POINTER(C.c_int16))
BodyGimmickSfxFn = C.CFUNCTYPE(I32, P, P, I32)
BodySfxAtFn = C.CFUNCTYPE(None, P, P, I32, C.POINTER(C.c_int16))
BodyPickupFn = C.CFUNCTYPE(None, P, P, I32)
BodyCameraNudgeFn = C.CFUNCTYPE(None, P, P, I32, I32, I32)


class BodyHooks(C.Structure):
    _fields_ = [("user", P), ("printf_error", BodyPrintfFn), ("sound_loop_start", BodySoundStartFn),
                ("sound_loop_stop", BodySoundStopFn), ("body_move", BodyMoveFn),
                ("build_probe_points", BodyProbeFn), ("body_stub_return_zero", BodyStubFn),
                ("collision_ray_test", BodyRayFn), ("gimmick_type_to_sfx", BodyGimmickSfxFn),
                ("play_sfx_at", BodySfxAtFn), ("pickup_collect", BodyPickupFn),
                ("camera_nudge", BodyCameraNudgeFn)]


class CSide:
    def __init__(self, dll: Path):
        self.lib = C.CDLL(str(dll))
        lib = self.lib
        lib.ae_ram_create.restype = P
        lib.ae_ram_destroy.argtypes = [P]
        lib.ae_ram_clear_error.argtypes = [P]
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_failed.restype = I32
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        lib.ae_ptr.argtypes = [P, U32, U32]
        lib.ae_ptr.restype = P
        self.ram = lib.ae_ram_create()
        if not self.ram:
            raise RuntimeError("ae_ram_create failed")
        self.main = (C.c_uint8 * 0x200000).from_address(lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.padding_values = {}
        self.spawn_padding_values = {}
        self.sound_calls = []
        self.player_calls = []
        self.transition_calls = []
        self.renderer_calls = []
        self.selection_calls = []
        self.condition_results = {}
        self.spawn_calls = []
        self.world_calls = []
        self.car_calls = []
        self.turn_calls = []
        self.contact_calls = []
        self.monkey_calls = []
        self.act_calls = []
        self.solid_height = 0
        self.can_activate = 0
        self.sound_handle = 0
        self.keep_padding = PositionPaddingFn(
            lambda user, ram, original_function: self.padding_values.get(original_function, 0))
        self.keep_sfx = PlaySfxFn(lambda user, ram, sound_id: self.sound_calls.append(u32(sound_id)))
        self.keep_aim_leave = SimpleHookFn(lambda user, ram: self.player_calls.append(("AimModeLeave", ())))
        self.keep_stop_sounds = StopGadgetSoundsFn(
            lambda user, ram, gadget: self.player_calls.append(("PlayerStopGadgetSounds", (gadget,))))
        self.keep_can_activate = CanActivateGadgetFn(
            lambda user, ram, gadget: self._can_activate_gadget(gadget))
        self.keep_condition = PlayerConditionFn(
            lambda user, ram, function: self._player_condition(function))
        self.keep_action = PlayerActionFn(
            lambda user, ram, function: self.transition_calls.append(("action", u32(function))))
        self.keep_renderer_hooks = []
        self.keep_base_renderer_hooks = {}
        renderer_fields = (
            ("AnimDecodeFrame", 1), ("MatrixFromFrontUp", 4), ("ScaleMatrixL", 2),
            ("ModelTransformSkinned", 1), ("ModelPrepCycleColors", 2),
            ("ModelEmitPrimsCycleColors", 3), ("ModelDraw", 3), ("DrawRibbonBetween", 4),
        )
        renderer_callbacks = []
        for hook_name, argument_count in renderer_fields:
            def record_renderer(user, ram, a0, a1, a2, a3,
                                hook_name=hook_name, argument_count=argument_count):
                arguments = tuple(u32(value) for value in (a0, a1, a2, a3)[:argument_count])
                self.renderer_calls.append((hook_name, arguments))
            callback = AddressHookFn(record_renderer)
            self.keep_renderer_hooks.append(callback)
            self.keep_base_renderer_hooks[hook_name] = callback
            renderer_callbacks.append(callback)
        self.keep_extended_renderer_hooks = {}
        extended_renderer_fields = (
            ("MatrixLookDir", 2), ("ModelTransformRigid", 3), ("ModelDrawWithOt", 4),
            ("ActorAnimDecodeEntry", 2), ("RotMatrixY", 2),
        )
        for hook_name, argument_count in extended_renderer_fields:
            def record_extended_renderer(user, ram, a0, a1, a2, a3,
                                         hook_name=hook_name, argument_count=argument_count):
                arguments = tuple(u32(value) for value in (a0, a1, a2, a3)[:argument_count])
                self.renderer_calls.append((hook_name, arguments))
            callback = AddressHookFn(record_extended_renderer)
            self.keep_renderer_hooks.append(callback)
            self.keep_extended_renderer_hooks[hook_name] = callback

        self.gte_matrix = 0

        def set_gte_matrix(name, address):
            address = u32(address)
            self.renderer_calls.append((name, (address,)))
            self.gte_matrix = address

        self.keep_set_rot_matrix = AddressHookFn(
            lambda user, ram, matrix, a1, a2, a3:
                set_gte_matrix("SetRotMatrix", matrix))
        self.keep_set_trans_matrix = AddressHookFn(
            lambda user, ram, matrix, a1, a2, a3:
                set_gte_matrix("SetTransMatrix", matrix))

        def transform_svector(input_address, output_address):
            input_offset = u32(input_address) - 0x80000000
            matrix_offset = u32(self.gte_matrix) - 0x80000000
            if not (0 <= input_offset <= len(self.main) - 6 and
                    0 <= matrix_offset <= len(self.main) - 32):
                values = (0, 0, 0)
            else:
                vector = struct.unpack_from("<hhh", self.main, input_offset)
                rows = ((0, 2, 4), (6, 8, 10), (12, 14, 16))
                translation_offsets = (20, 24, 28)
                values = []
                for row, offsets, translation_offset in zip(range(3), rows, translation_offsets):
                    rotation = tuple(struct.unpack_from("<h", self.main,
                                                        matrix_offset + offset)[0]
                                     for offset in offsets)
                    translation = struct.unpack_from("<i", self.main,
                                                     matrix_offset + translation_offset)[0]
                    result = (sum(rotation[i] * vector[i] for i in range(3)) +
                              translation * 0x1000) >> 12
                    values.append(max(-0x8000, min(0x7FFF, result)))
                values = tuple(values)
            output_offset = u32(output_address) - 0x80000000
            if 0 <= output_offset <= len(self.main) - 6:
                struct.pack_into("<hhh", self.main, output_offset, *values)

        self.keep_rot_trans_sv = AddressHookFn(
            lambda user, ram, source, output, depth, flags:
                self.renderer_calls.append(("RotTransSV", (u32(source), u32(output), u32(depth)))) or
                transform_svector(source, output))

        def project_point(source, screen_xy, depth, flags):
            source_offset = u32(source) - 0x80000000
            if 0 <= source_offset <= len(self.main) - 6:
                x, y, z = struct.unpack_from("<hhh", self.main, source_offset)
                screen_offset = u32(screen_xy) - 0x80000000
                if 0 <= screen_offset <= len(self.main) - 4:
                    struct.pack_into("<I", self.main, screen_offset,
                                     (x & 0xFFFF) | ((y & 0xFFFF) << 16))
                depth_offset = u32(depth) - 0x80000000
                if 0 <= depth_offset <= len(self.main) - 4:
                    struct.pack_into("<I", self.main, depth_offset, z & 0xFFFFFFFF)
            flags_offset = u32(flags) - 0x80000000
            if 0 <= flags_offset <= len(self.main) - 4:
                struct.pack_into("<I", self.main, flags_offset, 0)

        self.keep_rot_trans_pers = AddressHookFn(
            lambda user, ram, source, screen_xy, depth, flags:
                self.renderer_calls.append(("RotTransPers", tuple(u32(value) for value in
                    (source, screen_xy, depth, flags)))) or
                project_point(source, screen_xy, depth, flags))
        self.keep_aim_enter = SimpleHookFn(
            lambda user, ram: self.selection_calls.append(("AimModeEnter", ())))
        self.keep_trail_ring_reset = SimpleHookFn(
            lambda user, ram: self.selection_calls.append(("TrailRingReset", ())))
        self.keep_camera_fixed_mode_off = SimpleHookFn(
            lambda user, ram: self.contact_calls.append(("CameraFixedModeOff", ())))
        self.keep_contact_aim_clear = SimpleHookFn(
            lambda user, ram: self.contact_calls.append(("AimRequestClear", ())))
        self.keep_turn_towards_y = AddressHookFn(self._turn_towards_y)
        self.keep_pad_rumble = RumbleFn(
            lambda user, ram, motor, strength, frames:
                self.monkey_calls.append(("PadRumble", (motor, strength, frames))))
        self.keep_event_cam_start_random = SimpleHookFn(
            lambda user, ram: self.monkey_calls.append(("EventCamStartRandom", ())))
        self.keep_anim_frame_ptr = AnimFramePtrFn(
            lambda user, ram, instance, frame: self._anim_frame_ptr(instance, frame))
        self.keep_act_aim_clear = SimpleHookFn(
            lambda user, ram: self.contact_calls.append(("AimRequestClear", ())))
        self.keep_act_rumble = RumbleFn(
            lambda user, ram, motor, strength, frames:
                self.contact_calls.append(("PadRumble", (motor, strength, frames))))
        self.keep_act_damage = DamageFn(
            lambda user, ram, amount: self._player_damage(amount))
        self.act_hooks = PlayerActHooks()
        self.act_hooks.AnimFramePtr = self.keep_anim_frame_ptr
        self.keep_act_player_select = PlayerSelectActFn(self._act_player_select)
        self.keep_act_heap_free = PlayerActAddressFn(self._act_heap_free)
        self.keep_act_idle_setup = PlayerActAddressFn(self._act_idle_setup)
        self.act_hooks.PlayerSelectGadget = C.cast(self.keep_act_player_select, P)
        self.act_hooks.HeapFree = C.cast(self.keep_act_heap_free, P)
        self.act_hooks.PlayerIdleModelSetup = C.cast(self.keep_act_idle_setup, P)
        self.act_hooks.AimRequestClear = C.cast(self.keep_act_aim_clear, P)
        self.act_hooks.PadRumble = C.cast(self.keep_act_rumble, P)
        self.act_hooks.PlayerDamage = C.cast(self.keep_act_damage, P)
        self.player2_hooks = Player2Hooks(None, self.keep_padding, self.keep_sfx,
                                          self.keep_aim_leave, self.keep_stop_sounds,
                                          self.keep_can_activate, self.keep_condition,
                                          self.keep_action, *renderer_callbacks,
                                          self.keep_aim_enter, self.keep_trail_ring_reset,
                                          self.keep_camera_fixed_mode_off, self.keep_contact_aim_clear,
                                          self.keep_turn_towards_y,
                                          self.keep_pad_rumble,
                                          self.keep_extended_renderer_hooks["MatrixLookDir"],
                                          self.keep_extended_renderer_hooks["ModelTransformRigid"],
                                          self.keep_extended_renderer_hooks["ModelDrawWithOt"],
                                          self.keep_extended_renderer_hooks["ActorAnimDecodeEntry"],
                                          self.keep_set_rot_matrix, self.keep_set_trans_matrix,
                                          self.keep_rot_trans_sv, self.keep_rot_trans_pers,
                                          self.keep_extended_renderer_hooks["RotMatrixY"],
                                          self.keep_event_cam_start_random)
        self.keep_spawn_padding = PositionPaddingFn(
            lambda user, ram, original_function: self.spawn_padding_values.get(original_function, 0))
        null_spawn_hook = SpawnHookFn()
        self.keep_matrix_look_dir = SpawnHookFn(self._matrix_look_dir)
        self.keep_solid_height = SpawnHookFn(self._get_solid_height)
        self.world_hooks = WorldHooks(None, self.keep_solid_height, null_spawn_hook, null_spawn_hook,
                                      null_spawn_hook, null_spawn_hook, null_spawn_hook,
                                      null_spawn_hook, 0, 0)
        world_pointer = C.cast(C.pointer(self.world_hooks), P)
        self.spawn_hooks = SpawnHooks(None, world_pointer, null_spawn_hook, self.keep_matrix_look_dir,
                                      null_spawn_hook, self.keep_spawn_padding)
        null_printf = BodyPrintfFn()
        null_sound_stop = BodySoundStopFn()
        null_body_move = BodyMoveFn()
        null_probe = BodyProbeFn()
        null_body_stub = BodyStubFn()
        null_ray = BodyRayFn()
        null_gimmick_sfx = BodyGimmickSfxFn()
        null_sfx_at = BodySfxAtFn()
        null_pickup = BodyPickupFn()
        null_nudge = BodyCameraNudgeFn()
        self.keep_body_sound_start = BodySoundStartFn(
            lambda user, ram, category, sound_id: self._body_sound_start(category, sound_id))
        self.body_hooks = BodyHooks(None, null_printf, self.keep_body_sound_start, null_sound_stop,
                                    null_body_move, null_probe, null_body_stub, null_ray,
                                    null_gimmick_sfx, null_sfx_at, null_pickup, null_nudge)
        for name, (_, _, symbol, returns) in FUNCS.items():
            fn = getattr(lib, symbol)
            fn.argtypes = [P]
            if name == "PlayerOnWallBump":
                fn.argtypes.append(P)
                fn.argtypes.append(U32)
            elif name == "FUN_8013f0c0":
                fn.argtypes.append(P)
            elif name == "PlayerUpdateHeldByObject":
                fn.argtypes.extend([P, P])
            elif name == "PlayerDrawSquashed":
                fn.argtypes.append(P)
            elif name in ("PlayerDrawSquashedWithFx", "PlayerDrawSquashedFlattened"):
                fn.argtypes.extend([P, P])
            elif name == "PlayerUpdateIdleAnim":
                fn.argtypes.extend([P, P])
            elif name == "PlayerSelectGadget":
                fn.argtypes.extend([P, P, P, P, P, I32, I32])
            elif name in ("PlayerUpdateClimb", "PlayerUpdateRope"):
                fn.argtypes.extend([P, P])
            elif name == "PlayerOnContact":
                fn.argtypes.extend([P, P, P, P, P])
            elif name == "MonkeyNetCatchDraw":
                fn.argtypes.append(P)
            elif name == "MonkeyNetCatchUpdate":
                fn.argtypes.extend([P, P])
            elif name in ("PlayerApplyGadgetStance", "PlayPlayerVoice", "PlayerSpawnDust") or name in PADDING_SLOT:
                fn.argtypes.append(P)
                if name == "PlayPlayerVoice":
                    fn.argtypes.extend([U32, U32])
            elif name in ("PlayerAccelXZ", "PlayerAccelXYZ"):
                fn.argtypes.append(U32)
            elif name == "SoundSetVolumeMode":
                fn.argtypes.append(U32)
            elif name == "SpawnRandomParticle":
                fn.argtypes.extend([U32, U32, U32, I32])
            elif name in ("Max16", "SpuSetMainVolume"):
                fn.argtypes.extend([U32, U32])
            fn.restype = I32 if returns else None
        lib.ae_PlayerFxSpawn10C.argtypes = [P, P]
        lib.ae_PlayerFxSpawn10C.restype = None
        lib.ae_PlayerCheckWaterEntry.argtypes = [P, P, P]
        lib.ae_PlayerCheckWaterEntry.restype = I32
        lib.ae_PlayerSplashStart.argtypes = [P, P, P]
        lib.ae_PlayerSplashStart.restype = None
        lib.ae_PlayerRunTransitions.argtypes = [P, P, P, U32]
        lib.ae_PlayerRunTransitions.restype = None
        lib.ae_PlayerUpdateHeldByObject.argtypes = [P, P, P]
        lib.ae_PlayerUpdateHeldByObject.restype = None
        lib.ae_PlayerFxSquashParticles.argtypes = [P, P, P]
        lib.ae_PlayerFxSquashParticles.restype = None
        lib.ae_PlayerFxFlattenedParticles.argtypes = [P, P, P]
        lib.ae_PlayerFxFlattenedParticles.restype = None
        lib.ae_PlayerDrawSquashed.argtypes = [P, P]
        lib.ae_PlayerDrawSquashed.restype = None
        lib.ae_PlayerDrawSquashedWithFx.argtypes = [P, P, P]
        lib.ae_PlayerDrawSquashedWithFx.restype = None
        lib.ae_PlayerDrawSquashedFlattened.argtypes = [P, P, P]
        lib.ae_PlayerDrawSquashedFlattened.restype = None
        lib.ae_PlayerUpdateClimb.argtypes = [P, P, P]
        lib.ae_PlayerUpdateClimb.restype = None
        lib.ae_PlayerUpdateRope.argtypes = [P, P, P]
        lib.ae_PlayerUpdateRope.restype = None
        lib.ae_PlayerOnContact.argtypes = [P, P, P, P, P, P]
        lib.ae_PlayerOnContact.restype = I32
        lib.ae_MonkeyNetCatchDraw.argtypes = [P, P]
        lib.ae_MonkeyNetCatchDraw.restype = None
        lib.ae_MonkeyNetCatchUpdate.argtypes = [P, P, P]
        lib.ae_MonkeyNetCatchUpdate.restype = None
        lib.ae_PlayerUpdateIdleAnim.argtypes = [P, P, P]
        lib.ae_PlayerUpdateIdleAnim.restype = None
        lib.ae_PlayerSelectGadget.argtypes = [P, P, P, P, P, P, I32, I32]
        lib.ae_PlayerSelectGadget.restype = I32
        lib.ae_EffectGroundImpact.argtypes = [P, P, P, P, I32, I32, I32]
        lib.ae_EffectGroundImpact.restype = None
        lib.ae_EffectGroundImpactAtPlayer.argtypes = [P, P, P, P]
        lib.ae_EffectGroundImpactAtPlayer.restype = None
        lib.ae_RcCarSpawn.argtypes = [P, P, P, U32, U32, U32]
        lib.ae_RcCarSpawn.restype = None

    def _get_solid_height(self, user, ram, x, y, z, unused):
        args = tuple(mr.signed(value) for value in (x, y, z))
        self.world_calls.append(("GetSolidHeight", args))
        self.scratch[0x3F0:0x3F2] = b"\x12\x34"
        return u32(self.solid_height)

    def _can_activate_gadget(self, gadget):
        self.car_calls.append(("PlayerCanActivateGadget", (gadget,)))
        return self.can_activate

    def _player_condition(self, function):
        function = u32(function)
        self.transition_calls.append(("condition", function))
        return self.condition_results.get(function, 0)

    def _matrix_look_dir(self, user, ram, direction, matrix, unused_a2, unused_a3):
        self.spawn_calls.append(("MatrixLookDir", u32(direction)))
        start = u32(matrix) - 0x80000000
        if 0 <= start <= len(self.main) - 32:
            self.main[start:start + 32] = bytes(range(32))
        return 0

    def _body_sound_start(self, category, sound_id):
        self.car_calls.append(("SoundLoopStart", (category, sound_id)))
        return u32(self.sound_handle)

    def _anim_frame_ptr(self, instance, frame):
        self.act_calls.append(("AnimFramePtr", (u32(instance), u32(frame))))
        return u32(0x80153100 ^ u32(frame))

    def _act_player_select(self, user, ram, gadget, previous_gadget):
        self.act_calls.append(("PlayerSelectGadget", (gadget, previous_gadget)))

    def _act_heap_free(self, user, ram, address):
        self.act_calls.append(("HeapFree", (u32(address),)))

    def _act_idle_setup(self, user, ram, blob):
        self.act_calls.append(("PlayerIdleModelSetup", (u32(blob),)))

    def _player_damage(self, amount):
        self.contact_calls.append(("PlayerDamage", (amount,)))
        level_offset = 0x800F4474 - 0x80000000
        if amount == 0 or struct.unpack_from("<h", self.main, level_offset)[0] in (0x1F, 0x20):
            return
        health_offset = PLAYER - 0x80000000 + 0xE8
        health = struct.unpack_from("<h", self.main, health_offset)[0]
        struct.pack_into("<h", self.main, health_offset, max(0, health - amount))
        struct.pack_into("<h", self.main, PLAYER - 0x80000000 + 0xEA, 0x3C)
        self.main[PLAYER - 0x80000000 + 0xF0] = 1

    def _turn_towards_y(self, user, ram, target, direction, max_angle, unused):
        self.turn_calls.append((u32(target), u32(direction), u32(max_angle)))

    def load(self, snapshot):
        C.memmove(self.main, snapshot[0], len(snapshot[0]))
        C.memmove(self.scratch, snapshot[1], len(snapshot[1]))
        self.lib.ae_ram_clear_error(self.ram)

    def call(self, name, args):
        _, _, symbol, _ = FUNCS[name]
        fn = getattr(self.lib, symbol)
        if name == "PlayerApplyGadgetStance":
            return fn(self.ram, None)
        if name == "PlayerCheckWaterEntry":
            self.player_calls.clear()
            self.sound_calls.clear()
            return fn(self.ram, C.byref(self.player2_hooks), C.byref(self.spawn_hooks))
        if name == "PlayerSplashStart":
            self.sound_calls.clear()
            return fn(self.ram, C.byref(self.spawn_hooks), C.byref(self.player2_hooks))
        if name == "PlayerRunTransitions":
            self.transition_calls.clear()
            return fn(self.ram, C.byref(self.player2_hooks), None, *(u32(arg) for arg in args))
        if name == "PlayerUpdateHeldByObject":
            return fn(self.ram, C.byref(self.player2_hooks), None)
        if name in PLAYER_FX_FUNCTIONS:
            self.sound_calls.clear()
            self.spawn_calls.clear()
            return fn(self.ram, C.byref(self.spawn_hooks), C.byref(self.player2_hooks))
        if name in DRAW_FUNCTIONS:
            self.renderer_calls.clear()
            self.sound_calls.clear()
            self.spawn_calls.clear()
            if name == "PlayerDrawSquashed":
                return fn(self.ram, C.byref(self.player2_hooks))
            return fn(self.ram, C.byref(self.player2_hooks), C.byref(self.spawn_hooks))
        if name == "PlayerUpdateIdleAnim":
            self.renderer_calls.clear()
            return fn(self.ram, C.byref(self.player2_hooks), None)
        if name == "PlayerSelectGadget":
            self.selection_calls.clear()
            self.car_calls.clear()
            self.player_calls.clear()
            self.act_calls.clear()
            return fn(self.ram, C.byref(self.player2_hooks), C.byref(self.act_hooks),
                      C.byref(self.spawn_hooks), C.byref(self.body_hooks), None,
                      I32(args[0]), I32(args[1]))
        if name in ("PlayerUpdateClimb", "PlayerUpdateRope"):
            self.turn_calls.clear()
            self.act_calls.clear()
            return fn(self.ram, C.byref(self.player2_hooks), C.byref(self.act_hooks))
        if name == "PlayerOnContact":
            self.contact_calls.clear()
            self.sound_calls.clear()
            self.act_calls.clear()
            self.world_calls.clear()
            return fn(self.ram, C.byref(self.player2_hooks), C.byref(self.act_hooks),
                      C.byref(self.world_hooks), C.byref(self.spawn_hooks), None)
        if name in ("MonkeyNetCatchDraw", "MonkeyNetCatchUpdate"):
            self.renderer_calls.clear()
            self.monkey_calls.clear()
            self.sound_calls.clear()
            self.gte_matrix = 0
            if name == "MonkeyNetCatchDraw":
                return fn(self.ram, C.byref(self.player2_hooks))
            return fn(self.ram, C.byref(self.player2_hooks), C.byref(self.act_hooks))
        if name in PADDING_SLOT or name in ("PlayPlayerVoice", "PlayerOnWallBump"):
            self.sound_calls.clear()
            return fn(self.ram, C.byref(self.player2_hooks), *(u32(arg) for arg in args))
        if name == "PlayerSpawnDust":
            return fn(self.ram, C.byref(self.player2_hooks))
        if name == "FUN_8013f0c0":
            return fn(self.ram, C.byref(self.player2_hooks))
        if name == "PlayerFxSpawn10C":
            return fn(self.ram, C.byref(self.spawn_hooks))
        if name in GROUND_IMPACT_PADDING_SLOT:
            self.world_calls.clear()
            world = C.cast(C.pointer(self.world_hooks), P)
            spawn = C.cast(C.pointer(self.spawn_hooks), P)
            if name == "EffectGroundImpact":
                return fn(self.ram, world, spawn, C.byref(self.player2_hooks),
                          *(I32(arg) for arg in args))
            return fn(self.ram, world, spawn, C.byref(self.player2_hooks))
        if name == "RcCarSpawn":
            self.car_calls.clear()
            return fn(self.ram, C.byref(self.player2_hooks), C.byref(self.body_hooks),
                      *(u32(arg) for arg in args))
        return fn(self.ram, *(u32(arg) for arg in args))

    def snapshot(self):
        return bytes(self.main), bytes(self.scratch)

    def close(self):
        if self.ram:
            self.lib.ae_ram_destroy(self.ram)
            self.ram = None


def player_state_tables(cpu):
    """Load the real player state-set blob and rebase its four table pointers."""
    blob = (ROOT / "extract/members/008.bin").read_bytes()
    cpu.load_area(blob, addr=0x80103000)
    state_sets = 0x800F4DE0
    for index in range(10):
        for field_index in range(4):
            offset = int.from_bytes(blob[0x14 + index * 0x10 + field_index * 4:
                                         0x16 + index * 0x10 + field_index * 4], "little") * 4
            cpu.write32(state_sets + index * 0x10 + field_index * 4, 0x80103000 + offset)


def make_bases():
    areas = dump_areas.area_members()
    bases = {}
    for area_id in AREA_IDS:
        member = areas[area_id]
        area_path = ROOT / "extract/members" / f"{member:03X}.bin"
        cpu = mr.MipsRam.from_extract(area=area_path)
        area_blob = area_path.read_bytes()
        area_header = struct.unpack_from("<I", area_blob, 0x4D0)[0]
        area_grid = area_header + 4 * struct.unpack_from("<I", area_blob, area_header + 4)[0]
        width, depth, color_count = struct.unpack_from("<HHI", area_blob, area_grid)
        cells = area_grid + 8
        heights = cells + width * depth * 12 + color_count * 4
        cpu.write32(0x800F44E4, mr.AREA_BASE + area_header)
        cpu.write16(0x800EBFA0, width)
        cpu.write16(0x800EBFA2, depth)
        cpu.write32(0x800EBFA8, mr.AREA_BASE + cells)
        cpu.write32(0x800EBFB4, mr.AREA_BASE + heights)
        player_state_tables(cpu)
        bases[area_id] = cpu.snapshot()
    return bases


def random_bytes(rng, count):
    return bytes(rng.randrange(256) for _ in range(count))


def seed_case(cpu, name, args, fields, rng):
    # Random sentinels make wrong field widths, addresses, and stray writes visible.
    if name == "PlayerIsOnGround":
        cpu.write32(PLAYER + 0x12C, fields["ground_y"])
        cpu.write32(PLAYER + 0x28, fields["position_y"])
        cpu.write16(PLAYER + 0x136, fields["has_ground"])
    elif name == "PlayerCheckWaterEntry":
        cpu.write32(PLAYER + 0x70, fields["state_flags"])
        cpu.write16(PLAYER + 0x138, fields["ground_type"])
        cpu.write8(PLAYER + 0xF2, fields["gadget"])
        cpu.write8(0x800E6916, fields["aim_mode"])
        cpu.write8(0x800E6902, fields.get("aim_request", 0))
        cpu.write32(0x800F51E0, 0x12345678)
        cpu.write32(PLAYER + 0x7C, 0x87654321)
        cpu.write_bytes(PLAYER + 0x60, random_bytes(rng, 8))
        cpu.write_bytes(PLAYER + 0x80, random_bytes(rng, 8))
        cpu.write_bytes(0x800F51E8, random_bytes(rng, 4))
        cpu.write_bytes(GADGET4 + 4, random_bytes(rng, 0x1C))
        cpu.write8(0x800BAD78, 0)
        cpu.write16(SPLASH_GROUP_PADDING_SLOT[name], rng.randrange(0x10000))
    elif name in ("PlayerAccelXZ", "PlayerAccelXYZ"):
        for axis in range(3):
            cpu.write32(PLAYER + PLAYER_MOVE_VEL[axis], fields["move"][axis])
            cpu.write32(PLAYER + PLAYER_TARGET_MOVE_VEL[axis], fields["target"][axis])
    elif name == "PlayerStartHurtBlink":
        cpu.write_bytes(0x800F4DCC, random_bytes(rng, 6))
    elif name == "PlayerEnterSwimIfInWater":
        cpu.write16(PLAYER + 0x138, fields["ground_type"])
        cpu.write16(PLAYER + 0x5E, fields["old_state"])
        cpu.write16(PLAYER + 0x5C, rng.randrange(0x80))
        cpu.write8(PLAYER + 0xF2, fields.get("gadget", 0))
    elif name == "PlayerApplyGadgetStance":
        state = fields["state"]
        cpu.write8(PLAYER + 0xF2, fields["gadget"])
        cpu.write16(PLAYER + 0x5E, state)
        state_table = cpu.read32(PLAYER_STATE_SETS + 4)
        last_frame = cpu.read16(state_table + state * 8 + 2)
        cpu.write16(PLAYER + 0x5C, (last_frame + 1) & 0xFFFF)
    elif name == "PlayerResetAir":
        cpu.write_bytes(0x800F4DC6, random_bytes(rng, 6))
    elif name == "PlayerFxResetBlink":
        cpu.write_bytes(0x800BAD98, random_bytes(rng, 4))
    elif name == "PlayerOnWallBump":
        cpu.write16(PLAYER + 0xE8, fields["health"])
        cpu.write16(PLAYER + 0x60, fields["facing"][0])
        cpu.write16(PLAYER + 0x64, fields["facing"][1])
        cpu.write16(PLAYER + 0x5E, 0x20)
        cpu.write16(PLAYER + 0x5C, 0)
        for axis, offset in enumerate(PLAYER_MOVE_VEL):
            cpu.write32(PLAYER + offset, fields["move"][axis])
        cpu.write16(WALL_NORMAL, fields["normal"][0])
        cpu.write16(WALL_NORMAL + 2, rng.randrange(0x10000))
        cpu.write16(WALL_NORMAL + 4, fields["normal"][1])
    elif name == "PlayerSpawnDust":
        cpu.write16(0x800F4476, fields["current_area"])
        cpu.write32(0x800EC350, fields["frame"])
        for axis, offset in enumerate((0x24, 0x28, 0x2C)):
            cpu.write32(PLAYER + offset, fields["position"][axis])
        for axis, offset in enumerate((0x30, 0x34, 0x38)):
            cpu.write32(PLAYER + offset, fields["velocity"][axis])
        if fields["current_area"] == 2:
            cpu.write16(0x800AE8F0, fields.get("primary_seed", rng.randrange(0x10000)))
            cpu.write16(0x800AE8F2, fields.get("secondary_seed", rng.randrange(0x10000)))
            cpu.write16(AREA2_DUST_PADDING_NESTED, rng.randrange(0x10000))
    elif name == "FUN_8013f0c0":
        for axis, offset in enumerate((0x24, 0x28, 0x2C)):
            cpu.write16(PLAYER + offset, fields["position"][axis])
        cpu.write16(0x800AE8F0, fields["primary_seed"])
        cpu.write16(0x800AE8F2, fields["secondary_seed"])
        cpu.write16(AREA2_DUST_PADDING_DIRECT, rng.randrange(0x10000))
    elif name == "PlayerFxSpawn10C":
        for offset, value in enumerate(fields["position"]):
            cpu.write32(0x800B4D88 + 4 * offset, value)
        cpu.write16(0x800AE8F0, fields["rand_seed"])
        cpu.write16(SPAWN_GROUP_PADDING_SLOT, rng.randrange(0x10000))
    elif name in GROUND_IMPACT_PADDING_SLOT:
        if name == "EffectGroundImpactAtPlayer":
            for axis, offset in enumerate((0x24, 0x28, 0x2C)):
                cpu.write32(PLAYER + offset, fields["position"][axis])
        cpu.write16(0x800AE8F0, fields["rand_seed"])
        cpu.write16(GROUND_IMPACT_PADDING_SLOT[name], rng.randrange(0x10000))
    elif name == "RcCarSpawn":
        cpu.write32(0x800F5A90, fields["active"])
        cpu.write16(0x800F5A98, fields["cooldown"])
        cpu.write_bytes(PLAYER + 0x60, random_bytes(rng, 16))
        cpu.write_bytes(0x800BBDA0, b"\xFF" * (256 * 0x14))
        cpu.write32(0x800B20D8, 0)
        cpu.write32(0x800B20DC, 0)
        cpu.write_bytes(0x800B20D0, random_bytes(rng, 4))
    elif name in PADDING_SLOT:
        x, y, z = fields["position"]
        cpu.write16(PLAYER + 0x24, x)
        cpu.write16(PLAYER + 0x28, y)
        cpu.write16(PLAYER + 0x2C, z)
        if name == "PlayerSpawnDustBehind":
            cpu.write16(PLAYER + 0x60, fields["facing"][0])
            cpu.write16(PLAYER + 0x64, fields["facing"][1])
        else:
            cpu.write16(0x800AE8F0, fields["rand_seed"])
        cpu.write16(PADDING_SLOT[name], rng.randrange(0x10000))
    elif name == "AimTableClear3":
        cpu.write_bytes(0x800F508C, random_bytes(rng, 28))
    elif name == "PlayerFxClearDAT_800F5144":
        cpu.write_bytes(GADGET4 + 0x20, random_bytes(rng, 0x38))
    elif name == "IsPauseMenuOpen":
        cpu.write_bytes(0x800BEF70, random_bytes(rng, 8))
        cpu.write8(0x800BEF74, fields["pause_mode"])
    elif name == "RadarResetState":
        cpu.write_bytes(PLAYER + 0x60, random_bytes(rng, 8))
        cpu.write_bytes(GADGET4 + 4, random_bytes(rng, 0x1C))
    elif name == "SpuSetMainVolume":
        cpu.write16(0x800F75F2, fields["mono"])
        cpu.write32(0x800E1874, SPU_SHADOW_BUFFER)
        cpu.write_bytes(SPU_SHADOW_BUFFER, random_bytes(rng, 0x1C0))
    elif name == "SoundSetVolumeMode":
        cpu.write16(0x800BF67E, fields["reverb"])
        cpu.write16(0x800BF680, fields["main"])
        cpu.write16(0x800F75F2, fields["mono"])
        cpu.write32(0x800E1874, SPU_SHADOW_BUFFER)
        cpu.write_bytes(SPU_SHADOW_BUFFER, random_bytes(rng, 0x1C0))
    elif name == "SpawnRandomParticle":
        cpu.write16(0x800AE8F0, fields["rand_seed"])
        cpu.write8(RANDOM_PARTICLE_CURSOR, fields["cursor"])
    elif name == "PlayerSplashStart":
        # A nonzero surface and distinct previous height expose 8006B8B8's
        # shared water-height store, previously hidden by the zero fixture.
        cpu.write16(0x1F8003DE, 0xFAD6)
        cpu.write16(0x1F8003E4, 0)
        cpu.write16(0x800B4D90, 0x1357)
        cpu.write8(0x800BAD78, fields["fade"])
        for axis, offset in enumerate((0x24, 0x28, 0x2C)):
            cpu.write32(PLAYER + offset, fields["position"][axis])
        cpu.write32(PLAYER + 0x34, fields["move_y"])
        cpu.write16(SPLASH_GROUP_PADDING_SLOT[name], rng.randrange(0x10000))
    elif name == "PlayerRunTransitions":
        cpu.write16(PLAYER + 0x5E, fields["state"])
        transition_table = args[0]
        for index, entry in enumerate(fields["entries"]):
            cpu.write_bytes(transition_table + index * 4, bytes(value & 0xFF for value in entry))
        for index, function in TRANSITION_CONDITION_FUNCTIONS.items():
            cpu.write32(0x800B0298 + index * 4, function)
        for index, function in TRANSITION_ACTION_FUNCTIONS.items():
            cpu.write32(0x800B03AC + index * 4, function)
    elif name == "PlayerUpdateHeldByObject":
        cpu.write16(PLAYER + 0x5E, fields["state"])
        cpu.write8(PLAYER + 0xF2, fields["gadget"])
        cpu.write32(PLAYER + 0x50, fields["contact"])
        cpu.write16(PLAYER + 0x5C, fields["anim_frame"])
        cpu.write16(0x800E54DC, fields["travel"][0])
        cpu.write16(0x800E54DE, fields["travel"][1])
        cpu.write16(0x800F44A0, 0)
        for offset in PLAYER_MOVE_VEL:
            cpu.write32(PLAYER + offset, rng.getrandbits(32))
        cpu.write32(EMPTY_TRANSITIONS, 0x000000FF)
        player_sets = PLAYER_STATE_SETS
        gadget_set = player_sets + (fields["gadget"] + 1) * 0x10
        cpu.write32(player_sets + 8, EMPTY_TRANSITIONS)
        cpu.write32(gadget_set + 8, EMPTY_TRANSITIONS)
        body = HELD_OBJECT_BODY
        cpu.write32(0x800BBDA0 + fields["body_index"] * 0x14 + 4, body)
        for axis, offset in enumerate((0, 4, 8)):
            cpu.write32(body + offset, fields["body_pos"][axis])
        for axis, value in enumerate(fields["body_facing"]):
            cpu.write16(body + 0x18 + axis * 2, value)
        cpu.write16(body + 0x24, fields["body_type"])
    elif name in PLAYER_FX_FUNCTIONS:
        cpu.write16(0x800EC2E0, fields["frame"])
        cpu.write16(0x800AE8F0, fields["rand_seed"])
        if name == "PlayerFxSquashParticles":
            for axis, offset in enumerate((0x24, 0x28, 0x2C)):
                cpu.write32(PLAYER + offset, fields["player_pos"][axis])
        for axis, value in enumerate(fields["fx_pos"]):
            cpu.write16(0x800B4D88 + axis * 2, value)
        cpu.write16(FX_GROUP_PADDING_SLOT, rng.randrange(0x10000))
    elif name in DRAW_FUNCTIONS:
        model = 0x80170000
        bones = 0x80170100
        matrices = 0x80170200
        cpu.write16(0x800EC2E0, fields["frame"])
        cpu.write16(0x800AE8F0, fields["rand_seed"])
        cpu.write32(PLAYER + 0x00, model)
        cpu.write32(PLAYER + 0x0C, bones)
        cpu.write32(PLAYER + 0x14, matrices)
        cpu.write16(model + 4, 1)
        cpu.write16(model + 6, 17)
        cpu.write_bytes(bones, bytes(12))
        cpu.write16(bones + 10, 0)
        cpu.write_bytes(matrices, bytes(32))
        for offset in (0, 10, 16):
            cpu.write16(matrices + offset, 0x1000)
        for axis, value in enumerate((100, 400, -200)):
            cpu.write32(PLAYER + 0x24 + axis * 4, value)
        for offset in range(0, 32, 4):
            cpu.write32(0x800E78C0 + offset, 0)
        for offset in (0, 10, 16):
            cpu.write16(0x800E78C0 + offset, 0x1000)
        if name in DRAW_GROUP_PADDING_SLOT:
            cpu.write16(DRAW_GROUP_PADDING_SLOT[name], rng.randrange(0x10000))
    elif name == "PlayerUpdateIdleAnim":
        idle = 0x80171000
        model = 0x80172000
        bones = 0x80172100
        matrices = 0x80172200
        animation = 0x80172300
        cpu.write32(0x800B4D70, idle)
        cpu.write32(idle + 0x00, animation)
        cpu.write32(idle + 0x04, 0x80172400)
        cpu.write32(idle + 0x08, EMPTY_TRANSITIONS)
        cpu.write32(idle + 0x0C, 0x80172500)
        cpu.write32(idle + 0x10, model)
        cpu.write32(idle + 0x1C, bones)
        cpu.write32(idle + 0x24, matrices)
        cpu.write32(idle + 0x28, animation)
        cpu.write16(idle + 0x86, fields["state"])
        cpu.write32(animation, 0)
        cpu.write16(model + 4, 1)
        cpu.write16(model + 6, 9)
        cpu.write_bytes(bones, bytes(12))
        cpu.write16(bones + 10, 0)
        cpu.write_bytes(matrices, bytes(32))
        for offset in (0, 10, 16):
            cpu.write16(matrices + offset, 0x1000)
        cpu.write16(PLAYER + 0x5E, 0)
        cpu.write16(PLAYER + 0x5C, fields["player_frame"])
        cpu.write32(EMPTY_TRANSITIONS, 0xFFFFFFFF)
        cpu.write32(0x800EC350, fields["frame"])
        cpu.write32(0x800EC35C, fields["draw_buffer"])
        for axis, value in enumerate((100, 400, -200)):
            cpu.write32(PLAYER + 0x24 + axis * 4, value)
        cpu.write_bytes(0x800E78C0, bytes(32))
        for offset in (0, 10, 16):
            cpu.write16(0x800E78C0 + offset, 0x1000)
    elif name == "PlayerSelectGadget":
        gadget, previous_gadget = args
        cpu.write8(PLAYER + 0xF2, fields["current"])
        cpu.write8(PLAYER + 0xF3, 0xFF)
        cpu.write16(PLAYER + 0x5E, fields["state"])
        cpu.write32(PLAYER + 0x70, fields["state_flags"])
        for axis, value in enumerate(fields["position"]):
            cpu.write32(PLAYER + 0x24 + axis * 4, value)
        cpu.write32(PLAYER + 0x12C, fields["ground_y"])
        cpu.write8(0x800E6916, fields["aim_mode"])
        cpu.write8(0x800E6902, fields.get("aim_request", 0))
        cpu.write8(0x800BEF74, fields["pause_mode"])
        cpu.write_bytes(0x800F5120, random_bytes(rng, 0x30))
        cpu.write16(0x800F51BA, 0x55)
        cpu.write8(0x800F51BC, 0x77)
        cpu.write8(0x800F51C0, 0x20)
        cpu.write16(0x800F508C, rng.randrange(0x10000))
        cpu.write32(0x800B4D40, 0x80173000)
        cpu.write8(0x800E68D0 + 0x18, 0)
        cpu.write_bytes(0x80173000, bytes(0x100))
        cpu.write_bytes(0x800BBDA0, b"\xFF" * (256 * 0x14))
        cpu.write32(0x800F5A90, fields["car_active"])
        cpu.write16(0x800F5A98, fields["car_cooldown"])
        cpu.write32(0x800B20D8, 0)
        cpu.write32(0x800B20DC, 0)
        cpu.write_bytes(0x800B20D0, random_bytes(rng, 4))
        for sound_id in (3, 5, 0x24, 0x2E):
            cpu.write16(0x800B2D08 + sound_id * 8, 0xFFFF)
        cpu.write16(0x800BF66E, 0xFFFF)
        cpu.write16(0x800AE8F0, rng.randrange(0x10000))
        cpu.write16(GADGET_SELECT_GROUP_PADDING_SLOT, rng.randrange(0x10000))
        if gadget == 7 and previous_gadget == 7:
            cpu.write_bytes(PLAYER + 0x60, random_bytes(rng, 16))
    elif name in ("PlayerUpdateClimb", "PlayerUpdateRope"):
        cpu.write16(PLAYER + 0x5E, fields["state"])
        cpu.write8(PLAYER + 0xF2, fields.get("gadget", 0))
        cpu.write16(PLAYER + 0x5C, fields.get("frame", 2))
        cpu.write16(PLAYER + 0x5E, fields["state"])
        for axis, value in enumerate(fields["position"]):
            cpu.write32(PLAYER + 0x24 + axis * 4, value)
        facing = fields.get("facing", (0x1000, 0, 0, 0x1000))
        desired = fields.get("desired", facing)
        for axis, value in enumerate(facing):
            cpu.write16(PLAYER + 0x60 + axis * 2, value)
        for axis, value in enumerate(desired):
            cpu.write16(PLAYER + 0x74 + axis * 2, value)
        cpu.write32(PLAYER + 0xE0, fields.get("attach_flags", 4))
        cpu.write32(PLAYER + 0x70, 0)
        cpu.write32(0x1F800080, fields.get("input_block", 0))
        cpu.write32(0x1F800054, fields.get("held", 0))
        cpu.write32(0x1F800058, fields.get("changed", 0))
        cpu.write16(0x1F800064, fields.get("gesture", 0))
        cpu.write32(0x800F44A0, 0)
        cpu.write8(0x800E6916, fields.get("aim_mode", 0))
        cpu.write8(0x800E53C2, fields.get("stick_x", 0))
        cpu.write8(0x800E53C3, fields.get("stick_y", 0))
        cpu.write16(0x800E54A0, fields.get("rope_stick", 0))
        cpu.write16(0x800E5498, fields.get("jump_allowed", 0))
        cpu.write_bytes(PLAYER_STATE_TABLE, bytes(0x400))
        for index in range(128):
            cpu.write16(PLAYER_STATE_TABLE + index * 8, fields.get("table_start", 0))
            cpu.write16(PLAYER_STATE_TABLE + index * 8 + 2, fields.get("table_end", 20))
        cpu.write32(EMPTY_TRANSITIONS, 0xFFFFFFFF)
        for set_index in (0, 1):
            state_set = PLAYER_STATE_SETS + set_index * 0x10
            cpu.write32(state_set + 4, PLAYER_STATE_TABLE)
            cpu.write32(state_set + 8, EMPTY_TRANSITIONS)
            cpu.write32(state_set + 12, 0)
        if name == "PlayerUpdateClimb":
            for axis, value in enumerate(fields["target"]):
                cpu.write16(PLAYER + 0xB4 + axis * 2, value)
            cpu.write16(PLAYER + 0x5C, fields["frame"])
        else:
            for axis, value in enumerate(fields["rope"]):
                cpu.write16(PLAYER + 0xC0 + axis * 2, value)
            cpu.write16(PLAYER + 0xD0, fields.get("rope_normal_x", 0))
            cpu.write16(PLAYER + 0xD4, fields.get("rope_normal_z", 0))
            cpu.write32(0x800EC1E0 + 0x14, PLAYER_BONE_MATRICES)
            cpu.write_bytes(PLAYER_BONE_MATRICES, bytes(0x40))
    elif name == "PlayerOnContact":
        body = 0x80153000
        cpu.write16(PLAYER + 0x5E, fields["state"])
        cpu.write32(PLAYER + 0x70, fields["state_flags"])
        cpu.write32(PLAYER + 0x50, fields["contact"])
        cpu.write16(PLAYER + 0xE8, fields["health"])
        cpu.write16(PLAYER + 0xEA, fields["invuln"])
        cpu.write8(PLAYER + 0xEF, fields["contact_frames"])
        cpu.write8(PLAYER + 0xF0, fields["hurt"])
        cpu.write8(PLAYER + 0xF1, fields["cooldown"])
        cpu.write8(0x800F4DCE, fields["hurt_blink"])
        cpu.write16(0x800F44A0, fields["mode"])
        cpu.write8(0x800E6916, fields["aim_mode"])
        cpu.write8(0x800E6902, fields["aim_request"])
        cpu.write8(0x800E6908, fields["camera_fixed"])
        cpu.write16(PLAYER + 0x138, fields["ground_type"])
        cpu.write32(PLAYER + 0x12C, fields["ground_y"])
        for axis, value in enumerate(fields["position"]):
            cpu.write32(PLAYER + 0x24 + axis * 4, value)
        cpu.write_bytes(body, bytes(0x40))
        for axis, value in enumerate(fields["body_pos"]):
            cpu.write32(body + axis * 4, value)
        cpu.write16(body + 0x24, fields["body_type"])
        cpu.write16(body + 0x26, fields["body_flags"])
        cpu.write32(0x800BBDA0 + 3 * 0x14 + 4, body)
        cpu.write16(0x800F4474, 0)
        cpu.write_bytes(PLAYER_STATE_TABLE, bytes(0x400))
        for index in range(128):
            cpu.write16(PLAYER_STATE_TABLE + index * 8, 0)
            cpu.write16(PLAYER_STATE_TABLE + index * 8 + 2, 20)
        cpu.write32(EMPTY_TRANSITIONS, 0xFFFFFFFF)
        for set_index in (0, 1):
            state_set = PLAYER_STATE_SETS + set_index * 0x10
            cpu.write32(state_set + 4, PLAYER_STATE_TABLE)
            cpu.write32(state_set + 8, EMPTY_TRANSITIONS)
            cpu.write32(state_set + 12, 0)
        for sound_id in (0x33, 0x34, 0x35, 0x42, 0x4A, 0x5B, 0x5F, 0x84, 0x88, 0x89, 0x8B):
            cpu.write16(0x800B2D08 + sound_id * 8, 0xFFFF)
    elif name in ("MonkeyNetCatchDraw", "MonkeyNetCatchUpdate"):
        player_model = 0x80150000
        player_bones = 0x80150200
        player_matrices = 0x80152000
        catch_model = 0x80150020
        catch_bones = 0x80150220
        catch_matrices = 0x80152200
        static_model_header = 0x80150040
        static_model_slot = 2

        def identity_matrix(address):
            cpu.write_bytes(address, bytes(32))
            for offset in (0, 10, 16):
                cpu.write16(address + offset, 0x1000)

        def seed_actor(actor, model, bones, matrices, position):
            cpu.write32(actor, model)
            cpu.write32(actor + 0x0C, bones)
            cpu.write32(actor + 0x14, matrices)
            cpu.write16(model + 4, 1)
            cpu.write16(model + 6, 16)
            cpu.write_bytes(bones, bytes(12))
            cpu.write16(bones + 10, 0)
            for axis, value in enumerate(position):
                cpu.write32(actor + 0x24 + axis * 4, value)
            identity_matrix(matrices)
            identity_matrix(matrices + 32)

        cpu.write16(PLAYER + 0x5E, fields["state"])
        cpu.write16(PLAYER + 0x5C, 7)
        for axis, value in enumerate((0, 0, 0x1000, 0)):
            cpu.write16(PLAYER + 0x60 + axis * 2, value)
        for axis, value in enumerate((0, 0x1000, 0, 0)):
            cpu.write16(PLAYER + 0x68 + axis * 2, value)
        cpu.write32(PLAYER + 0x14, player_matrices)
        seed_actor(PLAYER, player_model, player_bones, player_matrices, (10, 20, 30))
        seed_actor(0x800F50B0, catch_model, catch_bones, catch_matrices, (40, 50, 60))
        cpu.write16(0x800F4474, fields["level"])
        cpu.write32(0x800EC35C, fields["draw_buffer"])
        cpu.write16(0x800F5174, fields.get("frame", 0))
        cpu.write32(0x800B4CF0, 0x80153080)
        cpu.write16(0x800AEB5A, 0)
        cpu.write16(0x80153080, 0x40)
        caught_group_lookup = bytearray(range(256))
        caught_group_lookup[1] = static_model_slot
        cpu.write_bytes(0x800AEBF8, caught_group_lookup)
        cpu.write32(0x800F4630 + static_model_slot * 0x10, static_model_header)
        cpu.write16(static_model_header + 4, 2)
        cpu.write32(0x800EC07C, 0x80154000)
        cpu.write32(0x800EC14C, 0x80154200)
        cpu.write_bytes(0x80154000, random_bytes(rng, 0x1000))
        cpu.write_bytes(0x80154200, random_bytes(rng, 0x1000))
        for point in range(0x52):
            for axis, value in enumerate((point * 2, point * 3, point * 4, 0)):
                cpu.write16(0x80154000 + point * 8 + axis * 2, value)
        for point in range(0x0D):
            for axis, value in enumerate((point * 2, point * 3, point * 4, 0)):
                cpu.write16(0x80154200 + point * 8 + axis * 2, value)
        for matrix_index in range(32):
            identity_matrix(0x800E9C60 + matrix_index * 32)
        identity_matrix(0x800E9E40)
        identity_matrix(0x800F4910)
        identity_matrix(0x800E78C0)
        cpu.write_bytes(0x800F4B30, bytes(0x100))
        cpu.write32(0x800B4DA0, 0x801530C0)
        cpu.write16(0x801530C0, fields.get("first_frame", 0))
        cpu.write16(0x801530C2, fields.get("last_frame", 40))
        cpu.write16(0x800F510C, fields.get("anim_frame", 0))
        for table in (0x800B2140, 0x800B2188):
            for point in range(6):
                record = table + point * 12
                cpu.write16(record, point)
                cpu.write16(record + 2, point)
                cpu.write16(record + 4, point * 7 - 12)
                cpu.write16(record + 6, point * 3 + 4)
                cpu.write16(record + 8, point * 9 - 20)
                cpu.write16(record + 10, 0)


class Harness:
    def __init__(self, source=None, target=None, mips=None):
        self.m = mips or mr.MipsRam.from_extract()
        if mips is None:
            self.m.attach_gte_oracle()
        self.m.coverage = set()
        self.bases = make_bases()
        self.area2_dust_overlay = AREA2_DUST_OVERLAY.read_bytes()
        self.c = CSide(build(source))
        self.target = target
        self.counts = {name: 0 for name in FUNCS}
        self.failures = []
        self.mips_sound_calls = []
        self.mips_player_calls = []
        self.mips_world_calls = []
        self.mips_car_calls = []
        self.mips_transition_calls = []
        self.mips_spawn_calls = []
        self.mips_renderer_calls = []
        self.mips_selection_calls = []
        self.mips_turn_calls = []
        self.mips_monkey_calls = []
        reg.declare("stack_window", f"{mr.STACK_WINDOW[0]:08X}..{mr.STACK_WINDOW[1]:08X}")

    def run_case(self, name, case_index, args, fields, seed):
        area = AREA_IDS[case_index % len(AREA_IDS)]
        self.m.restore(self.bases[area])
        if name == "FUN_8013f0c0" or (name == "PlayerSpawnDust" and fields["current_area"] == 2):
            self.m.write_bytes(AREA2_DUST_OVERLAY_ADDRESS, self.area2_dust_overlay)
        rng = random.Random(seed)
        seed_case(self.m, name, args, fields, rng)
        before = self.m.snapshot()
        self.c.load(before)
        # These are caller-specific uninitialized stack values. Each case must
        # install only its own values; retaining a previous function's padding
        # made the C side consume stale fixture state that the MIPS side did not.
        self.c.padding_values.clear()
        self.c.spawn_padding_values.clear()
        if name == "FUN_8013f0c0" or (name == "PlayerSpawnDust" and fields["current_area"] == 2):
            reg.declare("shared_random", "FUN_8013f0c0 stack position padding")
            padding_slot = (AREA2_DUST_PADDING_DIRECT if name == "FUN_8013f0c0"
                            else AREA2_DUST_PADDING_NESTED)
            padding_offset = padding_slot - 0x80000000
            self.c.padding_values[0x8013F0C0] = int.from_bytes(
                before[0][padding_offset:padding_offset + 2], "little")
        if name in PADDING_SLOT:
            reg.declare("shared_random", f"{name} original stack position padding")
            padding_offset = PADDING_SLOT[name] - 0x80000000
            self.c.padding_values[FUNCS[name][0]] = int.from_bytes(
                before[0][padding_offset:padding_offset + 2], "little")
        if name == "PlayerFxSpawn10C":
            reg.declare("shared_random", "PlayerFxSpawn10C SpawnEffectGroup stack position padding")
            padding_offset = SPAWN_GROUP_PADDING_SLOT - 0x80000000
            self.c.spawn_padding_values[0x800696EC] = int.from_bytes(
                before[0][padding_offset:padding_offset + 2], "little")
        if name == "PlayerSelectGadget":
            reg.declare("shared_random", "PlayerSelectGadget GadgetSelectFx SpawnEffectGroup stack position padding")
            padding_offset = GADGET_SELECT_GROUP_PADDING_SLOT - 0x80000000
            self.c.spawn_padding_values[0x800696EC] = int.from_bytes(
                before[0][padding_offset:padding_offset + 2], "little")
            self.c.can_activate = fields["can_activate"]
            self.c.sound_handle = fields["sound_handle"]
        if name in SPLASH_GROUP_PADDING_SLOT:
            reg.declare("shared_random", f"{name} SpawnEffectGroup stack position padding")
            padding_offset = SPLASH_GROUP_PADDING_SLOT[name] - 0x80000000
            self.c.spawn_padding_values[0x800696EC] = int.from_bytes(
                before[0][padding_offset:padding_offset + 2], "little")
        if name in PLAYER_FX_FUNCTIONS:
            reg.declare("shared_random", f"{name} SpawnEffectGroup stack position padding")
            padding_offset = FX_GROUP_PADDING_SLOT - 0x80000000
            self.c.spawn_padding_values[0x800696EC] = int.from_bytes(
                before[0][padding_offset:padding_offset + 2], "little")
        if name in DRAW_GROUP_PADDING_SLOT:
            reg.declare("shared_random", f"{name} SpawnEffectGroup stack position padding")
            padding_offset = DRAW_GROUP_PADDING_SLOT[name] - 0x80000000
            self.c.spawn_padding_values[0x800696EC] = int.from_bytes(
                before[0][padding_offset:padding_offset + 2], "little")
        if name in GROUND_IMPACT_PADDING_SLOT:
            reg.declare("shared_random", f"{name} original stack position padding")
            padding_offset = GROUND_IMPACT_PADDING_SLOT[name] - 0x80000000
            self.c.padding_values[FUNCS[name][0]] = int.from_bytes(
                before[0][padding_offset:padding_offset + 2], "little")
            self.c.solid_height = fields["solid"]
        if name == "PlayerOnContact":
            self.c.solid_height = fields.get("solid", 0)
        try:
            mips_hooks = None
            if name == "PlayerRunTransitions":
                self.mips_transition_calls.clear()
                self.c.condition_results = {
                    TRANSITION_CONDITION_FUNCTIONS[index]: result
                    for index, result in fields["condition_results"].items()
                }

                def make_condition(index):
                    def condition(cpu, regs):
                        function = TRANSITION_CONDITION_FUNCTIONS[index]
                        self.mips_transition_calls.append(("condition", function))
                        return u32(fields["condition_results"].get(index, 0))
                    return condition

                def make_action(index):
                    def action(cpu, regs):
                        function = TRANSITION_ACTION_FUNCTIONS[index]
                        self.mips_transition_calls.append(("action", function))
                        return 0
                    return action

                mips_hooks = {
                    **{function: make_condition(index)
                       for index, function in TRANSITION_CONDITION_FUNCTIONS.items()},
                    **{function: make_action(index)
                       for index, function in TRANSITION_ACTION_FUNCTIONS.items()},
                }
            elif name in ("PlayPlayerVoice", "PlayerOnWallBump"):
                self.mips_sound_calls.clear()

                def play_sfx(cpu, regs):
                    self.mips_sound_calls.append(u32(regs[4]))
                    return 0

                mips_hooks = {0x800877F8: play_sfx}
            elif name == "PlayerCheckWaterEntry":
                self.mips_player_calls.clear()
                self.mips_sound_calls.clear()

                def stop_sounds(cpu, regs):
                    self.mips_player_calls.append(("PlayerStopGadgetSounds", (mr.signed(regs[4]),)))
                    return 0

                def aim_leave(cpu, regs):
                    self.mips_player_calls.append(("AimModeLeave", ()))
                    return 0

                def play_sfx(cpu, regs):
                    self.mips_sound_calls.append(u32(regs[4]))
                    return 0

                mips_hooks = {0x8007A180: stop_sounds, 0x8004B510: aim_leave,
                              0x800877F8: play_sfx}
            elif name == "PlayerSplashStart":
                self.mips_sound_calls.clear()

                def play_sfx(cpu, regs):
                    self.mips_sound_calls.append(u32(regs[4]))
                    return 0

                mips_hooks = {0x800877F8: play_sfx}
            elif name in PLAYER_FX_FUNCTIONS:
                self.mips_sound_calls.clear()
                self.mips_spawn_calls.clear()

                def play_sfx(cpu, regs):
                    self.mips_sound_calls.append(u32(regs[4]))
                    return 0

                def matrix_look_dir(cpu, regs):
                    self.mips_spawn_calls.append(("MatrixLookDir", u32(regs[4])))
                    cpu.write_bytes(regs[5], bytes(range(32)))
                    return 0

                mips_hooks = {0x800877F8: play_sfx, 0x8001484C: matrix_look_dir}
            elif name in DRAW_FUNCTIONS:
                self.mips_sound_calls.clear()
                self.mips_spawn_calls.clear()
                self.mips_renderer_calls.clear()

                def play_sfx(cpu, regs):
                    self.mips_sound_calls.append(u32(regs[4]))
                    return 0

                def matrix_look_dir(cpu, regs):
                    self.mips_spawn_calls.append(("MatrixLookDir", u32(regs[4])))
                    cpu.write_bytes(regs[5], bytes(range(32)))
                    return 0

                def make_renderer(hook_name, argument_count):
                    def renderer(cpu, regs):
                        values = tuple(u32(regs[index]) for index in range(4, 4 + argument_count))
                        self.mips_renderer_calls.append((hook_name, values))
                        return 0
                    return renderer

                mips_hooks = {0x800877F8: play_sfx, 0x8001484C: matrix_look_dir}
                mips_hooks.update({address: make_renderer(hook_name, argument_count)
                                   for address, (hook_name, argument_count) in DRAW_RENDER_HOOKS.items()})
            elif name == "PlayerUpdateIdleAnim":
                self.mips_renderer_calls.clear()

                def make_renderer(hook_name, argument_count):
                    def renderer(cpu, regs):
                        values = tuple(u32(regs[index]) for index in range(4, 4 + argument_count))
                        self.mips_renderer_calls.append((hook_name, values))
                        return 0
                    return renderer

                idle_hook_addresses = (0x80014A84, 0x80016A60, 0x80017CD4, 0x8002020C)
                mips_hooks = {address: make_renderer(*DRAW_RENDER_HOOKS[address])
                              for address in idle_hook_addresses}
            elif name == "PlayerSelectGadget":
                self.mips_player_calls.clear()
                self.mips_car_calls.clear()
                self.mips_selection_calls.clear()
                self.mips_act_calls = []

                def stop_sounds(cpu, regs):
                    self.mips_player_calls.append(("PlayerStopGadgetSounds", (mr.signed(regs[4]),)))
                    return 0

                def aim_enter(cpu, regs):
                    self.mips_selection_calls.append(("AimModeEnter", ()))
                    return 0

                def trail_reset(cpu, regs):
                    self.mips_selection_calls.append(("TrailRingReset", ()))
                    return 0

                def can_activate(cpu, regs):
                    gadget_id = mr.signed(regs[4])
                    self.mips_car_calls.append(("PlayerCanActivateGadget", (gadget_id,)))
                    return u32(fields["can_activate"])

                def sound_loop_start(cpu, regs):
                    args2 = (mr.signed(regs[4]), mr.signed(regs[5]))
                    self.mips_car_calls.append(("SoundLoopStart", args2))
                    return u32(fields["sound_handle"])

                def anim_frame_ptr(cpu, regs):
                    args2 = (u32(regs[4]), u32(regs[5]))
                    self.mips_act_calls.append(("AnimFramePtr", args2))
                    return u32(0x80153100 ^ u32(regs[5]))

                mips_hooks = {0x8007A180: stop_sounds, 0x8004B4B8: aim_enter,
                              0x8001F884: trail_reset, 0x8007CA48: can_activate,
                              0x80015240: anim_frame_ptr,
                              0x800878E0: sound_loop_start}
            elif name in GROUND_IMPACT_PADDING_SLOT:
                self.mips_world_calls.clear()

                def solid_height(cpu, regs):
                    args = tuple(mr.signed(regs[index]) for index in (4, 5, 6))
                    self.mips_world_calls.append(("GetSolidHeight", args))
                    cpu.write16(0x1F8003F0, 0x3412)
                    return u32(fields["solid"])

                mips_hooks = {0x8004D8C0: solid_height}
            elif name == "RcCarSpawn":
                self.mips_car_calls.clear()
                self.c.can_activate = fields["can_activate"]
                self.c.sound_handle = fields["sound_handle"]

                def can_activate(cpu, regs):
                    gadget = mr.signed(regs[4])
                    self.mips_car_calls.append(("PlayerCanActivateGadget", (gadget,)))
                    return u32(fields["can_activate"])

                def sound_loop_start(cpu, regs):
                    args = (mr.signed(regs[4]), mr.signed(regs[5]))
                    self.mips_car_calls.append(("SoundLoopStart", args))
                    return u32(fields["sound_handle"])

                mips_hooks = {0x8007CA48: can_activate, 0x800878E0: sound_loop_start}
            elif name in ("PlayerUpdateClimb", "PlayerUpdateRope"):
                self.mips_turn_calls.clear()
                self.mips_act_calls = []

                def turn_towards_y(cpu, regs):
                    values = tuple(u32(regs[index]) for index in range(4, 7))
                    self.mips_turn_calls.append(values)
                    return 0

                def anim_frame_ptr(cpu, regs):
                    values = (u32(regs[4]), u32(regs[5]))
                    self.mips_act_calls.append(("AnimFramePtr", values))
                    return u32(0x80153100 ^ u32(regs[5]))

                mips_hooks = {0x80066C90: turn_towards_y, 0x80015240: anim_frame_ptr}
            elif name == "PlayerOnContact":
                self.mips_sound_calls.clear()
                self.mips_contact_calls = []
                self.mips_act_calls = []
                self.mips_world_calls.clear()

                def damage(cpu, regs):
                    amount = mr.signed(regs[4])
                    self.mips_contact_calls.append(("PlayerDamage", (amount,)))
                    level = cpu.read16(0x800F4474)
                    if amount == 0 or level in (0x1F, 0x20):
                        return 0
                    health = mr.signed(cpu.read16(PLAYER + 0xE8))
                    cpu.write16(PLAYER + 0xE8, max(0, health - amount))
                    cpu.write16(PLAYER + 0xEA, 0x3C)
                    cpu.write8(PLAYER + 0xF0, 1)
                    return 0

                def rumble(cpu, regs):
                    values = tuple(mr.signed(regs[index]) for index in (4, 5, 6))
                    self.mips_contact_calls.append(("PadRumble", values))
                    return 0

                def aim_clear(cpu, regs):
                    self.mips_contact_calls.append(("AimRequestClear", ()))
                    return 0

                def fixed_mode_off(cpu, regs):
                    self.mips_contact_calls.append(("CameraFixedModeOff", ()))
                    return 0

                def play_sfx(cpu, regs):
                    sound_id = u32(regs[4])
                    if 0x84 <= sound_id <= 0x8B:
                        self.mips_sound_calls.append(sound_id)
                    return 0

                def anim_frame_ptr(cpu, regs):
                    values = (u32(regs[4]), u32(regs[5]))
                    self.mips_act_calls.append(("AnimFramePtr", values))
                    return u32(0x80153100 ^ u32(regs[5]))

                def solid_height(cpu, regs):
                    args = tuple(mr.signed(regs[index]) for index in (4, 5, 6))
                    self.mips_world_calls.append(("GetSolidHeight", args))
                    cpu.write16(0x1F8003F0, 0x3412)
                    return u32(fields.get("solid", 0))

                def player_select(cpu, regs):
                    values = (mr.signed(regs[4]), mr.signed(regs[5]))
                    self.mips_act_calls.append(("PlayerSelectGadget", values))
                    return 0

                def heap_free(cpu, regs):
                    self.mips_act_calls.append(("HeapFree", (u32(regs[4]),)))
                    return 0

                def idle_setup(cpu, regs):
                    self.mips_act_calls.append(("PlayerIdleModelSetup", (u32(regs[4]),)))
                    return 0

                mips_hooks = {0x8005E6F0: damage, 0x80014168: rumble,
                              0x80048430: aim_clear, 0x80048748: fixed_mode_off,
                              0x800877F8: play_sfx, 0x80015240: anim_frame_ptr,
                              0x8004D8C0: solid_height, 0x8007AC64: player_select,
                              0x8008C064: heap_free, 0x80032ADC: idle_setup}
            elif name in ("MonkeyNetCatchDraw", "MonkeyNetCatchUpdate"):
                self.mips_renderer_calls.clear()
                self.mips_monkey_calls.clear()
                self.mips_sound_calls.clear()
                current_matrix = [0]

                def record_renderer(hook_name, argument_count):
                    def renderer(cpu, regs):
                        values = tuple(u32(regs[index]) for index in range(4, 4 + argument_count))
                        self.mips_renderer_calls.append((hook_name, values))
                        return 0
                    return renderer

                def set_matrix(hook_name):
                    def matrix(cpu, regs):
                        address = u32(regs[4])
                        self.mips_renderer_calls.append((hook_name, (address,)))
                        current_matrix[0] = address
                        return 0
                    return matrix

                def transform_svector(cpu, regs):
                    source, output, depth = (u32(regs[index]) for index in (4, 5, 6))
                    self.mips_renderer_calls.append(("RotTransSV", (source, output, depth)))
                    matrix = current_matrix[0]
                    vector = tuple(mr.signed(cpu.read16(source + axis * 2), 16) for axis in range(3))
                    result = []
                    for row in range(3):
                        rotation = tuple(mr.signed(cpu.read16(matrix + row * 6 + col * 2), 16)
                                         for col in range(3))
                        translation = mr.signed(cpu.read32(matrix + 20 + row * 4), 32)
                        transformed = (sum(rotation[col] * vector[col] for col in range(3)) +
                                       translation * 0x1000) >> 12
                        result.append(max(-0x8000, min(0x7FFF, transformed)))
                    for axis, value in enumerate(result):
                        cpu.write16(output + axis * 2, value)
                    return 0

                def transform_projected(cpu, regs):
                    source, screen_xy, depth, flags = (u32(regs[index]) for index in range(4, 8))
                    self.mips_renderer_calls.append(
                        ("RotTransPers", (source, screen_xy, depth, flags)))
                    x = mr.signed(cpu.read16(source), 16)
                    y = mr.signed(cpu.read16(source + 2), 16)
                    z = mr.signed(cpu.read16(source + 4), 16)
                    cpu.write32(screen_xy, (x & 0xFFFF) | ((y & 0xFFFF) << 16))
                    cpu.write32(depth, z)
                    cpu.write32(flags, 0)
                    return 0

                def rumble(cpu, regs):
                    values = tuple(mr.signed(regs[index]) for index in (4, 5, 6))
                    self.mips_monkey_calls.append(("PadRumble", values))
                    return 0

                def event_camera(cpu, regs):
                    self.mips_monkey_calls.append(("EventCamStartRandom", ()))
                    return 0

                def play_sfx(cpu, regs):
                    self.mips_sound_calls.append(u32(regs[4]))
                    return 0

                mips_hooks = {
                    0x80014D70: record_renderer("AnimDecodeFrame", 1),
                    0x80014A84: record_renderer("MatrixFromFrontUp", 4),
                    0x80016A60: record_renderer("ModelTransformSkinned", 1),
                    0x80017CD4: record_renderer("ModelDraw", 3),
                    0x800161C0: record_renderer("ModelTransformRigid", 3),
                    0x80017D94: record_renderer("ModelDrawWithOt", 4),
                    0x80014EF0: record_renderer("ActorAnimDecodeEntry", 2),
                    0x800305B0: record_renderer("ScaleMatrixL", 2),
                    0x80030890: record_renderer("RotMatrixY", 2),
                    0x8002FB00: set_matrix("SetRotMatrix"),
                    0x8002FB30: set_matrix("SetTransMatrix"),
                    0x80031230: transform_svector,
                    0x80031330: transform_projected,
                    0x80014168: rumble,
                    0x8004BC9C: event_camera,
                    0x800877F8: play_sfx,
                }
            mips_result = self.m.call(FUNCS[name][0], args, hooks=mips_hooks)[2]
        except mr.MipsError as error:
            self.failures.append(f"{name} case {case_index}: original MIPS error: {error}")
            return
        c_result = self.c.call(name, args)
        if self.c.lib.ae_ram_failed(self.c.ram):
            self.failures.append(f"{name} case {case_index}: C RAM error: "
                                 f"{self.c.lib.ae_ram_error(self.c.ram).decode(errors='replace')}")
            return
        problems = []
        if name in ("PlayPlayerVoice", "PlayerOnWallBump") and self.mips_sound_calls != self.c.sound_calls:
            problems.append(f"PlaySfx calls MIPS {self.mips_sound_calls}, C {self.c.sound_calls}")
        if name in ("PlayerCheckWaterEntry", "PlayerSplashStart") and self.mips_sound_calls != self.c.sound_calls:
            problems.append(f"PlaySfx calls MIPS {self.mips_sound_calls}, C {self.c.sound_calls}")
        if name in PLAYER_FX_FUNCTIONS and self.mips_sound_calls != self.c.sound_calls:
            problems.append(f"PlaySfx calls MIPS {self.mips_sound_calls}, C {self.c.sound_calls}")
        if name in PLAYER_FX_FUNCTIONS and self.mips_spawn_calls != self.c.spawn_calls:
            problems.append(f"spawn hooks MIPS {self.mips_spawn_calls}, C {self.c.spawn_calls}")
        if name in DRAW_FUNCTIONS and self.mips_renderer_calls != self.c.renderer_calls:
            problems.append(f"renderer hooks MIPS {self.mips_renderer_calls}, C {self.c.renderer_calls}")
        if name in DRAW_EFFECT_FUNCTIONS and self.mips_sound_calls != self.c.sound_calls:
            problems.append(f"PlaySfx calls MIPS {self.mips_sound_calls}, C {self.c.sound_calls}")
        if name in DRAW_EFFECT_FUNCTIONS and self.mips_spawn_calls != self.c.spawn_calls:
            problems.append(f"spawn hooks MIPS {self.mips_spawn_calls}, C {self.c.spawn_calls}")
        if name == "PlayerUpdateIdleAnim" and self.mips_renderer_calls != self.c.renderer_calls:
            problems.append(f"renderer hooks MIPS {self.mips_renderer_calls}, C {self.c.renderer_calls}")
        if name in ("MonkeyNetCatchDraw", "MonkeyNetCatchUpdate") and self.mips_renderer_calls != self.c.renderer_calls:
            problems.append(f"renderer hooks MIPS {self.mips_renderer_calls}, C {self.c.renderer_calls}")
        if name in ("MonkeyNetCatchDraw", "MonkeyNetCatchUpdate") and self.mips_monkey_calls != self.c.monkey_calls:
            problems.append(f"capture hooks MIPS {self.mips_monkey_calls}, C {self.c.monkey_calls}")
        if name in ("MonkeyNetCatchDraw", "MonkeyNetCatchUpdate") and self.mips_sound_calls != self.c.sound_calls:
            problems.append(f"player voice SFX MIPS {self.mips_sound_calls}, C {self.c.sound_calls}")
        if name == "PlayerSelectGadget" and self.mips_player_calls != self.c.player_calls:
            problems.append(f"player hooks MIPS {self.mips_player_calls}, C {self.c.player_calls}")
        if name == "PlayerSelectGadget" and self.mips_selection_calls != self.c.selection_calls:
            problems.append(f"selection hooks MIPS {self.mips_selection_calls}, C {self.c.selection_calls}")
        if name == "PlayerSelectGadget" and self.mips_car_calls != self.c.car_calls:
            problems.append(f"RC car hooks MIPS {self.mips_car_calls}, C {self.c.car_calls}")
        if name == "PlayerSelectGadget" and self.mips_act_calls != self.c.act_calls:
            problems.append(f"player-act hooks MIPS {self.mips_act_calls}, C {self.c.act_calls}")
        if name in ("PlayerUpdateClimb", "PlayerUpdateRope") and self.mips_turn_calls != self.c.turn_calls:
            problems.append(f"TurnTowardsY hook MIPS {self.mips_turn_calls}, C {self.c.turn_calls}")
        if name in ("PlayerUpdateClimb", "PlayerUpdateRope") and self.mips_act_calls != self.c.act_calls:
            problems.append(f"AnimFramePtr hook MIPS {self.mips_act_calls}, C {self.c.act_calls}")
        if name == "PlayerOnContact" and self.mips_contact_calls != self.c.contact_calls:
            problems.append(f"contact hooks MIPS {self.mips_contact_calls}, C {self.c.contact_calls}")
        if name == "PlayerOnContact" and self.mips_sound_calls != self.c.sound_calls:
            problems.append(f"player voice SFX MIPS {self.mips_sound_calls}, C {self.c.sound_calls}")
        if name == "PlayerOnContact" and self.mips_act_calls != self.c.act_calls:
            problems.append(f"player-act hooks MIPS {self.mips_act_calls}, C {self.c.act_calls}")
        if name == "PlayerOnContact" and self.mips_world_calls != self.c.world_calls:
            problems.append(f"world hooks MIPS {self.mips_world_calls}, C {self.c.world_calls}")
        if name == "PlayerCheckWaterEntry" and self.mips_player_calls != self.c.player_calls:
            problems.append(f"player hooks MIPS {self.mips_player_calls}, C {self.c.player_calls}")
        if name == "PlayerRunTransitions" and self.mips_transition_calls != self.c.transition_calls:
            problems.append(f"transition hooks MIPS {self.mips_transition_calls}, C {self.c.transition_calls}")
        if name in GROUND_IMPACT_PADDING_SLOT and self.mips_world_calls != self.c.world_calls:
            problems.append(f"world hooks MIPS {self.mips_world_calls}, C {self.c.world_calls}")
        if name == "RcCarSpawn" and self.mips_car_calls != self.c.car_calls:
            problems.append(f"RC car hooks MIPS {self.mips_car_calls}, C {self.c.car_calls}")
        if FUNCS[name][3] and u32(mips_result) != u32(c_result):
            problems.append(f"return MIPS {u32(mips_result):08X} C {u32(c_result):08X}")
        m_main, m_scratch = self.m.snapshot()
        c_main, c_scratch = self.c.snapshot()
        if m_main[:STACK_LO] != c_main[:STACK_LO]:
            for offset, (left, right) in enumerate(zip(m_main[:STACK_LO], c_main[:STACK_LO])):
                if left != right:
                    problems.append(f"RAM {0x80000000 + offset:08X}: MIPS {left:02X}, C {right:02X}")
                    break
        if m_scratch != c_scratch:
            for offset, (left, right) in enumerate(zip(m_scratch, c_scratch)):
                if left != right:
                    problems.append(f"scratch {0x1F800000 + offset:08X}: MIPS {left:02X}, C {right:02X}")
                    break
        if problems:
            self.failures.append(f"{name} case {case_index}: " + "; ".join(problems))
        self.counts[name] += 1

    def run_function(self, name, seed):
        for index, (args, fields) in enumerate(CASES[name]):
            self.run_case(name, index, args, fields, seed + index * 101 + FUNCS[name][0])

    def run_all(self, seed):
        for name in FUNCS:
            if self.target is not None and name != self.target:
                continue
            self.run_function(name, seed)

    def close(self):
        self.c.close()


def mutation_sources(original):
    specs = [
        ("PlayerIsOnGround", "ground equality", "ground_y <= position_y", "ground_y < position_y"),
        ("PlayerIsOnGround", "ignore has-ground", "has_ground != 0", "has_ground == 0"),
        ("PlayerIsOnGround", "read wrong vertical field", "AE_P2_PLAYER_POSITION_Y);", "AE_P2_PLAYER_GROUND_Y);"),
        ("PlayerStartHurtBlink", "blink duration off by one", "0x0Fu);", "0x0Eu);"),
        ("PlayerStartHurtBlink", "byte store", "ae_wr16(r, AE_P2_G_PLAYER_HURT_BLINK,", "ae_wr8(r, AE_P2_G_PLAYER_HURT_BLINK,"),
        ("PlayerStartHurtBlink", "write air meter instead", "AE_P2_G_PLAYER_HURT_BLINK,", "AE_P2_G_PLAYER_AIR,"),
        ("PlayerEnterSwimIfInWater", "wrong water kind", "== 1u", "== 2u"),
        ("PlayerEnterSwimIfInWater", "wrong swim state", "0x49u);", "0x48u);"),
        ("PlayerEnterSwimIfInWater", "wrong ground type shift", "ground_type >> 12", "ground_type >> 11"),
        ("PlayerResetAir", "air amount off by one", "600u);", "599u);"),
        ("PlayerResetAir", "leave warning pending", "AE_P2_G_AIR_WARNING_PENDING, 0", "AE_P2_G_AIR_WARNING_PENDING, 1"),
        ("PlayerResetAir", "clear byte only", "ae_wr16(r, AE_P2_G_PLAYER_AIR,", "ae_wr8(r, AE_P2_G_PLAYER_AIR,"),
        ("PlayerFxResetBlink", "wrong blink value", "0x10u);", "0x11u);"),
        ("PlayerFxResetBlink", "retain timer", "AE_P2_BLINK_FX_TIMER, 0", "AE_P2_BLINK_FX_TIMER, 1"),
        ("PlayerFxResetBlink", "wrong value field", "AE_P2_BLINK_FX_VALUE,", "AE_P2_BLINK_FX_TIMER,"),
        ("PlayerSpawnDustBehind", "wrong first particle", "0x14, position", "0x15, position"),
        ("PlayerSpawnDustBehind", "wrong vertical offset", "- 0x30u);", "- 0x31u);"),
        ("PlayerSpawnDustBehind", "face toward rather than away", "(uint32_t)(facing_x >> 6)", "(uint32_t)(-(facing_x >> 6))"),
        ("PlayerSparkleBurst", "omit final burst particle", "index < 10u", "index < 9u"),
        ("PlayerSparkleBurst", "wrong horizontal center", "(int32_t)(random_x & 0x7Fu) - 0x40",
         "(int32_t)(random_x & 0x7Fu) - 0x3F"),
        ("PlayerSparkleBurst", "wrong particle kind", "ae_SpawnParticle(r, NULL, 0x12, position)",
         "ae_SpawnParticle(r, NULL, 0x13, position)"),
        ("PlayPlayerVoice", "wrong voice zero sound", "sound_id = 0x86u;", "sound_id = 0x85u;"),
        ("PlayPlayerVoice", "do not suppress voice six", "case 6u:\n        return;", "case 6u:\n        break;"),
        ("PlayPlayerVoice", "wrong voice eleven sound", "sound_id = 0x8Bu;", "sound_id = 0x8Au;"),
        ("PlayerOnWallBump", "run at zero health", "<= 0) return;", "< 0) return;"),
        ("PlayerOnWallBump", "reverse wall direction test", "if (dot <= 0)", "if (dot > 0)"),
        ("PlayerOnWallBump", "wrong contact state", "ae_PlayerSetState(r, NULL, 0x42u);",
         "ae_PlayerSetState(r, NULL, 0x43u);"),
        ("PlayerOnWallBump", "over-damp on open surface", "move_x / 4", "move_x / 2"),
        ("PlayerSpawnDust", "wrong frame parity", "(ae_rd32(r, AE_P2_G_FRAME_COUNTER) & 1u) == 0", "(ae_rd32(r, AE_P2_G_FRAME_COUNTER) & 1u) != 0"),
        ("PlayerSpawnDust", "wrong dust particle", "ae_SpawnParticle(r, NULL, 4, position)",
         "ae_SpawnParticle(r, NULL, 5, position)"),
        ("PlayerSpawnDust", "use a different overlay area", "AE_P2_G_CURRENT_AREA) == 2u", "AE_P2_G_CURRENT_AREA) == 3u"),
        ("FUN_8013f0c0", "continue sampling at the limit",
         "keep_sampling = iteration < next_limit;", "keep_sampling = iteration <= next_limit;"),
        ("FUN_8013f0c0", "shift the X sample window",
         "((secondary_seed >> 8) & 0x7Fu) - 0x40u",
         "((secondary_seed >> 8) & 0x7Fu) - 0x3Fu"),
        ("PlayerFxSpawn10C", "wrong particle group", "0x10C, AE_P2_G_PLAYER_FX_POSITION", "0x10D, AE_P2_G_PLAYER_FX_POSITION"),
        ("PlayerFxSpawn10C", "offset the effect position", "AE_P2_G_PLAYER_FX_POSITION);",
         "AE_P2_G_PLAYER_FX_POSITION + 4u);"),
        ("PlayerFxSpawn10C", "omit spawn-group padding hook", "ae_SpawnParticle(r, hooks, 0x10C,",
         "(void)hooks;\n    ae_SpawnParticle(r, NULL, 0x10C,"),
        ("PlayerCheckWaterEntry", "wrong excluded player state", "state_flags == 0xC0u", "state_flags == 0xB0u"),
        ("PlayerCheckWaterEntry", "accept nonzero water subtype", "(ground_type & 0xFu) != 0", "(ground_type & 0xFu) != 1"),
        ("PlayerCheckWaterEntry", "leave aim mode on entry", "ae_rd8(r, AE_P2_G_AIM_MODE) == 1u", "ae_rd8(r, AE_P2_G_AIM_MODE) == 0u"),
        ("EffectGroundImpact", "drop final debris particle", "index < 25u", "index < 24u"),
        ("EffectGroundImpact", "change vertical random scale", "ae_p2_random_byte(r) >> 2", "ae_p2_random_byte(r) >> 1"),
        ("EffectGroundImpact", "wrong debris particle kind", "ae_SpawnParticle(r, spawn, 0x16, position)",
         "ae_SpawnParticle(r, spawn, 0x17, position)"),
        ("EffectGroundImpactAtPlayer", "use player X as height", "AE_P2_PLAYER_POSITION_Y));", "AE_P2_PLAYER_POSITION_X));"),
        ("EffectGroundImpactAtPlayer", "use direct-call stack padding", "0x8006BF84u);", "0x8006B570u);"),
        ("AimTableClear3", "omit last table entry", "index < 3u", "index < 2u"),
        ("AimTableClear3", "wrong table stride", "index * AE_P2_AIM_TABLE_STRIDE", "index * 4u"),
        ("AimTableClear3", "clear forward addresses", "AE_P2_G_AIM_TABLE_LAST - index", "AE_P2_G_AIM_TABLE_LAST + index"),
        ("PlayerFxClearDAT_800F5144", "wrong count value", "0xFFFFu);", "0xFFFEu);"),
        ("PlayerFxClearDAT_800F5144", "clear wrong initial flags", "AE_P2_GADGET_FX_CLEAR_FLAGS, 0", "AE_P2_GADGET_FX_CLEAR_TIMER, 0"),
        ("PlayerFxClearDAT_800F5144", "wrong fx direction", "0x1000u);", "0x0FFFu);"),
        ("IsPauseMenuOpen", "unsigned menu mode", "ae_rds8(r,", "ae_rd8(r,"),
        ("IsPauseMenuOpen", "toggle menu mode result", "return ae_rds8(r, AE_P2_G_PAUSE_MENU_MODE);", "return ae_rds8(r, AE_P2_G_PAUSE_MENU_MODE) ^ 1;"),
        ("IsPauseMenuOpen", "read adjacent mode byte", "AE_P2_G_PAUSE_MENU_MODE);", "AE_P2_G_PAUSE_MENU_MODE + 1u);"),
        ("RadarResetState", "copy position instead of facing", "AE_P2_PLAYER_FACING));", "AE_P2_PLAYER_POSITION_Y));"),
        ("RadarResetState", "leave marker initialized", "AE_P2_RADAR_MARKER_A, 0xFFFFu", "AE_P2_RADAR_MARKER_A, 0"),
        ("RadarResetState", "retain radar target", "AE_P2_RADAR_TARGET, 0", "AE_P2_RADAR_TARGET, 1"),
        ("Max16", "return smaller signed half", "left_s16 <= right_s16 ? right_s16 : left_s16",
         "left_s16 <= right_s16 ? left_s16 : right_s16"),
        ("Max16", "ignore signed truncation", "ae_p2_s16((uint32_t)left)", "(int32_t)left"),
        ("Max16", "off-by-one on left result", "return left_s16 <= right_s16 ? right_s16 : left_s16;",
         "return left_s16 <= right_s16 ? right_s16 : left_s16 + 1;"),
        ("PlayerAccelXZ", "take update path at exact distance", "if (max_step < distance)", "if (max_step <= distance)"),
        ("PlayerAccelXZ", "read Y target for Z delta", "AE_P2_PLAYER_TARGET_MOVE_VEL_Z),", "AE_P2_PLAYER_TARGET_MOVE_VEL_Y),"),
        ("PlayerAccelXZ", "round negative X by one less", "if (delta_x < 0) delta_x = ae_p2_add_wrap(delta_x, AE_P2_ACCEL_ROUNDING);",
         "if (delta_x < 0) delta_x = ae_p2_add_wrap(delta_x, 0x3Eu);"),
        ("PlayerAccelXYZ", "take update path at exact distance", "if (max_step < distance)", "if (max_step <= distance)"),
        ("PlayerAccelXYZ", "read Y target for Z delta", "AE_P2_PLAYER_TARGET_MOVE_VEL_Z),", "AE_P2_PLAYER_TARGET_MOVE_VEL_Y),"),
        ("PlayerAccelXYZ", "use Z delta for Y component", "scaled_y = ae_p2_shift_right_6(delta_y);",
         "scaled_y = ae_p2_shift_right_6(delta_z);"),
        ("PlayerApplyGadgetStance", "wrong stance gate", "if (has_alt_stance == 0)",
         "if (has_alt_stance != 0)"),
        ("PlayerApplyGadgetStance", "wrong normal stance state", "if (state == 4u)", "if (state == 5u)"),
        ("PlayerApplyGadgetStance", "wrong alternate walk state", "new_state = 1u;", "new_state = 2u;"),
        ("SpuSetMainVolume", "invert mono branch", "if (ae_rd16(r, AE_P2_G_SOUND_MONO) != 0)",
         "if (ae_rd16(r, AE_P2_G_SOUND_MONO) == 0)"),
        ("SpuSetMainVolume", "write left to right channel", "AE_P2_SPU_MAIN_LEFT, left_volume", "AE_P2_SPU_MAIN_RIGHT, left_volume"),
        ("SpuSetMainVolume", "left volume off by one", "AE_P2_SPU_MAIN_LEFT, left_volume",
         "AE_P2_SPU_MAIN_LEFT, left_volume + 1u"),
        ("SoundSetVolumeMode", "use main volume in mode zero", "AE_P2_G_REVERB_VOLUME);", "AE_P2_G_MAIN_VOLUME);"),
        ("SoundSetVolumeMode", "do not mute mode two", "volume = 0;", "volume = 1;"),
        ("SoundSetVolumeMode", "wrong reverb divisor", "(reverb * 0x7F) / 0x4B", "(reverb * 0x7F) / 0x4A"),
        ("RcCarSpawn", "activate the wrong gadget", "PlayerCanActivateGadget(player_hooks->user, r, 7)",
         "PlayerCanActivateGadget(player_hooks->user, r, 6)"),
        ("RcCarSpawn", "block zero cooldown", "AE_P2_G_RC_CAR + AE_P2_RC_CAR_COOLDOWN) > 0",
         "AE_P2_G_RC_CAR + AE_P2_RC_CAR_COOLDOWN) >= 0"),
        ("RcCarSpawn", "write Y to the Z slot", "AE_P2_RC_CAR_BODY + 4u, y", "AE_P2_RC_CAR_BODY + 8u, y"),
        ("SpawnRandomParticle", "wrong random particle kind mask", "(kind & 0xFFu) * 4u", "(kind & 0x7Fu) * 4u"),
        ("SpawnRandomParticle", "wrong initial velocity shift", "speed), 8);", "speed), 7);"),
        ("SpawnRandomParticle", "wrong ring size", "(cursor + 1u) & 7u", "(cursor + 1u) & 0xFu"),
        ("PlayerSplashStart", "invert splash debounce", "if (ae_rd8(r, AE_P2_G_SPLASH_FADE) != 0) return;",
         "if (ae_rd8(r, AE_P2_G_SPLASH_FADE) == 0) return;"),
        ("PlayerSplashStart", "wrong splash velocity divisor",
         "(uint8_t)splash_velocity > 0x14u ? 0x14 : splash_velocity",
         "(uint8_t)splash_velocity > 0x13u ? 0x13 : splash_velocity"),
        ("PlayerSplashStart", "wrong splash particle kind", "ae_SpawnParticle(r, spawn, 0x100, position)",
         "ae_SpawnParticle(r, spawn, 0x101, position)"),
        ("PlayerSplashStart", "lost shared water height",
         "ae_wr16(r, AE_P2_G_WATER_PARTICLE_HEIGHT, ae_rd16(r, AE_P2_G_SPLASH_SURFACE));",
         "(void)r;"),
        ("PlayerRunTransitions", "invert transition condition polarity",
         "condition_index > 0\n                ? condition_result != 0\n                : condition_result == 0",
         "condition_index > 0\n                ? condition_result == 0\n                : condition_result != 0"),
        ("PlayerRunTransitions", "run the action for a zero index",
         "if (action_index != 0)", "if (action_index == 0)"),
        ("PlayerRunTransitions", "use action byte as next state",
         "entry + AE_P2_TRANSITION_NEXT_STATE", "entry + AE_P2_TRANSITION_ACTION"),
        ("PlayerUpdateHeldByObject", "wrong animation-frame threshold",
         "stick_travel > 100 ? 2u : 1u", "stick_travel >= 100 ? 2u : 1u"),
        ("PlayerUpdateHeldByObject", "accept the wrong contact type",
         "(contact >> 28) == 0xBu", "(contact >> 28) == 0xAu"),
        ("PlayerUpdateHeldByObject", "copy position into facing",
         "body + AE_P2_BODY_FACING));", "body + AE_P2_BODY_POSITION_X));"),
        ("PlayerFxSquashParticles", "wrong direction buffer",
         "ae_p2_player_fx_squash_particles(r, spawn, hooks, AE_P2_LOCAL_SQUASH_DIRECTION);",
         "ae_p2_player_fx_squash_particles(r, spawn, hooks, AE_P2_G_PLAYER + AE_P2_PLAYER_FACING);"),
        ("PlayerFxSquashParticles", "drop particle backend",
         "ae_p2_player_fx_squash_particles(r, spawn, hooks, AE_P2_LOCAL_SQUASH_DIRECTION);",
         "ae_p2_player_fx_squash_particles(r, spawn == NULL ? spawn : NULL, hooks, AE_P2_LOCAL_SQUASH_DIRECTION);"),
        ("PlayerFxFlattenedParticles", "wrong direction buffer",
         "ae_p2_player_fx_flattened_particles(r, spawn, hooks, AE_P2_LOCAL_SQUASH_DIRECTION);",
         "ae_p2_player_fx_flattened_particles(r, spawn, hooks, AE_P2_G_PLAYER + AE_P2_PLAYER_FACING);"),
        ("PlayerFxFlattenedParticles", "drop particle backend",
         "ae_p2_player_fx_flattened_particles(r, spawn, hooks, AE_P2_LOCAL_SQUASH_DIRECTION);",
         "ae_p2_player_fx_flattened_particles(r, spawn == NULL ? spawn : NULL, hooks, AE_P2_LOCAL_SQUASH_DIRECTION);"),
        ("PlayerDrawSquashed", "use nested scale buffer",
         "ae_p2_draw_squashed(r, hooks, AE_P2_LOCAL_SQUASH_SCALE);",
         "ae_p2_draw_squashed(r, hooks, AE_P2_NESTED_SQUASH_SCALE);"),
        ("PlayerDrawSquashed", "shift scale buffer",
         "ae_p2_draw_squashed(r, hooks, AE_P2_LOCAL_SQUASH_SCALE);",
         "ae_p2_draw_squashed(r, hooks, AE_P2_LOCAL_SQUASH_SCALE + 4u);"),
        ("PlayerDrawSquashedWithFx", "include terminal flash frame",
         "AE_P2_PLAYER_SQUASH_FRAME) < 0x1E", "AE_P2_PLAYER_SQUASH_FRAME) <= 0x1E"),
        ("PlayerDrawSquashedWithFx", "wrong nested direction buffer",
         "AE_P2_NESTED_SQUASH_DIRECTION);", "AE_P2_G_PLAYER + AE_P2_PLAYER_FACING);"),
        ("PlayerDrawSquashedFlattened", "write over player height",
         "ae_wr32(r, player + AE_P2_PLAYER_VELOCITY_Y, 0);",
         "ae_wr32(r, player + AE_P2_PLAYER_POSITION_Y, 0);"),
        ("PlayerDrawSquashedFlattened", "advance frame twice",
         "ae_rd16(r, player + AE_P2_PLAYER_SQUASH_FRAME) + 1u);",
         "ae_rd16(r, player + AE_P2_PLAYER_SQUASH_FRAME) + 2u);"),
        ("PlayerUpdateIdleAnim", "wrong reflected animation frame",
         "frame = 32u - frame;", "frame = 31u - frame;"),
        ("PlayerUpdateIdleAnim", "ribbon frame off by one",
         "AE_P2_G_IDLE_RIBBON_END, frame + 0x18u,",
         "AE_P2_G_IDLE_RIBBON_END, frame + 0x19u,"),
        ("PlayerSelectGadget", "invert repeated selection return",
         "if (previous_gadget == gadget) return 0;",
         "if (previous_gadget != gadget) return 0;"),
        ("PlayerSelectGadget", "invert gadget-three repeat test",
         "if (ae_rds8(r, player + AE_P2_PLAYER_CURRENT_GADGET) == gadget)",
         "if (ae_rds8(r, player + AE_P2_PLAYER_CURRENT_GADGET) != gadget)"),
        ("PlayerUpdateClimb", "reverse target height test",
         "if (target_y < ae_rds32(r, player + AE_P2_PLAYER_POSITION_Y))",
         "if (target_y > ae_rds32(r, player + AE_P2_PLAYER_POSITION_Y))"),
        ("PlayerUpdateClimb", "treat zero climb speed as rising",
         "if (climb_speed > 0) {\n            ae_PlayerSetState(r, act, 0x3Bu);",
         "if (climb_speed >= 0) {\n            ae_PlayerSetState(r, act, 0x3Bu);"),
        ("PlayerUpdateRope", "add instead of invert rope fraction",
         "AE_P2_ROPE_FRACTION_SCALE - fraction",
         "AE_P2_ROPE_FRACTION_SCALE + fraction"),
        ("PlayerUpdateRope", "move rope gesture to the wrong state",
         "state_handler == 0x40u || state_handler == 0x41u",
         "state_handler == 0x3Fu || state_handler == 0x41u"),
        ("PlayerOnContact", "accept non-hazard contacts as hazards",
         "if (contact_type != 0xBu)", "if (contact_type == 0xBu)"),
        ("PlayerOnContact", "reverse ordinary knockback facing",
         "ae_p2_contact_ordinary_knockback(r, body_flags, normalized, 1);",
         "ae_p2_contact_ordinary_knockback(r, body_flags, normalized, 0);"),
        ("MonkeyNetCatchDraw", "shift local renderer matrix",
         "ae_p2_monkey_net_catch_draw(r, hooks, AE_P2_G_NET_FRAME_LOCAL_MATRIX);",
         "ae_p2_monkey_net_catch_draw(r, hooks, AE_P2_G_NET_FRAME_LOCAL_MATRIX + 4u);"),
        ("MonkeyNetCatchDraw", "omit renderer hooks",
         "ae_p2_monkey_net_catch_draw(r, hooks, AE_P2_G_NET_FRAME_LOCAL_MATRIX);",
         "ae_p2_monkey_net_catch_draw(r, hooks == NULL ? hooks : NULL, AE_P2_G_NET_FRAME_LOCAL_MATRIX);"),
        ("MonkeyNetCatchUpdate", "wrong rumble cadence",
         "if (frame % 5 == 0 && !ae_p2_monkey_net_rumble",
         "if (frame % 4 == 0 && !ae_p2_monkey_net_rumble"),
        ("MonkeyNetCatchUpdate", "wrong ribbon activation frame",
         "if (frame == 0x25)", "if (frame == 0x24)"),
    ]
    for name, label, before, after in specs:
        symbol = "ae_p2_effect_ground_impact" if name == "EffectGroundImpact" else f"ae_{name}"
        definition = re.search(rf"(?m)^(?:static\s+)?(?:void|int32_t)\s+{re.escape(symbol)}\(", original)
        if definition is None:
            raise AssertionError(f"mutation {name}: missing function definition {symbol}")
        start = definition.start()
        end = original.index("\n}\n", start) + 2
        body = original[start:end]
        if before not in body:
            raise AssertionError(f"mutation {name}: missing anchor {before!r}")
        yield name, label, original[:start] + body.replace(before, after, 1) + original[end:]


def run_mutations(seed):
    original = (ROOT / "src/player/ae_player2_leaf.c").read_text(encoding="utf-8")
    mutations = list(mutation_sources(original))
    caught, survivors = 0, []
    out = ROOT / "extract/mips"
    mips = mr.MipsRam.from_extract()
    mips.attach_gte_oracle()
    for index, (name, label, source) in enumerate(mutations):
        source_path = out / f"player2_mutation_{index}.c"
        source_path.write_text(source, encoding="utf-8")
        harness = Harness(source_path, target=name, mips=mips)
        harness.run_all(seed)
        if harness.failures:
            caught += 1
            print(f"caught {name}: {label}")
        else:
            survivors.append(f"{name}: {label}")
            print(f"SURVIVED {name}: {label}")
        harness.close()
        source_path.unlink(missing_ok=True)
    print(f"mutations caught: {caught}/{len(mutations)}")
    if survivors:
        print("\n".join("  " + item for item in survivors))
    return int(bool(survivors))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--function", choices=FUNCS)
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    args = parser.parse_args()
    if args.mutate:
        return run_mutations(args.seed)

    started = time.time()
    harness = Harness(target=args.function)
    harness.run_all(args.seed)
    for name, count in harness.counts.items():
        print(f"{name:38s} {count} cases", flush=True)
    summary = reg.coverage_summary([(name, address, size, True)
                                    for name, (address, size, _, _) in FUNCS.items()], harness.m.coverage)
    if args.coverage:
        reg.print_coverage(summary)
    gaps = any(row["uncovered_count"] or row["stale_declared"] for row in summary.values())
    for name, row in summary.items():
        if row["uncovered"]:
            print(f"COVERAGE GAP {name}: {row['uncovered']}")
    passed = not harness.failures and not gaps
    if args.seed == DEFAULT_SEED:
        reg.write_results(harness.counts, summary, passed, extra={"area_ids": list(AREA_IDS)},
                          function_names=list(FUNCS))
    print(f"total cases {sum(harness.counts.values())}; failures {len(harness.failures)}; "
          f"elapsed {time.time() - started:.1f}s")
    for failure in harness.failures[:30]:
        print("FAIL " + failure)
    harness.close()
    return int(not passed)


if __name__ == "__main__":
    sys.exit(main())
