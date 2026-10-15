"""Differential check of monkey port batch 4a (src/monkeys/ae_monkey_level4.c, call-graph level 4) against the original MIPS
code: for every ported function many scenarios are built in a byte-exact RAM image (SCUS_944.23 at its real addresses; monkeys /
nav nodes / extra link records / MonkeyInfo / animation tables / models / globals laid out as MonkeySpawnAll and the level loader
leave them, plus randomised and edge-case states); the original instructions (GTE through the PCSX-ReARMed oracle) and the C
port run on identical images with identical hook behaviour; return value, RAM outside the stack window, scratchpad and the
sequence of hook calls must be identical.  The batch 1..3 functions the code calls run as the C ports on the C side and as the
original instructions on the MIPS side (so this also checks them in context).  Needs extract/disc/SCUS_944.23 (skips if
missing).  See tools/mips/monkey_harness.py, monkey_scen.py, monkey_scen4.py and check_monkey_level3.py.

Usage: uv run python tools/mips/check_monkey_level4.py [--cases PERCENT] [--seed S] [--coverage] [--trapv] [--mutate]
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
from monkey_scen import combat, navpos_world, anim_globals, allowed_world, M32, s32, s16, u16, FRAME, G_TARGET  # noqa: E402

# name -> (original address, size, return kind: s = int32 / u = uint32 / v = void, takes hooks, argument kinds[, C name])
FUNCS = {
    "MonkeySetNavNode": (0x80035080, 236, "v", True, "s"),
    "MonkeyEnterState_1D": (0x800359F0, 140, "s", True, ""),
    "MonkeyEnterState_04": (0x80035A7C, 340, "s", True, ""),
    "MonkeyEnterState_2C": (0x80036964, 208, "s", True, ""),
    "MonkeyPickBestLink": (0x80038120, 1256, "s", True, "s"),
    "MonkeySwapNavNodes": (0x80038694, 52, "s", True, ""),
    "MonkeyAi_Fall": (0x8003FCD8, 164, "v", True, ""),
    "MonkeyArriveAtNode": (0x80040D90, 1264, "s", True, "s"),
    "MonkeyUpdateVisuals": (0x80042B68, 688, "v", True, ""),
}
# code reached through the functions above (for the coverage report)
EXTRA_COVERAGE = {
    "MonkeySetState (batch 3a)": (0x80033220, 216), "MonkeyUpdateLinkSlope (batch 3a)": (0x80035370, 260),
    "MonkeyUpdateGoalPos (batch 3a)": (0x80035474, 360), "MonkeyUpdateGoal (batch 3b)": (0x800355DC, 40),
    "MonkeyNavLinkAllowed (batch 3a)": (0x80037E8C, 456), "MonkeyPrepareJumpArc (batch 3a)": (0x800364BC, 0x270),
    "MonkeyMarkBlockedLinks (batch 3b)": (0x80038054, 204), "MonkeyEnterState_30 (batch 3b)": (0x80035DD8, 148),
    "MonkeyEnterState_32 (batch 3a)": (0x80035918, 0x9C), "MonkeyEnterRandomLook (batch 3b)": (0x800363E4, 56),
    "MonkeyDrawModel (batch 3b)": (0x80039ED4, 1120), "MonkeySelectLod (batch 2)": (0x8003A33C, 216),
    "MonkeyProjectToScreen (batch 2)": (0x800393E4, 332), "MonkeyBodyOff (batch 2)": (0x80036B24, 84),
    "MonkeyPickAnim (batch 2)": (0x800331B0, 112), "MonkeyFootDust (batch 2)": (0x80038F00, 216),
    "NavLinkLookupExtra (batch 2)": (0x8003B880, 320), "NavNodeGetPos (batch 2)": (0x80034EF8, 392),
    "MonkeyRandomHesitate (batch 2)": (0x80032F24, 0x70), "MonkeyQueueVoice (batch 1)": (0x80033744, 128),
    "MonkeyGetAnimIndex (batch 1)": (0x80032E84, 160), "NavNodeGetLinkIds (batch 1)": (0x80034B74, 148),
    "NavNodeGetLink (batch 1)": (0x8003386C, 0x120), "AreaIsBossArena (batch 1)": (0x800339EC, 80),
    "AreaIsSpecialMonkeyArea (batch 1)": (0x8003398C, 0x40), "MonkeyApproachAngleNear (batch 1)": (0x80037D6C, 0x80),
    "MonkeyApproachAngleFar (batch 1)": (0x80037DEC, 0xA0), "MonkeyIsState0C_or_24 (batch 1)": (0x80033C00, 52),
    "MonkeyIsJumpArcState (batch 1)": (0x80033C34, 76), "MonkeyRand (batch 1)": (0x80032E30, 40),
    "rcos (SDK)": (0x8002FC20, 160), "BodyContactType (bodies)": (0x800392E4, 12),
}

STATE_ENTRY = {"MonkeyEnterState_1D": 29, "MonkeyEnterState_04": 4, "MonkeyEnterState_2C": 0x2C}


def check_all(h, scale, seed):
    t0 = time.time()
    # monkey +0xC8: MonkeyUpdateGoalPos copies four words of the position buffer into +0xBC..+0xC8 although NavNodeGetPos only
    # fills three; the fourth is leftover stack (see the notes) and no code reads +0xC8, so it is not compared
    h.mask = [(MONKEYS + MONKEY_SIZE * k + 0xC8 - 0x80000000, 4) for k in range(8)]
    mh.reg.declare("shared_random", "MonkeySwapNavNodes $v0 (return value)")      # the $v0 left by Distance3D in MonkeyUpdateGoalPos

    def n(count):
        return max(1, int(count * scale / 100))

    def rng_of(name, i):
        return random.Random(f"{seed}/{name}/{i}")

    def run(name, count, setup=None, argsf=lambda c: (), nh=False, i0=0, **kw):
        """setup(m, rng, i) -> context; one fresh scenario per case"""
        for i in range(i0, i0 + n(count)):
            rng = rng_of(name, i)
            h.run(name, f"#{i}", i, (lambda m, rng=rng, i=i: setup(m, rng, i)) if setup else (lambda m, rng=rng: combat(m, rng)),
                  (lambda c, rng=rng, i=i: argsf(c, rng, i)) if argsf.__code__.co_argcount == 3 else argsf,
                  null_hooks=(nh and i % 8 == 7), **kw)

    # ------------------------------------------------------------------ SetNavNode / SwapNavNodes
    def navnode_world(m, rng, i):
        c = s4.goal_world(m, rng)
        w = W(m)
        w.w8(c + 0x78, rng.choice([9, 0x1A, 0x35, 4, 0x2C, 0x2D, 0x2E, 0x31, 0x0C, rng.randint(0, 0x42), rng.randint(0, 0x42)]))
        w.w32(c + 0x228, rng.choice([0, 1, 0x200, 0x201, rng.getrandbits(32), 2]) & M32)
        w.w16(0x800F4472, rng.choice([0, 1, 3, 6, 8, 2, 5]))
        s4.stale_goalpos(m, rng, 24)
        return c
    run("MonkeySetNavNode", 300, navnode_world, nh=True, allow_trap=True,
        argsf=lambda c, rng, i: (rng.choice([rng.randint(0, 40), rng.getrandbits(16), rng.getrandbits(32), 0xFFFF, -1]) & M32,))
    # the states x flags2 (bit 0 / bit 9) x boss x the others (every combination of the final copy condition)
    ci = 100000
    for st in range(0x43):
        for f2 in (0, 1, 0x200, 0x201):
            for boss in (0, 3):
                rng = rng_of("MonkeySetNavNode-grid", ci)

                def setup(m, rng=rng, st=st, f2=f2, boss=boss):
                    c = s4.goal_world(m, rng)
                    w = W(m)
                    w.w8(c + 0x78, st)
                    w.w32(c + 0x228, f2)
                    w.w16(0x800F4472, boss)
                    s4.stale_goalpos(m, rng, 24)
                    return c
                h.run("MonkeySetNavNode", f"grid {st} {f2} {boss}", ci, setup, lambda c, rng=rng: (rng.randint(0, 40),), allow_trap=True)
                ci += 1

    def swap_world(m, rng, i):
        c = s4.goal_world(m, rng)
        s4.stale_goalpos(m, rng, 24)
        return c
    run("MonkeySwapNavNodes", 400, swap_world, nh=True, allow_trap=True)

    # ------------------------------------------------------------------ state entries 1D / 04 / 2C
    def state1d_world(m, rng, i):
        c = s4.lookup_world(m, rng, 29)
        # the out word of NavLinkLookupExtra is only written when the link has an extra record: the stale stack slot decides
        s4.set_stale(m, "ENTER_1D", 40, 16, rng.choice([rng.getrandbits(32), 0, 0xFFFF, 0x7FFF8000, rng.getrandbits(16)]))
        s4.stale_goalpos(m, rng, 40)
        return c
    run("MonkeyEnterState_1D", 600, state1d_world, nh=True, allow_trap=True)

    def state04_world(m, rng, i):
        c = s4.nav_entry_world(m, rng, own=STATE_ENTRY["MonkeyEnterState_04"])
        s4.stale_goalpos(m, rng, 32)
        return c
    run("MonkeyEnterState_04", 600, state04_world, nh=True, allow_trap=True)

    def state2c_world(m, rng, i):
        c = s4.nav_entry_world(m, rng, own=0x2C)
        s4.stale_goalpos(m, rng, 32)
        return c
    run("MonkeyEnterState_2C", 500, state2c_world, nh=True, allow_trap=True)

    # ------------------------------------------------------------------ fall
    def fall_world(m, rng, i):
        c = combat(m, rng)
        w = W(m)
        w.w16(c + 0x92, rng.choice([0, 1, 2, 5, 20, 100, 181, 182, 0x7FFF, 0x8000, 0xFFFF, rng.getrandbits(16), rng.randint(0, 60)]))
        w.w16(c + 0xE4, rng.choice([0, 100, -100, 0x7FFF, 0x8000, rng.getrandbits(16)]) & 0xFFFF)
        w.w32(c + 0x248, rng.getrandbits(32))
        w.w8(c + 0x78, rng.choice([0x31, 0x30, 1, rng.randint(0, 0x42)]))
        anim_globals(m, rng, c)
        return c
    run("MonkeyAi_Fall", 600, fall_world, nh=True, allow_trap=True)

    # ------------------------------------------------------------------ ArriveAtNode
    NODE_BITS = [0x20, 0x1000, 0x4000, 0x800, 0x20000, 0x40000, 0x400, 0x80000, 4, 8, 0x8000, 0x10000]

    def arrive_world(m, rng, i, bits=None):
        c = navpos_world(m, rng)
        anim_globals(m, rng, c)
        w = W(m)
        nn = m.read16(0x800B4D18)
        node = rng.randint(0, nn - 1)
        a = NAV_BASE + 0x18 * node
        if bits is None:
            bits = sum(b for b in NODE_BITS if rng.random() < rng.choice([0.15, 0.3, 0.5]))
        w.w32(a, (m.read32(a) & 0x1 if rng.random() < 0.5 else 0) | bits | (rng.getrandbits(32) & 0x100 if rng.random() < 0.3 else 0))
        w.w8(c + 0x7A, rng.choice([4, 4, 4, 4, 4, 3, 0, 2, 5, rng.getrandbits(8)]))
        w.w8(c + 0x79, rng.choice([0, 1, 1, 2, 0, rng.getrandbits(8)]))
        dist = rng.choice([0, 500, 1499, 1500, 1501, 1499, 1500, rng.randint(-100, 4000), rng.getrandbits(32)])
        w.w32(c + 0x20C, dist & M32)
        fld = rng.choice([dist - 1, dist, dist + 1, rng.randint(-300, 3000), rng.getrandbits(16), -1, 0]) & 0xFFFF
        w.w16(a + 0x10, fld if not (bits & 0x400 and rng.random() < 0.5) else rng.choice([rng.randint(0, 15) * 100 + rng.randint(0, 99), fld]))
        n1 = rng.randint(0, nn - 1)
        w.w16(c + 0xF4, n1)
        w.w16(c + 0xF2, rng.randint(0, nn - 1))
        b = NAV_BASE + 0x18 * n1
        w.w16(b + 0x10, rng.choice([rng.randint(0, 15) * 100 + rng.randint(0, 99), rng.getrandbits(16), -1, -150, 32767]) & 0xFFFF)
        w.w32(c + 0x50, rng.getrandbits(32))
        ctype = (m.read32(c + 0x50) >> 28) & 0xF
        w.w16(c + 0x126, rng.choice([ctype, ctype, rng.getrandbits(16), (ctype + 1) & 0xF]))
        w.w32(c + 0x248, rng.choice([0, rng.getrandbits(32), 0xFFFFFFFF]) & M32)
        w.w32(c + 0x24C, rng.choice([0, 1, rng.getrandbits(32)]))
        w.w16(0x800F4472, rng.choice([0, 1, 3, 6, 8, 2, 5]))
        w.w32(c + 0x22C, rng.getrandbits(32))
        return {"cur": c, "node": node}

    run("MonkeyArriveAtNode", 1500, arrive_world, nh=True, allow_trap=True,
        argsf=lambda c, rng, i: (rng.choice([c["node"], c["node"], c["node"], c["node"] | 0x10000, rng.randint(0, 40)]) & M32,))
    # every single node flag bit and the pairs of the exclusive-looking ones
    ci = 200000
    combos = [b for b in NODE_BITS] + [a | b for a in (4, 8, 0x8000, 0x10000) for b in (4, 8, 0x8000, 0x10000) if a < b] + [0]
    for bits in combos:
        for rep in range(max(2, n(10))):
            rng = rng_of("MonkeyArriveAtNode-bits", ci)
            h.run("MonkeyArriveAtNode", f"bits {bits:X} #{rep}", ci,
                  lambda m, rng=rng, bits=bits, ci=ci: arrive_world(m, rng, ci, bits=bits),
                  lambda c: (c["node"],), allow_trap=True)
            ci += 1

    # node flag 8 (not 4) outside the boss arenas: the node's value at +0x10 equal to / one off the target distance (exact `<` test)
    def arrive_look_world(m, rng, i):
        r_ = arrive_world(m, rng, i, bits=8)
        w = W(m)
        a = NAV_BASE + 0x18 * r_["node"]
        dist = rng.choice([0, 1, 500, 1499, 1500, rng.randint(-100, 4000), -5])
        w.w32(r_["cur"] + 0x20C, dist & M32)
        w.w16(a + 0x10, (dist + rng.choice([0, 0, -1, 1])) & 0xFFFF)
        w.w16(0x800F4472, rng.choice([0, 1, 2, 5]))
        w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x43]))
        return r_
    run("MonkeyArriveAtNode", 300, arrive_look_world, nh=True, allow_trap=True, i0=70000,
        argsf=lambda c, rng, i: (c["node"],))

    # ------------------------------------------------------------------ PickBestLink
    def pick_world(m, rng, i, boss=None):
        c = s4.links_world(m, rng)
        anim_globals(m, rng, c)
        w = W(m)
        nn = m.read16(0x800B4D18)
        s4.set_link_counts(m, rng, nn)
        node0 = rng.randint(0, nn - 1)
        # a node with several usable links most of the time
        best = max(range(nn), key=lambda k: m.read8(NAV_BASE + 0x18 * k + 0xB) + rng.random())
        node0 = best if rng.random() < 0.7 else node0
        w.w16(c + 0xF2, node0)
        w.w16(c + 0xF4, rng.randint(0, nn - 1))
        a = NAV_BASE + 0x18 * node0
        links = []
        cnt = m.read8(a + 0xA)
        for j in range(cnt):
            e = m.read16(m.read32(a + 0xC) + 2 * j)
            if e & 0x4000:
                links.append(e & 0x3FFF)
        w.w16(c + 0xEC, rng.choice(links + [rng.randint(0, 40), 0xFFFF]) if links else rng.randint(0, 40))
        w.w32(c + 0x248, rng.choice([rng.getrandbits(32), rng.getrandbits(32) | (1 << 26), rng.getrandbits(32) & ~(1 << 26), 1 << 27, 0]) & M32)
        w.w8(c + 0x78, rng.choice([0x3D, 0x0C, 0x24, 1, rng.randint(0, 0x42), 0x3D]))
        w.w8(c + 0x79, rng.choice([0, 1, 1, 2]))
        s4.visit_counters(m, rng, c, nn)
        if rng.random() < 0.35:                      # the current node's own counter equal to the best neighbour's (ties)
            slot = m.read8(c + 0x7B)
            best = max([m.read8(NAV_BASE + 0x18 * q + slot + 0x14) for q in links] or [0])
            w.w8(NAV_BASE + 0x18 * node0 + slot + 0x14, best + rng.choice([0, 0, 0, 1, -1]) & 0xFF)
        w.w16(0x800F4472, rng.choice([0, 1, 2]) if boss is None else boss)
        if boss is None and rng.random() < 0.12:
            w.w16(0x800F4472, rng.choice([3, 6, 8]))
        w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x43, 0x35, 0x46]))
        for k in range(8):                                                  # hesitate depends on the info record
            pass
        cand = s4.cos_candidates(m)
        for j in range(52):
            v = rng.choice(cand) + rng.choice([0, 0, 0, 1, -1, 2, -2])
            if rng.random() < 0.3:
                v = rng.choice([-4096, 4096, 0, rng.randint(-4096, 4096), rng.getrandbits(16)])
            w.w16(c + 0x176 + 2 * j, v & 0xFFFF)
        return c
    run("MonkeyPickBestLink", 1400, pick_world, nh=True, allow_trap=True,
        argsf=lambda c, rng, i: (rng.choice([0, 0, 0, 1, 0x10000, 0x100, 0x8000, 0x1000, -1, rng.getrandbits(32)]) & M32,))
    def pick_boss_world(m, rng, i):
        c = pick_world(m, rng, i, boss=rng.choice([3, 6, 8]))
        w = W(m)
        node0 = m.read16(c + 0xF2)
        w.w8(NAV_BASE + 0x18 * node0 + 0xB, rng.choice([0, 1, 2, 3, 3, 5, 5, 7, 7, 9, 11, 13, 255]))
        w.w32(0x800B4D10, rng.choice([rng.getrandbits(16), rng.getrandbits(32), 0xFFFF, 0x7FFF, 0x8000]))
        return c
    run("MonkeyPickBestLink", 300, pick_boss_world, nh=True, allow_trap=True, i0=50000,
        argsf=lambda c, rng, i: (rng.choice([0, 1]),))

    def pick_veto_world(m, rng, i):
        """alerted, not a boss arena, state != 0x3D, the cosines of the links split around the threshold, so that the best link is
        vetoed and the second pass takes (or also rejects) another link"""
        c = pick_world(m, rng, i, boss=0)
        w = W(m)
        w.w8(c + 0x79, 1)
        w.w8(c + 0x78, rng.choice([1, 3, 0x0C, 0x24, 0x10]))
        w.w16(0x800F4476, rng.choice([0x10, 0x10, 0x29]))         # not a boss area; 0x10 takes the far / fixed-angle branch
        fl = m.read32(c + 0x248)
        w.w32(c + 0x248, (fl | (1 << 26)) if rng.random() < 0.5 else (fl & ~(1 << 26) & M32))
        if rng.random() < 0.5:
            w.w8(c + 0xD4, 0)
        cand = s4.cos_candidates(m)
        for j in range(52):                          # every link gets a value around one of the possible thresholds
            w.w16(c + 0x176 + 2 * j, (rng.choice(cand) + rng.choice([-2, -1, 0, 0, 1, 1, 2, 5, -5])) & 0xFFFF)
        return c
    run("MonkeyPickBestLink", 700, pick_veto_world, nh=True, allow_trap=True, i0=60000,
        argsf=lambda c, rng, i: (rng.choice([0, 0, 1]),))

    # ------------------------------------------------------------------ UpdateVisuals
    def visuals_world(m, rng, i):
        c = s4.draw_world(m, rng)
        w = W(m)
        ptrs = [MONKEYS + MONKEY_SIZE * rng.randint(0, 7) for _ in range(4)]
        if rng.random() < 0.03:
            ptrs[rng.randint(0, 3)] = 0
        cam = [rng.randint(-8000, 8000) for _ in range(3)]
        w.w16(0x800F45A8, cam[0] & 0xFFFF)
        w.w16(0x800F45A8 + 2, cam[1] & 0xFFFF)
        w.w16(0x800F45A8 + 4, cam[2] & 0xFFFF)
        for k, p in enumerate(ptrs):
            w.w32(0x800E54F0 + 4 * k, p)
            if p == 0:
                continue
            dx = rng.choice([rng.randint(-100, 100), 5990, 6000, 6001, -6000, rng.randint(-7000, 7000), rng.randint(-3000, 3000)])
            w.w32(p + 0x24, (cam[0] + dx) & M32)
            w.w32(p + 0x28, (cam[1] + rng.randint(-400, 400)) & M32)
            w.w32(p + 0x2C, (cam[2] + rng.choice([0, rng.randint(-1000, 1000)])) & M32)
            top = rng.choice([0x40020000, 0x40020000, 0x40030000, 0x40000000, 0, 0x00020000, 0x00030000, 0x80000000])
            w.w32(p + 0x248, (top | (rng.getrandbits(32) & ~0xC0030000 & M32 if rng.random() < 0.5 else 0)) & M32)
            lim = rng.choice([0, 1, 5, 100, 0x7FFF, 0x8000, 0xFFFF, rng.getrandbits(16)])
            w.w16(p + 0x128, lim)
            w.w16(p + 0xF0, rng.choice([0, -1, 1, lim - 1, lim, lim + 1, -5, 5, rng.getrandbits(16)]) & 0xFFFF)
            w.w32(p + 0x228, rng.choice([0, 0x4000, rng.getrandbits(32)]))
            w.w8(p + 0x7A, rng.choice([4, 3, 2, 1, 0, 4, 4, rng.getrandbits(8)]))
            w.w8(p + 0x7B, rng.randint(0, 7))
            w.w8(p + 0x78, rng.choice([30, 32, 1, 0x1A, 0x0C, rng.randint(0, 0x42)]))
            w.w16(p + 0x168, rng.choice([0, 0, 100, 500, rng.getrandbits(16)]))
            w.w32(p + 0x160, rng.choice([0xFFFFFFFF, 0xFFFFFFFF, 0, 5, rng.getrandbits(32)]))
            w.w16(p + 0x80, rng.getrandbits(16))
            w.w16(p + 0x82, rng.getrandbits(16))
            w.w8(p + 0x7C, rng.randint(0, 15))
            w.w8(p + 0xD5, rng.choice([0, 1, 2, 3]))
            w.w32(p + 0x14, rng.choice([0x801C9400, 0x801C9500]))
        for off in range(0, 0x18, 2):
            w.w16(0x800F4B30 + off, rng.getrandbits(16))
        for off in range(0x3F0, 0x400, 4):
            m.write32(0x1F800000 + off, rng.getrandbits(32))
        w.w8(0x800AEB58, rng.choice([0, 2, 2, 1, rng.getrandbits(8)]))
        w.w32(FRAME, rng.choice([rng.getrandbits(32), rng.randint(0, 100), 0xFFFFFFFF]))
        w.w16(0x800F4476, rng.choice([0x24, 0x24, 0x46, 0x10, 0x29]))
        w.w32(0x800B4D08, ptrs[0] or MONKEYS)
        w.w16(0x800F45A8 + 6, rng.getrandbits(16))
        return c
    run("MonkeyUpdateVisuals", 800, visuals_world, nh=True, allow_trap=True)

    print(f"scenario families done in {time.time() - t0:.1f}s")


MUTATIONS = [
    ("NavNode shift", 'for (int32_t history_index = 5; history_index > 0; --history_index) {', 'for (int32_t history_index = 4; history_index > 0; --history_index) {'),
    ("NavNode store", 'WR16(reloaded_monkey + AE_MK_NAV_NODE0, node);                       /* Store before the following call reads the monkey. */', 'WR16(reloaded_monkey + AE_MK_NAV_NODE1, node);                       /* Store before the following call reads the monkey. */'),
    ("NavNode st9", "if (st == 9 || st == 0x1A) {", "if (st == 9 || st == 0x1B) {"),
    ("NavNode st35", "} else if (st == 0x35) {", "} else if (st == 0x34) {"),
    ("NavNode f2bit", '} else if (RD32(state_monkey + AE_MK_FLAGS2) & 0x200u) {', '} else if (RD32(state_monkey + AE_MK_FLAGS2) & 0x100u) {'),
    ("NavNode jbit", "if (RD32(cur_monkey(r) + AE_MK_FLAGS2) & 1u) copy = 1;", "if (RD32(cur_monkey(r) + AE_MK_FLAGS2) & 2u) copy = 1;"),
    ("NavNode boss", "if (boss == 0) return;", "if (boss != 0) return;"),
    ("NavNode copy", 'WR16(updated_monkey + AE_MK_PREV_NODE, RD16(updated_monkey + AE_MK_NAV_NODE1));', 'WR16(updated_monkey + AE_MK_PREV_NODE, RD16(updated_monkey + AE_MK_NAV_NODE0));'),
    ("1D state", 'WR8(cur_monkey(r) + AE_MK_AI_STATE, 29);     /* Store before the following call reads the monkey. */', 'WR8(cur_monkey(r) + AE_MK_AI_STATE, 28);     /* Store before the following call reads the monkey. */'),
    ("1D skipoff", "if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != 29) {", "if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != 28) {"),
    ("1D lookup", 'ae_NavLinkLookupExtra(r, next_node, current_node, sp + 16);', 'ae_NavLinkLookupExtra(r, current_node, next_node, sp + 16);'),
    ("1D out", "uint32_t out = RD16(sp + 16);", "uint32_t out = RD16(sp + 18);"),
    ("1D clear", 'WR16(reloaded_monkey + AE_MK_COUNTER96, 0);\n    WR16(reloaded_monkey + AE_MK_PREV_NODE, n1);\n    return 1;', 'WR16(reloaded_monkey + AE_MK_COUNTER96, 1);\n    WR16(reloaded_monkey + AE_MK_PREV_NODE, n1);\n    return 1;'),
    ("1D n1", 'uint32_t n1 = RD16(reloaded_monkey + AE_MK_NAV_NODE1);\n    WR16(reloaded_monkey + AE_MK_ANIM_LENGTH, out);', 'uint32_t n1 = RD16(reloaded_monkey + AE_MK_NAV_NODE0);\n    WR16(reloaded_monkey + AE_MK_ANIM_LENGTH, out);'),
    ("1D timer", 'WR16(reloaded_monkey + AE_MK_ANIM_LENGTH, out);', 'WR16(reloaded_monkey + 0x12A, out);'),
    ("04 state", 'WR8(monkey + AE_MK_AI_STATE, 4);\n    WR16(monkey + AE_MK_PREV_NODE, next_node);', 'WR8(monkey + AE_MK_AI_STATE, 5);\n    WR16(monkey + AE_MK_PREV_NODE, next_node);'),
    ("04 count", 'WR16(cur_monkey(r) + AE_MK_JUMP_RISE_FRAMES, 5);', 'WR16(cur_monkey(r) + AE_MK_JUMP_RISE_FRAMES, 6);'),
    ("04 len", 'uint32_t landing_frames = last - first + 1u;\n    WR16(cm + AE_MK_JUMP_LAND_FRAMES, landing_frames);', 'uint32_t landing_frames = last - first;\n    WR16(cm + AE_MK_JUMP_LAND_FRAMES, landing_frames);'),
    ("04 boss", "if (ae_AreaIsBossArena(r)) {\n        ae_MonkeyQueueVoice(r, h, 177, 0);", "if (!ae_AreaIsBossArena(r)) {\n        ae_MonkeyQueueVoice(r, h, 177, 0);"),
    ("04 v177", "ae_MonkeyQueueVoice(r, h, 177, 0);", "ae_MonkeyQueueVoice(r, h, 176, 0);"),
    ("04 v17", "ae_MonkeyQueueVoice(r, h, 17, 0);", "ae_MonkeyQueueVoice(r, h, 17, 1);"),
    ("04 v19", 'ae_MonkeyQueueVoice(r, h, 19, (int32_t)(last_frame - l - 1u));', 'ae_MonkeyQueueVoice(r, h, 19, (int32_t)(last_frame - l));'),
    ("04 v68", "ae_MonkeyQueueVoice(r, h, 68, 3);\n        uint32_t f2;\n        uint32_t l2 = anim_span_parts(r, &f2);\n        ae_MonkeyQueueVoice(r, h, 69, (int32_t)(l2 - f2 + 1u));", "ae_MonkeyQueueVoice(r, h, 68, 4);\n        uint32_t f2;\n        uint32_t l2 = anim_span_parts(r, &f2);\n        ae_MonkeyQueueVoice(r, h, 69, (int32_t)(l2 - f2 + 1u));"),
    ("04 v69", "ae_MonkeyQueueVoice(r, h, 69, (int32_t)(l2 - f2 + 1u));", "ae_MonkeyQueueVoice(r, h, 69, (int32_t)(l2 - f2));"),
    ("04 jump", "ae_MonkeyPrepareJumpArc(r, h);\n    if (ae_ram_failed(r)) return 0;\n    ae_MonkeyFootDust(r, h);\n    if (ae_ram_failed(r)) return 0;\n    if (ae_AreaIsBossArena(r)) {", "ae_MonkeyFootDust(r, h);\n    if (ae_ram_failed(r)) return 0;\n    if (ae_AreaIsBossArena(r)) {"),
    ("04 node", 'uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);\n    WR8(monkey + AE_MK_AI_STATE, 4);', 'uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE0);\n    WR8(monkey + AE_MK_AI_STATE, 4);'),
    ("2C state", "WR8(monkey + AE_MK_AI_STATE, 0x2C);", "WR8(monkey + AE_MK_AI_STATE, 0x2D);"),
    ("2C count", 'WR16(monkey + AE_MK_JUMP_RISE_FRAMES, 3);', 'WR16(monkey + AE_MK_JUMP_RISE_FRAMES, 4);'),
    ("2C len", 'WR16(monkey_after_anim + AE_MK_JUMP_LAND_FRAMES, last_frame - first_frame + 1u);', 'WR16(monkey_after_anim + AE_MK_JUMP_LAND_FRAMES, last_frame - first_frame);'),
    ("2C entry", 'uint32_t last_frame = RD16(tab + 34u * 8u + AE_MK_ANIM_LAST_FRAME);', 'uint32_t last_frame = RD16(tab + 0x11Au);'),
    ("2C v69", "ae_MonkeyQueueVoice(r, h, 69, (int32_t)(l2 - f2));\n    return 1;", "ae_MonkeyQueueVoice(r, h, 69, (int32_t)(l2 - f2 + 1u));\n    return 1;"),
    ("2C v68", "ae_MonkeyQueueVoice(r, h, 68, 3);\n    uint32_t f2;\n    uint32_t l2 = anim_span_parts(r, &f2);\n    ae_MonkeyQueueVoice(r, h, 69, (int32_t)(l2 - f2));", "ae_MonkeyQueueVoice(r, h, 67, 3);\n    uint32_t f2;\n    uint32_t l2 = anim_span_parts(r, &f2);\n    ae_MonkeyQueueVoice(r, h, 69, (int32_t)(l2 - f2));"),
    ("2C dust", "ae_MonkeyFootDust(r, h);\n    if (ae_ram_failed(r)) return 0;\n    ae_MonkeyQueueVoice(r, h, 68, 3);", "ae_MonkeyQueueVoice(r, h, 68, 3);"),
    ("Swap order", 'WR16(monkey + AE_MK_NAV_NODE0, next_node);\n    WR16(monkey + AE_MK_NAV_NODE1, current_node);', 'WR16(monkey + AE_MK_NAV_NODE0, current_node);\n    WR16(monkey + AE_MK_NAV_NODE1, next_node);'),
    ("Swap ret", 'return RDS32(cur_monkey(r) + AE_MK_DIST_TO_GOAL);', 'return RDS32(cur_monkey(r) + AE_MK_DIST_XZ_TO_GOAL);'),
    ("Fall mul", 'int32_t scaled_frame = mul_lo(fall_frame, 6);', 'int32_t scaled_frame = mul_lo(fall_frame, 5);'),
    ("Fall off", 'int32_t start_y = RDS16(monkey + AE_MK_HOP_START_Y);', 'int32_t start_y = RDS16(monkey + AE_MK_GOAL_NODE);'),
    ("Fall 50", 'AE_MK_HOOK(r, h, GetGroundHeight, x, y + 50u, z, 0);', 'AE_MK_HOOK(r, h, GetGroundHeight, x, y + 51u, z, 0);'),
    ("Fall cmp", "if (ground < cy) {", "if (ground <= cy) {"),
    ("Fall snap", 'WR32(cur_monkey(r) + AE_MK_POS + 4, (uint32_t)ground);', 'WR32(cur_monkey(r) + AE_MK_POS + 4, (uint32_t)ground + 1u);'),
    ("Fall count", 'WR16(m + AE_MK_THROW_TIMER, RD16(m + AE_MK_THROW_TIMER) + 1u);', 'WR16(m + AE_MK_THROW_TIMER, RD16(m + AE_MK_THROW_TIMER) + 2u);'),
    ("Arrive act", 'if (RD8(monkey + AE_MK_ACTIVITY) != 4) return 0;', 'if (RD8(monkey + AE_MK_ACTIVITY) != 3) return 0;'),
    ("Arrive b5", 'if (nf & 0x20u) flags = RD32(monkey + AE_MK_FLAGS) | 0x04000000u;', 'if (nf & 0x40u) flags = RD32(monkey + AE_MK_FLAGS) | 0x04000000u;'),
    ("Arrive f26", 'else flags = RD32(monkey + AE_MK_FLAGS) & 0xFBFFFFFFu;', 'else flags = RD32(monkey + AE_MK_FLAGS) & 0xF7FFFFFFu;'),
    ("Arrive f22", 'flags = RD32(reloaded_monkey + AE_MK_FLAGS) & 0xFFBFFFFFu;', 'flags = RD32(reloaded_monkey + AE_MK_FLAGS) & 0xFF7FFFFFu;'),
    ("Arrive alerted1", 'if (node_flags == 1) {\n        nf =', 'if (node_flags == 2) {\n        nf ='),
    ("Arrive 1000", "if (nf & 0x1000u) {\n            uint32_t f3", "if (nf & 0x2000u) {\n            uint32_t f3"),
    ("Arrive 30", 'WR16(reloaded_monkey + AE_MK_ANIM_LENGTH, 30);', 'WR16(reloaded_monkey + AE_MK_ANIM_LENGTH, 31);'),
    ("Arrive f3", 'WR32(reloaded_monkey + AE_MK_FLAGS3, f3 | 1u);', 'WR32(reloaded_monkey + AE_MK_FLAGS3, f3 | 2u);'),
    ("Arrive v39", "ae_MonkeyQueueVoice(r, h, 39, 0);", "ae_MonkeyQueueVoice(r, h, 38, 0);"),
    ("Arrive 4000", "if (RD32(nd) & 0x4000u) {\n        AE_MK_HOOK(r, h, ObjSlotSetField_CF", "if (RD32(nd) & 0x2000u) {\n        AE_MK_HOOK(r, h, ObjSlotSetField_CF"),
    ("Arrive cf1", 'ObjSlotSetField_CF, RDS16(nd + AE_NAV_LINK_CODE), 1, 0, 0);', 'ObjSlotSetField_CF, RDS16(nd + AE_NAV_LINK_CODE), 2, 0, 0);'),
    ("Arrive ev0", "if (RD32(nd) & 0x800u) {\n        AE_MK_HOOK(r, h, GimmickEvent, 0,", "if (RD32(nd) & 0x1000u) {\n        AE_MK_HOOK(r, h, GimmickEvent, 0,"),
    ("Arrive ev1", "if (RD32(nd) & 0x20000u) {\n        AE_MK_HOOK(r, h, GimmickEvent, 1,", "if (RD32(nd) & 0x10000u) {\n        AE_MK_HOOK(r, h, GimmickEvent, 1,"),
    ("Arrive ev2", "if (RD32(nd) & 0x40000u) {\n        AE_MK_HOOK(r, h, GimmickEvent, 2,", "if (RD32(nd) & 0x80000u) {\n        AE_MK_HOOK(r, h, GimmickEvent, 2,"),
    ("Arrive evnum", 'AE_MK_HOOK(r, h, GimmickEvent, 2, RDS16(nd + AE_NAV_LINK_CODE), 0, 0);', 'AE_MK_HOOK(r, h, GimmickEvent, 3, RDS16(nd + AE_NAV_LINK_CODE), 0, 0);'),
    ("Arrive 400", "if (RD32(nd) & 0x400u) {\n        ae_NavNodeGetLinkIds", "if (RD32(nd) & 0x800u) {\n        ae_NavNodeGetLinkIds"),
    ("Arrive st29", "ae_MonkeySetState(r, 0x29);", "ae_MonkeySetState(r, 0x2A);"),
    ("Arrive 80000", "if (RD32(nd) & 0x80000u) {\n            ae_MonkeyEnterState_32", "if (RD32(nd) & 0x40000u) {\n            ae_MonkeyEnterState_32"),
    ("Arrive alert0", 'if (RD8(cur_monkey(r) + AE_MK_ALERTED) == 0) {\n        nd =', 'if (RD8(cur_monkey(r) + AE_MK_ALERTED) == 1) {\n        nd ='),
    ("Arrive b2", '    if (node_flags & 4u) {\n        uint32_t t', '    if (node_flags & 2u) {\n        uint32_t t'),
    ("Arrive neg", "if (f < 0) t = 0;", "if (f < -1) t = 0;"),
    ("Arrive 22C", 'WR16(waiting_monkey + AE_MK_NODE_WAIT, t);', 'WR16(waiting_monkey + AE_MK_LINK_SLOPE, t);'),
    ("Arrive st42", 'ae_MonkeySetState(r, 0x42);\n        state_changed = 1;\n        WR8(cur_monkey(r) + AE_MK_ALERTED, keep);', 'ae_MonkeySetState(r, 0x41);\n        state_changed = 1;\n        WR8(cur_monkey(r) + AE_MK_ALERTED, keep);'),
    ("Arrive keep", 'uint32_t keep = RD8(waiting_monkey + AE_MK_ALERTED);', 'uint32_t keep = RD8(waiting_monkey + AE_MK_ACTIVITY);'),
    ("Arrive 400000a", 'WR32(waiting_monkey + AE_MK_FLAGS, fl | 0x400000u);  /* Store before the following call reads the monkey. */', 'WR32(waiting_monkey + AE_MK_FLAGS, fl | 0x200000u);  /* Store before the following call reads the monkey. */'),
    ("Arrive b8", '        if (node_flags & 8u) {\n            if (ae_AreaIsBossArena(r)) {', '        if (node_flags & 0x10u) {\n            if (ae_AreaIsBossArena(r)) {'),
    ("Arrive bossst", 'next_state = (previous_contact_kind != bt) ? 0x16u : 0x26u;', 'next_state = (previous_contact_kind != bt) ? 0x26u : 0x16u;'),
    ("Arrive contact", 'uint32_t previous_contact_kind = RD16(c + AE_MK_KIND);', 'uint32_t previous_contact_kind = RD16(c + AE_MK_TIMER_124);'),
    ("Arrive look", "if (f < dist) {\n                        ae_MonkeyEnterRandomLook", "if (f <= dist) {\n                        ae_MonkeyEnterRandomLook"),
    ("Arrive lookflag", 'WR8(c2 + AE_MK_ALERTED, 0);\n                        WR32(c2 + AE_MK_FLAGS, fl | 0x400000u);', 'WR8(c2 + AE_MK_ALERTED, 1);\n                        WR32(c2 + AE_MK_FLAGS, fl | 0x400000u);'),
    ("Arrive 8000", "if (RD32(nd) & 0x8000u) {\n                uint32_t c = cur_monkey(r);", "if (RD32(nd) & 0x4000u) {\n                uint32_t c = cur_monkey(r);"),
    ("Arrive 1500", "if (RDS32(c + AE_MK_DIST_TO_TARGET) < 1500) {", "if (RDS32(c + AE_MK_DIST_TO_TARGET) < 1501) {"),
    ("Arrive 10000", "if (RD32(nd) & 0x10000u) {\n                    uint32_t c = cur_monkey(r);", "if (RD32(nd) & 0x20000u) {\n                    uint32_t c = cur_monkey(r);"),
    ("Arrive idx2", "if (idx != 2) do_state = 1;", "if (idx != 1) do_state = 1;"),
    ("Arrive slotnode", "uint32_t n1 = RD16(c + AE_MK_NAV_NODE1);\n                    int32_t fld", "uint32_t n1 = RD16(c + AE_MK_NAV_NODE0);\n                    int32_t fld"),
    ("Arrive st0", 'next_state = 0;\n                    if (idx != 2)', 'next_state = 1;\n                    if (idx != 2)'),
    ("Arrive 100000", 'WR32(c + AE_MK_FLAGS, RD32(c + AE_MK_FLAGS) | 0x100000u);\n    }\n    return state_changed;', 'WR32(c + AE_MK_FLAGS, RD32(c + AE_MK_FLAGS) | 0x200000u);\n    }\n    return state_changed;'),
    ("Arrive clr79", 'ae_MonkeySetState(r, next_state);\n                state_changed = 1;\n                WR8(cur_monkey(r) + AE_MK_ALERTED, 0);', 'ae_MonkeySetState(r, next_state);\n                state_changed = 1;'),
    ("Pick boss", "uint32_t dividend = rnd & 0xFFFFu;", "uint32_t dividend = rnd & 0x7Fu;"),
    ("Pick bossnode", 'return ae_NavNodeGetLink(r, s16(current_node), (int32_t)rem);', 'return ae_NavNodeGetLink(r, s16(current_node), (int32_t)rem + 1);'),
    ("Pick hes", "if (hes != 0) {\n        angle = 0;", "if (hes == 0) {\n        angle = 0;"),
    ("Pick 56", "angle = 56;", "angle = 57;"),
    ("Pick f26", "if (((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 26) & 1u) == 0) angle", "if (((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 25) & 1u) == 0) angle"),
    ("Pick dbl", 'if (ae_MonkeyIsState0C_or_24(r)) cosine_limit = (int32_t)((uint32_t)cosine_limit << 1);', 'if (!ae_MonkeyIsState0C_or_24(r)) cosine_limit = (int32_t)((uint32_t)cosine_limit << 1);'),
    ("Pick init", 'int32_t best_visit_count = -10000;\n    int32_t best_link_index = -1;', 'int32_t best_visit_count = 0;\n    int32_t best_link_index = -1;'),
    ("Pick hist", 'if (candidate_node != hist && RD8(monkey + (uint32_t)link_index + AE_MK_LINK_BLOCKED) == 0) {', 'if (candidate_node != hist && RD8(monkey + (uint32_t)link_index + 0x1DB) == 0) {'),
    ("Pick cmp1", 'if (s16((uint32_t)best_visit_count) < (int32_t)vis) {', 'if (s16((uint32_t)best_visit_count) <= (int32_t)vis) {'),
    ("Pick s6", 'best_link_index = link_index;\n                        best_node = linked_node;', 'best_link_index = link_index + 1;\n                        best_node = linked_node;'),
    ("Pick s7", 'best_node = linked_node;', 'best_node = linked_node + 1;'),
    ("Pick slot", 'uint32_t cell = (uint32_t)candidate_node * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + RD8(monkey + AE_MK_SLOT);', 'uint32_t cell = (uint32_t)candidate_node * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + RD8(monkey + AE_MK_SLOT) + 1u;'),
    ("Pick flag27", 'if ((fl >> 27) & 1u) WR32(monkey + AE_MK_FLAGS, fl & 0xF7FFFFFFu);', 'if ((fl >> 27) & 1u) WR32(monkey + AE_MK_FLAGS, fl & 0xFBFFFFFFu);'),
    ("Pick s3lt", 'if (!((int32_t)current_visits < best_visits_signed) && (limit & 0xFFFF) == 0) return -1;', 'if (!((int32_t)current_visits <= best_visits_signed) && (limit & 0xFFFF) == 0) return -1;'),
    ("Pick limit", "(limit & 0xFFFF) == 0) return -1;", "(limit & 0xFF) == 0) return -1;"),
    ("Pick alerted", 'if (RD8(m + AE_MK_ALERTED) == 0) return s16((uint32_t)best_node);', 'if (RD8(m + AE_MK_ALERTED) != 0) return s16((uint32_t)best_node);'),
    ("Pick state3D", 'if (RD8(current_monkey + AE_MK_AI_STATE) == 0x3D) return s16((uint32_t)best_node);', 'if (RD8(current_monkey + AE_MK_AI_STATE) == 0x3C) return s16((uint32_t)best_node);'),
    ("Pick cosine", 'if (RDS16(current_monkey + (uint32_t)(b * 2) + AE_MK_LINK_SCORES) < cosine_limit) return s16((uint32_t)best_node);', 'if (RDS16(current_monkey + (uint32_t)(b * 2) + AE_MK_LINK_SCORES) <= cosine_limit) return s16((uint32_t)best_node);'),
    ("Pick cos2", 'else if (RDS16(monkey + (uint32_t)(link_index * 2) + AE_MK_LINK_SCORES) < cosine_limit) take = 1;', 'else if (RDS16(monkey + (uint32_t)(link_index * 2) + AE_MK_LINK_SCORES) <= cosine_limit) take = 1;'),
    ("Pick cand", 'if (current_visits_signed < (int32_t)vis) cand = 1;', 'if (current_visits_signed <= (int32_t)vis) cand = 1;'),
    ("Pick alert2", 'if (RD8(monkey + AE_MK_ALERTED) == 0) take = 1;', 'if (RD8(monkey + AE_MK_ALERTED) == 1) take = 1;'),
    ("Pick bossret", 'if (ae_AreaIsBossArena(r) != 0) return s16((uint32_t)best_node);', 'if (ae_AreaIsBossArena(r) == 0) return s16((uint32_t)best_node);'),
    ("Visuals ptr", 'RD32(AE_G_MONKEY_PTRS + (uint32_t)s16((uint32_t)active_index) * 4u)', 'RD32(AE_G_MONKEY_PTRS + (uint32_t)s16((uint32_t)active_index) * 8u)'),
    ("Visuals mask", 'if ((flags & 0x40030000u) == 0x40020000u) {', 'if ((flags & 0x40030000u) == 0x40010000u) {'),
    ("Visuals f0", 'if (phase_value > 0) { skip = 1; test2 = 0; }', 'if (phase_value >= 0) { skip = 1; test2 = 0; }'),
    ("Visuals lim", 'else if (!(phase_value < (int32_t)RD16(monkey + AE_MK_ANIM_LENGTH))) { skip = 1; test2 = 0; }', 'else if (!(phase_value <= (int32_t)RD16(monkey + AE_MK_ANIM_LENGTH))) { skip = 1; test2 = 0; }'),
    ("Visuals sign", 'if ((int32_t)flags < 0) skip = 1;', 'if ((int32_t)flags <= 0) skip = 1;'),
    ("Visuals f2", 'else if (RD32(monkey + AE_MK_FLAGS2) & 0x4000u) skip = 1;', 'else if (RD32(monkey + AE_MK_FLAGS2) & 0x2000u) skip = 1;'),
    ("Visuals act2", 'else if (RD8(monkey + AE_MK_ACTIVITY) == 2) skip = 1;', 'else if (RD8(monkey + AE_MK_ACTIVITY) == 3) skip = 1;'),
    ("Visuals area", 'if (RD16(AE_G_CURRENT_AREA) == 0x24) {\n                uint32_t c = cur_monkey(r);\n                uint32_t sound_handle', 'if (RD16(AE_G_CURRENT_AREA) == 0x25) {\n                uint32_t c = cur_monkey(r);\n                uint32_t sound_handle'),
    ("Visuals snd", "SoundLoopStopAndFree, 0x602D, sound_handle, 0, 0);", "SoundLoopStopAndFree, 0x602E, sound_handle, 0, 0);"),
    ("Visuals snd handle", "SoundLoopStopAndFree, 0x602D, sound_handle, 0, 0);", "SoundLoopStopAndFree, 0x602D, 0, 0, 0);"),
    ("Visuals dbg", "if (RD8(AE_G_DEBUG_FLAG_EB58) == 2) {", "if (RD8(AE_G_DEBUG_FLAG_EB58) == 1) {"),
    ("Visuals fade", 'ColorFadeSet, RDS16(c + AE_MK_SCREEN_X), RDS16(c + AE_MK_SCREEN_Y), 0, 0);', 'ColorFadeSet, RDS16(c + AE_MK_SCREEN_Y), RDS16(c + AE_MK_SCREEN_X), 0, 0);'),
    ("Visuals text", 'RD8(cur_monkey(r) + AE_MK_ID), 0, 0);\n            }\n            uint32_t c = cur_monkey(r);\n            uint32_t staggered_frame', 'RD8(cur_monkey(r) + AE_MK_SLOT), 0, 0);\n            }\n            uint32_t c = cur_monkey(r);\n            uint32_t staggered_frame'),
    ("Visuals lod3", 'if (staggered_frame == q * 2u + q) {', 'if (staggered_frame == q * 2u + q + 1u) {'),
    ("Visuals slot", 'uint32_t staggered_frame = RD32(AE_G_FRAME_COUNTER) + RD8(c + AE_MK_SLOT);', 'uint32_t staggered_frame = RD32(AE_G_FRAME_COUNTER) + RD8(c + AE_MK_ID);'),
    ("Visuals st32", "if (st == 30 || st == 32) {", "if (st == 30 || st == 31) {"),
    ("Visuals A", 'WR16(animation_angles + AE_MK_ANIM_ANGLE_SECOND + 2, ((uint32_t)phase_value + ((uint32_t)phase_value << 2)) << 4);', 'WR16(animation_angles + AE_MK_ANIM_ANGLE_SECOND + 2, ((uint32_t)phase_value + ((uint32_t)phase_value << 2)) << 3);'),
    ("Visuals 12", 't = (int32_t)((uint32_t)(t + (int32_t)fc) << 4);\n                WR16(animation_angles + AE_MK_ANIM_ANGLE_THIRD + 2, t);', 't = (int32_t)((uint32_t)(t + (int32_t)fc) << 3);\n                WR16(animation_angles + AE_MK_ANIM_ANGLE_THIRD + 2, t);'),
    ("Visuals zero", 'WR16(animation_angles + AE_MK_ANIM_ANGLE_THIRD, 0);\n                WR16(animation_angles + AE_MK_ANIM_ANGLE_THIRD + 4, 0);', 'WR16(animation_angles + AE_MK_ANIM_ANGLE_THIRD, 0);'),
    ("Visuals decode", "AE_MK_HOOK(r, h, ActorAnimDecode, m, 0, 0, 0);", "AE_MK_HOOK(r, h, ActorAnimDecode, m + 4, 0, 0, 0);"),
    ("Visuals shadow", "if (size != 0) {", "if (size != 1) {"),
    ("Visuals scratch", 'uint32_t shadow_context = RD32(AE_G_SHADOW_CONTEXT);', 'uint32_t shadow_context = RD32(AE_SCRATCH_BASE + 0x3F8);'),
    ("Visuals loop", '} while (s16((uint32_t)active_index) < 4);', '} while (s16((uint32_t)active_index) < 3);'),
    ("Visuals draw", 'ae_MonkeyDrawModel(r, h);\n            if (ae_ram_failed(r)) return;\n        }\n        active_index += 1;', '}\n        active_index += 1;'),
]

# the functions a mutation can influence (by the first word of its label); the other families are skipped to save time
MUT_FUNCS = {
    "NavNode": ["MonkeySetNavNode"], "1D": ["MonkeyEnterState_1D"], "04": ["MonkeyEnterState_04"], "2C": ["MonkeyEnterState_2C"],
    "Swap": ["MonkeySwapNavNodes"], "Fall": ["MonkeyAi_Fall"], "Arrive": ["MonkeyArriveAtNode"], "Pick": ["MonkeyPickBestLink"],
    "Visuals": ["MonkeyUpdateVisuals"],
}


def run_mutations(scale, seed, only_labels=()):
    rel = "src/monkeys/ae_monkey_level4.c"
    text = (ROOT / rel).read_text(encoding="utf-8")
    survivors = []
    tmp = Path(tempfile.mkdtemp(prefix="mk4_mut_"))
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
            h.reload_c({rel: mp}, tag=f"mk4mut{k}")
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
    ap.add_argument("--cases", type=float, default=25, help="scale of the case counts in percent")
    ap.add_argument("--seed", type=int, default=0x944241)
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
