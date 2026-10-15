"""Differential check of monkey port batch 4c (src/monkeys/ae_monkey_level6.c, call-graph level 6) against the original MIPS
code: for every ported function many scenarios are built in a byte-exact RAM image (SCUS_944.23 at its real addresses; monkeys /
nav nodes / extra link records / MonkeyInfo / animation tables / globals laid out as MonkeySpawnAll and the level loader leave them,
plus randomised and edge-case states); the original instructions (GTE through the PCSX-ReARMed oracle) and the C port run on
identical images with identical hook behaviour; return value, RAM outside the stack window, scratchpad and the sequence of hook
calls must be identical.  The batch 1..4b functions the code calls run as the C ports on the C side and as the original
instructions on the MIPS side.  Needs extract/disc/SCUS_944.23 (skips if missing).  See check_monkey_level4.py / level5.py and
monkey_scen4.py.

Usage: uv run python tools/mips/check_monkey_level6.py [--cases PERCENT] [--seed S] [--coverage] [--trapv] [--mutate]
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
from monkey_scen import combat, navpos_world, anim_globals, M32, s32, s16, u16, FRAME, G_TARGET, STATE_TABLE  # noqa: E402

# name -> (original address, size, return kind: s = int32 / u = uint32 / v = void, takes hooks, argument kinds[, C name])
FUNCS = {
    "MonkeyAlertedDecide": (0x80037550, 880, "v", True, ""),
    "MonkeyChooseNextNodeA": (0x80038A1C, 128, "s", True, "ss"),
    "MonkeyChooseNextNodeB": (0x80038A9C, 1124, "s", True, "ss"),
}
EXTRA_COVERAGE = {
    "MonkeyNodeActionDispatch (batch 4b)": (0x80036CD0, 920), "MonkeyConsiderTurnBack (batch 4b)": (0x800386C8, 588),
    "MonkeyConsiderTurnBackRandom (batch 4b)": (0x80038914, 264), "MonkeyPickBestLink (batch 4a)": (0x80038120, 1256),
    "MonkeySetNavNode (batch 4a)": (0x80035080, 236), "MonkeySwapNavNodes (batch 4a)": (0x80038694, 52),
    "MonkeyEnterState_1D (batch 4a)": (0x800359F0, 140), "MonkeyEnterState_04 (batch 4a)": (0x80035A7C, 340),
    "MonkeyEnterState_2C (batch 4a)": (0x80036964, 208), "MonkeyEnterState_1E (batch 3b)": (0x8003581C, 252),
    "MonkeyEnterState_1A (batch 3b)": (0x80035FBC, 96), "MonkeyEnterState_35 (batch 3b)": (0x80035F8C, 48),
    "MonkeyEnterState_09 (batch 3b)": (0x8003601C, 188), "MonkeyEnterState_24 (batch 3b)": (0x80035F1C, 56),
    "MonkeyEnterState_0C (batch 3b)": (0x80035F54, 56), "MonkeyEnterState_3D (batch 3a)": (0x800360D8, 0x54),
    "MonkeyEnterState_3E (batch 3a)": (0x8003612C, 0x48), "MonkeyPrepareJumpArc (batch 3a)": (0x800364BC, 0x270),
    "MonkeySetState (batch 3a)": (0x80033220, 216), "MonkeyUpdateLinkSlope (batch 3a)": (0x80035370, 260),
    "MonkeyUpdateGoalPos (batch 3a)": (0x80035474, 360), "MonkeyUpdateGoal (batch 3b)": (0x800355DC, 40),
    "MonkeyScoreLinksByTarget (batch 3a)": (0x8003AC54, 0xE0), "MonkeyMarkBlockedLinks (batch 3b)": (0x80038054, 204),
    "MonkeyNavLinkAllowed (batch 3a)": (0x80037E8C, 456), "MonkeyPickAttackA (batch 3b)": (0x8003738C, 148),
    "MonkeyTryAttackKind (batch 3a)": (0x8003710C, 640), "MonkeyAlertPuff (batch 2)": (0x80039248, 84),
    "MonkeyReachedNode (batch 1)": (0x8003516C, 0x1B0), "MonkeyAnimFinished (batch 1)": (0x80033A3C, 0x90),
    "NavNodeGetLink (batch 1)": (0x8003386C, 0x120), "rcos (SDK)": (0x8002FC20, 160),
}
DISPATCH_BITS = [0x100, 0x800, 0x20, 0x20000, 8, 4, 0x10, 1, 0x10000, 0x800000, 0x1000000]


def check_all(h, scale, seed):
    t0 = time.time()
    h.mask = [(MONKEYS + MONKEY_SIZE * k + 0xC8 - 0x80000000, 4) for k in range(8)]   # see check_monkey_level4.py

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

    # ------------------------------------------------------------------ AlertedDecide
    def alerted_world(m, rng, i, st=None, bit3=None, flagbits=None):
        c = s4.nav_entry_world(m, rng)
        s4.attack_fields(m, rng, c)
        w = W(m)
        nn = m.read16(0x800B4D18)
        s4.no_stale_goal(m, rng, c, nn)
        s4.stale_enter(m, rng, 64)
        bits = 0 if rng.random() < 0.65 else sum(b for b in DISPATCH_BITS if rng.random() < 0.2)
        w.w32(c + 0x228, bits | (rng.getrandbits(32) & 0x200 if rng.random() < 0.3 else 0))
        state = st if st is not None else rng.choice([11, 11, 3, 8, 12, 36, 35, 57, 9, 0x1A, 0x35, 4, 1, rng.randint(0, 0x42)])
        w.w8(c + 0x78, state)
        if flagbits is None:
            flagbits = sum(b for b in (1 << 20, 1 << 23, 1 << 26, 1 << 22) if rng.random() < rng.choice([0.1, 0.5, 0.5]))
        w.w32(c + 0x248, (rng.getrandbits(32) & ~((1 << 20) | (1 << 23) | (1 << 26) | (1 << 22)) | flagbits) & M32)
        w.w8(c + 0x79, rng.choice([0, 1, 1, 2]))
        w.w8(c + 0x249, rng.getrandbits(8))
        dist = rng.choice([299, 300, 301, 1499, 1500, 1501, 1700, 0, rng.randint(250, 1600), rng.getrandbits(32)])
        w.w32(c + 0x20C, dist & M32)
        w.w8(c + 0x222, rng.choice([0, 1, 2, 0x7F, 0x80, 0xFF, rng.getrandbits(8)]))
        w.w16(c + 0x94, rng.choice([0, 1, -1, 5, 0x7FFF, 0x8000, rng.getrandbits(16)]) & 0xFFFF)
        flagw = m.read32(STATE_TABLE + 8 * (state % 0x43) + 4)
        b3 = bit3 if bit3 is not None else rng.choice([0, 1, 1])
        w.w32(STATE_TABLE + 8 * (state % 0x43) + 4, (flagw | 8) if b3 else (flagw & ~8))
        w.w8(0x800F447C, rng.choice([3, 3, 0, 1, 2, rng.getrandbits(8)]))
        w.w16(0x800F4472, rng.choice([0, 0, 0, 1, 3, 6, 8, 2]))
        w.w32(0x800B4D10, rng.getrandbits(16))
        return c
    run("MonkeyAlertedDecide", 2500, alerted_world, nh=True, allow_trap=True)
    ci = 400000
    for st in range(0x43):
        for b3 in (0, 1):
            for boss in (0, 3):
                rng = rng_of("MonkeyAlertedDecide-grid", ci)

                def setup(m, rng=rng, st=st, b3=b3, boss=boss, ci=ci):
                    c = alerted_world(m, rng, ci, st=st, bit3=b3, flagbits=0)
                    W(m).w16(0x800F4472, boss)
                    return c
                h.run("MonkeyAlertedDecide", f"grid {st} {b3} {boss}", ci, setup, lambda c: (), allow_trap=True)
                ci += 1

    # ------------------------------------------------------------------ ChooseNextNodeA / B
    def choose_world(m, rng, i, alerted=None):
        c = s4.pick_world(m, rng, i)
        w = W(m)
        nn = m.read16(0x800B4D18)
        s4.no_stale_goal(m, rng, c, nn)
        if alerted is not None:
            w.w8(c + 0x79, alerted)
        w.w16(c + 0xE6, rng.randint(0, nn - 1))
        w.w16(c + 0x220, rng.choice([rng.randint(-300, 600), 200, 0, 1, -1, rng.getrandbits(16)]) & 0xFFFF)
        w.w32(c + 0x228, rng.choice([0, 2, 3, rng.getrandbits(32)]))
        w.w16(0x800F4472, rng.choice([0, 0, 0, 0, 1, 2, 3, 6, 8]))
        return c
    arg_a = lambda c, rng, i: (rng.choice([0, 0, 0, 1, 0x10000, -1, rng.getrandbits(32)]) & M32, rng.choice([0, 0, 1, 0x100, -1, rng.getrandbits(32)]) & M32)
    run("MonkeyChooseNextNodeA", 2500, choose_world, nh=True, allow_trap=True, argsf=arg_a)
    run("MonkeyChooseNextNodeB", 3000, choose_world, nh=True, allow_trap=True, argsf=arg_a)
    run("MonkeyChooseNextNodeB", 1500, lambda m, rng, i: choose_world(m, rng, i, alerted=0), nh=True, allow_trap=True, i0=70000, argsf=arg_a)
    run("MonkeyChooseNextNodeB", 2000, lambda m, rng, i: choose_world(m, rng, i, alerted=1), nh=True, allow_trap=True, i0=80000, argsf=arg_a)

    # B alerted: the first usable link scored exactly around the threshold s7 (rcos(682), special areas rcos(angle * 4096 / 360))
    aim = s4.DotAim(h)

    def aimed_world(m, rng, i):
        c = choose_world(m, rng, i, alerted=1)
        w = W(m)
        nn = m.read16(0x800B4D18)
        s4.plain_nodes(m, nn)
        for k in range(3):
            w.w32(c + 0x24 + 4 * k, 0)
        tgt = m.read32(G_TARGET)
        w.w32(tgt + 0x28, 0)
        for q in range(nn):
            w.w16(NAV_BASE + 0x18 * q + 6, 0)
        node0 = m.read16(c + 0xF2)
        if node0 != 0 and rng.random() < 0.5:                  # a link to node 0 (the chosen link id 0 is a distinct case: `sel >= 0`)
            s4.relink(m, rng, nn, node0, 0, True, rng.random() < 0.5)
            s4.set_link_counts(m, rng, nn, exact=1.0)
            if rng.random() < 0.6:                              # the link to node 0 last (the lists are sorted by id otherwise, so
                lp = m.read32(NAV_BASE + 0x18 * node0 + 0xC)    # the fallback pass would find it first and the `sel >= 0` test would not matter)
                ents = [m.read16(lp + 2 * j) for j in range(m.read8(NAV_BASE + 0x18 * node0 + 0xA))]
                ents.sort(key=lambda e: (e & 0x3FFF) == 0)
                for j, e in enumerate(ents):
                    w.w16(lp + 2 * j, e)
        a = NAV_BASE + 0x18 * node0
        links = []
        for j in range(m.read8(a + 0xA)):
            e = m.read16(m.read32(a + 0xC) + 2 * j)
            if e & 0x4000:
                links.append(e & 0x3FFF)
        special = m.read16(0x800F4476) in (0x29, 0x43, 0x35)
        thr = s32(s4.mips_call(m, 0x8002FC20, (682,)))
        if special:
            ang = s32(s4.mips_call(m, 0x80037D6C))
            x = ang * 4096
            q = abs(x) // 360
            thr = s32(s4.mips_call(m, 0x8002FC20, ((q if x >= 0 else -q) & M32,)))
        if links:
            node = 0 if (0 in links and rng.random() < 0.5) else (links[0] if rng.random() < 0.7 else rng.choice(links))
            n = (m.reads16(NAV_BASE + 0x18 * node + 4), m.reads16(NAV_BASE + 0x18 * node + 8))
            res = aim.vector_for(rng, n, thr + rng.choice([-1, 0, 0, 1]), spread=(0,))
            if res:
                w.w32(tgt + 0x24, res[0][0] & M32)
                w.w32(tgt + 0x2C, res[0][1] & M32)
        w.w16(c + 0xEC, 0xFFFF)
        w.w32(0x800B4D10, s4.seed_for(rng, lambda v: v % 100 in rng.choice([(0, 1, 2, 3, 4), (4, 5), (5, 6), (4, 5, 6)])))
        return c
    run("MonkeyChooseNextNodeB", 1500, aimed_world, nh=True, allow_trap=True, i0=90000,
        argsf=lambda c, rng, i: (rng.choice([1, 1, 0x10000 | 1, -1]) & M32, rng.choice([0, 1, 0x100]) & M32))

    # AlertedDecide: attack pick A reached (flags bit 23, state 3) with the player flag set, so that it returns 100 and the state
    # choice (8 / 3 by chance) follows
    def alerted_attack_world(m, rng, i):
        c = alerted_world(m, rng, i, st=3, bit3=0, flagbits=(1 << 23))
        w = W(m)
        w.w32(c + 0x228, 0)
        w.w16(0x800EC2CA, rng.choice([1, -1, 0x100, 0, 0]) & 0xFFFF)
        w.w16(c + 0x94, rng.choice([0, 0, -3, 4]) & 0xFFFF)
        w.w32(c + 0x20C, rng.choice([0, 100, 5000]) & M32)
        w.w8(0x800F447C, 3)
        w.w16(0x800F4472, 0)
        w.w32(0x800B4D10, s4.seed_for(rng, lambda v: v % 100 in rng.choice([(29, 30), (28, 29, 30, 31), (5,)])))
        return c
    run("MonkeyAlertedDecide", 600, alerted_attack_world, nh=True, allow_trap=True, i0=95000)

    print(f"scenario families done in {time.time() - t0:.1f}s")


MUTATIONS = [
    ("Alert st12", 'uint32_t was_fast_ground_state = (st0 == 12 || st0 == 36) ? 1u : 0u;', 'uint32_t was_fast_ground_state = (st0 == 13 || st0 == 36) ? 1u : 0u;'),
    ("Alert st36", 'uint32_t was_fast_ground_state = (st0 == 12 || st0 == 36) ? 1u : 0u;', 'uint32_t was_fast_ground_state = (st0 == 12 || st0 == 37) ? 1u : 0u;'),
    ("Alert f20", "if ((RD32(monkey + AE_MK_FLAGS) >> 20) & 1u) return;", "if ((RD32(monkey + AE_MK_FLAGS) >> 19) & 1u) return;"),
    ("Alert 249", 'if (RD8(monkey + AE_MK_ALERTED) == 0) WR8(monkey + AE_MK_LOST_SIGHT_COUNT, 0);', 'if (RD8(monkey + AE_MK_ALERTED) == 0) WR8(monkey + AE_MK_FLAGS_HI, 0);'),
    ("Alert 79", 'if (RD8(monkey + AE_MK_ALERTED) == 0) WR8(monkey + AE_MK_LOST_SIGHT_COUNT, 0);', 'if (RD8(monkey + AE_MK_ALERTED) != 0) WR8(monkey + AE_MK_LOST_SIGHT_COUNT, 0);'),
    ("Alert 12C", 'if (RD8(AE_G_MONKEY_BOSS_MODE) != 3) {', 'if (RD8(AE_G_STATE_BASE + 0x12D) != 3) {'),
    ("Alert 3", 'if (RD8(AE_G_MONKEY_BOSS_MODE) != 3) {', 'if (RD8(AE_G_MONKEY_BOSS_MODE) != 2) {'),
    ("Alert bossst", "if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != 11) ae_MonkeySetState(r, 11);", "if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != 12) ae_MonkeySetState(r, 11);"),
    ("Alert bossset", "if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != 11) ae_MonkeySetState(r, 11);", "if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != 11) ae_MonkeySetState(r, 12);"),
    ("Alert alerted1", 'WR8(monkey + AE_MK_ALERTED, 1);\n    uint32_t dist', 'WR8(monkey + AE_MK_ALERTED, 2);\n    uint32_t dist'),
    ("Alert d300", 'uint32_t target_in_range = (dist - 300u) < 1201u;', 'uint32_t target_in_range = (dist - 301u) < 1201u;'),
    ("Alert d1500", 'uint32_t target_in_range = (dist - 300u) < 1201u;', 'uint32_t target_in_range = (dist - 300u) < 1200u;'),
    ("Alert distoff", 'uint32_t dist = RD32(cur_monkey(r) + AE_MK_DIST_TO_TARGET);', 'uint32_t dist = RD32(cur_monkey(r) + AE_MK_DIST_XZ_TO_GOAL);'),
    ("Alert boss2", 'if (boss_again == 0 && RD8(cur_monkey(r) + AE_MK_AI_STATE) == 11 && (target_in_range & 0xFFu) != 0) {', 'if (boss_again != 0 && RD8(cur_monkey(r) + AE_MK_AI_STATE) == 11 && (target_in_range & 0xFFu) != 0) {'),
    ("Alert st11", 'if (boss_again == 0 && RD8(cur_monkey(r) + AE_MK_AI_STATE) == 11 && (target_in_range & 0xFFu) != 0) {', 'if (boss_again == 0 && RD8(cur_monkey(r) + AE_MK_AI_STATE) == 10 && (target_in_range & 0xFFu) != 0) {'),
    ("Alert s1a", 'if (boss_again == 0 && RD8(cur_monkey(r) + AE_MK_AI_STATE) == 11 && (target_in_range & 0xFFu) != 0) {', 'if (boss_again == 0 && RD8(cur_monkey(r) + AE_MK_AI_STATE) == 11) {'),
    ("Alert f26", 'if (((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 26) & 1u) && (target_in_range & 0xFFu) != 0) {', 'if (((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 25) & 1u) && (target_in_range & 0xFFu) != 0) {'),
    ("Alert s1b", 'if (((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 26) & 1u) && (target_in_range & 0xFFu) != 0) {', 'if (((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 26) & 1u)) {'),
    ("Alert st11b", '} else if (((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 26) & 1u) && (target_in_range & 0xFFu) != 0) {\n        ae_MonkeySetState(r, 11);', '} else if (((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 26) & 1u) && (target_in_range & 0xFFu) != 0) {\n        ae_MonkeySetState(r, 12);'),
    ("Alert bit3", 'if (((state_flags >> 3) & 1u) && RDS8(current_monkey + AE_MK_PATROL_COUNTER) > 0) {', 'if (((state_flags >> 4) & 1u) && RDS8(current_monkey + AE_MK_PATROL_COUNTER) > 0) {'),
    ("Alert 222", 'if (((state_flags >> 3) & 1u) && RDS8(current_monkey + AE_MK_PATROL_COUNTER) > 0) {', 'if (((state_flags >> 3) & 1u) && RDS8(current_monkey + AE_MK_PATROL_COUNTER) >= 0) {'),
    ("Alert 222u", 'if (((state_flags >> 3) & 1u) && RDS8(current_monkey + AE_MK_PATROL_COUNTER) > 0) {', 'if (((state_flags >> 3) & 1u) && RD8(current_monkey + AE_MK_PATROL_COUNTER) > 0) {'),
    ("Alert disp", "if (ae_MonkeyNodeActionDispatch(r, h) != 0) {", "if (ae_MonkeyNodeActionDispatch(r, h) == 0) {"),
    ("Alert fin3", 'if (state == 3 || state == 8) return;', 'if (state == 3 || state == 9) return;'),
    ("Alert fin8", 'if (state == 3 || state == 8) return;', 'if (state == 4 || state == 8) return;'),
    ("Alert f23", 'if ((RD32(current_monkey + AE_MK_FLAGS) >> 23) & 1u) {\n                if (RD8(current_monkey + AE_MK_AI_STATE) == 3) {', 'if ((RD32(current_monkey + AE_MK_FLAGS) >> 22) & 1u) {\n                if (RD8(current_monkey + AE_MK_AI_STATE) == 3) {'),
    ("Alert atkst", 'if ((RD32(current_monkey + AE_MK_FLAGS) >> 23) & 1u) {\n                if (RD8(current_monkey + AE_MK_AI_STATE) == 3) {', 'if ((RD32(current_monkey + AE_MK_FLAGS) >> 23) & 1u) {\n                if (RD8(current_monkey + AE_MK_AI_STATE) == 4) {'),
    ("Alert k100", 'if (attack_kind != 100) return;', 'if (attack_kind != 99) return;'),
    ("Alert st8", 'if (state == 8 || state == 35 || state == 57 || RDS16(current_monkey + AE_MK_ACTION_COOLDOWN) > 0) {', 'if (state == 9 || state == 35 || state == 57 || RDS16(current_monkey + AE_MK_ACTION_COOLDOWN) > 0) {'),
    ("Alert st35", 'if (state == 8 || state == 35 || state == 57 || RDS16(current_monkey + AE_MK_ACTION_COOLDOWN) > 0) {', 'if (state == 8 || state == 36 || state == 57 || RDS16(current_monkey + AE_MK_ACTION_COOLDOWN) > 0) {'),
    ("Alert st57", 'if (state == 8 || state == 35 || state == 57 || RDS16(current_monkey + AE_MK_ACTION_COOLDOWN) > 0) {', 'if (state == 8 || state == 35 || state == 58 || RDS16(current_monkey + AE_MK_ACTION_COOLDOWN) > 0) {'),
    ("Alert cd", 'if (state == 8 || state == 35 || state == 57 || RDS16(current_monkey + AE_MK_ACTION_COOLDOWN) > 0) {', 'if (state == 8 || state == 35 || state == 57 || RDS16(current_monkey + AE_MK_ACTION_COOLDOWN) >= 0) {'),
    ("Alert cdoff", 'if (state == 8 || state == 35 || state == 57 || RDS16(current_monkey + AE_MK_ACTION_COOLDOWN) > 0) {', 'if (state == 8 || state == 35 || state == 57 || RDS16(current_monkey + AE_MK_COUNTER96) > 0) {'),
    ("Alert r30", 'next_state = (rem < 30u) ? 8u : 3u;', 'next_state = (rem < 31u) ? 8u : 3u;'),
    ("Alert r30b", 'next_state = (rem < 30u) ? 8u : 3u;', 'next_state = (rem < 30u) ? 3u : 8u;'),
    ("Alert jump", 'if (!ae_MonkeyIsJumpArcState(r)) {', 'if (ae_MonkeyIsJumpArcState(r)) {'),
    ("Alert s9", 'if (state != 9 && state != 0x1A && state != 0x35 &&\n                !(RD32(current_monkey + AE_MK_FLAGS2) & 0x200u) && !ae_AreaIsBossArena(r)) {', 'if (state != 8 && state != 0x1A && state != 0x35 &&\n                !(RD32(current_monkey + AE_MK_FLAGS2) & 0x200u) && !ae_AreaIsBossArena(r)) {'),
    ("Alert s1A", 'if (state != 9 && state != 0x1A && state != 0x35 &&\n                !(RD32(current_monkey + AE_MK_FLAGS2) & 0x200u) && !ae_AreaIsBossArena(r)) {', 'if (state != 9 && state != 0x1B && state != 0x35 &&\n                !(RD32(current_monkey + AE_MK_FLAGS2) & 0x200u) && !ae_AreaIsBossArena(r)) {'),
    ("Alert s35", 'if (state != 9 && state != 0x1A && state != 0x35 &&\n                !(RD32(current_monkey + AE_MK_FLAGS2) & 0x200u) && !ae_AreaIsBossArena(r)) {', 'if (state != 9 && state != 0x1A && state != 0x36 &&\n                !(RD32(current_monkey + AE_MK_FLAGS2) & 0x200u) && !ae_AreaIsBossArena(r)) {'),
    ("Alert f200", 'if (state != 9 && state != 0x1A && state != 0x35 &&\n                !(RD32(current_monkey + AE_MK_FLAGS2) & 0x200u) && !ae_AreaIsBossArena(r)) {', 'if (state != 9 && state != 0x1A && state != 0x35 &&\n                !(RD32(current_monkey + AE_MK_FLAGS2) & 0x100u) && !ae_AreaIsBossArena(r)) {'),
    ("Alert boss3", 'if (state != 9 && state != 0x1A && state != 0x35 &&\n                !(RD32(current_monkey + AE_MK_FLAGS2) & 0x200u) && !ae_AreaIsBossArena(r)) {', 'if (state != 9 && state != 0x1A && state != 0x35 &&\n                !(RD32(current_monkey + AE_MK_FLAGS2) & 0x200u) && ae_AreaIsBossArena(r)) {'),
    ("Alert prev", 'WR16(cur_monkey(r) + AE_MK_PREV_NODE, 0xFFFFu);', 'WR16(cur_monkey(r) + AE_MK_PREV_NODE, 0xFFFEu);'),
    ("Alert puff", "if (!ae_MonkeyIsState0C_or_24(r)) ae_MonkeyAlertPuff(r, h);\n    }", "if (ae_MonkeyIsState0C_or_24(r)) ae_MonkeyAlertPuff(r, h);\n    }"),
    ("Alert s2", '    if (was_fast_ground_state != 0) {\n        if (!ae_MonkeyIsState0C_or_24(r))', '    if (was_fast_ground_state == 0) {\n        if (!ae_MonkeyIsState0C_or_24(r))'),
    ("A zeroarg", "if ((((uint32_t)p1 << 16) == 0) && ae_MonkeyReachedNode(r) == 0) {", "if ((((uint32_t)p1 << 8) == 0) && ae_MonkeyReachedNode(r) == 0) {"),
    ("A reached", "if ((((uint32_t)p1 << 16) == 0) && ae_MonkeyReachedNode(r) == 0) {", "if ((((uint32_t)p1 << 16) == 0) && ae_MonkeyReachedNode(r) != 0) {"),
    ("A limit", "int32_t link = s16((uint32_t)ae_MonkeyPickBestLink(r, h, s16((uint32_t)p2)));", "int32_t link = s16((uint32_t)ae_MonkeyPickBestLink(r, h, s16((uint32_t)p2) + 1));"),
    ("A none", "if (link == -1) return 0;\n    ae_MonkeySetNavNode(r, h, link);", "if (link == -2) return 0;\n    ae_MonkeySetNavNode(r, h, link);"),
    ("A ret", "ae_MonkeySetNavNode(r, h, link);\n    return 1;", "ae_MonkeySetNavNode(r, h, link);\n    return 0;"),
    ("A ctb", "return ae_MonkeyConsiderTurnBack(r, h);", "ae_MonkeyConsiderTurnBack(r, h);\n        return 1;"),
    ("B 682", 'int32_t cosine_limit = ae_sdk_rcos(r, 682);', 'int32_t cosine_limit = ae_sdk_rcos(r, 683);'),
    ("B alerted", 'if (RD8(cur_monkey(r) + AE_MK_ALERTED) == 0) {\n        uint32_t cnt = link_count', 'if (RD8(cur_monkey(r) + AE_MK_ALERTED) != 0) {\n        uint32_t cnt = link_count'),
    ("B cnt", "if (!(0 < (int32_t)cnt)) {\n            ae_MonkeyUpdateGoal(r, h);\n            return 0;", "if (!(1 < (int32_t)cnt)) {\n            ae_MonkeyUpdateGoal(r, h);\n            return 0;"),
    ("B hist", 'int32_t hist = RDS16(monkey + AE_MK_PREV_NODE);\n            int32_t next_link_index = link_index + 1;\n            if (candidate_node != hist) {\n                if (RD8(monkey + (uint32_t)s16((uint32_t)link_index) + AE_MK_LINK_BLOCKED) != 1) {', 'int32_t hist = RDS16(monkey + AE_MK_NAV_MARK_COUNT);\n            int32_t next_link_index = link_index + 1;\n            if (candidate_node != hist) {\n                if (RD8(monkey + (uint32_t)s16((uint32_t)link_index) + AE_MK_LINK_BLOCKED) != 1) {'),
    ("B blockedoff", 'if (RD8(monkey + (uint32_t)s16((uint32_t)link_index) + AE_MK_LINK_BLOCKED) != 1) {', 'if (RD8(monkey + (uint32_t)s16((uint32_t)link_index) + 0x1DB) != 1) {'),
    ("B ret1", 'ae_MonkeySetNavNode(r, h, candidate_node);\n                    return 1;\n                }\n            }\n            link_index = next_link_index;\n            uint32_t c2 = link_count(r, monkey);\n            if (!(s16((uint32_t)link_index) < (int32_t)c2)) break;\n        }\n        ae_MonkeyUpdateGoal(r, h);\n        return 0;\n    }\n    if (ae_AreaIsSpecial', 'ae_MonkeySetNavNode(r, h, candidate_node);\n                    return 0;\n                }\n            }\n            link_index = next_link_index;\n            uint32_t c2 = link_count(r, monkey);\n            if (!(s16((uint32_t)link_index) < (int32_t)c2)) break;\n        }\n        ae_MonkeyUpdateGoal(r, h);\n        return 0;\n    }\n    if (ae_AreaIsSpecial'),
    ("B special", "if (ae_AreaIsSpecialMonkeyArea(r)) {\n        int32_t ang", "if (!ae_AreaIsSpecialMonkeyArea(r)) {\n        int32_t ang"),
    ("B shift", "int32_t x = (int32_t)((uint32_t)ang << 12);", "int32_t x = (int32_t)((uint32_t)ang << 11);"),
    ("B magic", "mul_hi(x, (int32_t)0xB60B60B7u) + x, 8);", "mul_hi(x, (int32_t)0xB60B60B7u) + x, 7);"),
    ("B p1", "if (((uint32_t)p1 << 16) == 0 && ae_MonkeyReachedNode(r) == 0) {\n        ae_MonkeyConsiderTurnBackRandom", "if (((uint32_t)p1 << 16) != 0 && ae_MonkeyReachedNode(r) == 0) {\n        ae_MonkeyConsiderTurnBackRandom"),
    ("B reached", "if (((uint32_t)p1 << 16) == 0 && ae_MonkeyReachedNode(r) == 0) {\n        ae_MonkeyConsiderTurnBackRandom", "if (((uint32_t)p1 << 16) == 0 && ae_MonkeyReachedNode(r) != 0) {\n        ae_MonkeyConsiderTurnBackRandom"),
    ("B score", "ae_MonkeyScoreLinksByTarget(r, h, RDS16(cur_monkey(r) + AE_MK_NAV_NODE0));", "ae_MonkeyScoreLinksByTarget(r, h, RDS16(cur_monkey(r) + AE_MK_NAV_NODE1));"),
    ("B cnt2", 'uint32_t cnt = link_count(r, cur_monkey(r));\n    int32_t link_index = 0;\n    if (cnt != 0) {', 'uint32_t cnt = link_count(r, cur_monkey(r));\n    int32_t link_index = 0;\n    if (cnt > 1) {'),
    ("B scoreoff", 'int32_t score = RDS16(sa + AE_MK_LINK_SCORES);', 'int32_t score = RDS16(sa + 0x178);'),
    ("B cmp7", 'if (score < cosine_limit) {\n                    if (rand_mod100(r) < 5u) {', 'if (score <= cosine_limit) {\n                    if (rand_mod100(r) < 5u) {'),
    ("B rand5", "if (rand_mod100(r) < 5u) {", "if (rand_mod100(r) < 6u) {"),
    ("B better", 'if (score < s16((uint32_t)best_score)) {', 'if (score <= s16((uint32_t)best_score)) {'),
    ("B take", 'best_score = (int32_t)score_bits;\n                        best_node = linked_node;\n                        take_now = 1;', 'best_score = (int32_t)score_bits;\n                        best_node = linked_node + 1;\n                        take_now = 1;'),
    ("B p2", 'if (((uint32_t)p2 << 16) == 0) {\n        if (!(s16((uint32_t)best_score) < cosine_limit)) {', 'if (((uint32_t)p2 << 16) != 0) {\n        if (!(s16((uint32_t)best_score) < cosine_limit)) {'),
    ("B s4s7", 'if (!(s16((uint32_t)best_score) < cosine_limit)) {\n            ae_MonkeyUpdateGoal(r, h);', 'if (!(s16((uint32_t)best_score) <= cosine_limit)) {\n            ae_MonkeyUpdateGoal(r, h);'),
    ("B sel", "if (sel >= 0) {", "if (sel > 0) {"),
    ("B c3", "if (c3 == 0) return 0;", "if (c3 == 1) return 0;"),
    ("B fallback", 'int32_t hist = RDS16(monkey + AE_MK_PREV_NODE);\n        int32_t next_link_index = link_index + 1;\n        if (candidate_node != hist) {\n            if (RD8(monkey + (uint32_t)link_index_narrow + AE_MK_LINK_BLOCKED) != 1) {', 'int32_t hist = RDS16(monkey + AE_MK_NAV_MARK_COUNT);\n        int32_t next_link_index = link_index + 1;\n        if (candidate_node != hist) {\n            if (RD8(monkey + (uint32_t)link_index_narrow + AE_MK_LINK_BLOCKED) != 1) {'),
]

MUT_FUNCS = {
    "Alert": ["MonkeyAlertedDecide"], "A": ["MonkeyChooseNextNodeA"], "B": ["MonkeyChooseNextNodeB"],
}


def run_mutations(scale, seed, only_labels=()):
    rel = "src/monkeys/ae_monkey_level6.c"
    text = (ROOT / rel).read_text(encoding="utf-8")
    survivors = []
    tmp = Path(tempfile.mkdtemp(prefix="mk6_mut_"))
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
            h.reload_c({rel: mp}, tag=f"mk6mut{k}")
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
    ap.add_argument("--cases", type=float, default=15, help="scale of the case counts in percent")
    ap.add_argument("--seed", type=int, default=0x944261)
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
