"""Differential checks for the readable member 186 C port, range C.

The original overlay is loaded at 0x80136000 over SCUS-94423 and real area
members.  Calls at PsyQ, BIOS, earlier-overlay and renderer boundaries are
named and modelled identically on the MIPS and AeRam sides.
"""
import argparse
import ctypes as C
import os
import random
import re
import shutil
import subprocess
import sys
import time
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

OVERLAY_BASE, OVERLAY_SIZE = 0x80136000, 0xF2A0
STACK_LO, STACK_HI = 0x801FF000, 0x80200000
WORK, WORK_SIZE = 0x801FE000, 12
MUTATION_SELECTOR, MUTATION_MARKER = 0x801FEE00, 0x801FED00
LIMIT = 180_000
M = 0xFFFFFFFF

# name -> (address, size, exported C symbol, return type, argument kinds)
FUNCS = {
    "CardMgr_SetState": (0x8013CD1C, 0x480, "ae_CardMgr_SetState", "i", "uu"),
    "CardMgr_StartOps": (0x8013D19C, 0x5C, "ae_CardMgr_StartOps", "i", ""),
    "SsGetMute": (0x8013D1F8, 0x20, "ae_SsGetMute", "i", ""),
    "CardMgr_IsBusy": (0x8013D218, 0x20, "ae_CardMgr_IsBusy", "i", ""),
    "CardMgr_StateMachine": (0x8013D238, 0x450, "ae_CardMgr_StateMachine", "i", ""),
    "CardMgr_OnCardResult_0": (0x8013D834, 0xF8, "ae_CardMgr_OnCardResult_0", "i", ""),
    "CardMgr_OnCardResult_1": (0x8013D92C, 0xD4, "ae_CardMgr_OnCardResult_1", "i", ""),
    "CardMgr_OnCardResult_2": (0x8013DA00, 0x7C, "ae_CardMgr_OnCardResult_2", "i", ""),
    "CardMgr_OnCardResult_3": (0x8013DA7C, 0xCC, "ae_CardMgr_OnCardResult_3", "i", ""),
    "CardMgr_OnCardResult_4": (0x8013DB48, 0xA8, "ae_CardMgr_OnCardResult_4", "i", ""),
    "CardMgr_OnCardResult_5": (0x8013DBF0, 0x88, "ae_CardMgr_OnCardResult_5", "i", ""),
    "CardMgr_OnCardResult_6": (0x8013DC78, 0x178, "ae_CardMgr_OnCardResult_6", "i", ""),
    "CardMgr_OnCardResult_7": (0x8013DDF0, 0xC4, "ae_CardMgr_OnCardResult_7", "i", ""),
    "CardMgr_OnCardResult_8": (0x8013DEB4, 0x8C, "ae_CardMgr_OnCardResult_8", "i", ""),
    "CardMgr_OnCardResult_9": (0x8013DF40, 0x158, "ae_CardMgr_OnCardResult_9", "i", ""),
    "McxCardTypeSync": (0x8013E098, 0x7C, "ae_McxCardTypeSync", "i", ""),
    "McxAllInfoSync": (0x8013E114, 0x74, "ae_McxAllInfoSync", "i", ""),
    "CardMgr_ResetSlots": (0x8013E188, 0xA4, "ae_CardMgr_ResetSlots", "i", "u"),
    "McxHideTransSync": (0x8013E22C, 0x64, "ae_McxHideTransSync", "i", "u"),
    "CardMgr_Draw": (0x8013E290, 0x7C, "ae_CardMgr_Draw", "i", ""),
    "CardMgr_MenuUpdate": (0x8013E30C, 0x12C, "ae_CardMgr_MenuUpdate", "i", ""),
    "CardMgr_AnimTick": (0x8013EB14, 0xBC, "ae_CardMgr_AnimTick", "i", "u"),
    "CardMgr_RunMessage": (0x8013F630, 0x1D8, "ae_CardMgr_RunMessage", "i", ""),
    "CardMgr_DrawMessage": (0x8013F808, 0x1F0, "ae_CardMgr_DrawMessage", "i", "u"),
    "CardMgr_DrawMenuLabels": (0x8013FB30, 0xF0, "ae_CardMgr_DrawMenuLabels", "i", "u"),
    "CardMgr_DrawBlinkingMenu": (0x8013FEFC, 0xF0, "ae_CardMgr_DrawBlinkingMenu", "i", "u"),
    "SaveUi_DrawStateOverlay": (0x8014016C, 0x238, "ae_SaveUi_DrawStateOverlay", "i", ""),
    "SaveUi_GetColorBuffer": (0x8013FC20, 0xC, "ae_SaveUi_GetColorBuffer", "u", ""),
    "CardMgr_DrawMenuItems": (0x8013FC2C, 0x118, "ae_CardMgr_DrawMenuItems", "i", "uu"),
    "SaveUi_GetBuffer2": (0x8013FD44, 0xC, "ae_SaveUi_GetBuffer2", "u", ""),
    "CardMgr_DrawMenuTexts": (0x8013FD50, 0x94, "ae_CardMgr_DrawMenuTexts", "i", "u"),
    "CardMgr_DrawMenuItems2": (0x8013FDE4, 0xBC, "ae_CardMgr_DrawMenuItems2", "i", "uu"),
    "SaveUi_DrawStringPair": (0x8013FEA0, 0x5C, "ae_SaveUi_DrawStringPair", "i", ""),
    "SaveUi_DrawYesNo": (0x8013FFEC, 0x88, "ae_SaveUi_DrawYesNo", "i", "u"),
    "SaveUi_DrawFileHeader": (0x80140074, 0x80, "ae_SaveUi_DrawFileHeader", "i", "u"),
    "SaveUi_DrawEmptySlotText": (0x801400F4, 0x78, "ae_SaveUi_DrawEmptySlotText", "i", "u"),
    "SaveUi_ThreadSetHook": (0x80141A10, 0xA4, "ae_SaveUi_ThreadSetHook", "i", "u"),
    "SaveUi_ThreadClose": (0x80141AB4, 0x3C, "ae_SaveUi_ThreadClose", "i", ""),
    "SaveUi_Nop": (0x80141AF0, 0x10, "ae_SaveUi_Nop", "i", ""),
    "SaveUi_ThreadSwitch": (0x80141B00, 0x34, "ae_SaveUi_ThreadSwitch", "i", ""),
    "SaveUi_ThreadSetHookReset": (0x80141B34, 0x30, "ae_SaveUi_ThreadSetHookReset", "i", ""),
    "McxStartCom": (0x80141B90, 0x70, "ae_McxStartCom", "i", ""),
    "MCXINIT_OBJ_B0": (0x80141C40, 0x4C, "ae_MCXINIT_OBJ_B0", "i", "uu"),
    "MCXINIT_OBJ_FC": (0x80141C8C, 0x8, "ae_MCXINIT_OBJ_FC", "u", "uu"),
    "MCXINIT_OBJ_104": (0x80141C94, 0x40, "ae_MCXINIT_OBJ_104", "i", ""),
    "MCXINIT_OBJ_144": (0x80141CD4, 0x40, "ae_MCXINIT_OBJ_144", "i", ""),
    "MCXINIT_OBJ_184": (0x80141D14, 0x138, "ae_MCXINIT_OBJ_184", "i", ""),
    "MCXINIT_OBJ_2BC": (0x80141E4C, 0x60, "ae_MCXINIT_OBJ_2BC", "i", ""),
}

# These routines build only native GPU packets. SelectionBars is patched to
# continue into the real animation tick, whose game-state effects are ported.
NATIVE_ONLY = {"CardMgr_DrawSelectionBars", "CardMgr_DrawFileList", "CardMgr_DrawIconAnim",
               "CardMgr_DrawProgressBar", "CardMgr_DrawMessageLines",
               }
EXTRA_COVERAGE = [
    ("CardMgr_DrawSelectionBars", 0x8013E438, 0x1CC),
    ("CardMgr_DrawFileList", 0x8013E604, 0x510),
    ("CardMgr_DrawIconAnim", 0x8013EBD0, 0x13C),
    ("CardMgr_DrawProgressBar", 0x8013ED0C, 0x1CC),
    ("CardMgr_DrawMessageLines", 0x8013F9F8, 0x110),
    ("CardMgr_DrawMessage inline GPU packets", 0x8013F848, 0x90),
]

API = {
    0x80010000: ("MemCardOp", 2),
    0x80021EC8: ("MemCardSync", 3),
    0x8002E520: ("VSync", 1),
    0x800877F8: ("PadGetState", 1),
    0x8008211C: ("TextDraw", 3),
    0x8002F200: ("EnterCriticalSection", 0),
    0x8002F210: ("GetCurrentThread", 0),
    0x8002F220: ("SetThreadHook", 3),
    0x8002F230: ("ExitCriticalSection", 0),
    0x8002F240: ("CloseThread", 1),
    0x8002F250: ("ChangeThread", 1),
    0x8002F260: ("CreateThread", 2),
    0x8002F270: ("OpenThread", 2),
    0x8002F280: ("StartThread", 2),
    0x8001325C: ("PlaySoundEffect", 1),
    0x8005DF48: ("BeginFrame", 0),
    0x80028850: ("ClearImage", 4),
    0x800286CC: ("DrawSync", 1),
    0x80028D30: ("PutDrawEnv", 1),
    0x80028EFC: ("PutDispEnv", 1),
    0x80028CC0: ("DrawOTag", 1),
    0x801444D4: ("McxCardType", 1),
    0x8014464C: ("McxAllInfo", 2),
    0x8014236C: ("McxSync", 3),
    0x80144084: ("McxHideTrans", 1),
    0x80143F54: ("McxShowTrans", 3),
    0x80145374: ("McxComInit", 0),
    0x801424E0: ("McxCheckThreadReady", 1),
    0x80141EBC: ("MCXINIT_OBJ_32C", 0),
    0x80145440: ("McxFastDsrEntry", 0),
}

OVERLAY_HOOKS = {
    0x80136000: ("SaveUi_DrawText", 4),
    0x80136120: ("SaveUi_TextWidth", 1),
    0x80136160: ("SaveUi_DrawWindow", 4),
    0x80136554: ("SaveUi_DrawWindowBorder", 4),
    0x8013946C: ("SaveUi_DrawIcon", 3),
    0x80139A3C: ("SaveUi_DrawMainWindow", 1),
    0x80139A68: ("SaveUi_DrawBackground3D", 0),
    0x80139F08: ("SaveUi_YesNoUpdate", 0),
    0x8013A400: ("SaveUi_ConfirmPressed", 0),
    0x8013A634: ("SaveUi_FadeTick", 0),
    0x8013784C: ("SaveUi_SetState", 2),
    0x80145374: ("McxComInit", 0),
    0x801424E0: ("McxCheckThreadReady", 1),
    0x80141EBC: ("MCXINIT_OBJ_32C", 0),
    0x80145440: ("McxFastDsrEntry", 0),
}

RENDERER_NAMES = {
    0x8013E438: "CardMgr_DrawSelectionBars",
    0x8013E604: "CardMgr_DrawFileList",
    0x8013EBD0: "CardMgr_DrawIconAnim",
    0x8013ED0C: "CardMgr_DrawProgressBar",
    0x8013F9F8: "CardMgr_DrawMessageLines",
}


def s32(v):
    v &= M
    return v - 0x100000000 if v & 0x80000000 else v


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


class Mem:
    def __init__(self, cpu=None, dll=None, ram=None):
        self.cpu, self.dll, self.ram = cpu, dll, ram

    def rd8(self, a): return self.cpu.read8(a) if self.cpu else self.dll.ae_rd8(self.ram, a)
    def rd16(self, a): return self.cpu.read16(a) if self.cpu else self.dll.ae_rd16(self.ram, a)
    def rd32(self, a): return self.cpu.read32(a) if self.cpu else self.dll.ae_rd32(self.ram, a)
    def wr8(self, a, v):
        if self.cpu: self.cpu.write8(a, v)
        else: self.dll.ae_wr8(self.ram, a, v)
    def wr16(self, a, v):
        if self.cpu: self.cpu.write16(a, v)
        else: self.dll.ae_wr16(self.ram, a, v)
    def wr32(self, a, v):
        if self.cpu: self.cpu.write32(a, v)
        else: self.dll.ae_wr32(self.ram, a, v)
    def write_bytes(self, a, data):
        if self.cpu: self.cpu.write_bytes(a, data)
        else:
            for i, value in enumerate(data): self.dll.ae_wr8(self.ram, a + i, value)
    def bytes(self, a, n):
        return self.cpu.read_bytes(a, n) if self.cpu else C.string_at(self.dll.ae_ptr(self.ram, a, n), n)


class Model:
    def __init__(self, mem, profile):
        self.mem, self.profile = mem, profile
        self.events, self.counts = [], Counter()

    def _canonical(self, name, args):
        out = list(args)
        slots = {"MemCardSync": {1: WORK, 2: WORK + 4},
                 "McxSync": {1: WORK, 2: WORK + 4},
                 "McxAllInfo": {1: WORK + 8}}
        for index, addr in slots.get(name, {}).items():
            if index < len(out): out[index] = addr
        return tuple(x & M for x in out)

    def call(self, name, address, args):
        args = tuple(int(x) & M for x in args)
        self.events.append(("call", name, address & M, self._canonical(name, args)))
        n = self.counts[name]
        self.counts[name] += 1
        if name in ("MemCardSync", "McxSync"):
            event_ptr, status_ptr = args[1], args[2]
            self.mem.wr32(event_ptr, self.profile.get("event_word", 0x100 + n))
            self.mem.wr32(status_ptr, self.profile.get("sync_status", 0))
            return 0
        if name == "McxAllInfo":
            self.mem.wr16(args[1], self.profile.get("card_info", 0))
            return 0
        if (name == "MemCardOp" and args[0] == 13 and
                "card_operation_after_memcard_op13" in self.profile):
            self.mem.wr16(0x800E3880,
                          self.profile["card_operation_after_memcard_op13"])
        if name == "VSync":
            return self.profile.get("vsync", 1000 + n)
        if name == "PadGetState":
            return 0
        if name == "TextDraw":
            return self.profile.get("text_draw_result", 0)
        if name == "SaveUi_TextWidth":
            total = 0
            for i in range(4096):
                ch = self.mem.rd8(args[0] + i)
                if ch == 0: break
                total += self.mem.rd8(0x8013B024 + ch)
            return total
        if name == "SaveUi_YesNoUpdate":
            return self.profile.get("yesno", 0)
        if name == "SaveUi_ConfirmPressed":
            return self.profile.get("confirm", 0)
        if name == "SaveUi_FadeTick":
            seq = self.profile.get("fade_seq", [-1])
            self.mem.wr16(0x8013AF66, self.profile.get("fade_menu", 0))
            return seq[min(n, len(seq) - 1)]
        if name == "CardMgr_RunMessage_InitialS1":
            return self.profile.get("initial_s1", 0)
        if name in ("GetCurrentThread",):
            return self.profile.get("thread_id", 4)
        if name == "SetThreadHook":
            return self.profile.get("thread_handle", 2)
        if name in ("CreateThread", "OpenThread", "StartThread", "CloseThread",
                    "ChangeThread", "EnterCriticalSection", "ExitCriticalSection",
                    "McxCardType", "McxHideTrans", "McxShowTrans", "MemCardOp",
                    "PlaySoundEffect", "BeginFrame", "ClearImage", "DrawSync",
                    "PutDrawEnv", "PutDispEnv", "DrawOTag", "SaveUi_DrawText",
                    "SaveUi_DrawWindow", "SaveUi_DrawWindowBorder", "SaveUi_DrawIcon",
                    "SaveUi_DrawMainWindow", "SaveUi_DrawBackground3D", "SaveUi_SetState",
                    "McxComInit", "McxCheckThreadReady", "MCXINIT_OBJ_32C",
                    "McxFastDsrEntry"):
            return self.profile.get("returns", {}).get(name, 0)
        return self.profile.get("returns", {}).get(name, 0)

    def render(self, name, args):
        self.events.append(("render", name, None, tuple(int(x) & M for x in args)))
        return 0


class OvlHooks(C.Structure):
    _fields_ = [("game", C.c_void_p), ("user", C.c_void_p),
                ("call", C.c_void_p), ("render", C.c_void_p)]


class CPort:
    def __init__(self, dll_path):
        self.dll = d = C.CDLL(str(dll_path))
        P, U, I = C.c_void_p, C.c_uint32, C.c_int32
        d.ae_ram_create.restype = P
        d.ae_ptr.argtypes, d.ae_ptr.restype = [P, U, U], P
        for n in ("ae_rd8", "ae_rd16", "ae_rd32"):
            getattr(d, n).argtypes, getattr(d, n).restype = [P, U], U
        for n in ("ae_wr8", "ae_wr16", "ae_wr32"):
            getattr(d, n).argtypes = [P, U, U]
        d.ae_ram_clear_error.argtypes = [P]
        d.ae_ram_failed.argtypes, d.ae_ram_failed.restype = [P], I
        d.ae_ram_error.argtypes, d.ae_ram_error.restype = [P], C.c_char_p
        self.ram = d.ae_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(d.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(d.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.mem = Mem(dll=d, ram=self.ram)
        self.hooks = OvlHooks()
        self.active = None
        self.keep = []
        call_t = C.CFUNCTYPE(I, P, P, C.c_char_p, U, U, C.POINTER(U))
        render_t = C.CFUNCTYPE(I, P, P, C.c_char_p, U, C.POINTER(U))

        def native(_user, _ram, name, address, count, argv):
            return self.active.call(name.decode("ascii"), address,
                                    [argv[i] for i in range(count)])
        def renderer(_user, _ram, name, count, argv):
            return self.active.render(name.decode("ascii"), [argv[i] for i in range(count)])
        self.cb_call, self.cb_render = call_t(native), render_t(renderer)
        self.keep += [self.cb_call, self.cb_render]
        self.hooks.call = C.cast(self.cb_call, C.c_void_p)
        self.hooks.render = C.cast(self.cb_render, C.c_void_p)
        self.funcs = {}
        for name, (_addr, _size, symbol, ret, kinds) in FUNCS.items():
            fn = getattr(d, symbol)
            fn.argtypes = [P, C.POINTER(OvlHooks)] + [I if k == "s" else U for k in kinds]
            fn.restype = U if ret == "u" else I
            self.funcs[name] = fn

    def sync(self, snap):
        C.memmove(self.main, snap[0], 0x200000)
        C.memmove(self.scratch, snap[1], 0x400)
        self.dll.ae_ram_clear_error(self.ram)

    def call(self, name, args, model):
        self.active = model
        fn = self.funcs[name]
        kinds = FUNCS[name][4]
        vals = [s32(v) if k == "i" else v & M for v, k in zip(args, kinds)]
        return int(fn(self.ram, C.byref(self.hooks), *vals)) & M

    def error(self):
        if self.dll.ae_ram_failed(self.ram):
            return self.dll.ae_ram_error(self.ram).decode("utf-8", "replace")
        return None


def build(source=None, tag="ovl186c"):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{time.time_ns()}.dll"
    exports = dll.with_suffix(".def")
    api_exports = ["ae_ram_create", "ae_ram_clear_error", "ae_ram_failed", "ae_ram_error", "ae_ptr",
                   "ae_rd8", "ae_rd16", "ae_rd32", "ae_wr8", "ae_wr16", "ae_wr32"]
    exports.write_text("EXPORTS\n" + "\n".join(api_exports + [v[2] for v in FUNCS.values()]) + "\n", encoding="ascii")
    gcc = shutil.which("gcc")
    if not gcc: raise SystemExit("gcc is required for the overlay C checker")
    subprocess.run([gcc, "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror", "-shared", "-static-libgcc",
                    "-Isrc/modes", "-Isrc/core", str(ROOT / "src/core/ae_ram.c"),
                    str(source or ROOT / "src/modes/ae_ovl186c.c"), str(exports), "-o", str(dll)],
                   cwd=ROOT, check=True)
    return dll


def data_cases(name, scale=4):
    """Return explicit (arguments, setup profile) scenarios. Tailored cases ensure branches are exercised."""
    rng = random.Random(f"186c/{name}")
    cases = []
    def add(args=(), **profile): cases.append((tuple(args), profile))
    if name == "CardMgr_SetState":
        for state in range(14, 50): add((state, 0), vsync=0x7100 + state)
        add((13, 0)); add((50, 0)); add((29, 1)); add((29, 0)); add((32, 0))
        add((29, 0), ui_operation=1); add((32, 0), ui_operation=1)
        add((34, 0), ui_operation=0); add((34, 0), ui_operation=1)
        add((32, 0), ui_operation=1,
            primary_values=[6, 70, 17, 24, 31, 38, 45, 60, 59, 66, 73, 80])
        add((32, 0), ui_operation=1,
            primary_values=[6, 10, 80, 24, 70, 38, 45, 12, 40, 66, 40, 80])
        add((32, 0), ui_operation=1,
            primary_values=[6, 10, 10, 24, 10, 38, 45, 12, 20, 66, 10, 80])
    elif name == "CardMgr_StateMachine":
        for state in range(14, 50):
            for variant in range(2):
                add((), state=state, state_variant=variant, yesno=(variant + state) % 3,
                    confirm=(variant + state) & 1, sync_status=(variant + state) % 2,
                    card_info=(variant + state) % 2, fade_menu=1)
        for state in (18, 30):
            for confirm in (0, 1):
                add((), state=state, card_mode=11, confirm=confirm, fade_menu=1)
        for operation in (0, 1):
            for buttons in (0, 0x20, 0x800, 0x820):
                add((), state=29, buttons=buttons, ui_operation=operation,
                    primary0=9, primary_limit=7, fade_menu=1)
        for timeout, operation in ((90, 0), (2, 1)):
            add((), state=34, timeout=timeout, ui_operation=operation, fade_menu=1)
        for confirm in (0, 1):
            add((), state=49, confirm=confirm, fade_menu=1)
    elif name.startswith("CardMgr_OnCardResult_"):
        cb = int(name.rsplit("_", 1)[1])
        targets = {
            0: [0x8013D878, 0x8013D89C, 0x8013D8C0, 0x8013D8E4, 0x8013D8EC, 0x8013D910],
            1: [0x8013D970, 0x8013D9B8, 0x8013D9C0, 0x8013D9E4],
            2: [0x8013DA44],
            3: [0x8013DAC0, 0x8013DAE4, 0x8013DB08, 0x8013DB2C],
            4: [0x8013DB8C, 0x8013DBB0, 0x8013DBD4],
            5: [0x8013DC34, 0x8013DC44, 0x8013DC54, 0x8013DC5C],
            6: [0x8013DCC0], 7: [0x8013DE34, 0x8013DE90, 0x8013DE98],
            8: [0x8013DEF8, 0x8013DF1C, 0x8013DF24],
        }
        if cb < 9:
            for idx, target in enumerate(targets[cb]):
                add((), operation=idx - 8, callback=cb, callback_target=target,
                    card_mode=10 if idx & 1 else 0, sync_status=idx & 1,
                    card_info=(idx + 1) & 1, state=39 if cb == 6 and idx & 1 else 37,
                    poll_timer=1 if cb == 6 else 0, ui_operation=idx & 1)
            add((), operation=2, callback=cb, callback_target=0, state=14)
            if cb == 1:
                add((), operation=0, callback=cb, callback_target=0x8013D970,
                    card_mode=10, sync_status=0, ui_busy=0)
            elif cb == 2:
                add((), operation=0, callback=cb, callback_target=0x8013DA44,
                    state=38)
            elif cb == 3:
                add((), operation=1, callback=cb, callback_target=0x8013DAE4,
                    sync_status=0, card_info=0, ui_busy=0)
            elif cb == 4:
                add((), operation=1, callback=cb, callback_target=0x8013DBB0,
                    sync_status=0, card_info=0, ui_busy=0)
            elif cb == 6:
                for state in (37, 38, 39):
                    for operation in (0, -4, -7, -2, -1, 1):
                        add((), operation=operation, callback=cb,
                            callback_target=0x8013DCC0, state=state,
                            poll_timer=1, sync_status=0, card_info=0,
                            ui_busy=0)
            elif cb == 7:
                add((), operation=0, callback=cb, callback_target=0x8013DE34,
                    sync_status=0, card_info=0, ui_busy=0, ui_operation=0)
            elif cb == 8:
                add((), operation=0, callback=cb, callback_target=0x8013DEF8,
                    ui_operation=0)
        else:
            for op in (-8, -7, -4, 0, 1):
                for retry in (0, 2):
                    add((), operation=op, retry=retry, sync_status=retry & 1,
                        card_info=(retry + 1) & 1, ui_operation=(op == -7))
            for op in (-2, -1):
                add((), operation=op, retry=2, sync_status=0, card_info=0)
            add((), operation=0, retry=2, sync_status=0, card_info=0, ui_busy=0)
            for op in (-7, -4, 1):
                add((), operation=op, retry=2, sync_status=0,
                    card_info=0, ui_busy=0)
            for op in (-7, -4, -2, -1, 1):
                add((), operation=0, retry=2, sync_status=0, card_info=0,
                    ui_busy=0, card_operation_after_memcard_op13=op)
    elif name in ("McxCardTypeSync", "McxAllInfoSync", "McxHideTransSync"):
        args_list = [()] if name != "McxHideTransSync" else [(v,) for v in (0, 1, 0xFFFFFFFC, 0xFFFFFFF9)]
        for args in args_list:
            for status in (0, 1, 2): add(args, sync_status=status, card_info=status - 1, ui_busy=0)
        if name in ("McxAllInfoSync", "McxHideTransSync"): add(args_list[0], ui_busy=1, sync_status=2)
    elif name == "CardMgr_ResetSlots":
        for reset in (0, 1):
            for busy in (0, 1): add((reset,), sync_status=busy, ui_busy=0)
    elif name == "CardMgr_MenuUpdate":
        for fade in (0, 1):
            for mode in (0, 0x1000, 0x4000):
                for buttons in (0, 0x20, 0x800, 0x820):
                    add((), fade_menu=fade, analog_mode=mode, buttons=buttons,
                        operation=rng.randrange(0, 2), ui_operation=rng.randrange(0, 2),
                        primary0=rng.randrange(-3, 9), primary_limit=rng.randrange(-3, 9))
        add((), fade_menu=1, analog_mode=0, buttons=0x20, ui_operation=0,
            primary0=3, primary_limit=7)
    elif name == "CardMgr_AnimTick":
        for edge in range(8): add((edge,), animation=edge)
    elif name == "CardMgr_Draw":
        for state in (18, 29, 30, 14, 19): add((), state=state)
    elif name == "CardMgr_RunMessage":
        for fade in ([-1], [0, -1], [0, 0, -1], [0, 0, 0, -1]):
            for answer in (0, 1): add((), fade_seq=fade, initial_s1=answer, confirm=answer,
                                      fade_menu=1, analog_mode=0, menu_choice=2 + answer)
        add((), fade_seq=[-1], initial_s1=-1, fade_menu=1)
    elif name == "CardMgr_DrawMessage":
        for choice in range(6):
            add((0,), menu_choice=choice, confirm=1, fade_menu=1,
                analog_mode=0, yesno=0)
        for mode in (0x1000, 0x4000):
            for choice in (2, 3, 4):
                add((0,), menu_choice=choice, confirm=1, fade_menu=1,
                    analog_mode=mode, yesno=choice - 2)
            add((0,), menu_choice=2, confirm=0, fade_menu=1,
                analog_mode=mode, yesno=0)
        add((0,), menu_choice=2, confirm=0, fade_menu=1,
            analog_mode=0, yesno=0)
        for choice in (2, 3, 4):
            add((0,), menu_choice=choice, confirm=0, fade_menu=0,
                analog_mode=0, yesno=choice - 2)
    elif name == "CardMgr_DrawMenuLabels":
        for menu in range(4): add((menu,))
        add((0xFFFFFFFF,))
    elif name == "CardMgr_DrawBlinkingMenu":
        for variant in (0, 1):
            for frame in (0, 9, 10, 11, 19, 20, 21):
                add((variant,), state_frame=frame)
    elif name == "SaveUi_DrawStateOverlay":
        for state in range(2, 50):
            for frame in (0, 10, 11):
                add((), state=state, state_frame=frame, card_variant=(state % 4) - 1)
        add((), state=1); add((), state=50)
    elif name == "CardMgr_DrawMenuItems":
        for selected in range(5): add((selected, 0x801FE800), menu_mask=5, enabled_count=5)
    elif name in ("CardMgr_DrawMenuTexts", "CardMgr_DrawMenuItems2"):
        for selected in range(8): add((selected, 0x801FE800) if name.endswith("Items2") else (0x801FE800,),
                                      menu_mask=7, enabled_count=7)
    elif name == "SaveUi_DrawStringPair": add(())
    elif name == "SaveUi_DrawYesNo":
        for selected in (0, 1, 2, 0xFFFFFFFF): add((selected,))
    elif name == "SaveUi_DrawFileHeader":
        for page in (0, 1, 2, 0xFFFFFFFF): add((page,))
    elif name == "SaveUi_DrawEmptySlotText":
        for slot in range(4): add((slot,))
    elif name == "SaveUi_ThreadSetHook":
        for _ in range(4): add((rng.getrandbits(32),), thread_id=rng.randrange(1, 8), thread_handle=2)
    elif name in ("SaveUi_ThreadClose", "McxStartCom"):
        for _ in range(4): add((), thread_id=rng.randrange(1, 8), thread_handle=2)
    elif name in ("SaveUi_Nop", "SaveUi_ThreadSetHookReset", "SaveUi_ThreadSwitch"):
        for state in (0, 1, 2, 0xFFFF): add((), thread_state=state)
    elif name == "MCXINIT_OBJ_B0":
        for linked in (0, 1, 2, 3): add((2, 0x801A0100), callback_linked=linked)
    elif name == "MCXINIT_OBJ_FC":
        add((0, 0x801A0100))
    elif name in ("MCXINIT_OBJ_104", "MCXINIT_OBJ_144"):
        for flags in (0, 1, 0x80, 0x81):
            for head in (0, 1, 0x80, 0xFFFFFFFF): add((), node_flags=flags, node_head=head)
    elif name == "MCXINIT_OBJ_184":
        for request in (0, 1, 2, 3):
            for mode, retry in ((17, 0), (17, 1), (17, 2), (0, 0)):
                add((), init_request=request, init_mode=mode, init_retry=retry,
                    init_result=0, init_argument=0x1234)
    elif name == "MCXINIT_OBJ_2BC":
        for result in (0, 1, 2, 3): add((), init_result=result)
    else:
        add(())
    return cases


class Harness:
    def __init__(self, dll):
        self.c = CPort(dll)
        self.areas = dump_areas.area_members()
        self.area_ids = [a for a in (1, 2, 5) if a in self.areas]
        if not self.area_ids: raise SystemExit("real area members 1, 2, or 5 are required")
        self.cpus, self.snaps = {}, {}
        self.coverage, self.counts, self.failures = set(), Counter(), 0
        self.verbose = bool(os.environ.get("OVL186C_DEBUG"))
        reg.declare("stack_window", "801FF000..80200000")
        reg.declare("ram_mask", f"{WORK:08X}:{WORK_SIZE}")
        reg.declare("shared_random", "CardMgr_RunMessage initial $s1")
        for area in self.area_ids:
            cpu = mr.MipsRam.from_extract(ROOT / f"extract/members/{self.areas[area]:03X}.bin")
            cpu.load_area((ROOT / "extract/members/186.bin").read_bytes(), OVERLAY_BASE)
            cpu.coverage = self.coverage
            self.cpus[area] = cpu
            self.snaps[area] = cpu.snapshot()

    def setup(self, name, args, p, mem):
        p = dict(p)
        # Initialize safe, deterministic backing records. They live outside the stack window.
        context, node, objects, cbroot, cbarray = 0x80190000, 0x80191000, 0x80192000, 0x80193000, 0x80194000
        mem.wr32(0x800E388C, context)
        mem.wr32(0x8014200C, node)
        mem.wr32(0x80142008, objects)
        mem.wr32(0x80142010, cbroot)
        mem.wr32(cbroot, cbarray)
        mem.wr32(0x80142014, 0x80195000)
        mem.wr32(0x80142024, 0x80195100)
        mem.wr32(0x80141B70, 0x80195200)
        mem.wr32(0x80141B88, 2)
        mem.wr32(0x80141B80, 0x80195300)
        mem.wr32(0x80141B84, 0x80195304)
        mem.wr32(0x80195300, 0x80195500)
        mem.wr32(0x80195500, 0x80195555)
        mem.wr32(0x80195210, 0x80196000)
        mem.wr32(0x80196000 + 16 + 2 * 0xC0 + 0x94, 0)
        # Typical context, menu-entry and animation values.
        for i in range(8): mem.wr32(context + 0x110 + i * 4, 0x22000000 + i * 0x10101)
        for i in range(12):
            mem.wr16(0x8013F5E4 + i * 2, (i * 7 + 3) % 101)
            mem.wr16(0x8013F5FC + i * 2, (i * 3 + 2) % 101)
        for i in range(32):
            base = 0x80141478 + i * 36
            if base + 40 < 0x8014F000:
                mem.wr16(base, 24 + i % 9 * 8)
                mem.wr16(base + 2, 40 + i % 7 * 12)
                mem.wr32(base + 4, 0x80180000 + (i % 8) * 0x100)
        mem.write_bytes(0x80180000, b"SAVE DATA\0EMPTY SLOT\0YES\0NO\0PAGE\0")
        for ch in b"SAVE DATAEMPTY SLOTYESNOPAGE": mem.wr8(0x8013B024 + ch, 8)
        mem.wr32(0x800EC350, 3)
        if "state_frame" in p: mem.wr32(0x800EC350, p["state_frame"])
        if "card_variant" in p: mem.wr16(0x800E3886, p["card_variant"])
        # Valid native display pointers for routines that make renderer calls.
        mem.wr32(0x800EC354, 0x80197000)
        mem.wr32(0x800EC35C, 0)
        # Default settings / globals.
        mem.wr16(0x800E3884, p.get("card_mode", 0))
        mem.wr16(0x800E3880, p.get("operation", 0))
        mem.wr16(0x800E3888, 0)
        mem.wr16(0x8013AF56, p.get("state", 18))
        mem.wr16(0x8013AF5A, 0)
        mem.wr16(0x8013AF66, p.get("fade_menu", 0))
        mem.wr16(0x8013AF68, 0)
        mem.wr32(0x8013F280, 0)
        mem.wr32(0x8013F284, p.get("ui_busy", 0))
        mem.wr32(0x8013F288, p.get("ui_operation", 0))
        mem.wr32(0x8013F28C, p.get("retry", 0))
        mem.wr32(0x8013F290, p.get("poll_timer", 0))
        mem.wr32(0x8013F294, p.get("timeout", 0))
        mem.wr32(0x8013FB20, p.get("menu_choice", 2))
        mem.wr32(0x800E5488, p.get("analog_mode", 0))
        mem.wr32(0x800E5478, p.get("buttons", 0))
        mem.wr32(0x800E5480, 0xFFFFFFFF)
        mem.wr16(0x8013F5E4, p.get("primary0", 6))
        mem.wr16(0x8013F5F0, p.get("primary_limit", 20))
        mem.wr32(0x8013F454 + 12, 2)
        mem.wr16(0x8013F55C + 8, 0)
        # Per-case optional setup values.
        if "menu_mask" in p:
            for i in range(8): mem.wr16(args[-1] + i * 2, 1 if p["menu_mask"] & (1 << i) else 0)
        if "animation" in p:
            for i in range(4):
                rec = 0x8013F55C + i * 14
                vals = [i, i % 3, 3, 2, (p["animation"] + i) % 5,
                        (p["animation"] % 3), 0x20 + i]
                for j, value in enumerate(vals): mem.wr16(rec + 2 * j, value)
        if "callback" in p:
            cb, target = p["callback"], p["callback_target"]
            op = p.get("operation", -8)
            idx = (op + 8) & 0xFFFF
            if idx < 10: mem.wr32(0x8013F118 + cb * 0x28 + idx * 4, target)
        if "callback_linked" in p:
            entry = cbarray + args[0] * 8
            if p["callback_linked"] == 0:
                mem.wr32(entry, 0)
            else:
                first = 0x801A0200
                mem.wr32(entry, first)
                for i in range(p["callback_linked"]):
                    cur = first + i * 8
                    mem.wr32(cur, first + (i + 1) * 8 if i + 1 < p["callback_linked"] else 0)
                mem.wr32(args[1], 0xABCDEF01)
        if "node_flags" in p:
            mem.wr32(node, p.get("node_head", 0))
            mem.wr32(node + 4, p["node_flags"])
        if "thread_state" in p:
            mem.wr16(0x80141B8C, p["thread_state"])
        if "init_request" in p:
            mem.wr32(0x801450CC, p["init_request"])
            mem.wr32(0x801450C8, p.get("init_result", 0))
            mem.wr32(0x801450D0, p.get("init_argument", 0))
            mem.wr32(0x801450D8, p.get("init_retry", 0))
            mem.wr32(0x80145270, p.get("init_mode", 0))
        if "init_result" in p:
            mem.wr32(0x801450C8, p["init_result"])
        if "primary_values" in p:
            for i, value in enumerate(p["primary_values"]):
                mem.wr16(0x8013F5E4 + i * 2, value)
        if "secondary_values" in p:
            for i, value in enumerate(p["secondary_values"]):
                mem.wr16(0x8013F5FC + i * 2, value)
        if name == "CardMgr_DrawMenuItems":
            args = (args[0], 0x801FE800)
        elif name == "CardMgr_DrawMenuTexts":
            args = (0x801FE800,)
        elif name == "CardMgr_DrawMenuItems2":
            args = (args[0], 0x801FE800)
        if name == "CardMgr_RunMessage":
            # FadeTick exits after the requested number of frames; keep it finite.
            p.setdefault("fade_seq", [-1])
        return tuple(args), p

    def mips_hooks(self, model):
        hooks = {}
        for addr, (name, arity) in (API | OVERLAY_HOOKS).items():
            def invoke(cpu, regs, label=name, n=arity, mdl=model):
                args = list(regs[4:8])[:n]
                if n > 4: args.extend(cpu.read32(regs[29] + 16 + 4 * i) for i in range(n - 4))
                return mdl.call(label, addr_for(label, API, OVERLAY_HOOKS), args)
            hooks[addr] = invoke
        # Native-only entry points from this range are replaced as renderer work.
        def render_event(name, args): return model.render(name, args)
        def patch_native(addr, cpu, regs):
            state = cpu.read16(0x8013AF56)
            choice = cpu.read32(0x8013FB20)
            if addr == 0x8013E438:
                render_event("CardMgr_DrawSelectionBars", [0x8013F29C, 0x8013F55C, state, choice])
                regs[4] = 1
                return 0x8013EB14
            if addr == 0x8013E604:
                render_event("CardMgr_DrawFileList", [0x8013F454, 0x8013F594, 0x8013F5E4, 0x8013F5FC, state])
            elif addr == 0x8013EBD0:
                render_event("CardMgr_DrawIconAnim", [0x8013F29C, 0x8013F55C, state])
            elif addr == 0x8013ED0C:
                sync = model.call("VSync", 0x8002E520, [M])
                render_event("CardMgr_DrawProgressBar", [sync, state, cpu.read32(0x8013F288), 0x8013F298])
            elif addr == 0x8013F9F8:
                render_event("CardMgr_DrawMessageLines", [regs[4], choice, 0x8013F454, state])
            return regs[31]
        patches = {addr: (lambda cpu, regs, a=addr: patch_native(a, cpu, regs)) for addr in RENDERER_NAMES}
        # DrawMessage's inline Gouraud background is a native packet-building phase.
        def message_frame(cpu, regs):
            render_event("CardMgr_DrawMessageFrame", [cpu.read32(0x8013FB20), 0x8013F454,
                                                        0x8013F594, cpu.read16(0x8013AF56)])
            render_event("CardMgr_DrawMessageLines", [cpu.read32(0x8013FB20) - 2, cpu.read32(0x8013FB20),
                                                        0x8013F454, cpu.read16(0x8013AF56)])
            model.call("SaveUi_DrawMainWindow", 0x80139A3C, [4])
            model.call("SaveUi_DrawBackground3D", 0x80139A68, [])
            return 0x8013F8D8
        patches[0x8013F848] = message_frame
        # RunMessage's saved s1 is a caller-owned value before the first draw.
        # This named hook gives the same value to the readable C implementation.
        def initial_s1(cpu, regs):
            model.call("CardMgr_RunMessage_InitialS1", 0x8013F630, [])
            regs[17] = model.profile.get("initial_s1", 0)
            # Original first instruction is addiu sp,sp,-56.
            regs[29] = (regs[29] - 56) & M
            cpu.coverage.add(0x8013F630)
            return 0x8013F634
        patches[0x8013F630] = initial_s1
        return hooks, patches

    def run(self, name, scenario, selector=0, source_mutant=False):
        args, raw_profile = data_cases(name)[scenario % len(data_cases(name))]
        area_id = self.area_ids[scenario % len(self.area_ids)]
        cpu = self.cpus[area_id]
        cpu.restore(self.snaps[area_id]); cpu.coverage = self.coverage
        mm = Mem(cpu=cpu)
        args, profile = self.setup(name, args, raw_profile, mm)
        mm.wr32(MUTATION_SELECTOR, selector)
        mm.wr32(MUTATION_MARKER, 0)
        before = cpu.snapshot()
        cm = Model(self.c.mem, profile)
        mm_model = Model(mm, profile)
        self.c.sync(before)
        # Repeat setup on C after syncing the exact same real RAM image.
        ca = self.c.mem
        cargs, cprofile = self.setup(name, args, raw_profile, ca)
        if cargs != args or cprofile != profile:
            self.fail(name, scenario, "C setup differs from MIPS setup")
            return False
        ca.wr32(MUTATION_SELECTOR, selector)
        ca.wr32(MUTATION_MARKER, 0)
        hooks, patches = self.mips_hooks(mm_model)
        addr, _size, _sym, ret, _kinds = FUNCS[name]
        try:
            if name == "MCXINIT_OBJ_2BC":
                # This is a tail-call body whose shared epilogue consumes the caller's saved $ra.
                mm.wr32(mr.DEFAULT_SP + 20, mr.SENTINEL)
            expected = cpu.call(addr, args, hooks=hooks, patches=patches,
                                limit=LIMIT, initial_regs={17: profile.get("initial_s1", 0)})
        except Exception as exc:  # noqa: BLE001
            self.fail(name, scenario, f"MIPS failed: {exc}; machine={getattr(cpu, '_last_machine', None)}")
            return False
        try:
            actual = self.c.call(name, args, cm)
        except Exception as exc:  # noqa: BLE001
            self.fail(name, scenario, f"C call failed: {exc}")
            return False
        self.counts[name] += 1
        if self.verbose:
            print(f"DEBUG {name}[{scenario}] state C={self.c.mem.rd16(0x8013AF56):04X} "
                  f"MIPS={cpu.read16(0x8013AF56):04X} hooks C={cm.events!r} MIPS={mm_model.events!r} "
                  f"coverage={sorted(pc for pc in self.coverage if addr <= pc < addr + FUNCS[name][1])!r}",
                  flush=True)
        if ret != "u" and actual != expected[2]:
            self.fail(name, scenario, f"return C={actual:08X} MIPS={expected[2]:08X}")
            return False
        # Work arguments in the C adapter replace MIPS stack temporaries; stack and
        # call-work are the only declared RAM exclusions for this initial checker.
        self_mask = [(WORK - 0x80000000, WORK_SIZE)]
        reg.check_ram_mask(self_mask)
        limit = STACK_LO - 0x80000000
        got = memoryview(self.c.main).cast("B")[:limit]
        want = memoryview(cpu.ram)[:limit]
        work_start, work_size = self_mask[0]
        spans = ((0, work_start), (work_start + work_size, limit))
        mismatch = next(((start, end) for start, end in spans
                         if got[start:end] != want[start:end]), None)
        if mismatch is not None:
            start, end = mismatch
            off = next(i for i in range(start, end) if got[i] != want[i])
            self.fail(name, scenario, f"RAM differs at {0x80000000+off:08X}: C={got[off:off+8].hex()} MIPS={want[off:off+8].hex()}")
            if self.verbose:
                print("  event tail C=", cm.events[-12:], "MIPS=", mm_model.events[-12:], flush=True)
            return False
        if self.c.error():
            self.fail(name, scenario, f"AeRam error: {self.c.error()}")
            return False
        if bytes(self.c.scratch) != bytes(cpu.scratch):
            self.fail(name, scenario, "scratchpad differs")
            return False
        if cm.events != mm_model.events:
            i = next((i for i, (a, b) in enumerate(zip(cm.events, mm_model.events)) if a != b),
                     min(len(cm.events), len(mm_model.events)))
            self.fail(name, scenario, f"hook sequence differs at {i}: C={cm.events[i:i+3]!r} MIPS={mm_model.events[i:i+3]!r} ({len(cm.events)}/{len(mm_model.events)})")
            if self.verbose: print("  C=", cm.events, "\n  MIPS=", mm_model.events, flush=True)
            return False
        return True

    def fail(self, name, scenario, reason):
        self.failures += 1
        print(f"FAIL {name}[{scenario}]: {reason}", flush=True)

    def all(self, scale=4, names=None):
        selected = list(names) if names else list(FUNCS)
        passed = True
        for name in selected:
            cases = data_cases(name, scale)
            for i in range(len(cases)):
                passed = self.run(name, i) and passed
            print(f"{name}: {len(cases)} cases", flush=True)
        return passed

    def finish(self, extra_coverage, default_run=False, show_coverage=False, names=None):
        selected = list(names) if names else list(FUNCS)
        items = [(n, FUNCS[n][0], FUNCS[n][1], True) for n in selected]
        items.extend((n, addr, size, False) for n, addr, size in extra_coverage)
        summary = reg.coverage_summary(items, self.coverage)
        if show_coverage: reg.print_coverage(summary)
        gaps = {n: row for n, row in summary.items() if row["uncovered_count"] or row["stale_declared"]}
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps,
                              extra={"overlay": "186.bin", "areas": self.area_ids}, function_names=selected)
        for n, row in gaps.items(): print(f"COVERAGE GAP {n}: {row['uncovered']}; stale={row['stale_declared']}", flush=True)
        return not gaps and not self.failures


def addr_for(label, *maps):
    for mapping in maps:
        for addr, pair in mapping.items():
            if pair[0] == label: return addr
    return 0


def instrument_mutations(source):
    mutated = source
    for index, (_name, (_addr, _size, symbol, _ret, _args)) in enumerate(FUNCS.items(), 1):
        pattern = re.compile(r"(\b(?:int32_t|uint32_t)\s+" + re.escape(symbol) + r"\s*\([^{}]*?\)\s*\{)", re.S)
        lines = (f"\n    if (ae_rd32(ram, 0x{MUTATION_SELECTOR:08X}u) == {index*2-1}u) "
                 f"ae_wr32(ram, 0x{MUTATION_MARKER:08X}u, 0x186C0001u);\n"
                 f"    if (ae_rd32(ram, 0x{MUTATION_SELECTOR:08X}u) == {index*2}u) "
                 f"ae_wr32(ram, 0x{MUTATION_MARKER:08X}u, 0x186C0002u);\n")
        mutated, n = pattern.subn(lambda m: m.group(1) + lines, mutated, count=1)
        if n != 1: raise SystemExit(f"cannot instrument {symbol}")
    return mutated


def run_mutations():
    src = ROOT / "src/modes/ae_ovl186c.c"
    mutant = ROOT / "extract/mips/ae_ovl186c_mutant.c"
    mutant.write_text(instrument_mutations(src.read_text(encoding="utf-8")), encoding="utf-8")
    try:
        h = Harness(build(mutant, "ovl186c_mutant"))
        caught = 0
        for index, name in enumerate(FUNCS, 1):
            for variant in (1, 2):
                before = h.failures
                ok = h.run(name, 0, selector=index * 2 - 2 + variant, source_mutant=True)
                if not ok and h.failures > before: caught += 1
                else: print(f"MUTATION SURVIVED {name}/{variant}", flush=True)
        total = len(FUNCS) * 2
        print(f"mutations caught: {caught}/{total}", flush=True)
        return 0 if caught == total else 1
    finally:
        mutant.unlink(missing_ok=True)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--cases", type=int, default=4)
    p.add_argument("--coverage", action="store_true")
    p.add_argument("--mutate", action="store_true")
    p.add_argument("--function", action="append", choices=FUNCS)
    a = p.parse_args()
    if a.mutate: return run_mutations()
    h = Harness(build())
    passed = h.all(a.cases, a.function)
    coverage = h.finish(EXTRA_COVERAGE, default_run=a.cases == 4 and not a.function,
                         show_coverage=a.coverage, names=a.function)
    total = sum(h.counts.values())
    print(f"differential: {total} cases; {'PASS' if passed and coverage else 'FAIL'}", flush=True)
    return 0 if passed and coverage else 1


if __name__ == "__main__": sys.exit(main())
