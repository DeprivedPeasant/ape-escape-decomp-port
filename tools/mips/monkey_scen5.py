"""Scenario builders for the check scripts of batch 5 (check_monkey_level9.py, check_monkey_level10.py).  Not a check itself.
Everything takes the MIPS-side RAM (`m`) and a `random.Random`; the builders of batches 2 - 4 live in monkey_scen.py /
monkey_scen4.py (and are re-used).  (check_monkey_level8.py keeps its own copies of the first helpers.)"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import monkey_harness as mh  # noqa: E402
import monkey_scen as sc  # noqa: E402
import monkey_scen4 as s4  # noqa: E402
from monkey_harness import W, MONKEYS, MONKEY_SIZE, NAV_BASE  # noqa: E402
from monkey_scen import M32, s32, s16, FRAME, G_TARGET, STATE_TABLE  # noqa: E402

G_FRAME2 = 0x800EC350
MOVE_STATES = [2, 3, 8, 9, 10, 12, 0x19, 0x1A, 0x23, 0x24, 0x35, 0x39, 0, 1, 7, 0xB, 0x1D, 0x3D]
PATROL_STATES = [0, 1, 0xB, 0x16, 0x17, 0x18, 0x1B, 0x1C, 0x21, 0x22, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x37, 0x38, 0x42]


def seed_pair(rng, pred):
    """a MonkeyRand seed whose next two values (v1, v2) satisfy pred(v1, v2)"""
    while True:
        seed = rng.getrandbits(16)
        s1 = (seed * 5 + 1) & 0xFFFF
        s2 = (s1 * 5 + 1) & 0xFFFF
        if pred(s1 >> 8, s2 >> 8):
            return seed


def common(m, rng, state=None, stale=True):
    """nav world + animation globals + attack / LOS fields; returns the current monkey.  stale: clears the flags2 bits that make the
    node dispatch enter the states whose entry functions read leftover stack (0x100, 0x800, 0x4) and keeps UpdateGoalPos from
    reading its never-written position (see monkey_scen4.py)"""
    c = s4.nav_entry_world(m, rng)
    s4.attack_fields(m, rng, c)
    s4.los_fields(m, rng, c)
    w = W(m)
    nn = m.read16(0x800B4D18)
    s4.no_stale_goal(m, rng, c, nn)
    if stale:
        w.w32(c + 0x228, m.read32(c + 0x228) & ~(0x100 | 0x800 | 0x4) & M32)
    if state is not None:
        w.w8(c + 0x78, state)
    return c


def move_fields(m, rng, c):
    """fields the move / patrol handlers read: reach radius / distances (ReachedNode), step vectors, link frame, flags2 bits, state table"""
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
    st = m.read8(c + 0x78)
    flagw = m.read32(STATE_TABLE + 8 * (st % 0x43) + 4)
    if rng.random() < 0.5:
        flagw = (flagw | 0x40) if rng.random() < 0.5 else (flagw & ~0x40 & M32)
        w.w32(STATE_TABLE + 8 * (st % 0x43) + 4, flagw)
    w.w8(c + 0x249, rng.choice([0, 0, 1, 2, 3, 0xFF]))


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
    w.w16(c + 0x5C, {"run5": first + 5 * rng.randint(0, k), "past": last + rng.choice([1, 2, 5]), "start": first + 1,
                     "rand": rng.getrandbits(16)}[mode] & 0xFFFF)


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


DISPATCH_BITS = [0x100, 0x800, 0x20, 0x20000, 8, 4, 0x10, 1, 0x10000, 0x800000, 0x1000000]


def select_world(m, rng, i):
    """the world of MonkeySelectNextNode (check_monkey_level7.py select_world): link lists with extra records, visit counters, history
    patterns for the backtrack test, node flags for the arrival, a random seed"""
    c = s4.pick_world(m, rng, i)
    sc.anim_globals(m, rng, c)
    w = W(m)
    nn = m.read16(0x800B4D18)
    s4.no_stale_goal(m, rng, c, nn)
    s4.route_fields(m, rng, c, nn)
    node0 = m.read16(c + 0xF2)
    w.w32(c + 0x248, (m.read32(c + 0x248) & ~(1 << 27)) | (rng.choice([0, 1, 1]) << 27))
    w.w16(c + 0xE6, rng.choice([node0, node0, rng.randint(0, nn - 1), rng.getrandbits(16)]))
    w.w8(c + 0x78, rng.choice([61, 61, 3, 1, 0x0C, rng.randint(0, 0x42)]))
    a, b, d = rng.randint(0, nn - 1), rng.randint(0, nn - 1), rng.randint(0, nn - 1)
    pat = rng.choice([[a, b, a, b, a, b], [a, a, a, a, a, a], [a, b, d, a, b, d], [a, b, d, d, b, a], [a, b, a, a, b, a],
                      [a, b, d, rng.randint(0, nn - 1), b, d], [rng.randint(0, nn - 1) for _ in range(6)]])
    if rng.random() < 0.6:
        pat[0] = node0
    for k, v in enumerate(pat):
        w.w16(c + 0xF2 + 2 * k, v)
    n1 = pat[1] % nn
    nb = NAV_BASE + 0x18 * n1
    if rng.random() < 0.25:
        w.w32(nb, m.read32(nb) | 0x10000)
    else:
        w.w32(nb, m.read32(nb) & ~0x10000 & M32)
    w.w16(c + 0xEC, rng.choice([pat[0], pat[1], rng.randint(0, 40), 0xFFFF]))
    w.w8(c + 0x79, rng.choice([0, 1, 1, 1]))
    w.w8(c + 0x7A, rng.choice([4, 4, 4, 3, 2]))
    for k in range(nn):
        a_ = NAV_BASE + 0x18 * k
        if rng.random() < 0.3:
            w.w32(a_, m.read32(a_) | rng.choice([0x20, 0x1000, 0x4000, 0x800, 0x20000, 0x40000, 0x80000, 4, 8, 0x8000, 0x10000]))
    w.w32(c + 0x20C, rng.choice([0, 500, 1499, 1500, rng.randint(0, 4000)]))
    w.w16(0x800F4472, rng.choice([0, 0, 0, 0, 1, 3, 6, 8]))
    w.w32(0x800B4D10, rng.getrandbits(16))
    w.w16(c + 0x220, rng.choice([rng.randint(-300, 600), 200, 0, 1, -1]) & 0xFFFF)
    # no dispatch states with leftover-stack reads (the call depth differs from the checks of the earlier levels)
    w.w32(c + 0x228, m.read32(c + 0x228) & ~(0x100 | 0x800 | 0x4) & M32)
    return c


def has_flag40(m):
    """True when a nav node with flag 0x40 can be read by MonkeyUpdateGoalPos: in the node array of the scenario, or anywhere in the
    0x8000 bytes after NAV_BASE (route tables can name any node index), or, for a real area, any of its nodes"""
    base = m.read32(0x800B4D00)
    count = 0x8000 // 0x18 if base == NAV_BASE else max(0, min(m.reads16(0x800B4D18), 4096))
    return any(m.read32(base + 0x18 * k) & 0x40 for k in range(count))


def narrow_init_mask(h, m, base_mask, init_mask):
    """MonkeyInit's UpdateGoalPos measures to a never-written position (leftover stack) when a node has flag 0x40, which is the only
    reason for masking the distances +0x214 / +0x218 (init_mask): worlds that asked for init_mask get it only if such a node exists"""
    if h.mask == init_mask:
        h.mask = init_mask if has_flag40(m) else base_mask
