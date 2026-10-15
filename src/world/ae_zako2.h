#ifndef AE_ZAKO2_H
#define AE_ZAKO2_H

#include "../core/ae_ram.h"
#include "ae_items_init.h"
#include "ae_world.h"
#include "../effects/ae_spawn.h"

typedef struct AeBodyHooks AeBodyHooks;

typedef int32_t (*AeZako2CallHook)(void *, AeRam *, uint32_t, uint32_t,
                                   uint32_t, uint32_t);
typedef int32_t (*AeZako2RenderHook)(void *, AeRam *, uint32_t, uint32_t,
                                     uint32_t, uint32_t, uint32_t, uint32_t);

/* Functions implemented outside this batch cross an explicit host/native boundary here. */
typedef struct AeZako2Hooks {
    void *user;
    const AeBodyHooks *body_hooks;
    const AeWorldHooks *world_hooks;
    const AeSpawnHooks *spawn_hooks;
    const AeItemsInitHooks *items_hooks;
    AeZako2CallHook PlaySfx;                 /* 800877F8 */
    AeZako2CallHook DrawShadow;              /* 8001F604 */
    AeZako2CallHook ModelTransformRigid;    /* 800161C0 */
    AeZako2CallHook ModelDraw;               /* 80017CD4 */
    AeZako2CallHook FUN_8013f660;             /* runtime overlay body is not in the EXE */
    AeZako2RenderHook DrawProjectileSprite;  /* native renderer replaces inline GTE/GPU block */
    AeZako2RenderHook DrawDebugGlyph;        /* native renderer replaces GPU packet construction */
    AeZako2RenderHook FinishDebugText;       /* native renderer replaces final GPU packet construction */
} AeZako2Hooks;

/* Original globals and fixed record layouts. */
#define AE_G_ZAKO2_PROJECTILES              0x800B8D30u
#define AE_ZAKO2_PROJECTILE_COUNT           16u
#define AE_ZAKO2_PROJECTILE_SIZE             0x30u
#define AE_ZAKO2_PROJECTILE_BODY             0x00u
#define AE_ZAKO2_PROJECTILE_OWNER            0x04u
#define AE_ZAKO2_PROJECTILE_SOURCE            0x08u
#define AE_ZAKO2_PROJECTILE_KIND             0x0Cu
#define AE_ZAKO2_PROJECTILE_FIELD_01         0x01u
#define AE_ZAKO2_PROJECTILE_FIELD_02         0x02u
#define AE_ZAKO2_PROJECTILE_FIELD_03         0x03u
#define AE_ZAKO2_PROJECTILE_FIELD_05         0x05u
#define AE_ZAKO2_PROJECTILE_FIELD_06         0x06u
#define AE_ZAKO2_PROJECTILE_FIELD_07         0x07u
#define AE_ZAKO2_PROJECTILE_FIELD_08         0x08u
#define AE_ZAKO2_PROJECTILE_FIELD_09         0x09u
#define AE_ZAKO2_PROJECTILE_FIELD_0A         0x0Au
#define AE_ZAKO2_PROJECTILE_FIELD_0B         0x0Bu
#define AE_ZAKO2_PROJECTILE_TIMER_BYTE       0x0Du
#define AE_ZAKO2_PROJECTILE_COOLDOWN         0x0Eu
#define AE_ZAKO2_PROJECTILE_FIELD_0F         0x0Fu
#define AE_ZAKO2_PROJECTILE_MODEL_INDEX      0x10u
#define AE_ZAKO2_PROJECTILE_STATE            0x11u
#define AE_ZAKO2_PROJECTILE_VARIANT          0x12u
#define AE_ZAKO2_PROJECTILE_AGE               0x13u
#define AE_ZAKO2_PROJECTILE_FIELD_14         0x14u
#define AE_ZAKO2_PROJECTILE_FIELD_15         0x15u
#define AE_ZAKO2_PROJECTILE_FIELD_16         0x16u
#define AE_ZAKO2_PROJECTILE_FIELD_17         0x17u
#define AE_ZAKO2_PROJECTILE_FIELD_18         0x18u
#define AE_ZAKO2_PROJECTILE_FIELD_1A         0x1Au
#define AE_ZAKO2_PROJECTILE_POS_X            0x1Cu
#define AE_ZAKO2_PROJECTILE_POS_Y            0x1Eu
#define AE_ZAKO2_PROJECTILE_POS_Z            0x20u
#define AE_ZAKO2_PROJECTILE_RADIUS           0x22u
#define AE_ZAKO2_PROJECTILE_HITBOX_X         0x24u
#define AE_ZAKO2_PROJECTILE_HITBOX_Y         0x26u
#define AE_ZAKO2_PROJECTILE_HITBOX_Z         0x28u
#define AE_ZAKO2_PROJECTILE_GROUND_Y         0x2Au
#define AE_ZAKO2_PROJECTILE_TIMER            0x2Eu
#define AE_ZAKO2_PROJECTILE_VEL_X            0x24u
#define AE_ZAKO2_PROJECTILE_VEL_Y            0x26u
#define AE_ZAKO2_PROJECTILE_VEL_Z            0x28u
#define AE_ZAKO2_BODY_POS_X                  0x00u
#define AE_ZAKO2_BODY_POS_Y                  0x04u
#define AE_ZAKO2_BODY_POS_Z                  0x08u
#define AE_ZAKO2_BODY_RADIUS                 0x20u
#define AE_ZAKO2_BODY_PARAM                  0x24u
#define AE_ZAKO2_BODY_FLAGS                  0x26u
#define AE_G_ZAKO2_PLAYER                    0x800EC1E0u
#define AE_ZAKO2_PLAYER_POS_X                0x24u
#define AE_ZAKO2_PLAYER_POS_Y                0x28u
#define AE_ZAKO2_PLAYER_POS_Z                0x2Cu
#define AE_ZAKO2_PLAYER_CONTACT              0x2Cu
#define AE_ZAKO2_PLAYER_TARGET_X             0x24u
#define AE_ZAKO2_PLAYER_TARGET_Y             0x28u
#define AE_ZAKO2_PLAYER_TARGET_Z             0x2Cu
#define AE_ZAKO2_PLAYER_VECTOR_X             0x30u
#define AE_ZAKO2_PLAYER_VECTOR_Y             0x34u
#define AE_ZAKO2_PLAYER_VECTOR_Z             0x38u
#define AE_G_ZAKO2_RANDOM_SEED               0x800AE8F0u
#define AE_G_ZAKO2_PICKUP_RANDOM_A           0x800AE8F0u
#define AE_G_ZAKO2_PICKUP_RANDOM_B           0x800AE8F2u
#define AE_G_ZAKO2_PICKUP_KIND_TABLE         0x800B28B4u
#define AE_G_ZAKO2_PICKUP_WEIGHT_TABLE       0x800B28A2u
#define AE_ZAKO2_PICKUP_WEIGHT_ROW_STRIDE     25u
#define AE_ZAKO2_PICKUP_WEIGHT_ENTRY_STRIDE   5u
#define AE_ZAKO2_PICKUP_WEIGHT_COUNT          5u
#define AE_ZAKO2_PICKUP_RANDOM_FACTOR         5u
#define AE_ZAKO2_PICKUP_PERCENT_DIVISOR       64u
#define AE_ZAKO2_PICKUP_PERCENT_SHIFT          6u
#define AE_ZAKO2_PICKUP_TIER_THRESHOLD        10u
#define AE_G_ZAKO2_PICKUP_STATE              0x800B28B0u
#define AE_ZAKO2_PICKUP_STATE_ACTIVE_OFFSET   0x00u
#define AE_ZAKO2_PICKUP_STATE_GATE_OFFSET     0x01u
#define AE_G_ZAKO2_PICKUP_DIFFICULTY          0x800BF2F8u
#define AE_G_ZAKO2_GADGET_UNLOCK_MASK        0x800F51C4u
#define AE_G_ZAKO2_COLLECTED_PICKUP_COUNT    0x800F448Cu
#define AE_G_ZAKO2_PICKUP_HEALTH_WEIGHT      0x800EC2C8u
#define AE_G_ZAKO2_STAGE_STATE_BASE          0x800EC350u
#define AE_ZAKO2_STAGE_STATE_OFFSET          0x8000u
#define AE_ZAKO2_STAGE_RENDER_FLAGS_OFFSET   0x100u
#define AE_ZAKO2_STAGE_MODE_OFFSET           0x120u
#define AE_ZAKO2_STAGE_AREA_KIND_OFFSET      0x122u
#define AE_ZAKO2_STAGE_WALL_PROBE_OFFSET     0x296u
#define AE_G_ZAKO2_STAGE_STATE               (AE_G_ZAKO2_STAGE_STATE_BASE + AE_ZAKO2_STAGE_STATE_OFFSET)
#define AE_ZAKO2_STAGE_RENDER_FLAGS          (AE_G_ZAKO2_STAGE_STATE + AE_ZAKO2_STAGE_RENDER_FLAGS_OFFSET)
#define AE_ZAKO2_STAGE_MODE                  (AE_G_ZAKO2_STAGE_STATE + AE_ZAKO2_STAGE_MODE_OFFSET)
#define AE_ZAKO2_STAGE_AREA_KIND             (AE_G_ZAKO2_STAGE_STATE + AE_ZAKO2_STAGE_AREA_KIND_OFFSET)
#define AE_ZAKO2_COLLISION_WALL_PROBE        (AE_G_ZAKO2_STAGE_STATE + AE_ZAKO2_STAGE_WALL_PROBE_OFFSET)
#define AE_ZAKO2_RENDER_CACHE_FIELD          0x296u
#define AE_G_ZAKO2_GTE_MATRIX                0x800E78C0u
#define AE_G_ZAKO2_SCREEN_VERTICES           0x800E7D00u
#define AE_G_ZAKO2_PROJECTILE_MODEL_TABLE    0x800F4C30u
#define AE_ZAKO2_PROJECTILE_MODEL_STRIDE     0x10u
#define AE_ZAKO2_MODEL_DRAW_CONTEXT_SIZE     0x4078u
#define AE_ZAKO2_VECTOR_Y_OFFSET             0x02u
#define AE_ZAKO2_VECTOR_Z_OFFSET             0x04u
#define AE_ZAKO2_VECTOR_W_OFFSET             0x0Cu
#define AE_ZAKO2_STACK_VECTOR_X_OFFSET       0x00u
#define AE_ZAKO2_STACK_VECTOR_Y_OFFSET       0x04u
#define AE_ZAKO2_STACK_VECTOR_Z_OFFSET       0x08u
#define AE_ZAKO2_STACK_VECTOR_W_OFFSET       0x0Cu
#define AE_ZAKO2_TEMP_VECTOR_OUTPUT_OFFSET   0x10u
#define AE_ZAKO2_VECTOR_COMPONENT_X_OFFSET   0x00u
#define AE_ZAKO2_VECTOR_COMPONENT_Y_OFFSET   0x02u
#define AE_ZAKO2_VECTOR_COMPONENT_Z_OFFSET   0x04u
#define AE_G_ZAKO2_RENDER_STATE              0x800EC350u
#define AE_ZAKO2_RENDER_OT_INDEX_OFFSET      0x0Cu
#define AE_ZAKO2_RENDER_MODEL_OT_OFFSET      0x80u
#define AE_ZAKO2_RENDER_PRIMITIVE_PTR_OFFSET 0x04u
#define AE_ZAKO2_RENDER_OT_INDEX             0x0Cu
#define AE_ZAKO2_RENDER_MODEL_OT             0x80u
#define AE_ZAKO2_RENDER_PRIMITIVE_PTR        0x04u
#define AE_G_ZAKO2_DEBUG_FADE                0x800DF7D8u
#define AE_ZAKO2_DEBUG_COLOR_OFFSET          0x00u
#define AE_ZAKO2_DEBUG_FADE_LEVEL_OFFSET     0x04u
#define AE_ZAKO2_DEBUG_CURSOR_X_OFFSET       0x08u
#define AE_ZAKO2_DEBUG_CURSOR_Y_OFFSET       0x0Cu
#define AE_ZAKO2_DEBUG_COLOR                 0x00u
#define AE_ZAKO2_DEBUG_FADE_LEVEL            0x04u
#define AE_ZAKO2_DEBUG_CURSOR_X              0x08u
#define AE_ZAKO2_DEBUG_CURSOR_Y              0x0Cu
#define AE_G_ZAKO2_DEBUG_TEXT_ENABLED        0x800F460Au
#define AE_G_ZAKO2_DEBUG_TEXT_MAX_WIDTH      0x800DF800u
#define AE_G_ZAKO2_DEBUG_PRIMITIVE_COUNT     0x800DF7FCu
#define AE_G_ZAKO2_DEBUG_DRAW_COLOR          0x800DF7F8u
#define AE_G_ZAKO2_DEBUG_OT                  0x800F04D0u
#define AE_G_ZAKO2_DEBUG_FINAL_OT            0x800F03D0u
#define AE_G_ZAKO2_DEBUG_FONT_GLYPH_TABLE    0x800B3390u
#define AE_ZAKO2_DEBUG_FONT_GLYPH_STRIDE     0x02u
#define AE_ZAKO2_DEBUG_FONT_GLYPH_WIDTH_OFFSET 0x01u
#define AE_ZAKO2_VARARG_FIRST_STACK_OFFSET   0x04u
#define AE_ZAKO2_VARARG_WORD_SIZE            0x04u
#define AE_ZAKO2_GLYPH_PACKET_SIZE_A          0x14u
#define AE_ZAKO2_GLYPH_PACKET_SIZE_B        0x28u
#define AE_ZAKO2_TEXT_FINAL_PACKET_SIZE      0x08u
#define AE_ZAKO2_TEMP_VECTOR                 0x801FFF00u
#define AE_ZAKO2_TEMP_MATRIX                 0x801FFF20u
#define AE_ZAKO2_TEMP_SCREEN_XY              0x801FFF40u
#define AE_ZAKO2_TEMP_SCREEN_DEPTH           0x801FFF48u
#define AE_ZAKO2_TEMP_SCREEN_FLAGS           0x801FFF4Cu
#define AE_ZAKO2_UPDATE_RENDER_FLAG_MASK     0x06u
#define AE_ZAKO2_BODY_CONTACT                0x2Cu
#define AE_ZAKO2_BODY_CONTACT_KIND_SHIFT     28u
#define AE_ZAKO2_PROJECTILE_STATE_AGE_LIMIT  0x04u
#define AE_ZAKO2_PROJECTILE_LIFETIME         0x5Au
#define AE_ZAKO2_PROJECTILE_PHASE_LIMIT      60u
#define AE_ZAKO2_PROJECTILE_PLAYER_RANGE2    6400u
#define AE_ZAKO2_PROJECTILE_NEAR_RANGE2      0x4000u
#define AE_ZAKO2_PROJECTILE_HIT_RANGE2       0x960Fu
#define AE_ZAKO2_PROJECTILE_CONTACT_KIND      6u
#define AE_ZAKO2_PROJECTILE_CONTACT_MASK      0xFu
#define AE_ZAKO2_PROJECTILE_AIRBORNE_LIMIT    91u
#define AE_ZAKO2_PROJECTILE_TYPE5_LIFETIME    90u
#define AE_ZAKO2_PROJECTILE_TYPE7_BOUNCE_TIME 1024u
#define AE_ZAKO2_PROJECTILE_TYPE89_BOUNCE_TIME 1024u
#define AE_ZAKO2_PROJECTILE_GRAVITY_TYPE5     6
#define AE_ZAKO2_PROJECTILE_GRAVITY_TYPE7     4
#define AE_ZAKO2_PROJECTILE_GRAVITY_TYPE89    4
#define AE_ZAKO2_PROJECTILE_TERMINAL_TYPE7    384
#define AE_ZAKO2_PROJECTILE_BOUNCE_TYPE7      (-20)
#define AE_ZAKO2_PROJECTILE_BOUNCE_TYPE89     (-20)
#define AE_ZAKO2_PROJECTILE_HIT_BOUNCE_Y      (-22)
#define AE_ZAKO2_PROJECTILE_VECTOR_DAMPING    475
#define AE_ZAKO2_PROJECTILE_VECTOR_DAMPING_SHIFT 9u
#define AE_ZAKO2_PROJECTILE_PLAYER_BODY_RANGE 0x4000u
#define AE_ZAKO2_PROJECTILE_NEAR_PLAYER_RANGE 128u
#define AE_ZAKO2_PROJECTILE_TYPE5_HIT_RANGE   6400u
#define AE_ZAKO2_PROJECTILE_TYPE5_Y_TOLERANCE 340u
#define AE_ZAKO2_PROJECTILE_DEFAULT_SOUND      77
#define AE_ZAKO2_PROJECTILE_TYPE5_BODY_RADIUS 200
#define AE_ZAKO2_PROJECTILE_TYPE5_BODY_PARAM  0
#define AE_ZAKO2_PROJECTILE_TYPE5_BODY_FLAGS  258
#define AE_ZAKO2_PROJECTILE_TYPE5_HIT_RADIUS  128
#define AE_ZAKO2_PROJECTILE_TYPE5_HIT_PARAM   1536
#define AE_ZAKO2_PROJECTILE_TYPE5_HIT_FLAGS   0
#define AE_ZAKO2_PROJECTILE_TYPE34_BODY_RADIUS 200
#define AE_ZAKO2_PROJECTILE_TYPE3_BODY_PARAM  0
#define AE_ZAKO2_PROJECTILE_TYPE3_BODY_FLAGS  258
#define AE_ZAKO2_PROJECTILE_TYPE4_BODY_PARAM  1
#define AE_ZAKO2_PROJECTILE_TYPE4_BODY_FLAGS  513
#define AE_ZAKO2_PROJECTILE_TYPE6_BODY_RADIUS 40
#define AE_ZAKO2_PROJECTILE_TYPE6_BODY_PARAM  1
#define AE_ZAKO2_PROJECTILE_TYPE6_BODY_FLAGS  512
#define AE_ZAKO2_PROJECTILE_TYPE7_BODY_RADIUS 32
#define AE_ZAKO2_PROJECTILE_TYPE7_BODY_PARAM  1
#define AE_ZAKO2_PROJECTILE_TYPE7_BODY_FLAGS  0
#define AE_ZAKO2_PROJECTILE_TYPE89_BODY_RADIUS 80
#define AE_ZAKO2_PROJECTILE_TYPE89_BODY_PARAM 0
#define AE_ZAKO2_PROJECTILE_TYPE89_BODY_FLAGS 0
#define AE_ZAKO2_PARTICLE_SMALL                26
#define AE_ZAKO2_PARTICLE_BURST                 258
#define AE_ZAKO2_PARTICLE_SPARK                  4
#define AE_ZAKO2_PARTICLE_BOUNCE                 5
#define AE_ZAKO2_PARTICLE_TRAIL                267
#define AE_ZAKO2_PARTICLE_LARGE               1024
#define AE_ZAKO2_PLAYER_BODY_CONTACT            0x2Cu
#define AE_ZAKO2_PROJECTILE_PLAYER_DISTANCE_X  0x24u
#define AE_ZAKO2_PROJECTILE_PLAYER_DISTANCE_Y  0x28u
#define AE_ZAKO2_PROJECTILE_PLAYER_DISTANCE_Z  0x2Cu
#define AE_ZAKO2_PICKUP_LCG_MULTIPLIER         5u
#define AE_ZAKO2_PICKUP_LCG_INCREMENT          1u
#define AE_ZAKO2_TEMP_VECTOR_SIZE              0x10u
#define AE_ZAKO2_PROJECTILE_TYPE5_GROUND_MARGIN 64
#define AE_ZAKO2_PROJECTILE_TYPE7_EXTRA_GRAVITY 8

void ae_ProjectilesUpdateDraw(AeRam *r, const AeZako2Hooks *h); /* 80059B08 */
void ae_ProjectileAddHitBody(AeRam *r, const AeZako2Hooks *h, uint32_t projectile,
                             int32_t dx, int32_t dy, int32_t dz); /* 80059A54 */
void ae_ProjectilesDraw(AeRam *r, const AeZako2Hooks *h); /* 80059B54 */
void ae_ProjectilesUpdate(AeRam *r, const AeZako2Hooks *h); /* 80059FA4 */

int32_t ae_PickupPickKindA(AeRam *r); /* 8008373C */
int32_t ae_PickupPickKindB(AeRam *r); /* 80083790 */
void ae_SpawnPickups(AeRam *r, const AeZako2Hooks *h, uint32_t kind,
                     uint32_t position, int32_t mode, uint32_t choose_kind); /* 80083AA8 */
void ae_ColorFadeSet(AeRam *r, int32_t color, int32_t level); /* 8008C3C0 */
void ae_DebugTextDrawGlyph(AeRam *r, const AeZako2Hooks *h, int32_t glyph); /* 8008C434 */
void ae_DebugTextPrintf(AeRam *r, const AeZako2Hooks *h, uint32_t format,
                        uint32_t argument_frame); /* 8008C6F0 */
void ae_DebugTextPrintfWhite(AeRam *r, const AeZako2Hooks *h, uint32_t format,
                             uint32_t argument_frame); /* 8008CBFC */
int32_t ae_IsTimeAttack(AeRam *r); /* 80097678 */
void ae_FUN_8013f660(AeRam *r, const AeZako2Hooks *h, uint32_t object); /* 8013F660 overlay shim */

#endif
