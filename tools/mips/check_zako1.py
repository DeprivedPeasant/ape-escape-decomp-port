"""Differential check for the readable Zako per-frame update port.

Loads the US EXE and real area members, executes each table entry on MipsRam,
and compares it with the corresponding AeRam function.  The renderer and
overlay calls use the named hooks in ae_zako1.h. Run with the repository venv:
  .venv/Scripts/python.exe tools/mips/check_zako1.py [--coverage] [--mutate]
"""
import argparse
import ctypes as C
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
sys.path.insert(0, str(ROOT / "tools"))
import dump_areas  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

U32, I32, P = C.c_uint32, C.c_int32, C.c_void_p
STACK_LO, STACK_HI = 0x801FF000, 0x80200000
MUTATION_SELECTOR, MUTATION_MARKER = 0x801FEF00, 0x801FEF04
HOOK_SCREEN_X, HOOK_SCREEN_Y, HOOK_DEPTH = 0x80187000, 0x80187002, 0x80187004
RENDER_PACKET_BASE, RENDER_PACKET_SIZE = 0x80186000, 0x1000
AREA_IDS = (1, 2, 3, 6)

# address, instruction bytes, return type, argument types, takes AeZako1Hooks
FUNCS = {
    "ZakoSpawnNearby": (0x80050B04, 304, "v", "", 1),
    "ZakoUpdateAll": (0x80050C34, 124, "v", "", 1),
    "ZakoAllocSlot": (0x80050CB0, 84, "u", "", 0),
    "ZakoUpdateLoop": (0x800511A4, 620, "v", "", 1),
    "PointOffscreen": (0x80051410, 292, "s", "uss", 1),
    "ZakoCullAndDraw": (0x80051534, 236, "v", "", 1),
    "ZakoGetBonePoint": (0x80051620, 184, "v", "ussu", 0),
    "ZakoUpdateLinkPoints": (0x800516D8, 104, "v", "u", 0),
    "ZakoUpdateColliderPos": (0x80051740, 364, "v", "u", 1),
    "ZakoBuildMatrix": (0x800518AC, 380, "v", "u", 0),
    "ZakoPoseHidden": (0x80051A28, 124, "v", "u", 1),
    "ModelLightColorsToScratch": (0x80051AA4, 348, "v", "uuus", 1),
    "ZakoDraw": (0x80051C00, 460, "v", "u", 1),
    "ObjReenableBody": (0x80051DCC, 104, "v", "u", 1),
    "ZakoDefeat": (0x80051F1C, 204, "v", "u", 1),
    "ZakoDropPickups": (0x8005216C, 316, "v", "u", 1),
    "ZakoSetKnockback": (0x800522A8, 356, "v", "uss", 1),
    "ZakoApplyHit": (0x8005240C, 404, "v", "uu", 1),
    "ObjApplyDamage": (0x800525A0, 64, "s", "uu", 0),
    "ZakoOnHitByGadget": (0x800525E0, 96, "v", "u", 1),
    "ObjIsOwnerType0B": (0x80052640, 24, "s", "u", 0),
    "ObjAnimClampLastFrame": (0x80052658, 48, "v", "u", 0),
    "ObjBounceOnGround": (0x80052688, 260, "s", "us", 0),
    "ObjApplyVelocity": (0x8005278C, 116, "s", "u", 1),
    "ObjKnockbackDecay": (0x80052800, 100, "s", "u", 0),
    "ObjDeathSfx": (0x80052864, 124, "v", "u", 1),
    "ObjResetMotion": (0x800528E0, 192, "v", "u", 1),
    "ObjSwapToReactAnim": (0x800529A0, 132, "v", "u", 1),
    "ObjUpdateMotionState": (0x80052A24, 420, "s", "u", 1),
    "ObjSyncBodyHitFlags": (0x80052BC8, 132, "v", "u", 0),
    "ObjPlaySfx": (0x80055404, 92, "v", "us", 1),
    "ObjTurnTowardsXZ": (0x8005685C, 272, "v", "ussss", 1),
    "ObjGroundHeightCached": (0x80056C58, 136, "s", "usss", 1),
    "ObjPuffAt": (0x800573A0, 76, "v", "usss", 1),
    "ZakoHooksRun": (0x8005760C, 116, "v", "", 1),
    "ZakoDebugMenuDraw": (0x80058954, 1912, "v", "", 1),
}

# Additional deterministic edge cases beyond the base fixture.  Each one
# changes an input that selects a distinct original control-flow path.
EXTRA_SCENARIOS = {
    "ZakoSpawnNearby": 1, "ZakoAllocSlot": 2, "ZakoUpdateLoop": 10,
    "PointOffscreen": 4, "ZakoCullAndDraw": 2, "ZakoGetBonePoint": 1,
    "ZakoUpdateColliderPos": 2, "ZakoBuildMatrix": 6, "ZakoPoseHidden": 2,
    "ModelLightColorsToScratch": 1, "ZakoDraw": 4, "ObjReenableBody": 2,
    "ZakoDefeat": 1, "ZakoDropPickups": 1, "ZakoSetKnockback": 2,
    "ZakoApplyHit": 5, "ObjApplyDamage": 2, "ZakoOnHitByGadget": 1,
    "ObjIsOwnerType0B": 1, "ObjAnimClampLastFrame": 1,
    "ObjBounceOnGround": 3, "ObjDeathSfx": 2, "ObjResetMotion": 4,
    "ObjSwapToReactAnim": 1, "ObjUpdateMotionState": 10,
    "ObjSyncBodyHitFlags": 2, "ObjTurnTowardsXZ": 4,
    "ObjGroundHeightCached": 1, "ZakoDebugMenuDraw": 2,
}

HOOKFN = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32)
DISPATCHFN = C.CFUNCTYPE(I32, P, P, U32, U32, U32, U32, U32)
PADFN = C.CFUNCTYPE(C.c_uint16, P, P, U32)
BODY_PRINTF = C.CFUNCTYPE(None, P, P, U32, I32)


def _hook_names():
    h = (ROOT / "src/world/ae_zako1.h").read_text(encoding="utf-8")
    body = h[h.index("typedef struct AeZako1Hooks"):]
    body = body[:body.index("} AeZako1Hooks;")]
    entries = []
    for m in re.finditer(r"(AeZako1(?:Dispatch|Hook)Fn)\s+(\w+);\s*/\*([^*]*)\*/", body):
        address = re.search(r"\b([0-9A-Fa-f]{8})\b", m.group(3))
        entries.append((m.group(2), m.group(1) == "AeZako1DispatchFn",
                        int(address.group(1), 16) if address else None))
    return entries


HOOK_NAMES = _hook_names()
HOOK_FIELDS = [(n, DISPATCHFN if d else HOOKFN) for n, d, _a in HOOK_NAMES]
# Number of argument registers actually consumed by each API. MIPS call sites
# can leave unrelated values in later argument registers; those are not part of
# the callee interface and are never forwarded by readable C ports.
HOOK_ARGUMENT_COUNTS = {"Atan2Safe": 2}


class Zako1Hooks(C.Structure):
    _fields_ = [("user", P), ("init_hooks", P), ("body_hooks", P),
                ("world_hooks", P), ("spawn_hooks", P), ("sound_hw", P)] + HOOK_FIELDS


class SpawnHooks(C.Structure):
    _fields_ = [("user", P), ("world", P), ("PlaySfx", HOOKFN),
                ("MatrixLookDir", HOOKFN), ("MatrixFromDirXZ", HOOKFN),
                ("position_padding", PADFN)]


class WorldHooks(C.Structure):
    _fields_ = [("user", P), ("GetSolidHeight", HOOKFN), ("PlaySfxAt", HOOKFN),
                ("GimmickActivate", HOOKFN), ("MeshInitFromAreaTable", HOOKFN),
                ("FUN_8013d358", HOOKFN), ("CameraShake", HOOKFN),
                ("EventCamStart", HOOKFN), ("ground_stale_x", C.c_uint16),
                ("ground_stale_z", C.c_uint16)]


class BodyHooks(C.Structure):
    _fields_ = [("user", P), ("printf_error", BODY_PRINTF)] + [
        (name, P) for name in ("sound_loop_start", "sound_loop_stop", "body_move",
                               "build_probe_points", "body_stub_return_zero",
                               "collision_ray_test", "gimmick_type_to_sfx",
                               "play_sfx_at", "pickup_collect", "camera_nudge")]


class InitHooks(C.Structure):
    _fields_ = [("user", P), ("Printf", HOOKFN), ("MenuItemInitBound", HOOKFN),
                ("MenuItemSetCallback", HOOKFN), ("ReadSizedValue", HOOKFN),
                ("body_hooks", P)]


def build(source=None, tag="zako1"):
    out = ROOT / "extract/mips"
    out.mkdir(parents=True, exist_ok=True)
    dll = out / f"{tag}_{time.time_ns()}.dll"
    sources = [
        "src/core/ae_ram.c", "src/core/ae_sdk.c", "src/core/ae_sdk_matrix.c",
        "src/core/ae_pose.c", "src/player/native_player.c", "src/bodies/ae_bodies.c",
        "src/world/ae_world.c", "src/effects/ae_spawn.c", "src/sound/ae_sound.c",
        "src/world/ae_zako_init.c", str(source or ROOT / "src/world/ae_zako1.c"),
    ]
    subprocess.run([shutil.which("gcc"), "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-Isrc/world",
                    "-shared", "-static-libgcc", *sources, "-o", str(dll), "-lm"],
                   cwd=ROOT, check=True)
    return dll


def s16(v): return v & 0xFFFF
def s32(v): return v & 0xFFFFFFFF


class MipsMem:
    def __init__(self, cpu): self.cpu = cpu
    def rd8(self, a): return self.cpu.read(a, 1)
    def rd16(self, a): return self.cpu.read(a, 2)
    def rd32(self, a): return self.cpu.read(a, 4)
    def wr8(self, a, v): self.cpu.write(a, 1, v & 0xFF)
    def wr16(self, a, v): self.cpu.write(a, 2, v & 0xFFFF)
    def wr32(self, a, v): self.cpu.write(a, 4, v & 0xFFFFFFFF)


class CMem:
    def __init__(self, dll, ram): self.dll, self.ram = dll, ram
    def rd8(self, a): return self.dll.ae_rd8(self.ram, a)
    def rd16(self, a): return self.dll.ae_rd16(self.ram, a)
    def rd32(self, a): return self.dll.ae_rd32(self.ram, a)
    def wr8(self, a, v): self.dll.ae_wr8(self.ram, a, v & 0xFF)
    def wr16(self, a, v): self.dll.ae_wr16(self.ram, a, v & 0xFFFF)
    def wr32(self, a, v): self.dll.ae_wr32(self.ram, a, v & 0xFFFFFFFF)


class HookModel:
    def __init__(self, mem): self.mem, self.events = mem, []
    def call(self, name, a):
        if name == 'Atan2Safe': a = a[:2]  # Its public API has two scalar inputs.
        args = tuple(x & 0xFFFFFFFF for x in a)
        detail = ()
        if name == "FUN_80031330":
            detail = (tuple(self.mem.rd16(args[0] + off) for off in (0, 2, 4)),)
        elif name == "SpawnPickups":
            detail = (tuple(self.mem.rd16(args[1] + off) for off in (0, 2, 4)),)
        self.events.append((name, args, detail))
        if name == "FUN_800A3740":
            for i in range(min(a[2] & 0xFFFFFFFF, 64)):
                self.mem.wr8(a[0] + i, 0)
            return 0
        if name == "FUN_80031330":
            self.mem.wr16(a[1], self.mem.rd16(HOOK_SCREEN_X))
            self.mem.wr16(a[1] + 2, self.mem.rd16(HOOK_SCREEN_Y))
            self.mem.wr32(a[2], 0)
            self.mem.wr32(a[3], 0)
            return self.mem.rd32(HOOK_DEPTH)
        if name == "Atan2Safe": return (a[0] + a[1]) & 0xFFF
        if name == "ZakoDebugDrawPrimitive":
            # This is the native replacement for the GTE lighting / packet block.
            if 0x800E6960 <= a[2] < 0x800E6960 + 16 * 0xE4:
                self.mem.wr16(0x1F8003C6, 0)
                self.mem.wr16(0x1F8003C4, 0)
                self.mem.wr8(0x1F8003F4, 2)
                self.mem.wr16(0x1F8003C2, self.mem.rd16(0x800FBFA0 + 0x28))
                self.mem.wr16(0x1F8003C0, self.mem.rd16(0x800FBFA0 + 0x24))
                return a[2]
            if RENDER_PACKET_BASE <= a[2] < RENDER_PACKET_BASE + RENDER_PACKET_SIZE:
                self.mem.wr32(a[3] + 4, (a[2] + 0x140) & 0x00FFFFFF)
                return a[2] + 0x150
            return a[2]
        return 0


def _make_hook(name, model, dispatch=False):
    if dispatch:
        def cb(_user, _ram, target, a0, a1, a2, a3):
            model.events.append((name, tuple(x & 0xFFFFFFFF for x in
                                             (target, a0, a1, a2, a3)), ()))
            return 0
        return DISPATCHFN(cb)
    def cb(_user, ram, a0, a1, a2, a3):
        model.mem = CMem(model.dll, ram)
        return model.call(name, (a0, a1, a2, a3))
    return HOOKFN(cb)


def hook_signature(event):
    """Compare arguments consumed by the original APIs, ignoring volatile filler.

    Several MIPS call sites leave unused argument registers live, and native
    stack buffers have different addresses on the two sides. Stack vectors are
    therefore compared by their contents at the projection/pickup boundaries.
    """
    name, args, detail = event
    if name in ("AreaModeUpdate", "ZakoTypeUpdate", "ZakoFrameHook"):
        return name, args[:2]  # indirect target and first game argument
    if name in ("FUN_8002FB00", "FUN_8002FB30", "FUN_8013F660",
                "ModelTransformSkinned"):
        return name, args[:1]
    if name == "FUN_80031330":
        return name, detail[0]
    if name in HOOK_ARGUMENT_COUNTS:
        if len(args) > HOOK_ARGUMENT_COUNTS[name]:
            reg.declare("ram_mask", f"hook {name} peek[{HOOK_ARGUMENT_COUNTS[name]}:4]")
        return name, args[:HOOK_ARGUMENT_COUNTS[name]]
    if name == "FUN_800A3740":
        return name, args[2:3]
    if name == "SpawnPickups":
        return name, (args[0], detail[0], args[2], args[3])
    if name in ("ModelPrepCycleColors",):
        return name, args[:2]
    if name in ("ModelEmitPrims",):
        return name, args[:3]
    if name == "ModelEmitPrimsCycleColors":
        return name, args[:3]
    if name == "DebugTextPrintfWhite":
        if args[0] in (0x800A52F8, 0x800A5308):
            return name, args[:2]
        return name, args[:1]
    return name, args


class CSide:
    def __init__(self, dll):
        self.dll = C.CDLL(str(dll))
        dll = self.dll
        dll.ae_ram_create.restype = P
        dll.ae_ptr.argtypes, dll.ae_ptr.restype = [P, U32, U32], P
        for fn, typ in (("ae_rd8", U32), ("ae_rd16", U32), ("ae_rd32", U32)):
            getattr(dll, fn).argtypes, getattr(dll, fn).restype = [P, U32], typ
        for fn in ("ae_wr8", "ae_wr16", "ae_wr32"):
            getattr(dll, fn).argtypes = [P, U32, U32]
        dll.ae_ram_failed.argtypes, dll.ae_ram_failed.restype = [P], C.c_int
        dll.ae_ram_error.argtypes, dll.ae_ram_error.restype = [P], C.c_char_p
        dll.ae_ram_clear_error.argtypes = [P]
        self.ram = dll.ae_ram_create()
        self.main = (C.c_uint8 * 0x200000).from_address(dll.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(dll.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.funcs = {}
        for name, (_addr, _size, ret, args, takes_hooks) in FUNCS.items():
            fn = getattr(dll, "ae_" + name)
            fn.argtypes = [P] + ([C.POINTER(Zako1Hooks)] if takes_hooks else []) + [I32 if k == "s" else U32 for k in args]
            fn.restype = {"v": None, "s": I32, "u": U32}[ret]
            self.funcs[name] = fn
        self.callbacks = []
        self.hooks = Zako1Hooks()
        self.spawn_hooks = SpawnHooks()
        self.world_hooks = WorldHooks()
        self.body_hooks = BodyHooks()
        self.init_hooks = InitHooks()
        self.position_padding_value = 0
        self.padding = PADFN(lambda _user, _ram, _fn: self.position_padding_value)
        self.body_printf = BODY_PRINTF(lambda _user, _ram, _fmt, _typ: None)
        self.body_hooks.printf_error = self.body_printf
        self.init_hooks.body_hooks = C.cast(C.pointer(self.body_hooks), P)
        self.spawn_hooks.position_padding = self.padding
        self.hooks.init_hooks = C.cast(C.pointer(self.init_hooks), P)
        self.hooks.spawn_hooks = C.cast(C.pointer(self.spawn_hooks), P)
        self.hooks.world_hooks = C.cast(C.pointer(self.world_hooks), P)
        self.callbacks.extend([self.padding, self.body_printf])
        self.model = None

    def sync(self, snapshot):
        C.memmove(self.main, snapshot[0], 0x200000)
        C.memmove(self.scratch, snapshot[1], 0x400)
        self.dll.ae_ram_clear_error(self.ram)

    def set_model(self, model):
        model.dll = self.dll
        self.model = model
        for name, dispatch, _addr in HOOK_NAMES:
            callback = _make_hook(name, model, dispatch)
            self.callbacks.append(callback)
            setattr(self.hooks, name, callback)
        self.world_hooks.GetSolidHeight = _make_hook("GetSolidHeight", model)
        self.callbacks.append(self.world_hooks.GetSolidHeight)

    def call(self, name, args):
        prefix = [self.ram]
        if FUNCS[name][4]: prefix.append(C.byref(self.hooks))
        return self.funcs[name](*prefix, *args)


def _store(mem, addr, value, size=4):
    (mem.wr8 if size == 1 else mem.wr16 if size == 2 else mem.wr32)(addr, value)


def reset_collision_slots(mem):
    base, stride = 0x800BBDA0, 0x14
    for index in range(256):
        slot = base + index * stride
        mem.wr16(slot, 0xFFFF)
        mem.wr16(slot + 2, index)
        mem.wr32(slot + 4, 0)
        mem.wr32(slot + 8, 0)
        mem.wr32(slot + 0x0C, 0)
        mem.wr32(slot + 0x10, 0)
    mem.wr32(0x800B20D8, 0)
    mem.wr32(0x800B20DC, 0)


def setup_case(mem, name, scenario=0):
    # Fixed object, links, collider, model and vector buffers in ordinary RAM.
    state, obj, player = 0x800E77D0, 0x800E6960, 0x800EC1E0
    entry_table, colliders = 0x80180000, 0x80181000
    model, matrix, output = 0x80182000, 0x80183000, 0x80184000
    for i in range(16 * 0xE4): mem.wr8(0x800E6960 + i, 0)
    for i in range(STACK_HI - STACK_LO): mem.wr8(STACK_LO + i, 0)
    mem.wr16(state + 0x0C, 0)
    mem.wr16(state + 0x10, 1)
    mem.wr32(state + 4, entry_table)
    mem.wr8(state + 0x15, 0)
    mem.wr8(0x800E77A0, 0)
    mem.wr8(0x800E77E4, 1)
    mem.wr32(0x800F4450, 0)
    mem.wr32(0x800AF574, 0x80100000)
    mem.wr32(0x800AF350, 0x80100004)
    mem.wr8(0x800E77A5, 0)
    mem.wr32(0x800B8D20, 0x80100008)
    mem.wr8(0x800E77E8, 1)
    mem.wr8(0x800E77EF, 0)
    mem.wr8(0x800E77EE, 1)
    mem.wr16(0x800B4D50, 200)
    mem.wr16(0x800B4D52, 200)
    mem.wr32(0x800F455C, colliders)
    mem.wr16(0x800E77A8, 0)
    for i in range(0x400): mem.wr8(0x800BF670 + i, 0xFF if i < 32 else 0)
    mem.wr16(player + 0x24, 300)
    mem.wr16(player + 0x2C, 500)
    mem.wr16(player + 0x60, 0x1000)
    mem.wr16(player + 0x64, 0)
    for off in (0x24, 0x28, 0x2C): mem.wr32(player + off, 100)
    mem.wr16(obj + 0x24, 100)
    mem.wr16(obj + 0x28, 120)
    mem.wr16(obj + 0x2C, 140)
    mem.wr16(obj + 0x60, 0x1000)
    mem.wr16(obj + 0x64, 0)
    mem.wr16(obj + 0x70, 0xFFFF)
    mem.wr8(obj + 0x81, 10)
    mem.wr8(obj + 0x83, 1)
    mem.wr8(obj + 0x82, 0)
    mem.wr8(obj + 0x88, 0)
    mem.wr32(obj + 0x88, 0x80185000)
    mem.wr16(0x80185000, 0)
    mem.wr16(0x80185002, 10)
    mem.wr16(obj + 0x5C, 0)
    mem.wr32(obj + 0x88, 0x80185000)
    mem.wr32(obj + 0x00, model)
    mem.wr32(obj + 0x04, 0x80185100)
    mem.wr32(obj + 0x08, 0x80185200)
    mem.wr32(obj + 0x0C, 0x80185300)
    mem.wr32(obj + 0x10, 0x80185400)
    mem.wr32(obj + 0x14, matrix)
    for i in range(0x20): mem.wr8(matrix + i, 0)
    for offset in (0, 8, 16): mem.wr16(matrix + offset, 4096)
    mem.wr8(obj + 0xC8, 255)
    mem.wr8(obj + 0xCC, 0xFF)
    mem.wr8(obj + 0xDC, 99)
    mem.wr8(obj + 0xE3, 0)
    mem.wr32(obj + 0xE0, 0)
    mem.wr32(obj + 0xD0, 100)
    mem.wr16(obj + 0x86, 100)
    mem.wr32(obj + 0xD0, 100)
    mem.wr32(obj + 0xE0, 0x100)
    mem.wr16(obj + 0x94, 1)
    mem.wr16(obj + 0x96, 2)
    mem.wr16(obj + 0x98, 3)
    mem.wr16(obj + 0xD8, 80)
    mem.wr8(obj + 0xDA, 2)
    mem.wr32(0x800F4730 + 0x14, 0)
    mem.wr16(0x800F4730 + 0x14, 0)
    mem.wr16(0x800F4730 + 0x18, 0)
    mem.wr16(0x800F4730 + 0x1C, 0)
    for i in range(0x40): mem.wr8(model + i, 0)
    mem.wr32(model + 0, 0x80185500)
    mem.wr16(model + 4, 0)
    mem.wr16(model + 6, 0)
    mem.wr16(model + 8, 0)
    mem.wr16(model + 0x24, 0)
    mem.wr16(model + 0x28, 0)
    mem.wr16(0x800FBFA0 + 0x24, 0)
    mem.wr16(0x800FBFA0 + 0x28, 0)
    mem.wr32(0x800E78C0, 0)
    mem.wr32(0x800E78C4, 0)
    mem.wr32(0x800E78C8, 0)
    mem.wr32(0x800E78CC, 0)
    mem.wr32(0x800E78D0, 0)
    mem.wr16(HOOK_SCREEN_X, 100)
    mem.wr16(HOOK_SCREEN_Y, 100)
    mem.wr32(HOOK_DEPTH, 100)
    mem.wr32(0x800EC354, RENDER_PACKET_BASE)
    mem.wr32(0x800EC350, 0)
    mem.wr32(0x800EC35C, 0)
    for i in range(RENDER_PACKET_SIZE): mem.wr8(RENDER_PACKET_BASE + i, 0)

    args = {
        "ZakoSpawnNearby": (), "ZakoUpdateAll": (), "ZakoAllocSlot": (), "ZakoUpdateLoop": (),
        "PointOffscreen": (obj + 0x24, 0, 0), "ZakoCullAndDraw": (),
        "ZakoGetBonePoint": (obj, 0, -1, output), "ZakoUpdateLinkPoints": (obj,),
        "ZakoUpdateColliderPos": (obj,), "ZakoBuildMatrix": (obj,), "ZakoPoseHidden": (obj,),
        "ModelLightColorsToScratch": (0x80185000, 0x800E7D00, obj, 0), "ZakoDraw": (obj,),
        "ObjReenableBody": (obj,), "ZakoDefeat": (obj,), "ZakoDropPickups": (obj,),
        "ZakoSetKnockback": (obj, obj + 0x60, 20, 1024), "ZakoApplyHit": (obj, obj + 0x24),
        "ObjApplyDamage": (obj, obj + 0x24), "ZakoOnHitByGadget": (obj,),
        "ObjIsOwnerType0B": (obj,), "ObjAnimClampLastFrame": (obj,),
        "ObjBounceOnGround": (obj, 100), "ObjApplyVelocity": (obj,),
        "ObjKnockbackDecay": (obj,), "ObjDeathSfx": (obj,), "ObjResetMotion": (obj,),
        "ObjSwapToReactAnim": (obj,), "ObjUpdateMotionState": (obj,),
        "ObjSyncBodyHitFlags": (obj,), "ObjPlaySfx": (obj, 1),
        "ObjTurnTowardsXZ": (obj, 20, 30, 0, 0x1000),
        "ObjGroundHeightCached": (obj, 0, 0, 0), "ObjPuffAt": (obj, 0, 0, 0),
        "ZakoHooksRun": (), "ZakoDebugMenuDraw": (),
    }
    if name == "ZakoSpawnNearby":
        mem.wr16(state + 0x0C, 1); mem.wr8(entry_table + 0x16, 1)
    if name == "ZakoUpdateAll":
        mem.wr8(0x800E77E4, 0); mem.wr16(state + 0x10, 0)
    if name == "ZakoUpdateLoop": mem.wr16(state + 0x10, 0)
    if name in ("ZakoCullAndDraw", "PointOffscreen"):
        mem.wr32(obj + 0xE0, 0x10 | 0x100)
    if name == "ZakoGetBonePoint":
        args[name] = (obj, 0, -1, output)
    if name == "ZakoUpdateLinkPoints":
        link = 0x80186000
        mem.wr32(obj + 0x74, link); mem.wr32(obj + 0x78, 0)
        mem.wr16(link + 8, 0); mem.wr16(link + 10, 0xFFFF)
    if name == "ZakoUpdateColliderPos":
        mem.wr16(obj + 0x70, 0); mem.wr32(0x800F455C, colliders)
    if name in ("ZakoPoseHidden", "ZakoDraw"):
        mem.wr32(obj + 0xE0, 0x100)
    if name == "ZakoDefeat": mem.wr8(obj + 0xCC, 0xFF)
    if name == "ZakoDropPickups": mem.wr8(obj + 0xDC, 99)
    if name == "ZakoApplyHit": mem.wr8(obj + 0xDB, 1)
    if name == "ZakoOnHitByGadget":
        mem.wr16(obj + 0x50, 0); mem.wr8(obj + 0xDB, 1); mem.wr8(obj + 0x81, 10)
    if name == "ObjApplyDamage": mem.wr8(0x800E77E8, 1); mem.wr8(obj + 0x81, 10)
    if name in ("ObjApplyVelocity", "ObjGroundHeightCached"):
        mem.wr32(obj + 0xE0, 0x100); mem.wr16(obj + 0x86, 100)
    if name == "ObjResetMotion":
        mem.wr8(obj + 0x81, 10); mem.wr8(obj + 0xCE, 0); mem.wr8(obj + 0x7F, 0)
    if name == "ObjUpdateMotionState": mem.wr8(obj + 0x82, 0)
    if name == "ObjPlaySfx":
        mem.wr16(0x800B2D08 + 8, 0)
        mem.wr16(0x800B2D08 + 10, 0); mem.wr16(0x800B2D08 + 12, 0)
        mem.wr16(0x800BF670, 0xFFFF)
    if name in ("ObjDeathSfx", "ObjPuffAt"):
        mem.wr16(0x800BF670, 0xFFFF)
    if name == "ZakoHooksRun":
        mem.wr8(0x800E77A5, 1); mem.wr32(0x800B8D20, 0x80100008)

    obj = 0x800E6960
    state = 0x800E77D0
    entry = 0x80180000
    attacker = obj + 0x24
    if scenario:
        if name == "ZakoSpawnNearby":
            mem.wr16(state + 0x0C, 1); mem.wr16(state + 0x10, 1)
            mem.wr8(entry + 0x16, 0); mem.wr16(entry + 0x10, 1)
            mem.wr16(entry + 0x12, 0)
            mem.wr16(entry + 0x02, 1500); mem.wr16(entry + 0x06, 1500)
            mem.wr8(entry, 0xAD)
        elif name == "ZakoAllocSlot":
            mem.wr16(state + 0x10, 1)
            if scenario == 1: mem.wr32(obj + 0xE0, 0)
            else: mem.wr32(obj + 0xE0, 0x10)
        elif name == "ZakoUpdateLoop":
            mem.wr16(state + 0x10, 1)
            mem.wr32(obj + 0xE0, 0x10 | 0x40000)
            mem.wr8(obj + 0x82, (scenario - 1) % 6)
            mem.wr8(obj + 0x81, 10)
            mem.wr16(obj + 0x5C, scenario)
            mem.wr32(obj + 0x88, 0x80185000)
            mem.wr16(0x80185002, 20)
            if scenario >= 2:
                mem.wr32(obj + 0xE0, 0x10 | 0x40000 | 0x00200000)
                obj_type = mem.rd8(obj + 0x7E)
                mem.wr32(0x800AF350 + obj_type * 4, 0x80100004)
            if scenario == 7:
                mem.wr16(obj + 0x70, 4); mem.wr16(0x800F45E0, 3)
                mem.wr16(0x80185000, 0); mem.wr16(0x80185002, 0)
                mem.wr16(obj + 0xAE, 1); mem.wr16(obj + 0xAC, 1000)
                mem.wr16(obj + 0xB2, -10000)
            elif scenario == 8:
                mem.wr32(obj + 0xE0, 0x10 | 0x40000 | 0x00200000 | 0x00100000)
                mem.wr8(obj + 0x82, 1); mem.wr16(obj + 0x5C, 9)
                mem.wr16(0x80185002, 10)
            elif scenario in (9, 10):
                mem.wr32(obj + 0xE0, 0x10)
                mem.wr8(obj + 0x82, 0); mem.wr8(obj + 0x7E, 63 if scenario == 9 else 1)
                mem.wr32(obj + 0x28, 3000); mem.wr16(obj + 0x9C, 100)
        elif name == "PointOffscreen":
            if scenario == 1:
                args[name] = (obj + 0x24, 0, 1)
                mem.wr16(HOOK_SCREEN_X, -101); mem.wr32(HOOK_DEPTH, 500)
            elif scenario == 2:
                args[name] = (obj + 0x24, 0, 1)
                mem.wr16(HOOK_SCREEN_X, -601); mem.wr32(HOOK_DEPTH, 100)
            elif scenario == 3:
                args[name] = (obj + 0x24, 0, 0)
                mem.wr32(HOOK_DEPTH, 201)
            else:
                args[name] = (obj + 0x24, 0, 0)
                mem.wr32(HOOK_DEPTH, 0)
        elif name == "ZakoCullAndDraw":
            mem.wr16(state + 0x10, 1)
            if scenario == 1:
                mem.wr32(obj + 0xE0, 0x100)
            else:
                mem.wr32(obj + 0xE0, 0x10 | 0x100 | 0x200)
                mem.wr8(obj + 0xC8, 0)
                mem.wr16(HOOK_SCREEN_X, 1000)
        elif name == "ZakoGetBonePoint":
            args[name] = (obj, 0, 0, 0x80184000)
        elif name == "ZakoUpdateColliderPos":
            mem.wr16(obj + 0xAE, 1)
            mem.wr16(obj + 0xB2, -0x800 if scenario == 1 else -10000)
        elif name == "ZakoBuildMatrix":
            if scenario <= 4:
                mode = (scenario - 1) % 4
                mem.wr32(obj + 0xE0, (mode << 22) | (0x80 if scenario == 4 else 0))
                mem.wr8(obj + 0xC8, (10, 50, 255, 10)[mode])
                mem.wr16(0x800E77EE, 1)
            else:
                mem.wr32(obj + 0xE0, 1 << 22)
                mem.wr16(obj + 0xAC, 0x3000)
                mem.wr8(obj + 0xC8, 50)
                mem.wr8(0x800E77EE, 0)
        elif name == "ZakoPoseHidden":
            mem.wr32(obj + 0xE0, 0x2000)
            mem.wr32(obj + 0x74, 0x80186000)
            mem.wr16(0x80186008, 0); mem.wr16(0x8018600A, 0xFFFF)
            if scenario == 2: mem.wr16(obj + 0x70, 4)
        elif name == "ModelLightColorsToScratch":
            args[name] = (0x80185000, 0x800E7D00, obj, 1)
        elif name == "ZakoDraw":
            if scenario == 1:
                mem.wr32(obj + 0xE0, 0x04000000)
            elif scenario == 2:
                mem.wr32(obj + 0xE0, 0x2000)
                mem.wr16(obj + 0xA0, 20)
                mem.wr32(obj + 0x74, 0x80186000)
                mem.wr16(0x80186008, 0); mem.wr16(0x8018600A, 0xFFFF)
                mem.wr16(obj + 0x70, 4); mem.wr16(obj + 0xAE, 1)
            else:
                mem.wr8(obj + 0x84, 8)
                if scenario == 4: mem.wr32(0x800EC350, 3)
        elif name == "ObjReenableBody":
            reset_collision_slots(mem)
            mem.wr32(obj + 0xE0, 0x4002 | (0x08000000 if scenario == 2 else 0))
        elif name == "ZakoDefeat":
            mem.wr8(obj + 0xE3, 2)
        elif name == "ZakoDropPickups":
            mem.wr8(obj + 0xDC, 0)
            mem.wr8(0x800AF300, 0); mem.wr8(0x800AF301, 3)
            mem.wr8(0x800AF302, 2); mem.wr8(0x800AF303, 0xFF)
        elif name == "ZakoSetKnockback":
            mem.wr8(obj + 0x82, 3 + scenario)
        elif name == "ZakoApplyHit":
            mem.wr8(obj + 0xDB, 1)
            if scenario == 1:
                mem.wr8(state + 0x19, 1); mem.wr8(state + 0x1A, 24); mem.wr8(state + 0x1C, 40)
            elif scenario == 2:
                mem.wr8(state + 0x19, 2); mem.wr8(state + 0x1A, 20); mem.wr8(state + 0x1C, 8)
            elif scenario == 3:
                mem.wr8(obj + 0xDB, 2); mem.wr8(obj + 0x82, 5)
            elif scenario == 4:
                mem.wr8(obj + 0xDB, 0); mem.wr16(attacker + 0x26, 0x0200)
            elif scenario == 5:
                mem.wr8(obj + 0x7D, 10); mem.wr8(obj + 0xDB, 1)
        elif name == "ObjApplyDamage":
            mem.wr8(obj + 0x81, 1)
            mem.wr16(attacker + 0x24, 2 if scenario == 1 else 15)
        elif name == "ZakoOnHitByGadget":
            reset_collision_slots(mem)
            mem.wr32(0x800BBDA0 + 0x14 + 4, 0x80185000)
            mem.wr16(0x800BBDA0 + 0x28, 4)
            mem.wr16(0x800BBDA0 + 0x2A, 2)
            mem.wr32(0x800BBDA0 + 0x2C, obj + 0x24)
            mem.wr32(0x800BBDA0 + 0x30, obj)
            mem.wr32(0x800B20D8, 0x800BBDA0 + 0x28)
            mem.wr32(0x800B20DC, 0x800BBDA0 + 0x28)
            mem.wr8(obj + 0x81, 1); mem.wr16(0x80185000 + 0x24, 2)
            mem.wr32(obj + 0x50, 0xB0000001)
            mem.wr32(obj + 0xE0, 0x4000)
        elif name == "ObjIsOwnerType0B":
            mem.wr32(obj + 0x50, 0xB0000000)
        elif name == "ObjAnimClampLastFrame":
            mem.wr16(obj + 0x5C, 10); mem.wr16(0x8018502A, 10)
        elif name == "ObjBounceOnGround":
            if scenario == 1: mem.wr32(obj + 0x28, 500)
            elif scenario == 2: mem.wr32(obj + 0x28, 50)
            else:
                mem.wr32(obj + 0x28, 120); mem.wr32(obj + 0xE0, 0x400)
        elif name == "ObjDeathSfx":
            mem.wr8(obj + 0xE3, scenario)
        elif name == "ObjResetMotion":
            if scenario == 1:
                reset_collision_slots(mem)
                mem.wr8(obj + 0x81, 0); mem.wr32(obj + 0xE0, 0x00204002)
            elif scenario == 2:
                mem.wr8(obj + 0x81, 0); mem.wr32(obj + 0xE0, 0)
                mem.wr8(0x800E77E8, 0)
            elif scenario == 3:
                mem.wr8(obj + 0x81, 0); mem.wr32(obj + 0xE0, 0)
                mem.wr8(0x800E77E8, 1)
            elif scenario == 4:
                mem.wr8(obj + 0x81, 10); mem.wr8(obj + 0x7F, 1); mem.wr8(obj + 0xCE, 2)
            else:
                mem.wr8(obj + 0x81, 10); mem.wr8(obj + 0x7F, 1); mem.wr8(obj + 0xCE, 2)
        elif name == "ObjSwapToReactAnim":
            reset_collision_slots(mem)
            mem.wr32(0x800BBDA0 + 0x14 + 4, 0x80185000)
            mem.wr16(0x800BBDA0 + 0x28, 4)
            mem.wr16(0x800BBDA0 + 0x2A, 2)
            mem.wr32(0x800BBDA0 + 0x2C, obj + 0x24)
            mem.wr32(0x800BBDA0 + 0x30, obj)
            mem.wr32(0x800B20D8, 0x800BBDA0 + 0x28)
            mem.wr32(0x800B20DC, 0x800BBDA0 + 0x28)
            mem.wr8(obj + 0x81, 1); mem.wr16(0x80185000 + 0x24, 0)
            mem.wr32(obj + 0xE0, 0x5000)
            mem.wr32(obj + 0x50, 0xB0000001)
        elif name == "ObjUpdateMotionState":
            motions = (1, 4, 5, 1, 4, 4, 5, 1, 5, 5)
            motion = motions[scenario - 1]
            mem.wr8(obj + 0x82, motion); mem.wr8(obj + 0x85, 0)
            if scenario == 4: mem.wr32(obj + 0xE0, 0x00100000)
            if scenario == 5:
                mem.wr8(state + 0x19, 1); mem.wr8(state + 0x1D, 4)
            if scenario == 6: mem.wr32(obj + 0xE0, 0x00000400 | 0x00000100)
            if scenario == 7:
                mem.wr32(obj + 0x28, 300); mem.wr16(obj + 0xD8, 30); mem.wr8(obj + 0xDA, 1)
            if scenario == 8:
                mem.wr32(obj + 0xE0, 0x00100000)
                mem.wr16(obj + 0x5C, 10)
            if scenario == 9: mem.wr32(obj + 0x28, 50)
            if scenario == 10: mem.wr32(obj + 0x28, -100)
        elif name == "ObjSyncBodyHitFlags":
            mem.wr32(obj + 0x50, 0xB0000000)
            mem.wr32(obj + 0xE0, 0x00008000 if scenario == 1 else 0x00028000)
        elif name == "ObjTurnTowardsXZ":
            if scenario == 1: mem.wr16(obj + 0xAC, 3000)
            elif scenario == 2:
                args[name] = (obj, 20, 30, -12000, 500)
            else:
                mem.wr16(obj + 0xAC, 0 if scenario == 4 else 50)
                args[name] = (obj, 20, 30, 0, 64)
            if scenario == 4: args[name] = (obj, 20, 30, 0, 16)
        elif name == "ObjGroundHeightCached":
            mem.wr32(obj + 0xE0, 0)
            mem.wr32(0x800F44E4, 0x80186000)
            mem.wr32(0x80186000, 2)
        elif name == "ZakoDebugMenuDraw":
            mem.wr8(state + 0x15, 1)
            mem.wr32(state + 4, entry)
            mem.wr16(state + 0x0C, 1)
            mem.wr8(entry + 0x16, 1)
            mem.wr16(entry + 0x10, 150); mem.wr16(entry + 0x12, 250)
            mem.wr16(state + 0x10, 1)
            mem.wr32(obj + 0xE0, 0x10)
            mem.wr32(0x800EC354, RENDER_PACKET_BASE)
            mem.wr32(0x800EC35C, 0)
            for i in range(0x4078): mem.wr8(0x800EC3D0 + i, 0)
            if scenario == 2:
                mem.wr8(state + 0x15, 2)
                mem.wr8(entry + 0x16, 0)
        else:
            raise ValueError(f"no scenario {scenario} defined for {name}")
    return args[name]


class Harness:
    def __init__(self, dll):
        reg.declare("stack_window", "801FF000..80200000")
        reg.check_ram_mask([(RENDER_PACKET_BASE - 0x80000000, RENDER_PACKET_SIZE)])
        members = dump_areas.area_members()
        self.bases = {a: mr.MipsRam.from_extract(ROOT / f"extract/members/{members[a]:03X}.bin").snapshot()
                      for a in AREA_IDS}
        self.cpu = mr.MipsRam.from_extract(ROOT / f"extract/members/{members[1]:03X}.bin")
        self.cpu.attach_gte_oracle()
        self.cpu.coverage = set()
        self.c = CSide(dll)
        self.counts = {n: 0 for n in FUNCS}
        self.failures = 0
        self.seed = 94423

    def run(self, name, area_id, selector=0, scenario=0):
        self.current_scenario = scenario
        self.c.position_padding_value = 0
        if name == "ObjSwapToReactAnim" and scenario == 1:
            # SpawnPuff reads the nested caller's uninitialised stack halfword
            # for particle position padding. Mirror that MIPS stack residue.
            self.c.position_padding_value = 0x8000
            reg.declare("shared_random", "ObjSwapToReactAnim nested SpawnPuff group position padding")
        self.cpu.restore(self.bases[area_id])
        self.cpu.gte_reset()
        self.cpu.coverage = self.coverage
        mm = MipsMem(self.cpu)
        args = setup_case(mm, name, scenario)
        mm.wr32(MUTATION_SELECTOR, selector)
        before = self.cpu.snapshot()
        mm_model = HookModel(mm)
        c_model = HookModel(None)
        self.c.sync(before)
        self.c.set_model(c_model)
        mips_hooks = {}
        for hook_name, dispatch, address in HOOK_NAMES:
            if address is None or dispatch: continue
            def wrapped(cpu, regs, n=hook_name, m=mm_model):
                m.mem = MipsMem(cpu)
                return m.call(n, tuple(regs[4:8]))
            mips_hooks[address] = wrapped
        def solid_height(cpu, regs):
            mm_model.mem = MipsMem(cpu)
            return mm_model.call("GetSolidHeight", tuple(regs[4:8]))
        mips_hooks[0x8004D8C0] = solid_height
        for name_, addr in (("AreaModeUpdate", 0x80100000),
                            ("ZakoTypeUpdate", 0x80100004),
                            ("ZakoFrameHook", 0x80100008)):
            def dispatched(cpu, regs, n=name_, target=addr, m=mm_model):
                m.events.append((n, tuple(x & 0xFFFFFFFF for x in
                                           (target, *regs[4:8])), ()))
                return 0
            mips_hooks[addr] = dispatched
        try:
            original = self.cpu.call(FUNCS[name][0], args, hooks=mips_hooks)
        except Exception as exc:
            self.fail(name, args, f"MIPS execution failed: {exc}")
            return False
        actual = self.c.call(name, args)
        self.counts[name] += 1
        ret_kind = FUNCS[name][2]
        if ret_kind != "v" and (int(actual) & 0xFFFFFFFF) != original[2]:
            return self.fail(name, args, f"return C={int(actual) & 0xFFFFFFFF:08X} MIPS={original[2]:08X}")
        expected = bytearray(self.cpu.ram[:0x1FF000])
        got = bytearray(self.c.main[:0x1FF000])
        packet_offset = RENDER_PACKET_BASE - 0x80000000
        expected[packet_offset:packet_offset + RENDER_PACKET_SIZE] = bytes(RENDER_PACKET_SIZE)
        got[packet_offset:packet_offset + RENDER_PACKET_SIZE] = bytes(RENDER_PACKET_SIZE)
        if got != expected:
            off = next(i for i, (x, y) in enumerate(zip(got, expected)) if x != y)
            return self.fail(name, args, f"RAM differs at {0x80000000 + off:08X}: C={got[off:off+8].hex()} MIPS={expected[off:off+8].hex()}")
        if bytes(self.c.scratch) != bytes(self.cpu.scratch):
            return self.fail(name, args, "scratchpad differs")
        m_events = [hook_signature(e) for e in mm_model.events
                    if e[0] != "ZakoDebugDrawPrimitive" and
                    not (name == "ZakoDebugMenuDraw" and e[0] == "FUN_80031330")]
        c_events = [hook_signature(e) for e in c_model.events
                    if e[0] != "ZakoDebugDrawPrimitive"]
        if m_events != c_events:
            return self.fail(name, args, f"hook order differs C={c_events} MIPS={m_events}")
        if self.c.dll.ae_ram_failed(self.c.ram):
            return self.fail(name, args, self.c.dll.ae_ram_error(self.c.ram).decode(errors="replace"))
        return True

    @property
    def coverage(self):
        return getattr(self, "_coverage", set())

    @coverage.setter
    def coverage(self, v): self._coverage = v

    def fail(self, name, args, msg):
        self.failures += 1
        scenario = getattr(self, "current_scenario", 0)
        print(f"FAIL {name}[scenario={scenario}]{tuple(args)}: {msg}", flush=True)
        return False

    def all(self, selector=0):
        self._coverage = set()
        ok = True
        for i, name in enumerate(FUNCS):
            for scenario in range(EXTRA_SCENARIOS.get(name, 0) + 1):
                ok = self.run(name, AREA_IDS[(i + scenario) % len(AREA_IDS)], selector,
                              scenario) and ok
            print(f"{name}: {self.counts[name]} case", flush=True)
        return ok


def run_default(args):
    dll = build()
    h = Harness(dll)
    passed = h.all()
    summary = reg.coverage_summary([(n, a, s, True) for n, (a, s, *_rest) in FUNCS.items()], h.cpu.coverage)
    if args.coverage: reg.print_coverage(summary)
    reg.write_results(h.counts, summary, passed, extra={"seed": args.seed}, function_names=FUNCS)
    print(f"differential: {sum(h.counts.values())} cases; {'PASS' if passed else 'FAIL'}", flush=True)
    return 0 if passed else 1


def run_mutations():
    source_path = ROOT / "src/world/ae_zako1.c"
    source = source_path.read_text(encoding="utf-8")
    # Compile two selector-gated mutants. Each target function gets a distinct
    # selector, so its mutation is active only during that function's direct case.
    caught = 0
    harness = None
    with tempfile.TemporaryDirectory(prefix="zako1_mut_", dir=ROOT / "extract/mips") as td:
        for variant, marker in ((1, 0x11), (2, 0x22)):
            mutated = source
            for index, name in enumerate(FUNCS, 1):
                pattern = re.compile(r"(\b(?:void|int32_t|uint32_t)\s+ae_" + re.escape(name) + r"\s*\([^)]*\)\s*\{)", re.S)
                mutation = (f"\n    if (ae_rd32(r, 0x{MUTATION_SELECTOR:08X}u) == {index}u) "
                            f"ae_wr8(r, 0x{MUTATION_MARKER:08X}u, {marker}u);\n")
                mutated, n = pattern.subn(r"\1" + mutation, mutated, count=1)
                if n != 1: raise SystemExit(f"could not instrument {name}")
            mutant_source = Path(td) / f"variant{variant}.c"
            mutant_source.write_text(mutated, encoding="utf-8")
            mutant_dll = build(mutant_source, f"zako1_mut{variant}")
            if harness is None:
                harness = Harness(mutant_dll)
            else:
                # Reuse the already-built GTE oracle; Windows holds its DLL
                # open for the process lifetime, so rebuilding the same path
                # for a second harness would fail.
                harness.c = CSide(mutant_dll)
            harness._coverage = set()
            for i, name in enumerate(FUNCS, 1):
                # The marker is cleared in the fixture; a semantic mutation
                # must make the byte-exact comparison fail for this target.
                before = harness.failures
                ok = harness.run(name, AREA_IDS[(i - 1) % len(AREA_IDS)], i)
                if not ok and harness.failures > before: caught += 1
                else: print(f"MUTATION SURVIVED variant{variant} {name}", flush=True)
            try: mutant_dll.unlink()
            except OSError: pass
    print(f"mutations caught: {caught}/72", flush=True)
    return 0 if caught == 72 else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--mutate", action="store_true")
    ap.add_argument("--seed", type=int, default=94423)
    args = ap.parse_args()
    return run_mutations() if args.mutate else run_default(args)


if __name__ == "__main__":
    sys.exit(main())
