"""Differential checks for the readable title/sequence-player ports in member 180."""
import argparse
import ctypes as C
import csv
import os
from pathlib import Path
import random
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402
import check_game  # noqa: E402
from game_call_inventory import ledger  # noqa: E402

U, I, P = C.c_uint32, C.c_int32, C.c_void_p
STACK_LO, STACK_HI = 0x801FF000, 0x80200000
OVERLAY_BASE, WORK = 0x80136000, 0x801C0000
MUTATION_SELECTOR, MUTATION_MARKER = 0x801FEF00, WORK + 0x5FC
HOOK = C.CFUNCTYPE(I, P, P, C.c_char_p, U, U, C.POINTER(U))

# name: address, byte size, C symbol, return kind, argument kinds, C hook argument
# Return/argument kinds use s for signed 32-bit and u for unsigned 32-bit.
FUNCS = {
    "SeqPlayer_StreamRingInit": (0x80136000, 0x74, "ae_SeqPlayer_StreamRingInit", "v", "uu", True),
    "SeqPlayer_Run": (0x80136074, 0x3E0, "ae_SeqPlayer_Run", "s", "u", True),
    "SeqPlayer_DisplayStateInit": (0x80136604, 0x88, "ae_SeqPlayer_DisplayStateInit", "u", "", True),
    "SeqPlayer_Init": (0x8013668C, 0x150, "ae_SeqPlayer_Init", "s", "uu", True),
    "SeqPlayer_Stop": (0x801369E0, 0x1F4, "ae_SeqPlayer_Stop", "v", "u", True),
    "SeqPlayer_FadeIn": (0x80136EA8, 0x148, "ae_SeqPlayer_FadeIn", "v", "", True),
    "SeqPlayer_SetTransitionSweep": (0x80139FA0, 0x18, "ae_SeqPlayer_SetTransitionSweep", "u", "uu", False),
    "SeqPlayer_AdvanceTransitionSweep": (0x80139FB8, 0x4A0, "ae_SeqPlayer_AdvanceTransitionSweep", "v", "uu", True),
    "SeqPlayer_CommandWriteObjectFlags": (0x80139EC4, 0x48, "ae_SeqPlayer_CommandWriteObjectFlags", "v", "uu", False),
    "SeqPlayer_ParseObjects": (0x8013990C, 0x4A0, "ae_SeqPlayer_ParseObjects", "v", "u", True),
    "Mode_Title": (0x80137AC0, 0x258, "ae_Mode_Title", "s", "", True),
    "SeqPlayer_InitHook": (0x80137D20, 0x54, "ae_SeqPlayer_InitHook", "v", "", True),
    "SeqPlayer_Hook2": (0x801398B0, 0x20, "ae_SeqPlayer_Hook2", "v", "uu", False),
    "SeqPlayer_GetObject": (0x801398D0, 0x20, "ae_SeqPlayer_GetObject", "u", "u", False),
    "SeqPlayer_FreeObjects": (0x80139DAC, 0x38, "ae_SeqPlayer_FreeObjects", "v", "", False),
    "SeqPlayer_GetObjectFlag": (0x80139DE4, 0x1C, "ae_SeqPlayer_GetObjectFlag", "u", "u", False),
    "SeqPlayer_ClearObjectFlags": (0x80139E00, 0x44, "ae_SeqPlayer_ClearObjectFlags", "v", "", False),
    "SeqPlayer_MeshInit": (0x80139E44, 0x44, "ae_SeqPlayer_MeshInit", "v", "uu", True),
    "SeqPlayer_ModelInit": (0x80139E88, 0x3C, "ae_SeqPlayer_ModelInit", "v", "uu", True),
    "SeqPlayer_ClearObjectFlagsWrap": (0x80139F0C, 0x2C, "ae_SeqPlayer_ClearObjectFlagsWrap", "v", "u", True),
    "SeqPlayer_SumRecordSizes": (0x80139F38, 0x34, "ae_SeqPlayer_SumRecordSizes", "u", "us", False),
    "SeqPlayer_FindRecordByTime": (0x80139F6C, 0x30, "ae_SeqPlayer_FindRecordByTime", "u", "uu", False),
    "SeqPlayer_FindRangeIndex": (0x8013AC98, 0x6C, "ae_SeqPlayer_FindRangeIndex", "s", "u", False),
    "SeqPlayer_ApplyFlagOps": (0x8013B8D0, 0xD4, "ae_SeqPlayer_ApplyFlagOps", "v", "usu", False),
}

API = {
    0x80011104: ("LoadListStart", 2), 0x800116C0: ("LoadListStep", 0),
    0x800125B8: ("CdMgrUpdate", 0), 0x8002E520: ("VSync", 1),
    0x8002E7C0: ("VSyncCallback", 1),
    0x800315D0: ("ReadGeomScreen", 0),
    0x80025C60: ("StSetRing", 2), 0x80025C90: ("StSetStream", 5),
    0x80025EB0: ("StFree", 1), 0x80025F60: ("StRingStatus", 2),
    0x80015C34: ("MeshInit", 2),
    0x80028634: ("SetDispMask", 1),
    0x8005DF48: ("BeginFrame", 0),
    0x80028EFC: ("PutDispEnv", 1), 0x80028D30: ("PutDrawEnv", 1),
    0x80028CC0: ("DrawOTag", 1), 0x8009B9C8: ("ScreenBandDraw", 3),
    0x801367DC: ("SeqPlayer_CopyScreenToBack", 0),
    0x80026A50: ("CdSync", 0), 0x80025E24: ("CdPosToInt", 1),
    0x80025D20: ("CdIntToPos", 2),
    0x80136454: ("SeqPlayer_CdReadSeek", 2),
    0x801364B0: ("SeqPlayer_CdPlayXaRange", 4),
    0x80086248: ("SoundStopAndRestoreVolume", 1),
    0x80136E68: ("SeqPlayer_CdSetFilter", 1),
    0x8002408C: ("CdReady", 3), 0x80136FF0: ("SeqPlayer_Present", 1),
    0x8002FAF0: ("SetGeomScreen", 1), 0x8013AA70: ("SeqPlayer_LoadSavedImage", 0),
    0x8013BA80: ("SeqPlayer_SaveRestoreVram", 1),
    0x800286CC: ("DrawSync", 1), 0x800A3750: ("srand", 1),
    0x800242FC: ("XaPlay", 3),
    0x800A3760: ("memcpy", 3),
    0x80136884: ("SeqPlayer_LoadFiles", 1),
    0x80136BD4: ("SeqPlayer_SetupDisplay", 0),
    0x80096360: ("ProgressSetByte", 2), 0x80096540: ("ProgressBitsSet", 1),
    0x80136E00: ("SeqPlayer_StGetNextWait", 2),
    0x80069BEC: ("RunEffectScriptPort", 0),
    0x8006A9F8: ("RunEffectsPort", 0),
    0x8001325C: ("RunPadUpdatePort", 1),
}

COMMAND_HOOKS = {
    0x80137DB4: "SeqPlayer_CommandDrawPoseMesh",
    0x80137F64: "SeqPlayer_CommandDrawPoseModel",
    0x8013800C: "SeqPlayer_CommandSetProjection",
    0x8013818C: "SeqPlayer_CommandDrawPrimitive",
    0x80138254: "SeqPlayer_CommandDrawPoseModelAlt",
    0x801382FC: "SeqPlayer_CommandSetBackColor",
    0x80138354: "SeqPlayer_DrawSprite",
    0x80138404: "SeqPlayer_CommandDrawObject",
    0x801385CC: "SeqPlayer_CommandDrawPoseModel2",
    0x80138670: "SeqPlayer_CommandSetObjectState",
    0x80138730: "SeqPlayer_CommandDrawText",
    0x8013B390: "SeqPlayer_CommandDrawAnimatedModel",
    0x801388F0: "SeqPlayer_CommandDrawObject2",
    0x80138A6C: "SeqPlayer_CommandDrawParticle",
    0x80138AC8: "SeqPlayer_CommandSetObjectMode",
    0x80138B40: "SeqPlayer_CommandSetObjectTransform",
    0x80138CE4: "SeqPlayer_CommandDrawTrail",
    0x80138D60: "SeqPlayer_CommandDrawPoseModel3",
    0x80138E1C: "SeqPlayer_CommandDrawPoseModel4",
    0x80138ED8: "SeqPlayer_CommandDrawPoseModel5",
    0x80138F90: "SeqPlayer_CommandDrawPoseMesh2",
    0x80139124: "SeqPlayer_CommandSetColor",
    0x801391C8: "SeqPlayer_CommandBuildModelPackets",
    0x8013AAF0: "SeqPlayer_CommandDrawMessage",
    0x8013AF50: "SeqPlayer_CommandDrawTextPage",
}

EMPTY_COMMANDS = {
    0x80137DAC, 0x80138344, 0x8013834C, 0x801383FC,
    0x80138728, 0x801388D8, 0x801388E0, 0x801388E8, 0x80138CDC,
    0x80136604,
}


class NativeHooks(C.Structure):
    _fields_ = [("user", P), ("native_call", HOOK), ("renderer", HOOK),
                ("ram_stack_pointer", U)]


class MipsMem:
    def __init__(self, cpu): self.cpu = cpu
    def rd8(self, a): return self.cpu.read8(a)
    def rd16(self, a): return self.cpu.read16(a)
    def rd32(self, a): return self.cpu.read32(a)
    def wr8(self, a, v): self.cpu.write8(a, v)
    def wr16(self, a, v): self.cpu.write16(a, v)
    def wr32(self, a, v): self.cpu.write32(a, v)


class CPort:
    def __init__(self, dll):
        self.dll = C.CDLL(str(dll))
        d = self.dll
        d.ae_ram_create.restype = P
        d.ae_ptr.argtypes = [P, U, U]; d.ae_ptr.restype = P
        for name in ("ae_rd8", "ae_rd16", "ae_rd32"):
            fn = getattr(d, name); fn.argtypes = [P, U]; fn.restype = U
        for name in ("ae_wr8", "ae_wr16", "ae_wr32"):
            getattr(d, name).argtypes = [P, U, U]
        d.ae_ram_clear_error.argtypes = [P]
        d.ae_ram_failed.argtypes = [P]; d.ae_ram_failed.restype = I
        d.ae_ram_error.argtypes = [P]; d.ae_ram_error.restype = C.c_char_p
        self.ram = d.ae_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(d.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(d.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.funcs = {}
        for name, (_address, _size, symbol, ret, kinds, with_hooks) in FUNCS.items():
            fn = getattr(d, symbol)
            fn.argtypes = [P] + ([C.POINTER(NativeHooks)] if with_hooks else []) + [I if kind == "s" else U for kind in kinds]
            fn.restype = {"v": None, "s": I, "u": U}[ret]
            self.funcs[name] = fn
        d.ae_HeapInit.argtypes = [P, P]
        d.ae_HeapAlloc.argtypes = [P, P, U]; d.ae_HeapAlloc.restype = U
        d.ae_ram_destroy.argtypes = [P]
        self.callbacks = []
        self.native = HOOK()
        self.hooks = NativeHooks(None, self.native, self.native, 0)
        d.ovl180_RunEffectScript.argtypes = [P, C.POINTER(NativeHooks)]
        d.ovl180_RunEffects.argtypes = [P, C.POINTER(NativeHooks)]
        d.ovl180_RunPadUpdate.argtypes = [P, C.POINTER(NativeHooks), U]
        self.ports = {"RunEffectScriptPort": d.ovl180_RunEffectScript,
                      "RunEffectsPort": d.ovl180_RunEffects,
                      "RunPadUpdatePort": d.ovl180_RunPadUpdate}

    def sync(self, snapshot):
        C.memmove(self.main, snapshot[0], 0x200000)
        C.memmove(self.scratch, snapshot[1], 0x400)
        self.dll.ae_ram_clear_error(self.ram)


class CallModel:
    def __init__(self, profile=None):
        self.profile = profile or {}
        self.events = []
        self.mem = None
        self.load_starts = 0
        self.load_steps = 0
        self.cd_ready_polls = 0
        self.stream_wait_calls = 0
        self.vsync_calls = 0

    def call(self, name, args):
        args = tuple(int(value) & 0xFFFFFFFF for value in args)
        self.events.append((name, args))
        if name == "LoadListStart":
            self.load_starts += 1
            self.load_steps = 0
        if name == "LoadListStep":
            self.load_steps += 1
            return 1 if self.load_steps >= self.profile.get("load_done_after", 2) else 0
        if name == "CdReady":
            self.cd_ready_polls += 1
            return int(self.cd_ready_polls % 2 == 0)
        if name == "VSync":
            self.vsync_calls += 1
            values = self.profile.get("vsync_values", ())
            return int(values[min(self.vsync_calls - 1, len(values) - 1)]) if values else 0
        if name == "SeqPlayer_StGetNextWait":
            self.stream_wait_calls += 1
            self.run_stack_pointer = (args[0] - 16) & 0xFFFFFFFF
            record = self.profile.get("stream_record", WORK + 0x1A00)
            end = self.profile.get("stream_end", record + 4)
            self.mem.wr32(args[0], record)
            self.mem.wr32(args[1], end)
            results = self.profile.get("stream_wait_results", (0, 1))
            return int(results[min(self.stream_wait_calls - 1, len(results) - 1)])
        if name == "StRingStatus":
            self.mem.wr16(args[0], self.profile.get("ring_status_a", 0))
            self.mem.wr16(args[1], self.profile.get("ring_status_b", 0))
            return 0
        if name == "PadGetState": return self.profile.get("pad_state", 6)
        if name == "PadInfoMode": return self.profile.get("pad_mode", 7)
        if name == "PadDemoPlaybackStep": return self.profile.get("pad_demo_step", 0)
        if name == "StickToButtons": return self.profile.get("stick_buttons", 0)
        if name == "MeshInit":
            destination, mesh = args
            self.mem.wr32(destination, mesh)
            self.mem.wr32(destination + 4, self.mem.rd32(mesh + 0x20))
            self.mem.wr32(destination + 8, self.mem.rd32(mesh + 0x24))
            return 0
        if name == "memcpy":
            destination, source, byte_count = args
            for offset in range(byte_count):
                self.mem.wr8(destination + offset, self.mem.rd8(source + offset))
            return destination
        if name == "XaPlay": return 1
        if name == "CdPosToInt": return self.profile.get("cd_position", 0)
        return int(self.profile.get("returns", {}).get(name, 0))


class Harness:
    def __init__(self, dll):
        reg.declare("stack_window", "801FF000..80200000")
        self.area_files = dump_areas.area_members()
        self.overlay = (ROOT / "extract/members/180.bin").read_bytes()
        self.coverage = set()
        self.cpus = {}
        for area in (1, 2, 3):
            member = self.area_files[area]
            cpu = mr.MipsRam.from_extract(ROOT / f"extract/members/{member:03X}.bin")
            cpu.load_area(self.overlay, OVERLAY_BASE)
            cpu.coverage = self.coverage
            self.cpus[area] = cpu
        self.base = {area: cpu.snapshot() for area, cpu in self.cpus.items()}
        self.c = CPort(dll)
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0
        self.active_c_model = None
        self.cmem = None
        self.active_mips_cpu = None
        self._bind_native()

    def _bind_native(self):
        c = self.c
        self.cmem = CMem(c.dll, c.ram)
        def native(_user, _ram, raw_name, _address, argc, words):
            name = raw_name.decode("ascii")
            return self.active_c_model.call(name, [words[i] for i in range(argc)])
        c.native = HOOK(native)
        c.callbacks.append(c.native)
        c.hooks.native_call = c.native
        c.hooks.renderer = c.native

    def setup(self, cpu, name, scenario):
        mem = MipsMem(cpu)
        for address in range(STACK_LO, STACK_HI, 4): mem.wr32(address, 0)
        rng = random.Random(0x180000 + scenario * 37 + list(FUNCS).index(name))
        for offset in range(0, 0x600, 4): mem.wr32(WORK + offset, rng.getrandbits(32))
        base = WORK + 0x100
        objects = WORK + 0x800
        flags = WORK + 0xA00
        records = WORK + 0xC00
        operation = WORK + 0x1000

        if name in ("SeqPlayer_StreamRingInit", "SeqPlayer_InitHook", "SeqPlayer_ClearObjectFlagsWrap", "SeqPlayer_Stop", "SeqPlayer_ParseObjects"):
            cpu.call(0x8008C01C, [])
            self.c.dll.ae_HeapInit(self.c.ram, None)
        if name == "SeqPlayer_StreamRingInit":
            mem.wr32(0x8013C1A8, 0 if scenario & 1 else WORK + 0x1800)
        if name == "SeqPlayer_InitHook":
            mem.wr32(0x8013C56C, scenario % 5)
            mem.wr32(0x8013C570, (scenario + 1) % 5)
        if name in ("SeqPlayer_GetObject", "SeqPlayer_GetObjectFlag"):
            mem.wr32(0x8013C564, objects)
            mem.wr32(0x8013C574, flags)
            for index in range(20):
                mem.wr32(objects + index * 4, WORK + 0x1800 + index * 0x20)
                mem.wr8(flags + index, (index * 3 + scenario) & 0xFF)
        if name in ("SeqPlayer_ClearObjectFlags", "SeqPlayer_ClearObjectFlagsWrap"):
            mem.wr32(0x8013C568, scenario % 7)
            mem.wr32(0x8013C574, flags)
            for index in range(20): mem.wr8(flags + index, rng.randrange(256))
        if name == "SeqPlayer_MeshInit":
            mesh = WORK + 0x1400
            mem.wr32(objects, mesh)
            mem.wr32(objects + 4, WORK + 0x1500)
            mem.wr16(mesh + 0x0E, 0x2345 + scenario)
        if name == "SeqPlayer_ModelInit":
            model = WORK + 0x1400
            mem.wr32(model + 0x0C, WORK + 0x1600)
        if name in ("SeqPlayer_SumRecordSizes", "SeqPlayer_FindRecordByTime", "SeqPlayer_ApplyFlagOps"):
            cursor = records
            for index in range(10):
                record = cursor
                size = 6 if name == "SeqPlayer_ApplyFlagOps" else 4 + (index & 3)
                mem.wr8(record + 1, size)
                mem.wr8(record + 2, rng.randrange(256))
                mem.wr8(record + 3, rng.randrange(256))
                mem.wr16(record + 0x12, rng.randrange(0x10000))
                cursor += size * 4
            mem.wr8(operation, scenario & 0xFF)
            mem.wr8(operation + 1, (scenario >> 1) & 1)
            mem.wr8(operation + 2, scenario % 4)
            mem.wr8(operation + 3, scenario % 8)
        if name == "SeqPlayer_FindRangeIndex":
            table = WORK + 0x1400
            mem.wr32(0x8013C714, 4 + scenario % 3)
            mem.wr32(0x8013C758, table)
            for index in range(8):
                mem.wr16(table + index * 8, index * 10)
                mem.wr16(table + index * 8 + 2, index * 10 + 8)
        if name == "SeqPlayer_Mode_Title":
            pass
        if name in ("SeqPlayer_Run", "Mode_Title"):
            sequence_index = scenario % 9 if name == "Mode_Title" else scenario % 9
            mem.wr16(0x800F44AA, sequence_index)
            for index in range(9):
                path = 0x8013C090 + index * 0x1C
                mem.wr8(path, 0)
                mem.wr8(path + 1, 1)
                mem.wr32(path + 0x10, 0)
                mem.wr32(path + 0x14, 0)
            if scenario % 8 == 6:
                mem.wr32(0x8013C090 + sequence_index * 0x1C + 0x14, 1)
            mem.wr32(0x8013C56C, 2)
            mem.wr32(0x8013C570, 3)
            mem.wr32(0x80148000, 0)
            mem.wr32(0x8013C494, 0)
            stream_records = WORK + 0x1A00
            branch = scenario % 8
            command_id = {2: 11, 3: 12}.get(branch, 0)
            record_size = 3 if command_id == 11 else 2
            mem.wr16(stream_records, record_size)
            mem.wr8(stream_records + 2, command_id)
            mem.wr8(stream_records + 3, 0)
            if command_id == 11:
                mem.wr8(stream_records + 4, scenario + 1)
            sentinel = stream_records + record_size * 2
            mem.wr16(sentinel, 0)
            mem.wr16(sentinel + 2,
                     0xFFFF if branch == 1 else 0)
            for address in (0x8013C564, 0x8013C568, 0x8013C574,
                            0x8013C1A8):
                mem.wr32(address, 0)
            mem.wr8(0x8013C580, 1)
            mem.wr16(0x8013C582, 0)
            for port in (0x800E52D0, 0x800E52F2):
                mem.wr8(port, 0)
                mem.wr8(port + 1, 5)
                buttons = (0xF79F if scenario % 8 == 4 else
                           0xFFFE if scenario % 8 == 5 else 0xFFFF)
                mem.wr8(port + 2, buttons & 0xFF)
                mem.wr8(port + 3, buttons >> 8)
                for offset in range(4, 8): mem.wr8(port + offset, 0x80)
            mem.wr32(0x800E5490, 0)
            mem.wr16(0x800E5494, 0)
            for index in range(40):
                mem.wr16(0x800E53C4 + index * 2, 1)
            # These already verified per-frame systems receive an empty scene state.
            for base, count, stride, active in (
                (0x800B9900, 100, 0x34, 0),
                (0x800BB920, 12, 0x44, 0x3C),
                (0x800BAF00, 16, 0x54, 0x4A),
            ):
                for index in range(count): mem.wr8(base + index * stride + active, 0)
            for index in range(8): mem.wr16(0x800F4E90 + index * 8 + 6, 0)
            mem.wr16(0x800BAD78, 0)
            mem.wr16(0x800BAD68 + 8, 0)
            for index in range(32): mem.wr16(0x800BADA0 + index * 8 + 6, 0)
        if name == "SeqPlayer_Init":
            path = 0x8013C090
            mem.wr8(path, 3 + scenario)
            mem.wr8(path + 1, 1 + scenario % 4)
            mem.wr32(path + 0x10, (0, 1, 2, 3, 8, 9, 10, 11)[scenario % 8])
            mem.wr32(0x8013C56C, 2)
            mem.wr32(0x8013C570, 3)
            mem.wr32(0x80148000, 0)
        if name == "SeqPlayer_ParseObjects":
            data = WORK + 0x100
            composite = scenario in (3, 4, 5, 6, 7)
            source_kind = 3 if scenario in (2, 4, 7) else 2
            if scenario == 0:
                count = 2
            elif scenario == 1:
                count = 1
            elif not composite:
                count = 1
            elif scenario in (3, 4):
                count = 3
            elif scenario == 5:
                count = 11
            else:
                count = 19
            mem.wr32(data, count)
            composite_index = count - 1 if composite else -1
            source_index = {3: 1, 4: 1, 5: 9, 6: 17, 7: 17}.get(scenario, 0)
            for index in range(count):
                descriptor = data + 4 + index * 4
                if scenario == 0 and index == 0:
                    mem.wr32(descriptor, 1 << 8)
                    mem.wr8(descriptor + 3, 1)
                elif scenario == 1:
                    mem.wr32(descriptor, 5 << 8)
                    mem.wr8(descriptor + 3, 5)
                elif index == composite_index or (scenario == 0 and index == 1):
                    mem.wr32(descriptor, 0x00000400 | source_index)
                    mem.wr8(descriptor + 3, 4)
                else:
                    model_index = 0x301 if source_kind == 3 else 0x201
                    mem.wr32(descriptor, (source_kind << 8) | model_index)
                    mem.wr8(descriptor + 3, source_kind)
            mesh = data + 0x201 * 4
            model = data + 0x301 * 4
            records = WORK + 0x1600
            duplicate_data = WORK + 0x1700
            mem.wr16(mesh + 0x0E, 2)
            mem.wr16(mesh + 0x12, 5)
            mem.wr16(mesh + 0x16, 2)
            mem.wr32(mesh + 0x20, duplicate_data)
            mem.wr32(mesh + 0x24, records)
            mem.wr16(model + 0x02, 2)
            mem.wr16(model + 0x04, 0)
            mem.wr16(model + 0x06, 4 + scenario)
            mem.wr16(model + 0x08, 2 + scenario)
            mem.wr16(model + 0x0E, 2)
            mem.wr16(model + 0x12, 3)
            mem.wr16(model + 0x16, 1)
            model_primitives = model + 12 + (2 + scenario) * 4 + (4 + scenario) * 8
            for offset, size in ((0, 1), (4, 1), (8, 2), (16, 1), (20, 1)):
                mem.wr8(records + offset + 1, size)
                mem.wr8(model_primitives + offset + 1, size)
            for offset in range(0, 32, 4):
                mem.wr32(duplicate_data + offset, 0xA0000000 | offset | scenario)
        if name == "SeqPlayer_FadeIn":
            path = WORK + 0x100
            mem.wr32(0x8013C484, path)
            mem.wr32(path + 0x10, 8 if scenario & 1 else 0)
        if name == "SeqPlayer_SetTransitionSweep":
            mem.wr8(0x8013C580, 0xAA)
            mem.wr16(0x8013C582, 0x1234)
            mem.wr8(WORK + 0x104, (scenario * 31) & 0xFF)
        if name == "SeqPlayer_AdvanceTransitionSweep":
            mode_timer = ((1, 0), (1, 192), (255, 0), (2, 190),
                          (1, 0xFFFF), (64, 129), (0, 0xFFFF), (7, 180))[scenario % 8]
            mem.wr8(0x8013C580, mode_timer[0])
            mem.wr16(0x8013C582, mode_timer[1])
        if name == "SeqPlayer_CommandWriteObjectFlags":
            flag_table = WORK + 0x1500
            mem.wr32(0x8013C574, flag_table)
            count = scenario % 5
            mem.wr16(WORK + 0x100, 2 + count * 2)
            for index in range(count):
                target = -2 if scenario == 7 and index == 0 else (index * 3 + scenario) % 16
                mem.wr16(WORK + 0x104 + index * 4, target & 0xFFFF)
                mem.wr8(WORK + 0x106 + index * 4, (scenario * 17 + index * 9) & 0xFF)
                mem.wr8(WORK + 0x107 + index * 4, 0xA5)
        if name == "SeqPlayer_Stop":
            flags_ptr = self.alloc(cpu, flags, 16)
            object_ptr = self.alloc(cpu, objects, 24)
            ring_ptr = self.alloc(cpu, WORK + 0x1800, 32)
            mem.wr32(0x8013C568, 2)
            mem.wr32(0x8013C574, flags_ptr)
            mem.wr32(0x8013C564, object_ptr)
            mem.wr32(0x8013C1A8, ring_ptr)
            mem.wr32(0x8013C488, WORK + 0x1900)
            mem.wr32(0x8013C48C, WORK + 0x1A00)
            mem.wr32(0x8013C490, 17 + scenario)
            mem.wr32(0x8013C494, 200 + scenario)
            mem.wr16(0x800F44AA, 6 if scenario % 2 == 0 else 5)
        return mem

    def alloc(self, cpu, initialize_at, size):
        pointer = cpu.call(0x8008C09C, [size])[2]
        got = self.c.dll.ae_HeapAlloc(self.c.ram, None, size)
        if got != pointer:
            raise RuntimeError(f"heap fixture differs: C={got:08X} MIPS={pointer:08X}")
        if initialize_at:
            return pointer
        return pointer

    def c_args(self, name, args):
        fn = self.c.funcs[name]
        _a, _s, _sym, _ret, kinds, with_hooks = FUNCS[name]
        out = [self.c.ram]
        if with_hooks: out.append(C.byref(self.c.hooks))
        out.extend(I(value) if kind == "s" else U(value) for value, kind in zip(args, kinds))
        if name == "SeqPlayer_InitHook": out.extend((U(0x801FFF80), U(0x801FFF84)))
        return fn(*out)

    def mips_args(self, name, scenario):
        if name == "SeqPlayer_StreamRingInit": return [0, 0 if scenario % 4 == 0 else scenario + 1]
        if name == "SeqPlayer_Init": return [0x8013C480, 0]
        if name == "SeqPlayer_Stop": return [scenario % 2]
        if name == "SeqPlayer_FadeIn": return []
        if name in ("SeqPlayer_SetTransitionSweep", "SeqPlayer_AdvanceTransitionSweep",
                    "SeqPlayer_CommandWriteObjectFlags"):
            return [WORK + 0x100, WORK + 0x104]
        if name == "SeqPlayer_Run": return [scenario % 9]
        if name == "SeqPlayer_DisplayStateInit": return []
        if name == "SeqPlayer_ParseObjects": return [WORK + 0x100]
        if name == "Mode_Title": return []
        if name == "SeqPlayer_InitHook": return []
        if name == "SeqPlayer_Hook2": return [WORK + 0x300, WORK + 0x304]
        if name == "SeqPlayer_GetObject": return [(-2 if scenario == 0 else scenario)]
        if name == "SeqPlayer_FreeObjects": return []
        if name == "SeqPlayer_GetObjectFlag": return [scenario % 7]
        if name == "SeqPlayer_ClearObjectFlags": return []
        if name == "SeqPlayer_MeshInit": return [WORK + 0x800, WORK + 0x1400]
        if name == "SeqPlayer_ModelInit": return [WORK + 0x800, WORK + 0x1400]
        if name == "SeqPlayer_ClearObjectFlagsWrap": return [scenario + 1]
        if name == "SeqPlayer_SumRecordSizes": return [WORK + 0xC00, (0 if scenario == 0 else scenario % 8)]
        if name == "SeqPlayer_FindRecordByTime": return [WORK + 0xC00, scenario * 5]
        if name == "SeqPlayer_FindRangeIndex": return [scenario * 10 + (scenario % 3)]
        if name == "SeqPlayer_ApplyFlagOps": return [WORK + 0xC00, scenario % 7, WORK + 0x1000]
        raise KeyError(name)

    def hooks(self, model):
        hooks = {}
        mapped = dict(API)
        mapped.update({address: (name, 2) for address, name in COMMAND_HOOKS.items()})
        for address, (name, arity) in mapped.items():
            def invoke(cpu, regs, label=name, n=arity, m=model):
                if label in self.c.ports:
                    snapshot = cpu.snapshot()
                    self.c.sync(snapshot)
                    self.active_c_model = m
                    m.mem = self.cmem
                    self.c.hooks.ram_stack_pointer = (regs[4] - 16) & 0xFFFFFFFF
                    if label == "RunPadUpdatePort":
                        self.c.ports[label](self.c.ram, C.byref(self.c.hooks), regs[4])
                    else:
                        self.c.ports[label](self.c.ram, C.byref(self.c.hooks))
                    cpu.ram[:] = bytes(self.c.main)
                    cpu.scratch[:] = bytes(self.c.scratch)
                    m.mem = MipsMem(cpu)
                    return 0
                if label == "SeqPlayer_Nop": return 0
                args = list(regs[4:8])[:min(n, 4)]
                args.extend(cpu.read32(regs[29] + 16 + 4 * i) for i in range(n - 4))
                return m.call(label, args)
            hooks[address] = invoke
        return hooks

    def patches(self, model):
        def draw_sweep(cpu, regs):
            args = [cpu.read8(0x8013C580), cpu.read16(0x8013C582), 0x800EC350]
            model.call("SeqPlayer_DrawTransitionSweep", args)
            return 0x8013A428
        return {0x8013A014: draw_sweep}

    def run(self, name, scenario=0, selector=0):
        area = (1, 2, 3)[scenario % 3]
        cpu = self.cpus[area]
        cpu.restore(self.base[area])
        self.c.sync(cpu.snapshot())
        mem = self.setup(cpu, name, scenario)
        snapshot = cpu.snapshot()
        self.c.sync(snapshot)
        self.counts[name] += 1

        cpu.write32(MUTATION_SELECTOR, selector)
        cpu.write32(MUTATION_MARKER, 0)
        before = cpu.snapshot()
        profile = {}
        if name in ("SeqPlayer_Run", "Mode_Title"):
            branch = scenario % 8
            wait_profiles = {
                0: (1,),
                1: (0, 1),
                2: (0, 0, 1),
                3: (0, 0, 0, 1),
                4: (0, 0, 1),
                5: (0, 0, 0, 1),
                6: (0, 0, 1),
                7: (0, 0, 0, 0, 1),
            }
            profile["stream_wait_results"] = wait_profiles[branch]
            if branch == 5:
                profile["ring_status_b"] = 4
                profile["vsync_values"] = (0, 0, 3, 3, 3, 3)
            if branch == 7:
                profile["ring_status_b"] = 3
                profile["vsync_values"] = (0, 0, 3, 3, 3, 3, 3)
        m_model = CallModel(profile)
        m_model.mem = MipsMem(cpu)
        c_model = CallModel(profile)
        c_model.mem = self.cmem
        self.active_c_model = c_model
        address = FUNCS[name][0]
        args = self.mips_args(name, scenario)
        try:
            expected = cpu.call(address, args, hooks=self.hooks(m_model),
                                patches=self.patches(m_model))
        except Exception as exc:
            self.fail(name, scenario, f"MIPS error {exc}")
            return False
        self.c.sync(before)
        self.last_before = before
        self.active_c_model = c_model
        self.c.hooks.ram_stack_pointer = getattr(m_model, "run_stack_pointer", 0)
        cargs = self.mips_args(name, scenario)
        actual = self.c_args(name, cargs)
        ret = FUNCS[name][3]
        if ret != "v" and (int(actual) & 0xFFFFFFFF) != expected[2]:
            self.fail(name, scenario, f"return C={int(actual)&0xffffffff:08X} MIPS={expected[2]:08X}")
            return False
        limit = STACK_LO - 0x80000000
        got, want = bytes(self.c.main[:limit]), bytes(cpu.ram[:limit])
        if got != want:
            off = next(i for i, (a, b) in enumerate(zip(got, want)) if a != b)
            self.fail(name, scenario, f"RAM at {0x80000000+off:08X}: C={got[off:off+8].hex()} MIPS={want[off:off+8].hex()}")
            return False
        if bytes(self.c.scratch) != bytes(cpu.scratch):
            self.fail(name, scenario, "scratchpad differs")
            return False
        if self.c.dll.ae_ram_failed(self.c.ram):
            self.fail(name, scenario, self.c.dll.ae_ram_error(self.c.ram).decode("utf-8", "replace"))
            return False
        if c_model.events != m_model.events:
            self.last_c_events, self.last_m_events = c_model.events, m_model.events
            self.fail(name, scenario, f"hooks differ: C={c_model.events!r}, MIPS={m_model.events!r}")
            return False
        return True

    def fail(self, name, scenario, message):
        self.failures += 1
        print(f"FAIL {name}[{scenario}]: {message}", flush=True)
        if os.environ.get("OVL180_DEBUG"):
            cpu = self.cpus[(1, 2, 3)[scenario % 3]]
            print(f"  C error={self.c.dll.ae_ram_error(self.c.ram).decode('utf-8', 'replace') if self.c.dll.ae_ram_failed(self.c.ram) else None}", flush=True)
            print(f"  C544/C548 C={self.cmem.rd32(0x8013C544):08X}/{self.cmem.rd32(0x8013C548):08X} MIPS={cpu.read32(0x8013C544):08X}/{cpu.read32(0x8013C548):08X}", flush=True)
            for address in (0x800A6C00, 0x800A6C10, 0x800BF690, 0x800BF694, 0x800BF698,
                            0x8013C564, 0x8013C568, 0x8013C574, 0x8013C1A8):
                before = self.last_before[0][address - 0x80000000:address - 0x80000000 + 4]
                print(f"  {address:08X}: before={before.hex()} C={self.cmem.rd32(address):08X} MIPS={cpu.read32(address):08X}", flush=True)
            print(f"  C event tail={getattr(self, 'last_c_events', [])[-12:]!r} MIPS={getattr(self, 'last_m_events', [])[-12:]!r}", flush=True)

    def all(self, cases=8, names=None):
        self.coverage.clear()
        for cpu in self.cpus.values(): cpu.coverage = self.coverage
        for name in (list(names) if names else FUNCS):
            n = max(cases, 10 if name in ("SeqPlayer_Run", "Mode_Title") else cases)
            for scenario in range(n): self.run(name, scenario)
            print(f"{name}: {self.counts[name]} cases", flush=True)

    def finish(self, default_run=False, show_coverage=False, names=None):
        selected = list(names) if names else list(FUNCS)
        rows = [(name, FUNCS[name][0], FUNCS[name][1], True) for name in selected]
        summary = reg.coverage_summary(rows, self.coverage)
        if show_coverage: reg.print_coverage(summary)
        gaps = {name: row for name, row in summary.items() if row["uncovered_count"] or row["stale_declared"]}
        for name, row in gaps.items(): print(f"COVERAGE GAP {name}: {row['uncovered']}", flush=True)
        if default_run:
            reg.write_results(self.counts, summary, not self.failures and not gaps,
                              extra={"overlay": "180.bin", "areas": [1, 2, 3]},
                              function_names=selected)
        return not self.failures and not gaps


class CMem:
    def __init__(self, dll, ram): self.dll, self.ram = dll, ram
    def rd8(self, address): return self.dll.ae_rd8(self.ram, address)
    def rd16(self, address): return self.dll.ae_rd16(self.ram, address)
    def rd32(self, address): return self.dll.ae_rd32(self.ram, address)
    def wr8(self, address, value): self.dll.ae_wr8(self.ram, address, value)
    def wr16(self, address, value): self.dll.ae_wr16(self.ram, address, value)
    def wr32(self, address, value): self.dll.ae_wr32(self.ram, address, value)


def build(source=None, tag="ovl180"):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{time.time_ns()}.dll"
    exports = dll.with_suffix(".def")
    modules = {row["source"] for row in ledger()}
    modules.update(check_game.SOURCES)
    modules.discard("src/game/ae_game.c")
    modules.discard("src/game/ae_game_stage.c")
    modules.discard("src/game/ae_game_hooks.c")
    modules.discard("src/game/ae_calls.c")
    modules.discard("src/game/ae_calls_special.c")
    modules.discard("src/game/ae_calls_legacy.c")
    modules.discard("src/game/ae_game_body.c")
    modules.discard("tools/mips/game_shim.c")
    modules.discard("tools/mips/game_integration_shim.c")
    modules.update({"src/core/ae_ram.c", "src/core/ae_sdk_matrix.c", "src/world/ae_areaset.c",
                    "src/render/ae_tex_copy.c", "src/modes/ae_ovl180.c"})
    export_names = ["ae_ram_create", "ae_ram_destroy", "ae_ram_clear_error", "ae_ram_failed",
                    "ae_ram_error", "ae_ptr", "ae_rd8", "ae_rd16", "ae_rd32", "ae_wr8", "ae_wr16", "ae_wr32",
                    "ae_HeapInit", "ae_HeapAlloc"]
    export_names += [row[2] for row in FUNCS.values()]
    export_names += ["ovl180_RunEffectScript", "ovl180_RunEffects",
                     "ovl180_RunPadUpdate"]
    exports.write_text("EXPORTS\n" + "\n".join(export_names) + "\n", encoding="ascii")
    gcc = shutil.which("gcc")
    if not gcc: raise SystemExit("gcc not found")
    inputs = [str(source) if source and path == "src/modes/ae_ovl180.c"
              else str(ROOT / path)
              for path in sorted(modules) if (ROOT / path).is_file()]
    subprocess.run([gcc, "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror", "-ffunction-sections",
                    "-fdata-sections", "-fvisibility=hidden", "-Wl,--gc-sections", "-Wl,--exclude-all-symbols",
                    "-shared", "-static-libgcc", "-Isrc/core", "-Isrc/world", "-Isrc/render", "-Isrc/modes",
                    "-Isrc/effects", *inputs, str(exports), "-o", str(dll), "-lm"], cwd=ROOT, check=True)
    return dll


def instrument(source):
    for index, (_name, (_addr, _size, symbol, _ret, _args, _hooks)) in enumerate(FUNCS.items(), 1):
        pattern = re.compile(r"(\b(?:void|int32_t|uint32_t)\s+" + re.escape(symbol) + r"\s*\([^)]*\)\s*\{)", re.S)
        lines = "\n".join(
            f"    if (ae_rd32(r, 0x{MUTATION_SELECTOR:08X}u) == {index*2-v}u) "
            f"ae_wr32(r, 0x{MUTATION_MARKER:08X}u, 0x18000000u | {index*2-v}u);"
            for v in (1, 2))
        source, count = pattern.subn(r"\1\n" + lines, source, count=1)
        if count != 1: raise RuntimeError(f"cannot instrument {symbol}")
    return source


def mutations():
    source = ROOT / "src/modes/ae_ovl180.c"
    mutant = ROOT / "extract/mips/ae_ovl180_mutant.c"
    mutant.write_text(instrument(source.read_text(encoding="utf-8")), encoding="utf-8")
    caught = 0
    try:
        harness = Harness(build(mutant, "ovl180_mutant"))
        for index, name in enumerate(FUNCS, 1):
            for variant in (1, 2):
                before = harness.failures
                harness.run(name, variant - 1, selector=index * 2 - variant)
                if harness.failures > before: caught += 1
                else: print(f"MUTATION SURVIVED {name}:{variant}", flush=True)
        total = len(FUNCS) * 2
        print(f"mutations caught: {caught}/{total}", flush=True)
        return caught == total
    finally:
        mutant.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=int, default=8)
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--function", action="append", choices=FUNCS)
    args = parser.parse_args()
    if args.mutate: return 0 if mutations() else 1
    harness = Harness(build())
    harness.all(args.cases, args.function)
    return 0 if harness.finish(default_run=args.cases == 8 and not args.function,
                               show_coverage=args.coverage, names=args.function) else 1


if __name__ == "__main__":
    raise SystemExit(main())
