"""Differential check of monkey port batch 5b (src/monkeys/ae_monkey_level9.c and ae_monkey_level9_patrol.c, call-graph level 9) against
the original MIPS code: for every ported function many scenarios are built in a byte-exact RAM image (SCUS_944.23 at its real addresses;
monkeys / nav nodes / extra link records / MonkeyInfo / animation tables / patrol routes / globals laid out as MonkeySpawnAll and the level
loader leave them, plus randomised and edge-case states); the original instructions (GTE through the PCSX-ReARMed oracle) and the C port run
on identical images with identical hook behaviour; return value, RAM outside the stack window, scratchpad and the sequence of hook calls must
be identical.  The batch 1..8 functions the code calls run as the C ports on the C side and as the original instructions on the MIPS side.
Needs extract/disc/SCUS_944.23 (skips if missing).  See check_monkey_level8.py and monkey_scen5.py.

Usage: uv run python tools/mips/check_monkey_level9.py [--cases PERCENT] [--seed S] [--coverage] [--trapv] [--mutate]
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
import monkey_scen5 as s5  # noqa: E402
from monkey_harness import W, MONKEYS, MONKEY_SIZE, NAV_BASE  # noqa: E402
from monkey_scen import M32, s32, s16, FRAME, G_TARGET, STATE_TABLE  # noqa: E402
from monkey_scen5 import common, move_fields, anim_cycle, in_sight, seed_pair, G_FRAME2, PATROL_STATES, MOVE_STATES  # noqa: E402

# name -> (original address, size, return kind: s = int32 / u = uint32 / v = void, takes hooks, argument kinds[, C name])
FUNCS = {
    "MonkeyAi_State3D": (0x8003FB44, 196, "v", True, ""),
    "MonkeyAi_JumpArc": (0x8003F530, 828, "v", True, ""),
    "MonkeyAi_JumpArcPhased": (0x8003F86C, 728, "v", True, ""),
    "MonkeyAi_JumpArcB": (0x8003FD7C, 688, "v", True, ""),
    "MonkeyAi_Patrol": (0x8003DC7C, 5392, "v", True, ""),
    "MonkeySelectActive": (0x800426C4, 1188, "v", True, ""),
}
EXTRA_COVERAGE = {
    "MonkeyInit (batch 5a)": (0x8003A414, 2112),
    "MonkeyDecideAction (batch 5a)": (0x800378C0, 72),
    "MonkeyAlertedDecide (batch 4c)": (0x80037550, 880), "MonkeyPatrolDecide (batch 4d)": (0x80037908, 1124),
    "MonkeySelectNextNode (batch 4d)": (0x8003B074, 1216), "MonkeyNodeActionDispatch (batch 4b)": (0x80036CD0, 920),
    "MonkeyArriveAtNode (batch 4a)": (0x80040D90, 1264), "MonkeySetNavNode (batch 4a)": (0x80035080, 236),
    "MonkeyUpdateSight (batch 3a)": (0x800405C4, 0x1A8), "MonkeyReachedNode (batch 1)": (0x8003516C, 516),
    "MonkeyAnimFinished (batch 1)": (0x80033A3C, 0x90), "MonkeyStartNotice (batch 3b)": (0x800357E0, 60),
    "MonkeyUpdateGoal (batch 3b)": (0x800355DC, 40), "MonkeyEnterState_2A (batch 3a)": (0x80036460, 0x5C),
    "MonkeyGetWaypointPos (batch 3a)": (0x80040CFC, 0x94), "MonkeyAdvanceAnimFrame (batch 2)": (0x800330C8, 0xE8),
    "MonkeyPickAnim (batch 2)": (0x800331B0, 112), "MonkeyFireProjectile6 (batch 2)": (0x80033DF0, 0x5C),
    "MonkeyAimAtTarget (batch 2)": (0x80033E4C, 0x450), "MonkeyFireProjectile7 (batch 2)": (0x800343B0, 0x5C),
    "MonkeyFireProjectile8 (batch 2)": (0x8003440C, 0x70), "MonkeySpawnProjectile (batch 1)": (0x80033CCC, 0x124),
    "MonkeyPickAttackA (batch 3b)": (0x8003738C, 0x94), "MonkeyTryAttackKind (batch 3a)": (0x8003710C, 0x280),
    "MonkeyAttackChance (batch 1)": (0x80037068, 0xA4), "MonkeyTurnTowardTarget (batch 2)": (0x8003CC3C, 0xE8),
    "NavNodeIsSpecial (batch 1)": (0x800349F4, 0x38), "MonkeyPickGoalNode (batch 2)": (0x8003AD9C, 728),
    "MonkeyResetNavVisits (batch 1)": (0x800407F0, 0x7C), "MonkeyBodyUnregister (batch 1)": (0x80033068, 96),
    "Distance3D (batch 1)": (0x80034684, 0xB8), "MonkeyGetCallVoice2 (batch 1)": (0x80035750, 0x90),
    "MonkeyGetCallVoice (batch 1)": (0x800356C0, 0x90), "MonkeyNop (batch 1)": (0x80033000, 8),
}
DISPATCH_BITS = [0x100, 0x800, 0x20, 0x20000, 8, 4, 0x10, 1, 0x10000, 0x800000, 0x1000000]


def check_all(h, scale, seed):
    t0 = time.time()
    base_mask = [(MONKEYS + MONKEY_SIZE * k + 0xC8 - 0x80000000, 4) for k in range(8)]   # see check_monkey_level4.py
    init_mask = base_mask + [(MONKEYS + MONKEY_SIZE * k + 0x214 - 0x80000000, 8) for k in range(8)]    # see MonkeyInit (check_monkey_level8.py)
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

    # ------------------------------------------------------------------ State3D
    def st3d_world(m, rng, i):
        h.mask = base_mask
        c = common(m, rng)
        w = W(m)
        w.w8(c + 0x78, rng.choice([0x3D, 0x3D, rng.randint(0, 0x42)]))
        move_fields(m, rng, c)
        w.w16(c + 0x96, rng.choice([0, 28, 29, 30, 31, 0x7FFF, 0x8000, 0xFFFF, rng.getrandbits(16)]))
        rad = s16(m.read16(c + 0x220))
        if rng.random() < 0.5:                  # reached
            w.w32(c + 0x214, (rad - 1) & M32)
            w.w32(c + 0x218, (rad - 1) & M32)
        return c
    run("MonkeyAi_State3D", 800, st3d_world, nh=True, allow_trap=True)

    def st3d_reach_world(m, rng, i):
        h.mask = base_mask
        c = s5.select_world(m, rng, i)
        w = W(m)
        w.w8(c + 0x78, 0x3D)
        move_fields(m, rng, c)
        w.w8(c + 0x79, rng.choice([0, 1, 1]))
        w.w16(c + 0x96, rng.choice([0, 28, 29, 30, 31]))
        rad = s16(m.read16(c + 0x220))
        w.w32(c + 0x214, (rad - 1) & M32)
        w.w32(c + 0x218, (rad - 1) & M32)
        w.w32(c + 0x228, m.read32(c + 0x228) & ~(0x100 | 0x800 | 0x4) & M32)
        return c
    run("MonkeyAi_State3D", 600, st3d_reach_world, nh=True, allow_trap=True, i0=20000)

    # ------------------------------------------------------------------ the jump arcs
    def arc_world(m, rng, i, states, anim=True):
        h.mask = base_mask
        c = common(m, rng)
        w = W(m)
        st = rng.choice(states)
        w.w8(c + 0x78, st)
        move_fields(m, rng, c)
        dur = rng.choice([0, 1, 2, 10, 20, 30, 60, rng.randint(0, 200), 0xFFFF, 0x8000, 7, 8])
        w.w16(c + 0x128, dur)
        cnt = rng.choice([dur // 2 - 1, dur // 2, dur // 2 + 1, dur - 1, dur, dur + 1, 0, rng.randint(0, 100), -1, 0x7FFF, 0x8000,
                          dur - (rng.randint(0, 40) // 2) - 1, dur - (rng.randint(0, 40) // 2), rng.getrandbits(16)])
        w.w16(c + 0x96, cnt & 0xFFFF)
        u170 = rng.choice([0, 1, 2, 4, 10, 30, 41, rng.getrandbits(16)])
        u172 = rng.choice([0, 1, 4, 10, 30, rng.getrandbits(16)])
        w.w16(c + 0x170, u170)
        w.w16(c + 0x172, u172)
        for off in (0x13C, 0x140, 0x144):
            w.w32(c + off, rng.choice([0, 0x1000, -0x1000, 0x12345, -0x54321, rng.randint(-0x20000, 0x20000), rng.getrandbits(32)]) & M32)
        for off in (0x12C, 0x130, 0x134):
            w.w32(c + off, rng.choice([0, 100, -100, rng.randint(-5000, 5000), rng.getrandbits(32)]) & M32)
        t = s16(cnt & 0xFFFF)
        if t % 2 and rng.random() < 0.3:
            # (vy * t) negative and = 1 mod 4096: the rounding toward zero (+ 4095) of the vertical term is exercised at its edge
            base = pow(t % 4096, -1, 4096)
            cands = [base + 4096 * j for j in range(-8, 9) if (base + 4096 * j) * t < 0]
            if cands:
                w.w32(c + 0x140, rng.choice(cands) & M32)
        if anim:
            # frame near the thresholds of the arc animation: first + u172, last, last - 1 ...
            anim_cycle(m, rng, c, "rand")
            idx = sc.anim_index(m, c)
            if 0 <= idx < 128:
                first = m.read16(sc.ANIM_BASE + 8 * idx)
                last = m.read16(sc.ANIM_BASE + 8 * idx + 2)
                w.w16(c + 0x5C, rng.choice([first + u172 - 1, first + u172, first + u172 + 1, last - 1, last, last + 1, first, first + 1,
                                            rng.getrandbits(16)]) & 0xFFFF)
        return c
    def arc_end_world(m, rng, i, states):
        """the counter is past the duration: the landing and the node selection (the world of MonkeySelectNextNode)"""
        h.mask = base_mask
        c = s5.select_world(m, rng, i)
        w = W(m)
        w.w8(c + 0x78, rng.choice(states))
        dur = rng.choice([0, 5, 30, 60])
        w.w16(c + 0x128, dur)
        w.w16(c + 0x96, rng.choice([dur, dur + 1, dur + 5, 0x7FFF, dur]) & 0xFFFF)
        return c
    def arc_noexit_world(m, rng, i, states):
        """the arc end where MonkeySelectNextNode finds no node (the node has no links): the state test of the fallback"""
        c = arc_end_world(m, rng, i, states)
        w = W(m)
        nn = m.read16(0x800B4D18)
        for q in (m.read16(c + 0xF2), m.read16(c + 0xF4)):
            if rng.random() < 0.8:
                w.w8(NAV_BASE + 0x18 * (q % nn) + 0xB, 0)
        return c
    run("MonkeyAi_JumpArc", 2500, lambda m, rng, i: arc_world(m, rng, i, [4, 4, 0x1D, 0x1D, 3, rng.randint(0, 0x42)]),
        nh=True, allow_trap=True)
    run("MonkeyAi_JumpArcB", 2000, lambda m, rng, i: arc_world(m, rng, i, [0x31, 0x31, 0x1D, 4, rng.randint(0, 0x42)]),
        nh=True, allow_trap=True)
    run("MonkeyAi_JumpArcPhased", 2500, lambda m, rng, i: arc_world(m, rng, i, [0x2C, 0x2D, 0x2E, 0x2D, 0x2E, 0x1D, 0x2F, rng.randint(0, 0x42)]),
        nh=True, allow_trap=True)

    run("MonkeyAi_JumpArc", 1500, lambda m, rng, i: arc_end_world(m, rng, i, [4, 0x1D, 0x1D, 3, 9, 0x3D, 0x1A, 0x35, 0xC, 0x24, 0]),
        nh=True, allow_trap=True, i0=20000)
    run("MonkeyAi_JumpArcB", 1500, lambda m, rng, i: arc_end_world(m, rng, i, [0x31, 0x1D, 3, 9, 0x3D, 0x1A, 0x35, 0xC, 0x24, 0]),
        nh=True, allow_trap=True, i0=20000)
    run("MonkeyAi_JumpArcPhased", 1500, lambda m, rng, i: arc_end_world(m, rng, i, [0x2E, 0x2E, 0x2D, 0x2C, 0x1D, 3, 9, 0x3D, 0x1A, 0x35, 0xC, 0x24, 0]),
        nh=True, allow_trap=True, i0=20000)

    NOEXIT = [0x3D, 0x3D, 9, 0x1A, 0x35, 0xC, 0x24, 3, 0]
    run("MonkeyAi_JumpArc", 600, lambda m, rng, i: arc_noexit_world(m, rng, i, NOEXIT), nh=True, allow_trap=True, i0=40000)
    run("MonkeyAi_JumpArcB", 600, lambda m, rng, i: arc_noexit_world(m, rng, i, NOEXIT), nh=True, allow_trap=True, i0=40000)
    run("MonkeyAi_JumpArcPhased", 600, lambda m, rng, i: arc_noexit_world(m, rng, i, NOEXIT), nh=True, allow_trap=True, i0=40000)

    # ------------------------------------------------------------------ MonkeyAi_Patrol
    NODE_BITS = [0x400, 0x40, 0x10000, 0x8000, 4, 8, 0x200, 0x1]

    def patrol_world(m, rng, i, kind="any", state=None, b3=None, n1_set=0, n1_clear=0, att=False, anim=None):
        h.mask = base_mask
        c = common(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        st = state if state is not None else rng.choice(PATROL_STATES + [rng.randint(0, 0x42)])
        w.w8(c + 0x78, st)
        move_fields(m, rng, c)
        n0, n1 = m.read16(c + 0xF2) % nn, m.read16(c + 0xF4) % nn
        w.w16(c + 0xF2, n0)
        w.w16(c + 0xF4, n1)
        for node, bone in ((n0, None), (n1, kind in ("bone",))):
            a = NAV_BASE + 0x18 * node
            f = m.read32(a) & ~sum(NODE_BITS) & M32
            for b in NODE_BITS:
                if rng.random() < 0.25:
                    f |= b
            if bone is True:
                f |= 0x400
            elif bone is False and kind in ("plain", "idle", "fin", "unfin"):
                f &= ~0x400 & M32
            if node == n1:
                f = (f | n1_set) & ~n1_clear & M32
            w.w32(a, f)
            w.w16(a + 0x10, rng.choice([rng.randint(0, 15) * 100 + rng.randint(0, 99), rng.randint(-300, 3000), rng.getrandbits(16)]) & 0xFFFF)
        w.w8(c + 0x79, rng.choice([0, 1, 1]))
        w.w8(c + 0x222, rng.choice([0, 1, 2, 0x7F, 0x80, 0xFF, rng.getrandbits(8)]))
        w.w16(c + 0x9A, rng.choice([0, 1, 2, 0x8000, 0xFFFF, rng.getrandbits(16)]) & 0xFFFF)
        w.w16(c + 0x22C, rng.choice([0, 1, rng.getrandbits(16)]))
        w.w32(c + 0x248, m.read32(c + 0x248) & ~((1 << 19) | (1 << 23)) & M32 | (rng.choice([0, 1]) << 19) | (rng.choice([0, 1]) << 23))
        w.w32(c + 0x21C, rng.choice([0, 341, 342, 343, 1705, 1706, 1707, 2389, 2390, 2391, 3754, 3755, 739, 740, 3356, 3357, 4000, rng.randint(0, 4096),
                                      1365, 1366, 1367, 2730, 2731, 2732, 3756, 4095, 4096, 4097, 4098]))
        w.w8(c + 0xE3, rng.choice([0, 1, 5, 20, 255]))
        w.w16(c + 0xE4, rng.choice([0, 100, -100, rng.getrandbits(16)]) & 0xFFFF)
        flagw = m.read32(STATE_TABLE + 8 * (st % 0x43) + 4)
        b3 = (rng.choice([0, 1, 1]) if kind != "unfin" else rng.choice([0, 1])) if b3 is None else b3
        w.w32(STATE_TABLE + 8 * (st % 0x43) + 4, (flagw | 8) if b3 else (flagw & ~8 & M32))
        anim_cycle(m, rng, c, anim or (rng.choice(["run5", "past", "past", "rand"]) if kind in ("any", "bone") else (
            "run5" if kind == "fin" else ("past" if kind == "unfin" else rng.choice(["run5", "past"])))))
        # distances / ranges / area
        dist = rng.choice([0, 199, 200, 201, 299, 300, 301, 1500, 1501, 1999, 2000, 1500 + 600, rng.randint(0, 3000), rng.getrandbits(32)]) & M32
        w.w32(c + 0x20C, dist)
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        d = s32(dist)
        for off in (0x26, 0x1E, 0x46, 0x3E, 0x36, 0x56):
            w.w16(info + off, rng.choice([0, 1, 1, 1, rng.getrandbits(16)]))
        for lo_off in (0x2A, 0x22, 0x4A, 0x42, 0x3A, 0x5A):
            w.w16(info + lo_off, rng.choice([d - 1, d, d + 1, 0, -5, rng.getrandbits(16)]) & 0xFFFF)
            w.w16(info + lo_off + 2, rng.choice([d - 1, d, d + 1, 5000, rng.getrandbits(16)]) & 0xFFFF)
        for k in range(8):
            w.w16(info + 12 + 2 * k, rng.choice([d + 1, d - 1, d, 3000, 0, rng.getrandbits(16)]) & 0xFFFF)
        w.w16(info + 0x1A, rng.choice([d + 1, 0, rng.getrandbits(16)]) & 0xFFFF)
        w.w16(0x800F4472, rng.choice([0, 0, 0, 1, 3, 6, 8, 2]))
        w.w8(0x800F447C, rng.choice([3, 3, 0, 1, rng.getrandbits(8)]))
        if att:
            # the attack states: the info record allows the attack (range around the distance), the counter is positive
            w.w8(c + 0x222, rng.choice([1, 2, 5, 0x7F]))
            for off in (0x26, 0x1E, 0x46, 0x3E, 0x36, 0x56):
                w.w16(info + off, rng.choice([1, 1, 1, 7, 0]))
            for lo_off in (0x2A, 0x22, 0x4A, 0x42, 0x3A, 0x5A):
                w.w16(info + lo_off, (d - rng.choice([0, 0, 1, 5, 100])) & 0xFFFF)
                w.w16(info + lo_off + 2, (d + rng.choice([0, 0, 1, 5, 100])) & 0xFFFF)
            w.w32(c + 0x228, m.read32(c + 0x228) & ~0x2 & M32)
        # frame counter: multiples of 300 / 5 / 3 / 150
        fr = rng.choice([rng.getrandbits(32), rng.randint(0, 1000) * 300, rng.randint(0, 1000) * 5, rng.randint(0, 1000) * 150 - 3,
                         rng.randint(0, 1000) * 3, rng.randint(0, 100000)])
        if rng.random() < 0.2:                  # (frame + id + 3) % 150 == 0 or (frame + id) % 3 == 0
            idv = m.read8(c + 0x7C)
            fr = rng.randint(1, 1000) * 150 - 3 - idv if rng.random() < 0.5 else rng.randint(1, 1000) * 3 - idv
        w.w32(FRAME, fr & M32)
        w.w32(0x800B4D10, rng.choice([rng.getrandbits(16), s4.seed_for(rng, lambda v: v % 20 == 0), s4.seed_for(rng, lambda v: v % 2000 < 700)]))
        # the 'attacking' animation hits: frame - first = 10 / 25 / 3 / 7 (mod 10)
        idx = sc.anim_index(m, c)
        if 0 <= idx < 128 and (rng.random() < 0.5 or att):
            first = m.read16(sc.ANIM_BASE + 8 * idx)
            last = m.read16(sc.ANIM_BASE + 8 * idx + 2)
            fo = rng.choice([10, 25, 3, 7, 13, 17, 1, 0] + ([10, 25, 3, 7, 10, 25, 13, 17] if att else []))
            if att and rng.random() < 0.8:
                fo = {28: 10, 27: 25, 55: rng.choice([3, 7, 13, 17, 23, 27])}.get(st, fo)
            w.w16(c + 0x5C, first + fo)
            if rng.random() < 0.8:
                w.w16(sc.ANIM_BASE + 8 * idx + 2, max(last, first + fo + rng.choice([0, 1, 5])))
        if rng.random() < 0.15:
            w.w16(0x800AEB6C + 2 * rng.randint(0, 5), rng.getrandbits(16))
        return c

    def kind_runner(kind, count, i0=0, states=None):
        run("MonkeyAi_Patrol", count, lambda m, rng, i: patrol_world(m, rng, i, kind, rng.choice(states) if states else None),
            nh=True, allow_trap=True, i0=i0)
    kind_runner("any", 2500)
    kind_runner("bone", 1500, 10000)
    kind_runner("plain", 1200, 20000)
    kind_runner("idle", 700, 30000, states=[1])
    kind_runner("fin", 2500, 40000, states=[0x22, 0x21, 0x38, 0x1C, 0x1B, 0x37, 1, 0xB, 0x22, 0x37, 0, 0x26, 0x29])
    ATT = [0x22, 0x21, 0x38, 0x1C, 0x1B, 0x37]
    run("MonkeyAi_Patrol", 2000, lambda m, rng, i: patrol_world(m, rng, i, rng.choice(["bone", "plain", "fin"]), rng.choice(ATT), b3=1, att=True,
                                                               n1_clear=0x10000, anim="run5"), nh=True, allow_trap=True, i0=60000)
    run("MonkeyAi_Patrol", 800, lambda m, rng, i: patrol_world(m, rng, i, "bone", rng.choice([0x2A, 0x2B, 0x29, 0x2A, 0x2A]), b3=0,
                                                               n1_set=0x400, anim=rng.choice(["run5", "past", "rand"])), nh=True, allow_trap=True, i0=70000)
    run("MonkeyAi_Patrol", 1500, lambda m, rng, i: patrol_world(m, rng, i, "unfin", rng.choice([0x1C, 0x37, 0x21, 0x38, 0x1B, 0x22, 0, 0x26, 0x27, 0x17, 0xB, 1]),
                                                               b3=0, n1_clear=0x400 | 0x10000 if rng.random() < 0.7 else 0x400, anim="past",
                                                               n1_set=rng.choice([0, 0, 4, 8, 0x8000, 0x10000, 0x8008, 0x10004])), nh=True, allow_trap=True, i0=80000)
    run("MonkeyAi_Patrol", 1000, lambda m, rng, i: patrol_world(m, rng, i, "unfin", 0xB, n1_clear=0x400 | 0x10000, anim="past"),
        nh=True, allow_trap=True, i0=90000)
    def pick_attack_world(m, rng, i):
        c = patrol_world(m, rng, i, "bone", rng.choice([0x29, 0x2A, 0x1C, 0x22, 0x21, 0x37]), b3=1, n1_set=0x400, anim="past")
        w = W(m)
        w.w32(c + 0x248, m.read32(c + 0x248) | (1 << 23))
        w.w32(STATE_TABLE + 8 * (m.read8(c + 0x78) % 0x43) + 4, m.read32(STATE_TABLE + 8 * (m.read8(c + 0x78) % 0x43) + 4) | 4)
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        for k in range(8):
            e = info + 8 * k
            w.w16(e + 0x1E, rng.choice([1, 2, 3]))
            w.w16(e + 0x20, rng.choice([100, 100, 100, 50]))
            w.w16(e + 0x22, 0)
            w.w16(e + 0x24, 5000)
            w.w32(0x800AEBB8 + 8 * k, rng.choice([1, 7, 3, 5, 1]))
            w.w8(0x800AEBB4 + 8 * k, rng.choice([1, 2, 3]))
            w.w8(0x800AEBB5 + 8 * k, rng.choice([8, 0x1E, rng.randint(0, 0x42)]))
        w.w16(c + 0x174, rng.choice([0, 0, 1]))
        w.w32(c + 0x20C, rng.randint(0, 2000))
        w.w8(c + 0x222, rng.choice([0, 0, 0x80, 0xFF]))     # not positive: the animation is not restarted at the top (state 0x29 instead)
        w.w8(c + 0xD4, 0)
        w.w16(0x800EC2CA, 0)
        w.w32(0x800B4D10, rng.getrandbits(16))
        return c
    run("MonkeyAi_Patrol", 800, pick_attack_world, nh=True, allow_trap=True, i0=95000)
    kind_runner("unfin", 2500, 50000, states=[0x37, 0x27, 0x28, 0x17, 0x18, 0x29, 1, 0xB, 0x1C, 0x21, 0x22, 0x38, 0x1B, 0, 0x16, 0x26, 0x2A, 0x2B, 0x42])

    # ------------------------------------------------------------------ MonkeySelectActive
    def active_world(m, rng, i):
        h.mask = init_mask
        c = common(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        cnt = rng.choice([8, 8, 4, 3, 1, 5, 0, 8, 2])
        w.w16(0x800AEB68, cnt)
        w.w16(0x800AEB66, rng.choice([4, 4, 3, 2, 1, 0, 4, 5]))
        for k in range(8):
            a = MONKEYS + MONKEY_SIZE * k
            w.w8(a + 0x7A, rng.choice([0, 1, 2, 3, 4, 4, 5, 0]))
            w.w8(a + 0x7C, rng.randint(0, 15))
            w.w8(a + 0x7B, rng.randint(0, 7))
            w.w16(a + 0xE6, rng.choice([-1, -1, 0, rng.randint(0, max(0, nn - 1)), rng.getrandbits(16)]) & 0xFFFF)
            w.w8(a + 0xD8, rng.getrandbits(8))
            w.w32(a + 0x210, rng.choice([0, 100, rng.randint(0, 5000), rng.getrandbits(32), -1000 & M32, -1001 & M32]) & M32)
            for off in (0x24, 0x28, 0x2C):
                w.w32(a + off, rng.choice([rng.randint(-3000, 3000), rng.getrandbits(32)]) & M32)
            w.w8(a + 0xD6, rng.choice([0, 0, 0, 1, 2]))
            w.w32(a + 0x228, m.read32(a + 0x228) & ~(0x100 | 0x800 | 0x4) & M32)     # no entries that read leftover stack (see common)
        if rng.random() < 0.4:                  # monkeys at the same place: equal distances to the player (the 'nearer' test of the list)
            for _ in range(rng.randint(1, 4)):
                src, dst = (MONKEYS + MONKEY_SIZE * rng.randint(0, 7) for _ in range(2))
                for off in (0x24, 0x28, 0x2C):
                    w.w32(dst + off, m.read32(src + off))
        # the active list: monkeys of the table, or empty slots
        for k in range(4):
            w.w32(0x800E54F0 + 4 * k, rng.choice([0, 0] + [MONKEYS + MONKEY_SIZE * q for q in range(8)]))
        ply = 0x800EC1E0
        for off in (0x24, 0x28, 0x2C):
            w.w32(ply + off, rng.choice([rng.randint(-3000, 3000), 0, rng.getrandbits(32)]) & M32)
        # MonkeyInit / PickGoalNode world: route table, node flags
        for k in range(nn):
            a = NAV_BASE + 0x18 * k
            if rng.random() < 0.2:
                w.w32(a, (m.read32(a) & ~0x440) & M32)
        w.w32(0x800B4D0C, sc.ROUTE_BASE)
        import struct
        data = bytearray()
        for k in range(0x200):
            data += struct.pack("<h", rng.choice([-1, -1, rng.randint(0, max(0, nn - 1)), rng.randint(0, 300)]))
        m.write_bytes(sc.ROUTE_BASE, bytes(data))
        s4.route_fields(m, rng, c, nn)
        w.w32(0x800B4D0C, sc.ROUTE_BASE)
        w.w8(0x800F44BC + 0, rng.choice([5, 4, 0]))
        return c
    run("MonkeySelectActive", 2500, active_world, nh=True, allow_trap=True)

    print(f"scenario families done in {time.time() - t0:.1f}s")


MUTATIONS = [
    ("State3D reach", "if (ae_MonkeyReachedNode(r) != 0) {\n        ae_MonkeySelectNextNode(r, h, 1, 1);", "if (ae_MonkeyReachedNode(r) == 0) {\n        ae_MonkeySelectNextNode(r, h, 1, 1);"),
    ("State3D sel", "ae_MonkeySelectNextNode(r, h, 1, 1);\n        ae_MonkeyDecideAction(r, h);", "ae_MonkeySelectNextNode(r, h, 1, 0);\n        ae_MonkeyDecideAction(r, h);"),
    ('State3D 30', 'if (counter < 30) WR16', 'if (counter < 31) WR16'),
    ('State3D f2', 'if (RD32(monkey + AE_MK_FLAGS2) & 2u) {', 'if (RD32(monkey + AE_MK_FLAGS2) & 4u) {'),
    ('Arc half', 'if (t < (int32_t)(duration >> 1)) {', 'if (t < (int32_t)(duration >> 2)) {'),
    ('Arc 172', 'uint32_t rise_frames = RD16(monkey + AE_MK_JUMP_RISE_FRAMES);', 'uint32_t rise_frames = RD16(monkey + AE_MK_JUMP_LAND_FRAMES);'),
    ('Arc lt', 'if ((int32_t)frame < (int32_t)(first_frame + rise_frames)) {', 'if ((int32_t)frame <= (int32_t)(first_frame + rise_frames)) {'),
    ('Arc 170', 'RD16(monkey + AE_MK_JUMP_LAND_FRAMES) >> 1);', 'RD16(monkey + AE_MK_JUMP_LAND_FRAMES) >> 2);'),
    ('Arc fin', 'if (ae_MonkeyAnimFinished(r) != 0) {\n                uint32_t m = cur_monkey(r);', 'if (ae_MonkeyAnimFinished(r) == 0) {\n                uint32_t m = cur_monkey(r);'),
    ('Arc clamp2', 'new_frame = RD16((uint32_t)anim_index * 8u + anim_table + 2);\n    }\n    WR16(actor', 'new_frame = RD16((uint32_t)anim_index * 8u + anim_table);\n    }\n    WR16(actor'),
    ('Move x', 'sra32(RDS32(monkey + AE_MK_JUMP_VEL), 12)', 'sra32(RDS32(monkey + AE_MK_JUMP_VEL), 11)'),
    ('Move z', 'sra32(RDS32(monkey + AE_MK_JUMP_VEL + 8), 12)', 'sra32(RDS32(monkey + AE_MK_JUMP_VEL + 4), 12)'),
    ('Move x0', 'uint32_t start_x = RD32(monkey + AE_MK_JUMP_START);', 'uint32_t start_x = RD32(monkey + AE_MK_JUMP_START + 4);'),
    ('Move wp', 'int32_t min_y = ae_MonkeyGetWaypointPos(r, h, 1);', 'int32_t min_y = ae_MonkeyGetWaypointPos(r, h, 0);'),
    ('Move vy', 'int32_t vel_y = RDS32(actor + AE_MK_JUMP_VEL + 4);', 'int32_t vel_y = RDS32(actor + AE_MK_JUMP_VEL);'),
    ('Move 4095', 'int32_t rise = mul_lo(vel_y, t) / 4096;', 'int32_t rise = sra32(mul_lo(vel_y, t), 12);'),
    ('Move 300', 'int32_t gravity = mul_lo(t, 300 * t) / 100;', 'int32_t gravity = mul_lo(t, 150 * t) / 100;'),
    ('Move cmp', 'if (min_y < (int32_t)y) {', 'if (min_y <= (int32_t)y) {'),
    ('Move 130', 'uint32_t y = RD32(actor + AE_MK_JUMP_START + 4);', 'uint32_t y = RD32(actor + AE_MK_JUMP_START + 8);'),
    ("End sel", "if (ae_MonkeySelectNextNode(r, h, 1, 0) != 0) {", "if (ae_MonkeySelectNextNode(r, h, 0, 0) != 0) {"),
    ("End 0C", "if (ae_MonkeyIsState0C_or_24(r) != 0) return;\n    ae_MonkeySetState(r, 0);", "if (ae_MonkeyIsState0C_or_24(r) == 0) return;\n    ae_MonkeySetState(r, 0);"),
    ('End boss', 'if (ae_AreaIsBossArena(r) != 0) return;\n    uint32_t state =', 'if (ae_AreaIsBossArena(r) == 0) return;\n    uint32_t state ='),
    ('JA 29', 'if (RD8(monkey + AE_MK_AI_STATE) != 29) {\n            arc_anim(r, monkey, t, duration);\n            arc_move(', 'if (RD8(monkey + AE_MK_AI_STATE) != 28) {\n            arc_anim(r, monkey, t, duration);\n            arc_move('),
    ('JA s16', 'arc_move(r, h, s16(counter_raw));', 'arc_move(r, h, s16(counter_raw) + 1);'),
    ('JA inc', 'WR16(monkey + AE_MK_COUNTER96, RD16(monkey + AE_MK_COUNTER96) + 1u);\n        return;\n    }\n    /* the flight is over: land on the node (state', 'WR16(monkey + AE_MK_COUNTER96, RD16(monkey + AE_MK_COUNTER96) + 2u);\n        return;\n    }\n    /* the flight is over: land on the node (state'),
    ('JA land', 'if (RD8(monkey + AE_MK_AI_STATE) == 29) {\n        WR32(monkey + AE_MK_POS, RD32(sp + 16));', 'if (RD8(monkey + AE_MK_AI_STATE) == 28) {\n        WR32(monkey + AE_MK_POS, RD32(sp + 16));'),
    ('PH 45', '    default:                                    /* 0x2D: the flight */\n        break;', '    case 0x2D:\n        return;\n    default:                                    /* 0x2D: the flight */\n        break;'),
    ('PH 44', '    case 0x2C:', '    case 0x2B:'),
    ('PH 46', '    case 0x2E:', '    case 0x2F:'),
    ('PH set45', 'WR8(monkey + AE_MK_AI_STATE, 0x2D);', 'WR8(monkey + AE_MK_AI_STATE, 0x2C);'),
    ('PH set46', 'WR8(monkey + AE_MK_AI_STATE, 0x2E);', 'WR8(monkey + AE_MK_AI_STATE, 0x2F);'),
    ('PH adv', 'if (frame < last_frame) ae_MonkeyAdvanceAnimFrame(r);', 'if (frame <= last_frame) ae_MonkeyAdvanceAnimFrame(r);'),
    ('PH lt', 'if (!(t < (int32_t)duration)) {\n        /* the flight is over', 'if (!(t <= (int32_t)duration)) {\n        /* the flight is over'),
    ('JB 400', 'mul_lo(c, 400 * c) / 100', 'mul_lo(c, 200 * c) / 100'),
    ('JB sub', 'new_y = sub32(new_y, rise);', 'new_y = add32(new_y, rise);'),
    ('JB cmp', 'if (ground < new_y) {', 'if (ground <= new_y) {'),
    ('JB 130', '(int32_t)RD32(m + AE_MK_JUMP_START + 4);', '(int32_t)RD32(m + AE_MK_JUMP_START + 8);'),
    ('SA 4', 'for (slot = 0; slot < 4; slot++) {\n        uint32_t active', 'for (slot = 0; slot < 3; slot++) {\n        uint32_t active'),
    ('SA act', 'if (RD8(monkey + AE_MK_ACTIVITY) - 2u >= 2u) {          /* activity is not 2 / 3 */\n            WR32(AE_G_CUR_MONKEY, monkey);\n            int32_t dist', 'if (RD8(monkey + AE_MK_ACTIVITY) - 2u > 2u) {          /* activity is not 2 / 3 */\n            WR32(AE_G_CUR_MONKEY, monkey);\n            int32_t dist'),
    ('SA 36', 'monkey + AE_MK_POS, 0x800EC204u)', 'monkey + AE_MK_POS - 4, 0x800EC204u)'),
    ('SA activity4', 'if (activity == 4) {', 'if (activity == 3) {'),
    ('SA farthest', 'if (farthest_dist < occupant_dist) {', 'if (farthest_dist <= occupant_dist) {'),
    ('SA cmp', 'AE_MK_DIST_TO_PLAYER) < farthest_dist) {', 'AE_MK_DIST_TO_PLAYER) <= farthest_dist) {'),
    ('SA demote', 'if (RD8(replaced + AE_MK_ACTIVITY) != 0)', 'if (RD8(replaced + AE_MK_ACTIVITY) == 0)'),
    ('SA init', 'ae_MonkeyInit(r, h, id, 4, (uint32_t)slot);', 'ae_MonkeyInit(r, h, id, 3, (uint32_t)slot);'),
    ('SA pick', 'if (old_activity == 0) ae_MonkeyPickGoalNode(r, h);', 'if (old_activity != 0) ae_MonkeyPickGoalNode(r, h);'),
    ('SA id', 'RDS16(saved_ids + (uint32_t)slot * 2u) != (int32_t)RD8(', 'RDS16(saved_ids + (uint32_t)slot * 2u) == (int32_t)RD8('),
    ('SA m1', 'if (goal_node != -1) {', 'if (goal_node == -1) {'),
    ('SA 14', 'AE_NAV_NODE_VISIT_MARKS, 1);', 'AE_NAV_NODE_VISIT_MARKS, 2);'),
    ('SA ee', 'AE_MK_NAV_MARK_COUNT, 1);', 'AE_MK_NAV_MARK_COUNT, 0);'),
    ("Pat bone", "    if (next_flags & AE_NAV_FLAG_BONE) {\n        /* ---- the next node is a bone node */", "    if (!(next_flags & AE_NAV_FLAG_BONE)) {\n        /* ---- the next node is a bone node */"),
    ("Pat alerted", "if (RD8(monkey + AE_MK_ALERTED) == 0) {\n        if (ae_MonkeyUpdateSight(r, h, 1) == 0) return;", "if (RD8(monkey + AE_MK_ALERTED) != 0) {\n        if (ae_MonkeyUpdateSight(r, h, 1) == 0) return;"),
    ("Pat sight1", "if (ae_MonkeyUpdateSight(r, h, 1) == 0) return;\n        int32_t voice = ae_MonkeyGetCallVoice(r);", "if (ae_MonkeyUpdateSight(r, h, 0) == 0) return;\n        int32_t voice = ae_MonkeyGetCallVoice(r);"),
    ("Pat set1", "WR8(cur_monkey(r) + AE_MK_ALERTED, 1);", "WR8(cur_monkey(r) + AE_MK_ALERTED, 2);"),
    ("Pat nop", "if ((int32_t)lost_count < limit) {", "if ((int32_t)lost_count <= limit) {"),
    ("Pat 300", "if (frame % 300u != 0) return;", "if (frame % 600u != 0) return;"),
    ("Pat 249", "RD8(monkey + AE_MK_LOST_SIGHT_COUNT) + 1u);", "RD8(monkey + AE_MK_LOST_SIGHT_COUNT) + 2u);"),
    ("Pat bit3", "if ((STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) & STATE_FLAG_ATTACK) && ae_MonkeyAnimFinished(r) == 0) {", "if ((STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) & (STATE_FLAG_ATTACK << 1)) && ae_MonkeyAnimFinished(r) == 0) {"),
    ("Pat 29", "else ae_MonkeySetState(r, PATROL_STATE_BONE_FOLLOW);", "else ae_MonkeySetState(r, PATROL_STATE_BONE_FOLLOW - 1);"),
    ("Pat div100", "int32_t object = (int16_t)(bone_ref / 100);", "int32_t object = (int16_t)(bone_ref / 101);"),
    ("Pat swap", "WR16(monkey + AE_MK_NAV_NODE0, next_node);\n        WR16(monkey + AE_MK_NAV_NODE1, this_node);", "WR16(monkey + AE_MK_NAV_NODE0, this_node);\n        WR16(monkey + AE_MK_NAV_NODE1, next_node);"),
    ("Pat vec", "WR16(monkey + AE_MK_MOVE_DIR, RD16(sp + 16) - RD16(sp + 32));", "WR16(monkey + AE_MK_MOVE_DIR, RD16(sp + 16) + RD16(sp + 32));"),
    ("Pat 50", "WR32(monkey + AE_MK_POS + 4, RD32(sp + 36) + 50u);", "WR32(monkey + AE_MK_POS + 4, RD32(sp + 36) + 49u);"),
    ("Pat gh50", "GetGroundHeight, y, y - 50u, node_z, 0);", "GetGroundHeight, y, y - 49u, node_z, 0);"),
    ("Pat attackA", "ae_MonkeyPickAttackA(r, h) == 100) {", "ae_MonkeyPickAttackA(r, h) != 100) {"),
    ("Pat 2A", "} else if (state == PATROL_STATE_BONE_HOP) {", "} else if (state == PATROL_STATE_BONE_HOP + 1) {"),
    ("Pat 100y", "start_y - n * 100u + (uint32_t)gravity", "start_y - n * 200u + (uint32_t)gravity"),
    ("Pat set2B", "ae_MonkeySetState(r, PATROL_STATE_BONE_LAND);", "ae_MonkeySetState(r, PATROL_STATE_BONE_LAND + 1);"),
    ("Pat 1000", "WR16(monkey + AE_MK_MOVE_DIR, 0x1000);", "WR16(monkey + AE_MK_MOVE_DIR, 0x1001);"),
    ("Pat 64", "GetGroundHeight, x, y - 64u, z, 0);", "GetGroundHeight, x, y - 63u, z, 0);"),
    ("Pat 9a", "WR16(monkey + AE_MK_IDLE_TIMER, timer - 1);", "WR16(monkey + AE_MK_IDLE_TIMER, timer - 2);"),
    ("Pat 2000", "        if (dist < 2000) {\n            int32_t chance", "        if (dist < 1999) {\n            int32_t chance"),
    ("Pat rand3", "if (roll2 % 10u < 3u) pick_by_angle = 1;", "if (roll2 % 10u < 4u) pick_by_angle = 1;"),
    ("Pat 342", "if (angle - 342u < 1365u) new_state = 0x0D;", "if (angle - 343u < 1365u) new_state = 0x0D;"),
    ("Pat 1366", "else if (angle - 1366u < 1365u) new_state = 0x13;", "else if (angle - 1366u < 1366u) new_state = 0x13;"),
    ("Pat 2390", "else if (angle - 2390u < 1365u) new_state = 0x10;", "else if (angle - 2391u < 1365u) new_state = 0x10;"),
    ("Pat 3357", "angle - 3357u < 740u) new_state", "angle - 3356u < 740u) new_state"),
    ("Pat idle0D", "new_state = 0x0D;\n        else if (angle - 1366u", "new_state = 0x0E;\n        else if (angle - 1366u"),
    ("Pat rand3b", "switch (ae_MonkeyRand(r) & 3u) {", "switch (ae_MonkeyRand(r) & 1u) {"),
    ("Pat boss11", "RD8(cur_monkey(r) + AE_MK_AI_STATE) == PATROL_STATE_WALK_BOSS_TURN && RD8(AE_G_EXIT_CODE_447C) == 3) {", "RD8(cur_monkey(r) + AE_MK_AI_STATE) == PATROL_STATE_WALK_BOSS_TURN - 1 && RD8(AE_G_EXIT_CODE_447C) == 3) {"),
    ("Pat exit3", "RD8(AE_G_EXIT_CODE_447C) == 3) {\n                ae_MonkeyAlertedDecide", "RD8(AE_G_EXIT_CODE_447C) == 2) {\n                ae_MonkeyAlertedDecide"),
    ("Pat eb5c", "WR32(AE_G_FLAG_EB5C, 0);", "WR32(AE_G_FLAG_EB5C, 1);"),
    ("Pat mod3", "if (tick % 3u == 0 && ae_MonkeyUpdateSight(r, h, 1) != 0) {", "if (tick % 6u == 0 && ae_MonkeyUpdateSight(r, h, 1) != 0) {"),
    ("Pat bit6", "if (STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) & STATE_FLAG_CAN_NOTICE) {", "if (STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) & (STATE_FLAG_CAN_NOTICE << 1)) {"),
    ("Pat 10000", "        if (next_flags & AE_NAV_FLAG_OBJECT) {\n            /* an object node", "        if (next_flags & (AE_NAV_FLAG_OBJECT << 1)) {\n            /* an object node"),
    ("Pat anim2", "(uint32_t)RDS16(node + 0x10), 0, 0, 0) == 2) {\n                ae_MonkeyAlertedDecide(r, h);\n            } else {", "(uint32_t)RDS16(node + 0x10), 0, 0, 0) == 3) {\n                ae_MonkeyAlertedDecide(r, h);\n            } else {"),
    ("Pat anim1", "0, 0, 0) == 1) {\n                    ae_MonkeyAlertedDecide(r, h);", "0, 0, 0) == 2) {\n                    ae_MonkeyAlertedDecide(r, h);"),
    ("Pat mod20", "if ((ae_MonkeyRand(r) & 0xFFFFu) % 20u == 0) {", "if ((ae_MonkeyRand(r) & 0xFFFFu) % 10u == 0) {"),
    ("Pat cf", "ObjSlotSetField_CF, (uint32_t)RDS16(node_of(r, index) + 0x10), 3, 0, 0);", "ObjSlotSetField_CF, (uint32_t)RDS16(node_of(r, index) + 0x10), 4, 0, 0);"),
    ("Pat 34", "if (state == PATROL_STATE_ATTACK_PROJ6 && patrol_attack_in_range", "if (state == PATROL_STATE_ATTACK_PROJ6 + 1 && patrol_attack_in_range"),
    ("Pat i26", "#define INFO_ATTACK_PROJ6 0x26u", "#define INFO_ATTACK_PROJ6 0x28u"),
    ("Pat i2A", "if (dist < RDS16(info + record + 4)) return 0;", "if (dist <= RDS16(info + record + 4)) return 0;"),
    ("Pat i2C", "if (RDS16(info + record + 6) < dist) return 0;", "if (RDS16(info + record + 6) <= dist) return 0;"),
    ("Pat mod5", "if (RD32(AE_G_FRAME_COUNTER) % 5u == 0) ae_MonkeyFireProjectile6", "if (RD32(AE_G_FRAME_COUNTER) % 10u == 0) ae_MonkeyFireProjectile6"),
    ("Pat aim0", "ae_MonkeyAimAtTarget(r, h, 0);", "ae_MonkeyAimAtTarget(r, h, 1);"),
    ("Pat aim5", "ae_MonkeyAimAtTarget(r, h, 5);", "ae_MonkeyAimAtTarget(r, h, 4);"),
    ("Pat 10f", "if (patrol_anim_offset(r) == 10) ae_MonkeyFireProjectile7", "if (patrol_anim_offset(r) == 11) ae_MonkeyFireProjectile7"),
    ("Pat 25", "if (patrol_anim_offset(r) == 25) ae_MonkeyFireProjectile8", "if (patrol_anim_offset(r) == 24) ae_MonkeyFireProjectile8"),
    ("Pat m3", "patrol_anim_offset(r) % 10 == 3 ||", "patrol_anim_offset(r) % 10 == 4 ||"),
    ("Pat m7", "patrol_anim_offset(r) % 10 == 7)", "patrol_anim_offset(r) % 10 == 6)"),
    ("Pat proj3", "ae_MonkeySpawnProjectile(r, h, 3);\n            ae_MonkeyQueueVoice(r, h, 0x48, 0);", "ae_MonkeySpawnProjectile(r, h, 2);\n            ae_MonkeyQueueVoice(r, h, 0x48, 0);"),
    ("Pat v48", "ae_MonkeyQueueVoice(r, h, 0x48, 0);", "ae_MonkeyQueueVoice(r, h, 0x49, 0);"),
    ("Pat 222", "        monkey = cur_monkey(r);\n        WR8(monkey + AE_MK_PATROL_COUNTER, RD8(monkey + AE_MK_PATROL_COUNTER) - 1u);", "        monkey = cur_monkey(r);\n        WR8(monkey + AE_MK_PATROL_COUNTER, RD8(monkey + AE_MK_PATROL_COUNTER) - 2u);"),
    ("Pat proj4", "ae_MonkeySpawnProjectile(r, h, 4);", "ae_MonkeySpawnProjectile(r, h, 3);"),
    ("Pat 55inc", "WR16(monkey + AE_MK_COUNTER96, RD16(monkey + AE_MK_COUNTER96) + 1u);\n            if (RDS8(", "WR16(monkey + AE_MK_COUNTER96, RD16(monkey + AE_MK_COUNTER96) + 2u);\n            if (RDS8("),
    ("Pat 39", "if (state - 39u < 2u ||", "if (state - 38u < 2u ||"),
    ("Pat 41", "state == 24 || state == 41) {", "state == 24 || state == 40) {"),
    ("Pat 1501", "RD32(monkey + AE_MK_DIST_TO_TARGET) < 1501)) return 1;", "RD32(monkey + AE_MK_DIST_TO_TARGET) < 1500)) return 1;"),
    ("Pat bit4", "if (node_flags & AE_NAV_FLAG_WAIT) {", "if (node_flags & AE_NAV_FLAG_NEAR_TARGET) {"),
    ("Pat 150", "if (tick % 150u == 0) {", "if (tick % 300u == 0) {"),
    ("Pat 200", "RD32(cur_monkey(r) + AE_MK_DIST_TO_TARGET) < 200)) return 0;", "RD32(cur_monkey(r) + AE_MK_DIST_TO_TARGET) < 199)) return 0;"),
    ("Pat 1201", "!(RD32(monkey + AE_MK_DIST_TO_TARGET) - 300u < 1201u)", "!(RD32(monkey + AE_MK_DIST_TO_TARGET) - 300u < 1200u)"),
    ("Pat 170", "ae_MonkeyTurnTowardTarget(r, 170);", "ae_MonkeyTurnTowardTarget(r, 171);"),
    ("Pat 284", "ae_MonkeyTurnTowardTarget(r, 284);", "ae_MonkeyTurnTowardTarget(r, 285);"),
    ("Pat 510", "ae_MonkeyTurnTowardTarget(r, 510);", "ae_MonkeyTurnTowardTarget(r, 511);"),
    ("Pat 55end", "if (state != PATROL_STATE_ATTACK_PROJ3) return;\n    ae_MonkeyTurnTowardTarget", "if (state != PATROL_STATE_ATTACK_PROJ3 - 1) return;\n    ae_MonkeyTurnTowardTarget"),
    ("Pat 28end", "if (state == PATROL_STATE_ATTACK_PROJ7 || state == PATROL_STATE_ATTACK_AIM0 || state == PATROL_STATE_ATTACK_AIM5) {\n        ae_MonkeyTurnTowardTarget", "if (state == PATROL_STATE_ATTACK_PROJ7 + 1 || state == PATROL_STATE_ATTACK_AIM0 || state == PATROL_STATE_ATTACK_AIM5) {\n        ae_MonkeyTurnTowardTarget"),
    ("Pat e140", "if (RD32(node) & AE_NAV_FLAG_REF_POS) patrol_place_at_reference", "if (RD32(node) & (AE_NAV_FLAG_REF_POS << 1)) patrol_place_at_reference"),
    ("Pat 56", "uint32_t offset = (uint32_t)record * 56u;", "uint32_t offset = (uint32_t)record * 28u;"),
]

MUT_FUNCS = {
    "State3D": ["MonkeyAi_State3D"], "Arc": ["MonkeyAi_JumpArc", "MonkeyAi_JumpArcB"], "Move": ["MonkeyAi_JumpArc", "MonkeyAi_JumpArcPhased"],
    "End": ["MonkeyAi_JumpArc", "MonkeyAi_JumpArcB", "MonkeyAi_JumpArcPhased"], "JA": ["MonkeyAi_JumpArc"], "PH": ["MonkeyAi_JumpArcPhased"],
    "JB": ["MonkeyAi_JumpArcB"], "SA": ["MonkeySelectActive"], "Pat": ["MonkeyAi_Patrol"],
}


def run_mutations(scale, seed, only_labels=(), shard=None):
    rels = ["src/monkeys/ae_monkey_level9.c", "src/monkeys/ae_monkey_level9_patrol.c"]
    texts = {rel: (ROOT / rel).read_text(encoding="utf-8") for rel in rels}
    survivors = []
    tmp = Path(tempfile.mkdtemp(prefix="mk9_mut_"))
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
            h.reload_c({rel: mp}, tag=f"mk9mut{k}")
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
    ap.add_argument("--seed", type=int, default=0x944291)
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
