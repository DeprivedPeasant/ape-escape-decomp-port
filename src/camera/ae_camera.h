#ifndef AE_CAMERA_H
#define AE_CAMERA_H

/* Gameplay camera routines running against the original AeRam address space. */
#include "../core/ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AE_G_CAMERA_SLOTS       0x800B4D40u
#define AE_G_CAMERA_STATE       0x800E68D0u
#define AE_G_CAMERA_FOCUS       0x800E68C0u
#define AE_G_CAMERA_EYE         0x800F45A8u
#define AE_G_CAMERA_TARGET      0x800F45B0u
#define AE_G_PLAYER             0x800EC1E0u
#define AE_G_PLAYER_SCRATCH     0x800E53B0u
#define AE_G_PAD_HELD           0x800E5478u
#define AE_G_RIGHT_STICK_MAG    0x800E54A0u
#define AE_G_PLAYER_MODE        0x800F44A0u
#define AE_G_CURRENT_AREA       0x800F4476u
#define AE_G_STAGE_FLAGS        0x800F4450u
#define AE_G_CAMERA_REMOVE_AREA 0x800B4D4Cu
#define AE_G_CAMERA_RADAR_TARGET 0x800B4D44u
#define AE_G_CAMERA_GADGET_PRIM_COUNT 0x800B8CB0u
#define AE_G_CAMERA_GADGET_PRIM_CURSOR 0x800EC354u
#define AE_G_CAMERA_GADGET_ORDER_INDEX 0x800EC35Cu
#define AE_G_CAMERA_DRAW_ENV    0x800EC3D0u
#define AE_G_CAMERA_PRIM_BUCKETS 0x800EC450u
#define AE_G_CAMERA_MATRIX      0x800E78C0u
#define AE_G_STAGE_EXIT_PENDING 0x800B8CE0u
#define AE_CAMERA_SLOT_SIZE     0xB0u
#define AE_CAMERA_SLOT_COUNT    12u
#define AE_CAMERA_REGION_STRIDE 0x30u

/* CameraSlot byte offsets. Every access in the port uses one of these names. */
#define AE_CAM_KIND             0x00u
#define AE_CAM_LINKED_SLOT      0x02u
#define AE_CAM_SCRIPT_ACTIVE    0x03u
#define AE_CAM_FOLLOW_DIST      0x04u
#define AE_CAM_EYE_HEIGHT       0x06u
#define AE_CAM_TARGET_HEIGHT    0x08u
#define AE_CAM_EYE              0x0Au
#define AE_CAM_TARGET           0x12u
#define AE_CAM_LAST_PLAYER_POS  0x1Au
#define AE_CAM_UNK_2C           0x2Cu
#define AE_CAM_STILL_FRAMES     0x30u
#define AE_CAM_YAW_HOLD         0x31u
#define AE_CAM_MODE_TIMER       0x32u
#define AE_CAM_FIXED_ACTIVE     0x33u
#define AE_CAM_CLAMP_STATE      0x34u
#define AE_CAM_CLAMP_TIMER      0x35u
#define AE_CAM_CLAMP_COUNT      0x36u
#define AE_CAM_CLAMP_FLAG       0x37u
#define AE_CAM_DISTANCE         0x38u
#define AE_CAM_YAW              0x40u
#define AE_CAM_SAVED_YAW        0x44u
#define AE_CAM_TARGET_YAW       0x48u
#define AE_CAM_YAW_DELTA        0x4Cu
#define AE_CAM_EYE_VELOCITY     0x50u
#define AE_CAM_TARGET_VELOCITY  0x58u
#define AE_CAM_SIDE             0x60u
#define AE_CAM_DIRECTION_X      0x64u
#define AE_CAM_DIRECTION_Z      0x68u
#define AE_CAM_ORBIT_X          0x6Cu
#define AE_CAM_ORBIT_Y          0x6Eu
#define AE_CAM_HEIGHT_A         0x70u
#define AE_CAM_HEIGHT_B         0x74u
#define AE_CAM_FOLLOW_FLAG_A    0x80u
#define AE_CAM_FOLLOW_FLAG_B    0x81u
#define AE_CAM_SCRIPT_POINTER   0x84u
#define AE_CAM_SCRIPT_TIMER     0x88u
#define AE_CAM_SCRIPT_STEP      0x8Au
#define AE_CAM_SCRIPT_RATE      0x8Cu
#define AE_CAM_REGION_FLAG      0x8Eu
#define AE_CAM_SCRIPT_STATE     0x90u
#define AE_CAM_REGION_LINK_INDEX 0x93u
#define AE_CAM_SCRIPT_FOCUS     0x94u
#define AE_CAM_SCRIPT_VELOCITY  0x9Cu
#define AE_CAM_FOCUS_DISTANCE   0xA4u
#define AE_CAM_FOCUS_HEIGHT     0xA6u
#define AE_CAM_ANCHOR           0xA8u
#define AE_CAM_EYE_X            (AE_CAM_EYE + 0x00u)
#define AE_CAM_EYE_Y            (AE_CAM_EYE + 0x02u)
#define AE_CAM_EYE_Z            (AE_CAM_EYE + 0x04u)
#define AE_CAM_TARGET_X         (AE_CAM_TARGET + 0x00u)
#define AE_CAM_TARGET_Y         (AE_CAM_TARGET + 0x02u)
#define AE_CAM_TARGET_Z         (AE_CAM_TARGET + 0x04u)

/* Global fields relative to 0x800E68D0. */
#define AE_CAM_G_REGION_DELAY   0x0Cu
#define AE_CAM_G_REGION_OLD     0x0Du
#define AE_CAM_G_ACTIVE_INDEX   0x0Eu
#define AE_CAM_G_ACTIVE_SLOT    0x08u
#define AE_CAM_G_FOLLOW_INDEX   0x18u
#define AE_CAM_G_MODE_TIMER     0x32u
#define AE_CAM_G_UNK_3E         0x3Eu
#define AE_CAM_G_REGION_SLOT    0x1Bu
#define AE_CAM_G_REGION_PTR     0x4Cu
#define AE_CAM_G_EYE_HEIGHT_ON  0x3Fu
#define AE_CAM_G_DISTANCE_ON    0x40u
#define AE_CAM_G_EYE_HEIGHT     0x34u
#define AE_CAM_G_FOLLOW_DIST    0x36u
#define AE_CAM_G_FIXED_ACTIVE   0x38u
#define AE_CAM_G_AIM_MODE       0x46u
#define AE_CAM_G_FOLLOW_GUARD   0x47u
#define AE_CAM_G_STAGE_MODE     0x48u
#define AE_CAM_G_SLOT_COUNT     0x02u
#define AE_CAM_G_PRIOR_SLOT     0x04u
#define AE_CAM_G_ACTIVE_MODE    0x06u
#define AE_CAM_G_REMOVE_GUARD   0x41u

/* Camera-region record fields. */
#define AE_CAM_REGION_FLAGS     0x02u

/* Scratchpad fields used by the camera routines. */
#define AE_CAM_SCR_DESIRED_EYE  0x00u
#define AE_CAM_SCR_DESIRED_TGT  0x08u
#define AE_CAM_SCR_PLAYER_X     0x10u
#define AE_CAM_SCR_PLAYER_Z     0x14u
#define AE_CAM_SCR_FOCUS        0x18u
#define AE_CAM_SCR_PLAYER_Y     0x1Au
#define AE_CAM_SCR_CAMERA_POSITION_Y 0x12u
#define AE_CAM_SCR_GROUND_REF   0x2Cu
#define AE_CAM_SCR_DIRECTION    0x40u
#define AE_CAM_SCR_PROJECTED_X  0x60u
#define AE_CAM_SCR_PROJECTED_Y  0x64u
#define AE_CAM_SCR_MOVE_X       0x68u
#define AE_CAM_SCR_MOVE_Y       0x6Cu
#define AE_CAM_SCR_MOVE_Z       0x70u
#define AE_CAM_SCR_AIM_CROSS    0x74u
#define AE_CAM_SCR_AIM_DOT      0x78u

/* Player fields used by the original camera. */
#define AE_PLAYER_POSITION      0x24u
#define AE_PLAYER_FACING        0x60u
#define AE_PLAYER_CURRENT_GADGET 0xF2u
#define AE_PLAYER_STATE_FLAGS   0x70u
#define AE_PLAYER_STATE         0x5Eu
#define AE_PLAYER_POSITION_X    (AE_PLAYER_POSITION + 0x00u)
#define AE_PLAYER_POSITION_Y    (AE_PLAYER_POSITION + 0x04u)
#define AE_PLAYER_POSITION_Z    (AE_PLAYER_POSITION + 0x08u)
#define AE_PLAYER_FACING_X      (AE_PLAYER_FACING + 0x00u)
#define AE_PLAYER_FACING_Z      (AE_PLAYER_FACING + 0x04u)
#define AE_PLAYER_INPUT_MODE    0x150u

/* Other original globals read by the requested entry points. */
#define AE_G_CAMERA_SCRIPT_CLEAR 0x800AF130u
#define AE_G_CAMERA_SCRIPT_ACTIVE 0x800AF131u
#define AE_G_CAMERA_REGION_FLAGS 0x800E68EBu
#define AE_G_CAMERA_REGION_TRANSITION 0x800E68DCu
#define AE_G_PLAYER_SCRATCH_RIGHT_X 0x800E53C2u
#define AE_G_PLAYER_SCRATCH_RIGHT_Y 0x800E53C3u
#define AE_G_GAMEPAD_PRESSED 0x800E5478u
#define AE_G_GAMEPAD_CHANGED 0x800E5480u
#define AE_G_CAMERA_AIM_CONTEXT 0x800E6902u
#define AE_G_PLAYER_SCRATCH_X 0x800E53C2u
#define AE_G_PLAYER_SCRATCH_Y 0x800E53C3u

/* Temporary RAM addresses passed to original pointer-taking SDK callbacks.
 * The checker excludes the ABI stack window, and every vector is initialized. */
#define AE_CAMERA_TEMP_BASE     0x801FF100u
#define AE_CAMERA_TEMP_AIM      (AE_CAMERA_TEMP_BASE + 0x00u)
#define AE_CAMERA_TEMP_VECTOR   (AE_CAMERA_TEMP_BASE + 0x10u)
#define AE_CAMERA_TEMP_FOCUS    (AE_CAMERA_TEMP_BASE + 0x20u)
#define AE_CAMERA_TEMP_MATRIX   (AE_CAMERA_TEMP_BASE + 0x40u)

/* External original routines remain explicit host/test hooks. The four words
 * are the original a0-a3 bit patterns; pointer arguments are AeRam addresses. */
typedef int32_t (*AeCameraHook)(void *user, AeRam *ram,
                                uint32_t a0, uint32_t a1,
                                uint32_t a2, uint32_t a3);

typedef struct AeCameraHooks {
    void *user;
    AeCameraHook Atan2Safe;
    AeCameraHook VectorNormalSS;
    AeCameraHook SquareRoot0;
    AeCameraHook GetGroundHeight;
    AeCameraHook rsin;
    AeCameraHook rcos;
    AeCameraHook RotTransPers;
    AeCameraHook SetRotMatrix;
    AeCameraHook SetTransMatrix;
    AeCameraHook memset;
    AeCameraHook PlayerCanExitStage;
    AeCameraHook PlayerIsSubmerged;
    AeCameraHook CameraRcCarActive;
    AeCameraHook CameraRcCarFollow;
    AeCameraHook IsFlightArea;
    AeCameraHook FUN_8013d000;
    AeCameraHook BossUpdate;
    AeCameraHook CameraAreaScriptDispatch;
    AeCameraHook CallIndexedHandler;
    AeCameraHook RotMatrixY;
    AeCameraHook ApplyMatrixSV;
    AeCameraHook MatrixIdentity;
    AeCameraHook CameraGetPlayerDir;
    AeCameraHook AimRequestStart;
    AeCameraHook AimRequestClear;
    AeCameraHook AimModeEnter;
    AeCameraHook AimModeLeave;
    AeCameraHook CameraFixedModeOff;
    AeCameraHook StageExitPending;
    AeCameraHook BossAimSfx;
    AeCameraHook BossKindForArea;
    AeCameraHook ScreenFadeSet;
    AeCameraHook PlaySfx;
    AeCameraHook HudDrawMonkeyInfo;
    AeCameraHook RadarUsable;
    AeCameraHook MonkeyFlag248Set;
    AeCameraHook CameraRadarFocusUpdate;
    AeCameraHook CameraRadarFocusSelect;
} AeCameraHooks;

void ae_CameraKindFixed_Nop(AeRam *r, const AeCameraHooks *h, uint32_t slot); /* 80046970 */
void ae_CameraSlotInitFollow(AeRam *r, const AeCameraHooks *h, uint32_t slot, int32_t kind); /* 80047104 */
int32_t ae_CameraSlotCreate(AeRam *r, const AeCameraHooks *h, int32_t kind, uint32_t eye); /* 800471E4 */
void ae_CameraSlotRemove(AeRam *r, const AeCameraHooks *h, uint32_t index); /* 80047290 */
void ae_CameraSlotAimAtFocus(AeRam *r, const AeCameraHooks *h, uint32_t slot); /* 80047360 */
void ae_CameraSlotSmoothEye(AeRam *r, const AeCameraHooks *h, uint32_t slot, int32_t dx, int32_t dy, int32_t dz); /* 80047464 */
void ae_CameraSlotSmoothTarget(AeRam *r, const AeCameraHooks *h, uint32_t slot, int32_t dx, int32_t dy, int32_t dz); /* 80047558 */
void ae_CameraKindAim_SnapToFocus(AeRam *r, const AeCameraHooks *h, uint32_t slot); /* 80047654 */
void ae_CameraFollowDispatch(AeRam *r, const AeCameraHooks *h, uint32_t slot); /* 800482C0 */
void ae_CameraKindAimUpdate(AeRam *r, const AeCameraHooks *h, uint32_t slot); /* 80048454 */
void ae_CameraKindFixedUpdate(AeRam *r, const AeCameraHooks *h, uint32_t slot); /* 8004875C */
void ae_CameraSlotPlaceBehindPlayer(AeRam *r, const AeCameraHooks *h, uint32_t slot, uint32_t pos, int32_t distance); /* 80048D10 */
void ae_CameraSlotUpdateYaw(AeRam *r, const AeCameraHooks *h, uint32_t slot); /* 80048DD0 */
void ae_CameraSlotGroundClamp(AeRam *r, const AeCameraHooks *h, uint32_t slot); /* 80048F6C */
void ae_CameraSlotGroundClampNear(AeRam *r, const AeCameraHooks *h, uint32_t slot, int32_t near); /* 8004906C */
void ae_CameraSlotStickOrbit(AeRam *r, const AeCameraHooks *h, uint32_t slot); /* 8004923C */
void ae_CameraSlotResetFollow(AeRam *r, const AeCameraHooks *h, uint32_t slot); /* 800493D4 */
void ae_CameraSlotSnapshotEye(AeRam *r, const AeCameraHooks *h); /* 80049514 */
void ae_CameraFollowUpdate(AeRam *r, const AeCameraHooks *h, uint32_t slot, int32_t mode); /* 800495F4 */
void ae_CameraSlotFlagBit81(AeRam *r, const AeCameraHooks *h); /* 8004AA24 */
void ae_CameraFollowAimedVariant(AeRam *r, const AeCameraHooks *h, uint32_t slot); /* 8004AC84 */
void ae_CameraFollowGadgetVariant(AeRam *r, const AeCameraHooks *h, uint32_t slot); /* 8004AF78 */
void ae_CameraKind3FollowUpdate(AeRam *r, const AeCameraHooks *h, uint32_t slot); /* 8004B5FC */
void ae_CameraSlotGetFocusPoint(AeRam *r, const AeCameraHooks *h, uint32_t slot, int32_t mode, uint32_t out); /* 8004B8F4 */
void ae_CameraSlotClearScript(AeRam *r, const AeCameraHooks *h, uint32_t slot); /* 8004C2E4 */
void ae_CameraSlotUpdate(AeRam *r, const AeCameraHooks *h, uint32_t slot); /* 8004CA70 */

#ifdef __cplusplus
}
#endif
#endif
