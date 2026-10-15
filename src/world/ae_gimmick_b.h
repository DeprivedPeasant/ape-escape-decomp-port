#ifndef AE_GIMMICK_B_H
#define AE_GIMMICK_B_H

#include "ae_gimmick.h"
#include "ae_world.h"
#include "../bodies/ae_bodies.h"
#include "../effects/ae_spawn.h"
#include "../player/native_player.h"
#include "../sound/ae_sound.h"

/* Additional named fields used by the second half of the gimmick handlers. */
#define AE_G_GIMMICK_STATE_TABLE 0x800DF820u
#define AE_G_AREA_PROP_MESHES   0x800F4770u
#define AE_G_EVENT_CAM_BUSY     0x800F44A0u
#define AE_G_STAGE_ENTRY_FLAGS  0x800F4450u
#define AE_G_PLAYER             0x800EC1E0u
#define AE_G_PLAYER_POS_X       (AE_G_PLAYER + 0x24u)
#define AE_G_PLAYER_POS_Y       (AE_G_PLAYER + 0x28u)
#define AE_G_PLAYER_POS_Z       (AE_G_PLAYER + 0x2Cu)
#define AE_G_PLAYER_WALL_STATE  0x800EC238u
#define AE_G_RC_CAR_ACTOR       (AE_G_RC_CAR + 0x70u)
#define AE_RC_CAR_POSITION_X    0x24u
#define AE_RC_CAR_POSITION_Y    0x28u
#define AE_RC_CAR_POSITION_Z    0x2Cu
#define AE_G_COLL_WALL_PROBE    0x800F45E6u
#define AE_G_STAGE_AREA         0x800F447Cu

#define AE_COLLIDER_POS_X       0x00u
#define AE_COLLIDER_POS_Y       0x02u
#define AE_COLLIDER_POS_Z       0x04u
#define AE_COLLIDER_HALF_X      0x06u
#define AE_COLLIDER_HALF_Y      0x08u
#define AE_COLLIDER_HALF_Z      0x0Au
#define AE_COLLIDER_INTERACT    0x0Cu
#define AE_COLLIDER_PROP_INDEX  0x0Eu
#define AE_COLLIDER_GEOMETRY    0x10u
#define AE_COLLIDER_SURFACE     0x12u
#define AE_COLLIDER_MODE        0x14u
#define AE_COLLIDER_FLAGS       0x18u
#define AE_COLLIDER_TOUCHER     0x1Eu
#define AE_COLLIDER_ORIGIN_TAG  0x1Cu

#define AE_COLLIDER_ZONE_MASK   0x20u
#define AE_COLLIDER_ORIGIN_X    0x24u
#define AE_COLLIDER_ORIGIN_Y    0x26u
#define AE_COLLIDER_ORIGIN_Z    0x28u
#define AE_COLLIDER_AUX_HALF_X  0x32u
#define AE_COLLIDER_AUX_HALF_Y  0x34u
#define AE_COLLIDER_AUX_HALF_Z  0x36u
#define AE_COLLIDER_BREAKABLE_COUNT 0x32u
#define AE_COLLIDER_SWING_X     0x2Au
#define AE_COLLIDER_SWING_Y     0x2Cu
#define AE_COLLIDER_SWING_Z     0x2Eu
#define AE_COLLIDER_SWING_STEP  0x36u

#define AE_AREA_PROP_MESH       0x00u
#define AE_AREA_PROP_POSITION_X 0x10u
#define AE_AREA_PROP_POSITION_Y 0x12u
#define AE_AREA_PROP_POSITION_Z 0x14u
#define AE_AREA_PROP_ROTATION   0x1Cu
#define AE_PROP_MESH_FLAGS      0x00u
#define AE_PROP_MESH_HIDDEN     0x2000u
#define AE_PROP_MESH_TYPE_MASK  0x000Fu
#define AE_PROP_MESH_ROT_MATRIX_TYPE 3u

#define AE_AREA_MESHES_COUNT      0x00u
#define AE_AREA_MESHES_ENTRY_SIZE 0x20u
#define AE_AREA_MESH_ENTRY_ROT    0x1Cu
#define AE_AREA_PROP_MESH_SET_SIZE 0x18u
#define AE_AREA_PROP_MESH_SET_COUNT 0x16u
#define AE_AREA_PROP_MESH_SET_COLLIDER 0x06u

#define AE_BODY_FACING          0x18u
#define AE_BODY_FACING_Y        0x1Cu
#define AE_BODY_POSITION_X      0x00u
#define AE_BODY_POSITION_Y      0x04u
#define AE_BODY_POSITION_Z      0x08u
#define AE_BODY_RADIUS          0x20u
#define AE_BODY_HEIGHT          0x22u
#define AE_BODY_PARAM           0x24u
#define AE_BODY_FLAGS           0x26u

#define AE_PLAYER_ROTATE_X      0x30u
#define AE_PLAYER_ROTATE_Z      0x38u
#define AE_PLAYER_ATTACH_X      0x60u
#define AE_PLAYER_ATTACH_Z      0x64u
#define AE_PLAYER_VERTICAL_EXTENT 0x44u
#define AE_PLAYER_COLLIDER_INDEX 0x58u
#define AE_AREA_PROP_ROTATION_PROGRESS 0x18u

#define AE_EVENT_CAM_START_ID   0x0100u
#define AE_GIMMICK_PARAM_BOUNCE 0x08u
#define AE_GIMMICK_PARAM_RADIUS 0x0Au

/* The hooks are named after the original callees. Arguments are the four original
 * register words; pointed-to PSX data remains in AeRam. */
typedef int32_t (*AeGimmickBHookFn)(void *, AeRam *, uint32_t, uint32_t, uint32_t, uint32_t);

typedef struct AeGimmickBHooks {
    void *user;
    AeGimmickBHookFn AreaMonkeyQuotaMet;       /* 80097454 */
    AeGimmickBHookFn FUN_80097504;             /* 80097504 */
    AeGimmickBHookFn SpecialActorPlaceAtCollider; /* 8009B578 */
    AeGimmickBHookFn FUN_8009B3B0;             /* 8009B3B0 */
    AeGimmickBHookFn IsTimeAttack;             /* 80097678 */
    AeGimmickBHookFn Indicator_Register;       /* 800716EC */
    AeGimmickBHookFn Indicator_Activate;       /* 800717B4 */
    AeGimmickBHookFn SpawnEffectGroupAt;       /* 800698E4 */
    AeGimmickBHookFn PadRumble;                /* 80014168 */
    AeGimmickBHookFn FUN_8006AA60;              /* 8006AA60 */
    AeGimmickBHookFn MeshInitFromAreaTable;     /* 80015CC8 */
    AeGimmickBHookFn FUN_80091598;              /* 80091598 */
    AeGimmickBHookFn FUN_80091AF0;               /* 80091AF0 */
    AeGimmickBHookFn SoundLoopStart;            /* 800878E0 */
    AeGimmickBHookFn SoundEmitterPan;           /* 80087B34 */
    AeGimmickBHookFn SoundLoopStop;             /* 8008806C */
    AeGimmickBHookFn FUN_80092324;               /* 80092324 */
    AeGimmickBHookFn FUN_80091C04;               /* 80091C04 */
    AeGimmickBHookFn EventCamStart;             /* 8004D39C */
    AeGimmickBHookFn FUN_80093848;               /* 80093848 */
    AeGimmickBHookFn PlayerAttachSet;            /* 80066EEC */
    AeGimmickBHookFn PlaySfx;                    /* 800877F8 */
    AeGimmickBHookFn FUN_80066F34;               /* 80066F34 */
    AeGimmickBHookFn SvecToDiagMatrix;           /* 800147D0 */
    AeGimmickBHookFn FUN_80094330;               /* 80094330 */
    AeGimmickBHookFn DiagonalSignedDistance;     /* 800774B8 */
    AeGimmickBHookFn FUN_80094A48;               /* 80094A48 */
    AeGimmickBHookFn FUN_80094E5C;               /* 80094E5C */
    AeGimmickBHookFn ColliderBoxContainsPoint;   /* 8009091C */
    AeGimmickBHookFn CameraNudge;                /* 80047690 */
    const AeGimmickHooks *gimmick_hooks;         /* direct C call boundary for GimmickActivate */
    const AeWorldHooks *world_hooks;             /* direct C call boundary for GetGroundHeight */
    const AeBodyHooks *body_hooks;               /* direct C call boundary for collision-body calls */
    const AeSoundHw *sound_hw;                    /* hardware boundary for the validated sound port */
} AeGimmickBHooks;

int32_t ae_Gimmick_Init_MonkeyQuota(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 80090D6C */
int32_t ae_Gimmick_MonkeyQuota_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 80090D94 */
int32_t ae_Gimmick_NopInit(AeRam *r, uint32_t gimmick);       /* 80090EB0 */
int32_t ae_Gimmick_NopUpdate(AeRam *r, uint32_t gimmick);    /* 80090EB8 */
int32_t ae_Gimmick_Init_Closed(AeRam *r, uint32_t gimmick);  /* 80090EC0 */
int32_t ae_Gimmick_Init_ZoneMask(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 80090F0C */
int32_t ae_Gimmick_PlayerInCylinder_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 80090F80 */
int32_t ae_Gimmick_ProxTrigger_Init(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 80091060 */
int32_t ae_Gimmick_ProxTrigger_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 80091178 */
int32_t ae_Gimmick_Breakable_Init(AeRam *r, uint32_t gimmick); /* 80091358 */
int32_t ae_Gimmick_Breakable_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 800913EC */
int32_t ae_Gimmick_Init_Closed2(AeRam *r, uint32_t gimmick); /* 80091688 */
int32_t ae_Gimmick_NopUpdate2(AeRam *r, uint32_t gimmick);   /* 800916FC */
int32_t ae_Gimmick_Init_PropHidden(AeRam *r, uint32_t gimmick); /* 80091704 */
int32_t ae_Gimmick_Resize_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 8009176C */
int32_t ae_Gimmick_Init_Closed3(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 80091870 */
int32_t ae_Gimmick_Slider_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 800918FC */
int32_t ae_Gimmick_SliderQuiet_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 80091A18 */
int32_t ae_Gimmick_PlayerBox_Update(AeRam *r, uint32_t gimmick); /* 80091CF8 */
int32_t ae_GimmickPlayerInBox(AeRam *r, int32_t kind, uint32_t box, int32_t extent); /* 80091D24 */
int32_t ae_Gimmick_Init_RecordOrigin(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 80091FB0 */
int32_t ae_Gimmick_LinkedLift_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 800920AC */
int32_t ae_Gimmick_Init_PropTilt(AeRam *r, uint32_t gimmick); /* 80092A68 */
int32_t ae_Gimmick_Oscillate_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 80092B0C */
int32_t ae_Gimmick_Init_PropMeshSet(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 80092E74 */
int32_t ae_Gimmick_Init_CounterGate(AeRam *r, uint32_t gimmick); /* 80092F50 */
int32_t ae_Gimmick_Counter_EventCam_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 80092FBC */
int32_t ae_Gimmick_PlayerBlast_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 800935F0 */
int32_t ae_Gimmick_Init_Disabled(AeRam *r, uint32_t gimmick); /* 80093740 */
int32_t ae_Gimmick_Grab_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 80093788 */
int32_t ae_Gimmick_Init_PropFlag2000(AeRam *r, uint32_t gimmick); /* 8009391C */
int32_t ae_Gimmick_Bounce_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 8009398C */
int32_t ae_Gimmick_Init_Hidden(AeRam *r, uint32_t gimmick); /* 80093E74 */
int32_t ae_Gimmick_Rotate_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 80093F04 */
int32_t ae_Gimmick_Falling_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick); /* 8009468C */
int32_t ae_GimmickMoveColliderTo(AeRam *r, const AeGimmickBHooks *h, int32_t collider_index,
                                 uint32_t source, uint32_t destination); /* 80094B6C */
int32_t ae_GimmickMoveCollider(AeRam *r, const AeGimmickBHooks *h, int32_t collider_index,
                               int32_t dx, int32_t dy, int32_t dz); /* 80094C10 */
int32_t ae_Gimmick_NopUpdate3(AeRam *r, uint32_t gimmick);   /* 80094F40 */
int32_t ae_Gimmick_NopUpdate4(AeRam *r, uint32_t gimmick);   /* 80095464 */
uint32_t ae_GimmickStateTablePtr(AeRam *r);                  /* 800962A4 */

#endif
