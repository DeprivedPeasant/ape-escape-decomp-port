"""Differential check of monkey port batch 4b (src/monkeys/ae_monkey_level5.c, call-graph level 5) against the original MIPS
code: for every ported function many scenarios are built in a byte-exact RAM image (SCUS_944.23 at its real addresses; monkeys /
nav nodes / MonkeyInfo / animation tables / globals laid out as MonkeySpawnAll and the level loader leave them, plus randomised and
edge-case states); the original instructions (GTE through the PCSX-ReARMed oracle) and the C port run on identical images with
identical hook behaviour; return value, RAM outside the stack window, scratchpad and the sequence of hook calls must be identical.
The batch 1..4a functions the code calls run as the C ports on the C side and as the original instructions on the MIPS side.
Needs extract/disc/SCUS_944.23 (skips if missing).  See check_monkey_level4.py and monkey_scen4.py.

Usage: uv run python tools/mips/check_monkey_level5.py [--cases PERCENT] [--seed S] [--coverage] [--trapv] [--mutate]
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
from monkey_harness import W, MONKEYS, MONKEY_SIZE, G_CUR, NAV_BASE  # noqa: E402
from monkey_scen import combat, navpos_world, anim_globals, M32, s32, s16, u16, FRAME, G_TARGET  # noqa: E402

# name -> (original address, size, return kind: s = int32 / u = uint32 / v = void, takes hooks, argument kinds[, C name])
FUNCS = {
    "MonkeyNodeActionDispatch": (0x80036CD0, 920, "s", True, ""),
    "MonkeyConsiderTurnBack": (0x800386C8, 588, "s", True, ""),
    "MonkeyConsiderTurnBackRandom": (0x80038914, 264, "v", True, ""),
}
EXTRA_COVERAGE = {
    "MonkeySetState (batch 3a)": (0x80033220, 216), "MonkeyUpdateLinkSlope (batch 3a)": (0x80035370, 260),
    "MonkeyUpdateGoalPos (batch 3a)": (0x80035474, 360), "MonkeyUpdateGoal (batch 3b)": (0x800355DC, 40),
    "MonkeySwapNavNodes (batch 4a)": (0x80038694, 52), "MonkeyNodeTargetCosines (batch 3a)": (0x8003B534, 0x138),
    "MonkeyEnterState_1D (batch 4a)": (0x800359F0, 140), "MonkeyEnterState_04 (batch 4a)": (0x80035A7C, 340),
    "MonkeyEnterState_2C (batch 4a)": (0x80036964, 208), "MonkeyEnterState_1E (batch 3b)": (0x8003581C, 252),
    "MonkeyEnterState_1A (batch 3b)": (0x80035FBC, 96), "MonkeyEnterState_35 (batch 3b)": (0x80035F8C, 48),
    "MonkeyEnterState_09 (batch 3b)": (0x8003601C, 188), "MonkeyEnterState_24 (batch 3b)": (0x80035F1C, 56),
    "MonkeyEnterState_0C (batch 3b)": (0x80035F54, 56), "MonkeyEnterState_3D (batch 3a)": (0x800360D8, 0x54),
    "MonkeyEnterState_3E (batch 3a)": (0x8003612C, 0x48), "MonkeyPrepareJumpArc (batch 3a)": (0x800364BC, 0x270),
    "NavNodeGetPos (batch 2)": (0x80034EF8, 392), "MonkeyRandomHesitate (batch 2)": (0x80032F24, 0x70),
    "NavLinkLookupExtra (batch 2)": (0x8003B880, 320), "NavNodeLinkFlag (batch 1)": (0x80038608, 140),
    "DotXZ_SV (batch 1)": (0x800348CC, 0x74), "rcos (SDK)": (0x8002FC20, 160),
    "AreaIsBossArena (batch 1)": (0x800339EC, 80), "AreaIsSpecialMonkeyArea (batch 1)": (0x8003398C, 0x40),
    "MonkeyApproachAngleNear (batch 1)": (0x80037D6C, 0x80), "MonkeyApproachAngleFar (batch 1)": (0x80037DEC, 0xA0),
    "MonkeyIsJumpArcState (batch 1)": (0x80033C34, 76), "MonkeyRand (batch 1)": (0x80032E30, 40),
}
DISPATCH_FRAME = 32
DISPATCH_BITS = [0x100, 0x800, 0x20, 0x20000, 8, 4, 0x10, 1, 0x10000, 0x800000, 0x1000000]


def check_all(h, scale, seed):
    t0 = time.time()
    h.mask = [(MONKEYS + MONKEY_SIZE * k + 0xC8 - 0x80000000, 4) for k in range(8)]   # see check_monkey_level4.py
    mh.reg.declare("shared_random", "MonkeyConsiderTurnBack $v0 (return value on one path)")      # see check_monkey_level4.py
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

    # ------------------------------------------------------------------ NodeActionDispatch
    def dispatch_world(m, rng, i, bits=None, state=None):
        c = s4.nav_entry_world(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        if bits is None:
            bits = sum(b for b in DISPATCH_BITS if rng.random() < rng.choice([0.1, 0.25, 0.5]))
        w.w32(c + 0x228, (bits | (rng.getrandbits(32) & 0x200 if rng.random() < 0.3 else 0)) & M32)
        w.w32(c + 0x248, rng.choice([0, 0x400000, rng.getrandbits(32), 0xFFBFFFFF]) & M32)
        w.w8(c + 0x78, state if state is not None else rng.choice([3, 3, 3, 61, 9, 0x1A, 0x35, 4, 0x2C, rng.randint(0, 0x42)]))
        w.w16(c + 0x94, rng.choice([0, 1, -1, 5, -5, 0x7FFF, 0x8000, rng.getrandbits(16)]) & 0xFFFF)
        w.w32(c + 0x218, rng.choice([299, 300, 301, 500, 799, 800, 801, 0, -1, rng.randint(0, 2000), rng.getrandbits(32)]) & M32)
        n1 = rng.randint(0, nn - 1)
        w.w16(c + 0xF4, n1)
        w.w16(c + 0xF2, rng.randint(0, nn - 1))
        for node in (m.read16(c + 0xF2), n1):
            a = NAV_BASE + 0x18 * node
            if rng.random() < 0.5:
                w.w32(a, m.read32(a) | rng.choice([0x200, 0x1, 0x201, 0x0]))
        w.w16(0x800F4472, rng.choice([0, 1, 3, 6, 8, 2, 5]))
        # never-written stack slots of the entered functions (leftover stack in the real game): the same value on both sides
        # (EnterState_1D's sp + 16 and EnterState_1E's sp + 32 are the same stack word in the original: one value for both)
        v_1d = rng.choice([rng.getrandbits(32), 0, 0xFFFF, 0x7FFF8000])
        for name, addr, off, val in (("ENTER_1D", 0x800359F0, 16, v_1d), ("ENTER_1E", 0x8003581C, 32, v_1d),
                                     ("ENTER_09", 0x8003601C, 16, rng.choice([rng.getrandbits(32), 0, 0xFFFF, 0x7FFF8000]))):
            s4.set_stale_chain(m, name, addr, DISPATCH_FRAME, off, val)
        return c
    run("MonkeyNodeActionDispatch", 1800, dispatch_world, nh=True, allow_trap=True)
    # the ground hop (no flag bit set, state 3, 10 % chance, distance window, node flags): the random number is aimed
    def hop_world(m, rng, i):
        c = dispatch_world(m, rng, i, bits=0, state=3)
        w = W(m)
        w.w32(c + 0x248, rng.choice([0, 1 << 20]))
        w.w16(0x800F4472, rng.choice([0, 1, 2]))
        # MonkeyRand: seed' = (seed * 5 + 1) & 0xFFFF, value = seed' >> 8; the test is value % 100 < 10 (hit most of the time)
        kind = rng.choice(["hit", "hit", "hit", "edge", "miss"])
        if kind == "hit":
            seed = s4.seed_for(rng, lambda v: v % 100 < 10)
        elif kind == "edge":                   # remainders 9 / 10 (the limit of the test)
            seed = s4.seed_for(rng, lambda v: v % 100 in (9, 10))
        else:
            seed = s4.seed_for(rng, lambda v: v % 100 >= 10)
        w.w32(0x800B4D10, seed)
        w.w32(c + 0x218, rng.choice([300, 301, 799, 800, 600, 299, 801, rng.randint(250, 850)]))
        nn = m.read16(0x800B4D18)
        for node in (m.read16(c + 0xF2), m.read16(c + 0xF4)):
            a = NAV_BASE + 0x18 * node
            w.w32(a, (m.read32(a) & ~1) | rng.choice([0, 1, 1]))
        return c
    run("MonkeyNodeActionDispatch", 500, hop_world, nh=True, allow_trap=True, i0=40000)

    def cool_world(m, rng, i):
        # flags2 bit 4 with +0x94 <= 0: the (always true) "% 100 < 100" test; remainders 98 / 99 are the edge
        c = dispatch_world(m, rng, i, bits=0x10, state=rng.choice([3, 9, 0x0C, 0x24, 1]))
        w = W(m)
        w.w16(c + 0x94, rng.choice([0, 0, -1, -5, 0x8000]) & 0xFFFF)
        w.w32(0x800B4D10, s4.seed_for(rng, lambda v: v % 100 in (97, 98, 99) or rng.random() < 0.2))
        return c
    run("MonkeyNodeActionDispatch", 250, cool_world, nh=True, allow_trap=True, i0=45000)
    # every flag bit alone and in pairs, each x several states
    ci = 300000
    for bits in DISPATCH_BITS + [a | b for a in DISPATCH_BITS for b in DISPATCH_BITS if a < b] + [0]:
        for st in (3, 61, 9):
            rng = rng_of("MonkeyNodeActionDispatch-bits", ci)
            h.run("MonkeyNodeActionDispatch", f"bits {bits:X} state {st}", ci,
                  lambda m, rng=rng, bits=bits, st=st, ci=ci: dispatch_world(m, rng, ci, bits=bits, state=st), lambda c: (), allow_trap=True)
            ci += 1

    # ------------------------------------------------------------------ turning back
    def turn_world(m, rng, i, rnd=False):
        c = s4.nav_entry_world(m, rng, plain=True)
        w = W(m)
        nn = m.read16(0x800B4D18)
        a, b = rng.randint(0, nn - 1), rng.randint(0, nn - 1)
        if nn > 1:
            while b == a:
                b = rng.randint(0, nn - 1)
        s4.relink(m, rng, nn, a, b, rng.random() < 0.8, rng.random() < 0.75)
        w.w16(c + 0xF2, a)
        w.w16(c + 0xF4, b)
        w.w16(c + 0xEC, rng.choice([a, b, rng.randint(0, 40), 0xFFFF, a, b, 0x8000 | a]) & 0xFFFF)
        for node in (a, b):
            for off in (0xA, 0xC):
                pass
        slot = m.read8(c + 0x7B)
        ca = rng.choice([0, 1, 2, 3, 255, rng.getrandbits(8)])
        cb = rng.choice([ca, ca + 1, ca - 1, 0, 1, 254, rng.getrandbits(8)]) & 0xFF
        w.w8(NAV_BASE + 0x18 * a + slot + 0x14, ca)
        w.w8(NAV_BASE + 0x18 * b + slot + 0x14, cb)
        w.w16(0x800F4472, rng.choice([0, 1, 2, 3, 6, 8]) if rng.random() < 0.15 else 0)
        w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x43, 0x35, 0x46]))
        w.w8(c + 0xD4, rng.choice([0, 0, 0, 1, 2, 0x80, rng.getrandbits(8)]))
        w.w32(c + 0x248, rng.choice([0, rng.getrandbits(32), 1 << 26]) & M32)
        w.w32(c + 0x24, 0)
        w.w32(c + 0x2C, 0)
        tgt = m.read32(G_TARGET)
        # nodes a / b plain: position (x, y, z) s16; the monkey is at x = z = 0 so the direction is the node position
        for node in (a, b):
            nb = NAV_BASE + 0x18 * node
            w.w32(nb, m.read32(nb) & ~0x401 & M32)
        if rng.random() < 0.25:                        # short node vectors: a 1 unit change of the y component matters
            for q in (a, b):
                w.w16(NAV_BASE + 0x18 * q + 4, rng.randint(-12, 12) & 0xFFFF)
                w.w16(NAV_BASE + 0x18 * q + 8, rng.randint(-12, 12) & 0xFFFF)
        na, nb_ = [(m.reads16(NAV_BASE + 0x18 * q + 4), m.reads16(NAV_BASE + 0x18 * q + 8)) for q in (a, b)]
        # the threshold: rcos of the approach angle (near for special areas, far otherwise)
        thr = None
        if rng.random() < 0.8:
            special = m.read16(0x800F4476) in (0x29, 0x43, 0x35)
            ang = s32(s4.mips_call(m, 0x80037D6C if special else 0x80037DEC))
            thr = s32(s4.mips_call(m, 0x8002FC20, (ang & M32,)))
        tx = tz = None
        if thr is not None:
            node = na if rng.random() < 0.5 else nb_
            res = aim.vector_for(rng, node, thr, spread=rng.choice([(0,), (0, 1, -1), (0,)]))
            if res:
                tx, tz = res[0]
        if tx is None:
            tx, tz = rng.randint(-8000, 8000), rng.randint(-8000, 8000)
        w.w32(tgt + 0x24, tx & M32)
        w.w32(tgt + 0x2C, tz & M32)
        w.w32(tgt + 0x28, rng.getrandbits(32))
        return c
    run("MonkeyConsiderTurnBack", 2300, turn_world, nh=True, allow_trap=True)

    def turn_rand_world(m, rng, i):
        c = turn_world(m, rng, i, rnd=True)
        w = W(m)
        # out0 / out1 thresholds: rcos(682), out1 < out0, out1 == 0
        a, b = m.read16(c + 0xF2), m.read16(c + 0xF4)
        na, nb_ = [(m.reads16(NAV_BASE + 0x18 * q + 4), m.reads16(NAV_BASE + 0x18 * q + 8)) for q in (a, b)]
        thr = s32(s4.mips_call(m, 0x8002FC20, (682,)))
        flat = rng.random() < 0.9               # NodeTargetCosines normalises the full vectors: aim in the plane (all y differences 0)
        if flat:
            w.w32(c + 0x28, 0)
            w.w32(m.read32(G_TARGET) + 0x28, 0)
            for q in (a, b):
                w.w16(NAV_BASE + 0x18 * q + 6, 0)
        res = aim.vector_for(rng, na, thr + rng.choice([0, 0, 1, -1, 5, -5, 300]))
        if res:
            tx, tz = res[0]
            d0 = res[1]
            w.w32(m.read32(G_TARGET) + 0x24, tx & M32)
            w.w32(m.read32(G_TARGET) + 0x2C, tz & M32)
            # next node: aim at out0 - 1 / out0 / out0 + 1 / 0 by moving the node b
            want = rng.choice([d0 - 1, d0, d0 + 1, 0, 0, d0 - 300, 5, d0 + 1, d0 + 50, d0 + 300])
            want = max(-4096, min(4096, want))
            nbp = aim.vector_for(rng, (tx, tz), want)
            if nbp:
                bx, bz = nbp[0]
                bb = NAV_BASE + 0x18 * b
                w.w16(bb + 4, bx & 0xFFFF)
                w.w16(bb + 8, bz & 0xFFFF)
        w.w32(0x800B4D10, s4.seed_for(rng, lambda v: rng.random() < 0.15 or v % 10 in rng.choice([(5, 6, 7, 8), (6, 7), (7, 8, 9)])))
        return c
    run("MonkeyConsiderTurnBackRandom", 2300, turn_rand_world, nh=True, allow_trap=True)

    print(f"scenario families done in {time.time() - t0:.1f}s")


MUTATIONS = [
    ("Disp flag22", 'if ((RD32(monkey + AE_MK_FLAGS) >> 22) & 1u) return 0;', 'if ((RD32(monkey + AE_MK_FLAGS) >> 21) & 1u) return 0;'),
    ("Disp 100", 'if (link_flags & 0x100u) {\n        ae_MonkeyEnterState_1D', 'if (link_flags & 0x80u) {\n        ae_MonkeyEnterState_1D'),
    ("Disp 800", 'if (link_flags & 0x800u) {\n            uint32_t n1', 'if (link_flags & 0x400u) {\n            uint32_t n1'),
    ("Disp node1", 'uint32_t n1 = RD16(monkey + AE_MK_NAV_NODE1);\n            if (RD32(node_of', 'uint32_t n1 = RD16(monkey + AE_MK_NAV_NODE0);\n            if (RD32(node_of'),
    ("Disp 200", "RD32(node_of(r, n1)) & 0x200u", "RD32(node_of(r, n1)) & 0x400u"),
    ("Disp 20", 'if (link_flags & 0x20u) {', 'if (link_flags & 0x40u) {'),
    ("Disp 20000", '} else if (link_flags & 0x20000u) {', '} else if (link_flags & 0x40000u) {'),
    ("Disp 8", '} else if (link_flags & 0x8u) {', '} else if (link_flags & 0x2u) {'),
    ("Disp setstate", "ae_MonkeySetState(r, 10);", "ae_MonkeySetState(r, 11);"),
    ("Disp 4", '} else if (link_flags & 0x4u) {', '} else if (link_flags & 0x2u) {'),
    ("Disp 10", '} else if (link_flags & 0x10u) {', '} else if (link_flags & 0x40u) {'),
    ("Disp cooldown", 'if (RDS16(monkey + AE_MK_ACTION_COOLDOWN) > 0) {', 'if (RDS16(monkey + AE_MK_ACTION_COOLDOWN) >= 0) {'),
    ("Disp cdoff", 'if (RDS16(monkey + AE_MK_ACTION_COOLDOWN) > 0) {', 'if (RDS16(monkey + AE_MK_COUNTER96) > 0) {'),
    ("Disp rand100", "} else if (rand_mod100(r) < 100u) {", "} else if (rand_mod100(r) < 99u) {"),
    ("Disp 1", '} else if (link_flags & 0x1u) {', '} else if (link_flags & 0x2u) {'),
    ("Disp 10000", '} else if (link_flags & 0x10000u) {', '} else if (link_flags & 0x20000u) {'),
    ("Disp 800000", '} else if (link_flags & 0x800000u) {', '} else if (link_flags & 0x400000u) {'),
    ("Disp 1000000", 'if (link_flags & 0x1000000u) {', 'if (link_flags & 0x2000000u) {'),
    ("Disp st61", 'if (RD8(monkey + AE_MK_AI_STATE) == 61) {', 'if (RD8(monkey + AE_MK_AI_STATE) == 60) {'),
    ("Disp st3", "if (RD8(c + AE_MK_AI_STATE) == 3 && !ae_AreaIsBossArena(r) && rand_mod100(r) < 10u) {", "if (RD8(c + AE_MK_AI_STATE) == 2 && !ae_AreaIsBossArena(r) && rand_mod100(r) < 10u) {"),
    ("Disp hopboss", "if (RD8(c + AE_MK_AI_STATE) == 3 && !ae_AreaIsBossArena(r) && rand_mod100(r) < 10u) {", "if (RD8(c + AE_MK_AI_STATE) == 3 && ae_AreaIsBossArena(r) && rand_mod100(r) < 10u) {"),
    ("Disp hoprand", "&& rand_mod100(r) < 10u) {", "&& rand_mod100(r) < 11u) {"),
    ("Disp hopf0", 'uint32_t f0 = RD32(RD16(reloaded_monkey + AE_MK_NAV_NODE0) * AE_NAV_NODE_SIZE + nodes);', 'uint32_t f0 = RD32(RD16(reloaded_monkey + AE_MK_NAV_NODE1) * AE_NAV_NODE_SIZE + nodes);'),
    ("Disp hopbit", "if (f0 & 1u) {", "if (f0 & 2u) {"),
    ("Disp hopf1", 'uint32_t f1 = RD32(RD16(reloaded_monkey + AE_MK_NAV_NODE1) * AE_NAV_NODE_SIZE + nodes);', 'uint32_t f1 = RD32(RD16(reloaded_monkey + AE_MK_NAV_NODE0) * AE_NAV_NODE_SIZE + nodes);'),
    ("Disp hopbit1", "if (f1 & 1u) blocked = 1;", "if (f1 & 2u) blocked = 1;"),
    ("Disp hopdist", '(RD32(reloaded_monkey + AE_MK_DIST_TO_GOAL) - 300u) < 501u', '(RD32(reloaded_monkey + AE_MK_DIST_TO_GOAL) - 300u) < 500u'),
    ("Disp hopdist2", '(RD32(reloaded_monkey + AE_MK_DIST_TO_GOAL) - 300u) < 501u', '(RD32(reloaded_monkey + AE_MK_DIST_TO_GOAL) - 299u) < 501u'),
    ("Disp hopdist3", '(RD32(reloaded_monkey + AE_MK_DIST_TO_GOAL) - 300u) < 501u', '(RD32(reloaded_monkey + AE_MK_DIST_XZ_TO_GOAL) - 300u) < 501u'),
    ("Disp jump", "if (ae_MonkeyIsJumpArcState(r)) return 1;", "if (!ae_MonkeyIsJumpArcState(r)) return 1;"),
    ("Disp st9", "if (st == 9 || st == 0x1A) return 1;", "if (st == 8 || st == 0x1A) return 1;"),
    ("Disp st1A", "if (st == 9 || st == 0x1A) return 1;", "if (st == 9 || st == 0x1B) return 1;"),
    ("Disp st35", "if (st == 0x35) return 1;", "if (st == 0x34) return 1;"),
    ("Disp f2200", "if (RD32(c + AE_MK_FLAGS2) & 0x200u) return 1;", "if (RD32(c + AE_MK_FLAGS2) & 0x100u) return 1;"),
    ("Disp boss2", 'if (ae_AreaIsBossArena(r)) return 1;\n    WR16(cur_monkey(r) + AE_MK_PREV_NODE, 0xFFFFu);', 'if (!ae_AreaIsBossArena(r)) return 1;\n    WR16(cur_monkey(r) + AE_MK_PREV_NODE, 0xFFFFu);'),
    ("Disp prev", 'WR16(cur_monkey(r) + AE_MK_PREV_NODE, 0xFFFFu);', 'WR16(cur_monkey(r) + AE_MK_PREV_NODE, 0xFFFEu);'),
    ("Disp prevoff", 'WR16(cur_monkey(r) + AE_MK_PREV_NODE, 0xFFFFu);', 'WR16(cur_monkey(r) + AE_MK_NAV_MARK_COUNT, 0xFFFFu);'),
    ("Disp entered", "    if (!entered) return 0;\n    if (ae_MonkeyIsJumpArcState(r)) return 1;", "    if (entered) return 0;\n    if (ae_MonkeyIsJumpArcState(r)) return 1;"),
    ("Turn boss", 'if (ae_AreaIsBossArena(r)) return 1;\n    uint32_t current_monkey = cur_monkey(r);\n    uint32_t current_node = RD16(current_monkey + AE_MK_NAV_NODE0);\n    int32_t previous_node = RDS16(current_monkey + AE_MK_PREV_NODE);\n    if (current_node == (uint32_t)previous_node) {\n        ae_MonkeySwapNavNodes(r, h);\n        return 1;', 'if (!ae_AreaIsBossArena(r)) return 1;\n    uint32_t current_monkey = cur_monkey(r);\n    uint32_t current_node = RD16(current_monkey + AE_MK_NAV_NODE0);\n    int32_t previous_node = RDS16(current_monkey + AE_MK_PREV_NODE);\n    if (current_node == (uint32_t)previous_node) {\n        ae_MonkeySwapNavNodes(r, h);\n        return 1;'),
    ("Turn hist0", 'int32_t previous_node = RDS16(current_monkey + AE_MK_PREV_NODE);\n    if (current_node == (uint32_t)previous_node) {\n        ae_MonkeySwapNavNodes(r, h);\n        return 1;', 'int32_t previous_node = RDS16(current_monkey + AE_MK_NAV_MARK_COUNT);\n    if (current_node == (uint32_t)previous_node) {\n        ae_MonkeySwapNavNodes(r, h);\n        return 1;'),
    ("Turn swapret", '        ae_MonkeySwapNavNodes(r, h);\n        return 1;\n    }\n    uint32_t next_node = RD16(current_monkey + AE_MK_NAV_NODE1);\n    if (next_node == (uint32_t)previous_node) return 1;', '        ae_MonkeySwapNavNodes(r, h);\n        return 0;\n    }\n    uint32_t next_node = RD16(current_monkey + AE_MK_NAV_NODE1);\n    if (next_node == (uint32_t)previous_node) return 1;'),
    ("Turn hist1", 'if (next_node == (uint32_t)previous_node) return 1;\n    if (ae_NavNodeLinkFlag(r, s16(current_node), s16(next_node)) == 0) return 1;', 'if (next_node == (uint32_t)previous_node) return 0;\n    if (ae_NavNodeLinkFlag(r, s16(current_node), s16(next_node)) == 0) return 1;'),
    ("Turn link01", 'if (ae_NavNodeLinkFlag(r, s16(current_node), s16(next_node)) == 0) return 1;', 'if (ae_NavNodeLinkFlag(r, s16(next_node), s16(current_node)) == 0) return 1;'),
    ("Turn link0ret", 'if (ae_NavNodeLinkFlag(r, s16(current_node), s16(next_node)) == 0) return 1;', 'if (ae_NavNodeLinkFlag(r, s16(current_node), s16(next_node)) == 0) return 0;'),
    ("Turn link10", "if (ae_NavNodeLinkFlag(r, RDS16(m + AE_MK_NAV_NODE1), RDS16(m + AE_MK_NAV_NODE0)) == 0) {\n        return ae_MonkeySwapNavNodes(r, h);", "if (ae_NavNodeLinkFlag(r, RDS16(m + AE_MK_NAV_NODE0), RDS16(m + AE_MK_NAV_NODE1)) == 0) {\n        return ae_MonkeySwapNavNodes(r, h);"),
    ("Turn swapv0", "        return ae_MonkeySwapNavNodes(r, h);\n    }\n    int32_t angle;", "        ae_MonkeySwapNavNodes(r, h);\n        return 1;\n    }\n    int32_t angle;"),
    ("Turn special", "if (ae_AreaIsSpecialMonkeyArea(r)) angle = ae_MonkeyApproachAngleNear(r);\n    else angle = ae_MonkeyApproachAngleFar(r);", "if (!ae_AreaIsSpecialMonkeyArea(r)) angle = ae_MonkeyApproachAngleNear(r);\n    else angle = ae_MonkeyApproachAngleFar(r);"),
    ("Turn dirx", 'WR16(sp + 16, RD16(target + AE_MK_POS) - RD16(monkey + AE_MK_POS));\n    WR16(sp + 18, 0);', 'WR16(sp + 16, RD16(target + AE_MK_POS) - RD16(monkey + AE_MK_POS) + 1u);\n    WR16(sp + 18, 0);'),
    ("Turn dirz", 'WR16(sp + 20, RD16(target + AE_MK_POS + 8) - RD16(monkey + AE_MK_POS + 8));', 'WR16(sp + 20, RD16(target + AE_MK_POS + 8) - RD16(monkey + AE_MK_POS + 4));'),
    ("Turn cnt", 'uint32_t cnt0 = RD8(current_node_again * AE_NAV_NODE_SIZE + nodes + slot + AE_NAV_NODE_VISIT_MARKS);', 'uint32_t cnt0 = RD8(current_node_again * AE_NAV_NODE_SIZE + nodes + slot + 0x15);'),
    ("Turn cnt1", 'uint32_t cnt1 = RD8(n1 * AE_NAV_NODE_SIZE + nodes + slot + AE_NAV_NODE_VISIT_MARKS);', 'uint32_t cnt1 = RD8(n1 * AE_NAV_NODE_SIZE + nodes + 0x14);'),
    ("Turn lt", "if (cnt0 < cnt1) {", "if (cnt0 <= cnt1) {"),
    ("Turn pos1", 'ae_NavNodeGetPos(r, h, (int32_t)n1, sp + 32);', 'ae_NavNodeGetPos(r, h, (int32_t)current_node_again, sp + 32);'),
    ("Turn pos0", 'ae_NavNodeGetPos(r, h, (int32_t)current_node_again, sp + 32);\n    if (ae_ram_failed(r)) return 0;\n    if (node_dir_and_hesitate', 'ae_NavNodeGetPos(r, h, (int32_t)n1, sp + 32);\n    if (ae_ram_failed(r)) return 0;\n    if (node_dir_and_hesitate'),
    ("Turn dirn", 'WR16(sp + 24, node_component - monkey_component);', 'WR16(sp + 24, node_component - monkey_component + 1u);'),
    ("Turn dirn2", 'WR16(sp + 28, node_component - monkey_component);', 'WR16(sp + 28, node_component - monkey_component - 1u);'),
    ("Turn dirny", "WR16(sp + 26, 0);", "WR16(sp + 26, 1);"),
    ("Turn hesA", "        if (hes == 0) {\n            int32_t dot", "        if (hes != 0) {\n            int32_t dot"),
    ("Turn dotA", "            if (!(dot < c)) return 0;\n        }\n        ae_MonkeySwapNavNodes(r, h);\n        return 1;", "            if (!(dot <= c)) return 0;\n        }\n        ae_MonkeySwapNavNodes(r, h);\n        return 1;"),
    ("Turn retA", "            if (!(dot < c)) return 0;", "            if (!(dot < c)) return 1;"),
    ("Turn hesB", "if (node_dir_and_hesitate(r, sp) != 0) return 1;", "if (node_dir_and_hesitate(r, sp) != 0) return 0;"),
    ("Turn dotB", "    return dot < c ? 1 : 0;", "    return dot <= c ? 1 : 0;"),
    ("Turn dotvec", "int32_t dot = s16((uint32_t)ae_DotXZ_SV(r, sp + 16, sp + 24));\n    int32_t c = ae_sdk_rcos(r, angle);\n    return", "int32_t dot = s16((uint32_t)ae_DotXZ_SV(r, sp + 16, sp + 16));\n    int32_t c = ae_sdk_rcos(r, angle);\n    return"),
    ("Turn cosB", "int32_t c = ae_sdk_rcos(r, angle);\n    return dot < c ? 1 : 0;", "int32_t c = ae_sdk_rcos(r, angle + 1);\n    return dot < c ? 1 : 0;"),
    ("Rand hist0", 'uint32_t sp = AE_STK(AE_STK_TURN_BACK_RANDOM);\n    uint32_t current_monkey = cur_monkey(r);\n    uint32_t current_node = RD16(current_monkey + AE_MK_NAV_NODE0);\n    int32_t previous_node = RDS16(current_monkey + AE_MK_PREV_NODE);', 'uint32_t sp = AE_STK(AE_STK_TURN_BACK_RANDOM);\n    uint32_t current_monkey = cur_monkey(r);\n    uint32_t current_node = RD16(current_monkey + AE_MK_NAV_NODE0);\n    int32_t previous_node = RDS16(current_monkey + AE_MK_NAV_MARK_COUNT);'),
    ("Rand hist1", '    uint32_t next_node = RD16(current_monkey + AE_MK_NAV_NODE1);\n    if (next_node == (uint32_t)previous_node) return;\n    if (ae_NavNodeLinkFlag(r, s16(current_node), s16(next_node)) == 0) return;', '    uint32_t next_node = RD16(current_monkey + AE_MK_NAV_NODE1);\n    if (next_node != (uint32_t)previous_node) return;\n    if (ae_NavNodeLinkFlag(r, s16(current_node), s16(next_node)) == 0) return;'),
    ("Rand link0", 'if (ae_NavNodeLinkFlag(r, s16(current_node), s16(next_node)) == 0) return;\n    uint32_t m = cur_monkey(r);\n    if (ae_NavNodeLinkFlag(r, RDS16(m + AE_MK_NAV_NODE1), RDS16(m + AE_MK_NAV_NODE0)) != 0) {', 'if (ae_NavNodeLinkFlag(r, s16(next_node), s16(current_node)) == 0) return;\n    uint32_t m = cur_monkey(r);\n    if (ae_NavNodeLinkFlag(r, RDS16(m + AE_MK_NAV_NODE1), RDS16(m + AE_MK_NAV_NODE0)) != 0) {'),
    ("Rand link1", "RDS16(m + AE_MK_NAV_NODE0)) != 0) {\n        if (rand_mod10", "RDS16(m + AE_MK_NAV_NODE0)) == 0) {\n        if (rand_mod10"),
    ("Rand mod7", "if (rand_mod10(r) < 7u) return;", "if (rand_mod10(r) < 6u) return;"),
    ("Rand mod7b", "if (rand_mod10(r) < 7u) return;", "if (rand_mod10(r) < 8u) return;"),
    ("Rand zero", "if (RDS16(sp + 18) == 0) return;", "if (RDS16(sp + 16) == 0) return;"),
    ("Rand 682", 'if (!(ae_sdk_rcos(r, 682) < current_node_cosine)) return;', 'if (!(ae_sdk_rcos(r, 683) < current_node_cosine)) return;'),
    ("Rand cmp682", 'if (!(ae_sdk_rcos(r, 682) < current_node_cosine)) return;', 'if (!(ae_sdk_rcos(r, 682) <= current_node_cosine)) return;'),
    ("Rand out", "if (!(RDS16(sp + 18) < RDS16(sp + 16))) return;", "if (!(RDS16(sp + 18) <= RDS16(sp + 16))) return;"),
    ("Rand outsw", "if (!(RDS16(sp + 18) < RDS16(sp + 16))) return;", "if (!(RDS16(sp + 16) < RDS16(sp + 18))) return;"),
]

MUT_FUNCS = {
    "Disp": ["MonkeyNodeActionDispatch"], "Turn": ["MonkeyConsiderTurnBack"], "Rand": ["MonkeyConsiderTurnBackRandom"],
}


def run_mutations(scale, seed, only_labels=()):
    rel = "src/monkeys/ae_monkey_level5.c"
    text = (ROOT / rel).read_text(encoding="utf-8")
    survivors = []
    tmp = Path(tempfile.mkdtemp(prefix="mk5_mut_"))
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
            h.reload_c({rel: mp}, tag=f"mk5mut{k}")
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
    ap.add_argument("--cases", type=float, default=30, help="scale of the case counts in percent")
    ap.add_argument("--seed", type=int, default=0x944251)
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
