/*
 * Monkey level7 routines, hand-ported and verified against the original game.
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
/* Return the unsigned high half of a 64-bit product. */
static uint32_t mulu_hi(uint32_t a, uint32_t b) { return (uint32_t)(((uint64_t)a * (uint64_t)b) >> 32); }
/* Read the current monkey address. */
static uint32_t cur_monkey(AeRam *r) { return RD32(AE_G_CUR_MONKEY); }
/* Locate the information record for a monkey. */
static uint32_t info_of(AeRam *r, uint32_t m) {          /* MonkeyInfo of the monkey: table + id * 0x88 */
    return RD32(AE_G_MONKEY_INFO_TABLE) + RD8(m + AE_MK_ID) * AE_MONKEY_INFO_SIZE;
}

/* MonkeyRand() % 100 as the compiler computes it: x = rand & 0xFFFF, q = x * 0x51EB851F >> 37, (x - q * 100) & 0xFFFF */
/* Choose a random percentage. */
static uint32_t rand_mod100(AeRam *r) {
    uint32_t x = ae_MonkeyRand(r) & 0xFFFFu;
    uint32_t q = mulu_hi(x, 0x51EB851Fu) >> 5;
    return (x - q * 100u) & 0xFFFFu;
}

/* the tail shared by MonkeyAlertedDecide and MonkeyPatrolDecide after a state was set (not for jump arc states, 9 / 0x1A / 0x35,
   flags2 bit 9, boss arenas): the "previous node" +0xEC is reset to -1 */
/* Clear previous-node history when the current state permits it. */
static void reset_prev_node(AeRam *r) {
    if (ae_MonkeyIsJumpArcState(r)) return;
    uint32_t c = cur_monkey(r);
    uint32_t st = RD8(c + AE_MK_AI_STATE);
    if (st == 9 || st == 0x1A) return;
    if (st == 0x35 || (RD32(c + AE_MK_FLAGS2) & 0x200u)) return;
    if (ae_AreaIsBossArena(r)) return;
    WR16(cur_monkey(r) + AE_MK_PREV_NODE, 0xFFFFu);
}

/* ================================================================================================
 * MonkeyPatrolDecide
 * ============================================================================================== */

/* MonkeyPatrolDecide @80037908: the per-frame decision of an unalerted monkey (flags bit 20 set: nothing; boss arenas: the alerted
   decision).  alerted = 0; states with state table flags bit 3 and +0x222 > 0 restart the animation; else NodeActionDispatch; else
   the MonkeyInfo record (+2 / +4 non-zero: a "wanderer", else a ground idler) decides between idle wait, new states 0 / 2 / 24 / 25 /
   39 / 40 and MonkeyEnterState_17.  Afterwards the "previous node" reset and an alert puff for monkeys that were not in state 0x0C /
   0x24 on entry. */
/* Choose idle or wandering behavior for an unalerted monkey. */
void ae_MonkeyPatrolDecide(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t c0 = cur_monkey(r);
    uint32_t alerted0 = RD8(c0 + AE_MK_ALERTED);
    uint32_t st0 = RD8(c0 + AE_MK_AI_STATE);
    uint32_t was_alerted = (alerted0 ^ 1u) < 1u;                    /* was alerted */
    uint32_t was_fast_ground_state = (st0 == 12 || st0 == 36) ? 1u : 0u;      /* was in state 0x0C / 0x24 */
    if ((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 20) & 1u) return;
    if (ae_AreaIsBossArena(r)) {
        ae_MonkeyAlertedDecide(r, h);
        return;
    }
    WR8(cur_monkey(r) + AE_MK_ALERTED, 0);
    uint32_t monkey = cur_monkey(r);
    uint32_t flagw = RD32(AE_G_MONKEY_STATE_TABLE + RD8(monkey + AE_MK_AI_STATE) * 8u + 4u);
    if (((flagw >> 3) & 1u) && RDS8(monkey + AE_MK_PATROL_COUNTER) > 0) {
        ae_MonkeyPickAnim(r);
    } else if (ae_MonkeyNodeActionDispatch(r, h) == 0) {
        monkey = cur_monkey(r);
        uint32_t info = info_of(r, monkey);
        if (RDS16(info + AE_MK_INFO_WALK_SPEED) == 0 && RDS16(info + AE_MK_INFO_ALERT_SPEED) == 0) {
            int32_t idle_distance = RDS16(info + AE_MK_INFO_IDLE_DISTANCE);
            int32_t dist = RDS32(monkey + AE_MK_DIST_TO_TARGET);
            int wait = !(idle_distance < dist);
            if (!wait) {
                uint32_t rem = rand_mod100(r);
                uint32_t info2 = info_of(r, cur_monkey(r));
                int32_t lim = RDS16(info2 + AE_MK_INFO_IDLE_CHANCE);
                wait = (int32_t)rem < lim;
            }
            if (wait) ae_MonkeyEnterIdleWait(r);
            else ae_MonkeySetState(r, 0);
        } else {
            uint32_t reloaded_monkey = cur_monkey(r);
            if (RDS16(reloaded_monkey + AE_MK_ACTION_COOLDOWN) > 0) {
                ae_MonkeySetState(r, 2);
            } else {
                uint32_t st = RD8(reloaded_monkey + AE_MK_AI_STATE);
                if (st == 25) {
                    ae_MonkeySetState(r, 24);
                } else {
                    int wait = 0;
                    if (st == 2 || ae_MonkeyIsIdleEndState(r) != 0) {
                        uint32_t rem = rand_mod100(r);
                        uint32_t info2 = info_of(r, cur_monkey(r));
                        int32_t lim = RDS16(info2 + AE_MK_INFO_IDLE_CHANCE);
                        wait = (int32_t)rem < lim;
                    }
                    if (wait) {
                        ae_MonkeyEnterIdleWait(r);
                    } else {
                        st = RD8(cur_monkey(r) + AE_MK_AI_STATE);
                        int start_activity = st == 2 && rand_mod100(r) < 10u;
                        if (start_activity) {
                            uint32_t rnd = ae_MonkeyRand(r);
                            uint32_t next_state = (rnd & 1u) ? 40u : 39u;
                            ae_MonkeySetState(r, next_state);
                        } else if (was_alerted != 0) {
                            uint32_t distance_monkey = cur_monkey(r);
                            int32_t lim = RDS16(info_of(r, distance_monkey) + AE_MK_INFO_ALERT_DISTANCE);
                            int32_t dist = RDS32(distance_monkey + AE_MK_DIST_TO_TARGET);
                            uint32_t next_state = (lim < dist) ? 24u : 25u;
                            ae_MonkeySetState(r, next_state);
                        } else {
                            uint32_t rem = rand_mod100(r);
                            if (rem < 9u) ae_MonkeyEnterState_17(r, h);
                            else ae_MonkeySetState(r, 2);
                        }
                    }
                }
            }
        }
    }
    if (ae_ram_failed(r)) return;
    reset_prev_node(r);
    if (was_fast_ground_state != 0) {
        if (!ae_MonkeyIsState0C_or_24(r)) ae_MonkeyAlertPuff(r, h);
    }
}

/* ================================================================================================
 * MonkeySelectNextNode
 * ============================================================================================== */

/* MonkeySelectNextNode @8003B074(p1, p2): picks the next nav node when the monkey has reached one.  Saves the node history
   (+0xF2, six u16), PickGoalNode (flags bit 27 and the goal node reached), blocked links, link scores.  Unalerted: ChooseNextNodeA(p1, 1)
   else B(p1, 1).  Alerted: A(p1, p2) (p2 = 1 in boss arenas) else, outside boss arenas and not in state 61, B(p1, 1);
   when a node was chosen: unless the next node has flag 0x10000 or p2 == 1, a backtracking monkey (NavBacktracked) goes back to a
   random node of the next node's links (best-scored one first); with none the history is restored and the monkey looks around
   (EnterRandomLook, returns 0).  Boss arena / failure paths return 0 (state 61: the first link).  At the end: ArriveAtNode for the new
   next node when the current node changed, LoadLinkExtra when the result is 1, a debug printf when p2 != 0 and nothing was loaded.
   Returns 1 when a node was selected. */
/* Advance navigation and recover from repeated or failed link choices. */
int32_t ae_MonkeySelectNextNode(AeRam *r, const AeMonkeyHooks *h, int32_t p1, int32_t p2) {
    uint32_t sp = AE_STK(AE_STK_SELECT_NEXT);
    uint32_t force_advance_bits = (uint32_t)p1, link_limit_bits = (uint32_t)p2;
    uint32_t selected = 1;
    uint32_t cm = cur_monkey(r);
    uint32_t original_node = RD16(cm + AE_MK_NAV_NODE0);
    for (uint32_t link_index = 0; link_index < 6; ++link_index) WR16(sp + 16 + 2 * link_index, RD16(cm + AE_MK_NAV_NODE0 + 2 * link_index));
    uint32_t monkey = cur_monkey(r);
    if ((RD32(monkey + AE_MK_FLAGS) >> 27) & 1u) {
        uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE0);
        int32_t goal_node = RDS16(monkey + AE_MK_GOAL_NODE);
        if (next_node == (uint32_t)goal_node) {
            ae_AreaIsSpecialMonkeyArea(r);
            ae_MonkeyPickGoalNode(r, h);
        }
    }
    ae_MonkeyMarkBlockedLinks(r, h, RDS16(cur_monkey(r) + AE_MK_NAV_NODE0));
    if (ae_ram_failed(r)) return 0;
    ae_MonkeyScoreLinksByTarget(r, h, RDS16(cur_monkey(r) + AE_MK_NAV_NODE0));
    if (ae_ram_failed(r)) return 0;
    int check_backtracking = 0;
    int use_first_link = 0;
    int selection_failed = 0;
    int32_t link_limit = 0;
    if (RD8(cur_monkey(r) + AE_MK_ALERTED) == 0) {
        int32_t link_index = s16(force_advance_bits);
        if (ae_MonkeyChooseNextNodeA(r, h, link_index, 1) == 0) ae_MonkeyChooseNextNodeB(r, h, link_index, 1);

    } else {
        if (ae_AreaIsBossArena(r)) link_limit_bits = 1;
        int32_t link_index = s16(force_advance_bits);
        link_limit = s16(link_limit_bits);
        if (ae_MonkeyChooseNextNodeA(r, h, link_index, link_limit) != 0) {
            check_backtracking = 1;
        } else if (ae_AreaIsBossArena(r)) {
            selection_failed = 1;
        } else if (RD8(cur_monkey(r) + AE_MK_AI_STATE) == 61) {
            use_first_link = 1;
        } else if (ae_MonkeyChooseNextNodeB(r, h, link_index, 1) == 0) {
            selection_failed = 1;
        } else {
            check_backtracking = 1;
        }
    }
    if (ae_ram_failed(r)) return 0;
    if (check_backtracking) {
        uint32_t next_node = RD16(cur_monkey(r) + AE_MK_NAV_NODE1);

        if (!(RD32(next_node * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES)) & 0x10000u) && link_limit != 1 && ae_MonkeyNavBacktracked(r) != 0) {
            ae_MonkeyScoreLinksByTarget(r, h, RDS16(cur_monkey(r) + AE_MK_NAV_NODE1));
            if (ae_ram_failed(r)) return 0;
            int32_t best_score = 0x40000000;
            ae_MonkeyMarkBlockedLinks(r, h, RDS16(cur_monkey(r) + AE_MK_NAV_NODE1));
            if (ae_ram_failed(r)) return 0;
            int32_t best_index = -1;
            uint32_t m = cur_monkey(r);
            uint32_t cnt = RD8(RD16(m + AE_MK_NAV_NODE1) * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + AE_NAV_ACTIVE_LINK_COUNT);
            int32_t candidate_count = 0;
            if (cnt != 0) {
                int32_t link_index = 0;
                for (;;) {
                    uint32_t m1 = cur_monkey(r);
                    int32_t link_index_narrow = s16((uint32_t)link_index);
                    int32_t link = ae_NavNodeGetLink(r, RDS16(m1 + AE_MK_NAV_NODE1), link_index_narrow);
                    if (ae_ram_failed(r)) return 0;
                    uint32_t candidate_monkey = cur_monkey(r);
                    uint32_t node0 = RD16(candidate_monkey + AE_MK_NAV_NODE0);
                    if ((uint32_t)link != node0) {
                        int32_t link2 = ae_NavNodeGetLink(r, RDS16(candidate_monkey + AE_MK_NAV_NODE1), link_index_narrow);
                        if (ae_ram_failed(r)) return 0;
                        candidate_monkey = cur_monkey(r);
                        int32_t hist = RDS16(candidate_monkey + AE_MK_PREV_NODE);
                        if (link2 != hist) {
                            if (RD8(candidate_monkey + (uint32_t)link_index + AE_MK_LINK_BLOCKED) == 0) {
                                int32_t score = RDS16(candidate_monkey + ((uint32_t)link_index << 1) + AE_MK_LINK_SCORES);
                                if (score < best_score) {
                                    best_score = score;
                                    best_index = link_index;
                                } else if (candidate_count < 10) {
                                    WR32(sp + 32 + (uint32_t)candidate_count * 4u, (uint32_t)link_index);
                                    candidate_count += 1;
                                }
                            }
                        }
                    }
                    uint32_t m2 = cur_monkey(r);
                    uint32_t cnt2 = RD8(RD16(m2 + AE_MK_NAV_NODE1) * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + AE_NAV_ACTIVE_LINK_COUNT);
                    link_index += 1;
                    if (!(link_index < (int32_t)cnt2)) break;
                }
            }
            int32_t chosen_index = 0;
            int pick = 0;
            if (best_index != -1) {
                chosen_index = s16((uint32_t)best_index);
                pick = 1;
            } else if (candidate_count > 0) {
                uint32_t x = ae_MonkeyRand(r) & 0xFFFFu;
                uint32_t rem = x % (uint32_t)candidate_count;
                chosen_index = RDS16(sp + 32 + rem * 4u);
                pick = 1;
            } else {
                for (uint32_t k = 0; k < 6; ++k) WR16(cur_monkey(r) + AE_MK_NAV_NODE0 + 2 * k, RD16(sp + 16 + 2 * k));
                ae_MonkeyEnterRandomLook(r);
                selected = 0;
            }
            if (pick) {
                uint32_t m3 = cur_monkey(r);
                int32_t v = ae_NavNodeGetLink(r, RDS16(m3 + AE_MK_NAV_NODE1), chosen_index);
                if (ae_ram_failed(r)) return 0;
                WR16(cur_monkey(r) + AE_MK_NAV_NODE0, v);
                ae_MonkeyUpdateGoal(r, h);
            }
        }
    }
    if (selection_failed) {
        selected = 0;
        use_first_link = RD8(cur_monkey(r) + AE_MK_AI_STATE) == 61;
    }
    if (use_first_link) {
        selected = 1;
        int32_t v = ae_NavNodeGetLink(r, RDS16(cur_monkey(r) + AE_MK_NAV_NODE0), 0);
        if (ae_ram_failed(r)) return 0;
        ae_MonkeySetNavNode(r, h, s16((uint32_t)v));
    }
    if (ae_ram_failed(r)) return 0;
    uint32_t arrival_monkey = cur_monkey(r);
    int32_t original_node_signed = s16(original_node);
    uint32_t new_current_node = RD16(arrival_monkey + AE_MK_NAV_NODE0);
    uint32_t selected_byte = selected & 0xFFu;
    if ((uint32_t)original_node_signed != new_current_node) {
        ae_MonkeyArriveAtNode(r, h, RDS16(arrival_monkey + AE_MK_NAV_NODE1));
    }
    if (selected_byte != 0) {
        uint32_t m = cur_monkey(r);
        ae_MonkeyLoadLinkExtra(r, RDS16(m + AE_MK_NAV_NODE1), RDS16(m + AE_MK_NAV_NODE0));
    }
    if ((link_limit_bits << 16) != 0 && selected_byte == 0) {
        AE_MK_HOOK(r, h, Printf, AE_G_MONKEY_SELECT_TEXT, original_node_signed, 0, 0);
    }
    return (int32_t)selected;
}

/* ================================================================================================
 * Idle activity / flagged node
 * ============================================================================================== */

/* the idle-activity state transition table of MonkeyAi_IdleActivityStart: 13 -> 14, 16 -> 17, 19 -> 20, 58 -> 59, others stay */

/* MonkeyAi_IdleActivityStart @8003F18C: a finished animation: every third frame (frame counter + monkey id divisible by 3), if
   UpdateSight(1) sees the target: monkeys in special areas or whose state table flags bit 6 is clear get a call voice + the alerted
   decision, the others become alerted (=1) and notice the target (MonkeyStartNotice).  Animation not finished: a new idle
   timer (+0x98 = 10 + rand % 20) and the idle states advance (13 -> 14, 16 -> 17, 19 -> 20, 58 -> 59). */
/* Look for the target or initialize the looping idle activity. */
void ae_MonkeyAi_IdleActivityStart(AeRam *r, const AeMonkeyHooks *h) {
    if (ae_MonkeyAnimFinished(r)) {
        uint32_t m = cur_monkey(r);
        uint32_t staggered_frame = RD32(AE_G_FRAME_COUNTER) + RD8(m + AE_MK_ID);
        uint32_t q = mulu_hi(staggered_frame, 0xAAAAAAABu) >> 1;
        if (staggered_frame != q * 2u + q) return;
        if (ae_MonkeyUpdateSight(r, h, 1) == 0) return;
        int notice = 0;
        if (ae_AreaIsSpecialMonkeyArea(r) == 0) {
            uint32_t c = cur_monkey(r);
            uint32_t flagw = RD32(AE_G_MONKEY_STATE_TABLE + RD8(c + AE_MK_AI_STATE) * 8u + 4u);
            if ((flagw >> 6) & 1u) notice = 1;
        }
        if (notice) {
            WR8(cur_monkey(r) + AE_MK_ALERTED, 1);            /* Mark alerted before starting the notice animation. */
            ae_MonkeyStartNotice(r, h);
        } else {
            int32_t voice = ae_MonkeyGetCallVoice(r);
            ae_MonkeyQueueVoice(r, h, voice, 0);
            ae_MonkeyAlertedDecide(r, h);
        }
        return;
    }
    uint32_t x = ae_MonkeyRand(r) & 0xFFFFu;
    uint32_t q = mulu_hi(x, 0xCCCCCCCDu) >> 4;
    uint32_t rem = x - q * 20u;
    uint32_t activity_monkey = cur_monkey(r);
    uint32_t st = RD8(activity_monkey + AE_MK_AI_STATE);
    WR16(activity_monkey + AE_MK_IDLE_ACTIVITY_TIMER, rem + 10u);
    uint32_t ns;
    if (st == 16) ns = 17;
    else if (st < 17) {
        if (st == 13) ns = 14;
        else return;
    } else if (st == 19) ns = 20;
    else if (st == 58) ns = 59;
    else return;
    WR8(activity_monkey + AE_MK_AI_STATE, ns);
}

/* MonkeyAi_IdleActivityLoop @8003F308: the idle timer +0x98 counts down; every 8th tick (low 3 bits zero) the target is looked for
   (UpdateSight with pattern 4 / 2 / 3 / 1 / 0 by state 17 / 14 / 20 / 59 / others) and a hit alerts the monkey (voice + alerted
   decision in special areas, else MonkeyStartNotice).  Other ticks: bit 15 of the counter set (negative) ends the idle: states
   17 / 14 / 20 become 18 / 15 / 21, others 60, then the animation restarts; otherwise a finished animation runs
   NodeActionDispatch and an unfinished one restarts the animation. */
/* Tick idle time, stagger sight checks and advance its ending phase. */
void ae_MonkeyAi_IdleActivityLoop(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    uint32_t timer = RD16(monkey + AE_MK_IDLE_ACTIVITY_TIMER) - 1u;
    WR16(monkey + AE_MK_IDLE_ACTIVITY_TIMER, timer);
    uint32_t timer_bits = timer & 0xFFFFu;
    if ((timer_bits & 7u) == 0) {
        uint32_t st = RD8(monkey + AE_MK_AI_STATE);
        uint32_t pattern;
        if (st == 17) pattern = 4;
        else if (st < 18) pattern = (st == 14) ? 2u : 0u;
        else if (st == 20) pattern = 3;
        else if (st == 59) pattern = 1;
        else pattern = 0;
        if (ae_MonkeyUpdateSight(r, h, pattern) == 0) return;
        WR8(cur_monkey(r) + AE_MK_ALERTED, 1);                /* Store before the following call reads the monkey. */
        if (ae_AreaIsSpecialMonkeyArea(r)) {
            int32_t voice = ae_MonkeyGetCallVoice(r);
            ae_MonkeyQueueVoice(r, h, voice, 0);
            ae_MonkeyAlertedDecide(r, h);
        } else {
            WR8(cur_monkey(r) + AE_MK_ALERTED, 1);            /* Store before the following call reads the monkey. */
            ae_MonkeyStartNotice(r, h);
        }
        return;
    }
    if ((int32_t)(timer_bits << 16) >= 0) {
        if (ae_MonkeyAnimFinished(r) == 0) {
            ae_MonkeyPickAnim(r);
            return;
        }
        ae_MonkeyNodeActionDispatch(r, h);
        return;
    }
    uint32_t st = RD8(monkey + AE_MK_AI_STATE);
    uint32_t ns;
    if (st == 17) ns = 18;
    else if (st < 18) ns = (st == 14) ? 15u : 60u;
    else ns = (st == 20) ? 21u : 60u;
    WR8(cur_monkey(r) + AE_MK_AI_STATE, ns);
    ae_MonkeyPickAnim(r);
}

/* MonkeyAi_GoToFlaggedNode @8004002C: once the animation is finished: the nearest node (3D distance below 60000) with flag 0x2000 becomes
   the next node (SetNavNode), its first link the node after that, LoadLinkExtra, ArriveAtNode; the alerted decision when nothing
   happened on arrival. The fallback node preserves the caller-provided value used by the original when no node qualifies;
   callers must supply that value explicitly. */
/* Move to the nearest flagged node, retaining the supplied fallback when none qualify. */
void ae_MonkeyAi_GoToFlaggedNode(AeRam *r, const AeMonkeyHooks *h, uint32_t fallback_node) {
    uint32_t sp = AE_STK(AE_STK_GOTO_FLAGGED);
    uint32_t best_distance = 0xEA60u;
    uint32_t best_node = fallback_node;
    if (ae_MonkeyAnimFinished(r) != 0) return;
    int32_t cnt = RDS16(AE_G_NAV_NODE_COUNT);
    uint32_t node_index = 0;
    if (0 < cnt) {
        for (;;) {
            uint32_t node_index_narrow = node_index & 0xFFFFu;
            uint32_t nodes = RD32(AE_G_NAV_NODES);
            if (RD32(node_index_narrow * AE_NAV_NODE_SIZE + nodes) & 0x2000u) {
                ae_NavNodeGetPos(r, h, (int32_t)node_index_narrow, sp + 16);
                if (ae_ram_failed(r)) return;
                uint32_t d = (uint32_t)ae_Distance3D(r, cur_monkey(r) + AE_MK_POS, sp + 16);
                if ((d & 0xFFFFu) < (best_distance & 0xFFFFu)) {
                    best_node = node_index;
                    best_distance = d;
                }
            }
            node_index += 1;
            if (!((int32_t)(node_index & 0xFFFFu) < RDS16(AE_G_NAV_NODE_COUNT))) break;
        }
    }
    int32_t node = s16(best_node);
    ae_MonkeySetNavNode(r, h, node);
    if (ae_ram_failed(r)) return;
    int32_t link = ae_NavNodeGetLink(r, node, 0);
    if (ae_ram_failed(r)) return;
    uint32_t monkey = cur_monkey(r);
    int32_t current_node = RDS16(monkey + AE_MK_NAV_NODE0);
    WR16(monkey + AE_MK_NAV_NODE1, link);                /* Store before the following call reads the monkey. */
    ae_MonkeyLoadLinkExtra(r, s16((uint32_t)link), current_node);
    if (ae_ram_failed(r)) return;
    if (ae_MonkeyArriveAtNode(r, h, node) != 0) return;
    ae_MonkeyAlertedDecide(r, h);
}
