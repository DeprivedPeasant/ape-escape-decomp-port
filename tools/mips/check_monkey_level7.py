"""Differential check of monkey port batch 4d (src/monkeys/ae_monkey_level7.c, call-graph level 7) against the original MIPS
code: for every ported function many scenarios are built in a byte-exact RAM image (SCUS_944.23 at its real addresses; monkeys /
nav nodes / extra link records / MonkeyInfo / animation tables / patrol routes / globals laid out as MonkeySpawnAll and the level
loader leave them, plus randomised and edge-case states); the original instructions (GTE through the PCSX-ReARMed oracle) and the
C port run on identical images with identical hook behaviour; return value, RAM outside the stack window, scratchpad and the sequence
of hook calls must be identical.  The batch 1..4c functions the code calls run as the C ports on the C side and as the original
instructions on the MIPS side.  Needs extract/disc/SCUS_944.23 (skips if missing).  See check_monkey_level4.py .. level6.py and
monkey_scen4.py.

Usage: uv run python tools/mips/check_monkey_level7.py [--cases PERCENT] [--seed S] [--coverage] [--trapv] [--mutate]
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
    "MonkeyPatrolDecide": (0x80037908, 1124, "v", True, ""),
    "MonkeySelectNextNode": (0x8003B074, 1216, "s", True, "ss"),
    "MonkeyAi_IdleActivityStart": (0x8003F18C, 380, "v", True, ""),
    "MonkeyAi_IdleActivityLoop": (0x8003F308, 428, "v", True, ""),
    "MonkeyAi_GoToFlaggedNode": (0x8004002C, 324, "v", True, "u"),
}
EXTRA_COVERAGE = {
    "MonkeyAlertedDecide (batch 4c)": (0x80037550, 880), "MonkeyChooseNextNodeA (batch 4c)": (0x80038A1C, 128),
    "MonkeyChooseNextNodeB (batch 4c)": (0x80038A9C, 1124), "MonkeyNodeActionDispatch (batch 4b)": (0x80036CD0, 920),
    "MonkeyConsiderTurnBack (batch 4b)": (0x800386C8, 588), "MonkeyConsiderTurnBackRandom (batch 4b)": (0x80038914, 264),
    "MonkeyArriveAtNode (batch 4a)": (0x80040D90, 1264), "MonkeyPickBestLink (batch 4a)": (0x80038120, 1256),
    "MonkeySetNavNode (batch 4a)": (0x80035080, 236), "MonkeySwapNavNodes (batch 4a)": (0x80038694, 52),
    "MonkeyEnterState_1D (batch 4a)": (0x800359F0, 140), "MonkeyEnterState_04 (batch 4a)": (0x80035A7C, 340),
    "MonkeyEnterState_2C (batch 4a)": (0x80036964, 208), "MonkeyEnterState_17 (batch 3a)": (0x8003641C, 0x44),
    "MonkeyEnterIdleWait (batch 3b)": (0x80036300, 164), "MonkeyEnterRandomLook (batch 3b)": (0x800363E4, 56),
    "MonkeyStartNotice (batch 3b)": (0x800357E0, 60), "MonkeyPickGoalNode (batch 2)": (0x8003AD9C, 728),
    "MonkeyLoadLinkExtra (batch 2)": (0x8003B674, 524), "MonkeyUpdateSight (batch 3a)": (0x800405C4, 0x1A8),
    "MonkeyLineOfSight (batch 2)": (0x800401F4, 976), "MonkeyNavBacktracked (batch 1)": (0x80035604, 0xBC),
    "MonkeyGetCallVoice (batch 1)": (0x800356C0, 0x90), "MonkeyIsIdleEndState (batch 1)": (0x80033B9C, 0x44),
    "MonkeyAnimFinished (batch 1)": (0x80033A3C, 0x90), "Distance3D (batch 1)": (0x80034684, 0xB8),
    "MonkeySetState (batch 3a)": (0x80033220, 216), "MonkeyUpdateGoal (batch 3b)": (0x800355DC, 40),
    "MonkeyScoreLinksByTarget (batch 3a)": (0x8003AC54, 0xE0), "MonkeyMarkBlockedLinks (batch 3b)": (0x80038054, 204),
}
DISPATCH_BITS = [0x100, 0x800, 0x20, 0x20000, 8, 4, 0x10, 1, 0x10000, 0x800000, 0x1000000]
PATROL_STATES = [2, 3, 25, 24, 39, 40, 15, 18, 21, 60, 0, 1, 12, 36, 11]
IDLE_STATES = [13, 14, 16, 17, 18, 19, 20, 21, 58, 59, 60, 15, 3]


def check_all(h, scale, seed):
    t0 = time.time()
    h.mask = [(MONKEYS + MONKEY_SIZE * k + 0xC8 - 0x80000000, 4) for k in range(8)]   # see check_monkey_level4.py
    aim = s4.DotAim(h)

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

    def common(m, rng, state=None, aboves=()):
        """nav world + animation globals + attack / LOS fields; returns the current monkey.  aboves: the stack depths of the possible
        call paths to the state entries that read leftover stack (summed frame sizes, see monkey_scen4.stale_enter)"""
        c = s4.nav_entry_world(m, rng)
        s4.attack_fields(m, rng, c)
        s4.los_fields(m, rng, c)
        w = W(m)
        nn = m.read16(0x800B4D18)
        s4.no_stale_goal(m, rng, c, nn)
        if aboves:
            s4.stale_enter(m, rng, list(aboves))
        if state is not None:
            w.w8(c + 0x78, state)
        return c

    def tune_info(m, rng, c):
        """the MonkeyInfo fields MonkeyPatrolDecide compares (+2, +4, +8, +0xE, +0x12) around the distance / small values"""
        w = W(m)
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        dist = s32(m.read32(c + 0x20C))
        w.w16(info + 2, rng.choice([0, 0, 0, 1, -1, 5, rng.getrandbits(16)]) & 0xFFFF)
        w.w16(info + 4, rng.choice([0, 0, 0, 1, -1, 5, rng.getrandbits(16)]) & 0xFFFF)
        w.w16(info + 8, rng.choice([0, 1, 5, 9, 10, 30, 50, 99, 100, 101, rng.getrandbits(16), 0x8000]) & 0xFFFF)
        w.w16(info + 0xE, rng.choice([dist - 1, dist, dist + 1, rng.randint(0, 3000), rng.getrandbits(16)]) & 0xFFFF)
        w.w16(info + 0x12, rng.choice([dist - 1, dist, dist + 1, rng.randint(0, 3000), rng.getrandbits(16), 0, -1]) & 0xFFFF)

    # ------------------------------------------------------------------ PatrolDecide
    def patrol_world(m, rng, i, st=None, alerted=None, bit3=None):
        c = common(m, rng, aboves=(40 + 32, 40 + 32 + 32))    # PatrolDecide (40) -> NodeActionDispatch (32) [-> AlertedDecide (32) in boss arenas]
        w = W(m)
        bits = 0 if rng.random() < 0.7 else sum(b for b in DISPATCH_BITS if rng.random() < 0.2)
        w.w32(c + 0x228, bits | (rng.getrandbits(32) & 0x200 if rng.random() < 0.3 else 0))
        state = st if st is not None else rng.choice(PATROL_STATES + [rng.randint(0, 0x42)])
        w.w8(c + 0x78, state)
        w.w32(c + 0x248, (rng.getrandbits(32) & ~((1 << 20) | (1 << 22))) | (rng.choice([0, 0, 0, 1 << 20, 1 << 22]) & M32))
        w.w8(c + 0x79, rng.choice([0, 1, 1, 2]) if alerted is None else alerted)
        w.w16(c + 0x94, rng.choice([0, 1, -1, 5, 0x7FFF, 0x8000, rng.getrandbits(16)]) & 0xFFFF)
        w.w8(c + 0x222, rng.choice([0, 1, 2, 0x7F, 0x80, 0xFF, rng.getrandbits(8)]))
        flagw = m.read32(STATE_TABLE + 8 * (state % 0x43) + 4)
        b3 = bit3 if bit3 is not None else rng.choice([0, 0, 1])
        w.w32(STATE_TABLE + 8 * (state % 0x43) + 4, (flagw | 8) if b3 else (flagw & ~8))
        tune_info(m, rng, c)
        w.w16(0x800F4472, rng.choice([0, 0, 0, 1, 3, 6, 8, 2]))
        w.w8(0x800F447C, rng.choice([3, 3, 0, 1, rng.getrandbits(8)]))
        # MonkeyRand: aim the remainders the function tests (< 9 / < 10 / the info limit / & 1)
        lim8 = s16(m.read16(m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C) + 8))
        if 0 <= lim8 < 100 and rng.random() < 0.5:        # the first random number right at the MonkeyInfo limit (+8)
            w.w32(0x800B4D10, s4.seed_for(rng, lambda v: v % 100 in (lim8 - 1, lim8, lim8, lim8 + 1)))
        else:
            w.w32(0x800B4D10, s4.seed_for(rng, lambda v: rng.random() < 0.2 or v % 100 in rng.choice([(8, 9), (9, 10), (0, 1, 2), (50, 51, 52)])))
        return c
    run("MonkeyPatrolDecide", 3000, patrol_world, nh=True, allow_trap=True)
    def patrol_b98_world(m, rng, i):
        # state 2 on the "wanderer" branch with no cooldown: Rand % 100 < 10, then Rand & 1 picks state 39 / 40
        c = patrol_world(m, rng, i, st=2, alerted=0, bit3=0)
        w = W(m)
        w.w32(c + 0x228, 0)
        w.w16(c + 0x94, rng.choice([0, -1]) & 0xFFFF)
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        w.w16(info + 2, 1)
        w.w16(info + 8, rng.choice([0, 1]))
        bit = rng.choice([0, 1])
        w.w32(0x800B4D10, s4.seed_for_seq(rng, [lambda v: v % 100 >= 2, lambda v: v % 100 < 10 if rng.random() < 0.8 else v % 100 in (9, 10),
                                                lambda v: (v & 1) == bit]))
        return c
    run("MonkeyPatrolDecide", 400, patrol_b98_world, nh=True, allow_trap=True, i0=30000)
    def patrol_c60_world(m, rng, i):
        # wanderer branch, ground state, no cooldown, unalerted before: the single Rand % 100 against 9 (edge 8 / 9 / 10)
        c = patrol_world(m, rng, i, st=rng.choice([3, 24, 39, 40, 1]), alerted=0, bit3=0)
        w = W(m)
        w.w32(c + 0x228, 0)
        w.w16(c + 0x94, rng.choice([0, -1]) & 0xFFFF)
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        w.w16(info + 2, 1)
        w.w32(0x800B4D10, s4.seed_for(rng, lambda v: v % 100 in (7, 8, 8, 9, 9, 10)))
        return c
    run("MonkeyPatrolDecide", 300, patrol_c60_world, nh=True, allow_trap=True, i0=32000)
    ci = 500000
    for st in range(0x43):
        for al in (0, 1):
            for bit3 in (0, 1):
                rng = rng_of("MonkeyPatrolDecide-grid", ci)

                def setup(m, rng=rng, st=st, al=al, bit3=bit3, ci=ci):
                    c = patrol_world(m, rng, ci, st=st, alerted=al, bit3=bit3)
                    W(m).w32(c + 0x228, 0)
                    return c
                h.run("MonkeyPatrolDecide", f"grid {st} {al} {bit3}", ci, setup, lambda c: (), allow_trap=True)
                ci += 1

    # ------------------------------------------------------------------ SelectNextNode
    def select_world(m, rng, i):
        c = s4.pick_world(m, rng, i)
        anim_globals(m, rng, c)
        w = W(m)
        nn = m.read16(0x800B4D18)
        s4.no_stale_goal(m, rng, c, nn)
        s4.route_fields(m, rng, c, nn)
        s4.los_fields(m, rng, c) if False else None
        node0 = m.read16(c + 0xF2)
        w.w32(c + 0x248, (m.read32(c + 0x248) & ~(1 << 27)) | (rng.choice([0, 1, 1]) << 27))
        w.w16(c + 0xE6, rng.choice([node0, node0, rng.randint(0, nn - 1), rng.getrandbits(16)]))
        w.w8(c + 0x78, rng.choice([61, 61, 3, 1, 0x0C, rng.randint(0, 0x42)]))
        # history patterns for NavBacktracked: [n0, n1, n2, p0, p1, p2]
        a, b, d = rng.randint(0, nn - 1), rng.randint(0, nn - 1), rng.randint(0, nn - 1)
        pat = rng.choice([[a, b, a, b, a, b], [a, a, a, a, a, a], [a, b, d, a, b, d], [a, b, d, d, b, a], [a, b, a, a, b, a],
                          [a, b, d, rng.randint(0, nn - 1), b, d], [rng.randint(0, nn - 1) for _ in range(6)]])
        if rng.random() < 0.6:
            pat[0] = node0
        for k, v in enumerate(pat):
            w.w16(c + 0xF2 + 2 * k, v)
        # the next node (+0xF4): flag 0x10000 sometimes
        n1 = pat[1] % nn
        nb = NAV_BASE + 0x18 * n1
        if rng.random() < 0.25:
            w.w32(nb, m.read32(nb) | 0x10000)
        else:
            w.w32(nb, m.read32(nb) & ~0x10000 & M32)
        w.w16(c + 0xEC, rng.choice([pat[0], pat[1], rng.randint(0, 40), 0xFFFF]))
        w.w8(c + 0x79, rng.choice([0, 1, 1, 1]))
        # the arrival side: node flags for ArriveAtNode (activity 4) and the extra link records (LoadLinkExtra)
        w.w8(c + 0x7A, rng.choice([4, 4, 4, 3, 2]))
        for k in range(nn):
            a_ = NAV_BASE + 0x18 * k
            if rng.random() < 0.3:
                w.w32(a_, m.read32(a_) | rng.choice([0x20, 0x1000, 0x4000, 0x800, 0x20000, 0x40000, 0x80000, 4, 8, 0x8000, 0x10000]))
        w.w32(c + 0x20C, rng.choice([0, 500, 1499, 1500, rng.randint(0, 4000)]))
        w.w16(0x800F4472, rng.choice([0, 0, 0, 0, 1, 3, 6, 8]))
        w.w32(0x800B4D10, rng.getrandbits(16))
        w.w16(c + 0x220, rng.choice([rng.randint(-300, 600), 200, 0, 1, -1]) & 0xFFFF)
        return c
    def backtrack_world(m, rng, i):
        # alerted, ChooseNextNodeA picks the neighbour L with the highest visit counter; the history [a, c, L, a, c, x] becomes
        # [L, a, c, L, a, c] after SetNavNode, which MonkeyNavBacktracked reports as a repetition
        c = select_world(m, rng, i)
        w = W(m)
        nn = m.read16(0x800B4D18)
        a = m.read16(c + 0xF2)
        for _ in range(2):
            ents = [m.read16(m.read32(NAV_BASE + 0x18 * a + 0xC) + 2 * j) for j in range(m.read8(NAV_BASE + 0x18 * a + 0xA))]
            if sum(1 for e in ents if e & 0x4000) >= 2:
                break
            q = rng.randint(0, nn - 1)
            if q != a:
                s4.relink(m, rng, nn, a, q, True, rng.random() < 0.6)
        s4.set_link_counts(m, rng, nn, exact=1.0)
        ents = [m.read16(m.read32(NAV_BASE + 0x18 * a + 0xC) + 2 * j) for j in range(m.read8(NAV_BASE + 0x18 * a + 0xA))]
        links = [e & 0x3FFF for e in ents if e & 0x4000]
        if not links:
            return c
        slot = m.read8(c + 0x7B)
        L = rng.choice(links)
        for q in range(nn):
            w.w8(NAV_BASE + 0x18 * q + slot + 0x14, 0)
        w.w8(NAV_BASE + 0x18 * L + slot + 0x14, 5)
        others = [q for q in links if q != L]
        w.w16(c + 0xEC, rng.choice(others + [0xFFFF]) if others else 0xFFFF)
        cc = rng.randint(0, nn - 1)
        for k, v in enumerate([a, cc, L, a, cc, rng.randint(0, nn - 1)]):
            w.w16(c + 0xF2 + 2 * k, v)
        na = NAV_BASE + 0x18 * a
        w.w32(na, m.read32(na) & ~0x10000 & M32)
        w.w8(c + 0x79, 1)
        w.w8(c + 0x78, rng.choice([3, 1, 0x0C, 5]))
        w.w16(0x800F4472, 0)
        nb = NAV_BASE + 0x18 * L
        w.w32(nb, m.read32(nb) & ~0x10000 & M32)
        return c
    run("MonkeySelectNextNode", 1500, backtrack_world, nh=True, allow_trap=True, i0=20000,
        argsf=lambda c, rng, i: (rng.choice([1, 1, 0x10001]) & M32, rng.choice([0, 0, 2, 0x100]) & M32))
    sel_args = lambda c, rng, i: (rng.choice([0, 0, 0, 1, 0x10000, -1, rng.getrandbits(32)]) & M32, rng.choice([0, 1, 1, 0x10000, 0x100, -1, rng.getrandbits(32)]) & M32)
    run("MonkeySelectNextNode", 4000, select_world, nh=True, allow_trap=True, argsf=sel_args)
    def select_boss_world(m, rng, i):
        """alerted in a boss arena (or not), state 61 / 60: the ChooseNextNodeA == 0 exits (turn back, no link) take the state 61 branch"""
        c = select_world(m, rng, i)
        w = W(m)
        w.w8(c + 0x79, 1)
        w.w8(c + 0x78, rng.choice([61, 61, 60, 3]))
        w.w16(0x800F4472, rng.choice([3, 6, 8, 3, 0]))
        return c
    run("MonkeySelectNextNode", 1500, select_boss_world, nh=True, allow_trap=True, i0=30000,
        argsf=lambda c, rng, i: (rng.choice([0, 0, 0, 1, 0x10000]) & M32, rng.choice([0, 1]) & M32))

    # ------------------------------------------------------------------ idle activity
    def idle_world(m, rng, i, st=None, aboves=(88, 64, 96)):
        c = common(m, rng, aboves=aboves)
        w = W(m)
        state = st if st is not None else rng.choice(IDLE_STATES + [rng.randint(0, 0x42)])
        w.w8(c + 0x78, state)
        w.w32(c + 0x228, 0 if rng.random() < 0.75 else sum(b for b in DISPATCH_BITS if rng.random() < 0.2))
        w.w8(c + 0x79, rng.choice([0, 1]))
        w.w16(c + 0x98, rng.choice([0, 1, 2, 8, 9, 16, 0x8000, 0x8001, 0x8008, 0xFFFF, rng.getrandbits(16), rng.randint(0, 64) * 8 + 1]) & 0xFFFF)
        flagw = m.read32(STATE_TABLE + 8 * (state % 0x43) + 4)
        w.w32(STATE_TABLE + 8 * (state % 0x43) + 4, (flagw | 0x40) if rng.random() < 0.5 else (flagw & ~0x40 & M32))
        # frame counter + id divisible by 3 half of the time
        idv = m.read8(c + 0x7C)
        fr = rng.getrandbits(32)
        if rng.random() < 0.6:
            fr = (fr - (fr + idv) % 3) & M32
        w.w32(FRAME, fr)
        w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x43, 0x35, 0x46]))
        w.w16(0x800F4472, rng.choice([0, 0, 1, 2, 3, 6]))
        # the link position counter +0x21C selects the UpdateSight window (0..4096): the window edges
        w.w32(c + 0x21C, rng.choice([0, 455, 456, 457, 568, 569, 570, 1479, 1480, 1481, 1591, 1592, 1593, 2503, 2504, 2505, 2615, 2616, 2617,
                                     3527, 3528, 3529, 3639, 3640, 3641, 4095, 4096, rng.randint(0, 4096)]))
        w.w32(0x800B4D10, s4.seed_for(rng, lambda v: rng.random() < 0.3 or v % 20 in rng.choice([(0, 1), (18, 19), (5,)])))
        return c
    def start_world(m, rng, i, st=None):
        # UpdateSight / GetCallVoice / QueueVoice run before the alerted decision and overwrite the stack words EnterState_1D / _1E /
        # _09 read when the link has no extra record (chain dependent, not modelled): those entries are not requested
        c = idle_world(m, rng, i, st=st, aboves=())
        W(m).w32(c + 0x228, m.read32(c + 0x228) & ~(0x100 | 0x800 | 0x4) & M32)
        return c
    run("MonkeyAi_IdleActivityStart", 3000, start_world, nh=True, allow_trap=True)
    run("MonkeyAi_IdleActivityLoop", 3000, idle_world, nh=True, allow_trap=True)
    # (depths: Start (24) -> AlertedDecide (32) -> NodeActionDispatch (32) = 88; Loop (32) -> NodeActionDispatch = 64, via AlertedDecide = 96)
    ci = 600000
    for st in range(0x43):
        for fin in (0, 1):
            rng = rng_of("MonkeyAi_Idle-grid", ci)

            def setup(m, rng=rng, st=st, ci=ci):
                return idle_world(m, rng, ci, st=st)
            def setup_s(m, rng=rng, st=st, ci=ci):
                return start_world(m, rng, ci, st=st)
            h.run("MonkeyAi_IdleActivityStart", f"grid {st} {fin}", ci, setup_s, lambda c: (), allow_trap=True)
            h.run("MonkeyAi_IdleActivityLoop", f"grid {st} {fin}", ci, setup, lambda c: (), allow_trap=True)
            ci += 1

    # ------------------------------------------------------------------ GoToFlaggedNode
    def flagged_world(m, rng, i):
        c = common(m, rng, aboves=(64 + 32 + 32,))             # GoToFlaggedNode (64) -> AlertedDecide (32) -> NodeActionDispatch (32)
        w = W(m)
        nn = m.read16(0x800B4D18)
        for k in range(nn):
            a = NAV_BASE + 0x18 * k
            w.w32(a, (m.read32(a) & ~0x2000) | (0x2000 if rng.random() < 0.3 else 0) | (rng.choice([0, 0x20, 0x4000, 0x800, 4, 8, 0x8000, 0x10000]) if rng.random() < 0.3 else 0))
        w.w8(c + 0x7A, rng.choice([4, 4, 4, 3]))
        w.w8(c + 0x79, rng.choice([0, 1]))
        # AlertedDecide -> NodeActionDispatch runs after SetNavNode / LoadLinkExtra / ArriveAtNode, whose frames have overwritten the
        # stack words EnterState_1D / _1E / _09 read when the link has no extra record (depends on that earlier chain: not modelled),
        # so those three entries are not requested
        w.w32(c + 0x228, m.read32(c + 0x228) & ~(0x100 | 0x800 | 0x4) & M32)
        if rng.random() < 0.05:
            w.w16(0x800B4D18, rng.choice([0, -1]))
        return c
    def far_world(m, rng, i):
        # one flagged node at a distance of exactly 60000 (the initial limit) or beyond 65535 (the low 16 bits of the distance are compared)
        c = flagged_world(m, rng, i)
        w = W(m)
        nn = s16(m.read16(0x800B4D18))
        if nn <= 0:
            return c
        for k in range(nn):
            a = NAV_BASE + 0x18 * k
            w.w32(a, m.read32(a) & ~0x2000 & ~0x401 & M32)
        k = rng.randint(0, nn - 1)
        a = NAV_BASE + 0x18 * k
        w.w32(a, m.read32(a) | 0x2000)
        # Distance3D loads each axis difference into a 16-bit GTE register (low half, sign extended) and squares it, so the distance is
        # at most 56755: the initial limit 60000 and the 16-bit mask never matter (mutations 'Flag s1' / 'Flag mask' are equivalent)
        dx, dy, dz = rng.choice([(36000, 48000, 0), (36000, 48000, 0), (0, 36000, 48000), (48000, 0, 36000), (36000, 47999, 0),
                                 (36001, 48001, 0), (36000, 48001, 0), (36001, 48000, 0), (rng.randint(0, 32767), rng.randint(0, 32767), rng.randint(0, 32767)),
                                 (60000, 0, 0), (70000, 5, 0)])
        w.w16(a + 4, 32767)
        w.w16(a + 6, 32767)
        w.w16(a + 8, 32767)
        w.w32(c + 0x24, (32767 - dx) & M32)
        w.w32(c + 0x28, (32767 - dy) & M32)
        w.w32(c + 0x2C, (32767 - dz) & M32)
        return c
    run("MonkeyAi_GoToFlaggedNode", 600, far_world, nh=True, allow_trap=True, i0=40000, argsf=lambda c: (0,))
    run("MonkeyAi_GoToFlaggedNode", 3500, flagged_world, nh=True, allow_trap=True, argsf=lambda c: (0,))

    print(f"scenario families done in {time.time() - t0:.1f}s")


MUTATIONS = [
    ("Patrol alerted", 'uint32_t was_alerted = (alerted0 ^ 1u) < 1u;', 'uint32_t was_alerted = (alerted0 ^ 1u) < 2u;'),
    ("Patrol st12", 'uint32_t was_fast_ground_state = (st0 == 12 || st0 == 36) ? 1u : 0u;', 'uint32_t was_fast_ground_state = (st0 == 11 || st0 == 36) ? 1u : 0u;'),
    ("Patrol st36", 'uint32_t was_fast_ground_state = (st0 == 12 || st0 == 36) ? 1u : 0u;', 'uint32_t was_fast_ground_state = (st0 == 12 || st0 == 35) ? 1u : 0u;'),
    ("Patrol f20", "if ((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 20) & 1u) return;\n    if (ae_AreaIsBossArena(r)) {\n        ae_MonkeyAlertedDecide", "if ((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 21) & 1u) return;\n    if (ae_AreaIsBossArena(r)) {\n        ae_MonkeyAlertedDecide"),
    ("Patrol boss", "if (ae_AreaIsBossArena(r)) {\n        ae_MonkeyAlertedDecide(r, h);", "if (!ae_AreaIsBossArena(r)) {\n        ae_MonkeyAlertedDecide(r, h);"),
    ("Patrol alert0", 'WR8(cur_monkey(r) + AE_MK_ALERTED, 0);\n    uint32_t monkey', 'WR8(cur_monkey(r) + AE_MK_ALERTED, 1);\n    uint32_t monkey'),
    ("Patrol bit3", 'if (((flagw >> 3) & 1u) && RDS8(monkey + AE_MK_PATROL_COUNTER) > 0) {\n        ae_MonkeyPickAnim', 'if (((flagw >> 4) & 1u) && RDS8(monkey + AE_MK_PATROL_COUNTER) > 0) {\n        ae_MonkeyPickAnim'),
    ("Patrol 222", 'if (((flagw >> 3) & 1u) && RDS8(monkey + AE_MK_PATROL_COUNTER) > 0) {\n        ae_MonkeyPickAnim', 'if (((flagw >> 3) & 1u) && RDS8(monkey + AE_MK_PATROL_COUNTER) >= 0) {\n        ae_MonkeyPickAnim'),
    ("Patrol disp", '} else if (ae_MonkeyNodeActionDispatch(r, h) == 0) {', '} else if (ae_MonkeyNodeActionDispatch(r, h) != 0) {'),
    ("Patrol info2", 'if (RDS16(info + AE_MK_INFO_WALK_SPEED) == 0 && RDS16(info + AE_MK_INFO_ALERT_SPEED) == 0) {', 'if (RDS16(info + AE_MK_INFO_WALK_SPEED) == 0 || RDS16(info + AE_MK_INFO_ALERT_SPEED) == 0) {'),
    ("Patrol info4", 'if (RDS16(info + AE_MK_INFO_WALK_SPEED) == 0 && RDS16(info + AE_MK_INFO_ALERT_SPEED) == 0) {', 'if (RDS16(info + AE_MK_INFO_WALK_SPEED) == 0 && RDS16(info + 6) == 0) {'),
    ("Patrol i12", 'int32_t idle_distance = RDS16(info + AE_MK_INFO_IDLE_DISTANCE);', 'int32_t idle_distance = RDS16(info + 0x10);'),
    ("Patrol lt12", 'int wait = !(idle_distance < dist);', 'int wait = !(idle_distance <= dist);'),
    ("Patrol lim8", 'int32_t lim = RDS16(info2 + AE_MK_INFO_IDLE_CHANCE);\n                wait = (int32_t)rem < lim;', 'int32_t lim = RDS16(info2 + AE_MK_INFO_IDLE_CHANCE);\n                wait = (int32_t)rem <= lim;'),
    ("Patrol st0", 'else ae_MonkeySetState(r, 0);', 'else ae_MonkeySetState(r, 1);'),
    ("Patrol cd", 'if (RDS16(reloaded_monkey + AE_MK_ACTION_COOLDOWN) > 0) {\n                ae_MonkeySetState(r, 2);', 'if (RDS16(reloaded_monkey + AE_MK_ACTION_COOLDOWN) >= 0) {\n                ae_MonkeySetState(r, 2);'),
    ("Patrol cdst", 'if (RDS16(reloaded_monkey + AE_MK_ACTION_COOLDOWN) > 0) {\n                ae_MonkeySetState(r, 2);', 'if (RDS16(reloaded_monkey + AE_MK_ACTION_COOLDOWN) > 0) {\n                ae_MonkeySetState(r, 3);'),
    ("Patrol st25", 'if (st == 25) {\n                    ae_MonkeySetState(r, 24);', 'if (st == 26) {\n                    ae_MonkeySetState(r, 24);'),
    ("Patrol st25b", 'if (st == 25) {\n                    ae_MonkeySetState(r, 24);', 'if (st == 25) {\n                    ae_MonkeySetState(r, 25);'),
    ("Patrol st2", 'if (st == 2 || ae_MonkeyIsIdleEndState(r) != 0) {', 'if (st == 3 || ae_MonkeyIsIdleEndState(r) != 0) {'),
    ("Patrol idleend", 'if (st == 2 || ae_MonkeyIsIdleEndState(r) != 0) {', 'if (st == 2 || ae_MonkeyIsIdleEndState(r) == 0) {'),
    ("Patrol rm", 'int32_t lim = RDS16(info2 + AE_MK_INFO_IDLE_CHANCE);\n                        wait = (int32_t)rem < lim;', 'int32_t lim = RDS16(info2 + AE_MK_INFO_IDLE_CHANCE);\n                        wait = (int32_t)rem <= lim;'),
    ("Patrol rm2", 'int32_t lim = RDS16(info2 + AE_MK_INFO_IDLE_CHANCE);\n                        wait = (int32_t)rem < lim;', 'int32_t lim = RDS16(info2 + AE_MK_INFO_IDLE_CHANCE);\n                        wait = !((int32_t)rem < lim);'),
    ("Patrol b98st", 'int start_activity = st == 2 && rand_mod100(r) < 10u;', 'int start_activity = st == 3 && rand_mod100(r) < 10u;'),
    ("Patrol b98r", 'int start_activity = st == 2 && rand_mod100(r) < 10u;', 'int start_activity = st == 2 && rand_mod100(r) < 11u;'),
    ("Patrol b98r9", 'int start_activity = st == 2 && rand_mod100(r) < 10u;', 'int start_activity = st == 2 && rand_mod100(r) < 9u;'),
    ("Patrol b98bit", 'next_state = (rnd & 1u) ? 40u : 39u;', 'next_state = (rnd & 1u) ? 39u : 40u;'),
    ("Patrol b98mask", 'next_state = (rnd & 1u) ? 40u : 39u;', 'next_state = (rnd & 2u) ? 40u : 39u;'),
    ("Patrol c14", '} else if (was_alerted != 0) {', '} else if (was_alerted == 0) {'),
    ("Patrol c14i", 'int32_t lim = RDS16(info_of(r, distance_monkey) + AE_MK_INFO_ALERT_DISTANCE);', 'int32_t lim = RDS16(info_of(r, distance_monkey) + AE_MK_INFO_SIGHT_RANGES);'),
    ("Patrol c14c", 'next_state = (lim < dist) ? 24u : 25u;', 'next_state = (lim <= dist) ? 24u : 25u;'),
    ("Patrol c14s", 'next_state = (lim < dist) ? 24u : 25u;', 'next_state = (lim < dist) ? 25u : 24u;'),
    ("Patrol c60", 'if (rem < 9u) ae_MonkeyEnterState_17(r, h);', 'if (rem < 10u) ae_MonkeyEnterState_17(r, h);'),
    ("Patrol c60b", 'if (rem < 9u) ae_MonkeyEnterState_17(r, h);', 'if (rem < 8u) ae_MonkeyEnterState_17(r, h);'),
    ("Patrol c60s", 'if (rem < 9u) ae_MonkeyEnterState_17(r, h);\n                            else ae_MonkeySetState(r, 2);', 'if (rem < 9u) ae_MonkeyEnterState_17(r, h);\n                            else ae_MonkeySetState(r, 3);'),
    ("Patrol puff", 'if (was_fast_ground_state != 0) {\n        if (!ae_MonkeyIsState0C_or_24(r)) ae_MonkeyAlertPuff(r, h);', 'if (was_fast_ground_state != 0) {\n        if (ae_MonkeyIsState0C_or_24(r)) ae_MonkeyAlertPuff(r, h);'),
    ("Patrol prev", 'WR16(cur_monkey(r) + AE_MK_PREV_NODE, 0xFFFFu);\n}\n\n/* ====', 'WR16(cur_monkey(r) + AE_MK_PREV_NODE, 0xFFFEu);\n}\n\n/* ===='),
    ("Patrol rst9", "if (st == 9 || st == 0x1A) return;\n    if (st == 0x35", "if (st == 8 || st == 0x1A) return;\n    if (st == 0x35"),
    ("Patrol rst35", "if (st == 0x35 || (RD32(c + AE_MK_FLAGS2) & 0x200u)) return;", "if (st == 0x36 || (RD32(c + AE_MK_FLAGS2) & 0x200u)) return;"),
    ("Patrol rf2", "if (st == 0x35 || (RD32(c + AE_MK_FLAGS2) & 0x200u)) return;", "if (st == 0x35 || (RD32(c + AE_MK_FLAGS2) & 0x100u)) return;"),
    ("Patrol rjump", "if (ae_MonkeyIsJumpArcState(r)) return;\n    uint32_t c = cur_monkey(r);", "if (!ae_MonkeyIsJumpArcState(r)) return;\n    uint32_t c = cur_monkey(r);"),
    ("Select hist", 'for (uint32_t link_index = 0; link_index < 6; ++link_index) WR16(sp + 16 + 2 * link_index, RD16(cm + AE_MK_NAV_NODE0 + 2 * link_index));', 'for (uint32_t link_index = 0; link_index < 5; ++link_index) WR16(sp + 16 + 2 * link_index, RD16(cm + AE_MK_NAV_NODE0 + 2 * link_index));'),
    ("Select f27", 'if ((RD32(monkey + AE_MK_FLAGS) >> 27) & 1u) {\n        uint32_t next_node = RD16', 'if ((RD32(monkey + AE_MK_FLAGS) >> 26) & 1u) {\n        uint32_t next_node = RD16'),
    ("Select goal", 'int32_t goal_node = RDS16(monkey + AE_MK_GOAL_NODE);\n        if (next_node == (uint32_t)goal_node) {', 'int32_t goal_node = RDS16(monkey + AE_MK_GOAL_NODE);\n        if (next_node != (uint32_t)goal_node) {'),
    ("Select alerted", 'if (RD8(cur_monkey(r) + AE_MK_ALERTED) == 0) {\n        int32_t link_index = s16(force_advance_bits);', 'if (RD8(cur_monkey(r) + AE_MK_ALERTED) != 0) {\n        int32_t link_index = s16(force_advance_bits);'),
    ("Select unA", 'if (ae_MonkeyChooseNextNodeA(r, h, link_index, 1) == 0) ae_MonkeyChooseNextNodeB(r, h, link_index, 1);', 'if (ae_MonkeyChooseNextNodeA(r, h, link_index, 1) != 0) ae_MonkeyChooseNextNodeB(r, h, link_index, 1);'),
    ("Select boss7", 'if (ae_AreaIsBossArena(r)) link_limit_bits = 1;', 'if (!ae_AreaIsBossArena(r)) link_limit_bits = 1;'),
    ("Select s2s", 'link_limit = s16(link_limit_bits);\n        if (ae_MonkeyChooseNextNodeA(r, h, link_index, link_limit) != 0) {', 'link_limit = s16(force_advance_bits);\n        if (ae_MonkeyChooseNextNodeA(r, h, link_index, link_limit) != 0) {'),
    ("Select A1", 'if (ae_MonkeyChooseNextNodeA(r, h, link_index, link_limit) != 0) {\n            check_backtracking = 1;', 'if (ae_MonkeyChooseNextNodeA(r, h, link_index, link_limit) == 0) {\n            check_backtracking = 1;'),
    ("Select st61", '} else if (RD8(cur_monkey(r) + AE_MK_AI_STATE) == 61) {\n            use_first_link = 1;', '} else if (RD8(cur_monkey(r) + AE_MK_AI_STATE) == 60) {\n            use_first_link = 1;'),
    ("Select Bp", '} else if (ae_MonkeyChooseNextNodeB(r, h, link_index, 1) == 0) {\n            selection_failed = 1;', '} else if (ae_MonkeyChooseNextNodeB(r, h, link_index, 1) != 0) {\n            selection_failed = 1;'),
    ("Select 10000", 'if (!(RD32(next_node * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES)) & 0x10000u) && link_limit != 1', 'if (!(RD32(next_node * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES)) & 0x20000u) && link_limit != 1'),
    ("Select s2s1", '& 0x10000u) && link_limit != 1 && ae_MonkeyNavBacktracked(r) != 0) {', '& 0x10000u) && link_limit != 0 && ae_MonkeyNavBacktracked(r) != 0) {'),
    ("Select backtr", '& 0x10000u) && link_limit != 1 && ae_MonkeyNavBacktracked(r) != 0) {', '& 0x10000u) && link_limit != 1 && ae_MonkeyNavBacktracked(r) == 0) {'),
    ("Select score1", 'ae_MonkeyScoreLinksByTarget(r, h, RDS16(cur_monkey(r) + AE_MK_NAV_NODE1));\n            if (ae_ram_failed(r)) return 0;\n            int32_t best_score', 'ae_MonkeyScoreLinksByTarget(r, h, RDS16(cur_monkey(r) + AE_MK_NAV_NODE0));\n            if (ae_ram_failed(r)) return 0;\n            int32_t best_score'),
    ("Select mark1", 'ae_MonkeyMarkBlockedLinks(r, h, RDS16(cur_monkey(r) + AE_MK_NAV_NODE1));\n            if (ae_ram_failed(r)) return 0;\n            int32_t best_index', 'ae_MonkeyMarkBlockedLinks(r, h, RDS16(cur_monkey(r) + AE_MK_NAV_NODE0));\n            if (ae_ram_failed(r)) return 0;\n            int32_t best_index'),
    ("Select node0", "if ((uint32_t)link != node0) {", "if ((uint32_t)link == node0) {"),
    ("Select hist2", 'int32_t hist = RDS16(candidate_monkey + AE_MK_PREV_NODE);\n                        if (link2 != hist) {', 'int32_t hist = RDS16(candidate_monkey + AE_MK_NAV_MARK_COUNT);\n                        if (link2 != hist) {'),
    ("Select blk", 'if (RD8(candidate_monkey + (uint32_t)link_index + AE_MK_LINK_BLOCKED) == 0) {', 'if (RD8(candidate_monkey + (uint32_t)link_index + 0x1DB) == 0) {'),
    ("Select score", 'int32_t score = RDS16(candidate_monkey + ((uint32_t)link_index << 1) + AE_MK_LINK_SCORES);', 'int32_t score = RDS16(candidate_monkey + ((uint32_t)link_index << 1) + 0x178);'),
    ("Select less", 'if (score < best_score) {\n                                    best_score = score;', 'if (score <= best_score) {\n                                    best_score = score;'),
    ("Select s5", 'best_score = score;\n                                    best_index = link_index;', 'best_score = score;\n                                    best_index = link_index + 1;'),
    ("Select s5neg", 'if (best_index != -1) {\n                chosen_index = s16((uint32_t)best_index);', 'if (best_index != 0) {\n                chosen_index = s16((uint32_t)best_index);'),
    ("Select restore", "for (uint32_t k = 0; k < 6; ++k) WR16(cur_monkey(r) + AE_MK_NAV_NODE0 + 2 * k, RD16(sp + 16 + 2 * k));", "for (uint32_t k = 0; k < 5; ++k) WR16(cur_monkey(r) + AE_MK_NAV_NODE0 + 2 * k, RD16(sp + 16 + 2 * k));"),
    ("Select s6zero", 'ae_MonkeyEnterRandomLook(r);\n                selected = 0;', 'ae_MonkeyEnterRandomLook(r);\n                selected = 1;'),
    ("Select store0", "WR16(cur_monkey(r) + AE_MK_NAV_NODE0, v);\n                ae_MonkeyUpdateGoal(r, h);", "WR16(cur_monkey(r) + AE_MK_NAV_NODE1, v);\n                ae_MonkeyUpdateGoal(r, h);"),
    ("Select b454", '        selected = 0;\n        use_first_link = RD8(cur_monkey(r) + AE_MK_AI_STATE) == 61;', '        selected = 1;\n        use_first_link = RD8(cur_monkey(r) + AE_MK_AI_STATE) == 61;'),
    ("Select b454s", '        selected = 0;\n        use_first_link = RD8(cur_monkey(r) + AE_MK_AI_STATE) == 61;', '        selected = 0;\n        use_first_link = RD8(cur_monkey(r) + AE_MK_AI_STATE) == 60;'),
    ("Select b46c", "int32_t v = ae_NavNodeGetLink(r, RDS16(cur_monkey(r) + AE_MK_NAV_NODE0), 0);", "int32_t v = ae_NavNodeGetLink(r, RDS16(cur_monkey(r) + AE_MK_NAV_NODE0), 1);"),
    ("Select arrive", 'if ((uint32_t)original_node_signed != new_current_node) {', 'if ((uint32_t)original_node_signed == new_current_node) {'),
    ("Select arrnode", 'ae_MonkeyArriveAtNode(r, h, RDS16(arrival_monkey + AE_MK_NAV_NODE1));', 'ae_MonkeyArriveAtNode(r, h, RDS16(arrival_monkey + AE_MK_NAV_NODE0));'),
    ("Select load", "ae_MonkeyLoadLinkExtra(r, RDS16(m + AE_MK_NAV_NODE1), RDS16(m + AE_MK_NAV_NODE0));", "ae_MonkeyLoadLinkExtra(r, RDS16(m + AE_MK_NAV_NODE0), RDS16(m + AE_MK_NAV_NODE1));"),
    ("Select s0e", 'uint32_t selected_byte = selected & 0xFFu;', 'uint32_t selected_byte = 1u;'),
    ("Select print", 'if ((link_limit_bits << 16) != 0 && selected_byte == 0) {', 'if ((link_limit_bits << 16) == 0 && selected_byte == 0) {'),
    ("Select printarg", 'AE_MK_HOOK(r, h, Printf, AE_G_MONKEY_SELECT_TEXT, original_node_signed, 0, 0);', 'AE_MK_HOOK(r, h, Printf, AE_G_MONKEY_SELECT_TEXT, original_node_signed + 1, 0, 0);'),
    ("Start mod3", 'if (staggered_frame != q * 2u + q) return;', 'if (staggered_frame != q * 2u + q + 1u) return;'),
    ("Start id", 'uint32_t staggered_frame = RD32(AE_G_FRAME_COUNTER) + RD8(m + AE_MK_ID);', 'uint32_t staggered_frame = RD32(AE_G_FRAME_COUNTER) + RD8(m + AE_MK_SLOT);'),
    ("Start sight", "if (ae_MonkeyUpdateSight(r, h, 1) == 0) return;", "if (ae_MonkeyUpdateSight(r, h, 0) == 0) return;"),
    ("Start special", "if (ae_AreaIsSpecialMonkeyArea(r) == 0) {\n            uint32_t c = cur_monkey(r);", "if (ae_AreaIsSpecialMonkeyArea(r) != 0) {\n            uint32_t c = cur_monkey(r);"),
    ("Start bit6", "if ((flagw >> 6) & 1u) notice = 1;", "if ((flagw >> 7) & 1u) notice = 1;"),
    ("Start alerted", 'WR8(cur_monkey(r) + AE_MK_ALERTED, 1);            /* Mark alerted before starting the notice animation. */', 'WR8(cur_monkey(r) + AE_MK_ALERTED, 2);            /* Mark alerted before starting the notice animation. */'),
    ("Start voice", "ae_MonkeyQueueVoice(r, h, voice, 0);\n            ae_MonkeyAlertedDecide(r, h);\n        }\n        return;\n    }\n    uint32_t x", "ae_MonkeyQueueVoice(r, h, voice, 1);\n            ae_MonkeyAlertedDecide(r, h);\n        }\n        return;\n    }\n    uint32_t x"),
    ("Start div20", "uint32_t rem = x - q * 20u;", "uint32_t rem = x - q * 21u;"),
    ("Start q", "uint32_t q = mulu_hi(x, 0xCCCCCCCDu) >> 4;", "uint32_t q = mulu_hi(x, 0xCCCCCCCDu) >> 3;"),
    ("Start t10", 'WR16(activity_monkey + AE_MK_IDLE_ACTIVITY_TIMER, rem + 10u);', 'WR16(activity_monkey + AE_MK_IDLE_ACTIVITY_TIMER, rem + 11u);'),
    ("Start t98", 'WR16(activity_monkey + AE_MK_IDLE_ACTIVITY_TIMER, rem + 10u);', 'WR16(activity_monkey + AE_MK_IDLE_TIMER, rem + 10u);'),
    ("Start 16", "if (st == 16) ns = 17;", "if (st == 15) ns = 17;"),
    ("Start 13", "if (st == 13) ns = 14;", "if (st == 12) ns = 14;"),
    ("Start 19", "} else if (st == 19) ns = 20;", "} else if (st == 18) ns = 20;"),
    ("Start 58", "else if (st == 58) ns = 59;", "else if (st == 57) ns = 59;"),
    ("Start 13s", "if (st == 13) ns = 14;", "if (st == 13) ns = 15;"),
    ("Loop dec", 'uint32_t timer = RD16(monkey + AE_MK_IDLE_ACTIVITY_TIMER) - 1u;', 'uint32_t timer = RD16(monkey + AE_MK_IDLE_ACTIVITY_TIMER) - 2u;'),
    ("Loop mask", 'if ((timer_bits & 7u) == 0) {', 'if ((timer_bits & 3u) == 0) {'),
    ("Loop p17", "if (st == 17) pattern = 4;", "if (st == 17) pattern = 3;"),
    ("Loop p14", "pattern = (st == 14) ? 2u : 0u;", "pattern = (st == 14) ? 1u : 0u;"),
    ("Loop p20", "else if (st == 20) pattern = 3;", "else if (st == 20) pattern = 2;"),
    ("Loop p59", "else if (st == 59) pattern = 1;", "else if (st == 59) pattern = 0;"),
    ("Loop sight", "if (ae_MonkeyUpdateSight(r, h, pattern) == 0) return;", "if (ae_MonkeyUpdateSight(r, h, pattern) != 0) return;"),
    ("Loop special", 'WR8(cur_monkey(r) + AE_MK_ALERTED, 1);                /* Store before the following call reads the monkey. */\n        if (ae_AreaIsSpecialMonkeyArea(r)) {', 'WR8(cur_monkey(r) + AE_MK_ALERTED, 1);                /* Store before the following call reads the monkey. */\n        if (!ae_AreaIsSpecialMonkeyArea(r)) {'),
    ("Loop fin", "if (ae_MonkeyAnimFinished(r) == 0) {\n            ae_MonkeyPickAnim(r);", "if (ae_MonkeyAnimFinished(r) != 0) {\n            ae_MonkeyPickAnim(r);"),
    ("Loop 17", "if (st == 17) ns = 18;", "if (st == 16) ns = 18;"),
    ("Loop 14", "ns = (st == 14) ? 15u : 60u;", "ns = (st == 14) ? 16u : 60u;"),
    ("Loop 20", "else ns = (st == 20) ? 21u : 60u;", "else ns = (st == 20) ? 22u : 60u;"),
    ("Loop 60", "else ns = (st == 20) ? 21u : 60u;", "else ns = (st == 20) ? 21u : 61u;"),
    ("Flag fin", "if (ae_MonkeyAnimFinished(r) != 0) return;", "if (ae_MonkeyAnimFinished(r) == 0) return;"),
    ("Flag 2000", 'if (RD32(node_index_narrow * AE_NAV_NODE_SIZE + nodes) & 0x2000u) {', 'if (RD32(node_index_narrow * AE_NAV_NODE_SIZE + nodes) & 0x4000u) {'),
    ("Flag lt", 'if ((d & 0xFFFFu) < (best_distance & 0xFFFFu)) {', 'if ((d & 0xFFFFu) <= (best_distance & 0xFFFFu)) {'),
    ("Flag s2", '                    best_node = node_index;\n                    best_distance = d;', '                    best_node = node_index + 1;\n                    best_distance = d;'),
    ("Flag pos", 'uint32_t d = (uint32_t)ae_Distance3D(r, cur_monkey(r) + AE_MK_POS, sp + 16);', 'uint32_t d = (uint32_t)ae_DistanceXZ(r, cur_monkey(r) + AE_MK_POS, sp + 16);'),
    ("Flag link", "int32_t link = ae_NavNodeGetLink(r, node, 0);", "int32_t link = ae_NavNodeGetLink(r, node, 1);"),
    ("Flag n1", 'WR16(monkey + AE_MK_NAV_NODE1, link);                /* Store before the following call reads the monkey. */', 'WR16(monkey + AE_MK_NAV_NODE0, link);                /* Store before the following call reads the monkey. */'),
    ("Flag load", 'ae_MonkeyLoadLinkExtra(r, s16((uint32_t)link), current_node);', 'ae_MonkeyLoadLinkExtra(r, current_node, s16((uint32_t)link));'),
    ("Flag arrive", "if (ae_MonkeyArriveAtNode(r, h, node) != 0) return;", "if (ae_MonkeyArriveAtNode(r, h, node) == 0) return;"),
]

MUT_FUNCS = {
    "Patrol": ["MonkeyPatrolDecide"], "Select": ["MonkeySelectNextNode"], "Start": ["MonkeyAi_IdleActivityStart"],
    "Loop": ["MonkeyAi_IdleActivityLoop"], "Flag": ["MonkeyAi_GoToFlaggedNode"],
}


def run_mutations(scale, seed, only_labels=()):
    rel = "src/monkeys/ae_monkey_level7.c"
    text = (ROOT / rel).read_text(encoding="utf-8")
    survivors = []
    tmp = Path(tempfile.mkdtemp(prefix="mk7_mut_"))
    h = mh.MonkeyHarness(FUNCS)
    for k, (label, old, new) in enumerate(MUTATIONS):
        if only_labels and not any(s in label for s in only_labels):
            continue
        if old not in text:
            print(f"MUTATION {label!r}: pattern not found")
            survivors.append(label + " (pattern missing)")
            continue
        mp = tmp / f"mut{k}_{Path(rel).name}"
        mp.write_text(text.replace(old, new, 1), encoding="utf-8")
        try:
            h.reload_c({rel: mp}, tag=f"mk7mut{k}")
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
    ap.add_argument("--cases", type=float, default=10, help="scale of the case counts in percent")
    ap.add_argument("--seed", type=int, default=0x944271)
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--trapv", action="store_true", help="build the port with -ftrapv (signed overflow aborts)")
    ap.add_argument("--mutate", action="store_true", help="mutation-test the checker (slow)")
    ap.add_argument("--mutate-only", nargs="*", default=[], help="only the mutations whose label contains one of these words")
    ap.add_argument("--stop-after", type=int, default=0)
    a = ap.parse_args()
    if not (ROOT / "extract/disc/SCUS_944.23").exists():
        print("SKIP: extract/disc/SCUS_944.23 not present")
        return 0
    if a.mutate:
        survivors = run_mutations(max(a.cases, 100), a.seed, a.mutate_only)   # mutations need the larger case counts
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
