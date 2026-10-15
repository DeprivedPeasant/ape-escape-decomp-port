/*
 * Monkey code, batch 5b (call-graph level 9: callees are batch 1..8 functions), translated from the original instructions
 * (see ae_monkey.h and notes/port/monkeys-port.md).  Where the Ghidra pseudocode and the listing differ, the listing was
 * followed.  Branch delay slots are reproduced where they matter.  The larger functions keep the control flow of the listing
 * (labels are named after the original addresses).
 */
#include "ae_monkey_inl.h"

/* ================================================================================================
 * MonkeyAi_State3D, MonkeyAi_JumpArc / JumpArcPhased / JumpArcB
 * ============================================================================================== */

/* MonkeyAi_State3D @8003FB44: state 0x3D: on reaching a node a new one is selected (1, 1) and the action decided; otherwise the monkey
   moves by the stored step (x / z += s16(+0x70 / +0x74), y += s16(+0x72) when flags2 bit 1 is set).  The counter at +0x96 counts up
   to 30. */
void ae_MonkeyAi_State3D(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey;
    if (ae_MonkeyReachedNode(r) != 0) {
        ae_MonkeySelectNextNode(r, h, 1, 1);
        ae_MonkeyDecideAction(r, h);
    } else {
        monkey = cur_monkey(r);
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
    monkey = cur_monkey(r);
    int32_t counter = RDS16(monkey + AE_MK_COUNTER96);
    uint32_t counter_raw = RD16(monkey + AE_MK_COUNTER96);
    if (counter < 30) WR16(monkey + AE_MK_COUNTER96, counter_raw + 1u);
}

/* the animation frame of the arc states 4 / 0x31 (shared by JumpArc and JumpArcB: identical code): t = the s16 counter (+0x96), duration =
   the arc duration (+0x128).  First half of the duration: the frame advances while it is below first frame + s16(+0x172);
   last stretch (the final half of s16(+0x170)): it advances when the animation is not past its last frame; then it is clamped to the last frame. */
static void arc_anim(AeRam *r, uint32_t monkey, int32_t t, uint32_t duration) {
    if (t < (int32_t)(duration >> 1)) {
        uint32_t frame = RD16(monkey + AE_MK_ANIM_FRAME);
        int32_t anim_index = ae_MonkeyGetAnimIndex(r);
        monkey = cur_monkey(r);
        uint32_t anim_table = RD32(monkey + AE_MK_ANIM_TABLE);
        uint32_t first_frame = RD16((uint32_t)anim_index * 8u + anim_table);
        uint32_t rise_frames = RD16(monkey + AE_MK_JUMP_RISE_FRAMES);
        if ((int32_t)frame < (int32_t)(first_frame + rise_frames)) {
            WR16(monkey + AE_MK_ANIM_FRAME, RD16(monkey + AE_MK_ANIM_FRAME) + 1u);
        }
    } else {
        uint32_t landing_start = duration - (RD16(monkey + AE_MK_JUMP_LAND_FRAMES) >> 1);
        if (!(t < (int32_t)landing_start)) {
            if (ae_MonkeyAnimFinished(r) != 0) {
                uint32_t m = cur_monkey(r);
                WR16(m + AE_MK_ANIM_FRAME, RD16(m + AE_MK_ANIM_FRAME) + 1u);
            }
        }
    }
    /* clamp the frame to the last frame of the clip */
    uint32_t actor = cur_monkey(r);
    int32_t anim_index = ae_MonkeyGetAnimIndex(r);
    monkey = cur_monkey(r);
    uint32_t anim_table = RD32(monkey + AE_MK_ANIM_TABLE);
    uint32_t frame = RD16(actor + AE_MK_ANIM_FRAME);
    uint32_t last_frame = RD16((uint32_t)anim_index * 8u + anim_table + 2);
    uint32_t new_frame;
    if (frame < last_frame) {
        new_frame = RD16(monkey + AE_MK_ANIM_FRAME);
    } else {
        anim_index = ae_MonkeyGetAnimIndex(r);
        anim_table = RD32(cur_monkey(r) + AE_MK_ANIM_TABLE);
        new_frame = RD16((uint32_t)anim_index * 8u + anim_table + 2);
    }
    WR16(actor + AE_MK_ANIM_FRAME, new_frame);
}

/* the position update of JumpArc / JumpArcPhased: x = start x + (s32(+0x13C) >> 12) * t, z likewise (+0x144), y = start y +
   (s32(+0x140) * t) / 4096 (rounded toward zero) + 300 t^2 / 100, kept at or above the waypoint height (MonkeyGetWaypointPos(1)).
   t = the s16 counter. */
static void arc_move(AeRam *r, const AeMonkeyHooks *h, int32_t t) {
    uint32_t monkey = cur_monkey(r);
    int32_t dx = mul_lo(sra32(RDS32(monkey + AE_MK_JUMP_VEL), 12), t);
    int32_t dz = mul_lo(sra32(RDS32(monkey + AE_MK_JUMP_VEL + 8), 12), t);
    uint32_t start_x = RD32(monkey + AE_MK_JUMP_START);
    uint32_t start_z = RD32(monkey + AE_MK_JUMP_START + 8);
    WR32(monkey + AE_MK_POS, start_x + (uint32_t)dx);
    WR32(monkey + AE_MK_POS + 8, start_z + (uint32_t)dz);
    int32_t min_y = ae_MonkeyGetWaypointPos(r, h, 1);
    if (ae_ram_failed(r)) return;
    uint32_t actor = cur_monkey(r);
    int32_t vel_y = RDS32(actor + AE_MK_JUMP_VEL + 4);
    int32_t rise = mul_lo(vel_y, t) / 4096;
    int32_t gravity = mul_lo(t, 300 * t) / 100;
    uint32_t y = RD32(actor + AE_MK_JUMP_START + 4);
    y += (uint32_t)rise;
    y += (uint32_t)gravity;
    if (min_y < (int32_t)y) {
        WR32(monkey + AE_MK_POS + 4, ae_MonkeyGetWaypointPos(r, h, 1));
    } else {
        WR32(monkey + AE_MK_POS + 4, y);
    }
}

/* the end of a jump arc: select the next node (1, 0); a node was selected: the action decision, else (not in a boss arena, not in the
   states 9 / 0x3D / 0x1A / 0x35 / 0x0C / 0x24) state 0 */
static void arc_end(AeRam *r, const AeMonkeyHooks *h) {
    if (ae_MonkeySelectNextNode(r, h, 1, 0) != 0) {
        ae_MonkeyDecideAction(r, h);
        return;
    }
    if (ae_AreaIsBossArena(r) != 0) return;
    uint32_t state = RD8(cur_monkey(r) + AE_MK_AI_STATE);
    if (state == 9 || state == 61 || state == 26 || state == 53) return;
    if (ae_MonkeyIsState0C_or_24(r) != 0) return;
    ae_MonkeySetState(r, 0);
}

/* MonkeyAi_JumpArc @8003F530: states 4 and 0x1D (the nav-link jump).  While t = +0x96 < the duration +0x128: (state 4) animation frame,
   position on the arc; t + 1.  Past the duration: the monkey lands on the next node (state 0x1D takes the node position), then the
   arc end. */
void ae_MonkeyAi_JumpArc(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_JUMP_ARC_AI);
    uint32_t monkey = cur_monkey(r);
    int32_t t = RDS16(monkey + AE_MK_COUNTER96);
    uint32_t duration = RD16(monkey + AE_MK_ANIM_LENGTH);
    uint32_t counter_raw = RD16(monkey + AE_MK_COUNTER96);
    if (t < (int32_t)duration) {
        if (RD8(monkey + AE_MK_AI_STATE) != 29) {
            arc_anim(r, monkey, t, duration);
            arc_move(r, h, s16(counter_raw));
            if (ae_ram_failed(r)) return;
        }
        monkey = cur_monkey(r);
        WR16(monkey + AE_MK_COUNTER96, RD16(monkey + AE_MK_COUNTER96) + 1u);
        return;
    }
    /* the flight is over: land on the node (state 0x1D takes the node position) */
    uint32_t node = RD16(monkey + AE_MK_NAV_NODE0);
    ae_NavNodeGetPos(r, h, (int32_t)node, sp + 16);
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    if (RD8(monkey + AE_MK_AI_STATE) == 29) {
        WR32(monkey + AE_MK_POS, RD32(sp + 16));
        WR32(monkey + AE_MK_POS + 4, RD32(sp + 20));
        WR32(monkey + AE_MK_POS + 8, RD32(sp + 24));
    }
    arc_end(r, h);
}

/* MonkeyAi_JumpArcPhased @8003F86C: states 0x2C - 0x2E: 0x2C (start) waits for the finished animation and becomes 0x2D (the flight, same arc
   as JumpArc, frame advance by MonkeyAdvanceAnimFrame while the frame is below the last); when the flight is over (t >= duration) the
   state becomes 0x2E (landing: the node position for state 0x1D, PickAnim); 0x2E ends like JumpArc when the animation is done. */
void ae_MonkeyAi_JumpArcPhased(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_JUMP_ARC_AI);
    uint32_t monkey = cur_monkey(r);
    uint32_t state = RD8(monkey + AE_MK_AI_STATE);
    uint32_t counter = RD16(monkey + AE_MK_COUNTER96);

    switch (state) {
    case 0x2C:                                  /* start: wait for the animation, then the flight begins */
        if (ae_MonkeyAnimFinished(r) != 0) return;
        monkey = cur_monkey(r);
        WR8(monkey + AE_MK_AI_STATE, 0x2D);
        return;
    case 0x2E:                                  /* landing: ends like JumpArc once the animation is done */
        if (ae_MonkeyAnimFinished(r) != 0) return;
        arc_end(r, h);
        return;
    default:                                    /* 0x2D: the flight */
        break;
    }

    monkey = cur_monkey(r);
    uint32_t duration = RD16(monkey + AE_MK_ANIM_LENGTH);
    int32_t t = s16(counter);
    if (!(t < (int32_t)duration)) {
        /* the flight is over: land on the node */
        WR8(monkey + AE_MK_AI_STATE, 0x2E);     /* delay slot of the jal */
        ae_MonkeyPickAnim(r);
        uint32_t node = RD16(cur_monkey(r) + AE_MK_NAV_NODE0);
        ae_NavNodeGetPos(r, h, (int32_t)node, sp + 16);
        if (ae_ram_failed(r)) return;
        monkey = cur_monkey(r);
        if (RD8(monkey + AE_MK_AI_STATE) != 29) return;
        WR32(monkey + 0x24, RD32(sp + 16));
        WR32(monkey + 0x28, RD32(sp + 20));
        WR32(monkey + 0x2C, RD32(sp + 24));
        return;
    }
    /* flight: the frame advances while it is below the last frame of the clip */
    int32_t anim_index = ae_MonkeyGetAnimIndex(r);
    uint32_t anim_table = RD32(cur_monkey(r) + AE_MK_ANIM_TABLE);
    uint32_t frame = RD16(monkey + AE_MK_ANIM_FRAME);
    uint32_t last_frame = RD16((uint32_t)anim_index * 8u + anim_table + 2);
    if (frame < last_frame) ae_MonkeyAdvanceAnimFrame(r);
    arc_move(r, h, t);
    if (ae_ram_failed(r)) return;
    monkey = cur_monkey(r);
    WR16(monkey + AE_MK_COUNTER96, RD16(monkey + AE_MK_COUNTER96) + 1u);
}

/* MonkeyAi_JumpArcB @8003FD7C: state 0x31, the arc variant: the animation like JumpArc, but only the height changes: y = start y - s32(+0x140) * t +
   4 t^2 (the 400 t^2 / 100 of the listing), kept at or above the ground height under the monkey (GetGroundHeight at its own position).
   The end of the arc is the common arc end (no node position). */
void ae_MonkeyAi_JumpArcB(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t monkey = cur_monkey(r);
    int32_t t = RDS16(monkey + AE_MK_COUNTER96);
    uint32_t duration = RD16(monkey + AE_MK_ANIM_LENGTH);
    uint32_t counter_raw = RD16(monkey + AE_MK_COUNTER96);
    if (t < (int32_t)duration) {
        if (RD8(monkey + AE_MK_AI_STATE) != 29) {
            arc_anim(r, monkey, t, duration);
            uint32_t actor = cur_monkey(r);
            uint32_t x = RD32(actor + AE_MK_POS), y = RD32(actor + AE_MK_POS + 4), z = RD32(actor + AE_MK_POS + 8);
            int32_t ground = AE_MK_HOOK(r, h, GetGroundHeight, x, y, z, 0);
            uint32_t m = cur_monkey(r);
            int32_t c = s16(counter_raw);
            int32_t rise = mul_lo(RDS32(m + AE_MK_JUMP_VEL + 4), c);
            int32_t gravity = mul_lo(c, 400 * c) / 100;
            int32_t new_y = (int32_t)RD32(m + AE_MK_JUMP_START + 4);
            new_y = sub32(new_y, rise);
            new_y = add32(new_y, gravity);
            if (ground < new_y) {
                int32_t ground_here = AE_MK_HOOK(r, h, GetGroundHeight, RD32(m + AE_MK_POS), RD32(m + AE_MK_POS + 4), RD32(m + AE_MK_POS + 8), 0);
                WR32(actor + AE_MK_POS + 4, ground_here);
            } else {
                WR32(actor + AE_MK_POS + 4, new_y);
            }
            if (ae_ram_failed(r)) return;
        }
        monkey = cur_monkey(r);
        WR16(monkey + AE_MK_COUNTER96, RD16(monkey + AE_MK_COUNTER96) + 1u);
        return;
    }
    arc_end(r, h);
}

/* ================================================================================================
 * MonkeySelectActive
 * ============================================================================================== */

/* MonkeySelectActive @800426C4: ranks the monkeys by the distance to the player and keeps the nearest g_ActiveMonkeyMax (s16 at 0x800AEB66) as the
   active ones (pointers at 0x800E54F0).  Nothing happens without monkeys / active slots.  Steps (all on the listing's order):
   1. the ids of the four current active monkeys are saved (u16 at sp + 16, always four entries);
   2. every monkey whose activity is not 2 / 3 becomes g_CurMonkey and gets its distance (+0x210 = Distance3D(pos, 0x800EC204, the player
      position)); an active (4) one is demoted to activity 1 and its collision body unregistered;
   3. the active list is cleared and refilled: for each monkey with activity not 2 / 3 the first free slot takes it; if the list is full it
      replaces the farthest active monkey when it is nearer (the replaced one gets activity 1 unless it is 0);
   4. each active monkey is initialised (MonkeyInit(id, 4, slot)); a monkey that had activity 0 picks a goal node;
   5. monkeys that were not in the list before (id differs from the saved one of that slot) get their goal node re-picked or restored
      (ResetNavVisits, +0xE6 and the group byte +0xD8), the visit counter of the goal node set to 1 and +0xEE = 1.
   g_CurMonkey is restored at the end (not on the early exits).  Note g_CurMonkey is only assigned for monkeys with activity not 2 / 3 in
   step 2 and may become 0 in step 4 (an empty slot). */
void ae_MonkeySelectActive(AeRam *r, const AeMonkeyHooks *h) {
    uint32_t sp = AE_STK(AE_STK_SELECT_ACTIVE);
    uint32_t saved_ids = sp + 16;               /* u16[4]: the monkey ids of the four active slots before the selection */
    int32_t i, slot;
    int32_t count = RDS16(AE_G_MONKEY_COUNT);
    uint32_t saved_cur_monkey = RD32(AE_G_CUR_MONKEY);
    if (count == 0) return;
    if (RDS16(AE_G_ACTIVE_MONKEY_MAX) == 0) return;

    /* 1. the ids of the four active slots */
    for (slot = 0; slot < 4; slot++) {
        uint32_t active = RD32(AE_G_MONKEY_PTRS + (uint32_t)slot * 4u);
        WR16(saved_ids + (uint32_t)slot * 2u, RD8(active + AE_MK_ID));
        if (ae_ram_failed(r)) return;
    }

    /* 2. distances to the player; an active monkey is demoted */
    for (i = 0; i < RDS16(AE_G_MONKEY_COUNT); i++) {
        uint32_t monkey = (uint32_t)i * AE_MONKEY_SIZE + AE_G_MONKEYS;
        if (RD8(monkey + AE_MK_ACTIVITY) - 2u >= 2u) {          /* activity is not 2 / 3 */
            WR32(AE_G_CUR_MONKEY, monkey);
            int32_t dist = ae_Distance3D(r, monkey + AE_MK_POS, 0x800EC204u);      /* 0x800EC204 = the player position */
            monkey = cur_monkey(r);
            uint32_t activity = RD8(monkey + AE_MK_ACTIVITY);
            WR32(monkey + AE_MK_DIST_TO_PLAYER, dist);
            if (activity == 4) {
                WR8(monkey + AE_MK_ACTIVITY, 1);
                ae_MonkeyBodyUnregister(r, h);
            }
        }
        if (ae_ram_failed(r)) return;
    }

    /* 3. the list is cleared and refilled */
    for (slot = 0; slot < 4; slot++) WR32(AE_G_MONKEY_PTRS + (uint32_t)slot * 4u, 0);
    for (i = 0; i < RDS16(AE_G_MONKEY_COUNT); i++) {
        int32_t farthest_dist = -1000;
        int32_t farthest_slot = -1;
        uint32_t monkey = (uint32_t)i * AE_MONKEY_SIZE + AE_G_MONKEYS;
        WR32(AE_G_CUR_MONKEY, monkey);
        if (RD8(monkey + AE_MK_ACTIVITY) - 2u >= 2u) {          /* activity is not 2 / 3 */
            int placed = 0;
            int32_t max_active = RDS16(AE_G_ACTIVE_MONKEY_MAX);
            for (slot = 0; slot < max_active; slot++) {
                uint32_t entry = AE_G_MONKEY_PTRS + (uint32_t)slot * 4u;
                uint32_t occupant = RD32(entry);
                if (occupant == 0) {                            /* a free slot takes the monkey */
                    WR32(entry, monkey);
                    placed = 1;
                    break;
                }
                int32_t occupant_dist = (int32_t)RD32(occupant + AE_MK_DIST_TO_PLAYER);
                if (farthest_dist < occupant_dist) {
                    farthest_dist = occupant_dist;
                    farthest_slot = slot;
                }
            }
            if (ae_ram_failed(r)) return;
            /* the list is full: the monkey replaces the farthest one when it is nearer */
            if (!placed && (int32_t)RD32(cur_monkey(r) + AE_MK_DIST_TO_PLAYER) < farthest_dist) {
                uint32_t entry = AE_G_MONKEY_PTRS + (uint32_t)(farthest_slot * 4);
                uint32_t replaced = RD32(entry);
                if (RD8(replaced + AE_MK_ACTIVITY) != 0) WR8(replaced + AE_MK_ACTIVITY, 1);
                WR32(entry, cur_monkey(r));
            }
        }
        if (ae_ram_failed(r)) return;
    }

    /* 4. initialise the active monkeys */
    if (RDS16(AE_G_ACTIVE_MONKEY_MAX) > 0) {
        for (slot = 0; slot < RDS16(AE_G_ACTIVE_MONKEY_MAX); slot++) {
            uint32_t monkey = RD32(AE_G_MONKEY_PTRS + (uint32_t)slot * 4u);
            WR32(AE_G_CUR_MONKEY, monkey);
            if (monkey != 0) {
                uint32_t id = RD8(monkey + AE_MK_ID);
                uint32_t old_activity = RD8(monkey + AE_MK_ACTIVITY);
                ae_MonkeyInit(r, h, id, 4, (uint32_t)slot);
                if (ae_ram_failed(r)) return;
                if (old_activity == 0) ae_MonkeyPickGoalNode(r, h);
            }
            if (ae_ram_failed(r)) return;
        }
    }

    /* 5. monkeys that are new in their slot: restore or re-pick the goal node and mark it visited */
    if (RDS16(AE_G_ACTIVE_MONKEY_MAX) > 0) {
        for (slot = 0; slot < RDS16(AE_G_ACTIVE_MONKEY_MAX); slot++) {
            uint32_t monkey = RD32(AE_G_MONKEY_PTRS + (uint32_t)slot * 4u);
            if (monkey != 0 && RDS16(saved_ids + (uint32_t)slot * 2u) != (int32_t)RD8(monkey + AE_MK_ID)) {
                int32_t goal_node = RDS16(monkey + AE_MK_GOAL_NODE);
                WR32(AE_G_CUR_MONKEY, monkey);
                uint32_t group = RD8(monkey + AE_MK_GROUP);
                if (goal_node != -1) {
                    ae_MonkeyResetNavVisits(r);
                    uint32_t m = cur_monkey(r);
                    WR16(m + AE_MK_GOAL_NODE, goal_node);
                    WR8(m + AE_MK_GROUP, group);
                } else {
                    ae_MonkeyPickGoalNode(r, h);
                }
                if (ae_ram_failed(r)) return;
                uint32_t m = cur_monkey(r);
                uint32_t node = node_of(r, (uint32_t)RDS16(m + AE_MK_GOAL_NODE));
                WR8(node + RD8(m + AE_MK_SLOT) + AE_NAV_NODE_VISIT_MARKS, 1);
                WR16(cur_monkey(r) + AE_MK_NAV_MARK_COUNT, 1);
            }
            if (ae_ram_failed(r)) return;
        }
    }
    WR32(AE_G_CUR_MONKEY, saved_cur_monkey);
}
