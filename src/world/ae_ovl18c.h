#ifndef AE_OVL18C_H
#define AE_OVL18C_H

#include "ae_zako1.h"
#include "../effects/ae_spawn.h"

/* Area 02's code overlay is loaded at 0x8013D000. Its private tables are part of
 * that RAM image and are read by address; mutable working records are in the
 * game's ordinary RAM below. */
#define AE_OVL18C_BASE                         0x8013D000u
#define AE_OVL18C_AMBIENT_EFFECT_DEF           0x8013F1C0u
#define AE_OVL18C_ENEMY_MODEL_INSTANCE         0x8013F340u
#define AE_OVL18C_EFFECT_SLOTS                 0x8013F2A0u
#define AE_OVL18C_EFFECT_SLOT_COUNT            8u
#define AE_OVL18C_EFFECT_SLOT_SIZE             0x14u
#define AE_OVL18C_SURFACE_RECORDS              0x8013F3B0u
#define AE_OVL18C_SURFACE_RECORD_COUNT         0x8013F7B0u
#define AE_OVL18C_SURFACE_RECORD_SIZE          0x20u
#define AE_OVL18C_SURFACE_SOURCE_POINTER       0x800F459Cu
#define AE_OVL18C_SURFACE_OT_CURSOR             0x800EC354u
#define AE_OVL18C_SURFACE_LIST_SELECTOR         0x800EC35Cu
#define AE_OVL18C_SURFACE_LAYOUT_BASE           0x800EC3D0u
#define AE_OVL18C_SURFACE_LAYOUT_STRIDE         0x4078u
#define AE_OVL18C_SURFACE_MATRIX_SOURCE         0x800F45A8u
#define AE_OVL18C_SURFACE_MATRIX_POSITION_X     0x800F45B0u
#define AE_OVL18C_SURFACE_MODE_FLAGS            0x800EC250u
#define AE_OVL18C_SURFACE_GAME_MODE             0x800E6908u
#define AE_OVL18C_SURFACE_VIEW_MATRIX           0x800E78C0u
#define AE_OVL18C_SURFACE_ANIM_RECORDS          0x8013F7B8u
#define AE_OVL18C_SURFACE_ANIM_RECORD_COUNT     230u
#define AE_OVL18C_SURFACE_ANIM_RECORD_SIZE      8u
#define AE_OVL18C_SURFACE_ANIM_POS_U            0x00u
#define AE_OVL18C_SURFACE_ANIM_POS_V            0x02u
#define AE_OVL18C_SURFACE_ANIM_POS_W            0x04u
#define AE_OVL18C_SURFACE_ANIM_STATE            0x06u
#define AE_OVL18C_SURFACE_ANIM_INTENSITY        0x07u
#define AE_OVL18C_SURFACE_GLOBAL_PHASE_U        0x800AE8F0u
#define AE_OVL18C_SURFACE_GLOBAL_PHASE_V        0x800AE8F2u
#define AE_OVL18C_SURFACE_OVERLAY_PHASE_U       0x8013FEE8u
#define AE_OVL18C_SURFACE_OVERLAY_PHASE_V       0x8013FEEAu
#define AE_OVL18C_SURFACE_SCRATCH               0x1F800000u
#define AE_OVL18C_SURFACE_SCRATCH_ORIGIN_X      0x28u
#define AE_OVL18C_SURFACE_SCRATCH_ORIGIN_Y      0x2Au
#define AE_OVL18C_SURFACE_SCRATCH_ORIGIN_Z      0x2Cu
#define AE_OVL18C_SURFACE_SCRATCH_POINT_X       0x20u
#define AE_OVL18C_SURFACE_SCRATCH_POINT_Y       0x22u
#define AE_OVL18C_SURFACE_SCRATCH_POINT_Z       0x24u
#define AE_OVL18C_SURFACE_SCRATCH_PHASE_U       0x1Cu
#define AE_OVL18C_SURFACE_SCRATCH_PHASE_V       0x1Eu
#define AE_OVL18C_SURFACE_SCRATCH_SCREEN_XY     0x08u
#define AE_OVL18C_SURFACE_SCRATCH_SCREEN_Y      0x0Cu
#define AE_OVL18C_SURFACE_SCRATCH_DEPTH         0x10u
#define AE_OVL18C_SURFACE_SCRATCH_PROJECTED_XY  0x14u
#define AE_OVL18C_SURFACE_SCRATCH_VERTEX_XY     0x18u
#define AE_OVL18C_SURFACE_TYPE_Y_OFFSET         500
#define AE_OVL18C_SURFACE_PHASE_STEP            5u
#define AE_OVL18C_SURFACE_PHASE_BIAS            1u
#define AE_OVL18C_SURFACE_PHASE_ORIGIN           128
#define AE_OVL18C_SURFACE_PHASE_U_SCALE         16
#define AE_OVL18C_SURFACE_PHASE_V_SCALE         4
#define AE_OVL18C_SURFACE_ANIM_INITIAL_V        -600
#define AE_OVL18C_SURFACE_RENDER_PACKET_WIDTH   2624
#define AE_OVL18C_SURFACE_RENDER_PACKET_HEIGHT  1968
#define AE_OVL18C_SURFACE_HOOK_PROJECT_POINT    0x8013ED58u
#define AE_OVL18C_SURFACE_HOOK_PROJECT_VERTEX   0x8013EE10u
#define AE_OVL18C_SURFACE_HOOK_SUBMIT_PRIMARY   0x8013EE2Cu
#define AE_OVL18C_SURFACE_HOOK_SUBMIT_SECONDARY 0x8013EF34u
#define AE_OVL18C_EFFECT_DRAW_RENDER_HOOK        0x8013E620u
#define AE_OVL18C_ENEMY_DISTANCE_RECORDS       0x800FC350u
#define AE_OVL18C_ENEMY_DISTANCE_RECORD_SIZE   0x38u
#define AE_OVL18C_ENEMY_DISTANCE_RECORD_POINTER 0x800F455Cu
#define AE_OVL18C_DISTANCE_MIN                 0x06u
#define AE_OVL18C_DISTANCE_MAX                 0x0Au
#define AE_OVL18C_PLAYER_GROUND_Y              0x800EC208u
#define AE_OVL18C_PLAYER_POSITION              0x800FC1E0u
#define AE_OVL18C_EFFECT_OBJECTS               0x800E6960u
#define AE_OVL18C_EFFECT_OBJECT_SIZE           0xE4u
#define AE_OVL18C_ENEMY_LOOK_X                 0x800E77A8u
#define AE_OVL18C_ENEMY_LOOK_Y                 0x800E77AAu
#define AE_OVL18C_ENEMY_LOOK_Z                 0x800E77ACu

#define AE_OVL18C_SURFACE_HEADER_MAGIC         0x00u
#define AE_OVL18C_SURFACE_HEADER_KIND          0x02u
#define AE_OVL18C_SURFACE_HEADER_COUNT         0x04u
#define AE_OVL18C_SURFACE_HEADER_DATA          0x08u
#define AE_OVL18C_SURFACE_SOURCE_X             0x02u
#define AE_OVL18C_SURFACE_SOURCE_Y             0x04u
#define AE_OVL18C_SURFACE_SOURCE_Z             0x06u
#define AE_OVL18C_SURFACE_SOURCE_PARAM         0x0Au
#define AE_OVL18C_SURFACE_SOURCE_STRIDE        0x10u
#define AE_OVL18C_SURFACE_GROUND_OFFSET_1      512
#define AE_OVL18C_SURFACE_GROUND_OFFSET_2      4096
#define AE_OVL18C_SURFACE_COUNT_MAX            32u
#define AE_OVL18C_SURFACE_VALID_KIND           70u
#define AE_OVL18C_SURFACE_MAGIC                0x452Eu

/* Fields in the overlay's 0x14-byte transient effect records. */
#define AE_OVL18C_EFFECT_POS_X                 0x00u
#define AE_OVL18C_EFFECT_POS_Y                 0x02u
#define AE_OVL18C_EFFECT_POS_Z                 0x04u
#define AE_OVL18C_EFFECT_VEL_X                 0x06u
#define AE_OVL18C_EFFECT_VEL_Y                 0x08u
#define AE_OVL18C_EFFECT_VEL_Z                 0x0Au
#define AE_OVL18C_EFFECT_AGE                   0x0Cu
#define AE_OVL18C_EFFECT_OWNER                 0x0Eu
#define AE_OVL18C_EFFECT_GRAVITY_STEP          4
#define AE_OVL18C_EFFECT_GROUND_CLEARANCE      1536

/* Fields in the overlay's 0x20-byte surface/particle records. */
#define AE_OVL18C_SURFACE_POS_X                0x00u
#define AE_OVL18C_SURFACE_POS_Y                0x02u
#define AE_OVL18C_SURFACE_POS_Z                0x04u
#define AE_OVL18C_SURFACE_GROUND_Y             0x06u
#define AE_OVL18C_SURFACE_PREV_X               0x08u
#define AE_OVL18C_SURFACE_PREV_Y               0x0Au
#define AE_OVL18C_SURFACE_PREV_Z               0x0Cu
#define AE_OVL18C_SURFACE_TIMER                0x0Eu
#define AE_OVL18C_SURFACE_FLAG                 0x18u
#define AE_OVL18C_SURFACE_PARAM                0x1Au

/* Draw-record animation fields at offsets that share the initialized surface
 * record table. */
#define AE_OVL18C_DRAW_RECORD_TIMER             0x0Eu
#define AE_OVL18C_DRAW_RECORD_FRAME              0x0Fu
#define AE_OVL18C_DRAW_RECORD_TYPE               0x16u
#define AE_OVL18C_DRAW_RECORD_LIFETIME           0x18u
#define AE_OVL18C_DRAW_RECORD_RELOAD             0x1Au
#define AE_OVL18C_DRAW_RECORD_TEXTURE            0x1Cu
#define AE_OVL18C_DRAW_RECORD_ORIGIN_X           0x08u
#define AE_OVL18C_DRAW_RECORD_ORIGIN_Y           0x0Au
#define AE_OVL18C_DRAW_RECORD_ORIGIN_Z           0x0Cu
#define AE_OVL18C_DRAW_RECORD_DELTA_X            0x10u
#define AE_OVL18C_DRAW_RECORD_DELTA_Y            0x12u
#define AE_OVL18C_DRAW_RECORD_DELTA_Z            0x14u
#define AE_OVL18C_DRAW_RECORD_STOP_Y             0x06u
#define AE_OVL18C_DRAW_RECORD_HEIGHT             0x12u
#define AE_OVL18C_DRAW_RECORD_TEXTURE_PHASE      0x1Du

/* Extra named object fields used by overlay 18C. */
#define AE_OVL18C_OBJ_WAKE_TIMER               0x08Fu
#define AE_OVL18C_OBJ_EFFECT_COUNT             0x0CBu
#define AE_OVL18C_OBJ_EFFECT_ANIM               0x0CDu
#define AE_OVL18C_OBJ_PREV_EFFECT_ANIM          0x0CEu
#define AE_OVL18C_OBJ_DRAW_SCALE                0x0A0u
#define AE_OVL18C_OBJ_HIT_REACT_PARAM           0x09Au
#define AE_OVL18C_OBJ_HIT_REACT_PARAM_Z         0x09Eu
#define AE_OVL18C_OBJ_RENDER_MODE               0x0C8u
#define AE_OVL18C_OBJ_RENDER_COOLDOWN           0x0C9u
#define AE_OVL18C_OBJ_FRAME_PARAM               0x066u
#define AE_OVL18C_OBJ_BODY_TYPE                 0x07Du
#define AE_OVL18C_OBJ_LINKED_BODY               0x050u
#define AE_OVL18C_OBJ_DEFEAT_FIELD              0x0B0u
#define AE_OVL18C_OBJ_RESET_FIELD               0x048u
#define AE_OVL18C_OBJ_DEFEAT_FLAGS              0x0DCu
#define AE_OVL18C_OBJ_EFFECT_INDEX              0x0CBu

#define AE_OVL18C_EFFECT_LOCAL_POS_X            0x00u
#define AE_OVL18C_EFFECT_LOCAL_POS_Y            0x02u
#define AE_OVL18C_EFFECT_LOCAL_POS_Z            0x04u
#define AE_OVL18C_EFFECT_LOCAL_POS_PAD          0x06u
#define AE_OVL18C_SOURCE_POS_X                  0x00u
#define AE_OVL18C_SOURCE_POS_Y                  0x02u
#define AE_OVL18C_SOURCE_POS_Z                  0x04u
#define AE_OVL18C_SOURCE_POS_PAD                0x06u
#define AE_OVL18C_LOOK_Y_OFFSET                 180
#define AE_OVL18C_AMBIENT_OFFSET_SCALE          25
#define AE_OVL18C_AMBIENT_OFFSET_SHIFT          9u
#define AE_OVL18C_OBJ_WAKE_ORIGIN_Y             0x01Eu
#define AE_OVL18C_RADIAL_VELOCITY_SHIFT         3u
#define AE_OVL18C_RADIAL_POSITION_SHIFT         12u
#define AE_OVL18C_RADIAL_Y_VELOCITY             -40
#define AE_OVL18C_RADIAL_Y_OFFSET               70
#define AE_OVL18C_RADIAL_RADIUS                 160
#define AE_OVL18C_WAKE_HIT_Y_OFFSET             250

/* Explicit RAM scratch used in place of the original stack-local vectors.
 * This falls inside the checker-declared stack window. */
#define AE_OVL18C_STACK_POSITION                0x801FF180u
#define AE_OVL18C_STACK_HIT_ARGS                0x801FF190u

typedef int32_t (*AeOvl18cCallFn)(void *user, AeRam *r, const char *name,
                                  uint32_t address, uint32_t count,
                                  const uint32_t *args);

typedef int32_t (*AeOvl18cRenderFn)(void *user, AeRam *r, const char *name,
                                    uint32_t address, uint32_t count,
                                    const uint32_t *args);

typedef struct AeOvl18cHooks {
    void *user;
    const AeZakoHooks *zako_hooks;
    const AeZako1Hooks *zako1_hooks;
    const AeSpawnHooks *spawn_hooks;
    const AeWorldHooks *world_hooks;
    /* Unported EXE game helpers retain their original names, addresses,
     * argument words, return values, and call order through this callback. */
    AeOvl18cCallFn original_call;
    /* Renderer boundaries: Area02_EnemyModelDraw (8013D700),
     * Area02_EffectDraw (8013E33C), Area02_SurfaceDraw (8013E950), and
     * DrawShadow (8001F604). */
    AeOvl18cRenderFn renderer;
} AeOvl18cHooks;

/* Ported functions in overlay member 18C.bin. */
void ae_Area02_SpawnAmbientParticle(AeRam *r, const AeOvl18cHooks *h,
                                    uint32_t obj);                 /* 8013D000 */
int32_t ae_Area02_InterpolateAnimValue(AeRam *r, uint32_t obj,
                                      int32_t value_a,
                                      int32_t value_b);             /* 8013D0B0 */
int32_t ae_Area02_PlayerWithinHeightAndRadius(AeRam *r,
                                              const AeOvl18cHooks *h); /* 8013D1C4 */
int32_t ae_Area02_PlayerOutsideHeightAndRadius(AeRam *r,
                                               const AeOvl18cHooks *h); /* 8013D224 */
void ae_Area02_EnemyAnimationTriggerUpdate(AeRam *r,
                                          const AeOvl18cHooks *h,
                                          uint32_t obj);             /* 8013D294 */
void ae_Area02_UpdateEnemyWakeOrigin(AeRam *r, uint32_t obj);       /* 8013D3B8 */
void ae_Area02_UpdateEnemyWake(AeRam *r, const AeOvl18cHooks *h,
                               uint32_t obj);                       /* 8013D430 */
void ae_Area02_EnemyType0B_Update(AeRam *r, const AeOvl18cHooks *h,
                                 uint32_t obj);                     /* 8013D518 */
void ae_Area02_CopyEnemyModel(AeRam *r, uint32_t source,
                              uint32_t destination, uint32_t type); /* 8013D660 */
void ae_Area02_InitializeEffects(AeRam *r, const AeOvl18cHooks *h,
                                 uint32_t obj);                     /* 8013D7C8 */
int32_t ae_Area02_FindFreeEffectSlot(AeRam *r);                     /* 8013D81C */
void ae_Area02_SpawnRadialEffect(AeRam *r,
                                 const AeOvl18cHooks *h,
                                 uint32_t obj, int32_t y_offset,
                                 int32_t radius);                   /* 8013D858 */
int32_t ae_Area02_EffectBelowGround(AeRam *r,
                                    const AeOvl18cHooks *h,
                                    uint32_t effect);                /* 8013D990 */
void ae_Area02_SpawnEffectParticle(AeRam *r,
                                   const AeOvl18cHooks *h,
                                   uint32_t position);               /* 8013D9CC */
void ae_Area02_SpawnEffectBurst(AeRam *r,
                                const AeOvl18cHooks *h,
                                uint32_t position);                  /* 8013DA0C */
void ae_Area02_UpdateEffects(AeRam *r, const AeOvl18cHooks *h);     /* 8013DA94 */
void ae_Area02_SpawnEnemyHitParticle(AeRam *r,
                                     const AeOvl18cHooks *h,
                                     uint32_t obj);                  /* 8013DC70 */
void ae_Area02_EnemyHitUpdate(AeRam *r, const AeOvl18cHooks *h,
                              uint32_t obj);                         /* 8013DC9C */
void ae_Area02_EnemyEnterHitReaction(AeRam *r,
                                     const AeOvl18cHooks *h,
                                     uint32_t obj);                  /* 8013DD2C */
void ae_Area02_EnemySplashUpdate(AeRam *r, const AeOvl18cHooks *h,
                                 uint32_t obj);                     /* 8013DDBC */
void ae_Area02_EnemyEnterDefeat(AeRam *r, const AeOvl18cHooks *h,
                                uint32_t obj);                      /* 8013DE20 */
int32_t ae_Area02_EnemyPhase6Update(AeRam *r,
                                   const AeOvl18cHooks *h,
                                   uint32_t obj);                    /* 8013DEF8 */
void ae_Area02_StageSpecificEnemyUpdate(AeRam *r,
                                        const AeOvl18cHooks *h,
                                        uint32_t obj);               /* 8013DF54 */
int32_t ae_Area02_SurfaceEffectsInit(AeRam *r,
                                    const AeOvl18cHooks *h);         /* 8013E1E0 */
int32_t ae_Area02_EffectDraw(AeRam *r,
                             const AeOvl18cHooks *h);                /* 8013E33C */
int32_t ae_Area02_SurfaceDraw(AeRam *r,
                              const AeOvl18cHooks *h);               /* 8013E950 */

#endif
