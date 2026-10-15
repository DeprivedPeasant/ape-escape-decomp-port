/*
 * Monkey code, batch 5a (call-graph level 8: callees are batch 1..7 functions), ported from the original instructions
 * (see ae_monkey.h and notes/port/monkeys-port.md).  Where the Ghidra pseudocode and the listing differ, the listing was
 * followed.
 */
#include "ae_monkey_inl.h"

#define STATE_FLAGS(st) RD32(AE_G_MONKEY_STATE_TABLE + (uint32_t)(st) * 8u + 4u)

/* ================================================================================================
 * MonkeyDecideAction, MonkeyAi_IdleActivityEnd, MonkeyAi_State07
 * ============================================================================================== */

/* MonkeyDecideAction @800378C0: an alerted monkey (+0x79 == 1) takes the alerted decision, any other the patrol decision. */
void ae_MonkeyDecideAction(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_ALERTED) == 1) ae_MonkeyAlertedDecide(r, h);
    else ae_MonkeyPatrolDecide(r, h);
}

/* MonkeyAi_IdleActivityEnd @8003F4B4: states 0x0F / 0x12 / 0x15 / 0x3C.  Animation not finished: (Rand & 0xFFFF) % 6 == 0 starts
   another idle wait, else the patrol decision. */
void ae_MonkeyAi_IdleActivityEnd(AeRam *r, const AeMonkeyHooks *h) {
    if (ae_MonkeyAnimFinished(r) != 0) return;
    uint32_t roll = ae_MonkeyRand(r) & 0xFFFFu;
    if (roll % 6u == 0) ae_MonkeyEnterIdleWait(r);
    else ae_MonkeyPatrolDecide(r, h);
}

/* MonkeyAi_State07 @80040170: state 7: when the animation is at its start the move direction (+0x60) becomes the s16 x / z
   difference to the target, normalised (VectorNormalSS in place, y = 0); then, if the animation is not finished, a new node is
   selected (0, 0) and the alerted decision taken. */
void ae_MonkeyAi_State07(AeRam *r, const AeMonkeyHooks *h) {
    if (ae_MonkeyAnimAtStart(r) != 0) {
        uint32_t target = RD32(AE_G_MONKEY_TARGET);
        uint32_t monkey = cur_monkey(r);
        uint32_t target_x = RD16(target + AE_MK_POS);
        uint32_t monkey_x = RD16(monkey + AE_MK_POS);
        uint32_t move_dir = monkey + AE_MK_MOVE_DIR;
        WR16(monkey + AE_MK_MOVE_DIR + 2, 0);
        WR16(monkey + AE_MK_MOVE_DIR, target_x - monkey_x);
        uint32_t target_z = RD16(target + AE_MK_POS + 8);
        uint32_t monkey_z = RD16(monkey + AE_MK_POS + 8);
        WR16(monkey + AE_MK_MOVE_DIR + 4, target_z - monkey_z);
        ae_sdk_VectorNormalSS(r, move_dir, move_dir);
    }
    if (ae_MonkeyAnimFinished(r) == 0) {
        ae_MonkeySelectNextNode(r, h, 0, 0);
        ae_MonkeyAlertedDecide(r, h);
    }
}

/* ================================================================================================
 * MonkeyAi_MoveUnalerted
 * ============================================================================================== */

/* the movement tail shared by the move handlers: y += s16(+0x72) when flags2 bit 1 is set, then x += s16(+0x70), z += s16(+0x74) */
static void apply_step(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    if (RD32(monkey + AE_MK_FLAGS2) & 2u) {
        int32_t step_y = RDS16(monkey + AE_MK_VEL + 2);
        WR32(monkey + AE_MK_POS + 4, RD32(monkey + AE_MK_POS + 4) + (uint32_t)step_y);
        monkey = cur_monkey(r);
    }
    int32_t step_x = RDS16(monkey + AE_MK_VEL);
    uint32_t x = RD32(monkey + AE_MK_POS);
    int32_t step_z = RDS16(monkey + AE_MK_VEL + 4);
    uint32_t z = RD32(monkey + AE_MK_POS + 8);
    WR32(monkey + AE_MK_POS, x + (uint32_t)step_x);
    WR32(monkey + AE_MK_POS + 8, z + (uint32_t)step_z);
}

/* MonkeyAi_MoveUnalerted @8003D9D4: movement states of a monkey that has not noticed the target.  Every third frame (frame counter
   + id divisible by 3) UpdateSight(1): a hit alerts it (special areas / state table flags bit 6 clear: call voice + alerted
   decision, else alerted = 1 and MonkeyStartNotice) and ends the handler.  Otherwise: on reaching a node (states 0x1A / 0x35 with
   +0x94 > 0 first take the offset between the two nodes as the goal vector) select the next node and take the patrol decision; a
   state change ends the handler.  Without a state change (or without reaching a node) a finished animation moves the monkey
   (link vector interpolation in states 0x0C / 0x24, else the matrix step of +0x9C on +0x60 while +0x94 > 0; then the step +0x70 / +0x72 / +0x74), an unfinished one takes the patrol decision. */
void ae_MonkeyAi_MoveUnalerted(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_MOVE_UNALERTED);
    uint32_t monkey = cur_monkey(r);
    uint32_t tick = RD32(AE_G_FRAME_COUNTER) + RD8(monkey + AE_MK_ID);

    /* every third frame (per monkey): does the monkey see the target? */
    if (tick % 3u == 0 && ae_MonkeyUpdateSight(r, h, 1) != 0) {
        int notice = 0;
        if (ae_AreaIsSpecialMonkeyArea(r) == 0) {
            monkey = cur_monkey(r);
            if ((STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) >> 6) & 1u) notice = 1;
        }
        if (notice) {
            WR8(monkey + AE_MK_ALERTED, 1);     /* delay slot of the jal: monkey is the monkey loaded above */
            ae_MonkeyStartNotice(r, h);
        } else {
            int32_t voice = ae_MonkeyGetCallVoice(r);
            ae_MonkeyQueueVoice(r, h, voice, 0);
            ae_MonkeyAlertedDecide(r, h);
        }
        return;
    }

    if (ae_MonkeyReachedNode(r) != 0) {
        monkey = cur_monkey(r);
        int32_t cooldown = RDS16(monkey + AE_MK_ACTION_COOLDOWN);
        uint32_t state_before = RD8(monkey + AE_MK_AI_STATE);
        if (cooldown > 0 && (state_before == 26 || state_before == 53)) {
            /* the goal vector is the offset between the two nodes */
            ae_NavNodeGetPos(r, h, (int32_t)RD16(monkey + AE_MK_NAV_NODE0), sp + 16);
            if (ae_ram_failed(r)) return;
            monkey = cur_monkey(r);
            ae_NavNodeGetPos(r, h, (int32_t)RD16(monkey + AE_MK_NAV_NODE1), sp + 32);
            if (ae_ram_failed(r)) return;
            uint32_t dx = RD32(sp + 16) - RD32(sp + 32);
            monkey = cur_monkey(r);
            WR32(monkey + AE_MK_GOAL_POS, dx);
            WR32(monkey + AE_MK_GOAL_POS + 8, RD32(sp + 24) - RD32(sp + 40));
        }
        ae_MonkeySelectNextNode(r, h, 1, 0);
        if (ae_ram_failed(r)) return;
        ae_MonkeyPatrolDecide(r, h);
        if (ae_ram_failed(r)) return;
        if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != 9) ae_MonkeyUpdateFacing(r, h);
        if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != state_before) return;
    }

    if (ae_MonkeyAnimFinished(r) == 0) {
        ae_MonkeyPatrolDecide(r, h);
        return;
    }
    if (ae_MonkeyIsState0C_or_24(r) != 0) {
        if (RDS16(cur_monkey(r) + AE_MK_LINK_FRAME) > 0) ae_MonkeyInterpolateLinkVector(r);
    } else {
        monkey = cur_monkey(r);
        if (RDS16(monkey + AE_MK_ACTION_COOLDOWN) > 0) {
            ae_sdk_ApplyMatrixSV(r, monkey + 0x9C, monkey + AE_MK_MOVE_DIR, monkey + AE_MK_MOVE_DIR);
            monkey = cur_monkey(r);
            WR16(monkey + AE_MK_ACTION_COOLDOWN, RD16(monkey + AE_MK_ACTION_COOLDOWN) - 1u);
        }
    }
    apply_step(r);
}

/* ================================================================================================
 * MonkeyAi_MoveAlerted
 * ============================================================================================== */

/* update the facing unless the state is 9 or an alt-animation state */
static void alerted_update_facing(AeRam *r, const AeMonkeyHooks *h) {
    if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != 9) {
        if (ae_MonkeyIsAltAnimState(r) == 0) ae_MonkeyUpdateFacing(r, h);
    }
}

/* MonkeyAi_MoveAlerted @8003C6D4: movement states of a monkey that has noticed the target.  States 3 / 8: foot dust.  States 0x24 /
   8 at the first frame of the animation minus 10, or 0x1A / 0x35 at frame counter % 300 != 0: UpdateSight(0) resets (hit) or
   increments (miss) the counter at +0x249.  On reaching a node: select the next node (1, 1); if one was selected the alerted
   decision (with UpdateFacing unless the state is 9 / alt-anim; a state change ends the handler), else outside boss arenas a
   plain state-0 reset when the target is far (+0x20C >= 200), otherwise (0, 0) + node dispatch.  With a finished animation: every
   fifth frame UpdateSight(1) re-selects the node, attack B at frames % 5 in state 8, the movement of the unalerted handler; an
   unfinished animation looks for the target (UpdateSight(0)) or, via the counter at +0x249 against MonkeyNop() (2), falls back to
   the patrol decision. */
void ae_MonkeyAi_MoveAlerted(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey;
    uint32_t state = RD8(cur_monkey(r) + AE_MK_AI_STATE);
    if (state == 3 || state == 8) ae_MonkeyFootDust(r, h);

    /* does the monkey look for the target in this frame? */
    int check_sight = 0;
    state = RD8(cur_monkey(r) + AE_MK_AI_STATE);
    if (state == 36 || state == 8) {
        /* ten frames into the animation */
        int32_t anim_index = ae_MonkeyGetAnimIndex(r);
        monkey = cur_monkey(r);
        uint32_t anim_table = RD32(monkey + AE_MK_ANIM_TABLE);
        uint32_t frame = RD16(monkey + AE_MK_ANIM_FRAME);
        uint32_t first_frame = RD16((uint32_t)anim_index * 8u + anim_table);
        if (first_frame == frame - 10u) check_sight = 1;
    }
    if (!check_sight) {
        state = RD8(cur_monkey(r) + AE_MK_AI_STATE);
        if (state == 26 || state == 53) {
            if (RD32(AE_G_FRAME_COUNTER) % 300u != 0) check_sight = 1;
        }
    }
    if (check_sight) {
        if ((uint32_t)ae_MonkeyUpdateSight(r, h, 0) << 16 != 0) {
            WR8(cur_monkey(r) + AE_MK_LOST_SIGHT_COUNT, 0);
        } else {
            monkey = cur_monkey(r);
            WR8(monkey + AE_MK_LOST_SIGHT_COUNT, RD8(monkey + AE_MK_LOST_SIGHT_COUNT) + 1u);
        }
    }

    if (ae_MonkeyReachedNode(r) != 0) {
        if ((ae_MonkeySelectNextNode(r, h, 1, 1) & 0xFF) != 0) {
            uint32_t state_before = RD8(cur_monkey(r) + AE_MK_AI_STATE);
            ae_MonkeyAlertedDecide(r, h);
            if (ae_ram_failed(r)) return;
            alerted_update_facing(r, h);
            if (RD8(cur_monkey(r) + AE_MK_AI_STATE) != state_before) return;
        } else {
            int reset_state = 0;
            if (ae_AreaIsBossArena(r) == 0) {
                state = RD8(cur_monkey(r) + AE_MK_AI_STATE);
                if (state != 9 && ae_MonkeyIsAltAnimState(r) == 0) {
                    state = RD8(cur_monkey(r) + AE_MK_AI_STATE);
                    if (!(state == 61 || state == 26 || state == 53) && ae_MonkeyIsState0C_or_24(r) == 0) {
                        if (!((int32_t)RD32(cur_monkey(r) + AE_MK_DIST_TO_TARGET) < 200)) reset_state = 1;
                    }
                }
            }
            if (reset_state) {
                ae_MonkeySetState(r, 0);
                return;
            }
            ae_MonkeySelectNextNode(r, h, 0, 0);
            if (ae_ram_failed(r)) return;
            ae_MonkeyNodeActionDispatch(r, h);
            if (ae_ram_failed(r)) return;
            alerted_update_facing(r, h);
        }
    }

    if (ae_MonkeyAnimFinished(r) == 0) {
        /* ---- animation not finished */
        if (ae_MonkeyUpdateSight(r, h, 0) != 0) {
            ae_MonkeySelectNextNode(r, h, 0, 0);
            ae_MonkeyAlertedDecide(r, h);
            if (ae_ram_failed(r)) return;
            alerted_update_facing(r, h);
            return;
        }
        /* lost sight: after MonkeyNop() failed tests the monkey calms down (patrol decision) */
        int calm_down = 0;
        if (ae_AreaIsSpecialMonkeyArea(r) == 0) {
            uint32_t lost_count = RD8(cur_monkey(r) + AE_MK_LOST_SIGHT_COUNT);
            int32_t limit = ae_MonkeyNop(r);
            if (!((int32_t)lost_count < limit)) calm_down = 1;
        }
        if (!calm_down) {
            if (ae_AreaIsSpecialMonkeyArea(r) == 0 || RD8(cur_monkey(r) + AE_MK_LOST_SIGHT_COUNT) == 0) {
                ae_MonkeyAlertedDecide(r, h);
                return;
            }
        }
        WR8(cur_monkey(r) + AE_MK_LOST_SIGHT_COUNT, 0);
        ae_MonkeyPatrolDecide(r, h);
        return;
    }

    /* ---- animation finished: every fifth frame the sight is tested and a new node selected */
    if (RD32(AE_G_FRAME_COUNTER) % 5u == 0) {
        if (ae_MonkeyUpdateSight(r, h, 1) != 0) {
            ae_MonkeySelectNextNode(r, h, 0, 0);
            if (ae_ram_failed(r)) return;
            alerted_update_facing(r, h);
            return;
        }
    }
    /* state 8: attack B every fifth animation frame */
    {
        int32_t anim_index = ae_MonkeyGetAnimIndex(r);
        monkey = cur_monkey(r);
        uint32_t anim_table = RD32(monkey + AE_MK_ANIM_TABLE);
        int32_t frame_offset = (int32_t)RD16(monkey + AE_MK_ANIM_FRAME);
        frame_offset = sub32(frame_offset, (int32_t)RD16((uint32_t)anim_index * 8u + anim_table));
        if (frame_offset % 5 == 0) {
            if ((RD32(monkey + AE_MK_FLAGS) >> 23) & 1u) {
                if (RD8(monkey + AE_MK_AI_STATE) == 8) {
                    int32_t kind = s16((uint32_t)ae_MonkeyPickAttackB(r, h));
                    if (kind != 100) ae_MonkeyFireProjectileAimed(r, h, kind);
                }
            }
        }
    }
    if (ae_MonkeyIsState0C_or_24(r) != 0) {
        if (RDS16(cur_monkey(r) + AE_MK_LINK_FRAME) > 0) ae_MonkeyInterpolateLinkVector(r);
    } else {
        monkey = cur_monkey(r);
        if (RDS16(monkey + AE_MK_ACTION_COOLDOWN) > 0) {
            uint32_t info = info_of(r, monkey);
            if (RDS16(info + 4) != 0) {
                ae_sdk_ApplyMatrixSV(r, monkey + 0x9C, monkey + AE_MK_MOVE_DIR, monkey + AE_MK_MOVE_DIR);
                monkey = cur_monkey(r);
                WR16(monkey + AE_MK_ACTION_COOLDOWN, RD16(monkey + AE_MK_ACTION_COOLDOWN) - 1u);
            }
        }
    }
    apply_step(r);
}

/* ================================================================================================
 * MonkeyAi_State1E_20
 * ============================================================================================== */

#define AE_G_FRAME_EC350 0x800EC350u    /* u32 counter divided by 300 for the sound / target pattern */
#define AE_G_BYTES_EC08 0x800AEC08u     /* s8 per slot: +1 / -1 (flight direction) */
#define AE_G_BYTES_EC0C 0x800AEC0Cu     /* s8 per slot: +1 / -1 (circling direction) */
#define AE_G_ATTACK_KIND 0x800AEC00u    /* s16 per monkey id: the attack kind in progress (100 = none) */
#define AE_G_BOSS_POS_XZ 0x800B8CA0u    /* s16 x, s16 z of the monkey */

/* the dying monkey (health below 401) that has not started its flight yet: state 0x1E, flag bit 24 set, the node it stands on becomes the
   previous node and a random neighbour (never the next node: then the following link) the next one */
static void state1e_start_death_flight(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    WR8(monkey + AE_MK_AI_STATE, 0x1E);
    monkey = cur_monkey(r);
    uint32_t flags = RD32(monkey + AE_MK_FLAGS) | 0x01000000u;
    uint32_t node = RD16(monkey + AE_MK_NAV_NODE0);
    WR32(monkey + AE_MK_FLAGS, flags);
    WR16(monkey + AE_MK_PREV_NODE, node);       /* delay slot of the jal */
    uint32_t roll = ae_MonkeyRand(r) & 0xFFFFu;
    monkey = cur_monkey(r);
    node = RD16(monkey + AE_MK_NAV_NODE0);
    uint32_t link_count = RD8(node_of(r, node) + 0xB);
    int32_t pick = rem_trap(r, (int32_t)roll, (int32_t)link_count);
    if (ae_ram_failed(r)) return;
    int32_t link = ae_NavNodeGetLink(r, s16(node), pick);
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);
    uint32_t this_node = RD16(monkey + AE_MK_NAV_NODE0);
    if ((uint32_t)s16((uint32_t)link) != next_node) {
        WR16(monkey + AE_MK_NAV_NODE0, link);
    } else {
        uint32_t links_here = RD8(node_of(r, this_node) + 0xB);
        int32_t second = rem_trap(r, pick + 1, (int32_t)links_here);
        if (ae_ram_failed(r)) return;
        int32_t other_link = ae_NavNodeGetLink(r, s16(this_node), second);
        if (ae_ram_failed(r)) return;
        WR16(cur_monkey(r) + AE_MK_NAV_NODE0, other_link);
    }
    ae_MonkeySetNavNode(r, h, s16((uint32_t)link));
    if (ae_ram_failed(r)) return;
    WR16(cur_monkey(r) + AE_MK_NAV_NODE1, this_node);
}

/* the dying monkey flies to its goal; at the goal (closer than +0x220) the loop sound stops, the next node is selected, the monkey is
   defeated (state 0x36 with the burst) and the function ends without publishing the position (returns 1) */
static int state1e_death_flight(AeRam *r, const AeMonkeyHooks *h) {
    ae_MonkeyApplyMoveDir(r);
    ae_MonkeyMoveStep(r, h);
    ae_MonkeyDebrisBurst6(r, h);
    if (ae_ram_failed(r)) return 0;
    uint32_t monkey = cur_monkey(r);
    int32_t dist = ae_Distance3D(r, monkey + AE_MK_GOAL_POS, monkey + AE_MK_POS);
    monkey = cur_monkey(r);
    int32_t radius = RDS16(monkey + AE_MK_REACH_RADIUS);
    if (dist < radius) {
        AE_MK_HOOK(r, h, SoundLoopStopAndFree, 0x6050, RD32(monkey + 0x15C), 0, 0);
        ae_MonkeySelectNextNode(r, h, 1, 1);
        if (ae_ram_failed(r)) return 0;
        WR16(cur_monkey(r) + 0x4A, ae_AreaIsBossArena(r) != 0 ? 0xC000u : 0x8000u);       /* collision body flags */
        alerted_update_facing(r, h);
        WR16(cur_monkey(r) + AE_MK_HEALTH, 1000);              /* delay slot of the jal */
        ae_MonkeyEnterState_36(r, h);
        ae_MonkeyDefeatBurst(r, h);
        return 1;
    }
    int32_t step_x = RDS16(monkey + AE_MK_VEL), step_y = RDS16(monkey + AE_MK_VEL + 2), step_z = RDS16(monkey + AE_MK_VEL + 4);
    WR32(monkey + AE_MK_POS, RD32(monkey + AE_MK_POS) + (uint32_t)step_x);
    uint32_t y = RD32(monkey + AE_MK_POS + 4);
    uint32_t z = RD32(monkey + AE_MK_POS + 8);
    WR32(monkey + AE_MK_POS + 4, y + (uint32_t)step_y);
    WR32(monkey + AE_MK_POS + 8, z + (uint32_t)step_z);
    return 0;
}

/* the goal height while circling: the target's y minus `drop`, unless the target is not above the ground at the goal point: then the ground
   height there minus `drop` */
static void state1e_goal_height(AeRam *r, const AeMonkeyHooks *h, uint32_t monkey, uint32_t drop) {
    uint32_t target = RD32(AE_G_MONKEY_TARGET);
    int32_t ground = AE_MK_HOOK(r, h, GetGroundHeight, RD32(monkey + AE_MK_GOAL_POS), RD32(monkey + AE_MK_GOAL_POS + 4),
                                RD32(monkey + AE_MK_GOAL_POS + 8), 0);
    uint32_t goal_y;
    if (!((int32_t)RD32(target + AE_MK_POS + 4) < ground)) {
        uint32_t m = cur_monkey(r);
        int32_t ground_here = AE_MK_HOOK(r, h, GetGroundHeight, RD32(m + AE_MK_GOAL_POS), RD32(m + AE_MK_GOAL_POS + 4),
                                         RD32(m + AE_MK_GOAL_POS + 8), 0);
        goal_y = (uint32_t)ground_here - drop;
    } else {
        goal_y = RD32(RD32(AE_G_MONKEY_TARGET) + AE_MK_POS + 4) - drop;
    }
    WR32(monkey + AE_MK_GOAL_POS + 4, goal_y);
}

/* amplitude of the circling goals: radius / 2 + radius * (wobble + 4096) / 16384 */
static int32_t state1e_amplitude(int32_t radius, int32_t wobble) {
    return add32(radius / 2, mul_lo(radius, add32(wobble, 0x1000)) / 16384);
}

/* every 300th sample of the frame counter and (1 in 300) a random roll the flight direction (EC08, per slot) flips; the new direction -1
   picks a hover point on the circle around the circle centre (radius s16(+0x240) * (sin + 1) / 2), 500 above the target */
static void state1e_maybe_flip_direction(AeRam *r) {
    if (RD32(AE_G_FRAME_EC350) % 300u != 0) return;
    uint32_t roll_a = ae_MonkeyRand(r) & 0xFFFFu;
    uint32_t roll_b = ae_MonkeyRand(r) & 0xFFFFu;
    if (mul_lo((int32_t)roll_a, (int32_t)roll_b) % 300 != 0) return;
    uint32_t direction = AE_G_BYTES_EC08 + RD8(cur_monkey(r) + AE_MK_SLOT);
    WR8(direction, 0u - RD8(direction));
    if (RDS8(AE_G_BYTES_EC08 + RD8(cur_monkey(r) + AE_MK_SLOT)) != -1) return;

    uint32_t monkey = cur_monkey(r);
    int32_t radius = RDS16(monkey + AE_MK_CIRCLE_RADIUS);
    int32_t angle = (int32_t)(RD32(AE_G_FRAME_COUNTER) * 5u & 0xFFFu);
    int32_t amplitude = mul_lo(radius, add32(ae_sdk_rsin(r, angle), 0x1000)) / 8192;
    angle = (int32_t)(RD32(AE_G_FRAME_COUNTER) * 5u & 0xFFFu);
    int32_t offset_x = mul_lo(amplitude, ae_sdk_rsin(r, angle)) / 4096;
    monkey = cur_monkey(r);
    WR32(monkey + AE_MK_GOAL_POS, RD32(monkey + AE_MK_CIRCLE_CENTER) + (uint32_t)offset_x);
    angle = (int32_t)(RD32(AE_G_FRAME_COUNTER) * 5u & 0xFFFu);
    WR32(monkey + AE_MK_GOAL_POS + 4, RD32(RD32(AE_G_MONKEY_TARGET) + AE_MK_POS + 4) - 500u);
    int32_t offset_z = mul_lo(amplitude, ae_sdk_rcos(r, angle)) / 4096;
    monkey = cur_monkey(r);
    WR32(monkey + AE_MK_GOAL_POS + 8, RD32(monkey + AE_MK_CIRCLE_CENTER + 8) + (uint32_t)offset_z);
}

/* direction +1 with the target inside the circle radius: the goal is a point 1500 units from the target at the angle 30 * circling direction
   (EC0C, flips at random) * frame counter, 500 below the target */
static void state1e_chase_goal(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t roll_a = ae_MonkeyRand(r) & 0xFFFFu;
    uint32_t roll_b = ae_MonkeyRand(r) & 0xFFFFu;
    if (mul_lo((int32_t)roll_a, (int32_t)roll_b) % 600 == 0) {
        uint32_t direction = AE_G_BYTES_EC0C + RD8(cur_monkey(r) + AE_MK_SLOT);
        WR8(direction, 0u - RD8(direction));
    }
    int32_t circling = RDS8(AE_G_BYTES_EC0C + RD8(cur_monkey(r) + AE_MK_SLOT));
    int32_t angle = (int32_t)((uint32_t)mul_lo(circling, (int32_t)RD32(AE_G_FRAME_COUNTER)) * 30u);
    int32_t sine = ae_sdk_rsin(r, angle);
    uint32_t monkey = cur_monkey(r);
    uint32_t target = RD32(AE_G_MONKEY_TARGET);
    WR32(monkey + AE_MK_GOAL_POS, RD32(target + AE_MK_POS) + (uint32_t)(sine * 1500 / 4096));
    state1e_goal_height(r, h, monkey, 500);
    circling = RDS8(AE_G_BYTES_EC0C + RD8(cur_monkey(r) + AE_MK_SLOT));
    angle = (int32_t)((uint32_t)mul_lo(circling, (int32_t)RD32(AE_G_FRAME_COUNTER)) * 30u);
    int32_t cosine = ae_sdk_rcos(r, angle);
    monkey = cur_monkey(r);
    target = RD32(AE_G_MONKEY_TARGET);
    WR32(monkey + AE_MK_GOAL_POS + 8, RD32(target + AE_MK_POS + 8) + (uint32_t)(cosine * 1500 / 4096));
}

/* direction +1 with the target outside the circle radius: the goal circles the circle centre on a pulsing radius (angle 30 * frame counter),
   300 below the target */
static void state1e_orbit_goal(AeRam *r, const AeMonkeyHooks *h) {
    int32_t radius = RDS16(cur_monkey(r) + AE_MK_CIRCLE_RADIUS);
    int32_t wobble = ae_sdk_rsin(r, (int32_t)(RD32(AE_G_FRAME_COUNTER) & 0xFFFu));
    int32_t amplitude = state1e_amplitude(radius, wobble);
    int32_t angle = (int32_t)(RD32(AE_G_FRAME_COUNTER) * 30u & 0xFFEu);
    int32_t offset_x = mul_lo(amplitude, ae_sdk_rsin(r, angle)) / 4096;
    uint32_t monkey = cur_monkey(r);
    WR32(monkey + AE_MK_GOAL_POS, RD32(monkey + AE_MK_CIRCLE_CENTER) + (uint32_t)offset_x);
    state1e_goal_height(r, h, monkey, 300);
    angle = (int32_t)(RD32(AE_G_FRAME_COUNTER) * 30u & 0xFFEu);
    int32_t offset_z = mul_lo(amplitude, ae_sdk_rcos(r, angle)) / 4096;
    monkey = cur_monkey(r);
    WR32(monkey + AE_MK_GOAL_POS + 8, RD32(monkey + AE_MK_CIRCLE_CENTER + 8) + (uint32_t)offset_z);
}

/* direction -1 near the goal (+0x214 < 200): a random point on the pulsing circle, 500 above the target (flags3 bit 2 is set) */
static void state1e_hover_goal(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    uint32_t flags3 = RD32(monkey + AE_MK_FLAGS3);
    int32_t dist_to_goal = (int32_t)RD32(monkey + AE_MK_DIST_XZ_TO_GOAL);
    WR32(monkey + AE_MK_FLAGS3, flags3 | 4u);
    if (!(dist_to_goal < 200)) return;
    uint32_t roll_a = ae_MonkeyRand(r) & 0xFFFFu;
    uint32_t roll_b = ae_MonkeyRand(r) & 0xFFFFu;
    int32_t angle = mul_lo((int32_t)roll_a, (int32_t)roll_b) % 4096;
    int32_t wobble = ae_sdk_rsin(r, angle);
    int32_t radius = RDS16(cur_monkey(r) + AE_MK_CIRCLE_RADIUS);
    int32_t amplitude = state1e_amplitude(radius, wobble % 4096);
    int32_t offset_x = mul_lo(amplitude, ae_sdk_rsin(r, angle)) / 4096;
    monkey = cur_monkey(r);
    uint32_t target = RD32(AE_G_MONKEY_TARGET);
    WR32(monkey + AE_MK_GOAL_POS, RD32(monkey + AE_MK_CIRCLE_CENTER) + (uint32_t)offset_x);
    WR32(monkey + AE_MK_GOAL_POS + 4, RD32(target + AE_MK_POS + 4) - 500u);
    int32_t offset_z = mul_lo(amplitude, ae_sdk_rcos(r, angle)) / 4096;
    monkey = cur_monkey(r);
    WR32(monkey + AE_MK_GOAL_POS + 8, RD32(monkey + AE_MK_CIRCLE_CENTER + 8) + (uint32_t)offset_z);
}

/* the healthy flying monkey: state 0x20 returns to 0x1E when its animation ends; below 240 frames it rises (goal 3000 above), later it picks a
   goal around the circle centre / target (flipping flight direction), moves toward it, bobs up and down, stays above the ground, turns toward
   the target (below 240 frames) and attacks (attack kind of the monkey id; PickAttackC when the animation has not ended) */
static void state1e_fly_and_attack(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_AI_STATE) == 0x20) {
        ae_MonkeyDebrisBurst6(r, h);
        if (ae_MonkeyAnimFinished(r) == 0) WR8(cur_monkey(r) + AE_MK_AI_STATE, 0x1E);
    }
    monkey = cur_monkey(r);
    if (RDS32(monkey + AE_MK_STATE_TIMER) < 240) {
        WR32(monkey + AE_MK_GOAL_POS, RD32(monkey + AE_MK_POS));
        WR32(monkey + AE_MK_GOAL_POS + 8, RD32(monkey + AE_MK_POS + 8));
        WR32(monkey + AE_MK_GOAL_POS + 4, RD32(monkey + AE_MK_POS + 4) - 3000u);
    } else {
        int32_t dist = ae_DistanceXZ(r, monkey + AE_MK_POS, monkey + AE_MK_GOAL_POS);
        WR32(cur_monkey(r) + AE_MK_DIST_XZ_TO_GOAL, dist);
        state1e_maybe_flip_direction(r);
        monkey = cur_monkey(r);
        if (RDS8(AE_G_BYTES_EC08 + RD8(monkey + AE_MK_SLOT)) == 1) {
            int32_t radius = RDS16(monkey + AE_MK_CIRCLE_RADIUS);
            int32_t target_dist = ae_DistanceXZ(r, monkey + AE_MK_CIRCLE_CENTER, RD32(AE_G_MONKEY_TARGET) + AE_MK_POS);
            if (target_dist < radius) state1e_chase_goal(r, h);
            else state1e_orbit_goal(r, h);
        } else {
            state1e_hover_goal(r);
        }
    }

    /* move toward the goal */
    ae_MonkeyCalcMoveDir(r);
    ae_MonkeyMoveStep(r, h);
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    if (!((int32_t)RD32(monkey + AE_MK_DIST_XZ_TO_GOAL) < 101)) {
        int32_t step_x = RDS16(monkey + AE_MK_VEL), step_z = RDS16(monkey + AE_MK_VEL + 4);
        uint32_t x = RD32(monkey + AE_MK_POS);
        uint32_t z = RD32(monkey + AE_MK_POS + 8);
        WR32(monkey + AE_MK_POS, x + (uint32_t)step_x);
        WR32(monkey + AE_MK_POS + 8, z + (uint32_t)step_z);
    }
    monkey = cur_monkey(r);
    if (RDS32(monkey + AE_MK_STATE_TIMER) < 240) {
        WR32(monkey + AE_MK_POS + 4, RD32(monkey + AE_MK_POS + 4) - 5u);
    } else {
        /* bobbing: y += vel y + ((30 cos(f / 80) / 4096 + 60) * (cos(f / 40) - cos((f - 1) / 40))) / 4096 with f = the frame counter << 12 */
        uint32_t frame = RD32(AE_G_FRAME_COUNTER);
        int32_t slow = ae_sdk_rcos(r, (int32_t)((frame << 12) / 80u));
        frame = RD32(AE_G_FRAME_COUNTER);
        int32_t fast = ae_sdk_rcos(r, (int32_t)((frame << 12) / 40u));
        frame = RD32(AE_G_FRAME_COUNTER);
        int32_t fast_prev = ae_sdk_rcos(r, (int32_t)(((frame - 1u) << 12) / 40u));
        monkey = cur_monkey(r);
        int32_t vel_y = RDS16(monkey + AE_MK_VEL + 2);
        int32_t amplitude = add32(mul_lo(slow, 30) / 4096, 60);
        int32_t bob = mul_lo(amplitude, sub32(fast, fast_prev)) / 4096;
        WR32(monkey + AE_MK_POS + 4, RD32(monkey + AE_MK_POS + 4) + (uint32_t)add32(vel_y, bob));
    }

    /* stay above the ground */
    monkey = cur_monkey(r);
    int32_t ground = AE_MK_HOOK(r, h, GetGroundHeight, RD32(monkey + AE_MK_POS), RD32(monkey + AE_MK_POS + 4), RD32(monkey + AE_MK_POS + 8), 0);
    if (!((int32_t)RD32(monkey + AE_MK_POS + 4) < ground)) {
        uint32_t m = cur_monkey(r);
        int32_t ground_here = AE_MK_HOOK(r, h, GetGroundHeight, RD32(m + AE_MK_POS), RD32(m + AE_MK_POS + 4), RD32(m + AE_MK_POS + 8), 0);
        WR32(monkey + AE_MK_POS + 4, ground_here);
    }
    monkey = cur_monkey(r);
    if (RDS32(monkey + AE_MK_STATE_TIMER) < 240) ae_MonkeyTurnTowardTarget(r, 113);
    if (ae_ram_failed(r)) return;

    /* attack */
    monkey = cur_monkey(r);
    if (RDS8(monkey + AE_MK_PATROL_COUNTER) > 0) {
        int32_t kind = RDS16(AE_G_ATTACK_KIND + RD8(monkey + AE_MK_ID) * 2u);
        if (kind != 100) ae_MonkeyFireProjectileByKind(r, h, kind);
    }
    if (ae_MonkeyAnimFinished(r) != 0) return;
    if (((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 23) & 1u) == 0) {
        ae_MonkeyPickAnim(r);
        return;
    }
    int32_t picked = ae_MonkeyPickAttackC(r, h);
    int32_t kind = s16((uint32_t)picked);
    if (kind == 100) {
        ae_MonkeyPickAnim(r);
        return;
    }
    WR16(AE_G_ATTACK_KIND + RD8(cur_monkey(r) + AE_MK_ID) * 2u, picked);       /* delay slot of the jal */
    ae_MonkeyFireProjectileByKind(r, h, kind);
}

/* MonkeyAi_State1E_20 @8003CD84: the flying monkey of the special area (states 0x1E / 0x20).  +0x230 counts frames: below 30 only
   the flag bit 31 is set; below 0x191 hit points (+0x8E) the monkey is dying (flags bit 24: it flies to its goal, then
   SelectNextNode + the defeat burst; else it picks a random neighbour node first); otherwise it circles the target (below 240
   frames: rises over the start position; later: random hover points on a circle around the area centre +0x234 / +0x23C, flying
   in a sine pattern near the target), then moves, follows the ground height, turns toward the target and attacks. */
void ae_MonkeyAi_State1E_20(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    WR32(monkey + AE_MK_STATE_TIMER, RD32(monkey + AE_MK_STATE_TIMER) + 1u);
    WR32(monkey + AE_MK_FLAGS3, RD32(monkey + AE_MK_FLAGS3) & 0xFFFFFFFBu);
    if (RDS32(monkey + AE_MK_STATE_TIMER) < 30) {
        WR32(monkey + AE_MK_FLAGS, RD32(monkey + AE_MK_FLAGS) | 0x80000000u);
        return;
    }
    int32_t health = RDS16(monkey + AE_MK_HEALTH);
    WR32(monkey + AE_MK_FLAGS, RD32(monkey + AE_MK_FLAGS) & 0x7FFFFFFFu);
    if (health < 1000) ae_MonkeyDebrisBurst1(r, h);

    monkey = cur_monkey(r);
    if (RDS16(monkey + AE_MK_HEALTH) < 401) {
        if (RD8(monkey + AE_MK_FLAGS + 3) & 1u) {
            if (state1e_death_flight(r, h)) return;
        } else {
            state1e_start_death_flight(r, h);
        }
    } else {
        state1e_fly_and_attack(r, h);
    }

    /* the monkey's position is published for the boss camera */
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    WR16(AE_G_BOSS_POS_XZ, RD16(monkey + AE_MK_POS));
    WR16(AE_G_BOSS_POS_XZ + 2, RD16(monkey + AE_MK_POS + 8));
}

/* ================================================================================================
 * MonkeyInit
 * ============================================================================================== */

/* MonkeyInit @8003A414(id, activity, slot): initialises the current monkey record.  An already-active record (+0x7A == 1) only takes the
   new activity (and registers its collision body for activity 4); activity 2 only stores itself.  Otherwise the whole record is
   reset (sound handles -1, vectors, shadow size 80, animation table from the area header +0x18), a start node chosen (random node,
   or from the patrol route table of the monkey's id when +0xD6 is non-zero, the table entry -1 meaning random), the history
   filled with it, the monkey placed on the node (unless it is a bone node and the saved activity is 5; a boss arena starts at a
   random link instead), the goal vector set up (nodes with flag 0x400: the first link or the node position + 100; else the
   node position, the monkey moved by -2 in x / z; nodes with flag 0x40 are only approached by 500) and finally ArriveAtNode +
   LoadLinkExtra + PatrolDecide, the ground height (+ApplyMoveDir, LOD reset to 255, SelectLod) and state 0x0B for nodes with
   flag 0x40 and in boss arenas.  The word at +0xC8 receives the uninitialised stack word at sp + 28 (the original copies four words
   from sp + 16, NavNodeGetPos writes three). */
void ae_MonkeyInit(AeRam *r, const AeMonkeyHooks *h, uint32_t id, uint32_t activity, uint32_t slot) {
    uint32_t sp = AE_STK(AE_STK_MONKEY_INIT);
    uint32_t monkey, node;
    int32_t i;

    monkey = cur_monkey(r);
    WR8(monkey + AE_MK_ID, id);
    monkey = cur_monkey(r);
    WR8(monkey + AE_MK_SLOT, slot);
    monkey = cur_monkey(r);
    uint32_t previous_activity = RD8(monkey + AE_MK_ACTIVITY);
    WR8(monkey + AE_MK_ACTIVITY, activity);
    if (previous_activity == 1) {
        /* already active: only the collision body registration for the new activity */
        if ((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 30) & 1u) return;
        if ((activity & 0xFFu) != 4) return;
        ae_MonkeyBodyRegister(r, h);
        return;
    }
    if ((activity & 0xFFu) == 2) return;

    /* ---- reset the record */
    monkey = cur_monkey(r);
    WR32(monkey + AE_MK_RESET_154, 0xFFFFFFFFu);
    WR32(monkey + 0x15C, 0xFFFFFFFFu);
    WR32(monkey + 0x160, 0xFFFFFFFFu);
    uint32_t flags = RD32(monkey + AE_MK_FLAGS);
    WR16(monkey + AE_MK_TIMER_158, 0);
    WR16(monkey + AE_MK_REACH_RADIUS, 200);
    WR16(monkey + 0x4E, 0);
    WR8(monkey + AE_MK_ALERTED, 0);
    WR32(monkey + AE_MK_FLAGS, flags & 0xFFDFFFFFu);
    monkey = cur_monkey(r);
    WR16(monkey + 0x8C, 100);
    WR16(monkey + AE_MK_HEALTH, 1000);
    WR16(monkey + AE_MK_MOVE_DIR, 0x1000);
    WR16(monkey + AE_MK_WISH_DIR, 0x1000);
    WR16(monkey + 0x168, 80);                   /* shadow size */
    WR16(monkey + AE_MK_MOVE_DIR + 2, 0);
    WR16(monkey + AE_MK_MOVE_DIR + 4, 0);
    WR16(monkey + AE_MK_WISH_DIR + 2, 0);
    WR16(monkey + AE_MK_WISH_DIR + 4, 0);
    WR16(monkey + AE_MK_ROT, 0);
    WR16(monkey + AE_MK_ROT + 2, 0);
    WR16(monkey + AE_MK_ROT + 4, 0);
    WR8(monkey + AE_MK_GROUP, 0);
    WR16(monkey + AE_MK_THROW_DAMP, 200);
    WR8(cur_monkey(r) + AE_MK_GROUP + 1, 0);
    monkey = cur_monkey(r);
    uint32_t header = RD32(AE_G_AREA_HEADER);
    uint32_t area_data = RD32(AE_G_AREA_DATA);
    uint32_t anim_table_ofs = RD32(header + 0x18);
    uint32_t route_count = RD8(monkey + AE_MK_ROUTE_COUNT);
    WR32(monkey + AE_MK_ANIM_TABLE, area_data + anim_table_ofs);

    /* ---- the start node: random, or from the patrol route table of the monkey (-1 = random) */
    int32_t start_node;
    if (route_count == 0) {
        uint32_t roll = ae_MonkeyRand(r) & 0xFFFFu;
        start_node = rem_trap(r, (int32_t)roll, RDS16(AE_G_NAV_NODE_COUNT));
        if (ae_ram_failed(r)) return;
    } else {
        uint32_t roll = ae_MonkeyRand(r) & 0xFFFFu;
        int32_t route_choice = rem_trap(r, (int32_t)roll, (int32_t)RD8(cur_monkey(r) + AE_MK_ROUTE_COUNT));
        if (ae_ram_failed(r)) return;
        int32_t route_monkey = rem_trap(r, s16(id), RDS16(AE_G_MONKEY_COUNT));
        if (ae_ram_failed(r)) return;
        start_node = RDS16(RD32(AE_G_ROUTE_TABLE) + (uint32_t)route_choice * 2u + (uint32_t)route_monkey * 16u + 8);
    }
    if (start_node == -1) {
        uint32_t roll = ae_MonkeyRand(r) & 0xFFFFu;
        start_node = rem_trap(r, (int32_t)roll, RDS16(AE_G_NAV_NODE_COUNT));
        if (ae_ram_failed(r)) return;
    }
    monkey = cur_monkey(r);
    for (i = 0; i < 6; i++) WR16(monkey + AE_MK_NAV_NODE0 + (uint32_t)i * 2u, start_node);       /* the node history */

    monkey = cur_monkey(r);
    ae_MonkeyArriveAtNode(r, h, RDS16(monkey + AE_MK_NAV_NODE0));
    if (ae_ram_failed(r)) return;
    ae_NavNodeGetPos(r, h, (int32_t)((uint32_t)start_node & 0xFFFFu), sp + 16);
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    uint32_t next_flags = RD32(node_of(r, RD16(monkey + AE_MK_NAV_NODE1)));
    if ((next_flags & AE_NAV_FLAG_BONE) == 0 || RD8(AE_G_MONKEY_ACTIVITY + RD8(monkey + AE_MK_ID)) != 5) {
        WR32(monkey + AE_MK_POS, RD32(sp + 16));
        WR32(monkey + AE_MK_POS + 4, RD32(sp + 20));
        WR32(monkey + AE_MK_POS + 8, RD32(sp + 24));
    }
    monkey = cur_monkey(r);
    for (i = 0; i < 6; i++) WR16(monkey + AE_MK_NAV_NODE0 + (uint32_t)i * 2u, start_node);
    if (ae_AreaIsBossArena(r) != 0) {
        /* a boss arena starts at a random link of the node */
        uint32_t roll = ae_MonkeyRand(r) & 0xFFFFu;
        node = RD16(cur_monkey(r) + AE_MK_NAV_NODE0);
        uint32_t link_count = RD8(node_of(r, node) + 0xB);
        int32_t pick = rem_trap(r, (int32_t)roll, (int32_t)link_count);
        if (ae_ram_failed(r)) return;
        int32_t link = ae_NavNodeGetLink(r, s16(node), pick);
        if (ae_ram_failed(r)) return;
        WR16(cur_monkey(r) + AE_MK_NAV_NODE0, link);
    }
    if (!((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 30) & 1u) && (activity & 0xFFu) == 4) ae_MonkeyBodyRegister(r, h);

    /* ---- the rest of the record */
    monkey = cur_monkey(r);
    uint32_t anim_set = RD32(AE_G_ANIM_SET);
    WR32(monkey + 0x50, 0);
    WR16(monkey + AE_MK_TIMER_90, 1);
    WR16(monkey + 0x66, 2);
    WR32(monkey + 0x18, anim_set);              /* delay slot of the jal */
    ae_MonkeyPickAnim(r);
    monkey = cur_monkey(r);
    WR16(monkey + 0x44, 64);
    uint32_t state_flags = STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE));
    WR32(monkey + AE_MK_VEL32, 0);
    WR32(monkey + AE_MK_VEL32 + 4, 0);
    WR32(monkey + AE_MK_VEL32 + 8, 0);
    uint32_t body_height = 0x100u;
    if (((state_flags >> 4) & 1u) != 0 && ae_AreaIsBossArena(r) == 0) body_height = 0xFFFFFF00u;
    WR16(cur_monkey(r) + AE_MK_BODY_HEIGHT, body_height);
    if (ae_AreaIsBossArena(r) != 0 || (RD32(cur_monkey(r) + AE_MK_FLAGS2) & 0x02000000u)) WR16(cur_monkey(r) + 0x4A, 0xC000);
    else WR16(cur_monkey(r) + 0x4A, 0x8000);

    monkey = cur_monkey(r);
    flags = RD32(monkey + AE_MK_FLAGS);
    WR16(monkey + 0x48, 0);
    WR16(monkey + AE_MK_THROW_TIMER, 0);
    WR16(monkey + AE_MK_GOAL_AGE, 0);
    WR16(monkey + AE_MK_NAV_MARK_COUNT, 0);
    WR16(monkey + AE_MK_NOTICE_COUNT, 0);
    node = RD16(monkey + AE_MK_NAV_NODE0);
    uint32_t nodes = RD32(AE_G_NAV_NODES);
    WR16(monkey + AE_MK_PREV_NODE, 0xFFFFu);
    WR32(monkey + AE_MK_FLAGS, flags & 0x0FFCFFFFu);       /* clears the bits 16 / 17 and 28 - 31 */

    /* ---- the goal vector */
    if (RD32(node * AE_NAV_NODE_SIZE + nodes) & AE_NAV_FLAG_BONE) {
        uint32_t next_node = RD16(monkey + AE_MK_NAV_NODE1);
        if (RD8(next_node * AE_NAV_NODE_SIZE + nodes + 0xB) == 1) {
            /* a single link: the monkey faces its first link */
            int32_t link = ae_NavNodeGetLink(r, s16(node), 0);
            if (ae_ram_failed(r)) return;
            WR16(cur_monkey(r) + AE_MK_NAV_NODE0, link);
            ae_NavNodeGetPos(r, h, (int32_t)((uint32_t)link & 0xFFFFu), sp + 16);
            if (ae_ram_failed(r)) return;
        } else {
            WR32(sp + 16, RD32(sp + 16) + 100u);
            WR32(sp + 24, RD32(sp + 24) + 100u);
            WR32(sp + 20, RD32(sp + 20) + 100u);
        }
        monkey = cur_monkey(r);
        WR32(monkey + AE_MK_GOAL_POS, RD32(sp + 16));
        WR32(monkey + AE_MK_GOAL_POS + 4, RD32(sp + 20));
        WR32(monkey + AE_MK_GOAL_POS + 8, RD32(sp + 24));
        WR32(monkey + AE_MK_GOAL_POS + 12, RD32(sp + 28));
        ae_MonkeyUpdateTargetDistance(r, h);
    } else {
        ae_NavNodeGetPos(r, h, (int32_t)node, sp + 16);
        if (ae_ram_failed(r)) return;
        monkey = cur_monkey(r);
        WR32(monkey + AE_MK_GOAL_POS, RD32(sp + 16));
        WR32(monkey + AE_MK_GOAL_POS + 4, RD32(sp + 20));
        WR32(monkey + AE_MK_GOAL_POS + 8, RD32(sp + 24));
        WR32(monkey + AE_MK_GOAL_POS + 12, RD32(sp + 28));
        WR32(monkey + AE_MK_POS, RD32(monkey + AE_MK_POS) - 2u);
        WR32(monkey + AE_MK_POS + 8, RD32(monkey + AE_MK_POS + 8) - 2u);
        if (RD32(node_of(r, RD16(monkey + AE_MK_NAV_NODE0))) & AE_NAV_FLAG_REF_POS) {
            /* a reference position node: only approached by 500 */
            WR32(monkey + AE_MK_GOAL_POS, RD32(monkey + AE_MK_POS) + 500u);
            WR32(monkey + AE_MK_GOAL_POS + 4, RD32(monkey + AE_MK_POS + 4));
            WR32(monkey + AE_MK_GOAL_POS + 8, RD32(monkey + AE_MK_POS + 8) + 500u);
            ae_MonkeyUpdateTargetDistance(r, h);
        } else {
            ae_MonkeyUpdateTargetDistance(r, h);
            ae_MonkeySelectNextNode(r, h, 0, 1);
        }
    }

    /* ---- arrive at the node, decide, stand on the ground */
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    ae_MonkeyArriveAtNode(r, h, RDS16(monkey + AE_MK_NAV_NODE1));
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    ae_MonkeyLoadLinkExtra(r, RDS16(monkey + AE_MK_NAV_NODE1), RDS16(monkey + AE_MK_NAV_NODE0));
    if (ae_ram_failed(r)) return;
    ae_MonkeyPatrolDecide(r, h);
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    if ((RD32(monkey + AE_MK_FLAGS2) & 2u) == 0) {
        if ((RD32(node_of(r, RD16(monkey + AE_MK_NAV_NODE0))) & AE_NAV_FLAG_BONE) == 0) {
            uint32_t x = RD32(monkey + AE_MK_POS);
            uint32_t y = RD32(monkey + AE_MK_POS + 4);
            uint32_t z = RD32(monkey + AE_MK_POS + 8);
            int32_t ground = AE_MK_HOOK(r, h, GetGroundHeight, x, y - 200u, z, 0);
            WR32(cur_monkey(r) + AE_MK_POS + 4, ground);
        }
    }
    ae_MonkeyApplyMoveDir(r);
    WR8(cur_monkey(r) + 0xD5, 255);             /* the LOD byte, delay slot of the jal */
    ae_MonkeySelectLod(r, h);
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    if (RD32(node_of(r, RD16(monkey + AE_MK_NAV_NODE0))) & AE_NAV_FLAG_REF_POS) ae_MonkeySetState(r, 11);
    if (ae_AreaIsBossArena(r) != 0) ae_MonkeySetState(r, 11);
}
