#ifndef AE_FXSCRIPT_H
#define AE_FXSCRIPT_H

#include "../core/ae_ram.h"
#include "../sound/ae_sound.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Address constants and layouts used by EffectScriptUpdate, particles, projectiles and the RC car. */
#define AE_FX_PARTICLES                 0x800B9900u
#define AE_FX_PARTICLE_COUNT            100u
#define AE_FX_PARTICLE_STRIDE           0x34u
#define AE_FX_PARTICLE_TYPE             0x00u
#define AE_FX_PARTICLE_FLAGS            0x01u
#define AE_FX_PARTICLE_ANCHOR           0x02u
#define AE_FX_PARTICLE_LOOP_DEPTH       0x03u
#define AE_FX_PARTICLE_SCRIPT           0x04u
#define AE_FX_PARTICLE_LOOP_PC0         0x08u
#define AE_FX_PARTICLE_LOOP_PC1         0x0Cu
#define AE_FX_PARTICLE_LOOP_COUNT0      0x10u
#define AE_FX_PARTICLE_LOOP_COUNT1      0x11u
#define AE_FX_PARTICLE_WIDTH            0x12u
#define AE_FX_PARTICLE_HEIGHT           0x13u
#define AE_FX_PARTICLE_POSITION         0x14u
#define AE_FX_PARTICLE_FADE             0x1Au
#define AE_FX_PARTICLE_VELOCITY         0x1Cu
#define AE_FX_PARTICLE_WAIT             0x22u
#define AE_FX_PARTICLE_UV0              0x24u
#define AE_FX_PARTICLE_CLUT             0x26u
#define AE_FX_PARTICLE_UV1              0x28u
#define AE_FX_PARTICLE_TPAGE            0x2Au
#define AE_FX_PARTICLE_UV2              0x2Cu
#define AE_FX_PARTICLE_UV3              0x2Eu
#define AE_FX_PARTICLE_RGB              0x30u
#define AE_FX_PARTICLE_PRIMITIVE        0x33u
#define AE_FX_RANDOM_SEED               0x800AE8F0u
#define AE_FX_WATER_RANDOM_SEED          0x800AE8F2u
#define AE_FX_VIEW_MATRIX               0x800E78C0u
#define AE_FX_PLAYER                    0x800EC1E0u
#define AE_FX_PLAYER_POSITION           0x24u
#define AE_FX_PLAYER_GROUND_HEIGHT      0x12Cu
#define AE_FX_PLAYER_GADGET7_ALLOWED    0xF2u
#define AE_FX_PLAYER_GADGET7_TIMER      0xE8u
#define AE_FX_PLAYER_FACING             0x60u
#define AE_FX_PLAYER_UP                 0x64u
#define AE_FX_PARTICLE_ORIGIN           0x800B4D80u
#define AE_FX_PARTICLE_ORIGIN_LOW       0x800B4D88u
#define AE_FX_PARTICLE_WATER_HEIGHT     0x800B4D90u
#define AE_FX_ERROR_FORMAT_LOOP         0x800A5BF0u
#define AE_FX_ERROR_FORMAT_COMMAND      0x800A5C10u
#define AE_FX_ERROR_FORMAT_LOOP_DEPTH  0x800A5C18u
#define AE_FX_ERROR_FORMAT_END          0x800A5C14u

#define AE_FX_PROJECTILE_GROUPS         0x800BF300u
#define AE_FX_PROJECTILE_GROUP_STRIDE   0x1Cu
#define AE_FX_PROJECTILE_GROUP_COUNT    4u
#define AE_FX_PROJECTILE_GROUP_ACTIVE   0x12u
#define AE_FX_PROJECTILE_GROUP_OWNER    0x08u
#define AE_FX_PROJECTILE_GROUP_RECORD   0x04u
#define AE_FX_PROJECTILE_GROUP_DATA     0x0Cu
#define AE_FX_PROJECTILE_GROUP_MOTION   0x13u
#define AE_FX_PROJECTILE_GROUP_KIND     0x14u
#define AE_FX_PROJECTILE_GROUP_FLAGS    0x16u
#define AE_FX_PROJECTILE_GROUP_TIMER    0x18u
#define AE_FX_PROJECTILE_GROUP_AUX      0x1Au
#define AE_FX_PROJECTILE_GROUP_PROGRESS 0x13u
#define AE_FX_PROJECTILE_GROUP_VELOCITY 0x14u
#define AE_FX_PROJECTILE_GROUP_POSITION  0x0Cu
#define AE_FX_PROJECTILE_AIM_MODE       0x800E6916u
#define AE_FX_PROJECTILE_BODY_HEIGHT    0x800FD3DEu
#define AE_FX_PROJECTILE_BODY_BASE      0x800FD3E4u
#define AE_FX_PROJECTILE_BODY_OWNER     0x800EC1E0u
#define AE_FX_PROJECTILE_GROUP_MODEL_TABLE 0x800B296Cu
#define AE_FX_PROJECTILE_GROUP_MODEL_SOURCE 0x800F4C30u
#define AE_FX_PROJECTILE_GROUP_OVERLAY_KIND 0x800F4476u
#define AE_FX_PROJECTILE_GROUP_MODEL_INSTANCES 0x800E7D00u
#define AE_FX_PROJECTILE_GROUP_MODEL_OBJECTS 0x800EC3D0u
#define AE_FX_PROJECTILE_GROUP_WORLD_ORIGIN 0x800E7D00u
#define AE_FX_PROJECTILE_GROUP_MATRIX_LOCAL 0x801FF060u
#define AE_FX_PROJECTILE_GROUP_MATRIX_TEMP 0x801FF080u
#define AE_FX_PROJECTILE_GROUP_LOCAL_VECTOR 0x801FF0A0u
#define AE_FX_PROJECTILE_GROUP_NORMALIZED   0x801FF0C0u
#define AE_FX_PROJECTILE_STATE_AIM_TARGET_X 0x258u
#define AE_FX_PROJECTILE_STATE_AIM_TARGET_Y 0x25Au
#define AE_FX_PROJECTILE_STATE_AIM_TARGET_Z 0x25Cu
#define AE_FX_PROJECTILE_LOCAL_NORMAL   0x801FF010u
#define AE_FX_PROJECTILE_LOCAL_IMPACT   0x801FF018u
#define AE_FX_PROJECTILE_LOCAL_RIBBON   0x801FF020u
#define AE_FX_SCRIPT_LOCAL_GROUND       0x801FF018u
#define AE_FX_SCRIPT_LOCAL_WATER        0x801FF020u
#define AE_FX_PROJECTILE_RECORDS        0x800F5380u
#define AE_FX_PROJECTILE_RECORD_STRIDE  0x34u
#define AE_FX_PROJECTILE_RECORD_COUNT   32u
#define AE_FX_PROJECTILE_RECORD_STATE   0x00u
#define AE_FX_PROJECTILE_RECORD_EXPIRE  0x04u
#define AE_FX_PROJECTILE_RECORD_KIND    0x01u
#define AE_FX_PROJECTILE_RECORD_FLAGS   0x02u
#define AE_FX_PROJECTILE_RECORD_OWNER   0x08u
#define AE_FX_PROJECTILE_RECORD_BODY    0x0Cu
#define AE_FX_PROJECTILE_RECORD_SOURCE  0x10u
#define AE_FX_PROJECTILE_RECORD_NORMAL  0x14u
#define AE_FX_PROJECTILE_RECORD_POSITION 0x1Cu
#define AE_FX_PROJECTILE_RECORD_VELOCITY 0x24u
#define AE_FX_PROJECTILE_RECORD_AGE     0x03u
#define AE_FX_PROJECTILE_RECORD_SURFACE 0x28u
#define AE_FX_PROJECTILE_RECORD_RADIUS  0x1Cu
#define AE_FX_PROJECTILE_RECORD_HEIGHT  0x1Eu
#define AE_FX_PROJECTILE_RECORD_PARAM   0x20u
#define AE_FX_PROJECTILE_RECORD_FALL     0x22u
#define AE_FX_PROJECTILE_RECORD_COLOR   0x23u
#define AE_FX_PROJECTILE_RECORD_TPAGE   0x2Au
#define AE_FX_PROJECTILE_RECORD_CLUT    0x2Cu
#define AE_FX_PROJECTILE_RECORD_WIDTH   0x25u
#define AE_FX_PROJECTILE_RECORD_COLOR_INDEX 0x23u
#define AE_FX_PROJECTILE_RECORD_COLOR_MODE 0x24u
#define AE_FX_PROJECTILE_RECORD_SIZE    0x30u
#define AE_FX_PROJECTILE_RECORD_UPDATE  0x32u
#define AE_FX_PROJECTILE_COLOR_TABLE    0x800B2950u
#define AE_FX_RIBBON_COLOR_TABLE        0x800B1B78u
#define AE_FX_PROJECTILE_SPREAD_TABLE   0x800B2960u
#define AE_FX_PROJECTILE_ROTATIONS      0x800F5090u
#define AE_FX_PROJECTILE_GROUP_STATE    0x800F4350u
#define AE_FX_PROJECTILE_GROUP_NEXT     0x800FC350u
#define AE_FX_PROJECTILE_GROUP_NEXT_OFFSET 0x800FC354u
#define AE_FX_COLLISION_BODY_KIND       0x2Cu
#define AE_FX_COLLISION_BODY_VECTOR     0x18u
#define AE_FX_COLLISION_BODY_HEIGHT     0x20u
#define AE_FX_COLLISION_BODY_TPAGE      0x24u
#define AE_FX_COLLISION_BODY_CLUT       0x26u
#define AE_FX_MATRIX_TRANSLATION        0x24u
#define AE_FX_RIBBON_OT_OBJECT_INDEX    0x0Cu

#define AE_FX_RC_CAR                    0x800F5A20u
#define AE_FX_RC_ACTIVE                 0x70u
#define AE_FX_RC_POSITION               0x24u
#define AE_FX_RC_VELOCITY               0x30u
#define AE_FX_RC_VERTICAL_SPEED         0x34u
#define AE_FX_RC_VERTICAL_ACCEL         0x38u
#define AE_FX_RC_DIRECTION               0x68u
#define AE_FX_RC_SPIN                   0x6Au
#define AE_FX_RC_COLLISION_BODY         0x24u
#define AE_FX_RC_BODY                   0x800F5A44u
#define AE_FX_RC_BODY_POINTER            0x800F5A44u
#define AE_FX_RC_AIM_STATE              0x800F51C0u
#define AE_FX_RC_AIM_KIND                0x150u
#define AE_FX_RC_AIM_ACTIVE              0x800F5A90u
#define AE_FX_RC_AIM_SPEED_X             0x800F5A50u
#define AE_FX_RC_AIM_SPEED_Z             0x800F5A58u
#define AE_FX_RC_LAST_SURFACE            0x800F5A94u
#define AE_FX_RC_UPDATE_TIMER             0x78u
#define AE_FX_RC_INPUT_OFFSET             0x46u
#define AE_FX_RC_MATRIX                   0x800F5A80u
#define AE_FX_RC_PROJECTILE_POS           0x800F5AA0u
#define AE_FX_RC_RUMBLE_COUNTER           0x800F5A98u
#define AE_FX_RC_PARTICLE_POSITION        0x800F5A38u
#define AE_FX_RC_TURN_VECTOR               0x800F5320u
#define AE_FX_RC_VERTICAL_STATE             0x34u
#define AE_FX_RC_SPEED_X                    0x30u
#define AE_FX_RC_SPEED_Z                    0x38u
#define AE_FX_RC_IMPACT_FLAGS               0x08u
#define AE_FX_RC_FACING_VECTOR              0x68u
#define AE_FX_RC_SURFACE_PREVIOUS           0x74u
#define AE_FX_RC_SURFACE_CURRENT            0x76u
#define AE_FX_RC_IMPACT_COUNT               0x7Au
#define AE_FX_RC_BODY_CONTACT               0x50u
#define AE_FX_RC_CAR_SIZE                   0x46u
#define AE_FX_RC_AIM_VECTOR_X                0x60u
#define AE_FX_RC_AIM_VECTOR_Z                0x64u
#define AE_FX_RC_AIM_STRENGTH                 0x800E54A2u
#define AE_FX_RC_GTE_NORMAL_X                 0x0Cu
#define AE_FX_RC_GTE_NORMAL_Z                 0x0Eu
#define AE_FX_RC_STEERING_MODE             0x800F44A0u
#define AE_FX_RC_TARGET_TURN                0x800E54BCu
#define AE_FX_RC_TARGET_STRENGTH            0x800E54A2u
#define AE_FX_RC_FRAME_COUNTER              0x800EC350u
#define AE_FX_RC_LOCAL_POSITION              0x801FF010u
#define AE_FX_RC_LOCAL_DIRECTION             0x801FF020u

#define AE_FX_RIBBON_PROJECTED_X          0x1F80008Cu
#define AE_FX_RIBBON_PROJECTED_Y          0x1F8000ACu
#define AE_FX_RIBBON_DEPTH                 0x1F80000Cu
#define AE_FX_RIBBON_HALF_WIDTH            0x1F80004Cu
#define AE_FX_RIBBON_EDGE_A                0x1F8000CCu
#define AE_FX_RIBBON_EDGE_B                0x1F8000D0u
#define AE_FX_RIBBON_EDGE_STRIDE            8u
#define AE_FX_RENDER_SCRATCH               0x1F800000u
#define AE_FX_RIBBON_OT_TABLE               0x800EC350u

typedef int32_t (*AeFxscriptHookFn)(void *user, AeRam *r,
                                    uint32_t a0, uint32_t a1, uint32_t a2,
                                    uint32_t a3, uint32_t a4);
typedef uint16_t (*AeFxscriptPositionPaddingFn)(void *user, AeRam *r,
                                                uint32_t original_function);

typedef struct AeFxscriptHooks {
    void *user;
    const AeSoundHw *sound;
    uint16_t stale_spawn_position_pad;
    AeFxscriptPositionPaddingFn position_padding;

    /* Hardware facing SDK entry points, renderer output and libc memset. */
    AeFxscriptHookFn SetRotMatrix;             /* 8002FB00 */
    AeFxscriptHookFn SetTransMatrix;            /* 8002FB30 */
    AeFxscriptHookFn ClearMemory;               /* memset 800A3740 */
    AeFxscriptHookFn ProjectEffectPoint;         /* renderer GTE projection replacement */
    AeFxscriptHookFn DrawParticleQuad;           /* renderer particle primitive replacement */
    AeFxscriptHookFn DrawRibbonStrip;             /* renderer ribbon primitive replacement */

    /* Game functions which are outside this function table. */
    AeFxscriptHookFn Printf;                     /* 800A3720 */
    AeFxscriptHookFn SoundLoopStop;              /* 8008806C */
    AeFxscriptHookFn CameraShake;                /* 80048A00 */
    AeFxscriptHookFn PadRumble;                  /* 80014168 */
    AeFxscriptHookFn PlayerCanActivateGadget;     /* 8007CA48 */
    AeFxscriptHookFn TurnTowardsY;                /* 80066C90 */
    AeFxscriptHookFn StepToward;                  /* 800463A4 */
    AeFxscriptHookFn SpawnRandomParticle;         /* 80070B70 */
    AeFxscriptHookFn ProjectileExpireCallback;    /* callback address stored at projectile +4 */
    AeFxscriptHookFn FUN_8014022c;                /* overlay-only callback, address not in EXE */
    AeFxscriptHookFn FUN_8014263c;                /* overlay-only callback, address not in EXE */

    /* Native integrations reached through already ported Spawn / World routines. */
    AeFxscriptHookFn GetSolidHeight;              /* 8004D8C0 */
    AeFxscriptHookFn GimmickActivate;             /* 8008FC30 */
    AeFxscriptHookFn MeshInitFromAreaTable;        /* 80015CC8 */
    AeFxscriptHookFn FUN_8013d358;                /* ovl_197 */
    AeFxscriptHookFn EventCamStart;                /* 8004D39C */
    AeFxscriptHookFn ModelTransformRigid;           /* 800161C0 */
    AeFxscriptHookFn ModelDraw;                    /* 80017CD4 */
} AeFxscriptHooks;

void ae_ParticlesDraw(AeRam *r, const AeFxscriptHooks *h); /* 80069920 */
void ae_EffectScriptUpdate(AeRam *r, const AeFxscriptHooks *h); /* 80069BEC */
void ae_RibbonDraw(AeRam *r, const AeFxscriptHooks *h, uint32_t points,
                   uint32_t count, uint32_t color_table, uint32_t scale); /* 8006F3D0 */
void ae_ProjectileSystemsUpdate(AeRam *r, const AeFxscriptHooks *h); /* 80084AA0 */
void ae_ProjectileExpire(AeRam *r, const AeFxscriptHooks *h,
                         uint32_t projectile, uint32_t reason); /* 80084BC0 */
void ae_ProjectileRecordsUpdate(AeRam *r, const AeFxscriptHooks *h); /* 80084C94 */
uint32_t ae_ProjectileGroupSpawn(AeRam *r, const AeFxscriptHooks *h,
                                 uint32_t position, uint32_t direction,
                                 uint32_t count, uint32_t owner); /* 80085254 */
void ae_ProjectileGroupSpawnSpread(AeRam *r, const AeFxscriptHooks *h,
                                   uint32_t position, uint32_t direction,
                                   uint32_t mode); /* 80085630 */
void ae_ProjectileGroupsUpdate(AeRam *r, const AeFxscriptHooks *h); /* 800856FC */
void ae_RcCarUpdate(AeRam *r, const AeFxscriptHooks *h); /* 800994A8 */
int32_t ae_FUN_8014022c(AeRam *r, const AeFxscriptHooks *h,
                        uint32_t position, uint32_t vector); /* 8014022C */
int32_t ae_FUN_8014263c(AeRam *r, const AeFxscriptHooks *h,
                        uint32_t position, uint32_t vector); /* 8014263C */

#ifdef __cplusplus
}
#endif
#endif
