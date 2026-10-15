"""Differential checks for the missa main-EXE ports.

Each case starts from a real EXE and area-member image. The original MIPS
function and the AeRam implementation receive the same initialized state and
the same deterministic boundary callbacks; returns, RAM, scratchpad, and
callback order are compared.
"""
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
sys.path[:0] = [str(ROOT / "tools/mips"), str(ROOT / "tools")]
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402
import check_hud  # noqa: E402

P, U32, I32, VP = C.c_void_p, C.c_uint32, C.c_int32, C.c_void_p
HookFn = C.CFUNCTYPE(I32, VP, VP, U32, U32, U32, U32)
Hook5Fn = C.CFUNCTYPE(I32, VP, VP, U32, U32, U32, U32, U32)
STACK_LO = 0x1FF000
STACK_HI = 0x200000
DEFAULT_CASES = 32
SEED = 0x94423A

EXTRA_COVERAGE = {
    "HudDrawTimeRankIcon packet block": (0x8005F70C, 0x5C),
    "HudDrawMonkeyInfo badge packet block": (0x800605C8, 0xA8),
}

# (address, byte size, C symbol, result type, C argument types, hooks)
FUNCS = {
    "AreaUsesAltMember": (0x800115F4, 204, "ae_AreaUsesAltMember", "s", "ss", False),
    "AreaCheckExits": (0x80010D80, 816, "ae_AreaCheckExits", "s", "u", True),
    "PlayerPlaceAtEntry": (0x80032D38, 240, "ae_PlayerPlaceAtEntry", "s", "sss", True),
    "PlayerInit": (0x8003272C, 944, "ae_PlayerInit", "v", "", True),
    "TrailRingPush": (0x8001F8B0, 164, "ae_TrailRingPush", "v", "", False),
    "TrailRingDraw": (0x8001F954, 580, "ae_TrailRingDraw", "v", "uuuuu", True),
    "PadDemoPlaybackStep": (0x80013128, 308, "ae_PadDemoPlaybackStep", "s", "", False),
    "StickToButtons": (0x800140B4, 104, "ae_StickToButtons", "u", "u", False),
    "ModelInitNormals": (0x80015B30, 112, "ae_ModelInitNormals", "v", "uu", False),
    "ScreenFadeSet": (0x80046978, 24, "ae_ScreenFadeSet", "v", "uuu", False),
    "PlayTimeAdd": (0x8005DD44, 52, "ae_PlayTimeAdd", "v", "s", False),
    "HudDrawTimeRankIcon": (0x8005F68C, 228, "ae_HudDrawTimeRankIcon", "v", "ssuu", True),
    "HudDrawMonkeyInfo": (0x80060448, 812, "ae_HudDrawMonkeyInfo", "v", "u", True),
    "CameraRcCarFollow": (0x8004AA58, 556, "ae_CameraRcCarFollow", "v", "u", True),
    "PlayerInput_StickGesture": (0x800650D8, 16, "ae_PlayerInput_StickGesture", "u", "", False),
    "PointInBoxXZ": (0x80073B64, 100, "ae_PointInBoxXZ", "s", "uu", False),
    "DirIndexFromFlags": (0x8007774C, 96, "ae_DirIndexFromFlags", "s", "u", False),
    "FadeFrame": (0x8007A14C, 40, "ae_FadeFrame", "s", "", True),
    "ZakoClearDefeatedBits": (0x80054A08, 180, "ae_ZakoClearDefeatedBits", "v", "u", False),
    "ZakoSaveDefeatedMask": (0x800509CC, 304, "ae_ZakoSaveDefeatedMask", "v", "", True),
    "CameraRadarFocusSelect": (0x80047E8C, 84, "ae_CameraRadarFocusSelect", "v", "", True),
    "CameraRadarFocusUpdate": (0x80047EE0, 272, "ae_CameraRadarFocusUpdate", "v", "", False),
    "CameraGetSlotTarget": (0x8004B54C, 52, "ae_CameraGetSlotTarget", "v", "u", False),
    "BossAimSfx": (0x8004898C, 96, "ae_BossAimSfx", "v", "", True),
    "AimRequestStart": (0x80048398, 152, "ae_AimRequestStart", "v", "", True),
    "CameraAreaScriptDispatch": (0x8004B590, 108, "ae_CameraAreaScriptDispatch", "s", "uuuu", True),
    "CallIndexedHandler": (0x8004D880, 56, "ae_CallIndexedHandler", "s", "uuuu", True),
    "StageExitStart": (0x8004D73C, 100, "ae_StageExitStart", "v", "", True),
    "EventCamStartVictory": (0x8004BBF0, 108, "ae_EventCamStartVictory", "v", "", True),
    "EventCamStartDefault": (0x8004BC5C, 64, "ae_EventCamStartDefault", "v", "", True),
    "PlayerAttachSet": (0x80066EEC, 72, "ae_PlayerAttachSet", "v", "uuuu", True),
    "PlayerSetBounceFlag": (0x80066F34, 28, "ae_PlayerSetBounceFlag", "v", "", False),
    "SpawnEffectGroupAt": (0x800698E4, 60, "ae_SpawnEffectGroupAt", "v", "ssss", True),
    "BreakableFinalizeVisualUpdate": (0x8006AA60, 176, "ae_BreakableFinalizeVisualUpdate", "v", "uus", True),
    "PlayerFxJumpBurst": (0x8006C58C, 280, "ae_PlayerFxJumpBurst", "v", "", True),
    "PlayerFxSpawn116": (0x8006CE9C, 84, "ae_PlayerFxSpawn116", "v", "", False),
    "PlayerFxSetMatrixPos": (0x8006CFBC, 136, "ae_PlayerFxSetMatrixPos", "v", "u", False),
    "Indicator_Activate": (0x800717B4, 580, "ae_Indicator_Activate", "v", "ss", True),
    "TargetListFindBest": (0x8007CDFC, 344, "ae_TargetListFindBest", "u", "uus", True),
    "PadDirectionCode": (0x8007F40C, 340, "ae_PadDirectionCode", "v", "", True),
}
HOOKS = {
    "BeginFrame": 0x8005DF48, "PlaySfx": 0x800877F8,
    "ClearWords": 0x800A3740,
    "PropVisualUpdate": 0x80071120,
    "Printf": 0x800A3720,
    "PlayerAuxModelInit": 0x80098FB0,
    "HudDrawTimeRankIcon": 0x8005F70C,
    "HudDrawStatPips": 0x80060224,
    "HudDrawFrameBox": 0x80060334,
    "HudTextWidth": 0x80082230,
    "MessageBoxOpen": 0x8008DEEC,
    "DrawMenuChrome": 0x8007D71C,
    "DrawMenuRows": 0x8007D9F0,
    "DrawMenuHighlight": 0x8007E030,
    "DrawMenuItem": 0x8007E1B8,
    "DrawMenuSelection": 0x8007E3F8,
    "DrawMenuCursor": 0x8007F148,
    "SetRotMatrix": 0x8002FB00,
    "SetTransMatrix": 0x8002FB30,
}
MIPS_ONLY_HOOKS = {
    "FadeUpdate": 0x80079ECC, "BossKindForArea": 0x8005AB00,
    "CameraFixedModeOff": 0x80048748,
    "GetLevelFirstArea": 0x8009764C, "ObjSlotStatus": 0x80050580,
    "EventCamCreate": 0x8004BB68, "StageExitRequest": 0x8004D6F4,
    "StageTypeClearInit": 0x80094F48,
}
MIPS_DIRECT_CALLS = {
    "BossAimSfx": {"PlaySfx"},
    "AimRequestStart": {"BossKindForArea", "PlaySfx"},
    "EventCamStartVictory": {"EventCamCreate", "CameraFixedModeOff", "PlaySfx"},
    "EventCamStartDefault": {"EventCamCreate", "CameraFixedModeOff"},
    "StageExitStart": {"StageExitRequest"},
    "BreakableFinalizeVisualUpdate": {"PlaySfx"},
    "PadDirectionCode": {"PlaySfx"},
    "Indicator_Activate": {"CameraFixedModeOff", "PlaySfx"},
    "ZakoSaveDefeatedMask": {"GetLevelFirstArea", "ObjSlotStatus"},
}
MIPS_HOOKS = {**HOOKS, **MIPS_ONLY_HOOKS, "MatrixLookDir": 0x8001484C,
              "MatrixFromDirXZ": 0x80014804,
              "GetSolidHeight": 0x8004D8C0,
              "TrailRingDraw": 0x8001F954,
              "SetDrawTPage": 0x8002B410,
              "SpuWaitIdle": 0x800A1398,
              "RotTransPers": 0x80031330,
              "CdIntToPos": 0x80025D20,
              "CdPosToInt": 0x80025E24}
CAMERA_HOOK_NAMES = (
    "Atan2Safe", "VectorNormalSS", "SquareRoot0", "GetGroundHeight",
    "rsin", "rcos", "RotTransPers", "SetRotMatrix", "SetTransMatrix",
    "memset", "PlayerCanExitStage", "PlayerIsSubmerged",
    "CameraRcCarActive", "CameraRcCarFollow", "IsFlightArea",
    "FUN_8013d000", "BossUpdate", "CameraAreaScriptDispatch",
    "CallIndexedHandler", "RotMatrixY", "ApplyMatrixSV", "MatrixIdentity",
    "CameraGetPlayerDir", "AimRequestStart", "AimRequestClear",
    "AimModeEnter", "AimModeLeave", "CameraFixedModeOff",
    "StageExitPending", "BossAimSfx", "BossKindForArea", "ScreenFadeSet",
    "PlaySfx", "HudDrawMonkeyInfo", "RadarUsable", "MonkeyFlag248Set",
    "CameraRadarFocusUpdate", "CameraRadarFocusSelect")
SPAWN_FUNCS = {"PlayerFxSpawn116"}
RENDERER_HOOKS = {
    "TrailRingDraw", "HudDrawStatPips", "HudDrawFrameBox",
    "DrawMenuChrome", "DrawMenuRows", "DrawMenuHighlight",
    "DrawMenuItem", "DrawMenuSelection", "DrawMenuCursor",
}
MENU_WRAPPERS = {
    "DrawMenuChrome", "DrawMenuRows", "DrawMenuHighlight",
    "DrawMenuItem", "DrawMenuSelection", "DrawMenuCursor",
}
INDIRECT_TARGET = 0x80003000
INDIRECT_FN = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32, U32)


def build_dll(core_source=None, extra_sources=(), source_overrides=None):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"missa_{time.time_ns()}.dll"
    sources = [ROOT / "src/core/ae_ram.c", Path(core_source or ROOT / "src/core/ae_missa_core.c"),
               ROOT / "src/core/ae_sdk.c",
               ROOT / "src/core/ae_sdk_matrix.c", ROOT / "src/bodies/ae_bodies.c",
               *sorted((ROOT / "src/monkeys").glob("ae_monkey*.c")),
               ROOT / "src/player/native_gadget_slots.c",
               ROOT / "src/camera/ae_camera.c", ROOT / "src/camera/ae_camupd.c",
               ROOT / "src/core/ae_pad.c",
               ROOT / "src/sound/ae_sound.c", ROOT / "src/player/ae_player_spawn.c",
               ROOT / "src/core/ae_pose.c", ROOT / "src/player/ae_gadget.c",
               ROOT / "src/player/ae_player1.c", ROOT / "src/world/ae_areaset.c",
               ROOT / "src/world/ae_solid.c",
               ROOT / "src/world/ae_items_init.c",
               ROOT / "src/world/ae_zako2.c",
               ROOT / "src/world/ae_missa_zako.c",
               ROOT / "src/world/ae_zako_init.c",
               ROOT / "src/world/ae_stage.c",
               ROOT / "src/world/ae_stagefade.c",
               ROOT / "src/world/ae_missa_area.c",
               ROOT / "src/camera/ae_missa_camera.c",
               ROOT / "src/camera/ae_missa_rc_car.c",
               ROOT / "src/player/ae_missa_gadget.c",
               ROOT / "src/world/ae_world.c", ROOT / "src/effects/ae_spawn.c",
               ROOT / "src/effects/ae_missa_spawn.c",
               ROOT / "src/effects/ae_effects_core.c",
               ROOT / "src/ui/ae_hud.c", ROOT / "src/player/native_player.c",
               ROOT / "src/player/ae_missa_player.c",
               ROOT / "src/player/ae_player_act_mode.c",
               ROOT / "src/player/ae_player_act_state.c",
               ROOT / "src/ui/ae_missa_renderer.c",
               ROOT / "src/ui/ae_missa_hud.c",
               ROOT / "src/world/ae_missa_indicator.c",
               ROOT / "src/ui/ae_pausemenu.c",
               ROOT / "src/world/ae_progress.c",
               ROOT / "tools/mips/missa_test_stubs.c",
               *[ROOT / source for source in extra_sources]]
    if source_overrides:
        sources = [Path(source_overrides.get(
            str(path.relative_to(ROOT)).replace("\\", "/"), path))
                   for path in sources]
    linker = [] if source_overrides else ["-Wl,--gc-sections"]
    subprocess.run([shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-ffunction-sections", "-fdata-sections", "-shared", "-static-libgcc",
                    *[flag for directory in ("core", "effects", "ui", "player", "sound", "world")
                      for flag in ("-I", str(ROOT / "src" / directory))],
                    *map(str, sources), *linker, "-o", str(dll)], check=True)
    return dll


def _seed_spawn_group(m, effect_type, padding, stack_depth):
    table_index = effect_type - (256 if effect_type < 456 else 456)
    stream = 0x800D7000
    m.write32(0x800B1828 + table_index * 4, stream)
    m.write16(stream, 7)
    m.write16(stream + 2, 1)
    m.write16(stream + 4, 0)
    m.write16(stream + 6, 0)
    padding_address = mr.DEFAULT_SP - stack_depth - 64 + 30
    m.write16(padding_address, padding)


class Side:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        self.lib.ae_ram_create.restype = P
        self.lib.ae_ptr.argtypes, self.lib.ae_ptr.restype = [P, U32, U32], P
        self.lib.ae_ram_clear_error.argtypes = [P]
        self.lib.ae_ram_failed.argtypes, self.lib.ae_ram_failed.restype = [P], C.c_int
        self.lib.ae_ram_error.argtypes, self.lib.ae_ram_error.restype = [P], C.c_char_p
        self.ram = self.lib.ae_ram_create()
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(
            self.lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(
            self.lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.hooks_type = type("AeMissaHooks", (C.Structure,), {
            "_fields_": [("user", VP), ("stack_pointer", U32), ("spawn", VP),
                         ("stage", VP), ("world", VP), ("area_set", VP),
                         ("camera", VP), ("progress", VP), ("player1", VP),
                         ("player_act", VP), ("zako2", VP),
                         ("stage_fade", VP), ("camupd", VP), ("sound", VP)] +
                        [(name, HookFn) for name in HOOKS] +
                        [("RendererHudMonkeyBadge", HookFn),
                         ("TrailRingDraw", Hook5Fn),
                         ("CallIndirect", INDIRECT_FN)]
        })
        self.progress_hooks_type = type("AeProgressHooks", (C.Structure,), {
            "_fields_": [("user", VP), ("sound_hw", VP), ("gimmick_a", VP),
                         ("area_set", VP), ("SoundLoopsSetPan", HookFn),
                         ("FUN_8013FB20", HookFn), ("SoundBankSetVolume", HookFn),
                         ("SetDrawTPage", HookFn)]
        })
        self.stage_fade_hooks_type = type("AeStageFadeHooks", (C.Structure,), {
            "_fields_": [("user", VP), ("stack_pointer", U32),
                         ("caller_a3", U32), ("stage", VP), ("sound_hw", VP)] +
                        [(name, HookFn) for name in (
                            "DebugTextPrintf", "SeqTrackClose",
                            "SeqTrackSetPlayState", "SoundBankFree",
                            "SoundStopAllVoices", "RenderScreenFlash",
                            "RenderFadeSpiral")]
        })
        self.spawn_hooks_type = type("AeSpawnHooks", (C.Structure,), {
            "_fields_": [("user", VP), ("world", VP), ("PlaySfx", HookFn),
                         ("MatrixLookDir", HookFn), ("MatrixFromDirXZ", HookFn),
                         ("position_padding", C.CFUNCTYPE(C.c_uint16, VP, VP, U32))]
        })
        self.world_hooks_type = type("AeWorldHooks", (C.Structure,), {
            "_fields_": [("user", VP)] + [(name, HookFn) for name in (
                "GetSolidHeight", "PlaySfxAt", "GimmickActivate",
                "MeshInitFromAreaTable", "FUN_8013d358", "CameraShake")] +
                [("ground_stale_x", C.c_uint16), ("ground_stale_z", C.c_uint16)]
        })
        self.camera_hooks_type = type("AeCameraHooks", (C.Structure,), {
            "_fields_": [("user", VP)] + [(name, HookFn) for name in CAMERA_HOOK_NAMES]
        })
        look_at_fn = C.CFUNCTYPE(I32, VP, VP, U32, U32, U32, U32, U32)
        self.camupd_hooks_type = type("AeCamupdHooks", (C.Structure,), {
            "_fields_": [("user", VP)] +
                        [(name, HookFn) for name in (
                            "PadGetState", "SpawnParticle", "Printf", "memset",
                            "CameraAreaScriptDispatch")] +
                        [("CameraLookAt", look_at_fn)] +
                        [(name, HookFn) for name in (
                            "RenderScreenFlash", "RenderCameraTransition")] +
                        [("camera", self.camera_hooks_type), ("stage", VP)]
        })
        stage_header = (ROOT / "src/world/ae_stage.h").read_text()
        stage_body = re.search(
            r"typedef struct AeStageHooks \{(.*?)\} AeStageHooks;",
            stage_header, re.S).group(1)
        stage_fields = re.findall(
            r"(?:AeStageHookFn|const AeMonkeyHooks\s*\*|const AeGadgetHooks\s*\*)\s*(\w+)\s*;",
            stage_body)
        self.stage_hooks_type = type("AeStageHooks", (C.Structure,), {
            "_fields_": [("user", VP), ("stack_pointer", U32)] +
                        [(name, HookFn) for name in stage_fields]
        })
        self.sound_wait_idle_type = C.CFUNCTYPE(I32, VP, VP)
        self.sound_wait_loop_type = C.CFUNCTYPE(None, VP, VP, U32)
        self.sound_set_adsr_type = C.CFUNCTYPE(None, VP, VP, U32,
                                               C.c_uint16, C.c_uint16)
        self.sound_key_off_type = C.CFUNCTYPE(None, VP, VP, U32, C.c_uint16)
        self.sound_printf_type = C.CFUNCTYPE(None, VP, VP, U32, U32,
                                             I32, I32, I32)
        self.sound_volume_type = C.CFUNCTYPE(None, VP, VP, U32, U32,
                                             C.c_uint16)
        self.sound_hw_type = type("AeSoundHw", (C.Structure,), {
            "_fields_": [("user", VP), ("wait_idle", self.sound_wait_idle_type),
                         ("wait_loop", self.sound_wait_loop_type),
                         ("set_voice_adsr", self.sound_set_adsr_type),
                         ("voice_key_off", self.sound_key_off_type),
                         ("printf_error", self.sound_printf_type),
                         ("set_voice_volume", self.sound_volume_type)]
        })
        self.player1_hooks_type = type("AePlayer1Hooks", (C.Structure,), {
            "_fields_": [("user", VP)] + [(name, HookFn) for name in (
                "CdIntToPos", "CdPosToInt", "CdControl", "CdReadyCallback",
                "VSync", "printf", "SoundSetVolumeMode", "EventCamCreate",
                "ModelEmitPrimsCycleColors", "DrawRibbonBetween",
                "ModelProjectVertices", "ModelEmitPrims")] +
                [("sound_hw", VP), ("CdMgrQueueCommand", HookFn)]
        })
        self.items_hooks_type = type("AeItemsInitHooks", (C.Structure,), {
            "_fields_": [("user", VP), ("GetLevelFirstArea", HookFn),
                         ("printf", HookFn), ("SetBackColor", HookFn),
                         ("SetFarColor", HookFn), ("SetColorMatrix", VP),
                         ("rsin", VP), ("rcos", VP)]
        })
        self.zako2_hooks_type = type("AeZako2Hooks", (C.Structure,), {
            "_fields_": [(name, VP) for name in (
                "user", "body_hooks", "world_hooks", "spawn_hooks",
                "items_hooks")] + [(name, HookFn) for name in (
                    "PlaySfx", "DrawShadow", "ModelTransformRigid", "ModelDraw",
                    "FUN_8013f660", "DrawProjectileSprite", "DrawDebugGlyph",
                    "FinishDebugText")]
        })
        self.callbacks = []
        self.events = []
        self.hooks = self.hooks_type()
        self.hooks.stack_pointer = mr.DEFAULT_SP
        self.hooks.stage_fade = VP()
        self.progress_hooks = self.progress_hooks_type()
        self.hooks.progress = C.cast(C.pointer(self.progress_hooks), VP)
        self.hook_returns = {}
        self.spawn_hooks = self.spawn_hooks_type()
        self.world_hooks = self.world_hooks_type()
        self.camera_hooks = self.camera_hooks_type()
        self.player1_hooks = self.player1_hooks_type()
        self.items_hooks = self.items_hooks_type()
        self.zako2_hooks = self.zako2_hooks_type()
        for name in HOOKS:
            callback = HookFn(lambda _u, _r, a0, a1, a2, a3, n=name:
                              self._hook(n, (a0, a1, a2, a3)))
            self.callbacks.append(callback)
            setattr(self.hooks, name, callback)
        self.stage_fade_hooks = self.stage_fade_hooks_type()
        self.stage_fade_hooks.stack_pointer = mr.DEFAULT_SP
        self.stage_fade_callback = HookFn(
            lambda _u, _r, a0, a1, a2, a3:
            self._hook("RenderFadeSpiral", (a0, a1, a2, a3)))
        self.callbacks.append(self.stage_fade_callback)
        self.stage_fade_hooks.RenderFadeSpiral = self.stage_fade_callback
        self.hooks.stage_fade = C.cast(C.pointer(self.stage_fade_hooks), VP)
        self.stage_hooks = self.stage_hooks_type()
        self.stage_hooks.stack_pointer = mr.DEFAULT_SP
        self.stage_type_clear_callback = HookFn(
            lambda _u, _r, a0, a1, a2, a3:
            self._hook("StageTypeClearInit", (a0, a1, a2, a3)))
        self.callbacks.append(self.stage_type_clear_callback)
        self.stage_hooks.StageTypeClearInit = self.stage_type_clear_callback
        self.camera_hooks = self.camera_hooks_type()
        self.camupd_hooks = self.camupd_hooks_type()
        self.camupd_hooks.camera = self.camera_hooks
        self.sound_hw = self.sound_hw_type()
        self.sound_wait_idle_callback = self.sound_wait_idle_type(
            lambda _u, _r: self._hook("SpuWaitIdle", (0, 0, 0, 0)))
        self.callbacks.append(self.sound_wait_idle_callback)
        self.sound_printf_callback = self.sound_printf_type(
            lambda _u, _r, fmt, _argc, a, b, c:
            self._hook("Printf", (fmt, a, b, c)))
        self.callbacks.append(self.sound_printf_callback)
        self.sound_hw.wait_idle = self.sound_wait_idle_callback
        self.sound_hw.printf_error = self.sound_printf_callback
        self.hooks.camupd = C.cast(C.pointer(self.camupd_hooks), VP)
        self.hooks.sound = C.cast(C.pointer(self.sound_hw), VP)
        self.badge_callback = HookFn(
            lambda _u, _r, a0, a1, a2, a3:
            self._hook("RendererHudMonkeyBadge", (a0, a1, a2, a3)))
        self.callbacks.append(self.badge_callback)
        self.hooks.RendererHudMonkeyBadge = self.badge_callback
        for name in CAMERA_HOOK_NAMES:
            callback = HookFn(lambda _u, _r, a0, a1, a2, a3, n=name:
                              self._hook(n, (a0, a1, a2, a3)))
            self.callbacks.append(callback)
            setattr(self.camera_hooks, name, callback)
        self.hooks.camera = C.cast(C.pointer(self.camera_hooks), VP)
        self.camupd_hooks.camera = self.camera_hooks
        for name in ("CdIntToPos", "CdPosToInt", "VSync", "printf"):
            callback = HookFn(lambda _u, _r, a0, a1, a2, a3, n=name:
                              self._hook(n, (a0, a1, a2, a3)))
            self.callbacks.append(callback)
            setattr(self.player1_hooks, name, callback)
        self.hooks.player1 = C.cast(C.pointer(self.player1_hooks), VP)
        self.zako2_hooks.items_hooks = C.cast(C.pointer(self.items_hooks), VP)
        self.hooks.zako2 = C.cast(C.pointer(self.zako2_hooks), VP)
        self.progress_draw_page_callback = HookFn(
            lambda _u, _r, a0, a1, a2, a3:
            self._hook("SetDrawTPage", (a0, a1, a2, a3)))
        self.callbacks.append(self.progress_draw_page_callback)
        self.progress_hooks.SetDrawTPage = self.progress_draw_page_callback
        self.trail_draw_callback = Hook5Fn(
            lambda _u, _r, a0, a1, a2, a3, a4:
            self._hook("TrailRingDraw", (a0, a1, a2, a3, a4)))
        self.callbacks.append(self.trail_draw_callback)
        self.hooks.TrailRingDraw = self.trail_draw_callback
        self.spawn_hooks.PlaySfx = self.hooks.PlaySfx
        world_callback = HookFn(lambda _u, _r, a0, a1, a2, a3:
                                self._hook("GetSolidHeight", (a0, a1, a2, a3)))
        self.callbacks.append(world_callback)
        self.world_hooks.GetSolidHeight = world_callback
        self.hooks.world = C.cast(C.pointer(self.world_hooks), VP)
        for name in ("MatrixLookDir", "MatrixFromDirXZ"):
            callback = HookFn(lambda _u, _r, a0, a1, a2, a3, n=name:
                              self._matrix_hook(n, (a0, a1, a2, a3)))
            self.callbacks.append(callback)
            setattr(self.spawn_hooks, name, callback)
        self.padding = 0x5A5A
        self.padding_callback = C.CFUNCTYPE(C.c_uint16, VP, VP, U32)(
            lambda _u, _r, _original: self.padding)
        self.spawn_hooks.position_padding = self.padding_callback
        self.hooks.spawn = C.cast(C.pointer(self.spawn_hooks), VP)
        self.indirect_callback = INDIRECT_FN(
            lambda _u, _r, target, a0, a1, a2, a3:
            self._indirect(target, (a0, a1, a2, a3)))
        self.hooks.CallIndirect = self.indirect_callback
        self.functions = {}
        for name, (_address, _size, symbol, result, arg_types, takes_hooks) in FUNCS.items():
            fn = getattr(self.lib, symbol)
            hook_type = C.POINTER(self.spawn_hooks_type) if name in SPAWN_FUNCS else C.POINTER(self.hooks_type)
            fn.argtypes = [P] + ([hook_type] if takes_hooks or name in SPAWN_FUNCS else []) + [
                I32 if kind == "s" else U32 for kind in arg_types
            ]
            fn.restype = {"s": I32, "u": U32, "v": None}[result]
            self.functions[name] = fn

    def _hook(self, name, args):
        normalized = self._normalize_hook(name, args)
        self.events.append((name, tuple(value & 0xFFFFFFFF for value in normalized)))
        if name == "ClearWords":
            address, value, size = args[:3]
            start = address - 0x80000000
            C.memset(C.addressof(self.main) + start, value & 0xFF, size)
        value = self.hook_returns.get(name, 0)
        return value(*args) if callable(value) else value

    def _indirect(self, target, args):
        self.events.append(("CallIndirect", (target & 0xFFFFFFFF,) +
                           tuple(value & 0xFFFFFFFF for value in args)))
        return 0

    def _matrix_hook(self, name, args):
        normalized = self._normalize_hook(name, args)
        self.events.append((name, tuple(value & 0xFFFFFFFF for value in normalized)))
        address = args[1]
        for index in range(16):
            ptr = self.lib.ae_ptr(self.ram, address + index * 2, 2)
            if ptr:
                C.cast(ptr, C.POINTER(C.c_uint16))[0] = (0x100 + index * 7) & 0xFFFF
        return 0

    @staticmethod
    def _normalize_hook(name, args):
        values = list(args)
        if name in ("MatrixLookDir", "MatrixFromDirXZ", "PropVisualUpdate"):
            values[2:] = [0, 0]
        elif name == "PlaySfx":
            values[1:] = [0, 0, 0]
        elif name == "ClearWords":
            values[3:] = [0]
        elif name in ("GetLevelFirstArea", "ObjSlotStatus", "Printf", "PlayerAuxModelInit"):
            values[1:] = [0, 0, 0]
        elif name == "HudTextWidth":
            values[1:] = [0, 0, 0]
        elif name in ("SetRotMatrix", "SetTransMatrix"):
            values[1:] = [0, 0, 0]
        elif name in ("MessageBoxOpen",):
            values = [0, 0, 0, 0]
        elif name in ("SpuWaitIdle", "StageTypeClearInit"):
            values = [0, 0, 0, 0]
        elif name == "CdIntToPos":
            values[2:] = [0, 0]
        elif name == "CdPosToInt":
            values[1:] = [0, 0, 0]
        elif name == "HudDrawStatPips":
            values[2:] = [0, 0]
        if name in ("MatrixLookDir", "MatrixFromDirXZ", "ClearWords"):
            values = [0xFFFFFFFF if 0x801FF000 <= value < 0x80200000 else value
                      for value in values]
        if name == "GetSolidHeight":
            values[3] = 0
        return tuple(values)

    def sync(self, mips):
        C.memmove(self.main, bytes(mips.ram), mr.RAM_SIZE)
        C.memmove(self.scratch, bytes(mips.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)
        self.events.clear()


class Harness:
    def __init__(self, dll, seed):
        reg.declare("stack_window", "801FF000..80200000")
        members = dump_areas.area_members()
        if 1 not in members:
            raise SystemExit("real area 0x01 member is missing")
        member = ROOT / f"extract/members/{members[1]:03X}.bin"
        self.m = mr.MipsRam.from_extract(member)
        member_008 = ROOT / "extract/members/008.bin"
        if not member_008.is_file():
            raise SystemExit("real member 008 is required for PlayerInit")
        self.m.write_bytes(0x80103000, member_008.read_bytes())
        self.m.attach_gte_oracle()
        self.m.coverage = set()
        self.base = self.m.snapshot()
        self.c = Side(dll)
        self.rng = random.Random(seed)
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0

    def setup(self, name, case):
        m = self.m
        self.c.hook_returns.clear()
        self.c.padding = (0x5A5A + case * 193) & 0xFFFF
        rng = random.Random(f"{name}/{case}/{SEED}")
        if name == "HudDrawTimeRankIcon":
            thresholds = 0x800D4000
            threshold_sets = ((100, 200, 300), (300, 200, 100),
                              (-1, 0, 1), (0x7FFF, -0x8000, 42))
            for index, threshold in enumerate(threshold_sets[case % len(threshold_sets)]):
                m.write16(thresholds + index * 2, threshold)
            times = (0, 1, 50, 100, 101, 200, 201, 300, 301,
                     1000, 0x7FFF, 0xFFFFFFFF)
            return (24 + case, 32 + case, times[case % len(times)], thresholds)
        if name == "HudDrawMonkeyInfo":
            if case % 5 == 0:
                return (0,)
            monkey = 0x800D5000
            table = 0x800D6000
            info_index = case % 16
            info = table + info_index * 0x88
            m.write32(0x800B4CF0, table)
            m.write8(monkey + 0x7C, info_index)
            scores = (-1000, -1, 0, 9, 95, 999, 1000, 1234, 9999)
            m.write16(info + 0x1C, scores[case % len(scores)])
            m.write_bytes(info + 0x5E, bytes((0x4D, 0x4F, 0x4E, 0x4B, 0x45, 0x59, 0)))
            m.write_bytes(info + 0x6A, bytes((0x53, 0x50, 0x59, 0x00)))
            m.write32(0x800F4350, 0x10 if case % 2 else 0)
            m.write32(0x800EC350, rng.getrandbits(32))
            m.write32(0x800EC354, 0x801D7000)
            m.write32(0x800EC35C, 0)
            widths = (-100, 0, 40, 320, 384, 500)
            self.c.hook_returns["HudTextWidth"] = widths[case % len(widths)]
            return (monkey,)
        if name == "CameraRcCarFollow":
            reference, player, camera_state = 0x800F5A20, 0x800EC1E0, 0x800E68D0
            slot = 0x800D8000
            offsets = (0, 100, 384, 386, 385, 1000)
            distance = offsets[case % len(offsets)]
            m.write16(reference + 0x24, 120)
            m.write16(reference + 0x28, 80)
            m.write16(reference + 0x2C, -60)
            m.write32(reference + 0x30, rng.choice([-4, -1, 0, 1, 4]))
            m.write32(reference + 0x38, rng.choice([-4, -1, 0, 1, 4]))
            m.write32(player + 0x24, 120 - distance)
            m.write32(player + 0x28, 80 + rng.randrange(-10, 11))
            m.write32(player + 0x2C, -60)
            m.write16(0x800E54A2, (0, 20, 21, 40)[case % 4])
            m.write16(camera_state + 6, 1 if case % 3 == 0 else 0)
            m.write8(camera_state + 0x47, 1)
            m.write8(slot + 0x80, case * 7)
            m.write8(slot + 0x81, 1 if case % 3 == 0 else 0)
            projected_depths = (120, 0, -1, 800, 2000, 500, 10496)
            self.c.hook_returns["RotTransPers"] = projected_depths[case % len(projected_depths)]
            return (slot,)
        if name == "Indicator_Activate":
            index = case % 4
            indicator = 0x800BBD34 + index * 24
            kind, state, timer = ((-1, 0, 0), (0, 1, 0), (0, 0, 0),
                                  (0, 0, 0), (1, 0, 0), (2, 1, 0),
                                  (2, 0, 1), (2, 0, 0), (3, 0, 0))[case % 9]
            if case % 9 >= 4:
                index = 0
                indicator = 0x800BBD34
            m.write16(indicator, kind)
            m.write16(indicator + 0x16, state)
            m.write16(indicator + 0x0A, timer)
            m.write16(indicator + 0x04, rng.randrange(-900, 900))
            m.write16(indicator + 0x06, rng.randrange(-900, 900))
            m.write16(indicator + 0x08, rng.randrange(-900, 900))
            m.write8(0x800F5A00, 1 if case % 9 == 2 else 0)
            m.write32(0x800B7E50 + 20 * 4, (0x123 << 20) | 0x456)
            m.write16(0x800E3904, 0)
            m.write16(0x800E3906, 0)
            m.write16(0x800F4470, 0)
            m.write16(0x800F4472, 0)
            m.write16(0x800F44A0, 0)
            m.write8(0x800B28B0, 0)
            m.write8(0x800B28B1, 0)
            for pickup_index in range(32):
                m.write8(0x800BEFA0 + pickup_index * 24 + 0x0A, 0)
            m.write16(0x800AE8F0, rng.randrange(0x10000))
            m.write16(0x800AE8F2, rng.randrange(0x10000))
            self.c.hook_returns["CdPosToInt"] = 0x1200
            return (index, case - 3)
        if name == "TrailRingDraw":
            ring_cursor = 0x800B8AB0
            ring_records = ring_cursor + 4
            ring_stride = 20
            ring_life = 16
            current_index = case % 12
            m.write32(ring_cursor, current_index)
            for index in range(12):
                record = ring_records + index * ring_stride
                for word in range(4):
                    m.write32(record + word * 4,
                              rng.choice([-0x1000, -1, 0, 1, 0x1000,
                                          rng.getrandbits(32)]))
                m.write32(record + ring_life, 0)
            chains = ((0,), (1,), (31,), (32,), (33,), (63,), (64,),
                      (65,), (32, 32), (64, 32), (96, 64, 32),
                      (128, 96, 64, 32), (96, 0, 32), (64, 0),
                      (65, 64, 32), (1, 1, 1))
            chain = chains[case % len(chains)]
            for offset, life in enumerate(chain):
                index = (current_index - offset) % 12
                m.write32(ring_records + index * ring_stride + ring_life, life)
            m.write32(0x800EC354, 0x801D7000 + case * 0x80)
            m.write32(0x800EC358, 0x801D6000 + case * 0x20)
            return tuple(rng.getrandbits(32) for _ in range(5))
        if name in RENDERER_HOOKS:
            argument_count = {"TrailRingDraw": 5, "HudDrawStatPips": 2}.get(name, 4)
            return tuple(rng.getrandbits(32) for _ in range(argument_count))
        if name == "AreaUsesAltMember":
            m.write8(0x800F4350 + 0x12C, 11 if case % 8 == 0 else 0)
            values = [1, 2, 3, 4, 5, 6, 0xFF]
            m.write_bytes(0x800A851C, bytes(values))
            area = values[0] if case % 4 in (0, 1) else rng.choice([-1, 99, 0x10001])
            index = values[1] if case % 4 == 0 else rng.choice([values[4], -1, 99, 0x10002])
            return (area, index)
        if name == "AreaCheckExits":
            stage, player = 0x800F4350, 0x800EC1E0
            exits = 0x800D9000
            mode = case % 2
            route_kind = 1
            exit_count = 2
            if case % 12 == 0:
                exits, exit_count = 0, 3
            elif case % 12 == 1:
                route_kind = 36
            elif case % 12 == 2:
                exit_count = 0
            elif case % 12 == 3:
                mode, route_kind, exit_count = 0, 17, 1
            elif case % 12 == 4:
                mode, route_kind, exit_count = 0, 1, 1
            elif case % 12 == 5:
                mode, route_kind, exit_count = 0, 1, 2
            elif case % 12 == 6:
                mode, exit_count = 1, 1
            elif case % 12 == 7:
                mode, exit_count = 1, 2
            elif case % 12 == 8:
                mode, exit_count = 1, 1
            elif case % 12 == 9:
                mode, exit_count = 1, 1
            elif case % 12 == 10:
                mode, exit_count = 1, 1
            else:
                mode, exit_count = 1, 1
            m.write32(stage + 0x238, exits)
            m.write32(stage + 0x28C, exit_count)
            m.write16(stage + 0x120, route_kind)
            m.write16(stage + 0x126, 7 + case)
            m.write16(stage + 0x12A, case & 7)
            m.write8(stage + 0x12C, 32)
            centers = [rng.randrange(-2000, 2000) for _ in range(3)]
            if case % 12 == 10 and case // 12 == 0:
                m.write32(player + 0x24, centers[0])
                m.write32(player + 0x28, centers[1] + 1000)
                m.write32(player + 0x2C, centers[2])
            elif case % 12 == 11 and case // 12 == 0:
                m.write32(player + 0x24, centers[0])
                m.write32(player + 0x28, centers[1])
                m.write32(player + 0x2C, centers[2] + 1000)
            elif case % 12 in (10, 11):
                for axis, value in enumerate(centers):
                    m.write32(player + 0x24 + axis * 4, value - 1)
            elif case % 12 in (3, 4, 6):
                inside_delta = 0
                if case % 12 == 3:
                    inside_delta = -1 if (case // 12) % 2 == 0 else 1
                m.write32(player + 0x24, centers[0] + inside_delta)
                m.write32(player + 0x28, centers[1] + inside_delta)
                m.write32(player + 0x2C, centers[2] + inside_delta)
            else:
                for axis, value in enumerate(centers):
                    m.write32(player + 0x24 + axis * 4, value + 1000)
            types = {3: 2, 4: 2, 5: 0, 6: 0, 7: 0, 8: 3, 9: 1, 10: 1, 11: 4}
            for index in range(max(0, exit_count)):
                record = exits + index * 0x28
                exit_type = types.get(case % 12, rng.choice([0, 1, 2, 3, 4]))
                m.write32(record + 0x00, exit_type)
                for axis, offset in enumerate((4, 8, 12)):
                    m.write32(record + offset, centers[axis])
                m.write32(record + 0x10, 500)
                for offset in (0x14, 0x18, 0x1C):
                    m.write32(record + offset, 500)
                m.write32(record + 0x20, 0x40 + case)
                m.write32(record + 0x24, 0x20 + index)
            return (mode,)
        if name == "PlayerPlaceAtEntry":
            stage = 0x800F4350
            m.write32(stage + 0x234, 0x80154000)
            area = [1, 2, 61, -1, 0x12345][case % 5]
            entry = [0, 1, 7, 15, 13][case % 5]
            speed = rng.choice([-32769, -4096, -1, 0, 1, 2048, 32767, 32768])
            m.write32(0x800D6000, 2)
            m.write32(0x800F44E4, 0x800D6000)
            self.c.hook_returns["GetSolidHeight"] = rng.choice([-20000, -1, 0, 1000, 16383])
            return (speed, area, entry)
        if name == "PlayerInit":
            if case == DEFAULT_CASES - 1:
                # Empty boot header: StaticModelInit's output aliases the
                # stored auxiliary count. 800329B8 must reload that count.
                m.write_bytes(0x80103000, bytes(0x100))
            return ()
        if name == "TrailRingPush":
            m.write32(0x800B8AB0, rng.choice([0, 1, 10, 11, 0x7FFFFFFF, 0xFFFFFFFF, rng.getrandbits(32)]))
            for offset in range(0x34, 0x44, 4):
                m.write32(0x800F5120 + offset, rng.getrandbits(32))
            return ()
        if name == "PadDemoPlaybackStep":
            m.write32(0x800A8A50, 0x800D0000)
            frame_index = 3599 if case == DEFAULT_CASES - 1 else case
            m.write32(0x800A8A54, frame_index)
            m.write32(0x800A8A58, 1)
            m.write32(0x800E5490, rng.randrange(40))
            m.write32(0x800E5478, rng.getrandbits(32))
            record = 0x800D0000 + frame_index * 8
            m.write32(record, rng.getrandbits(32))
            m.write32(record + 4, rng.getrandbits(32) or 1)
            m.write8(0x800E52D0, 0xFF if case % 3 == 0 else 0)
            m.write16(0x800E52D2, 0xFFFF)
            if case % 3 == 1:
                m.write16(0x800E52D2, 0xF000)
            if case % 3 == 2:
                m.write32(record + 4, 0)
            return ()
        if name == "StickToButtons":
            index = case % 40
            m.write32(0x800E5490, index)
            m.write_bytes(0x800E5320 + index * 4, bytes([0, 0, rng.randrange(256), rng.randrange(256)]))
            return (rng.getrandbits(32),)
        if name == "ModelInitNormals":
            model, state = 0x800D1000, 0x800D2000
            for off in range(0, 0x20, 2):
                m.write16(model + off, rng.choice([0, 1, 2, 7, 31, 255]))
            return (state, model)
        if name == "ScreenFadeSet":
            return tuple(rng.getrandbits(32) for _ in range(3))
        if name == "PlayTimeAdd":
            m.write32(0x800F4490, rng.getrandbits(32))
            return (rng.choice([-32769, -32768, -1, 0, 1, 32767, 32768, rng.randrange(-100000, 100000)]),)
        if name == "PlayerInput_StickGesture":
            m.write16(0x1F80005E, rng.getrandbits(16))
            return ()
        if name == "PointInBoxXZ":
            pos, box = 0x800D1000, 0x800D2000
            center_x, center_z = rng.randrange(-1000, 1000), rng.randrange(-1000, 1000)
            half_x, half_z = rng.randrange(0, 400), rng.randrange(0, 400)
            m.write16(box + 0, center_x)
            m.write16(box + 4, center_z)
            m.write16(box + 6, half_x)
            m.write16(box + 10, half_z)
            if case % 4 == 0:
                x, z = center_x, center_z
            elif case % 4 == 1:
                x, z = center_x + half_x + 1, center_z
            elif case % 4 == 2:
                x, z = center_x, center_z - half_z - 1
            else:
                x, z = rng.getrandbits(32), rng.getrandbits(32)
            m.write32(pos + 0, x)
            m.write32(pos + 8, z)
            return (pos, box)
        if name == "DirIndexFromFlags":
            return ([0x400, 0x800, 0xC00, 0, 0xF00, rng.getrandbits(32)][case % 6],)
        if name == "FadeFrame":
            return ()
        if name == "ZakoClearDefeatedBits":
            object_count = rng.choice([0, 1, 2, 5, 9])
            m.write16(0x800E77E0, object_count)
            mask_ptr = 0x800D2000
            m.write32(mask_ptr, rng.getrandbits(32))
            for slot in range(12):
                object = 0x800E6960 + slot * 0xE4
                m.write_bytes(object, rng.randbytes(0xE4))
                if slot < object_count:
                    m.write32(object + 0xE0, rng.choice([0, 0x10, 0x11, 0xFFFFFFFF]))
                    m.write8(object + 0x7E, rng.choice([0x68, 0x69, 0x67, 0xFF]))
                    m.write8(object + 0xCA, rng.choice([slot, 0, 1, 2, 0xFF]))
                    m.write8(object + 0xCC, rng.randrange(32))
            return (mask_ptr,)
        if name == "ZakoSaveDefeatedMask":
            stage = 0x800F4350
            area_list = 0x800E77D0
            records = 0x800D8000
            level = rng.choice([0, 1, 8, 31, 32, 33])
            first_area = rng.choice([0, 1, 4, 20, 64])
            current_area = (first_area + rng.choice([-2, -1, 0, 1, 7, 8, 13])) & 0xFFFF
            spawn_count = rng.choice([-1, 0, 1, 2, 7])
            m.write16(stage + 0x124, level)
            m.write16(stage + 0x126, current_area)
            m.write16(stage + 0x160, rng.getrandbits(16))
            m.write16(0x800B3858 + level * 2, first_area)
            m.write32(area_list + 4, records)
            m.write16(area_list + 0xC, spawn_count)
            for slot in range(max(0, spawn_count)):
                record = records + slot * 0x20
                m.write16(record, rng.randrange(0x800))
                m.write8(record + 0x16, rng.randrange(2))
            return ()
        if name == "CameraRadarFocusSelect":
            active = case % 2 != 0
            area = 0x58 if case % 4 == 0 else 1
            area_kind = 0x0B if case % 4 == 1 else 0
            for base in (0x800F4350, 0x800F8000):
                m.write16(base + 0x126, area)
                m.write16(base + 0x152, area_kind)
                m.write8(base + 0x12C, 3 if active else 2)
            m.write16(0x800EC1E0 + 0x5E, 0x46 if case % 4 == 2 else 0)
            m.write8(0x800EC1E0 + 0xF2, 2 if active else 1)
            m.write32(0x800F5138, 0x800D1000 if active else 0)
            m.write32(0x800F513C, 1 if active else 0)
            return ()
        if name == "CameraRadarFocusUpdate":
            actor = 0x800D1000 if case % 3 else 0
            m.write32(0x800B4D44, actor)
            if actor:
                for offset in range(0, 0x40, 2):
                    m.write16(actor + offset, rng.getrandbits(16))
                m.write16(actor + 0x46, -1 if case % 2 else 1)
            return ()
        if name == "CameraGetSlotTarget":
            slot, out = 0x800D1000, 0x800D2000
            m.write32(0x800E68D8, slot)
            m.write_bytes(slot + 0x12, rng.randbytes(8))
            return (out,)
        if name == "BossAimSfx":
            m.write8(0x800E68D0 + 0x2F, 1 if case % 5 == 0 else 0)
            m.write8(0x800E68D0 + 0x41, 1 if case % 5 == 1 else 0)
            exit_code = 2 if case % 5 in (2, 3) else 3
            m.write8(0x800F447C, exit_code)
            m.write8(0x800F812C, exit_code)
            return ()
        if name == "AimRequestStart":
            m.write8(0x800E68D0 + 0x32, 1 if case == 0 else 0)
            m.write8(0x800E68D0 + 0x3E, 1 if case == 1 else 0)
            m.write8(0x800E68D0 + 0x41, 1 if case == 2 else 0)
            exit_code = 2 if case in (3, 4) else 3
            m.write8(0x800F447C, exit_code)
            m.write8(0x800F812C, exit_code)
            m.write16(0x800F4350 + 0x126,
                      0x44 if case in (5, 6, 7) else 1)
            return ()
        if name == "CameraAreaScriptDispatch":
            table = 0x80141390 if case % 2 == 0 else 0x8014440C
            m.write16(0x800F4476, 56 if case % 2 == 0 else 66)
            m.write32(table + 3 * 4, INDIRECT_TARGET)
            self.indirect_target = INDIRECT_TARGET
            return (3, 0x12340000 | case, 0x87654321, 0xFEDCBA98)
        if name == "CallIndexedHandler":
            index = case % 8
            m.write32(0x800AF140 + index * 4, INDIRECT_TARGET)
            self.indirect_target = INDIRECT_TARGET
            return (0x12340000 | case, index, 0x87654321, 0xFEDCBA98)
        if name == "StageExitStart":
            m.write16(0x800E68D0 + 0x6A, 0 if case % 2 == 0 else (0x100 + case))
            return ()
        if name in ("EventCamStartVictory", "EventCamStartDefault"):
            return ()
        if name == "PlayerAttachSet":
            record = 0x800D1000
            m.write32(record, 0x800D2000 | case)
            m.write16(record + 4, 0x8000 + case)
            _seed_spawn_group(m, 278, self.c.padding, stack_depth=80)
            return (record, 30 + case, 0x12345678, 0x87654321)
        if name == "PlayerSetBounceFlag":
            m.write32(0x800EC2C0, rng.getrandbits(32))
            return ()
        if name == "SpawnEffectGroupAt":
            _seed_spawn_group(m, 274, self.c.padding, stack_depth=32)
            return (274, -12, 18, 44)
        if name == "PadDirectionCode":
            mask_by_dir = [0x1000, 0x4000, 0x8000, 0x2000,
                           0x0004, 0x0001, 0x0008, 0x0002]
            direction = case % 8
            mask = mask_by_dir[direction] if case % 10 else 0
            m.write32(0x800E5478, mask)
            m.write32(0x800E5480, mask)
            if mask != 0 and case % 10 != 0:
                table_index = case % 5
                code = ((case + 1) << 4) | (direction + 1)
                m.write32(0x800BEF70, case + 1)
                m.write32(0x800B2220 + table_index * 4, code)
            else:
                m.write32(0x800BEF70, rng.getrandbits(32))
            return ()
        if name == "TargetListFindBest":
            table, origin, direction = 0x800F51F0, 0x800D1000, 0x800D1100
            target_count = case % 9
            for index in range(32):
                m.write32(table + index * 4, 0)
            for index in range(target_count):
                target = 0x800D2000 + index * 0x20
                m.write32(table + index * 4, target)
                for axis, offset in enumerate((0, 2, 4)):
                    base = m.read16(origin + axis * 4)
                    delta = rng.choice([-3000, -512, -12, -1, 0, 1, 12, 512, 3000])
                    m.write16(target + offset, base + delta)
            for axis in range(3):
                m.write16(origin + axis * 4, rng.randrange(-1000, 1000))
                m.write16(direction + axis * 2, rng.choice([-2048, -512, 0, 512, 2047]))
            max_angle = 0 if case % 4 == 0 else (0x7FFF if case % 4 == 1 else 2048)
            return (origin, direction, max_angle)
        if name == "BreakableFinalizeVisualUpdate":
            _seed_spawn_group(m, 265, self.c.padding,
                              stack_depth=56)
            return (0x800D1000, 0x800D2000, case % 4)
        if name == "PlayerFxJumpBurst":
            for offset in range(0x24, 0x3C, 2):
                m.write16(0x800EC1E0 + offset, rng.getrandbits(16))
            return ()
        if name == "PlayerFxSpawn116":
            _seed_spawn_group(m, 278, self.c.padding,
                              stack_depth=56)
            return ()
        if name == "PlayerFxSetMatrixPos":
            source, matrix = 0x800D2000, 0x800D1000
            m.write32(0x800FC0DC, matrix)
            for index in range(0, 0x80, 2):
                m.write16(source + index, rng.getrandbits(16))
                if index < 0x20:
                    m.write16(matrix + index, rng.choice([0x1000, 0, 0x100, -0x100]))
            return (source,)
        raise AssertionError(name)

    def run(self, name, case, symbol=None):
        self.m.restore(self.base)
        self.m.gte_reset()
        self.m.hook_log.clear()
        self.indirect_target = None
        args = self.setup(name, case)
        self.c.camupd_hooks.stage = (
            C.cast(C.pointer(self.c.stage_hooks), VP)
            if name == "StageExitStart" else VP())
        self.c.camupd_hooks.camera = self.c.camera_hooks
        self.c.sync(self.m)
        self.c.events.clear()
        original_events = []

        def hook(name, cpu, regs):
            if name == "FadeUpdate":
                timer = mr.signed(cpu.read16(0x800F4482), 16)
                if timer >= 25:
                    timer = 24
                    cpu.write16(0x800F4482, timer)
                original_events.append(("RenderFadeSpiral", (timer, 0, 0, 0)))
                return 0
            args4 = tuple(value & 0xFFFFFFFF for value in regs[4:8])
            if name == "TrailRingDraw":
                args4 += (cpu.read32((regs[29] + 16) & 0xFFFFFFFF),)
            elif name == "HudDrawTimeRankIcon":
                args4 = tuple(regs[index] & 0xFFFFFFFF for index in (12, 5, 9, 6))
            if name in ("MatrixLookDir", "MatrixFromDirXZ"):
                args4 = self.c._normalize_hook(name, args4)
                output = regs[5]
                for index in range(16):
                    cpu.write16(output + index * 2, (0x100 + index * 7) & 0xFFFF)
            elif name == "ClearWords":
                address, value, size = (regs[index] & 0xFFFFFFFF for index in (4, 5, 6))
                for offset in range(size):
                    cpu.write8(address + offset, value & 0xFF)
                args4 = self.c._normalize_hook(name, args4)
            else:
                args4 = self.c._normalize_hook(name, args4)
            original_events.append((name, args4))
            value = self.c.hook_returns.get(name, 0)
            return value(*args4) if callable(value) else value

        def indirect_hook(target, cpu, regs):
            args4 = tuple(value & 0xFFFFFFFF for value in regs[4:8])
            original_events.append(("CallIndirect", (target,) + args4))
            return 0

        direct_calls = MIPS_DIRECT_CALLS.get(name, set())
        mips_hooks = {address: (lambda cpu, regs, n=hook_name: hook(n, cpu, regs))
                      for hook_name, address in MIPS_HOOKS.items()
                      if hook_name not in direct_calls and
                      not (name == "TrailRingDraw" and hook_name == "TrailRingDraw")}
        mips_patches = {}
        if name == "TrailRingDraw":
            def render_trail_segment(cpu, regs):
                segment_args = tuple(regs[index] & 0xFFFFFFFF
                                     for index in (17, 4, 5, 9, 6))
                original_events.append(("TrailRingDraw", segment_args))
                return 0x8001FB10
            mips_patches[0x8001FA48] = render_trail_segment
        if name == "HudDrawMonkeyInfo":
            def render_monkey_badge(cpu, regs):
                args4 = (cpu.read32(0x800F4350) & 0x10, 0, 0, 0)
                original_events.append(("RendererHudMonkeyBadge", args4))
                # The packet block's LUI also supplies the score divisor used
                # after the block. Preserve that live value across the hook.
                regs[2] = 0x51EB0000
                return 0x80060670
            mips_patches[0x800605C8] = render_monkey_badge
        if self.indirect_target is not None:
            target = self.indirect_target
            mips_hooks[target] = lambda cpu, regs, t=target: indirect_hook(t, cpu, regs)
        address = FUNCS[name][0]
        original = self.m.call(
            address,
            tuple(value & 0xFFFFFFFF for value in args),
            hooks=mips_hooks,
            patches=mips_patches,
            limit=2_000_000,
        )[2]
        fn = self.c.functions[name]
        if symbol is not None:
            mutant = getattr(self.c.lib, symbol)
            mutant.argtypes = fn.argtypes
            mutant.restype = fn.restype
            fn = mutant
        takes_hooks = FUNCS[name][5]
        hook_args = ([C.byref(self.c.spawn_hooks)] if name in SPAWN_FUNCS else
                     [C.byref(self.c.hooks)] if takes_hooks else [])
        c_args = [self.c.ram] + hook_args + list(args)
        result = fn(*c_args)
        self.counts[name] += 1

        if FUNCS[name][3] != "v" and (int(result) & 0xFFFFFFFF) != original:
            self.fail(name, case, f"args={args!r} return C={int(result) & 0xFFFFFFFF:08X} MIPS={original:08X}")
        expected = bytes(self.m.ram[:STACK_LO]) + bytes(self.m.ram[STACK_HI:])
        actual = bytes(self.c.main[:STACK_LO]) + bytes(self.c.main[STACK_HI:])
        if actual != expected:
            offset = next(i for i, (a, b) in enumerate(zip(actual, expected)) if a != b)
            address = 0x80000000 + offset
            actual_word = int.from_bytes(actual[offset:offset + 4], "little")
            expected_word = int.from_bytes(expected[offset:offset + 4], "little")
            self.fail(name, case,
                      f"RAM differs at {address:08X}: C={actual_word:08X} MIPS={expected_word:08X}")
        if bytes(self.c.scratch) != bytes(self.m.scratch):
            self.fail(name, case, "scratchpad differs")
        if original_events != self.c.events:
            self.fail(name, case, f"hook sequence C={self.c.events!r} MIPS={original_events!r}")
        if self.c.lib.ae_ram_failed(self.c.ram):
            self.fail(name, case, self.c.lib.ae_ram_error(self.c.ram).decode(errors="replace"))

    @staticmethod
    def fail(name, case, detail):
        raise SystemExit(f"FAIL {name} case {case}: {detail}")

    def finish(self, extra_coverage, default_run, show_coverage):
        items = [(name, addr, size, True)
                 for name, (addr, size, *_rest) in FUNCS.items()]
        items.extend((name, addr, size, False)
                     for name, (addr, size) in extra_coverage.items())
        summary = reg.coverage_summary(items, self.m.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        gaps = {name: info for name, info in summary.items()
                if info["own"] and
                (info["uncovered_count"] or info["stale_declared"])}
        if default_run:
            reg.write_results(self.counts, summary, not gaps, function_names=list(FUNCS))
        if gaps:
            for name, info in gaps.items():
                print(f"COVERAGE GAP {name}: uncovered={info['uncovered']} "
                      f"stale={info['stale_declared']}", flush=True)
            if default_run:
                raise SystemExit("FAIL: instruction coverage gaps remain")
        return summary


def _function_span(source, name):
    match = re.search(
        rf"(?m)^(?:void|uint32_t|int32_t)\s+(ae_{re.escape(name)})\s*\(",
        source)
    if match is None:
        raise AssertionError(f"missing ae_{name} for mutation")
    opening = source.index("{", match.start())
    depth = 1
    cursor = opening + 1
    while depth:
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
        cursor += 1
    return match.start(), opening, cursor - 1


def _missa_source_paths():
    paths = list((ROOT / "src").rglob("ae_*.c"))
    result = {}
    for name in FUNCS:
        if name in MENU_WRAPPERS:
            result[name] = ROOT / "src/ui/ae_missa_renderer.c"
            continue
        pattern = re.compile(
            rf"(?m)^(?:void|uint32_t|int32_t)\s+ae_{re.escape(name)}\s*\(")
        found = [path for path in paths
                 if pattern.search(path.read_text(encoding="utf-8"))]
        if len(found) != 1:
            raise AssertionError(f"expected one source for ae_{name}, found {found}")
        result[name] = found[0]
    return result


def run_mutations():
    """Compile 2 wrong-copy variants per function, each with an extra RAM store."""
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    function_sources = _missa_source_paths()
    by_source = {}
    for name, path in function_sources.items():
        by_source.setdefault(path, []).append(name)

    mutant_paths = {}
    mutant_addresses = {}
    mutation_index = 0
    try:
        for source_path, names in by_source.items():
            original = source_path.read_text(encoding="utf-8")
            clones = []
            for name in names:
                if name in MENU_WRAPPERS:
                    signature = (
                        f"int32_t ae_{name}(AeRam *r, const AeMissaHooks *hooks, "
                        "uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)\n")
                    body = (f"\n    return ae_{name}(r, hooks, a0, a1, a2, a3);\n")
                else:
                    start, opening, end = _function_span(original, name)
                    signature = original[start:opening]
                    body = original[opening + 1:end]
                for variant in range(2):
                    symbol = f"ae_mut_{name}_{variant}"
                    address = 0x80100000 + mutation_index * 4
                    mutation_index += 1
                    mutant_addresses[(name, variant)] = address
                    address_name = f"mutation_address_{mutation_index}"
                    mutated_body = (
                        f"\n    /* Wrong-copy mutation: unexpected store at {address:08X}. */\n"
                        f"    uint32_t {address_name} = 0x{address:08X}u;\n"
                        f"    ae_wr32(r, {address_name}, ae_rd32(r, {address_name}) ^ 1u);\n"
                        + body)
                    clone_signature = signature.replace(f"ae_{name}", symbol, 1)
                    clones.append(clone_signature + "{" + mutated_body + "}\n")

            relative = source_path.relative_to(ROOT / "src").as_posix()
            include = f'#include "../../src/{relative}"\n'
            mutant_path = out / f"check_missa_mutants_{source_path.stem}.c"
            mutant_path.write_text(include + "\n" + "\n".join(clones),
                                   encoding="utf-8")
            mutant_paths[str(source_path.relative_to(ROOT)).replace("\\", "/")] = mutant_path

        dll = build_dll(source_overrides=mutant_paths)
        harness = Harness(dll, SEED)
        caught = 0
        for name in FUNCS:
            for variant in range(2):
                address = mutant_addresses[(name, variant)]
                try:
                    harness.run(name, 0, symbol=f"ae_mut_{name}_{variant}")
                except SystemExit as error:
                    expected = f"RAM differs at {address:08X}"
                    if expected not in str(error):
                        print(f"MUTATION FAILED {name} variant={variant}: {error}",
                              flush=True)
                        return 1
                    caught += 1
                    print(f"mutation caught: {name} variant={variant}", flush=True)
                else:
                    print(f"MUTATION SURVIVED {name} variant={variant}", flush=True)
                    return 1
        print(f"mutations caught: {caught}/{2 * len(FUNCS)}", flush=True)
        return 0 if caught == 2 * len(FUNCS) else 1
    finally:
        for path in mutant_paths.values():
            Path(path).unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=int, default=DEFAULT_CASES)
    parser.add_argument("--seed", type=int, default=SEED)
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    args = parser.parse_args()
    if args.mutate:
        return run_mutations()
    h = Harness(build_dll(), args.seed)
    for name in FUNCS:
        for case in range(args.cases):
            h.run(name, case)
    summary = h.finish(EXTRA_COVERAGE,
                       args.cases == DEFAULT_CASES and args.seed == SEED,
                       args.coverage)
    print("cases per function:")
    for name, count in h.counts.items():
        info = summary[name]
        print(f"  {name:28s} {count:4d} cases  {info['executed']}/{info['total']} instructions")
    print(f"OK: {sum(h.counts.values())} differential cases, no mismatches")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
