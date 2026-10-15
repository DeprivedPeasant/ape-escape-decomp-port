"""Shared differential harness for small non-stage mode overlays 183/184/185/18A."""
import argparse
import ctypes as C
import csv
import os
import re
import shutil
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
import check_game  # noqa: E402
import check_stage  # noqa: E402

U, I, P = C.c_uint32, C.c_int32, C.c_void_p
STACK_LO, STACK_HI = 0x801FF000, 0x80200000
MIPS_INSTRUCTION_BUDGET = 2_000_000
MUTATION_SELECTOR, MUTATION_MARKER = 0x801FEF00, 0x801FEF04
WORK = 0x801C0000
OVERLAY_BASES = {"183": 0x80136000, "184": 0x80136000,
                 "185": 0x80146000, "18A": 0x80136000}
OVERLAY_SIZES = {"183": 0x1000, "184": 0x800, "185": 0x1000, "18A": 0x1800}
OVERLAY_SOURCES = {"183": "src/modes/ae_ovl183.c", "184": "src/modes/ae_ovl184.c",
                   "185": "src/modes/ae_ovl185.c", "18A": "src/modes/ae_ovl18a.c"}
_GTE_LIBRARY = None


def attach_cached_gte_oracle(cpu):
    global _GTE_LIBRARY
    if _GTE_LIBRARY is None:
        sys.path.insert(0, str(ROOT / "tools/codex"))
        import gte_oracle
        _GTE_LIBRARY = gte_oracle.build()
    state = (C.c_uint32 * 64)()
    cpu._gte_lib = _GTE_LIBRARY
    cpu.gte_state = state
    cpu.gte_backend = lambda ins, value, lib=_GTE_LIBRARY, regs=state: \
        lib.oracle_gte_step(regs, ins, value & 0xFFFFFFFF)
    cpu.gte_backend._native_function = _GTE_LIBRARY.oracle_gte_step
    cpu.gte_backend._native_state = state
    return cpu

CALL4 = C.CFUNCTYPE(I, P, P, U, U, U, U)
CALL6 = C.CFUNCTYPE(I, P, P, U, U, U, U, U, U)
COLOR3 = C.CFUNCTYPE(None, P, P, U, U, U)
MATRIX1 = C.CFUNCTYPE(None, P, P, U)
TRIG1 = C.CFUNCTYPE(I, P, P, I)
PAD_GET = C.CFUNCTYPE(I, P, P, I)
PAD_INFO = C.CFUNCTYPE(I, P, P, I, I, I)
PAD_SETUP = C.CFUNCTYPE(None, P, P, I)
PAD_DEMO = C.CFUNCTYPE(I, P, P)
PAD_REQUEST = C.CFUNCTYPE(None, P, P, I, I, I)
PAD_STICK = C.CFUNCTYPE(U, P, P)
PLAYER1_HOOK = C.CFUNCTYPE(U, P, P, U, U, U, U)
RENDER6 = C.CFUNCTYPE(I, P, P, U, U, U, U, U, U)
NATIVE = C.CFUNCTYPE(I, P, P, C.c_char_p, U, U, C.POINTER(U))


class PadHooks(C.Structure):
    _fields_ = [("user", P), ("pad_get_state", PAD_GET), ("pad_info_mode", PAD_INFO),
                ("pad_setup_dualshock", PAD_SETUP), ("pad_demo_step", PAD_DEMO),
                ("stage_request", PAD_REQUEST), ("stick_to_buttons", PAD_STICK),
                ("pad_rumble_reset", C.CFUNCTYPE(None, P, P, I))]


class SoundHw(C.Structure):
    _fields_ = [("user", P), ("wait_idle", C.CFUNCTYPE(I, P, P)),
                ("wait_loop", C.CFUNCTYPE(None, P, P, U)),
                ("set_voice_adsr", C.CFUNCTYPE(None, P, P, U, C.c_uint16, C.c_uint16)),
                ("voice_key_off", C.CFUNCTYPE(None, P, P, U, C.c_uint16)),
                ("printf_error", C.CFUNCTYPE(None, P, P, U, U, I, I, I)),
                ("set_voice_volume", C.CFUNCTYPE(None, P, P, U, U, C.c_uint16))]


class Player1Hooks(C.Structure):
    _fields_ = [("user", P)] + [
        (name, PLAYER1_HOOK) for name in (
            "CdIntToPos", "CdPosToInt", "CdControl", "CdReadyCallback", "VSync",
            "printf", "SoundSetVolumeMode", "EventCamCreate",
            "ModelEmitPrimsCycleColors", "DrawRibbonBetween",
            "ModelProjectVertices", "ModelEmitPrims")
    ] + [("sound_hw", P), ("CdMgrQueueCommand", PLAYER1_HOOK)]


class ProgressHooks(C.Structure):
    _fields_ = [("user", P), ("sound_hw", P), ("gimmick_a", P), ("area_set", P)] + [
        (name, CALL4) for name in ("SoundLoopsSetPan", "FUN_8013FB20",
                                    "SoundBankSetVolume", "SetDrawTPage")]


class ItemsHooks(C.Structure):
    _fields_ = [("user", P), ("GetLevelFirstArea", CALL4), ("printf", CALL4),
                ("SetBackColor", COLOR3), ("SetFarColor", COLOR3),
                ("SetColorMatrix", MATRIX1), ("rsin", TRIG1), ("rcos", TRIG1)]


class Zako2Hooks(C.Structure):
    _fields_ = [("user", P), ("body_hooks", P), ("world_hooks", P),
                ("spawn_hooks", P), ("items_hooks", P)] + [
                    (name, CALL4) for name in ("PlaySfx", "DrawShadow", "ModelTransformRigid",
                                               "ModelDraw", "FUN_8013f660")] + [
                    (name, RENDER6) for name in ("DrawProjectileSprite", "DrawDebugGlyph",
                                                 "FinishDebugText")]


def _call_fields(header, struct_name, first_fields=()):
    source = (ROOT / header).read_text(encoding="utf-8")
    body = source.split("typedef struct " + struct_name, 1)[1].split("} " + struct_name + ";", 1)[0]
    fields = list(first_fields)
    for raw in body.splitlines():
        line = raw.split("/*", 1)[0].strip()
        if not line or line.startswith("*"):
            continue
        match = re.match(r"(?:AeStageHookFn|AeAreaSetHookFn|AeMiscCall)\s+(\w+)\s*;", line)
        if match:
            fields.append((match.group(1), CALL4))
            continue
        match = re.match(r"AeCollProbeClimbFn\s+(\w+)\s*;", line)
        if match:
            fields.append((match.group(1), CALL6))
            continue
        if re.search(r"const Ae\w+Hooks\s*\*", line):
            name = re.search(r"\*\s*(\w+)\s*;", line).group(1)
            fields.append((name, P))
    return fields


class StageHooks(C.Structure):
    _fields_ = _call_fields("src/world/ae_stage.h", "AeStageHooks", [("user", P), ("stack_pointer", U)])


class MonkeyHooks(C.Structure):
    _fields_ = [("user", P), ("body", P)] + [
        (name, P) for name in re.findall(
            r"AeMonkeyHookFn\s+(\w+)\s*;",
            (ROOT / "src/monkeys/ae_monkey.h").read_text(encoding="utf-8").split(
                "typedef struct AeMonkeyHooks {", 1)[1].split("} AeMonkeyHooks", 1)[0])]


GADGET_SELECT = C.CFUNCTYPE(None, P, P, I, I)
GADGET_HUD = C.CFUNCTYPE(None, P, P)


class SlotGadgetHooks(C.Structure):
    _fields_ = [("user", P), ("select_gadget", GADGET_SELECT), ("hud_find_icon_pos", GADGET_HUD)]


class StageFadeHooks(C.Structure):
    _fields_ = [("user", P), ("stack_pointer", U), ("caller_a3", U),
                ("stage", P), ("sound_hw", P)] + [
                    (name, CALL4) for name in ("DebugTextPrintf", "SeqTrackClose",
                                               "SeqTrackSetPlayState", "SoundBankFree",
                                               "SoundStopAllVoices", "RenderScreenFlash",
                                               "RenderFadeSpiral")]


class AreaSetHooks(C.Structure):
    _fields_ = _call_fields("src/world/ae_areaset.h", "AeAreaSetHooks", [("user", P), ("body_hooks", P)])


def _misc_fields():
    source = (ROOT / "src/world/ae_misc.h").read_text(encoding="utf-8")
    body = source.split("typedef struct AeMiscHooks", 1)[1].split("} AeMiscHooks;", 1)[0]
    fields = [("user", P), ("stack_pointer", U)]
    for raw in body.splitlines():
        line = raw.split("/*", 1)[0].strip()
        match = re.match(r"AeMiscCall\s+(\w+)\s*;", line)
        if match:
            fields.append((match.group(1), CALL4))
        else:
            match = re.match(r"AeCollProbeClimbFn\s+(\w+)\s*;", line)
            if match:
                fields.append((match.group(1), CALL6))
            elif "const AeSoundHw *" in line:
                fields.append((re.search(r"\*\s*(\w+)\s*;", line).group(1), P))
    return fields


class MiscHooks(C.Structure):
    _fields_ = _misc_fields()


OVL_HOOK_TYPES = {
    "183": type("Ovl183Hooks", (C.Structure,), {"_fields_": [
        ("user", P), ("stack_pointer", U), ("pad", P), ("sound_hw", P),
        ("progress", P), ("stage_fade", P), ("text", P), ("native_call", NATIVE),
        ("renderer", NATIVE)]}),
    "184": type("Ovl184Hooks", (C.Structure,), {"_fields_": [
        ("user", P), ("stack_pointer", U), ("pad", P), ("sound_hw", P),
        ("stage_fade", P), ("text", P), ("native_call", NATIVE), ("renderer", NATIVE)]}),
    "185": type("Ovl185Hooks", (C.Structure,), {"_fields_": [
        ("user", P), ("stack_pointer", U), ("pad", P), ("sound_hw", P),
        ("text", P), ("stage", P), ("misc", P), ("native_call", NATIVE),
        ("renderer", NATIVE)]}),
    "18A": type("Ovl18aHooks", (C.Structure,), {"_fields_": [
        ("user", P), ("stack_pointer", U), ("pad", P), ("sound_hw", P), ("player1", P),
        ("area_set", P), ("items", P), ("progress", P), ("stage_fade", P),
        ("text", P), ("native_call", NATIVE), ("renderer", NATIVE)]}),
}


COMMON_API = {
    0x8005CFEC: ("InitDisplay", 0), 0x80028350: ("ResetGraph", 1),
    0x80028850: ("ClearImage", 4), 0x800286CC: ("DrawSync", 1),
    0x8005DF48: ("BeginFrame", 0), 0x8002E520: ("VSync", 1),
    0x80028EFC: ("PutDispEnv", 1), 0x80028D30: ("PutDrawEnv", 1),
    0x80028CC0: ("DrawOTag", 1), 0x80011194: ("ArcLoadEntry", 1),
    0x80011104: ("LoadListStart", 2), 0x800116C0: ("LoadListStep", 1),
    0x800125B8: ("CdMgrUpdate", 0), 0x8001256C: ("CdMgrStopStream", 1),
    0x8008CC60: ("DebugTextSetCursor", 2), 0x800A342C: ("SoundStopAllVoices", 1),
    0x80025D20: ("CdIntToPos", 2), 0x80025E24: ("CdPosToInt", 1),
    0x8002408C: ("CdControl", 3), 0x80024078: ("CdReadyCallback", 1),
    0x8009B9C8: ("ScreenBandDraw", 4),
    0x80137248: ("FUN_80137248", 1),
    0x8007D1E0: ("MenuItemInitBound", 3), 0x8007D290: ("MenuItemSetRange", 3),
    0x8007D29C: ("MenuItemSetCallback", 2), 0x8007D2A4: ("MenuItemSetStep", 2),
    0x80088548: ("SoundLoopTableGetParams", 3), 0x80086170: ("SoundStopAndRestoreVolume", 0),
    0x800A2640: ("SoundBankSetVolume", 4), 0x800315E0: ("SetBackColor", 3),
    0x80031630: ("SetFarColor", 3), 0x80031600: ("SetColorMatrix", 1),
    0x8002FAF0: ("SetGeomScreen", 1), 0x80015C34: ("MeshInit", 2),
    0x800305B0: ("ScaleMatrixL", 2), 0x80016A60: ("ModelTransformSkinned", 1),
    0x80017CD4: ("ModelDraw", 3), 0x800160EC: ("MeshTransform", 3),
    0x80017EFC: ("MeshDrawAnimColors", 4), 0x80011F6C: ("XaPlay", 1),
    0x8005BEF0: ("StageRenderInit", 0), 0x8009B7B0: ("AreaFeature30Request", 1),
    0x8005DEE0: ("QueueFrameFlip", 4), 0x8014666C: ("Ovl185_DrawText", 4),
    0x8002B52C: ("PadGetState", 1), 0x8002B5F8: ("PadInfoMode", 3),
    0x80012CA0: ("PadSetupDualShock", 1), 0x80013128: ("PadDemoPlaybackStep", 0),
    0x8005D100: ("StageRequest", 3), 0x800140B4: ("StickToButtons", 0),
    0x80014208: ("PadRumbleReset", 1),
    0x800A1F3C: ("SeqTrackClose", 1), 0x800A2B6C: ("SeqTrackSetPlayState", 2),
    0x800A1CCC: ("SoundBankFree", 1),
}

DEBUG_FADE, DEBUG_COLOR, DEBUG_PRIMITIVES = 0x800DF7D8, 0x800DF7F8, 0x800DF7FC
RENDER_STATE, DEBUG_OT, OT_STRIDE = 0x800EC350, 0x800F04D0, 0x4078
FONT_TABLE = 0x800B3390


def u32(value):
    return int(value) & 0xFFFFFFFF


def s32(value):
    value &= 0xFFFFFFFF
    return value - 0x100000000 if value & 0x80000000 else value


def build(member, source_override=None):
    source = OVERLAY_SOURCES[member]
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"ovl{member}_{os.getpid()}_{time.time_ns()}.dll"
    exports = dll.with_suffix(".def")
    with (ROOT / "tools/verified.csv").open(newline="", encoding="utf-8") as f:
        modules = {row["source"] for row in csv.DictReader(f) if row["status"] == "diffed"}
    modules.update(check_game.SOURCES)
    modules.add(source)
    inputs = [str((ROOT / rel).resolve()) for rel in sorted(modules)]
    if source_override is not None:
        inputs = [str(source_override.resolve()) if p == str((ROOT / source).resolve()) else p for p in inputs]
    exports_list = ["ae_ram_create", "ae_ram_clear_error", "ae_ram_failed", "ae_ram_error", "ae_ptr",
                    "ae_rd8", "ae_rd16", "ae_rd32", "ae_wr8", "ae_wr16", "ae_wr32"]
    exports_list += [spec[2] for spec in CONFIGS[member]["functions"].values()]
    exports.write_text("EXPORTS\n" + "\n".join(exports_list) + "\n", encoding="ascii")
    gcc = shutil.which("gcc")
    if not gcc:
        raise SystemExit("gcc is required to build the overlay differential checker")
    subprocess.run([gcc, "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-ffunction-sections", "-fdata-sections", "-fvisibility=hidden",
                    "-Wl,--gc-sections", "-Wl,--exclude-all-symbols", "-shared", "-static-libgcc",
                    "-I" + str(ROOT / "src/world"), "-I" + str(ROOT / "src/modes"),
                    "-I" + str(ROOT / "src/player"), "-I" + str(ROOT / "src/sound"),
                    "-I" + str(ROOT / "src/monkeys"), "-I" + str(ROOT / "src/bodies"),
                    *inputs, str(exports), "-o", str(dll), "-lm"], cwd=ROOT, check=True)
    return dll


class MipsMem:
    def __init__(self, cpu): self.cpu = cpu
    def rd8(self, a): return self.cpu.read8(a)
    def rd16(self, a): return self.cpu.read16(a)
    def rd32(self, a): return self.cpu.read32(a)
    def wr8(self, a, v): self.cpu.write8(a, v & 0xFF)
    def wr16(self, a, v): self.cpu.write16(a, v & 0xFFFF)
    def wr32(self, a, v): self.cpu.write32(a, v & 0xFFFFFFFF)
    def write_bytes(self, a, values):
        for i, value in enumerate(values): self.cpu.write8(a + i, value)


class CMem:
    def __init__(self, side): self.side = side
    def rd8(self, a): return self.side.dll.ae_rd8(self.side.ram, a)
    def rd16(self, a): return self.side.dll.ae_rd16(self.side.ram, a)
    def rd32(self, a): return self.side.dll.ae_rd32(self.side.ram, a)
    def wr8(self, a, v): self.side.dll.ae_wr8(self.side.ram, a, v & 0xFF)
    def wr16(self, a, v): self.side.dll.ae_wr16(self.side.ram, a, v & 0xFFFF)
    def wr32(self, a, v): self.side.dll.ae_wr32(self.side.ram, a, v & 0xFFFFFFFF)
    def write_bytes(self, a, values):
        for i, value in enumerate(values): self.side.dll.ae_wr8(self.side.ram, a + i, value)


class Model:
    def __init__(self, profile, mem):
        self.profile = dict(profile)
        self.mem = mem
        self.events = []
        self.load_index = 0
        self.pad_sample_index = 0
        self.stick_sample_index = 0
        self.next_alloc = 0x801C0000

    def call(self, name, args):
        # These two SDK helpers execute as original MIPS on the reference side
        # and as AeRam ports inside AreaLoad on the C side. Mirror their RAM
        # effects without adding native-hook events on the C side.
        if name == "HeapInit":
            for index in range(64): self.mem.wr32(0x800BF690 + index * 4, 0)
            self.mem.wr32(0x800BF790, 0)
            self.mem.wr32(0x800DF798, 0x80102800)
            return 0
        if name == "AreaBumpAlloc":
            size = u32(args[0]) if args else 0
            cursor = self.mem.rd32(0x800DF798)
            if cursor + size > 0x801027FF:
                return 0
            self.mem.wr32(0x800DF798, u32(cursor + ((size + 3) & ~3)))
            return cursor
        if name == "SetColorMatrixMode":
            mode = u32(args[0]) if args else 0
            if mode == 0:
                self.call("SetBackColor", [0, 0, 0])
                for offset in (0, 2, 6, 8, 12, 14):
                    self.mem.wr16(0x800EBFF0 + offset, 4096)
                for offset in (4, 10, 16): self.mem.wr16(0x800EBFF0 + offset, 0)
                self.call("SetColorMatrix", [0x800EBFF0])
            return 0
        if name in EVENT_ARITIES:
            args = args[:EVENT_ARITIES[name]]
        args = tuple(u32(x) for x in args)
        self.events.append((name, args))
        if name == "PadGetState":
            sequence = self.profile.get("pad_button_sequence")
            if sequence is not None and args and args[0] == 0:
                index = min(self.pad_sample_index, len(sequence) - 1)
                button = sequence[index] if sequence else 0
                packet = (~button) & 0xFFFF
                self.mem.wr8(0x800E52D2, packet >> 8)
                self.mem.wr8(0x800E52D3, packet)
                self.pad_sample_index += 1
            return 0
        if name == "PadInfoMode": return 0
        if name == "PadSetupDualShock" or name in ("PadRumbleReset", "StageRequest", "PadDemoPlaybackStep"):
            return int(self.profile.get("pad_demo_result", 0)) if name == "PadDemoPlaybackStep" else 0
        if name == "StickToButtons":
            sequence = self.profile.get("stick_button_sequence")
            if sequence is not None:
                index = min(self.stick_sample_index, len(sequence) - 1)
                value = sequence[index] if sequence else 0
                self.stick_sample_index += 1
                return u32(value)
            return u32(self.profile.get("stick_buttons", 0))
        if name == "DebugTextSetCursor":
            self.mem.wr32(DEBUG_FADE + 8, args[0]); self.mem.wr32(DEBUG_FADE + 12, args[1]); return 0
        if name == "SoundLoopTableGetParams":
            selection, low_out, high_out = args
            record = 0x800B2D08 + selection * 8
            self.mem.wr32(low_out, s32(self.mem.rd16(record + 2) if self.mem.rd16(record + 2) < 0x8000 else self.mem.rd16(record + 2) - 0x10000))
            self.mem.wr32(high_out, s32(self.mem.rd16(record + 4) if self.mem.rd16(record + 4) < 0x8000 else self.mem.rd16(record + 4) - 0x10000))
            return 0
        if name == "LoadListStep":
            seq = self.profile.get("load_sequence", [-1])
            result = seq[min(self.load_index, len(seq) - 1)] if seq else -1
            self.load_index += 1
            return s32(result)
        if name == "MonkeyClearWords":
            address, words = args
            for index in range(words): self.mem.wr32(address + index * 4, 0)
            return 0
        if name == "CdIntToPos":
            sector, output = args
            self.mem.wr32(output, sector ^ 0x5A37C91D)
            return 0
        if name == "CdPosToInt":
            return self.mem.rd32(args[0]) ^ 0x13579BDF
        if name == "CdControl":
            command, _parameter, output = args
            if output != 0: self.mem.wr32(output, 0xCD000000 | (command & 0xFF))
            return 1
        if name == "ScreenBandDraw":
            self.mem.wr32(0x800EC354, u32(self.mem.rd32(0x800EC354) + 0x28))
            return 0
        if name == "AreaFeature30Request":
            self.mem.wr8(0x800F5AA0, 1)
            self.mem.wr8(0x800F5AA3, args[0])
            return 0
        if name == "AreaSetFeature30":
            self.mem.wr32(0x800F5AA0, 0)
            self.mem.wr32(0x800F5AA8, args[0])
            self.mem.wr32(0x800F5AAC, 0)
            self.mem.wr32(0x800F5AB0, 0)
            return 0
        if name == "CdMgrQueueCommand":
            if len(args) >= 4 and args[0] == 0x20 and args[3] != 0:
                self.mem.wr8(args[3], 1)
            return 0
        if name == "AreaBumpAllocSet":
            self.mem.wr32(0x800DF798, args[0])
            return 0
        if name == "CdMgrUpdate":
            manager = 0x800E3890
            read_index = self.mem.rd16(manager + 0x74)
            write_index = self.mem.rd16(manager + 0x76)
            if read_index != write_index:
                entry = manager + 0x78 + read_index * 0x1C
                done_flag = self.mem.rd32(entry + 0x14)
                if done_flag != 0: self.mem.wr8(done_flag, 1)
                read_index = (read_index + 1) % 16
                self.mem.wr16(manager + 0x74, read_index)
                if read_index == write_index: self.mem.wr32(manager, 0)
            return 0
        if name == "AreaGetKind": return int(self.profile.get("area_kind", 0))
        if name == "AreaGetLevel": return int(self.profile.get("area_level", 1))
        if name == "GetLevelFirstArea": return int(self.profile.get("first_area", 1))
        if name == "AreaUsesAltMember": return 0
        if name == "ArcGetMemberPos":
            if len(args) >= 2:
                self.mem.wr32(args[0], 0)
                self.mem.wr32(args[0] + 4, 0)
            return 0
        if name == "MeshInit": return 0
        if name in ("SetBackColor", "SetFarColor", "SetColorMatrix", "ScaleMatrixL",
                    "ModelTransformSkinned", "ModelDraw", "MeshTransform", "MeshDrawAnimColors",
                    "Ovl185_DrawText", "Ovl185_DrawSelection", "SoundTest_DrawMainMenuSelection",
                    "Ovl18A_DrawClearPanel", "Ovl18A_DrawAnimatedPanel", "FinishDebugText",
                    "DrawDebugGlyph", "QueueFrameFlip", "StageRenderInit", "ClearImage",
                    "DrawSync", "PutDispEnv", "PutDrawEnv", "DrawOTag", "VSync", "BeginFrame",
                    "InitDisplay", "ResetGraph", "ArcLoadEntry", "LoadListStart", "CdMgrUpdate",
                    "CdMgrStopStream", "XaPlay", "SoundStopAndRestoreVolume", "SoundStopAllVoices",
                    "SoundBankSetVolume", "SeqTrackClose", "SeqTrackSetPlayState", "SoundBankFree",
                    "SetGeomScreen", "AreaSetVramAnimList", "AreaSetZoneBoxes", "IndicatorsReset",
                    "AreaSetFeature31", "AreaSetFeature32", "AreaSetSpecialActor", "AreaSetIndicatorModelA",
                    "AreaSetIndicatorModelB", "HeapInit", "AreaBumpAllocSet", "AreaSpecialFlagsClear",
                    "GimmickSaveStates", "GimmickSaveApplyProgress", "ItemSaveCollectedMask",
                    "ZakoSaveDefeatedMask", "GimmickSaveTableReset", "ItemsClearFlags", "ZakoClearSlotTable",
                    "SoundSilence", "LevelMonkeyCountGet", "GetZoneMask", "FUN_80030890", "FUN_800A3700",
                    "SetColorMatrixMode", "SoundLoopsStopAll", "BuildTerrainBlockMask", "TableClearDAT_800BEF60",
                    "MenuItemInitBound", "MenuItemSetRange", "MenuItemSetCallback", "MenuItemSetStep",
                    "DebugMenuInitPlayerParams", "DebugMenuInitCounters", "DebugMenuInitCounters2",
                    "MessageBoxClose", "PlayerSpawnAtEntry", "TargetListClear", "ZakoInit", "MonkeyLodModelsInit",
                    "FxSlotsReset", "ProjectilesReset", "EffectsReset", "ColliderZoneMasksUpdate", "GimmickSpawnAll",
                    "BossInit", "ItemsLoadForArea", "RcCarDespawn", "CutsceneModeOff", "CutsceneModeOn", "PlayerInit",
                    "PlayerSetMode", "WaitFrameFlip", "BgmFadeOut", "SoundStop", "PlaySfx", "ProgressSetByte",
                    "PlayTimeAdd", "StageGradeUpdate", "StageVictoryStart", "StageTypeClearInit", "StageTypeClearUpdate",
                    "StageIntroStart", "PlayerEnterCutsceneMode", "StageIntroUpdate", "GimmickNopUpdate3", "GimmickNopUpdate4",
                    "XaPlay", "CdMgrUpdate", "FadeFrame", "FUN_80096638", "FUN_8013EA24", "Printf", "exit",
                    "FUN_80028850", "FUN_800286CC", "ApplyLevelFog", "BgmStart", "BgmUpdateVolume", "ScreenBandDraw",
                    "FUN_80137248", "SoundLoopsSetPan", "FUN_8013FB20", "SetDrawTPage", "RenderScreenFlash", "RenderFadeSpiral"):
            return 0
        if name == "AreaGetKind" or name == "AreaGetLevel": return 0
        return int(self.profile.get("returns", {}).get(name, 0))


class CPort:
    def __init__(self, member, dll):
        self.member = member
        self.dll = C.CDLL(str(dll))
        d = self.dll
        d.ae_ram_create.restype = P
        d.ae_ptr.argtypes = [P, U, U]; d.ae_ptr.restype = P
        for n, t in (("ae_rd8", U), ("ae_rd16", U), ("ae_rd32", U)):
            getattr(d, n).argtypes = [P, U]; getattr(d, n).restype = t
        for n in ("ae_wr8", "ae_wr16", "ae_wr32"): getattr(d, n).argtypes = [P, U, U]
        d.ae_ram_clear_error.argtypes = [P]
        d.ae_ram_failed.argtypes = [P]; d.ae_ram_failed.restype = I
        d.ae_ram_error.argtypes = [P]; d.ae_ram_error.restype = C.c_char_p
        self.ram = d.ae_ram_create()
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(d.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(d.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.hooks = OVL_HOOK_TYPES[member]()
        self.player1_hooks = Player1Hooks()
        self.pad = PadHooks(); self.sound = SoundHw(); self.progress = ProgressHooks()
        self.items = ItemsHooks(); self.text = Zako2Hooks(); self.stage = StageHooks()
        self.monkey = MonkeyHooks(); self.gadget_slots = SlotGadgetHooks()
        self.stage_fade = StageFadeHooks(); self.area_set = AreaSetHooks(); self.misc = MiscHooks()
        self.callbacks = []
        self.mem = CMem(self)
        self.active_model = None
        self._configure_struct_callbacks()
        self.hooks.user = None; self.hooks.stack_pointer = mr.DEFAULT_SP
        self.hooks.pad = C.cast(C.pointer(self.pad), P)
        self.hooks.sound_hw = C.cast(C.pointer(self.sound), P)
        if member == "18A":
            self.player1_hooks.sound_hw = C.cast(C.pointer(self.sound), P)
            self.hooks.player1 = C.cast(C.pointer(self.player1_hooks), P)
        if member in ("183", "18A"): self.hooks.progress = C.cast(C.pointer(self.progress), P)
        if member in ("183", "184", "18A"): self.hooks.stage_fade = C.cast(C.pointer(self.stage_fade), P)
        if member in ("183", "184", "185", "18A"): self.hooks.text = C.cast(C.pointer(self.text), P)
        if member == "185":
            self.hooks.stage = C.cast(C.pointer(self.stage), P)
            self.hooks.misc = C.cast(C.pointer(self.misc), P)
        if member == "18A":
            self.hooks.area_set = C.cast(C.pointer(self.area_set), P)
            self.hooks.items = C.cast(C.pointer(self.items), P)
        self.native_cb = NATIVE(self._native)
        self.renderer_cb = NATIVE(self._renderer)
        self.callbacks += [self.native_cb, self.renderer_cb]
        self.hooks.native_call = self.native_cb; self.hooks.renderer = self.renderer_cb
        self.funcs = {}
        for name, spec in CONFIGS[member]["functions"].items():
            fn = getattr(d, spec[2])
            argtypes = [P] + ([P] if name not in CONFIGS[member]["no_hooks"] else [])
            argtypes += [I if k == "s" else U for k in spec[4]]
            fn.argtypes = argtypes; fn.restype = {"v": None, "s": I, "u": U}[spec[3]]
            self.funcs[name] = fn

    def _keep(self, callback):
        self.callbacks.append(callback); return callback

    def _configure_struct_callbacks(self):
        self._keep(PAD_GET(lambda _u, _r, port: self.active_model.call("PadGetState", [port])))
        self.pad.pad_get_state = self.callbacks[-1]
        self._keep(PAD_INFO(lambda _u, _r, port, a, b: self.active_model.call("PadInfoMode", [port, a, b])))
        self.pad.pad_info_mode = self.callbacks[-1]
        self._keep(PAD_SETUP(lambda _u, _r, arg: self.active_model.call("PadSetupDualShock", [arg])))
        self.pad.pad_setup_dualshock = self.callbacks[-1]
        self._keep(PAD_DEMO(lambda _u, _r: self.active_model.call("PadDemoPlaybackStep", [])))
        self.pad.pad_demo_step = self.callbacks[-1]
        self._keep(PAD_REQUEST(lambda _u, _r, a, b, c: self.active_model.call("StageRequest", [a, b, c])))
        self.pad.stage_request = self.callbacks[-1]
        self._keep(PAD_STICK(lambda _u, _r: self.active_model.call("StickToButtons", [])))
        self.pad.stick_to_buttons = self.callbacks[-1]
        self._keep(C.CFUNCTYPE(None, P, P, I)(lambda _u, _r, port: self.active_model.call("PadRumbleReset", [port])))
        self.pad.pad_rumble_reset = self.callbacks[-1]

        self._keep(C.CFUNCTYPE(I, P, P)(lambda _u, _r: 0)); self.sound.wait_idle = self.callbacks[-1]
        self._keep(C.CFUNCTYPE(None, P, P, U)(lambda _u, _r, units: None)); self.sound.wait_loop = self.callbacks[-1]
        self._keep(C.CFUNCTYPE(None, P, P, U, C.c_uint16, C.c_uint16)(lambda *_: None)); self.sound.set_voice_adsr = self.callbacks[-1]
        self._keep(C.CFUNCTYPE(None, P, P, U, C.c_uint16)(lambda *_: None)); self.sound.voice_key_off = self.callbacks[-1]
        self._keep(C.CFUNCTYPE(None, P, P, U, U, I, I, I)(lambda *_: None)); self.sound.printf_error = self.callbacks[-1]
        self._keep(C.CFUNCTYPE(None, P, P, U, U, C.c_uint16)(lambda *_: None)); self.sound.set_voice_volume = self.callbacks[-1]

        if self.member == "18A":
            for name, arity in (("CdIntToPos", 2), ("CdPosToInt", 1),
                                ("CdControl", 3), ("CdReadyCallback", 1),
                                ("VSync", 1), ("printf", 4)):
                def player1_callback(_user, _ram, a0, a1, a2, a3, label=name, count=arity):
                    return self.active_model.call(label, [a0, a1, a2, a3][:count])
                setattr(self.player1_hooks, name, self._keep(PLAYER1_HOOK(player1_callback)))

        self._fill_call4(self.progress, ("SoundLoopsSetPan", "FUN_8013FB20", "SoundBankSetVolume", "SetDrawTPage"))
        self._keep(COLOR3(lambda _u, _r, a, b, c: self.active_model.call("SetBackColor", [a, b, c])))
        self.items.SetBackColor = self.callbacks[-1]
        self._keep(COLOR3(lambda _u, _r, a, b, c: self.active_model.call("SetFarColor", [a, b, c])))
        self.items.SetFarColor = self.callbacks[-1]
        self._keep(MATRIX1(lambda _u, _r, a: self.active_model.call("SetColorMatrix", [a])))
        self.items.SetColorMatrix = self.callbacks[-1]
        self._keep(TRIG1(lambda _u, _r, a: self.active_model.call("rsin", [a])))
        self.items.rsin = self.callbacks[-1]
        self._keep(TRIG1(lambda _u, _r, a: self.active_model.call("rcos", [a])))
        self.items.rcos = self.callbacks[-1]

        self._fill_call4(self.stage, [n for n, t in self.stage._fields_
                                     if t is CALL4 and n not in ("user", "stack_pointer")])
        self._keep(CALL4(lambda _u, _r, a, b, c, d:
                         self.active_model.call("MonkeyClearWords", [a, b, c, d])))
        self.monkey.ClearWords = C.cast(self.callbacks[-1], P)
        self._keep(CALL4(lambda _u, _r, a, b, c, d:
                         self.active_model.call("VSync", [a, b, c, d])))
        self.monkey.VSync = C.cast(self.callbacks[-1], P)
        for label in ("MenuItemInitBound", "MenuItemSetRange",
                      "MenuItemSetCallback", "MenuItemSetStep"):
            self._keep(CALL4(lambda _u, _r, a, b, c, d, name=label:
                             self.active_model.call(name, [a, b, c, d])))
            setattr(self.monkey, label, C.cast(self.callbacks[-1], P))
        self._keep(GADGET_SELECT(lambda _u, _r, gadget, old:
                                 self.active_model.call("PlayerSelectGadget", [gadget, old])))
        self.gadget_slots.select_gadget = self.callbacks[-1]
        self._keep(GADGET_HUD(lambda _u, _r:
                              self.active_model.call("HudFindGadgetIconPos", [])))
        self.gadget_slots.hud_find_icon_pos = self.callbacks[-1]
        self.stage.monkey_hooks = C.cast(C.pointer(self.monkey), P)
        self.stage.gadget_hooks = C.cast(C.pointer(self.gadget_slots), P)
        self._fill_call4(self.stage_fade, [n for n, _t in self.stage_fade._fields_ if n not in ("user", "stack_pointer", "caller_a3", "stage", "sound_hw")])
        self.stage.stack_pointer = mr.DEFAULT_SP
        self.stage_fade.stack_pointer = mr.DEFAULT_SP
        self.stage_fade.stage = C.cast(C.pointer(self.stage), P)
        self.stage_fade.sound_hw = C.cast(C.pointer(self.sound), P)
        self._fill_call4(self.area_set, [n for n, _t in self.area_set._fields_ if n not in ("user", "body_hooks")])
        self._fill_call4(self.misc, [n for n, t in self.misc._fields_ if t is CALL4])
        self.misc.stack_pointer = mr.DEFAULT_SP
        for name, kind in self.text._fields_:
            if kind is CALL4:
                self._install_call4(self.text, name)
            elif kind is RENDER6:
                self._keep(RENDER6(lambda _u, _r, a, b, c, d, e, f, label=name:
                                   self.active_model.call(label, [a, b, c, d, e, f])))
                setattr(self.text, name, self.callbacks[-1])

    def _install_call4(self, obj, name):
        self._keep(CALL4(lambda _u, _r, a, b, c, d, label=name:
                         self.active_model.call(label, [a, b, c, d])))
        setattr(obj, name, self.callbacks[-1])

    def _fill_call4(self, obj, names):
        for name in names: self._install_call4(obj, name)

    def _native(self, _u, _ram, name, address, argc, words):
        label = name.decode("ascii")
        return self.active_model.call(label, [words[i] for i in range(argc)])

    def _renderer(self, _u, _ram, name, address, argc, words):
        label = name.decode("ascii")
        return self.active_model.call(label, [words[i] for i in range(argc)])

    def sync(self, snapshot):
        C.memmove(self.main, snapshot[0], mr.RAM_SIZE)
        C.memmove(self.scratch, snapshot[1], mr.SCRATCH_SIZE)
        self.dll.ae_ram_clear_error(self.ram)

    def call(self, name, args):
        spec = CONFIGS[self.member]["functions"][name]
        converted = [s32(v) if kind == "s" else u32(v) for v, kind in zip(args, spec[4])]
        call_args = [self.ram]
        if name not in CONFIGS[self.member]["no_hooks"]: call_args.append(C.byref(self.hooks))
        call_args.extend(converted)
        return self.funcs[name](*call_args)


def debug_patches(model):
    def small(cpu, regs):
        glyph = s32(regs[11]); packet = cpu.read32(RENDER_STATE + 4)
        x = cpu.read32(DEBUG_FADE + 8); y = cpu.read32(DEBUG_FADE + 12)
        if glyph != 0:
            ot = DEBUG_OT + cpu.read32(RENDER_STATE + 0x0C) * OT_STRIDE
            model.call("DrawDebugGlyph", [glyph, x, y, cpu.read32(DEBUG_COLOR), ot, packet])
            packet = u32(packet + 0x14)
        cpu.write32(DEBUG_FADE + 8, u32(x + 8)); regs[6] = packet
        cpu.write32(RENDER_STATE + 4, packet)
        return 0x8008C570

    def large(cpu, regs):
        glyph = s32(regs[11]); x = cpu.read32(DEBUG_FADE + 8); y = cpu.read32(DEBUG_FADE + 12)
        glyph_data = u32(FONT_TABLE + glyph * 2); packet = cpu.read32(RENDER_STATE + 4)
        model.call("DrawDebugGlyph", [glyph, x, y, cpu.read32(DEBUG_COLOR), glyph_data, packet])
        packet = u32(packet + 0x28); cpu.write32(DEBUG_FADE + 8, u32(x + cpu.read8(glyph_data + 1)))
        regs[8] = packet; cpu.write32(RENDER_STATE + 4, packet)
        return 0x8008C6E4

    def finish(cpu, regs):
        packet = cpu.read32(RENDER_STATE + 4); ot = regs[23]
        model.call("FinishDebugText", [ot, packet, cpu.read32(DEBUG_FADE),
                    cpu.read32(DEBUG_FADE + 4), cpu.read32(DEBUG_COLOR),
                    cpu.read32(DEBUG_PRIMITIVES)])
        cpu.write32(RENDER_STATE + 4, u32(packet + 8))
        return 0x8008CAA4
    return {0x8008C474: small, 0x8008C57C: large, 0x8008CA34: finish}


def _button_setup(mem, button=None, stick=False):
    for addr in (0x800E52D0, 0x800E52F2):
        mem.wr8(addr, 0); mem.wr8(addr + 1, 0x41)
        mem.wr8(addr + 2, 0xFF); mem.wr8(addr + 3, 0xFF)
        for j in range(4, 8): mem.wr8(addr + j, 0x80)
    if button is not None and not stick:
        word = (~button) & 0xFFFF
        mem.wr8(0x800E52D2, (word >> 8) & 0xFF)
        mem.wr8(0x800E52D3, word & 0xFF)
    for addr in range(0x800E5320, 0x800E54D8): mem.wr8(addr, 0)
    for address in (0x800E5478, 0x800E547C, 0x800E5480, 0x800E5484,
                    0x800E5488, 0x800E548C): mem.wr32(address, 0)
    mem.wr16(0x800E5490, 0)
    return int(button or 0) if stick else 0


def api_table(member):
    table = dict(COMMON_API)
    for name, address, argc in check_stage.HOOKS:
        table[address] = (name, argc)
    table.update(COMMON_API)
    # These entries are implemented by verified AeRam ports and therefore run
    # as original instructions on the MIPS side just as they do in C.
    for address in (0x800877F8, 0x80086FD0, 0x8008C01C, 0x8008C09C,
                    0x80015D58, 0x80031B60, 0x8009B7FC, 0x8008D064,
                    0x80015780, 0x800301B0, 0x800301E0, 0x800303C0,
                    0x80030450, 0x8002FF50, 0x80014804, 0x80030E60,
                    0x80096580, 0x800963AC, 0x8001325C):
        table.pop(address, None)
    if member != "184":
        table[0x800116C0] = ("LoadListStep", 0)
    if member == "183":
        table.update({0x800A342C: ("SoundStopAllVoices", 1)})
    elif member == "184":
        pass
    elif member == "185":
        for address in (0x8007B374, 0x80012C0C, 0x80012360):
            table.pop(address, None)
    elif member == "18A":
        # The XA wrapper and its verified callees execute as original MIPS;
        # the CD SDK calls within XaPlay remain named hooks.
        for address in (0x80015D58, 0x8008C01C, 0x8008C09C, 0x8007B374,
                        0x80012C0C, 0x80011F6C, 0x80012360, 0x80012A0C,
                        0x80086248): table.pop(address, None)
    return table


EVENT_ARITIES = {name: count for name, count in COMMON_API.values()}
EVENT_ARITIES.update({name: count for name, _address, count in check_stage.HOOKS})
EVENT_ARITIES.update({
    "SoundTest_DrawMainMenuSelection": 4, "Ovl185_DrawSelection": 5,
    "Ovl18A_DrawClearPanel": 4, "Ovl18A_DrawAnimatedPanel": 4,
    "DrawDebugGlyph": 6, "FinishDebugText": 6, "Ovl185_DrawText": 4,
    "ScaleMatrixL": 2, "SoundLoopTableGetParams": 3,
    "MonkeyClearWords": 2, "PlayerSelectGadget": 2, "HudFindGadgetIconPos": 0,
})


class Harness:
    def __init__(self, member, dll):
        self.member = member
        self.cfg = CONFIGS[member]
        self.area_map = dump_areas.area_members()
        self.coverage = set()
        self.cpus = {}
        self.baselines = {}
        self.c = CPort(member, dll)
        self.api = api_table(member)
        self.counts = {name: 0 for name in self.cfg["functions"]}
        self.failures = 0
        self.failures_by_function = {}
        reg.declare("stack_window", "801FF000..80200000")

    def cpu_for(self, scenario):
        if self.member == "185":
            # The first four scene rows point at these area members. Keep the
            # area blob in RAM consistent with LoadScene's selected row.
            area_id = (68, 71, 69, 70)[scenario % 4]
        elif self.member == "18A":
            # Scene rows share their real area member in groups of three (the
            # last two groups have two and three rows, respectively).
            area_id = (69, 69, 69, 70, 70, 70, 71, 71, 71, 72, 72,
                       73, 73, 73)[scenario % 14]
        else:
            area_id = (1, 2, 3, 4)[scenario % 4]
        member_id = self.area_map[area_id]
        if member_id not in self.cpus:
            cpu = mr.MipsRam.from_extract(area=ROOT / f"extract/members/{member_id:03X}.bin")
            attach_cached_gte_oracle(cpu)
            cpu.load_area((ROOT / f"extract/members/{self.member}.bin").read_bytes(), OVERLAY_BASES[self.member])
            cpu.coverage = self.coverage
            self.cpus[member_id] = cpu
            self.baselines[member_id] = cpu.snapshot()
        cpu = self.cpus[member_id]; cpu.coverage = self.coverage
        cpu.restore(self.baselines[member_id])
        return cpu, area_id

    def setup(self, name, scenario, mem, profile):
        profile["area_kind"] = scenario % 5
        profile["area_level"] = (scenario % 8) + 1
        profile["first_area"] = (scenario % 4) + 1
        profile["load_sequence"] = [0, -1] if scenario % 3 == 0 else [-1]
        for i in range(STACK_HI - STACK_LO): mem.wr8(STACK_LO + i, 0)
        _button_setup(mem)
        mem.wr32(MUTATION_SELECTOR, 0); mem.wr32(MUTATION_MARKER, 0)
        mem.wr32(0x800EC354, WORK + 0x1000); mem.wr32(0x800EC35C, scenario % 2)
        mem.wr32(0x800EC350, scenario); mem.wr32(0x800EC358, WORK + 0x7000)
        mem.wr32(0x800DF7E0, 0); mem.wr32(0x800DF7E4, 0)
        mem.wr32(0x800DF7F8, 0x00FFFFFF); mem.wr32(0x800DF7FC, 0)
        mem.wr16(0x800F460A, 1)
        mem.wr32(0x800BF670, 0xFFFFFFFF); mem.wr32(0x800BF674, 0xFFFFFFFF)
        mem.wr16(0x800BF676, 0xFFFF)
        for i in range(256):
            record = 0x800B2D08 + i * 8
            mem.wr16(record + 2, (i * 7) & 0x7FFF)
            mem.wr16(record + 4, ((i * 11) & 0xFFFF) - 0x8000)
        mem.wr32(0x800B35C4, 0)
        mem.wr32(0x800E3890, 0)
        if self.member == "183":
            if name == "ovl183_entry_80136000":
                profile["stick_button_sequence"] = [0, 0x0800]
                profile["stick_buttons"] = 0
            if name == "SoundTest_DrawMainMenu":
                selections = (0, 1, 2, 1, 0, 2, 2, 2)
                mem.wr32(0x80136B50, selections[scenario % len(selections)])
                masks = (0x1000, 0x4000, 0x2000, 0x8000, 0x0800, 0x0800, 0x4000, 0x0800)
                mem.wr32(0x800E5478, masks[scenario % len(masks)])
                mem.wr32(0x800E5480, 0xFFFFFFFF if masks[scenario % len(masks)] else 0)
                mem.wr32(0x800E5488, masks[scenario % len(masks)])
                if scenario % len(masks) == 2: mem.wr32(0x80136B40, 42)
                if scenario % len(masks) == 3: mem.wr32(0x80136B40, 0)
                if scenario % len(masks) == 4: mem.wr32(0x80136B50, 0)
                if scenario % len(masks) == 5: mem.wr32(0x80136B50, 2)
                if scenario % len(masks) == 5: profile["load_sequence"] = [0, -1]
                if scenario % len(masks) == 7: mem.wr32(0x80136B50, 1)
            if name == "SoundTest_DrawCommonTest":
                selection = (scenario * 23) % 172
                if scenario % 8 == 1: selection = 0
                if scenario % 8 == 2: selection = 171
                mem.wr32(0x80136B44, selection)
                masks = (0x0800, 0x1000, 0x4000, 0x0080, 0x0100, 0x0000, 0x0020, 0x0000)
                mask = masks[scenario % len(masks)]
                mem.wr32(0x800E5478, mask); mem.wr32(0x800E5480, 0xFFFFFFFF if mask else 0)
                mem.wr32(0x800E5488, mask)
            if name == "SoundTest_DrawSightTest":
                selection = 172 + (scenario * 7) % 25
                if scenario % 8 == 1: selection = 172
                if scenario % 8 == 2: selection = 196
                mem.wr32(0x80136B44, selection)
                if scenario % 8 == 3: mem.wr32(0x80136B48, 1)
                masks = (0x0800, 0x1000, 0x4000, 0x0008, 0x0080, 0x0100, 0x0008, 0x0020)
                mask = masks[scenario % len(masks)]
                mem.wr32(0x800E5478, mask); mem.wr32(0x800E5480, 0xFFFFFFFF if mask else 0)
                mem.wr32(0x800E5488, mask)
            if name == "SoundTest_Update":
                pages = (0, 1, 2, 0xFFFFFFFF, 0, 1, 2, 3)
                mem.wr32(0x80136B4C, pages[scenario % len(pages)])
        elif self.member == "184":
            mem.wr32(0x801363E0, 0x00000000)
            if name == "ovl184_entry_80136000": profile["raw_button"] = 0x0800
            if name == "Ovl184_DemoLoop":
                profile["pad_button_sequence"] = ([0x0800] if scenario % 2 == 0
                                                    else [0, 0x0800])
                profile["raw_button"] = profile["pad_button_sequence"][0]
            if name == "Ovl184_DrawDemoPrompt":
                mem.wr32(0x800E5478, 0x0800 if scenario == 0 else 0)
                mem.wr32(0x800E5480, 0x0800 if scenario == 0 else 0)
            if name == "Ovl184_LoadListLoop": profile["load_sequence"] = [0, -1] if scenario % 2 == 0 else [-1]
        elif self.member == "185":
            if name == "ovl185_entry_80146000":
                profile["pad_button_sequence"] = ([0x0860] + [0] * 64 + [0x0100]
                                                    if scenario % 2 == 0 else [0x0100])
                profile["raw_button"] = profile["pad_button_sequence"][0]
            if name == "Ovl185_Run":
                profile["pad_button_sequence"] = [0, 0x0100]
                profile["raw_button"] = 0
            if name == "Ovl185_Update":
                masks = (0x0860, 0x0100, 0x1000, 0x4000, 0, 0x0800, 0x0060, 0)
                mask = masks[scenario % len(masks)]
                mem.wr32(0x800E5478, mask); mem.wr32(0x800E5480, mask)
                if scenario % len(masks) == 3:
                    mem.wr32(0x80146774, 11)
            if name == "Ovl185_LoadScene":
                profile["load_sequence"] = [0, -1] if scenario % 2 == 0 else [-1]
                mem.wr8(0x800F5AA0, 2); mem.wr16(0x800F5AA2, 0)
                mem.wr32(0x80146774, scenario % 4)
                mem.wr16(0x800E3904, 0); mem.wr16(0x800E3906, 0)
                for offset in range(0x78, 0x78 + 16 * 0x1C, 4):
                    mem.wr32(0x800E3890 + offset, 0)
        else:
            if name == "StageClearUi_LookAt_18A":
                eye, target = WORK + 0x400, WORK + 0x410
                for i, value in enumerate((scenario * 7 - 20, scenario * 5 + 12, 240,
                                           scenario * 3 + 45, -40, 620)):
                    mem.wr16(eye + i * 2 if i < 3 else target + (i - 3) * 2, value)
                if scenario == 7:
                    mem.wr16(target, mem.rd16(eye))
                    mem.wr16(target + 4, mem.rd16(eye + 4))
                return [eye, target]
            if name == "Ovl18A_StringTableIndex":
                mem.write_bytes(WORK + 0x300, b"one\0two\0three\0four\0\0")
            if name == "ovl_18A_entry_80136208":
                # XaPlay queues eight CD commands; keep Start released until
                # CdMgrUpdate has drained them and the entry can leave cleanly.
                profile["pad_button_sequence"] = [0] * 8 + [0x0800]
                profile["raw_button"] = profile["pad_button_sequence"][0]
                profile["load_sequence"] = [0, -1] if scenario == 0 else [-1]
                mem.wr16(0x80136E30, 599 if scenario == 0 else 600)
                mem.wr32(0x800E3890, 0)
            if name in ("ovl_18A_entry_80136208", "Ovl18A_PlayXa"):
                mem.wr32(0x800E3890, 0)
                mem.wr16(0x800E3890 + 0x74, 0)
                mem.wr16(0x800E3890 + 0x76, 0)
                for offset in range(0x78, 0x78 + 16 * 0x1C, 4):
                    mem.wr32(0x800E3890 + offset, 0)
            if name == "Ovl18A_WaitInputTimeout":
                timeout = (0, 599, 600, 0x7FFF, 0x8000, 0xFFFF, 17, 99)[scenario % 8]
                mem.wr16(0x80136E30, timeout)
                if scenario == 0:
                    mem.wr32(0x800E5478, 0x0800); mem.wr32(0x800E5480, 0x0800)
            if name in ("Ovl18A_UpdateSpin", "Ovl18A_AdvanceAnim"):
                self._prepare_actor_fixture(mem, scenario)
            if name == "Ovl18A_StartClip":
                self._prepare_actor_fixture(mem, scenario)
                mem.wr32(0x80137058, scenario % 14)
                for j in range(14): mem.wr8(0x80136E4C + j * 12 + 1, j % 3)
            if name == "Ovl18A_PickScene":
                scene = scenario % 14
                mem.wr32(0x800EC350, scene)
                mem.wr8(0x800DFC81, 1 if scenario % 2 else 0)
                progress_toggle = scenario // 14
                for offset in (4, 5):
                    mem.wr8(0x800DFDD8 + offset, 5 if progress_toggle == 1 and scene < 6 else 0)
                mem.wr8(0x800DFDD8 + 8, 5 if scene in (9, 10) and progress_toggle == 0 else 0)
                mem.wr8(0x800DFDD8 + 12, 5 if scene >= 11 and progress_toggle == 0 else 0)
            if name in ("Ovl18A_SceneInit", "Ovl18A_UpdateSpin", "Ovl18A_AdvanceAnim", "Ovl18A_StartClip"):
                mem.wr32(0x800BF790, 0); mem.wr32(0x800DF798, 0x80102800)
                for j in range(64): mem.wr32(0x800BF690 + j * 4, 0)
        if "raw_button" in profile:
            _button_setup(mem, profile["raw_button"])
        if name == "Ovl18A_StringTableIndex": return [WORK + 0x300, (scenario % 7) - 1]
        if self.member == "184" and name in ("Ovl184_DemoLoop", "Ovl184_DrawDemoPrompt"): return [scenario % 7 - 3]
        return []

    def _prepare_actor_fixture(self, mem, scenario):
        actor0, actor1 = 0x80136F00, 0x80136F88
        table = WORK + 0x3000
        transform0, transform1 = WORK + 0x3400, WORK + 0x3440
        buffer = WORK + 0x3500
        for actor, transform in ((actor0, transform0), (actor1, transform1)):
            mem.wr32(actor + 0x14, transform)
            mem.wr32(actor + 0x74, table)
            mem.wr32(actor + 0x18, WORK + 0x3600)
            mem.wr16(actor + 0x5C, scenario % 8)
            mem.wr16(actor + 0x5E, 0)
            mem.wr16(actor + 0x60, 20); mem.wr16(actor + 0x62, 30); mem.wr16(actor + 0x64, -40)
            mem.wr16(actor + 0x70, 0xFFFF)
        for index, (start, end) in enumerate(((0, 4), (2, 8), (1, 6), (0, 12))):
            mem.wr16(table + index * 4, start); mem.wr16(table + index * 4 + 2, end)
        mem.wr32(0x80136F00, WORK + 0x3700); mem.wr32(0x80136F88, WORK + 0x3800)
        mem.wr32(0x800B4D64, buffer); mem.wr32(0x800F45D0, 2)
        mem.wr32(0x800EC350, scenario * 17)
        mem.wr32(0x8013706C, scenario % 8)
        mem.wr32(0x80137074, scenario % 8)
        mem.wr32(0x80137010 + 0x50, 16)

    def _api_hooks(self, model):
        hooks = {}
        for address, (name, arity) in self.api.items():
            def invoke(cpu, regs, label=name, n=arity, current=model):
                args = list(regs[4:8])[:n]
                args.extend(cpu.read32(regs[29] + 16 + i * 4) for i in range(max(0, n - 4)))
                return current.call(label, args)
            hooks[address] = invoke
        return hooks

    def _overlay_patches(self, model):
        patches = debug_patches(model)
        member = self.member
        if member == "183":
            def draw_main(cpu, regs):
                cursor = cpu.read32(0x800EC354)
                model.call("SoundTest_DrawMainMenuSelection", [cursor, cpu.read32(0x800EC35C),
                            cpu.read32(0x80136B50), cpu.read32(0x80136B40)])
                regs[5] = 0x800EC350
                regs[6] = 0x800E5320
                regs[16] = 0x80130000
                regs[17] = 0x80130000
                regs[18] = 0x80136B54
                return 0x801364E4
            patches[0x80136330] = draw_main
        elif member == "185":
            def draw_selection(cpu, regs):
                cursor = cpu.read32(0x800EC354)
                model.call("Ovl185_DrawSelection", [cursor, cpu.read32(0x800EC35C),
                            cpu.read32(0x80146774), 0x800F03CC, 0x4078])
                regs[4] = 0x800EC350
                regs[3] = u32(cursor + 36)
                return 0x801464E4
            patches[0x80146358] = draw_selection
        elif member == "18A":
            def clear_panel(cpu, regs):
                cursor = cpu.read32(0x800EC354)
                model.call("Ovl18A_DrawClearPanel", [cursor, cpu.read32(0x800EC35C),
                            cpu.read32(0x80137058), cpu.read32(0x800EC358)])
                regs[3] = u32(cursor + 24)
                regs[17] = 0x800EC350
                return 0x8013641C
            def animated_panel(cpu, regs):
                cursor = cpu.read32(0x800EC354)
                model.call("Ovl18A_DrawAnimatedPanel", [cursor, cpu.read32(0x800EC35C),
                            cpu.read32(0x80137058), cpu.read32(0x80137058 + 0x48)])
                regs[4] = u32(cursor + 16)
                regs[17] = 0x800EC350
                return 0x80136488
            patches[0x801363B4] = clear_panel
            patches[0x8013642C] = animated_panel
        return patches

    def _prime_18a(self, cpu, model, scenario):
        """Run SceneInit on both sides before animation-draw function cases."""
        name = "Ovl18A_SceneInit"
        cpu.coverage = self.coverage
        mips_hooks = self._api_hooks(model)
        before = cpu.snapshot()
        try:
            cpu.call(0x80136780, (), hooks=mips_hooks, patches=debug_patches(model),
                     limit=MIPS_INSTRUCTION_BUDGET, sp=mr.DEFAULT_SP)
        except Exception as exc: raise RuntimeError(f"SceneInit fixture MIPS failed: {exc}")
        model_c = Model(model.profile, self.c.mem)
        self.c.active_model = model_c
        self.c.sync(before)
        self.c.call(name, [])
        if self.c.dll.ae_ram_failed(self.c.ram):
            raise RuntimeError(self.c.dll.ae_ram_error(self.c.ram).decode(errors="replace"))
        got = bytes(self.c.main[:STACK_LO - 0x80000000]); want = bytes(cpu.ram[:STACK_LO - 0x80000000])
        if got != want:
            off = next(i for i, (a, b) in enumerate(zip(got, want)) if a != b)
            raise RuntimeError(f"SceneInit fixture differs at {0x80000000 + off:08X}")
        if model_c.events != model.events:
            raise RuntimeError("SceneInit fixture hook sequence differs")
        model.events.clear()
        model_c.events.clear()
        self.c.sync(cpu.snapshot())
        return model_c

    def run(self, name, scenario, selector=0):
        cpu, area_id = self.cpu_for(scenario)
        mem = MipsMem(cpu)
        profile = {"load_sequence": [-1], "area_id": area_id}
        args = self.setup(name, scenario, mem, profile)
        mem.wr32(MUTATION_SELECTOR, selector); mem.wr32(MUTATION_MARKER, 0)
        before = cpu.snapshot()
        model_mips = Model(profile, mem)
        self.c.sync(before)
        model_c = Model(profile, self.c.mem)
        self.c.active_model = model_c
        hooks = self._api_hooks(model_mips)
        patches = self._overlay_patches(model_mips)
        address, _size, _symbol, ret_kind, _kinds = self.cfg["functions"][name]
        try:
            if self.member == "18A" and name in ("Ovl18A_UpdateSpin", "Ovl18A_AdvanceAnim", "Ovl18A_StartClip"):
                # Establish actor/model structures from the same real area on both sides.
                self._prime_18a(cpu, model_mips, scenario)
                mem = MipsMem(cpu)
                if name in ("Ovl18A_UpdateSpin", "Ovl18A_AdvanceAnim"):
                    actor = 0x80136F00 if name == "Ovl18A_UpdateSpin" else 0x80136F88
                    table = WORK + 0x3000
                    mem.wr32(actor + 0x74, table)
                    frame = (scenario * 3) & 0xFFFF
                    mem.wr16(actor + 0x5C, frame)
                    mem.wr16(actor + 0x5E, 0)
                    if name == "Ovl18A_UpdateSpin":
                        mem.wr16(actor + 0x70, 0)
                    else:
                        mem.wr16(actor + 0x70, 0xFFFF if scenario % 3 == 0 else 1)
                    mem.wr32(0x80137010 + 0x5C, 9 if scenario % 4 else 0)
                    mem.wr32(0x80137010 + 0x54, 3)
                else:
                    mem.wr32(0x80137058, scenario % 14)
                    for j in range(14): mem.wr8(0x80136E4C + j * 12 + 1, j % 3)
                before = cpu.snapshot(); self.c.sync(before)
                model_c = Model(profile, self.c.mem); self.c.active_model = model_c
            expected = cpu.call(address, args, hooks=hooks, patches=patches,
                                limit=MIPS_INSTRUCTION_BUDGET, sp=mr.DEFAULT_SP)
        except Exception as exc:
            self._fail(name, scenario, f"MIPS execution failed: {exc}")
            return False
        self.c.active_model = model_c
        self.c.hooks.stack_pointer = mr.DEFAULT_SP
        self.c.stage.stack_pointer = mr.DEFAULT_SP
        self.c.misc.stack_pointer = mr.DEFAULT_SP
        if self.member == "185" and name == "ovl185_entry_80146000":
            self.c.hooks.stack_pointer = mr.DEFAULT_SP - 0x20
            self.c.stage.stack_pointer = mr.DEFAULT_SP - 0x48
            self.c.misc.stack_pointer = mr.DEFAULT_SP - 0x48
        if self.member == "185" and name == "Ovl185_LoadScene":
            # Ovl185_LoadScene's MIPS frame is 0x28 bytes; nested stage and
            # area-feature helpers address their caller-backed locals from it.
            nested_sp = mr.DEFAULT_SP - 0x28
            self.c.stage.stack_pointer = nested_sp
            self.c.misc.stack_pointer = nested_sp
        actual = self.c.call(name, args)
        self.counts[name] += 1
        if ret_kind != "v" and u32(actual) != expected[2]:
            self._fail(name, scenario, f"return C={u32(actual):08X} MIPS={expected[2]:08X}")
            return False
        if self.c.dll.ae_ram_failed(self.c.ram):
            err = self.c.dll.ae_ram_error(self.c.ram).decode(errors="replace")
            self._fail(name, scenario, f"C RAM error: {err}"); return False
        if model_c.events != model_mips.events:
            idx = next((i for i, pair in enumerate(zip(model_c.events, model_mips.events)) if pair[0] != pair[1]),
                       min(len(model_c.events), len(model_mips.events)))
            self._fail(name, scenario, f"hook sequence differs at {idx}: C={model_c.events!r} MIPS={model_mips.events!r}")
            return False
        got = bytes(self.c.main[:STACK_LO - 0x80000000]); want = bytes(cpu.ram[:STACK_LO - 0x80000000])
        if got != want:
            off = next(i for i, (a, b) in enumerate(zip(got, want)) if a != b)
            self._fail(name, scenario, f"RAM differs at {0x80000000 + off:08X}: C={got[off:off+8].hex()} MIPS={want[off:off+8].hex()}")
            return False
        if bytes(self.c.scratch) != bytes(cpu.scratch):
            self._fail(name, scenario, "scratchpad differs"); return False
        return True

    def _fail(self, name, scenario, message):
        self.failures += 1
        self.failures_by_function[name] = self.failures_by_function.get(name, 0) + 1
        print(f"FAIL {name}[{scenario}]: {message}", flush=True)

    def all(self, cases=8, names=None):
        selected = list(names) if names else list(self.cfg["functions"])
        for name in selected:
            self.counts[name] = 0
            count = 28 if self.member == "18A" and name == "Ovl18A_PickScene" else cases
            for scenario in range(count):
                self.run(name, scenario)
            print(f"{name}: {self.counts[name]} cases", flush=True)
        return self.failures == 0

    def finish(self, default_run=False, show_coverage=False, names=None):
        selected = list(names) if names else list(self.cfg["functions"])
        items = [(name, self.cfg["functions"][name][0], self.cfg["functions"][name][1], True)
                 for name in selected]
        summary = reg.coverage_summary(items, self.coverage)
        if show_coverage: reg.print_coverage(summary)
        gaps = {name: row for name, row in summary.items()
                if row["uncovered_count"] or row["stale_declared"]}
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps,
                              extra={"overlay": f"{self.member}.bin",
                                     "areas": [1, 2, 3, 4]}, function_names=selected)
        for name, row in gaps.items():
            print(f"COVERAGE GAP {name}: {row['uncovered']}; stale={row['stale_declared']}", flush=True)
        return not gaps and self.failures == 0


CONFIGS = {
    "183": {"no_hooks": set(), "functions": {
        "ovl183_entry_80136000": (0x80136000, 0x1BC, "ae_ovl183_entry_80136000", "s", ""),
        "SoundTest_Update": (0x801361BC, 0x88, "ae_SoundTest_Update", "s", ""),
        "SoundTest_DrawMainMenu": (0x80136244, 0x454, "ae_SoundTest_DrawMainMenu", "s", ""),
        "SoundTest_DrawCommonTest": (0x80136698, 0x1B4, "ae_SoundTest_DrawCommonTest", "s", ""),
        "SoundTest_DrawSightTest": (0x8013684C, 0x228, "ae_SoundTest_DrawSightTest", "s", ""),
    }},
    "184": {"no_hooks": set(), "functions": {
        "ovl184_entry_80136000": (0x80136000, 0x38, "ae_ovl184_entry_80136000", "s", ""),
        "Ovl184_LoadListLoop": (0x80136038, 0x198, "ae_Ovl184_LoadListLoop", "s", ""),
        "Ovl184_DemoLoop": (0x801361D0, 0x180, "ae_Ovl184_DemoLoop", "s", "s"),
        "Ovl184_DrawDemoPrompt": (0x80136350, 0x84, "ae_Ovl184_DrawDemoPrompt", "s", "s"),
    }},
    "185": {"no_hooks": set(), "functions": {
        "ovl185_entry_80146000": (0x80146000, 0x7C, "ae_ovl185_entry_80146000", "s", ""),
        "Ovl185_Run": (0x8014607C, 0x1B4, "ae_Ovl185_Run", "s", ""),
        "Ovl185_Update": (0x80146230, 0x2D0, "ae_Ovl185_Update", "s", ""),
        "Ovl185_LoadScene": (0x80146500, 0x16C, "ae_Ovl185_LoadScene", "s", ""),
    }},
    "18A": {"no_hooks": {"Ovl18A_StringTableIndex", "Ovl18A_WaitInputTimeout",
                           "Ovl18A_StartClip", "Ovl18A_PickScene"}, "functions": {
        "StageClearUi_LookAt_18A": (0x80136000, 0x1D8, "ae_StageClearUi_LookAt_18A", "v", "uu"),
        "Ovl18A_StringTableIndex": (0x801361D8, 0x30, "ae_Ovl18A_StringTableIndex", "u", "us"),
        "ovl_18A_entry_80136208": (0x80136208, 0x51C, "ae_ovl18A_entry_80136208", "s", ""),
        "Ovl18A_WaitInputTimeout": (0x80136724, 0x5C, "ae_Ovl18A_WaitInputTimeout", "s", ""),
        "Ovl18A_SceneInit": (0x80136780, 0x14C, "ae_Ovl18A_SceneInit", "s", ""),
        "Ovl18A_UpdateSpin": (0x801368CC, 0x1D4, "ae_Ovl18A_UpdateSpin", "v", ""),
        "Ovl18A_AdvanceAnim": (0x80136AA0, 0x164, "ae_Ovl18A_AdvanceAnim", "v", ""),
        "Ovl18A_StartClip": (0x80136C04, 0x68, "ae_Ovl18A_StartClip", "v", ""),
        "Ovl18A_PlayXa": (0x80136C6C, 0x28, "ae_Ovl18A_PlayXa", "s", ""),
        "Ovl18A_PickScene": (0x80136C94, 0x154, "ae_Ovl18A_PickScene", "v", ""),
    }},
}


def _check_member_names():
    for member, cfg in CONFIGS.items():
        cfg["base"] = OVERLAY_BASES[member]
        cfg["size"] = OVERLAY_SIZES[member]


_check_member_names()


def instrument_mutations(member, source_text):
    mutated = source_text
    funcs = list(CONFIGS[member]["functions"].values())
    for i, spec in enumerate(funcs, 1):
        symbol = spec[2]
        pattern = re.compile(r"(\b(?:void|int32_t|uint32_t)\s+" + re.escape(symbol) + r"\s*\([^)]*\)\s*\{)", re.S)
        probes = "\n".join(
            f"    if (ae_rd32(r, 0x{MUTATION_SELECTOR:08X}u) == {i * 2 - variant}u) "
            f"ae_wr32(r, 0x{MUTATION_MARKER:08X}u, 0x{int(member,16):04X}0000u | {i * 2 - variant}u);"
            for variant in (1, 2))
        mutated, count = pattern.subn(r"\1\n" + probes, mutated, count=1)
        if count != 1: raise SystemExit(f"could not instrument {symbol}")
    return mutated


def run_mutations(member):
    source = ROOT / OVERLAY_SOURCES[member]
    mutant = ROOT / "extract/mips" / f"ae_ovl{member}_mutant.c"
    mutant.parent.mkdir(parents=True, exist_ok=True)
    mutant.write_text(instrument_mutations(member, source.read_text(encoding="utf-8")), encoding="utf-8")
    try:
        harness = Harness(member, build(member, mutant))
        caught = 0
        for index, name in enumerate(CONFIGS[member]["functions"], 1):
            for variant in (1, 2):
                before = harness.failures
                ok = harness.run(name, variant - 1, selector=index * 2 - variant)
                if not ok and harness.failures > before:
                    caught += 1
                else:
                    print(f"MUTATION SURVIVED {name} selector={index * 2 - variant}", flush=True)
        total = len(CONFIGS[member]["functions"]) * 2
        print(f"mutations caught: {caught}/{total}", flush=True)
        return 0 if caught == total else 1
    finally:
        mutant.unlink(missing_ok=True)


def main(member, argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=int, default=8)
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--function", action="append", choices=CONFIGS[member]["functions"])
    args = parser.parse_args(argv)
    if args.mutate: return run_mutations(member)
    harness = Harness(member, build(member))
    passed = harness.all(args.cases, args.function)
    complete = harness.finish(default_run=args.cases == 8 and not args.function,
                              show_coverage=args.coverage, names=args.function)
    print(f"differential: {sum(harness.counts.values())} cases; {'PASS' if passed and complete else 'FAIL'}", flush=True)
    return 0 if passed and complete else 1
