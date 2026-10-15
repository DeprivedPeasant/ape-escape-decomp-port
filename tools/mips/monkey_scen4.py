"""Scenario builders for the check scripts of batch 4 (check_monkey_level4.py .. level7.py).  Not a check itself.  Everything
here takes the MIPS-side RAM (`m`) and a `random.Random`; the builders of batches 2 and 3 live in monkey_scen.py (and are
re-used), the ones that batch 3 kept inside check_monkey_level3.py are provided here at module level."""
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import monkey_harness as mh  # noqa: E402
import exclusion_registry as reg  # noqa: E402
import monkey_scen as sc  # noqa: E402
from monkey_harness import W, MONKEYS, MONKEY_SIZE, G_CUR, NAV_BASE  # noqa: E402,F401
from monkey_scen import (combat, navpos_world, anim_globals, attack_world, allowed_world, static_models, link_pair,  # noqa: E402,F401
                         ANIM_BASE, EXTRA_BASE, M32, s32, s16, u16, FLAG_BITS, G_TARGET, FRAME, HDR)

STK_TOP = 0x801FFF00        # sp of the function the harness calls (mips_ram.DEFAULT_SP)


def stk_index(name):
    """the number of the scratch block AE_STK_<name> (parsed from the header's enum)"""
    import re
    text = (mh.ROOT / "src/monkeys/ae_monkey.h").read_text(encoding="utf-8")
    body = text[text.index("enum {"):]
    body = body[:body.index("};")]
    names = re.findall(r"^\s*AE_STK_(\w+)\s*(?:=\s*\d+\s*)?(?:,|/\*|$)", body, re.M)
    return names.index(name)


def stk_c(name, off=0):
    """address of byte `off` of scratch block AE_STK_<name> of the C port"""
    return 0x801FF000 + 0x40 * stk_index(name) + off


def set_stale(m, name, frame, off, value, size=4):
    """puts `value` into the never-written stack slot sp + off on both sides: the MIPS function (frame bytes, called at the default
    stack top) and the scratch block of the C port.  Every (block, offset) must be a shared_random row of the running checker"""
    reg.declare("shared_random", f"AE_STK_{name}+{off}")
    for a in (frame_slot(frame, off), stk_c(name, off)):
        if size == 4:
            m.write32(a, value & M32)
        elif size == 2:
            m.write16(a, value & 0xFFFF)
        else:
            m.write8(a, value & 0xFF)


def frame_slot(frame, off):
    """address of sp + off in the MIPS run for a function whose frame is `frame` bytes (called with the default stack top)"""
    return STK_TOP - frame + off


def mips_call(m, addr, args=(), hooks=None):
    """runs original code on the scenario image and returns $v0 (for pure helpers used to aim the scenarios); restores nothing,
    so callers pick functions without side effects on the scenario (rcos, the approach angles ...)"""
    regs = m.call(addr, args, hooks=hooks or {})
    return regs[2]


def cos_candidates(m):
    """the values s4 = rcos(angle) (and doubled) that MonkeyPickBestLink / ConsiderTurnBack compare the cosine table against"""
    vals = set()
    for ang in (0, 56):
        vals.add(s32(mips_call(m, 0x8002FC20, (ang,))))
    for f in (0x80037D6C, 0x80037DEC):
        ang = s32(mips_call(m, f))
        vals.add(s32(mips_call(m, 0x8002FC20, (ang & M32,))))
    out = set()
    for v in vals:
        out.add(v)
        out.add(s32((v << 1) & M32))
    return sorted(out)


def plain_nodes(m, nn):
    w = W(m)
    for k in range(nn):
        a = NAV_BASE + 0x18 * k
        w.w32(a, m.read32(a) & ~0x401 & M32)


def entry_world(m, rng, own=None):
    """combat world + the animation globals; the monkey is in the entered state with some probability (the early-outs)"""
    c = combat(m, rng)
    anim_globals(m, rng, c)
    w = W(m)
    w.w8(c + 0x78, own if (own is not None and rng.random() < 0.3) else rng.choice([rng.randint(0, 0x42), 0x0C, 0x24, 0x1A, 0x35, 0x1E, 0x20, 1, rng.getrandbits(8)]))
    w.w32(c + 0x248, rng.choice([rng.getrandbits(32), rng.getrandbits(32) | 0x200000, rng.getrandbits(32) & ~0x200000, 0, 0x200000]) & M32)
    w.w16(c + 0x242, rng.choice([0, 1, 0xFFFF, 0x7FFF, rng.getrandbits(16)]))
    w.w16(0x800F4472, rng.choice([0, 1, 3, 6, 8, 2, 4, 5, 7]))
    w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x46, 0x35, 0x24]))
    return c


def nav_entry_world(m, rng, own=None, plain=None):
    """navpos world (nodes with bones / references / plain) + animation globals + a random state"""
    c = navpos_world(m, rng)
    anim_globals(m, rng, c)
    w = W(m)
    nn = m.read16(0x800B4D18)
    w.w8(c + 0x78, own if (own is not None and rng.random() < 0.3) else rng.choice([rng.randint(0, 0x42), 0x0C, 0x24, 0x1A, 0x35, 0x1E, 0x20, 1, 9]))
    w.w16(c + 0xF2, rng.randint(0, max(0, nn - 1)))
    w.w16(c + 0xF4, rng.randint(0, max(0, nn - 1)))
    w.w8(c + 0x22E, rng.choice([0, 1, 2, 2, 3]))
    w.w16(0x800F4472, rng.choice([0, 1, 3, 6, 8, 2]))
    w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x46, 0x35, 0x24]))
    if plain if plain is not None else rng.random() < 0.3:
        plain_nodes(m, nn)
    return c


def goal_world(m, rng):
    """the world of MonkeyUpdateGoal (link slope + goal position)"""
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


def links_world(m, rng):
    """all flagged links of the graph get extra records with the flag combinations of MonkeyNavLinkAllowed (level 3 builder)"""
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


def set_link_counts(m, rng, nn, exact=0.85):
    """the byte at node + 0xB (number of usable links, what MarkBlockedLinks / PickBestLink / ... loop over) = the number of
    list entries with bit 14 set (exactly in `exact` of the cases, else a random small value)"""
    w = W(m)
    for k in range(nn):
        a = NAV_BASE + 0x18 * k
        cnt, lp = m.read8(a + 0xA), m.read32(a + 0xC)
        usable = sum(1 for j in range(cnt) if m.read16(lp + 2 * j) & 0x4000)
        w.w8(a + 0xB, usable if rng.random() < exact else rng.choice([0, 1, 2, 3, 4, 6, 8, rng.randint(0, 8)]))


def visit_counters(m, rng, c, nn, mode=None):
    """the per-monkey visit counters (byte at node + slot + 0x14)"""
    w = W(m)
    slot = m.read8(c + 0x7B)
    mode = mode or rng.choice(["sparse", "dense", "zero", "full", "ones"])
    for k in range(nn):
        a = NAV_BASE + 0x18 * k + slot + 0x14
        w.w8(a, {"sparse": rng.choice([0, 0, 0, 0, 1, 2, 3, 255]), "dense": rng.choice([0, 1, 2, 3, 254, 255, rng.getrandbits(8)]),
                 "zero": 0, "full": rng.choice([1, 2, 3, 255]), "ones": rng.choice([0, 1, 1, 2])}[mode])
    return mode


def draw_world(m, rng):
    """MonkeyDrawModel world (level 3 builder)"""
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
    if m.read16(0x800F4476) == 0x24 and rng.random() < 0.6:
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
        w.w16(0x800E78C0 + off, rng.getrandbits(16))
    return c


def lookup_world(m, rng, own_state):
    c = allowed_world(m, rng)
    ctx_c = c["cur"]
    anim_globals(m, rng, ctx_c)
    w = W(m)
    a, b = c["pair"]
    n0, n1 = rng.choice([(a, b), (b, a), (a, b), (rng.randint(0, 40), rng.randint(0, 40))])
    w.w16(ctx_c + 0xF2, n0)
    w.w16(ctx_c + 0xF4, n1)
    w.w8(ctx_c + 0x78, own_state if rng.random() < 0.4 else rng.choice([29, 30, 32, 0x1A, 0x35, 0x1E, 1, rng.randint(0, 0x42)]))
    w.w8(ctx_c + 0x22E, rng.choice([0, 1, 2, 2, 3]))
    w.w16(0x800F4472, rng.choice([0, 1, 3, 6, 8, 2]))
    w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x46]))
    w.w32(ctx_c + 0x248, rng.getrandbits(32))
    return ctx_c


# ---------------------------------------------------------------------------------------------------- level 5+ helpers
def frame_size(m, addr):
    """the frame size of the original function at `addr` (its first instruction is `addiu $sp, $sp, -N`)"""
    ins = m.read32(addr)
    assert ins >> 16 == 0x27BD, f"{addr:08X} does not start with addiu sp"
    return (-s16(ins & 0xFFFF)) & 0xFFFF


def set_stale_chain(m, name, callee_addr, above, off, value, size=4):
    """like set_stale for a callee that is not the function the harness calls: `above` = the summed frame sizes of the functions
    between the called function (its entry sp is the default stack top) and the callee"""
    reg.declare("shared_random", f"AE_STK_{name}+{off}")
    fr = frame_size(m, callee_addr)
    for a in (STK_TOP - above - fr + off, stk_c(name, off)):
        if size == 4:
            m.write32(a, value & M32)
        elif size == 2:
            m.write16(a, value & 0xFFFF)
        else:
            m.write8(a, value & 0xFF)


RELINK_BASE = 0x801D9000


def relink(m, rng, nn, a, b, flag_ab, flag_ba):
    """node `a` gets a link entry for node `b` (bit 14 = flag_ab) and vice versa, in freshly written sorted link lists (the old
    entries stay); returns nothing"""
    w = W(m)
    for src, dst, fl in ((a, b, flag_ab), (b, a, flag_ba)):
        n = NAV_BASE + 0x18 * src
        cnt, lp = m.read8(n + 0xA), m.read32(n + 0xC)
        ents = [m.read16(lp + 2 * j) for j in range(cnt)]
        ents = [e for e in ents if (e & 0x3FFF) != dst]
        ents.append(dst | (0x4000 if fl else 0))
        ents.sort(key=lambda e: e & 0x3FFF)
        base = RELINK_BASE + 0x80 * src
        for j, e in enumerate(ents):
            w.w16(base + 2 * j, e)
        w.w8(n + 0xA, len(ents))
        w.w32(n + 0xC, base)


class DotAim:
    """finds s16 vector pairs (x, 0, z) with DotXZ_SV == a target value, evaluated with the validated C port (batch 1)"""

    def __init__(self, h):
        import ctypes as C
        self.h = h
        self.C = C
        lib = h.c.lib
        lib.ae_DotXZ_SV.restype = C.c_int32
        lib.ae_DotXZ_SV.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32]
        self.cache = {}

    def dot(self, a, b):
        c = self.h.c
        for base, v in ((0x801FE000, a), (0x801FE010, b)):
            c.wr16(base, v[0] & 0xFFFF)
            c.wr16(base + 2, 0)
            c.wr16(base + 4, v[1] & 0xFFFF)
        return s32(self.h.c.lib.ae_DotXZ_SV(c.ram, 0x801FE000, 0x801FE010))

    def vector_for(self, rng, n, target, spread=(0, 1, -1)):
        """a vector t = (x, z) (|t| 300..8000) with dot(t, n) in target + spread, or None"""
        import math
        if not any(n):
            return None
        want = max(-4096, min(4096, target))
        ang_n = math.atan2(n[1], n[0])
        th = math.acos(want / 4096.0)
        best = None
        for _ in range(60):
            sgn = rng.choice([1, -1])
            d = th + sgn * rng.uniform(-0.004, 0.004)
            ln = rng.choice([300, 1000, 2000, 4000, 8000, rng.randint(300, 8000)])
            t = (round(ln * math.cos(ang_n + d)), round(ln * math.sin(ang_n + d)))
            dv = self.dot(t, n)
            if dv - target in spread:
                return t, dv
            if best is None or abs(dv - target) < abs(best[1] - target):
                best = (t, dv)
        return best


def stale_goalpos(m, rng, x_frame, via_goal=True):
    """random leftover stack contents in the frame that MonkeyUpdateLinkSlope and MonkeyUpdateGoalPos share (sp + 16 .. 47), on both
    sides, for a caller X (frame x_frame bytes, called by the harness; via_goal: through MonkeyUpdateGoal, frame 24)"""
    above = x_frame + (24 if via_goal else 0)
    for off in range(16, 48, 4):
        set_stale_chain(m, "LINK_SLOPE", 0x80035474, above, off, rng.getrandbits(32))


def pick_world(m, rng, i=0, boss=None):
    """MonkeyPickBestLink / ChooseNextNode world: links with extra records, link counts, visit counters, the cosine table around the
    thresholds, the previous node, alerted / state / area"""
    c = links_world(m, rng)
    anim_globals(m, rng, c)
    w = W(m)
    nn = m.read16(0x800B4D18)
    set_link_counts(m, rng, nn)
    node0 = rng.randint(0, nn - 1)
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
    visit_counters(m, rng, c, nn)
    if rng.random() < 0.35:                      # the current node's own counter equal to the best neighbour's (ties)
        slot = m.read8(c + 0x7B)
        bestc = max([m.read8(NAV_BASE + 0x18 * q + slot + 0x14) for q in links] or [0])
        w.w8(NAV_BASE + 0x18 * node0 + slot + 0x14, (bestc + rng.choice([0, 0, 0, 1, -1])) & 0xFF)
    w.w16(0x800F4472, rng.choice([0, 1, 2]) if boss is None else boss)
    if boss is None and rng.random() < 0.12:
        w.w16(0x800F4472, rng.choice([3, 6, 8]))
    w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x43, 0x35, 0x46]))
    cand = cos_candidates(m)
    for j in range(52):
        v = rng.choice(cand) + rng.choice([0, 0, 0, 1, -1, 2, -2])
        if rng.random() < 0.3:
            v = rng.choice([-4096, 4096, 0, rng.randint(-4096, 4096), rng.getrandbits(16)])
        w.w16(c + 0x176 + 2 * j, v & 0xFFFF)
    return c


def no_stale_goal(m, rng, c, nn):
    """makes MonkeyUpdateGoalPos always copy its position buffer (flags3 byte 0x24B bit 0, no node with flag bit 6), i.e. it never
    reads the leftover stack words that depend on the depth of the call (several invocations at different depths in one run)"""
    w = W(m)
    w.w8(c + 0x24B, m.read8(c + 0x24B) | 1)
    for k in range(nn):
        a = NAV_BASE + 0x18 * k
        w.w32(a, m.read32(a) & ~0x40 & M32)


def stale_enter(m, rng, above):
    """`above`: an int or a list of ints (one per possible call path; the same word is written at every depth)"""
    if isinstance(above, (list, tuple)):
        v = rng.choice([rng.getrandbits(32), 0, 0xFFFF, 0x7FFF8000])
        v09 = rng.choice([rng.getrandbits(32), 0, 0xFFFF, 0x7FFF8000])
        for ab in above:
            for name, addr, off, val in (("ENTER_1D", 0x800359F0, 16, v), ("ENTER_1E", 0x8003581C, 32, v), ("ENTER_09", 0x8003601C, 16, v09)):
                set_stale_chain(m, name, addr, ab, off, val)
        return
    _stale_enter_one(m, rng, above)


def _stale_enter_one(m, rng, above):
    """leftover words of the stack slots that EnterState_1D / _1E / _09 read when the link has no extra record (see
    check_monkey_level5.py): `above` = the summed frame sizes of the functions between the harness-called function and the entered
    state function's caller; 1D's sp + 16 and 1E's sp + 32 are the same stack word"""
    v = rng.choice([rng.getrandbits(32), 0, 0xFFFF, 0x7FFF8000])
    for name, addr, off, val in (("ENTER_1D", 0x800359F0, 16, v), ("ENTER_1E", 0x8003581C, 32, v),
                                 ("ENTER_09", 0x8003601C, 16, rng.choice([rng.getrandbits(32), 0, 0xFFFF, 0x7FFF8000]))):
        set_stale_chain(m, name, addr, above, off, val)


def attack_fields(m, rng, c):
    """the MonkeyInfo attack records / counters / distance / attack table of attack_world applied to an existing world"""
    w = W(m)
    info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
    dist = rng.choice([0, 100, 500, 1000, 1500, 2000, 3000, rng.randint(0, 4000), rng.getrandbits(32)])
    w.w32(c + 0x20C, dist & M32)
    for k in range(8):
        e = info + 8 * k
        lo = rng.choice([0, 100, 500, rng.randint(-100, 2000), rng.getrandbits(16)])
        hi = rng.choice([lo, lo + 1, lo + rng.randint(0, 3000), 5000, rng.getrandbits(16), -1])
        w.w16(e + 0x1E, rng.choice([0, 1, 2, 3, 5, 0xFF, 0x100, rng.getrandbits(16)]) if rng.random() < 0.85 else 0)
        w.w16(e + 0x20, rng.choice([100, 100, 100, 0, 1, 50, 99, 101, 0xFFFF, rng.randint(0, 150), rng.getrandbits(16)]))
        w.w16(e + 0x22, lo & 0xFFFF)
        w.w16(e + 0x24, hi & 0xFFFF)
        if rng.random() < 0.5 and dist < 0x7FFF:
            w.w16(e + 0x22, dist - rng.choice([0, 0, 1, 10]))
            w.w16(e + 0x24, dist + rng.choice([0, 0, 1, 10]))
    w.w16(info + 0x58, rng.choice([0, 1, 50, 200, rng.getrandbits(16)]))
    w.w8(c + 0xD4, rng.choice([0, 0, 1, 2, 0x80, 0xFF, 0x7F, rng.getrandbits(8)]))
    w.w16(c + 0x174, rng.choice([0, 0, 1, 90, rng.getrandbits(16)]))
    if rng.random() < 0.6:
        for k in range(8):
            w.w8(0x800AEBB4 + 8 * k, rng.choice([1, 2, 3, 5, 0, rng.getrandbits(8)]))
            w.w8(0x800AEBB5 + 8 * k, rng.choice([rng.randint(0, 0x42), 0x1E, 0x20, rng.getrandbits(8)]))
            w.w32(0x800AEBB8 + 8 * k, rng.choice([0, 1, 2, 4, 7, 3, 5, 6, rng.getrandbits(32)]))
    w.w16(0x800EC2CA, rng.choice([0, 0, 0, 0, 1, -1, 0x100, rng.getrandbits(16)]) & 0xFFFF)


def seed_for(rng, pred):
    """a MonkeyRand seed whose NEXT value (((seed * 5 + 1) & 0xFFFF) >> 8) satisfies pred(value)"""
    while True:
        seed = rng.getrandbits(16)
        val = ((seed * 5 + 1) & 0xFFFF) >> 8
        if pred(val):
            return seed


def los_fields(m, rng, c, flag=None):
    """the fields of monkey_scen.los_world applied to an existing world (positions of the monkey / the target, the target svec read by
    CopySvecAt1C, distance, frame counter, camera matrix)"""
    import math
    w = W(m)
    tgt = m.read32(G_TARGET)
    flag = rng.choice([0, 1]) if flag is None else flag
    w.w8(0x800B4D04, flag)
    cx, cy, cz = (rng.randint(-8000, 8000) for _ in range(3))
    L = rng.choice([300, 800, 1500, 4000, 8000, 20000])
    az = rng.uniform(0, 2 * math.pi)
    d = [int(L * math.cos(az)), rng.choice([0, 0, 50, -50, 124, 125, 126, -25, -225, -24, -26, -224, -226, rng.randint(-300, 300), rng.randint(-3000, 3000)]),
         int(L * math.sin(az))]
    if rng.random() < 0.15:
        d[0] = 0
    if rng.random() < 0.15:
        d[2] = 0
    if rng.random() < 0.15:
        d = [rng.randint(-3, 3), rng.randint(-3, 3), rng.randint(-3, 3)]
    tx, ty, tz = cx + d[0], cy + d[1], cz + d[2]
    for k, v in enumerate((cx, cy, cz)):
        w.w32(c + 0x24 + 4 * k, v & M32)
    for k, v in enumerate((tx, ty, tz)):
        w.w16(tgt + 0x1C + 2 * k, v & 0xFFFF)
        w.w32(tgt + 0x24 + 4 * k, v & M32 if rng.random() < 0.9 else rng.getrandbits(32))
    w.w16(tgt + 0x1C + 6, rng.getrandbits(16))
    if rng.random() < 0.3:
        w.w32(c + 0x20C, rng.choice([249, 250, 251, 0, -1, rng.randint(0, 600)]) & M32)
    else:
        w.w32(c + 0x20C, rng.randint(250, 9000))
    w.w32(FRAME, rng.getrandbits(32))
    w.w32(c + 0x248, rng.choice([0, 1 << 28, rng.getrandbits(32), 0xFFFFFFFF]))
    for off in range(0, 0x20, 2):
        w.w16(0x800E78C0 + off, rng.getrandbits(16))


def route_fields(m, rng, c, nn):
    """the patrol route fields MonkeyPickGoalNode reads (check_monkey_level1.py goal_setup) applied to an existing world"""
    w = W(m)
    routes = rng.choice([0, 0, 1, 2, 3, 4, 8, 255])
    w.w8(c + 0xD7, routes)
    w.w8(c + 0xD8, rng.choice([rng.randint(0, max(0, routes - 1)), rng.randint(0, 8), rng.getrandbits(8), 0, 1]))
    lens = [rng.choice([0, 1, 2, 3, 4, 5, 8, 16, 33, 255, rng.randint(0, 16)]) for _ in range(8)]
    for k, v in enumerate(lens):
        w.w8(c + 0xDA + k, v)
    for k in range(8, 16):
        w.w8(c + 0xDA + k, rng.getrandbits(8))
    route = m.read8(c + 0xD8)
    ln = m.read8(c + 0xDA + route)
    w.w8(c + 0xD9, rng.choice([0, 1, max(0, ln - 1), ln, ln + 1, rng.getrandbits(8)]))
    w.w16(c + 0xE8, rng.choice([0, 0xFFFF, 0x7FFF, rng.getrandbits(16), (1 << min(ln, 15)) - 1, rng.getrandbits(16) & ((1 << min(ln, 15)) - 1),
                                0x8000, 1]))
    w.w32(0x800B4D0C, sc.ROUTE_BASE)
    data = bytearray()
    for k in range(0x1000):
        data += struct.pack("<H", rng.randint(0, max(0, nn - 1)) if rng.random() < 0.97 else rng.getrandbits(16))
    m.write_bytes(sc.ROUTE_BASE + 0x88, bytes(data[:0x1000]))
    w.w8(0x800AEB59, rng.choice([0, 1]))


def seed_for2(rng, pred1, pred2):
    """a MonkeyRand seed whose next value satisfies pred1 and the value after that pred2"""
    while True:
        seed = rng.getrandbits(16)
        s1 = (seed * 5 + 1) & 0xFFFF
        s2 = (s1 * 5 + 1) & 0xFFFF
        if pred1(s1 >> 8) and pred2(s2 >> 8):
            return seed


def seed_for_seq(rng, preds):
    """a MonkeyRand seed whose next len(preds) values satisfy preds[0], preds[1], ..."""
    while True:
        seed = rng.getrandbits(16)
        s = seed
        ok = True
        for pr in preds:
            s = (s * 5 + 1) & 0xFFFF
            if not pr(s >> 8):
                ok = False
                break
        if ok:
            return seed
