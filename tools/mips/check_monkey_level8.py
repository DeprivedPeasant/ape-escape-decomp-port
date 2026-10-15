"""Differential check of monkey port batch 5a (src/monkeys/ae_monkey_level8.c, call-graph level 8) against the original MIPS
code: for every ported function many scenarios are built in a byte-exact RAM image (SCUS_944.23 at its real addresses; monkeys /
nav nodes / extra link records / MonkeyInfo / animation tables / patrol routes / globals laid out as MonkeySpawnAll and the level
loader leave them, plus randomised and edge-case states); the original instructions (GTE through the PCSX-ReARMed oracle) and the
C port run on identical images with identical hook behaviour; return value, RAM outside the stack window, scratchpad and the sequence
of hook calls must be identical.  The batch 1..7 functions the code calls run as the C ports on the C side and as the original
instructions on the MIPS side.  Needs extract/disc/SCUS_944.23 (skips if missing).  See check_monkey_level7.py and monkey_scen4.py.

Usage: uv run python tools/mips/check_monkey_level8.py [--cases PERCENT] [--seed S] [--coverage] [--trapv] [--mutate]
"""
import argparse
import random
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import monkey_harness as mh  # noqa: E402
import monkey_scen as sc  # noqa: E402
import monkey_scen4 as s4  # noqa: E402
from monkey_harness import W, MONKEYS, MONKEY_SIZE, G_CUR, NAV_BASE, INFO_BASE  # noqa: E402
from monkey_scen import combat, navpos_world, anim_globals, M32, s32, s16, u16, FRAME, G_TARGET, STATE_TABLE  # noqa: E402

# name -> (original address, size, return kind: s = int32 / u = uint32 / v = void, takes hooks, argument kinds[, C name])
FUNCS = {
    "MonkeyDecideAction": (0x800378C0, 72, "v", True, ""),
    "MonkeyAi_IdleActivityEnd": (0x8003F4B4, 124, "v", True, ""),
    "MonkeyAi_State07": (0x80040170, 132, "v", True, ""),
    "MonkeyAi_MoveUnalerted": (0x8003D9D4, 680, "v", True, ""),
    "MonkeyAi_MoveAlerted": (0x8003C6D4, 1384, "v", True, ""),
    "MonkeyAi_State1E_20": (0x8003CD84, 3152, "v", True, ""),
    "MonkeyInit": (0x8003A414, 2112, "v", True, "uuu"),
}
EXTRA_COVERAGE = {
    "MonkeyAlertedDecide (batch 4c)": (0x80037550, 880), "MonkeyPatrolDecide (batch 4d)": (0x80037908, 1124),
    "MonkeySelectNextNode (batch 4d)": (0x8003B074, 1216), "MonkeyChooseNextNodeA (batch 4c)": (0x80038A1C, 128),
    "MonkeyChooseNextNodeB (batch 4c)": (0x80038A9C, 1124), "MonkeyNodeActionDispatch (batch 4b)": (0x80036CD0, 920),
    "MonkeyArriveAtNode (batch 4a)": (0x80040D90, 1264), "MonkeySetNavNode (batch 4a)": (0x80035080, 236),
    "MonkeyUpdateSight (batch 3a)": (0x800405C4, 0x1A8), "MonkeyReachedNode (batch 1)": (0x8003516C, 516),
    "MonkeyAnimFinished (batch 1)": (0x80033A3C, 0x90), "MonkeyAnimAtStart (batch 1)": (0x80033ACC, 0x80),
    "MonkeyStartNotice (batch 3b)": (0x800357E0, 60), "MonkeyEnterIdleWait (batch 3b)": (0x80036300, 164),
    "MonkeyUpdateFacing (batch 3a)": (0x8003C204, 0x364), "MonkeyInterpolateLinkVector (batch 1)": (0x8003C568, 364),
    "MonkeyTurnTowardTarget (batch 2)": (0x8003CC3C, 0xE8), "MonkeyFireProjectileByKind (batch 3a)": (0x8003CD24, 0x60),
    "MonkeyMoveStep (batch 2)": (0x8003B9C0, 0x6A4), "MonkeyPickAttackB (batch 3b)": (0x80037420, 0x98),
    "MonkeyPickAttackC (batch 3b)": (0x800374B8, 0x98), "MonkeyUpdateTargetDistance (batch 3b)": (0x80039B30, 0x28),
    "MonkeySelectLod (batch 2)": (0x8003A33C, 0xD8), "MonkeyDebrisBurst1 (batch 2)": (0x800390F8, 0x150),
    "MonkeyDebrisBurst6 (batch 2)": (0x80038FD8, 0x120), "MonkeyDefeatBurst (batch 2)": (0x80036C84, 0x44),
    "MonkeyEnterState_36 (batch 3b)": (0x80035E6C, 0x38), "MonkeyFootDust (batch 2)": (0x80038F00, 0xD8),
    "MonkeyFireProjectileAimed (batch 2)": (0x8003429C, 0x114), "NavNodeGetPos (batch 2)": (0x80034EF8, 392),
    "MonkeyBodyRegister (batch 1)": (0x80033008, 96), "MonkeySetState (batch 3a)": (0x80033220, 216),
    "MonkeyPickAnim (batch 2)": (0x800331B0, 112), "MonkeyApplyMoveDir (batch 2)": (0x80034834, 0x98),
    "MonkeyCalcMoveDir (batch 1)": (0x800347D8, 0x44), "MonkeyLoadLinkExtra (batch 2)": (0x8003B674, 524),
    "MonkeyEnterState_17 (batch 3a)": (0x8003641C, 0x44), "MonkeyAlertPuff (batch 2)": (0x80039248, 0x54),
    "MonkeyUpdateGoal (batch 3b)": (0x800355DC, 40), "MonkeyPickGoalNode (batch 2)": (0x8003AD9C, 728),
}
DISPATCH_BITS = [0x100, 0x800, 0x20, 0x20000, 8, 4, 0x10, 1, 0x10000, 0x800000, 0x1000000]
MOVE_STATES = [2, 3, 8, 9, 10, 12, 0x19, 0x1A, 0x23, 0x24, 0x35, 0x39, 0, 1, 7, 0xB, 0x1D, 0x3D]
G_FRAME2 = 0x800EC350


def seed_pair(rng, pred):
    """a MonkeyRand seed whose next two values (v1, v2) satisfy pred(v1, v2)"""
    while True:
        seed = rng.getrandbits(16)
        s1 = (seed * 5 + 1) & 0xFFFF
        s2 = (s1 * 5 + 1) & 0xFFFF
        if pred(s1 >> 8, s2 >> 8):
            return seed


def seed_seq(rng, mods):
    """a MonkeyRand seed whose successive pairs of values have a product divisible by the entries of mods (0: any)"""
    while True:
        seed = rng.getrandbits(16)
        x = seed
        ok = True
        for mod in mods:
            x = (x * 5 + 1) & 0xFFFF
            a = x >> 8
            x = (x * 5 + 1) & 0xFFFF
            b = x >> 8
            if mod and (a * b) % mod:
                ok = False
                break
        if ok:
            return seed


def check_all(h, scale, seed):
    t0 = time.time()
    base_mask = [(MONKEYS + MONKEY_SIZE * k + 0xC8 - 0x80000000, 4) for k in range(8)]   # see check_monkey_level4.py
    h.mask = base_mask

    def n(count):
        return max(1, int(count * scale / 100))

    def rng_of(name, i):
        return random.Random(f"{seed}/{name}/{i}")

    def run(name, count, setup=None, argsf=lambda c: (), nh=False, i0=0, **kw):
        for i in range(i0, i0 + n(count)):
            rng = rng_of(name, i)
            h.run(name, f"#{i}", i, (lambda m, rng=rng, i=i: setup(m, rng, i)) if setup else (lambda m, rng=rng: combat(m, rng)),
                  (lambda c, rng=rng, i=i: argsf(c, rng, i)) if argsf.__code__.co_argcount == 3 else argsf,
                  null_hooks=(nh and i % 8 == 7), **kw)

    def common(m, rng, state=None, aboves=(), stale=True):
        """nav world + animation globals + attack / LOS fields; returns the current monkey"""
        h.mask = base_mask
        c = s4.nav_entry_world(m, rng)
        s4.attack_fields(m, rng, c)
        s4.los_fields(m, rng, c)
        w = W(m)
        nn = m.read16(0x800B4D18)
        s4.no_stale_goal(m, rng, c, nn)
        w.w32(c + 0x228, m.read32(c + 0x228) & ~(0x100 | 0x800 | 0x4) & M32 if stale else m.read32(c + 0x228))
        if aboves:
            s4.stale_enter(m, rng, list(aboves))
        if state is not None:
            w.w8(c + 0x78, state)
        return c

    def move_fields(m, rng, c):
        """fields the move handlers read: reach radius / distances (ReachedNode), step vectors, link frame, flags2 bits, state table"""
        w = W(m)
        rad = rng.choice([200, 100, 0, 1, rng.randint(-50, 400)])
        w.w16(c + 0x220, rad & 0xFFFF)
        for off in (0x214, 0x218):
            w.w32(c + off, rng.choice([rad - 1, rad, rad + 1, rad * 2 - 1, rad * 2, rng.randint(0, 600), rng.getrandbits(32)]) & M32)
        for off in (0x70, 0x72, 0x74):
            w.w16(c + off, rng.choice([0, 1, -1, 100, -100, rng.getrandbits(16)]) & 0xFFFF)
        w.w16(c + 0x94, rng.choice([0, 1, 2, -1, 5, rng.getrandbits(16)]) & 0xFFFF)
        w.w16(c + 0x120, rng.choice([0, 1, -1, 5, rng.getrandbits(16)]) & 0xFFFF)
        w.w32(c + 0x228, (m.read32(c + 0x228) & ~2 & M32) | (2 if rng.random() < 0.4 else 0))
        w.w8(c + 0x79, rng.choice([0, 1, 1, 2]))
        for off in range(0, 0x14, 2):
            w.w16(c + 0x9C + off, rng.choice([0x1000, 0, -0x1000, 0x0B50, rng.getrandbits(16)]) & 0xFFFF)
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        w.w16(info + 4, rng.choice([0, 0, 1, -1, 500]) & 0xFFFF)
        w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x43, 0x35, 0x46, 0x24]))
        w.w16(0x800F4472, rng.choice([0, 0, 0, 1, 3, 6, 8, 2]))
        # the state table flags (bit 6: notice, bit 3 ...)
        st = m.read8(c + 0x78)
        flagw = m.read32(STATE_TABLE + 8 * (st % 0x43) + 4)
        if rng.random() < 0.5:
            flagw = (flagw | 0x40) if rng.random() < 0.5 else (flagw & ~0x40 & M32)
            w.w32(STATE_TABLE + 8 * (st % 0x43) + 4, flagw)
        w.w8(c + 0x249, rng.choice([0, 0, 1, 2, 3, 0xFF]))

    def frame_for_third(m, rng, c, p=0.6):
        idv = m.read8(c + 0x7C)
        fr = rng.getrandbits(32)
        if rng.random() < p:
            fr = (fr - (fr + idv) % 3) & M32
        W(m).w32(FRAME, fr)

    # ------------------------------------------------------------------ DecideAction
    def decide_world(m, rng, i):
        c = common(m, rng, aboves=(24 + 32 + 32,))
        w = W(m)
        w.w8(c + 0x79, rng.choice([0, 1, 1, 2, 0xFF]))
        w.w8(c + 0x78, rng.choice(MOVE_STATES + [rng.randint(0, 0x42)]))
        move_fields(m, rng, c)
        w.w8(c + 0x79, rng.choice([0, 1, 1, 2]))
        w.w32(c + 0x248, m.read32(c + 0x248) & ~(1 << 20) & M32 if rng.random() < 0.8 else m.read32(c + 0x248))
        return c
    run("MonkeyDecideAction", 600, decide_world, nh=True, allow_trap=True)

    # ------------------------------------------------------------------ IdleActivityEnd / State07
    def idle_end_world(m, rng, i):
        c = common(m, rng)
        w = W(m)
        w.w8(c + 0x78, rng.choice([0xF, 0x12, 0x15, 0x3C, rng.randint(0, 0x42)]))
        move_fields(m, rng, c)
        kind = rng.choice(["zero", "zero", "nonzero", "any"])
        if kind == "zero":
            w.w32(0x800B4D10, s4.seed_for(rng, lambda v: v % 6 == 0))     # the 16 bit value is the 8 bit one only when < 256: v % 6
        elif kind == "nonzero":
            w.w32(0x800B4D10, s4.seed_for(rng, lambda v: v % 6 != 0))
        else:
            w.w32(0x800B4D10, rng.getrandbits(16))
        return c
    run("MonkeyAi_IdleActivityEnd", 700, idle_end_world, nh=True, allow_trap=True)

    def state07_world(m, rng, i):
        c = common(m, rng, aboves=(24 + 32 + 32,))
        w = W(m)
        w.w8(c + 0x78, rng.choice([7, 7, 7, rng.randint(0, 0x42)]))
        move_fields(m, rng, c)
        return c
    run("MonkeyAi_State07", 700, state07_world, nh=True, allow_trap=True)

    # ------------------------------------------------------------------ MoveUnalerted
    def unalerted_world(m, rng, i):
        c = common(m, rng, aboves=(64 + 32 + 32,))
        w = W(m)
        w.w8(c + 0x78, rng.choice(MOVE_STATES + [rng.randint(0, 0x42)]))
        move_fields(m, rng, c)
        frame_for_third(m, rng, c)
        # sight: distance inside the ranges of the MonkeyInfo (+12 + 2 * type, +0x1A)
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        dist = rng.randint(0, 3000)
        w.w32(c + 0x20C, dist)
        for k in range(8):
            w.w16(info + 12 + 2 * k, rng.choice([dist + 1, dist - 1, dist, 3000, 0, rng.getrandbits(16)]) & 0xFFFF)
        w.w16(info + 0x1A, rng.choice([dist + 1, 0, rng.getrandbits(16)]) & 0xFFFF)
        w.w32(c + 0x21C, rng.choice([0, 600, 1000, 2000, 3000, 4000, rng.randint(0, 4096)]))
        # leave the first position test of the link inside the range
        return c
    run("MonkeyAi_MoveUnalerted", 3500, unalerted_world, nh=True, allow_trap=True)

    def unalerted_reach_world(m, rng, i):
        c = unalerted_world(m, rng, i)
        w = W(m)
        rad = s16(m.read16(c + 0x220))
        w.w32(c + 0x214, (rad - rng.choice([1, 2, 5])) & M32)
        w.w32(c + 0x218, (rad - rng.choice([1, 2, 5])) & M32)
        w.w8(c + 0x78, rng.choice([0x1A, 0x35, 0x1A, 0x35, 2, 3, 0xC, 0x24, 8, 9, 0x39]))
        w.w16(c + 0x94, rng.choice([1, 2, 5]))
        idv = m.read8(c + 0x7C)
        w.w32(FRAME, rng.randint(0, 1000) * 3 + (1 - idv) % 3)      # (frame + id) % 3 == 1: the sight test is skipped
        if rng.random() < 0.4:                  # nodes without links: SelectNextNode / PatrolDecide leave the goal x written above alone
            nn = m.read16(0x800B4D18)
            for q in (m.read16(c + 0xF2), m.read16(c + 0xF4)):
                if rng.random() < 0.8:
                    w.w8(NAV_BASE + 0x18 * (q % nn) + 0xB, 0)
        return c
    run("MonkeyAi_MoveUnalerted", 2500, unalerted_reach_world, nh=True, allow_trap=True, i0=30000)

    # ------------------------------------------------------------------ MoveAlerted
    def alerted_world(m, rng, i):
        c = common(m, rng, aboves=(32 + 32 + 32,))
        w = W(m)
        w.w8(c + 0x78, rng.choice(MOVE_STATES + [rng.randint(0, 0x42), 0x3D]))
        move_fields(m, rng, c)
        w.w8(c + 0x79, 1)
        fr = rng.choice([rng.getrandbits(32), rng.randint(0, 1000) * 300, rng.randint(0, 1000) * 5, rng.randint(0, 1000) * 5 + 1, 0])
        w.w32(FRAME, fr & M32)
        w.w32(c + 0x20C, rng.choice([199, 200, 201, 0, -1, rng.randint(0, 600), rng.getrandbits(32)]) & M32)
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        dist = s32(m.read32(c + 0x20C))
        for k in range(8):
            w.w16(info + 12 + 2 * k, rng.choice([dist + 1, dist - 1, dist, 3000, 0, rng.getrandbits(16)]) & 0xFFFF)
        w.w16(info + 0x1A, rng.choice([dist + 1, 0, rng.getrandbits(16)]) & 0xFFFF)
        w.w32(c + 0x21C, rng.choice([0, 600, 1000, 2000, 3000, 4000, rng.randint(0, 4096)]))
        # animation frame near first/last +/- 10 (the first test) and the frame difference % 5
        idx = sc.anim_index(m, c)
        if 0 <= idx < 128:
            first = m.read16(sc.ANIM_BASE + 8 * idx)
            last = m.read16(sc.ANIM_BASE + 8 * idx + 2)
            w.w16(c + 0x5C, rng.choice([first + 10, first + 10, last, last + 5, last + 1, first + 5, first + 15, first + 9, first, rng.getrandbits(16)]) & 0xFFFF)
        return c
    run("MonkeyAi_MoveAlerted", 4500, alerted_world, nh=True, allow_trap=True)

    def anim_cycle(m, rng, c, mode):
        """the animation of the current state: first frame, 5 * k + extra frames; mode 'run5' puts the frame on first + 5 * j (<= last: the
        animation 'is finished' for MonkeyAnimFinished), 'past' beyond the last frame, 'start' at first + 1"""
        w = W(m)
        st = m.read8(c + 0x78)
        w.w32(STATE_TABLE + 8 * (st % 0x43) + 4, m.read32(STATE_TABLE + 8 * (st % 0x43) + 4) | 4)
        idx = sc.anim_index(m, c)
        if not (0 <= idx < 128):
            return
        first = rng.randint(0, 300)
        k = rng.randint(1, 6)
        last = first + 5 * k + rng.choice([0, 0, 1, 3])
        w.w16(sc.ANIM_BASE + 8 * idx, first)
        w.w16(sc.ANIM_BASE + 8 * idx + 2, last)
        w.w16(c + 0x5C, {"run5": first + 5 * rng.randint(0, k), "past": last + rng.choice([1, 2, 5]), "start": first + 1}[mode])

    def in_sight(m, rng, c):
        """MonkeyUpdateSight returns 1 early for the ground states 3 / 8 when the distance is below one of the ranges"""
        w = W(m)
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        dist = rng.randint(0, 1500)
        w.w32(c + 0x20C, dist)
        for k in range(8):
            w.w16(info + 12 + 2 * k, dist + rng.choice([1, 5, 100]))
        w.w16(info + 0x1A, dist + 1)
        w.w16(0x800F4472, rng.choice([0, 1, 2]))

    def alerted_reach_world(m, rng, i):
        c = alerted_world(m, rng, i)
        w = W(m)
        w.w8(c + 0x78, rng.choice([3, 2, 0x1A, 0x35, 8, 9, 0x24, 0xC, 61, 1, 0, 0x23, rng.randint(0, 0x42)]))
        rad = rng.choice([200, 100, 50])
        w.w16(c + 0x220, rad)
        w.w32(c + 0x214, rad - rng.choice([1, 2, 5]))
        w.w32(c + 0x218, rad - rng.choice([1, 2, 5]))
        nn = m.read16(0x800B4D18)
        node0 = m.read16(c + 0xF2)
        if rng.random() < 0.6:
            w.w8(NAV_BASE + 0x18 * (node0 % nn) + 0xB, 0)
        w.w16(0x800F4472, rng.choice([0, 0, 1, 2, 3, 6, 8]))
        w.w32(c + 0x20C, rng.choice([199, 200, 201, 500, 0, rng.randint(0, 600)]))
        w.w8(c + 0x79, 1)
        return c
    run("MonkeyAi_MoveAlerted", 1500, alerted_reach_world, nh=True, allow_trap=True, i0=20000)

    def alerted_sight_world(m, rng, i):
        c = alerted_world(m, rng, i)
        w = W(m)
        w.w8(c + 0x78, rng.choice([3, 8, 3, 8, 2, 9]))
        in_sight(m, rng, c)
        anim_cycle(m, rng, c, rng.choice(["run5", "run5", "past"]))
        w.w32(FRAME, rng.randint(0, 2000) * 5 + rng.choice([0, 0, 0, 1, 2]))
        w.w32(0x800B4D10, rng.getrandbits(16))
        return c
    run("MonkeyAi_MoveAlerted", 1500, alerted_sight_world, nh=True, allow_trap=True, i0=30000)

    def alerted_fire_world(m, rng, i):
        c = alerted_world(m, rng, i)
        w = W(m)
        w.w8(c + 0x78, 8)
        in_sight(m, rng, c)
        anim_cycle(m, rng, c, "run5")
        w.w32(FRAME, rng.randint(0, 2000) * 5 + rng.choice([1, 2, 3, 4]))
        w.w32(c + 0x248, m.read32(c + 0x248) | (1 << 23))
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        for k in range(8):
            e = info + 8 * k
            w.w16(e + 0x1E, rng.choice([1, 2, 3]))
            w.w16(e + 0x20, rng.choice([100, 100, 100, 50]))
            w.w16(e + 0x22, 0)
            w.w16(e + 0x24, 5000)
            w.w32(0x800AEBB8 + 8 * k, rng.choice([2, 7, 3, 6, 2]))
            w.w8(0x800AEBB4 + 8 * k, rng.choice([1, 2, 3]))
            w.w8(0x800AEBB5 + 8 * k, rng.choice([8, 0x1E, rng.randint(0, 0x42)]))
        w.w16(c + 0x174, rng.choice([0, 0, 1]))
        w.w16(c + 0xD4, 0)
        w.w32(0x800B4D10, rng.getrandbits(16))
        return c
    run("MonkeyAi_MoveAlerted", 1000, alerted_fire_world, nh=True, allow_trap=True, i0=40000)

    def state07_start_world(m, rng, i):
        """the animation of state 7 is at its start frame (first + 1): the branch that sets the facing from the target"""
        c = state07_world(m, rng, i)
        w = W(m)
        w.w8(c + 0x78, 7)
        anim_cycle(m, rng, c, "start")
        return c
    run("MonkeyAi_State07", 300, state07_start_world, nh=True, allow_trap=True, i0=40000)

    # ------------------------------------------------------------------ State1E_20
    def st1e_world(m, rng, i):
        c = common(m, rng, aboves=())
        w = W(m)
        move_fields(m, rng, c)
        w.w8(c + 0x78, rng.choice([0x1E, 0x20, 0x1E, 0x20, rng.randint(0, 0x42)]))
        w.w32(c + 0x230, rng.choice([0, 28, 29, 30, 31, 100, 238, 239, 240, 241, 1000, rng.randint(0, 400), rng.getrandbits(32)]) & M32)
        w.w16(c + 0x8E, rng.choice([1000, 999, 1001, 401, 400, 399, 0, -1, 50, rng.randint(-100, 1100), rng.getrandbits(16)]) & 0xFFFF)
        w.w32(c + 0x248, (m.read32(c + 0x248) & ~(1 << 24) & M32) | (rng.choice([0, 1, 1]) << 24))
        w.w32(c + 0x248, m.read32(c + 0x248) & ~(1 << 23) & M32 | (rng.choice([0, 1, 1]) << 23))
        w.w8(0x800AEC08 + m.read8(c + 0x7B), rng.choice([1, 1, 0xFF, 0xFF, 0, 2]))
        w.w8(0x800AEC0C + m.read8(c + 0x7B), rng.choice([1, 0xFF, 0xFF, 0, 2]))
        w.w32(c + 0x214, rng.choice([99, 100, 101, 199, 200, 201, 0, -1, rng.randint(0, 300), rng.getrandbits(32)]) & M32)
        w.w16(c + 0x240, rng.choice([0, 1, 100, 500, 1000, 3000, -1, -300, rng.getrandbits(16)]) & 0xFFFF)
        for off in (0x234, 0x23C):
            w.w32(c + off, rng.choice([0, rng.randint(-3000, 3000), rng.getrandbits(32)]) & M32)
        tgt = m.read32(G_TARGET)
        # positions: target near the area centre so that the XZ distance to it straddles the radius +0x240
        rad = s16(m.read16(c + 0x240))
        ang = rng.randint(0, 3)
        d = rng.choice([rad - 1, rad, rad + 1, 0, rng.randint(0, 3000)])
        w.w32(tgt + 0x24, (s32(m.read32(c + 0x234)) + d) & M32)
        w.w32(tgt + 0x2C, s32(m.read32(c + 0x23C)) & M32)
        w.w16(c + 0x72, rng.choice([0, 3, -3, 100, rng.getrandbits(16)]) & 0xFFFF)
        w.w16(c + 0x222, 0)
        w.w8(c + 0x222, rng.choice([0, 1, 2, 0x7F, 0x80, 0xFF]))
        w.w32(G_FRAME2, rng.choice([0, 300, 600, 900, rng.randint(0, 1000) * 300, rng.getrandbits(32)]) & M32)
        w.w32(FRAME, rng.choice([rng.getrandbits(32), rng.randint(0, 100000), 0, 1, 0xFFFFFFFF]) & M32)
        for k in range(8):
            w.w16(0x800AEC00 + 2 * k, rng.choice([100, 100, 99, 99, 101, 0, 1, 5, rng.getrandbits(16)]) & 0xFFFF)
        # the two random numbers whose product is tested against 300 / 600
        kind = rng.choice(["300", "600", "300", "any", "both"])
        if kind == "any":
            w.w32(0x800B4D10, rng.getrandbits(16))
        else:
            mod = 600 if kind == "600" else 300
            w.w32(0x800B4D10, seed_pair(rng, lambda a, b: (a * b) % mod == 0 and rng.random() < 0.9))
        # the goal distance for the dying branch: node goal vs radius +0x220
        rr = s16(m.read16(c + 0x220))
        gp = [s32(m.read32(c + 0x24 + 4 * k)) for k in range(3)]
        w.w32(c + 0xBC, (gp[0] + rng.choice([0, 1, rr - 1, rr, rr + 1, 100])) & M32)
        w.w32(c + 0xC0, gp[1] & M32)
        w.w32(c + 0xC4, gp[2] & M32)
        return c
    run("MonkeyAi_State1E_20", 5000, st1e_world, nh=True, allow_trap=True)

    def st1e_link_world(m, rng, i):
        """not flying (flags bit 24 clear), hit points below 401, past the first 30 frames: the random neighbour of the node; the link that
        MonkeyRand % count picks is made the previous node (+0xF4) most of the time"""
        c = st1e_world(m, rng, i)
        w = W(m)
        w.w32(c + 0x230, rng.choice([29, 30, 100, 500]))
        w.w16(c + 0x8E, rng.choice([400, 399, 0, -5, 100]))
        w.w32(c + 0x248, m.read32(c + 0x248) & ~(1 << 24) & M32)
        nn = m.read16(0x800B4D18)
        node0 = m.read16(c + 0xF2)
        sd = rng.getrandbits(16)
        w.w32(0x800B4D10, sd)
        if node0 < nn and rng.random() < 0.85:
            cnt = m.read8(NAV_BASE + 0x18 * node0 + 0xB)
            if cnt:
                rnd = (((sd * 5 + 1) & 0xFFFF) >> 8)
                link = s4.mips_call(m, 0x8003386C, (s16(node0) & M32, rnd % cnt))
                w.w16(c + 0xF4, link & 0xFFFF)
        if rng.random() < 0.2 and node0 < nn:
            w.w8(NAV_BASE + 0x18 * node0 + 0xB, rng.choice([0, 1, 1, 2]))
        return c
    run("MonkeyAi_State1E_20", 1500, st1e_link_world, nh=True, allow_trap=True, i0=20000)

    def st1e_goal_world(m, rng, i):
        """flying (flags bit 24) with the goal within the reach radius: the end of the flight (defeat burst, SelectNextNode, state 0x36)"""
        c = st1e_world(m, rng, i)
        w = W(m)
        w.w32(c + 0x230, rng.choice([30, 100, 500]))
        w.w16(c + 0x8E, rng.choice([400, 399, 0, -5, 100]))
        w.w32(c + 0x248, m.read32(c + 0x248) | (1 << 24))
        rr = rng.choice([100, 200, 50, 1000])
        w.w16(c + 0x220, rr)
        gp = [s32(m.read32(c + 0x24 + 4 * k)) for k in range(3)]
        for k, off in enumerate((0xBC, 0xC0, 0xC4)):
            w.w32(c + off, (gp[k] + rng.choice([0, 1, -1, 5])) & M32 if rng.random() < 0.9 else rng.getrandbits(32))
        w.w16(0x800F4472, rng.choice([0, 1, 2, 4, 5, 7, 3, 6, 8]))
        w.w8(c + 0x78, rng.choice([0x1E, 0x20, 9, rng.randint(0, 0x42)]))
        return c
    run("MonkeyAi_State1E_20", 1200, st1e_goal_world, nh=True, allow_trap=True, i0=30000)

    def st1e_circle_world(m, rng, i):
        """past the first 240 frames with enough hit points: the circling flight around the area centre (frame counter and random
        numbers chosen so that the direction flips +1 / -1 (+0xEC08 / +0xEC0C) and both ground height branches happen)"""
        c = st1e_world(m, rng, i)
        w = W(m)
        w.w32(c + 0x230, rng.choice([240, 241, 500, 1000]))
        w.w16(c + 0x8E, rng.choice([1000, 1000, 500, 401, 1001]))
        w.w8(c + 0x78, rng.choice([0x1E, 0x20]))
        gate = rng.random() < 0.6
        w.w32(G_FRAME2, (rng.randint(0, 1000) * 300) if gate else rng.getrandbits(32))
        w.w8(0x800AEC08 + m.read8(c + 0x7B), rng.choice([1, 0xFF, 0xFF, 1, 0, 2]))
        w.w8(0x800AEC0C + m.read8(c + 0x7B), rng.choice([1, 0xFF, 1, 0xFF, 0]))
        rad = rng.choice([rng.randint(100, 1500), rng.randint(-1500, -100), 0, 1000])
        w.w16(c + 0x240, rad & 0xFFFF)
        w.w32(c + 0x214, rng.choice([300, 1000, 0]))
        tgt = m.read32(G_TARGET)
        d = rng.choice([rad - 1, rad, rad + 1, 0, rng.randint(0, 3000), rng.randint(0, 100)])
        w.w32(tgt + 0x24, (s32(m.read32(c + 0x234)) + d) & M32)
        w.w32(tgt + 0x2C, s32(m.read32(c + 0x23C)) & M32)
        need = [(300 if gate else 0), rng.choice([0, 600])]
        if not gate:
            need = need[1:]
        w.w32(0x800B4D10, seed_seq(rng, need))
        return c
    run("MonkeyAi_State1E_20", 2500, st1e_circle_world, nh=True, allow_trap=True, i0=40000)

    # ------------------------------------------------------------------ MonkeyInit
    def init_mask(m):
        """a node with flag 0x40 (anywhere in the node memory: the route table can name any index) makes MonkeyUpdateGoalPos measure
        to the never-written position at sp + 32 (leftover stack of earlier calls at that depth, not modelled): only then the two
        distances +0x214 / +0x218 of the monkeys are not compared"""
        for k in range((0x8000 // 0x18)):
            if m.read32(NAV_BASE + 0x18 * k) & 0x40:
                h.mask = base_mask + [(MONKEYS + MONKEY_SIZE * q + 0x214 - 0x80000000, 8) for q in range(8)]
                return
        h.mask = base_mask

    def init_world(m, rng, i):
        c = common(m, rng, aboves=())
        w = W(m)
        nn = m.read16(0x800B4D18)
        move_fields(m, rng, c)
        w.w8(c + 0x7A, rng.choice([0, 1, 1, 2, 3, 4, 5]))
        w.w8(c + 0xD6, rng.choice([0, 0, 1, 2, 4, 7, 0xFF]))
        w.w32(c + 0x248, m.read32(c + 0x248) & ~(1 << 30) & M32 | (rng.choice([0, 0, 1]) << 30))
        w.w32(c + 0x228, (m.read32(c + 0x228) & ~0x02000002 & M32) | rng.choice([0, 0x02000000, 2, 0x02000002]))
        w.w32(0x800B4D0C, sc.ROUTE_BASE)
        data = bytearray()
        import struct
        for k in range(0x200):
            data += struct.pack("<h", rng.choice([-1, -1, rng.randint(0, max(0, nn - 1)), rng.randint(0, max(0, nn - 1)), rng.randint(0, 300)]))
        m.write_bytes(sc.ROUTE_BASE, bytes(data))
        w.w16(0x800AEB68, rng.choice([1, 2, 3, 4, 8, 5, 0, -1]))
        # flags of the node the monkey starts at: bone nodes (0x400), approach nodes (0x40; only in a quarter of the cases, which are
        # the only ones where the two distances +0x214 / +0x218 are masked)
        with40 = rng.random() < 0.25
        for k in range(nn):
            a = NAV_BASE + 0x18 * k
            f = m.read32(a)
            if rng.random() < 0.3:
                f = (f & ~0x440 & M32) | rng.choice([0x400, 0x40, 0x440, 0] if with40 else [0x400, 0, 0x400])
            w.w32(a, f)
            if rng.random() < 0.3:
                w.w8(a + 0xB, rng.choice([0, 1, 1, 2, 3]))
        for k in range(8):
            w.w8(0x800F44BC + k, rng.choice([5, 5, 4, 0, 1, rng.getrandbits(8)]))
        w.w16(0x800F4472, rng.choice([0, 0, 0, 1, 3, 6, 8]))
        w.w32(0x800B4D10, rng.getrandbits(16))
        init_mask(m)
        w.w8(c + 0x78, rng.choice([0, 0xB, 3, 0x1E, rng.randint(0, 0x42)]))
        w.w32(c + 0x20C, rng.choice([0, 500, 1499, 1500, rng.randint(0, 4000)]))
        if rng.random() < 0.03:
            w.w16(0x800B4D18, rng.choice([0, -1]) & 0xFFFF)
        return c
    def init_args(c, rng, i):
        return (rng.choice([rng.randint(0, 15), rng.randint(0, 7), 0, 1, 0x10003, 0xFFFF, rng.getrandbits(32)]) & M32,
                rng.choice([4, 4, 3, 2, 1, 0, 5, rng.getrandbits(32)]) & M32, rng.choice([rng.randint(0, 7), rng.getrandbits(32)]) & M32)
    run("MonkeyInit", 4000, init_world, nh=True, allow_trap=True, argsf=init_args)

    def init_bone_world(m, rng, i):
        """node0 is a bone node (flag 0x400) and the next node has exactly one link or none: the 'first link' branch of the goal set-up"""
        c = init_world(m, rng, i)
        w = W(m)
        nn = min(m.read16(0x800B4D18), 64)
        w.w8(c + 0x7A, rng.choice([0, 3, 4, 5]))
        w.w8(c + 0xD6, 0)
        for k in range(nn):
            a = NAV_BASE + 0x18 * k
            w.w32(a, m.read32(a) | 0x400)
            w.w8(a + 0xB, rng.choice([0, 1, 1, 1, 2]))
        w.w16(0x800F4472, rng.choice([0, 1, 2]))
        return c
    run("MonkeyInit", 1500, init_bone_world, nh=True, allow_trap=True, argsf=init_args, i0=20000)

    def init_route_world(m, rng, i):
        """route table start nodes (+0xD6 != 0), entries -1 (random node) with a zero node count some of the time"""
        import struct
        c = init_world(m, rng, i)
        w = W(m)
        w.w8(c + 0xD6, rng.choice([1, 2, 3, 4, 7]))
        w.w16(0x800AEB68, rng.choice([1, 2, 3, 8]))
        data = bytearray()
        nn = min(m.read16(0x800B4D18), 64)
        for k in range(0x200):
            data += struct.pack("<h", rng.choice([-1, -1, -1, 0, rng.randint(0, max(0, nn - 1))]))
        m.write_bytes(sc.ROUTE_BASE, bytes(data))
        if rng.random() < 0.3:
            w.w16(0x800B4D18, 0)
        return c
    run("MonkeyInit", 800, init_route_world, nh=True, allow_trap=True, argsf=init_args, i0=30000)

    print(f"scenario families done in {time.time() - t0:.1f}s")


MUTATIONS = [
    ("Decide alerted", "if (RD8(monkey + AE_MK_ALERTED) == 1) ae_MonkeyAlertedDecide(r, h);", "if (RD8(monkey + AE_MK_ALERTED) == 0) ae_MonkeyAlertedDecide(r, h);"),
    ("IdleEnd mod6", "if (roll % 6u == 0) ae_MonkeyEnterIdleWait(r);", "if (roll % 12u == 0) ae_MonkeyEnterIdleWait(r);"),
    ("IdleEnd zero", "if (roll % 6u == 0) ae_MonkeyEnterIdleWait(r);", "if (roll % 6u != 0) ae_MonkeyEnterIdleWait(r);"),
    ("IdleEnd fin", "if (ae_MonkeyAnimFinished(r) != 0) return;\n    uint32_t roll", "if (ae_MonkeyAnimFinished(r) == 0) return;\n    uint32_t roll"),
    ("State07 y", "WR16(monkey + AE_MK_MOVE_DIR + 2, 0);\n        WR16(monkey + AE_MK_MOVE_DIR, target_x - monkey_x);", "WR16(monkey + AE_MK_MOVE_DIR + 2, 1);\n        WR16(monkey + AE_MK_MOVE_DIR, target_x - monkey_x);"),
    ("State07 z", "uint32_t monkey_z = RD16(monkey + AE_MK_POS + 8);", "uint32_t monkey_z = RD16(monkey + AE_MK_POS + 4);"),
    ("State07 sel", "if (ae_MonkeyAnimFinished(r) == 0) {\n        ae_MonkeySelectNextNode(r, h, 0, 0);", "if (ae_MonkeyAnimFinished(r) == 0) {\n        ae_MonkeySelectNextNode(r, h, 1, 0);"),
    ("State07 start", "if (ae_MonkeyAnimAtStart(r) != 0) {", "if (ae_MonkeyAnimAtStart(r) == 0) {"),
    ("Unalerted mod3", "if (tick % 3u == 0 && ae_MonkeyUpdateSight(r, h, 1) != 0) {", "if (tick % 6u == 0 && ae_MonkeyUpdateSight(r, h, 1) != 0) {"),
    ("Unalerted sight", "if (tick % 3u == 0 && ae_MonkeyUpdateSight(r, h, 1) != 0) {", "if (tick % 3u == 0 && ae_MonkeyUpdateSight(r, h, 0) != 0) {"),
    ("Unalerted bit6", "if ((STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) >> 6) & 1u) notice = 1;", "if ((STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) >> 7) & 1u) notice = 1;"),
    ("Unalerted alerted", "WR8(monkey + AE_MK_ALERTED, 1);     /* delay slot", "WR8(monkey + AE_MK_ALERTED, 2);     /* delay slot"),
    ("Unalerted 53", "(state_before == 26 || state_before == 53)", "(state_before == 26 || state_before == 52)"),
    ("Unalerted sel", "ae_MonkeySelectNextNode(r, h, 1, 0);\n        if (ae_ram_failed(r)) return;\n        ae_MonkeyPatrolDecide(r, h);", "ae_MonkeySelectNextNode(r, h, 1, 1);\n        if (ae_ram_failed(r)) return;\n        ae_MonkeyPatrolDecide(r, h);"),
    ("Unalerted st9", "if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != 9) ae_MonkeyUpdateFacing(r, h);\n        if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != state_before) return;", "if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != 8) ae_MonkeyUpdateFacing(r, h);\n        if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != state_before) return;"),
    ("Unalerted s1", "ae_MonkeyUpdateFacing(r, h);\n        if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != state_before) return;\n    }\n\n    if (ae_MonkeyAnimFinished(r) == 0) {\n        ae_MonkeyPatrolDecide(r, h);", "ae_MonkeyUpdateFacing(r, h);\n        if (RD8(cur_monkey(r) + AE_MK_AI_STATE) == state_before) return;\n    }\n\n    if (ae_MonkeyAnimFinished(r) == 0) {\n        ae_MonkeyPatrolDecide(r, h);"),
    ("Unalerted lf", "if (RDS16(cur_monkey(r) + AE_MK_LINK_FRAME) > 0) ae_MonkeyInterpolateLinkVector(r);\n    } else {\n        monkey = cur_monkey(r);\n        if (RDS16(monkey + AE_MK_ACTION_COOLDOWN) > 0) {\n            ae_sdk_ApplyMatrixSV(r, monkey + 0x9C", "if (RDS16(cur_monkey(r) + AE_MK_LINK_FRAME) >= 0) ae_MonkeyInterpolateLinkVector(r);\n    } else {\n        monkey = cur_monkey(r);\n        if (RDS16(monkey + AE_MK_ACTION_COOLDOWN) > 0) {\n            ae_sdk_ApplyMatrixSV(r, monkey + 0x9C"),
    ("Unalerted dec", "WR16(monkey + AE_MK_ACTION_COOLDOWN, RD16(monkey + AE_MK_ACTION_COOLDOWN) - 1u);\n        }\n    }\n    apply_step(r);\n}\n\n/* ====", "WR16(monkey + AE_MK_ACTION_COOLDOWN, RD16(monkey + AE_MK_ACTION_COOLDOWN) - 2u);\n        }\n    }\n    apply_step(r);\n}\n\n/* ===="),
    ("Unalerted step", "if (RD32(monkey + AE_MK_FLAGS2) & 2u) {\n        int32_t step_y = RDS16(monkey + AE_MK_VEL + 2);", "if (RD32(monkey + AE_MK_FLAGS2) & 4u) {\n        int32_t step_y = RDS16(monkey + AE_MK_VEL + 2);"),
    ("Alerted dust", "if (state == 3 || state == 8) ae_MonkeyFootDust(r, h);", "if (state == 3 || state == 9) ae_MonkeyFootDust(r, h);"),
    ("Alerted 10", "if (first_frame == frame - 10u) check_sight = 1;", "if (first_frame == frame - 9u) check_sight = 1;"),
    ("Alerted 300", "if (RD32(AE_G_FRAME_COUNTER) % 300u != 0) check_sight = 1;", "if (RD32(AE_G_FRAME_COUNTER) % 600u != 0) check_sight = 1;"),
    ("Alerted cnt", "WR8(monkey + AE_MK_LOST_SIGHT_COUNT, RD8(monkey + AE_MK_LOST_SIGHT_COUNT) + 1u);\n        }\n    }\n\n    if (ae_MonkeyReachedNode", "WR8(monkey + AE_MK_LOST_SIGHT_COUNT, RD8(monkey + AE_MK_LOST_SIGHT_COUNT) + 2u);\n        }\n    }\n\n    if (ae_MonkeyReachedNode"),
    ("Alerted sel", "if ((ae_MonkeySelectNextNode(r, h, 1, 1) & 0xFF) != 0) {", "if ((ae_MonkeySelectNextNode(r, h, 1, 0) & 0xFF) != 0) {"),
    ("Alerted 200", "< 200)) reset_state = 1;", "< 201)) reset_state = 1;"),
    ("Alerted 61", "!(state == 61 || state == 26 || state == 53)", "!(state == 60 || state == 26 || state == 53)"),
    ("Alerted sight", "if (RD32(AE_G_FRAME_COUNTER) % 5u == 0) {\n        if (ae_MonkeyUpdateSight(r, h, 1) != 0) {", "if (RD32(AE_G_FRAME_COUNTER) % 5u != 0) {\n        if (ae_MonkeyUpdateSight(r, h, 1) != 0) {"),
    ("Alerted div5", "if (frame_offset % 5 == 0) {", "if (frame_offset % 6 == 0) {"),
    ("Alerted bit23", "if ((RD32(monkey + AE_MK_FLAGS) >> 23) & 1u) {\n                if (RD8(monkey + AE_MK_AI_STATE) == 8) {", "if ((RD32(monkey + AE_MK_FLAGS) >> 22) & 1u) {\n                if (RD8(monkey + AE_MK_AI_STATE) == 8) {"),
    ("Alerted kind", "if (kind != 100) ae_MonkeyFireProjectileAimed(r, h, kind);", "if (kind != 99) ae_MonkeyFireProjectileAimed(r, h, kind);"),
    ("Alerted nop", "if (!((int32_t)lost_count < limit)) calm_down = 1;", "if (!((int32_t)lost_count <= limit)) calm_down = 1;"),
    ("Alerted reset", "WR8(cur_monkey(r) + AE_MK_LOST_SIGHT_COUNT, 0);\n        ae_MonkeyPatrolDecide(r, h);\n        return;", "WR8(cur_monkey(r) + AE_MK_LOST_SIGHT_COUNT, 1);\n        ae_MonkeyPatrolDecide(r, h);\n        return;"),
    ("Alerted info4", "if (RDS16(info + 4) != 0) {\n                ae_sdk_ApplyMatrixSV", "if (RDS16(info + 4) == 0) {\n                ae_sdk_ApplyMatrixSV"),
    ("S1E 30", "if (RDS32(monkey + AE_MK_STATE_TIMER) < 30) {", "if (RDS32(monkey + AE_MK_STATE_TIMER) < 31) {"),
    ("S1E bit31", "| 0x80000000u);", "| 0x40000000u);"),
    ("S1E 1000", "if (health < 1000) ae_MonkeyDebrisBurst1(r, h);", "if (health < 1001) ae_MonkeyDebrisBurst1(r, h);"),
    ("S1E 401", "if (RDS16(monkey + AE_MK_HEALTH) < 401) {", "if (RDS16(monkey + AE_MK_HEALTH) < 400) {"),
    ("S1E state", "WR8(monkey + AE_MK_AI_STATE, 0x1E);\n    monkey = cur_monkey(r);\n    uint32_t flags = RD32(monkey + AE_MK_FLAGS) | 0x01000000u;", "WR8(monkey + AE_MK_AI_STATE, 0x1F);\n    monkey = cur_monkey(r);\n    uint32_t flags = RD32(monkey + AE_MK_FLAGS) | 0x01000000u;"),
    ("S1E rem1", "rem_trap(r, pick + 1, ", "rem_trap(r, pick + 2, "),
    ("S1E 6050", "SoundLoopStopAndFree, 0x6050,", "SoundLoopStopAndFree, 0x6051,"),
    ("S1E c000", "0xC000u : 0x8000u", "0xC001u : 0x8000u"),
    ("S1E 999", "WR16(cur_monkey(r) + AE_MK_HEALTH, 1000);              /* delay slot", "WR16(cur_monkey(r) + AE_MK_HEALTH, 999);              /* delay slot"),
    ("S1E 3000", "RD32(monkey + AE_MK_POS + 4) - 3000u);", "RD32(monkey + AE_MK_POS + 4) - 2999u);"),
    ("S1E 240", "if (RDS32(monkey + AE_MK_STATE_TIMER) < 240) {\n        WR32(monkey + AE_MK_GOAL_POS, RD32(monkey + AE_MK_POS));", "if (RDS32(monkey + AE_MK_STATE_TIMER) < 241) {\n        WR32(monkey + AE_MK_GOAL_POS, RD32(monkey + AE_MK_POS));"),
    ("S1E 300", "state1e_goal_height(r, h, monkey, 300);", "state1e_goal_height(r, h, monkey, 299);"),
    ("S1E 101", "< 101)) {", "< 100)) {"),
    ("S1E sink", "RD32(monkey + AE_MK_POS + 4) - 5u);", "RD32(monkey + AE_MK_POS + 4) - 4u);"),
    ("S1E turn", "ae_MonkeyTurnTowardTarget(r, 113);", "ae_MonkeyTurnTowardTarget(r, 114);"),
    ("S1E bit23", "if (((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 23) & 1u) == 0) {\n        ae_MonkeyPickAnim(r);", "if (((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 22) & 1u) == 0) {\n        ae_MonkeyPickAnim(r);"),
    ("S1E posz", "WR16(AE_G_BOSS_POS_XZ + 2, RD16(monkey + AE_MK_POS + 8));", "WR16(AE_G_BOSS_POS_XZ + 2, RD16(monkey + AE_MK_POS + 4));"),
    ("S1E ec08", "uint32_t direction = AE_G_BYTES_EC08 + RD8(cur_monkey(r) + AE_MK_SLOT);\n    WR8(direction", "uint32_t direction = AE_G_BYTES_EC0C + RD8(cur_monkey(r) + AE_MK_SLOT);\n    WR8(direction"),
    ("S1E mod600", "% 600 == 0) {", "% 300 == 0) {"),
    ("Init act1", "if (previous_activity == 1) {", "if (previous_activity == 0) {"),
    ("Init bit30", "if ((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 30) & 1u) return;\n        if ((activity & 0xFFu) != 4) return;", "if ((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 29) & 1u) return;\n        if ((activity & 0xFFu) != 4) return;"),
    ("Init act2", "if ((activity & 0xFFu) == 2) return;", "if ((activity & 0xFFu) == 3) return;"),
    ("Init 200", "WR16(monkey + AE_MK_REACH_RADIUS, 200);", "WR16(monkey + AE_MK_REACH_RADIUS, 201);"),
    ("Init 80", "WR16(monkey + 0x168, 80);", "WR16(monkey + 0x168, 81);"),
    ("Init d6", "uint32_t route_count = RD8(monkey + AE_MK_ROUTE_COUNT);", "uint32_t route_count = RD8(monkey + AE_MK_ROUTE_COUNT + 1);"),
    ("Init cnt", "rem_trap(r, s16(id), RDS16(AE_G_MONKEY_COUNT))", "rem_trap(r, s16(id), RDS16(AE_G_ACTIVE_MONKEY_MAX))"),
    ("Init route", "(uint32_t)route_monkey * 16u", "(uint32_t)route_monkey * 8u"),
    ("Init act5", "RD8(AE_G_MONKEY_ACTIVITY + RD8(monkey + AE_MK_ID)) != 5", "RD8(AE_G_MONKEY_ACTIVITY + RD8(monkey + AE_MK_ID)) != 4"),
    ("Init 100", "WR32(sp + 16, RD32(sp + 16) + 100u);", "WR32(sp + 16, RD32(sp + 16) + 99u);"),
    ("Init flags", "WR32(monkey + AE_MK_FLAGS, flags & 0x0FFCFFFFu);", "WR32(monkey + AE_MK_FLAGS, flags & 0x0FFEFFFFu);"),
    ("Init ff00", "body_height = 0xFFFFFF00u;", "body_height = 0x100u;"),
    ("Init 8000", "else WR16(cur_monkey(r) + 0x4A, 0x8000);", "else WR16(cur_monkey(r) + 0x4A, 0x8001);"),
    ("Init minus2", "WR32(monkey + AE_MK_POS, RD32(monkey + AE_MK_POS) - 2u);", "WR32(monkey + AE_MK_POS, RD32(monkey + AE_MK_POS) - 1u);"),
    ("Init 500", "WR32(monkey + AE_MK_GOAL_POS, RD32(monkey + AE_MK_POS) + 500u);", "WR32(monkey + AE_MK_GOAL_POS, RD32(monkey + AE_MK_POS) + 501u);"),
    ("Init sel", "ae_MonkeyUpdateTargetDistance(r, h);\n            ae_MonkeySelectNextNode(r, h, 0, 1);", "ae_MonkeyUpdateTargetDistance(r, h);\n            ae_MonkeySelectNextNode(r, h, 1, 1);"),
    ("Init 200y", "GetGroundHeight, x, y - 200u, z, 0);", "GetGroundHeight, x, y - 199u, z, 0);"),
    ("Init 255", "WR8(cur_monkey(r) + 0xD5, 255);", "WR8(cur_monkey(r) + 0xD5, 254);"),
    ("Init 11", "& AE_NAV_FLAG_REF_POS) ae_MonkeySetState(r, 11);", "& AE_NAV_FLAG_REF_POS) ae_MonkeySetState(r, 12);"),
    ("Init boss", "if (ae_AreaIsBossArena(r) != 0) ae_MonkeySetState(r, 11);\n}", "if (ae_AreaIsBossArena(r) == 0) ae_MonkeySetState(r, 11);\n}"),
]

MUT_FUNCS = {
    "Decide": ["MonkeyDecideAction"], "IdleEnd": ["MonkeyAi_IdleActivityEnd"], "State07": ["MonkeyAi_State07"],
    "Unalerted": ["MonkeyAi_MoveUnalerted"], "Alerted": ["MonkeyAi_MoveAlerted"], "S1E": ["MonkeyAi_State1E_20"], "Init": ["MonkeyInit"],
}


def run_mutations(scale, seed, only_labels=(), shard=None):
    rel = "src/monkeys/ae_monkey_level8.c"
    text = (ROOT / rel).read_text(encoding="utf-8")
    survivors = []
    tmp = Path(tempfile.mkdtemp(prefix="mk8_mut_"))
    h = mh.MonkeyHarness(FUNCS)
    for k, (label, old, new) in enumerate(MUTATIONS):
        if only_labels and not any(s in label for s in only_labels):
            continue
        if shard and k % shard[1] != shard[0]:
            continue
        if old not in text:
            print(f"MUTATION {label!r}: pattern not found")
            survivors.append(label + " (pattern missing)")
            continue
        mp = tmp / f"mut{k}_{Path(rel).name}"
        mp.write_text(text.replace(old, new, 1), encoding="utf-8")
        try:
            h.reload_c({rel: mp}, tag=f"mk8mut{k}")
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
    ap.add_argument("--seed", type=int, default=0x944281)
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--trapv", action="store_true", help="build the port with -ftrapv (signed overflow aborts)")
    ap.add_argument("--mutate", action="store_true", help="mutation-test the checker (slow)")
    ap.add_argument("--mutate-only", nargs="*", default=[], help="only the mutations whose label contains one of these words")
    ap.add_argument("--stop-after", type=int, default=0)
    ap.add_argument("--shard", default="", help="with --mutate: K/N runs every N-th mutation starting at K (parallel runs)")
    a = ap.parse_args()
    if not (ROOT / "extract/disc/SCUS_944.23").exists():
        print("SKIP: extract/disc/SCUS_944.23 not present")
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
