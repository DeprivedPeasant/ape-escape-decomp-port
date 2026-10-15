#ifndef AE_CAMUPD_H
#define AE_CAMUPD_H

/* Exact RAM-image ports for the per-frame camera update tree. */
#include "ae_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t (*AeCamupdHookFn)(void *user, AeRam *r,
                                  uint32_t a0, uint32_t a1,
                                  uint32_t a2, uint32_t a3);
typedef int32_t (*AeCamupdLookAtFn)(void *user, AeRam *r,
                                   uint32_t eye, uint32_t target,
                                   uint32_t view_matrix, uint32_t rotation,
                                   uint32_t roll);
struct AeStageHooks;

/* Functions outside the requested batch that cross a host or SDK boundary. */
typedef struct AeCamupdHooks {
    void *user;
    AeCamupdHookFn PadGetState;        /* 8002B52C */
    AeCamupdHookFn SpawnParticle;      /* 800698B8 */
    AeCamupdHookFn Printf;             /* 800A3720 */
    AeCamupdHookFn memset;             /* 800A3740 */
    AeCamupdHookFn CameraAreaScriptDispatch; /* 8004B590; area script boundary */
    AeCamupdLookAtFn CameraLookAt;     /* 80046670; renderer / GTE boundary */
    AeCamupdHookFn RenderScreenFlash;  /* native renderer replacement */
    AeCamupdHookFn RenderCameraTransition; /* native renderer replacement */
    AeCameraHooks camera;              /* callbacks used by existing camera ports */
    const struct AeStageHooks *stage;  /* direct calls into the verified stage port */
} AeCamupdHooks;

/* Named RAM fields used by this module. */
#define AE_CAMUPD_G_RUMBLE_ALLOWED 0x800F4488u
#define AE_CAMUPD_G_RUMBLE_STATE   0x800E5468u
#define AE_CAMUPD_G_CUTSCENE_MODE  0x800B01C0u
#define AE_CAMUPD_G_FADE_COUNTER   (AE_G_CAMERA_STATE + 0x3Au)
#define AE_CAMUPD_G_FADE_VALUE     (AE_G_CAMERA_STATE + 0x3Bu)
#define AE_CAMUPD_G_FADE_STEP      (AE_G_CAMERA_STATE + 0x3Cu)
#define AE_CAMUPD_G_FORCE_FOLLOW   (AE_G_CAMERA_STATE + 0x42u)
#define AE_CAMUPD_G_AIM_INDEX      (AE_G_CAMERA_STATE + 0x19u)
#define AE_CAMUPD_G_FIXED_INDEX    (AE_G_CAMERA_STATE + 0x1Au)
#define AE_CAMUPD_G_ACTIVE_KIND    (AE_G_CAMERA_STATE + 0x0Fu)
#define AE_CAMUPD_G_BLEND_STYLE    (AE_G_CAMERA_STATE + 0x2Eu)
#define AE_CAMUPD_G_BLEND_TIMER    (AE_G_CAMERA_STATE + 0x30u)
#define AE_CAMUPD_G_SHAKE           0x800E6950u
#define AE_CAMUPD_G_REGION_COUNT    0x800E68D0u
#define AE_CAMUPD_G_REGION_TABLE    0x800E691Cu
#define AE_CAMUPD_G_REGION_HALF_EXTENTS (AE_G_CAMERA_FOCUS + 0x08u)
#define AE_CAMUPD_G_REGION_STATE_A  (AE_G_CAMERA_STATE + 0x3Fu)
#define AE_CAMUPD_G_REGION_STATE_B  (AE_G_CAMERA_STATE + 0x40u)
#define AE_CAMUPD_G_REGION_STATE_C  (AE_G_CAMERA_STATE + 0x42u)
#define AE_CAMUPD_G_EVENT_RECORDS   (AE_CAMUPD_G_REGION_COUNT + 0x60u)
#define AE_CAMUPD_G_EVENT_RECORD_COUNT (AE_CAMUPD_G_REGION_COUNT + 0x68u)
#define AE_CAMUPD_G_EVENT_RECORDS_ALT (AE_CAMUPD_G_REGION_COUNT + 0x64u)
#define AE_CAMUPD_G_EVENT_FLAGS_A 0x800E5478u
#define AE_CAMUPD_G_EVENT_FLAGS_B 0x800E5480u
#define AE_CAMUPD_G_EVENT_ACTIVE    (AE_CAMUPD_G_REGION_COUNT + 0x46u)
#define AE_CAMUPD_G_EVENT_RUNNING   (AE_CAMUPD_G_EVENT_CAMERA + 0x24u)
#define AE_CAMUPD_G_EVENT_RECORD_INDEX (AE_CAMUPD_G_EVENT_CAMERA + 0x25u)
#define AE_CAMUPD_G_EVENT_RECORD_SLOT  (AE_CAMUPD_G_EVENT_CAMERA + 0x26u)
#define AE_CAMUPD_G_EVENT_KIND (AE_CAMUPD_G_EVENT_CAMERA + AE_CAMUPD_EVENT_RECORD_KIND)
#define AE_CAMUPD_G_EVENT_SLOT      (AE_CAMUPD_G_EVENT_CAMERA + 0x26u)
#define AE_CAMUPD_G_EVENT_DONE      (AE_CAMUPD_G_EVENT_CAMERA + 0x29u)
#define AE_CAMUPD_EVENT_RECORD_SIZE 0x20u
#define AE_CAMUPD_EVENT_START_ERROR_FORMAT 0x800A4DB0u
#define AE_CAMUPD_G_STAGE_STATE     0x800F4350u
#define AE_CAMUPD_STAGE_CURRENT_AREA 0x126u
#define AE_CAMUPD_STAGE_ENTRY_INDEX 0x12Au
#define AE_CAMUPD_EVENT_CREATE_EYE_TEMP 0x801FFB10u
#define AE_CAMUPD_EVENT_ACTIVE_SLOT (AE_G_CAMERA_STATE + 0x1Bu)
#define AE_CAMUPD_EVENT_TRIGGER_FLAG (AE_G_CAMERA_STATE + 0x41u)
#define AE_CAMUPD_G_EVENT_TRIGGER_FLAG 0x800AF131u
#define AE_CAMUPD_G_EVENT_TRIGGER_PARTICLE 0x800B4D88u
#define AE_CAMUPD_G_CAMERA_EXIT_CLEAR 0x800F4450u
#define AE_CAMUPD_G_CAMERA_EXIT_BUFFER 0x800F4350u
#define AE_CAMUPD_G_TRANSITION_COUNTER 0x800AF130u
#define AE_CAMUPD_G_DISPLAY_STATE 0x800EC350u
#define AE_CAMUPD_G_ORDER_TABLE_CURSOR (AE_CAMUPD_G_DISPLAY_STATE + 0x04u)
#define AE_CAMUPD_G_DISPLAY_INDEX (AE_CAMUPD_G_DISPLAY_STATE + 0x0Cu)
#define AE_CAMUPD_G_SCRIPT_EYE_HEIGHT (AE_G_CAMERA_STATE + 0x34u)
#define AE_CAMUPD_G_SCRIPT_FOLLOW_DISTANCE (AE_G_CAMERA_STATE + 0x36u)
#define AE_CAMUPD_G_EYE_BLEND_X (AE_G_CAMERA_STATE + 0x10u)
#define AE_CAMUPD_G_EYE_BLEND_Y (AE_G_CAMERA_STATE + 0x12u)
#define AE_CAMUPD_G_EYE_BLEND_Z (AE_G_CAMERA_STATE + 0x14u)
#define AE_CAMUPD_G_SCRIPT_EYE_HEIGHT_ACTIVE (AE_G_CAMERA_STATE + 0x3Fu)
#define AE_CAMUPD_G_SCRIPT_DISTANCE_ACTIVE (AE_G_CAMERA_STATE + 0x40u)
#define AE_CAMUPD_G_SCRIPT_DIRECTION (AE_CAM_SCRIPT_STATE + 0x01u)
#define AE_CAMUPD_G_SCRIPT_FOCUS_MODE (AE_CAM_SCRIPT_STATE + 0x02u)
#define AE_CAMUPD_SCRIPT_VECTOR_TEMP 0x801FFE00u
#define AE_CAMUPD_SCRIPT_DIRECTION_TEMP (AE_CAMUPD_SCRIPT_VECTOR_TEMP + 0x10u)
#define AE_CAMUPD_SCRIPT_FOCUS_TEMP (AE_CAMUPD_SCRIPT_VECTOR_TEMP + 0x20u)
#define AE_CAMUPD_SCRIPT_VECTOR_TEMP2 (AE_CAMUPD_SCRIPT_VECTOR_TEMP + 0x30u)
#define AE_CAMUPD_SCRIPT_ERROR_FORMAT 0x800A4D04u
#define AE_CAMUPD_SCRIPT_MAX_OPCODE 26u
#define AE_CAMUPD_G_CAMERA_ROTATION 0x800EC330u
#define AE_CAMUPD_CAMERA_AIM_TEMP_ABI 0x801FFEA8u
#define AE_CAMUPD_DISPLAY_RECORD_STRIDE 0x4078u
#define AE_CAMUPD_DISPLAY_RECORD_MODE 0x12u
#define AE_CAMUPD_DISPLAY_RECORD_IMAGE 0x80u
#define AE_CAMUPD_TRANSITION_OT_ADVANCE 0x68u
#define AE_CAMUPD_EXIT_CAMERA_EYE 0x258u
#define AE_CAMUPD_EXIT_CAMERA_TARGET 0x260u
#define AE_CAMUPD_EXIT_TEMP_POS 0x801FFEE8u
#define AE_CAMUPD_EXIT_TEMP_TARGET 0x801FFEE0u
#define AE_CAMUPD_EXIT_TEMP_MEMSET (AE_CAMUPD_EXIT_TEMP_POS + 0x08u)
#define AE_CAMUPD_PLAYER_POSITION_FIELD_SIZE 0x04u
#define AE_CAMUPD_EXIT_SLOT_DISTANCE 0x333
#define AE_CAMUPD_G_EVENT_CAMERA    0x800B8CC0u
#define AE_CAMUPD_EVENT_FRAME_COUNT 0x20u
#define AE_CAMUPD_EVENT_STATE       0x22u
#define AE_CAMUPD_EVENT_RECORD_KIND 0x28u
#define AE_CAMUPD_EVENT_START_EYE   0x00u
#define AE_CAMUPD_EVENT_END_EYE     0x08u
#define AE_CAMUPD_EVENT_START_ROT   0x10u
#define AE_CAMUPD_EVENT_ROT_DELTA   0x18u
#define AE_CAMUPD_EVENT_RECORD_TIME 0x0Eu
#define AE_CAMUPD_EVENT_RECORD_EYE  0x00u
#define AE_CAMUPD_EVENT_RECORD_END  0x08u
#define AE_CAMUPD_EVENT_RECORD_START_ROT 0x10u
#define AE_CAMUPD_EVENT_RECORD_END_ROT   0x18u
#define AE_CAMUPD_EVENT_RECORD_STATE 0x16u
#define AE_CAMUPD_ROTATION_TEMP     0x801FFEA0u
#define AE_CAMUPD_ROTATION_MATRIX_SIZE 0x18u
#define AE_CAMUPD_ROTATION_FIELDS   3u
#define AE_CAMUPD_ROTATION_FIELD_SIZE 2u
#define AE_CAMUPD_MATRIX_FIRST_ROW  0x04u
#define AE_CAMUPD_MATRIX_ROW_SIZE   0x06u
#define AE_CAMUPD_REGION_SIZE       0x30u
#define AE_CAMUPD_REGION_FLAGS      0x02u
#define AE_CAMUPD_REGION_SCRIPT_INDEX 0x00u
#define AE_CAMUPD_REGION_CENTER_X   0x04u
#define AE_CAMUPD_REGION_CENTER_Y   0x06u
#define AE_CAMUPD_REGION_CENTER_Z   0x08u
#define AE_CAMUPD_REGION_EXTENT_X   0x0Au
#define AE_CAMUPD_REGION_EXTENT_Y   0x0Cu
#define AE_CAMUPD_REGION_EXTENT_Z   0x0Eu
#define AE_CAMUPD_REGION_TEMP_PLAYER 0x801FFEE8u
#define AE_CAMUPD_REGION_TEST_POSITION 0x801FFEE0u
#define AE_CAMUPD_REGION_SLOT_MASK  0x000Fu
#define AE_CAMUPD_REGION_KIND_MASK  0x6000u
#define AE_CAMUPD_REGION_KIND_EVENT 0x4000u
#define AE_CAMUPD_REGION_ACTIVE_MASK 0x8000u
#define AE_CAMUPD_REGION_REMOVE_MASK 0x6000u
#define AE_CAMUPD_SHAKE_X           0x00u
#define AE_CAMUPD_SHAKE_Y           0x02u
#define AE_CAMUPD_SHAKE_Z           0x04u
#define AE_CAMUPD_SHAKE_COUNTER     0x08u
#define AE_CAMUPD_G_BLEND_EYE      (AE_G_CAMERA_STATE + 0x1Cu)
#define AE_CAMUPD_G_BLEND_TARGET   (AE_G_CAMERA_STATE + 0x24u)
#define AE_CAMUPD_COPY_SVEC_OFFSET 0x1Cu
#define AE_CAMUPD_PLAYER_LINEAR_VELOCITY 0x30u
#define AE_CAMUPD_PLAYER_CAM_FOCUS       0x12Cu
#define AE_CAMUPD_VEC_Y                  0x02u
#define AE_CAMUPD_VEC_Z                  0x04u
#define AE_CAMUPD_VEC_W                  0x06u
#define AE_CAMUPD_VEC_SIZE               0x08u
#define AE_CAMUPD_VEC_COMPONENT_SIZE     0x02u
#define AE_CAMUPD_VEC_COMPONENT_COUNT    3u
#define AE_CAMUPD_BLEND_TIME_SCALE       0x80u
#define AE_CAMUPD_BLEND_STYLE1_FAST_END  32u
#define AE_CAMUPD_BLEND_STYLE2_FAST_END  48u
#define AE_CAMUPD_FLIGHT_AREA_FIRST      0x48u
#define AE_CAMUPD_FLIGHT_AREA_COUNT      5u
#define AE_CAMUPD_FLIGHT_PLAYER_MODE     2u
#define AE_CAMUPD_VELOCITY_Y             0x04u
#define AE_CAMUPD_VELOCITY_Z             0x08u
#define AE_CAMUPD_PLAYER_DIR       (AE_G_PLAYER + AE_PLAYER_FACING)
#define AE_CAMUPD_SCR_PLAYER_POS   (AE_SCRATCH_BASE + 0x10u)
#define AE_CAMUPD_SCR_PLAYER_DIR   (AE_SCRATCH_BASE + 0x40u)
#define AE_CAMUPD_SCR_PLAYER_X     (AE_SCRATCH_BASE + 0x18u)
#define AE_CAMUPD_SCR_PLAYER_Z     (AE_SCRATCH_BASE + 0x1Cu)
#define AE_CAMUPD_SCR_CAMERA_FOCUS (AE_SCRATCH_BASE + 0x2Cu)
#define AE_CAMUPD_SCR_CAMERA_VELOCITY (AE_SCRATCH_BASE + 0x68u)
#define AE_CAMUPD_CAMERA_SNAPSHOT_POS  (AE_G_CAMERA_STATE + 0x6Cu)
#define AE_CAMUPD_CAMERA_SNAPSHOT_DIR  (AE_G_CAMERA_STATE + 0x74u)

void ae_PadRumble(AeRam *r, const AeCamupdHooks *h,
                  int32_t port, uint32_t small_motor, uint32_t large_motor); /* 80014168 */
void ae_CopySvecAt1C(AeRam *r, uint32_t actor, uint32_t destination); /* 800154DC */
int32_t ae_Atan2Safe(AeRam *r, int32_t x, int32_t z); /* 80046340 */
void ae_CameraGetEye(AeRam *r, uint32_t destination); /* 800470CC */
int32_t ae_IsFlightArea(AeRam *r); /* 8004A940 */
void ae_CameraGetPlayerDir(AeRam *r, uint32_t slot, uint32_t mode,
                           uint32_t destination); /* 8004B880 */
void ae_CameraSnapshotPlayer(AeRam *r); /* 80047DA4 */
void ae_CameraSelectMode(AeRam *r, const AeCamupdHooks *h); /* 80046DE8 */
void ae_CameraApplySlot(AeRam *r); /* 80047FF0 */
void ae_CameraShakeApply(AeRam *r); /* 80048BBC */
int32_t ae_CameraRegionTest(AeRam *r, uint32_t position); /* 8004BD98 */
void ae_EventCamLoadRecord(AeRam *r, uint32_t record); /* 8004D134 */
void ae_EventCamStart(AeRam *r, const AeCamupdHooks *h,
                      uint32_t record_index); /* 8004D39C */
int32_t ae_EventCamCreate(AeRam *r, const AeCamupdHooks *h, int32_t script_index,
                          uint32_t activate, uint32_t script_set); /* 8004BB68 */
void ae_EventCamTrigger(AeRam *r, const AeCamupdHooks *h,
                        uint32_t trigger); /* 8004BA0C */
void ae_StageExitRequest(AeRam *r, const AeCamupdHooks *h); /* 8004D6F4 */
void ae_EventCamUpdate(AeRam *r, const AeCamupdHooks *h); /* 8004D7A0 */
void ae_StageExitSequenceUpdate(AeRam *r,
                                const AeCamupdHooks *h); /* 8004D40C */
void ae_CameraTransitionFrameDraw(AeRam *r,
                                  const AeCamupdHooks *h); /* 8004CEB4 */
void ae_CameraRegionUpdate(AeRam *r,
                           const AeCamupdHooks *h); /* 8004BF20 */
void ae_CameraScriptRun(AeRam *r, const AeCamupdHooks *h,
                        uint32_t slot); /* 8004C318 */
void ae_CameraUpdate(AeRam *r, const AeCamupdHooks *h); /* 80047BD0 */
void ae_ScreenFlashDraw(AeRam *r, const AeCamupdHooks *h,
                        uint32_t intensity); /* 80046990 */
void ae_ScreenFadeUpdate(AeRam *r, const AeCamupdHooks *h); /* 80046A78 */
void ae_CutsceneModeOn(AeRam *r); /* 8005DD18 */
void ae_CutsceneModeOff(AeRam *r); /* 8005DD28 */

#ifdef __cplusplus
}
#endif
#endif
