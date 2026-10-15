/*
 * Monkey level2 routines, hand-ported and verified against the original game.
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
/* Negate with 32-bit wraparound. */
static int32_t neg32(int32_t v) { return (int32_t)(0u - (uint32_t)v); }
/* Add with 32-bit wraparound. */
static int32_t add32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
/* Subtract with 32-bit wraparound. */
static int32_t sub32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
/* Shift left with 32-bit wraparound. */
static int32_t shift_left32(int32_t v, uint32_t n) { return (int32_t)((uint32_t)v << n); }   /* Preserve 32-bit wraparound. */
/* Read the current monkey address. */
static uint32_t cur_monkey(AeRam *r) { return RD32(AE_G_CUR_MONKEY); }
/* Locate the information record for a monkey. */
static uint32_t info_of(AeRam *r, uint32_t m) {          /* MonkeyInfo of the monkey: table + id * 0x88 */
    return RD32(AE_G_MONKEY_INFO_TABLE) + RD8(m + AE_MK_ID) * AE_MONKEY_INFO_SIZE;
}
/* Locate a navigation node. */
static uint32_t node_of(AeRam *r, uint32_t index) {      /* nav node address: table + index * 0x18 */
    return index * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES);
}
/* MIPS div / divu: break (a trap) on a zero divisor and, signed, INT_MIN / -1 */
/* Divide signed values and report invalid divisors. */
static int32_t div_trap(AeRam *r, int32_t a, int32_t b) {
    if (b == 0 || (b == -1 && a == INT32_MIN)) {
        ae_ram_fail(r, "divide trap (div by zero / INT_MIN / -1; the original executes break)");
        return 0;
    }
    return a / b;
}
/* Divide unsigned values and report a zero divisor. */
static uint32_t divu_trap(AeRam *r, uint32_t a, uint32_t b) {
    if (b == 0) {
        ae_ram_fail(r, "divide trap (divu by zero; the original executes break)");
        return 0;
    }
    return a / b;
}

/* ================================================================================================
 * State changes
 * ============================================================================================== */

/* MonkeySetState @80033220 */
/* Change the AI state, choose its animation set and restart animation. */
void ae_MonkeySetState(AeRam *r, uint32_t state) {
    uint32_t monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_AI_STATE) == state) return;
    WR8(monkey + AE_MK_AI_STATE, state);
    uint32_t reloaded_monkey = cur_monkey(r);
    if (RD8(reloaded_monkey + AE_MK_AI_STATE) - 50u < 3u) {
        WR32(reloaded_monkey + AE_MK_ANIM_SET, RD32(AE_G_ANIM_SET2));
        WR32(reloaded_monkey + AE_MK_ANIM_TABLE, RD32(AE_G_AREA_DATA) + RD32(RD32(AE_G_AREA_HEADER) + AE_MK_AREA_ALT_ANIM_TABLE));
    } else {
        uint32_t set = RD32(AE_G_ANIM_SET);
        if (RD32(reloaded_monkey + AE_MK_ANIM_SET) != set) {
            WR32(reloaded_monkey + AE_MK_ANIM_SET, set);
            WR32(reloaded_monkey + AE_MK_ANIM_TABLE, RD32(AE_G_AREA_DATA) + RD32(RD32(AE_G_AREA_HEADER) + AE_MK_AREA_ANIM_TABLE));
            ae_MonkeyPickAnim(r);
        }
    }
    ae_MonkeyPickAnim(r);
}

/* MonkeyEnterState_32 @80035918 */
/* Enter the node-selected animation state and facing direction. */
void ae_MonkeyEnterState_32(AeRam *r, uint32_t node) {
    uint32_t sp = AE_STK(AE_STK_ENTER_32);
    WR8(cur_monkey(r) + AE_MK_AI_STATE, 50);
    uint32_t m = cur_monkey(r);
    ae_NavNodeGetLinkIdsEx(r, s16(node), m + AE_MK_ANIM_STATE_FRAME, sp + 24, m + AE_MK_ALT_ANIM_CODE, m + AE_MK_ALT_ANIM_FRAME);
    if (ae_ram_failed(r)) return;
    uint32_t monkey = cur_monkey(r);
    WR32(monkey + AE_MK_ANIM_SET, RD32(AE_G_ANIM_SET2));
    uint32_t sum = RD32(AE_G_AREA_DATA) + RD32(RD32(AE_G_AREA_HEADER) + AE_MK_AREA_ALT_ANIM_TABLE);
    WR8(monkey + AE_MK_FLAGS, 0);
    WR32(monkey + AE_MK_ANIM_TABLE, sum);
    ae_MonkeyFacingFromLinkCode(r, RDS16(sp + 24));
    if (ae_ram_failed(r)) return;
    ae_MonkeyPickAnim(r);
}

/* MonkeyEnterState_3D @800360D8 */
/* Enter accelerated movement and queue its voice. */
void ae_MonkeyEnterState_3D(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    WR16(monkey + AE_MK_PREV_NODE, RD16(monkey + AE_MK_NAV_NODE1));
    if (RD8(monkey + AE_MK_AI_STATE) == 61) return;
    WR16(monkey + AE_MK_COUNTER96, 0);
    WR8(monkey + AE_MK_AI_STATE, 61);
    ae_MonkeyPickAnim(r);
    ae_MonkeyQueueVoice(r, h, 43, 0);
}

/* MonkeyEnterState_3E @8003612C */
/* Enter decelerating movement with a fresh duration. */
void ae_MonkeyEnterState_3E(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_AI_STATE) == 62) return;
    uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);
    WR16(monkey + AE_MK_COUNTER96, 0);
    WR16(monkey + AE_MK_ANIM_LENGTH, 50);
    WR8(monkey + AE_MK_AI_STATE, 62);
    WR16(monkey + AE_MK_PREV_NODE, next_node);
    ae_MonkeyPickAnim(r);
}

/* MonkeyEnterState_3F @80036174 */
/* Enter the terminal movement state and restart animation. */
void ae_MonkeyEnterState_3F(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_AI_STATE) == 63) return;
    uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);
    WR8(monkey + AE_MK_AI_STATE, 63);
    WR16(monkey + AE_MK_PREV_NODE, next_node);
    ae_MonkeyPickAnim(r);
}

/* MonkeyEnterState_17 @8003641C */
/* Enter the special idle state and queue its delayed voice. */
void ae_MonkeyEnterState_17(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_AI_STATE) != 23) {
        WR8(monkey + AE_MK_AI_STATE, 23);
        ae_MonkeyPickAnim(r);
    }
    ae_MonkeyQueueVoice(r, h, 70, 30);
}

/* MonkeyEnterState_2A @80036460 */
/* Enter the hop state, save starting height and enable the body. */
void ae_MonkeyEnterState_2A(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_AI_STATE) == 42) return;
    WR8(monkey + AE_MK_AI_STATE, 42);
    ae_MonkeyPickAnim(r);
    WR8(cur_monkey(r) + AE_MK_HOP_COUNTER, 0);
    monkey = cur_monkey(r);
    WR16(monkey + AE_MK_HOP_START_Y, RD16(monkey + AE_MK_POS + 4));
    ae_MonkeyBodyOn(r, h);
}

/* ================================================================================================
 * Navigation / goal
 * ============================================================================================== */

/* MonkeyUpdateLinkSlope @80035370: +0x22E = 0 (steep or flat link not relevant), 1 / 2 (shallow link going up / down) */
/* Classify the height change along the current navigation link. */
void ae_MonkeyUpdateLinkSlope(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_LINK_SLOPE);
    ae_NavNodeGetPos(r, h, RD16(cur_monkey(r) + AE_MK_NAV_NODE0), sp + 16);
    if (ae_ram_failed(r)) return;
    ae_NavNodeGetPos(r, h, RD16(cur_monkey(r) + AE_MK_NAV_NODE1), sp + 32);
    if (ae_ram_failed(r)) return;
    int32_t distance_3d = ae_Distance3D(r, sp + 16, sp + 32);
    int32_t distance_xz = ae_DistanceXZ(r, sp + 16, sp + 32);
    if (distance_3d != 0) {
        int32_t q = div_trap(r, mul_lo(distance_xz, 100), distance_3d);
        if (ae_ram_failed(r)) return;
        if (q < 0) q = neg32(q);
        if (q < 11) {
            int32_t ya = (int32_t)RD32(sp + 20);
            int32_t yb = (int32_t)RD32(sp + 36);
            WR8(cur_monkey(r) + AE_MK_LINK_SLOPE, ya < yb ? 1 : 2);
            return;
        }
    }
    WR8(cur_monkey(r) + AE_MK_LINK_SLOPE, 0);
}

/* MonkeyUpdateGoalPos @80035474: goal position (+0xBC..) of the current node (or the target for nodes with bit 6) and the
   distances to it (+0x214 XZ, +0x218 3D) */
/* Resolve the goal position and refresh horizontal and full distances. */
void ae_MonkeyUpdateGoalPos(AeRam *r, const AeMonkeyHooks *h) {
    /* same block as MonkeyUpdateLinkSlope: both are callees of MonkeyUpdateGoal at the same stack depth, so the never-written
       position at +32 holds what UpdateLinkSlope left there (the position of the second node) */
    uint32_t sp = AE_STK(AE_STK_LINK_SLOPE);
    uint32_t monkey = cur_monkey(r);
    int run = 1;
    if (RD8(monkey + AE_MK_AI_STATE) == 30 && !(RD8(monkey + AE_MK_FLAGS + 3) & 1u)) run = 0;
    if (run) {
        uint32_t node_index = RD16(monkey + AE_MK_NAV_NODE0);
        if (RD32(node_index * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES)) & 0x40u) {
            uint32_t tgt = RD32(AE_G_MONKEY_TARGET);
            uint32_t target_x = RD32(tgt + AE_MK_POS);
            WR32(monkey + AE_MK_GOAL_POS + 4, 0);
            WR32(monkey + AE_MK_GOAL_POS, target_x);
            WR32(monkey + AE_MK_GOAL_POS + 8, RD32(tgt + AE_MK_POS + 8));
        } else {
            ae_NavNodeGetPos(r, h, node_index, sp + 16);
            if (ae_ram_failed(r)) return;
            uint32_t m = cur_monkey(r);
            uint32_t goal_x = RD32(sp + 16), goal_y = RD32(sp + 20), goal_z = RD32(sp + 24), goal_padding = RD32(sp + 28);
            WR32(m + AE_MK_GOAL_POS, goal_x);
            WR32(m + AE_MK_GOAL_POS + 4, goal_y);
            WR32(m + AE_MK_GOAL_POS + 8, goal_z);
            WR32(m + AE_MK_GOAL_POS_PAD, goal_padding);
            WR32(sp + 32, RD32(m + AE_MK_POS));
            uint32_t y = RD32(m + AE_MK_POS + 4);
            WR32(sp + 36, y);
            WR32(sp + 40, RD32(m + AE_MK_POS + 8));
            if (RD32(m + AE_MK_FLAGS2) & 2u) {
                if (RD8(m + AE_MK_AI_STATE) == 9 && RD8(m + AE_MK_LINK_SLOPE) == 1) WR32(sp + 36, y - 200u);
            }
        }
    }
    uint32_t m = cur_monkey(r);
    int32_t distance = ae_DistanceXZ(r, sp + 32, m + AE_MK_GOAL_POS);
    uint32_t reloaded_monkey = cur_monkey(r);
    WR32(reloaded_monkey + AE_MK_DIST_XZ_TO_GOAL, distance);
    distance = ae_Distance3D(r, sp + 32, reloaded_monkey + AE_MK_GOAL_POS);
    WR32(cur_monkey(r) + AE_MK_DIST_TO_GOAL, distance);
}

/* MonkeyPrepareJumpArc @800364BC: sets up the jump arc towards the next node: horizontal velocity (+0x13C / +0x144),
   vertical velocity (+0x140) and the duration (+0x128). */
/* Compute launch velocity and duration for the next navigation node. */
void ae_MonkeyPrepareJumpArc(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_JUMP_ARC);
    ae_NavNodeGetPos(r, h, RD16(cur_monkey(r) + AE_MK_NAV_NODE1), sp + 16);
    if (ae_ram_failed(r)) return;
    uint32_t monkey = cur_monkey(r);
    uint32_t node_component = RD16(sp + 16);
    uint32_t direction_component = RD16(monkey + AE_MK_GOAL_POS);
    WR16(monkey + AE_MK_MOVE_DIR + 2, 0);
    WR16(monkey + AE_MK_MOVE_DIR, direction_component - node_component);
    direction_component = RD16(monkey + AE_MK_GOAL_POS + 8);
    node_component = RD16(sp + 24);
    WR16(monkey + AE_MK_COUNTER96, 0);
    WR16(monkey + AE_MK_MOVE_DIR + 4, direction_component - node_component);
    ae_MonkeyPickAnim(r);
    uint32_t saved_monkey = cur_monkey(r);
    uint32_t current_node = RD16(saved_monkey + AE_MK_NAV_NODE0);
    uint32_t px = RD32(saved_monkey + AE_MK_POS), py = RD32(saved_monkey + AE_MK_POS + 4), pz = RD32(saved_monkey + AE_MK_POS + 8);
    WR32(saved_monkey + AE_MK_JUMP_START, px);
    WR32(saved_monkey + AE_MK_JUMP_START + 4, py);
    WR32(saved_monkey + AE_MK_JUMP_START + 8, pz);
    ae_NavNodeGetPos(r, h, current_node, sp + 16);
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    uint32_t node_y = RD32(sp + 20);
    direction_component = RD16(sp + 16);
    uint32_t position_component = RD16(monkey + AE_MK_POS);
    uint32_t position_y = RD32(monkey + AE_MK_POS + 4);
    WR16(monkey + AE_MK_WISH_DIR + 2, 0);
    WR16(monkey + AE_MK_WISH_DIR, direction_component - position_component);
    direction_component = RD16(sp + 24);
    position_component = RD16(monkey + AE_MK_POS + 8);
    int32_t height_delta = sub32((int32_t)position_y, (int32_t)node_y);
    direction_component = direction_component - position_component;
    int32_t dxz = ae_DistanceXZ(r, sp + 16, monkey + AE_MK_POS);
    WR16(monkey + AE_MK_WISH_DIR + 4, direction_component);
    uint32_t horizontal_distance = (uint32_t)dxz & 0xFFFFu;
    int32_t launch_angle;
    if (height_delta < 0 && neg32(height_delta) >= 100) {
        /* flight_monkey drop of at least 100 */
        if (horizontal_distance == 0) {
            launch_angle = 1024;
        } else {
            int32_t ang = ae_Atan2Angle4096(r, neg32(height_delta), (int32_t)horizontal_distance);
            launch_angle = ang;
            int32_t t = mul_lo(launch_angle, 70);
            launch_angle = sub32(sra32(mul_hi(t, 0x51EB851F), 5), sra32(t, 31));
            if (launch_angle < 341) launch_angle = 341;
        }
    } else if (horizontal_distance == 0) {
        launch_angle = 1024;
    } else {
        int32_t flight_monkey = height_delta;
        if ((height_delta < 0 ? neg32(height_delta) : height_delta) < 100) {
            height_delta = 0;
            flight_monkey = height_delta;
        }
        int32_t ang = ae_Atan2Angle4096(r, flight_monkey, (int32_t)horizontal_distance);
        int32_t t = add32(ang, 1024);
        t = add32(t, (int32_t)((uint32_t)t >> 31));
        launch_angle = sra32(t, 1);
    }
    if (ae_ram_failed(r)) return;
    if (!(launch_angle < 1002)) {
        int32_t vv = shift_left32(add32(height_delta, 2700), 12);
        vv = neg32(vv);
        int32_t hi = mul_hi(vv, (int32_t)0x88888889);
        uint32_t flight_monkey = cur_monkey(r);
        WR16(flight_monkey + AE_MK_ANIM_LENGTH, 30);
        WR32(flight_monkey + AE_MK_JUMP_VEL + 8, 0);
        WR32(flight_monkey + AE_MK_JUMP_VEL, 0);
        int32_t q = sub32(sra32(add32(hi, vv), 4), sra32(vv, 31));
        WR32(flight_monkey + AE_MK_JUMP_VEL + 4, q);
        ae_MonkeyFootDust(r, h);
        return;
    }
    int32_t launch_sine = ae_sdk_rsin(r, launch_angle);
    int32_t launch_cosine = ae_sdk_rcos(r, launch_angle);
    int32_t d = sub32(mul_lo(launch_cosine, height_delta), mul_lo(launch_sine, (int32_t)horizontal_distance));
    int32_t trajectory_denominator = d < 0 ? neg32(d) : d;
    if (trajectory_denominator == 0) {
        uint32_t m = cur_monkey(r);
        WR32(m + AE_MK_JUMP_VEL + 8, 0);
        WR32(m + AE_MK_JUMP_VEL + 4, 0);
        WR32(m + AE_MK_JUMP_VEL, 0);
        ae_MonkeyFootDust(r, h);
        return;
    }
    /* launch_cosine * 1080000 */
    int32_t scaled_cosine = mul_lo(launch_cosine, 1080000);
    int32_t scaled_denominator = mul_lo(trajectory_denominator, 100);
    uint32_t q1 = divu_trap(r, (uint32_t)scaled_cosine, (uint32_t)scaled_denominator);
    if (ae_ram_failed(r)) return;
    int32_t root = ae_sdk_SquareRoot0(r, (int32_t)q1);
    if (ae_ram_failed(r)) return;
    int32_t pv = mul_lo(root, (int32_t)horizontal_distance);
    int32_t hi = mul_hi(pv, (int32_t)0x88888889);
    int32_t horizontal_speed = sub32(sra32(add32(hi, pv), 5), sra32(pv, 31));          /* pv / 60 */
    int32_t t = mul_lo(horizontal_speed, 120);
    int32_t sra31 = sra32(t, 31);
    uint32_t m = cur_monkey(r);
    int32_t direction_x = RDS16(m + AE_MK_WISH_DIR);
    int32_t direction_z = RDS16(m + AE_MK_WISH_DIR + 4);
    WR32(m + AE_MK_JUMP_VEL + 4, 0);
    WR32(m + AE_MK_JUMP_VEL, direction_x);
    WR32(m + AE_MK_JUMP_VEL + 8, direction_z);
    int32_t hi2 = mul_hi(t, 0x51EB851F);
    horizontal_speed = sub32(sra32(hi2, 5), sra31);
    ae_sdk_VectorNormal(r, m + AE_MK_JUMP_VEL, m + AE_MK_JUMP_VEL);
    if (ae_ram_failed(r)) return;
    int32_t sq = mul_lo(launch_sine, launch_sine);
    int32_t cosine_divisor = mul_lo(launch_cosine, 100);
    int32_t distance_times_300 = mul_lo((int32_t)horizontal_distance, 300);
    int32_t g = div_trap(r, sq, cosine_divisor);
    if (ae_ram_failed(r)) return;
    int32_t distance_product = mul_lo((int32_t)horizontal_distance, distance_times_300);
    int32_t g2 = div_trap(r, distance_product, trajectory_denominator);
    if (ae_ram_failed(r)) return;
    uint32_t flight_monkey = cur_monkey(r);
    int32_t x2 = mul_lo((int32_t)RD32(flight_monkey + AE_MK_JUMP_VEL), horizontal_speed);
    int32_t z2 = mul_lo((int32_t)RD32(flight_monkey + AE_MK_JUMP_VEL + 8), horizontal_speed);
    int32_t vertical_speed_squared = mul_lo(g, g2);
    WR32(flight_monkey + AE_MK_JUMP_VEL, x2);
    WR32(flight_monkey + AE_MK_JUMP_VEL + 8, z2);
    root = ae_sdk_SquareRoot0(r, vertical_speed_squared);
    if (ae_ram_failed(r)) return;
    int32_t d300 = mul_lo(launch_cosine, 300);
    int32_t q2 = div_trap(r, scaled_denominator, d300);
    if (ae_ram_failed(r)) return;
    int32_t vy = neg32(shift_left32(root, 12));
    flight_monkey = cur_monkey(r);
    WR32(flight_monkey + AE_MK_JUMP_VEL + 4, vy);
    root = ae_sdk_SquareRoot0(r, q2);
    if (ae_ram_failed(r)) return;
    int32_t u = mul_lo(root, 110);
    int32_t sg = sra32(u, 31);
    flight_monkey = cur_monkey(r);
    int32_t hi3 = mul_hi(u, 0x51EB851F);
    WR16(flight_monkey + AE_MK_ANIM_LENGTH, sub32(sra32(hi3, 5), sg));
    ae_MonkeyFootDust(r, h);
}

/* ================================================================================================
 * Attacks
 * ============================================================================================== */

/* MonkeyTryAttackKind @8003710C: tries to start attack `kind` (0..7; table of {count multiplier, state, flags} pairs at
   0x800AEBB4): range check against the MonkeyInfo record, random chance, then enters the attack state.  1 = started. */
/* Check range and probability before entering the requested attack. */
int32_t ae_MonkeyTryAttackKind(AeRam *r, const AeMonkeyHooks *h, int32_t arg) {
    const uint32_t ATTACK_TABLE = AE_G_MONKEY_ATTACK_TABLE;
    uint32_t kind_bits = (uint32_t)arg;
    if (s16(kind_bits) == 1) {
        if (RD16(cur_monkey(r) + AE_MK_TIMER_174) != 0) return 0;
    }
    uint32_t monkey = cur_monkey(r);
    uint32_t info = info_of(r, monkey);
    int32_t kind = s16(kind_bits);
    uint32_t kind_offset = (uint32_t)kind << 3;
    uint32_t entry_info = info + kind_offset;
    if (RD16(entry_info + AE_MK_INFO_ATTACK_COUNT) == 0) return 0;
    int32_t dist = (int32_t)RD32(monkey + AE_MK_DIST_TO_TARGET);
    if (dist < RDS16(entry_info + AE_MK_INFO_ATTACK_MIN_RANGE)) return 0;
    if (RDS16(entry_info + AE_MK_INFO_ATTACK_MAX_RANGE) < dist) return 0;
    uint32_t rnd = ae_MonkeyRand(r) & 0xFFFFu;
    uint32_t q = mulu_hi(rnd, 0x51EB851Fu) >> 5;
    uint32_t t = ((q << 1) + q) << 3;
    t = (t + q) << 2;
    uint32_t pct = (rnd - t) & 0xFFFFu;
    int32_t chance = ae_MonkeyAttackChance(r, kind);
    if (ae_ram_failed(r)) return 0;
    if (!((int32_t)pct < chance)) return 0;
    monkey = cur_monkey(r);
    uint32_t attack_entry = kind_offset + ATTACK_TABLE;
    uint32_t entry_info_after_roll = info_of(r, monkey) + kind_offset;
    uint32_t b = RD8(attack_entry);
    uint32_t cnt = RD16(entry_info_after_roll + AE_MK_INFO_ATTACK_COUNT);
    WR8(monkey + AE_MK_PATROL_COUNTER, b * cnt);
    if (kind == 1) WR16(cur_monkey(r) + AE_MK_TIMER_174, 90);
    if (ae_MonkeyIsState1E_to_20(r)) return 1;
    WR8(cur_monkey(r) + AE_MK_AI_STATE, RD8(attack_entry + 1));
    monkey = cur_monkey(r);
    if (!((RD32(AE_G_MONKEY_STATE_TABLE + RD8(monkey + AE_MK_AI_STATE) * 8u + 4u) >> 2) & 1u)) {
        WR16(monkey + AE_MK_COUNTER96, 0);
        WR16(monkey + AE_MK_ANIM_LENGTH, 12);
    }
    ae_MonkeyPickAnim(r);
    if (kind == 0 || kind == 5) {
        int32_t idx = ae_MonkeyGetAnimIndex(r);
        monkey = cur_monkey(r);
        uint32_t tbl = RD32(monkey + AE_MK_ANIM_TABLE);
        uint32_t f = RD16(tbl + ((uint32_t)idx << 3)) + 4u;
        WR16(monkey + AE_MK_ANIM_FRAME, f);
        idx = ae_MonkeyGetAnimIndex(r);
        monkey = cur_monkey(r);
        tbl = RD32(monkey + AE_MK_ANIM_TABLE);
        WR16(monkey + AE_MK_AIM_FRAME, RD16(tbl + ((uint32_t)idx << 3)));
    }
    if (s16(kind_bits) != 7) return 1;
    int32_t voice = ae_MonkeyGetCallVoice(r);
    ae_MonkeyQueueVoice(r, h, voice, 0);
    return 1;
}

/* MonkeyNavLinkAllowed @80037E8C: may the monkey use the link from -> to?  Looks up the link's extra record (flags +
   a value) and tests the flags: 0x40 / 0x80 gimmick open / closed, 0x400 distance limit, 0x1000 only for one monkey id,
   0x200000 / 0x400000 only when alerted / not alerted, 0x40000 object animation, node flag 0x100 only towards the goal. */
/* Test link flags, target awareness and external gimmick conditions. */
int32_t ae_MonkeyNavLinkAllowed(AeRam *r, const AeMonkeyHooks *h, int32_t from, int32_t to) {
    uint32_t sp = AE_STK(AE_STK_NAV_LINK_ALLOWED);
    uint32_t to_node = (uint32_t)to;
    uint32_t link_flags = (uint32_t)ae_NavLinkLookupExtra(r, s16((uint32_t)from), s16(to_node), sp + 48);
    if (ae_ram_failed(r)) return 0;
    if (link_flags & 0x40u) {
        if (ae_GimmickAllowsMonkeyPassage(r, h, RD32(sp + 48)) == 0) return 0;
        if (ae_ram_failed(r)) return 0;
    }
    if (link_flags & 0x80u) {
        if (ae_GimmickAllowsMonkeyPassage(r, h, RD32(sp + 48)) != 0) return 0;
        if (ae_ram_failed(r)) return 0;
    }
    if (link_flags & 0x400u) {
        uint32_t m = cur_monkey(r);
        WR32(sp + 32, RD32(m + AE_MK_POS));
        WR32(sp + 36, RD32(m + AE_MK_POS + 4));
        WR32(sp + 40, RD32(m + AE_MK_POS + 8));
        ae_NavNodeGetPos(r, h, to_node & 0xFFFFu, sp + 16);
        if (ae_ram_failed(r)) return 0;
        int32_t d = ae_Distance3D(r, sp + 32, sp + 16);
        if ((int32_t)RD32(sp + 48) < d) return 0;
    }
    if (link_flags & 0x1000u) {
        if (RD8(cur_monkey(r) + AE_MK_ID) != RD32(sp + 48)) return 0;
    }
    if (link_flags & 0x200000u) {
        if (RD8(cur_monkey(r) + AE_MK_ALERTED) != 1u) return 0;
    }
    if (link_flags & 0x400000u) {
        if (RD8(cur_monkey(r) + AE_MK_ALERTED) == 1u) return 0;
    }
    if (link_flags & 0x40000u) {
        int32_t anim = AE_MK_HOOK(r, h, ObjSlotGetAnimIndex, RDS16(sp + 48), 0, 0, 0);
        if (anim == 1) return 0;
    }
    if (RD32(node_of(r, to_node)) & 0x100u) {
        if (to_node != (uint32_t)RDS16(cur_monkey(r) + AE_MK_GOAL_NODE)) return 0;
    }
    return 1;
}

/* MonkeyDrawAlertIcon @80039C38: the alert / question mark icon above the monkey (hook AlertIconDraw) */
/* Place and draw the current alert icon and tick its transition timer. */
void ae_MonkeyDrawAlertIcon(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_ALERT_ICON);
    int32_t kind = ae_MonkeyAlertIconKind(r);
    uint32_t monkey = cur_monkey(r);
    if ((int32_t)RD32(monkey + AE_MK_ALERT_ICON_KIND) != kind) WR16(monkey + AE_MK_SHOT_COOLDOWN, 10);
    WR32(cur_monkey(r) + AE_MK_ALERT_ICON_KIND, kind);
    uint32_t icon;
    if (kind == 448) icon = 1;
    else if (kind < 449) icon = 0;
    else if (kind == 896) icon = 2;
    else icon = 0;
    uint32_t reloaded_monkey = cur_monkey(r);
    uint32_t show_transition = 1;
    if (!(RDS16(reloaded_monkey + AE_MK_SHOT_COOLDOWN) > 0)) {
        if (!((RD32(AE_G_MONKEY_STATE_TABLE + RD8(reloaded_monkey + AE_MK_AI_STATE) * 8u + 4u) >> 3) & 1u)) {
            if (!ae_MonkeyIsAttackState(r)) {
                if (!ae_MonkeyIsIdleActivityState(r)) show_transition = 0;
            }
        }
    }
    reloaded_monkey = cur_monkey(r);
    uint32_t lod = RD8(reloaded_monkey + AE_MK_MODEL_LOD);
    if (lod < 3u) {
        WR16(sp + 16, 0);
        WR16(sp + 18, 100);
        WR16(sp + 20, 0);
        ae_sdk_ApplyMatrixSV(r, AE_G_MONKEY_ICON_MATRIX, sp + 16, sp + 16);
        if (ae_ram_failed(r)) return;
        uint32_t vx = RD16(sp + 16) + RD16(AE_G_MONKEY_ICON_MATRIX + AE_MK_MATRIX_TRANS_X);
        uint32_t ay = RD16(sp + 18) + 220u;
        WR16(sp + 16, vx);
        uint32_t vy = RD16(AE_G_MONKEY_ICON_MATRIX + AE_MK_MATRIX_TRANS_Y) + ay;
        uint32_t vz = RD16(sp + 20) + RD16(AE_G_MONKEY_ICON_MATRIX + AE_MK_MATRIX_TRANS_Z);
        WR16(sp + 18, vy);
        WR16(sp + 20, vz);
    } else if (lod == 3u) {
        WR16(sp + 16, RD16(reloaded_monkey + AE_MK_POS));
        WR16(sp + 18, RD16(reloaded_monkey + AE_MK_POS + 4) + 10u);
        WR16(sp + 20, RD16(reloaded_monkey + AE_MK_POS + 8));
    } else {
        uint32_t m = cur_monkey(r);
        WR16(sp + 16, RD16(m + AE_MK_POS));
        WR16(sp + 18, RD16(m + AE_MK_POS + 4));
        WR16(sp + 20, RD16(m + AE_MK_POS + 8));
    }
    AE_MK_HOOK(r, h, AlertIconDraw, sp + 16, icon, show_transition, 0);
    uint32_t m = cur_monkey(r);
    WR16(m + AE_MK_SHOT_COOLDOWN, RD16(m + AE_MK_SHOT_COOLDOWN) - 1u);
}

/* ================================================================================================
 * Link scoring / facing / small wrappers
 * ============================================================================================== */

/* MonkeyScoreLinksByTarget @8003AC54: for every link of the node, DotXZ (direction to the target, direction to the
   far node of the link) -> s16 table at +0x176 of the monkey */
/* Score outgoing link directions against the direction to the target. */
void ae_MonkeyScoreLinksByTarget(AeRam *r, const AeMonkeyHooks *h, int32_t node) {
    uint32_t sp = AE_STK(AE_STK_SCORE_LINKS);
    uint32_t target = RD32(AE_G_MONKEY_TARGET);
    uint32_t monkey = cur_monkey(r);
    int32_t node_index = s16((uint32_t)node);
    WR16(sp + 32, RD16(target + AE_MK_POS) - RD16(monkey + AE_MK_POS));
    WR16(sp + 34, RD16(target + AE_MK_POS + 4) - RD16(monkey + AE_MK_POS + 4));
    WR16(sp + 36, RD16(target + AE_MK_POS + 8) - RD16(monkey + AE_MK_POS + 8));
    uint32_t node_offset = (uint32_t)node_index * AE_NAV_NODE_SIZE;
    if (RD8(node_offset + RD32(AE_G_NAV_NODES) + AE_NAV_ACTIVE_LINK_COUNT) == 0) return;
    int32_t link_index = 0;
    do {
        int32_t link = ae_NavNodeGetLink(r, node_index, s16((uint32_t)link_index));
        if (ae_ram_failed(r)) return;
        ae_NavNodeGetPos(r, h, (uint32_t)link & 0xFFFFu, sp + 16);
        if (ae_ram_failed(r)) return;
        uint32_t m = cur_monkey(r);
        WR16(sp + 40, RD16(sp + 16) - RD16(m + AE_MK_POS));
        WR16(sp + 42, RD16(sp + 20) - RD16(m + AE_MK_POS + 4));
        WR16(sp + 44, RD16(sp + 24) - RD16(m + AE_MK_POS + 8));
        int32_t dot = ae_DotXZ_SV(r, sp + 32, sp + 40);
        if (ae_ram_failed(r)) return;
        m = cur_monkey(r);
        WR16(m + AE_MK_LINK_SCORES + (uint32_t)link_index * 2u, dot);
        link_index += 1;
    } while (link_index < (int32_t)RD8(node_offset + RD32(AE_G_NAV_NODES) + AE_NAV_ACTIVE_LINK_COUNT));
}

/* MonkeyNodeTargetCosines @8003B534: DotXZ of (direction to the target) with the direction to the current node (-> *out0)
   and to the next node (-> *out1) */
/* Measure target alignment with the current and next node directions. */
void ae_MonkeyNodeTargetCosines(AeRam *r, const AeMonkeyHooks *h, uint32_t out0, uint32_t out1) {
    uint32_t sp = AE_STK(AE_STK_NODE_COSINES);
    uint32_t target = RD32(AE_G_MONKEY_TARGET);
    uint32_t monkey = cur_monkey(r);
    WR16(sp + 24, RD16(target + AE_MK_POS) - RD16(monkey + AE_MK_POS));
    WR16(sp + 26, RD16(target + AE_MK_POS + 4) - RD16(monkey + AE_MK_POS + 4));
    WR16(sp + 28, RD16(target + AE_MK_POS + 8) - RD16(monkey + AE_MK_POS + 8));
    ae_NavNodeGetPos(r, h, RD16(monkey + AE_MK_NAV_NODE0), sp + 32);
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    WR16(sp + 16, RD16(sp + 32) - RD16(monkey + AE_MK_POS));
    WR16(sp + 18, RD16(sp + 36) - RD16(monkey + AE_MK_POS + 4));
    WR16(sp + 20, RD16(sp + 40) - RD16(monkey + AE_MK_POS + 8));
    int32_t d = ae_DotXZ_SV(r, sp + 24, sp + 16);
    if (ae_ram_failed(r)) return;
    WR16(out0, d);
    ae_NavNodeGetPos(r, h, RD16(cur_monkey(r) + AE_MK_NAV_NODE1), sp + 32);
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    WR16(sp + 16, RD16(sp + 32) - RD16(monkey + AE_MK_POS));
    WR16(sp + 18, RD16(sp + 36) - RD16(monkey + AE_MK_POS + 4));
    WR16(sp + 20, RD16(sp + 40) - RD16(monkey + AE_MK_POS + 8));
    d = ae_DotXZ_SV(r, sp + 24, sp + 16);
    if (ae_ram_failed(r)) return;
    WR16(out1, d);
}

/* MonkeyFireProjectileByKind @8003CD24: kind 1 -> projectile 6, kinds 0 and 5 -> aimed throw, others nothing */
/* Dispatch the firing operation for the current attack kind. */
void ae_MonkeyFireProjectileByKind(AeRam *r, const AeMonkeyHooks *h, int32_t kind) {
    if (kind == 1) {
        ae_MonkeyFireProjectile6(r, h);
        return;
    }
    if (kind < 2) {
        if (kind != 0) return;
    } else if (kind != 5) {
        return;
    }
    ae_MonkeyAimAtTarget(r, h, s16((uint32_t)kind));
}

/* MonkeyGetWaypointPos @80040CFC: NavGroundHeightBetween (current node, next node or the saved position) */
/* Interpolate ground height using a node or saved jump position. */
int32_t ae_MonkeyGetWaypointPos(AeRam *r, const AeMonkeyHooks *h, int32_t which) {
    uint32_t sp = AE_STK(AE_STK_WAYPOINT);
    ae_NavNodeGetPos(r, h, RD16(cur_monkey(r) + AE_MK_NAV_NODE0), sp + 16);
    if (ae_ram_failed(r)) return 0;
    if (((uint32_t)which << 16) != 0) {
        uint32_t monkey = cur_monkey(r);
        WR32(sp + 32, RD32(monkey + AE_MK_JUMP_START));
        WR32(sp + 36, RD32(monkey + AE_MK_JUMP_START + 4));
        WR32(sp + 40, RD32(monkey + AE_MK_JUMP_START + 8));
    } else {
        ae_NavNodeGetPos(r, h, RD16(cur_monkey(r) + AE_MK_NAV_NODE1), sp + 32);
        if (ae_ram_failed(r)) return 0;
    }
    return ae_NavGroundHeightBetween(r, sp + 16, sp + 32);
}

/* MonkeyUpdateFacing @8003C204: turns the move direction (+0x60) towards the wished direction and derives the turn
   animation parameters.  the turn step is (22 / 34 / 170 depending on the state, through the jump table at 0x800A49B8). */
/* Prepare the facing rotation and turn animation while preserving the old direction. */
void ae_MonkeyUpdateFacing(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_UPDATE_FACING);
    int32_t turn_step;
    uint32_t idx = RD8(cur_monkey(r) + AE_MK_AI_STATE) - 2u;
    if (idx < 56u) {
        uint32_t target = RD32(AE_G_MONKEY_TURN_TABLE + idx * 4u);
        if (target == 0x8003C250u) turn_step = 34;
        else if (target == 0x8003C258u) turn_step = 170;
        else if (target == 0x8003C260u) turn_step = 22;
        else {
            ae_ram_fail(r, "MonkeyUpdateFacing: jump table entry %08X is not one of the original code blocks", (unsigned)target);
            return;
        }
    } else {
        turn_step = 170;
    }
    uint32_t m = cur_monkey(r);
    {
        uint32_t b[8];
        for (uint32_t i = 0; i < 8; ++i) b[i] = RD8(m + AE_MK_MOVE_DIR + i);
        for (uint32_t i = 0; i < 8; ++i) WR8(sp + 24 + i, b[i]);
    }
    ae_MonkeyApplyMoveDir(r);
    if (ae_ram_failed(r)) return;
    int32_t is0c = ae_MonkeyIsState0C_or_24(r);
    if (is0c) {
        uint32_t monkey = cur_monkey(r);
        WR32(monkey + AE_MK_LINK_FROM, RDS16(sp + 24));
        WR32(monkey + AE_MK_LINK_FROM + 4, RDS16(sp + 26));
        WR32(monkey + AE_MK_LINK_FROM + 8, RDS16(sp + 28));
        ae_NavNodeGetPos(r, h, RD16(monkey + AE_MK_NAV_NODE0), sp + 32);
        if (ae_ram_failed(r)) return;
        ae_NavNodeGetPos(r, h, RD16(cur_monkey(r) + AE_MK_NAV_NODE1), sp + 48);
        if (ae_ram_failed(r)) return;
        uint32_t turn_monkey = cur_monkey(r);
        WR32(turn_monkey + AE_MK_LINK_TO, sub32((int32_t)RD32(sp + 32), (int32_t)RD32(sp + 48)));
        WR32(turn_monkey + AE_MK_LINK_TO + 4, sub32((int32_t)RD32(sp + 36), (int32_t)RD32(sp + 52)));
        WR32(turn_monkey + AE_MK_LINK_TO + 8, sub32((int32_t)RD32(sp + 40), (int32_t)RD32(sp + 56)));
        int32_t dot = ae_DotXZ_V(r, turn_monkey + AE_MK_LINK_FROM, turn_monkey + AE_MK_LINK_TO);
        if (ae_ram_failed(r)) return;
        if (s16((uint32_t)dot) < -3686) {
            uint32_t reloaded_monkey = cur_monkey(r);
            int32_t v = (int32_t)RD32(reloaded_monkey + AE_MK_LINK_TO);
            int32_t x11 = sub32(shift_left32(add32(shift_left32(v, 1), v), 2), v);                  /* 11 * v */
            int32_t turn_delta = sub32(sra32(mul_hi(x11, 0x66666667), 2), sra32(x11, 31));
            int32_t x9 = add32(shift_left32(turn_delta, 3), turn_delta);                                  /* 9 * turn_delta */
            WR32(reloaded_monkey + AE_MK_LINK_TO, turn_delta);
            WR32(reloaded_monkey + AE_MK_LINK_TO + 8, sub32(sra32(mul_hi(x9, 0x66666667), 2), sra32(x9, 31)));
        }
        uint32_t turn_target = cur_monkey(r) + AE_MK_LINK_TO;
        ae_sdk_VectorNormal(r, turn_target, turn_target);
        if (ae_ram_failed(r)) return;
        uint32_t reloaded_monkey = cur_monkey(r);
        int32_t ang = ae_AngleBetweenXZ(r, reloaded_monkey + AE_MK_LINK_FROM, reloaded_monkey + AE_MK_LINK_TO);
        if (ae_ram_failed(r)) return;
        int32_t absolute_angle = ang < 0 ? neg32(ang) : ang;
        int32_t turn_frames = sub32(sra32(mul_hi(absolute_angle, 0x2E8BA2E9), 2), sra32(absolute_angle, 31));
        turn_frames = sub32(turn_frames, 1);
        monkey = cur_monkey(r);
        WR16(monkey + AE_MK_LINK_FRAMES, turn_frames < 0 ? 0u : (uint32_t)turn_frames);
        turn_frames = sub32(sub32(sra32(mul_hi(absolute_angle, 0x2E8BA2E9), 2), sra32(absolute_angle, 31)), 1);
        reloaded_monkey = cur_monkey(r);
        WR16(reloaded_monkey + AE_MK_LINK_FRAME, turn_frames < 0 ? 0u : (uint32_t)turn_frames);
    } else {
        int32_t first = ae_Atan2Angle4096(r, RDS16(sp + 28), RDS16(sp + 24));
        uint32_t monkey = cur_monkey(r);
        int32_t direction_z = RDS16(monkey + AE_MK_MOVE_DIR + 4), direction_x = RDS16(monkey + AE_MK_MOVE_DIR);
        int32_t previous_angle = neg32(first);
        int32_t second = ae_Atan2Angle4096(r, direction_z, direction_x);
        int32_t turn_delta = sub32(neg32(second), previous_angle);
        if (turn_delta < 2049) {
            if (turn_delta < -2048) turn_delta = add32(turn_delta, 4096);
        } else {
            turn_delta = add32(turn_delta, -4096);
        }
        int32_t q = div_trap(r, turn_delta, turn_step);
        if (ae_ram_failed(r)) return;
        int32_t aq = q < 0 ? neg32(q) : q;
        monkey = cur_monkey(r);
        WR16(monkey + AE_MK_ACTION_COOLDOWN, aq);
        if (turn_delta > 0) {
            WR16(sp + 16, 0);
            WR16(sp + 18, turn_step);
        } else {
            WR16(sp + 16, 0);
            WR16(sp + 18, neg32(turn_step));
        }
        WR16(sp + 20, 0);
        ae_sdk_RotMatrixYXZ(r, sp + 16, cur_monkey(r) + AE_MK_ROT_MATRIX);
        if (ae_ram_failed(r)) return;
    }
    {
        uint32_t saved_monkey = cur_monkey(r);
        uint32_t b[8];
        for (uint32_t i = 0; i < 8; ++i) b[i] = RD8(sp + 24 + i);
        for (uint32_t i = 0; i < 8; ++i) WR8(saved_monkey + AE_MK_MOVE_DIR + i, b[i]);
    }
}

/* MonkeyUpdateSight @800405C4: updates the "sees the target" bits of flags (+0x248: bit 28 = in sight, bit 29 =
   sight and a second condition) and returns the sight result.  arg = the sight pattern 0..4 (jump table at 0x800A4A98):
   which window of the link position counter (+0x21C) allows a line-of-sight test. */
/* Apply range and angular windows before updating cached visibility flags. */
int32_t ae_MonkeyUpdateSight(AeRam *r, const AeMonkeyHooks *h, uint32_t arg) {
    int32_t boss = ae_AreaIsBossArena(r);
    if (boss) {
        uint32_t boss_monkey = cur_monkey(r);
        WR32(boss_monkey + AE_MK_FLAGS, RD32(boss_monkey + AE_MK_FLAGS) | 0x10000000u);
        return 1;
    }
    uint32_t target_motion = 1;
    if (RD8(AE_G_NAV_AREA_FLAG) == 0) target_motion = RD32(RD32(AE_G_MONKEY_TARGET) + AE_MK_TARGET_STATE_FLAGS) & 0xFu;
    uint32_t monkey = cur_monkey(r);
    uint32_t info = info_of(r, monkey);
    int32_t range1 = RDS16(info + target_motion * 2u + AE_MK_INFO_SIGHT_RANGES);
    int32_t dist = (int32_t)RD32(monkey + AE_MK_DIST_TO_TARGET);
    if (!(dist < range1)) {
        int32_t range2 = RDS16(info + AE_MK_INFO_SIGHT_RANGE);
        if (!(dist < range2)) {
            WR32(monkey + AE_MK_FLAGS, (RD32(monkey + AE_MK_FLAGS) & 0xDFFFFFFFu) & 0xEFFFFFFFu);
            return 0;
        }
    }
    uint32_t state = RD8(monkey + AE_MK_AI_STATE);
    if (state == 3 || state == 8) {
        WR32(monkey + AE_MK_FLAGS, (RD32(monkey + AE_MK_FLAGS) | 0x20000000u) | 0x10000000u);
        return 1;
    }
    if (target_motion == 7) {
        WR32(monkey + AE_MK_FLAGS, RD32(monkey + AE_MK_FLAGS) | 0x10000000u);
        return 1;
    }
    uint32_t pos = RD32(monkey + AE_MK_LINK_ANGLE);
    uint32_t sight_result = 0;
    uint32_t flags;
    if (arg < 5u) {
        uint32_t target = RD32(AE_G_MONKEY_SIGHT_TABLE + arg * 4u);
        if (target == 0x800406F8u) {
            sight_result = 1;
            flags = 1;
        } else if (target == 0x80040704u) {
            sight_result = 1;
            if (pos < 569u) flags = 1;
            else flags = (pos - 3528u) < 569u;
        } else if (target == 0x8004072Cu) {
            flags = (pos - 456u) < 1137u;
        } else if (target == 0x80040734u) {
            flags = (pos - 1480u) < 1137u;
        } else if (target == 0x8004073Cu) {
            flags = (pos - 2504u) < 1137u;
        } else {
            ae_ram_fail(r, "MonkeyUpdateSight: jump table entry %08X is not one of the original code blocks", (unsigned)target);
            return 0;
        }
    } else {
        sight_result = 1;
        flags = 1;
    }
    if (flags == 0) {
        monkey = cur_monkey(r);
        WR32(monkey + AE_MK_FLAGS, (RD32(monkey + AE_MK_FLAGS) & 0xDFFFFFFFu) & 0xEFFFFFFFu);
        return 0;
    }
    int32_t los = ae_MonkeyLineOfSight(r, h, (int32_t)sight_result);
    if (ae_ram_failed(r)) return 0;
    uint32_t bit = (uint32_t)los & 1u;
    uint32_t m = cur_monkey(r);
    uint32_t f = RD32(m + AE_MK_FLAGS) & 0xDFFFFFFFu;
    f |= bit << 29;
    f &= 0xEFFFFFFFu;
    f |= bit << 28;
    WR32(m + AE_MK_FLAGS, f);
    return (int32_t)bit;
}
