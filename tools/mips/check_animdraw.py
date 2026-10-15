"""Differential checks for the animation, player draw, RC car, and Monkey Radar ports.

The original SCUS_944.23 instructions and the native AeRam functions run on the same EXE image
and real area members. Renderer, audio, and SDK hardware calls are deterministic named hooks.
Run with .venv/Scripts/python.exe tools/mips/check_animdraw.py [--coverage] [--mutate].
"""
from __future__ import annotations

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
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(ROOT / "tools"))
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

M32 = 0xFFFFFFFF
STACK_LO = 0x801FF000
STACK_HI = 0x80200000
DEFAULT_SEED = 0x944230
MODEL = 0x80180000
ACTOR = 0x80181000
ANIMATION = 0x80182000
POSE = 0x80183000
FRONT = 0x80184000
UP = 0x80184010
MATRIX = 0x80184100
PLAYER = 0x800EC1E0
AREA_STATE = 0x800F4350
ANIM_ANGLES = 0x800F4B30
MONKEYS = 0x800E5500
AREAS = ("05B.bin", "064.bin", "066.bin", "05E.bin")

# address, size, C symbol, C argument kinds, return value is meaningful
FUNCS = {
    "MatrixFromFrontUp": (0x80014A84, 748, "ae_MatrixFromFrontUp", "uuuu", False),
    "AnimDecodeFrame": (0x80014D70, 196, "ae_AnimDecodeFrame", "u", False),
    "AnimFramePtr": (0x80015240, 44, "ae_AnimFramePtr", "uu", True),
    "AnimDecodeLerpPtr": (0x8001526C, 624, "ae_AnimDecodeLerpPtr", "uuuu", False),
    "StepToward": (0x800463A4, 64, "ae_StepToward", "sss", True),
    "IntSqrt": (0x80046584, 148, "ae_IntSqrt", "s", True),
    "PlayerAnimateAndDraw": (0x80063628, 2204, "ae_PlayerAnimateAndDraw", "h", False),
    "PlayerSwimSfxUpdate": (0x80068BC0, 328, "ae_PlayerSwimSfxUpdate", "h", False),
    "PlayerRumbleSlam": (0x8007C618, 180, "ae_PlayerRumbleSlam", "h", False),
    "PlayerCanActivateGadget": (0x8007CA48, 152, "ae_PlayerCanActivateGadget", "u", True),
    "AreaGetMonkeyRecord": (0x80097E88, 164, "ae_AreaGetMonkeyRecord", "auu", True),
    "PlayerHeadLookUpdate": (0x80098FEC, 876, "ae_PlayerHeadLookUpdate", "h", False),
    "RcCarDraw": (0x80099AE0, 356, "ae_RcCarDraw", "h", False),
    "RadarUpdateDraw": (0x80099D70, 1036, "ae_RadarUpdateDraw", "h", False),
    "MonkeyFlag248Set": (0x8009A17C, 40, "ae_MonkeyFlag248Set", "u", True),
    "MonkeyRadarSignal": (0x8009A1A4, 836, "ae_MonkeyRadarSignal", "h", True),
}
BASE_CASES = {
    "MatrixFromFrontUp": 120, "AnimDecodeFrame": 80, "AnimFramePtr": 80,
    "AnimDecodeLerpPtr": 100, "StepToward": 100, "IntSqrt": 120,
    "PlayerAnimateAndDraw": 80, "PlayerSwimSfxUpdate": 48, "PlayerRumbleSlam": 32,
    "PlayerCanActivateGadget": 64, "AreaGetMonkeyRecord": 32, "PlayerHeadLookUpdate": 32,
    "RcCarDraw": 32, "RadarUpdateDraw": 64, "MonkeyFlag248Set": 48,
    "MonkeyRadarSignal": 80,
}
EXTRA_COVERAGE = {
    "AnimDecodeLerp (already-ported callee)": (0x80014FA0, 672),
    "ModelBuildBoneMatrices (already-ported callee)": (0x80015504, 636),
    "AreaGetLevel (already-ported callee)": (0x80010CC0, 28),
    "GetLevelFirstArea (already-ported callee)": (0x8009764C, 24),
}

TOP_HOOKS = {
    "SoundLoopStart": (0x800878E0, 2), "SoundLoopStop": (0x8008806C, 2),
    "PlaySfx": (0x800877F8, 1), "PadRumble": (0x80014168, 3),
    "ModelTransformRigid": (0x800161C0, 3), "ModelTransformSkinned": (0x80016A60, 1),
    "ModelCalcEnvUVs": (0x80016B94, 1), "ModelDraw": (0x80017CD4, 3),
    "ModelEmitPrims": (0x800180B8, 3), "ModelFillVertexColors": (0x80020A20, 5),
    "ModelDrawEnvMap": (0x8001CE54, 4), "ModelEmitPrimsCycleColors": (0x8001D530, 4),
    "DrawShadow": (0x8001F604, 4), "Draw_Nop": (0x8005E000, 2),
    "EffectRingDraw": (0x8006E5B4, 4), "VertexWobble": (0x8001F434, 3),
    "SfxVoiceSetPitch": (0x800A35FC, 2), "SetRotMatrix": (0x8002FB00, 1),
    "SetTransMatrix": (0x8002FB30, 1), "SDK_80031230": (0x80031230, 3),
    "SDK_800302D0": (0x800302D0, 2), "TurnTowardsY": (0x80066C90, 3),
    "SoundEmitterPitch": (0x80087C28, 3), "PlayerStopGadgetSounds": (0x8007A180, 1),
    "RandomRange": (0x800A3740, 3), "SetBackColor": (0x800315E0, 3),
    "SetFarColor": (0x80031620, 3), "SetColorMatrix": (0x80031600, 1),
    "RendererPrimitiveBlock": (0xFFFFFF10, 4), "RendererWobbleGte": (0xFFFFFF14, 4),
}

# Original hooks used by the existing Gadget ports. Ported functions in this task are deliberately
# omitted so the MIPS oracle executes them and the native side calls the direct C ports.
GADGET_HOOKS = [
    ("PlayerCanExitStage", 0x8005DC78, 0), ("IsPlayerSwimming", 0x80068B7C, 0),
    ("RibbonDraw", 0x8006F3D0, 4), ("EffectSpawnFromDef", 0x80070284, 5),
    ("SpawnParticle", 0x800698B8, 2), ("TrailSpawn", 0x8006ED78, 4),
    ("PlaySfx", 0x800877F8, 1), ("CameraGetSlotTarget", 0x8004B54C, 1),
    ("TargetListFindBest", 0x8007CDFC, 3), ("SdkVectorHelper_80031330", 0x80031330, 4),
    ("AimReticleDrawCircle", 0x80085E08, 3), ("PlayerSetState", 0x80064208, 1),
    ("PadRumble", 0x80014168, 3), ("DrawScaledModelAt", 0x8007CAE0, 3),
    ("SdkVectorHelper_80030A30", 0x80030A30, 2), ("SdkVectorHelper_80031830", 0x80031830, 3),
    ("ModelTransformRigid", 0x800161C0, 3), ("SetColorMatrixMode", 0x80015D58, 4),
    ("ModelDraw", 0x80017CD4, 3),
    ("SoundLoopStart", 0x800878E0, 2), ("SoundLoopStop", 0x8008806C, 2),
    ("SoundEmitterVolume", 0x80087BA4, 4), ("MatrixRotYMul", 0x80014750, 2),
    ("SdkVectorHelper_800305B0", 0x800305B0, 2), ("PlayerFxSetMatrixPos", 0x8006CFBC, 2),
    ("RadarDrawModel", 0x8009A574, 4), ("SdkVectorHelper_8002FB00", 0x8002FB00, 1),
    ("SdkVectorHelper_8002FB30", 0x8002FB30, 1), ("SdkVectorHelper_80031230", 0x80031230, 3),
    ("TrailRingPush", 0x8001F8B0, 0), ("TrailRingDraw", 0x8001F954, 0),
    ("DrawRibbonBetween", 0x8002020C, 4), ("ProjectileGroupSpawnSpread", 0x80085630, 3),
    ("AimTableClear3", 0x8007A304, 0), ("GetSolidHeight", 0x8004D8C0, 3),
    ("PlaySfxAt", 0x80087A20, 2), ("GimmickActivate", 0x8008FC30, 2),
    ("MeshInitFromAreaTable", 0x80015CC8, 2), ("AreaEffect", 0x8013D358, 3),
    ("CameraShake", 0x80048A00, 1), ("EventCamStart", 0x8004D39C, 1),
    ("PrintfError", 0x800A3720, 2), ("GimmickTypeToSfx", 0x8005BB98, 1),
    ("PickupCollect", 0x80078A18, 1), ("CameraNudge", 0x80047690, 3),
    ("GpuDrawGadget7Trail", 0xFFFFFF00, 0), ("GpuDrawGadget4Trail", 0xFFFFFF04, 1),
    ("GpuProjectGadget3Aim", 0xFFFFFF08, 0), ("GpuDrawGadget3AimLine", 0xFFFFFF0C, 0),
]
DIRECT_GADGET_HOOKS = {"PlayerCanActivateGadget", "IsPlayerSwimming"}
HOOK_ARITIES = dict(TOP_HOOKS)
for _name, _address, _arity in GADGET_HOOKS:
    if _name not in DIRECT_GADGET_HOOKS:
        HOOK_ARITIES.setdefault(_name, (_address, _arity))

HOOK_ALIASES = {
    "SetRotMatrix": "SdkVectorHelper_8002FB00",
    "SetTransMatrix": "SdkVectorHelper_8002FB30",
    "SDK_80031230": "SdkVectorHelper_80031230",
}
HOOK_POINTER_ARGS = {
    "SpawnParticle": {1}, "CameraGetSlotTarget": {0}, "TargetListFindBest": {1},
    "SdkVectorHelper_80031330": {1, 2, 3}, "EffectSpawnFromDef": {0, 4},
    "TrailSpawn": {0}, "ProjectileGroupSpawnSpread": {1}, "ModelTransformRigid": {1},
    "SdkVectorHelper_80030A30": {1}, "SdkVectorHelper_80031830": {0, 1, 2},
    "SdkVectorHelper_80031230": {0, 1, 2}, "RibbonDraw": {0, 2}, "PlaySfxAt": {1},
    "GpuDrawGadget4Trail": {0},
}
HOOK_POINTER_SIZES = {
    "ModelTransformRigid": {1: 32}, "SdkVectorHelper_80030A30": {1: 32},
    "SdkVectorHelper_80031830": {0: 32, 1: 32, 2: 32},
    # The third vector is an output buffer; the original caller leaves its stack bytes stale.
    "SdkVectorHelper_80031230": {0: 8, 1: 8},
}

P, U32, I32 = C.c_void_p, C.c_uint32, C.c_int32
CALL = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32)
CALL5 = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32, U32)
GADGET_CALL = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32, U32)
COLOR_CALL = C.CFUNCTYPE(None, P, P, U32, U32, U32)
MATRIX_CALL = C.CFUNCTYPE(None, P, P, U32)
TRIG_CALL = C.CFUNCTYPE(I32, P, P, I32)


class AnimHooks(C.Structure):
    _fields_ = [("user", P)] + [
        (name, CALL5 if name == "ModelFillVertexColors" else CALL)
        for name in ("SoundLoopStart SoundLoopStop PlaySfx PadRumble ModelTransformRigid "
                     "ModelTransformSkinned ModelCalcEnvUVs ModelDraw ModelEmitPrims "
                     "ModelFillVertexColors ModelDrawEnvMap ModelEmitPrimsCycleColors "
                     "DrawShadow Draw_Nop EffectRingDraw VertexWobble SfxVoiceSetPitch "
                     "SetRotMatrix SetTransMatrix SDK_80031230 SDK_800302D0 TurnTowardsY "
                     "SoundEmitterPitch IsPlayerSwimming PlayerStopGadgetSounds RandomRange "
                     "RendererPrimitiveBlock RendererWobbleGte").split()
    ] + [(name, P) for name in ("area_hooks gadget_hooks player1_hooks player2_hooks items_hooks").split()]


class Player1Hooks(C.Structure):
    _fields_ = [("user", P)] + [(name, CALL) for name in (
        "CdIntToPos CdPosToInt CdControl CdReadyCallback VSync printf SoundSetVolumeMode "
        "EventCamCreate ModelEmitPrimsCycleColors DrawRibbonBetween ModelProjectVertices ModelEmitPrims"
    ).split()] + [("sound_hw", P)]


class ItemsHooks(C.Structure):
    _fields_ = [("user", P), ("GetLevelFirstArea", CALL), ("printf", CALL),
                ("SetBackColor", COLOR_CALL), ("SetFarColor", COLOR_CALL),
                ("SetColorMatrix", MATRIX_CALL), ("rsin", TRIG_CALL), ("rcos", TRIG_CALL)]


def gadget_hook_fields():
    text = (ROOT / "src/player/ae_gadget.h").read_text(encoding="utf-8")
    block = text[text.index("typedef struct AeGadgetHooks"):]
    block = block[:block.index("} AeGadgetHooks;")]
    return [match.group(1) for match in re.finditer(r"AeGadgetHookFn\s+(\w+);", block)]


GadgetHooks = type("GadgetHooks", (C.Structure,), {
    "_fields_": [("user", P), ("stale_s1", U32)] +
                [(name, GADGET_CALL) for name in gadget_hook_fields()]
})


def signed32(value):
    value &= M32
    return value - 0x100000000 if value & 0x80000000 else value


def build_dll(trapv=False, source=None):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"animdraw_{C.get_errno()}_{time.time_ns()}.dll"
    srcs = [
        "src/core/ae_ram.c", "src/core/ae_sdk.c", "src/core/ae_sdk_matrix.c", "src/core/ae_pose.c",
        "src/player/native_player.c", "src/player/ae_gadget.c", "src/player/ae_player1.c",
        "src/player/ae_player_spawn.c", "src/sound/ae_sound.c",
        "src/camera/ae_camera.c",
        "src/world/ae_items_init.c", "src/world/ae_areaset.c", "src/world/ae_zako_init.c",
        "src/world/ae_world.c", "src/world/ae_solid.c", "src/bodies/ae_bodies.c",
    ]
    flags = ["-O1", "-ftrapv"] if trapv else ["-O2"]
    command = [shutil.which("gcc"), "-std=c11", *flags, "-Wall", "-Wextra", "-Werror",
               "-ffunction-sections", "-fdata-sections", "-shared", "-static-libgcc",
               "-Wl,--gc-sections", "-I", str(ROOT / "src/player"), "-I", str(ROOT / "src/world"),
               *[str(ROOT / path) for path in srcs], str(source or ROOT / "src/player/ae_animdraw.c"),
               "-o", str(dll)]
    subprocess.run(command, check=True)
    return dll


class Memory:
    def __init__(self, cpu=None, lib=None, ram=None):
        self.cpu, self.lib, self.ram = cpu, lib, ram

    def rd8(self, address):
        return self.cpu.read8(address) if self.cpu else self.lib.ae_rd8(self.ram, address)

    def rd16(self, address):
        return self.cpu.read16(address) if self.cpu else self.lib.ae_rd16(self.ram, address)

    def rd32(self, address):
        return self.cpu.read32(address) if self.cpu else self.lib.ae_rd32(self.ram, address)

    def wr8(self, address, value):
        if self.cpu: self.cpu.write8(address, value & 0xFF)
        else: self.lib.ae_wr8(self.ram, address, value & 0xFF)

    def wr16(self, address, value):
        if self.cpu: self.cpu.write16(address, value & 0xFFFF)
        else: self.lib.ae_wr16(self.ram, address, value & 0xFFFF)

    def wr32(self, address, value):
        if self.cpu: self.cpu.write32(address, value & M32)
        else: self.lib.ae_wr32(self.ram, address, value & M32)

    def bytes(self, address, size):
        if self.cpu: return self.cpu.read_bytes(address, size)
        return bytes(self.lib.ae_rds8(self.ram, address + i) & 0xFF for i in range(size))


def mips_lwl(cpu, address, old):
    shift = (address & 3) * 8
    word = cpu.read32(address & ~3)
    return ((old & ((1 << (24 - shift)) - 1)) | ((word << (24 - shift)) & M32)) & M32


def mips_swl(cpu, address, value):
    offset = address & 3
    aligned = address & ~3
    for index in range(offset + 1):
        cpu.write8(aligned + index, value >> ((3 - offset + index) * 8))


class HookModel:
    def __init__(self):
        self.log = []
        self.callback_errors = []

    def invoke(self, name, args, memory):
        arity = HOOK_ARITIES[name][1]
        args = tuple(value & M32 for value in args[:arity])
        canonical = HOOK_ALIASES.get(name, name)
        normalized = list(args)
        pointer_data = []
        for index in HOOK_POINTER_ARGS.get(canonical, ()):
            if STACK_LO <= normalized[index] < STACK_HI:
                size = HOOK_POINTER_SIZES.get(canonical, {}).get(index)
                if size is not None:
                    pointer_data.append((index, memory.bytes(normalized[index], size)))
                normalized[index] = "STACK"
        payload = ()
        if name == "SpawnParticle" and STACK_LO <= args[1] < STACK_HI:
            payload = tuple(memory.rd16(args[1] + offset) for offset in (0, 2, 4))
        self.log.append((canonical, *normalized, payload, tuple(pointer_data)))
        if name == "RandomRange":
            output, lower, limit = args[:3]
            memory.wr32(output, lower + max(0, limit // 2))
        elif name == "RendererPrimitiveBlock":
            cursor, cycle, model, _phase = args
            memory.wr8(cursor + 3, 5)
            memory.wr32(cursor + 4, 0xE100023D)
            memory.wr8(cursor + 11, 0x67)
            memory.wr16(cursor + 12, memory.rd16(model + 0x140) - 15)
            memory.wr16(cursor + 14, memory.rd16(model + 0x142) - 34)
            memory.wr32(cursor + 16, 0x713740A0)
            memory.wr32(cursor + 20, 0x00200020)
            previous = mips_lwl(memory.cpu, cycle + 2, 0xE100023D) if memory.cpu else self._c_lwl(memory, cycle + 2, 0xE100023D)
            if memory.cpu:
                mips_swl(memory.cpu, cycle + 2, (cursor << 8) & M32)
                mips_swl(memory.cpu, cursor + 2, previous)
            else:
                self._c_swl(memory, cycle + 2, (cursor << 8) & M32)
                self._c_swl(memory, cursor + 2, previous)
        if name == "RendererWobbleGte":
            return 0x12340000
        if name == "SoundLoopStart":
            return 7
        return 0

    @staticmethod
    def _c_lwl(memory, address, old):
        shift = (address & 3) * 8
        word = memory.rd32(address & ~3)
        return ((old & ((1 << (24 - shift)) - 1)) | ((word << (24 - shift)) & M32)) & M32

    @staticmethod
    def _c_swl(memory, address, value):
        offset, aligned = address & 3, address & ~3
        for index in range(offset + 1):
            memory.wr8(aligned + index, value >> ((3 - offset + index) * 8))

    def mips_hook(self, name, arity, cpu, regs):
        args = list(regs[4:8])
        if arity > 4:
            args.append(cpu.read32(regs[29] + 16))
        return self.invoke(name, args, Memory(cpu=cpu))

    def mips_packet_patch(self, cpu, regs):
        args = (regs[5], regs[21], regs[19], regs[2])
        self.invoke("RendererPrimitiveBlock", args, Memory(cpu=cpu))
        regs[5] = (regs[5] + 24) & M32
        return 0x80099F98

    def mips_wobble_patch(self, cpu, regs):
        args = (regs[16], (regs[20] + 16) & M32, regs[17], 0)
        regs[6] = self.invoke("RendererWobbleGte", args, Memory(cpu=cpu))
        return 0x80063974

    def mips_gadget3_project_patch(self, cpu, _regs):
        self.invoke("GpuProjectGadget3Aim", (), Memory(cpu=cpu))
        cpu.write16(0x800F5144, 1)
        return 0x8009B058

    def mips_gadget3_line_patch(self, cpu, _regs):
        self.invoke("GpuDrawGadget3AimLine", (), Memory(cpu=cpu))
        return 0x8009B1A8

    def c_hook(self, name, lib, ram, args):
        return self.invoke(name, args, Memory(lib=lib, ram=ram))


class CSide:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        lib = self.lib
        lib.ae_ram_create.restype = P
        lib.ae_ram_destroy.argtypes = [P]
        lib.ae_ptr.argtypes = [P, U32, U32]
        lib.ae_ptr.restype = P
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_failed.restype = C.c_int
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        lib.ae_ram_clear_error.argtypes = [P]
        lib.ae_rd8.argtypes = [P, U32]; lib.ae_rd8.restype = U32
        lib.ae_rd16.argtypes = [P, U32]; lib.ae_rd16.restype = U32
        lib.ae_rd32.argtypes = [P, U32]; lib.ae_rd32.restype = U32
        lib.ae_rds8.argtypes = [P, U32]; lib.ae_rds8.restype = I32
        lib.ae_wr8.argtypes = [P, U32, U32]
        lib.ae_wr16.argtypes = [P, U32, U32]
        lib.ae_wr32.argtypes = [P, U32, U32]
        self.ram = lib.ae_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.anim_hooks = AnimHooks()
        self.gadget_hooks = GadgetHooks()
        self.player1_hooks = Player1Hooks()
        self.items_hooks = ItemsHooks()
        self.callback_refs = []
        self.model = None
        self._bind_callbacks()
        self._bind_functions()

    def _keep(self, callback):
        self.callback_refs.append(callback)
        return callback

    def _bind_callbacks(self):
        self.anim_hooks.user = 0
        for field, _ in TOP_HOOKS.items():
            if not hasattr(self.anim_hooks, field):
                continue
            if field == "ModelFillVertexColors":
                def callback(_user, ram, a0, a1, a2, a3, a4, name=field):
                    return self.model.c_hook(name, self.lib, ram, (a0, a1, a2, a3, a4))
                cb = self._keep(CALL5(callback))
            elif field in ("SetBackColor", "SetColorMatrix"):
                continue
            else:
                def callback(_user, ram, a0, a1, a2, a3, name=field):
                    return self.model.c_hook(name, self.lib, ram, (a0, a1, a2, a3))
                cb = self._keep(CALL(callback))
            setattr(self.anim_hooks, field, cb)

        for name in gadget_hook_fields():
            if name in DIRECT_GADGET_HOOKS:
                continue
            if name not in HOOK_ARITIES:
                continue
            def callback(_user, ram, a0, a1, a2, a3, a4, hook_name=name):
                return self.model.c_hook(hook_name, self.lib, ram, (a0, a1, a2, a3, a4))
            setattr(self.gadget_hooks, name, self._keep(GADGET_CALL(callback)))

        def direct_can_activate(_user, ram, gadget, _a1, _a2, _a3, _a4):
            return self.lib.ae_PlayerCanActivateGadget(ram, gadget)
        self.gadget_hooks.PlayerCanActivateGadget = self._keep(GADGET_CALL(direct_can_activate))

        def direct_is_swimming(_user, ram, _a0, _a1, _a2, _a3, _a4):
            state = self.lib.ae_rd16(ram, 0x800EC23E)
            return self.lib.ae_player_is_swimming(state)
        self.gadget_hooks.IsPlayerSwimming = self._keep(GADGET_CALL(direct_is_swimming))

        def direct_set_color_matrix_mode(_user, ram, mode, red, green, blue, _a4):
            self.lib.ae_SetColorMatrixMode(ram, C.byref(self.items_hooks),
                                           mode, red, green, blue)
            return 0
        self.gadget_hooks.SetColorMatrixMode = self._keep(
            GADGET_CALL(direct_set_color_matrix_mode))

        for name in ("ModelEmitPrimsCycleColors", "ModelEmitPrims", "DrawRibbonBetween"):
            def callback(_user, ram, a0, a1, a2, a3, hook_name=name):
                return self.model.c_hook(hook_name, self.lib, ram, (a0, a1, a2, a3))
            setattr(self.player1_hooks, name, self._keep(CALL(callback)))

        def color_callback(_user, ram, a0, a1, a2, name="SetBackColor"):
            return self.model.c_hook(name, self.lib, ram, (a0, a1, a2))
        self.items_hooks.SetBackColor = self._keep(COLOR_CALL(color_callback))
        self.items_hooks.SetFarColor = self._keep(COLOR_CALL(
            lambda _user, ram, a0, a1, a2: self.model.c_hook("SetFarColor", self.lib, ram, (a0, a1, a2))))
        self.items_hooks.SetColorMatrix = self._keep(MATRIX_CALL(
            lambda _user, ram, matrix: self.model.c_hook("SetColorMatrix", self.lib, ram, (matrix,))))
        self.anim_hooks.gadget_hooks = C.cast(C.pointer(self.gadget_hooks), P)
        self.anim_hooks.player1_hooks = C.cast(C.pointer(self.player1_hooks), P)
        self.anim_hooks.items_hooks = C.cast(C.pointer(self.items_hooks), P)
        self.anim_hooks.area_hooks = None
        self.anim_hooks.player2_hooks = None

    def _bind_functions(self):
        for name, (_, _, symbol, kinds, returns) in FUNCS.items():
            fn = getattr(self.lib, symbol)
            argtypes = []
            for kind in kinds:
                argtypes.append({"u": U32, "s": I32, "h": P, "a": P}[kind])
            fn.argtypes = [P] + argtypes
            fn.restype = I32 if returns else None
        self.lib.ae_player_is_swimming.argtypes = [C.c_uint16]
        self.lib.ae_player_is_swimming.restype = C.c_int
        self.lib.ae_SetColorMatrixMode.argtypes = [P, P, U32, U32, U32, U32]
        self.lib.ae_SetColorMatrixMode.restype = None

    def sync(self, m):
        C.memmove(self.main, bytes(m.ram), 0x200000)
        C.memmove(self.scratch, bytes(m.scratch), 0x400)
        self.lib.ae_ram_clear_error(self.ram)

    def call(self, name, args):
        symbol = FUNCS[name][2]
        kinds = FUNCS[name][3]
        converted = []
        values = iter(args)
        for kind in kinds:
            if kind == "h": converted.append(C.byref(self.anim_hooks))
            elif kind == "a": converted.append(None)
            else:
                value = next(values)
                converted.append(signed32(value) if kind == "s" else value & M32)
        return getattr(self.lib, symbol)(self.ram, *converted)

    def failed(self):
        return bool(self.lib.ae_ram_failed(self.ram))

    def error(self):
        message = self.lib.ae_ram_error(self.ram)
        return message.decode("utf-8", "replace") if message else ""


def set16(m, address, value): m.write16(address, value & 0xFFFF)
def set32(m, address, value): m.write32(address, value & M32)
def set8(m, address, value): m.write8(address, value & 0xFF)


def setup_actor(m, rng, actor=ACTOR, bones=3, model=MODEL, animation=ANIMATION, pose=POSE, matrices=MATRIX):
    m.write32(actor, model)
    m.write32(actor + 0x0C, pose)
    m.write32(actor + 0x14, matrices)
    m.write32(actor + 0x18, animation)
    m.write16(model + 4, bones)
    m.write16(actor + 0x5C, rng.randrange(8))
    m.write16(actor + 0x8A, rng.randrange(8))
    m.write32(actor + 0x90, animation + 3 * (bones + 2) * 4)
    frame_stride = (bones + 2) * 4
    for frame in range(12):
        m.write32(animation + frame * frame_stride, rng.getrandbits(32))
        m.write32(animation + frame * frame_stride + 4, rng.getrandbits(32))
        for bone in range(bones):
            m.write32(animation + frame * frame_stride + 8 + bone * 4, rng.getrandbits(32))
    m.write_bytes(ANIM_ANGLES, rng.randbytes(0x100))
    for bone in range(max(1, bones)):
        m.write_bytes(pose + bone * 12, rng.randbytes(12))
        m.write16(pose + bone * 12 + 10, max(0, bone - 1))
    m.write_bytes(matrices, rng.randbytes(max(0x40, bones * 0x20)))
    return frame_stride


def setup_player(m, rng, case):
    bones = (1, 2, 3, 5)[case % 4]
    setup_actor(m, rng, actor=PLAYER, bones=bones, model=MODEL, animation=ANIMATION,
                pose=POSE, matrices=MATRIX)
    swim_states = {1: 0x47, 2: 0x49, 3: 0x4C, 4: 0x4A, 20: 0x46}
    player_state = swim_states.get(case, 15 if case % 10 in (5, 7) else
                                   0 if case % 10 == 9 else 14)
    m.write16(PLAYER + 0x5E, player_state)
    m.write16(PLAYER + 0xE8, 1 + (case % 3))
    m.write16(PLAYER + 0x8C, (0, 0xFFFF, 0xFFFE, 1024, 2048, 3072, 4095, 4096)[case % 8])
    m.write32(PLAYER + 0x70, (0, 0x20, 0x10, 0x40)[case % 4])
    set32(m, PLAYER + 0xE0, 1 if case == 7 else 0)
    if case == 7:
        m.write_bytes(PLAYER + 0x94, rng.randbytes(32))
    m.write16(PLAYER + 0xFE, 5 if case % 3 == 0 else 0)
    m.write16(PLAYER + 0x102, 1 if case % 5 == 0 else 0)
    m.write16(PLAYER + 0x138, 0x1000 if case % 7 == 0 else 0)
    m.write16(PLAYER + 0x13C, rng.randrange(-2048, 2048))
    m.write16(PLAYER + 0x136, 1)
    set16(m, AREA_STATE + 0x150, 12 if case % 19 == 0 else (6 if case % 11 == 0 else 0))
    m.write16(AREA_STATE + 0x152, case % 24)
    m.write32(AREA_STATE + 0x27C, 0 if case % 7 == 0 else 0x1000)
    m.write8(0x800E68D0 + 0x32, 1 if case % 6 == 0 else 0)
    m.write8(0x800E68D0 + 0x46, 1 if case % 13 == 0 else 0)
    set16(m, 0x800F4DCE, 1 if case % 17 == 0 else (2 if case % 17 == 1 else 0))
    m.write8(0x800EC2D2, 10 if case == 18 else case % 10)
    m.write8(0x800E6902, 1 if case % 9 == 0 else 0)
    m.write8(0x800E53C2, rng.randrange(-8, 9))
    m.write8(0x800E53C3, rng.randrange(-8, 9))
    m.write32(0x800EC060 + 8 * 0x10, MODEL)
    m.write16(MODEL + 6, 1)
    for axis in range(3):
        m.write16(PLAYER + 0x80 + axis * 2, (4096, 0, 0)[axis])
        m.write16(PLAYER + 0x68 + axis * 2, (0, 4096, 0)[axis])
        m.write32(PLAYER + 0x24 + axis * 4, rng.randrange(-3000, 3000))
    m.write32(0x800EC350, rng.getrandbits(32))
    m.write32(0x800EC35C, case % 4)
    m.write16(0x800F51D0 + 0x18, -1)
    m.write16(0x800F51D0 + 0x1A, -1)
    m.write32(0x800F51E0, 0)
    m.write32(0x800EC110, MODEL)
    m.write32(0x800E7D00, MODEL)
    m.write32(0x800E9C60, MATRIX)
    for offset in range(0, 0x100, 4):
        m.write32(0x800EC060 + offset, MODEL)
    # Exercise the color transition, swimming sound/ring branches, and the stage-gadget exit.
    m.write_bytes(PLAYER + 0xF4, bytes((20, 40, 60, 0)))
    m.write_bytes(PLAYER + 0xF8, bytes((80, 100, 120, 0)))
    set32(m, 0x800E5478, 0x400 if case == 2 else 0)
    set32(m, 0x800E5480, 0x400)
    set32(m, 0x800F51E0, 1 if case == 1 else 0)
    if case in (5, 9):
        set16(m, 0x800F51D0 + 0x18, 7)
        set16(m, 0x800F51D0 + 0x1A, 9)
    if case == 24:
        set8(m, 0x800E68D0 + 0x32, 1)
        set8(m, 0x800E68D0 + 0x46, 0)
        set16(m, 0x800F5124, 1)
    if case == 18:
        set32(m, 0x800EC060 + 10 * 0x10, MODEL)


def cases(name, count, seed):
    rng = random.Random(seed ^ sum(name.encode("ascii")))
    if name == "StepToward":
        edge = [(0, 0, 1), (1, 0, 1), (-1, 0, 1), (100, 0, 100), (-100, 0, 100),
                (0x7FFFFFFF, -0x80000000, 0x7FFFFFFF), (-0x80000000, 0x7FFFFFFF, 0x80000000),
                (12, 15, 0), (4, 3, -1), (-4, 3, 2)]
        for i in range(count):
            args = edge[i] if i < len(edge) else tuple(rng.randint(-0x80000000, 0x7FFFFFFF) for _ in range(3))
            yield f"#{i} {args}", lambda _m: None, args
    elif name == "IntSqrt":
        edge = [0, 1, 2, 3, 4, 15, 16, 17, 0x7FFFFFFF, -1, -2, -0x80000000]
        for i in range(count):
            value = edge[i] if i < len(edge) else rng.randint(-0x80000000, 0x7FFFFFFF)
            yield f"#{i} n={value}", lambda _m: None, (value,)
    elif name == "MatrixFromFrontUp":
        vectors = [(4096, 0, 0), (0, 4096, 0), (0, 0, 4096), (4096, 0, 4096),
                   (-4096, 0, 4096), (0, 0, 0), (8192, -8192, 4096), (-4096, -4096, -4096)]
        for i in range(count):
            front = vectors[i % len(vectors)] if i < 16 else tuple(rng.randint(-8192, 8192) for _ in range(3))
            up = vectors[(i + 3) % len(vectors)] if i < 16 else tuple(rng.randint(-8192, 8192) for _ in range(3))
            mode = (0, 1, 2, 0xFFFFFFFF)[i % 4]
            def setup(m, f=front, u=up, randomizer=rng):
                for base, values in ((FRONT, f), (UP, u)):
                    for axis, value in enumerate(values): set16(m, base + axis * 2, value)
                m.write_bytes(MATRIX, randomizer.randbytes(32))
            yield f"#{i} front={front} up={up} mode={mode:08X}", setup, (FRONT, UP, MATRIX, mode)
    elif name in ("AnimDecodeFrame", "AnimFramePtr", "AnimDecodeLerpPtr"):
        for i in range(count):
            bones, frame = (0, 1, 3, 8, 17)[i % 5], i % 8
            fraction = (0, 1, 1024, 2048, 3072, 4095, 4096, 0xFFFFFFFF)[i % 8] if i < 24 else rng.getrandbits(32)
            frame_a = ANIMATION + 3 * (bones + 2) * 4
            frame_b = ANIMATION + 4 * (bones + 2) * 4
            def setup(m, n=bones, f=frame, randomizer=rng):
                setup_actor(m, randomizer, bones=n)
                m.write16(ACTOR + 0x5C, f)
            args = ((ACTOR,) if name == "AnimDecodeFrame" else
                    (ACTOR, frame) if name == "AnimFramePtr" else (ACTOR, frame_a, frame_b, fraction))
            yield f"#{i} bones={bones} frame={frame} t={fraction:08X}", setup, args
    elif name == "PlayerCanActivateGadget":
        for i in range(count):
            def setup(m, k=i):
                m.write16(PLAYER + 0xE8, (0, 1, 2, -1)[k % 4])
                m.write16(AREA_STATE + 0x150, (0, 5, 6, 12, 0)[k % 5])
                m.write16(AREA_STATE + 0x152, (0, 11, 1)[k % 3])
                m.write16(PLAYER + 0x5E, (0, 14, 15, 27, 30, 33, 47, 48, 66, 67, 70)[k % 11])
            yield f"#{i}", setup, (2,)
    elif name == "AreaGetMonkeyRecord":
        for i in range(count):
            area = (0, 1, 2, 5, 10, 15, 19, 22, 24, 29, 35, 47, 63)[i % 13]
            def setup(m, k=i):
                set32(m, 0x80185000, 0xDEADBEEF)
                selected_area = area
                if k % 4 == 0: set16(m, 0x800A81B4 + selected_area * 8, 32)
                elif k % 4 == 1: set16(m, 0x800A81B4 + selected_area * 8, 31)
                elif k % 4 == 2: set16(m, 0x800A81B4 + selected_area * 8, 24)
            yield f"#{i} area={area}", setup, (area, 0x80185000)
    elif name == "MonkeyFlag248Set":
        for i in range(count):
            monkey = 0x80190000
            def setup(m, k=i):
                set8(m, monkey + 0x7A, 4 if k % 3 else 2)
                set32(m, monkey + 0x248, (1 << 18) if k % 2 else 0)
            yield f"#{i}", setup, (monkey,)
    elif name == "PlayerSwimSfxUpdate":
        for i in range(count):
            def setup(m, k=i):
                set16(m, PLAYER + 0x5E, (71, 71, 0, 0, 0)[k % 5])
                set16(m, AREA_STATE + 0x152, 11 if k % 9 == 0 else 0)
                set32(m, 0x800E5478, (0, 2, 8, 10)[k % 4])
                velocities = ((0, 0, 0), (0, 0, 0), (-3, -3, -3),
                              (0, 9, -3), (0, 0, 9))
                for offset, value in zip((0x30, 0x34, 0x38), velocities[k % 5]):
                    set32(m, PLAYER + offset, value)
                set16(m, 0x800F51D0 + 0x18, (-1, 7, -1, 9)[k % 4])
                set16(m, 0x800F51D0 + 0x1A, (-1, 115, 63, 0xFFFF)[(k + 1) % 4])
            yield f"#{i}", setup, ()
    elif name == "PlayerRumbleSlam":
        for i in range(count):
            def setup(m, k=i, randomizer=rng):
                for offset in (0x1F4, 0x1F8, 0x1FC, 0x214, 0x218, 0x21C):
                    set32(m, 0x800F4730 + offset, randomizer.getrandbits(32))
                for offset in (0x80, 0x84): set32(m, PLAYER + offset, randomizer.getrandbits(32))
            yield f"#{i}", setup, ()
    elif name == "PlayerHeadLookUpdate":
        for i in range(count):
            def setup(m, k=i, randomizer=rng):
                set32(m, 0x800E07D0 + 0x14, MATRIX)
                for j in range(8): set32(m, 0x800A6830 + j * 4, randomizer.getrandbits(32))
                for j in range(3): set16(m, 0x800F5180 + j * 2, randomizer.randrange(-4096, 4096))
                set32(m, 0x800F4910, 0x80185000)
                set32(m, 0x800E78C0, 0x80185000)
                m.write_bytes(MATRIX, randomizer.randbytes(0xA0))
                m.write_bytes(0x800E9E40, randomizer.randbytes(0x80))
            yield f"#{i}", setup, ()
    elif name == "RcCarDraw":
        for i in range(count):
            def setup(m, k=i, randomizer=rng):
                set32(m, 0x800F5A20 + 0x70, 1)
                set32(m, 0x800F5A20, 0)
                for j in range(3):
                    set16(m, 0x800F5A20 + 0x60 + j * 2, (4096, 0, 0)[j])
                    set16(m, 0x800F5A20 + 0x68 + j * 2, (0, 4096, 0)[j])
                    set32(m, 0x800F5A20 + 0x24 + j * 4, randomizer.randrange(-2000, 2000))
                set32(m, 0x800EC110, MODEL)
                set16(m, MODEL + 4, 3)
                set16(m, MODEL + 6, 1)
                set16(m, 0x800F5A20 + 0x78, 1 if k % 3 == 0 else 0)
                set8(m, 0x800EC2D2, 7 if k % 2 else 3)
                set32(m, 0x800EC35C, k % 4)
                set32(m, 0x800BAD90, 0)
                set32(m, 0x800F5120, 0)
            yield f"#{i}", setup, ()
    elif name == "MonkeyRadarSignal":
        for i in range(count):
            def setup(m, k=i, randomizer=rng):
                set16(m, PLAYER + 0x24, 0); set16(m, PLAYER + 0x2C, 0)
                set16(m, PLAYER + 0x60, 0); set16(m, PLAYER + 0x64, 4096)
                set16(m, 0x800F5124, 0); set16(m, 0x800F5128, 4096)
                set32(m, 0x800F5120 + 0x1C, 0)
                set32(m, 0x800F5120 + 0x18, 0)
                active = k % 4 not in (0, 1)
                for slot in range(8):
                    monkey = MONKEYS + slot * 0x250
                    kind = (4 if k % 3 == 0 else 1) if slot == 0 and active else 0
                    set8(m, monkey + 0x7A, kind)
                    set32(m, monkey + 0x248, (1 << 18) if kind == 4 else 0)
                    set16(m, monkey + 0x24, 0)
                    z = -100 if (k % 10 == 3 and slot == 0) else 100 + (k % 10) * 60
                    set16(m, monkey + 0x2C, z if slot == 0 else 0)
                set32(m, 0x800F4588, 0x80186000)
                set32(m, 0x800F45DC, 1 if k % 4 == 0 else 0)
                set32(m, 0x80186000 + 0x20, 0)
                set16(m, 0x80186000 + 0x04, 0)
                set16(m, 0x80186000 + 0x0C, 200)
                set16(m, 0x800A81B4, (32, 31, 24, 1)[(k // 4) % 4])
                set16(m, 0x800B3858 + 2, 0)
                set16(m, 0x800DFFE0, 8)
                for j in range(8):
                    set8(m, 0x800DFE00 + j, 2 if k % 32 == 28 else 0)
            yield f"#{i}", setup, ()
    elif name == "RadarUpdateDraw":
        for i in range(count):
            def setup(m, k=i, randomizer=rng):
                setup_player(m, randomizer, k)
                set16(m, PLAYER + 0xE8, 10 if k % 17 else 0)
                set16(m, PLAYER + 0x5E, 0)
                set16(m, PLAYER + 0x24, 0)
                set16(m, PLAYER + 0x2C, 0)
                set16(m, PLAYER + 0x60, 0)
                set16(m, PLAYER + 0x64, 4096)
                set16(m, 0x800F5124, 0)
                set16(m, 0x800F5128, 4096)
                set16(m, 0x800E5320 + 0x17A, 1 if k % 5 else 0)
                set16(m, 0x800EC2C8, 10 if k % 7 else 0)
                set32(m, AREA_STATE + 0x100, 1 if k % 13 == 0 else 0)
                set32(m, 0x800EC350, k * 29 + 3)
                set32(m, 0x800EC35C, k % 4)
                set32(m, 0x800EC170 + 8, 0x80187000)
                set32(m, 0x800EC354, 0x80187000)
                set16(m, 0x800F5120 + 0x14, -1 if k % 4 == 0 else 7)
                set16(m, 0x800F5120 + 0x16, -1 if k % 3 == 0 else 8)
                for slot in range(8):
                    monkey = MONKEYS + slot * 0x250
                    kind = (4 if k % 4 == 1 else 1) if slot == 0 and k % 2 else 0
                    set8(m, monkey + 0x7A, kind)
                    set32(m, monkey + 0x248, (1 << 18) if kind == 4 else 0)
                    set16(m, monkey + 0x24, 0)
                    signal_distance = 2048 if k % 8 == 7 else 100 + (k % 8) * 70
                    set16(m, monkey + 0x2C, signal_distance if slot == 0 else 0)
                set32(m, 0x800F4588, 0x80186000)
                set32(m, 0x800F45DC, 0)
            yield f"#{i}", setup, ()
    elif name == "PlayerAnimateAndDraw":
        for i in range(count):
            def setup(m, k=i, randomizer=rng): setup_player(m, randomizer, k)
            yield f"#{i} gadget={i % 10}", setup, ()


class Harness:
    def __init__(self, dll, coverage=True, gte_lib=None):
        self.m = mr.MipsRam.from_extract()
        if gte_lib is None:
            self.m.attach_gte_oracle()
        else:
            self._attach_shared_gte_oracle(self.m, gte_lib)
        self.m.coverage = set() if coverage else None
        self.base = self.m.snapshot()
        self.area_blobs = []
        for name in AREAS:
            path = ROOT / "extract/members" / name
            if not path.is_file(): raise SystemExit(f"required real area member is missing: {path}")
            self.area_blobs.append(path.read_bytes())
        reg.declare("stack_window", "801FF000..80200000")
        reg.declare("unreachable", "RadarUpdateDraw 80099F3C..80099F94")
        reg.declare("unreachable", "PlayerAnimateAndDraw 80063958..80063970")
        self.c = CSide(dll)
        self.failures = 0
        self.counts = {}
        self.cases_seen = 0
        self.start_area = 0
        self.hook_failures = []
        self.mips_hook_map = {}
        for name, (address, arity) in TOP_HOOKS.items():
            if address not in (0xFFFFFF10, 0xFFFFFF14):
                self.mips_hook_map.setdefault(address, (name, arity))
        for name, address, arity in GADGET_HOOKS:
            # Let the MIPS oracle execute the already-ported SetColorMatrixMode body.
            if (address < 0xFFFFFF00 and name not in DIRECT_GADGET_HOOKS and
                    name != "SetColorMatrixMode"):
                self.mips_hook_map.setdefault(address, (name, arity))

    @staticmethod
    def _attach_shared_gte_oracle(cpu, lib):
        state = (C.c_uint32 * 64)()
        cpu._gte_lib = lib
        cpu.gte_state = state
        backend = lambda instruction, value: lib.oracle_gte_step(
            state, instruction, value & M32)
        backend._native_function = lib.oracle_gte_step
        backend._native_state = state
        cpu.gte_backend = backend

    def run(self, name, label, setup, args, area_index):
        self.m.restore(self.base)
        self.m.gte_reset()
        self.m.load_area(self.area_blobs[area_index % len(self.area_blobs)])
        setup(self.m)
        self.c.sync(self.m)
        mips_model = HookModel()
        c_model = HookModel()
        self.c.model = c_model
        self.counts[name] = self.counts.get(name, 0) + 1
        self.cases_seen += 1
        hooks = {address: (lambda cpu, regs, key=hook_name, n=arity:
                           mips_model.mips_hook(key, n, cpu, regs))
                 for address, (hook_name, arity) in self.mips_hook_map.items()}
        patches = {
            0x80099F3C: mips_model.mips_packet_patch,
            0x80063958: mips_model.mips_wobble_patch,
            0x8009AE9C: mips_model.mips_gadget3_project_patch,
            0x8009B094: mips_model.mips_gadget3_line_patch,
        }
        original_error = None
        original = None
        try:
            regs = self.m.call(FUNCS[name][0], tuple(value & M32 for value in args),
                               hooks=hooks, patches=patches)
            original = regs[2]
        except mr.MipsError as error:
            original_error = str(error)
        try:
            native = self.c.call(name, args)
        except Exception as error:  # callback exceptions are checker failures
            self.fail(name, label, f"C call raised {error!r}")
            return
        if original_error:
            self.fail(name, label, f"MIPS failed: {original_error}")
            return
        if self.c.failed():
            self.fail(name, label, f"C failed closed: {self.c.error()}")
            return
        if FUNCS[name][4] and (original & M32) != (native & M32):
            self.fail(name, label, f"return differs: MIPS {original & M32:08X}, C {native & M32:08X}")
            return
        if mips_model.log != c_model.log:
            self.fail(name, label, f"hook calls differ: MIPS {mips_model.log!r}; C {c_model.log!r}")
            return
        end = STACK_LO - 0x80000000
        original_ram, native_ram = bytes(self.m.ram)[:end], bytes(self.c.main)[:end]
        if original_ram != native_ram:
            differences = [i for i, (a, b) in enumerate(zip(original_ram, native_ram)) if a != b]
            self.fail(name, label, "RAM differs at " + ", ".join(
                f"{0x80000000 + off:08X}(MIPS {original_ram[off]:02X}, C {native_ram[off]:02X})"
                for off in differences[:8]))
            return
        if bytes(self.m.scratch) != bytes(self.c.scratch):
            a, b = bytes(self.m.scratch), bytes(self.c.scratch)
            differences = [i for i, (x, y) in enumerate(zip(a, b)) if x != y]
            self.fail(name, label, "scratchpad differs at " + ", ".join(
                f"{0x1F800000 + off:08X}(MIPS {a[off]:02X}, C {b[off]:02X})" for off in differences[:8]))

    def fail(self, name, label, message):
        self.failures += 1
        print(f"FAIL {name} [{label}] {message}")

    def finish(self, default_run=False, show_coverage=False, function_names=None):
        summary = None
        if self.m.coverage is not None:
            names = function_names or list(FUNCS)
            items = [(name, *FUNCS[name][:2], True) for name in names]
            items.extend((name, address, size, False) for name, (address, size) in EXTRA_COVERAGE.items())
            summary = reg.coverage_summary(items, self.m.coverage)
            if show_coverage: reg.print_coverage(summary)
        if default_run:
            reg.write_results(self.counts, summary, not self.failures, function_names=list(FUNCS))
        return summary


MUTATION_SELECTOR = 0x800B4DF0
MUTATION_MARKER = 0x800B4DF4


def mutate_function(source, symbol):
    pattern = re.compile(r"(\b" + re.escape(symbol) + r"\s*\([^;]*?\)\s*\{)", re.S)
    injection = (
        r"\1\n    uint32_t mutation_selector = ae_rd32(r, 0x800B4DF0u);"
        r"\n    if (mutation_selector == 1u) ae_wr32(r, 0x800B4DF4u, 0xA11CE001u);"
        r"\n    else if (mutation_selector == 2u) ae_wr16(r, 0x800B4DF8u, 0xA11Cu);"
    )
    mutated, substitutions = pattern.subn(injection, source, count=1)
    if substitutions != 1:
        raise RuntimeError(f"could not inject mutations into {symbol}")
    return mutated


def run_mutations(seed, gte_lib):
    original = (ROOT / "src/player/ae_animdraw.c").read_text(encoding="utf-8")
    caught = 0
    total = len(FUNCS) * 2
    for name, (_, _, symbol, _, _) in FUNCS.items():
        mutant = mutate_function(original, symbol)
        mutant_path = ROOT / "extract/mips" / f"animdraw_mut_{name}.c"
        mutant_path.write_text(mutant, encoding="utf-8")
        dll = build_dll(source=mutant_path)
        h = Harness(dll, coverage=False, gte_lib=gte_lib)
        fixture = next(cases(name, 1, seed ^ 0xA17C))
        label, setup, args = fixture
        for selector in (1, 2):
            def mutation_setup(m, base_setup=setup, selected=selector):
                base_setup(m)
                set32(m, MUTATION_SELECTOR, selected)
            before = h.failures
            h.run(name, f"mutation {selector} {label}", mutation_setup, args, h.cases_seen)
            if h.failures > before:
                caught += 1
                print(f"caught mutation {caught:02d}/{total}: {name} variant {selector}")
            else:
                print(f"MUTATION SURVIVED: {name} variant {selector}")
        h.c.lib.ae_ram_destroy(h.c.ram)
    print(f"mutation result: {caught}/{total} caught")
    return 0 if caught == total else 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=float, default=100)
    parser.add_argument("--seed", type=lambda value: int(value, 0), default=DEFAULT_SEED)
    parser.add_argument("--only", nargs="*")
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--trapv", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    args = parser.parse_args()
    if not (ROOT / "extract/disc/SCUS_944.23").exists():
        print("SKIP: extract/disc/SCUS_944.23 not present")
        return 0
    selected = set(args.only or FUNCS)
    unknown = selected - set(FUNCS)
    if unknown: raise SystemExit("unknown function name(s): " + ", ".join(sorted(unknown)))
    harness = Harness(build_dll(args.trapv), coverage=True)
    started = time.time()
    for name, default_count in BASE_CASES.items():
        if name not in selected: continue
        scaled = max(1, round(default_count * args.cases / 100.0))
        for index, (label, setup, call_args) in enumerate(cases(name, scaled, args.seed)):
            harness.run(name, label, setup, call_args, harness.cases_seen)
    default_run = (not args.only and args.cases == 100 and args.seed == DEFAULT_SEED and
                   not args.trapv and not args.mutate)
    summary = harness.finish(default_run=default_run, show_coverage=args.coverage,
                             function_names=[name for name in FUNCS if name in selected])
    if summary:
        gaps = [(name, data["uncovered"], data["stale_declared"]) for name, data in summary.items()
                if data["own"] and (data["uncovered"] or data["stale_declared"])]
        if gaps:
            for name, uncovered, stale in gaps:
                if uncovered: print(f"COVERAGE GAP {name}: {uncovered}")
                if stale: print(f"STALE EXCLUSION {name}: {stale}")
            harness.failures += len(gaps)
    print("cases per function:")
    for name, count in sorted(harness.counts.items()): print(f"  {name:24s} {count}")
    print(f"total cases {harness.cases_seen}; MIPS instructions {harness.m.steps}; {time.time() - started:.1f}s")
    if harness.failures:
        print(f"FAILED: {harness.failures} mismatch(es)")
        return 1
    print("OK: animation, player draw, RC car, and Monkey Radar ports match the original instructions")
    if args.mutate:
        return run_mutations(args.seed, harness.m._gte_lib)
    return 0


if __name__ == "__main__":
    sys.exit(main())

