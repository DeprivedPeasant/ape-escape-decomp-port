"""Differential checker for the readable member 189 overlay port.

The original member is loaded at 0x80136000 over the real SCUS_944.23 image and
one of several real area members. The C and MIPS sides receive the same RAM,
native calls, renderer calls, and deterministic controller input. Hardware only
functions and the overlay's dedicated drawing entries are intercepted at their
named boundaries; the other overlay functions execute as original MIPS.
"""
from __future__ import annotations

import argparse
import copy
import ctypes as C
import csv
import random
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools/mips"))
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

U32, I32, P = C.c_uint32, C.c_int32, C.c_void_p
STACK_LO, STACK_HI = 0x801FF000, 0x80200000
OVERLAY_BASE, OVERLAY_SIZE = 0x80136000, 0x4000
MUTATION_SELECTOR, MUTATION_MARKER = 0x800FEE00, 0x800FEE04
DEFAULT_CASES, DEFAULT_SEED = 12, 0x189944
DEFAULT_FUNCTION_CASES = {"Mode_LeaveHub": 20, "HubExitUi_Run": 29, "SaveUi_LookAt_189": 13}
RUN_SCENARIOS = (
    # screen state, held, changed, repeat, selection, slot, transition state
    (1, 0, 0, 0, 0, 0, 0), (1, 0, 0, 0, 0, 0, 0),
    (-1, 0, 0, 0, 0, 0, 0),
    (0, 3, 3, 0, 0, 0, 0), (0, 0x800, 0x800, 0, 0, 0, 0),
    (0, 0x4000, 0x4000, 0, 10, 0, 0),
    (0, 0x1000, 0x1000, 0, 2, 0, 0),
    (0, 0x8004, 0x8004, 0, 0, 0, 0),
    (0, 0x2008, 0x2008, 0, 10, 0, 0),
    (3, 0, 0, 8, 1, 0, 0), (2, 0, 0, 0, 0, 0, 0),
    (3, 0, 0, 0x10, 1, 0, 0), (3, 0, 0, 0x80, 1, 0, 0),
    (3, 0, 0, 4, 1, 2, 0), (3, 0, 0, 4, 1, 0, 0),
    (3, 0, 0, 8, 1, 0, 0), (3, 0, 0, 0x2000, 1, 0, 0),
    (3, 0, 0, 0x8000, 1, 1, 0),
    (3, 0, 0, 0x20, 1, 0, 0), (3, 0, 0, 0x40, 1, 0, 0),
    (3, 0, 0, 0x800, 1, 0, 0), (3, 0, 0, 0x40, 1, 0, 0),
    (3, 0, 0, 0x200, 1, 0, 0), (3, 0, 0, 0x11, 1, 0, 0),
    (3, 0, 0, 0x1000, 1, 0, 0), (3, 0, 0, 0x4000, 1, 0, 0),
    (3, 0, 0, 0x8000, 1, 0, 0), (3, 0, 0, 0xA000, 1, 0, 0),
    (3, 0, 0, 0x20, 1, 0, 0),
)

# Name: original address, byte size, exported wrapper, return kind, argument kinds.
FUNCS = {
    "HubExitUi_Init": (0x80136000, 0x6C, "ovl189_init", "s", "u"),
    "Mode_LeaveHub": (0x8013606C, 0x1554, "ovl189_mode", "s", "u"),
    "HubExitUi_DrawModelA": (0x801375C0, 0x154, "ovl189_draw_model_a", "v", "uuuu"),
    "HubExitUi_DrawModelB": (0x80137714, 0x158, "ovl189_draw_model_b", "v", "uuuu"),
    "HubExitUi_DrawModelC": (0x8013786C, 0x0E4, "ovl189_draw_model_c", "v", "uuu"),
    "HubExitUi_Run": (0x80137950, 0x988, "ovl189_run", "s", ""),
    "HubExitUi_IsDestinationUnlocked": (0x801382D8, 0x114, "ovl189_unlocked", "s", "ss"),
    "HubExitUi_GetProgress": (0x801383EC, 0x7C, "ovl189_progress", "s", "ss"),
    "HubExitUi_GetFlags": (0x80138468, 0xF8, "ovl189_flags", "s", "ss"),
    "SaveUi_LookAt_189": (0x80138560, 0x1A0, "ovl189_look_at", "v", "uu"),
    "HubExitUi_DrawText": (0x80138700, 0xFC, "ovl189_draw_text", "v", "u"),
    "SaveUi_StringTableIndex_189": (0x801387FC, 0x30, "ovl189_string_index", "u", "us"),
    "HubExitUi_TextWidth": (0x8013882C, 0x40, "ovl189_text_width", "s", "u"),
}

# EXE functions and SDK APIs that remain native hooks in this port. Argument
# counts are the original o32 argument words used by the overlay.
CALLS = {
    0x8002FAF0: ("SetGeomScreen", 1),
    0x80015C34: ("MeshInitFromAreaTable", 2),
    0x80011194: ("ArcLoadEntry", 1),
    0x800116C0: ("LoadListStep", 0),
    0x800125B8: ("CdMgrUpdate", 0),
    0x8002E520: ("VSync", 1),
    0x80086170: ("SoundStopAndRestoreVolume", 0),
    0x8003272C: ("PlayerInit", 0),
    0x8005DF48: ("BeginFrame", 0),
    0x800306F0: ("RotMatrixZ", 2),
    0x80030890: ("RotMatrixY", 2),
    0x80030A30: ("RotMatrixX", 2),
    0x800161C0: ("ModelTransformRigid", 3),
    0x800180B8: ("ModelEmitPrims", 3),
    0x8001CE54: ("ModelDrawEnvMap", 3),
    0x8001A414: ("ModelEmitPrims", 1),
    0x8002B410: ("SetDrawTPage", 4),
    0x800286CC: ("DrawSync", 1),
    0x80028EFC: ("PutDispEnv", 1),
    0x80028D30: ("PutDrawEnv", 1),
    0x80028CC0: ("DrawOTag", 1),
    0x80097D0C: ("LevelGetInfo", 2),
    0x80097E60: ("LevelTableGet", 1),
    0x8005F68C: ("HudDrawTimeRankIcon", 4),
    0x80095E4C: ("ProgressGetByte", 0),
    0x8005CFEC: ("InitDisplay", 1),
    0x80028350: ("ResetGraph", 1),
    0x80028850: ("ClearImage", 4),
    0x800964B0: ("ProgressGetStageResult", 1),
    0x80097FB0: ("LevelCaughtCount", 1),
}

# Calls to already-diffed C ports execute the same verified port on the MIPS
# side. Their original instruction bodies have their own ledger rows/checkers.
PORTED = {
    0x80010CC0: "AreaGetLevel",
    0x800323F8: "ApplyLevelFog",
    0x80015D10: "StaticModelInit",
    0x80015D58: "SetColorMatrixMode",
    0x8001325C: "PadUpdate",
    0x800160EC: "MeshTransform",
    0x80016D60: "MeshCalcEnvUVs",
    0x800690E8: "AreaSetVramAnimList",
    0x800691F8: "VramScrollUpdate",
    0x8008C09C: "HeapAlloc",
    0x8008C01C: "HeapInit",
    0x8008C390: "ColorFadeReset",
    0x8008C3C0: "ColorFadeSet",
    0x8008C6F0: "DebugTextPrintf",
    0x80086FD0: "SoundStop",
    0x800877F8: "PlaySfx",
    0x80096580: "LevelProgressGet",
    0x800976B8: "HubDestinationState",
    0x8005F978: "HudDrawTime",
}

NO_HOOK_ARGUMENT = {
    "SaveUi_LookAt_189", "SaveUi_StringTableIndex_189", "HubExitUi_TextWidth",
}

AREA_BASE = 0x80154000
AREA_FIXTURES = (1, 2, 3, 4, 5, 19, 24, 30)
M32 = 0xFFFFFFFF
EXTRA_COVERAGE = {}
RENDERER_RAM_MASKS = (
    (0x00000000, 0x2000),
    (0x00008150, 4),
    (0x00008168, 4),
    (0x000AE8F0, 2),
    (0x000EC354, 4),
    (0x000EC3D1, 1),
    (0x000EC378, 1),
    (0x000EC3D4, 2),
    (0x000F03CC, 2),
    (0x000F03F0, 1),
)
C_ONLY_RENDER_HOOKS = {"HubExitUi_RenderRows"}
SIGNATURE_MASKS = {
    ("call", "SetDrawTPage"): (0, "hook SetDrawTPage cursor"),
    ("nested", "SetDrawMove"): (0, "hook SetDrawMove packet offset"),
    ("render", "HubExitUi_DrawText"): (5, "hook HubExitUi_DrawText cursor"),
    ("render", "HubExitUi_RenderMainPanel"): (0, "hook HubExitUi_RenderMainPanel cursor"),
    ("render", "HubExitUi_RenderStatusIcon"): (0, "hook HubExitUi_RenderStatusIcon cursor"),
}


CALL_CB = C.CFUNCTYPE(I32, P, P, C.c_char_p, U32, U32, C.POINTER(U32))
RENDER_CB = C.CFUNCTYPE(I32, P, P, C.c_char_p, U32, C.POINTER(U32))
CALL6 = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32)
RENDER8 = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32, U32, U32)
PAD_GET = C.CFUNCTYPE(I32, P, P, I32)
PAD_INFO = C.CFUNCTYPE(I32, P, P, I32, I32, I32)
PAD_SETUP = C.CFUNCTYPE(None, P, P, I32)
PAD_DEMO = C.CFUNCTYPE(I32, P, P)
PAD_REQUEST = C.CFUNCTYPE(None, P, P, I32, I32, I32)
STICK_BUTTONS = C.CFUNCTYPE(U32, P, P)
PAD_RUMBLE = C.CFUNCTYPE(None, P, P, I32)
WAIT_IDLE = C.CFUNCTYPE(I32, P, P)
WAIT_LOOP = C.CFUNCTYPE(None, P, P, U32)
VOICE_ADSR = C.CFUNCTYPE(None, P, P, U32, C.c_uint16, C.c_uint16)
VOICE_KEYOFF = C.CFUNCTYPE(None, P, P, U32, C.c_uint16)
SOUND_PRINTF = C.CFUNCTYPE(None, P, P, U32, U32, I32, I32, I32)
VOICE_VOLUME = C.CFUNCTYPE(None, P, P, U32, U32, C.c_uint16)
MOVE_IMAGE = C.CFUNCTYPE(I32, P, P, U32, I32, I32)
COLOR_HOOK = C.CFUNCTYPE(None, P, P, U32, U32, U32)
MATRIX_HOOK = C.CFUNCTYPE(None, P, P, U32)
HUD_DRAW = C.CFUNCTYPE(None, P, P, P)


class OvlHooks(C.Structure):
    _fields_ = [("user", P), ("call", CALL_CB), ("render", RENDER_CB)] + [
        (name, P) for name in ("area_set", "items_init", "vram_anim", "progress", "misc",
                               "pad", "effects", "hud", "debug_text", "stage_fade", "sound_hw")
    ]


class SoundHw(C.Structure):
    _fields_ = [("user", P), ("wait_idle", P), ("wait_loop", P), ("set_voice_adsr", P),
                ("voice_key_off", P), ("printf_error", P), ("set_voice_volume", P)]


class ItemsHooks(C.Structure):
    _fields_ = [(name, P) for name in ("user", "GetLevelFirstArea", "printf", "SetBackColor",
                                       "SetFarColor", "SetColorMatrix", "rsin", "rcos")]


class VramHooks(C.Structure):
    _fields_ = [("user", P), ("MoveImage", P)]


class PadHooks(C.Structure):
    _fields_ = [(name, P) for name in ("user", "pad_get_state", "pad_info_mode", "pad_setup_dualshock",
                                       "pad_demo_step", "stage_request", "stick_to_buttons", "pad_rumble_reset")]


class ProgressHooks(C.Structure):
    _fields_ = [(name, P) for name in ("user", "sound_hw", "gimmick_a", "area_set", "SoundLoopsSetPan",
                                       "FUN_8013FB20", "SoundBankSetVolume", "SetDrawTPage")]


class EffectsHooks(C.Structure):
    _fields_ = [(name, P) for name in (
        "user", "EffectRingDraw", "SpawnRandomParticle", "GpuProjectMesh", "GpuDrawMesh",
        "Gadget4TrailUpdate", "GadgetFlashUpdate", "sound", "LocalPositionPadding", "GpuDrawSprite",
        "RibbonDraw", "SetRotMatrix", "SetTransMatrix", "GpuDrawTrailPoint", "GpuDrawEffectObj",
        "SparkAddRandom", "GpuDrawFxPlayerState6", "GpuDrawGroundSpark")]


class HudHooks(C.Structure):
    _fields_ = [("user", P), ("stack_pointer", U32), ("draw_sprite", P), ("draw_quad", P),
                ("draw_glyph", P), ("draw_stage_overlay", P), ("draw_gauge", P), ("draw_message_frame", P),
                ("Ovl_8013D424", P), ("DR_MOVE", P), ("ClearMemory", P), ("ModelDrawScreenSpace", P),
                ("sound_hw", P)]


class Zako2Hooks(C.Structure):
    _fields_ = [(name, P) for name in ("user", "body_hooks", "world_hooks", "spawn_hooks", "items_hooks",
                                       "PlaySfx", "DrawShadow", "ModelTransformRigid", "ModelDraw",
                                       "FUN_8013f660", "DrawProjectileSprite", "DrawDebugGlyph", "FinishDebugText")]


class StageFadeHooks(C.Structure):
    _fields_ = [("user", P), ("stack_pointer", U32), ("caller_a3", U32), ("stage", P), ("sound_hw", P),
                ("DebugTextPrintf", P), ("SeqTrackClose", P), ("SeqTrackSetPlayState", P),
                ("SoundBankFree", P), ("SoundStopAllVoices", P), ("RenderScreenFlash", P),
                ("RenderFadeSpiral", P)]


def u32(value):
    return int(value) & M32


def signed(value, bits=32):
    value = int(value) & ((1 << bits) - 1)
    sign = 1 << (bits - 1)
    return value - (1 << bits) if value & sign else value


class MipsMem:
    def __init__(self, cpu):
        self.cpu = cpu

    def rd8(self, address): return self.cpu.read8(address)
    def rd16(self, address): return self.cpu.read16(address)
    def rd32(self, address): return self.cpu.read32(address)
    def wr8(self, address, value): self.cpu.write8(address, value)
    def wr16(self, address, value): self.cpu.write16(address, value)
    def wr32(self, address, value): self.cpu.write32(address, value)
    def write_bytes(self, address, data): self.cpu.write_bytes(address, data)


class CMem:
    def __init__(self, dll, ram):
        self.dll, self.ram = dll, ram

    def rd8(self, address): return self.dll.ovl189_rd8(self.ram, address)
    def rd16(self, address): return self.dll.ovl189_rd16(self.ram, address)
    def rd32(self, address): return self.dll.ovl189_rd32(self.ram, address)
    def wr8(self, address, value): self.dll.ovl189_wr8(self.ram, address, value)
    def wr16(self, address, value): self.dll.ovl189_wr16(self.ram, address, value)
    def wr32(self, address, value): self.dll.ovl189_wr32(self.ram, address, value)
    def write_bytes(self, address, data):
        pointer = self.dll.ovl189_ptr(self.ram, address, len(data))
        C.memmove(pointer, data, len(data))


class Model:
    def __init__(self, profile):
        self.profile = profile
        self.events = []
        self.mem = None
        self.pad_index = 0

    def matrix_words(self, address):
        if self.mem is None:
            return ()
        return tuple(self.mem.rd32(address + offset) for offset in range(0, 32, 4))

    def call(self, name, address, args):
        words = tuple(u32(x) for x in args)
        if name == "ClearImage" and words and self.mem is not None:
            rectangle = tuple(signed(self.mem.rd16(words[0] + axis * 2), 16)
                              for axis in range(4))
            words = (rectangle, *words[1:])
        if name == "ModelTransformRigid" and len(words) >= 2:
            words = (words[0], ("matrix", self.matrix_words(words[1])), *words[2:])
        self.events.append(("call", name, address & M32, words))
        if name == "LevelGetInfo" and len(words) >= 2:
            out = words[1]
            self.mem.wr32(out, self.profile["time"])
            self.mem.wr16(out + 0x0E, self.profile["required_caught"])
        if name == "LoadListStep":
            pending = self.profile.get("load_steps", 0)
            if pending > 0:
                self.profile["load_steps"] = pending - 1
                return 0
            return -1
        if name == "LevelCaughtCount":
            return self.profile["caught"]
        if name == "ProgressGetStageResult":
            return self.profile["stage_result"]
        if name == "LevelTableGet":
            return self.profile["rank"]
        if name == "ArcLoadEntry":
            entry = words[0]
            for _ in range(16):
                member = self.mem.rd16(entry)
                flags = self.mem.rd16(entry + 2)
                if flags == 0xFFFF:
                    break
                destination = self.mem.rd32(entry + 4)
                asset_path = ROOT / f"extract/members/{member:03X}.bin"
                if not asset_path.is_file():
                    raise FileNotFoundError(f"ArcLoadEntry member {member:03X} is missing")
                self.mem.write_bytes(destination, asset_path.read_bytes())
                entry += 8
            else:
                raise RuntimeError("ArcLoadEntry descriptor has no terminator")
            for index, kind in enumerate(self.profile.get("area_record_kinds", ())):
                if index < self.mem.rd8(AREA_BASE):
                    self.mem.wr8(AREA_BASE + 6 + index * 4, kind)
        return 0

    def render(self, name, args):
        words = tuple(u32(x) for x in args)
        if name == "HubExitUi_ProjectModelVertices" and len(words) >= 2:
            words = (words[0], ("matrix", self.matrix_words(words[1])), *words[2:])
        self.events.append(("render", name, words))
        return 0

    def nested(self, name, args):
        words = tuple(u32(x) for x in args)
        if name == "GpuProjectMesh" and len(words) >= 2:
            words = (words[0], ("matrix", self.matrix_words(words[1])), *words[2:])
        self.events.append(("nested", name, words))
        return 0

    def next_pad(self):
        values = self.profile["pad_values"]
        result = values[min(self.pad_index, len(values) - 1)]
        self.pad_index += 1
        return result


def _u8_arg(value):
    return int(value) & 0xFF


WRAPPER_SOURCE = r'''#include "ae_ovl189.h"
#include "ae_pad.h"
#include "ae_stage.h"
#include "ae_areaset.h"
#include "ae_items_init.h"
#include "ae_progress.h"
#include "ae_zako2.h"
#include "ae_misc.h"
#include "ae_stagefade.h"
#include "ae_effects.h"
#include "ae_sound.h"
#include "ae_hud.h"
#include "ae_vram_anim.h"
#include <stddef.h>

__declspec(dllexport) AeRam *ovl189_ram_create(void) { return ae_ram_create(); }
__declspec(dllexport) uint8_t *ovl189_ptr(AeRam *r, uint32_t a, uint32_t n) { return ae_ptr(r, a, n); }
__declspec(dllexport) uint32_t ovl189_rd8(AeRam *r, uint32_t a) { return ae_rd8(r, a); }
__declspec(dllexport) uint32_t ovl189_rd16(AeRam *r, uint32_t a) { return ae_rd16(r, a); }
__declspec(dllexport) uint32_t ovl189_rd32(AeRam *r, uint32_t a) { return ae_rd32(r, a); }
__declspec(dllexport) void ovl189_wr8(AeRam *r, uint32_t a, uint32_t v) { ae_wr8(r, a, v); }
__declspec(dllexport) void ovl189_wr16(AeRam *r, uint32_t a, uint32_t v) { ae_wr16(r, a, v); }
__declspec(dllexport) void ovl189_wr32(AeRam *r, uint32_t a, uint32_t v) { ae_wr32(r, a, v); }
__declspec(dllexport) int ovl189_failed(const AeRam *r) { return ae_ram_failed(r); }
__declspec(dllexport) const char *ovl189_error(const AeRam *r) { return ae_ram_error(r); }
__declspec(dllexport) void ovl189_clear_error(AeRam *r) { ae_ram_clear_error(r); }

__declspec(dllexport) int32_t ovl189_init(AeRam *r, const AeOvl189Hooks *h, uint32_t a) { return ae_HubExitUi_Init(r,h,a); }
__declspec(dllexport) int32_t ovl189_mode(AeRam *r, const AeOvl189Hooks *h, uint32_t a) { return ae_Mode_LeaveHub(r,h,a); }
__declspec(dllexport) int32_t ovl189_run(AeRam *r,const AeOvl189Hooks*h){return ae_HubExitUi_Run(r,h);}
__declspec(dllexport) int32_t ovl189_unlocked(AeRam *r,const AeOvl189Hooks*h,int32_t a,int32_t b){return ae_HubExitUi_IsDestinationUnlocked(r,h,a,b);}
__declspec(dllexport) int32_t ovl189_progress(AeRam *r,const AeOvl189Hooks*h,int32_t a,int32_t b){return ae_HubExitUi_GetProgress(r,h,a,b);}
__declspec(dllexport) int32_t ovl189_flags(AeRam *r,const AeOvl189Hooks*h,int32_t a,int32_t b){return ae_HubExitUi_GetFlags(r,h,a,b);}
__declspec(dllexport) void ovl189_look_at(AeRam *r,uint32_t a,uint32_t b){ae_SaveUi_LookAt_189(r,a,b);}
__declspec(dllexport) void ovl189_draw_model_a(AeRam*r,const AeOvl189Hooks*h,uint32_t p,uint32_t q,uint32_t m,uint32_t f){ae_HubExitUi_DrawModelA(r,h,p,q,m,f);}
__declspec(dllexport) void ovl189_draw_model_b(AeRam*r,const AeOvl189Hooks*h,uint32_t p,uint32_t q,uint32_t m,uint32_t f){ae_HubExitUi_DrawModelB(r,h,p,q,m,f);}
__declspec(dllexport) void ovl189_draw_model_c(AeRam*r,const AeOvl189Hooks*h,uint32_t p,uint32_t q,uint32_t m){ae_HubExitUi_DrawModelC(r,h,p,q,m);}
__declspec(dllexport) void ovl189_draw_text(AeRam *r,const AeOvl189Hooks*h,uint32_t s){ae_HubExitUi_DrawText(r,h,s);}
__declspec(dllexport) uint32_t ovl189_string_index(AeRam*r,uint32_t s,int32_t i){return ae_SaveUi_StringTableIndex_189(r,s,i);}
__declspec(dllexport) int32_t ovl189_text_width(AeRam*r,uint32_t s){return ae_HubExitUi_TextWidth(r,s);}

__declspec(dllexport) int32_t ovl189_ported(AeRam *r, const AeOvl189Hooks *h, uint32_t pc,
                                             uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    switch (pc) {
    case 0x80010CC0u: return ae_AreaGetLevel(r,h ? h->area_set : 0,(int32_t)a0);
    case 0x800323F8u: ae_ApplyLevelFog(r,h ? (const AeItemsInitHooks *)h->items_init : 0); return 0;
    case 0x80015D10u: ae_StaticModelInit(r,h ? h->area_set : 0,a0,a1); return 0;
    case 0x80015D58u: ae_SetColorMatrixMode(r,h ? h->items_init : 0,a0,a1,a2,a3); return 0;
    case 0x8001325Cu: ae_pad_PadUpdate(r,h ? (const AePadHooks *)h->pad : 0,a0); return 0;
    case 0x800160ECu: ae_MeshTransform(r,h ? (const AeEffectsHooks *)h->effects : 0,a0,a1,a2); return 0;
    case 0x80016D60u: ae_MeshCalcEnvUVs(r,h ? (const AeMiscHooks *)h->misc : 0,a0); return 0;
    case 0x800690E8u: return ae_AreaSetVramAnimList(r,h ? (const AeVramAnimHooks *)h->vram_anim : 0,a0);
    case 0x800691F8u: ae_VramScrollUpdate(r,h ? (const AeMiscHooks *)h->misc : 0); return 0;
    case 0x8008C01Cu: ae_HeapInit(r,h ? h->area_set : 0); return 0;
    case 0x8008C09Cu: return (int32_t)ae_HeapAlloc(r,h ? h->area_set : 0,a0);
    case 0x8008C390u: ae_ColorFadeReset(r); return 0;
    case 0x8008C3C0u: ae_ColorFadeSet(r,(int32_t)a0,(int32_t)a1); return 0;
    case 0x8008C6F0u: ae_DebugTextPrintf(r,h ? (const AeZako2Hooks *)h->debug_text : 0,a0,a1); return 0;
    case 0x80086FD0u: return ae_SoundStop(r,h ? (const AeStageFadeHooks *)h->stage_fade : 0,(int32_t)a0);
    case 0x800877F8u: return ae_PlaySfx(r,h ? (const AeSoundHw *)h->sound_hw : 0,(int32_t)a0);
    case 0x80096580u: return ae_LevelProgressGet(r,a0);
    case 0x800976B8u: return ae_HubDestinationState(r,h ? h->progress : 0,(int32_t)a0,(int32_t)a1);
    case 0x8005F978u: ae_HudDrawTime(r,h ? (const AeHudHooks *)h->hud : 0,(int32_t)a0,(int32_t)a1,(int32_t)a2); return 0;
    default: ae_ram_fail(r,"check_ovl189: unregistered port bridge %08X",(unsigned)pc); return 0;
    }
}

__declspec(dllexport) size_t ovl189_stage_size(void) { return sizeof(AeStageHooks); }
__declspec(dllexport) size_t ovl189_stage_vsync_offset(void) { return offsetof(AeStageHooks,VSync); }
__declspec(dllexport) size_t ovl189_misc_size(void) { return sizeof(AeMiscHooks); }
__declspec(dllexport) size_t ovl189_misc_stack_pointer_offset(void) { return offsetof(AeMiscHooks,stack_pointer); }
__declspec(dllexport) size_t ovl189_misc_drawmove_offset(void) { return offsetof(AeMiscHooks,SetDrawMove); }
__declspec(dllexport) size_t ovl189_misc_render_env_uv_offset(void) { return offsetof(AeMiscHooks,RenderEnvUV); }
'''


def build_dll(source_override: Path | None = None, tag="ovl189"):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    with (ROOT / "tools/verified.csv").open(newline="", encoding="utf-8") as stream:
        sources = sorted({row["source"] for row in csv.DictReader(stream)
                          if row["status"] == "diffed" and row["source"]})
    for required in ("src/core/ae_ram.c", "src/effects/ae_effects_mesh.c",
                     "src/math/native_sdk_apply.c", "src/math/native_sdk_matrix.c",
                     "src/modes/ae_ovl189.c"):
        if required not in sources:
            sources.append(required)
    with tempfile.TemporaryDirectory(prefix="ovl189_", dir=None) as temp:
        wrapper = Path(temp) / "ovl189_exports.c"
        wrapper.write_text(WRAPPER_SOURCE, encoding="utf-8")
        source_paths = [str(source_override if rel == "src/modes/ae_ovl189.c" and source_override else ROOT / rel)
                        for rel in sources]
        dll = out / f"{tag}_{time.time_ns()}.dll"
        gcc = shutil.which("gcc") or shutil.which("x86_64-w64-mingw32-gcc")
        if not gcc:
            raise SystemExit("gcc is required to build the overlay checker DLL")
        include_dirs = ("src/modes", "src/core", "src/world", "src/effects", "src/sound",
                        "src/ui", "src/render", "src/player")
        include_flags = [flag for rel in include_dirs for flag in ("-I", str(ROOT / rel))]
        subprocess.run([gcc, "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                        *include_flags, "-shared", "-static-libgcc", *source_paths,
                        str(wrapper), "-o", str(dll), "-lm"], cwd=ROOT, check=True)
        return dll


def hook_signature(event):
    kind, name, *rest = event
    args = list(rest[-1])
    key = (kind, name)
    if key in SIGNATURE_MASKS:
        index, declaration = SIGNATURE_MASKS[key]
        reg.declare("ram_mask", declaration)
        if index < len(args):
            args[index] = "renderer-owned packet cursor"

    def normalize(value):
        if isinstance(value, tuple):
            return tuple(normalize(item) for item in value)
        if isinstance(value, str):
            return value
        return "STACK" if STACK_LO <= u32(value) < STACK_HI else u32(value)

    normalized = tuple(normalize(value) for value in args)
    if kind == "call":
        return kind, name, rest[0], normalized
    return kind, name, normalized


def renderer_packet_link_masks(*models):
    """Mask only the four-byte tag/link words at emitted SetDrawMove destinations."""
    masks = set()
    for model in models:
        for event in model.events:
            if event[0] != "nested" or event[1] != "SetDrawMove":
                continue
            if not event[2]:
                raise AssertionError("SetDrawMove has no packet destination")
            address = u32(event[2][0])
            if STACK_LO <= address < STACK_HI:
                continue
            if not (0x80000000 <= address and address + 4 <= STACK_LO):
                raise AssertionError(f"SetDrawMove packet destination is outside compared RAM: {address:08X}")
            if address & 3:
                raise AssertionError(f"SetDrawMove packet destination is unaligned: {address:08X}")
            masks.add((address - 0x80000000, 4))
    if masks:
        reg.declare("ram_mask", "hook SetDrawMove packet link fields")
    return sorted(masks)


class CSide:
    def __init__(self, dll_path):
        self.dll = C.CDLL(str(dll_path))
        self._keep = []
        lib = self.dll
        lib.ovl189_ram_create.restype = P
        lib.ovl189_ptr.restype, lib.ovl189_ptr.argtypes = P, [P, U32, U32]
        for name in ("ovl189_rd8", "ovl189_rd16", "ovl189_rd32"):
            f = getattr(lib, name); f.restype = U32; f.argtypes = [P, U32]
        for name in ("ovl189_wr8", "ovl189_wr16", "ovl189_wr32"):
            getattr(lib, name).argtypes = [P, U32, U32]
        lib.ovl189_failed.argtypes, lib.ovl189_failed.restype = [P], C.c_int
        lib.ovl189_error.argtypes, lib.ovl189_error.restype = [P], C.c_char_p
        lib.ovl189_clear_error.argtypes = [P]
        lib.ovl189_ported.argtypes = [P, C.POINTER(OvlHooks), U32, U32, U32, U32, U32]
        lib.ovl189_ported.restype = I32
        self.ram = lib.ovl189_ram_create()
        if not self.ram:
            raise SystemExit("AeRam allocation failed")
        self.main = (C.c_uint8 * 0x200000).from_address(lib.ovl189_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(lib.ovl189_ptr(self.ram, 0x1F800000, 0x400))
        self.model = None
        self.hooks = self._make_hooks()
        self._bind_functions()

    def set_misc_stack_pointer(self, stack_pointer):
        C.c_uint32.from_buffer(self.misc, self.misc_stack_pointer_offset).value = u32(stack_pointer)

    def keep(self, callback):
        self._keep.append(callback)
        return C.cast(callback, P).value

    def _cb(self, proto, label, transform=None):
        def callback(user, ram, *args):
            if transform is not None:
                args = transform(args)
            self.model.nested(label, args)
            return 0
        cb = proto(callback)
        return self.keep(cb)

    def _make_hooks(self):
        lib = self.dll
        def top_call(_user, ram, name, address, count, args):
            self.model.mem = CMem(lib, ram)
            words = [args[i] for i in range(count)] if args else []
            return self.model.call(name.decode("ascii"), address, words)
        def top_render(_user, ram, name, count, args):
            self.model.mem = CMem(lib, ram)
            words = [args[i] for i in range(count)] if args else []
            return self.model.render(name.decode("ascii"), words)
        top_call_cb, top_render_cb = CALL_CB(top_call), RENDER_CB(top_render)
        self._keep.extend((top_call_cb, top_render_cb))
        h = OvlHooks()
        h.call, h.render = top_call_cb, top_render_cb

        self.sound = SoundHw()
        self.sound.wait_idle = self._cb(WAIT_IDLE, "SpuWaitIdle")
        self.sound.wait_loop = self._cb(WAIT_LOOP, "SpuWaitLoop", lambda a: (a[0],))
        self.sound.set_voice_adsr = self._cb(VOICE_ADSR, "SfxSetVoiceAdsr")
        self.sound.voice_key_off = self._cb(VOICE_KEYOFF, "SfxVoiceKeyOff")
        self.sound.printf_error = self._cb(SOUND_PRINTF, "SfxPrintfError")
        self.sound.set_voice_volume = self._cb(VOICE_VOLUME, "SfxSetVoiceVolume")
        h.sound_hw = C.cast(C.pointer(self.sound), P)

        self.area = (P * 64)()
        h.area_set = C.cast(self.area, P)

        self.items = ItemsHooks()
        self.items.SetBackColor = self._cb(COLOR_HOOK, "SetBackColor")
        self.items.SetFarColor = self._cb(COLOR_HOOK, "SetFarColor")
        self.items.SetColorMatrix = self._cb(MATRIX_HOOK, "SetColorMatrix", lambda a: (a[0],))
        h.items_init = C.cast(C.pointer(self.items), P)

        self.vram = VramHooks()
        self.vram.MoveImage = self._cb(MOVE_IMAGE, "MoveImage")
        h.vram_anim = C.cast(C.pointer(self.vram), P)

        self.progress = ProgressHooks()
        self.progress.sound_hw = h.sound_hw
        self.progress.area_set = h.area_set
        for name in ("SoundLoopsSetPan", "FUN_8013FB20", "SoundBankSetVolume", "SetDrawTPage"):
            setattr(self.progress, name, self._cb(CALL6, name))
        h.progress = C.cast(C.pointer(self.progress), P)

        self.misc = C.create_string_buffer(int(lib.ovl189_misc_size()))
        self.misc_stack_pointer_offset = int(lib.ovl189_misc_stack_pointer_offset())
        drawmove = self._cb(CALL6, "SetDrawMove")
        render_env_uv = self._cb(CALL6, "RenderEnvUV")
        C.memmove(C.addressof(self.misc) + int(lib.ovl189_misc_drawmove_offset()), C.byref(C.c_void_p(drawmove)), C.sizeof(P))
        C.memmove(C.addressof(self.misc) + int(lib.ovl189_misc_render_env_uv_offset()), C.byref(C.c_void_p(render_env_uv)), C.sizeof(P))
        h.misc = C.cast(C.pointer(self.misc), P)

        self.pad = PadHooks()
        self.pad.pad_get_state = self._cb(PAD_GET, "PadGetState", lambda a: (a[0],))
        self.pad.pad_info_mode = self._cb(PAD_INFO, "PadInfoMode")
        self.pad.pad_setup_dualshock = self._cb(PAD_SETUP, "PadSetupDualShock", lambda a: (a[0],))
        self.pad.pad_demo_step = self._cb(PAD_DEMO, "PadDemoPlaybackStep", lambda a: ())
        self.pad.stage_request = self._cb(PAD_REQUEST, "StageRequest")
        stick_cb = STICK_BUTTONS(lambda _u, _r: self.model.next_pad())
        self._keep.append(stick_cb)
        self.pad.stick_to_buttons = C.cast(stick_cb, P).value
        self.pad.pad_rumble_reset = self._cb(PAD_RUMBLE, "PadRumbleReset", lambda a: (a[0],))
        h.pad = C.cast(C.pointer(self.pad), P)

        self.effects = EffectsHooks()
        self.effects.GpuProjectMesh = self._cb(CALL6, "GpuProjectMesh")
        self.effects.GpuDrawMesh = self._cb(CALL6, "GpuDrawMesh")
        self.effects.sound = h.sound_hw
        h.effects = C.cast(C.pointer(self.effects), P)

        self.hud = HudHooks()
        self.hud.stack_pointer = 0x801FF000
        for name in ("draw_sprite", "draw_quad", "draw_glyph", "draw_stage_overlay", "draw_gauge", "draw_message_frame"):
            cb = HUD_DRAW(lambda _u, _r, _draw, label=name: self.model.nested(label, ()))
            self._keep.append(cb)
            setattr(self.hud, name, C.cast(cb, P).value)
        self.hud.sound_hw = h.sound_hw
        h.hud = C.cast(C.pointer(self.hud), P)

        self.debug = Zako2Hooks()
        self.debug.DrawDebugGlyph = self._cb(RENDER8, "DrawDebugGlyph")
        self.debug.FinishDebugText = self._cb(RENDER8, "FinishDebugText")
        h.debug_text = C.cast(C.pointer(self.debug), P)

        stage_size = int(lib.ovl189_stage_size())
        self.stage = C.create_string_buffer(stage_size)
        vsync_cb = self._cb(CALL6, "Stage.VSync")
        C.memmove(C.addressof(self.stage) + int(lib.ovl189_stage_vsync_offset()), C.byref(C.c_void_p(vsync_cb)), C.sizeof(P))
        self.fade = StageFadeHooks()
        self.fade.stack_pointer = 0x801FF000
        self.fade.stage = C.cast(C.pointer(self.stage), P)
        self.fade.sound_hw = h.sound_hw
        for name in ("DebugTextPrintf", "SeqTrackClose", "SeqTrackSetPlayState", "SoundBankFree",
                     "SoundStopAllVoices", "RenderScreenFlash", "RenderFadeSpiral"):
            setattr(self.fade, name, self._cb(CALL6, name))
        h.stage_fade = C.cast(C.pointer(self.fade), P)
        return h

    def _bind_functions(self):
        for name, (_address, _size, symbol, ret, kinds) in FUNCS.items():
            f = getattr(self.dll, symbol)
            fixed_args = [P] if name in NO_HOOK_ARGUMENT else [P, C.POINTER(OvlHooks)]
            f.argtypes = fixed_args + [I32 if kind == "s" else U32 for kind in kinds]
            f.restype = {"v": None, "s": I32, "u": U32}[ret]

    def sync_from_mips(self, cpu):
        C.memmove(self.main, bytes(cpu.ram), 0x200000)
        C.memmove(self.scratch, bytes(cpu.scratch), 0x400)
        self.dll.ovl189_clear_error(self.ram)

    def sync_to_mips(self, cpu):
        cpu.ram[:] = bytes(self.main)
        cpu.scratch[:] = bytes(self.scratch)

    def call(self, name, args):
        _, _, symbol, _ret, kinds = FUNCS[name]
        converted = [signed(value) if kind == "s" else u32(value) for value, kind in zip(args, kinds)]
        call_args = [self.ram]
        if name not in NO_HOOK_ARGUMENT:
            call_args.append(C.byref(self.hooks))
        return getattr(self.dll, symbol)(*call_args, *converted)


class Harness:
    def __init__(self, dll_path):
        reg.declare("stack_window", f"{STACK_LO:08X}..{STACK_HI:08X}")
        reg.check_ram_mask(RENDERER_RAM_MASKS)
        self.overlay = (ROOT / "extract/members/189.bin").read_bytes()
        if len(self.overlay) != OVERLAY_SIZE:
            raise SystemExit(f"member 189.bin is {len(self.overlay)} bytes, expected {OVERLAY_SIZE}")
        members = dump_areas.area_members()
        self.areas = []
        for area in AREA_FIXTURES:
            member = members.get(area)
            if member is None:
                continue
            path = ROOT / f"extract/members/{member:03X}.bin"
            if path.is_file():
                self.areas.append((area, path))
        if len(self.areas) < 4:
            raise SystemExit(f"only {len(self.areas)} real area fixtures are available")
        self.m = mr.MipsRam.from_extract()
        self.m.attach_gte_oracle()
        self.exe_base = self.m.snapshot()
        self.coverage = set()
        self.bases = {}
        for area, path in self.areas:
            self.m.restore(self.exe_base)
            self.m.load_area(path.read_bytes(), AREA_BASE)
            self.m.load_area(self.overlay, OVERLAY_BASE)
            self.m.coverage = self.coverage
            self.bases[area] = self.m.snapshot()
        self.c = CSide(dll_path)
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0
        self.total_steps = 0
        self.seed = DEFAULT_SEED
        self.calls_compared = 0

    def _profile(self, name, scenario, seed):
        rng = random.Random(seed)
        caught = rng.choice((0, 1, 8, 12, 0x7FFF))
        mode_pads = (
            [0, 0x10], [0x2000, 0x10], [0, 0x10],
            [0x20, 0x10], [0x40, 0x10], [0x800, 0x10],
            [0, 0x10], [0, 0x10], [0, 0x10],
            [0x1000, 0x10], [0x4000, 0x10], [0, 0x10],
        )
        return {
            "time": rng.choice((0, 1, 60, 3599, 7200, rng.randrange(0x10000))),
            "caught": caught,
            "required_caught": caught if scenario & 1 else ((caught + 1) & 0xFFFF),
            "stage_result": rng.choice((0, 0x10, 0x30, 0xFFFF)),
            "rank": rng.randrange(4),
            "load_steps": int(scenario == 1),
            "area_record_kinds": (0, 3) if name == "Mode_LeaveHub" and scenario == 1 else (),
            "pad_values": ({
                12: [0] * 30 + [0x80],
                13: [0] * 10 + [0x8000, 0x80],
                14: [4] + [0] * 45 + [0x80],
                15: [0] * 22 + [0x80],
                16: [0x800, 0x80],
                17: [0x20, 0x80],
                18: [0x20, 0x80],
                19: [0] * 10 + [0x40, 0x80],
            }.get(scenario, mode_pads[scenario % len(mode_pads)])
                if name == "Mode_LeaveHub" else [0]),
        }

    def _seed_case(self, mem, name, scenario, rng):
        # These are state owned by the game and the overlay. The immutable menu
        # tables themselves remain the bytes from real member 189.bin.
        mem.wr32(0x800EC354, 0x80001000)
        mem.wr32(0x800EC35C, 0)
        mem.wr32(0x800EC350, scenario & 1)
        mem.wr16(0x800AE8F0, scenario * 60)
        mem.wr32(0x800F4470, 0)
        mem.wr16(0x800F4476, (scenario * 7) & 0xFFFF)
        mem.wr32(0x800E5478, 0)
        mem.wr32(0x800E5480, 0)
        mem.wr32(0x800E5488, 0)
        mem.wr32(0x800E5490, 0)
        mem.wr32(0x800E5494, 2)
        mem.wr32(0x800A8A58, 0)
        mem.wr32(0x800DFDCC, 0)
        mem.wr16(0x800BF676, 0xFFFF)
        mem.wr16(0x800BF674, 0xFFFF)
        mem.wr16(0x800BF670, 0xFFFF)
        mem.wr16(0x800BF674 + 2, 0xFFFF)
        mem.wr16(0x800BF676 + 0x10, 0xFFFF)
        mem.wr16(0x800F460A, 0)
        mem.wr32(0x800DF7D8, 0)
        mem.wr32(0x800DF7DC, 0)
        mem.wr32(0x800DF7E0, 0)
        mem.wr8(0x800DF7A0, 0)
        initial_route = 7 if name == "Mode_LeaveHub" and scenario in (12, 15) else (
            6 if name == "Mode_LeaveHub" and scenario == 13 else (
            1 if name == "Mode_LeaveHub" and scenario >= 13 else scenario % 11)
        )
        initial_slot = (2 if name == "Mode_LeaveHub" and scenario in (12, 15) else
                        1 if name == "Mode_LeaveHub" and scenario == 13 else 0)
        mem.wr32(0x800F4350 + 0x2CC, initial_route)
        mem.wr8(0x800F4350 + 0x2CD, initial_slot)
        mem.wr8(0x800F4350 + 0x2CE, scenario & 1)
        progress_mode = scenario % 3
        for level in range(0x20):
            mem.wr8(0x800DFC70 + level, progress_mode)
        mem.wr8(0x800DFC88, progress_mode)
        if name == "Mode_LeaveHub" and scenario in (14, 15, 17):
            for level in range(0x20):
                mem.wr8(0x800DFC70 + level, 2)
            mem.wr32(0x800DFDCC, 1 << 6)
        elif name == "Mode_LeaveHub" and scenario == 16:
            for level in range(0x20):
                mem.wr8(0x800DFC70 + level, 2)
            mem.wr8(0x800DFC88, 0)
            mem.wr32(0x800DFDCC, 1 << 6)
        elif name == "Mode_LeaveHub" and scenario in (17, 18, 19):
            for level in range(0x20):
                mem.wr8(0x800DFC70 + level, 2 if scenario == 17 else 0)
            mem.wr8(0x800DFC88, 0 if scenario == 19 else 2)
            mem.wr32(0x800DFDCC, 0 if scenario == 17 else 1 << 6)
        if name == "Mode_LeaveHub" and scenario == 19:
            mem.wr32(0x800DFDCC, 0xFFFFFFFF)
            mem.wr32(0x800DFDF8, 0xFFFFFFFF)
        mem.wr32(0x80139BC0, 0)
        mem.wr32(0x80139BC4, scenario % 11)
        mem.wr32(0x80139BC8, 0)
        mem.wr32(0x80139BCC, 0)
        mem.wr32(0x80139BD0, 0)
        mem.wr32(0x80139BD4, 0)
        mem.wr32(0x80139BD8, 0)
        if name == "Mode_LeaveHub" and scenario == 15:
            mem.wr32(0x80139BD8, 0xFFFFFFFF)
        mem.wr32(0x80139BE0, scenario * 30)
        mem.wr8(0x80139BE4, scenario % 9)
        # Geometry and animation paths are exercised on real area records, but
        # an empty scroll/animation queue has no renderer-owned setup packets.
        mem.wr32(0x800B98F0, 0)
        mem.wr32(0x800B98F4, 0)
        mem.wr32(0x800B9A00, 0)
        mem.wr32(MUTATION_SELECTOR, 0)
        mem.wr32(MUTATION_MARKER, 0)
        eye, target = 0x801FF100, 0x801FF110
        if name == "SaveUi_LookAt_189" and scenario == 12:
            eye_values = (0, -1400, 0)
            target_values = (0, 0, 2624)
        else:
            eye_values = ((0, 0, 0) if scenario % 12 == 11 else
                          (rng.choice((-300, -1, 0, 400)),
                           rng.choice((-10, 0, 2)), 100))
            target_values = ((0, 0, 0) if scenario % 12 == 11 else
                             (100, 0, rng.choice((0, 2624, -500))))
        for offset, value in enumerate(eye_values):
            mem.wr16(eye + offset * 2, value)
        for offset, value in enumerate(target_values):
            mem.wr16(target + offset * 2, value)
        if name in ("HubExitUi_DrawModelA", "HubExitUi_DrawModelB",
                    "HubExitUi_DrawModelC"):
            position, rotation, model = 0x801FFD60, 0x801FFD70, 0x801FFD80
            mesh, vertices, primitives = 0x801FFDA0, 0x801FFDC0, 0x801FFDE0
            for axis, value in enumerate((scenario * 17 - 80, scenario * 9 - 40, 1800 + scenario)):
                mem.wr16(position + axis * 2, value)
            for axis, value in enumerate((scenario * 31, 2048 - scenario * 13, scenario * 7)):
                mem.wr16(rotation + axis * 2, value)
            mem.wr32(model, mesh)
            mem.wr32(model + 4, vertices)
            mem.wr32(model + 8, primitives)
            mem.wr16(mesh, 2)
            mem.wr16(mesh + 6, 8)
            mem.wr16(mesh + 0x0E, 2)
            for index in range(4):
                mem.wr32(vertices + index * 4, (scenario << 16) | index)
            mem.wr32(mesh + 0x20, 0x00010002)
            mem.wr32(mesh + 0x24, 0x00030004)
            mem.wr32(mesh + 0x28, 0x00050006)
            mem.wr32(mesh + 0x2C, 0x00070008)
        if name == "HubExitUi_Run":
            state, held, changed, repeat, selection, slot, transition = (
                RUN_SCENARIOS[scenario % len(RUN_SCENARIOS)])
            mem.wr32(0x80139BDC, state)
            mem.wr32(0x80139BC4, selection)
            mem.wr32(0x80139BCC, slot)
            mem.wr32(0x80139BD8, transition)
            mem.wr32(0x800E5478, held)
            mem.wr32(0x800E5480, changed)
            mem.wr32(0x800E5488, repeat)
            if scenario in (20, 21):
                for level in range(0x20):
                    mem.wr8(0x800DFC70 + level, 2)
                mem.wr8(0x800DFC88, 0 if scenario == 20 else 2)
                mem.wr32(0x800DFDCC, (1 << 6) if scenario == 20 else 0xFFFFFFFF)
            if scenario == 9:
                for level in range(0x20):
                    mem.wr8(0x800DFC70 + level, 2)
                mem.wr32(0x800DFDCC, 0xFFFFFFFF)
            if scenario == 16:
                for level in range(0x20):
                    mem.wr8(0x800DFC70 + level, 0)
                mem.wr8(0x800DFC70 + 6, 2)
                mem.wr32(0x800DFDCC, 0xFFFFFFFF)
            if scenario == 21:
                mem.wr32(0x800DFDF8, 0xFFFFFFFF)
        return {
            "HubExitUi_Init": (0x801389D0,),
            "Mode_LeaveHub": (0x801389D0,),
            "HubExitUi_Run": (),
            "HubExitUi_IsDestinationUnlocked": (scenario % 11, scenario % 2),
            "HubExitUi_GetProgress": (scenario % 11, scenario % 2),
            "HubExitUi_GetFlags": (scenario % 11, scenario % 2),
            "SaveUi_LookAt_189": (eye, target),
            "HubExitUi_DrawModelA": (0x801FFD60, 0x801FFD70, 0x801FFD80, 4096),
            "HubExitUi_DrawModelB": (0x801FFD60, 0x801FFD70, 0x801FFD80, 4096),
            "HubExitUi_DrawModelC": (0x801FFD60, 0x801FFD70, 0x801FFD80),
            "HubExitUi_DrawText": (0x801399E8,),
            "SaveUi_StringTableIndex_189": (0x801399E8,
                -1 if scenario % 12 == 11 else scenario % 10),
            "HubExitUi_TextWidth": (0x801399E8,),
        }[name]

    def _ported_hook(self, model, cpu, address):
        def invoke(_cpu, regs):
            if address == 0x800691F8:
                model.vram_stack_pointers.append(u32(regs[29]))
                self.c.set_misc_stack_pointer(regs[29])
            self.c.sync_from_mips(cpu)
            self.c.model = model
            model.mem = CMem(self.c.dll, self.c.ram)
            result = self.c.dll.ovl189_ported(self.c.ram, C.byref(self.c.hooks), address,
                                               *[u32(x) for x in regs[4:8]])
            if self.c.dll.ovl189_failed(self.c.ram):
                raise RuntimeError(self.c.dll.ovl189_error(self.c.ram).decode(errors="replace"))
            self.c.sync_to_mips(cpu)
            return u32(result)
        return invoke

    def _mips_patches(self, name, model, cpu):
        patches = {}
        if name in ("Mode_LeaveHub", "HubExitUi_DrawModelA"):
            def project_model_a(_cpu, regs):
                instance = regs[17]
                mesh = cpu.read32(instance)
                vertex_count = cpu.read16(mesh + 0x0E)
                if vertex_count:
                    model.render("HubExitUi_ProjectModelVertices", (
                        instance, regs[29] + 16, cpu.read32(instance + 4),
                        vertex_count, regs[20], 0x1F800000))
                return 0x801376BC
            patches[0x80137668] = project_model_a
        if name in ("Mode_LeaveHub", "HubExitUi_DrawModelB"):
            def project_model_b(_cpu, regs):
                instance = regs[17]
                mesh = cpu.read32(instance)
                vertex_count = cpu.read16(mesh + 0x0E)
                if vertex_count:
                    model.render("HubExitUi_ProjectModelVertices", (
                        instance, regs[29] + 16, cpu.read32(instance + 4),
                        vertex_count, regs[20], 0x1F800000))
                return 0x80137814
            patches[0x801377C0] = project_model_b
        if name in ("Mode_LeaveHub", "HubExitUi_Run", "HubExitUi_DrawText"):
            packet_store_pcs = (
                0x80138740, 0x80138748, 0x8013876C, 0x80138778,
                0x80138788, 0x80138790, 0x80138798, 0x801387A0,
                0x801387A4, 0x801387A8, 0x801387BC, 0x801387C0,
            )

            def draw_glyph(_cpu, regs):
                args = [regs[8], cpu.read16(0x80139CF0),
                        cpu.read16(0x80139CF2), cpu.read16(0x80139CF4),
                        cpu.read32(0x80139CF8), regs[7]]
                model.render("HubExitUi_DrawText", args)
                return 0x80138744

            for pc in packet_store_pcs:
                if pc == 0x80138740:
                    patches[pc] = draw_glyph
                else:
                    patches[pc] = lambda _cpu, _regs, at=pc: at + 4
        if name != "Mode_LeaveHub":
            return patches

        def short_vector(address):
            return tuple(signed(cpu.read16(address + axis * 2), 16)
                         for axis in range(3))

        def status_icon(_cpu, regs):
            cursor_address = regs[19] + 4
            cursor = cpu.read32(cursor_address)
            model.render("HubExitUi_RenderStatusIcon", (
                cursor, cpu.read32(regs[19]), cpu.read32(0x80139BC4),
                cpu.read32(0x80139BCC)))
            cpu.write32(cursor_address, cursor + 48)
            return 0x8013708C

        def progress_badge(_cpu, regs):
            destination = cpu.read32(0x80139BC4)
            route = regs[18]
            cursor_address = regs[19] + 4
            cursor = cpu.read32(cursor_address)
            position = regs[29] + 56
            model.render("HubExitUi_RenderProgressBadge", (
                destination, route, regs[17], *short_vector(position),
                signed(regs[5]), cpu.read16(0x800AE8F0)))
            cpu.write32(cursor_address, cursor + 40)
            return 0x80136B78

        def collected_badge(_cpu, regs):
            destination = cpu.read32(0x80139BC4)
            route = regs[18]
            cursor_address = regs[19] + 4
            cursor = cpu.read32(cursor_address)
            route_record = 0x80139914 + destination * 9 + route * 2
            route_area = cpu.read8(route_record + 1)
            model.render("HubExitUi_RenderCollectedBadge", (
                destination, route, regs[17], route_area,
                cpu.read32(0x80139BD0), cpu.read32(0x80139BD4),
                cpu.read16(0x800AE8F0)))
            cpu.write32(cursor_address, cursor + 40)
            return 0x80136D14

        def stage_status(_cpu, regs):
            level = signed(regs[16])
            if level == 30:
                return 0x80137274

            time = cpu.read32(0x1F800000)
            frame = cpu.read32(0x80139BE0) + 1
            cpu.write32(0x80139BE0, frame)
            hud_regs = list(regs)
            hud_regs[4:8] = (44, 196, time, 0)
            self._ported_hook(model, cpu, 0x8005F978)(cpu, hud_regs)
            rank = model.call("LevelTableGet", 0x80097E60, (level,))
            model.call("HudDrawTimeRankIcon", 0x8005F68C,
                       (24, 180, time, rank))
            model.render("HubExitUi_RenderStageStatus", (
                level, 2, time, rank, cpu.read32(0x80139BD4), frame,
                0x800F4350))
            cursor_address = regs[19] + 4
            cpu.write32(cursor_address, cpu.read32(cursor_address) + 24)
            return 0x80137274

        def main_panel(_cpu, regs):
            cursor_address = regs[19] + 4
            cursor = cpu.read32(cursor_address)
            context_index = cpu.read32(regs[19] + 12)
            animation_address = 0x800AE8F0
            animation = cpu.read16(animation_address)
            model.render("HubExitUi_RenderMainPanel", (
                cursor, context_index, animation,
                cpu.read32(0x80139BC4), cpu.read32(0x80139BC8)))
            for _ in range(92):
                animation = (animation * 5 + 1) & 0xFFFF
            cpu.write16(animation_address, animation)
            model.call("SetDrawTPage", 0x8002B410,
                       (cursor + 92 * 20, 0, 1, 0))
            cpu.write32(cursor_address, cursor + 92 * 20 + 8)
            return 0x8013737C

        patches.update({
            0x80136F9C: status_icon,
            0x801368D4: progress_badge,
            0x80136B8C: collected_badge,
            0x80137134: stage_status,
            0x80137274: main_panel,
        })
        return patches

    def _mips_hooks(self, model, cpu):
        hooks = {}
        for address, (name, count) in CALLS.items():
            def callback(_cpu, regs, label=name, arity=count, address_=address):
                args = list(regs[4:8])[:arity]
                model.mem = MipsMem(cpu)
                return u32(model.call(label, address_, args))
            hooks[address] = callback
        for address, name in PORTED.items():
            hooks[address] = self._ported_hook(model, cpu, address)
        return hooks

    def _args_for(self, name, scenario, rng):
        if name == "SaveUi_LookAt_189":
            return (0x801FF100, 0x801FF110)
        if name in ("HubExitUi_IsDestinationUnlocked", "HubExitUi_GetProgress", "HubExitUi_GetFlags"):
            return (scenario % 11, scenario % 2)
        if name == "HubExitUi_DrawText": return (0x801399E8 + (scenario % 4),)
        if name == "SaveUi_StringTableIndex_189": return (0x801399E8, scenario % 8)
        if name == "HubExitUi_TextWidth": return (0x801399E8 + (scenario % 4),)
        if name == "HubExitUi_Init": return (0x801389D0 + scenario * 8,)
        if name == "Mode_LeaveHub": return (0x801389D0,)
        return ()

    def run(self, name, scenario=0, selector=0):
        seed = self.seed + scenario * 0x9E37 + list(FUNCS).index(name) * 0x1103
        rng = random.Random(seed)
        area, _path = self.areas[scenario % len(self.areas)]
        cpu = self.m
        cpu.restore(self.bases[area])
        cpu.coverage = self.coverage
        cpu.gte_reset()
        m_mem = MipsMem(cpu)
        profile = self._profile(name, scenario, seed)
        args = self._seed_case(m_mem, name, scenario, rng)
        if selector:
            m_mem.wr32(MUTATION_SELECTOR, selector)
        before = cpu.snapshot()
        initial_ram, initial_scratch = bytes(cpu.ram), bytes(cpu.scratch)

        m_model, c_model = Model(copy.deepcopy(profile)), Model(copy.deepcopy(profile))
        m_model.vram_stack_pointers = []
        self.last_m_model, self.last_c_model = m_model, c_model
        m_model.mem = m_mem
        self.c.model = c_model
        C.memmove(self.c.main, initial_ram, 0x200000)
        C.memmove(self.c.scratch, initial_scratch, 0x400)
        self.c.dll.ovl189_clear_error(self.c.ram)
        self.c.hooks.call  # keep the nested call table alive while ctypes enters C
        mips_hooks = self._mips_hooks(m_model, cpu)
        address = FUNCS[name][0]
        steps_before = cpu.steps
        try:
            patches = self._mips_patches(name, m_model, cpu)
            original = cpu.call(address, args, hooks=mips_hooks,
                                patches=patches, limit=2_000_000)
        except Exception as exc:  # noqa: BLE001
            self.fail(name, scenario, f"MIPS execution failed: {exc}")
            return False
        self.total_steps += cpu.steps - steps_before

        # Use the same deterministic host responses for the C-side call.
        C.memmove(self.c.main, initial_ram, 0x200000)
        C.memmove(self.c.scratch, initial_scratch, 0x400)
        self.c.dll.ovl189_clear_error(self.c.ram)
        if name == "Mode_LeaveHub":
            stack_pointers = set(m_model.vram_stack_pointers)
            if len(stack_pointers) > 1:
                self.fail(name, scenario,
                          f"VramScrollUpdate used changing stack pointers: {sorted(stack_pointers)}")
                return False
            self.c.set_misc_stack_pointer(next(iter(stack_pointers), 0))
        else:
            self.c.set_misc_stack_pointer(0)
        self.c.model = c_model
        c_model.mem = CMem(self.c.dll, self.c.ram)
        c_result = self.c.call(name, args)
        self.counts[name] += 1
        if self.c.dll.ovl189_failed(self.c.ram):
            self.fail(name, scenario, self.c.dll.ovl189_error(self.c.ram).decode(errors="replace"))
            return False
        ret_kind = FUNCS[name][3]
        if ret_kind != "v" and u32(c_result) != u32(original[2]):
            self.fail(name, scenario, f"return C={u32(c_result):08X} MIPS={u32(original[2]):08X}")
            return False

        stack_offset = STACK_LO - 0x80000000
        expected = bytearray(cpu.ram[:stack_offset])
        actual = bytes(self.c.main[:stack_offset])
        packet_link_masks = renderer_packet_link_masks(m_model, c_model)
        for off, length in (*RENDERER_RAM_MASKS, *packet_link_masks):
            expected[off:off + length] = actual[off:off + length]
        if actual != expected:
            off = next(i for i, (x, y) in enumerate(zip(actual, expected)) if x != y)
            self.fail(name, scenario,
                      f"RAM differs at {0x80000000 + off:08X}: C={actual[off:off+8].hex()} MIPS={expected[off:off+8].hex()}")
            return False
        if bytes(self.c.scratch) != bytes(cpu.scratch):
            off = next(i for i, (x, y) in enumerate(zip(bytes(self.c.scratch), bytes(cpu.scratch))) if x != y)
            self.fail(name, scenario, f"scratchpad differs at {0x1F800000 + off:08X}")
            return False
        m_events = [hook_signature(event) for event in m_model.events]
        c_events = [hook_signature(event) for event in c_model.events]
        # The screen-zero menu packet loop has no one-to-one MIPS callback;
        # badge and stage hooks are emitted by instruction-address patches above.
        for event in (*m_events, *c_events):
            if event[0] == "render" and event[1] in C_ONLY_RENDER_HOOKS:
                reg.declare("ram_mask", f"hook {event[1]} inline packet block")
        m_events = [event for event in m_events if not
                    (event[0] == "render" and event[1] in C_ONLY_RENDER_HOOKS)]
        c_events = [event for event in c_events if not
                    (event[0] == "render" and event[1] in C_ONLY_RENDER_HOOKS)]
        if c_events != m_events:
            mismatch = next((i for i, pair in enumerate(zip(c_events, m_events))
                             if pair[0] != pair[1]), min(len(c_events), len(m_events)))
            c_event = c_events[mismatch] if mismatch < len(c_events) else "<end>"
            m_event = m_events[mismatch] if mismatch < len(m_events) else "<end>"
            self.fail(name, scenario,
                      f"hook order differs at event {mismatch}/{len(c_events)}/{len(m_events)}: "
                      f"C={c_event!r}; MIPS={m_event!r}")
            return False
        self.calls_compared += len(m_events)
        return True

    def fail(self, name, scenario, message):
        self.failures += 1
        print(f"FAIL {name}[case={scenario}]: {message}", flush=True)

    def all(self, cases=DEFAULT_CASES):
        self.coverage.clear()
        passed = True
        default_scale = cases == DEFAULT_CASES and self.seed == DEFAULT_SEED
        for name in FUNCS:
            function_cases = DEFAULT_FUNCTION_CASES.get(name, cases) if default_scale else cases
            for scenario in range(function_cases):
                passed = self.run(name, scenario) and passed
            print(f"{name}: {self.counts[name]} cases", flush=True)
        return passed

    def finish(self, extra=None, default_run=False, show_coverage=False):
        items = [(name, spec[0], spec[1], True) for name, spec in FUNCS.items()]
        summary = reg.coverage_summary(items, self.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        gaps = {name: row for name, row in summary.items()
                if row["uncovered_count"] or row["stale_declared"]}
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps,
                              extra={"overlay": "189.bin", "areas": [a for a, _ in self.areas]},
                              function_names=list(FUNCS))
        for name, row in gaps.items():
            print(f"COVERAGE GAP {name}: {row['uncovered']}; stale={row['stale_declared']}", flush=True)
        return not self.failures and not gaps


def instrument_mutations(source):
    import re
    mutated = source
    for i, name in enumerate(FUNCS, 1):
        symbol = FUNCS[name][2]
        pattern = re.compile(r"(\b(?:void|int32_t|uint32_t)\s+ae_" + re.escape(name) + r"\s*\([^)]*\)\s*\{)", re.S)
        if name == "SaveUi_LookAt_189":
            c_name = "SaveUi_LookAt_189"
        else:
            c_name = name
        pattern = re.compile(r"(\b(?:void|int32_t|uint32_t)\s+ae_" + re.escape(c_name) + r"\s*\([^)]*\)\s*\{)", re.S)
        injected = "\n".join(
            f"    if (ae_rd32(ram, 0x{MUTATION_SELECTOR:08X}u) == {i * 2 - variant}u) "
            f"ae_wr32(ram, 0x{MUTATION_MARKER:08X}u, 0x18900000u | {i * 2 - variant}u);"
            for variant in (1, 2))
        mutated, count = pattern.subn(r"\1\n" + injected, mutated, count=1)
        if count != 1:
            raise SystemExit(f"could not instrument {symbol} ({name}) for two mutations")
    return mutated


def run_mutations():
    source = ROOT / "src/modes/ae_ovl189.c"
    with tempfile.TemporaryDirectory(prefix="ovl189_mut_") as temp:
        mutant_source = Path(temp) / "ae_ovl189_mutant.c"
        mutant_source.write_text(instrument_mutations(source.read_text(encoding="utf-8")), encoding="utf-8")
        dll = build_dll(mutant_source, "ovl189_mut")
        h = Harness(dll)
        caught = 0
        total = 0
        for i, name in enumerate(FUNCS, 1):
            for variant in (1, 2):
                selector = i * 2 - variant
                before = h.failures
                total += 1
                ok = h.run(name, scenario=variant - 1, selector=selector)
                if not ok and h.failures > before:
                    caught += 1
                else:
                    print(f"MUTATION SURVIVED {name} selector={selector}", flush=True)
        print(f"mutations caught: {caught}/{total}", flush=True)
        return 0 if caught == total else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", type=int, default=DEFAULT_CASES)
    ap.add_argument("--seed", type=int, default=DEFAULT_SEED)
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--mutate", action="store_true")
    args = ap.parse_args()
    if not (ROOT / "extract/disc/SCUS_944.23").is_file() or not (ROOT / "extract/members/189.bin").is_file():
        print("SKIP: real executable and member 189.bin are required")
        return 0
    if args.mutate:
        return run_mutations()
    dll = build_dll()
    h = Harness(dll)
    h.seed = args.seed
    passed = h.all(args.cases)
    coverage_ok = h.finish(EXTRA_COVERAGE,
                           default_run=args.cases == DEFAULT_CASES and args.seed == DEFAULT_SEED,
                           show_coverage=args.coverage)
    print(f"differential: {sum(h.counts.values())} cases, {h.calls_compared} native hooks, "
          f"{h.total_steps} MIPS instructions; {'PASS' if passed and coverage_ok else 'FAIL'}", flush=True)
    return 0 if passed and coverage_ok else 1


if __name__ == "__main__":
    sys.exit(main())
