"""Differential checks for the menu-window RAM-image ports."""
from __future__ import annotations

import argparse
import ctypes as C
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

AREA_IDS = (0x01, 0x02, 0x04, 0x05, 0x07)
STACK_WINDOW = (0x801FF000, 0x80200000)
MENU_STACK = (0x801FF000, 0x801FF100)
MENU_RENDER_MASKS = ((0x800EC354, 0x800F4350),)
FUNCS = {
    "WriteSizedValue": (0x8007D188, 88, "s", "uus", "ae_WriteSizedValue"),
    "MenuStrLen": (0x8007D2AC, 44, "u", "u", "ae_MenuStrLen"),
    "MenuWindowAdd": (0x8007D2D8, 600, "s", "u", "ae_MenuWindowAdd"),
    "MenuWindowsRun": (0x8007D530, 7888, "s", "uuuu", "ae_MenuWindowsRun"),
}
EXTRA_COVERAGE = {
    "MenuWindowsRun inline renderer blocks": (0x8007D71C, 0x28C),
    "MenuWindowsRun row and chrome packet assembly": (0x8007D9F0, 0x174),
    "MenuWindowsRun item frame packets": (0x8007DB28, 0x1FC),
    "MenuWindowsRun alternate row packets": (0x8007DD24, 0x240),
    "MenuWindowsRun animated panel packet writes": (0x8007E030, 0x100),
    "MenuWindowsRun value row packet writes": (0x8007E1B8, 0xCC),
    "MenuWindowsRun selection packet writes": (0x8007E3F8, 0x1E0),
    "MenuWindowsRun cursor packet writes": (0x8007F148, 0x198),
}

PAD_HELD, PAD_CHANGED, PAD_BUFFER = 0x800E5478, 0x800E5480, 0x800E52D0
DEBUG_ENABLED = 0x800F460A
STAGE_STATE = 0x800F4350
STAGE_PROGRESS_TYPE = 0x800F4350 + 0x12C
SOUND_EFFECTS = 0x800B2D08
SOUND_BANK_IDS = 0x800BF670
AE_MENU_DEF_POSITION = 0x00
AE_MENU_DEF_ITEMS = 0x04
AE_MENU_DEF_ITEM_TEXT = 0x04
AE_MENU_DEF_ITEM_SIZE = 0x24

CALL_HOOK = C.CFUNCTYPE(C.c_uint32, C.c_void_p, C.c_void_p,
                        C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
CALL_HOOK0 = C.CFUNCTYPE(C.c_uint32, C.c_void_p, C.c_void_p)
CALL_HOOK1 = C.CFUNCTYPE(C.c_uint32, C.c_void_p, C.c_void_p, C.c_uint32)
CALL_HOOK2 = C.CFUNCTYPE(C.c_uint32, C.c_void_p, C.c_void_p,
                         C.c_uint32, C.c_uint32)
CALL_HOOK3 = C.CFUNCTYPE(C.c_uint32, C.c_void_p, C.c_void_p,
                         C.c_uint32, C.c_uint32, C.c_uint32)
CALL_HOOK4 = C.CFUNCTYPE(C.c_uint32, C.c_void_p, C.c_void_p,
                         C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
ITEM_HOOK = C.CFUNCTYPE(C.c_uint32, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32)
STAGE_HOOK = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                         C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
PAD_STATE = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_int32)
PAD_RESET = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32)
DEBUG_RENDER = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                           C.c_uint32, C.c_uint32, C.c_uint32,
                           C.c_uint32, C.c_uint32, C.c_uint32)


class PadHooks(C.Structure):
    _fields_ = [("user", C.c_void_p), ("pad_get_state", PAD_STATE),
                ("pad_info_mode", C.c_void_p), ("pad_setup_dualshock", C.c_void_p),
                ("pad_demo_step", C.c_void_p), ("stage_request", C.c_void_p),
                ("stick_to_buttons", C.c_void_p), ("pad_rumble_reset", PAD_RESET)]


STAGE_HOOK_NAMES = (
    "AreaUsesAltMember ArcGetMemberPos FUN_80025E24 CdMgrQueueCommand StageExitStart IsTimeAttack GetLevelFirstArea "
    "GimmickSaveStates GimmickSaveApplyProgress ItemSaveCollectedMask ZakoSaveDefeatedMask AreaGetKind AreaGetLevel "
    "GimmickSaveTableReset ItemsClearFlags ZakoClearSlotTable SoundSilence LevelMonkeyCountGet AreaBumpAlloc MeshInit "
    "GetZoneMask FUN_80030890 FUN_800A3700 HeapInit AreaBumpAllocSet AreaSpecialFlagsClear AreaSetVramAnimList "
    "AreaSetZoneBoxes IndicatorsReset AreaSetFeature30 AreaSetSpecialActor AreaSetIndicatorModelA "
    "AreaSetIndicatorModelB AreaSetFeature31 AreaSetFeature32 SoundAreaKeepsMusic BgmStart BgmUpdateVolume ApplyLevelFog "
    "SetColorMatrixMode SoundLoopsStopAll BuildTerrainBlockMask TableClearDAT_800BEF60 DebugMenuInitPlayerParams "
    "DebugMenuInitCounters DebugMenuInitCounters2 MessageBoxClose PlayerSpawnAtEntry TargetListClear ZakoInit "
    "MonkeyLodModelsInit FxSlotsReset ProjectilesReset EffectsReset ColliderZoneMasksUpdate GimmickSpawnAll BossInit "
    "ItemsLoadForArea AreaTexCopyInit RcCarDespawn PlayerSelectGadget HudFindGadgetIconPos MonkeyClearWords VSync Printf "
    "CutsceneModeOff CutsceneModeOn PlayerInit PlayerSetMode FUN_80028850 FUN_800286CC WaitFrameFlip BgmFadeOut "
    "SoundStop PlaySfx ProgressSetByte PlayTimeAdd StageGradeUpdate StageVictoryStart StageTypeClearInit "
    "StageTypeClearUpdate StageIntroStart PlayerEnterCutsceneMode StageIntroUpdate GimmickNopUpdate3 "
    "GimmickNopUpdate4 XaPlay AreaCheckExits CdMgrUpdate FadeFrame LoadListStep FUN_80096638 FUN_8013EA24 "
    "AimRequestClear AimModeLeave").split()


class StageHooks(C.Structure):
    _fields_ = [("user", C.c_void_p), ("stack_pointer", C.c_uint32)] + [
        (name, STAGE_HOOK) for name in STAGE_HOOK_NAMES
    ] + [("monkey_hooks", C.c_void_p), ("gadget_hooks", C.c_void_p)]


class DebugHooks(C.Structure):
    _fields_ = [("user", C.c_void_p), ("body_hooks", C.c_void_p),
                ("world_hooks", C.c_void_p), ("spawn_hooks", C.c_void_p),
                ("items_hooks", C.c_void_p)] + [
                    (name, CALL_HOOK) for name in (
                        "PlaySfx", "DrawShadow", "ModelTransformRigid", "ModelDraw", "FUN_8013f660")
                ] + [(name, DEBUG_RENDER) for name in (
                    "DrawProjectileSprite", "DrawDebugGlyph", "FinishDebugText")]


MENU_HOOK_ARGUMENT_COUNTS = {
    "SetDrawMove": 4,
    "DrawPrim": 1,
    "SetDrawArea": 2,
    "SetDrawTPage": 4,
    "BeginFrame": 0,
    "ColorFadeReset": 0,
    "OtLinkSkipEmpty": 1,
    "DrawSync": 1,
    "VSync": 1,
    "PutDrawEnv": 1,
    "PutDispEnv": 1,
    "DrawOTag": 1,
    "ModelTransformRigid": 3,
    "ModelDraw": 3,
    "DrawMenuChrome": 4,
    "DrawMenuRows": 4,
    "DrawMenuHighlight": 4,
    "DrawMenuItem": 4,
    "DrawMenuSelection": 4,
    "DrawMenuCursor": 4,
}
CALL_HOOK_BY_COUNT = {
    0: CALL_HOOK0,
    1: CALL_HOOK1,
    2: CALL_HOOK2,
    3: CALL_HOOK3,
    4: CALL_HOOK4,
}


class MenuHooks(C.Structure):
    _fields_ = [("user", C.c_void_p), ("pad", C.POINTER(PadHooks)),
                ("stage", C.POINTER(StageHooks)), ("sound", C.c_void_p),
                ("debug_text", C.POINTER(DebugHooks))] + [
                    (name, CALL_HOOK_BY_COUNT[MENU_HOOK_ARGUMENT_COUNTS[name]])
                    for name in MENU_HOOK_ARGUMENT_COUNTS
                ] + [("item_callback", ITEM_HOOK)]


class SoundHw(C.Structure):
    _fields_ = [(name, C.c_void_p) for name in (
        "user", "wait_idle", "wait_loop", "set_voice_adsr", "voice_key_off",
        "printf_error", "set_voice_volume")]


def build_source_paths(source=None):
    return [
        ROOT / "src/core/ae_ram.c", ROOT / "src/core/ae_sdk.c",
        ROOT / "src/core/ae_sdk_matrix.c", ROOT / "src/core/ae_pad.c",
        ROOT / "src/player/native_player.c", ROOT / "src/player/native_gadget_slots.c",
        ROOT / "src/player/ae_player_spawn.c", ROOT / "src/camera/ae_camera.c",
        ROOT / "src/bodies/ae_bodies.c", ROOT / "src/effects/ae_spawn.c",
        ROOT / "src/sound/ae_sound.c", ROOT / "src/world/ae_world.c",
        ROOT / "src/world/ae_solid.c", ROOT / "src/world/ae_items_init.c",
        ROOT / "src/world/ae_zako2.c", ROOT / "src/world/ae_zones.c",
        ROOT / "src/world/ae_stage.c",
        *sorted((ROOT / "src/monkeys").glob("ae_monkey*.c")),
        Path(source) if source else ROOT / "src/ui/ae_menuwin.c",
    ]


def build(source=None, tag="menuwin", trapv=False):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{os.getpid()}_{time.time_ns()}.dll"
    sources = build_source_paths(source)
    command = [
        shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
        "-shared", "-static-libgcc", "-Isrc/core", "-Isrc/ui", "-Isrc/player",
        "-Isrc/camera", "-Isrc/world", *map(str, sources), "-o", str(dll), "-lm",
    ]
    if trapv:
        command.insert(2, "-ftrapv")
    subprocess.run(command, cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    return dll


def build_mutation_dlls(mutated_sources, temp):
    """Compile shared dependencies once, then link one menu-window mutant per source file."""
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    compiler = shutil.which("gcc")
    include_flags = ["-Isrc/core", "-Isrc/ui", "-Isrc/player", "-Isrc/camera", "-Isrc/world"]
    common_sources = build_source_paths()[:-1]
    common_objects = []
    for index, source in enumerate(common_sources):
        obj = Path(temp) / f"common_{index}.o"
        subprocess.run(
            [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
             "-ffunction-sections", "-fdata-sections", *include_flags,
             "-c", str(source), "-o", str(obj)],
            cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
        common_objects.append(obj)

    dlls = []
    for index, source in enumerate(mutated_sources):
        obj = Path(temp) / f"mutated_{index}.o"
        subprocess.run(
            [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
             "-ffunction-sections", "-fdata-sections", *include_flags,
             "-c", str(source), "-o", str(obj)],
            cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
        dll = out / f"menuwin_mut_{os.getpid()}_{index}_{time.time_ns()}.dll"
        subprocess.run(
            [compiler, "-Wl,--gc-sections", "-shared", "-static-libgcc",
             *map(str, common_objects), str(obj), "-o", str(dll), "-lm"],
            cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
        dlls.append(dll)
    return dlls


class CSide:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        P, U, I = C.c_void_p, C.c_uint32, C.c_int32
        self.lib.ae_ram_create.restype = P
        self.lib.ae_ram_destroy.argtypes = [P]
        self.lib.ae_ptr.argtypes = [P, U, U]
        self.lib.ae_ptr.restype = P
        self.lib.ae_ram_failed.argtypes = [P]
        self.lib.ae_ram_failed.restype = C.c_int
        self.lib.ae_ram_error.argtypes = [P]
        self.lib.ae_ram_error.restype = C.c_char_p
        self.lib.ae_ram_clear_error.argtypes = [P]
        self.lib.ae_wr8.argtypes = [P, U, U]
        self.lib.ae_wr16.argtypes = [P, U, U]
        self.lib.ae_wr32.argtypes = [P, U, U]
        self.ram = self.lib.ae_ram_create()
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(self.lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(self.lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        for name, (_addr, _size, result, args, c_name) in FUNCS.items():
            fn = getattr(self.lib, c_name)
            fn.argtypes = ([P, C.POINTER(MenuHooks)] + [I if c == "s" else U for c in args]
                           if name == "MenuWindowsRun" else
                           [P] + [I if c == "s" else U for c in args])
            fn.restype = {"s": I, "u": U, "v": None}[result]

        self.lib.ae_pad_PadUpdate.argtypes = [P, C.POINTER(PadHooks), U]
        self.lib.ae_pad_PadUpdate.restype = None
        self.lib.ae_StageRequestUpdate.argtypes = [P, C.POINTER(StageHooks)]
        self.lib.ae_StageRequestUpdate.restype = I
        self.lib.ae_ColorFadeSet.argtypes = [P, I, I]
        self.lib.ae_ColorFadeSet.restype = None
        self.lib.ae_DebugTextPrintf.argtypes = [P, C.POINTER(DebugHooks), U, U]
        self.lib.ae_DebugTextPrintf.restype = None
        self.lib.ae_ReadSizedValue.argtypes = [P, U, I]
        self.lib.ae_ReadSizedValue.restype = I
        self.lib.ae_PlaySfx.argtypes = [P, C.POINTER(SoundHw), I]
        self.lib.ae_PlaySfx.restype = I
        self.lib.ae_StageRequest.argtypes = [P, C.POINTER(StageHooks), I, I, I]
        self.lib.ae_StageRequest.restype = I
        self.lib.ae_sdk_RotMatrixZYX.argtypes = [P, U, U]
        self.lib.ae_sdk_RotMatrixZYX.restype = U
        self.lib.ae_sdk_ScaleMatrix.argtypes = [P, U, U]
        self.lib.ae_sdk_ScaleMatrix.restype = U

        self.hook_model = None
        self.keep = []
        self.sound_hw = SoundHw()
        self.configure(None)

    def close(self):
        if self.ram is not None:
            self.lib.ae_ram_destroy(self.ram)
            self.ram = None

    def call(self, name, args):
        fn = getattr(self.lib, FUNCS[name][4])
        if name == "MenuWindowsRun":
            return int(fn(self.ram, C.byref(self.menu_hooks), *args))
        return int(fn(self.ram, *args))

    def configure(self, model):
        self.hook_model = model
        self.keep = []
        self.pad_hooks = PadHooks()
        self.stage_hooks = StageHooks()
        self.debug_hooks = DebugHooks()
        self.menu_hooks = MenuHooks()

        def keep(callback):
            self.keep.append(callback)
            return callback

        def pad_state(_user, _ram, port):
            if model is not None:
                model.event("PadGetState", (port,))
            return 2

        def pad_reset(_user, _ram, port):
            if model is not None:
                model.event("PadRumbleReset", (port,))

        self.pad_hooks.pad_get_state = keep(PAD_STATE(pad_state))
        self.pad_hooks.pad_rumble_reset = keep(PAD_RESET(pad_reset))

        def cd_update(_user, _ram, a0, a1, a2, a3):
            if model is not None:
                model.event("CdMgrUpdate", (a0, a1, a2, a3))
            return 0

        self.stage_hooks.CdMgrUpdate = keep(STAGE_HOOK(cd_update))

        def is_time_attack(_user, _ram, a0, a1, a2, a3):
            if model is not None:
                model.event("IsTimeAttack", (a0, a1, a2, a3))
            return 0

        self.stage_hooks.IsTimeAttack = keep(STAGE_HOOK(is_time_attack))

        for stage_name in STAGE_HOOK_NAMES:
            if stage_name in ("CdMgrUpdate", "IsTimeAttack"):
                continue
            def stage_callback(_user, _ram, a0, a1, a2, a3, name=stage_name):
                if model is not None:
                    model.event(name, (a0, a1, a2, a3))
                return 0
            setattr(self.stage_hooks, stage_name, keep(STAGE_HOOK(stage_callback)))

        def debug_glyph(_user, _ram, a0, a1, a2, a3, a4, a5):
            if model is not None:
                model.event("DrawDebugGlyph", (a0, a1, a2, a3, a4, a5))
            return 0

        def debug_finish(_user, _ram, a0, a1, a2, a3, a4, a5):
            if model is not None:
                model.event("FinishDebugText", (a0, a1, a2, a3, a4, a5))
            return 0

        self.debug_hooks.DrawDebugGlyph = keep(DEBUG_RENDER(debug_glyph))
        self.debug_hooks.FinishDebugText = keep(DEBUG_RENDER(debug_finish))

        native_names = (
            "SetDrawMove", "DrawPrim", "SetDrawArea", "SetDrawTPage", "BeginFrame",
            "ColorFadeReset", "OtLinkSkipEmpty", "DrawSync", "VSync", "PutDrawEnv",
            "PutDispEnv", "DrawOTag", "ModelTransformRigid", "ModelDraw", "DrawMenuChrome",
            "DrawMenuRows", "DrawMenuHighlight", "DrawMenuItem", "DrawMenuSelection", "DrawMenuCursor")
        for name in native_names:
            def make_callback(hook_name):
                def callback(_user, ram, *args):
                    if model is not None:
                        model.event(hook_name, args)
                        if hook_name == "BeginFrame":
                            model.begin_c(self.lib, ram)
                    return 0
                return callback
            hook_type = CALL_HOOK_BY_COUNT[MENU_HOOK_ARGUMENT_COUNTS[name]]
            setattr(self.menu_hooks, name, keep(hook_type(make_callback(name))))

        def item_callback(_user, _ram, target, value):
            return model.item_callback(target, value) if model is not None else 0

        self.menu_hooks.item_callback = keep(ITEM_HOOK(item_callback))
        self.menu_hooks.pad = C.pointer(self.pad_hooks)
        self.menu_hooks.stage = C.pointer(self.stage_hooks)
        self.menu_hooks.sound = C.cast(C.pointer(self.sound_hw), C.c_void_p)
        self.menu_hooks.debug_text = C.pointer(self.debug_hooks)
        self.keep.extend([self.pad_hooks, self.stage_hooks, self.debug_hooks, self.menu_hooks])

    def bridge(self, cpu, function_name, args):
        C.memmove(self.main, bytes(cpu.ram), mr.RAM_SIZE)
        C.memmove(self.scratch, bytes(cpu.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)
        if function_name == "PadUpdate":
            self.lib.ae_pad_PadUpdate(self.ram, C.byref(self.pad_hooks), args[0])
            result = 0
        elif function_name == "StageRequestUpdate":
            result = self.lib.ae_StageRequestUpdate(self.ram, C.byref(self.stage_hooks))
        elif function_name == "ColorFadeSet":
            self.lib.ae_ColorFadeSet(self.ram, args[0], args[1])
            result = 0
        elif function_name == "DebugTextPrintf":
            for index, value in enumerate(args):
                self.lib.ae_wr32(self.ram, 0x801FFE68 + index * 4, value)
            for index in range(8):
                self.lib.ae_wr32(self.ram, 0x801FFE78 + index * 4, 0)
            self.lib.ae_DebugTextPrintf(self.ram, C.byref(self.debug_hooks),
                                        args[0], 0x801FFE68)
            result = 0
        elif function_name == "ReadSizedValue":
            result = self.lib.ae_ReadSizedValue(self.ram, args[0], args[1])
        elif function_name == "PlaySfx":
            result = self.lib.ae_PlaySfx(self.ram, C.byref(self.sound_hw), args[0])
        elif function_name == "StageRequest":
            result = self.lib.ae_StageRequest(self.ram, C.byref(self.stage_hooks),
                                              args[0], args[1], args[2])
        elif function_name == "RotMatrixZYX":
            result = self.lib.ae_sdk_RotMatrixZYX(self.ram, args[0], args[1])
        elif function_name == "ScaleMatrix":
            result = self.lib.ae_sdk_ScaleMatrix(self.ram, args[0], args[1])
        else:
            raise AssertionError(f"unknown direct port {function_name}")
        if self.lib.ae_ram_failed(self.ram):
            raise mr.MipsError(self.lib.ae_ram_error(self.ram).decode())
        cpu.ram[:] = bytes(self.main)
        cpu.scratch[:] = bytes(self.scratch)
        return int(result) & 0xFFFFFFFF


class HookModel:
    def __init__(self, scheduled_buttons=(), callback_result=0):
        self.scheduled_buttons = list(scheduled_buttons)
        self.callback_result = callback_result
        self.begin_count = 0
        self.events = []
        self.render_events = []

    def event(self, name, args):
        values = tuple(int(value) & 0xFFFFFFFF for value in args)
        if name in MENU_HOOK_ARGUMENT_COUNTS:
            argument_count = MENU_HOOK_ARGUMENT_COUNTS[name]
            if argument_count < 4 and len(values) > argument_count:
                reg.declare("ram_mask", f"hook {name} peek[{argument_count}:4]")
            values = values[:argument_count]
        pointer_fields = {
            "SetDrawMove": (0,), "DrawPrim": (0,), "SetDrawArea": (0,),
            "SetDrawTPage": (0,), "DrawDebugGlyph": (4, 5),
            "FinishDebugText": (0, 1),
        }
        if name in pointer_fields:
            values = tuple(0 if index in pointer_fields[name] else value
                           for index, value in enumerate(values))
        entry = (name, *values)
        if name.startswith("DrawMenu"):
            self.render_events.append(entry)
        else:
            self.events.append(entry)

    def next_buttons(self):
        if self.begin_count < len(self.scheduled_buttons):
            buttons = self.scheduled_buttons[self.begin_count]
        else:
            buttons = 0x0140
        self.begin_count += 1
        return buttons

    def begin_c(self, lib, ram):
        buttons = self.next_buttons()
        released = (~buttons) & 0xFFFF
        lib.ae_wr8(ram, PAD_BUFFER, 0)
        lib.ae_wr8(ram, PAD_BUFFER + 1, 0x41)
        lib.ae_wr8(ram, PAD_BUFFER + 2, (released >> 8) & 0xFF)
        lib.ae_wr8(ram, PAD_BUFFER + 3, released & 0xFF)

    def begin_mips(self, cpu):
        buttons = self.next_buttons()
        released = (~buttons) & 0xFFFF
        cpu.write8(PAD_BUFFER, 0)
        cpu.write8(PAD_BUFFER + 1, 0x41)
        cpu.write8(PAD_BUFFER + 2, (released >> 8) & 0xFF)
        cpu.write8(PAD_BUFFER + 3, released & 0xFF)

    def item_callback(self, target, value):
        self.event("ItemCallback", (target, value))
        return target if self.callback_result == 0 else self.callback_result


PORT_MIPS_CALLS = {
    0x8001325C: ("PadUpdate", 1),
    0x8005D890: ("StageRequestUpdate", 0),
    0x8008C3C0: ("ColorFadeSet", 2),
    0x8008C6F0: ("DebugTextPrintf", 4),
    0x8007D118: ("ReadSizedValue", 2),
    0x800877F8: ("PlaySfx", 1),
    0x8005D100: ("StageRequest", 3),
    0x8002FCC0: ("RotMatrixZYX", 2),
    0x80030E60: ("ScaleMatrix", 2),
}
NATIVE_MIPS_CALLS = {
    0x8002B380: "SetDrawMove",
    0x80028C64: "DrawPrim",
    0x8005DF48: "BeginFrame",
    0x8008C390: "ColorFadeReset",
    0x80029458: "SetDrawArea",
    0x8002B410: "SetDrawTPage",
    0x800161C0: "ModelTransformRigid",
    0x80017CD4: "ModelDraw",
    0x8001F3A0: "OtLinkSkipEmpty",
    0x800286CC: "DrawSync",
    0x8002E520: "VSync",
    0x80028D30: "PutDrawEnv",
    0x80028EFC: "PutDispEnv",
    0x80028CC0: "DrawOTag",
}


def mips_menu_hooks(harness, cpu, model):
    hooks = {}
    for address, (name, argument_count) in PORT_MIPS_CALLS.items():
        def make_port(port_name, n):
            def invoke(machine, regs):
                args = tuple(regs[4 + index] for index in range(n))
                return harness.c.bridge(machine, port_name, args)
            return invoke
        hooks[address] = make_port(name, argument_count)

    for address, name in NATIVE_MIPS_CALLS.items():
        def make_native(hook_name):
            def invoke(machine, regs):
                args = tuple(regs[4:8])
                model.event(hook_name, args)
                if hook_name == "BeginFrame":
                    model.begin_mips(machine)
                return 0
            return invoke
        hooks[address] = make_native(name)

    def default_item_callback(machine, regs):
        return model.item_callback(0x8007D110, regs[4])

    hooks[0x8007D110] = default_item_callback
    return hooks


class MenuHarness:
    def __init__(self, scale=1.0, seed=0x4D454E55, source=None, trapv=False):
        self.scale = scale
        self.seed = seed
        self.rng = random.Random(seed)
        self.members = dump_areas.area_members()
        missing = [area for area in AREA_IDS if area not in self.members or
                   not (ROOT / "extract/members" / f"{self.members.get(area, 0):03X}.bin").is_file()]
        if missing:
            raise SystemExit(f"real area-data members missing for menu-window checks: {missing}")
        reg.declare("stack_window", f"{STACK_WINDOW[0]:08X}..{STACK_WINDOW[1]:08X}")
        reg.check_ram_mask([
            (start - 0x80000000, end - start)
            for start, end in MENU_RENDER_MASKS
        ])
        self.c = CSide(build(source=source, trapv=trapv))
        self.coverage = set()
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0
        self.mips_by_area = {}
        self.base_by_area = {}
        for area in AREA_IDS:
            member = self.members[area]
            cpu = mr.MipsRam.from_extract(area=ROOT / "extract/members" / f"{member:03X}.bin")
            self.mips_by_area[area] = cpu
            self.base_by_area[area] = cpu.snapshot()

    def case_count(self, base):
        return max(1, int(base * self.scale))

    def fresh(self, case_index):
        area = AREA_IDS[case_index % len(AREA_IDS)]
        cpu = self.mips_by_area[area]
        cpu.restore(self.base_by_area[area])
        cpu.coverage = set()
        cpu.hook_log.clear()
        cpu.steps = 0
        return cpu

    def compare(self, name, label, cpu, args, mips_hooks=None,
                mips_model=None, c_model=None):
        initial_ram = bytes(cpu.ram)
        initial_scratch = bytes(cpu.scratch)
        C.memmove(self.c.main, initial_ram, mr.RAM_SIZE)
        C.memmove(self.c.scratch, initial_scratch, mr.SCRATCH_SIZE)
        self.c.lib.ae_ram_clear_error(self.c.ram)
        mips_error = None
        try:
            mret = cpu.call(FUNCS[name][0], args, hooks=mips_hooks or {})[2]
        except mr.MipsError as exc:
            mips_error = str(exc)
            mret = None
        self.coverage.update(cpu.coverage or ())

        C.memmove(self.c.main, initial_ram, mr.RAM_SIZE)
        C.memmove(self.c.scratch, initial_scratch, mr.SCRATCH_SIZE)
        self.c.lib.ae_ram_clear_error(self.c.ram)
        if c_model is not None:
            self.c.configure(c_model)
        cret = self.c.call(name, args)
        c_failed = bool(self.c.lib.ae_ram_failed(self.c.ram))
        if mips_error and not c_failed:
            self.fail(name, label, f"original MIPS failed ({mips_error}) but the C port did not flag an error")
            return
        if not mips_error and c_failed:
            self.fail(name, label, "C port flagged an error: " + self.c.lib.ae_ram_error(self.c.ram).decode())
            return
        if not mips_error and (mret & 0xFFFFFFFF) != (cret & 0xFFFFFFFF):
            self.fail(name, label, f"return mismatch MIPS {mret & 0xFFFFFFFF:08X}, C {cret & 0xFFFFFFFF:08X}")
            return
        if mips_model is not None and c_model is not None and mips_model.events != c_model.events:
            common = min(len(mips_model.events), len(c_model.events))
            index = next((i for i in range(common) if mips_model.events[i] != c_model.events[i]), common)
            want = mips_model.events[index] if index < len(mips_model.events) else "<end>"
            got = c_model.events[index] if index < len(c_model.events) else "<end>"
            mips_draw = [event for event in mips_model.events if event[0] in ("SetDrawMove", "SetDrawArea", "SetDrawTPage")]
            c_draw = [event for event in c_model.events if event[0] in ("SetDrawMove", "SetDrawArea", "SetDrawTPage")]
            context = ""
            if name == "MenuWindowsRun":
                context = (f"; nearby MIPS={mips_model.events[max(0, index - 3):index + 4]!r}, "
                           f"C={c_model.events[max(0, index - 3):index + 4]!r}")
            self.fail(name, label, f"hook sequence differs at {index}: MIPS {want!r}, C {got!r}; draw calls MIPS={mips_draw!r}, C={c_draw!r}{context}")
            return
        mips_ram = bytearray(cpu.ram[:0x1FF000])
        c_ram = bytearray(self.c.main[:0x1FF000])
        if name == "MenuWindowsRun":
            for start, end in MENU_RENDER_MASKS:
                lo, hi = start - 0x80000000, end - 0x80000000
                mips_ram[lo:hi] = bytes(hi - lo)
                c_ram[lo:hi] = bytes(hi - lo)
        if mips_ram != c_ram:
            diffs = [i for i, (a, b) in enumerate(zip(mips_ram, c_ram)) if a != b]
            extra = ""
            if name == "MenuWindowsRun":
                start = AE_MENU_WINDOWS_TEST - 0x80000000
                extra = (f"; initial window={initial_ram[start:start + 0x2C].hex()}, "
                         f"MIPS={mips_ram[start:start + 0x2C].hex()}, "
                         f"C={c_ram[start:start + 0x2C].hex()}")
            self.fail(name, label, "RAM differs at " + ", ".join(
                f"{0x80000000 + i:08X}(MIPS {mips_ram[i]:02X}, C {c_ram[i]:02X})" for i in diffs[:8]) + extra)
            return
        if bytes(cpu.scratch) != bytes(self.c.scratch):
            self.fail(name, label, "scratchpad differs")

    def fail(self, name, label, message):
        self.failures += 1
        print(f"FAIL {name} [{label}]: {message}")

    def run_write_cases(self):
        name = "WriteSizedValue"
        sizes = [-3, -1, 0, 1, 2, 3, 4, 8]
        count = self.case_count(320)
        for index in range(count):
            cpu = self.fresh(index)
            rng = random.Random(f"{self.seed}/{name}/{index}")
            address = 0x801D6000 + rng.randrange(0, 0x100) & ~3
            size = sizes[index % len(sizes)] if index < len(sizes) else rng.choice(sizes)
            if size == 1 and index % 3 == 0:
                address += 1
            value = rng.getrandbits(32)
            cpu.write_bytes(address - 4, rng.randbytes(16))
            self.compare(name, str(index), cpu, (address, value, size))
            self.counts[name] += 1

    def run_string_cases(self):
        name = "MenuStrLen"
        count = self.case_count(240)
        for index in range(count):
            cpu = self.fresh(index)
            rng = random.Random(f"{self.seed}/{name}/{index}")
            address = 0x801D7000
            if index % 5 == 0:
                value = b"`" + bytes([rng.randrange(0x20, 0x7F)]) + rng.randbytes(index % 41) + b"\0"
            else:
                value = rng.randbytes(index % 47) + b"\0"
            cpu.write_bytes(address, value)
            self.compare(name, str(index), cpu, (address,))
            self.counts[name] += 1

    def seed_definition(self, cpu, rng, index):
        definition = 0x801D8000
        items = 0x801D8100
        strings = 0x801D9000
        item_count = rng.choice([0, 1, 2, 5, 9, 32])
        x = rng.choice([0, 16, 100, 300, 0x7FFF, 0x8000])
        y = rng.choice([0, 16, 100, 220, 239, 0x7FFF, 0x8000])
        cpu.write32(definition, (x & 0xFFFF) | ((y & 0xFFFF) << 16))
        cpu.write32(definition + 4, items)
        cpu.write_bytes(0x801D8F00, bytes(rng.randrange(256) for _ in range(0x100)))
        for item_index in range(item_count):
            item = items + item_index * 0x24
            choices = [1, 2, 0x2000, 0x4000, 0x6000, 0x8000, 0xA000, 0xE000]
            flags = rng.choice(choices)
            cpu.write16(item, flags)
            text = strings + item_index * 64
            raw = (b"`X" if item_index % 4 == 0 else b"") + rng.randbytes(rng.randrange(0, 34)) + b"\0"
            cpu.write32(item + 4, text)
            cpu.write_bytes(text, raw)
            cpu.write_bytes(item + 8, rng.randbytes(0x24 - 8))
        cpu.write16(items + item_count * 0x24, 0)
        cpu.write_bytes(AE_MENU_WINDOWS_TEST, rng.randbytes(0x2C * 6))
        cpu.write16(0x800BEF60, rng.choice([0, 0, 1, 2, 5, 6, 0xFFFF]))
        cpu.write16(0x800BEF62, rng.getrandbits(16))
        return definition

    def run_add_cases(self):
        name = "MenuWindowAdd"
        count = self.case_count(360)
        for index in range(count):
            cpu = self.fresh(index)
            rng = random.Random(f"{self.seed}/{name}/{index}")
            definition = self.seed_definition(cpu, rng, index)
            self.compare(name, str(index), cpu, (definition,))
            self.counts[name] += 1

    @staticmethod
    def set_pad_buttons(cpu, buttons, buffer=PAD_BUFFER):
        released = (~buttons) & 0xFFFF
        cpu.write8(buffer, 0)
        cpu.write8(buffer + 1, 0x41)
        cpu.write8(buffer + 2, (released >> 8) & 0xFF)
        cpu.write8(buffer + 3, released & 0xFF)

    def seed_run_window(self, cpu, rng, index):
        definition = 0x801D8000
        items = 0x801D8100
        strings = 0x801D9000
        variable = 0x801DA000
        child_definition = 0x801DC000
        child_items = 0x801DC100
        child_label = 0x801DD000
        item_types = (0x8000, 0xC005, 0xC008, 0x8003, 0x8002, 0x8004, 0x2006)
        selected_type = (0x8007 if index == 68 else
                         0x2006 if index == 80 else
                         0x8002 if index in (81, 83) else
                         0x8003 if index == 82 else
                         0xC006 if index == 84 else item_types[index % len(item_types)])
        item_count = 22 if index % 16 == 6 else (2 if index % 3 == 0 else 1)
        x = rng.choice([16, 96, 300, 0x7FFF])
        y = rng.choice([16, 100, 220, 239, 0x7FFF])
        cpu.write32(definition, (x & 0xFFFF) | ((y & 0xFFFF) << 16))
        cpu.write32(definition + 4, items)
        cpu.write32(definition + 8, strings)
        cpu.write_bytes(strings, b"Pause menu\0")

        for item_index in range(item_count):
            item = items + item_index * 0x24
            kind = selected_type if item_index == 0 else 0x8000
            cpu.write16(item, kind)
            label = strings + 0x40 + item_index * 0x40
            cpu.write32(item + 4, label)
            cpu.write_bytes(label, (b"Adjust value\0" if kind in (0xC005, 0xC008)
                                    else b"Continue\0"))
            cpu.write32(item + 0x0C, 0x8007D110)
            if kind in (0xC005, 0xC008):
                data = variable + item_index * 4
                cpu.write32(item + 8, data)
                cpu.write32(item + 0x10, 0)
                cpu.write32(item + 0x14, 100)
                cpu.write32(item + 0x18, 25)
                cpu.write32(item + 0x1C, 5)
                cpu.write8(item + 0x20, 2)
                cpu.write32(data, 25)
            elif kind == 0x8003:
                cpu.write32(item + 8, 0 if index == 82 else child_items)
            elif kind == 0x8002:
                cpu.write32(item + 8, child_items if index == 81 else 0)
        cpu.write16(items + item_count * 0x24, 0)

        cpu.write32(child_definition, ((x + 4) & 0xFFFF) | (((y - 4) & 0xFFFF) << 16))
        cpu.write32(child_definition + 4, child_items)
        cpu.write16(child_items, 0)
        cpu.write32(child_label, 0)

        created = cpu.call(0x8007D2D8, (definition,))[2]
        if (created & 0xFFFFFFFF) != 0:
            raise AssertionError(f"MenuWindowAdd setup returned {created:08X}, expected window 0")
        if item_count > 1 and index % 8 == 3:
            cpu.write8(AE_MENU_WINDOWS_TEST + 0x0F, 1)
        if index % 9 == 1:
            cpu.write8(AE_MENU_WINDOWS_TEST + 0x0D, 1)
        elif index % 9 == 2:
            cpu.write8(AE_MENU_WINDOWS_TEST + 0x0D, 0)
        elif index % 16 == 6:
            maximum_height = cpu.read16(AE_MENU_WINDOWS_TEST + 0x0A)
            cpu.write16(AE_MENU_WINDOWS_TEST + 0x14, maximum_height - 1)
        cpu.coverage = set()

        self.set_pad_buttons(cpu, 0)
        self.set_pad_buttons(cpu, 0, PAD_BUFFER + 0x22)
        cpu.write32(PAD_HELD, 0)
        cpu.write32(PAD_CHANGED, 0)
        cpu.write32(PAD_HELD + 4, 0)
        cpu.write32(PAD_CHANGED + 4, 0)
        cpu.write32(0x800A8A58, 0)
        cpu.write16(0x800BEF60, 1)
        cpu.write16(0x800BEF62, 0)
        cpu.write16(0x800BEF64, 1)
        cpu.write16(0x800BEF70, 0)
        cpu.write16(0x800BEF74, 0)
        cpu.write8(STAGE_PROGRESS_TYPE, 0)
        cpu.write16(DEBUG_ENABLED, 0)

        context = 0x800EC350
        cpu.write32(context, 0)
        cpu.write32(context + 4, 0x800F1000)
        cpu.write32(context + 8, 0x800F2000)
        cpu.write32(context + 12, 0)
        cpu.write16(context + 0x6E, rng.choice([0, 1, 0x7FFF]))
        cpu.write16(context + 0x12, 0)
        return selected_type

    def prepare_window_case(self, index):
        name = "MenuWindowsRun"
        cpu = self.fresh(index)
        rng = random.Random(f"{self.seed}/{name}/{index}")
        kind = self.seed_run_window(cpu, rng, index)
        initial_inputs = {
            0x8000: 0,
            0xC005: 0x2000,
            0xC008: 0x8000,
            0x8007: 0x0800,
            0x8003: 0x0800,
            0x8002: 0x0800,
            0x8004: 0x0800,
            0x2006: 0,
            0xC006: 0x0800,
        }
        initial_buttons = initial_inputs[kind]
        item = 0x801D8100
        if index == 64:
            initial_buttons = 0
            cpu.write32(0x801DA000, 100)
        elif index == 65:
            initial_buttons = 0
            cpu.write32(0x801DA000, 0)
        elif index in (66, 72):
            cpu.write16(item, 0x8000)
            initial_buttons = 0
            if index == 72:
                cpu.write16(AE_MENU_WINDOWS_TEST + 0x10, 63)
        elif index == 68:
            initial_buttons = 0
        elif index == 70:
            cpu.write16(AE_MENU_WINDOWS_TEST + 0x10, 63)
        elif index == 71:
            cpu.write16(AE_MENU_WINDOWS_TEST + 0x12, 0xFFFE)
            cpu.write16(AE_MENU_WINDOWS_TEST + 0x16, 2)
        elif index in (73, 75):
            variable = 0x801DA000
            kind = 0xC005
            initial_buttons = 0
            cpu.write16(item, kind)
            cpu.write32(item + 8, variable)
            cpu.write32(item + 0x10, 0)
            cpu.write32(item + 0x14, 100)
            cpu.write32(item + 0x1C, 0 if index == 73 else 0xFFFFFFFF)
            cpu.write8(item + 0x20, 2)
            cpu.write32(variable, 0 if index == 73 else 0x80000000)
            if index == 75:
                cpu.write8(AE_MENU_WINDOWS_TEST + 0x0F, 0)
        elif index == 74:
            initial_buttons = 0
            cpu.write8(AE_MENU_WINDOWS_TEST + 0x0D, 0)
            cpu.write16(AE_MENU_WINDOWS_TEST + 2, 10)
        elif index == 76:
            variable = 0x801DA000
            kind = 0xC005
            initial_buttons = 0
            cpu.write16(item, kind)
            cpu.write8(AE_MENU_WINDOWS_TEST + 0x0C, 1)
            cpu.write8(AE_MENU_WINDOWS_TEST + 0x0F, 0)
            cpu.write32(item + 8, variable)
            cpu.write32(item + 0x10, 0)
            cpu.write32(item + 0x14, 100)
            cpu.write32(item + 0x1C, 5)
            cpu.write8(item + 0x20, 2)
            cpu.write32(variable, 25)
        elif index == 77:
            initial_buttons = 0x1000
        elif index == 78:
            cpu.write16(item, 0x8000)
            cpu.write8(AE_MENU_WINDOWS_TEST + 0x0F, 1)
            initial_buttons = 0x4000
        elif index == 79:
            cpu.write8(AE_MENU_WINDOWS_TEST + 0x1E, 0)
        elif index == 80:
            cpu.write8(AE_MENU_WINDOWS_TEST + 0x0C, 1)
            cpu.write8(AE_MENU_WINDOWS_TEST + 0x0F, 0)
        elif index in (81, 82, 83, 84):
            initial_buttons = 0
        if index in (64, 65, 66, 68, 72, 73, 75, 76, 77, 78, 79, 80,
                     81, 82, 83, 84):
            cpu.write8(AE_MENU_WINDOWS_TEST + 0x0D, 0)
        if index in (81, 82, 83, 84):
            cpu.write8(AE_MENU_WINDOWS_TEST + 0x0C, 1)
            cpu.write8(AE_MENU_WINDOWS_TEST + 0x0F, 0)
        self.set_pad_buttons(cpu, initial_buttons)
        cpu.write16(0x800BEF64, 1)
        if index == 77:
            cpu.write32(PAD_HELD, 0xFFFF1000)
            cpu.write32(PAD_CHANGED, 0)
        elif index == 78:
            cpu.write32(PAD_HELD, 0xFFFF4000)
            cpu.write32(PAD_CHANGED, 0)
        if index == 74:
            cpu.write16(0x800BEF62, 1)
        scheduled_inputs = {
            64: [0x2008, 0x2008, 0x0140],
            65: [0x8000, 0x8000, 0x0140],
            66: [0x1000, 0x1000, 0x0140],
            68: [0x0800, 0x0800, 0x0140],
            72: [0x4000, 0x4000, 0x0140],
            73: [0x2000, 0x2000, 0x0140],
            75: [0x2000, 0x2000, 0x0140],
            76: [0x0802, 0x0802, 0x0140],
            80: [0x0800, 0x0140],
            81: [0x0800, 0x0140],
            82: [0x0800, 0x0140],
            83: [0x0800, 0x0140],
            84: [0x0800, 0x0140],
        }
        schedule = scheduled_inputs.get(
            index, [0, 0, 0, 0x0140] if index % 8 == 0 else [0x0140])
        callback_result = 1 if index % 7 == 0 or index in (64, 68) else 0
        mips_model = HookModel(schedule, callback_result=callback_result)
        c_model = HookModel(schedule, callback_result=callback_result)
        self.c.configure(mips_model)
        hooks = mips_menu_hooks(self, cpu, mips_model)
        return cpu, hooks, mips_model, c_model

    def run_window_cases(self):
        name = "MenuWindowsRun"
        for index in range(self.case_count(85)):
            cpu, hooks, mips_model, c_model = self.prepare_window_case(index)
            self.compare(name, str(index), cpu, (0, 0, 0, 0), hooks,
                         mips_model=mips_model, c_model=c_model)
            sfx_calls = [a0 for address, a0, _a1, _a2, _a3 in cpu.hook_log
                         if address == 0x800877F8]
            callback_calls = [event for event in mips_model.events
                              if event[0] == "ItemCallback"]
            if index in (82, 83) and sfx_calls != [36, 33]:
                self.fail(name, str(index),
                          f"null-variable action called SFX {sfx_calls}, expected [36, 33]")
            if index == 81 and (len(callback_calls) != 1 or callback_calls[0][2] != 0):
                self.fail(name, str(index),
                          f"0x8002 child action callbacks were {callback_calls!r}, expected one arg-0 call")
            if index == 84 and (callback_calls or sfx_calls):
                self.fail(name, str(index),
                          f"unknown high item dispatched callbacks {callback_calls!r}, SFX {sfx_calls!r}")
            self.counts[name] += 1

    def finish(self, extra=None, default_run=False, show_coverage=False):
        summary = reg.coverage_summary(
            [(name, spec[0], spec[1], True) for name, spec in FUNCS.items()] +
            [(name, addr, size, False) for name, (addr, size) in (extra or {}).items()], self.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        if default_run:
            reg.write_results(self.counts, summary, not self.failures, function_names=list(FUNCS))
        return summary

    def verify_mutations(self):
        source_text = (ROOT / "src/ui/ae_menuwin.c").read_text(encoding="utf-8")
        mutations = [
            ("WriteSizedValue", "ae_wr16(r, address, value);", "ae_wr8(r, address, value);",
             "halfword store", ("write", 1)),
            ("WriteSizedValue", "ae_wr32(r, address, value);", "ae_wr16(r, address, value);",
             "word store", ("write", 2)),
            ("WriteSizedValue", "if (size == 0) {\n            ae_wr8(r, address, value);",
             "if (size == 0) {\n            ae_wr16(r, address, value);", "byte store", ("write", 0)),
            ("WriteSizedValue", "ae_wr16(r, address, value);\n        return 1;",
             "ae_wr16(r, address, value);\n        return 2;", "size one return", ("write", 1)),
            ("WriteSizedValue", "ae_wr32(r, address, value);\n        return 3;",
             "ae_wr32(r, address, value);\n        return 2;", "size two return", ("write", 2)),
            ("WriteSizedValue", "    }\n    return 3;\n}\n\nuint32_t ae_MenuStrLen",
             "    }\n    return 2;\n}\n\nuint32_t ae_MenuStrLen", "fallback return", ("write", 4)),
            ("MenuStrLen", "cursor += 2u;", "cursor += 1u;", "prefix skip", ("string", b"`Xmenu\0")),
            ("MenuStrLen", "return cursor - start + 1u;", "return cursor - start;",
             "terminator count", ("string", b"menu\0")),
            ("MenuStrLen", "++cursor;", "cursor += 2u;", "scan increment", ("string", b"a\0bc\0")),
            ("MenuStrLen", "while (ae_rd8(r, cursor) != 0u)",
             "while (ae_rd8(r, cursor) != (uint32_t)'m')", "stop condition", ("string", b"menu\0")),
            ("MenuStrLen", "if (ae_rd8(r, cursor) == (uint32_t)'`') {",
             "if (ae_rd8(r, cursor) == (uint32_t)'~') {", "prefix marker", ("string", b"`Xmenu\0")),
            ("MenuStrLen", "start = cursor;", "start = cursor + 1u;",
             "prefix start", ("string", b"`Xmenu\0")),
            ("MenuWindowAdd", "(flags & 0x8000u) != 0u", "(flags & 0x4000u) != 0u",
             "selection flag", ("add", "selected")),
            ("MenuWindowAdd", "((uint32_t)max_text_length + 1u) << 3",
             "((uint32_t)max_text_length + 2u) << 3", "label width", ("add", "long_label")),
            ("MenuWindowAdd", "ae_wr16(r, window + AE_MENU_WINDOW_FRAME_WIDTH, 17u);",
             "ae_wr16(r, window + AE_MENU_WINDOW_FRAME_WIDTH, 18u);", "frame width", ("add", "short_label")),
            ("MenuWindowAdd", "(uint32_t)(232 - item_text_width)",
             "(uint32_t)(231 - item_text_width)", "bottom clamp", ("add", "y_clamp")),
            ("MenuWindowAdd", "(flags & 0x6000u) != 0u", "(flags & 0x2000u) != 0u",
             "extended item width", ("add", "item_flag_4000")),
            ("MenuWindowAdd", "if (row_height < 16) {\n        row_height = 16;",
             "if (row_height < 16) {\n        row_height = 17;",
             "minimum row height", ("add", "empty")),
            ("MenuWindowsRun", "row_step = (row_step * 9) / 10;",
             "row_step = (row_step * 8) / 10;", "row step decay", ("run", 70, "row_step")),
            ("MenuWindowsRun", "window_x + frame_width - 14u",
             "window_x + frame_width - 13u", "cursor right edge", ("run", 70, "cursor")),
            ("MenuWindowsRun", "(held & changed) != 0u ? 20u : 3u",
             "(held & changed) != 0u ? 19u : 3u", "repeat delay", ("run", 64, "repeat")),
            ("MenuWindowsRun", "render_height + (int32_t)item_index * 10",
             "render_height + (int32_t)item_index * 11", "animated row scale", ("run", 70, "rows")),
            ("MenuWindowsRun", "(directions & 0x1000u) != 0u",
             "(directions & 0x2000u) != 0u", "up navigation", ("run", 66, "navigation")),
            ("MenuWindowsRun", "if (type != 0xC008u &&",
             "if (type != 0xC005u &&", "value callback", ("run", 64, "callback")),
            ("MenuWindowsRun", "if (next_y > 0xFFFDu) next_y = 0xFFFEu;",
             "if (next_y > 0xFFFDu) next_y = 0xFFFDu;", "animation y clamp", ("run", 71, "animation_y")),
            ("MenuWindowsRun", "ae_rd8(r, window + AE_MENU_WINDOW_STYLE_FLAG) != 0u",
             "ae_rd8(r, window + AE_MENU_WINDOW_STYLE_FLAG) == 0u", "selection gate", ("run", 74, "selection_gate")),
            ("MenuWindowsRun", "case 0xC005u:\n            *deferred_close = 1u;",
             "case 0xC005u:\n            *deferred_close = 0u;", "deferred close", ("run", 76, "deferred_close")),
            ("MenuWindowsRun", "ae_rd32(r, item + AE_MENU_ITEM_VALUE),\n                                 (int32_t)ae_rd8(r, item + AE_MENU_ITEM_VALUE_SIZE));",
             "ae_rd32(r, item + AE_MENU_ITEM_MAXIMUM),\n                                 (int32_t)ae_rd8(r, item + AE_MENU_ITEM_VALUE_SIZE));",
             "save item value", ("run", 76, "save_value")),
        ]
        original_c = self.c
        caught = {name: 0 for name in FUNCS}
        temp_root = ROOT / "extract/mips"
        temp_root.mkdir(parents=True, exist_ok=True)
        try:
            with tempfile.TemporaryDirectory(prefix="menuwin-mutations-", dir=temp_root) as temp:
                mutated_sources = []
                for index, (name, before, after, label, _target) in enumerate(mutations):
                    if before not in source_text:
                        raise AssertionError(f"mutation source not found: {name} {label}")
                    mutated_path = Path(temp) / f"mutation_{index}.c"
                    mutated_path.write_text(source_text.replace(before, after, 1), encoding="utf-8")
                    mutated_sources.append(mutated_path)
                dlls = build_mutation_dlls(mutated_sources, temp)

                for index, (name, _before, _after, label, target) in enumerate(mutations):
                    if self.c is not original_c:
                        self.c.close()
                    self.c = CSide(dlls[index])
                    failures_before = self.failures
                    cpu = self.fresh(10000 + index)
                    if target[0] == "write":
                        address = 0x801D6000
                        cpu.write_bytes(address - 4, bytes(range(16)))
                        self.compare(name, "mutation " + label,
                                     cpu, (address, 0x12345678, target[1]))
                    elif target[0] == "string":
                        address = 0x801D7000
                        cpu.write_bytes(address, target[1])
                        self.compare(name, "mutation " + label, cpu, (address,))
                    elif target[0] == "add":
                        definition = 0x801D8000
                        items = 0x801D8100
                        text = 0x801D9000
                        y = 239 if target[1] == "y_clamp" else 100
                        cpu.write32(definition, (100 & 0xFFFF) | ((y & 0xFFFF) << 16))
                        cpu.write32(definition + AE_MENU_DEF_ITEMS, items)
                        item_flags = {
                            "selected": 0x8000,
                            "long_label": 1,
                            "short_label": 1,
                            "y_clamp": 1,
                            "item_flag_4000": 0x4000,
                        }.get(target[1], 0)
                        item_count = 0 if target[1] == "empty" else 1
                        if item_count:
                            cpu.write16(items, item_flags)
                            cpu.write32(items + AE_MENU_DEF_ITEM_TEXT, text)
                            raw = (b"a deliberately long label\0" if target[1] == "long_label"
                                   else b"x\0")
                            cpu.write_bytes(text, raw)
                        cpu.write16(items + item_count * AE_MENU_DEF_ITEM_SIZE, 0)
                        cpu.write16(0x800BEF60, 0)
                        self.compare(name, "mutation " + label, cpu, (definition,))
                    else:
                        case_index = target[1]
                        cpu, hooks, mips_model, c_model = self.prepare_window_case(case_index)
                        if target[2] == "row_step":
                            cpu.write16(AE_MENU_WINDOWS_TEST + 0x16, 20)
                        self.compare(name, "mutation " + label, cpu,
                                     (0, 0, 0, 0), hooks,
                                     mips_model=mips_model, c_model=c_model)

                    if self.failures == failures_before:
                        raise AssertionError(f"mutation was not caught: {name} {label}")
                    caught[name] += 1
                    self.failures = failures_before
        finally:
            if self.c is not original_c:
                self.c.close()
            self.c = original_c
        if any(count < 2 for count in caught.values()):
            raise AssertionError(f"fewer than two mutations caught per function: {caught}")
        print("mutation checks caught " + ", ".join(f"{name}={count}" for name, count in caught.items()))


AE_MENU_WINDOWS_TEST = 0x800F5270


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", type=int, default=100, help="percentage of the default case count")
    ap.add_argument("--seed", type=int, default=0x4D454E55)
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--trapv", action="store_true")
    ap.add_argument("--mutate", action="store_true")
    args = ap.parse_args()
    h = MenuHarness(args.cases / 100.0, args.seed, trapv=args.trapv)
    h.run_write_cases()
    h.run_string_cases()
    h.run_add_cases()
    h.run_window_cases()
    if args.mutate:
        h.verify_mutations()
    summary = h.finish(EXTRA_COVERAGE, default_run=(args.cases == 100 and args.seed == 0x4D454E55 and not args.trapv and not args.mutate),
                       show_coverage=args.coverage)
    print(f"{sum(h.counts.values())} differential cases; {h.failures} mismatches")
    if h.failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
