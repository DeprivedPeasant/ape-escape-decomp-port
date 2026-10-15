#ifndef AE_AREASET_H
#define AE_AREASET_H

#include "../core/ae_ram.h"

typedef struct AeBodyHooks AeBodyHooks;

/* Calls outside this port use their original names and ABI argument words.
 * A missing callback sets AeRam's error flag. */
typedef int32_t (*AeAreaSetHookFn)(void *, AeRam *, uint32_t, uint32_t,
                                   uint32_t, uint32_t);

typedef struct AeAreaSetHooks {
    void *user;
    const AeBodyHooks *body_hooks;
    AeAreaSetHookFn Printf;             /* PsyQ syscall veneer */
    AeAreaSetHookFn Exit;               /* PsyQ syscall veneer */
    AeAreaSetHookFn ProgressGetByte;    /* 800963AC, not in this batch */
    AeAreaSetHookFn FUN_8013d1f0;
    AeAreaSetHookFn Area44_Boss_SetAnim;
    AeAreaSetHookFn FUN_8013f68c;
    AeAreaSetHookFn FUN_8013f130;
    AeAreaSetHookFn FUN_8013ecf4;
    AeAreaSetHookFn FUN_8013d52c;
    AeAreaSetHookFn FUN_8013db98;
    AeAreaSetHookFn FUN_8013dc00;
    AeAreaSetHookFn FUN_8013d440;
    AeAreaSetHookFn Area46_Boss_SetAnim;
    AeAreaSetHookFn Area46_Boss_ClearFlag;
    AeAreaSetHookFn FUN_8013f544;
    AeAreaSetHookFn FUN_8013e14c;
    AeAreaSetHookFn FUN_8013ec5c;
    AeAreaSetHookFn FUN_8013d8bc;
    AeAreaSetHookFn FUN_8013df28;
    AeAreaSetHookFn FUN_8013d10c;
    AeAreaSetHookFn FUN_8013f4f0;
    AeAreaSetHookFn FUN_8013e8c0;
    AeAreaSetHookFn FUN_8013de14;
    AeAreaSetHookFn FUN_8013e4d8;
    AeAreaSetHookFn FUN_8013d804;
    AeAreaSetHookFn FUN_8013d000;
    AeAreaSetHookFn Area56_Boss_SetCameraObjAnim;
    AeAreaSetHookFn FUN_8013f86c;
    AeAreaSetHookFn FUN_8013e4a8;
    AeAreaSetHookFn FUN_8013d47c;
    AeAreaSetHookFn FUN_8013e070;
    AeAreaSetHookFn FUN_8013e7f8;
    AeAreaSetHookFn FUN_8013f080;
    AeAreaSetHookFn Area57_Boss_SetAnim;
    AeAreaSetHookFn Area57_Boss_ClearFlag;
    AeAreaSetHookFn FUN_8013e020;
    AeAreaSetHookFn FUN_8013d33c;
    AeAreaSetHookFn FUN_801407bc;
    AeAreaSetHookFn FUN_8013d9a8;
    AeAreaSetHookFn FUN_8013ee9c;
} AeAreaSetHooks;

int32_t ae_AreaGetKind(AeRam *r, const AeAreaSetHooks *h, int32_t area_index);
int32_t ae_AreaGetLevel(AeRam *r, const AeAreaSetHooks *h, int32_t area_index);
void ae_ModelInit(AeRam *r, const AeAreaSetHooks *h, uint32_t instance, uint32_t model_header);
void ae_StaticModelInit(AeRam *r, const AeAreaSetHooks *h, uint32_t instance, uint32_t model_header);
void ae_AreaSetZoneBoxes(AeRam *r, const AeAreaSetHooks *h, uint32_t feature);
void ae_ProjectilesReset(AeRam *r, const AeAreaSetHooks *h);
int32_t ae_BossKindForArea(AeRam *r, const AeAreaSetHooks *h);
void ae_BossInit(AeRam *r, const AeAreaSetHooks *h);
void ae_FUN_8013d1ac(AeRam *r, const AeAreaSetHooks *h);
void ae_FUN_8013da10(AeRam *r, const AeAreaSetHooks *h);
void ae_FUN_8013dae8(AeRam *r, const AeAreaSetHooks *h);
void ae_FUN_8013dc68(AeRam *r, const AeAreaSetHooks *h);
void ae_FUN_8013df90(AeRam *r, const AeAreaSetHooks *h);
void ae_EffectsReset(AeRam *r, const AeAreaSetHooks *h);
void ae_EffectsResetSubsystems(AeRam *r, const AeAreaSetHooks *h);
void ae_PlayerFxBlinkReset(AeRam *r, const AeAreaSetHooks *h);
void ae_SparkTableClear(AeRam *r, const AeAreaSetHooks *h);
void ae_TrailsReset(AeRam *r, const AeAreaSetHooks *h);
void ae_EffectObjsReset(AeRam *r, const AeAreaSetHooks *h);
void ae_RandomParticlesReset(AeRam *r, const AeAreaSetHooks *h);
void ae_FxTableClear12(AeRam *r, const AeAreaSetHooks *h);
void ae_IndicatorsReset(AeRam *r, const AeAreaSetHooks *h);
void ae_AreaSetIndicatorModelA(AeRam *r, const AeAreaSetHooks *h, uint32_t feature);
void ae_AreaSetIndicatorModelB(AeRam *r, const AeAreaSetHooks *h, uint32_t feature);
void ae_AreaSpecialFlagsClear(AeRam *r, const AeAreaSetHooks *h);
void ae_MonkeyLodModelsInit(AeRam *r, const AeAreaSetHooks *h);
void ae_TargetListClear(AeRam *r, const AeAreaSetHooks *h);
void ae_TableClearDAT_800BEF60(AeRam *r, const AeAreaSetHooks *h);
void ae_FxSlotsReset(AeRam *r, const AeAreaSetHooks *h);
void ae_ProjectileGroupsClear(AeRam *r, const AeAreaSetHooks *h);
void ae_HeapInit(AeRam *r, const AeAreaSetHooks *h);
void ae_AreaBumpAllocSet(AeRam *r, const AeAreaSetHooks *h, uint32_t cursor);
uint32_t ae_HeapAlloc(AeRam *r, const AeAreaSetHooks *h, uint32_t byte_count);
uint32_t ae_AreaBumpAlloc(AeRam *r, const AeAreaSetHooks *h, uint32_t byte_count);
int32_t ae_SmallocError(AeRam *r, const AeAreaSetHooks *h, uint32_t byte_count);
void ae_MessageBoxClose(AeRam *r, const AeAreaSetHooks *h);
void ae_RcCarDespawn(AeRam *r, const AeAreaSetHooks *h);
void ae_AreaSetSpecialActor(AeRam *r, const AeAreaSetHooks *h,
                            uint32_t feature, uint32_t feature_kind);
void ae_AreaSetFeature30(AeRam *r, const AeAreaSetHooks *h, uint32_t feature);
void ae_AreaSetFeature31(AeRam *r, const AeAreaSetHooks *h, uint32_t feature);
void ae_AreaSetFeature32(AeRam *r, const AeAreaSetHooks *h, uint32_t feature);
int32_t ae_exit(AeRam *r, const AeAreaSetHooks *h, uint32_t status);
int32_t ae_printf(AeRam *r, const AeAreaSetHooks *h, uint32_t format,
                  uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif
