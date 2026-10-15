#ifndef AE_PLAYER_COND_H
#define AE_PLAYER_COND_H

#include "../core/ae_ram.h"
#include "../core/ae_pad.h"
#include "../world/ae_world.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Original globals and fields read or written by PlayerCond_* (tools/structs.txt). */
#define AE_G_PLAYER 0x800EC1E0u
#define AE_G_PLAYER_SCRATCH 0x1F800048u
#define AE_G_PLAYER_NORMALIZED_VEL_DIR 0x1F800000u
#define AE_G_PLAYER_COND_RAND_SEED 0x800AE8F0u
#define AE_G_PLAYER_FLICK_BUSY 0x800F5164u /* unnamed signed halfword in the EXE */
#define AE_G_GADGET4_MODE 0x800F5124u

#define AE_PLAYER_POS_X 0x024u
#define AE_PLAYER_POS_Y 0x028u
#define AE_PLAYER_POS_Z 0x02Cu
#define AE_PLAYER_VEL_X 0x030u
#define AE_PLAYER_VEL_Y 0x034u
#define AE_PLAYER_VEL_Z 0x038u
#define AE_PLAYER_CONTACT 0x050u
#define AE_PLAYER_CONTACT_KIND_BYTE 0x053u
#define AE_PLAYER_ANIM_FRAME 0x05Cu
#define AE_PLAYER_STATE 0x05Eu
#define AE_PLAYER_FACING_X 0x060u
#define AE_PLAYER_FACING_Z 0x064u
#define AE_PLAYER_STATE_FLAGS 0x070u
#define AE_PLAYER_DESIRED_DIR_X 0x074u
#define AE_PLAYER_DESIRED_DIR_Z 0x078u
#define AE_PLAYER_ANIM_LERP 0x08Au
#define AE_PLAYER_ATTACH_TIMER 0x0DEu
#define AE_PLAYER_HEALTH 0x0E8u
#define AE_PLAYER_SURFACE_KIND 0x0ECu
#define AE_PLAYER_GADGET 0x0F2u
#define AE_PLAYER_SLOPE_X 0x104u
#define AE_PLAYER_SLOPE_Z 0x106u
#define AE_PLAYER_MOVE_VEL_Y 0x124u
#define AE_PLAYER_GROUND_Y 0x12Cu
#define AE_PLAYER_HAS_GROUND 0x136u
#define AE_PLAYER_GROUND_TYPE 0x138u
#define AE_PLAYER_WATER_Y 0x13Cu
#define AE_PLAYER_SURFACE_TABLE 0x800AEF60u
#define AE_PLAYER_SURFACE_ENTRY_SIZE 0x10u
#define AE_PLAYER_SURFACE_JUMP_SLOPE_LIMIT 0x0Cu

#define AE_PLAYER_SCRATCH_STATE_TABLE 0x000u
#define AE_PLAYER_SCRATCH_HELD 0x00Cu
#define AE_PLAYER_SCRATCH_CHANGED 0x010u
#define AE_PLAYER_SCRATCH_LEFT_STICK_CLASS 0x014u
#define AE_PLAYER_SCRATCH_RIGHT_STICK_GESTURE 0x016u
#define AE_PLAYER_SCRATCH_GESTURE_EVENTS 0x01Au
#define AE_PLAYER_SCRATCH_LEFT_STICK_MAG 0x01Cu
#define AE_PLAYER_SCRATCH_RIGHT_STICK_MAG 0x01Eu
#define AE_PLAYER_SCRATCH_RIGHT_STICK_CAM_FWD 0x028u
#define AE_PLAYER_SCRATCH_RIGHT_STICK_CAM_SIDE 0x02Au
#define AE_PLAYER_SCRATCH_LEFT_STICK_CAM_FWD 0x02Cu
#define AE_PLAYER_SCRATCH_LEFT_STICK_CAM_SIDE 0x02Eu
#define AE_PLAYER_SCRATCH_STICK_ROTATION 0x030u
#define AE_PLAYER_SCRATCH_GADGET_TRAVEL 0x034u
#define AE_PLAYER_SCRATCH_INPUT_LOCKED 0x038u
#define AE_PLAYER_COND_DIR_X 0x000u
#define AE_PLAYER_COND_DIR_Z 0x004u

#define AE_PLAYER_STATE_INDEX_MASK 0x7Fu
#define AE_PLAYER_STATE_ENTRY_SIZE 0x08u
#define AE_PLAYER_STATE_FIRST_FRAME 0x000u
#define AE_PLAYER_STATE_LAST_FRAME 0x002u
#define AE_PLAYER_HISTORY_SIZE 40u
#define AE_PLAYER_HISTORY_WINDOW 10u
#define AE_PLAYER_HISTORY_VALUE_SIZE 2u

/* Hooks for original calls outside this task's 65-function batch.
 * GetGroundHeight is called directly through its verified ae_GetGroundHeight port; its existing
 * world hook table supplies GetSolidHeight. The three Player* callbacks remain named game hooks. */
typedef int32_t (*AePlayerCondBoolHook)(void *user, AeRam *r);
typedef void (*AePlayerCondVoidHook)(void *user, AeRam *r);
typedef struct AePlayerCondHooks {
    void *user;
    const AeWorldHooks *world; /* GetGroundHeight's already-ported GetSolidHeight dependency */
    AePlayerCondBoolHook PlayerIsOnGround; /* 800652E4 */
    AePlayerCondVoidHook PlayerSplash; /* 8006B950 */
    AePlayerCondVoidHook PlayerFxJumpBurst; /* 8006C58C */
} AePlayerCondHooks;

int32_t ae_PlayerCond_False(AeRam *r);
int32_t ae_PlayerCond_JumpHeld(AeRam *r);
int32_t ae_PlayerCond_JumpPressed(AeRam *r);
int32_t ae_PlayerCond_RStickEvent02(AeRam *r);
int32_t ae_PlayerCond_RStickEvent40(AeRam *r);
int32_t ae_PlayerCond_RStickBackHard(AeRam *r);
int32_t ae_PlayerCond_GadgetBusyOrGesture(AeRam *r);
int32_t ae_PlayerCond_FacingDesiredDir(AeRam *r);
int32_t ae_PlayerCond_RStickSideNeg(AeRam *r);
int32_t ae_PlayerCond_RStickSidePos(AeRam *r);
int32_t ae_PlayerCond_RStickFwd(AeRam *r);
int32_t ae_PlayerCond_RStickBack(AeRam *r);
int32_t ae_PlayerCond_AttachTimerDone(AeRam *r);
int32_t ae_PlayerCond_GroundBehindLow(AeRam *r, const AePlayerCondHooks *h);
int32_t ae_PlayerCond_RStickGestureOrTravel(AeRam *r);
int32_t ae_PlayerCond_AnimDoneRandom(AeRam *r);
int32_t ae_PlayerCond_NoMoveInput(AeRam *r);
int32_t ae_PlayerCond_LStickFwd(AeRam *r);
int32_t ae_PlayerCond_LStickBack(AeRam *r);
int32_t ae_PlayerCond_RStickEvent04(AeRam *r);
int32_t ae_PlayerCond_RStickEvent08(AeRam *r);
int32_t ae_PlayerCond_NoGestureR3Held(AeRam *r);
int32_t ae_PlayerCond_RStickRotating(AeRam *r);
int32_t ae_PlayerCond_Gadget1RStickFwdFlick(AeRam *r);
int32_t ae_PlayerCond_Gadget3RStickBack(AeRam *r);
int32_t ae_PlayerCond_RStickGesture10(AeRam *r);
int32_t ae_PlayerCond_RStickGesture20(AeRam *r);
int32_t ae_PlayerCond_RStickNoGesture(AeRam *r);
int32_t ae_PlayerCond_GestureEvent04(AeRam *r);
int32_t ae_PlayerCond_GestureEvent08(AeRam *r);
int32_t ae_PlayerCond_LStickIdleL3Held(AeRam *r);
int32_t ae_PlayerCond_LStickIdle(AeRam *r);
int32_t ae_PlayerCond_LStickClass2(AeRam *r);
int32_t ae_PlayerCond_LStickClass4(AeRam *r);
int32_t ae_PlayerCond_LStickClass8(AeRam *r);
int32_t ae_PlayerCond_LStickFull(AeRam *r);
int32_t ae_PlayerCond_RStickFull(AeRam *r);
int32_t ae_PlayerCond_LStickPartial(AeRam *r);
int32_t ae_PlayerCond_RStickPartial(AeRam *r);
int32_t ae_PlayerCond_LandingSoon(AeRam *r, const AePlayerCondHooks *h);
int32_t ae_PlayerCond_FacingContactWall(AeRam *r);
int32_t ae_PlayerCond_ContactKind8(AeRam *r);
int32_t ae_PlayerCond_NotContactKind8(AeRam *r);
int32_t ae_PlayerCond_FallingHigh(AeRam *r);
int32_t ae_PlayerCond_RStickEvent10(AeRam *r);
int32_t ae_PlayerCond_RStickEvent20(AeRam *r);
int32_t ae_PlayerCond_FirstFrameRStickEvent10(AeRam *r);
int32_t ae_PlayerCond_FirstFrameRStickEvent20(AeRam *r);
int32_t ae_PlayerCond_JumpPressedAtApex(AeRam *r, const AePlayerCondHooks *h);
int32_t ae_PlayerCond_LStickSidePos(AeRam *r);
int32_t ae_PlayerCond_LStickSideNeg(AeRam *r);
int32_t ae_PlayerCond_LStickSideAny(AeRam *r);
int32_t ae_PlayerCond_LStickIdleStillMoving(AeRam *r);
int32_t ae_PlayerCond_GestureEvent0C(AeRam *r);
int32_t ae_PlayerCond_InWater(AeRam *r);
int32_t ae_PlayerCond_WadingOnGround(AeRam *r, const AePlayerCondHooks *h);
int32_t ae_PlayerCond_ContactWall3B(AeRam *r);
int32_t ae_PlayerCond_JumpOutOfWater(AeRam *r, const AePlayerCondHooks *h);
int32_t ae_PlayerCond_CanJump(AeRam *r);
int32_t ae_PlayerCond_LStickIdleL3Pressed(AeRam *r);
int32_t ae_PlayerCond_Dead(AeRam *r);
int32_t ae_PlayerCond_Gadget4Mode1(AeRam *r);
int32_t ae_PlayerCond_AnimDone(AeRam *r);
int32_t ae_PlayerCond_False2(AeRam *r);
int32_t ae_PlayerCond_Still(AeRam *r);

#ifdef __cplusplus
}
#endif
#endif
