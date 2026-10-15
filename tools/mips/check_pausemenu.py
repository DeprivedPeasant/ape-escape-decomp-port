"""Differential checks for the pause-menu RAM ports."""
import argparse
import ctypes as C
import random
import re
import shutil
import subprocess
import sys
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mips_ram as mr  # noqa: E402
import exclusion_registry as reg  # noqa: E402

OUT = ROOT / "extract/mips"
DEFAULT_SEED = 0x504D
M32 = 0xFFFFFFFF
AREAS = ("000.bin", "002.bin", "004.bin")

# address, actual instruction span, C function, return kind, argument kinds
FUNCS = {
    "SoundLoopsResume": (0x800729CC, 72, "ae_SoundLoopsResume", "v", ""),
    "PlayerStopGadgetSounds": (0x8007A180, 388, "ae_PlayerStopGadgetSounds", "v", "s"),
    "DrawScaledModelAt": (0x8007CAE0, 556, "ae_DrawScaledModelAt", "v", "usi"),
    "PauseMenuDrawButton": (0x8007F5E4, 588, "ae_PauseMenuDrawButton", "u", "ssisi"),
    "ScreenWipeReset": (0x8007F830, 24, "ae_ScreenWipeReset", "v", ""),
    "ScreenSnapshotRestore": (0x8007F848, 172, "ae_ScreenSnapshotRestore", "v", ""),
    "ScreenWipeBlurBlock": (0x8007F8F4, 220, "ae_ScreenWipeBlurBlock", "v", "u"),
    "ScreenWipeStep": (0x8007F9D0, 124, "ae_ScreenWipeStep", "v", ""),
    # The inventory says 3980 bytes, but this function's return is at 80081144.
    "PauseMenuRun": (0x8007FA4C, 5880, "ae_PauseMenuRun", "s", ""),
    "PauseMenuSetLayout": (0x80081B9C, 440, "ae_PauseMenuSetLayout", "v", "u"),
    "PauseMenuDrawButtons": (0x80081D54, 452, "ae_PauseMenuDrawButtons", "v", ""),
    "HudDrawWindowFrame": (0x80081F18, 280, "ae_HudDrawWindowFrame", "v", "ssss"),
    "PauseItemEnabled": (0x80082030, 236, "ae_PauseItemEnabled", "s", "s"),
    "PauseMenuUpdate": (0x800821D4, 92, "ae_PauseMenuUpdate", "s", ""),
    "SetDAT_800BEF80": (0x8008227C, 12, "ae_SetDAT_800BEF80", "v", "u"),
    "SoundLoopsSetPan": (0x80086788, 228, "ae_SoundLoopsSetPan", "s", "s"),
    "MessageBoxDrawString": (0x8008DDC8, 84, "ae_MessageBoxDrawString", "v", "u"),
    "TextWidthUntilGlyph": (0x8008DE68, 108, "ae_TextWidthUntilGlyph", "s", "uu"),
    "RadarDrawModel": (0x8009A574, 648, "ae_RadarDrawModel", "s", "uuuu"),
}
NO_HOOKS = {"ScreenWipeReset", "PauseMenuSetLayout", "PauseItemEnabled", "SetDAT_800BEF80"}
CHECKED = tuple(FUNCS)
HOOKED_CHECKED = tuple(name for name in FUNCS if name not in NO_HOOKS)
HOOK_COUNTS = {
    "DrawSync": 1, "VSync": 1, "PutDispEnv": 1, "PutDrawEnv": 1,
    "StoreImage": 2, "ClearOTagR": 2, "DrawOTag": 1,
    "LoadImage": 2, "SoundLoopStop": 2, "SfxVoiceSetPanVol": 3, "PauseMenuOverlay1B": 1,
    "PlaySfx": 1, "GadgetSlotRemap": 2, "SetPriority": 1,
    "HeapAlloc": 1, "HeapFree": 1, "TextSetCursor": 2, "Printf": 1,
    "HudTextWidth": 1, "DrawTextSprites": 4, "DrawMenuArrows": 3,
    "HudPanelA_Start": 1, "HudPanelB_Start": 1, "HudCounterC_Start": 1,
    "HudDrawIcon16": 3, "HudDrawTime": 3, "HudDrawTimerGauge": 1,
    "HudPanelA_DrawIcon": 2, "HudDrawHealth": 1, "HudPanelB_DrawSprite": 2,
    "HudDrawDigit": 3, "HudDrawNumber": 3, "HudCounterC_Draw": 3,
    "SetRotMatrix": 1, "SetTransMatrix": 1, "RotTrans": 3, "RotTransSV": 3,
    "RotMatrixZ": 2,
    "ModelTransformRigid": 3, "ModelDraw": 3, "ModelDrawScreenSpace": 4,
    "IsTimeAttack": 0, "RendererHudDrawWindowFrame": 4,
    "TextNextGlyph": 2, "FontDrawGlyph": 2,
    "RotMatrixX": 2, "RotMatrixY": 2,
}
HOOK_ADDRS = {}
_HOOK_HEADER = (ROOT / "src/ui/ae_pausemenu.h").read_text(encoding="utf-8")
for _field, _address in re.findall(r"AePauseMenuHookFn\s+(\w+);\s*/\*\s*([0-9A-Fa-f]{8})", _HOOK_HEADER):
    HOOK_ADDRS[_field] = int(_address, 16)
HOOK_ADDRS.update({"TextNextGlyph": 0x8008DD68, "FontDrawGlyph": 0x8008D9D0,
                   "ModelDrawScreenSpace": 0x800207B4,
                   "RendererHudDrawWindowFrame": 0x80081F18})
HOOK_ADDRS["RendererPauseMenuDrawGadgetScreen"] = 0x8008052C
CALL_TARGETS = {}
with (ROOT / "notes/inventory/calls.csv").open(newline="", encoding="utf-8") as _calls_file:
    for _row in __import__("csv").reader(_calls_file):
        if len(_row) >= 2 and re.fullmatch(r"[0-9A-Fa-f]{8}", _row[0]) and re.fullmatch(r"[0-9A-Fa-f]{8}", _row[1]):
            CALL_TARGETS.setdefault(int(_row[0], 16), set()).add(int(_row[1], 16))
FUNCTION_HOOKS = {
    "PlayerStopGadgetSounds": ("SoundLoopStop",),
    "DrawScaledModelAt": ("SetRotMatrix", "SetTransMatrix", "RotTrans", "ModelTransformRigid", "ModelDraw"),
    "PauseMenuDrawButton": ("ModelDrawScreenSpace",),
    "ScreenSnapshotRestore": ("LoadImage",),
    "PauseMenuDrawButtons": ("IsTimeAttack", "RendererPauseMenuDrawButtons"),
    "HudDrawWindowFrame": ("RendererHudDrawWindowFrame",),
    "SoundLoopsSetPan": ("SfxVoiceSetPanVol",),
    "MessageBoxDrawString": ("TextNextGlyph", "FontDrawGlyph"),
    "TextWidthUntilGlyph": ("TextNextGlyph",),
    "RadarDrawModel": ("ModelTransformRigid", "ModelDraw", "SetRotMatrix", "SetTransMatrix",
                        "RotTransSV", "RotMatrixX", "RotMatrixY"),
}
FUNCTION_HOOKS["SoundLoopsResume"] = ("SfxVoiceSetPanVol", "PauseMenuOverlay1B")
FUNCTION_HOOKS["ScreenWipeBlurBlock"] = ("RendererIrgbToIr123", "RendererPackIrgb")
FUNCTION_HOOKS["ScreenWipeStep"] = ("RendererIrgbToIr123", "RendererPackIrgb")
FUNCTION_HOOKS["PauseMenuUpdate"] = ("WaitFrameFlip", "SoundLoopStop", "SfxVoiceSetPanVol",
                                      "PauseMenuOverlay1B", "RendererIrgbToIr123", "RendererPackIrgb")
FUNCTION_HOOKS["PauseMenuRun"] = (
    "DrawSync", "VSync", "PutDispEnv", "PutDrawEnv", "StoreImage", "HeapAlloc", "HeapFree",
    "ColorFadeReset", "PadDirectionCode", "TextSetCursor", "PlaySfx", "PauseMenuInitVolumes",
    "HudTextWidth", "DrawTextSprites", "DrawMenuArrows", "IsTimeAttack", "HudPanelA_Start",
    "HudPanelB_Start", "HudCounterC_Start", "HudDrawGadgetSlots", "GadgetSlotRemap",
    "HudDrawGadgetSlotsFrame", "SetPriority", "CutsceneModeOffCaller", "CutsceneModeOn",
    "PauseDrawCounter", "PauseDrawCaptureGrid", "PauseMenuDrawSubScreen", "HudDrawIcon16",
    "HudDrawTime", "GetDAT_800B01C2", "HudDrawTimerGauge", "HudPanelA_DrawIcon", "HudDrawHealth",
    "HudPanelB_DrawSprite", "HudDrawDigit", "HudDrawNumber", "HudCounterC_Draw",
    "HudDrawGadgetIcons", "CdMgrUpdate", "Printf", "SoundLoopStop", "SfxVoiceSetPanVol",
    "RotMatrixZ", "RotMatrixY", "RotMatrixX", "SetRotMatrix", "SetTransMatrix", "RotTrans",
    "ModelTransformRigid", "ModelDraw", "ModelEmitPrimsScratchColors", "GadgetDrawSlot0Attachment",
    "RendererGetProjectionDistance", "RendererSetProjectionDistance", "RendererHudDrawWindowFrame",
    "TextNextGlyph", "FontDrawGlyph", "RendererIrgbToIr123", "RendererPackIrgb")
HOOK_COUNTS.update({"FontDrawGlyph": 3, "RendererPauseMenuDrawButtons": 4,
                    "RendererIrgbToIr123": 2, "RendererPackIrgb": 4,
                    "RendererPauseMenuDrawGadgetScreen": 4})
COMPARE_C_ONLY_HOOKS = {"RendererIrgbToIr123", "RendererPackIrgb", "RendererPauseMenuDrawButtons",
                        "RendererGetProjectionDistance", "RendererSetProjectionDistance"}
_HOOKS_BY_ADDR = {}
for _name, _addr in HOOK_ADDRS.items():
    _HOOKS_BY_ADDR.setdefault(_addr, []).append(_name)
for _name, (_addr, *_rest) in FUNCS.items():
    _derived = []
    for _target in CALL_TARGETS.get(_addr, set()):
        _derived.extend(_HOOKS_BY_ADDR.get(_target, ()))
    FUNCTION_HOOKS[_name] = tuple(dict.fromkeys((*FUNCTION_HOOKS.get(_name, ()), *_derived)))
STACK_LO = 0x801FF000


def make_hooks_type():
    header = (ROOT / "src/ui/ae_pausemenu.h").read_text(encoding="utf-8")
    body = header.split("typedef struct AePauseMenuHooks", 1)[1].split("} AePauseMenuHooks;", 1)[0]
    callback = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                           C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
    fields = [("user", C.c_void_p), ("stack_pointer", C.c_uint32),
              ("caller_s5", C.c_uint32)]
    fields += [(name, callback) for name in re.findall(r"AePauseMenuHookFn\s+(\w+);", body)]
    fields += [(name, C.c_void_p) for name in
               ("pad", "player2", "player_act", "spawn", "bodies", "sound_hw")]
    return type("PauseMenuHooks", (C.Structure,), {"_fields_": fields}), callback


def build_dll(source=None, tag="pausemenu"):
    # Link the verified dependency closure, including CollisionUpdate's body
    # movement ports. The former hand-maintained list omitted merged callees.
    import check_game as game
    return game.build_dll({"src/ui/ae_pausemenu.c": source} if source else None, tag)


class CPort:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        self.lib.ae_ram_create.restype = C.c_void_p
        self.lib.ae_ptr.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32]
        self.lib.ae_ptr.restype = C.c_void_p
        self.lib.ae_ram_clear_error.argtypes = [C.c_void_p]
        self.lib.ae_ram_failed.argtypes = [C.c_void_p]
        self.lib.ae_ram_failed.restype = C.c_int32
        self.lib.ae_ram_error.argtypes = [C.c_void_p]
        self.lib.ae_ram_error.restype = C.c_char_p
        self.ram = self.lib.ae_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(
            self.lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scr = (C.c_uint8 * 0x400).from_address(
            self.lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.lib.ae_rd16.argtypes = [C.c_void_p, C.c_uint32]
        self.lib.ae_rd16.restype = C.c_uint16
        self.lib.ae_wr16.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32]
        self.lib.ae_wr32.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32]
        self.lib.ae_wr8.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32]
        self.Hooks, self.HookFn = make_hooks_type()
        self.hook_log = []
        self._keep = []
        self.PadHooks = type("PadHooks", (C.Structure,), {"_fields_": [
            ("user", C.c_void_p),
            ("pad_get_state", C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_int32)),
            ("pad_info_mode", C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_int32, C.c_int32, C.c_int32)),
            ("pad_setup_dualshock", C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32)),
            ("pad_demo_step", C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p)),
            ("stage_request", C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32, C.c_int32, C.c_int32)),
            ("stick_to_buttons", C.CFUNCTYPE(C.c_uint32, C.c_void_p, C.c_void_p)),
            ("pad_rumble_reset", C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32)),
        ]})
        for name, (_, _, c_name, kind, args) in FUNCS.items():
            fn = getattr(self.lib, c_name)
            fn.argtypes = [C.c_void_p]
            if name not in NO_HOOKS:
                fn.argtypes.append(C.POINTER(self.Hooks))
            for arg in args:
                fn.argtypes.append(C.c_int32 if arg in "si" else C.c_uint32)
            fn.restype = {"v": None, "s": C.c_int32, "u": C.c_uint32}[kind]

    def run(self, name, args, model=None):
        model = model or TestModel(self)
        self.lib.ae_ram_clear_error(self.ram)
        hook_struct = self.Hooks()
        hook_struct.stack_pointer = mr.DEFAULT_SP
        hook_struct.caller_s5 = model.caller_s5
        self.hook_log = []
        self._keep = []
        for hook_name in FUNCTION_HOOKS.get(name, ()):
            def make_callback(n):
                def callback(user, ram, a0, a1, a2, a3):
                    return model.call_c(n, ram, (a0, a1, a2, a3))
                return callback
            callback = self.HookFn(make_callback(hook_name))
            self._keep.append(callback)
            setattr(hook_struct, hook_name, callback)
        if name in ("ScreenWipeBlurBlock", "ScreenWipeStep", "PauseMenuRun", "PauseMenuUpdate"):
            for hook_name in ("RendererIrgbToIr123", "RendererPackIrgb"):
                def make_callback(n):
                    def callback(user, ram, a0, a1, a2, a3):
                        return model.call_c(n, ram, (a0, a1, a2, a3))
                    return callback
                callback = self.HookFn(make_callback(hook_name))
                self._keep.append(callback)
                setattr(hook_struct, hook_name, callback)
        for hook_name in model.extra_c_hooks(name):
            def make_callback(n):
                def callback(user, ram, a0, a1, a2, a3):
                    return model.call_c(n, ram, (a0, a1, a2, a3))
                return callback
            callback = self.HookFn(make_callback(hook_name))
            self._keep.append(callback)
            setattr(hook_struct, hook_name, callback)

        if name in ("PauseMenuRun", "PauseMenuUpdate"):
            pad = self.PadHooks()
            for field in ("pad_get_state", "pad_info_mode", "pad_setup_dualshock", "pad_demo_step",
                          "stage_request", "stick_to_buttons", "pad_rumble_reset"):
                proto = dict(self.PadHooks._fields_)[field]
                def make_pad_callback(n, ftype):
                    def callback(*args):
                        return model.call_c("Pad:" + n, self.ram, args[2:] if len(args) > 2 else ())
                    return ftype(callback)
                callback = make_pad_callback(field, proto)
                self._keep.append(callback)
                setattr(pad, field, callback)
            self._keep.append(pad)
            hook_struct.pad = C.cast(C.pointer(pad), C.c_void_p)
        else:
            pad = None

        if name not in NO_HOOKS:
            value = getattr(self.lib, FUNCS[name][2])(self.ram, C.byref(hook_struct), *args)
        else:
            value = getattr(self.lib, FUNCS[name][2])(self.ram, *args)
        if self.lib.ae_ram_failed(self.ram):
            raise RuntimeError(self.lib.ae_ram_error(self.ram).decode("utf-8", "replace"))
        return 0 if value is None else value & M32


class TestModel:
    """The same deterministic native-boundary model runs beside MIPS and the C port."""

    def __init__(self, cport, time_attack=0, stick_buttons=0,
                 stick_sequence=None, subscreen_result=0, timer_gauge=0,
                 caller_s5=0):
        self.cport = cport
        self.time_attack = time_attack
        self.stick_buttons = stick_buttons
        self.stick_sequence = list(stick_sequence or ())
        self.stick_index = 0
        self.subscreen_result = subscreen_result
        self.subscreen_index = 0
        self.timer_gauge = timer_gauge
        self.caller_s5 = caller_s5
        self.log = []
        self.alloc_count = 0

    def extra_c_hooks(self, name):
        if name in ("PauseMenuRun", "PauseMenuUpdate"):
            body = _HOOK_HEADER.split("typedef struct AePauseMenuHooks", 1)[1].split("} AePauseMenuHooks;", 1)[0]
            return tuple(re.findall(r"AePauseMenuHookFn\s+(\w+);", body))
        return ()

    def _write(self, ram, address, size, value):
        if size == 2:
            self.cport.lib.ae_wr16(ram, address, value)
        elif size == 1:
            self.cport.lib.ae_wr8(ram, address, value)
        else:
            self.cport.lib.ae_wr32(ram, address, value)

    def _record(self, name, vals):
        if name not in COMPARE_C_ONLY_HOOKS:
            self.log.append((name, *tuple(v & M32 for v in vals[:HOOK_COUNTS.get(name, 0)])))

    def call_c(self, name, ram, regs):
        vals = tuple(int(v) & M32 for v in regs)
        if name.startswith("Pad:"):
            return self._pad_c(name[4:], vals)
        self._record(name, vals)
        if name == "HeapAlloc":
            address = (0x801E0000, 0x801E9000)[min(self.alloc_count, 1)]
            self.alloc_count += 1
            return address
        if name == "IsTimeAttack":
            return self.time_attack
        if name == "GetDAT_800B01C2":
            return self.timer_gauge
        if name == "PauseMenuDrawSubScreen":
            if isinstance(self.subscreen_result, (list, tuple)):
                index = min(self.subscreen_index, len(self.subscreen_result) - 1)
                self.subscreen_index += 1
                return int(self.subscreen_result[index])
            return int(self.subscreen_result)
        if name == "RendererGetProjectionDistance":
            return 0
        if name == "RendererIrgbToIr123":
            color, out = vals[:2]
            self._write(ram, out, 4, (color & 0x1F) << 7)
            self._write(ram, out + 4, 4, ((color >> 5) & 0x1F) << 7)
            self._write(ram, out + 8, 4, ((color >> 10) & 0x1F) << 7)
        elif name == "RendererPackIrgb":
            red, green, blue, destination = vals
            red = red if red < 0x80000000 else red - 0x100000000
            green = green if green < 0x80000000 else green - 0x100000000
            blue = blue if blue < 0x80000000 else blue - 0x100000000
            r = max(0, min(31, red >> 7))
            g = max(0, min(31, green >> 7))
            b = max(0, min(31, blue >> 7))
            self._write(ram, destination, 2, r | (g << 5) | (b << 10))
        elif name == "RendererPauseMenuDrawButtons":
            _, enabled_mask, _, primitive = vals
            draw_buffer = self.cport.lib.ae_rd32(ram, 0x800EC35C)
            ordering_table = 0x800F03D0 + draw_buffer * 16504
            for item in range(4):
                record = vals[0] + item * 12
                packet = primitive + item * 20
                old_tag = self._lwl_c(ram, ordering_table + 2, primitive)
                self._swl_c(ram, ordering_table + 2, (packet << 8) & M32)
                self._swl_c(ram, packet + 2, old_tag)
                self._write(ram, packet + 3, 1, 4)
                color = 0x64808080 if enabled_mask & (1 << item) else 0x64202020
                self._write(ram, packet + 4, 4, color)
                width_height = self.cport.lib.ae_rd32(ram, record + 4)
                self._write(ram, packet + 8, 4, width_height)
                texture = self.cport.lib.ae_rd32(ram, record + 8)
                self._write(ram, packet + 12, 4, texture)
                self._write(ram, packet + 16, 4, 0x00100080)
            terminal = primitive + 80
            old_tag = self._lwl_c(ram, ordering_table + 2, terminal)
            self._swl_c(ram, ordering_table + 2, (terminal << 8) & M32)
            self._swl_c(ram, terminal + 2, old_tag)
            self._write(ram, terminal + 3, 1, 1)
            self._write(ram, terminal + 4, 4, 0xE100021A)
        elif name == "TextNextGlyph":
            text, out = vals[:2]
            glyph = self.cport.lib.ae_rd16(ram, text) if text & 1 == 0 else self.cport.lib.ae_rd8(ram, text)
            self._write(ram, out, 2, glyph)
            return (text + 1) & M32
        return 0

    def _lwl_c(self, ram, address, old):
        shift = (address & 3) * 8
        word = self.cport.lib.ae_rd32(ram, address & ~3)
        keep = 0 if shift == 24 else (1 << (24 - shift)) - 1
        return (old & keep) | ((word << (24 - shift)) & M32)

    def _swl_c(self, ram, address, value):
        offset = address & 3
        aligned = address & ~3
        for byte in range(offset + 1):
            shift = (3 - offset + byte) * 8
            self._write(ram, aligned + byte, 1, value >> shift)

    def _pad_c(self, field, vals):
        self._record("Pad:" + field, vals)
        if field == "pad_get_state": return 6
        if field == "pad_info_mode": return 0
        if field == "pad_demo_step": return 0
        if field == "stick_to_buttons":
            if self.stick_sequence:
                index = min(self.stick_index, len(self.stick_sequence) - 1)
                self.stick_index += 1
                return self.stick_sequence[index]
            return self.stick_buttons
        return 0

    def call_mips(self, name, cpu, regs):
        vals = tuple(regs[4:8])
        if name.startswith("Pad:"):
            return self._pad_mips(name[4:], cpu, vals)
        self._record(name, vals)
        if name == "HeapAlloc":
            address = (0x801E0000, 0x801E9000)[min(self.alloc_count, 1)]
            self.alloc_count += 1
            return address
        if name == "IsTimeAttack":
            return self.time_attack
        if name == "GetDAT_800B01C2":
            return self.timer_gauge
        if name == "PauseMenuDrawSubScreen":
            if isinstance(self.subscreen_result, (list, tuple)):
                index = min(self.subscreen_index, len(self.subscreen_result) - 1)
                self.subscreen_index += 1
                return self.subscreen_result[index] & M32
            return int(self.subscreen_result) & M32
        if name == "TextNextGlyph":
            text, out = vals[:2]
            glyph = cpu.read16(text) if (text & 1) == 0 else cpu.read8(text)
            cpu.write16(out, glyph)
            return (text + 1) & M32
        return 0

    def _pad_mips(self, field, cpu, vals):
        self._record("Pad:" + field, vals)
        if field == "pad_get_state": return 6
        if field == "pad_info_mode": return 0
        if field == "pad_demo_step": return 0
        if field == "stick_to_buttons":
            if self.stick_sequence:
                index = min(self.stick_index, len(self.stick_sequence) - 1)
                self.stick_index += 1
                return self.stick_sequence[index]
            return self.stick_buttons
        return 0


def _gte_backend_factory():
    probe = mr.MipsRam()
    probe.attach_gte_oracle()
    return probe._gte_lib, probe.gte_backend, probe.gte_state


_GTE = None


def _attach_gte(cpu):
    global _GTE
    if _GTE is None:
        _GTE = _gte_backend_factory()
    lib, backend, state = _GTE
    for index in range(64):
        state[index] = 0
    cpu._gte_lib = lib
    cpu.gte_state = state
    cpu.gte_backend = backend


def _mips_hooks(name, model, calls):
    hooks = {}
    hook_names = set(FUNCTION_HOOKS.get(name, ()))
    if name in ("PauseMenuRun", "PauseMenuUpdate"):
        hook_names.update(HOOK_ADDRS)
    else:
        # Nested calls made by the ported wrapper use the same boundary callbacks.
        for nested in ("SoundLoopsResume", "ScreenSnapshotRestore", "PauseMenuDrawButton",
                       "PauseMenuDrawButtons", "HudDrawWindowFrame", "MessageBoxDrawString"):
            hook_names.update(FUNCTION_HOOKS.get(nested, ()))
    for hook_name in hook_names:
        if hook_name in COMPARE_C_ONLY_HOOKS:
            continue
        address = HOOK_ADDRS.get(hook_name)
        if address is None:
            continue
        def make_hook(n):
            def hook(machine, regs):
                value = model.call_mips(n, machine, regs)
                if value is None:
                    return 0
                return value
            return hook
        hooks[address] = make_hook(hook_name)
    if name in ("PauseMenuRun", "PauseMenuUpdate"):
        pad_targets = {
            "pad_get_state": 0x8002B52C,
            "pad_info_mode": 0x8002B5F8,
            "pad_setup_dualshock": 0x80012CA0,
            "pad_demo_step": 0x80013128,
            "stage_request": 0x8005D100,
            "stick_to_buttons": 0x800140B4,
            "pad_rumble_reset": 0x80014208,
        }
        for field, address in pad_targets.items():
            def make_pad_hook(n):
                def hook(machine, regs):
                    return model.call_mips("Pad:" + n, machine, regs)
                return hook
            hooks[address] = make_pad_hook(field)
        reg.declare("stack_window", "801FF000..80200000")
    if name == "PauseMenuRun":
        reg.declare("shared_random", "PauseMenuRun $s5")
    patches = {}
    if name == "PauseMenuRun":
        reg.declare("unreachable", "PauseMenuRun 8008052C..80080A3C")

        def gadget_screen_patch(machine, regs):
            model.call_mips("CutsceneModeOn", machine, (0, 0, 0, 0))
            hook_regs = regs[:]
            hook_regs[4] = regs[20]
            hook_regs[5] = machine.read8(0x800EC2D2)
            hook_regs[6] = machine.read32(0x800F51C4)
            hook_regs[7] = machine.read32(0x800EC35C)
            model.call_mips("RendererPauseMenuDrawGadgetScreen", machine, hook_regs)
            regs[4] = 276  # The skipped scene code leaves this x coordinate for TextSetCursor.
            return 0x80080A40

        patches[0x8008052C] = gadget_screen_patch
    if name == "HudDrawWindowFrame":
        reg.declare("unreachable", "HudDrawWindowFrame 80081F18..8008202C")
    return hooks, patches


def ram_case(cpu, cport, name, args, set_up, case_id, time_attack=0, stick_buttons=0,
             model_options=None):
    cpu.restore(cpu._pausemenu_base)
    set_up(cpu, case_id)
    cpu.coverage = set()
    _attach_gte(cpu)
    before = cpu.snapshot()
    options = model_options or {}
    mips_model = TestModel(cport, time_attack, stick_buttons,
                           options.get("stick_sequence"),
                           options.get("subscreen_result", 0),
                           options.get("timer_gauge", 0),
                           options.get("caller_s5", 0))
    c_model = TestModel(cport, time_attack, stick_buttons,
                        options.get("stick_sequence"),
                        options.get("subscreen_result", 0),
                        options.get("timer_gauge", 0),
                        options.get("caller_s5", 0))
    mips_hooks, mips_patches = _mips_hooks(name, mips_model, None)
    limit = 30_000_000 if name == "PauseMenuRun" else (
        9_000_000 if name == "PauseMenuUpdate" else 2_000_000)
    initial_regs = {21: options.get("caller_s5", 0)} if name == "PauseMenuRun" else None
    try:
        mips_result = cpu.call(FUNCS[name][0], args, hooks=mips_hooks,
                               patches=mips_patches, limit=limit,
                               initial_regs=initial_regs)[2] & M32
    except mr.MipsError as mips_error:
        scaled_model_trap = (name == "DrawScaledModelAt" and
                             "break at 8007CB40" in str(mips_error))
        pause_ring_trap = (name == "PauseMenuRun" and
                           any(f"break at {address:08X}" in str(mips_error)
                               for address in (0x80080020, 0x80080070)))
        if not scaled_model_trap and not pause_ring_trap:
            raise
        C.memmove(cport.main, before[0], 0x200000)
        C.memmove(cport.scr, before[1], 0x400)
        try:
            cport.run(name, args, c_model)
        except RuntimeError as c_error:
            expected = ("DrawScaledModelAt: signed divide trap" if scaled_model_trap
                        else "PauseMenuRun ring ")
            if expected not in str(c_error):
                raise
        else:
            raise AssertionError(f"{name} MIPS divide trap was not reproduced in C")
        lo = STACK_LO - 0x80000000
        if bytes(cport.main)[:lo] != bytes(cpu.ram)[:lo]:
            offset = next(i for i, (left, right) in enumerate(zip(cport.main[:lo], cpu.ram[:lo]))
                          if left != right)
            raise AssertionError(f"{name} divide trap leaves different RAM at "
                                 f"{0x80000000 + offset:08X}: "
                                 f"MIPS={cpu.ram[offset]:02X}/C={cport.main[offset]:02X}")
        if bytes(cport.scr) != bytes(cpu.scratch):
            raise AssertionError(f"{name} divide trap leaves different scratchpad")
        if c_model.log != mips_model.log:
            raise AssertionError(f"{name} divide trap makes different hook calls")
        return before, set(cpu.coverage)
    C.memmove(cport.main, before[0], 0x200000)
    C.memmove(cport.scr, before[1], 0x400)
    c_result = cport.run(name, args, c_model)
    if FUNCS[name][3] != "v" and c_result != mips_result:
        raise AssertionError(f"{name} case {case_id}: result MIPS={mips_result:08X}, C={c_result:08X}")
    c_ram, m_ram = bytes(cport.main), bytes(cpu.ram)
    lo = (STACK_LO - 0x80000000) if name in HOOKED_CHECKED else len(c_ram)
    if c_ram[:lo] != m_ram[:lo]:
        offset = next(i for i, (left, right) in enumerate(zip(c_ram[:lo], m_ram[:lo])) if left != right)
        mismatches = [(0x80000000 + i, m_ram[i], c_ram[i])
                      for i in range(offset, min(offset + 32, lo))
                      if c_ram[i] != m_ram[i]]
        detail = ", ".join(f"{address:08X}: MIPS={left:02X}/C={right:02X}"
                            for address, left, right in mismatches[:8])
        log_len = min(len(mips_model.log), len(c_model.log))
        log_diff = next((i for i in range(log_len) if mips_model.log[i] != c_model.log[i]), log_len)
        raise AssertionError(f"{name} case {case_id}: RAM differs at {0x80000000 + offset:08X} ({detail}); "
                             f"hook lengths MIPS={len(mips_model.log)}, C={len(c_model.log)}, first diff={log_diff}; "
                             f"hooks MIPS={mips_model.log[log_diff:log_diff+5]}, C={c_model.log[log_diff:log_diff+5]}")
    if bytes(cport.scr) != bytes(cpu.scratch):
        raise AssertionError(f"{name} case {case_id}: scratchpad differs")
    if name in HOOKED_CHECKED and c_model.log != mips_model.log:
        limit = min(len(c_model.log), len(mips_model.log))
        first = next((i for i in range(limit) if c_model.log[i] != mips_model.log[i]), limit)
        start, end = max(0, first - 6), first + 6
        raise AssertionError(f"{name} case {case_id}: hook calls differ at {first}: "
                             f"MIPS={mips_model.log[start:end]}, C={c_model.log[start:end]}")
    coverage = set(cpu.coverage)
    if name == "PauseMenuRun":
        coverage.difference_update(range(0x8008052C, 0x80080A40, 4))
    return before, coverage


def make_cases(name, rng, scale):
    if name == "PauseItemEnabled":
        items = [-0x80000000, -1, 0, 1, 2, 3, 4, 5, 0x7FFFFFFF]
        areas = [0, 19, 36, 37, 91, 92, 98, 99, 0xFFFF]
        levels = [0, 13, 15, 0xFFFF]
        rows = [(item, area, level) for item in items for area in areas for level in levels]
        return rows if scale == 100 else [rows[i] for i in range(0, len(rows), max(1, 100 // scale))]
    if name == "PauseMenuSetLayout":
        return [(layout,) for layout in range(8)]
    if name == "SetDAT_800BEF80":
        fixed = [0, 1, M32, 0x80000000]
        return [(v,) for v in fixed] + [(rng.getrandbits(32),) for _ in range(60)]
    if name == "PlayerStopGadgetSounds":
        return [(gadget,) for gadget in (0, 2, 4, 6, 1, 3, 5, 7)] * 2
    if name == "SoundLoopsSetPan":
        return [(resume,) for resume in (0, 1)] * 32
    if name == "SoundLoopsResume":
        return [() for _ in range(16)]
    if name == "DrawScaledModelAt":
        return [(0x800E78C0, value, update) for value in
                (0x400, 0x800, 0x1000, 0x1001, 0x2000, 0x4000, -0x1000, 0x10000, -1)
                for update in (0, 1)] + [(0x800E78C0, 0, 0)]
    if name == "PauseMenuDrawButton":
        rows = []
        for mode in range(4):
            for item in range(4):
                for selected in (-1, item):
                    for progress in (0, 1, 15, 31):
                        rows.append((item * 8 - 12, 20 - item * 6, item, selected, 0x801E0000))
        rows.extend([(20, 12, 4, -1, 0x801E0000), (20, 12, 4, 4, 0x801E0000)])
        return rows
    if name == "ScreenWipeBlurBlock":
        return [(block,) for block in (0, 1, 16, 239)]
    if name == "ScreenWipeStep":
        return [() for _ in range(3)]
    if name == "PauseMenuDrawButtons":
        return [() for _ in range(8)]
    if name == "HudDrawWindowFrame":
        return [(10, 20, 120, 80), (0, 0, 384, 240), (-1, 17, -50, 63)]
    if name == "PauseMenuUpdate":
        return [(0, 0), (2, 0), (1, 0x800)]
    if name == "MessageBoxDrawString":
        return [(0x800B2600 + i * 8,) for i in range(8)]
    if name == "TextWidthUntilGlyph":
        return [(0x800B2600 + i * 8, glyph) for i, glyph in enumerate((0, 65, 67, 0xFFFF))]
    if name == "RadarDrawModel":
        return [(0x800E78C0, 0x800E7900, 0x800E7940, angle)
                for angle in (0, 0x100, 0x400, 0x800)]
    if name == "PauseMenuRun":
        return [
            {"mode": 0, "progress": 0, "time_attack": 0, "sequence": [0]},
            {
                "mode": 1, "progress": 1, "time_attack": 0, "level": 0,
                "area": 37, "gadget_mask": 1, "current_gadget": 0,
                "player_mode": 0, "player_state": 0,
                "sequence": [0, 0x80, 0x1000, 0x1000, 0x800, 0x20, 0x800,
                             0x10, 0x8000, 0x2000, 0x1000, 0x4000, 0x10, 0x80,
                             0x20, 0x40, 0x800, 0x40, 0xC00, 0, 0xC00],
                "subscreen_result": [0, -1],
            },
            {
                "mode": 4, "progress": 0, "time_attack": 1, "level": 13,
                "area": 37, "sequence": [0x4000, 0x800, 0x20, 0x800, 0, 0x400],
            },
            {"mode": 4, "progress": 15, "time_attack": 0, "level": 0,
             "area": 37, "sequence": [0x800]},
            {"mode": 4, "progress": 15, "time_attack": 1, "level": 0,
             "area": 37, "sequence": [0x1000, 0x800]},
            {"mode": 1, "progress": 14, "time_attack": 0, "level": 0,
             "area": 37, "sequence": [0x800, 0]},
            {"mode": 10, "progress": 0, "time_attack": 0, "level": 0,
             "area": 37, "gadget_mask": 1, "current_gadget": 0,
             "slots": [0, 0, 0, 0],
             "sequence": [0x8000, 0, 0x2000, 0, 0x1000, 0, 0x4000, 0, 0x4000, 0,
                          0x10, 0, 0x80, 0, 0x20, 0, 0x40, 0, 0x800, 0, 0xC00]},
            {"mode": 10, "progress": 0, "time_attack": 0, "level": 0,
             "area": 37, "gadget_mask": 0, "current_gadget": 0,
             "slots": [0, 1, 2, 3], "sequence": [0x10, 0, 0x100]},
            {"mode": 11, "progress": 0, "time_attack": 0, "level": 0,
             "area": 37, "sequence": [0x800, 0, 0xC00]},
            {"mode": 12, "progress": 0, "time_attack": 0, "level": 0,
             "area": 37, "sequence": [0, 0xC00], "subscreen_result": -1},
            {"mode": 2, "progress": 256, "time_attack": 0, "level": 0,
             "area": 37, "gadget_mask": 1, "current_gadget": 0,
             "slots": [0, 1, 2, 3], "sequence": [0x10, 0, 0x100]},
            {"mode": 2, "progress": 256, "time_attack": 0, "level": 0,
             "area": 37, "sequence": [0x20, 0, 0x800, 0, 0xC00]},
            {"mode": 2, "progress": 256, "time_attack": 0, "level": 0,
             "area": 37, "sequence": [0x40, 0, 0xC00], "subscreen_result": -1},
            {"mode": 4, "progress": 0, "time_attack": 1, "level": 13,
             "area": 37, "sequence": [0x4000, 0, 0x4000, 0, 0x800],
             "timer_gauge": 1},
            {"mode": 1, "progress": 26, "time_attack": 0, "level": 0,
             "area": 37, "sequence": [0]},
            {"mode": 1, "progress": 37, "time_attack": 0, "level": 0,
             "area": 37, "sequence": [0]},
            {"mode": 10, "progress": 0, "time_attack": 0, "level": 0,
             "area": 37, "gadget_mask": 1, "current_gadget": 0,
             "slots": [1, 2, 3, 4], "sequence": [0x10, 0x100]},
            {"mode": 10, "progress": 0, "time_attack": 0, "level": 0,
             "area": 37, "gadget_mask": 1, "current_gadget": 0,
             "slots": [0, 0, 0, 0],
             "sequence": [0x10, 0, 0x80, 0, 0x20, 0, 0x40, 0x100]},
            {"mode": 4, "progress": 0, "time_attack": 1, "level": 13,
             "area": 37, "sequence": [0x800]},
            {"mode": 4, "progress": 0, "time_attack": 0, "level": 0,
             "area": 37, "sequence": [0x4000, 0, 0x20, 0, 0x800, 0]},
            {"mode": 2, "progress": 256, "time_attack": 1, "level": 13,
             "area": 37, "sequence": [0x80, 0x4000, 0, 0x800]},
            {"mode": 3, "progress": 15, "time_attack": 0, "level": 0,
             "area": 37, "caller_s5": 1, "sequence": [0, 0x800, 0, 0xC00]},
            {"mode": 3, "progress": 15, "time_attack": 0, "level": 0,
             "area": 37, "caller_s5": 4, "sequence": [0, 0x100]},
            {"mode": 0, "progress": 0, "time_attack": 0, "level": 0,
             "area": 37, "secondary_mode": 1, "sequence": [0x100]},
            {"mode": 0, "progress": 0, "time_attack": 0, "level": 0,
             "area": 19, "secondary_mode": 1, "sequence": [0x800]},
            {"mode": 10, "progress": 0, "time_attack": 0, "level": 0,
             "area": 37, "gadget_mask": 1, "current_gadget": 1,
             "slots": [0, 0, 2, 3], "sequence": [0x80, 0, 0x100]},
        ]
    return [() for _ in range(16)]


def setup_for(name, row):
    def setup(cpu, case_id):
        if name == "PauseItemEnabled":
            item, area, level = row
            cpu.write16(0x800F4472, level)
            cpu.write16(0x800F4476, area)
        elif name == "PauseMenuSetLayout":
            cpu.write_bytes(0x800B264C, bytes((case_id + i * 17) & 0xFF for i in range(64)))
        elif name == "SetDAT_800BEF80":
            cpu.write32(0x800BEF80, 0xA5A5A5A5)
        elif name == "ScreenSnapshotRestore":
            cpu.write32(0x800BEF7C, row[0])
            cpu.write32(0x800EC35C, case_id & 1)
        elif name == "PlayerStopGadgetSounds":
            base = 0x800F5120
            for offset in range(0, 0x50, 2):
                cpu.write16(base + offset, (offset + case_id + 7) & 0xFFFF)
            cpu.write32(base + 8, (1 << (case_id % 3)) | ((case_id & 1) << 1))
            cpu.write16(0x800F51D0 + 0x18, 12 + case_id)
            cpu.write16(0x800F51D0 + 0x1A, 20 + case_id)
        elif name == "SoundLoopsSetPan" or name == "SoundLoopsResume":
            if name == "SoundLoopsResume":
                cpu.write16(0x800F4474, 0x1B if case_id & 1 else 0)
                resume = 1
            else:
                resume = row[0]
            base = 0x800BF370
            for entry in range(24):
                at = base + entry * 28
                pan = (case_id * 5 + entry) & 0xF000
                voice = pan if pan != 0 else entry + 1
                cpu.write16(at, voice)
                cpu.write16(at + 2, entry if entry % 5 else -1)
                cpu.write16(at + 22, 1 if (entry + case_id) % 7 == 0 else 0)
        elif name == "DrawScaledModelAt":
            matrix = row[0]
            for offset in range(0, 32, 4):
                cpu.write32(matrix + offset, (0x1000 if offset in (0, 10, 20) else 0))
            for offset in range(0, 32, 4):
                cpu.write32(0x800E78C0 + offset, (0x1000 if offset in (0, 10, 20) else 0))
            cpu.write32(0x800FC0B0, 0xAABBCCDD)
            cpu.write32(0x800FC100, 0x11223344)
            cpu.write32(0x800F512C, 0x55667788)
        elif name == "PauseMenuDrawButton":
            mode = (case_id // 32) & 3
            progress = (0, 1, 15, 31)[case_id & 3]
            if case_id >= 128:
                mode = 4
            cpu.write8(0x800BEF74, mode)
            cpu.write16(0x800BEF76, progress)
            cpu.write16(0x800F4472, 1 if case_id & 1 else 0)
            cpu.write16(0x800F4476, (19, 36, 91, 99)[case_id & 3])
            cpu.write32(0x800EC350, case_id * 13)
            cpu.write32(0x800EC354, 0x801E0000 + (case_id & 0xFF) * 8)
        elif name in ("ScreenWipeBlurBlock", "ScreenWipeStep"):
            cpu.write_bytes(0x801A8000, bytes((i * 37 + case_id * 13) & 0xFF for i in range(0x40000)))
            kernel = (-4, 1, -2, 2, -1, 4, 0, 2, 1, 2, 2, 1, 3, 1, 4, 0, -3, 1)
            for i, value in enumerate(kernel):
                cpu.write16(0x800B2704 + i * 2, value)
            if name == "ScreenWipeBlurBlock":
                cpu.write32(0x800BEF78, 0)
                cpu.write32(0x800BEF7C, row[0])
            else:
                cpu.write32(0x800BEF78, 0 if case_id != 2 else 1)
                cpu.write32(0x800BEF7C, (239, 4, 0)[case_id])
        elif name == "PauseMenuDrawButtons":
            cpu.write8(0x800BEF74, case_id & 3)
            cpu.write16(0x800F4472, case_id % 16)
            cpu.write16(0x800F4476, (19, 36, 37, 91, 98, 99, 0, 13)[case_id])
            cpu.write32(0x800EC354, 0x801E0000)
            for item in range(4):
                at = 0x800B264C + item * 12
                for off, value in ((0, 20 + item * 13), (2, 32 + item * 9),
                                   (4, 50 + item * 7), (6, 60 + item * 11),
                                   (8, 0), (10, 0)):
                    cpu.write16(at + off, value)
        elif name == "PauseMenuUpdate":
            mode, _ = row
            cpu.write8(0x800BEF74, mode)
            cpu.write16(0x800BEF76, 1 if mode == 1 else 0)
            cpu.write8(0x800EC2D2, case_id & 7)
        elif name in ("MessageBoxDrawString", "TextWidthUntilGlyph"):
            text = row[0]
            cpu.write_bytes(text, b"ABC\x00")
            cpu.write_bytes(0x800B3640, bytes((i * 7 + case_id) & 0xFF for i in range(256)))
        elif name == "RadarDrawModel":
            for matrix in (row[0], row[1]):
                for offset in range(0, 32, 4):
                    cpu.write32(matrix + offset, (0x1000 if offset in (0, 10, 20) else 0))
            direction = row[2]
            cpu.write16(direction, 0x300)
            cpu.write16(direction + 2, (-2, 0, 2, 0x1000)[case_id])
            cpu.write16(direction + 4, 0x600)
            cpu.write16(0x800FC0BC, 0x200)
            cpu.write32(0x800EC35C, case_id & 1)
        elif name == "PauseMenuRun":
            mode, progress = row["mode"], row["progress"]
            cpu.write8(0x800BEF74, mode)
            cpu.write8(0x800BEF75, row.get("secondary_mode", 0))
            cpu.write16(0x800BEF76, progress)
            cpu.write32(0x800EC35C, case_id & 1)
            cpu.write16(0x800F4472, row.get("level", 0))
            cpu.write16(0x800F4476, row.get("area", 37))
            cpu.write8(0x800EC2D2, row.get("current_gadget", 0))
            cpu.write16(0x800F44A0, row.get("player_mode", 0))
            cpu.write16(0x800EC23E, row.get("player_state", 0))
            cpu.write32(0x800F51C4, row.get("gadget_mask", 0))
            for slot, gadget in enumerate(row.get("slots", (0, 1, 2, 3))):
                cpu.write8(0x800F51A8 + slot, gadget)
            cpu.write32(0x800E5478, 0)
            cpu.write32(0x800E5480, 0)
            cpu.write_bytes(0x800E52D0, bytes((0, 3, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80)))
            cpu.write_bytes(0x800E52F2, bytes((0, 3, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80)))
        else:
            cpu.write32(0x800BEF78, case_id * 0x10203)
            cpu.write32(0x800BEF7C, 0xDEADBEEF ^ case_id)
    return setup


def run_checked(cport, seed, scale):
    rng = random.Random(seed)
    bases = []
    for member in AREAS:
        path = ROOT / "extract/members" / member
        if path.is_file():
            bases.append(mr.MipsRam.from_extract(area=path).snapshot())
    if not bases:
        bases.append(mr.MipsRam.from_extract().snapshot())
    counts, coverage = {}, set()
    for name in CHECKED:
        if name == "ScreenSnapshotRestore":
            rows = [(row,) for row in ([-1, 0, 1, 239, 240, 241, 0x7FFFFFFF, 0x80000000] * 2)]
        else:
            rows = make_cases(name, rng, scale)
        counts[name] = len(rows)
        for index, row in enumerate(rows):
            cpu = mr.MipsRam()
            cpu._pausemenu_base = bases[index % len(bases)]
            call_args = () if name in ("ScreenWipeReset", "PauseMenuDrawButtons", "ScreenWipeStep",
                                       "SoundLoopsResume", "PauseMenuUpdate", "PauseMenuRun") else row
            if name == "PauseItemEnabled":
                call_args = row[:1]
            if name == "ScreenSnapshotRestore":
                call_args = ()
            time_attack = row["time_attack"] if name == "PauseMenuRun" else (
                (index & 1) if name == "PauseMenuDrawButtons" else 0)
            stick_buttons = 0 if name == "PauseMenuRun" else (
                row[1] if name == "PauseMenuUpdate" else 0)
            model_options = ({"stick_sequence": row.get("sequence", ()),
                             "subscreen_result": row.get("subscreen_result", 0),
                             "timer_gauge": row.get("timer_gauge", 0),
                             "caller_s5": row.get("caller_s5", 0)}
                            if name == "PauseMenuRun" else None)
            try:
                _, seen = ram_case(cpu, cport, name, call_args, setup_for(name, row), index,
                                   time_attack=time_attack, stick_buttons=stick_buttons,
                                   model_options=model_options)
            except BaseException as error:
                raise RuntimeError(f"{name} row {index}: {row}") from error
            coverage.update(seen)
    summary = reg.coverage_summary(
        [(name, FUNCS[name][0], FUNCS[name][1], True)
         for name in CHECKED], coverage)
    return counts, summary


def run_mutations(seed):
    """Require two intentionally wrong source variants per checked function to be detected."""
    mutations = {
        "ScreenWipeReset": [
            ("ae_wr32(ram, AE_PM_G_WIPE_DONE, 0u);", "ae_wr32(ram, AE_PM_G_WIPE_DONE, 1u);", ()),
            ("ae_wr32(ram, AE_PM_G_WIPE_ROW, 239u);", "ae_wr32(ram, AE_PM_G_WIPE_ROW, 238u);", ()),
        ],
        "PauseMenuSetLayout": [
            ("AE_PM_LAYOUT_BUTTON0_HEIGHT, -32", "AE_PM_LAYOUT_BUTTON0_HEIGHT, -31", (0,)),
            ("AE_PM_LAYOUT_BUTTON3_Y, 30", "AE_PM_LAYOUT_BUTTON3_Y, 31", (5,)),
        ],
        "PauseItemEnabled": [
            ("area == 19u || area == 36u", "area == 20u || area == 36u", (0, 19, 0)),
            ("current_level != 0x0Du", "current_level == 0x0Du", (1, 0, 0)),
        ],
        "SetDAT_800BEF80": [
            ("AE_PM_G_PAUSE_MENU_VALUE, value);", "AE_PM_G_PAUSE_MENU_VALUE, value ^ 1u);", (0x1234ABCD,)),
            ("AE_PM_G_PAUSE_MENU_VALUE, value);", "AE_PM_G_PAUSE_MENU_VALUE + 4u, value);", (0x87654321,)),
        ],
        "ScreenSnapshotRestore": [
            ("if (row > 0)", "if (row >= 0)", (0,)),
            ("AE_PM_G_SCREEN_SNAPSHOT + source_offset", "AE_PM_G_SCREEN_SNAPSHOT + source_offset + 2u", (239,)),
        ],
        "PlayerStopGadgetSounds": [
            ("AE_PM_SOUND_LOOP_ID_GADGET4_B      38u", "AE_PM_SOUND_LOOP_ID_GADGET4_B      39u", (2,)),
            ("sound_state + AE_PM_GADGET_SOUND_HANDLE);\n    if (handle != -1)",
             "sound_state + AE_PM_GADGET_SOUND_HANDLE);\n    if (handle == -1)", (1,)),
        ],
        "SoundLoopsSetPan": [
            ("stored_pan == 0", "stored_pan != 0", (0,)),
            ("ae_rds16(ram, entry + AE_PM_GADGET3_FRAME_FIELD) != 1",
             "ae_rds16(ram, entry + AE_PM_GADGET3_FRAME_FIELD) == 1", (1,)),
        ],
        "SoundLoopsResume": [
            ("ae_rd16(ram, AE_PM_G_STAGE_KIND) == 0x1Bu",
             "ae_rd16(ram, AE_PM_G_STAGE_KIND) != 0x1Bu", ()),
            ("ae_SoundLoopsSetPan(ram, hooks, 1);", "ae_SoundLoopsSetPan(ram, hooks, 0);", ()),
        ],
        "DrawScaledModelAt": [
            ("if (update_player_matrix != 0)", "if (update_player_matrix == 0)",
             (0x800E78C0, 0x1000, 1)),
            ("uint32_t first_transform = AE_PM_G_MODEL_MATRIX_A;",
             "uint32_t first_transform = AE_PM_G_MODEL_MATRIX_B;",
             (0x800E78C0, 0x1000, 0)),
        ],
        "PauseMenuDrawButton": [
            ("{2u, 3u, 1u, 0u}", "{3u, 3u, 1u, 0u}",
             (20, 12, 0, -1, 0x801E0000)),
            ("uint32_t model_index = pm_button_model_index((uint32_t)item);",
             "uint32_t model_index = pm_button_model_index((uint32_t)item) ^ 1u;",
             (20, 12, 0, 0, 0x801E0000)),
        ],
        "ScreenWipeBlurBlock": [
            ("pixel < 382u", "pixel < 381u", (0,)),
            ("block_start + 0x600u", "block_start + 0x602u", (1,)),
        ],
        "ScreenWipeStep": [
            ("row < 4 ? 0 : row - 4", "row < 4 ? 0 : row - 3", ()),
            ("if (stop_row == 0)", "if (stop_row != 0)", ()),
        ],
        "PauseMenuRun": [
            ("ae_PauseMenuSetLayout(ram, 2u);", "ae_PauseMenuSetLayout(ram, 3u);",
             {"mode": 0, "progress": 0, "secondary_mode": 1,
              "time_attack": 0, "area": 37, "sequence": [0x100]}),
            ("return_value = gadget_cursor;", "return_value = menu_cursor;",
             {"mode": 10, "progress": 0, "time_attack": 0, "area": 37,
              "gadget_mask": 2, "current_gadget": 0, "slots": [1, 2, 3, 4],
              "sequence": [0x2000, 0, 0x10, 0x100]}),
        ],
        "PauseMenuDrawButtons": [
            ("current_ot + 88u", "current_ot + 84u", ()),
            ("time_attack != 0u && ae_rds8(ram, AE_PM_G_PAUSE_MODE) != AE_PM_MODE_SUBMENU_A",
             "time_attack == 0u && ae_rds8(ram, AE_PM_G_PAUSE_MODE) != AE_PM_MODE_SUBMENU_A", ()),
        ],
        "HudDrawWindowFrame": [
            ("(uint32_t)x, (uint32_t)y, (uint32_t)width, (uint32_t)height",
             "(uint32_t)(x + 1), (uint32_t)y, (uint32_t)width, (uint32_t)height",
             (10, 20, 120, 80)),
            ("(uint32_t)x, (uint32_t)y, (uint32_t)width, (uint32_t)height",
             "(uint32_t)x, (uint32_t)y, (uint32_t)(width + 1), (uint32_t)height",
             (10, 20, 120, 80)),
        ],
        "PauseMenuUpdate": [
            ("PM_CALL(ram, hooks, WaitFrameFlip, 0u, 0u, 0u, 0u)",
             "(void)0", (1, 0x800)),
            ("ae_PlayerStopGadgetSounds(ram, hooks, gadget);",
             "ae_PlayerStopGadgetSounds(ram, hooks, gadget + 1);", (1, 0x800)),
        ],
        "MessageBoxDrawString": [
            ("FontDrawGlyph, glyph, 496u, 0u", "FontDrawGlyph, glyph, 497u, 0u",
             (0x800B2600,)),
            ("if (glyph == 0u) return;", "if (glyph == (uint32_t)'B') return;",
             (0x800B2600,)),
        ],
        "TextWidthUntilGlyph": [
            ("width += ae_rd8(ram, widths + current);",
             "width += ae_rd8(ram, widths + current) + 1u;",
             (0x800B2600, ord("C"))),
            ("if (current == wanted) return (int32_t)width;",
             "if (current != wanted) return (int32_t)width;",
             (0x800B2600, ord("A"))),
        ],
        "RadarDrawModel": [
            ("RotMatrixY, 0x400u, local_matrix", "RotMatrixY, 0x401u, local_matrix",
             (0x800E78C0, 0x800E7900, 0x800E7940, 0x400)),
            ("if (height > 0)", "if (height >= 0)",
             (0x800E78C0, 0x800E7900, 0x800E7940, 0x100)),
        ],
    }
    base_source = (ROOT / "src/ui/ae_pausemenu.c").read_text(encoding="utf-8")
    base_cpu = mr.MipsRam.from_extract(area=ROOT / "extract/members/000.bin")
    cport = None
    caught = 0
    for name, variants in mutations.items():
        for index, (old, new, call_args) in enumerate(variants):
            if old not in base_source:
                raise AssertionError(f"mutation target missing for {name}: {old}")
            mutated = base_source.replace(old, new, 1)
            source_path = OUT / f"pausemenu_mutant_{name}_{index}_{os.getpid()}.c"
            source_path.write_text(mutated, encoding="utf-8")
            try:
                mutant = CPort(build_dll(source_path, tag=f"pausemenu_mutant_{name}_{index}"))
            finally:
                source_path.unlink(missing_ok=True)
            if name in HOOKED_CHECKED:
                cpu = mr.MipsRam()
                cpu._pausemenu_base = base_cpu.snapshot()
                test_row = call_args
                case_id = 2 if name == "PauseMenuUpdate" else index
                actual_args = () if name in (
                    "ScreenSnapshotRestore", "SoundLoopsResume", "PauseMenuRun",
                    "PauseMenuDrawButtons", "PauseMenuUpdate") else call_args
                time_attack = 0
                stick_buttons = 0
                model_options = None
                if name == "PauseMenuRun":
                    time_attack = test_row["time_attack"]
                    model_options = {
                        "stick_sequence": test_row.get("sequence", ()),
                        "subscreen_result": test_row.get("subscreen_result", 0),
                        "timer_gauge": test_row.get("timer_gauge", 0),
                        "caller_s5": test_row.get("caller_s5", 0),
                    }
                elif name == "PauseMenuUpdate":
                    stick_buttons = test_row[1]
                elif name == "PauseMenuDrawButtons":
                    time_attack = case_id & 1
                try:
                    ram_case(cpu, mutant, name, actual_args, setup_for(name, test_row), case_id,
                             time_attack=time_attack, stick_buttons=stick_buttons,
                             model_options=model_options)
                except AssertionError:
                    caught += 1
                    print(f"caught mutation: {name} #{index + 1}")
                    continue
                raise AssertionError(f"checker did not catch mutation {name} #{index + 1}")
            cpu = mr.MipsRam(); cpu.restore(base_cpu.snapshot())
            if name == "PauseItemEnabled":
                item, area, level = call_args
                cpu.write16(0x800F4472, level); cpu.write16(0x800F4476, area)
                args = (item,)
            elif name == "PauseMenuSetLayout":
                cpu.write_bytes(0x800B264C, bytes(range(64)))
                args = call_args
            elif name == "SetDAT_800BEF80":
                args = call_args
            else:
                args = call_args
            before = cpu.snapshot()
            mips_result = cpu.call(FUNCS[name][0], args)[2] & M32
            C.memmove(mutant.main, before[0], 0x200000)
            c_result = mutant.run(name, args)
            differs = (FUNCS[name][3] != "v" and c_result != mips_result) or bytes(mutant.main) != bytes(cpu.ram)
            if not differs:
                raise AssertionError(f"checker did not catch mutation {name} #{index + 1}")
            caught += 1
            print(f"caught mutation: {name} #{index + 1}")
    return caught


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", type=int, default=100, help="percentage of randomized cases (default: 100)")
    ap.add_argument("--seed", type=lambda x: int(x, 0), default=DEFAULT_SEED)
    ap.add_argument("--mutate", action="store_true")
    args = ap.parse_args()
    if not 1 <= args.cases <= 100:
        ap.error("--cases must be 1..100")

    cport = CPort(build_dll())
    counts, summary = run_checked(cport, args.seed, args.cases)
    for name in CHECKED:
        print(f"{name}: {counts[name]} cases, {summary[name]['coverage_pct']:.2f}% instruction coverage")
        if summary[name]["uncovered"]:
            print(f"  uncovered: {summary[name]['uncovered']}")
    print("Functions checked:", len(CHECKED))
    mutation_count = run_mutations(args.seed) if args.mutate else 0
    default_run = args.cases == 100 and args.seed == DEFAULT_SEED and not args.mutate
    if default_run:
        all_counts = {name: counts.get(name, 0) for name in FUNCS}
        result = reg.write_results(all_counts, summary, True,
                                   extra={"checked_functions": list(CHECKED),
                                          "wip_functions": [],
                                          "notes": "PauseMenuRun actual span to 80081144 is 5880 bytes; inventory lists 3980."},
                                   function_names=list(FUNCS))
        print(f"results: {result}")
    return 0


if __name__ == "__main__":
    main()
