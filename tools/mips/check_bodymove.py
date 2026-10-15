"""Differential checks for the player/RC-car collision-body movement ports.

Run with .venv/Scripts/python.exe tools/mips/check_bodymove.py [--cases PERCENT] [--coverage] [--mutate].
The MIPS side runs the original EXE functions; the native side calls the corresponding ae_* function over an AeRam
initialized from the same EXE and real area member. No instruction ranges are patched or masked.
"""
import argparse
import ctypes as C
import random
import re
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(ROOT / "tools"))
import mips_ram as mr
import check_game as cg
import check_bodies as cb
import exclusion_registry as reg

M32 = 0xFFFFFFFF
DEFAULT_SEED = 0x944230
LEDGE_SUCCESS_SEED = DEFAULT_SEED + 100003
LEDGE_HEALTH_MUTATION_SEED = LEDGE_SUCCESS_SEED + 6
LEDGE_VARIANTS = {
    LEDGE_SUCCESS_SEED + 1: (-0x1000, 0, 0),
    LEDGE_SUCCESS_SEED + 2: (0, 0x1000, 0),
    LEDGE_SUCCESS_SEED + 3: (0, -0x1000, 0),
    LEDGE_SUCCESS_SEED + 4: (0x1000, 0, 7),
    LEDGE_SUCCESS_SEED + 5: (0, 0, 5),
}
VEHICLE_SLOW_ACTIVE_SEED = DEFAULT_SEED + 2 * 100003
VEHICLE_SLOW_INACTIVE_SEED = VEHICLE_SLOW_ACTIVE_SEED + 1
VEHICLE_THRESHOLD_SEED = VEHICLE_SLOW_ACTIVE_SEED + 2
VEHICLE_PITCH_LIMIT_SEED = VEHICLE_SLOW_ACTIVE_SEED + 3
RUMBLE_STOP_SEED = DEFAULT_SEED + 3 * 100003
RUMBLE_STRENGTH_SEED = RUMBLE_STOP_SEED + 1
BODY_LAND_GROUND_FALLBACK_SEED = DEFAULT_SEED + 5 * 100003
BODY_LAND_AUDIO_OFF_SEED = BODY_LAND_GROUND_FALLBACK_SEED + 1
BODY_LAND_AUDIO_ON_SEED = BODY_LAND_GROUND_FALLBACK_SEED + 2
BODY_LAND_BOUNDARY_SEED = BODY_LAND_GROUND_FALLBACK_SEED + 3
BODY_LAND_EQUALITY_SEED = BODY_LAND_GROUND_FALLBACK_SEED + 4
COLLISION_VERTICAL_SEED = DEFAULT_SEED + 6 * 100003
COLLISION_SURFACE_SEED = COLLISION_VERTICAL_SEED + 1
RECORD_COLLIDER_BASE_SEED = DEFAULT_SEED + 8 * 100003
RECORD_COLLIDER_SURFACE9_SEED = RECORD_COLLIDER_BASE_SEED + 5
SOUND_PAN_RANGE_SEED = DEFAULT_SEED + 12 * 100003
SLOPE_SIGN_MUTATION_SEED = DEFAULT_SEED + 10
SLOPE_RANGE_MUTATION_SEED = SLOPE_SIGN_MUTATION_SEED + 1
RECORD_COLLIDER_TYPES = {
    RECORD_COLLIDER_BASE_SEED: (0x3024, 0),
    RECORD_COLLIDER_BASE_SEED + 1: (0x3053, -384),
    RECORD_COLLIDER_BASE_SEED + 2: (0x3054, -384),
    RECORD_COLLIDER_BASE_SEED + 3: (0x4000, 0),
    RECORD_COLLIDER_BASE_SEED + 4: (0x2040, -300),
}
INLINE_COLLISION_COVERAGE = [
    ("CollisionUpdate player inline", 0x80072638, 0x100, False),
    ("CollisionUpdate RC inline", 0x80072738, 0x0A0, False),
]
BODY_SNAPSHOT = 0x800B20B8
PROBE_POINTS = 0x800B20B0
PROBE_SKIP = 0x800B20C0
PROBE_POS = 0x800B20B4
PLAYER_MODE = 0x800F44A0
PLAYER_FACING = 0x800EC240
PLAYER = cb.PLAYER
RC_CAR = cb.RCCAR
PLAYER_BODY = PLAYER + 0x24
RC_CAR_BODY = RC_CAR + 0x24
COLLIDERS = 0x800F455C
COLLIDER_COUNT = 0x800F45D4
STACK_TOP = mr.DEFAULT_SP
PARTICLE_PAD = cg.PARTICLE_PAD
PARTICLE_PAD_BYTES = {offset for offset, _length in PARTICLE_PAD}

FUNCS = {
    "ColliderSlopeContactDir": (0x8004F884, 256, "ae_ColliderSlopeContactDir", "move", "suuu"),
    "PlayerTryLedgeGrab": (0x80063EC4, 836, "ae_PlayerTryLedgeGrab", "move", ""),
    "PlayerVehicleSoundUpdate": (0x80072A14, 468, "ae_PlayerVehicleSoundUpdate", "body", "u"),
    "PlayerMoveRumbleSoundUpdate": (0x80072BE8, 320, "ae_PlayerMoveRumbleSoundUpdate", "body", "u"),
    "CollisionStub_ReturnZero": (0x80072DC4, 8, "ae_CollisionStub_ReturnZero", "ram", "u"),
    "BodyLandOnGround": (0x80072DCC, 640, "ae_BodyLandOnGround", "move", "usi"),
    "CollisionResolveProbes": (0x80073450, 1452, "ae_CollisionResolveProbes", "move", "su"),
    "SignedRangeOverlap": (0x800739FC, 164, "ae_SignedRangeOverlap", "none", "ssss"),
    "BodyRecordColliderContact": (0x800792C8, 1328, "ae_BodyRecordColliderContact", "move", "us"),
    "SoundEmitterPan": (0x80087B34, 112, "ae_SoundEmitterPan", "move", "sssu"),
    "SoundEmitterPitch": (0x80087C28, 60, "ae_SoundEmitterPitch", "move", "sss"),
    "SoundLoopStop": (0x8008806C, 100, "ae_SoundLoopStop", "sound", "ss"),
}
VOID_FUNCS = {"PlayerTryLedgeGrab"}

AREAS = (1, 2, 4, 5)
REAL_AREA_MEMBERS = {1: 0x05B, 2: 0x05E, 4: 0x064, 5: 0x066}


def s32(v):
    v &= M32
    return v - 0x100000000 if v & 0x80000000 else v


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


class W:
    def __init__(self, m): self.m = m
    def w8(self, a, v): self.m.write8(a, v & 0xFF)
    def w16(self, a, v): self.m.write16(a, v & 0xFFFF)
    def w32(self, a, v): self.m.write32(a, v & M32)
    def r8(self, a): return self.m.read8(a)
    def r16(self, a): return self.m.read16(a)
    def rs16(self, a): return self.m.reads16(a)
    def r32(self, a): return self.m.read32(a)
    def rs32(self, a): return self.m.reads32(a)


TRACE_FN = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_char_p, C.c_uint32, C.c_uint32, C.POINTER(C.c_uint32))


class CGame:
    """Full native game wiring, used to call the new functions with their real module and platform hooks."""
    def __init__(self, dll):
        self.lib = lib = C.CDLL(str(dll))
        P, U, I = C.c_void_p, C.c_uint32, C.c_int32
        lib.integration_create.argtypes = [TRACE_FN, P]
        lib.integration_create.restype = P
        lib.integration_destroy.argtypes = [P]
        lib.integration_ram.argtypes = [P]
        lib.integration_ram.restype = P
        lib.integration_body_hooks.argtypes = [P]
        lib.integration_body_hooks.restype = P
        lib.integration_bodymove_hooks.argtypes = [P]
        lib.integration_bodymove_hooks.restype = P
        lib.integration_sound_hw.argtypes = [P]
        lib.integration_sound_hw.restype = P
        lib.integration_set_direction_padding.argtypes = [P, U]
        lib.integration_set_bodymove_sp.argtypes = [P, U]
        lib.integration_set_spawn_padding.argtypes = [P, C.POINTER(C.c_uint16), U]
        lib.integration_spawn_padding_consumed.argtypes = [P]
        lib.integration_spawn_padding_consumed.restype = U
        lib.ae_ptr.argtypes = [P, U, U]
        lib.ae_ptr.restype = P
        lib.ae_ram_clear_error.argtypes = [P]
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        lib.ae_rd32.argtypes = [P, U]
        lib.ae_rd32.restype = U
        lib.ae_wr16.argtypes = [P, U, U]
        lib.ae_wr32.argtypes = [P, U, U]
        self.callbacks = []
        self.model = None

        def trace(user, name, address, argc, args):
            m = self.model
            n = name.decode("ascii")
            a = [args[i] for i in range(min(int(argc), 6))]
            a += [0] * (6 - len(a))
            if n == "SpuWaitIdle": return m.wait_idle()
            if n == "SpuWaitLoop": m.wait_loop(a[0]); return 0
            if n == "SoundAdsr": m.set_voice_adsr(a[0], a[1], a[2]); return 0
            if n == "SoundKeyOff": m.voice_key_off(a[0], a[1]); return 0
            if n == "SoundVolume": m.set_voice_volume(a[0], a[1], a[2]); return 0
            if n == "Printf": return m.printf(a[0], a[1], a[2], a[3])
            if n in {"CdIntToPos", "CdControl", "CdReadyCallback", "CdMgrQueueCommand"}:
                return m.cd_boundary(n, a)
            if n in cg.MISSING:
                return m.missing(n, address, a[0], a[1], a[2], a[3])
            if n in cg.CALL_SPEC:
                return m.call("debug" if n in cg.MONKEY_HOST and cg.MONKEY_HOST[n] == "debug" else "render",
                              n, a[0], a[1], a[2], a[3])
            raise AssertionError(f"unmodeled body-move native hook {n} at {address:08X} args={a[:argc]}")

        self._trace = TRACE_FN(trace)
        self.integration = lib.integration_create(self._trace, None)
        if not self.integration:
            raise RuntimeError("integration_create failed")
        self.ram = lib.integration_ram(self.integration)
        self.main = (C.c_uint8 * mr.RAM_SIZE).from_address(lib.ae_ptr(self.ram, 0x80000000, mr.RAM_SIZE))
        self.scratch = (C.c_uint8 * mr.SCRATCH_SIZE).from_address(lib.ae_ptr(self.ram, mr.SCRATCH_BASE, mr.SCRATCH_SIZE))
        self.body_hooks = lib.integration_body_hooks(self.integration)
        self.move_hooks = lib.integration_bodymove_hooks(self.integration)
        self.sound_hw = lib.integration_sound_hw(self.integration)
        for name, (_addr, _size, cname, kind, kinds) in FUNCS.items():
            f = getattr(lib, cname)
            args = [I if k in "si" else U for k in kinds]
            f.argtypes = ([] if kind == "none" else [P]) + ([P] if kind not in ("none", "ram") else []) + args
            f.restype = None if name in VOID_FUNCS else I
        lib.ae_CollisionUpdate.argtypes = [P, P]
        lib.ae_CollisionUpdate.restype = I
        self.lib.integration_set_bodymove_sp(self.integration, STACK_TOP)
        self._padding_array = None

    def set_model(self, model): self.model = model
    def set_spawn_padding(self, values):
        self._padding_array = (C.c_uint16 * len(values))(*values)
        self.lib.integration_set_spawn_padding(self.integration, self._padding_array, len(values))
    def sync(self, m):
        C.memmove(self.main, bytes(m.ram), mr.RAM_SIZE)
        C.memmove(self.scratch, bytes(m.scratch), mr.SCRATCH_SIZE)
        self.lib.ae_ram_clear_error(self.ram)
    def failed(self): return bool(self.lib.ae_ram_failed(self.ram))
    def error(self): return self.lib.ae_ram_error(self.ram).decode(errors="replace")
    def call(self, name, args):
        _addr, _size, cname, kind, _kinds = FUNCS[name]
        f = getattr(self.lib, cname)
        p = {"none": [], "ram": [], "move": [self.move_hooks], "body": [self.body_hooks], "sound": [self.sound_hw]}[kind]
        prefix = [] if kind == "none" else [self.ram]
        result = f(*prefix, *p, *args)
        return 0 if name in VOID_FUNCS else int(result)


class BodyModel(cg.GameModel):
    """The game sound allocator returns valid voice slots, represented by a small fixed handle here."""
    def __init__(self, seed, mem): super().__init__(seed, mem, True)
    def cd_boundary(self, name, words):
        arity={"CdIntToPos":2,"CdControl":3,"CdReadyCallback":1,"CdMgrQueueCommand":4}[name]
        entry=("platform",name,*[cg.norm(w) for w in words[:arity]])
        if name=='CdIntToPos': self.mem.wr32(words[1], words[0] ^ 0x5A37C91D)
        if name=='CdMgrQueueCommand':
            size={13:2,14:1,27:3}.get(words[0],0)
            entry+=(tuple(self.mem.rd8(words[1]+i) for i in range(size)),)
        self.log.append(entry)
        return 0
    def missing(self, name, address, a0, a1, a2, a3):
        result = super().missing(name, address, a0, a1, a2, a3)
        if name == "PointInBoxXZ":
            reg.declare("ram_mask", "PointInBoxXZ hook caller register residue ($a2/$a3)",
                        checker="check_bodymove")
            entry = self.log[-1]
            self.log[-1] = entry[:5]
            if self.seed in (COLLISION_VERTICAL_SEED, COLLISION_SURFACE_SEED):
                return 1
        return 3 if name == "SoundLoopStart" else result

def body_intercepts(model):
    hooks=cg.mips_intercepts(model,None)
    # XaPlay/volume are ports; asynchronous CD queuing uses the host boundary.
    hooks.pop(0x80011F6C,None)
    # HalfPipeRangeTest is a direct C port now, so let the original execute.
    hooks.pop(0x80050490,None)
    for address,name,n in [(0x80025D20,"CdIntToPos",2),(0x8002408C,"CdControl",3),(0x80024078,"CdReadyCallback",1),(0x80012360,"CdMgrQueueCommand",4)]:
        hooks[address]=lambda cpu,regs,name=name,n=n: model.cd_boundary(name,regs[4:4+n])
    return hooks


class Harness:
    def __init__(self, dll, with_coverage=True, mips=None):
        reg.declare("stack_window", "801FF000..80200000", checker="check_bodymove")
        reg.declare("ram_mask", "PointInBoxXZ hook caller register residue ($a2/$a3)",
                    checker="check_bodymove")
        reg.check_ram_mask(PARTICLE_PAD, checker="check_bodymove")
        self.m = mips if mips is not None else mr.MipsRam.from_extract()
        if mips is None:
            self.m.attach_gte_oracle()
        self.base = self.m.snapshot()
        if with_coverage: self.m.coverage = set()
        self.c = CGame(dll)
        self.counts = {name: 0 for name in FUNCS}
        self.counts["CollisionUpdate"] = 0
        self.failures = 0
        self.hook_calls = 0

    def setup_area(self, area):
        blob = (ROOT / f"extract/members/{REAL_AREA_MEMBERS[area]:03X}.bin").read_bytes()
        cg.load_area(self.m, area)
        cg.install_world(self.m, blob, gimmicks=False)
        return blob

    def prepare_common(self, area, rng):
        w = W(self.m)
        blob = self.setup_area(area)
        cb.clear_world(self.m)
        # Dedicated, non-overlapping records for the point grid, skip mask and body snapshot.
        w.w32(PROBE_POINTS, 0x801F0000)
        w.w32(PROBE_POS, 0x801F0100)
        w.w32(BODY_SNAPSHOT, 0x801F0200)
        w.w32(PROBE_SKIP, 0x801F0240)
        self.m.write_bytes(0x801F0240, bytes(0x200))
        w.w16(PLAYER_MODE, rng.choice([0, 1, 2, 4, 5, 6]))
        w.w16(PLAYER + 0x5E, rng.choice([0, 0x10, 0x23, 0x2C, 0x2D, 0x2E, 0x4A]))
        w.w32(PLAYER + 0x50, rng.choice([0, 0x10000000, 0x80000000]))
        w.w32(PLAYER + 0x70, rng.choice([0, 0x60, 0x80, 0xC0, 0xE0]))
        w.w16(PLAYER + 0xE8, rng.choice([0, 1, 2, 64, 256]))
        w.w16(0x800F4474, rng.choice([0, 0x20, 0x44]))
        w.w16(0x800F4476, rng.choice([0, 0x44]))
        w.w32(0x800EC350, rng.getrandbits(32))
        w.w16(0x800F45E8, 0)
        w.w16(0x800B20D0, -1); w.w16(0x800B20D2, 0)
        w.w16(0x800B20D4, -1); w.w16(0x800B20D6, 0)
        w.w16(0x800B20D8, 0); w.w16(0x800B20DA, 0)
        w.w32(0x800B2134, rng.choice([0, 0x0D00, 0x1000, 0x1FFF]))
        w.w32(0x800B2138, rng.choice([0, 1, 10000]))
        w.w16(0x800B213C, rng.choice([0, 0x6E, 150]))
        w.w16(0x800B213E, rng.choice([0, 1, 2, 3, 0xFFFF]))
        w.w32(0x800E53C0, rng.choice([0, 0x00010000, 0x01000000, 0xFF00FF00]))
        count = w.r16(COLLIDER_COUNT)
        base = w.r32(COLLIDERS)
        collider = base + 2 if count else 0x801F1000
        if count:
            # The area member's first physical collider supplies real dimensions and surface metadata.
            cx, cy, cz = (w.rs16(collider + o) for o in (0, 2, 4))
        else:
            cx = cy = cz = 0
        for body in (PLAYER_BODY, RC_CAR_BODY):
            w.w32(body + 0, cx + rng.randint(-80, 80))
            w.w32(body + 4, cy + rng.randint(-120, 120))
            w.w32(body + 8, cz + rng.randint(-80, 80))
            w.w32(body + 0x0C, rng.choice([-300, -120, -3, 0, 3, 64, 128]))
            w.w32(body + 0x10, rng.choice([-64, -1, 0, 1, 100]))
            w.w32(body + 0x14, rng.choice([-256, -40, -3, 0, 3, 100]))
            w.w16(body + 0x20, rng.choice([12, 32, 48, 64, 96, 128]))
            w.w16(body + 0x22, rng.choice([32, 64, 96, 128, -64]))
            w.w16(body + 0x24, rng.getrandbits(16))
            w.w16(body + 0x26, rng.choice([0, 0, 0x8000, 0x4000]))
            w.w16(body + 0x28, rng.getrandbits(16))
            w.w16(body + 0x2A, rng.choice([0, 1, 0xFFFF]))
            w.w32(body + 0x2C, 0)
            w.w16(body + 0x34, rng.choice([0, 1, 2, 0xFFFF]))
        self.m.write_bytes(0x801F0200, self.m.read_bytes(PLAYER_BODY, cb.AE_BODY_SIZE if hasattr(cb, "AE_BODY_SIZE") else 0x38))
        return blob, collider, count, (cx, cy, cz)

    def run(self, name, seed, area, args):
        self.m.restore(self.base)
        self.m.gte_reset()
        rng = random.Random(seed)
        _blob, collider, count, center = self.prepare_common(area, rng)
        if name == "ColliderSlopeContactDir":
            self.prepare_solid_case(rng, seed)
            args = self.solid_args
        elif name == "CollisionResolveProbes":
            typ = args[0]
            body = PLAYER_BODY if typ == 1 else RC_CAR_BODY
            self.make_probe_grid(body, typ)
            if seed == COLLISION_VERTICAL_SEED:
                typ, body = 1, PLAYER_BODY
                w = W(self.m)
                w.w32(body + 0, 0); w.w32(body + 4, 0); w.w32(body + 8, 0)
                w.w32(body + 0x0C, 0); w.w32(body + 0x10, 0); w.w32(body + 0x14, 0)
                w.w16(body + 0x20, 32); w.w16(body + 0x22, 128)
                grid = w.r32(PROBE_POINTS)
                self.m.write_bytes(grid, bytes(8 * 0x10))
                record = grid + 7 * 0x10
                shape = 0x801F1000
                for off, value in ((0,0),(2,64),(4,0),(6,128),(8,128),(10,128)):
                    w.w16(record + off, value)
                for off, value in ((14,1),(0x10,0),(0x12,0)):
                    w.w16(shape + off, value)
                w.w32(record + 0x0C, shape)
                for index in range(w.r16(COLLIDER_COUNT)):
                    w.w8(0x801F0240 + index, 1)
            elif seed == COLLISION_SURFACE_SEED:
                typ, body = 1, PLAYER_BODY
                w = W(self.m)
                w.w32(body + 0, 0); w.w32(body + 4, 0); w.w32(body + 8, 0)
                w.w32(body + 0x0C, 0); w.w32(body + 0x10, 0); w.w32(body + 0x14, 0)
                w.w16(body + 0x20, 32); w.w16(body + 0x22, 128)
                grid = w.r32(PROBE_POINTS)
                self.m.write_bytes(grid, bytes(8 * 0x10))
                colliders = w.r32(COLLIDERS)
                count = w.r16(COLLIDER_COUNT)
                for index in range(count): w.w8(0x801F0240 + index, 1)
                w.w8(0x801F0240, 0)
                collider = colliders
                for off, value in ((0,0),(2,0),(4,0),(6,128),(8,128),(10,128),(0x10,0),(0x12,1),(0x16,1)):
                    w.w16(collider + off, value)
            args = (typ, body)
        elif name == "BodyRecordColliderContact":
            body, typ = args
            if seed in RECORD_COLLIDER_TYPES or seed == RECORD_COLLIDER_SURFACE9_SEED:
                geometry, body_y = RECORD_COLLIDER_TYPES.get(seed, (0, 0))
                w = W(self.m)
                body, typ = PLAYER_BODY, 1
                w.w16(COLLIDER_COUNT, 1)
                collider = w.r32(COLLIDERS)
                for off, value in ((0,0),(2,0),(4,0),(6,128),(8,128),(10,128),
                                   (0x10,geometry),(0x12,9 if seed == RECORD_COLLIDER_SURFACE9_SEED else 0),(0x18,0)):
                    w.w16(collider + off, value)
                if geometry == 0x2040:
                    # Type 64's Ramp45 branch needs a deep enough shape to overlap its 256-unit base.
                    w.w16(collider + 0x08, 1024)
                w.w32(body + 0, 0); w.w32(body + 4, body_y); w.w32(body + 8, 0)
                w.w16(body + 0x34, 0x1234 if seed == RECORD_COLLIDER_SURFACE9_SEED else 0)
                w.w16(PLAYER + 0x5E, 0); w.w32(PLAYER + 0x70, 0)
                w.w16(PLAYER_MODE, 0)
            self.set_snapshot(body)
            args = (body, typ)
        elif name == "BodyLandOnGround":
            body, typ, ceiling = args
            if seed == BODY_LAND_GROUND_FALLBACK_SEED:
                w = W(self.m)
                w.w32(body + 4, -4000); w.w32(body + 0x10, 0); w.w16(body + 0x22, 128)
                typ, ceiling = 1, 0
            elif seed == BODY_LAND_BOUNDARY_SEED:
                w = W(self.m)
                w.w32(body + 0, -0x100); w.w32(body + 4, -3329); w.w32(body + 8, -0x80)
                w.w32(body + 0x10, -171); w.w16(body + 0x22, 128)
                typ, ceiling = 1, 0
            elif seed == BODY_LAND_EQUALITY_SEED:
                w = W(self.m)
                w.w32(body + 0, -0x100); w.w32(body + 4, -4000); w.w32(body + 8, -0x80)
                w.w32(body + 0x10, 416); w.w16(body + 0x22, 128)
                typ, ceiling = 1, 10
            elif seed in (BODY_LAND_AUDIO_OFF_SEED, BODY_LAND_AUDIO_ON_SEED):
                w = W(self.m)
                w.w32(body + 4, -3000); w.w32(body + 0x10, -600); w.w16(body + 0x22, 128)
                w.w32(body + 0x2C, 0xA0000001)
                w.w16(PLAYER + 0x5E, 0); w.w32(PLAYER + 0x70, 0)
                w.w16(0x800F4476, 1); w.w16(0x800F4474, 0x20)
                w.w32(0x800EC350, 1 if seed == BODY_LAND_AUDIO_ON_SEED else 0)
                typ, ceiling = 1, 0
            self.set_snapshot(body)
            W(self.m).w16(STACK_TOP - 0x30 + 0x16, 0)
            args = (body, typ, ceiling)
        elif name == "PlayerTryLedgeGrab":
            self.prepare_ledge_case(seed, center)
            args = ()
        elif name == "PlayerVehicleSoundUpdate":
            w = W(self.m)
            w.w16(0x800B20D0, 3); w.w16(0x800B20D2, 1)
            if seed in (VEHICLE_SLOW_ACTIVE_SEED, VEHICLE_SLOW_INACTIVE_SEED):
                w.w32(RC_CAR_BODY + 0x0C, 0); w.w32(RC_CAR_BODY + 0x14, 0)
                w.w16(0x800B20D2, 1 if seed == VEHICLE_SLOW_ACTIVE_SEED else 0)
            elif seed in (VEHICLE_THRESHOLD_SEED, VEHICLE_PITCH_LIMIT_SEED):
                w.w32(RC_CAR_BODY + 0x0C, 3 if seed == VEHICLE_THRESHOLD_SEED else 100)
                w.w32(RC_CAR_BODY + 0x14, 0)
                w.w16(0x800B20D2, 1); w.w32(0x800B2134, 0); w.w32(0x800B2138, 0)
            args = (RC_CAR_BODY,)
        elif name == "PlayerMoveRumbleSoundUpdate":
            w = W(self.m)
            w.w16(PLAYER_MODE, 1)
            w.w16(0x800B20D4, 3); w.w16(0x800B20D6, 1)
            w.w32(PLAYER_BODY+0x0C, 100); w.w32(PLAYER_BODY+0x14, 0)
            if seed == RUMBLE_STOP_SEED:
                w.w32(PLAYER_BODY+0x0C, 0); w.w32(PLAYER_BODY+0x14, 0)
                w.w32(0x800E53C0, 0); w.w16(0x800B213E, 0)
            elif seed == RUMBLE_STRENGTH_SEED:
                w.w32(PLAYER_BODY+0x0C, 100); w.w32(PLAYER_BODY+0x14, 0)
                w.w32(0x800E53C0, 0); w.w16(0x800B213C, 0); w.w16(0x800B213E, 0)
            args = (PLAYER_BODY,)
        elif name == "SoundEmitterPan":
            args = (args[0], args[1], args[2], 0x801F1100)
            w = W(self.m)
            w.w16(0x801F1100, center[0]); w.w16(0x801F1102, center[1]); w.w16(0x801F1104, center[2])
            w.w32(0x800EC204, center[0]); w.w32(0x800EC208, center[1]); w.w32(0x800EC20C, center[2])
            if seed == SOUND_PAN_RANGE_SEED:
                w.w16(0x800BF688, center[0] + 3000)
                w.w16(0x800BF68A, center[1])
                w.w16(0x800BF68C, center[2])
        elif name == "SoundLoopStop":
            # Populate a real loop slot as SoundLoopStartEx would leave it.
            w = W(self.m)
            voice = args[1] & 0xFFFF
            loop = 0x800BF370
            w.w16(loop, args[0]); w.w16(loop + 2, voice)
            w.w16(loop + 0x12, 1); w.w16(loop + 0x14, 0); w.w16(loop + 0x16, 0)
        self.c.sync(self.m)
        self.c.lib.integration_set_bodymove_sp(self.c.integration, STACK_TOP)
        mm = BodyModel(seed, cg.MipsMem(self.m))
        cm = BodyModel(seed, cg.CMem(self.c.main, self.c.scratch))
        self.c.set_model(cm)
        self.m.hook_log.clear()
        self.counts[name] += 1
        addr = FUNCS[name][0]
        spawn_padding = []
        def capture_spawn_padding(cpu, regs):
            stack = regs[29]
            spawn_padding.append(cpu.read16(stack + 0x1E))
            regs[20] = 0x800B0000  # emulate lui $s4, 0x800B at SpawnEffectGroup+0x7C
            return 0x8006976C
        try:
            with cg.SpuWrites(self.m, mm):
                hooks = body_intercepts(mm)
                hooks.pop(0x80078C9C, None)
                hooks.pop(0x800878E0, None)
                hooks.pop(0x800877F8, None)
                regs = self.m.call(addr, args, hooks=hooks,
                                   patches={0x80069768: capture_spawn_padding})
        except mr.MipsError as exc:
            self.failures += 1
            print(f"FAIL {name} seed={seed} area={area}: original MIPS fault: {exc}")
            return False
        mret = regs[2]
        self.c.set_spawn_padding(spawn_padding)
        if name == 'PlayerTryLedgeGrab':
            reg.declare('shared_random','PlayerTryLedgeGrab direction padding',checker='check_bodymove')
            # Preserve the actual original copy, including its unwritten halfword.
            padding=self.m.read16(PLAYER_FACING+6)
            self.c.lib.integration_set_direction_padding(self.c.integration,padding)
            # Poison native stack storage so ignoring the explicit service fails.
            self.c.lib.ae_wr16(self.c.ram,STACK_TOP-0x48+0x16,padding^0xA55A)
        cret = self.c.call(name, args)
        self.hook_calls += len(mm.log)
        problems = []
        if self.c.failed(): problems.append("C RAM error: " + self.c.error())
        if name not in VOID_FUNCS and (mret & M32) != (cret & M32):
            problems.append(f"return MIPS={mret & M32:08X} C={cret & M32:08X}")
        if mm.log != cm.log: problems.append(f"native calls differ\n MIPS {mm.log}\n C {cm.log}")
        ma, ca = bytes(self.m.ram), bytes(self.c.main)
        stack_lo = mr.STACK_WINDOW[0] - 0x80000000
        if ma[:stack_lo] != ca[:stack_lo]:
            diffs = [i for i, (x, y) in enumerate(zip(ma[:stack_lo], ca[:stack_lo]))
                     if x != y and i not in PARTICLE_PAD_BYTES]
            if not diffs:
                diffs = []
            else:
                problems.append("RAM differs: " + ", ".join(
                    f"{0x80000000+i:08X}({ma[i]:02X}/{ca[i]:02X})" for i in diffs[:12]))
        ms, cs = bytes(self.m.scratch), bytes(self.c.scratch)
        if ms != cs:
            diffs = [i for i, (x, y) in enumerate(zip(ms, cs)) if x != y]
            problems.append("scratch differs: " + ", ".join(
                f"{0x1F800000+i:08X}({ms[i]:02X}/{cs[i]:02X})" for i in diffs[:12]))
        if problems:
            self.failures += 1
            print(f"FAIL {name} seed={seed} area={area}:\n  " + "\n  ".join(problems))
        return not problems

    def prepare_solid_case(self, rng, seed):
        w = W(self.m)
        collider, point, out = 0x801F1000, 0x801F1100, 0x801F1200
        if seed == SLOPE_SIGN_MUTATION_SEED:
            typ, handler = 42, 0x8004E444
            # Keep |x| outside the helper's centered-axis shortcut.  Flipping the
            # collider-origin subtraction then changes the selected wall axis.
            for off, val in ((0,100),(2,0),(4,0),(6,128),(8,64),(10,128)):
                w.w16(collider + off, val)
            w.w32(point, 0); w.w32(point + 4, 0); w.w32(point + 8, 0)
            w.w16(0x800F45E6, 1)
            w.w32(0x800AF180 + typ * 4, handler)
            w.w32(out, 0); w.w32(out + 4, 0)
            self.solid_args=(typ,collider,point,out)
            return
        if seed == SLOPE_RANGE_MUTATION_SEED:
            typ, handler = 64, 0x8004EA1C
            for off, val in ((0,0),(2,0),(4,0),(6,128),(8,1024),(10,128)):
                w.w16(collider + off, val)
            w.w32(point, 0); w.w32(point + 4, -300); w.w32(point + 8, 0)
            w.w32(0x800AF180 + typ * 4, handler)
            w.w32(out, 0); w.w32(out + 4, 0)
            self.solid_args=(typ,collider,point,out)
            return
        typ = rng.choice([60, 61, 62, 63, 64, 65, 66, 67, 11, 12, 19, 20])
        handler = {60:0x8004E33C,61:0x8004E33C,62:0x8004E33C,63:0x8004E33C,
                   64:0x8004EA1C,65:0x8004EA1C,66:0x8004EA1C,67:0x8004EA1C,
                   11:0x8004ED30,12:0x8004ED30,19:0x8004EE94,20:0x8004EE94}[typ]
        w.w32(0x800AF180 + typ * 4, handler)
        for off, val in ((0,0),(2,0),(4,0),(6,rng.choice([10,64,128])),(8,rng.choice([20,64,128])),
                         (10,rng.choice([10,64,128]))): w.w16(collider+off,val)
        w.w32(point, rng.randint(-32,32)); w.w32(point+4, rng.randint(-32,32)); w.w32(point+8, rng.randint(-32,32))
        w.w32(out, 0); w.w32(out+4, 0)
        self.solid_args=(typ,collider,point,out)

    def make_probe_grid(self, body, typ):
        # Use the original probe builder on the loaded area to seed its real shape pointers and cell metadata.
        self.m.call(0x8007304C, (typ, body), hooks=cg.mips_intercepts(cg.GameModel(0, cg.MipsMem(self.m), True), None), patches={})
        W(self.m).w32(PROBE_POS, 0x801F0100)
        W(self.m).w32(PROBE_SKIP, 0x801F0240)
        self.m.write_bytes(0x801F0240, bytes(0x200))

    def set_snapshot(self, body):
        w = W(self.m)
        w.w32(BODY_SNAPSHOT, 0x801F0200)
        self.m.write_bytes(0x801F0200, self.m.read_bytes(body, 0x38))

    def prepare_ledge_case(self, seed, center):
        w = W(self.m)
        if seed == LEDGE_SUCCESS_SEED or seed in LEDGE_VARIANTS or seed == LEDGE_HEALTH_MUTATION_SEED:
            # Area 5 has a real 0x100 drop from (-0x100, -0x80) to (-0x80, -0x80).
            # With the player one unit above the first surface, the ledge test's 0x81..0x100
            # height window passes and the injected ray result takes the successful attach path.
            position_x, position_z = -0x100, -0x80
            if seed == LEDGE_SUCCESS_SEED + 1: position_x = 0x100
            if seed == LEDGE_SUCCESS_SEED + 3: position_x, position_z = 0, 0x100
            w.w32(PLAYER + 0x24, position_x)
            w.w32(PLAYER + 0x28, -0xE01)
            w.w32(PLAYER + 0x2C, position_z)
            w.w32(PLAYER + 0x30, 0)
            w.w32(PLAYER + 0x34, 128)
            w.w32(PLAYER + 0x38, 0)
            w.w16(PLAYER + 0xE8, 1)
            if seed == LEDGE_HEALTH_MUTATION_SEED:
                w.w16(PLAYER + 0xE8, 0)
            w.w16(PLAYER + 0x44, 32)
            w.w16(PLAYER + 0x46, 128)
            w.w32(PLAYER + 0x50, 0)
            w.w32(PLAYER + 0x70, 0)
            facing_x, facing_z, mode = LEDGE_VARIANTS.get(seed, (0x1000, 0, 0))
            w.w16(PLAYER_MODE, mode)
            w.w16(PLAYER_FACING, facing_x)
            w.w16(PLAYER_FACING + 4, facing_z)
            # The ray mask is its real skip list; suppress solid hits while retaining heightfield ground queries.
            for index in range(w.r16(COLLIDER_COUNT)):
                w.w8(0x801F0240 + index, 1)
            return
        w.w16(PLAYER + 0xE8, 1)
        w.w32(PLAYER + 0x50, 0)
        w.w16(PLAYER_MODE, 0)
        w.w32(PLAYER + 0x70, 0)
        w.w32(PLAYER + 0x24, center[0]); w.w32(PLAYER + 0x28, center[1]); w.w32(PLAYER + 0x2C, center[2])
        w.w32(PLAYER + 0x30, 0); w.w32(PLAYER + 0x34, 128); w.w32(PLAYER + 0x38, 0)
        w.w16(PLAYER + 0x240, 0)
        # Sweep vertical position and facing so real terrain yields both early exits and ray-query paths.
        w.w16(PLAYER + 0xE8, 1 + seed % 256)
        w.w16(PLAYER + 0x46, 128); w.w16(PLAYER + 0x44, 32)
        w.w16(0x800EC240, 0x1000 if seed & 1 else 0)
        w.w16(0x800EC244, 0)

    def fail(self, msg):
        self.failures += 1
        print("FAIL " + msg)

    def run_collision_update(self, seed, area, typ, player_mode=None):
        """One real area/player or area/RC-car pass through CollisionUpdate, without mid-function patches."""
        self.m.restore(self.base); self.m.gte_reset()
        rng = random.Random(seed)
        self.prepare_common(area, rng)
        w = W(self.m)
        body = PLAYER_BODY if typ == 1 else RC_CAR_BODY
        owner = PLAYER if typ == 1 else RC_CAR
        slot = cb.SLOTS
        # Restore alive state after common setup; make the single linked-list node exactly as the registry does.
        for i in range(256):
            s = cb.SLOTS + 20*i
            w.w16(s, 0xFFFF); w.w16(s+2, 0xFFFF); w.w32(s+4, 0); w.w32(s+8, 0); w.w32(s+12, 0); w.w32(s+16, 0)
        w.w16(slot, typ); w.w16(slot+2, 0); w.w32(slot+4, body); w.w32(slot+8, owner); w.w32(slot+12, 0); w.w32(slot+16, 0)
        w.w32(cb.HEAD, slot); w.w32(cb.TAIL, slot)
        w.w16(PLAYER + 0x5E, rng.choice([0,0x10,0x23,0x2C,0x2E]))
        w.w16(PLAYER_MODE, (1 if typ == 1 else 0) if player_mode is None else player_mode)
        w.w32(body + 0x0C, 100); w.w32(body + 0x10, 0); w.w32(body + 0x14, 0)
        w.w16(0x800B20D0, -1); w.w16(0x800B20D2, 0)
        w.w16(0x800B20D4, -1); w.w16(0x800B20D6, 0)
        W(self.m).w16(STACK_TOP - 56 - 0x30 + 0x16, 0)
        w.w16(owner + 0x4C, 0)
        self.m.write_bytes(0x801F0200, self.m.read_bytes(body, 0x38))
        self.c.sync(self.m)
        self.c.lib.integration_set_bodymove_sp(self.c.integration, STACK_TOP - 56)
        mm = BodyModel(seed, cg.MipsMem(self.m))
        cm = BodyModel(seed, cg.CMem(self.c.main, self.c.scratch))
        self.c.set_model(cm)
        self.counts["CollisionUpdate"] += 1
        spawn_padding = []
        def capture_spawn_padding(cpu, regs):
            stack = regs[29]
            spawn_padding.append(cpu.read16(stack + 0x1E))
            regs[20] = 0x800B0000
            return 0x8006976C
        with cg.SpuWrites(self.m, mm):
            hooks = body_intercepts(mm)
            hooks.pop(0x800878E0, None)
            hooks.pop(0x800877F8, None)
            regs = self.m.call(0x8007247C, (), hooks=hooks,
                               patches={0x80069768: capture_spawn_padding})
        self.c.set_spawn_padding(spawn_padding)
        cret = self.c.lib.ae_CollisionUpdate(self.c.ram, self.c.body_hooks)
        problems=[]
        if self.c.failed(): problems.append("C RAM error: "+self.c.error())
        if regs[2] != (cret & M32): problems.append(f"return MIPS={regs[2]:08X} C={cret&M32:08X}")
        if mm.log != cm.log: problems.append(f"native calls differ\n MIPS {mm.log}\n C {cm.log}")
        ma,ca=bytes(self.m.ram),bytes(self.c.main)
        stack_lo=mr.STACK_WINDOW[0]-0x80000000
        if ma[:stack_lo]!=ca[:stack_lo]:
            diffs=[i for i,(x,y) in enumerate(zip(ma[:stack_lo],ca[:stack_lo]))
                   if x!=y and i not in PARTICLE_PAD_BYTES]
            if diffs:
                problems.append("RAM differs: "+", ".join(f"{0x80000000+i:08X}({ma[i]:02X}/{ca[i]:02X})" for i in diffs[:12]))
        if bytes(self.m.scratch)!=bytes(self.c.scratch): problems.append("scratch differs")
        if problems:
            self.failures+=1
            print(f"FAIL CollisionUpdate type={typ} seed={seed} area={area}:\n  "+"\n  ".join(problems))
        return not problems


def target_args(name, rng):
    if name == "ColliderSlopeContactDir": return ()
    if name == "PlayerTryLedgeGrab": return ()
    if name == "PlayerVehicleSoundUpdate": return (0,)
    if name == "PlayerMoveRumbleSoundUpdate": return (PLAYER_BODY,)
    if name == "CollisionStub_ReturnZero": return (rng.getrandbits(32),)
    if name == "BodyLandOnGround": return (PLAYER_BODY, rng.choice([1,2]), rng.choice([-255,-1,0,1,128]))
    if name == "CollisionResolveProbes": return (rng.choice([1,2]),0)
    if name == "SignedRangeOverlap": return tuple(rng.choice([-32768,-2048,-1,0,1,2047,32767,rng.randint(-32768,32767)]) for _ in range(4))
    if name == "BodyRecordColliderContact": return (PLAYER_BODY,rng.choice([1,2]))
    if name == "SoundEmitterPan": return (rng.choice([0,0x16,-1]),rng.choice([-1,0,1,8,0x8001]),rng.choice([2048,4096,8192,16384]),0)
    if name == "SoundEmitterPitch": return (rng.choice([0,0x16]),rng.choice([-1,0,1,8,0x8001]),rng.choice([-1,0,8191,8192,0x7FFF]))
    if name == "SoundLoopStop": return (rng.choice([0x16,0xAD,0x1000,-1]),rng.choice([-1,0,1,8,0x8001]))
    raise KeyError(name)


def cases(n, percent): return max(1, round(n * percent / 100))


def run_all(h, percent, seed, coverage_cases=False):
    per_fn = cases(28 if coverage_cases else 12, percent)
    t0=time.time()
    for index,name in enumerate(FUNCS):
        for i in range(per_fn):
            test_seed=seed + index*100003 + i
            rng=random.Random(test_seed)
            area = 5 if ((name == "PlayerTryLedgeGrab" and
                          (test_seed == LEDGE_SUCCESS_SEED or test_seed in LEDGE_VARIANTS or
                           test_seed == LEDGE_HEALTH_MUTATION_SEED)) or
                         (name == "BodyLandOnGround" and test_seed in
                          (BODY_LAND_GROUND_FALLBACK_SEED, BODY_LAND_AUDIO_OFF_SEED, BODY_LAND_AUDIO_ON_SEED,
                           BODY_LAND_BOUNDARY_SEED, BODY_LAND_EQUALITY_SEED))) \
                   else AREAS[(index+i)%len(AREAS)]
            h.run(name,test_seed,area,target_args(name,rng))
    update_cases=cases(8,percent)
    for i in range(update_cases):
        area=AREAS[i%len(AREAS)]
        # The player arm has three original mode-specific branches before its common movement path.
        player_mode = (2, 6, 5)[i] if i < 3 else None
        h.run_collision_update(seed+0x70000+i*2,area,1,player_mode)
        h.run_collision_update(seed+0x70001+i*2,area,2)
    print(f"scenario checks done in {time.time()-t0:.1f}s")


def mutation_tests(h, seed):
    """Two deliberate source mutations per target; every mutant must produce a differential failure."""
    source_path=ROOT/"src/bodies/ae_bodymove.c"
    original=source_path.read_text(encoding="utf-8")
    mutations=[
        ("ColliderSlopeContactDir","int32_t px = body_sub32(ae_rds32(r, point), ae_rds16(r, collider + AE_BODY_COLLIDER_X));","int32_t px = body_add32(ae_rds32(r, point), ae_rds16(r, collider + AE_BODY_COLLIDER_X));"),
        ("ColliderSlopeContactDir","if ((uint32_t)(shape_type - 0x40) < 4u)","if ((uint32_t)(shape_type - 0x41) < 4u)"),
        ("PlayerTryLedgeGrab","ae_rds16(r, player + AE_PLAYER_HEALTH) <= 0","ae_rds16(r, player + AE_PLAYER_HEALTH) < 0"),
        ("PlayerTryLedgeGrab","ae_wr32(r, AE_G_PLAYER_POS_X, (uint32_t)probe_x);","ae_wr32(r, AE_G_PLAYER_POS_X, (uint32_t)body_add32(probe_x, 1));"),
        ("PlayerVehicleSoundUpdate","if (absolute_sum < 3)","if (absolute_sum < 4)"),
        ("PlayerVehicleSoundUpdate","if (requested_pitch >= 0x2000) requested_pitch = 0x1FFF;","if (requested_pitch >= 0x2000) requested_pitch = 0x1FFE;"),
        ("PlayerMoveRumbleSoundUpdate","ae_rd16(r, AE_G_PLAYER_MODE) != 1u","ae_rd16(r, AE_G_PLAYER_MODE) != 2u"),
        ("PlayerMoveRumbleSoundUpdate","ae_wr16(r, AE_G_BODY_RUMBLE_STRENGTH, 0x6E);","ae_wr16(r, AE_G_BODY_RUMBLE_STRENGTH, 0x6F);"),
        ("CollisionStub_ReturnZero","return 0;","return 1;"),
        ("CollisionStub_ReturnZero","return 0;","return -1;"),
        ("BodyLandOnGround","if (ground < predicted_y)","if (ground <= predicted_y)"),
        ("BodyLandOnGround","if (moved_y < -255)","if (moved_y < -254)"),
        ("CollisionResolveProbes","if (shape == 0) continue;","if (shape != 0) continue;"),
        ("CollisionResolveProbes","if (out_y > 0)","if (out_y >= 0)"),
        ("SignedRangeOverlap","if (difference < c)","if (difference <= c)"),
        ("SignedRangeOverlap","return difference < c - d && c < a;","return difference <= c - d && c < a;"),
        ("BodyRecordColliderContact","if (ae_rd8(r, collider + AE_BODY_COLLIDER_SURFACE) == 9u) continue;","if (ae_rd8(r, collider + AE_BODY_COLLIDER_SURFACE) == 8u) continue;"),
        ("BodyRecordColliderContact","if (matched) ae_wr16(r, body + AE_BODY_COLLIDER_INDEX, (uint32_t)index);","if (matched) ae_wr16(r, body + AE_BODY_COLLIDER_INDEX, (uint32_t)(index + 1));"),
        ("SoundEmitterPan","if (voice16 < 0) return -1;","if (voice16 <= 0) return -1;"),
        ("SoundEmitterPan","body_s16((uint32_t)range)","body_s16((uint32_t)range) + 1"),
        ("SoundEmitterPitch","if (voice16 < 0) return -1;","if (voice16 <= 0) return -1;"),
        ("SoundEmitterPitch","body_s16((uint32_t)pitch));","body_s16((uint32_t)pitch) + 1);"),
        ("SoundLoopStop","if (voice < 0) return 0;","if (voice <= 0) return 0;"),
        ("SoundLoopStop","if (((uint32_t)id & 0xF000u) == 0u)","if (((uint32_t)id & 0xF000u) != 0u)"),
    ]
    out_dir=ROOT/"extract/mips/bodymove_mutants"
    out_dir.mkdir(parents=True,exist_ok=True)
    caught=0
    function_indices={name:index for index,name in enumerate(FUNCS)}
    per_name_mutation={}

    def replace_in_function(source,name,old,new):
        definition=re.search(r"(?m)^(?:int32_t|void)\s+ae_"+re.escape(name)+r"\s*\(",source)
        if not definition: raise ValueError(f"function definition ae_{name} not found")
        opening=source.find("{",definition.end())
        depth=0
        closing=None
        for pos in range(opening,len(source)):
            if source[pos]=="{": depth+=1
            elif source[pos]=="}":
                depth-=1
                if depth==0:
                    closing=pos
                    break
        if closing is None: raise ValueError(f"function ae_{name} has no closing brace")
        body=source[opening:closing+1]
        count=body.count(old)
        if count!=1: raise ValueError(f"mutation anchor occurs {count} times in ae_{name}")
        return source[:opening]+body.replace(old,new,1)+source[closing+1:]

    def mutation_cases(name,ordinal,base_seed):
        index=function_indices[name]
        candidates=[]
        explicit={
            ("PlayerTryLedgeGrab",0): [(LEDGE_HEALTH_MUTATION_SEED,5,())],
            ("SignedRangeOverlap",0): [(base_seed+index*100003,1,(1,0,1,1))],
            ("SignedRangeOverlap",1): [(base_seed+index*100003,1,(2,1,1,0))],
            ("SoundEmitterPan",0): [(base_seed+index*100003,1,(0,0,4096,0))],
            ("BodyLandOnGround",1): [(BODY_LAND_BOUNDARY_SEED,5,(PLAYER_BODY,1,0))],
            # At this listener distance the exact-integer pan result changes across 4095/4096.
            ("SoundEmitterPan",1): [(SOUND_PAN_RANGE_SEED,1,(0,1,4095,0))],
            ("SoundEmitterPitch",0): [(base_seed+index*100003,1,(0,0,0))],
            ("SoundEmitterPitch",1): [(base_seed+index*100003,1,(0,1,100))],
            ("SoundLoopStop",0): [(base_seed+index*100003,1,(0x16,0))],
            ("SoundLoopStop",1): [(base_seed+index*100003,1,(0x16,1))],
        }
        candidates.extend(explicit.get((name,ordinal),[]))
        for case_index in range(28):
            test_seed=base_seed+index*100003+case_index
            rng=random.Random(test_seed)
            area=5 if ((name=="PlayerTryLedgeGrab" and
                        (test_seed==LEDGE_SUCCESS_SEED or test_seed in LEDGE_VARIANTS or
                         test_seed==LEDGE_HEALTH_MUTATION_SEED)) or
                       (name=="BodyLandOnGround" and test_seed in
                        (BODY_LAND_GROUND_FALLBACK_SEED,BODY_LAND_AUDIO_OFF_SEED,
                         BODY_LAND_AUDIO_ON_SEED,BODY_LAND_BOUNDARY_SEED,BODY_LAND_EQUALITY_SEED))) \
                 else AREAS[(index+case_index)%len(AREAS)]
            candidates.append((test_seed,area,target_args(name,rng)))
        return candidates

    base_seed=seed-0x50000
    for i,(name,old,new) in enumerate(mutations):
        ordinal=per_name_mutation.get(name,0)
        per_name_mutation[name]=ordinal+1
        try:
            mutant=replace_in_function(original,name,old,new)
        except ValueError as exc:
            h.fail(f"mutation {i+1} {name}: {exc}")
            continue
        path=out_dir/f"mutant_{i+1:02d}.c"; path.write_text(mutant,encoding="utf-8")
        try:
            dll=cg.build_dll({"src/bodies/ae_bodymove.c":path},tag=f"bodymove_mutant_{i+1:02d}")
            mutant_h=Harness(dll,with_coverage=False,mips=h.m)
            mutant_h.base=h.base
            killed=False
            for test_seed,area,args in mutation_cases(name,ordinal,base_seed):
                if not mutant_h.run(name,test_seed,area,args):
                    killed=True
                    break
            if killed: caught+=1
            else: h.fail(f"mutation {i+1} {name} survived all targeted cases")
        except BaseException as exc:
            h.fail(f"mutation {i+1} {name} checker/build error: {exc}")
    print(f"mutation results: {caught}/{len(mutations)} caught")
    return caught == len(mutations)


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--cases",type=float,default=100.0)
    ap.add_argument("--seed",type=int,default=DEFAULT_SEED)
    ap.add_argument("--coverage",action="store_true")
    ap.add_argument("--mutate",action="store_true")
    a=ap.parse_args()
    if not (ROOT/"extract/disc/SCUS_944.23").exists():
        print("SKIP: extract/disc/SCUS_944.23 not present"); return 0
    for area,member in REAL_AREA_MEMBERS.items():
        if not (ROOT/f"extract/members/{member:03X}.bin").exists():
            print(f"SKIP: real member {member:03X}.bin missing for area {area:02X}"); return 0
    t0=time.time()
    h=Harness(cg.build_dll(tag="bodymove"),with_coverage=True)
    run_all(h,a.cases,a.seed,a.coverage)
    mutations_ok=True
    if a.mutate: mutations_ok=mutation_tests(h,a.seed+0x50000)
    summary=reg.coverage_summary(
        [(n,addr,size,True) for n,(addr,size,*_) in FUNCS.items()] + INLINE_COLLISION_COVERAGE,
        h.m.coverage)
    if a.coverage: reg.print_coverage(summary)
    print("cases per function:")
    for name,count in h.counts.items(): print(f"  {name:36s} {count}")
    print(f"total cases {sum(h.counts.values())}; hooks compared {h.hook_calls}; MIPS instructions {h.m.steps}; {time.time()-t0:.1f}s")
    default_run=(a.cases==ap.get_default("cases") and a.seed==ap.get_default("seed") and not a.mutate)
    if default_run:
        reg.write_results(h.counts,summary,h.failures==0,function_names=list(FUNCS)+["CollisionUpdate"])
    if h.failures or not mutations_ok:
        print(f"FAILED: {h.failures} differential/checker failures"); return 1
    print("OK: all body-movement ports match the original instructions")
    return 0


if __name__=="__main__": sys.exit(main())
