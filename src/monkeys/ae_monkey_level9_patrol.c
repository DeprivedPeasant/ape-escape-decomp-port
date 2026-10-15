/*
 * Monkey code, batch 5b (call-graph level 9): MonkeyAi_Patrol, ported from the original instructions (see ae_monkey.h and
 * notes/port/monkeys-port.md).  Where the Ghidra pseudocode and the listing differ, the listing was followed.  The parts of the
 * function that are self-contained blocks are static helpers (patrol_*); MonkeyAi_Patrol itself reads top to bottom as the
 * original does.
 */
#include "ae_monkey_inl.h"

#define STATE_FLAGS(st) RD32(AE_G_MONKEY_STATE_TABLE + (uint32_t)(st) * 8u + 4u)
#define STATE_FLAG_ATTACK 0x08u           /* state table flags: the attacking states (0x1B 0x1C 0x21 0x22 0x37 0x38 ...) */
#define STATE_FLAG_CAN_NOTICE 0x40u       /* state table flags: the monkey may notice the target in this state */
#define AE_G_EXIT_CODE_447C 0x800F447Cu   /* u8: the stage exit code (3 = the boss fight is over) */
#define AE_G_FLAG_EB5C 0x800AEB5Cu        /* u32: cleared by MonkeyAi_Patrol after the boss decision */

/* AI states MonkeyAi_Patrol handles by name (see notes/17-monkey-states.md) */
enum {
    PATROL_STATE_IDLE = 0x01,             /* waiting: counts +0x9A down, then picks an idle activity */
    PATROL_STATE_WALK_BOSS_TURN = 0x0B,   /* turns toward the target at rate 284 outside boss arenas */
    PATROL_STATE_ATTACK_PROJ8 = 0x1B,     /* fires projectile 8 at animation offset 25 */
    PATROL_STATE_ATTACK_PROJ7 = 0x1C,     /* fires projectile 7 at animation offset 10, turn rate 170 */
    PATROL_STATE_ATTACK_AIM0 = 0x21,      /* AimAtTarget(0), turn rate 170 */
    PATROL_STATE_ATTACK_PROJ6 = 0x22,     /* projectile 6 every fifth frame */
    PATROL_STATE_ATTACK_PROJ3 = 0x37,     /* projectile 3 at animation offsets 3 / 7 (mod 10), turn rate 510 */
    PATROL_STATE_ATTACK_AIM5 = 0x38,      /* AimAtTarget(5), turn rate 170 */
    PATROL_STATE_BONE_FOLLOW = 0x29,      /* follows an object bone */
    PATROL_STATE_BONE_HOP = 0x2A,         /* hop (arc) toward the ground */
    PATROL_STATE_BONE_LAND = 0x2B         /* landing */
};

/* MonkeyInfo attack records (8 bytes each from +0x1E: enable u16, chance s16, minimum range s16, maximum range s16) */
#define INFO_ATTACK_AIM0 0x1Eu
#define INFO_ATTACK_PROJ6 0x26u
#define INFO_ATTACK_PROJ8 0x36u
#define INFO_ATTACK_PROJ7 0x3Eu
#define INFO_ATTACK_AIM5 0x46u
#define INFO_ATTACK_PROJ3 0x56u

/* the bone node case: while alerted the monkey keeps testing sight every 300th frame and loses the alert after MonkeyNop() failed tests;
   not alerted: a successful sight test makes it call (voice) and become alerted. */
static void patrol_watch_target(AeRam *r, const AeMonkeyHooks *h, uint32_t monkey) {
    if (RD8(monkey + AE_MK_ALERTED) == 0) {
        if (ae_MonkeyUpdateSight(r, h, 1) == 0) return;
        int32_t voice = ae_MonkeyGetCallVoice(r);
        ae_MonkeyQueueVoice(r, h, voice, 0);
        WR8(cur_monkey(r) + AE_MK_ALERTED, 1);
        WR8(cur_monkey(r) + AE_MK_LOST_SIGHT_COUNT, 0);
        return;
    }
    uint32_t lost_count = RD8(monkey + AE_MK_LOST_SIGHT_COUNT);
    int32_t limit = ae_MonkeyNop(r);
    if ((int32_t)lost_count < limit) {
        uint32_t frame = RD32(AE_G_FRAME_COUNTER);
        if (frame % 300u != 0) return;
        if (ae_MonkeyUpdateSight(r, h, 1) != 0) return;
        monkey = cur_monkey(r);
        WR8(monkey + AE_MK_LOST_SIGHT_COUNT, RD8(monkey + AE_MK_LOST_SIGHT_COUNT) + 1u);
    } else {
        WR8(cur_monkey(r) + AE_MK_ALERTED, 0);
        WR8(cur_monkey(r) + AE_MK_LOST_SIGHT_COUNT, 0);
    }
}

/* states 0x29 and the attacking states at a bone node: the monkey rides the bone: it stands at the next node (y + 50, then dropped to the
   ground unless it is a bone node) facing from the next node toward the current one; the object's animation index 1 starts the 0x2A hop
   instead (the two nodes are swapped). */
static void patrol_follow_bone(AeRam *r, const AeMonkeyHooks *h, uint32_t sp, uint32_t monkey) {
    uint32_t node0 = RD16(monkey + AE_MK_NAV_NODE0);
    int32_t bone_ref = RDS16(node0 * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES) + 0x10);      /* object * 100 + bone */
    int32_t object = (int16_t)(bone_ref / 100);
    if (AE_MK_HOOK(r, h, ObjSlotGetAnimIndex, (uint32_t)object, 0, 0, 0) == 1) {
        ae_MonkeyEnterState_2A(r, h);
        monkey = cur_monkey(r);
        uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);
        uint32_t this_node = RD16(monkey + AE_MK_NAV_NODE0);
        WR16(monkey + AE_MK_NAV_NODE0, next_node);
        WR16(monkey + AE_MK_NAV_NODE1, this_node);
        ae_MonkeyUpdateGoal(r, h);
        return;
    }
    ae_NavNodeGetPos(r, h, (int32_t)RD16(cur_monkey(r) + AE_MK_NAV_NODE1), sp + 32);       /* next node position */
    if (ae_ram_failed(r)) return;
    ae_NavNodeGetPos(r, h, (int32_t)RD16(cur_monkey(r) + AE_MK_NAV_NODE0), sp + 16);       /* current node position */
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    WR32(monkey + AE_MK_GOAL_POS, RD32(sp + 16));
    WR32(monkey + AE_MK_GOAL_POS + 4, RD32(sp + 20));
    WR32(monkey + AE_MK_GOAL_POS + 8, RD32(sp + 24));
    WR16(monkey + AE_MK_MOVE_DIR + 2, 0);
    WR16(monkey + AE_MK_MOVE_DIR, RD16(sp + 16) - RD16(sp + 32));
    WR16(monkey + AE_MK_MOVE_DIR + 4, RD16(sp + 24) - RD16(sp + 40));
    ae_sdk_VectorNormalSS(r, monkey + AE_MK_MOVE_DIR, monkey + AE_MK_MOVE_DIR);
    monkey = cur_monkey(r);
    uint32_t node_z = RD32(sp + 40);
    WR32(monkey + AE_MK_POS, RD32(sp + 32));
    WR32(monkey + AE_MK_POS + 4, RD32(sp + 36) + 50u);
    uint32_t flags2 = RD32(monkey + AE_MK_FLAGS2);
    WR32(monkey + AE_MK_POS + 8, node_z);
    if (flags2 & 2u) return;
    uint32_t this_node_index = RD16(monkey + AE_MK_NAV_NODE0);
    if (RD32(node_of(r, this_node_index)) & AE_NAV_FLAG_BONE) return;
    uint32_t y = RD32(monkey + AE_MK_POS + 4);
    int32_t ground = AE_MK_HOOK(r, h, GetGroundHeight, y, y - 50u, node_z, 0);     /* the third argument is the node's z */
    WR32(cur_monkey(r) + AE_MK_POS + 4, ground);
}

/* state 0x2A: the hop.  y = start y (s16 at +0xE4) - 100 n + 4 n^2 with n = the frame counter at +0xE3; landing on the ground (the s16 of
   GetGroundHeight) switches to state 0x2B. */
static void patrol_hop(AeRam *r, const AeMonkeyHooks *h, uint32_t monkey) {
    uint32_t n = RD8(monkey + AE_MK_HOP_COUNTER);
    int32_t gravity = mul_lo((int32_t)n, (int32_t)(n * 4u));
    WR8(monkey + AE_MK_HOP_COUNTER, RD8(monkey + AE_MK_HOP_COUNTER) + 1u);
    uint32_t start_y = (uint32_t)RDS16(monkey + AE_MK_HOP_START_Y);
    WR32(monkey + AE_MK_POS + 4, start_y - n * 100u + (uint32_t)gravity);
    uint32_t actor = cur_monkey(r);
    int32_t ground = AE_MK_HOOK(r, h, GetGroundHeight, RD32(actor + AE_MK_POS), RD32(actor + AE_MK_POS + 4), RD32(actor + AE_MK_POS + 8), 0);
    uint32_t y = RD32(actor + AE_MK_POS + 4);
    int32_t ground16 = s16((uint32_t)ground);
    if (ground16 < (int32_t)y) {
        ae_MonkeySetState(r, PATROL_STATE_BONE_LAND);
        monkey = cur_monkey(r);
        WR16(monkey + AE_MK_MOVE_DIR, 0x1000);
        WR16(monkey + AE_MK_MOVE_DIR + 2, 0);
        WR16(monkey + AE_MK_MOVE_DIR + 4, 0);
        WR16(monkey + 0x84, 0);
        WR16(monkey + 0x86, 0);
        WR16(monkey + 0x88, 0);
        WR32(monkey + AE_MK_POS + 4, ground16);
    }
}

/* the current node has flag 0x40: the monkey is put at the reference position (record s16 at +0x10 of the table at 0x800F455C, 56 bytes each:
   x, y, z as s16) and dropped to the ground below it (64 below the record's y). */
static void patrol_place_at_reference(AeRam *r, const AeMonkeyHooks *h, uint32_t monkey, uint32_t node) {
    int32_t record = RDS16(node + 0x10);
    uint32_t offset = (uint32_t)record * 56u;
    WR32(monkey + AE_MK_POS, RDS16(RD32(AE_G_NAV_GROUND_REF) + offset));
    WR32(monkey + AE_MK_POS + 4, RDS16(RD32(AE_G_NAV_GROUND_REF) + offset + 2));
    uint32_t x = RD32(monkey + AE_MK_POS);
    uint32_t y = RD32(monkey + AE_MK_POS + 4);
    uint32_t z = (uint32_t)RDS16(RD32(AE_G_NAV_GROUND_REF) + offset + 4);
    WR32(monkey + AE_MK_POS + 8, z);
    int32_t ground = AE_MK_HOOK(r, h, GetGroundHeight, x, y - 64u, z, 0);
    WR32(cur_monkey(r) + AE_MK_POS + 4, ground);
}

/* state 1 timer ran out: pick one of the idle activities (start states 0x0D / 0x10 / 0x13 / 0x3A).  A monkey that has noticed the target (flag
   bit 19) or, within 2000 units, a random roll (nearer = more likely, then 3 in 10) picks by the link angle +0x21C (which sector faces the
   target); otherwise the pick is random. */
static void patrol_pick_idle_activity(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    int pick_by_angle = (RD32(monkey + AE_MK_FLAGS) >> 19) & 1u;
    if (!pick_by_angle) {
        int32_t dist = (int32_t)RD32(monkey + AE_MK_DIST_TO_TARGET);
        if (dist < 2000) {
            int32_t chance = (int32_t)(2000u - (uint32_t)dist);
            uint32_t roll = ae_MonkeyRand(r) & 0xFFFFu;
            if ((int32_t)(roll % 2000u) < chance) {
                uint32_t roll2 = ae_MonkeyRand(r) & 0xFFFFu;
                if (roll2 % 10u < 3u) pick_by_angle = 1;
            }
        }
    }
    if (pick_by_angle) {
        monkey = cur_monkey(r);
        uint32_t flags = RD32(monkey + AE_MK_FLAGS);
        uint32_t angle = RD32(monkey + AE_MK_LINK_ANGLE);
        if ((flags >> 19) & 1u) {
            int32_t voice = ae_MonkeyGetCallVoice2(r);
            ae_MonkeyQueueVoice(r, h, voice, 0);
        }
        uint32_t new_state = 0;
        if (angle - 342u < 1365u) new_state = 0x0D;
        else if (angle - 1366u < 1365u) new_state = 0x13;
        else if (angle - 2390u < 1365u) new_state = 0x10;
        else if (angle < 740u || angle - 3357u < 740u) new_state = 0x3A;
        if (new_state != 0) WR8(cur_monkey(r) + AE_MK_AI_STATE, new_state);
        return;
    }
    switch (ae_MonkeyRand(r) & 3u) {
    case 0: WR8(cur_monkey(r) + AE_MK_AI_STATE, 0x0D); break;
    case 1: WR8(cur_monkey(r) + AE_MK_AI_STATE, 0x10); break;
    case 2: WR8(cur_monkey(r) + AE_MK_AI_STATE, 0x13); break;
    case 3: WR8(cur_monkey(r) + AE_MK_AI_STATE, 0x3A); break;
    }
}

/* the target distance lies in the attack record's [minimum, maximum] range (and the attack is enabled in the MonkeyInfo) */
static int patrol_attack_in_range(AeRam *r, uint32_t monkey, uint32_t info, uint32_t record) {
    if (RD16(info + record) == 0) return 0;
    int32_t dist = (int32_t)RD32(monkey + AE_MK_DIST_TO_TARGET);
    if (dist < RDS16(info + record + 4)) return 0;
    if (RDS16(info + record + 6) < dist) return 0;
    return 1;
}

/* frames into the current animation clip */
static int32_t patrol_anim_offset(AeRam *r) {
    int32_t anim_index = ae_MonkeyGetAnimIndex(r);
    uint32_t monkey = cur_monkey(r);
    uint32_t anim_table = RD32(monkey + AE_MK_ANIM_TABLE);
    uint32_t frame = RD16(monkey + AE_MK_ANIM_FRAME);
    uint32_t first_frame = RD16((uint32_t)anim_index * 8u + anim_table);
    return (int32_t)(frame - first_frame);
}

/* an attacking state with attacks left (+0x222 > 0): when the target is in range, the state's attack happens at its animation frame; without
   a match the attack counter is decremented */
static void patrol_attack(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    uint32_t state = RD8(monkey + AE_MK_AI_STATE);
    if (!(STATE_FLAGS(state) & STATE_FLAG_ATTACK)) return;
    if (RDS8(monkey + AE_MK_PATROL_COUNTER) <= 0) return;

    if (state == PATROL_STATE_ATTACK_PROJ6 && patrol_attack_in_range(r, monkey, info_of(r, monkey), INFO_ATTACK_PROJ6)) {
        if (RD32(AE_G_FRAME_COUNTER) % 5u == 0) ae_MonkeyFireProjectile6(r, h);
    } else if (state == PATROL_STATE_ATTACK_AIM0 && patrol_attack_in_range(r, monkey, info_of(r, monkey), INFO_ATTACK_AIM0)) {
        ae_MonkeyAimAtTarget(r, h, 0);
    } else if (state == PATROL_STATE_ATTACK_AIM5 && patrol_attack_in_range(r, monkey, info_of(r, monkey), INFO_ATTACK_AIM5)) {
        ae_MonkeyAimAtTarget(r, h, 5);
    } else if (state == PATROL_STATE_ATTACK_PROJ7 && patrol_attack_in_range(r, monkey, info_of(r, monkey), INFO_ATTACK_PROJ7)) {
        if (patrol_anim_offset(r) == 10) ae_MonkeyFireProjectile7(r, h);
    } else if (state == PATROL_STATE_ATTACK_PROJ8 && patrol_attack_in_range(r, monkey, info_of(r, monkey), INFO_ATTACK_PROJ8)) {
        if (patrol_anim_offset(r) == 25) ae_MonkeyFireProjectile8(r, h);
    } else if (state == PATROL_STATE_ATTACK_PROJ3 && patrol_attack_in_range(r, monkey, info_of(r, monkey), INFO_ATTACK_PROJ3)) {
        if (patrol_anim_offset(r) % 10 == 3 || patrol_anim_offset(r) % 10 == 7) {
            ae_MonkeySpawnProjectile(r, h, 3);
            ae_MonkeyQueueVoice(r, h, 0x48, 0);
        }
    } else {
        monkey = cur_monkey(r);
        WR8(monkey + AE_MK_PATROL_COUNTER, RD8(monkey + AE_MK_PATROL_COUNTER) - 1u);
    }
}

/* animation not finished, ordinary node (no bone, no object): should the monkey decide its next action here (1), or only restart the animation
   (0)?  node_flags = the flags of the next node. */
static int patrol_decide_at_node(AeRam *r, const AeMonkeyHooks *h, uint32_t monkey, uint32_t node_flags) {
    uint32_t state = RD8(monkey + AE_MK_AI_STATE);
    if (STATE_FLAGS(state) & STATE_FLAG_ATTACK) return 1;
    if (state == PATROL_STATE_ATTACK_PROJ7 || state == PATROL_STATE_ATTACK_PROJ3 || state == PATROL_STATE_ATTACK_AIM0 ||
        state == PATROL_STATE_ATTACK_AIM5 || state == PATROL_STATE_ATTACK_PROJ8) {
        if (RDS8(monkey + AE_MK_PATROL_COUNTER) <= 0) return 1;
    }
    if (node_flags & AE_NAV_FLAG_WAIT) {
        if (RD16(monkey + AE_MK_NODE_WAIT) == 0) return 1;
    }
    uint32_t next_flags = RD32(node_of(r, RD16(cur_monkey(r) + AE_MK_NAV_NODE1)));
    if (next_flags & AE_NAV_FLAG_NEAR_TARGET) {
        if (ae_AreaIsBossArena(r) == 0) {
            uint32_t m = cur_monkey(r);
            int32_t radius = RDS16(node_of(r, RD16(m + AE_MK_NAV_NODE1)) + 0x10);
            if ((int32_t)RD32(m + AE_MK_DIST_TO_TARGET) < radius) return 1;
        }
    }
    monkey = cur_monkey(r);
    uint32_t node = node_of(r, RD16(monkey + AE_MK_NAV_NODE1));
    next_flags = RD32(node);
    if (next_flags & AE_NAV_FLAG_FAR_TARGET) {
        if (!((int32_t)RD32(monkey + AE_MK_DIST_TO_TARGET) < 1501)) return 1;
    }
    if (next_flags & AE_NAV_FLAG_OBJECT) {
        if (AE_MK_HOOK(r, h, ObjSlotGetAnimIndex, (uint32_t)RDS16(node + 0x10), 0, 0, 0) == 2) return 1;
    }
    monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_ALERTED) == 1) {
        if (!(STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) & STATE_FLAG_ATTACK)) {
            if (ae_NavNodeIsSpecial(r, RDS16(monkey + AE_MK_NAV_NODE1)) == 0) return 1;
        }
    }
    monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_AI_STATE) == PATROL_STATE_ATTACK_PROJ3) return 0;
    uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);
    if ((RD32(next_node * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES)) & AE_NAV_FLAG_OBJECT) == 0) {
        if (ae_NavNodeIsSpecial(r, s16(next_node)) == 0) {
            monkey = cur_monkey(r);
            uint32_t tick = RD32(AE_G_FRAME_COUNTER) + RD8(monkey + AE_MK_ID) + 3u;
            if (tick % 150u == 0) {
                if (ae_MonkeyUpdateSight(r, h, 1) == 0) return 1;
            }
        }
    }
    monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_AI_STATE) == PATROL_STATE_ATTACK_PROJ3) return 0;
    if (ae_NavNodeIsSpecial(r, RDS16(monkey + AE_MK_NAV_NODE1)) != 0) return 0;
    if (!((int32_t)RD32(cur_monkey(r) + AE_MK_DIST_TO_TARGET) < 200)) return 0;
    return 1;
}

/* the end of the function: turn toward the target (rate 170 for the attack states 0x1C / 0x21 / 0x38, 284 in state 0x0B outside boss arenas,
   510 in state 0x37) */
static void patrol_turn_to_target(AeRam *r) {
    if (ae_ram_failed(r)) return;
    uint32_t state = RD8(cur_monkey(r) + AE_MK_AI_STATE);
    if (state == PATROL_STATE_ATTACK_PROJ7 || state == PATROL_STATE_ATTACK_AIM0 || state == PATROL_STATE_ATTACK_AIM5) {
        ae_MonkeyTurnTowardTarget(r, 170);
        return;
    }
    if (state == PATROL_STATE_WALK_BOSS_TURN) {
        if (ae_AreaIsBossArena(r) == 0) {
            ae_MonkeyTurnTowardTarget(r, 284);
            return;
        }
    }
    state = RD8(cur_monkey(r) + AE_MK_AI_STATE);
    if (state != PATROL_STATE_ATTACK_PROJ3) return;
    ae_MonkeyTurnTowardTarget(r, 510);
}

/* MonkeyAi_Patrol @8003DC7C: the walking / idling states (0 1 0x0B 0x16-0x18 0x1B 0x1C 0x21 0x22 0x26-0x2B 0x37 0x38 0x42).
   - the next node (+0xF4) is a bone node (flag 0x400): the alert handling (+0x79 and the counter at +0x249), the attack animation end
     (state table flags bit 3), the position follow of the bone (states 0x29 and the attacking states) and the 0x2A / 0x2B hop; any other
     next node: a current node with flag 0x40 places the monkey at the reference position (the table at 0x800F455C).
   - state 1 counts down +0x9A and picks the idle states 0x0D / 0x10 / 0x13 / 0x3A by the link angle (+0x21C) or at random.
   - animation finished (MonkeyAnimFinished != 0): boss arena handling, notice of the target, bone / link nodes (ObjSlotGetAnimIndex 1 / 2),
     the attack states (0x22 projectile 6, 0x21 aim 0, 0x38 aim 5, 0x1C projectile 7, 0x1B projectile 8, 0x37 projectile 3), else the
     counter +0x222 - 1.
   - animation not finished: the decision tree at the node (PatrolDecide / AlertedDecide / DecideAction / PickAnim).
   - at the end the turn toward the target (patrol_turn_to_target).
   Two calls pass a third argument that the pseudocode does not show: GetGroundHeight(y, y - 50, z) and (x, y - 64, z) with the z of the node. */
void ae_MonkeyAi_Patrol(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_PATROL);
    uint32_t monkey = cur_monkey(r);
    uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);
    uint32_t nodes = RD32(AE_G_NAV_NODES);
    uint32_t next_flags = RD32(next_node * AE_NAV_NODE_SIZE + nodes);

    if (next_flags & AE_NAV_FLAG_BONE) {
        /* ---- the next node is a bone node */
        patrol_watch_target(r, h, monkey);
        if (ae_ram_failed(r)) return;
        monkey = cur_monkey(r);
        if ((STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) & STATE_FLAG_ATTACK) && ae_MonkeyAnimFinished(r) == 0) {
            if (RDS8(cur_monkey(r) + AE_MK_PATROL_COUNTER) > 0) ae_MonkeyPickAnim(r);
            else ae_MonkeySetState(r, PATROL_STATE_BONE_FOLLOW);
        }
        monkey = cur_monkey(r);
        uint32_t state = RD8(monkey + AE_MK_AI_STATE);
        if (state == PATROL_STATE_BONE_FOLLOW || (STATE_FLAGS(state) & STATE_FLAG_ATTACK)) {
            patrol_follow_bone(r, h, sp, monkey);
            if (ae_ram_failed(r)) return;
            if (ae_MonkeyAnimFinished(r) == 0) {
                if (((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 23) & 1u) == 0 || ae_MonkeyPickAttackA(r, h) == 100) {
                    ae_MonkeyPickAnim(r);
                } else {
                    return;
                }
            }
        } else if (state == PATROL_STATE_BONE_HOP) {
            patrol_hop(r, h, monkey);
            if (ae_MonkeyAnimFinished(r) == 0) ae_MonkeyPickAnim(r);
        } else if (state == PATROL_STATE_BONE_LAND) {
            if (ae_MonkeyAnimFinished(r) == 0) {
                ae_MonkeyPickAnim(r);
                return;
            }
        }
        if (ae_ram_failed(r)) return;
        if (!(STATE_FLAGS(RD8(cur_monkey(r) + AE_MK_AI_STATE)) & STATE_FLAG_ATTACK)) return;
    } else {
        /* ---- not a bone node: a current node with flag 0x40 gives the position */
        uint32_t node = RD16(monkey + AE_MK_NAV_NODE0) * AE_NAV_NODE_SIZE + nodes;
        if (RD32(node) & AE_NAV_FLAG_REF_POS) patrol_place_at_reference(r, h, monkey, node);
    }

    /* ---- state 1: the idle countdown */
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_AI_STATE) == PATROL_STATE_IDLE) {
        int32_t timer = RDS16(monkey + AE_MK_IDLE_TIMER);
        WR16(monkey + AE_MK_IDLE_TIMER, timer - 1);
        if (!(timer > 0)) {
            patrol_pick_idle_activity(r, h);
            ae_MonkeyPickAnim(r);
            return;
        }
    }

    if (ae_MonkeyAnimFinished(r) != 0) {
        /* ---- the animation is finished */
        if (ae_AreaIsBossArena(r) != 0) {
            if (RD8(cur_monkey(r) + AE_MK_AI_STATE) == PATROL_STATE_WALK_BOSS_TURN && RD8(AE_G_EXIT_CODE_447C) == 3) {
                ae_MonkeyAlertedDecide(r, h);
                WR32(AE_G_FLAG_EB5C, 0);
            }
        }
        if (ae_ram_failed(r)) return;
        monkey = cur_monkey(r);
        uint32_t state = RD8(monkey + AE_MK_AI_STATE);
        if (state == PATROL_STATE_IDLE) {
            /* every third frame (per monkey) an idle monkey tests whether it sees the target */
            uint32_t tick = RD32(AE_G_FRAME_COUNTER) + RD8(monkey + AE_MK_ID);
            if (tick % 3u == 0 && ae_MonkeyUpdateSight(r, h, 1) != 0) {
                if (ae_AreaIsSpecialMonkeyArea(r) == 0) {
                    monkey = cur_monkey(r);
                    if (STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) & STATE_FLAG_CAN_NOTICE) {
                        WR8(monkey + AE_MK_ALERTED, state);
                        ae_MonkeyStartNotice(r, h);
                        return;
                    }
                }
                int32_t voice = ae_MonkeyGetCallVoice(r);
                ae_MonkeyQueueVoice(r, h, voice, 0);
                ae_MonkeyAlertedDecide(r, h);
                return;
            }
        }
        if (ae_ram_failed(r)) return;
        monkey = cur_monkey(r);
        next_node = RD16(monkey + AE_MK_NAV_NODE1);
        uint32_t node = next_node * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES);
        next_flags = RD32(node);
        if (next_flags & AE_NAV_FLAG_OBJECT) {
            /* an object node: the object's animation index 2 / 1 decides, else it is poked at random */
            if (AE_MK_HOOK(r, h, ObjSlotGetAnimIndex, (uint32_t)RDS16(node + 0x10), 0, 0, 0) == 2) {
                ae_MonkeyAlertedDecide(r, h);
            } else {
                uint32_t index = RD16(cur_monkey(r) + AE_MK_NAV_NODE1);
                if (AE_MK_HOOK(r, h, ObjSlotGetAnimIndex, (uint32_t)RDS16(node_of(r, index) + 0x10), 0, 0, 0) == 1) {
                    ae_MonkeyAlertedDecide(r, h);
                } else {
                    index = RD16(cur_monkey(r) + AE_MK_NAV_NODE1);
                    if (RD32(node_of(r, index)) & AE_NAV_FLAG_OBJECT) {
                        if ((ae_MonkeyRand(r) & 0xFFFFu) % 20u == 0) {
                            index = RD16(cur_monkey(r) + AE_MK_NAV_NODE1);
                            AE_MK_HOOK(r, h, ObjSlotSetField_CF, (uint32_t)RDS16(node_of(r, index) + 0x10), 3, 0, 0);
                        }
                    }
                }
            }
        } else if (next_flags & AE_NAV_FLAG_BONE) {
            patrol_attack(r, h);
        } else if (!(STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) & STATE_FLAG_ATTACK)) {
            /* nothing to do */
        } else if (RDS8(monkey + AE_MK_PATROL_COUNTER) > 0) {
            patrol_attack(r, h);
        } else {
            ae_MonkeyAlertedDecide(r, h);
        }
    } else {
        /* ---- the animation is not finished */
        if (ae_ram_failed(r)) return;
        if (ae_AreaIsBossArena(r) != 0 && RD8(AE_G_EXIT_CODE_447C) != 3) {
            ae_MonkeyPickAnim(r);
            ae_MonkeyPickAnim(r);
            patrol_turn_to_target(r);
            return;
        }
        monkey = cur_monkey(r);
        uint32_t state = RD8(monkey + AE_MK_AI_STATE);
        if (state == PATROL_STATE_ATTACK_PROJ3) {
            WR8(monkey + AE_MK_PATROL_COUNTER, RD8(monkey + AE_MK_PATROL_COUNTER) - 1u);
            WR16(monkey + AE_MK_COUNTER96, RD16(monkey + AE_MK_COUNTER96) + 1u);
            if (RDS8(cur_monkey(r) + AE_MK_PATROL_COUNTER) <= 0) ae_MonkeySpawnProjectile(r, h, 4);
            ae_MonkeyPickAnim(r);
            patrol_turn_to_target(r);
            return;
        }
        next_node = RD16(monkey + AE_MK_NAV_NODE1);
        next_flags = RD32(node_of(r, next_node));
        if (!(next_flags & AE_NAV_FLAG_BONE) && (STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) & STATE_FLAG_ATTACK)) {
            if (RDS8(monkey + AE_MK_PATROL_COUNTER) <= 0) {
                ae_MonkeyAlertedDecide(r, h);
            } else {
                ae_MonkeyPickAnim(r);
            }
            ae_MonkeyPickAnim(r);
            patrol_turn_to_target(r);
            return;
        }
        monkey = cur_monkey(r);
        next_node = RD16(monkey + AE_MK_NAV_NODE1);
        uint32_t node = next_node * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES);
        next_flags = RD32(node);
        if (next_flags & AE_NAV_FLAG_OBJECT) {
            if (AE_MK_HOOK(r, h, ObjSlotGetAnimIndex, (uint32_t)RDS16(node + 0x10), 0, 0, 0) == 2) ae_MonkeyAlertedDecide(r, h);
            ae_MonkeyPickAnim(r);
        } else {
            state = RD8(monkey + AE_MK_AI_STATE);
            if (state - 39u < 2u || state == 23 || state == 24 || state == 41) {
                ae_MonkeyPatrolDecide(r, h);
                return;
            }
            if ((RD32(RD16(monkey + AE_MK_NAV_NODE0) * AE_NAV_NODE_SIZE + RD32(AE_G_NAV_NODES)) & AE_NAV_FLAG_REF_POS) ||
                state == PATROL_STATE_IDLE) {
                ae_MonkeyPickAnim(r);
                return;
            }
            if (patrol_decide_at_node(r, h, monkey, next_flags)) {
                monkey = cur_monkey(r);
                if (RD8(monkey + AE_MK_AI_STATE) == PATROL_STATE_WALK_BOSS_TURN &&
                    !(RD32(monkey + AE_MK_DIST_TO_TARGET) - 300u < 1201u)) {
                    ae_MonkeyAlertedDecide(r, h);
                } else {
                    ae_MonkeyDecideAction(r, h);
                }
            }
            ae_MonkeyPickAnim(r);
        }
    }
    patrol_turn_to_target(r);
}
