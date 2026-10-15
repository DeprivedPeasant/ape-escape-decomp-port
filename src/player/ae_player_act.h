#ifndef AE_PLAYER_ACT_H
#define AE_PLAYER_ACT_H

#include "../core/ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Original globals and field offsets used by the PlayerAct / PlayerSet batch. */
#define AE_PLAYER_ACT_PLAYER 0x800EC1E0u
#define AE_PLAYER_ACT_PARAMS 0x800EC010u
#define AE_PLAYER_ACT_STATE_SETS 0x800F4DE0u
#define AE_PLAYER_ACT_MODE 0x800F44A0u
#define AE_PLAYER_ACT_SCRATCH 0x1F800000u
#define AE_PLAYER_ACT_PLAYER_SCRATCH 0x1F800048u
#define AE_PLAYER_ACT_STATE_CHANGE_MODE 0x800EC23Eu
#define AE_PLAYER_ACT_GADGET4_STATE 0x800F5120u
#define AE_PLAYER_ACT_GADGET_SPIN_LIMIT 0x800F5148u
#define AE_PLAYER_ACT_GADGET_SPIN_AMOUNT 0x800F514Au
#define AE_PLAYER_ACT_GADGET_SPIN_DELTA 0x800F514Cu
#define AE_PLAYER_ACT_GADGET_FLAGS 0x800F5164u
#define AE_PLAYER_ACT_TINT_EFFECT 0x800BAD78u
#define AE_PLAYER_ACT_DR_MOVE_QUEUE 0x800F4354u
#define AE_PLAYER_ACT_DR_MOVE_QUEUE_BASE 0x800F8350u
#define AE_PLAYER_ACT_DR_MOVE_QUEUE_LIMIT 0x800F837Cu
#define AE_PLAYER_ACT_PLAYER_IDLE_BLOCK 0x800B4D70u
#define AE_PLAYER_ACT_MODE_DISPATCH_OLD 0x800A6040u
#define AE_PLAYER_ACT_MODE_DISPATCH_NEW 0x800A6070u
#define AE_PA_MODE_ACTOR_POINTER 0x800B4DC4u
#define AE_PA_MODE_CALLBACK_POINTER 0x800B4DD0u
#define AE_PA_MODE_IDLE_ACTOR_POINTER 0x800B4D70u
#define AE_PA_MODE_IDLE_BLOCK 0x800E68D0u
#define AE_PA_MODE_HUD_FLAGS 0x800F4DD0u
#define AE_PA_MODE_STAGE_ENTRY_FLAGS 0x800F4450u
#define AE_PA_MODE_TRANSITION_FLAGS 0x800EC26Cu
#define AE_PA_MODE_REQUEST13_FLAGS 0x800EC2E0u
#define AE_PA_MODE_GADGET4_EFFECT 0x800F5174u
#define AE_PA_MODE_REQUEST_FLAGS 0x800F44A2u
#define AE_PA_MODE_REQUEST_MODE11_VALUE 0x800F4450u
#define AE_PA_MODE_TEMP_VECTOR 0x801FFEE8u
#define AE_PA_CURRENT_AREA 0x800F4476u
#define AE_PA_GADGET4_MODE 0x800F5124u
#define AE_PA_GADGET4_AUX 0x800F5128u
#define AE_PA_GADGET4_TURN_STATE 0x004u
#define AE_PA_GADGET4_DIRECTION_FLAGS 0x008u
#define AE_PA_TINT_EFFECT_MODE 0x002u
#define AE_PA_TINT_EFFECT_DURATION 0x006u

#define AE_PA_ACTOR_POSITION 0x0024u
#define AE_PA_ACTOR_FACING 0x0060u
#define AE_PA_ACTOR_MODEL 0x0070u
#define AE_PA_ACTOR_FALL_STATE 0x008Eu
#define AE_PA_ACTOR_MODE 0x008Fu
#define AE_PA_MODEL_FLAGS 0x0018u
#define AE_PA_MODE_IDLE_FLAGS 0x0046u
#define AE_PA_MODE_IDLE_ACTIVE 0x0032u
#define AE_PA_MODE_IDLE_GADGET 0x0084u
#define AE_PA_MODE_IDLE_BUFFER 0x0080u
#define AE_PA_MODE_ACTOR_TRANSFORM_SIZE 0x0010u
#define AE_PA_MODE_ACTOR_ROTATION_SIZE 0x0008u

#define AE_PA_CLIMB_TARGET_X 0x00u
#define AE_PA_CLIMB_TARGET_Y 0x02u
#define AE_PA_CLIMB_TARGET_Z 0x04u
#define AE_PA_CLIMB_TARGET_HEIGHT 0x06u
#define AE_PA_CLIMB_TARGET_FLAGS 0x08u
#define AE_PA_CLIMB_TARGET_Y_OFFSET 0x0Au

#define AE_PA_VECTOR_X 0x0000u
#define AE_PA_VECTOR_Y 0x0004u
#define AE_PA_VECTOR_Z 0x0008u
#define AE_PA_SVECTOR_X 0x0000u
#define AE_PA_SVECTOR_Y 0x0002u
#define AE_PA_SVECTOR_Z 0x0004u

#define AE_PA_PLAYER_MODEL 0x000u
#define AE_PA_PLAYER_ANIM_SET 0x018u
#define AE_PA_PLAYER_POSITION 0x024u
#define AE_PA_PLAYER_VELOCITY 0x030u
#define AE_PA_PLAYER_FACING 0x060u
#define AE_PA_PLAYER_DESIRED_DIR 0x074u
#define AE_PA_PLAYER_CLIMB_TARGET 0x0B4u
#define AE_PA_PLAYER_ROPE 0x0C0u
#define AE_PA_PLAYER_ATTACH_FLAGS 0x0E0u
#define AE_PA_PLAYER_CURRENT_GADGET 0x0F2u
#define AE_PA_PLAYER_TINT_TARGET 0x0F8u
#define AE_PA_PLAYER_FALL_FRAMES 0x0FCu
#define AE_PA_PLAYER_SQUASH_TIMER 0x0FEu
#define AE_PA_PLAYER_SQUASH_FRAME 0x100u
#define AE_PA_PLAYER_ANIM_FRAME 0x05Cu
#define AE_PA_PLAYER_STATE 0x05Eu
#define AE_PA_PLAYER_ANIM_LERP 0x08Au
#define AE_PA_PLAYER_ANIM_BLEND 0x08Cu
#define AE_PA_PLAYER_ANIM_SPEED 0x08Eu
#define AE_PA_PLAYER_ANIM_BLEND_FROM 0x090u
#define AE_PA_PLAYER_STATE_FLAGS 0x070u
#define AE_PA_PLAYER_TARGET_MOVE_VEL 0x114u
#define AE_PA_PLAYER_MOVE_VEL 0x120u
#define AE_PA_PLAYER_VEL_FRAC 0x108u
#define AE_PA_PLAYER_GROUND_Y 0x12Cu
#define AE_PA_PLAYER_CONTACT 0x050u
#define AE_PA_PLAYER_HEALTH 0x0E8u
#define AE_PA_PLAYER_PREVIOUS_GADGET 0x0F3u
#define AE_PA_PLAYER_UP_VECTOR 0x068u
#define AE_PA_PLAYER_CLIMB_TARGET_FLAGS 0x0E0u
#define AE_PA_PLAYER_ROPE_FLAGS 0x0E0u
#define AE_PA_PLAYER_ROPE_MODE 0x0C0u

#define AE_PA_PLAYER_ROPE_FIRST_X 0x0C0u
#define AE_PA_PLAYER_ROPE_FIRST_Y 0x0C2u
#define AE_PA_PLAYER_ROPE_FIRST_Z 0x0C4u
#define AE_PA_PLAYER_ROPE_SECOND_X 0x0C6u
#define AE_PA_PLAYER_ROPE_SECOND_Y 0x0C8u
#define AE_PA_PLAYER_ROPE_SECOND_Z 0x0CAu
#define AE_PA_PLAYER_ROPE_LENGTH_FRACTION 0x0CCu
#define AE_PA_PLAYER_ROPE_LENGTH_SQRT 0x0CEu
#define AE_PA_PLAYER_ROPE_NORMAL 0x0D0u

#define AE_PA_PARAMS_GRAVITY 0x004u
#define AE_PA_PARAMS_JUMP_IMPULSE 0x008u
#define AE_PA_PARAMS_JUMP_HOLD_BOOST 0x00Cu
#define AE_PA_PARAMS_JUMP_VEL2 0x010u
#define AE_PA_PARAMS_DROP_VEL 0x014u
#define AE_PA_PARAMS_SQUASH_FRAMES 0x018u
#define AE_PA_PARAMS_MOVE_SPEED 0x024u
#define AE_PA_PARAMS_JUMP_SPEED_PCT 0x028u
#define AE_PA_PARAMS_JUMP_SPEED_PCT2 0x02Cu
#define AE_PA_PARAMS_JUMP_SPEED_PCT3 0x030u
#define AE_PA_PARAMS_FLYER_THRUST_A 0x034u
#define AE_PA_PARAMS_FLYER_LAUNCH_A 0x038u
#define AE_PA_PARAMS_FLYER_LAUNCH_B 0x03Cu
#define AE_PA_PARAMS_FLYER_THRUST_B 0x040u
#define AE_PA_PARAMS_FLYER_MAX_THRUST 0x044u

#define AE_PA_SCRATCH_INPUT_X 0x000u
#define AE_PA_SCRATCH_INPUT_Y 0x004u
#define AE_PA_SCRATCH_INPUT_Z 0x008u
#define AE_PA_SCRATCH_DIR_X 0x008u
#define AE_PA_SCRATCH_DIR_Y 0x00Au
#define AE_PA_SCRATCH_DIR_Z 0x00Cu
#define AE_PA_SCRATCH_GESTURE_EVENTS 0x062u
#define AE_PA_SCRATCH_STICK_ROTATION 0x078u

/* PlayerInput globals, relative to the input block at 0x800E5320. */
#define AE_PA_INPUT_BLOCK 0x800E5320u
#define AE_PA_INPUT_MOVE_PUSH_STATE 0x178u
#define AE_PA_INPUT_GADGET_STICK_MAG 0x182u
#define AE_PA_INPUT_STICK_HISTORY_INDEX 0x170u
#define AE_PA_INPUT_GADGET_DIR_X 0x19Cu
#define AE_PA_INPUT_GADGET_DIR_Z 0x1A0u
#define AE_PA_INPUT_MOVE_DIR_X 0x194u
#define AE_PA_INPUT_MOVE_DIR_Z 0x198u
#define AE_PA_INPUT_STICK_HISTORY_BASE 0x0A4u
#define AE_PA_INPUT_FLYER_PULSE_FRAME 0x1B4u
#define AE_PA_INPUT_FLYER_PULSE_TIMER 0x1BCu

#define AE_PA_SET_ANIM 0x00u
#define AE_PA_SET_STATES 0x04u
#define AE_PA_SET_TRANSITIONS 0x08u
#define AE_PA_SET_EVENTS 0x0Cu
#define AE_PA_SET_SIZE 0x10u
#define AE_PA_STATE_ENTRY_SIZE 0x08u
#define AE_PA_STATE_START_FRAME 0x00u
#define AE_PA_STATE_END_FRAME 0x02u
#define AE_PA_STATE_PACKED_MODES 0x04u

typedef struct AePlayerActHooks {
    void *user;
    void (*AimRequestClear)(void *user, AeRam *r);
    int32_t (*PlayerCond_JumpHeld)(void *user, AeRam *r);
    int32_t (*PlayerInput_StickGesture)(void *user, AeRam *r);
    void (*PadRumble)(void *user, AeRam *r, int32_t motor, int32_t strength, int32_t frames);
    void (*Gadget3AimUpdateAmmo)(void *user, AeRam *r);
    void (*AimModeEnter)(void *user, AeRam *r);
    void (*AimModeLeave)(void *user, AeRam *r);
    void (*PlayPlayerVoice)(void *user, AeRam *r, int32_t voice_id);
    void (*EffectGroundImpactAtPlayer)(void *user, AeRam *r);
    void (*PlayerDamage)(void *user, AeRam *r, int32_t amount);
    void (*PlaySfx)(void *user, AeRam *r, int32_t sound_id);
    void (*StageRequest)(void *user, AeRam *r, int32_t code, int32_t area, int32_t argument);
    void (*EffectBigBurst)(void *user, AeRam *r, int32_t x, int32_t y, int32_t z);
    int32_t (*StageGoalReached)(void *user, AeRam *r);
    void (*HudPanelA_Start)(void *user, AeRam *r, int32_t mode);
    void (*HeapFree)(void *user, AeRam *r, uint32_t address);
    void (*CameraAreaScriptDispatch)(void *user, AeRam *r, int32_t index, int32_t argument, uint32_t table);
    void (*PlayerSelectGadget)(void *user, AeRam *r, int32_t gadget, int32_t previous_gadget);
    void (*PlayerIdleModelSetup)(void *user, AeRam *r, uint32_t blob);
    uint32_t (*AnimFramePtr)(void *user, AeRam *r, uint32_t instance, int32_t frame);
    void (*DR_MOVE)(void *user, AeRam *r, uint32_t command, uint32_t rectangle, int32_t x, int32_t y);
    void (*PlayerModeCallback)(void *user, AeRam *r, uint32_t function, int32_t argument);
    uint32_t incoming_a1;
    uint32_t incoming_a2;
    uint32_t incoming_a3;
} AePlayerActHooks;

void ae_PlayerAct_ScaleMoveVel(AeRam *r, const AePlayerActHooks *hooks, uint32_t player, uint32_t gadget4_state);
void ae_PlayerAct_JumpHoldBoost(AeRam *r, const AePlayerActHooks *hooks);
void ae_PlayerAct_JumpPct3(AeRam *r);
void ae_PlayerAct_VerticalOnly(AeRam *r);
void ae_PlayerAct_JumpPct2(AeRam *r);
void ae_PlayerAct_SquashReset(AeRam *r, const AePlayerActHooks *hooks);
void ae_PlayerAct_FlyerThrust(AeRam *r, const AePlayerActHooks *hooks);
void ae_PlayerAct_FlyerLaunchA(AeRam *r);
void ae_PlayerAct_FlyerLaunchB(AeRam *r);
void ae_PlayerAct_GadgetSpinFromStick(AeRam *r, const AePlayerActHooks *hooks);
void ae_PlayerAct_Gadget3Aim(AeRam *r, const AePlayerActHooks *hooks);
void ae_PlayerAct_DriftForward(AeRam *r);
void ae_PlayerAct_NegateInputXZ(AeRam *r);
void ae_PlayerAct_TurnAround(AeRam *r);
void ae_PlayerAct_TurnAroundAndJump(AeRam *r, const AePlayerActHooks *hooks, uint32_t player, uint32_t gadget4_state);
void ae_PlayerAct_Nop(AeRam *r);
void ae_PlayerAct_FaceStick(AeRam *r);
void ae_PlayerAct_JumpFromLStick(AeRam *r);
void ae_PlayerAct_StepBackDrop(AeRam *r);
void ae_PlayerAct_BounceBack(AeRam *r, const AePlayerActHooks *hooks);
void ae_PlayerAct_ExitToMode0Forward(AeRam *r, const AePlayerActHooks *hooks);
void ae_PlayerAct_AnimSpeedNormal(AeRam *r);
void ae_PlayerAct_AnimSpeedHalf(AeRam *r);
void ae_PlayerAct_FaceStickMaybeHalfSpeed(AeRam *r);
void ae_PlayerAct_FaceStickRotateA(AeRam *r);
void ae_PlayerAct_FaceStickRotateB(AeRam *r);
void ae_PlayerAct_AimModeEnter(AeRam *r, const AePlayerActHooks *hooks);
void ae_PlayerAct_AimModeLeave(AeRam *r, const AePlayerActHooks *hooks);
void ae_PlayerAct_FaceRightPerp(AeRam *r, const AePlayerActHooks *hooks);
void ae_PlayerAct_FaceLeftPerp(AeRam *r, const AePlayerActHooks *hooks);
void ae_PlayerAct_TurnFacingLeft90(AeRam *r);
void ae_PlayerAct_TurnFacingRight90(AeRam *r);
void ae_PlayerAct_FaceStickOnRotation(AeRam *r);
void ae_PlayerAct_Rebound(AeRam *r);
void ae_PlayerAct_HurtVoiceFlag(AeRam *r, const AePlayerActHooks *hooks);
void ae_PlayerAct_Land(AeRam *r, const AePlayerActHooks *hooks);
void ae_PlayerSetClimbTarget(AeRam *r, int32_t x, int32_t y, int32_t z, int32_t target_flags,
                             int32_t y_offset, int32_t target_height);
void ae_PlayerSetRope(AeRam *r, uint32_t first, uint32_t second);
void ae_PlayerSetEyeFrame(AeRam *r, const AePlayerActHooks *hooks, int32_t frame);
void ae_PlayerSetTintEffect(AeRam *r, int32_t mode);
void ae_PlayerSetState(AeRam *r, const AePlayerActHooks *hooks, uint32_t state);
void ae_PlayerSetMode(AeRam *r, const AePlayerActHooks *hooks, int32_t mode);

#ifdef __cplusplus
}
#endif
#endif
