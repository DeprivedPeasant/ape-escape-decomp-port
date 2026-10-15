#ifndef AE_WORLD_H
#define AE_WORLD_H
#include "../core/ae_ram.h"

/* Original argument words; RAM addresses, including scratchpad, remain PSX addresses.
 * Hooks must implement all original side effects. Missing hooks set the sticky error. */
typedef int32_t (*AeWorldHookFn)(void *, AeRam *, uint32_t, uint32_t, uint32_t, uint32_t);
typedef struct AeWorldHooks {
    void *user;
    AeWorldHookFn GetSolidHeight;          /* 8004D8C0: collision grid/box ground query */
    AeWorldHookFn PlaySfxAt;               /* 80087A20: positional sound */
    AeWorldHookFn GimmickActivate;         /* 8008FC30: recursive linked activation */
    AeWorldHookFn MeshInitFromAreaTable;   /* 80015CC8: replace prop mesh */
    AeWorldHookFn FUN_8013d358;            /* ovl_197: effect/particle burst on smash */
    AeWorldHookFn CameraShake;             /* 80048A00 */
    AeWorldHookFn EventCamStart;           /* 8004D39C */
    /* Original no-heightfield path can copy two uninitialized stack halfwords
     * to scratchpad if GetSolidHeight returns > 16384. Host supplies these. */
    uint16_t ground_stale_x, ground_stale_z;
} AeWorldHooks;

void ae_ClearWords(AeRam *r, uint32_t ptr, int32_t words);        /* 80032708 */
int32_t ae_StageExitIsActive(AeRam *r);                         /* 8005DCE4 */
int32_t ae_TargetListAdd(AeRam *r, uint32_t ptr);                /* 8007CD88 */
void ae_TargetListRemove(AeRam *r, uint32_t ptr);                /* 8007CDC4 */
int32_t ae_ObjSlotGetAnimIndex(AeRam *r, int32_t slot);          /* 80052C4C */
void ae_ObjSlotSetField_CF(AeRam *r, int32_t slot, uint32_t value); /* 80052CA0 */
int32_t ae_ObjSlotGetTarget(AeRam *r, uint32_t slot, uint32_t out); /* 80052D04 */
void ae_SetDAT_800E77E2(AeRam *r, uint32_t value);                /* 80052CEC */
int32_t ae_GimmickGetInfo(AeRam *r, int32_t index, uint32_t out); /* 8005B7BC */
int32_t ae_GimmickEvent(AeRam *r, const AeWorldHooks *h, uint32_t event, int32_t index, uint32_t arg); /* 8005B880 */
int32_t ae_GetGroundHeight(AeRam *r, const AeWorldHooks *h, int32_t x, int32_t y, int32_t z); /* 80043400 */
int32_t ae_GetGroundHeightEx(AeRam *r, const AeWorldHooks *h, int32_t x, int32_t y, int32_t z, uint32_t out); /* 800433B0 */
#endif
