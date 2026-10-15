"""Differential check of monkey port batch 3a (src/monkeys/ae_monkey_level2.c, call-graph level 2) against the original MIPS
code: for every ported function many scenarios are built in a byte-exact RAM image (SCUS_944.23 at its real addresses; monkeys /
nav nodes / MonkeyInfo / animation tables / objects / globals laid out as MonkeySpawnAll leaves them, plus randomised and
edge-case states); the original instructions (GTE through the PCSX-ReARMed oracle) and the C port run on identical images with
identical hook behaviour; return value, RAM outside the stack window, scratchpad and the sequence of hook calls must be
identical.  The batch 1 / 2 functions the code calls run as the C ports on the C side and as the original instructions on the
MIPS side (so this also checks them in context).  Needs extract/disc/SCUS_944.23 (skips if missing).  See
tools/mips/monkey_harness.py, monkey_scen.py and check_monkey_level1.py.

Usage: uv run python tools/mips/check_monkey_level2.py [--cases PERCENT] [--seed S] [--coverage] [--trapv] [--mutate]
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
from monkey_scen import (combat, navpos_world, navpos_extras, los_world, body_world, link_pair, extra_records, ANIM_BASE, FRAME,  # noqa: E402
                         STATE_TABLE, VEC_OUT, M32, s32, s16, u16, G_TARGET)

# name -> (original address, size, return kind: s = int32 / u = uint32 / v = void, takes hooks, argument kinds[, C name])
FUNCS = {
    "MonkeySetState": (0x80033220, 216, "v", False, "u"),
    "MonkeyUpdateLinkSlope": (0x80035370, 260, "v", True, ""),
    "MonkeyUpdateGoalPos": (0x80035474, 360, "v", True, ""),
    "MonkeyEnterState_32": (0x80035918, 156, "v", False, "u"),
    "MonkeyEnterState_3D": (0x800360D8, 84, "v", True, ""),
    "MonkeyEnterState_3E": (0x8003612C, 72, "v", False, ""),
    "MonkeyEnterState_3F": (0x80036174, 64, "v", False, ""),
    "MonkeyEnterState_17": (0x8003641C, 68, "v", True, ""),
    "MonkeyEnterState_2A": (0x80036460, 92, "v", True, ""),
    "MonkeyPrepareJumpArc": (0x800364BC, 1192, "v", True, ""),
    "MonkeyTryAttackKind": (0x8003710C, 640, "s", True, "s"),
    "MonkeyNavLinkAllowed": (0x80037E8C, 456, "s", True, "ss"),
    "MonkeyDrawAlertIcon": (0x80039C38, 536, "v", True, ""),
    "MonkeyScoreLinksByTarget": (0x8003AC54, 328, "v", True, "s"),
    "MonkeyNodeTargetCosines": (0x8003B534, 320, "v", True, "uu"),
    "MonkeyUpdateFacing": (0x8003C204, 868, "v", True, ""),
    "MonkeyFireProjectileByKind": (0x8003CD24, 96, "v", True, "s"),
    "MonkeyUpdateSight": (0x800405C4, 556, "s", True, "u"),
    "MonkeyGetWaypointPos": (0x80040CFC, 148, "s", True, "s"),
}
# code reached through the functions above (for the coverage report)
EXTRA_COVERAGE = {
    "MonkeyPickAnim (batch 2)": (0x800331B0, 112), "MonkeyFootDust (batch 2)": (0x80038F00, 216),
    "NavNodeGetPos (batch 2)": (0x80034EF8, 392), "MonkeyBodyOn (batch 2)": (0x80036B78, 88),
    "MonkeyLineOfSight (batch 2)": (0x800401F4, 976), "NavLinkLookupExtra (batch 2)": (0x8003B880, 320),
    "MonkeyApplyMoveDir (batch 2)": (0x80034834, 152), "MonkeyAlertIconKind (batch 2)": (0x80039B90, 168),
    "MonkeyAimAtTarget (batch 2)": (0x80033E4C, 1104), "MonkeyFireProjectile6 (batch 2)": (0x80033DF0, 92),
    "normalize core (SDK)": (0x80030210, 0xC0), "Square0 (SDK)": (0x80014498, 0x28),
    "SquareRoot0 (SDK)": (0x800303C0, 0x84), "ratan2 (SDK)": (0x800145D0, 0xB4), "ApplyMatrixSV (SDK)": (0x800300B0, 0x5C),
    "RotMatrixYXZ (SDK)": (0x80030FA0, 0x28C), "rsin (SDK)": (0x8002FB50, 200), "rcos (SDK)": (0x8002FC20, 160),
    "MonkeyRand (batch 1)": (0x80032E30, 40), "MonkeyGetAnimIndex (batch 1)": (0x80032E84, 160),
    "MonkeyQueueVoice (batch 1)": (0x80033744, 128), "MonkeyAttackChance (batch 1)": (0x80037068, 0x120),
    "MonkeyGetCallVoice (batch 1)": (0x800356C0, 0x90), "NavNodeLinkFlag (batch 1)": (0x80038608, 140),
    "NavNodeGetLink (batch 1)": (0x8003386C, 0x120), "NavGroundHeightBetween (batch 1)": (0x80040BDC, 0x120),
    "GimmickAllowsMonkeyPassage (batch 1)": (0x800335D0, 0x138),
    "MonkeyFacingFromLinkCode (batch 1)": (0x80036BD0, 0xB4), "NavNodeGetLinkIdsEx (batch 1)": (0x80034C08, 0x128),
    "DotXZ_SV (batch 1)": (0x800348CC, 116), "DotXZ_V (batch 1)": (0x80034940, 116), "Distance3D (batch 1)": (0x80034684, 184),
    "DistanceXZ (batch 1)": (0x8003473C, 156), "AngleBetweenXZ (batch 1)": (0x800344C8, 148),
    "Atan2Angle4096 (batch 1)": (0x800349B4, 64), "MonkeyIsState1E_to_20 (batch 1)": (0x800339CC, 32),
    "AreaIsBossArena (batch 1)": (0x800339EC, 80), "MonkeyIsAttackState (batch 1)": (0x80033C80, 76),
    "MonkeyIsIdleActivityState (batch 1)": (0x80033B10, 140), "MonkeyIsState0C_or_24 (batch 1)": (0x80033C00, 52),
    "MonkeyPuff5 (batch 1)": (0x8003929C, 72), "MonkeyBodyRegister (batch 1)": (0x80033008, 96),
}

class CCalc:
    """The validated C ports of batch 1 (Atan2Angle4096, Distance3D, DistanceXZ, DotXZ_V) on the C side's RAM, used only to aim
    scenarios at exact thresholds (the checks themselves always compare against the original instructions)."""
    A, B = 0x801F0400, 0x801F0420

    def __init__(self, h):
        import ctypes as C
        h.m.restore(h.base)
        h.sync_to_c()                      # the SDK tables are read from the RAM image
        self.lib, self.ram = h.c.lib, h.c.ram
        P, u32, i32 = C.c_void_p, C.c_uint32, C.c_int32
        self.lib.ae_Atan2Angle4096.argtypes = [P, i32, i32]
        self.lib.ae_Atan2Angle4096.restype = i32
        for nm in ("ae_Distance3D", "ae_DistanceXZ", "ae_DotXZ_V"):
            getattr(self.lib, nm).argtypes = [P, u32, u32]
            getattr(self.lib, nm).restype = i32
        self.cache = {}

    def vec(self, addr, v):
        for k in range(3):
            self.lib.ae_wr32(self.ram, addr + 4 * k, v[k] & M32)

    def atan2(self, y, x): return self.lib.ae_Atan2Angle4096(self.ram, y, x)

    def two(self, fn, a, b):
        self.vec(self.A, a)
        self.vec(self.B, b)
        return getattr(self.lib, fn)(self.ram, self.A, self.B)

    def dist3(self, a, b): return self.two("ae_Distance3D", a, b)
    def distxz(self, a, b): return self.two("ae_DistanceXZ", a, b)
    def dotxzv(self, a, b): return s16(self.two("ae_DotXZ_V", a, b))

    def slope_pairs(self):
        """(dx, dy) with dz = 0 for which MonkeyUpdateLinkSlope's quotient 100 * XZ / 3D is exactly 10 / 11 / 12"""
        if "slope" not in self.cache:
            res = {10: [], 11: [], 12: []}
            for dx in range(1, 90):
                for dy in range(max(1, dx * 100 // 12 - 4), dx * 100 // 10 + 6):
                    d3 = self.dist3((0, 0, 0), (dx, dy, 0))
                    xz = self.distxz((0, 0, 0), (dx, dy, 0))
                    if d3 and xz * 100 // d3 in res:
                        res[xz * 100 // d3].append((dx, dy))
            self.cache["slope"] = res
        return self.cache["slope"]

    def arc_pairs(self):
        """(s1, s5) for which Atan2Angle4096(s1, s5) is each of the angles around the flat / steep switch (s0 = (angle + 1024) / 2 < 1002:
        angle 979..982) and the drop path (angle of (-s1, s5): 70 % = 341 at angle 487..489)"""
        if "arc" not in self.cache:
            res = {}
            import math
            for target in list(range(976, 986)) + list(range(484, 492)):
                lst = []
                for s5 in range(12, 400, 3):
                    guess = s5 * math.tan(target * 2 * math.pi / 4096)
                    for s1 in range(int(guess) - 3, int(guess) + 4):
                        if s1 >= 100 and self.atan2(s1, s5) == target:
                            lst.append((s1, s5))
                res[target] = lst
            self.cache["arc"] = res
        return self.cache["arc"]

    def angvec(self, angle):
        """(x, z) with Atan2Angle4096(z, x) == angle and a length around 3000..4096"""
        if "angvec" not in self.cache:
            self.cache["angvec"] = {}
        c = self.cache["angvec"]
        if angle not in c:
            import math
            best = None
            for L in (4096, 3000, 4000, 2896, 1500, 3500):
                for da in (0, 1, -1, 2, -2):
                    ph = (angle + da * 0.25) * 2 * math.pi / 4096
                    x, z = round(L * math.cos(ph)), round(L * math.sin(ph))
                    if self.atan2(z, x) == angle % 4096:
                        best = (x, z)
                        break
                if best:
                    break
            c[angle] = best
        return c[angle]

    def dot_vectors(self, targets=(-3688, -3687, -3686, -3685, -3684)):
        """{target: [(a, b), ...]}: s32 triples with y = 0 for which DotXZ_V(a, b) as s16 is the target (random search around an
        angle of 154 degrees; the normalisation quantises the products, so not every value is reachable by a simple sweep)"""
        key = ("dot", tuple(targets))
        if key not in self.cache:
            import math
            import random
            g = random.Random(1234)
            res = {tg: [] for tg in targets}
            for _ in range(150000):
                if all(len(v) >= 4 for v in res.values()):
                    break
                la, lb = g.choice([300, 1000, 2000, 4096, 6000]), g.choice([300, 1000, 2000, 4096, 6000])
                pa = g.uniform(0, 2 * math.pi)
                pb = pa + math.pi * g.uniform(0.84, 0.86)
                a = (round(la * math.cos(pa)), 0, round(la * math.sin(pa)))
                b = (round(lb * math.cos(pb)), 0, round(lb * math.sin(pb)))
                d = self.dotxzv(a, b)
                if d in res and len(res[d]) < 4:
                    res[d].append((a, b))
            self.cache[key] = res
        return self.cache[key]


HDR = 0x801C2000
BAL_STATES = [0x0C, 0x24]


def check_all(h, scale, seed):
    t0 = time.time()

    def n(count):
        return max(1, int(count * scale / 100))

    def rng_of(name, i):
        return random.Random(f"{seed}/{name}/{i}")

    calc = CCalc(h)

    def cur(m): return m.read32(G_CUR)

    def run(name, count, setup=None, argsf=lambda c: (), nh=False, i0=0, **kw):
        """setup(m, rng, i) -> context; one fresh scenario per case"""
        for i in range(i0, i0 + n(count)):
            rng = rng_of(name, i)
            h.run(name, f"#{i}", i, (lambda m, rng=rng, i=i: setup(m, rng, i)) if setup else (lambda m, rng=rng: combat(m, rng)),
                  (lambda c, rng=rng, i=i: argsf(c, rng, i)) if argsf.__code__.co_argcount == 3 else argsf,
                  null_hooks=(nh and i % 8 == 7), **kw)

    def swept_states(name, count, setup, argsf=lambda c, rng, i: (), nh=False, **kw):
        """the state byte is swept 0..0x45 (not scaled) and then random"""
        for i in range(0x46 + n(count)):
            rng = rng_of(name, i)
            st = i if i < 0x46 else rng.choice([rng.randint(0, 0x42)] * 3 + [rng.getrandbits(8)])
            h.run(name, f"state {st} #{i}", i, lambda m, rng=rng, i=i, st=st: setup(m, rng, i, st), lambda c, rng=rng, i=i: argsf(c, rng, i),
                  null_hooks=(nh and i % 8 == 7), **kw)

    def plain_nodes(m, rng, nn):
        """make every node a plain one (position = the s16 triple at +4 / +6 / +8)"""
        w = W(m)
        for k in range(nn):
            a = NAV_BASE + 0x18 * k
            w.w32(a, m.read32(a) & ~0x401 & M32)

    def node_set(m, c, rng, nn):
        w = W(m)
        w.w16(c + 0xF2, rng.randint(0, max(0, nn - 1)) if rng.random() < 0.95 else rng.getrandbits(16))
        w.w16(c + 0xF4, rng.randint(0, max(0, nn - 1)) if rng.random() < 0.95 else rng.getrandbits(16))

    # ------------------------------------------------------------------ MonkeySetState
    def set_state_setup(m, rng, i):
        c = combat(m, rng)
        w = W(m)
        w.w32(0x800B4D14, HDR)
        w.w32(HDR + 0x18, rng.choice([0, 0x100, 0x200, 0x7F8]))
        w.w32(HDR + 0x1C, rng.choice([0, 0x100, 0x200, 0x3F0]))
        w.w32(0x800F4590, ANIM_BASE)
        s1, s2 = rng.choice([0x800AE000, 0x801C3000, 0x1234, 0]), rng.choice([0x800AF000, 0x801C3100, 0xABCD, 0])
        w.w32(0x800F4504, s1)
        w.w32(0x800F4508, s2)
        w.w32(c + 0x18, rng.choice([s1, s2, s1, rng.getrandbits(32)]))
        st = rng.choice([rng.randint(0, 0x45), rng.randint(0x30, 0x36), rng.getrandbits(8)])
        w.w8(c + 0x78, st)
        return {"st": st}
    for i in range(0x48 + n(500)):
        rng = rng_of("MonkeySetState", i)
        h.run("MonkeySetState", f"#{i}", i, lambda m, rng=rng, i=i: set_state_setup(m, rng, i),
              lambda c, rng=rng, i=i: ((i if i < 0x48 else rng.choice([c["st"], c["st"], rng.randint(0, 0x45), rng.getrandbits(32),
                                                                      rng.getrandbits(8) | 0x100, 0x32, 0x33, 0x34, 0x31, 0x35])) & M32,),
              allow_trap=True)

    # ------------------------------------------------------------------ small state entries
    def enter_setup(m, rng, i, st=None):
        c = combat(m, rng, state=st)
        w = W(m)
        if rng.random() < 0.3:
            w.w8(c + 0x78, rng.choice([0x3D, 0x3E, 0x3F, 0x17, 0x2A, rng.randint(0, 0x42)]))
        return c
    def enter_own(m, rng, i, own):
        c = enter_setup(m, rng, i)
        if rng.random() < 0.4:
            W(m).w8(c + 0x78, own)
        return c
    for name, own in (("MonkeyEnterState_3D", 0x3D), ("MonkeyEnterState_3E", 0x3E), ("MonkeyEnterState_3F", 0x3F),
                      ("MonkeyEnterState_17", 0x17)):
        run(name, 200, lambda m, rng, i, own=own: enter_own(m, rng, i, own), nh=True, allow_trap=True)

    def enter2a(m, rng, i):
        c = body_world(m, rng)
        if rng.random() < 0.3:
            W(m).w8(c + 0x78, 0x2A)
        return c
    run("MonkeyEnterState_2A", 300, enter2a, nh=True, allow_trap=True)

    # body_world deliberately leaves the animation pointer random. Supply a valid
    # table as well so PickAnim completes and the hop/body-on tail is exercised.
    def enter2a_complete(m, rng, i):
        c = body_world(m, rng)
        w = W(m)
        w.w8(c + 0x78, 1)
        sc.anim_tables(m, rng, c)
        w.w8(c + 0xE3, 91)
        w.w16(c + 0xE4, 0x1234)
        w.w32(c + 0x28, (-321 if i & 1 else 456) & M32)
        return c
    run("MonkeyEnterState_2A", 20, enter2a_complete, nh=True, allow_trap=True, i0=30000)

    def enter32_setup(m, rng, i):
        c = navpos_world(m, rng)
        w = W(m)
        w.w32(0x800B4D14, HDR)
        w.w32(HDR + 0x18, rng.choice([0, 0x100]))
        w.w32(HDR + 0x1C, rng.choice([0, 0x100, 0x200]))
        w.w32(0x800F4590, ANIM_BASE)
        w.w32(0x800F4508, rng.choice([0x800AF000, 0x1234]))
        nn = m.read16(0x800B4D18)
        for k in range(nn):                     # the link code fields: hundreds select the code, the code table is random
            a = NAV_BASE + 0x18 * k
            if rng.random() < 0.7:
                w.w16(a + 0x10, rng.randint(0, 9) * 100 + rng.randint(0, 99))
                w.w16(a + 0x12, rng.randint(0, 9) * 100 + rng.randint(0, 99))
        for j in range(0, 0x20, 2):
            w.w16(0x800AEB6C + j, rng.choice([rng.randint(0, 8), rng.randint(0, 3), rng.getrandbits(16)]))
        return c
    run("MonkeyEnterState_32", 500, enter32_setup, lambda c, rng, i: (rng.choice([rng.randint(0, 6), rng.randint(0, 39), rng.randint(-2, 42), rng.getrandbits(32)]) & M32,),
        allow_trap=True)

    # ------------------------------------------------------------------ link slope / goal position
    def slope_setup(m, rng, i):
        c = navpos_world(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        node_set(m, c, rng, nn)
        n0, n1 = m.read16(c + 0xF2), m.read16(c + 0xF4)
        if rng.random() < 0.8 and n0 < nn and n1 < nn:
            plain_nodes(m, rng, nn)
            a0, a1 = NAV_BASE + 0x18 * n0, NAV_BASE + 0x18 * n1
            mode = rng.choice(["shallow", "shallow", "steep", "flat", "same", "edge", "exact", "exact", "exact"])
            p0 = [rng.randint(-1500, 1500) for _ in range(3)]
            if mode == "shallow":
                d = [rng.randint(-8, 8), rng.choice([rng.randint(20, 800), rng.randint(-800, -20)]), rng.randint(-8, 8)]
            elif mode == "steep":
                d = [rng.randint(-500, 500), rng.randint(-100, 100), rng.randint(-500, 500)]
            elif mode == "flat":
                d = [rng.randint(-500, 500), 0, rng.randint(-500, 500)]
            elif mode == "same":
                d = [0, 0, 0]
            elif mode == "exact":
                dx_, dy_ = rng.choice(calc.slope_pairs()[rng.choice([10, 11, 11, 12])])
                d = [rng.choice([dx_, -dx_]), rng.choice([dy_, -dy_]), 0]
            else:
                k = rng.randint(1, 3000)
                d = [k, rng.randint(-5, 5) + rng.choice([0, k * 10, -k * 10]), rng.randint(-3, 3)]
            for k in range(3):
                w.w16(a0 + 4 + 2 * k, p0[k] & 0xFFFF)
                w.w16(a1 + 4 + 2 * k, (p0[k] + d[k]) & 0xFFFF)
        return c
    run("MonkeyUpdateLinkSlope", 800, slope_setup, nh=True, allow_trap=True)

    def goal_setup(m, rng, i):
        c = navpos_world(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        node_set(m, c, rng, nn)
        w.w8(c + 0x78, rng.choice([30, 30, 9, 9, 3, rng.randint(0, 0x42)]))
        w.w8(c + 0x24B, rng.getrandbits(8))
        w.w32(c + 0x228, rng.choice([0, 2, 3, rng.getrandbits(32)]))
        w.w8(c + 0x22E, rng.choice([0, 1, 1, 2, rng.getrandbits(8)]))
        for k in range(nn):
            a = NAV_BASE + 0x18 * k
            if rng.random() < 0.4:
                w.w32(a, m.read32(a) | 0x40)
        if rng.random() < 0.1:
            w.w16(0x800B4D18, rng.choice([0, 1]))
        # a goal position around: the old +0xBC values stay random
        return c
    run("MonkeyUpdateGoalPos", 900, goal_setup, nh=True, allow_trap=True)

    def goal_drop_boundary(m, rng, i):
        # Pure vertical separation makes a one-unit slope correction observable
        # in Distance3D instead of disappearing in integer square-root rounding.
        c = navpos_world(m, rng)
        w = W(m)
        w.w16(c + 0xF2, 0)
        w.w32(NAV_BASE, 0)
        for k in range(3):
            w.w16(NAV_BASE + 4 + 2 * k, 0)
            w.w32(c + 0x24 + 4 * k, 0)
        w.w8(c + 0x78, 9)
        w.w32(c + 0x228, 2)
        w.w8(c + 0x22E, 1)
        return c
    run("MonkeyUpdateGoalPos", 4, goal_drop_boundary, i0=31000)

    # ------------------------------------------------------------------ the jump arc
    def arc_setup(m, rng, i):
        c = navpos_world(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        node_set(m, c, rng, nn)
        n0, n1 = m.read16(c + 0xF2), m.read16(c + 0xF4)
        if rng.random() < 0.9 and n0 < nn and n1 < nn:
            plain_nodes(m, rng, nn)
            a0, a1 = NAV_BASE + 0x18 * n0, NAV_BASE + 0x18 * n1
            np0 = [rng.randint(-1500, 1500) for _ in range(3)]
            np1 = [rng.randint(-1500, 1500) for _ in range(3)]
            for k in range(3):
                w.w16(a0 + 4 + 2 * k, np0[k] & 0xFFFF)
                w.w16(a1 + 4 + 2 * k, np1[k] & 0xFFFF)
            dy = rng.choice([0, 1, -1, 99, 100, 101, -99, -100, -101, rng.randint(-300, 300), rng.randint(-3000, 3000), rng.randint(-40, 40),
                             rng.randint(100, 2000), rng.randint(-2000, -100), rng.randint(-10000, 10000)])
            dh = rng.choice([0, 0, 1, 10, 100, 300, 800, 1500, rng.randint(0, 3000), rng.randint(0, 5000), 65536, 70000, rng.randint(0, 100000)])
            ang = rng.uniform(0, 6.2831853)
            import math
            dx, dz = int(dh * math.cos(ang)), int(dh * math.sin(ang))
            if rng.random() < 0.3:
                dx, dz = dh, 0
            cx, cz = np0[0] + dx, np0[2] + dz
            cy = np0[1] + dy
            for k, v in enumerate((cx, cy, cz)):
                w.w32(c + 0x24 + 4 * k, v & M32)
            w.w16(0x800B4D18, nn)
            # the goal position: near the second node, or random
            for k, v in enumerate(np1):
                w.w32(c + 0xBC + 4 * k, (v + rng.choice([0, 0, 5, -5, rng.randint(-300, 300)])) & M32)
            w.w32(FRAME, rng.choice([0, 3, 6, 9, rng.getrandbits(32)]))
        return c
    run("MonkeyPrepareJumpArc", 2500, arc_setup, nh=True, allow_trap=True)

    def arc_edge_setup(m, rng, i):
        """the flat / steep and drop thresholds exactly: (s1, s5) pairs with the angle around the switch"""
        c = navpos_world(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        node_set(m, c, rng, nn)
        n0, n1 = m.read16(c + 0xF2), m.read16(c + 0xF4)
        if n0 >= nn or n1 >= nn:
            return c
        plain_nodes(m, rng, nn)
        a0, a1 = NAV_BASE + 0x18 * n0, NAV_BASE + 0x18 * n1
        np0 = [rng.randint(-1500, 1500) for _ in range(3)]
        np1 = [rng.randint(-1500, 1500) for _ in range(3)]
        for k in range(3):
            w.w16(a0 + 4 + 2 * k, np0[k] & 0xFFFF)
            w.w16(a1 + 4 + 2 * k, np1[k] & 0xFFFF)
        pairs = calc.arc_pairs()
        tgt = rng.choice(sorted(pairs))
        if not pairs[tgt]:
            return c
        s1, s5 = rng.choice(pairs[tgt])
        if tgt < 700:
            s1 = -s1                                    # the drop path: atan2(-s1, s5)
        cx, cy, cz = np0[0] + s5, np0[1] + s1, np0[2]
        for k, v in enumerate((cx, cy, cz)):
            w.w32(c + 0x24 + 4 * k, v & M32)
        for k, v in enumerate(np1):
            w.w32(c + 0xBC + 4 * k, v & M32)
        return c
    run("MonkeyPrepareJumpArc", 600, arc_edge_setup, nh=True, allow_trap=True, i0=20000)

    # ------------------------------------------------------------------ attacks
    def attack_setup(m, rng, i):
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
            if rng.random() < 0.5:                   # keep the distance inside this kind's window
                w.w16(e + 0x22, min(dist & 0xFFFF, 0x7FFF) - rng.choice([0, 0, 1, 10]) if dist < 0x7FFF else lo)
                w.w16(e + 0x24, min(dist & 0xFFFF, 0x7FFF) + rng.choice([0, 0, 1, 10]) if dist < 0x7FFF else hi)
        w.w16(info + 0x58, rng.choice([0, 1, 50, 200, rng.getrandbits(16)]))
        w.w8(c + 0xD4, rng.choice([0, 0, 1, 2, 0x80, 0xFF, 0x7F, rng.getrandbits(8)]))
        w.w16(c + 0x174, rng.choice([0, 0, 1, 90, rng.getrandbits(16)]))
        w.w8(c + 0x78, rng.choice([rng.randint(0, 0x42), 0x1E, 0x1F, 0x20, 3, 8, rng.getrandbits(8)]))
        # the attack table (kind -> {count multiplier, state, flags}) is EXE data: make it vary
        if rng.random() < 0.6:
            for k in range(8):
                w.w8(0x800AEBB4 + 8 * k, rng.choice([1, 2, 3, 5, 0, rng.getrandbits(8)]))
                w.w8(0x800AEBB5 + 8 * k, rng.choice([rng.randint(0, 0x42), 0x1E, 0x20, rng.getrandbits(8)]))
                w.w32(0x800AEBB8 + 8 * k, rng.choice([0, 1, rng.getrandbits(32)]))
        # the random number: make the percentage hit the chance of the kind sometimes
        w.w32(0x800B4D10, rng.getrandbits(16) if rng.random() < 0.9 else rng.getrandbits(32))
        return c
    run("MonkeyTryAttackKind", 2500, attack_setup, nh=True, allow_trap=True,
        argsf=lambda c, rng, i: (rng.choice([0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 5, 7, 8, -1, 0x10001, 0xFFFF0007, rng.randint(0, 7), rng.randint(0, 7)]) & M32,))

    # ------------------------------------------------------------------ link allowed
    FLAG_BITS = [0x40, 0x80, 0x400, 0x1000, 0x200000, 0x400000, 0x40000]

    def allowed_setup(m, rng, i):
        c = navpos_world(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        pair = link_pair(m, rng)
        if pair is None and nn > 1:
            pair = (rng.randint(0, nn - 1), rng.randint(0, nn - 1))
        flagsel = [sum(b for b in FLAG_BITS if rng.random() < 0.3) | rng.choice([0, 0, 0x2000]) for _ in range(8)] + \
                  [b for b in FLAG_BITS] + [0]
        pr = extra_records(m, rng, pair, flagsel)
        # the value (record +8): a gimmick id / a distance / a monkey id / an object slot
        data = bytearray(m.read_bytes(sc.EXTRA_BASE, 12 * m.read16(0x800B4D20)))
        for e in range(m.read16(0x800B4D20)):
            val = rng.choice([rng.randint(0, 20), rng.randint(0, 3000), rng.randint(0, 15), rng.getrandbits(16), rng.getrandbits(32)])
            data[12 * e + 8:12 * e + 12] = struct.pack("<I", val & M32)
        m.write_bytes(sc.EXTRA_BASE, bytes(data) + bytes(16))
        w.w32(0x800B4D1C, sc.EXTRA_BASE)
        w.w8(c + 0x7C, rng.choice([rng.randint(0, 15), rng.randint(0, 15), 3]))
        w.w8(c + 0x79, rng.choice([0, 1, 1, 2, rng.getrandbits(8)]))
        to = pr[1]
        if to < nn and rng.random() < 0.5:
            # the distance limit exactly (the value of the record = Distance3D(monkey, node) +- 1) with a plain node
            a = NAV_BASE + 0x18 * to
            w.w32(a, m.read32(a) & ~0x401 & M32)
            np_ = [m.reads16(a + 4), m.reads16(a + 6), m.reads16(a + 8)]
            cp = [m.reads32(c + 0x24), m.reads32(c + 0x28), m.reads32(c + 0x2C)]
            if rng.random() < 0.7:
                cp = [np_[0] + rng.randint(-300, 300), np_[1] + rng.randint(-300, 300), np_[2] + rng.randint(-300, 300)]
                for k in range(3):
                    w.w32(c + 0x24 + 4 * k, cp[k] & M32)
            d3 = calc.dist3(cp, np_)
            n_rec = m.read16(0x800B4D20)
            for e in range(n_rec):
                lo, hi = m.reads16(sc.EXTRA_BASE + 12 * e), m.reads16(sc.EXTRA_BASE + 12 * e + 2)
                if (lo, hi) == (min(pr), max(pr)):
                    m.write32(sc.EXTRA_BASE + 12 * e + 4, m.read32(sc.EXTRA_BASE + 12 * e + 4) | 0x400)
                    m.write32(sc.EXTRA_BASE + 12 * e + 8, (d3 + rng.choice([0, 0, 0, 1, -1])) & M32)
        if to < nn:
            a = NAV_BASE + 0x18 * to
            if rng.random() < 0.5:
                w.w32(a, m.read32(a) | 0x100)
            if rng.random() < 0.5:
                w.w16(c + 0xE6, to)
        return {"pair": pr, "cur": c}

    def allowed_args(c, rng, i):
        a, b = c["pair"]
        if rng.random() < 0.15:
            a, b = b, a
        if rng.random() < 0.3:
            a |= rng.getrandbits(16) << 16
        if rng.random() < 0.3:
            b |= rng.getrandbits(16) << 16
        return (a & M32, b & M32)
    run("MonkeyNavLinkAllowed", 2500, allowed_setup, allowed_args, nh=True, allow_trap=True)

    # ------------------------------------------------------------------ alert icon
    def icon_setup(m, rng, i):
        c = combat(m, rng, positions=False)
        w = W(m)
        w.w8(c + 0x79, rng.choice([0, 1, 1, 2, 3, 255, rng.getrandbits(8)]))
        w.w8(c + 0x249, rng.choice([0, 1, 2, 3, 255, rng.getrandbits(8)]))
        w.w32(c + 0x248, rng.choice([0, 1 << 19, rng.getrandbits(32), 0xFFFFFFFF, ~(1 << 19) & M32]))
        w.w8(c + 0xD5, rng.choice([0, 1, 2, 3, 4, 5, 255, 0x80, rng.getrandbits(8)]))
        w.w16(c + 0x150, rng.choice([0, 1, -1, 5, 10, 0x7FFF, 0x8000, rng.getrandbits(16)]))
        w.w32(c + 0x14C, rng.choice([0, 448, 896, 1, rng.getrandbits(32), m.read32(c + 0x14C)]))
        for off in range(0, 0x20, 2):
            w.w16(0x800F47F0 + off, rng.choice([0x1000, 0, -0x1000 & 0xFFFF, 0x0B50, 0x7FFF, 0x8000, rng.getrandbits(16)]))
        for off in (0x14, 0x18, 0x1C):
            w.w16(0x800F47F0 + off, rng.getrandbits(16))
        st = rng.choice([rng.randint(0, 0x42), 3, 8, rng.getrandbits(8)])
        w.w8(c + 0x78, st)
        return c
    def icon_st(m, rng, i, st):
        c = icon_setup(m, rng, i)
        W(m).w8(c + 0x78, st)
        return c
    swept_states("MonkeyDrawAlertIcon", 900, icon_st, nh=True, allow_trap=True)

    # ------------------------------------------------------------------ link scoring / cosines / waypoint
    def score_setup(m, rng, i):
        c = navpos_world(m, rng)
        w = W(m)
        nn = m.read16(0x800B4D18)
        for k in range(nn):
            a = NAV_BASE + 0x18 * k
            w.w8(a + 0xB, rng.choice([0, 1, 2, 3, 4, 6, 8, rng.randint(0, 8), 20]))
            if rng.random() < 0.8:
                w.w8(a + 0xA, max(m.read8(a + 0xA), m.read8(a + 0xB)))
        return c
    run("MonkeyScoreLinksByTarget", 900, score_setup, nh=True, allow_trap=True,
        argsf=lambda c, rng, i: (rng.choice([rng.randint(0, 5), rng.randint(0, 39), rng.randint(-2, 42), rng.getrandbits(32)]) & M32,))

    def cos_setup(m, rng, i):
        c = navpos_world(m, rng)
        node_set(m, c, rng, m.read16(0x800B4D18))
        return c
    run("MonkeyNodeTargetCosines", 700, cos_setup, nh=True, allow_trap=True,
        argsf=lambda c, rng, i: (VEC_OUT, VEC_OUT + rng.choice([2, 4, 0x40, 0])))

    def way_setup(m, rng, i):
        c = navpos_world(m, rng)
        node_set(m, c, rng, m.read16(0x800B4D18))
        return c
    run("MonkeyGetWaypointPos", 800, way_setup, nh=True, allow_trap=True,
        argsf=lambda c, rng, i: (rng.choice([0, 0, 1, -1, 0x10000, 0x10001, 5, rng.getrandbits(32)]) & M32,))

    run("MonkeyFireProjectileByKind", 400, nh=True, allow_trap=True,
        argsf=lambda c, rng, i: (rng.choice([0, 1, 5, 2, 3, 4, 6, -1, 0x10000, 0x10005, 0x10001, rng.getrandbits(32), rng.randint(-3, 9)]) & M32,))

    # ------------------------------------------------------------------ facing
    def facing_setup(m, rng, i, st=None):
        c = navpos_world(m, rng, state=st)
        w = W(m)
        nn = m.read16(0x800B4D18)
        node_set(m, c, rng, nn)
        for off in (0x60, 0x62, 0x64, 0x66, 0x68, 0x6A, 0x6C, 0x6E):
            w.w16(c + off, rng.choice([0, 4096, -4096, 2896, -2896, 1, -1, rng.getrandbits(16), rng.randint(-4096, 4096)]) & 0xFFFF)
        if st is None:
            w.w8(c + 0x78, rng.choice(BAL_STATES + [rng.randint(0, 0x42)]))
        n0, n1 = m.read16(c + 0xF2), m.read16(c + 0xF4)
        if rng.random() < 0.6 and n0 < nn and n1 < nn:
            plain_nodes(m, rng, nn)
            w = W(m)
            a0, a1 = NAV_BASE + 0x18 * n0, NAV_BASE + 0x18 * n1
            for k in range(3):
                w.w16(a0 + 4 + 2 * k, rng.randint(-1500, 1500) & 0xFFFF)
                w.w16(a1 + 4 + 2 * k, rng.randint(-1500, 1500) & 0xFFFF)
            # make the wish direction roughly (anti)parallel to the node direction sometimes (the 0x0C / 0x24 path tests the dot product)
            if rng.random() < 0.5:
                dxn = m.reads16(a0 + 4) - m.reads16(a1 + 4)
                dzn = m.reads16(a0 + 8) - m.reads16(a1 + 8)
                for (ox, oz) in ((0x68, 0x6C), (0x60, 0x64)):
                    sgn = rng.choice([1, -1])
                    sc_ = rng.choice([1, 2, 4])
                    w.w16(c + ox, (sgn * dxn * sc_ // 4) & 0xFFFF)
                    w.w16(c + oz, (sgn * dzn * sc_ // 4) & 0xFFFF)
        return c
    swept_states("MonkeyUpdateFacing", 2000, lambda m, rng, i, st: facing_setup(m, rng, i, st), nh=True, allow_trap=True)
    run("MonkeyUpdateFacing", 1500, facing_setup, nh=True, allow_trap=True, i0=7000)

    def facing12(m, rng, i):
        c = facing_setup(m, rng, i)
        W(m).w8(c + 0x78, rng.choice(BAL_STATES))
        return c
    run("MonkeyUpdateFacing", 1500, facing12, nh=True, allow_trap=True, i0=9000)

    def facing_angle(m, rng, i):
        """first - second angle exactly at +-2048 / +-2049 (the wrap), the wished direction along +x (angle 0) or at a known angle"""
        c = navpos_world(m, rng, state=rng.choice([1, 2, 3, 4, 5, 6, 7, 10, 13, 14]))
        w = W(m)
        first = rng.choice([2047, 2048, 2049, 2050, 0, 1, 2, 100, 4095, 3000, 2049, 2049, rng.randint(0, 4095)])
        second_angle = rng.choice([0, 0, 0, 0, 1, 1024, 2049, 2048, 4095, rng.randint(0, 4095)])
        old = calc.angvec(first)
        wish = (4096, 0) if second_angle == 0 else calc.angvec(second_angle)
        if old is None or wish is None:
            return c
        w.w16(c + 0x60, old[0] & 0xFFFF)
        w.w16(c + 0x62, 0)
        w.w16(c + 0x64, old[1] & 0xFFFF)
        # MonkeyCalcMoveDir: wish = normalise(goal - position); the wished direction is set through the goal position
        pos = [rng.randint(-2000, 2000) for _ in range(3)]
        for k in range(3):
            w.w32(c + 0x24 + 4 * k, pos[k] & M32)
        for k, dv in enumerate((wish[0], 0, wish[1])):
            w.w32(c + 0xBC + 4 * k, (pos[k] + dv) & M32)
        return c
    run("MonkeyUpdateFacing", 700, facing_angle, nh=True, allow_trap=True, i0=11000)

    def facing_dot(m, rng, i):
        """state 12 / 36 with the dot product of the old move direction and the node direction at -3686 +- 1"""
        c = navpos_world(m, rng, state=rng.choice(BAL_STATES))
        w = W(m)
        nn = m.read16(0x800B4D18)
        node_set(m, c, rng, nn)
        n0, n1 = m.read16(c + 0xF2), m.read16(c + 0xF4)
        if n0 >= nn or n1 >= nn or n0 == n1:
            return c
        plain_nodes(m, rng, nn)
        tgt = rng.choice([-3688, -3687, -3686, -3685, -3684, -3686])
        sols = calc.dot_vectors()[tgt]
        if not sols:
            return c
        (v1, v2) = rng.choice(sols)
        a0, a1 = NAV_BASE + 0x18 * n0, NAV_BASE + 0x18 * n1
        # node direction = node0 - node1 = v2
        base = [rng.randint(-500, 500) for _ in range(3)]
        for k in range(3):
            w.w16(a1 + 4 + 2 * k, base[k] & 0xFFFF)
            w.w16(a0 + 4 + 2 * k, (base[k] + v2[k]) & 0xFFFF)
        w.w16(c + 0x60, v1[0] & 0xFFFF)
        w.w16(c + 0x62, 0)
        w.w16(c + 0x64, v1[2] & 0xFFFF)
        return c
    run("MonkeyUpdateFacing", 400, facing_dot, nh=True, allow_trap=True, i0=12000)

    # ------------------------------------------------------------------ sight
    def sight_setup(m, rng, i):
        ctx = los_world(m, rng)
        c = cur(m)
        w = W(m)
        tgt = m.read32(G_TARGET)
        w.w32(tgt + 0x70, rng.choice([0, 1, 2, 3, 7, 0xF, 0x17, rng.getrandbits(32)]))
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        r1 = rng.choice([100, 500, 2000, 300, 0, -5, rng.getrandbits(16), rng.randint(0, 3000)])
        r2 = rng.choice([100, 500, 2000, 300, 0, -5, rng.getrandbits(16), rng.randint(0, 3000)])
        for k in range(8):
            w.w16(info + 12 + 2 * k, rng.choice([r1, r2, rng.randint(0, 3000), 0, rng.getrandbits(16)]) & 0xFFFF)
        w.w16(info + 0x1A, r2 & 0xFFFF)
        dist = m.reads32(c + 0x20C)
        a1 = 1 if m.read8(0x800B4D04) else (m.read32(tgt + 0x70) & 0xF)
        base = rng.choice([m.reads16(info + 12 + 2 * a1), m.reads16(info + 0x1A)])
        if rng.random() < 0.6:
            w.w32(c + 0x20C, (base + rng.choice([-2, -1, 0, 1, 2, 300, -300])) & M32)
        w.w8(c + 0x78, rng.choice([3, 8, 1, 2, 5, 13, rng.randint(0, 0x42)]))
        w.w32(c + 0x21C, rng.choice([0, 567, 568, 569, 3528, 3527, 4096, 456, 457, 1592, 1593, 1480, 2616, 2504, 3640, 3641, 3639,
                                     rng.randint(0, 4200), rng.getrandbits(32), 455, 454, 1479, 1478, 2503, 2502]))
        if rng.random() < 0.15:
            w.w16(0x800F4472, rng.choice([3, 6, 8]))
        elif rng.random() < 0.3:
            w.w16(0x800F4472, rng.choice([0, 1, 2, 4, 5, 7]))
            w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x35, 0x45, 0x47]))
        elif rng.random() < 0.05:
            w.w16(0x800F4476, 0x46)
        return ctx
    def sight_window(m, rng, i):
        """arg 0..4 with the link position counter exactly at the window edges, the target close, no boss area"""
        ctx = los_world(m, rng)
        c = cur(m)
        w = W(m)
        tgt = m.read32(G_TARGET)
        w.w16(0x800F4472, rng.choice([0, 1, 2, 4, 5, 7]))
        w.w16(0x800F4476, rng.choice([0x10, 0x29, 0x35, 0x45, 0x47]))
        w.w8(0x800B4D04, rng.choice([0, 1]))
        w.w32(tgt + 0x70, rng.choice([0, 1, 2, 3]))
        info = m.read32(0x800B4CF0) + 0x88 * m.read8(c + 0x7C)
        for k in range(8):
            w.w16(info + 12 + 2 * k, 5000)
        w.w16(info + 0x1A, 5000)
        w.w32(c + 0x20C, rng.randint(250, 3000))
        w.w8(c + 0x78, rng.choice([1, 2, 5, 13]))
        edges = [0, 567, 568, 569, 3527, 3528, 3529, 4095, 4096, 455, 456, 457, 1592, 1593, 1594, 1479, 1480, 1481, 1616, 2615, 2616,
                 2617, 2503, 2504, 2505, 3639, 3640, 3641, 1137, 1136, 1138]
        w.w32(c + 0x21C, rng.choice(edges))
        return ctx
    run("MonkeyUpdateSight", 1500, sight_window, nh=True, allow_trap=True, i0=30000,
        argsf=lambda c, rng, i: (rng.choice([0, 1, 2, 3, 4, 1, 2, 3, 4, 5]) & M32,))
    run("MonkeyUpdateSight", 3000, sight_setup, nh=True, allow_trap=True,
        argsf=lambda c, rng, i: (rng.choice([0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 0xFFFFFFFF, 0x10000, rng.getrandbits(32)]) & M32,))

    def sight_upper_boundary(m, rng, i):
        # Pattern 2 excludes 456 + 1137. A nearby target guarantees that
        # widening the window by one changes visibility instead of being
        # hidden by a subsequent failed line-of-sight probe.
        ctx = sight_window(m, rng, i)
        c = cur(m)
        w = W(m)
        w.w16(0x800F4472, 0)
        w.w16(0x800F4476, 0x10)
        w.w8(0x800B4D04, 0)
        w.w32(m.read32(G_TARGET) + 0x70, 0)
        w.w8(c + 0x78, 1)
        w.w32(c + 0x20C, 0)
        w.w32(c + 0x21C, 456 + 1137)
        return ctx
    run("MonkeyUpdateSight", 4, sight_upper_boundary, lambda c: (2,), i0=32000)

    print(f"scenario families done in {time.time() - t0:.1f}s")


MUTATIONS = [
    ("SetState range", 'RD8(reloaded_monkey + AE_MK_AI_STATE) - 50u < 3u', 'RD8(reloaded_monkey + AE_MK_AI_STATE) - 50u < 2u'),
    ("SetState header", 'WR32(reloaded_monkey + AE_MK_ANIM_TABLE, RD32(AE_G_AREA_DATA) + RD32(RD32(AE_G_AREA_HEADER) + AE_MK_AREA_ALT_ANIM_TABLE));', 'WR32(reloaded_monkey + AE_MK_ANIM_TABLE, RD32(AE_G_AREA_DATA) + RD32(RD32(AE_G_AREA_HEADER) + AE_MK_AREA_ANIM_TABLE));'),
    ("SetState set", "uint32_t set = RD32(AE_G_ANIM_SET);", "uint32_t set = RD32(AE_G_ANIM_SET2);"),
    ("Enter32 flags", "WR8(monkey + AE_MK_FLAGS, 0);", "WR8(monkey + AE_MK_FLAGS, 1);"),
    ("Enter32 state", "WR8(cur_monkey(r) + AE_MK_AI_STATE, 50);", "WR8(cur_monkey(r) + AE_MK_AI_STATE, 51);"),
    ("Enter32 code", "ae_MonkeyFacingFromLinkCode(r, RDS16(sp + 24));", "ae_MonkeyFacingFromLinkCode(r, RDS16(sp + 24) + 1);"),
    ("Enter3D voice", "ae_MonkeyQueueVoice(r, h, 43, 0);", "ae_MonkeyQueueVoice(r, h, 42, 0);"),
    ("Enter3D node", 'WR16(monkey + AE_MK_PREV_NODE, RD16(monkey + AE_MK_NAV_NODE1));\n    if (RD8(monkey + AE_MK_AI_STATE) == 61) return;', 'WR16(monkey + AE_MK_PREV_NODE, RD16(monkey + AE_MK_NAV_NODE0));\n    if (RD8(monkey + AE_MK_AI_STATE) == 61) return;'),
    ("Enter3E timer", 'WR16(monkey + AE_MK_ANIM_LENGTH, 50);', 'WR16(monkey + AE_MK_ANIM_LENGTH, 51);'),
    ("Enter3F node", 'uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);\n    WR8(monkey + AE_MK_AI_STATE, 63);', 'uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE0);\n    WR8(monkey + AE_MK_AI_STATE, 63);'),
    ("Enter17 voice", "ae_MonkeyQueueVoice(r, h, 70, 30);", "ae_MonkeyQueueVoice(r, h, 70, 29);"),
    ("Enter2A y", 'WR16(monkey + AE_MK_HOP_START_Y, RD16(monkey + AE_MK_POS + 4));', 'WR16(monkey + AE_MK_HOP_START_Y, RD16(monkey + AE_MK_POS));'),
    ("LinkSlope threshold", "if (q < 11) {", "if (q < 12) {"),
    ("LinkSlope updown", "ya < yb ? 1 : 2", "ya < yb ? 2 : 1"),
    ("GoalPos flag", '!(RD8(monkey + AE_MK_FLAGS + 3) & 1u)) run = 0;', '!(RD8(monkey + AE_MK_FLAGS + 3) & 2u)) run = 0;'),
    ("GoalPos drop", "WR32(sp + 36, y - 200u);", "WR32(sp + 36, y - 199u);"),
    ("GoalPos flags2", "if (RD32(m + AE_MK_FLAGS2) & 2u) {", "if (RD32(m + AE_MK_FLAGS2) & 4u) {"),
    ("GoalPos target", 'WR32(monkey + AE_MK_GOAL_POS + 8, RD32(tgt + AE_MK_POS + 8));', 'WR32(monkey + AE_MK_GOAL_POS + 8, RD32(tgt + AE_MK_POS + 4));'),
    ("GoalPos node bit", "& 0x40u) {\n            uint32_t tgt", "& 0x80u) {\n            uint32_t tgt"),
    ("JumpArc 100", 'neg32(height_delta) >= 100) {', 'neg32(height_delta) >= 101) {'),
    ("JumpArc 70", 'int32_t t = mul_lo(launch_angle, 70);', 'int32_t t = mul_lo(launch_angle, 140);'),
    ("JumpArc 341", 'if (launch_angle < 341) launch_angle = 341;', 'if (launch_angle < 340) launch_angle = 340;'),
    ("JumpArc 1002", 'if (!(launch_angle < 1002)) {', 'if (!(launch_angle < 1003)) {'),
    ("JumpArc steep", 'int32_t vv = shift_left32(add32(height_delta, 2700), 12);', 'int32_t vv = shift_left32(add32(height_delta, 2701), 12);'),
    ("JumpArc 1080000", 'int32_t scaled_cosine = mul_lo(launch_cosine, 1080000);', 'int32_t scaled_cosine = mul_lo(launch_cosine, 2160000);'),
    ("JumpArc 60", 'int32_t horizontal_speed = sub32(sra32(add32(hi, pv), 5), sra32(pv, 31));', 'int32_t horizontal_speed = sub32(sra32(add32(hi, pv), 4), sra32(pv, 31));'),
    ("JumpArc 300", 'int32_t distance_times_300 = mul_lo((int32_t)horizontal_distance, 300);', 'int32_t distance_times_300 = mul_lo((int32_t)horizontal_distance, 299);'),
    ("JumpArc 110", 'int32_t u = mul_lo(root, 110);', 'int32_t u = mul_lo(root, 220);'),
    ("JumpArc duration", 'WR16(flight_monkey + AE_MK_ANIM_LENGTH, sub32(sra32(hi3, 5), sg));', 'WR16(flight_monkey + AE_MK_ANIM_LENGTH, sub32(sra32(hi3, 4), sg));'),
    ("JumpArc mask", 'uint32_t horizontal_distance = (uint32_t)dxz & 0xFFFFu;', 'uint32_t horizontal_distance = (uint32_t)dxz & 0xFFFEu;'),
    ("JumpArc flat", '} else if (horizontal_distance == 0) {\n        launch_angle = 1024;', '} else if (horizontal_distance == 0) {\n        launch_angle = 1000;'),
    ("JumpArc flat100", 'if ((height_delta < 0 ? neg32(height_delta) : height_delta) < 100) {', 'if ((height_delta < 0 ? neg32(height_delta) : height_delta) < 101) {'),
    ("JumpArc vel 120", 'int32_t t = mul_lo(horizontal_speed, 120);', 'int32_t t = mul_lo(horizontal_speed, 60);'),
    ("JumpArc vy", 'int32_t vy = neg32(shift_left32(root, 12));', 'int32_t vy = neg32(shift_left32(root, 11));'),
    ("Attack chance", "if (!((int32_t)pct < chance)) return 0;", "if (!((int32_t)pct <= chance)) return 0;"),
    ("Attack timer", 'if (kind == 1) WR16(cur_monkey(r) + AE_MK_TIMER_174, 90);', 'if (kind == 1) WR16(cur_monkey(r) + AE_MK_TIMER_174, 91);'),
    ("Attack count", 'WR8(monkey + AE_MK_PATROL_COUNTER, b * cnt);', 'WR8(monkey + AE_MK_PATROL_COUNTER, (b + 1) * cnt);'),
    ("Attack frame", "uint32_t f = RD16(tbl + ((uint32_t)idx << 3)) + 4u;", "uint32_t f = RD16(tbl + ((uint32_t)idx << 3)) + 5u;"),
    ("Attack counter12", 'WR16(monkey + AE_MK_ANIM_LENGTH, 12);', 'WR16(monkey + AE_MK_ANIM_LENGTH, 13);'),
    ("Attack voice", "ae_MonkeyQueueVoice(r, h, voice, 0);", "ae_MonkeyQueueVoice(r, h, voice, 1);"),
    ("Attack kind7", 'if (s16(kind_bits) != 7) return 1;', 'if (s16(kind_bits) != 6) return 1;'),
    ("Attack range", 'if (RDS16(entry_info + AE_MK_INFO_ATTACK_MAX_RANGE) < dist) return 0;', 'if (RDS16(entry_info + AE_MK_INFO_ATTACK_MAX_RANGE) <= dist) return 0;'),
    ("Attack range lo", 'if (dist < RDS16(entry_info + AE_MK_INFO_ATTACK_MIN_RANGE)) return 0;', 'if (dist <= RDS16(entry_info + AE_MK_INFO_ATTACK_MIN_RANGE)) return 0;'),
    ("Attack timercheck", 'if (RD16(cur_monkey(r) + AE_MK_TIMER_174) != 0) return 0;', 'if (RD16(cur_monkey(r) + AE_MK_LINK_SCORES) != 0) return 0;'),
    ("Attack anim0", 'if (kind == 0 || kind == 5) {', 'if (kind == 0 || kind == 4) {'),
    ("Attack flagbit", ">> 2) & 1u)) {\n        WR16(monkey + AE_MK_COUNTER96, 0);", ">> 3) & 1u)) {\n        WR16(monkey + AE_MK_COUNTER96, 0);"),
    ("Allowed 1000", "if (RD8(cur_monkey(r) + AE_MK_ID) != RD32(sp + 48)) return 0;", "if (RD8(cur_monkey(r) + AE_MK_ID) != RD32(sp + 48) + 1u) return 0;"),
    ("Allowed alerted", 'if (RD8(cur_monkey(r) + AE_MK_ALERTED) != 1u) return 0;', 'if (RD8(cur_monkey(r) + AE_MK_ALERTED) != 2u) return 0;'),
    ("Allowed notalerted", 'if (RD8(cur_monkey(r) + AE_MK_ALERTED) == 1u) return 0;', 'if (RD8(cur_monkey(r) + AE_MK_ALERTED) == 2u) return 0;'),
    ("Allowed goal", 'if (to_node != (uint32_t)RDS16(cur_monkey(r) + AE_MK_GOAL_NODE)) return 0;', 'if (to_node != (uint32_t)RDS16(cur_monkey(r) + AE_MK_GROUP_VISIT_MASK)) return 0;'),
    ("Allowed dist", "if ((int32_t)RD32(sp + 48) < d) return 0;", "if ((int32_t)RD32(sp + 48) <= d) return 0;"),
    ("Allowed gimmick80", "if (ae_GimmickAllowsMonkeyPassage(r, h, RD32(sp + 48)) != 0) return 0;", "if (ae_GimmickAllowsMonkeyPassage(r, h, RD32(sp + 48)) == 0) return 0;"),
    ("Allowed gimmick40", "if (ae_GimmickAllowsMonkeyPassage(r, h, RD32(sp + 48)) == 0) return 0;", "if (ae_GimmickAllowsMonkeyPassage(r, h, RD32(sp + 48)) != 0) return 0;"),
    ("Allowed anim", "if (anim == 1) return 0;", "if (anim == 2) return 0;"),
    ("Allowed nodeflag", '& 0x100u) {\n        if (to_node !=', '& 0x200u) {\n        if (to_node !='),
    ("Icon 10", 'if ((int32_t)RD32(monkey + AE_MK_ALERT_ICON_KIND) != kind) WR16(monkey + AE_MK_SHOT_COOLDOWN, 10);', 'if ((int32_t)RD32(monkey + AE_MK_ALERT_ICON_KIND) != kind) WR16(monkey + AE_MK_SHOT_COOLDOWN, 11);'),
    ("Icon 896", 'else if (kind == 896) icon = 2;', 'else if (kind == 897) icon = 2;'),
    ("Icon flag", ">> 3) & 1u)) {\n            if (!ae_MonkeyIsAttackState(r)) {", ">> 2) & 1u)) {\n            if (!ae_MonkeyIsAttackState(r)) {"),
    ("Icon y100", "WR16(sp + 18, 100);", "WR16(sp + 18, 101);"),
    ("Icon 220", "uint32_t ay = RD16(sp + 18) + 220u;", "uint32_t ay = RD16(sp + 18) + 221u;"),
    ("Icon lod3 y", 'WR16(sp + 18, RD16(reloaded_monkey + AE_MK_POS + 4) + 10u);', 'WR16(sp + 18, RD16(reloaded_monkey + AE_MK_POS + 4) + 11u);'),
    ("Icon lod", "if (lod < 3u) {", "if (lod < 4u) {"),
    ("Icon decrement", 'WR16(m + AE_MK_SHOT_COOLDOWN, RD16(m + AE_MK_SHOT_COOLDOWN) - 1u);', 'WR16(m + AE_MK_SHOT_COOLDOWN, RD16(m + AE_MK_SHOT_COOLDOWN) - 2u);'),
    ("Score index", 'WR16(m + AE_MK_LINK_SCORES + (uint32_t)link_index * 2u, dot);', 'WR16(m + 0x178 + (uint32_t)link_index * 2u, dot);'),
    ("Score count", 'if (RD8(node_offset + RD32(AE_G_NAV_NODES) + AE_NAV_ACTIVE_LINK_COUNT) == 0) return;', 'if (RD8(node_offset + RD32(AE_G_NAV_NODES) + AE_NAV_LINK_COUNT) == 0) return;'),
    ("Score loop", '} while (link_index < (int32_t)RD8(node_offset + RD32(AE_G_NAV_NODES) + AE_NAV_ACTIVE_LINK_COUNT));', '} while (link_index < (int32_t)RD8(node_offset + RD32(AE_G_NAV_NODES) + AE_NAV_LINK_COUNT));'),
    ("Cosines second", "WR16(out0, d);\n    ae_NavNodeGetPos(r, h, RD16(cur_monkey(r) + AE_MK_NAV_NODE1)", "WR16(out0, d);\n    ae_NavNodeGetPos(r, h, RD16(cur_monkey(r) + AE_MK_NAV_NODE0)"),
    ("Cosines out", "WR16(out1, d);", "WR16(out1 + 2, d);"),
    ("FireKind 5", "} else if (kind != 5) {", "} else if (kind != 4) {"),
    ("FireKind 1", "if (kind == 1) {\n        ae_MonkeyFireProjectile6(r, h);", "if (kind == 2) {\n        ae_MonkeyFireProjectile6(r, h);"),
    ("Waypoint z", 'WR32(sp + 40, RD32(monkey + AE_MK_JUMP_START + 8));', 'WR32(sp + 40, RD32(monkey + AE_MK_JUMP_START + 4));'),
    ("Waypoint arg", "if (((uint32_t)which << 16) != 0) {", "if (((uint32_t)which << 15) != 0) {"),
    ("Facing 34", 'if (target == 0x8003C250u) turn_step = 34;', 'if (target == 0x8003C250u) turn_step = 35;'),
        ("Facing 170", 'turn_step = 170;\n    }\n    uint32_t m = cur_monkey(r);', 'turn_step = 171;\n    }\n    uint32_t m = cur_monkey(r);'),
    ("Facing 3686", "if (s16((uint32_t)dot) < -3686) {", "if (s16((uint32_t)dot) < -3685) {"),
    ("Facing 9", 'int32_t x9 = add32(shift_left32(turn_delta, 3), turn_delta);', 'int32_t x9 = add32(shift_left32(turn_delta, 2), turn_delta);'),
    ("Facing 22div", 'sra32(absolute_angle, 31));\n        turn_frames = sub32(turn_frames, 1);', 'sra32(absolute_angle, 31));\n        turn_frames = sub32(turn_frames, 2);'),
    ("Facing 2049", 'if (turn_delta < 2049) {', 'if (turn_delta < 2050) {'),
    ("Facing sign", 'if (turn_delta > 0) {\n            WR16(sp + 16, 0);\n            WR16(sp + 18, turn_step);', 'if (turn_delta >= 0) {\n            WR16(sp + 16, 0);\n            WR16(sp + 18, turn_step);'),
    ("Facing neg", 'int32_t turn_delta = sub32(neg32(second), previous_angle);', 'int32_t turn_delta = sub32(second, neg32(previous_angle));'),
    ("Facing 120", 'WR16(monkey + AE_MK_LINK_FRAMES, turn_frames < 0 ? 0u : (uint32_t)turn_frames);', 'WR16(monkey + AE_MK_LINK_FRAME, turn_frames < 0 ? 0u : (uint32_t)turn_frames);'),
    ("Facing step", "uint32_t idx = RD8(cur_monkey(r) + AE_MK_AI_STATE) - 2u;\n    if (idx < 56u) {", "uint32_t idx = RD8(cur_monkey(r) + AE_MK_AI_STATE) - 2u;\n    if (idx < 57u) {"),
    ("Sight 569", 'if (pos < 569u) flags = 1;', 'if (pos < 570u) flags = 1;'),
    ("Sight 3528", 'else flags = (pos - 3528u) < 569u;', 'else flags = (pos - 3527u) < 569u;'),
    ("Sight 1137", 'flags = (pos - 456u) < 1137u;', 'flags = (pos - 456u) < 1138u;'),
    ("Sight 1480", 'flags = (pos - 1480u) < 1137u;', 'flags = (pos - 1481u) < 1137u;'),
    ("Sight 2504", 'flags = (pos - 2504u) < 1137u;', 'flags = (pos - 2503u) < 1137u;'),
    ("Sight mode", 'sight_result = 1;\n            flags = 1;\n        } else if (target == 0x80040704u) {', 'sight_result = 0;\n            flags = 1;\n        } else if (target == 0x80040704u) {'),
    ("Sight state", "if (state == 3 || state == 8) {", "if (state == 3 || state == 9) {"),
    ("Sight seven", 'if (target_motion == 7) {', 'if (target_motion == 6) {'),
    ("Sight bit", "f |= bit << 29;", "f |= bit << 28;"),
    ("Sight range2", 'int32_t range2 = RDS16(info + AE_MK_INFO_SIGHT_RANGE);', 'int32_t range2 = RDS16(info + 0x18);'),
    ("Sight boss", 'WR32(boss_monkey + AE_MK_FLAGS, RD32(boss_monkey + AE_MK_FLAGS) | 0x10000000u);\n        return 1;\n    }\n    uint32_t target_motion = 1;', 'WR32(boss_monkey + AE_MK_FLAGS, RD32(boss_monkey + AE_MK_FLAGS) | 0x20000000u);\n        return 1;\n    }\n    uint32_t target_motion = 1;'),
    ("Sight rc", 'if (RD8(AE_G_NAV_AREA_FLAG) == 0) target_motion', 'if (RD8(AE_G_NAV_AREA_FLAG) != 0) target_motion'),
    ("Sight mask", 'RD32(RD32(AE_G_MONKEY_TARGET) + AE_MK_TARGET_STATE_FLAGS) & 0xFu', 'RD32(RD32(AE_G_MONKEY_TARGET) + AE_MK_TARGET_STATE_FLAGS) & 0x7u'),
    ("Sight dist", "if (!(dist < range1)) {", "if (!(dist <= range1)) {"),
]

# the functions a mutation can influence (by the first word of its label); the other families are skipped to save time
MUT_FUNCS = {
    "SetState": ["MonkeySetState"], "Enter32": ["MonkeyEnterState_32"], "Enter3D": ["MonkeyEnterState_3D"],
    "Enter3E": ["MonkeyEnterState_3E"], "Enter3F": ["MonkeyEnterState_3F"], "Enter17": ["MonkeyEnterState_17"],
    "Enter2A": ["MonkeyEnterState_2A"], "LinkSlope": ["MonkeyUpdateLinkSlope"], "GoalPos": ["MonkeyUpdateGoalPos"],
    "JumpArc": ["MonkeyPrepareJumpArc"], "Attack": ["MonkeyTryAttackKind"], "Allowed": ["MonkeyNavLinkAllowed"],
    "Icon": ["MonkeyDrawAlertIcon"], "Score": ["MonkeyScoreLinksByTarget"], "Cosines": ["MonkeyNodeTargetCosines"],
    "FireKind": ["MonkeyFireProjectileByKind"], "Waypoint": ["MonkeyGetWaypointPos"], "Facing": ["MonkeyUpdateFacing"],
    "Sight": ["MonkeyUpdateSight"],
}


def run_mutations(scale, seed, only_labels=()):
    rel = "src/monkeys/ae_monkey_level2.c"
    text = (ROOT / rel).read_text(encoding="utf-8")
    survivors = []
    tmp = Path(tempfile.mkdtemp(prefix="mk3a_mut_"))
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
        h.reload_c({rel: mp}, tag=f"mk3amut{k}")
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
    ap.add_argument("--cases", type=float, default=32, help="scale of the case counts in percent")
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
