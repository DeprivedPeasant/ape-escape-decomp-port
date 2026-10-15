"""Differential checks for the missb UI, progress, sound, and collider ports.

Every original entry runs on the retail EXE image. Area-dependent cases load real
members from several areas. Hardware and out-of-batch game calls use matching
named hooks on both sides; packet bytes remain native renderer state.

Usage: .venv/Scripts/python.exe tools/mips/check_missb.py [--only NAME ...]
       .venv/Scripts/python.exe tools/mips/check_missb.py --mutate
"""
from __future__ import annotations

import argparse
import ctypes as C
import os
import random
import re
import struct
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(ROOT / "tools"))
import mips_ram as mr  # noqa: E402
import dump_areas  # noqa: E402
import check_game as cg  # noqa: E402
import exclusion_registry as reg  # noqa: E402

M32 = 0xFFFFFFFF
DEFAULT_SEED = 0x4D495353
ORACLE_BUDGET = 2_000_000
STACK_LO, STACK_HI = 0x801FF000, 0x80200000
MUTATION_SELECTOR = 0x800B4DF0
MUTATION_MARKER = 0x800B4DF4

# name: (entry, byte size, C symbol, return kind, arguments after AeRam)
# Argument kinds: h = AeMissbHooks pointer, s = signed word, u = word/address.
FUNCS = {
    "HudTextWidth": (0x80082230, 0x04C, "ae_HudTextWidth", "s", ("u",)),
    "PauseDrawCaptureGrid": (0x80082398, 0x560, "ae_PauseDrawCaptureGrid", "v", ("h",)),
    "HudSetGeomOffsetThenDraw": (0x800828F8, 0x10C, "ae_HudSetGeomOffsetThenDraw", "v", ("h", "s", "s")),
    "PauseDrawCounter": (0x80082A04, 0x33C, "ae_PauseDrawCounter", "v", ("h",)),
    "PauseMenuInitVolumes": (0x80082D40, 0x02C, "ae_PauseMenuInitVolumes", "v", ()),
    "PauseMenuDrawSubScreen": (0x80082D6C, 0x590, "ae_PauseMenuDrawSubScreen", "s", ("h",)),
    "StringTableIndex_Main": (0x800832FC, 0x030, "ae_StringTableIndex_Main", "u", ("u", "s")),
    "ItemsClearFlags": (0x800847C4, 0x028, "ae_ItemsClearFlags", "v", ()),
    "ItemSaveCollectedMask": (0x800847EC, 0x0C4, "ae_ItemSaveCollectedMask", "v", ("h",)),
    "SoundSilence": (0x80086148, 0x028, "ae_SoundSilence", "s", ("h",)),
    "SoundStopAndRestoreVolume": (0x80086170, 0x0D8, "ae_SoundStopAndRestoreVolume", "s", ("h",)),
    "BgmFadeOut": (0x8008760C, 0x048, "ae_BgmFadeOut", "s", ("h",)),
    "SoundEmitterVolume": (0x80087BA4, 0x084, "ae_SoundEmitterVolume", "s", ("h", "u", "s", "s", "s")),
    "SoundSetStereo": (0x80087C64, 0x028, "ae_SoundSetStereo", "s", ("s",)),
    "ColliderScriptRiseToGround": (0x80088D78, 0x1B4, "ae_ColliderScriptRiseToGround", "s", ("h", "u", "s")),
    "ColliderScriptLiftUpdate": (0x80088F2C, 0x398, "ae_ColliderScriptLiftUpdate", "s", ("h", "u", "s")),
    "ColliderScriptRotateUpdate": (0x800895F0, 0x520, "ae_ColliderScriptRotateUpdate", "s", ("h", "u", "s")),
    "ColliderScriptOrbitPlayer": (0x80089B10, 0x254, "ae_ColliderScriptOrbitPlayer", "s", ("h", "u", "s")),
    "ColliderScriptNoOpA": (0x80089D64, 0x008, "ae_ColliderScriptNoOpA", "s", ("u", "s")),
    "ColliderScriptPropTransformUpdate": (0x80089D6C, 0x744, "ae_ColliderScriptPropTransformUpdate", "s", ("h", "u", "s")),
    "ColliderScriptNoOpB": (0x8008AA64, 0x008, "ae_ColliderScriptNoOpB", "s", ("u", "s")),
    "MessageBoxOpen": (0x8008DEEC, 0x034, "ae_MessageBoxOpen", "v", ()),
    "BodyInsideBox": (0x8008EC70, 0x08C, "ae_BodyInsideBox", "s", ("u", "u", "s", "u")),
    "SwingDoorBodyDelta": (0x8008F308, 0x160, "ae_SwingDoorBodyDelta", "s", ("u", "s")),
    "SlideDoorMove": (0x8008FAD0, 0x158, "ae_SlideDoorMove", "s", ("u", "s", "s")),
    "GimmickTouchCheck": (0x80090754, 0x1C8, "ae_GimmickTouchCheck", "s", ("s", "u", "s", "s")),
    "ColliderBoxContainsPoint": (0x8009091C, 0x08C, "ae_ColliderBoxContainsPoint", "s", ("u", "s", "s", "s")),
    "GimmickBreakableFloorAdjust": (0x80091598, 0x0F0, "ae_GimmickBreakableFloorAdjust", "s", ("h", "u", "u")),
    "GimmickApplyPropScale": (0x80091AF0, 0x114, "ae_GimmickApplyPropScale", "s", ("h", "u", "u")),
    "GimmickSetPropRotation": (0x80091C04, 0x0F4, "ae_GimmickSetPropRotation", "s", ("u", "s", "s")),
    "GimmickColliderContactUpdate": (0x80092324, 0x744, "ae_GimmickColliderContactUpdate", "s", ("h", "u", "u")),
    "GimmickGetGrabOffset": (0x80093848, 0x0D4, "ae_GimmickGetGrabOffset", "s", ("u", "u", "u")),
    "GimmickPointWithinExpandedBox": (0x80094330, 0x0AC, "ae_GimmickPointWithinExpandedBox", "s", ("u", "u", "s", "s")),
    "GimmickLinkedAreaActive": (0x80094A48, 0x098, "ae_GimmickLinkedAreaActive", "s", ("s",)),
    "GimmickMonkeyTouchingBox": (0x80094E5C, 0x0A8, "ae_GimmickMonkeyTouchingBox", "s", ("u",)),
    "StageType_Clear_Init": (0x80094F48, 0x0E8, "ae_StageType_Clear_Init", "s", ("h",)),
    "StageType_Clear_Update": (0x80095030, 0x214, "ae_StageType_Clear_Update", "s", ("h",)),
    "StageVictoryStart": (0x8009543C, 0x028, "ae_StageVictoryStart", "s", ("h",)),
    "StageIntroStart": (0x8009546C, 0x058, "ae_StageIntroStart", "s", ("h",)),
    "StageIntroUpdate": (0x800954C4, 0x0C4, "ae_StageIntroUpdate", "s", ("h",)),
    "PlayerEnterCutsceneMode": (0x80095588, 0x040, "ae_PlayerEnterCutsceneMode", "s", ("h",)),
    "ProgressChecksum": (0x800962B0, 0x038, "ae_ProgressChecksum", "s", ("u",)),
    "ProgressFlagSet": (0x800962F4, 0x038, "ae_ProgressFlagSet", "s", ("s",)),
    "ProgressSetByte": (0x80096360, 0x04C, "ae_ProgressSetByte", "s", ("s", "s")),
    "ProgressGetStageResult": (0x800964B0, 0x02C, "ae_ProgressGetStageResult", "s", ("s",)),
    "LevelMonkeyCountGet": (0x800965EC, 0x030, "ae_LevelMonkeyCountGet", "s", ("s",)),
    "LevelProgressRecordExit": (0x80096638, 0x07C, "ae_LevelProgressRecordExit", "s", ("s",)),
    "AreaMonkeyQuotaMet": (0x80097454, 0x0B0, "ae_AreaMonkeyQuotaMet", "s", ("h", "u")),
    "AreaAggregateActiveMonkeyCount": (0x80097504, 0x11C, "ae_AreaAggregateActiveMonkeyCount", "s", ("h",)),
    "GetDAT_800B38B8": (0x80097620, 0x010, "ae_GetDAT_800B38B8", "s", ()),
    "StageGoalReached": (0x80097A60, 0x19C, "ae_StageGoalReached", "s", ("h",)),
    "StageGradeUpdate": (0x80097BFC, 0x110, "ae_StageGradeUpdate", "s", ()),
    "LevelTableGet": (0x80097E60, 0x028, "ae_LevelTableGet", "u", ("s",)),
    "LevelCaughtCount": (0x80097FB0, 0x0D8, "ae_LevelCaughtCount", "s", ("s",)),
    "SaveBuildImage": (0x80098088, 0x0E4, "ae_SaveBuildImage", "s", ()),
    "SaveComputeChecksum": (0x8009816C, 0x1B4, "ae_SaveComputeChecksum", "s", ("u",)),
    "HudPopupStart": (0x80098330, 0x068, "ae_HudPopupStart", "v", ("s",)),
    "PlayerAuxModelInit": (0x80098FB0, 0x03C, "ae_PlayerAuxModelInit", "v", ("h", "u")),
    "GimmickCylinderSetPlayerFlag": (0x8009B3B0, 0x01C, "ae_GimmickCylinderSetPlayerFlag", "v", ("s",)),
    "SpecialActorPlaceAtCollider": (0x8009B578, 0x150, "ae_SpecialActorPlaceAtCollider", "v", ("h", "s", "s", "s")),
    "SeqSetMonoFlag": (0x800A2294, 0x00C, "ae_SeqSetMonoFlag", "v", ("s",)),
    "SoundBankSetVolume": (0x800A2640, 0x1B0, "ae_SoundBankSetVolume", "s", ("h", "s", "s")),
    "SeqTrackSetPlayState": (0x800A2B6C, 0x2D0, "ae_SeqTrackSetPlayState", "v", ("h", "s", "s")),
    "SeqFadeTrackOut": (0x800A2F54, 0x090, "ae_SeqFadeTrackOut", "s", ("h", "s", "s", "s")),
    "SoundStopAllVoices": (0x800A342C, 0x144, "ae_SoundStopAllVoices", "v", ("h", "s")),
    "SfxVoiceSetPitch": (0x800A35FC, 0x060, "ae_SfxVoiceSetPitch", "v", ("h", "s", "s")),
}

DEFAULT_CASES = {name: 3 for name in FUNCS}
DEFAULT_CASES.update({
    "SwingDoorBodyDelta": 8, "SlideDoorMove": 19,
    "PauseDrawCaptureGrid": 6, "PauseDrawCounter": 9,
    "PauseMenuDrawSubScreen": 12,
    "ColliderScriptRiseToGround": 8, "ColliderScriptLiftUpdate": 13,
    "ColliderScriptRotateUpdate": 13, "ColliderScriptOrbitPlayer": 12,
    "ColliderScriptPropTransformUpdate": 18, "BodyInsideBox": 5,
    "GimmickTouchCheck": 8, "GimmickBreakableFloorAdjust": 7,
    "GimmickApplyPropScale": 8, "GimmickSetPropRotation": 9,
    "GimmickColliderContactUpdate": 64,
    "GimmickGetGrabOffset": 6, "GimmickLinkedAreaActive": 6,
    "AreaMonkeyQuotaMet": 6, "AreaAggregateActiveMonkeyCount": 6,
    "StageType_Clear_Init": 4, "StageType_Clear_Update": 8,
    "StageIntroUpdate": 8, "StageGoalReached": 8, "StageGradeUpdate": 7,
    "LevelMonkeyCountGet": 6, "LevelTableGet": 6,
    "LevelCaughtCount": 6, "SaveComputeChecksum": 6,
    "SpecialActorPlaceAtCollider": 7, "SoundSilence": 5,
    "SoundStopAndRestoreVolume": 5, "SoundBankSetVolume": 6,
    "SeqTrackSetPlayState": 8, "SeqFadeTrackOut": 6,
    "SoundStopAllVoices": 5,
})

# Out-of-batch native or renderer calls intercepted at their original entries.
# The argument counts are the meaningful words compared with the C hook.
EXTERNALS = {
    0x800161C0: ("ModelTransformRigid", 3, "zero"),
    0x8001CE54: ("ModelDrawEnvMap", 3, "zero"),
    0x80032D38: ("PlayerPlaceAtEntry", 3, "zero"),
    0x8004BBF0: ("EventCamStartVictory", 0, "zero"),
    0x8004BC5C: ("EventCamStartDefault", 0, "zero"),
    0x8005F68C: ("HudDrawTimeRankIcon", 4, "zero"),
    0x80081F18: ("RendererHudDrawWindowFrame", 4, "renderer_silent"),
    0x8008211C: ("RendererDrawMenuArrows", 3, "renderer_menu_arrows"),
    0x8008332C: ("RendererPauseMenuDrawArrows", 3, "renderer_pause_menu_arrows"),
    0x80085E08: ("RendererAimReticleDrawCircle", 3, "renderer_silent"),
    0x8008BE4C: ("ColliderGroundProbe", 2, "ground_probe"),
    0x80097E88: ("AreaGetMonkeyRecord", 2, "monkey_records"),
    0x8009E4D8: ("SeqTrackChannelReset", 2, "zero"),
    0x8002E520: ("VSync", 1, "zero"),
    0x80011F6C: ("XaPlay", 4, "zero"),
    0x80086788: ("SoundLoopsSetPan", 4, "zero"),
    0x8013FB20: ("FUN_8013FB20", 4, "zero"),
    0x800A1398: ("SpuWaitIdle", 0, "wait_idle"),
    0x8009D79C: ("SpuWaitLoop", 1, "wait_loop"),
    0x8002B410: ("SetDrawTPage", 4, "draw_tpage"),
}

EVENT_NAMES = {
    1: "wait_idle", 2: "wait_loop", 3: "set_voice_adsr",
    4: "voice_key_off", 5: "printf_error", 6: "set_voice_volume",
    7: "SetDrawTPage", 8: "SoundLoopsSetPan", 9: "FUN_8013FB20",
}

# Only packet/OT stores in the retained logical functions and their nested
# renderer helpers are patched. Game-state stores, cursor movement, branches,
# text formatting, calls, and return values continue to execute on MIPS.
RENDERER_ENTRY_HOOKS = {
    "PauseDrawCaptureGrid": ("RendererPauseDrawCaptureGrid", 4),
    "PauseMenuDrawSubScreen": ("RendererPauseMenuDrawSubScreen", 4),
}

RENDERER_PACKET_STORES = {
    "PauseDrawCaptureGrid": (
        0x80082430, 0x80082444, 0x80082448, 0x8008244C, 0x80082464,
        0x80082468, 0x800824A4, 0x800824F4, 0x800824FC, 0x80082500,
        0x80082508, 0x8008250C, 0x80082548, 0x80082550, 0x80082554,
        0x80082560, 0x80082564, 0x8008258C, 0x80082590, 0x8008259C,
        0x800825A0,
    ),
    "PauseMenuDrawSubScreen": (
        0x80082DB8, 0x80082DC0, 0x80082DF8, 0x80082E20, 0x80082E2C,
        0x80082E3C, 0x80082E40, 0x80082E44, 0x80082E50, 0x80082E54,
        0x80082E7C, 0x80082E80, 0x80082E84, 0x80082E88, 0x80082E94,
        0x80082EA0, 0x80082EA4, 0x80082F08, 0x80082F0C, 0x80082F18,
        0x80082F1C,
    ),
}

# The store at 80082504 is in the delay slot of the packet-block jump at
# 80082500. The patch at the jump returns to its target and skips both words.
RENDERER_PACKET_DELAY_SLOTS = {
    "PauseDrawCaptureGrid": (0x80082504,),
}

# Packet-only helper bodies reached by the three logical functions. Their
# control flow and cursor updates still execute; only these writes to GPU
# primitives and ordering-table tags are intercepted.
NESTED_RENDERER_PACKET_STORES = {
    "PauseDrawCaptureGrid": (
        0x8008DABC, 0x8008DAC4, 0x8008DAD4, 0x8008DAF8, 0x8008DB04,
        0x8008DB18, 0x8008DB30, 0x8008DB44, 0x8008DB98, 0x8008DBA0,
        0x8008DBA8, 0x8008DBCC, 0x8008DBD8, 0x8008DBEC, 0x8008DC04,
        0x8008DC18, 0x8008DCA8, 0x8008DCAC, 0x8008DCB0, 0x8008DCB4,
        0x8008DCC0, 0x8008DCC8, 0x8008DCE4, 0x8008DCFC,
        0x8005FB14, 0x8005FB18, 0x8005FB1C, 0x8005FB20, 0x8005FB24,
        0x8005FB28, 0x8005FB30, 0x8005FB60, 0x8005FB6C, 0x8005FB70,
        0x8005FB84, 0x8005FB88, 0x8005FB8C, 0x8005FB90, 0x8005FB94,
        0x8005FB98, 0x8005FBA0, 0x8005FBD0, 0x8005FBDC, 0x8005FBE0,
        0x8005FBF8, 0x8005FBFC, 0x8005FC00, 0x8005FC04, 0x8005FC08,
        0x8005FC0C, 0x8005FC14, 0x8005FC44, 0x8005FC50, 0x8005FC54,
    ),
    "PauseDrawCounter": (
        0x8008DABC, 0x8008DAC4, 0x8008DAD4, 0x8008DAF8, 0x8008DB04,
        0x8008DB18, 0x8008DB30, 0x8008DB44, 0x8008DB98, 0x8008DBA0,
        0x8008DBA8, 0x8008DBCC, 0x8008DBD8, 0x8008DBEC, 0x8008DC04,
        0x8008DC18, 0x8008DCA8, 0x8008DCAC, 0x8008DCB0, 0x8008DCB4,
        0x8008DCC0, 0x8008DCC8, 0x8008DCE4, 0x8008DCFC,
    ),
    "PauseMenuDrawSubScreen": (
        0x80081E18, 0x80081E1C, 0x80081E74, 0x80081E80, 0x80081E84,
        0x80081E90, 0x80081E94, 0x80081EC4, 0x80081EC8, 0x80081ED4,
        0x80081ED8,
        0x8008DABC, 0x8008DAC4, 0x8008DAD4, 0x8008DAF8, 0x8008DB04,
        0x8008DB18, 0x8008DB30, 0x8008DB44, 0x8008DB98, 0x8008DBA0,
        0x8008DBA8, 0x8008DBCC, 0x8008DBD8, 0x8008DBEC, 0x8008DC04,
        0x8008DC18, 0x8008DCA8, 0x8008DCAC, 0x8008DCB0, 0x8008DCB4,
        0x8008DCC0, 0x8008DCC8, 0x8008DCE4, 0x8008DCFC,
    ),
}

# These HudDrawTime packet writes occupy MIPS branch delay slots. Intercept the
# branch, preserve its target/fallthrough value computation, and skip only the
# packet write because a delay-slot patch cannot be installed independently.
NESTED_RENDERER_PACKET_DELAY_BRANCHES = {
    0x8005FB2C: (0x8005FB30, 0x8005FB38, 9),
    0x8005FB9C: (0x8005FBA0, 0x8005FBA8, 8),
    0x8005FC10: (0x8005FC14, 0x8005FC1C, 12),
}

PROP_TRANSFORM_UNREACHABLE = (
    "ColliderScriptPropTransformUpdate 8008A198 8008A1A8..8008A1B0 "
    "8008A1D4 8008A1E4..8008A1EC 8008A210 8008A220..8008A228"
)

# These entries are named native renderer hooks, so their original bodies are
# not in FUNCS and have no port ledger rows. Keep their complete instruction
# spans registered as renderer-only, with the store audit recorded alongside.
PURE_RENDERER_HOOK_AUDITS = (
    "DrawMenuArrows 8008211C..800821D0",
    "PauseMenuDrawArrows 8008332C..80083464",
    "AimReticleDrawCircle 80085E08..80086038",
)


def u32(value):
    return int(value) & M32


def s16(value):
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def write_text(cpu, address, data):
    for index, value in enumerate(data):
        cpu.write8(address + index, value)


def install_area_ground(cpu, blob, area):
    """Install the real member's terrain pointers as AreaLoad does."""
    area_header = struct.unpack_from("<I", blob, 0x4D0)[0]
    grid = area_header + struct.unpack_from("<I", blob, area_header + 4)[0] * 4
    width, depth, color_count = struct.unpack_from("<HHI", blob, grid)
    cells = grid + 8
    heights = cells + width * depth * 12 + color_count * 4
    cpu.write32(0x800F44E4, mr.AREA_BASE + area_header)
    cpu.write16(0x800EBFA0, width)
    cpu.write16(0x800EBFA2, depth)
    cpu.write32(0x800EBFA8, mr.AREA_BASE + cells)
    cpu.write32(0x800EBFB4, mr.AREA_BASE + heights)
    cpu.write16(0x800F4476, area)


class CEvent(C.Structure):
    _fields_ = [("kind", C.c_uint32), ("values", C.c_uint32 * 6)]


GENERIC_CB = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_char_p,
                         C.c_uint32, C.POINTER(C.c_uint32), C.c_uint32)


class CPort:
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
        lib.ae_rd8.argtypes = [P, U]
        lib.ae_rd8.restype = U
        lib.ae_rd16.argtypes = [P, U]
        lib.ae_rd16.restype = U
        lib.ae_rd32.argtypes = [P, U]
        lib.ae_rd32.restype = U
        lib.ae_wr8.argtypes = [P, U, U]
        lib.ae_wr16.argtypes = [P, U, U]
        lib.ae_wr32.argtypes = [P, U, U]
        lib.ae_missb_test_prepare.argtypes = [P, GENERIC_CB, P, U, U, I, I]
        lib.ae_missb_test_hooks.restype = P
        lib.ae_missb_test_clear_events.argtypes = []
        lib.ae_missb_test_event_count_get.restype = U
        lib.ae_missb_test_event_get.argtypes = [U, C.POINTER(CEvent)]
        lib.ae_missb_test_event_get.restype = I
        lib.ae_missb_test_record_generic.argtypes = [U, U, C.POINTER(U)]

        self.ram = lib.ae_ram_create()
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(
            lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(
            lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.hooks = lib.ae_missb_test_hooks()
        self.callback = GENERIC_CB(self._generic_callback)
        self._callback_context = C.c_uint32(0)
        self._callback_log = []
        self.functions = {}
        for name, (_address, _size, symbol, result, kinds) in FUNCS.items():
            fn = getattr(lib, symbol)
            fn.argtypes = [P] + [P if kind == "h" else I if kind == "s" else U
                                 for kind in kinds]
            fn.restype = None if result == "v" else U if result == "u" else I
            self.functions[name] = fn

    def _generic_callback(self, _user, ram, c_name, address, args, argc):
        name = c_name.decode("ascii", errors="replace") if c_name else ""
        values = tuple(u32(args[i]) for i in range(min(int(argc), 4)))
        event_position = self.lib.ae_missb_test_event_count_get()
        self._callback_log.append((name, u32(address), values, event_position))
        if address != 0:
            self.lib.ae_missb_test_record_generic(address, argc, args)
        mode = EXTERNALS.get(u32(address), (None, 0, "zero"))[2]
        return self._hook_effect(mode, ram, values)

    def _hook_effect(self, mode, ram, args):
        if mode == "ground_probe" and len(args) >= 2:
            vector = args[1]
            self.lib.ae_wr16(ram, vector, 0)
            self.lib.ae_wr16(ram, vector + 2, 0)
            self.lib.ae_wr16(ram, vector + 4, 0)
            return 0
        if mode == "monkey_records" and len(args) >= 1:
            area = s16(args[0])
            records = 0x801E4000
            for index in range(8):
                self.lib.ae_wr8(ram, records + index,
                                2 if ((area + index) % 3) == 0 else 4)
            return records
        if mode == "draw_tpage" and len(args) >= 4:
            packet, semi, draw_page, tpage = args[:4]
            value = (0xE1000200 if draw_page else 0) | (tpage & 0x9FF)
            if semi:
                value |= 0x400
            self.lib.ae_wr8(ram, packet + 3, 1)
            self.lib.ae_wr32(ram, packet + 4, value)
        return 0

    def sync_from(self, snapshot):
        C.memmove(self.main, snapshot[0], mr.RAM_SIZE)
        C.memmove(self.scratch, snapshot[1], mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)

    def prepare(self, sp, caller_a3, caller_s1, caller_s2):
        self.lib.ae_missb_test_prepare(self.ram, self.callback, None, sp,
                                       caller_a3, caller_s1, caller_s2)
        self.lib.ae_missb_test_clear_events()
        self._callback_log.clear()

    def call(self, name, args):
        _address, _size, _symbol, _result, kinds = FUNCS[name]
        passed = [self.ram]
        values = iter(args)
        for kind in kinds:
            if kind == "h":
                passed.append(self.hooks)
                continue
            value = next(values)
            if kind == "s":
                passed.append(C.c_int32(value))
            else:
                passed.append(C.c_uint32(value))
        if next(values, None) is not None:
            raise AssertionError(f"extra C arguments for {name}: {args!r}")
        return self.functions[name](*passed)

    def events(self):
        raw_events = []
        lengths = {1: 0, 2: 1, 3: 3, 4: 2, 5: 5, 6: 3,
                   7: 4, 8: 4, 9: 4}
        for index in range(self.lib.ae_missb_test_event_count_get()):
            event = CEvent()
            if not self.lib.ae_missb_test_event_get(index, C.byref(event)):
                raise AssertionError("test shim lost a recorded event")
            values = tuple(u32(v) for v in event.values)
            if event.kind == 10:
                address, argc = values[:2]
                raw_events.append(("call", address, argc,
                                   values[2:2 + min(argc, 4)]))
            else:
                raw_events.append((EVENT_NAMES[event.kind],
                                   values[:lengths[event.kind]]))
        native_by_position = {}
        for name, address, args, position in self._callback_log:
            if address == 0:
                native_by_position.setdefault(position, []).append(
                    ("native", name, args))
        result = []
        for position in range(len(raw_events) + 1):
            result.extend(native_by_position.get(position, ()))
            if position < len(raw_events):
                result.append(raw_events[position])
        return result


def _mips_event(address, argc, regs):
    return ("call", address, argc,
            tuple(u32(regs[4 + i]) for i in range(argc)))


def _write_tpage(cpu, args):
    packet, semi, draw_page, tpage = args
    value = (0xE1000200 if draw_page else 0) | (tpage & 0x9FF)
    if semi:
        value |= 0x400
    cpu.write8(packet + 3, 1)
    cpu.write32(packet + 4, value)


def mips_hooks(cpu, events):
    hooks = {}
    for address, (name, argc, mode) in EXTERNALS.items():
        def callback(machine, regs, addr=address, call_name=name,
                     count=argc, effect=mode):
            values = tuple(u32(regs[4 + i]) for i in range(count))
            if effect in ("renderer_menu_arrows", "renderer_pause_menu_arrows"):
                events.append(("native", call_name, values))
                cursor = machine.read32(0x800EC354)
                if effect == "renderer_menu_arrows":
                    cursor += 40
                else:
                    bars = s16(values[2])
                    packets = 2 + ((bars + 7) // 8 if bars > 0 else 0)
                    cursor += packets * 36
                machine.write32(0x800EC354, cursor)
                return 0
            if effect not in ("wait_idle", "wait_loop", "draw_tpage", "renderer_silent") and addr not in (0x80086788, 0x8013FB20):
                events.append(_mips_event(addr, count, regs))
            if effect == "ground_probe" and len(values) >= 2:
                vector = values[1]
                machine.write16(vector, 0)
                machine.write16(vector + 2, 0)
                machine.write16(vector + 4, 0)
                return 0
            if effect == "monkey_records" and values:
                area = s16(values[0])
                records = 0x801E4000
                for index in range(8):
                    machine.write8(records + index,
                                   2 if ((area + index) % 3) == 0 else 4)
                return records
            if effect == "draw_tpage" and len(values) >= 4:
                _write_tpage(machine, values[:4])
                events.append(("SetDrawTPage", values[:4]))
                return 0
            if effect == "wait_idle":
                events.append(("wait_idle", ()))
                return 0
            if effect == "wait_loop":
                events.append(("wait_loop", values))
                return 0
            if addr == 0x80086788:
                events.append(("SoundLoopsSetPan", values))
                return 0
            if addr == 0x8013FB20:
                events.append(("FUN_8013FB20", values))
                return 0
            return 0
        hooks[address] = callback

    # MeshCalcEnvUVs is a separately verified renderer boundary. Its C port
    # writes a zero result for every vertex when the native callback is absent.
    def mesh_env_uv(machine, regs):
        instance = regs[4]
        mesh = machine.read32(instance)
        count = machine.read16(mesh + 6)
        output = 0x800EA060
        for _ in range(count):
            machine.write16(output + 4, 0)
            output += 8
        return 0
    hooks[0x80016D60] = mesh_env_uv
    return hooks


class SpuWrites:
    """Turn original SPU stores into the native sound callback events."""
    def __init__(self, cpu, events):
        self.cpu, self.events, self.write = cpu, events, cpu.write
        self.adsr = {}

    def __call__(self, address, size, value):
        shadow_base = self.cpu.read32(0x800E1874)
        if size == 2 and address in (shadow_base + 0x18C, shadow_base + 0x18E):
            offset = address - shadow_base
            self.events.append(("voice_key_off", ((offset - 0x18C) // 2, value & 0xFFFF)))
            self.write(address, size, value)
            return
        if size == 2 and shadow_base <= address < shadow_base + 24 * 16:
            offset = address - shadow_base
            if offset % 16 in (0, 2):
                self.events.append(("set_voice_volume",
                                    (offset // 16, (offset % 16) // 2, value & 0xFFFF)))
                self.write(address, size, value)
                return
        if 0x1F801C00 <= address < 0x1F801E00:
            offset = address - 0x1F801C00
            if size != 2:
                raise AssertionError(f"unexpected SPU write width {size} at {address:08X}")
            value &= 0xFFFF
            if offset in (0x18C, 0x18E):
                self.events.append(("voice_key_off", ((offset - 0x18C) // 2, value)))
            elif offset % 16 == 8:
                self.adsr[offset // 16] = value
            elif offset % 16 == 10:
                self.events.append(("set_voice_adsr",
                                    (offset // 16, self.adsr.pop(offset // 16), value)))
            elif offset % 16 in (0, 2):
                self.events.append(("set_voice_volume",
                                    (offset // 16, (offset % 16) // 2, value)))
            else:
                raise AssertionError(f"unexpected SPU halfword write {address:08X}={value:04X}")
            return
        self.write(address, size, value)


def _ram_write_setup(cpu, address, value, size=4):
    {1: cpu.write8, 2: cpu.write16, 4: cpu.write32}[size](address, value)


def common_setup(cpu, area, index):
    state = 0x800F4350
    player = 0x800EC1E0
    props = 0x801E2000
    collider = 0x801E1000
    records = 0x801E4000
    actor = 0x801E5000
    # Current stage and player data for the area dependent ports.
    cpu.write16(state + 0x124, 5 if index % 2 == 0 else 24)
    cpu.write16(state + 0x126, area)
    cpu.write16(state + 0x128, 3)
    cpu.write16(state + 0x120, 19 if index % 5 == 0 else 1)
    cpu.write16(state + 0x122, 3 if index % 3 == 0 else 0)
    cpu.write16(state + 0x12A, area)
    cpu.write16(state + 0x12C, 0x180 + index)
    cpu.write16(state + 0x12E, index % 3)
    cpu.write16(state + 0x166, [0, 3, 9, 20, 40, 80][index % 6])
    cpu.write16(state + 0x186, [2, 5, 12, 16, 30, 64][index % 6])
    cpu.write16(state + 0x188, [1, 4, 10, 15, 25, 60][index % 6])
    cpu.write32(0x800F4470, 0 if index % 2 == 0 else 0x13)
    cpu.write16(0x800F4474, 5 if index % 2 == 0 else 24)
    cpu.write16(0x800F4476, area)
    cpu.write32(0x800EC350, index * 5 + 3)
    cpu.write16(0x800BF670, -1)
    cpu.write16(0x800BF674, -1)
    cpu.write16(0x800BF676, -1)

    cpu.write32(0x800B4D64, props)
    cpu.write32(0x800B4D70, 0)
    cpu.write32(state + 0x20C, records)
    cpu.write16(state + 0x284, 2)
    cpu.write16(state + 0x290, 0)
    for i in range(2):
        body = records + i * 56
        cpu.write16(body, 20 + i * 40)
        cpu.write16(body + 2, 0)
        cpu.write16(body + 4, -10 + i * 50)
        cpu.write16(body + 6, 24)
        cpu.write16(body + 8, 48)
        cpu.write16(body + 10, 24)
        cpu.write8(body + 0x12, 1)
    cpu.write32(0x800F455C, records)

    cpu.write32(0x800B4DC4, actor)
    cpu.write32(0x800F5A20 + 0x70, 0)
    cpu.write16(player + 0x24, 300)
    cpu.write16(player + 0x26, 0)
    cpu.write16(player + 0x28, 300)
    cpu.write32(player + 0x24, 300)
    cpu.write32(player + 0x28, 120)
    cpu.write32(player + 0x2C, 300)
    cpu.write32(player + 0x34, 0)
    cpu.write32(player + 0x44, 20)
    for i in range(8):
        monkey = 0x800E5500 + i * 0x250
        cpu.write8(monkey + 0x7A, (2, 3, 4, 0)[(i + index) % 4])
        cpu.write32(monkey + 0x24, 500 + i * 50)
        cpu.write32(monkey + 0x28, 10)
        cpu.write32(monkey + 0x2C, 500 + i * 30)

    # Safe prop/collider records used by the scenarios below.
    for i in range(8):
        prop = props + i * 0x20
        cpu.write32(prop, 0x801E6000 + i * 0x100)
        cpu.write16(prop + 6, 0x1000)
        cpu.write16(prop + 8, 0)
        cpu.write16(prop + 0x10, 0)
        cpu.write16(prop + 0x12, 0)
        cpu.write16(prop + 0x14, 0)
        cpu.write32(prop + 0x1C, 0x801E7000 + i * 0x40)
        cpu.write32(0x801E6000 + i * 0x100, 3)

    cpu.write16(collider, 0)
    cpu.write16(collider + 2, 0)
    cpu.write16(collider + 4, 0)
    cpu.write16(collider + 6, 80)
    cpu.write16(collider + 8, 64)
    cpu.write16(collider + 0x0A, 80)
    cpu.write16(collider + 0x0E, 0)
    cpu.write16(collider + 0x12, 1)
    cpu.write16(collider + 0x18, 0)
    cpu.write16(collider + 0x1C, 0)
    cpu.write16(collider + 0x24, 0)
    cpu.write16(collider + 0x26, 0)
    cpu.write16(collider + 0x28, 0)
    cpu.write16(collider + 0x32, 80)
    cpu.write16(collider + 0x34, 96)
    cpu.write16(collider + 0x36, 80)

    # Stage quota row; preserve the actual level and area tables from the EXE.
    for i in range(256):
        cpu.write16(0x800DFFE0 + i * 2, (i + index) % 9)

    # Sound banks, sequence, voice/channel records and shadow buffers.
    bank_state = 0x800BF670
    cpu.write16(bank_state, 0)
    cpu.write16(bank_state + 4, 1)
    cpu.write16(bank_state + 6, 0)
    cpu.write16(bank_state + 0x10, 90)
    cpu.write16(bank_state + 0x16, 0)
    cpu.write32(0x800E1874, 0x801E9000)
    cpu.write32(0x800E1870, 0x801E8000)
    for bank_index in range(8):
        bank = 0x800E1180 + bank_index * 12
        header = 0x801E8000 + bank_index * 0x1000
        cpu.write32(bank, 1)
        cpu.write32(bank + 4, header)
        cpu.write32(header + 0x10, 0)
        cpu.write32(header + 0x20, 0x100)
        cpu.write32(header + 0x100, 0)
        for tone in range(24):
            rec = header + 0x110 + tone * 16
            cpu.write8(rec + 3, 64)
            cpu.write8(rec + 14, 0)
    for track in range(24):
        record = 0x800F5AC0 + track * 0x120
        cpu.write8(record + 0x20, 0)
        cpu.write8(record + 0x24, 1)
        cpu.write8(record + 0x25, 1 if index & 1 else 0)
        cpu.write32(record + 0x10, 0x801EA000 + track * 0x100)
        cpu.write32(record + 0x18, 0)
        cpu.write16(record + 0xDE, 1 << (track & 15))
        cpu.write16(record + 0xE0, 1 << (track & 15))
        cpu.write16(record + 0x1A, 1 if track % 5 == 0 else 0)
        cpu.write8(record + 0x26, 1 if track % 3 == 0 else 0)
        shadow = 0x801E9000 + track * 16
        cpu.write16(shadow + 8, 1)
        cpu.write16(shadow + 10, 1)
    for channel_index in range(24):
        channel = 0x800E07F0 + channel_index * 0x66
        cpu.write16(channel + 6, channel_index % 4)
        cpu.write16(channel + 0x1A, 1 if (channel_index + index) % 3 == 0 else 0)
        cpu.write16(channel + 8, 0)

    # Text and renderer state use the real game string data with safe OT cursors.
    cpu.write32(0x800EC354, 0x801EB000)
    cpu.write32(0x800EC35C, index % 3)
    cpu.write32(0x800F03D0 + (index % 3) * 0x4078, 0x801EB100)
    cpu.write32(0x800EC358, 0x801E3000)
    cpu.write32(0x801E3000, 0x801E3100)
    cpu.write16(0x801E3106, 0)
    cpu.write32(0x800F4D00, 0x801E3200)
    cpu.write32(0x800BF0A0, 0x801E3300)
    cpu.write32(0x800E7D00, 0x801E3400)


def setup_case(cpu, name, index, area):
    common_setup(cpu, area, index)
    collider = 0x801E1000
    point = 0x801E1100
    half = 0x801E1120
    door = 0x801E1200
    gimmick = 0x801E1300
    prop = 0x801E2000
    out = 0x801E1400
    i = index
    sign_values = (-32768, -100, -1, 0, 1, 32767)

    if name == "HudTextWidth":
        data = (b"\x01\x02\x03\x00", b"\x10\x0d\x20", b"\x7f\xff\x00")[i % 3]
        write_text(cpu, 0x801E1500, data)
        for glyph in range(256): cpu.write8(0x800B2734 + glyph, (glyph + i) & 0xFF)
        return (0x801E1500,)
    if name == "PauseDrawCaptureGrid":
        total = (0, 1, 8, 9, 16, 24)[i]
        cpu.write16(0x800F4350 + 0x186, total)
        cpu.write16(0x800F4350 + 0x166, min(total, (0, 1, 4, 9, 12, 0)[i]))
        cpu.write16(0x800F4350 + 0x188, min(total, (0, 0, 5, 10, 15, 24)[i]))
        cpu.write16(0x800F4350 + 0x124, (7, 14, 5, 17, 24, 29)[i])
        cpu.write16(0x800F4350 + 0x18A, (5, 10, 12, 76, 20, 24)[i])
        cpu.write32(0x800F4350 + 0x148, (-1, 300, 3599, 3601, 7212, -1)[i])
        cpu.write32(0x800F4350 + 0x14C, (0, 299, 3600, 3602, 7211, 1)[i])
        cpu.write32(0x800F4350 + 0x144, (0, 59, 3600, 3661, 7201, 0)[i])
        cpu.write8(0x800F4350 + 0x2CC, i)
        cpu.write8(0x800F4350 + 0x2CD, 5 - i)
        cpu.write32(0x800F4350 + 4, 0x801EB200)
        cpu.write32(0x800F4350 + 8, 0x801EB400)
        if i == 0:
            # Cover the completed time-attack result path used by area 19.
            cpu.write8(0x800DFDD0, 0x10)
        elif i == 1:
            # A saved level status and grade 2 take the alternate grid-cell path.
            cpu.write8(0x800DFC70 + 14, 1)
            cpu.write16(0x800F4350 + 0x12E, 2)
        elif i == 2:
            # Give level 5 seven collected areas to exercise the >= 6 digit path.
            cpu.write8(0x800DFC70 + 5, 1)
            cpu.write16(0x800F4350 + 0x12E, 2)
            cpu.write16(0x800B3858 + 5 * 2, 30)
            cpu.write8(0x80102400 + 5 * 16, 7)
            for entry in range(7):
                cpu.write8(0x800DFB70 + entry * 2, 30 + entry)
            cpu.write8(0x800DFB70 + 7 * 2, 0xFF)
        return ()
    if name == "HudSetGeomOffsetThenDraw": return ((276, 180), (320, 112), (0, 0), (1, -1))[i]
    if name == "PauseDrawCounter":
        cpu.write16(0x800F44BA, (0, 9, 10, 99, 123)[i % 5])
        cpu.write16(0x800F4350 + 0x166, 2 + i)
        cpu.write16(0x800F4350 + 0x124, 5)
        cpu.write32(0x800F4350 + 4, 0x801EB200)
        cpu.write32(0x800F4350 + 8, 0x801EB400)
        progress_items = 0x800DFC98
        for byte in range(25):
            cpu.write8(progress_items + byte, 0)
        if i == 1:
            cpu.write8(progress_items, 4)
        elif i == 2:
            cpu.write8(progress_items, 200)
            cpu.write8(progress_items + 1, 200)
        elif i == 3:
            for byte in range(25):
                cpu.write8(progress_items + byte, 232)
        elif i == 4:
            for byte in range(32):
                cpu.write8(0x800DFC70 + byte, 0)
            cpu.write8(0x800DFC88, 1)
            cpu.write16(0x800DFB70, 0xFFFF)
        return ()
    if name == "PauseMenuInitVolumes": return ()
    if name == "PauseMenuDrawSubScreen":
        cpu.write32(0x800BEF80, i % 3)
        cpu.write16(0x800BEF74, i % 4)
        cpu.write16(0x800BEF76, i % 5)
        selections = (0, 1, 2, 3, 4, 2, 3, 4, 3, 3, 5, 2)
        pressed = (0x00F0, 0xA0F0, 0x2000, 0x8000,
                   0x00F0, 0xB000, 0x4000, 0x0800,
                   0x2000, 0x8000, 0, 0x8000)
        sfx = (0, 1, 64, 127, 80, 127, 0, 75, 64, 0, 80, 64)[i]
        bgm = (127, 80, 0, 40, 100, 64, 127, 75, 64, 64, 80, 0)[i]
        cpu.write32(0x800BEF90, selections[i])
        cpu.write32(0x800BEF94, sfx)
        cpu.write32(0x800BEF98, bgm)
        cpu.write32(0x800E5478, pressed[i])
        cpu.write32(0x800E5480, pressed[i])
        cpu.write16(0x800F4350 + 0x138, i & 1)
        cpu.write16(0x800F4350 + 0x13A, (i // 2) & 1)
        cpu.write16(0x800BF680, sfx)
        cpu.write16(0x800BF67E, bgm)
        return ()
    if name == "StringTableIndex_Main":
        write_text(cpu, 0x801E1500, b"a\0bc\0d\0e\0")
        return (0x801E1500, (0, 1, 2, 3, -1)[i % 5])
    if name == "PauseMenuDrawArrows": return (12 + i, 24 + i, 100 - i)
    if name == "ItemsClearFlags": return ()
    if name == "ItemSaveCollectedMask":
        cpu.write16(0x800F4350 + 0x124, 5)
        cpu.write16(0x800F4350 + 0x126, area)
        for item in range(32):
            cpu.write8(0x800BEFA0 + item * 0x18 + 0x0A,
                       0 if (item + i) % 3 == 0 else 1)
            cpu.write8(0x800BF2D8 + item, 1 if (item + i) & 1 else 0)
        return ()
    if name in ("SoundSilence", "SoundStopAndRestoreVolume", "BgmFadeOut"):
        if name == "SoundStopAndRestoreVolume" and i == 3:
            cpu.write16(0x800F4470, 9)
        return ()
    if name == "SoundEmitterVolume": return ((0x801E1000, -1, 32, 2), (0, 0, 127, 1), (0, 3, 100, -1))[i % 3]
    if name == "SoundSetStereo": return (i & 1,)
    if name in ("ColliderScriptRiseToGround", "ColliderScriptLiftUpdate",
                "ColliderScriptRotateUpdate", "ColliderScriptOrbitPlayer",
                "ColliderScriptPropTransformUpdate"):
        lift_heights = (2500, 3073, 1200, 0, 1000, 800)
        cpu.write16(collider + 2, lift_heights[i % 6] if name == "ColliderScriptLiftUpdate"
                    else (2500, 2900, 1200, 0, -200)[i % 5])
        cpu.write16(collider + 0x0E, i % 3)
        cpu.write16(collider + 0x12, 1)
        cpu.write16(collider + 0x18, i & 1)
        cpu.write16(collider + 0x1C, 8 + i)
        cpu.write16(collider + 0x24, 0)
        cpu.write16(collider + 0x26, 0)
        cpu.write16(collider + 0x28, 0)
        cpu.write16(0x801E1300 + 8, (0, 1, 2, 3, 4, 5)[i % 6])
        cpu.write16(0x801E1300 + 0x10, 24 + i)
        cpu.write16(0x801E1300 + 0x16, 20 + i)
        cpu.write32(0x800B3388, i)
        cpu.write16(0x800EC1E0 + 0x70, 0)
        if name == "ColliderScriptLiftUpdate":
            phase_x = (0, 0, 24, 16, 0, 8)[i % 6]
            phase_z = (0, 0, -20, 12, 0, -8)[i % 6]
            cpu.write16(collider + 0x34, phase_x)
            cpu.write16(collider + 0x36, phase_z)
            if i == 3:
                cpu.write16(collider + 0x32, -1)
            if i == 5:
                cpu.write32(0x800EC1E0 + 0x70, 0x70)
                cpu.write16(0x800EC1E0 + 0x30, 60)
                cpu.write16(0x800EC1E0 + 0x38, -40)
            if i in (6, 7, 8):
                cpu.write16(collider + 0x32, 1)
                cpu.write32(0x800B3388, 1)
                cpu.write32(0x800EC1E0 + 0x70, 0x70)
                cpu.write16(0x800EC1E0 + 0x30, 48 + i)
                cpu.write16(0x800EC1E0 + 0x38, -32 + i)
                cpu.write16(0x800F4350 + 0x126, (11, 15, 101)[i - 6])
            elif i >= 9:
                probe_phases = ((256, 256), (-256, 256),
                                (512, -512), (-1024, -512))
                phase_x, phase_z = probe_phases[i - 9]
                cpu.write16(collider + 6, 300)
                cpu.write16(collider + 0x0A, 300)
                cpu.write16(collider, (0, 512, -512)[(i - 9) % 3])
                cpu.write16(collider + 4, (0, -512, 512)[(i - 9) % 3])
            if i >= 9:
                cpu.write16(collider + 0x34, phase_x)
                cpu.write16(collider + 0x36, phase_z)
            if i == 12:
                cpu.write16(collider + 2, -2000)
                cpu.write16(collider + 6, 80)
                cpu.write16(collider + 0x0A, 80)
                cpu.write16(collider, 0)
                cpu.write16(collider + 4, 0)
                cpu.write16(collider + 0x34, 12000)
                cpu.write16(collider + 0x36, 0)
        if name == "ColliderScriptRiseToGround":
            if i == 4:
                cpu.write16(collider + 2, -200)
            elif i == 5:
                cpu.write16(collider + 2, 4000)
            elif i == 6:
                cpu.write16(collider + 2, 4000)
                cpu.write16(collider + 0x0E, -1)
                cpu.write16(collider + 0x32, 0)
            if i == 5:
                cpu.write16(collider + 2, 3000)
                cpu.write16(0x1F8003F0, -1)
                cpu.write16(0x1F8003F8, 1)
            if i == 6:
                cpu.write16(collider + 2, 0)
            elif i == 7:
                cpu.write16(collider + 2, 4000)
                cpu.write16(collider + 0x0E, 0)
        if name == "ColliderScriptRotateUpdate":
            rotate_geometry = (0x1105, 0x0204, 0x0105, 0x1205,
                               0x0304, 0x1103)
            geometry = rotate_geometry[(i - 6) % len(rotate_geometry)] if i >= 6 else 1
            cpu.write16(collider + 0x12, geometry)
            cpu.write16(collider + 0x22, -1 if i % 3 != 2 else 1)
            cpu.write16(collider + 0x1C, (8, 12, 16, 24, 32, 40)[i % 6])
            cpu.write16(collider + 0x0C, (0, 4, 8, 12, 20, 31)[i % 6])
            cpu.write16(collider + 0x1A, (80, 120, -96, 160, 40, -64)[i % 6])
            cpu.write16(collider + 0x32, (0, 1, 4, 8, 15, 0)[i % 6])
            cpu.write16(collider + 0x34, (8, -12, 16, 24, 32, -40)[i % 6])
            cpu.write16(collider + 0x36, (0, 1, -1, 2, -2, 3)[i % 6])
            cpu.write16(collider + 0x0E, (0, 1, 2, -1, 3, 4)[i % 6])
            if i == 6:
                cpu.write16(collider + 0x1C, 8)
                cpu.write16(collider + 0x0C, 2)
                cpu.write16(collider + 0x1A, -96)
                cpu.write16(collider + 0x32, 6)
                cpu.write16(collider + 0x34, 8)
            elif i == 8:
                cpu.write16(collider + 0x1C, 16)
                cpu.write16(collider + 0x0C, 16)
                cpu.write16(collider + 0x32, 16)
                cpu.write16(collider + 0x34, 16)
            elif i == 9:
                cpu.write16(collider + 0x1C, 8)
                cpu.write16(collider + 0x0C, 0)
                cpu.write16(collider + 0x0E, 0)
            elif i == 10:
                cpu.write16(collider + 0x1C, -1)
                cpu.write16(collider + 0x0C, 0)
            elif i == 11:
                cpu.write16(collider + 0x0E, 0)
            elif i == 12:
                cpu.write16(collider + 0x1C, 8)
                cpu.write16(collider + 0x0C, 0)
                cpu.write16(collider + 0x1A, -96)
                cpu.write16(collider + 0x32, 0)
                cpu.write16(collider + 0x34, -1)
                cpu.write16(collider + 0x0E, 0)
            if i >= 6:
                cpu.write32(0x800EC1E0 + 0x58, area)
                cpu.write32(0x800F5A20 + 0x58, area)
        if name == "ColliderScriptOrbitPlayer":
            orbit_geometries = (0x0001, 0x1001, 0x2001, 0x3001,
                                0x4001, 0x5001)
            cpu.write16(collider + 0x12,
                        orbit_geometries[(i - 6) % 6] if i >= 6 else 1)
            cpu.write16(collider + 0x0C, (0, 32, 128, 512, -128, -512)[i % 6])
            cpu.write16(collider + 0x1C, (16, 32, 64, -16, -32, 128)[i % 6])
            cpu.write16(collider + 0x24, (0, 100, -100, 256, -256, 512)[i % 6])
            cpu.write32(0x800EC1E0 + 0x24,
                        (300, 300, -300, -300, 0, 512)[i % 6])
            cpu.write32(0x800EC1E0 + 0x2C,
                        (300, -300, 300, -300, 512, 0)[i % 6])
            if i == 6:
                cpu.write16(collider + 0x0C, 1)
                cpu.write16(collider + 0x1C, -1)
        if name == "ColliderScriptPropTransformUpdate":
            prop_case = i % 6
            prop_variant = i // 6
            prop_types = (8, 14, 1, 1, 12, 1)
            prop_actions = (0, 0, 59, 89, 0, 0)
            prop_type = prop_types[prop_case]
            collider_action = prop_actions[prop_case]
            record = 0x801E5000
            cpu.write16(collider + 0x0E, 0)
            cpu.write16(collider + 0x10, collider_action)
            cpu.write16(collider + 0x12, prop_type)
            cpu.write16(collider + 0x22, 0)
            cpu.write16(collider + 0x2A, 64)
            cpu.write16(collider + 0x2C, 96)
            cpu.write16(collider + 0x2E, 80)
            cpu.write16(collider + 6, 100)
            cpu.write16(collider + 8, 140)
            cpu.write16(collider + 0x0A, 120)
            cpu.write16(collider, 10 + prop_case)
            cpu.write16(collider + 2, 20 + prop_case)
            cpu.write16(collider + 4, 30 + prop_case)
            cpu.write32(0x800F4350 + 0x214, record)
            target_position = ((80, 120, -40), (120, 160, 40),
                               (40, 80, -80), (-40, -80, 80),
                               (200, 40, -120), (-120, 200, 120))[prop_case]
            record_size = (0x1000, 0x0C00, 0x1400) if prop_type == 8 else (80, 120, 64)
            duration = 0 if prop_variant == 2 and prop_case == 5 else 8
            for offset, value in ((0, target_position[0]),
                                  (2, target_position[1]),
                                  (4, target_position[2]),
                                  (6, record_size[0]), (8, record_size[1]),
                                  (10, record_size[2]), (12, duration), (14, 1)):
                cpu.write16(record + offset, value)
            model_data = 0x801E5300
            cpu.write32(record + 0x1C, model_data)
            cpu.write16(model_data + 6, 80)
            cpu.write16(model_data + 8, 96)
            cpu.write16(model_data + 10, 64)
            cpu.write32(0x800B4D64, 0x801E2000)
            cpu.write32(0x801E2000, 0x801E2100)
            cpu.write32(0x801E2000 + 0x1C, 0x801E2200)
            cpu.write32(0x801E2100, 3)
            cpu.write16(0x800F4350 + 0x150, 6 if prop_type == 14 else 5)
            cpu.write16(0x800EC1E0 + 0x58, area)
            cpu.write16(0x800F5A20 + 0x58, area)
            if prop_variant == 1:
                for actor_address, direction in ((0x800EC1E0, 1),
                                                 (0x800F5A20, -1)):
                    for offset, center in ((0x24, 10 + prop_case),
                                           (0x28, 20 + prop_case),
                                           (0x2C, 30 + prop_case)):
                        cpu.write32(actor_address + offset,
                                    center + direction * 200)
            elif prop_variant == 2:
                for actor_address, direction in ((0x800EC1E0, -1),
                                                 (0x800F5A20, 1)):
                    for offset, center in ((0x24, 10 + prop_case),
                                           (0x28, 20 + prop_case),
                                           (0x2C, 30 + prop_case)):
                        cpu.write32(actor_address + offset,
                                    center + direction * 200)
            return (collider, area)
        return (collider, i)
    if name in ("ColliderScriptNoOpA", "ColliderScriptNoOpB"): return (collider, i)
    if name == "MessageBoxOpen": return ()
    if name == "BodyInsideBox":
        cpu.write32(point, 90 if i == 4 else 100 + i)
        cpu.write32(point + 4, (0, 20, 100, 300, -100)[i])
        cpu.write32(point + 8, 20 + i)
        cpu.write16(collider + 0x24, 100)
        cpu.write16(collider + 0x26, 100)
        cpu.write16(collider + 0x28, 100)
        cpu.write16(collider + 8, 100)
        cpu.write16(half, 100)
        cpu.write16(half + 2, 100)
        return (point, collider, 0, half)
    if name == "ColliderBoxContainsPoint":
        cpu.write16(collider, 0)
        cpu.write16(collider + 2, 0)
        cpu.write16(collider + 4, 0)
        cpu.write16(collider + 6, 100)
        cpu.write16(collider + 8, 100)
        cpu.write16(collider + 0x0A, 100)
        return (collider, (0, 100, -100, 101, -101, 0)[i],
                (0, 100, -100, -1, -101, 101)[i],
                (0, -100, 100, 0, -101, 101)[i])
    if name == "GimmickTouchCheck":
        cpu.write16(collider, 0)
        cpu.write16(collider + 2, 0)
        cpu.write16(collider + 4, 0)
        cpu.write16(collider + 6, 150)
        cpu.write16(collider + 8, 100)
        cpu.write16(collider + 0x0A, 150)
        cpu.write32(0x800F5A20 + 0x70, 1 if i % 2 else 0)
        cpu.write32(0x800F5A20 + 0x24, 40)
        cpu.write32(0x800F5A20 + 0x28, 0)
        cpu.write32(0x800F5A20 + 0x2C, 40)
        cpu.write32(0x800F4350 + 0x20C, 0x801E4000)
        cpu.write16(0x800F4350 + 0x284, 2)
        if i >= 6:
            cpu.write32(0x800F5A20 + 0x70, 1 if i == 6 else 0)
            if i == 6:
                cpu.write32(0x800F5A20 + 0x24, 500)
            cpu.write16(0x800F4350 + 0x284, 2)
            for body_index in range(2):
                body = 0x801E4000 + body_index * 56
                cpu.write16(body, 0 if body_index == 0 else -500)
                cpu.write16(body + 2, -1)
                cpu.write16(body + 4, -500 - body_index * 20)
                cpu.write16(body + 6, 24)
                cpu.write16(body + 8, 24)
                cpu.write16(body + 10, 24)
                cpu.write8(body + 0x12, 1)
            for monkey_index in range(8):
                monkey = 0x800E5500 + monkey_index * 0x250
                cpu.write8(monkey + 0x7A, 0)
            if i == 7:
                monkey = 0x800E5500
                cpu.write8(monkey + 0x7A, 4)
                cpu.write32(monkey + 0x24, 0)
                cpu.write32(monkey + 0x28, -1)
                cpu.write32(monkey + 0x2C, 0)
        if i == 4:
            for body_index in range(2):
                body = 0x801E4000 + body_index * 56
                cpu.write16(body, 500 + body_index * 20)
                cpu.write16(body + 4, 500 + body_index * 20)
        return (1 if i >= 6 else i & 1, collider, 0, 0)
    if name in ("SwingDoorBodyDelta", "SlideDoorMove"):
        cpu.write16(door + 0x0E, -1 if name == "SlideDoorMove" and i == 0 else i % 3)
        cpu.write16(door, (1, 3, 20, 101, 0, 2, 3, 20)[i % 8])
        cpu.write16(door + 6, 40 + i)
        axes = (60, 61, 62, 63, 64, 65, 60, 63)
        cpu.write16(door + 8, axes[i % 8] if name == "SwingDoorBodyDelta" else 60 + i)
        if name == "SlideDoorMove":
            directions = (0, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
                          10, 11, 12, 13, 14, 15, 16)
            return (door, directions[i], 4 + i)
        return (door, i % 4)
    if name in ("GimmickBreakableFloorAdjust", "GimmickApplyPropScale"):
        cpu.write16(collider + 2, 100 + i * 8)
        cpu.write16(collider + 0x0E, i % 4)
        cpu.write16(collider + 0x32, 0x1000 + i * 128)
        cpu.write16(collider + 0x34, 0x1000 + i * 64)
        cpu.write16(collider + 0x36, 0x1000 - i * 96)
        cpu.write16(gimmick + 4, i % 3)
        cpu.write16(gimmick + 0x16, 16 + i)
        cpu.write16(prop + 6, (0x1000, 0x800, 0x1800, -0x1000, 0x400, 0x2000)[i % 6])
        cpu.write16(prop + 8, i % 3)
        if name == "GimmickBreakableFloorAdjust" and i == 6:
            cpu.write16(collider + 2, -500)
            cpu.write16(gimmick + 0x16, -1000)
        if name == "GimmickApplyPropScale" and i == 6:
            cpu.write16(prop + 6, -0x1000)
            cpu.write16(prop + 8, 1)
            cpu.write16(collider + 0x36, 300)
        if name == "GimmickApplyPropScale" and i == 7:
            cpu.write16(prop + 6, -0x1000)
            cpu.write16(prop + 8, 2)
            cpu.write16(collider + 0x34, 300)
        return (collider, gimmick) if name == "GimmickBreakableFloorAdjust" else (collider, prop)
    if name == "GimmickSetPropRotation":
        directions = (0, 1, 2, 3, 4, 8, 5, 6, 7)
        amounts = sign_values[i] if i < len(sign_values) else 512
        return directions[i], i % 4, amounts
    if name == "GimmickColliderContactUpdate":
        case_index = i % 8
        variant = i // 8
        orientation = case_index
        cpu.write16(gimmick + 8, orientation)
        cpu.write16(gimmick + 0x10, 32 + case_index)
        angle = (0, 128, 256, 511, 512, 1024, 2048, 4095)[case_index]
        if variant == 1:
            angle = (angle + 1024) & 0xFFF
        elif variant == 2:
            angle = 256
        elif variant == 3:
            angle = 512
        elif variant == 4:
            angle = 2560
        elif variant in (5, 6, 7):
            angle = 256
        cpu.write16(gimmick + 0x16, angle)
        extent = 400 if variant in (3, 7) else 160
        vertical_extent = 800 if variant in (3, 7) else 120
        cpu.write16(collider + 0x32, extent)
        cpu.write16(collider + 0x34, vertical_extent)
        cpu.write16(collider + 0x36, extent)
        cpu.write16(collider + 0x06, 180)
        cpu.write16(collider + 0x0A, 180)
        cpu.write16(collider + 0x24, 0)
        cpu.write16(collider + 0x26, 0)
        cpu.write16(collider + 0x28, 0)
        cpu.write16(0x800EC1E0 + 0x44, 24)
        player_x = (0, 100, 1000, -100, 100, -100, 100, -100)[case_index]
        player_z = (0, 100, -100, 100, 100, -100, 100, -100)[case_index]
        if variant == 1 or variant == 2:
            player_x, player_z = 0, 0
        elif variant == 3:
            player_x, player_z = 1000, -1000
        elif variant == 5:
            player_x, player_z = -100, -100
        elif variant == 6:
            player_x, player_z = ((-500, 500) if case_index < 4
                                  else (500, -500))
        elif variant == 7:
            player_x, player_z = 1000, -1000
        cpu.write32(0x800EC1E0 + 0x24, player_x)
        cpu.write32(0x800EC1E0 + 0x28,
                    -64 if variant in (2, 5, 6) else 100)
        cpu.write32(0x800EC1E0 + 0x2C, player_z)
        cpu.write32(0x800EC1E0 + 0x34, 0)
        return (gimmick, collider)
    if name == "GimmickGetGrabOffset":
        cpu.write32(0x800EC1E0 + 0x24, (0, 1, -1, 100, -100, 0)[i])
        cpu.write32(0x800EC1E0 + 0x2C, (0, 1, -1, 100, -100, 0)[(i + 2) % 6])
        if i >= 4:
            cpu.write32(0x800EC1E0 + 0x28, 0)
            cpu.write32(0x800EC1E0 + 0x24, 1 if i == 4 else 0)
            cpu.write32(0x800EC1E0 + 0x2C, 0 if i == 4 else 1)
        return (i % 4, collider, out)
    if name == "GimmickPointWithinExpandedBox":
        cpu.write16(collider, 0)
        cpu.write16(collider + 2, 0)
        cpu.write16(collider + 4, 0)
        cpu.write16(collider + 6, 40)
        cpu.write16(collider + 8, 80)
        cpu.write16(collider + 0x0A, 40)
        cpu.write32(point, (0, 40, 41, -40, -41, 0)[i])
        cpu.write32(point + 4, (0, 10, -80, 80, 0, 1)[i])
        cpu.write32(point + 8, (0, 40, -41, -40, 0, 1)[i])
        return (collider, point, (0, 4, 0, -4, 20, -20)[i],
                (0, 4, -4, 0, 20, -20)[i])
    if name == "GimmickLinkedAreaActive":
        head = 0x801E1800
        next_node = 0x801E1820
        cpu.write32(0x800B4D70, head if i & 1 else next_node)
        cpu.write32(0x800F4350 + 0x220, head)
        cpu.write16(0x800EC238, area)
        for record, kind, linked_area, next_index in (
                (head, 11, area + 1, 1),
                (head + 24, 11, area, 2),
                (head + 48, 0, area + 1, -1),
                (head + 72, 0, area, -1)):
            cpu.write16(record, kind)
            cpu.write16(record + 4, linked_area)
            cpu.write16(record + 0x0A, next_index)
        return ((0, 2, 3)[i % 3],)
    if name == "GimmickMonkeyTouchingBox":
        cpu.write16(collider, 0)
        cpu.write16(collider + 2, 0)
        cpu.write16(collider + 4, 0)
        cpu.write16(collider + 6, 300)
        cpu.write16(collider + 8, 300)
        cpu.write16(collider + 0x0A, 300)
        if i in (0, 1):
            monkey = 0x800E5500
            cpu.write8(monkey + 0x7A, 4)
            cpu.write32(monkey + 0x24, 0 if i == 0 else -1)
            cpu.write32(monkey + 0x28, -1)
            cpu.write32(monkey + 0x2C, 0 if i == 0 else -1)
        return (collider,)
    if name == "StageType_Clear_Init":
        cpu.write16(0x800F4350 + 0x124, (5, 31, 33, 24, 5, 33)[i])
        cpu.write16(0x800F4350 + 0x126, (area, 78, 86, area, area, area)[i])
        cpu.write16(0x800F4350 + 0x122, i % 4)
        cpu.write16(0x800F4470, 0x13 if i % 2 else 0)
        return ()
    if name == "StageType_Clear_Update":
        cpu.write16(0x800B37E4, (0, 2, 3, 1, -1, 2, 3, 4)[i])
        cpu.write16(0x800B37E6, (0, 90, 90, 0, 180, 2, 0, 270)[i])
        cpu.write16(0x800EC1E0 + 0x100, (0, 1, 90, 2, 0, 1, 3, 90)[i])
        cpu.write16(0x800B38B8, 1 if i == 2 else i % 2)
        return ()
    if name in ("StageVictoryStart", "StageIntroStart", "StageIntroUpdate",
                "PlayerEnterCutsceneMode"):
        cpu.write16(0x800B37E0, (0x50, 0, 1, 2, 3, 0xFFFF)[i % 6])
        intro_state = (0, 2, 2, 4, 4, 0)[i % 6]
        if name == "StageIntroUpdate" and i == 6: intro_state = 1
        if name == "StageIntroUpdate" and i == 7: intro_state = 3
        cpu.write16(0x800B37E2, intro_state)
        cpu.write16(0x800F4350 + 0x12C, 0x100 + i)
        return ()
    if name == "ProgressChecksum":
        for off in range(25): cpu.write8(0x801E1500 + off, (off + i * 3) & 0xFF)
        return (0 if i == 0 else 0x801E1500,)
    if name == "ProgressFlagSet": return ((-1, 0, 1, 31, 32, 0x1F)[i],)
    if name == "ProgressSetByte": return ((0, 1, 4, 257, -1, 0x3FFFFF)[i], (0x1234, -1, 256, 0, 0xA55A, 0x100)[i])
    if name == "ProgressGetStageResult":
        cpu.write32(0x800DFDD0, 0xA1B2C3D4 + i)
        return (19 if i % 2 == 0 else area,)
    if name in ("LevelMonkeyCountGet", "LevelCaughtCount", "LevelTableGet"):
        level = (0, 5, 17, 24, 29, 30)[i]
        if name == "LevelCaughtCount":
            cpu.write8(0x80102400 + level * 16, 3)
        return (level,)
    if name == "LevelProgressRecordExit":
        cpu.write16(0x800F4350 + 0x12E, i % 3)
        cpu.write8(0x800F4350 + 0x12E, (0, 1, 2, 3, 255, 5)[i])
        return ((0, 5, 24, 29, 30, 17)[i],)
    if name in ("AreaMonkeyQuotaMet", "AreaAggregateActiveMonkeyCount"):
        cpu.write16(0x800F4350 + 0x126, area)
        cpu.write16(0x800F4350 + 0x128, (1, 2, 3, 4, 5, 6)[i])
        for slot in range(8):
            cpu.write8(0x800E5500 + slot * 0x250 + 0x7A,
                       (2, 3, 4, 0, 2, 3, 0, 4)[(slot + i) % 8])
        return (out,) if name == "AreaMonkeyQuotaMet" else ()
    if name in ("GetDAT_800B38B8", "SaveBuildImage"): return ()
    if name == "StageGradeUpdate":
        levels = (17, 18, 18, 24, 30, 5, 5)
        caught_values = (4, 4, 9, 6, 12, 7, 4)
        goals = (5, 5, 5, 5, 10, 5, 5)
        alternate_goals = (9, 8, 8, 8, 12, 8, 8)
        cpu.write16(0x800F4350 + 0x120, 11)
        cpu.write16(0x800F4350 + 0x122, 3 if i == 6 else 0)
        cpu.write16(0x800F4350 + 0x124, levels[i])
        cpu.write16(0x800F4350 + 0x166, caught_values[i])
        cpu.write16(0x800F4350 + 0x188, goals[i])
        cpu.write16(0x800F4350 + 0x186, alternate_goals[i])
        cpu.write16(0x800F4350 + 0x12E, 7)
        return ()
    if name == "StageGoalReached":
        cpu.write16(0x800F4350 + 0x120, (19, 1, 1, 1, 1, 1)[i % 6])
        cpu.write16(0x800F4350 + 0x124, (5, 22, 17, 24, 5, 30)[i % 6])
        cpu.write16(0x800F4350 + 0x12E, (0, 1, 2, 2, 3, 0)[i % 6])
        cpu.write16(0x800F4350 + 0x166, (5, 1, 2, 7, 4, 20)[i % 6])
        cpu.write16(0x800F4350 + 0x188, (5, 3, 5, 8, 4, 12)[i % 6])
        if 1 <= i <= 3:
            cpu.write32(0x800DFDD8, 0x05050505)
        if i == 6:
            cpu.write16(0x800F4350 + 0x120, 1)
            cpu.write16(0x800F4350 + 0x124, 5)
            cpu.write16(0x800F4350 + 0x12E, 1)
            cpu.write16(0x800F4350 + 0x166, 3)
            cpu.write16(0x800F4350 + 0x186, 5)
        if i == 7:
            cpu.write16(0x800F4350 + 0x120, 1)
            cpu.write16(0x800F4350 + 0x124, 5)
            cpu.write16(0x800F4350 + 0x12E, 2)
            cpu.write16(0x800F4350 + 0x166, 8)
            cpu.write16(0x800F4350 + 0x186, 4)
        return ()
    if name == "SaveComputeChecksum":
        image = 0x801E6000
        for off in range(0x700): cpu.write8(image + off, 0)
        for level in range(25):
            cpu.write8(image + 0x28 + level, (level + i) % 3)
            cpu.write32(image + 0x690 + level * 4, i % 4)
            table = 0x80102400 + level * 16
            cpu.write16(table + 8, 3)
            cpu.write16(table + 10, 2)
            cpu.write16(table + 12, 1)
        cpu.write8(image + 0x46, i % 3)
        cpu.write32(image + 0x668, (0x1010, 0x10, 0x1000, 0)[i % 4])
        cpu.write32(image + 0x670, (0x50505, 0x50500, 0x5, 0)[i % 4])
        cpu.write8(image + 0x674, i % 6)
        return (image,)
    if name == "HudPopupStart": return ((0, 1, 2, 3, 255)[i],)
    if name == "PlayerAuxModelInit": return (0x800E07D0,)
    if name == "GimmickCylinderSetPlayerFlag": return (i,)
    if name == "SpecialActorPlaceAtCollider":
        kind = 2 if i == 6 else (0, 1, 4, 8, 12, 16)[i % 6]
        angle = (0, 512, 1024, 1536, 2048, 4095)[i % 6]
        return (kind, i % 4, angle)
    if name == "SeqSetMonoFlag": return ((0, 1, -1)[i],)
    if name == "SoundBankSetVolume":
        bank, volume = ((0, 1, 2, 3, -1, 127)[i], (0, 64, 127, 128, -1, 96)[i])
        for voice_index in range(3):
            voice = 0x800E07F0 + voice_index * 0x66
            cpu.write16(voice, 1)
            cpu.write16(voice + 4, 0)
            cpu.write16(voice + 6, voice_index)
            cpu.write16(voice + 0x1A, 1)
            cpu.write16(voice + 0x22, bank if voice_index < 2 else bank + 1)
        return (bank, volume)
    if name == "SeqTrackSetPlayState":
        track = i % 4
        record = 0x800F5AC0 + track * 0x120
        cpu.write8(record + 0x20, 0)
        cpu.write8(record + 0x24, 1)
        cpu.write8(record + 0x25, (0, 1, 0, 0, 1, 1, 1, 0)[i])
        for channel_index in range(24):
            channel = 0x800E07F0 + channel_index * 0x66
            cpu.write16(channel + 6, track if channel_index < 3 else 0x7FFF)
            cpu.write16(channel + 0x1A, 0)
        return (track, (0, 1, 2, 3, -1, 2, 3, 4)[i])
    if name == "SeqFadeTrackOut":
        track = i % 4
        cpu.write8(0x800F5AC0 + track * 0x120 + 0x25, 1)
        return (track, (0, 1, 90, 255, 256, -1)[i],
                (0, 1, 64, 127, 128, -1)[i])
    if name == "SoundStopAllVoices": return (i & 1,)
    if name == "SfxVoiceSetPitch": return (i % 24, (0, 1, -1, 0x4000, 0x7FFF, -32768)[i])
    raise KeyError(f"missing scenario for {name}")


def caller_context(name, index):
    caller_a3 = (0x12345678 + index * 0x1021) & M32
    caller_s1 = (-700 + index * 13)
    caller_s2 = (900 - index * 17)
    if name == "GimmickColliderContactUpdate":
        caller_a3 = (0, 1, -1, 32, 64, 128, 256, 0x7FFF)[index % 8]
        caller_s1 = (-100, 100, 200, -200, 300, -300, 0, 50)[index % 8]
        caller_s2 = (50, -50, 100, -100, 200, -200, 0, 75)[index % 8]
    return caller_a3, caller_s1, caller_s2


def c_events(port):
    return port.events()


def compare_case(cpu, port, name, expected, actual, c_error, label,
                 compare_events=True):
    issues = []
    ret_kind = FUNCS[name][3]
    if c_error:
        issues.append("C AeRam error: " + c_error)
    if ret_kind != "v" and u32(expected[2]) != u32(actual):
        issues.append(f"return differs: MIPS={u32(expected[2]):08X}, C={u32(actual):08X}")
    if compare_events:
        mips_events = expected[3]
        native_events = c_events(port)
        # Keep native requests which replace packet blocks or named renderer
        # calls in the original body. Other C-side renderer callbacks have no
        # matching MIPS call site because their packet instructions were patched.
        root_hook = RENDERER_ENTRY_HOOKS.get(name, (None, 0))[0]
        matched_renderer_hooks = {root_hook}
        if name == "PauseMenuDrawSubScreen":
            matched_renderer_hooks.update({"RendererDrawMenuArrows",
                                           "RendererPauseMenuDrawArrows"})
        native_events = [event for event in native_events
                         if event[0] != "native" or event[1] in matched_renderer_hooks]
        if mips_events != native_events:
            issues.append(f"hook/hardware events differ: MIPS={mips_events!r}, C={native_events!r}")

    c_ram = bytes(port.main[:STACK_LO - 0x80000000])
    mips_ram = bytes(cpu.ram[:STACK_LO - 0x80000000])
    if mips_ram != c_ram:
        ram_diffs = []
        for offset, (left, right) in enumerate(zip(mips_ram, c_ram)):
            if left != right:
                ram_diffs.append((offset, left, right))
                if len(ram_diffs) >= 8:
                    break
        issues.append("RAM differs: " + ", ".join(
            f"{0x80000000 + off:08X} MIPS={m:02X} C={c:02X}"
            for off, m, c in ram_diffs))
    c_scratch = bytes(port.scratch)
    if bytes(cpu.scratch) != c_scratch:
        diffs = [(i, a, b) for i, (a, b) in enumerate(zip(cpu.scratch, c_scratch)) if a != b][:8]
        issues.append("scratch differs: " + ", ".join(
            f"{mr.SCRATCH_BASE + off:08X} MIPS={a:02X} C={b:02X}" for off, a, b in diffs))
    return issues


def oracle_run(cpu, name, args, caller_a3, caller_s1, caller_s2, events):
    """The single MIPS execution gateway always passes ORACLE_BUDGET explicitly."""
    spec = FUNCS[name]
    hooks = mips_hooks(cpu, events)
    patches = {}
    renderer_inputs = {}
    for pc in RENDERER_PACKET_STORES.get(name, ()):
        def skip_packet(machine, regs, patch_pc=pc):
            if name == "PauseDrawCaptureGrid" and patch_pc == 0x8008258C:
                stage = 0x800F4350
                values = (
                    u32(s16(machine.read16(stage + 0x186))),
                    u32(s16(machine.read16(stage + 0x166))),
                    u32(s16(machine.read16(stage + 0x188))),
                    u32(machine.read16(stage + 0x124)),
                )
                events.append(("native", "RendererPauseDrawCaptureGrid", values))
            elif name == "PauseMenuDrawSubScreen" and patch_pc in (
                    0x80082DF8, 0x80082E20):
                renderer_inputs["x"] = s16(regs[2])
            elif name == "PauseMenuDrawSubScreen" and patch_pc == 0x80082E2C:
                renderer_inputs["y"] = s16(regs[2])
            elif name == "PauseMenuDrawSubScreen" and patch_pc == 0x80082E3C:
                values = (
                    u32(regs[19]), u32(regs[4]),
                    u32(renderer_inputs.get("x", 0)),
                    u32(renderer_inputs.get("y", 0)),
                )
                events.append(("native", "RendererPauseMenuDrawSubScreen", values))
            if name == "PauseDrawCaptureGrid" and patch_pc == 0x80082500:
                return 0x80082510
            return patch_pc + 4
        patches[pc] = skip_packet
    nested_delays = {delay for delay, _target, _reg in
                     NESTED_RENDERER_PACKET_DELAY_BRANCHES.values()}
    for branch_pc, (delay_pc, target_pc, source_reg) in \
            NESTED_RENDERER_PACKET_DELAY_BRANCHES.items():
        if delay_pc not in NESTED_RENDERER_PACKET_STORES.get(name, ()):
            continue
        def skip_nested_delay(machine, regs, branch=branch_pc, target=target_pc,
                              source=source_reg):
            raw_value = regs[source] & M32
            signed_value = raw_value if raw_value < 0x80000000 else raw_value - 0x100000000
            if signed_value < 0:
                regs[2] = u32(signed_value + 3)
            return target
        patches[branch_pc] = skip_nested_delay
    for pc in NESTED_RENDERER_PACKET_STORES.get(name, ()):
        if pc in nested_delays:
            continue
        patches[pc] = lambda _machine, _regs, patch_pc=pc: patch_pc + 4
    initial_regs = {7: caller_a3, 17: caller_s1 & M32, 18: caller_s2 & M32}
    old_backend = os.environ.get("MIPS_BACKEND")
    if name in {"SoundSilence", "SoundStopAndRestoreVolume", "SeqTrackSetPlayState",
                "SoundStopAllVoices", "SfxVoiceSetPitch", "BgmFadeOut",
                "SoundEmitterVolume", "SoundBankSetVolume"}:
        os.environ["MIPS_BACKEND"] = "py"
    elif old_backend is None:
        os.environ["MIPS_BACKEND"] = "c"
    write_proxy = None
    if os.environ.get("MIPS_BACKEND") == "py":
        write_proxy = SpuWrites(cpu, events)
        cpu.write = write_proxy
    try:
        regs = cpu.call(spec[0], args, hooks=hooks, patches=patches,
                        limit=ORACLE_BUDGET,
                        sp=mr.DEFAULT_SP, initial_regs=initial_regs)
        return regs
    finally:
        if write_proxy is not None:
            cpu.write = write_proxy.write
        if old_backend is None:
            os.environ.pop("MIPS_BACKEND", None)
        else:
            os.environ["MIPS_BACKEND"] = old_backend


def renderer_packet_exclusion(name):
    pcs = set(RENDERER_PACKET_STORES.get(name, ()))
    pcs.update(RENDERER_PACKET_DELAY_SLOTS.get(name, ()))
    pcs.update(NESTED_RENDERER_PACKET_STORES.get(name, ()))
    pcs.update(branch for branch, (delay, _target, _reg) in
               NESTED_RENDERER_PACKET_DELAY_BRANCHES.items()
               if delay in NESTED_RENDERER_PACKET_STORES.get(name, ()))
    return f"{name} " + " ".join(f"{pc:08X}" for pc in sorted(pcs))


def build_dll(overrides=None, tag="missb"):
    required = [
        "src/ui/ae_missb_ui.c", "src/world/ae_missb_boxes.c",
        "src/world/ae_missb_colliders.c", "src/world/ae_missb_game.c",
        "src/world/ae_missb_gimmicks.c", "src/world/ae_missb_items.c",
        "src/world/ae_missb_progress.c", "src/sound/ae_missb_sound.c",
        "tools/mips/missb_shim.c",
    ]
    original = cg.SOURCES[:]
    try:
        for source in required:
            if source not in cg.SOURCES:
                cg.SOURCES.append(source)
        return cg.build_dll(overrides=overrides, tag=tag)
    finally:
        cg.SOURCES[:] = original


def load_corpus(cpu):
    member_map = dump_areas.area_members()
    area_ids = [area for area in (1, 2, 5, 19, 88, 99, 103) if area in member_map]
    if len(area_ids) < 4:
        raise SystemExit(f"expected multiple real area members, found {area_ids}")
    blobs = {area: (ROOT / f"extract/members/{member_map[area]:03X}.bin").read_bytes()
             for area in area_ids}
    print("real area members: " + ", ".join(
        f"{area:02X}->{member_map[area]:03X}" for area in area_ids), flush=True)
    cpu.coverage = set()
    cpu.attach_gte_oracle()
    return cpu.snapshot(), blobs, area_ids


def run_default(args):
    started = time.time()
    dll = build_dll()
    port = CPort(dll)
    cpu = mr.MipsRam.from_extract()
    base, blobs, area_ids = load_corpus(cpu)
    reg.declare("stack_window", f"{STACK_LO:08X}..{STACK_HI:08X}")
    reg.declare("unreachable", PROP_TRANSFORM_UNREACHABLE)
    for audit in PURE_RENDERER_HOOK_AUDITS:
        reg.declare("unreachable", audit)
    for name in sorted(set(RENDERER_PACKET_STORES) |
                       set(NESTED_RENDERER_PACKET_STORES)):
        reg.declare("unreachable", renderer_packet_exclusion(name))
    counts = {name: 0 for name in FUNCS}
    failures = 0
    selected = args.only or list(FUNCS)
    for name in selected:
        count = max(1, round(DEFAULT_CASES[name] * args.cases / 100))
        for index in range(count):
            area = area_ids[(index + list(FUNCS).index(name)) % len(area_ids)]
            if name == "ColliderScriptLiftUpdate" and index == 12:
                area = area_ids[0]
            if name == "ColliderScriptRiseToGround" and index == 5:
                area = area_ids[0]
            cpu.restore(base)
            cpu.load_area(blobs[area])
            if name == "ColliderScriptLiftUpdate" and index == 12:
                install_area_ground(cpu, blobs[area], area)
            if name == "ColliderScriptRiseToGround" and index == 5:
                install_area_ground(cpu, blobs[area], area)
                area_header = struct.unpack_from("<I", blobs[area], 0x4D0)[0]
                grid = area_header + struct.unpack_from("<I", blobs[area], area_header + 4)[0] * 4
                width, depth, _color_count = struct.unpack_from("<HHI", blobs[area], grid)
                center = (depth // 2) * width + width // 2
                center_cell = mr.AREA_BASE + grid + 8 + center * 4
                cpu.write16(center_cell + 2, 1)
            if hasattr(cpu, "gte_reset"):
                cpu.gte_reset()
            cpu.hook_log.clear()
            case_args = setup_case(cpu, name, index, area)
            caller_a3, caller_s1, caller_s2 = caller_context(name, index)
            initial = cpu.snapshot()
            mips_events = []
            try:
                mips_regs = oracle_run(cpu, name, case_args, caller_a3,
                                       caller_s1, caller_s2, mips_events)
                mips_state = cpu.snapshot()
                expected = (mips_state, mips_regs, mips_events)
                port.sync_from(initial)
                port.prepare(mr.DEFAULT_SP, caller_a3, caller_s1, caller_s2)
                result = port.call(name, case_args)
                c_error = (port.lib.ae_ram_error(port.ram).decode(errors="replace")
                           if port.lib.ae_ram_failed(port.ram) else "")
                problems = compare_case(cpu, port, name,
                                        (expected[0], expected[1], expected[1][2], expected[2]),
                                        result, c_error, f"{name}[{index}]")
                if problems:
                    failures += 1
                    print(f"FAIL {name}[{index}] area={area:02X}: " + "\n  ".join(problems), flush=True)
                else:
                    counts[name] += 1
            except Exception as error:  # continue collecting evidence across the target table
                failures += 1
                print(f"FAIL {name}[{index}] area={area:02X}: {type(error).__name__}: {error}", flush=True)
            if index + 1 == count:
                print(f"{name}: {counts[name]}/{count} cases", flush=True)

    items = [(name, FUNCS[name][0], FUNCS[name][1], True)
             for name in selected]
    summary = reg.coverage_summary(items, cpu.coverage)
    if args.coverage or args.cases == 100 and not args.only:
        reg.print_coverage(summary)
    gaps = [(name, row["uncovered"], row["stale_declared"])
            for name, row in summary.items()
            if row["own"] and (row["uncovered"] or row["stale_declared"])]
    if gaps:
        for name, uncovered, stale in gaps:
            if uncovered: print(f"COVERAGE GAP {name}: {uncovered}", flush=True)
            if stale: print(f"STALE EXCLUSION {name}: {stale}", flush=True)
        failures += len(gaps)
    default_run = (not args.only and args.cases == 100 and args.seed == DEFAULT_SEED
                   and not args.mutate)
    if default_run:
        result_path = reg.write_results(
            counts, summary, failures == 0,
            extra={"real_areas": area_ids, "mutations_required": len(FUNCS) * 2,
                   "oracle_instruction_budget": ORACLE_BUDGET,
                   "elapsed_seconds": round(time.time() - started, 2)},
            function_names=list(FUNCS))
        if result_path:
            print(f"wrote {result_path.relative_to(ROOT)}", flush=True)
    print(f"result: {'PASS' if failures == 0 else 'FAIL'}; "
          f"{sum(counts.values())} differential cases; {time.time() - started:.1f}s", flush=True)
    port.lib.ae_ram_destroy(port.ram)
    return 0 if failures == 0 else 1


def mutate_sources(destination):
    overrides = {}
    selector_ids = {}
    mutated_symbols = set()
    for name, (address, _size, symbol, _ret, _kinds) in FUNCS.items():
        selector_ids[name] = (len(selector_ids) * 2 + 1, len(selector_ids) * 2 + 2)
    for source in [
            "src/ui/ae_missb_ui.c", "src/world/ae_missb_boxes.c",
            "src/world/ae_missb_colliders.c", "src/world/ae_missb_game.c",
            "src/world/ae_missb_gimmicks.c", "src/world/ae_missb_items.c",
            "src/world/ae_missb_progress.c", "src/sound/ae_missb_sound.c"]:
        content = (ROOT / source).read_text(encoding="utf-8")
        for name, (_address, _size, symbol, _ret, _kinds) in FUNCS.items():
            pattern = re.compile(r"(\b" + re.escape(symbol) + r"\s*\([^;{}]*?\)\s*\{)", re.S)
            if pattern.search(content) is None:
                continue
            one, two = selector_ids[name]
            inject = (
                r"\1\n    uint32_t ae_missb_mutation_selector = ae_rd32(r, 0x800B4DF0u);"
                f"\n    if (ae_missb_mutation_selector == {one}u) ae_wr32(r, 0x800B4DF4u, 0xA11CE001u);"
                f"\n    else if (ae_missb_mutation_selector == {two}u) ae_wr32(r, 0x800B4DF4u, 0xA11CE002u);"
            )
            content, replacements = pattern.subn(inject, content, count=1)
            if replacements != 1:
                raise RuntimeError(f"could not inject mutation selectors into {symbol}")
            mutated_symbols.add(symbol)
        destination.mkdir(parents=True, exist_ok=True)
        out = destination / Path(source).name
        out.write_text(content, encoding="utf-8")
        overrides[source] = out
    if len(selector_ids) != len(FUNCS):
        raise AssertionError("not every target received two mutation selectors")
    expected_symbols = {spec[2] for spec in FUNCS.values()}
    if mutated_symbols != expected_symbols:
        missing = sorted(expected_symbols - mutated_symbols)
        raise RuntimeError("mutation functions not found: " + ", ".join(missing))
    return overrides, selector_ids


def run_mutations(args):
    started = time.time()
    reg.declare("stack_window", f"{STACK_LO:08X}..{STACK_HI:08X}")
    mutant_dir = ROOT / "extract/mips/missb_mutants"
    overrides, selector_ids = mutate_sources(mutant_dir)
    dll = build_dll(overrides=overrides, tag="missb_mut")
    port = CPort(dll)
    cpu = mr.MipsRam.from_extract()
    base, blobs, area_ids = load_corpus(cpu)
    caught = 0
    total = len(FUNCS) * 2

    for func_index, name in enumerate(args.only or list(FUNCS)):
        case_index = 0
        area = area_ids[(func_index + 2) % len(area_ids)]
        cpu.restore(base)
        cpu.load_area(blobs[area])
        if hasattr(cpu, "gte_reset"):
            cpu.gte_reset()
        case_args = setup_case(cpu, name, case_index, area)
        caller_a3, caller_s1, caller_s2 = caller_context(name, case_index)
        initial = cpu.snapshot()
        mips_events = []
        try:
            regs = oracle_run(cpu, name, case_args, caller_a3,
                              caller_s1, caller_s2, mips_events)
            mips_ram = cpu.snapshot()
        except Exception as error:
            print(f"FAIL mutation fixture {name}: {type(error).__name__}: {error}", flush=True)
            return 1

        for selector in selector_ids[name]:
            port.sync_from(initial)
            port.prepare(mr.DEFAULT_SP, caller_a3, caller_s1, caller_s2)
            port.lib.ae_wr32(port.ram, MUTATION_SELECTOR, selector)
            result = port.call(name, case_args)
            c_error = (port.lib.ae_ram_error(port.ram).decode(errors="replace")
                       if port.lib.ae_ram_failed(port.ram) else "")
            expected = (mips_ram, regs, regs[2], mips_events)
            problems = compare_case(cpu, port, name, expected, result,
                                    c_error, f"mutation {name}/{selector}")
            if problems:
                caught += 1
            else:
                print(f"MUTATION SURVIVED {name} selector={selector}", flush=True)
    print(f"mutation result: {caught}/{total} caught in {time.time() - started:.1f}s", flush=True)
    port.lib.ae_ram_destroy(port.ram)
    return 0 if caught == total else 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--only", nargs="*", default=None)
    parser.add_argument("--cases", type=float, default=100)
    parser.add_argument("--seed", type=lambda value: int(value, 0), default=DEFAULT_SEED)
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    args = parser.parse_args()
    unknown = set(args.only or ()) - set(FUNCS)
    if unknown:
        raise SystemExit("unknown target(s): " + ", ".join(sorted(unknown)))
    if not (ROOT / "extract/disc/SCUS_944.23").is_file():
        print("SKIP: extract/disc/SCUS_944.23 is not present")
        return 0
    if args.mutate:
        return run_mutations(args)
    return run_default(args)


if __name__ == "__main__":
    raise SystemExit(main())
