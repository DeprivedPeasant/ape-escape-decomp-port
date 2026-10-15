#ifndef AE_ZAKO1_H
#define AE_ZAKO1_H

#include "ae_zako_init.h"
#include "../bodies/ae_bodies.h"
#include "../effects/ae_spawn.h"
#include "../sound/ae_sound.h"
#include "../core/ae_pose.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "ae_world.h"

/* Calls outside the direct EXE ports and already-ported helpers.  Values are
 * the original MIPS argument words; pointer arguments remain AeRam addresses. */
typedef int32_t (*AeZako1HookFn)(void *, AeRam *, uint32_t, uint32_t,
                                 uint32_t, uint32_t);
typedef int32_t (*AeZako1DispatchFn)(void *, AeRam *, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t);

typedef struct AeZako1Hooks {
    void *user;
    const AeZakoHooks *init_hooks;
    const AeBodyHooks *body_hooks;
    const AeWorldHooks *world_hooks;
    const AeSpawnHooks *spawn_hooks;
    const AeSoundHw *sound_hw;

    AeZako1HookFn ProjectilesUpdateDraw;    /* 80059B08 */
    AeZako1HookFn Draw_Nop;                 /* 8005E000 */
    AeZako1HookFn FUN_8002FB00;             /* 8002FB00 */
    AeZako1HookFn FUN_8002FB30;             /* 8002FB30 */
    AeZako1HookFn FUN_80031330;             /* 80031330 */
    AeZako1HookFn Atan2Safe;                /* 80046340 */
    AeZako1HookFn FUN_800A3740;             /* 800A3740: BIOS memset wrapper */
    AeZako1HookFn SpawnPickups;              /* 80083AA8 */
    AeZako1HookFn FUN_8013F660;              /* 8013F660: overlay type-0x3F handler */
    AeZako1DispatchFn ZakoTypeUpdate;        /* indirect through 800AF350 */
    AeZako1DispatchFn AreaModeUpdate;        /* indirect through 800AF574 */
    AeZako1DispatchFn ZakoFrameHook;         /* indirect from 800B8D20 */
    AeZako1HookFn DrawShadow;                /* 8001F604 */
    AeZako1HookFn ModelTransformSkinned;     /* 80016A60: native renderer boundary */
    AeZako1HookFn ModelPrepCycleColors;      /* 8001D4A8 */
    AeZako1HookFn ModelEmitPrims;            /* 800180B8: native renderer boundary */
    AeZako1HookFn ModelEmitPrimsCycleColors; /* 8001D530: native renderer boundary */
    AeZako1HookFn DebugTextPrintfWhite;      /* 8008CBFC */
    AeZako1HookFn ColorFadeSet;              /* 8008C3C0 */
    AeZako1HookFn ZakoDebugDrawPrimitive;    /* native renderer packet boundary */
} AeZako1Hooks;

/* Global addresses referenced by the per-frame enemy routines. */
#define AE_G_PLAYER_OBJECT             0x800EC1E0u
#define AE_G_ZAKO_MODE_HANDLER_TABLE   0x800AF574u
#define AE_G_ZAKO_TYPE_HANDLER_TABLE   0x800AF350u
#define AE_G_ZAKO_MODEL_BONE_MATRICES  0x800F4730u
#define AE_G_ZAKO_COLLIDER_TABLE_PTR   0x800F455Cu
#define AE_G_ZAKO_GROUND_QUERY_MODE    0x800F45E8u
#define AE_G_ZAKO_PROJECTION_SETTINGS  0x800E78C0u
#define AE_G_ZAKO_SCREEN_VERTICES      0x800E7D00u
#define AE_G_ZAKO_DEBUG_DRAW_COUNT     0x800EC350u
#define AE_G_ZAKO_DEBUG_OT             0x800EC354u
#define AE_G_ZAKO_DEBUG_PACKET_CURSOR  0x800EC35Cu
#define AE_G_ZAKO_DEBUG_DRAW_INDEX     0x800EC35Cu
#define AE_G_ZAKO_DEBUG_CONTEXT_BASE   0x800EC3D0u
#define AE_G_ZAKO_DEBUG_FRAME_STATE    0x800F4470u
#define AE_G_ZAKO_DEBUG_ENTRY_LABELS   0x800A5308u
#define AE_G_ZAKO_DEBUG_STATUS_LABELS  0x800A52F8u
#define AE_G_ZAKO_NO_HIT_DAMAGE        0x800E77E8u
#define AE_G_ZAKO_DEFENSE_SCALE        0x800E77EFu
#define AE_G_ZAKO_DRAW_SCALE_CONTROL   0x800E77EEu
#define AE_G_ZAKO_MOTION_TABLE         0x800AF330u
#define AE_G_ZAKO_DROP_TABLE           0x800AF300u
#define AE_G_ZAKO_FRAME_STATE          0x800F4450u
#define AE_G_ZAKO_FRAME_GATE           0x800E77E4u
#define AE_G_ZAKO_CURRENT_COLLIDER     0x800F45E0u
#define AE_G_ZAKO_MODEL_DRAW_COUNTER   0x800F435Cu
#define AE_G_ZAKO_FRAME_ENABLE         0x800E77E4u
#define AE_G_ZAKO_AREA_MODE_HOOK_COUNT 0x800E77A5u
#define AE_G_ZAKO_MODE_FLAGS           0x800E77A0u
#define AE_G_ZAKO_COLLIDER_CONFIG      0x800E77A0u
#define AE_G_ZAKO_COLLIDER_CONFIG_X    0x08u
#define AE_G_ZAKO_COLLIDER_CONFIG_Y    0x0Au
#define AE_G_ZAKO_COLLIDER_CONFIG_Z    0x0Cu
#define AE_G_ZAKO_DEBUG_ENABLED       (AE_G_ZAKO_STATE + 0x15u)
#define AE_G_ZAKO_DEBUG_ENTRY_COUNT   (AE_G_ZAKO_STATE + 0x0Cu)
#define AE_G_ZAKO_DEBUG_ENTRY_TABLE   (AE_G_ZAKO_STATE + 0x04u)
#define AE_G_ZAKO_DRAW_CONTEXT        0x800EC350u
#define AE_G_ZAKO_OT_HEAD             0x800EC354u
#define AE_G_ZAKO_OT_PACKET_CURSOR    0x800EC35Cu
#define AE_G_ZAKO_DRAW_COUNT          0x800F435Cu
#define AE_G_ZAKO_PROJECTION_MATRIX   0x800E78C0u
#define AE_G_ZAKO_NEAR_MARGIN         0x800B4D52u
#define AE_G_ZAKO_NEAR_NO_MARGIN      0x800B4D50u
#define AE_G_ZAKO_RENDER_CONTEXT      0x800E7D00u

#define AE_ZAKO_STATE_HIT_MODE         0x19u
#define AE_ZAKO_STATE_HIT_STRENGTH     0x1Au
#define AE_ZAKO_STATE_HIT_ANGLE        0x1Cu
#define AE_ZAKO_STATE_GRAVITY          0x1Du
#define AE_ZAKO_STATE_HIT_SCALE        0x1Fu
#define AE_ZAKO_ENTRY_REMAINING        0x10u
#define AE_ZAKO_ENTRY_ACTIVE           0x16u
#define AE_ZAKO_ENTRY_X                0x02u
#define AE_ZAKO_ENTRY_Z                0x06u
#define AE_ZAKO_ENTRY_SPAWN_RADIUS     0x12u
#define AE_ZAKO_ENTRY_RECORD_SIZE      0x20u
#define AE_ZAKO_ENTRY_MODEL_OFFSET     0x18u
#define AE_ZAKO_ENTRY_ANIMATION_OFFSET 0x1Cu
#define AE_ZAKO_HIT_BODY_DIRECTION     0x18u
#define AE_ZAKO_HIT_BODY_DAMAGE        0x24u
#define AE_ZAKO_HIT_BODY_KIND          0x26u
#define AE_ZAKO_MOTION_RECORD_SIZE     0x04u
#define AE_ZAKO_MOTION_STRENGTH        0x00u
#define AE_ZAKO_MOTION_ANGLE           0x01u
#define AE_ZAKO_MOTION_GRAVITY         0x02u
#define AE_ZAKO_DROP_TABLE_COUNT       4u

#define AE_ZAKO1_OBJ_FLAG_UNK_8        0x00000100u

#define AE_ANIM_CLIP_FIRST_FRAME       0x00u
#define AE_ANIM_CLIP_LAST_FRAME        0x02u
#define AE_GROUND_QUERY_RESULT         0x1F8003FCu

#define AE_G_ZAKO_OBJECT_FRAME_GATE    0x800F4450u
#define AE_G_ZAKO_OBJECT_DRAW_INDEX    0x800F4470u
#define AE_G_ZAKO_MODEL_DRAW_CONTEXT   0x800F4350u
#define AE_G_ZAKO_PACKET_CURSOR        0x800F4354u

/* Fields shared with Obj and its animation / collision data. */
#define AE_OBJ_CACHED_GROUND_Y          0x086u
#define AE_OBJ_COLLIDER_FLOW_ANGLE      0x02Au
#define AE_OBJ_COLLIDER_FLOW_X          0x02Cu
#define AE_OBJ_COLLIDER_FLOW_Z          0x02Eu
#define AE_OBJ_HIT_REACT                0x05Eu
#define AE_OBJ_DEBUG_FLASH_COUNT        0x084u
#define AE_OBJ_FLAGS2                   0x0D4u
#define AE_OBJ_FLAGS3                   0x0D6u
#define AE_OBJ_HIT_EFFECT_COUNT         0x0E3u
#define AE_OBJ_SCALE_COMPONENT_Y        0x0C4u

#define AE_OBJ_DRAW_SCALE               0x0C8u
#define AE_OBJ_DRAW_FLASH_TIMER         0x084u
#define AE_OBJ_DRAW_FLAGS               0x0E0u
#define AE_OBJ_DRAW_FADE_COUNT          0x084u
#define AE_OBJ_DRAW_MODEL_INDEX         0x0CCu

/* Collision record layout used by the object collider table. */
#define AE_ZAKO_COLLIDER_RECORD_SIZE    0x38u
#define AE_ZAKO_COLLIDER_POS_X          0x00u
#define AE_ZAKO_COLLIDER_POS_Y          0x02u
#define AE_ZAKO_COLLIDER_POS_Z          0x04u
#define AE_ZAKO_COLLIDER_HEIGHT_INPUT   0x08u
#define AE_ZAKO_COLLIDER_FLOW_ANGLE     0x2Au
#define AE_ZAKO_COLLIDER_FLOW_X         0x2Cu
#define AE_ZAKO_COLLIDER_FLOW_Z         0x2Eu
#define AE_ZAKO_MATRIX_RECORD_SIZE      0x20u
#define AE_ZAKO_MATRIX_TRANSLATE_X      0x14u
#define AE_ZAKO_MATRIX_TRANSLATE_Y      0x18u
#define AE_ZAKO_MATRIX_TRANSLATE_Z      0x1Cu
#define AE_ZAKO_MODEL_HEADER_VERTICES   0x00u
#define AE_ZAKO_MODEL_HEADER_COLORS     0x04u
#define AE_ZAKO_MODEL_HEADER_PRIMITIVES 0x08u
#define AE_ZAKO_MODEL_HEADER_VERTEX_COUNT 0x06u
#define AE_ZAKO_MODEL_HEADER_COLOR_COUNT  0x08u
#define AE_ZAKO_MODEL_HEADER_PRIM_COUNT   0x0Au
#define AE_ZAKO_MODEL_VERTEX_DATA       0x0Cu
#define AE_ZAKO_MODEL_LIGHT_COLOR_DATA  0x0Cu
#define AE_ZAKO_MODEL_INSTANCE_DRAW_COUNT 0x00u
#define AE_ZAKO_DEBUG_ENTRY_SIZE        0x20u
#define AE_ZAKO_DEBUG_ENTRY_ENABLED     0x16u
#define AE_ZAKO_DEBUG_ENTRY_X           0x10u
#define AE_ZAKO_DEBUG_ENTRY_Z           0x12u
#define AE_ZAKO_DEBUG_ENTRY_RANGE       0x9Au
#define AE_ZAKO_DEBUG_ENTRY_RANGE_ALT   0x9Cu
#define AE_ZAKO_DEBUG_ENTRY_Y           0x9Eu
#define AE_ZAKO_DEBUG_ENTRY_HEIGHT      0xA4u
#define AE_ZAKO_DEBUG_ENTRY_RADIUS      0xA6u
#define AE_ZAKO_DEBUG_ENTRY_WORLD_POS   0x24u
#define AE_ZAKO_DEBUG_ENTRY_FLOW_ANGLE  0xA2u
#define AE_ZAKO_DEBUG_ENTRY_FLOW_ADJUST 0xACu
#define AE_ZAKO_DEBUG_ENTRY_LINK_COUNT  0x8Cu
#define AE_ZAKO_DEBUG_ENTRY_LINKS       0x90u
#define AE_ZAKO_DEBUG_HOOK_COUNT        0x13u
#define AE_ZAKO_DEBUG_TARGET_COUNT      2u
#define AE_ZAKO_DEBUG_PRIMITIVE_SIZE    0x10u
#define AE_ZAKO_DEBUG_VERTEX_TEMP       0x10u
#define AE_ZAKO_DEBUG_PROJECTED_TEMP    0x20u
#define AE_ZAKO_DRAW_CONTEXT_COUNTER    0x0Cu
#define AE_ZAKO_OT_BUFFER_FIRST         0x80u
#define AE_ZAKO_OT_DRAW_STRIDE          0x4078u
#define AE_ZAKO_LINK_POINT_BONE         0x08u
#define AE_ZAKO_LINK_POINT_VERTEX       0x0Au
#define AE_ZAKO_SVECTOR_X               0x00u
#define AE_ZAKO_SVECTOR_Y               0x02u
#define AE_ZAKO_SVECTOR_Z               0x04u
#define AE_ZAKO_SVECTOR_PAD             0x06u
#define AE_ZAKO_VECTOR_X                0x00u
#define AE_ZAKO_VECTOR_Y                0x04u
#define AE_ZAKO_VECTOR_Z                0x08u
#define AE_ZAKO_SCREEN_X                0x00u
#define AE_ZAKO_SCREEN_Y                0x02u
#define AE_ZAKO_PROJECTION_SCREEN       0x08u
#define AE_ZAKO_PROJECTION_FLAGS        0x0Cu

void ae_ZakoSpawnNearby(AeRam *r, const AeZako1Hooks *h); /* 80050B04 */
void ae_ZakoUpdateAll(AeRam *r, const AeZako1Hooks *h); /* 80050C34 */
uint32_t ae_ZakoAllocSlot(AeRam *r); /* 80050CB0 */
void ae_ZakoUpdateLoop(AeRam *r, const AeZako1Hooks *h); /* 800511A4 */
int32_t ae_PointOffscreen(AeRam *r, const AeZako1Hooks *h, uint32_t pos,
                          int32_t y_offset, int32_t use_margin); /* 80051410 */
void ae_ZakoCullAndDraw(AeRam *r, const AeZako1Hooks *h); /* 80051534 */
void ae_ZakoGetBonePoint(AeRam *r, uint32_t obj, int32_t bone, int32_t vertex,
                         uint32_t out); /* 80051620 */
void ae_ZakoUpdateLinkPoints(AeRam *r, uint32_t obj); /* 800516D8 */
void ae_ZakoUpdateColliderPos(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 80051740 */
void ae_ZakoBuildMatrix(AeRam *r, uint32_t obj); /* 800518AC */
void ae_ZakoPoseHidden(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 80051A28 */
void ae_ModelLightColorsToScratch(AeRam *r, const AeZako1Hooks *h,
                                  uint32_t ot, uint32_t vertices,
                                  uint32_t model, int32_t flash); /* 80051AA4 */
void ae_ZakoDraw(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 80051C00 */
void ae_ObjReenableBody(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 80051DCC */
void ae_ZakoDefeat(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 80051F1C */
void ae_ZakoDropPickups(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 8005216C */
void ae_ZakoSetKnockback(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                         int32_t direction, int32_t strength, int32_t angle); /* 800522A8 */
void ae_ZakoApplyHit(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                     uint32_t hitter_body); /* 8005240C */
int32_t ae_ObjApplyDamage(AeRam *r, uint32_t obj, uint32_t attacker_body); /* 800525A0 */
void ae_ZakoOnHitByGadget(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 800525E0 */
int32_t ae_ObjIsOwnerType0B(AeRam *r, uint32_t obj); /* 80052640 */
void ae_ObjAnimClampLastFrame(AeRam *r, uint32_t obj); /* 80052658 */
int32_t ae_ObjBounceOnGround(AeRam *r, uint32_t obj, int32_t ground_y); /* 80052688 */
int32_t ae_ObjApplyVelocity(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 8005278C */
int32_t ae_ObjKnockbackDecay(AeRam *r, uint32_t obj); /* 80052800 */
void ae_ObjDeathSfx(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 80052864 */
void ae_ObjResetMotion(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 800528E0 */
void ae_ObjSwapToReactAnim(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 800529A0 */
int32_t ae_ObjUpdateMotionState(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 80052A24 */
void ae_ObjSyncBodyHitFlags(AeRam *r, uint32_t obj); /* 80052BC8 */
void ae_ObjPlaySfx(AeRam *r, const AeZako1Hooks *h, uint32_t obj, int32_t id); /* 80055404 */
void ae_ObjTurnTowardsXZ(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                         int32_t dx, int32_t dz, int32_t offset, int32_t steps); /* 8005685C */
int32_t ae_ObjGroundHeightCached(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                                 int32_t dx, int32_t dy, int32_t dz); /* 80056C58 */
void ae_ObjPuffAt(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                  int32_t dx, int32_t dy, int32_t dz); /* 800573A0 */
void ae_ZakoHooksRun(AeRam *r, const AeZako1Hooks *h); /* 8005760C */
void ae_ZakoDebugMenuDraw(AeRam *r, const AeZako1Hooks *h); /* 80058954 */

#endif
