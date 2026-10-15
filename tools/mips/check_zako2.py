"""Differential checks for the second zako/projectile and debug-text batch.

Usage: .venv/Scripts/python.exe tools/mips/check_zako2.py [--coverage] [--cases SCALE]
       .venv/Scripts/python.exe tools/mips/check_zako2.py --mutate
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
STACK_LO_ADDR, STACK_HI_ADDR = 0x801FF000, 0x80200000
PROJECTILES = 0x800B8D30
PROJECTILE_SIZE = 0x30
PROJECTILE_COUNT = 16
PLAYER = 0x800EC1E0
STAGE = 0x800F4350
PICKUP_STATE = 0x800B28B0
PICKUP_RANDOM_A = 0x800AE8F0
PICKUP_RANDOM_B = 0x800AE8F2
BODY_POOL_FREE = 0x800BEDA0
BODY_POOL_COUNT = 128
BODY_POOL = 0x800BD1A0
BODY_SIZE = 0x38
BODY_SLOTS = 0x800BBDA0
BODY_SLOT_SIZE = 0x14
BODY_SLOT_COUNT = 256
BODY_HEAD, BODY_TAIL = 0x800B20D8, 0x800B20DC
DEBUG_FADE = 0x800DF7D8
DEBUG_ENABLED = 0x800F460A
DEBUG_COLOR = 0x800DF7F8
DEBUG_PRIMITIVES = 0x800DF7FC
DEBUG_OT = 0x801003D0
FONT_TABLE = 0x800B3390
RENDER_STATE = 0x800EC350
RENDER_CONTEXT_SIZE = 0x4078
RENDER_OT = 0x800F04D0
RENDER_PATCH_PROJECTILE = 0x80059CD0
RENDER_PATCH_GLYPH_SMALL = 0x8008C474
RENDER_PATCH_GLYPH_LARGE = 0x8008C57C
RENDER_PATCH_TEXT_FINAL = 0x8008CA34

# address, bytes, C name, takes AeZako2Hooks, return value, C argument kinds
FUNCS = {
    "ProjectileAddHitBody": (0x80059A54, 180, "ae_ProjectileAddHitBody", True, False, "uiii"),
    "ProjectilesUpdateDraw": (0x80059B08, 76, "ae_ProjectilesUpdateDraw", True, False, ""),
    "ProjectilesDraw": (0x80059B54, 1104, "ae_ProjectilesDraw", True, False, ""),
    "ProjectilesUpdate": (0x80059FA4, 2896, "ae_ProjectilesUpdate", True, False, ""),
    "PickupPickKindA": (0x8008373C, 84, "ae_PickupPickKindA", False, True, ""),
    "PickupPickKindB": (0x80083790, 180, "ae_PickupPickKindB", False, True, ""),
    "SpawnPickups": (0x80083AA8, 528, "ae_SpawnPickups", True, False, "uuiu"),
    "ColorFadeSet": (0x8008C3C0, 28, "ae_ColorFadeSet", False, False, "ii"),
    "DebugTextDrawGlyph": (0x8008C434, 700, "ae_DebugTextDrawGlyph", True, False, "i"),
    "DebugTextPrintf": (0x8008C6F0, 996, "ae_DebugTextPrintf", True, False, "uu"),
    "DebugTextPrintfWhite": (0x8008CBFC, 100, "ae_DebugTextPrintfWhite", True, False, "uu"),
    "IsTimeAttack": (0x80097678, 64, "ae_IsTimeAttack", False, True, ""),
    "FUN_8013f660": (0x8013F660, 1, "ae_FUN_8013f660", True, False, "u"),
}

HOOK_COUNTS = {
    "GetSolidHeight": 3,
    "PlaySfx": 1,
    "DrawShadow": 4,
    "ModelTransformRigid": 3,
    "ModelDraw": 3,
    "FUN_8013f660": 1,
    "DrawProjectileSprite": 6,
    "DrawDebugGlyph": 6,
    "FinishDebugText": 6,
}
HOOK_POINTERS = {
    "DrawShadow": {0},
    "ModelTransformRigid": {0, 1, 2},
    "ModelDraw": {0, 1, 2},
    "DrawProjectileSprite": {0, 1, 3, 4},
    "DrawDebugGlyph": {4, 5},
    "FinishDebugText": {0, 1},
}

CALL_HOOK = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                        C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
RENDER_HOOK = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p,
                          C.c_uint32, C.c_uint32, C.c_uint32,
                          C.c_uint32, C.c_uint32, C.c_uint32)
WORLD_HOOK = CALL_HOOK
BODY_VOID2 = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32, C.c_uint32, C.c_uint32)
BODY_VOID1 = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32, C.c_uint32)
BODY_STUB = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p)
BODY_RAY = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_uint32,
                       C.c_uint32, C.POINTER(C.c_int16))
BODY_SFX = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_int32)
BODY_PLAY = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32, C.POINTER(C.c_int16))
BODY_NUDGE = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32, C.c_int32, C.c_int32)
SPAWN_PAD = C.CFUNCTYPE(C.c_uint16, C.c_void_p, C.c_void_p, C.c_uint32)
ITEM_COLOR = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p,
                          C.c_uint32, C.c_uint32, C.c_uint32)
ITEM_MATRIX = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32)
ITEM_TRIG = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_int32)


class BodyHooks(C.Structure):
    _fields_ = [
        ("user", C.c_void_p),
        ("printf_error", C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_int32)),
        ("sound_loop_start", C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_void_p, C.c_int32, C.c_int32)),
        ("sound_loop_stop", C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int32, C.c_int32)),
        ("body_move", BODY_VOID2), ("build_probe_points", BODY_VOID1),
        ("body_stub_return_zero", BODY_STUB), ("collision_ray_test", BODY_RAY),
        ("gimmick_type_to_sfx", BODY_SFX), ("play_sfx_at", BODY_PLAY),
        ("pickup_collect", BODY_SFX), ("camera_nudge", BODY_NUDGE),
    ]


class WorldHooks(C.Structure):
    _fields_ = [("user", C.c_void_p)] + [(name, WORLD_HOOK) for name in (
        "GetSolidHeight", "PlaySfxAt", "GimmickActivate", "MeshInitFromAreaTable",
        "FUN_8013d358", "CameraShake", "EventCamStart")] + [
            ("ground_stale_x", C.c_uint16), ("ground_stale_z", C.c_uint16)]


class SpawnHooks(C.Structure):
    _fields_ = [("user", C.c_void_p), ("world", C.POINTER(WorldHooks)),
                ("PlaySfx", WORLD_HOOK), ("MatrixLookDir", WORLD_HOOK),
                ("MatrixFromDirXZ", WORLD_HOOK), ("position_padding", SPAWN_PAD)]


class ItemsHooks(C.Structure):
    _fields_ = [("user", C.c_void_p), ("GetLevelFirstArea", CALL_HOOK),
                ("printf", CALL_HOOK), ("SetBackColor", ITEM_COLOR),
                ("SetFarColor", ITEM_COLOR), ("SetColorMatrix", ITEM_MATRIX),
                ("rsin", ITEM_TRIG), ("rcos", ITEM_TRIG)]


class Zako2Hooks(C.Structure):
    _fields_ = [("user", C.c_void_p), ("body_hooks", C.POINTER(BodyHooks)),
                ("world_hooks", C.POINTER(WorldHooks)), ("spawn_hooks", C.POINTER(SpawnHooks)),
                ("items_hooks", C.POINTER(ItemsHooks))] + [
                    (name, CALL_HOOK) for name in ("PlaySfx", "DrawShadow", "ModelTransformRigid",
                                                   "ModelDraw", "FUN_8013f660")] + [
                    (name, RENDER_HOOK) for name in ("DrawProjectileSprite", "DrawDebugGlyph",
                                                     "FinishDebugText")]


def s32(value):
    return mr.signed(value, 32)


def s16(value):
    return mr.signed(value, 16)


def normalize_pointer(value):
    value &= M32
    return "STACK" if STACK_LO_ADDR <= value < STACK_HI_ADDR else value


def build(source=None, trapv=False):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"zako2_{os_getpid()}_{time.time_ns()}.dll"
    srcs = [ROOT / "src/core/ae_ram.c", ROOT / "src/core/ae_sdk.c",
            ROOT / "src/core/ae_sdk_matrix.c", ROOT / "src/world/ae_world.c",
            ROOT / "src/world/ae_solid.c", ROOT / "src/bodies/ae_bodies.c",
            ROOT / "src/player/native_player.c", ROOT / "src/effects/ae_spawn.c",
            ROOT / "src/world/ae_items_init.c", Path(source) if source else ROOT / "src/world/ae_zako2.c"]
    flags = ["-O1", "-ftrapv"] if trapv else ["-O2"]
    subprocess.run([shutil.which("gcc"), "-std=c11", *flags, "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT / "src/player"), "-I", str(ROOT / "src/world"),
                    "-shared", "-static-libgcc",
                    *map(str, srcs), "-o", str(dll), "-lm"], check=True)
    return dll


def os_getpid():
    import os
    return os.getpid()


class Side:
    def __init__(self, dll):
        self.lib = lib = C.CDLL(str(dll))
        P, U, I = C.c_void_p, C.c_uint32, C.c_int32
        lib.ae_ram_create.restype = P
        lib.ae_ptr.argtypes = [P, U, U]
        lib.ae_ptr.restype = P
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_failed.restype = C.c_int
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        lib.ae_ram_clear_error.argtypes = [P]
        lib.ae_rd8.argtypes = [P, U]; lib.ae_rd8.restype = U
        lib.ae_rd16.argtypes = [P, U]; lib.ae_rd16.restype = U
        lib.ae_rd32.argtypes = [P, U]; lib.ae_rd32.restype = U
        lib.ae_wr8.argtypes = [P, U, U]
        lib.ae_wr16.argtypes = [P, U, U]
        lib.ae_wr32.argtypes = [P, U, U]
        self.ram = lib.ae_ram_create()
        if not self.ram:
            raise SystemExit("ae_ram_create failed")
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scr = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.model = None
        self.keep = []

        self.body_hooks = BodyHooks()
        self.world_hooks = WorldHooks()
        self.spawn_hooks = SpawnHooks()
        self.items_hooks = ItemsHooks()
        self.world_hooks.ground_stale_x = 0
        self.world_hooks.ground_stale_z = 0
        self.spawn_hooks.world = C.pointer(self.world_hooks)
        self.spawn_hooks.position_padding = SPAWN_PAD(lambda _u, _r, _f: 0)
        self.keep.append(self.spawn_hooks.position_padding)

        ground_cb = WORLD_HOOK(lambda _u, _r, x, y, z, _w: self.model.call("GetSolidHeight", x, y, z))
        self.world_hooks.GetSolidHeight = ground_cb
        self.keep.append(ground_cb)

        self.zako_hooks = Zako2Hooks()
        self.zako_hooks.body_hooks = C.pointer(self.body_hooks)
        self.zako_hooks.world_hooks = C.pointer(self.world_hooks)
        self.zako_hooks.spawn_hooks = C.pointer(self.spawn_hooks)
        self.zako_hooks.items_hooks = C.pointer(self.items_hooks)
        self.zako_callbacks = {}
        for name in ("PlaySfx", "DrawShadow", "ModelTransformRigid", "ModelDraw", "FUN_8013f660"):
            callback = CALL_HOOK(lambda _u, _r, a0, a1, a2, a3, hook_name=name:
                                 self.model.call(hook_name, a0, a1, a2, a3))
            setattr(self.zako_hooks, name, callback)
            self.zako_callbacks[name] = callback
        for name in ("DrawProjectileSprite", "DrawDebugGlyph", "FinishDebugText"):
            callback = RENDER_HOOK(lambda _u, _r, a0, a1, a2, a3, a4, a5, hook_name=name:
                                   self.model.call(hook_name, a0, a1, a2, a3, a4, a5))
            setattr(self.zako_hooks, name, callback)
            self.zako_callbacks[name] = callback

        for name, (_, _, cname, takes_hooks, returns_value, kinds) in FUNCS.items():
            fn = getattr(lib, cname)
            fn.argtypes = [P] + ([P] if takes_hooks else []) + [I if k in "is" else U for k in kinds]
            fn.restype = I if returns_value else None

    def configure(self, model):
        self.model = model

    def sync_from(self, snap):
        C.memmove(self.main, snap[0], mr.RAM_SIZE)
        C.memmove(self.scr, snap[1], mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)

    def read8(self, address):
        return self.lib.ae_rd8(self.ram, address)

    def read16(self, address):
        return self.lib.ae_rd16(self.ram, address)

    def read32(self, address):
        return self.lib.ae_rd32(self.ram, address)

    def call(self, name, args):
        _, _, cname, takes_hooks, _, kinds = FUNCS[name]
        converted = [s32(value) if kind in "is" else value & M32 for value, kind in zip(args, kinds)]
        fn = getattr(self.lib, cname)
        return fn(self.ram, C.byref(self.zako_hooks), *converted) if takes_hooks else fn(self.ram, *converted)


class Memory:
    def __init__(self, side):
        self.side = side

    def rd8(self, address): return self.side.read8(address)
    def rd16(self, address): return self.side.read16(address)
    def rd32(self, address): return self.side.read32(address)
    def wr8(self, address, value): self.side.lib.ae_wr8(self.side.ram, address, value & 0xFF)
    def wr16(self, address, value): self.side.lib.ae_wr16(self.side.ram, address, value & 0xFFFF)
    def wr32(self, address, value): self.side.lib.ae_wr32(self.side.ram, address, value & M32)


class MipsMemory:
    def __init__(self, cpu): self.cpu = cpu
    def rd8(self, address): return self.cpu.read8(address)
    def rd16(self, address): return self.cpu.read16(address)
    def rd32(self, address): return self.cpu.read32(address)
    def wr8(self, address, value): self.cpu.write8(address, value & 0xFF)
    def wr16(self, address, value): self.cpu.write16(address, value & 0xFFFF)
    def wr32(self, address, value): self.cpu.write32(address, value & M32)


class Model:
    def __init__(self, mem, solid_height=32767):
        self.mem = mem
        self.solid_height = solid_height
        self.log = []

    def call(self, name, *values):
        args = [value & M32 for value in values[:HOOK_COUNTS[name]]]
        payload = ()
        if name in HOOK_POINTERS:
            for index in HOOK_POINTERS[name]:
                args[index] = normalize_pointer(args[index])
        if name == "DrawShadow":
            pointer = values[0] & M32
            payload = tuple(s16(self.mem.rd16(pointer + offset))
                            if offset in (0, 4, 8) else None for offset in (0, 4, 8))
        self.log.append((name, *args, payload))
        if name == "GetSolidHeight":
            return self.solid_height
        return 0


def set_area_grid(cpu, member):
    blob = (ROOT / f"extract/members/{member:03X}.bin").read_bytes()
    area_offset = struct.unpack_from("<I", blob, 0x4D0)[0]
    grid_offset = area_offset + 4 * struct.unpack_from("<I", blob, area_offset + 4)[0]
    width, depth, collider_count = struct.unpack_from("<HHI", blob, grid_offset)
    cells = grid_offset + 8
    heights = cells + width * depth * 12 + collider_count * 4
    cpu.write32(0x800F44E4, mr.AREA_BASE + area_offset)
    cpu.write16(0x800EBFA0, width)
    cpu.write16(0x800EBFA2, depth)
    cpu.write32(0x800EBFA8, mr.AREA_BASE + cells)
    cpu.write32(0x800EBFB4, mr.AREA_BASE + heights)


def clear_body_pool(cpu):
    for index in range(BODY_SLOT_COUNT):
        slot = BODY_SLOTS + index * BODY_SLOT_SIZE
        cpu.write16(slot, 0xFFFF)
        cpu.write16(slot + 2, 0xFFFF)
        for offset in (4, 8, 12, 16): cpu.write32(slot + offset, 0)
    for index in range(BODY_POOL_COUNT):
        cpu.write16(0x800BEDA0 + 2 * index, 0xFFFF)
    cpu.write32(BODY_HEAD, 0)
    cpu.write32(BODY_TAIL, 0)


def record_address(index=0):
    return PROJECTILES + index * PROJECTILE_SIZE


def prep_projectile(cpu, index, kind, *, cooldown=0, age=0, timer=0,
                    position=(256, 128, 256), velocity=(0, 0, 0), source=0x800EC1E0,
                    ground=0, contact=0):
    p = record_address(index)
    cpu.write32(p, 0)
    cpu.write32(p + 4, 0)
    cpu.write32(p + 8, source)
    cpu.write8(p + 0x0C, kind)
    cpu.write8(p + 0x0D, 0)
    cpu.write8(p + 0x0E, cooldown)
    cpu.write8(p + 0x0F, 0)
    cpu.write8(p + 0x10, index & 0x0F)
    cpu.write8(p + 0x11, 0)
    cpu.write8(p + 0x12, 0)
    cpu.write8(p + 0x13, age)
    cpu.write8(p + 0x14, 0)
    cpu.write8(p + 0x15, 0)
    for offset, value in zip((0x1C, 0x1E, 0x20), position): cpu.write16(p + offset, value)
    for offset, value in zip((0x24, 0x26, 0x28), velocity): cpu.write16(p + offset, value)
    cpu.write16(p + 0x2A, ground)
    cpu.write16(p + 0x2E, timer)
    if contact:
        body = BODY_POOL
        cpu.write32(p, body)
        cpu.write32(body + 0x2C, contact << 28)


def setup_globals(cpu, seed=0):
    clear_body_pool(cpu)
    for offset in range(0, PROJECTILE_COUNT * PROJECTILE_SIZE, 4):
        cpu.write32(PROJECTILES + offset, 0)
    cpu.write32(PLAYER + 0x24, 256)
    cpu.write32(PLAYER + 0x28, 128)
    cpu.write32(PLAYER + 0x2C, 256)
    cpu.write32(PLAYER + 0x30, 0)
    cpu.write32(PLAYER + 0x34, 0)
    cpu.write32(PLAYER + 0x38, 0)
    cpu.write16(PICKUP_RANDOM_A, seed & 0xFFFF)
    cpu.write16(PICKUP_RANDOM_B, (seed ^ 0xA55A) & 0xFFFF)
    cpu.write8(PICKUP_STATE, 0)
    cpu.write8(PICKUP_STATE + 1, 0)
    cpu.write32(0x800F4450, 0)
    cpu.write32(0x800FC354, 0x800F7000)
    cpu.write32(RENDER_STATE + 0x0C, 0)
    cpu.write32(0x800DF7FC, 0)
    cpu.write16(DEBUG_ENABLED, 1)
    cpu.write16(0x800F448C, 0)
    cpu.write32(0x800F51C4, 8)
    cpu.write16(0x800EC2C8, 0)
    cpu.write16(0x800F4470, 0)
    cpu.write16(0x800F4472, 0)


def set16(cpu, address, value): cpu.write16(address, value & 0xFFFF)
def set32(cpu, address, value): cpu.write32(address, value & M32)


class Harness:
    def __init__(self, side, areas):
        self.side = side
        self.areas = areas
        self.mips = {}
        for area_id, member in areas:
            cpu = mr.MipsRam.from_extract(area=ROOT / f"extract/members/{member:03X}.bin")
            if not self.mips:
                cpu.attach_gte_oracle()
            else:
                oracle = next(iter(self.mips.values()))
                cpu.gte_backend = oracle.gte_backend
                cpu.gte_state = oracle.gte_state
                cpu._gte_lib = oracle._gte_lib
            cpu.coverage = set()
            set_area_grid(cpu, member)
            self.mips[member] = cpu
        self.failures = 0
        self.counts = {name: 0 for name in FUNCS}
        self.fixtures = {}
        self.patched_pcs = set()
        reg.declare("stack_window", "801FF000..80200000")

    def _patches(self, model):
        def patch_projectile(cpu, regs):
            projectile = (regs[16] - 0x16) & M32
            kind = cpu.read8(projectile + 0x0C)
            render = RENDER_STATE
            model.call("DrawProjectileSprite", projectile, projectile + 0x1C, kind,
                       render, cpu.read32(render + 4), 0)
            return 0x80059F64

        def patch_glyph_small(cpu, regs):
            glyph = s32(regs[11])
            packet = cpu.read32(RENDER_STATE + 4)
            cursor = cpu.read32(DEBUG_FADE + 8)
            y = cpu.read32(DEBUG_FADE + 12)
            if glyph != 0:
                ot_index = cpu.read32(RENDER_STATE + 0x0C)
                ot = RENDER_OT + ot_index * RENDER_CONTEXT_SIZE
                model.call("DrawDebugGlyph", glyph, cursor, y, cpu.read32(DEBUG_COLOR), ot, packet)
                packet += 0x14
            cpu.write32(DEBUG_FADE + 8, cursor + 8)
            regs[6] = packet & M32
            cpu.write32(RENDER_STATE + 4, packet)
            return 0x8008C570

        def patch_glyph_large(cpu, regs):
            glyph = s32(regs[11])
            cursor = cpu.read32(DEBUG_FADE + 8)
            y = cpu.read32(DEBUG_FADE + 12)
            glyph_data = (FONT_TABLE + glyph * 2) & M32
            packet = cpu.read32(RENDER_STATE + 4)
            model.call("DrawDebugGlyph", glyph, cursor, y, cpu.read32(DEBUG_COLOR), glyph_data, packet)
            cpu.write32(DEBUG_FADE + 8, cursor + cpu.read8(glyph_data + 1))
            regs[8] = (packet + 0x28) & M32
            cpu.write32(RENDER_STATE + 4, packet + 0x28)
            return 0x8008C6E4

        def patch_text_final(cpu, regs):
            packet = cpu.read32(RENDER_STATE + 4)
            ot = regs[23] & M32
            model.call("FinishDebugText", ot, packet, cpu.read32(DEBUG_FADE),
                       cpu.read32(DEBUG_FADE + 4), cpu.read32(DEBUG_COLOR),
                       cpu.read32(DEBUG_PRIMITIVES))
            cpu.write32(RENDER_STATE + 4, packet + 8)
            return 0x8008CAA4

        return {
            RENDER_PATCH_PROJECTILE: patch_projectile,
            RENDER_PATCH_GLYPH_SMALL: patch_glyph_small,
            RENDER_PATCH_GLYPH_LARGE: patch_glyph_large,
            RENDER_PATCH_TEXT_FINAL: patch_text_final,
        }

    def run(self, name, mips_args=(), c_args=(), setup=lambda _m: None,
            label="case", area_member=None, initial_regs=None, capture=True):
        area_member = area_member or self.areas[0][1]
        cpu = self.mips[area_member]
        base = cpu.snapshot()
        cpu.restore(base)
        cpu.gte_reset()
        setup(cpu)
        before = cpu.snapshot()
        mmodel = Model(MipsMemory(cpu))
        self.side.configure(None)
        cpu.hook_log.clear()
        hooks = {
            0x8004D8C0: lambda m, regs: mmodel.call("GetSolidHeight", *regs[4:7]),
            0x800877F8: lambda m, regs: mmodel.call("PlaySfx", regs[4]),
            0x8001F604: lambda m, regs: mmodel.call("DrawShadow", *regs[4:8]),
            0x800161C0: lambda m, regs: mmodel.call("ModelTransformRigid", *regs[4:7]),
            0x80017CD4: lambda m, regs: mmodel.call("ModelDraw", *regs[4:7]),
            0x8013F660: lambda m, regs: mmodel.call("FUN_8013f660", regs[4]),
        }
        patches = self._patches(mmodel)
        for start, end in ((0x80059CD0, 0x80059F60),
                           (0x8008C474, 0x8008C56C),
                           (0x8008C57C, 0x8008C6E0),
                           (0x8008CA34, 0x8008CA9C)):
            self.patched_pcs.update(range(start, end + 1, 4))
        try:
            mregs = cpu.call(FUNCS[name][0], mips_args, hooks=hooks, patches=patches,
                             initial_regs=initial_regs)
            mret = mregs[2]
            merror = None
        except mr.MipsError as error:
            mret, merror = None, str(error)
        mram, mscratch = bytes(cpu.ram), bytes(cpu.scratch)

        self.side.sync_from(before)
        cmodel = Model(Memory(self.side), mmodel.solid_height)
        self.side.configure(cmodel)
        cret = self.side.call(name, c_args)
        self.counts[name] += 1
        if capture and name not in self.fixtures:
            self.fixtures[name] = (mips_args, c_args, setup, label, area_member, initial_regs)

        problems = []
        if merror:
            problems.append("MIPS harness error: " + merror)
        if self.side.lib.ae_ram_failed(self.side.ram):
            problems.append("C error flag: " + self.side.lib.ae_ram_error(self.side.ram).decode())
        if FUNCS[name][4] and mret != (cret & M32):
            problems.append(f"return MIPS {mret & M32:08X} C {cret & M32:08X}")
        if mmodel.log != cmodel.log:
            problems.append(f"hook calls differ\n      MIPS {mmodel.log}\n      C    {cmodel.log}")
        cram = bytes(self.side.main)
        cscratch = bytes(self.side.scr)
        if mram[:STACK_LO] != cram[:STACK_LO]:
            diffs = [i for i, (a, b) in enumerate(zip(mram[:STACK_LO], cram[:STACK_LO])) if a != b]
            problems.append("RAM differs at " + ", ".join(
                f"{0x80000000+i:08X}:{mram[i]:02X}/{cram[i]:02X}" for i in diffs[:10]))
        if mscratch != cscratch:
            diffs = [i for i, (a, b) in enumerate(zip(mscratch, cscratch)) if a != b]
            problems.append("scratchpad differs at " + ", ".join(
                f"{mr.SCRATCH_BASE+i:08X}:{mscratch[i]:02X}/{cscratch[i]:02X}" for i in diffs[:10]))
        if problems:
            self.failures += 1
            if self.failures <= 25:
                print(f"FAIL {name} [{label}]")
                for problem in problems: print("  " + problem)
        return not problems

    def finish(self, default_run, show_coverage):
        items = [(name, *spec[:2], True) for name, spec in FUNCS.items()]
        coverage = set().union(*(cpu.coverage for cpu in self.mips.values()))
        summary = reg.coverage_summary(items, coverage,
                                       patched=self.patched_pcs)
        if show_coverage:
            reg.print_coverage(summary)
        if default_run:
            reg.write_results(self.counts, summary, not self.failures, function_names=list(FUNCS))
        return summary


def set_format(cpu, address, value):
    raw = value.encode("ascii") + b"\0"
    cpu.write_bytes(address, raw)


def run_cases(h, scale=1.0, seed=94423):
    rng = random.Random(seed)
    member_for = lambda index: h.areas[index % len(h.areas)][1]

    for index, (dx, dy, dz) in enumerate(((0, 0, 0), (200, 1, 513), (-32768, 32767, -1), (128, 1536, 0))):
        def setup(m, i=index):
            setup_globals(m, seed + i)
            prep_projectile(m, 0, 6, position=(rng.randint(-1000, 1000), rng.randint(-1000, 1000), 257))
        h.run("ProjectileAddHitBody", (record_address(), dx, dy, dz),
              (record_address(), dx, dy, dz), setup, f"hit body fields {index}", member_for(index))

    for index, flags in enumerate((0, 2, 6, 1)):
        def setup(m, value=flags, i=index):
            setup_globals(m, seed + i)
            m.write32(0x800F4450, value)
            prep_projectile(m, 0, 5, timer=0, position=(260, 100, 260), velocity=(0, 1, 0))
            m.write32(RENDER_STATE + 4, 0x800F7000)
        h.run("ProjectilesUpdateDraw", (), (), setup, f"update/draw guard {flags}", member_for(index))

    draw_kinds = (0, 5, 6, 7, 9, 10)
    for index, kind in enumerate(draw_kinds):
        def setup(m, k=kind, i=index):
            setup_globals(m, seed + i)
            prep_projectile(m, 0, k, age=0, timer=3, position=(256 + i * 16, 100, 320),
                            velocity=(10, -3, 8), ground=100)
            if i % 2:
                prep_projectile(m, 1, 5, age=1)
            m.write32(RENDER_STATE + 4, 0x800F7000)
            m.write32(RENDER_STATE + 0x0C, i % 3)
        h.run("ProjectilesDraw", (), (), setup, f"draw kind {kind}", member_for(index))

    update_cases = [
        (1, 0, 0, (256, 0, 256), (0, 0, 0), 1, 0),
        (2, 0, 0, (256, 0, 256), (0, 0, 0), 3, 0),
        (3, 0, 0, (256, 64, 256), (0, 0, 0), 3, 0),
        (4, 0, 1, (256, 64, 256), (0, 0, 0), 3, 0),
        (4, 0, 0, (256, 64, 256), (0, 0, 0), 0, 0),
        (5, 0, 1, (256, 200, 256), (0, -8, 0), 0, 0),
        (5, 0, 0, (256, 40, 256), (0, 10, 0), 0, 0),
        (5, 0, 0, (256, 50, 256), (0, 0, 0), 0, 0),
        (5, 0, 1, (256, 128, 256), (0, 0, 0), 0, 0),
        (5, 0, 59, (256, 128, 256), (0, 0, 0), 0, 0),
        (5, 0, 60, (256, 128, 256), (0, 0, 0), 0, 0),
        (5, 0, 89, (256, 128, 256), (0, 0, 0), 0, 0),
        (6, 0, 0, (256, 0, 256), (4, 2, -3), 0, 0),
        (6, 0, 90, (256, 0, 256), (4, 2, -3), 6, 0),
        (6, 0, 2, (256, 100, 256), (4, 2, -3), 6, 0),
        (6, 0, 0, (256, -3000, 256), (4, 2, -3), 0, 0),
        (6, 0, 3, (256, -3000, 256), (4, 2, -3), 0, 0),
        (6, 0, 0, (256, -3000, 256), (4, 2, -3), 6, 0),
        (6, 0, 90, (256, -3000, 256), (4, 2, -3), 0, 0),
        (6, 0, 1, (256, -1000, 256), (0, 0, 0), 0, 0),
        (6, 0, 1, (256, -3000, 256), (2000, 2000, -2000), 0, 0),
        (6, 0, 1, (256, -3000, 256), (-2000, -2000, 2000), 0, 0),
        (7, 0, 0, (256, 10, 256), (2, -8, 3), 0, 0),
        (7, 0, 1023, (256, 10, 256), (2, -8, 3), 0, 0),
        (7, 0, 2, (256, 200, 256), (2, 12, 3), 6, 0),
        (7, 0, 0, (256, 32767, 256), (-5, 0, -7), 0, 0),
        (7, 0, 1024, (256, -2000, 256), (0, 0, 0), 0, 0),
        (7, 0, 1024, (256, 32755, 256), (0, 12, 0), 0, 0),
        (7, 0, 1024, (256, 32751, 256), (0, 16, 0), 0, 0),
        (7, 0, 3, (256, 100, 256), (-5, 12, -7), 6, 0),
        (7, 0, 0, (256, -2000, 256), (5, -8, 7), 6, 0),
        (7, 0, 0, (256, 100, 256), (0, 383, 0), 0, 0),
        (8, 0, 0, (256, 100, 256), (2, -8, 3), 0, 0),
        (8, 0, 1023, (256, 100, 256), (2, -8, 3), 0, 0),
        (8, 0, 1024, (256, 32755, 256), (0, 12, 0), 0, 0),
        (8, 0, 1024, (256, 32747, 256), (0, 20, 0), 0, 0),
        (8, 0, 0, (256, 32767, 256), (-5, 0, -7), 0, 0),
        (9, 0, 0, (256, 100, 256), (2, -8, 3), 0, 0),
        (9, 0, 1024, (256, 32755, 256), (0, 12, 0), 0, 0),
        (9, 0, 1024, (256, 32747, 256), (0, 20, 0), 0, 0),
        (9, 0, 0, (256, 32767, 256), (-5, 0, -7), 0, 0),
        (10, 0, 0, (256, 100, 256), (0, 0, 0), 3, 0),
        (10, 0, 0, (256, 100, 256), (0, 0, 0), 0, 1),
        (10, 0, 0, (256, 100, 256), (0, 0, 0), 0, 2),
        (1, 0, 0, (256, 0, 256), (0, 0, 0), 0, 1),
        (2, 0, 0, (256, 0, 256), (0, 0, 0), 0, 2),
        (12, 0, 0, (256, 100, 256), (0, 0, 0), 0, 0),
    ]
    for index, (kind, cooldown, timer, pos, vel, contact, source_kind) in enumerate(update_cases):
        def setup(m, spec=(kind, cooldown, timer, pos, vel, contact, source_kind), i=index):
            setup_globals(m, seed + i)
            k, cd, tm, p, v, ct, sk = spec
            prep_projectile(m, 0, k, cooldown=cd, timer=tm, position=p, velocity=v,
                            source=0x800EC1E0, ground=50, contact=ct)
            m.write8(0x800EC1E0, sk)
            projectile = record_address()
            if k == 5 and p[1] in (40, 50):
                m.write8(projectile + 0x12, 1)
            if k == 6 and tm == 0 and p[1] == -3000 and ct == 0:
                m.write8(projectile + 0x0D, 1)
            if k == 6 and tm == 3 and p[1] == -3000:
                m.write8(projectile + 0x0F, 1)
                m.write8(projectile + 0x0D, 1)
            if k == 6 and p[1] == -1000 and tm == 1:
                m.write32(PLAYER + 0x28, p[1] + 128)
        h.run("ProjectilesUpdate", (), (), setup,
              f"update #{index} kind {kind} timer {timer}", member_for(index))

    for index, (value, unlock) in enumerate(((0, 0), (1, 8), (0x1234, 8), (0xFFFF, 0), (0xFF00, 8))):
        def setup(m, v=value, u=unlock, i=index):
            setup_globals(m, seed + i)
            m.write16(PICKUP_RANDOM_A, v)
            m.write32(0x800F51C4, u)
        h.run("PickupPickKindA", (), (), setup, f"pick A seed {value:04X}", member_for(index))

    for index, (value, collected, active) in enumerate(((0, 0, 0), (0xFFFF, 0, 0), (0x1234, 3, 0),
                                                         (0xBEEF, 10, 0), (0x4321, 200, 1))):
        def setup(m, v=value, c=collected, a=active, i=index):
            setup_globals(m, seed + i)
            m.write16(PICKUP_RANDOM_B, v)
            m.write16(0x800F448C, c)
            m.write8(PICKUP_STATE, a)
        h.run("PickupPickKindB", (), (), setup, f"pick B seed/count {value:04X}/{collected}", member_for(index))

    spawn_cases = [(3, 0, 0, 0), (2, 1, 1, 0), (7, 2, 2, 0), (3, 3, 3, 1),
                   (4, 0, 0, 2), (5, 1, 0, 0), (2, 4, 2, 1), (7, 2, 8, 0),
                   (6, 1, 1, 0), (2, 3, 4, 0)]
    for index, (kind, mode, choose, time_attack) in enumerate(spawn_cases):
        def setup(m, values=(kind, mode, choose, time_attack), i=index):
            setup_globals(m, seed + i)
            selected, spawn_mode, flag, ta = values
            m.write16(0x800F4470, 0x13 if ta else 0)
            m.write16(0x800F4472, 0)
            m.write8(PICKUP_STATE + 1, 0)
            m.write16(PICKUP_RANDOM_A, 0x1000 + i)
            m.write16(PICKUP_RANDOM_B, 0xA000 + i)
            m.write8(0x800BF2F8, 0)
            for j in range(25):
                m.write8(0x800B28A2 + j, 0)
            if flag != 0:
                difficulty = flag if flag != 1 else 0
                row = 0x800B28A2 + difficulty * 25
                for j in range(25):
                    m.write8(row + j, 0)
            for j, k in enumerate((3, 2, 3, 6, 7)):
                m.write8(0x800B28B4 + j, k)
            if flag != 0:
                chosen_bucket = (None if flag == 4 else 3 if flag == 1 else
                                 4 if flag == 2 and i == 6 else
                                 {2: 1, 3: 0, 8: 3}.get(flag, i % 5))
                if chosen_bucket is not None:
                    m.write8(row + chosen_bucket * 5, 255)
            m.write16(0x800EC2C8, 0)
            for j in range(11): m.write8(0x800F7000 + j, 0)
        pos = 0x800F7020
        cargs = (kind, pos, mode, choose)
        h.run("SpawnPickups", cargs, cargs, setup, f"spawn kind/mode/choose {kind}/{mode}/{choose}",
              member_for(index))

    for index, pair in enumerate(((0, 0), (0x123456, -7), (-1, 0x7FFFFFFF), (0x7FFFFFFF, -1))):
        def setup(m, i=index): setup_globals(m, seed + i)
        h.run("ColorFadeSet", pair, pair, setup, f"fade {index}", member_for(index))

    for index, (glyph, count) in enumerate(((0, 0), (1, 0), (36, 3), (12, 4), (47, 6), (79, 10), (80, 0), (100, 5))):
        def setup(m, g=glyph, c=count, i=index):
            setup_globals(m, seed + i)
            m.write32(DEBUG_FADE, 0x00FFFFFF)
            m.write32(DEBUG_FADE + 4, 4)
            m.write32(DEBUG_FADE + 8, 24)
            m.write32(DEBUG_FADE + 12, 48)
            m.write32(DEBUG_COLOR, 0x00336699)
            m.write32(DEBUG_PRIMITIVES, c)
            m.write32(RENDER_STATE + 4, 0x800F7000)
            m.write32(RENDER_STATE + 0x0C, i % 4)
            m.write8(FONT_TABLE + (g & 0xFF) * 2 + 1, 7 + (i & 3))
        h.run("DebugTextDrawGlyph", (glyph,), (glyph,), setup, f"glyph {glyph} count {count}", member_for(index))

    fmt_cases = [
        ("hello!", ()), ("A\nB", ()), ("%d", (-12345,)), ("%5d", (42,)),
        ("%c-%x", (65, 0xFEED)), ("%s", (0x800F7100,)),
        ("`3red", ()), ("%qZ", ()), ("%d:%d", (-2147483648, 0)),
        ("%D", (18,)), ("%x", (0x1234,)),
    ]
    for index, (fmt, values) in enumerate(fmt_cases):
        format_address = 0x800F7200
        string_address = 0x800F7100
        args_regs = list(values[:3])
        init_regs = {5 + i: value & M32 for i, value in enumerate(args_regs)}
        mips_args = (format_address,)
        cargs = (format_address, mr.DEFAULT_SP)
        def setup(m, f=fmt, vals=values, i=index):
            setup_globals(m, seed + i)
            set_format(m, format_address, f)
            set_format(m, string_address, "a`2b")
            m.write16(DEBUG_ENABLED, 1)
            m.write32(DEBUG_FADE + 8, 0)
            m.write32(DEBUG_FADE + 12, 0)
            m.write32(RENDER_STATE + 4, 0x800F7000)
            m.write32(RENDER_STATE + 0x0C, i % 4)
            for j, value in enumerate(vals): m.write32(mr.DEFAULT_SP + 4 + 4 * j, value)
        h.run("DebugTextPrintf", mips_args, cargs, setup, f"printf {fmt!r}", member_for(index), init_regs)

    white_cases = [("white:%d", (23,)), ("`4X", ()), ("%s", (0x800F7100,))]
    for index, (fmt, values) in enumerate(white_cases):
        format_address = 0x800F7200
        init_regs = {5 + i: value & M32 for i, value in enumerate(values[:3])}
        def setup(m, f=fmt, vals=values, i=index):
            setup_globals(m, seed + i)
            set_format(m, format_address, f)
            set_format(m, 0x800F7100, "ok")
            m.write16(DEBUG_ENABLED, 0)
            m.write32(RENDER_STATE + 4, 0x800F7000)
            for j, value in enumerate(vals): m.write32(mr.DEFAULT_SP + 4 + 4 * j, value)
        h.run("DebugTextPrintfWhite", (format_address,), (format_address, mr.DEFAULT_SP), setup,
              f"printf white {fmt!r}", member_for(index), init_regs)

    for index, (mode, area) in enumerate(((0, 0), (0x13, 0), (0, 3), (0x13, 3))):
        def setup(m, mo=mode, ar=area, i=index):
            setup_globals(m, seed + i)
            m.write16(0x800F4470, mo)
            m.write16(0x800F4472, ar)
        h.run("IsTimeAttack", (), (), setup, f"time attack {mode:04X}/{area}", member_for(index))

    for index, object_address in enumerate((0x800EC1E0, 0x800ECA40, 0x80154000, 0xFFFFFFFF)):
        def setup(m, i=index): setup_globals(m, seed + 700 + i)
        h.run("FUN_8013f660", (object_address,), (object_address,), setup,
              f"runtime overlay hook argument {index}", member_for(index))

    # Deterministic randomized edge cases expand the projectile loop and the pickup LCG.
    for index in range(max(0, int(round(12 * scale)))):
        kind = rng.randint(1, 10)
        cooldown = rng.choice((0, 0, 1, 2, 255))
        timer = rng.choice((0, 1, 59, 60, 89, 90, 1023, 1024))
        pos = tuple(rng.randint(-256, 256) for _ in range(3))
        vel = tuple(rng.randint(-24, 24) for _ in range(3))
        def setup(m, values=(kind, cooldown, timer, pos, vel), i=index):
            setup_globals(m, seed + 500 + i)
            k, cd, tm, p, v = values
            prep_projectile(m, 0, k, cooldown=cd, timer=tm, position=p, velocity=v,
                            ground=0, contact=rng.choice((0, 0, 6)))
            m.write8(0x800EC1E0, rng.choice((0, 1, 2, 3)))
        h.run("ProjectilesUpdate", (), (), setup, f"random update {index}", member_for(index))


def run_mutations(h):
    # Each mutant inserts a distinct unexpected byte write at a function entry.
    targets = list(FUNCS)
    caught = 0
    original = (ROOT / "src/world/ae_zako2.c").read_text(encoding="utf-8")
    for index in range(30):
        target = targets[index % len(targets)]
        c_function = FUNCS[target][2]
        match = re.search(rf"(?m)^(?:void|int32_t)\s+{re.escape(c_function)}\s*\([^;{{}}]*\)\s*\{{", original)
        if match is None:
            raise SystemExit(f"mutation could not locate {c_function}")
        address = 0x801E0000 + index * 4
        value = 0x6B000000 | index
        injection = f"\n    /* Wrong-copy mutation {index + 1}. */\n    ae_wr32(r, 0x{address:08X}u, 0x{value:08X}u);\n"
        mutated = original[:match.end()] + injection + original[match.end():]
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", suffix=".c",
                                         dir=ROOT / "extract/mips", delete=False) as temp:
            temp.write(mutated)
            path = Path(temp.name)
        try:
            mutant = Side(build(source=path))
        finally:
            path.unlink(missing_ok=True)
        h.side = mutant
        fixture = h.fixtures.get(target)
        if fixture is None:
            raise SystemExit(f"no differential fixture captured for {target}")
        args_mips, args_c, setup, label, area, regs = fixture
        old_failures = h.failures
        h.run(target, args_mips, args_c, setup, f"mutation {index + 1}: {target}/{label}", area, regs,
              capture=False)
        if h.failures == old_failures:
            print(f"mutation escaped detection: {index + 1} {target}")
            return 1
        caught += 1
    print(f"PASS: {caught}/30 mutations caught across {len(FUNCS)} functions")
    return 0 if caught == 30 else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--cases", type=float, default=1.0)
    parser.add_argument("--seed", type=int, default=94423)
    parser.add_argument("--trapv", action="store_true")
    parser.add_argument("--mutate", action="store_true")
    args = parser.parse_args()
    exe = ROOT / "extract/disc/SCUS_944.23"
    if not exe.exists():
        print("SKIP: extract/disc/SCUS_944.23 not present")
        return 0
    members = dump_areas.area_members()
    requested = [1, 2, 4, 5, 7]
    areas = [(area_id, members[area_id]) for area_id in requested
             if area_id in members and (ROOT / f"extract/members/{members[area_id]:03X}.bin").exists()]
    if len(areas) < 3:
        raise SystemExit("three real area-data members are required for zako2 checks")
    side = Side(build(trapv=args.trapv))
    harness = Harness(side, areas)
    started = time.time()
    run_cases(harness, args.cases, args.seed)
    default_run = args.cases == 1.0 and args.seed == 94423 and not args.trapv
    summary = harness.finish(default_run, args.coverage or default_run)
    print("per-function differential cases:")
    for name, count in harness.counts.items(): print(f"  {name:28s} {count}")
    gaps = [name for name, row in summary.items() if row["uncovered"] or row["stale_declared"]]
    if gaps: print("coverage gaps: " + ", ".join(gaps))
    print(f"total {sum(harness.counts.values())} cases; {time.time() - started:.1f}s")
    if harness.failures or gaps:
        print(f"FAILED: {harness.failures} differential mismatches")
        return 1
    if args.mutate:
        return run_mutations(harness)
    print("PASS: zako2 function diffs matched")
    return 0


if __name__ == "__main__":
    sys.exit(main())
