"""Differential check for the stage-fade, sound-stop, resident-TIM and HUD ports."""
import argparse
import contextlib
import ctypes as C
import hashlib
import io
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
import check_stage as stage_check
import dump_areas
import exclusion_registry as reg
import mips_ram as mr

OUT = ROOT / "extract/mips"
STACK_LO = 0x1FF000
SPU_BASE = 0x1F801C00
SEQ_BASE = 0x801E0000
ARC_STATE = 0x800E3890
ARC_RECORDS = 0x1C
ARC_RECORD_INDEX = 0x20
FADE_STATE = 0x800F4350
FADE_FRAME_COUNTER = 0x800EC350
ORDER_TABLE_CURSOR = FADE_FRAME_COUNTER + 0x04
TIM_POINTERS = 0x800DF7A0
TIM_IDS = 0x800DF7C0
TIM_AGES = 0x800DF7D0
TIM_COUNT = 0x800B35C4
SOUND_BANKS = 0x800BF670
LOOP_TABLE = 0x800BF370

FUNCS = {
    "StageFadeUpdate": (0x80079B10, 872, "s", "", True),
    "ScreenFlashFadeDraw": (0x80079E78, 84, "v", "", True),
    "FadeSpiralDraw": (0x80079ECC, 640, "v", "", True),
    "SoundStop": (0x80086FD0, 652, "s", "s", True),
    "SoundLoopsReset": (0x80088194, 60, "s", "", False),
    "TimListResidentUpdate": (0x8008D064, 396, "s", "ss", True),
    "HudPanelA_Start": (0x80095244, 88, "s", "s", False),
}

NATIVE_HOOKS = [
    ("DebugTextPrintf", 0x8008C6F0),
    ("SeqTrackClose", 0x800A1F3C),
    ("SeqTrackSetPlayState", 0x800A2B6C),
    ("SoundBankFree", 0x800A1CCC),
    ("SoundStopAllVoices", 0x800A342C),
]
EXTRA_COVERAGE = {"BgmSeqSetVolume": (0x800A2428, 420)}
_GTE_ORACLE = None
STAGE_HOOKS = stage_check.HOOKS
STAGE_HOOK_NAMES = [name for name, _address, _nargs in STAGE_HOOKS]
STAGE_HEADER = (ROOT / "src/world/ae_stage.h").read_text(encoding="utf-8")
STAGE_HOOK_BLOCK = STAGE_HEADER.split("typedef struct AeStageHooks {", 1)[1].split(
    "} AeStageHooks", 1)[0]
STAGE_HOOK_FIELDS = re.findall(r"AeStageHookFn\s+(\w+)\s*;", STAGE_HOOK_BLOCK)
FADE_HEADER = (ROOT / "src/world/ae_stagefade.h").read_text(encoding="utf-8")
FADE_HOOK_BLOCK = FADE_HEADER.split("typedef struct AeStageFadeHooks {", 1)[1].split(
    "} AeStageFadeHooks", 1)[0]
FADE_HOOK_FIELDS = re.findall(r"AeStageFadeHookFn\s+(\w+)\s*;", FADE_HOOK_BLOCK)
HookFn = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                     C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
U32, I32, P = C.c_uint32, C.c_int32, C.c_void_p


def s32(value):
    return mr.signed(value, 32)


def s16(value):
    return mr.signed(value, 16)


def attach_shared_gte_oracle(cpu):
    global _GTE_ORACLE
    if _GTE_ORACLE is None:
        cpu.attach_gte_oracle()
        _GTE_ORACLE = (cpu._gte_lib, cpu.gte_state, cpu.gte_backend)
    else:
        cpu._gte_lib, cpu.gte_state, cpu.gte_backend = _GTE_ORACLE


class SoundHw(C.Structure):
    _fields_ = [
        ("user", P),
        ("wait_idle", C.CFUNCTYPE(I32, P, P)),
        ("wait_loop", C.CFUNCTYPE(None, P, P, U32)),
        ("set_voice_adsr", C.CFUNCTYPE(None, P, P, U32, C.c_uint16, C.c_uint16)),
        ("voice_key_off", C.CFUNCTYPE(None, P, P, U32, C.c_uint16)),
        ("printf_error", C.CFUNCTYPE(None, P, P, U32, U32, I32, I32, I32)),
        ("set_voice_volume", C.CFUNCTYPE(None, P, P, U32, U32, C.c_uint16)),
    ]


class FadeHarness:
    def __init__(self, source=None, tag="stagefade", trapv=False):
        reg.declare("stack_window", "801FF000..80200000")
        reg.declare("unreachable", "FadeSpiralDraw 80079F90 8007A000..8007A004 8007A02C..8007A10C")
        self.area_members = dump_areas.area_members()
        needed = (1, 2, 5, 24, 53)
        missing = [area for area in needed if area not in self.area_members]
        if missing:
            raise AssertionError(f"real area-member map is missing areas {missing}")
        self.area_bases = {}
        exe = mr.MipsRam.from_extract()
        exe_base = exe.snapshot()
        for area in needed:
            blob = (ROOT / f"extract/members/{self.area_members[area]:03X}.bin").read_bytes()
            exe.restore(exe_base)
            exe.load_area(blob)
            self.area_bases[area] = exe.snapshot()
        self.m = exe
        attach_shared_gte_oracle(self.m)
        self.m.coverage = set()

        self.lib = C.CDLL(str(build_dll(source, tag, trapv)))
        self.c = stage_check.CSide.__new__(stage_check.CSide)
        self._bind_stage_side()
        self.stage_hooks_type = type(self.c.hooks)
        self.fade_hooks_type = type("AeStageFadeHooks", (C.Structure,), {
            "_fields_": [("user", P), ("stack_pointer", U32), ("caller_a3", U32),
                         ("stage", C.POINTER(self.stage_hooks_type)), ("sound_hw", P)] +
                        [(name, HookFn) for name in FADE_HOOK_FIELDS]
        })
        self._bind_fade_functions()
        self.fade_hooks = self.fade_hooks_type()
        self.sound_hw = SoundHw()
        self.sound_hw_keep = []
        self._install_sound_hw()
        self.fade_hooks.sound_hw = C.cast(C.pointer(self.sound_hw), P)
        self.fade_hooks.stage = C.pointer(self.c.hooks)
        self.fade_callbacks = []
        self._install_fade_hooks()
        self.nested_sound_stop = HookFn(self._nested_sound_stop)
        self.c.callbacks.append(self.nested_sound_stop)
        self.c.hooks.SoundStop = self.nested_sound_stop
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0
        self.active_mips_model = None
        self.active_c_model = None

    def _bind_stage_side(self):
        c = self.c
        c.lib = self.lib
        c.lib.stagefade_ram_create.restype = P
        c.lib.stagefade_ram_ptr.argtypes = [P, U32, U32]
        c.lib.stagefade_ram_ptr.restype = P
        c.lib.stagefade_ram_failed.argtypes = [P]
        c.lib.stagefade_ram_failed.restype = I32
        c.lib.stagefade_ram_error.argtypes = [P]
        c.lib.stagefade_ram_error.restype = C.c_char_p
        c.lib.stagefade_ram_clear_error.argtypes = [P]
        c.ram = c.lib.stagefade_ram_create()
        main_pointer = c.lib.stagefade_ram_ptr(c.ram, 0x80000000, mr.RAM_SIZE)
        scratch_pointer = c.lib.stagefade_ram_ptr(c.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE)
        if not c.ram or not main_pointer or not scratch_pointer:
            raise MemoryError("could not allocate the C RAM image")
        c.main = (C.c_uint8 * mr.RAM_SIZE).from_address(main_pointer)
        c.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(scratch_pointer)
        c.model = None
        c.callbacks = []

        def sync(mips):
            C.memmove(c.main, bytes(mips.ram), mr.RAM_SIZE)
            C.memmove(c.scratch, bytes(mips.scratch), mr.SCRATCH_SIZE)
            c.lib.stagefade_ram_clear_error(c.ram)

        c.sync = sync

        stage_header = STAGE_HEADER
        hook_block = stage_header.split("typedef struct AeStageHooks {", 1)[1].split(
            "} AeStageHooks", 1)[0]
        names = re.findall(r"AeStageHookFn\s+(\w+)\s*;", hook_block)
        self.stage_hooks_type = type("AeStageHooks", (C.Structure,), {
            "_fields_": [("user", P), ("stack_pointer", U32)] +
                        [(name, HookFn) for name in names] +
                        [("monkey_hooks", P), ("gadget_hooks", P)]
        })
        c.hooks = self.stage_hooks_type()
        # StageRequest action 13 reaches the stage module's monkey/gadget hook
        # helpers. Preserve the existing test harness's exact sub-hook layouts.
        monkey_header = (ROOT / "src/monkeys/ae_monkey.h").read_text(encoding="utf-8")
        monkey_block = monkey_header.split("typedef struct AeMonkeyHooks {", 1)[1].split(
            "} AeMonkeyHooks", 1)[0]
        monkey_names = re.findall(r"AeMonkeyHookFn\s+(\w+)\s*;", monkey_block)
        c.monkey_hooks_type = type("AeMonkeyHooks", (C.Structure,), {
            "_fields_": [("user", P), ("body", P)] + [(name, P) for name in monkey_names]
        })
        c.monkey_hooks = c.monkey_hooks_type()
        c.gadget_select_type = C.CFUNCTYPE(None, P, P, I32, I32)
        c.gadget_hud_type = C.CFUNCTYPE(None, P, P)
        c.gadget_hooks_type = type("AeGadgetHooks", (C.Structure,), {
            "_fields_": [("user", P), ("select_gadget", c.gadget_select_type),
                         ("hud_find_icon_pos", c.gadget_hud_type)]
        })
        c.gadget_hooks = c.gadget_hooks_type()
        c.hooks.monkey_hooks = C.cast(C.pointer(c.monkey_hooks), P)
        c.hooks.gadget_hooks = C.cast(C.pointer(c.gadget_hooks), P)

        c.model = None
        for name in STAGE_HOOK_FIELDS:
            cb = HookFn(lambda _u, _r, a0, a1, a2, a3, n=name:
                        c.model.call(n, (a0, a1, a2, a3)))
            c.callbacks.append(cb)
            setattr(c.hooks, name, cb)
        # Stage module wrappers require these secondary callback objects.
        clear_fn = HookFn(lambda _u, _r, a0, a1, a2, a3:
                          c.model.call("MonkeyClearWords", (a0, a1, a2, a3)))
        vsync_fn = HookFn(lambda _u, _r, a0, a1, a2, a3:
                          c.model.call("VSync", (a0, a1, a2, a3)))
        c.callbacks.extend((clear_fn, vsync_fn))
        for name in monkey_names:
            setattr(c.monkey_hooks, name, C.cast(clear_fn, P) if name == "ClearWords" else
                    (C.cast(vsync_fn, P) if name == "VSync" else P()))
        gadget_select = c.gadget_select_type(
            lambda _user, _ram, gadget, old:
                c.model.call("PlayerSelectGadget", (gadget, old, 0, 0)))
        gadget_hud = c.gadget_hud_type(
            lambda _user, _ram: c.model.call("HudFindGadgetIconPos", (0, 0, 0, 0)))
        c.callbacks.extend((gadget_select, gadget_hud))
        c.gadget_hooks.select_gadget = gadget_select
        c.gadget_hooks.hud_find_icon_pos = gadget_hud
        self.stage_hook_type = HookFn

    def _bind_fade_functions(self):
        c = self.c
        c.libfuncs = {}
        for name, (_address, _size, ret, args, takes_hooks) in FUNCS.items():
            fn = getattr(c.lib, "stagefade_call_" + name)
            fn.argtypes = [P] + ([C.POINTER(self.fade_hooks_type)] if takes_hooks else []) + [
                I32 if kind == "s" else U32 for kind in args]
            fn.restype = None if ret == "v" else I32
            c.libfuncs[name] = fn

    def _install_sound_hw(self):
        def idle(_u, _r):
            return self.active_c_model.call("SpuWaitIdle", ())

        def wait_loop(_u, _r, units):
            self.active_c_model.call("SpuWaitLoop", (units,))

        def adsr(_u, _r, channel, adsr1, adsr2):
            self.active_c_model.record("set_voice_adsr", (channel, adsr1, adsr2))

        def key_off(_u, _r, half, mask):
            self.active_c_model.record("voice_key_off", (half, mask))

        def print_error(_u, _r, fmt, argc, a, b, c):
            self.active_c_model.record("printf_error", (fmt, argc, a, b, c))

        def volume(_u, _r, channel, side, value):
            self.active_c_model.record("set_voice_volume", (channel, side, value))

        callbacks = [
            C.CFUNCTYPE(I32, P, P)(idle),
            C.CFUNCTYPE(None, P, P, U32)(wait_loop),
            C.CFUNCTYPE(None, P, P, U32, C.c_uint16, C.c_uint16)(adsr),
            C.CFUNCTYPE(None, P, P, U32, C.c_uint16)(key_off),
            C.CFUNCTYPE(None, P, P, U32, U32, I32, I32, I32)(print_error),
            C.CFUNCTYPE(None, P, P, U32, U32, C.c_uint16)(volume),
        ]
        self.sound_hw_keep = callbacks
        self.sound_hw.user = C.c_void_p(1)
        for name, cb in zip(("wait_idle", "wait_loop", "set_voice_adsr",
                             "voice_key_off", "printf_error", "set_voice_volume"), callbacks):
            setattr(self.sound_hw, name, cb)

    def _install_fade_hooks(self):
        for name in FADE_HOOK_FIELDS:
            cb = HookFn(lambda _u, _r, a0, a1, a2, a3, n=name:
                        self.active_c_model.call(n, (a0, a1, a2, a3)))
            self.fade_callbacks.append(cb)
            setattr(self.fade_hooks, name, cb)

    def _nested_sound_stop(self, _user, ram, mode, _a1, _a2, _a3):
        return self.c.libfuncs["SoundStop"](
            ram, C.byref(self.fade_hooks), s32(mode))

    def run(self, name, args=(), setup=None, context=None, area=1, initial_regs=None):
        self.m.restore(self.area_bases[area])
        self.m.gte_reset()
        if setup is not None:
            setup(self.m)
        if (name == "StageFadeUpdate" and
                self.m.read16(FADE_STATE + 0x130) == 3 and
                self.m.reads16(FADE_STATE + 0x132) >= 47):
            # Area loading reaches MonkeySpawnAll. Keep the real area member,
            # but seed its spawn count to zero so this checker does not silently
            # exercise another batch's monkey initialization hooks.
            monkey_header = 0x80154000 + self.m.read32(0x801544E4)
            self.m.write32(monkey_header, 0)
            # SoundStop itself is exercised directly in its own cases. Keep
            # the area-load path independent of any save-data BGM track id.
            self.m.write16(SOUND_BANKS + 0x06, 0xFFFF)
            self.m.write16(SOUND_BANKS + 0x16, 0xFFFF)
        self.current_case = (f"area={area} state={self.m.read16(FADE_STATE + 0x130)} "
                             f"timer={self.m.reads16(FADE_STATE + 0x132)} "
                             f"exit={self.m.read8(FADE_STATE + 0x12C)} args={tuple(args)}")
        context = dict(context or {})
        context.setdefault("alternate", 0)
        context.setdefault("seek_result", 0)
        context.setdefault("time_attack", 0)
        context.setdefault("area_kind", 1)
        context.setdefault("level", 1)
        context.setdefault("first_area", 1)
        context.setdefault("monkey_count", 0)
        context.setdefault("load_list_step", -1)

        self.c.sync(self.m)
        entry_sp = mr.DEFAULT_SP
        self.c.hooks.stack_pointer = entry_sp
        self.fade_hooks.stack_pointer = entry_sp
        self.fade_hooks.caller_a3 = (initial_regs or {}).get(7, 0)
        self.fade_hooks.stage = C.pointer(self.c.hooks)

        mips_model = FadeModel(stage_check.Memory(ram=self.m), context)
        c_model = FadeModel(stage_check.Memory(side=self.c), context)
        self.active_mips_model = mips_model
        self.active_c_model = c_model
        self.c.model = c_model

        hooks = {
            address: (lambda cpu, regs, n=hook:
                      self._mips_hook(mips_model, cpu, n, regs))
            for hook, address, _nargs in STAGE_HOOKS if hook != "SoundStop"
        }
        for hook, address in NATIVE_HOOKS:
            hooks[address] = lambda cpu, regs, n=hook: self._mips_hook(mips_model, cpu, n, regs)
        hooks[0x800A1398] = lambda cpu, _regs: mips_model.call("SpuWaitIdle", ())
        hooks[0x8009D79C] = lambda cpu, regs: mips_model.call("SpuWaitLoop", (regs[4],))
        hooks[0x80046990] = lambda cpu, regs: mips_model.call("RenderScreenFlash", (regs[4],))
        patches = {
            0x8007A02C: lambda cpu, regs:
                self._mips_fade_spiral_renderer(mips_model, cpu, regs)
        }

        original_write = self.m.write
        self.m.write = lambda address, size, value: self._mips_write(
            mips_model, original_write, address, size, value)
        try:
            mips_result = self.m.call(FUNCS[name][0], args, hooks=hooks,
                                      patches=patches,
                                      initial_regs=initial_regs)
        finally:
            self.m.write = original_write

        kinds = FUNCS[name][3]
        c_args = [s32(value) if kind == "s" else value & 0xFFFFFFFF
                  for value, kind in zip(args, kinds)]
        fn = self.c.libfuncs[name]
        if FUNCS[name][4]:
            try:
                result = fn(self.c.ram, C.byref(self.fade_hooks), *c_args)
            except TypeError as error:
                self.fail(name, args, f"C ABI call rejected arguments {c_args!r}: {error}")
        else:
            try:
                result = fn(self.c.ram, *c_args)
            except TypeError as error:
                self.fail(name, args, f"C ABI call rejected arguments {c_args!r}: {error}")
        self.counts[name] += 1
        self._compare(name, args, mips_result[2], result,
                      mips_model.events, c_model.events)

    def _mips_hook(self, model, cpu, name, regs):
        return model.call(name, regs[4:8])

    def _mips_fade_spiral_renderer(self, model, cpu, regs):
        timer = cpu.reads16(0x800F4482)
        model.call("RenderFadeSpiral", (timer,))
        # The renderer emits 32 packet records of 24 bytes each. Resume at the
        # original epilogue so its game-RAM cursor store remains instruction-
        # covered and compared on the MIPS side.
        regs[22] = (regs[22] + 32 * 24) & 0xFFFFFFFF
        return 0x8007A110

    def _mips_write(self, model, original_write, address, size, value):
        if SPU_BASE <= address < SPU_BASE + 0x200:
            offset = address - SPU_BASE
            if size != 2:
                raise AssertionError(f"unexpected SPU write width {size} at {address:08X}")
            value &= 0xFFFF
            if offset in (0x18C, 0x18E):
                model.record("voice_key_off", ((offset - 0x18C) // 2, value))
            elif offset < 24 * 16 and offset % 16 in (0, 2):
                model.record("set_voice_volume", (offset // 16, (offset % 16) // 2, value))
            elif offset < 24 * 16 and offset % 16 == 8:
                model.pending_adsr[offset // 16] = value
            elif offset < 24 * 16 and offset % 16 == 10:
                channel = offset // 16
                adsr1 = model.pending_adsr.pop(channel, 0)
                model.record("set_voice_adsr", (channel, adsr1, value))
            else:
                raise AssertionError(f"unexpected SPU register write {address:08X}/{size}")
            return
        original_write(address, size, value)

    def _compare(self, name, args, original_ret, c_ret, original_events, c_events):
        ret_kind = FUNCS[name][2]
        if ret_kind != "v" and (int(c_ret) & 0xFFFFFFFF) != original_ret:
            self.fail(name, args, f"return differs: C={int(c_ret) & 0xFFFFFFFF:08X} MIPS={original_ret:08X}")
        expected = bytes(self.m.ram[:STACK_LO])
        actual = C.string_at(self.c.main, STACK_LO)
        if actual != expected:
            index = next(i for i, (left, right) in enumerate(zip(actual, expected))
                         if left != right)
            lo, hi = max(0, index - 16), min(len(actual), index + 32)
            event_index = next((i for i, pair in enumerate(zip(c_events, original_events))
                                if pair[0] != pair[1]),
                               min(len(c_events), len(original_events)))
            event_diff = (c_events[event_index:event_index + 2],
                          original_events[event_index:event_index + 2])
            self.fail(name, args,
                      f"RAM differs at {0x80000000 + index:08X}: C={actual[index]:02X} MIPS={expected[index]:02X}; "
                      f"C-window={actual[lo:hi].hex()} MIPS-window={expected[lo:hi].hex()}; "
                      f"{self.current_case}; event counts C/MIPS={len(c_events)}/{len(original_events)}; "
                      f"first event difference @{event_index}: C={event_diff[0]!r}, "
                      f"MIPS={event_diff[1]!r}; C-tail={c_events[-5:]!r}; "
                      f"MIPS-tail={original_events[-5:]!r}")
        if C.string_at(self.c.scratch, mr.SCRATCH_SIZE) != bytes(self.m.scratch):
            self.fail(name, args, "scratchpad differs")
        if original_events != c_events:
            event_index = next((i for i, pair in enumerate(zip(c_events, original_events))
                                if pair[0] != pair[1]),
                               min(len(c_events), len(original_events)))
            self.fail(name, args,
                      f"hook calls differ after {event_index} equal events: "
                      f"counts C/MIPS={len(c_events)}/{len(original_events)}; "
                      f"C-next={c_events[event_index:event_index + 4]!r}; "
                      f"MIPS-next={original_events[event_index:event_index + 4]!r}")
        if self.c.lib.stagefade_ram_failed(self.c.ram):
            self.fail(name, args,
                      self.c.lib.stagefade_ram_error(self.c.ram).decode(errors="replace"))

    @staticmethod
    def fail(name, args, detail):
        raise AssertionError(f"{name}{tuple(args)}: {detail}")

    def finish(self, extra=None, default_run=False, show_coverage=False, selected=None):
        selected = list(selected or FUNCS)
        items = [(name, address, size, True)
                 for name, (address, size, *_rest) in FUNCS.items()
                 if name in selected]
        items += [(name, address, size, False)
                  for name, (address, size) in (extra or {}).items()]
        summary = reg.coverage_summary(items, self.m.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        gaps = [name for name, row in summary.items()
                if row["own"] and (row["uncovered_count"] or row["stale_declared"])]
        if default_run:
            reg.write_results(self.counts, summary, not gaps,
                              function_names=selected)
        if gaps:
            raise AssertionError("undeclared instruction coverage gaps: " + ", ".join(gaps))
        return summary


class FadeModel:
    def __init__(self, memory, context):
        self.stage = stage_check.HookModel(memory, context)
        self.memory = memory
        self.context = context
        self.events = self.stage.events
        self.result_index = {}
        self.load_index = 0
        self.pending_adsr = {}
        self.cd_updates = 0

    def record(self, name, args=(), extra=None):
        self.events.append((name, tuple(value & 0xFFFFFFFF for value in args), extra))

    def call(self, name, args):
        args = tuple(value & 0xFFFFFFFF for value in args)
        if name in STAGE_HOOK_NAMES:
            result = self.stage.call(name, args)
            if name == "FUN_80028850" and args and args[0] != 0:
                address = args[0]
                extra = tuple(self.memory.read(address + offset, 4) for offset in (0, 4))
                prev = self.events[-1]
                self.events[-1] = (prev[0], prev[1], extra)
            if name == "CdMgrUpdate":
                self.cd_updates += 1
                if self.context.get("clear_cd_on_update", False):
                    self.memory.write(ARC_STATE, 0, 4)
            if name == "LoadListStep":
                sequence = self.context.get("load_list_sequence")
                if sequence:
                    result = sequence[min(self.load_index, len(sequence) - 1)]
                    self.load_index += 1
            return result

        argc = {
            "DebugTextPrintf": 4,
            "SeqTrackSetPlayState": 2,
            "SeqTrackClose": 1,
            "SoundBankFree": 1,
            "SoundStopAllVoices": 1,
        }.get(name, 1)
        values = args[:argc]
        self.record(name, values)
        if name == "SeqTrackClose":
            return self._sequenced(name, 0)
        if name == "SoundBankFree":
            return self._sequenced(name, 0)
        if name == "IsTimeAttack":
            return self.context.get("time_attack", 0)
        if name == "SpuWaitIdle":
            return self.context.get("spu_idle", 0)
        if name == "RenderScreenFlash":
            changed_exit_kind = self.context.get("render_exit_kind")
            if changed_exit_kind is not None:
                self.memory.write(FADE_STATE + 0x12C, changed_exit_kind, 1)
            return 0
        if name == "RenderFadeSpiral":
            if self.memory.ram is None:
                cursor = self.memory.read(ORDER_TABLE_CURSOR, 4)
                self.memory.write(ORDER_TABLE_CURSOR, cursor + 32 * 24, 4)
            return 0
        return 0

    def _sequenced(self, name, default):
        sequence = self.context.get(name + "_results")
        if not sequence:
            return default
        index = self.result_index.get(name, 0)
        self.result_index[name] = index + 1
        return sequence[min(index, len(sequence) - 1)]


def build_dll(source=None, tag="stagefade", trapv=False):
    OUT.mkdir(parents=True, exist_ok=True)
    dll = OUT / f"{tag}_{os.getpid()}_{time.time_ns()}.dll"
    exports = OUT / f"{tag}_{os.getpid()}_{time.time_ns()}_exports.c"
    exports_def = OUT / f"{tag}_{os.getpid()}_{time.time_ns()}_exports.def"
    wrappers = [
        "__declspec(dllexport) AeRam *stagefade_ram_create(void) { return ae_ram_create(); }",
        "__declspec(dllexport) void *stagefade_ram_ptr(AeRam *r, uint32_t a, uint32_t n) { return ae_ptr(r, a, n); }",
        "__declspec(dllexport) int32_t stagefade_ram_failed(AeRam *r) { return ae_ram_failed(r); }",
        "__declspec(dllexport) const char *stagefade_ram_error(AeRam *r) { return ae_ram_error(r); }",
        "__declspec(dllexport) void stagefade_ram_clear_error(AeRam *r) { ae_ram_clear_error(r); }",
    ]
    for name, (_address, _size, ret, arg_kinds, takes_hooks) in FUNCS.items():
        parameters = ["AeRam *ram"]
        arguments = ["ram"]
        if takes_hooks:
            parameters.append("const AeStageFadeHooks *hooks")
            arguments.append("hooks")
        for index, kind in enumerate(arg_kinds):
            ctype = "int32_t" if kind == "s" else "uint32_t"
            parameters.append(f"{ctype} arg{index}")
            arguments.append(f"arg{index}")
        ctype = "void" if ret == "v" else "int32_t"
        wrappers.append(
            f"__declspec(dllexport) {ctype} stagefade_call_{name}"
            f"({', '.join(parameters)}) {{ "
            f"{'return ' if ret != 'v' else ''}ae_{name}({', '.join(arguments)}); }}")
    exports.write_text(
        '#include "ae_stagefade.h"\n' + "\n".join(wrappers) + "\n",
        encoding="utf-8")
    exports_def.write_text(
        "EXPORTS\n" +
        "\n".join([" stagefade_ram_" + name for name in
                   ("create", "ptr", "failed", "error", "clear_error")] +
                  [" stagefade_call_" + name for name in FUNCS]) + "\n",
        encoding="utf-8")
    # Link the same real module closure as the game host. Misc now dispatches
    # to the merged collider ports; no link-only gameplay stand-ins are needed.
    import check_game as game_check
    from game_call_inventory import ledger
    modules = {row['source'] for row in ledger()}
    modules.update(game_check.SOURCES)
    modules.discard('src/world/ae_stagefade.c')
    sources = [ROOT / relative for relative in sorted(modules)]
    sources += [Path(source or ROOT / 'src/world/ae_stagefade.c'), exports]
    compiler = shutil.which("gcc")
    compile_flags = [
        "-std=c11", "-O1" if trapv else "-O2",
        *( ["-ftrapv"] if trapv else [] ), "-Wall", "-Wextra", "-Werror",
        "-ffunction-sections", "-fdata-sections", "-fvisibility=hidden",
        "-I" + str(ROOT / "src/world"), "-I" + str(ROOT / "src/player"),
        "-I" + str(ROOT / "src/sound"), "-I" + str(ROOT / "src/monkeys"),
        "-I" + str(ROOT / "src/bodies"),
    ]
    cache = OUT / "stagefade_objects"
    cache.mkdir(parents=True, exist_ok=True)
    objects = []
    headers = b"".join(path.read_bytes() for path in sorted((ROOT / "src").rglob("*.h")))
    for index, path in enumerate(sources):
        contents = Path(path).read_bytes()
        fingerprint = hashlib.sha256(
            contents + headers + " ".join(compile_flags).encode("utf-8")).hexdigest()[:16]
        obj = cache / f"{index:02d}_{Path(path).stem}_{fingerprint}.o"
        if not obj.exists():
            subprocess.run([compiler, *compile_flags, "-c", str(path), "-o", str(obj)],
                           check=True)
        objects.append(str(obj))
    subprocess.run([
        compiler, "-Wl,--gc-sections", "-Wl,--exclude-all-symbols",
        "-shared", "-static-libgcc", *objects, str(exports_def), "-o", str(dll),
    ], check=True)
    return dll


def make_sound_setup(rng, active_bgm):
    def setup(m):
        for offset, value in ((0, 3), (2, 2), (4, 1), (6, 0), (0x16, 0)):
            m.write16(SOUND_BANKS + offset,
                      value if active_bgm or offset != 6 else 0xFFFF)
        if not active_bgm:
            for offset in (0, 2, 4, 6, 0x16):
                m.write16(SOUND_BANKS + offset, -1)
        else:
            # BgmSeqSetVolume is a verified direct call from SoundStop. Give it
            # a real track-shaped record and valid channel pointers; the track
            # has no active voices in this scenario.
            track = 0x800F5AC0
            sequence = SEQ_BASE
            m.write8(track + 0x24, 1)
            m.write8(track + 0x20, 2)
            m.write32(track + 0x10, sequence)
            m.write8(sequence, rng.randrange(1, 0x80))
            for channel in range(16):
                record_pointer = sequence + 0x200 + channel * 0x10
                m.write32(sequence + 0x10 + channel * 0x10, record_pointer)
                m.write8(record_pointer + 3, rng.randrange(0x80))
        for index in range(24):
            entry = LOOP_TABLE + 0x1C * index
            for off in (0, 2, 0x12, 0x14, 0x16):
                m.write16(entry + off, rng.getrandbits(16))
    return setup


def setup_fade(m, state, timer, exit_kind=0, level=1, area=1,
               cd_state=0, load_member=None):
    m.write16(FADE_STATE + 0x124, level)
    m.write16(FADE_STATE + 0x126, area)
    m.write16(FADE_STATE + 0x12A, 3)
    m.write8(FADE_STATE + 0x12C, exit_kind)
    m.write16(FADE_STATE + 0x130, state)
    m.write16(FADE_STATE + 0x132, timer)
    m.write32(FADE_FRAME_COUNTER, 0x12345678)
    m.write32(ARC_STATE, cd_state)
    if load_member is not None:
        record_list = 0x800EFA00
        m.write32(ARC_STATE + ARC_RECORDS, record_list)
        m.write32(ARC_STATE + ARC_RECORD_INDEX, 0)
        m.write16(record_list, load_member)
        m.write32(record_list + 4, 0x800)


def setup_tim(m, count=8, pointer_count=8, ages=None):
    m.write8(TIM_COUNT, count)
    ages = ages or [0, 1, 78, 79, 80, 254, 255, 0]
    for i in range(pointer_count):
        m.write32(TIM_POINTERS + i * 4, 0x80160000 + i * 0x20)
        m.write16(TIM_IDS + i * 2, (i * 3 + 1) & 0xFFFF)
        m.write8(TIM_AGES + i, ages[i % len(ages)])


def scenario_table(h, selected, count, seed):
    rng = random.Random(seed)
    rows = {name: [] for name in selected}

    def add(name, args=(), setup=None, context=None, area=1, initial_regs=None):
        if name in rows:
            rows[name].append((args, setup, context, area, initial_regs))

    areas = (1, 2, 5, 24, 53)
    for index, area in enumerate(areas):
        member = h.area_members[area]
        for args_state in (
            (1, 2, 1, 1, 0),       # flash in still running
            (1, 1, 1, 1, 0),       # completed exit to next area
            (1, 1, 1, 33, 0),      # time-station/clear action selected by level
            (1, 1, 13, 1, 0),      # completed area load and HUD starts
            (1, 1, 2, 1, 0),       # completed request action 3
            (1, 0, 0x7F, 1, 0),    # invalid exit kind clears the fade state
            (2, 0, 0, 1, 0),       # fade-out counter below terminal step
            (2, 47, 0, 1, 0),      # fade-out terminal step
            (2, 0x7FFF, 0, 1, 0),  # signed timer wrap remains below terminal step
            (3, 0, 0, 1, 0),       # reload counter below terminal step
            (3, 47, 0, 1, 0),      # queue a resident area read
            (3, 47, 0, 1, 1),      # pending CD manager and two load-list steps
            (3, 47, 0, 1, 0),      # reload a special area, SoundStop(2) path
            (4, 1, 0, 1, 0),       # spiral fade still running
            (4, 0, 0, 1, 0),       # spiral fade terminal step
            (4, 25, 0, 1, 0),      # timer clamped in FadeSpiralDraw before decrement
            (0, 0, 0, 1, 0),       # no fade
            (5, 2, 0, 1, 0),       # out-of-range state leaves state untouched
        ):
            state, timer, exit_kind, level, cd_mode = args_state
            area_id = areas[(index + len(rows.get("StageFadeUpdate", ()))) % len(areas)]
            member_id = h.area_members[area_id]
            cd_state = 0x50000 if state == 3 and cd_mode else 0
            setup = lambda m, s=state, t=timer, k=exit_kind, l=level, a=area_id, cd=cd_state, mem=member_id: \
                setup_fade(m, s, t, k, l, a, cd, mem if s == 3 and t >= 47 else None)
            context = {"time_attack": 1 if (state == 1 and exit_kind == 1 and level == 1 and index == 2) else 0,
                       "clear_cd_on_update": state == 3 and cd_mode != 0,
                       "load_list_sequence": [0, -1] if state == 3 and cd_mode else [-1],
                       "load_list_step": -1}
            if state == 3 and cd_mode == 0 and index % 5 == 0:
                context["load_list_sequence"] = [0, -1]
            add("StageFadeUpdate", (), setup, context, area_id)

    if "StageFadeUpdate" in rows:
        add("StageFadeUpdate", (),
            lambda m: setup_fade(m, 1, 1, 1, 1, 1),
            {"time_attack": 0, "render_exit_kind": 13}, 1)
        while len(rows["StageFadeUpdate"]) < count:
            area = rng.choice(areas)
            state = rng.choice((0, 1, 2, 3, 4, 5))
            timer = rng.choice((0, 1, 2, 24, 46, 47, 48, 0x7FFF, 0xFFFF,
                                rng.randrange(0x10000)))
            exit_kind = rng.choice((0, 1, 2, 3, 13))
            level = rng.choice((1, 33, 31, rng.randrange(0x10000)))
            member = h.area_members[area]
            setup = lambda m, s=state, t=timer, k=exit_kind, l=level, a=area, mem=member: \
                setup_fade(m, s, t, k, l, a, 0, mem if s == 3 and t >= 47 else None)
            add("StageFadeUpdate", (), setup,
                {"time_attack": rng.randrange(2), "load_list_step": -1}, area)

    def direct_cases(name, directed, setup_factory=None):
        if name not in rows:
            return
        for args, setup, context, area, regs in directed:
            add(name, args, setup, context, area, regs)
        while len(rows[name]) < count:
            index = len(rows[name])
            if setup_factory is not None:
                args, setup, context, area, regs = setup_factory(rng, index)
                add(name, args, setup, context, area, regs)
            else:
                area = rng.choice((1, 2, 5))
                kinds = FUNCS[name][3]
                args = tuple(s32(rng.getrandbits(32)) if kind == "s" else
                             rng.getrandbits(32) for kind in kinds)
                add(name, args, None, None, area)

    fade_draw = []
    for timer in (-32768, -1, 0, 1, 12, 24, 25, 48, 0x7FFF):
        fade_draw.append(((), lambda m, t=timer: m.write16(0x800F4482, t), None, 1, None))
    direct_cases("ScreenFlashFadeDraw", fade_draw)

    spiral = []
    for timer in (-32768, -1, 0, 1, 24, 25, 26, 48, 0x7FFF):
        spiral.append(((), lambda m, t=timer: m.write16(0x800F4482, t), None, 1, None))
    direct_cases("FadeSpiralDraw", spiral)

    sound_rows = []
    for mode, active in ((0, True), (1, True), (2, True), (3, True),
                         (0, False), (1, False), (3, False), (-1, False),
                         (4, False)):
        sound_rows.append(((mode,), make_sound_setup(rng, active),
                           {"SeqTrackClose_results": [-1, 0],
                            "SoundBankFree_results": [-1, 0]}, 1, None))
    direct_cases("SoundStop", sound_rows,
        lambda g, i: ((g.choice((0, 1, 2, 3, 4, -1)),),
                      make_sound_setup(g, bool(i & 1)),
                      {"SeqTrackClose_results": [-1, 0],
                       "SoundBankFree_results": [-1, 0]}, g.choice((1, 2, 5)), None))

    loop_cases = []
    for i in range(8):
        def reset_loops(m, seed=seed + i):
            local = random.Random(seed)
            for slot in range(24):
                address = LOOP_TABLE + slot * 0x1C
                for off in range(0, 0x1C, 2):
                    m.write16(address + off, local.getrandbits(16))
        loop_cases.append(((), reset_loops, None, areas[i % len(areas)], None))
    direct_cases("SoundLoopsReset", loop_cases)

    tim_cases = [
        ((0, -1), lambda m: setup_tim(m, 8), None, 1, None),
        ((0, 0), lambda m: setup_tim(m, 8), None, 1, {7: 0x12345678}),
        ((0, 7), lambda m: setup_tim(m, 8, ages=[79] * 8), None, 2,
         {7: 0xA5A55A5A}),
        ((1, -1), lambda m: setup_tim(m, 8), None, 1, {7: 0xA5A55A5A}),
        ((-1, -1), lambda m: setup_tim(m, 8), None, 1, {7: 0xFFFFFFFF}),
        ((8, -1), lambda m: setup_tim(m, 8), None, 1, {7: 0}),
        ((0x80170000, 42), lambda m: setup_tim(m, 0), None, 1, None),
        ((0x80170020, -2), lambda m: setup_tim(m, 7), None, 2, None),
        ((0x80170040, 32767), lambda m: setup_tim(m, 7), None, 5, None),
        ((0x80170060, 0), lambda m: setup_tim(m, 255, 8), None, 1, None),
        ((0x80170080, 1), lambda m: setup_tim(m, 7), None, 1, None),
    ]
    direct_cases("TimListResidentUpdate", tim_cases,
        lambda g, i: ((g.choice((0, 1, 7, 8, -1, 0x80170000 + i * 0x20)),
                       g.choice((-1, -2, 0, 1, 80, 0x7FFF))),
                      lambda m, count=g.randrange(0, 9): setup_tim(m, count),
                      None, g.choice((1, 2, 5)), {7: g.getrandbits(32)}))

    hud_cases = [((mode,), None, None, 1, None) for mode in (0, 1, -1, 2, 0x7FFFFFFF)]
    direct_cases("HudPanelA_Start", hud_cases)

    for name, cases in rows.items():
        if name not in selected:
            continue
        for args, setup, context, area, initial_regs in cases:
            h.run(name, args, setup, context, area, initial_regs)
    return rows


MUTATIONS = [
    ("Flash shade", "(uint32_t)shade, 0u, 0u, 0u", "(uint32_t)(shade + 1), 0u, 0u, 0u", "ScreenFlashFadeDraw", (),
     lambda m: m.write16(0x800F4482, 12), 1),
    ("Flash wrong hook", "h != NULL ? h->RenderScreenFlash : NULL", "h != NULL ? h->RenderFadeSpiral : NULL", "ScreenFlashFadeDraw", (),
     lambda m: m.write16(0x800F4482, 12), 1),
    ("Flash helper arg", "stagefade_render_screen_flash(r, h, shade);", "stagefade_render_screen_flash(r, h, shade + 1);", "ScreenFlashFadeDraw", (),
     lambda m: m.write16(0x800F4482, 12), 1),
    ("Fade action", "time_attack != 0 ||", "time_attack == 0 ||", "StageFadeUpdate", (),
     (lambda m: setup_fade(m, 1, 1, 1, 1, 1), {"time_attack": 0}), 1),
    ("Fade timer step", "AE_STAGEFADE_STATE_FADE_TIMER) - 1u);", "AE_STAGEFADE_STATE_FADE_TIMER) + 1u);", "StageFadeUpdate", (),
     (lambda m: setup_fade(m, 1, 2, 1, 1, 1), {}), 1),
    ("Fade invalid kind", "initial_exit_kind != 1u && initial_exit_kind != 13u &&\n            initial_exit_kind != 2u", "initial_exit_kind != 1u && initial_exit_kind != 13u", "StageFadeUpdate", (),
     (lambda m: setup_fade(m, 1, 2, 2, 1, 1), {}), 1),
    ("Flash ratio", "(timer * 255) / 48", "(timer * 255) / 24", "ScreenFlashFadeDraw", (),
     lambda m: m.write16(0x800F4482, 12), 1),
    ("Flash scale", "(timer * 255) / 48", "(timer * 254) / 48", "ScreenFlashFadeDraw", (),
     lambda m: m.write16(0x800F4482, 48), 1),
    ("Flash clear", "stagefade_render_screen_flash(r, h, shade);", "stagefade_render_screen_flash(r, h, shade + 1);", "ScreenFlashFadeDraw", (),
     lambda m: m.write16(0x800F4482, 12), 1),
    ("Spiral threshold", "if (timer >= 25)", "if (timer > 25)", "FadeSpiralDraw", (),
     lambda m: m.write16(0x800F4482, 25), 1),
    ("Spiral clamp value", "timer = 24;", "timer = 23;", "FadeSpiralDraw", (),
     lambda m: m.write16(0x800F4482, 30), 1),
    ("Spiral renderer arg", "(uint32_t)timer, 0u, 0u, 0u);", "(uint32_t)(timer + 1), 0u, 0u, 0u);", "FadeSpiralDraw", (),
     lambda m: m.write16(0x800F4482, 4), 1),
    ("Sound volume", "ae_BgmSeqSetVolume(r, h->sound_hw, bgm_track, 0);", "ae_BgmSeqSetVolume(r, h->sound_hw, bgm_track, 1);", "SoundStop", (0,), make_sound_setup(random.Random(1), True), 1),
    ("Sound slot clear", "if (mode != 1)", "if (mode == 1)", "SoundStop", (0,), make_sound_setup(random.Random(2), True), 1),
    ("Sound stop flag", "\"SoundStopAllVoices\", 1u, 0u);", "\"SoundStopAllVoices\", 0u, 0u);", "SoundStop", (0,), make_sound_setup(random.Random(3), True), 1),
    ("Loops active", "AE_STAGEFADE_SOUND_LOOP_ACTIVE, 0u", "AE_STAGEFADE_SOUND_LOOP_ACTIVE, 1u", "SoundLoopsReset", (), None, 1),
    ("Loops index", "AE_STAGEFADE_SOUND_LOOP_INDEX, 0xFFFFu", "AE_STAGEFADE_SOUND_LOOP_INDEX, 0u", "SoundLoopsReset", (), None, 1),
    ("Loops flags", "AE_STAGEFADE_SOUND_LOOP_FLAGS, 0u", "AE_STAGEFADE_SOUND_LOOP_FLAGS, 2u", "SoundLoopsReset", (), None, 1),
    ("Tim age limit", "age < AE_STAGEFADE_TIM_MAX_AGE", "age <= AE_STAGEFADE_TIM_MAX_AGE", "TimListResidentUpdate", (0, 0),
     lambda m: setup_tim(m, 8, ages=[79] * 8), 1),
    ("Tim saturate", "age != 0u && age != 0xFFu", "age != 0u", "TimListResidentUpdate", (0, 0),
     lambda m: setup_tim(m, 8, ages=[255] * 8), 1),
    ("Tim id stride", "i * AE_STAGEFADE_TIM_ID_STRIDE);", "i * AE_STAGEFADE_TIM_POINTER_STRIDE);", "TimListResidentUpdate", (0, 0),
     lambda m: setup_tim(m, 8), 1),
    ("Panel timer", "mode == 0 ? 40u : 0u", "mode == 0 ? 39u : 0u", "HudPanelA_Start", (0,), None, 1),
    ("Panel state", "mode == 0 ? 0u : 2u", "mode == 0 ? 0u : 1u", "HudPanelA_Start", (1,), None, 1),
    ("Panel flags", "| 4u);", "| 8u);", "HudPanelA_Start", (1,), None, 1),
]


def run_mutations(cases, seed):
    source_path = ROOT / "src/world/ae_stagefade.c"
    original = source_path.read_text(encoding="utf-8")
    failures = []
    for index, (label, old, new, name, args, setup_data, area) in enumerate(MUTATIONS):
        if old not in original:
            failures.append(f"{label} (pattern missing)")
            continue
        with tempfile.TemporaryDirectory(prefix="stagefade_mut_", dir=OUT) as tmp_name:
            variant = Path(tmp_name) / "ae_stagefade.c"
            variant.write_text(original.replace(old, new, 1), encoding="utf-8")
            try:
                h = FadeHarness(variant, f"stagefade_mut{index}_{os.getpid()}")
            except subprocess.CalledProcessError as error:
                failures.append(f"{label} (mutation build failed: {error.returncode})")
                continue
            setup = context = None
            if isinstance(setup_data, tuple):
                setup, context = setup_data
            else:
                setup = setup_data
            try:
                h.run(name, args, setup, context, area)
                caught = False
            except AssertionError:
                caught = True
            if not caught:
                failures.append(label)
            print(f"mutation {label:28s} {'caught' if caught else 'SURVIVED'}", flush=True)
    if failures:
        print("SURVIVORS / invalid mutations: " + ", ".join(failures))
        return 1
    print(f"all {len(MUTATIONS)} mutations caught")
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=int, default=32)
    parser.add_argument("--seed", type=lambda value: int(value, 0), default=0x944241)
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--only", nargs="*", default=[])
    parser.add_argument("--trapv", action="store_true")
    args = parser.parse_args()
    selected = args.only or list(FUNCS)
    unknown = sorted(set(selected) - set(FUNCS))
    if unknown:
        parser.error("unknown function(s): " + ", ".join(unknown))
    if not (ROOT / "extract/disc/SCUS_944.23").exists():
        print("SKIP: extract/disc/SCUS_944.23 not present")
        return 0
    if args.mutate:
        return run_mutations(None, args.seed)

    started = time.time()
    harness = FadeHarness(trapv=args.trapv)
    rows = scenario_table(harness, selected, max(args.cases, 1), args.seed)
    is_default = (args.cases == parser.get_default("cases") and
                  args.seed == parser.get_default("seed") and
                  not args.only and not args.trapv)
    summary = harness.finish(EXTRA_COVERAGE, default_run=is_default,
                             show_coverage=args.coverage, selected=selected)
    total_cases = 0
    for name in selected:
        count = harness.counts[name]
        total_cases += count
        row = summary[name]
        print(f"{name:28s} {count:4d} cases; {row['executed']}/{row['total']} instructions")
    print(f"total {total_cases} cases; {harness.m.steps} MIPS steps; "
          f"{time.time() - started:.1f}s")
    if harness.failures:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

