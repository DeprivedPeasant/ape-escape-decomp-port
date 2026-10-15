/*
 * Monkey level5 routines, hand-ported and verified against the original game.
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
/* Return the unsigned high half of a 64-bit product. */
static uint32_t mulu_hi(uint32_t a, uint32_t b) { return (uint32_t)(((uint64_t)a * (uint64_t)b) >> 32); }
/* Read the current monkey address. */
static uint32_t cur_monkey(AeRam *r) { return RD32(AE_G_CUR_MONKEY); }
/* Locate a navigation node. */
static uint32_t node_of(AeRam *r, uint32_t index) {      /* nav node address: table + index * 0x18 */
    return index * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES);
}

/* MonkeyRand() % 100 as the compiler computes it: x = rand & 0xFFFF, q = x * 0x51EB851F >> 37, (x - q * 100) & 0xFFFF */
/* Choose a random percentage. */
static uint32_t rand_mod100(AeRam *r) {
    uint32_t x = ae_MonkeyRand(r) & 0xFFFFu;
    uint32_t q = mulu_hi(x, 0x51EB851Fu) >> 5;
    return (x - q * 100u) & 0xFFFFu;
}
/* the same for % 10 (multiply by 0xCCCCCCCD, >> 35) */
/* Choose a random digit. */
static uint32_t rand_mod10(AeRam *r) {
    uint32_t x = ae_MonkeyRand(r) & 0xFFFFu;
    uint32_t q = mulu_hi(x, 0xCCCCCCCDu) >> 3;
    return (x - q * 10u) & 0xFFFFu;
}

/* ================================================================================================
 * MonkeyNodeActionDispatch
 * ============================================================================================== */

/* MonkeyNodeActionDispatch @80036CD0: enters the state that the flags2 word asks for (one of them, in this priority order) and
   returns 1; returns 0 when flags bit 22 is set or nothing applies.  After an entry (not for jump arc states, the states 9 /
   0x1A / 0x35, flags2 bit 9, boss arenas) the "previous node" +0xEC is reset to -1.
     flags2 0x100 -> EnterState_1D; 0x800 with node1 flag 0x200 -> _1E; 0x20 -> _1A; 0x20000 -> _35; 0x8 -> SetState(10); 0x4 -> _09;
     0x10 -> _24 (or _0C while +0x94 > 0); 0x1 -> _04; 0x10000 -> _2C; 0x800000 -> _3D; 0x1000000 in state 61 -> _3E;
     otherwise a ground monkey (state 3) hops (_04) in 10 % of the frames when its distance (+0x218) is 300..800 and the two
     nodes do not both have flag 1.  The `% 100 < 100` test of the 0x10 branch is always true. */
/* Enter the highest-priority action requested by link flags. */
int32_t ae_MonkeyNodeActionDispatch(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    if ((RD32(monkey + AE_MK_FLAGS) >> 22) & 1u) return 0;
    uint32_t link_flags = RD32(monkey + AE_MK_FLAGS2);
    int entered = 0;
    if (link_flags & 0x100u) {
        ae_MonkeyEnterState_1D(r, h);
        entered = 1;
    } else {
        if (link_flags & 0x800u) {
            uint32_t n1 = RD16(monkey + AE_MK_NAV_NODE1);
            if (RD32(node_of(r, n1)) & 0x200u) {
                ae_MonkeyEnterState_1E(r, h);
                entered = 1;
            }
        }
        if (!entered) {
            monkey = cur_monkey(r);
            link_flags = RD32(monkey + AE_MK_FLAGS2);
            if (link_flags & 0x20u) {
                ae_MonkeyEnterState_1A(r);
                entered = 1;
            } else if (link_flags & 0x20000u) {
                ae_MonkeyEnterState_35(r);
                entered = 1;
            } else if (link_flags & 0x8u) {
                ae_MonkeySetState(r, 10);
                entered = 1;
            } else if (link_flags & 0x4u) {
                ae_MonkeyEnterState_09(r);
                entered = 1;
            } else if (link_flags & 0x10u) {
                if (RDS16(monkey + AE_MK_ACTION_COOLDOWN) > 0) {
                    ae_MonkeyEnterState_0C(r, h);
                } else if (rand_mod100(r) < 100u) {
                    ae_MonkeyEnterState_24(r, h);
                } else {
                    ae_MonkeyEnterState_0C(r, h);
                }
                entered = 1;
            } else if (link_flags & 0x1u) {
                ae_MonkeyEnterState_04(r, h);
                entered = 1;
            } else if (link_flags & 0x10000u) {
                ae_MonkeyEnterState_2C(r, h);
                entered = 1;
            } else if (link_flags & 0x800000u) {
                ae_MonkeyEnterState_3D(r, h);
                entered = 1;
            } else {
                if (link_flags & 0x1000000u) {
                    if (RD8(monkey + AE_MK_AI_STATE) == 61) {
                        ae_MonkeyEnterState_3E(r);
                        entered = 1;
                    }
                }
                if (!entered) {
                    uint32_t c = cur_monkey(r);
                    if (RD8(c + AE_MK_AI_STATE) == 3 && !ae_AreaIsBossArena(r) && rand_mod100(r) < 10u) {
                        uint32_t reloaded_monkey = cur_monkey(r);
                        uint32_t nodes = RD32(AE_G_NAV_NODES);
                        uint32_t f0 = RD32(RD16(reloaded_monkey + AE_MK_NAV_NODE0) * AE_NAV_NODE_SIZE + nodes);
                        int blocked = 0;
                        if (f0 & 1u) {
                            uint32_t f1 = RD32(RD16(reloaded_monkey + AE_MK_NAV_NODE1) * AE_NAV_NODE_SIZE + nodes);
                            if (f1 & 1u) blocked = 1;
                        }
                        if (!blocked && (RD32(reloaded_monkey + AE_MK_DIST_TO_GOAL) - 300u) < 501u) {
                            ae_MonkeyEnterState_04(r, h);
                            entered = 1;
                        }
                    }
                }
            }
        }
    }
    if (ae_ram_failed(r)) return 0;
    if (!entered) return 0;
    if (ae_MonkeyIsJumpArcState(r)) return 1;
    uint32_t c = cur_monkey(r);
    uint32_t st = RD8(c + AE_MK_AI_STATE);
    if (st == 9 || st == 0x1A) return 1;
    if (st == 0x35) return 1;
    if (RD32(c + AE_MK_FLAGS2) & 0x200u) return 1;
    if (ae_AreaIsBossArena(r)) return 1;
    WR16(cur_monkey(r) + AE_MK_PREV_NODE, 0xFFFFu);
    return 1;
}

/* ================================================================================================
 * Turning back
 * ============================================================================================== */

/* the direction to the target (x, 0, z as s16 from the low halves of the positions) at sp + 16 */
/* Build the horizontal direction from monkey to target. */
static void target_dir_xz(AeRam *r, uint32_t sp) {
    uint32_t target = RD32(AE_G_MONKEY_TARGET);
    uint32_t monkey = cur_monkey(r);
    WR16(sp + 16, RD16(target + AE_MK_POS) - RD16(monkey + AE_MK_POS));
    WR16(sp + 18, 0);
    WR16(sp + 20, RD16(target + AE_MK_POS + 8) - RD16(monkey + AE_MK_POS + 8));
}

/* the direction from the monkey to the node position at sp + 32 as (x, 0, z) at sp + 24, the third component stored in the
   direction is complete before the hesitation check; returns that check's result */
/* Build the direction to a node before testing hesitation. */
static int32_t node_dir_and_hesitate(AeRam *r, uint32_t sp) {
    uint32_t monkey = cur_monkey(r);
    uint32_t node_component = RD16(sp + 32);
    uint32_t monkey_component = RD16(monkey + AE_MK_POS);
    WR16(sp + 26, 0);
    WR16(sp + 24, node_component - monkey_component);
    node_component = RD16(sp + 40);
    monkey_component = RD16(monkey + AE_MK_POS + 8);
    WR16(sp + 28, node_component - monkey_component);                      /* Store before the following call reads the monkey. */
    return ae_MonkeyRandomHesitate(r);
}

/* MonkeyConsiderTurnBack @800386C8: boss arenas: 1.  Decides whether the monkey reverses along the current link (swaps the two
   nodes): immediately when the previous node (+0xEC) is the current node; never (1) when it is the next node or when there is no
   link between the nodes; otherwise by the visit counters (the node with the larger counter at the front is preferred) and the
   cosine of the direction to the target against the direction to the node (threshold = rcos of the approach angle, near for
   special areas).  Returns 1 (turned or nothing to do), 0 (not turned); on one path the distance returned by SwapNavNodes. */
/* Decide whether to reverse the current link using history and alignment. */
int32_t ae_MonkeyConsiderTurnBack(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_TURN_BACK);
    if (ae_AreaIsBossArena(r)) return 1;
    uint32_t current_monkey = cur_monkey(r);
    uint32_t current_node = RD16(current_monkey + AE_MK_NAV_NODE0);
    int32_t previous_node = RDS16(current_monkey + AE_MK_PREV_NODE);
    if (current_node == (uint32_t)previous_node) {
        ae_MonkeySwapNavNodes(r, h);
        return 1;
    }
    uint32_t next_node = RD16(current_monkey + AE_MK_NAV_NODE1);
    if (next_node == (uint32_t)previous_node) return 1;
    if (ae_NavNodeLinkFlag(r, s16(current_node), s16(next_node)) == 0) return 1;
    uint32_t m = cur_monkey(r);
    if (ae_NavNodeLinkFlag(r, RDS16(m + AE_MK_NAV_NODE1), RDS16(m + AE_MK_NAV_NODE0)) == 0) {
        return ae_MonkeySwapNavNodes(r, h);
    }
    int32_t angle;
    if (ae_AreaIsSpecialMonkeyArea(r)) angle = ae_MonkeyApproachAngleNear(r);
    else angle = ae_MonkeyApproachAngleFar(r);
    if (ae_ram_failed(r)) return 0;
    target_dir_xz(r, sp);
    uint32_t monkey = cur_monkey(r);
    uint32_t current_node_again = RD16(monkey + AE_MK_NAV_NODE0);
    uint32_t slot = RD8(monkey + AE_MK_SLOT);
    uint32_t n1 = RD16(monkey + AE_MK_NAV_NODE1);
    uint32_t nodes = RD32(AE_G_NAV_NODES);
    uint32_t cnt0 = RD8(current_node_again * AE_NAV_NODE_SIZE + nodes + slot + AE_NAV_NODE_VISIT_MARKS);
    uint32_t cnt1 = RD8(n1 * AE_NAV_NODE_SIZE + nodes + slot + AE_NAV_NODE_VISIT_MARKS);
    if (cnt0 < cnt1) {
        ae_NavNodeGetPos(r, h, (int32_t)n1, sp + 32);
        if (ae_ram_failed(r)) return 0;
        int32_t hes = node_dir_and_hesitate(r, sp);
        if (hes == 0) {
            int32_t dot = s16((uint32_t)ae_DotXZ_SV(r, sp + 16, sp + 24));
            int32_t c = ae_sdk_rcos(r, angle);
            if (!(dot < c)) return 0;
        }
        ae_MonkeySwapNavNodes(r, h);
        return 1;
    }
    ae_NavNodeGetPos(r, h, (int32_t)current_node_again, sp + 32);
    if (ae_ram_failed(r)) return 0;
    if (node_dir_and_hesitate(r, sp) != 0) return 1;
    int32_t dot = s16((uint32_t)ae_DotXZ_SV(r, sp + 16, sp + 24));
    int32_t c = ae_sdk_rcos(r, angle);
    return dot < c ? 1 : 0;
}

/* MonkeyConsiderTurnBackRandom @80038914: the alerted variant: swaps at once when the previous node is the current node; does
   nothing when it is the next node or the nodes are not linked; swaps when the link does not work in the other direction; if it
   does, 3 frames in 10 it swaps when MonkeyNodeTargetCosines gives a non-zero result (its result is the second cosine, out1), out0 >
   rcos(682) and out1 < out0 */
/* Occasionally reverse an alerted monkey using target alignment. */
void ae_MonkeyConsiderTurnBackRandom(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_TURN_BACK_RANDOM);
    uint32_t current_monkey = cur_monkey(r);
    uint32_t current_node = RD16(current_monkey + AE_MK_NAV_NODE0);
    int32_t previous_node = RDS16(current_monkey + AE_MK_PREV_NODE);
    if (current_node == (uint32_t)previous_node) {
        ae_MonkeySwapNavNodes(r, h);
        return;
    }
    uint32_t next_node = RD16(current_monkey + AE_MK_NAV_NODE1);
    if (next_node == (uint32_t)previous_node) return;
    if (ae_NavNodeLinkFlag(r, s16(current_node), s16(next_node)) == 0) return;
    uint32_t m = cur_monkey(r);
    if (ae_NavNodeLinkFlag(r, RDS16(m + AE_MK_NAV_NODE1), RDS16(m + AE_MK_NAV_NODE0)) != 0) {
        if (rand_mod10(r) < 7u) return;
        ae_MonkeyNodeTargetCosines(r, h, sp + 16, sp + 18);
        if (ae_ram_failed(r)) return;
        /* the original tests the $previous_node left by NodeTargetCosines: the second DotXZ_SV result, which it also stores (halfword) at out1;
           the result is at most 4096 in magnitude, so the stored halfword has the same value */
        if (RDS16(sp + 18) == 0) return;
        int32_t current_node_cosine = RDS16(sp + 16);
        if (!(ae_sdk_rcos(r, 682) < current_node_cosine)) return;
        if (!(RDS16(sp + 18) < RDS16(sp + 16))) return;
    }
    ae_MonkeySwapNavNodes(r, h);
}
