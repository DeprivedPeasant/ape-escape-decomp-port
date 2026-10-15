#ifndef AE_ZAKOHELPERS_H
#define AE_ZAKOHELPERS_H

#include "ae_zako1.h"

/* Globals used by the shared enemy movement and state helpers. */
#define AE_G_ZAKO_HELPER_CHASE_ACCEL      0x800E77EAu
#define AE_G_ZAKO_HELPER_WATCH_RADIUS     0x800E77EBu
#define AE_G_ZAKO_HELPER_ACCEL_STEP       0x800E77ECu
#define AE_G_ZAKO_HELPER_COOLDOWN          0x800E77EDu
#define AE_G_ZAKO_HELPER_TURN_ADJUST       0x800E77F1u
#define AE_G_ZAKO_HELPER_WANDER_SEED       0x800AE8F0u
#define AE_G_ZAKO_HELPER_WANDER_SEED_2     0x800AE8F2u

/* Additional object fields used by this batch. Existing AE_OBJ_* offsets are
 * declared in ae_zako_init.h and ae_zako1.h. */
#define AE_ZAKO_HELPER_ACTION_FLAGS         0x0E2u
#define AE_ZAKO_HELPER_FLAG_UNK_5           0x00000020u
#define AE_ZAKO_HELPER_FLAG_UNK_10          0x00000400u
#define AE_ZAKO_HELPER_FLAG_UNK_16          0x00010000u
#define AE_ZAKO_HELPER_FLAG_UNK_17          0x00020000u
#define AE_ZAKO_HELPER_FLAG_NO_Y_CHECK      0x00000004u
#define AE_ZAKO_HELPER_CONE_ANGLE           0x0A2u

/* Scratch vector and hit-body fields. */
#define AE_ZAKO_HELPER_VECTOR_X             0x00u
#define AE_ZAKO_HELPER_VECTOR_Y             0x02u
#define AE_ZAKO_HELPER_VECTOR_Z             0x04u
#define AE_ZAKO_HELPER_VECTOR_PAD           0x06u
#define AE_ZAKO_HELPER_HIT_BODY_POSITION_X  0x00u
#define AE_ZAKO_HELPER_HIT_BODY_POSITION_Y  0x04u
#define AE_ZAKO_HELPER_HIT_BODY_POSITION_Z  0x08u
#define AE_ZAKO_HELPER_HIT_BODY_DIRECTION   0x18u
#define AE_ZAKO_HELPER_HIT_BODY_KIND         0x20u
#define AE_ZAKO_HELPER_HIT_BODY_FLAGS        0x22u
#define AE_ZAKO_HELPER_HIT_BODY_DAMAGE       0x24u
#define AE_ZAKO_HELPER_HIT_BODY_RADIUS       0x26u

/* AeRam addresses standing in for the original functions' stack vectors.
 * These are inside the checker stack window; the position/copy fields are
 * still explicitly written where the original code writes them. */
#define AE_ZAKO_HELPER_STACK_CLEAR          0x801FFEE8u
#define AE_ZAKO_HELPER_STACK_MOVE_VECTOR    0x801FFEE0u
#define AE_ZAKO_HELPER_STACK_CONE_VECTOR    0x801FFED8u
#define AE_ZAKO_HELPER_STACK_PARTICLE_POS   0x801FFEF0u
#define AE_ZAKO_HELPER_PARTICLE_FUNCTION    0x80057504u

void ae_ZakoWalkToTarget(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 80052D70 */
void ae_ZakoStateHurt(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 80052DFC */
void ae_ZakoEnterIdleWait(AeRam *r, uint32_t obj); /* 80052ED4 */
void ae_ObjFootstepFx(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 80052F28 */
void ae_ZakoChasePlayer(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 80052FE8 */
void ae_ZakoEnterDeath(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 800530D0 */
void ae_ObjTurnTowardsPlayerXZ(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 80053154 */
void ae_ObjSetSpeed96(AeRam *r, uint32_t obj, int32_t speed); /* 8005325C */
void ae_ZakoStateWatch(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 80053264 */
void ae_Zako_WanderBounce(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 80053304 */
void ae_Zako_InitDefaultStats(AeRam *r, uint32_t obj); /* 8005337C */
void ae_Zako_RecoverFromStun(AeRam *r, uint32_t obj); /* 80053440 */
void ae_ZakoReturnIfFar(AeRam *r, uint32_t obj); /* 800534E8 */

int32_t ae_ObjTargetBeyondRange(AeRam *r, uint32_t obj, int32_t extra_range); /* 80055370 */
void ae_ObjWalkOnGround(AeRam *r, const AeZako1Hooks *h, uint32_t obj); /* 80055460 */
void ae_ObjSetVelAlongDir(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                          uint32_t direction, int32_t speed,
                          int32_t turn_speed); /* 80055524 */
void ae_ObjSetVelTowardPlayer(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                              int32_t speed); /* 800555D8 */
void ae_ObjSetVelTowardTarget(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                              int32_t speed, int32_t turn_speed); /* 80055674 */
int32_t ae_ObjCooldown(AeRam *r, uint32_t obj, int32_t cooldown); /* 8005588C */
int32_t ae_ObjWalkToPoint(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                          int32_t target_x, int32_t target_z,
                          int32_t arrival_radius); /* 80055AA0 */
void ae_ObjChasePlayer(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                       int32_t speed); /* 80055C00 */
void ae_ObjWanderStep(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                      int32_t speed, int32_t turn_speed); /* 80055D28 */
int32_t ae_ObjWanderOrReturn(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                             int32_t speed, int32_t turn_speed); /* 80055DC0 */
void ae_ObjFleeFromPlayerInArea(AeRam *r, const AeZako1Hooks *h,
                                uint32_t obj, int32_t speed); /* 80056128 */
void ae_ObjSetVelFromFacing(AeRam *r, uint32_t obj, int32_t speed); /* 80056200 */
int32_t ae_ObjTryStepOnGround(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                              int32_t step_down); /* 80056344 */
int32_t ae_ObjFallToGround(AeRam *r, const AeZako1Hooks *h,
                           uint32_t obj); /* 8005648C */
int32_t ae_ObjPlayerOutOfYRange(AeRam *r, uint32_t obj, int32_t y); /* 800564EC */
int32_t ae_ObjPlayerNearPointXZ(AeRam *r, uint32_t obj, int32_t x,
                                int32_t y, int32_t z,
                                int32_t radius); /* 8005654C */
int32_t ae_ObjPlayerInCone(AeRam *r, uint32_t obj, int32_t cone_angle,
                           int32_t dot_threshold, int32_t offset_x,
                           int32_t offset_z); /* 800565C4 */
int32_t ae_ObjPlayerInConeFromPos2(AeRam *r, uint32_t obj,
                                   int32_t cone_angle,
                                   int32_t dot_threshold,
                                   int32_t vertical_radius); /* 800567B8 */
void ae_ObjTurnToPlayer(AeRam *r, const AeZako1Hooks *h,
                        uint32_t obj); /* 8005696C */
int32_t ae_ObjAccelerate(AeRam *r, uint32_t obj, int32_t target_speed,
                         int32_t accelerate_step,
                         int32_t decelerate_step); /* 80056DC4 */
void ae_ObjClampFrame(AeRam *r, uint32_t obj, int32_t frame_limit); /* 800570D4 */
int32_t ae_PlayerWithinRadiusXZ(AeRam *r, int32_t x, int32_t z,
                                int32_t radius); /* 800570F4 */
void ae_ObjAimAtPlayer(AeRam *r, uint32_t obj); /* 800571C4 */
int32_t ae_ObjFallAndBounce(AeRam *r, const AeZako1Hooks *h,
                            uint32_t obj); /* 800572CC */
void ae_ObjSpawnParticleAt(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                           int32_t type, int32_t offset_x,
                           int32_t offset_y, int32_t offset_z); /* 80057504 */
void ae_Zako_Nop(AeRam *r, uint32_t obj, int32_t radius); /* 80058134 */
uint32_t ae_ObjAddHitBody(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                          int32_t kind, int32_t x, int32_t y, int32_t z,
                          int32_t damage, int32_t radius); /* 8005813C */
uint32_t ae_ObjAddHitBodyIfPlayerNear(AeRam *r, const AeZako1Hooks *h,
                                      uint32_t obj, int32_t kind,
                                      int32_t x, int32_t y, int32_t z,
                                      int32_t damage, int32_t radius); /* 80058220 */
int32_t ae_ObjGetLinkedObjState(AeRam *r, uint32_t obj,
                                int32_t link_index); /* 800582EC */

#endif
