"""Differential checks for the Area 0x57 overlay member 1A8 ports."""
from __future__ import annotations

import argparse
import ctypes as C
import random
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import check_game  # noqa: E402
import check_camera_ram as camera_check  # noqa: E402
import check_player1 as player1_check  # noqa: E402
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402
import check_zako2 as z2  # noqa: E402

OVERLAY = ROOT / "extract/members/1A8.bin"
AREA = ROOT / "extract/members/126.bin"
OVERLAY_BASE = 0x8013D000
STACK_LO, STACK_HI = 0x801FF000, 0x80200000
MUTATION_SELECTOR, MUTATION_MARKER = 0x801FEF00, 0x801FEF04
ACTORS = 0x80142338
ANIMATIONS = 0x801A0000
POSITION = 0x801FF180
PARTICLE_POSITION_PADDING = [
    (0x800B9900 - 0x80000000 + index * 0x34 + 0x1B, 1)
    for index in range(100)
]
EFFECT_POSITION_PADDING = [
    (0x800BAF00 - 0x80000000 + index * 0x54 + 6, 2)
    for index in range(16)
]
TRAIL_POSITION_PADDING = [
    (0x800F5010 - 0x80000000 + index * 0x10 + 6, 2)
    for index in range(8)
]

# name: (original address, byte length, C symbol, return type, C argument types)
FUNCS = {
    "Area57_Boss_ModelInit": (0x8013D000, 0xA0, "ae_ovl1a8_BossModelInit", "v", "uuu"),
    "Area57_Boss_SpawnEffectNear": (0x8013D1FC, 0x114, "ae_ovl1a8_BossSpawnEffectNear", "v", "uuuuu"),
    "Area57_Boss_SpawnEffectNearWrap": (0x8013D310, 0x2C, "ae_ovl1a8_BossSpawnEffectNearWrap", "v", "uuus"),
    "Area57_Boss_SpawnDebris": (0x8013D364, 0x168, "ae_ovl1a8_BossSpawnDebris", "v", "u"),
    "Area57_Boss_UpdateDebris": (0x8013D4CC, 0x3D0, "ae_ovl1a8_BossUpdateDebris", "v", ""),
    "Area57_Boss_ClearDebris": (0x8013D33C, 0x28, "ae_ovl1a8_BossClearDebris", "v", ""),
    "Area57_Boss_Init": (0x8013DA10, 0x184, "ae_ovl1a8_BossInit", "v", ""),
    "Area57_Boss_Start": (0x80140E70, 0x98, "ae_ovl1a8_BossStart", "v", ""),
    "Area57_Boss_UpdateCameraSequence": (0x8013E8A8, 0x17C, "ae_ovl1a8_BossUpdateCameraSequence", "v", "u"),
    "Area57_Boss_Update": (0x8013E4B4, 0x3F4, "ae_ovl1a8_BossUpdate", "s", "u"),
    "Area57_Boss_UpdateWakeSequence": (0x8013EA9C, 0x400, "ae_ovl1a8_BossUpdateWakeSequence", "v", "u"),
    "Area57_Boss_Wake": (0x8013EA24, 0x78, "ae_ovl1a8_BossWake", "v", ""),
    "Area57_Boss_RumbleLong": (0x8013D89C, 0x28, "ae_ovl1a8_BossRumbleLong", "v", ""),
    "Area57_Boss_RumbleShort": (0x8013D8C4, 0x28, "ae_ovl1a8_BossRumbleShort", "v", ""),
    "Area57_Boss_RumbleMedium": (0x8013D8EC, 0x28, "ae_ovl1a8_BossRumbleMedium", "v", ""),
    "Area57_Boss_SpawnDropPickups": (0x8013D914, 0x94, "ae_ovl1a8_BossSpawnDropPickups", "v", "u"),
    "Area57_Boss_ResetLoopHandle": (0x8013D9A8, 0x10, "ae_ovl1a8_BossResetLoopHandle", "v", ""),
    "Area57_Boss_LoopStart": (0x8013D9B8, 0x28, "ae_ovl1a8_BossLoopStart", "v", "s"),
    "Area57_Boss_LoopStop": (0x8013D9E0, 0x30, "ae_ovl1a8_BossLoopStop", "v", "s"),
    "Area57_Boss_ClearEntries": (0x8013E020, 0x24, "ae_ovl1a8_BossClearEntries", "v", ""),
    "Area57_Boss_AddEntry": (0x8013E044, 0xDC, "ae_ovl1a8_BossAddEntry", "v", "u"),
    "Area57_Boss_UpdateEntries": (0x8013E120, 0x394, "ae_ovl1a8_BossUpdateEntries", "v", ""),
    "Area57_Boss_SpawnEffectGroupsN": (0x8013DFB0, 0x70, "ae_ovl1a8_BossSpawnEffectGroupsN", "v", "u"),
    "Area57_Boss_AddSparks": (0x8013EEC0, 0x1AC, "ae_ovl1a8_BossAddSparks", "v", "uu"),
    "Area57_Boss_UpdateSparks": (0x8013F06C, 0x304, "ae_ovl1a8_BossUpdateSparks", "v", "u"),
    "Area57_Boss_ClearSparks": (0x8013EE9C, 0x24, "ae_ovl1a8_BossClearSparks", "v", ""),
    "Area57_Boss_ResetFlag": (0x8013F370, 0x28, "ae_ovl1a8_BossResetFlag", "v", ""),
    "Area57_Boss_ClearFlag": (0x8013F398, 0x0C, "ae_ovl1a8_BossClearFlag", "v", "u"),
    "Area57_Boss_SetAnim": (0x8013F3A4, 0x38, "ae_ovl1a8_BossSetAnim", "v", "uu"),
    "Area57_Boss_StepAnim": (0x8013F3DC, 0x88, "ae_ovl1a8_BossStepAnim", "v", "u"),
    "Area57_Boss_ClearTable8": (0x8013FFFC, 0x24, "ae_ovl1a8_BossClearTable8", "v", ""),
    "Area57_Boss_AddWaveEntry": (0x80140020, 0xD4, "ae_ovl1a8_BossAddWaveEntry", "s", ""),
    "Area57_Boss_UpdateWaves": (0x801400F4, 0x6A0, "ae_ovl1a8_BossUpdateWaves", "v", ""),
    "Area57_Boss_ClearRipples": (0x801407BC, 0x28, "ae_ovl1a8_BossClearRipples", "v", ""),
    "Area57_Boss_UpdateRipples": (0x801409D4, 0x49C, "ae_ovl1a8_BossUpdateRipples", "v", ""),
    "Area57_Boss_RumbleBig": (0x80140794, 0x28, "ae_ovl1a8_BossRumbleBig", "v", ""),
    "Area57_Boss_SpawnGroundRipples": (0x801407E4, 0x1F0, "ae_ovl1a8_BossSpawnGroundRipples", "v", "uu"),
    "Area57_Boss_SlamImpact": (0x80140F08, 0x60, "ae_ovl1a8_BossSlamImpact", "v", "uu"),
    "Area57_Boss_DeathBurst": (0x80140F68, 0x94, "ae_ovl1a8_BossDeathBurst", "v", "uu"),
    "Area57_Boss_DrawScaledModel": (0x8013DDC4, 0x1EC, "ae_ovl1a8_BossDrawScaledModel", "v", "u"),
    "Area57_Boss_UpdateSequence": (0x8013F464, 0xB98, "ae_ovl1a8_BossUpdateSequence", "v", ""),
    "Area57_Boss_UpdateLateSequence": (0x80140FFC, 0xC54, "ae_ovl1a8_BossUpdateLateSequence", "v", ""),
}

CALL_HOOK_FUNCS = {
    "Area57_Boss_ModelInit",
    "Area57_Boss_SpawnEffectNear", "Area57_Boss_SpawnEffectNearWrap",
    "Area57_Boss_UpdateDebris",
    "Area57_Boss_UpdateRipples",
    "Area57_Boss_RumbleLong", "Area57_Boss_RumbleShort",
    "Area57_Boss_RumbleMedium", "Area57_Boss_SpawnDropPickups",
    "Area57_Boss_LoopStart", "Area57_Boss_LoopStop",
    "Area57_Boss_AddWaveEntry",
    "Area57_Boss_UpdateWaves",
    "Area57_Boss_DrawScaledModel",
    "Area57_Boss_UpdateSparks",
    "Area57_Boss_Init", "Area57_Boss_Start",
    "Area57_Boss_UpdateCameraSequence", "Area57_Boss_Wake",
    "Area57_Boss_Update",
    "Area57_Boss_UpdateWakeSequence", "Area57_Boss_UpdateEntries",
    "Area57_Boss_SpawnEffectGroupsN", "Area57_Boss_SpawnGroundRipples",
    "Area57_Boss_SlamImpact", "Area57_Boss_DeathBurst",
    "Area57_Boss_RumbleBig",
    "Area57_Boss_UpdateSequence", "Area57_Boss_UpdateLateSequence",
}

EXTRA_COVERAGE = {
    "Area57_Boss_ModelApplyLighting": (0x8013D0A0, 0x15C),
    "Area57_Boss_ProjectAndEmit": (0x8013DB94, 0x230),
}
DEFAULT_CASES = {
    "Area57_Boss_Update": 11,
    "Area57_Boss_UpdateWakeSequence": 11,
    "Area57_Boss_UpdateEntries": 7,
    "Area57_Boss_UpdateWaves": 12,
    "Area57_Boss_DrawScaledModel": 21,
    "Area57_Boss_UpdateSequence": 45,
    "Area57_Boss_UpdateLateSequence": 32,
}
SPARK_RENDERER_GAP = (
    "Area57_Boss_UpdateSparks 8013F1F0..8013F310"
)
DEBRIS_RENDERER_GAP = (
    "Area57_Boss_UpdateDebris 8013D52C..8013D644 8013D6AC..8013D808 "
    "8013D818..8013D864"
)
RIPPLE_RENDERER_GAP = (
    "Area57_Boss_UpdateRipples 80140A7C..80140B94 80140C60..80140E2C"
)

U32, I32, P = C.c_uint32, C.c_int32, C.c_void_p
OvlCallFn = C.CFUNCTYPE(I32, P, P, C.c_char_p, U32, U32, C.POINTER(U32))
OvlRenderFn = C.CFUNCTYPE(I32, P, P, C.c_char_p, U32, U32, C.POINTER(U32))
SpawnHookFn = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32)
PositionPadFn = C.CFUNCTYPE(C.c_uint16, P, P, U32)
SoundWaitFn = C.CFUNCTYPE(I32, P, P)
SoundLoopFn = C.CFUNCTYPE(None, P, P, U32)
SoundVoiceFn = C.CFUNCTYPE(None, P, P, U32, U32, U32)
SoundKeyoffFn = C.CFUNCTYPE(None, P, P, U32, C.c_uint16)
SoundPrintFn = C.CFUNCTYPE(None, P, P, U32, U32, I32, I32, I32)
SoundVolumeFn = C.CFUNCTYPE(None, P, P, U32, U32, C.c_uint16)


class SpawnHooks(C.Structure):
    _fields_ = [("user", P), ("world", P), ("PlaySfx", SpawnHookFn),
                ("MatrixLookDir", SpawnHookFn), ("MatrixFromDirXZ", SpawnHookFn),
                ("position_padding", PositionPadFn)]


class OvlHooks(C.Structure):
    _fields_ = [("user", P), ("body", P), ("camupd", P), ("camera", P),
                ("spawn", P), ("world", P), ("zako2", P), ("player1", P),
                ("player2", P),
                ("sound_hw", P), ("stage", P),
                ("original_call", OvlCallFn), ("renderer", OvlRenderFn)]

BodyPrintFn = C.CFUNCTYPE(None, P, P, U32, I32)


class BodyHooks(C.Structure):
    # The first callback fields match AeBodyHooks. These cases only use the
    # diagnostic callback if slot allocation fails; a live allocation path
    # leaves the remaining callbacks untouched.
    _fields_ = [("user", P), ("printf_error", BodyPrintFn)]


class SoundHw(C.Structure):
    _fields_ = [("user", P), ("wait_idle", SoundWaitFn),
                ("wait_loop", SoundLoopFn), ("set_voice_adsr", SoundVoiceFn),
                ("voice_key_off", SoundKeyoffFn), ("printf_error", SoundPrintFn),
                ("set_voice_volume", SoundVolumeFn)]

WorldHookFn = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32)
CamupdHookFn = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32)
SpawnPadFn = C.CFUNCTYPE(C.c_uint16, P, P, U32)
StageHookFn = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32)


class WorldHooks(C.Structure):
    _fields_ = [("user", P), ("GetSolidHeight", WorldHookFn)]


class CamupdHooks(C.Structure):
    _fields_ = [("user", P), ("PadGetState", CamupdHookFn)]


class StageHooks(C.Structure):
    # AeStageHooks has a pointer, a 32-bit stack pointer, alignment padding,
    # then 95 function pointers. Only the action-29 callbacks are exercised.
    _fields_ = [("user", P), ("stack_pointer", U32), ("callbacks", P * 95)]


def build(source_override: Path | None = None, tag="ovl1a8"):
    source_rel = "src/world/ae_ovl1a8.c"
    if source_rel not in check_game.SOURCES:
        check_game.SOURCES.append(source_rel)
    overrides = {source_rel: source_override} if source_override else None
    return check_game.build_dll(overrides=overrides, tag=tag)


class CMem:
    def __init__(self, dll, ram):
        self.dll, self.ram = dll, ram

    def rd8(self, address): return self.dll.ae_rd8(self.ram, address)
    def rd16(self, address): return self.dll.ae_rd16(self.ram, address)
    def rd32(self, address): return self.dll.ae_rd32(self.ram, address)
    def wr8(self, address, value): self.dll.ae_wr8(self.ram, address, value & 0xFF)
    def wr16(self, address, value): self.dll.ae_wr16(self.ram, address, value & 0xFFFF)
    def wr32(self, address, value): self.dll.ae_wr32(self.ram, address, value & 0xFFFFFFFF)


class MipsMem:
    def __init__(self, cpu): self.cpu = cpu
    def rd8(self, address): return self.cpu.read8(address)
    def rd16(self, address): return self.cpu.read16(address)
    def rd32(self, address): return self.cpu.read32(address)
    def wr8(self, address, value): self.cpu.write8(address, value & 0xFF)
    def wr16(self, address, value): self.cpu.write16(address, value & 0xFFFF)
    def wr32(self, address, value): self.cpu.write32(address, value & 0xFFFFFFFF)


class RamImage:
    def __init__(self, dll_path):
        self.dll = C.CDLL(str(dll_path))
        self.dll.ae_ram_create.restype = P
        self.dll.ae_ptr.argtypes, self.dll.ae_ptr.restype = [P, U32, U32], P
        for name in ("ae_rd8", "ae_rd16", "ae_rd32"):
            fn = getattr(self.dll, name)
            fn.argtypes, fn.restype = [P, U32], U32
        for name in ("ae_wr8", "ae_wr16", "ae_wr32"):
            getattr(self.dll, name).argtypes = [P, U32, U32]
        self.dll.ae_ram_failed.argtypes, self.dll.ae_ram_failed.restype = [P], C.c_int
        self.dll.ae_ram_error.argtypes, self.dll.ae_ram_error.restype = [P], C.c_char_p
        self.dll.ae_ram_clear_error.argtypes = [P]
        self.ram = self.dll.ae_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(
            self.dll.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(
            self.dll.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.funcs = {}
        for name, (_address, _size, symbol, result, args) in FUNCS.items():
            fn = getattr(self.dll, symbol)
            fn.argtypes = [P] + ([P] if name in CALL_HOOK_FUNCS else []) + [
                I32 if kind == "s" else U32 for kind in args]
            fn.restype = {"v": None, "s": I32, "u": U32}[result]
            self.funcs[name] = fn

    def sync(self, snapshot):
        C.memmove(self.main, snapshot[0], 0x200000)
        C.memmove(self.scratch, snapshot[1], 0x400)
        self.dll.ae_ram_clear_error(self.ram)

    def call(self, name, args, hooks=None):
        prefix = [self.ram]
        if name in CALL_HOOK_FUNCS:
            prefix.append(C.byref(hooks) if hooks is not None else None)
        return self.funcs[name](*prefix, *args)


def seed_case(mem, name, scenario, rng):
    mem.wr32(MUTATION_SELECTOR, 0)
    mem.wr32(MUTATION_MARKER, 0)
    for address in range(STACK_LO, STACK_HI):
        mem.wr8(address, 0)
    mem.wr16(0x800AE8F0, rng.choice((0, 1, 0x1234, 0xFFFF, rng.randrange(0x10000))))
    mem.wr16(0x800BF670, 0xFFFF)  # silent bank path keeps original PlaySfx RAM quiet
    for i in range(16): mem.wr16(0x800BF670 + i * 2, 0xFFFF)
    mem.wr32(0x80142798, 1 if name == "Area57_Boss_LoopStop" and scenario & 1 else 0xFFFFFFFF)
    mem.wr8(0x800DFDE4, 5 if name == "Area57_Boss_Init" and scenario == 5 else 0)
    mem.wr16(0x800EC2C8, 3 if name == "Area57_Boss_SpawnDropPickups" and scenario & 1 else 0)
    mem.wr8(0x800F4488, 0)       # avoid a pad callback in cases that do not exercise rumble
    mem.wr32(0x800B20D8, 0)
    mem.wr32(0x800B20DC, 0)
    mem.wr8(0x800BAD50, 0)
    for i in range(100):
        mem.wr8(0x800B9900 + i * 0x34, 0)
        mem.wr8(0x800B9900 + i * 0x34 + 2, 0)
    for i in range(256):
        mem.wr16(0x800BBDA0 + i * 0x14, 0xFFFF)
        mem.wr16(0x800BBDA0 + i * 0x14 + 2, 0xFFFF)
        for off in (4, 8, 12, 16): mem.wr32(0x800BBDA0 + i * 0x14 + off, 0)
    for i in range(128): mem.wr16(0x800BEDA0 + i * 2, 0xFFFF)
    for actor in (ACTORS, ACTORS + 0xA0):
        mem.wr32(actor + 0x24, 100)
        mem.wr32(actor + 0x28, -2560)
        mem.wr32(actor + 0x2C, 120)
    for off, value in ((0x24, 150), (0x28, -2400), (0x2C, 170)):
        mem.wr32(0x800EC1E0 + off, value)
    for i in range(80):
        mem.wr8(0x8014278C - i * 12 + 10, rng.randrange(256))
    for i in range(32):
        mem.wr8(0x801427C0 + i * 12 + 6, rng.randrange(2))
        mem.wr8(0x80142B34 - i * 12 + 11, rng.randrange(2))
    for i in range(4):
        mem.wr8(0x80142940 + i * 32 + 11, rng.randrange(2))
    for i in range(2):
        mem.wr8(0x801427A0 + i * 28 + 10, rng.randrange(2))

    if name == "Area57_Boss_SpawnDebris":
        for off in (0, 2, 4): mem.wr16(POSITION + off, rng.choice((-32768, -1, 0, 1, 32767)))
        return (POSITION,)
    if name == "Area57_Boss_AddEntry":
        source = POSITION
        for off in range(8): mem.wr8(source + off, rng.randrange(256))
        mem.wr8(0x801427AA, 1 if scenario & 1 else 0)
        mem.wr8(0x801427C6, 1 if scenario & 1 else 0)
        return (source,)
    if name == "Area57_Boss_AddSparks":
        actor = 0x801A1000
        for off in (0x24, 0x28, 0x2C): mem.wr16(actor + off, rng.choice((-32768, -1, 0, 1, 32767)))
        return actor, scenario & 1
    if name in ("Area57_Boss_SetAnim", "Area57_Boss_StepAnim"):
        actor = ACTORS
        mem.wr32(actor + 0x74, ANIMATIONS)
        mem.wr16(actor + 0x78, scenario & 1 if name.endswith("SetAnim") else 0)
        frames = (7, 8, 8, 0, 0xFFFF, 15)
        mem.wr16(actor + 0x5C, frames[scenario % len(frames)])
        mem.wr8(actor + 0x7B, scenario & 1)
        for i in range(4):
            mem.wr16(ANIMATIONS + i * 4, i * 8)
            mem.wr16(ANIMATIONS + i * 4 + 2, i * 8 + 8)
        if name.endswith("SetAnim"):
            animation = scenario & 1 if scenario & 1 else 1
            return actor, animation
        return (actor,)
    if name == "Area57_Boss_ClearFlag": return (scenario & 0xFF,)
    if name == "Area57_Boss_UpdateCameraSequence":
        slot = 0x800B4D40
        mem.wr8(0x80142328, 1 if scenario == 5 else 0)
        mem.wr8(0x80142321, 22 + scenario % 5 if scenario in (1, 2, 3, 4) else 1)
        for off, value in ((0x0A, 300), (0x0C, -1100), (0x0E, 200),
                           (0x12, 200), (0x14, -900), (0x16, 240)):
            mem.wr16(slot + off, value)
        return (slot,)
    if name == "Area57_Boss_UpdateWakeSequence":
        slot = 0x800B4D40
        mem.wr8(0x80142329, (1, 2, 3, 4)[scenario % 4])
        mem.wr16(0x8014232A, 16 if scenario & 1 else (1, 5, 16)[scenario % 3])
        mem.wr16(0x8014232C, (0, 49, 50, 51)[scenario % 4])
        mem.wr16(slot + 0x0A, rng.randrange(-2000, 2000))
        mem.wr16(slot + 0x0C, rng.randrange(-2000, 2000))
        mem.wr16(slot + 0x0E, rng.randrange(-2000, 2000))
        if scenario == 6:
            mem.wr16(0x8014232A, 4)  # state 1: substate % 5 == 4 triggers rumble
        elif scenario in (7, 8, 9):
            mem.wr8(0x80142329, 0)
            mem.wr16(0x8014232A, (0xFFD4, 0xFFD6, 0xFFE0)[scenario - 7])
        elif scenario == 10:
            mem.wr8(0x80142329, 1)
            mem.wr16(0x8014232A, 1)
            mem.wr32(0x800EC1E0 + 0x60, -256)
            mem.wr32(0x800EC1E0 + 0x64, -384)
        return (slot,)
    if name == "Area57_Boss_UpdateEntries":
        entry = 0x801427A0
        mode = (1, 1, 2, 2, 2, 0)[scenario % 6]
        mem.wr8(entry + 10, mode)
        mem.wr8(entry + 11, (0, 15, 1, 16, 65, 0)[scenario % 6])
        for off, value in ((4, 100), (6, -2600 if scenario & 1 else -2540),
                           (8, -120), (0x14, -32768 if scenario & 1 else 32767),
                           (0x16, -1 if scenario & 1 else 1),
                           (0x18, -100 if scenario & 1 else 100)):
            mem.wr16(entry + off, value)
        mem.wr8(entry + 0x1A, scenario & 1)
        if scenario == 6:
            mem.wr8(entry + 10, 3)  # exercise the original invalid-state fallthrough
        return ()
    if name == "Area57_Boss_SpawnEffectGroupsN":
        for off, value in ((0, 0), (2, -2560), (4, 0)):
            mem.wr16(POSITION + off, value)
        return (POSITION,)
    if name == "Area57_Boss_SpawnGroundRipples":
        for off, value in ((0, 100), (2, -2500), (4, -120)):
            mem.wr16(POSITION + off, value)
        return POSITION, (1, 4, 8, 16, 32)[scenario % 5]
    if name in ("Area57_Boss_SlamImpact", "Area57_Boss_DeathBurst"):
        for off, value in ((0, 100), (2, -2624), (4, 120)):
            mem.wr16(POSITION + off, value)
        return ACTORS, POSITION
    if name == "Area57_Boss_Update":
        slot = 0x800B4D40
        phase = (0, 11, 12, 13, 19, 20, 21, 26)[scenario % 8]
        mem.wr8(0x80142321, phase)
        mem.wr8(0x80142320, 1 if scenario % 3 else 0)
        mem.wr16(0x8014232E, (0, 50, 51, 52, 100)[scenario % 5])
        mem.wr8(0x80142328, 1 if scenario % 7 == 6 else 0)
        player_y = (-3000, -2593, -2592, -2400)[scenario % 4]
        mem.wr32(0x800EC1E0 + 0x28, player_y)
        mem.wr32(0x80142338 + 0x28, -2400 + scenario * 17)
        mem.wr8(0x800EC2D2, 3 if scenario % 3 == 1 else 0)
        mem.wr8(0x800E6916, 1 if scenario % 4 == 3 else 0)
        if scenario == 8:
            mem.wr16(0x8014232E, 60000)  # angle multiplication wraps through signed negative
            mem.wr8(0x800EC2D2, 3)
            mem.wr8(0x800E6916, 0)
        elif scenario == 9:
            mem.wr8(0x80142321, 13)
            mem.wr8(0x80142320, 1)
            mem.wr8(0x800EC2D2, 3)
            mem.wr8(0x800E6916, 0)
            for off, value in ((0x24, -3000), (0x2C, -4000)):
                mem.wr32(0x800EC1E0 + off, value)
                mem.wr32(0x80142338 + off, value)
        elif scenario == 10:
            mem.wr8(0x80142321, 13)
            mem.wr8(0x80142320, 0)
            mem.wr8(0x800EC2D2, 3)
            mem.wr8(0x800E6916, 0)
            for off, value in ((0x24, -3000), (0x2C, -4000)):
                mem.wr32(0x800EC1E0 + off, value)
                mem.wr32(0x80142338 + off, value)
        for off, value in ((0x0A, 0), (0x0C, 0), (0x0E, 0),
                           (0x12, 0), (0x14, 0), (0x16, 0)):
            mem.wr16(slot + off, value)
        return (slot,)
    if name == "Area57_Boss_UpdateSequence":
        phases = (1, 2, 3, 4, 10, 11, 12, 13, 14, 18, 19,
                  19, 19, 19, 20, 21, 22, 23, 24, 25)
        phase = phases[scenario % len(phases)]
        mem.wr8(0x80142320, 0)
        mem.wr8(0x80142321, phase)
        mem.wr8(0x80142322, 1 if phase in (11, 12) else 0)
        mem.wr8(0x80142324, 0)
        mem.wr8(ACTORS + 0x7A, 1 if scenario in (10, 12, 13) else 0)
        mem.wr8(ACTORS + 0x7B, 0)
        mem.wr8(ACTORS + 0x7E, (scenario % 4) + 1)
        mem.wr8(ACTORS + 0x89, 1 if phase == 1 and scenario & 1 else 0)
        mem.wr16(ACTORS + 0x8A, (0, 1, 30)[scenario % 3])
        mem.wr16(ACTORS + 0x90, (0, 1, 16)[scenario % 3])
        mem.wr16(ACTORS + 0x92, (0, 1, 12, 30, 120)[scenario % 5])
        mem.wr16(ACTORS + 0x80, rng.choice((-48, -1, 0, 1, 48)))
        mem.wr16(ACTORS + 0x82, rng.choice((-32, -1, 0, 1, 32)))
        mem.wr16(ACTORS + 0x84, rng.choice((-48, -1, 0, 1, 48)))
        mem.wr32(0x800EC354, 0x801FD000)
        mem.wr16(ACTORS + 0x5C, 0)
        mem.wr32(ACTORS + 0x74, ANIMATIONS)
        mem.wr32(ACTORS + 0x50, 0xB0000001 if scenario == 12 else 0)
        for index in range(8):
            mem.wr16(ANIMATIONS + index * 4, index * 8)
            mem.wr16(ANIMATIONS + index * 4 + 2, index * 8 + 8)
        if scenario >= 20:
            variant = scenario - 20
            phase = phases[variant % len(phases)]
            mem.wr8(0x80142321, phase)
            mem.wr32(0x80142324, 0)
            mem.wr8(ACTORS + 0x7A, 0)
            mem.wr8(ACTORS + 0x89, 0)
            mem.wr16(ACTORS + 0x8A, 0)
            mem.wr16(ACTORS + 0x92, 0)
            mem.wr16(ACTORS + 0x80, 0)
            mem.wr16(ACTORS + 0x82, 0)
            mem.wr16(ACTORS + 0x84, 0)
            mem.wr32(ACTORS + 0x50, 0)
            if phase == 1:
                mem.wr8(ACTORS + 0x89, 1)
                mem.wr16(ACTORS + 0x8A, 2)
            elif phase == 2:
                mem.wr32(0x80142324, 11)
            elif phase == 3:
                mem.wr32(0x80142324, 29)
            elif phase == 4:
                mem.wr32(0x80142324, 15)
            elif phase == 11:
                mem.wr32(0x80142324, 1)
            elif phase == 12:
                mem.wr32(ACTORS + 0x50, 0xB0000001)
            elif phase == 13:
                mem.wr32(0x80142324, 1)
            elif phase == 19:
                mem.wr32(ACTORS + 0x28, -2500)
                mem.wr16(ACTORS + 0x82, 0)
            if variant == 10:
                mem.wr16(0x800AE8F0, 0)
            if variant == 20:
                mem.wr8(0x80142321, 1)
                mem.wr8(ACTORS + 0x89, 0)
                mem.wr16(ACTORS + 0x8A, 0)
                mem.wr16(0x800AE8F0, 0)
            elif variant == 21:
                mem.wr8(0x80142321, 1)
                mem.wr8(ACTORS + 0x89, 1)
                mem.wr16(ACTORS + 0x8A, 1)
            elif variant == 22:
                mem.wr8(0x80142321, 19)
                mem.wr32(ACTORS + 0x28, -2420)
                mem.wr16(ACTORS + 0x82, 0)
            elif variant == 23:
                mem.wr8(0x80142321, 1)
                mem.wr8(ACTORS + 0x7E, 3)
                mem.wr16(ACTORS + 0x90, 1)
                mem.wr32(ACTORS + 0x50, 0xB0000001)
            elif variant == 24:
                mem.wr8(0x80142321, 1)
                mem.wr8(ACTORS + 0x7E, 1)
                mem.wr16(ACTORS + 0x90, 1)
                mem.wr32(ACTORS + 0x50, 0xB0000001)
            for index in range(80):
                mem.wr8(0x8014278C - index * 12 + 10, 0)
            for index in range(2):
                mem.wr8(0x801427A0 + index * 28 + 10, 0)
            for index in range(32):
                mem.wr8(0x801427C0 + index * 12 + 6, 0)
                mem.wr8(0x80142B34 - index * 12 + 11, 0)
        return ()
    if name == "Area57_Boss_UpdateLateSequence":
        phases = (20, 21, 22, 23, 24, 25, 25, 21, 25, 25)
        phase = phases[scenario % len(phases)]
        mem.wr8(0x80142321, phase)
        mem.wr8(0x80142322, 1 if scenario in (2, 4) else 0)
        mem.wr8(0x80142328, 1 if scenario == 7 else 0)
        mem.wr8(0x80142338 + 0x7C, 1 if scenario not in (7,) else 0)
        mem.wr8(0x80142338 + 0x7A, 1 if scenario in (1, 3, 4) else 0)
        mem.wr8(0x80142338 + 0x7E, (scenario % 5) + 1)
        mem.wr16(0x80142338 + 0x8E, (0, 1, 16, 150, 300)[scenario % 5])
        mem.wr16(0x80142338 + 0x92, (0, 1, 2, 15)[scenario % 4])
        mem.wr16(0x80142338 + 0x90, (0, 1, 24, 120)[scenario % 4])
        mem.wr16(0x80142338 + 0x80, rng.choice((-80, -1, 0, 1, 80)))
        mem.wr16(0x80142338 + 0x82, rng.choice((-48, -1, 0, 1, 48)))
        mem.wr16(0x80142338 + 0x84, rng.choice((-80, -1, 0, 1, 80)))
        mem.wr16(0x80142338 + 0x94, rng.choice((-512, -1, 0, 1, 512)))
        mem.wr16(0x80142338 + 0x98, rng.choice((-512, -1, 0, 1, 512)))
        mem.wr32(0x800EC354, 0x801FD000)
        mem.wr32(0x80142338 + 0x74, ANIMATIONS)
        mem.wr32(0x80142338 + 0x50, 0xB0000001 if scenario == 8 else 0)
        if phase == 20:
            mem.wr32(0x80142338 + 0x28,
                     -2560 if scenario & 1 else -2570)
        if scenario == 6:
            mem.wr32(0x800EC1E0 + 0x24, 110)
            mem.wr32(0x800EC1E0 + 0x2C, 130)
        else:
            mem.wr32(0x800EC1E0 + 0x24, 12000)
            mem.wr32(0x800EC1E0 + 0x2C, -12000)
        for index in range(8):
            mem.wr16(ANIMATIONS + index * 4, index * 8)
            mem.wr16(ANIMATIONS + index * 4 + 2, index * 8 + 8)
        if scenario >= 10:
            variant = scenario - 10
            phase = (20, 21, 21, 21, 21, 21, 22, 21, 21, 21,
                     23, 24, 21, 21, 21, 21, 21, 21, 21, 21)[variant % 20]
            actor = ACTORS
            mem.wr8(0x80142321, phase)
            mem.wr8(actor + 0x7C, 1)
            mem.wr8(actor + 0x7A, 0)
            mem.wr8(0x80142322, 0)
            mem.wr8(0x80142328, 0)
            mem.wr16(actor + 0x8E, 0)
            mem.wr16(actor + 0x92, 0)
            mem.wr16(actor + 0x90, 2)
            mem.wr8(actor + 0x8C, 15)
            mem.wr16(actor + 0x80, 0)
            mem.wr16(actor + 0x82, 0)
            mem.wr16(actor + 0x84, 0)
            mem.wr16(actor + 0x94, 0)
            mem.wr16(actor + 0x98, 0)
            mem.wr32(actor + 0x50, 0)
            mem.wr32(actor + 0x24, 100)
            mem.wr32(actor + 0x2C, 120)
            mem.wr32(0x800EC1E0 + 0x24, 12000)
            mem.wr32(0x800EC1E0 + 0x2C, -12000)
            if variant == 0:  # ground impact in the descending phase
                mem.wr32(actor + 0x28, -2500)
            elif variant == 1:  # close player drives the close steering branch
                mem.wr32(0x800EC1E0 + 0x24, 110)
                mem.wr32(0x800EC1E0 + 0x2C, 130)
                mem.wr16(actor + 0x8E, 1)
            elif variant == 2:  # far steering, both speed clamps, and world edge
                mem.wr32(actor + 0x24, 5000)
                mem.wr32(actor + 0x2C, 5000)
                mem.wr16(actor + 0x80, 200)
                mem.wr16(actor + 0x84, -200)
                mem.wr16(actor + 0x90, 24)
            elif variant == 3:  # timer expires with no remaining wave counter
                mem.wr32(0x800EC1E0 + 0x24, 1000)
                mem.wr32(0x800EC1E0 + 0x2C, 1200)
                mem.wr16(actor + 0x8E, 1)
                mem.wr32(actor + 0x50, 0xB0000001)
                mem.wr8(actor + 0x7E, 1)
            elif variant == 4:  # contact response with remaining waves
                mem.wr32(actor + 0x50, 0xB0000001)
                mem.wr16(actor + 0x92, 2)
                mem.wr8(actor + 0x7E, 2)
                mem.wr16(actor + 0x9C, 100)
                mem.wr16(actor + 0x9E, -200)
            elif variant == 5:  # non-sequence camera branch
                mem.wr8(actor + 0x7C, 0)
            elif variant == 6:  # non-mode branch with wave-sfx set
                mem.wr8(actor + 0x7C, 0)
                mem.wr8(0x80142323, 1)
                mem.wr16(actor + 0x78, 5)
            elif variant == 7:  # sub-800 motion with even ring timer
                mem.wr32(0x800EC1E0 + 0x24, 110)
                mem.wr32(0x800EC1E0 + 0x2C, 130)
                mem.wr16(actor + 0x92, 2)
                mem.wr16(actor + 0x90, 24)
            elif variant == 8:  # tagged contact, final counter and hit
                mem.wr32(actor + 0x50, 0xB0000001)
                mem.wr16(actor + 0x8E, 1)
                mem.wr16(actor + 0x90, 1)
                mem.wr8(actor + 0x7E, 1)
            elif variant == 9:  # early return with mode false and wave-sfx clear
                mem.wr8(actor + 0x7C, 0)
            elif variant == 10:  # phase 23 branch and world mode 2 effect group
                mem.wr32(0x800EC350, 2)
            elif variant == 11:  # phase 24 branch and world mode 0 effect group
                mem.wr32(0x800EC350, 0)
            elif variant == 12:  # non-sequence model update with animation 5
                mem.wr8(actor + 0x7C, 0)
                mem.wr8(0x80142323, 1)
                mem.wr32(actor + 0x78, 5)
            elif variant == 13:  # negative steering speeds exercise signed /8 rounding
                mem.wr16(actor + 0x9C, -100)
                mem.wr16(actor + 0x9E, -100)
            elif variant == 14:  # close steering with divisor clamped to 400
                mem.wr32(0x800EC1E0 + 0x24, 110)
                mem.wr32(0x800EC1E0 + 0x2C, 130)
                mem.wr16(actor + 0x7E, 1)
                mem.wr16(actor + 0x92, -100)
            elif variant == 15:  # all wave slots occupied
                mem.wr32(0x800EC1E0 + 0x24, 110)
                mem.wr32(0x800EC1E0 + 0x2C, 130)
            elif variant == 16:  # last wave counter triggers the slam
                mem.wr32(0x800EC1E0 + 0x24, 110)
                mem.wr32(0x800EC1E0 + 0x2C, 130)
                mem.wr16(actor + 0x92, 1)
            elif variant in (17, 18):  # partial contact and final contact slam
                mem.wr16(actor + 0x8E, 10)
                mem.wr16(actor + 0x90, 1)
                mem.wr32(actor + 0x50, 0xB0000001)
                mem.wr8(actor + 0x7E, 2)
                mem.wr16(actor + 0x92, 4 if variant == 17 else 2)
                mem.wr32(0x800EC1E0 + 0x24, 50)
                mem.wr32(0x800EC1E0 + 0x2C, 60)
            elif variant == 19:  # opposite heading/velocity signs exercise -113 turn
                mem.wr32(actor + 0x24, 100)
                mem.wr32(actor + 0x2C, 120)
                mem.wr16(actor + 0x60, 4096)
                mem.wr16(actor + 0x64, 0)
                mem.wr16(actor + 0x94, 100)
                mem.wr16(actor + 0x98, 1120)
                mem.wr32(0x800EC1E0 + 0x24, 12000)
                mem.wr32(0x800EC1E0 + 0x2C, -12000)
            elif variant == 20:  # positive far steering reaches the upper speed clamp and negative /32 rounding
                mem.wr8(0x80142321, 21)
                mem.wr16(actor + 0x80, 80)
                mem.wr16(actor + 0x84, 80)
                mem.wr16(actor + 0x8E, 0)
                mem.wr16(actor + 0x90, 1)
                mem.wr16(actor + 0x92, 2)
                mem.wr32(actor + 0x50, 0xB0000001)
                mem.wr16(actor + 0x94, 12000)
                mem.wr16(actor + 0x98, 12000)
                mem.wr32(0x800EC1E0 + 0x24, 12000)
                mem.wr32(0x800EC1E0 + 0x2C, 12000)
                mem.wr16(0x800AE8F0, 0x1234)
            elif variant == 21:  # zero wave counter with a tagged body contact
                mem.wr8(0x80142321, 21)
                mem.wr16(actor + 0x8E, 0)
                mem.wr16(actor + 0x90, 1)
                mem.wr16(actor + 0x92, 0)
                mem.wr8(actor + 0x7E, 1)
                mem.wr32(actor + 0x50, 0xB0000001)
            for index in range(80):
                mem.wr8(0x8014278C - index * 12 + 10, 0)
            for index in range(2):
                mem.wr8(0x801427A0 + index * 28 + 10, 0)
            for index in range(32):
                mem.wr8(0x801427C0 + index * 12 + 6, 0)
                mem.wr8(0x80142B34 - index * 12 + 11, 0)
            for index in range(4):
                mem.wr8(0x80142940 + index * 32 + 11, 0)
            if variant == 15:
                for index in range(4):
                    wave = 0x80142940 + index * 32
                    mem.wr8(wave + 11, 1)
                    mem.wr8(wave + 10, 0)
            if variant in (17, 18):
                mem.wr32(0x800EC1E0 + 0x24, 50)
                mem.wr32(0x800EC1E0 + 0x2C, 60)
            if variant == 19:
                mem.wr16(actor + 0x80, 0)
                mem.wr16(actor + 0x84, 0)
        return ()
    if name == "Area57_Boss_Wake":
        mem.wr8(0x80142328, 0 if scenario == 0 else 1)
        return ()
    if name == "Area57_Boss_Init":
        return ()
    if name == "Area57_Boss_Start":
        return ()
    if name == "Area57_Boss_ModelInit":
        actor = ACTORS
        return 1, actor, actor + 0x74
    if name in ("Area57_Boss_SpawnEffectNear", "Area57_Boss_SpawnEffectNearWrap"):
        coordinate = 0 if scenario & 1 else 30000
        mem.wr16(POSITION, coordinate)
        mem.wr16(POSITION + 2, 0)
        mem.wr16(POSITION + 4, coordinate)
        mem.wr32(0x800EC204, 0)
        mem.wr32(0x800EC20C, 0)
        if name.endswith("Wrap"):
            return POSITION, ACTORS, 80, 0
        return POSITION, ACTORS, 80, 512, 128
    if name == "Area57_Boss_UpdateSparks":
        mem.wr32(ACTORS + 0x24, -100 if scenario & 1 else 100)
        mem.wr32(ACTORS + 0x28, -2560)
        mem.wr32(ACTORS + 0x2C, -120 if scenario & 2 else 120)
        for i in range(32):
            spark = 0x801427C0 + i * 12
            mem.wr8(spark + 6, 0)
            mem.wr8(spark + 8, 1)
        spark = 0x801427C0
        mem.wr8(spark + 6, 2)
        mem.wr8(spark + 8, 0)
        mem.wr8(spark + 7, scenario & 1)
        mem.wr16(spark, 0)
        mem.wr16(spark + 2, 0)
        mem.wr16(spark + 4, 0)
        delayed = spark + 12
        mem.wr8(delayed + 6, 1)
        mem.wr8(delayed + 8, 1)
        mem.wr8(delayed + 7, scenario & 1)
        return (ACTORS,)
    if name == "Area57_Boss_UpdateDebris":
        for i in range(80):
            debris = 0x8014278C - i * 12
            mem.wr8(debris + 10, 0 if i & 1 else rng.choice((1, 2, 40, 255)))
            for off in (0, 2, 4):
                mem.wr16(debris + off, rng.choice((-32768, -1, 0, 1, 32767)))
            mem.wr16(debris + 6, rng.choice((0, 1, 0x7FFF, 0x8000, 0xFFFF)))
            mem.wr8(debris + 8, rng.choice((0, 1, 0x7F, 0x80, 0xFF)))
            mem.wr8(debris + 9, rng.choice((0, 1, 0x7F, 0x80, 0xFF)))
        mem.wr32(0x800EC354, 0x801FD000)
        return ()
    if name == "Area57_Boss_UpdateRipples":
        for i in range(32):
            ripple = 0x80142B34 - i * 12
            mem.wr8(ripple + 11, 0 if i & 1 else rng.choice((1, 2, 40, 255)))
            for off in (0, 2, 4):
                mem.wr16(ripple + off, rng.choice((-32768, -2593, -2592, -1,
                                                     0, 1, 32767)))
            mem.wr16(ripple + 6, rng.choice((-32768, -2593, -2592, -1, 0, 32767)))
            for off in (8, 9, 10):
                mem.wr8(ripple + off, rng.choice((0, 1, 10, 11, 12, 0x7F, 0x80, 0xFF)))
        mem.wr32(0x800EC354, 0x801FD000)
        return ()
    if name == "Area57_Boss_UpdateWaves":
        mem.wr32(0x800EC354, 0x801FD000)
        # Keep an inactive slot, two expanding rings around both sides of the
        # age threshold, and one falling ring. Later cases vary its landing,
        # horizontal range, and impact-object path.
        for i in range(4):
            wave = 0x80142940 + i * 32
            for off in range(32): mem.wr8(wave + off, 0)
        mem.wr8(0x80142940 + 11, 0)
        for i, age in ((1, 20 if scenario & 1 else 128),
                       (2, 129 if scenario & 1 else 128)):
            wave = 0x80142940 + i * 32
            mem.wr8(wave + 11, 1)
            mem.wr8(wave + 10, age)
            mem.wr32(wave + 20, rng.choice((0, 4095, 0x7FFF, 0x8000)))
            mem.wr32(wave + 24, rng.choice((0, 1, 2048, 4095)))
            mem.wr16(wave + 28, rng.choice((0, 1, 170, 4095)))
        wave = 0x80142940 + 3 * 32
        mem.wr8(wave + 11, 2)
        mem.wr8(wave + 10, (180, 181, 200, 255, 0, 129)[scenario % 6])
        near_xz = not (scenario & 2)
        mem.wr16(wave + 4, 100 if near_xz else 12000)
        mem.wr16(wave + 6, -3000 if scenario & 1 else -2500)
        mem.wr16(wave + 8, 120 if near_xz else -12000)
        mem.wr16(wave + 12, rng.choice((-48, -1, 0, 1, 48)))
        mem.wr16(wave + 14, rng.choice((-16, -8, 0, 8, 16)))
        mem.wr16(wave + 16, rng.choice((-48, -1, 0, 1, 48)))
        if scenario & 1:
            impact_object = 0x801A2000
            mem.wr32(wave, impact_object)
            mem.wr32(impact_object + 44, 0x60000000)
        else:
            mem.wr32(wave, 0)
        mem.wr8(0x80142322, 1 if scenario in (1, 3) else 0)
        if scenario >= 6:
            variant = scenario - 6
            wave0, wave1, wave2, wave3 = (0x80142940 + i * 32 for i in range(4))
            for item in (wave0, wave1, wave2, wave3):
                for off in range(32): mem.wr8(item + off, 0)
            if variant == 0:
                mem.wr8(wave0 + 11, 1)
                mem.wr8(wave0 + 10, 0)
                mem.wr16(wave0 + 20, 0)
                mem.wr32(wave0 + 24, 0)
                mem.wr32(wave0 + 28, 0)
            elif variant == 1:
                mem.wr8(wave0 + 11, 1)
                mem.wr8(wave0 + 10, 0)
                mem.wr32(wave0 + 20, -0x800)  # negative ring angle
                mem.wr32(wave0 + 24, 0xFFFFF800)  # negative ring phase
                mem.wr32(wave0 + 28, 0)
                for item, ring_angle, ring_phase in (
                        (wave1, 0, 2048), (wave2, 0, 3072)):
                    mem.wr8(item + 11, 1)
                    mem.wr8(item + 10, 129)
                    mem.wr32(item + 20, ring_angle)
                    mem.wr32(item + 24, ring_phase)
                    mem.wr32(item + 28, 0)
            else:
                mem.wr8(wave3 + 11, 2)
                mem.wr8(wave3 + 10, 100)
                mem.wr16(wave3 + 4, 1200 if variant in (2, 4) else 100)
                mem.wr16(wave3 + 6, -3000 if variant in (2, 4, 5) else -2590)
                mem.wr16(wave3 + 8, 1200 if variant in (2, 4) else 120)
                mem.wr16(wave3 + 12, -120 if variant == 3 else
                         (120 if variant in (2, 4) else 0))
                mem.wr16(wave3 + 14, -8 if variant in (2, 4) else 0)
                mem.wr16(wave3 + 16, 120 if variant == 3 else
                         (-120 if variant in (2, 4) else 0))
                if variant in (2, 4):
                    mem.wr32(0x800EC1E0 + 0x28, -4000)
                    mem.wr32(wave3, 0x801A2000)
                    mem.wr32(0x801A2000 + 44, 0x60000000)
                else:
                    mem.wr32(0x800EC1E0 + 0x28, -2400)
                    mem.wr32(wave3, 0)
                    mem.wr16(0x800AE8F0, 0)
                if variant == 4:
                    mem.wr16(0x800AE8F0, 4)
        return ()
    if name == "Area57_Boss_DrawScaledModel":
        actor = ACTORS
        mem.wr8(0x80142321, scenario % 20)
        mem.wr32(0x80142324, 8 if scenario == 4 else scenario % 8)
        mem.wr16(actor + 0x86, (-100, 0, 4090, 32767, -32768)[scenario % 5])
        mem.wr8(actor + 0x7E, (0, 1, 2, 3)[scenario % 4])
        mem.wr32(0x80141D00, (0, 7, 8, 15, 255)[scenario % 5])
        if scenario == 20:
            mem.wr8(0x80142321, 20)  # late-phase default model scale
        return (actor,)
    if name == "Area57_Boss_SpawnDropPickups":
        return (POSITION,)
    if name in ("Area57_Boss_LoopStart", "Area57_Boss_LoopStop"):
        return (174,)
    return ()


class Harness:
    def __init__(self, dll_path):
        reg.declare("stack_window", "801FF000..80200000")
        reg.check_ram_mask(PARTICLE_POSITION_PADDING)
        reg.check_ram_mask(EFFECT_POSITION_PADDING)
        reg.check_ram_mask(TRAIL_POSITION_PADDING)
        self.cpu = mr.MipsRam.from_extract(AREA)
        self.cpu.load_area(OVERLAY.read_bytes(), OVERLAY_BASE)
        self.cpu.attach_gte_oracle()
        self.cpu.coverage = set()
        self.base = self.cpu.snapshot()
        self.c = RamImage(dll_path)
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0
        self.seed = 0x1A857
        self.mips_group_padding = []
        self.c_group_padding_index = 0
        self.mips_entry_padding = []

    def make_hooks(self):
        dll = self.c.dll
        dll.ae_sdk_MatrixLookDir.argtypes = [P, U32, U32]
        dll.ae_sdk_MatrixLookDir.restype = U32
        dll.ae_sdk_MatrixFromDirXZ.argtypes = [P, U32, U32]
        dll.ae_sdk_MatrixFromDirXZ.restype = U32
        dll.ae_PlaySfx.argtypes = [P, P, I32]
        dll.ae_PlaySfx.restype = I32

        def play_sfx(_user, ram, sound, _b, _c, _d):
            return dll.ae_PlaySfx(ram, None, C.c_int32(sound).value)

        def matrix_look(_user, ram, direction, matrix, _c, _d):
            return dll.ae_sdk_MatrixLookDir(ram, direction, matrix)

        def matrix_from(_user, ram, direction, matrix, _c, _d):
            return dll.ae_sdk_MatrixFromDirXZ(ram, direction, matrix)

        def position_padding(_user, _ram, function):
            if int(function) == 0x8013E120:
                return self.mips_entry_padding[0] if self.mips_entry_padding else 0
            if int(function) == 0x800696EC:
                index = self.c_group_padding_index
                self.c_group_padding_index += 1
                if index < len(self.mips_group_padding):
                    return self.mips_group_padding[index]
            return 0

        def ground_height(_user, _ram, _x, _y, _z, _out):
            return 0

        def pad_state(_user, _ram, _a0, _a1, _a2, _a3):
            return 6

        def original_call(_user, _ram, name, address, count, words):
            values = tuple(int(words[i]) & 0xFFFFFFFF for i in range(count))
            self.c_events.append((name.decode("ascii"), int(address), values))
            return 0

        def renderer(_user, _ram, name, address, count, words):
            values = tuple(int(words[i]) & 0xFFFFFFFF for i in range(count))
            self.c_events.append((name.decode("ascii"), int(address), values))
            if name == b"Area57_Boss_SparkProject":
                return (values[5] + 24) & 0xFFFFFFFF
            return 0

        self._callbacks = [
            SpawnHookFn(play_sfx), SpawnHookFn(matrix_look), SpawnHookFn(matrix_from),
            SpawnPadFn(position_padding), WorldHookFn(ground_height),
            CamupdHookFn(pad_state), OvlCallFn(original_call), OvlRenderFn(renderer),
        ]
        self.spawn_hooks = SpawnHooks()
        self.spawn_hooks.PlaySfx = self._callbacks[0]
        self.spawn_hooks.MatrixLookDir = self._callbacks[1]
        self.spawn_hooks.MatrixFromDirXZ = self._callbacks[2]
        self.spawn_hooks.position_padding = self._callbacks[3]
        self.world_hooks = WorldHooks()
        self.world_hooks.GetSolidHeight = self._callbacks[4]
        self.camupd_hooks = CamupdHooks()
        self.camupd_hooks.PadGetState = self._callbacks[5]
        self.stage_hooks = StageHooks()
        self._stage_callback = StageHookFn(lambda _u, _r, _a0, _a1, _a2, _a3: 0)
        for index in (72, 77, 91):
            self.stage_hooks.callbacks[index] = C.cast(self._stage_callback, P).value
        self.sound_hw = SoundHw()
        self._sound_callbacks = [
            SoundWaitFn(lambda _u, _r: 0),
            SoundLoopFn(lambda _u, _r, _units: None),
            SoundVoiceFn(lambda _u, _r, _channel, _a, _b: None),
            SoundKeyoffFn(lambda _u, _r, _half, _mask: None),
            SoundPrintFn(lambda _u, _r, _fmt, _argc, _a, _b, _c: None),
            SoundVolumeFn(lambda _u, _r, _channel, _side, _volume: None),
        ]
        for field, callback in zip(("wait_idle", "wait_loop", "set_voice_adsr",
                                    "voice_key_off", "printf_error", "set_voice_volume"),
                                   self._sound_callbacks):
            setattr(self.sound_hw, field, callback)
        hooks = OvlHooks()
        self.body_hooks = BodyHooks()
        self.body_hooks.printf_error = BodyPrintFn(lambda _u, _r, _fmt, _kind: None)
        hooks.body = C.cast(C.pointer(self.body_hooks), P)
        hooks.camupd = C.cast(C.pointer(self.camupd_hooks), P)
        hooks.spawn = C.cast(C.pointer(self.spawn_hooks), P)
        hooks.world = C.cast(C.pointer(self.world_hooks), P)
        hooks.stage = C.cast(C.pointer(self.stage_hooks), P)
        self.zako2_items_hooks = z2.ItemsHooks()
        self.zako2_hooks = z2.Zako2Hooks()
        self.zako2_hooks.items_hooks = C.pointer(self.zako2_items_hooks)
        hooks.zako2 = C.cast(C.pointer(self.zako2_hooks), P)
        hooks.sound_hw = C.cast(C.pointer(self.sound_hw), P)
        self.camera_hooks = camera_check.HOOK_STRUCT()
        self._camera_callbacks = [
            camera_check.HOOKFN(lambda _u, _r, _a0, _a1, _a2, _a3: 0)
            for _name, _address, _count in camera_check.HOOKS
        ]
        for field, callback in zip(
                (name for name, _address, _count in camera_check.HOOKS),
                self._camera_callbacks):
            setattr(self.camera_hooks, field, callback)
        hooks.camera = C.cast(C.pointer(self.camera_hooks), P)
        self.player1_hooks = player1_check.Player1Hooks()
        self._player1_callbacks = [
            player1_check.PLAYER1_HOOK(lambda _u, _r, _a0, _a1, _a2, _a3: 0)
            for _ in range(12)
        ]
        for field, callback in zip(
                ("CdIntToPos", "CdPosToInt", "CdControl", "CdReadyCallback",
                 "VSync", "printf", "SoundSetVolumeMode", "EventCamCreate",
                 "ModelEmitPrimsCycleColors", "DrawRibbonBetween",
                 "ModelProjectVertices", "ModelEmitPrims"),
                self._player1_callbacks):
            setattr(self.player1_hooks, field, callback)
        self.player1_hooks.sound_hw = C.cast(C.pointer(self.sound_hw), P)
        hooks.player1 = C.cast(C.pointer(self.player1_hooks), P)
        hooks.original_call = self._callbacks[6]
        hooks.renderer = self._callbacks[7]
        return hooks

    def run(self, name, scenario=0, selector=0):
        self.cpu.restore(self.base)
        self.cpu.coverage = self.cpu.coverage
        mips_mem = MipsMem(self.cpu)
        rng = random.Random(self.seed + scenario * 0x9E37 + selector)
        args = seed_case(mips_mem, name, scenario, rng)
        mips_mem.wr32(MUTATION_SELECTOR, selector)
        mips_mem.wr32(MUTATION_MARKER, 0)
        before = self.cpu.snapshot()
        self.c_events = []
        self.mips_events = []
        self.mips_group_padding = []
        self.c_group_padding_index = 0
        self.mips_entry_padding = []

        address, size, _symbol, result_kind, _arg_types = FUNCS[name]
        budget = max(600, size * 120)

        def sdk_hook(hook_name, hook_address):
            def hook(_cpu, regs):
                self.mips_events.append((hook_name, hook_address,
                                         (regs[4] & 0xFFFFFFFF,)))
                return 0
            return hook

        hooks = {
            0x8002FB00: sdk_hook("SetRotMatrix", 0x8002FB00),
            0x8002FB30: sdk_hook("SetTransMatrix", 0x8002FB30),
            0x8004BB68: lambda _cpu, _regs: 0,
            0x80097BFC: lambda _cpu, _regs: 0,
            0x80096638: lambda _cpu, _regs: 0,
            0x8008760C: lambda _cpu, _regs: 0,
        }
        patches = {}
        def capture_entry_position_padding(cpu, regs):
            if regs[4] == 200:
                self.mips_entry_padding.append(cpu.read16(regs[5] + 6))
            regs[2] = 1 if regs[4] < 256 else 0
            return 0x80069704

        def capture_group_padding(cpu, regs):
            # SpawnEffectGroup's local vector begins at sp+24 (decimal), so
            # its untouched fourth halfword is at sp+30 (decimal).
            self.mips_group_padding.append(cpu.read16(regs[29] + 30))
            regs[20] = 0x800B0000
            return 0x8006976C

        patches[0x80069700] = capture_entry_position_padding
        patches[0x80069768] = capture_group_padding
        def renderer_hook(hook_name, hook_address, count=4):
            def hook(_cpu, regs):
                values = tuple(regs[index] & 0xFFFFFFFF
                               for index in range(4, 4 + count))
                self.mips_events.append((hook_name, hook_address, values))
                return 0
            return hook
        if name in ("Area57_Boss_UpdateWaves",
                    "Area57_Boss_UpdateSequence",
                    "Area57_Boss_UpdateLateSequence"):
            hooks[0x8006E5B4] = renderer_hook("EffectRingDraw", 0x8006E5B4)
        if name in ("Area57_Boss_UpdateWaves",
                    "Area57_Boss_UpdateLateSequence"):
            def draw_shadow(cpu, regs):
                position = regs[4]
                values = (
                    C.c_int32(cpu.read32(position)).value & 0xFFFFFFFF,
                    C.c_int32(cpu.read32(position + 4)).value & 0xFFFFFFFF,
                    C.c_int32(cpu.read32(position + 8)).value & 0xFFFFFFFF,
                    regs[5] & 0xFFFFFFFF,
                    regs[6] & 0xFFFFFFFF,
                )
                self.mips_events.append(("DrawShadow", 0x8001F604, values))
                return 0
            hooks[0x8001F604] = draw_shadow
        if name in ("Area57_Boss_UpdateEntries", "Area57_Boss_UpdateWakeSequence"):
            hooks[0x8006E5B4] = renderer_hook("EffectRingDraw", 0x8006E5B4)
        def project_and_emit(cpu, regs):
            scale = regs[5]
            values = (regs[4] & 0xFFFFFFFF,
                      cpu.read32(scale), cpu.read32(scale + 4),
                      cpu.read32(scale + 8), regs[6] & 0xFFFFFFFF)
            self.mips_events.append(("Area57_Boss_ProjectAndEmit",
                                     0x8013DB94, values))
            return 0
        hooks[0x8013DB94] = project_and_emit
        if name in ("Area57_Boss_UpdateDebris",
                    "Area57_Boss_UpdateSequence",
                    "Area57_Boss_UpdateLateSequence"):
            reg.declare("unreachable", DEBRIS_RENDERER_GAP)

            def debris_renderer(cpu, regs):
                return 0x8013D648

            def debris_project(cpu, regs):
                debris = regs[17]
                cursor = regs[19]
                values = (
                    debris,
                    cpu.read16(debris), cpu.read16(debris + 2), cpu.read16(debris + 4),
                    C.c_int8(cpu.read8(debris + 8)).value & 0xFFFFFFFF,
                    cpu.read16(debris + 6),
                    C.c_int8(cpu.read8(debris + 9)).value & 0xFFFFFFFF,
                    cpu.read8(debris + 10), regs[21], cursor,
                )
                self.mips_events.append(("Area57_Boss_DebrisProjectAndEmit",
                                         0x8013D6AC, values))
                return 0x8013D80C

            def debris_header(cpu, regs):
                cursor = regs[19]
                self.mips_events.append(("Area57_Boss_DebrisBatchHeader",
                                         0x8013D818, (cursor,)))
                return 0x8013D868

            patches.update({
                0x8013D52C: debris_renderer,
                0x8013D6AC: debris_project,
                0x8013D818: debris_header,
            })
        if name in ("Area57_Boss_UpdateRipples",
                    "Area57_Boss_UpdateSequence",
                    "Area57_Boss_UpdateLateSequence"):
            reg.declare("unreachable", RIPPLE_RENDERER_GAP)

            def ripple_setup(cpu, regs):
                old_seed = cpu.read16(0x800AE8F0)
                next_seed = (old_seed * 5 + 1) & 0xFFFF
                cpu.write16(0x800AE8F0, next_seed)
                self.mips_events.append(("Area57_Boss_RippleMeshSetup",
                                         0x80140A7C, (old_seed, next_seed)))
                cursor = cpu.read32(0x800EC354)
                regs[19] = cursor
                regs[18] = (cursor + 4) & 0xFFFFFFFF
                return 0x80140B98

            def ripple_project(cpu, regs):
                ripple = regs[16]
                values = (
                    ripple,
                    C.c_int16(cpu.read16(ripple)).value & 0xFFFFFFFF,
                    C.c_int16(cpu.read16(ripple + 2)).value & 0xFFFFFFFF,
                    C.c_int16(cpu.read16(ripple + 4)).value & 0xFFFFFFFF,
                    C.c_int16(cpu.read16(ripple + 6)).value & 0xFFFFFFFF,
                    C.c_int8(cpu.read8(ripple + 8)).value & 0xFFFFFFFF,
                    C.c_int8(cpu.read8(ripple + 9)).value & 0xFFFFFFFF,
                    C.c_int8(cpu.read8(ripple + 10)).value & 0xFFFFFFFF,
                    cpu.read8(ripple + 11), regs[19],
                )
                self.mips_events.append(("Area57_Boss_RippleProjectAndEmit",
                                         0x80140C60, values))
                return 0x80140E30

            patches.update({
                0x80140A7C: ripple_setup,
                0x80140C60: ripple_project,
            })
        if name in ("Area57_Boss_UpdateSparks",
                    "Area57_Boss_UpdateSequence",
                    "Area57_Boss_UpdateLateSequence"):
            reg.declare("unreachable", SPARK_RENDERER_GAP)

            def spark_renderer(cpu, regs):
                actor = regs[22]
                spark = regs[17]
                cursor = regs[18]
                status = cpu.read8(spark + 7)
                values = (actor, spark,
                          C.c_int16(cpu.read16(spark)).value & 0xFFFFFFFF,
                          C.c_int16(cpu.read16(spark + 2)).value & 0xFFFFFFFF,
                          C.c_int16(cpu.read16(spark + 4)).value & 0xFFFFFFFF,
                          cursor, status)
                self.mips_events.append(("Area57_Boss_SparkProject", 0x8013F228, values))
                if status:
                    # The original's `li a0, 207` sits in the replaced GTE
                    # block's branch delay slot immediately before F314.
                    regs[4] = 207
                    return 0x8013F314
                regs[18] = (cursor + 24) & 0xFFFFFFFF
                return 0x8013F328

            patches[0x8013F1F0] = spark_renderer
        try:
            original = self.cpu.call(address, args, hooks=hooks, patches=patches,
                                     limit=budget)
        except Exception as exc:  # pragma: no cover - shown with function context
            self.fail(name, scenario, f"MIPS failed within {budget} instructions: {exc}")
            return False

        self.c.sync(before)
        try:
            c_hooks = self.make_hooks()
            self.c_events = []
            actual = self.c.call(name, args, c_hooks)
        except Exception as exc:  # pragma: no cover - ctypes errors are useful diagnostics
            self.fail(name, scenario, f"C call failed: {exc}")
            return False
        self.counts[name] += 1
        if result_kind != "v" and (int(actual) & 0xFFFFFFFF) != original[2]:
            self.fail(name, scenario,
                      f"return C={int(actual) & 0xFFFFFFFF:08X} MIPS={original[2]:08X}")
            return False

        expected = bytearray(self.cpu.ram[:STACK_LO - 0x80000000])
        got = bytearray(self.c.main[:STACK_LO - 0x80000000])
        for offset, length in (PARTICLE_POSITION_PADDING + EFFECT_POSITION_PADDING +
                               TRAIL_POSITION_PADDING):
            expected[offset:offset + length] = b"\0" * length
            got[offset:offset + length] = b"\0" * length
        if got != expected:
            offset = next(i for i, (a, b) in enumerate(zip(got, expected)) if a != b)
            self.fail(name, scenario,
                      f"RAM differs at {0x80000000 + offset:08X}: C={got[offset:offset+8].hex()} "
                      f"MIPS={expected[offset:offset+8].hex()}")
            return False
        if bytes(self.c.scratch) != bytes(self.cpu.scratch):
            self.fail(name, scenario, "scratchpad differs")
            return False
        if self.c_events != self.mips_events:
            mismatch = next((i for i, pair in enumerate(zip(self.c_events, self.mips_events))
                             if pair[0] != pair[1]),
                            min(len(self.c_events), len(self.mips_events)))
            self.fail(name, scenario,
                      f"hook events differ at #{mismatch}: "
                      f"C={self.c_events[mismatch:mismatch + 1]!r} "
                      f"MIPS={self.mips_events[mismatch:mismatch + 1]!r} "
                      f"(counts {len(self.c_events)}/{len(self.mips_events)})")
            return False
        if self.c.dll.ae_ram_failed(self.c.ram):
            error = self.c.dll.ae_ram_error(self.c.ram).decode(errors="replace")
            self.fail(name, scenario, f"AeRam failed: {error}")
            return False
        return True

    def fail(self, name, scenario, message):
        self.failures += 1
        print(f"FAIL {name}[case={scenario}]: {message}", flush=True)

    def all(self, cases=None):
        self.cpu.coverage.clear()
        ok = True
        for name in FUNCS:
            count = DEFAULT_CASES.get(name, 6) if cases is None else cases
            for scenario in range(count):
                ok = self.run(name, scenario) and ok
            print(f"{name}: {self.counts[name]} cases", flush=True)
        return ok

    def finish(self, extra_coverage, default_run=False, show_coverage=False):
        items = [(name, address, size, True) for name, (address, size, *_tail) in FUNCS.items()]
        items.extend((name, address, size, False)
                     for name, (address, size) in extra_coverage.items())
        summary = reg.coverage_summary(items, self.cpu.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        gaps = {name: row for name, row in summary.items()
                if row["uncovered_count"] or row["stale_declared"]}
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps,
                              extra={"overlay": "1A8.bin", "areas": [0x57]},
                              function_names=list(FUNCS))
        for name, row in gaps.items():
            print(f"COVERAGE GAP {name}: {row['uncovered']}; stale={row['stale_declared']}", flush=True)
        return not gaps and not self.failures


def instrument_mutations(source):
    result = source
    for index, (name, (_address, _size, symbol, _ret, _args)) in enumerate(FUNCS.items(), 1):
        pattern = re.compile(r"(\b(?:void|int32_t|uint32_t)\s+" + re.escape(symbol) +
                             r"\s*\([^)]*\)\s*\{)", re.S)
        code = "\n".join(
            f"    if (ae_rd32(r, 0x{MUTATION_SELECTOR:08X}u) == {index * 2 - variant}u) "
            f"ae_wr32(r, 0x{MUTATION_MARKER:08X}u, 0x1A800000u | {index * 2 - variant}u);"
            for variant in (1, 2))
        result, count = pattern.subn(r"\1\n" + code, result, count=1)
        if count != 1:
            raise SystemExit(f"could not instrument {symbol} for mutations")
    return result


def run_mutations():
    source_path = ROOT / "src/world/ae_ovl1a8.c"
    mutant_path = ROOT / "extract/codex-task/ae_ovl1a8_mutant.c"
    mutant_path.write_text(instrument_mutations(source_path.read_text(encoding="utf-8")), encoding="utf-8")
    try:
        harness = Harness(build(mutant_path, "ovl1a8_mutant"))
        caught = 0
        tested = 0
        for index, name in enumerate(FUNCS, 1):
            for variant in (1, 2):
                selector = index * 2 - variant
                before = harness.failures
                if not harness.run(name, variant - 1, selector) and harness.failures > before:
                    caught += 1
                tested += 1
        print(f"mutations caught: {caught}/{tested}", flush=True)
        return 0 if caught == tested else 1
    finally:
        mutant_path.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--cases", type=int)
    args = parser.parse_args()
    if args.mutate:
        return run_mutations()
    harness = Harness(build())
    passed = harness.all(args.cases)
    coverage_ok = harness.finish(EXTRA_COVERAGE, default_run=args.cases is None,
                                 show_coverage=args.coverage)
    print(f"differential: {sum(harness.counts.values())} cases; "
          f"{'PASS' if passed and coverage_ok else 'FAIL'}", flush=True)
    return 0 if passed and coverage_ok else 1


if __name__ == "__main__":
    sys.exit(main())
