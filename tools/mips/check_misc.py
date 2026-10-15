"""Differential validation for the miscellaneous stage-system ports.

Run with `.venv/Scripts/python.exe tools/mips/check_misc.py [--coverage] [--mutate]`.
The MIPS side executes the SCUS_944.23 instructions from a real EXE/area image;
the C side runs the RAM-image ports from src/world/ae_misc.c.
"""
import argparse
import ctypes as C
import math
import os
import random
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/mips"), str(ROOT / "tools")]
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

OUT = ROOT / "extract/mips"
STACK_LO = 0x1FF000
STACK_HI = 0x200000
DEFAULT_SEED = 94423
M32 = 0xFFFFFFFF
P, U, I = C.c_void_p, C.c_uint32, C.c_int32
HOOK_FN = C.CFUNCTYPE(I, P, P, U, U, U, U)
CLIMB_FN = C.CFUNCTYPE(I, P, P, U, U, U, U, U, U)
SOUND_IDLE = C.CFUNCTYPE(I, P, P)
SOUND_WAIT = C.CFUNCTYPE(None, P, P, U)
SOUND_ADSR = C.CFUNCTYPE(None, P, P, U, C.c_uint16, C.c_uint16)
SOUND_OFF = C.CFUNCTYPE(None, P, P, U, C.c_uint16)
SOUND_PRINT = C.CFUNCTYPE(None, P, P, U, U, I, I, I)
SOUND_VOLUME = C.CFUNCTYPE(None, P, P, U, U, C.c_uint16)


class SoundHw(C.Structure):
    _fields_ = [("user", P), ("wait_idle", SOUND_IDLE), ("wait_loop", SOUND_WAIT),
                ("set_voice_adsr", SOUND_ADSR), ("voice_key_off", SOUND_OFF),
                ("printf_error", SOUND_PRINT), ("set_voice_volume", SOUND_VOLUME)]

# name -> (address, byte size, result type, argument kinds, has hooks)
FUNCS = {
    "MatrixRotYMul": (0x80014750, 128, "v", "su", True),
    "MeshCalcEnvUVs": (0x80016D60, 264, "v", "u", True),
    "ItemsDraw": (0x8008435C, 572, "v", "", True),
    "ItemDrawModelEnvMap": (0x800846D8, 236, "v", "su", True),
    "SoundEmitterUpdate": (0x8008686C, 1080, "s", "", True),
    "CameraNudge": (0x80047690, 144, "v", "sss", False),
    "PadRumbleReset": (0x80014208, 44, "v", "s", False),
    "AreaOverlayUpdate": (0x8005ACE8, 160, "s", "", True),
    "StageTimerAdd": (0x8005DD78, 40, "v", "s", False),
    "PlayerHeal": (0x8005E794, 88, "v", "s", True),
    "PlayerEyeBlinkUpdate": (0x80067AF0, 280, "v", "", True),
    "IndicatorHintFree": (0x800715B4, 72, "v", "", True),
    "IndicatorHintReady": (0x800715FC, 64, "s", "", True),
    "IndicatorsUpdateDraw": (0x800719F8, 804, "v", "", True),
    "PickupCollect": (0x80078A18, 644, "v", "s", True),
    "CollisionRayTest": (0x80078C9C, 1580, "s", "uuu", True),
    "PickupsUpdate": (0x80083CB8, 1700, "v", "", True),
    "PlayerClearDAT44A2": (0x8007BC9C, 24, "v", "", False),
    "PickupSetPosition": (0x800836E0, 92, "v", "u", False),
    "CollectedItemsAdd": (0x80084964, 160, "v", "u", False),
    "BgmApplyPlayerEffect": (0x80087654, 420, "s", "", False),
    "SoundEmitterAttenuation": (0x80087EE0, 396, "s", "uu", False),
    "ColliderScriptRun": (0x8008B7F8, 388, "s", "us", True),
    "ColliderScriptsUpdate": (0x80088AB8, 704, "s", "", True),
    "GetZoneMask": (0x8004626C, 208, "s", "ssss", False),
    "CameraRcCarActive": (0x8004A978, 172, "s", "", False),
    "StageExitPending": (0x8004D104, 16, "s", "", False),
    "BossUpdate": (0x8005AC28, 192, "s", "u", True),
    "VramScrollUpdate": (0x800691F8, 768, "v", "", True),
    "PlayerIsSubmerged": (0x8006B9F8, 52, "s", "", False),
    "Indicator_Register": (0x800716EC, 200, "s", "sus", True),
    "MenuItemSetRange": (0x8007D290, 12, "v", "uuu", False),
    "MenuItemSetStep": (0x8007D2A4, 8, "v", "uu", False),
    "HudFindGadgetIconPos": (0x8008197C, 108, "v", "", False),
    "ColorFadeReset": (0x8008C390, 48, "v", "", False),
    "ProgressFlagGet": (0x8009632C, 52, "s", "u", False),
    "AreaFeature30Update": (0x8009B7FC, 460, "v", "", True),
    "ColliderScriptFollowPath": (0x800892C4, 812, "s", "us", True),
    "ColliderScriptPlayerMove": (0x8008A4B0, 1132, "s", "us", True),
    "ColliderScriptCarry": (0x8008A91C, 328, "s", "us", True),
    "ColliderScriptCommand": (0x8008B980, 1076, "s", "uuss", True),
    "ColliderResetToStart": (0x8008BDB4, 152, "v", "u", False),
    "MessageBoxSetText": (0x8008DF20, 28, "v", "u", False),
    "MessageBoxBusy": (0x8008DF3C, 16, "s", "", False),
    "HudPanelB_Start": (0x80095600, 144, "s", "ss", False),
    "HudCounterC_Start": (0x800958C0, 100, "s", "s", False),
    "HudPopupStartPickup": (0x80098398, 80, "v", "", False),
    "HudPopupUpdate": (0x800983E8, 752, "v", "", True),
    "ItemsUpdateDraw": (0x80084598, 72, "v", "", True),
    "SpecialActorPlayerNear": (0x8009B6C8, 204, "v", "", False),
    "AreaFeature30GetState": (0x8009B7A0, 16, "s", "", False),
    "SeqFadeTrackProgram": (0x800A25CC, 116, "s", "ssss", False),
    "SeqGetChannelProgram": (0x800A30D8, 128, "s", "sss", False),
    "SfxVoiceSetPanVol": (0x800A3570, 140, "v", "sss", True),
}

HOOK_ADDRESSES = {
    "MulMatrix": 0x80031470,
    "SetRotMatrix": 0x8002FB00,
    "SetTransMatrix": 0x8002FB30,
    "RotTransPers": 0x80031330,
    "ModelDraw": 0x800845E0,
    "ModelDrawWithOt": 0x80017D94,
    "ModelTransformRigid": 0x800161C0,
    "ModelTransformSkinned": 0x80016A60,
    "ModelDrawEnvMap": 0x8001CE54,
    "SpawnParticle": 0x800698B8,
    "TrailSpawn": 0x8006ED78,
    "GetGroundHeight": 0x80043400,
    "DrawShadow": 0x8001F604,
    "PlaySfx": 0x800877F8,
    "CopyPlayerEyeTexture": 0x80067C08,
    "HudShowHealth": 0x8005E42C,
    "FUN_8013f464": 0x8013F464,
    "FUN_8013fa2c": 0x8013FA2C,
    "FUN_8013fd74": 0x8013FD74,
    "FUN_80140a60": 0x80140A60,
    "FUN_80140d60": 0x80140D60,
    "IsTimeAttack": 0x80097678,
    "SoundLoopStop": 0x8008806C,
    "SpuWaitIdle": 0x800A1398,
    "ColliderScriptUpdate": 0x800B3000,
    "Printf": 0x800A3720,
    "PointInBoxXZ": 0x80073B64,
    "ColliderSlopeContactDir": 0x8004F884,
    "DirIndexFromFlags": 0x8007774C,
    "PlayerSetClimbTarget": 0x80066E28,
    "PlayerSetRope": 0x80066E8C,
    "ScreenBandDraw": 0x8009B9C8,
    "SetDrawMove": 0x8002B380,
    "exit": 0x800A3700,
    "FUN_801402d8": 0x801402D8,
    "Area53_BossUpdate": 0x8013D57C,
    "FUN_80143854": 0x80143854,
    "Area46_BossUpdate": 0x8013F1AC,
    "Area57_BossUpdate": 0x8013E4B4,
    "FUN_80137248": 0x80137248,
    "FUN_8013fc18": 0x8013FC18,
    "FUN_8013fe4c": 0x8013FE4C,
    "FUN_80088d78": 0x80088D78,
    "FUN_80088f2c": 0x80088F2C,
    "FUN_800895f0": 0x800895F0,
    "FUN_80089b10": 0x80089B10,
    "FUN_80089d64": 0x80089D64,
    "FUN_80089d6c": 0x80089D6C,
    "FUN_8008aa64": 0x8008AA64,
    "GetSolidHeight": 0x8004D8C0,
    "GetColliderQuadrantMaskXZ": 0x800889B4,
    "CdIntToPos": 0x80025D20,
    "CdControl": 0x8002408C,
    "CdReadyCallback": 0x80024078,
    "VSync": 0x8002E520,
    "AnimFramePtr": 0x80015240,
}

HOOK_ARITY = {
    "MulMatrix": 2, "SetRotMatrix": 1, "SetTransMatrix": 1,
    "RotTransPers": 4, "ModelDraw": 2, "ModelDrawWithOt": 4,
    "ModelTransformRigid": 3, "ModelTransformSkinned": 1,
    "ModelDrawEnvMap": 3, "DrawShadow": 4,
    "SpawnParticle": 2,
    "RenderEnvUV": 4,
    "RenderIndicator": 4, "RenderHudPopup": 4,
    "TrailSpawn": 4, "GetGroundHeight": 3,
    "PlaySfx": 1,
    "CopyPlayerEyeTexture": 1, "HudShowHealth": 1,
    "FUN_8013f464": 0, "FUN_8013fa2c": 0, "FUN_8013fd74": 0,
    "FUN_80140a60": 0, "FUN_80140d60": 0,
    "IsTimeAttack": 0, "HeapFree": 1, "SoundLoopStop": 2,
    "SpuWaitIdle": 0,
    "ColliderScriptMotion": 4,
    "ColliderScriptUpdate": 2,
    "Printf": 2, "SoundLoopStart": 2,
    "PointInBoxXZ": 2,
    "ColliderSlopeContactDir": 4, "DirIndexFromFlags": 1,
    "PlayerSetClimbTarget": 6, "PlayerSetRope": 2,
    "ScreenBandDraw": 3, "SetDrawMove": 4,
    "exit": 1, "FUN_801402d8": 1, "Area53_BossUpdate": 1,
    "FUN_80143854": 1, "Area46_BossUpdate": 1, "Area57_BossUpdate": 1,
    "FUN_80137248": 1, "FUN_8013fc18": 2, "FUN_8013fe4c": 2,
    "FUN_80088d78": 2, "FUN_80088f2c": 2, "FUN_800895f0": 2,
    "FUN_80089b10": 2, "FUN_80089d64": 2, "FUN_80089d6c": 2,
    "FUN_8008aa64": 2,
    # GetSolidHeight's callable interface is (x, y, z); the EXE caller leaves
    # $a3 unspecified and the native adapter supplies zero in that dead slot.
    "GetSolidHeight": 3,
    "GetColliderQuadrantMaskXZ": 2, "CdIntToPos": 2, "CdControl": 3,
    "CdReadyCallback": 3, "VSync": 1, "AnimFramePtr": 2,
}


def s32(value):
    value &= M32
    return value - 0x100000000 if value & 0x80000000 else value


def s16(value):
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def parse_hook_fields():
    text = (ROOT / "src/world/ae_misc.h").read_text(encoding="utf-8")
    body = text.split("typedef struct AeMiscHooks", 1)[1].split("} AeMiscHooks;", 1)[0]
    return re.findall(r"\b(?:AeMiscCall|AeCollProbeClimbFn)\s+(\w+)\s*;", body)


class Hooks(C.Structure):
    _fields_ = [("user", P), ("stack_pointer", U)] + [(name, CLIMB_FN if name == "PlayerSetClimbTarget" else HOOK_FN) for name in parse_hook_fields()] + [
        ("sound_hw", P)]


def build(source=None, tag="misc", include_mutations=False):
    OUT.mkdir(parents=True, exist_ok=True)
    dll = OUT / f"{tag}_{os.getpid()}_{time.time_ns()}.dll"
    exports = dll.with_suffix(".def")
    names = ["ae_ram_create", "ae_ram_destroy", "ae_ram_clear_error", "ae_ram_failed",
             "ae_ram_error", "ae_ptr"]
    names += ["ae_" + name for name in FUNCS]
    if include_mutations:
        names += [f"ae_mut_{name}_{variant}" for name in FUNCS for variant in range(2)]
    exports.write_text("EXPORTS\n" + "\n".join(names) + "\n", encoding="ascii")
    # Link the verified module closure: CollisionUpdate now calls body-movement
    # ports directly. Mutants combine misc and misc_batch in one translation unit.
    import csv
    import check_game as cg
    sources = {ROOT / rel for rel in cg.SOURCES}
    with (ROOT / 'tools/verified.csv').open(newline='') as stream:
        sources.update(ROOT / row['source'] for row in csv.DictReader(stream) if row['status']=='diffed')
    if source is not None:
        sources.difference_update({ROOT/'src/world/ae_misc.c',ROOT/'src/world/ae_misc_batch.c'})
        sources.add(Path(source))
    sources=sorted(sources)
    compiler = shutil.which("gcc") or r"C:\minGW\mingw64\bin\gcc.EXE"
    subprocess.run([compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                    "-Wl,--exclude-all-symbols", "-shared", "-static-libgcc",
                    *map(str, sources), str(exports), "-o", str(dll), "-lm"], check=True)
    return dll


class Model:
    def __init__(self, side, case_name, results=None):
        self.side = side
        self.case_name = case_name
        self.results = results or {}
        self.events = []
        self.call_counts = {}

    def call(self, name, args):
        arity = HOOK_ARITY.get(name, 4)
        args = tuple(value & M32 for value in args[:arity])
        self.events.append((name, args))
        call_index = self.call_counts.get(name, 0)
        self.call_counts[name] = call_index + 1
        result = self.results.get(name, 0)
        if isinstance(result, (list, tuple)):
            return s32(result[min(call_index, len(result) - 1)])
        return s32(result)


class CSide:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        lib = self.lib
        lib.ae_ram_create.restype = P
        lib.ae_ram_destroy.argtypes = [P]
        lib.ae_ptr.argtypes = [P, U, U]
        lib.ae_ptr.restype = P
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_failed.restype = C.c_int
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        lib.ae_ram_clear_error.argtypes = [P]
        self.ram = lib.ae_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.model = None
        self.keep = []
        self.hooks = Hooks()
        self.hooks.user = P(1)
        self.hooks.stack_pointer = mr.DEFAULT_SP
        self.sound_hw = SoundHw()
        self.sound_hw.user = P(1)
        self.sound_hw_keep = [
            SOUND_IDLE(lambda _u, _r: self.model.call("SpuWaitIdle", ())),
            SOUND_WAIT(lambda _u, _r, _units: None),
            SOUND_ADSR(lambda _u, _r, _channel, _a1, _a2: None),
            SOUND_OFF(lambda _u, _r, _half, _mask: None),
            SOUND_PRINT(lambda _u, _r, _fmt, _argc, _a, _b, _c: None),
            SOUND_VOLUME(lambda _u, _r, _channel, _side, _volume: None),
        ]
        for (field, _field_type), callback in zip(SoundHw._fields_[1:], self.sound_hw_keep):
            setattr(self.sound_hw, field, callback)
        self.hooks.sound_hw = C.cast(C.pointer(self.sound_hw), P)
        self.hooks.user = id(self)
        self.user_mismatches=[]
        def call_with_user(user,name,args):
            if user != self.hooks.user: self.user_mismatches.append(name)
            return self.model.call(name,args)
        for field in parse_hook_fields():
            cb = HOOK_FN(lambda _u, _r, a0, a1, a2, a3, name=field:
                         call_with_user(_u,name, (a0, a1, a2, a3)))
            if field == "PlayerSetClimbTarget":
                cb = CLIMB_FN(lambda _u, _r, a0, a1, a2, a3, a4, a5: call_with_user(_u,"PlayerSetClimbTarget", (a0, a1, a2, a3, a4, a5)))
            self.keep.append(cb)
            setattr(self.hooks, field, cb)
        for name, (_address, _size, result_kind, arg_kinds, takes_hooks) in FUNCS.items():
            self.signature("ae_" + name, result_kind, arg_kinds, takes_hooks)

    def signature(self, symbol, result_kind, arg_kinds, takes_hooks):
        fn = getattr(self.lib, symbol)
        fn.argtypes = [P] + ([C.POINTER(Hooks)] if takes_hooks else []) + [I if k == "s" else U for k in arg_kinds]
        fn.restype = None if result_kind == "v" else I

    def sync(self, cpu):
        self.user_mismatches=[]
        C.memmove(self.main, bytes(cpu.ram), len(cpu.ram))
        C.memmove(self.scratch, bytes(cpu.scratch), len(cpu.scratch))
        self.lib.ae_ram_clear_error(self.ram)

    def invoke(self, name, args, model, symbol=None):
        self.model = model
        _a, _n, _ret, kinds, takes_hooks = FUNCS[name]
        converted = [s32(arg) if kind == "s" else arg & M32 for arg, kind in zip(args, kinds)]
        fn = getattr(self.lib, symbol or ("ae_" + name))
        if takes_hooks:
            return fn(self.ram, C.byref(self.hooks), *converted)
        return fn(self.ram, *converted)


class Harness:
    def __init__(self, dll):
        reg.declare("stack_window", "801FF000..80200000")
        reg.declare("unreachable", "SoundEmitterAttenuation 80088014 80088024..8008802C")
        self.c = CSide(dll)
        self.coverage = set()
        self.cases_run = {name: 0 for name in FUNCS}
        self.steps = 0
        self.area_members = dump_areas.area_members()
        self.cpus = {}
        gte_oracle = None
        overlay_for_area = {0x44: 0x19E, 0x46: 0x1A0, 0x53: 0x1A5,
                            0x56: 0x1A6, 0x57: 0x1A8}
        for area in (0x01, 0x07, 0x16, 0x44, 0x46, 0x53, 0x56, 0x57):
            member = self.area_members.get(area)
            if member is None:
                continue
            cpu = mr.MipsRam.from_extract(ROOT / f"extract/members/{member:03X}.bin")
            overlay = overlay_for_area.get(area)
            if overlay is not None:
                overlay_path = ROOT / f"extract/members/{overlay:03X}.bin"
                cpu.load_area(overlay_path.read_bytes(), 0x8013D000)
            cpu.coverage = self.coverage
            if gte_oracle is None:
                cpu.attach_gte_oracle()
                gte_oracle = (cpu.gte_state, cpu.gte_backend)
            else:
                cpu.gte_state, cpu.gte_backend = gte_oracle
            self.cpus[area] = cpu
        if not self.cpus:
            raise SystemExit("no real area members found under extract/members")

    def run(self, name, args=(), setup=None, area=0x01, results=None, symbol=None):
        cpu = self.cpus[area]
        if not hasattr(cpu, "_misc_base"):
            cpu._misc_base = cpu.snapshot()
        cpu.restore(cpu._misc_base)
        cpu.gte_reset()
        if setup:
            setup(cpu)
        self.c.sync(cpu)
        mips_model = Model(cpu, name, results)
        c_model = Model(self.c, name, results)
        hooks = {address: (lambda m, regs, field=field:
                           mips_model.call(field, regs[4:8] + ([m.read32(regs[29]+16), m.read32(regs[29]+20)] if field == "PlayerSetClimbTarget" else [])))
                 for field, address in HOOK_ADDRESSES.items()}
        if name in ("ColliderScriptPlayerMove", "ColliderScriptsUpdate"):
            hooks.pop(0x80043400, None)
        cpu.hook_log.clear()
        start_steps = cpu.steps
        address, _size, result_kind, _kinds, _takes_hooks = FUNCS[name]
        patches = {}
        if name == "IndicatorsUpdateDraw":
            def render_indicator_patch(machine, regs):
                kind = machine.reads16(regs[17])
                model = 0x800BBC54 if kind < 2 else 0x800BBCC4
                mode = 64 if kind == 1 else 0
                regs[4:8] = [regs[17], model, mode, kind & M32]
                mips_model.call("RenderIndicator", regs[4:8])
                return 0x80071C54
            patches[0x80071B6C] = render_indicator_patch
        elif name == "HudPopupUpdate":
            def render_popup_patch(machine, regs):
                regs[4:8] = [0x800E07C0, 0x800F4350, machine.read32(0x800EC350), regs[16]]
                mips_model.call("RenderHudPopup", regs[4:8])
                return 0x800986C4
            patches[0x80098478] = render_popup_patch
        registers = cpu.call(address, args, hooks=hooks, patches=patches)
        self.steps += cpu.steps - start_steps
        render_uv_expected = []
        if name == "MeshCalcEnvUVs":
            instance = args[0]
            mesh = cpu.read32(instance)
            vertex_count = cpu.read16(mesh + 6)
            vertex_data = mesh + (cpu.read16(mesh) << 2)
            uv_records = 0x800EA060
            uv_results = []
            for index in range(vertex_count):
                source = vertex_data + index * 8
                destination = uv_records + index * 8
                uv_results.append(cpu.read16(destination + 6))
                render_uv_expected.append((source, destination, instance, index))
            c_model.results["RenderEnvUV"] = uv_results
        elif name == "ItemDrawModelEnvMap":
            model_index, instance = args
            slot = 0x800F4C30 + (model_index & M32) * 0x10
            mesh = cpu.read32(slot)
            vertex_count = cpu.read16(mesh + 6)
            vertex_data = mesh + (cpu.read16(mesh) << 2)
            uv_results = []
            for index in range(vertex_count):
                source = vertex_data + index * 8
                destination = 0x800EA060 + index * 8
                uv_results.append(cpu.read16(destination + 6))
                render_uv_expected.append((source, destination, slot, index))
            c_model.results["RenderEnvUV"] = uv_results
        result = self.c.invoke(name, args, c_model, symbol)
        if self.c.user_mismatches: raise AssertionError(f'{name}: callback user context lost: {self.c.user_mismatches}')
        if results and "_climb_expected" in results:
            expected_climb = ("PlayerSetClimbTarget", results["_climb_expected"])
            if expected_climb not in mips_model.events:
                raise AssertionError(f"climb scenario did not reach its original call: {mips_model.events}")
        self.cases_run[name] += 1
        if result_kind != "v" and (int(result) & M32) != registers[2]:
            raise AssertionError(f"{name} case {self.cases_run[name]} {args}: return C={int(result) & M32:08X} MIPS={registers[2]:08X}")
        expected = bytes(cpu.ram[:STACK_LO])
        actual = C.string_at(self.c.main, STACK_LO)
        if expected != actual:
            offset = next(i for i, (left, right) in enumerate(zip(expected, actual)) if left != right)
            raise AssertionError(f"{name} case {self.cases_run[name]} {args}: RAM differs at {0x80000000 + offset:08X}: "
                                 f"C={actual[offset]:02X}, MIPS={expected[offset]:02X}; "
                                 f"actor={cpu.read32(0x800B4DC4):08X}; "
                                 f"hooks MIPS={mips_model.events!r}, C={c_model.events!r}")
        if bytes(cpu.scratch) != bytes(self.c.scratch):
            diff = next(i for i, (left, right) in enumerate(zip(cpu.scratch, self.c.scratch))
                        if left != right)
            raise AssertionError(f"{name} case {self.cases_run[name]} {args}: scratchpad differs at "
                                 f"{0x1F800000 + diff:08X}: C={self.c.scratch[diff]:02X}, "
                                 f"MIPS={cpu.scratch[diff]:02X}")
        render_uv_actual = [event[1] for event in c_model.events if event[0] == "RenderEnvUV"]
        if render_uv_actual != render_uv_expected:
            raise AssertionError(f"{name}{args}: renderer UV requests differ: "
                                 f"expected={render_uv_expected!r}; C={render_uv_actual!r}")
        mips_events = [event for event in mips_model.events if event[0] != "RenderEnvUV"]
        c_events = [event for event in c_model.events if event[0] != "RenderEnvUV"]
        if mips_events != c_events:
            raise AssertionError(f"{name} case {self.cases_run[name]} {args}: hook calls differ: "
                                 f"MIPS={mips_model.events!r}; C={c_model.events!r}")
        if self.c.lib.ae_ram_failed(self.c.ram):
            raise AssertionError(self.c.lib.ae_ram_error(self.c.ram).decode(errors="replace"))

    def finish(self, coverage=False, default_run=False):
        items = [(name, row[0], row[1], True) for name, row in FUNCS.items()]
        summary = reg.coverage_summary(items, self.coverage)
        if coverage:
            reg.print_coverage(summary)
        gaps = {name: row for name, row in summary.items()
                if row["uncovered_count"] or row["stale_declared"]}
        if default_run:
            reg.write_results(self.cases_run, summary, not gaps, function_names=list(FUNCS))
        if gaps:
            for name, row in gaps.items():
                print(f"COVERAGE GAP {name}: {row['uncovered']}; stale={row['stale_declared']}")
            raise SystemExit("FAIL: undeclared miscellaneous instruction coverage gaps")
        return summary


def set16(cpu, address, value):
    cpu.write16(address, value)


def set32(cpu, address, value):
    cpu.write32(address, value)


def scenarios(h, random_rounds=1, seed=DEFAULT_SEED):
    state, player = 0x800F4350, 0x800EC1E0
    cases = []
    add = lambda name, args=(), setup=None, area=0x01, results=None: cases.append((name, args, setup, area, results))

    for vertices in ((), ((32767, 32767, 32767),),
                     ((-32768, 32767, -32768), (32767, -32768, 32767),
                      (12000, -24000, 30000), (-12000, 24000, -30000)),
                     ((-32768, -32768, -32768), (-32768, -30000, -20000),
                      (-32768, -32768, -30000))):
        def mesh_setup(m, vertices=vertices):
            instance, mesh = 0x800F2000, 0x800F3000
            m.write32(instance, mesh)
            m.write16(mesh, 4)
            m.write16(mesh + 6, len(vertices))
            m.write_bytes(mesh + 16, bytes([0xA5] * (8 * max(1, len(vertices)))))
            for index, (x, y, z) in enumerate(vertices):
                m.write32(mesh + 16 + index * 8,
                          (x & 0xFFFF) | ((y & 0xFFFF) << 16))
                m.write32(mesh + 20 + index * 8, z & 0xFFFF)
            def pack(first, second):
                return (first & 0xFFFF) | ((second & 0xFFFF) << 16)
            state = m.gte_state
            for base in (0, 8, 16):
                state[32 + base + 0] = pack(0x1000, 0)
                state[32 + base + 1] = pack(0, 0)
                state[32 + base + 2] = pack(0x1000, 0)
                state[32 + base + 3] = pack(0, 0)
                state[32 + base + 4] = 0x1000
            state[6] = 0x00FFFFFF
            for offset in range(0, 8 * max(1, len(vertices))):
                m.write8(0x800EA060 + offset, 0xCC)
        add("MeshCalcEnvUVs", (0x800F2000,), mesh_setup)

    def env_map_setup(header_flag=0, vertices=()):
        def setup(m):
            slot = 0x800F4C30 + 16 * 0x10
            item, mesh = 0x800F1000, 0x800F3000
            set32(m, slot, mesh)
            m.write16(mesh, 4)
            m.write16(mesh + 6, len(vertices))
            m.write16(mesh + 0x0A, header_flag)
            for index, (x, y, z) in enumerate(vertices):
                m.write32(mesh + 16 + index * 8,
                          (x & 0xFFFF) | ((y & 0xFFFF) << 16))
                m.write32(mesh + 20 + index * 8, z & 0xFFFF)
            set16(m, item + 4, -120)
            set16(m, item + 6, 220)
            set16(m, item + 8, 340)
            m.write8(item + 0x0B, 3)
            def pack(first, second):
                return (first & 0xFFFF) | ((second & 0xFFFF) << 16)
            for base in (0, 8, 16):
                m.gte_state[32 + base + 0] = pack(0x1000, 0)
                m.gte_state[32 + base + 1] = pack(0, 0)
                m.gte_state[32 + base + 2] = pack(0x1000, 0)
                m.gte_state[32 + base + 3] = pack(0, 0)
                m.gte_state[32 + base + 4] = 0x1000
            m.gte_state[6] = 0x00FFFFFF
            m.write_bytes(0x800EA060, bytes([0xCC] * 64))
        return setup
    for header_flag, vertices in (
            (0, ((32767, 32767, 32767),)),
            (1, ((-32768, -32768, -32768), (-32768, -30000, -20000),
                 (-32768, -32768, -30000)))):
        add("ItemDrawModelEnvMap", (16, 0x800F1000),
            env_map_setup(header_flag, vertices), area=0x16)

    def item_draw_setup(active=(), draw_kinds=(), result=1, screen_x=0,
                        screen_y=0, effect_count=0, ot_limit=2048):
        def setup(m):
            for index in range(32):
                item = 0x800BEFA0 + index * 0x18
                m.write8(item + 0x0A, 0)
                m.write8(item + 0x17, 0)
            for index, kind in enumerate(active):
                item = 0x800BEFA0 + index * 0x18
                m.write8(item + 0x0A, kind)
                m.write8(item + 0x17, draw_kinds[index] if index < len(draw_kinds) else 0)
            set32(m, 0x800EBFC8, ot_limit)
            m.write8(0x800B28B2, effect_count)
            set32(m, 0x800EC1E0 + 0x24, 123)
            set32(m, 0x800EC1E0 + 0x28, 456)
            set32(m, 0x800EC1E0 + 0x2C, 789)
            set32(m, mr.DEFAULT_SP - 40, (screen_x & 0xFFFF) | ((screen_y & 0xFFFF) << 16))
            # The pickup effect's environment-mapped model has no vertices in
            # these cases; this keeps the renderer boundary exercised without
            # hiding MeshCalcEnvUVs iteration coverage.
            mesh = 0x800F3800
            m.write16(mesh, 4)
            m.write16(mesh + 6, 0)
            m.write16(mesh + 0x0A, 0)
            for model_index in (13, 15, 16):
                set32(m, 0x800F4C30 + model_index * 0x10, mesh)
        return setup
    add("ItemsDraw", (), item_draw_setup())
    add("ItemsDraw", (), item_draw_setup((1, 2, 3, 4, 5, 6)),
        results={"RotTransPers": [1] * 6}, area=0x07)
    add("ItemsDraw", (), item_draw_setup((7,)), results={"RotTransPers": 1})
    add("ItemsDraw", (), item_draw_setup((1,), (2,)), results={"RotTransPers": 1})
    add("ItemsDraw", (), item_draw_setup((1,), (1,)), results={"RotTransPers": 1})
    add("ItemsDraw", (), item_draw_setup((1,), ot_limit=100),
        results={"RotTransPers": 1}, area=0x44)
    add("ItemsDraw", (), item_draw_setup((1,), result=1, screen_x=417),
        results={"RotTransPers": 1}, area=0x16)
    add("ItemsDraw", (), item_draw_setup((1,), result=1, screen_y=273),
        results={"RotTransPers": 1}, area=0x44)
    add("ItemsDraw", (), item_draw_setup((1,), effect_count=1),
        results={"RotTransPers": 1}, area=0x07)
    add("ItemsDraw", (), item_draw_setup((1,), effect_count=8),
        results={"RotTransPers": 1}, area=0x16)

    for angle in (0, 0x400, -0x1234):
        add("MatrixRotYMul", (angle, 0x800F1200), lambda m: None)
    for fixed, flags in ((0, 0), (1, 0), (0, 1)):
        def camera(m, fixed=fixed, flags=flags):
            set32(m, 0x800E68D8, 0x800F1000)
            m.write8(0x800E6908, fixed)
            set32(m, state + 0x100, flags)
            for offset, value in ((0x0A, 100), (0x0C, -20), (0x0E, 300),
                                  (0x12, 50), (0x14, 0), (0x16, 400)):
                set16(m, 0x800F1000 + offset, value)
        add("CameraNudge", (7, -9, 13), camera)
    for area_id in (0x44, 0x46, 0x53, 0x56, 0x57):
        def overlay_setup(m, area_id=area_id):
            m.write8(0x800AF840, 0)
            set16(m, 0x800F4476, area_id)
        add("AreaOverlayUpdate", (), overlay_setup, area=area_id)
    def overlay_disabled(m):
        m.write8(0x800AF840, 1)
        set16(m, 0x800F4476, 0x44)
    add("AreaOverlayUpdate", (), overlay_disabled, area=0x44)
    def overlay_missing(m):
        m.write8(0x800AF840, 0)
        set16(m, 0x800F4476, 0x01)
    add("AreaOverlayUpdate", (), overlay_missing, area=0x01)
    for amount, initial in ((1, 2), (5, 5), (-2, 1)):
        add("PlayerHeal", (amount,), lambda m, initial=initial:
            set16(m, player + 0xE8, initial))
    for mode, health, feature, frame, actor_mode in ((0, 4, 0, 114, 0), (0, 0, 0, 15, 0),
                                                     (10, 4, 0, 4, 7), (1, 4, 1, 3, 0)):
        def eye(m, mode=mode, health=health, feature=feature, frame=frame, actor_mode=actor_mode):
            set16(m, player + 0xE8, health)
            set16(m, state + 0x150, mode)
            m.write8(0x800F5AA0, feature)
            set32(m, 0x800B4D70, 0x800F1200)
            set16(m, 0x800F1286, actor_mode)
            set32(m, 0x800EC350, frame)
        add("PlayerEyeBlinkUpdate", (), eye)
    for frame in (0, 1, 116, 117, 118, 119, 120, 234, 238):
        def blink_frame(m, frame=frame):
            set16(m, player + 0xE8, 4)
            set16(m, state + 0x150, 0)
            m.write8(0x800F5AA0, 0)
            set32(m, 0x800B4D70, 0x800F1200)
            set16(m, 0x800F1286, 0)
            set32(m, 0x800EC350, frame)
        add("PlayerEyeBlinkUpdate", (), blink_frame,
            area=(0x07 if frame in (116, 118) else 0x16 if frame == 238 else 0x01))
    add("IndicatorHintFree", (), lambda m: set32(m, 0x800BBD94, 0))
    for hint, busy in ((0, 1), (2, 0), (2, 1)):
        def ready(m, hint=hint, busy=busy):
            m.write8(0x800BBD99, hint)
            m.write8(0x800F5A00, busy)
        add("IndicatorHintReady", (), ready)
    def indicator_setup(entries=(), hint_state=0, hint_busy=0,
                        flags=0, message=0):
        def setup(m):
            set32(m, 0x800BBC50, len(entries))
            m.write_bytes(0x800BBD34, bytes(4 * 0x18))
            set16(m, 0x800BBCB0, 0x1234)
            set16(m, 0x800BBD20, 0x5678)
            for index, (kind, state_value, frame, timer, position) in enumerate(entries):
                slot = 0x800BBD34 + index * 0x18
                set16(m, slot, kind)
                set16(m, slot + 4, position[0])
                set16(m, slot + 6, position[1])
                set16(m, slot + 8, position[2])
                set16(m, slot + 10, timer)
                set16(m, slot + 20, frame)
                set16(m, slot + 22, state_value)
            set32(m, 0x800BBD98, flags)
            set32(m, 0x800BBD94, message)
            m.write8(0x800BBD99, hint_state)
            m.write8(0x800F5A00, hint_busy)
            set32(m, 0x800F4DD0 + 0x20, 0)
        return setup
    neutral_position = (120, 240, -360)
    add("IndicatorsUpdateDraw", (), indicator_setup())
    for entry, area in (
            ((0, 0, 28, 0, neutral_position), 0x01),
            ((0, 0, 29, 0, neutral_position), 0x07),
            ((1, 1, 88, 0, neutral_position), 0x16),
            ((0, 2, 29, 0, neutral_position), 0x01),
            ((3, 0, 9, 0, neutral_position), 0x07),
            ((2, 0, 57, 1, neutral_position), 0x44),
            ((2, 1, 87, 0, neutral_position), 0x46),
            ((2, 2, 20, -1, neutral_position), 0x53),
            ((1, 0, 9, 0, neutral_position), 0x56),
            ((-1, 0, 9, 0, neutral_position), 0x01)):
        add("IndicatorsUpdateDraw", (), indicator_setup((entry,)), area=area)
    add("IndicatorsUpdateDraw", (), indicator_setup(
        ((0, 0, 2, 0, neutral_position), (1, 1, 87, 0, neutral_position),
         (2, 0, 56, 1, neutral_position))))
    add("IndicatorsUpdateDraw", (), indicator_setup(
        ((0, 0, 2, 0, neutral_position),), hint_state=2,
        hint_busy=0, message=0x800F2000))
    add("IndicatorsUpdateDraw", (), indicator_setup(
        ((0, 0, 2, 0, neutral_position),), hint_state=2,
        hint_busy=1, message=0x800F2000))
    add("IndicatorsUpdateDraw", (), indicator_setup(
        ((0, 0, 2, 0, neutral_position),), hint_state=1,
        flags=0x00010100, message=0x800F2000))
    add("StageTimerAdd", (-12,), lambda m: set16(m, 0x800B01C2, 65000))
    add("PlayerClearDAT44A2", (), lambda m: set16(m, state + 0x152, 0x7FFF))
    for msg in (0x800F2000, 0x80154000):
        add("MessageBoxSetText", (msg,))
    for busy in (0, 1):
        add("MessageBoxBusy", (), lambda m, busy=busy: m.write8(0x800F5A00, busy))
    for mode, argument in ((0, 0), (1, 1), (-1, 4)):
        add("HudPanelB_Start", (mode, argument))
    for argument, counter in ((1, 0), (0, 10), (0, 16), (0, 90)):
        add("HudCounterC_Start", (argument,), lambda m, counter=counter:
            set16(m, 0x800F4DB0, counter))
    add("HudPopupStartPickup")
    for mode, counter, timer, effect, time_attack in (
            (0, 0, 0, 0, 0), (1, 3, 1, 1, 0), (1, 1, 1, 1, 0),
            (2, 7, 4, 0, 1),
            (3, 254, 0, 1, 0), (4, 2, 1, 0, 1), (5, 1, 1, 1, 0)):
        def popup(m, mode=mode, counter=counter, timer=timer, effect=effect):
            m.write8(0x800E07C0, mode)
            m.write8(0x800E07C1, counter)
            set16(m, 0x800E07C2, timer)
            set16(m, 0x800E07C4, -42)
            set16(m, 0x800E07C6, 77)
            m.write8(0x800B28B2, effect)
            set32(m, 0x800F4350 + 0x04, 100)
            set32(m, 0x800F4350 + 0x0C, 200)
        add("HudPopupUpdate", (), popup, results={"IsTimeAttack": time_attack})
    for value in (0, 1, 3):
        add("AreaFeature30GetState", (), lambda m, value=value: m.write8(0x800F5AA0, value))
    for active, program, volume in ((0, 42, 100), (1, 42, 100)):
        def seqfade(m, active=active, program=program, volume=volume):
            track = 0x800F5AC0 + 3 * 0x120
            m.write8(track + 0x24, active)
            set32(m, track + 0x10, 0x800F8000)
            set16(m, track + 0x0C, volume)
            set16(m, 0x800F8000 + 0x12, program)
        add("SeqFadeTrackProgram", (3, program, volume, 15), seqfade)
    for force, channel, program in ((0, 0, 42), (0, 15, 0xFF), (1, 3, 12)):
        def seqget(m, force=force, channel=channel, program=program):
            track = 0x800F5AC0 + 3 * 0x120
            m.write8(track + 0x24, 1)
            set32(m, track + 0x10, 0x800F8000)
            m.write8(0x800F8000 + channel * 0x10 + 0x12, program)
        add("SeqGetChannelProgram", (3, channel, force), seqget)
    for wait in (0, 1):
        voice_setup = lambda m: None
        add("SfxVoiceSetPanVol", (2, 300, -500), voice_setup,
            results={"SpuWaitIdle": wait})
    for near, timer, y_delta, player_mode in (
            (True, 2, 0, 0x40), (False, 2, 0, 0x40),
            (True, 3, 0, 0x40), (True, 0, 0, 0x40),
            (True, 0, 512, 0x40), (True, 0, 0, 0x10)):
        def actor_near(m, near=near, timer=timer,
                       y_delta=y_delta, player_mode=player_mode):
            actor = 0x800F2000
            set32(m, 0x800B4DC4, actor)
            actor_z = 150 if near and timer == 0 and y_delta == 0 and player_mode == 0x40 else 200
            for off, val in ((0x24, 100), (0x28, 400 + y_delta), (0x2C, actor_z),
                             (0x8F, timer)):
                m.write8(actor + off, val) if off == 0x8F else set32(m, actor + off, val)
            set32(m, player + 0x24, 100 if near else 400)
            set32(m, player + 0x28, 0)
            set32(m, player + 0x2C, 200)
            set32(m, player + 0x70, player_mode)
        add("SpecialActorPlayerNear", (), actor_near)
    def collected_setup(count=0, entries=()):
        def setup(m):
            set16(m, 0x800F44BA, count)
            for index in range(128):
                set16(m, 0x800DFB70 + index * 2, 0x00FF)
            for index, value in enumerate(entries):
                set16(m, 0x800DFB70 + index * 2, value)
        return setup
    for count, entries in ((0, ()), (1, (0x42,)), (127, ())):
        add("CollectedItemsAdd", (0x42,), collected_setup(count, entries))
    def collected_no_terminator(m):
        set16(m, 0x800F44BA, 0)
        for index in range(128):
            set16(m, 0x800DFB70 + index * 2, 1)
    add("CollectedItemsAdd", (0x42,), collected_no_terminator)
    add("PickupSetPosition", (0x800F2000,), lambda m: (
        set32(m, 0x800F2000, 0x800F3000), set16(m, 0x800F2004, -20),
        set16(m, 0x800F2006, 300), set16(m, 0x800F2008, -800), m.write8(0x800F200A, 17)))
    def collision_ray_setup(grid_type=None, grid_flags=0, grid_contact=35,
                           collider_type=None, collider_flags=0,
                           collider_contact=44, wall_probe=1,
                           ground_solid=-1, ground_result=5,
                           collider_copy=0x1F8000B8,
                           collider_base=0x800F5000,
                           loop_existing=None):
        def setup(m):
            probes, grid_shape = 0x1F800000, 0x800F3000
            colliders, collider_flags_table = collider_base, 0x1F800140
            params, output = 0x800F6000, 0x800F8000
            m.write_bytes(probes, bytes(8 * 0x10))
            m.write_bytes(colliders, bytes(0x38 * 2))
            m.write_bytes(collider_flags_table, bytes(256))
            m.write_bytes(0x800BEEA0, bytes(16 * 12))
            for slot_index in range(16):
                set16(m, 0x800BEEA0 + slot_index * 12, -1)
            set32(m, 0x800B20B0, probes)
            set32(m, 0x800B20B4, 0x1F800080)
            set32(m, 0x800B20B8, 0x1F80008C)
            set32(m, 0x800B20BC, collider_copy)
            set32(m, 0x800B20C0, collider_flags_table)
            set32(m, 0x800F455C, colliders)
            set16(m, 0x800F45D4, 0 if collider_type is None else 1)
            set16(m, 0x800F45E6, wall_probe)
            set32(m, params + 0x00, 100)
            # Exercise the real HalfPipeRangeTest overlap gate now that the
            # caller no longer forces its old mocked return value of 1.
            set32(m, params + 0x04, 280)
            set32(m, params + 0x08, 200)
            set32(m, params + 0x0C, 5)
            set32(m, params + 0x10, 50)
            set16(m, output, 0x1234)
            set16(m, 0x1F8003F0, ground_solid)
            set16(m, 0x1F8003FA, ground_result)
            if grid_type is not None:
                record = probes + 7 * 0x10
                for offset, value in ((0x00, 100), (0x02, 200), (0x04, 200),
                                      (0x06, 20), (0x08, 100), (0x0A, 20)):
                    set16(m, record + offset, value)
                set32(m, record + 0x0C, grid_shape)
                set16(m, grid_shape + 0x0C, grid_flags)
                set16(m, grid_shape + 0x0E, grid_contact)
                set16(m, grid_shape + 0x12, grid_type)
            if collider_type is not None:
                record = colliders
                for offset, value in ((0x00, 100), (0x02, 200), (0x04, 200),
                                      (0x06, 20), (0x08, 100), (0x0A, 20),
                                      (0x10, collider_type), (0x14, collider_flags),
                                      (0x16, collider_contact)):
                    address = record + offset
                    if address & 1:
                        m.write_bytes(address, (value & 0xFFFF).to_bytes(2, "little"))
                    else:
                        set16(m, address, value)
            if loop_existing is not None:
                loop_id, loop_key = loop_existing
                set16(m, 0x800BEEA0, loop_id)
                set32(m, 0x800BEEA4, loop_key)
                set16(m, 0x800BEEA8, 1)
        return setup
    add("CollisionRayTest", (0, 0x800F6000, 0x800F8000),
        collision_ray_setup(), results={"GetGroundHeight": 0})
    add("CollisionRayTest", (0, 0x800F6000, 0x800F8000),
        collision_ray_setup(), results={"GetGroundHeight": 200})
    add("CollisionRayTest", (0, 0x800F6000, 0x800F8000),
        collision_ray_setup(grid_type=0),
        results={"PointInBoxXZ": 0})
    add("CollisionRayTest", (0, 0x800F6000, 0x800F8000),
        collision_ray_setup(collider_type=0),
        results={"PointInBoxXZ": 0})
    add("CollisionRayTest", (2 << 16, 0x800F6000, 0x800F8000),
        collision_ray_setup(grid_type=0),
        results={"PointInBoxXZ": 0})
    add("CollisionRayTest", (2 << 16, 0x800F6000, 0x800F8000),
        collision_ray_setup(grid_type=0, grid_flags=0x2000),
        results={"PointInBoxXZ": 0, "GetGroundHeight": 0})
    add("CollisionRayTest", (2 << 16, 0x800F6000, 0x800F8000),
        collision_ray_setup(grid_type=0, loop_existing=(1, 0x800F3000)),
        results={"PointInBoxXZ": 0})
    add("CollisionRayTest", (1 << 16, 0x800F6000, 0x800F8000),
        collision_ray_setup(collider_type=0, collider_base=0x800F5001),
        results={"PointInBoxXZ": 0})
    add("CollisionRayTest", (1 << 16, 0x800F6000, 0x800F8000),
        collision_ray_setup(collider_type=0),
        results={"PointInBoxXZ": 0})
    add("CollisionRayTest", (1 << 16, 0x800F6000, 0x800F8000),
        collision_ray_setup(collider_type=0, collider_base=0x800F5001,
                            loop_existing=(2, 0x800F5001)),
        results={"PointInBoxXZ": 0})
    add("CollisionRayTest", (2 << 16, 0x800F6000, 0x800F8000),
        collision_ray_setup(collider_type=0),
        results={"PointInBoxXZ": 0})
    add("CollisionRayTest", (2 << 16, 0x800F6000, 0x800F8000),
        collision_ray_setup(collider_type=0,
                            loop_existing=(2, 0x800F5000)),
        results={"PointInBoxXZ": 0})
    add("CollisionRayTest", (3 << 16, 0x800F6000, 0x800F8000),
        collision_ray_setup(collider_type=0),
        results={"PointInBoxXZ": 0})
    add("CollisionRayTest", (1 << 16, 0x800F6000, 0x800F8000),
        collision_ray_setup(ground_solid=-1), results={"GetGroundHeight": 0})
    add("CollisionRayTest", (1 << 16, 0x800F6000, 0x800F8000),
        collision_ray_setup(ground_solid=-1, loop_existing=(0, 0)),
        results={"GetGroundHeight": 0})
    for climb_type, half_y in ((59, 173), (89, 219)):
        def climb_setup(m, kind=climb_type, height=half_y):
            collision_ray_setup(collider_type=0x3000 | kind)(m)
            set16(m, 0x800F5008, height)
            set32(m, 0x800F600C, 5000)
            set16(m, 0x800B20CC, 1)
            set16(m, 0x800F44A0, 0)
        add("CollisionRayTest", (0, 0x800F6000, 0x800F8000), climb_setup,
            results={"PointInBoxXZ": 0,
                     "_climb_expected": (100, 200, 200, 20, half_y, int(climb_type == 59))})
    def pickup_update_setup(entries=(), player_position=(100, 128, 200),
                            rc_position=None, pool_available=True,
                            collision_slots_available=True):
        def setup(m):
            m.write_bytes(0x800BEFA0, bytes(32 * 0x18))
            m.write_bytes(0x800BEDA0, bytes([0xFF if pool_available else 0] * 256))
            m.write_bytes(0x800BBDA0, bytes(256 * 0x14))
            for slot_index in range(256):
                slot = 0x800BBDA0 + slot_index * 0x14
                set16(m, slot, -1 if collision_slots_available else 0)
                set16(m, slot + 2, -1)
            set32(m, 0x800B20D8, 0)
            set32(m, 0x800B20DC, 0)
            for offset, value in zip((0x24, 0x28, 0x2C), player_position):
                set32(m, 0x800EC1E0 + offset, value)
            set32(m, 0x800F5A20 + 0x70, 0 if rc_position is None else 1)
            if rc_position is not None:
                for offset, value in zip((0x24, 0x28, 0x2C), rc_position):
                    set32(m, 0x800F5A20 + offset, value)
            body_used = False
            for index, (kind, mode, position, velocity, age, phase, timer,
                        attached, contact) in enumerate(entries):
                item = 0x800BEFA0 + index * 0x18
                m.write8(item + 0x0A, kind)
                m.write8(item + 0x0B, 7)
                for offset, value in zip((4, 6, 8), position):
                    set16(m, item + offset, value)
                for offset, value in zip((0x0C, 0x0E, 0x10), velocity):
                    set16(m, item + offset, value)
                m.write8(item + 0x14, mode)
                m.write8(item + 0x15, age)
                m.write8(item + 0x16, phase)
                m.write8(item + 0x17, timer)
                if attached and not body_used:
                    body = 0x800BD1A0
                    slot = 0x800BBDA0
                    m.write_bytes(body, bytes(0x38))
                    set16(m, 0x800BEDA0, 1)
                    set32(m, body + 0x2C, (contact & 0xF) << 28)
                    set32(m, item, body)
                    set16(m, slot, 9)
                    set16(m, slot + 2, 0)
                    set32(m, slot + 4, body)
                    set32(m, slot + 8, 0)
                    set32(m, slot + 0x0C, 0)
                    set32(m, slot + 0x10, 0)
                    set32(m, 0x800B20D8, slot)
                    set32(m, 0x800B20DC, slot)
                    body_used = True
        return setup
    def pickup_entry(kind=1, mode=0, position=(100, 10, 200), velocity=(0, -10, 0),
                     age=0, phase=0, timer=0, attached=False, contact=0):
        return (kind, mode, position, velocity, age, phase, timer, attached, contact)
    add("PickupsUpdate", (), pickup_update_setup())
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(),)), results={"GetGroundHeight": 64})
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(),), player_position=(900, 900, 900)),
        results={"GetGroundHeight": 64})
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(position=(-200, 0, 300)),),
        player_position=(900, 900, 900), rc_position=(-200, 128, 300)),
        results={"GetGroundHeight": 64})
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(attached=True),), player_position=(900, 900, 900)),
        results={"GetGroundHeight": 64})
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(attached=True),), pool_available=False,
        collision_slots_available=True), results={"GetGroundHeight": 64})
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(attached=False),), collision_slots_available=False),
        results={"GetGroundHeight": 64, "Printf": 0})
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(kind=2, attached=True, contact=1),)),
        results={"GetGroundHeight": 64})
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(kind=4, attached=True, contact=1),)))
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(mode=1, timer=1, attached=True),)))
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(mode=1, timer=0),)))
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(mode=2, position=(100, 10, 200), velocity=(0, -10, 0), phase=1),)))
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(mode=3, kind=3, phase=1),)))
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(mode=3, kind=3, phase=2),)))
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(position=(100, -200, 200), age=90),)),
        results={"GetGroundHeight": 64})
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(velocity=(12, -10, -6)),)),
        results={"GetGroundHeight": 64})
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(position=(200, 200, 300)),),
        player_position=(100, 100, 200)), results={"GetGroundHeight": 64})
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(position=(200, 200, 300)),),
        player_position=(900, 900, 900), rc_position=(100, 100, 200)),
        results={"GetGroundHeight": 64})
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(kind=1, attached=True, contact=1),)))
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(mode=4),)))
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(position=(100, 200, 200), velocity=(0, 0, 0)),)),
        results={"GetGroundHeight": 64})
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(position=(100, 200, 200), velocity=(12, 0, -6)),)),
        results={"GetGroundHeight": 64})
    add("PickupsUpdate", (), pickup_update_setup(
        (pickup_entry(position=(100, -200, 200), age=0),)),
        results={"GetGroundHeight": 64})
    def items_update_draw_setup(flags):
        draw = item_draw_setup()
        def setup(m):
            draw(m)
            set32(m, 0x800F4350 + 0x100, flags)
        return setup
    add("ItemsUpdateDraw", (), items_update_draw_setup(0))
    add("ItemsUpdateDraw", (), items_update_draw_setup(1), area=0x07)
    def pickup_collect_setup(kind, score=0, gold=0, game_mode=0,
                             relic_count=0, relic_id=0x42, tally=0, target=2):
        def setup(m):
            set16(m, player + 0xE8, 4)
            set16(m, 0x800F4350 + 0x120, game_mode)
            set16(m, 0x800F4350 + 0x13C, gold)
            set16(m, 0x800F4350 + 0x168, score)
            set16(m, 0x800F4350 + 0x16A, relic_count)
            set16(m, 0x800F4350 + 0x126, relic_id)
            set16(m, 0x800F4350 + 0x154, target)
            set16(m, 0x800F4350 + 0x156, target)
            m.write8(0x800F5120 + 0xA1, tally)
            m.write8(0x800F5120 + 0xA2, tally)
            if kind != 4:
                set16(m, 0x800F44BA, 0)
            for index in range(128):
                set16(m, 0x800DFB70 + index * 2, 0x00FF)
        return setup
    for kind, score, gold, game_mode, relic_count, tally, target in (
            (0, 0, 0, 0, 0, 0, 2), (1, 0, 0, 0, 0, 0, 2),
            (2, 0, 99, 0, 0, 0, 2), (2, 0, 100, 0, 0, 0, 2),
            (3, 99, 100, 0, 0, 0, 2), (3, 0, 0, 19, 0, 0, 2),
            (4, 0, 0, 0, 60, 0, 2), (5, 99, 100, 0, 0, 0, 2),
            (5, 0, 0, 19, 0, 0, 2), (6, 0, 0, 0, 0, 1, 2),
            (7, 0, 0, 0, 0, 1, 2)):
        add("PickupCollect", (kind,), pickup_collect_setup(
            kind, score, gold, game_mode, relic_count, 0x42, tally, target),
            area=(0x16 if kind in (4, 6) else 0x07 if kind in (2, 7) else 0x01))

    # Sound and script leaves use real global images plus explicit, bounded records.
    for player_mode, distance, maximum, emitter_type in (
            (0, 2100, 2048, 0x4000), (0, 100, 2200, 0x4000),
            (0xC0, 100, 2200, 0x4000), (0, 2100, 3000, 0x3000),
            (0, 100, 3000, 0x3000), (0, 2200, 2187, 0x4000),
            (0, 2199, 2200, 0x4000)):
        def attenuation(m, player_mode=player_mode, distance=distance,
                        maximum=maximum, emitter_type=emitter_type):
            emitter, out = 0x800F1000, 0x800F1100
            set32(m, player + 0x70, player_mode)
            set16(m, emitter, emitter_type)
            set16(m, emitter + 4, 0)
            set16(m, emitter + 0x0C, 0)
            if emitter_type == 0x3000:
                set16(m, emitter + 6, 0)
                set16(m, emitter + 8, 0)
                set16(m, emitter + 0x0C, -distance)
                set16(m, emitter + 0x0E, maximum)
            else:
                set16(m, emitter + 6, distance)
                set16(m, emitter + 0x0A, maximum)
            set16(m, 0x800BF688, 0)
            set16(m, 0x800BF68A, 0)
        add("SoundEmitterAttenuation", (0x800F1000, 0x800F1100), attenuation)

    def sound_update_setup(emitter_rows, camera_mode=0, global_mode=0,
                           game_flags=0, track_state=0, track_mix=0,
                           valid_sound=False):
        def setup(m):
            for index in range(24):
                record = 0x800BF370 + index * 0x1C
                m.write_bytes(record, bytes([0] * 0x1C))
                set16(m, record, -1)
                set16(m, record + 2, -1)
            set32(m, state + 0x100, game_flags)
            set16(m, state + 0x12C, camera_mode)
            set16(m, state + 0x130, track_state)
            set16(m, state + 0x132, track_mix)
            m.write8(0x800E6908, global_mode)
            for offset, value in ((0, 100), (2, 120), (4, 140)):
                set16(m, 0x800BF688 + offset, value)
            for offset, value in ((0x24, 100), (0x28, 120), (0x2C, 140)):
                set32(m, player + offset, value)
            for index, row in enumerate(emitter_rows):
                kind, active, handle, coords, maximum = row
                record = 0x800BF370 + index * 0x1C
                set16(m, record, kind)
                set16(m, record + 2, handle)
                set16(m, record + 0x12, active)
                for offset, value in zip((4, 6, 8), coords):
                    set16(m, record + offset, value)
                if (kind & 0xF000) == 0x3000:
                    set16(m, record + 0x0E, maximum)
                else:
                    set16(m, record + 0x0A, maximum)
                set16(m, record + 0x10, 0)
                if (kind & 0xF000) == 0x6000:
                    actor = 0x800E5500
                    for offset, value in zip((0x24, 0x28, 0x2C), coords):
                        set16(m, actor + offset, value)
                elif (kind & 0xF000) == 0x7000:
                    position = 0x800F8000
                    set32(m, record + 0x18, position)
                    for offset, value in zip((0, 4, 8), coords):
                        set16(m, position + offset, value)
            if valid_sound:
                header = 0x801C0000
                table = header + 0x100
                m.write_bytes(0x800E1180, bytes(256 * 12))
                set32(m, 0x800E1180, 1)
                set32(m, 0x800E1184, header)
                m.write_bytes(header, bytes(0x400))
                set32(m, header + 0x14, 0x300)
                set32(m, header + 0x1C, 0x100)
                set32(m, header + 0x20, 0x200)
                set16(m, table, 2)
                set16(m, table + 4, 0x21)
                set16(m, table + 0x20, 2)
                set16(m, table + 0x24, 0x80)
                m.write8(0x800F75C3, 1)
                m.write_bytes(0x800F5AC0, bytes(24 * 0x120))
                set16(m, 0x800B2D08, 0)
                set16(m, 0x800B2D0A, 1)
                set16(m, 0x800B2D0C, 1)
                set16(m, 0x800BF670, 0)
            else:
                # A failed loop start returns before touching audio callbacks.
                set16(m, 0x800B2D08, 0)
                set16(m, 0x800BF670, -1)
        return setup
    near_emitters = (
        (0x2000, 1, 3, (100, 120, 140), 0x2000),
        (0x3000, 1, 4, (100, 120, 140), 3000),
        (0x4000, 1, 5, (100, 120, 140), 3000),
        (0x5000, 1, 6, (100, 120, 140), 3000),
        (0x6000, 1, 7, (100, 120, 140), 0x2000),
        (0x7000, 1, 8, (100, 120, 140), 0x2000),
        (0x8000, 1, 9, (100, 120, 140), 3000),
    )
    add("SoundEmitterUpdate", (), sound_update_setup(near_emitters),
        area=0x44, results={"SpuWaitIdle": 0})
    add("SoundEmitterUpdate", (), sound_update_setup(
        ((0x4000, 1, 12, (30000, 120, -30000), 20),)),
        area=0x07, results={"SpuWaitIdle": 1})
    add("SoundEmitterUpdate", (), sound_update_setup(
        ((0x4000, 0, 13, (100, 120, 140), 3000),),
        camera_mode=24, global_mode=1, track_state=2, track_mix=17),
        area=0x16, results={"SpuWaitIdle": 0})
    add("SoundEmitterUpdate", (), sound_update_setup(
        ((0x4000, 0, 14, (100, 120, 140), 3000),)),
        area=0x01, results={"SpuWaitIdle": 0})
    add("SoundEmitterUpdate", (), sound_update_setup(
        ((0x4000, 1, 17, (100, 120, 140), 3000),),
        camera_mode=24, global_mode=1, track_state=2, track_mix=17),
        area=0x16, results={"SpuWaitIdle": 0})
    add("SoundEmitterUpdate", (), sound_update_setup(
        ((0x6000, 1, 15, (100, 120, 140), 0x2000),), game_flags=2),
        area=0x07, results={"SpuWaitIdle": 0})
    add("SoundEmitterUpdate", (), sound_update_setup(
        ((0x7000, 1, 16, (100, 120, 140), 0x2000),), game_flags=4),
        area=0x44, results={"SpuWaitIdle": 0})
    add("SoundEmitterUpdate", (), sound_update_setup(
        ((0x6000, 0, 18, (100, 120, 140), 0x2000),)),
        area=0x01, results={"SpuWaitIdle": 0})
    add("SoundEmitterUpdate", (), sound_update_setup(
        ((0x7000, 0, 19, (100, 120, 140), 0x2000),),
        track_state=2, track_mix=17, valid_sound=True),
        area=0x16, results={"SpuWaitIdle": 0})
    add("SoundEmitterUpdate", (), sound_update_setup(
        ((0x7000, 0, 21, (100, 120, 140), 0x2000),)),
        area=0x07, results={"SpuWaitIdle": 0})
    add("SoundEmitterUpdate", (), sound_update_setup(
        ((0x1000, 1, 20, (100, 120, 140), 0),), track_state=2, track_mix=17),
        area=0x44, results={"SpuWaitIdle": 0})
    def collider_run(m):
        collider, commands = 0x800F3000, 0x800F4000
        set32(m, 0x800F4350 + 0x218, commands)
        for offset, value in ((0x00, 90), (0x02, 100), (0x04, -20), (0x18, 0), (0x30, 0)):
            set16(m, collider + offset, value)
        for axis, value in ((0x24, 100), (0x28, 80), (0x2C, 0)):
            set32(m, player + axis, value)
        set16(m, commands, 3)
        set16(m, commands + 0x0E, 0xFFFF)
    add("ColliderScriptRun", (0x800F3000, 0), collider_run)
    def collider_script_value(m, data_kind=1):
        collider, commands = 0x800F3000, 0x800F4000
        record, output = 0x800F5000, 0x800F6000
        set32(m, 0x800F4350 + 0x218, commands)
        set32(m, 0x800F4350 + 0x20C, record)
        set32(m, 0x800F4350 + 0x214, output)
        for offset, value in ((0x00, 90), (0x02, 100), (0x04, -20), (0x18, 0), (0x30, 0)):
            set16(m, collider + offset, value)
        set16(m, commands, 5)
        set16(m, commands + 2, 80)
        set16(m, commands + 4, 0)
        set16(m, commands + 6, 0)
        set16(m, commands + 8, 0x1234)
        set16(m, commands + 10, 0x5678)
        set16(m, commands + 0x0E, 0xFFFF)
        set16(m, record + 0x18, data_kind)
    add("ColliderScriptRun", (0x800F3000, 0), collider_script_value)
    add("ColliderScriptRun", (0x800F3000, 0), lambda m: collider_script_value(m, 0))
    add("ColliderScriptsUpdate", (), lambda m: (
        set32(m, state + 0x100, 0), set16(m, state + 0x284, 0)))
    for record_kind, cursor, unaligned in ((1, 0, False), (2, -1, False),
                                            (2, 0, False), (2, -1, True)):
        def scripts_update(m, record_kind=record_kind, cursor=cursor,
                           unaligned=unaligned):
            record, work, commands = ((0x800F3002, 0x800F4002, 0x800F5000)
                                      if unaligned else
                                      (0x800F3000, 0x800F4000, 0x800F5000))
            set32(m, state + 0x20C, record)
            set16(m, state + 0x284, 1)
            set32(m, 0x800B3340, work)
            set32(m, 0x800B3344, 0x800B3000)
            m.write_bytes(record, bytes((index * 17 + 3) & 0xFF for index in range(0x38)))
            m.write8(record + 0x12, 0)
            set16(m, record + 0x18, record_kind)
            set16(m, record + 0x30, cursor)
            set32(m, state + 0x218, commands)
            set16(m, commands, 3)
            set16(m, commands + 0x0E, 0xFFFF)
        add("ColliderScriptsUpdate", (), scripts_update,
            area=(0x44 if record_kind == 2 else 0x07))

    def zone_setup(rows):
        def setup(m):
            set32(m, 0x800B4D30, len(rows))
            set32(m, 0x800B4D34, 0x800F1000)
            for i, row in enumerate(rows):
                for off, value in zip((0, 2, 4, 6, 8, 10, 12), row):
                    set16(m, 0x800F1000 + i * 0x10 + off, value)
        return setup
    def pad_rumble_setup(m):
        m.write8(0x800E5468, 0xA5)
        m.write8(0x800E5469, 0x5A)
    add("PadRumbleReset", (0,), pad_rumble_setup, area=0x01)
    add("PadRumbleReset", (1,), pad_rumble_setup, area=0x44)
    add("PadRumbleReset", (2,), pad_rumble_setup, area=0x53)

    add("GetZoneMask", (0, 0, 0, 0), zone_setup(((0, 0, 0, 10, 10, 10, 1),
         (12, 0, 0, 5, 5, 5, 2))), area=0x01)
    add("GetZoneMask", (-10, -9, -10, 1), zone_setup(((0, 0, 0, 11, 11, 11, 4),)), area=0x44)
    add("GetZoneMask", (200, 200, 200, 0), zone_setup(()), area=0x07)
    add("GetZoneMask", (10, 10, 10, 0), zone_setup(((0, 0, 0, 20, 20, 20, 8),)), area=0x53)

    add("StageExitPending", (), lambda m: set16(m, 0x800B8CE0, 0), area=0x01)
    add("StageExitPending", (), lambda m: set16(m, 0x800B8CE0, -1), area=0x53)
    add("PlayerIsSubmerged", (), lambda m: (
        m.write8(0x800BAD7B, 0), set32(m, player + 0x28, 400), set16(m, player + 0x13C, 0)))
    add("PlayerIsSubmerged", (), lambda m: (
        m.write8(0x800BAD7B, 1), set32(m, player + 0x28, 400), set16(m, player + 0x13C, 100)), area=0x53)
    add("PlayerIsSubmerged", (), lambda m: (
        m.write8(0x800BAD7B, 1), set32(m, player + 0x28, 200), set16(m, player + 0x13C, 0)), area=0x07)

    def rc_car_setup(area_tag=7, mode=10, current=0, car=0x800F8000):
        def setup(m):
            m.write8(0x800EC2D2, area_tag)
            set32(m, 0x800F5A90, car)
            set16(m, 0x800F4350 + 0x150, current)
            set16(m, 0x800F4350 + 0x126, mode)
        return setup
    add("CameraRcCarActive", (), rc_car_setup(mode=10))
    add("CameraRcCarActive", (), rc_car_setup(mode=0x61), area=0x53)
    add("CameraRcCarActive", (), rc_car_setup(area_tag=6), area=0x07)
    add("CameraRcCarActive", (), rc_car_setup(current=1), area=0x44)
    for boss_area in (0x44, 0x53, 0x56, 0x46, 0x57):
        def boss_setup(m, boss_area=boss_area):
            set16(m, 0x800F4476, boss_area)
            m.write8(0x800AF840, 0)
        add("BossUpdate", (0x800F2000,), boss_setup, area=boss_area,
            results={"FUN_801402d8": 0, "Area53_BossUpdate": 1,
                     "FUN_80143854": 1, "Area46_BossUpdate": 0,
                     "Area57_BossUpdate": 1})
    add("BossUpdate", (0x800F2000,), lambda m: m.write8(0x800AF840, 1), area=0x01)

    def vram_scroll_setup(rows, frame=9, area_index=0):
        def setup(m):
            set32(m, 0x800B98F0, 1)
            set32(m, 0x800B98F4, 0x800F1000)
            set32(m, 0x800EC350, frame)
            set32(m, 0x800EC354, 0x800F7000)
            set32(m, 0x800EC35C, area_index)
            set32(m, 0x800F7000, 0xAA000001)
            set32(m, 0x800F03CC + area_index * 0x4078, 0xBB000002)
            for i, row in enumerate(rows):
                m.write_bytes(0x800F1000 + i * 0x10, bytes(row))
            m.write8(0x800F1000 + len(rows) * 0x10, 0)
        return setup
    add("VramScrollUpdate", (), vram_scroll_setup((
        (1, 0, 1, 0, 40, 50, 2, 3, 4, 5, 10, 11, 12, 5, 0, 4),
        (2, 0, 8, 9, 10, 11, 12, 13, 1, 0, 14, 15, 16, 2, 0, 4),
        (3, 0, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33))),
        area=0x44)
    add("VramScrollUpdate", (), vram_scroll_setup(((1, 1, 0, 0, 2, 3, 4, 5,
        6, 7, 8, 9, 10, 4, 1, 2), (2, 1, 0, 0, 1, 2, 3, 4,
        5, 6, 7, 8, 9, 2, 0, 4))), area=0x07)
    add("VramScrollUpdate", (), vram_scroll_setup((
        (4, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12),), frame=-8), area=0x46)
    add("VramScrollUpdate", (), vram_scroll_setup((
        (2, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1, 4),), frame=-8), area=0x53)
    add("VramScrollUpdate", (), vram_scroll_setup((
        (1, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 4),), frame=8), area=0x53)
    add("VramScrollUpdate", (), vram_scroll_setup((
        (1, 0, 0, 0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 4),), frame=8), area=0x57)
    add("VramScrollUpdate", (), vram_scroll_setup((
        (2, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 1),), frame=0), area=0x57)
    add("VramScrollUpdate", (), lambda m: set32(m, 0x800B98F0, 0), area=0x01)

    add("Indicator_Register", (2, 0x800F1001, -123), lambda m: (
        set32(m, 0x800BBC50, 0), m.write_bytes(0x800F1001, bytes(range(8)))), area=0x01)
    add("Indicator_Register", (1, 0x800F2000, 0x400), lambda m: (
        set32(m, 0x800BBC50, 3), m.write_bytes(0x800F2000, bytes((0xEE, 0x11, 0x22, 0x33,
                                                                    0x44, 0x55, 0x66, 0x77)))), area=0x53)
    add("Indicator_Register", (3, 0x800F3000, 1024), lambda m: (
        set32(m, 0x800BBC50, 4), m.write_bytes(0x800F3000, bytes(range(8)))),
        results={"exit": 0}, area=0x07)
    add("MenuItemSetRange", (0x800F1000, 0xFFFFFFFF, 0x80000000))
    add("MenuItemSetStep", (0x800F1000, 0x12345678), area=0x07)
    add("HudFindGadgetIconPos", (), lambda m: (
        m.write8(0x800EC2D2, 3), m.write_bytes(0x800F5120 + 0x88, bytes((1, 2, 3, 4))),
        set16(m, 0x800B2234 + 2 * 4, 100), set16(m, 0x800B2234 + 2 * 4 + 2, 200)), area=0x44)
    add("HudFindGadgetIconPos", (), lambda m: (
        m.write8(0x800EC2D2, 99), m.write_bytes(0x800F5120 + 0x88, bytes((1, 2, 3, 4))),
        set16(m, 0x800B2234, 90), set16(m, 0x800B2236, 120)), area=0x07)
    add("ColorFadeReset", ())
    add("ProgressFlagGet", (0xFFFFFFFF,), lambda m: set32(m, 0x800DFCBC, 0x80000000))
    add("ProgressFlagGet", (33,), lambda m: set32(m, 0x800DFCC0 + 4, 2), area=0x53)

    def follow_path_setup(route_duration=5, speed=1, route_index=0, prop_index=-1,
                          next_duration=0):
        def setup(m):
            route_table, collider = 0x800F1000, 0x800F3000
            set32(m, 0x800F4564, route_table)
            set32(m, 0x800B4D64, 0x800F5000)
            set32(m, 0x800E68D8, 0x800F6000)
            m.write8(0x800E68D0 + 0x38, 0)
            set32(m, 0x800F4350 + 0x100, 0)
            set16(m, collider + 0x22, route_index)
            set16(m, collider + 0x0C, speed)
            set16(m, collider + 0x0E, prop_index)
            set16(m, collider + 0x12, 0x3000)
            set16(m, collider + 0x24, 120)
            set16(m, collider + 0x26, 40)
            set16(m, collider + 0x28, -80)
            set16(m, collider + 0x00, 100)
            set16(m, collider + 0x02, 20)
            set16(m, collider + 0x04, -100)
            for off, val in zip((0, 2, 4, 0x0C, 0x0E), (200, 60, -20, route_duration, 1)):
                set16(m, route_table + off, val)
            for off, val in zip((0, 2, 4, 0x0C, 0x0E),
                                (220, 70, 0, next_duration, 0)):
                set16(m, route_table + 0x10 + off, val)
            set16(m, route_table + 0x20 + 0x0E, 0)
            set16(m, player + 0x54, 0)
            set16(m, 0x800F3000 + 0x38, 0)
            set16(m, 0x800F3000 + 0x3A, 0)
        return setup
    add("ColliderScriptFollowPath", (0x800F3000, 0), follow_path_setup(), area=0x44,
        results={"GetColliderQuadrantMaskXZ": 0x1234})
    add("ColliderScriptFollowPath", (0x800F3000, 0),
        follow_path_setup(speed=10, prop_index=0), area=0x53,
        results={"GetColliderQuadrantMaskXZ": 0x4321})
    add("ColliderScriptFollowPath", (0x800F3000, 0),
        follow_path_setup(speed=10, next_duration=3), area=0x57,
        results={"GetColliderQuadrantMaskXZ": 0x00FF})
    add("ColliderScriptFollowPath", (0x800F3000, 0), follow_path_setup(route_index=-1), area=0x07)

    def collider_player_move_setup(motion_type=0x100, player_mode=0x80, inside=True):
        def setup(m):
            collider = 0x800F3000
            set32(m, 0x800F5A90, 0x800F5A20)
            set32(m, 0x800E68D8, 0x800F6000)
            m.write8(0x800E6908, 0)
            set32(m, 0x800F44E4, 0x800F7000)
            set32(m, 0x800F7000, 2)
            set16(m, collider + 0x12, motion_type)
            set16(m, collider + 0x1C, 2)
            for off, val in zip((0, 2, 4, 6, 8, 10, 0x2A, 0x2C, 0x2E),
                                (100, 100, 100, 30, 30, 30, 2, 4, 6)):
                set16(m, collider + off, val)
            for off, val in zip((0x24, 0x28, 0x2C), ((100, 100, 100) if inside else (1000, 1000, 1000))):
                set32(m, player + off, val)
            set32(m, player + 0x70, player_mode)
            set16(m, player + 0x54, 0)
            set16(m, player + 0x46, 0)
            set32(m, 0x800F5A20 + 0x70, 1)
            for off, val in zip((0x24, 0x28, 0x2C), (100, 100, 100)):
                set32(m, 0x800F5A20 + off, val)
            set32(m, 0x800F4350 + 0x100, 0)
            set16(m, 0x800F4476, 0)
        return setup
    add("ColliderScriptPlayerMove", (0x800F3000, 0), collider_player_move_setup(), area=0x44,
        results={"GetSolidHeight": 200, "GetColliderQuadrantMaskXZ": 0})
    add("ColliderScriptPlayerMove", (0x800F3000, 0), collider_player_move_setup(0x300, 0x60), area=0x53,
        results={"GetSolidHeight": 100, "GetColliderQuadrantMaskXZ": 0})
    add("ColliderScriptPlayerMove", (0x800F3000, 0), collider_player_move_setup(0x100, 0x80, False), area=0x07)
    add("ColliderScriptPlayerMove", (0x800F3000, 0),
        collider_player_move_setup(0x200, 0x80), area=0x16)
    add("ColliderScriptPlayerMove", (0x800F3000, 0),
        collider_player_move_setup(0x400, 0x80), area=0x44)
    add("ColliderScriptPlayerMove", (0x800F3000, 0),
        collider_player_move_setup(0x100, 0xC0), area=0x53)
    add("ColliderScriptPlayerMove", (0x800F3000, 0),
        collider_player_move_setup(0x300, 0x80), area=0x57)
    def player_move_area_29(m):
        collider_player_move_setup(0x300, 0x80)(m)
        set16(m, 0x800F4476, 29)
    add("ColliderScriptPlayerMove", (0x800F3000, 0), player_move_area_29,
        area=0x44, results={"FUN_8013fc18": 0})
    add("ColliderScriptPlayerMove", (0x800F3000, 0),
        collider_player_move_setup(0x300, 0x60), area=0x57,
        results={"GetSolidHeight": -100, "AnimFramePtr": 0x800F7000,
                 "GetColliderQuadrantMaskXZ": 0})
    add("ColliderScriptPlayerMove", (0x800F3000, 0),
        collider_player_move_setup(0x800, 0x80), area=0x16,
        results={"FUN_8013fc18": 0})
    add("ColliderScriptPlayerMove", (0x800F3000, 0),
        collider_player_move_setup(0x300, 0x60), area=0x44,
        results={"GetSolidHeight": 200, "GetGroundHeight": 200,
                 "AnimFramePtr": 0x800F7000,
                 "GetColliderQuadrantMaskXZ": 0})

    def collider_carry_setup(prop_index=-1, flow=(0, 0, 0)):
        def setup(m):
            collider = 0x800F3000
            set32(m, 0x800B4D64, 0x800F5000)
            set16(m, collider + 0x0E, prop_index)
            for off, val in zip((0, 2, 4, 0x24, 0x26, 0x28, 0x2A, 0x2C, 0x2E),
                                (110, 120, 130, 100, 100, 100, *flow)):
                set16(m, collider + off, val)
            set16(m, 0x800EC238, 0)
            set16(m, player + 0x54, 0)
            set32(m, 0x800F4350 + 0x100, 0)
            set32(m, 0x800E68D8, 0x800F6000)
        return setup
    add("ColliderScriptCarry", (0x800F3000, 0), collider_carry_setup(0, (1, 0, 0)), area=0x44,
        results={"GetColliderQuadrantMaskXZ": 0})
    add("ColliderScriptCarry", (0x800F3000, 0), collider_carry_setup(-1, (0, 0, 0)), area=0x44,
        results={"GetColliderQuadrantMaskXZ": 0})
    add("ColliderScriptCarry", (0x800F3000, 1), collider_carry_setup(-1), area=0x07,
        results={"GetColliderQuadrantMaskXZ": 0})

    def reset_collider_setup(prop_index):
        def setup(m):
            collider = 0x800F3000
            set32(m, 0x800B4D64, 0x800F5000)
            set16(m, collider + 0x18, 0x8002)
            set16(m, collider + 0x0E, prop_index)
            for off, val in zip((0, 2, 4, 0x24, 0x26, 0x28), (10, 20, 30, 100, 200, 300)):
                set16(m, collider + off, val)
            set16(m, collider + 0x0C, 14)
            set16(m, collider + 0x32, 16)
        return setup
    add("ColliderResetToStart", (0x800F3000,), reset_collider_setup(1))
    add("ColliderResetToStart", (0x800F3000,), reset_collider_setup(-1), area=0x53)

    def command_setup(letter, phase=1, kind=0, timer=0, distance=25,
                      body_index=0, player_mode=0, state_byte=0,
                      command_value=50, radius=40, travel=20, duration=20,
                      delay=60, referenced_index=0, referenced_phase=0):
        def setup(m):
            collider, command = 0x800F3000, 0x800F4000
            set16(m, collider + 0x12, kind)
            set16(m, collider + 0x18, phase)
            set16(m, collider + 0x32, timer)
            set16(m, collider + 0x34, state_byte)
            set16(m, collider + 0x00, 100)
            set16(m, collider + 0x02, 100)
            set16(m, collider + 0x04, 100)
            set16(m, collider + 0x24, 100)
            set16(m, collider + 0x26, 100)
            set16(m, collider + 0x28, 100)
            set16(m, player + 0x58, body_index)
            set32(m, player + 0x70, player_mode)
            set16(m, command + 2, ord(letter))
            set16(m, command + 4, command_value)
            set16(m, command + 6, radius)
            set16(m, command + 8, duration)
            set16(m, command + 10, delay)
            set16(m, command + 12, 3)
            set16(m, 0x800F4476, 0)
            set32(m, 0x800F455C, 0x800F5000)
            set16(m, 0x800F5000 + referenced_index * 0x38 + 0x18,
                  referenced_phase)
        return setup
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 25, 0), command_setup("C", distance=25))
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 60, 0), command_setup("C", distance=60), area=0x53)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 10, 0), command_setup("M", phase=1, kind=0), area=0x07)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 10, 2), command_setup("M", phase=2, kind=0), area=0x44)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 10, 0),
        command_setup("M", phase=1, player_mode=0x60), area=0x53)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 10, 0),
        command_setup("M", phase=2), area=0x57)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 10, 0),
        command_setup("M", phase=2, state_byte=1), area=0x16)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 20, 0), command_setup("S", phase=1), area=0x53)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 20, 0),
        command_setup("S", phase=1, command_value=0, referenced_phase=2), area=0x57)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 20, 0), command_setup("T", phase=1), area=0x46)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 60, 0),
        command_setup("T", phase=2, kind=2), area=0x53)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 10, 0),
        command_setup("T", phase=1, command_value=-1), area=0x57)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 20, 0), command_setup("Z", phase=2), area=0x57)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 20, 0),
        command_setup("Z", phase=2, state_byte=1), area=0x46)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 20, 0),
        command_setup("Z", phase=2, timer=9, state_byte=1, kind=0x300), area=0x53)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 20, 0),
        command_setup("Z", phase=2, timer=3, state_byte=1, kind=0x300), area=0x46)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 20, 0),
        command_setup("Z", phase=2, timer=20, state_byte=1, kind=0x300), area=0x57)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 20, 0),
        command_setup("Z", phase=2, timer=60, state_byte=0, delay=60), area=0x01)
    add("ColliderScriptCommand", (0x800F3000, 0x800F4000, 20, 0),
        command_setup("Z", phase=2, radius=0), area=0x44)

    def area_feature_setup(state_byte, started=0, finished=0, progress=0):
        def setup(m):
            m.write8(0x800F5AA0, state_byte)
            m.write8(0x800F5AA1, started)
            m.write8(0x800F5AA2, finished)
            m.write8(0x800F5AA3, 7)
            set16(m, 0x800F5AA4, progress)
            set32(m, 0x800E3890, 0)
            set16(m, 0x800E3890 + 0x74, 0)
            set16(m, 0x800E3890 + 0x76, 0)
            set32(m, 0x800B7E50 + 0x180 * 4, 0x00012345)
            set16(m, 0x800F4476, 0)
        return setup
    add("AreaFeature30Update", (), area_feature_setup(1), area=0x44,
        results={"CdIntToPos": 0})
    add("AreaFeature30Update", (), area_feature_setup(1, 1, 1, 224), area=0x53,
        results={"FUN_80137248": 0})
    add("AreaFeature30Update", (), area_feature_setup(2, 1, 0, 32), area=0x46)
    add("AreaFeature30Update", (), area_feature_setup(2, 1, 0, 0), area=0x57)
    add("AreaFeature30Update", (), area_feature_setup(3, 1, 0, 0), area=0x01)

    for mode, active, program in ((0, 0, 0), (0x80, 0, 22), (0, 1, 42),
                                  (0x80, 0, 42), (0, 1, 22)):
        def bgm(m, mode=mode, active=active, program=program):
            track_index = 3
            track = 0x800BF670
            seq_track = 0x800F5AC0 + track_index * 0x120
            m.write8(track + 0x24, 1)
            set16(m, track + 6, track_index)
            set16(m, track + 8, active)
            set16(m, track + 0x0C, 100)
            m.write8(seq_track + 0x24, 1)
            set32(m, seq_track + 0x10, 0x800F8000)
            set32(m, player + 0x70, mode)
            for channel in range(16):
                m.write8(0x800F8000 + channel * 0x10 + 0x12, program)
        add("BgmApplyPlayerEffect", (), bgm)

    # Add seeded, randomized values alongside the hand-picked edge cases. All
    # addresses and pointer-bearing structures stay in valid RAM regions.
    for random_round in range(max(1, int(random_rounds))):
        rng = random.Random(seed + random_round * 0x9E3779B1)

        def random_s16():
            return rng.randint(-32768, 32767)

        angle = random_s16()
        add("MatrixRotYMul", (angle, 0x800F1200))

        camera_coords = [random_s16() for _ in range(6)]
        camera_delta = tuple(random_s16() for _ in range(3))
        def random_camera(m, coords=camera_coords):
            set32(m, 0x800E68D8, 0x800F1000)
            m.write8(0x800E68D0 + 0x38, 0)
            set32(m, state + 0x100, 0)
            for offset, value in zip((0x0A, 0x0C, 0x0E, 0x12, 0x14, 0x16), coords):
                set16(m, 0x800F1000 + offset, value)
        add("CameraNudge", camera_delta, random_camera)

        timer_start = rng.randrange(0x10000)
        timer_seconds = rng.getrandbits(32)
        timer_seconds = s32(timer_seconds)
        add("StageTimerAdd", (timer_seconds,),
            lambda m, value=timer_start: set16(m, 0x800B01C2, value))

        heal_amount, initial_health = random_s16(), random_s16()
        add("PlayerHeal", (heal_amount,),
            lambda m, value=initial_health: set16(m, player + 0xE8, value))

        eye_mode = rng.randrange(13)
        eye_health = rng.randint(-2, 7)
        eye_feature = rng.randrange(2)
        eye_frame = rng.randrange(1000)
        eye_actor_mode = rng.randrange(9)
        def random_eye(m, health=eye_health, mode=eye_mode, feature=eye_feature,
                       actor_mode=eye_actor_mode, frame=eye_frame):
            set16(m, player + 0xE8, health)
            set16(m, state + 0x150, mode)
            m.write8(0x800F5AA0, feature)
            set32(m, 0x800B4D70, 0x800F1200)
            set16(m, 0x800F1286, actor_mode)
            set32(m, 0x800EC350, frame)
        add("PlayerEyeBlinkUpdate", (), random_eye)

        text_pointer = 0x800F9000 + rng.randrange(0x100) * 4
        add("MessageBoxSetText", (text_pointer,))
        add("MessageBoxBusy", (),
            lambda m, busy=rng.randrange(256): m.write8(0x800F5A00, busy))
        panel_mode, panel_argument = s32(rng.getrandbits(32)), s32(rng.getrandbits(32))
        add("HudPanelB_Start", (panel_mode, panel_argument))
        panel_counter = random_s16()
        counter_argument = s32(rng.getrandbits(32))
        add("HudCounterC_Start", (counter_argument,),
            lambda m, value=panel_counter: set16(m, 0x800F4DB0, value))
        add("AreaFeature30GetState", (),
            lambda m, value=rng.randrange(256): m.write8(0x800F5AA0, value))

        fade_track = rng.randrange(8)
        fade_program = rng.randrange(128)
        fade_volume = rng.randint(-64, 127)
        fade_frames = rng.randint(-4, 120)
        def random_seq_fade(m, track_index=fade_track):
            set32(m, 0x800F5AC0 + track_index * 0x120 + 0x10, 0x800F8000)
            m.write8(0x800F5AC0 + track_index * 0x120 + 0x24, 1)
        add("SeqFadeTrackProgram", (fade_track, fade_program, fade_volume, fade_frames),
            random_seq_fade)

        seq_track, seq_channel, seq_force = rng.randrange(8), rng.randrange(16), rng.randrange(2)
        seq_program = rng.choice([rng.randrange(128), 0xFF])
        def random_seq_get(m, track_index=seq_track, channel_index=seq_channel,
                           program_number=seq_program):
            track = 0x800F5AC0 + track_index * 0x120
            m.write8(track + 0x24, 1)
            set32(m, track + 0x10, 0x800F8000)
            m.write8(0x800F8000 + channel_index * 0x10 + 0x12, program_number)
        add("SeqGetChannelProgram", (seq_track, seq_channel, seq_force), random_seq_get)

        voice, left, right = rng.randrange(8), random_s16(), random_s16()
        add("SfxVoiceSetPanVol", (voice, left, right),
            results={"SpuWaitIdle": rng.randrange(2)})

        atten_type = rng.choice((0x3000, 0x4000))
        listener_x, listener_y, listener_z = (rng.randint(-2000, 2000) for _ in range(3))
        emitter_x, emitter_y, emitter_z = (rng.randint(-2000, 2000) for _ in range(3))
        emitter_y2 = rng.randint(-500, 500)
        dx = listener_y - (emitter_y - emitter_y2)
        dz = listener_z - emitter_z if atten_type == 0x3000 else listener_x - emitter_x
        max_range = min(30000, math.isqrt(dx * dx + dz * dz) + rng.randint(700, 5000))
        def random_attenuation(m, sound_type=atten_type, lx=listener_x, ly=listener_y,
                               lz=listener_z, ex=emitter_x, ey=emitter_y,
                               ez=emitter_z, ey2=emitter_y2, limit=max_range):
            set16(m, 0x800EC250, 0)
            for offset, value in zip((0, 2, 4), (lx, ly, lz)):
                set16(m, 0x800BF688 + offset, value)
            set16(m, 0x800F1000, sound_type)
            set16(m, 0x800F1004, ex)
            set16(m, 0x800F1006, ey)
            set16(m, 0x800F1008, ez)
            set16(m, 0x800F100C, ey2)
            set16(m, 0x800F100A, limit)
            set16(m, 0x800F100E, limit)
        add("SoundEmitterAttenuation", (0x800F1000, 0x800F1100), random_attenuation)

        record_position = tuple(random_s16() for _ in range(3))
        pickup_parameter = rng.randrange(256)
        def random_pickup_position(m, coords=record_position, parameter=pickup_parameter):
            set32(m, 0x800F2000, 0x800F3000)
            for offset, value in zip((4, 6, 8), coords):
                set16(m, 0x800F2000 + offset, value)
            m.write8(0x800F200A, parameter)
        add("PickupSetPosition", (0x800F2000,), random_pickup_position)

        collected_id = rng.randrange(256)
        def random_collected(m):
            set16(m, 0x800F44BA, 0)
            for index in range(128):
                set16(m, 0x800DFB70 + index * 2, 0x00FF)
        add("CollectedItemsAdd", (collected_id,), random_collected)

        ray_values = tuple(random_s16() for _ in range(3))
        ray_radius, ray_height = rng.randrange(2048), rng.randrange(4096)
        ray_mode = rng.choice((0, 1 << 16, 2 << 16, 3 << 16))
        ray_ground = rng.randint(-256, 512)
        base_ray_setup = collision_ray_setup(ground_solid=-1,
                                             ground_result=rng.randint(-64, 256))
        def random_ray_setup(m, setup=base_ray_setup, coords=ray_values,
                             radius=ray_radius, height=ray_height):
            setup(m)
            for offset, value in zip((0, 4, 8, 12, 16),
                                     (*coords, radius, height)):
                set32(m, 0x800F6000 + offset, value)
        add("CollisionRayTest", (ray_mode, 0x800F6000, 0x800F8000),
            random_ray_setup, results={"GetGroundHeight": ray_ground})

        actor_position = tuple(rng.randint(-1000, 1000) for _ in range(3))
        player_position = tuple(rng.randint(-1000, 1000) for _ in range(3))
        actor_timer, actor_state = rng.randrange(256), rng.randrange(16)
        random_player_mode = rng.choice((0x10, 0x40, 0x80))
        def random_actor_near(m, actor_xyz=actor_position, player_xyz=player_position,
                              timer=actor_timer, mode=actor_state,
                              player_mode=random_player_mode):
            actor = 0x800F2000
            set32(m, 0x800B4DC4, actor)
            for offset, value in zip((0x24, 0x28, 0x2C), actor_xyz):
                set32(m, actor + offset, value)
            m.write8(actor + 0x8F, timer)
            m.write8(actor + 0x86, mode)
            for offset, value in zip((0x24, 0x28, 0x2C), player_xyz):
                set32(m, player + offset, value)
            set32(m, player + 0x70, player_mode)
        add("SpecialActorPlayerNear", (), random_actor_near)

        collider_case = next(case for case in cases if case[0] == "ColliderScriptRun")
        collider_setup = collider_case[2]
        collider_xyz = tuple(random_s16() for _ in range(3))
        player_xyz = tuple(random_s16() for _ in range(3))
        def random_collider_run(m, setup=collider_setup, collider_pos=collider_xyz,
                                player_pos=player_xyz):
            setup(m)
            for offset, value in zip((0, 2, 4), collider_pos):
                set16(m, 0x800F3000 + offset, value)
            for offset, value in zip((0x24, 0x28, 0x2C), player_pos):
                set32(m, player + offset, value)
        add("ColliderScriptRun", collider_case[1], random_collider_run)

        collect_case = next(case for case in cases if case[0] == "PickupCollect")
        collect_setup = collect_case[2]
        collect_gold, collect_score = rng.randrange(300), rng.randrange(5000)
        collect_tally, collect_target = rng.randrange(10), rng.randrange(1, 12)
        def random_collect(m, setup=collect_setup, gold=collect_gold, score=collect_score,
                           tally=collect_tally, target=collect_target):
            setup(m)
            set16(m, state + 0x13C, gold)
            set16(m, state + 0x168, score)
            m.write8(0x800F5120 + 0xA1, tally)
            m.write8(0x800F5120 + 0xA2, tally)
            set16(m, state + 0x154, target)
            set16(m, state + 0x156, target)
        add("PickupCollect", collect_case[1], random_collect)

        active_kinds = tuple(rng.randint(1, 7) for _ in range(rng.randint(1, 3)))
        draw_kinds = tuple(rng.randrange(4) for _ in active_kinds)
        screen_x, screen_y = rng.randint(0, 400), rng.randint(0, 240)
        add("ItemsDraw", (), item_draw_setup(active_kinds, draw_kinds,
                                             screen_x=screen_x, screen_y=screen_y,
                                             effect_count=rng.randrange(9)),
            area=0x07,
            results={"RotTransPers": [1] * len(active_kinds)})

        pickup_mode = rng.choice((0, 2))
        pickup_position = tuple(rng.randint(-300, 300) for _ in range(3))
        pickup_velocity = tuple(rng.randint(-16, 16) for _ in range(3))
        pickup_setup = pickup_update_setup(
            (pickup_entry(mode=pickup_mode, position=pickup_position,
                          velocity=pickup_velocity, age=rng.randrange(100)),),
            player_position=tuple(rng.randint(-500, 500) for _ in range(3)))
        add("PickupsUpdate", (), pickup_setup, results={"GetGroundHeight": 64})

        sound_coords = tuple(rng.randint(-1200, 1200) for _ in range(3))
        sound_handle = rng.randrange(8)
        sound_state = rng.randint(1, 4)
        sound_mix = rng.randint(-64, 64)
        add("SoundEmitterUpdate", (), sound_update_setup(
            ((0x2000, 1, sound_handle, sound_coords, 0x2000),),
            track_state=sound_state, track_mix=sound_mix),
            results={"SpuWaitIdle": 0})

    for name, args, setup, area, results in cases:
        h.run(name, args, setup, area, results)
    return cases


def function_span(source, name):
    match = re.search(rf"(?m)^(?:int32_t|uint32_t|void)\s+(ae_{re.escape(name)})\s*\(", source)
    if not match:
        raise AssertionError(f"missing ae_{name}")
    opening = source.index("{", match.start())
    depth, i = 1, opening + 1
    while depth:
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
        i += 1
    return match.start(), opening, i - 1


def mutation_source(original):
    clones = []
    for name in FUNCS:
        start, opening, end = function_span(original, name)
        signature = original[start:opening]
        body = original[opening + 1:end]
        for variant, marker in enumerate((0xA5A50001, 0x5A5A0002)):
            clone_signature = signature.replace("ae_" + name, f"ae_mut_{name}_{variant}", 1)
            clones.append(clone_signature + f"{{ ae_wr32(r, 0x800EFFFCu, 0x{marker:08X}u);" + body + "}\n")
    return '#include "../../src/world/ae_misc.c"\n#include "../../src/world/ae_misc_batch.c"\n\n' + "\n".join(clones)


def mutate(h):
    # Exercise the copied probe callback contract independently of RAM-marker mutants.
    climb_cases = [case for case in scenarios_for_mutations
                   if case[0] == "CollisionRayTest" and "_climb_expected" in case[4]]
    original_callback = C.cast(C.cast(h.c.hooks.PlayerSetClimbTarget, P).value, CLIMB_FN)
    for label, drop4, drop5 in (("fifth word", True, False),
                               ("sixth word", False, True),
                               ("old four-word behavior", True, True)):
        callback = CLIMB_FN(lambda u, r, a0, a1, a2, a3, a4, a5:
                           original_callback(u, r, a0, a1, a2, a3,
                                             0 if drop4 else a4, 0 if drop5 else a5))
        h.c.hooks.PlayerSetClimbTarget = callback
        caught_contract = False
        try:
            for name, args, setup, area, results in climb_cases:
                try:
                    h.run(name, args, setup, area, results)
                except AssertionError as error:
                    if "PlayerSetClimbTarget" not in str(error):
                        raise
                    caught_contract = True
                    print(f"climb mutation {label}: caught: {error}")
                    break
        finally:
            h.c.hooks.PlayerSetClimbTarget = original_callback
        if not caught_contract:
            raise AssertionError(f"climb mutation {label} SURVIVED")
    source = ((ROOT / "src/world/ae_misc.c").read_text(encoding="utf-8") + "\n" +
              (ROOT / "src/world/ae_misc_batch.c").read_text(encoding="utf-8"))
    path = OUT / "misc_mutants.c"
    path.write_text(mutation_source(source), encoding="utf-8")
    c = CSide(build(path, "misc_mutants", include_mutations=True))
    caught = survivors = 0
    for name in FUNCS:
        address, _size, _ret, kinds, _takes_hooks = FUNCS[name]
        # A well-formed first input for every port is selected from its normal scenarios.
        candidates = [case for case in scenarios_for_mutations if case[0] == name]
        if not candidates:
            print(f"SKIP mutation seed for {name}: no default scenario")
            continue
        _name, args, setup, area, results = candidates[0]
        for variant in range(2):
            symbol = f"ae_mut_{name}_{variant}"
            c.signature(symbol, FUNCS[name][2], kinds, FUNCS[name][4])
            old_c = h.c
            h.c = c
            try:
                h.run(name, args, setup, area, results, symbol)
            except AssertionError as error:
                if "RAM differs" not in str(error):
                    raise
                caught += 1
            else:
                survivors += 1
            h.c = old_c
    tested = caught + survivors
    print(f"Mutations: {tested}; caught: {caught}; survivors: {survivors}")
    return int(survivors != 0 or tested < 30)


scenarios_for_mutations = []


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--cases", type=float, default=1.0)
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    args = parser.parse_args()
    if not (ROOT / "extract/disc/SCUS_944.23").is_file():
        raise SystemExit("missing extract/disc/SCUS_944.23")
    started = time.time()
    h = Harness(build())
    runs = scenarios(h, random_rounds=max(1, int(args.cases)), seed=args.seed)
    scenarios_for_mutations.extend(runs)
    if args.mutate:
        return mutate(h)
    print("cases per function:")
    for name, count in h.cases_run.items():
        print(f"  {name:32s} {count}")
    print(f"total cases {sum(h.cases_run.values())}; MIPS steps {h.steps}; elapsed {time.time() - started:.1f}s")
    h.finish(coverage=args.coverage, default_run=(args.cases == 1.0 and args.seed == DEFAULT_SEED))
    print("OK: all configured miscellaneous ports match the original instructions")
    return 0


if __name__ == "__main__":
    sys.exit(main())
