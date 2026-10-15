"""Differential checks for the readable Area 02 overlay 18C ports.

The checker starts from the real area 02 member (094.bin), overlays member
18C.bin at 0x8013D000, and runs each gameplay routine on the original MIPS
image and on the AeRam C port. Native renderer routines are intercepted by
their named renderer hook on both sides.
"""
import argparse
import ctypes as C
import random
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

U32, I32, P = C.c_uint32, C.c_int32, C.c_void_p
STACK_LO, STACK_HI = 0x801FF000, 0x80200000
MUTATION_SELECTOR, MUTATION_MARKER = 0x801FEF00, 0x801FEF04
OVERLAY_BASE, OVERLAY_SIZE = 0x8013D000, 0x3000
OBJ, PLAYER = 0x800E6960, 0x800EC1E0
ANIM_TABLE = 0x801A0300
ZED_DATA = 0x801A0000
ENTRY_TABLE = 0x8019F000
BODY_RECORD = 0x801A0800
DISTANCE_TABLE = 0x801B0000
SURFACE_DATA = 0x801A1000
SURFACE_VOLUME = 0x801A2000
ENEMY_PATH_POINTS = 0x801A3000
RENDER_PACKET_BASE, RENDER_PACKET_SIZE = 0x801E0000, 0x10000

# name: (address, size, C symbol, return kind, argument kinds)
FUNCS = {
    "Area02_SpawnAmbientParticle": (0x8013D000, 0xB0, "ae_Area02_SpawnAmbientParticle", "v", "u"),
    "Area02_InterpolateAnimValue": (0x8013D0B0, 0x114, "ae_Area02_InterpolateAnimValue", "s", "uss"),
    "Area02_PlayerWithinHeightAndRadius": (0x8013D1C4, 0x60, "ae_Area02_PlayerWithinHeightAndRadius", "s", ""),
    "Area02_PlayerOutsideHeightAndRadius": (0x8013D224, 0x70, "ae_Area02_PlayerOutsideHeightAndRadius", "s", ""),
    "Area02_EnemyAnimationTriggerUpdate": (0x8013D294, 0x124, "ae_Area02_EnemyAnimationTriggerUpdate", "v", "u"),
    "Area02_UpdateEnemyWakeOrigin": (0x8013D3B8, 0x78, "ae_Area02_UpdateEnemyWakeOrigin", "v", "u"),
    "Area02_UpdateEnemyWake": (0x8013D430, 0xE8, "ae_Area02_UpdateEnemyWake", "v", "u"),
    "Area02_EnemyType0B_Update": (0x8013D518, 0x148, "ae_Area02_EnemyType0B_Update", "v", "u"),
    "Area02_CopyEnemyModel": (0x8013D660, 0xA0, "ae_Area02_CopyEnemyModel", "v", "uuu"),
    "Area02_InitializeEffects": (0x8013D7C8, 0x54, "ae_Area02_InitializeEffects", "v", "u"),
    "Area02_FindFreeEffectSlot": (0x8013D81C, 0x3C, "ae_Area02_FindFreeEffectSlot", "s", ""),
    "Area02_SpawnRadialEffect": (0x8013D858, 0x138, "ae_Area02_SpawnRadialEffect", "v", "uss"),
    "Area02_EffectBelowGround": (0x8013D990, 0x3C, "ae_Area02_EffectBelowGround", "s", "u"),
    "Area02_SpawnEffectParticle": (0x8013D9CC, 0x40, "ae_Area02_SpawnEffectParticle", "v", "u"),
    "Area02_SpawnEffectBurst": (0x8013DA0C, 0x88, "ae_Area02_SpawnEffectBurst", "v", "u"),
    "Area02_UpdateEffects": (0x8013DA94, 0x1DC, "ae_Area02_UpdateEffects", "v", ""),
    "Area02_SpawnEnemyHitParticle": (0x8013DC70, 0x2C, "ae_Area02_SpawnEnemyHitParticle", "v", "u"),
    "Area02_EnemyHitUpdate": (0x8013DC9C, 0x90, "ae_Area02_EnemyHitUpdate", "v", "u"),
    "Area02_EnemyEnterHitReaction": (0x8013DD2C, 0x90, "ae_Area02_EnemyEnterHitReaction", "v", "u"),
    "Area02_EnemySplashUpdate": (0x8013DDBC, 0x64, "ae_Area02_EnemySplashUpdate", "v", "u"),
    "Area02_EnemyEnterDefeat": (0x8013DE20, 0xD8, "ae_Area02_EnemyEnterDefeat", "v", "u"),
    "Area02_EnemyPhase6Update": (0x8013DEF8, 0x5C, "ae_Area02_EnemyPhase6Update", "s", "u"),
    "Area02_StageSpecificEnemyUpdate": (0x8013DF54, 0x28C, "ae_Area02_StageSpecificEnemyUpdate", "v", "u"),
    "Area02_SurfaceEffectsInit": (0x8013E1E0, 0x15C, "ae_Area02_SurfaceEffectsInit", "s", ""),
    "Area02_EffectDraw": (0x8013E33C, 0x614, "ae_Area02_EffectDraw", "s", ""),
    "Area02_SurfaceDraw": (0x8013E950, 0x770, "ae_Area02_SurfaceDraw", "s", ""),
}
NATIVE_BOUNDARIES = {
    "Area02_EnemyModelDraw": (0x8013D700, 0xC8),
}
NO_HOOKS = {
    "Area02_InterpolateAnimValue", "Area02_UpdateEnemyWakeOrigin",
    "Area02_CopyEnemyModel", "Area02_FindFreeEffectSlot",
}

ORIGINAL_HELPERS = {
    0x800570F4: ("PlayerWithinRadiusXZ", 3),
    0x8005588C: ("ObjCooldown", 2),
    0x8005654C: ("ObjPlayerNearPointXZ", 5),
    0x8005696C: ("ObjTurnToPlayer", 1),
    0x80057504: ("ObjSpawnParticleAt", 5),
    0x80058134: ("Zako_Nop", 2),
    0x8005813C: ("ObjAddHitBody", 7),
    0x800582EC: ("ObjGetLinkedObjState", 2),
    0x8004D8C0: ("GetSolidHeight", 4),
    0x8001484C: ("MatrixLookDir", 4),
}
REGRESSIONS = {
    'Area02_StageSpecificEnemyUpdate': (8, 9, 10, 11, 12),
    'Area02_SurfaceDraw': (8,),
    'Area02_EffectDraw': (8, 9),
}
RENDERERS = {
    0x8013D700: ("Area02_EnemyModelDraw", 2),
    0x8001F604: ("DrawShadow", 4),
}

OvlCallFn = C.CFUNCTYPE(I32, P, P, C.c_char_p, U32, U32, C.POINTER(U32))
OvlRenderFn = C.CFUNCTYPE(I32, P, P, C.c_char_p, U32, U32, C.POINTER(U32))


class OvlHooks(C.Structure):
    _fields_ = [
        ("user", P), ("zako_hooks", P), ("zako1_hooks", P),
        ("spawn_hooks", P), ("world_hooks", P),
        ("original_call", OvlCallFn), ("renderer", OvlRenderFn),
    ]


class CallModel:
    def __init__(self, profile):
        self.profile = profile
        self.events = []
        self.mem = None

    def call(self, name, args):
        words = tuple(int(x) & 0xFFFFFFFF for x in args)
        self.events.append((name, words, ()))
        if name == "Atan2Safe":
            return (words[0] + words[1]) & 0xFFF
        if name in ("Area02_SurfaceProjectPoint", "Area02_SurfaceProjectVertex"):
            screen_x = self.profile.get("projection_x", 224) & 0xFFFF
            screen_y = self.profile.get("projection_y", 120) & 0xFFFF
            packed = screen_x | (screen_y << 16)
            if self.mem is not None:
                self.mem.wr32(words[3], packed)
                if name == "Area02_SurfaceProjectPoint":
                    self.mem.wr32(words[4], self.profile.get("projection_depth", 512))
            return self.profile.get("projection_depth", 512)
        if name == "Area02_EffectDrawRecord":
            return (words[7] + 40) & 0xFFFFFFFF
        if name in ("Area02_SurfaceSubmitPrimary", "Area02_SurfaceSubmitSecondary"):
            return 0
        if name == "PlayerWithinRadiusXZ":
            return self.profile.get("radius", 0)
        if name == "ObjGetLinkedObjState":
            return self.profile.get("linked", 0)
        if name == "ObjPlayerNearPointXZ":
            return self.profile.get("near", 0)
        if name == "ObjCooldown":
            return self.profile.get("cooldown", 0)
        if name == "GetSolidHeight":
            return self.profile.get("ground", 0)
        if name == "ObjAddHitBody":
            return 0x801A0F00
        return 0


def build(source=None, tag="ovl18c"):
    import check_game
    return check_game.build_dll({'src/world/ae_ovl18c.c': source} if source else None, tag)


class MipsMem:
    def __init__(self, cpu): self.cpu = cpu
    def rd8(self, addr): return self.cpu.read8(addr)
    def rd16(self, addr): return self.cpu.read16(addr)
    def rd32(self, addr): return self.cpu.read32(addr)
    def wr8(self, addr, value): self.cpu.write8(addr, value)
    def wr16(self, addr, value): self.cpu.write16(addr, value)
    def wr32(self, addr, value): self.cpu.write32(addr, value)
    def write_bytes(self, addr, data):
        for offset, value in enumerate(data): self.cpu.write8(addr + offset, value)


def u32(value): return int(value) & 0xFFFFFFFF


def seed_surface_volume(mem):
    mem.wr32(0x800F455C, SURFACE_VOLUME)
    mem.wr16(0x800F45D4, 1)
    mem.wr16(SURFACE_VOLUME + 0x00, 0)
    mem.wr16(SURFACE_VOLUME + 0x02, 500)
    mem.wr16(SURFACE_VOLUME + 0x04, 0)
    mem.wr16(SURFACE_VOLUME + 0x06, 30000)
    mem.wr16(SURFACE_VOLUME + 0x08, 500)
    mem.wr16(SURFACE_VOLUME + 0x0A, 30000)
    mem.wr16(SURFACE_VOLUME + 0x12, 0x0109)


def setup_common(mem, rng, profile, name, scenario):
    # Real member and overlay bytes remain in the image; seed the runtime state
    # that the area loader and Zako initialization normally prepare.
    for i in range(16 * 0xE4): mem.wr8(OBJ + i, 0)
    for i in range(STACK_HI - STACK_LO): mem.wr8(STACK_LO + i, 0)
    for i in range(0x1000): mem.wr8(0x800B9900 + i, 0)
    for i in range(512):
        mem.wr16(0x800BF670 + i * 2, -1 if i == 0 else 0)
        mem.wr16(0x800B2D08 + i * 8, 0)

    # Single deterministic ZED entry and its minimal model/animation headers.
    mem.wr32(0x800E77D4, ENTRY_TABLE)
    mem.wr16(0x800E77DC, 1)
    mem.wr32(0x800F4594, ZED_DATA)
    mem.wr16(ENTRY_TABLE, 0x83)
    mem.wr32(ENTRY_TABLE + 0x18, 0x100)
    mem.wr32(ENTRY_TABLE + 0x1C, 0x200)
    mem.wr16(ZED_DATA + 0x100 + 4, 0)
    mem.wr16(ZED_DATA + 0x100 + 6, 0)
    mem.wr16(ZED_DATA + 0x100 + 8, 0)
    mem.wr16(ZED_DATA + 0x100 + 0xA, 0)
    mem.wr16(ZED_DATA + 0x200, 8)
    mem.wr32(ZED_DATA + 0x200 + 4, ANIM_TABLE - ZED_DATA)
    for clip in range(16):
        mem.wr16(ANIM_TABLE + clip * 4, clip * 10)
        mem.wr16(ANIM_TABLE + clip * 4 + 2, clip * 10 + 8)
    mem.wr32(OBJ + 0x88, ANIM_TABLE)
    mem.wr32(OBJ + 0x18, ANIM_TABLE)
    mem.wr8(OBJ + 0x7F, 0)
    mem.wr16(OBJ + 0x5C, 4)
    mem.wr8(OBJ + 0x81, 10)
    mem.wr8(OBJ + 0x83, 1)
    mem.wr16(OBJ + 0x24, 100)
    mem.wr16(OBJ + 0x28, 100)
    mem.wr16(OBJ + 0x2C, 100)
    mem.wr16(OBJ + 0x60, 0x1000)
    mem.wr16(OBJ + 0x64, 0)
    mem.wr16(OBJ + 0x66, 0)
    mem.wr16(OBJ + 0x70, 0)
    mem.wr16(OBJ + 0x8C, 0)
    mem.wr16(OBJ + 0x9A, 0)
    mem.wr16(OBJ + 0x9E, 0)
    mem.wr8(OBJ + 0xCB, 0)
    mem.wr8(OBJ + 0xCD, 1)
    mem.wr32(OBJ + 0x50, 0)
    mem.wr32(OBJ + 0xE0, 8)
    mem.wr16(PLAYER + 0x24, 150)
    mem.wr16(PLAYER + 0x2C, 150)
    mem.wr16(0x800FC1E0 + 0x24, 150)
    mem.wr16(0x800FC1E0 + 0x2C, 150)
    mem.wr16(0x800E77A8, 100)
    mem.wr16(0x800E77AA, 100)
    mem.wr16(0x800E77AC, 100)
    mem.wr32(0x800EC208, 100)
    mem.wr32(0x800F44E4, 0x801A0F80)
    mem.wr32(0x801A0F80, 2)
    mem.wr16(0x800EBFA0, 0)
    mem.wr16(0x800EBFA2, 0)
    mem.wr32(0x800EBFA8, 0x801A0FC0)
    mem.wr32(0x800EBFB4, 0x801A0FC4)
    mem.wr32(0x800F455C, DISTANCE_TABLE)
    for i in range(256):
        mem.wr16(DISTANCE_TABLE + i * 0x38 + 6, 0)
        mem.wr16(DISTANCE_TABLE + i * 0x38 + 10, 0)

    # These tables are used by the already-ported sound/effect routines. A
    # negative loaded-bank entry makes ObjPlaySfx follow its original silent path.
    mem.wr16(0x800BF670, -1)
    mem.wr32(0x800F459C, SURFACE_DATA)
    mem.wr32(MUTATION_SELECTOR, 0)
    mem.wr32(MUTATION_MARKER, 0)
    mem.wr16(0x8013F2A0 + 12, 0)
    for i in range(8):
        slot = 0x8013F2A0 + i * 0x14
        for off in range(0x14): mem.wr8(slot + off, 0)
    for i in range(8 * 0x20): mem.wr8(0x8013F3B0 + i, 0)
    if name in ("Area02_EffectDraw", "Area02_SurfaceDraw"):
        mem.wr32(0x800EC354, RENDER_PACKET_BASE)
        mem.wr32(0x800EC35C, 0)
    if name == "Area02_EffectDraw":
        mem.wr8(0x8013F7B0, 8)
        for i in range(8):
            record = 0x8013F3B0 + i * 0x20
            mem.wr16(record + 0x08, 100 + i * 20)
            mem.wr16(record + 0x0A, 200 + i * 10)
            mem.wr16(record + 0x0C, -300 + i * 30)
            mem.wr16(record + 0x18, 1 + (i % 3))
            mem.wr16(record + 0x1A, 4 + i)
            mem.wr8(record + 0x0E, 0 if i < 4 else 1)
            mem.wr8(record + 0x0F, (10, 34, 40, 2, 0, 1, 5, 8)[i])
            mem.wr8(record + 0x16, (0, 0, 0, 1, 2, 0, 1, 3)[i])
            mem.wr8(record + 0x1D, 0 if i != 3 else 3)
    if name == "Area02_SurfaceDraw":
        mem.write_bytes(0x8013F7B8, bytes(230 * 8))
        mem.wr16(0x8013FEE8, 0x0123)
        mem.wr16(0x8013FEEA, 0x0456)
        mem.wr32(0x800EC354, RENDER_PACKET_BASE)
    if name == "Area02_EnemyType0B_Update":
        # The direct path ports follow the object path, so give the caller
        # checker a valid path even in its baseline (scenario zero) case.
        mem.wr16(OBJ + 0x8C, 2)
        mem.wr8(OBJ + 0x8E, 0)
        mem.wr32(OBJ + 0x90, ENEMY_PATH_POINTS)
        mem.wr16(ENEMY_PATH_POINTS + 0, 120)
        mem.wr16(ENEMY_PATH_POINTS + 2, 100)
        mem.wr16(ENEMY_PATH_POINTS + 4, 120)
        mem.wr16(ENEMY_PATH_POINTS + 8, 160)
        mem.wr16(ENEMY_PATH_POINTS + 10, 100)
        mem.wr16(ENEMY_PATH_POINTS + 12, 120)
        mem.wr16(OBJ + 0xB0, 0x0400)

    args = {
        "Area02_SpawnAmbientParticle": (OBJ,),
        "Area02_InterpolateAnimValue": (OBJ, 20, 8),
        "Area02_PlayerWithinHeightAndRadius": (),
        "Area02_PlayerOutsideHeightAndRadius": (),
        "Area02_EnemyAnimationTriggerUpdate": (OBJ,),
        "Area02_UpdateEnemyWakeOrigin": (OBJ,),
        "Area02_UpdateEnemyWake": (OBJ,),
        "Area02_EnemyType0B_Update": (OBJ,),
        "Area02_CopyEnemyModel": (OBJ, 0x8013F340, 0x83),
        "Area02_InitializeEffects": (OBJ,),
        "Area02_FindFreeEffectSlot": (),
        "Area02_SpawnRadialEffect": (OBJ, 70, 160),
        "Area02_EffectBelowGround": (0x8013F2A0,),
        "Area02_SpawnEffectParticle": (0x801FF180,),
        "Area02_SpawnEffectBurst": (0x801FF180,),
        "Area02_UpdateEffects": (),
        "Area02_SpawnEnemyHitParticle": (OBJ,),
        "Area02_EnemyHitUpdate": (OBJ,),
        "Area02_EnemyEnterHitReaction": (OBJ,),
        "Area02_EnemySplashUpdate": (OBJ,),
        "Area02_EnemyEnterDefeat": (OBJ,),
        "Area02_EnemyPhase6Update": (OBJ,),
        "Area02_StageSpecificEnemyUpdate": (OBJ,),
        "Area02_SurfaceEffectsInit": (),
        "Area02_EffectDraw": (),
        "Area02_SurfaceDraw": (),
    }

    if scenario:
        if name == "Area02_SpawnAmbientParticle":
            mem.wr16(0x800AE8F0, rng.choice([0, 0xFF, 0x1234, rng.getrandbits(16)]))
        elif name == "Area02_InterpolateAnimValue":
            mem.wr16(ANIM_TABLE, 10)
            mem.wr16(ANIM_TABLE + 2, 18)
            case = scenario % 5
            mem.wr16(OBJ + 0x5C, [4, 3, 8, 20, 0xFFFF][case])
            if scenario == 6:
                mem.wr16(ANIM_TABLE, 10)
                mem.wr16(ANIM_TABLE + 2, 9)
                mem.wr16(OBJ + 0x5C, 3)
            if scenario == 7:
                mem.wr16(ANIM_TABLE + 2, 10)
                mem.wr16(OBJ + 0x5C, 10)
            args[name] = (OBJ, rng.choice([-20, 0, 20, 200]), rng.choice([-15, 8, 40, -200]))
        elif name in ("Area02_PlayerWithinHeightAndRadius", "Area02_PlayerOutsideHeightAndRadius"):
            mem.wr16(0x800E77AA, 100 + rng.choice([0, 49, 50, 51, -49, -50]))
        elif name == "Area02_EnemyAnimationTriggerUpdate":
            case = scenario % 5
            mem.wr32(OBJ + 0xE0, [0, 0, 1, 1, 1][case])
            mem.wr8(OBJ + 0x7F, [0, 0, 1, 1, 2][case])
            mem.wr16(OBJ + 0x5C, [0, 10, 25, 18, 20][case])
            profile["radius"] = 1 if case in (1, 2) else 0
        elif name == "Area02_UpdateEnemyWake":
            mem.wr8(OBJ + 0x8F, 1 if scenario == 2 else 0)
            profile["linked"] = 6 if scenario == 1 else 0
        elif name == "Area02_EnemyType0B_Update":
            mem.wr32(OBJ + 0xE0, 0 if scenario == 1 else 8)
            mem.wr16(ANIM_TABLE + 2, 30)
        elif name == "Area02_FindFreeEffectSlot":
            for i in range(8): mem.wr16(0x8013F2A0 + i * 0x14 + 12, 1)
        elif name == "Area02_SpawnRadialEffect":
            mem.wr16(OBJ + 0xAC, rng.choice([0, 512, 1024, 2048, -1024]))
            mem.wr8(OBJ + 0xCB, rng.randrange(8))
            for i in range(8): mem.wr16(0x8013F2A0 + i * 0x14 + 12, 0 if i == 0 else 1)
        elif name == "Area02_UpdateEffects":
            mode = scenario % 3
            profile["linked"] = 6 if mode == 1 else 0
            profile["ground"] = 200 if mode == 2 else 0
            for i in range(8):
                slot = 0x8013F2A0 + i * 0x14
                mem.wr16(slot + 12, 1)
                mem.wr16(slot + 14, 0)
                mem.wr16(slot + 0, 100 + i)
                mem.wr16(slot + 2, 100 + i)
                mem.wr16(slot + 4, 100 + i)
                mem.wr16(slot + 6, 2)
                mem.wr16(slot + 8, -2)
                mem.wr16(slot + 10, 3)
        elif name == "Area02_EnemyHitUpdate":
            mem.wr32(OBJ + 0x50, 0xB000000B)
            mem.wr32(OBJ + 0xE0, 2 if scenario != 4 else 0)
        elif name == "Area02_EnemySplashUpdate":
            mem.wr8(OBJ + 0x8F, rng.choice([0, 1, 2, 254, 255]))
        elif name == "Area02_EnemyPhase6Update":
            mem.wr32(OBJ + 0x50, rng.choice([0, 0x60000000]))
        elif name == "Area02_StageSpecificEnemyUpdate":
            states = [5, 1, 3, 4, 6, 4, 3, 2]
            state = states[scenario]
            mem.wr32(OBJ + 0xE0, 0 if scenario == 1 else 8)
            mem.wr8(OBJ + 0x80, state)
            mem.wr8(OBJ + 0x7F, 0 if scenario == 2 else 1)
            mem.wr16(OBJ + 0x5C, [0, 0, 20, 30, 100, 30, 20, 0][scenario])
            mem.wr8(OBJ + 0x8F, 2 if scenario == 3 else 0)
        elif name == "Area02_SurfaceEffectsInit":
            mem.wr16(SURFACE_DATA, 0x1234 if scenario == 4 else 0x452E)
            mem.wr8(SURFACE_DATA + 2, 70)
            mem.wr8(SURFACE_DATA + 4, [1, 40, 4, 32, 1][scenario % 5])
            for i in range(40):
                record = SURFACE_DATA + 8 + i * 0x10
                for off in (2, 4, 6): mem.wr16(record + off, rng.choice([-500, 0, 100, 500]))
                mem.wr16(record + 10, rng.getrandbits(16))
        elif name == "Area02_EffectDraw":
            for i in range(8):
                record = 0x8013F3B0 + i * 0x20
                mem.wr8(record + 0x0E, (0, 1, 0, 1, 0, 1, 0, 1)[(i + scenario) % 8])
                mem.wr16(record + 0x18, (0, 1, 2, 3, 4, 5, 6, 7)[(i + scenario) % 8])
                mem.wr8(record + 0x0F, (0, 10, 33, 34, 35, 96, 2, 8)[(i + scenario) % 8])
                mem.wr8(record + 0x16, (0, 1, 2, 0, 1, 2, 0, 1)[(i + scenario) % 8])
            record = 0x8013F3B0
            mem.wr8(record + 0x0E, 1)
            mem.wr8(record + 0x0F, 10)
            mem.wr8(record + 0x16, 0)
            record += 0x20
            mem.wr8(record + 0x0E, 1)
            mem.wr8(record + 0x0F, 34)
            mem.wr8(record + 0x16, 0)
            mem.wr16(record + 0x12, 82)
        elif name == "Area02_SurfaceDraw":
            projections = ((224, 120, 512), (577, 120, 512), (224, -32, 512),
                           (224, 273, 512), (224, 120, 0), (224, 120, 2048),
                           (576, 272, 1), (0, 0, 1))
            px, py, depth = projections[scenario % len(projections)]
            profile.update(projection_x=px, projection_y=py, projection_depth=depth)
            if scenario == 1:
                mem.wr32(0x800EC250, 0x40)
            elif scenario == 2:
                mem.wr32(0x800EC250, 0x40)
                seed_surface_volume(mem)
            elif scenario == 4:
                mem.wr8(0x800E6908, 1)
                seed_surface_volume(mem)
            elif scenario == 5:
                mem.wr8(0x800E6908, 1)
                mem.wr16(0x800F45D4, 0)
            if scenario:
                rows = ((1, 0, -4000, 0, 12), (4, 1023, -1023, 1023, 32),
                        (5, 1024, 1024, -1024, 64), (2, -1023, 700, 600, 96))
                for index, (state, u, v, w, intensity) in enumerate(rows):
                    record = 0x8013F7B8 + index * 8
                    mem.wr16(record, u)
                    mem.wr16(record + 2, v)
                    mem.wr16(record + 4, w)
                    mem.wr8(record + 6, state)
                    mem.wr8(record + 7, intensity)
    else:
        if name == "Area02_SpawnAmbientParticle": mem.wr16(0x800AE8F0, 0)
        if name in ("Area02_PlayerWithinHeightAndRadius", "Area02_PlayerOutsideHeightAndRadius"):
            profile["radius"] = 1
        if name == "Area02_UpdateEffects":
            profile["ground"] = 200
            for i in range(8):
                slot = 0x8013F2A0 + i * 0x14
                mem.wr16(slot + 12, 0 if i else 1)
                mem.wr16(slot + 14, 0)
                mem.wr16(slot + 0, 100); mem.wr16(slot + 2, 100); mem.wr16(slot + 4, 100)
                mem.wr16(slot + 6, 1); mem.wr16(slot + 8, 0); mem.wr16(slot + 10, 1)
        if name == "Area02_SurfaceEffectsInit":
            mem.wr16(SURFACE_DATA, 0x452E); mem.wr8(SURFACE_DATA + 2, 70); mem.wr8(SURFACE_DATA + 4, 1)
            for off, value in ((2, 0), (4, 100), (6, 0), (10, 7)):
                mem.wr16(SURFACE_DATA + 8 + off, value)
        if name == "Area02_StageSpecificEnemyUpdate":
            mem.wr32(OBJ + 0xE0, 8)
            mem.wr8(OBJ + 0x80, 5)
            mem.wr8(OBJ + 0x7F, 1)
        if name == "Area02_SurfaceDraw":
            profile.update(projection_x=224, projection_y=120, projection_depth=512)
            record = 0x8013F7B8
            mem.wr16(record, 0)
            mem.wr16(record + 2, -4000)
            mem.wr16(record + 4, 0)
            mem.wr8(record + 6, 1)
            mem.wr8(record + 7, 12)
    profile.setdefault("ground", 0)
    profile.setdefault("radius", 0)
    profile.setdefault("linked", 0)
    profile.setdefault("near", 0)
    profile.setdefault("cooldown", 0)
    return args[name]


def argtypes_for(args):
    return [I32 if kind == "s" else U32 for kind in args]


class Harness:
    def __init__(self, dll):
        reg.declare("stack_window", "801FF000..80200000")
        members = dump_areas.area_members()
        area_path = ROOT / f"extract/members/{members[2]:03X}.bin"
        overlay = (ROOT / "extract/members/18C.bin").read_bytes()
        self.cpu = mr.MipsRam.from_extract(area_path)
        self.cpu.load_area(overlay, OVERLAY_BASE)
        self.cpu.attach_gte_oracle()
        self.cpu.coverage = set()
        self.base = self.cpu.snapshot()
        self.c = z1.CSide(dll)
        self.dll = self.c.dll
        self.dll.ae_ram_failed.argtypes, self.dll.ae_ram_failed.restype = [P], C.c_int
        self.dll.ae_ram_error.argtypes, self.dll.ae_ram_error.restype = [P], C.c_char_p
        self.ovl_funcs = {}
        for name, (_address, _size, symbol, result, args) in FUNCS.items():
            fn = getattr(self.dll, symbol)
            fn.argtypes = [P] + ([] if name in NO_HOOKS else [C.POINTER(OvlHooks)]) + argtypes_for(args)
            fn.restype = {"v": None, "s": I32, "u": U32}[result]
            self.ovl_funcs[name] = fn
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0
        self.seed = 0x18C02

    def _callbacks(self, model, cside):
        cside.set_model(model)
        # ObjFacePathPoint and the other direct area-02 ports use the verified
        # Atan2Safe port. Mirror that call from the existing Zako helper table.
        atan2 = cside.dll.ae_Atan2Safe
        atan2.argtypes = [P, I32, I32]
        atan2.restype = I32
        def atan2_callback(_user, ram, x, z, _a2, _a3):
            return atan2(ram, z1.s32(x), z1.s32(z))
        atan2_hook = z1.HOOKFN(atan2_callback)
        cside.hooks.Atan2Safe = atan2_hook
        cside.callbacks.append(atan2_hook)
        # The real particle port asks for caller-owned fourth-halfword values.
        cside.position_padding_value = 0
        for name in ("PlaySfx", "MatrixLookDir", "MatrixFromDirXZ"):
            callback = z1._make_hook(name, model)
            setattr(cside.spawn_hooks, name, callback)
            cside.callbacks.append(callback)
        def original_call(_u, ram, name, _address, count, words):
            model.mem = z1.CMem(cside.dll, ram)
            return model.call(name.decode("ascii"), [words[i] for i in range(count)])
        def render_call(_u, ram, name, _address, count, words):
            model.mem = z1.CMem(cside.dll, ram)
            return model.call(name.decode("ascii"), [words[i] for i in range(count)])
        call_cb = OvlCallFn(original_call)
        render_cb = OvlRenderFn(render_call)
        hooks = OvlHooks()
        hooks.zako_hooks = C.cast(C.pointer(cside.init_hooks), P)
        hooks.zako1_hooks = C.cast(C.pointer(cside.hooks), P)
        hooks.spawn_hooks = C.cast(C.pointer(cside.spawn_hooks), P)
        hooks.world_hooks = C.cast(C.pointer(cside.world_hooks), P)
        hooks.original_call = call_cb
        hooks.renderer = render_cb
        cside.callbacks.extend([call_cb, render_cb])
        return hooks

    def _mips_hooks(self, model, cpu):
        hooks = {}
        for address, (name, count) in ORIGINAL_HELPERS.items():
            def callback(cpu_, regs, label=name, arity=count, model_=model):
                args = list(regs[4:8])
                if arity > 4:
                    args.extend(cpu_.read32(regs[29] + 16 + 4 * i) for i in range(arity - 4))
                return model_.call(label, args[:arity])
            hooks[address] = callback
        for address, (name, count) in RENDERERS.items():
            def renderer(cpu_, regs, label=name, arity=count, model_=model):
                args = list(regs[4:8])[:arity]
                return model_.call(label, args)
            hooks[address] = renderer
        for name, _dispatch, address in z1.HOOK_NAMES:
            if address is None: continue
            if name == "Atan2Safe":
                continue
            hooks[address] = lambda cpu_, regs, label=name, model_=model: model_.call(label, regs[4:8])
        return hooks

    def _mips_patches(self, name, model, cpu):
        if name == "Area02_EffectDraw":
            def draw_effect_records(cpu_, regs):
                cursor = cpu_.read32(0x800EC354)
                count = cpu_.read8(0x8013F7B0)
                for index in range(count):
                    record = 0x8013F3B0 + index * 0x20
                    if cpu_.read8(record + 0x0E) == 0:
                        continue
                    args = (record, mr.signed(cpu_.read16(record), 16),
                            mr.signed(cpu_.read16(record + 2), 16),
                            mr.signed(cpu_.read16(record + 4), 16), cpu_.read8(record + 0x0F),
                            cpu_.read8(record + 0x16), cpu_.read16(record + 0x18), cursor)
                    cursor = model.call("Area02_EffectDrawRecord", args)
                regs[11] = cursor & 0xFFFFFFFF
                return 0x8013E928
            return {0x8013E590: draw_effect_records}

        if name != "Area02_SurfaceDraw":
            return {}

        def project_point(cpu_, regs):
            scratch = regs[17]
            point = scratch + 32
            args = (mr.signed(cpu_.read16(point), 16),
                    mr.signed(cpu_.read16(point + 2), 16),
                    mr.signed(cpu_.read16(point + 4), 16),
                    scratch + 20, scratch + 16, 0x800E78C0)
            model.call("Area02_SurfaceProjectPoint", args)
            return 0x8013ED88

        def project_vertex(cpu_, regs):
            scratch = regs[17]
            point = scratch + 32
            args = (mr.signed(cpu_.read16(point), 16),
                    mr.signed(cpu_.read16(point + 2), 16),
                    mr.signed(cpu_.read16(point + 4), 16),
                    scratch + 24, 0, 0x800E78C0)
            model.call("Area02_SurfaceProjectVertex", args)
            return 0x8013EE2C

        def submit_primary(cpu_, regs):
            record = regs[7] - 6
            scratch = regs[17]
            args = (record, mr.signed(cpu_.read16(record), 16),
                    mr.signed(cpu_.read16(record + 2), 16),
                    mr.signed(cpu_.read16(record + 4), 16),
                    cpu_.read32(scratch + 16), cpu_.read32(scratch + 20),
                    cpu_.read32(scratch + 24), regs[9])
            model.call("Area02_SurfaceSubmitPrimary", args)
            regs[8] = (regs[8] + 24) & 0xFFFFFFFF
            regs[9] = (regs[9] + 24) & 0xFFFFFFFF
            return 0x8013F060

        def submit_secondary(cpu_, regs):
            record = regs[7] - 6
            scratch = regs[17]
            args = (record, mr.signed(cpu_.read16(record), 16),
                    mr.signed(cpu_.read16(record + 2), 16),
                    mr.signed(cpu_.read16(record + 4), 16),
                    cpu_.read32(scratch + 20), cpu_.read32(scratch + 16),
                    regs[4], regs[5], regs[9], cpu_.read8(regs[7]),
                    cpu_.read8(record + 7))
            model.call("Area02_SurfaceSubmitSecondary", args)
            regs[8] = (regs[8] + 40) & 0xFFFFFFFF
            regs[9] = (regs[9] + 40) & 0xFFFFFFFF
            return 0x8013F060

        return {
            0x8013ED58: project_point,
            0x8013EE10: project_vertex,
            0x8013EE2C: submit_primary,
            0x8013EF34: submit_secondary,
        }

    def run(self, name, scenario=0, selector=0, mutant=False):
        seed = self.seed + 0x9E37 * (len(self.counts) + sum(self.counts.values()) + scenario + selector)
        rng = random.Random(seed)
        profile = {"ground": 0, "radius": 0, "linked": 0, "near": 0, "cooldown": 0}
        self.cpu.restore(self.base)
        self.cpu.coverage = self.cpu.coverage
        mem = MipsMem(self.cpu)
        args = setup_common(mem, rng, profile, name, scenario % 8)
        if scenario >= 8:
            if name == 'Area02_StageSpecificEnemyUpdate':
                mem.wr32(OBJ + 0xE0, 3 if scenario == 8 else 8)
                mem.wr8(OBJ + 0x80, 5 if scenario == 8 else (1 if scenario < 11 else 4))
                mem.wr8(OBJ + 0xC8, 0x44)
                mem.wr8(OBJ + 0x8F, 3)
                mem.wr16(ANIM_TABLE + 4, 10); mem.wr16(ANIM_TABLE + 6, 100)
                mem.wr8(OBJ + 0x7F, 1)
                mem.wr16(OBJ + 0x5C, 30 if scenario == 11 else 15)
                profile['near'] = 1 if scenario == 9 else 0
                profile['cooldown'] = 1 if scenario == 10 else 0
                if scenario == 9: mem.wr32(OBJ + 0x28, -90000)
            elif name == 'Area02_SurfaceDraw':
                for i, value in enumerate((200, -2000, 100)):
                    mem.wr16(0x800F45A8 + i*2, value)
                for i, value in enumerate((600, -1000, 900)):
                    mem.wr16(0x800F45B0 + i*2, value)
            elif name == 'Area02_EffectDraw':
                record = 0x8013F3B0
                mem.wr8(0x8013F7B0, 1)
                mem.wr8(record + 0x0E, 1); mem.wr8(record + 0x16, 0)
                mem.wr8(record + 0x0F, 34 if scenario == 8 else 35)
                mem.wr16(record + 0x12, 0)
                mem.wr16(record + 2, -2000); mem.wr16(record + 6, 100)
        mem.wr32(MUTATION_SELECTOR, selector)
        mem.wr32(MUTATION_MARKER, 0)
        before = self.cpu.snapshot()

        m_model = CallModel(profile)
        m_model.mem = MipsMem(self.cpu)
        c_model = CallModel(profile)
        c_model.mem = None
        c_hooks = self._callbacks(c_model, self.c)
        if name == "Area02_EnemyHitUpdate":
            # ObjPuffAt's eight-byte position copy includes the unwritten
            # fourth halfword left by the original nested call frames. The
            # direct and nested ObjPuffAt frames leave different stack words.
            pad_values = (0, 0x8000)
            pad_index = [0]
            def next_padding(_user, _ram, _function):
                index = pad_index[0]
                pad_index[0] += 1
                return pad_values[min(index, len(pad_values) - 1)]
            pad_callback = z1.PADFN(next_padding)
            self.c.spawn_hooks.position_padding = pad_callback
            self.c.callbacks.append(pad_callback)
            reg.declare("shared_random", "Area02_EnemyHitUpdate ObjPuffAt stack position padding")
        self.c.sync(before)
        mips_hooks = self._mips_hooks(m_model, self.cpu)
        address, _size, _symbol, ret_kind, arg_kinds = FUNCS[name]
        expected_trap = False
        try:
            original = self.cpu.call(address, args, hooks=mips_hooks,
                                     patches=self._mips_patches(name, m_model, self.cpu))
        except Exception as exc:
            expected_trap = (name == "Area02_InterpolateAnimValue" and scenario == 7 and
                             str(exc) == "syscall/break at 8013D0F4")
            if not expected_trap:
                self.fail(name, scenario, f"MIPS execution failed: {exc}")
                return False
        call_args = [self.c.ram]
        if name not in NO_HOOKS:
            call_args.append(C.byref(c_hooks))
        call_args.extend(args)
        actual = self.ovl_funcs[name](*call_args)
        self.counts[name] += 1
        if not expected_trap and ret_kind != "v" and (int(actual) & 0xFFFFFFFF) != original[2]:
            self.fail(name, scenario,
                      f"return C={int(actual) & 0xFFFFFFFF:08X} MIPS={original[2]:08X}")
            return False

        expected = bytearray(self.cpu.ram[:STACK_LO - 0x80000000])
        got = bytearray(self.c.main[:STACK_LO - 0x80000000])
        if got != expected:
            offset = next(i for i, (a, b) in enumerate(zip(got, expected)) if a != b)
            self.fail(name, scenario,
                      f"RAM differs at {0x80000000 + offset:08X}: C={got[offset:offset+8].hex()} "
                      f"MIPS={expected[offset:offset+8].hex()}")
            return False
        if bytes(self.c.scratch) != bytes(self.cpu.scratch):
            self.fail(name, scenario, "scratchpad differs")
            return False
        normalize = z1.hook_signature
        m_events = [normalize(event) for event in m_model.events]
        c_events = [normalize(event) for event in c_model.events]
        if c_events != m_events:
            self.fail(name, scenario, f"hook order differs C={c_events!r} MIPS={m_events!r}")
            return False
        c_failed = bool(self.dll.ae_ram_failed(self.c.ram))
        if expected_trap:
            error = self.dll.ae_ram_error(self.c.ram).decode(errors="replace")
            if not c_failed or "signed division trap" not in error:
                self.fail(name, scenario, f"MIPS trapped at 8013D0F4; C error={error!r}")
                return False
        elif c_failed:
            self.fail(name, scenario, self.dll.ae_ram_error(self.c.ram).decode(errors="replace"))
            return False
        return True

    def fail(self, name, scenario, message):
        self.failures += 1
        print(f"FAIL {name}[scenario={scenario}]: {message}", flush=True)

    def all(self, cases_per_function=8):
        self.cpu.coverage.clear()
        passed = True
        for index, name in enumerate(FUNCS):
            self.counts[name] = 0
            for scenario in range(cases_per_function):
                passed = self.run(name, scenario) and passed
            for scenario in REGRESSIONS.get(name, ()):
                passed = self.run(name, scenario) and passed
            print(f"{name}: {self.counts[name]} cases", flush=True)
        return passed

    def finish(self, default_run=False, show_coverage=False):
        items = [(name, data[0], data[1], True) for name, data in FUNCS.items()]
        items.extend((name, address, size, True)
                     for name, (address, size) in NATIVE_BOUNDARIES.items())
        summary = reg.coverage_summary(items, self.cpu.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        gaps = {name: row for name, row in summary.items()
                if row["uncovered_count"] or row["stale_declared"]}
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps,
                              extra={"overlay": "18C.bin", "areas": [2]},
                              function_names=list(FUNCS))
        if gaps:
            for name, row in gaps.items():
                print(f"COVERAGE GAP {name}: {row['uncovered']}; stale={row['stale_declared']}", flush=True)
            return False
        return not self.failures


def old_source_regressions():
    """Require each recovered defect to fail its focused original comparison."""
    source = (ROOT/'src/world/ae_ovl18c.c').read_text()
    normalize = ('    (void)ae_sdk_VectorNormalSS(r, scratch + AE_OVL18C_SURFACE_SCRATCH_POINT_X,\n'
                 '                               scratch + AE_OVL18C_SURFACE_SCRATCH_POINT_X);\n')
    variants = [
        ('initial flag bit', ' & ~1u);', ' & ~2u);', 'Area02_StageSpecificEnemyUpdate', 8),
        ('idle render clear', '        ae_wr8(r, obj + AE_OVL18C_OBJ_RENDER_MODE, 0);\n        int32_t ground_y',
         '        int32_t ground_y', 'Area02_StageSpecificEnemyUpdate', 10),
        ('near-player branch', '        if (ovl_call(r, h, "ObjPlayerNearPointXZ", 0x8005654Cu, 5, near_args) != 0)\n            break;',
         '        (void)ovl_call(r, h, "ObjPlayerNearPointXZ", 0x8005654Cu, 5, near_args);', 'Area02_StageSpecificEnemyUpdate', 9),
        ('near-player full-word Y', '            ae_rd32(r, obj + AE_OBJ_POS_Y),',
         '            (uint32_t)ae_rds16(r, obj + AE_OBJ_POS_Y),', 'Area02_StageSpecificEnemyUpdate', 9),
        ('phase-four animation gate', '        if (ae_Area02_EnemyPhase6Update(r, h, obj) == 0) {',
         '        if (ae_Area02_EnemyPhase6Update(r, h, obj) == 0 && ovl_animation_finished(r, obj)) {', 'Area02_StageSpecificEnemyUpdate', 12),
        ('surface normalization', normalize, '', 'Area02_SurfaceDraw', 8),
        ('frame-34 texture step', 'if (frame == 10u || frame == 34u)', 'if (frame == 10u)', 'Area02_EffectDraw', 8),
        ('unclamped moving Y', '                ae_wr16(r, record + AE_OVL18C_SURFACE_POS_Y, (uint32_t)moving_y);\n',
         '', 'Area02_EffectDraw', 9),
    ]
    directory=ROOT/'extract/mips/ovl18c_old_sources';directory.mkdir(parents=True,exist_ok=True)
    rejected=0
    for index,(label,old,new,name,scenario) in enumerate(variants):
        assert old in source, label
        path=directory/f'old_{index}.c';path.write_text(source.replace(old,new,1))
        h=Harness(build(path, f'ovl18c_old_{index}'))
        if h.run(name, scenario): raise AssertionError(f'old source survived: {label}')
        rejected += 1
        print(f'Old source rejected: {label}', flush=True)
    return rejected


def instrument_mutations(source):
    import re
    mutated = source
    for function_index, (_name, (_address, _size, symbol, _ret, _args)) in enumerate(FUNCS.items(), 1):
        pattern = re.compile(r"(\b(?:void|int32_t|uint32_t)\s+" + re.escape(symbol) +
                             r"\s*\([^)]*\)\s*\{)", re.S)
        mutations = "\n".join(
            f"    if (ae_rd32(r, 0x{MUTATION_SELECTOR:08X}u) == {function_index * 2 - variant}u) "
            f"ae_wr32(r, 0x{MUTATION_MARKER:08X}u, 0x6B180000u | {function_index * 2 - variant}u);"
            for variant in (1, 2))
        mutated, count = pattern.subn(r"\1\n" + mutations, mutated, count=1)
        if count != 1:
            raise SystemExit(f"could not instrument {symbol} for mutations")
    return mutated


def run_mutations():
    source_path = ROOT / "src/world/ae_ovl18c.c"
    mutant_source = source_path.with_name("ae_ovl18c_mutant.c")
    mutant_source.write_text(instrument_mutations(source_path.read_text(encoding="utf-8")), encoding="utf-8")
    try:
        dll = build(mutant_source, "ovl18c_mutant")
        h = Harness(dll)
        caught = 0
        for i, name in enumerate(FUNCS, 1):
            for variant in (1, 2):
                selector = i * 2 - variant
                before = h.failures
                ok = h.run(name, scenario=variant - 1, selector=selector, mutant=True)
                if not ok and h.failures > before:
                    caught += 1
                else:
                    print(f"MUTATION SURVIVED {name} selector={selector}", flush=True)
        print(f"mutations caught: {caught}/{2 * len(FUNCS)}", flush=True)
        return 0 if caught == 2 * len(FUNCS) else 1
    finally:
        mutant_source.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--cases", type=int, default=8)
    args = parser.parse_args()
    if args.mutate:
        return run_mutations()
    dll = build()
    harness = Harness(dll)
    passed = harness.all(args.cases)
    coverage_ok = harness.finish(default_run=args.cases == 8, show_coverage=args.coverage)
    print(f"differential: {sum(harness.counts.values())} cases; "
          f"{'PASS' if passed and coverage_ok else 'FAIL'}", flush=True)
    return 0 if passed and coverage_ok else 1


if __name__ == "__main__":
    sys.exit(main())
