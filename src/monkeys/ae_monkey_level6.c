/*
 * Monkey level6 routines, hand-ported and verified against the original game.
 * Field layouts and integration hooks are declared in ae_monkey.h.
 */
#include "ae_monkey.h"

#define RD8(a) ae_rd8(r, (a))
#define RD16(a) ae_rd16(r, (a))
#define RD32(a) ae_rd32(r, (a))
#define RDS8(a) ae_rds8(r, (a))
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
/* Read the current monkey address. */
static uint32_t cur_monkey(AeRam *r) { return RD32(AE_G_CUR_MONKEY); }

/* MonkeyRand() % 100 as the compiler computes it: x = rand & 0xFFFF, q = x * 0x51EB851F >> 37, (x - q * 100) & 0xFFFF */
/* Choose a random percentage. */
static uint32_t rand_mod100(AeRam *r) {
    uint32_t x = ae_MonkeyRand(r) & 0xFFFFu;
    uint32_t q = mulu_hi(x, 0x51EB851Fu) >> 5;
    return (x - q * 100u) & 0xFFFFu;
}

/* the node count byte (+0xB) of the monkey's current node */
/* Read the active link count of the current node. */
static uint32_t link_count(AeRam *r, uint32_t m) {
    return RD8(RD16(m + AE_MK_NAV_NODE0) * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + AE_NAV_ACTIVE_LINK_COUNT);
}

/* ================================================================================================
 * MonkeyAlertedDecide
 * ============================================================================================== */

/* MonkeyAlertedDecide @80037550: the per-frame decision of an alerted monkey in a walking state (flags bit 20 set: nothing).
   Boss arenas: state 11 (or its animation restart) unless byte 0x800F447C == 3.  Otherwise alerted = 1; the target is 300..1500
   away; state 11 within that range keeps the animation, flags bit 26 within range enters state 11; states that allow it (state
   table flags bit 3) with +0x222 > 0 restart the animation; else NodeActionDispatch; if nothing was entered: a finished animation in
   a ground state stops; attack pick A (flags bit 23, state 3); then state 8 / 3 by chance (30 %); finally the "previous node" reset
   and an alert puff for monkeys that were not in state 0x0C / 0x24 before. */
/* Choose an alerted walking state or attack and apply transition effects. */
void ae_MonkeyAlertedDecide(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t st0 = RD8(cur_monkey(r) + AE_MK_AI_STATE);
    uint32_t was_fast_ground_state = (st0 == 12 || st0 == 36) ? 1u : 0u;
    uint32_t monkey = cur_monkey(r);
    if ((RD32(monkey + AE_MK_FLAGS) >> 20) & 1u) return;
    if (RD8(monkey + AE_MK_ALERTED) == 0) WR8(monkey + AE_MK_LOST_SIGHT_COUNT, 0);
    int32_t boss = ae_AreaIsBossArena(r);
    if (boss != 0) {
        if (RD8(AE_G_MONKEY_BOSS_MODE) != 3) {
            if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != 11) ae_MonkeySetState(r, 11);
            else ae_MonkeyPickAnim(r);
            return;
        }
    }
    monkey = cur_monkey(r);
    WR8(monkey + AE_MK_ALERTED, 1);
    uint32_t dist = RD32(cur_monkey(r) + AE_MK_DIST_TO_TARGET);
    uint32_t target_in_range = (dist - 300u) < 1201u;
    int32_t boss_again = ae_AreaIsBossArena(r);
    int reset_previous = 1;
    if (boss_again == 0 && RD8(cur_monkey(r) + AE_MK_AI_STATE) == 11 && (target_in_range & 0xFFu) != 0) {
        ae_MonkeyPickAnim(r);
    } else if (((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 26) & 1u) && (target_in_range & 0xFFu) != 0) {
        ae_MonkeySetState(r, 11);
    } else {
        uint32_t current_monkey = cur_monkey(r);
        uint32_t state_flags = RD32(AE_G_MONKEY_STATE_TABLE + RD8(current_monkey + AE_MK_AI_STATE) * 8u + 4u);
        if (((state_flags >> 3) & 1u) && RDS8(current_monkey + AE_MK_PATROL_COUNTER) > 0) {
            ae_MonkeyPickAnim(r);
        } else if (ae_MonkeyNodeActionDispatch(r, h) != 0) {
            /* The node action has already handled the previous-node field. */
            reset_previous = 0;
        } else {
            if (ae_MonkeyAnimFinished(r)) {
                uint32_t state = RD8(cur_monkey(r) + AE_MK_AI_STATE);
                if (state == 3 || state == 8) return;
            }
            current_monkey = cur_monkey(r);
            if ((RD32(current_monkey + AE_MK_FLAGS) >> 23) & 1u) {
                if (RD8(current_monkey + AE_MK_AI_STATE) == 3) {
                    int32_t attack_kind = ae_MonkeyPickAttackA(r, h);
                    if (attack_kind != 100) return;
                }
            }
            current_monkey = cur_monkey(r);
            uint32_t state = RD8(current_monkey + AE_MK_AI_STATE);
            uint32_t next_state;
            if (state == 8 || state == 35 || state == 57 || RDS16(current_monkey + AE_MK_ACTION_COOLDOWN) > 0) {
                next_state = 3;
            } else {
                uint32_t rem = rand_mod100(r);
                next_state = (rem < 30u) ? 8u : 3u;
            }
            ae_MonkeySetState(r, next_state);
        }
    }
    if (reset_previous) {
        if (ae_ram_failed(r)) return;
        if (!ae_MonkeyIsJumpArcState(r)) {
            uint32_t current_monkey = cur_monkey(r);
            uint32_t state = RD8(current_monkey + AE_MK_AI_STATE);
            if (state != 9 && state != 0x1A && state != 0x35 &&
                !(RD32(current_monkey + AE_MK_FLAGS2) & 0x200u) && !ae_AreaIsBossArena(r)) {
                WR16(cur_monkey(r) + AE_MK_PREV_NODE, 0xFFFFu);
            }
        }
    }
    if (ae_ram_failed(r)) return;
    if (was_fast_ground_state != 0) {
        if (!ae_MonkeyIsState0C_or_24(r)) ae_MonkeyAlertPuff(r, h);
    }
}

/* ================================================================================================
 * MonkeyChooseNextNodeA / B
 * ============================================================================================== */

/* MonkeyChooseNextNodeA @80038A1C: goal position; unless p1 != 0 or the node was reached: the turn back decision (its result is
   returned); otherwise the best link (p2 as the PickBestLink limit) becomes the next node: returns 1, or 0 when there is none */
/* Advance through the best link or consider turning back before arrival. */
int32_t ae_MonkeyChooseNextNodeA(AeRam *r, const AeMonkeyHooks *h, int32_t p1, int32_t p2) {
    ae_MonkeyUpdateGoalPos(r, h);
    if (ae_ram_failed(r)) return 0;
    if ((((uint32_t)p1 << 16) == 0) && ae_MonkeyReachedNode(r) == 0) {
        return ae_MonkeyConsiderTurnBack(r, h);
    }
    int32_t link = s16((uint32_t)ae_MonkeyPickBestLink(r, h, s16((uint32_t)p2)));
    if (ae_ram_failed(r)) return 0;
    if (link == -1) return 0;
    ae_MonkeySetNavNode(r, h, link);
    return 1;
}

/* MonkeyChooseNextNodeB @80038A9C: unalerted: the first link that is not the previous node and whose blocked byte is not 1 becomes the
   next node (1), none: goal update (0).  Alerted: the link with the smallest score (+0x176 table filled by ScoreLinksByTarget)
   below 8192, scores below rcos(682) (rcos(angle * 4096 / 360) in special areas) are taken at once in 5 % of the cases; fallback to
   the first admissible link; the turn-back-random variant when p1 == 0 and the node was not reached.  Returns 1 or 0. */
/* Choose the first eligible or best target-scored link. */
int32_t ae_MonkeyChooseNextNodeB(AeRam *r, const AeMonkeyHooks *h, int32_t p1, int32_t p2) {
    int32_t cosine_limit = ae_sdk_rcos(r, 682);
    ae_MonkeyMarkBlockedLinks(r, h, RDS16(cur_monkey(r) + AE_MK_NAV_NODE0));
    if (ae_ram_failed(r)) return 0;
    if (RD8(cur_monkey(r) + AE_MK_ALERTED) == 0) {
        uint32_t cnt = link_count(r, cur_monkey(r));
        if (!(0 < (int32_t)cnt)) {
            ae_MonkeyUpdateGoal(r, h);
            return 0;
        }
        int32_t link_index = 0;
        for (;;) {
            uint32_t m = cur_monkey(r);
            int32_t link = ae_NavNodeGetLink(r, RDS16(m + AE_MK_NAV_NODE0), s16((uint32_t)link_index));
            if (ae_ram_failed(r)) return 0;
            uint32_t monkey = cur_monkey(r);
            int32_t candidate_node = s16((uint32_t)link);
            int32_t hist = RDS16(monkey + AE_MK_PREV_NODE);
            int32_t next_link_index = link_index + 1;
            if (candidate_node != hist) {
                if (RD8(monkey + (uint32_t)s16((uint32_t)link_index) + AE_MK_LINK_BLOCKED) != 1) {
                    ae_MonkeySetNavNode(r, h, candidate_node);
                    return 1;
                }
            }
            link_index = next_link_index;
            uint32_t c2 = link_count(r, monkey);
            if (!(s16((uint32_t)link_index) < (int32_t)c2)) break;
        }
        ae_MonkeyUpdateGoal(r, h);
        return 0;
    }
    if (ae_AreaIsSpecialMonkeyArea(r)) {
        int32_t ang = ae_MonkeyApproachAngleNear(r);
        int32_t x = (int32_t)((uint32_t)ang << 12);
        int32_t approach_angle = sra32(mul_hi(x, (int32_t)0xB60B60B7u) + x, 8);
        approach_angle = approach_angle - sra32(x, 31);
        cosine_limit = ae_sdk_rcos(r, approach_angle);
    }
    ae_MonkeyUpdateGoalPos(r, h);
    if (ae_ram_failed(r)) return 0;
    if (((uint32_t)p1 << 16) == 0 && ae_MonkeyReachedNode(r) == 0) {
        ae_MonkeyConsiderTurnBackRandom(r, h);
        return 1;
    }
    ae_MonkeyScoreLinksByTarget(r, h, RDS16(cur_monkey(r) + AE_MK_NAV_NODE0));
    if (ae_ram_failed(r)) return 0;
    int32_t best_score = 0x2000;
    int32_t best_node = -1;
    uint32_t cnt = link_count(r, cur_monkey(r));
    int32_t link_index = 0;
    if (cnt != 0) {
        for (;;) {
            uint32_t m = cur_monkey(r);
            int32_t link_index_narrow = s16((uint32_t)link_index);
            int32_t linked_node = ae_NavNodeGetLink(r, RDS16(m + AE_MK_NAV_NODE0), link_index_narrow);
            if (ae_ram_failed(r)) return 0;
            uint32_t candidate_monkey = cur_monkey(r);
            int32_t hist = RDS16(candidate_monkey + AE_MK_PREV_NODE);
            int32_t next_link_index = link_index + 1;
            int take_now = 0;
            if (s16((uint32_t)linked_node) != hist && RD8(candidate_monkey + (uint32_t)link_index_narrow + AE_MK_LINK_BLOCKED) != 1) {
                uint32_t sa = candidate_monkey + ((uint32_t)link_index_narrow << 1);
                int32_t score = RDS16(sa + AE_MK_LINK_SCORES);
                uint32_t score_bits = RD16(sa + AE_MK_LINK_SCORES);
                if (score < cosine_limit) {
                    if (rand_mod100(r) < 5u) {
                        best_score = (int32_t)score_bits;
                        best_node = linked_node;
                        take_now = 1;
                    }
                }
                if (take_now) break;
                if (score < s16((uint32_t)best_score)) {
                    best_score = (int32_t)score_bits;
                    best_node = linked_node;
                }
            }
            link_index = next_link_index;
            uint32_t c2 = link_count(r, cur_monkey(r));
            if (!(s16((uint32_t)link_index) < (int32_t)c2)) break;
        }
    }
    if (((uint32_t)p2 << 16) == 0) {
        if (!(s16((uint32_t)best_score) < cosine_limit)) {
            ae_MonkeyUpdateGoal(r, h);
            return 0;
        }
    }
    int32_t sel = s16((uint32_t)best_node);
    if (sel >= 0) {
        ae_MonkeySetNavNode(r, h, sel);
        return 1;
    }
    uint32_t c3 = link_count(r, cur_monkey(r));
    if (c3 == 0) return 0;
    link_index = 0;
    for (;;) {
        uint32_t m = cur_monkey(r);
        int32_t link_index_narrow = s16((uint32_t)link_index);
        int32_t link = ae_NavNodeGetLink(r, RDS16(m + AE_MK_NAV_NODE0), link_index_narrow);
        if (ae_ram_failed(r)) return 0;
        uint32_t monkey = cur_monkey(r);
        int32_t candidate_node = s16((uint32_t)link);
        int32_t hist = RDS16(monkey + AE_MK_PREV_NODE);
        int32_t next_link_index = link_index + 1;
        if (candidate_node != hist) {
            if (RD8(monkey + (uint32_t)link_index_narrow + AE_MK_LINK_BLOCKED) != 1) {
                ae_MonkeySetNavNode(r, h, candidate_node);
                return 1;
            }
        }
        link_index = next_link_index;
        uint32_t c2 = link_count(r, monkey);
        if (!(s16((uint32_t)link_index) < (int32_t)c2)) break;
    }
    ae_MonkeyUpdateGoal(r, h);
    return 0;
}
