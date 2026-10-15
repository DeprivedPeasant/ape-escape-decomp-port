#ifndef AE_ZAKOAI_H
#define AE_ZAKOAI_H

#include "ae_zakohelpers.h"

/* Overlay enemy callbacks remain outside the EXE port. */
typedef int32_t (*AeZakoAiHookFn)(void *, AeRam *, uint32_t, uint32_t,
                                  uint32_t, uint32_t, uint32_t, uint32_t,
                                  uint32_t);

typedef struct AeZakoAiHooks {
    void *user;
    const AeZako1Hooks *zako1_hooks;

    AeZakoAiHookFn FUN_8013DF54;                 /* overlay function */
    AeZakoAiHookFn FUN_80140880;                 /* overlay function */
    AeZakoAiHookFn FUN_8013F504;                 /* overlay function */
    AeZakoAiHookFn FUN_8014116C;                 /* overlay function */
} AeZakoAiHooks;

/* Tables and globals used by the original dispatchers. */
#define AE_G_ZAKOAI_STATE_HANDLER_TABLE  0x800A4F80u
#define AE_G_ZAKOAI_ACCEL_ADJUST         0x800E77ECu
#define AE_G_ZAKOAI_TYPE_HANDLER_TABLE  0x800AF350u
#define AE_G_ZAKOAI_AREA_MODE_TABLE     0x800AF574u
#define AE_G_ZAKOAI_STAGE_STATE         0x800F4350u

/* Object fields used directly by the generic enemy update. */
#define AE_ZAKOAI_OBJ_HIT_POS_X         0x01Cu
#define AE_ZAKOAI_OBJ_HIT_POS_Y         0x01Eu
#define AE_ZAKOAI_OBJ_HIT_POS_Z         0x020u
#define AE_ZAKOAI_OBJ_ACTION_FLAGS      0x0E2u
#define AE_ZAKOAI_OBJ_FLAG_LINKED_HIT_STATE 0x00000001u
#define AE_ZAKOAI_ACTION_FLAG_0         0x0001u
#define AE_ZAKOAI_FLAGS_CLEAR_AFTER_BOUNCE 0x00010002u
#define AE_ZAKOAI_HIT_BODY_KIND         0x5Fu
#define AE_ZAKOAI_HIT_BODY_RADIUS        514u
#define AE_ZAKOAI_STATE_IDLE_WATCH      0x01u
#define AE_ZAKOAI_STATE_FALL_TO_GROUND  0x0Au
#define AE_ZAKOAI_STATE_HIT_BODY_BOUNCE 0x0Bu
#define AE_ZAKOAI_STATE_POST_BOUNCE     0x0Cu
#define AE_ZAKOAI_TYPE_STAGE_SPECIFIC   0x28u
#define AE_ZAKOAI_TYPE_HIGH_FALL_CHECK  0x49u
#define AE_ZAKOAI_STAGE_FALL_MARGIN     614

void ae_ZakoUpdateGenericEnemy(AeRam *r, const AeZakoAiHooks *h,
                               uint32_t obj); /* 80053550, 0x464 bytes */
void ae_ZakoUpdateStageSpecificEnemy(AeRam *r, const AeZakoAiHooks *h,
                                     uint32_t obj); /* 800546DC, 0xA4 bytes */
void ae_ZakoAreaModeNoop(AeRam *r); /* 80057680, 8 bytes */

#endif
