"""Differential check of monkey port batch 5c (src/monkeys/ae_monkey_level10.c: MonkeySpawnAll, MonkeyAiStateUpdate, MonkeyThinkActive,
MonkeyUpdateAll; call-graph levels 10 - 12) and of RotMatrix (src/core/ae_sdk.c) against the original MIPS code: for every ported function many
scenarios are built in a byte-exact RAM image (SCUS_944.23 at its real addresses; a real area member for MonkeySpawnAll / MonkeyUpdateAll;
monkeys / nav nodes / MonkeyInfo / animation tables / globals as MonkeySpawnAll leaves them, plus randomised and edge-case states); the original
instructions (GTE through the PCSX-ReARMed oracle) and the C port run on identical images with identical hook behaviour; return value, RAM
outside the stack window, scratchpad and the sequence of hook calls must be identical.  The batch 1..9 functions the code calls run as the C
ports on the C side and as the original instructions on the MIPS side.  Needs extract/disc/SCUS_944.23 and extract/members (skips if missing).
See check_monkey_level8.py / level9.py and monkey_scen5.py.

Usage: uv run python tools/mips/check_monkey_level10.py [--cases PERCENT] [--seed S] [--coverage] [--trapv] [--mutate]
"""
import argparse
import random
import struct
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import monkey_harness as mh  # noqa: E402
import monkey_scen as sc  # noqa: E402
import monkey_scen4 as s4  # noqa: E402
import monkey_scen5 as s5  # noqa: E402
from monkey_harness import W, MONKEYS, MONKEY_SIZE, NAV_BASE  # noqa: E402
from monkey_scen import M32, s32, s16, FRAME, G_TARGET, STATE_TABLE  # noqa: E402
from monkey_scen5 import common, move_fields, anim_cycle, in_sight  # noqa: E402

# name -> (original address, size, return kind: s = int32 / u = uint32 / v = void, takes hooks, argument kinds[, C name])
FUNCS = {
    "MonkeyAiStateUpdate": (0x800412B0, 4644, "v", True, "u"),
    "MonkeySpawnAll": (0x8003958C, 1444, "v", True, ""),
    "MonkeyThinkActive": (0x800424D4, 496, "v", True, ""),
    "MonkeyUpdateAll": (0x80042E18, 572, "v", True, ""),
    "RotMatrix (SDK)": (0x80030BD0, 0x288, "u", False, "uu", "ae_sdk_RotMatrix"),
}
EXTRA_COVERAGE = {
    "MonkeySelectActive (batch 5b)": (0x800426C4, 1188), "MonkeyInit (batch 5a)": (0x8003A414, 2112),
    "MonkeyAi_Patrol (batch 5b)": (0x8003DC7C, 5392), "MonkeyAi_MoveAlerted (batch 5a)": (0x8003C6D4, 1384),
    "MonkeyAi_MoveUnalerted (batch 5a)": (0x8003D9D4, 680), "MonkeyAi_State1E_20 (batch 5a)": (0x8003CD84, 3152),
    "MonkeyAi_State07 (batch 5a)": (0x80040170, 132), "MonkeyAi_IdleActivityEnd (batch 5a)": (0x8003F4B4, 124),
    "MonkeyDecideAction (batch 5a)": (0x800378C0, 72), "MonkeyAi_JumpArc (batch 5b)": (0x8003F530, 828),
    "MonkeyAi_JumpArcPhased (batch 5b)": (0x8003F86C, 728), "MonkeyAi_JumpArcB (batch 5b)": (0x8003FD7C, 688),
    "MonkeyAi_State3D (batch 5b)": (0x8003FB44, 196), "MonkeyAi_IdleActivityStart (batch 4d)": (0x8003F18C, 380),
    "MonkeyAi_IdleActivityLoop (batch 4d)": (0x8003F308, 428), "MonkeyAi_GoToFlaggedNode (batch 4d)": (0x8004002C, 324),
    "MonkeyAi_Fall (batch 4a)": (0x8003FCD8, 0x68), "MonkeyAi_State3E (batch 3b)": (0x8003FC08, 0x38),
    "MonkeyUpdateVisuals (batch 4a)": (0x80042B68, 0x2D0), "MonkeyThinkActive callee NavVisitUpdate (batch 3b)": (0x8004086C, 0x84),
    "MonkeyUpdateTargetDistance (batch 3b)": (0x80039B30, 0x28), "MonkeyUpdateContact (batch 2)": (0x800392F0, 0xF4),
    "MonkeyEnterState_20 (batch 3b)": (0x80035EA4, 0x78), "MonkeyEnterKnockdown (batch 3b)": (0x80035BD0, 0x148),
    "MonkeyEnterState2F (batch 3b)": (0x80035D18, 0xC0), "MonkeyEnterState_34 (batch 3b)": (0x800359B4, 0x3C),
    "MonkeyResumeIdle (batch 3b)": (0x800363A4, 0x40), "MonkeyTargetInSightRange (batch 2)": (0x800361B4, 0x90),
    "MonkeyMoveStep (batch 2)": (0x8003B9C0, 0x6A4), "MonkeyBodyOn (batch 2)": (0x80036B78, 0x44),
    "MonkeyBodyOff (batch 2)": (0x80036B24, 0x54), "MonkeyPuff14 (batch 1)": (0x80039200, 0x44),
    "MonkeyApplyMoveDir (batch 2)": (0x80034834, 0x98), "MonkeyCalcMoveDir (batch 1)": (0x800347D8, 0x44),
    "MonkeyGetWaypointPos (batch 3a)": (0x80040CFC, 0x94), "ActorContactOwnerType (batch 1)": (0x80041280, 0x30),
    "MonkeySetState (batch 3a)": (0x80033220, 216), "MonkeyPickAnim (batch 2)": (0x800331B0, 112),
    "MonkeyAdvanceAnimFrame (batch 2)": (0x800330C8, 0xE8), "MonkeyTurnTowardTarget (batch 2)": (0x8003CC3C, 0xE8),
    "MonkeyDebugMenuInit (batch 1)": (0x80043060, 0x90), "MonkeyDebugMenuInit2 (batch 1)": (0x800430F0, 0x268),
    "MonkeyRestoreActivities (batch 3b)": (0x800332F8, 0x1D0), "MonkeyVoiceQueueReset (batch 1)": (0x80033708, 60),
    "DelayedSfxUpdate (batch 1)": (0x800337C4, 168), "MonkeyStub_ReturnZero (batch 1)": (0x8003A334, 8),
    "ApplyMatrixSV (SDK)": (0x800300B0, 0x28), "VectorNormalSS (SDK)": (0x800301E0, 0x30),
}
AREA_MEMBERS = {0x1: 0x05B, 0x4: 0x064, 0x5: 0x066, 0x2: 0x05E}
MOVE_STATES = s5.MOVE_STATES


def G_TARGET_PTR(m):
    return m.read32(G_TARGET)


def load_area(m, area):
    """loads a real area member at 0x80154000 and points g_MonkeyAreaData (0x800F4590) at its monkey section"""
    blob = (ROOT / f"extract/members/{AREA_MEMBERS[area]:03X}.bin").read_bytes()
    m.load_area(blob)
    ms = struct.unpack_from("<I", blob, 0x4E4)[0]
    W(m).w32(0x800F4590, 0x80154000 + ms)
    return ms


def check_all(h, scale, seed):
    t0 = time.time()
    base_mask = [(MONKEYS + MONKEY_SIZE * k + 0xC8 - 0x80000000, 4) for k in range(8)]   # see check_monkey_level4.py
    init_mask = base_mask + [(MONKEYS + MONKEY_SIZE * k + 0x214 - 0x80000000, 8) for k in range(8)]   # see check_monkey_level8.py
    h.mask = base_mask

    def n(count):
        return max(1, int(count * scale / 100))

    def rng_of(name, i):
        return random.Random(f"{seed}/{name}/{i}")

    def fin(m, c):
        s5.narrow_init_mask(h, m, base_mask, init_mask)
        return c

    def run(name, count, setup=None, argsf=lambda c: (), nh=False, i0=0, **kw):
        for i in range(i0, i0 + n(count)):
            rng = rng_of(name, i)
            h.run(name, f"#{i}", i, (lambda m, rng=rng, i=i: fin(m, setup(m, rng, i))) if setup else (lambda m, rng=rng: sc.combat(m, rng)),
                  (lambda c, rng=rng, i=i: argsf(c, rng, i)) if argsf.__code__.co_argcount == 3 else argsf,
                  null_hooks=(nh and i % 8 == 7), **kw)

    # ------------------------------------------------------------------ RotMatrix (SDK)
    def rot_world(m, rng, i):
        w = W(m)
        for k in range(0x20):
            w.w8(0x801F0100 + k, rng.getrandbits(8))
        for k in range(3):
            w.w16(0x801F0000 + 2 * k, rng.choice([0, 1, -1, 1024, 2048, 4095, 4096, -4096, 0x7FFF, 0x8000, rng.getrandbits(16)]))
        return None
    run("RotMatrix (SDK)", 600, rot_world, argsf=lambda c: (0x801F0000, 0x801F0100))

    # ------------------------------------------------------------------ MonkeyAiStateUpdate
    NODE_BITS = [0x10, 0x400, 0x40, 4, 8, 0x200]

    def ai_world(m, rng, i, state=None, act=None, contact=None):
        # the states the monkeys are re-initialised to by SelectActive (activity 2 / area 0x46) run MonkeyInit, whose UpdateGoalPos reads a
        # leftover position for start nodes with flag 0x40 (+0x214 / +0x218 not compared, see check_monkey_level8.py)
        h.mask = init_mask
        c = common(m, rng)
        w = W(m)
        for k in range(8):
            a = MONKEYS + MONKEY_SIZE * k       # no node dispatch entries that read leftover stack for the other monkeys either
            w.w32(a + 0x228, m.read32(a + 0x228) & ~(0x100 | 0x800 | 0x4) & M32)
        nn = m.read16(0x800B4D18)
        st = state if state is not None else rng.choice(list(range(0x43)) + [rng.randint(0, 255)])
        w.w8(c + 0x78, st)
        move_fields(m, rng, c)
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
                if rng.random() < 0.2:
                    f |= b
            w.w32(a, f)
            w.w16(a + 0x10, rng.choice([rng.randint(0, 3000), rng.getrandbits(16)]) & 0xFFFF)
        # the contact type (top nibble of +0x50 through BodyContactType) and what the contact handling reads
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
        w.w32(c + 0x18, rng.choice([m.read32(0x800F4504), m.read32(0x800F4508), rng.getrandbits(32)]))
        w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x46, 0x35, 0x24, 0x46]))
        w.w16(0x800F4472, rng.choice([0, 0, 0, 1, 3, 6, 8, 2]))
        w.w32(0x800F4450, rng.getrandbits(32))
        w.w32(FRAME, rng.choice([rng.getrandbits(32), rng.randint(0, 1000) * 50, rng.randint(0, 1000) * 50 + 9, rng.randint(0, 1000) * 50 + 10,
                                 rng.randint(0, 1000) * 3, 0]) & M32)
        if rng.random() < 0.3:
            anim_cycle(m, rng, c, rng.choice(["run5", "past", "rand", "start"]))
        # the state code blocks of 5 / 0x41 (+0x92 counter, velocity) need a finished animation; the net catch needs the exit flag (hook)
        return c
    run("MonkeyAiStateUpdate", 4000, ai_world, nh=True, allow_trap=True, argsf=lambda c: (0,))
    ci = 500000
    for st in range(0x43):
        for act in (4, 3, 1):
            rng = rng_of("MonkeyAiStateUpdate-grid", ci)

            def setup(m, rng=rng, st=st, act=act, ci=ci):
                return fin(m, ai_world(m, rng, ci, state=st, act=act))
            h.run("MonkeyAiStateUpdate", f"grid {st:02X} {act}", ci, setup, lambda c: (0,), allow_trap=True)
            ci += 1

    def contact_world(m, rng, i):
        """activity 4 in a non-boss area, no collision-body-off flag: the contact handling (net catch, knockdown, flying monkey states)"""
        c = ai_world(m, rng, i, state=rng.choice([30, 30, 3, 8, 2, 0x1A, 0x35, 9, 1, 0x0C, 0x24, 0x20, 0x1E, 0x22, 0x37, 0, 7, rng.randint(0, 0x42)]),
                     act=4, contact=rng.choice([8, 11, 11, 11, 8, 1, 10, 3]))
        w = W(m)
        w.w16(0x800F4472, rng.choice([0, 1, 2, 4, 5, 7]))
        w.w32(c + 0x248, m.read32(c + 0x248) & ~(1 << 30) & M32)
        w.w32(c + 0x228, m.read32(c + 0x228) & ~0x80000 & M32)
        w.w8(c + 0xE2, 0)
        w.w32(c + 0x230, rng.choice([239, 240, 241, 300]))
        return c
    run("MonkeyAiStateUpdate", 2500, contact_world, nh=True, allow_trap=True, i0=20000, argsf=lambda c: (0,))

    def st33_world(m, rng, i):
        c = ai_world(m, rng, i, state=rng.choice([0x33, 0x33, 0x32, 0x34]), act=4)
        w = W(m)
        w.w16(c + 0x242, rng.choice([7, 7, 7, 6, 8]))
        w.w32(0x800B4D10, s4.seed_for(rng, lambda v: v % 10 == 0 if rng.random() < 0.8 else v % 10 != 0))
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        w.w16(info + 0x1A, rng.choice([0, 1, 100, 1000]))
        w.w32(c + 0x20C, rng.choice([0, 100, 299, 300, 301, 3000, 3001, 5000]))
        w.w16(c + 0x246, rng.choice([1, 2, 5, 100]))
        w.w16(c + 0x244, rng.choice([0, 1, 5, 200]))
        w.w32(c + 0x248, (m.read32(c + 0x248) & ~0xFF) | rng.choice([0, 1, 2, 3, 4, 99]))
        anim_cycle(m, rng, c, rng.choice(["past", "run5"]))
        if rng.random() < 0.6:
            # the notice test at the end of the update: alerted 0, the target within the sight range of the MonkeyInfo
            w.w8(c + 0x79, 0)
            w.w32(G_TARGET_PTR(m) + 0x70, rng.choice([0, 1, 0x10, 0x11, 2, 3]))
            w.w16(info + 0xE, rng.choice([5000, 5000, 0, 100]))
            w.w16(info + 0x1A, 5000)
            w.w32(c + 0x20C, rng.choice([0, 100, 4000]))
            w.w16(0x800F4472, rng.choice([0, 1, 2]))
            w.w32(c + 0x248, m.read32(c + 0x248) & ~(1 << 19) & M32)
            w.w16(c + 0x246, 100)
            w.w16(c + 0x244, 0)
            w.w32(c + 0x248, (m.read32(c + 0x248) & ~0xFF))
        return c
    run("MonkeyAiStateUpdate", 700, st33_world, nh=True, allow_trap=True, i0=30000, argsf=lambda c: (0,))

    def idle6_world(m, rng, i):
        c = ai_world(m, rng, i, state=rng.choice([6, 0x40, 0x3F, 0x34, 0x25, 0x36]), act=rng.choice([4, 4, 3, 1]))
        w = W(m)
        w.w8(c + 0x79, rng.choice([0, 0, 1]))
        w.w32(c + 0x228, m.read32(c + 0x228) & ~0x8000 & M32 | (0x8000 if rng.random() < 0.2 else 0))
        w.w16(c + 0x92, rng.choice([0, 1, 2, 5]))
        anim_cycle(m, rng, c, rng.choice(["past", "past", "run5"]))
        return c
    run("MonkeyAiStateUpdate", 600, idle6_world, nh=True, allow_trap=True, i0=40000, argsf=lambda c: (0,))

    def fly_world(m, rng, i):
        c = ai_world(m, rng, i, state=rng.choice([5, 0x41, 5, 0x41, 0x25]), act=4)
        w = W(m)
        for off in (0x70, 0x72, 0x74):
            w.w16(c + off, rng.choice([0, 100, -100, 1000, -1000, rng.getrandbits(16)]) & 0xFFFF)
        w.w16(c + 0x166, rng.choice([0, 1, 100, 200, -200, 0x1000, rng.getrandbits(16)]) & 0xFFFF)
        w.w16(c + 0x92, rng.choice([0, 1, 5, 19, 20, 21, 40, -3, rng.getrandbits(16)]) & 0xFFFF)
        anim_cycle(m, rng, c, rng.choice(["run5", "run5", "past"]))
        return c
    run("MonkeyAiStateUpdate", 600, fly_world, nh=True, allow_trap=True, i0=50000, argsf=lambda c: (0,))

    def boss26_world(m, rng, i):
        c = ai_world(m, rng, i, state=rng.choice([26, 26, 0x35, 3]), act=4)
        w = W(m)
        w.w16(0x800F4472, rng.choice([3, 6, 8, 0]))
        w.w32(c + 0x228, (m.read32(c + 0x228) | 2) & ~0x20020 & M32 | (rng.choice([0, 0x20, 0x20000])))
        nn = m.read16(0x800B4D18)
        for node in (m.read16(c + 0xF2) % nn, m.read16(c + 0xF4) % nn):
            a = NAV_BASE + 0x18 * node
            w.w32(a, m.read32(a) & ~(0x400 | 0x40) & M32)
        w.w32(c + 0x248, m.read32(c + 0x248) & ~(1 << 25) & M32)
        return c
    run("MonkeyAiStateUpdate", 500, boss26_world, nh=True, allow_trap=True, i0=60000, argsf=lambda c: (0,))

    def contact2_world(m, rng, i):
        c = ai_world(m, rng, i, state=rng.choice([9, 0xC, 0x1A, 0x24, 0x35, 9, 0xC, 0x1A, 0x24, 0x35, 3, 8, 2, 1, 0x22, 0x37, 0x26, 0x32, 0x33, 0xB]), act=4, contact=11)
        w = W(m)
        w.w16(0x800F4472, rng.choice([0, 1, 2, 4]))
        w.w32(c + 0x248, m.read32(c + 0x248) & ~(1 << 30) & M32)
        w.w32(c + 0x228, (m.read32(c + 0x228) & ~0x80000 & M32) | (0x8000 if rng.random() < 0.7 else 0))
        w.w8(c + 0xE2, 0)
        w.w16(c + 0x126, 11)
        w.w32(c + 0x230, rng.choice([239, 240, 300]))
        idx = rng.randint(1, 6)
        w.w32(c + 0x50, (11 << 28) | (rng.getrandbits(16) << 12) | idx)
        for q in range(1, 7):
            owner = 0x801F0400 + 0x40 * q
            w.w32(0x800BBDA0 + 20 * q + 4, owner)
            w.w16(owner + 0x24, rng.choice([0x20, 0x30, 0x10, 0x00, 0x20, 0x30, rng.getrandbits(16)]))
        w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x24]))
        return c
    run("MonkeyAiStateUpdate", 1200, contact2_world, nh=True, allow_trap=True, i0=70000, argsf=lambda c: (0,))

    # ------------------------------------------------------------------ MonkeyThinkActive
    def think_world(m, rng, i):
        h.mask = base_mask
        c = common(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        w.w16(0x800F44D4, rng.choice([8, 8, 4, 3, 1, 0, -1]) & 0xFFFF)
        w.w16(0x800AEB66, rng.choice([4, 4, 3, 2, 1, 0, 0, 5, -1]) & 0xFFFF)
        for k in range(4):
            w.w32(0x800E54F0 + 4 * k, rng.choice([0] + [MONKEYS + MONKEY_SIZE * q for q in range(8)]))
        for k in range(8):
            a = MONKEYS + MONKEY_SIZE * k
            w.w8(a + 0x7A, rng.choice([4, 4, 3, 2, 1, 0]))
            w.w16(a + 0xEA, rng.choice([0, 100, 2699, 2700, 2701, 0xFFFF, rng.getrandbits(16)]))
            w.w8(a + 0x7B, rng.randint(0, 7))
            w.w8(a + 0x78, rng.choice(s5.PATROL_STATES + MOVE_STATES + [rng.randint(0, 0x42)]))
            w.w32(a + 0x228, m.read32(a + 0x228) & ~(0x100 | 0x800 | 0x4) & M32)
        w.w8(0x800EC2D2, rng.choice([7, 7, 0, 3, 0xFF]))
        for off in (0x24, 0x28, 0x2C):
            w.w32(0x800F5A20 + off, rng.choice([rng.randint(-3000, 3000), rng.getrandbits(32)]) & M32)
            w.w32(0x800EC1E0 + off, rng.choice([rng.randint(-3000, 3000), rng.getrandbits(32)]) & M32)
        w.w32(FRAME, rng.choice([rng.getrandbits(32), rng.randint(0, 100)]))
        w.w16(0x800F4472, rng.choice([0, 0, 0, 1, 3, 6, 8]))
        # the slot test (frame % active max == slot % active max) hits sometimes: the slot byte of each monkey = frame % max
        mx = s16(m.read16(0x800AEB66))
        if mx > 0 and rng.random() < 0.7:
            fr = m.read32(FRAME)
            for k in range(8):
                a = MONKEYS + MONKEY_SIZE * k
                w.w8(a + 0x7B, (fr % mx) if rng.random() < 0.7 else rng.randint(0, 7))
        w.w32(c + 0x248, m.read32(c + 0x248) & ~(1 << 20) & M32)
        return c
    run("MonkeyThinkActive", 1500, think_world, nh=True, allow_trap=True)

    # ------------------------------------------------------------------ MonkeySpawnAll
    def spawn_world(m, rng, i, area=None):
        h.mask = init_mask
        c = common(m, rng)
        w = W(m)
        area = area if area is not None else rng.choice(list(AREA_MEMBERS))
        load_area(m, area)
        w.w32(0x800A8A58, rng.choice([0, 0, 0, 1]))
        w.w16(0x800F4472, rng.choice([0, 0, 0, 1, 3, 6, 8]))
        w.w16(0x800F4476, area)
        w.w8(0x800B4CF4, rng.choice([0, 1]))
        w.w32(0x800B4D10, rng.getrandbits(16))
        w.w8(0x800F447C, rng.choice([3, 0]))
        k = rng.random()
        if k < 0.08:                              # a wrong header word: the monkey system is switched off
            ms = m.read32(0x800F4590)
            w.w32(ms, rng.choice([0, 0x10, 0x12, rng.getrandbits(32)]))
        elif k < 0.2:                             # too many active monkeys
            ms = m.read32(0x800F4590)
            w.w32(ms + 0x28, rng.choice([5, 6, 0x7FFF, 0xFFFF, 0, 4]))
        elif k < 0.3:                             # no route section (offset 0)
            ms = m.read32(0x800F4590)
            w.w32(ms + 4, 0)
        return c
    run("MonkeySpawnAll", 400, spawn_world, nh=True, allow_trap=True)

    # ------------------------------------------------------------------ MonkeyUpdateAll
    def update_world(m, rng, i):
        """a real area after the original MonkeySpawnAll (run in the setup with the same hook model), then a few frames of state changes"""
        h.mask = init_mask
        c = common(m, rng)
        w = W(m)
        area = rng.choice(list(AREA_MEMBERS))
        load_area(m, area)
        w.w16(0x800F4476, area)
        w.w16(0x800F4472, rng.choice([0, 0, 0, 1, 2, 3, 6, 8]))
        w.w32(0x800A8A58, 0)
        w.w32(0x800B4D10, rng.getrandbits(16))
        w.w8(0x800F447C, 0)
        mm = mh.Model(i, mh.MipsMem(m))
        try:
            m.call(0x8003958C, (), hooks=h.mips_hooks(mm))
        except mh.mr.MipsError:
            pass                                   # the random world made the original fault: the case runs on what it left
        # randomise the run time state of the monkeys
        cnt = s16(m.read16(0x800AEB68))
        for k in range(8):
            a = MONKEYS + MONKEY_SIZE * k
            if k < cnt:
                w.w8(a + 0x7A, rng.choice([4, 4, 3, 2, 1, 0]))
                w.w8(a + 0x78, rng.choice(s5.PATROL_STATES + MOVE_STATES + [64, 5, 65, 37, 6] * 2 + [rng.randint(0, 0x42)]))
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
        return c
    run("MonkeyUpdateAll", 800, update_world, nh=True, allow_trap=True)

    print(f"scenario families done in {time.time() - t0:.1f}s")


MUTATIONS = [
    ("Ai 174", "WR16(monkey + AE_MK_TIMER_174, timer - 1u);", "WR16(monkey + AE_MK_TIMER_174, timer - 2u);"),
    ("Ai bit20", "WR32(monkey + AE_MK_FLAGS, RD32(monkey + AE_MK_FLAGS) & 0xFFEFFFFFu);", "WR32(monkey + AE_MK_FLAGS, RD32(monkey + AE_MK_FLAGS) & 0xFFDFFFFFu);"),
    ("Ai s1", "uint32_t body_flag_before = (STATE_FLAGS(state) >> 5) & 1u;", "uint32_t body_flag_before = (STATE_FLAGS(state) >> 4) & 1u;"),
    ("Ai act0", "if (activity == 0 || activity == 2) return;", "if (activity == 1 || activity == 2) return;"),
    ("Ai 94", "if (!keeps_cooldown) WR16(cur_monkey(r) + AE_MK_ACTION_COOLDOWN, 0);", "if (!keeps_cooldown) WR16(cur_monkey(r) + AE_MK_ACTION_COOLDOWN, 1);"),
    ("Ai 66", "state != 0 && state != 66 &&", "state != 0 && state != 65 &&"),
    ("Ai bit25", "((RD32(monkey + AE_MK_FLAGS) >> 25) & 1u) == 0 && state != 7", "((RD32(monkey + AE_MK_FLAGS) >> 26) & 1u) == 0 && state != 7"),
    ("Ai FDFF", "RD32(monkey + AE_MK_FLAGS) & 0xFDFFFFFFu);", "RD32(monkey + AE_MK_FLAGS) & 0xFBFFFFFFu);"),
    ("Ai act3", "if (RD8(monkey + AE_MK_ACTIVITY) == 3) {\n        WR16(monkey + AE_MK_TIMER_90", "if (RD8(monkey + AE_MK_ACTIVITY) == 4) {\n        WR16(monkey + AE_MK_TIMER_90"),
    ("Ai adv", "AE_MK_AI_STATE)) >> 2) & 1u) ae_MonkeyAdvanceAnimFrame(r);", "AE_MK_AI_STATE)) >> 3) & 1u) ae_MonkeyAdvanceAnimFrame(r);"),
    ("Ai 67", "if (state < 67u) ai_run_state", "if (state < 66u) ai_run_state"),
    ("Ai alerted", "if (RD8(cur_monkey(r) + AE_MK_ALERTED) != 1) ae_MonkeyAi_MoveUnalerted", "if (RD8(cur_monkey(r) + AE_MK_ALERTED) != 0) ae_MonkeyAi_MoveUnalerted"),
    ("Ai 33", "WR8(monkey + AE_MK_AI_STATE, 0x33);", "WR8(monkey + AE_MK_AI_STATE, 0x34);"),
    ("Ai 242", "WR16(monkey + AE_MK_ANIM_STATE_FRAME, RD16(monkey + AE_MK_ANIM_STATE_FRAME) + 1u);", "WR16(monkey + AE_MK_ANIM_STATE_FRAME, RD16(monkey + AE_MK_ANIM_STATE_FRAME) + 2u);"),
    ("Ai 7", "RDS16(monkey + AE_MK_ANIM_STATE_FRAME) == 7 &&", "RDS16(monkey + AE_MK_ANIM_STATE_FRAME) == 6 &&"),
    ("Ai 272", "SpawnParticle, 272, sp + 64", "SpawnParticle, 273, sp + 64"),
    ("Ai m32", "RD16(monkey + AE_MK_POS + 4) - 32u", "RD16(monkey + AE_MK_POS + 4) - 31u"),
    ("Ai rand10", "== 7 && (ae_MonkeyRand(r) & 0xFFFFu) % 10u == 0", "== 7 && (ae_MonkeyRand(r) & 0xFFFFu) % 20u == 0"),
    ("Ai x3", "if (dist >= add32(L(range, 1), range)) {", "if (dist >= add32(L(range, 2), range)) {"),
    ("Ai ctr", "WR8(monkey + AE_MK_FLAGS, RD8(monkey + AE_MK_FLAGS) + 1u);", "WR8(monkey + AE_MK_FLAGS, RD8(monkey + AE_MK_FLAGS) + 2u);"),
    ("Ai 246", "if (counter < limit_hi ||", "if (counter <= limit_hi ||"),
    ("Ai 244", "!(RDS16(monkey + 0x244) < limit_hi)", "!(RDS16(monkey + 0x244) <= limit_hi)"),
    ("Ai 34fin", "if (ae_MonkeyAnimFinished(r) != 0) break;\n        ae_MonkeyDecideAction(r, h);\n        ae_MonkeyApplyMoveDir(r);", "if (ae_MonkeyAnimFinished(r) == 0) break;\n        ae_MonkeyDecideAction(r, h);\n        ae_MonkeyApplyMoveDir(r);"),
    ("Ai 400", "sub32(400, mul_lo(t, t)) / 250", "sub32(401, mul_lo(t, t)) / 250"),
    ("Ai s15", "WR16(monkey + AE_MK_VEL + 4, mul_lo(RDS16(monkey + AE_MK_VEL + 4), damp) / 32768);", "WR16(monkey + AE_MK_VEL + 4, mul_lo(RDS16(monkey + AE_MK_VEL + 4), damp) / 16384);"),
    ("Ai s4", "sub32(400, mul_lo(t, t)) / 250", "sub32(400, mul_lo(t, t)) / 500"),
    ("Ai w30", "WR32(monkey + AE_MK_VEL32, mul_lo(", "WR32(monkey + AE_MK_VEL32 + 4, mul_lo("),
    ("Ai 40", "state == 5 ? 5u : 40u", "state == 5 ? 5u : 41u"),
    ("Ai st25", "ae_MonkeySetState(r, 0x25);", "ae_MonkeySetState(r, 0x24);"),
    ("Ai f8000", "if (RD32(monkey + AE_MK_FLAGS2) & 0x8000u) {\n        ae_MonkeyEnterState2F(r, h);\n        return;", "if (RD32(monkey + AE_MK_FLAGS2) & 0x4000u) {\n        ae_MonkeyEnterState2F(r, h);\n        return;"),
    ("Ai st6", "WR8(cur_monkey(r) + AE_MK_AI_STATE, 6);", "WR8(cur_monkey(r) + AE_MK_AI_STATE, 7);"),
    ("Ai a79", "else if (RD8(monkey + AE_MK_ALERTED) == 0) ae_MonkeyResumeIdle(r);", "else if (RD8(monkey + AE_MK_ALERTED) != 0) ae_MonkeyResumeIdle(r);"),
    ("Ai set2", "WR32(monkey + 0x18, RD32(AE_G_ANIM_SET2));", "WR32(monkey + 0x18, RD32(AE_G_ANIM_SET));"),
    ("Ai t50", "if (RD8(monkey + AE_MK_AI_STATE) - 50u < 3u) {", "if (RD8(monkey + AE_MK_AI_STATE) - 49u < 3u) {"),
    ("Ai 22", "ae_MonkeyTurnTowardTarget(r, 0x22);", "ae_MonkeyTurnTowardTarget(r, 0x23);"),
    ("Ai f4000", "int body_off = (RD32(monkey + AE_MK_FLAGS2) & 0x4000u) != 0", "int body_off = (RD32(monkey + AE_MK_FLAGS2) & 0x2000u) != 0"),
    ("Ai n10", "AE_MK_NAV_NODE1))) & 0x10u) != 0 ||", "AE_MK_NAV_NODE1))) & 0x20u) != 0 ||"),
    ("Ai 29", "RD8(monkey + AE_MK_AI_STATE) == 29;", "RD8(monkey + AE_MK_AI_STATE) == 28;"),
    ("Ai bit30a", "uint32_t disabled = (RD32(cur_monkey(r) + AE_MK_FLAGS) >> 30) & 1u;", "uint32_t disabled = (RD32(cur_monkey(r) + AE_MK_FLAGS) >> 29) & 1u;"),
    ("Ai t124", "if (timer_124 > 0) WR16", "if (timer_124 >= 0) WR16"),
    ("Ai pickanim", "if (ae_MonkeyAnimFinished(r) == 0) ae_MonkeyPickAnim(r);\n    ai_update_height", "if (ae_MonkeyAnimFinished(r) != 0) ae_MonkeyPickAnim(r);\n    ai_update_height"),
    ("Ai 47", "state == 30 || state == 32 || state - 47u < 2u", "state == 30 || state == 32 || state - 46u < 2u"),
    ("Ai 20020", "(RD32(cur_monkey(r) + AE_MK_FLAGS2) & 0x20020u) != 0", "(RD32(cur_monkey(r) + AE_MK_FLAGS2) & 0x20021u) != 0"),
    ("Ai 256w", "(uint32_t)y + 256u", "(uint32_t)y + 255u"),
    ("Ai wp26", "RD8(cur_monkey(r) + AE_MK_AI_STATE) == 26) {\n            y = ae_MonkeyGetWaypointPos(r, h, 1);", "RD8(cur_monkey(r) + AE_MK_AI_STATE) == 25) {\n            y = ae_MonkeyGetWaypointPos(r, h, 1);"),
    ("Ai wp0", "y = ae_MonkeyGetWaypointPos(r, h, 0);\n            WR32(cur_monkey(r) + AE_MK_POS + 4, (uint32_t)y + 256u);", "y = ae_MonkeyGetWaypointPos(r, h, 1);\n            WR32(cur_monkey(r) + AE_MK_POS + 4, (uint32_t)y + 256u);"),
    ("Ai m50", "if (RD32(AE_G_FRAME_COUNTER) % 50u < 10u)", "if (RD32(AE_G_FRAME_COUNTER) % 100u < 10u)"),
    ("Ai p10", "if (RD32(AE_G_FRAME_COUNTER) % 50u < 10u)", "if (RD32(AE_G_FRAME_COUNTER) % 50u < 11u)"),
    ("Ai t22C", "WR16(monkey + AE_MK_NODE_WAIT, wait - 1u);", "WR16(monkey + AE_MK_NODE_WAIT, wait - 2u);"),
    ("Ai reg", "((RD32(monkey + AE_MK_FLAGS3) >> 1) & 1u) == 0", "((RD32(monkey + AE_MK_FLAGS3) >> 2) & 1u) == 0"),
    ("Ai unreg", "AE_MK_AI_STATE)) >> 5) & 1u) == 0) ae_MonkeyBodyUnregister", "AE_MK_AI_STATE)) >> 6) & 1u) == 0) ae_MonkeyBodyUnregister"),
    ("Ai e2", "WR8(monkey + AE_MK_STUN_TIMER, stun - 1u);", "WR8(monkey + AE_MK_STUN_TIMER, stun - 2u);"),
    ("Ai act4", "if (RD8(monkey + AE_MK_ACTIVITY) == 4 && ae_AreaIsBossArena(r) == 0", "if (RD8(monkey + AE_MK_ACTIVITY) == 3 && ae_AreaIsBossArena(r) == 0"),
    ("Ai 241", "RDS32(monkey + AE_MK_STATE_TIMER) >= 241", "RDS32(monkey + AE_MK_STATE_TIMER) >= 240"),
    ("Ai 11a", "if (RD16(monkey + AE_MK_KIND) != 11) return;\n        ae_MonkeyEnterState_20(r, h);", "if (RD16(monkey + AE_MK_KIND) != 10) return;\n        ae_MonkeyEnterState_20(r, h);"),
    ("Ai m200", "WR16(monkey + AE_MK_HEALTH, RD16(monkey + AE_MK_HEALTH) - 200u);", "WR16(monkey + AE_MK_HEALTH, RD16(monkey + AE_MK_HEALTH) - 199u);"),
    ("Ai 8", "if (RD16(monkey + AE_MK_KIND) == 8 &&", "if (RD16(monkey + AE_MK_KIND) == 7 &&"),
    ("Ai catch", "WR8(cur_monkey(r) + AE_MK_ACTIVITY, 3);\n        ae_MonkeyBodyUnregister(r, h);", "WR8(cur_monkey(r) + AE_MK_ACTIVITY, 2);\n        ae_MonkeyBodyUnregister(r, h);"),
    ("Ai caught", "WR16(AE_G_CAUGHT_MONKEY, RD8(", "WR16(AE_G_CAUGHT_MONKEY + 2, RD8("),
    ("Ai 80000", "if (flags2 & 0x80000u) return;", "if (flags2 & 0x40000u) return;"),
    ("Ai s40", "ae_MonkeySetState(r, 0x40);", "ae_MonkeySetState(r, 0x41);"),
    ("Ai m400", "damage = 400;", "damage = 399;"),
    ("Ai m300", "knockdown = 64;\n        damage = 300;", "knockdown = 64;\n        damage = 299;"),
    ("Ai m500", "damage = 500;", "damage = 499;"),
    ("Ai a5", "knockdown = 5;", "knockdown = 6;"),
    ("Ai 70", "if (RD16(AE_G_CURRENT_AREA) == 70) {", "if (RD16(AE_G_CURRENT_AREA) == 69) {"),
    ("Ai ge", "GimmickEvent, 10, 0, 0, 0", "GimmickEvent, 11, 0, 0, 0"),
    ("Ai t90", "if (RD16(monkey + AE_MK_TIMER_90) == 0) {", "if (RD16(monkey + AE_MK_TIMER_90) != 0) {"),
    ("Ai 41", "RD8(cur_monkey(r) + AE_MK_AI_STATE) == 41 ||", "RD8(cur_monkey(r) + AE_MK_AI_STATE) == 42 ||"),
    ("Ai rot", "ae_sdk_RotMatrix(r, cur_monkey(r) + AE_MK_ROT, matrix);", "ae_sdk_RotMatrix(r, cur_monkey(r) + AE_MK_ROT + 2, matrix);"),
    ("Ai copy", "WR32(monkey + AE_MK_PREV_DIR, dir_lo);", "WR32(monkey + AE_MK_PREV_DIR - 4, dir_lo);"),
    ("Ai f24A", "(RD16(monkey + AE_MK_FLAGS_HI) & 3u) - 1u < 2u", "(RD16(monkey + AE_MK_FLAGS_HI) & 7u) - 1u < 2u"),
    ("Ai f0", "WR16(monkey + AE_MK_NOTICE_COUNT, RD16(monkey + AE_MK_NOTICE_COUNT) + 1u);", "WR16(monkey + AE_MK_NOTICE_COUNT, RD16(monkey + AE_MK_NOTICE_COUNT) + 2u);"),
    ("Ai alerted2", "if (RD8(monkey + AE_MK_ALERTED) != 0) return;\n    if (((STATE_FLAGS", "if (RD8(monkey + AE_MK_ALERTED) == 0) return;\n    if (((STATE_FLAGS"),
    ("Ai bit6", "AE_MK_AI_STATE)) >> 6) & 1u) == 0) return;\n    if (ae_MonkeyTargetInSightRange", "AE_MK_AI_STATE)) >> 5) & 1u) == 0) return;\n    if (ae_MonkeyTargetInSightRange"),
    ("Ai 51", "if (state == 51) {\n        ae_MonkeyEnterState_34(r);", "if (state == 52) {\n        ae_MonkeyEnterState_34(r);"),
    ("Ai 52", "if (state == 52) return;", "if (state == 53) return;"),
    ("Ai fff7", "RD32(monkey + AE_MK_FLAGS) & 0xFFF7FFFFu);", "RD32(monkey + AE_MK_FLAGS) & 0xFFEFFFFFu);"),
    ("Ai 5E", "WR16(monkey + AE_MK_ANIM_INDEX, anim_index);", "WR16(monkey + 0x5C, anim_index);"),
    ("Ai ff00", "scale = 0xFFFFFF00u;", "scale = 0xFFFFFF01u;"),
    ("Ai 256b", "uint32_t scale = 256;", "uint32_t scale = 255;"),
    ("Ai e30", "if (state == 30 || state == 32) WR16(monkey + AE_MK_BODY_HEIGHT, 256);", "if (state == 31 || state == 32) WR16(monkey + AE_MK_BODY_HEIGHT, 256);"),
    ("Spawn seed", "vsync * 0x231Fu", "vsync * 0x118Fu"),
    ("Spawn 4930", "0x800A4930u, 4, 0, 0", "0x800A4930u, 5, 0, 0"),
    ("Spawn 2a", "header + 42u", "header + 43u"),
    ("Spawn 11", "if (RD32(header) != 0x11) {", "if (RD32(header) != 0x12) {"),
    ("Spawn 5", "if (RDS16(header + 0x28) >= 5) {", "if (RDS16(header + 0x28) >= 6) {"),
    ("Spawn 1A4", "WR32(AE_G_LOD_MODELS, area_base + RD32(header + 8));", "WR32(AE_G_LOD_MODELS + 4, area_base + RD32(header + 8));"),
    ("Spawn 1160", "WR32(AE_G_NAV_NODES, nav_data + 1160u);", "WR32(AE_G_NAV_NODES, nav_data + 1161u);"),
    ("Spawn d24", "node_bytes / AE_NAV_NODE_SIZE", "node_bytes / 12u"),
    ("Spawn extra", "WR16(AE_G_NAV_EXTRA_COUNT, RD16(nav_data + 4));", "WR16(AE_G_NAV_EXTRA_COUNT, RD16(nav_data + 6));"),
    ("Spawn lt8", "if (!(count < 8)) break;\n    }\n    if (count < 0)", "if (!(count < 7)) break;\n    }\n    if (count < 0)"),
    ("Spawn d6", "WR8(monkey + AE_MK_ROUTE_COUNT, candidates);", "WR8(monkey + AE_MK_ROUTE_COUNT + 1, candidates);"),
    ("Spawn DA", "WR8(monkey + AE_MK_GROUP_SIZES + (uint32_t)group, nodes_in_group);", "WR8(monkey + AE_MK_GROUP_SIZES + 1 + (uint32_t)group, nodes_in_group);"),
    ("Spawn D7", "WR8(monkey + AE_MK_GROUP_COUNT, group);\n                break;", "WR8(monkey + AE_MK_GROUP_COUNT + 1, group);\n                break;"),
    ("Spawn 88", "if (RDS16(RD32(AE_G_ROUTE_TABLE) + group_ofs + 0x88) == -1) {", "if (RDS16(RD32(AE_G_ROUTE_TABLE) + group_ofs + 0x8A) == -1) {"),
    ("Spawn link", "WR32(node + 0xC, link_lists);", "WR32(node + 0x10, link_lists);"),
    ("Spawn initarg", "ae_MonkeyInit(r, h, (uint32_t)s16((uint32_t)i), 0, 0);", "ae_MonkeyInit(r, h, (uint32_t)s16((uint32_t)i), 1, 1);"),
    ("Spawn min", "active_max < monkey_count ? active_max : monkey_count", "active_max < monkey_count ? monkey_count : active_max"),
    ("Spawn seed0", "if (RD32(AE_G_SPAWN_SWITCH) != 0) {", "if (RD32(AE_G_SPAWN_SWITCH) == 0) {"),
    ("Spawn bossseed", "} else if (ae_AreaIsBossArena(r) != 0) {\n        WR32(AE_G_MONKEY_RAND_SEED, 0);", "} else if (ae_AreaIsBossArena(r) == 0) {\n        WR32(AE_G_MONKEY_RAND_SEED, 0);"),
    ("Spawn clear", "AE_MONKEY_SIZE / 4u,", "AE_MONKEY_SIZE / 4u - 1u,"),
    ("Spawn 4CF4", "WR8(AE_G_MONKEY_FLAG_4CF4, 0);\n    if (RDS16(header + 0x28)", "WR8(AE_G_MONKEY_FLAG_4CF4, 1);\n    if (RDS16(header + 0x28)"),
    ("Spawn 1B8", "WR32(AE_G_ANIM_SET2, area_base + RD32(header + 36));", "WR32(AE_G_ANIM_SET2 + 4, area_base + RD32(header + 36));"),
    ("Spawn off", "WR16(AE_G_ACTIVE_MONKEY_MAX, 0);\n        WR8(AE_G_MONKEY_FLAG_4CF4, 1);", "WR16(AE_G_ACTIVE_MONKEY_MAX, 1);\n        WR8(AE_G_MONKEY_FLAG_4CF4, 1);"),
    ("Think 7", "if (RDS8(AE_G_PLAYER + 242) == 7) {", "if (RDS8(AE_G_PLAYER + 242) == 6) {"),
    ("Think car", "if (car_dist < player_dist) target_is_car = 1;", "if (car_dist <= player_dist) target_is_car = 1;"),
    ("Think flag", "WR8(AE_G_NAV_AREA_FLAG, 1);", "WR8(AE_G_NAV_AREA_FLAG, 2);"),
    ("Think 2", "if (RD8(monkey + AE_MK_ACTIVITY) == 2) continue;", "if (RD8(monkey + AE_MK_ACTIVITY) == 3) continue;"),
    ("Think 2701", "if (!(RD16(monkey + AE_MK_GOAL_AGE) < 2701u)) {", "if (!(RD16(monkey + AE_MK_GOAL_AGE) < 2700u)) {"),
    ("Think slot", "rem_trap(r, (int32_t)RD8(monkey + AE_MK_SLOT), ", "rem_trap(r, (int32_t)RD8(monkey + AE_MK_ID), "),
    ("Think boss", "if (ae_AreaIsBossArena(r) == 0) ae_MonkeyNavVisitUpdate(r, h);", "if (ae_AreaIsBossArena(r) != 0) ae_MonkeyNavVisitUpdate(r, h);"),
    ("Think s2", "ae_MonkeyAiStateUpdate(r, h, AE_G_PLAYER);", "ae_MonkeyAiStateUpdate(r, h, 0);"),
    ("Think 4", "for (slot = 0; slot < 4; slot++) {\n        uint32_t monkey = RD32(AE_G_MONKEY_PTRS", "for (slot = 0; slot < 3; slot++) {\n        uint32_t monkey = RD32(AE_G_MONKEY_PTRS"),
    ("UA inc", "WR32(AE_G_FRAME_COUNTER_EB5C, RD32(AE_G_FRAME_COUNTER_EB5C) + 1u);", "WR32(AE_G_FRAME_COUNTER_EB5C, RD32(AE_G_FRAME_COUNTER_EB5C) + 2u);"),
    ("UA boss", "if (ae_AreaIsBossArena(r) != 0) WR32(AE_G_FRAME_COUNTER, RD32(AE_G_FRAME_COUNTER_EB5C));", "if (ae_AreaIsBossArena(r) == 0) WR32(AE_G_FRAME_COUNTER, RD32(AE_G_FRAME_COUNTER_EB5C));"),
    ("UA bit21", "(RD32(monkey + AE_MK_FLAGS) >> 21) & 1u;", "(RD32(monkey + AE_MK_FLAGS) >> 22) & 1u;"),
    ("UA act4", "if (RD8(monkey + AE_MK_ACTIVITY) != 4 || ae_MonkeyIsAttackState(r) == 0) {", "if (RD8(monkey + AE_MK_ACTIVITY) == 4 || ae_MonkeyIsAttackState(r) == 0) {"),
    ("UA 154", "WR32(monkey + AE_MK_RESET_154, 0xFFFFFFFFu);", "WR32(monkey + AE_MK_TIMER_158, 0xFFFFFFFFu);"),
    ("UA 158", "WR16(monkey + AE_MK_TIMER_158, RD16(monkey + AE_MK_TIMER_158) - 1u);", "WR16(monkey + AE_MK_TIMER_158, RD16(monkey + AE_MK_TIMER_158) - 2u);"),
    ("UA 8", "for (i = 0; i < AE_MONKEY_COUNT; i++) {\n        uint32_t monkey = AE_G_MONKEYS + (uint32_t)i * AE_MONKEY_SIZE;\n        uint32_t transient", "for (i = 0; i < AE_MONKEY_COUNT - 1; i++) {\n        uint32_t monkey = AE_G_MONKEYS + (uint32_t)i * AE_MONKEY_SIZE;\n        uint32_t transient"),
    ("UA 4CF4", "if (RD8(AE_G_MONKEY_FLAG_4CF4) != 0) return;", "if (RD8(AE_G_MONKEY_FLAG_4CF4) == 0) return;"),
    ("UA 100", "(RD32(AE_G_GAME_FLAGS_F4450) & 2u) == 0", "(RD32(AE_G_GAME_FLAGS_F4450) & 1u) == 0"),
    ("UA 30", "RD32(AE_G_FRAME_COUNTER) % 30u != 0)) ae_MonkeySelectActive", "RD32(AE_G_FRAME_COUNTER) % 60u != 0)) ae_MonkeySelectActive"),
    ("UA store", "WR16(AE_G_LAST_ACTIVE_MAX, RD16(AE_G_ACTIVE_MONKEY_MAX));", "WR16(AE_G_LAST_ACTIVE_MAX + 2, RD16(AE_G_ACTIVE_MONKEY_MAX));"),
    ("UA cmp", "int unchanged = RDS16(AE_G_LAST_MONKEY_COUNT) == RDS16(", "int unchanged = RDS16(AE_G_LAST_MONKEY_COUNT) != RDS16("),
    ("UA mod", "RD32(AE_G_FRAME_COUNTER) % 30u != 0)) ae_MonkeySelectActive", "RD32(AE_G_FRAME_COUNTER) % 29u != 0)) ae_MonkeySelectActive"),
    ("Rot m10", "ae_wr16(r, m + 10, (uint32_t)sra32(neg32(mul_s(cy, sx)), 12));", "ae_wr16(r, m + 10, (uint32_t)sra32(mul_s(cy, sx), 12));"),
    ("Rot m6", "ae_wr16(r, m + 6, (uint32_t)((uint32_t)t9 - (uint32_t)t6));", "ae_wr16(r, m + 6, (uint32_t)((uint32_t)t9 + (uint32_t)t6));"),
    ("Rot m14", "ae_wr16(r, m + 14, (uint32_t)((uint32_t)t9 - (uint32_t)t7));", "ae_wr16(r, m + 14, (uint32_t)((uint32_t)t9 + (uint32_t)t7));"),
    ("Rot m2", "ae_wr16(r, m + 2, (uint32_t)sra32(neg32(mul_s(sz, cy)), 12));", "ae_wr16(r, m + 2, (uint32_t)sra32(mul_s(sz, cy), 12));"),
    ("Rot m4", "ae_wr16(r, m + 4, (uint32_t)sy);", "ae_wr16(r, m + 4, (uint32_t)ny);"),
]

MUT_FUNCS = {
    "Ai": ["MonkeyAiStateUpdate", "MonkeyThinkActive", "MonkeyUpdateAll"], "Spawn": ["MonkeySpawnAll"], "Think": ["MonkeyThinkActive"],
    "UA": ["MonkeyUpdateAll"], "Rot": ["RotMatrix (SDK)"],
}


def run_mutations(scale, seed, only_labels=(), shard=None):
    rels = ["src/monkeys/ae_monkey_level10.c", "src/core/ae_sdk.c"]
    texts = {rel: (ROOT / rel).read_text(encoding="utf-8") for rel in rels}
    survivors = []
    tmp = Path(tempfile.mkdtemp(prefix="mk10_mut_"))
    h = mh.MonkeyHarness(FUNCS)
    for k, (label, old, new) in enumerate(MUTATIONS):
        if only_labels and not any(s in label for s in only_labels):
            continue
        if shard and k % shard[1] != shard[0]:
            continue
        rel = next((rl for rl in rels if old in texts[rl]), None)
        if rel is None:
            print(f"MUTATION {label!r}: pattern not found")
            survivors.append(label + " (pattern missing)")
            continue
        mp = tmp / f"mut{k}_{Path(rel).name}"
        mp.write_text(texts[rel].replace(old, new, 1), encoding="utf-8")
        try:
            h.reload_c({rel: mp}, tag=f"mk10mut{k}")
        except Exception as e:  # noqa: BLE001
            print(f"mutation {label:34s} BUILD ERROR (invalid mutation): {type(e).__name__}", flush=True)
            survivors.append(label + " (build error)")
            continue
        h.failures = 0
        h.stop_after = 1
        only = MUT_FUNCS.get(label.split()[0])
        if "run" in h.__dict__:
            del h.__dict__["run"]
        if only:
            real_run = h.run
            h.run = lambda name, *a, _real=real_run, _only=only, **kw: _real(name, *a, **kw) if name in _only else None
        try:
            import contextlib, io
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                check_all(h, scale, seed)
            survived = h.failures == 0
        except SystemExit:
            survived = False
        print(f"mutation {label:34s} {'SURVIVED (checker too weak)' if survived else 'caught'}", flush=True)
        if survived:
            survivors.append(label)
    return survivors


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", type=float, default=8, help="scale of the case counts in percent")
    ap.add_argument("--seed", type=int, default=0x9442A1)
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--trapv", action="store_true", help="build the port with -ftrapv (signed overflow aborts)")
    ap.add_argument("--mutate", action="store_true", help="mutation-test the checker (slow)")
    ap.add_argument("--mutate-only", nargs="*", default=[], help="only the mutations whose label contains one of these words")
    ap.add_argument("--stop-after", type=int, default=0)
    ap.add_argument("--shard", default="", help="with --mutate: K/N runs every N-th mutation starting at K (parallel runs)")
    a = ap.parse_args()
    if not (ROOT / "extract/disc/SCUS_944.23").exists() or not (ROOT / "extract/members/05B.bin").exists():
        print("SKIP: extract/disc/SCUS_944.23 or extract/members not present")
        return 0
    if a.mutate:
        shard = tuple(int(x) for x in a.shard.split("/")) if a.shard else None
        survivors = run_mutations(max(a.cases, 100), a.seed, a.mutate_only, shard)   # mutations need the larger case counts
        print("all mutations caught" if not survivors else f"SURVIVORS: {survivors}")
        return 1 if survivors else 0
    t0 = time.time()
    h = mh.MonkeyHarness(FUNCS, with_coverage=True, trapv=a.trapv)
    h.stop_after = a.stop_after or None
    check_all(h, a.cases, a.seed)
    print("cases per function:")
    for k, v in sorted(h.counts.items()):
        print(f"  {k:36s} {v}")
    print(f"original faults / traps matched by a port error flag: {h.traps_agreed}; NULL-hook (fail closed) cases: {h.fail_closed_checked}")
    print(f"total cases {sum(h.counts.values())}; hook calls compared {h.hook_calls}; "
          f"MIPS instructions executed {h.m.steps}; {time.time() - t0:.1f}s")
    h.finish(EXTRA_COVERAGE, default_run=(a.cases == ap.get_default("cases") and a.seed == ap.get_default("seed") and not a.trapv),
             show_coverage=a.coverage)
    if h.failures:
        print(f"FAILED: {h.failures} mismatching cases")
        return 1
    print("OK: the C port matches the original instructions on every case")
    return 0


if __name__ == "__main__":
    sys.exit(main())
