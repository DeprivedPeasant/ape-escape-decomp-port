"""Shared RAM scenario builders for the monkey check scripts of batches 2 and later (check_monkey_level*.py).  Not a check
itself.  Every builder takes the MIPS-side RAM (`m`) and a `random.Random` and lays out monkeys, nav nodes, the MonkeyInfo
table, animation tables, objects / bones for NavNodeGetPos, the extra link records, ... the way MonkeySpawnAll and the
level loader leave them, plus randomised and edge-case values."""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import monkey_harness as mh  # noqa: E402
from monkey_harness import W, build_monkey_world, MONKEYS, MONKEY_SIZE, G_CUR, NAV_BASE, LINK_BASE, INFO_BASE  # noqa: E402
import check_bodies as cb  # noqa: E402

M32 = 0xFFFFFFFF
s32, s16 = mh.s32, mh.s16
STATE_TABLE = 0x800AE940
FRAME = 0x800AEB60
QUEUE = 0x800E6780
ANIM_BASE = 0x801C4000
ROUTE_BASE = 0x801C0000
EXTRA_BASE = 0x801C8000
GROUND_REF = 0x801C9000
BONES = 0x801C0000
VEC_OUT = 0x801F0300
TARGETS = [0x800EC1E0, 0x800F5A20, 0x800E6A00]
G_TARGET = 0x800B4CF8


def u16(v): return v & 0xFFFF


def anim_index(m, cur):
    """what MonkeyGetAnimIndex returns for the monkey (python replica, only used to pick interesting frame values)"""
    st = m.read8(cur + 0x78)
    if st in (3, 8):
        info = m.read32(0x800B4CF0) + m.read8(cur + 0x7C) * 0x88
        if m.reads16(info + 4) == 0:
            return 0
    if 50 <= st <= 52:
        return m.reads16(cur + 0x242)
    return m.read32(STATE_TABLE + st * 8)


def anim_tables(m, rng, c):
    """a 128 entry {first, last, x, x} table for the current monkey and a frame near an interesting value of it"""
    w = W(m)
    m.write32(c + 0x16C, ANIM_BASE)
    data = bytearray()
    for e in range(128):
        first = rng.choice([0, 1, 5, rng.randint(0, 300), rng.randint(0, 300), 0xFFFF, 0xFFFE, rng.getrandbits(16)])
        last = rng.choice([first, first + 1, first + rng.randint(0, 60), first + rng.randint(0, 60), rng.randint(0, 400),
                           0xFFFF, rng.getrandbits(16)]) & 0xFFFF
        data += struct.pack("<HHHH", first & 0xFFFF, last, rng.getrandbits(16), rng.getrandbits(16))
    m.write_bytes(ANIM_BASE, bytes(data))
    w.w16(c + 0x242, rng.choice([0, 1, 5, 20, 44, rng.randint(0, 100), rng.getrandbits(16)]))
    idx = anim_index(m, c)
    first = m.read16(ANIM_BASE + 8 * idx) if 0 <= idx < 128 else 0
    last = m.read16(ANIM_BASE + 8 * idx + 2) if 0 <= idx < 128 else 0
    w.w16(c + 0x5C, rng.choice([last, last + 1, last - 1, last + 2, first, first + 1, first - 1, rng.getrandbits(16), 0xFFFF,
                                0xFFFE, rng.randint(0, 400)]) & 0xFFFF)
    st = m.read8(c + 0x78)
    if rng.random() < 0.5:
        w.w32(STATE_TABLE + 8 * (st % 0x43) + 4, rng.getrandbits(32))
    base = rng.choice([0, 1, 5, 30, 0x7FFF, 0x8000, 0xFFFF, rng.getrandbits(16)])
    w.w16(c + 0x128, base)
    w.w16(c + 0x96, rng.choice([base, base + 1, base - 1, base + 2, rng.getrandbits(16)]) & 0xFFFF)


def place(m, rng, c, spread=None):
    """current monkey and target positions; the target a distance away"""
    w = W(m)
    tgt = m.read32(G_TARGET)
    spread = spread or rng.choice([300, 1500, 3000, 12000])
    cp = [rng.randint(-spread, spread) for _ in range(3)]
    for k in range(3):
        w.w32(c + 0x24 + 4 * k, cp[k] & M32)
    mode = rng.random()
    for k in range(3):
        if mode < 0.8:
            v = cp[k] + rng.choice([0, 0, 1, -1, rng.randint(-spread, spread), rng.randint(-100, 100), rng.randint(-spread // 3, spread // 3)])
        elif mode < 0.9:
            v = rng.getrandbits(32)
        else:
            v = rng.choice([0x7FFFFFFF, -0x80000000, 0x8000, -0x8000, 0xFFFF, 0x10000])
        w.w32(tgt + 0x24 + 4 * k, v & M32)
    w.w32(tgt + 0x70, rng.getrandbits(32) if rng.random() < 0.5 else rng.choice([0, 1, 2, 3, 0x10, 0x11]))
    return tgt


def voice_queue(m, rng):
    w = W(m)
    mode = rng.choice(["free", "free", "free", "full", "mixed", "onefree"])
    free_at = rng.randint(0, 15)
    for k in range(16):
        e = QUEUE + 20 * k
        t = {"onefree": -1 if k == free_at else rng.choice([0, 1, 2, 5, 0x7FFF]), "free": -1,
             "full": rng.choice([0, 1, 2, 5, 0x7FFF]), "mixed": rng.choice([-1, 0, 1, 2, 5, 0x7FFF, rng.getrandbits(16)])}[mode]
        w.w16(e + 4, t)
        w.w32(e, rng.choice([-1, 0, 1, 0x602D, rng.getrandbits(32)]))
        for off in (8, 12, 16):
            w.w32(e + off, rng.getrandbits(32))


def combat(m, rng, state=None, wild=None, positions=True):
    """world + animation tables + positions + voice queue + counters"""
    c = build_monkey_world(m, rng, wild=wild)
    w = W(m)
    if state is not None:
        w.w8(c + 0x78, state)
    if positions:
        place(m, rng, c)
    anim_tables(m, rng, c)
    voice_queue(m, rng)
    w.w32(FRAME, rng.choice([rng.getrandbits(32), rng.getrandbits(16), 0, 4, 5, 20, 60, 0xFFFFFFFF, rng.randint(0, 1000) * 20,
                             rng.randint(0, 1000) * 4, rng.randint(0, 1000) * 5, rng.randint(0, 100000)]))
    w.w16(c + 0x96, rng.choice([0, 1, 0x7FFF, 0xFFFF, rng.getrandbits(16)]))
    w.w8(c + 0x222, rng.choice([0, 1, 2, 255, rng.getrandbits(8)]))
    w.w8(0x800AEBF4, rng.getrandbits(8))
    return c


def navpos_extras(m, rng, nn):
    """objects / bones / matrices / the moving reference for the three kinds of nodes NavNodeGetPos knows (bit 10 = bone of an
    object, bit 0 = reference + offset, else plain); rewrites the node flags and link fields accordingly"""
    w = W(m)
    w.w32(0x800F455C, GROUND_REF)
    w.rand_fill(rng, GROUND_REF - 0x400, 0x1000)
    for k in range(16):
        o = mh.OBJS + 0xE4 * k
        w.rand_fill(rng, o, 0xE4)
        w.w32(o + 0xE0, rng.choice([0x10, 0, 0x10, 0x11, rng.getrandbits(32)]))
        w.w32(o + 0x10, BONES + rng.choice([0, 8, 0x100]))
        w.w32(o + 0x14, rng.getrandbits(32))
    w.rand_fill(rng, BONES, 0x400)
    for j in range(0, 0x800, 0x20):
        for q in range(0, 0x20, 2):
            w.w16(0x800F4750 + j + q, rng.choice([0x1000, 0, -0x1000 & 0xFFFF, 0x0B50, 0x7FFF, 0x8000, rng.getrandbits(16)]))
    for k in range(nn):
        a = NAV_BASE + 0x18 * k
        r_ = rng.random()
        flags = (0x400 if r_ < 0.2 else (0x1 if r_ < 0.35 else 0))
        w.w32(a, flags)
        obj = rng.choice([rng.randint(0, 15), rng.randint(0, 15), rng.randint(-2, 20)])
        mat = rng.choice([rng.randint(0, 99), rng.randint(0, 99), rng.randint(0, 5)])
        w.w16(a + 0x10, (obj * 100 + mat) & 0xFFFF if flags & 0x400 else rng.choice([rng.randint(-3, 20), rng.randint(-3, 20), rng.getrandbits(16)]))
        w.w16(a + 0x12, rng.choice([rng.randint(0, 40), rng.randint(0, 40), rng.getrandbits(16)]))
        for off in (4, 6, 8):
            w.w16(a + off, rng.choice([rng.getrandbits(16), rng.randint(-2000, 2000) & 0xFFFF, rng.randint(-300, 300) & 0xFFFF]))
    for j in range(0, 0x10, 2):
        w.w16(0x800AEB6C + j, rng.getrandbits(16))


def navpos_world(m, rng, state=None, positions=True):
    """combat world + the NavNodeGetPos extras; the nodes are mostly plain"""
    c = combat(m, rng, state=state, positions=positions)
    nn = m.read16(0x800B4D18)
    navpos_extras(m, rng, nn)
    return c


def node_pos_py(m, node):
    """plain node position (s16 triple at +4 / +6 / +8) or None when the node is not plain (python replica for scenario aiming)"""
    a = NAV_BASE + 0x18 * node
    if m.read32(a) & 0x401:
        return None
    return [m.reads16(a + 4), m.reads16(a + 6), m.reads16(a + 8)]


def body_world(m, rng):
    """collision body world (the body ports register / unregister the monkey) + monkeys; returns the current monkey"""
    k = rng.choice([0, 1, 3, 10, 60, 200, 254, 255, 256])
    cb.build_world(m, rng, n=min(k, 256), full=(k == 256) and rng.random() < 0.7)
    c = build_monkey_world(m, rng)
    w = W(m)
    flags = m.read32(c + 0x24C)
    registered = rng.random() < 0.6
    m.write32(c + 0x24C, (flags | 2) if registered else (flags & ~2))
    w.w32(c + 0x248, rng.choice([0, rng.getrandbits(32), 0xFFFFFFFF, 0x40000000, 0x30000, 0xBFFFFFFF]))
    if registered and rng.random() < 0.85 and k < 256:
        hooks = {0x800A3720: lambda cpu, r: 0, 0x800878E0: lambda cpu, r: 0}
        m.call(0x80071E44, (3, c + 0x24, c), hooks=hooks)
    return c


def link_pair(m, rng):
    """a (from, to) pair for which NavNodeLinkFlag is non-zero, or None"""
    nn = m.read16(0x800B4D18)
    cands = []
    for k in range(nn):
        a = NAV_BASE + 0x18 * k
        cnt = m.read8(a + 0xA)
        lp = m.read32(a + 0xC)
        for j in range(cnt):
            e = m.read16(lp + 2 * j)
            if e & 0x4000:
                cands.append((k, e & 0x3FFF))
    return rng.choice(cands) if cands else None


def extra_records(m, rng, pair, flag_choices):
    """the sorted extra link records ({s16 lo, s16 hi, u32 flags, u32 value}, 12 bytes) with one for `pair` most of the time"""
    w = W(m)
    pr = pair if pair else (rng.randint(0, 40), rng.randint(0, 40))
    lo, hi = min(pr), max(pr)
    ents = [(rng.randint(0, 40), rng.randint(0, 40)) for _ in range(rng.choice([0, 1, 2, 3, 6, 10]))]
    ents = [(min(a, b), max(a, b)) for a, b in ents]
    if rng.random() < 0.85:
        ents.append((lo, hi))
    ents = sorted(set(ents))
    data = bytearray()
    for a, b in ents:
        data += struct.pack("<hhII", a, b, rng.choice(flag_choices), rng.getrandbits(32))
    m.write_bytes(EXTRA_BASE, bytes(data) + bytes(16))
    w.w32(0x800B4D1C, EXTRA_BASE)
    w.w16(0x800B4D20, len(ents))
    return pr


def los_world(m, rng, flag=None):
    """MonkeyLineOfSight / MonkeyUpdateSight scenario: positions, the target svec read by CopySvecAt1C, distance, frame counter"""
    import math
    c = combat(m, rng, positions=False)
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
    w.w8(c + 0x7C, rng.randint(0, 15))
    for off in range(0, 0x20, 2):
        w.w16(0x800E78C0 + off, rng.getrandbits(16))
    return {"ty": s16(ty & 0xFFFF)}


HDR = 0x801C2000
FLAG_BITS = [0x40, 0x80, 0x400, 0x1000, 0x200000, 0x400000, 0x40000]


def anim_globals(m, rng, c=None):
    """the globals MonkeySetState reads (animation sets, the area header with the two animation table offsets, the area data base)
    and a valid animation table at ANIM_BASE; with `c` the current monkey also gets one of the two sets"""
    w = W(m)
    w.w32(0x800B4D14, HDR)
    w.w32(HDR + 0x18, rng.choice([0, 0x100, 0x200, 0x7F8]))
    w.w32(HDR + 0x1C, rng.choice([0, 0x100, 0x200, 0x3F0]))
    w.w32(0x800F4590, ANIM_BASE)
    s1, s2 = rng.choice([0x800AE000, 0x801C3000, 0x1234, 0]), rng.choice([0x800AF000, 0x801C3100, 0xABCD, 0])
    w.w32(0x800F4504, s1)
    w.w32(0x800F4508, s2)
    if c is not None:
        w.w32(c + 0x18, rng.choice([s1, s2, s1, rng.getrandbits(32)]))
    for k in range(8):
        a = MONKEYS + MONKEY_SIZE * k
        w.w32(a + 0x16C, ANIM_BASE)
        w.w32(a + 0x18, rng.choice([s1, s2, rng.getrandbits(32)]))
    # a valid animation table, as anim_tables does (the entries only need to exist)
    data = bytearray()
    for e in range(128):
        first = rng.choice([0, 1, 5, rng.randint(0, 300)])
        data += struct.pack("<HHHH", first, first + rng.randint(0, 60), rng.getrandbits(16), rng.getrandbits(16))
    m.write_bytes(ANIM_BASE, bytes(data))


def attack_world(m, rng):
    """world for MonkeyTryAttackKind and the pickers: ranges / counts / chances of the 8 attack kinds in the MonkeyInfo record
    of the current monkey, the distance, the attack table (EXE data) and the random seed"""
    c = combat(m, rng)
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
    w.w8(c + 0x78, rng.choice([rng.randint(0, 0x42), 0x1E, 0x1F, 0x20, 3, 8, rng.getrandbits(8)]))
    if rng.random() < 0.6:
        for k in range(8):
            w.w8(0x800AEBB4 + 8 * k, rng.choice([1, 2, 3, 5, 0, rng.getrandbits(8)]))
            w.w8(0x800AEBB5 + 8 * k, rng.choice([rng.randint(0, 0x42), 0x1E, 0x20, rng.getrandbits(8)]))
            w.w32(0x800AEBB8 + 8 * k, rng.choice([0, 1, 2, 4, 7, 3, 5, 6, rng.getrandbits(32)]))
    w.w32(0x800B4D10, rng.getrandbits(16) if rng.random() < 0.9 else rng.getrandbits(32))
    anim_globals(m, rng, c)
    return c


def allowed_world(m, rng):
    """navigation world for MonkeyNavLinkAllowed: node flags, extra link records with the flag combinations of the function,
    values (gimmick / distance / monkey id / object slot) and the monkey fields it compares; returns the context"""
    c = navpos_world(m, rng)
    w = W(m)
    nn = m.read16(0x800B4D18)
    pair = link_pair(m, rng)
    if pair is None and nn > 1:
        pair = (rng.randint(0, nn - 1), rng.randint(0, nn - 1))
    flagsel = [sum(b for b in FLAG_BITS if rng.random() < 0.3) | rng.choice([0, 0, 0x2000]) for _ in range(8)] + list(FLAG_BITS) + [0]
    pr = extra_records(m, rng, pair, flagsel)
    data = bytearray(m.read_bytes(EXTRA_BASE, 12 * m.read16(0x800B4D20)))
    for e in range(m.read16(0x800B4D20)):
        val = rng.choice([rng.randint(0, 20), rng.randint(0, 3000), rng.randint(0, 15), rng.getrandbits(16), rng.getrandbits(32)])
        data[12 * e + 8:12 * e + 12] = struct.pack("<I", val & M32)
    m.write_bytes(EXTRA_BASE, bytes(data) + bytes(16))
    w.w32(0x800B4D1C, EXTRA_BASE)
    w.w8(c + 0x7C, rng.choice([rng.randint(0, 15), rng.randint(0, 15), 3]))
    w.w8(c + 0x79, rng.choice([0, 1, 1, 2, rng.getrandbits(8)]))
    to = pr[1]
    if to < nn:
        a = NAV_BASE + 0x18 * to
        if rng.random() < 0.5:
            w.w32(a, m.read32(a) | 0x100)
        if rng.random() < 0.5:
            w.w16(c + 0xE6, to)
    return {"pair": pr, "cur": c}


def static_models(m, rng):
    """valid model pointers for the StaticModelDraw objects MonkeyDrawModel draws (0x800F4630 + 16 * i, 0x800EC180, 0x800EC0C0,
    0x800F46B0) and the draw context index"""
    w = W(m)
    objs = [0x800F4630 + 16 * i for i in range(0, 80)] + [0x800EC180, 0x800EC0C0, 0x800F46B0]
    for o in objs:
        w.w32(o, 0x801C9000 if rng.random() < 0.9 else 0x801C9100)
    w.w16(0x801C9004, rng.choice([0, 1, 5, 0x1FF, rng.getrandbits(16)]))
    w.w16(0x801C9104, rng.choice([0, 1, 5, 0x1FF, rng.getrandbits(16)]))
    w.w32(0x800EC35C, rng.choice([0, 1, 2, 3, 7, rng.getrandbits(32)]))
