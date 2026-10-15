"""Differential checker for the effect-script / projectile / RC-car RAM ports.

Run with .venv/Scripts/python.exe tools/mips/check_fxscript.py [--coverage] [--mutate].
The original runs from SCUS_944.23 with real area members; only the native
renderer and hardware / out-of-scope game calls are intercepted.
"""
from __future__ import annotations

import argparse
import ctypes as C
from contextlib import redirect_stdout
import io
import os
from pathlib import Path
import random
import re
import shutil
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/mips"), str(ROOT / "tools")]
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

M32 = 0xFFFFFFFF
STACK_LO = 0x801FF000
AREA_BASE = mr.AREA_BASE
PARTICLES = 0x800B9900
PROJECTILES = 0x800F5380
GROUPS = 0x800BF300
CAR = 0x800F5A20
POINTS = 0x801D0000
SCRIPT_BASE = 0x801D8000
PACKETS = 0x801A0000
EXPIRE_CALLBACK = 0x80123450

# name -> original address, byte size, C name, return kind, argument kinds
FUNCS = {
    "ParticlesDraw": (0x80069920, 716, "ae_ParticlesDraw", "v", ""),
    "EffectScriptUpdate": (0x80069BEC, 3524, "ae_EffectScriptUpdate", "v", ""),
    "RibbonDraw": (0x8006F3D0, 1596, "ae_RibbonDraw", "v", "uuuu"),
    "ProjectileSystemsUpdate": (0x80084AA0, 40, "ae_ProjectileSystemsUpdate", "v", ""),
    "ProjectileExpire": (0x80084BC0, 68, "ae_ProjectileExpire", "v", "uu"),
    "ProjectileRecordsUpdate": (0x80084C94, 1432, "ae_ProjectileRecordsUpdate", "v", ""),
    "ProjectileGroupSpawn": (0x80085254, 988, "ae_ProjectileGroupSpawn", "u", "uuuu"),
    "ProjectileGroupSpawnSpread": (0x80085630, 204, "ae_ProjectileGroupSpawnSpread", "v", "uuu"),
    "ProjectileGroupsUpdate": (0x800856FC, 1804, "ae_ProjectileGroupsUpdate", "v", ""),
    "RcCarUpdate": (0x800994A8, 1592, "ae_RcCarUpdate", "v", ""),
    "FUN_8014022c": (0x8014022C, 1, "ae_FUN_8014022c", "s", "uu"),
    "FUN_8014263c": (0x8014263C, 1, "ae_FUN_8014263c", "s", "uu"),
}

# All whole-function MIPS calls outside the requested table or native output boundary.
HOOKS = [
    ("SetRotMatrix", 0x8002FB00, 1), ("SetTransMatrix", 0x8002FB30, 1),
    ("ClearMemory", 0x800A3740, 3), ("Printf", 0x800A3720, 4),
    ("SpuWaitIdle", 0x800A1398, 0), ("SpuWaitLoop", 0x8009D79C, 1),
    ("SoundLoopStop", 0x8008806C, 2), ("PadRumble", 0x80014168, 3),
    ("PlayerCanActivateGadget", 0x8007CA48, 1), ("TurnTowardsY", 0x80066C90, 3),
    ("StepToward", 0x800463A4, 3), ("SpawnRandomParticle", 0x80070B70, 4),
    ("FUN_8014022c", 0x8014022C, 2), ("FUN_8014263c", 0x8014263C, 2),
    ("GetSolidHeight", 0x8004D8C0, 3), ("GimmickActivate", 0x8008FC30, 2),
    ("MeshInitFromAreaTable", 0x80015CC8, 2), ("FUN_8013d358", 0x8013D358, 3),
    ("EventCamStart", 0x8004D39C, 1), ("ModelTransformRigid", 0x800161C0, 3),
    ("ModelDraw", 0x80017CD4, 3),
]
HOOK_NAMES = {name: count for name, _address, count in HOOKS}
HOOK_NAMES["ProjectileExpireCallback"] = 4
HOOKFN = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, *([C.c_uint32] * 5))
PADFN = C.CFUNCTYPE(C.c_uint16, C.c_void_p, C.c_void_p, C.c_uint32)
IDLE = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p)
WAIT = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32)
ADSR = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint16, C.c_uint16)
VOICE_OFF = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint16)
SOUND_PRINTF = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32,
                           C.c_int32, C.c_int32, C.c_int32)
VOLUME = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32, C.c_uint16)


class SoundHw(C.Structure):
    _fields_ = [("user", C.c_void_p), ("wait_idle", IDLE), ("wait_loop", WAIT),
                ("set_voice_adsr", ADSR), ("voice_key_off", VOICE_OFF),
                ("printf_error", SOUND_PRINTF), ("set_voice_volume", VOLUME)]


class Hooks(C.Structure):
    _fields_ = [("user", C.c_void_p), ("sound", C.c_void_p),
                ("stale_spawn_position_pad", C.c_uint16), ("position_padding", PADFN)] + [
        ("SetRotMatrix", HOOKFN), ("SetTransMatrix", HOOKFN),
        ("ClearMemory", HOOKFN), ("ProjectEffectPoint", HOOKFN),
        ("DrawParticleQuad", HOOKFN), ("DrawRibbonStrip", HOOKFN),
        ("Printf", HOOKFN), ("SoundLoopStop", HOOKFN), ("CameraShake", HOOKFN),
        ("PadRumble", HOOKFN), ("PlayerCanActivateGadget", HOOKFN),
        ("TurnTowardsY", HOOKFN), ("StepToward", HOOKFN),
        ("SpawnRandomParticle", HOOKFN), ("ProjectileExpireCallback", HOOKFN),
        ("FUN_8014022c", HOOKFN), ("FUN_8014263c", HOOKFN),
        ("GetSolidHeight", HOOKFN), ("GimmickActivate", HOOKFN),
        ("MeshInitFromAreaTable", HOOKFN), ("FUN_8013d358", HOOKFN),
        ("EventCamStart", HOOKFN), ("ModelTransformRigid", HOOKFN),
        ("ModelDraw", HOOKFN)]


class Mem:
    def __init__(self, cpu=None, side=None):
        self.cpu, self.side = cpu, side

    def rd(self, address, size):
        if self.cpu is not None:
            return self.cpu.read(address, size)
        fn = {1: self.side.lib.ae_rd8, 2: self.side.lib.ae_rd16, 4: self.side.lib.ae_rd32}[size]
        return int(fn(self.side.c_ram, address))

    def wr(self, address, value, size):
        if self.cpu is not None:
            self.cpu.write(address, size, value)
        else:
            fn = {1: self.side.lib.ae_wr8, 2: self.side.lib.ae_wr16, 4: self.side.lib.ae_wr32}[size]
            fn(self.side.c_ram, address, value & ((1 << (size * 8)) - 1))


class Model:
    def __init__(self, mem, context):
        self.mem, self.context, self.log = mem, context, []
        self.last_projection_depth = 0

    def call(self, name, args):
        args = tuple(x & M32 for x in args)
        count = HOOK_NAMES.get(name, 0)
        if name == "Printf":
            # compare the format plus as many values as it has conversions: the original passes a leftover
            # register as an extra, unused argument after "Type[%d] Illegal command...: " (0x800A5BF0)
            if args[0] == 0x800A5C18:
                count = 3
            elif args[0] == 0x800A5BF0:
                count = 2
            elif args[0] == 0x800A5C14:
                count = 1
            else:
                count = 2
        logged = tuple(STACK_LO if STACK_LO <= value < 0x80200000 else value
                       for value in args[:count])
        self.log.append((name, *logged))
        if name == "ClearMemory":
            address, fill, length = args[:3]
            for index in range(min(length, 0x10000)):
                self.mem.wr(address + index, fill & 0xFF, 1)
            return address
        if name == "ProjectEffectPoint":
            point, output = args[:2]
            x = mr.signed(self.mem.rd(point, 2), 16)
            y = mr.signed(self.mem.rd(point + 2, 2), 16)
            z = mr.signed(self.mem.rd(point + 4, 2), 16)
            sx = (224 + (x >> 2)) & 0xFFFF
            sy = (120 - (y >> 2)) & 0xFFFF
            depth = 512 + (abs(z) % 4500)
            self.mem.wr(output, sx | (sy << 16), 4)
            self.last_projection_depth = (depth << 2) & M32
            return self.last_projection_depth
        if name == "DrawParticleQuad":
            cursor = self.mem.rd(0x800EC354, 4)
            self.mem.wr(0x800EC354, cursor + 40, 4)
            return 0
        if name == "DrawRibbonStrip":
            _points, count, _colors, _scale, work = args
            emitted = 0
            for index in range(max(0, min(count - 1, 15))):
                if self.mem.rd(work + 12 + index * 4, 4) != 0:
                    emitted += 1
            cursor = self.mem.rd(0x800EC354, 4)
            self.mem.wr(0x800EC354, cursor + 40 * emitted, 4)
            return 0
        if name == "SpuWaitIdle":
            return self.context.get("idle", 0)
        if name == "GetSolidHeight":
            return self.context.get("solid", -100)
        if name == "PlayerCanActivateGadget":
            return self.context.get("can_gadget", 1)
        if name == "StepToward":
            current, target, step = map(lambda n: mr.signed(n, 32), args[:3])
            if current < target - step:
                return current + step
            if current > target + step:
                return current - step
            return target
        if name in ("FUN_8014022c", "FUN_8014263c", "ProjectileExpireCallback"):
            return self.context.get("callback_result", 0)
        if name == "GimmickActivate":
            return self.context.get("gimmick_result", 0)
        return 0


def build(source=None, tag="fxscript"):
    # Link the verified dependency closure; CollisionUpdate now calls bodymove.
    import check_game as cg
    overrides={"src/effects/ae_fxscript.c":Path(source)} if source else {}
    return cg.build_dll(overrides,tag)


def build_mutant(source, tag):
    """Use the shared content-cached dependency closure for source mutations."""
    return build(source, tag)


class Harness:
    def __init__(self, source=None, tag="fxscript"):
        self.member_map = dump_areas.area_members()
        self.areas = [area for area in (1, 2, 5, 7, 0x1D) if area in self.member_map]
        if len(set(self.areas)) < 3:
            raise RuntimeError("fxscript checker requires real members from at least three areas")
        self.m = mr.MipsRam.from_extract()
        self.m.attach_gte_oracle()
        self.m.coverage = set()
        exe = self.m.snapshot()
        self.area_bases = {}
        self.area_blobs = {}
        for area in self.areas:
            member = self.member_map[area]
            blob = (ROOT / f"extract/members/{member:03X}.bin").read_bytes()
            self.m.restore(exe)
            self.m.load_area(blob)
            self._install_area_globals(self.m, blob, area)
            self.area_bases[area] = self.m.snapshot()
            self.area_blobs[area] = blob
        self.m.restore(self.area_bases[self.areas[0]])
        self.stack_lo = STACK_LO - 0x80000000
        reg.declare("stack_window", "801FF000..80200000")
        self.dll = build(source, tag)
        self.lib = C.CDLL(str(self.dll))
        P, U, I = C.c_void_p, C.c_uint32, C.c_int32
        lib = self.lib
        lib.ae_ram_create.restype = P
        lib.ae_ptr.argtypes = [P, U, U]; lib.ae_ptr.restype = P
        lib.ae_ram_clear_error.argtypes = [P]
        lib.ae_ram_failed.argtypes = [P]; lib.ae_ram_failed.restype = I
        lib.ae_ram_error.argtypes = [P]; lib.ae_ram_error.restype = C.c_char_p
        for width, restype in ((8, U), (16, U), (32, U)):
            fn = getattr(lib, f"ae_rd{width}"); fn.argtypes = [P, U]; fn.restype = restype
            wr = getattr(lib, f"ae_wr{width}"); wr.argtypes = [P, U, U]
        self.c_ram = lib.ae_ram_create()
        self.c_main = (C.c_uint8 * mr.RAM_SIZE).from_address(lib.ae_ptr(self.c_ram, 0x80000000, mr.RAM_SIZE))
        self.c_scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(
            lib.ae_ptr(self.c_ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        for name, (_address, _size, cname, ret, kinds) in FUNCS.items():
            fn = getattr(lib, cname)
            fn.argtypes = [P, P] + [I if kind == "s" else U for kind in kinds]
            fn.restype = None if ret == "v" else (U if ret == "u" else I)
        self.hooks = Hooks()
        self.sound = SoundHw()
        self._callbacks = []
        self._wire_callbacks()
        self.counts = {name: 0 for name in FUNCS}
        self.failures = 0
        self.mutation_function = None
        self.masks = []

    @staticmethod
    def _install_area_globals(m, blob, area):
        if len(blob) < 0x4DC:
            raise RuntimeError(f"area {area:02X} member is too short")
        header = struct.unpack_from("<I", blob, 0x4D0)[0]
        if not header or header + 8 > len(blob):
            raise RuntimeError(f"area {area:02X} has no readable area header")
        grid = header + struct.unpack_from("<I", blob, header + 4)[0] * 4
        if grid + 8 > len(blob):
            raise RuntimeError(f"area {area:02X} has no readable ground grid")
        width, depth, colors = struct.unpack_from("<HHI", blob, grid)
        cells = grid + 8
        heights = cells + width * depth * 12 + colors * 4
        m.write32(0x800F44E4, AREA_BASE + header)
        m.write16(0x800EBFA0, width)
        m.write16(0x800EBFA2, depth)
        m.write32(0x800EBFA8, AREA_BASE + cells)
        m.write32(0x800EBFB4, AREA_BASE + heights)
        m.write16(0x800F4476, area)

    def _wire_callbacks(self):
        P, U, I = C.c_void_p, C.c_uint32, C.c_int32
        keep = self._callbacks
        def hook_cb(name):
            cb = HOOKFN(lambda _u, _r, a0, a1, a2, a3, a4, n=name:
                         self.c_model.call(n, (a0, a1, a2, a3, a4)))
            keep.append(cb)
            return cb
        self.hooks.sound = C.addressof(self.sound)
        self.hooks.stale_spawn_position_pad = 0
        padding = PADFN(lambda _u, _r, _fn: 0)
        keep.append(padding); self.hooks.position_padding = padding
        self.sound.wait_idle = IDLE(lambda _u, _r: self.c_model.call("SpuWaitIdle", ()))
        self.sound.wait_loop = WAIT(lambda _u, _r, units: self.c_model.call("SpuWaitLoop", (units,)))
        self.sound.set_voice_adsr = ADSR(lambda _u, _r, ch, a, b:
                                         self.c_model.call("set_voice_adsr", (ch, a, b)))
        self.sound.voice_key_off = VOICE_OFF(lambda _u, _r, half, mask:
                                             self.c_model.call("voice_key_off", (half, mask)))
        self.sound.printf_error = SOUND_PRINTF(lambda _u, _r, fmt, argc, a, b, c:
                                               self.c_model.call("Printf", (fmt, a, b, c, 0)))
        self.sound.set_voice_volume = VOLUME(lambda _u, _r, ch, side, value:
                                             self.c_model.call("set_voice_volume", (ch, side, value)))
        keep.extend([self.sound.wait_idle, self.sound.wait_loop, self.sound.set_voice_adsr,
                     self.sound.voice_key_off, self.sound.printf_error, self.sound.set_voice_volume])
        # callback fields follow the C struct's declaration order after its fixed fields
        fields = ["SetRotMatrix", "SetTransMatrix", "ClearMemory", "ProjectEffectPoint",
                  "DrawParticleQuad", "DrawRibbonStrip", "Printf", "SoundLoopStop", "CameraShake",
                  "PadRumble", "PlayerCanActivateGadget", "TurnTowardsY", "StepToward",
                  "SpawnRandomParticle", "ProjectileExpireCallback", "FUN_8014022c", "FUN_8014263c",
                  "GetSolidHeight", "GimmickActivate", "MeshInitFromAreaTable", "FUN_8013d358",
                  "EventCamStart", "ModelTransformRigid", "ModelDraw"]
        for name in fields:
            setattr(self.hooks, name, hook_cb(name))
        # Retain these globals to document the ABI and avoid accidental ctypes collection.
        _ = P, U, I

    def sync_to_c(self):
        C.memmove(self.c_main, bytes(self.m.ram), mr.RAM_SIZE)
        C.memmove(self.c_scratch, bytes(self.m.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.c_ram)

    def base_setup(self, m):
        # Real area members are loaded for every scenario. The sound bank table is
        # deliberately unavailable so already-ported sound calls take their real
        # missing-bank return path without emulating SPU state.
        m.write_bytes(0x800B2D08, bytes(198 * 8))
        m.write_bytes(0x800BF670, b"\xff" * 0x100)
        m.write_bytes(0x800BF610, b"\xff" * (12 * 8))
        m.write_bytes(0x800F5AC0, bytes(24 * 0x120))
        m.write_bytes(0x800E07F0, bytes(24 * 0x66))
        for i in range(24):
            m.write16(0x800BF370 + i * 0x1C, 0xFFFF)
            m.write16(0x800BF370 + i * 0x1C + 2, 0xFFFF)
            m.write16(0x800BF370 + i * 0x1C + 0x12, 0)
        m.write_bytes(0x800B9900, bytes(100 * 0x34))
        m.write_bytes(0x800F5380, bytes(32 * 0x34))
        m.write_bytes(0x800BF300, bytes(4 * 0x1C))
        for i in range(32):
            m.write8(PROJECTILES + i * 0x34, 0)
        # A clean collision-body allocator/registration state.
        m.write32(0x800B20D8, 0); m.write32(0x800B20DC, 0)
        for i in range(256):
            slot = 0x800BBDA0 + i * 0x14
            m.write16(slot, 0xFFFF); m.write16(slot + 2, 0xFFFF)
            for off in (4, 8, 12, 16): m.write32(slot + off, 0)
        for i in range(128): m.write16(0x800BEDA0 + 2 * i, 0xFFFF)
        for i in range(16):
            loop = 0x800BEEA0 + 12 * i
            m.write16(loop, 0xFFFF); m.write16(loop + 2, 0); m.write32(loop + 4, 0); m.write16(loop + 8, 0)
        m.write16(0x800F45D4, 0)
        m.write32(0x800EC350, 0); m.write32(0x800EC354, PACKETS); m.write32(0x800EC35C, 0)
        m.write32(0x800FC358, 0)
        m.write32(0x800FC350 + 12, 0)
        m.write16(0x800E6916, 0)
        m.write16(0x800E38B4, 0)
        m.write32(0x800EC350, 0)
        m.write16(0x800B1B74, 0); m.write8(0x800BAD50, 0)
        m.write16(0x800BE8F0, 0x7391)
        m.write16(0x800AE8F0, 0x2541)
        # distinctive player position (s32 x / y / z; the script origin copies their low halfwords)
        m.write32(0x800EC1E0 + 0x24, 0x00011234); m.write32(0x800EC1E0 + 0x28, 0xFFFE5678)
        m.write32(0x800EC1E0 + 0x2C, 0x00039ABC)
        m.write32(0x800EC350, 0)

    def mips_hooks(self, model, extra=None):
        hooks = {}
        for name, address, _count in HOOKS:
            hooks[address] = lambda cpu, regs, n=name: model.call(n, tuple(regs[4:9]))
        hooks[EXPIRE_CALLBACK] = lambda cpu, regs: model.call(
            "ProjectileExpireCallback", (regs[4], regs[5], EXPIRE_CALLBACK, regs[7], 0))
        if extra:
            hooks.update(extra)
        def memset(cpu, regs):
            return model.call("ClearMemory", (regs[4], regs[5], regs[6], 0, 0))
        hooks[0x800A3740] = memset
        return hooks

    def mips_patches(self, model):
        def project(cpu, regs, point_reg, out):
            model.call("ProjectEffectPoint", (regs[point_reg], out, 0, 0, 0))
        def particle_projection(cpu, regs):
            project(cpu, regs, 2, mr.SCRATCH_BASE)
            return 0x800699EC
        def particle_depth(cpu, regs):
            cpu.write32(mr.SCRATCH_BASE + 4, model.last_projection_depth >> 2)
            return 0x80069A2C
        def particle_packet(cpu, regs):
            particle = regs[14]
            sx = cpu.read16(mr.SCRATCH_BASE)
            sy = cpu.reads16(mr.SCRATCH_BASE + 2)
            depth = cpu.read32(mr.SCRATCH_BASE + 4)
            size = (regs[6] & 0xFFFF) | ((regs[8] & 0xFFFF) << 16)
            model.call("DrawParticleQuad", (particle, sx, sy, depth, size))
            regs[11] = (regs[11] + 40) & M32
            return 0x80069BB4
        def ribbon_projection(cpu, regs):
            project(cpu, regs, 4, mr.SCRATCH_BASE + 8)
            return 0x8006F498
        def ribbon_depth(cpu, regs):
            cpu.write32(regs[7], model.last_projection_depth >> 2)
            return 0x8006F4F0
        def ribbon_packet(cpu, regs):
            color_table = cpu.read32(regs[29] + 0x50)
            args = (regs[20], regs[30], color_table, regs[19], mr.SCRATCH_BASE)
            model.call("DrawRibbonStrip", args)
            emitted = sum(1 for i in range(max(0, min(regs[30] - 1, 15)))
                          if cpu.read32(mr.SCRATCH_BASE + 12 + i * 4) != 0)
            regs[10] = (regs[10] + emitted * 40) & M32
            cpu.write32(0x800EC354, regs[10])
            return 0x8006F9DC
        return {
            0x800699E4: particle_projection, 0x80069A1C: particle_depth,
            0x80069AC8: particle_packet,
            0x8006F490: ribbon_projection, 0x8006F4E0: ribbon_depth,
            0x8006F874: ribbon_packet,
        }

    def run(self, name, label, args, setup, context=None, area=None):
        if self.mutation_function and name != self.mutation_function:
            return
        area = area if area in self.areas else self.areas[0]
        self.m.restore(self.area_bases[area])
        self.m.gte_reset()
        self.base_setup(self.m)
        setup(self.m)
        initial = self.m.snapshot()
        self.sync_to_c()
        context = dict(context or {})
        m_model = Model(Mem(cpu=self.m), context)
        c_model = Model(Mem(side=self), context)
        self.m.hook_log.clear()
        self.c_model = c_model
        address, _size, _cname, retkind, _argkinds = FUNCS[name]
        extra = {}
        if name in ("FUN_8014022c", "FUN_8014263c"):
            # Overlay RAM is not loaded into the EXE image. Validate its adapter
            # at the entry boundary; the game-provided implementation stays a hook.
            extra[address] = lambda cpu, regs, n=name: m_model.call(n, tuple(regs[4:9]))
        if name == "ProjectileExpire" and self.m.read32(args[0] + 4) == EXPIRE_CALLBACK:
            extra[EXPIRE_CALLBACK] = lambda cpu, regs: m_model.call(
                "ProjectileExpireCallback", (regs[4], regs[5], EXPIRE_CALLBACK, regs[7], 0))
        try:
            result = self.m.call(address, args, hooks=self.mips_hooks(m_model, extra),
                                 patches=self.mips_patches(m_model))
        except mr.MipsError as error:
            machine = self.m._last_machine
            state = self.m._last_state
            registers = state[0] if state else []
            print(f"MIPS exception in {name}: {error}; state={machine}; regs="
                  f"a0-a3={[hex(x) for x in registers[4:8]]}, s1/s4/fp={[hex(registers[x]) for x in (17,20,30)] if registers else []}")
            raise
        fn = getattr(self.lib, FUNCS[name][2])
        converted = [mr.signed(v, 32) if k == "s" else v & M32 for v, k in zip(args, FUNCS[name][4])]
        c_result = fn(self.c_ram, C.byref(self.hooks), *converted)
        self.counts[name] += 1
        problems = []
        if self.lib.ae_ram_failed(self.c_ram):
            problems.append("C error: " + self.lib.ae_ram_error(self.c_ram).decode(errors="replace"))
        if retkind != "v":
            c_value = (c_result or 0) & M32
            if result[2] != c_value:
                problems.append(f"return MIPS {result[2]:08X}, C {c_value:08X}")
        if m_model.log != c_model.log:
            problems.append(f"hooks MIPS={m_model.log!r} C={c_model.log!r}")
        left, right = bytearray(bytes(self.m.ram[:self.stack_lo])), bytearray(bytes(self.c_main[:self.stack_lo]))
        for off, length in self.masks:
            left[off:off + length] = bytes(length); right[off:off + length] = bytes(length)
        if left != right:
            diffs = [i for i, (a, b) in enumerate(zip(left, right)) if a != b]
            problems.append("RAM differs " + ", ".join(
                f"{0x80000000+i:08X}(M={left[i]:02X},C={right[i]:02X})" for i in diffs[:10]))
        if bytes(self.m.scratch) != bytes(self.c_scratch):
            diffs = [i for i, (a, b) in enumerate(zip(self.m.scratch, self.c_scratch)) if a != b]
            problems.append("scratch differs " + ", ".join(
                f"{mr.SCRATCH_BASE+i:08X}(M={self.m.scratch[i]:02X},C={self.c_scratch[i]:02X})" for i in diffs[:10]))
        if problems:
            self.failures += 1
            print(f"FAIL {name} [{label}]")
            for problem in problems: print("  ", problem)
            raise SystemExit(1)
        return initial

    def finish(self, default_run=False, show_coverage=False):
        summary = reg.coverage_summary([(n, spec[0], spec[1], True) for n, spec in FUNCS.items()], self.m.coverage)
        if show_coverage:
            reg.print_coverage(summary)
        if default_run:
            reg.write_results(self.counts, summary, not self.failures, function_names=list(FUNCS))
        return summary


def reset_particle_slots(m):
    m.write_bytes(PARTICLES, bytes(100 * 0x34))
    m.write16(0x800BE8F0, 0x7291)


def write_projected_point(m, address, x, y, z, pad=0):
    m.write16(address, x); m.write16(address + 2, y); m.write16(address + 4, z); m.write16(address + 6, pad)


def write_record(m, index=0, state=3, kind=0, timer=30, source=0x801D1000, body=0):
    p = PROJECTILES + index * 0x34
    m.write_bytes(p, bytes(0x34))
    m.write8(p, state); m.write8(p + 1, kind); m.write8(p + 2, timer)
    m.write32(p + 0x10, source); m.write32(p + 0x0C, body)
    m.write16(p + 0x1C, 40); m.write16(p + 0x1E, -12); m.write16(p + 0x20, 20)
    m.write16(p + 0x24, 3); m.write16(p + 0x26, -2); m.write16(p + 0x28, 5)
    m.write16(p + 0x30, 80); m.write16(p + 0x22, 1)
    m.write16(source, 0); m.write16(source + 2, 0); m.write16(source + 4, 0)
    return p


def install_surface_one(m):
    """Install one test-only surface volume over real area height data."""
    collider = 0x801D2000
    m.write32(0x800F455C, collider)
    m.write16(0x800F45D4, 1)
    m.write_bytes(collider, bytes(0x38))
    for off, value in ((0x00, 0), (0x02, 500), (0x04, 0),
                       (0x06, 2000), (0x08, 500), (0x0A, 2000),
                       (0x12, 0x0109)):
        m.write16(collider + off, value)


def setup_script_case(m, opcodes):
    reset_particle_slots(m)
    cursor = SCRIPT_BASE
    for slot, (opcode, operands) in enumerate(opcodes):
        stream = bytearray(struct.pack("<H", opcode))
        for value in operands: stream += struct.pack("<H", value & 0xFFFF)
        stream += struct.pack("<H", 4)
        m.write_bytes(cursor, stream)
        particle = PARTICLES + slot * 0x34
        m.write8(particle, slot + 1)
        m.write8(particle + 3, 0xFF)
        m.write32(particle + 4, cursor)
        m.write16(particle + 0x14, 10); m.write16(particle + 0x16, 100); m.write16(particle + 0x18, 20)
        m.write16(particle + 0x1C, 3); m.write16(particle + 0x1E, 1); m.write16(particle + 0x20, -2)
        m.write8(particle + 0x12, 0); m.write8(particle + 0x13, 0)
        cursor += (len(stream) + 3) & ~3


def setup_script_stream(m, commands, position=(10, 100, 20), color=0):
    reset_particle_slots(m)
    stream = bytearray()
    for opcode, operands in commands:
        stream += struct.pack("<H", opcode)
        for value in operands:
            stream += struct.pack("<H", value & 0xFFFF)
    m.write_bytes(SCRIPT_BASE, stream)
    particle = PARTICLES
    m.write8(particle, 1); m.write8(particle + 3, 0xFF)
    m.write32(particle + 4, SCRIPT_BASE)
    for axis, value in enumerate(position):
        m.write16(particle + 0x14 + axis * 2, value)
    m.write16(particle + 0x30, color)
    m.write16(particle + 0x1A, 0); m.write16(particle + 0x1C, 0); m.write16(particle + 0x1E, 0)


def scenarios(h, scale=1.0, seed=94423):
    rng = random.Random(seed)
    reps = max(1, round(4 * scale))

    # Leaves first: the two RAM-only adapters and expire behavior.
    for name in ("FUN_8014022c", "FUN_8014263c"):
        for i in range(2):
            h.run(name, f"adapter-{i}", (0x801D0000 + i * 8, 0x801D0100 + i * 8),
                  lambda m: None, {"callback_result": i - 1})
    for i, callback in enumerate((0, EXPIRE_CALLBACK)):
        p = PROJECTILES + 0x34
        def setup_expire(m, callback=callback):
            m.write_bytes(p, bytes(0x34)); m.write32(p + 4, callback); m.write8(p, 3); m.write32(p + 0x0C, 0x801D1000)
        h.run("ProjectileExpire", f"callback-{i}", (p, i), setup_expire)

    # Projection / clipping / OT output cases with varied input positions.
    for i, (x, y, z, width, height) in enumerate([
        (0, 0, 0, 8, 8), (900, 500, 1500, 32, 12), (-1200, -400, 4000, 1, 1),
        (100, 100, 6000, 12, 0), (0, 0, 4, 255, 255),
    ]):
        def setup_draw(m, x=x, y=y, z=z, width=width, height=height):
            reset_particle_slots(m); p = PARTICLES
            m.write8(p, 1); m.write8(p + 0x12, width); m.write8(p + 0x13, height)
            write_projected_point(m, p + 0x14, x, y, z)
            m.write32(p + 0x24, 0x04030201); m.write16(p + 0x26, 0x1234); m.write16(p + 0x2A, 0x5678)
        h.run("ParticlesDraw", f"projection-{i}", (), setup_draw, area=h.areas[i % len(h.areas)])
    def setup_particle_edge(m, y, z):
        reset_particle_slots(m); p = PARTICLES
        m.write8(p, 1); m.write8(p + 0x12, 8); m.write8(p + 0x13, 8)
        write_projected_point(m, p + 0x14, 0, y, z)
    h.run("ParticlesDraw", "screen-y-minus-32", (),
          lambda m: setup_particle_edge(m, 608, 0), area=h.areas[0])
    h.run("ParticlesDraw", "depth-4095", (),
          lambda m: setup_particle_edge(m, 0, 3583), area=h.areas[1])

    # Explicit legal opcodes, plus a valid and invalid END_LOOP path and
    # both ground/water exits. The stream bytes are synthetic; all render /
    # terrain dependencies still run against the selected real area image.
    legal = {
        1: [2], 2: [3, 0xFFFE], 3: [], 4: [], 8: [1, 0xFFFF, 3],
        9: [4, 2, 3, 5, 0xFFFE, 1], 10: [4], 11: [4, 2], 12: [], 13: [5], 14: [5, 2],
        15: [1, 2, 3], 16: [1], 17: [2], 18: [3], 19: [4, 2, 5, 3, 6, 4],
        20: [2, 1], 21: [3, 2], 22: [4, 3], 23: [1, 2, 3], 24: [2048, 4096, 1024],
        25: [], 26: [], 27: [], 30: [0x1234, 0x0804], 31: [7], 32: [9], 33: [12, 14],
        34: [10, 2, 11, 3], 35: [20, 5], 36: [4096, 2048], 37: [12, 30, 44],
        38: [40, 10], 39: [3], 40: [20, 30], 41: [], 42: [], 43: [], 44: [], 45: [],
        46: [], 47: [], 48: [20], 49: [3, 10], 51: [], 52: [0],
    }
    script_cases = []
    for opcode, operands in legal.items():
        script_cases.append((opcode, operands))
    def all_scripts(m): setup_script_case(m, script_cases)
    h.run("EffectScriptUpdate", "legal-opcode-sweep", (), all_scripts, area=h.areas[1])
    h.run("EffectScriptUpdate", "end-loop", (),
          lambda m: setup_script_case(m, [(1, [3]), (3, []), (4, [])]), area=h.areas[2])
    h.run("EffectScriptUpdate", "ground-kill", (),
          lambda m: (setup_script_case(m, [(51, [])]), m.write32(0x800EC1E0 + 0x12C, -1000),
                     m.write16(PARTICLES + 0x16, 2000)), {"solid": -1000}, area=h.areas[0])
    h.run("EffectScriptUpdate", "water-crossing", (),
          lambda m: (setup_script_stream(m, [(51, []), (4, [])], (10, 200, 20)),
                     m.write32(0x800EC1E0 + 0x12C, 1000),
                     m.write16(0x800B4D90, 100), m.write16(0x800B4D92, 0x7291)),
          {"solid": 1000}, area=h.areas[2])
    h.run("EffectScriptUpdate", "ground-equal-water-crossing", (),
          lambda m: (setup_script_stream(m, [(51, [])], (10, 1000, 20)),
                     m.write32(0x800EC1E0 + 0x12C, 1000),
                     m.write16(0x800B4D90, 100)),
          {"solid": 1000}, area=h.areas[0])
    h.run("EffectScriptUpdate", "invalid-command", (),
          lambda m: setup_script_stream(m, [(53, [])]), area=h.areas[1])
    h.run("EffectScriptUpdate", "loop-depth-fixed", (),
          lambda m: setup_script_stream(m, [(1, [2]), (39, [1]), (3, []), (4, [])]),
          area=h.areas[0])
    h.run("EffectScriptUpdate", "loop-depth-warning", (),
          lambda m: (setup_script_stream(m, [(1, [2]), (4, [])]),
                     m.write8(PARTICLES + 3, 1)), area=h.areas[1])
    h.run("EffectScriptUpdate", "loop-depth-random-error", (),
          lambda m: setup_script_stream(m, [(2, [1, 0]), (2, [1, 0]),
                                            (2, [1, 0]), (4, [])]), area=h.areas[3])
    h.run("EffectScriptUpdate", "fade-underflow", (),
          lambda m: setup_script_stream(m, [(39, [1]), (10, [2])], color=0), area=h.areas[4])
    h.run("EffectScriptUpdate", "wait-and-fade", (),
          lambda m: (setup_script_case(m, [(10, [3]), (12, []), (39, [1])]),
                     m.write16(PARTICLES + 0x22, 2), m.write8(PARTICLES + 0x30, 1)), area=h.areas[3])

    # Ribbon counts cover the early returns, duplicate collapse, width clipping,
    # both endpoints, midpoint fallback and maximum point limit.
    for count in (0, 1, 2, 3, 4, 16, 17, 0xFFFFFFFF):
        def setup_ribbon(m, count=count):
            for i in range(16):
                write_projected_point(m, POINTS + i * 8, (i - 4) * 36, i * 17, 100 + i * 100)
            if count == 3:
                write_projected_point(m, POINTS + 8, 0, 0, 100)
                write_projected_point(m, POINTS + 16, 0, 0, 100)
        h.run("RibbonDraw", f"count-{count:08X}", (POINTS, count, 0x800B1B78, 24), setup_ribbon,
              area=h.areas[count % len(h.areas)])
    # The original compares even the first packed SXY against an initial zero.
    # This first point projects to (0,0), and must collapse before point two.
    h.run("RibbonDraw", "first-projection-origin", (POINTS, 2, 0x800B1B78, 24),
          lambda m: (write_projected_point(m, POINTS, -896, 480, 100),
                     write_projected_point(m, POINTS + 8, 40, 20, 200)),
          area=h.areas[0])
    def setup_ribbon_bowtie(m):
        for i in range(3):
            write_projected_point(m, POINTS + i * 8, (i - 1) * 80, i * 32, 1200)
        write_projected_point(m, POINTS + 16, -80, 0, 1200)
    h.run("RibbonDraw", "interior-reversal", (POINTS, 3, 0x800B1B78, 24),
          setup_ribbon_bowtie, area=h.areas[0])
    h.run("RibbonDraw", "clipped-points", (POINTS, 2, 0x800B1B78, 24),
          lambda m: (write_projected_point(m, POINTS, 10000, 1000, 1200),
                     write_projected_point(m, POINTS + 8, 12000, 2000, 1200)),
          area=h.areas[1])
    h.run("RibbonDraw", "width-rounding-boundary", (POINTS, 2, 0x800B1B78, 24),
          lambda m: (write_projected_point(m, POINTS, 0, 0, 51),
                     write_projected_point(m, POINTS + 8, 80, 40, 51)),
          area=h.areas[2])

    # Group spawn modes and exhaustion, then the spread wrapper.
    position, direction = 0x801D1000, 0x801D1020
    for count in (0, 1, 2, 3, 4, 5, 6, 7, 0xFFFFFFFF):
        def setup_spawn(m, count=count):
            for axis, value in enumerate((100, 200, -300)):
                m.write16(position + axis * 2, value); m.write16(direction + axis * 2, value - 40)
            m.write16(0x800F5090, 1); m.write16(0x800F5098, 2); m.write16(0x800F50A0, 3)
            m.write16(0x800B2960, -128); m.write16(0x800B2964, 0); m.write16(0x800B2968, 128)
            m.write16(0x800E6916, (count >> 1) & 1)
        h.run("ProjectileGroupSpawn", f"mode-{count:08X}", (position, direction, count, 0x800EC1E0),
              setup_spawn, area=h.areas[count % len(h.areas)])
    h.run("ProjectileGroupSpawn", "aim-mode-five", (position, direction, 5, 0x800EC1E0),
          lambda m: (m.write16(position, 100), m.write16(position + 2, 200),
                     m.write16(position + 4, -300), m.write16(direction, 60),
                     m.write16(direction + 2, 40), m.write16(direction + 4, -80),
                     m.write16(0x800E6916, 1)), area=h.areas[2])
    def setup_group_exhausted(m):
        m.write_bytes(GROUPS, bytes(4 * 0x1C))
        for i in range(4): m.write8(GROUPS + i * 0x1C + 0x12, 1)
        m.write16(position, 100); m.write16(position + 2, 200); m.write16(position + 4, -300)
        m.write16(direction, 60); m.write16(direction + 2, 40); m.write16(direction + 4, -80)
    h.run("ProjectileGroupSpawn", "group-table-exhausted", (position, direction, 2, 0x800EC1E0),
          setup_group_exhausted, area=h.areas[1])
    for mode in (0, 1, 2, 3, 4):
        h.run("ProjectileGroupSpawnSpread", f"spread-{mode}", (position, direction, mode),
              lambda m: (m.write16(position, 80), m.write16(position + 2, 40), m.write16(position + 4, -160),
                         m.write16(direction, 100), m.write16(direction + 2, -50), m.write16(direction + 4, 30)),
              area=h.areas[mode % len(h.areas)])

    # Projectile record states, contact cases, timer expiration and surface hits.
    for state, kind, timer, contact in [
        (0, 0, 0, 0), (1, 0, 0, 0), (2, 0, 0, 0), (3, 0, 1, 0),
        (3, 0, 30, 6), (3, 0, 30, 11), (3, 0x20, 30, 10),
        (3, 0x40, 30, 10), (3, 1, 30, 0), (3, 0x10, 30, 0),
    ]:
        def setup_record(m, state=state, kind=kind, timer=timer, contact=contact):
            p = write_record(m, state=state, kind=kind, timer=timer)
            if contact:
                body = 0x801D1100
                m.write32(p + 0x0C, body); m.write32(body + 0x2C, contact << 28)
            m.write32(0x800B20D8, 0); m.write32(0x800B20DC, 0)
        h.run("ProjectileRecordsUpdate", f"record-{state}-{kind:02X}-{timer}-{contact}", (), setup_record,
              {"solid": 200}, area=h.areas[(state + kind + contact) % len(h.areas)])
    for label, flags, height, fall, material in [
        ("body10-no-flags", 0x00, -12, 0, False),
        ("body10-positive-20", 0x20, 30, 0, False),
        ("body10-rounding-boundary", 0x20, 25, 0, False),
        ("body10-expire-20", 0x20, 30, 30, False),
        ("body10-positive-40", 0x40, 30, 0, False),
        ("body10-bounce", 0x20, -30, 0, False),
        ("surface-entry", 0x01, -12, 1, True),
    ]:
        def setup_record_deep(m, flags=flags, height=height, fall=fall, material=material):
            body = 0 if material else 0x801D1100
            p = write_record(m, state=3, kind=flags | 1, timer=30, body=body)
            if body:
                m.write32(body + 0x2C, 10 << 28)
            m.write16(p + 0x1E, height); m.write8(p + 0x22, fall)
            if material:
                install_surface_one(m)
        h.run("ProjectileRecordsUpdate", label, (), setup_record_deep,
              {"solid": 200}, area=h.areas[2 if material else 4])
    h.run("ProjectileSystemsUpdate", "records-and-groups", (),
          lambda m: (write_record(m, state=3, kind=1, timer=20),
                     m.write8(GROUPS + 0x12, 3), m.write16(GROUPS + 0x14, 8),
                     m.write16(GROUPS + 0x16, 4)), {"solid": 200}, area=h.areas[0])

    # Group state machine transitions and render callbacks.
    for state, rec_state in [(1, 1), (2, 2), (3, 0), (4, 1), (5, 2), (6, 0), (7, 0)]:
        def setup_group(m, state=state, rec_state=rec_state):
            m.write_bytes(GROUPS, bytes(4 * 0x1C)); g = GROUPS
            rec = PROJECTILES + 0x34
            m.write32(g + 4, rec); m.write8(g + 0x12, state); m.write8(g + 0x13, state)
            m.write32(g + 8, 0x800EC1E0)
            for axis, value in enumerate((100, 80, -160)):
                m.write16(g + 0x0C + axis * 2, value)
                m.write16(g + 0x14 + axis * 2, 20 - axis * 10)
            m.write8(g + 1, 2); m.write8(g + 2, 3); m.write8(g + 3, 4)
            m.write8(rec, rec_state)
            m.write32(0x800EC350, 0); m.write32(0x800EC354, PACKETS)
            m.write32(0x800F4350, 2)
        h.run("ProjectileGroupsUpdate", f"state-{state}-record-{rec_state}", (), setup_group,
              {"solid": 300}, area=h.areas[state % len(h.areas)])
    def setup_group_rounding(m):
        setup_group(m, state=1, rec_state=1)
        rec = PROJECTILES + 0x34
        m.write16(rec + 0x1C, 17); m.write16(rec + 0x20, 19)
    h.run("ProjectileGroupsUpdate", "state-1-negative-rounding", (), setup_group_rounding,
          {"solid": 300}, area=h.areas[0])
    h.run("ProjectileGroupsUpdate", "state-1-live-record", (),
          lambda m: setup_group(m, state=1, rec_state=0),
          {"solid": 300}, area=h.areas[1])
    for overlay in (66, 56):
        def setup_group_overlay(m, overlay=overlay):
            setup_group(m, state=5, rec_state=1)
            m.write16(0x800F4476, overlay)
            m.write16(0x800EC1E0, 300); m.write16(0x800EC1E2, 0)
            m.write16(0x800EC1E4, -100)
        h.run("ProjectileGroupsUpdate", f"state-5-overlay-{overlay}", (), setup_group_overlay,
              {"solid": 300}, area=h.areas[1])
    def setup_group_state5_live(m):
        setup_group(m, state=5, rec_state=0)
        m.write8(PROJECTILES + 0x34 + 3, 2)
    h.run("ProjectileGroupsUpdate", "state-5-live", (), setup_group_state5_live,
          {"solid": 300}, area=h.areas[2])
    def setup_group_extreme(m, x, y, z):
        setup_group(m, state=3, rec_state=0)
        rec = PROJECTILES + 0x34
        for axis, value in enumerate((x, y, z)):
            m.write16(rec + 0x1C + axis * 2, value)
    h.run("ProjectileGroupsUpdate", "state-3-clamp-high", (),
          lambda m: setup_group_extreme(m, 20000, 20000, 20000),
          {"solid": 300}, area=h.areas[3])
    h.run("ProjectileGroupsUpdate", "state-3-clamp-low", (),
          lambda m: setup_group_extreme(m, -20000, -20000, -20000),
          {"solid": 300}, area=h.areas[4])
    def setup_group_state7_hit(m, velocity_y):
        setup_group(m, state=7, rec_state=0)
        m.write16(GROUPS + 0x0E, 400)
        m.write16(GROUPS + 0x16, velocity_y)
    h.run("ProjectileGroupsUpdate", "state-7-bounce", (),
          lambda m: setup_group_state7_hit(m, 100), {"solid": 300}, area=h.areas[0])
    h.run("ProjectileGroupsUpdate", "state-7-deactivate", (),
          lambda m: setup_group_state7_hit(m, 20), {"solid": 300}, area=h.areas[2])
    h.run("ProjectileGroupsUpdate", "state-7-bounce-boundary", (),
          lambda m: setup_group_state7_hit(m, 45), {"solid": 300}, area=h.areas[1])
    def setup_group_timeout(m):
        setup_group(m, state=7, rec_state=0)
        m.write8(GROUPS + 0x13, 59)
    h.run("ProjectileGroupsUpdate", "state-7-timeout", (), setup_group_timeout,
          {"solid": 300}, area=h.areas[3])

    # RC car active/timer/steering/collision/surface branches.
    for i, (active, timer, vertical, speed, aim, contact, impacts) in enumerate([
        (0, 0, 0, 0, 0, 0, 0), (1, 2, 0, 0, 0, 0, 0), (1, 0, -20, 40, 0, 0, 0),
        (1, 0, 0, 40, 500, 0, 0), (1, 0, 0, 5, 0, 0, 0),
        (1, 0, 0, 40, 0, 11, 0), (1, 0, 0, 40, 0, 0, 30),
    ]):
        def setup_car(m, active=active, timer=timer, vertical=vertical, speed=speed,
                      aim=aim, contact=contact, impacts=impacts):
            m.write_bytes(CAR, bytes(0x80)); m.write32(CAR + 0x70, active)
            m.write32(CAR + 0x24, 0); m.write32(CAR + 0x28, 100); m.write32(CAR + 0x2C, 0)
            m.write32(CAR + 0x30, speed); m.write32(CAR + 0x34, vertical); m.write32(CAR + 0x38, speed)
            m.write16(CAR + 0x46, 36); m.write16(CAR + 0x74, 0); m.write16(CAR + 0x76, 0)
            m.write16(CAR + 0x7A, impacts); m.write32(CAR + 0x50, contact << 28)
            m.write16(CAR + 0x78, timer); m.write16(0x800E5320 + 0x182, aim)
            m.write8(0x800EC1E0 + 0xF2, 7); m.write16(0x800EC1E0 + 0xE8, 10)
            m.write16(0x800EC1E0 + 0x60, 2048); m.write16(0x800EC1E0 + 0x68, -2048)
            m.write16(0x800EC350, 0)
        h.run("RcCarUpdate", f"case-{i}", (), setup_car,
              {"solid": -100, "can_gadget": 1}, area=h.areas[i % len(h.areas)])
    def setup_car_surface(m, old_surface=0, impacts=0, steering_mode=0,
                          collision_flags=6, speed=5, aim=0):
        setup_car(m, active=1, timer=0, vertical=0, speed=speed, aim=aim,
                  contact=0, impacts=impacts)
        install_surface_one(m)
        m.write16(CAR + 0x76, old_surface)
        m.write16(0x800F44A0, steering_mode)
        m.write16(0x1F8003F8, collision_flags)
        m.write16(CAR + 0x60, 1200); m.write16(CAR + 0x64, -600)
    h.run("RcCarUpdate", "ground-impact-flags", (),
          lambda m: (setup_car(m, active=1, timer=0, vertical=0, speed=5, aim=0,
                               contact=0, impacts=0), m.write32(CAR + 0x28, 10000),
                     m.write16(0x1F8003F8, 1)),
          {"solid": -100, "can_gadget": 1}, area=h.areas[1])
    h.run("RcCarUpdate", "surface-transition", (),
          lambda m: setup_car_surface(m), {"solid": -100, "can_gadget": 1}, area=h.areas[2])
    h.run("RcCarUpdate", "surface-impact-limit", (),
          lambda m: setup_car_surface(m, old_surface=0x1000, impacts=30),
          {"solid": -100, "can_gadget": 1}, area=h.areas[3])
    h.run("RcCarUpdate", "steering-mode-gate", (),
          lambda m: setup_car_surface(m, steering_mode=2, speed=40, aim=500),
          {"solid": -100, "can_gadget": 1}, area=h.areas[4])
    h.run("RcCarUpdate", "aim-vector", (),
          lambda m: setup_car_surface(m, speed=40, aim=500),
          {"solid": -100, "can_gadget": 1}, area=h.areas[0])
    def setup_car_aim_boundary(m):
        setup_car_surface(m, speed=94, aim=100)
        m.write16(CAR + 0x60, 12042); m.write16(CAR + 0x64, 0)
    h.run("RcCarUpdate", "aim-divisor-boundary", (), setup_car_aim_boundary,
          {"solid": -100, "can_gadget": 1}, area=h.areas[1])
    def setup_car_slope(m):
        setup_car_surface(m, speed=40, aim=0)
        m.write32(CAR + 0x24, -1537); m.write32(CAR + 0x2C, -5120)
        m.write32(CAR + 0x28, 10000)
    h.run("RcCarUpdate", "slope-adjustment", (), setup_car_slope,
          {"solid": 32767, "can_gadget": 1}, area=h.areas[0])
    def setup_car_below_ground(m):
        setup_car(m, active=1, timer=0, vertical=0, speed=40, aim=0, contact=0, impacts=0)
        m.write32(CAR + 0x28, -32768)
    h.run("RcCarUpdate", "below-ground", (), setup_car_below_ground,
          {"solid": -100, "can_gadget": 1}, area=h.areas[3])
    def setup_car_gadget_disabled(m):
        setup_car(m, active=1, timer=0, vertical=0, speed=40, aim=0, contact=0, impacts=0)
        m.write32(CAR + 0x28, 10000)
        m.write8(0x800EC1E0 + 0xF2, 0)
    h.run("RcCarUpdate", "gadget-disabled", (), setup_car_gadget_disabled,
          {"solid": -100, "can_gadget": 1}, area=h.areas[4])
    def setup_car_speed_threshold(m):
        setup_car(m, active=1, timer=0, vertical=0, speed=30, aim=0, contact=0, impacts=0)
        m.write32(CAR + 0x38, 0)
    h.run("RcCarUpdate", "speed-square-900", (), setup_car_speed_threshold,
          {"solid": -100, "can_gadget": 1}, area=h.areas[1])
    def setup_car_contact_11(m):
        setup_car(m, active=1, timer=0, vertical=0, speed=40, aim=0, contact=11, impacts=0)
        m.write32(CAR + 0x28, 10000)
    h.run("RcCarUpdate", "contact-kind-11", (), setup_car_contact_11,
          {"solid": -100, "can_gadget": 1}, area=h.areas[0])

    # The dispatcher is validated after each of its leaves has independent cases.
    h.run("ProjectileSystemsUpdate", "dispatcher-empty", (), lambda m: None, area=h.areas[1])


# (tested original function, edited C function/helper, old text, replacement, label)
MUTATIONS = {
    "ParticlesDraw": [
        ("ae_ParticlesDraw", "width == 0u", "width == 1u", "zero-width gate"),
        ("ae_ParticlesDraw", "screen_y < -32", "screen_y < -31", "lower clip edge"),
        ("ae_ParticlesDraw", ">= 4095u) continue;", ">= 4094u) continue;", "depth limit"),
    ],
    "EffectScriptUpdate": [
        ("fx_script_command", "case 51:", "case 50:", "ground/water opcode"),
        ("ae_EffectScriptUpdate", "AE_FX_PLAYER_POSITION + 4u));", "AE_FX_PLAYER_POSITION + 2u));",
         "player Y read with a halfword stride"),
        ("fx_report_bad_script", "word < 6u", "word < 5u", "invalid-command dump"),
        ("fx_script_ground_or_water", "if (ground_y < particle_y)",
         "if (ground_y <= particle_y)", "ground equality"),
    ],
    "RibbonDraw": [
        ("ae_RibbonDraw", "164u * scale", "165u * scale", "width scale"),
        ("ae_RibbonDraw", "if (count < 2u)", "if (count <= 2u)", "minimum point count"),
        ("ae_RibbonDraw", "if (nx == 0 && ny == 0)", "if (nx == 0 && ny == 1)", "reversal fallback"),
    ],
    "ProjectileSystemsUpdate": [
        ("ae_ProjectileSystemsUpdate", "ae_ProjectileRecordsUpdate(r, h);",
         "ae_ProjectileGroupsUpdate(r, h);", "record dispatcher"),
        ("ae_ProjectileSystemsUpdate", "ae_ProjectileGroupsUpdate(r, h);",
         "ae_ProjectileRecordsUpdate(r, h);", "group dispatcher"),
        ("ae_ProjectileSystemsUpdate", "if (!ae_ram_failed(r))", "if (ae_ram_failed(r))",
         "failure gate"),
    ],
    "ProjectileExpire": [
        ("ae_ProjectileExpire", "AE_FX_PROJECTILE_RECORD_STATE, 1u", "AE_FX_PROJECTILE_RECORD_STATE, 2u",
         "dead state"),
        ("ae_ProjectileExpire", "AE_FX_PROJECTILE_RECORD_BODY, 0u", "AE_FX_PROJECTILE_RECORD_BODY, projectile",
         "body clear"),
        ("ae_ProjectileExpire", "projectile, reason, callback, 0",
         "projectile, reason + 1u, callback, 0", "callback reason"),
    ],
    "ProjectileRecordsUpdate": [
        ("ae_ProjectileRecordsUpdate", "(ae_rd8(r, record + AE_FX_PROJECTILE_RECORD_KIND + 1u) - 1u)",
         "(ae_rd8(r, record + AE_FX_PROJECTILE_RECORD_KIND + 1u) - 2u)", "record timer"),
        ("ae_ProjectileRecordsUpdate", "vertical = fx_asr32(-7 * vertical + 7, 3);",
         "vertical = fx_asr32(-7 * vertical + 6, 3);", "body10 damping"),
        ("ae_ProjectileRecordsUpdate", "motion_mode == 0u || motion_mode == 1u",
         "motion_mode == 0u", "gravity mode"),
    ],
    "ProjectileGroupSpawn": [
        ("ae_ProjectileGroupSpawn", "fx_div(r, component, 20,", "fx_div(r, component, 21,", "normalized direction"),
        ("ae_ProjectileGroupSpawn", "uint32_t mode = count - 1u;", "uint32_t mode = count;", "spawn mode"),
        ("ae_ProjectileGroupSpawn", "AE_FX_PROJECTILE_STATE_AIM_TARGET_Y) + 32u",
         "AE_FX_PROJECTILE_STATE_AIM_TARGET_Y) + 31u", "aim target offset"),
    ],
    "ProjectileGroupSpawnSpread": [
        ("ae_ProjectileGroupSpawnSpread", "if (mode != 3u)", "if (mode != 2u)", "spread mode"),
        ("ae_ProjectileGroupSpawnSpread", "index < 3u", "index < 2u", "spread count"),
        ("ae_ProjectileGroupSpawnSpread", "AE_FX_PROJECTILE_SPREAD_TABLE + index * 4u",
         "AE_FX_PROJECTILE_SPREAD_TABLE + index * 4u + 2u", "spread angle"),
    ],
    "ProjectileGroupsUpdate": [
        ("ae_ProjectileGroupsUpdate", "if (group_state == 5u) {\n            fx_projectile_spawn_at_group(r, h, 4, group);",
         "if (group_state == 4u) {\n            fx_projectile_spawn_at_group(r, h, 4, group);", "live-state particle"),
        ("ae_ProjectileGroupsUpdate", "ae_rd16(r, AE_FX_RANDOM_SEED) * 5u + 1u",
         "ae_rd16(r, AE_FX_RANDOM_SEED) * 6u + 1u", "state-1 random velocity"),
        ("fx_projectile_group_ground_step", "if (ae_rds16(r, group + AE_FX_PROJECTILE_GROUP_VELOCITY + 2u) < -19)",
         "if (ae_rds16(r, group + AE_FX_PROJECTILE_GROUP_VELOCITY + 2u) < -18)", "bounce threshold"),
    ],
    "RcCarUpdate": [
        ("ae_RcCarUpdate", "facing_x * (int32_t)aim_strength, 0x3000",
         "facing_x * (int32_t)aim_strength, 0x2FFF", "aim scale"),
        ("ae_RcCarUpdate", "if (aim_timer > 0)", "if (aim_timer >= 0)", "timer return"),
        ("ae_RcCarUpdate", "speed_squared >= 901", "speed_squared >= 900", "body speed threshold"),
    ],
    "FUN_8014022c": [
        ("ae_FUN_8014022c", "position, vector, 0, 0", "vector, position, 0, 0", "adapter arguments"),
        ("ae_FUN_8014022c", "return FX_CALL(r, h, FUN_8014022c, position, vector, 0, 0);",
         "return FX_CALL(r, h, FUN_8014022c, position, vector, 0, 0) + 1;", "adapter return"),
        ("ae_FUN_8014022c", "return FX_CALL(r, h, FUN_8014022c, position, vector, 0, 0);",
         "return 0;", "adapter hook"),
    ],
    "FUN_8014263c": [
        ("ae_FUN_8014263c", "position, vector, 0, 0", "vector, position, 0, 0", "adapter arguments"),
        ("ae_FUN_8014263c", "return FX_CALL(r, h, FUN_8014263c, position, vector, 0, 0);",
         "return FX_CALL(r, h, FUN_8014263c, position, vector, 0, 0) + 1;", "adapter return"),
        ("ae_FUN_8014263c", "return FX_CALL(r, h, FUN_8014263c, position, vector, 0, 0);",
         "return 0;", "adapter hook"),
    ],
}


def _function_span(source, symbol):
    pattern = re.compile(r"\b" + re.escape(symbol) + r"\s*\(")
    for match in pattern.finditer(source):
        open_paren = source.find("(", match.start())
        depth = 0
        close_paren = None
        for pos in range(open_paren, len(source)):
            if source[pos] == "(": depth += 1
            elif source[pos] == ")":
                depth -= 1
                if depth == 0:
                    close_paren = pos + 1
                    break
        if close_paren is None: continue
        while close_paren < len(source) and source[close_paren].isspace(): close_paren += 1
        if close_paren >= len(source) or source[close_paren] != "{": continue
        brace_depth = 0
        for pos in range(close_paren, len(source)):
            if source[pos] == "{": brace_depth += 1
            elif source[pos] == "}":
                brace_depth -= 1
                if brace_depth == 0: return match.start(), pos + 1
    raise RuntimeError(f"could not locate C function body for {symbol}")


def _mutate_function(source, symbol, old, new):
    start, end = _function_span(source, symbol)
    body = source[start:end]
    if body.count(old) != 1:
        raise RuntimeError(f"mutation for {symbol} expected one {old!r}, got {body.count(old)}")
    return source[:start] + body.replace(old, new, 1) + source[end:]


def _bind_library(h, lib):
    P, U, I = C.c_void_p, C.c_uint32, C.c_int32
    h.lib = lib
    lib.ae_ram_clear_error.argtypes = [P]
    lib.ae_ram_failed.argtypes = [P]; lib.ae_ram_failed.restype = I
    lib.ae_ram_error.argtypes = [P]; lib.ae_ram_error.restype = C.c_char_p
    for width, restype in ((8, U), (16, U), (32, U)):
        fn = getattr(lib, f"ae_rd{width}"); fn.argtypes = [P, U]; fn.restype = restype
        wr = getattr(lib, f"ae_wr{width}"); wr.argtypes = [P, U, U]
    for _name, (_address, _size, cname, ret, kinds) in FUNCS.items():
        fn = getattr(lib, cname)
        fn.argtypes = [P, P] + [I if kind == "s" else U for kind in kinds]
        fn.restype = None if ret == "v" else (U if ret == "u" else I)


def run_mutations(h, scale, seed):
    source_path = ROOT / "src/effects/ae_fxscript.c"
    base_source = source_path.read_text(encoding="utf-8")
    base_library = h.lib
    variant_dir = ROOT / "extract/mips"
    total = caught = 0
    for target, mutations in MUTATIONS.items():
        h.mutation_function = target
        for symbol, old, new, label in mutations:
            total += 1
            variant = _mutate_function(base_source, symbol, old, new)
            source = variant_dir / f"fxscript_mutation_{total}.c"
            source.write_text(variant, encoding="utf-8")
            dll = build_mutant(source, tag=f"fxmut{total}")
            source.unlink()
            mutant_lib = C.CDLL(str(dll))
            _bind_library(h, mutant_lib)
            h.failures = 0
            captured = io.StringIO()
            try:
                with redirect_stdout(captured):
                    scenarios(h, scale, seed)
            except SystemExit as error:
                if error.code != 1: raise
                caught += 1
                print(f"caught {total:02d}/{sum(map(len, MUTATIONS.values()))}: {target} / {label}")
            else:
                raise RuntimeError(f"mutation survived: {target} / {label}")
    h.mutation_function = None
    _bind_library(h, base_library)
    return caught, total


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--cases", type=float, default=1.0)
    parser.add_argument("--seed", type=int, default=94423)
    parser.add_argument("--mutate", action="store_true")
    args = parser.parse_args()
    start = time.time()
    h = Harness()
    scenarios(h, args.cases, args.seed)
    summary = h.finish(default_run=(args.cases == 1.0 and args.seed == 94423 and not args.mutate),
                       show_coverage=args.coverage)
    print("default cases:")
    for name, count in h.counts.items(): print(f"  {name:30s} {count:4d}")
    print(f"failures={h.failures} hooks={sum(h.counts.values())} elapsed={time.time()-start:.1f}s")
    if args.mutate:
        caught, total = run_mutations(h, args.cases, args.seed)
        print(f"mutations caught={caught}/{total}")
        if caught != total:
            raise SystemExit(1)
    if any(s["uncovered_count"] or s["stale_declared"] for s in summary.values() if s["own"]):
        raise SystemExit(4)


if __name__ == "__main__":
    main()
