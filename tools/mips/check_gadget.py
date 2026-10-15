"""Differentially check the RAM-image gadget ports against SCUS_944.23.

Usage: .venv/Scripts/python.exe tools/mips/check_gadget.py [--coverage] [--cases PERCENT] [--seed N] [--mutate]
"""
import argparse
import ctypes as C
import random
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas
import exclusion_registry as reg
import mips_ram as mr

M32 = 0xFFFFFFFF
STACK_LO = 0x1FF000
TEST_VECTOR = 0x1F800200
HOOK_MARKER = 0x1F800240
HOOK_RESULT = HOOK_MARKER + 4
HOOK_RESULT_EXIT = HOOK_MARKER + 8
HOOK_RESULT_SWIM = HOOK_MARKER + 12
HOOK_RESULT_ACTIVATE = HOOK_MARKER + 16
HOOK_RESULT_SOLID = HOOK_MARKER + 20
HOOK_RESULT_GIMMICK_SFX = HOOK_MARKER + 24
HOOK_VECTOR_X = 0x800E7100
HOOK_VECTOR_Y = 0x800E7104
HOOK_VECTOR_Z = 0x800E7108
STACK_LO_ADDR, STACK_HI_ADDR = 0x801FF000, 0x80200000

# name -> (address, size, C function, has hook-table argument, argument kinds, returns value)
FUNCS = {
    "GadgetHasAltStance": (0x80065694, 72, "ae_GadgetHasAltStance", False, "", True),
    "GadgetFlashSet": (0x8006C6A4, 48, "ae_GadgetFlashSet", False, "u", False),
    "GadgetFlashUpdate": (0x8006C6D4, 600, "ae_GadgetFlashUpdate", True, "", False),
    "GadgetSelectFx": (0x8006CDE0, 188, "ae_GadgetSelectFx", True, "", False),
    "Gadget6SparkleFx": (0x8006CEF0, 180, "ae_Gadget6SparkleFx", True, "", False),
    "Gadget7TrailUpdate": (0x8006D044, 1140, "ae_Gadget7TrailUpdate", True, "", False),
    "Gadget4TrailReset": (0x8006DAC8, 48, "ae_Gadget4TrailReset", False, "", False),
    "Gadget4TrailUpdate": (0x8006DAF8, 1332, "ae_Gadget4TrailUpdate", True, "", False),
    "Gadget3FindAimTarget": (0x8007A32C, 352, "ae_Gadget3FindAimTarget", True, "", False),
    "Gadget4PlayerUpdate": (0x8007A514, 1288, "ae_Gadget4PlayerUpdate", True, "", False),
    "Gadget3AimUpdateAmmo": (0x8007AA98, 460, "ae_Gadget3AimUpdateAmmo", True, "", False),
    "GadgetSelectFxDraw": (0x8007C6CC, 780, "ae_GadgetSelectFxDraw", True, "", False),
    "GadgetDrawSlot0Attachment": (0x8009ABF4, 356, "ae_GadgetDrawSlot0Attachment", True, "uus", False),
    "GadgetSwingUpdate": (0x8009A800, 552, "ae_GadgetSwingUpdate", True, "", False),
    "GadgetDrawStaticModel": (0x80099C44, 216, "ae_GadgetDrawStaticModel", True, "", False),
    "GadgetDrawWithStateCheck": (0x8009A4E8, 140, "ae_GadgetDrawWithStateCheck", True, "uuuu", False),
    "Gadget6UpdateSound": (0x8009B1C0, 488, "ae_Gadget6UpdateSound", True, "", False),
    "Gadget4BuildMatrix": (0x80098DD0, 476, "ae_Gadget4BuildMatrix", True, "", False),
    "Gadget4UpdateDraw": (0x800986E0, 1776, "ae_Gadget4UpdateDraw", True, "", False),
    "Gadget3UpdateDraw": (0x8009AD60, 1120, "ae_Gadget3UpdateDraw", True, "", False),
    "Gadget3LockOnAdd": (0x8007A48C, 136, "ae_Gadget3LockOnAdd", True, "u", False),
    "GadgetSwingParamsInit": (0x8007AA1C, 72, "ae_GadgetSwingParamsInit", False, "", False),
    "GadgetUnlockMask": (0x8007B358, 28, "ae_GadgetUnlockMask", False, "u", False),
    "GadgetSetSelected": (0x8007CD0C, 72, "ae_GadgetSetSelected", True, "s", False),
    "GadgetSlotRemap": (0x80081144, 60, "ae_GadgetSlotRemap", False, "ss", False),
}

HOOKS = [
    ("SpawnParticle", 0x800698B8, 2),
    ("PlaySfx", 0x800877F8, 1),
    ("CameraGetSlotTarget", 0x8004B54C, 1),
    ("TargetListFindBest", 0x8007CDFC, 3),
    ("SdkVectorHelper_80031330", 0x80031330, 4),
    ("AimReticleDrawCircle", 0x80085E08, 3),
    ("SdkVectorHelper_8002FB00", 0x8002FB00, 1),
    ("SdkVectorHelper_8002FB30", 0x8002FB30, 1),
    ("ModelTransformRigid", 0x800161C0, 3),
    ("PlayerFxSetMatrixPos", 0x8006CFBC, 2),
    ("ModelDraw", 0x80017CD4, 3),
    ("RadarDrawModel", 0x8009A574, 4),
    ("EffectSpawnFromDef", 0x80070284, 5),
    ("TrailSpawn", 0x8006ED78, 4),
    ("SoundLoopStart", 0x800878E0, 2),
    ("SoundLoopStop", 0x8008806C, 2),
    ("MatrixRotYMul", 0x80014750, 2),
    ("SdkVectorHelper_800305B0", 0x800305B0, 2),
    ("ProjectileGroupSpawnSpread", 0x80085630, 3),
    ("DrawRibbonBetween", 0x8002020C, 4),
    ("SdkVectorHelper_80031230", 0x80031230, 3),
    ("TrailRingPush", 0x8001F8B0, 0),
    ("TrailRingDraw", 0x8001F954, 0),
    ("PlayerCanExitStage", 0x8005DC78, 0),
    ("IsPlayerSwimming", 0x80068B7C, 0),
    ("RibbonDraw", 0x8006F3D0, 4),
    ("PlayerSetState", 0x80064208, 1),
    ("PadRumble", 0x80014168, 3),
    ("DrawScaledModelAt", 0x8007CAE0, 3),
    ("PlayerCanActivateGadget", 0x8007CA48, 1),
    ("SdkVectorHelper_80030A30", 0x80030A30, 2),
    ("SdkVectorHelper_80031830", 0x80031830, 3),
    ("SetColorMatrixMode", 0x80015D58, 4),
    ("SoundEmitterVolume", 0x80087BA4, 4),
    ("PlayerSelectGadget", 0x8007AC64, 2),
    ("AimTableClear3", 0x8007A304, 0),
    ("GetSolidHeight", 0x8004D8C0, 3),
    ("PlaySfxAt", 0x80087A20, 2),
    ("GimmickActivate", 0x8008FC30, 2),
    ("MeshInitFromAreaTable", 0x80015CC8, 2),
    ("AreaEffect", 0x8013D358, 3),
    ("CameraShake", 0x80048A00, 1),
    ("EventCamStart", 0x8004D39C, 1),
    ("PrintfError", 0x800A3720, 2),
    ("GimmickTypeToSfx", 0x8005BB98, 1),
    ("PickupCollect", 0x80078A18, 1),
    ("CameraNudge", 0x80047690, 3),
    ("GpuDrawGadget7Trail", 0xFFFFFF00, 0),
    ("GpuDrawGadget4Trail", 0xFFFFFF04, 1),
    ("GpuProjectGadget3Aim", 0xFFFFFF08, 0),
    ("GpuDrawGadget3AimLine", 0xFFFFFF0C, 0),
]
HOOK_TAGS = {name: (index + 1) * 0x1021 for index, (name, _, _) in enumerate(HOOKS)}
HOOK_POINTER_ARGS = {
    "SpawnParticle": {1},
    "CameraGetSlotTarget": {0},
    "TargetListFindBest": {1},
    "SdkVectorHelper_80031330": {1, 2, 3},
    "EffectSpawnFromDef": {0, 4},
    "TrailSpawn": {0},
    "ProjectileGroupSpawnSpread": {1},
    "ModelTransformRigid": {1},
    "SdkVectorHelper_80031230": {2},
    "RibbonDraw": {0, 2},
    "PlaySfxAt": {1},
    "GpuDrawGadget4Trail": {0},
}
HookFn = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32,
                     C.c_uint32, C.c_uint32, C.c_uint32)


def s32(value):
    return mr.signed(value, 32)


def s16(value):
    return mr.signed(value, 16)


def build(source=None, out_dir=None, trapv=False):
    # Link the verified dependency closure; CollisionUpdate now calls bodymove.
    import check_game
    overrides = {"src/player/ae_gadget.c": Path(source)} if source else None
    return check_game.build_dll(overrides, "gadget", trapv)


def gadget_hook_fields():
    text = (ROOT / "src/player/ae_gadget.h").read_text(encoding="utf-8")
    block = text[text.index("typedef struct AeGadgetHooks"):]
    block = block[:block.index("} AeGadgetHooks;")]
    return [match.group(1) for match in re.finditer(r"AeGadgetHookFn\s+(\w+);", block)]


class Side:
    def __init__(self, dll):
        self.lib = lib = C.CDLL(str(dll))
        P, U, I = C.c_void_p, C.c_uint32, C.c_int32
        lib.ae_ram_create.restype = P
        lib.ae_ptr.argtypes = [P, U, U]
        lib.ae_ptr.restype = P
        lib.ae_ram_clear_error.argtypes = [P]
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_failed.restype = C.c_int
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        self.ram = lib.ae_ram_create()
        if not self.ram:
            raise SystemExit("ae_ram_create failed")
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scr = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))

        lib.ae_GadgetHasAltStance.argtypes = [P]
        lib.ae_GadgetHasAltStance.restype = I
        lib.ae_GadgetFlashSet.argtypes = [P, U]
        lib.ae_GadgetFlashSet.restype = None
        lib.ae_GadgetFlashUpdate.argtypes = [P, P]
        lib.ae_GadgetFlashUpdate.restype = None
        lib.ae_GadgetSelectFx.argtypes = [P, P]
        lib.ae_GadgetSelectFx.restype = None
        lib.ae_Gadget6SparkleFx.argtypes = [P, P]
        lib.ae_Gadget6SparkleFx.restype = None
        lib.ae_Gadget7TrailUpdate.argtypes = [P, P]
        lib.ae_Gadget7TrailUpdate.restype = None
        lib.ae_Gadget4TrailReset.argtypes = [P]
        lib.ae_Gadget4TrailReset.restype = None
        lib.ae_Gadget4TrailUpdate.argtypes = [P, P]
        lib.ae_Gadget4TrailUpdate.restype = None
        lib.ae_Gadget3FindAimTarget.argtypes = [P, P]
        lib.ae_Gadget3FindAimTarget.restype = None
        lib.ae_Gadget4PlayerUpdate.argtypes = [P, P]
        lib.ae_Gadget4PlayerUpdate.restype = None
        lib.ae_Gadget3AimUpdateAmmo.argtypes = [P, P]
        lib.ae_Gadget3AimUpdateAmmo.restype = None
        lib.ae_GadgetSelectFxDraw.argtypes = [P, P]
        lib.ae_GadgetSelectFxDraw.restype = None
        lib.ae_GadgetDrawSlot0Attachment.argtypes = [P, P, U, U, I]
        lib.ae_GadgetDrawSlot0Attachment.restype = None
        lib.ae_GadgetSwingUpdate.argtypes = [P, P]
        lib.ae_GadgetSwingUpdate.restype = None
        lib.ae_GadgetDrawStaticModel.argtypes = [P, P]
        lib.ae_GadgetDrawStaticModel.restype = None
        lib.ae_GadgetDrawWithStateCheck.argtypes = [P, P, U, U, U, U]
        lib.ae_GadgetDrawWithStateCheck.restype = None
        lib.ae_Gadget6UpdateSound.argtypes = [P, P]
        lib.ae_Gadget6UpdateSound.restype = None
        lib.ae_Gadget4BuildMatrix.argtypes = [P, P]
        lib.ae_Gadget4BuildMatrix.restype = None
        lib.ae_Gadget4UpdateDraw.argtypes = [P, P]
        lib.ae_Gadget4UpdateDraw.restype = None
        lib.ae_Gadget3UpdateDraw.argtypes = [P, P]
        lib.ae_Gadget3UpdateDraw.restype = None
        lib.ae_Gadget3LockOnAdd.argtypes = [P, P, U]
        lib.ae_Gadget3LockOnAdd.restype = None
        lib.ae_GadgetSwingParamsInit.argtypes = [P]
        lib.ae_GadgetSwingParamsInit.restype = None
        lib.ae_GadgetUnlockMask.argtypes = [P, U]
        lib.ae_GadgetUnlockMask.restype = None
        lib.ae_GadgetSetSelected.argtypes = [P, P, I]
        lib.ae_GadgetSetSelected.restype = None
        lib.ae_GadgetSlotRemap.argtypes = [P, I, I]
        lib.ae_GadgetSlotRemap.restype = None

        fields = [("user", P), ("stale_s1", U)] + [(name, HookFn) for name in gadget_hook_fields()]
        self.hooks_type = type("AeGadgetHooks", (C.Structure,), {"_fields_": fields})
        self.hooks = self.hooks_type()
        self.callbacks = {}
        self.model = None
        for name, _, _ in HOOKS:
            cb = HookFn(lambda _user, _ram, a0, a1, a2, a3, a4, hook_name=name:
                        self.model.call(hook_name, a0, a1, a2, a3, a4))
            self.callbacks[name] = cb
            setattr(self.hooks, name, cb)

    def sync_from(self, cpu):
        C.memmove(self.main, bytes(cpu.ram), mr.RAM_SIZE)
        C.memmove(self.scr, bytes(cpu.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)

    def rd16(self, address):
        off = address & 0x1FFFFFFF
        blob = self.scr if mr.SCRATCH_BASE <= address < mr.SCRATCH_BASE + mr.SCRATCH_SIZE else self.main
        if blob is self.scr:
            off = address - mr.SCRATCH_BASE
        return blob[off] | (blob[off + 1] << 8)

    def rd32(self, address):
        return self.lib.ae_rd32(self.ram, address)

    def wr32(self, address, value):
        self.lib.ae_wr32(self.ram, address, value & M32)

    def call(self, name, args):
        _, _, cname, takes_hooks, kinds, _ = FUNCS[name]
        converted = [s32(a) if kind == "s" else a & M32 for a, kind in zip(args, kinds)]
        fn = getattr(self.lib, cname)
        if takes_hooks:
            return fn(self.ram, C.byref(self.hooks), *converted)
        return fn(self.ram, *converted)


class MipsMemory:
    def __init__(self, cpu): self.cpu = cpu
    def rd16(self, address): return self.cpu.read16(address)
    def rd32(self, address): return self.cpu.read32(address)
    def wr16(self, address, value): self.cpu.write16(address, value & 0xFFFF)
    def wr32(self, address, value): self.cpu.write32(address, value & M32)


class SideMemory:
    def __init__(self, side): self.side = side
    def rd16(self, address): return self.side.rd16(address)
    def rd32(self, address): return self.side.rd32(address)
    def wr16(self, address, value): self.side.lib.ae_wr16(self.side.ram, address, value & 0xFFFF)
    def wr32(self, address, value): self.side.wr32(address, value)


class HookModel:
    def __init__(self, mem):
        self.mem = mem
        self.log = []

    @staticmethod
    def normalize_pointer(value):
        return "STACK" if STACK_LO_ADDR <= (value & M32) < STACK_HI_ADDR else (value & M32)

    def call(self, name, a0, a1, a2, a3, a4=0):
        args = [a0 & M32, a1 & M32, a2 & M32, a3 & M32, a4 & M32]
        used = args[:next(count for hook, _, count in HOOKS if hook == name)]
        payload = ()
        if name == "SpawnParticle":
            payload = tuple(s16(self.mem.rd16(a1 + offset)) for offset in (0, 2, 4))
        elif name == "ProjectileGroupSpawnSpread":
            payload = tuple(s16(self.mem.rd16(a1 + offset)) for offset in (0, 2, 4))
        elif name in ("EffectSpawnFromDef", "TrailSpawn"):
            payload = tuple(s16(self.mem.rd16(a0 + offset)) for offset in (0, 2, 4))
            if name == "EffectSpawnFromDef" and a4 != 0:
                payload += tuple(s16(self.mem.rd16(a4 + offset)) for offset in (0, 2, 4))
        for index in HOOK_POINTER_ARGS.get(name, ()):
            used[index] = self.normalize_pointer(used[index])
        event = (name, *used, payload)
        self.log.append(event)
        marker = self.mem.rd32(HOOK_MARKER)
        marker_args = args[:1] if name == "SpawnParticle" else used
        mixed = sum((((0xFFFFFFFF if value == "STACK" else value) * (index + 1)) & M32)
                    for index, value in enumerate(marker_args))
        mixed += sum((value & 0xFFFF) * (index + 2) for index, value in enumerate(payload))
        self.mem.wr32(HOOK_MARKER, marker + mixed + HOOK_TAGS[name])
        if name == "CameraGetSlotTarget":
            for offset, value in zip((0, 2, 4), (320, -90, 640)):
                self.mem.wr16(a0 + offset, value)
        elif name in ("TargetListFindBest", "SoundLoopStart"):
            return self.mem.rd32(HOOK_RESULT)
        elif name == "PlayerCanExitStage":
            return self.mem.rd32(HOOK_RESULT_EXIT)
        elif name == "IsPlayerSwimming":
            return self.mem.rd32(HOOK_RESULT_SWIM)
        elif name == "PlayerCanActivateGadget":
            return self.mem.rd32(HOOK_RESULT_ACTIVATE)
        elif name == "GetSolidHeight":
            return self.mem.rd32(HOOK_RESULT_SOLID)
        elif name == "GimmickTypeToSfx":
            return self.mem.rd32(HOOK_RESULT_GIMMICK_SFX)
        elif name == "SdkVectorHelper_80031330":
            self.mem.wr32(a1, self.mem.rd32(HOOK_VECTOR_X))
            self.mem.wr32(a2, self.mem.rd32(HOOK_VECTOR_Y))
            self.mem.wr32(a3, self.mem.rd32(HOOK_VECTOR_Z))
        return 0


class GadgetHarness:
    def __init__(self, side, area_member):
        self.funcs = FUNCS
        self.m = mr.MipsRam.from_extract(area=area_member)
        self.m.attach_gte_oracle()
        self.m.coverage = set()
        self.base = self.m.snapshot()
        self.side = side
        self.failures = 0
        self.counts = {name: 0 for name in FUNCS}
        self.quiet = False
        reg.declare("stack_window", "801FF000..80200000")

    def run(self, name, args, setup, label, initial_regs=None):
        self.m.restore(self.base)
        self.m.gte_reset()
        setup(self.m)
        self.side.sync_from(self.m)
        self.side.hooks.stale_s1 = (initial_regs or {}).get(17, 0)
        mm = HookModel(MipsMemory(self.m))
        cm = HookModel(SideMemory(self.side))
        self.side.model = cm
        self.m.hook_log.clear()
        mips_hooks = {
            address: (lambda cpu, regs, hook_name=hook_name, arg_count=arg_count:
                      mm.call(hook_name, *regs[4:8],
                              cpu.read32((regs[29] + 16) & M32) if arg_count > 4 else 0))
            for hook_name, address, arg_count in HOOKS
        }
        def renderer_patch(hook_name, return_pc, values=lambda regs: (0, 0, 0, 0), write_cache=False):
            def apply(cpu, regs):
                a0, a1, a2, a3 = values(regs)
                mm.call(hook_name, a0, a1, a2, a3, 0)
                if write_cache:
                    cpu.write16(0x800F5144, 1)
                return return_pc
            return apply
        mips_patches = {
            0x8006D27C: lambda cpu, regs: self._gadget7_renderer_patch(cpu, regs, mm),
            0x8006DE00: renderer_patch("GpuDrawGadget4Trail", 0x8006DFE4,
                                       lambda regs: (regs[20], 0, 0, 0)),
            0x8009AE9C: renderer_patch("GpuProjectGadget3Aim", 0x8009B058, write_cache=True),
            0x8009B094: renderer_patch("GpuDrawGadget3AimLine", 0x8009B1A8),
        }
        merror = None
        try:
            registers = self.m.call(FUNCS[name][0], args, hooks=mips_hooks, patches=mips_patches,
                                    initial_regs=initial_regs)
            mret = registers[2]
        except mr.MipsError as error:
            mret, merror = None, str(error)
        cret = self.side.call(name, args)
        self.counts[name] += 1

        problems = []
        if merror:
            problems.append("MIPS harness error: " + merror)
        if self.side.lib.ae_ram_failed(self.side.ram):
            problems.append("C error flag: " + self.side.lib.ae_ram_error(self.side.ram).decode())
        if FUNCS[name][5] and mret != (cret & M32):
            problems.append(f"return MIPS {mret & M32:08X} C {cret & M32:08X}")
        if mm.log != cm.log:
            problems.append(f"hook calls differ\n      MIPS {mm.log}\n      C    {cm.log}")
        mram = bytes(self.m.ram[:STACK_LO])
        cram = bytes(self.side.main[:STACK_LO])
        if mram != cram:
            diffs = [i for i, (a, b) in enumerate(zip(mram, cram)) if a != b]
            problems.append("RAM differs at " + ", ".join(
                f"{0x80000000 + i:08X}:{mram[i]:02X}/{cram[i]:02X}" for i in diffs[:8]))
        if bytes(self.m.scratch) != bytes(self.side.scr):
            diffs = [i for i, (a, b) in enumerate(zip(self.m.scratch, self.side.scr)) if a != b]
            problems.append("scratchpad differs at " + ", ".join(
                f"{mr.SCRATCH_BASE + i:08X}:{self.m.scratch[i]:02X}/{self.side.scr[i]:02X}"
                for i in diffs[:12]))
        if problems:
            self.failures += 1
            if not self.quiet:
                print(f"FAIL {name} [{label}]")
                for problem in problems: print("   " + problem)

    @staticmethod
    def _gadget7_renderer_patch(cpu, regs, model):
        model.call("SdkVectorHelper_8002FB00", 0x800E78C0, 0, 0, 0)
        model.call("SdkVectorHelper_8002FB30", 0x800E78C0, 0, 0, 0)
        model.call("GpuDrawGadget7Trail", 0, 0, 0, 0)
        return 0x8006D48C

    def finish(self, default_run, show_coverage):
        items = [(name, *spec[:2], True) for name, spec in self.funcs.items()]
        summary = reg.coverage_summary(items, self.m.coverage)
        if show_coverage: reg.print_coverage(summary)
        if default_run:
            reg.write_results(self.counts, summary, not self.failures, function_names=list(self.funcs))
        return summary


def setup_empty(_m):
    pass


def set16(m, addr, value): m.write16(addr, value & 0xFFFF)
def set32(m, addr, value): m.write32(addr, value & M32)


def base_hook_state(m, seed):
    rng = random.Random(seed)
    set16(m, 0x800EC1E0 + 0x60, rng.randrange(-0x1000, 0x1000))
    set16(m, 0x800EC1E0 + 0x64, rng.randrange(-0x1000, 0x1000))
    for offset in (0x30, 0x34, 0x38): set32(m, 0x800EC1E0 + offset, rng.getrandbits(32))
    for offset in (0, 2, 4): set16(m, 0x800B4D88 + offset, rng.getrandbits(16))
    set32(m, HOOK_MARKER, rng.getrandbits(32))
    return rng


def set_gadget_hook_results(m, exit_ok=1, swimming=0, can_activate=1,
                            solid_height=17000, gimmick_sfx=0, result=0):
    set32(m, HOOK_RESULT, result)
    set32(m, HOOK_RESULT_EXIT, exit_ok)
    set32(m, HOOK_RESULT_SWIM, swimming)
    set32(m, HOOK_RESULT_ACTIVATE, can_activate)
    set32(m, HOOK_RESULT_SOLID, solid_height)
    set32(m, HOOK_RESULT_GIMMICK_SFX, gimmick_sfx)


def clear_body_pool(m):
    for index in range(256):
        slot = 0x800BBDA0 + index * 0x14
        set16(m, slot, 0xFFFF)
        set16(m, slot + 2, 0xFFFF)
        for offset in (4, 8, 12, 16): set32(m, slot + offset, 0)
    for index in range(128): set16(m, 0x800BEDA0 + index * 2, 0xFFFF)
    set32(m, 0x800B20D8, 0)
    set32(m, 0x800B20DC, 0)


def set_real_area_grid(m):
    member = dump_areas.area_members()[1]
    blob = (ROOT / f"extract/members/{member:03X}.bin").read_bytes()
    area = struct.unpack_from("<I", blob, 0x4D0)[0]
    grid = area + 4 * struct.unpack_from("<I", blob, area + 4)[0]
    width, depth, collider_count = struct.unpack_from("<HHI", blob, grid)
    cells = grid + 8
    heights = cells + width * depth * 12 + collider_count * 4
    set32(m, 0x800F44E4, mr.AREA_BASE + area)
    set16(m, 0x800EBFA0, width)
    set16(m, 0x800EBFA2, depth)
    set32(m, 0x800EBFA8, mr.AREA_BASE + cells)
    set32(m, 0x800EBFB4, mr.AREA_BASE + heights)


def set_water_collider(m, address=0x801D6000):
    set16(m, 0x800F45D4, 1)
    set32(m, 0x800F455C, address)
    for offset, value in ((0x00, 0), (0x02, 0), (0x04, 0),
                          (0x06, 32767), (0x08, 32767), (0x0A, 32767),
                          (0x12, 0x0109)):
        set16(m, address + offset, value)


def run_cases(h, scale, seed):
    count = lambda n: max(2, int(round(n * scale)))
    rng = random.Random(seed)

    # The byte sweep covers both signs and every alternate-stance id after lb sign extension.
    for gadget in range(count(256)):
        value = gadget % 256
        h.run("GadgetHasAltStance", (), lambda m, v=value: m.write8(0x800EC1E0 + 0xF2, v), f"current gadget {value:02X}")

    for index in range(count(8)):
        x, y, z = (rng.getrandbits(32) for _ in range(3))
        def setup(m, values=(x, y, z)):
            for offset, value in zip((0, 4, 8), values): set32(m, TEST_VECTOR + offset, value)
        h.run("GadgetFlashSet", (TEST_VECTOR,), setup, f"flash vector {index}")

    for index, timer in enumerate((0, 1, 2, 5, 14, 18, 30, 255, 6, 30)):
        def setup(m, i=index, frames=timer):
            rng_case = random.Random(seed + 300 + i)
            base_hook_state(m, seed + i + 20)
            m.write8(0x800BAD80, frames)
            set16(m, 0x800BAD84, rng_case.randrange(-3000, 3001))
            set16(m, 0x800BAD86, rng_case.randrange(-3000, 3001))
            set16(m, 0x800BAD88, rng_case.randrange(-3000, 3001))
            set16(m, 0x800AE8F0, rng_case.getrandbits(16))
        h.run("GadgetFlashUpdate", (), setup, f"flash timer {timer} ({index})")

    for index, (guard_a, guard_b) in enumerate(((0, 0), (1, 0), (0, 1), (2, 2), (0, 0))):
        def setup(m, i=index, ga=guard_a, gb=guard_b):
            base_hook_state(m, seed + i)
            m.write8(0x800E6902, ga); m.write8(0x800E6916, gb)
        h.run("GadgetSelectFx", (), setup, f"fx guard pair {guard_a}/{guard_b}")

    for index in range(count(12)):
        def setup(m, i=index):
            base_hook_state(m, seed + i * 11)
            set16(m, 0x800AE8F0, rng.getrandbits(16))
            set32(m, 0x800EC350, rng.getrandbits(32))
        h.run("Gadget6SparkleFx", (), setup, f"sparkle {index}")

    trail7_cases = (
        (0, 0, 0, 1, 0), (7, 10, 0, 1, 0), (7, 0, 1, 1, 0),
        (7, 0, 0, 0, 0), (7, 0, 0, 1, 1), (7, 0, 0, 1, 0),
        (7, 0, 0, 1, 0), (7, 0, 0, 1, 0),
    )
    for index, (gadget, stage_gate, guard, exit_ok, swimming) in enumerate(trail7_cases):
        def setup(m, i=index, values=(gadget, stage_gate, guard, exit_ok, swimming)):
            current, gate, guarded, can_exit, swim = values
            rng_case = base_hook_state(m, seed + 2100 + i)
            m.write8(0x800EC1E0 + 0xF2, current)
            set16(m, 0x800F44A0, gate)
            m.write8(0x800E6902, guarded)
            m.write8(0x800BAD99, i % 7)
            for axis in range(3):
                set16(m, 0x800BAD90 + axis * 2, rng_case.randrange(-2048, 2048))
                set32(m, 0x800F5A20 + 0x24 + axis * 4,
                      rng_case.randrange(-20000, 20001))
            set16(m, 0x800AE8F0, rng_case.getrandbits(16))
            set32(m, 0x800EC350, (i & 1) | (rng_case.getrandbits(30) << 1))
            set_gadget_hook_results(m, exit_ok=can_exit, swimming=swim)
        h.run("Gadget7TrailUpdate", (), setup, f"RC trail gate case {index}")

    for index in range(count(12)):
        def setup(m, i=index):
            rng_case = base_hook_state(m, seed + 2200 + i)
            m.write8(0x800EC1E0 + 0xF2, 4)
            set_gadget_hook_results(m, exit_ok=1 if i != 11 else 0)
            set16(m, 0x800F5124, i & 1)
            set32(m, 0x800F5128, rng_case.randrange(-200, 2000))
            for axis, offset in enumerate((0x30, 0x34, 0x38)):
                set32(m, 0x800EC1E0 + offset, rng_case.randrange(-2000, 2001))
            for offset in range(0x24, 0x2C, 2):
                set16(m, 0x800EC1E0 + offset, rng_case.randrange(-2000, 2001))
            for record_index in range(40):
                record = 0x800F4ED0 + record_index * 8
                for offset in (0, 2, 4): set16(m, record + offset, rng_case.randrange(-1000, 1001))
                low_state = (record_index + i) % 8 if (record_index + i) % 3 else 0
                set16(m, record + 6, (rng_case.getrandbits(11) << 5) | low_state)
        h.run("Gadget4TrailUpdate", (), setup, f"Super Hoop trail mode/state mix {index}")

    for index in range(3):
        def setup(m, i=index):
            for slot in range(40):
                set16(m, 0x800F4ED0 + slot * 8 + 6, rng.getrandbits(16))
        h.run("Gadget4TrailReset", (), setup, f"40 trail records {index}")

    for index in range(count(12)):
        target = 0x800E0000 + index * 4
        def setup(m, i=index, ptr=target):
            set32(m, 0x800F5090, 0x800E1000 + i)
            set32(m, 0x800F5098, 0x800E2000 + i)
            set32(m, 0x800F50A0, 0x800E3000 + i)
            if i % 3 == 0: set32(m, 0x800F50A0, ptr)       # duplicate in last entry
            elif i % 3 == 1: set32(m, 0x800F5098, 0)      # first free is middle entry
            else: set32(m, 0x800F5090, 0)                 # first free is first entry
            set32(m, HOOK_MARKER, i)
        h.run("Gadget3LockOnAdd", (target,), setup, f"lock list shape {index % 3}")

    for index, (aim_mode, candidate) in enumerate(((0, 0), (1, 0), (1, 0x800E6100),
                                                    (2, 0x800E6200), (0, 0x800E6300), (1, 0x800E6400))):
        def setup(m, i=index, aim=aim_mode, found=candidate):
            rng_case = random.Random(seed + 500 + i)
            base_hook_state(m, seed + i + 200)
            m.write8(0x800E6916, aim)
            for axis in range(3):
                set32(m, 0x800EC1E0 + 0x24 + axis * 4, rng_case.randrange(-200, 201))
            set32(m, HOOK_RESULT, found)
            set32(m, HOOK_VECTOR_X, rng_case.getrandbits(32))
            set32(m, HOOK_VECTOR_Y, rng_case.getrandbits(32))
            set32(m, HOOK_VECTOR_Z, rng_case.getrandbits(32))
            for slot in range(3):
                record = 0x800F5090 + slot * 8
                pointer = 0 if slot >= (i % 3) else 0x800E6500 + slot * 4
                set32(m, record, pointer)
                set16(m, record + 4, (i * 3 + slot) % 9 - 2)
        h.run("Gadget3FindAimTarget", (), setup, f"aim mode {aim_mode}, result {candidate:08X}")

    for index, (aim_mode, ammo_kind, ammo, level, game_mode, target) in enumerate((
            (0, 0, 0, 1, 0, 0), (0, 1, 3, 1, 0, 0), (0, 2, 4, 1, 0, 0x800E6700),
            (1, 1, 1, 1, 0, 0), (0, 1, 5, 0x20, 0, 0x800E6740),
            (0, 2, 1, 1, 0x13, 0x800E6780), (0, 2, 0, 1, 0, 0), (1, 3, 0, 1, 0, 0))):
        def setup(m, i=index, aiming=aim_mode, kind=ammo_kind, rounds=ammo,
                  current_level=level, current_mode=game_mode, found=target):
            rng_case = random.Random(seed + 1400 + i)
            base_hook_state(m, seed + 1300 + i)
            m.write8(0x800E6916, aiming)
            m.write8(0x800F51C0, kind)
            m.write8(0x800F51C1, rounds)
            m.write8(0x800F51C2, rounds)
            set16(m, 0x800F4474, current_level)
            set16(m, 0x800F4470, current_mode)
            set32(m, HOOK_RESULT, found)
            for offset, value in zip((0, 2, 4), (100, -200, 300)):
                set16(m, 0x800F45A8 + offset, value)
                set16(m, 0x800F45B0 + offset, value + rng_case.randrange(-500, 501))
            if found:
                for offset, value in zip((0, 2, 4), (900, -700, 1100)):
                    set16(m, found + offset, value)
        h.run("Gadget3AimUpdateAmmo", (), setup,
              f"aim {aim_mode}, ammo kind {ammo_kind}, count {ammo}, level {level:02X}, mode {game_mode:02X}")

    player4_cases = (
        (0, 128, 0, 120, 0), (0, 128, 13, 240, 1), (0, 129, 19, 200, 1),
        (0, 129, 34, 120, 0), (0, 130, 7, 300, 0), (0, 131, 2, 150, 0),
        (1, 128, 0, 130, 1), (1, 129, 33, 300, 1), (2, 130, 0, 60, 0),
        (0, 128, 0, 121, 1), (0, 129, 18, 120, 0), (0, 130, 20, 121, 1),
        (0, 14, 0, 120, 0), (0, 49, 0, 121, 1),
        (1, 128, 13, 240, 1), (0, 128, 1, 120, 0),
        (1, 129, 19, 240, 0), (0, 130, 20, 121, 1),
    )
    for index, (mode, state, frame, level_frame, level_enable) in enumerate(player4_cases):
        def setup(m, i=index, values=(mode, state, frame, level_frame, level_enable)):
            current_mode, player_state, anim_frame, lframe, enable = values
            base_hook_state(m, seed + 2300 + i)
            clear_body_pool(m)
            set_gadget_hook_results(m)
            m.write8(0x800EC1E0 + 0xF2, 4)
            m.write8(0x800F51C3, i & 1)
            set16(m, 0x800EC1E0 + 0x5E, player_state)
            set16(m, 0x800EC1E0 + 0x5C, anim_frame)
            set16(m, 0x800F5128, current_mode)
            set16(m, 0x800F5124, 1200 + i)
            set16(m, 0x800E5320 + 0x182, lframe)
            set16(m, 0x800E5320 + 0x184, 83 + i)
            set16(m, 0x800E5320 + 0x17A, enable)
            set16(m, 0x800E5320 + 0x1AC, (i * 211) & 0xFFFF)
            set16(m, 0x800F4474, 14 + (i % 6))
            if 14 <= player_state < 66:
                set32(m, 0x800A5EF8 + 4 * (player_state - 14),
                      0x8007A804 if i & 1 else 0x8007A814)
            set32(m, 0x800F4E34, 0xABC00000 + i)
            set32(m, 0x800F5120 + 0x14, 0)
            set32(m, 0x800F5120 + 0x18, 0)
            for offset, value in ((0x0C, 10 + i), (0x0E, -30 + i), (0x10, 50 - i)):
                set16(m, 0x800F5120 + offset, value)
            set16(m, 0x800EC1E0 + 0xE8, 1)
            if i == 16:
                set32(m, 0x800F5120 + 0x14, 0x801D8000)
            elif i == 17:
                set32(m, 0x800F5120 + 0x18, 0x801D8000)
            if i in (16, 17):
                set32(m, 0x801D802C, 10 << 28)
                set16(m, 0x801D8018, 32)
                set16(m, 0x801D801C, -64)
            if i == 17:
                set16(m, 0x800E5320 + 0x1AC, 300)
                set32(m, 0x800EC250, 0x80)
            set32(m, HOOK_RESULT, 65 + i)
        h.run("Gadget4PlayerUpdate", (), setup, f"Hoop player state/mode case {index}")

    fx_cases = (0, 1, 9, 10, 11, 14, 15, 16, 17, 20, 21, 0xFFFFFFFF)
    for index, animation in enumerate(fx_cases):
        def setup(m, i=index, timer=animation):
            rng_case = base_hook_state(m, seed + 2400 + i)
            clear_body_pool(m)
            set_gadget_hook_results(m, solid_height=17000)
            set_real_area_grid(m)
            set16(m, 0x800F44A2, 0)
            set16(m, 0x800EC1E0 + 0xE8, 2)
            for offset, value in ((0, 0), (2, -20000), (4, 0),
                                  (6, rng_case.randrange(-4096, 4096)),
                                  (8, 0), (10, 0), (12, 4096)):
                set16(m, 0x800F51D0 + offset, value)
            set32(m, 0x800F51D0 + 16, timer)
            set32(m, 0x800EC358, 0x13570000 + i)
            if i & 1: set_water_collider(m)
            else: set16(m, 0x800F45D4, 0)
        h.run("GadgetSelectFxDraw", (), setup, f"selection FX animation {animation:08X}")

    for index, (mode, stage_flags, selected, level, strength, activate) in enumerate((
            (0, 0, 1, 14, 450, 0), (0, 0, 1, 15, 350, 1), (0, 0, 1, 50, 700, 1),
            (1, 0, 1, 14, 20, 1), (1, 0, 1, 50, 30, 1), (1, 0, 0, 51, 100, 1),
            (2, 0, 1, 15, 700, 0), (0, 1, 1, 14, 0, 0), (1, 0, 1, 14, 2, 1),
            (0, 0, 1, 51, 321, 1), (1, 0, 1, 14, 500, 1), (1, 0, 1, 14, 0, 1),
            (0, 0, 1, 14, 450, 1), (0, 0, 1, 14, 450, 1),
            (0, 0, 1, 14, 450, 1), (0, 0, 1, 14, 450, 1),
            (1, 0, 1, 14, 450, 1), (1, 0, 1, 14, 450, 1),
            (1, 0, 1, 14, 450, 1), (1, 0, 1, 14, 450, 1),
            (1, 0, 1, 14, 0, 1), (1, 0, 1, 14, 450, 1),
            (0, 0, 1, 14, 0, 1),
    )):
        def setup(m, i=index, values=(mode, stage_flags, selected, level, strength, activate)):
            hoop_mode, stage, current_selected, current_level, power, can_activate = values
            rng_case = base_hook_state(m, seed + 2500 + i)
            clear_body_pool(m)
            set_gadget_hook_results(m, can_activate=can_activate, result=71 + i)
            m.write8(0x800F51C3, current_selected)
            set32(m, 0x800F4450, stage)
            set16(m, 0x800F44A2, 11 if i == 12 else 0)
            set16(m, 0x800F4474, current_level)
            set16(m, 0x800F5124, hoop_mode)
            set32(m, 0x800F5128, power)
            set16(m, 0x800F512A, 0 if i in (20, 22) else i * 23)
            set16(m, 0x800F5140, -1)
            set16(m, 0x800F5142, -1)
            set16(m, 0x800F5140, 7 if i in (13, 21) else -1)
            set16(m, 0x800F5142, 8 if i == 13 else -1)
            set16(m, 0x800F5120 + 0x20, 7 if i in (13, 21) else -1)
            set16(m, 0x800F5120 + 0x22, 8 if i == 13 else -1)
            set32(m, 0x800F512C, 0)
            set32(m, 0x800F5130, 0)
            set16(m, 0x800F512C, 0xFFFF)
            set16(m, 0x800F512E, 0xFFFF)
            m.write8(0x800F51C3, current_selected)
            set32(m, 0x800A5EF8 + ((current_level - 14) & 0xFFFF) * 4,
                  0x8007A804 if current_level in (14, 15, 49, 50, 51) and i % 2 else 0x8007A814)
            set16(m, 0x800E5320 + 0x182, 132 + i)
            set16(m, 0x800E5320 + 0x19C, 900 + i * 3)
            set16(m, 0x800E5320 + 0x1A0, -700 + i * 4)
            set16(m, 0x800E5320 + 0x1B4, rng_case.randrange(-300, 301))
            set16(m, 0x800E5320 + 0x1BC, rng_case.randrange(-300, 301))
            set32(m, 0x800EC0A0, 0x801D7000)
            set16(m, 0x801D7004, 2)
            for offset in range(0, 32, 4):
                set32(m, 0x800F4750 + offset, rng_case.getrandbits(32))
                set32(m, 0x800F4730 + 2 * 0x20 + offset, rng_case.getrandbits(32))
            player_state = 49 + i - 14 if 14 <= i <= 16 else (130 if i % 2 == 0 else 141)
            set16(m, 0x800EC1E0 + 0x5E, player_state)
            set16(m, 0x800EC1E0 + 0xE8, 1)
            set32(m, 0x800F5120 + 0x0C, 0x801D8000)
            set32(m, 0x800F5120 + 0x10, 0x801D8040)
            set32(m, 0x800F5120 + 0x14, 0x801D8080)
            active_contact = {17: 1, 18: 2}.get(i, 0)
            for body_index, (body, contact) in enumerate(((0x801D8000, 10), (0x801D8040, 0), (0x801D8080, 0))):
                contact = 10 if body_index == active_contact else 0
                set32(m, body + 0x2C, contact << 28)
                set16(m, body + 0x18, rng_case.randrange(-4096, 4096))
                set16(m, body + 0x1C, rng_case.randrange(-4096, 4096))
            set32(m, 0x800EC1E0 + 0x124, 0x10000)
            if i == 19:
                set32(m, 0x800EC250, 0x80)
            if i == 22:
                set16(m, 0x800E5320 + 0x1B4, 0)
            set32(m, 0x800EC350, rng_case.getrandbits(32))
            set32(m, 0x800EC35C, rng_case.getrandbits(32))
        h.run("Gadget4UpdateDraw", (), setup, f"Hoop draw mode {mode}, level {level}, case {index}")

    for index, (state, aim_mode, ammo_kind, ammo, stage_flag) in enumerate((
            (127, 0, 0, 0, 0), (131, 1, 2, 1, 0), (131, 0, 2, 1, 0),
            (131, 0, 1, 0, 0), (128, 2, 0, 0, 1), (132, 0, 2, 1, 0),
            (131, 3, 2, 2, 1), (140, 0, 1, 5, 0), (131, 0, 2, 0, 0),
            (129, 1, 1, 3, 1), (131, 2, 2, 1, 0), (136, 0, 3, 0, 1),
    )):
        def setup(m, i=index, values=(state, aim_mode, ammo_kind, ammo, stage_flag)):
            player_state, aiming, kind, rounds, stage = values
            rng_case = base_hook_state(m, seed + 2600 + i)
            set_gadget_hook_results(m, result=0x800E6700 + (i % 4) * 0x40)
            set16(m, 0x800F44A2, 0 if i % 3 else 11)
            set32(m, 0x800E5478, 0x400 if stage else rng_case.getrandbits(16))
            set32(m, 0x800E5480, 0x400)
            set16(m, 0x800EC1E0 + 0x5E, player_state)
            m.write8(0x800E6916, aiming)
            m.write8(0x800F51C0, kind)
            m.write8(0x800F51C2, rounds)
            set32(m, 0x800F4910, 0x801D9000)
            for offset in range(0, 32, 4): set32(m, 0x800F4910 + offset, rng_case.getrandbits(32))
            for axis in range(3):
                set32(m, 0x800EC1E0 + 0x24 + axis * 4, rng_case.randrange(-200, 201))
                set32(m, 0x800E78C0 + axis * 4, rng_case.getrandbits(32))
            for slot in range(3):
                record = 0x800F5090 + slot * 8
                set32(m, record, 0 if slot >= i % 3 else 0x800E6500 + slot * 4)
                set16(m, record + 4, 4 + slot + i)
            for offset, value in zip((0, 2, 4), (100, -200, 300)):
                set16(m, 0x800F45A8 + offset, value)
                set16(m, 0x800F45B0 + offset, value + rng_case.randrange(-100, 101))
        h.run("Gadget3UpdateDraw", (), setup, f"Slingback state {state}, aim {aim_mode}, case {index}")

    for index, gadget in enumerate((0, 1, 2, 4, 6, 7, 8)):
        def setup(m, i=index, selected=gadget):
            rng_case = random.Random(seed + 800 + i)
            m.write8(0x800EC1E0 + 0xF2, selected)
            instance = 0x800EC060 + selected * 0x10
            model = 0x801D1000 + i * 0x40
            set32(m, instance, model)
            set16(m, model + 4, rng_case.randrange(0, 80))
            set32(m, 0x800EC35C, rng_case.getrandbits(32))
        h.run("GadgetDrawStaticModel", (), setup, f"static model gadget {gadget}")

    for index, state in enumerate((0x37, 0x38, 0x3C, 0x3D, 0x80)):
        def setup(m, i=index, player_state=state):
            m.write16(0x800EC1E0 + 0x5E, player_state)
            m.write8(0x800F51C3, i & 1)
            m.write8(0x800E6902, 1)
            m.write8(0x800E6916, 1)
            set32(m, HOOK_MARKER, i)
        h.run("GadgetDrawWithStateCheck", (0x800EC060, 0x800E7D00, 0x800F4730, index),
              setup, f"held model state {state:02X}")

    for index, (sound_state, player_state, voice_result) in enumerate((
            (0, 127, 6), (0, 128, 9), (0, 131, 11), (0, 132, 0xFFFFFFFF),
            (1, 127, 0), (1, 130, 0), (1, 131, 0),
            (2, 130, 0), (2, 131, 0), (2, 132, 0), (2, 133, 0), (3, 131, 0))):
        def setup(m, i=index, mode=sound_state, state=player_state, result=voice_result):
            rng_case = random.Random(seed + 1000 + i)
            base_hook_state(m, seed + i + 900)
            set32(m, 0x800F5128, mode)
            set16(m, 0x800F5124, 13 + i)
            set16(m, 0x800F5126, 17 + i)
            set16(m, 0x800EC1E0 + 0x5E, state)
            set32(m, HOOK_RESULT, result)
            set32(m, 0x800EC35C, rng_case.getrandbits(32))
            set32(m, 0x800EC350, rng_case.getrandbits(32))
        h.run("Gadget6UpdateSound", (), setup,
              f"sound state {sound_state}, player {player_state:02X}, result {voice_result:08X}")

    for index, mode in enumerate((0, 1, 2)):
        def setup(m, i=index, current_mode=mode):
            rng_case = random.Random(seed + 1200 + i)
            set16(m, 0x800F5124, current_mode)
            set32(m, 0x800EC0A0, 0x801D2000 + i * 0x100)
            set32(m, 0x800EC120, 0x801D2040 + i * 0x100)
            set16(m, 0x801D2004 + i * 0x100, 2 + i)
            set16(m, 0x801D2044 + i * 0x100, 4 + i)
            for offset in range(0, 32, 4):
                set32(m, 0x800F4730 + (2 + i) * 0x20 + offset, rng_case.getrandbits(32))
                set32(m, 0x800E9C60 + (4 + i) * 0x20 + offset, rng_case.getrandbits(32))
            set16(m, 0x800F5138, rng_case.randrange(-2000, 2001))
            set16(m, 0x800F513C, rng_case.randrange(-2000, 2001))
            set32(m, 0x800F5128, rng_case.getrandbits(32))
            set32(m, 0x800EC350, rng_case.getrandbits(32))
            set32(m, 0x800EC35C, rng_case.getrandbits(32))
        h.run("Gadget4BuildMatrix", (), setup, f"build matrix mode {mode}",
              initial_regs={17: 0x800EC0A0 + index * 0x10})

    for index, (source_matrix, output_matrix, angle) in enumerate((
            (0x801D3000, 0, 0), (0x801D3080, 0x801D30C0, 0x400),
            (0x801D3100, 0x801D3140, -0x1000))):
        def setup(m, i=index, source=source_matrix, output=output_matrix, turn=angle):
            rng_case = random.Random(seed + 1500 + i)
            for base in (source, output):
                if not base: continue
                for offset in range(0, 32, 4):
                    set32(m, base + offset, rng_case.getrandbits(32))
            set32(m, 0x800EC350, rng_case.getrandbits(32))
            set32(m, 0x800EC35C, rng_case.getrandbits(32))
        h.run("GadgetDrawSlot0Attachment", (source_matrix, output_matrix, angle), setup,
              f"slot-0 attachment {index}")

    swing_cases = (
        (1, 0, 100, 10, -20, 0, -1, 0, 9),
        (0, 138, 100, 12, -8, 5, -1, -17, 12),
        (0, 139, 9000, 500, 80, 6, 4, 22, 13),
        (0, 137, -10, 1, -1, 0, 7, 0, 14),
        (0, 140, 2000, 30, 20, 1, -1, 100, 15),
        (0, 142, 1000, 3, 0, 6, 8, -100, 16),
        (0, 138, 32760, 500, 100, 9, -1, 5, 0xFFFFFFFF),
        (0, 141, 8000, 50, -40, 2, 22, -5, 18),
    )
    for index, (flags, state, angle, acceleration, speed, timer, voice, spin, result) in enumerate(swing_cases):
        def setup(m, i=index, values=(flags, state, angle, acceleration, speed, timer, voice, spin, result)):
            stage_flags, player_state, swing_angle, swing_accel, swing_speed, trail_timer, loop_voice, stick_spin, returned = values
            base_hook_state(m, seed + 1600 + i)
            set32(m, 0x800F4450, stage_flags)
            set16(m, 0x800EC1E0 + 0x5E, player_state)
            set16(m, 0x800F5148, swing_angle)
            set16(m, 0x800F514A, swing_accel)
            set16(m, 0x800F514C, swing_speed)
            set16(m, 0x800F5164, trail_timer)
            set16(m, 0x800F5168, loop_voice)
            set16(m, 0x800E54D4, stick_spin)
            set32(m, 0x800EC0FC, 0x801D4000)
            set32(m, HOOK_RESULT, returned)
        h.run("GadgetSwingUpdate", (), setup, f"swing case {index}")

    for event in (0, 8, 0xFFFF, 0xFFF7):
        h.run("GadgetSwingParamsInit", (), lambda m, v=event: set16(m, 0x800E549E, v), f"gesture events {event:04X}")

    for index in range(count(12)):
        initial, mask = rng.getrandbits(32), rng.getrandbits(32)
        def setup(m, before=initial): set32(m, 0x800F51C4, before)
        h.run("GadgetUnlockMask", (mask,), setup, f"mask OR {index}")

    for index, (selected, gadget, ga, gb) in enumerate(((0, 0, 0, 0), (1, 1, 0, 0), (9, 7, 1, 0),
                                                        (7, 7, 0, 0), (2, 9, 0, 1), (255, 3, 2, 2))):
        def setup(m, i=index, s=selected, a=gadget, g1=ga, g2=gb):
            base_hook_state(m, seed + i + 100)
            m.write8(0x800F51C3, s); m.write8(0x800E6902, g1); m.write8(0x800E6916, g2)
        h.run("GadgetSetSelected", (gadget,), setup, f"selection {selected}->{gadget}")

    for index in range(count(20)):
        old = rng.choice([-1, 0, 1, 2, 7, 9, 255])
        new = rng.choice([-1, 0, 1, 2, 7, 9, 255])
        slots = [rng.choice([-1, 0, 1, 2, 7, 9, 255]) for _ in range(4)]
        if index % 2 == 0: slots[index % 4] = old
        def setup(m, values=slots):
            for slot, value in enumerate(values): m.write8(0x800F51A8 + slot, value)
        h.run("GadgetSlotRemap", (old, new), setup, f"remap {old}->{new} {index}")


def run_mutations(area_member):
    """Exercise the RAM-diff path with 50 distinct, function-scoped wrong copies."""
    original_source = (ROOT / "src/player/ae_gadget.c").read_text(encoding="utf-8")
    baseline_side = Side(build())
    harness = GadgetHarness(baseline_side, area_member)
    harness.quiet = True

    # Reuse the first real default fixture for each function. Capture its setup closure without
    # running it, so every mutant gets exactly the same EXE/member state and hook inputs.
    fixtures = {}
    saved_run = harness.run
    def capture(name, args, setup, label, initial_regs=None):
        fixtures.setdefault(name, (args, setup, label, initial_regs))
    harness.run = capture
    run_cases(harness, 1.0, 94423)
    harness.run = saved_run
    if set(fixtures) != set(FUNCS):
        print("mutation fixture capture missed: " + ", ".join(set(FUNCS) - set(fixtures)))
        return 1

    # Confirm the selected default fixtures match before testing mutated copies.
    for name, (args, setup, label, initial_regs) in fixtures.items():
        harness.run(name, args, setup, label + " mutation baseline", initial_regs)
    if harness.failures:
        print(f"mutation baseline failed on {harness.failures} default fixtures")
        return 1

    targets = list(FUNCS)
    caught = 0
    for index in range(50):
        target = targets[index % len(targets)]
        c_function = FUNCS[target][2]
        definition = re.search(
            rf"(?m)^(?:void|int32_t)\s+{re.escape(c_function)}\s*\([^;{{}}]*\)\s*\{{",
            original_source)
        if definition is None:
            print(f"mutation harness could not find the definition of {c_function}")
            return 1
        brace = definition.end() - 1
        address = 0x80100000 + index * 4
        value = 0xA5000000 | index
        mutation = (f"\n    /* Intentional mutation {index + 1}: an unexpected RAM write. */\n"
                    f"    ae_wr32(r, 0x{address:08X}u, 0x{value:08X}u);\n")
        mutant_source = original_source[:brace + 1] + mutation + original_source[brace + 1:]
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", suffix=".c",
                                         dir=ROOT / "extract/mips", delete=False) as source_file:
            source_file.write(mutant_source)
            source_path = Path(source_file.name)
        try:
            mutant_dll = build(source=source_path)
        finally:
            source_path.unlink(missing_ok=True)

        harness.side = Side(mutant_dll)
        failures_before = harness.failures
        args, setup, label, initial_regs = fixtures[target]
        harness.run(target, args, setup, f"mutation {index + 1}: {target} / {label}", initial_regs)
        if harness.failures == failures_before:
            print(f"mutation {index + 1} escaped detection in {target}")
            return 1
        caught += 1

    print(f"PASS: {caught}/50 mutations caught across {len(FUNCS)} functions")
    return 0 if caught == 50 else 1


def main():
    sys.path.insert(0,str(ROOT/'tools/codex'))
    import gte_oracle
    oracle=gte_oracle.build()
    gte_oracle.build=lambda:oracle
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--cases", type=float, default=1.0, help="scale randomized cases")
    parser.add_argument("--seed", type=int, default=94423)
    parser.add_argument("--trapv", action="store_true")
    parser.add_argument("--mutate", action="store_true", help="run 50 intentional wrong-copy mutations")
    args = parser.parse_args()
    exe = ROOT / "extract/disc/SCUS_944.23"
    if not exe.exists():
        print("SKIP: extract/disc/SCUS_944.23 not present")
        return 0
    members = dump_areas.area_members()
    member_id = members.get(1)
    if member_id is None:
        raise SystemExit("area 0x01 has no real area-data member")
    area_member = ROOT / f"extract/members/{member_id:03X}.bin"
    if not area_member.exists():
        raise SystemExit(f"area 0x01 member is missing: {area_member}")

    if args.mutate:
        return run_mutations(area_member)

    side = Side(build(trapv=args.trapv))
    h = GadgetHarness(side, area_member)
    started = time.time()
    run_cases(h, args.cases, args.seed)
    default_run = args.cases == 1.0 and args.seed == 94423 and not args.trapv
    summary = h.finish(default_run, args.coverage or default_run)
    # The isolated hook model cannot detect callee-local aliasing. Compose the
    # real flash effects and SDK MatrixLookDir against original instructions.
    from check_game_area import Harness as GameHarness
    GameHarness().flash_regression()
    print("per-function differential cases:")
    for name, cases in h.counts.items(): print(f"  {name:28s} {cases}")
    print(f"total cases {sum(h.counts.values())}; {time.time() - started:.1f}s")
    gaps = [name for name, row in summary.items() if row["uncovered"] or row["stale_declared"]]
    if gaps: print("coverage gaps: " + ", ".join(gaps))
    if h.failures or gaps:
        print(f"FAILED: {h.failures} differential mismatches")
        return 1
    print("PASS: gadget function diffs matched")
    return 0


if __name__ == "__main__":
    sys.exit(main())
