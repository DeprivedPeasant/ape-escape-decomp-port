"""Differentially check gimmick update helpers against the SCUS-94423 MIPS listings.

Usage: .venv/Scripts/python.exe tools/mips/check_gimmick_b.py [--coverage] [--mutate] [--function NAME]
"""
import argparse
import ctypes as C
import gc
import os
from pathlib import Path
import re
import random
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

STACK_LO = 0x1FF000
GIMMICKS = 0x801B0000
COLLIDERS = 0x801C0000
PROPS = 0x801D0000
MESHES = 0x801E0000
MESH_SETS = 0x801E1000
BOX = 0x801E2000
DEST = 0x801E4000
HOOK_MARKER = 0x800B1000
_GTE_ORACLE_LIB = None


def attach_gte_oracle(cpu):
    """Share the pinned GTE DLL across harnesses; Windows cannot overwrite a loaded DLL."""
    global _GTE_ORACLE_LIB
    if _GTE_ORACLE_LIB is None:
        sys.path.insert(0, str(ROOT / "tools/codex"))
        import gte_oracle
        _GTE_ORACLE_LIB = gte_oracle.build()
    state = (C.c_uint32 * 64)()
    cpu.gte_state = state
    lib = _GTE_ORACLE_LIB
    cpu.gte_backend = lambda instruction, value: lib.oracle_gte_step(
        state, instruction, value & 0xFFFFFFFF)
    return cpu

# The five shared GimmickSwing helpers belong to check_gimmick_a.
# Batch B has no callers of them; its remaining callees keep running end to end.
FUNCS = {
    "Gimmick_Init_MonkeyQuota": (0x80090D6C, 40, "ae_Gimmick_Init_MonkeyQuota", True, 1),
    "Gimmick_MonkeyQuota_Update": (0x80090D94, 192, "ae_Gimmick_MonkeyQuota_Update", True, 1),
    "Gimmick_NopInit": (0x80090EB0, 8, "ae_Gimmick_NopInit", False, 1),
    "Gimmick_NopUpdate": (0x80090EB8, 8, "ae_Gimmick_NopUpdate", False, 1),
    "Gimmick_Init_Closed": (0x80090EC0, 76, "ae_Gimmick_Init_Closed", False, 1),
    "Gimmick_Init_ZoneMask": (0x80090F0C, 116, "ae_Gimmick_Init_ZoneMask", True, 1),
    "Gimmick_PlayerInCylinder_Update": (0x80090F80, 224, "ae_Gimmick_PlayerInCylinder_Update", True, 1),
    "Gimmick_ProxTrigger_Init": (0x80091060, 280, "ae_Gimmick_ProxTrigger_Init", True, 1),
    "Gimmick_ProxTrigger_Update": (0x80091178, 480, "ae_Gimmick_ProxTrigger_Update", True, 1),
    "Gimmick_Breakable_Init": (0x80091358, 148, "ae_Gimmick_Breakable_Init", False, 1),
    "Gimmick_Breakable_Update": (0x800913EC, 428, "ae_Gimmick_Breakable_Update", True, 1),
    "Gimmick_Init_Closed2": (0x80091688, 116, "ae_Gimmick_Init_Closed2", False, 1),
    "Gimmick_NopUpdate2": (0x800916FC, 8, "ae_Gimmick_NopUpdate2", False, 1),
    "Gimmick_Init_PropHidden": (0x80091704, 104, "ae_Gimmick_Init_PropHidden", False, 1),
    "Gimmick_Resize_Update": (0x8009176C, 260, "ae_Gimmick_Resize_Update", True, 1),
    "Gimmick_Init_Closed3": (0x80091870, 140, "ae_Gimmick_Init_Closed3", True, 1),
    "Gimmick_Slider_Update": (0x800918FC, 284, "ae_Gimmick_Slider_Update", True, 1),
    "Gimmick_SliderQuiet_Update": (0x80091A18, 216, "ae_Gimmick_SliderQuiet_Update", True, 1),
    "Gimmick_PlayerBox_Update": (0x80091CF8, 44, "ae_Gimmick_PlayerBox_Update", False, 1),
    # The inventory size stops at 0x80091EDC, but the function's tail runs through
    # its epilogue at 0x80091FAC, immediately before Gimmick_Init_RecordOrigin.
    "GimmickPlayerInBox": (0x80091D24, 652, "ae_GimmickPlayerInBox", False, 3),
    "Gimmick_Init_RecordOrigin": (0x80091FB0, 252, "ae_Gimmick_Init_RecordOrigin", True, 1),
    "Gimmick_LinkedLift_Update": (0x800920AC, 632, "ae_Gimmick_LinkedLift_Update", True, 1),
    "Gimmick_Init_PropTilt": (0x80092A68, 164, "ae_Gimmick_Init_PropTilt", False, 1),
    "Gimmick_Oscillate_Update": (0x80092B0C, 872, "ae_Gimmick_Oscillate_Update", True, 1),
    "Gimmick_Init_PropMeshSet": (0x80092E74, 220, "ae_Gimmick_Init_PropMeshSet", True, 1),
    "Gimmick_Init_CounterGate": (0x80092F50, 108, "ae_Gimmick_Init_CounterGate", False, 1),
    "Gimmick_Counter_EventCam_Update": (0x80092FBC, 232, "ae_Gimmick_Counter_EventCam_Update", True, 1),
    "Gimmick_PlayerBlast_Update": (0x800935F0, 332, "ae_Gimmick_PlayerBlast_Update", True, 1),
    "Gimmick_Init_Disabled": (0x80093740, 72, "ae_Gimmick_Init_Disabled", False, 1),
    "Gimmick_Grab_Update": (0x80093788, 192, "ae_Gimmick_Grab_Update", True, 1),
    "Gimmick_Init_PropFlag2000": (0x8009391C, 112, "ae_Gimmick_Init_PropFlag2000", False, 1),
    "Gimmick_Bounce_Update": (0x8009398C, 1256, "ae_Gimmick_Bounce_Update", True, 1),
    "Gimmick_Init_Hidden": (0x80093E74, 144, "ae_Gimmick_Init_Hidden", False, 1),
    # Its eight-way jump table targets out-of-line blocks through 0x8009432C;
    # the helper FUN_80094330 begins immediately after the epilogue.
    "Gimmick_Rotate_Update": (0x80093F04, 1068, "ae_Gimmick_Rotate_Update", True, 1),
    "Gimmick_Falling_Update": (0x8009468C, 452, "ae_Gimmick_Falling_Update", True, 1),
    "GimmickMoveColliderTo": (0x80094B6C, 164, "ae_GimmickMoveColliderTo", True, 3),
    "GimmickMoveCollider": (0x80094C10, 588, "ae_GimmickMoveCollider", True, 4),
    "Gimmick_NopUpdate3": (0x80094F40, 8, "ae_Gimmick_NopUpdate3", False, 1),
    "Gimmick_NopUpdate4": (0x80095464, 8, "ae_Gimmick_NopUpdate4", False, 1),
    "GimmickStateTablePtr": (0x800962A4, 12, "ae_GimmickStateTablePtr", False, 0),
}
HOOKS = [
    ("AreaMonkeyQuotaMet", 0x80097454, 1), ("FUN_80097504", 0x80097504, 0),
    ("SpecialActorPlaceAtCollider", 0x8009B578, 3), ("FUN_8009B3B0", 0x8009B3B0, 1),
    ("IsTimeAttack", 0x80097678, 0), ("Indicator_Register", 0x800716EC, 3),
    ("Indicator_Activate", 0x800717B4, 2),
    ("SpawnEffectGroupAt", 0x800698E4, 4),
    ("PadRumble", 0x80014168, 3), ("FUN_8006AA60", 0x8006AA60, 3),
    ("MeshInitFromAreaTable", 0x80015CC8, 2), ("FUN_80091598", 0x80091598, 2),
    ("FUN_80091AF0", 0x80091AF0, 2),
    ("SoundLoopStart", 0x800878E0, 2), ("SoundEmitterPan", 0x80087B34, 4),
    ("SoundLoopStop", 0x8008806C, 2), ("FUN_80092324", 0x80092324, 2),
    ("FUN_80091C04", 0x80091C04, 3), ("EventCamStart", 0x8004D39C, 1),
    ("FUN_80093848", 0x80093848, 3), ("PlayerAttachSet", 0x80066EEC, 3),
    ("PlaySfx", 0x800877F8, 1), ("FUN_80066F34", 0x80066F34, 0),
    ("SvecToDiagMatrix", 0x800147D0, 2), ("FUN_80094330", 0x80094330, 4),
    ("FUN_80094A48", 0x80094A48, 1),
    ("FUN_80094E5C", 0x80094E5C, 1), ("ColliderBoxContainsPoint", 0x8009091C, 4),
    ("CameraNudge", 0x80047690, 3), ("GetSolidHeight", 0x8004D8C0, 3),
    ("PropSetMesh", 0x80090E54, 4), ("FUN_801414a0", 0x801414A0, 1),
]
HOOK_COUNT = {name: n for name, _, n in HOOKS}
HOOK_TAG = {name: (i + 1) * 0x1021 for i, (name, _, _) in enumerate(HOOKS)}
HookFn = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, *([C.c_uint32] * 4))
WorldHookFn = HookFn
B_HOOK_NAMES = [
    "AreaMonkeyQuotaMet", "FUN_80097504", "SpecialActorPlaceAtCollider", "FUN_8009B3B0",
    "IsTimeAttack", "Indicator_Register", "Indicator_Activate",
    "SpawnEffectGroupAt", "PadRumble", "FUN_8006AA60",
    "MeshInitFromAreaTable", "FUN_80091598", "FUN_80091AF0",
    "SoundLoopStart", "SoundEmitterPan", "SoundLoopStop", "FUN_80092324",
    "FUN_80091C04", "EventCamStart", "FUN_80093848", "PlayerAttachSet", "PlaySfx",
    "FUN_80066F34", "SvecToDiagMatrix", "FUN_80094330", "DiagonalSignedDistance",
    "FUN_80094A48", "FUN_80094E5C", "ColliderBoxContainsPoint", "CameraNudge",
]


class CHooks(C.Structure):
    _fields_ = [("user", C.c_void_p)] + [(name, HookFn) for name in B_HOOK_NAMES] + [
        ("gimmick_hooks", C.c_void_p), ("world_hooks", C.c_void_p), ("body_hooks", C.c_void_p),
        ("sound_hw", C.c_void_p),
    ]


class WorldHooks(C.Structure):
    _fields_ = [("user", C.c_void_p)] + [(name, WorldHookFn) for name in (
        "GetSolidHeight", "PlaySfxAt", "GimmickActivate", "MeshInitFromAreaTable",
        "FUN_8013d358", "CameraShake", "EventCamStart")]
    _fields_ += [("ground_stale_x", C.c_uint16), ("ground_stale_z", C.c_uint16)]


class GimmickHooks(C.Structure):
    _fields_ = [("user", C.c_void_p)] + [(name, HookFn) for name in (
        "PlaySfx", "PlaySfxAt", "PropSetMesh", "FUN_801414a0")]


WaitIdleFn = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p)
WaitLoopFn = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32)
VoiceAdsrFn = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint16, C.c_uint16)
VoiceKeyOffFn = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint16)
PrintErrorFn = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32,
                           C.c_int32, C.c_int32, C.c_int32)


class SoundHw(C.Structure):
    _fields_ = [("user", C.c_void_p), ("wait_idle", WaitIdleFn), ("wait_loop", WaitLoopFn),
                ("set_voice_adsr", VoiceAdsrFn), ("voice_key_off", VoiceKeyOffFn),
                ("printf_error", PrintErrorFn)]


def build(source=None, tag="gimmick_b", out_dir=None):
    out = Path(out_dir or ROOT / "extract/mips")
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{os.getpid()}_{time.time_ns()}.dll"
    sources = [
        ROOT / "src/core/ae_ram.c", Path(source or ROOT / "src/world/ae_gimmick_b.c"),
        ROOT / "src/world/ae_gimmick_a.c",
        ROOT / "src/world/ae_gimmick.c", ROOT / "src/world/ae_world.c",
        ROOT / "src/world/ae_solid.c",
        ROOT / "src/effects/ae_spawn.c",
        ROOT / "src/sound/ae_sound.c",
        ROOT / "src/bodies/ae_bodies.c", ROOT / "src/player/native_player.c",
        ROOT / "src/core/ae_sdk.c", ROOT / "src/core/ae_sdk_matrix.c",
    ]
    subprocess.run([
        shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-shared", "-static-libgcc", "-I" + str(ROOT / "src/world"),
        "-I" + str(ROOT / "src/core"), "-I" + str(ROOT / "src/bodies"),
        "-I" + str(ROOT / "src/sound"),
        *map(str, sources), "-o", str(dll), "-lm",
    ], check=True)
    return dll


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
        lib.ae_rd16.argtypes = [P, U]
        lib.ae_rd16.restype = U
        lib.ae_rd32.argtypes = [P, U]
        lib.ae_rd32.restype = U
        lib.ae_wr16.argtypes = [P, U, U]
        lib.ae_wr32.argtypes = [P, U, U]
        lib.ae_PlaySfxAt.argtypes = [P, P, I, U]
        lib.ae_PlaySfxAt.restype = I
        lib.ae_CameraShake.argtypes = [P, I]
        lib.ae_CameraShake.restype = None
        for name, (_, _, cname, has_hooks, nargs) in FUNCS.items():
            f = getattr(lib, cname)
            f.argtypes = [P] + ([P] if has_hooks else []) + [U] * nargs
            f.restype = U if name == "GimmickStateTablePtr" else I
        self.ram = lib.ae_ram_create()
        if not self.ram:
            raise SystemExit("ae_ram_create failed")
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(
            lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scr = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(
            lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.hooks = CHooks()
        self.callbacks = {}
        self.model = None
        self.sound_idle = 0
        self.sound_hw = SoundHw()
        self.sound_callbacks = [
            WaitIdleFn(lambda user, ram: self.record_sound_event("wait_idle") or self.sound_idle),
            WaitLoopFn(lambda user, ram, units: self.record_sound_event("wait_loop", units)),
            VoiceAdsrFn(lambda user, ram, channel, adsr1, adsr2:
                        self.record_sound_event("set_voice_adsr", channel, adsr1, adsr2)),
            VoiceKeyOffFn(lambda user, ram, half, mask:
                          self.record_sound_event("voice_key_off", half, mask)),
            PrintErrorFn(lambda user, ram, fmt, argc, a, b, c:
                         self.record_sound_event("printf_error", fmt, argc,
                                                 *[a, b, c][:argc])),
        ]
        for (name, _), callback in zip(SoundHw._fields_[1:], self.sound_callbacks):
            setattr(self.sound_hw, name, callback)
        self.hooks.sound_hw = C.addressof(self.sound_hw)
        for name in B_HOOK_NAMES:
            cb = HookFn(lambda user, ram, a0, a1, a2, a3, n=name:
                        self.model.hook(n, (a0, a1, a2, a3)))
            self.callbacks[name] = cb
            setattr(self.hooks, name, cb)
        self.world_hooks = WorldHooks()
        self.world_callbacks = {}
        for name in ("GetSolidHeight", "PlaySfxAt", "GimmickActivate", "MeshInitFromAreaTable",
                     "FUN_8013d358", "CameraShake", "EventCamStart"):
            if name == "CameraShake":
                cb = WorldHookFn(self.camera_shake)
            elif name == "PlaySfxAt":
                cb = WorldHookFn(self.sound_call)
            else:
                cb = WorldHookFn(lambda user, ram, a0, a1, a2, a3, n=name:
                                 self.model.hook(n, (a0, a1, a2, a3)))
            self.world_callbacks[name] = cb
            setattr(self.world_hooks, name, cb)
        self.hooks.world_hooks = C.addressof(self.world_hooks)
        self.gimmick_hooks = GimmickHooks()
        self.gimmick_callbacks = {}
        for name in ("PlaySfx", "PlaySfxAt", "PropSetMesh", "FUN_801414a0"):
            if name == "PlaySfxAt":
                cb = HookFn(self.sound_call)
            else:
                cb = HookFn(lambda user, ram, a0, a1, a2, a3, n=name:
                            self.model.hook(n, (a0, a1, a2, a3)))
            self.gimmick_callbacks[name] = cb
            setattr(self.gimmick_hooks, name, cb)
        self.hooks.gimmick_hooks = C.addressof(self.gimmick_hooks)

    def record_sound_event(self, name, *args):
        if self.model is not None:
            self.model.record(name, *args)

    def sound_call(self, user, ram, sound_id, position, unused2, unused3):
        return self.lib.ae_PlaySfxAt(ram, C.byref(self.sound_hw), signed32(sound_id), position)

    def camera_shake(self, user, ram, level, unused1, unused2, unused3):
        self.lib.ae_CameraShake(ram, signed32(level))
        return 0

    def sync(self, m):
        C.memmove(self.main, bytes(m.ram), mr.RAM_SIZE)
        C.memmove(self.scr, bytes(m.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)

    def read16(self, address):
        return self.lib.ae_rd16(self.ram, address)

    def read32(self, address):
        return self.lib.ae_rd32(self.ram, address)

    def write32(self, address, value):
        self.lib.ae_wr32(self.ram, address, value & 0xFFFFFFFF)

    def write16(self, address, value):
        self.lib.ae_wr16(self.ram, address, value & 0xFFFF)

    def call(self, name, args, with_hooks=True):
        _, _, cname, has_hooks, _ = FUNCS[name]
        f = getattr(self.lib, cname)
        return f(self.ram, *([C.byref(self.hooks)] if has_hooks and with_hooks else []), *args)


class HookModel:
    def __init__(self, mem, cfg=None):
        self.mem = mem
        self.log = []
        self.cfg = cfg or {}
        self.ground_height_index = 0
        self.sound_idle = 0

    def record(self, name, *args):
        self.log.append((name, *args))

    def hook(self, name, words):
        count = HOOK_COUNT[name]
        if name == "PlayerAttachSet" and words[0] != 0x801FFEF0:
            count = 2  # the other two call sites leave a2 unspecified in the listing
        args = tuple(int(x) & 0xFFFFFFFF for x in words[:count])
        self.log.append((name, *args))
        marker = self.mem.read32(HOOK_MARKER)
        mixed = sum((word * (i + 1)) & 0xFFFFFFFF for i, word in enumerate(args))
        self.mem.write32(HOOK_MARKER, (marker + mixed + HOOK_TAG[name]) & 0xFFFFFFFF)
        if name == "AreaMonkeyQuotaMet":
            self.mem.write32(args[0], self.cfg.get("quota_count", 3))
            return self.cfg.get("quota_met", 0)
        if name == "FUN_80097504":
            return self.cfg.get("quota_count", 3)
        if name == "IsTimeAttack":
            return self.cfg.get("time_attack", 0)
        if name == "Indicator_Register":
            return self.cfg.get("indicator_id", 7)
        if name == "FUN_80092324":
            return self.cfg.get("record_origin_result", 1)
        if name == "FUN_80094330":
            return self.cfg.get("rotate_probe", 1)
        if name == "DiagonalSignedDistance":
            out = self.cfg.get("diagonal_output", 0)
            self.mem.write16(args[3], out)
            return self.cfg.get("diagonal_distance", 0)
        if name == "FUN_80094A48":
            return self.cfg.get("falling_probe", 1)
        if name == "FUN_80094E5C":
            return self.cfg.get("falling_collision", 1)
        if name == "FUN_80093848":
            out = args[2]
            for i, value in enumerate(self.cfg.get("grab_point", (0, 0, 0))):
                self.mem.write32(out + i * 4, value)
            return self.cfg.get("grab_success", 1)
        if name == "ColliderBoxContainsPoint":
            return self.cfg.get("box_contains", 0)
        if name == "GetSolidHeight":
            self.mem.write16(0x800F45E6, 0)
            values = self.cfg.get("ground_heights")
            if values:
                index = min(self.ground_height_index, len(values) - 1)
                self.ground_height_index += 1
                return values[index]
            return self.cfg.get("ground_height", 0)
        return 0


def signed16(value):
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def signed32(value):
    value &= 0xFFFFFFFF
    return value - 0x100000000 if value & 0x80000000 else value


class GimmickBHarness:
    def __init__(self, source=None, with_coverage=True, out_dir=None, tag="gimmick_b"):
        self.funcs = FUNCS
        member = dump_areas.area_members()[1]
        self.m = mr.MipsRam.from_extract(ROOT / f"extract/members/{member:03X}.bin")
        attach_gte_oracle(self.m)
        self.m.coverage = set() if with_coverage else None
        self.base = self.m.snapshot()
        self.side = Side(build(source, tag=tag, out_dir=out_dir))
        self.stack_lo = STACK_LO
        self.counts = {}
        self.failures = 0
        self.mutation_mode = False
        self.mutation_caught = False
        reg.declare("stack_window", "801FF000..80200000")

    def setup(self, m, name, cfg=None, seed=94423):
        cfg = cfg or {}
        rng = random.Random(seed)
        m.write32(0x800F455C, COLLIDERS)
        m.write32(0x800F4570, GIMMICKS)
        m.write32(0x800B4D64, PROPS)
        m.write32(0x800F4770, MESH_SETS)
        m.write16(0x800F44A0, cfg.get("event_busy", 0))
        m.write16(0x800F4474, cfg.get("level", 0))
        m.write16(0x800F4476, cfg.get("area", 0))
        m.write32(0x800F5A90, cfg.get("rc_car", 0))
        m.write16(0x800EC238, cfg.get("wall_state", -1))
        m.write32(0x800EC204, cfg.get("player_x", 0))
        m.write32(0x800EC208, cfg.get("player_y", 0))
        m.write32(0x800EC20C, cfg.get("player_z", 0))
        m.write16(0x800EC238, cfg.get("wall_state", -1))
        m.write16(0x800EC1E0 + 0x44, cfg.get("player_vertical_extent", 0))
        m.write16(0x800EC1E0 + 0x58, cfg.get("player_collider", -1))
        m.write16(0x800EC1E0 + 0x30, cfg.get("player_rotate_x", 0))
        m.write16(0x800EC1E0 + 0x38, cfg.get("player_rotate_z", 0))
        for key, offset in (("player_rotate_x32", 0x30), ("player_rotate_z32", 0x38)):
            if key in cfg:
                m.write32(0x800EC1E0 + offset, cfg[key])
        m.write16(0x800F45E6, cfg.get("wall_probe", 0))
        for i in range(8):
            g = GIMMICKS + i * 0x18
            m.write_bytes(g, rng.randbytes(0x18))
            m.write16(g + 0x00, rng.randrange(0x10000))
            m.write16(g + 0x02, rng.randrange(0x10000))
            m.write16(g + 0x04, i)
            m.write16(g + 0x06, rng.randrange(0x10000))
            m.write16(g + 0x08, rng.randrange(0x10000))
            m.write16(g + 0x0A, rng.randrange(0x10000))
            m.write16(g + 0x0C, -1)
            m.write16(g + 0x0E, rng.randrange(0x10000))
            m.write16(g + 0x10, rng.randrange(0x10000))
            m.write16(g + 0x12, rng.randrange(0x10000))
            m.write16(g + 0x14, rng.randrange(0x10000))
            m.write16(g + 0x16, rng.randrange(0x10000))
            c = COLLIDERS + i * 0x38
            m.write_bytes(c, rng.randbytes(0x38))
            m.write16(c + 0x00, rng.randrange(0x10000))
            m.write16(c + 0x02, rng.randrange(0x10000))
            m.write16(c + 0x04, rng.randrange(0x10000))
            m.write16(c + 0x06, rng.randrange(0x10000))
            m.write16(c + 0x08, rng.randrange(0x10000))
            m.write16(c + 0x0A, rng.randrange(0x10000))
            m.write16(c + 0x0C, rng.randrange(0x10000))
            m.write16(c + 0x0E, i)
            m.write16(c + 0x12, rng.randrange(0x10000))
            m.write16(c + 0x14, rng.randrange(0x10000))
            m.write16(c + 0x18, rng.randrange(0x10000))
            m.write16(c + 0x1E, -1)
            m.write16(c + 0x20, rng.randrange(0x10000))
            m.write16(c + 0x32, rng.randrange(0x10000))
            m.write16(c + 0x34, rng.randrange(0x10000))
            m.write16(c + 0x36, rng.randrange(0x10000))
            p = PROPS + i * 0x20
            m.write_bytes(p, rng.randbytes(0x20))
            m.write16(p + 0x16, rng.randrange(0x10000))
            mesh = MESHES + i * 0x20
            m.write32(p + 0, mesh)
            m.write32(mesh, (rng.getrandbits(32) & 0xFFFFFFF0) | cfg.get("mesh_type", 3))
            m.write32(p + 0x1C, MESHES + (i + 8) * 0x20)
            set_entry = MESH_SETS + i * 0x18
            m.write16(set_entry + 0x16, 2)
        for i in range(256):
            m.write16(0x800BBDA0 + i * 0x14, -1)
        for i in range(128):
            m.write16(0x800BEDA0 + i * 2, -1)
        for mesh_index, count in cfg.get("mesh_counts", {}).items():
            m.write16(GIMMICKS + int(mesh_index) * 0x18 + 0x16, count)
        m.write32(HOOK_MARKER, rng.getrandbits(32))
        idx = int(cfg.get("index", 1))
        g = GIMMICKS + idx * 0x18
        c = COLLIDERS + idx * 0x38
        m.write16(g + 0x04, idx)
        m.write16(c + 0x0E, cfg.get("prop_index", idx))
        if "flags" in cfg:
            m.write16(c + 0x18, cfg["flags"])
        if "subtype" in cfg:
            m.write16(g + 0x08, cfg["subtype"])
        if "link" in cfg:
            m.write16(g + 0x0C, cfg["link"])
        if "param_a" in cfg:
            m.write16(g + 0x0A, cfg["param_a"])
        if "param_14" in cfg:
            m.write16(g + 0x14, cfg["param_14"])
        for key, offset in (("timer", 0x06), ("param_10", 0x10), ("param_12", 0x12),
                            ("param_16", 0x16), ("param_e", 0x0E)):
            if key in cfg:
                m.write16(g + offset, cfg[key])
        for key, offset in (("state", 0x02), ("subtype", 0x08), ("param_a", 0x0A),
                            ("link", 0x0C)):
            if key in cfg:
                m.write16(g + offset, cfg[key])
        for key, offset in (("collider_x", 0x00), ("collider_y", 0x02), ("collider_z", 0x04),
                            ("half_x", 0x06), ("half_y", 0x08), ("half_z", 0x0A),
                            ("interact", 0x0C), ("flags", 0x18), ("touched_by", 0x1E),
                            ("breakable_count", 0x32)):
            if key in cfg:
                m.write16(c + offset, cfg[key])
        if "sound_handle" in cfg:
            m.write16(c + 0x1C, cfg["sound_handle"])
        if "count" in cfg:
            linked = GIMMICKS + int(cfg.get("linked_index", 2)) * 0x18
            m.write16(linked + 0x16, cfg["count"])
        if "linked_index" in cfg:
            m.write16(g + 0x08, cfg["linked_index"])
        if "area" in cfg:
            m.write16(0x800F4476, cfg["area"])
            m.write8(0x800F447C, cfg["area"])
        if "collider_type" in cfg:
            m.write16(c + 0x10, cfg["collider_type"])
        for key, offset in (("swing_x", 0x2A), ("swing_y", 0x2C), ("swing_z", 0x2E),
                            ("swing_step", 0x36), ("aux_half_x", 0x32),
                            ("aux_half_y", 0x34), ("aux_half_z", 0x36)):
            if key in cfg:
                m.write16(c + offset, cfg[key])
        if "gimmick_type" in cfg:
            m.write16(g + 0x00, cfg["gimmick_type"])
        if "child_index" in cfg:
            m.write16(g + 0x0A, cfg["child_index"])
        if "target_type" in cfg:
            target = GIMMICKS + int(cfg.get("target_index", 2)) * 0x18
            m.write16(target + 0x00, cfg["target_type"])
        if "target_state" in cfg:
            target = GIMMICKS + int(cfg.get("target_index", 2)) * 0x18
            m.write16(target + 0x02, cfg["target_state"])
        if "gimmick_box" in cfg:
            for i, value in enumerate(cfg["gimmick_box"]):
                m.write16(g + 0x0A + i * 2, value)
        if "mesh_header" in cfg:
            mesh_index = int(cfg.get("prop_index", idx))
            m.write32(MESHES + mesh_index * 0x20, cfg["mesh_header"])
        for i, value in enumerate(cfg.get("prop_position", (0, 0, 0))):
            m.write16(PROPS + idx * 0x20 + 0x10 + i * 2, value)
        m.write32(0x800EC1E0 + 0x24, cfg.get("player_x", 0))
        m.write32(0x800EC1E0 + 0x28, cfg.get("player_y", 0))
        m.write32(0x800EC1E0 + 0x2C, cfg.get("player_z", 0))
        if name == "Gimmick_ProxTrigger_Update" and "distance" in cfg:
            collider = COLLIDERS + idx * 0x38
            m.write16(collider + 0x00, 0)
            m.write16(collider + 0x02, 0)
            m.write16(collider + 0x04, 0)
            m.write32(0x800EC204, cfg["distance"])
            m.write32(0x800EC208, 0)
            m.write32(0x800EC20C, 0)
        for i, value in enumerate(cfg.get("box", (0, 0, 0, 64, 64, 64))):
            m.write16(BOX + i * 2, value)
        for i in range(16):
            m.write32(DEST + i * 4, rng.getrandbits(32))

        if name in ("GimmickPlayerInBox",):
            args = (cfg.get("kind", 4), cfg.get("box_ptr", BOX), cfg.get("extent", 64))
        elif name == "GimmickMoveColliderTo":
            source = cfg.get("source", COLLIDERS + idx * 0x38)
            destination = cfg.get("destination", DEST)
            args = (cfg.get("collider_index", idx), source, destination)
        elif name == "GimmickMoveCollider":
            args = (cfg.get("collider_index", idx), cfg.get("dx", 32),
                    cfg.get("dy", 0), cfg.get("dz", -16))
        elif name == "GimmickStateTablePtr":
            args = ()
        else:
            args = (g,)
        return args

    def fail(self, name, label, problems, intentional=False):
        if intentional:
            return False
        self.failures += 1
        print(f"FAIL {name} [{label}]")
        for problem in problems:
            print("   ", problem)
        return False

    def run(self, name, cfg=None, *, seed=94423, intentional=False, count=True):
        if (intentional or self.mutation_mode) and self.mutation_caught:
            return False
        self.m.restore(self.base)
        args = self.setup(self.m, name, cfg, seed)
        initial = self.m.snapshot()
        self.side.sync(self.m)
        mmodel = HookModel(self.m, cfg)
        cmodel = HookModel(self.side, cfg)
        self.side.model = cmodel
        m_hooks = {address: (lambda cpu, regs, n=hook: mmodel.hook(n, regs[4:8]))
                   for hook, address, _ in HOOKS}
        mmodel.sound_idle = self.side.sound_idle = cfg.get("sound_idle", 0) if isinstance(cfg, dict) else 0
        m_hooks[0x800A1398] = lambda cpu, regs: mmodel.record("wait_idle") or mmodel.sound_idle
        m_hooks[0x8009D79C] = lambda cpu, regs: mmodel.record("wait_loop", regs[4]) or 0
        def printf_error(cpu, regs):
            fmt = regs[4]
            argc = 1 if fmt == 0x800A627C else 3
            values = tuple(signed32(value) for value in regs[5:5 + argc])
            mmodel.record("printf_error", fmt, argc, *values)
            return 0
        m_hooks[0x800A3720] = printf_error
        self.m.hook_log.clear()
        self.m.gte_reset()
        original_write = self.m.write
        adsr_values = {}
        def hardware_write(address, size, value):
            if 0x1F801C00 <= address < 0x1F801E00:
                offset = address - 0x1F801C00
                if size != 2:
                    raise AssertionError(f"unexpected SPU write size {size} at {address:08X}")
                value &= 0xFFFF
                if offset in (0x18C, 0x18E):
                    mmodel.record("voice_key_off", (offset - 0x18C) // 2, value)
                elif offset % 16 == 8:
                    adsr_values[offset // 16] = value
                elif offset % 16 == 10:
                    mmodel.record("set_voice_adsr", offset // 16,
                                  adsr_values.pop(offset // 16), value)
                else:
                    raise AssertionError(f"unexpected SPU write {address:08X}")
                return
            original_write(address, size, value)
        self.m.write = hardware_write
        mips_trap = None
        try:
            mret = self.m.call(FUNCS[name][0], args, hooks=m_hooks)[2]
        except mr.MipsError as e:
            expected_pc = cfg.get("expected_trap_pc") if isinstance(cfg, dict) else None
            if expected_pc is None or f"{expected_pc:08X}" not in str(e):
                return self.fail(name, str(cfg), ["MIPS harness error: " + str(e)], intentional)
            mips_trap = expected_pc
            mret = None
        finally:
            self.m.write = original_write
        cret = self.side.call(name, args)
        if count:
            self.counts[name] = self.counts.get(name, 0) + 1
        problems = []
        c_failed = bool(self.side.lib.ae_ram_failed(self.side.ram))
        c_error = self.side.lib.ae_ram_error(self.side.ram).decode() if c_failed else ""
        if mips_trap is not None:
            expected_error = cfg.get("expected_c_error", "")
            if not c_failed or expected_error not in c_error:
                problems.append(f"expected C error containing {expected_error!r}, got {c_error!r}")
        elif c_failed:
            problems.append("C error: " + c_error)
        if mret is not None and (mret & 0xFFFFFFFF) != (int(cret) & 0xFFFFFFFF):
            problems.append(f"return MIPS {mret & 0xFFFFFFFF:08X} C {int(cret) & 0xFFFFFFFF:08X}")
        if mmodel.log != cmodel.log:
            problems.append(f"hook log differs: MIPS {mmodel.log}; C {cmodel.log}")
        mram = bytes(self.m.ram[:STACK_LO])
        cram = bytes(self.side.main[:STACK_LO])
        if mram != cram:
            diff = [i for i, (a, b) in enumerate(zip(mram, cram)) if a != b]
            problems.append("RAM differs at " + ", ".join(
                f"{0x80000000 + i:08X}(MIPS {mram[i]:02X}, C {cram[i]:02X})" for i in diff[:8]))
        if bytes(self.m.scratch) != bytes(self.side.scr):
            diff = [i for i, (a, b) in enumerate(zip(self.m.scratch, self.side.scr)) if a != b]
            problems.append("scratchpad differs at " + ", ".join(
                f"{mr.SCRATCH_BASE + i:08X}" for i in diff[:8]))
        okay = not problems
        if problems:
            if intentional or self.mutation_mode:
                self.mutation_caught = True
            self.fail(name, str(cfg), problems, intentional or self.mutation_mode)
        return okay

    def scenarios(self, selected=None, scale=1, seed=94423):
        cases = {
            "Gimmick_Init_MonkeyQuota": [
                {"subtype": 0, "param_a": -1, "quota_met": 0, "quota_count": 3, "link": 2, "target_type": 3},
                {"subtype": 0, "param_a": -1, "quota_met": 1, "quota_count": 7, "link": 2, "target_type": 3},
                {"subtype": 0, "param_a": 4, "quota_met": 0, "quota_count": 3, "link": 2, "target_type": 3},
                {"subtype": 1, "param_a": 3, "quota_count": 4, "link": 2, "target_type": 3},
            ],
            "Gimmick_MonkeyQuota_Update": [
                {"timer": 0, "subtype": 0, "param_a": -1, "quota_met": 0, "quota_count": 2, "link": 2, "target_type": 3},
                {"timer": 0, "subtype": 0, "param_a": -1, "quota_met": 1, "quota_count": 5, "link": 2, "target_type": 3},
                {"timer": 0, "subtype": 0, "param_a": 3, "quota_count": 2, "link": 2, "target_type": 3},
                {"timer": 0, "subtype": 0, "param_a": 3, "quota_count": 4, "link": 2, "target_type": 3},
                {"timer": 0, "subtype": 1, "param_a": 5, "quota_count": 5, "link": 2, "target_type": 3},
                {"timer": 1, "subtype": 1, "param_a": 0},
                {"timer": 0, "subtype": 0, "param_a": 0, "quota_met": 0, "quota_count": 0,
                 "link": 2, "target_type": 3},
            ],
            "Gimmick_NopInit": [None] * 8,
            "Gimmick_NopUpdate": [None] * 8,
            "Gimmick_Init_Closed": [{"flags": v} for v in (0, 1, 0xFFFF, 0x8123, 0xFFF0, 0x000F)],
            "Gimmick_Init_ZoneMask": [
                {"subtype": 0, "param_a": 1, "flags": 0xFFFF},
                {"subtype": 7, "param_a": -3, "flags": 0x8123},
            ],
            "Gimmick_PlayerInCylinder_Update": [
                {"flags": 0, "collider_x": 0, "collider_y": 100, "collider_z": 0,
                 "half_x": 100, "half_y": 100, "player_x": 0, "player_y": 50, "player_z": 0},
                {"flags": 0x8000, "collider_x": 0, "collider_y": 100, "collider_z": 0,
                 "half_x": 100, "half_y": 100, "player_x": 0, "player_y": 50, "player_z": 0},
                {"flags": 0, "collider_x": 0, "collider_y": 100, "collider_z": 0,
                 "half_x": 100, "half_y": 100, "player_x": 110, "player_y": 50, "player_z": 0},
                {"flags": 0, "collider_x": 0, "collider_y": 100, "collider_z": 0,
                 "half_x": 100, "half_y": 100, "player_x": 0, "player_y": 200, "player_z": 0},
            ],
            "Gimmick_ProxTrigger_Init": [
                {"time_attack": 1, "subtype": 0, "area": 1},
                {"time_attack": 0, "subtype": 0, "area": 92, "param_a": 5, "link": 2},
                {"time_attack": 0, "subtype": 1, "area": 10, "param_a": 5, "link": 2},
            ],
            "Gimmick_ProxTrigger_Update": [
                {"state": 0, "distance": 2, "param_e": 8, "param_10": 0, "param_14": 7},
                {"state": 0, "distance": 2, "param_e": 8, "param_10": 1, "param_14": 7},
                {"state": 1, "distance": 2, "param_e": 8, "area": 92, "param_14": 7},
                {"state": 1, "distance": 2, "param_e": 8, "area": 10, "param_14": 7},
                {"state": 2, "distance": 9, "param_e": 8, "param_14": 7},
                {"state": 3, "distance": 9, "param_e": 8, "param_14": 7},
                {"state": 3, "distance": 2, "param_e": 8, "param_14": 7, "touched_by": 4},
                {"state": 0, "distance": 2, "param_e": 8, "area": 23},
                {"state": 0, "distance": 2, "param_e": 8, "area": 92},
                {"state": -1, "distance": 2, "param_e": 8},
                {"state": 4, "distance": 2, "param_e": 8},
                {"state": 1, "distance": 2, "param_e": 8, "area": 60,
                 "param_14": 7, "touched_by": 0xFFFF},
                {"state": 1, "distance": 2, "param_e": 8, "area": 10,
                 "param_14": 7, "param_10": 0, "touched_by": 4},
                {"state": 3, "distance": 2, "param_e": 8, "param_14": 7,
                 "touched_by": 4, "param_10": 0},
            ],
            "Gimmick_Breakable_Init": [
                {"param_a": 0, "state": 2, "timer": 8},
                {"param_a": 1, "state": 2, "prop_index": 3},
                {"param_a": 1, "state": 2, "prop_index": -1},
            ],
            "Gimmick_Breakable_Update": [
                {"state": 2, "touched_by": 1},
                {"state": 0, "touched_by": -1},
                {"state": 0, "touched_by": 1, "subtype": 0, "param_a": 0, "link": 4},
                {"state": 0, "touched_by": 0x0800, "subtype": 1, "param_a": 0, "link": 1,
                 "timer": 8, "area": 29},
                {"state": 0, "touched_by": 0x0800, "subtype": 1, "param_a": 0, "link": 1,
                 "timer": 3, "param_e": -1},
                {"state": 0, "touched_by": 1, "subtype": 1, "param_a": 0, "link": 1,
                 "timer": 2, "prop_index": -1},
                {"state": 0, "touched_by": 1, "subtype": 0, "param_a": 0, "link": 1,
                 "timer": 3, "param_e": 0, "prop_index": 1},
            ],
            "Gimmick_Init_Closed2": [
                {"subtype": 0, "prop_index": 0}, {"subtype": 0, "prop_index": 3},
                {"subtype": 0, "prop_index": -1}, {"subtype": 1, "prop_index": 0},
                {"subtype": -1, "prop_index": 2},
            ],
            "Gimmick_NopUpdate2": [None] * 8,
            "Gimmick_Init_PropHidden": [
                {"prop_index": 0, "flags": 0}, {"prop_index": 2, "flags": 0x2000},
                {"prop_index": -1, "flags": 0xFFFF}, {"prop_index": 7, "flags": 0x8000},
            ],
            "Gimmick_Resize_Update": [
                {"state": 0, "breakable_count": 1},
                {"state": 1, "breakable_count": 1, "param_a": 4, "level": 6},
                {"state": 1, "breakable_count": 4, "param_a": 4, "area": 0x65},
                {"state": 1, "breakable_count": 4, "param_a": 4, "area": 1},
                {"state": 1, "breakable_count": 1, "link": 1, "level": 0, "area": 1},
            ],
            "Gimmick_Init_Closed3": [
                {"prop_index": 0, "flags": 0}, {"prop_index": 2, "flags": 0xFFFF},
                {"prop_index": -1, "flags": 0xD5A3}, {"prop_index": 7, "flags": 0x000F},
            ],
            "Gimmick_Slider_Update": [
                {"state": 0}, {"state": 1, "timer": 0, "param_a": 100},
                {"state": 1, "timer": 4000, "param_a": 500},
                {"state": 1, "timer": 4095, "param_a": 1},
                {"state": 2, "timer": 10, "param_e": 20},
                {"state": 2, "timer": 100, "param_e": 10},
                {"state": 3},
            ],
            "Gimmick_SliderQuiet_Update": [
                {"state": 0}, {"state": 1, "timer": 0, "param_a": 100},
                {"state": 1, "timer": 4000, "param_a": 500},
                {"state": 1, "timer": 4095, "param_a": 1},
                {"state": 2, "timer": 10, "param_e": 20},
                {"state": 2, "timer": 100, "param_e": 10},
                {"state": 3},
            ],
            "Gimmick_PlayerBox_Update": [
                {"subtype": 4, "gimmick_box": (0, 0, 0, 64, 64, 64),
                 "player_x": 0, "player_y": -10, "player_z": 0},
                {"subtype": 5, "gimmick_box": (0, 0, 0, 64, 64, 64),
                 "player_x": -20, "player_y": -10, "player_z": 0},
                {"subtype": 6, "gimmick_box": (0, 0, 0, 64, 64, 64),
                 "player_x": 0, "player_y": -10, "player_z": 20},
            ],
            "GimmickPlayerInBox": [
                {"kind": k, "box": (0, 0, 0, 64, 64, 64), "extent": 64,
                 "player_x": 0, "player_y": -10, "player_z": 0}
                for k in (0, 1, 2, 3, 4, 5, 6, 7)
            ] + [
                {"kind": 4, "box": (0, 0, 0, 64, 64, 64), "extent": 64,
                 "player_x": 64, "player_y": -10, "player_z": 0},
                {"kind": 6, "box": (0, 0, 0, 64, 64, 64), "extent": 64,
                 "player_x": 0, "player_y": -10, "player_z": -20},
            ],
            "Gimmick_Init_RecordOrigin": [
                {"param_a": 0xFFFF, "record_origin_result": 1},
                {"param_a": 0x0100, "record_origin_result": 0},
                {"param_a": 0x0203, "record_origin_result": -1},
            ],
            "Gimmick_LinkedLift_Update": [
                {"state": 0, "param_a": 0x0302, "mesh_counts": {2: 10, 3: 8},
                 "param_16": 2, "param_12": 1, "param_e": 4, "param_14": 2,
                 "timer": 0, "sound_handle": -1},
                {"state": 0, "param_a": 0x0302, "mesh_counts": {2: 4, 3: 2},
                 "param_16": 5, "param_12": 1, "param_e": 2, "param_14": 2,
                 "timer": 0, "sound_handle": 2},
                {"state": 1, "param_a": 0x0302, "mesh_counts": {2: 8, 3: 6},
                 "param_16": 8, "param_12": 1, "param_e": 2, "param_14": 3,
                 "timer": 2, "sound_handle": 3},
                {"state": 1, "param_a": 0x0302, "mesh_counts": {2: 8, 3: 6},
                 "param_16": 8, "param_12": 1, "param_e": 2, "param_14": 1,
                 "timer": 2, "sound_handle": -1},
                {"state": 2, "param_a": 0x0302, "mesh_counts": {2: 2, 3: 2},
                 "param_16": 1, "param_12": 1, "param_e": 1, "param_14": 1,
                 "timer": 0, "sound_handle": 1},
                {"state": 3, "param_a": 0x0302, "sound_handle": 1},
                {"state": -1, "param_a": 0x0302, "sound_handle": 1},
                {"state": 2, "param_a": 0x0302, "mesh_counts": {2: -4, 3: -4},
                 "param_16": -1, "param_12": 1, "param_e": 1, "param_14": 1,
                 "timer": 0, "sound_handle": 1},
            ],
            "Gimmick_Init_PropTilt": [
                {"link": -1}, {"link": 2, "subtype": 0, "param_e": -15},
                {"link": 2, "subtype": 1, "param_e": 25},
            ],
            "Gimmick_Oscillate_Update": [
                {"state": 0, "timer": 1, "param_a": 4, "subtype": 0, "param_e": 500,
                 "link": 2, "param_10": 64, "param_12": 40},
                {"state": 0, "timer": 4, "param_a": 4, "subtype": 1, "param_e": -300,
                 "link": 2, "param_10": 48, "param_12": 50},
                {"state": 1, "timer": 2, "param_a": 4, "subtype": 0, "param_e": 600,
                 "link": 2, "param_10": 64, "param_12": 25},
                {"state": 1, "timer": 4, "param_a": 4, "subtype": 1, "param_e": 500,
                 "link": 2, "param_10": 48, "param_12": 30},
                {"state": 2, "timer": 0, "param_a": 4, "subtype": 0, "param_e": 100,
                 "link": 2, "param_10": 32, "param_12": 10},
                {"state": 0, "timer": 2, "param_a": 4, "link": -1},
                {"state": 1, "timer": 2, "param_a": 4, "subtype": 0, "param_e": 600,
                 "link": 2, "param_10": 64, "param_12": 25, "area": 60,
                 "player_y": -3000},
                {"state": 1, "timer": 2, "param_a": 4, "subtype": 0, "param_e": 600,
                 "link": 2, "param_10": 64, "param_12": 25, "area": 23},
                {"state": 1, "timer": 1, "param_a": 4, "subtype": 0, "param_e": 600,
                 "link": 2, "param_10": 64, "param_12": 25, "area": 60,
                 "player_y": -3000},
                {"state": 1, "timer": 1, "param_a": 4, "subtype": 0, "param_e": 600,
                 "link": 2, "param_10": 64, "param_12": 25, "area": 23},
                {"state": 0, "timer": -1, "param_a": 0, "link": -1,
                 "expected_trap_pc": 0x80092BA8,
                 "expected_c_error": "Gimmick_Oscillate_Update division by zero"},
                {"state": 1, "timer": -1, "param_a": 0, "link": -1,
                 "expected_trap_pc": 0x80092C40,
                 "expected_c_error": "Gimmick_Oscillate_Update division by zero"},
            ],
            "Gimmick_Init_PropMeshSet": [
                {"param_a": -1, "link": 0, "param_e": 0},
                {"param_a": 0, "link": 0, "param_e": 1, "subtype": 4,
                 "mesh_counts": {0: 3}},
                {"param_a": 1, "link": 2, "param_e": 2, "subtype": 1,
                 "mesh_counts": {1: 1}},
            ],
            "Gimmick_Init_CounterGate": [
                {"param_14": 1, "count": 0, "param_a": 0, "linked_index": 2},
                {"param_14": 0, "count": 2, "param_a": 3, "linked_index": 2},
                {"param_14": 0, "count": 3, "param_a": 3, "linked_index": 2},
                {"param_14": 0, "count": 4, "param_a": 3, "linked_index": 2},
                {"param_14": -1, "count": -32768, "param_a": 32767, "linked_index": 2},
            ],
            "Gimmick_Counter_EventCam_Update": [
                {"state": 0, "subtype": 2, "param_a": 3, "event_busy": 8,
                 "mesh_counts": {2: 4}},
                {"state": 0, "subtype": 2, "param_a": 4, "event_busy": 8,
                 "mesh_counts": {2: 4}},
                {"state": 0, "subtype": 2, "param_a": 5, "event_busy": 8,
                 "mesh_counts": {2: 4}},
                {"state": 0, "subtype": 2, "param_a": 3, "event_busy": 0,
                 "mesh_counts": {2: 4}},
                {"state": 1, "subtype": 2, "link": 4, "event_busy": 0},
                {"state": 1, "subtype": 2, "link": 4, "event_busy": 8},
                {"state": 0, "subtype": -1, "param_a": 0},
            ],
            "Gimmick_PlayerBlast_Update": [
                {"subtype": 0, "param_a": 0, "link": 0, "param_e": 100,
                 "param_10": 0, "distance": 50, "player_x": 10, "player_y": 20, "player_z": 30},
                {"subtype": 0, "param_a": 0, "link": 0, "param_e": 50,
                 "param_10": 0, "distance": 50, "player_x": 10, "player_y": 20, "player_z": 30},
                {"subtype": 0, "param_a": 0, "link": 0, "param_e": 100,
                 "param_10": 1, "distance": 50},
                {"subtype": 0, "param_a": 0, "link": 0, "param_e": 100,
                 "param_10": 2, "distance": 50},
                {"subtype": 0, "param_a": 0, "link": 0, "param_e": 10,
                 "distance": 11},
            ],
            "Gimmick_Init_Disabled": [{"flags": v} for v in (0, 0xFFFF, 0x7FFF, 0x1234)],
            "Gimmick_Grab_Update": [
                {"state": 0, "grab_success": 0}, {"state": 0, "grab_success": 1,
                 "grab_point": (10, 20, 30)},
                {"state": 1, "timer": 1}, {"state": 1, "timer": 0}, {"state": 3},
            ],
            "Gimmick_Init_PropFlag2000": [
                {"link": 0, "prop_index": 0}, {"link": -1, "prop_index": 1},
                {"link": 1, "prop_index": 0}, {"link": 2, "prop_index": -1},
                {"link": 3, "prop_index": 4},
                {"link": 0, "prop_index": 0, "mesh_header": 3},
            ],
            "Gimmick_Bounce_Update": [
                {"state": 0, "link": 1, "param_12": 1, "player_collider": 1, "subtype": 100,
                 "param_a": 0, "area": 7},
                {"state": 0, "link": 1, "param_12": 0, "player_collider": 1,
                 "subtype": 100, "param_a": 0, "area": 81},
                {"state": 0, "link": 1, "param_12": 0, "player_collider": 1,
                 "subtype": 100, "param_a": 0, "area": 84},
                {"state": 0, "link": 0, "param_12": 0, "player_collider": 1,
                 "subtype": 100, "param_a": 0, "area": 50},
                {"state": 0, "link": 0, "param_12": 0, "player_collider": 1,
                 "subtype": 100, "param_a": 0, "area": 71},
                {"state": 0, "link": -1, "player_collider": -1, "collider_x": 0,
                 "collider_y": 0, "collider_z": 0, "half_x": 100, "half_y": 100, "player_x": 0,
                 "player_y": -10, "player_z": 0, "param_12": 0, "player_rotate_x": 100,
                 "player_rotate_z": 100, "subtype": 100, "area": 25},
                {"state": 0, "link": -1, "player_collider": -1, "collider_x": 0,
                 "collider_y": 0, "collider_z": 0, "half_y": 100, "player_x": 1000,
                 "player_y": 10, "player_z": 0, "param_12": 0},
                {"state": 0, "link": -1, "param_12": 1, "player_collider": -1,
                 "player_x": 0, "player_y": -10, "half_x": 100, "half_y": 100},
                {"state": -1},
                {"state": 1, "subtype": 512, "param_a": 0, "prop_index": 1, "mesh_type": 3},
                {"state": 1, "subtype": 100, "param_a": 0, "prop_index": 1, "mesh_type": 3},
                {"state": 2, "subtype": 512, "param_a": 1024, "prop_index": 1, "mesh_type": 2},
                {"state": 2, "subtype": 512, "param_a": 3072, "prop_index": 1, "mesh_type": 2},
                {"state": 2, "subtype": 100, "param_a": 1024, "prop_index": 1, "mesh_type": 2},
                {"state": 1, "subtype": 512, "param_a": 3072, "prop_index": 1, "mesh_type": 3},
                {"state": 3, "subtype": 512, "param_a": 3840, "param_e": 256,
                 "param_10": 1, "prop_index": 1, "mesh_type": 3},
                {"state": 1, "subtype": 512, "param_a": 4096, "prop_index": 1, "mesh_type": 3},
                {"state": 4},
            ],
            "Gimmick_Init_Hidden": [
                {"subtype": 0, "prop_index": 0}, {"subtype": 3, "prop_index": 1},
                {"subtype": 4, "prop_index": 2}, {"subtype": 7, "prop_index": -1},
                {"subtype": -1, "prop_index": 3},
                {"subtype": 3, "prop_index": 1, "flags": 0},
            ],
            "Gimmick_Rotate_Update": [
                {"state": 0, "subtype": 0, "rotate_probe": 1, "link": 100,
                 "param_a": 4, "param_e": 64, "player_rotate_x": 0,
                 "player_rotate_z": 80, "area": 21},
                {"state": 0, "subtype": 1, "rotate_probe": 1, "link": 100,
                 "param_a": 4, "param_e": 64, "player_rotate_x": 80,
                 "player_rotate_z": 0, "area": 84},
                {"state": 0, "subtype": 2, "rotate_probe": 1, "link": 100,
                 "param_a": 4, "param_e": 64, "player_rotate_x": 80,
                 "player_rotate_z": 100},
                {"state": 0, "subtype": 3, "rotate_probe": 1, "link": 100,
                 "param_a": 4, "param_e": 64, "player_rotate_x": 80,
                 "player_rotate_z": 100},
                {"state": 0, "subtype": 4, "rotate_probe": 1, "link": 1000,
                 "param_a": 4, "param_e": 64, "player_x": 100,
                 "player_rotate_x": 80, "player_rotate_z": 100},
                {"state": 0, "subtype": 5, "rotate_probe": 1, "link": 1000,
                 "param_a": 4, "param_e": 64, "player_x": 100,
                 "player_rotate_x": 80, "player_rotate_z": 100},
                {"state": 0, "subtype": 6, "rotate_probe": 0, "link": 1000},
                {"state": 0, "subtype": 7, "rotate_probe": 1, "link": 1000,
                 "param_a": 4, "param_e": 64},
                {"state": 0, "subtype": 8, "rotate_probe": 1, "link": 1000},
                {"state": 0, "subtype": 0, "rotate_probe": 1, "link": 1000,
                 "param_a": 4, "player_rotate_x": 80, "player_rotate_z": 0,
                 "player_rotate_z32": 0, "player_rotate_x32": 80},
                {"state": 0, "subtype": 4, "rotate_probe": 1, "link": 1000,
                 "param_a": 4, "param_e": 64, "area": 84,
                 "player_x": -200, "collider_x": 0, "collider_z": 0},
                {"state": 0, "subtype": 4, "rotate_probe": 1, "link": 1000,
                 "param_a": 4, "param_e": 64, "area": 84,
                 "player_x": 100, "collider_x": 0, "collider_z": 0},
                {"state": 0, "subtype": 4, "rotate_probe": 1, "link": 1000,
                 "param_a": 4, "param_e": 64, "area": 84,
                 "player_rotate_x32": 0, "player_rotate_z32": 0,
                 "collider_x": 0, "collider_z": 0},
                {"state": 0, "subtype": 4, "rotate_probe": 1, "link": 1000,
                 "param_a": 4, "param_e": 64, "area": 84, "player_x": 100,
                 "player_rotate_x32": 80, "player_rotate_z32": 0,
                 "collider_x": 0, "collider_z": 0},
                {"state": 0, "subtype": 5, "rotate_probe": 1, "link": 1000,
                 "param_a": 4, "param_e": 64, "player_x": 100,
                 "collider_x": 0, "collider_z": 0},
                {"state": 0, "subtype": 5, "rotate_probe": 1, "link": 1000,
                 "param_a": 4, "param_e": 64, "player_rotate_x32": 0,
                 "player_rotate_z32": 0, "collider_x": 0, "collider_z": 0},
                {"state": 0, "subtype": 5, "rotate_probe": 1, "link": 1000,
                 "param_a": 4, "param_e": 64, "player_x": 100,
                 "player_rotate_x32": 80, "player_rotate_z32": 0,
                 "collider_x": 0, "collider_z": 0},
                {"state": 0, "subtype": 5, "rotate_probe": 1, "link": 1000,
                 "param_a": 4, "param_e": 64, "player_x": -100,
                 "collider_x": 0, "collider_z": 0},
                {"state": 1, "timer": 0, "param_a": 8, "param_10": 4, "param_e": 32},
                {"state": 1, "timer": 7, "param_a": 8, "param_10": 4, "param_e": 32},
                {"state": 2, "timer": 2, "param_a": 8, "param_10": 4, "param_e": 32},
                {"state": 2, "timer": 0, "param_a": 8, "param_10": 4, "param_e": 32},
                {"state": 1, "timer": 0, "param_a": 0, "param_10": 4,
                 "prop_index": 1, "expected_trap_pc": 0x800942E0,
                 "expected_c_error": "Gimmick_Rotate_Update division by zero"},
                {"state": 2, "timer": 18, "param_a": -1, "param_10": 32767,
                 "prop_index": 1, "expected_trap_pc": 0x800942F8,
                 "expected_c_error": "Gimmick_Rotate_Update division overflow"},
                {"state": -1},
                {"state": 3},
            ],
            "Gimmick_Falling_Update": [
                {"state": 0, "timer": 0, "param_a": 1, "falling_probe": 0,
                 "falling_collision": 0, "wall_state": -1},
                {"state": 0, "timer": 0, "param_a": 1, "falling_probe": 1,
                 "falling_collision": 0, "wall_state": -1},
                {"state": 0, "timer": 0, "param_a": 1, "falling_probe": 0,
                 "falling_collision": 1, "wall_state": 1},
                {"state": 0, "timer": 1, "param_a": 1, "falling_probe": 0,
                 "wall_state": -1},
                {"state": 1, "link": 3, "param_a": -1, "breakable_count": 0,
                 "prop_position": (0, 0, 0)},
                {"state": 1, "link": 1, "param_a": 2, "breakable_count": 5,
                 "param_12": 2, "wall_state": -1, "falling_probe": 1},
                {"state": 2},
            ],
            "GimmickMoveColliderTo": [
                {"collider_index": -1},
                {"collider_index": 1, "source": COLLIDERS + 0x38,
                 "destination": DEST, "collider_x": 20, "collider_y": 30,
                 "collider_z": 40},
            ],
            "GimmickMoveCollider": [
                {"collider_index": -1},
                {"collider_index": 1, "dx": 12, "dy": -5, "dz": 20,
                 "box_contains": 0, "prop_index": -1},
                {"collider_index": 1, "dx": 12, "dy": -5, "dz": 20,
                 "box_contains": 1, "player_x": 100, "rc_car": 0x801E5000,
                 "gimmick_type": 11, "child_index": 2, "target_type": 3,
                 "target_index": 2},
            ],
            "Gimmick_NopUpdate3": [None] * 8,
            "Gimmick_NopUpdate4": [None] * 8,
            "GimmickStateTablePtr": [None],
        }

        def run(name, config=None, variation=0):
            if selected and name not in selected:
                return
            self.run(name, config, seed=seed + variation)

        for name in FUNCS:
            if selected is not None and name not in selected:
                continue
            variants = cases.get(name, [None])
            count = max(len(variants), round(len(variants) * scale))
            for i in range(count):
                config = variants[i % len(variants)]
                if isinstance(config, dict) and i >= len(variants):
                    config = dict(config)
                run(name, config, i)

    def finish(self, selected=None, default_run=False, show_coverage=False):
        summary = None
        if self.m.coverage is not None:
            items = [(name, spec[0], spec[1], True) for name, spec in self.funcs.items()
                     if selected is None or name in selected]
            summary = reg.coverage_summary(items, self.m.coverage)
            if show_coverage:
                reg.print_coverage(summary)
        if default_run:
            reg.write_results(self.counts, summary, not self.failures,
                              function_names=[name for name in self.funcs if name in self.counts])
        return summary


def mutation_sources():
    # Swing-only mutants were removed with the duplicate implementations.
    # No batch B call sites exist to retarget them; retain all remaining mutants.
    specs = []
    def add(function, label, old, new):
        specs.append((function, label, old, new))
    for name in ("Gimmick_NopInit", "Gimmick_NopUpdate", "Gimmick_NopUpdate2",
                 "Gimmick_NopUpdate3", "Gimmick_NopUpdate4"):
        add(name, name + " nonzero result", "return 0;", "return 1;")
        add(name, name + " negative result", "return 0;", "return -1;")
    add("Gimmick_Init_Closed", "Closed state", "AE_GIMMICK_STATE, 0);", "AE_GIMMICK_STATE, 1);")
    add("Gimmick_Init_Closed", "Closed collider low bits", "set_collider_ready(r, collider);", "(void)collider;")
    add("Gimmick_Init_Closed2", "Closed2 subtype branch", "AE_GIMMICK_SUBTYPE) == 0", "AE_GIMMICK_SUBTYPE) != 0")
    add("Gimmick_Init_Closed2", "Closed2 prop zone mask", "AE_AREA_PROP_ZONE_MASK, 0);", "AE_AREA_PROP_ZONE_MASK, 1);")
    add("Gimmick_Init_PropHidden", "PropHidden zone mask", "AE_AREA_PROP_ZONE_MASK, 0);", "AE_AREA_PROP_ZONE_MASK, 1);")
    add("Gimmick_Init_PropHidden", "PropHidden collider state", "set_collider_ready(r, collider);", "(void)collider;")
    add("Gimmick_Init_Closed3", "Closed3 saved height", "ae_rd16(r, collider + AE_COLLIDER_HALF_Y));", "ae_rd16(r, collider + AE_COLLIDER_HALF_Z));")
    add("Gimmick_Init_Closed3", "Closed3 helper arguments", "FUN_80091AF0, collider, gimmick", "FUN_80091AF0, gimmick, collider")
    add("Gimmick_Init_Disabled", "Disabled flag", "flags | AE_COLLIDER_DISABLED", "flags & ~AE_COLLIDER_DISABLED")
    add("Gimmick_Init_Disabled", "Disabled state", "AE_GIMMICK_STATE, 0);", "AE_GIMMICK_STATE, 1);")
    add("Gimmick_Init_PropFlag2000", "PropFlag2000 link guard", "AE_GIMMICK_LINK) > 0", "AE_GIMMICK_LINK) >= 0")
    add("Gimmick_Init_PropFlag2000", "PropFlag2000 mesh flag", "| AE_PROP_MESH_HIDDEN", "| 0x1000u")
    add("Gimmick_Init_Hidden", "Hidden subtype guard", "AE_GIMMICK_SUBTYPE) >= 4", "AE_GIMMICK_SUBTYPE) >= 3")
    add("Gimmick_Init_Hidden", "Hidden mesh flag", "| AE_PROP_MESH_HIDDEN", "| 0x1000u")
    add("Gimmick_Init_CounterGate", "CounterGate comparison", "count < target", "count <= target")
    add("Gimmick_Init_CounterGate", "CounterGate explicit state", "AE_GIMMICK_PARAM_14) != 0", "AE_GIMMICK_PARAM_14) > 0")
    add("GimmickStateTablePtr", "state table address", "return AE_G_GIMMICK_STATE_TABLE;", "return AE_G_GIMMICK_STATE_TABLE + 4u;")
    add("GimmickStateTablePtr", "state table nonnull", "return AE_G_GIMMICK_STATE_TABLE;", "return 0;")
    add("Gimmick_Init_MonkeyQuota", "MonkeyQuota init state", "AE_GIMMICK_STATE, 0);", "AE_GIMMICK_STATE, 1);")
    add("Gimmick_Init_MonkeyQuota", "MonkeyQuota init timer", "AE_GIMMICK_TIMER, 0);", "AE_GIMMICK_TIMER, 1);")
    add("Gimmick_MonkeyQuota_Update", "MonkeyQuota update result slot", "AE_B_STACK_QUOTA_RESULT_UPDATE", "gimmick + AE_GIMMICK_TIMER")
    add("Gimmick_MonkeyQuota_Update", "MonkeyQuota update return", "return update_monkey_quota(r, h, gimmick, AE_B_STACK_QUOTA_RESULT_UPDATE);", "return update_monkey_quota(r, h, gimmick, AE_B_STACK_QUOTA_RESULT_UPDATE) + 1;")
    add("Gimmick_Init_ZoneMask", "ZoneMask collider bits", "AE_COLLIDER_ZONE_MASK, 0x1111", "AE_COLLIDER_ZONE_MASK, 0x1110")
    add("Gimmick_Init_ZoneMask", "ZoneMask state", "AE_GIMMICK_STATE, 0);", "AE_GIMMICK_STATE, 1);")
    add("Gimmick_PlayerInCylinder_Update", "Cylinder hook arguments", "FUN_8009B3B0, 0, 0, 0, 0", "FUN_8009B3B0, 1, 0, 0, 0")
    add("Gimmick_PlayerInCylinder_Update", "Cylinder disabled gate", "flags & AE_COLLIDER_DISABLED", "flags & 0u")
    add("Gimmick_ProxTrigger_Init", "ProxTrigger radius", "AE_COLLIDER_HALF_X, 80", "AE_COLLIDER_HALF_X, 81")
    add("Gimmick_ProxTrigger_Init", "ProxTrigger final parameter", "AE_GIMMICK_PARAM_10, 0);", "AE_GIMMICK_PARAM_10, 1);")
    add("Gimmick_ProxTrigger_Update", "ProxTrigger state-1 parameter gate", "AE_GIMMICK_PARAM_10) != 0", "AE_GIMMICK_PARAM_10) == 0")
    add("Gimmick_ProxTrigger_Update", "ProxTrigger special state", "? 3u : 1u", "? 2u : 1u")
    add("Gimmick_Breakable_Init", "Breakable init guard", "AE_GIMMICK_PARAM_A) == 0", "AE_GIMMICK_PARAM_A) != 0")
    add("Gimmick_Breakable_Init", "Breakable prop zone", "AE_AREA_PROP_ZONE_MASK, 0);", "AE_AREA_PROP_ZONE_MASK, 1);")
    add("Gimmick_Breakable_Update", "Breakable hit threshold", "if (hits < ae_rds16(r, gimmick + AE_GIMMICK_LINK))", "if (hits <= ae_rds16(r, gimmick + AE_GIMMICK_LINK))")
    add("Gimmick_Breakable_Update", "Breakable toucher class", "!= 0x0800u", "!= 0x0900u")
    add("Gimmick_Resize_Update", "Resize state gate", "AE_GIMMICK_STATE) != 1", "AE_GIMMICK_STATE) != 2")
    add("Gimmick_Resize_Update", "Resize break threshold", "if (current < ae_rds16(r, gimmick + AE_GIMMICK_LINK)) return 0;", "if (current <= ae_rds16(r, gimmick + AE_GIMMICK_LINK)) return 0;")
    add("Gimmick_Slider_Update", "Slider upper clamp edge", "if (progress > 4096)", "if (progress >= 4096)")
    add("Gimmick_Slider_Update", "Slider up parameter", "gimmick + AE_GIMMICK_PARAM_A));", "gimmick + AE_GIMMICK_PARAM_12));")
    add("Gimmick_SliderQuiet_Update", "SliderQuiet upper clamp edge", "if (progress > 4096) ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 4096);", "if (progress >= 4096) ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 4095);")
    add("Gimmick_SliderQuiet_Update", "SliderQuiet up parameter", "gimmick + AE_GIMMICK_PARAM_A));", "gimmick + AE_GIMMICK_PARAM_12));")
    add("Gimmick_PlayerBox_Update", "PlayerBox extent lower", "128);", "64);")
    add("Gimmick_PlayerBox_Update", "PlayerBox extent upper", "128);", "129);")
    add("GimmickPlayerInBox", "PlayerBox horizontal edge", "player_x >= box_x + half_x", "player_x > box_x + half_x")
    add("GimmickPlayerInBox", "PlayerBox center direction", "else if (direction == 4)", "else if (direction == 5)")
    add("Gimmick_Init_RecordOrigin", "RecordOrigin low id guard", "if (low_id != 0xFFu)", "if (low_id == 0xFFu)")
    add("Gimmick_Init_RecordOrigin", "RecordOrigin high id guard", "if (high_id != 0xFFu)", "if (high_id == 0xFFu)")
    add("Gimmick_LinkedLift_Update", "LinkedLift state branch", "if (state == 0)", "if (state == 1)")
    add("Gimmick_LinkedLift_Update", "LinkedLift pan scale", "SoundEmitterPan, 127, sound_handle, 8192", "SoundEmitterPan, 127, sound_handle, 8193")
    add("Gimmick_Init_PropTilt", "PropTilt axis branch", "AE_GIMMICK_SUBTYPE) == 0", "AE_GIMMICK_SUBTYPE) != 0")
    add("Gimmick_Init_PropTilt", "PropTilt X sign", "(int16_t)(-amount / 2)", "(int16_t)(amount / 2)")
    add("Gimmick_Oscillate_Update", "Oscillate trig phase", "ae_sdk_rcos(r, phase)", "ae_sdk_rcos(r, phase + 256)")
    add("Gimmick_Oscillate_Update", "Oscillate state transition", "state == 0 ? 1 : 0", "state == 0 ? 0 : 1")
    add("Gimmick_Init_PropMeshSet", "PropMeshSet mesh limit", "i < mesh_count", "i <= mesh_count")
    add("Gimmick_Init_PropMeshSet", "PropMeshSet mesh index", "MeshInitFromAreaTable, prop, mesh_index", "MeshInitFromAreaTable, prop, mesh_index + 1")
    add("Gimmick_Counter_EventCam_Update", "EventCam required count edge", "count < required", "count <= required")
    add("Gimmick_Counter_EventCam_Update", "EventCam busy phase", "AE_G_EVENT_CAM_BUSY) != 8", "AE_G_EVENT_CAM_BUSY) == 8")
    add("Gimmick_PlayerBlast_Update", "PlayerBlast body radius", "AE_BODY_RADIUS, extent);", "AE_BODY_RADIUS, extent + 1);")
    add("Gimmick_PlayerBlast_Update", "PlayerBlast body flags", "AE_BODY_FLAGS, 0x202);", "AE_BODY_FLAGS, 0x203);")
    add("Gimmick_Grab_Update", "Grab response condition", "if (grabbed != 0)", "if (grabbed == 0)")
    add("Gimmick_Grab_Update", "Grab hold timer", "AE_GIMMICK_TIMER, 30);", "AE_GIMMICK_TIMER, 31);")
    add("Gimmick_Bounce_Update", "Bounce collider gate", "== collider_index", "!= collider_index")
    add("Gimmick_Bounce_Update", "Bounce area sound", "if (area == 7) sound = 181", "if (area == 7) sound = 180")
    add("Gimmick_Rotate_Update", "Rotate accepted gate", "if (accepted == 0) return 0;", "if (accepted != 0) return 0;")
    add("Gimmick_Rotate_Update", "Rotate area sound", "PlaySfx, 179, 0, 0, 0", "PlaySfx, 178, 0, 0, 0")
    add("Gimmick_Falling_Update", "Falling state guard", "if (state != 1) return 0;", "if (state == 1) return 0;")
    add("Gimmick_Falling_Update", "Falling start timer", "ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 1);\n        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);", "ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);\n        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);")
    add("GimmickMoveColliderTo", "MoveColliderTo negative index path", "if (index >= 0)", "if (index < 0)")
    add("GimmickMoveColliderTo", "MoveColliderTo source X", "destination + AE_VECTOR3_X, ae_rd16(r, source + AE_VECTOR3_X)", "destination + AE_VECTOR3_X, ae_rd16(r, source + AE_VECTOR3_Y)")
    add("GimmickMoveCollider", "MoveCollider RC-car gate", "if (rc_car != 0)", "if (rc_car == 0)")
    add("GimmickMoveCollider", "MoveCollider X displacement", "ae_wr32(r, AE_G_PLAYER_POS_X, player_x + dx);", "ae_wr32(r, AE_G_PLAYER_POS_X, player_x - dx);")
    return specs


def mutate_function(source, function, old, new):
    signature = re.search(rf"(?m)^(?:int32_t|uint32_t)\s+ae_{re.escape(function)}\(", source)
    if signature is None:
        raise RuntimeError(f"missing function {function}")
    start = signature.start()
    candidates = [p for token in ("\nint32_t ae_", "\nuint32_t ae_")
                  if (p := source.find(token, signature.end())) >= 0]
    end = min(candidates) if candidates else len(source)
    prefix, body, suffix = source[:start], source[start:end], source[end:]
    count = body.count(old)
    if count != 1:
        raise RuntimeError(f"mutation {function}: anchor matched {count} times: {old!r}")
    return prefix + body.replace(old, new, 1) + suffix


def run_mutations():
    original = (ROOT / "src/world/ae_gimmick_b.c").read_text(encoding="utf-8")
    variants = mutation_sources()
    mutations_dir = ROOT / "extract/mips"
    mutations_dir.mkdir(parents=True, exist_ok=True)
    caught, survivors = [], []
    with tempfile.TemporaryDirectory(prefix="gimmick-b-mutants-", dir=mutations_dir,
                                     ignore_cleanup_errors=True) as temp_name:
        temp = Path(temp_name)
        for index, (function, label, old, new) in enumerate(variants, 1):
            mutated = mutate_function(original, function, old, new)
            source = temp / f"mutant_{index:02d}.c"
            source.write_text(mutated, encoding="utf-8")
            harness = GimmickBHarness(source, with_coverage=False, out_dir=temp,
                                      tag=f"gimmick_b_mutant_{index:02d}")
            harness.m.coverage = None
            harness.mutation_mode = True
            harness.scenarios({function})
            okay = harness.mutation_caught
            if okay:
                caught.append(label)
            else:
                survivors.append(label)
            side = harness.side
            side.model = None
            side.callbacks.clear()
            side.hooks = CHooks()
            del harness, side
            gc.collect()
    print(f"mutation coverage: {len(variants)} across {len({x[0] for x in variants})} functions")
    print(f"mutations caught: {len(caught)}/{len(variants)}")
    if survivors:
        print("mutation survivors: " + ", ".join(survivors))
    return not survivors and len(variants) >= 40


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    parser.add_argument("--function", action="append", help="run only this function (repeatable)")
    parser.add_argument("--cases", type=float, default=1.0)
    parser.add_argument("--seed", type=int, default=94423)
    args = parser.parse_args()
    selected = set(args.function) if args.function else None
    unknown = (selected or set()) - set(FUNCS)
    if unknown:
        raise SystemExit("unknown function(s): " + ", ".join(sorted(unknown)))
    h = GimmickBHarness()
    h.scenarios(selected, args.cases, args.seed)
    default_run = selected is None and args.cases == 1.0 and args.seed == 94423
    summary = h.finish(selected, default_run, args.coverage or default_run)
    print("per-function differential cases:")
    for name in FUNCS:
        if name in h.counts:
            print(f"  {name:36s} {h.counts[name]}")
    if summary:
        gaps = [name for name, item in summary.items() if item["uncovered"] or item["stale_declared"]]
        if gaps:
            print("coverage gaps: " + ", ".join(gaps))
    else:
        gaps = []
    mutation_ok = True
    if args.mutate:
        mutation_ok = run_mutations()
    if h.failures or gaps or not mutation_ok:
        return 1
    print("PASS: gimmick batch scenarios matched")
    return 0


if __name__ == "__main__":
    sys.exit(main())
