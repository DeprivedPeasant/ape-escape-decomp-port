/*
 * Monkey base routines, hand-ported and verified against the original game.
 * Field layouts and integration hooks are declared in ae_monkey.h.
 */
#include "ae_monkey.h"

/* ---- helpers ---------------------------------------------------------------------------------- */
#define RD8(a) ae_rd8(r, (a))
#define RD16(a) ae_rd16(r, (a))
#define RD32(a) ae_rd32(r, (a))
#define RDS16(a) ae_rds16(r, (a))
#define RDS32(a) ae_rds32(r, (a))
#define WR8(a, v) ae_wr8(r, (a), (uint32_t)(v))
#define WR16(a, v) ae_wr16(r, (a), (uint32_t)(v))
#define WR32(a, v) ae_wr32(r, (a), (uint32_t)(v))

/* Narrow to a signed halfword with wraparound. */
static int32_t s16(uint32_t v) { return (int32_t)(int16_t)(uint16_t)v; }
/* Shift right with sign extension. */
static int32_t sra32(int32_t v, uint32_t n) {
    n &= 31u;
    return v < 0 ? (int32_t)~((~(uint32_t)v) >> n) : (int32_t)((uint32_t)v >> n);
}
/* Read the current monkey address. */
static uint32_t cur_monkey(AeRam *r) { return RD32(AE_G_CUR_MONKEY); }
/* Locate the information record for a monkey ID. */
static uint32_t info_entry(AeRam *r, uint32_t id) { return RD32(AE_G_MONKEY_INFO_TABLE) + id * AE_MONKEY_INFO_SIZE; }

/* Report a missing integration hook. */
int32_t ae_monkey_hook_missing(AeRam *r, const char *name) {
    ae_ram_fail(r, "AeMonkeyHooks.%s not provided", name);
    return 0;
}

/* MIPS div: the original checks for a zero divisor and INT_MIN / -1 and executes break (a trap). */
/* Divide signed values and report invalid divisors. */
static int32_t div_trap(AeRam *r, int32_t a, int32_t b) {
    if (b == 0 || (b == -1 && a == INT32_MIN)) {
        ae_ram_fail(r, "divide trap (div by zero / INT_MIN / -1; the original executes break)");
        return 0;
    }
    return a / b;
}

/* signed multiply, low 32 bits (mult + mflo) */
/* Multiply with 32-bit wraparound. */
static int32_t mul_lo(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
/* signed multiply, high 32 bits (mult + mfhi) */
/* Return the signed high half of a 64-bit product. */
static int32_t mul_hi(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * (int64_t)b) >> 32); }

/* ================================================================================================
 * Random / small queries
 * ============================================================================================== */

/* MonkeyRand @80032E30 */
/* Advance the monkey random seed and return its high byte. */
uint32_t ae_MonkeyRand(AeRam *r) {
    uint32_t seed = RD32(AE_G_MONKEY_RAND_SEED);
    uint32_t next_seed = (seed * 5u + 1u) & 0xFFFFu;
    WR32(AE_G_MONKEY_RAND_SEED, next_seed);
    return next_seed >> 8;
}

/* MonkeyCaughtGroupValue @80032E58: MonkeyInfo[caught].callVoice */
/* Read the captured monkey voice group. */
uint32_t ae_MonkeyCaughtGroupValue(AeRam *r) {
    int32_t idx = RDS16(AE_G_CAUGHT_MONKEY);
    return RD16(RD32(AE_G_MONKEY_INFO_TABLE) + (uint32_t)idx * AE_MONKEY_INFO_SIZE);
}

/* MonkeyGetAnimIndex @80032E84 */
/* Select the animation index for the current AI state. */
int32_t ae_MonkeyGetAnimIndex(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    uint32_t state = RD8(monkey + AE_MK_AI_STATE);
    if (state == 3 || state == 8) {
        uint32_t id = RD8(monkey + AE_MK_ID);
        if (RDS16(info_entry(r, id) + AE_MK_INFO_ALERT_SPEED) == 0) return 0;
    }
    monkey = cur_monkey(r);
    state = RD8(monkey + AE_MK_AI_STATE);
    if (state - 50u < 3u) return RDS16(monkey + AE_MK_ANIM_STATE_FRAME);
    return (int32_t)RD32(AE_G_MONKEY_STATE_TABLE + RD8(monkey + AE_MK_AI_STATE) * 8u);
}

/* MonkeyNop @80033000 */
/* Return the original constant query value of two. */
int32_t ae_MonkeyNop(AeRam *r) {
    (void)r;
    return 2;
}

/* MonkeyBodyRegister @80033008: registers the monkey's collision body (type 3) once (flags3 bit 1). */
/* Register the monkey collision body and targeting entry once. */
int32_t ae_MonkeyBodyRegister(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    uint32_t flags = RD32(monkey + AE_MK_FLAGS3);
    if ((flags >> 1) & 1u) return 0;
    WR32(monkey + AE_MK_FLAGS3, flags | AE_MK_FLAGS3_BODY_REGISTERED);
    ae_CollisionBodyRegister(r, h ? h->body : NULL, 3, monkey + AE_MK_POS, monkey);
    AE_MK_HOOK(r, h, TargetListAdd, monkey + AE_MK_POS_SHORT, 0, 0, 0);
    return 1;
}

/* MonkeyBodyUnregister @80033068 */
/* Remove the monkey collision body and targeting entry once. */
int32_t ae_MonkeyBodyUnregister(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    uint32_t flags = RD32(monkey + AE_MK_FLAGS3);
    if (!((flags >> 1) & 1u)) return 0;
    WR32(monkey + AE_MK_FLAGS3, flags & ~AE_MK_FLAGS3_BODY_REGISTERED);
    ae_CollisionBodyUnregister(r, h ? h->body : NULL, monkey + AE_MK_POS);
    AE_MK_HOOK(r, h, TargetListRemove, monkey + AE_MK_POS_SHORT, 0, 0, 0);
    return 1;
}

/* MonkeyActivityReset @8003317C */
/* Clear the saved activities of all monkey slots. */
void ae_MonkeyActivityReset(AeRam *r) {
    for (uint32_t i = 0; i < 8; ++i) WR8(AE_G_MONKEY_ACTIVITY + i, 0);
}

/* MonkeyStoreActivities @800334C8: walks all 8 monkey slots (g_CurMonkey is left pointing at the last one). */
/* Save activities with the original capture and object-node conversions. */
void ae_MonkeyStoreActivities(AeRam *r) {
    for (uint32_t i = 0; i < AE_MONKEY_COUNT; ++i) {
        uint32_t slot_monkey = AE_G_MONKEYS + i * AE_MONKEY_SIZE;
        uint32_t saved_activity = AE_G_MONKEY_ACTIVITY + i;
        WR32(AE_G_CUR_MONKEY, slot_monkey);
        if (RDS16(AE_G_MONKEY_COUNT) == 0) { WR8(saved_activity, 0); continue; }
        if (RD8(AE_G_MONKEY_FLAG_4CF4) != 0) { WR8(saved_activity, 0); continue; }
        if (RD8(slot_monkey + AE_MK_ACTIVITY) == 4) {
            uint32_t flags = RD32(RD32(AE_G_NAV_NODES) + RD16(slot_monkey + AE_MK_NAV_NODE1) * AE_NAV_NODE_SIZE);
            if ((flags & 0x400u) != 0 && RD8(slot_monkey + AE_MK_AI_STATE) == 0x2B) { WR8(saved_activity, 5); continue; }
        }
        uint32_t act = RD8(cur_monkey(r) + AE_MK_ACTIVITY);
        if (act == 1) WR8(saved_activity, 0);
        else if (act == 3) WR8(saved_activity, 2);
        else WR8(saved_activity, RD8(slot_monkey + AE_MK_ACTIVITY));
    }
}

/* GimmickAllowsMonkeyPassage @800335D0: GimmickGetInfo(gimmick, &{s16 type, s16 value}) then a per type test of the value. */
/* Test the reported gimmick state for monkey passage. */
int32_t ae_GimmickAllowsMonkeyPassage(AeRam *r, const AeMonkeyHooks *h, uint32_t gimmick) {
    uint32_t sp = AE_STK(AE_STK_GIMMICK_PASSAGE);
    int32_t got = AE_MK_HOOK(r, h, GimmickGetInfo, gimmick, sp, 0, 0);
    if (got == -1) return 0;
    int32_t type = RDS16(sp);
    switch (type) {
    case 1: {
        int32_t value = RDS16(sp + 2);
        if (value == 0) return 1;
        return value == 5;
    }
    case 6:
        return RDS16(sp + 2) == 3;
    case 9:
    case 17:
    case 24:
        return RDS16(sp + 2) > 0;
    case 49:
        return RDS16(sp + 2) == 0;
    case 101: {
        uint32_t value = RD16(sp + 2);
        if (value < 2u) return 1;
        return s16(value) == 5;
    }
    default:
        return RDS16(sp + 2) == 2;
    }
}

/* MonkeyVoiceQueueReset @80033708 */
/* Free every delayed voice entry. */
void ae_MonkeyVoiceQueueReset(AeRam *r) {
    for (uint32_t i = 0; i < AE_VOICE_QUEUE_COUNT; ++i) {
        uint32_t voice_entry = AE_G_VOICE_QUEUE + i * AE_VOICE_QUEUE_SIZE;
        WR32(voice_entry, 0xFFFFFFFFu);
        WR16(voice_entry + AE_MK_VOICE_TIMER, 0xFFFFu);
        WR32(voice_entry + AE_MK_VOICE_POS + 8, 0);
        WR32(voice_entry + AE_MK_VOICE_POS + 4, 0);
        WR32(voice_entry + AE_MK_VOICE_POS, 0);
    }
}

/* MonkeyQueueVoice @80033744: first free entry (timer < 0) gets (sfx, timer, monkey position). */
/* Queue a delayed sound at the current monkey position. */
void ae_MonkeyQueueVoice(AeRam *r, const AeMonkeyHooks *h, int32_t sfx, int32_t timer) {
    uint32_t monkey = cur_monkey(r);
    uint32_t voice_entry = AE_G_VOICE_QUEUE;
    for (int32_t i = 1;; ++i, voice_entry += AE_VOICE_QUEUE_SIZE) {
        if (RDS16(voice_entry + AE_MK_VOICE_TIMER) < 0) {
            WR32(voice_entry, sfx);
            WR16(voice_entry + AE_MK_VOICE_TIMER, timer);
            WR32(voice_entry + AE_MK_VOICE_POS, RD32(monkey + AE_MK_POS));
            WR32(voice_entry + AE_MK_VOICE_POS + 4, RD32(monkey + AE_MK_POS + 4));
            WR32(voice_entry + AE_MK_VOICE_POS + 8, RD32(monkey + AE_MK_POS + 8));
            return;
        }
        if (i >= 16) break;
    }
    AE_MK_HOOK(r, h, Printf, AE_G_VOICE_QUEUE_FULL_TEXT, 0, 0, 0);
}

/* DelayedSfxUpdate @800337C4: counts the queue timers down; at 0 the sound is played at the stored position. */
/* Play due sounds and advance the voice queue timers. */
void ae_DelayedSfxUpdate(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_DELAYED_SFX);
    for (uint32_t voice_entry = AE_G_VOICE_QUEUE; voice_entry < AE_G_VOICE_QUEUE + AE_VOICE_QUEUE_COUNT * AE_VOICE_QUEUE_SIZE;
         voice_entry += AE_VOICE_QUEUE_SIZE) {
        int32_t timer = RDS16(voice_entry + AE_MK_VOICE_TIMER);
        if (timer == 0) {
            if (RDS32(voice_entry) > 0) {
                WR16(sp + 0, RD16(voice_entry + AE_MK_VOICE_POS));
                WR16(sp + 2, RD16(voice_entry + AE_MK_VOICE_POS + 4));
                WR16(sp + 4, RD16(voice_entry + AE_MK_VOICE_POS + 8));
                AE_MK_HOOK(r, h, PlaySfxAt, RD32(voice_entry), sp, 0, 0);
            }
            timer = RDS16(voice_entry + AE_MK_VOICE_TIMER);
        }
        uint32_t timer_bits = RD16(voice_entry + AE_MK_VOICE_TIMER);
        if (timer >= 0) WR16(voice_entry + AE_MK_VOICE_TIMER, timer_bits - 1u);
    }
}

/* NavNodeGetLink @8003386C: the `index`-th link (counting only links with bit 14 set) of the node, flag stripped. */
/* Return an active link by its ordinal index. */
int32_t ae_NavNodeGetLink(AeRam *r, int32_t node, int32_t index) {
    int32_t wanted = s16((uint32_t)index);
    if (wanted < 0) return -1;
    int32_t found = -1;                         /* index of the last link with bit 14 seen */
    uint32_t n = RD32(AE_G_NAV_NODES) + (uint32_t)s16((uint32_t)node) * AE_NAV_NODE_SIZE;
    uint32_t link_count = RD8(n + AE_NAV_LINK_COUNT);
    if (link_count == 0) return -1;
    uint32_t link_ptr = RD32(n + AE_NAV_LINKS);
    for (int32_t pos = 0; pos < (int32_t)link_count; pos++, link_ptr += 2) {
        if (RD16(link_ptr) & 0x4000u) ++found;
        if (found == wanted) {
            uint32_t n2 = RD32(AE_G_NAV_NODES) + (uint32_t)s16((uint32_t)node) * AE_NAV_NODE_SIZE;
            return (int32_t)(RD16(RD32(n2 + AE_NAV_LINKS) + (uint32_t)pos * 2u) & 0x3FFFu);
        }
    }
    return -1;
}

/* AreaIsSpecialMonkeyArea @8003398C */
/* Test the three areas that use special monkey approach rules. */
int32_t ae_AreaIsSpecialMonkeyArea(AeRam *r) {
    uint32_t a = RD16(AE_G_CURRENT_AREA);
    return a == 0x29 || a == 0x43 || a == 0x35;
}

/* MonkeyIsState1E_to_20 @800339CC */
/* Test the three flying AI states. */
int32_t ae_MonkeyIsState1E_to_20(AeRam *r) {
    return RD8(cur_monkey(r) + AE_MK_AI_STATE) - 30u < 3u;
}

/* AreaIsBossArena @800339EC */
/* Test the area kind and the additional boss area. */
int32_t ae_AreaIsBossArena(AeRam *r) {
    uint32_t kind = RD16(AE_G_AREA_KIND);
    if (kind == 3 || kind == 6 || kind == 8) return 1;
    return RD16(AE_G_CURRENT_AREA) == 0x46;
}

/* state tests (lbu g_CurMonkey->aiState) */
/* Test whether the current state belongs to a set. */
static int state_in(AeRam *r, const uint8_t *set, int n) {
    uint32_t s = RD8(cur_monkey(r) + AE_MK_AI_STATE);
    for (int i = 0; i < n; ++i) if (s == set[i]) return 1;
    return 0;
}

/* MonkeyIsIdleActivityState @80033B10 */
/* Test the idle activity state set. */
int32_t ae_MonkeyIsIdleActivityState(AeRam *r) {
    static const uint8_t set[] = {1, 13, 16, 19, 58, 14, 17, 20, 59, 15, 18, 21, 60};
    return state_in(r, set, (int)sizeof set);
}

/* MonkeyIsIdleEndState @80033B9C */
/* Test the final phase of an idle activity. */
int32_t ae_MonkeyIsIdleEndState(AeRam *r) {
    static const uint8_t set[] = {15, 18, 21, 60};
    return state_in(r, set, (int)sizeof set);
}

/* MonkeyIsAltAnimState @80033BE0 */
/* Test the states that use node-selected animations. */
int32_t ae_MonkeyIsAltAnimState(AeRam *r) {
    return RD8(cur_monkey(r) + AE_MK_AI_STATE) - 50u < 3u;
}

/* MonkeyIsState0C_or_24 @80033C00 */
/* Test the two fast ground movement states. */
int32_t ae_MonkeyIsState0C_or_24(AeRam *r) {
    static const uint8_t set[] = {12, 36};
    return state_in(r, set, (int)sizeof set);
}

/* MonkeyIsJumpArcState @80033C34 */
/* Test the states driven by jump arcs. */
int32_t ae_MonkeyIsJumpArcState(AeRam *r) {
    static const uint8_t set[] = {4, 44, 45, 46, 49};
    return state_in(r, set, (int)sizeof set);
}

/* MonkeyIsAttackState @80033C80 */
/* Test the attack state set. */
int32_t ae_MonkeyIsAttackState(AeRam *r) {
    static const uint8_t set[] = {64, 5, 65, 37, 6};
    return state_in(r, set, (int)sizeof set);
}

/* MonkeySpawnProjectile @80033CCC: ProjectileSpawn(&pos, monkey, type, &offsetToTarget | counter % 5). */
/* Spawn the requested projectile using the monkey and target positions. */
int32_t ae_MonkeySpawnProjectile(AeRam *r, const AeMonkeyHooks *h, int32_t type) {
    uint32_t sp = AE_STK(AE_STK_SPAWN_PROJECTILE);       /* sp+16 -> sp+0, sp+24 -> sp+8 */
    uint32_t source_monkey = cur_monkey(r);
    if ((uint32_t)type - 3u < 2u) {
        WR16(sp + 0, RD16(source_monkey + AE_MK_POS));
        WR16(sp + 2, RD16(source_monkey + AE_MK_POS + 4));
    } else {
        WR16(sp + 0, RD16(source_monkey + AE_MK_POS));
        WR16(sp + 2, RD16(source_monkey + AE_MK_POS + 4) - 200u);
    }
    WR16(sp + 4, RD16(source_monkey + AE_MK_POS + 8));
    uint32_t target = RD32(AE_G_MONKEY_TARGET);
    uint32_t monkey = cur_monkey(r);
    WR16(sp + 8, RD16(target + AE_MK_POS) - RD16(monkey + AE_MK_POS));
    WR16(sp + 10, RD16(target + AE_MK_POS + 4) - RD16(monkey + AE_MK_POS + 4));
    WR16(sp + 12, RD16(target + AE_MK_POS + 8) - RD16(monkey + AE_MK_POS + 8));
    uint32_t projectile_arg;
    if (type == 6) {
        uint32_t cnt = RD8(AE_G_PROJECTILE_COUNTER);
        projectile_arg = cnt & 0xFFu;
        WR8(AE_G_PROJECTILE_COUNTER, cnt + 1u);
        uint32_t hi = (uint32_t)(((uint64_t)projectile_arg * 0xCCCCCCCDull) >> 32);
        uint32_t q = hi >> 2;
        projectile_arg = (projectile_arg - q * 5u) & 0xFFu;
    } else {
        projectile_arg = sp + 8;
    }
    return AE_MK_HOOK(r, h, ProjectileSpawn, sp, monkey, type, projectile_arg);
}

/* ================================================================================================
 * Vector maths (SDK maths from ae_sdk)
 * ============================================================================================== */

/* AngleBetweenXZ @800344C8: ratan2 of the cross and dot product of the two normalised vectors (>> 12). */
/* Measure the angle between two normalized horizontal vectors. */
int32_t ae_AngleBetweenXZ(AeRam *r, uint32_t a, uint32_t b) {
    int32_t n1[3], n2[3];
    int32_t first_vector[3] = {RDS32(a), RDS32(a + 4), RDS32(a + 8)};
    int32_t second_vector[3] = {RDS32(b), RDS32(b + 4), RDS32(b + 8)};
    ae_sdk_normalize_core(r, first_vector[0], first_vector[1], first_vector[2], n1);
    if (ae_ram_failed(r)) return 0;
    ae_sdk_normalize_core(r, second_vector[0], second_vector[1], second_vector[2], n2);
    if (ae_ram_failed(r)) return 0;
    int32_t dot_x = mul_lo(n1[0], n2[0]);
    int32_t cross_forward = mul_lo(n1[0], n2[2]);
    int32_t cross = mul_lo(n1[2], n2[0]);
    cross = (int32_t)((uint32_t)cross_forward - (uint32_t)cross);
    int32_t dot = (int32_t)((uint32_t)dot_x + (uint32_t)mul_lo(n1[2], n2[2]));
    int32_t dot_scaled = sra32(dot, 12);
    if (cross < 0) cross = (int32_t)((uint32_t)cross + 4095u);
    cross = sra32(cross, 12);
    return ae_sdk_ratan2(r, cross, dot_scaled);
}

/* DistanceIntToShort3D @800345D4: sqrt(dx^2 + dz^2) of (s32 vector - s16 vector); Y is never squared. */
/* Measure horizontal distance between integer and short positions. */
int32_t ae_DistanceIntToShort3D(AeRam *r, uint32_t a, uint32_t b) {
    int32_t dx = (int32_t)(RD32(a) - (uint32_t)RDS16(b));
    int32_t dz = (int32_t)(RD32(a + 8) - (uint32_t)RDS16(b + 4));
    (void)RD32(a + 4); (void)RDS16(b + 2);    /* y is read and subtracted but not used further */
    int32_t sum = (int32_t)((uint32_t)ae_sdk_square0_lane(dx) + (uint32_t)ae_sdk_square0_lane(dz));
    return ae_sdk_SquareRoot0(r, sum);
}

/* Distance3D @80034684 */
/* Measure distance between two integer positions. */
int32_t ae_Distance3D(AeRam *r, uint32_t a, uint32_t b) {
    int32_t dx = (int32_t)(RD32(a) - RD32(b));
    int32_t dy = (int32_t)(RD32(a + 4) - RD32(b + 4));
    int32_t dz = (int32_t)(RD32(a + 8) - RD32(b + 8));
    int32_t sum = (int32_t)((uint32_t)ae_sdk_square0_lane(dx) + (uint32_t)ae_sdk_square0_lane(dy));
    sum = (int32_t)((uint32_t)sum + (uint32_t)ae_sdk_square0_lane(dz));
    return ae_sdk_SquareRoot0(r, sum);
}

/* DistanceXZ @8003473C */
/* Measure horizontal distance between two integer positions. */
int32_t ae_DistanceXZ(AeRam *r, uint32_t a, uint32_t b) {
    int32_t dx = (int32_t)(RD32(a) - RD32(b));
    int32_t dz = (int32_t)(RD32(a + 8) - RD32(b + 8));
    int32_t sum = (int32_t)((uint32_t)ae_sdk_square0_lane(dx) + (uint32_t)ae_sdk_square0_lane(dz));
    return ae_sdk_SquareRoot0(r, sum);
}

/* MonkeyCalcMoveDir @800347D8: wishDir = normalise(goalPos - pos) (16 bit wraparound subtraction). */
/* Normalize the wrapped short direction toward the goal. */
void ae_MonkeyCalcMoveDir(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    WR16(monkey + AE_MK_WISH_DIR + 0, RD16(monkey + AE_MK_GOAL_POS + 0) - RD16(monkey + AE_MK_POS));
    uint32_t y = RD16(monkey + AE_MK_GOAL_POS + 4) - RD16(monkey + AE_MK_POS + 4);
    uint32_t z = RD16(monkey + AE_MK_GOAL_POS + 8) - RD16(monkey + AE_MK_POS + 8);
    WR16(monkey + AE_MK_WISH_DIR + 2, y);
    WR16(monkey + AE_MK_WISH_DIR + 4, z);
    ae_sdk_VectorNormalSS(r, monkey + AE_MK_WISH_DIR, monkey + AE_MK_WISH_DIR);
}

/* rounding of the dot products: (v + (v < 0 ? 4095 : 0)) << 4 >> 16 */
/* Round a fixed-point dot product toward zero and narrow it. */
static int32_t dot_round(int32_t v) {
    if (v < 0) v = (int32_t)((uint32_t)v + 4095u);
    return sra32((int32_t)((uint32_t)v << 4), 16);
}

/* DotXZ_SV @800348CC */
/* Compute the normalized horizontal dot product of short vectors. */
int32_t ae_DotXZ_SV(AeRam *r, uint32_t a, uint32_t b) {
    int32_t n1[3], n2[3];
    ae_sdk_normalize_core(r, RDS16(a), RDS16(a + 2), RDS16(a + 4), n1);
    if (ae_ram_failed(r)) return 0;
    ae_sdk_normalize_core(r, RDS16(b), RDS16(b + 2), RDS16(b + 4), n2);
    if (ae_ram_failed(r)) return 0;
    int32_t x = s16((uint32_t)n1[0]) * s16((uint32_t)n2[0]);       /* the results were stored as s16 and reloaded */
    int32_t z = s16((uint32_t)n1[2]) * s16((uint32_t)n2[2]);
    return dot_round((int32_t)((uint32_t)x + (uint32_t)z));
}

/* DotXZ_V @80034940 */
/* Compute the normalized horizontal dot product of integer vectors. */
int32_t ae_DotXZ_V(AeRam *r, uint32_t a, uint32_t b) {
    int32_t n1[3], n2[3];
    ae_sdk_normalize_core(r, RDS32(a), RDS32(a + 4), RDS32(a + 8), n1);
    if (ae_ram_failed(r)) return 0;
    ae_sdk_normalize_core(r, RDS32(b), RDS32(b + 4), RDS32(b + 8), n2);
    if (ae_ram_failed(r)) return 0;
    int32_t x = mul_lo(n1[0], n2[0]);
    int32_t z = mul_lo(n1[2], n2[2]);
    return dot_round((int32_t)((uint32_t)x + (uint32_t)z));
}

/* Atan2Angle4096 @800349B4: ratan2 mapped to 0..4095; x == 0 gives 1024 (y > 0) or 3072. */
/* Map the vector angle to the unsigned 4096-unit circle. */
int32_t ae_Atan2Angle4096(AeRam *r, int32_t y, int32_t x) {
    if (x == 0) return y > 0 ? 1024 : 3072;
    int32_t v = ae_sdk_ratan2(r, y, x);
    if (v < 0) v += 4096;
    return v;
}

/* ================================================================================================
 * Navigation nodes
 * ============================================================================================== */

/* NavNodeIsSpecial @800349F4: flags & 0x1800C */
/* Test the special navigation-node flag mask. */
int32_t ae_NavNodeIsSpecial(AeRam *r, int32_t node) {
    uint32_t flags = RD32(RD32(AE_G_NAV_NODES) + (uint32_t)s16((uint32_t)node) * AE_NAV_NODE_SIZE);
    return (flags & 0x1800Cu) != 0;
}

/* signed s16 value / 100 truncated toward zero, the way the original does it (mult by 0x51EB851F, hi >> 5, + sign) */
/* Divide by 100 with rounding toward zero. */
static int32_t div100(int32_t s) {
    int32_t hi = mul_hi(s, 0x51EB851F);
    int32_t q = sra32(hi, 5);
    return (int32_t)((uint32_t)q - (uint32_t)(s < 0 ? -1 : 0));
}
/* Multiply by 100 with 32-bit wraparound. */
static int32_t times100(int32_t q16) { return (int32_t)((uint32_t)q16 * 100u); }

/* NavNodeGetLinkIds @80034B74: node field +0x10 -> (field / 100, field % 100 style remainder), +0x12 as is. */
/* Decode the two packed node link fields. */
void ae_NavNodeGetLinkIds(AeRam *r, int32_t node, uint32_t o1, uint32_t o2, uint32_t o3) {
    uint32_t node_offset = (uint32_t)s16((uint32_t)node) * AE_NAV_NODE_SIZE;
    uint32_t f = RD16(RD32(AE_G_NAV_NODES) + node_offset + AE_NAV_LINK_CODE);
    int32_t q = div100(s16(f));
    WR16(o1, q);
    int32_t hundreds = times100(s16((uint32_t)q));
    uint32_t node_addr = RD32(AE_G_NAV_NODES) + node_offset;
    WR16(o2, RD16(node_addr + AE_NAV_LINK_CODE) - (uint32_t)hundreds);
    uint32_t t = RD32(AE_G_NAV_NODES) + node_offset;
    WR16(o3, RD16(t + AE_NAV_LINK_EXTRA_CODE));
}

/* NavNodeGetLinkIdsEx @80034C08 */
/* Decode node link fields and map the first code through the lookup table. */
void ae_NavNodeGetLinkIdsEx(AeRam *r, int32_t node, uint32_t o1, uint32_t o2, uint32_t o3, uint32_t o4) {
    uint32_t node_offset = (uint32_t)s16((uint32_t)node) * AE_NAV_NODE_SIZE;
    uint32_t f = RD16(RD32(AE_G_NAV_NODES) + node_offset + AE_NAV_LINK_CODE);
    int32_t q = div100(s16(f));
    uint32_t idx = (uint32_t)s16((uint32_t)q) * 2u;
    WR16(o1, RD16(AE_G_LINK_CODE_TABLE + idx));
    uint32_t link_code = RD16(RD32(AE_G_NAV_NODES) + node_offset + AE_NAV_LINK_CODE);
    q = div100(s16(link_code));
    int32_t hundreds = times100(s16((uint32_t)q));
    WR16(o2, link_code - (uint32_t)hundreds);
    uint32_t g = RD16(RD32(AE_G_NAV_NODES) + node_offset + AE_NAV_LINK_EXTRA_CODE);
    q = div100(s16(g));
    WR16(o3, q);
    hundreds = times100(s16((uint32_t)q));
    WR16(o4, RD16(RD32(AE_G_NAV_NODES) + node_offset + AE_NAV_LINK_EXTRA_CODE) - (uint32_t)hundreds);
}

/* ObjGetBonePos @80034D30: pose the object's bones, transform bone `bone` by matrix `matrix_index`, add its translation. */
/* Pose and transform an object bone into world space. */
int32_t ae_ObjGetBonePos(AeRam *r, const AeMonkeyHooks *h, int32_t obj, uint32_t matrix_index, uint32_t bone, uint32_t out) {
    uint32_t object = AE_G_OBJS + (uint32_t)s16((uint32_t)obj) * AE_OBJ_SIZE;
    uint32_t matrix = AE_G_BONE_MATRICES + ((matrix_index & 0xFFFFu) << 5);
    if (object == 0) return 0;
    if (!((RD32(object + AE_MK_OBJ_FLAGS) >> 4) & 1u)) return 0;
    AE_MK_HOOK(r, h, MatrixFromDirXZ, object + AE_MK_OBJ_MOVE_DIR, RD32(object + AE_MK_OBJ_BONE_MATRICES), 0, 0);
    AE_MK_HOOK(r, h, ModelPoseBones, object, 3, 0, 0);
    ae_sdk_ApplyMatrixSV(r, matrix, RD32(object + AE_MK_OBJ_VERTS) + ((bone & 0xFFFFu) << 3), out);
    WR16(out + 0, RD16(out + 0) + RD16(matrix + AE_MK_MATRIX_TRANS_X));
    uint32_t y = RD16(out + 2) + RD16(matrix + AE_MK_MATRIX_TRANS_Y);
    uint32_t zz = RD16(out + 4);
    WR16(out + 2, y);
    WR16(out + 4, zz + RD16(matrix + AE_MK_MATRIX_TRANS_Z));
    return 1;
}

/* MonkeyReachedNode @8003516C: has the monkey come within its (state dependent) arrival radius of the goal? */
/* Test distance against the state and area dependent arrival radius. */
int32_t ae_MonkeyReachedNode(AeRam *r) {
    uint32_t source_monkey = cur_monkey(r);
    uint32_t state = RD8(source_monkey + AE_MK_AI_STATE);
    int32_t arrival_radius;
    if (state == 12 || state == 36) {
        arrival_radius = (int32_t)((uint32_t)RDS16(source_monkey + AE_MK_REACH_RADIUS) << 1);
    } else if (state == 9) {
        int32_t x = RDS16(source_monkey + AE_MK_REACH_RADIUS);
        int32_t scaled_radius = (int32_t)(((uint32_t)x << 4) - (uint32_t)x);
        int32_t hi = mul_hi(scaled_radius, 0x66666667);
        arrival_radius = (int32_t)((uint32_t)sra32(hi, 2) - (uint32_t)(scaled_radius < 0 ? -1 : 0));
    } else {
        int half = 0;
        uint32_t area = RD16(AE_G_CURRENT_AREA);
        if (area == 0x4D) {
            uint32_t n = RD16(source_monkey + AE_MK_NAV_NODE0);
            if (n - 5u < 2u) half = 1;
            else if (n == 0x13 || n == 0x14 || n == 0xB4 || n == 0xB5 || n == 0x9E || n == 0x9F || n == 0xA4) half = 1;
        }
        if (!half) {
            area = RD16(AE_G_CURRENT_AREA);
            if (area == 0x1A) {
                uint32_t n = RD16(cur_monkey(r) + AE_MK_NAV_NODE0);
                if (n - 151u < 3u) half = 1;
                else if (n == 156 || n == 157 || n == 158 || n == 159 || n == 160 || n == 169 || n == 170 ||
                         n == 171 || n == 173) half = 1;
            }
        }
        if (half) {
            uint32_t u = RD16(cur_monkey(r) + AE_MK_REACH_RADIUS);
            int32_t sv = s16(u);
            arrival_radius = (int32_t)(((uint32_t)sv + (uint32_t)(u << 16 >> 31)) >> 1);      /* Unsigned shift before narrowing preserves the original rounding. */
        } else {
            arrival_radius = (int32_t)RD16(cur_monkey(r) + AE_MK_REACH_RADIUS);
        }
    }
    uint32_t monkey = cur_monkey(r);
    int32_t radius = s16((uint32_t)arrival_radius);
    if (RD32(monkey + AE_MK_FLAGS2) & 2u) return RDS32(monkey + AE_MK_DIST_TO_GOAL) < radius;
    return RDS32(monkey + AE_MK_DIST_XZ_TO_GOAL) < radius;
}

/* MonkeyNavBacktracked @80035604: do the last three visited nodes repeat the three before? */
/* Detect repeating navigation history. */
int32_t ae_MonkeyNavBacktracked(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    uint32_t node0 = RD16(monkey + AE_MK_NAV_NODE0);
    uint32_t node1 = RD16(monkey + AE_MK_NAV_NODE1);
    uint32_t node2 = RD16(monkey + AE_MK_NAV_NODE2);
    if (node0 == node1) {
        if (node0 == node2) return 1;
    } else if (node0 == node2) {
        /* node0 node1 node0 | node1 node0 ...: nodes 1 and 2 as one word equal the previous nodes 0 and 1 */
        if (RD32(monkey + AE_MK_NAV_NODE1) == RD32(monkey + AE_MK_NAV_PREV0) &&
            RD16(monkey + AE_MK_NAV_PREV0) == RD16(monkey + AE_MK_NAV_PREV2)) {
            return 1;
        }
    }
    monkey = cur_monkey(r);
    if (RD16(monkey + AE_MK_NAV_NODE0) != RD16(monkey + AE_MK_NAV_PREV0)) return 0;
    if (RD16(monkey + AE_MK_NAV_NODE1) != RD16(monkey + AE_MK_NAV_PREV1)) return 0;
    return RD16(monkey + AE_MK_NAV_NODE2) == RD16(monkey + AE_MK_NAV_PREV2);
}

/* MonkeyGetCallVoice @800356C0 / MonkeyGetCallVoice2 @80035750: sound ids by MonkeyInfo.callVoice */
/* Choose a sound ID from the monkey voice group. */
static int32_t call_voice(AeRam *r, int32_t v_192_64, int32_t v_other, int32_t v_0_320) {
    uint32_t v = RD16(info_entry(r, RD8(cur_monkey(r) + AE_MK_ID)));
    if (v == 192 || v == 64) return v_192_64;
    if (v == 0 || v == 320) return v_0_320;
    return v_other;
}
/* Choose the primary monkey call sound. */
int32_t ae_MonkeyGetCallVoice(AeRam *r) { return call_voice(r, 82, 83, 84); }
/* Choose the secondary monkey call sound. */
int32_t ae_MonkeyGetCallVoice2(AeRam *r) { return call_voice(r, 85, 86, 87); }

/* MonkeyFacingFromLinkCode @80036BD0: moveDir (x, 0, z) for link code 0..7 (eight compass directions, 4096 = 1.0 and 2896 = 1/sqrt 2;
   the original dispatches through the constant code table at 0x800A4910); any other code gives (0, 0, -4096). */
/* Set a compass direction from the node link code. */
void ae_MonkeyFacingFromLinkCode(AeRam *r, int32_t code) {
    uint32_t monkey = cur_monkey(r);
    int32_t index = s16((uint32_t)code);
    int32_t x = 0, z = 0;
    WR16(monkey + AE_MK_MOVE_DIR, 0);
    WR16(monkey + AE_MK_MOVE_DIR + 2, 0);
    WR16(monkey + AE_MK_MOVE_DIR + 4, 0);
    switch ((uint32_t)index) {
    case 0: z = -4096; break;
    case 1: x = -2896; z = -2896; break;
    case 2: x = -4096; break;
    case 3: x = -2896; z = 2896; break;
    case 4: z = 4096; break;
    case 5: x = 2896; z = 2896; break;
    case 6: x = 4096; break;
    case 7: x = 2896; z = -2896; break;
    default:
        WR16(monkey + AE_MK_MOVE_DIR + 4, (uint32_t)-4096);
        return;
    }
    WR16(monkey + AE_MK_MOVE_DIR, x);
    WR16(monkey + AE_MK_MOVE_DIR + 4, z);
}

/* MonkeyAttackChance @80037068 */
/* Read the attack probability for the monkey detail level. */
int32_t ae_MonkeyAttackChance(AeRam *r, int32_t index) {
    uint32_t monkey = cur_monkey(r);
    if ((int8_t)RD8(monkey + AE_MK_LOD) <= 0) {
        uint32_t e = info_entry(r, RD8(monkey + AE_MK_ID));
        return (int32_t)RD16(e + ((uint32_t)s16((uint32_t)index) << 3) + AE_MK_INFO_ATTACK_CHANCES);
    }
    if (s16((uint32_t)index) != 7) return s16((uint32_t)index);
    uint32_t e = info_entry(r, RD8(monkey + AE_MK_ID));
    return times100((int32_t)RD16(e + AE_MK_INFO_LOD_ATTACK_CHANCE));
}

/* MonkeyApproachAngleNear @80037D6C / MonkeyApproachAngleFar @80037DEC: angle by distance to the target */
/* Compute the close-range approach angle from target distance. */
int32_t ae_MonkeyApproachAngleNear(AeRam *r) {
    int32_t distance = RDS32(cur_monkey(r) + AE_MK_DIST_TO_TARGET);
    if (distance < 800) return 50;
    if (distance < 2001) {
        int32_t remaining_distance = (int32_t)(2000u - (uint32_t)distance);
        int32_t hi = mul_hi(remaining_distance, 0x2AAAAAAB);
        int32_t angle_fixed = (int32_t)((uint32_t)sra32(hi, 2) - (uint32_t)sra32(remaining_distance, 31));
        angle_fixed = (int32_t)((uint32_t)angle_fixed << 12);
        int32_t degree_product = mul_hi(angle_fixed, (int32_t)0xB60B60B7u);
        int32_t degree_quotient = sra32((int32_t)((uint32_t)degree_product + (uint32_t)angle_fixed), 8);
        return (int32_t)((uint32_t)degree_quotient - (uint32_t)sra32(angle_fixed, 31));
    }
    return 0;
}

/* Compute the long-range approach angle from target distance. */
int32_t ae_MonkeyApproachAngleFar(AeRam *r) {
    int32_t distance = RDS32(cur_monkey(r) + AE_MK_DIST_TO_TARGET);
    if (distance < 260) return 682;
    if (3000 < distance) return 0;
    int32_t remaining_distance = (int32_t)(3000u - (uint32_t)distance);
    int32_t angle_fixed = (int32_t)((((uint32_t)remaining_distance << 4) - (uint32_t)remaining_distance) << 2);
    angle_fixed = div_trap(r, angle_fixed, 2740);
    angle_fixed = (int32_t)((uint32_t)angle_fixed << 12);
    int32_t degree_product = mul_hi(angle_fixed, (int32_t)0xB60B60B7u);
    int32_t degree_quotient = sra32((int32_t)((uint32_t)degree_product + (uint32_t)angle_fixed), 8);
    return (int32_t)((uint32_t)degree_quotient - (uint32_t)sra32(angle_fixed, 31));
}

/* NavNodeLinkFlag @80038608: bit 14 of the link entry for link id `link` in the node's sorted link list (0 if absent) */
/* Find the active flag of a link in a sorted node list. */
int32_t ae_NavNodeLinkFlag(AeRam *r, int32_t node, int32_t link) {
    uint32_t n = RD32(AE_G_NAV_NODES) + (uint32_t)s16((uint32_t)node) * AE_NAV_NODE_SIZE;
    uint32_t link_count = RD8(n + AE_NAV_LINK_COUNT);
    if (link_count == 0) return 0;
    int32_t wanted_link = s16((uint32_t)link);
    uint32_t link_ptr = RD32(n + AE_NAV_LINKS);
    uint32_t links_seen = 0;
    for (;;) {
        uint32_t link_entry = RD16(link_ptr);
        int32_t link_id = (int32_t)(link_entry & 0x3FFFu);
        if (wanted_link < link_id) return 0;
        ++links_seen;
        if (link_id == wanted_link) return (int32_t)(link_entry & 0x4000u);
        if (links_seen < link_count) link_ptr += 2; else return 0;
    }
}

/* MonkeyPuff14 @80039200 / MonkeyPuff5 @8003929C: SpawnParticle(type, &monkey position) */
/* Spawn a particle at the monkey position. */
static int32_t puff(AeRam *r, const AeMonkeyHooks *h, uint32_t sp, int32_t type) {
    uint32_t m = cur_monkey(r);
    WR16(sp + 0, RD16(m + AE_MK_POS));
    WR16(sp + 2, RD16(m + AE_MK_POS + 4));
    WR16(sp + 4, RD16(m + AE_MK_POS + 8));
    return AE_MK_HOOK(r, h, SpawnParticle, type, sp, 0, 0);
}
/* Spawn particle 0x14 at the monkey position. */
int32_t ae_MonkeyPuff14(AeRam *r, const AeMonkeyHooks *h) { return puff(r, h, AE_STK(AE_STK_PUFF14), 20); }
/* Spawn the body-transition puff at the monkey position. */
int32_t ae_MonkeyPuff5(AeRam *r, const AeMonkeyHooks *h) { return puff(r, h, AE_STK(AE_STK_PUFF5), 5); }

/* StaticModelDraw @80039E50 */
/* Transform and draw a rigid model in the current draw buffer. */
int32_t ae_StaticModelDraw(AeRam *r, const AeMonkeyHooks *h, uint32_t obj) {
    uint32_t vertices = AE_G_MODEL_VERTICES;
    uint32_t bone_matrices = AE_G_MODEL_BONE_BUFFER + (RD16(RD32(obj) + AE_MK_MODEL_BONE_COUNT) << 5);
    AE_MK_HOOK(r, h, ModelTransformRigid, obj, bone_matrices, vertices, 0);
    uint32_t draw_index = RD32(AE_G_MODEL_DRAW_BASE + AE_MK_DRAW_INDEX);       /* 0x800EC350: not the 0x800F4350 game-state block */
    uint32_t draw_context = AE_G_MODEL_DRAW_BASE + AE_MK_DRAW_CONTEXTS + draw_index * 16504u;
    return AE_MK_HOOK(r, h, ModelDraw, draw_context, vertices, obj, 0);
}

/* MonkeyStub_ReturnZero @8003A334 */
/* Return the original unused stub result of zero. */
int32_t ae_MonkeyStub_ReturnZero(AeRam *r) {
    (void)r;
    return 0;
}

/* MonkeyInterpolateLinkVector @8003C568: moveDir = normalise(from + (to - from) * (frames - frame) / frames); frame--. */
/* Interpolate and normalize facing along the current link. */
void ae_MonkeyInterpolateLinkVector(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    int32_t frames = RDS16(monkey + AE_MK_LINK_FRAMES);
    int32_t component_delta = (int32_t)(RD32(monkey + AE_MK_LINK_TO) - RD32(monkey + AE_MK_LINK_FROM));
    int32_t elapsed = (int32_t)((uint32_t)frames - (uint32_t)RDS16(monkey + AE_MK_LINK_FRAME));
    int32_t offset_x = div_trap(r, mul_lo(component_delta, elapsed), frames);
    if (ae_ram_failed(r)) return;
    int32_t offset_y = RDS16(monkey + AE_MK_LINK_FRAMES);
    component_delta = (int32_t)(RD32(monkey + AE_MK_LINK_TO + 4) - RD32(monkey + AE_MK_LINK_FROM + 4));
    elapsed = (int32_t)((uint32_t)offset_y - (uint32_t)RDS16(monkey + AE_MK_LINK_FRAME));
    offset_y = div_trap(r, mul_lo(component_delta, elapsed), offset_y);
    if (ae_ram_failed(r)) return;
    int32_t offset_z = RDS16(monkey + AE_MK_LINK_FRAMES);
    component_delta = (int32_t)(RD32(monkey + AE_MK_LINK_TO + 8) - RD32(monkey + AE_MK_LINK_FROM + 8));
    elapsed = (int32_t)((uint32_t)offset_z - (uint32_t)RDS16(monkey + AE_MK_LINK_FRAME));
    offset_z = div_trap(r, mul_lo(component_delta, elapsed), offset_z);
    if (ae_ram_failed(r)) return;
    WR16(monkey + AE_MK_MOVE_DIR, RD16(monkey + AE_MK_LINK_FROM) + (uint32_t)offset_x);
    WR16(monkey + AE_MK_MOVE_DIR + 2, RD16(monkey + AE_MK_LINK_FROM + 4) + (uint32_t)offset_y);
    WR16(monkey + AE_MK_MOVE_DIR + 4, RD16(monkey + AE_MK_LINK_FROM + 8) + (uint32_t)offset_z);
    ae_sdk_VectorNormalSS(r, monkey + AE_MK_MOVE_DIR, monkey + AE_MK_MOVE_DIR);
    uint32_t updated_monkey = cur_monkey(r);
    WR16(updated_monkey + AE_MK_LINK_FRAME, RD16(updated_monkey + AE_MK_LINK_FRAME) - 1u);
}

/* MonkeyResetNavVisits @800407F0: clears this monkey's visit mark on every nav node, then the goal. */
/* Clear this monkey visit marks and goal selection. */
void ae_MonkeyResetNavVisits(AeRam *r) {
    if (RDS16(AE_G_NAV_NODE_COUNT) > 0) {
        uint32_t node_offset = 0;
        int32_t nodes_seen = 0;
        do {
            ++nodes_seen;
            uint32_t slot = RD8(cur_monkey(r) + AE_MK_SLOT);
            WR8(RD32(AE_G_NAV_NODES) + node_offset + slot + AE_NAV_NODE_VISIT_MARKS, 0);
            if (!(nodes_seen < RDS16(AE_G_NAV_NODE_COUNT))) break;
            node_offset += AE_NAV_NODE_SIZE;
        } while (1);
    }
    uint32_t monkey = cur_monkey(r);
    uint32_t flags = RD32(monkey + AE_MK_FLAGS);
    WR16(monkey + AE_MK_GOAL_AGE, 0);
    WR16(monkey + AE_MK_GOAL_NODE, 0xFFFFu);
    WR32(monkey + AE_MK_FLAGS, flags & 0xF7FFFFFFu);
}

/* NavGroundHeightBetween @80040BDC: Y at the monkey's position interpolated along the segment b -> a (X or Z, the longer one). */
/* Interpolate ground height along the dominant horizontal axis. */
int32_t ae_NavGroundHeightBetween(AeRam *r, uint32_t a, uint32_t b) {
    uint32_t end_x = RD32(a), start_x = RD32(b);
    uint32_t end_z = RD32(a + 8), start_z = RD32(b + 8);
    uint32_t end_y = RD32(a + 4), start_y = RD32(b + 4);
    int32_t link_dx = (int32_t)(end_x - start_x);
    int32_t link_dz = (int32_t)(end_z - start_z);
    if (link_dx == 0 && link_dz == 0) return (int32_t)RD32(cur_monkey(r) + AE_MK_POS + 4);
    uint32_t monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_AI_STATE) == 9) return (int32_t)RD32(cur_monkey(r) + AE_MK_POS + 4);
    int32_t abs_dx = link_dx < 0 ? (int32_t)(0u - (uint32_t)link_dx) : link_dx;
    int32_t abs_dz = link_dz < 0 ? (int32_t)(0u - (uint32_t)link_dz) : link_dz;
    int32_t dy = (int32_t)(end_y - start_y);
    if (!(abs_dx < abs_dz)) {
        int32_t dx = (int32_t)(RD32(monkey + AE_MK_POS) - start_x);
        int32_t q = div_trap(r, mul_lo(dy, dx), link_dx);
        return (int32_t)(start_y + (uint32_t)q);
    }
    int32_t dz = (int32_t)(RD32(monkey + AE_MK_POS + 8) - start_z);
    int32_t q = div_trap(r, mul_lo(dy, dz), link_dz);
    return (int32_t)(start_y + (uint32_t)q);
}

/* ActorContactOwnerType @80041280: the object that touched the monkey (contact word & 0xFFF) -> its +0x24 & 0xF0 */
/* Read the kind flags of the object recorded in the contact word. */
int32_t ae_ActorContactOwnerType(AeRam *r) {
    uint32_t idx = RD32(cur_monkey(r) + AE_MK_CONTACT) & 0xFFFu;
    uint32_t o = ae_GetObjectByIndex(r, idx);
    return (int32_t)(RD16(o + AE_MK_CONTACT_OWNER_KIND) & 0xF0u);
}

/* MonkeyDebugMenuInit @80043060 */
/* Bind the monkey count and active-count debug items. */
void ae_MonkeyDebugMenuInit(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t menu = AE_G_MONKEY_DEBUG_MENU;
    uint32_t count_item = AE_G_MONKEY_COUNT_ITEM;
    WR32(menu + AE_MK_MENU_COUNT_LINK, count_item);
    WR32(menu + AE_MK_MENU_INFO_LINK, AE_G_MONKEY_INFO_ITEMS);
    AE_MK_HOOK(r, h, MenuItemInitBound, count_item, AE_G_MONKEY_COUNT_F44D4, 1, 0);
    AE_MK_HOOK(r, h, MenuItemSetRange, count_item, 0, 8, 0);
    menu = count_item + AE_MK_MENU_ITEM_SIZE;
    AE_MK_HOOK(r, h, MenuItemInitBound, menu, AE_G_ACTIVE_MONKEY_MAX, 1, 0);
    AE_MK_HOOK(r, h, MenuItemSetRange, menu, 0, 4, 0);
}

/* MonkeyDebugMenuInit2 @800430F0: menu items for one monkey's info record (call voice, 8 ranges, name ...). */
/* Bind one monkey information record to the debug menu. */
void ae_MonkeyDebugMenuInit2(AeRam *r, const AeMonkeyHooks *h, int32_t slot_index) {
    uint32_t item = AE_G_MONKEY_INFO_ITEMS;
    uint32_t menu_monkey = AE_G_MONKEYS + (uint32_t)slot_index * AE_MONKEY_SIZE;
    AE_MK_HOOK(r, h, MenuItemInitBound, item, RD32(AE_G_MENU_VALUE_4D44), 0, 0);
    AE_MK_HOOK(r, h, MenuItemSetRange, item, 0, 7, 0);
    AE_MK_HOOK(r, h, MenuItemSetCallback, item, AE_G_MONKEY_MENU_CALLBACK, 0, 0);
    item += 2u * AE_MK_MENU_ITEM_SIZE;
    uint32_t range_offset = AE_MK_INFO_RANGES;
    int32_t range_count = 0;
    do {
        uint32_t entry = RD32(AE_G_MONKEY_INFO_TABLE) + RD8(menu_monkey + AE_MK_ID) * AE_MONKEY_INFO_SIZE;
        range_count += 1;
        AE_MK_HOOK(r, h, MenuItemInitBound, item, entry + range_offset, 1, 0);
        AE_MK_HOOK(r, h, MenuItemSetStep, item, 10, 0, 0);
        item += AE_MK_MENU_ITEM_SIZE;
        range_offset += 2;
    } while (range_count < 8);
    static const struct { uint32_t off, flag; } items[8] = {
        {AE_MK_INFO_WALK_SPEED, 1}, {AE_MK_INFO_ALERT_SPEED, 1},
        {AE_MK_INFO_ATTACK_COUNT, 0}, {AE_MK_INFO_ATTACK_COUNT + 8u, 0},
        {AE_MK_INFO_ATTACK_COUNT + 16u, 0}, {AE_MK_INFO_ATTACK_COUNT + 32u, 0},
        {AE_MK_INFO_ATTACK_COUNT + 24u, 0}, {AE_MK_INFO_ATTACK_COUNT_7, 0}};
    item += AE_MK_MENU_ITEM_SIZE;
    for (int i = 0; i < 8; ++i) {
        uint32_t entry = RD32(AE_G_MONKEY_INFO_TABLE) + RD8(menu_monkey + AE_MK_ID) * AE_MONKEY_INFO_SIZE;
        AE_MK_HOOK(r, h, MenuItemInitBound, item, entry + items[i].off, items[i].flag, 0);
        if (i < 7) item += AE_MK_MENU_ITEM_SIZE;
    }
}
