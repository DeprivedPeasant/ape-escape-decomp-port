#ifndef AE_EFFECTS_H
#define AE_EFFECTS_H

#include "../core/ae_ram.h"
#include "ae_spawn.h"
#include "../sound/ae_sound.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Globals and records used by the per-frame effect code. */
#define AE_G_STAGE_STATE       0x800F4350u
#define AE_G_STAGE_EXIT_CODE  (AE_G_STAGE_STATE + 0x12Cu)
#define AE_G_PLAYER            0x800EC1E0u
#define AE_G_TRAIL_POINTS      0x800F4E90u
#define AE_G_TRAIL_POINT_SEED  0x800AE8F0u
#define AE_G_SPRITE_FX_QUEUE   0x800BADA0u
#define AE_G_SPRITE_FX_CURSOR  0x800B1B40u
#define AE_G_SPLASH_STATE     0x800BAD78u
#define AE_G_SPLASH_WATER_Y   0x800BAD7Cu
#define AE_G_SPLASH_TEMP_POS  0x801FFE00u
#define AE_G_PLAYER_FX_TRAIL  0x800BAD68u
#define AE_G_PLAYER_FX_DEF_A  0x800B1894u
#define AE_G_PLAYER_FX_DEF_B  0x800B18E8u
#define AE_G_EFFECTS_TINT_CACHE 0x800EC2D8u
#define AE_G_TINT_RANDOM_PARTICLE_ORIGIN 0x800B4D80u
#define AE_G_FLYING_MESHES 0x800BB920u
#define AE_G_FLYING_MESH_MODEL_TABLE 0x800B4D64u
#define AE_G_FLYING_MESH_CAMERA_MATRIX 0x800E78C0u
#define AE_G_FLYING_MESH_OUTPUT_VERTICES 0x800E7D00u
#define AE_G_FLYING_MESH_OT_STATE 0x800EC350u
#define AE_G_FLYING_MESH_OT_BASE 0x800EC3D0u
#define AE_G_FLYING_MESH_TEMP_MATRIX 0x801FFE80u
#define AE_G_WATER_ENTRY_EFFECT_POSITION 0x800B4D90u
#define AE_G_WATER_PLAYER_SPECIAL_STATE 0x800E6902u
#define AE_G_WATER_PLAYER_FLAGS 0x800FC250u
#define AE_G_WATER_FRAME_FLAGS 0x800FC350u
#define AE_G_WATER_SURFACE_SCRATCH 0x1F8003D4u
#define AE_G_WATER_TEMP_POSITION 0x801FFE00u
#define AE_G_WATER_RANDOM_SEED 0x800AE8F2u
#define AE_G_RANDOM_PARTICLES 0x800BB840u
#define AE_G_RANDOM_PARTICLE_STYLE_TABLE 0x800B1B78u
#define AE_G_RANDOM_PARTICLE_TEMP_POINTS 0x801FF010u
#define AE_G_TRAIL_POINT_RENDER_VERTICES 0x1F800008u
#define AE_G_EFFECTS_CAMERA_MATRIX 0x800E78C0u
#define AE_G_EFFECT_OBJECTS 0x800BAF00u
#define AE_G_FX_PLAYER_STATE6 0x800BAEA0u
#define AE_G_FX_PLAYER_STATE6_OT_POINTER 0x800EC354u
#define AE_G_GROUND_SPARKS 0x800BB440u
#define AE_G_GROUND_SPARKS_OT_STATE 0x800EC350u
#define AE_G_GROUND_SPARK_CAMERA_MATRIX 0x1F800088u
#define AE_G_GROUND_SPARK_TEMP_MATRIX 0x1F800068u
#define AE_G_EFFECT_OBJECT_CAMERA_MATRIX 0x1F80018Cu
#define AE_G_EFFECT_OBJECT_TEMP_MATRIX 0x1F800000u

enum {
    AE_PLAYER_STATE = 0x05Eu,
    AE_PLAYER_STATE_FLAGS = 0x070u,
    AE_PLAYER_POS_X = 0x024u,
    AE_PLAYER_POS_Y = 0x028u,
    AE_PLAYER_POS_Z = 0x02Cu,
    AE_PLAYER_VEL_X = 0x030u,
    AE_PLAYER_VEL_Y = 0x034u,
    AE_PLAYER_VEL_Z = 0x038u,
    AE_PLAYER_GROUND_Y = 0x12Cu,
    AE_SPLASH_STATE_ACTIVE = 0,
    AE_SPLASH_STATE_MODE = 2,
    AE_SPLASH_STATE_TIMER = 1,
    AE_SPLASH_STATE_TINT_TIMER = 6,
    AE_EFFECT_POSITION_X = 0,
    AE_EFFECT_POSITION_Y = 2,
    AE_EFFECT_POSITION_Z = 4,
    AE_PLAYER_TINT_MODE_TRAIL = 1,
    AE_PLAYER_TINT_MODE_BUBBLES = 2,
    AE_PLAYER_TINT_MODE_RANDOM = 3,
    AE_PLAYER_FX_TRAIL_TIMER = 8,
    AE_PLAYER_FX_TRAIL_POS_SIZE = 8,
    AE_EFFECT_TRAIL_COUNT = 8,
    AE_FLYING_MESH_COUNT = 12,
    AE_FLYING_MESH_SIZE = 0x44,
    AE_FM_MODEL_ID = 0x40,
    AE_FM_ANGLE_X = 0x20,
    AE_FM_ANGLE_Y = 0x22,
    AE_FM_ANGLE_Z = 0x24,
    AE_FM_ANGULAR_VELOCITY_X = 0x28,
    AE_FM_ANGULAR_VELOCITY_Y = 0x2A,
    AE_FM_ANGULAR_VELOCITY_Z = 0x2C,
    AE_FM_POSITION_X = 0x30,
    AE_FM_POSITION_Y = 0x32,
    AE_FM_POSITION_Z = 0x34,
    AE_FM_VELOCITY_X = 0x36,
    AE_FM_VELOCITY_Y = 0x38,
    AE_FM_VELOCITY_Z = 0x3A,
    AE_FM_STATE = 0x3C,
    AE_FM_AGE = 0x3D,
    AE_FM_GROUND_Y = 0x3E,
    AE_FM_GRAVITY = 10,
    AE_TRAIL_POINT_COUNT = 8,
    AE_TRAIL_POINT_SIZE = 8,
    AE_TRAIL_POINT_ACTIVE = 6,
    AE_TRAIL_POINT_LIFE_BASE = 12,
    AE_SPRITE_FX_COUNT = 32,
    AE_SPRITE_FX_SIZE = 8,
    AE_SPRITE_FX_AGE = 6,
    AE_SPRITE_FX_KIND = 7,
    AE_SPRITE_FX_INITIAL_AGE = 9
};

enum {
    AE_EFFECT_OBJECT_COUNT = 16,
    AE_EFFECT_OBJECT_SIZE = 0x54,
    AE_EFFECT_OBJECT_POSITION_X = 0x00,
    AE_EFFECT_OBJECT_POSITION_Y = 0x02,
    AE_EFFECT_OBJECT_POSITION_Z = 0x04,
    AE_EFFECT_OBJECT_SIZE_X = 0x08,
    AE_EFFECT_OBJECT_SIZE_Y = 0x0A,
    AE_EFFECT_OBJECT_SIZE_Z = 0x0C,
    AE_EFFECT_OBJECT_SEGMENTS = 0x0E,
    AE_EFFECT_OBJECT_RADIUS_X = 0x10,
    AE_EFFECT_OBJECT_RADIUS_Y = 0x12,
    AE_EFFECT_OBJECT_ROTATION = 0x14,
    AE_EFFECT_OBJECT_COLOR = 0x16,
    AE_EFFECT_OBJECT_RENDER_COLOR = 0x1C,
    AE_EFFECT_OBJECT_ROTATION_MATRIX = 0x20,
    AE_EFFECT_OBJECT_COLOR_STEP = 0x40,
    AE_EFFECT_OBJECT_VELOCITY_X = 0x44,
    AE_EFFECT_OBJECT_VELOCITY_Y = 0x46,
    AE_EFFECT_OBJECT_VELOCITY_Z = 0x48,
    AE_EFFECT_OBJECT_ACTIVE = 0x4A,
    AE_EFFECT_OBJECT_BODY_TYPE = 0x4C,
    AE_EFFECT_OBJECT_TIMER = 0x4E,
    AE_EFFECT_OBJECT_SPAWN_BODY = 0x4B,
    AE_EFFECT_OBJECT_BODY = 0x50,
    AE_EFFECT_BODY_CONTACT = 0x2C
};

typedef int32_t (*AeEffectsCallFn)(void *user, AeRam *r,
                                   uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
typedef uint16_t (*AeEffectsPaddingFn)(void *user, AeRam *r,
                                       uint32_t original_function, int32_t stack_offset);
typedef struct AeEffectsHooks {
    void *user;
    AeEffectsCallFn EffectRingDraw; /* 8006E5B4 */
    AeEffectsCallFn SpawnRandomParticle; /* 80070B70 */
    AeEffectsCallFn GpuProjectMesh; /* Replaces MeshTransform's RTPT block. */
    AeEffectsCallFn GpuDrawMesh; /* Replaces MeshDraw's GTE lighting and packet emission. */
    AeEffectsCallFn Gadget4TrailUpdate; /* 8006DAF8, outside this task */
    AeEffectsCallFn GadgetFlashUpdate; /* 8006C6D4, outside this task */
    const AeSoundHw *sound;
    AeEffectsPaddingFn LocalPositionPadding;
    AeEffectsCallFn GpuDrawSprite; /* Replaces SpriteEffectsDraw projection and packet emission. */
    AeEffectsCallFn RibbonDraw; /* 8006F3D0 */
    AeEffectsCallFn SetRotMatrix; /* PsyQ SDK 8002FB00 */
    AeEffectsCallFn SetTransMatrix; /* PsyQ SDK 8002FB30 */
    AeEffectsCallFn GpuDrawTrailPoint; /* Native trail point quad renderer. */
    AeEffectsCallFn GpuDrawEffectObj; /* Native projection and packet emission for EffectObjDraw. */
    AeEffectsCallFn SparkAddRandom; /* 8006E0DC */
    AeEffectsCallFn GpuDrawFxPlayerState6; /* Native GTE projection and packet emission. */
    AeEffectsCallFn GpuDrawGroundSpark; /* Native GTE projection and packet emission. */
} AeEffectsHooks;

int32_t ae_PlayerCanExitStage(AeRam *r);                                      /* 8005DC78 */
void ae_TrailPointAdd(AeRam *r, uint32_t position);                           /* 8006EF08 */
void ae_SpriteEffectQueue(AeRam *r, uint32_t spec, int32_t kind);             /* 8006D4C0 */
void ae_SpriteEffectQueue2(AeRam *r, uint32_t spec);                          /* 8006D588 */
void ae_PlayerSplash(AeRam *r, const AeSpawnHooks *spawn,
                     const AeSoundHw *sound);                                  /* 8006B950 */
void ae_PlayerFxTrailUpdate(AeRam *r, const AeSpawnHooks *spawn);             /* 8006B168 */
void ae_PlayerTintEffectUpdate(AeRam *r, const AeSpawnHooks *spawn,
                               const AeEffectsHooks *hooks);                   /* 8006C334 */
void ae_MeshTransform(AeRam *r, const AeEffectsHooks *hooks,
                      uint32_t mesh, uint32_t matrix, uint32_t output);        /* 800160EC */
void ae_MeshDraw(AeRam *r, const AeEffectsHooks *hooks,
                 uint32_t ordering_table, uint32_t screen_vertices,
                 uint32_t mesh);                                              /* 80017E5C */
void ae_FlyingMeshesUpdateDraw(AeRam *r, const AeSpawnHooks *spawn,
                               const AeEffectsHooks *hooks);                   /* 80071370 */
void ae_PlayerEffectsUpdate(AeRam *r, const AeSpawnHooks *spawn,
                            const AeEffectsHooks *hooks);                      /* 8006B800 */
void ae_PlayerWaterEffectsUpdate(AeRam *r, const AeSpawnHooks *spawn,
                                 const AeEffectsHooks *hooks);                   /* 8006BA2C */
void ae_SpriteEffectsDraw(AeRam *r, const AeEffectsHooks *hooks);              /* 8006D5A8 */
void ae_EffectsUpdateAll(AeRam *r, const AeSpawnHooks *spawn,
                         const AeEffectsHooks *hooks);                         /* 8006A9F8 */
void ae_TrailsUpdate(AeRam *r, const AeEffectsHooks *hooks);                   /* 8006EE3C */
void ae_RandomParticlesUpdate(AeRam *r, const AeEffectsHooks *hooks);          /* 80070D94 */
void ae_TrailPointsDraw(AeRam *r, const AeEffectsHooks *hooks);                /* 8006EF88 */
void ae_EffectObjDraw(AeRam *r, const AeEffectsHooks *hooks, uint32_t object); /* 8006FA10 */
void ae_EffectObjsUpdate(AeRam *r, const AeEffectsHooks *hooks);                /* 8006FEE8 */
void ae_FxPlayerState6Update(AeRam *r, const AeEffectsHooks *hooks);             /* 8006E1C0 */
void ae_GroundSparksUpdateDraw(AeRam *r, const AeSpawnHooks *spawn,
                               const AeEffectsHooks *hooks);                    /* 80070560 */

#ifdef __cplusplus
}
#endif
#endif
