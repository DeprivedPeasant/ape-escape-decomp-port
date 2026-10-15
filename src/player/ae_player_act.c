#include "ae_player_act.h"

#include "../core/ae_sdk.h"

#include <limits.h>
#include <string.h>

#define AE_PA_GADGET4_MODE 0x800F5124u
#define AE_PA_GADGET4_AUX 0x800F5128u
#define AE_PA_FACE_MATRIX_A 0x800AE760u
#define AE_PA_FACE_MATRIX_B 0x800AE740u
#define AE_PA_DIRECTION_BYTES 8u

static int32_t pa_s16(uint32_t value)
{
    value &= 0xFFFFu;
    return value < 0x8000u ? (int32_t)value : (int32_t)value - 0x10000;
}

static int32_t pa_wrap(uint32_t bits)
{
    if (bits <= INT32_MAX) return (int32_t)bits;
    return (int32_t)((int64_t)bits - 0x100000000LL);
}

static int32_t pa_add(int32_t a, int32_t b)
{
    return pa_wrap((uint32_t)a + (uint32_t)b);
}

static int32_t pa_sub(int32_t a, int32_t b)
{
    return pa_wrap((uint32_t)a - (uint32_t)b);
}

static int32_t pa_mul(int32_t a, int32_t b)
{
    return pa_wrap((uint32_t)((uint64_t)(uint32_t)a * (uint32_t)b));
}

static int32_t pa_sra(int32_t value, unsigned amount)
{
    uint32_t magnitude;
    if (amount == 0) return value;
    if (value >= 0) return value >> amount;
    magnitude = (uint32_t)(-(int64_t)value);
    return (int32_t)-((magnitude + ((1u << amount) - 1u)) >> amount);
}

static int32_t pa_div100(int32_t value)
{
    return value / 100;
}

static uint32_t pa_field(uint32_t base, uint32_t offset)
{
    return base + offset;
}

static void pa_fail_hook(AeRam *r, const char *name)
{
    ae_ram_fail(r, "%s hook not provided", name);
}

static void pa_face_stick(AeRam *r)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    const uint32_t input = AE_PA_INPUT_BLOCK;
    if (ae_rd16(r, pa_field(input, AE_PA_INPUT_GADGET_STICK_MAG)) < 0x41u) return;
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR + 2u), 0);
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR),
            ae_rd16(r, pa_field(input, AE_PA_INPUT_GADGET_DIR_X)));
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR + 4u),
            ae_rd16(r, pa_field(input, AE_PA_INPUT_GADGET_DIR_Z)));
}

void ae_PlayerAct_JumpPct3(AeRam *r)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    const uint32_t params = AE_PLAYER_ACT_PARAMS;
    int32_t percentage = ae_rds32(r, pa_field(params, AE_PA_PARAMS_JUMP_SPEED_PCT3));
    int32_t vx = pa_mul(ae_rds32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL)), percentage);
    int32_t vz = pa_mul(ae_rds32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL + 8u)), percentage);
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL + 4u),
            (uint32_t)pa_wrap((uint32_t)ae_rds32(r, pa_field(params, AE_PA_PARAMS_JUMP_VEL2)) << 7));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL), (uint32_t)pa_div100(vx));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL + 8u), (uint32_t)pa_div100(vz));
}

void ae_PlayerAct_VerticalOnly(AeRam *r)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    int32_t drop = ae_rds32(r, pa_field(AE_PLAYER_ACT_PARAMS, AE_PA_PARAMS_DROP_VEL));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL), 0);
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL), 0);
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL + 4u),
            (uint32_t)pa_wrap((uint32_t)drop << 7));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL + 8u), 0);
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL + 8u), 0);
}

void ae_PlayerAct_JumpPct2(AeRam *r)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    const uint32_t params = AE_PLAYER_ACT_PARAMS;
    int32_t percentage = ae_rds32(r, pa_field(params, AE_PA_PARAMS_JUMP_SPEED_PCT2));
    int32_t vx = pa_mul(ae_rds32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL)), percentage);
    int32_t vz = pa_mul(ae_rds32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL + 8u)), percentage);
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL + 4u),
            (uint32_t)pa_wrap((uint32_t)ae_rds32(r, pa_field(params, AE_PA_PARAMS_JUMP_VEL2)) << 7));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL), (uint32_t)pa_div100(vx));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL + 8u), (uint32_t)pa_div100(vz));
}

void ae_PlayerAct_DriftForward(AeRam *r)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    int32_t facing_x = ae_rds16(r, pa_field(player, AE_PA_PLAYER_FACING));
    int32_t facing_z = ae_rds16(r, pa_field(player, AE_PA_PLAYER_FACING + 4u));
    uint32_t frac_x = pa_field(player, AE_PA_PLAYER_VEL_FRAC);
    uint32_t frac_z = pa_field(player, AE_PA_PLAYER_VEL_FRAC + 8u);
    ae_wr32(r, frac_x, (uint32_t)pa_add(ae_rds32(r, frac_x), pa_wrap((uint32_t)facing_x << 5)));
    ae_wr32(r, frac_z, (uint32_t)pa_add(ae_rds32(r, frac_z), pa_wrap((uint32_t)facing_z << 5)));
}

void ae_PlayerAct_NegateInputXZ(AeRam *r)
{
    int32_t x = ae_rd16(r, pa_field(AE_PLAYER_ACT_SCRATCH, AE_PA_SCRATCH_INPUT_X));
    int32_t z = ae_rd16(r, pa_field(AE_PLAYER_ACT_SCRATCH, AE_PA_SCRATCH_INPUT_Y));
    ae_wr16(r, pa_field(AE_PLAYER_ACT_SCRATCH, AE_PA_SCRATCH_DIR_Y), 0);
    ae_wr16(r, pa_field(AE_PLAYER_ACT_SCRATCH, AE_PA_SCRATCH_DIR_X), (uint32_t)pa_sub(0, x));
    ae_wr16(r, pa_field(AE_PLAYER_ACT_SCRATCH, AE_PA_SCRATCH_DIR_Z), (uint32_t)pa_sub(0, z));
}

void ae_PlayerAct_TurnAround(AeRam *r)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    uint32_t facing_x = pa_field(player, AE_PA_PLAYER_FACING);
    uint32_t facing_z = pa_field(player, AE_PA_PLAYER_FACING + 4u);
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_ANIM_BLEND), 0);
    ae_wr16(r, facing_x, (uint32_t)pa_sub(0, ae_rds16(r, facing_x)));
    ae_wr16(r, facing_z, (uint32_t)pa_sub(0, ae_rds16(r, facing_z)));
}

void ae_PlayerAct_Nop(AeRam *r)
{
    (void)r;
}

void ae_PlayerAct_AnimSpeedNormal(AeRam *r)
{
    ae_wr16(r, pa_field(AE_PLAYER_ACT_PLAYER, AE_PA_PLAYER_ANIM_SPEED), 0x1000u);
}

void ae_PlayerAct_AnimSpeedHalf(AeRam *r)
{
    ae_wr16(r, pa_field(AE_PLAYER_ACT_PLAYER, AE_PA_PLAYER_ANIM_SPEED), 0x0800u);
}

void ae_PlayerAct_TurnFacingLeft90(AeRam *r)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    int32_t x = ae_rds16(r, pa_field(player, AE_PA_PLAYER_FACING));
    int32_t z = ae_rds16(r, pa_field(player, AE_PA_PLAYER_FACING + 4u));
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_FACING + 2u), 0);
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_FACING), (uint32_t)pa_sub(0, z));
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_FACING + 4u), (uint32_t)x);
}

void ae_PlayerAct_TurnFacingRight90(AeRam *r)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    int32_t x = ae_rds16(r, pa_field(player, AE_PA_PLAYER_FACING));
    int32_t z = ae_rds16(r, pa_field(player, AE_PA_PLAYER_FACING + 4u));
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_FACING + 2u), 0);
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_FACING), (uint32_t)z);
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_FACING + 4u), (uint32_t)pa_sub(0, x));
}

void ae_PlayerAct_Rebound(AeRam *r)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    int32_t target_y = ae_rds32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL + 4u));
    int32_t negative = pa_sub(0, target_y);
    int32_t quotient = pa_sra(pa_add(negative, (int32_t)((uint32_t)negative >> 31)), 1);
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL + 4u), (uint32_t)quotient);
}

void ae_PlayerAct_ScaleMoveVel(AeRam *r, const AePlayerActHooks *hooks,
                               uint32_t player, uint32_t gadget4_state)
{
    const uint32_t params = AE_PLAYER_ACT_PARAMS;
    int32_t x;
    int32_t z;
    int32_t percent;

    if (ae_rds8(r, pa_field(player, AE_PA_PLAYER_CURRENT_GADGET)) == 4) {
        if (ae_rds16(r, gadget4_state + 4u) == 1) {
            x = pa_sra(ae_rds32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL)), 1);
            z = pa_sra(ae_rds32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL + 8u)), 1);
            ae_wr32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL), (uint32_t)x);
            ae_wr32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL + 8u), (uint32_t)z);
        }
        /* 80065C4C branches to 80065C6C when the mode is not 1: both
         * controller fields still clear on every gadget-4 jump. */
        ae_wr16(r, gadget4_state + 4u, 0);
        ae_wr32(r, gadget4_state + 8u, 0);
    }

    if (hooks == NULL || hooks->AimRequestClear == NULL) {
        pa_fail_hook(r, "AimRequestClear");
    } else {
        hooks->AimRequestClear(hooks->user, r);
    }

    percent = ae_rds32(r, pa_field(params, AE_PA_PARAMS_JUMP_SPEED_PCT));
    x = pa_div100(pa_mul(ae_rds32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL)), percent));
    z = pa_div100(pa_mul(ae_rds32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL + 8u)), percent));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL), (uint32_t)x);
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL + 8u), (uint32_t)z);
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL), (uint32_t)x);
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL + 4u),
            (uint32_t)pa_wrap((uint32_t)pa_add(
                ae_rds32(r, pa_field(params, AE_PA_PARAMS_GRAVITY)),
                ae_rds32(r, pa_field(params, AE_PA_PARAMS_JUMP_IMPULSE))) << 7));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL + 8u), (uint32_t)z);
}

void ae_PlayerAct_JumpHoldBoost(AeRam *r, const AePlayerActHooks *hooks)
{
    int32_t held;
    if (hooks == NULL || hooks->PlayerCond_JumpHeld == NULL) {
        pa_fail_hook(r, "PlayerCond_JumpHeld");
        return;
    }
    held = hooks->PlayerCond_JumpHeld(hooks->user, r);
    if (held != 0) {
        uint32_t vy = pa_field(AE_PLAYER_ACT_PLAYER, AE_PA_PLAYER_MOVE_VEL + 4u);
        int32_t boost = ae_rds32(r, pa_field(AE_PLAYER_ACT_PARAMS, AE_PA_PARAMS_JUMP_HOLD_BOOST));
        ae_wr32(r, vy, (uint32_t)pa_add(ae_rds32(r, vy),
                pa_wrap((uint32_t)boost << 7)));
    }
}

void ae_PlayerAct_SquashReset(AeRam *r, const AePlayerActHooks *hooks)
{
    ae_wr16(r, pa_field(AE_PLAYER_ACT_PLAYER, AE_PA_PLAYER_SQUASH_FRAME), 0);
    ae_wr32(r, pa_field(AE_PLAYER_ACT_PLAYER, AE_PA_PLAYER_MOVE_VEL + 4u), 0);
    if (hooks == NULL || hooks->PadRumble == NULL) {
        pa_fail_hook(r, "PadRumble");
        return;
    }
    hooks->PadRumble(hooks->user, r, 1, 0xFF, 0x14);
}

void ae_PlayerAct_FlyerThrust(AeRam *r, const AePlayerActHooks *hooks)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    const uint32_t params = AE_PLAYER_ACT_PARAMS;
    uint32_t squash_frame = pa_field(player, AE_PA_PLAYER_SQUASH_FRAME);
    int32_t frame = ae_rd16(r, squash_frame);
    int32_t limit = ae_rds32(r, pa_field(params, AE_PA_PARAMS_SQUASH_FRAMES));
    int32_t rumble_strength;
    int32_t rumble_frames;

    ae_wr16(r, squash_frame, (uint32_t)(frame + 1));
    if (frame < limit) {
        uint32_t vy = pa_field(player, AE_PA_PLAYER_MOVE_VEL + 4u);
        int32_t thrust = ae_rds32(r, pa_field(params, AE_PA_PARAMS_FLYER_THRUST_A));
        ae_wr32(r, vy, (uint32_t)pa_add(ae_rds32(r, vy), pa_wrap((uint32_t)thrust << 7)));
        rumble_strength = 0x8C;
        rumble_frames = 5;
    } else {
        uint32_t move_y = pa_field(player, AE_PA_PLAYER_MOVE_VEL + 4u);
        int32_t thrust = ae_rds32(r, pa_field(params, AE_PA_PARAMS_FLYER_THRUST_B));
        uint32_t state = pa_field(AE_PA_INPUT_BLOCK, 0x1B4u);
        uint16_t pulse = (uint16_t)(ae_rd16(r, state) + 7u);
        int32_t cap = 0;
        ae_wr32(r, move_y, (uint32_t)pa_add(ae_rds32(r, move_y), pa_wrap((uint32_t)thrust << 7)));
        if ((uint16_t)(pulse & 0xFFFFu) >= 15u || ae_rds16(r, pa_field(AE_PA_INPUT_BLOCK, 0x1BCu)) >= 801) {
            cap = pa_wrap((uint32_t)ae_rds32(r, pa_field(params, AE_PA_PARAMS_FLYER_MAX_THRUST)) << 5);
        }
        if (cap < ae_rds32(r, move_y)) ae_wr32(r, move_y, (uint32_t)cap);
        rumble_strength = 0x50;
        rumble_frames = 5;
    }

    if (hooks == NULL || hooks->PadRumble == NULL) {
        pa_fail_hook(r, "PadRumble");
        return;
    }
    hooks->PadRumble(hooks->user, r, 1, rumble_strength, rumble_frames);
}

void ae_PlayerAct_FlyerLaunchA(AeRam *r)
{
    ae_wr16(r, pa_field(AE_PLAYER_ACT_PLAYER, AE_PA_PLAYER_SQUASH_FRAME),
            ae_rd16(r, pa_field(AE_PLAYER_ACT_PARAMS, AE_PA_PARAMS_SQUASH_FRAMES)));
    ae_wr32(r, pa_field(AE_PLAYER_ACT_PLAYER, AE_PA_PLAYER_MOVE_VEL + 4u),
            (uint32_t)pa_wrap((uint32_t)ae_rds32(r, pa_field(AE_PLAYER_ACT_PARAMS,
                    AE_PA_PARAMS_FLYER_LAUNCH_A)) << 5));
}

void ae_PlayerAct_FlyerLaunchB(AeRam *r)
{
    ae_wr16(r, pa_field(AE_PLAYER_ACT_PLAYER, AE_PA_PLAYER_SQUASH_FRAME),
            ae_rd16(r, pa_field(AE_PLAYER_ACT_PARAMS, AE_PA_PARAMS_SQUASH_FRAMES)));
    ae_wr32(r, pa_field(AE_PLAYER_ACT_PLAYER, AE_PA_PLAYER_MOVE_VEL + 4u),
            (uint32_t)pa_wrap((uint32_t)ae_rds32(r, pa_field(AE_PLAYER_ACT_PARAMS,
                    AE_PA_PARAMS_FLYER_LAUNCH_B)) << 5));
}

void ae_PlayerAct_GadgetSpinFromStick(AeRam *r, const AePlayerActHooks *hooks)
{
    int32_t active;
    int32_t rotation;
    uint32_t amount = AE_PLAYER_ACT_GADGET_SPIN_AMOUNT;
    if (hooks == NULL || hooks->PlayerInput_StickGesture == NULL) {
        pa_fail_hook(r, "PlayerInput_StickGesture");
        return;
    }
    active = hooks->PlayerInput_StickGesture(hooks->user, r);
    if (active == 0 || ae_rds16(r, AE_PLAYER_ACT_GADGET_SPIN_LIMIT) >= 0x2000) return;
    rotation = pa_s16(ae_rd16(r, pa_field(AE_PLAYER_ACT_SCRATCH, AE_PA_SCRATCH_STICK_ROTATION)));
    ae_wr16(r, AE_PLAYER_ACT_GADGET_SPIN_DELTA, (uint32_t)-64);
    ae_wr16(r, amount, (uint32_t)pa_add(ae_rd16(r, amount), rotation < 0 ? -rotation : rotation));
}

void ae_PlayerAct_Gadget3Aim(AeRam *r, const AePlayerActHooks *hooks)
{
    if (hooks == NULL || hooks->Gadget3AimUpdateAmmo == NULL) {
        pa_fail_hook(r, "Gadget3AimUpdateAmmo");
        return;
    }
    hooks->Gadget3AimUpdateAmmo(hooks->user, r);
}

void ae_PlayerAct_TurnAroundAndJump(AeRam *r, const AePlayerActHooks *hooks,
                                    uint32_t player, uint32_t gadget4_state)
{
    ae_PlayerAct_TurnAround(r);
    ae_PlayerAct_ScaleMoveVel(r, hooks, player, gadget4_state);
}

void ae_PlayerAct_FaceStick(AeRam *r)
{
    pa_face_stick(r);
}

void ae_PlayerAct_JumpFromLStick(AeRam *r)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    const uint32_t params = AE_PLAYER_ACT_PARAMS;
    const uint32_t input = AE_PA_INPUT_BLOCK;
    const uint32_t scratch = AE_PLAYER_ACT_SCRATCH;
    uint32_t target_x = pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL);
    uint32_t target_z = pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL + 8u);

    if (ae_rd16(r, pa_field(input, AE_PA_INPUT_MOVE_PUSH_STATE)) != 0) {
        int32_t magnitude = ae_rd16(r, pa_field(scratch, 0x64u));
        int32_t direction_x = pa_s16(ae_rd16(r, pa_field(input, AE_PA_INPUT_MOVE_DIR_X)));
        int32_t direction_z = pa_s16(ae_rd16(r, pa_field(input, AE_PA_INPUT_MOVE_DIR_Z)));
        int32_t speed = ae_rds32(r, pa_field(params, AE_PA_PARAMS_MOVE_SPEED));
        int32_t jump_speed = ae_rds32(r, pa_field(params, AE_PA_PARAMS_JUMP_SPEED_PCT3));
        int32_t stick_x = pa_sra(pa_mul(direction_x, magnitude), 7);
        int32_t stick_z = pa_sra(pa_mul(direction_z, magnitude), 7);
        int32_t move_x = pa_sra(pa_mul(stick_x, speed), 7);
        int32_t move_z = pa_sra(pa_mul(stick_z, speed), 7);
        int32_t scaled_x = pa_sra(pa_mul(move_x, jump_speed), 5);
        int32_t scaled_z = pa_sra(pa_mul(move_z, jump_speed), 5);
        ae_wr16(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR + 2u), 0);
        ae_wr16(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR),
                ae_rd16(r, pa_field(input, AE_PA_INPUT_MOVE_DIR_X)));
        ae_wr16(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR + 4u),
                ae_rd16(r, pa_field(input, AE_PA_INPUT_MOVE_DIR_Z)));
        ae_wr32(r, target_x, (uint32_t)scaled_x);
        ae_wr32(r, target_z, (uint32_t)scaled_z);
    } else {
        uint32_t facing = pa_field(player, AE_PA_PLAYER_FACING);
        uint32_t desired = pa_field(player, AE_PA_PLAYER_DESIRED_DIR);
        uint8_t copy[8];
        memcpy(copy, ae_ptr(r, facing, sizeof(copy)), sizeof(copy));
        memcpy(ae_ptr(r, desired, sizeof(copy)), copy, sizeof(copy));
    }

    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL + 4u),
            (uint32_t)pa_wrap((uint32_t)ae_rds32(r, pa_field(params, AE_PA_PARAMS_JUMP_VEL2)) << 7));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL), ae_rd32(r, target_x));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL + 8u), ae_rd32(r, target_z));
}

void ae_PlayerAct_StepBackDrop(AeRam *r)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    int32_t facing_x = ae_rds16(r, pa_field(player, AE_PA_PLAYER_FACING));
    int32_t facing_z = ae_rds16(r, pa_field(player, AE_PA_PLAYER_FACING + 4u));
    int32_t back_x = pa_sra(pa_add(facing_x, facing_x < 0 ? 31 : 0), 5);
    int32_t back_z = pa_sra(pa_add(facing_z, facing_z < 0 ? 31 : 0), 5);
    uint32_t pos = pa_field(player, AE_PA_PLAYER_POSITION);

    ae_wr32(r, pos, (uint32_t)pa_sub(ae_rds32(r, pos), back_x));
    ae_wr32(r, pos + 4u, (uint32_t)pa_add(ae_rds32(r, pos + 4u), 0x100));
    ae_wr32(r, pos + 8u, (uint32_t)pa_sub(ae_rds32(r, pos + 8u), back_z));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL), 0);
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL + 4u), 0);
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL + 8u), 0);
    pa_face_stick(r);
}

void ae_PlayerAct_BounceBack(AeRam *r, const AePlayerActHooks *hooks)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    int32_t facing_x = ae_rds16(r, pa_field(player, AE_PA_PLAYER_FACING));
    int32_t facing_z = ae_rds16(r, pa_field(player, AE_PA_PLAYER_FACING + 4u));
    int32_t back_x = pa_sra(pa_add(facing_x, facing_x < 0 ? 31 : 0), 5);
    int32_t back_z = pa_sra(pa_add(facing_z, facing_z < 0 ? 31 : 0), 5);
    uint32_t pos = pa_field(player, AE_PA_PLAYER_POSITION);
    ae_wr32(r, pos, (uint32_t)pa_sub(ae_rds32(r, pos), back_x));
    ae_wr32(r, pos + 8u, (uint32_t)pa_sub(ae_rds32(r, pos + 8u), back_z));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL),
            (uint32_t)pa_wrap((uint32_t)pa_wrap((uint32_t)-facing_x) << 4));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL + 4u), (uint32_t)-0x1E000);
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL + 8u),
            (uint32_t)pa_wrap((uint32_t)pa_wrap((uint32_t)-facing_z) << 4));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR),
            ae_rd32(r, pa_field(AE_PA_INPUT_BLOCK, AE_PA_INPUT_GADGET_DIR_X)));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR + 4u),
            ae_rd32(r, pa_field(AE_PA_INPUT_BLOCK, AE_PA_INPUT_GADGET_DIR_Z)));
    memcpy(ae_ptr(r, pa_field(player, AE_PA_PLAYER_FACING), AE_PA_DIRECTION_BYTES),
           ae_ptr(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR), AE_PA_DIRECTION_BYTES),
           AE_PA_DIRECTION_BYTES);
    ae_PlayerSetMode(r, hooks, 0);
}

void ae_PlayerAct_ExitToMode0Forward(AeRam *r, const AePlayerActHooks *hooks)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    uint32_t pos_y = pa_field(player, AE_PA_PLAYER_POSITION + 4u);
    int32_t y = ae_rds32(r, pos_y);
    int32_t facing_x = ae_rds16(r, pa_field(player, AE_PA_PLAYER_FACING));
    int32_t facing_z = ae_rds16(r, pa_field(player, AE_PA_PLAYER_FACING + 4u));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_TARGET_MOVE_VEL + 4u), 0);
    ae_wr32(r, pos_y, (uint32_t)pa_add(y, 0x10));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL + 4u), 0);
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL), (uint32_t)pa_wrap((uint32_t)facing_x << 4));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_MOVE_VEL + 8u), (uint32_t)pa_wrap((uint32_t)facing_z << 4));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR),
            ae_rd32(r, pa_field(AE_PA_INPUT_BLOCK, AE_PA_INPUT_GADGET_DIR_X)));
    ae_wr32(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR + 4u),
            ae_rd32(r, pa_field(AE_PA_INPUT_BLOCK, AE_PA_INPUT_GADGET_DIR_Z)));
    memcpy(ae_ptr(r, pa_field(player, AE_PA_PLAYER_FACING), AE_PA_DIRECTION_BYTES),
           ae_ptr(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR), AE_PA_DIRECTION_BYTES),
           AE_PA_DIRECTION_BYTES);
    ae_PlayerSetMode(r, hooks, 0);
}

void ae_PlayerAct_FaceStickMaybeHalfSpeed(AeRam *r)
{
    ae_PlayerAct_FaceStick(r);
    if ((ae_rd16(r, pa_field(AE_PLAYER_ACT_SCRATCH, AE_PA_SCRATCH_GESTURE_EVENTS)) & 4u) != 0) {
        ae_PlayerAct_AnimSpeedHalf(r);
    }
}

void ae_PlayerAct_FaceStickRotateA(AeRam *r)
{
    ae_PlayerAct_FaceStick(r);
    (void)ae_sdk_ApplyMatrixSV(r, AE_PA_FACE_MATRIX_A,
            pa_field(AE_PLAYER_ACT_PLAYER, AE_PA_PLAYER_DESIRED_DIR),
            pa_field(AE_PLAYER_ACT_PLAYER, AE_PA_PLAYER_DESIRED_DIR));
}

void ae_PlayerAct_FaceStickRotateB(AeRam *r)
{
    ae_PlayerAct_FaceStick(r);
    (void)ae_sdk_ApplyMatrixSV(r, AE_PA_FACE_MATRIX_B,
            pa_field(AE_PLAYER_ACT_PLAYER, AE_PA_PLAYER_DESIRED_DIR),
            pa_field(AE_PLAYER_ACT_PLAYER, AE_PA_PLAYER_DESIRED_DIR));
}

void ae_PlayerAct_AimModeEnter(AeRam *r, const AePlayerActHooks *hooks)
{
    if (hooks == NULL || hooks->AimModeEnter == NULL) {
        pa_fail_hook(r, "AimModeEnter");
        return;
    }
    hooks->AimModeEnter(hooks->user, r);
}

void ae_PlayerAct_AimModeLeave(AeRam *r, const AePlayerActHooks *hooks)
{
    if (hooks == NULL || hooks->AimModeLeave == NULL) {
        pa_fail_hook(r, "AimModeLeave");
        return;
    }
    hooks->AimModeLeave(hooks->user, r);
}

void ae_PlayerAct_FaceRightPerp(AeRam *r, const AePlayerActHooks *hooks)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR + 2u), 0);
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR),
            (uint32_t)pa_sub(0, ae_rd16(r, pa_field(AE_PA_INPUT_BLOCK, AE_PA_INPUT_GADGET_DIR_Z))));
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR + 4u),
            ae_rd16(r, pa_field(AE_PA_INPUT_BLOCK, AE_PA_INPUT_GADGET_DIR_X)));
    ae_PlayerAct_GadgetSpinFromStick(r, hooks);
}

void ae_PlayerAct_FaceLeftPerp(AeRam *r, const AePlayerActHooks *hooks)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR + 2u), 0);
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR),
            ae_rd16(r, pa_field(AE_PA_INPUT_BLOCK, AE_PA_INPUT_GADGET_DIR_Z)));
    ae_wr16(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR + 4u),
            (uint32_t)pa_sub(0, ae_rd16(r, pa_field(AE_PA_INPUT_BLOCK, AE_PA_INPUT_GADGET_DIR_X))));
    ae_PlayerAct_GadgetSpinFromStick(r, hooks);
}

void ae_PlayerAct_FaceStickOnRotation(AeRam *r)
{
    const uint32_t input = AE_PA_INPUT_BLOCK;
    uint32_t hist_index = ae_rd32(r, pa_field(input, AE_PA_INPUT_STICK_HISTORY_INDEX));
    uint32_t history_entry = pa_field(input, AE_PA_INPUT_STICK_HISTORY_BASE + (hist_index << 1));
    uint32_t event = ae_rd16(r, history_entry);
    if (event == 0x10u || event == 0x20u) {
        const uint32_t player = AE_PLAYER_ACT_PLAYER;
        ae_wr16(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR + 2u), 0);
        ae_wr16(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR),
                ae_rd16(r, pa_field(input, AE_PA_INPUT_GADGET_DIR_X)));
        ae_wr16(r, pa_field(player, AE_PA_PLAYER_DESIRED_DIR + 4u),
                ae_rd16(r, pa_field(input, AE_PA_INPUT_GADGET_DIR_Z)));
    }
}

void ae_PlayerAct_HurtVoiceFlag(AeRam *r, const AePlayerActHooks *hooks)
{
    ae_wr16(r, AE_PLAYER_ACT_GADGET_FLAGS, 5);
    if (hooks == NULL || hooks->PlayPlayerVoice == NULL) {
        pa_fail_hook(r, "PlayPlayerVoice");
        return;
    }
    hooks->PlayPlayerVoice(hooks->user, r, 2);
}

