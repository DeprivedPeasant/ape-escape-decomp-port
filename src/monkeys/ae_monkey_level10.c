/*
 * Monkey code, batch 5c (call-graph levels 10 - 12): MonkeySpawnAll, MonkeyAiStateUpdate (level 10), MonkeyThinkActive (level 11),
 * MonkeyUpdateAll (level 12), ported from the original instructions (see ae_monkey.h and notes/port/monkeys-port.md).  Where the
 * Ghidra pseudocode and the listing differ, the listing was followed.
 */
#include "ae_monkey_inl.h"

#define STATE_FLAGS(st) RD32(AE_G_MONKEY_STATE_TABLE + (uint32_t)(st) * 8u + 4u)
#define AE_G_FRAME_COUNTER_EB5C 0x800AEB5Cu  /* u32: counter MonkeyUpdateAll increments (boss arenas copy it to the frame counter) */
#define AE_G_STATE_BASE_FRAME 0x800EC350u    /* u32: the frame counter the non-boss areas copy to 0x800AEB60 */
#define AE_G_MONKEY_AREA_SECTION 0x800F4500u /* u32: area data pointer written by MonkeySpawnAll (header +0x14) */

/* ================================================================================================
 * MonkeyAiStateUpdate
 * ============================================================================================== */

/* states 5 / 0x41, the thrown-away flight: a monkey whose animation has not ended gets the timer for state 0x25 (5 frames for state 5, else
   40) and switches to 0x25; when the animation has ended the velocity (+0x70 / +0x72 / +0x74) is damped by s16(+0x166) / 32768 and the
   flight velocity +0x30 / +0x38 is set to -velocity * max(0, (400 - t^2) / 250) with t = the counter at +0x92 (incremented). */
static void ai_state_thrown(AeRam *r) {
    uint32_t monkey;
    if (ae_MonkeyAnimFinished(r) == 0) {
        monkey = cur_monkey(r);
        uint32_t state = RD8(monkey + AE_MK_AI_STATE);
        WR16(monkey + AE_MK_THROW_TIMER, state == 5 ? 5u : 40u);
        ae_MonkeySetState(r, 0x25);
        return;
    }
    monkey = cur_monkey(r);
    int32_t damp = RDS16(monkey + AE_MK_THROW_DAMP);
    WR16(monkey + AE_MK_THROW_TIMER, RD16(monkey + AE_MK_THROW_TIMER) + 1u);
    WR16(monkey + AE_MK_VEL, mul_lo(RDS16(monkey + AE_MK_VEL), damp) / 32768);
    WR16(monkey + AE_MK_VEL + 2, mul_lo(RDS16(monkey + AE_MK_VEL + 2), damp) / 32768);
    WR16(monkey + AE_MK_VEL + 4, mul_lo(RDS16(monkey + AE_MK_VEL + 4), damp) / 32768);
    int32_t t = RDS16(monkey + AE_MK_THROW_TIMER);
    int32_t strength = sub32(400, mul_lo(t, t)) / 250;
    if (strength < 0) strength = 0;
    strength = s16((uint32_t)strength);
    WR32(monkey + AE_MK_VEL32, mul_lo(-RDS16(monkey + AE_MK_VEL), strength));
    WR32(monkey + AE_MK_VEL32 + 8, mul_lo(-RDS16(monkey + AE_MK_VEL + 4), strength));
}

/* state 0x25: waits the timer at +0x92 out, then (flags2 bit 15 set) state 0x2F or back to state 6 at the current node */
static void ai_state_25(AeRam *r, const AeMonkeyHooks *h, uint32_t sp) {
    uint32_t monkey = cur_monkey(r);
    uint32_t timer = RD16(monkey + AE_MK_THROW_TIMER);
    WR16(monkey + AE_MK_THROW_TIMER, timer - 1u);
    if (s16(timer) > 0) return;
    if (RD32(monkey + AE_MK_FLAGS2) & 0x8000u) {
        ae_MonkeyEnterState2F(r, h);
        return;
    }
    ae_NavNodeGetPos(r, h, (int32_t)RD16(monkey + AE_MK_NAV_NODE0), sp + 16);
    if (ae_ram_failed(r)) return;
    WR8(cur_monkey(r) + AE_MK_AI_STATE, 6);
    monkey = cur_monkey(r);
    WR32(monkey + AE_MK_GOAL_POS, RD32(sp + 16));
    WR32(monkey + AE_MK_GOAL_POS + 4, RD32(sp + 20));
    WR32(monkey + AE_MK_GOAL_POS + 8, RD32(sp + 24));
    WR32(monkey + AE_MK_GOAL_POS + 12, RD32(sp + 28));
    ae_MonkeyPickAnim(r);
}

/* state 0x33: at animation step 7 a particle puff (1 in 10); the monkey is pulled to the next state by the target distance (nearer than three
   times the MonkeyInfo range +0x1A) or after the counter (low byte of the flags word) passed the limits at +0x244 / +0x246 */
static void ai_state_33(AeRam *r, const AeMonkeyHooks *h, uint32_t sp) {
    uint32_t monkey = cur_monkey(r);
    if (RDS16(monkey + AE_MK_ANIM_STATE_FRAME) == 7 && (ae_MonkeyRand(r) & 0xFFFFu) % 10u == 0) {
        for (uint32_t i = 0; i < 8; ++i) WR8(sp + 72u + i, 0);        /* memset(sp + 72, 0, 8) */
        monkey = cur_monkey(r);
        WR16(sp + 72, RD16(monkey + AE_MK_POS));
        WR16(sp + 74, RD16(monkey + AE_MK_POS + 4) - 32u);
        WR16(sp + 76, RD16(monkey + AE_MK_POS + 8));
        uint32_t w0 = RD32(sp + 72), w1 = RD32(sp + 76);              /* lwl / lwr pairs, aligned */
        WR32(sp + 64, w0);
        WR32(sp + 68, w1);
        AE_MK_HOOK(r, h, SpawnParticle, 272, sp + 64, 0, 0);
    }
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    int32_t range = RDS16(info_of(r, monkey) + 0x1A);
    int32_t dist = (int32_t)RD32(monkey + AE_MK_DIST_TO_TARGET);
    if (dist >= add32(L(range, 1), range)) {
        if (ae_MonkeyAnimFinished(r) != 0) return;
        monkey = cur_monkey(r);
        WR8(monkey + AE_MK_FLAGS, RD8(monkey + AE_MK_FLAGS) + 1u);       /* the frame counter lives in the low byte of +0x248 */
        monkey = cur_monkey(r);
        int32_t counter = (int32_t)RD8(monkey + AE_MK_FLAGS);
        int32_t limit_hi = RDS16(monkey + 0x246);
        if (counter < limit_hi || !(RDS16(monkey + 0x244) < limit_hi)) {
            ae_MonkeyPickAnim(r);
            return;
        }
    }
    ae_MonkeyEnterState_34(r);
}

/* the MonkeyAiStateUpdate switch: each state runs the handler the original dispatches to through the table at 0x800A4AB0 (the table maps the
   states to 24 code blocks; the case groups below are those blocks) */
static void ai_run_state(AeRam *r, const AeMonkeyHooks *h, uint32_t sp, uint32_t s2_in, uint32_t state) {
    uint32_t monkey;
    switch (state) {
    case 0x02: case 0x03: case 0x08: case 0x09: case 0x0A: case 0x0C:
    case 0x19: case 0x1A: case 0x23: case 0x24: case 0x35: case 0x39:
        if (RD8(cur_monkey(r) + AE_MK_ALERTED) != 1) ae_MonkeyAi_MoveUnalerted(r, h);
        else ae_MonkeyAi_MoveAlerted(r, h);
        break;
    case 0x00: case 0x01: case 0x0B: case 0x16: case 0x17: case 0x18: case 0x1B: case 0x1C: case 0x21: case 0x22:
    case 0x26: case 0x27: case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x37: case 0x38: case 0x42:
        ae_MonkeyAi_Patrol(r, h);
        break;
    case 0x0D: case 0x10: case 0x13: case 0x3A:
        ae_MonkeyAi_IdleActivityStart(r, h);
        break;
    case 0x0E: case 0x11: case 0x14: case 0x3B:
        ae_MonkeyAi_IdleActivityLoop(r, h);
        break;
    case 0x0F: case 0x12: case 0x15: case 0x3C:
        ae_MonkeyAi_IdleActivityEnd(r, h);
        break;
    case 0x04: case 0x1D:
        ae_MonkeyAi_JumpArc(r, h);
        break;
    case 0x2C: case 0x2D: case 0x2E:
        ae_MonkeyAi_JumpArcPhased(r, h);
        break;
    case 0x31:
        ae_MonkeyAi_JumpArcB(r, h);
        break;
    case 0x1E: case 0x20:
        ae_MonkeyAi_State1E_20(r, h);
        break;
    case 0x36:
        if (ae_MonkeyAnimFinished(r) == 0) ae_MonkeyPatrolDecide(r, h);
        break;
    case 0x07:
        ae_MonkeyAi_State07(r, h);
        break;
    case 0x32:                                  /* waits for the end of the animation, then 0x33 */
        if (ae_MonkeyAnimFinished(r) != 0) break;
        monkey = RD32(AE_G_CUR_MONKEY);
        WR8(monkey + AE_MK_AI_STATE, 0x33);
        monkey = RD32(AE_G_CUR_MONKEY);
        WR16(monkey + AE_MK_ANIM_STATE_FRAME, RD16(monkey + AE_MK_ANIM_STATE_FRAME) + 1u);
        ae_MonkeyPickAnim(r);
        break;
    case 0x33:
        ai_state_33(r, h, sp);
        break;
    case 0x34:
        if (ae_MonkeyAnimFinished(r) != 0) break;
        ae_MonkeyDecideAction(r, h);
        ae_MonkeyApplyMoveDir(r);
        break;
    case 0x3D:
        ae_MonkeyAi_State3D(r, h);
        break;
    case 0x3E:
        ae_MonkeyAi_State3E(r);
        break;
    case 0x3F:
        if (ae_MonkeyAnimFinished(r) != 0) break;
        ae_MonkeyDecideAction(r, h);
        break;
    case 0x05: case 0x41:
        ai_state_thrown(r);
        break;
    case 0x25:
        ai_state_25(r, h, sp);
        break;
    case 0x40:
        if (ae_MonkeyAnimFinished(r) != 0) break;
        monkey = cur_monkey(r);
        if (RD32(monkey + AE_MK_FLAGS2) & 0x8000u) ae_MonkeyEnterState2F(r, h);
        else if (RD8(monkey + AE_MK_ALERTED) == 0) ae_MonkeyResumeIdle(r);
        else ae_MonkeyDecideAction(r, h);
        break;
    case 0x2F:
        ae_MonkeyAi_Fall(r, h);
        break;
    case 0x30:
        ae_MonkeyAi_GoToFlaggedNode(r, h, s2_in);
        break;
    case 0x06:
        if (ae_MonkeyAnimFinished(r) != 0) break;
        if (RD8(cur_monkey(r) + AE_MK_ALERTED) != 0) ae_MonkeyDecideAction(r, h);
        else ae_MonkeyResumeIdle(r);
        break;
    default:                                    /* 0x1F: no handler */
        break;
    }
}

/* the animation set / table for the state: states 0x32..0x34 use the second animation set, the others the normal one (re-picked on a change) */
static void ai_update_anim_set(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_AI_STATE) - 50u < 3u) {
        WR32(monkey + 0x18, RD32(AE_G_ANIM_SET2));
        uint32_t header = RD32(AE_G_AREA_HEADER);
        WR32(monkey + AE_MK_ANIM_TABLE, RD32(AE_G_AREA_DATA) + RD32(header + 0x1C));
        return;
    }
    uint32_t anim_set = RD32(AE_G_ANIM_SET);
    if (RD32(monkey + 0x18) == anim_set) return;
    WR32(monkey + 0x18, anim_set);
    uint32_t header = RD32(AE_G_AREA_HEADER);
    WR32(monkey + AE_MK_ANIM_TABLE, RD32(AE_G_AREA_DATA) + RD32(header + 0x18));
    ae_MonkeyPickAnim(r);
}

/* the collision body is switched off at flagged nodes (flags2 bit 14, node flag 0x10) and in state 29, else back on */
static void ai_update_body_switch(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    int body_off = (RD32(monkey + AE_MK_FLAGS2) & 0x4000u) != 0 ||
                   (RD32(node_of(r, RD16(monkey + AE_MK_NAV_NODE1))) & 0x10u) != 0 ||
                   RD8(monkey + AE_MK_AI_STATE) == 29;
    uint32_t disabled = (RD32(cur_monkey(r) + AE_MK_FLAGS) >> 30) & 1u;
    if (body_off) {
        if (!disabled) ae_MonkeyBodyOff(r, h);
    } else {
        if (disabled) ae_MonkeyBodyOn(r, h);
    }
}

/* the height of the monkey: unless it is jumping or at a bone / reference node it stands on the ground below it (flags2 bit 1 clear) or at the
   waypoint height (flags2 bit 1 set: +256 for the attack states with flags2 0x20020, the boss arena state 26 uses waypoint 1) */
static void ai_update_height(AeRam *r, const AeMonkeyHooks *h) {
    if (ae_MonkeyIsJumpArcState(r) != 0) return;
    uint32_t monkey = cur_monkey(r);
    uint32_t nodes = RD32(AE_G_NAV_NODES);
    if (RD32(RD16(monkey + AE_MK_NAV_NODE1) * AE_NAV_NODE_SIZE + nodes) & AE_NAV_FLAG_BONE) return;
    uint32_t node_flags = RD32(RD16(monkey + AE_MK_NAV_NODE0) * AE_NAV_NODE_SIZE + nodes);
    if (node_flags & AE_NAV_FLAG_REF_POS) return;
    uint32_t state = RD8(monkey + AE_MK_AI_STATE);
    if (state == 30 || state == 32 || state - 47u < 2u) return;
    uint32_t flags2 = RD32(monkey + AE_MK_FLAGS2);
    uint32_t bone_here = node_flags & AE_NAV_FLAG_BONE;
    int32_t y;
    if ((flags2 & 2u) == 0) {
        if (bone_here != 0) return;
        y = AE_MK_HOOK(r, h, GetGroundHeight, RD32(monkey + AE_MK_POS), RD32(monkey + AE_MK_POS + 4), RD32(monkey + AE_MK_POS + 8), 0);
    } else {
        if (ae_AreaIsBossArena(r) != 0 && RD8(cur_monkey(r) + AE_MK_AI_STATE) == 26) {
            y = ae_MonkeyGetWaypointPos(r, h, 1);
        } else if ((RD32(cur_monkey(r) + AE_MK_FLAGS2) & 0x20020u) != 0 && ae_MonkeyIsAttackState(r) != 0) {
            y = ae_MonkeyGetWaypointPos(r, h, 0);
            WR32(cur_monkey(r) + AE_MK_POS + 4, (uint32_t)y + 256u);
            return;
        } else {
            y = ae_MonkeyGetWaypointPos(r, h, 0);
        }
    }
    WR32(cur_monkey(r) + AE_MK_POS + 4, y);
}

/* contact handling of the active (activity 4) monkeys outside boss arenas: net catch (area kind 8 with the stage exit active), the state 0x1E
   monkey of kind 11 after 240 frames, and the knockdown by what hit it (ActorContactOwnerType) */
static void ai_update_contact(AeRam *r, const AeMonkeyHooks *h) {
    if (ae_MonkeyUpdateContact(r) == 0) return;
    uint32_t monkey = cur_monkey(r);
    uint32_t state = RD8(monkey + AE_MK_AI_STATE);
    if (state == 32) return;
    if (state == 30 && RDS32(monkey + AE_MK_STATE_TIMER) >= 241) {
        if (RD16(monkey + AE_MK_KIND) != 11) return;
        ae_MonkeyEnterState_20(r, h);
        monkey = cur_monkey(r);
        WR16(monkey + AE_MK_HEALTH, RD16(monkey + AE_MK_HEALTH) - 200u);
        return;
    }
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    if (RD16(monkey + AE_MK_KIND) == 8 && AE_MK_HOOK(r, h, StageExitIsActive, 0, 0, 0, 0) != 0) {
        /* caught with the net: the monkey is taken out of the game */
        WR8(cur_monkey(r) + AE_MK_ACTIVITY, 3);
        ae_MonkeyBodyUnregister(r, h);
        WR16(AE_G_CAUGHT_MONKEY, RD8(cur_monkey(r) + AE_MK_ID));
        return;
    }
    monkey = cur_monkey(r);
    if (RD16(monkey + AE_MK_KIND) != 11) return;
    if (RD8(monkey + AE_MK_STUN_TIMER) != 0) return;
    uint32_t flags2 = RD32(monkey + AE_MK_FLAGS2);
    if (flags2 & 0x80000u) return;
    if ((flags2 & 0x8000u) == 0) {
        if (RD32(node_of(r, RD16(monkey + AE_MK_NAV_NODE0))) & AE_NAV_FLAG_BONE) return;
        state = RD8(monkey + AE_MK_AI_STATE);
        if (state == 26 || state == 53 || state == 9) return;
        if (ae_MonkeyIsState0C_or_24(r) != 0) return;
    }
    uint32_t attack_kind = STATE_FLAGS(RD8(cur_monkey(r) + AE_MK_AI_STATE)) & 3u;
    if (attack_kind == 1) {
        ae_MonkeySetState(r, 0x40);
        return;
    }
    if ((int32_t)attack_kind < 2) return;
    int32_t owner = s16((uint32_t)ae_ActorContactOwnerType(r));
    if (ae_ram_failed(r)) return;
    int32_t knockdown;
    uint32_t damage;
    if (owner == 32) {
        knockdown = 5;
        damage = 400;
    } else if (owner == 48) {
        knockdown = 65;
        damage = 500;
    } else {
        knockdown = 64;
        damage = 300;
    }
    monkey = cur_monkey(r);
    WR16(monkey + AE_MK_HEALTH, RD16(monkey + AE_MK_HEALTH) - damage);        /* delay slot of the jal */
    ae_MonkeyEnterKnockdown(r, h, knockdown);
}

/* the notice test: a monkey that has not noticed the target (+0x79 == 0) in a state with state table flag bit 6 and the target in sight range
   notices it when the next node is not special or (waiting / near the target) there; state 0x33 goes to 0x34, 0x34 stays, else back to idle */
static void ai_update_notice(AeRam *r) {
    uint32_t monkey = cur_monkey(r);
    if ((RD16(monkey + AE_MK_FLAGS_HI) & 3u) - 1u < 2u) {
        if (RDS16(monkey + AE_MK_NOTICE_COUNT) <= 0) {
            WR16(monkey + AE_MK_NOTICE_COUNT, RD16(monkey + AE_MK_NOTICE_COUNT) + 1u);
            return;
        }
    }
    monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_ALERTED) != 0) return;
    if (((STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) >> 6) & 1u) == 0) return;
    if (ae_MonkeyTargetInSightRange(r) == 0) return;
    int notice = 0;
    if (ae_NavNodeIsSpecial(r, RDS16(cur_monkey(r) + AE_MK_NAV_NODE1)) == 0) {
        notice = 1;
    } else {
        monkey = cur_monkey(r);
        uint32_t node_flags = RD32(node_of(r, RD16(monkey + AE_MK_NAV_NODE1)));
        if ((node_flags & AE_NAV_FLAG_WAIT) != 0 && RD16(monkey + AE_MK_NODE_WAIT) == 0) {
            notice = 1;
        } else if (node_flags & AE_NAV_FLAG_NEAR_TARGET) {
            if (ae_AreaIsBossArena(r) == 0) {
                monkey = cur_monkey(r);
                int32_t radius = RDS16(node_of(r, RD16(monkey + AE_MK_NAV_NODE1)) + 0x10);
                if ((int32_t)RD32(monkey + AE_MK_DIST_TO_TARGET) < radius) notice = 1;
            }
        }
    }
    if (!notice) return;
    uint32_t state = RD8(cur_monkey(r) + AE_MK_AI_STATE);
    if (state == 51) {
        ae_MonkeyEnterState_34(r);
        return;
    }
    if (state == 52) return;
    ae_MonkeyResumeIdle(r);
}

/* MonkeyAiStateUpdate @800412B0: the per-frame state machine of one monkey (g_CurMonkey).  `s2_in` = the caller's $s2 (the original does not
   save it and passes it on to MonkeyAi_GoToFlaggedNode in state 0x30: MonkeyThinkActive has 0x800EC1E0 in it).
     1. +0x174 counts down, flags bit 20 is cleared; nothing else happens for activities 0 / 2.
     2. the move direction (+0x94 reset outside the walking states, CalcMoveDir for states 0x0C / 0x24, ApplyMoveDir for the others),
        flags bit 25 cleared; activity 3 only counts +0x90 down, else the animation frame advances (state table flags bit 2) and the
        state's handler runs (ai_run_state).
     3. the common tail: animation set / table for the state (states 0x32..0x34 use set 2), turn toward the target (alerted, MonkeyInfo +4 == 0),
        collision body on / off by the node flags, the +0x124 timer, PickAnim when the animation ended, the ground / waypoint height, dust puffs
        in states 0x0C / 0x24, MoveStep, the +0x22C timer, collision body registration changes, the +0xE2 timer, contact handling for activity 4
        (net catch, knockdown), the removal in area 0x46 / when +0x90 is 0 (activity 2 + SelectActive), the orientation matrix of the move
        direction (RotMatrix, ApplyMatrixSV, VectorNormalSS), the copy of the move direction to +0x3C, the notice test, the animation index
        (+0x5E) and the scale (+0x46). */
void ae_MonkeyAiStateUpdate(AeRam *r, const AeMonkeyHooks *h, uint32_t s2_in) {
    uint32_t sp = AE_STK(AE_STK_AI_STATE_UPDATE);
    uint32_t monkey = cur_monkey(r);
    uint32_t timer = RD16(monkey + AE_MK_TIMER_174);
    if (timer != 0) WR16(monkey + AE_MK_TIMER_174, timer - 1u);

    /* ---- 1. start of the frame */
    monkey = cur_monkey(r);
    uint32_t state = RD8(monkey + AE_MK_AI_STATE);
    uint32_t body_flag_before = (STATE_FLAGS(state) >> 5) & 1u;     /* state table flag bit 5: the collision body is registered */
    WR32(monkey + AE_MK_FLAGS, RD32(monkey + AE_MK_FLAGS) & 0xFFEFFFFFu);
    uint32_t activity = RD8(monkey + AE_MK_ACTIVITY);
    if (activity == 0 || activity == 2) return;

    /* ---- 2. move direction and the state handler */
    int keeps_cooldown = state == 2 || ae_MonkeyIsIdleActivityState(r) != 0 || state == 3 || state == 8 || state == 10 ||
                         ae_MonkeyIsState0C_or_24(r) != 0 || state == 26 || state == 53;
    if (!keeps_cooldown) WR16(cur_monkey(r) + AE_MK_ACTION_COOLDOWN, 0);
    if (ae_ram_failed(r)) return;
    if (ae_MonkeyIsState0C_or_24(r) != 0) {
        ae_MonkeyCalcMoveDir(r);
    } else {
        monkey = cur_monkey(r);
        state = RD8(monkey + AE_MK_AI_STATE);
        if (!(RDS16(monkey + AE_MK_ACTION_COOLDOWN) > 0) && state != 0 && state != 66 &&
            ((RD32(monkey + AE_MK_FLAGS) >> 25) & 1u) == 0 && state != 7) {
            ae_MonkeyApplyMoveDir(r);
        }
    }
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    WR32(monkey + AE_MK_FLAGS, RD32(monkey + AE_MK_FLAGS) & 0xFDFFFFFFu);
    if (RD8(monkey + AE_MK_ACTIVITY) == 3) {
        WR16(monkey + AE_MK_TIMER_90, RD16(monkey + AE_MK_TIMER_90) - 1u);
    } else {
        if ((STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) >> 2) & 1u) ae_MonkeyAdvanceAnimFrame(r);
        state = RD8(cur_monkey(r) + AE_MK_AI_STATE);
        if (state < 67u) ai_run_state(r, h, sp, s2_in, state);
    }

    /* ---- 3. the common tail */
    if (ae_ram_failed(r)) return;
    ai_update_anim_set(r);
    monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_ALERTED) == 1) {
        if (RDS16(info_of(r, monkey) + 4) == 0) ae_MonkeyTurnTowardTarget(r, 0x22);
    }
    if (ae_ram_failed(r)) return;
    ai_update_body_switch(r, h);
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    int32_t timer_124 = RDS16(monkey + AE_MK_TIMER_124);
    if (timer_124 > 0) WR16(monkey + AE_MK_TIMER_124, RD16(monkey + AE_MK_TIMER_124) - 1u);
    if (ae_MonkeyAnimFinished(r) == 0) ae_MonkeyPickAnim(r);
    ai_update_height(r, h);
    if (ae_ram_failed(r)) return;
    if (ae_MonkeyIsState0C_or_24(r) != 0) {
        if (RD32(AE_G_FRAME_COUNTER) % 50u < 10u) ae_MonkeyPuff14(r, h);       /* dust puffs while running */
    }
    ae_MonkeyMoveStep(r, h);
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    uint32_t wait = RD16(monkey + AE_MK_NODE_WAIT);
    if (wait != 0) WR16(monkey + AE_MK_NODE_WAIT, wait - 1u);

    /* collision body registration follows the state table flag bit 5 */
    if (body_flag_before == 0) {
        monkey = cur_monkey(r);
        if (((STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) >> 5) & 1u) != 0 && ((RD32(monkey + AE_MK_FLAGS3) >> 1) & 1u) == 0 &&
            ((RD32(monkey + AE_MK_FLAGS) >> 30) & 1u) == 0) {
            ae_MonkeyBodyRegister(r, h);
        }
    } else {
        if (((STATE_FLAGS(RD8(cur_monkey(r) + AE_MK_AI_STATE)) >> 5) & 1u) == 0) ae_MonkeyBodyUnregister(r, h);
    }
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    uint32_t stun = RD8(monkey + AE_MK_STUN_TIMER);
    if (stun != 0) WR8(monkey + AE_MK_STUN_TIMER, stun - 1u);

    monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_ACTIVITY) == 4 && ae_AreaIsBossArena(r) == 0 && ((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 30) & 1u) == 0) {
        ai_update_contact(r, h);
    }

    /* ---- removal: area 0x46 with negative health, or the +0x90 timer at 0 */
    if (ae_ram_failed(r)) return;
    if (RD16(AE_G_CURRENT_AREA) == 70) {
        if (RDS16(cur_monkey(r) + AE_MK_HEALTH) < 0) {
            AE_MK_HOOK(r, h, GimmickEvent, 10, 0, 0, 0);
            WR8(cur_monkey(r) + AE_MK_ACTIVITY, 2);
            ae_MonkeySelectActive(r, h);
            return;
        }
    }
    monkey = cur_monkey(r);
    if (RD16(monkey + AE_MK_TIMER_90) == 0) {
        WR8(monkey + AE_MK_ACTIVITY, 2);
        ae_MonkeySelectActive(r, h);
    }
    if (ae_ram_failed(r)) return;

    /* ---- the orientation matrix of the move direction */
    state = RD8(cur_monkey(r) + AE_MK_AI_STATE);
    if (!(state == 9 || ae_MonkeyIsAltAnimState(r) != 0 || RD8(cur_monkey(r) + AE_MK_AI_STATE) == 41 ||
          RD8(cur_monkey(r) + AE_MK_AI_STATE) == 43)) {
        uint32_t matrix = sp + 32u;
        ae_sdk_RotMatrix(r, cur_monkey(r) + AE_MK_ROT, matrix);
        uint32_t move_dir = cur_monkey(r) + AE_MK_MOVE_DIR;
        ae_sdk_ApplyMatrixSV(r, matrix, move_dir, move_dir);
        move_dir = cur_monkey(r) + AE_MK_MOVE_DIR;
        ae_sdk_VectorNormalSS(r, move_dir, move_dir);
    }

    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    uint32_t dir_lo = RD32(monkey + AE_MK_MOVE_DIR);
    uint32_t dir_hi = RD32(monkey + AE_MK_MOVE_DIR + 4);
    WR32(monkey + AE_MK_PREV_DIR, dir_lo);
    WR32(monkey + AE_MK_PREV_DIR + 4, dir_hi);
    ai_update_notice(r);

    if (ae_ram_failed(r)) return;
    if ((RD32(cur_monkey(r) + AE_MK_FLAGS) >> 19) & 1u) {
        if (ae_MonkeyIsIdleActivityState(r) == 0) {
            monkey = cur_monkey(r);
            WR32(monkey + AE_MK_FLAGS, RD32(monkey + AE_MK_FLAGS) & 0xFFF7FFFFu);
        }
    }
    int32_t anim_index = ae_MonkeyGetAnimIndex(r);
    monkey = cur_monkey(r);
    WR16(monkey + AE_MK_ANIM_INDEX, anim_index);
    uint32_t scale = 256;
    if ((STATE_FLAGS(RD8(monkey + AE_MK_AI_STATE)) >> 4) & 1u) {
        if (ae_AreaIsBossArena(r) == 0) scale = 0xFFFFFF00u;
    }
    WR16(cur_monkey(r) + AE_MK_BODY_HEIGHT, scale);
    monkey = cur_monkey(r);
    state = RD8(monkey + AE_MK_AI_STATE);
    if (state == 30 || state == 32) WR16(monkey + AE_MK_BODY_HEIGHT, 256);
}

/* ================================================================================================
 * MonkeySpawnAll
 * ============================================================================================== */

/* MonkeySpawnAll @8003958C: the area loader for the monkeys.  Clears the voice queue and the eight records (ClearWords of 0x94 words), the
   target becomes the player, the active list is emptied; the random seed starts at 0 (bosses / the 0x800A8A58 switch) or VSync * 0x231F; the
   area data (0x800F4590) gives the header (0x800B4D14), the MonkeyInfo table (+0x2A) and, when the header word is 0x11, the active maximum
   (+0x28, at most 4 with a printf), the LOD models and animation sets (+8.. +0x24 added to the area base), the nav data (+4: route
   table 0x800B4D0C, nodes at +0x488, extra links, node count), the monkey count (entries of the route table until -1, at most 8), the
   per-monkey route and group counts, the link lists of the nodes, MonkeyInit(i, 0, 0) for each monkey, RestoreActivities and the
   first SelectActive.  A wrong header word switches the monkey system off (0x800F44D4 = 0, active maximum 0, 0x800B4CF4 = 1). */
void ae_MonkeySpawnAll(AeRam *r, const AeMonkeyHooks *h) {
    int32_t i;
    WR32(AE_G_FRAME_COUNTER_EB5C, 0);
    ae_MonkeyVoiceQueueReset(r);
    WR32(AE_G_MONKEY_TARGET, AE_G_PLAYER);
    for (i = 0; i < AE_MONKEY_COUNT; i++) {
        AE_MK_HOOK(r, h, ClearWords, AE_G_MONKEYS + (uint32_t)i * AE_MONKEY_SIZE, AE_MONKEY_SIZE / 4u, 0, 0);
    }
    for (i = 3; i >= 0; i--) WR32(AE_G_MONKEY_PTRS + (uint32_t)i * 4u, 0);

    /* the random seed */
    if (RD32(AE_G_SPAWN_SWITCH) != 0) {
        WR32(AE_G_MONKEY_RAND_SEED, 0);
    } else if (ae_AreaIsBossArena(r) != 0) {
        WR32(AE_G_MONKEY_RAND_SEED, 0);
    } else {
        uint32_t vsync = (uint32_t)AE_MK_HOOK(r, h, VSync, 0xFFFFFFFFu, 0, 0, 0);
        WR32(AE_G_MONKEY_RAND_SEED, vsync * 0x231Fu);
    }
    if (ae_ram_failed(r)) return;

    for (i = 0; i < AE_MONKEY_COUNT; i++) WR8(AE_G_MONKEYS + (uint32_t)i * AE_MONKEY_SIZE + AE_MK_ACTIVITY, 2);
    uint32_t header = RD32(AE_G_AREA_DATA);
    WR32(AE_G_AREA_HEADER, header);
    WR32(AE_G_MONKEY_INFO_TABLE, header + 42u);
    header = RD32(AE_G_AREA_HEADER);
    if (RD32(header) != 0x11) {
        /* not monkey area data: the monkey system is switched off */
        WR16(AE_G_MONKEY_COUNT_F44D4, 0);
        WR16(AE_G_ACTIVE_MONKEY_MAX, 0);
        WR8(AE_G_MONKEY_FLAG_4CF4, 1);
        return;
    }
    WR8(AE_G_MONKEY_FLAG_4CF4, 0);
    if (RDS16(header + 0x28) >= 5) {
        AE_MK_HOOK(r, h, Printf, 0x800A4930u, 4, 0, 0);
        WR16(AE_G_ACTIVE_MONKEY_MAX, 4);
    } else {
        WR16(AE_G_ACTIVE_MONKEY_MAX, RD16(header + 0x28));
    }

    /* models and animation sets */
    uint32_t area_base = RD32(AE_G_AREA_DATA);
    header = RD32(AE_G_AREA_HEADER);
    WR32(AE_G_LOD_MODELS, area_base + RD32(header + 8));
    WR32(AE_G_LOD_MODELS + 4, area_base + RD32(header + 12));
    WR32(AE_G_LOD_MODELS + 8, area_base + RD32(header + 16));
    WR32(AE_G_MONKEY_AREA_SECTION, area_base + RD32(header + 20));
    WR32(AE_G_ANIM_SET, area_base + RD32(header + 32));
    WR32(AE_G_ANIM_SET2, area_base + RD32(header + 36));

    /* the nav data */
    uint32_t nav_data = area_base + RD32(header + 4);
    WR16(AE_G_ACTIVE_MAX_COPY, RD16(AE_G_ACTIVE_MONKEY_MAX));
    uint32_t link_lists = 0;                                    /* the link id lists follow the nodes */
    if (nav_data != 0) {
        WR32(AE_G_ROUTE_TABLE, nav_data);
        uint32_t node_bytes = (uint32_t)RDS16(nav_data + 2) - 1160u;
        WR32(AE_G_NAV_NODES, nav_data + 1160u);
        link_lists = nav_data + (uint32_t)RDS16(nav_data + 2);
        WR16(AE_G_NAV_EXTRA_COUNT, RD16(nav_data + 4));
        WR32(AE_G_NAV_EXTRA_LINKS, nav_data + (uint32_t)RDS16(nav_data + 6));
        WR16(AE_G_NAV_NODE_COUNT, node_bytes / AE_NAV_NODE_SIZE);
    }

    /* the monkey count: route table entries until one with -1 at +8, at most 8 */
    uint32_t route = RD32(AE_G_ROUTE_TABLE);
    int32_t count = 0;
    for (;;) {
        if (RDS16(route + 8) == -1) break;
        count++;
        route += 16u;
        if (ae_ram_failed(r)) return;
        if (!(count < 8)) break;
    }
    if (count < 0) count = 0;
    WR16(AE_G_MONKEY_COUNT_F44D4, count);
    WR16(AE_G_MONKEY_COUNT, count);
    for (i = 0; i < AE_MONKEY_COUNT; i++) WR8(AE_G_MONKEYS + (uint32_t)i * AE_MONKEY_SIZE + AE_MK_ACTIVITY, 2);

    /* per monkey: the number of route entries (spawn candidates) before the -1 */
    for (i = 0; i < RDS16(AE_G_MONKEY_COUNT); i++) {
        uint32_t monkey = AE_G_MONKEYS + (uint32_t)i * AE_MONKEY_SIZE;
        uint32_t entry = RD32(AE_G_ROUTE_TABLE) + (uint32_t)i * 16u;
        int32_t candidates = 0;
        for (;;) {
            if (RDS16(entry + 8) == -1) break;
            candidates++;
            entry += 2u;
            if (ae_ram_failed(r)) return;
            if (!(candidates < 8)) break;
        }
        WR8(monkey + AE_MK_ROUTE_COUNT, candidates);
    }

    /* per monkey: the patrol groups (8 groups of 8 node ids at +0x88 of the route record, 0x80 bytes per monkey): the group count and the
       number of nodes of each group */
    for (i = 0; i < RDS16(AE_G_MONKEY_COUNT); i++) {
        uint32_t monkey = AE_G_MONKEYS + (uint32_t)i * AE_MONKEY_SIZE;
        uint32_t monkey_routes = (uint32_t)i << 7;
        int32_t group = 0;
        for (;;) {
            uint32_t group_ofs = monkey_routes + (uint32_t)group * 16u;
            if (RDS16(RD32(AE_G_ROUTE_TABLE) + group_ofs + 0x88) == -1) {
                WR8(monkey + AE_MK_GROUP_COUNT, group);
                break;
            }
            int32_t nodes_in_group = 0;
            if (i < 8) {
                uint32_t node_ptr = RD32(AE_G_ROUTE_TABLE) + (uint32_t)group * 16u + monkey_routes + 0x88u;
                while (RDS16(node_ptr) != -1) {
                    node_ptr += 2u;
                    nodes_in_group++;
                    if (ae_ram_failed(r)) return;
                }
            }
            WR8(monkey + AE_MK_GROUP_SIZES + (uint32_t)group, nodes_in_group);
            group++;
            if (!(group < 8)) break;
        }
        if (group == 8) WR8(monkey + AE_MK_GROUP_COUNT, group);
    }

    ae_MonkeyDebugMenuInit(r, h);
    ae_MonkeyDebugMenuInit2(r, h, 0);
    if (ae_ram_failed(r)) return;
    if (RDS16(AE_G_MONKEY_COUNT_F44D4) <= 0) return;

    /* the link id lists: each node gets its list pointer, the lists follow one another */
    int32_t node_count = RDS16(AE_G_NAV_NODE_COUNT);
    for (i = 0; i < node_count; i++) {
        uint32_t node = RD32(AE_G_NAV_NODES) + (uint32_t)i * AE_NAV_NODE_SIZE;
        uint32_t link_count = RD8(node + 0xA);
        WR32(node + 0xC, link_lists);
        link_lists += link_count * 2u;
    }

    for (i = 0; i < RDS16(AE_G_MONKEY_COUNT); i++) {
        WR32(AE_G_CUR_MONKEY, AE_G_MONKEYS + (uint32_t)i * AE_MONKEY_SIZE);
        ae_MonkeyInit(r, h, (uint32_t)s16((uint32_t)i), 0, 0);
        if (ae_ram_failed(r)) return;
    }
    ae_MonkeyRestoreActivities(r, h);
    if (ae_ram_failed(r)) return;
    int32_t active_max = RDS16(AE_G_ACTIVE_MONKEY_MAX);
    int32_t monkey_count = RDS16(AE_G_MONKEY_COUNT_F44D4);
    WR16(AE_G_ACTIVE_MONKEY_MAX, active_max < monkey_count ? active_max : monkey_count);        /* min(active maximum, monkey count) */
    ae_MonkeySelectActive(r, h);
}

/* ================================================================================================
 * MonkeyThinkActive
 * ================================================================================================ */

/* MonkeyThinkActive @800424D4: the AI of the (up to four) active monkeys, when the monkey count (0x800F44D4) is positive.  For each slot the
   monkey becomes g_CurMonkey; in area-state byte 0x800EC2D2 == 7 (the RC car mode) the target is the nearer of the player / the RC car
   (nav area flag 1 for the car) else the player.  Empty slots and monkeys with activity 2 are skipped; a monkey whose +0xEA counter
   exceeds 2700 picks a new goal node, otherwise (every active-maximum frames, by its slot) the node visit update runs (not in boss
   arenas); then the target distance and MonkeyAiStateUpdate (the original calls it with $s2 = the player address, which GoToFlaggedNode
   uses as an uninitialised node index). */
void ae_MonkeyThinkActive(AeRam *r, const AeMonkeyHooks *h) {
    int32_t slot;
    if (RDS16(AE_G_MONKEY_COUNT_F44D4) <= 0) return;
    for (slot = 0; slot < 4; slot++) {
        uint32_t monkey = RD32(AE_G_MONKEY_PTRS + (uint32_t)slot * 4u);
        WR32(AE_G_CUR_MONKEY, monkey);

        /* the target: the player, or the RC car when it is nearer in the RC car mode */
        int target_is_car = 0;
        if (RDS8(AE_G_PLAYER + 242) == 7) {
            int32_t player_dist = ae_Distance3D(r, monkey + AE_MK_POS, AE_G_PLAYER + 36u);
            int32_t car_dist = ae_Distance3D(r, cur_monkey(r) + AE_MK_POS, AE_G_RC_CAR + 36u);
            if (car_dist < player_dist) target_is_car = 1;
        }
        if (target_is_car) {
            WR8(AE_G_NAV_AREA_FLAG, 1);
            WR32(AE_G_MONKEY_TARGET, AE_G_RC_CAR);
        } else {
            WR8(AE_G_NAV_AREA_FLAG, 0);
            WR32(AE_G_MONKEY_TARGET, AE_G_PLAYER);
        }
        if (ae_ram_failed(r)) return;

        monkey = cur_monkey(r);
        if (monkey == 0) continue;
        if (RD8(monkey + AE_MK_ACTIVITY) == 2) continue;
        if (!(RD16(monkey + AE_MK_GOAL_AGE) < 2701u)) {
            ae_MonkeyPickGoalNode(r, h);
        } else {
            uint32_t active_max = (uint32_t)RDS16(AE_G_ACTIVE_MONKEY_MAX);
            uint32_t frame = RD32(AE_G_FRAME_COUNTER);
            uint32_t quotient = divu_trap(r, frame, active_max);
            uint32_t frame_slot = frame - quotient * active_max;        /* frame % active maximum */
            int32_t monkey_slot = rem_trap(r, (int32_t)RD8(monkey + AE_MK_SLOT), (int32_t)active_max);
            if (ae_ram_failed(r)) return;
            if (frame_slot == (uint32_t)monkey_slot) {
                if (ae_AreaIsBossArena(r) == 0) ae_MonkeyNavVisitUpdate(r, h);
            }
        }
        if (ae_ram_failed(r)) return;
        ae_MonkeyUpdateTargetDistance(r, h);
        ae_MonkeyAiStateUpdate(r, h, AE_G_PLAYER);
        if (ae_ram_failed(r)) return;
    }
}

/* ================================================================================================
 * MonkeyUpdateAll
 * ============================================================================================== */

/* MonkeyUpdateAll @80042E18: the per-frame monkey system.  The counter 0x800AEB5C is incremented; boss arenas copy it to the frame counter
   0x800AEB60, other areas copy 0x800EC350.  For each of the eight monkeys: flags bit 21 is cleared (and +0x154 set to -1) unless the monkey
   is in activity 4 and in an attack state; +0x158 counts down.  Unless 0x800B4CF4 is set or there are no monkeys: ThinkActive (not when
   0x800F4450 bit 1 is set), UpdateVisuals, the stub callback, DelayedSfxUpdate; then SelectActive when the monkey count / active maximum
   changed since the last frame or every 30th frame; the last values are stored at 0x800AEC10 / 0x800AEC12. */
void ae_MonkeyUpdateAll(AeRam *r, const AeMonkeyHooks *h) {
    int32_t i;
    WR32(AE_G_FRAME_COUNTER_EB5C, RD32(AE_G_FRAME_COUNTER_EB5C) + 1u);
    if (ae_AreaIsBossArena(r) != 0) WR32(AE_G_FRAME_COUNTER, RD32(AE_G_FRAME_COUNTER_EB5C));
    else WR32(AE_G_FRAME_COUNTER, RD32(AE_G_STATE_BASE_FRAME));

    for (i = 0; i < AE_MONKEY_COUNT; i++) {
        uint32_t monkey = AE_G_MONKEYS + (uint32_t)i * AE_MONKEY_SIZE;
        uint32_t transient = (RD32(monkey + AE_MK_FLAGS) >> 21) & 1u;      /* flags bit 21 */
        WR32(AE_G_CUR_MONKEY, monkey);
        if (transient == 1) {
            if (RD8(monkey + AE_MK_ACTIVITY) != 4 || ae_MonkeyIsAttackState(r) == 0) {
                monkey = cur_monkey(r);
                uint32_t flags = RD32(monkey + AE_MK_FLAGS);
                WR32(monkey + AE_MK_RESET_154, 0xFFFFFFFFu);
                WR32(monkey + AE_MK_FLAGS, flags & 0xFFDFFFFFu);
            }
        }
        monkey = cur_monkey(r);
        int32_t timer = RDS16(monkey + AE_MK_TIMER_158);
        if (timer > 0) WR16(monkey + AE_MK_TIMER_158, RD16(monkey + AE_MK_TIMER_158) - 1u);
    }
    if (ae_ram_failed(r)) return;
    if (RD8(AE_G_MONKEY_FLAG_4CF4) != 0) return;
    if (RDS16(AE_G_MONKEY_COUNT_F44D4) <= 0) return;
    if ((RD32(AE_G_GAME_FLAGS_F4450) & 2u) == 0) ae_MonkeyThinkActive(r, h);
    if (ae_ram_failed(r)) return;
    ae_MonkeyUpdateVisuals(r, h);
    ae_MonkeyStub_ReturnZero(r);
    ae_DelayedSfxUpdate(r, h);
    if (ae_ram_failed(r)) return;

    /* SelectActive when the monkey count or the active maximum changed, and every 30th frame */
    int unchanged = RDS16(AE_G_LAST_MONKEY_COUNT) == RDS16(AE_G_MONKEY_COUNT_F44D4) &&
                    RDS16(AE_G_LAST_ACTIVE_MAX) == RDS16(AE_G_ACTIVE_MONKEY_MAX);
    if (!(unchanged && RD32(AE_G_FRAME_COUNTER) % 30u != 0)) ae_MonkeySelectActive(r, h);
    WR16(AE_G_LAST_MONKEY_COUNT, RD16(AE_G_MONKEY_COUNT_F44D4));
    WR16(AE_G_LAST_ACTIVE_MAX, RD16(AE_G_ACTIVE_MONKEY_MAX));
}
