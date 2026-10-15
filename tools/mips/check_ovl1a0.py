"""Differential checks for the readable Area 46 overlay member 1A0 ports.

The checker loads the real area member 0F7.bin, overlays 1A0.bin at
0x8013D000, and compares each original MIPS function with its AeRam port.
Hardware rendering and non-ported EXE calls cross named hooks on both sides.
"""
import argparse
import ctypes as C
import random
import re
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import check_game  # noqa: E402
import check_zako1 as z1  # noqa: E402
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

U32, I32, U16, P = C.c_uint32, C.c_int32, C.c_uint16, C.c_void_p
STACK_LO, STACK_HI = 0x801FF000, 0x80200000
MUTATION_SELECTOR, MUTATION_MARKER = 0x801FEF00, 0x801FEF04
OVERLAY_BASE = 0x8013D000
MUTATION_MAGIC = 0xA10F0000

# name: (address, byte size, exported C symbol, return kind, C args, uses hooks)
FUNCS = {
    "Area46_Boss_RegisterEntry": (0x8013D000, 0x6C, "ae_Area46_Boss_RegisterEntry", "s", "u", 0),
    "Area46_Boss_UpdateEncounterActor": (0x8013D06C, 0x234, "ae_Area46_Boss_UpdateEncounterActor", "v", "u", 1),
    "Area46_Boss_UpdateEntryProximity": (0x8013D2A0, 0x14C, "ae_Area46_Boss_UpdateEntryProximity", "s", "u", 1),
    "Area46_Boss_InitializeModelEntry": (0x8013D440, 0xA0, "ae_Area46_Boss_InitializeModelEntry", "v", "uuuu", 1),
    "Area46_Boss_ProjectAndEmit": (0x8013D63C, 0x140, "ae_Area46_Boss_ProjectAndEmit", "s", "uuu", 1),
    "Area46_Boss_SpawnEffectNear": (0x8013D77C, 0x114, "ae_Area46_Boss_SpawnEffectNear", "v", "uuiii", 1),
    "Area46_Boss_SpawnEffectNearWrap": (0x8013D890, 0x2C, "ae_Area46_Boss_SpawnEffectNearWrap", "v", "uuii", 1),
    "Area46_Boss_ClearDebris": (0x8013D8BC, 0x28, "ae_Area46_Boss_ClearDebris", "v", "", 0),
    "Area46_Boss_SpawnDebris": (0x8013D8E4, 0x168, "ae_Area46_Boss_SpawnDebris", "v", "u", 0),
    "Area46_Boss_UpdateDebris": (0x8013DA4C, 0x3D0, "ae_Area46_Boss_UpdateDebris", "v", "", 1),
    "Area46_Boss_RumbleLong": (0x8013DE1C, 0x28, "ae_Area46_Boss_RumbleLong", "v", "", 1),
    "Area46_Boss_RumbleShort": (0x8013DE44, 0x28, "ae_Area46_Boss_RumbleShort", "v", "", 1),
    "Area46_Boss_RumbleMedium": (0x8013DE6C, 0x28, "ae_Area46_Boss_RumbleMedium", "v", "", 1),
    "Area46_Boss_SpawnDropPickups": (0x8013DE94, 0x94, "ae_Area46_Boss_SpawnDropPickups", "v", "u", 1),
    "Area46_Boss_ResetLoopHandle": (0x8013DF28, 0x10, "ae_Area46_Boss_ResetLoopHandle", "v", "", 0),
    "Area46_Boss_StartLoop": (0x8013DF38, 0x28, "ae_Area46_Boss_StartLoop", "s", "i", 1),
    "Area46_Boss_LoopStop": (0x8013DF60, 0x30, "ae_Area46_Boss_LoopStop", "v", "", 1),
    "Area46_BossInit": (0x8013DF90, 0x1BC, "ae_Area46_BossInit", "v", "", 1),
    "Area46_Boss_InitializeActorRecords": (0x8013E14C, 0x60, "ae_Area46_Boss_InitializeActorRecords", "v", "u", 1),
    "Area46_Boss_ClearEntries": (0x8013E1AC, 0x24, "ae_Area46_Boss_ClearEntries", "v", "", 0),
    "Area46_Boss_AddEntry": (0x8013E1D0, 0xEC, "ae_Area46_Boss_AddEntry", "v", "uu", 0),
    "Area46_Boss_UpdateEntries": (0x8013E2BC, 0x9A0, "ae_Area46_Boss_UpdateEntries", "v", "", 1),
    "Area46_Boss_InitializeBackdrop": (0x8013EC5C, 0x38, "ae_Area46_Boss_InitializeBackdrop", "v", "u", 1),
    "Area46_Boss_SetBackdrop": (0x8013EC94, 0x40, "ae_Area46_Boss_SetBackdrop", "v", "iii", 0),
    "Area46_Boss_UpdateBackdrop": (0x8013ECD4, 0x1A0, "ae_Area46_Boss_UpdateBackdrop", "v", "", 1),
    "Area46_Boss_ResetBoss": (0x8013EE74, 0x44, "ae_Area46_Boss_ResetBoss", "v", "", 0),
    "Area46_Boss_OnBossStart": (0x8013EEB8, 0x48, "ae_Area46_Boss_OnBossStart", "v", "", 1),
    "Area46_Boss_ClearFlag": (0x8013EF00, 0x0C, "ae_Area46_Boss_ClearFlag", "v", "u", 0),
    "Area46_Boss_SetAnim": (0x8013EF0C, 0x38, "ae_Area46_Boss_SetAnim", "v", "uu", 0),
    "Area46_Boss_StepAnim": (0x8013EF44, 0x88, "ae_Area46_Boss_StepAnim", "v", "u", 0),
    "Area46_Boss_UpdateHop": (0x8013EFCC, 0x114, "ae_Area46_Boss_UpdateHop", "v", "u", 1),
    "Area46_Boss_UpdatePlayerKnockback": (0x8013F0E0, 0xCC, "ae_Area46_Boss_UpdatePlayerKnockback", "v", "", 1),
    "Area46_BossUpdate": (0x8013F1AC, 0x39C, "ae_Area46_BossUpdate", "s", "u", 1),
    "Area46_Boss_ClearShockwaves": (0x8013F544, 0x64, "ae_Area46_Boss_ClearShockwaves", "v", "u", 1),
    "Area46_Boss_AddShockwave": (0x8013F5A8, 0x150, "ae_Area46_Boss_AddShockwave", "v", "u", 1),
    "Area46_Boss_UpdateShockwaves": (0x8013F6F8, 0x25C, "ae_Area46_Boss_UpdateShockwaves", "v", "", 1),
}
NATIVE_BOUNDARIES = {
    "Area46_Boss_ModelApplyLighting": (0x8013D4E0, 0x15C),
    "Area46_Boss_FillScreenFlash": (0x8013F954, 0xD8),
}
NO_C_HOOKS = {name for name, row in FUNCS.items() if not row[5]}

OvlCallFn = C.CFUNCTYPE(I32, P, P, C.c_char_p, U32, U32, C.POINTER(U32))
OvlRenderFn = C.CFUNCTYPE(I32, P, P, C.c_char_p, U32, U32, C.POINTER(U32))
CAM_HOOK = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32)
CAMUPD_HOOK = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32)
CAMLOOK_HOOK = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32, U32)
STAGE_HOOK = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32)
STAGEFADE_HOOK = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32)


def _camera_struct():
    header = (ROOT / "src/camera/ae_camera.h").read_text(encoding="utf-8")
    block = header.split("typedef struct AeCameraHooks", 1)[1].split("} AeCameraHooks;", 1)[0]
    fields = [("user", P)] + [(m.group(1), CAM_HOOK) for m in re.finditer(r"AeCameraHook\s+(\w+)\s*;", block)]
    return type("CameraHooks", (C.Structure,), {"_fields_": fields})


CameraHooks = _camera_struct()


class CamupdHooks(C.Structure):
    _fields_ = [("user", P), ("PadGetState", CAMUPD_HOOK),
                ("SpawnParticle", CAMUPD_HOOK), ("Printf", CAMUPD_HOOK),
                ("memset", CAMUPD_HOOK), ("CameraAreaScriptDispatch", CAMUPD_HOOK),
                ("CameraLookAt", CAMLOOK_HOOK), ("RenderScreenFlash", CAMUPD_HOOK),
                ("RenderCameraTransition", CAMUPD_HOOK), ("camera", CameraHooks), ("stage", P)]


def _stage_struct():
    header = (ROOT / "src/world/ae_stage.h").read_text(encoding="utf-8")
    block = header.split("typedef struct AeStageHooks", 1)[1].split("} AeStageHooks;", 1)[0]
    fields = [("user", P), ("stack_pointer", U32)] + [
        (m.group(1), STAGE_HOOK) for m in re.finditer(r"AeStageHookFn\s+(\w+)\s*;", block)]
    return type("StageHooks", (C.Structure,), {"_fields_": fields})


StageHooks = _stage_struct()


class SoundHw(C.Structure):
    _fields_ = [("user", P),
                ("wait_idle", C.CFUNCTYPE(I32, P, P)),
                ("wait_loop", C.CFUNCTYPE(None, P, P, U32)),
                ("set_voice_adsr", C.CFUNCTYPE(None, P, P, U32, U16, U16)),
                ("voice_key_off", C.CFUNCTYPE(None, P, P, U32, U16)),
                ("printf_error", C.CFUNCTYPE(None, P, P, U32, U32, I32, I32, I32)),
                ("set_voice_volume", C.CFUNCTYPE(None, P, P, U32, U32, U16))]


class Zako2Hooks(C.Structure):
    _fields_ = [("user", P), ("body_hooks", P), ("world_hooks", P),
                ("spawn_hooks", P), ("items_hooks", P),
                ("PlaySfx", CAM_HOOK), ("DrawShadow", CAM_HOOK),
                ("ModelTransformRigid", CAM_HOOK), ("ModelDraw", CAM_HOOK),
                ("FUN_8013f660", CAM_HOOK),
                ("DrawProjectileSprite", C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32, U32, U32)),
                ("DrawDebugGlyph", C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32, U32, U32)),
                ("FinishDebugText", C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32, U32, U32))]


class ItemsHooks(C.Structure):
    _fields_ = [("user", P), ("GetLevelFirstArea", CAM_HOOK),
                ("printf", CAM_HOOK), ("SetBackColor", C.CFUNCTYPE(None, P, P, U32, U32, U32)),
                ("SetFarColor", C.CFUNCTYPE(None, P, P, U32, U32, U32)),
                ("SetColorMatrix", C.CFUNCTYPE(None, P, P, U32)),
                ("rsin", C.CFUNCTYPE(I32, P, P, I32)),
                ("rcos", C.CFUNCTYPE(I32, P, P, I32))]


class StageFadeHooks(C.Structure):
    _fields_ = [("user", P), ("stack_pointer", U32), ("caller_a3", U32),
                ("stage", P), ("sound_hw", P),
                ("DebugTextPrintf", STAGEFADE_HOOK), ("SeqTrackClose", STAGEFADE_HOOK),
                ("SeqTrackSetPlayState", STAGEFADE_HOOK), ("SoundBankFree", STAGEFADE_HOOK),
                ("SoundStopAllVoices", STAGEFADE_HOOK),
                ("RenderScreenFlash", STAGEFADE_HOOK), ("RenderFadeSpiral", STAGEFADE_HOOK)]


class OvlHooks(C.Structure):
    _fields_ = [("user", P), ("body_hooks", P), ("spawn_hooks", P),
                ("camera_hooks", P), ("camupd_hooks", P), ("zako2_hooks", P),
                ("sound_hw", P), ("stagefade_hooks", P),
                ("original_call", OvlCallFn), ("renderer", OvlRenderFn)]


class MipsMem:
    def __init__(self, cpu): self.cpu = cpu
    def rd8(self, a): return self.cpu.read8(a)
    def rd16(self, a): return self.cpu.read16(a)
    def rd32(self, a): return self.cpu.read32(a)
    def wr8(self, a, v): self.cpu.write8(a, v)
    def wr16(self, a, v): self.cpu.write16(a, v)
    def wr32(self, a, v): self.cpu.write32(a, v)


def u32(v): return int(v) & 0xFFFFFFFF


class HookModel:
    def __init__(self, profile):
        self.profile = profile
        self.events = []
        self.mem = None

    def call(self, name, args, address=0):
        words = tuple(u32(x) for x in args)
        detail = ()
        result = 0
        if name == "RotTransPers":
            point = tuple(self.mem.rd16(words[0] + off) for off in (0, 2, 4))
            sx = self.profile.get("screen_x", 224) & 0xFFFF
            sy = self.profile.get("screen_y", 120) & 0xFFFF
            depth = self.profile.get("projection_depth", 512) & 0xFFFF
            self.mem.wr16(words[1], sx)
            self.mem.wr16(words[1] + 2, sy)
            self.mem.wr16(words[2], depth)
            self.mem.wr32(words[3], 0)
            result = depth
            detail = (point, sx, sy, depth)
            words = ()
        elif name == "Area46_Boss_DebrisProject":
            scratch = 0x1F800000
            depth = self.profile.get("projection_depth", 512)
            self.mem.wr32(scratch + 0xC8, self.profile.get("screen_x", 224))
            self.mem.wr32(scratch + 0xCC, self.profile.get("screen_y", 120))
            self.mem.wr32(scratch + 0xD0, depth)
            self.mem.wr32(scratch + 0xD4, depth)
            result = depth
        elif name == "memset":
            for i in range(words[2]):
                self.mem.wr8(words[0] + i, words[1])
        elif name == "PadGetState":
            result = self.profile.get("pad_state", 6)
        elif name == "Area46_Boss_DebrisRender":
            # The hook is responsible for native packet/OT writes. The values
            # supplied by the C port are already narrowed and assembled.
            self._debris_packet(words)
        elif name == "Area46_Boss_DebrisFinalize":
            self._debris_finish(words)
        self.events.append((name, address, words, detail))
        return result

    def _debris_packet(self, args):
        if len(args) < 7: return
        cursor, ot_entry, screen_x, screen_y, screen_z, color, depth = args[:7]
        self.mem.wr8(cursor + 3, 6)
        self.mem.wr32(cursor + 4, color)
        self.mem.wr32(cursor + 8, screen_x)
        self.mem.wr32(cursor + 12, 0x00404040)
        self.mem.wr32(cursor + 16, screen_y)
        self.mem.wr32(cursor + 20, 0)
        self.mem.wr32(cursor + 24, screen_z)
        current = self.mem.rd32(cursor)
        next_packet = self.mem.rd32(ot_entry + depth * 4)
        self.mem.wr32(cursor, (current & 0xFF000000) | (next_packet & 0x00FFFFFF))
        self.mem.wr32(ot_entry + depth * 4,
                      (next_packet & 0xFF000000) | (cursor & 0x00FFFFFF))

    def _debris_finish(self, args):
        if len(args) < 2: return
        cursor, ot_entry = args[:2]
        self.mem.wr8(cursor + 3, 1)
        self.mem.wr32(cursor + 4, 0xE1000200)
        self.mem.wr32(cursor + 8, 0)
        self.mem.wr32(cursor + 12, 0x00F00180)
        current = self.mem.rd32(cursor)
        self.mem.wr32(cursor, (current & 0xFF000000) |
                      (self.mem.rd32(ot_entry) & 0x00FFFFFF))
        self.mem.wr32(ot_entry, (self.mem.rd32(ot_entry) & 0xFF000000) |
                      (cursor & 0x00FFFFFF))
        cursor += 16
        self.mem.wr8(cursor + 3, 1)
        self.mem.wr32(cursor + 4, 0xE1000220)
        self.mem.wr32(cursor, (self.mem.rd32(cursor) & 0xFF000000) |
                      (self.mem.rd32(ot_entry) & 0x00FFFFFF))
        self.mem.wr32(ot_entry, (self.mem.rd32(ot_entry) & 0xFF000000) |
                      (cursor & 0x00FFFFFF))

    @staticmethod
    def signature(events):
        out = []
        for name, _address, args, detail in events:
            args = tuple(("stack",) if STACK_LO <= value < STACK_HI else value
                         for value in args)
            if name == "PadGetState":
                args = ()
            out.append((name, args, detail))
        return out


def _seed_model_data(mem):
    model_data = 0x80143200
    mem.wr32(0x800F4594, model_data)
    mem.wr32(model_data + 8, 0x20)
    for index in range(5):
        layout = model_data + 0x20 + index * 0x20
        instance = model_data + 0x300 + index * 0x100
        header = model_data + 0x1000 + index * 0x100
        mem.wr32(layout + 0x18, instance - model_data)
        mem.wr32(layout + 0x1C, header - model_data)
        for off in range(0, 0x20, 2):
            mem.wr16(instance + off, 0)
        mem.wr16(instance + 0, 0)   # bone count
        mem.wr16(instance + 2, 0)   # vertex count
        mem.wr16(instance + 4, 0)   # color count
        mem.wr16(instance + 6, 0)   # has normals
        mem.wr16(header + 0, 0)
        for clip in range(16):
            mem.wr16(header + 4 + clip * 4, clip * 8)
            mem.wr16(header + 6 + clip * 4, clip * 8 + 7)
    actor_table = 0x80143000
    mem.wr32(0x800F455C, actor_table)
    for i in range(8 * 56): mem.wr8(actor_table + i, 0)
    actor_flags = 0x80143100
    mem.wr32(0x800B4D64, actor_flags)
    for i in range(128 * 32): mem.wr8(actor_flags + i, 0)


def setup_case(mem, name, scenario):
    rng = random.Random(0x1A000 + scenario * 97 + list(FUNCS).index(name) * 13)
    for i in range(256):
        slot = 0x800BBDA0 + i * 0x14
        mem.wr16(slot, 0xFFFF)
        mem.wr16(slot + 2, i)
        mem.wr32(slot + 4, 0)
        mem.wr32(slot + 8, 0)
        mem.wr32(slot + 12, 0)
        mem.wr32(slot + 16, 0)
    mem.wr32(0x800B20D8, 0)
    mem.wr32(0x800B20DC, 0)
    for i in range(128): mem.wr16(0x800BEDA0 + i * 2, 0xFFFF)
    for i in range(STACK_HI - STACK_LO): mem.wr8(STACK_LO + i, 0)
    mem.wr8(0x800F4488, 1)  # rumble allowed
    mem.wr16(0x800BF670, 0xFFFF)
    mem.wr16(0x800BF672, 0xFFFF)
    mem.wr16(0x800BF674, 0xFFFF)
    mem.wr16(0x800BF676, 0xFFFF)
    mem.wr16(0x800EC2C8, 0)
    mem.wr32(0x800EC1E0 + 0x24, 1568)
    mem.wr32(0x800EC1E0 + 0x28, -2560)
    mem.wr32(0x800EC1E0 + 0x2C, -3520)
    mem.wr32(0x800EC204, -300)
    mem.wr8(0x800F5AA0, 0)
    mem.wr8(0x800DFDE0, 0)
    mem.wr16(0x800E69BE, 0)
    mem.wr32(0x800EC350 + 12, 0)
    mem.wr32(0x800EC354, 0x80170000)
    for i in range(0x1000): mem.wr8(0x80170000 + i, 0)
    _seed_model_data(mem)
    for i in range(256):
        mem.wr16(0x80143000 + i * 56 + 24, 0)
        mem.wr16(0x80143000 + i * 56 + 22, 0)
    mem.wr16(0x800AE8F0, 0x6D2B)
    mem.wr16(0x800AE8F2, 0x1234)
    mem.wr32(0x80142728, 0xFFFFFFFF)
    mem.wr32(0x80141880 + 0x0C, 0)
    for index in range(80):
        mem.wr8(0x80142368 + index * 12 + 10, 0)
    for index in range(8):
        record = 0x801418A0 + index * 0x90
        mem.wr32(record, 0x80144900)
        mem.wr32(record + 0x0C, 0x80144920)
        mem.wr32(record + 0x14, 0x80144800 + index * 0x20)
        mem.wr32(record + 0x18, 0x80144940)
        mem.wr8(record + 0x84, 0)
    for index in range(8):
        mem.wr8(0x80141D20 + index * 0x94 + 0x84, 0)
    mem.wr32(0x801421C0, 0x80144900)
    mem.wr32(0x801421C0 + 0x0C, 0x80144920)
    mem.wr32(0x801421C0 + 0x14, 0x80144800)
    mem.wr32(0x801421C0 + 0x18, 0x80144940)
    mem.wr32(0x801421C0 + 0x24, 1536)
    mem.wr32(0x801421C0 + 0x28, -2560)
    mem.wr32(0x801421C0 + 0x2C, -6976)
    mem.wr16(0x801421C0 + 0x60, 0)
    mem.wr16(0x801421C0 + 0x62, 0)
    mem.wr16(0x801421C0 + 0x64, 0)
    mem.wr32(0x801421C0 + 0x70, 0x80144080)
    mem.wr8(0x801421C0 + 0x78, 0)
    mem.wr32(MUTATION_SELECTOR, 0)
    mem.wr32(MUTATION_MARKER, 0)

    profile = {"screen_x": 224, "screen_y": 120, "projection_depth": 512,
               "pad_state": 6}
    actor = 0x80144000
    vector = 0x80144040
    mem.wr32(actor + 0x24, 1536)
    mem.wr32(actor + 0x28, -2560)
    mem.wr32(actor + 0x2C, -3520)
    mem.wr16(actor + 2, 0)
    mem.wr16(actor + 4, 3)
    mem.wr16(actor + 8, 9)
    mem.wr16(actor + 12, 2)
    mem.wr16(actor + 14, 4)
    mem.wr16(actor + 16, 6)
    mem.wr32(vector, 1536)
    mem.wr32(vector + 4, -2560)
    mem.wr32(vector + 8, -6976)
    mem.wr32(vector + 12, 0)

    # Stable model and animation records for all model-driven entry points.
    for index in range(5):
        model = 0x80142260 + index * 0x94
        mem.wr32(model + 0x74, 0x80144080 + index * 0x80)
        mem.wr32(model + 0x14, 0x80144100 + index * 0x80)
        mem.wr32(model + 0x24, 1536)
        mem.wr32(model + 0x28, -2560)
        mem.wr32(model + 0x2C, -6976)
        for clip in range(16):
            mem.wr16(0x80144080 + index * 0x80 + clip * 4, clip * 8)
            mem.wr16(0x80144082 + index * 0x80 + clip * 4, clip * 8 + 7)
    for index in range(8):
        record = 0x80141D20 + index * 0x94
        # A model record is passed to ModelPoseBones during UpdateEntries.
        # Keep its zero-bone header and matrix/animation pointers valid so the
        # native helper follows its ordinary no-bone path.
        mem.wr32(record, 0x80144900)
        mem.wr32(record + 0x0C, 0x80144920)
        mem.wr32(record + 0x14, 0x80144800)
        mem.wr32(record + 0x18, 0x80144940)
        mem.wr32(record + 0x1C, 0x80144080)
        mem.wr32(record + 0x7C, 0x80144080)
        mem.wr16(record + 0x24, 100 + index)
        mem.wr16(record + 0x28, -2400)
        mem.wr16(record + 0x2C, -3500)
        mem.wr16(record + 0x26, 0xFFFF)
    for address in (0x80144800, 0x80144900, 0x80144920, 0x80144940,
                    0x80144B00):
        for offset in range(0x100): mem.wr8(address + offset, 0)

    args = {
        "Area46_Boss_RegisterEntry": (actor,),
        "Area46_Boss_UpdateEncounterActor": (actor,),
        "Area46_Boss_UpdateEntryProximity": (actor,),
        "Area46_Boss_InitializeModelEntry": (2, 0x80142260, 0x801422D4, 1),
        "Area46_Boss_ProjectAndEmit": (0x80142260, 0x80144000, 0),
        "Area46_Boss_SpawnEffectNear": (0x80144180, 0x80142260, 80, 0, 512),
        "Area46_Boss_SpawnEffectNearWrap": (0x80144180, 0x80142260, 80, 512),
        "Area46_Boss_ClearDebris": (),
        "Area46_Boss_SpawnDebris": (vector,),
        "Area46_Boss_UpdateDebris": (),
        "Area46_Boss_RumbleLong": (),
        "Area46_Boss_RumbleShort": (),
        "Area46_Boss_RumbleMedium": (),
        "Area46_Boss_SpawnDropPickups": (vector,),
        "Area46_Boss_ResetLoopHandle": (),
        "Area46_Boss_StartLoop": (0x27,),
        "Area46_Boss_LoopStop": (),
        "Area46_BossInit": (),
        "Area46_Boss_InitializeActorRecords": (1,),
        "Area46_Boss_ClearEntries": (),
        "Area46_Boss_AddEntry": (vector, 2),
        "Area46_Boss_UpdateEntries": (),
        "Area46_Boss_InitializeBackdrop": (1,),
        "Area46_Boss_SetBackdrop": (1536, -2560, -6976),
        "Area46_Boss_UpdateBackdrop": (),
        "Area46_Boss_ResetBoss": (),
        "Area46_Boss_OnBossStart": (),
        "Area46_Boss_ClearFlag": (1,),
        "Area46_Boss_SetAnim": (0x80142260, 1),
        "Area46_Boss_StepAnim": (0x80142260,),
        "Area46_Boss_UpdateHop": (0x80142260,),
        "Area46_Boss_UpdatePlayerKnockback": (),
        "Area46_BossUpdate": (0x80144B00,),
        "Area46_Boss_ClearShockwaves": (1,),
        "Area46_Boss_AddShockwave": (vector,),
        "Area46_Boss_UpdateShockwaves": (),
    }[name]

    if scenario >= 0:
        extra = scenario % 8
        if name == "Area46_Boss_UpdateEncounterActor":
            mem.wr16(actor + 2, (0, 1, 2, 3, 4, 5, 0, 3)[extra])
            mem.wr32(0x800EC204, -300 if extra in (0, 1, 2, 6) else -500)
            mem.wr8(0x800F4480, 0 if extra % 3 else 1)
            mem.wr16(0x800E69BE, 1 if extra in (3, 7) else 0)
            mem.wr8(0x800F5AA0, 1 if extra == 5 else 0)
        elif name == "Area46_Boss_UpdateEntryProximity":
            state = 2 if extra == 2 else extra & 1
            mem.wr16(actor + 2, state)
            player_x, player_y, player_z = (1568, -2560, -3520)
            if extra == 1: player_x = 3168
            elif extra == 3: player_y = -2000
            elif extra == 4: player_x, player_y, player_z = (1068, -2760, -3920)
            elif extra == 5: player_x, player_y = (568, -2560)
            elif extra == 6: player_x, player_y = (1068, -3000)
            elif extra == 7: player_x, player_y, player_z = (1068, -2660, -4000)
            mem.wr32(0x800EC1E0 + 0x24, player_x)
            mem.wr32(0x800EC1E0 + 0x28, player_y)
            mem.wr32(0x800EC1E0 + 0x2C, player_z)
        elif name == "Area46_Boss_ProjectAndEmit":
            profile["screen_x"] = (224, 640, 224, 224, 224, 224, 224, 224)[extra]
            profile["screen_y"] = (120, 120, 441, -128, 120, 120, 120, 120)[extra]
            profile["projection_depth"] = (512, 512, 512, 0, 4097, 4096, 1, 0)[extra]
        elif name in ("Area46_Boss_SpawnEffectNear", "Area46_Boss_SpawnEffectNearWrap"):
            effect = 0x80144180
            near_x = -300 if extra % 2 == 0 else 1000
            near_z = -3520 if extra % 2 == 0 else 1000
            mem.wr16(effect + 4, near_x)
            mem.wr16(effect + 8, near_z)
        elif name == "Area46_Boss_UpdateDebris":
            profile["projection_depth"] = (512, 0, 4097, 4096, 1, 512, 0, 512)[extra]
            record = 0x80142368 + (79 if extra & 1 else 0) * 12
            mem.wr16(record, -300)
            mem.wr16(record + 2, -2400)
            mem.wr16(record + 4, -3500)
            mem.wr16(record + 6, -4)
            mem.wr8(record + 8, -2)
            mem.wr8(record + 9, 3)
            mem.wr8(record + 10, 4 if extra in (0, 1, 3, 5, 7) else 1)
        elif name == "Area46_Boss_SpawnDebris":
            mem.wr32(vector, 1536 + extra * 16)
            mem.wr32(vector + 4, -2560 - extra * 8)
            mem.wr32(vector + 8, -6976 + extra * 32)
        elif name == "Area46_Boss_SpawnDropPickups":
            mem.wr16(0x800EC2C8, (0, 2, 3, 3, 4, 3, 0, 5)[extra])
            mem.wr16(0x800F4350 + 0x154, (0, 0, 0, 1, 1, 0, 1, 0)[extra])
            mem.wr8(0x800F51C1, 1)
            mem.wr16(0x800AE8F0, (0x6D2B, 0xF000, 0x1000, 0xA55A,
                                  0x0001, 0xFFFF, 0x8000, 0x1234)[extra])
            if extra == 3:
                mem.wr16(0x800F4350 + 0x154, 1)
                mem.wr16(0x800AE8F0, 0x0100)
        elif name == "Area46_BossInit":
            mem.wr8(0x800DFDE0, 5 if extra == 7 else 0)
        elif name == "Area46_Boss_AddEntry":
            if extra in (1, 3, 5, 7):
                for slot in range(3): mem.wr8(0x80141D20 + slot * 0x94 + 0x84, 1)
            if extra == 6:
                for slot in range(8): mem.wr8(0x80141D20 + slot * 0x94 + 0x84, 1)
            args = (vector, 4 if extra & 1 else 2)
        elif name == "Area46_Boss_UpdateEntries":
            records = [0x80141D20 + slot * 0x94 for slot in range(8)]
            for record in records:
                mem.wr8(record + 0x82, 0)
                mem.wr8(record + 0x84, 0)
                mem.wr8(record + 0x85, 0)
                mem.wr8(record + 0x7B, 0)
                mem.wr8(record + 0x7A, 0)
                mem.wr16(record + 0x86, 0xFFFF)
                mem.wr16(record + 0x88, 12)
                mem.wr16(record + 0x8A, 0)
                mem.wr16(record + 0x8C, -8)
                mem.wr8(record + 0x8E, 0)
                mem.wr8(record + 0x8F, 0)
                mem.wr32(record + 0x24, 1000)
                mem.wr32(record + 0x28, -2400)
                mem.wr32(record + 0x2C, -3500)
                mem.wr32(record + 0x50, 0)
            first, second = records[:2]
            active = (1, 3, 2, 2, 2, 2, 2, 2)[extra]
            kind = (0, 0, 0, 1, 2, 3, 4, 4)[extra]
            mem.wr8(first + 0x84, active)
            mem.wr8(first + 0x83, kind)
            mem.wr8(first + 0x85, (0, 0, 1, 16, 0, 1, 0, 0)[extra])
            mem.wr32(first + 0x24, -300 if extra in (4, 6) else 1000)
            mem.wr32(first + 0x28, -2496 if extra == 4 else -2400)
            mem.wr32(first + 0x2C, -3520)
            mem.wr8(first + 0x8E, 0 if extra == 6 else 4)
            mem.wr8(first + 0x8F, 1 if extra == 6 else 0)
            mem.wr16(first + 0x90, 1 if extra == 7 else 5)
            if extra == 7: mem.wr32(first + 0x50, 0xB0000000)
            mem.wr8(second + 0x84, 2 if extra in (0, 1, 2, 3, 4) else 4)
            mem.wr8(second + 0x83, (0, 1, 2, 3, 4, 4, 4, 4)[extra])
            mem.wr8(second + 0x85, 1)
            if extra == 5:
                mem.wr8(second + 0x7A, 1)
            elif extra == 6:
                mem.wr8(second + 0x7A, 5)
            mem.wr32(second + 0x24, -300)
            mem.wr32(second + 0x28, -2496)
            mem.wr32(second + 0x2C, -3520)
            if extra == 7:
                mem.wr8(first + 0x82, 1)
            if extra == 0:
                mem.wr8(first + 0x85, 1)
            elif extra == 1:
                # Put all four motion states in one pass so coverage reaches
                # their shared vector, animation, and render tails.
                for slot, state in enumerate((0, 1, 2, 3, 4, 0, 1, 2)):
                    record = records[slot]
                    mem.wr8(record + 0x84, 3 if slot == 0 else 2)
                    mem.wr8(record + 0x83, state)
                    mem.wr8(record + 0x85, slot + 1)
                    mem.wr16(record + 0x86, 0xFFFF)
                    mem.wr16(record + 0x88, (slot - 3) * 8)
                    mem.wr16(record + 0x8A, 0)
                    mem.wr16(record + 0x8C, (3 - slot) * 5)
                    mem.wr8(record + 0x7B, 16 if state == 1 else 0)
                    mem.wr8(record + 0x8F, 1 if state == 4 else 0)
                    mem.wr32(record + 0x24, -300 + slot * 30)
                    mem.wr32(record + 0x28, -2496)
                    mem.wr32(record + 0x2C, -3520)
                    if state == 3: mem.wr8(record + 0x7B, 1)
                    if state == 4:
                        mem.wr8(record + 0x8E, 4)
                        mem.wr16(record + 0x88, 100)
                        mem.wr16(record + 0x8C, 100)
                        mem.wr32(record + 0x24, 4000)
            elif extra == 2:
                mem.wr8(first + 0x84, 2)
                mem.wr8(first + 0x83, 0)
                mem.wr32(first + 0x50, 0xB0000000)
                mem.wr8(first + 0x90, 3)
            elif extra == 3:
                mem.wr8(first + 0x84, 2)
                mem.wr8(first + 0x83, 0)
                mem.wr8(first + 0x85, 1)
                mem.wr16(first + 0x86, 0)
                mem.wr8(first + 0x90, 1)
                mem.wr32(first + 0x2C, -3000)
                mem.wr32(first + 0x50, 0xB0000000)
            elif extra == 4:
                for slot, state in enumerate((0, 1, 2, 3, 4, 0, 1, 2)):
                    record = records[slot]
                    mem.wr8(record + 0x84, 2)
                    mem.wr8(record + 0x83, state)
                    mem.wr8(record + 0x85, 1)
                    mem.wr16(record + 0x88, (slot - 2) * 12)
                    mem.wr16(record + 0x8C, (2 - slot) * 7)
                    mem.wr8(record + 0x7B, 1 if state == 3 else
                            (31 if state == 2 else 0))
                    mem.wr32(record + 0x50, 0xB0000000 if slot == 0 else 0)
                    if state == 0 and slot == 0:
                        mem.wr16(record + 0x88, -100)
                        mem.wr16(record + 0x8C, 100)
                    if state == 4:
                        mem.wr8(record + 0x8E, 0)
                        mem.wr8(record + 0x8F, 1)
                        mem.wr16(record + 0x88, -100)
                        mem.wr16(record + 0x8C, -100)
                        mem.wr32(record + 0x24, -500)
            elif extra == 5:
                mem.wr8(first + 0x84, 2)
                mem.wr8(first + 0x83, 3)
                mem.wr8(first + 0x7B, 1)
                mem.wr32(first + 0x2C, -3000)
            elif extra == 6:
                mem.wr8(first + 0x84, 4)
                mem.wr8(first + 0x83, 4)
                mem.wr8(first + 0x85, 4)
                mem.wr8(first + 0x7A, 10)
            elif extra == 7:
                mem.wr8(first + 0x84, 2)
                mem.wr8(first + 0x83, 4)
                mem.wr8(first + 0x85, 4)
                mem.wr8(first + 0x7A, 0)
                mem.wr8(first + 0x7B, 0)
                mem.wr8(first + 0x8E, 0)
                mem.wr8(first + 0x8F, 1)
                mem.wr16(first + 0x88, 100)
                mem.wr16(first + 0x8C, 100)
                mem.wr32(first + 0x24, 4000)
        elif name == "Area46_Boss_UpdateBackdrop":
            mem.wr8(0x801421C0 + 0x78, (0, 1, 2, 5, 16, 20, 24, 128)[extra])
            if extra in (2, 3): mem.wr16(0x801421C0 + 0x5C, 8)
        elif name == "Area46_Boss_UpdateHop":
            actor = 0x80142260
            mem.wr8(actor + 0x90, (-8, -24, 0, 4)[extra % 4])
            mem.wr8(actor + 0x86, extra % 3)
            if extra == 1: mem.wr32(0x80141870, 6)
            elif extra == 2: mem.wr32(0x80141870, 12)
            args = (actor,)
        elif name == "Area46_Boss_StepAnim":
            model = 0x80142260
            if extra in (1, 3, 5):
                mem.wr16(model + 0x5C, 6 if extra == 1 else 7)
                mem.wr8(model + 0x7B, 1 if extra == 5 else 0)
            args = (model,)
        elif name == "Area46_Boss_UpdatePlayerKnockback":
            if extra == 1:
                mem.wr16(0x80141882, 1)
                mem.wr16(0x80141884, -17)
                mem.wr16(0x80141888, 23)
                mem.wr32(0x800EC1E0 + 0x2C, -3000)
            elif extra == 2:
                mem.wr16(0x80141882, 2)
                mem.wr16(0x80141884, 25)
                mem.wr16(0x80141888, -19)
                mem.wr32(0x800EC1E0 + 0x2C, -9000)
        elif name == "Area46_BossUpdate":
            mem.wr8(0x80141880, 0)
            mem.wr8(0x80141880 + 0x1F, 0)
            mem.wr8(0x80142260 + 0x84, 1 if extra in (2, 4, 6, 7) else 0)
            mem.wr8(0x80141881, (13, 4, 8, 20, 24, 25, 4, 25)[extra])
            if extra == 1:
                mem.wr32(0x800EC204, -500)
            if extra == 4:
                mem.wr32(0x80142260 + 0x24, -1000)
            if extra == 5:
                mem.wr32(0x80141880 + 0x0C, 0)
            if extra == 7:
                mem.wr32(0x80141880 + 0x0C, 73)
            if extra == 6:
                mem.wr8(0x800EC2D2, 3)
                mem.wr8(0x800E6916, 0)
                mem.wr32(0x800EC1E0 + 0x24, 4096)
            if extra == 7:
                mem.wr32(0x800EC1E0 + 0x24, -4096)
        elif name == "Area46_Boss_AddShockwave":
            for slot in range(8): mem.wr8(0x801418A0 + slot * 0x90 + 0x84, 1 if extra == 7 else 0)
        elif name == "Area46_Boss_UpdateShockwaves":
            rec = 0x801418A0
            mem.wr8(rec + 0x84, 1)
            mem.wr32(rec + 0x24, 1500)
            mem.wr32(rec + 0x28, (-2400, -2500, -2600, -1000)[extra % 4])
            mem.wr32(rec + 0x2C, (-7000, -6500, -6300, -6000)[extra % 4])
        elif name == "Area46_Boss_StartLoop":
            args = (-1 if extra & 1 else 0x27,)
        elif name == "Area46_Boss_LoopStop":
            mem.wr32(0x80142728, 0xFFFFFFFF if extra & 1 else 0)
        elif name == "Area46_Boss_SpawnDropPickups":
            mem.wr16(0x800EC2C8, 3 if extra & 1 else 0)
            mem.wr16(0x800AE8F0,
                     0x0100 if extra == 3 else (0xF000 if extra & 1 else 0x1000))
            if extra == 3:
                mem.wr16(0x800F4350 + 0x154, 1)

    if name == "Area46_Boss_UpdateEntries" and scenario >= 8:
        # These directed fixtures isolate branches that ordinary actor lists
        # rarely reach. Keep every other entry inactive so its native body
        # state cannot affect the scenario under test.
        for record in (0x80141D20 + slot * 0x94 for slot in range(8)):
            mem.wr8(record + 0x84, 0)
        record = 0x80141D20
        mem.wr8(record + 0x84, 2)
        mem.wr8(record + 0x85, 0)
        mem.wr8(record + 0x83, 0)
        mem.wr8(record + 0x7B, 0)
        mem.wr8(record + 0x7A, 0)
        mem.wr8(record + 0x8E, 0)
        mem.wr8(record + 0x8F, 0)
        mem.wr16(record + 0x86, 0xFFFF)
        mem.wr8(record + 0x90, 0)
        mem.wr32(record + 0x24, -300)
        mem.wr32(record + 0x28, -2496)
        mem.wr32(record + 0x2C, -3520)
        mem.wr16(record + 0x88, 0)
        mem.wr16(record + 0x8A, 0)
        mem.wr16(record + 0x8C, 0)
        mem.wr32(record + 0x50, 0)
        mem.wr16(record + 0x5C, 0)
        mem.wr32(record + 0x1C, 0x80144080)

        directed_case = scenario - 8
        if directed_case == 0:
            # Both signed velocity clamps and the low-probability stationary
            # sound branch; no frame particle is emitted on this tick.
            mem.wr8(record + 0x83, 0)
            mem.wr8(record + 0x85, 0)
            mem.wr16(record + 0x88, -100)
            mem.wr16(record + 0x8C, 100)
            mem.wr16(0x800AE8F0, 0)
        elif directed_case == 1:
            # Retreat damping rounds a negative product with an added bias.
            mem.wr8(record + 0x83, 3)
            mem.wr16(record + 0x88, -128)
            mem.wr8(record + 0x7B, 2)
        elif directed_case == 2:
            # Spin effect crosses the signed X clamp and the negative position
            # boundary after its first timer update.
            mem.wr8(record + 0x83, 4)
            mem.wr16(record + 0x88, -100)
            mem.wr16(record + 0x8C, 100)
            mem.wr8(record + 0x8E, 1)
            mem.wr32(record + 0x24, -500)
        elif directed_case == 3:
            # X is in range while Z is outside the effect's proximity window.
            mem.wr32(record + 0x24, 1568)
            mem.wr32(record + 0x28, -2560)
            mem.wr32(record + 0x2C, -5000)
        elif directed_case == 4:
            # Incrementing the final frame wraps to the clip's first frame.
            mem.wr16(record + 0x5C, 7)
            mem.wr32(record + 0x24, 0)
            mem.wr32(record + 0x2C, -5000)
        elif directed_case == 5:
            # Isolate the contact cooldown's zero transition and guarded drop.
            mem.wr8(record + 0x83, 0)
            mem.wr8(record + 0x85, 0)
            mem.wr32(record + 0x2C, -3520)
            mem.wr8(record + 0x90, 1)
            mem.wr32(record + 0x50, 0xB0000000)
            mem.wr8(0x80141880 + 0x1C, 0)

    return args, profile


def _mips_args(name, args):
    if name == "Area46_Boss_StartLoop":
        return args
    if name in ("Area46_Boss_InitializeActorRecords",
                "Area46_Boss_InitializeBackdrop", "Area46_Boss_ClearShockwaves"):
        return (0, 0, 0, *args)
    return args


def build(source=None, tag="ovl1a0"):
    rel = "src/world/ae_ovl1a0.c"
    if rel not in check_game.SOURCES:
        check_game.SOURCES.append(rel)
    return check_game.build_dll({rel: source} if source else None, tag)


class Harness:
    def __init__(self, dll):
        reg.declare("stack_window", "801FF000..80200000")
        areas = dump_areas.area_members()
        self.area_id = 0x46
        area_path = ROOT / "extract/members/0F7.bin"
        self.cpu = mr.MipsRam.from_extract(area_path)
        self.cpu.load_area((ROOT / "extract/members/1A0.bin").read_bytes(), OVERLAY_BASE)
        self.cpu.attach_gte_oracle()
        self.cpu.coverage = set()
        self.base = self.cpu.snapshot()
        self.cside = z1.CSide(dll)
        self.dll = self.cside.dll
        self.ram = self.cside.ram
        self.main = self.cside.main
        self.scratch = self.cside.scratch
        self.cmem = z1.CMem(self.dll, self.ram)
        self.functions = {}
        for name, (_address, _size, symbol, result, args, with_hooks) in FUNCS.items():
            fn = getattr(self.dll, symbol)
            fn.argtypes = [P] + ([C.POINTER(OvlHooks)] if with_hooks else []) + [I32 if k == "i" else U32 for k in args]
            fn.restype = {"v": None, "s": I32, "u": U32}[result]
            self.functions[name] = fn
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0
        self.callbacks = []

    def _make_hooks(self, model):
        self.cside.set_model(model)
        camera = CameraHooks()
        camupd = CamupdHooks()
        stage = StageHooks()
        sound = SoundHw()
        zako2 = Zako2Hooks()
        stagefade = StageFadeHooks()
        items = ItemsHooks()
        callbacks = []

        atan = self.dll.ae_Atan2Safe
        atan.argtypes = [P, I32, I32]
        atan.restype = I32
        normalize = self.dll.ae_sdk_VectorNormalSS
        normalize.argtypes = [P, U32, U32]
        normalize.restype = I32

        def atan2_safe(_user, ram, x, z, _a2, _a3):
            return atan(ram, x, z)

        def vector_normal(_user, ram, vector, output, _a2, _a3):
            return normalize(ram, vector, output)

        def stage_exit_pending(_user, _ram, _a0, _a1, _a2, _a3):
            return 0

        camera.Atan2Safe = CAM_HOOK(atan2_safe)
        camera.VectorNormalSS = CAM_HOOK(vector_normal)
        camera.StageExitPending = CAM_HOOK(stage_exit_pending)
        callbacks.extend([camera.Atan2Safe, camera.VectorNormalSS,
                          camera.StageExitPending])

        def generic_cb(_user, ram, name, address, count, words):
            model.mem = z1.CMem(self.dll, ram)
            return model.call(name.decode("ascii"), [words[i] for i in range(count)], address)

        call_cb = OvlCallFn(generic_cb)
        render_cb = OvlRenderFn(generic_cb)
        callbacks.extend([call_cb, render_cb])

        def pad_state(_user, ram, a0, a1, a2, a3):
            model.mem = z1.CMem(self.dll, ram)
            return model.call("PadGetState", (a0, a1, a2, a3), 0x8002B52C)
        pad_cb = CAMUPD_HOOK(pad_state)
        camupd.PadGetState = pad_cb
        callbacks.append(pad_cb)

        def empty_call(name, addr):
            def callback(_user, ram, a0, a1, a2, a3):
                model.mem = z1.CMem(self.dll, ram)
                return model.call(name, (a0, a1, a2, a3), addr)
            return CAM_HOOK(callback)

        for name, addr in (("BgmSetVolumeMode", 0x800863B0),
                           ("AreaFeature30Request", 0x8009B7B0),
                           ("MeshInitFromAreaTable", 0x80015CC8),
                           ("SoundStopAllVoices", 0x800A342C),
                           ("VSync", 0x8002E520)):
            cb = empty_call(name, addr)
            callbacks.append(cb)
            if name == "SoundStopAllVoices":
                def stop_voices(_u, ram, a0, a1, a2, a3, label=name, address=addr):
                    model.mem = z1.CMem(self.dll, ram)
                    return model.call(label, (a0, a1, a2, a3), address)
                stagefade.SoundStopAllVoices = STAGEFADE_HOOK(stop_voices)
                callbacks.append(stagefade.SoundStopAllVoices)
            if name == "VSync":
                def vsync(_u, ram, a0, a1, a2, a3, label=name, address=addr):
                    model.mem = z1.CMem(self.dll, ram)
                    return model.call(label, (a0, a1, a2, a3), address)
                stage.VSync = STAGE_HOOK(vsync)
                callbacks.append(stage.VSync)

        # Sound hardware hooks stay inert in the fixture unless a sound path
        # requests them; their calls still preserve order in the native model.
        sound.wait_idle = C.CFUNCTYPE(I32, P, P)(lambda _u, _r: 0)
        sound.wait_loop = C.CFUNCTYPE(None, P, P, U32)(lambda _u, _r, _n: None)
        sound.set_voice_adsr = C.CFUNCTYPE(None, P, P, U32, U16, U16)(lambda *_: None)
        sound.voice_key_off = C.CFUNCTYPE(None, P, P, U32, U16)(lambda *_: None)
        sound.printf_error = C.CFUNCTYPE(None, P, P, U32, U32, I32, I32, I32)(lambda *_: None)
        sound.set_voice_volume = C.CFUNCTYPE(None, P, P, U32, U32, U16)(lambda *_: None)
        stagefade.stage = C.cast(C.pointer(stage), P)
        stagefade.sound_hw = C.cast(C.pointer(sound), P)
        callbacks.extend([sound.wait_idle, sound.wait_loop, sound.set_voice_adsr,
                          sound.voice_key_off, sound.printf_error, sound.set_voice_volume])

        hooks = OvlHooks()
        hooks.body_hooks = C.cast(C.pointer(self.cside.body_hooks), P)
        hooks.spawn_hooks = C.cast(C.pointer(self.cside.spawn_hooks), P)
        hooks.camera_hooks = C.cast(C.pointer(camera), P)
        hooks.camupd_hooks = C.cast(C.pointer(camupd), P)
        zako2.body_hooks = hooks.body_hooks
        zako2.world_hooks = C.cast(C.pointer(self.cside.world_hooks), P)
        zako2.spawn_hooks = hooks.spawn_hooks
        zako2.items_hooks = C.cast(C.pointer(items), P)
        hooks.zako2_hooks = C.cast(C.pointer(zako2), P)
        hooks.sound_hw = C.cast(C.pointer(sound), P)
        hooks.stagefade_hooks = C.cast(C.pointer(stagefade), P)
        hooks.original_call = call_cb
        hooks.renderer = render_cb
        callbacks.extend([camera, camupd, stage, sound, items, zako2, stagefade, hooks])
        self.callbacks.extend(callbacks)
        return hooks

    def _mips_hooks(self, model):
        hooks = {}
        specs = {
            0x8002FB00: ("GTE_SetRotMatrix", 1),
            0x8002FB30: ("GTE_SetTransMatrix", 1),
            0x80031330: ("RotTransPers", 4),
            0x80016A60: ("ModelTransformSkinned", 1),
            0x8013D4E0: ("Area46_Boss_ModelApplyLighting", 4),
            0x800863B0: ("BgmSetVolumeMode", 2),
            0x8009B7B0: ("AreaFeature30Request", 1),
            0x80015CC8: ("MeshInitFromAreaTable", 2),
            0x800A3740: ("memset", 3),
            0x8002B52C: ("PadGetState", 4),
            0x800A342C: ("SoundStopAllVoices", 1),
            0x8002E520: ("VSync", 0),
        }
        for address, (name, count) in specs.items():
            def callback(cpu, regs, label=name, arity=count, addr=address):
                values = list(regs[4:8])
                if arity > 4:
                    values.extend(cpu.read32(regs[29] + 16 + i * 4) for i in range(arity - 4))
                model.mem = MipsMem(cpu)
                return model.call(label, values[:arity], addr)
            hooks[address] = callback
        return hooks

    def _patches(self, name, model):
        if name != "Area46_Boss_UpdateDebris": return {}

        def project(cpu, regs):
            matrix = regs[16]
            remaining = regs[21]
            vertex = matrix + (((remaining & 3) << 5) + 0x48)
            lifetime = cpu.read8(regs[17] + 0x0A)
            model.mem = MipsMem(cpu)
            depth = model.call("Area46_Boss_DebrisProject",
                               (matrix, vertex, 9, lifetime), 0x8013DCA0)
            return 0x8013DCE8

        def emit(cpu, regs):
            scratch = 0x1F800000
            model.mem = MipsMem(cpu)
            cursor = regs[19]
            table_index = cpu.read32(0x800EC35C)
            display_record = 0x800EC3D0 + table_index * 0x4078
            args = (cursor, display_record, cpu.read32(scratch + 0xC8),
                    cpu.read32(scratch + 0xCC), cpu.read32(scratch + 0xD0),
                    0x32808080, cpu.read32(scratch + 0xD4))
            model.call("Area46_Boss_DebrisRender", args, 0x8013DD0C)
            regs[18] = (regs[18] + 28) & 0xFFFFFFFF
            regs[19] = (regs[19] + 28) & 0xFFFFFFFF
            return 0x8013DD8C

        def finish(cpu, regs):
            model.mem = MipsMem(cpu)
            table_index = cpu.read32(0x800EC35C)
            display_record = 0x800EC3D0 + table_index * 0x4078
            model.call("Area46_Boss_DebrisFinalize", (regs[19], display_record), 0x8013DD98)
            regs[19] = (regs[19] + 8) & 0xFFFFFFFF
            return 0x8013DDE8

        return {0x8013DCA0: project, 0x8013DD0C: emit, 0x8013DD98: finish}

    def run(self, name, scenario=0, selector=0):
        self.cpu.restore(self.base)
        self.cpu.coverage = self.cpu.coverage or set()
        mem = MipsMem(self.cpu)
        args, profile = setup_case(mem, name, scenario)
        mem.wr32(MUTATION_SELECTOR, selector)
        mem.wr32(MUTATION_MARKER, 0)
        before = self.cpu.snapshot()
        m_model = HookModel(profile)
        c_model = HookModel(profile)
        m_model.mem = mem
        mips_hooks = self._mips_hooks(m_model)
        hooks = self._make_hooks(c_model)
        self.cside.sync(before)
        mips_args = _mips_args(name, args)
        address, _size, _symbol, ret_kind, _argtypes, with_hooks = FUNCS[name]
        try:
            original = self.cpu.call(address, mips_args, hooks=mips_hooks,
                                     patches=self._patches(name, m_model),
                                     limit=2_000_000, sp=STACK_HI - 0x20)
        except Exception as exc:  # noqa: BLE001
            self.fail(name, scenario, f"MIPS execution failed: {exc}")
            return False
        call_args = [self.ram]
        if with_hooks: call_args.append(C.byref(hooks))
        call_args.extend(args)
        actual = self.functions[name](*call_args)
        self.counts[name] += 1
        if ret_kind != "v" and u32(actual) != original[2]:
            self.fail(name, scenario, f"return C={u32(actual):08X} MIPS={original[2]:08X}")
            return False

        expected = bytearray(self.cpu.ram[:STACK_LO - 0x80000000])
        got = bytearray(self.main[:STACK_LO - 0x80000000])
        if expected != got:
            off = next(i for i, (a, b) in enumerate(zip(expected, got)) if a != b)
            self.fail(name, scenario, f"RAM differs at {0x80000000 + off:08X}: C={got[off:off+8].hex()} MIPS={expected[off:off+8].hex()}")
            return False
        if bytes(self.cside.scratch) != bytes(self.cpu.scratch):
            self.fail(name, scenario, "scratchpad differs")
            return False
        if self.cmem.rd32(MUTATION_MARKER) != self.cpu.read32(MUTATION_MARKER):
            self.fail(name, scenario, "mutation marker differs")
            return False
        if HookModel.signature(c_model.events) != HookModel.signature(m_model.events):
            self.fail(name, scenario, f"hook sequence differs C={HookModel.signature(c_model.events)!r} MIPS={HookModel.signature(m_model.events)!r}")
            return False
        if self.dll.ae_ram_failed(self.ram):
            self.fail(name, scenario, self.dll.ae_ram_error(self.ram).decode(errors="replace"))
            return False
        return True

    def fail(self, name, scenario, message):
        self.failures += 1
        print(f"FAIL {name}[scenario={scenario}]: {message}", flush=True)

    def all(self, cases=8):
        self.cpu.coverage.clear()
        ok = True
        for name in FUNCS:
            function_cases = cases + (6 if name == "Area46_Boss_UpdateEntries" else 0)
            for scenario in range(function_cases):
                ok = self.run(name, scenario) and ok
            print(f"{name}: {self.counts[name]} cases", flush=True)
        return ok

    def finish(self, default_run=False, show_coverage=False):
        items = [(name, data[0], data[1], True) for name, data in FUNCS.items()]
        items.extend((name, address, size, True) for name, (address, size) in NATIVE_BOUNDARIES.items())
        summary = reg.coverage_summary(items, self.cpu.coverage)
        if show_coverage: reg.print_coverage(summary)
        gaps = {name: row for name, row in summary.items() if row["uncovered_count"] or row["stale_declared"]}
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps,
                              extra={"overlay": "1A0.bin", "areas": [0x46]},
                              function_names=list(FUNCS))
        for name, row in gaps.items():
            print(f"COVERAGE GAP {name}: {row['uncovered']}; stale={row['stale_declared']}", flush=True)
        return not gaps and not self.failures


def instrument_mutations(source):
    mutated = source
    for function_index, (name, row) in enumerate(FUNCS.items(), 1):
        symbol = row[2]
        pattern = re.compile(r"(\b(?:void|int32_t|uint32_t)\s+" + re.escape(symbol) + r"\s*\([^)]*\)\s*\{)", re.S)
        inserts = "\n".join(
            f"    if (ae_rd32(r, 0x{MUTATION_SELECTOR:08X}u) == {function_index * 2 - v}u) "
            f"ae_wr32(r, 0x{MUTATION_MARKER:08X}u, 0x{MUTATION_MAGIC:08X}u | {function_index * 2 - v}u);"
            for v in (1, 2))
        mutated, count = pattern.subn(r"\1\n" + inserts, mutated, count=1)
        if count != 1: raise SystemExit(f"could not instrument {symbol} ({name})")
    return mutated


def run_mutations():
    source_path = ROOT / "src/world/ae_ovl1a0.c"
    mutant_path = source_path.with_name("ae_ovl1a0_mutant.c")
    mutant_path.write_text(instrument_mutations(source_path.read_text(encoding="utf-8")), encoding="utf-8")
    try:
        harness = Harness(build(mutant_path, "ovl1a0_mutant"))
        caught = 0
        for i, name in enumerate(FUNCS, 1):
            for variant in (1, 2):
                selector = i * 2 - variant
                before = harness.failures
                ok = harness.run(name, variant - 1, selector)
                if not ok and harness.failures > before:
                    caught += 1
                else:
                    print(f"MUTATION SURVIVED {name} selector={selector}", flush=True)
        print(f"mutations caught: {caught}/{2 * len(FUNCS)}", flush=True)
        return 0 if caught == 2 * len(FUNCS) else 1
    finally:
        mutant_path.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--cases", type=int, default=8)
    args = parser.parse_args()
    if args.mutate: return run_mutations()
    start = time.time()
    harness = Harness(build())
    passed = harness.all(args.cases)
    coverage = harness.finish(default_run=args.cases == 8, show_coverage=args.coverage)
    print(f"differential: {sum(harness.counts.values())} cases in {time.time()-start:.1f}s; "
          f"{'PASS' if passed and coverage else 'FAIL'}", flush=True)
    return 0 if passed and coverage else 1


if __name__ == "__main__":
    sys.exit(main())
