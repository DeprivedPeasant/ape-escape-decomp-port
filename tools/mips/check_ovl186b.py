"""Differential checks for game logic in save-menu overlay member 186."""
import argparse
import copy
import ctypes as C
import math
import os
from pathlib import Path
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas
import exclusion_registry as reg
import mips_ram as mr

OVERLAY = ROOT / "extract/members/186.bin"
OVERLAY_BASE = 0x80136000
STACK_LO = mr.STACK_WINDOW[0] - 0x80000000
MUTATION_SELECTOR = 0x801FE100
MUTATION_TARGETS = (0x801FE104, 0x801FE108)

PAD_STATE = 0x800E5320
PAD_HELD = 0x800E5478
PAD_CHANGED = 0x800E5480
PAD_REPEAT = 0x800E5488
PAD_DEMO = 0x800A8A58
PAD_BUF1 = 0x800E52D0
PAD_BUF2 = 0x800E52F2
PAD_STICK_DONE = 0x800E53C4
SAVE_UI_DATA = 0x800E3880
SAVE_SLOT_BASE = 0x800E1880
SAVE_SLOT_STRIDE = 0x7A0
SAVE_SLOT_NAME = 0x198
SAVE_SLOT_USED = 0x1A8
SAVE_ENTRY_TYPE = 0x180
SAVE_SELECTION = 0x8013AF52
SAVE_CONFIRM_MODE = 0x8013AF54
SAVE_FADE_STATE = 0x8013AF66
SAVE_FADE_TIMER = 0x8013AF68
SAVE_CHOICE = 0x8013AF5E
SAVE_NAME_LENGTH = 0x8013AF50
SAVE_NAME_BUFFER = 0x8013C5C0
SAVE_NAME_GRID = 0x8013C540
SAVE_NAME_CURSOR_X = 0x8013C5A8
SAVE_NAME_CURSOR_Y = 0x8013C5AA
SAVE_NAME_PAGE = 0x8013C5AC
SAVE_NAME_TABLE_OFFSET = 0x8013C5AE
SAVE_NAME_CELL_COUNT = 0x8013C5B0
SAVE_NAME_COLUMNS = 0x8013C5B2
SAVE_NAME_TIMERS = 0x8013C5D0
SAVE_UI_CARD_AVAILABLE = 0x8013AF64
FILE_SELECT_OPTIONS = 0x8013AFE8
CARDMGR_STATE = 0x8013F280
CARDMGR_TIMER = 0x8013F288
DISPLAY_STATE = 0x800EC350
DISPLAY_INDEX = DISPLAY_STATE + 0x0C
CARDMGR_DISPLAY = 0x800F4350
CARDMGR_STATS = 0x8014F5E4
CARDMGR_HEADER = 0x800F5120
MODEL_POINTER = 0x8014B214
MODEL_ANGLE = 0x8013AF78
MODEL_DRAW_FIRST = 0x8013AF6C
MENU_RENDER_STATE = 0x800F4470
NAME_BUFFER_HOOK_RESULT = 0x80141800
CARD_RESULT_TABLE = 0x8013C4E0
SCRATCH_TEST_BASE = 0x801D8000

FUNCS = {
    "SaveUi_DrawDetail": (0x801391B0, 700, "v"),
    "SaveUi_LastUsedEntry": (0x801399E4, 88, "s"),
    "SaveUi_DrawMainWindow": (0x80139A3C, 44, "s"),
    "SaveUi_DrawBackground3D": (0x80139A68, 648, "v"),
    "SaveUi_YesNoUpdate": (0x80139F08, 192, "s"),
    "SaveUi_FileSelectUpdate": (0x80139FC8, 1080, "s"),
    "SaveUi_ConfirmPressed": (0x8013A400, 60, "s"),
    "SaveUi_CancelPressed": (0x8013A43C, 60, "s"),
    "SaveUi_FadeTick": (0x8013A634, 216, "s"),
    "SaveUi_OnCardResult_7": (0x8013B400, 124, "s"),
    "SaveUi_ConfirmDialogUpdate": (0x8013B47C, 212, "s"),
    "SaveUi_NameEntryInit": (0x8013B550, 372, "s"),
    "SaveUi_NameEntryUpdate": (0x8013B6C4, 1440, "s"),
    "SaveUi_NameEntryDraw": (0x8013BC64, 2168, "v"),
    "CardMgr_Run": (0x8013C7F0, 248, "s"),
    "CardMgr_Init": (0x8013C8E8, 108, "s"),
    "CardMgr_RunFileList": (0x8013C954, 476, "s"),
    "CardMgr_RunFileDetail": (0x8013CB30, 492, "s"),
}

DEFAULT_CASES = {
    "SaveUi_DrawDetail": 4,
    "SaveUi_LastUsedEntry": 4,
    "SaveUi_DrawMainWindow": 1,
    "SaveUi_DrawBackground3D": 4,
    "SaveUi_YesNoUpdate": 5,
    "SaveUi_FileSelectUpdate": 21,
    "SaveUi_ConfirmPressed": 4,
    "SaveUi_CancelPressed": 4,
    "SaveUi_FadeTick": 7,
    "SaveUi_OnCardResult_7": 10,
    "SaveUi_ConfirmDialogUpdate": 3,
    "SaveUi_NameEntryInit": 3,
    "SaveUi_NameEntryUpdate": 18,
    "SaveUi_NameEntryDraw": 5,
    "CardMgr_Run": 3,
    "CardMgr_Init": 2,
    "CardMgr_RunFileList": 2,
    "CardMgr_RunFileDetail": 2,
}

HOOKS = {
    # Native platform, card I/O, audio and display calls.
    0x800877F8: ("PlaySfx", 1),
    0x80010000: ("MemCardOp", 2),
    0x8005CFEC: ("InitDisplay", 0),
    0x80028350: ("ResetGraph", 1),
    0x8005DF48: ("BeginFrame", 0),
    0x800286CC: ("DrawSync", 1),
    0x8002E520: ("VSync", 1),
    0x80028850: ("ClearImage", 4),
    0x80028EFC: ("PutDispEnv", 1),
    0x80028D30: ("PutDrawEnv", 1),
    0x80028CC0: ("DrawOTag", 1),
    0x8002FAF0: ("SetGeomScreen", 1),
    0x8008211C: ("DrawMenuArrows", 3),
    0x800160EC: ("MeshTransform", 3),
    0x8001A414: ("ModelEmitPrimsAlt", 3),
    # Earlier/later save-menu overlay ranges stay named calls.
    0x80136160: ("SaveUi_DrawWindow", 4),
    0x80136000: ("SaveUi_DrawText", 4),
    0x80138DC0: ("SaveUi_DrawSlot", 2),
    0x801368E8: ("SaveUi_LookAt", 2),
    0x8013FFEC: ("SaveUi_DrawYesNo", 1),
    0x8013784C: ("SaveUi_SetState", 2),
    0x8013FD44: ("SaveUi_GetBuffer2", 0),
    0x8013FD50: ("CardMgr_DrawMenuTexts", 1),
    0x8014016C: ("SaveUi_DrawStateOverlay", 0),
    0x8013D19C: ("CardMgr_StartOps", 0),
    0x8013F630: ("CardMgr_RunMessage", 0),
    0x8013D1F8: ("CardMgr_IsBusy", 0),
    0x8013D218: ("CardMgr_IsBusy", 0),
    0x80141A10: ("SaveUi_ThreadSetHook", 1),
    0x80141AB4: ("SaveUi_ThreadClose", 0),
    0x80141B00: ("SaveUi_ThreadSwitch", 0),
    0x800962B0: ("ProgressChecksum", 1),
    # Native-only renderer functions called from game logic.
    0x80139570: ("SaveUi_DrawPortrait", 2),
    0x80139CF0: ("SaveUi_DrawCursorAnim", 0),
    0x8013A478: ("SaveUi_DrawFileIcons", 0),
    0x8013A70C: ("SaveUi_FadeDraw", 0),
    0x8013A804: ("SaveUi_DrawModel", 4),
    0x8013946C: ("SaveUi_DrawIcon", 3),
    # Pad driver boundaries reached by the real PadUpdate port.
    0x8002B52C: ("PadGetState", 1),
    0x8002B5F8: ("PadInfoMode", 3),
    0x80012CA0: ("PadSetupDualShock", 1),
    0x80013128: ("PadDemoPlaybackStep", 0),
    0x8005D100: ("StageRequest", 3),
    0x800140B4: ("StickToButtons", 0),
    0x80014208: ("PadRumbleReset", 1),
    0x8001325C: ("PadUpdate", 1),
}

EXTRA_COVERAGE = {}


def build(source=None, tag="ovl186b"):
    import check_game as cg
    extra='tools/mips/ovl186b_test_api.c'
    cg.SOURCES.append(extra)
    try:
        overrides={'src/modes/ae_ovl186b.c':source} if source else None
        return cg.build_dll(overrides,tag)
    finally:
        cg.SOURCES.remove(extra)


class CMemory:
    def __init__(self, side):
        self.side = side

    def read(self, address, size):
        offset = address - 0x80000000
        return int.from_bytes(bytes(self.side.main[offset:offset + size]), "little")

    def write(self, address, value, size):
        offset = address - 0x80000000
        mask = (1 << (size * 8)) - 1
        self.side.main[offset:offset + size] = (value & mask).to_bytes(size, "little")


class HookModel:
    def __init__(self, memory, context=None):
        self.memory = memory
        self.context = copy.deepcopy(context or {})
        self.events = []
        self.busy_calls = 0
        self.message_calls = 0
        self.icon_calls = 0

    @staticmethod
    def s16(value):
        value &= 0xFFFF
        return value - 0x10000 if value & 0x8000 else value

    def call(self, name, args=()):
        args = tuple(value & 0xFFFFFFFF for value in args)
        self.events.append((name, args))
        if name == "SaveUi_GetBuffer2":
            return int(self.context.get("name_buffer", NAME_BUFFER_HOOK_RESULT))
        if name == "ProgressChecksum":
            return int(self.context.get("checksum", 0x5A6B))
        if name == "CardMgr_RunMessage":
            index = self.message_calls
            self.message_calls += 1
            destination_state = self.context.get("run_message_state")
            if index == 0 and destination_state is not None:
                if isinstance(self.memory, CMemory):
                    self.memory.write(CARDMGR_STATE, destination_state, 4)
                else:
                    self.memory.write(CARDMGR_STATE, 4, destination_state)
            results = self.context.get("run_message_results", [-1])
            if index < len(results):
                return int(results[index])
            return int(results[-1]) if results else -1
        if name == "CardMgr_IsBusy":
            index = self.busy_calls
            self.busy_calls += 1
            busy = self.context.get("busy", 1)
            if isinstance(busy, (list, tuple)):
                return int(busy[min(index, len(busy) - 1)]) if busy else 0
            return int(busy)
        if name == "PadGetState":
            return int(self.context.get("pad_state", 1))
        if name == "PadInfoMode":
            return int(self.context.get("pad_info_mode", 1))
        if name == "PadDemoPlaybackStep":
            return int(self.context.get("pad_demo_step", 0))
        if name == "StickToButtons":
            return int(self.context.get("stick_buttons", 0))
        if name == "SaveUi_DrawIcon":
            cursor = self.memory.read(DISPLAY_STATE + 4, 4)
            # The mixed caller emits the first icon packet header and advances
            # its cursor by 28 bytes before entering DrawIcon. That setup is
            # inline in the MIPS function; the C hook owns it on the C side.
            if self.icon_calls == 0 and isinstance(self.memory, CMemory):
                cursor = (cursor + 28) & 0xFFFFFFFF
            cursor = (cursor + 20) & 0xFFFFFFFF
            if isinstance(self.memory, CMemory):
                self.memory.write(DISPLAY_STATE + 4, cursor, 4)
            else:
                self.memory.write(DISPLAY_STATE + 4, 4, cursor)
            self.icon_calls += 1
        return int(self.context.get("returns", {}).get(name, 0))


class CSide:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        P, U, I32 = C.c_void_p, C.c_uint32, C.c_int32
        self.CallFn = C.CFUNCTYPE(I32, P, P, C.c_char_p, U, U, C.POINTER(U))
        self.PadGet = C.CFUNCTYPE(I32, P, P, I32)
        self.PadInfo = C.CFUNCTYPE(I32, P, P, I32, I32, I32)
        self.PadSetup = C.CFUNCTYPE(None, P, P, I32)
        self.PadDemo = C.CFUNCTYPE(I32, P, P)
        self.StageRequest = C.CFUNCTYPE(None, P, P, I32, I32, I32)
        self.StickButtons = C.CFUNCTYPE(U, P, P)
        self.PadRumble = C.CFUNCTYPE(None, P, P, I32)
        self.PadHooks = type("AePadHooks", (C.Structure,), {
            "_fields_": [
                ("user", P), ("pad_get_state", self.PadGet),
                ("pad_info_mode", self.PadInfo),
                ("pad_setup_dualshock", self.PadSetup),
                ("pad_demo_step", self.PadDemo),
                ("stage_request", self.StageRequest),
                ("stick_to_buttons", self.StickButtons),
                ("pad_rumble_reset", self.PadRumble),
            ]
        })
        self.OverlayHooks = type("AeOvl186bHooks", (C.Structure,), {
            "_fields_": [
                ("user", P), ("stack_pointer", U), ("caller_a1", U),
                ("caller_a2", U), ("caller_s0", U), ("pad", P),
                ("overlay_call", self.CallFn), ("native_call", self.CallFn),
                ("renderer", self.CallFn),
            ]
        })
        self.lib.ae_ram_create.restype = P
        self.lib.ae_ram_destroy.argtypes = [P]
        self.lib.ae_ptr.argtypes = [P, U, U]
        self.lib.ae_ptr.restype = P
        self.lib.ae_ram_failed.argtypes = [P]
        self.lib.ae_ram_failed.restype = C.c_int
        self.lib.ae_ram_error.argtypes = [P]
        self.lib.ae_ram_error.restype = C.c_char_p
        self.lib.ae_ram_clear_error.argtypes = [P]
        self.lib.o186_call.argtypes = [P, C.POINTER(self.OverlayHooks), U, U, U, U, U]
        self.lib.o186_call.restype = I32
        self.lib.ae_pad_PadUpdate.argtypes = [P, P, U]
        self.ram = self.lib.ae_ram_create()
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(
            self.lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(
            self.lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.callbacks = []
        self.hooks = self.OverlayHooks()
        self.pad_hooks = self.PadHooks()
        self.model = None

    def sync(self, cpu):
        C.memmove(self.main, bytes(cpu.ram), mr.RAM_SIZE)
        C.memmove(self.scratch, bytes(cpu.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)

    def make_pad_hooks(self, model):
        pad_hooks = self.PadHooks()
        callbacks = []
        def add(field, callback_type, name, arg_builder, returns=True):
            def invoke(*values):
                # C pad callbacks receive user and AeRam before function arguments.
                args = arg_builder(*values[2:])
                result = model.call(name, args)
                return result if returns else None
            callback = callback_type(invoke)
            callbacks.append(callback)
            setattr(pad_hooks, field, callback)

        add("pad_get_state", self.PadGet, "PadGetState", lambda port: (port,))
        add("pad_info_mode", self.PadInfo, "PadInfoMode", lambda a, b, c: (a, b, c))
        add("pad_setup_dualshock", self.PadSetup, "PadSetupDualShock", lambda arg: (arg,), False)
        add("pad_demo_step", self.PadDemo, "PadDemoPlaybackStep", lambda: ())
        add("stage_request", self.StageRequest, "StageRequest", lambda a, b, c: (a, b, c), False)
        add("stick_to_buttons", self.StickButtons, "StickToButtons", lambda: ())
        add("pad_rumble_reset", self.PadRumble, "PadRumbleReset", lambda port: (port,), False)
        return pad_hooks, callbacks

    def configure(self, model):
        self.model = model
        self.callbacks = []

        def call(_user, _ram, raw_name, _address, argc, argv):
            name = raw_name.decode("ascii")
            args = tuple(argv[index] for index in range(argc)) if argc else ()
            return model.call(name, args)

        cb = self.CallFn(call)
        self.callbacks.append(cb)
        self.pad_hooks, pad_callbacks = self.make_pad_hooks(model)
        self.callbacks.extend(pad_callbacks)

        self.hooks = self.OverlayHooks()
        self.hooks.stack_pointer = mr.DEFAULT_SP
        self.hooks.caller_s0 = model.context.get("caller_s0", 0)
        self.hooks.pad = C.cast(C.pointer(self.pad_hooks), C.c_void_p)
        for field in ("overlay_call", "native_call", "renderer"):
            setattr(self.hooks, field, cb)

    def invoke(self, name, args=()):
        address, _size, _ret = FUNCS[name]
        argv = list(args) + [0] * (4 - len(args))
        result = self.lib.o186_call(self.ram, C.byref(self.hooks), address, *argv)
        if self.lib.ae_ram_failed(self.ram):
            error = self.lib.ae_ram_error(self.ram).decode(errors="replace")
            raise AssertionError("C AeRam failure: " + error)
        return int(result)


def slot_record(slot):
    return SAVE_SLOT_BASE + slot * SAVE_SLOT_STRIDE


def set_name_page_state(cpu, page):
    page_state = {
        0: (0, 90, 15),
        1: (90, 90, 15),
        2: (180, 44, 11),
        3: (44, 44, 11),
        4: (0, 44, 11),
    }
    table_offset, cell_count, columns = page_state[page]
    cpu.write16(SAVE_NAME_PAGE, page)
    cpu.write16(SAVE_NAME_TABLE_OFFSET, table_offset)
    cpu.write16(SAVE_NAME_CELL_COUNT, cell_count)
    cpu.write16(SAVE_NAME_COLUMNS, columns)
    cpu.write16(SAVE_NAME_CURSOR_X, min(2, cell_count - 1))
    cpu.write16(SAVE_NAME_CURSOR_Y, 2)


def base_setup(cpu, area_id, member, name, seed=186):
    # Several real area images are retained under the EXE and member 186.  These
    # functions rarely touch area records, but the full image is still present.
    cpu.write32(DISPLAY_INDEX, 0)
    # The model pointer is an overlay-owned runtime global. Preserve the real
    # image value unless a scenario explicitly supplies a valid model object.
    cpu.write16(MENU_RENDER_STATE, 0)
    cpu.write16(SAVE_FADE_STATE, 1)
    cpu.write16(SAVE_FADE_TIMER, 0)
    cpu.write16(SAVE_SELECTION, 1)
    cpu.write16(SAVE_CONFIRM_MODE, 0)
    cpu.write16(SAVE_CHOICE, 0)
    cpu.write16(SAVE_NAME_LENGTH, 4)
    cpu.write16(SAVE_UI_DATA + 6, 1)
    cpu.write16(SAVE_UI_DATA, 0)
    cpu.write16(SAVE_UI_DATA + 2, 1)
    cpu.write16(SAVE_UI_DATA + 4, 2)
    cpu.write16(SAVE_UI_DATA + 8, 3)
    cpu.write16(SAVE_UI_DATA + 10, 4)
    cpu.write16(0x8013AF70, 0)
    cpu.write32(0x8013AF74, 0)
    cpu.write32(MODEL_ANGLE, 0)
    cpu.write16(MODEL_DRAW_FIRST, 0)
    cpu.write16(SAVE_UI_CARD_AVAILABLE, 1)
    for index in range(5):
        record = slot_record(index)
        cpu.write16(record + SAVE_ENTRY_TYPE, 1)
        cpu.write8(record + SAVE_SLOT_NAME, 1)
        cpu.write_bytes(record + SAVE_SLOT_NAME,
                        bytes((ord("A") + index, ord("p"), ord("e"), 0)) + bytes(12))
        cpu.write_bytes(record + SAVE_SLOT_USED, bytes(25))
    cpu.write_bytes(SAVE_NAME_BUFFER, b"APE\0" + bytes(12))
    for index in range(5):
        cpu.write16(SAVE_NAME_TIMERS + 2 * index, 0)
    for offset in (0, 2, 4, 6, 8):
        cpu.write16(SAVE_NAME_GRID + offset, 0)
    for option in range(5):
        cpu.write16(FILE_SELECT_OPTIONS + option * 2, 1)
    # Quiet controller state for CardMgr's genuine PadUpdate call.
    cpu.write_bytes(PAD_BUF1, bytes([0, 0x73, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80]))
    cpu.write_bytes(PAD_BUF2, bytes([0xFF, 0x73, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80]))
    cpu.write32(PAD_DEMO, 0)
    cpu.write32(0x800E5494, 1)
    for index in range(40):
        cpu.write16(PAD_STICK_DONE + index * 2, 1)
    cpu.write16(0x800E5490, 0)
    cpu.write32(PAD_HELD, 0)
    cpu.write32(PAD_CHANGED, 0)
    cpu.write32(PAD_REPEAT, 0)

    default_args = ()
    context = {"checksum": 0x5A6B, "pad_state": 1, "pad_info_mode": 1}
    if name == "SaveUi_DrawDetail":
        cpu.write16(SAVE_UI_DATA + 6, 2)
    elif name == "SaveUi_LastUsedEntry":
        default_args = (1,)
    elif name == "SaveUi_NameEntryDraw":
        set_name_page_state(cpu, 0)
        cpu.write16(SAVE_NAME_CURSOR_X, 0)
    elif name in ("CardMgr_RunFileList", "CardMgr_RunFileDetail"):
        context["busy"] = 1
    return default_args, context


def scenario_setup(cpu, name, case, args, context, seed=186):
    rng = random.Random(seed * 1000 + sum(map(ord, name)) * 10 + case)
    initial_regs = {}

    if name == "SaveUi_DrawDetail":
        changed, held = ((0, 0), (4, 0), (4, 4), (8, 8))[case]
        cpu.write32(PAD_CHANGED, changed)
        cpu.write32(PAD_HELD, held)
        cpu.write16(SAVE_UI_DATA + 6, case % 4)
    elif name == "SaveUi_LastUsedEntry":
        slot = case % 4
        markers = ((), (0,), (4, 19), (24,))
        for marker in markers[case]:
            cpu.write8(slot_record(slot) + SAVE_SLOT_USED + marker, 1 + case)
        args = (slot,)
    elif name == "SaveUi_DrawBackground3D":
        states = (20, 21, 21, 21)
        selections = (1, 2, -1, 4)
        angles = (-9, -1024, -1024, 0x100)
        steps = (-16, 0, 16, 31)
        cpu.write16(MENU_RENDER_STATE, states[case])
        cpu.write16(SAVE_SELECTION, selections[case])
        cpu.write32(0x8013AF70, angles[case])
        cpu.write32(MODEL_ANGLE, angles[case])
        cpu.write32(0x8013AF74, steps[case])
        cpu.write16(MODEL_DRAW_FIRST, 1 if case == 2 else 0)
    elif name == "SaveUi_YesNoUpdate":
        cases = [
            (0, 0, 0, 0),
            (1, 0x1000, 0, 0),
            (1, 0, 0x60, 0x60),
            (1, 0x4000, 0x20, 0x20),
            (1, 0, 0, 0),
        ]
        fade, repeat, held, changed = cases[case]
        cpu.write16(SAVE_FADE_STATE, fade)
        cpu.write32(PAD_REPEAT, repeat)
        cpu.write32(PAD_HELD, held)
        cpu.write32(PAD_CHANGED, changed)
        cpu.write16(0x8013AF5A, case & 1)
    elif name == "SaveUi_FileSelectUpdate":
        cases = [
            (0, 0, 0, 1, 0, 0, (1, 1, 1, 1, 1)),
            (1, 0, 0, 1, 0x60, 0x60, (1, 1, 1, 1, 1)),
            (1, 0, 2, 1, 0x60, 0x60, (1, 1, 1, 1, 1)),
            (1, 1, 2, 0, 0x90, 0x90, (1, 1, 1, 1, 1)),
            (1, 1, 1, 1, 0x60, 0x60, (1, 1, 1, 1, 1)),
            (1, 1, 1, 1, 0x60, 0x60, (1, 1, 1, 1, 1)),
            (1, 1, 1, 1, 0x60, 0x60, (1, 1, 1, 1, 1)),
            (1, 1, 2, 1, 0x60, 0x60, (1, 1, 1, 1, 1)),
            (1, 1, 2, 1, 0x60, 0x60, (1, 1, 1, 1, 1)),
            (1, 1, 3, 1, 0x60, 0x60, (1, 1, 1, 1, 1)),
            (1, 1, 4, 1, 0x60, 0x60, (1, 1, 1, 1, 1)),
            (1, 1, 4, 1, 0x60, 0x60, (1, 1, 1, 1, 1)),
            (1, 0, 1, 1, 0, 0, (0, 1, 0, 1, 1)),
            (1, 0, 0, 1, 0x04, 0x04, (1, 1, 1, 1, 1)),
            (1, 0, 0, 1, 0x08, 0x08, (1, 1, 1, 1, 1)),
            (1, 0, 4, 1, 0, 0, (1, 1, 1, 1, 1)),
            (1, 1, 1, -1, 0x60, 0x60, (1, 1, 1, 1, 1)),
            (1, 1, 1, -1, 0x60, 0x60, (1, 1, 1, 1, 1)),
            (1, 1, 2, -1, 0x60, 0x60, (1, 1, 1, 1, 1)),
            (1, 1, 1, 1, 0, 0, (1, 1, 1, 1, 1)),
            (1, 1, 4, 1, 0, 0, (1, 1, 1, 1, 1)),
        ]
        fade, confirm_mode, selection, entry_type, held, changed, options = cases[case]
        cpu.write16(SAVE_FADE_STATE, fade)
        cpu.write16(SAVE_CONFIRM_MODE, confirm_mode)
        cpu.write16(SAVE_SELECTION, selection)
        cpu.write16(SAVE_UI_DATA + 6, 1)
        cpu.write16(slot_record(1) + SAVE_ENTRY_TYPE, entry_type)
        cpu.write16(SAVE_UI_CARD_AVAILABLE, 0 if case in (5, 17) else 1)
        cpu.write32(PAD_HELD, held)
        cpu.write32(PAD_CHANGED, changed)
        repeat_values = {12: 0x1000, 15: 0x4000, 19: 0x1000, 20: 0x4000}
        cpu.write32(PAD_REPEAT, repeat_values.get(case, 0))
        if case == 19:
            cpu.write16(SAVE_UI_DATA + 6, 0)
        elif case == 20:
            cpu.write16(SAVE_UI_DATA + 6, 3)
        for index, enabled in enumerate(options):
            cpu.write16(FILE_SELECT_OPTIONS + index * 2, enabled)
    elif name in ("SaveUi_ConfirmPressed", "SaveUi_CancelPressed"):
        states = (0, 1, 1, 1)
        masks = (0, 0x60 if name.endswith("ConfirmPressed") else 0x90,
                 0, 0x40 if name.endswith("ConfirmPressed") else 0x80)
        cpu.write16(SAVE_FADE_STATE, states[case])
        cpu.write32(PAD_HELD, masks[case])
        cpu.write32(PAD_CHANGED, masks[case])
    elif name == "SaveUi_FadeTick":
        states = (1, 0, 0, 2, 2, -1, 3)
        timers = (0, 1, 0, 30, 29, 0, 0)
        cpu.write16(SAVE_FADE_STATE, states[case])
        cpu.write16(SAVE_FADE_TIMER, timers[case])
    elif name == "SaveUi_OnCardResult_7":
        # The jump table is outside this loaded member; seed the exact targets
        # used by the original switch so the oracle follows each dispatch arm.
        targets = (0x8013B46C, 0x8013B454, 0x8013B444, 0x8013B45C)
        result_index = case
        for index in range(10):
            cpu.write32(CARD_RESULT_TABLE + index * 4,
                        targets[index] if index < len(targets) else targets[0])
        cpu.write16(SAVE_UI_DATA, result_index - 8)
        cpu.write16(0x8013C5B4, 3 + case)
    elif name == "SaveUi_ConfirmDialogUpdate":
        cpu.write16(SAVE_FADE_STATE, 1)
        cpu.write16(0x8013AF5A, 0 if case != 2 else 1)
        cpu.write32(PAD_HELD, 0x60 if case else 0)
        cpu.write32(PAD_CHANGED, 0x60 if case else 0)
        cpu.write32(PAD_REPEAT, 0)
        cpu.write16(SAVE_UI_DATA + 6, 1)
        cpu.write16(SAVE_NAME_LENGTH, 4)
        cpu.write_bytes(SAVE_NAME_BUFFER, b"MONK\0" + bytes(11))
        cpu.write16(MENU_RENDER_STATE, 21)
        cpu.write16(MODEL_DRAW_FIRST, 1)
    elif name == "SaveUi_NameEntryInit":
        cpu.write16(SAVE_UI_DATA + 6, 2)
        cpu.write16(0x8013AF50, 5 + case)
        cpu.write8(slot_record(2) + SAVE_SLOT_NAME, 0 if case == 0 else ord("X"))
        cpu.write_bytes(slot_record(2) + SAVE_SLOT_NAME,
                        (bytes(16) if case == 0 else b"EXISTING NAME\0\0\0"))
        cpu.write_bytes(NAME_BUFFER_HOOK_RESULT, b"DEFAULT\0" + bytes(8))
        context["caller_s0"] = (0x1234, 0xFFFFFFF0, 0x80000000)[case]
        initial_regs[16] = context["caller_s0"]
    elif name == "SaveUi_NameEntryUpdate":
        page = (case - 4) // 2 if 4 <= case < 14 else 0
        set_name_page_state(cpu, page)
        cpu.write16(SAVE_NAME_CURSOR_X, case % 3)
        cursor_y = 4 if case == 2 else (0 if case == 3 else case % 4)
        cpu.write16(SAVE_NAME_CURSOR_Y, cursor_y)
        for index in range(5):
            cpu.write16(SAVE_NAME_TIMERS + index * 2, 0 if case == 0 else index + 1)
        if case == 0:
            buttons, repeat = 0x800, 0
        elif case in (1, 2):
            buttons, repeat = (0x20 if case == 1 else 0x40), 0
        elif case == 3:
            buttons, repeat = 0x80, 0
        elif case < 14:
            buttons = 0x08 if case % 2 == 0 else 0x04
            repeat = 0
            page = (case - 4) // 2
            set_name_page_state(cpu, page)
        else:
            buttons = 0
            repeat = (0x1000, 0x4000, 0x2000, 0x8000)[case - 14]
            edge_cursor = {15: 80, 16: 89, 17: 0}.get(case)
            if edge_cursor is not None:
                cpu.write16(SAVE_NAME_CURSOR_X, edge_cursor)
        cpu.write32(PAD_HELD, buttons)
        cpu.write32(PAD_CHANGED, buttons)
        cpu.write32(PAD_REPEAT, repeat)
    elif name == "SaveUi_NameEntryDraw":
        page = case
        set_name_page_state(cpu, page)
        cpu.write16(SAVE_NAME_CURSOR_X, (0, 14, 43, 20, 0)[case])
        cpu.write16(SAVE_NAME_CURSOR_Y, case)
        cpu.write16(SAVE_NAME_TABLE_OFFSET, (0, 0, 180, 44, 0)[case])
        cpu.write16(SAVE_NAME_CELL_COUNT, (90, 90, 44, 44, 44)[case])
        for index in range(5):
            cpu.write16(SAVE_NAME_TIMERS + index * 2, (0, 4, 8, 2, 6)[(index + case) % 5])
    elif name == "CardMgr_Run":
        if case == 0:
            context["run_message_results"] = [-1]
        else:
            context["run_message_state"] = 1 if case == 1 else 2
            context["run_message_results"] = [0, -1]
            context["busy"] = 1
    elif name == "CardMgr_Init":
        cpu.write16(CARDMGR_DISPLAY + 0x13C, 0x1234 + case)
        cpu.write16(CARDMGR_DISPLAY + 0x154, 0x2345 + case)
        cpu.write16(CARDMGR_DISPLAY + 0x156, 0x3456 + case)
        cpu.write8(CARDMGR_HEADER + 0xA1, 0x45 + case)
        cpu.write8(CARDMGR_HEADER + 0xA2, 0x67 + case)
    elif name in ("CardMgr_RunFileList", "CardMgr_RunFileDetail"):
        context["busy"] = 1 if case == 0 else [0, 1]
        cpu.write16(SAVE_FADE_STATE, 1)
        cpu.write16(SAVE_FADE_TIMER, 0)
        cpu.write32(DISPLAY_INDEX, 0)
        for byte in range(8):
            cpu.write8(0x8014EEE0 + byte, rng.randrange(256))

    return args, context, initial_regs


class Harness:
    def __init__(self, source=None, tag="ovl186b"):
        reg.declare("stack_window", "801FF000..80200000")
        reg.declare("shared_random", "SaveUi_NameEntryInit caller $s0")
        self.dll = build(source, tag)
        self.c = CSide(self.dll)
        self.counts = {name: 0 for name in FUNCS}
        self.coverage = set()
        self.failures = 0
        all_areas = dump_areas.area_members()
        selected_area_ids = [1, 2, 5, 19, 36, 44]
        self.area_members = [(area, all_areas[area]) for area in selected_area_ids
                             if area in all_areas]
        self.area_to_member = all_areas
        if len(self.area_members) < 3:
            raise AssertionError("expected real members from several areas")
        # The checked ranges contain only integer code and pure matrix/trig
        # routines; inline GTE work is inside the renderer patches below.
        self.m = mr.MipsRam.from_extract()
        self.base_cpus = {}
        for _area, member in self.area_members:
            cpu = mr.MipsRam.from_extract()
            cpu.load_area(Path(ROOT / f"extract/members/{member:03X}.bin").read_bytes(), addr=0x80154000)
            cpu.load_area(OVERLAY.read_bytes(), addr=OVERLAY_BASE)
            self.base_cpus[member] = cpu.snapshot()
        self.progress = False

    @staticmethod
    def _signed16(value):
        value &= 0xFFFF
        return value - 0x10000 if value & 0x8000 else value

    def _patches(self, model):
        def detail(cpu, regs):
            selected = self._signed16(cpu.read16(SAVE_UI_DATA + 6))
            choice = cpu.read16(SAVE_CHOICE)
            model.call("SaveUi_DrawDetailPanel", (selected, choice))
            return 0x80139458

        def name_panel(cpu, regs):
            model.call("SaveUi_NameEntryPanel", (
                self._signed16(cpu.read16(SAVE_NAME_PAGE)),
                self._signed16(cpu.read16(SAVE_NAME_CURSOR_X)),
                self._signed16(cpu.read16(SAVE_NAME_CURSOR_Y)),
                SAVE_NAME_BUFFER,
            ))
            # The replaced packet builder also establishes this pointer for
            # the following icon packet emitters.
            regs[16] = DISPLAY_STATE
            return 0x8013C3B4

        def name_footer(cpu, regs):
            model.call("SaveUi_NameEntryFooter", ())
            # The skipped packet footer ends with lw v0, 4(s0); C49C advances
            # that OT pointer by eight bytes before calling DrawMainWindow.
            regs[2] = cpu.read32(DISPLAY_STATE + 4)
            return 0x8013C49C

        return {0x80139240: detail, 0x8013BF44: name_panel,
                0x8013C43C: name_footer}

    def _mips_hooks(self, model):
        hooks = {}
        for address, (name, argc) in HOOKS.items():
            def invoke(cpu, regs, hook_name=name, count=argc, active=model):
                if hook_name == "PadUpdate":
                    # PadUpdate is already an exact, separately diffed port.
                    # Use that port as the MIPS-side adapter, as other overlay
                    # checkers do, and compare its hardware-hook sequence.
                    self.c.sync(cpu)
                    pad_hooks, callbacks = self.c.make_pad_hooks(active)
                    self.c.lib.ae_pad_PadUpdate(
                        self.c.ram, C.byref(pad_hooks), regs[4])
                    if self.c.lib.ae_ram_failed(self.c.ram):
                        message = self.c.lib.ae_ram_error(self.c.ram).decode(errors="replace")
                        raise AssertionError("PadUpdate adapter failed: " + message)
                    cpu.ram[:] = bytes(self.c.main)
                    cpu.scratch[:] = bytes(self.c.scratch)
                    return 0
                return active.call(hook_name, tuple(regs[4:4 + count]))
            hooks[address] = invoke
        return hooks

    def _initial_regs(self, name, values):
        regs = dict(values or {})
        if name == "SaveUi_NameEntryInit":
            regs[16] = regs.get(16, 0)
            self.c.model.context["caller_s0"] = regs[16]
        return regs

    def run(self, name, args=None, area=None, case=0, seed=186,
            setup=None, mips_limit=2_000_000):
        area_id, member = self.area_members[case % len(self.area_members)]
        if area is not None:
            area_id = area
            member = self.area_to_member.get(area, member)
        self.m.restore(self.base_cpus[member])
        default_args, context = base_setup(self.m, area_id, member, name, seed)
        if args is None:
            args = default_args
        args, context, initial_regs = scenario_setup(
            self.m, name, case, tuple(args), context, seed)
        if setup is not None:
            setup(self.m)
        self.m.coverage = set()
        self.c.sync(self.m)
        m_model = HookModel(self.m, context)
        c_model = HookModel(CMemory(self.c), context)
        self.c.configure(c_model)
        initial_regs = self._initial_regs(name, initial_regs)
        c_main = bytes(self.c.main)
        c_scratch = bytes(self.c.scratch)

        try:
            registers = self.m.call(
                FUNCS[name][0], args, hooks=self._mips_hooks(m_model),
                patches=self._patches(m_model), limit=mips_limit,
                sp=mr.DEFAULT_SP, initial_regs=initial_regs)
            self.coverage.update(self.m.coverage or ())
            self.c.main[:] = c_main
            self.c.scratch[:] = c_scratch
            self.c.lib.ae_ram_clear_error(self.c.ram)
            c_result = self.c.invoke(name, args)
        except Exception as exc:
            machine = getattr(self.m, "_last_machine", None)
            events = m_model.events[-10:]
            raise AssertionError(f"{name} case {case} execution failed: {exc}; machine={machine}; events={events}") from exc

        self.counts[name] += 1
        if FUNCS[name][2] != "v" and (c_result & 0xFFFFFFFF) != registers[2]:
            self.fail(name, case, f"return differs C={c_result & 0xFFFFFFFF:08X} MIPS={registers[2]:08X}")
        self.compare_ram(name, case)
        if c_model.events != m_model.events:
            self.fail(name, case, f"hook sequence differs C={c_model.events!r}; MIPS={m_model.events!r}")
        if self.c.lib.ae_ram_failed(self.c.ram):
            message = self.c.lib.ae_ram_error(self.c.ram).decode(errors="replace")
            self.fail(name, case, message)

    def compare_ram(self, name, case):
        expected = bytes(self.m.ram[:STACK_LO])
        actual = bytes(self.c.main[:STACK_LO])
        if actual != expected:
            offset = next(index for index, (a, b) in enumerate(zip(actual, expected)) if a != b)
            more = [(f"{0x80000000 + index:08X}", actual[index], expected[index])
                    for index, (a, b) in enumerate(zip(actual, expected))
                    if a != b][:16]
            self.fail(name, case,
                      f"RAM differs at {0x80000000 + offset:08X}: C={actual[offset]:02X} MIPS={expected[offset]:02X}; first differences={more!r}; events C={self.c.model.events[-12:]!r} MIPS={getattr(self.m, 'hook_log', [])[-12:]!r}")
        if bytes(self.c.scratch) != bytes(self.m.scratch):
            self.fail(name, case, "scratchpad differs")

    def fail(self, name, case, message):
        self.failures += 1
        raise AssertionError(f"FAIL {name} case {case}: {message}")

    def finish(self, extra_coverage=EXTRA_COVERAGE, default_run=False,
               show_coverage=False):
        items = [(name, address, size, True)
                 for name, (address, size, _ret) in FUNCS.items()]
        items.extend((name, address, size, False)
                     for name, (address, size) in extra_coverage.items())
        summary = reg.coverage_summary(items, self.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        gaps = [(name, entry["uncovered"], entry["stale_declared"])
                for name, entry in summary.items()
                if entry["uncovered_count"] or entry["stale_declared"]]
        if default_run:
            reg.write_results(self.counts, summary, not gaps,
                              function_names=list(FUNCS))
        if gaps and default_run:
            print("coverage gaps:", gaps)
            raise AssertionError("undeclared overlay instruction coverage gaps")
        return summary


def scenarios(h, scale=1.0, seed=186, only=None, progress=False,
              case_index=None, mips_limit=2_000_000):
    names = list(FUNCS) if only is None else [only]
    for name in names:
        count = max(1, min(DEFAULT_CASES[name], math.ceil(DEFAULT_CASES[name] * scale)))
        indices = range(count) if case_index is None else (case_index,)
        for case in indices:
            if case < 0 or case >= DEFAULT_CASES[name]:
                raise ValueError(f"case index {case} out of range for {name}")
            if progress:
                print(f"checking {name} case {case + 1}/{DEFAULT_CASES[name]}", flush=True)
            h.run(name, case=case, seed=seed, mips_limit=mips_limit)


def run_mutations():
    source_path = ROOT / "src/modes/ae_ovl186b.c"
    source = source_path.read_text(encoding="utf-8")
    insertions = []
    for index, name in enumerate(FUNCS):
        pattern = re.compile(
            rf"(?m)^(?:void|int32_t|uint32_t)\s+ae_{re.escape(name)}"
            rf"\s*\([^)]*\)\s*\{{"
        )
        matches = list(pattern.finditer(source))
        if len(matches) != 1:
            raise AssertionError(f"expected one definition of ae_{name}, found {len(matches)}")
        first_selector = index * 2 + 1
        second_selector = first_selector + 1
        mutation = (
            f"\n    if (ae_rd32(r, 0x{MUTATION_SELECTOR:08X}u) == {first_selector}u) "
            f"ae_wr8(r, 0x{MUTATION_TARGETS[0]:08X}u, 0x5Au);\n"
            f"    else if (ae_rd32(r, 0x{MUTATION_SELECTOR:08X}u) == {second_selector}u) "
            f"ae_wr8(r, 0x{MUTATION_TARGETS[1]:08X}u, 0xA5u);\n"
        )
        insertions.append((matches[0].end(), mutation))
    for offset, insertion in sorted(insertions, reverse=True):
        source = source[:offset] + insertion + source[offset:]
    fd, temporary_name = tempfile.mkstemp(prefix="ae_ovl186b_mutations_",
                                          suffix=".c", dir=ROOT / "src/modes")
    temporary = Path(temporary_name)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as output:
            output.write(source)
        h = Harness(source=temporary, tag="ovl186b_mutations")
        caught = 0
        for index, name in enumerate(FUNCS):
            for delta, target in enumerate(MUTATION_TARGETS):
                selector = index * 2 + delta + 1
                def setup(cpu, selected=selector):
                    cpu.write32(MUTATION_SELECTOR, selected)
                    cpu.write8(MUTATION_TARGETS[0], 0)
                    cpu.write8(MUTATION_TARGETS[1], 0)
                try:
                    h.run(name, case=0, setup=setup)
                except AssertionError as error:
                    if f"RAM differs at {target:08X}" not in str(error):
                        raise AssertionError(f"{name} mutation {selector} failed for another reason: {error}") from error
                    caught += 1
                else:
                    raise AssertionError(f"{name} mutation {selector} was not detected")
        print(f"OK: caught {caught}/{2 * len(FUNCS)} mutations (2 per function)")
    finally:
        temporary.unlink(missing_ok=True)
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--cases", type=float, default=1.0)
    ap.add_argument("--seed", type=int, default=186)
    ap.add_argument("--function", choices=list(FUNCS))
    ap.add_argument("--mutate", action="store_true")
    ap.add_argument("--progress", action="store_true")
    ap.add_argument("--case-index", type=int)
    ap.add_argument("--mips-limit", type=int, default=2_000_000,
                    help="maximum oracle instructions per case")
    args = ap.parse_args()
    if args.case_index is not None and args.function is None:
        ap.error("--case-index requires --function")
    if args.mutate:
        if args.function is not None or args.case_index is not None:
            ap.error("--mutate cannot be combined with --function or --case-index")
        if args.cases != 1.0 or args.seed != 186:
            ap.error("--mutate always uses default scenarios")
        return run_mutations()
    h = Harness()
    h.progress = args.progress
    scenarios(h, args.cases, args.seed, args.function, args.progress,
              args.case_index, args.mips_limit)
    h.finish(EXTRA_COVERAGE, default_run=(args.cases == 1.0 and args.seed == 186
                                         and args.function is None),
             show_coverage=args.coverage)
    print(" ".join(f"{name}={h.counts[name]}" for name in FUNCS))
    print(f"OK: {sum(h.counts.values())} cases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
