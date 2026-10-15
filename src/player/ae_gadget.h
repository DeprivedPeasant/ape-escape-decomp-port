#ifndef AE_GADGET_H
#define AE_GADGET_H
/*
 * Readable RAM-image ports of the original Gadget* functions.
 * Function addresses are kept beside the entry points; game pointers and fields stay at their
 * original PS1 RAM addresses so the MIPS checker can compare the whole image after each call.
 */
#include "../core/ae_ram.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Player and gadget globals. */
#define AE_G_GADGET_TRAIL4       0x800F4ED0u /* 40 records, 8 bytes each */
#define AE_G_GADGET_WORK         0x800F5120u /* per-gadget state words */
#define AE_G_GADGET4_MODE        0x800F5124u
#define AE_G_GADGET_POWER        0x800F5128u
#define AE_G_GADGET_LOOP_VOICES  0x800F5134u /* two s16 voice handles */
#define AE_G_GADGET_SWING        0x800F5148u /* s16 angle, acceleration, speed */
#define AE_G_GADGET_SELECTED     0x800F51C3u
#define AE_G_GADGET_UNLOCK_MASK  0x800F51C4u
#define AE_G_GADGET_SLOTS        0x800F51A8u /* four s8 gadget ids */
#define AE_G_GADGET_FLASH_TIMER  0x800BAD80u
#define AE_G_GADGET_FLASH_POS    0x800BAD84u /* three s16 coordinates */
#define AE_G_GADGET_SELECT_FX_ORIGIN 0x800B4D88u /* three s16 base coordinates */
#define AE_G_GADGET_SELECT_FX_GUARD_A 0x800E6902u
#define AE_G_GADGET_SELECT_FX_GUARD_B 0x800E6916u
#define AE_G_GADGET6_SPARKLE_SEED 0x800AE8F0u
#define AE_G_GADGET6_SPARKLE_GATE 0x800EC350u
#define AE_G_GADGET3_AIM_MODE    0x800E6916u
#define AE_G_GADGET3_AIM_ORIGIN 0x800EC204u
#define AE_G_GADGET3_PROJECTILE_ORIGIN (AE_G_PLAYER + AE_PLAYER_POS - 8u)
#define AE_G_GADGET3_AIM_LINE_START 0x800F45A8u
#define AE_G_GADGET3_AIM_LINE_END   0x800F45B0u
#define AE_G_GAMEMODE 0x800F4470u
#define AE_G_CURRENT_LEVEL 0x800F4474u
#define AE_G_GADGET3_AMMO_KIND 0x800F51C0u
#define AE_G_GADGET3_AMMO_1 0x800F51C1u
#define AE_G_GADGET3_AMMO_2 0x800F51C2u
#define AE_G_CAMERA_LOOK_MATRIX 0x800E78C0u
#define AE_G_GADGET3_LOCK_LIST  0x800F5090u /* three {target*, timer} records */
#define AE_G_STICK_GESTURE_EVENTS 0x800E549Eu
#define AE_G_PLAYER              0x800EC1E0u
#define AE_G_PLAYER_STATIC_MODELS 0x800EC060u /* 19 instances, 0x10 bytes each */
#define AE_G_GADGET_MODEL_MATRIX_BASE 0x800E9C60u
#define AE_G_GADGET_MODEL_ROOT_MATRIX 0x800E7D00u
#define AE_G_GADGET_MODEL_POSITION_BASE 0x800F4730u
#define AE_G_GADGET_MODEL_MATRIX_STRIDE 0x20u
#define AE_G_GADGET_MODEL_DRAW_INDEX 0x800EC35Cu
#define AE_G_GADGET_MODEL_DRAW_BASE 0x800EC3D0u
#define AE_G_GADGET4_MODEL_BASE 0x800EC0A0u
#define AE_G_GADGET4_ALT_MODEL_BASE 0x800EC120u
#define AE_G_GADGET4_HOOP_MATRIX_BASE 0x800F4730u
#define AE_G_GADGET4_DRAW_PHASE 0x800EC350u
#define AE_G_GADGET4_MATRIX_TEMP 0x1F800028u
#define AE_G_GADGET4_VECTOR_TEMP 0x1F800018u
#define AE_G_GADGET4_SCRATCH_SPEED_X 0x1F80003Cu
#define AE_G_GADGET4_SCRATCH_SPEED_Z 0x1F800044u
#define AE_G_GADGET_ATTACHMENT_LOCAL_MATRIX AE_G_GADGET_TEMP_VECTOR
#define AE_G_GADGET_ATTACHMENT_COMP_MATRIX 0x800F5124u
#define AE_G_GADGET_ATTACHMENT_MODEL_SLOT0 0x800EC060u
#define AE_G_GADGET_ATTACHMENT_MODEL_SLOT9 0x800EC0F0u
#define AE_G_GADGET_ATTACHMENT_START_MATRIX 0x800E9E40u
#define AE_G_GADGET_ATTACHMENT_OUTPUT_MATRIX 0x800F4910u
#define AE_G_GADGET_ATTACHMENT_MODEL_VECTOR 0x9Cu
#define AE_G_GADGET_ATTACHMENT_VECTOR_X 64u
#define AE_G_GADGET_ATTACHMENT_VECTOR_Z 72u
#define AE_G_GADGET_ATTACHMENT_POS_A 0x34u
#define AE_G_GADGET_ATTACHMENT_POS_B 0x3Cu
#define AE_G_GADGET_ATTACHMENT_RIBBON_COLOR 0x00FF8080u
#define AE_G_GADGET6_MODEL_BASE_EARLY 0x800EC0C0u
#define AE_G_GADGET6_MODEL_BASE_LATE 0x800EC180u
#define AE_G_GADGET6_MODEL_BONE_MATRIX 0x800E9E40u
#define AE_G_GADGET6_LOOP_VOICE_1 (AE_G_GADGET_WORK + 4u)
#define AE_G_GADGET6_LOOP_VOICE_2 (AE_G_GADGET_WORK + 6u)
#define AE_G_GADGET6_SOUND_STATE  (AE_G_GADGET_WORK + 8u)
#define AE_G_GADGET_STAGE_FLAGS 0x800F4450u
#define AE_G_GADGET_SWING_TIMER (AE_G_GADGET_WORK + 0x44u)
#define AE_G_GADGET_SWING_VOICE (AE_G_GADGET_WORK + 0x48u)
#define AE_G_STICK_SPIN 0x800E54D4u
#define AE_G_PLAYER_CURRENT_GADGET (AE_G_PLAYER + 0xF2u)
#define AE_G_GADGET_TEMP_VECTOR  0x801FFEF0u /* adapter buffer in the excluded caller stack window */
#define AE_G_GADGET_RANDOM_VECTOR (AE_G_GADGET_TEMP_VECTOR + 16u)
/* FlashUpdate keeps its position live across EffectSpawnFromDef/MatrixLookDir.
 * MatrixLookDir's normalized local occupies TEMP_VECTOR; these caller locals
 * must remain separate, just as they do in the original nested stack frames. */
#define AE_G_GADGET_FLASH_LOCAL_POSITION 0x801FFDB0u
#define AE_G_GADGET_SELECT_FX    0x800F51D0u /* selection effect animation/state record */
#define AE_G_GADGET_SELECT_FX_MODEL 0x800EC130u
#define AE_G_GADGET_SELECT_FX_OT 0x800EC358u
#define AE_G_GADGET_SELECT_FX_AREA_STATE (AE_G_GADGET7_STAGE_GATE + 2u)
#define AE_G_GADGET3_SELECT_FLAGS 0x800E5478u
#define AE_G_GADGET3_SELECT_MASK  0x800E5480u
#define AE_G_GADGET3_MODEL_INSTANCE 0x800EC090u
#define AE_G_GADGET_STACK_MATRIX_A 0x801FFEB0u
#define AE_G_GADGET_STACK_MATRIX_B 0x801FFE90u
#define AE_G_GADGET_STACK_SCALE    0x801FFED0u
#define AE_G_GADGET_SELECT_FX_TIMER (AE_G_GADGET_SELECT_FX + 16u)
#define AE_G_GADGET_SELECT_FX_PRIM_CURSOR 0x800EC358u
#define AE_G_GADGET7_STATE       0x800BAD90u /* RC-car trail counter and phase */
#define AE_G_GADGET7_CAR         0x800F5A20u
#define AE_G_GADGET7_RIBBON_COLORS 0x800B1BA0u
#define AE_G_GADGET_DRAW_PRIM_CURSOR 0x800EC354u
#define AE_G_GADGET7_STAGE_GATE  0x800F44A0u
#define AE_G_GADGET4_LEVEL_DATA  0x800E5320u
#define AE_G_GADGET4_AREA_DISPATCH 0x800A5EF8u
#define AE_G_GADGET4_MATRIX_SOURCE 0x800F4750u
#define AE_G_GADGET4_AREA_MODE    0x800EC250u
#define AE_G_GADGET4_CONTACT_SOUND 28u
#define AE_G_GADGET4_VOICE_LEFT   0x20u
#define AE_G_GADGET4_VOICE_RIGHT  0x22u
#define AE_G_GADGET4_BODY_LEFT    0x14u
#define AE_G_GADGET4_BODY_RIGHT   0x18u
#define AE_G_GADGET4_BODY_RADIUS  0x20u
#define AE_G_GADGET4_BODY_HEIGHT  0x24u
#define AE_G_GADGET4_BODY_FLAGS   0x26u
#define AE_G_GADGET4_LEVEL_FRAME  0x182u
#define AE_G_GADGET4_LEVEL_END_FRAME 0x184u
#define AE_G_GADGET4_GLOBAL_END_FRAME 0x800E54A2u
#define AE_G_GADGET4_LEVEL_ENABLE 0x17Au
#define AE_G_GADGET4_LEVEL_WINDOW 0x1ACu
#define AE_G_GADGET4_LEVEL_SPEED_X 0x19Cu
#define AE_G_GADGET4_LEVEL_SPEED_Z 0x1A0u
#define AE_G_GADGET4_LEVEL_TURN_RATE_X 0x1B4u
#define AE_G_GADGET4_LEVEL_TURN_RATE_Z 0x1BCu
#define AE_G_GADGET4_SAVED_STATE 0x800F4E34u
#define AE_G_GADGET4_GESTURE_STATE 0x800E549Au
#define AE_G_GADGET4_SCRATCH_ZOOM (AE_G_GADGET_SCRATCH + 0x66u)
#define AE_G_GADGET4_SCRATCH_SAVED_STATE (AE_G_GADGET_SCRATCH + 0x48u)
#define AE_G_GADGET4_SCRATCH_RATE_X (AE_G_GADGET_SCRATCH + 0x78u)
#define AE_G_GADGET4_SCRATCH_RATE_Z (AE_G_GADGET_SCRATCH + 0x7Cu)
#define AE_G_GADGET4_SCRATCH_MATRIX (AE_G_GADGET_SCRATCH + 0x28u)
#define AE_G_GADGET4_TRAIL_BODY_A 0x0Cu
#define AE_G_GADGET4_TRAIL_BODY_B 0x10u
#define AE_G_GADGET4_TRAIL_BODY_C 0x14u
#define AE_G_GADGET4_WORK_SPEED_X 0x18u
#define AE_G_GADGET4_WORK_SPEED_Z 0x1Cu
#define AE_G_GADGET4_WORK_TARGET_SCALE 0x04u
#define AE_G_GADGET4_WORK_SCALE 0x06u
#define AE_G_GADGET4_WORK_MODE_STRENGTH 0x08u
#define AE_G_GADGET4_WORK_HIT_X 0x0Cu
#define AE_G_GADGET4_WORK_HIT_Y 0x0Eu
#define AE_G_GADGET4_WORK_HIT_Z 0x10u
#define AE_G_GADGET4_WORK_CLEAR_FIELD 0x1Au
#define AE_G_GADGET4_TRAIL_OT_TABLE (AE_G_GADGET_WORK + 0x80u)
#define AE_G_GADGET_TRAIL_SEED   0x800AE8F0u
#define AE_G_GADGET_SCRATCH      0x1F800000u
#define AE_G_GADGET3_AIM_CACHE   (AE_G_GADGET_WORK + 0x24u)
#define AE_G_FX_GADGET_FLASH_1   0x800B1A8Cu
#define AE_G_FX_GADGET_FLASH_2   0x800B1AE0u
#define AE_G_FX_GADGET_FLASH_3   0x800B1A38u
#define AE_G_FX_GADGET_FLASH_4   0x800B193Cu
#define AE_G_FX_GADGET_FLASH_5   0x800B1990u

#define AE_GADGET_POS_X          0x00u
#define AE_GADGET_POS_Y          0x04u
#define AE_GADGET_POS_Z          0x08u
#define AE_GADGET_FLASH_Y        0x02u
#define AE_GADGET_FLASH_Z        0x04u
#define AE_GADGET_SWING_ACCEL    0x02u
#define AE_GADGET_SWING_SPEED    0x04u
#define AE_GADGET4_ORIGIN_X      0x18u
#define AE_GADGET4_ORIGIN_Z      0x1Cu
#define AE_MATRIX_TRANSLATION_X  0x14u
#define AE_MATRIX_TRANSLATION_Y  0x18u
#define AE_MATRIX_TRANSLATION_Z  0x1Cu
#define AE_GADGET3_LOCK_COUNT    3u
#define AE_GADGET3_LOCK_STRIDE   8u
#define AE_GADGET3_LOCK_TARGET   0x00u
#define AE_GADGET3_LOCK_TIMER    0x04u
#define AE_GADGET_SELECT_FX_POS_X 0x00u
#define AE_GADGET_SELECT_FX_POS_Y 0x02u
#define AE_GADGET_SELECT_FX_POS_Z 0x04u
#define AE_GADGET_SELECT_FX_TURN 0x06u
#define AE_GADGET_SELECT_FX_DIR_X 0x08u
#define AE_GADGET_SELECT_FX_DIR_Y 0x0Au
#define AE_GADGET_SELECT_FX_DIR_Z 0x0Cu
#define AE_COLLISION_BODY_POS_X   0x00u
#define AE_COLLISION_BODY_POS_Y   0x04u
#define AE_COLLISION_BODY_POS_Z   0x08u
#define AE_COLLISION_BODY_FACING  0x18u
#define AE_COLLISION_BODY_FACING_X 0x18u
#define AE_COLLISION_BODY_FACING_Y 0x1Au
#define AE_COLLISION_BODY_FACING_Z 0x1Cu
#define AE_COLLISION_BODY_RADIUS  0x20u
#define AE_COLLISION_BODY_HEIGHT  0x22u
#define AE_COLLISION_BODY_FLAGS   0x26u
#define AE_COLLISION_BODY_CONTACT 0x2Cu

/* Player field offsets from struct Player (tools/structs.txt). */
#define AE_PLAYER_MODEL          0x00u
#define AE_PLAYER_BONE_MATRICES  0x14u
#define AE_PLAYER_POS            0x24u
#define AE_PLAYER_VEL            0x30u
#define AE_PLAYER_FACING         0x60u
#define AE_PLAYER_ANIM_FRAME     0x5Cu
#define AE_PLAYER_STATE          0x5Eu
#define AE_PLAYER_POS_SHORTS     0x1Cu
#define AE_PLAYER_CURRENT_GADGET_FIELD 0xF2u
#define AE_PLAYER_MOVE_VEL       0x120u
#define AE_PLAYER_MOVE_VEL_Y     0x124u
#define AE_PLAYER_HEALTH         0xE8u
#define AE_PLAYER_ANIM_BLEND     0x8Cu
#define AE_PLAYER_FACING_X       0x00u
#define AE_PLAYER_FACING_Z       0x04u
#define AE_PLAYER_VEL_X          0x00u
#define AE_PLAYER_VEL_Y          0x04u
#define AE_PLAYER_VEL_Z          0x08u

/* Player static-model entry fields from the 0x10-byte ModelInstance record. */
#define AE_STATIC_MODEL_MODEL   0x00u
#define AE_STATIC_MODEL_BONES   0x04u
#define AE_STATIC_MODEL_ANIM    0x08u
#define AE_STATIC_MODEL_FLAGS   0x0Cu
#define AE_STATIC_MODEL_SIZE    0x10u

/* Gadget 4 trail fields. */
#define AE_GADGET4_TRAIL_COUNT  40u
#define AE_GADGET4_TRAIL_STRIDE 8u
#define AE_GADGET4_TRAIL_STATE  6u
#define AE_GADGET4_TRAIL_STATE_MASK 0x001Fu
#define AE_GADGET4_TRAIL_X      0x00u
#define AE_GADGET4_TRAIL_Y      0x02u
#define AE_GADGET4_TRAIL_Z      0x04u
#define AE_GADGET7_COUNTER      0x09u
#define AE_GADGET7_ANGLE_X      0x00u
#define AE_GADGET7_ANGLE_Y      0x02u
#define AE_GADGET7_ANGLE_Z      0x04u
#define AE_GADGET7_POS_X        0x24u
#define AE_GADGET7_POS_Y        0x28u
#define AE_GADGET7_POS_Z        0x2Cu
#define AE_GADGET4_TRAIL_SPEED_X 0x30u
#define AE_GADGET4_TRAIL_SPEED_Y 0x34u
#define AE_GADGET4_TRAIL_SPEED_Z 0x38u
#define AE_GADGET4_TRAIL_ORIGIN_X 0x24u
#define AE_GADGET4_TRAIL_ORIGIN_Y 0x28u
#define AE_GADGET4_TRAIL_ORIGIN_Z 0x2Cu
#define AE_GADGET4_TRAIL_MODE    0x04u
#define AE_GADGET4_TRAIL_RATE    0x08u
#define AE_GADGET4_TRAIL_INDEX   0x0Cu
#define AE_GADGET4_TRAIL_RATE_X  0x18u
#define AE_GADGET4_TRAIL_RATE_Z  0x1Cu
#define AE_GADGET4_TRAIL_RATE_Y  0x1Eu
#define AE_GADGET4_TRAIL_LOOP_VOICE 0x20u
#define AE_GADGET4_TRAIL_ALT_VOICE 0x22u
#define AE_GADGET4_TRAIL_ACCEL   0x24u
#define AE_GADGET4_TRAIL_PHASE   0x28u
#define AE_GADGET4_TRAIL_YAW     0x2Cu

/* Scratchpad values used while Super Hoop trails are updated. */
#define AE_GADGET_SCRATCH_SEED       (AE_G_GADGET_SCRATCH + 0x00u)
#define AE_GADGET_SCRATCH_PROJECT_X  (AE_G_GADGET_SCRATCH + 0x04u)
#define AE_GADGET_SCRATCH_PROJECT_Y  (AE_G_GADGET_SCRATCH + 0x08u)
#define AE_GADGET_SCRATCH_PROJECT_Z  (AE_G_GADGET_SCRATCH + 0x0Cu)
#define AE_GADGET_SCRATCH_SPEED_X    (AE_G_GADGET_SCRATCH + 0x10u)
#define AE_GADGET_SCRATCH_SPEED_Y    (AE_G_GADGET_SCRATCH + 0x14u)
#define AE_GADGET_SCRATCH_SPEED_Z    (AE_G_GADGET_SCRATCH + 0x18u)
#define AE_GADGET_SCRATCH_POS_SHORTS (AE_G_GADGET_SCRATCH + 0x1Cu)
#define AE_GADGET_SCRATCH_TRAIL_EMIT (AE_G_GADGET_SCRATCH + 0x24u)
#define AE_GADGET_SCRATCH_ANGLE      (AE_G_GADGET_SCRATCH + 0x28u)

/* The common adapter uses one named callback per original function that remains outside this
 * batch and has no verified RAM-image port.  The first four words are the original MIPS argument
 * registers; the checker records only the meaningful argument count for each function. */
typedef int32_t (*AeGadgetHookFn)(void *user, AeRam *r,
                                  uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3,
                                  uint32_t a4);

typedef struct AeGadgetHooks {
    void *user;
    uint32_t stale_s1; /* declared caller value for Gadget4BuildMatrix's non-0/1 mode path */
    AeGadgetHookFn PlayerCanExitStage;       /* 8005DC78 */
    AeGadgetHookFn IsPlayerSwimming;         /* 80068B7C */
    AeGadgetHookFn RibbonDraw;               /* 8006F3D0 */
    AeGadgetHookFn EffectSpawnFromDef;       /* 80070284 */
    AeGadgetHookFn SpawnParticle;            /* 800698B8 */
    AeGadgetHookFn TrailSpawn;               /* 8006ED78 */
    AeGadgetHookFn PlaySfx;                  /* 800877F8 */
    AeGadgetHookFn CameraGetSlotTarget;      /* 8004B54C */
    AeGadgetHookFn TargetListFindBest;       /* 8007CDFC */
    AeGadgetHookFn SdkVectorHelper_80031330; /* 80031330 */
    AeGadgetHookFn AimReticleDrawCircle;     /* 80085E08 */
    AeGadgetHookFn PlayerSetState;           /* 80064208 */
    AeGadgetHookFn PadRumble;                /* 80014168 */
    AeGadgetHookFn DrawScaledModelAt;        /* 8007CAE0 */
    AeGadgetHookFn SdkVectorHelper_80030A30; /* 80030A30 */
    AeGadgetHookFn SdkVectorHelper_80031830; /* 80031830 */
    AeGadgetHookFn ModelTransformRigid;      /* 800161C0 */
    AeGadgetHookFn SetColorMatrixMode;       /* 80015D58 */
    AeGadgetHookFn ModelDraw;                /* 80017CD4 */
    AeGadgetHookFn PlayerCanActivateGadget;   /* 8007CA48 */
    AeGadgetHookFn SoundLoopStart;           /* 800878E0 */
    AeGadgetHookFn SoundLoopStop;            /* 8008806C */
    AeGadgetHookFn SoundEmitterVolume;       /* 80087BA4 */
    AeGadgetHookFn MatrixRotYMul;             /* 80014750 */
    AeGadgetHookFn SdkVectorHelper_800305B0; /* 800305B0 */
    AeGadgetHookFn PlayerFxSetMatrixPos;     /* 8006CFBC */
    AeGadgetHookFn RadarDrawModel;            /* 8009A574 */
    AeGadgetHookFn SdkVectorHelper_8002FB00; /* 8002FB00 */
    AeGadgetHookFn SdkVectorHelper_8002FB30; /* 8002FB30 */
    AeGadgetHookFn SdkVectorHelper_80031230; /* 80031230 */
    AeGadgetHookFn TrailRingPush;            /* 8001F8B0 */
    AeGadgetHookFn TrailRingDraw;            /* 8001F954 */
    AeGadgetHookFn DrawRibbonBetween;        /* 8002020C */
    AeGadgetHookFn ProjectileGroupSpawnSpread; /* 80085630 */
    AeGadgetHookFn PlayerSelectGadget;       /* 8007AC64 */
    AeGadgetHookFn AimTableClear3;           /* 8007A304 */
    AeGadgetHookFn GetSolidHeight;           /* 8004D8C0: nested by verified GetGroundHeight */
    AeGadgetHookFn PlaySfxAt;                /* 80087A20: nested by verified world/body ports */
    AeGadgetHookFn GimmickActivate;          /* 8008FC30: nested by verified GetGroundHeight */
    AeGadgetHookFn MeshInitFromAreaTable;    /* 80015CC8: nested by verified GetGroundHeight */
    AeGadgetHookFn AreaEffect;               /* 8013D358: overlay callback used by GetGroundHeight */
    AeGadgetHookFn CameraShake;              /* 80048A00: nested by verified GetGroundHeight */
    AeGadgetHookFn EventCamStart;            /* 8004D39C: nested by verified GetGroundHeight */
    AeGadgetHookFn PrintfError;              /* 800A3720: nested by verified CollisionBodyRegister */
    AeGadgetHookFn GimmickTypeToSfx;          /* 8005BB98: nested by verified body code */
    AeGadgetHookFn PickupCollect;             /* 80078A18: nested by verified body code */
    AeGadgetHookFn CameraNudge;               /* 80047690: nested by verified body code */
    AeGadgetHookFn GpuDrawGadget7Trail;        /* native renderer hook for inline RC-car trail packets */
    AeGadgetHookFn GpuDrawGadget4Trail;        /* native renderer hook for inline Super Hoop trail packets */
    AeGadgetHookFn GpuProjectGadget3Aim;       /* native renderer hook for inline slingback projection */
    AeGadgetHookFn GpuDrawGadget3AimLine;      /* native renderer hook for the inline aim-line primitive */
} AeGadgetHooks;

/* ---- original functions in this batch ------------------------------------------------------- */
int32_t ae_GadgetHasAltStance(AeRam *r);                                      /* 80065694 */
void ae_GadgetFlashSet(AeRam *r, uint32_t position);                          /* 8006C6A4 */
void ae_GadgetFlashUpdate(AeRam *r, const AeGadgetHooks *h);                   /* 8006C6D4 */
void ae_GadgetSelectFx(AeRam *r, const AeGadgetHooks *h);                      /* 8006CDE0 */
void ae_Gadget6SparkleFx(AeRam *r, const AeGadgetHooks *h);                   /* 8006CEF0 */
void ae_Gadget7TrailUpdate(AeRam *r, const AeGadgetHooks *h);                  /* 8006D044 */
void ae_Gadget4TrailReset(AeRam *r);                                           /* 8006DAC8 */
void ae_Gadget4TrailUpdate(AeRam *r, const AeGadgetHooks *h);                  /* 8006DAF8 */
void ae_Gadget3FindAimTarget(AeRam *r, const AeGadgetHooks *h);                /* 8007A32C */
void ae_Gadget3LockOnAdd(AeRam *r, const AeGadgetHooks *h, uint32_t target);   /* 8007A48C */
void ae_Gadget4PlayerUpdate(AeRam *r, const AeGadgetHooks *h);                 /* 8007A514 */
void ae_GadgetSwingParamsInit(AeRam *r);                                       /* 8007AA1C */
void ae_Gadget3AimUpdateAmmo(AeRam *r, const AeGadgetHooks *h);                /* 8007AA98 */
void ae_GadgetUnlockMask(AeRam *r, uint32_t mask);                             /* 8007B358 */
void ae_GadgetSelectFxDraw(AeRam *r, const AeGadgetHooks *h);                  /* 8007C6CC */
void ae_GadgetSetSelected(AeRam *r, const AeGadgetHooks *h, int32_t gadget);   /* 8007CD0C */
void ae_GadgetSlotRemap(AeRam *r, int32_t old_gadget, int32_t new_gadget);    /* 80081144 */
void ae_Gadget4UpdateDraw(AeRam *r, const AeGadgetHooks *h);                   /* 800986E0 */
void ae_Gadget4BuildMatrix(AeRam *r, const AeGadgetHooks *h);                  /* 80098DD0 */
void ae_GadgetDrawStaticModel(AeRam *r, const AeGadgetHooks *h);               /* 80099C44 */
void ae_GadgetDrawWithStateCheck(AeRam *r, const AeGadgetHooks *h,
                                 uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3); /* 8009A4E8 */
void ae_GadgetSwingUpdate(AeRam *r, const AeGadgetHooks *h);                   /* 8009A800 */
void ae_GadgetDrawSlot0Attachment(AeRam *r, const AeGadgetHooks *h,
                                  uint32_t source_matrix, uint32_t output_matrix,
                                  int32_t angle);                                /* 8009ABF4 */
void ae_Gadget3UpdateDraw(AeRam *r, const AeGadgetHooks *h);                   /* 8009AD60 */
void ae_Gadget6UpdateSound(AeRam *r, const AeGadgetHooks *h);                  /* 8009B1C0 */

#ifdef __cplusplus
}
#endif
#endif
