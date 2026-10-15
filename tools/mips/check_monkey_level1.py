"""Differential check of monkey port batch 2 (src/monkeys/ae_monkey_level1.c, call-graph level 1; the SDK additions rsin /
rcos / RotMatrixYXZ of src/core/ae_sdk.c) against the original MIPS code: for every ported function many scenarios are built in
a byte-exact RAM image (SCUS_944.23 at its real addresses; monkeys / nav nodes / MonkeyInfo / animation tables / globals laid
out as MonkeySpawnAll leaves them, plus randomised and edge-case states); the original instructions (GTE through the
PCSX-ReARMed oracle) and the C port run on identical images with identical hook behaviour; return value, RAM outside the
stack window, scratchpad and the sequence of hook calls must be identical.  The batch 1 functions the code calls run as the
C ports on the C side and as the original instructions on the MIPS side (so this also checks them in context).  Needs
extract/disc/SCUS_944.23 (skips if missing).  See tools/mips/monkey_harness.py and check_monkey_base.py.

Usage: uv run python tools/mips/check_monkey_level1.py [--cases PERCENT] [--seed S] [--coverage] [--trapv] [--mutate]
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
from monkey_harness import W, build_monkey_world, MONKEYS, MONKEY_SIZE, G_CUR, G_NAV, NAV_BASE, LINK_BASE, INFO_BASE  # noqa: E402
import check_bodies as cb  # noqa: E402

# name -> (original address, size, return kind: s = int32 / u = uint32 / v = void, takes hooks, argument kinds[, C name])
FUNCS = {
    "MonkeyRandomHesitate": (0x80032F24, 220, "s", False, ""),
    "MonkeyAdvanceAnimFrame": (0x800330C8, 180, "v", False, ""),
    "MonkeyPickAnim": (0x800331B0, 112, "v", False, ""),
    "MonkeyAnimFinished": (0x80033A3C, 144, "s", False, ""),
    "MonkeyAnimAtStart": (0x80033ACC, 68, "s", False, ""),
    "MonkeyFireProjectile6": (0x80033DF0, 92, "v", True, ""),
    "MonkeyAimAtTarget": (0x80033E4C, 1104, "v", True, "s"),
    "MonkeyFireProjectileAimed": (0x8003429C, 276, "v", True, "s"),
    "MonkeyFireProjectile7": (0x800343B0, 92, "v", True, ""),
    "MonkeyFireProjectile8": (0x8003440C, 188, "v", True, ""),
    "MonkeyAngleToTarget": (0x8003455C, 120, "s", False, ""),
    "MonkeyApplyMoveDir": (0x80034834, 152, "v", False, ""),
    "NavNodeGetPos": (0x80034EF8, 392, "v", True, "su"),
    "MonkeyTargetInSightRange": (0x800361B4, 332, "s", False, ""),
    "MonkeyBodyOff": (0x80036B24, 84, "v", True, ""),
    "MonkeyBodyOn": (0x80036B78, 88, "v", True, ""),
    "MonkeyDefeatBurst": (0x80036C84, 76, "v", True, ""),
    "MonkeyFootDust": (0x80038F00, 216, "v", True, ""),
    "MonkeyDebrisBurst6": (0x80038FD8, 288, "v", True, ""),
    "MonkeyDebrisBurst1": (0x800390F8, 264, "v", True, ""),
    "MonkeyAlertPuff": (0x80039248, 84, "v", True, ""),
    "MonkeyUpdateContact": (0x800392F0, 244, "s", False, ""),
    "MonkeyProjectToScreen": (0x800393E4, 332, "s", True, ""),
    "MonkeyAlertIconKind": (0x80039B90, 168, "s", False, ""),
    "MonkeySelectLod": (0x8003A33C, 216, "v", True, ""),
    "MonkeyPickGoalNode": (0x8003AD9C, 728, "v", True, ""),
    "MonkeyLoadLinkExtra": (0x8003B674, 524, "v", False, "ss"),
    "NavLinkLookupExtra": (0x8003B880, 320, "s", False, "ssu"),
    "MonkeyMoveStep": (0x8003B9C0, 2116, "v", True, ""),
    "MonkeyTurnTowardTarget": (0x8003CC3C, 232, "v", False, "s"),
    "MonkeyLineOfSight": (0x800401F4, 976, "s", True, "s"),
}
# SDK maths (src/core/ae_sdk.c), also diffed directly
FUNCS.update({
    "rsin (SDK)": (0x8002FB50, 200, "s", False, "s", "ae_sdk_rsin"),
    "rcos (SDK)": (0x8002FC20, 160, "s", False, "s", "ae_sdk_rcos"),
    "RotMatrixYXZ (SDK)": (0x80030FA0, 0x28C, "u", False, "uu", "ae_sdk_RotMatrixYXZ"),
})
# code reached through the functions above (for the coverage report)
EXTRA_COVERAGE = {
    "CopySvecAt1C (ported inline)": (0x800154DC, 0x28),
    "normalize core (SDK)": (0x80030210, 0xC0), "Square0 (SDK)": (0x80014498, 0x28),
    "SquareRoot0 (SDK)": (0x800303C0, 0x84), "ratan2 (SDK)": (0x800145D0, 0xB4), "ApplyMatrixSV (SDK)": (0x800300B0, 0x5C),
    "VectorNormalSS (SDK)": (0x800301E0, 0x30),
    "MonkeyRand (batch 1)": (0x80032E30, 40), "MonkeyGetAnimIndex (batch 1)": (0x80032E84, 160),
    "MonkeyQueueVoice (batch 1)": (0x80033744, 128), "MonkeySpawnProjectile (batch 1)": (0x80033CCC, 292),
    "AngleBetweenXZ (batch 1)": (0x800344C8, 148), "MonkeyCalcMoveDir (batch 1)": (0x800347D8, 92),
    "DistanceIntToShort3D (batch 1)": (0x800345D4, 176), "Atan2Angle4096 (batch 1)": (0x800349B4, 64),
    "NavNodeGetLinkIds (batch 1)": (0x80034B74, 148), "ObjGetBonePos (batch 1)": (0x80034D30, 268),
    "NavNodeLinkFlag (batch 1)": (0x80038608, 140), "MonkeyResetNavVisits (batch 1)": (0x800407F0, 124),
    "MonkeyBodyRegister (batch 1)": (0x80033008, 96), "MonkeyBodyUnregister (batch 1)": (0x80033068, 96),
    "MonkeyPuff5 (batch 1)": (0x8003929C, 72), "AreaIsBossArena (batch 1)": (0x800339EC, 80),
    "MonkeyIsIdleActivityState (batch 1)": (0x80033B10, 140), "MonkeyIsState0C_or_24 (batch 1)": (0x80033C00, 52),
    "MonkeyIsJumpArcState (batch 1)": (0x80033C34, 76), "MonkeyIsAltAnimState (batch 1)": (0x80033BE0, 32),
    "MonkeyNop (batch 1)": (0x80033000, 8),
}

QUEUE = 0x800E6780
ANIM_BASE = 0x801C4000
ROUTE_BASE = 0x801C0000
EXTRA_BASE = 0x801C8000
GROUND_REF = 0x801C9000
BONES = 0x801C0000
VEC_OUT = 0x801F0300
M32 = 0xFFFFFFFF
s32, s16 = mh.s32, mh.s16
STATE_TABLE = 0x800AE940
FRAME = 0x800AEB60
TARGETS = [0x800EC1E0, 0x800F5A20, 0x800E6A00]


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


def check_all(h, scale, seed):
    t0 = time.time()

    def n(count):
        return max(1, int(count * scale / 100))

    def rng_of(name, i):
        return random.Random(f"{seed}/{name}/{i}")

    def cur(m): return m.read32(G_CUR)

    # ------------------------------------------------------------------ scenario pieces
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
        # the state table (writable) decides which of the two compare modes MonkeyAnimFinished uses
        st = m.read8(c + 0x78)
        if rng.random() < 0.5:
            w.w32(STATE_TABLE + 8 * (st % 0x43) + 4, rng.getrandbits(32))
        base = rng.choice([0, 1, 5, 30, 0x7FFF, 0x8000, 0xFFFF, rng.getrandbits(16)])
        w.w16(c + 0x128, base)
        w.w16(c + 0x96, rng.choice([base, base + 1, base - 1, base + 2, rng.getrandbits(16)]) & 0xFFFF)

    def place(m, rng, c, spread=None):
        """current monkey and target positions; the target a distance away (also used for the targets reading 32 bit values)"""
        w = W(m)
        tgt = m.read32(0x800B4CF8)
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

    def run(name, count, setup=None, argsf=lambda c: (), nh=False, i0=0, **kw):
        """setup(m, rng, i) -> context; one fresh scenario per case"""
        for i in range(i0, i0 + n(count)):
            rng = rng_of(name, i)
            h.run(name, f"#{i}", i, (lambda m, rng=rng, i=i: setup(m, rng, i)) if setup else (lambda m, rng=rng: combat(m, rng)),
                  (lambda c, rng=rng, i=i: argsf(c, rng, i)) if argsf.__code__.co_argcount == 3 else argsf,
                  null_hooks=(nh and i % 8 == 7), **kw)

    def swept_states(name, count, setup, argsf=lambda c, rng, i: (), nh=False, **kw):
        """the state byte is swept 0..0x42 (not scaled) and then random"""
        for i in range(0x43 + n(count)):
            rng = rng_of(name, i)
            st = i if i < 0x43 else rng.choice([rng.randint(0, 0x42)] * 3 + [rng.getrandbits(8)])
            h.run(name, f"state {st} #{i}", i, lambda m, rng=rng, i=i, st=st: setup(m, rng, i, st), lambda c, rng=rng, i=i: argsf(c, rng, i),
                  null_hooks=(nh and i % 8 == 7), **kw)

    # ------------------------------------------------------------------ animation
    def hesitate_setup(m, rng, i):
        c = combat(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        w.w16(c + 0xF2, rng.randint(0, max(0, nn - 1)) if rng.random() < 0.95 else rng.getrandbits(16))
        w.w8(c + 0xD4, rng.choice([0, 1, 2, 3, 4, 0x7F, 0x80, 0xFF, 0, 1, rng.getrandbits(8)]))
        for k in range(nn):
            if rng.random() < 0.5:
                a = NAV_BASE + 0x18 * k
                w.w32(a, m.read32(a) | 0x100000 if rng.random() < 0.25 else m.read32(a) & ~0x100000)
        want = rng.choice([0, 1, 2, 100, 101, 102, 200, 201, 202, 3, 50, 99, 103, 199, 203, 255, rng.randint(0, 255)])
        for _ in range(5000):
            sd = rng.getrandbits(16)
            if ((sd * 5 + 1) & 0xFFFF) >> 8 == want:
                break
        w.w32(0x800B4D10, sd if rng.random() < 0.9 else rng.getrandbits(32))
    run("MonkeyRandomHesitate", 500, hesitate_setup, allow_trap=True)

    swept_states("MonkeyAdvanceAnimFrame", 300, lambda m, rng, i, st: combat(m, rng, state=st), allow_trap=True)
    swept_states("MonkeyPickAnim", 300, lambda m, rng, i, st: combat(m, rng, state=st), allow_trap=True)
    swept_states("MonkeyAnimFinished", 300, lambda m, rng, i, st: combat(m, rng, state=st), allow_trap=True)
    swept_states("MonkeyAnimAtStart", 300, lambda m, rng, i, st: combat(m, rng, state=st), allow_trap=True)

    # ------------------------------------------------------------------ projectiles / aiming
    run("MonkeyFireProjectile6", 200, nh=True, allow_trap=True)
    run("MonkeyFireProjectile7", 200, nh=True, allow_trap=True)
    run("MonkeyFireProjectile8", 200, nh=True, allow_trap=True)
    run("MonkeyFireProjectileAimed", 400, nh=True, allow_trap=True,
        argsf=lambda c, rng, i: (rng.choice([6, 6, 5, 0, -1, 0x10006, 0xFFFF0006, 7, rng.getrandbits(32)]) & M32,))

    def aim_setup(m, rng, i):
        c = combat(m, rng, state=rng.choice([30, 30, rng.randint(0, 0x42), 3, 8, 50]))
        w = W(m)
        tgt = m.read32(0x800B4CF8)
        # a direction with a controlled elevation (the thresholds of the animation choice are on |y|)
        import math
        L = rng.choice([200, 800, 1500, 3000])
        el = math.radians(rng.uniform(-90, 90))
        az = math.radians(rng.uniform(0, 360))
        d = [L * math.cos(el) * math.cos(az), L * math.sin(el), L * math.cos(el) * math.sin(az)]
        for k in range(3):
            w.w32(tgt + 0x24 + 4 * k, int(m.reads32(c + 0x24 + 4 * k) + d[k]) & M32)
        cnt = rng.choice([4, 8, 5, 20, 40, 100, 3, 7, rng.getrandbits(32), rng.randint(0, 5000), rng.randint(0, 5000) * 20,
                          rng.randint(0, 5000) * 4, rng.randint(0, 5000) * 5, 0])
        w.w32(FRAME, cnt)
    run("MonkeyAimAtTarget", 1500, aim_setup, nh=True, allow_trap=True,
        argsf=lambda c, rng, i: (rng.choice([0, 0, 5, 5, 5, 0, 1, -1, 0x10000, 0x10005, 6, 0xFFFF0000, 0xFFFF0005, rng.getrandbits(32)]) & M32,))

    def move_setup(m, rng, i):
        c = combat(m, rng)
        w = W(m)
        for off in (0x60, 0x64, 0x68, 0x6C):
            w.w16(c + off, rng.choice([0, 4096, -4096, 2896, 1, rng.getrandbits(16), rng.randint(-4096, 4096)]) & 0xFFFF)
        w.w16(c + 0x62, rng.getrandbits(16))
        w.w8(c + 0x78, rng.choice([9, 41, 43, 4, 44, 45, 46, 49, 50, 51, 52, rng.randint(0, 0x42), rng.randint(0, 0x42)]))
        if rng.random() < 0.2:                               # goal == position (zero vector)
            for k in range(3):
                w.w32(c + 0xBC + 4 * k, m.read32(c + 0x24 + 4 * k))
        else:
            for k in range(3):
                w.w32(c + 0xBC + 4 * k, (m.reads32(c + 0x24 + 4 * k) + rng.randint(-2000, 2000)) & M32)
    run("MonkeyApplyMoveDir", 600, move_setup, allow_trap=True)

    def angle_setup(m, rng, i):
        c = combat(m, rng)
        w = W(m)
        kind = rng.choice(["sane", "sane", "zero", "wild"])
        for off in (0x60, 0x64):
            w.w16(c + off, {"sane": rng.randint(-4096, 4096), "zero": rng.choice([0, 0, 1]), "wild": rng.getrandbits(16)}[kind] & 0xFFFF)
        if rng.random() < 0.15:
            tgt = m.read32(0x800B4CF8)
            for k in (0, 2):
                w.w32(tgt + 0x24 + 4 * k, m.read32(c + 0x24 + 4 * k))
    run("MonkeyAngleToTarget", 700, angle_setup, allow_trap=True)

    # NavNodeGetPos
    def navpos_setup(m, rng, i):
        c = build_monkey_world(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
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
            flags = (0x400 if r_ < 0.4 else (0x1 if r_ < 0.75 else rng.choice([0, 0x2, 0x401, 0x800, rng.getrandbits(32)])))
            w.w32(a, flags | (rng.getrandbits(32) & 0 if True else 0))
            obj = rng.choice([rng.randint(0, 15), rng.randint(0, 15), rng.randint(-2, 20)])
            mat = rng.choice([rng.randint(0, 99), rng.randint(0, 99), rng.randint(0, 5)])
            w.w16(a + 0x10, (obj * 100 + mat) & 0xFFFF if flags & 0x400 else rng.choice([rng.randint(-3, 20), rng.randint(-3, 20), rng.getrandbits(16)]))
            w.w16(a + 0x12, rng.choice([rng.randint(0, 40), rng.randint(0, 40), rng.getrandbits(16)]))
            for off in (4, 6, 8):
                w.w16(a + off, rng.choice([rng.getrandbits(16), rng.randint(-2000, 2000) & 0xFFFF]))
        for j in range(0, 0x10, 2):
            w.w16(0x800AEB6C + j, rng.getrandbits(16))
        return {"matrix": 0x800F4750}

    def navpos_args(c, rng, i):
        nn = 1
        node = rng.choice([rng.randint(0, 4), rng.randint(0, 39), rng.randint(-2, 42), rng.getrandbits(32)])
        if rng.random() < 0.2:
            node = (node & 0xFFFF) | (rng.getrandbits(16) << 16)
        out = VEC_OUT
        if rng.random() < 0.12:        # an output that aliases the node record
            out = NAV_BASE + 0x18 * rng.randint(0, 3) + rng.choice([0x4, 0x6, 0x8, 0x10, 0xC, 0x0])
        return (node & M32, out)
    run("NavNodeGetPos", 900, navpos_setup, navpos_args, nh=True, allow_trap=True)

    # ------------------------------------------------------------------ in sight range
    def sight_setup(m, rng, i):
        c = combat(m, rng)
        w = W(m)
        tgt = m.read32(0x800B4CF8)
        w.w32(tgt + 0x70, rng.choice([0, 1, 2, 3, 0x10, 0x11, 0x12, 0xF, 2, 2, 3, 1, 0, rng.getrandbits(32)]))
        w.w8(c + 0x78, rng.choice([51, 51, 50, 51, 52, 50, 1, 13, 16, 19, 58, 14, 17, 20, 59, 15, 18, 21, 60, rng.randint(0, 0x42)]))
        w.w32(c + 0x248, rng.choice([0, 1 << 19, rng.getrandbits(32), 0xFFFFFFFF, 0x80000]))
        if rng.random() < 0.75:
            w.w16(0x800F4472, rng.choice([0, 1, 2, 4, 5, 7]))       # not a boss arena (the interesting part comes after the arena test)
            w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x35, 0x45]))
        elif rng.random() < 0.5:
            w.w16(0x800F4476, 0x46)
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        v14 = rng.choice([100, 500, 2000, 0, -5, rng.getrandbits(16), rng.randint(0, 3000)])
        v1a = rng.choice([100, 500, 2000, 0, -5, rng.getrandbits(16), rng.randint(0, 3000)])
        w.w16(info + 14, v14 & 0xFFFF)
        w.w16(info + 0x1A, v1a & 0xFFFF)
        base = rng.choice([s16(v14 & 0xFFFF), 3 * s16(v1a & 0xFFFF), s16(v1a & 0xFFFF)])
        w.w32(c + 0x20C, (base + rng.choice([-1, 0, 1, 0, 2, -2, rng.randint(-3000, 3000)])) & M32 if rng.random() < 0.85 else rng.getrandbits(32))
    run("MonkeyTargetInSightRange", 900, sight_setup, allow_trap=True)

    # ------------------------------------------------------------------ body on / off (CollisionBodyRegister / Unregister are the ported body code)
    def body_setup(m, rng, i):
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
    run("MonkeyBodyOff", 300, body_setup, nh=True, allow_trap=True)
    run("MonkeyBodyOn", 300, body_setup, nh=True, allow_trap=True)
    run("MonkeyDefeatBurst", 120, nh=True)
    def foot_setup(m, rng, i):
        c = combat(m, rng)
        w = W(m)
        w.w32(FRAME, rng.choice([0, 3, 6, 9, 12, 1, 2, 4, 5, rng.randint(0, 3000) * 3, rng.randint(0, 3000), rng.getrandbits(32), 0xFFFFFFFF]))
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        w.w16(info + 4, rng.choice([0, 0, 1, -1, 500, rng.getrandbits(16)]) & 0xFFFF)
        w.w8(c + 0x78, rng.choice([26, 53, 12, 36, 0, 1, 2, rng.randint(0, 0x42)]))
    run("MonkeyFootDust", 400, nh=True, setup=lambda m, rng, i: foot_setup(m, rng, i))

    run("MonkeyDebrisBurst6", 150, nh=True)
    run("MonkeyDebrisBurst1", 150, nh=True)
    run("MonkeyAlertPuff", 150, nh=True)

    # ------------------------------------------------------------------ contact (the Monkey Net and other gadget hits)
    def contact_setup(m, rng, i, st, contact=None, f2bit=None, c124=None, light=False):
        if light:       # the sweeps only need the current monkey (the base image has everything else the code reads)
            c = MONKEYS + MONKEY_SIZE * rng.randint(0, 7)
            m.write32(G_CUR, c)
            m.write8(c + 0x78, st)
        else:
            c = combat(m, rng, state=st, positions=False)
        w = W(m)
        ct = rng.randint(0, 15) if contact is None else contact
        w.w32(c + 0x50, (ct << 28) | (rng.getrandbits(28) if rng.random() < 0.7 else 0))
        f2 = rng.getrandbits(32) if f2bit is None else ((rng.getrandbits(32) & ~0x8000) | (0x8000 if f2bit else 0))
        w.w32(c + 0x228, f2)
        w.w16(c + 0x124, rng.choice([0, 1, -1, 5, -5, 0x7FFF, -0x8000, rng.getrandbits(16)]) if c124 is None else c124)
        w.w16(c + 0x126, rng.getrandbits(16))
        # the rest of the monkey: other monkeys / unrelated fields do not matter, but make the neighbours distinct
    # every state x every contact type
    ci = 0
    for st in range(0x43):
        for ct in range(16):
            rng = rng_of("MonkeyUpdateContact", ci)
            h.run("MonkeyUpdateContact", f"state {st} contact {ct}", ci, lambda m, rng=rng, st=st, ct=ct: contact_setup(m, rng, ci, st, ct, light=True),
                  lambda c: ())
            ci += 1
    # the states that matter x the contact types that matter x flags2 bit 15 x sign of +0x124
    for st in (0, 1, 2, 3, 13, 14, 15, 30, 31, 32, 33, 58, 59, 60):
        for ct in (0, 1, 2, 3, 4, 8, 9, 10, 11, 15):
            for f2 in (0, 1):
                for c124 in (0, 5, -5, 0x7FFF, -0x8000):
                    rng = rng_of("MonkeyUpdateContact", ci)
                    h.run("MonkeyUpdateContact", f"state {st} contact {ct} f2 {f2} c124 {c124}", ci,
                          lambda m, rng=rng, st=st, ct=ct, f2=f2, c124=c124: contact_setup(m, rng, ci, st, ct, f2, c124 & 0xFFFF, light=True),
                          lambda c: ())
                    ci += 1
    for i in range(n(300)):
        rng = rng_of("MonkeyUpdateContact-rand", i)
        h.run("MonkeyUpdateContact", f"random #{i}", ci + i, lambda m, rng=rng, i=i: contact_setup(m, rng, i, rng.choice([rng.randint(0, 0x42), rng.getrandbits(8)])), lambda c: ())

    # ------------------------------------------------------------------ screen / alert / LOD
    def proj_setup(m, rng, i):
        c = combat(m, rng)
        w = W(m)
        w.w8(c + 0x7A, rng.choice([1, 3, 4, 5, 1, 3, 0, 2, rng.getrandbits(8)]))
        dx = rng.choice([rng.randint(5985, 6015), rng.randint(5985, 6015), rng.randint(5985, 6015), 6000, 6001, rng.randint(-6100, 6100), rng.randint(-100, 100)])
        dz = rng.choice([0, 0, rng.randint(-100, 100), rng.randint(-3000, 3000)]) if abs(dx) < 5990 or abs(dx) > 6010 else 0
        cx, cz = rng.randint(-12000, 12000), rng.randint(-12000, 12000)
        w.w32(c + 0x24, cx & M32)
        w.w32(c + 0x2C, cz & M32)
        w.w32(c + 0x28, rng.randint(-12000, 12000) & M32)
        w.w16(0x800F45A8, u16(cx - dx))
        w.w16(0x800F45A8 + 2, rng.getrandbits(16))
        w.w16(0x800F45A8 + 4, u16(cz - dz))
        for off in range(0, 0x20, 2):
            w.w16(0x800E78C0 + off, rng.getrandbits(16))
    run("MonkeyProjectToScreen", 900, proj_setup, nh=True)

    def icon_setup(m, rng, i):
        c = combat(m, rng, positions=False)
        w = W(m)
        w.w8(c + 0x79, rng.choice([0, 1, 1, 2, 3, 255, rng.getrandbits(8)]))
        w.w8(c + 0x249, rng.choice([0, 1, 2, 3, 255, rng.getrandbits(8)]))
        w.w32(c + 0x248, rng.choice([0, 1 << 19, rng.getrandbits(32), 0xFFFFFFFF, ~(1 << 19) & M32]))
    run("MonkeyAlertIconKind", 300, icon_setup)

    def lod_setup(m, rng, i):
        c = combat(m, rng)
        w = W(m)
        w.w8(c + 0x78, rng.choice([30, 32, 0, 1, rng.randint(0, 0x42)]))
        w.w8(c + 0xD5, rng.choice([0, 1, 2, 3, 0, 1, 2, 3, rng.getrandbits(8)]))
        w.w8(0x800E6908, rng.choice([0, 1, 2, 1]))
        dist = rng.choice([999, 1000, 1001, 1999, 2000, 2001, rng.randint(900, 1100), rng.randint(1900, 2100), rng.randint(1950, 2100), rng.randint(0, 4000), 0, rng.randint(0, 40000)])
        cx = rng.randint(-12000, 12000)
        w.w32(c + 0x24, cx & M32)
        w.w16(0x800F45A8, u16(cx - dist))
        w.w32(c + 0x2C, 0)
        w.w16(0x800F45A8 + 4, 0)
        for k in range(4):
            w.w32(0x800F44F4 + 4 * k, rng.getrandbits(32))
    run("MonkeySelectLod", 400, lod_setup, nh=True)

    # ------------------------------------------------------------------ nav: goal node, link extras
    def goal_setup(m, rng, i):
        c = build_monkey_world(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        routes = rng.choice([0, 0, 1, 2, 3, 4, 8, 255])
        w.w8(c + 0xD7, routes)
        w.w8(c + 0xD8, rng.choice([rng.randint(0, max(0, routes - 1)), rng.randint(0, 8), rng.getrandbits(8), 0, 1]))
        lens = [rng.choice([0, 1, 2, 3, 4, 5, 8, 16, 33, 255, rng.randint(0, 16)]) for _ in range(8)]
        for k, v in enumerate(lens):
            w.w8(c + 0xDA + k, v)
        for k in range(8, 16):
            w.w8(c + 0xDA + k, rng.getrandbits(8))
        route = m.read8(c + 0xD8)
        ln = m.read8(c + 0xDA + route) if route < 0x100 - 0xDA + 0x100 else 0
        w.w8(c + 0xD9, rng.choice([0, 1, max(0, ln - 1), ln, ln + 1, rng.getrandbits(8)]))
        w.w16(c + 0xE8, rng.choice([0, 0xFFFF, 0x7FFF, rng.getrandbits(16), (1 << min(ln, 15)) - 1, rng.getrandbits(16) & ((1 << min(ln, 15)) - 1),
                                    0x8000, 1]))
        w.w16(c + 0xE6, rng.getrandbits(16))
        w.w32(0x800B4D0C, ROUTE_BASE)
        data = bytearray()
        for k in range(0x1000):
            data += struct.pack("<H", rng.randint(0, max(0, nn - 1)) if rng.random() < 0.97 else rng.getrandbits(16))
        m.write_bytes(ROUTE_BASE + 0x88, bytes(data[:0x1000]))
        w.w8(0x800AEB59, rng.choice([0, 1]))
        if rng.random() < 0.05:
            w.w16(0x800B4D18, rng.choice([0, -1, 1]))
        if rng.random() < 0.3:
            w.w8(c + 0xD7, routes)
    run("MonkeyPickGoalNode", 1200, goal_setup, nh=True, allow_trap=True)

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

    def extra_setup(m, rng, i):
        c = build_monkey_world(m, rng)
        w = W(m)
        pair = link_pair(m, rng)
        pr = pair if pair and rng.random() < 0.85 else (rng.randint(0, 40), rng.randint(0, 40))
        lo, hi = min(pr), max(pr)
        ents = [(rng.randint(0, 40), rng.randint(0, 40)) for _ in range(rng.choice([0, 1, 2, 3, 6, 10]))]
        ents = [(min(a, b), max(a, b)) for a, b in ents]
        if rng.random() < 0.7:
            ents.append((lo, hi))
        if rng.random() < 0.3:
            ents.append((lo, hi + 1)); ents.append((lo, max(0, hi - 1)))
        ents = sorted(set(ents))
        data = bytearray()
        for a, b in ents:
            f2 = rng.choice([0, 0x02000000, 0x200, 0x2000, 0x04000000, rng.getrandbits(32), 0xFFFFFFFF, 0x02000000 | 0x200])
            data += struct.pack("<hhII", a, b, f2, rng.getrandbits(32))
        m.write_bytes(EXTRA_BASE, bytes(data) + bytes(16))
        w.w32(0x800B4D1C, EXTRA_BASE)
        w.w16(0x800B4D20, len(ents) if rng.random() < 0.95 else rng.choice([0, -1, len(ents) + 1]))
        w.w32(c + 0x228, rng.getrandbits(32))
        w.w32(c + 0x248, rng.getrandbits(32))
        w.w16(c + 0x4A, rng.getrandbits(16))
        w.w16(c + 0xEC, rng.getrandbits(16))
        if rng.random() < 0.4:
            w.w16(0x800F4476, rng.choice([0x46, 0x29]))
            w.w16(0x800F4472, rng.choice([3, 6, 8, 0, 1]))
        return {"pair": pr, "cur": c}

    def extra_args(c, rng, i):
        a, b = c["pair"]
        r_ = rng.random()
        if r_ < 0.15:
            a, b = b, a
        if rng.random() < 0.3:
            a |= rng.getrandbits(16) << 16
        if rng.random() < 0.3:
            b |= rng.getrandbits(16) << 16
        return (a & M32, b & M32)

    def load_extra_setup(m, rng, i):
        ctx = extra_setup(m, rng, i)
        w = W(m)
        c = ctx["cur"]
        a, b = ctx["pair"]
        if rng.random() < 0.3:                     # the monkey already holds this link (nothing happens)
            w.w16(c + 0x224, a)
            w.w16(c + 0x226, b)
        elif rng.random() < 0.3:
            w.w16(c + 0x224, a)
        return ctx
    run("MonkeyLoadLinkExtra", 900, load_extra_setup, extra_args, allow_trap=True)

    def lookup_args(c, rng, i):
        a, b = extra_args(c, rng, i)
        out = VEC_OUT
        if rng.random() < 0.1:
            out = EXTRA_BASE + rng.choice([4, 8, 0xC + 4, 0])
        return (a, b, out)
    run("NavLinkLookupExtra", 900, extra_setup, lookup_args, allow_trap=True)

    # ------------------------------------------------------------------ movement
    def step_setup(m, rng, i, st=None):
        c = combat(m, rng, positions=True)
        w = W(m)
        w.w8(c + 0x78, st if st is not None else rng.choice([61, 62, 30, 32, 26, 53, 9, 3, 8, 12, 36, 4, 44, 45, 46, 49, rng.randint(0, 0x42)]))
        w.w32(c + 0x24C, rng.choice([0, 1, 4, 5, 2, 3, 0xFFFFFFFF, rng.getrandbits(32), 1, 5, 4, 0, 0xFFFFFFFE]))
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        for off in (2, 4):
            w.w16(info + off, rng.choice([0, 100, 360, 1000, -50, 1, -1, 37, rng.getrandbits(16), rng.randint(0, 2000)]) & 0xFFFF)
        w.w8(c + 0x24B, rng.getrandbits(8))
        w.w32(c + 0x230, rng.choice([179, 180, 181, 0, -1, rng.getrandbits(32), rng.randint(0, 400)]) & M32)
        w.w8(c + 0x79, rng.choice([0, 1, 1, 2, 3, rng.getrandbits(8)]))
        w.w32(c + 0x20C, rng.choice([599, 600, 601, 0, 5000, -5, rng.getrandbits(32), rng.randint(0, 700)]) & M32)
        w.w16(c + 0x120, rng.choice([0, 1, -1, 7, rng.getrandbits(16)]) & 0xFFFF)
        w.w16(c + 0x94, rng.choice([0, 1, -1, 7, rng.getrandbits(16)]) & 0xFFFF)
        for off in (0x60, 0x62, 0x64, 0x68, 0x6A, 0x6C, 0x70, 0x72, 0x74):
            w.w16(c + off, rng.choice([0, 4096, -4096, 2896, 1, -1, 15, -15, rng.getrandbits(16), rng.randint(-4096, 4096)]) & 0xFFFF)
        w.w16(0x800F4472, rng.choice([0, 1, 2, 4, 5, 0, 1, 3, 6, 8]))
        w.w16(0x800F4476, rng.choice([0x29, 0x10, 0x35, 0x10, 0x46]))
        if rng.random() < 0.5:
            w.w32(c + 0x24C, rng.choice([0, 4, 2, 6, 0xFFFFFFFA, 0]))          # no hop; bit 2 set / clear
        if rng.random() < 0.25:                                                   # rounding of the scaled direction (x * s1 = -32767 ...)
            sp_ = rng.choice([1, -1, 32767, 3, -3, 100])
            w.w16(info + 2, sp_ & 0xFFFF)
            w.w16(info + 4, sp_ & 0xFFFF)
            w.w8(c + 0x79, 0)
            for off in (0x68, 0x6C):
                w.w16(c + off, rng.choice([-1, 1, -32767, 32767, -32768, 2, -3]) & 0xFFFF)
        if rng.random() < 0.2:
            w.w8(c + 0x79, 1)
            w.w32(c + 0x20C, rng.choice([599, 600, 601, 602, 603]) & M32)
        if rng.random() < 0.3 and (m.read32(c + 0x24C) & 1):
            # the hop: counters around the limit
            lim = rng.choice([0, 1, 5, 30, 0x7FFF, 0x8000, 0xFFFF, rng.getrandbits(16)])
            w.w16(c + 0x128, lim)
            w.w16(c + 0x96, rng.choice([lim - 1, lim - 2, lim, lim + 1, rng.getrandbits(16), 0xFFFF, 0x7FFF]) & 0xFFFF)
    swept_states("MonkeyMoveStep", 1300, lambda m, rng, i, st: step_setup(m, rng, i, st), nh=True, allow_trap=True)
    run("MonkeyMoveStep", 400, step_setup, nh=True, allow_trap=True, i0=5000)

    def step_focus(m, rng, i):
        """the non-hop, non-boss speed rules: states 30 / 32 / 26 / 53 and the generic ones with the distance clamp"""
        step_setup(m, rng, i)
        c = cur(m)
        w = W(m)
        w.w8(c + 0x78, rng.choice([30, 30, 32, 26, 53, 1, 2, 5, 6, 7, 10, 0]))
        w.w32(c + 0x24C, rng.choice([0, 4, 2, 6]))
        w.w16(0x800F4472, rng.choice([0, 1, 2, 4]))
        w.w16(0x800F4476, 0x10)
        w.w8(c + 0x79, rng.choice([1, 1, 1, 0, 2]))
        w.w32(c + 0x20C, rng.choice([599, 600, 601, 602, 603, 0, 300, 1000, 800, 900]) & M32)
        w.w8(c + 0x24B, rng.choice([0, 1, 0, 2]))
        w.w32(c + 0x230, rng.choice([179, 180, 181, 0, 500]) & M32)
    run("MonkeyMoveStep", 500, step_focus, nh=True, allow_trap=True, i0=9000)

    def turn_setup(m, rng, i):
        c = combat(m, rng)
        w = W(m)
        for off in (0x60, 0x64):
            w.w16(c + off, rng.choice([0, 0, 4096, -4096, 2896, rng.randint(-4096, 4096), rng.getrandbits(16)]) & 0xFFFF)
        w.w16(c + 0x62, rng.getrandbits(16))
        w.w32(c + 0x248, rng.getrandbits(32))
        if rng.random() < 0.2:
            tgt = m.read32(0x800B4CF8)
            for k in (0, 2):
                w.w32(tgt + 0x24 + 4 * k, m.read32(c + 0x24 + 4 * k))
        # the angle difference the function computes (to hit the threshold rate + 45 exactly)
        def ang(y, x):
            return m.call(0x800349B4, (y & M32, x & M32))[2]
        try:
            tgt = m.read32(0x800B4CF8)
            a1 = s32(ang(m.read32(tgt + 0x2C) - m.read32(c + 0x2C), m.read32(tgt + 0x24) - m.read32(c + 0x24)))
            a2 = s32(ang(m.reads16(c + 0x64), m.reads16(c + 0x60)))
            return {"d": a1 - a2}
        except Exception:
            return {"d": 100}
    run("MonkeyTurnTowardTarget", 1000, turn_setup, allow_trap=True,
        argsf=lambda c, rng, i: (rng.choice([0, 5, 10, 20, 45, -45, 100, -1, 1, 2, 300, rng.randint(-100, 300), rng.getrandbits(32),
                                             abs(c["d"]) - 45, abs(c["d"]) - 45, abs(c["d"]) - 46, abs(c["d"]) - 44]) & M32,))

    def los_setup(m, rng, i):
        import math
        c = combat(m, rng, positions=False)
        w = W(m)
        tgt = m.read32(0x800B4CF8)
        flag = rng.choice([0, 1])
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
            w.w16(tgt + 0x1C + 2 * k, v & 0xFFFF)                       # svec read by CopySvecAt1C
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
        ty_used = s16((ty if flag == 0 else ty) & 0xFFFF)
        return {"ty": ty_used}
    run("MonkeyLineOfSight", 700, los_setup, nh=True, allow_trap=True,
        argsf=lambda c, rng, i: (rng.choice([1, 1, 0, 0, 2, -1, rng.getrandbits(32)]) & M32,))

    # ------------------------------------------------------------------ SDK maths, directly
    def angle(rng):
        return rng.choice([0, 1, -1, 1023, 1024, 1025, 2047, 2048, 2049, 3071, 3072, 3073, 4095, 4096, -4096, 4097, 0x7FFFFFFF, -0x80000000,
                           rng.randint(-5000, 5000), rng.randint(-1 << 20, 1 << 20), rng.getrandbits(32), rng.randint(0, 4095),
                           rng.randint(-4095, 0)]) & M32
    sweep = sorted(set([a for e in (0, 1024, 2048, 3072, 4096, -1024, -2048, -3072, -4096) for a in range(e - 3, e + 4)] +
                       list(range(-4100, 4200, 13)) + [0x7FFFFFFF, -0x80000000, 0x7FFFFFFE, -0x7FFFFFFF, 0x10000, -0x10000, 0xFFF, 0x1000, 0x1001]))
    for name in ("rsin (SDK)", "rcos (SDK)"):
        for i in range(len(sweep) + n(300)):
            rng = rng_of(name, i)
            a = sweep[i] if i < len(sweep) else angle(rng)
            h.run(name, f"#{i}", i, lambda m: None, lambda c, a=a: (a & M32,))
    for i in range(n(800)):
        rng = rng_of("RotMatrixYXZ", i)
        def setup(m, rng=rng):
            build_monkey_world(m, rng, wild=False)
            w = W(m)
            for k, a in enumerate((angle(rng), angle(rng), angle(rng))):
                w.w16(0x801F0000 + 2 * k, a & 0xFFFF)
            for k in range(0, 0x20, 2):
                w.w16(0x801F0100 + k, rng.getrandbits(16))
        h.run("RotMatrixYXZ (SDK)", f"#{i}", i, setup, lambda c: (0x801F0000, 0x801F0100))
    print(f"scenario families done in {time.time() - t0:.1f}s")


# --------------------------------------------------------------------------------------- mutation testing
MUTATIONS = [
    ("Hesitate chance", "if (rem < 3u) {", "if (rem < 4u) {"),
    ("Hesitate flag bit", "if (flags & 0x100000u) return 1;", "if (flags & 0x200000u) return 1;"),
    ("AdvanceAnimFrame clamp", 'WR16(saved_monkey + AE_MK_ANIM_FRAME, (uint32_t)RD16(tbl + ((uint32_t)idx << 3) + AE_MK_ANIM_LAST_FRAME) + 1u);', 'WR16(saved_monkey + AE_MK_ANIM_FRAME, (uint32_t)RD16(tbl + ((uint32_t)idx << 3) + AE_MK_ANIM_LAST_FRAME) + 2u);'),
    ("AdvanceAnimFrame branch", 'if (frame < last1) {', 'if (frame < last1 - 1) {'),
    ("PickAnim flag bit", "if (!((flags >> 2) & 1u)) WR16(monkey + AE_MK_COUNTER96, 0);", "if (!((flags >> 3) & 1u)) WR16(monkey + AE_MK_COUNTER96, 0);"),
    ("AnimFinished compare", 'past_end = (int32_t)RD16(monkey + AE_MK_ANIM_LENGTH) < frame;', 'past_end = (int32_t)RD16(monkey + AE_MK_ANIM_LENGTH) <= frame;'),
    ("AnimAtStart plus one", 'uint32_t first_frame_plus_one = RD16(tbl + ((uint32_t)idx << 3)) + 1u;', 'uint32_t first_frame_plus_one = RD16(tbl + ((uint32_t)idx << 3)) + 2u;'),
    ("Fire6 voice", "ae_MonkeyQueueVoice(r, h, 75, 5);", "ae_MonkeyQueueVoice(r, h, 75, 4);"),
    ("Fire7 type", "ae_MonkeySpawnProjectile(r, h, 7);", "ae_MonkeySpawnProjectile(r, h, 6);"),
    ("Aim wobble y", 'component = RD16(tgt_actor + AE_MK_POS + 4) + 125u - RD16(origin_monkey + AE_MK_POS + 4);', 'component = RD16(tgt_actor + AE_MK_POS + 4) + 126u - RD16(origin_monkey + AE_MK_POS + 4);'),
    ("Aim shift 6", 'int32_t velocity_x = sra32(direction_x_rounded, 6);', 'int32_t velocity_x = sra32(direction_x_rounded, 5);'),
    ("Aim lod threshold", 'if (abs_pitch < 1060) {', 'if (abs_pitch < 1100) {'),
    ("Aim slope 2896", '} else if (abs_pitch < 2896) {', '} else if (abs_pitch < 2950) {'),
    ("Aim arg 0 mask", "if ((cnt & 3u) != 0) return;", "if ((cnt & 7u) != 0) return;"),
    ("Aim every fifth", "if (cnt != q * 5u) return;", "if (cnt != q * 4u) return;"),
    ("Aim state 30 offset", "WR16(sp + 18, py + 20u);", "WR16(sp + 18, py + 21u);"),
    ("Aim idle voice", "ae_MonkeyQueueVoice(r, h, 100, 3);", "ae_MonkeyQueueVoice(r, h, 100, 2);"),
    ("Aim type 5 scale", 'int32_t scaled_velocity_x = mul_lo(x, 180);', 'int32_t scaled_velocity_x = mul_lo(x, 90);'),
    ("Aimed random", "uint32_t q = mulu_hi(rnd, 0xAAAAAAABu) >> 1;", "uint32_t q = mulu_hi(rnd, 0xAAAAAAABu) >> 2;"),
    ("Aimed y offset", 'WR16(sp + 18, RD16(monkey + AE_MK_POS + 4) - 200u);', 'WR16(sp + 18, RD16(monkey + AE_MK_POS + 4) - 199u);'),
    ("Fire8 y offset", 'WR16(sp + 18, RD16(monkey + AE_MK_POS + 4) + 200u);', 'WR16(sp + 18, RD16(monkey + AE_MK_POS + 4) + 201u);'),
    ("AngleToTarget wrap", 'if (!(angle > 0)) angle = add32(angle, 4096);', 'if (!(angle >= 0)) angle = add32(angle, 4096);'),
    ("ApplyMoveDir state 43", "if (st == 41 || st == 43) return;", "if (st == 41 || st == 42) return;"),
    ("NavNodeGetPos y", 'WR32(position_out + 4, (uint32_t)sub32(add32(y1, y2), y3));', 'WR32(position_out + 4, (uint32_t)sub32(add32(y1, y3), y2));'),
    ("NavNodeGetPos record", 'uint32_t rec = (uint32_t)v * 56u + RD32(AE_G_NAV_GROUND_REF);\n        int32_t x', 'uint32_t rec = (uint32_t)v * 52u + RD32(AE_G_NAV_GROUND_REF);\n        int32_t x'),
    ("SightRange state 51", "if (RD8(monkey + AE_MK_AI_STATE) == 51) {", "if (RD8(monkey + AE_MK_AI_STATE) == 50) {"),
    ("SightRange nibble", '(RD32(RD32(AE_G_MONKEY_TARGET) + AE_MK_TARGET_STATE_FLAGS) & 0xFu) < 2u', '(RD32(RD32(AE_G_MONKEY_TARGET) + AE_MK_TARGET_STATE_FLAGS) & 0xFu) < 3u'),
    ("BodyOff flags", 'flags = ((flags | 0x40000000u) & 0xFFFCFFFFu) | 0x20000u;', 'flags = ((flags | 0x40000000u) & 0xFFFCFFFFu) | 0x10000u;'),
    ("BodyOn flags", 'flags = ((flags & 0xBFFFFFFFu) & 0xFFFCFFFFu) | 0x10000u;', 'flags = ((flags & 0xBFFFFFFFu) & 0xFFFEFFFFu) | 0x10000u;'),
    ("DefeatBurst voice", "ae_MonkeyQueueVoice(r, h, 121, 2);", "ae_MonkeyQueueVoice(r, h, 121, 1);"),
    ("FootDust modulus", 'if (frame != quotient + (quotient << 1)) return;', 'if (frame != quotient + (quotient << 2)) return;'),
    ("FootDust y", 'WR16(sp + 18, RD16(m + AE_MK_POS + 4) - 32u);', 'WR16(sp + 18, RD16(m + AE_MK_POS + 4) - 31u);'),
    ("Debris modulus", 'uint32_t q = mulu_hi(rnd, 0x51EB851Fu) >> 6;\n    uint32_t scaled_quotient = q * 200u;\n    uint32_t random_offset = rnd - scaled_quotient - 100u;',
     'uint32_t q = mulu_hi(rnd, 0x51EB851Fu) >> 6;\n    uint32_t scaled_quotient = q * 200u;\n    uint32_t random_offset = rnd - scaled_quotient - 99u;'),
    ("Debris count", '} while (particles_spawned < 6);', '} while (particles_spawned < 5);'),
    ("AlertPuff particle", "AE_MK_HOOK(r, h, SpawnParticle, 256, sp + 16, 0, 0);", "AE_MK_HOOK(r, h, SpawnParticle, 255, sp + 16, 0, 0);"),
    ("Contact net state", 'if (state == 30 || state == 32) {', 'if (state == 30 || state == 31) {'),
    ("Contact net clears", 'WR16(contact_monkey + AE_MK_KIND, 0);\n            return 0;', 'WR16(contact_monkey + AE_MK_KIND, 0);\n            return 1;'),
    ("Contact type 11", 'if (contact_type == 8 || contact_type == 1 || contact_type == 11 || contact_type == 10) return 1;', 'if (contact_type == 8 || contact_type == 1 || contact_type == 10) return 1;'),
    ("Contact type 3 state", 'if (RD8(contact_monkey_after_test + AE_MK_AI_STATE) != 2) {', 'if (RD8(contact_monkey_after_test + AE_MK_AI_STATE) != 3) {'),
    ("Contact flags2 bit", 'if ((RD32(RD32(current_monkey_global) + AE_MK_FLAGS2) & 0x8000u) == 0) return 0;', 'if ((RD32(RD32(current_monkey_global) + AE_MK_FLAGS2) & 0x4000u) == 0) return 0;'),
    ("Contact +0x124", 'return RDS16(cur_monkey(r) + AE_MK_TIMER_124) <= 0;', 'return RDS16(cur_monkey(r) + AE_MK_TIMER_124) < 0;'),
    ("Project range", "if (!(d < 6001)) return 1;", "if (!(d < 6015)) return 1;"),
    ("Project screen x", "int32_t inside = in_x < 485u;", "int32_t inside = in_x < 484u;"),
    ("Project y low", "if (sy < -25) return 1;", "if (sy < -24) return 1;"),
    ("Project otz", 'if (feet_depth > 0) return 0;', 'if (feet_depth >= 0) return 0;'),
    ("Project offset", "WR16(sp + 18, t - 400u);", "WR16(sp + 18, t - 399u);"),
    ("AlertIcon 896", "return 896;", "return 897;"),
    ("AlertIcon nop", 'if (sub32(n, (int32_t)lost_sight_count) < 2) return 448;', 'if (sub32(n, (int32_t)lost_sight_count) < 3) return 448;'),
    ("SelectLod 2000", 'else if (dist < 2000) new_lod = 1;', 'else if (dist < 2100) new_lod = 1;'),
    ("SelectLod model", 'uint32_t model = RD32(AE_G_LOD_MODELS + (uint32_t)new_lod * 4u);', 'uint32_t model = RD32(AE_G_LOD_MODELS + (uint32_t)new_lod * 8u);'),
    ("PickGoal wrap", "int32_t next_group = (int32_t)RD8(monkey + AE_MK_GROUP) + 1;", "int32_t next_group = (int32_t)RD8(monkey + AE_MK_GROUP) + 2;"),
    ("PickGoal bit scan", "if ((sra32(visited, (uint32_t)first_unvisited) & 1) == 0) break;", "if ((sra32(visited, (uint32_t)first_unvisited) & 2) == 0) break;"),
    ("PickGoal table", 'uint32_t goal = RD16(route_table + offset + AE_MK_ROUTE_NODES);', 'uint32_t goal = RD16(route_table + offset + 0x8A);'),
    ("PickGoal wrap to 0", "if ((uint32_t)first_unvisited == current_length) pick = 0;", "if ((uint32_t)first_unvisited == current_length + 1u) pick = 0;"),
    ("LoadLinkExtra boss", 'WR16(cm + AE_MK_BODY_FLAGS, 0xC000);', 'WR16(cm + AE_MK_BODY_FLAGS, 0x8000);'),
    ("LoadLinkExtra bit", "if (RD32(cm + AE_MK_FLAGS2) & 0x2000u) f &= 0xFF7FFFFFu; else f |= 0x800000u;", "if (RD32(cm + AE_MK_FLAGS2) & 0x2000u) f &= 0xFF7FFFFFu; else f |= 0x400000u;"),
    ("LoadLinkExtra order", 'if (high_node < entry_node) break;\n                if (entry_node == high_node) {', 'if (high_node <= entry_node) break;\n                if (entry_node == high_node) {'),
    ("Lookup swap", 'if (to_node < from_node) { high_bits = from_bits; low_bits = to_bits; }', 'if (from_node < to_node) { high_bits = from_bits; low_bits = to_bits; }'),
    ("Lookup result", 'return (int32_t)RD32(entry_offset + RD32(AE_G_NAV_EXTRA_LINKS) + AE_NAV_EXTRA_FLAGS);', 'return (int32_t)RD32(entry_offset + RD32(AE_G_NAV_EXTRA_LINKS) + 8u);'),
    ("MoveStep info speed", '} else {\n            speed = RDS16(info_of(r, monkey) + AE_MK_INFO_WALK_SPEED);\n        }', '} else {\n            speed = RDS16(info_of(r, monkey) + AE_MK_INFO_ALERT_SPEED);\n        }'),
    ("MoveStep 360", "speed = 360;", "speed = 361;"),
    ("MoveStep wobble", "return add32(mul_lo(sine, 50) / 4096, 240);", "return add32(mul_lo(sine, 50) / 4096, 241);"),
    ("MoveStep 74 percent", "WR16(cur_monkey(r) + AE_MK_REACH_RADIUS, mul_lo(speed, 74) / 100);", "WR16(cur_monkey(r) + AE_MK_REACH_RADIUS, mul_lo(speed, 148) / 100);"),
    ("MoveStep rounding", "WR16(monkey + AE_MK_VEL, scaled_x / 32768);", "WR16(monkey + AE_MK_VEL, sra32(scaled_x < 0 ? add32(scaled_x, 32766) : scaled_x, 15));"),
    ("MoveStep state 61", "if (state == 61) {", "if (state == 60) {"),
    ("MoveStep hop spark", "WR16(sp + 32, neg32(RDS16(monkey + AE_MK_MOVE_DIR)) / 16);", "WR16(sp + 32, neg32(RDS16(monkey + AE_MK_MOVE_DIR)) / 8);"),
    ("MoveStep angle", 'int32_t angle = shift_left32(dist, 12) / 2400;', 'int32_t angle = shift_left32(dist, 12) / 4800;'),
    ("MoveStep double", 'speed = shift_left32(RDS16(info_of(r, cur_monkey(r)) + AE_MK_INFO_WALK_SPEED), 1);\n    } else {', 'speed = shift_left32(RDS16(info_of(r, cur_monkey(r)) + AE_MK_INFO_ALERT_SPEED), 1);\n    } else {'),
    ("MoveStep 85 percent", "if (RD8(monkey + AE_MK_ALERTED) == 1) speed = mul_lo(speed, 85) / 100;", "if (RD8(monkey + AE_MK_ALERTED) == 1) speed = mul_lo(speed, 45) / 100;"),
    ("Turn threshold", 'if (ad < add32(turn_rate, 45)) return;', 'if (ad < add32(turn_rate, 46)) return;'),
    ("Turn sign", 'WR16(sp + 18, neg32(turn_rate));', 'WR16(sp + 18, turn_rate);'),
    ("LOS min distance", "if (RDS32(monkey + AE_MK_DIST_TO_TARGET) < 250) return 1;", "if (RDS32(monkey + AE_MK_DIST_TO_TARGET) < 251) return 1;"),
    ("LOS cache bit", "return (int32_t)((RD32(monkey + AE_MK_FLAGS) >> 28) & 1u);", "return (int32_t)((RD32(monkey + AE_MK_FLAGS) >> 27) & 1u);"),
    ("LOS y offset", 'int32_t origin_y = sub32((int32_t)RD32(origin_monkey + AE_MK_POS + 4), 125);', 'int32_t origin_y = sub32((int32_t)RD32(origin_monkey + AE_MK_POS + 4), 124);'),
    ("LOS ground 200", 'if (sub32(ground_height, target_y) < -200) return 0;', 'if (sub32(ground_height, target_y) < -201) return 0;'),
    ("LOS ground 50", 'if (sub32(ground_height, sample_y) < -50) return 0;', 'if (sub32(ground_height, sample_y) < -49) return 0;'),
    ("LOS step z", 'int32_t step_z = (int32_t)RD32(sp + 28);', 'int32_t step_z = (int32_t)RD32(sp + 24);'),
    ("LOS flag y", 'WR8(sp + 32, scaled_component < 100);', 'WR8(sp + 32, scaled_component < 101);'),
    ("sdk rsin quadrant", "if (a < 3073u) return neg32(ae_rds16(r, SDK_SIN_T2 + (a << 1)));", "if (a < 3080u) return neg32(ae_rds16(r, SDK_SIN_T2 + (a << 1)));"),
    ("sdk rcos table", "return neg32(ae_rds16(r, SDK_COS_T3 + (a << 1)));", "return neg32(ae_rds16(r, SDK_COS_T3 + 2 + (a << 1)));"),
    ("sdk RotMatrixYXZ m01", "ae_wr16(r, m + 2, (uint32_t)((uint32_t)t7 + (uint32_t)t9));", "ae_wr16(r, m + 2, (uint32_t)((uint32_t)t7 - (uint32_t)t9));"),
    ("sdk RotMatrixYXZ B", "int32_t B = sra32(mul_s(cy, sx), 12);", "int32_t B = sra32(mul_s(cy, sx), 11);"),
]


# the functions a mutation can influence (by the first word of its label); the other families are skipped to save time
MUT_FUNCS = {
    "Hesitate": ["MonkeyRandomHesitate"], "AdvanceAnimFrame": ["MonkeyAdvanceAnimFrame"], "PickAnim": ["MonkeyPickAnim"],
    "AnimFinished": ["MonkeyAnimFinished"], "AnimAtStart": ["MonkeyAnimAtStart"], "Fire6": ["MonkeyFireProjectile6"],
    "Fire7": ["MonkeyFireProjectile7"], "Fire8": ["MonkeyFireProjectile8"], "Aim": ["MonkeyAimAtTarget"],
    "Aimed": ["MonkeyFireProjectileAimed"], "AngleToTarget": ["MonkeyAngleToTarget"], "ApplyMoveDir": ["MonkeyApplyMoveDir"],
    "NavNodeGetPos": ["NavNodeGetPos"], "SightRange": ["MonkeyTargetInSightRange"], "BodyOff": ["MonkeyBodyOff"],
    "BodyOn": ["MonkeyBodyOn"], "DefeatBurst": ["MonkeyDefeatBurst"], "FootDust": ["MonkeyFootDust"],
    "Debris": ["MonkeyDebrisBurst6", "MonkeyDebrisBurst1"], "AlertPuff": ["MonkeyAlertPuff"], "Contact": ["MonkeyUpdateContact"],
    "Project": ["MonkeyProjectToScreen"], "AlertIcon": ["MonkeyAlertIconKind"], "SelectLod": ["MonkeySelectLod"],
    "PickGoal": ["MonkeyPickGoalNode"], "LoadLinkExtra": ["MonkeyLoadLinkExtra"], "Lookup": ["NavLinkLookupExtra"],
    "MoveStep": ["MonkeyMoveStep"], "Turn": ["MonkeyTurnTowardTarget"], "LOS": ["MonkeyLineOfSight"],
    "sdk": None,
}


def run_mutations(scale, seed, only_labels=()):
    src_files = {"src/monkeys/ae_monkey_level1.c": ROOT / "src/monkeys/ae_monkey_level1.c",
                 "src/core/ae_sdk.c": ROOT / "src/core/ae_sdk.c"}
    survivors = []
    tmp = Path(tempfile.mkdtemp(prefix="mk2_mut_"))
    h = mh.MonkeyHarness(FUNCS)
    for k, (label, old, new) in enumerate(MUTATIONS):
        if only_labels and not any(s in label for s in only_labels):
            continue
        rel = "src/core/ae_sdk.c" if label.startswith("sdk") else "src/monkeys/ae_monkey_level1.c"
        text = src_files[rel].read_text(encoding="utf-8")
        if old not in text:
            print(f"MUTATION {label!r}: pattern not found")
            survivors.append(label + " (pattern missing)")
            continue
        mp = tmp / f"mut{k}_{Path(rel).name}"
        mp.write_text(text.replace(old, new, 1), encoding="utf-8")
        h.reload_c({rel: mp}, tag=f"mk2mut{k}")
        h.failures = 0
        h.stop_after = 1
        only = MUT_FUNCS.get(label.split()[0])
        if label.startswith("sdk RotMatrixYXZ"):
            only = ["RotMatrixYXZ (SDK)", "MonkeyTurnTowardTarget"]
        elif label.startswith("sdk rsin"):
            only = ["rsin (SDK)", "MonkeyAimAtTarget", "MonkeyMoveStep"]
        elif label.startswith("sdk rcos"):
            only = ["rcos (SDK)", "MonkeyAimAtTarget", "MonkeyMoveStep"]
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
        print(f"mutation {label:34s} {'SURVIVED (checker too weak)' if survived else 'caught'}")
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
