"""Differential check of monkey port batch 1 (src/monkeys/ae_monkey_base.c, src/core/ae_sdk.c) against the original
MIPS code: for every ported function many scenarios are built in a byte-exact RAM image (SCUS_944.23 at its real
addresses, monkeys / nav nodes / MonkeyInfo / globals laid out as MonkeySpawnAll leaves them, plus randomised and
edge-case states); the original instructions (GTE through the PCSX-ReARMed oracle) and the C port run on identical
images with identical hook behaviour; return value, RAM outside the stack window, scratchpad and the sequence of hook
calls must be identical.  Needs extract/disc/SCUS_944.23 (skips if missing).  See tools/mips/monkey_harness.py.

Usage: uv run python tools/mips/check_monkey_base.py [--cases PERCENT] [--seed S] [--coverage] [--trapv]
       [--mutate]   (build deliberately wrong copies of the port; the checker must fail on every one)
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
from monkey_harness import W, build_monkey_world, build_nav, MONKEYS, MONKEY_SIZE, G_CUR, G_NAV  # noqa: E402
import check_bodies as cb  # noqa: E402

# name -> (original address, size, return kind: s = int32 / u = uint32 / v = void, takes hooks, argument kinds)
FUNCS = {
    "MonkeyRand": (0x80032E30, 40, "u", False, ""),
    "MonkeyCaughtGroupValue": (0x80032E58, 44, "u", False, ""),
    "MonkeyGetAnimIndex": (0x80032E84, 160, "s", False, ""),
    "MonkeyNop": (0x80033000, 8, "s", False, ""),
    "MonkeyBodyRegister": (0x80033008, 96, "s", True, ""),
    "MonkeyBodyUnregister": (0x80033068, 96, "s", True, ""),
    "MonkeyActivityReset": (0x8003317C, 52, "v", False, ""),
    "MonkeyStoreActivities": (0x800334C8, 264, "v", False, ""),
    "GimmickAllowsMonkeyPassage": (0x800335D0, 312, "s", True, "u"),
    "MonkeyVoiceQueueReset": (0x80033708, 60, "v", False, ""),
    "MonkeyQueueVoice": (0x80033744, 128, "v", True, "ss"),
    "DelayedSfxUpdate": (0x800337C4, 168, "v", True, ""),
    "NavNodeGetLink": (0x8003386C, 212, "s", False, "ss"),
    "AreaIsSpecialMonkeyArea": (0x8003398C, 64, "s", False, ""),
    "MonkeyIsState1E_to_20": (0x800339CC, 32, "s", False, ""),
    "AreaIsBossArena": (0x800339EC, 80, "s", False, ""),
    "MonkeyIsIdleActivityState": (0x80033B10, 140, "s", False, ""),
    "MonkeyIsIdleEndState": (0x80033B9C, 68, "s", False, ""),
    "MonkeyIsAltAnimState": (0x80033BE0, 32, "s", False, ""),
    "MonkeyIsState0C_or_24": (0x80033C00, 52, "s", False, ""),
    "MonkeyIsJumpArcState": (0x80033C34, 76, "s", False, ""),
    "MonkeyIsAttackState": (0x80033C80, 76, "s", False, ""),
    "MonkeySpawnProjectile": (0x80033CCC, 292, "s", True, "s"),
    "AngleBetweenXZ": (0x800344C8, 148, "s", False, "uu"),
    "DistanceIntToShort3D": (0x800345D4, 176, "s", False, "uu"),
    "Distance3D": (0x80034684, 184, "s", False, "uu"),
    "DistanceXZ": (0x8003473C, 156, "s", False, "uu"),
    "MonkeyCalcMoveDir": (0x800347D8, 92, "v", False, ""),
    "DotXZ_SV": (0x800348CC, 116, "s", False, "uu"),
    "DotXZ_V": (0x80034940, 116, "s", False, "uu"),
    "Atan2Angle4096": (0x800349B4, 64, "s", False, "ss"),
    "NavNodeIsSpecial": (0x800349F4, 56, "s", False, "s"),
    "NavNodeGetLinkIds": (0x80034B74, 148, "v", False, "suuu"),
    "NavNodeGetLinkIdsEx": (0x80034C08, 296, "v", False, "suuuu"),
    "ObjGetBonePos": (0x80034D30, 268, "s", True, "suuu"),
    "MonkeyReachedNode": (0x8003516C, 516, "s", False, ""),
    "MonkeyNavBacktracked": (0x80035604, 188, "s", False, ""),
    "MonkeyGetCallVoice": (0x800356C0, 144, "s", False, ""),
    "MonkeyGetCallVoice2": (0x80035750, 144, "s", False, ""),
    "MonkeyFacingFromLinkCode": (0x80036BD0, 180, "v", False, "s"),
    "MonkeyAttackChance": (0x80037068, 164, "s", False, "s"),
    "MonkeyApproachAngleNear": (0x80037D6C, 128, "s", False, ""),
    "MonkeyApproachAngleFar": (0x80037DEC, 160, "s", False, ""),
    "NavNodeLinkFlag": (0x80038608, 140, "s", False, "ss"),
    "MonkeyPuff14": (0x80039200, 72, "s", True, ""),
    "MonkeyPuff5": (0x8003929C, 72, "s", True, ""),
    "StaticModelDraw": (0x80039E50, 132, "s", True, "u"),
    "MonkeyStub_ReturnZero": (0x8003A334, 8, "s", False, ""),
    "MonkeyInterpolateLinkVector": (0x8003C568, 364, "v", False, ""),
    "MonkeyResetNavVisits": (0x800407F0, 124, "v", False, ""),
    "NavGroundHeightBetween": (0x80040BDC, 288, "s", False, "uu"),
    "ActorContactOwnerType": (0x80041280, 48, "s", False, ""),
    "MonkeyDebugMenuInit": (0x80043060, 144, "v", True, ""),
    "MonkeyDebugMenuInit2": (0x800430F0, 616, "v", True, "s"),
}
# SDK maths (src/core/ae_sdk.c), also diffed directly: name -> (address, size, return, hooks, args, C name)
FUNCS.update({
    "VectorNormal (SDK)": (0x800301B0, 0x30, "s", False, "uu", "ae_sdk_VectorNormal"),
    "VectorNormalSS (SDK)": (0x800301E0, 0x30, "s", False, "uu", "ae_sdk_VectorNormalSS"),
    "SquareRoot0 (SDK)": (0x800303C0, 0x84, "s", False, "s", "ae_sdk_SquareRoot0"),
    "ratan2 (SDK)": (0x800145D0, 0xB4, "s", False, "ss", "ae_sdk_ratan2"),
    "ApplyMatrixSV (SDK)": (0x800300B0, 0x5C, "u", False, "uuu", "ae_sdk_ApplyMatrixSV"),
})
# code exercised through the functions above (for the coverage report)
EXTRA_COVERAGE = {
    "normalize core (SDK)": (0x80030210, 0xC0), "Square0 (SDK)": (0x80014498, 0x28),
    "CollisionBodyRegister (bodies)": (0x80071E44, 408), "CollisionBodyUnregister (bodies)": (0x80072018, 344),
    "GetObjectByIndex (bodies)": (0x80072170, 68),
}

VEC_A, VEC_B, OUT_A = 0x801F0000, 0x801F0040, 0x801F0100
BONES = 0x801C0000
M32 = 0xFFFFFFFF
s32, s16 = mh.s32, mh.s16


def check_all(h, scale, seed):
    t0 = time.time()

    def n(count):
        return max(1, int(count * scale / 100))

    def rng_of(name, i):
        return random.Random(f"{seed}/{name}/{i}")

    def world(rng, extra=None, wild=None):
        def setup(m):
            build_monkey_world(m, rng, wild=wild)
            return extra(m) if extra else None
        return setup

    def cur(m): return m.read32(G_CUR)

    def run_simple(name, count, extra=None, argsf=lambda c: (), nh=False, **kw):
        for i in range(n(count)):
            rng = rng_of(name, i)
            h.run(name, f"#{i}", i, world(rng, extra and (lambda m, rng=rng, i=i: extra(m, rng, i))), argsf,
                  null_hooks=(nh and i % 8 == 7), **kw)

    # ---- trivial ones
    run_simple("MonkeyRand", 300, lambda m, rng, i: W(m).w32(0x800B4D10, rng.choice(
        [rng.getrandbits(16), rng.getrandbits(32), 0, 0xFFFF, 0xCCCC, 0x10000, 0xFFFFFFFF, 1])))
    run_simple("MonkeyCaughtGroupValue", 100)
    run_simple("MonkeyNop", 1)
    run_simple("MonkeyStub_ReturnZero", 1)

    def set_state(m, rng, i, states=None):
        st = states[i % len(states)] if states else i & 0xFF
        m.write8(cur(m) + 0x78, st)

    for name in ("MonkeyIsState1E_to_20", "MonkeyIsIdleActivityState", "MonkeyIsIdleEndState", "MonkeyIsAltAnimState",
                 "MonkeyIsState0C_or_24", "MonkeyIsJumpArcState", "MonkeyIsAttackState"):
        # the whole state range 0..0x47 is always swept (not scaled), plus random bytes
        for i in range(0x48 + max(4, n(16))):
            rng = rng_of(name, i)
            h.run(name, f"state {i}", i, world(rng, lambda m, rng=rng, i=i: m.write8(cur(m) + 0x78, i if i < 0x48 else rng.getrandbits(8))), lambda c: ())

    def area_setup(m, rng, i):
        w = W(m)
        specials = [0x29, 0x43, 0x35, 0x46, 0x28, 0x2A, 0x42, 0x44, 0x34, 0x36, 0x45, 0x47, 0, 0xFFFF]
        w.w16(0x800F4476, specials[i % len(specials)] if i < 40 else rng.getrandbits(16))
        w.w16(0x800F4472, [3, 6, 8, 0, 1, 2, 4, 5, 7, 9, 0xFFFF][(i // 4) % 11] if i % 3 else rng.getrandbits(16))
    for name in ("AreaIsSpecialMonkeyArea", "AreaIsBossArena"):
        for i in range(60 + n(60)):
            rng = rng_of(name, i)
            h.run(name, f"#{i}", i, world(rng, lambda m, rng=rng, i=i: area_setup(m, rng, i)), lambda c: ())

    def anim_setup(m, rng, i):
        st = rng.choice([3, 8, 3, 8, 50, 51, 52, 49, 53, rng.randint(0, 0x42), rng.getrandbits(8)])
        m.write8(cur(m) + 0x78, st)
        if rng.random() < 0.3:
            m.write16(0x800AEB5A, 0)
        if rng.random() < 0.5:
            m.write32(0x800AE940 + 8 * (st % 0x43), rng.getrandbits(32))   # the (writable) state table
    run_simple("MonkeyGetAnimIndex", 400, anim_setup)

    for name in ("MonkeyGetCallVoice", "MonkeyGetCallVoice2"):
        run_simple(name, 120)

    # ---- body registration (CollisionBodyRegister / Unregister are the ported body code, diffed here with the original)
    def body_setup(m, rng, i):
        k = rng.choice([0, 1, 3, 10, 60, 200, 254, 255, 256])
        cb.build_world(m, rng, n=min(k, 256), full=(k == 256) and rng.random() < 0.7)
        build_monkey_world(m, rng)      # (the world builder above clears the body tables only)
        flags = m.read32(cur(m) + 0x24C)
        registered = rng.random() < 0.6
        m.write32(cur(m) + 0x24C, (flags | 2) if registered else (flags & ~2))
        if registered and rng.random() < 0.85 and k < 256:
            # make the monkey's body a real member of the list: run the original registration on the image
            hooks = {0x800A3720: lambda cpu, r: 0, 0x800878E0: lambda cpu, r: 0}
            m.call(0x80071E44, (3, cur(m) + 0x24, cur(m)), hooks=hooks)
    for name in ("MonkeyBodyRegister", "MonkeyBodyUnregister"):
        for i in range(n(300)):
            rng = rng_of(name, i)
            h.run(name, f"#{i}", i, lambda m, rng=rng, i=i: body_setup(m, rng, i), lambda c: (), null_hooks=(i % 8 == 7))

    run_simple("MonkeyActivityReset", 40, lambda m, rng, i: [W(m).w8(0x800F44BC + k, rng.getrandbits(8)) for k in range(8)])

    def store_setup(m, rng, i):
        w = W(m)
        nodes = m.read16(0x800B4D18)
        for k in range(8):
            a = MONKEYS + MONKEY_SIZE * k
            w.w8(a + 0x7A, rng.choice([0, 1, 2, 3, 4, 4, 4, 5]))
            w.w8(a + 0x78, rng.choice([0x2B, 0x2B, 0x2A, rng.randint(0, 0x42)]))
            w.w16(a + 0xF4, rng.randint(0, max(0, nodes - 1)) if rng.random() < 0.9 else rng.getrandbits(16))
        for k in range(nodes):
            if rng.random() < 0.6:
                w.w32(0x801D0000 + 0x18 * k, rng.choice([0x400, 0x401, 0, 0xFFFFFFFF, rng.getrandbits(32)]))
        w.w8(0x800B4CF4, rng.choice([0, 0, 0, 0, 1]))
        w.w16(0x800AEB68, rng.choice([0, 1, 1, 8]))
    run_simple("MonkeyStoreActivities", 400, store_setup, allow_trap=True)

    run_simple("GimmickAllowsMonkeyPassage", 500, argsf=lambda c: (random.Random(seed).getrandbits(32),), nh=True)

    # ---- voice queue
    QUEUE = 0x800E6780

    def queue_setup(m, rng, i):
        w = W(m)
        mode = rng.choice(["free", "full", "mixed", "mixed", "onefree"])
        free_at = rng.choice([15, 15, 0, rng.randint(0, 15)])
        for k in range(16):
            e = QUEUE + 20 * k
            t = {"onefree": -1 if k == free_at else rng.choice([0, 1, 2, 5, 0x7FFF]), "free": -1, "full": rng.choice([0, 1, 2, 5, 0x7FFF]), "mixed": rng.choice([-1, 0, 1, 2, 5, 0x7FFF, -0x8000, rng.getrandbits(16)])}[mode]
            w.w16(e + 4, t)
            w.w32(e, rng.choice([-1, 0, 1, 0x602D, 5, rng.getrandbits(32), 0x7FFFFFFF]))
            for off in (8, 12, 16):
                w.w32(e + off, rng.getrandbits(32))
    run_simple("MonkeyVoiceQueueReset", 20, queue_setup)
    for i in range(n(300)):
        rng = rng_of("MonkeyQueueVoice", i)
        h.run("MonkeyQueueVoice", f"#{i}", i, world(rng, lambda m, rng=rng, i=i: queue_setup(m, rng, i)),
              lambda c, rng=rng: (rng.choice([0, 1, -1, 0x602D, rng.getrandbits(32)]), rng.choice([0, 1, 5, 30, -1, 0x12345, rng.getrandbits(32)])),
              null_hooks=(i % 8 == 7))
    run_simple("DelayedSfxUpdate", 300, queue_setup, nh=True)

    # ---- nav nodes
    for i in range(n(600)):
        rng = rng_of("NavNodeGetLink", i)
        def argsf(c, rng=rng):
            node = rng.choice([rng.randint(0, 5), rng.randint(0, 39), rng.randint(-3, 45), rng.getrandbits(32)])
            idx = rng.choice([-1, 0, 1, 2, 3, 4, rng.randint(-3, 9), rng.getrandbits(32)])
            if rng.random() < 0.3:
                node = (node & 0xFFFF) | (rng.getrandbits(16) << 16)
            return (node & M32, idx & M32)
        h.run("NavNodeGetLink", f"#{i}", i, world(rng), argsf, allow_trap=True)
    for name, count in (("NavNodeIsSpecial", 300),):
        for i in range(n(count)):
            rng = rng_of(name, i)
            h.run(name, f"#{i}", i, world(rng), lambda c, rng=rng: (rng.choice([rng.randint(0, 39), rng.getrandbits(32), rng.randint(-2, 41)]) & M32,), allow_trap=True)

    def link_ids_setup(rng):
        def setup(m):
            build_monkey_world(m, rng)
            w = W(m)
            for k in range(m.read16(0x800B4D18)):
                a = 0x801D0000 + 0x18 * k
                for off in (0x10, 0x12):
                    w.w16(a + off, rng.choice([rng.getrandbits(16), rng.randint(0, 4099), rng.randint(0, 40) * 100 + rng.randint(0, 99),
                                               0x8000, 0xFFFF, 0x7FFF, 0, 99, 100, 101, 0xFF9C, 0xFF9B, rng.randint(-5000, 0) & 0xFFFF]))
            for off in range(0, 0x10, 2):
                w.w16(0x800AEB6C + off * 40 // 2 * 0, rng.getrandbits(16))
            for k in range(0, 700, 2):
                w.w16(0x800AEB6C + k, rng.getrandbits(16))
        return setup
    for name, kinds in (("NavNodeGetLinkIds", 3), ("NavNodeGetLinkIdsEx", 4)):
        for i in range(n(400)):
            rng = rng_of(name, i)
            def argsf(c, rng=rng, kinds=kinds):
                node = rng.choice([rng.randint(0, 39), rng.randint(0, 5), rng.randint(-2, 45), rng.getrandbits(32)])
                outs = [0x801F0200 + 0x10 * k for k in range(kinds)]
                if rng.random() < 0.1:                       # an output that aliases the node record itself
                    outs[rng.randrange(kinds)] = 0x801D0000 + 0x18 * rng.randint(0, 3) + rng.choice([0x10, 0x12, 0x11])
                return (node & M32, *outs)
            h.run(name, f"#{i}", i, link_ids_setup(rng), argsf, allow_trap=True)

    for i in range(n(600)):
        rng = rng_of("NavNodeLinkFlag", i)
        def argsf(c, rng=rng):
            node = rng.choice([rng.randint(0, 39), rng.randint(0, 5), rng.randint(-2, 45)])
            link = rng.choice([rng.randint(0, 40), rng.randint(-2, 40), rng.getrandbits(32)])
            if rng.random() < 0.3:
                link = (link & 0xFFFF) | (rng.getrandbits(16) << 16)
            return (node & M32, link & M32)
        h.run("NavNodeLinkFlag", f"#{i}", i, world(rng), argsf, allow_trap=True)

    # link counting walk with a hand-built node (links with and without bit 14, all positions)
    for i in range(n(600)):
        rng = rng_of("NavNodeGetLink-lists", i)
        def setup(m, rng=rng):
            build_monkey_world(m, rng)
            w = W(m)
            cnt = rng.randint(0, 8)
            a = 0x801D0000
            w.w8(a + 0xA, cnt)
            w.w32(a + 0xC, 0x801D8000)
            for j in range(cnt):
                w.w16(0x801D8000 + 2 * j, rng.choice([rng.getrandbits(16), 0x4000 | rng.randint(0, 99), rng.randint(0, 99)]))
        h.run("NavNodeGetLink", f"lists #{i}", i, setup, lambda c, rng=rng: (0, rng.choice([-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9])))

    # ---- navigation history / arrival
    for i in range(n(600)):
        rng = rng_of("MonkeyNavBacktracked", i)
        def setup(m, rng=rng):
            build_monkey_world(m, rng)
            w = W(m)
            a = cur(m)
            pool = [rng.randint(0, 9) for _ in range(rng.choice([1, 2, 3, 4]))]
            vals = [rng.choice(pool) for _ in range(6)]
            if rng.random() < 0.4:                       # a genuine repeat: previous = current
                vals[3:6] = vals[0:3]
            if rng.random() < 0.2:
                vals[1] = vals[0]
            for off, v in zip((0xF2, 0xF4, 0xF6, 0xF8, 0xFA, 0xFC), vals):
                w.w16(a + off, v | (rng.getrandbits(16) << 0 if rng.random() < 0.05 else 0))
        h.run("MonkeyNavBacktracked", f"#{i}", i, setup, lambda c: ())

    def reached_setup(m, rng, i):
        w = W(m)
        a = cur(m)
        rad = rng.choice([200, 100, 1, 0, -1, 255, 300, rng.randint(-400, 800), rng.getrandbits(16), 0x7FFF, 0x8000, 17])
        w.w16(a + 0x220, rad)
        w.w8(a + 0x78, rng.choice([12, 36, 9, 9, 0, 1, rng.randint(0, 0x42)]))
        w.w16(0x800F4476, rng.choice([0x4D, 0x1A, 0x4D, 0x1A, 0x10, rng.getrandbits(16)]))
        node = rng.choice([5, 6, 7, 4, 0x13, 0x14, 0xB4, 0xB5, 0x9E, 0x9F, 0xA4, 0xA5, 151, 152, 153, 154, 150,
                           156, 157, 158, 159, 160, 161, 169, 170, 171, 172, 173, 174, rng.getrandbits(16)])
        w.w16(a + 0xF2, node)
        # compare value near radius * k  (k from the state branch)
        r16 = s16(rad)
        base = rng.choice([r16, r16 * 2, (r16 * 15) // 10, r16 // 2, (r16 + (1 if r16 < 0 else 0)), 0])
        flags2 = rng.choice([0, 2, 0, 2, 1, 3])
        w.w32(a + 0x228, flags2)
        for off in (0x214, 0x218):
            w.w32(a + off, base + rng.choice([-2, -1, 0, 1, 2]) if rng.random() < 0.8 else rng.getrandbits(32))
    run_simple("MonkeyReachedNode", 900, reached_setup)

    # ---- projectile / puffs / draw
    def proj_setup(m, rng, i):
        w = W(m)
        if i % 3 == 0:
            for k in range(3):
                w.w32(cur(m) + 0x24 + 4 * k, rng.getrandbits(32))
    for i in range(n(400)):
        rng = rng_of("MonkeySpawnProjectile", i)
        h.run("MonkeySpawnProjectile", f"#{i}", i, world(rng, lambda m, rng=rng, i=i: proj_setup(m, rng, i)),
              lambda c, rng=rng: (rng.choice([6, 6, 3, 4, 5, 0, 1, 2, 7, -1, 0x10003, rng.getrandbits(32)]),), null_hooks=(i % 8 == 7))
    run_simple("MonkeyPuff14", 100, nh=True)
    run_simple("MonkeyPuff5", 100, nh=True)

    def draw_setup(m, rng, i):
        w = W(m)
        obj = 0x801C8000
        w.w32(obj, 0x801C9000)
        w.w16(0x801C9004, rng.choice([0, 1, 5, 0x1FF, rng.getrandbits(16)]))
        w.w32(0x800EC35C, rng.choice([0, 1, 2, 3, 7, rng.getrandbits(32)]))
    run_simple("StaticModelDraw", 120, draw_setup, argsf=lambda c: (0x801C8000,), nh=True)

    # ---- vector maths
    def vec(rng, kind):
        r = rng.random()
        if kind == "wild" or r < 0.1:
            return [rng.getrandbits(32) for _ in range(3)]
        if r < 0.2:
            return [rng.choice([0, 1, -1, 4096, -4096, 0x7FFF, -0x8000, 0x8000, 0x7FFFFFFF, -0x80000000]) & M32 for _ in range(3)]
        mag = rng.choice([10, 300, 4096, 20000, 32767])
        return [rng.randint(-mag, mag) & M32 for _ in range(3)]

    def vec_setup_sv(rng):
        def setup(m):
            build_monkey_world(m, rng)
            w = W(m)
            va = [x & 0xFFFF for x in vec(rng, "sane")]
            vb = va if rng.random() < 0.1 else ([(-x) & 0xFFFF for x in va] if rng.random() < 0.1 else [x & 0xFFFF for x in vec(rng, "sane")])
            for k in range(3):
                w.w16(VEC_A + 2 * k, va[k])
                w.w16(VEC_B + 2 * k, vb[k])
        return setup
    def vec_setup_v(rng, kind=None):
        def setup(m):
            build_monkey_world(m, rng)
            w = W(m)
            va = vec(rng, kind or rng.choice(["sane", "sane", "wild"]))
            r = rng.random()
            vb = va if r < 0.08 else ([(-x) & M32 for x in va] if r < 0.16 else ([x & M32 for x in [v * 2 for v in va]] if r < 0.22 else vec(rng, kind or "sane")))
            for k in range(3):
                w.w32(VEC_A + 4 * k, va[k])
                w.w32(VEC_B + 4 * k, vb[k])
        return setup
    for name in ("AngleBetweenXZ", "DotXZ_V", "Distance3D", "DistanceXZ"):
        for i in range(n(700)):
            rng = rng_of(name, i)
            h.run(name, f"#{i}", i, vec_setup_v(rng), lambda c: (VEC_A, VEC_B), allow_trap=True)
    # AngleBetweenXZ with a negative cross product that is 1 mod 4096 (the +4095 rounding is only observable there):
    # found with a small replica of the normalise core (only used to pick the vectors; the check itself is the diff)
    import monkey_harness as _mh
    table = [s16(h.base[0][0xA9C14 + 2 * k] | h.base[0][0xA9C14 + 2 * k + 1] << 8) for k in range(192)]
    def norm_rep(v):
        ir = [s16(x & 0xFFFF) for x in v]
        sq = sum(x * x for x in ir)
        if sq == 0 or sq > 0x7FFFFFFF:
            return None
        lz = 32 - sq.bit_length()
        e = lz & ~1
        t4 = sq >> (24 - e) if e < 24 else (sq << (e - 24)) & M32
        idx = t4 - 64
        if not 0 <= idx < 192:
            return None
        return [(table[idx] * x) >> ((31 - e) >> 1) for x in ir]
    found, trial = [], random.Random(seed ^ 0xA11)
    while len(found) < max(20, n(60)):
        v1 = [trial.randint(-3000, 3000), 0, trial.randint(-3000, 3000)]
        v2 = [trial.randint(-3000, 3000), 0, trial.randint(-3000, 3000)]
        a_, b_ = norm_rep(v1), norm_rep(v2)
        if a_ and b_:
            cross = a_[0] * b_[2] - a_[2] * b_[0]
            dot = a_[0] * b_[0] + a_[2] * b_[2]
            # near 45 degrees (both ratan2 operands large), where a step of 1 in the cross term changes the angle
            if cross < 0 and (cross + 4095) % 4096 == 0 and -cross > 1200 * 4096 and abs(dot) > 1200 * 4096:
                found.append((v1, v2))
    for i, (v1, v2) in enumerate(found):
        def setup(m, v1=v1, v2=v2, i=i):
            build_monkey_world(m, rng_of("AngleBetweenXZ-target", i), wild=False)
            w = W(m)
            for k in range(3):
                w.w32(VEC_A + 4 * k, v1[k] & M32)
                w.w32(VEC_B + 4 * k, v2[k] & M32)
        h.run("AngleBetweenXZ", f"cross%4096==1 #{i}", i, setup, lambda c: (VEC_A, VEC_B))
    for i in range(n(700)):
        rng = rng_of("DotXZ_SV", i)
        h.run("DotXZ_SV", f"#{i}", i, vec_setup_sv(rng), lambda c: (VEC_A, VEC_B), allow_trap=True)
    for i in range(n(700)):
        rng = rng_of("DistanceIntToShort3D", i)
        def setup(m, rng=rng):
            build_monkey_world(m, rng)
            w = W(m)
            va = vec(rng, rng.choice(["sane", "wild"]))
            vb = [x & 0xFFFF for x in vec(rng, "sane")]
            for k in range(3):
                w.w32(VEC_A + 4 * k, va[k])
                w.w16(VEC_B + 2 * k, vb[k])
        h.run("DistanceIntToShort3D", f"#{i}", i, setup, lambda c: (VEC_A, VEC_B))
    for i in range(n(700)):
        rng = rng_of("Atan2Angle4096", i)
        def argsf(c, rng=rng):
            def v():
                return rng.choice([0, 0, 1, -1, rng.randint(-5000, 5000), rng.randint(-1 << 20, 1 << 20), rng.getrandbits(32),
                                   0x7FFFFFFF, -0x80000000, 1023, 1024, -1024, 0x200000, -0x200000, 4096, -4096]) & M32
            return (v(), v()) if rng.random() < 0.9 else (v(), 0)
        h.run("Atan2Angle4096", f"#{i}", i, world(rng), argsf, allow_trap=True)
    for i in range(n(700)):
        rng = rng_of("MonkeyCalcMoveDir", i)
        def setup(m, rng=rng):
            build_monkey_world(m, rng)
            w = W(m)
            a = cur(m)
            kind = rng.choice(["sane", "sane", "wild"])
            for off in (0xBC, 0xC0, 0xC4):
                w.w32(a + off, vec(rng, kind)[0])
            if rng.random() < 0.1:
                for k in range(3):
                    w.w32(a + 0xBC + 4 * k, m.read32(a + 0x24 + 4 * k))     # goal == position: zero vector
        h.run("MonkeyCalcMoveDir", f"#{i}", i, setup, lambda c: (), allow_trap=True)

    # ---- facing / attack / approach / interpolation
    for i in range(n(150)):
        rng = rng_of("MonkeyFacingFromLinkCode", i)
        h.run("MonkeyFacingFromLinkCode", f"#{i}", i, world(rng),
              lambda c, i=i, rng=rng: (((i % 12 - 2) if i < 120 else rng.getrandbits(32)) & M32,))

    def attack_setup(m, rng, i):
        m.write8(cur(m) + 0xD4, rng.choice([0, 1, 2, 0x7F, 0x80, 0xFF, 0xFE, rng.getrandbits(8)]))
    for i in range(n(400)):
        rng = rng_of("MonkeyAttackChance", i)
        h.run("MonkeyAttackChance", f"#{i}", i, world(rng, lambda m, rng=rng, i=i: attack_setup(m, rng, i)),
              lambda c, rng=rng: (rng.choice([7, 7, 0, 1, 2, 3, 4, 5, 6, 8, -1, -2, rng.getrandbits(32), 0x10007]) & M32,),
              allow_trap=True)

    def dist_setup(m, rng, i):
        m.write32(cur(m) + 0x20C, rng.choice([799, 800, 801, 1999, 2000, 2001, 2002, 259, 260, 261, 2999, 3000, 3001,
                                              0, -5, 1, 1400, 2500, rng.randint(0, 4000), rng.getrandbits(32),
                                              0x7FFFFFFF, -0x80000000]) & M32)
    run_simple("MonkeyApproachAngleNear", 400, dist_setup)
    run_simple("MonkeyApproachAngleFar", 400, dist_setup)

    def interp_setup(m, rng, i):
        w = W(m)
        a = cur(m)
        frames = rng.choice([0, 1, 2, 10, 30, -1, rng.randint(-5, 60), rng.getrandbits(16)])
        w.w16(a + 0x122, frames)
        w.w16(a + 0x120, rng.choice([0, 1, frames, frames - 1, rng.randint(0, 60), rng.getrandbits(16)]))
        kind = rng.choice(["sane", "sane", "wild"])
        for off in (0x100, 0x104, 0x108, 0x110, 0x114, 0x118):
            w.w32(a + off, vec(rng, kind)[0])
        if i % 10 < 3:      # product == INT_MIN with divisor -1 on one axis (the original breaks there)
            k = i % 3
            w.w16(a + 0x122, 0xFFFF)
            w.w16(a + 0x120, 0xFFFE)
            for off in (0x100, 0x104, 0x108):
                w.w32(a + off, 0)
            for off in (0x110, 0x114, 0x118):
                w.w32(a + off, rng.randint(-3, 3))
            w.w32(a + 0x110 + 4 * k, 0x80000000)
    run_simple("MonkeyInterpolateLinkVector", 600, interp_setup, allow_trap=True)

    def visit_setup(m, rng, i):
        w = W(m)
        w.w16(0x800B4D18, rng.choice([0, 1, 2, 5, 20, 40, -1, -5, rng.randint(0, 40), 0x7FFF if rng.random() < 0.02 else 3]))
        w.w8(cur(m) + 0x7B, rng.choice([0, 1, 7, 8, 0xFF, rng.getrandbits(8)]))
    run_simple("MonkeyResetNavVisits", 300, visit_setup, allow_trap=True)

    for i in range(n(800)):
        rng = rng_of("NavGroundHeightBetween", i)
        def setup(m, rng=rng):
            build_monkey_world(m, rng)
            w = W(m)
            kind = rng.choice(["sane", "sane", "wild"])
            va, vb = vec(rng, kind), vec(rng, kind)
            r = rng.random()
            if r < 0.15:
                vb[0], vb[2] = va[0], va[2]
            elif r < 0.3:
                vb[0] = va[0]
            elif r < 0.4:
                vb[2] = va[2]
            elif r < 0.5:
                vb[0] = (va[0] + rng.choice([-3, 3, -100, 100])) & M32
                vb[2] = (va[2] + rng.choice([-3, 3, -100, 100])) & M32
            for k in range(3):
                w.w32(VEC_A + 4 * k, va[k])
                w.w32(VEC_B + 4 * k, vb[k])
            w.w8(cur(m) + 0x78, rng.choice([9, 9, 0, rng.randint(0, 0x42)]))
            if rng.random() < 0.12:     # divisor -1 and product INT_MIN (the original breaks)
                k = rng.choice([0, 2])
                o = 2 - k
                w.w32(VEC_A + 4 * k, (m.read32(VEC_B + 4 * k) - 1) & M32)
                w.w32(VEC_A + 4 * o, m.read32(VEC_B + 4 * o))
                w.w32(VEC_A + 4, 0x80000000)
                w.w32(VEC_B + 4, 0)
                w.w32(cur(m) + 0x24 + 4 * k, (m.read32(VEC_B + 4 * k) + 1) & M32)
                w.w8(cur(m) + 0x78, 0)
        h.run("NavGroundHeightBetween", f"#{i}", i, setup, lambda c: (VEC_A, VEC_B), allow_trap=True)

    # ---- SDK maths, directly
    def i32v(rng):
        return rng.choice([0, 0, 1, -1, 2, 3, 4, 5, 0x7FFFFFFF, -0x80000000, 0x40000000, -0x40000000, 0x3FFFFFFF,
                           rng.getrandbits(32), rng.getrandbits(32), rng.randint(0, 100000), rng.randint(-100000, 0),
                           (1 << rng.randint(0, 31)) & M32, ((1 << rng.randint(0, 31)) - 1) & M32,
                           (-(1 << rng.randint(0, 30))) & M32, rng.randint(0, 5000), 0xFFFF, 0x10000]) & M32
    for i in range(n(800)):
        rng = rng_of("SquareRoot0", i)
        h.run("SquareRoot0 (SDK)", f"#{i}", i, world(rng, wild=False), lambda c, rng=rng: (i32v(rng),))
    for i in range(n(800)):
        rng = rng_of("ratan2", i)
        h.run("ratan2 (SDK)", f"#{i}", i, world(rng, wild=False),
              lambda c, rng=rng, i=i: (0x80000000, 0) if i % 97 == 0 else (i32v(rng), i32v(rng)), allow_trap=True)
    for name, kind in (("VectorNormal (SDK)", 4), ("VectorNormalSS (SDK)", 2)):
        for i in range(n(500)):
            rng = rng_of(name, i)
            def setup(m, rng=rng, kind=kind):
                build_monkey_world(m, rng, wild=False)
                w = W(m)
                v = vec(rng, rng.choice(["sane", "sane", "wild"]))
                for k in range(3):
                    (w.w32 if kind == 4 else w.w16)(VEC_A + kind * k, v[k])
                    (w.w32 if kind == 4 else w.w16)(OUT_A + kind * k, rng.getrandbits(32))
            h.run(name, f"#{i}", i, setup, lambda c: (VEC_A, OUT_A), allow_trap=True)
    MAT = 0x801F0200
    for i in range(n(600)):
        rng = rng_of("ApplyMatrixSV", i)
        def setup(m, rng=rng):
            build_monkey_world(m, rng, wild=False)
            w = W(m)
            ext = rng.random() < 0.3
            for k in range(0, 0x20, 2):
                w.w16(MAT + k, rng.choice([0x7FFF, 0x8000, 0xFFFF, 0, 0x1000]) if ext else rng.choice([0x1000, 0xF000, 0x0B50, rng.getrandbits(16), rng.randint(0, 0x2000), rng.randint(-0x2000, 0) & 0xFFFF]))
            for k in range(0, 8, 2):
                w.w16(VEC_A + k, rng.choice([0x7FFF, 0x8000, 0, 1, 0xFFFF]) if ext else rng.choice([rng.getrandbits(16), rng.randint(-500, 500) & 0xFFFF, rng.randint(-20000, 20000) & 0xFFFF]))
            w.w16(OUT_A, rng.getrandbits(16))
        h.run("ApplyMatrixSV (SDK)", f"#{i}", i, setup, lambda c: (MAT, VEC_A, OUT_A))

    # ---- ActorContactOwnerType (the contact owner is a slot of the collision body table)
    for i in range(n(300)):
        rng = rng_of("ActorContactOwnerType", i)
        def setup(m, rng=rng):
            cb.build_world(m, rng, n=rng.choice([0, 1, 3, 8, 40]))
            build_monkey_world(m, rng)
            a = cur(m)
            m.write32(a + 0x50, rng.choice([rng.getrandbits(32), rng.randint(0, 20), 0x100, 0x101, 0xFFF, 0x1000, 0x80000005,
                                            rng.randint(0, 255), rng.randint(0, 255), rng.randint(0, 255), 0xABC00000 | rng.randint(0, 255)]))
        h.run("ActorContactOwnerType", f"#{i}", i, setup, lambda c: (), allow_trap=True)

    # ---- ObjGetBonePos
    for i in range(n(500)):
        rng = rng_of("ObjGetBonePos", i)
        info = {}
        def setup(m, rng=rng, info=info):
            build_monkey_world(m, rng)
            w = W(m)
            mi = rng.choice([0, 1, 2, 5, 10, 63, rng.getrandbits(16), rng.randint(0, 200)])
            info["matrix"] = 0x800F4750 + ((mi & 0xFFFF) << 5)
            for k in range(16):
                o = mh.OBJS + 0xE4 * k
                w.rand_fill(rng, o, 0xE4)
                w.w32(o + 0xE0, rng.choice([0x10, 0, 0x10, 0x11, rng.getrandbits(32)]))
                w.w32(o + 0x10, BONES + rng.choice([0, 8, 0x100]))
                w.w32(o + 0x14, rng.getrandbits(32))
            w.rand_fill(rng, BONES, 0x400)
            if 0x800F4750 <= info["matrix"] < 0x80100000:
                for j in range(0, 0x20, 2):
                    w.w16(info["matrix"] + j, rng.choice([0x1000, 0, -0x1000 & 0xFFFF, 0x0B50, 0x7FFF, 0x8000, rng.getrandbits(16)]))
            for j in range(0, 0x10, 2):
                w.w16(OUT_A + j, rng.getrandbits(16))
            info["mi"] = mi
            return info
        def argsf(c, rng=rng):
            obj = rng.choice([rng.randint(0, 15), rng.randint(0, 15), rng.randint(0, 15), rng.randint(-2, 20), rng.getrandbits(32)])
            bone = rng.choice([0, 1, 2, 7, 30, 63, rng.getrandbits(16)])
            return (obj & M32, c["mi"], bone, OUT_A)
        h.run("ObjGetBonePos", f"#{i}", i, setup, argsf, allow_trap=True, null_hooks=(i % 8 == 7))

    # ---- debug menu
    run_simple("MonkeyDebugMenuInit", 4, nh=False)
    for i in range(n(200)):
        rng = rng_of("MonkeyDebugMenuInit2", i)
        h.run("MonkeyDebugMenuInit2", f"#{i}", i, world(rng),
              lambda c, rng=rng, i=i: (((i % 8) if i < 160 else rng.getrandbits(32)) & M32,), allow_trap=True, null_hooks=(i % 8 == 7))
    print(f"scenario families done in {time.time() - t0:.1f}s")


# --------------------------------------------------------------------------------------- mutation testing
MUTATIONS = [
    ("MonkeyRand multiplier", 'uint32_t next_seed = (seed * 5u + 1u) & 0xFFFFu;', 'uint32_t next_seed = (seed * 5u + 2u) & 0xFFFFu;'),
    ("GetAnimIndex state 8", "if (state == 3 || state == 8) {", "if (state == 3 || state == 9) {"),
    ("StoreActivities state 0x2B", 'RD8(slot_monkey + AE_MK_AI_STATE) == 0x2B', 'RD8(slot_monkey + AE_MK_AI_STATE) == 0x2A'),
    ("Gimmick type 49", "    case 49:\n        return RDS16(sp + 2) == 0;", "    case 49:\n        return RDS16(sp + 2) != 0;"),
    ("NavNodeGetLink mask", 'return (int32_t)(RD16(RD32(n2 + AE_NAV_LINKS) + (uint32_t)pos * 2u) & 0x3FFFu);', 'return (int32_t)(RD16(RD32(n2 + AE_NAV_LINKS) + (uint32_t)pos * 2u) & 0x7FFFu);'),
    ("BossArena area", "return RD16(AE_G_CURRENT_AREA) == 0x46;", "return RD16(AE_G_CURRENT_AREA) == 0x45;"),
    ("IdleEnd set", "static const uint8_t set[] = {15, 18, 21, 60};", "static const uint8_t set[] = {15, 18, 21, 61};"),
    ("Projectile y offset", 'RD16(source_monkey + AE_MK_POS + 4) - 200u', 'RD16(source_monkey + AE_MK_POS + 4) - 201u'),
    ("Projectile counter", 'projectile_arg = (projectile_arg - q * 5u) & 0xFFu;', 'projectile_arg = (projectile_arg - q * 4u) & 0xFFu;'),
    ("AngleBetween cross rounding", 'if (cross < 0) cross = (int32_t)((uint32_t)cross + 4095u);', 'if (cross < 0) cross = (int32_t)((uint32_t)cross + 4094u);'),
    ("DistanceIntToShort3D uses y", "int32_t sum = (int32_t)((uint32_t)ae_sdk_square0_lane(dx) + (uint32_t)ae_sdk_square0_lane(dz));\n    return ae_sdk_SquareRoot0(r, sum);\n}\n\n/* Distance3D",
     "int32_t sum = (int32_t)((uint32_t)ae_sdk_square0_lane(dx) + (uint32_t)ae_sdk_square0_lane(dz) + (uint32_t)ae_sdk_square0_lane(RD32(a + 4)));\n    return ae_sdk_SquareRoot0(r, sum);\n}\n\n/* Distance3D"),
    ("DotXZ rounding", "if (v < 0) v = (int32_t)((uint32_t)v + 4095u);", "if (v < 0) v = (int32_t)((uint32_t)v + 4096u);"),
    ("Atan2 x == 0", "return y > 0 ? 1024 : 3072;", "return y >= 0 ? 1024 : 3072;"),
    ("NavNodeIsSpecial mask", "(flags & 0x1800Cu) != 0", "(flags & 0x1800Du) != 0"),
    ("LinkIdsEx table", "WR16(o1, RD16(AE_G_LINK_CODE_TABLE + idx));", "WR16(o1, RD16(AE_G_LINK_CODE_TABLE + idx + 2));"),
    ("ReachedNode state 9", '(uint32_t)x << 4) - (uint32_t)x);\n        int32_t hi = mul_hi(scaled_radius, 0x66666667);', '(uint32_t)x << 4) + (uint32_t)x);\n        int32_t hi = mul_hi(scaled_radius, 0x66666667);'),
    ("ReachedNode area", "if (area == 0x4D) {", "if (area == 0x4C) {"),
    ("Backtracked prev1", 'if (RD16(monkey + AE_MK_NAV_NODE1) != RD16(monkey + AE_MK_NAV_PREV1)) return 0;', 'if (RD16(monkey + AE_MK_NAV_NODE1) != RD16(monkey + AE_MK_NAV_PREV0)) return 0;'),
    ("CallVoice 2", "int32_t ae_MonkeyGetCallVoice2(AeRam *r) { return call_voice(r, 85, 86, 87); }", "int32_t ae_MonkeyGetCallVoice2(AeRam *r) { return call_voice(r, 85, 86, 86); }"),
    ("Facing table", "case 3: x = -2896; z = 2896; break;", "case 3: x = -2896; z = -2896; break;"),
    ("AttackChance x100", 'return times100((int32_t)RD16(e + AE_MK_INFO_LOD_ATTACK_CHANCE));', 'return times100((int32_t)RD16(e + AE_MK_INFO_ATTACK_COUNT_7));'),
    ("ApproachNear shift", 'int32_t degree_quotient = sra32((int32_t)((uint32_t)degree_product + (uint32_t)angle_fixed), 8);\n        return', 'int32_t degree_quotient = sra32((int32_t)((uint32_t)degree_product + (uint32_t)angle_fixed), 9);\n        return'),
    ("ApproachFar divisor", 'angle_fixed = div_trap(r, angle_fixed, 2740);', 'angle_fixed = div_trap(r, angle_fixed, 2741);'),
    ("LinkFlag stop", 'if (wanted_link < link_id) return 0;', 'if (wanted_link <= link_id) return 0;'),
    ("StaticModelDraw bones", 'AE_G_MODEL_BONE_BUFFER + (RD16(RD32(obj) + AE_MK_MODEL_BONE_COUNT) << 5)', 'AE_G_MODEL_BONE_BUFFER + (RD16(RD32(obj) + AE_MK_MODEL_BONE_COUNT) << 4)'),
    ("Interpolate z", 'WR16(monkey + AE_MK_MOVE_DIR + 4, RD16(monkey + AE_MK_LINK_FROM + 8) + (uint32_t)offset_z);', 'WR16(monkey + AE_MK_MOVE_DIR + 4, RD16(monkey + AE_MK_LINK_FROM + 4) + (uint32_t)offset_z);'),
    ("ResetNavVisits mask", "flags & 0xF7FFFFFFu", "flags & 0xEFFFFFFFu"),
    ("GroundHeight compare", 'if (!(abs_dx < abs_dz)) {', 'if (!(abs_dx <= abs_dz)) {'),
    ("ContactOwnerType mask", 'RD16(o + AE_MK_CONTACT_OWNER_KIND) & 0xF0u', 'RD16(o + AE_MK_CONTACT_OWNER_KIND) & 0xF1u'),
    ("BonePos translation", "WR16(out + 2, y);", "WR16(out + 2, y + 1u);"),
    ("DebugMenuInit2 loop", '} while (range_count < 8);', '} while (range_count < 7);'),
    ("DelayedSfx decrement", 'if (timer >= 0) WR16(voice_entry + AE_MK_VOICE_TIMER, timer_bits - 1u);', 'if (timer > 0) WR16(voice_entry + AE_MK_VOICE_TIMER, timer_bits - 1u);'),
    ("QueueVoice scan", "if (i >= 16) break;", "if (i >= 15) break;"),
    ("BodyRegister type", 'ae_CollisionBodyRegister(r, h ? h->body : NULL, 3, monkey + AE_MK_POS, monkey);', 'ae_CollisionBodyRegister(r, h ? h->body : NULL, 4, monkey + AE_MK_POS, monkey);'),
    ("sdk normalize shift", "int32_t t6 = sra32(31 - v1, 1);", "int32_t t6 = sra32(32 - v1, 1);"),
    ("sdk ratan2 table", "v1 = (int32_t)(1024u - (uint32_t)v1);", "v1 = (int32_t)(1025u - (uint32_t)v1);"),
    ("sdk sqrt shift", "return (int32_t)((uint32_t)t5 >> 12);", "return (int32_t)((uint32_t)t5 >> 13);"),
]


def run_mutations(scale, seed):
    src_files = {"src/monkeys/ae_monkey_base.c": ROOT / "src/monkeys/ae_monkey_base.c",
                 "src/core/ae_sdk.c": ROOT / "src/core/ae_sdk.c"}
    survivors = []
    tmp = Path(tempfile.mkdtemp(prefix="mk_mut_"))
    h = mh.MonkeyHarness(FUNCS)
    for k, (label, old, new) in enumerate(MUTATIONS):
        rel = "src/core/ae_sdk.c" if label.startswith("sdk") else "src/monkeys/ae_monkey_base.c"
        text = src_files[rel].read_text(encoding="utf-8")
        if old not in text:
            print(f"MUTATION {label!r}: pattern not found")
            survivors.append(label + " (pattern missing)")
            continue
        mp = tmp / f"mut{k}_{Path(rel).name}"
        mp.write_text(text.replace(old, new, 1), encoding="utf-8")
        h.reload_c({rel: mp}, tag=f"mkmut{k}")
        h.failures = 0
        h.stop_after = 1
        # Other monkey checkers restrict each mutation to the directly affected
        # scenario family. Do the same here; default runs still test every family.
        affected = {
            "MonkeyRand multiplier": "MonkeyRand", "GetAnimIndex state 8": "MonkeyGetAnimIndex",
            "StoreActivities state 0x2B": "MonkeyStoreActivities", "Gimmick type 49": "GimmickAllowsMonkeyPassage",
            "NavNodeGetLink mask": "NavNodeGetLink", "BossArena area": "AreaIsBossArena",
            "IdleEnd set": "MonkeyIsIdleEndState", "Projectile y offset": "MonkeySpawnProjectile",
            "Projectile counter": "MonkeySpawnProjectile", "AngleBetween cross rounding": "AngleBetweenXZ",
            "DistanceIntToShort3D uses y": "DistanceIntToShort3D", "DotXZ rounding": "DotXZ_SV",
            "Atan2 x == 0": "Atan2Angle4096", "NavNodeIsSpecial mask": "NavNodeIsSpecial",
            "LinkIdsEx table": "NavNodeGetLinkIdsEx", "ReachedNode state 9": "MonkeyReachedNode",
            "ReachedNode area": "MonkeyReachedNode", "Backtracked prev1": "MonkeyNavBacktracked",
            "CallVoice 2": "MonkeyGetCallVoice2", "Facing table": "MonkeyFacingFromLinkCode",
            "AttackChance x100": "MonkeyAttackChance", "ApproachNear shift": "MonkeyApproachAngleNear",
            "ApproachFar divisor": "MonkeyApproachAngleFar", "LinkFlag stop": "NavNodeLinkFlag",
            "StaticModelDraw bones": "StaticModelDraw", "Interpolate z": "MonkeyInterpolateLinkVector",
            "ResetNavVisits mask": "MonkeyResetNavVisits", "GroundHeight compare": "NavGroundHeightBetween",
            "ContactOwnerType mask": "ActorContactOwnerType", "BonePos translation": "ObjGetBonePos",
            "DebugMenuInit2 loop": "MonkeyDebugMenuInit2", "DelayedSfx decrement": "DelayedSfxUpdate",
            "QueueVoice scan": "MonkeyQueueVoice", "BodyRegister type": "MonkeyBodyRegister",
            "sdk normalize shift": "VectorNormal (SDK)", "sdk ratan2 table": "ratan2 (SDK)",
            "sdk sqrt shift": "SquareRoot0 (SDK)",
        }
        if "run" in h.__dict__:
            del h.__dict__["run"]
        only = affected.get(label)
        if only:
            real_run = h.run
            h.run = lambda name, *a, _real=real_run, _only=only, **kw: _real(name, *a, **kw) if name == _only else None
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
    ap.add_argument("--seed", type=int, default=0x944230)
    ap.add_argument("--coverage", action="store_true")
    ap.add_argument("--trapv", action="store_true", help="build the port with -ftrapv (signed overflow aborts)")
    ap.add_argument("--mutate", action="store_true", help="mutation-test the checker (slow)")
    ap.add_argument("--stop-after", type=int, default=0)
    a = ap.parse_args()
    if not (ROOT / "extract/disc/SCUS_944.23").exists():
        print("SKIP: extract/disc/SCUS_944.23 not present")
        return 0
    if a.mutate:
        survivors = run_mutations(a.cases, a.seed)
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
