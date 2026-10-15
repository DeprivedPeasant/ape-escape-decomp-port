"""Diff stage request, progress, area and CD-queue ports against the real EXE."""
import argparse
import ctypes as C
import os
from pathlib import Path
import re
import random
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import exclusion_registry as reg
import mips_ram as mr
import dump_areas

FUNCS = {
    "AreaLoadStartSeek": (0x80010D04, 124, "s", "s", True),
    "AreaLoadQueueRead": (0x80011208, 220, "s", "", True),
    "AreaLoadStart": (0x800114D8, 284, "s", "s", True),
    "AreaLoadGridAndProps": (0x8003244C, 700, "v", "", True),
    "AreaLoad": (0x80031B60, 2200, "s", "ss", True),
    "StageRequest": (0x8005D100, 1936, "s", "sss", True),
    "StageRequestUpdate": (0x8005D890, 1000, "s", "", True),
    "StageRequestRetry": (0x800955C8, 56, "s", "", True),
    "StageRequestExit": (0x80094F20, 32, "s", "", True),
    "StageProgressStoreArea": (0x80096E0C, 416, "s", "s", True),
    "StageProgressOnExit": (0x80096FAC, 820, "s", "s", True),
    "StageProgressOnEnter": (0x800966B4, 1880, "s", "s", True),
    "StageProgressRestore": (0x800972E0, 372, "s", "s", True),
    "StageProgressRestore_Nop": (0x80050AFC, 8, "v", "", False),
    "StageProgressRestore_Nop2": (0x800848B0, 8, "v", "", False),
}
HOOKS = [
    ("AreaUsesAltMember", 0x800115F4, 2),
    ("ArcGetMemberPos", 0x80012C0C, 2),
    ("FUN_80025E24", 0x80025E24, 1),
    ("CdMgrQueueCommand", 0x80012360, 4),
    ("StageExitStart", 0x8004D73C, 0),
    ("IsTimeAttack", 0x80097678, 0),
    ("GetLevelFirstArea", 0x8009764C, 1),
    ("GimmickSaveStates", 0x8005ADF8, 0),
    ("GimmickSaveApplyProgress", 0x8005B480, 0),
    ("ItemSaveCollectedMask", 0x800847EC, 0),
    ("ZakoSaveDefeatedMask", 0x800509CC, 0),
    ("AreaGetKind", 0x80010CA4, 1),
    ("AreaGetLevel", 0x80010CC0, 1),
    ("GimmickSaveTableReset", 0x8005AD90, 0),
    ("ItemsClearFlags", 0x800847C4, 0),
    ("ZakoClearSlotTable", 0x800509A4, 0),
    ("SoundSilence", 0x80086148, 0),
    ("LevelMonkeyCountGet", 0x800965EC, 1),
    ("AreaBumpAlloc", 0x8008C1B8, 1),
    ("MeshInit", 0x80015C34, 2),
    ("GetZoneMask", 0x8004626C, 4),
    ("FUN_80030890", 0x80030890, 4),
    ("FUN_800A3700", 0x800A3700, 1),
    ("HeapInit", 0x8008C01C, 0),
    ("AreaBumpAllocSet", 0x8008C058, 1),
    ("AreaSpecialFlagsClear", 0x8007AFB8, 0),
    ("AreaSetVramAnimList", 0x800690E8, 1),
    ("AreaSetZoneBoxes", 0x80043380, 1),
    ("IndicatorsReset", 0x800715A0, 0),
    ("AreaSetFeature30", 0x8009B7C8, 1),
    ("AreaSetSpecialActor", 0x8009B3CC, 2),
    ("AreaSetIndicatorModelA", 0x8007163C, 1),
    ("AreaSetIndicatorModelB", 0x80071694, 1),
    ("AreaSetFeature31", 0x8009B7E4, 1),
    ("AreaSetFeature32", 0x8009B7F0, 1),
    ("SoundAreaKeepsMusic", 0x80086F00, 1),
    ("BgmStart", 0x80087388, 0),
    ("BgmUpdateVolume", 0x8008745C, 0),
    ("ApplyLevelFog", 0x800323F8, 1),
    ("SetColorMatrixMode", 0x80015D58, 4),
    ("SoundLoopsStopAll", 0x80086540, 0),
    ("BuildTerrainBlockMask", 0x800438F8, 0),
    ("TableClearDAT_800BEF60", 0x8007D0F0, 0),
    ("DebugMenuInitPlayerParams", 0x8005CB1C, 0),
    ("DebugMenuInitCounters", 0x8005CC58, 0),
    ("DebugMenuInitCounters2", 0x8005CCDC, 0),
    ("MessageBoxClose", 0x8008DEE0, 0),
    ("PlayerSpawnAtEntry", 0x80032BBC, 0),
    ("TargetListClear", 0x8007CD60, 0),
    ("ZakoInit", 0x800505C8, 0),
    ("MonkeyLodModelsInit", 0x8007BCB4, 0),
    ("FxSlotsReset", 0x80084A60, 0),
    ("ProjectilesReset", 0x800592E0, 0),
    ("EffectsReset", 0x80069500, 0),
    ("ColliderZoneMasksUpdate", 0x80088670, 0),
    ("GimmickSpawnAll", 0x8005B538, 0),
    ("BossInit", 0x8005AB90, 0),
    ("ItemsLoadForArea", 0x80083470, 0),
    ("AreaTexCopyInit", 0x8005E7EC, 4),
    ("RcCarDespawn", 0x8009946C, 0),
    ("PlayerSelectGadget", 0x8007AC64, 2),
    ("HudFindGadgetIconPos", 0x8008197C, 0),
    ("MonkeyClearWords", 0x80032708, 2),
    ("VSync", 0x8002E520, 1),
    ("Printf", 0x800A3720, 4),
    ("CutsceneModeOff", 0x8005DD28, 0),
    ("CutsceneModeOn", 0x8005DD18, 0),
    ("PlayerInit", 0x8003272C, 0),
    ("PlayerSetMode", 0x8007B374, 1),
    ("FUN_80028850", 0x80028850, 4),
    ("FUN_800286CC", 0x800286CC, 1),
    ("WaitFrameFlip", 0x8005DE8C, 0),
    ("BgmFadeOut", 0x8008760C, 0),
    ("SoundStop", 0x80086FD0, 1),
    ("PlaySfx", 0x800877F8, 1),
    ("ProgressSetByte", 0x80096360, 2),
    ("PlayTimeAdd", 0x8005DD44, 1),
    ("StageGradeUpdate", 0x80097BFC, 0),
    ("StageVictoryStart", 0x8009543C, 0),
    ("StageTypeClearInit", 0x80094F48, 0),
    ("StageTypeClearUpdate", 0x80095030, 0),
    ("StageIntroStart", 0x8009546C, 0),
    ("PlayerEnterCutsceneMode", 0x80095588, 0),
    ("StageIntroUpdate", 0x800954C4, 0),
    ("GimmickNopUpdate3", 0x80094F40, 0),
    ("GimmickNopUpdate4", 0x80095464, 0),
    ("XaPlay", 0x80011F6C, 1),
    ("AreaCheckExits", 0x80010D80, 1),
    ("CdMgrUpdate", 0x800125B8, 0),
    ("FadeFrame", 0x8007A14C, 0),
    ("LoadListStep", 0x800116C0, 0),
    ("FUN_80096638", 0x80096638, 1),
    ("FUN_8013EA24", 0x8013EA24, 0),
    ("AimRequestClear", 0x80048430, 0),
    ("AimModeLeave", 0x8004B510, 0),
]
HookFn = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, *([C.c_uint32] * 4))
STACK_LO = 0x1FF000
EXTRA_COVERAGE = {}


def build(source=None, tag="stage"):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{os.getpid()}_{time.time_ns()}.dll"
    subprocess.run([
        shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
        "-shared", "-static-libgcc", "-I" + str(ROOT / "src/world"),
        str(ROOT / "src/core/ae_ram.c"),
        str(ROOT / "src/core/ae_sdk.c"),
        str(ROOT / "src/core/ae_sdk_matrix.c"),
        str(ROOT / "src/player/native_player.c"),
        str(ROOT / "src/bodies/ae_bodies.c"),
        *map(str, sorted((ROOT / "src/monkeys").glob("ae_monkey*.c"))),
        str(ROOT / "src/player/native_gadget_slots.c"),
        str(source or ROOT / "src/world/ae_stage.c"), "-o", str(dll),
    ], check=True)
    return dll


class Memory:
    def __init__(self, ram=None, side=None):
        self.ram = ram
        self.side = side

    def read(self, address, size):
        if self.ram is not None:
            return self.ram.read(address, size)
        buf, off = self._buffer(address)
        return int.from_bytes(bytes(buf[off:off + size]), "little")

    def write(self, address, value, size):
        if self.ram is not None:
            return self.ram.write(address, size, value)
        buf, off = self._buffer(address)
        buf[off:off + size] = (value & ((1 << (size * 8)) - 1)).to_bytes(size, "little")

    def _buffer(self, address):
        if mr.SCRATCH_BASE <= address < mr.SCRATCH_BASE + mr.SCRATCH_SIZE:
            return self.side.scratch, address - mr.SCRATCH_BASE
        return self.side.main, address & 0x1FFFFF


class HookModel:
    def __init__(self, memory, context):
        self.memory = memory
        self.context = context
        self.events = []

    def call(self, name, args):
        nargs = next(n for hook, _, n in HOOKS if hook == name)
        args = tuple(value & 0xFFFFFFFF for value in args[:nargs])
        extra = None
        if name == "AreaUsesAltMember":
            result = self.context["alternate"]
        elif name == "ArcGetMemberPos":
            out, member = args
            self.context["last_member_out"] = out
            location = (0x12345 + member * 0x101) & 0xFFFFF
            byte_size = 0x2000 + member * 0x800
            self.memory.write(out, location, 4)
            self.memory.write(out + 4, byte_size, 4)
            result = 0
        elif name == "FUN_80025E24":
            out = args[0]
            extra = tuple(self.memory.read(out + offset, 4) for offset in (0, 4))
            result = self.context["seek_result"]
        elif name == "CdMgrQueueCommand":
            if args[1] != 0:
                if args[0] == 14 and 0x801FF000 <= args[1] < 0x80200000:
                    extra = (self.memory.read(args[1], 1),)
                elif (args[0] == 2 and
                      args[1] == self.context.get("last_member_out")):
                    extra = tuple(self.memory.read(args[1] + offset, 4)
                                  for offset in (0, 4))
                else:
                    extra = tuple(self.memory.read(args[1] + offset, 4)
                                  for offset in (0, 4, 8))
            result = 0
        elif name == "StageExitStart":
            result = self.context.get("exit_result", 0)
        elif name == "IsTimeAttack":
            result = self.context.get("time_attack", 0)
        elif name == "GetLevelFirstArea":
            result = self.context.get("first_area", 1)
        elif name == "AreaGetKind":
            result = self.context.get("area_kind", 0)
        elif name == "AreaGetLevel":
            result = self.context.get("level", 1)
        elif name == "LevelMonkeyCountGet":
            result = self.context.get("monkey_count", 0)
        elif name == "AreaBumpAlloc":
            result = self.context.get("alloc_result", 0x80138000)
        elif name == "MeshInit":
            prop, mesh_entry = args
            self.memory.write(prop, mesh_entry, 4)
            extra = (self.memory.read(prop, 4),)
            result = 0
        elif name == "GetZoneMask":
            result = self.context.get("zone_mask", 0x4321)
        elif name == "SoundAreaKeepsMusic":
            result = self.context.get("keep_music", 0)
        elif name == "AreaCheckExits":
            result = self.context.get(f"exit_check_{args[0]}",
                                      self.context.get("exit_check", -1))
        elif name == "LoadListStep":
            result = self.context.get("load_list_step", -1)
        elif name == "AimRequestClear":
            if self.memory.read(0x800E6902, 1) == 1:
                self.memory.write(0x800E6902, 0, 1)
            result = 0
        elif name == "AimModeLeave":
            if self.memory.read(0x800E6916, 1) != 0:
                self.memory.write(0x800E6916, 0, 1)
                self.call("PlaySfx", (5,))
            result = 0
        elif name == "BuildTerrainBlockMask":
            result = self.context.get("terrain_mask", 0x800E6000)
        elif name == "AreaSpecialFlagsClear":
            self.memory.write(0x800F5120, 0, 4)
            result = 0
        elif name == "MonkeyClearWords":
            address, words = args
            for index in range(words):
                self.memory.write(address + index * 4, 0, 4)
            result = 0
        elif name == "VSync":
            result = self.context.get("vsync", 0x1234)
        elif name == "AreaTexCopyInit":
            pointer, _mode, color, _unused = args
            extra = (tuple(self.memory.read(pointer + offset, 4) for offset in (0, 4)), color)
            result = 0
        elif name in ("PlayerSelectGadget", "HudFindGadgetIconPos"):
            result = 0
        elif name in ("FUN_80030890", "FUN_800A3700", "Printf"):
            result = 0
        elif name in ("GimmickSaveStates", "GimmickSaveApplyProgress",
                      "ItemSaveCollectedMask", "ZakoSaveDefeatedMask",
                      "GimmickSaveTableReset", "ItemsClearFlags", "ZakoClearSlotTable",
                      "SoundSilence"):
            result = 0
        elif name in {
            "HeapInit", "AreaBumpAllocSet", "AreaSetVramAnimList", "AreaSetZoneBoxes",
            "IndicatorsReset", "AreaSetFeature30", "AreaSetSpecialActor",
            "AreaSetIndicatorModelA", "AreaSetIndicatorModelB", "AreaSetFeature31",
            "AreaSetFeature32", "BgmStart", "BgmUpdateVolume", "ApplyLevelFog",
            "SetColorMatrixMode", "SoundLoopsStopAll", "TableClearDAT_800BEF60",
            "DebugMenuInitPlayerParams", "DebugMenuInitCounters", "DebugMenuInitCounters2",
            "MessageBoxClose", "PlayerSpawnAtEntry", "TargetListClear", "ZakoInit",
            "MonkeyLodModelsInit", "FxSlotsReset", "ProjectilesReset", "EffectsReset",
            "ColliderZoneMasksUpdate", "GimmickSpawnAll", "BossInit", "ItemsLoadForArea",
            "RcCarDespawn", "Printf"
            , "CutsceneModeOff", "CutsceneModeOn", "PlayerInit", "PlayerSetMode",
            "FUN_80028850", "FUN_800286CC", "WaitFrameFlip", "BgmFadeOut",
            "SoundStop", "PlaySfx", "ProgressSetByte", "PlayTimeAdd",
            "StageGradeUpdate", "StageVictoryStart", "StageTypeClearInit",
            "StageTypeClearUpdate", "StageIntroStart", "PlayerEnterCutsceneMode",
            "StageIntroUpdate", "GimmickNopUpdate3", "GimmickNopUpdate4", "XaPlay",
            "CdMgrUpdate", "FadeFrame", "FUN_80096638", "FUN_8013EA24"
        }:
            result = 0
        else:
            raise AssertionError(name)
        self.events.append((name, args, extra))
        return result


class CSide:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        P, U = C.c_void_p, C.c_uint32
        self.lib.ae_ram_create.restype = P
        self.lib.ae_ram_destroy.argtypes = [P]
        self.lib.ae_ptr.argtypes = [P, U, U]
        self.lib.ae_ptr.restype = P
        self.lib.ae_ram_failed.argtypes = [P]
        self.lib.ae_ram_failed.restype = C.c_int
        self.lib.ae_ram_error.argtypes = [P]
        self.lib.ae_ram_error.restype = C.c_char_p
        self.lib.ae_ram_clear_error.argtypes = [P]
        self.ram = self.lib.ae_ram_create()
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(
            self.lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(
            self.lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.model = None
        self.callbacks = []
        monkey_header = (ROOT / "src/monkeys/ae_monkey.h").read_text(encoding="utf-8")
        monkey_hook_struct = monkey_header.split("typedef struct AeMonkeyHooks {", 1)[1].split(
            "} AeMonkeyHooks", 1)[0]
        monkey_hook_names = re.findall(r"AeMonkeyHookFn\s+(\w+)\s*;", monkey_hook_struct)
        self.monkey_hooks_type = type("AeMonkeyHooks", (C.Structure,), {
            "_fields_": [("user", C.c_void_p), ("body", C.c_void_p)] +
                        [(name, C.c_void_p) for name in monkey_hook_names]
        })
        self.monkey_hooks = self.monkey_hooks_type()
        self.gadget_select_type = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p,
                                               C.c_int32, C.c_int32)
        self.gadget_hud_type = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p)
        self.gadget_hooks_type = type("AeGadgetHooks", (C.Structure,), {
            "_fields_": [("user", C.c_void_p), ("select_gadget", self.gadget_select_type),
                         ("hud_find_icon_pos", self.gadget_hud_type)]
        })
        self.gadget_hooks = self.gadget_hooks_type()
        self.hooks = type("AeStageHooks", (C.Structure,), {
            "_fields_": [("user", C.c_void_p), ("stack_pointer", C.c_uint32)] +
                        [(name, HookFn) for name, _, _ in HOOKS] +
                        [("monkey_hooks", C.c_void_p), ("gadget_hooks", C.c_void_p)]
        })()
        for name, (_, _, ret, arg_types, has_hooks) in FUNCS.items():
            fn = getattr(self.lib, "ae_" + name)
            fn.argtypes = [P] + ([C.POINTER(type(self.hooks))] if has_hooks else []) + [
                C.c_int32 if kind == "s" else U for kind in arg_types
            ]
            fn.restype = None if ret == "v" else C.c_int32
        for name, _, _ in HOOKS:
            callback = HookFn(
                lambda _user, _ram, a0, a1, a2, a3, n=name:
                    self.model.call(n, (a0, a1, a2, a3)))
            self.callbacks.append(callback)
            setattr(self.hooks, name, callback)
        for name, model_name in (("ClearWords", "MonkeyClearWords"), ("VSync", "VSync")):
            callback = HookFn(
                lambda _user, _ram, a0, a1, a2, a3, n=model_name:
                    self.model.call(n, (a0, a1, a2, a3)))
            self.callbacks.append(callback)
            setattr(self.monkey_hooks, name, C.cast(callback, C.c_void_p))
        gadget_select = self.gadget_select_type(
            lambda _user, _ram, gadget, old:
                self.model.call("PlayerSelectGadget", (gadget, old, 0, 0)))
        gadget_hud = self.gadget_hud_type(
            lambda _user, _ram: self.model.call("HudFindGadgetIconPos", (0, 0, 0, 0)))
        self.callbacks.extend((gadget_select, gadget_hud))
        self.gadget_hooks.select_gadget = gadget_select
        self.gadget_hooks.hud_find_icon_pos = gadget_hud
        self.hooks.monkey_hooks = C.cast(C.pointer(self.monkey_hooks), C.c_void_p)
        self.hooks.gadget_hooks = C.cast(C.pointer(self.gadget_hooks), C.c_void_p)

    def sync(self, mips):
        C.memmove(self.main, bytes(mips.ram), mr.RAM_SIZE)
        C.memmove(self.scratch, bytes(mips.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)

    def invoke(self, name, args):
        fn = getattr(self.lib, "ae_" + name)
        _, _, _, _, has_hooks = FUNCS[name]
        self.hooks.stack_pointer = mr.DEFAULT_SP
        return fn(self.ram, *([C.byref(self.hooks)] if has_hooks else []), *args)


class StageHarness:
    def __init__(self, source=None, tag="stage"):
        reg.declare("stack_window", "801FF000..80200000")
        member = dump_areas.area_members()[1]
        self.m = mr.MipsRam.from_extract(ROOT / f"extract/members/{member:03X}.bin")
        self.m.coverage = set()
        self.base = self.m.snapshot()
        self.c = CSide(build(source, tag))
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0

    def run(self, name, args=(), setup=lambda _m: None, context=None):
        self.m.restore(self.base)
        setup(self.m)
        self.c.sync(self.m)
        context = context or {"alternate": 0, "seek_result": 0}
        original_model = HookModel(Memory(ram=self.m), context)
        c_model = HookModel(Memory(side=self.c), context)
        self.c.model = c_model
        mips_hooks = {
            address: (lambda cpu, regs, n=hook: original_model.call(n, regs[4:8]))
            for hook, address, _ in HOOKS
        }
        self.m.hook_log.clear()
        registers = self.m.call(FUNCS[name][0], args, hooks=mips_hooks)
        result = self.c.invoke(name, args)
        self.counts[name] += 1
        self.compare(name, args, registers[2], result, original_model.events, c_model.events)

    def compare(self, name, args, original, result, original_events, c_events):
        if FUNCS[name][2] == "s" and (result & 0xFFFFFFFF) != original:
            self.fail(name, args, f"return differs: C={result & 0xFFFFFFFF:08X} MIPS={original:08X}")
        expected = bytes(self.m.ram[:STACK_LO])
        actual = C.string_at(self.c.main, STACK_LO)
        if actual != expected:
            index = next(i for i, (a, b) in enumerate(zip(actual, expected)) if a != b)
            self.fail(name, args, f"RAM differs at {0x80000000 + index:08X}: C={actual[index]:02X} MIPS={expected[index]:02X}")
        if bytes(self.c.scratch) != bytes(self.m.scratch):
            self.fail(name, args, "scratchpad differs")
        if original_events != c_events:
            self.fail(name, args, f"hook sequence differs: C={c_events!r}; MIPS={original_events!r}")
        if self.c.lib.ae_ram_failed(self.c.ram):
            self.fail(name, args, self.c.lib.ae_ram_error(self.c.ram).decode(errors="replace"))

    def fail(self, name, args, detail):
        self.failures += 1
        print(f"FAIL {name}{tuple(args)}: {detail}", flush=True)
        raise SystemExit(1)

    def finish(self, show_coverage=False, default_run=False):
        items = [(name, addr, size, True) for name, (addr, size, *_rest) in FUNCS.items()]
        summary = reg.coverage_summary(items, self.m.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        gaps = any(s["uncovered_count"] or s["stale_declared"] for s in summary.values())
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps,
                              function_names=list(FUNCS))
        if gaps:
            raise SystemExit("FAIL: undeclared stage instruction coverage gaps")
        return summary


def setup_load_queue(m, index):
    m.write32(0x800E38AC, 0x800A71B0)
    m.write32(0x800E38B0, index)


def scenarios(h, scale=1.0, seed=94423, mutation_function=None):
    rng = random.Random(seed)
    runs = [
        ("StageProgressRestore_Nop", (), lambda _m: None, {}),
        ("StageProgressRestore_Nop2", (), lambda _m: None, {}),
        ("StageRequestExit", (), lambda _m: None, {"exit_result": rng.randrange(256)}),
    ]
    for first_area, current_area, area_count in ((1, 1, 3), (24, 26, 5), (40, 42, 2)):
        def progress(m, first_area=first_area, current_area=current_area, area_count=area_count):
            state = 0x800F4350
            m.write16(state + 0x124, first_area)
            m.write16(state + 0x126, current_area)
            m.write16(state + 0x128, area_count)
            m.write8(state + 0x166, rng.randrange(256))
            for slot in range(8):
                m.write8(state + 0x16C + slot, rng.randrange(256))
                for area_index in range(area_count):
                    m.write8(0x800DF820 + (first_area + area_index) * 8 + slot,
                             rng.randrange(256))
                    m.write8(0x800DFE00 + area_index * 0x18 + slot,
                             rng.randrange(256))
        runs.append(("StageProgressStoreArea", (current_area,), progress,
                     {"first_area": first_area, "time_attack": 0}))
    runs.append(("StageProgressStoreArea", (26,), lambda _m: None,
                 {"first_area": 24, "time_attack": 1}))
    exit_scenarios = [
        (1, 1, 1, 0, 0, 0),       # ordinary exit, not the time station
        (69, 1, 69, 6, 0, 0),     # save the special area's current byte
        (69, 1, 69, 6, 0, 1),     # time attack skips that one-byte save
        (69, 28, 28, 6, 0, 1),    # special time-attack result table
        (69, 27, 27, 6, 5, 1),    # shift the hub's five per-level rows
        (69, 1, 1, 6, 3, 0),      # copy gimmick rows back to stage progress
        (69, 1, 2, 6, 2, 0),      # mismatched first area
        (69, 1, 1, 4, 0, 0),      # wrong exit kind
        (69, 1, 1, 6, 0, 0),      # no gimmick rows: only the aggregate byte
        (69, 8, 8, 2, 0, 1),      # unsupported time-attack level
    ]
    for call_area, level, current_area, area_kind, area_count, time_attack in exit_scenarios:
        def exit_setup(m, level=level, current_area=current_area,
                       area_kind=area_kind, area_count=area_count):
            state = 0x800F4350
            m.write16(state + 0x124, level)
            m.write16(state + 0x126, current_area)
            m.write16(state + 0x128, area_count)
            m.write8(state + 0x12C, area_kind)
            m.write8(state + 0x166, rng.randrange(256))
            for row in range(6):
                for slot in range(8):
                    m.write8(0x800DFE00 + row * 0x18 + slot, rng.randrange(256))
                    m.write8(0x800DF820 + (current_area + row) * 8 + slot,
                             rng.randrange(256))
        runs.append(("StageProgressOnExit", (call_area,), exit_setup,
                     {"first_area": current_area if call_area == 69 else 1,
                      "time_attack": time_attack}))
    enter_scenarios = [
        (1, 1, 1, 1, 0, 0, 3, 3),
        (28, 28, 28, 1, 19, 1, 5, 8),
        (27, 27, 27, 1, 19, 0, 5, 8),
        (1, 1, 1, 1, 19, 0, 2, 4),
        (69, 69, 69, 1, 0, 0, 4, 1),
        (33, 33, 33, 1, 0, 0, 2, 7),
        (31, 31, 31, 1, 0, 0, 2, 4),
        (32, 32, 32, 1, 0, 0, 2, 4),
        (34, 34, 34, 1, 0, 0, 2, 4),
        (1, 1, 1, 3, 0, 0, 2, 4),
        (24, 24, 24, 1, 0, 0, 1, 0),
        (1, 1, 2, 1, 0, 0, 2, 4),
    ]
    for area, level, first_area, area_kind, mode, progress_type, area_count, monkey_count in enter_scenarios:
        def enter_setup(m, level=level, area_kind=area_kind, mode=mode,
                        progress_type=progress_type, area_count=area_count,
                        first_area=first_area):
            state = 0x800F4350
            m.write16(state + 0x120, mode)
            m.write8(state + 0x12C, progress_type)
            m.write16(state + 0x15C, 1)
            table = 0x80102400
            levels = set(range(24, 30)) | {level}
            for level_id in levels:
                entry = table + level_id * 0x10
                count = area_count if level_id == level else 1
                m.write8(entry, count)
                m.write8(entry + 2, 0x30 + level_id)
                m.write8(entry + 3, level_id & 0x0F)
                m.write8(entry + 4, 0x50 + level_id)
            for row in range(12):
                for slot in range(8):
                    m.write8(0x800DFE00 + row * 0x18 + slot, rng.randrange(256))
                    m.write8(0x800DFF20 + row * 0x18 + slot, rng.randrange(256))
            for row in range(max(area_count, 1) + 2):
                for slot in range(8):
                    m.write8(0x800DF820 + (first_area + row) * 8 + slot,
                             rng.randrange(256))
            m.write8(0x800DFC88, 0x63)
            m.write8(0x800DFCB0, 0x75)
            for level_id in range(70):
                m.write8(0x800DFC70 + level_id, (level_id + 3) & 0xFF)
                m.write8(0x800DFC98 + level_id, (level_id + 7) & 0xFF)
        runs.append(("StageProgressOnEnter", (area,), enter_setup,
                     {"level": level, "first_area": first_area,
                      "area_kind": area_kind, "monkey_count": monkey_count}))
    restore_scenarios = [
        (2, 1, 0, 1),     # restore a later area from the level's first-area record
        (1, 1, 2, 1),     # update the one-time flag without the hub path
        (1, 69, 2, 1),    # enter the saved level from the hub
        (1, 69, 2, 2),    # hub return with a nonmatching first area
        (1, 69, 0, 1),    # hub area but no saved-entry restore
        (69, 1, 2, 69),   # special early reset
    ]
    for area, current_area, progress_type, first_area in restore_scenarios:
        def restore_setup(m, current_area=current_area, progress_type=progress_type):
            state = 0x800F4350
            m.write16(state + 0x120, 0)
            m.write16(state + 0x124, 1)
            m.write16(state + 0x126, current_area)
            m.write8(state + 0x12C, progress_type)
            m.write16(state + 0x15C, 1)
            for row in range(12):
                for slot in range(8):
                    m.write8(0x800DFE00 + row * 0x18 + slot,
                             rng.randrange(256))
            entry = 0x80102410
            m.write8(entry, 1)
            m.write8(entry + 2, 0x31)
            m.write8(entry + 3, 1)
            m.write8(entry + 4, 0x51)
        runs.append(("StageProgressRestore", (area,), restore_setup,
                     {"level": 1, "first_area": first_area,
                      "area_kind": 1, "monkey_count": 3}))
    stage_request_scenarios = [(code, 1, 1, 0, 0) for code in range(1, 35)]
    stage_request_scenarios.extend([
        (1, 1, 86, 0, 0),
        (2, 53, 1, 0, 0),
        (2, 54, 1, 0, 0),
        (2, 1, 1, 0, 1),
        (1, 1, 1, 32, 0),
        (1, 1, 1, 34, 0),
        (0, 1, 1, 0, 0),
        (35, 1, 1, 0, 0),
        (0x4004, 1, 1, 0, 0),
    ])
    for code, area, current_area, initial_type, keep_music in stage_request_scenarios:
        def request_setup(m, current_area=current_area, initial_type=initial_type,
                          action=code):
            state = 0x800F4350
            m.write16(state + 0x120, 0)
            m.write16(state + 0x122, 1)
            m.write16(state + 0x124, 1)
            m.write16(state + 0x126, current_area)
            m.write16(state + 0x128, 1)
            m.write16(state + 0x12A, 0)
            m.write8(state + 0x12C, initial_type)
            m.write16(state + 0x13C, 1)
            m.write16(state + 0x150, 1)
            m.write16(state + 0x158, 254)
            m.write16(0x800EC23E, 1)
            m.write32(0x800E3890, 0)
            m.write32(0x800E3894, 0xFFFFFFFF)
            m.write32(0x800E3898, 0)
            m.write32(0x800E389C, 0)
            m.write32(0x800E38A0, 0)
            m.write32(0x800E38A4, 0xFFFFFFFF)
            m.write32(0x800E38A8, 0)
            if action in (26, 31, 32, 34):
                m.write8(0x800E6902, 1)
                m.write8(0x800E6916, 1)
            monkey_header = 0x80154000 + m.read32(0x801544E4)
            m.write32(monkey_header, 0)
        runs.append(("StageRequest", (code, area, 0), request_setup,
                     {"alternate": 0, "seek_result": 0,
                      "area_kind": 1, "level": 1, "first_area": 1,
                      "monkey_count": 3, "keep_music": keep_music,
                      "alloc_result": 0x80138000, "zone_mask": 0x4321,
                      "terrain_mask": 0x800E6000, "vsync": 0x2222}))

    def setup_stage_flow(m, config):
        state = 0x800F4350
        arc = 0x800E3890
        m.write16(state + 0x120, config.get("mode", 0))
        m.write16(state + 0x122, config.get("area_kind", 1))
        m.write16(state + 0x124, config.get("level", 1))
        m.write16(state + 0x126, config.get("current_area", 1))
        m.write16(state + 0x128, config.get("area_count", 1))
        m.write16(state + 0x12A, config.get("entry", 0))
        m.write16(state + 0x12C, config.get("progress_type", 1))
        m.write16(state + 0x12E, config.get("progress_selector", 0))
        m.write16(state + 0x132, config.get("transition_step", 0))
        m.write16(state + 0x13C, config.get("transition_count", 1))
        m.write16(state + 0x158, config.get("time_station_count", 0))
        m.write16(0x800EC23E, config.get("transition_check_level", 1))
        m.write32(arc, config.get("load_state", 0))
        m.write32(arc + 0x04, config.get("load_status", 0xFFFFFFFF))
        m.write32(arc + 0x08, config.get("read_status", 0))
        m.write32(arc + 0x0C, config.get("exit_input", 0))
        m.write32(arc + 0x10, config.get("last_result", 0xFFFFFFFF))
        m.write32(arc + 0x14, config.get("exit_record", 0))
        m.write32(arc + 0x18, config.get("retry_index", 0))
        m.write32(arc + 0x68, config.get("next_index", 0))
        m.write16(0x800F4476, config.get("area_list_index", 4))
        m.write8(0x800F447F, 0)
        monkey_header = 0x80154000 + m.read32(0x801544E4)
        m.write32(monkey_header, 0)

    request_edges = [
        {"code": 8, "level": 33},
        {"code": 9, "level": 33},
        {"code": 9, "time_station_count": 255},
        {"code": 10, "level": 33},
        {"code": 10, "area_kind": 1, "transition_count": 2},
        {"code": 10, "area_kind": 3, "transition_count": 1},
        {"code": 10, "area_kind": 1, "transition_count": 1, "mode": 19},
        {"code": 10, "area_kind": 1, "current_area": 31, "transition_count": 2},
    ]
    for config in request_edges:
        action = config["code"]
        def edge_request_setup(m, config=config):
            setup_stage_flow(m, {**config, "progress_type": 1})
        edge_context = {"alternate": 0, "seek_result": 0, "area_kind": 1,
                        "level": 1, "first_area": 1, "monkey_count": 3,
                        "alloc_result": 0x80138000, "zone_mask": 0x4321,
                        "terrain_mask": 0x800E6000, "vsync": 0x2222}
        runs.append(("StageRequest", (action, config.get("current_area", 1),
                                      config.get("entry", 0)), edge_request_setup,
                     edge_context))

    alternate_request_config = {"progress_type": 1, "current_area": 1}
    def alternate_request_setup(m):
        setup_stage_flow(m, alternate_request_config)
    runs.append(("StageRequest", (6, 1, 0), alternate_request_setup,
                 {"alternate": 1, "seek_result": 0, "area_kind": 1,
                  "level": 1, "first_area": 1, "time_attack": 0,
                  "monkey_count": 3, "alloc_result": 0x80138000,
                  "zone_mask": 0x4321, "terrain_mask": 0x800E6000,
                  "vsync": 0x2222}))

    retry_config = {"progress_type": 1, "current_area": 1, "entry": 2}
    def retry_setup(m):
        setup_stage_flow(m, retry_config)
    runs.append(("StageRequestRetry", (), retry_setup,
                 {"alternate": 0, "seek_result": 0, "area_kind": 1,
                  "level": 1, "first_area": 1, "monkey_count": 3,
                  "alloc_result": 0x80138000, "zone_mask": 0x4321,
                  "terrain_mask": 0x800E6000, "vsync": 0x2222}))

    update_cases = [
        # Initial cutscene condition, then each recognised transition level.
        ({"progress_type": 3, "transition_check_level": 35,
          "current_area": 1, "transition_count": 1}, {}),
        ({"progress_type": 4, "transition_check_level": 74,
          "current_area": 1, "transition_count": 2}, {}),
        ({"progress_type": 5, "transition_check_level": 45,
          "current_area": 31, "transition_count": 2}, {}),
        ({"progress_type": 3, "transition_check_level": 1}, {}),
        # Progress type reset and action 3 exit-result paths.
        ({"progress_type": 0x8003, "load_state": 0, "load_status": 0xFFFFFFFF},
         {"exit_check_0": -1}),
        ({"progress_type": 3, "load_state": 0, "load_status": 0},
         {"exit_check_0": 0x0201}),
        ({"progress_type": 0xC003, "load_state": 0,
          "load_status": 0xFFFFFFFF, "last_result": 7,
          "area_list_index": 4},
         {"exit_check_0": 0x0201, "seek_result": 6}),
        ({"progress_type": 3, "load_state": 0,
          "load_status": 0xFFFFFFFF, "last_result": 7,
          "area_list_index": 4},
         {"exit_check_0": 0x0201, "seek_result": 7}),
        # Action 4: changed destination, failed secondary exit, list-index
        # retry and completed seek paths.
        ({"progress_type": 4, "exit_record": 0},
         {"exit_check_0": 0x0201}),
        ({"progress_type": 4, "exit_record": 0x0201,
          "load_state": 1},
         {"exit_check_0": 0x0201, "exit_check_1": -1}),
        ({"progress_type": 4, "exit_record": 0x0201,
          "load_state": 1},
         {"exit_check_0": 0x0201, "exit_check_1": 0}),
        ({"progress_type": 4, "exit_record": 0x0201,
          "load_state": 0, "next_index": 1, "retry_index": 0},
         {"exit_check_0": 0x0201, "exit_check_1": 0}),
        ({"progress_type": 4, "exit_record": 0x0201,
          "load_state": 0, "next_index": 0, "retry_index": 0,
          "area_list_index": 4},
         {"exit_check_0": 0x0201, "exit_check_1": 0, "seek_result": 9}),
        # Action 5: mismatch, missing secondary exit, and completed handoff.
        ({"progress_type": 5, "exit_record": 0},
         {"exit_check_0": 0x0201}),
        ({"progress_type": 5, "exit_record": 0x0201},
         {"exit_check_0": 0x0201, "exit_check_1": -1}),
        ({"progress_type": 5, "exit_record": 0x0201,
          "area_list_index": 4},
         {"exit_check_0": 0x0201, "exit_check_1": 0}),
        # Action 6 list statuses and completion, including StageRequest(2).
        ({"progress_type": 6, "read_status": 0xFFFFFFFF}, {}),
        ({"progress_type": 6, "read_status": 3}, {}),
        ({"progress_type": 6, "read_status": 0}, {"load_list_step": 0}),
        ({"progress_type": 6, "read_status": 0}, {"load_list_step": -1}),
        ({"progress_type": 6, "read_status": 2}, {}),
        # The simple transition update cases, including retry.
        ({"progress_type": 18}, {}),
        ({"progress_type": 19}, {}),
        ({"progress_type": 20, "load_state": 0}, {}),
        ({"progress_type": 20, "load_state": 1}, {}),
        ({"progress_type": 24}, {}),
        ({"progress_type": 25}, {}),
    ]
    for config, context_overrides in update_cases:
        def update_setup(m, config=config):
            setup_stage_flow(m, config)
        update_context = {"alternate": 0, "seek_result": 0,
                          "area_kind": 1, "level": 1, "first_area": 1,
                          "monkey_count": 3, "keep_music": 0,
                          "alloc_result": 0x80138000, "zone_mask": 0x4321,
                          "terrain_mask": 0x800E6000, "vsync": 0x2222,
                          "exit_check": -1, "load_list_step": -1}
        update_context.update(context_overrides)
        runs.append(("StageRequestUpdate", (), update_setup, update_context))
    area_load_scenarios = [
        (1, 0, 0, 0, 0, None, None, None),
        (4, 2, 2, 0, 1, [0x08, 0x10, 0x11, 0x20, 0x30, 0x31, 0x32, 0x40], 1, 1),
        (72, 0, 2, 2, 0, [0x41, 0x47, 0, 0x08, 0x10, 0x11, 0x20, 0x30], 1, 0),
        (99, 3, 1, 2, 0, None, 0, 0),
    ]
    for area, entry_index, progress_type, progress_selector, keep_music, features, fog_enabled, texcopy in area_load_scenarios:
        def area_load_setup(m, progress_type=progress_type,
                           progress_selector=progress_selector, features=features,
                           fog_enabled=fog_enabled, texcopy=texcopy):
            state = 0x800F4350
            m.write8(state + 0x12C, progress_type)
            m.write16(state + 0x12E, progress_selector)
            m.write8(0x800F447F, 0)
            header = 0x80154000 + m.read32(0x801544D0)
            if features is not None:
                for index, feature_id in enumerate(features):
                    word = (0x100 << 8) | feature_id
                    m.write32(header + 0x34 + index * 4, word)
            if fog_enabled is not None:
                for i, byte in enumerate((0x5A, 0x6B, 0x7C, 0x8D)):   # distinctive fog colour (area 01's is zero)
                    m.write8(header + 0x20 + i, byte)
                m.write16(header + 0x28, fog_enabled)
                m.write16(header + 0x2C, 1000 if fog_enabled else 1200)
                m.write16(header + 0x2E, 2000 if fog_enabled else 1200)
            if texcopy is not None:
                m.write16(header + 0x2A, texcopy)
            monkey_area_header = 0x80154000 + m.read32(0x801544E4)
            m.write32(monkey_area_header, 0)
        runs.append(("AreaLoad", (area, entry_index), area_load_setup,
                     {"area_kind": 1, "level": 1, "keep_music": keep_music,
                      "vsync": 0x2222, "alloc_result": 0x80138000,
                      "zone_mask": 0x4321, "terrain_mask": 0x800E6000}))
    area_load_path_scenarios = [
        (10, 0, 2, 2, 1, 1, 2000, 1000, 8, 0),
        (72, 1, 2, 2, 1, 1, 2000, 1000, 0, 0),
        (104, 0, 2, 2, 1, 0, 1200, 1200, 0, 0x12345678),
    ]
    for area, entry, progress_type, selector, keep_music, fog_enabled, fog_near, fog_far, sound_size, audio_descriptor in area_load_path_scenarios:
        def area_load_path_setup(m, progress_type=progress_type, selector=selector,
                                 fog_enabled=fog_enabled, fog_near=fog_near,
                                 fog_far=fog_far, sound_size=sound_size,
                                 audio_descriptor=audio_descriptor):
            state = 0x800F4350
            m.write8(state + 0x12C, progress_type)
            m.write16(state + 0x12E, selector)
            m.write8(0x800F447F, 0)
            header = 0x80154000 + m.read32(0x801544D0)
            m.write16(header + 0x28, fog_enabled)
            m.write16(header + 0x2C, fog_near)
            m.write16(header + 0x2E, fog_far)
            m.write16(header + 0x2A, 0)
            m.write32(0x80154504, sound_size)
            m.write32(0x80154240, audio_descriptor)
            m.write32(0x80154000 + m.read32(0x801544E4), 0)
        runs.append(("AreaLoad", (area, entry), area_load_path_setup,
                     {"area_kind": 1, "level": 1, "keep_music": keep_music,
                      "vsync": 0x2222, "alloc_result": 0x80138000,
                      "zone_mask": 0x4321, "terrain_mask": 0x800E6000}))
    grid_scenarios = [
        ([0, 4, 5, 6], False),
        ([3] * 33, False),
        ([1], True),
        ([], False),
    ]
    for mesh_types, skip_heightfield in grid_scenarios:
        def grid_setup(m, mesh_types=mesh_types, skip_heightfield=skip_heightfield):
            state = 0x800F4350
            header = 0x80154000 + m.read32(0x801544D0)
            grid_data = header + m.read32(header + 4) * 4
            if skip_heightfield:
                m.write32(header, m.read32(header) | 2)
            m.write32(state + 0x194, header)
            m.write32(state + 0x198, grid_data)
            m.write32(state + 0x224, 0x80130000)
            m.write32(state + 0x228, 0x80134000)
            m.write32(state + 0x280, len(mesh_types))
            for index, mesh_type in enumerate(mesh_types):
                mesh_index = 64 + mesh_type
                placement = 0x80134000 + index * 12
                m.write16(placement, index)
                m.write16(placement + 2, 0x1234 + index)
                m.write16(placement + 4, index - 16)
                m.write16(placement + 6, 32 - index)
                m.write16(placement + 8, index * 3)
                m.write16(0x80130000 + index * 4 + 4, mesh_index)
                m.write32(0x80130000 + mesh_index * 4, mesh_type)
        runs.append(("AreaLoadGridAndProps", (), grid_setup,
                     {"alloc_result": 0x80138000, "zone_mask": 0x4321}))
    for index in (1, 2):
        for alternate in (0, 1):
            def area(m, index=index, alternate=alternate):
                m.write16(0x800F4476, 1 + index * 3 + alternate)
            runs.append(("AreaLoadStartSeek", (index,), area,
                         {"alternate": alternate, "seek_result": rng.randrange(256)}))
            runs.append(("AreaLoadStart", (index,), area,
                         {"alternate": alternate, "seek_result": rng.randrange(256)}))
    for index in (0, 1):
        runs.append(("AreaLoadQueueRead", (), lambda m, i=index: setup_load_queue(m, i),
                     {"alternate": 0, "seek_result": 0}))
    reps = max(1, round(scale))
    for _ in range(reps):
        index = rng.choice((1, 2, 3, 4))
        alternate = rng.randrange(2)
        context = {"alternate": alternate, "seek_result": rng.randrange(256)}
        def area(m, index=index, alternate=alternate):
            m.write16(0x800F4476, rng.randrange(0x10000))
        runs.append(("AreaLoadStartSeek", (index,), area, context))
        runs.append(("AreaLoadStart", (index,), area, context))
    for name, args, setup, context in runs:
        if mutation_function and name != mutation_function:
            continue
        h.run(name, args, setup, context)


def mutation_sources(original):
    specs = [
        ("StageProgressRestore_Nop", "write one", "(void)r;", "ae_wr32(r, 0x800F447C, 1);"),
        ("StageProgressRestore_Nop", "write two", "(void)r;", "ae_wr32(r, 0x800F447C, 2);"),
        ("StageProgressRestore_Nop2", "write one", "(void)r;", "ae_wr32(r, 0x800F447C, 1);"),
        ("StageProgressRestore_Nop2", "write two", "(void)r;", "ae_wr32(r, 0x800F447C, 2);"),
        ("AreaLoadStartSeek", "alternate member field", "alternate ? 0u : 8u", "alternate ? 8u : 0u"),
        ("AreaLoadStartSeek", "wrong area for alt-member test", "AE_STAGE_STATE_CURRENT_AREA", "AE_STAGE_STATE_CURRENT_AREA + 2u"),
        ("AreaLoadQueueRead", "wrong position command", "STAGE_CALL(CdMgrQueueCommand, 2u", "STAGE_CALL(CdMgrQueueCommand, 3u"),
        ("AreaLoadQueueRead", "wrong record stride", "record_index << 3", "record_index << 2"),
        ("AreaLoadStart", "wrong record index field", "AE_STAGE_ARC_LIST_INDEX, (uint32_t)list_index", "AE_STAGE_ARC_RECORD_INDEX, (uint32_t)list_index"),
        ("AreaLoadStart", "alternate record choice", "if (alternate) ae_wr32(r, state + AE_STAGE_ARC_RECORD_INDEX, 0u);\n    else ae_wr32(r, state + AE_STAGE_ARC_RECORD_INDEX, 1u);", "if (alternate) ae_wr32(r, state + AE_STAGE_ARC_RECORD_INDEX, 1u);\n    else ae_wr32(r, state + AE_STAGE_ARC_RECORD_INDEX, 0u);"),
        ("StageRequestExit", "skip exit-start hook", "STAGE_CALL(StageExitStart, 0, 0, 0, 0);", "(void)r; (void)h;"),
        ("StageRequestExit", "wrong return value", "return 0;", "return 1;"),
        ("StageProgressStoreArea", "invert time-attack guard", "if (STAGE_CALL(IsTimeAttack, 0, 0, 0, 0) != 0)", "if (STAGE_CALL(IsTimeAttack, 0, 0, 0, 0) == 0)"),
        ("StageProgressStoreArea", "wrong progress row stride", "area_index * AE_STAGE_PROGRESS_RECORD_SIZE", "area_index * AE_STAGE_PROGRESS_SLOT_COUNT"),
        ("StageProgressOnExit", "invert time-station dispatch", "if (area != 0x45)", "if (area == 0x45)"),
        ("StageProgressOnExit", "wrong time-attack row shift", "(row + 1u) * AE_STAGE_PROGRESS_RECORD_SIZE", "row * AE_STAGE_PROGRESS_RECORD_SIZE"),
        ("StageProgressOnEnter", "wrong number of area-order entries", "index < area_count * 2u", "index < area_count"),
        ("StageProgressOnEnter", "invert first-area test", "first_area != requested_area ||", "first_area == requested_area ||"),
        ("StageProgressOnEnter", "invert time-attack table clear", "ae_rd16(r, state + AE_STAGE_STATE_MODE) == 19u) {", "ae_rd16(r, state + AE_STAGE_STATE_MODE) != 19u) {"),
        ("StageProgressOnEnter", "wrong level-27 table source row", "(row + 1u) * AE_STAGE_PROGRESS_RECORD_SIZE + slot", "row * AE_STAGE_PROGRESS_RECORD_SIZE + slot"),
        ("StageProgressRestore", "invert special-area reset guard", "requested_area == 0x45u && progress_type == 2u", "requested_area != 0x45u && progress_type == 2u"),
        ("StageProgressRestore", "skip hub entry restoration", "ae_StageProgressOnEnter(r, h, area);", "(void)h;"),
        ("StageProgressRestore", "wrong saved progress row stride", "(uint32_t)area_delta * AE_STAGE_PROGRESS_RECORD_SIZE", "(uint32_t)area_delta * AE_STAGE_PROGRESS_SLOT_COUNT"),
        ("StageProgressRestore", "invert hub area gate", "if (current_area == 0x45u)", "if (current_area != 0x45u)"),
        ("AreaLoadGridAndProps", "wrong grid cell-count pointer", "uint32_t cells = grid_data + AE_STAGE_GRID_CELL_DATA_OFFSET;", "uint32_t cells = grid_data + AE_STAGE_LEVEL_GRID_COLOR_COUNT;"),
        ("AreaLoadGridAndProps", "wrong prop allocation stride", "(uint32_t)prop_count * AE_STAGE_AREA_PROP_SIZE", "(uint32_t)prop_count * AE_STAGE_PROP_PLACEMENT_SIZE"),
        ("AreaLoadGridAndProps", "wrong placement x field", "placement + AE_STAGE_PROP_X));", "placement + AE_STAGE_PROP_Y));"),
        ("AreaLoadGridAndProps", "wrong rotated-prop limit", "rotation_count >= AE_STAGE_ROTATION_LIMIT", "rotation_count > AE_STAGE_ROTATION_LIMIT"),
        ("AreaLoad", "wrong load-mode reset", "ae_wr32(r, AE_STAGE_G_AREA_LOAD_MODE, 0u);", "ae_wr32(r, AE_STAGE_G_AREA_LOAD_MODE, 1u);"),
        ("AreaLoad", "wrong current-area field", "ae_wr16(r, state + AE_STAGE_STATE_CURRENT_AREA, area_index);", "ae_wr16(r, state + AE_STAGE_STATE_AREA_ENTRY, area_index);"),
        ("AreaLoad", "invert music-preservation copy", "if (preserve_music != 0) {", "if (preserve_music == 0) {"),
        ("AreaLoad", "fog colour to the header offset", "ae_wr32(r, grid + AE_STAGE_LEVEL_GRID_FOG_COLOR,", "ae_wr32(r, grid + AE_STAGE_AREA_FOG_COLOR,"),
        ("StageRequest", "allow active transition 34", "if (active_transition == 32u || active_transition == 34u) return 0;", "if (active_transition == 32u) return 0;"),
        ("StageRequest", "drop transition flags when storing action", "(uint32_t)code);", "(uint32_t)action);"),
        ("StageRequest", "skip code-13 area load", "ae_AreaLoad(r, nested_hooks, area, entry);", "(void)area; (void)entry;"),
        ("StageRequestUpdate", "lose transition high flags", "transition_type & 0x7FFFu", "transition_type & 0x3FFFu"),
        ("StageRequestUpdate", "invert secondary-exit success", "if (secondary_exit >= 0) {", "if (secondary_exit < 0) {"),
        ("StageRequestUpdate", "invert list-step completion", "if (step_result >= 0) return 0;", "if (step_result < 0) return 0;"),
        ("StageRequestRetry", "retry wrong action", "ae_StageRequest(r, nested_hooks, 8, area, entry);", "ae_StageRequest(r, nested_hooks, 9, area, entry);"),
        ("StageRequestRetry", "route retry to exit action", "ae_StageRequest(r, nested_hooks, 8, area, entry);", "ae_StageRequest(r, nested_hooks, 6, area, entry);"),
        ("StageRequestRetry", "wrong return value", "return 0;", "return entry;"),
    ]
    for name, label, before, after in specs:
        match = re.search(rf"(?m)^(?:void|int32_t)\s+ae_{re.escape(name)}\s*\(", original)
        assert match is not None, name
        start = match.start()
        end = original.index("\n}\n", start) + 2
        body = original[start:end]
        assert before in body, (name, label, before)
        yield f"{name}: {label}", original[:start] + body.replace(before, after, 1) + original[end:]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--mutate", action="store_true")
    ap.add_argument("--cases", type=float, default=1.0)
    ap.add_argument("--seed", type=int, default=94423)
    args = ap.parse_args()
    if args.mutate:
        original = (ROOT / "src/world/ae_stage.c").read_text(encoding="utf-8")
        mutations = list(mutation_sources(original))
        survivors = []
        for index, (label, source) in enumerate(mutations):
            path = ROOT / f"extract/mips/stage_mutation_{index}.c"
            path.write_text(source, encoding="utf-8")
            function = label.split(":", 1)[0]
            h = StageHarness(path, f"stage_mutation_{index}")
            try:
                scenarios(h, max(args.cases, 1.0), args.seed, function)
            except SystemExit:
                if not h.failures:
                    raise
            else:
                print("SURVIVED", label)
                survivors.append(label)
        print(f"Mutations: {len(mutations)}; caught: {len(mutations)-len(survivors)}; survivors: {len(survivors)}")
        return int(bool(survivors))

    h = StageHarness()
    scenarios(h, args.cases, args.seed)
    summary = h.finish(show_coverage=args.coverage,
                       default_run=args.cases == 1.0 and args.seed == 94423)
    print(" ".join(f"{name}={h.counts[name]}" for name in FUNCS))
    print(f"OK: {sum(h.counts.values())} cases")
    return int(any(s["uncovered_count"] or s["stale_declared"] for s in summary.values()))


if __name__ == "__main__":
    sys.exit(main())
