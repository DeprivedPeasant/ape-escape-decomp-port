"""Integration differential check of the game host (src/game/ae_game.c): proves the WIRING of the ported RAM-image modules.

The unit checkers (check_monkey_*.py, check_bodies.py, check_world.py, ...) test each module against the original code with a
hook table modelled in Python.  This checker runs the REAL host instead: a DLL of src/game/ae_game.c plus every module it
links, driven through the test shim tools/mips/game_shim.c (an AeGame whose AeGameHost callbacks forward to Python).  One game
frame (ae_game_frame = MonkeyUpdateAll, then CollisionUpdate) is compared with the ORIGINAL instructions of the same frame
(MonkeyUpdateAll 80042E18, then CollisionUpdate 8007247C) on identical RAM images, for several consecutive frames:

  * every function that ae_game.c wires to a port runs as original instructions on the MIPS side (GimmickGetInfo, PlaySfxAt,
    ProjectileSpawn, SpawnParticle, GetGroundHeight, GetSolidHeight, GimmickEvent / GimmickActivate, the sound functions, ...);
  * exactly the functions the C side sends to the host are intercepted on the MIPS side and answered by the same Python model:
    the MISSING table of ae_game.c (unported game logic), the rendering / debug-menu / Printf / VSync hooks of the monkey hook
    table (src/monkeys/ae_monkey.h), the sound-chip leaves behind AeSoundHw (SpuWaitIdle, SpuWaitLoop, the ADSR and key-off
    register writes), plus the BIOS memset that ClearWords jumps to (not a host call: filled in place);
  * the final RAM (outside the stack window, tools/mips/check_exclusions.csv), the scratchpad and the sequence of host calls
    (kind, name, address, arguments) must be identical after every frame.

ae_game.c functions are glue, not original functions: there are no tools/verified.csv rows (notes/port/validation/ledger.md).
Needs extract/disc/SCUS_944.23 and extract/members (skips if missing).

Usage: .venv/Scripts/python.exe tools/mips/check_game.py [--cases PERCENT] [--seed S] [--frames N] [--mutate]
"""
import argparse
import collections
import ctypes as C
import hashlib
import json
import os
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
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(ROOT / "tools"))
import mips_ram as mr  # noqa: E402
import monkey_harness as mh  # noqa: E402
import monkey_scen as sc  # noqa: E402
import monkey_scen5 as s5  # noqa: E402
import check_bodies as cb  # noqa: E402
import check_sound as ks  # noqa: E402
import check_pose as kp  # noqa: E402
import exclusion_registry as reg  # noqa: E402
from monkey_harness import W, MONKEYS, MONKEY_SIZE, NAV_BASE  # noqa: E402
from monkey_scen import M32, s32, s16  # noqa: E402

OUT = ROOT / "extract/mips"
STACK_LO, STACK_HI = 0x801FF000, 0x80200000
PLAYER, RCCAR = cb.PLAYER, cb.RCCAR

H_CASES = 16                          # cases per hook of the per-hook layer at the default scale

# ---------------------------------------------------------------------------------------------------------------- tables
# The MISSING table of ae_game.c: name (as reported to AeGameHost.missing), original address (the intercept address on the MIPS
# side; number of meaningful arguments.
MISSING = {
    "ModelInit": (0x80015AC0, 2), "PlaySfx": (0x800877F8, 1), "PlayPlayerVoice": (0x8006896C, 1),
    "CollisionRayTest": (0x80078C9C, 2), "GimmickTypeToSfx": (0x8005BB98, 1), "PickupCollect": (0x80078A18, 1),
    "CameraNudge": (0x80047690, 3), "EventCamStart": (0x8004D39C, 1), "FUN_8013d358 (area 0x29 overlay)": (0x8013D358, 3),
    "FUN_801414a0 (gimmick overlay)": (0x801414A0, 1), "PlayerEventCallback": (0x800B027C, 1),
    "SfxVoiceSetPitch": (0x800A35FC, 2), "XaPlay": (0x80011F6C, 1),
    "PointInBoxXZ": (0x80073B64, 4),
}
# Monkey hooks that ae_game.c sends to the host (everything else of the monkey hook table is wired to a port).
MONKEY_HOST = {
    "Printf": "printf", "ModelInit": "missing", "VSync": "vblank",
    "ModelTransformRigid": "render", "ModelDraw": "render", "SetRotMatrix": "render", "SetTransMatrix": "render",
    "RotTransPers": "render", "AlertIconDraw": "render", "ModelTransformSkinned": "render", "ModelDrawWithOt": "render",
    "DrawShadow": "render", "ColorFadeSet": "render",
    "MenuItemInitBound": "debug", "MenuItemSetRange": "debug", "MenuItemSetCallback": "debug", "MenuItemSetStep": "debug",
    "DebugTextPrintfWhite": "debug",
}
# argument counts / peeks / effects of the render and debug callbacks (the monkey hook model, plus the two world / gimmick ones)
CALL_SPEC = {n: mh.HOOK_SPECS[n] for n, k in MONKEY_HOST.items() if k in ("render", "debug")}
CALL_SPEC["MeshInitFromAreaTable"] = dict(n=2)
CALL_SPEC["PropSetMesh"] = dict(n=4)
# number of integer arguments after the format string, by format address (monkey debug text, body error, sound errors)
PRINTF_ARGS = {0x800A5D20: 1, 0x800A627C: 1, 0x800A622C: 3, 0x800A6240: 3, 0x800A4964: 3, 0x800A4988: 2, 0x800A499C: 1,
               0x800A4930: 1, 0x800A4D04: 1}
# original functions behind the sound chip boundary (AeSoundHw)
SPU_WAIT_IDLE, SPU_WAIT_LOOP, PRINTF, MEMSET = 0x800A1398, 0x8009D79C, 0x800A3720, 0x800A3740
# original entry points of everything ae_game.c wires to a port (reported as "reached" when the MIPS side executed them)
PORT_ENTRIES = {
    "GimmickGetInfo": 0x8005B7BC, "PlaySfxAt": 0x80087A20, "ProjectileSpawn": 0x80059304, "SpawnParticle": 0x800698B8,
    "TargetListAdd": 0x8007CD88, "TargetListRemove": 0x8007CDC4, "MatrixFromDirXZ": 0x80014804, "ModelPoseBones": 0x80015780,
    "EffectBigBurst": 0x8006B3F0, "SparkAddRandom": 0x8006E0DC, "GetGroundHeight": 0x80043400,
    "ObjSlotGetAnimIndex": 0x80052C4C, "ObjSlotSetField_CF": 0x80052CA0, "ObjSlotGetTarget": 0x80052D04,
    "SoundLoopStartEx": 0x800882C4, "SoundLoopStopAndFree": 0x800880D0, "SpawnPuff": 0x8006B3CC, "MatrixLookDir": 0x8001484C,
    "SoundLoopStart": 0x800878E0,
    "ScaleMatrix": 0x80030E60, "ModelBuildBoneMatrices": 0x80015504, "GimmickEvent": 0x8005B880, "ActorAnimDecode": 0x80014E34,
    "ClearWords": 0x80032708, "StageExitIsActive": 0x8005DCE4, "GetSolidHeight": 0x8004D8C0, "GimmickActivate": 0x8008FC30,
    "CameraShake": 0x80048A00, "CollisionBuildProbePoints": 0x8007304C, "SfxVoiceStartEx": 0x800A31B8, "SfxVoiceStop": 0x800A329C,
    "CollisionBodyRegister": 0x80071E44, "CollisionBodyUnregister": 0x80072018,
}
FRAME_FUNCS = (0x80042E18, 0x8007247C)       # MonkeyUpdateAll, CollisionUpdate


def norm(a):
    return "STK" if STACK_LO <= (a & M32) < STACK_HI else a & M32


# ------------------------------------------------------------------------------------------------------------------ build
SOURCES = ["src/game/ae_game.c", "src/game/ae_game_stage.c", "src/game/ae_game_hooks.c", "src/game/ae_calls.c", "src/game/ae_calls_special.c", "src/game/ae_calls_legacy.c", "src/core/ae_ram.c", "src/core/ae_sdk.c", "src/core/ae_sdk_matrix.c", "src/core/ae_pose.c",
           "src/bodies/ae_bodies.c", "src/bodies/ae_probe.c", "src/bodies/ae_bodymove.c", "src/world/ae_world.c", "src/world/ae_solid.c",
           "src/world/ae_gimmick.c", "src/effects/ae_spawn.c", "src/sound/ae_sound.c", "src/player/ae_anim_events.c",
           "src/world/ae_area02.c", "src/world/ae_area02_paths.c",
           "src/player/native_player.c", "src/effects/ae_effects_mesh.c", "src/math/native_sdk_matrix.c", "src/math/native_sdk_apply.c",
           "tools/mips/game_shim.c", "tools/mips/game_integration_shim.c", "src/game/ae_game_body.c"]


def build_dll(overrides=None, tag="game", trapv=False):
    """Builds the game DLL: ae_game.c, every module it links and the shim.  overrides: repository path -> replacement file."""
    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob(f"{tag}_*.dll"):
        try:
            if time.time() - old.stat().st_mtime > 600:
                old.unlink()
        except OSError:
            pass
    overrides = overrides or {}
    import csv
    with (ROOT / "tools/verified.csv").open(newline="") as stream:
        modules = {row["source"] for row in csv.DictReader(stream) if row["status"] == "diffed"}
    rels = sorted(set(SOURCES) | modules)
    srcs = [Path(overrides.get(rel, ROOT / rel)) for rel in rels]
    dll = OUT / f"{tag}_{os.getpid()}_{time.time_ns()}.dll"
    # object files are cached by content, so a mutation of ae_game.c only recompiles that file
    cache = OUT / "game_obj"
    cache.mkdir(parents=True, exist_ok=True)
    flags = ["-std=c11", "-O1" if trapv else "-O2", "-Wall", "-Wextra", "-Werror"]
    if trapv:
        flags.append("-ftrapv")
    if overrides:
        # Removing a wire or an argument can leave unused declarations in a valid mutant.
        flags += ["-Wno-unused-function", "-Wno-unused-parameter"]
    gcc = shutil.which("gcc")
    if not gcc:
        raise SystemExit("gcc is required to build the game checker DLL")
    header_digest = hashlib.sha1()
    dependencies=list((ROOT / "src").rglob("*.h"))
    dependencies+=list((ROOT / "tools/mips").glob("game_integration_*.inc"))
    for header in sorted(dependencies):
        header_digest.update(str(header.relative_to(ROOT)).encode())
        header_digest.update(header.read_bytes())
    objs = []
    for rel, src in zip(rels, srcs):
        # a replacement file lives elsewhere: its relative includes are resolved against the original directory
        iq = ["-iquote", str(ROOT / Path(rel).parent)]
        key = hashlib.sha1(src.read_bytes() + header_digest.digest() + " ".join(flags).encode() + rel.encode()).hexdigest()[:16]
        obj = cache / f"{Path(rel).stem}_{key}.o"
        objs.append(obj)
        if not obj.exists():
            tmp = obj.with_suffix(f".{os.getpid()}.tmp")
            result = subprocess.run([gcc, *flags, *iq, "-c", str(src), "-o", str(tmp)],
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if result.returncode:
                raise SystemExit("compile error:\n" + result.stdout.decode(errors="replace"))
            os.replace(tmp, obj)
    subprocess.run([gcc, "-shared", "-static-libgcc", *map(str, objs), "-o", str(dll), "-lm"], check=True)
    return dll


# --------------------------------------------------------------------------------------------------------- C side (shim)
MISSING_FN = C.CFUNCTYPE(C.c_int32, C.c_char_p, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
RENDER_FN = C.CFUNCTYPE(C.c_int32, C.c_char_p, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
DEBUG_FN = C.CFUNCTYPE(None, C.c_char_p, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)
VBLANK_FN = C.CFUNCTYPE(C.c_int32)
IDLE_FN = C.CFUNCTYPE(C.c_int32)
WAIT_FN = C.CFUNCTYPE(None, C.c_uint32)
ADSR_FN = C.CFUNCTYPE(None, C.c_uint32, C.c_uint32, C.c_uint32)
OFF_FN = C.CFUNCTYPE(None, C.c_uint32, C.c_uint32)
VOLUME_FN = C.CFUNCTYPE(None, C.c_uint32, C.c_uint32, C.c_uint32)
PRINTF_FN = C.CFUNCTYPE(None, C.c_uint32, C.c_uint32, C.c_int32, C.c_int32, C.c_int32)


TRACE_FN = C.CFUNCTYPE(None, C.c_int, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32)


class ShimCallbacks(C.Structure):
    _fields_ = [("missing", MISSING_FN), ("render", RENDER_FN), ("debug", DEBUG_FN), ("vblank_count", VBLANK_FN),
                ("wait_idle", IDLE_FN), ("wait_loop", WAIT_FN), ("set_voice_adsr", ADSR_FN), ("voice_key_off", OFF_FN),
                ("set_voice_volume", VOLUME_FN),
                ("printf_error", PRINTF_FN)]


class CMem:
    """Memory view of the C side's AeRam with the interface of the MIPS side's cpu (main RAM and scratchpad)."""

    def __init__(self, main, scr):
        self.main, self.scr = memoryview(main).cast("B"), memoryview(scr).cast("B")

    def _loc(self, a, n):
        a &= M32
        if 0x1F800000 <= a < 0x1F800400:
            return self.scr, a - 0x1F800000
        return self.main, a & 0x1FFFFF

    def _rd(self, a, n):
        blob, o = self._loc(a, n)
        return int.from_bytes(blob[o:o + n], "little")

    def _wr(self, a, v, n):
        blob, o = self._loc(a, n)
        blob[o:o + n] = (v & ((1 << (8 * n)) - 1)).to_bytes(n, "little")

    def rd8(self, a): return self._rd(a, 1)
    def rd16(self, a): return self._rd(a, 2)
    def rd32(self, a): return self._rd(a, 4)
    def rds16(self, a): return s16(self._rd(a, 2))
    def wr8(self, a, v): self._wr(a, v, 1)
    def wr16(self, a, v): self._wr(a, v, 2)
    def wr32(self, a, v): self._wr(a, v, 4)


class MipsMem(mh.MipsMem):
    pass


class GameSide:
    """The DLL, the shim (an AeGame over its own AeRam) and the callbacks forwarding to the current GameModel."""

    def __init__(self, dll):
        self.lib = lib = C.CDLL(str(dll))
        P, u32 = C.c_void_p, C.c_uint32
        lib.ae_ptr.restype = P
        lib.ae_ptr.argtypes = [P, u32, u32]
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        lib.ae_ram_clear_error.argtypes = [P]
        lib.shim_create.restype = P
        lib.shim_create.argtypes = [P]
        lib.shim_ram.restype = P
        lib.shim_ram.argtypes = [P]
        lib.shim_frame.argtypes = [P]
        i32 = C.c_int32
        for fam in ("monkey", "world", "gimmick", "spawn"):
            getattr(lib, f"shim_{fam}_hook_count").restype = i32
            f = getattr(lib, f"shim_{fam}_hook")
            f.restype = i32
            f.argtypes = [P, i32, u32, u32, u32, u32]
        lib.shim_spawn_position_padding.argtypes = [P, u32]
        lib.shim_spawn_position_padding.restype = i32
        lib.shim_tables_linked.argtypes = [P]
        for name, args, res in [
                ("printf_error", [u32, i32], None), ("sound_loop_start", [i32, i32], i32), ("sound_loop_stop", [i32, i32], None),
                ("build_probe_points", [i32, u32], None),
                ("collision_ray_test", [u32, u32, P], i32), ("gimmick_type_to_sfx", [i32], i32), ("play_sfx_at", [i32, P], None),
                ("pickup_collect", [i32], None), ("camera_nudge", [i32, i32, i32], None), ("stub_return_zero", [], i32)]:
            f = getattr(lib, f"shim_body_{name}")
            f.argtypes = [P] + args
            f.restype = res
        for name, args in [("play_sfx", [i32]), ("play_player_voice", [i32]), ("event_callback", [u32])]:
            f = getattr(lib, f"shim_anim_{name}")
            f.argtypes = [P] + args
            f.restype = None
        self.model = None
        self.cb_error = None

        def guard(fn, default=0):
            def inner(*a):
                try:
                    return fn(*a)
                except BaseException as e:  # noqa: BLE001  (a callback must not raise into C)
                    if self.cb_error is None:
                        self.cb_error = repr(e)
                    return default
            return inner

        m = lambda: self.model  # noqa: E731
        cbs = ShimCallbacks(
            MISSING_FN(guard(lambda name, addr, a0, a1, a2, a3: s32(m().missing(name.decode(), addr, a0, a1, a2, a3)))),
            RENDER_FN(guard(lambda name, a0, a1, a2, a3: s32(m().call("render", name.decode(), a0, a1, a2, a3)))),
            DEBUG_FN(guard(lambda name, a0, a1, a2, a3: m().call("debug", name.decode(), a0, a1, a2, a3))),
            VBLANK_FN(guard(lambda: s32(m().vblank()))),
            IDLE_FN(guard(lambda: s32(m().wait_idle()))),
            WAIT_FN(guard(lambda units: m().wait_loop(units))),
            ADSR_FN(guard(lambda ch, a1, a2: m().set_voice_adsr(ch, a1, a2))),
            OFF_FN(guard(lambda half, mask: m().voice_key_off(half, mask))),
            VOLUME_FN(guard(lambda ch, side, volume: m().set_voice_volume(ch, side, volume))),
            PRINTF_FN(guard(lambda fmt, argc, a, b, c: m().printf(fmt, a, b, c))))
        self._keep = cbs
        self.shim = lib.shim_create(C.byref(cbs))
        assert self.shim
        self.ram = lib.shim_ram(self.shim)
        self.main = (C.c_uint8 * 0x200000).from_address(lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scr = (C.c_uint8 * 0x400).from_address(lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.mem = CMem(self.main, self.scr)

    def set_spawn_padding(self, values):
        array=(C.c_uint16*len(values))(*values)
        self.lib.shim_set_spawn_padding.argtypes=[C.c_void_p,C.POINTER(C.c_uint16),C.c_uint32]
        self.lib.shim_set_spawn_padding(self.shim,array,len(values))

    def set_trace(self, fn):
        """calls fn(monkey hook index, a0..a3) before every monkey hook the game calls (debugging, coverage of the C side)"""
        self._trace = TRACE_FN(fn)
        self.lib.shim_trace_monkey_hooks.argtypes = [C.c_void_p, TRACE_FN]
        self.lib.shim_trace_monkey_hooks(self.shim, self._trace)

    def failed(self):
        return bool(self.lib.ae_ram_failed(self.ram))

    def error(self):
        return self.lib.ae_ram_error(self.ram).decode()

    def frame(self):
        return self.lib.shim_frame(self.shim)


# ----------------------------------------------------------------------------------------------------------- host model
class GameModel(mh.Model):
    """Answers every host call (identically on both sides) and logs it: (kind, name, [address,] arguments...)."""

    def __init__(self, seed, mem, rich):
        super().__init__(seed, mem, 0)
        self.spawn_padding = []
        self.rich = rich            # False: the missing functions return 0 (what ae_game.c does without a host callback)

    # -- Printf (monkey debug text, body error, sound error): one normalised entry whoever calls it
    def printf(self, fmt, a, b, c):
        fmt &= M32
        self.log.append(("printf", fmt, *[s32(x) for x in (a, b, c)[:PRINTF_ARGS.get(fmt, 0)]]))
        return 0

    def vblank(self):
        self.log.append(("vblank",))
        return s32(self.rng("vsync").getrandbits(32))

    # -- sound chip
    def wait_idle(self):
        self.log.append(("wait_idle",))
        return 0

    def wait_loop(self, units):
        self.log.append(("wait_loop", units & M32))

    def set_voice_adsr(self, ch, a1, a2):
        self.log.append(("adsr", ch & M32, a1 & 0xFFFF, a2 & 0xFFFF))

    def voice_key_off(self, half, mask):
        self.log.append(("key_off", half & M32, mask & 0xFFFF))

    def set_voice_volume(self, channel, side, volume):
        self.log.append(("volume", channel & M32, side & M32, volume & 0xFFFF))

    # -- render / debug callbacks
    def call(self, kind, name, a0, a1, a2, a3):
        if name == "Printf":
            return self.printf(a0, a1, a2, a3)
        spec = CALL_SPEC[name]
        args = [a0 & M32, a1 & M32, a2 & M32, a3 & M32][:spec["n"]]
        entry = [kind, name] + [norm(a) for a in args]
        for idx, nbytes in spec.get("peek", {}).items():
            a = args[idx]
            if STACK_LO <= a < STACK_HI or 0x80000000 <= a < 0x80200000:
                entry.append(tuple(s16(self.mem.rd16(a + i)) for i in range(0, nbytes, 2)))
        self.log.append(tuple(entry))
        if kind == "debug":
            return 0
        eff = spec.get("effect")
        if eff:
            return eff(self, a0, a1, a2, a3)
        return s32(self.rng("ret-" + name).getrandbits(32))

    # -- unported game logic (ae_game.c MISSING table)
    def missing(self, name, address, a0, a1, a2, a3):
        addr, n = MISSING[name]
        args = [a0 & M32, a1 & M32, a2 & M32, a3 & M32][:n]
        entry = ["missing", name, address & M32] + [norm(a) for a in args]
        if name == "CollisionRayTest":    # the real one reads the five parameter words and two globals
            entry.append(tuple(self.mem.rd32(args[1] + 4 * i) for i in range(5)))
            entry += [self.mem.rds16(0x800F45E6) & 0xFFFF, self.mem.rds16(0x800B20CC) & 0xFFFF]
        self.log.append(tuple(entry))
        if not self.rich:
            return 0
        g = self.rng("m-" + name)
        if name == "CollisionRayTest":
            return g.choice([-1, -1, 0, 1, 5, 77, 0x18000, 0x7FFF, 300])
        if name == "GimmickTypeToSfx":
            return g.choice([-1, 0x80, 0x81, 0x82, 0x29, 5, 0x40, 0x7F, 0x83, 0])
        if name == "SoundLoopStart":
            return s32(g.getrandbits(32))
        return g.choice([0, 0, 1, -1, s32(g.getrandbits(32))])


# ----------------------------------------------------------------------------------------------------------- MIPS side
def mips_intercepts(model, spu_state):
    """Entry hooks of the original functions that the C side sends to the host (and the BIOS memset)."""
    hooks = {}
    addr_of = dict(mh.parse_hooks())

    def host(kind, name):
        def fn(cpu, r):
            return model.call(kind, name, r[4], r[5], r[6], r[7]) & M32
        return fn

    def miss(name):
        def fn(cpu, r):
            if name == "CollisionRayTest":      # the C wrapper hands the port zero results
                cpu.write16(r[6], 0)
                cpu.write16(r[6] + 2, 0)
            return model.missing(name, MISSING[name][0], r[4], r[5], r[6], r[7]) & M32
        return fn

    unknown = [n for n in addr_of if n not in MONKEY_HOST and n not in PORT_HOOKS]
    assert not unknown, f"monkey hooks neither host nor port in check_game.py: {unknown}"
    for name, kind in MONKEY_HOST.items():
        addr = addr_of[name]
        if kind == "printf":
            hooks[addr] = lambda cpu, r: model.printf(r[4], r[5], r[6], r[7])
        elif kind == "missing":
            hooks[addr] = miss(name)
        elif kind == "vblank":
            hooks[addr] = lambda cpu, r: model.vblank() & M32
        else:
            hooks[addr] = host(kind, name)
    # world / gimmick hooks
    hooks[0x80015CC8] = host("render", "MeshInitFromAreaTable")
    hooks[0x80090E54] = host("render", "PropSetMesh")
    for name, (addr, _n) in MISSING.items():
        if addr in hooks or name == "PlayerEventCallback":
            continue
        if name != "PlaySfx": hooks[addr] = miss(name)
    # sound chip
    hooks[SPU_WAIT_IDLE] = lambda cpu, r: model.wait_idle() & M32
    hooks[SPU_WAIT_LOOP] = lambda cpu, r: model.wait_loop(r[4])
    hooks[MEMSET] = mh._memset_hook
    return hooks


def mips_patches(model):
    """Observe the existing caller-owned padding boundary; execute all gameplay."""
    reg.declare('shared_random','spawn position padding',checker='check_spawn')
    def capture(cpu,regs):
        model.spawn_padding.append(cpu.read16(regs[29]+0x1E))
        regs[20]=0x800B0000  # original lui s4 at the observation point
        return 0x8006976C
    return {0x80069768:capture}


class SpuWrites:
    """Replaces cpu.write: SPU register stores become the AeSoundHw calls (ADSR1+ADSR2 pair, key-off halfwords)."""

    def __init__(self, cpu, model):
        self.cpu, self.model, self.write = cpu, model, cpu.write
        self.adsr = {}

    def __call__(self, address, size, value):
        if 0x1F801C00 <= address < 0x1F801E00:
            offset = address - 0x1F801C00
            assert size == 2, (address, size)
            value &= 0xFFFF
            if offset in (0x18C, 0x18E):
                self.model.voice_key_off((offset - 0x18C) // 2, value)
            elif offset % 16 in (0, 2):
                self.model.set_voice_volume(offset // 16, (offset % 16) // 2, value)
            elif offset % 16 == 8:
                self.adsr[offset // 16] = value
            elif offset % 16 == 10:
                self.model.set_voice_adsr(offset // 16, self.adsr.pop(offset // 16), value)
            else:
                raise AssertionError(f"unexpected SPU write {address:08X}")
        else:
            self.write(address, size, value)

    def __enter__(self):
        self.cpu.write = self
        return self

    def __exit__(self, *exc):
        self.cpu.write = self.write


# monkey hooks wired to a port (original instructions on the MIPS side)
PORT_HOOKS = {"GimmickGetInfo", "PlaySfxAt", "ProjectileSpawn", "SpawnParticle", "TargetListAdd", "TargetListRemove",
              "MatrixFromDirXZ", "ModelPoseBones", "EffectBigBurst", "SparkAddRandom", "GetGroundHeight",
              "ObjSlotGetAnimIndex", "ObjSlotSetField_CF", "ObjSlotGetTarget", "SoundLoopStartEx", "SoundLoopStopAndFree",
              "SpawnPuff", "MatrixLookDir", "ScaleMatrix", "ModelBuildBoneMatrices", "GimmickEvent", "ActorAnimDecode",
              "ClearWords", "StageExitIsActive"}


# ------------------------------------------------------------------------------------------------------------ harness
class Harness:
    def __init__(self, dll=None, with_coverage=True, tag="game", overrides=None):
        self.m = mr.MipsRam.from_extract()
        self.m.attach_gte_oracle()
        self.coverage = with_coverage
        if with_coverage:
            self.m.coverage = set()
        self.base = self.m.snapshot()
        self.attach(dll or build_dll(overrides, tag))
        self.stack_lo = STACK_LO - 0x80000000
        reg.declare("stack_window", f"{STACK_LO:08X}..{STACK_HI:08X}")
        reg.declare("unreachable", "absent-host missing diagnostic fallback")
        self.reset_counters()
        self.stop_after = None
        self.only = set()
        self.hook_filter = None
        self.survey = None
        self._mask_ok = set()

    def attach(self, dll):
        """the C side: a fresh DLL (the MIPS side and its GTE oracle stay)"""
        self.c = GameSide(dll)
        self.lib = self.c.lib

    def reset_counters(self):
        self.counts = collections.Counter()
        self.frames = collections.Counter()
        self.reached = collections.Counter()          # (layer, kind, name) -> host calls compared
        self.failures = 0
        self.traps_agreed = 0
        self.hook_calls = 0
        self.skipped = 0
        self.trap_kinds = collections.Counter()
        self.trap_example = {}
        self.completed_hooks = collections.Counter()
        self.survey = None if not hasattr(self, "survey") else self.survey

    def fail(self, family, label, *lines):
        self.failures += 1
        print(f"FAIL {family} [{label}]")
        for line in lines:
            print("   ", line)
        if self.stop_after and self.failures >= self.stop_after:
            raise SystemExit(1)

    def sync_to_c(self):
        C.memmove(self.c.main, bytes(self.m.ram), 0x200000)
        C.memmove(self.c.scr, bytes(self.m.scratch), 0x400)
        self.lib.ae_ram_clear_error(self.c.ram)

    def original(self, addr, args=(), seed="setup"):
        """runs original code of the scenario setup with the full host model (nothing compared)"""
        model = GameModel(seed, MipsMem(self.m), True)
        try:
            with SpuWrites(self.m, model):
                self.m.call(addr, args, hooks=mips_intercepts(model, None), patches=mips_patches(model))
        except mr.MipsError:
            pass                    # the random world made the original fault: the case runs on what it left

    def set_mask(self, entries):
        key = tuple(entries)
        if key not in self._mask_ok:
            reg.check_ram_mask(key)
            self._mask_ok.add(key)
        return list(entries)

    def compare(self, family, label, frame, mm, cm, mask, extra=(), args=None):
        problems = list(extra)
        if self.c.cb_error:
            problems.append("callback error: " + self.c.cb_error)
        if mm.log != cm.log:
            n = next((i for i, (a, b) in enumerate(zip(mm.log, cm.log)) if a != b), min(len(mm.log), len(cm.log)))
            problems.append(f"host calls differ at call {n} of {len(mm.log)} MIPS / {len(cm.log)} C\n"
                            f"      MIPS {mm.log[max(0, n - 1):n + 2]}\n      C    {cm.log[max(0, n - 1):n + 2]}")
        lo = self.stack_lo
        a, b = bytearray(bytes(self.m.ram)[:lo]), bytearray(bytes(self.c.main)[:lo])
        reg.declare('ram_mask', 'camera slot target padding (slot +0x18:2)', checker='check_game_area')
        slots = self.m.read32(0x800B4D40)
        for index in range(12):
            slot = slots + index * 0xB0
            if slots and self.m.read8(slot):
                offset = slot + 0x18 - 0x80000000
                b[offset:offset + 2] = a[offset:offset + 2]
        sa, sb = bytearray(self.m.scratch), bytearray(self.c.scr)
        reg.check_ram_mask(PARTICLE_PAD)
        reg.declare("ram_mask", "1F8003FC:2")
        mask = list(mask) + PARTICLE_PAD
        if self.m.read32(self.m.read32(0x800F44E4)) & 2:
            mask += [(0x1F8003FC-0x80000000,2)]
        for off, ln in mask:
            if off < 0:                          # scratchpad (0x1F800000 + n is below the RAM base)
                off += 0x80000000 - 0x1F800000
                sa[off:off + ln] = bytes(ln)
                sb[off:off + ln] = bytes(ln)
            else:
                a[off:off + ln] = bytes(ln)
                b[off:off + ln] = bytes(ln)
        if a != b:
            diffs = [i for i in range(lo) if a[i] != b[i]]
            problems.append(f"RAM differs ({len(diffs)} bytes) at " +
                            ", ".join(f"{0x80000000 + i:08X}(mips {a[i]:02X} c {b[i]:02X})" for i in diffs[:10]))
        if sa != sb:
            diffs = [i for i in range(0x400) if sa[i] != sb[i]]
            problems.append("scratchpad differs at " + ", ".join(
                f"{0x1F800000 + i:08X}(mips {sa[i]:02X} c {sb[i]:02X})" for i in diffs[:8]))
        if self.survey is not None:           # discovery mode: record the differing bytes, resync, go on
            if a != b:
                for i in range(lo):
                    if a[i] != b[i]:
                        self.survey[(family, i)] += 1
            if sa != sb:
                for i in range(0x400):
                    if sa[i] != sb[i]:
                        self.survey[(family, 0x200000 + i)] += 1
            if mm.log != cm.log:
                self.survey[(family, "LOG")] += 1
                if self.survey["printed"] < 6:
                    self.survey["printed"] += 1
                    print(f"survey LOG {family} {label} frame {frame}:")
                    print("  " + problems[0])
            self.sync_to_c()
            return True
        if problems:
            self.fail(family, f"{label} frame {frame}" if args is None else f"{label} args {[hex(x) for x in args]}", *problems)
        return not problems

    def run(self, family, label, seed, setup, frames, rich=True):
        """setup(h) builds the scenario in the MIPS RAM image (h.original() may run original code) and returns the RAM mask;
        then `frames` game frames run on both sides, compared after every frame."""
        self.m.restore(self.base)
        self.m.gte_reset()
        mask = self.set_mask(setup(self) or [])
        self.sync_to_c()
        mm, cm = GameModel(seed, MipsMem(self.m), rich), GameModel(seed, self.c.mem, rich)
        self.c.model = cm
        self.c.cb_error = None
        self.counts[family] += 1
        for frame in range(frames):
            mm.log.clear()
            mm.spawn_padding.clear()
            cm.log.clear()
            trap = None
            try:
                with SpuWrites(self.m, mm):
                    hooks = mips_intercepts(mm, None)
                    self.m.call(FRAME_FUNCS[0], (), hooks=hooks, patches=mips_patches(mm))
                    self.m.call(FRAME_FUNCS[1], (), hooks=hooks, patches=mips_patches(mm))
            except (mr.MipsError, AssertionError) as e:
                trap = str(e)
                tb = e.__traceback__
                while tb is not None:
                    if tb.tb_frame.f_code.co_name == "call" and "pc" in tb.tb_frame.f_locals:
                        trap += f" [pc {tb.tb_frame.f_locals['pc']:08X}]"
                    tb = tb.tb_next
                if trap.startswith("instruction budget"):      # a runaway loop of the random world: nothing to compare
                    self.skipped += 1
                    return
            self.c.set_spawn_padding(mm.spawn_padding)
            self.c.frame()
            if trap:
                if self.c.failed() and not trap.startswith("unexpected SPU"):
                    self.traps_agreed += 1
                    self.trap_kinds[family + ": " + re.sub(r"(?<!pc )[0-9A-Fa-f]{8}", "ADDR", trap)] += 1
                    self.trap_example.setdefault(re.search(r"\[pc \w+\]", trap).group(0) if "[pc" in trap else trap, f"{family} {label} frame {frame}: {trap}")
                else:
                    self.fail(family, f"{label} frame {frame}", "MIPS harness error: " + trap,
                              f"port flagged={self.c.failed()} {self.c.error() if self.c.failed() else ''}")
                return
            if self.c.failed():
                self.fail(family, f"{label} frame {frame}", "the original completed but the game flagged: " + self.c.error(),
                          f"host calls so far MIPS {mm.log[-3:]}")
                return
            self.frames[family] += 1
            if not self.compare(family, label, frame, mm, cm, mask):
                return
            self.hook_calls += len(mm.log)
            self.note_reached("frame", mm.log)
            for off, ln in mask:                 # masked words are leftover stack: later frames see the same value on both sides
                if off < 0:
                    off += 0x80000000 - 0x1F800000
                    C.memmove(C.addressof(self.c.scr) + off, bytes(self.m.scratch[off:off + ln]), ln)
                else:
                    C.memmove(C.addressof(self.c.main) + off, bytes(self.m.ram[off:off + ln]), ln)


    def mips_call(self, mm, addr, args):
        """one original function (or the intercept at its address) on the MIPS side; returns $v0"""
        with SpuWrites(self.m, mm):
            regs = self.m.call(addr, tuple(a & M32 for a in args), hooks=mips_intercepts(mm, None), patches=mips_patches(mm))
        return regs[2]

    def run_direct(self, family, label, seed, world, rng, mips, cdo, args, void=False, rich=True):
        """one hook called directly: `mips(model, ctx, args)` runs the original, `cdo(ctx, args)` calls the wired hook"""
        self.m.restore(self.base)
        self.m.gte_reset()
        ctx = world(self, rng)
        a = args(ctx)
        self.sync_to_c()
        mm, cm = GameModel(seed, MipsMem(self.m), rich), GameModel(seed, self.c.mem, rich)
        self.c.model = cm
        self.c.cb_error = None
        self.counts[family] += 1
        try:
            mret = mips(mm, ctx, a)
        except (mr.MipsError, AssertionError) as e:
            trap = str(e)
            if trap.startswith("instruction budget"):
                self.skipped += 1
                return
            try:
                cdo(ctx, a)
            except AssertionError:
                pass
            if self.c.failed():
                self.traps_agreed += 1
            else:
                self.fail(family, label, "MIPS harness error: " + trap, "the wired hook did not flag an error")
            return
        self.c.set_spawn_padding(mm.spawn_padding)
        try:
            cret = cdo(ctx, a)
        except AssertionError as e:
            self.fail(family, label, str(e))
            return
        if self.c.failed():
            self.fail(family, label, "the original completed but the hook flagged: " + self.c.error(),
                      f"args {[hex(x) for x in a]} host calls MIPS {mm.log[-3:]}")
            return
        self.frames[family] += 1
        problems = []
        if not void and (mret & M32) != ((cret or 0) & M32):
            problems.append(f"return value MIPS {mret & M32:08X} C {(cret or 0) & M32:08X}")
        ok = self.compare(family, label, 0, mm, cm, [], extra=problems, args=a)
        if ok:
            self.hook_calls += len(mm.log)
            self.completed_hooks[(family, label.split(" #")[0])] += 1
            self.note_reached("hook", mm.log)

    def note_reached(self, layer, log):
        for entry in log:
            self.reached[(layer, entry[0], entry[1] if entry[0] in ("render", "debug", "missing") else "")] += 1

    def entries_reached(self):
        cov = self.m.coverage or set()
        return {name: addr in cov for name, addr in PORT_ENTRIES.items()}


# ------------------------------------------------------------------------------------------------------------ scenarios
AREA_MEMBERS = {0x1: 0x05B, 0x4: 0x064, 0x5: 0x066, 0x2: 0x05E}
BASE_MASK = [(MONKEYS + MONKEY_SIZE * k + 0xC8 - 0x80000000, 4) for k in range(8)]
INIT_MASK = BASE_MASK + [(MONKEYS + MONKEY_SIZE * k + 0x214 - 0x80000000, 8) for k in range(8)]
PARTICLE_PAD = [(0x800B9900 - 0x80000000 + 0x34 * k + 0x1B, 1) for k in range(100)]      # see check_exclusions.csv
STALE_SCRATCH = [(0x1F8003FE - 0x80000000, 2)]
FRAME_MASK = INIT_MASK + PARTICLE_PAD + STALE_SCRATCH


def load_area(m, area):
    blob = (ROOT / f"extract/members/{AREA_MEMBERS[area]:03X}.bin").read_bytes()
    m.load_area(blob)
    ms = struct.unpack_from("<I", blob, 0x4E4)[0]
    W(m).w32(0x800F4590, 0x80154000 + ms)
    return blob


def install_world(m, blob, gimmicks=True):
    """AreaLoad's pointer fixups for the height field, the collision data and the gimmicks of the area loaded at 0x80154000
    (as tools/mips/check_world.py / check_solid.py reconstruct them)."""
    u32 = lambda off: struct.unpack_from("<I", blob, off)[0]  # noqa: E731
    base = mr.AREA_BASE
    ah = u32(0x4D0)
    tgrid = ah + 4 * u32(ah + 4)
    tw, td, nc = struct.unpack_from("<HHI", blob, tgrid)
    cells = tgrid + 8
    heights = cells + tw * td * 12 + nc * 4
    m.write32(0x800F44E4, base + ah)
    m.write16(0x800EBFA0, tw)
    m.write16(0x800EBFA2, td)
    m.write32(0x800EBFA8, base + cells)
    m.write32(0x800EBFB4, base + heights)
    collision = ah + 4 * u32(ah + 16)
    grid = collision + u32(collision)
    shapes = collision + u32(collision + 4)
    colliders = collision + u32(collision + 8)
    width, depth = struct.unpack_from("<hh", blob, grid)
    count = struct.unpack_from("<H", blob, colliders)[0]
    for addr, value in [(0x800F4558, base + shapes + 2), (0x800F4560, base + grid + 4), (0x800F455C, base + colliders + 2),
                        (0x800F45F0, width), (0x800F45F4, depth), (0x800F45F8, width * 256), (0x800F45FC, depth * 256)]:
        m.write32(addr, value & M32)
    m.write16(0x800F45D4, count)
    total, instances = 0, 0
    if gimmicks:
        table = u32(0x4D4)
        groups = u32(table)
        total = sum(struct.unpack_from("<h", blob, table + 4 + 8 * k)[0] for k in range(groups))
        instances = base + table + 4 + 8 * groups
        m.write32(0x800F456C, base + table + 4)
        m.write32(0x800F45D8, groups)
        m.write32(0x800F4570, base + table + 4 + 8 * groups)
        m.write32(0x800B4D64, 0x801B0000)
        for k in range(groups):
            m.write16(base + table + 6 + 8 * k, 0)
    return dict(tw=tw, td=td, width=width, depth=depth, colliders=count, gimmicks=max(0, total), gimmick_base=instances)


def register_player(h, rng, near):
    """the player (and sometimes the RC car) as a registered collision body, near `near` = (x, y, z)"""
    m, w = h.m, W(h.m)
    for obj, typ in ((PLAYER, 1), (RCCAR, 2)):
        if typ == 2 and rng.random() < 0.7:
            continue
        cb.write_body(w, rng, obj + 0x24, near[0], near[1], near[2], rng.choice([200, 600, 1500]),
                      radius=rng.choice([60, 100, 150, 40]), height=rng.choice([150, 200, 300]))
        w.w32(obj + 0x24, near[0] + rng.randint(-800, 800))
        w.w32(obj + 0x28, near[1] + rng.randint(-100, 100))
        w.w32(obj + 0x2C, near[2] + rng.randint(-800, 800))
        w.w32(obj + 0x50, 0)
        h.original(0x80071E44, (typ, obj + 0x24, obj))


def monkey_world(h, rng, area=None, world=False, register=True):
    """a real area after the original MonkeySpawnAll (run with the full game model), then random run time states of the
    monkeys (as check_monkey_level10.py update_world), the collision bodies registered through the original code"""
    m = h.m
    c = s5.common(m, rng)
    w = W(m)
    area = area if area is not None else rng.choice(list(AREA_MEMBERS))
    blob = load_area(m, area)
    if world:
        install_world(m, blob, gimmicks=rng.random() < 0.7)
    w.w16(0x800F4476, area)
    w.w16(0x800F4472, rng.choice([0, 0, 0, 1, 2, 3, 6, 8]))
    w.w32(0x800A8A58, 0)
    w.w32(0x800B4D10, rng.getrandbits(16))
    w.w8(0x800F447C, 0)
    h.original(0x80071D3C)                          # CollisionBodiesReset: every slot free
    h.original(0x8003958C)                          # MonkeySpawnAll
    cnt = s16(m.read16(0x800AEB68))
    for k in range(8):
        a = MONKEYS + MONKEY_SIZE * k
        if k < cnt:
            w.w8(a + 0x7A, rng.choice([4, 4, 3, 2, 1, 0]))
            w.w8(a + 0x78, rng.choice(s5.PATROL_STATES + s5.MOVE_STATES + [64, 5, 65, 37, 6] * 2 + [rng.randint(0, 0x42)]))
            w.w32(a + 0x248, (m.read32(a + 0x248) & ~(1 << 21) & M32) | (rng.choice([0, 1]) << 21))
            w.w16(a + 0x158, rng.choice([0, 1, 5, -1, rng.getrandbits(16)]) & 0xFFFF)
            w.w32(a + 0x228, m.read32(a + 0x228) & ~(0x100 | 0x800 | 0x4) & M32)
            w.w16(a + 0xEA, rng.choice([0, 100, 2700, 2701]))
    w.w32(0x800AEB5C, rng.choice([0, 1, 29, 30, 59, rng.getrandbits(32)]) & M32)
    w.w32(0x800EC350, rng.choice([0, 29, 30, 60, 90, rng.getrandbits(32)]) & M32)
    w.w16(0x800AEC10, rng.choice([m.read16(0x800F44D4), rng.getrandbits(16)]))
    w.w16(0x800AEC12, rng.choice([m.read16(0x800AEB66), rng.getrandbits(16)]))
    w.w32(0x800F4450, rng.choice([0, 0, 2, rng.getrandbits(32)]) & M32)
    w.w8(0x800B4CF4, rng.choice([0, 0, 0, 1]))
    w.w8(0x800EC2D2, rng.choice([0, 7, 3]))
    if register:
        pos = (m.reads32(MONKEYS + 0x24), m.reads32(MONKEYS + 0x28), m.reads32(MONKEYS + 0x2C))
        register_player(h, rng, pos)
    return FRAME_MASK



NODE_BITS = [0x10, 0x400, 0x40, 4, 8, 0x200]
NODE_BIT_CHANCE = {0x400: 0.04, 0x40: 0.1, 0x10: 0.1}      # bone / reference-position / object nodes need valid data behind them


def sane_voice_queue(m, rng):
    """the delayed sound queue (16 x {s32 sfx, s16 timer, pad, s32 pos[3]}) with real sound ids (random ids index outside RAM)"""
    w = W(m)
    for k in range(16):
        e = 0x800E6780 + 20 * k
        w.w32(e, rng.choice([0, 0, rng.randint(1, 197), rng.randint(1, 197)]))
        w.w16(e + 4, rng.choice([0, 1, 2, 5, -1, rng.randint(-5, 30)]))
        for off in (8, 12, 16):
            w.w32(e + off, rng.randint(-3000, 3000))


def ai_monkey(m, rng, c, state=None, act=None, contact=None, gimmicks=0):
    """random state of monkey `c` for MonkeyAiStateUpdate (the per-monkey part of check_monkey_level10.py ai_world)"""
    w = W(m)
    nn = m.read16(0x800B4D18)
    for k in range(8):
        a = MONKEYS + MONKEY_SIZE * k       # no node dispatch entries that read leftover stack
        w.w32(a + 0x228, m.read32(a + 0x228) & ~(0x100 | 0x800 | 0x4) & M32)
    st = state if state is not None else rng.choice(list(range(0x43)) + [rng.randint(0, 255)])
    w.w8(c + 0x78, st)
    s5.move_fields(m, rng, c)
    w.w8(c + 0x7A, act if act is not None else rng.choice([4, 4, 4, 3, 1, 0, 2, 5]))
    w.w16(c + 0x174, rng.choice([0, 1, 2, 0x8000, rng.getrandbits(16)]))
    w.w16(c + 0x90, rng.choice([0, 1, 2, 30, 0xFFFF, rng.getrandbits(16)]))
    bits = 0
    for b in (19, 20, 25, 30):
        if rng.random() < (0.5 if b != 30 else 0.3):
            bits |= 1 << b
    w.w32(c + 0x248, (rng.getrandbits(32) & ~((1 << 19) | (1 << 20) | (1 << 25) | (1 << 30))) | bits)
    f2 = m.read32(c + 0x228) & ~(0x4000 | 0x8000 | 0x80000 | 0x20 | 0x20000) & M32
    for b in (0x4000, 0x8000, 0x80000, 0x20, 0x20000):
        if rng.random() < 0.25:
            f2 |= b
    w.w32(c + 0x228, f2)
    w.w32(c + 0x24C, (m.read32(c + 0x24C) & ~2 & M32) | (rng.choice([0, 2])))
    n0, n1 = m.read16(c + 0xF2) % nn, m.read16(c + 0xF4) % nn
    w.w16(c + 0xF2, n0)
    w.w16(c + 0xF4, n1)
    for node in (n0, n1):
        a = NAV_BASE + 0x18 * node
        f = m.read32(a) & ~sum(NODE_BITS) & M32
        for b in NODE_BITS:
            if rng.random() < NODE_BIT_CHANCE.get(b, 0.2):
                f |= b
        w.w32(a, f)
        link = rng.choice([rng.randint(0, 39), rng.randint(0, 39), rng.randint(0, 1599)])
        if gimmicks and rng.random() < 0.7:                      # the link code names a real gimmick of the area
            link = rng.randint(0, gimmicks - 1)
        w.w16(a + 0x10, link & 0xFFFF)
    ctype = contact if contact is not None else rng.choice([0, 1, 3, 8, 10, 11, 11, 11, 8, rng.randint(0, 15)])
    w.w32(c + 0x50, (ctype << 28) | rng.getrandbits(28))
    w.w16(c + 0x126, rng.choice([0, 8, 11, 3, rng.getrandbits(16)]))
    w.w32(c + 0x230, rng.choice([0, 239, 240, 241, 242, 1000, rng.getrandbits(32)]) & M32)
    w.w16(c + 0x8E, rng.choice([1000, 100, 0, -1, -500, 5, rng.getrandbits(16)]) & 0xFFFF)
    w.w8(c + 0xE2, rng.choice([0, 0, 0, 1, 5, 255]))
    w.w16(c + 0x22C, rng.choice([0, 0, 1, 5]))
    w.w16(c + 0x242, rng.choice([0, 6, 7, 8, 7, 7, rng.getrandbits(16)]))
    w.w16(c + 0x244, rng.choice([0, 1, 2, 5, 100, -1, rng.getrandbits(16)]) & 0xFFFF)
    w.w16(c + 0x246, rng.choice([0, 1, 2, 5, 100, -1, rng.getrandbits(16)]) & 0xFFFF)
    w.w16(c + 0x166, rng.choice([0, 100, 200, 0x1000, -50, rng.getrandbits(16)]) & 0xFFFF)
    w.w16(c + 0x92, rng.choice([0, 1, 2, 5, 20, 40, rng.getrandbits(16)]) & 0xFFFF)
    w.w16(c + 0x124, rng.choice([0, 1, 5, -1, rng.getrandbits(16)]) & 0xFFFF)
    w.w16(c + 0xF0, rng.choice([0, 1, -1, 5, rng.getrandbits(16)]) & 0xFFFF)
    for off in (0x84, 0x86, 0x88):
        w.w16(c + off, rng.choice([0, 1, 1024, -1024, 4096, rng.getrandbits(16)]) & 0xFFFF)
    w.w32(c + 0x18, rng.choice([m.read32(0x800F4504), m.read32(0x800F4508)]))
    if rng.random() < 0.3:
        s5.anim_cycle(m, rng, c, rng.choice(["run5", "past", "rand", "start"]))
    if rng.random() < 0.3:      # the notice test at the end of the update / net catch
        w.w8(c + 0x79, 0)
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        w.w16(info + 0xE, rng.choice([5000, 5000, 0, 100]))
        w.w16(info + 0x1A, 5000)


def ai_frame_world(h, rng, world=False):
    """several monkeys in random AI states, all four active slots thinking, the player target, bodies registered"""
    m = h.m
    c = s5.common(m, rng)
    sane_voice_queue(m, rng)
    w = W(m)
    gimmicks = 0
    if world:
        area = rng.choice(list(AREA_MEMBERS))
        gimmicks = install_world(m, load_area(m, area), gimmicks=rng.random() < 0.85)["gimmicks"]
    h.original(0x80071D3C)
    nn = m.read16(0x800B4D18)
    count = rng.choice([8, 8, 4, 3, 1])
    w.w16(0x800F44D4, count)
    mx = rng.choice([4, 4, 3, 2, 1, 5])
    w.w16(0x800AEB66, mx)
    w.w16(0x800AEB68, rng.choice([8, 8, 4, 3]))
    w.w16(0x800AEC10, count)          # no SelectActive from a changed count (every 30th frame still runs it)
    w.w16(0x800AEC12, mx)
    order = [c] + [MONKEYS + MONKEY_SIZE * k for k in range(8) if MONKEYS + MONKEY_SIZE * k != c]
    rng.shuffle(order)
    active = order[:4] if rng.random() < 0.8 else [order[0], 0, order[1], 0]
    for k, a in enumerate(active):
        w.w32(0x800E54F0 + 4 * k, a)
    for k in range(8):
        a = MONKEYS + MONKEY_SIZE * k
        w.w8(a + 0x7B, rng.randint(0, 7))
        w.w16(a + 0xEA, rng.choice([0, 100, 2699, 2700, 2701, 0xFFFF, rng.getrandbits(16)]))
    for a in active:
        if a:
            ai_monkey(m, rng, a, gimmicks=gimmicks)
    w.w8(0x800EC2D2, rng.choice([7, 0, 0, 3, 0xFF]))
    for off in (0x24, 0x28, 0x2C):
        w.w32(0x800F5A20 + off, rng.choice([rng.randint(-3000, 3000), rng.getrandbits(32)]) & M32)
        w.w32(0x800EC1E0 + off, rng.choice([rng.randint(-3000, 3000), rng.getrandbits(32)]) & M32)
    w.w32(sc.FRAME, rng.choice([rng.getrandbits(32), rng.randint(0, 1000) * 50, rng.randint(0, 1000) * 50 + 9,
                                rng.randint(0, 1000) * 3, 0, 29]) & M32)
    w.w32(0x800EC350, m.read32(sc.FRAME))
    w.w32(0x800AEB5C, rng.choice([0, 1, 29, 30, 59]))
    w.w16(0x800F4472, rng.choice([0, 0, 0, 1, 3, 6, 8]))
    # area 0x24 reaches the monkey sound-loop stop sites (fixed in 6230373)
    w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x46, 0x35, 0x35, 1, 4, 5, 2, 0x24, 0x24]))
    w.w32(0x800F4450, rng.choice([0, 0, 0, 2]))
    w.w8(0x800B4CF4, 0)
    w.w8(0x800F447C, rng.choice([0, 1, 2, 3, 5, 6, 7, 8, 0xFF]))
    for a in active:
        if a and rng.random() < 0.6:                        # the monkey's collision body is registered
            h.original(0x80071E44, (3, a + 0x24, a))
            w.w32(a + 0x24C, m.read32(a + 0x24C) | 2)
    register_player(h, rng, (m.reads32(order[0] + 0x24), m.reads32(order[0] + 0x28), m.reads32(order[0] + 0x2C)))
    return FRAME_MASK


# ------------------------------------------------------------------------------------------- per-hook layer (direct calls)
# Every hook of every table that ae_game.c fills is called directly through the wired function pointer (shim_*_hook) and compared
# with the original function at the hook's address run on the MIPS side (the intercept itself for host-bound hooks).  This is
# what reaches the rarely taken call sites of a frame, and it checks argument order, sign extension and the choice of the
# function for each adapter.
BUF = 0x801A0000                      # random s16 data the pointer arguments point into
VOID_HOOKS = {"monkey": {"SpawnParticle", "TargetListRemove", "ModelPoseBones", "EffectBigBurst", "SparkAddRandom",
                         "ObjSlotSetField_CF", "SpawnPuff", "ModelBuildBoneMatrices", "ClearWords"},
              "world": {"CameraShake"}, "gimmick": set(), "spawn": set()}


def parse_table(header, fn_type, first):
    """[(name, original address)] of the `fn_type Name;` members of a header, from `first` on (the address is the 8 digit hex
    number of the comment, or the one in an overlay function's name)"""
    text = (ROOT / header).read_text(encoding="utf-8")
    out = []
    for m in re.finditer(rf"{fn_type}\s+(\w+);\s*(?:/\*\s*([0-9A-Fa-f]{{8}}))?", text):
        addr = m.group(2) or (re.fullmatch(r"FUN_([0-9a-f]{8})", m.group(1)) or [None, None])[1]
        out.append((m.group(1), int(addr, 16) if addr else None))
    names = [n for n, _ in out]
    out = out[names.index(first):]
    missing = [n for n, a in out if a is None]
    if missing:
        raise SystemExit(f"{header}: no original address for hook(s) {missing}")
    return out


def hook_tables():
    return {
        "monkey": mh.parse_hooks(),
        "world": parse_table("src/world/ae_world.h", "AeWorldHookFn", "GetSolidHeight"),
        "gimmick": parse_table("src/world/ae_gimmick.h", "AeGimmickHookFn", "PlaySfx"),
        "spawn": parse_table("src/effects/ae_spawn.h", "AeSpawnHookFn", "PlaySfx"),
    }


_HI_TABLE = bytes([0x00 if b < 90 else 0xFF if b < 180 else 0x01 if b < 200 else 0xFE if b < 220 else 0x02 if b < 225 else
                   0xFD if b < 230 else b for b in range(256)])
_HI_TABLE_SANE = bytes([0x00 if b < 100 else 0xFF if b < 200 else 0x01 if b < 225 else 0xFE for b in range(256)])


def small_halfwords(rng, wild=True):
    """0x4000 bytes of s16 values, mostly within +-700 (positions / vectors the pointer arguments point at), some wild ones"""
    data = bytearray(rng.randbytes(0x4000))
    data[1::2] = rng.randbytes(0x2000).translate(_HI_TABLE if wild else _HI_TABLE_SANE)
    return bytes(data)


def direct_world_a(h, rng):
    """the world of the monkey / world / gimmick / spawn hooks: a real area with its collision data and gimmicks, the monkey
    world, the sound banks, random data behind BUF"""
    m = h.m
    w = W(m)
    c = s5.common(m, rng)
    area = rng.choice(list(AREA_MEMBERS))
    info = install_world(m, load_area(m, area), gimmicks=True)
    w.w16(0x800F4476, rng.choice([area, area, 0x29, 0x46, 0x24, 0x10]))
    w.w16(0x800F4474, rng.choice([0, 1, 2]))
    if rng.random() < 0.8:
        ks.request_setup(m, rng, rng.choice([0, 0, 0, 1, 3, 4, 5, 6]))
    m.write_bytes(BUF, small_halfwords(rng))
    # Distinguish a 16-bit slot index from an accidentally narrowed byte index.
    m.write8(0x800E77B0, 0)
    m.write8(0x800E77B0 + 256, 1)
    m.write16(0x800E6960 + 0x5E, 101)
    m.write16(0x800E6960 + 0xE4 + 0x5E, 202)
    m.write_bytes(0x800B8D30, bytes(16 * 0x30))
    register_player(h, rng, (rng.randint(-2000, 2000), rng.randint(-500, 500), rng.randint(-2000, 2000)))
    return dict(buf=BUF, monkeys=[MONKEYS + MONKEY_SIZE * k for k in range(8)], objs=[0x800E6960 + 0xE4 * k for k in range(16)],
                cur=c, **info)


def arg_gen(group, name, rng, ctx):
    P = lambda: ctx["buf"] + 8 * rng.randrange(0x400)  # noqa: E731
    INT = lambda: rng.choice([0, 1, 2, 3, -1, -2, 5, 100, 255, 256, 0x7FFF, 0x8000, 0xFFFF, -0x8000, 0x10000, 0x10001,  # noqa: E731
                              rng.randint(-5000, 5000), rng.getrandbits(32)]) & M32
    SMALL = lambda: rng.randint(-3, 20) & M32  # noqa: E731
    MK = lambda: rng.choice(ctx["monkeys"])  # noqa: E731
    OBJ = lambda: rng.choice(ctx["objs"] + ctx["monkeys"])  # noqa: E731
    SLOT = lambda: rng.choice([-1, 0, 1, 2, 5, 15, 16, 0xFFFF, 0x10000, 0x10001, 0xFFFFFFFF, rng.getrandbits(32),  # noqa: E731
                               rng.randint(-3, 20)]) & M32
    SFX = lambda: rng.choice([rng.randint(0, 197), rng.randint(0, 197), 9, 0x75, 0xB0, 0xAC, 0xAF, -1, 198, 0x10009,  # noqa: E731
                              INT()]) & M32
    GIM = lambda: rng.choice([-2, -1, 0, 1, 2, ctx["gimmicks"] - 1, ctx["gimmicks"], ctx["gimmicks"] + 1,  # noqa: E731
                              rng.randint(0, max(0, ctx["gimmicks"])), INT()]) & M32
    GRID = lambda: (rng.randint(-ctx["tw"] * 256, ctx["tw"] * 256) & M32,  # noqa: E731
                    rng.choice([0, -100, -2000, 500, 3000, -32768, 32767, rng.randint(-5000, 5000)]) & M32,
                    rng.randint(-ctx["td"] * 256, ctx["td"] * 256) & M32)
    if group == "monkey":
        specs = {
            "Printf": lambda: (rng.choice([*PRINTF_ARGS, 0x800A4BBC, P()]), INT(), INT(), INT()),
            "GimmickGetInfo": lambda: (GIM(), P(), INT(), INT()),
            "PlaySfxAt": lambda: (SFX(), P(), INT(), INT()),
            "ProjectileSpawn": lambda: (P(), MK(), 6, 3),
            "SpawnParticle": lambda: (rng.choice([rng.randint(0, 460), rng.randint(0, 460), 0x10D, 0x113, 0x112, INT()]), P(),
                                      INT(), INT()),
            "TargetListAdd": lambda: (rng.choice([0, P(), MK(), 0x80123456]), INT(), INT(), INT()),
            "TargetListRemove": lambda: (rng.choice([0, P(), MK(), 0x80123456]), INT(), INT(), INT()),
            "MatrixFromDirXZ": lambda: (P(), P(), INT(), INT()),
            "ModelPoseBones": lambda: (OBJ(), rng.choice([3, INT()]), INT(), INT()),
            "EffectBigBurst": lambda: (INT(), INT(), INT(), INT()),
            "SparkAddRandom": lambda: (P(), P(), SMALL(), INT()),
            "GetGroundHeight": lambda: (*GRID(), INT()),
            "ObjSlotGetAnimIndex": lambda: (256, INT(), INT(), INT()),
            "ObjSlotSetField_CF": lambda: (SLOT(), rng.choice([4, 1, 3, INT()]), INT(), INT()),
            "ObjSlotGetTarget": lambda: (SLOT(), P(), INT(), INT()),
            "SoundLoopStartEx": lambda: (rng.choice([0, 1, 0xFFFFFFFF, INT()]),
                                         rng.choice([0x6050, 0x602D, 9, 0xA009, rng.randint(0, 0x7FFF), INT()]),
                                         rng.randint(0, 15), INT()),
            "SoundLoopStopAndFree": lambda: (9, 0x8005, INT(), INT()),
            "SpawnPuff": lambda: (P(), INT(), INT(), INT()),
            "MatrixLookDir": lambda: (P(), P(), INT(), INT()),
            "ScaleMatrix": lambda: (P(), P(), INT(), INT()),
            "ModelBuildBoneMatrices": lambda: (OBJ(), rng.choice([3, INT()]), INT(), INT()),
            "GimmickEvent": lambda: (rng.choice([*range(0, 18), INT()]), GIM(), rng.choice([0, 1, 2, 0xFFFFFFFE, INT()]), INT()),
            "ActorAnimDecode": lambda: (OBJ(), INT(), INT(), INT()),
            "ClearWords": lambda: (P(), rng.choice([0, 1, 5, 16, 64, 0x400, 0xFFFFFFFF, 0xFFFFFFFB, rng.randint(0, 200)]),
                                   INT(), INT()),
            "StageExitIsActive": lambda: (INT(), INT(), INT(), INT()),
            "ModelTransformRigid": lambda: (OBJ(), P(), P(), INT()),
            "ModelDraw": lambda: (P(), P(), OBJ(), INT()),
            "MenuItemInitBound": lambda: (P(), P(), INT(), INT()),
            "MenuItemSetRange": lambda: (P(), INT(), INT(), INT()),
            "MenuItemSetCallback": lambda: (P(), INT(), INT(), INT()),
            "MenuItemSetStep": lambda: (P(), INT(), INT(), INT()),
            "ModelInit": lambda: (MK(), P(), INT(), INT()),
            "SetRotMatrix": lambda: (P(), INT(), INT(), INT()),
            "SetTransMatrix": lambda: (P(), INT(), INT(), INT()),
            "RotTransPers": lambda: (P(), P(), P(), P()),
            "AlertIconDraw": lambda: (P(), rng.randint(0, 3), INT(), INT()),
            "ModelTransformSkinned": lambda: (OBJ(), INT(), INT(), INT()),
            "ModelDrawWithOt": lambda: (P(), P(), OBJ(), INT()),
            "DrawShadow": lambda: (P(), INT(), INT(), INT()),
            "ColorFadeSet": lambda: (INT(), INT(), INT(), INT()),
            "DebugTextPrintfWhite": lambda: (P(), INT(), INT(), INT()),
            "VSync": lambda: (0xFFFFFFFF, INT(), INT(), INT()),
        }
    else:
        specs = {
            "GetSolidHeight": lambda: (*GRID(), INT()),
            "PlaySfxAt": lambda: (SFX(), P(), INT(), INT()),
            "GimmickActivate": lambda: (GIM(), rng.choice([P(), ctx["gimmick_base"] + 0x18 * rng.randint(0, max(0, ctx["gimmicks"] - 1)),
                                                           INT()]), INT(), INT()),
            "MeshInitFromAreaTable": lambda: (INT(), INT(), INT(), INT()),
            "FUN_8013d358": lambda: (INT(), INT(), INT(), INT()),
            "CameraShake": lambda: (rng.choice([0, 1, 2, 3, 0xFFFFFFFF, INT()]), INT(), INT(), INT()),
            "EventCamStart": lambda: (rng.choice([0, 1, 2, INT()]), INT(), INT(), INT()),
            "PlaySfx": lambda: (rng.choice([0xB0, 0x4C, INT()]), INT(), INT(), INT()),
            "PropSetMesh": lambda: (INT(), INT(), INT(), INT()),
            "FUN_801414a0": lambda: (INT(), INT(), INT(), INT()),
            "MatrixLookDir": lambda: (P(), P(), INT(), INT()),
            "MatrixFromDirXZ": lambda: (P(), P(), INT(), INT()),
        }
    return specs[name]()


def direct_world_b(h, rng):
    """the world of the collision-body hooks: bodies of every type (check_bodies.build_world), the sound banks"""
    m = h.m
    info = cb.build_world(m, rng, n=rng.choice([2, 3, 4, 6, 8]), spread=rng.choice([150, 300, 600]))
    if rng.random() < 0.8:
        ks.request_setup(m, rng, rng.choice([0, 0, 0, 1, 3, 4, 5, 6]))
    m.write_bytes(BUF, small_halfwords(rng, wild=False))
    # A type-one collider exposes the body_type-dependent marker write in the probe adapter.
    m.write16(0x800F45D4, 1)
    collider = m.read32(0x800F455C)
    m.write16(collider + 0x12, 1)
    m.write16(collider + 0x18, 0)
    m.write16(collider + 0x32, 123)
    return dict(buf=BUF, bodies=info)


def run_hooks(h, scale, seed):
    """the per-hook layer: n cases for every hook of every wired table"""
    t0 = time.time()
    tables = hook_tables()
    lib = h.lib
    counts = dict(monkey=lib.shim_monkey_hook_count(), world=lib.shim_world_hook_count(),
                  gimmick=lib.shim_gimmick_hook_count(), spawn=lib.shim_spawn_hook_count())
    for group, table in tables.items():
        if counts[group] != len(table):
            raise SystemExit(f"{group}: the shim sees {counts[group]} hooks, the header lists {len(table)}")
    if not lib.shim_tables_linked(h.c.shim):
        h.fail("wiring", "tables", "spawn.world / monkey.body do not point at the game's own tables")
    n = max(1, int(scale / 100 * H_CASES))
    for group, table in tables.items():
        for idx, (name, addr) in enumerate(table):
            if h.only and not any(o[0] == f"hook-{group}" for o in h.only):
                continue
            if h.hook_filter is not None and (group, name) not in h.hook_filter:
                continue
            void = name in VOID_HOOKS[group]
            pose_specs = kp.case_specs(name, n, seed) if name in ("ModelPoseBones", "ModelBuildBoneMatrices", "ActorAnimDecode") else None
            for i in range(n):
                rng = random.Random(f"{seed}/hook/{group}/{name}/{i}")
                def world(harness, rng, i=i):
                    ctx = direct_world_a(harness, rng)
                    if name == "SoundLoopStopAndFree":
                        ks.stop_setup(harness.m, rng, 0x8005)
                    if pose_specs:
                        harness.m.load_area(kp.ASSETS.blob)
                        pose_specs[i][1](harness.m)
                    return ctx
                h.run_direct(f"hook-{group}", f"{name} #{i}", i, world, rng,
                             mips=lambda mm, ctx, args, addr=addr: h.mips_call(mm, addr, args),
                             cdo=lambda ctx, args, idx=idx, group=group: getattr(lib, f"shim_{group}_hook")(h.c.shim, idx, *args),
                             args=lambda ctx, group=group, name=name, rng=rng, i=i: (tuple(pose_specs[i][2]) + (0,) * (4 - len(pose_specs[i][2]))) if pose_specs else arg_gen(group, name, rng, ctx),
                             void=void, rich=(i % 2 == 0))
    run_body_hooks(h, n, seed)
    if not h.only and h.hook_filter is None:
        for group, table in tables.items():
            for name, _ in table:
                if not h.completed_hooks[(f"hook-{group}", name)]:
                    h.fail("coverage", name, "no completed direct comparison for this hook")
        for name in ("printf_error", "sound_loop_start", "sound_loop_stop", "build_probe_points",
                     "collision_ray_test", "gimmick_type_to_sfx", "play_sfx_at", "pickup_collect", "camera_nudge",
                     "stub_return_zero", "anim_play_sfx", "anim_play_player_voice", "anim_event_callback",
                     "spawn_position_padding"):
            if not h.completed_hooks[("hook-body", name)]:
                h.fail("coverage", name, "no completed direct comparison for this hook")
    print(f"hook layer done in {time.time() - t0:.1f}s")


def run_body_hooks(h, n, seed):
    lib, shim = h.lib, h.c.shim
    I = lambda rng: rng.choice([0, 1, 2, 3, -1, 5, 0x16, 100, 0x7FFF, -0x8000, rng.randint(-5000, 5000),  # noqa: E731
                                rng.getrandbits(32)]) & M32
    BODY = lambda rng, ctx: rng.choice([b[3] for b in ctx["bodies"] if b[3]] or [0])  # noqa: E731

    def case(name, i, make_args, mips, cdo, void=False):
        if h.only and not any(o[0] == "hook-body" for o in h.only):
            return
        if h.hook_filter is not None and ("body", name) not in h.hook_filter:
            return
        rng = random.Random(f"{seed}/hook/body/{name}/{i}")
        h.run_direct("hook-body", f"{name} #{i}", i, direct_world_b, rng, mips=mips, cdo=cdo,
                     args=lambda ctx: make_args(rng, ctx), void=void, rich=(i % 2 == 0))

    for i in range(n):
        case("printf_error", i, lambda rng, ctx: (rng.choice([0x800A5D20, 0x800A5D20, 0x800A4BBC]), I(rng)),
             lambda mm, ctx, a: h.mips_call(mm, PRINTF, a), lambda ctx, a: lib.shim_body_printf_error(shim, *a), void=True)
        case("sound_loop_start", i, lambda rng, ctx: (I(rng), rng.choice([0x16, I(rng)])),
             lambda mm, ctx, a: h.mips_call(mm, 0x800878E0, a), lambda ctx, a: lib.shim_body_sound_loop_start(shim, *a))
        case("sound_loop_stop", i, lambda rng, ctx: (rng.choice([0x16, I(rng)]), I(rng)),
             lambda mm, ctx, a: h.mips_call(mm, 0x8008806C, a), lambda ctx, a: lib.shim_body_sound_loop_stop(shim, *a), void=True)
        case("build_probe_points", i, lambda rng, ctx: (3, BODY(rng, ctx)),
             lambda mm, ctx, a: h.mips_call(mm, 0x8007304C, a), lambda ctx, a: lib.shim_body_build_probe_points(shim, *a), void=True)
        case("gimmick_type_to_sfx", i, lambda rng, ctx: (I(rng),),
             lambda mm, ctx, a: h.mips_call(mm, 0x8005BB98, a), lambda ctx, a: lib.shim_body_gimmick_type_to_sfx(shim, *a))
        case("pickup_collect", i, lambda rng, ctx: (I(rng),),
             lambda mm, ctx, a: h.mips_call(mm, 0x80078A18, a), lambda ctx, a: lib.shim_body_pickup_collect(shim, *a), void=True)
        case("camera_nudge", i, lambda rng, ctx: (I(rng), I(rng), I(rng)),
             lambda mm, ctx, a: h.mips_call(mm, 0x80047690, a), lambda ctx, a: lib.shim_body_camera_nudge(shim, *a), void=True)
        case("stub_return_zero", i, lambda rng, ctx: (),
             lambda mm, ctx, a: h.mips_call(mm, 0x80098320, a), lambda ctx, a: lib.shim_body_stub_return_zero(shim))
        case("anim_play_sfx", i, lambda rng, ctx: (I(rng),),
             lambda mm, ctx, a: h.mips_call(mm, 0x800877F8, a), lambda ctx, a: lib.shim_anim_play_sfx(shim, *a), void=True)
        case("anim_play_player_voice", i, lambda rng, ctx: (I(rng),),
             lambda mm, ctx, a: h.mips_call(mm, 0x8006896C, a), lambda ctx, a: lib.shim_anim_play_player_voice(shim, *a), void=True)
        case("anim_event_callback", i, lambda rng, ctx: (rng.choice([0x800B027C, 0x8007C468, I(rng)]),),
             lambda mm, ctx, a: mm.missing("PlayerEventCallback", MISSING["PlayerEventCallback"][0], *a, 0, 0, 0),
             lambda ctx, a: lib.shim_anim_event_callback(shim, *a), void=True)
        case("spawn_position_padding", i, lambda rng, ctx: (rng.choice([0x800696EC, 0x8006B3F0, I(rng)]),),
             lambda mm, ctx, a: 0, lambda ctx, a: lib.shim_spawn_position_padding(shim, *a))

        # the two hooks that pass pointers to C arrays
        def ray_make(rng, ctx):
            return (rng.choice([0, 1, I(rng)]), rng.choice([0x1F800108, BUF]))

        def ray_c(ctx, a):
            out = (C.c_int16 * 2)(0x1234, 0x4321)
            res = lib.shim_body_collision_ray_test(shim, a[0], a[1], out)
            if (out[0], out[1]) != (0, 0):
                raise AssertionError(f"collision_ray_test left out = {out[0]}, {out[1]} (the port must see 0, 0)")
            return res
        case("collision_ray_test", i, ray_make, lambda mm, ctx, a: h.mips_call(mm, 0x80078C9C, (a[0], a[1], 0x801FFE40)), ray_c)

        def sfx_make(rng, ctx):
            return (rng.choice([rng.randint(0, 197), 9, I(rng)]), BUF + 8 * rng.randrange(0x400))

        def sfx_c(ctx, a):
            pos = (C.c_int16 * 3)(*[h.c.mem.rds16(a[1] + 2 * k) for k in range(3)])
            return lib.shim_body_play_sfx_at(shim, a[0], pos)
        case("play_sfx_at", i, sfx_make, lambda mm, ctx, a: h.mips_call(mm, 0x80087A20, a), sfx_c, void=True)


def setup_ai(rng, **kw):
    return lambda h: ai_frame_world(h, rng, **kw)


def setup_monkeys(rng, **kw):
    return lambda h: monkey_world(h, rng, **kw)


def setup_boundary(rng, name, i):
    """two bodies placed exactly on the thresholds of one body-vs-body test (check_bodies.boundary_world), monkeys off"""
    def setup(h):
        cb.boundary_world(h.m, rng, name, i)
        W(h.m).w16(0x800F44D4, 0)
        return []
    return setup


def setup_bodies(rng, **kw):
    """collision bodies of every type (check_bodies.build_world); the monkeys stay switched off"""
    def setup(h):
        m = h.m
        cb.build_world(m, rng, n=rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 14]), spread=rng.choice([150, 300, 600]),
                       **kw)
        W(m).w16(0x800F44D4, 0)             # g_MonkeyCount: the monkey system is off
        return []
    return setup


def scenarios(h, scale, seed, frames):
    t0 = time.time()

    def n(count):
        return max(1, int(count * scale / 100))

    def rng_of(name, i):
        return random.Random(f"{seed}/{name}/{i}")

    def go(family, i, make_setup, rich):
        if h.only and (family, i) not in h.only:
            return
        h.run(family, f"#{i}", i, make_setup(rng_of(family, i)), frames, rich=rich)

    for i in range(n(36)):
        go("monkeys", i, setup_monkeys, i % 2 == 0)
    for i in range(n(40)):
        go("ai", i, setup_ai, i % 2 == 0)
    for i in range(n(16)):
        go("ai-world", i, lambda rng: setup_ai(rng, world=True), i % 2 == 0)
    for i in range(n(24)):
        go("bodies", i, setup_bodies, i % 2 == 0)
    # Guarantee every CollisionUpdate dispatch type, rather than relying on random draws.
    for typ in range(1, 18):
        go("body-type", typ, lambda rng, typ=typ: setup_bodies(rng, force_types=[typ]), typ % 2 == 0)
    for name in ("BodyCollideWithBodies", "BodyFindOverlapType1", "BodyFindOverlapType3to5", "BodyCheckPickupContact",
                 "BodyCollideBodiesDetailed"):
        for i in range(n(8)):
            go("boundary-" + name[4:], i, lambda rng, name=name, i=i: setup_boundary(rng, name, i * 7), i % 2 == 0)
    print(f"scenario families done in {time.time() - t0:.1f}s")


# ---------------------------------------------------------------------------------------------------------- mutation test
# Wrong variants of src/game/ae_game.c (swapped adapter arguments, a dropped sign extension / truncation, a hook wired to the
# wrong function or the wrong host entry, a wrong host name, a table not linked, the frame order): every one must be caught.
# (label, anchor = a text that identifies a line of ae_game.c, old text in that line, new text, what to run)
# run: ("hook", group, name) | ("body", name) | ("frames",) | ("tables",)
MUTATIONS = [
    ("mk GimmickGetInfo swap", "MONKEY_HOOK(GimmickGetInfo)", "(int32_t)a0, a1", "(int32_t)a1, a0", ("hook", "monkey", "GimmickGetInfo")),
    ("mk PlaySfxAt swap", "MONKEY_HOOK(PlaySfxAt)", "(int32_t)a0, a1", "(int32_t)a1, a0", ("hook", "monkey", "PlaySfxAt")),
    ("mk ProjectileSpawn swap", "MONKEY_HOOK(ProjectileSpawn)", "a0, a1, (int32_t)a2, a3", "a1, a0, (int32_t)a2, a3", ("hook", "monkey", "ProjectileSpawn")),
    ("mk ProjectileSpawn velocity", "MONKEY_HOOK(ProjectileSpawn)", "(int32_t)a2, a3", "(int32_t)a2, a2", ("hook", "monkey", "ProjectileSpawn")),
    ("mk SpawnParticle swap", "MONKEY_HOOK(SpawnParticle)", "(int32_t)a0, a1", "(int32_t)a1, a0", ("hook", "monkey", "SpawnParticle")),
    ("mk EffectBigBurst swap", "MONKEY_HOOK(EffectBigBurst)", "(int32_t)a1, (int32_t)a2", "(int32_t)a2, (int32_t)a1", ("hook", "monkey", "EffectBigBurst")),
    ("mk SparkAddRandom swap", "MONKEY_HOOK(SparkAddRandom)", "a0, a1, (int32_t)a2", "a1, a0, (int32_t)a2", ("hook", "monkey", "SparkAddRandom")),
    ("mk GetGroundHeight swap", "MONKEY_HOOK(GetGroundHeight)", "(int32_t)a0, (int32_t)a1, (int32_t)a2", "(int32_t)a2, (int32_t)a1, (int32_t)a0", ("hook", "monkey", "GetGroundHeight")),
    ("mk ObjSlotGetAnimIndex truncation", "MONKEY_HOOK(ObjSlotGetAnimIndex)", "(int32_t)a0", "(int32_t)(uint8_t)a0", ("hook", "monkey", "ObjSlotGetAnimIndex")),
    ("mk ObjSlotGetTarget swap", "MONKEY_HOOK(ObjSlotGetTarget)", "a0, a1", "a1, a0", ("hook", "monkey", "ObjSlotGetTarget")),
    ("mk SoundLoopStartEx swap", "MONKEY_HOOK(SoundLoopStartEx)", "(int32_t)a1, (int32_t)a2", "(int32_t)a2, (int32_t)a1", ("hook", "monkey", "SoundLoopStartEx")),
    ("mk SoundLoopStopAndFree truncation", "MONKEY_HOOK(SoundLoopStopAndFree)", "(int32_t)a1", "(int32_t)(int16_t)a1", ("hook", "monkey", "SoundLoopStopAndFree")),
    ("mk SpawnPuff arg", "MONKEY_HOOK(SpawnPuff)", "->spawn, a0)", "->spawn, a1)", ("hook", "monkey", "SpawnPuff")),
    ("mk MatrixLookDir wrong function", "MONKEY_HOOK(MatrixLookDir)", "ae_sdk_MatrixLookDir", "ae_sdk_MatrixFromDirXZ", ("hook", "monkey", "MatrixLookDir")),
    ("mk MatrixFromDirXZ swap", "MONKEY_HOOK(MatrixFromDirXZ)", "(r, a0, a1)", "(r, a1, a0)", ("hook", "monkey", "MatrixFromDirXZ")),
    ("mk ScaleMatrix swap", "MONKEY_HOOK(ScaleMatrix)", "(r, a0, a1)", "(r, a1, a0)", ("hook", "monkey", "ScaleMatrix")),
    ("mk ModelBuildBoneMatrices arg", "MONKEY_HOOK(ModelBuildBoneMatrices)", "(r, a0, a1)", "(r, a0, 3)", ("hook", "monkey", "ModelBuildBoneMatrices")),
    ("mk ModelPoseBones swap", "MONKEY_HOOK(ModelPoseBones)", "(r, a0, a1)", "(r, a1, a0)", ("hook", "monkey", "ModelPoseBones")),
    ("mk GimmickEvent swap", "MONKEY_HOOK(GimmickEvent)", "a0, (int32_t)a1, a2", "a0, (int32_t)a2, a1", ("hook", "monkey", "GimmickEvent")),
    ("mk GimmickEvent index zero extension", "MONKEY_HOOK(GimmickEvent)", "(int32_t)a1", "(int32_t)(uint16_t)a1", ("hook", "monkey", "GimmickEvent")),
    ("mk ClearWords count zero extension", "MONKEY_HOOK(ClearWords)", "(int32_t)a1", "(int32_t)(uint16_t)a1", ("hook", "monkey", "ClearWords")),
    ("mk ActorAnimDecode arg", "MONKEY_HOOK(ActorAnimDecode)", "(r, a0)", "(r, a1)", ("hook", "monkey", "ActorAnimDecode")),
    ("mk TargetListRemove wrong function", "MONKEY_HOOK(TargetListRemove)", "ae_TargetListRemove(r, a0)", "ae_TargetListAdd(r, a0)", ("hook", "monkey", "TargetListRemove")),
    ("mk TargetListAdd arg", "MONKEY_HOOK(TargetListAdd)", "(r, a0)", "(r, a1)", ("hook", "monkey", "TargetListAdd")),
    ("mk StageExitIsActive result", "MONKEY_HOOK(StageExitIsActive)", "return ae_StageExitIsActive(r);", "return !ae_StageExitIsActive(r);", ("hook", "monkey", "StageExitIsActive")),
    ("mk ModelInit wrong missing entry", "MONKEY_HOOK(ModelInit)", "M_ModelInit", "M_PlaySfx", ("hook", "monkey", "ModelInit")),
    ("mk VSync off by one", "g->host.vblank_count ? g->host.vblank_count", "g->host.vblank_count(g->host.user) : 0", "g->host.vblank_count(g->host.user) + 1 : 0", ("hook", "monkey", "VSync")),
    ("render hook as debug", "RENDER_HOOK(ColorFadeSet)", "RENDER_HOOK(ColorFadeSet)", "DEBUG_HOOK(ColorFadeSet)", ("hook", "monkey", "ColorFadeSet")),
    ("debug hook as render", "DEBUG_HOOK(MenuItemSetStep)", "DEBUG_HOOK(MenuItemSetStep)", "RENDER_HOOK(MenuItemSetStep)", ("hook", "monkey", "MenuItemSetStep")),
    ("mk Printf argument", "MONKEY_HOOK(Printf)", "\"Printf\", a0, a1, a2, a3", "\"Printf\", a0, a2, a1, a3", ("hook", "monkey", "Printf")),
    ("world GetSolidHeight swap", "HOOK6(world, GetSolidHeight)", "(int32_t)a0, (int32_t)a1, (int32_t)a2", "(int32_t)a0, (int32_t)a2, (int32_t)a1", ("hook", "world", "GetSolidHeight")),
    ("world PlaySfxAt swap", "HOOK6(world, PlaySfxAt)", "(int32_t)a0, a1", "(int32_t)a1, a0", ("hook", "world", "PlaySfxAt")),
    ("world GimmickActivate swap", "HOOK6(world, GimmickActivate)", "(int32_t)a0, a1", "(int32_t)a1, a0", ("hook", "world", "GimmickActivate")),
    ("world CameraShake argument", "HOOK6(world, CameraShake)", "(int32_t)a0", "0", ("hook", "world", "CameraShake")),
    ("world EventCamStart wrong entry", "HOOK6(world, EventCamStart)", "M_EventCamStart", "M_CameraNudge", ("hook", "world", "EventCamStart")),
    ("world FUN_8013d358 wrong entry", "HOOK6(world, Ovl8013d358)", "M_Ovl8013d358", "M_Ovl801414a0", ("hook", "world", "FUN_8013d358")),
    ("world MeshInit render name", "HOOK6(world, MeshInitFromAreaTable)", "\"MeshInitFromAreaTable\"", "\"PropSetMesh\"", ("hook", "world", "MeshInitFromAreaTable")),
    ("gimmick PlaySfx wrong entry", "HOOK6(gimmick, PlaySfx)", "M_PlaySfx", "M_PlayPlayerVoice", ("hook", "gimmick", "PlaySfx")),
    ("gimmick PropSetMesh render name", "HOOK6(gimmick, PropSetMesh)", "\"PropSetMesh\"", "\"MeshInitFromAreaTable\"", ("hook", "gimmick", "PropSetMesh")),
    ("gimmick FUN_801414a0 arguments", "HOOK6(gimmick, Ovl801414a0)", "a0, a1, a2, a3", "a1, a0, a2, a3", ("hook", "gimmick", "FUN_801414a0")),
    ("gimmick PlaySfxAt wired to GetSolidHeight", "gm->PlaySfxAt", "world_PlaySfxAt", "world_GetSolidHeight", ("hook", "gimmick", "PlaySfxAt")),
    ("spawn PlaySfx wrong entry", "HOOK6(spawn, PlaySfx)", "M_PlaySfx", "M_ModelInit", ("hook", "spawn", "PlaySfx")),
    ("spawn MatrixLookDir swap", "HOOK6(spawn, MatrixLookDir)", "(r, a0, a1)", "(r, a1, a0)", ("hook", "spawn", "MatrixLookDir")),
    ("spawn MatrixFromDirXZ wrong function", "HOOK6(spawn, MatrixFromDirXZ)", "ae_sdk_MatrixFromDirXZ", "ae_sdk_MatrixLookDir", ("hook", "spawn", "MatrixFromDirXZ")),
    ("spawn position padding", "(void)user; (void)r; (void)original_function;", "(void)original_function;", "(void)original_function; return 1;", ("body", "spawn_position_padding")),
    ("body printf_error swap", "debug(game_of(user), \"Printf\", fmt", "fmt, (uint32_t)type, 0, 0", "(uint32_t)type, fmt, 0, 0", ("body", "printf_error")),
    ("body sound_loop_start swap", "M_SoundLoopStart", "(uint32_t)a, (uint32_t)id", "(uint32_t)id, (uint32_t)a", ("body", "sound_loop_start")),
    ("body build_probe_points body", "ae_CollisionBuildProbePoints", "(r, type, body)", "(r, type, body + 4)", ("body", "build_probe_points")),
    ("body build_probe_points type", "ae_CollisionBuildProbePoints", "(r, type, body)", "(r, 1, body)", ("body", "build_probe_points")),
    ("body ray test out", "out[0] = out[1] = 0;", "out[0] = out[1] = 0;", "out[0] = out[1] = 1;", ("body", "collision_ray_test")),
    ("body ray test swap", "M_CollisionRayTest", "arg, params, 0, 0", "params, arg, 0, 0", ("body", "collision_ray_test")),
    ("body gimmick_type_to_sfx arg", "M_GimmickTypeToSfx", "(uint32_t)collider_type", "0", ("body", "gimmick_type_to_sfx")),
    ("body play_sfx_at axis order", "ae_wr16(r, AE_GAME_ARG_SCRATCH", "(uint16_t)pos[axis]", "(uint16_t)pos[2 - axis]", ("body", "play_sfx_at")),
    ("body play_sfx_at id", "ae_PlaySfxAt(r, &game_of(user)->host.sound_hw, sfx", "sfx, AE_GAME_ARG_SCRATCH", "sfx + 1, AE_GAME_ARG_SCRATCH", ("body", "play_sfx_at")),
    ("body pickup_collect wrong entry", "missing(game_of(user), M_PickupCollect", "M_PickupCollect", "M_CameraNudge", ("body", "pickup_collect")),
    ("body camera_nudge swap", "M_CameraNudge", "(uint32_t)dx, (uint32_t)dy, (uint32_t)dz", "(uint32_t)dy, (uint32_t)dx, (uint32_t)dz", ("body", "camera_nudge")),
    ("anim play_sfx wrong entry", "M_PlaySfx, (uint32_t)id", "M_PlaySfx, (uint32_t)id", "M_PlayPlayerVoice, (uint32_t)id", ("body", "anim_play_sfx")),
    ("anim event_callback argument", "M_PlayerEventCallback", "function, 0, 0, 0", "0, function, 0, 0", ("body", "anim_event_callback")),
    ("table spawn.world unlinked", "sp->world = w;", "sp->world = w;", "sp->world = NULL;", ("tables",)),
    ("table monkey.body unlinked", "m->body = b;", "m->body = b;", "m->body = NULL;", ("tables",)),
    ("table CameraShake wired to EventCamStart", "w->CameraShake", "world_CameraShake", "world_EventCamStart", ("hook", "world", "CameraShake")),
    ("table gimmick hook missing", "gm->PropSetMesh", "gm->PropSetMesh = gimmick_PropSetMesh;", "gm->PropSetMesh = NULL;", ("hook", "gimmick", "PropSetMesh")),
    ("wire ColorFadeSet to DrawShadow", "#undef WIRE", "#undef WIRE", "m->ColorFadeSet = mk_DrawShadow;\n#undef WIRE", ("hook", "monkey", "ColorFadeSet")),
    ("wire SetTransMatrix to SetRotMatrix", "#undef WIRE", "#undef WIRE", "m->SetTransMatrix = mk_SetRotMatrix;\n#undef WIRE", ("hook", "monkey", "SetTransMatrix")),
    ("wire TargetListAdd to Remove", "#undef WIRE", "#undef WIRE", "m->TargetListAdd = mk_TargetListRemove;\n#undef WIRE", ("hook", "monkey", "TargetListAdd")),
    ("wire ObjSlotGetTarget to AnimIndex", "#undef WIRE", "#undef WIRE", "m->ObjSlotGetTarget = mk_ObjSlotGetAnimIndex;\n#undef WIRE", ("hook", "monkey", "ObjSlotGetTarget")),
    ("wire GimmickEvent unwired", "#undef WIRE", "#undef WIRE", "m->GimmickEvent = NULL;\n#undef WIRE", ("hook", "monkey", "GimmickEvent")),
    ("frame order", "ae_MonkeyUpdateAll(g->ram, &g->monkey);", "ae_MonkeyUpdateAll(g->ram, &g->monkey);", "ae_CollisionUpdate(g->ram, &g->body); ae_MonkeyUpdateAll(g->ram, &g->monkey);", ("frames",)),
    ("frame without CollisionUpdate", "ae_CollisionUpdate(g->ram, &g->body);", "ae_CollisionUpdate(g->ram, &g->body);", "(void)0;", ("frames",)),
]


def mutate_text(text, anchor, old, new):
    lines = text.split("\n")
    hits = [i for i, line in enumerate(lines) if anchor in line]
    if not hits:
        raise ValueError(f"anchor {anchor!r} not found")
    for i in hits:
        if old in lines[i]:
            lines[i] = lines[i].replace(old, new, 1)
            return "\n".join(lines)
    raise ValueError(f"{old!r} not found in the line(s) of {anchor!r}")


def run_mutations(scale, seed, only_labels=(), frames=3):
    import contextlib
    import io
    text = (ROOT / "src/game/ae_game.c").read_text(encoding="utf-8")
    tmp = Path(tempfile.mkdtemp(prefix="game_mut_"))
    h = Harness(with_coverage=False)
    h.stop_after = 1
    survivors, tested = [], 0
    for k, (label, anchor, old, new, target) in enumerate(MUTATIONS):
        if only_labels and not any(w in label for w in only_labels):
            continue
        try:
            mutated = mutate_text(text, anchor, old, new)
        except ValueError as e:
            print(f"mutation {label:44s} PATTERN MISSING ({e})", flush=True)
            survivors.append(label + " (pattern missing)")
            continue
        path = tmp / f"ae_game_mut{k}.c"
        path.write_text(mutated, encoding="utf-8")
        try:
            dll = build_dll({"src/game/ae_game.c": path}, tag="gamemut")
        except SystemExit as e:
            print(f"mutation {label:44s} BUILD ERROR (invalid mutation): {str(e)[:300]}", flush=True)
            survivors.append(label + " (build error)")
            continue
        h.attach(dll)
        h.reset_counters()
        tested += 1
        buf = io.StringIO()
        try:
            with contextlib.redirect_stdout(buf):
                if target[0] == "hook":
                    h.hook_filter = {(target[1], target[2])}
                    run_hooks(h, scale, seed)
                elif target[0] == "body":
                    h.hook_filter = {("body", target[1])}
                    run_hooks(h, scale, seed)
                elif target[0] == "tables":
                    h.hook_filter = set()
                    run_hooks(h, scale, seed)
                else:
                    h.hook_filter = None
                    scenarios(h, 25, seed, frames)
        except SystemExit:
            pass
        survived = h.failures == 0
        reason = next((line.strip() for line in buf.getvalue().splitlines() if line.startswith("FAIL ")), "")
        print(f"mutation {label:44s} {'SURVIVED (checker too weak)' if survived else 'caught'} {reason}", flush=True)
        if survived:
            survivors.append(label)
    print(f"{tested} mutations tested")
    return survivors


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", type=float, default=100, help="scale of the case counts in percent")
    ap.add_argument("--seed", type=int, default=0x6A3E01)
    ap.add_argument("--frames", type=int, default=4)
    ap.add_argument("--stop-after", type=int, default=0)
    ap.add_argument("--only", nargs="*", default=[], help="run only these cases, family:index (for example monkeys:22)")
    ap.add_argument("--survey", action="store_true", help="discovery: list the RAM bytes that differ (resyncs after every frame)")
    ap.add_argument("--traps", action="store_true", help="print one example of every kind of agreed original fault")
    ap.add_argument("--hooks-only", action="store_true", help="only the per-hook layer")
    ap.add_argument("--frames-only", action="store_true", help="only the whole-frame layer")
    ap.add_argument("--mutate", action="store_true")
    ap.add_argument("--mutate-only", nargs="*", default=[])
    a = ap.parse_args()
    if a.cases <= 0 or a.frames < 2 or (a.hooks_only and a.frames_only):
        ap.error("cases must be positive, frames at least two, and layer filters mutually exclusive")
    if not (ROOT / "extract/disc/SCUS_944.23").exists() or not (ROOT / "extract/members/05B.bin").exists():
        print("SKIP: extract/disc/SCUS_944.23 or extract/members not present")
        return 0
    if a.mutate:
        survivors = run_mutations(100, a.seed, a.mutate_only, a.frames)
        print("all mutations caught" if not survivors else f"SURVIVORS: {survivors}")
        return 1 if survivors else 0
    t0 = time.time()
    h = Harness()
    reg.declare("unreachable", "rejected invalid random scenarios")
    h.stop_after = a.stop_after or None
    if a.survey:
        h.survey = collections.Counter()
    h.only = {(x.split(':')[0], int(x.split(':')[1])) for x in a.only}
    if not a.hooks_only:
        scenarios(h, a.cases, a.seed, a.frames)
    if not a.frames_only:
        run_hooks(h, a.cases, a.seed)
    if h.survey is not None:
        for (fam, key), cnt in sorted(((k, v) for k, v in h.survey.items() if k != "printed"), key=lambda kv: str(kv[0])):
            print(f"  survey {fam:8s} {key if isinstance(key, str) else hex(0x80000000 + key) if key < 0x200000 else 'scratch+' + hex(key - 0x200000)}  frames {cnt}")
    print("cases per family:", dict(h.counts), "frames:", dict(h.frames))
    print(f"rejected faulting cases (both sides flagged, no RAM/call proof): {h.traps_agreed}; runaway cases skipped: {h.skipped}")
    if a.traps:
        for ex in h.trap_example.values():
            print("  trap example:", ex)
    for kind, cnt in h.trap_kinds.most_common(12):
        print(f"  trap {cnt:4d} {kind}")
    print("host calls compared (whole frames / direct hook calls):")
    names = sorted({k[1:] for k in h.reached})
    for kind, name in names:
        print(f"  {kind:8s} {name:36s} {h.reached[('frame', kind, name)]:7d} {h.reached[('hook', kind, name)]:7d}")
    missing_frame = [n for k, n in names if k == "missing" and not h.reached[("frame", k, n)]]
    unreached = [n for n in MISSING if ("missing", n) not in names]
    print(f"missing functions not reached by any whole frame: {missing_frame}; never reached: {unreached}")
    print("original entry points of ported functions executed:")
    for name, ok in h.entries_reached().items():
        print(f"  {name:28s} {'yes' if ok else 'NO'}")
    frame_count = sum(count for family, count in h.frames.items() if not family.startswith("hook-"))
    direct_count = sum(count for family, count in h.frames.items() if family.startswith("hook-"))
    print(f"total cases {sum(h.counts.values())}; frames {frame_count}; direct comparisons {direct_count}; host calls compared {h.hook_calls}; "
          f"MIPS instructions executed {h.m.steps}; {time.time() - t0:.1f}s")
    if h.failures:
        print(f"FAILED: {h.failures} mismatching cases")
        return 1
    report = dict(seed=a.seed, cases=dict(h.counts), comparisons=dict(h.frames),
                  rejected_faults=h.traps_agreed, runaway=h.skipped, failures=h.failures,
                  host_calls=[dict(layer=layer, kind=kind, name=name, count=count)
                              for (layer, kind, name), count in sorted(h.reached.items())],
                  completed_hooks=[dict(family=family, name=name, count=count)
                                   for (family, name), count in sorted(h.completed_hooks.items())],
                  entries=h.entries_reached(), seconds=round(time.time() - t0, 2))
    report_name = "game-filtered-results.json" if a.hooks_only or a.frames_only or a.only or a.survey else "game-results.json"
    report_path = ROOT / "extract/codex-task" / report_name
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("OK: completed frame and hook comparisons match the original instructions")
    return 0


if __name__ == "__main__":
    sys.exit(main())
