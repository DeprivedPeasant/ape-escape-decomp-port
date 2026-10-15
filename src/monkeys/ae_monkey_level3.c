/*
 * Monkey level3 routines, hand-ported and verified against the original game.
 * Field layouts and integration hooks are declared in ae_monkey.h.
 */
#include "ae_monkey.h"

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
/* Return the signed high half of a 64-bit product. */
static int32_t mul_hi(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * (int64_t)b) >> 32); }
/* Return the unsigned high half of a 64-bit product. */
static uint32_t mulu_hi(uint32_t a, uint32_t b) { return (uint32_t)(((uint64_t)a * (uint64_t)b) >> 32); }
/* Subtract with 32-bit wraparound. */
static int32_t sub32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
/* Read the current monkey address. */
static uint32_t cur_monkey(AeRam *r) { return RD32(AE_G_CUR_MONKEY); }
/* Locate the information record for a monkey. */
static uint32_t info_of(AeRam *r, uint32_t m) {          /* MonkeyInfo of the monkey: table + id * 0x88 */
    return RD32(AE_G_MONKEY_INFO_TABLE) + RD8(m + AE_MK_ID) * AE_MONKEY_INFO_SIZE;
}
/* s16 / 100 as the compiler does it: (v * 0x51EB851F) >> 37 - (v >> 31), then narrowed to s16 */
/* Divide by 100 and narrow the quotient to a signed halfword. */
static int32_t div100_s16(int32_t v) {
    return s16((uint32_t)sub32(sra32(mul_hi(v, 0x51EB851F), 5), sra32(v, 31)));
}

/* g_Player (0x800EC1E0) +0xEA, read as s16: the attack pickers do nothing while it is non-zero */
#define AE_G_PLAYER_BLOCK 0x800EC2CAu

/* ================================================================================================
 * Activities / goal
 * ============================================================================================== */

/* MonkeyRestoreActivities @800332F8: restores the saved activity of all 8 monkeys; monkeys at a node with bit 10 (an object
   node) and activity 2 / 5 tell the object (slot = node field / 100) via ObjSlotSetField_CF; activity 5 additionally
   resets the activity, fetches the object's target position and enters state 0x2B. */
/* Restore saved activities and notify associated object slots. */
void ae_MonkeyRestoreActivities(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t slot_monkey = AE_G_MONKEYS;
    for (int32_t slot_index = 0; slot_index < 8; ++slot_index) {
        WR32(AE_G_CUR_MONKEY, slot_monkey);
        uint32_t saved_activity = RD8(AE_G_MONKEY_ACTIVITY + (uint32_t)slot_index);
        WR8(slot_monkey + AE_MK_ACTIVITY, saved_activity == 4 ? 0u : saved_activity);
        uint32_t monkey = cur_monkey(r);
        uint32_t node_addr = RD16(monkey + AE_MK_NAV_NODE1) * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES);
        if (RD32(node_addr) & 0x400u) {
            uint32_t act = RD8(monkey + AE_MK_ACTIVITY);
            if (act == 2) {
                AE_MK_HOOK(r, h, ObjSlotSetField_CF, div100_s16(RDS16(node_addr + AE_NAV_LINK_CODE)), 4, 0, 0);
            } else if (act == 5) {
                AE_MK_HOOK(r, h, ObjSlotSetField_CF, div100_s16(RDS16(node_addr + AE_NAV_LINK_CODE)), 4, 0, 0);
                WR8(cur_monkey(r) + AE_MK_ACTIVITY, 0);
                monkey = cur_monkey(r);
                uint32_t n = RD16(monkey + AE_MK_NAV_NODE1) * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES);
                AE_MK_HOOK(r, h, ObjSlotGetTarget, div100_s16(RDS16(n + AE_NAV_LINK_CODE)), monkey + AE_MK_POS, 0, 0);
                ae_MonkeySetState(r, 0x2B);
            }
        }
        slot_monkey += AE_MONKEY_SIZE;
    }
}

/* MonkeyUpdateGoal @800355DC */
/* Refresh link slope before resolving the goal position. */
void ae_MonkeyUpdateGoal(AeRam *r, const AeMonkeyHooks *h) {
    ae_MonkeyUpdateLinkSlope(r, h);
    if (ae_ram_failed(r)) return;
    ae_MonkeyUpdateGoalPos(r, h);
}

/* MonkeyStartNotice @800357E0 */
/* Queue the notice voice and enter its animation with dust. */
void ae_MonkeyStartNotice(AeRam *r, const AeMonkeyHooks *h) {
    int32_t voice = ae_MonkeyGetCallVoice(r);
    ae_MonkeyQueueVoice(r, h, voice, 0);
    ae_MonkeySetState(r, 7);
    ae_MonkeyFootDust(r, h);
}

/* ================================================================================================
 * State entries
 * ============================================================================================== */

/* MonkeyEnterState_1E @8003581C: always returns 1 */
/* Enter flying movement and initialize its circle and sound loop. */
int32_t ae_MonkeyEnterState_1E(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_ENTER_1E);
    uint32_t previous_state = RD8(cur_monkey(r) + AE_MK_AI_STATE);
    ae_MonkeySetState(r, 30);
    uint32_t monkey = cur_monkey(r);
    int32_t next_node = RDS16(monkey + AE_MK_NAV_NODE1);
    int32_t current_node = RDS16(monkey + AE_MK_NAV_NODE0);
    WR16(monkey + AE_MK_HEALTH, 1000);
    uint32_t flags = RD32(monkey + AE_MK_FLAGS);
    WR32(monkey + AE_MK_STATE_TIMER, 0);
    flags |= 0x80000000u;
    WR32(monkey + AE_MK_FLAGS, flags);
    WR16(monkey + AE_MK_BODY_FLAGS, 0xC000u);
    WR32(monkey + AE_MK_FLAGS, flags & 0xFEFFFFFFu);
    ae_NavLinkLookupExtra(r, next_node, current_node, sp + 32);
    if (ae_ram_failed(r)) return 0;
    ae_NavNodeGetPos(r, h, RD16(cur_monkey(r) + AE_MK_NAV_NODE0), sp + 16);
    if (ae_ram_failed(r)) return 0;
    uint32_t flight_monkey = cur_monkey(r);
    WR32(flight_monkey + AE_MK_CIRCLE_CENTER, RD32(sp + 16));
    WR32(flight_monkey + AE_MK_CIRCLE_CENTER + 4, RD32(sp + 20));
    uint32_t extra = RD16(sp + 32);
    uint32_t z = RD32(sp + 24);
    WR16(flight_monkey + AE_MK_CIRCLE_RADIUS, extra);
    WR32(flight_monkey + AE_MK_CIRCLE_CENTER + 8, z);
    ae_MonkeyFootDust(r, h);
    if (previous_state != 30 && previous_state != 32) {
        int32_t handle = AE_MK_HOOK(r, h, SoundLoopStartEx, 1, 0x6050, RD8(cur_monkey(r) + AE_MK_ID), 0);
        WR32(cur_monkey(r) + AE_MK_FLIGHT_SFX_HANDLE, handle);
    }
    return 1;
}

/* MonkeyEnterState_34 @800359B4 */
/* Advance the node-selected animation phase. */
void ae_MonkeyEnterState_34(AeRam *r) {
    ae_MonkeySetState(r, 0x34);
    uint32_t monkey = cur_monkey(r);
    WR16(monkey + AE_MK_ANIM_STATE_FRAME, RD16(monkey + AE_MK_ANIM_STATE_FRAME) + 1u);
}

/* the "stunned" sound + stars shared by the knockdown entries: once per knockdown (flags bit 21) */
/* Play the stun sound and mark the knockdown effect active. */
static void knock_sfx(AeRam *r, const AeMonkeyHooks *h, uint32_t sp, uint32_t mk) {
    uint32_t monkey = cur_monkey(r);
    WR16(sp + 16, RD16(monkey + AE_MK_POS));
    WR16(sp + 18, RD16(monkey + AE_MK_POS + 4));
    WR16(sp + 20, RD16(monkey + AE_MK_POS + 8));
    AE_MK_HOOK(r, h, PlaySfxAt, 78, sp + 16, 0, 0);
    WR16(mk + AE_MK_TIMER_158, 50);
    WR32(mk + AE_MK_FLAGS, RD32(mk + AE_MK_FLAGS) | 0x200000u);
}

/* MonkeyEnterKnockdown @80035BD0 (state 0x2F / 0x30 / ...: arg = the state) */
/* Enter the requested knockdown state and initialize stun effects. */
void ae_MonkeyEnterKnockdown(AeRam *r, const AeMonkeyHooks *h, int32_t state) {
    uint32_t sp = AE_STK(AE_STK_KNOCKDOWN);
    int32_t knockdown_state = s16((uint32_t)state);
    int32_t jump = ae_MonkeyIsJumpArcState(r);
    if (!jump) {
        uint32_t monkey = cur_monkey(r);
        uint32_t target = RD32(AE_G_MONKEY_TARGET);
        WR32(monkey + AE_MK_GOAL_POS, RD32(target + AE_MK_POS));
        WR32(monkey + AE_MK_GOAL_POS + 4, RD32(target + AE_MK_POS + 4));
        WR32(monkey + AE_MK_GOAL_POS + 8, RD32(target + AE_MK_POS + 8));
    }
    ae_MonkeySetState(r, (uint32_t)knockdown_state);
    uint32_t reloaded_monkey = cur_monkey(r);
    WR8(reloaded_monkey + AE_MK_STUN_TIMER, knockdown_state == 48 ? 90u : 30u);
    reloaded_monkey = cur_monkey(r);
    WR16(reloaded_monkey + AE_MK_TIMER_124, 120);
    WR32(reloaded_monkey + AE_MK_CONTACT, 0);
    ae_MonkeyQueueVoice(r, h, 71, 3);
    reloaded_monkey = cur_monkey(r);
    if (!((RD32(reloaded_monkey + AE_MK_FLAGS) >> 21) & 1u)) {
        knock_sfx(r, h, sp, cur_monkey(r));
    }
    reloaded_monkey = cur_monkey(r);
    WR16(sp + 24, RD16(reloaded_monkey + AE_MK_POS));
    WR16(sp + 26, RD16(reloaded_monkey + AE_MK_POS + 4) - 128u);
    WR16(sp + 28, RD16(reloaded_monkey + AE_MK_POS + 8));
    AE_MK_HOOK(r, h, SpawnPuff, sp + 24, 0, 0, 0);
}

/* MonkeyEnterState2F @80035D18 */
/* Enter the falling knockdown state and initialize its height and timers. */
void ae_MonkeyEnterState2F(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_ENTER_2F);
    uint32_t saved_monkey = cur_monkey(r);
    ae_MonkeySetState(r, 0x2F);
    WR8(saved_monkey + AE_MK_STUN_TIMER, 30);
    uint32_t y = RD16(saved_monkey + AE_MK_POS + 4);
    WR16(saved_monkey + AE_MK_THROW_TIMER, 0);
    WR16(saved_monkey + AE_MK_TIMER_124, 120);
    WR16(saved_monkey + AE_MK_HOP_START_Y, y + 128u);
    ae_MonkeyQueueVoice(r, h, 71, 3);
    if (!((RD32(saved_monkey + AE_MK_FLAGS) >> 21) & 1u)) {
        uint32_t monkey = cur_monkey(r);
        WR16(sp + 16, RD16(monkey + AE_MK_POS));
        WR16(sp + 18, RD16(monkey + AE_MK_POS + 4));
        WR16(sp + 20, RD16(monkey + AE_MK_POS + 8));
        AE_MK_HOOK(r, h, PlaySfxAt, 78, sp + 16, 0, 0);
        WR16(saved_monkey + AE_MK_TIMER_158, 50);
        WR32(saved_monkey + AE_MK_FLAGS, RD32(saved_monkey + AE_MK_FLAGS) | 0x200000u);
    }
}

/* MonkeyEnterState_30 @80035DD8 */
/* Enter the landing knockdown state and activate its stun effect. */
void ae_MonkeyEnterState_30(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_ENTER_30);
    uint32_t saved_monkey = cur_monkey(r);
    ae_MonkeySetState(r, 0x30);
    if (!((RD32(saved_monkey + AE_MK_FLAGS) >> 21) & 1u)) {
        uint32_t monkey = cur_monkey(r);
        WR16(sp + 16, RD16(monkey + AE_MK_POS));
        WR16(sp + 18, RD16(monkey + AE_MK_POS + 4));
        WR16(sp + 20, RD16(monkey + AE_MK_POS + 8));
        AE_MK_HOOK(r, h, PlaySfxAt, 78, sp + 16, 0, 0);
        WR16(saved_monkey + AE_MK_TIMER_158, 50);
        WR32(saved_monkey + AE_MK_FLAGS, RD32(saved_monkey + AE_MK_FLAGS) | 0x200000u);
    }
}

/* MonkeyEnterState_36 @80035E6C */
/* Enter the recovery animation with its voice. */
void ae_MonkeyEnterState_36(AeRam *r, const AeMonkeyHooks *h) {
    ae_MonkeySetState(r, 0x36);
    WR32(cur_monkey(r) + AE_MK_CONTACT, 0);
    ae_MonkeyQueueVoice(r, h, 71, 3);
}

/* MonkeyEnterState_20 @80035EA4 */
/* Enter flying movement with two particles and delayed voices. */
void ae_MonkeyEnterState_20(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_ENTER_20);
    uint32_t monkey = cur_monkey(r);
    WR16(sp + 16, RD16(monkey + AE_MK_POS));
    WR16(sp + 18, RD16(monkey + AE_MK_POS + 4));
    WR16(sp + 20, RD16(monkey + AE_MK_POS + 8));
    ae_MonkeySetState(r, 0x20);
    AE_MK_HOOK(r, h, SpawnParticle, 5, sp + 16, 0, 0);
    AE_MK_HOOK(r, h, SpawnParticle, 5, sp + 16, 0, 0);
    ae_MonkeyQueueVoice(r, h, 120, 0);
    ae_MonkeyQueueVoice(r, h, 120, 2);
}

/* MonkeyEnterState_24 @80035F1C / MonkeyEnterState_0C @80035F54: an alert puff unless already in state 0x0C / 0x24 */
/* Enter fast ground movement and emit an alert transition if needed. */
void ae_MonkeyEnterState_24(AeRam *r, const AeMonkeyHooks *h) {
    if (!ae_MonkeyIsState0C_or_24(r)) ae_MonkeyAlertPuff(r, h);
    ae_MonkeySetState(r, 0x24);
}
/* Enter alternate fast ground movement and emit an alert transition if needed. */
void ae_MonkeyEnterState_0C(AeRam *r, const AeMonkeyHooks *h) {
    if (!ae_MonkeyIsState0C_or_24(r)) ae_MonkeyAlertPuff(r, h);
    ae_MonkeySetState(r, 0x0C);
}

/* MonkeyEnterState_35 @80035F8C */
/* Enter alternate traversal and preserve the previous node. */
void ae_MonkeyEnterState_35(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);
    WR16(monkey + AE_MK_PREV_NODE, next_node);
    ae_MonkeySetState(r, 0x35);
}

/* MonkeyEnterState_1A @80035FBC */
/* Enter special traversal, saving the jump origin in boss arenas. */
void ae_MonkeyEnterState_1A(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);
    WR16(monkey + AE_MK_PREV_NODE, next_node);
    if (ae_AreaIsBossArena(r)) {
        uint32_t m = cur_monkey(r);
        uint32_t x = RD32(m + AE_MK_POS), y = RD32(m + AE_MK_POS + 4), z = RD32(m + AE_MK_POS + 8);
        WR32(m + AE_MK_JUMP_START, x);
        WR32(m + AE_MK_JUMP_START + 4, y);
        WR32(m + AE_MK_JUMP_START + 8, z);
    }
    ae_MonkeySetState(r, 0x1A);
}

/* MonkeyEnterState_09 @8003601C */
/* Enter slope traversal and set its link-selected facing. */
void ae_MonkeyEnterState_09(AeRam *r) {
    uint32_t sp = AE_STK(AE_STK_ENTER_09);
    uint32_t monkey = cur_monkey(r);
    int32_t next_node = RDS16(monkey + AE_MK_NAV_NODE1);
    uint32_t next_node_bits = RD16(monkey + AE_MK_NAV_NODE1);
    int32_t current_node = RDS16(monkey + AE_MK_NAV_NODE0);
    WR16(monkey + AE_MK_ACTION_COOLDOWN, 0);
    WR16(monkey + AE_MK_PREV_NODE, next_node_bits);
    ae_NavLinkLookupExtra(r, next_node, current_node, sp + 16);
    if (ae_ram_failed(r)) return;
    uint32_t m = cur_monkey(r);
    uint32_t st = RD8(m + AE_MK_AI_STATE);
    int cond = 0;
    if (st == 26) {
        if (ae_AreaIsBossArena(r) == 0) cond = 1;
    }
    if (!cond) {
        m = cur_monkey(r);
        if (RD8(m + AE_MK_AI_STATE) == 53) cond = 1;
    }
    if (cond) {
        uint32_t a = cur_monkey(r);
        if (RD8(a + AE_MK_LINK_SLOPE) == 2) WR32(a + AE_MK_POS + 4, RD32(a + AE_MK_POS + 4) + 200u);
    }
    ae_MonkeySetState(r, 9);
    ae_MonkeyFacingFromLinkCode(r, RDS16(sp + 16));
}

/* MonkeyEnterIdleWait @80036300 */
/* Enter idle wait and choose its randomized timer. */
void ae_MonkeyEnterIdleWait(AeRam *r) {
    ae_MonkeySetState(r, 1);
    uint32_t rnd = ae_MonkeyRand(r) & 0xFFFFu;
    uint32_t monkey = cur_monkey(r);
    int32_t idle_duration = RDS16(info_of(r, monkey) + AE_MK_INFO_IDLE_DURATION);
    int32_t random_duration = (int32_t)(rnd << 1);
    if (idle_duration == 0 || (idle_duration == -1 && random_duration == INT32_MIN)) {
        ae_ram_fail(r, "divide trap (div by zero / INT_MIN / -1; the original executes break)");
        return;
    }
    int32_t rem = random_duration % idle_duration;
    WR32(monkey + AE_MK_FLAGS, RD32(monkey + AE_MK_FLAGS) & 0xFFF7FFFFu);
    WR16(monkey + AE_MK_IDLE_TIMER, (uint32_t)(idle_duration << 1) - (uint32_t)rem);
}

/* MonkeyResumeIdle @800363A4 */
/* Resume idle with the continuation flag set. */
void ae_MonkeyResumeIdle(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    ae_MonkeySetState(r, 1);
    WR16(monkey + AE_MK_IDLE_TIMER, 0);
    WR32(monkey + AE_MK_FLAGS, RD32(monkey + AE_MK_FLAGS) | 0x80000u);
}

/* MonkeyEnterRandomLook @800363E4 */
/* Choose one of the two look-around states. */
void ae_MonkeyEnterRandomLook(AeRam *r) {
    uint32_t v = ae_MonkeyRand(r);
    ae_MonkeySetState(r, (v & 1u) ? 0x26u : 0x16u);
}

/* ================================================================================================
 * Attack pickers
 * ============================================================================================== */

/* the common loop of MonkeyPickAttackA / B / C: up to 12 tries of a random kind 0..7 whose flag `bit` (word at
   0x800AEBB8 + 8 * kind) is set; the first that MonkeyTryAttackKind starts is returned, 100 when none / the player flag is set */
/* Try up to twelve random attacks permitted by the requested flag. */
static int32_t pick_attack(AeRam *r, const AeMonkeyHooks *h, uint32_t bit) {
    if (RDS16(AE_G_PLAYER_BLOCK) != 0) return 100;
    for (int32_t attempt = 0; attempt < 12; ++attempt) {
        uint32_t kind = ae_MonkeyRand(r) & 7u;
        if ((RD32(AE_G_MONKEY_ATTACK_FLAGS + kind * 8u) >> bit) & 1u) {
            int32_t ok = ae_MonkeyTryAttackKind(r, h, (int32_t)kind);
            if (ae_ram_failed(r)) return 0;
            if (ok != 0) return (int32_t)kind;
        }
    }
    return 100;
}
/* Choose an attack allowed by category A. */
int32_t ae_MonkeyPickAttackA(AeRam *r, const AeMonkeyHooks *h) { return pick_attack(r, h, 0); }   /* 8003738C */
/* Choose an attack allowed by category B. */
int32_t ae_MonkeyPickAttackB(AeRam *r, const AeMonkeyHooks *h) { return pick_attack(r, h, 1); }   /* 80037420 */
/* Choose an attack allowed by category C. */
int32_t ae_MonkeyPickAttackC(AeRam *r, const AeMonkeyHooks *h) { return pick_attack(r, h, 2); }   /* 800374B8 */

/* ================================================================================================
 * Navigation
 * ============================================================================================== */

/* MonkeyMarkBlockedLinks @80038054: byte table at +0x1DA (50 entries) = 1 for each link of the node that the monkey may not use */
/* Clear and rebuild the blocked-link marker table. */
void ae_MonkeyMarkBlockedLinks(AeRam *r, const AeMonkeyHooks *h, int32_t node) {
    uint32_t monkey = cur_monkey(r);
    for (int32_t link_index = 49; link_index >= 0; --link_index) WR8(monkey + (uint32_t)link_index + AE_MK_LINK_BLOCKED, 0);
    int32_t node_index = s16((uint32_t)node);
    uint32_t link_count = RD8((uint32_t)node_index * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + AE_NAV_ACTIVE_LINK_COUNT);
    if (link_count == 0) return;
    int32_t link_index = 0;
    do {
        int32_t link = ae_NavNodeGetLink(r, node_index, s16((uint32_t)link_index));
        if (ae_ram_failed(r)) return;
        int32_t ok = ae_MonkeyNavLinkAllowed(r, h, node_index, link);
        if (ae_ram_failed(r)) return;
        if (ok == 0) WR8(monkey + (uint32_t)link_index + AE_MK_LINK_BLOCKED, 1);
        link_index += 1;
    } while (link_index < (int32_t)link_count);
}

/* MonkeyUpdateTargetDistance @80039B30: goal position (unless idle), distance (+0x20C) and angle (+0x21C) to the target */
/* Refresh goal data, target distance and relative target angle. */
void ae_MonkeyUpdateTargetDistance(AeRam *r, const AeMonkeyHooks *h) {
    if (!ae_MonkeyIsIdleActivityState(r)) {
        ae_MonkeyUpdateGoalPos(r, h);
        if (ae_ram_failed(r)) return;
    }
    uint32_t monkey = cur_monkey(r);
    uint32_t target = RD32(AE_G_MONKEY_TARGET);
    int32_t d = ae_Distance3D(r, monkey + AE_MK_POS, target + AE_MK_POS);
    uint32_t reloaded_monkey = cur_monkey(r);
    WR32(reloaded_monkey + AE_MK_DIST_TO_TARGET, d);
    int32_t ang = ae_MonkeyAngleToTarget(r);
    reloaded_monkey = cur_monkey(r);
    WR32(reloaded_monkey + AE_MK_LINK_ANGLE, ang);
}

/* MonkeyAi_State3E @8003FC08 */
/* Advance decelerating movement until entering its terminal state. */
void ae_MonkeyAi_State3E(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    uint32_t elapsed = RD16(monkey + AE_MK_COUNTER96) + 1u;
    uint32_t duration = RD16(monkey + AE_MK_ANIM_LENGTH);
    WR16(monkey + AE_MK_COUNTER96, elapsed);
    if (s16(elapsed) < (int32_t)duration) {
        if (RD32(monkey + AE_MK_FLAGS2) & 2u) {
            WR32(monkey + AE_MK_POS + 4, RD32(monkey + AE_MK_POS + 4) + (uint32_t)RDS16(monkey + AE_MK_VEL + 2));
        }
        monkey = cur_monkey(r);
        int32_t velocity_x = RDS16(monkey + AE_MK_VEL);
        uint32_t x = RD32(monkey + AE_MK_POS);
        int32_t velocity_z = RDS16(monkey + AE_MK_VEL + 4);
        uint32_t z = RD32(monkey + AE_MK_POS + 8);
        WR32(monkey + AE_MK_POS, x + (uint32_t)velocity_x);
        WR32(monkey + AE_MK_POS + 8, z + (uint32_t)velocity_z);
    } else {
        ae_MonkeyEnterState_3F(r);
    }
}

/* MonkeyNavVisitUpdate @8004086C: the per-monkey visit counters (byte at node + slot + 0x14).  Flags bit 27 = "nothing left to
   mark": while both nodes of the current link are unvisited it waits; every other call (goalAge odd) ages the counters
   (+1, max 255), on even calls the nodes next to a visited node (counter >= 2) get marked 1 when the link may be used. */
/* Age visit marks or propagate them through permitted neighboring links. */
void ae_MonkeyNavVisitUpdate(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    uint32_t flags = RD32(monkey + AE_MK_FLAGS);
    uint32_t force_propagation = 0;
    if ((flags >> 27) & 1u) {
        uint32_t nodes = RD32(AE_G_NAV_NODES);
        uint32_t node_index = RD16(monkey + AE_MK_NAV_NODE0);
        uint32_t slot = RD8(monkey + AE_MK_SLOT);
        if (RD8(node_index * AE_NAV_NODE_SIZE + nodes + slot + AE_NAV_NODE_VISIT_MARKS) != 0) return;
        uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);
        uint32_t next_visit_cell = next_node * AE_NAV_NODE_SIZE + nodes + slot;
        if (RD8(next_visit_cell + AE_NAV_NODE_VISIT_MARKS) != 0) return;
        WR32(monkey + AE_MK_FLAGS, flags & 0xF7FFFFFFu);
        force_propagation = 1;
    }
    uint32_t current_monkey = cur_monkey(r);
    WR16(current_monkey + AE_MK_GOAL_AGE, RD16(current_monkey + AE_MK_GOAL_AGE) + 1u);
    uint32_t current_flags = RD32(current_monkey + AE_MK_FLAGS);
    uint32_t odd_age = RD16(current_monkey + AE_MK_GOAL_AGE) & 1u;
    WR32(current_monkey + AE_MK_FLAGS, current_flags & 0xF7FFFFFFu);
    if (odd_age == 1u && force_propagation == 0) {
        /* age every visited counter */
        if (RDS16(AE_G_NAV_NODE_COUNT) <= 0) return;
        int32_t node_index = 0;
        uint32_t node_offset = 0;
        do {
            uint32_t m = cur_monkey(r);
            uint32_t p = node_offset + RD32(AE_G_NAV_NODES) + RD8(m + AE_MK_SLOT);
            if (RD8(p + AE_NAV_NODE_VISIT_MARKS) != 0) {
                uint32_t old_mark = RD8(p + AE_NAV_NODE_VISIT_MARKS);
                int32_t next_mark_signed = (int32_t)(old_mark + 1u);
                if (!(next_mark_signed < 256)) next_mark_signed = 255;
                uint32_t out;
                if (next_mark_signed <= 0) {
                    out = 1;
                } else {
                    uint32_t next_mark_bits = old_mark + 1u;
                    out = next_mark_bits < 256u ? next_mark_bits : 255u;
                }
                WR8(p + AE_NAV_NODE_VISIT_MARKS, out);
            }
            node_index += 1;
            node_offset += AE_NAV_NODE_SIZE;
        } while (node_index < RDS16(AE_G_NAV_NODE_COUNT));
        return;
    }
    /* mark the unvisited nodes that touch a visited one */
    int32_t marks_added = 0;
    current_monkey = cur_monkey(r);
    current_flags = RD32(current_monkey + AE_MK_FLAGS);
    int32_t cnt = RDS16(AE_G_NAV_NODE_COUNT);
    WR32(current_monkey + AE_MK_FLAGS, current_flags & 0xF7FFFFFFu);
    if (cnt > 0) {
        uint32_t node_offset = 0;
        int32_t node_index = 0;
        do {
            uint32_t m = cur_monkey(r);
            uint32_t nodes = RD32(AE_G_NAV_NODES);
            uint32_t slot = RD8(m + AE_MK_SLOT);
            uint32_t node = node_offset + nodes;
            if (RD8(node + slot + AE_NAV_NODE_VISIT_MARKS) == 0) {
                if (RD8(node + AE_NAV_LINK_COUNT) != 0) {
                    int32_t link_index = 0;
                    do {
                        uint32_t nodes_base = RD32(AE_G_NAV_NODES);
                        uint32_t list = RD32(node_offset + nodes_base + AE_NAV_LINKS);
                        uint32_t entry = RD16((uint32_t)link_index * 2u + list);
                        uint32_t m2 = cur_monkey(r);
                        uint32_t linked_node = entry & 0x3FFFu;
                        uint32_t slot2 = RD8(m2 + AE_MK_SLOT);
                        uint32_t vis = RD8(linked_node * AE_NAV_NODE_SIZE + nodes_base + slot2 + AE_NAV_NODE_VISIT_MARKS);
                        if (!(vis < 2u) && (entry & 0x4000u)) {
                            int32_t ok = ae_MonkeyNavLinkAllowed(r, h, node_index, (int32_t)linked_node);
                            if (ae_ram_failed(r)) return;
                            if (ok != 0) {
                                marks_added += 1;
                                uint32_t m3 = cur_monkey(r);
                                uint32_t n3 = RD32(AE_G_NAV_NODES);
                                WR8(node_offset + n3 + RD8(m3 + AE_MK_SLOT) + AE_NAV_NODE_VISIT_MARKS, 1);
                            }
                        }
                        link_index += 1;
                    } while (link_index < (int32_t)RD8(node_offset + RD32(AE_G_NAV_NODES) + AE_NAV_LINK_COUNT));
                }
            }
            node_index += 1;
            node_offset += AE_NAV_NODE_SIZE;
        } while (node_index < RDS16(AE_G_NAV_NODE_COUNT));
    }
    uint32_t cm = cur_monkey(r);
    WR16(cm + AE_MK_NAV_MARK_COUNT, marks_added);
    if (marks_added != 0) return;
    int32_t cnt2 = RDS16(AE_G_NAV_NODE_COUNT);
    WR32(cm + AE_MK_FLAGS, RD32(cm + AE_MK_FLAGS) | 0x08000000u);
    if (cnt2 <= 0) return;
    /* the maximum of the counters is computed by the original but never used; its reads are kept (they can fault) */
    uint32_t slot = RD8(cm + AE_MK_SLOT);
    uint32_t nodes = RD32(AE_G_NAV_NODES);
    for (int32_t i = 0; i < cnt2; ++i) (void)RD8(nodes + (uint32_t)i * AE_NAV_NODE_SIZE + slot + AE_NAV_NODE_VISIT_MARKS);
}

/* ================================================================================================
 * Drawing
 * ============================================================================================== */

/* MonkeyDrawModel @80039ED4: draws the monkey (activity 4, or 3 on every third frame of its counter +0x90): bone matrices,
   model, the alert icon, the held / carried static models of the attacks and the area specific extras (area 0x46 / 0x24). */
/* Draw the animated monkey, alert icon and eligible held or area models. */
void ae_MonkeyDrawModel(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_DRAW_MODEL);
    uint32_t draw_monkey = cur_monkey(r);
    uint32_t act = RD8(draw_monkey + AE_MK_ACTIVITY);
    if (act != 4) {
        if (act != 3) return;
        uint32_t timer = RD16(draw_monkey + AE_MK_TIMER_90);
        uint32_t q = mulu_hi(timer, 0xAAAAAAABu) >> 1;
        uint32_t timer_remainder = (timer - (q * 2u + q)) & 0xFFFFu;
        if (timer_remainder != 0) return;
    }
    if (ae_MonkeyIsState0C_or_24(r)) {
        draw_monkey = cur_monkey(r);
        AE_MK_HOOK(r, h, MatrixLookDir, draw_monkey + AE_MK_MOVE_DIR, RD32(draw_monkey + AE_MK_BONE_MATRICES), 0, 0);
    } else {
        draw_monkey = cur_monkey(r);
        AE_MK_HOOK(r, h, MatrixFromDirXZ, draw_monkey + AE_MK_MOVE_DIR, RD32(draw_monkey + AE_MK_BONE_MATRICES), 0, 0);
    }
    uint32_t monkey = cur_monkey(r);
    int32_t f0 = RDS16(monkey + AE_MK_NOTICE_COUNT);
    if (!(f0 > 0)) {
        uint32_t t = RD16(monkey + AE_MK_FLAGS_HI) & 3u;
        if (t == 2) {
            int32_t v = ae_sdk_rcos(r, (int32_t)((uint32_t)f0 << 10));
            WR32(sp + 24, v);
            WR32(sp + 20, v);
            WR32(sp + 16, v);
        } else if (t == 1) {
            int32_t v = ae_sdk_rsin(r, (int32_t)((uint32_t)f0 << 10));
            WR32(sp + 24, v);
            WR32(sp + 20, v);
            WR32(sp + 16, v);
        }
        uint32_t scale_monkey = cur_monkey(r);
        if (((RD16(scale_monkey + AE_MK_FLAGS_HI) & 3u) - 1u) < 2u) {
            AE_MK_HOOK(r, h, ScaleMatrix, RD32(scale_monkey + AE_MK_BONE_MATRICES), sp + 16, 0, 0);
        }
    }
    draw_monkey = cur_monkey(r);
    AE_MK_HOOK(r, h, ModelBuildBoneMatrices, draw_monkey, 3, 0, 0);
    draw_monkey = cur_monkey(r);
    AE_MK_HOOK(r, h, ModelTransformSkinned, draw_monkey, 0, 0, 0);
    const uint32_t SCREEN_VERTS = AE_G_MODEL_VERTICES;
    if (ae_AreaIsBossArena(r)) {
        uint32_t idx = RD32(AE_G_MODEL_DRAW_BASE + AE_MK_DRAW_INDEX);
        uint32_t ctx = idx * 16504u + (AE_G_MODEL_DRAW_BASE + AE_MK_DRAW_CONTEXTS);
        AE_MK_HOOK(r, h, ModelDraw, ctx, SCREEN_VERTS, cur_monkey(r), 0);
    } else {
        uint32_t draw_monkey_after_icon = cur_monkey(r);
        uint32_t ctx, ot_offset;
        if (RD8(draw_monkey_after_icon + AE_MK_MODEL_LOD) == 3) {
            uint32_t idx = RD32(AE_G_MODEL_DRAW_BASE + AE_MK_DRAW_INDEX);
            ot_offset = 0;
            ctx = idx * 16504u + (AE_G_MODEL_DRAW_BASE + AE_MK_DRAW_CONTEXTS);
        } else {
            ae_MonkeyDrawAlertIcon(r, h);
            if (ae_ram_failed(r)) return;
            uint32_t idx = RD32(AE_G_MODEL_DRAW_BASE + AE_MK_DRAW_INDEX);
            draw_monkey_after_icon = cur_monkey(r);
            uint32_t id = RD8(draw_monkey_after_icon + AE_MK_ID);
            uint32_t info = id * AE_MONKEY_INFO_SIZE + RD32(AE_G_MONKEY_INFO_TABLE);
            ot_offset = RD32(draw_monkey_after_icon + AE_MK_ALERT_ICON_KIND) + RD16(info);
            ctx = AE_G_MODEL_DRAW_BASE + idx * 16504u + AE_MK_DRAW_CONTEXTS_WITH_OT;
        }
        AE_MK_HOOK(r, h, ModelDrawWithOt, ctx, SCREEN_VERTS, draw_monkey_after_icon, ot_offset);
    }
    uint32_t area = RD16(AE_G_CURRENT_AREA);
    if (area == 0x46) {
        ae_StaticModelDraw(r, h, AE_G_MONKEY_BOSS_MODEL);
        if (ae_ram_failed(r)) return;
        area = RD16(AE_G_CURRENT_AREA);
    }
    if (area == 0x24) {
        uint32_t sound_monkey = cur_monkey(r);
        uint32_t extra_model;
        if (RD8(sound_monkey + AE_MK_AI_STATE) == 0x1A) {
            extra_model = AE_G_MONKEY_EXTRA_MODEL_A;
            if (RD32(sound_monkey + AE_MK_SFX_HANDLE) == 0xFFFFFFFFu) {
                int32_t handle = AE_MK_HOOK(r, h, SoundLoopStartEx, 1, 0x602D, RD8(sound_monkey + AE_MK_ID), 0);
                WR32(cur_monkey(r) + AE_MK_SFX_HANDLE, handle);
            }
        } else {
            extra_model = AE_G_MONKEY_EXTRA_MODEL_B;
            if (RD32(sound_monkey + AE_MK_SFX_HANDLE) != 0xFFFFFFFFu) {
                AE_MK_HOOK(r, h, SoundLoopStopAndFree, 0x602D, RD32(sound_monkey + AE_MK_SFX_HANDLE), 0, 0);
                WR32(cur_monkey(r) + AE_MK_SFX_HANDLE, 0xFFFFFFFFu);
            }
        }
        ae_StaticModelDraw(r, h, extra_model);
        if (ae_ram_failed(r)) return;
    }
    uint32_t state = RD8(cur_monkey(r) + AE_MK_AI_STATE);
    if (state != 30 && state != 32) {
        int32_t held_model = 0;
        do {
            uint32_t m = cur_monkey(r);
            uint32_t k8 = (uint32_t)s16((uint32_t)held_model) * 8u;
            uint32_t rec = RD8(m + AE_MK_ID) * AE_MONKEY_INFO_SIZE + RD32(AE_G_MONKEY_INFO_TABLE) + k8;
            if (RD16(rec + AE_MK_INFO_ATTACK_COUNT) != 0) {
                int32_t model = RDS16(AE_G_MONKEY_ATTACK_TABLE + k8 + 2u);
                if (model != -1) {
                    ae_StaticModelDraw(r, h, (uint32_t)model * 16u + AE_G_MONKEY_HELD_MODELS);
                    if (ae_ram_failed(r)) return;
                }
            }
            held_model += 1;
        } while (s16((uint32_t)held_model) < 8);
    }
    if (ae_AreaIsBossArena(r)) return;
    uint32_t sound_monkey = cur_monkey(r);
    if (RD8(sound_monkey + AE_MK_MODEL_LOD) == 3) return;
    uint32_t monkey_id = RD8(sound_monkey + AE_MK_ID);
    uint32_t v = RD16(monkey_id * AE_MONKEY_INFO_SIZE + RD32(AE_G_MONKEY_INFO_TABLE)) >> 6;
    uint32_t idx = RD8(AE_G_MONKEY_GROUP_MODELS + v);
    ae_StaticModelDraw(r, h, idx * 16u + AE_G_MONKEY_HELD_MODELS);
}
