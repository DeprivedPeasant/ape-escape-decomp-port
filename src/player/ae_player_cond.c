/* Player transition predicates from SCUS_944.23, executed on the original AeRam image. */
#include "ae_player_cond.h"

#include <limits.h>

#define PLAYER(r, field) (AE_G_PLAYER + (field))
#define SCRATCH(r, field) (AE_G_PLAYER_SCRATCH + (field))

static int32_t wrap_add32(int32_t left, int32_t right) {
    return (int32_t)((uint32_t)left + (uint32_t)right);
}

static int32_t wrap_sub32(int32_t left, int32_t right) {
    return (int32_t)((uint32_t)left - (uint32_t)right);
}

static int32_t wrap_mul32(int32_t left, int32_t right) {
    return (int32_t)((uint32_t)left * (uint32_t)right);
}

static int32_t wrap_neg32(int32_t value) {
    return (int32_t)(0u - (uint32_t)value);
}

/* R3000 `sra`, expressed without relying on implementation-defined negative shifts. */
static int32_t arithmetic_shift_right(int32_t value, unsigned shift) {
    if (value >= 0) return value >> shift;
    return (int32_t)(-((-(int64_t)value + (((int64_t)1 << shift) - 1)) >> shift));
}

/* The compiler sequence (negative ? value + 0xFFF : value) >> 12 divides toward zero. */
static int32_t divide_pow2_toward_zero(int32_t value, unsigned shift) {
    int64_t magnitude = value < 0 ? -(int64_t)value : value;
    int32_t quotient = (int32_t)(magnitude >> shift);
    return value < 0 ? -quotient : quotient;
}

static uint32_t player_contact(AeRam *r) {
    return ae_rd32(r, PLAYER(r, AE_PLAYER_CONTACT));
}

static int32_t recent_history_slot_signed(uint32_t history_pos, uint32_t age) {
    /* PlayerCond_RStickEvent02 / Event40 use the original signed divide-by-40 sequence. */
    int32_t offset = (int32_t)(history_pos + AE_PLAYER_HISTORY_SIZE - age);
    return offset % (int32_t)AE_PLAYER_HISTORY_SIZE;
}

static uint32_t recent_history_slot_unsigned(uint32_t history_pos, uint32_t age) {
    /* Event04 / 08 / 10 / 20 use the original unsigned divide-by-40 sequence. */
    return (history_pos + AE_PLAYER_HISTORY_SIZE - age) % AE_PLAYER_HISTORY_SIZE;
}

static int32_t has_recent_stick_event(AeRam *r, uint16_t event, int signed_slot, int require_unlocked) {
    if (require_unlocked && ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_INPUT_LOCKED)) != 0) return 0;

    uint32_t history_pos = ae_rd32(r, AE_PAD_HISTORY_POS);
    for (uint32_t age = 0; age < AE_PLAYER_HISTORY_WINDOW; ++age) {
        uint32_t slot;
        if (signed_slot) slot = (uint32_t)recent_history_slot_signed(history_pos, age);
        else slot = recent_history_slot_unsigned(history_pos, age);
        uint32_t sample = AE_PAD_STICK_DONE + slot * AE_PLAYER_HISTORY_VALUE_SIZE;
        if (ae_rd16(r, sample) == event) return 1;
    }
    return 0;
}

static int32_t ground_height(AeRam *r, const AePlayerCondHooks *h, int32_t x, int32_t y, int32_t z) {
    if (!h || !h->world) {
        ae_ram_fail(r, "PlayerCond: GetGroundHeight world hooks not provided");
        return 0;
    }
    return ae_GetGroundHeight(r, h->world, x, y, z);
}

static int32_t player_bool_hook(AeRam *r, const AePlayerCondHooks *h, AePlayerCondBoolHook fn,
                                const char *name) {
    if (!h || !fn) {
        ae_ram_fail(r, "PlayerCond: hook %s not provided", name);
        return 0;
    }
    return fn(h->user, r) != 0;
}

static int32_t player_void_hook(AeRam *r, const AePlayerCondHooks *h, AePlayerCondVoidHook fn,
                                const char *name) {
    if (!h || !fn) {
        ae_ram_fail(r, "PlayerCond: hook %s not provided", name);
        return 0;
    }
    fn(h->user, r);
    return 1;
}

int32_t ae_PlayerCond_False(AeRam *r) {
    (void)r;
    return 0;
}

int32_t ae_PlayerCond_JumpHeld(AeRam *r) {
    return (int32_t)(ae_rd32(r, SCRATCH(r, AE_PLAYER_SCRATCH_HELD)) & 0x0Au);
}

int32_t ae_PlayerCond_JumpPressed(AeRam *r) {
    uint32_t held = ae_rd32(r, SCRATCH(r, AE_PLAYER_SCRATCH_HELD));
    uint32_t changed = ae_rd32(r, SCRATCH(r, AE_PLAYER_SCRATCH_CHANGED));
    return (int32_t)(held & changed & 0x0Au);
}

int32_t ae_PlayerCond_RStickEvent02(AeRam *r) {
    return has_recent_stick_event(r, 0x02u, 1, 1);
}

int32_t ae_PlayerCond_RStickEvent40(AeRam *r) {
    return has_recent_stick_event(r, 0x40u, 1, 0);
}

int32_t ae_PlayerCond_RStickBackHard(AeRam *r) {
    int32_t forward = ae_rds16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_CAM_FWD));
    uint32_t magnitude = ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_MAG));
    return forward < -0xB50 && magnitude >= 0x6Fu;
}

int32_t ae_PlayerCond_GadgetBusyOrGesture(AeRam *r) {
    if (ae_rds16(r, AE_G_PLAYER_FLICK_BUSY) != 0) return 1;
    if ((ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_GESTURE)) & 0x30u) != 0) return 1;
    return ae_rds16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_CAM_FWD)) < 0xB50;
}

int32_t ae_PlayerCond_FacingDesiredDir(AeRam *r) {
    int32_t facing_x = ae_rds16(r, PLAYER(r, AE_PLAYER_FACING_X));
    int32_t facing_z = ae_rds16(r, PLAYER(r, AE_PLAYER_FACING_Z));
    int32_t desired_x = ae_rds16(r, PLAYER(r, AE_PLAYER_DESIRED_DIR_X));
    int32_t desired_z = ae_rds16(r, PLAYER(r, AE_PLAYER_DESIRED_DIR_Z));
    int32_t dot = wrap_add32(wrap_mul32(facing_x, desired_x), wrap_mul32(facing_z, desired_z));
    if (dot < 0) dot = wrap_add32(dot, 0xFFF);
    return arithmetic_shift_right(dot, 12) >= 0xDDC;
}

int32_t ae_PlayerCond_RStickSideNeg(AeRam *r) {
    if (ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_MAG)) == 0) return 0;
    return ae_rds16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_CAM_SIDE)) < -0xDDB;
}

int32_t ae_PlayerCond_RStickSidePos(AeRam *r) {
    if (ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_MAG)) == 0) return 0;
    return ae_rds16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_CAM_SIDE)) > 0xDDB;
}

int32_t ae_PlayerCond_RStickFwd(AeRam *r) {
    if (ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_MAG)) == 0) return 0;
    return ae_rds16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_CAM_FWD)) > 0xDDB;
}

int32_t ae_PlayerCond_RStickBack(AeRam *r) {
    if (ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_MAG)) == 0) return 0;
    return ae_rds16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_CAM_FWD)) < -0xDDB;
}

int32_t ae_PlayerCond_AttachTimerDone(AeRam *r) {
    uint16_t timer = (uint16_t)(ae_rd16(r, PLAYER(r, AE_PLAYER_ATTACH_TIMER)) - 1u);
    ae_wr16(r, PLAYER(r, AE_PLAYER_ATTACH_TIMER), timer);
    return timer == 0;
}

int32_t ae_PlayerCond_GroundBehindLow(AeRam *r, const AePlayerCondHooks *h) {
    int32_t behind_x = arithmetic_shift_right(ae_rds16(r, PLAYER(r, AE_PLAYER_FACING_X)), 5);
    int32_t behind_z = arithmetic_shift_right(ae_rds16(r, PLAYER(r, AE_PLAYER_FACING_Z)), 5);
    int32_t x = wrap_sub32(ae_rds32(r, PLAYER(r, AE_PLAYER_POS_X)), behind_x);
    int32_t y = ae_rds32(r, PLAYER(r, AE_PLAYER_POS_Y));
    int32_t z = wrap_sub32(ae_rds32(r, PLAYER(r, AE_PLAYER_POS_Z)), behind_z);
    int32_t ground = ground_height(r, h, x, y, z);
    if (ae_ram_failed(r)) return 0;
    return ground < wrap_add32(y, 0x100);
}

int32_t ae_PlayerCond_RStickGestureOrTravel(AeRam *r) {
    if ((ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_GESTURE)) & 0x30u) != 0) return 1;
    return ae_rds16(r, SCRATCH(r, AE_PLAYER_SCRATCH_GADGET_TRAVEL)) > 800;
}

int32_t ae_PlayerCond_AnimDoneRandom(AeRam *r) {
    uint16_t seed = ae_rd16(r, AE_G_PLAYER_COND_RAND_SEED);
    seed = (uint16_t)((uint32_t)seed * 5u + 1u);
    ae_wr16(r, AE_G_PLAYER_COND_RAND_SEED, seed);
    if (((seed >> 8) & 1u) == 0) return 0;
    return ae_PlayerCond_AnimDone(r) != 0;
}

int32_t ae_PlayerCond_NoMoveInput(AeRam *r) {
    if (ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_CLASS)) != 0) return 0;
    return (ae_rd32(r, SCRATCH(r, AE_PLAYER_SCRATCH_HELD)) & 0x20Au) == 0;
}

int32_t ae_PlayerCond_LStickFwd(AeRam *r) {
    if (ae_rds16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_CAM_FWD)) <= 0xB50) return 0;
    return ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_CLASS)) != 0;
}

int32_t ae_PlayerCond_LStickBack(AeRam *r) {
    if (ae_rds16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_CAM_FWD)) >= -0xB50) return 0;
    return ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_CLASS)) != 0;
}

int32_t ae_PlayerCond_RStickEvent04(AeRam *r) {
    return has_recent_stick_event(r, 0x04u, 0, 1);
}

int32_t ae_PlayerCond_RStickEvent08(AeRam *r) {
    return has_recent_stick_event(r, 0x08u, 0, 1);
}

int32_t ae_PlayerCond_NoGestureR3Held(AeRam *r) {
    if (ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_GESTURE)) != 0) return 0;
    return (ae_rd32(r, SCRATCH(r, AE_PLAYER_SCRATCH_HELD)) & 0x400u) != 0;
}

int32_t ae_PlayerCond_RStickRotating(AeRam *r) {
    if ((ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_GESTURE)) & 0x30u) == 0) return 0;
    int32_t rotation = ae_rds16(r, SCRATCH(r, AE_PLAYER_SCRATCH_STICK_ROTATION));
    if (rotation < 0) rotation = wrap_neg32(rotation);
    return rotation >= 0x15;
}

int32_t ae_PlayerCond_Gadget1RStickFwdFlick(AeRam *r) {
    if (ae_rds16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_CAM_FWD)) <= 0xB50) return 0;
    if ((ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_GESTURE)) & 0x0Cu) == 0) return 0;
    return ae_rds8(r, PLAYER(r, AE_PLAYER_GADGET)) == 1;
}

int32_t ae_PlayerCond_Gadget3RStickBack(AeRam *r) {
    if (ae_rds16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_CAM_FWD)) >= -0xB50) return 0;
    if (ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_MAG)) < 0x6Fu) return 0;
    return ae_rds8(r, PLAYER(r, AE_PLAYER_GADGET)) == 3;
}

int32_t ae_PlayerCond_RStickGesture10(AeRam *r) {
    return ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_GESTURE)) & 0x10u;
}

int32_t ae_PlayerCond_RStickGesture20(AeRam *r) {
    return ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_GESTURE)) & 0x20u;
}

int32_t ae_PlayerCond_RStickNoGesture(AeRam *r) {
    return ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_GESTURE)) == 0;
}

int32_t ae_PlayerCond_GestureEvent04(AeRam *r) {
    return ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_GESTURE_EVENTS)) & 0x04u;
}

int32_t ae_PlayerCond_GestureEvent08(AeRam *r) {
    return ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_GESTURE_EVENTS)) & 0x08u;
}

int32_t ae_PlayerCond_LStickIdleL3Held(AeRam *r) {
    if (ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_CLASS)) != 0) return 0;
    return (ae_rd32(r, SCRATCH(r, AE_PLAYER_SCRATCH_HELD)) & 0x200u) != 0;
}

int32_t ae_PlayerCond_LStickIdle(AeRam *r) {
    return ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_CLASS)) == 0;
}

int32_t ae_PlayerCond_LStickClass2(AeRam *r) {
    return ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_CLASS)) & 0x02u;
}

int32_t ae_PlayerCond_LStickClass4(AeRam *r) {
    return ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_CLASS)) & 0x04u;
}

int32_t ae_PlayerCond_LStickClass8(AeRam *r) {
    return ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_CLASS)) & 0x08u;
}

int32_t ae_PlayerCond_LStickFull(AeRam *r) {
    return ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_MAG)) >= 0x7Du;
}

int32_t ae_PlayerCond_RStickFull(AeRam *r) {
    return ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_MAG)) >= 0x7Du;
}

int32_t ae_PlayerCond_LStickPartial(AeRam *r) {
    uint32_t magnitude = ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_MAG));
    return (uint32_t)(magnitude - 1u) < 0x7Bu;
}

int32_t ae_PlayerCond_RStickPartial(AeRam *r) {
    uint32_t magnitude = ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_RIGHT_STICK_MAG));
    return (uint32_t)(magnitude - 1u) < 0x7Bu;
}

int32_t ae_PlayerCond_LandingSoon(AeRam *r, const AePlayerCondHooks *h) {
    if (ae_rds16(r, PLAYER(r, AE_PLAYER_HAS_GROUND)) == 0) return 0;

    int32_t move_vel_y = ae_rds32(r, PLAYER(r, AE_PLAYER_MOVE_VEL_Y));
    int32_t step_y = divide_pow2_toward_zero(move_vel_y, 12);
    int32_t pos_x = ae_rds32(r, PLAYER(r, AE_PLAYER_POS_X));
    int32_t pos_y = ae_rds32(r, PLAYER(r, AE_PLAYER_POS_Y));
    int32_t pos_z = ae_rds32(r, PLAYER(r, AE_PLAYER_POS_Z));
    int32_t ground = ground_height(r, h, pos_x, wrap_add32(pos_y, step_y), pos_z);
    if (ae_ram_failed(r)) return 0;

    int32_t three_steps = wrap_mul32(divide_pow2_toward_zero(move_vel_y, 12), 3);
    int32_t landing_y = wrap_add32(pos_y, three_steps);
    return ground < landing_y;
}

int32_t ae_PlayerCond_FacingContactWall(AeRam *r) {
    uint32_t contact = player_contact(r);
    if ((contact >> 28) != 0xAu) return 0;
    uint32_t contact_class = contact & 0xF000u;
    if (contact_class == 0x3000u || contact_class == 0x9000u) return 0;

    int32_t input_x = ae_rds16(r, AE_G_PLAYER_NORMALIZED_VEL_DIR + AE_PLAYER_COND_DIR_X);
    int32_t input_z = ae_rds16(r, AE_G_PLAYER_NORMALIZED_VEL_DIR + AE_PLAYER_COND_DIR_Z);
    int32_t facing_x = ae_rds16(r, PLAYER(r, AE_PLAYER_FACING_X));
    int32_t facing_z = ae_rds16(r, PLAYER(r, AE_PLAYER_FACING_Z));
    int32_t dot = wrap_add32(wrap_mul32(input_x, facing_x), wrap_mul32(input_z, facing_z));
    return dot < -0xDDB000;
}

int32_t ae_PlayerCond_ContactKind8(AeRam *r) {
    uint32_t contact_kind = ae_rd8(r, AE_G_PLAYER + AE_PLAYER_CONTACT_KIND_BYTE) & 0x0Fu;
    return contact_kind == 8;
}

int32_t ae_PlayerCond_NotContactKind8(AeRam *r) {
    uint32_t contact_kind = ae_rd8(r, AE_G_PLAYER + AE_PLAYER_CONTACT_KIND_BYTE) & 0x0Fu;
    return contact_kind != 8;
}

int32_t ae_PlayerCond_FallingHigh(AeRam *r) {
    int32_t pos_y = ae_rds32(r, PLAYER(r, AE_PLAYER_POS_Y));
    int32_t ground_y = ae_rds32(r, PLAYER(r, AE_PLAYER_GROUND_Y));
    if (wrap_add32(pos_y, 0x40) >= ground_y) return 0;
    return ae_rds32(r, PLAYER(r, AE_PLAYER_VEL_Y)) >= 0x11;
}

int32_t ae_PlayerCond_RStickEvent10(AeRam *r) {
    return has_recent_stick_event(r, 0x10u, 0, 1);
}

int32_t ae_PlayerCond_RStickEvent20(AeRam *r) {
    return has_recent_stick_event(r, 0x20u, 0, 1);
}

static int32_t first_frame_stick_event(AeRam *r, uint16_t event) {
    uint32_t state = ae_rd16(r, PLAYER(r, AE_PLAYER_STATE)) & AE_PLAYER_STATE_INDEX_MASK;
    uint32_t state_table = ae_rd32(r, SCRATCH(r, AE_PLAYER_SCRATCH_STATE_TABLE));
    uint32_t entry = state_table + state * AE_PLAYER_STATE_ENTRY_SIZE;
    uint32_t first_frame = ae_rd16(r, entry + AE_PLAYER_STATE_FIRST_FRAME);
    if (ae_rd16(r, PLAYER(r, AE_PLAYER_ANIM_FRAME)) != first_frame + 1u) return 0;

    uint32_t history_pos = ae_rd32(r, AE_PAD_HISTORY_POS);
    uint32_t current_event = AE_PAD_STICK_DONE + (history_pos << 1);
    return ae_rd16(r, current_event) == event;
}

int32_t ae_PlayerCond_FirstFrameRStickEvent10(AeRam *r) {
    return first_frame_stick_event(r, 0x10u);
}

int32_t ae_PlayerCond_FirstFrameRStickEvent20(AeRam *r) {
    return first_frame_stick_event(r, 0x20u);
}

int32_t ae_PlayerCond_JumpPressedAtApex(AeRam *r, const AePlayerCondHooks *h) {
    if (!ae_PlayerCond_JumpPressed(r)) return 0;
    int32_t vertical_velocity = ae_rds32(r, PLAYER(r, AE_PLAYER_VEL_Y));
    if (vertical_velocity < 0) vertical_velocity = wrap_neg32(vertical_velocity);
    if (vertical_velocity >= 0x26) return 0;
    return player_void_hook(r, h, h ? h->PlayerFxJumpBurst : 0, "PlayerFxJumpBurst");
}

int32_t ae_PlayerCond_LStickSidePos(AeRam *r) {
    return ae_rds16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_CAM_SIDE)) > 0xB50;
}

int32_t ae_PlayerCond_LStickSideNeg(AeRam *r) {
    return ae_rds16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_CAM_SIDE)) < -0xB50;
}

int32_t ae_PlayerCond_LStickSideAny(AeRam *r) {
    uint32_t side = ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_CAM_SIDE));
    uint32_t range_value = (side + 0xB50u) & 0xFFFFu;
    return range_value >= 0x16A1u;
}

int32_t ae_PlayerCond_LStickIdleStillMoving(AeRam *r) {
    if (ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_CLASS)) != 0) return 0;
    int32_t x_velocity = ae_rds32(r, PLAYER(r, AE_PLAYER_VEL_X));
    int32_t z_velocity = ae_rds32(r, PLAYER(r, AE_PLAYER_VEL_Z));
    int32_t speed_squared = wrap_add32(wrap_mul32(x_velocity, x_velocity), wrap_mul32(z_velocity, z_velocity));
    return speed_squared >= 0x401;
}

int32_t ae_PlayerCond_GestureEvent0C(AeRam *r) {
    return ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_GESTURE_EVENTS)) & 0x0Cu;
}

int32_t ae_PlayerCond_InWater(AeRam *r) {
    return (ae_rd16(r, PLAYER(r, AE_PLAYER_GROUND_TYPE)) >> 12) == 1;
}

int32_t ae_PlayerCond_WadingOnGround(AeRam *r, const AePlayerCondHooks *h) {
    uint16_t ground_type = (uint16_t)ae_rd16(r, PLAYER(r, AE_PLAYER_GROUND_TYPE));
    if ((ground_type >> 12) != 1 || (ground_type & 0x0Fu) == 0) return 0;

    int32_t slope_x = ae_rds16(r, PLAYER(r, AE_PLAYER_SLOPE_X));
    int32_t slope_z = ae_rds16(r, PLAYER(r, AE_PLAYER_SLOPE_Z));
    if (slope_x < 0) slope_x = wrap_neg32(slope_x);
    if (slope_z < 0) slope_z = wrap_neg32(slope_z);
    if (wrap_add32(slope_x, slope_z) >= 0x200) return 0;

    return player_bool_hook(r, h, h ? h->PlayerIsOnGround : 0, "PlayerIsOnGround");
}

int32_t ae_PlayerCond_ContactWall3B(AeRam *r) {
    uint32_t contact = player_contact(r);
    return (contact >> 28) == 0xAu && (contact & 0xFFu) == 0x3Bu;
}

int32_t ae_PlayerCond_JumpOutOfWater(AeRam *r, const AePlayerCondHooks *h) {
    uint32_t held = ae_rd32(r, SCRATCH(r, AE_PLAYER_SCRATCH_HELD));
    uint32_t changed = ae_rd32(r, SCRATCH(r, AE_PLAYER_SCRATCH_CHANGED));
    if ((held & changed & 0x0Au) == 0) return 0;
    if ((held & 0x200u) != 0) return 0;

    int32_t water_y = ae_rds16(r, PLAYER(r, AE_PLAYER_WATER_Y));
    int32_t pos_y = ae_rds32(r, PLAYER(r, AE_PLAYER_POS_Y));
    if (wrap_sub32(pos_y, water_y) >= 200) return 0;
    return player_void_hook(r, h, h ? h->PlayerSplash : 0, "PlayerSplash");
}

int32_t ae_PlayerCond_CanJump(AeRam *r) {
    if (!ae_PlayerCond_JumpPressed(r)) return 0;

    uint16_t ground_type = (uint16_t)ae_rd16(r, PLAYER(r, AE_PLAYER_GROUND_TYPE));
    if ((ground_type >> 12) == 1 && (player_contact(r) >> 28) == 0xAu) return 1;

    int32_t surface_kind = ae_rds16(r, PLAYER(r, AE_PLAYER_SURFACE_KIND));
    if (surface_kind < 0) return 0;
    uint32_t surface = AE_PLAYER_SURFACE_TABLE + (uint32_t)surface_kind * AE_PLAYER_SURFACE_ENTRY_SIZE;
    int32_t slope_limit = ae_rds16(r, surface + AE_PLAYER_SURFACE_JUMP_SLOPE_LIMIT);
    int32_t slope_z = ae_rds16(r, PLAYER(r, AE_PLAYER_SLOPE_Z));
    if (slope_z < 0) slope_z = wrap_neg32(slope_z);
    if (slope_limit < slope_z) return 0;

    int32_t slope_x = ae_rds16(r, PLAYER(r, AE_PLAYER_SLOPE_X));
    if (slope_x < 0) slope_x = wrap_neg32(slope_x);
    return slope_limit >= slope_x;
}

int32_t ae_PlayerCond_LStickIdleL3Pressed(AeRam *r) {
    if (ae_rd16(r, SCRATCH(r, AE_PLAYER_SCRATCH_LEFT_STICK_CLASS)) != 0) return 0;
    uint32_t held = ae_rd32(r, SCRATCH(r, AE_PLAYER_SCRATCH_HELD));
    uint32_t changed = ae_rd32(r, SCRATCH(r, AE_PLAYER_SCRATCH_CHANGED));
    return (held & changed & 0x200u) != 0;
}

int32_t ae_PlayerCond_Dead(AeRam *r) {
    if (ae_rds16(r, PLAYER(r, AE_PLAYER_HEALTH)) > 0) return 0;
    return (ae_rd32(r, PLAYER(r, AE_PLAYER_STATE_FLAGS)) & 0xF0u) != 0xE0u;
}

int32_t ae_PlayerCond_Gadget4Mode1(AeRam *r) {
    return ae_rds16(r, AE_G_GADGET4_MODE) == 1;
}

int32_t ae_PlayerCond_AnimDone(AeRam *r) {
    uint32_t state = ae_rd16(r, PLAYER(r, AE_PLAYER_STATE)) & AE_PLAYER_STATE_INDEX_MASK;
    uint32_t state_table = ae_rd32(r, SCRATCH(r, AE_PLAYER_SCRATCH_STATE_TABLE));
    uint32_t entry = state_table + state * AE_PLAYER_STATE_ENTRY_SIZE;
    uint32_t last_frame = ae_rd16(r, entry + AE_PLAYER_STATE_LAST_FRAME);
    uint32_t anim_frame = ae_rd16(r, PLAYER(r, AE_PLAYER_ANIM_FRAME));
    uint32_t anim_lerp = ae_rd16(r, PLAYER(r, AE_PLAYER_ANIM_LERP)) != 0;
    return last_frame < anim_frame + anim_lerp;
}

int32_t ae_PlayerCond_False2(AeRam *r) {
    (void)r;
    return 0;
}

int32_t ae_PlayerCond_Still(AeRam *r) {
    int32_t velocity_x = ae_rds32(r, PLAYER(r, AE_PLAYER_VEL_X));
    if ((uint32_t)((uint32_t)velocity_x + 4u) >= 9u) return 0;
    int32_t velocity_z = ae_rds32(r, PLAYER(r, AE_PLAYER_VEL_Z));
    if ((uint32_t)((uint32_t)velocity_z + 4u) >= 9u) return 0;

    uint32_t slope_z = (ae_rd16(r, PLAYER(r, AE_PLAYER_SLOPE_Z)) + 0xFFu) & 0xFFFFu;
    if (slope_z >= 0x1FFu) return 0;
    uint32_t slope_x = (ae_rd16(r, PLAYER(r, AE_PLAYER_SLOPE_X)) + 0xFFu) & 0xFFFFu;
    return slope_x < 0x1FFu;
}
