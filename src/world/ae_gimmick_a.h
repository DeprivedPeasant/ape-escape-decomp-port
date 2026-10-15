#ifndef AE_GIMMICK_A_H
#define AE_GIMMICK_A_H

#include "ae_gimmick.h"
#include "ae_world.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"

/* Gimmick batch A: globals and layouts recovered from the original EXE listings. */
#define AE_GA_GIMMICK_GROUPS             0x800F456Cu
#define AE_GA_GIMMICK_GROUP_COUNT        0x800F45D8u
#define AE_GA_HANDLER_TABLE              0x800AF868u
#define AE_GA_SAVE_TABLE                 0x800B9030u
#define AE_GA_STAGE_ENTRY_FLAGS          0x800F4450u
#define AE_GA_AREA_EXIT_CODE              0x800F447Cu
#define AE_GA_UPDATE_GROUP_STATE          0x800F45E0u
#define AE_GA_WALL_PROBE_FLAG            0x800F45E6u
#define AE_GA_CARRY_OVER_X               0x800BFA20u
#define AE_GA_CARRY_OVER_Y               0x800BFA22u
#define AE_GA_CARRY_OVER_Z               0x800BFA24u
#define AE_GA_PLAYER                     0x800EC1E0u
#define AE_GA_RC_CAR                     0x800F5A20u
#define AE_GA_MONKEYS                    0x800E5500u
#define AE_GA_MONKEY_SIZE                0x250u
#define AE_GA_MONKEY_ACTIVITY            0x7Au
#define AE_GA_RC_CAR_ACTIVE              0x70u

#define AE_GA_SAVE_ENTRY_SIZE            0x1Eu
#define AE_GA_SAVE_ENTRY_USED            0x00u
#define AE_GA_SAVE_ENTRY_STAGE           0x02u
#define AE_GA_SAVE_ENTRY_GIMMICK_INDEX   0x04u
#define AE_GA_SAVE_ENTRY_DATA            0x06u
#define AE_GA_SAVE_ENTRY_TYPE            0x06u
#define AE_GA_SAVE_ENTRY_FIELD_0E        0x0Eu
#define AE_GA_SAVE_ENTRY_FIELD_10        0x10u
#define AE_GA_SAVE_ENTRY_FIELD_12        0x12u
#define AE_GA_SAVE_ENTRY_FIELD_14        0x14u
#define AE_GA_SAVE_ENTRY_FIELD_16        0x16u
#define AE_GA_SAVE_ENTRY_FIELD_18        0x18u
#define AE_GA_SAVE_ENTRY_FIELD_1A        0x1Au
#define AE_GA_SAVE_ENTRY_COUNT           64u

#define AE_GA_GROUP_RECORD_SIZE          8u
#define AE_GA_GROUP_COUNT_FIELD          0x00u
#define AE_GA_GROUP_FLAGS                0x02u
#define AE_GA_GROUP_FIRST_GIMMICK        0x04u
#define AE_GA_HANDLER_RECORD_SIZE        12u
#define AE_GA_HANDLER_INIT               0x00u
#define AE_GA_HANDLER_UPDATE             0x04u
#define AE_GA_HANDLER_GROUP_STATE        0x08u

#define AE_GA_PLAYER_BODY_COLLIDER       0x58u
#define AE_GA_ACTOR_POSITION             0x24u
#define AE_GA_POSITION_X                 0x00u
#define AE_GA_POSITION_Y                 0x04u
#define AE_GA_POSITION_Z                 0x08u
#define AE_GA_DELTA_X                    0x00u
#define AE_GA_DELTA_Y                    0x02u
#define AE_GA_DELTA_Z                    0x04u
#define AE_GA_VECTOR_X                   0x00u
#define AE_GA_VECTOR_Y                   0x02u
#define AE_GA_VECTOR_Z                   0x04u
#define AE_GA_SWING_TEMP_VECTOR           0x801FF000u
#define AE_GA_SPIN_TEMP_VECTOR            0x801FF010u
#define AE_GA_PROBE_HEIGHTS               0x1F80003Cu
#define AE_GA_PROBE_HEIGHT_FOURTH         0x06u
#define AE_GA_GROUND_QUERY_SCRATCH        0x1F8003F0u
#define AE_GA_GROUND_QUERY_SURFACE        0x08u
#define AE_GA_GROUND_QUERY_SENTINEL       0x00u
#define AE_GA_COLLIDER_GEOMETRY          0x10u
#define AE_GA_COLLIDER_SFX_TYPE          0x16u
#define AE_GA_COLLIDER_TOUCHED_BY        0x1Eu
#define AE_GA_COLLIDER_ORIGIN_X          0x24u
#define AE_GA_COLLIDER_ORIGIN_Y          0x26u
#define AE_GA_COLLIDER_ORIGIN_Z          0x28u
#define AE_GA_COLLIDER_SWING_RANGE_X     0x2Au
#define AE_GA_COLLIDER_SWING_RANGE_Y     0x2Cu
#define AE_GA_COLLIDER_SWING_RANGE_Z     0x2Eu
#define AE_GA_COLLIDER_SWING_STEP        0x36u
#define AE_GA_PROP_MATRIX_POINTER        0x1Cu
#define AE_GA_PROP_RECORD_SIZE            0x20u
#define AE_GA_PROP_OBJECT_POINTER         0x00u
#define AE_GA_PROP_OBJECT_KIND            0x00u
#define AE_GA_PROP_POSITION_X             0x10u
#define AE_GA_PROP_POSITION_Y             0x12u
#define AE_GA_PROP_POSITION_Z             0x14u
#define AE_GA_PAIR_AREA_TABLE             0x800F4770u
#define AE_GA_PAIR_AREA_RECORD_SIZE       0x18u
#define AE_GA_PAIR_AREA_HEIGHT            0x16u

/* Most callbacks have the original four argument registers.  They are named after the
 * original function.  GimmickHandler is only for indirect area-table entries: its first
 * argument is the original function address, followed by the original a0 value. */
typedef int32_t (*AeGimmickAHookFn)(void *, AeRam *, uint32_t, uint32_t, uint32_t, uint32_t);
typedef int32_t (*AeGimmickAHandlerFn)(void *, AeRam *, uint32_t, uint32_t, uint32_t);

typedef struct AeGimmickAHooks {
    void *user;
    AeGimmickAHookFn CameraNudge;             /* 80047690 */
    AeGimmickAHookFn BodyInsideBox;            /* 8008EC70 */
    AeGimmickAHookFn GimmickTouchCheck;         /* 80090754 */
    AeGimmickAHookFn MeshInitFromAreaTable;     /* 80015CC8 */
    AeGimmickAHookFn PropSetMesh;               /* 80090E54 */
    AeGimmickAHookFn PlaySfx;                   /* 800877F8 */
    AeGimmickAHookFn PlaySfxAt;                 /* 80087A20 */
    AeGimmickAHookFn SoundLoopStart;             /* 800878E0 */
    AeGimmickAHookFn SoundEmitterPan;            /* 80087B34 */
    AeGimmickAHookFn SoundLoopStop;              /* 8008806C */
    AeGimmickAHookFn ProgressFlagGet;            /* 8009632C */
    AeGimmickAHookFn ProgressFlagSet;            /* 800962F4 */
    AeGimmickAHookFn ProgressGetByte;            /* 800963AC */
    AeGimmickAHookFn ProgressSetByte;            /* 80096360 */
    AeGimmickAHookFn GetSolidHeight;              /* 8004D8C0, called through ae_GetGroundHeight */
    AeGimmickAHookFn SwingDoorBodyDelta;           /* 8008F308 */
    AeGimmickAHookFn SlideDoorMove;                 /* 8008FAD0 */
    AeGimmickAHandlerFn GimmickHandler;          /* remaining indirect area init/update handlers */
    uint16_t ground_stale_x, ground_stale_z;     /* GetGroundHeight no-heightfield stack values */
} AeGimmickAHooks;

int32_t ae_GimmickSaveTableReset(AeRam *r);                                      /* 8005AD90 */
int32_t ae_GimmickSaveStates(AeRam *r);                                          /* 8005ADF8 */
int32_t ae_GimmickRestoreStates(AeRam *r);                                       /* 8005AF4C */
int32_t ae_GimmickSaveEntryAdd(AeRam *r, int32_t index, uint32_t gimmick);       /* 8005B130 */
int32_t ae_GimmickInitFromProgress(AeRam *r, const AeGimmickAHooks *h);           /* 8005B318 */
int32_t ae_GimmickSaveApplyProgress(AeRam *r, const AeGimmickAHooks *h);          /* 8005B480 */
int32_t ae_GimmickSpawnAll(AeRam *r, const AeGimmickAHooks *h);                   /* 8005B538 */
int32_t ae_GimmickUpdateAll(AeRam *r, const AeGimmickAHooks *h);                  /* 8005B68C */
int32_t ae_GimmickGetState(AeRam *r, int32_t index);                             /* 8005BB5C */
int32_t ae_GimmickTypeToSfx(AeRam *r, int32_t type);                             /* 8005BB98 */
int32_t ae_GimmickCarryBodiesByDelta(AeRam *r, const AeGimmickAHooks *h,
                                     uint32_t gimmick, uint32_t delta, int32_t collider_index); /* 8008AA6C */
int32_t ae_GimmickCarryBodiesSwing(AeRam *r, const AeGimmickAHooks *h,
                                   uint32_t collider, uint32_t delta, int32_t collider_index); /* 8008ABB0 */
int32_t ae_GimmickSwingCarryPoint(AeRam *r, uint32_t collider, uint32_t position,
                                 uint32_t offset, uint32_t out);                 /* 8008AD14 */
int32_t ae_GimmickSwingAngle(AeRam *r, int32_t range, int32_t step);             /* 8008AF98 */
int32_t ae_GimmickSwingFirstStep(AeRam *r, uint32_t collider);                   /* 8008B060 */
int32_t ae_GimmickSwingAdvance(AeRam *r, uint32_t collider);                     /* 8008B228 */
int32_t ae_GimmickSwingProbeWall(AeRam *r, const AeGimmickAHooks *h,
                                uint32_t collider, int32_t dx, int32_t dz, uint32_t out); /* 8008B3A4 */
int32_t ae_Gimmick_SwingDoor_Init(AeRam *r, const AeGimmickAHooks *h,
                                  uint32_t gimmick);                             /* 8008E5C0 */
int32_t ae_Gimmick_SwingDoor_Update(AeRam *r, const AeGimmickAHooks *h, uint32_t gimmick); /* 8008E90C */
int32_t ae_GimmickPlayTypeSfx(AeRam *r, const AeGimmickAHooks *h, uint32_t collider); /* 8008EAE4 */
int32_t ae_GimmickAnyBodyInside(AeRam *r, const AeGimmickAHooks *h, uint32_t collider,
                                int32_t radius, uint32_t arg);                  /* 8008EB58 */
int32_t ae_Gimmick_LiftDoor_Init(AeRam *r, uint32_t gimmick);                     /* 8008EF60 */
int32_t ae_Gimmick_LiftDoor_Update(AeRam *r, const AeGimmickAHooks *h, uint32_t gimmick); /* 8008F0BC */
int32_t ae_Gimmick_Init_ThresholdDoor(AeRam *r, uint32_t gimmick);                /* 8008F468 */
int32_t ae_Gimmick_ThresholdDoor_Update(AeRam *r, const AeGimmickAHooks *h, uint32_t gimmick); /* 8008F53C */
int32_t ae_Gimmick_SlideDoor_Init(AeRam *r, const AeGimmickAHooks *h,
                                  uint32_t gimmick);                             /* 8008F720 */
int32_t ae_Gimmick_SlideDoor_Update(AeRam *r, const AeGimmickAHooks *h, uint32_t gimmick); /* 8008F8D4 */
int32_t ae_Gimmick_Switch_Init(AeRam *r, const AeGimmickAHooks *h, uint32_t gimmick); /* 80090370 */
int32_t ae_Gimmick_Switch_Update(AeRam *r, const AeGimmickAHooks *h, uint32_t gimmick); /* 80090500 */
int32_t ae_Gimmick_Init_PairToggle(AeRam *r, const AeGimmickAHooks *h, uint32_t gimmick); /* 800905A4 */
int32_t ae_Gimmick_Hit_Update(AeRam *r, const AeGimmickAHooks *h, uint32_t gimmick); /* 80090660 */
int32_t ae_Gimmick_Init_Counter(AeRam *r, uint32_t gimmick);                      /* 800909A8 */
int32_t ae_Gimmick_SpinRoller_Update(AeRam *r, uint32_t gimmick);                 /* 800909FC */

#endif
