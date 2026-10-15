/*
 * Monkey level4 routines, hand-ported and verified against the original game.
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
/* Multiply with 32-bit wraparound. */
static int32_t mul_lo(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
/* Return the signed high half of a 64-bit product. */
static int32_t mul_hi(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * (int64_t)b) >> 32); }
/* Return the unsigned high half of a 64-bit product. */
static uint32_t mulu_hi(uint32_t a, uint32_t b) { return (uint32_t)(((uint64_t)a * (uint64_t)b) >> 32); }
/* Subtract with 32-bit wraparound. */
static int32_t sub32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
/* Read the current monkey address. */
static uint32_t cur_monkey(AeRam *r) { return RD32(AE_G_CUR_MONKEY); }
/* s16 / 100 as the compiler does it: (v * 0x51EB851F) >> 37 - (v >> 31), then narrowed to s16 */
/* Divide by 100 and narrow the quotient to a signed halfword. */
static int32_t div100_s16(int32_t v) {
    return s16((uint32_t)sub32(sra32(mul_hi(v, 0x51EB851F), 5), sra32(v, 31)));
}

/* ================================================================================================
 * Navigation node history / state entries
 * ============================================================================================== */

/* MonkeySetNavNode @80035080: shifts the node history (+0xF2.. six u16, newest first) and stores `node` in front; updates the
   goal; afterwards copies the new current node (+0xF4) to +0xEC (the "came from" node) for jump arcs with flags2 bit 0, the
   states 9 / 0x1A / 0x35, flags2 bit 9 and boss arenas. */
/* Shift navigation history and refresh the new node goal. */
void ae_MonkeySetNavNode(AeRam *r, const AeMonkeyHooks *h, int32_t node) {
    uint32_t monkey = cur_monkey(r);
    for (int32_t history_index = 5; history_index > 0; --history_index) {
        WR16(monkey + AE_MK_NAV_NODE0 + (uint32_t)history_index * 2u, RD16(monkey + AE_MK_NAV_NODE0 + (uint32_t)(history_index - 1) * 2u));
    }
    uint32_t reloaded_monkey = cur_monkey(r);
    WR16(reloaded_monkey + AE_MK_NAV_NODE0, node);                       /* Store before the following call reads the monkey. */
    ae_MonkeyUpdateGoal(r, h);
    if (ae_ram_failed(r)) return;
    int copy = 0;
    if (ae_MonkeyIsJumpArcState(r)) {
        if (RD32(cur_monkey(r) + AE_MK_FLAGS2) & 1u) copy = 1;
    }
    if (!copy) {
        uint32_t state_monkey = cur_monkey(r);
        uint32_t st = RD8(state_monkey + AE_MK_AI_STATE);
        if (st == 9 || st == 0x1A) {
            copy = 1;
        } else if (st == 0x35) {
            copy = 1;
        } else if (RD32(state_monkey + AE_MK_FLAGS2) & 0x200u) {
            copy = 1;
        } else {
            int32_t boss = ae_AreaIsBossArena(r);
            if (boss == 0) return;
            copy = 1;
        }
    }
    uint32_t updated_monkey = cur_monkey(r);
    WR16(updated_monkey + AE_MK_PREV_NODE, RD16(updated_monkey + AE_MK_NAV_NODE1));
}

/* MonkeyEnterState_1D @800359F0: body off (unless already in state 29), state 29, anim, the extra record of the current link
   (its out word's low half becomes the timer +0x128), goal; always returns 1 */
/* Enter body-disabled traversal with the extra-link duration. */
int32_t ae_MonkeyEnterState_1D(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_ENTER_1D);
    if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != 29) {
        ae_MonkeyBodyOff(r, h);
        if (ae_ram_failed(r)) return 0;
    }
    WR8(cur_monkey(r) + AE_MK_AI_STATE, 29);     /* Store before the following call reads the monkey. */
    ae_MonkeyPickAnim(r);
    uint32_t monkey = cur_monkey(r);
    int32_t next_node = RDS16(monkey + AE_MK_NAV_NODE1);
    int32_t current_node = RDS16(monkey + AE_MK_NAV_NODE0);
    ae_NavLinkLookupExtra(r, next_node, current_node, sp + 16);
    if (ae_ram_failed(r)) return 0;
    ae_MonkeyUpdateGoal(r, h);
    if (ae_ram_failed(r)) return 0;
    uint32_t reloaded_monkey = cur_monkey(r);
    uint32_t out = RD16(sp + 16);
    uint32_t n1 = RD16(reloaded_monkey + AE_MK_NAV_NODE1);
    WR16(reloaded_monkey + AE_MK_ANIM_LENGTH, out);
    WR16(reloaded_monkey + AE_MK_COUNTER96, 0);
    WR16(reloaded_monkey + AE_MK_PREV_NODE, n1);
    return 1;
}

/* the animation length (last frame - first frame of the table entry of the current state) used by states 04 / 2C: the
   original calls MonkeyGetAnimIndex twice and reads the two table halfwords */
/* Read the first and last frames of the current animation. */
static uint32_t anim_span_parts(AeRam *r, uint32_t *first) {
    int32_t i0 = ae_MonkeyGetAnimIndex(r);
    int32_t i1 = ae_MonkeyGetAnimIndex(r);
    uint32_t tab = RD32(cur_monkey(r) + AE_MK_ANIM_TABLE);
    uint32_t last = RD16((uint32_t)i0 * 8u + tab + AE_MK_ANIM_LAST_FRAME);
    *first = RD16((uint32_t)i1 * 8u + tab);
    return last;
}

/* MonkeyEnterState_04 @80035A7C: state 4 (counter +0x172 = 5), animation length into +0x170, goal, jump arc, dust, voices
   (boss arenas: 177 / 17 / 19 with length - 1; else 68 / 69 with length + 1); returns 1 */
/* Enter a jump arc with landing animation and delayed sounds. */
int32_t ae_MonkeyEnterState_04(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);
    WR8(monkey + AE_MK_AI_STATE, 4);
    WR16(monkey + AE_MK_PREV_NODE, next_node);                         /* Store before the following call reads the monkey. */
    ae_MonkeyPickAnim(r);
    WR16(cur_monkey(r) + AE_MK_JUMP_RISE_FRAMES, 5);
    uint32_t first;
    uint32_t last = anim_span_parts(r, &first);
    uint32_t cm = cur_monkey(r);
    uint32_t landing_frames = last - first + 1u;
    WR16(cm + AE_MK_JUMP_LAND_FRAMES, landing_frames);                        /* Store before the following call reads the monkey. */
    ae_MonkeyUpdateGoal(r, h);
    if (ae_ram_failed(r)) return 0;
    ae_MonkeyPrepareJumpArc(r, h);
    if (ae_ram_failed(r)) return 0;
    ae_MonkeyFootDust(r, h);
    if (ae_ram_failed(r)) return 0;
    if (ae_AreaIsBossArena(r)) {
        ae_MonkeyQueueVoice(r, h, 177, 0);
        ae_MonkeyQueueVoice(r, h, 17, 0);
        int32_t i0 = ae_MonkeyGetAnimIndex(r);
        int32_t i1 = ae_MonkeyGetAnimIndex(r);
        uint32_t tab = RD32(cur_monkey(r) + AE_MK_ANIM_TABLE);
        uint32_t last_frame = RD16((uint32_t)i0 * 8u + tab + AE_MK_ANIM_LAST_FRAME);
        uint32_t l = RD16((uint32_t)i1 * 8u + tab);
        ae_MonkeyQueueVoice(r, h, 19, (int32_t)(last_frame - l - 1u));
    } else {
        ae_MonkeyQueueVoice(r, h, 68, 3);
        uint32_t f2;
        uint32_t l2 = anim_span_parts(r, &f2);
        ae_MonkeyQueueVoice(r, h, 69, (int32_t)(l2 - f2 + 1u));
    }
    return 1;
}

/* MonkeyEnterState_2C @80036964: like state 04 without the boss branch (counter +0x172 = 3; +0x170 is the length of the fixed
   table entry 34; voices 68 / 69 with the length of the current animation) */
/* Enter the phased jump arc with fixed landing animation length. */
int32_t ae_MonkeyEnterState_2C(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);
    WR16(monkey + AE_MK_JUMP_RISE_FRAMES, 3);
    WR8(monkey + AE_MK_AI_STATE, 0x2C);
    WR16(monkey + AE_MK_PREV_NODE, next_node);                         /* Store before the following call reads the monkey. */
    ae_MonkeyPickAnim(r);
    uint32_t monkey_after_anim = cur_monkey(r);
    uint32_t tab = RD32(monkey_after_anim + AE_MK_ANIM_TABLE);          /* the fixed table entry 34 (not the animation index) */
    uint32_t last_frame = RD16(tab + 34u * 8u + AE_MK_ANIM_LAST_FRAME);
    uint32_t first_frame = RD16(tab + 34u * 8u + AE_MK_ANIM_FIRST_FRAME);
    WR16(monkey_after_anim + AE_MK_JUMP_LAND_FRAMES, last_frame - first_frame + 1u);             /* Store before the following call reads the monkey. */
    ae_MonkeyUpdateGoal(r, h);
    if (ae_ram_failed(r)) return 0;
    ae_MonkeyPrepareJumpArc(r, h);
    if (ae_ram_failed(r)) return 0;
    ae_MonkeyFootDust(r, h);
    if (ae_ram_failed(r)) return 0;
    ae_MonkeyQueueVoice(r, h, 68, 3);
    uint32_t f2;
    uint32_t l2 = anim_span_parts(r, &f2);
    ae_MonkeyQueueVoice(r, h, 69, (int32_t)(l2 - f2));
    return 1;
}

/* MonkeySwapNavNodes @80038694: swaps the current / next node and updates the goal.  Returns the $v0 the original leaves:
   that of MonkeyUpdateGoal -> MonkeyUpdateGoalPos, i.e. the Distance3D value it stores at +0x218 (MonkeyConsiderTurnBack
   returns it on one path). */
/* Reverse the current link and return the refreshed goal distance. */
int32_t ae_MonkeySwapNavNodes(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);
    uint32_t current_node = RD16(monkey + AE_MK_NAV_NODE0);
    WR16(monkey + AE_MK_NAV_NODE0, next_node);
    WR16(monkey + AE_MK_NAV_NODE1, current_node);              /* Store before the following call reads the monkey. */
    ae_MonkeyUpdateGoal(r, h);
    return RDS32(cur_monkey(r) + AE_MK_DIST_TO_GOAL);
}

/* MonkeyAi_Fall @8003FCD8: y = s16(+0xE4) + counter^2 * 6 (counter = s16 +0x92, product in 32 bits); landing (ground below the
   monkey 50 up is above y) enters state 0x30 and snaps y to the ground; the counter +0x92 grows by one */
/* Advance the falling parabola and enter the landing state on contact. */
void ae_MonkeyAi_Fall(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    int32_t fall_frame = RDS16(monkey + AE_MK_THROW_TIMER);
    int32_t scaled_frame = mul_lo(fall_frame, 6);
    int32_t lo = mul_lo(fall_frame, scaled_frame);
    int32_t start_y = RDS16(monkey + AE_MK_HOP_START_Y);
    uint32_t x = RD32(monkey + AE_MK_POS);
    uint32_t z = RD32(monkey + AE_MK_POS + 8);
    uint32_t y = (uint32_t)start_y + (uint32_t)lo;
    WR32(monkey + AE_MK_POS + 4, y);
    int32_t ground = AE_MK_HOOK(r, h, GetGroundHeight, x, y + 50u, z, 0);
    int32_t cy = RDS32(monkey + AE_MK_POS + 4);
    if (ground < cy) {
        ae_MonkeyEnterState_30(r, h);
        if (ae_ram_failed(r)) return;
        WR32(cur_monkey(r) + AE_MK_POS + 4, (uint32_t)ground);
    }
    uint32_t m = cur_monkey(r);
    WR16(m + AE_MK_THROW_TIMER, RD16(m + AE_MK_THROW_TIMER) + 1u);
}

/* ================================================================================================
 * MonkeyArriveAtNode
 * ============================================================================================== */

/* MonkeyArriveAtNode @80040D90: only for activity 4.  Applies the flags of the node (nav node flags): bit 5 -> monkey flags
   bit 26; flags2 bit... see below.  Returns 1 when a state change happened (and sets flags bit 20), else 0. */
/* Apply node events and flags, entering the requested arrival state. */
int32_t ae_MonkeyArriveAtNode(AeRam *r, const AeMonkeyHooks *h, int32_t node) {
    uint32_t sp = AE_STK(AE_STK_ARRIVE);
    uint32_t monkey = cur_monkey(r);
    int32_t state_changed = 0;
    if (RD8(monkey + AE_MK_ACTIVITY) != 4) return 0;
    int32_t node_index = s16((uint32_t)node);
    uint32_t nf = RD32((uint32_t)node_index * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES));
    uint32_t flags;
    if (nf & 0x20u) flags = RD32(monkey + AE_MK_FLAGS) | 0x04000000u;
    else flags = RD32(monkey + AE_MK_FLAGS) & 0xFBFFFFFFu;
    WR32(monkey + AE_MK_FLAGS, flags);
    uint32_t reloaded_monkey = cur_monkey(r);
    flags = RD32(reloaded_monkey + AE_MK_FLAGS) & 0xFFBFFFFFu;
    uint32_t node_flags = RD8(reloaded_monkey + AE_MK_ALERTED);
    WR32(reloaded_monkey + AE_MK_FLAGS, flags);
    if (node_flags == 1) {
        nf = RD32((uint32_t)node_index * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES));
        if (nf & 0x1000u) {
            uint32_t f3 = RD32(reloaded_monkey + AE_MK_FLAGS3);
            WR16(reloaded_monkey + AE_MK_COUNTER96, 0);
            WR16(reloaded_monkey + AE_MK_ANIM_LENGTH, 30);
            WR32(reloaded_monkey + AE_MK_FLAGS3, f3 | 1u);    /* Store before the following call reads the monkey. */
            ae_MonkeyQueueVoice(r, h, 39, 0);
        }
    }
    uint32_t node_offset = (uint32_t)node_index * AE_NAV_NODE_SIZE;
    uint32_t nd = node_offset + RD32(AE_G_NAV_NODES);
    if (RD32(nd) & 0x4000u) {
        AE_MK_HOOK(r, h, ObjSlotSetField_CF, RDS16(nd + AE_NAV_LINK_CODE), 1, 0, 0);
    }
    nd = node_offset + RD32(AE_G_NAV_NODES);
    if (RD32(nd) & 0x800u) {
        AE_MK_HOOK(r, h, GimmickEvent, 0, RDS16(nd + AE_NAV_LINK_CODE), 0, 0);
    }
    nd = node_offset + RD32(AE_G_NAV_NODES);
    if (RD32(nd) & 0x20000u) {
        AE_MK_HOOK(r, h, GimmickEvent, 1, RDS16(nd + AE_NAV_LINK_CODE), 0, 0);
    }
    nd = node_offset + RD32(AE_G_NAV_NODES);
    if (RD32(nd) & 0x40000u) {
        AE_MK_HOOK(r, h, GimmickEvent, 2, RDS16(nd + AE_NAV_LINK_CODE), 0, 0);
    }
    nd = node_offset + RD32(AE_G_NAV_NODES);
    if (RD32(nd) & 0x400u) {
        ae_NavNodeGetLinkIds(r, node_index, sp + 16, sp + 18, sp + 20);
        if (ae_ram_failed(r)) return 0;
        ae_MonkeySetState(r, 0x29);
        state_changed = 1;
    }
    if (RD8(cur_monkey(r) + AE_MK_ALERTED) == 0) {
        nd = node_offset + RD32(AE_G_NAV_NODES);
        if (RD32(nd) & 0x80000u) {
            ae_MonkeyEnterState_32(r, (uint32_t)node_index);
            if (ae_ram_failed(r)) return 0;
            state_changed = 1;
        }
    }
    uint32_t node_addr = (uint32_t)s16((uint32_t)node) * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES);
    node_offset = (uint32_t)s16((uint32_t)node) * AE_NAV_NODE_SIZE;
    node_flags = RD32(node_addr);
    if (node_flags & 4u) {
        uint32_t t = RD16(node_addr + AE_NAV_LINK_CODE);
        int32_t f = RDS16(node_addr + AE_NAV_LINK_CODE);
        uint32_t waiting_monkey = cur_monkey(r);
        uint32_t keep = RD8(waiting_monkey + AE_MK_ALERTED);
        if (f < 0) t = 0;
        uint32_t fl = RD32(waiting_monkey + AE_MK_FLAGS);
        WR16(waiting_monkey + AE_MK_NODE_WAIT, t);
        WR32(waiting_monkey + AE_MK_FLAGS, fl | 0x400000u);  /* Store before the following call reads the monkey. */
        ae_MonkeySetState(r, 0x42);
        state_changed = 1;
        WR8(cur_monkey(r) + AE_MK_ALERTED, keep);
    } else {
        int go_end = 0;
        uint32_t next_state = 0;                     /* the state argument of the final MonkeySetState */
        if (node_flags & 8u) {
            if (ae_AreaIsBossArena(r)) {
                uint32_t c = cur_monkey(r);
                uint32_t previous_contact_kind = RD16(c + AE_MK_KIND);
                uint32_t bt = ae_BodyContactType(r, c);
                next_state = (previous_contact_kind != bt) ? 0x16u : 0x26u;
                ae_MonkeySetState(r, next_state);
                state_changed = 1;
                uint32_t c2 = cur_monkey(r);
                uint32_t fl = RD32(c2 + AE_MK_FLAGS);
                WR8(c2 + AE_MK_ALERTED, 0);
                WR32(c2 + AE_MK_FLAGS, fl | 0x400000u);
                go_end = 1;
            } else {
                nd = node_offset + RD32(AE_G_NAV_NODES);
                if (RD32(nd) & 8u) {
                    uint32_t c = cur_monkey(r);
                    int32_t f = RDS16(nd + AE_NAV_LINK_CODE);
                    int32_t dist = RDS32(c + AE_MK_DIST_TO_TARGET);
                    if (f < dist) {
                        ae_MonkeyEnterRandomLook(r);          /* The following updates apply to the entered state. */
                        state_changed = 1;
                        uint32_t c2 = cur_monkey(r);
                        uint32_t fl = RD32(c2 + AE_MK_FLAGS);
                        WR8(c2 + AE_MK_ALERTED, 0);
                        WR32(c2 + AE_MK_FLAGS, fl | 0x400000u);
                        go_end = 1;
                    }
                }
            }
        }
        if (!go_end) {
            int do_state = 0;
            nd = (uint32_t)s16((uint32_t)node) * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES);
            if (RD32(nd) & 0x8000u) {
                uint32_t c = cur_monkey(r);
                if (RDS32(c + AE_MK_DIST_TO_TARGET) < 1500) { next_state = 0x42; do_state = 1; }
            }
            if (!do_state) {
                uint32_t nodes = RD32(AE_G_NAV_NODES);
                nd = (uint32_t)s16((uint32_t)node) * AE_NAV_NODE_SIZE + nodes;
                if (RD32(nd) & 0x10000u) {
                    uint32_t c = cur_monkey(r);
                    uint32_t n1 = RD16(c + AE_MK_NAV_NODE1);
                    int32_t fld = RDS16(n1 * AE_NAV_NODE_SIZE + nodes + AE_NAV_LINK_CODE);
                    int32_t slot = div100_s16(fld);
                    int32_t idx = AE_MK_HOOK(r, h, ObjSlotGetAnimIndex, slot, 0, 0, 0);
                    next_state = 0;
                    if (idx != 2) do_state = 1;
                }
            }
            if (do_state) {
                uint32_t c = cur_monkey(r);
                WR32(c + AE_MK_FLAGS, RD32(c + AE_MK_FLAGS) | 0x400000u);   /* Store before the following call reads the monkey. */
                ae_MonkeySetState(r, next_state);
                state_changed = 1;
                WR8(cur_monkey(r) + AE_MK_ALERTED, 0);
            }
        }
    }
    if (ae_ram_failed(r)) return 0;
    if (state_changed != 0) {
        uint32_t c = cur_monkey(r);
        WR32(c + AE_MK_FLAGS, RD32(c + AE_MK_FLAGS) | 0x100000u);
    }
    return state_changed;
}

/* ================================================================================================
 * MonkeyPickBestLink
 * ============================================================================================== */

/* MonkeyPickBestLink @80038120: the link index (into the link list of the current node) the monkey takes next, or -1.
   Boss arenas: a random link.  Otherwise the links that are not the previous node (+0xEC) and not blocked (MarkBlockedLinks)
   are rated by the visit counter of the neighbour (byte at node + slot + 0x14, the largest wins; ties keep the first); a
   cosine test against the direction of the target (monkey +0x176, one s16 per link) can veto while alerted.  `limit` != 0
   forces a choice even when the current node's own counter is not smaller.  Returns the link (a node id), not the index. */
/* Choose an unblocked outgoing link using visits and target alignment. */
int32_t ae_MonkeyPickBestLink(AeRam *r, const AeMonkeyHooks *h, int32_t limit) {
    uint32_t sp = AE_STK(AE_STK_PICK_LINK);
    if (ae_AreaIsBossArena(r)) {
        uint32_t rnd = ae_MonkeyRand(r);
        uint32_t cm = cur_monkey(r);
        uint32_t current_node = RD16(cm + AE_MK_NAV_NODE0);
        uint32_t cnt = RD8(current_node * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + AE_NAV_ACTIVE_LINK_COUNT);
        uint32_t dividend = rnd & 0xFFFFu;
        if (cnt == 0) {
            ae_ram_fail(r, "divide trap (div by zero; the original executes break)");
            return 0;
        }
        uint32_t rem = dividend % cnt;
        return ae_NavNodeGetLink(r, s16(current_node), (int32_t)rem);
    }
    int32_t angle;
    int32_t hes = ae_MonkeyRandomHesitate(r);
    if (hes != 0) {
        angle = 0;
    } else if (ae_AreaIsSpecialMonkeyArea(r)) {
        angle = ae_MonkeyApproachAngleNear(r);
    } else {
        angle = 56;
        if (((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 26) & 1u) == 0) angle = ae_MonkeyApproachAngleFar(r);
    }
    if (ae_ram_failed(r)) return 0;
    int32_t cosine_limit = ae_sdk_rcos(r, angle);
    if (ae_MonkeyIsState0C_or_24(r)) cosine_limit = (int32_t)((uint32_t)cosine_limit << 1);
    uint32_t tgt = RD32(AE_G_MONKEY_TARGET);
    uint32_t cm = cur_monkey(r);
    WR16(sp + 120, RD16(tgt + AE_MK_POS) - RD16(cm + AE_MK_POS));
    WR16(sp + 122, RD16(tgt + AE_MK_POS + 4) - RD16(cm + AE_MK_POS + 4));
    WR16(sp + 124, RD16(tgt + AE_MK_POS + 8) - RD16(cm + AE_MK_POS + 8));
    int32_t best_visit_count = -10000;
    int32_t best_link_index = -1;
    int32_t best_node = 0;                              /* uninitialised register in the original; only used when best_link_index >= 0 */
    ae_MonkeyMarkBlockedLinks(r, h, RDS16(cm + AE_MK_NAV_NODE0));
    if (ae_ram_failed(r)) return 0;
    int32_t candidate_count = 0;
    int32_t link_index = 0;
    {
        uint32_t m = cur_monkey(r);
        uint32_t cnt = RD8(RD16(m + AE_MK_NAV_NODE0) * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + AE_NAV_ACTIVE_LINK_COUNT);
        if (cnt != 0) {
            do {
                uint32_t m1 = cur_monkey(r);
                int32_t linked_node = ae_NavNodeGetLink(r, RDS16(m1 + AE_MK_NAV_NODE0), s16((uint32_t)link_index));
                if (ae_ram_failed(r)) return 0;
                uint32_t monkey = cur_monkey(r);
                int32_t candidate_node = s16((uint32_t)linked_node);
                int32_t hist = RDS16(monkey + AE_MK_PREV_NODE);
                if (candidate_node != hist && RD8(monkey + (uint32_t)link_index + AE_MK_LINK_BLOCKED) == 0) {
                    WR16(sp + 16 + (uint32_t)(s16((uint32_t)candidate_count) * 2), (uint32_t)linked_node);
                    uint32_t cell = (uint32_t)candidate_node * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + RD8(monkey + AE_MK_SLOT);
                    uint32_t vis = RD8(cell + AE_NAV_NODE_VISIT_MARKS);
                    candidate_count += 1;
                    if (s16((uint32_t)best_visit_count) < (int32_t)vis) {
                        best_visit_count = (int32_t)vis;
                        best_link_index = link_index;
                        best_node = linked_node;
                    }
                }
                uint32_t mm = cur_monkey(r);
                uint32_t cnt2 = RD8(RD16(mm + AE_MK_NAV_NODE0) * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + AE_NAV_ACTIVE_LINK_COUNT);
                link_index += 1;
                if (!(link_index < (int32_t)cnt2)) break;
            } while (1);
        }
    }
    uint32_t monkey = cur_monkey(r);
    uint32_t current_visits = RD8(RD16(monkey + AE_MK_NAV_NODE0) * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + RD8(monkey + AE_MK_SLOT) + AE_NAV_NODE_VISIT_MARKS);
    int32_t best_visits_signed = s16((uint32_t)best_visit_count);
    if ((uint32_t)best_visit_count << 16 == 0 && current_visits == 0) {
        uint32_t fl = RD32(monkey + AE_MK_FLAGS);
        if ((fl >> 27) & 1u) WR32(monkey + AE_MK_FLAGS, fl & 0xF7FFFFFFu);
        return -1;
    }
    if (!((int32_t)current_visits < best_visits_signed) && (limit & 0xFFFF) == 0) return -1;
    int32_t b = s16((uint32_t)best_link_index);
    if (b >= 0) {
        uint32_t m = cur_monkey(r);
        if (RD8(m + AE_MK_ALERTED) == 0) return s16((uint32_t)best_node);
        if (ae_AreaIsBossArena(r) != 0) return s16((uint32_t)best_node);
        uint32_t current_monkey = cur_monkey(r);
        if (RD8(current_monkey + AE_MK_AI_STATE) == 0x3D) return s16((uint32_t)best_node);
        if (RDS16(current_monkey + (uint32_t)(b * 2) + AE_MK_LINK_SCORES) < cosine_limit) return s16((uint32_t)best_node);
    }
    /* second pass: the first link that is neither the previous node nor blocked and whose counter beats the current node's
       (or everything is unvisited), subject to the same cosine test while alerted */
    {
        uint32_t m = cur_monkey(r);
        uint32_t cnt = RD8(RD16(m + AE_MK_NAV_NODE0) * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + AE_NAV_ACTIVE_LINK_COUNT);
        if (cnt == 0) return -1;
        link_index = 0;
        int32_t current_visits_signed = (int32_t)current_visits;
        int32_t best_visits_narrow = s16((uint32_t)best_visit_count);
        do {
            uint32_t m1 = cur_monkey(r);
            int32_t v = ae_NavNodeGetLink(r, RDS16(m1 + AE_MK_NAV_NODE0), s16((uint32_t)link_index));
            if (ae_ram_failed(r)) return 0;
            uint32_t monkey = cur_monkey(r);
            int32_t linked_node = s16((uint32_t)v);
            int take = 0;
            if (linked_node != RDS16(monkey + AE_MK_PREV_NODE) && RD8(monkey + (uint32_t)link_index + AE_MK_LINK_BLOCKED) == 0) {
                uint32_t vis = RD8((uint32_t)linked_node * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + RD8(monkey + AE_MK_SLOT) + AE_NAV_NODE_VISIT_MARKS);
                int cand = 0;
                if (current_visits_signed < (int32_t)vis) cand = 1;
                else if (best_visits_narrow == 0 && current_visits_signed == 0 && vis == 0) cand = 1;
                if (cand) {
                    if (RD8(monkey + AE_MK_ALERTED) == 0) take = 1;
                    else if (RDS16(monkey + (uint32_t)(link_index * 2) + AE_MK_LINK_SCORES) < cosine_limit) take = 1;
                }
            }
            if (take) return linked_node;
            {
                uint32_t mm = cur_monkey(r);
                uint32_t cnt2 = RD8(RD16(mm + AE_MK_NAV_NODE0) * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + AE_NAV_ACTIVE_LINK_COUNT);
                link_index += 1;
                if (!(link_index < (int32_t)cnt2)) break;
            }
        } while (1);
    }
    return -1;
}

/* ================================================================================================
 * MonkeyUpdateVisuals
 * ============================================================================================== */

/* MonkeyUpdateVisuals @80042B68: for the four monkeys of the table at 0x800E54F0: sets g_CurMonkey, draws the monkey that is
   on screen (alerted and visible) or frees the looping sound of area 0x24; see the comments in the code. */
/* Update visible monkey animation, shadow, detail level and drawing. */
void ae_MonkeyUpdateVisuals(AeRam *r, const AeMonkeyHooks *h) {
    int32_t active_index = 0;
    do {
        uint32_t monkey = RD32(AE_G_MONKEY_PTRS + (uint32_t)s16((uint32_t)active_index) * 4u);
        uint32_t flags = RD32(monkey + AE_MK_FLAGS);
        WR32(AE_G_CUR_MONKEY, monkey);               /* Store before the following call reads the monkey. */
        int skip = 0;
        int test2 = 1;
        if ((flags & 0x40030000u) == 0x40020000u) {
            int32_t phase_value = RDS16(monkey + AE_MK_NOTICE_COUNT);
            if (phase_value > 0) { skip = 1; test2 = 0; }
            else if (!(phase_value < (int32_t)RD16(monkey + AE_MK_ANIM_LENGTH))) { skip = 1; test2 = 0; }
        }
        if (test2) {
            if ((int32_t)flags < 0) skip = 1;
            else if (RD32(monkey + AE_MK_FLAGS2) & 0x4000u) skip = 1;
            else if (monkey == 0) skip = 1;
            else if (RD8(monkey + AE_MK_ACTIVITY) == 2) skip = 1;
            else {
                int32_t off = ae_MonkeyProjectToScreen(r, h);
                if (ae_ram_failed(r)) return;
                if (off != 0) skip = 1;
            }
        }
        if (skip) {
            if (RD16(AE_G_CURRENT_AREA) == 0x24) {
                uint32_t c = cur_monkey(r);
                uint32_t sound_handle = RD32(c + AE_MK_SFX_HANDLE);
                if (sound_handle != 0xFFFFFFFFu) {
                    AE_MK_HOOK(r, h, SoundLoopStopAndFree, 0x602D, sound_handle, 0, 0);
                    WR32(cur_monkey(r) + AE_MK_SFX_HANDLE, 0xFFFFFFFFu);
                }
            }
        } else {
            if (RD8(AE_G_DEBUG_FLAG_EB58) == 2) {
                uint32_t c = cur_monkey(r);
                AE_MK_HOOK(r, h, ColorFadeSet, RDS16(c + AE_MK_SCREEN_X), RDS16(c + AE_MK_SCREEN_Y), 0, 0);
                AE_MK_HOOK(r, h, DebugTextPrintfWhite, AE_G_MONKEY_ID_TEXT, RD8(cur_monkey(r) + AE_MK_ID), 0, 0);
            }
            uint32_t c = cur_monkey(r);
            uint32_t staggered_frame = RD32(AE_G_FRAME_COUNTER) + RD8(c + AE_MK_SLOT);
            uint32_t q = mulu_hi(staggered_frame, 0xAAAAAAABu) >> 1;
            if (staggered_frame == q * 2u + q) {
                ae_MonkeySelectLod(r, h);
                if (ae_ram_failed(r)) return;
            }
            uint32_t m = cur_monkey(r);
            uint32_t st = RD8(m + AE_MK_AI_STATE);
            if (st == 30 || st == 32) {
                uint32_t fc = RD32(AE_G_FRAME_COUNTER);
                const uint32_t animation_angles = AE_G_MONKEY_ANIM_ANGLES;
                WR16(animation_angles, 0);
                WR16(animation_angles + 2, 0);
                WR16(animation_angles + 4, 0);
                WR16(animation_angles + 8, 0);
                WR16(animation_angles + AE_MK_ANIM_ANGLE_SECOND + 4, 0);
                WR16(animation_angles + AE_MK_ANIM_ANGLE_THIRD, 0);
                WR16(animation_angles + AE_MK_ANIM_ANGLE_THIRD + 4, 0);
                int32_t phase_value = (int32_t)(0u - fc);
                WR16(animation_angles + AE_MK_ANIM_ANGLE_SECOND + 2, ((uint32_t)phase_value + ((uint32_t)phase_value << 2)) << 4);
                int32_t t = (int32_t)(fc << 1);
                t = (int32_t)((uint32_t)(t + (int32_t)fc) << 3);
                t = (int32_t)((uint32_t)(t + (int32_t)fc) << 4);
                WR16(animation_angles + AE_MK_ANIM_ANGLE_THIRD + 2, t);
            } else {
                AE_MK_HOOK(r, h, ActorAnimDecode, m, 0, 0, 0);
            }
            uint32_t shadow_monkey = cur_monkey(r);
            uint32_t size = RD16(shadow_monkey + AE_MK_SHADOW_SIZE);
            if (size != 0) {
                int32_t g = AE_MK_HOOK(r, h, GetGroundHeight, RD32(shadow_monkey + AE_MK_POS), RD32(shadow_monkey + AE_MK_POS + 4), RD32(shadow_monkey + AE_MK_POS + 8), 0);
                uint32_t reloaded_monkey = cur_monkey(r);
                uint32_t shadow_context = RD32(AE_G_SHADOW_CONTEXT);
                AE_MK_HOOK(r, h, DrawShadow, reloaded_monkey + AE_MK_POS, g, RD16(reloaded_monkey + AE_MK_SHADOW_SIZE), shadow_context);
            }
            ae_MonkeyDrawModel(r, h);
            if (ae_ram_failed(r)) return;
        }
        active_index += 1;
    } while (s16((uint32_t)active_index) < 4);
}
