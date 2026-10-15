"""Differential check of monkey port batch 3b (src/monkeys/ae_monkey_level3.c, call-graph level 3) against the original MIPS
code: for every ported function many scenarios are built in a byte-exact RAM image (SCUS_944.23 at its real addresses; monkeys /
nav nodes / extra link records / MonkeyInfo / animation tables / models / globals laid out as MonkeySpawnAll and the level loader
leave them, plus randomised and edge-case states); the original instructions (GTE through the PCSX-ReARMed oracle) and the C
port run on identical images with identical hook behaviour; return value, RAM outside the stack window, scratchpad and the
sequence of hook calls must be identical.  The batch 1 / 2 / 3a functions the code calls run as the C ports on the C side and as
the original instructions on the MIPS side (so this also checks them in context).  Needs extract/disc/SCUS_944.23 (skips if
missing).  See tools/mips/monkey_harness.py, monkey_scen.py and check_monkey_level2.py.

Usage: uv run python tools/mips/check_monkey_level3.py [--cases PERCENT] [--seed S] [--coverage] [--trapv] [--mutate]
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
from monkey_harness import W, build_monkey_world, MONKEYS, MONKEY_SIZE, G_CUR, NAV_BASE, INFO_BASE  # noqa: E402
from monkey_scen import (combat, navpos_world, navpos_extras, body_world, link_pair, extra_records, anim_globals, attack_world,  # noqa: E402
                         allowed_world, static_models, ANIM_BASE, FRAME, STATE_TABLE, VEC_OUT, M32, s32, s16, u16, G_TARGET, EXTRA_BASE,
                         FLAG_BITS)

# name -> (original address, size, return kind: s = int32 / u = uint32 / v = void, takes hooks, argument kinds[, C name])
FUNCS = {
    "MonkeyRestoreActivities": (0x800332F8, 464, "v", True, ""),
    "MonkeyUpdateGoal": (0x800355DC, 40, "v", True, ""),
    "MonkeyStartNotice": (0x800357E0, 60, "v", True, ""),
    "MonkeyEnterState_1E": (0x8003581C, 252, "s", True, ""),
    "MonkeyEnterState_34": (0x800359B4, 60, "v", False, ""),
    "MonkeyEnterKnockdown": (0x80035BD0, 328, "v", True, "s"),
    "MonkeyEnterState2F": (0x80035D18, 192, "v", True, ""),
    "MonkeyEnterState_30": (0x80035DD8, 148, "v", True, ""),
    "MonkeyEnterState_36": (0x80035E6C, 56, "v", True, ""),
    "MonkeyEnterState_20": (0x80035EA4, 120, "v", True, ""),
    "MonkeyEnterState_24": (0x80035F1C, 56, "v", True, ""),
    "MonkeyEnterState_0C": (0x80035F54, 56, "v", True, ""),
    "MonkeyEnterState_35": (0x80035F8C, 48, "v", False, ""),
    "MonkeyEnterState_1A": (0x80035FBC, 96, "v", False, ""),
    "MonkeyEnterState_09": (0x8003601C, 188, "v", False, ""),
    "MonkeyEnterIdleWait": (0x80036300, 164, "v", False, ""),
    "MonkeyResumeIdle": (0x800363A4, 64, "v", False, ""),
    "MonkeyEnterRandomLook": (0x800363E4, 56, "v", False, ""),
    "MonkeyPickAttackA": (0x8003738C, 148, "s", True, ""),
    "MonkeyPickAttackB": (0x80037420, 152, "s", True, ""),
    "MonkeyPickAttackC": (0x800374B8, 152, "s", True, ""),
    "MonkeyMarkBlockedLinks": (0x80038054, 204, "v", True, "s"),
    "MonkeyUpdateTargetDistance": (0x80039B30, 96, "v", True, ""),
    "MonkeyDrawModel": (0x80039ED4, 1120, "v", True, ""),
    "MonkeyAi_State3E": (0x8003FC08, 160, "v", False, ""),
    "MonkeyNavVisitUpdate": (0x8004086C, 880, "v", True, ""),
}
# code reached through the functions above (for the coverage report)
EXTRA_COVERAGE = {
    "MonkeySetState (batch 3a)": (0x80033220, 216), "MonkeyUpdateLinkSlope (batch 3a)": (0x80035370, 260),
    "MonkeyUpdateGoalPos (batch 3a)": (0x80035474, 360), "MonkeyTryAttackKind (batch 3a)": (0x8003710C, 640),
    "MonkeyNavLinkAllowed (batch 3a)": (0x80037E8C, 456), "MonkeyDrawAlertIcon (batch 3a)": (0x80039C38, 536),
    "MonkeyEnterState_3F (batch 3a)": (0x80036174, 64),
    "MonkeyPickAnim (batch 2)": (0x800331B0, 112), "MonkeyFootDust (batch 2)": (0x80038F00, 216),
    "NavNodeGetPos (batch 2)": (0x80034EF8, 392), "MonkeyAlertPuff (batch 2)": (0x80039248, 84),
    "NavLinkLookupExtra (batch 2)": (0x8003B880, 320), "MonkeyAlertIconKind (batch 2)": (0x80039B90, 168),
    "MonkeyAngleToTarget (batch 2)": (0x8003455C, 120),
    "StaticModelDraw (batch 1)": (0x80039E50, 132), "MonkeyIsJumpArcState (batch 1)": (0x80033C34, 76),
    "MonkeyGetCallVoice (batch 1)": (0x800356C0, 0x90), "MonkeyFacingFromLinkCode (batch 1)": (0x80036BD0, 0xB4),
    "NavNodeGetLink (batch 1)": (0x8003386C, 0x120), "AreaIsBossArena (batch 1)": (0x800339EC, 80),
    "MonkeyIsState0C_or_24 (batch 1)": (0x80033C00, 52), "MonkeyIsIdleActivityState (batch 1)": (0x80033B10, 140),
    "MonkeyRand (batch 1)": (0x80032E30, 40), "MonkeyAttackChance (batch 1)": (0x80037068, 0x120),
    "rsin (SDK)": (0x8002FB50, 200), "rcos (SDK)": (0x8002FC20, 160),
}

HDR = sc.HDR
STATE_ENTRY = {"MonkeyEnterState_34": 0x34, "MonkeyEnterState2F": 0x2F, "MonkeyEnterState_30": 0x30, "MonkeyEnterState_36": 0x36,
               "MonkeyEnterState_20": 0x20, "MonkeyEnterState_24": 0x24, "MonkeyEnterState_0C": 0x0C, "MonkeyEnterState_35": 0x35,
               "MonkeyEnterState_1A": 0x1A, "MonkeyEnterState_09": 0x09, "MonkeyEnterIdleWait": 1, "MonkeyResumeIdle": 1,
               "MonkeyEnterRandomLook": 0x16, "MonkeyEnterKnockdown": 0x2F, "MonkeyEnterState_1E": 0x1E,
               "MonkeyStartNotice": 7}


def check_all(h, scale, seed):
    t0 = time.time()

    def n(count):
        return max(1, int(count * scale / 100))

    def rng_of(name, i):
        return random.Random(f"{seed}/{name}/{i}")

    def cur(m): return m.read32(G_CUR)

    def run(name, count, setup=None, argsf=lambda c: (), nh=False, i0=0, **kw):
        """setup(m, rng, i) -> context; one fresh scenario per case"""
        for i in range(i0, i0 + n(count)):
            rng = rng_of(name, i)
            h.run(name, f"#{i}", i, (lambda m, rng=rng, i=i: setup(m, rng, i)) if setup else (lambda m, rng=rng: combat(m, rng)),
                  (lambda c, rng=rng, i=i: argsf(c, rng, i)) if argsf.__code__.co_argcount == 3 else argsf,
                  null_hooks=(nh and i % 8 == 7), **kw)

    def plain_nodes(m, nn):
        w = W(m)
        for k in range(nn):
            a = NAV_BASE + 0x18 * k
            w.w32(a, m.read32(a) & ~0x401 & M32)

    # ------------------------------------------------------------------ the state entries
    def entry_world(m, rng, name):
        """combat world + the animation globals; the monkey is in the entered state with some probability (the early-outs)"""
        c = combat(m, rng)
        anim_globals(m, rng, c)
        w = W(m)
        own = STATE_ENTRY.get(name)
        w.w8(c + 0x78, own if (own is not None and rng.random() < 0.3) else rng.choice([rng.randint(0, 0x42), 0x0C, 0x24, 0x1A, 0x35, 0x1E, 0x20, 1, rng.getrandbits(8)]))
        w.w32(c + 0x248, rng.choice([rng.getrandbits(32), rng.getrandbits(32) | 0x200000, rng.getrandbits(32) & ~0x200000, 0, 0x200000]) & M32)
        w.w16(c + 0x242, rng.choice([0, 1, 0xFFFF, 0x7FFF, rng.getrandbits(16)]))
        w.w16(0x800F4472, rng.choice([0, 1, 3, 6, 8, 2, 4, 5, 7]))
        w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x46, 0x35, 0x24]))
        return c

    for name, cnt in (("MonkeyEnterState_34", 150), ("MonkeyEnterState2F", 250), ("MonkeyEnterState_30", 250), ("MonkeyEnterState_36", 200),
                      ("MonkeyEnterState_20", 200), ("MonkeyEnterState_24", 250), ("MonkeyEnterState_0C", 250),
                      ("MonkeyEnterState_35", 150), ("MonkeyEnterState_1A", 300), ("MonkeyResumeIdle", 150),
                      ("MonkeyEnterRandomLook", 200), ("MonkeyStartNotice", 250)):
        run(name, cnt, lambda m, rng, i, name=name: entry_world(m, rng, name), nh=True, allow_trap=True)

    def idle_wait_world(m, rng, i):
        c = entry_world(m, rng, "MonkeyEnterIdleWait")
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        W(m).w16(info + 10, rng.choice([0, 1, -1, 2, 5, 100, 7, 1000, -50, 0x7FFF, 0x8000, rng.getrandbits(16)]) & 0xFFFF)
        return c
    run("MonkeyEnterIdleWait", 400, idle_wait_world, allow_trap=True)

    def knock_world(m, rng, i):
        c = entry_world(m, rng, "MonkeyEnterKnockdown")
        w = W(m)
        w.w8(c + 0x78, rng.choice([9, 41, 43, 3, 0x2F, 0x30, 0x31, rng.randint(0, 0x42), 0x0C, 62]))
        return c
    run("MonkeyEnterKnockdown", 600, knock_world, nh=True, allow_trap=True,
        argsf=lambda c, rng, i: (rng.choice([0x2F, 0x30, 0x30, 0x2F, 0x31, 0x36, 1, 0, rng.randint(0, 0x45), 0x10030, 0xFFFF0030, rng.getrandbits(32)]) & M32,))

    # ------------------------------------------------------------------ nav-link dependent entries (extra records, node positions)
    def lookup_world(m, rng, own_state):
        c = allowed_world(m, rng)
        ctx_c = c["cur"]
        anim_globals(m, rng, ctx_c)
        w = W(m)
        a, b = c["pair"]
        nn = m.read16(0x800B4D18)
        # the lookup is (node1, node0): put the pair into the monkey's node registers (some of the time swapped / off by one)
        n0, n1 = rng.choice([(a, b), (b, a), (a, b), (rng.randint(0, 40), rng.randint(0, 40))])
        w.w16(ctx_c + 0xF2, n0)
        w.w16(ctx_c + 0xF4, n1)
        w.w8(ctx_c + 0x78, own_state if rng.random() < 0.4 else rng.choice([30, 32, 0x1A, 0x35, 0x1E, 1, rng.randint(0, 0x42)]))
        w.w8(ctx_c + 0x22E, rng.choice([0, 1, 2, 2, 3]))
        w.w16(0x800F4472, rng.choice([0, 1, 3, 6, 8, 2]))
        w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x46]))
        w.w32(ctx_c + 0x248, rng.getrandbits(32))
        for k in range(nn):                         # plain nodes + NavNodeGetPos for node0
            pass
        return ctx_c

    def state1e_world(m, rng, i):
        c = lookup_world(m, rng, 0x1E)
        return c
    run("MonkeyEnterState_1E", 600, state1e_world, nh=True, allow_trap=True)

    def state09_world(m, rng, i):
        c = lookup_world(m, rng, 9)
        W(m).w8(c + 0x78, rng.choice([0x1A, 0x35, 0x1A, 0x35, 9, rng.randint(0, 0x42)]))
        return c
    run("MonkeyEnterState_09", 600, state09_world, allow_trap=True)

    # ------------------------------------------------------------------ restore activities
    def restore_world(m, rng, i):
        c = navpos_world(m, rng)
        anim_globals(m, rng, c)
        w = W(m)
        nn = m.read16(0x800B4D18)
        for k in range(8):
            a = MONKEYS + MONKEY_SIZE * k
            w.w16(a + 0xF4, rng.randint(0, max(0, nn - 1)) if rng.random() < 0.9 else rng.getrandbits(16))
            w.w8(a + 0x78, rng.choice([0x2B, 1, rng.randint(0, 0x42)]))
            w.w8(0x800F44BC + k, rng.choice([0, 1, 2, 3, 4, 5, 5, 2, rng.getrandbits(8)]))
        for k in range(nn):
            a = NAV_BASE + 0x18 * k
            if rng.random() < 0.5:                # object nodes (bit 10): slot * 100 + material in the field at +0x10
                w.w32(a, m.read32(a) | 0x400)
                w.w16(a + 0x10, rng.choice([rng.randint(0, 15) * 100 + rng.randint(0, 99), rng.getrandbits(16), -1, -150, 32767]) & 0xFFFF)
        w.w32(0x800B4D14, HDR)
        return c
    run("MonkeyRestoreActivities", 700, restore_world, nh=True, allow_trap=True)

    # ------------------------------------------------------------------ goal / target distance
    def goal_world(m, rng, i):
        c = navpos_world(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        w.w16(c + 0xF2, rng.randint(0, max(0, nn - 1)))
        w.w16(c + 0xF4, rng.randint(0, max(0, nn - 1)))
        w.w8(c + 0x78, rng.choice([30, 9, 1, 13, 16, 3, rng.randint(0, 0x42)]))
        w.w8(c + 0x7A, rng.choice([0, 1, 2, 3, 4, 5]))
        w.w8(c + 0x24B, rng.getrandbits(8))
        w.w32(c + 0x228, rng.choice([0, 2, 3, rng.getrandbits(32)]))
        w.w8(c + 0x22E, rng.choice([0, 1, 2]))
        if rng.random() < 0.3:
            plain_nodes(m, nn)
        for k in range(nn):
            a = NAV_BASE + 0x18 * k
            if rng.random() < 0.3:
                w.w32(a, m.read32(a) | 0x40)
        return c
    run("MonkeyUpdateGoal", 500, goal_world, nh=True, allow_trap=True)

    def target_dist_world(m, rng, i):
        c = goal_world(m, rng, i)
        return c
    for i in range(0x46 + n(500)):
        rng = rng_of("MonkeyUpdateTargetDistance", i)
        st = i if i < 0x46 else rng.choice([1, 13, 16, 19, 58, 14, 17, 20, 59, 15, 18, 21, 60, rng.randint(0, 0x42)])

        def setup(m, rng=rng, i=i, st=st):
            c = target_dist_world(m, rng, i)
            W(m).w8(c + 0x78, st)
            return c
        h.run("MonkeyUpdateTargetDistance", f"state {st} #{i}", i, setup, lambda c: (), null_hooks=(i % 8 == 7), allow_trap=True)

    # ------------------------------------------------------------------ attack pickers
    def pick_world(m, rng, i):
        c = attack_world(m, rng)
        W(m).w16(0x800EC2CA, rng.choice([0, 0, 0, 0, 1, -1, 0x100, rng.getrandbits(16)]))
        return c
    for nm in ("MonkeyPickAttackA", "MonkeyPickAttackB", "MonkeyPickAttackC"):
        run(nm, 1200, pick_world, nh=True, allow_trap=True)

    # ------------------------------------------------------------------ blocked links / visit update
    def links_world(m, rng, extra_all=True):
        """all flagged links of the graph get extra records with the flag combinations of MonkeyNavLinkAllowed"""
        c = navpos_world(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        pairs = set()
        for k in range(nn):
            a = NAV_BASE + 0x18 * k
            cnt, lp = m.read8(a + 0xA), m.read32(a + 0xC)
            for j in range(cnt):
                e = m.read16(lp + 2 * j)
                if e & 0x4000:
                    t = e & 0x3FFF
                    pairs.add((min(k, t), max(k, t)))
        for k in range(rng.randint(0, 4)):
            pairs.add(tuple(sorted((rng.randint(0, 40), rng.randint(0, 40)))))
        pairs = sorted(pairs)
        flagsel = [0, 0, 0, 0, 0] + [sum(b for b in FLAG_BITS if rng.random() < 0.3) for _ in range(6)] + list(FLAG_BITS)
        data = bytearray()
        for (lo, hi) in pairs[:250]:
            if rng.random() < 0.6:
                continue
            val = rng.choice([rng.randint(0, 20), rng.randint(0, 3000), rng.randint(0, 15), rng.getrandbits(32)])
            data += struct.pack("<hhII", lo, hi, rng.choice(flagsel), val & M32)
        m.write_bytes(EXTRA_BASE, bytes(data) + bytes(16))
        w.w32(0x800B4D1C, EXTRA_BASE)
        w.w16(0x800B4D20, len(data) // 12)
        w.w8(c + 0x7C, rng.randint(0, 15))
        w.w8(c + 0x79, rng.choice([0, 1, 1, 2]))
        for k in range(nn):
            a = NAV_BASE + 0x18 * k
            if rng.random() < 0.2:
                w.w32(a, m.read32(a) | 0x100)
        w.w16(c + 0xE6, rng.randint(0, max(0, nn - 1)))
        w.w16(0x800F4472, rng.choice([0, 1, 3]))
        w.w16(0x800F4476, rng.choice([0x10, 0x29]))
        return c

    def blocked_world(m, rng, i):
        c = links_world(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        for k in range(nn):
            a = NAV_BASE + 0x18 * k
            w.w8(a + 0xB, rng.choice([0, 1, 2, 3, 4, 6, 8, rng.randint(0, 8), 20]))
        for j in range(50):
            w.w8(c + 0x1DA + j, rng.getrandbits(8))
        return c
    run("MonkeyMarkBlockedLinks", 900, blocked_world, nh=True, allow_trap=True,
        argsf=lambda c, rng, i: (rng.choice([rng.randint(0, 5), rng.randint(0, 39), rng.randint(-2, 42), rng.getrandbits(32)]) & M32,))

    def visit_world(m, rng, i):
        c = links_world(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        slot = m.read8(c + 0x7B)
        mode = rng.choice(["sparse", "dense", "zero", "full"])
        for k in range(nn):
            a = NAV_BASE + 0x18 * k + slot + 0x14
            w.w8(a, {"sparse": rng.choice([0, 0, 0, 0, 1, 2, 3, 255]), "dense": rng.choice([0, 1, 2, 3, 254, 255, rng.getrandbits(8)]),
                     "zero": 0, "full": rng.choice([1, 2, 3, 255])}[mode])
        w.w32(c + 0x248, rng.choice([rng.getrandbits(32) | 0x08000000, rng.getrandbits(32) & ~0x08000000, 0x08000000, 0]) & M32)
        w.w16(c + 0xEA, rng.choice([0, 1, 2, 3, 0xFFFF, 0xFFFE, rng.getrandbits(16)]))
        w.w16(c + 0xF2, rng.randint(0, max(0, nn - 1)))
        w.w16(c + 0xF4, rng.randint(0, max(0, nn - 1)))
        if rng.random() < 0.05:
            w.w16(0x800B4D18, rng.choice([0, -1, 1]))
        return c
    run("MonkeyNavVisitUpdate", 1500, visit_world, nh=True, allow_trap=True)

    # ------------------------------------------------------------------ small AI step
    def ai3e_world(m, rng, i):
        c = combat(m, rng)
        w = W(m)
        lim = rng.choice([0, 1, 5, 50, 0x7FFF, 0x8000, 0xFFFF, rng.getrandbits(16)])
        w.w16(c + 0x128, lim)
        w.w16(c + 0x96, rng.choice([lim - 1, lim - 2, lim, lim + 1, 0xFFFF, 0x7FFF, rng.getrandbits(16)]) & 0xFFFF)
        w.w32(c + 0x228, rng.choice([0, 2, 3, rng.getrandbits(32)]))
        for off in (0x70, 0x72, 0x74):
            w.w16(c + off, rng.choice([0, 1, -1, 0x7FFF, 0x8000, rng.getrandbits(16)]) & 0xFFFF)
        w.w8(c + 0x78, rng.choice([0x3E, 0x3F, 1]))
        return c
    run("MonkeyAi_State3E", 500, ai3e_world, allow_trap=True)

    # ------------------------------------------------------------------ drawing
    def draw_world(m, rng, i):
        c = combat(m, rng)
        w = W(m)
        static_models(m, rng)
        w.w8(c + 0x7A, rng.choice([3, 4, 4, 3, 0, 1, 2, 5]))
        w.w16(c + 0x90, rng.choice([0, 3, 6, 9, 1, 2, 4, 5, 0xFFFF, 0xFFFD, rng.getrandbits(16)]))
        w.w8(c + 0x78, rng.choice([0x0C, 0x24, 0x1E, 0x20, 0x1A, 1, rng.randint(0, 0x42)]))
        w.w16(c + 0xF0, rng.choice([0, -1, -5, -100, -4096, 1, 5, 0x7FFF, -0x8000, rng.getrandbits(16)]) & 0xFFFF)
        w.w32(c + 0x248, rng.getrandbits(32))
        w.w8(c + 0xD5, rng.choice([3, 0, 1, 2, 3, 4, 0xFF, 0x80, rng.getrandbits(8)]))
        w.w32(c + 0x160, rng.choice([0xFFFFFFFF, 0xFFFFFFFF, 0, 5, rng.getrandbits(32)]))
        w.w32(c + 0x14C, rng.choice([0, 448, 896, rng.getrandbits(32), rng.randint(0, 2000)]))
        w.w32(c + 0x14, rng.choice([0x801C9400, 0x801C9500, rng.getrandbits(32)]))
        w.w16(0x800F4476, rng.choice([0x46, 0x24, 0x24, 0x46, 0x10, 0x29, rng.getrandbits(16)]))
        w.w16(0x800F4472, rng.choice([0, 1, 3, 6, 8, 2]))
        w.w32(0x800EC35C, rng.choice([0, 1, 2, 3]))
        if m.read16(0x800F4476) == 0x24 and rng.random() < 0.6:          # the looping sound of area 0x24
            w.w8(c + 0x78, rng.choice([0x1A, 0x1A, 1, 2, 0x1B]))
            w.w32(c + 0x160, rng.choice([0xFFFFFFFF, 0xFFFFFFFF, 0, 5, rng.getrandbits(32)]))
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        w.w16(info, rng.choice([0, 64, 191, 192, 320, 1000, rng.randint(0, 4000), rng.getrandbits(16)]))
        for k in range(8):
            w.w16(info + 0x1E + 8 * k, rng.choice([0, 0, 1, 5, rng.getrandbits(16)]))
            w.w16(0x800AEBB6 + 8 * k, rng.choice([-1, -1, 0, 1, 5, 20, 60, rng.getrandbits(16)]) & 0xFFFF)
        for j in range(0, 80):
            w.w8(0x800AEBF8 + j, rng.choice([0, 1, 5, 20, 60, rng.getrandbits(8)]))
        for off in range(0, 0x20, 2):
            w.w16(c + 0x24A if False else 0x800E78C0 + off, rng.getrandbits(16))
        return c
    run("MonkeyDrawModel", 2500, draw_world, nh=True, allow_trap=True)

    print(f"scenario families done in {time.time() - t0:.1f}s")


MUTATIONS = [
    ("Restore four", 'WR8(slot_monkey + AE_MK_ACTIVITY, saved_activity == 4 ? 0u : saved_activity);', 'WR8(slot_monkey + AE_MK_ACTIVITY, saved_activity == 5 ? 0u : saved_activity);'),
    ("Restore act2", "if (act == 2) {", "if (act == 3) {"),
    ("Restore act5", "} else if (act == 5) {", "} else if (act == 4) {"),
    ("Restore div100", "sub32(sra32(mul_hi(v, 0x51EB851F), 5), sra32(v, 31))", "sub32(sra32(mul_hi(v, 0x51EB851F), 4), sra32(v, 31))"),
    ("Restore nodebit", 'if (RD32(node_addr) & 0x400u) {', 'if (RD32(node_addr) & 0x800u) {'),
    ("Restore state2B", "ae_MonkeySetState(r, 0x2B);", "ae_MonkeySetState(r, 0x2C);"),
    ("Restore field", 'div100_s16(RDS16(node_addr + AE_NAV_LINK_CODE)), 4, 0, 0);\n            } else if', 'div100_s16(RDS16(node_addr + AE_NAV_LINK_CODE)), 5, 0, 0);\n            } else if'),
    ("Restore node1", "uint32_t n = RD16(monkey + AE_MK_NAV_NODE1)", "uint32_t n = RD16(monkey + AE_MK_NAV_NODE0)"),
    ("Restore reset", "WR8(cur_monkey(r) + AE_MK_ACTIVITY, 0);\n                monkey", "WR8(cur_monkey(r) + AE_MK_ACTIVITY, 1);\n                monkey"),
    ("Restore target", 'div100_s16(RDS16(n + AE_NAV_LINK_CODE)), monkey + AE_MK_POS, 0, 0);', 'div100_s16(RDS16(n + AE_NAV_LINK_CODE)), monkey + AE_MK_POS + 4, 0, 0);'),
    ("Notice state", "ae_MonkeySetState(r, 7);", "ae_MonkeySetState(r, 8);"),
    ("Notice voice", "ae_MonkeyQueueVoice(r, h, voice, 0);", "ae_MonkeyQueueVoice(r, h, voice, 1);"),
    ("1E timer", 'WR16(monkey + AE_MK_HEALTH, 1000);', 'WR16(monkey + AE_MK_HEALTH, 999);'),
    ("1E flag", 'WR32(monkey + AE_MK_FLAGS, flags & 0xFEFFFFFFu);', 'WR32(monkey + AE_MK_FLAGS, flags & 0xFDFFFFFFu);'),
    ("1E boss4a", 'WR16(monkey + AE_MK_BODY_FLAGS, 0xC000u);', 'WR16(monkey + AE_MK_BODY_FLAGS, 0x8000u);'),
    ("1E z230", 'WR32(monkey + AE_MK_STATE_TIMER, 0);', 'WR32(monkey + AE_MK_STATE_TIMER, 1);'),
    ("1E sound", "SoundLoopStartEx, 1, 0x6050,", "SoundLoopStartEx, 1, 0x6051,"),
    ("1E state", 'if (previous_state != 30 && previous_state != 32) {', 'if (previous_state != 30 && previous_state != 31) {'),
    ("1E extra", "uint32_t extra = RD16(sp + 32);", "uint32_t extra = RD16(sp + 34);"),
    ("1E from", 'ae_NavLinkLookupExtra(r, next_node, current_node, sp + 32);', 'ae_NavLinkLookupExtra(r, current_node, next_node, sp + 32);'),
    ("1E pos", 'WR32(flight_monkey + AE_MK_CIRCLE_CENTER + 4, RD32(sp + 20));', 'WR32(flight_monkey + AE_MK_CIRCLE_CENTER + 4, RD32(sp + 24));'),
    ("34 inc", "WR16(monkey + AE_MK_ANIM_STATE_FRAME, RD16(monkey + AE_MK_ANIM_STATE_FRAME) + 1u);", "WR16(monkey + AE_MK_ANIM_STATE_FRAME, RD16(monkey + AE_MK_ANIM_STATE_FRAME) + 2u);"),
    ("34 state", "ae_MonkeySetState(r, 0x34);", "ae_MonkeySetState(r, 0x33);"),
    ("Knock 90", 'WR8(reloaded_monkey + AE_MK_STUN_TIMER, knockdown_state == 48 ? 90u : 30u);', 'WR8(reloaded_monkey + AE_MK_STUN_TIMER, knockdown_state == 48 ? 91u : 30u);'),
    ("Knock 30", 'WR8(reloaded_monkey + AE_MK_STUN_TIMER, knockdown_state == 48 ? 90u : 30u);', 'WR8(reloaded_monkey + AE_MK_STUN_TIMER, knockdown_state == 48 ? 90u : 31u);'),
    ("Knock timer", 'WR16(reloaded_monkey + AE_MK_TIMER_124, 120);', 'WR16(reloaded_monkey + AE_MK_TIMER_124, 121);'),
    ("Knock voice", 'ae_MonkeyQueueVoice(r, h, 71, 3);\n    reloaded_monkey = cur_monkey(r);\n    if (!((RD32(reloaded_monkey + AE_MK_FLAGS) >> 21) & 1u)) {\n        knock_sfx', 'ae_MonkeyQueueVoice(r, h, 70, 3);\n    reloaded_monkey = cur_monkey(r);\n    if (!((RD32(reloaded_monkey + AE_MK_FLAGS) >> 21) & 1u)) {\n        knock_sfx'),
    ("Knock bit", 'if (!((RD32(reloaded_monkey + AE_MK_FLAGS) >> 21) & 1u)) {\n        knock_sfx', 'if (!((RD32(reloaded_monkey + AE_MK_FLAGS) >> 20) & 1u)) {\n        knock_sfx'),
    ("Knock sfx", 'AE_MK_HOOK(r, h, PlaySfxAt, 78, sp + 16, 0, 0);\n    WR16(mk + AE_MK_TIMER_158, 50);', 'AE_MK_HOOK(r, h, PlaySfxAt, 79, sp + 16, 0, 0);\n    WR16(mk + AE_MK_TIMER_158, 50);'),
    ("Knock 158", 'WR16(mk + AE_MK_TIMER_158, 50);', 'WR16(mk + AE_MK_TIMER_158, 51);'),
    ("Knock flag21", "WR32(mk + AE_MK_FLAGS, RD32(mk + AE_MK_FLAGS) | 0x200000u);", "WR32(mk + AE_MK_FLAGS, RD32(mk + AE_MK_FLAGS) | 0x100000u);"),
    ("Knock puff", 'WR16(sp + 26, RD16(reloaded_monkey + AE_MK_POS + 4) - 128u);', 'WR16(sp + 26, RD16(reloaded_monkey + AE_MK_POS + 4) - 127u);'),
    ("Knock jump", "if (!jump) {", "if (jump) {"),
    ("Knock target", 'WR32(monkey + AE_MK_GOAL_POS + 4, RD32(target + AE_MK_POS + 4));', 'WR32(monkey + AE_MK_GOAL_POS + 4, RD32(target + AE_MK_POS + 8));'),
    ("Knock clear", 'WR32(reloaded_monkey + AE_MK_CONTACT, 0);\n    ae_MonkeyQueueVoice(r, h, 71, 3);\n    reloaded_monkey', 'WR32(reloaded_monkey + 0x54, 0);\n    ae_MonkeyQueueVoice(r, h, 71, 3);\n    reloaded_monkey'),
    ("2F e2", 'WR8(saved_monkey + AE_MK_STUN_TIMER, 30);', 'WR8(saved_monkey + AE_MK_STUN_TIMER, 31);'),
    ("2F y", 'WR16(saved_monkey + AE_MK_HOP_START_Y, y + 128u);', 'WR16(saved_monkey + AE_MK_HOP_START_Y, y + 129u);'),
    ("2F r92", 'WR16(saved_monkey + AE_MK_THROW_TIMER, 0);', 'WR16(saved_monkey + AE_MK_THROW_TIMER, 1);'),
    ("2F r124", 'WR16(saved_monkey + AE_MK_TIMER_124, 120);', 'WR16(saved_monkey + AE_MK_TIMER_124, 121);'),
    ("2F sfx", 'PlaySfxAt, 78, sp + 16, 0, 0);\n        WR16(saved_monkey + AE_MK_TIMER_158, 50);', 'PlaySfxAt, 77, sp + 16, 0, 0);\n        WR16(saved_monkey + AE_MK_TIMER_158, 50);'),
    ("2F state", "ae_MonkeySetState(r, 0x2F);", "ae_MonkeySetState(r, 0x2E);"),
    ("30 state", "ae_MonkeySetState(r, 0x30);", "ae_MonkeySetState(r, 0x31);"),
    ("30 flag", 'WR32(saved_monkey + AE_MK_FLAGS, RD32(saved_monkey + AE_MK_FLAGS) | 0x200000u);\n    }\n}\n\n/* MonkeyEnterState_36', 'WR32(saved_monkey + AE_MK_FLAGS, RD32(saved_monkey + AE_MK_FLAGS) | 0x400000u);\n    }\n}\n\n/* MonkeyEnterState_36'),
    ("36 clear", 'WR32(cur_monkey(r) + AE_MK_CONTACT, 0);\n    ae_MonkeyQueueVoice(r, h, 71, 3);\n}', 'WR32(cur_monkey(r) + 0x54, 0);\n    ae_MonkeyQueueVoice(r, h, 71, 3);\n}'),
    ("36 voice", 'ae_MonkeySetState(r, 0x36);\n    WR32(cur_monkey(r) + AE_MK_CONTACT, 0);\n    ae_MonkeyQueueVoice(r, h, 71, 3);', 'ae_MonkeySetState(r, 0x36);\n    WR32(cur_monkey(r) + AE_MK_CONTACT, 0);\n    ae_MonkeyQueueVoice(r, h, 71, 4);'),
    ("20 particle", "AE_MK_HOOK(r, h, SpawnParticle, 5, sp + 16, 0, 0);\n    AE_MK_HOOK", "AE_MK_HOOK(r, h, SpawnParticle, 6, sp + 16, 0, 0);\n    AE_MK_HOOK"),
    ("20 voice", "ae_MonkeyQueueVoice(r, h, 120, 2);", "ae_MonkeyQueueVoice(r, h, 120, 3);"),
    ("20 state", "ae_MonkeySetState(r, 0x20);", "ae_MonkeySetState(r, 0x21);"),
    ("24 state", "ae_MonkeySetState(r, 0x24);", "ae_MonkeySetState(r, 0x25);"),
    ("24 puff", "if (!ae_MonkeyIsState0C_or_24(r)) ae_MonkeyAlertPuff(r, h);\n    ae_MonkeySetState(r, 0x24);", "if (ae_MonkeyIsState0C_or_24(r)) ae_MonkeyAlertPuff(r, h);\n    ae_MonkeySetState(r, 0x24);"),
    ("0C state", "ae_MonkeySetState(r, 0x0C);", "ae_MonkeySetState(r, 0x0D);"),
    ("35 node", 'uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);\n    WR16(monkey + AE_MK_PREV_NODE, next_node);\n    ae_MonkeySetState(r, 0x35);', 'uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE0);\n    WR16(monkey + AE_MK_PREV_NODE, next_node);\n    ae_MonkeySetState(r, 0x35);'),
    ("1A boss", "if (ae_AreaIsBossArena(r)) {\n        uint32_t m = cur_monkey(r);\n        uint32_t x", "if (!ae_AreaIsBossArena(r)) {\n        uint32_t m = cur_monkey(r);\n        uint32_t x"),
    ("1A y", 'WR32(m + AE_MK_JUMP_START + 4, y);', 'WR32(m + AE_MK_JUMP_START + 4, y + 1u);'),
    ("1A state", "ae_MonkeySetState(r, 0x1A);", "ae_MonkeySetState(r, 0x1B);"),
    ("09 plus200", 'WR32(a + AE_MK_POS + 4, RD32(a + AE_MK_POS + 4) + 200u);', 'WR32(a + AE_MK_POS + 4, RD32(a + AE_MK_POS + 4) + 201u);'),
    ("09 r22E", 'if (RD8(a + AE_MK_LINK_SLOPE) == 2)', 'if (RD8(a + AE_MK_LINK_SLOPE) == 1)'),
    ("09 s53", "if (RD8(m + AE_MK_AI_STATE) == 53) cond = 1;", "if (RD8(m + AE_MK_AI_STATE) == 52) cond = 1;"),
    ("09 s26", "if (st == 26) {", "if (st == 25) {"),
    ("09 boss", "if (ae_AreaIsBossArena(r) == 0) cond = 1;", "if (ae_AreaIsBossArena(r) != 0) cond = 1;"),
    ("09 clear", 'WR16(monkey + AE_MK_ACTION_COOLDOWN, 0);', 'WR16(monkey + AE_MK_ACTION_COOLDOWN, 1);'),
    ("09 facing", "ae_MonkeyFacingFromLinkCode(r, RDS16(sp + 16));", "ae_MonkeyFacingFromLinkCode(r, RDS16(sp + 16) + 1);"),
    ("09 state9", "ae_MonkeySetState(r, 9);", "ae_MonkeySetState(r, 10);"),
    ("Idle state", "ae_MonkeySetState(r, 1);\n    uint32_t rnd", "ae_MonkeySetState(r, 2);\n    uint32_t rnd"),
    ("Idle mask", '& 0xFFF7FFFFu);\n    WR16(monkey + AE_MK_IDLE_TIMER', '& 0xFFEFFFFFu);\n    WR16(monkey + AE_MK_IDLE_TIMER'),
    ("Idle shl", 'int32_t random_duration = (int32_t)(rnd << 1);', 'int32_t random_duration = (int32_t)(rnd << 2);'),
    ("Idle off", 'RDS16(info_of(r, monkey) + AE_MK_INFO_IDLE_DURATION)', 'RDS16(info_of(r, monkey) + 12)'),
    ("Idle store", 'WR16(monkey + AE_MK_IDLE_TIMER, (uint32_t)(idle_duration << 1) - (uint32_t)rem);', 'WR16(monkey + AE_MK_IDLE_TIMER, (uint32_t)(idle_duration << 2) - (uint32_t)rem);'),
    ("Resume flag", "| 0x80000u);", "| 0x40000u);"),
    ("Resume clear", 'WR16(monkey + AE_MK_IDLE_TIMER, 0);', 'WR16(monkey + AE_MK_IDLE_TIMER, 1);'),
    ("Look order", "(v & 1u) ? 0x26u : 0x16u", "(v & 1u) ? 0x16u : 0x26u"),
    ("Pick B bit", "return pick_attack(r, h, 1); }", "return pick_attack(r, h, 2); }"),
    ("Pick A bit", "return pick_attack(r, h, 0); }", "return pick_attack(r, h, 1); }"),
    ("Pick C bit", "return pick_attack(r, h, 2); }", "return pick_attack(r, h, 3); }"),
    ("Pick tries", 'for (int32_t attempt = 0; attempt < 12; ++attempt) {', 'for (int32_t attempt = 0; attempt < 11; ++attempt) {'),
    ("Pick none", '    return 100;\n}\n/* Choose an attack allowed by category A. */\nint32_t ae_MonkeyPickAttackA', '    return 99;\n}\n/* Choose an attack allowed by category A. */\nint32_t ae_MonkeyPickAttackA'),
    ("Pick player", "if (RDS16(AE_G_PLAYER_BLOCK) != 0) return 100;", "if (RDS16(AE_G_PLAYER_BLOCK + 2) != 0) return 100;"),
    ("Pick mask", 'uint32_t kind = ae_MonkeyRand(r) & 7u;', 'uint32_t kind = ae_MonkeyRand(r) & 3u;'),
    ("Mark clear", 'for (int32_t link_index = 49; link_index >= 0; --link_index)', 'for (int32_t link_index = 48; link_index >= 0; --link_index)'),
    ("Mark base", 'WR8(monkey + (uint32_t)link_index + AE_MK_LINK_BLOCKED, 0);', 'WR8(monkey + (uint32_t)link_index + 0x1DB, 0);'),
    ("Mark cond", 'if (ok == 0) WR8(monkey + (uint32_t)link_index + AE_MK_LINK_BLOCKED, 1);', 'if (ok != 0) WR8(monkey + (uint32_t)link_index + AE_MK_LINK_BLOCKED, 1);'),
    ("Mark count", 'uint32_t link_count = RD8((uint32_t)node_index * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + AE_NAV_ACTIVE_LINK_COUNT);', 'uint32_t link_count = RD8((uint32_t)node_index * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + AE_NAV_LINK_COUNT);'),
    ("Mark args", 'ae_MonkeyNavLinkAllowed(r, h, node_index, link);', 'ae_MonkeyNavLinkAllowed(r, h, link, node_index);'),
    ("Target idle", "if (!ae_MonkeyIsIdleActivityState(r)) {", "if (ae_MonkeyIsIdleActivityState(r)) {"),
    ("Target store", 'WR32(reloaded_monkey + AE_MK_DIST_TO_TARGET, d);', 'WR32(reloaded_monkey + AE_MK_DIST_XZ_TO_GOAL, d);'),
    ("Target angle", 'WR32(reloaded_monkey + AE_MK_LINK_ANGLE, ang);', 'WR32(reloaded_monkey + AE_MK_REACH_RADIUS, ang);'),
    ("Target pos", 'int32_t d = ae_Distance3D(r, monkey + AE_MK_POS, target + AE_MK_POS);', 'int32_t d = ae_Distance3D(r, monkey + AE_MK_POS, target + AE_MK_POS + 4);'),
    ("3E limit", 'if (s16(elapsed) < (int32_t)duration) {', 'if (s16(elapsed) <= (int32_t)duration) {'),
    ("3E flags2", "if (RD32(monkey + AE_MK_FLAGS2) & 2u) {", "if (RD32(monkey + AE_MK_FLAGS2) & 4u) {"),
    ("3E z", 'WR32(monkey + AE_MK_POS + 8, z + (uint32_t)velocity_z);', 'WR32(monkey + AE_MK_POS + 8, z + (uint32_t)velocity_z + 1u);'),
    ("3E y", '(uint32_t)RDS16(monkey + AE_MK_VEL + 2));', '(uint32_t)RDS16(monkey + AE_MK_VEL));'),
    ("Visit bit", 'if ((flags >> 27) & 1u) {', 'if ((flags >> 26) & 1u) {'),
    ("Visit node1", 'uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);\n        uint32_t next_visit_cell = next_node', 'uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE0);\n        uint32_t next_visit_cell = next_node'),
    ("Visit parity", 'if (odd_age == 1u && force_propagation == 0) {', 'if (odd_age == 0u && force_propagation == 0) {'),
    ("Visit a2", 'if (odd_age == 1u && force_propagation == 0) {', 'if ((void)force_propagation, odd_age == 1u) {'),
    ("Visit cap", 'out = next_mark_bits < 256u ? next_mark_bits : 255u;', 'out = next_mark_bits < 256u ? next_mark_bits : 254u;'),
    ("Visit skip2", "if (!(vis < 2u) && (entry & 0x4000u)) {", "if (!(vis < 3u) && (entry & 0x4000u)) {"),
    ("Visit entry", "(entry & 0x4000u)) {", "(entry & 0x8000u)) {"),
    ("Visit mark", 'WR8(node_offset + n3 + RD8(m3 + AE_MK_SLOT) + AE_NAV_NODE_VISIT_MARKS, 1);', 'WR8(node_offset + n3 + RD8(m3 + AE_MK_SLOT) + AE_NAV_NODE_VISIT_MARKS, 2);'),
    ("Visit count", 'WR16(cm + AE_MK_NAV_MARK_COUNT, marks_added);', 'WR16(cm + AE_MK_NAV_MARK_COUNT, marks_added + 1);'),
    ("Visit flagset", "RD32(cm + AE_MK_FLAGS) | 0x08000000u);", "RD32(cm + AE_MK_FLAGS) | 0x04000000u);"),
    ("Visit args", 'ae_MonkeyNavLinkAllowed(r, h, node_index, (int32_t)linked_node)', 'ae_MonkeyNavLinkAllowed(r, h, (int32_t)linked_node, node_index)'),
    ("Visit goalAge", 'WR16(current_monkey + AE_MK_GOAL_AGE, RD16(current_monkey + AE_MK_GOAL_AGE) + 1u);', 'WR16(current_monkey + AE_MK_GOAL_AGE, RD16(current_monkey + AE_MK_GOAL_AGE) + 2u);'),
    ("Visit clear27", 'WR32(monkey + AE_MK_FLAGS, flags & 0xF7FFFFFFu);', 'WR32(monkey + AE_MK_FLAGS, flags & 0xFBFFFFFFu);'),
    ("Visit nodes0", 'if (RD8(node_index * AE_NAV_NODE_SIZE + nodes + slot + AE_NAV_NODE_VISIT_MARKS) != 0) return;', 'if (RD8(node_index * AE_NAV_NODE_SIZE + nodes + slot + 0x15) != 0) return;'),
    ("Visit clearflag", 'WR32(current_monkey + AE_MK_FLAGS, current_flags & 0xF7FFFFFFu);\n    if (odd_age == 1u', 'WR32(current_monkey + AE_MK_FLAGS, current_flags & 0xFBFFFFFFu);\n    if (odd_age == 1u'),
    ("Visit slot", 'uint32_t slot = RD8(m + AE_MK_SLOT);\n            uint32_t node = node_offset + nodes;', 'uint32_t slot = RD8(m + AE_MK_SLOT) + 1u;\n            uint32_t node = node_offset + nodes;'),
    ("Draw act3", "if (act != 3) return;", "if (act != 2) return;"),
    ("Draw mod3", 'uint32_t timer_remainder = (timer - (q * 2u + q)) & 0xFFFFu;', 'uint32_t timer_remainder = (timer - (q * 2u + q + 1u)) & 0xFFFFu;'),
    ("Draw 0C", 'if (ae_MonkeyIsState0C_or_24(r)) {\n        draw_monkey = cur_monkey(r);\n        AE_MK_HOOK(r, h, MatrixLookDir', 'if (!ae_MonkeyIsState0C_or_24(r)) {\n        draw_monkey = cur_monkey(r);\n        AE_MK_HOOK(r, h, MatrixLookDir'),
    ("Draw shift cos", "ae_sdk_rcos(r, (int32_t)((uint32_t)f0 << 10))", "ae_sdk_rcos(r, (int32_t)((uint32_t)f0 << 9))"),
    ("Draw shift sin", "ae_sdk_rsin(r, (int32_t)((uint32_t)f0 << 10))", "ae_sdk_rsin(r, (int32_t)((uint32_t)f0 << 11))"),
    ("Draw f0", "if (!(f0 > 0)) {", "if (!(f0 > 1)) {"),
    ("Draw flagbits", 'uint32_t t = RD16(monkey + AE_MK_FLAGS_HI) & 3u;', 'uint32_t t = RD16(monkey + AE_MK_FLAGS_HI) & 7u;'),
    ("Draw scale", 'if (((RD16(scale_monkey + AE_MK_FLAGS_HI) & 3u) - 1u) < 2u) {', 'if (((RD16(scale_monkey + AE_MK_FLAGS_HI) & 3u) - 1u) < 3u) {'),
    ("Draw bossctx", 'uint32_t ctx = idx * 16504u + (AE_G_MODEL_DRAW_BASE + AE_MK_DRAW_CONTEXTS);\n        AE_MK_HOOK(r, h, ModelDraw', 'uint32_t ctx = idx * 16504u + (AE_G_MODEL_DRAW_BASE + 0x88u);\n        AE_MK_HOOK(r, h, ModelDraw'),
    ("Draw lod3", 'if (RD8(draw_monkey_after_icon + AE_MK_MODEL_LOD) == 3) {', 'if (RD8(draw_monkey_after_icon + AE_MK_MODEL_LOD) == 2) {'),
    ("Draw ot", 'ot_offset = RD32(draw_monkey_after_icon + AE_MK_ALERT_ICON_KIND) + RD16(info);', 'ot_offset = RD32(draw_monkey_after_icon + AE_MK_ALERT_ICON_KIND) + RD16(info) + 1u;'),
    ("Draw ctx90", 'ctx = AE_G_MODEL_DRAW_BASE + idx * 16504u + AE_MK_DRAW_CONTEXTS_WITH_OT;', 'ctx = AE_G_MODEL_DRAW_BASE + idx * 16504u + 0x94u;'),
    ("Draw area46", "if (area == 0x46) {", "if (area == 0x45) {"),
    ("Draw area24", "if (area == 0x24) {", "if (area == 0x25) {"),
    ("Draw state1A", 'if (RD8(sound_monkey + AE_MK_AI_STATE) == 0x1A) {', 'if (RD8(sound_monkey + AE_MK_AI_STATE) == 0x1B) {'),
    ("Draw sound", "SoundLoopStartEx, 1, 0x602D,", "SoundLoopStartEx, 1, 0x602E,"),
    ("Draw s2a", 'extra_model = AE_G_MONKEY_EXTRA_MODEL_A;', 'extra_model = 0x800EC190u;'),
    ("Draw s2b", 'extra_model = AE_G_MONKEY_EXTRA_MODEL_B;', 'extra_model = 0x800EC0D0u;'),
    ("Draw state30", "if (state != 30 && state != 32) {", "if (state != 30 && state != 31) {"),
    ("Draw modelidx", '(uint32_t)model * 16u + AE_G_MONKEY_HELD_MODELS', '(uint32_t)model * 15u + AE_G_MONKEY_HELD_MODELS'),
    ("Draw loop8", '} while (s16((uint32_t)held_model) < 8);', '} while (s16((uint32_t)held_model) < 7);'),
    ("Draw final", "RD32(AE_G_MONKEY_INFO_TABLE)) >> 6;", "RD32(AE_G_MONKEY_INFO_TABLE)) >> 5;"),
    ("Draw lodend", 'if (RD8(sound_monkey + AE_MK_MODEL_LOD) == 3) return;', 'if (RD8(sound_monkey + AE_MK_MODEL_LOD) == 2) return;'),
    ("Draw table", 'RD8(AE_G_MONKEY_GROUP_MODELS + v)', 'RD8(0x800AEBF9u + v)'),
    ("Draw rec", 'if (RD16(rec + AE_MK_INFO_ATTACK_COUNT) != 0) {', 'if (RD16(rec + 0x20) != 0) {'),
    ("Draw modeloff", 'RDS16(AE_G_MONKEY_ATTACK_TABLE + k8 + 2u)', 'RDS16(AE_G_MONKEY_ATTACK_TABLE + k8 + 4u)'),
    ("Draw activity", "if (act != 4) {", "if (act != 5) {"),
    ("Draw handle", 'if (RD32(sound_monkey + AE_MK_SFX_HANDLE) == 0xFFFFFFFFu) {', 'if (RD32(sound_monkey + AE_MK_SFX_HANDLE) == 0xFFFFFFFEu) {'),
    ("Draw free", "SoundLoopStopAndFree, 0x602D, RD32(sound_monkey + AE_MK_SFX_HANDLE), 0, 0);", "SoundLoopStopAndFree, 0x602C, RD32(sound_monkey + AE_MK_SFX_HANDLE), 0, 0);"),
    ("Draw free handle", "SoundLoopStopAndFree, 0x602D, RD32(sound_monkey + AE_MK_SFX_HANDLE), 0, 0);", "SoundLoopStopAndFree, 0x602D, 0, 0, 0);"),
    ("Draw screen", 'const uint32_t SCREEN_VERTS = AE_G_MODEL_VERTICES;', 'const uint32_t SCREEN_VERTS = 0x800E7D10u;'),
    ("Draw bones", 'AE_MK_HOOK(r, h, ModelBuildBoneMatrices, draw_monkey, 3, 0, 0);', 'AE_MK_HOOK(r, h, ModelBuildBoneMatrices, draw_monkey, 2, 0, 0);'),
]

# the functions a mutation can influence (by the first word of its label); the other families are skipped to save time
MUT_FUNCS = {
    "Restore": ["MonkeyRestoreActivities"], "Notice": ["MonkeyStartNotice"], "1E": ["MonkeyEnterState_1E"],
    "34": ["MonkeyEnterState_34"], "Knock": ["MonkeyEnterKnockdown"], "2F": ["MonkeyEnterState2F"], "30": ["MonkeyEnterState_30"],
    "36": ["MonkeyEnterState_36"], "20": ["MonkeyEnterState_20"], "24": ["MonkeyEnterState_24"], "0C": ["MonkeyEnterState_0C"],
    "35": ["MonkeyEnterState_35"], "1A": ["MonkeyEnterState_1A"], "09": ["MonkeyEnterState_09"], "Idle": ["MonkeyEnterIdleWait"],
    "Resume": ["MonkeyResumeIdle"], "Look": ["MonkeyEnterRandomLook"],
    "Pick": ["MonkeyPickAttackA", "MonkeyPickAttackB", "MonkeyPickAttackC"], "Mark": ["MonkeyMarkBlockedLinks"],
    "Target": ["MonkeyUpdateTargetDistance"], "3E": ["MonkeyAi_State3E"], "Visit": ["MonkeyNavVisitUpdate"],
    "Draw": ["MonkeyDrawModel"],
}


def run_mutations(scale, seed, only_labels=()):
    rel = "src/monkeys/ae_monkey_level3.c"
    text = (ROOT / rel).read_text(encoding="utf-8")
    survivors = []
    tmp = Path(tempfile.mkdtemp(prefix="mk3b_mut_"))
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
        h.reload_c({rel: mp}, tag=f"mk3bmut{k}")
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
    ap.add_argument("--cases", type=float, default=40, help="scale of the case counts in percent")
    ap.add_argument("--seed", type=int, default=0x944231)
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
        survivors = run_mutations(a.cases, a.seed, a.mutate_only)
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
