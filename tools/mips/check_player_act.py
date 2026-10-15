"""Differential check for the player action and mode setter ports.

The original EXE runs on a byte-exact RAM image with real player state tables and area 1;
the readable C ports run on an identical AeRam snapshot. Only the original host stack is
excluded. External systems are deterministic named hooks and their call sequences are compared.
"""
from __future__ import annotations

import argparse
import ctypes as C
import os
import random
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import exclusion_registry as reg  # noqa: E402
import mips_ram as mr  # noqa: E402

OUT = ROOT / "extract/mips"
PLAYER = 0x800EC1E0
PARAMS = 0x800EC010
STATE_SETS = 0x800F4DE0
MODE = 0x800F44A0
INPUT = 0x800E5320
SCRATCH = 0x1F800000
ACTOR = 0x80140000
ACTOR_MODEL = 0x80140200
IDLE_ACTOR = 0x80140400
M32 = 0xFFFFFFFF
STACK_LO = mr.STACK_WINDOW[0] - 0x80000000
STACK_HI = mr.STACK_WINDOW[1] - 0x80000000

# name -> (entry, byte size, C symbol, has hook table, argument kinds)
FUNCS = {
    "PlayerSetState": (0x80064208, 588, "ae_PlayerSetState", True, "u"),
    "PlayerAct_ScaleMoveVel": (0x80065C20, 252, "ae_PlayerAct_ScaleMoveVel", True, "uu"),
    "PlayerAct_JumpHoldBoost": (0x80065D1C, 76, "ae_PlayerAct_JumpHoldBoost", True, ""),
    "PlayerAct_JumpPct3": (0x80065D68, 132, "ae_PlayerAct_JumpPct3", False, ""),
    "PlayerAct_VerticalOnly": (0x80065DEC, 44, "ae_PlayerAct_VerticalOnly", False, ""),
    "PlayerAct_JumpPct2": (0x80065E18, 132, "ae_PlayerAct_JumpPct2", False, ""),
    "PlayerAct_SquashReset": (0x80065E9C, 56, "ae_PlayerAct_SquashReset", True, ""),
    "PlayerAct_FlyerThrust": (0x80065ED4, 244, "ae_PlayerAct_FlyerThrust", True, ""),
    "PlayerAct_FlyerLaunchA": (0x80065FC8, 40, "ae_PlayerAct_FlyerLaunchA", False, ""),
    "PlayerAct_FlyerLaunchB": (0x80065FF0, 40, "ae_PlayerAct_FlyerLaunchB", False, ""),
    "PlayerAct_GadgetSpinFromStick": (0x80066028, 108, "ae_PlayerAct_GadgetSpinFromStick", True, ""),
    "PlayerAct_Gadget3Aim": (0x800660C8, 32, "ae_PlayerAct_Gadget3Aim", True, ""),
    "PlayerAct_DriftForward": (0x800660E8, 52, "ae_PlayerAct_DriftForward", False, ""),
    "PlayerAct_NegateInputXZ": (0x8006611C, 36, "ae_PlayerAct_NegateInputXZ", False, ""),
    "PlayerAct_TurnAround": (0x80066140, 40, "ae_PlayerAct_TurnAround", False, ""),
    "PlayerAct_TurnAroundAndJump": (0x80066168, 40, "ae_PlayerAct_TurnAroundAndJump", True, "uu"),
    "PlayerAct_Nop": (0x80066190, 8, "ae_PlayerAct_Nop", False, ""),
    "PlayerAct_FaceStick": (0x80066198, 60, "ae_PlayerAct_FaceStick", False, ""),
    "PlayerAct_JumpFromLStick": (0x800661D4, 264, "ae_PlayerAct_JumpFromLStick", False, ""),
    "PlayerAct_StepBackDrop": (0x800662DC, 124, "ae_PlayerAct_StepBackDrop", False, ""),
    "PlayerAct_BounceBack": (0x80066358, 220, "ae_PlayerAct_BounceBack", True, ""),
    "PlayerAct_ExitToMode0Forward": (0x80066434, 156, "ae_PlayerAct_ExitToMode0Forward", True, ""),
    "PlayerAct_AnimSpeedNormal": (0x800664D0, 16, "ae_PlayerAct_AnimSpeedNormal", False, ""),
    "PlayerAct_AnimSpeedHalf": (0x800664E0, 16, "ae_PlayerAct_AnimSpeedHalf", False, ""),
    "PlayerAct_FaceStickMaybeHalfSpeed": (0x800664F0, 64, "ae_PlayerAct_FaceStickMaybeHalfSpeed", False, ""),
    "PlayerAct_FaceStickRotateA": (0x80066530, 56, "ae_PlayerAct_FaceStickRotateA", False, ""),
    "PlayerAct_FaceStickRotateB": (0x80066568, 56, "ae_PlayerAct_FaceStickRotateB", False, ""),
    "PlayerAct_AimModeEnter": (0x800665A0, 32, "ae_PlayerAct_AimModeEnter", True, ""),
    "PlayerAct_AimModeLeave": (0x800665C0, 32, "ae_PlayerAct_AimModeLeave", True, ""),
    "PlayerAct_FaceRightPerp": (0x800665F8, 68, "ae_PlayerAct_FaceRightPerp", True, ""),
    "PlayerAct_FaceLeftPerp": (0x8006663C, 68, "ae_PlayerAct_FaceLeftPerp", True, ""),
    "PlayerAct_TurnFacingLeft90": (0x80066680, 36, "ae_PlayerAct_TurnFacingLeft90", False, ""),
    "PlayerAct_TurnFacingRight90": (0x800666A4, 36, "ae_PlayerAct_TurnFacingRight90", False, ""),
    "PlayerAct_FaceStickOnRotation": (0x800666C8, 84, "ae_PlayerAct_FaceStickOnRotation", False, ""),
    "PlayerAct_Rebound": (0x8006671C, 40, "ae_PlayerAct_Rebound", False, ""),
    "PlayerAct_HurtVoiceFlag": (0x80066744, 44, "ae_PlayerAct_HurtVoiceFlag", True, ""),
    "PlayerAct_Land": (0x80066770, 304, "ae_PlayerAct_Land", True, ""),
    "PlayerSetClimbTarget": (0x80066E28, 100, "ae_PlayerSetClimbTarget", False, "ssssss"),
    "PlayerSetRope": (0x80066E8C, 96, "ae_PlayerSetRope", False, "uu"),
    "PlayerSetEyeFrame": (0x80067C08, 164, "ae_PlayerSetEyeFrame", True, "s"),
    "PlayerSetTintEffect": (0x8006C2E8, 76, "ae_PlayerSetTintEffect", False, "s"),
    "PlayerSetMode": (0x8007B374, 2344, "ae_PlayerSetMode", True, "s"),
}
EXTRA_COVERAGE = {
    "ApplyMatrixSV": (0x800300B0, 0x5C),
    "VectorNormalSS": (0x800301E0, 0x30),
    "VectorNormalSS core": (0x80030210, 0xC0),
    "VecLength3": (0x80046618, 0x4C),
    "IntSqrt": (0x80046584, 0x94),
}

# MIPS hooks are deliberately kept outside the ported batch. Their MIPS register arguments
# are recorded in raw 32-bit form so C signedness cannot hide an ABI mismatch.
HOOKS = {
    0x80015240: "AnimFramePtr", 0x8002B380: "DR_MOVE", 0x80014168: "PadRumble",
    0x80048430: "AimRequestClear", 0x800650D8: "PlayerInput_StickGesture",
    0x8007AA98: "Gadget3AimUpdateAmmo", 0x8004B4B8: "AimModeEnter",
    0x8004B510: "AimModeLeave", 0x8006896C: "PlayPlayerVoice",
    0x80064AF8: "PlayerCond_JumpHeld",
    0x8006BF84: "EffectGroundImpactAtPlayer", 0x8005E6F0: "PlayerDamage",
    0x800877F8: "PlaySfx", 0x8005D100: "StageRequest", 0x8006B3F0: "EffectBigBurst",
    0x80097A60: "StageGoalReached", 0x80095244: "HudPanelA_Start",
    0x8008C064: "HeapFree", 0x8004B590: "CameraAreaScriptDispatch",
    0x8007AC64: "PlayerSelectGadget", 0x80032ADC: "PlayerIdleModelSetup",
    0x8007FFE0: "PlayerModeCallback",
}
HOOK_ARGC = {
    "AnimFramePtr": 2, "DR_MOVE": 4, "PadRumble": 3, "AimRequestClear": 0,
    "PlayerInput_StickGesture": 0, "Gadget3AimUpdateAmmo": 0, "AimModeEnter": 0,
    "AimModeLeave": 0, "PlayPlayerVoice": 1, "EffectGroundImpactAtPlayer": 0,
    "PlayerDamage": 1, "PlaySfx": 1, "StageRequest": 3, "EffectBigBurst": 3,
    "StageGoalReached": 0, "HudPanelA_Start": 1, "HeapFree": 1,
    "CameraAreaScriptDispatch": 3, "PlayerSelectGadget": 2, "PlayerIdleModelSetup": 1,
    "PlayerModeCallback": 2, "PlayerCond_JumpHeld": 0,
}
RETURN_HOOKS = {"AnimFramePtr", "PlayerInput_StickGesture", "StageGoalReached"}


def u32(v): return int(v) & M32
def s32(v):
    v = u32(v)
    return v if v < 0x80000000 else v - 0x100000000


class Model:
    def __init__(self, seed, case_no):
        self.seed, self.case_no, self.log, self.calls = seed, case_no, [], {}

    def invoke(self, name, args):
        args = tuple(u32(x) for x in args)
        self.log.append((name, args))
        n = self.calls.get(name, 0)
        self.calls[name] = n + 1
        if name == "AnimFramePtr":
            return u32(0x80120000 + (s32(args[1]) & 0x3FFF) * 4)
        if name == "PlayerInput_StickGesture":
            return (0, 0x10, 0x20, 0x30)[self.case_no & 3]
        if name == "StageGoalReached":
            return (self.case_no >> 1) & 1
        if name == "PlayerCond_JumpHeld":
            return self.case_no & 1
        return None


P, U32, I32, VP = C.c_void_p, C.c_uint32, C.c_int32, C.c_void_p
CB = C.CFUNCTYPE


class Hooks(C.Structure):
    _fields_ = [
        ("user", VP),
        ("AimRequestClear", CB(None, VP, VP)),
        ("PlayerCond_JumpHeld", CB(I32, VP, VP)),
        ("PlayerInput_StickGesture", CB(I32, VP, VP)),
        ("PadRumble", CB(None, VP, VP, I32, I32, I32)),
        ("Gadget3AimUpdateAmmo", CB(None, VP, VP)),
        ("AimModeEnter", CB(None, VP, VP)),
        ("AimModeLeave", CB(None, VP, VP)),
        ("PlayPlayerVoice", CB(None, VP, VP, I32)),
        ("EffectGroundImpactAtPlayer", CB(None, VP, VP)),
        ("PlayerDamage", CB(None, VP, VP, I32)),
        ("PlaySfx", CB(None, VP, VP, I32)),
        ("StageRequest", CB(None, VP, VP, I32, I32, I32)),
        ("EffectBigBurst", CB(None, VP, VP, I32, I32, I32)),
        ("StageGoalReached", CB(I32, VP, VP)),
        ("HudPanelA_Start", CB(None, VP, VP, I32)),
        ("HeapFree", CB(None, VP, VP, U32)),
        ("CameraAreaScriptDispatch", CB(None, VP, VP, I32, I32, U32)),
        ("PlayerSelectGadget", CB(None, VP, VP, I32, I32)),
        ("PlayerIdleModelSetup", CB(None, VP, VP, U32)),
        ("AnimFramePtr", CB(U32, VP, VP, U32, I32)),
        ("DR_MOVE", CB(None, VP, VP, U32, U32, I32, I32)),
        ("PlayerModeCallback", CB(None, VP, VP, U32, I32)),
        ("incoming_a1", U32), ("incoming_a2", U32), ("incoming_a3", U32),
    ]


class CSide:
    def __init__(self, dll):
        self.lib = C.CDLL(str(dll))
        lib = self.lib
        lib.ae_ram_create.restype = P
        lib.ae_ram_destroy.argtypes = [P]
        lib.ae_ram_clear_error.argtypes = [P]
        lib.ae_ram_failed.argtypes = [P]
        lib.ae_ram_failed.restype = I32
        lib.ae_ram_error.argtypes = [P]
        lib.ae_ram_error.restype = C.c_char_p
        lib.ae_ptr.argtypes = [P, U32, U32]
        lib.ae_ptr.restype = P
        self.ram = lib.ae_ram_create()
        if not self.ram:
            raise RuntimeError("ae_ram_create failed")
        self.main = (C.c_uint8 * 0x200000).from_address(lib.ae_ptr(self.ram, 0x80000000, 0x200000))
        self.scratch = (C.c_uint8 * 0x400).from_address(lib.ae_ptr(self.ram, 0x1F800000, 0x400))
        self.model = None
        self.hooks = Hooks()
        self.keepalive = []

        void = {"AimRequestClear", "PadRumble", "Gadget3AimUpdateAmmo", "AimModeEnter", "AimModeLeave",
                "PlayPlayerVoice", "EffectGroundImpactAtPlayer", "PlayerDamage", "PlaySfx", "StageRequest",
                "EffectBigBurst", "HudPanelA_Start", "HeapFree", "CameraAreaScriptDispatch", "PlayerSelectGadget",
                "PlayerIdleModelSetup", "DR_MOVE", "PlayerModeCallback"}
        signatures = {
            "AimRequestClear": (None, VP, VP), "PlayerCond_JumpHeld": (I32, VP, VP),
            "PlayerInput_StickGesture": (I32, VP, VP), "PadRumble": (None, VP, VP, I32, I32, I32),
            "Gadget3AimUpdateAmmo": (None, VP, VP), "AimModeEnter": (None, VP, VP),
            "AimModeLeave": (None, VP, VP), "PlayPlayerVoice": (None, VP, VP, I32),
            "EffectGroundImpactAtPlayer": (None, VP, VP), "PlayerDamage": (None, VP, VP, I32),
            "PlaySfx": (None, VP, VP, I32), "StageRequest": (None, VP, VP, I32, I32, I32),
            "EffectBigBurst": (None, VP, VP, I32, I32, I32), "StageGoalReached": (I32, VP, VP),
            "HudPanelA_Start": (None, VP, VP, I32), "HeapFree": (None, VP, VP, U32),
            "CameraAreaScriptDispatch": (None, VP, VP, I32, I32, U32),
            "PlayerSelectGadget": (None, VP, VP, I32, I32), "PlayerIdleModelSetup": (None, VP, VP, U32),
            "AnimFramePtr": (U32, VP, VP, U32, I32), "DR_MOVE": (None, VP, VP, U32, U32, I32, I32),
            "PlayerModeCallback": (None, VP, VP, U32, I32),
        }
        for name, sig in signatures.items():
            restype, *argtypes = sig
            def callback(_user, _ram, *args, _name=name):
                result = self.model.invoke(_name, args)
                return 0 if result is None else result
            cb = CB(restype, *argtypes)(callback)
            self.keepalive.append(cb)
            setattr(self.hooks, name, cb)
        def jump_held(_user, _ram):
            self.model.log.append(("PlayerCond_JumpHeld", ()))
            return self.model.case_no & 1
        cb = CB(I32, VP, VP)(jump_held)
        self.keepalive.append(cb)
        self.hooks.PlayerCond_JumpHeld = cb
        self.hooks.incoming_a1 = 0x13572468
        self.hooks.incoming_a2 = 0x24681357
        self.hooks.incoming_a3 = 0x35792468
        for name, (_, _, cname, has_hooks, kinds) in FUNCS.items():
            fn = getattr(lib, cname)
            fn.argtypes = [P] + ([P] if has_hooks else []) + [I32 if k == "s" else U32 for k in kinds]
            fn.restype = None

    def load(self, snapshot):
        C.memmove(self.main, snapshot[0], len(snapshot[0]))
        C.memmove(self.scratch, snapshot[1], len(snapshot[1]))
        self.lib.ae_ram_clear_error(self.ram)

    def call(self, name, args):
        _, _, cname, has_hooks, kinds = FUNCS[name]
        vals = [s32(a) if k == "s" else u32(a) for a, k in zip(args, kinds)]
        fn = getattr(self.lib, cname)
        if has_hooks:
            fn(self.ram, C.byref(self.hooks), *vals)
        else:
            fn(self.ram, *vals)

    def snapshot(self):
        return bytes(self.main), bytes(self.scratch)


def build_dll(mutation=False):
    OUT.mkdir(parents=True, exist_ok=True)
    dll = OUT / f"playeract_{os.getpid()}_{'mut' if mutation else 'base'}.dll"
    sources = [ROOT / "src/core/ae_ram.c", ROOT / "src/core/ae_sdk.c", ROOT / "src/player/native_player.c",
               ROOT / "src/player/ae_player_act.c", ROOT / "src/player/ae_player_act_state.c",
               ROOT / "src/player/ae_player_act_land.c", ROOT / "src/player/ae_player_act_mode.c"]
    if mutation:
        generated = []
        for source in sources[3:]:
            content = source.read_text(encoding="utf-8")
            for index, (_, _, cname, _, _) in enumerate(FUNCS.values(), 1):
                m = __import__("re").search(r"\bvoid\s+" + __import__("re").escape(cname) + r"\s*\([^)]*\)\s*\{", content)
                if not m:
                    continue
                injection = (f"\n    if (ae_player_act_mutation == {2 * index - 1}) "
                             f"ae_wr32(r, 0x800FEFF0u, 0xBAD00000u + {2 * index - 1}u);\n"
                             f"    if (ae_player_act_mutation == {2 * index}) "
                             f"ae_wr32(r, 0x800FEFF4u, 0xBAD00000u + {2 * index}u);\n")
                content = content[:m.end()] + injection + content[m.end():]
            tmp = OUT / f"{source.stem}_mut_{os.getpid()}.c"
            tmp.write_text("extern int ae_player_act_mutation;\n" + content, encoding="utf-8")
            generated.append(tmp)
        mutation_var = OUT / f"playeract_mutation_{os.getpid()}.c"
        mutation_var.write_text("int ae_player_act_mutation = 0;\n", encoding="utf-8")
        sources = sources[:3] + generated + [mutation_var]
    gcc = shutil.which("gcc")
    if not gcc:
        raise RuntimeError("gcc is required for the player action checker")
    subprocess.run([gcc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-shared", "-static-libgcc",
                    "-I", str(ROOT / "src/player"), "-I", str(ROOT / "src/core"),
                    *map(str, sources), "-o", str(dll), "-lm"], check=True)
    return dll


def write_player_tables(cpu):
    blob = (ROOT / "extract/members/008.bin").read_bytes()
    cpu.load_area(blob, addr=0x80103000)
    for i in range(10):
        offsets = [struct.unpack_from("<H", blob, 0x14 + i * 0x10 + k)[0] * 4 for k in (0, 4, 8, 12)]
        for j, offset in enumerate(offsets):
            cpu.write32(STATE_SETS + i * 0x10 + j * 4, 0x80103000 + offset)


def cases_for(name):
    # Each tuple is (arguments, fixture overrides). State/mode-specific cases make both
    # branch outcomes reproducible; the shared randomized fixture varies unrelated RAM.
    if name == "PlayerSetState":
        rows = [(0x24, 0x25, 0x20, 0, 0), (0x24, 0x20, 0, 0, 0), (0, 0x24, 0x10, 0, 0),
                (0, 0x25, 0x10, 0, 0), (0x46, 0x47, 0x10, 0, 0), (0x47, 0x46, 0x10, 0, 0),
                (0x46, 0x46, 0x10, 0, 0), (0x80, 0x81, 0x10, 0, 4), (0x83, 0x84, 0x10, 0, 7),
                (0x18, 0x4B, 0x10, 0, 0), (0x38, 0x3D, 0x10, 0, 0), (0x03, 0x00, 0x20, 2, 2),
                (0x00, 0x00, 0x00, 0, 0), (-1, 0x24, 0x10, 0, 0)]
        return [((new,), {"old_state": old, "old_frame": frame, "state_change": mode, "gadget": gadget})
                for new, old, frame, mode, gadget in rows]
    if name == "PlayerSetMode":
        rows = [(0, req, {}) for req in range(15)]
        rows += [(old, req, {}) for old, req in ((1, 0), (1, 2), (2, 13), (3, 0), (3, 4), (4, 13),
                                                   (5, 6), (6, 5), (7, 13), (8, 0), (8, 9), (8, 2),
                                                   (9, 13), (10, 0), (11, 13), (12, 14))]
        rows += [(8, 0, {"exit_code": 0x20}), (8, 9, {"exit_code": 0x20}),
                 (8, 0, {"goal": 1}),
                 (1, 4, {"actor_state": 0}), (1, 4, {"actor_state": 2}),
                 (3, 4, {"idle_gadget_state": 1}), (10, 2, {"idle_active": 1}),
                 (0, 1, {"area_id": 0x51}), (0, 11, {"gadget": 4, "gadget4_mode": 1}),
                 (0, 14, {"gadget": 4, "gadget4_mode": 1}),
                 (0, 10, {"idle_gadget_state": 1, "idle_active": 1}),
                 (1, 4, {"actor_state": 2, "idle_gadget_state": 1}),
                 (0, 6, {"rope_same_endpoints": 1}),
                 (0, 6, {"rope_degenerate": 1, "expected_trap": "break"}),
                 (12, 0, {})]
        return [((req,), {"current_mode": old, **extra}) for old, req, extra in rows]
    if name == "PlayerAct_Land":
        return [((), {"state": s, "fall_frames": f, "move_y": y})
                for s, f, y in ((0x45, 2, 0), (0x45, 6, 0), (0x24, 2, 0x20),
                                (0x24, 2, 0x21), (0x4B, 6, 0))]
    if name == "PlayerAct_FlyerThrust":
        return [((), {"squash_frame": n, "pulse_frame": p, "pulse_timer": t})
                for n, p, t in ((0, 0, 0), (29, 14, 800), (30, 15, 801), (40, 1, 0))]
    if name == "PlayerAct_ScaleMoveVel":
        return [((PLAYER, 0x800F5120), {"gadget": 4, "gadget4_mode": 1}),
                ((PLAYER, 0x800F5120), {"gadget": 4, "gadget4_mode": 2}),
                ((PLAYER, 0x800F5120), {"gadget": 4, "gadget4_mode": -1}),
                ((PLAYER, 0x800F5120), {"gadget": 1, "gadget4_mode": 0})]
    if name == "PlayerAct_TurnAroundAndJump":
        return [((PLAYER, 0x800F5120), {"gadget": 4, "gadget4_mode": 1}),
                ((PLAYER, 0x800F5120), {"gadget": 0, "gadget4_mode": 0})]
    if name == "PlayerAct_FaceStick":
        return [((), {"stick_mag": n}) for n in (0x40, 0x41, 0xFF)]
    if name == "PlayerAct_FaceStickMaybeHalfSpeed":
        return [((), {"stick_mag": 0x41, "gesture_events": e}) for e in (0, 4)]
    if name == "PlayerAct_FaceStickOnRotation":
        return [((), {"history_event": e}) for e in (0, 0x10, 0x20, 0x40)]
    if name == "PlayerAct_GadgetSpinFromStick":
        return [((), {"spin_active": active, "spin_limit": limit}) for active, limit in ((0, 0), (1, 0), (1, 0x1FFF), (1, 0x2000))]
    if name == "PlayerSetClimbTarget":
        return [((100, 200, -300, 3, 0x120, 0x80), {"current_mode": m}) for m in (5, 0)]
    if name == "PlayerSetRope":
        return [((0x80141000, 0x80141010), {"rope": 1})]
    if name == "PlayerSetEyeFrame":
        # a real packet cursor: with 0 the command-link writes store zeros over zeros and hide a wrong address
        return [((f,), {"eye_index": i, "eye_queue": q}) for i, f, q in
                ((0, 0, 0), (1, 3, 0x800E2340), (0, 5, 0x800E2A18), (1, 7, 0x800E3000))]
    if name == "PlayerSetTintEffect":
        return [((m,), {}) for m in (-1, 0, 1, 2, 3, 4)]
    if name == "PlayerAct_JumpHoldBoost":
        return [((), {"move_y": y}) for y in (-1, 0x100, 0x1000, 0x8000)]
    if name == "PlayerAct_Gadget3Aim":
        return [((), {"gadget": g}) for g in (2, 3, 4)]
    if name == "PlayerAct_JumpFromLStick":
        return [((), {"move_push_state": push, "move_magnitude": mag, "move_dir_x": x,
                      "move_dir_z": z, "gadget": gadget})
                for push, mag, x, z, gadget in ((0, 0, 0, 0, 0), (1, 0, 0x1000, 0, 0),
                                                 (1, 0x60, -0x1000, 0x800, 0),
                                                 (1, 0x7F, 0x1200, -0x800, 4))]
    if name == "PlayerAct_BounceBack":
        return [((), {"current_mode": 0}), ((), {"current_mode": 8, "exit_code": 0x20})]
    if name == "PlayerAct_ExitToMode0Forward":
        return [((), {"current_mode": 0}), ((), {"current_mode": 8, "exit_code": 0x20})]
    if name == "PlayerAct_FaceStickRotateA" or name == "PlayerAct_FaceStickRotateB":
        return [((), {"stick_mag": 0x40}), ((), {"stick_mag": 0x80})]
    if name == "PlayerAct_FaceRightPerp" or name == "PlayerAct_FaceLeftPerp":
        return [((), {"spin_active": a, "spin_limit": b}) for a, b in ((0, 0), (1, 0x1FFF), (1, 0x2000))]
    return [((), {}), ((), {"gadget": 4, "state": 0x46, "move_y": -0x12345})]


def _write_random(cpu, rng, address, count):
    cpu.write_bytes(address, bytes(rng.randrange(256) for _ in range(count)))


def make_fixture(cpu, seed, profile):
    rng = random.Random(seed)
    _write_random(cpu, rng, PLAYER, 0x250)
    _write_random(cpu, rng, PARAMS, 0x80)
    _write_random(cpu, rng, INPUT, 0x240)
    cpu.write_bytes(SCRATCH, bytes(rng.randrange(256) for _ in range(0x400)))
    _write_random(cpu, rng, ACTOR, 0x500)
    cpu.write32(ACTOR + 0x70, ACTOR_MODEL)
    cpu.write32(0x800B4DC4, ACTOR)
    cpu.write32(0x800B4D70, IDLE_ACTOR)
    cpu.write32(0x800B4DD0, 0x8007FFE0)
    cpu.write32(IDLE_ACTOR + 0x80, 0x80143000)
    cpu.write16(0x800E68D0 + 0x46, profile.get("idle_gadget_state", 0))
    cpu.write8(0x800E68D0 + 0x32, profile.get("idle_active", 0))
    cpu.write32(0x80143000, 0x13579BDF)
    cpu.write16(MODE, profile.get("current_mode", 0))
    cpu.write16(PLAYER + 0x5E, profile.get("old_state", profile.get("state", 0x24)))
    cpu.write16(PLAYER + 0x5C, profile.get("old_frame", 0x10))
    cpu.write16(PLAYER + 0xFC, profile.get("fall_frames", 0))
    cpu.write32(PLAYER + 0x124, profile.get("move_y", 0x100))
    cpu.write16(PLAYER + 0x100, profile.get("squash_frame", 0))
    cpu.write8(PLAYER + 0xF2, profile.get("gadget", 0))
    cpu.write8(PLAYER + 0xF3, profile.get("previous_gadget", 1))
    cpu.write32(PLAYER + 0x74, rng.getrandbits(16))
    cpu.write32(PLAYER + 0x78, rng.getrandbits(16))
    cpu.write16(PLAYER + 0x5E, profile.get("old_state", profile.get("state", 0x24)))
    cpu.write16(0x800F5124, profile.get("gadget4_mode", 0))
    cpu.write16(0x800F5148, profile.get("spin_limit", 0))
    cpu.write16(0x800F514A, profile.get("spin_amount", 0x80))
    cpu.write16(0x800F514C, profile.get("spin_delta", 0x20))
    cpu.write16(0x800F5164, profile.get("spin_active", 1))
    cpu.write16(INPUT + 0x182, profile.get("stick_mag", 0x80))
    cpu.write16(INPUT + 0x178, profile.get("move_push_state", 0))
    cpu.write16(SCRATCH + 0x64, profile.get("move_magnitude", 0))
    cpu.write16(INPUT + 0x194, profile.get("move_dir_x", 0x800))
    cpu.write16(INPUT + 0x198, profile.get("move_dir_z", -0x400))
    cpu.write16(INPUT + 0x19C, 0x2345)
    cpu.write16(INPUT + 0x1A0, 0x3456)
    cpu.write32(INPUT + 0x170, 0)
    cpu.write16(INPUT + 0xA4, profile.get("history_event", 0))
    cpu.write16(SCRATCH + 0x62, profile.get("gesture_events", 0))
    cpu.write16(INPUT + 0x1B4, profile.get("pulse_frame", 0))
    cpu.write16(INPUT + 0x1BC, profile.get("pulse_timer", 0))
    cpu.write32(0x800EC354, profile.get("eye_queue", 0))   # ordering-table packet cursor
    cpu.write32(0x800EC35C, profile.get("eye_index", 0))
    cpu.write16(0x800F4350 + 0x124, profile.get("exit_code", 0))
    cpu.write16(0x800F4350 + 0x126, profile.get("area_id", 0x52))
    cpu.write16(0x800F4350 + 0x12A, profile.get("area_argument", 0x1234))
    cpu.write16(0x800F4350 + 0x152, profile.get("area_flags", 0x100))
    cpu.write32(0x800F4350 + 0x100, 0x87654321)
    cpu.write32(0x800FEFF0, 0)
    cpu.write32(0x800FEFF4, 0)
    # Safe bounded parameters, including both positive and negative movement values.
    for offset, value in ((4, -0x180), (8, 0x300), (12, 0x80), (16, 0x280),
                          (20, -0x100), (24, 30), (36, 0x180), (40, 75),
                          (44, 60), (48, 45), (52, 0x100), (56, 0x200),
                          (60, 0x180), (64, 0x80), (68, 0x500)):
        cpu.write32(PARAMS + offset, value)
    # Validity bounds for the two rope endpoints used by PlayerSetMode(6).
    cpu.write16(PLAYER + 0xC0, 0x110)
    cpu.write16(PLAYER + 0xC2, 0x20)
    cpu.write16(PLAYER + 0xC4, 0x220)
    cpu.write16(PLAYER + 0xC6, 0x300)
    cpu.write16(PLAYER + 0xC8, 0x30)
    cpu.write16(PLAYER + 0xCA, 0x180)
    if profile.get("rope_same_endpoints"):
        cpu.write16(PLAYER + 0xC6, 0x110)
        cpu.write16(PLAYER + 0xC8, 0x20)
        cpu.write16(PLAYER + 0xCA, 0x220)
    if profile.get("rope_degenerate"):
        for offset in (0x24, 0x28, 0x2C):
            cpu.write32(PLAYER + offset, 0)
        for offset in (0xC0, 0xC2, 0xC4, 0xC6, 0xC8, 0xCA):
            cpu.write16(PLAYER + offset, 0)
    cpu.write16(0x800F4450, profile.get("request11_word", 0))
    cpu.write16(0x800F44A2, profile.get("request_flags", 0))
    cpu.write32(0x800F4DD0, profile.get("hud_flags", 0xFFFFFFFF))
    cpu.write32(0x800FC26C, profile.get("transition_flags", 0xFFFFFFFF))
    cpu.write32(0x800FC2E0, profile.get("request13_flags", 0xFFFFFFFF))
    cpu.write16(0x800F5174, 0xFFFF)
    cpu.write16(0x800F5120, 0)
    cpu.write8(ACTOR + 0x8E, profile.get("actor_state", 1))
    cpu.write32(ACTOR + 0x24, 0x12340000)
    cpu.write32(ACTOR + 0x28, 0x23450000)
    cpu.write32(ACTOR + 0x2C, 0x34560000)
    cpu.write16(ACTOR + 0x60, 0x1000)
    cpu.write16(ACTOR + 0x62, 0)
    cpu.write16(ACTOR + 0x64, 0x2000)
    cpu.write16(ACTOR_MODEL + 0x18, 0x0123)
    cpu.write32(ACTOR + 0x1C, 0x80140020)
    cpu.write16(IDLE_ACTOR + 0x84, 2)
    cpu.write16(0x800F4476, profile.get("area_id", 0x52))


def hook_map(model):
    result = {}
    for address, name in HOOKS.items():
        def invoke(cpu, regs, _name=name):
            if _name == "PlayerModeCallback":
                args = (0x8007FFE0, regs[4])
            else:
                args = tuple(regs[4:4 + HOOK_ARGC[_name]])
            return model.invoke(_name, args)
        result[address] = invoke
    # PlayerCond_JumpHeld has no original MIPS address in the action range's call map;
    # its documented original call target is separately intercepted here.
    result[0x80064AF8] = lambda cpu, regs: model.invoke("PlayerCond_JumpHeld", ())
    return result


def compare_snapshots(a, b, baseline=None):
    main_a, scratch_a = a
    main_b, scratch_b = b
    if scratch_a != scratch_b:
        for i, (x, y) in enumerate(zip(scratch_a, scratch_b)):
            if x != y:
                base = "" if baseline is None else f" (base {baseline[1][i]:02X}, MIPS {x:02X}, C {y:02X})"
                return f"scratch+0x{i:X}: {x:02X}!={y:02X}{base}"
    if main_a[:STACK_LO] != main_b[:STACK_LO]:
        for i, (x, y) in enumerate(zip(main_a[:STACK_LO], main_b[:STACK_LO])):
            if x != y:
                base = "" if baseline is None else f" (base {baseline[0][i]:02X}, MIPS {x:02X}, C {y:02X})"
                return f"RAM {0x80000000+i:08X}: {x:02X}!={y:02X}{base}"
    if main_a[STACK_HI:] != main_b[STACK_HI:]:
        for i, (x, y) in enumerate(zip(main_a[STACK_HI:], main_b[STACK_HI:])):
            if x != y:
                off = STACK_HI + i
                base = "" if baseline is None else f" (base {baseline[0][off]:02X}, MIPS {x:02X}, C {y:02X})"
                return f"RAM {0x80000000+off:08X}: {x:02X}!={y:02X}{base}"
    return None


def run_one(cpu, side, name, args, profile, seed, case_no, initial_regs=None, mutation_id=0):
    cpu.restore(cpu._playeract_fixture)
    make_fixture(cpu, seed, profile)
    before = cpu.snapshot()
    model_m = Model(seed, case_no)
    cpu.hook_log.clear()
    mips_args = tuple(args)
    if name.startswith("PlayerAct_") and name not in ("PlayerAct_Nop",):
        # PlayerUpdate's action table receives the live player and gadget-4 control block.
        # Several original actions use these caller arguments while other entries use globals.
        mips_args = (PLAYER, 0x800F5120) + mips_args
    expected_trap = profile.get("expected_trap")
    mips_trap = None
    try:
        regs = cpu.call(FUNCS[name][0], mips_args, hooks=hook_map(model_m), initial_regs=initial_regs or {5: 0, 6: 0, 7: 0})
    except mr.MipsError as exc:
        if expected_trap != "break" or "break" not in str(exc):
            return f"{name}: original MIPS fault: {exc}"
        mips_trap = str(exc)
    if expected_trap and mips_trap is None:
        return f"{name}: expected original {expected_trap} trap, but the function returned"
    mips_log = list(model_m.log)
    if [HOOKS.get(row[0], "PlayerModeCallback" if row[0] == 0x8007FFE0 else str(row[0])) for row in cpu.hook_log] != [x[0] for x in mips_log]:
        return f"{name}: hook dispatch order differs: CPU {cpu.hook_log}, model {mips_log}"
    model_c = Model(seed, case_no)
    side.load(before)
    side.model = model_c
    side.hooks.incoming_a1 = u32((initial_regs or {}).get(5, 0))
    side.hooks.incoming_a2 = u32((initial_regs or {}).get(6, 0))
    side.hooks.incoming_a3 = u32((initial_regs or {}).get(7, 0))
    if hasattr(side.lib, "ae_player_act_mutation"):
        C.c_int.in_dll(side.lib, "ae_player_act_mutation").value = mutation_id
    side.call(name, args)
    c_failed = bool(side.lib.ae_ram_failed(side.ram))
    if c_failed and not expected_trap:
        return f"{name}: C RAM error: {side.lib.ae_ram_error(side.ram).decode(errors='replace')}"
    if expected_trap and not c_failed:
        return f"{name}: expected C failure for original trap {mips_trap}, but the C port returned"
    if model_c.log != mips_log:
        return f"{name}: hook arguments/order differ: MIPS {mips_log}, C {model_c.log}"
    mismatch = compare_snapshots(cpu.snapshot(), side.snapshot(), before)
    return f"{name}: RAM differs at {mismatch}" if mismatch else None


def run_mutations(seed):
    dll = build_dll(mutation=True)
    side = CSide(dll)
    cpu = mr.MipsRam.from_extract()
    cpu.attach_gte_oracle()
    area = (ROOT / "extract/members/091.bin").read_bytes()
    cpu.load_area(area)
    write_player_tables(cpu)
    cpu._playeract_fixture = cpu.snapshot()
    caught = 0
    failures = []
    for index, name in enumerate(FUNCS, 1):
        args, profile = cases_for(name)[0]
        for selector in (2 * index - 1, 2 * index):
            regs = {5: 0, 6: 0, 7: 0}
            if name == "PlayerSetMode": regs = {5: 0x13572468, 6: 0x24681357, 7: 0x35792468}
            error = run_one(cpu, side, name, args, profile, seed, index, initial_regs=regs, mutation_id=selector)
            changed_address = "800FEFF0" if selector & 1 else "800FEFF4"
            if error is not None and f"RAM differs at RAM {changed_address}" in error:
                caught += 1
            else:
                failures.append(f"mutation {selector} ({name}) was not detected as intended: {error}")
    print(f"mutations caught: {caught}/84")
    for line in failures[:12]: print("  " + line)
    return bool(failures)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--mutate", action="store_true")
    ap.add_argument("--seed", type=int, default=0x944241)
    args = ap.parse_args()
    if not (ROOT / "extract/disc/SCUS_944.23").exists():
        print("SKIP: extract/disc/SCUS_944.23 not present")
        return 0
    reg.declare("stack_window", f"{mr.STACK_WINDOW[0]:08X}..{mr.STACK_WINDOW[1]:08X}")
    reg.declare("shared_random", "PlayerSetMode caller $a1/$a2/$a3")
    if args.mutate:
        return 1 if run_mutations(args.seed) else 0
    t0 = time.time()
    cpu = mr.MipsRam.from_extract()
    cpu.attach_gte_oracle()
    cpu.coverage = set()
    cpu.load_area((ROOT / "extract/members/091.bin").read_bytes())
    write_player_tables(cpu)
    cpu._playeract_fixture = cpu.snapshot()
    side = CSide(build_dll())
    counts, failures = {}, []
    for name in FUNCS:
        rows = cases_for(name)
        counts[name] = 0
        for case_index, (call_args, profile) in enumerate(rows):
            seed = args.seed + case_index * 101 + FUNCS[name][0]
            initial = {5: 0x800F5120, 6: 0, 7: 0}
            if name == "PlayerAct_Nop": initial = {5: 0, 6: 0, 7: 0}
            if name == "PlayerSetMode":
                initial = {5: 0x13572468, 6: 0x24681357, 7: 0x35792468}
            error = run_one(cpu, side, name, call_args, profile, seed, case_index, initial)
            counts[name] += 1
            if error:
                failures.append(f"case {case_index} {profile}: {error}")
                print("FAIL", failures[-1], flush=True)
        print(f"{name:36s} {counts[name]:3d} cases", flush=True)
    items = [(n, a, size, True) for n, (a, size, *_rest) in FUNCS.items()]
    items += [(n, a, size, False) for n, (a, size) in EXTRA_COVERAGE.items()]
    summary = reg.coverage_summary(items, cpu.coverage)
    if args.coverage:
        reg.print_coverage(summary)
    for name, entry in summary.items():
        if entry["uncovered"]:
            print(f"COVERAGE GAP {name}: {entry['uncovered']}")
    passed = not failures and all(not s["uncovered"] for n, s in summary.items() if s["own"])
    reg.write_results(counts, summary, passed, extra={"mutations_required": 84, "area_member": 91})
    print(f"total cases {sum(counts.values())}; MIPS steps {cpu.steps}; elapsed {time.time()-t0:.1f}s")
    if failures or not passed:
        print(f"FAILED: {len(failures)} differentials, full coverage={all(not s['uncovered'] for n,s in summary.items() if s['own'])}")
        return 1
    print("OK: all player action and mode ports match the original instructions with full instruction coverage")
    return 0


if __name__ == "__main__":
    sys.exit(main())
