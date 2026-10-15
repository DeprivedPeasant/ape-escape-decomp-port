#ifndef AE_STAGE_H
#define AE_STAGE_H

#include "../core/ae_ram.h"

typedef struct AeMonkeyHooks AeMonkeyHooks;
typedef struct AeGadgetHooks AeGadgetHooks;

/* Calls outside this batch stay at the hardware / subsystem boundary. Every
 * callback receives the original a0-a3 argument words as PSX addresses/values. */
typedef int32_t (*AeStageHookFn)(void *, AeRam *, uint32_t, uint32_t, uint32_t, uint32_t);

typedef struct AeStageHooks {
    void *user;
    uint32_t stack_pointer; /* MIPS entry SP for stack-backed ABI hook arguments */
    AeStageHookFn AreaUsesAltMember; /* 800115F4 (area, signed list index) */
    AeStageHookFn ArcGetMemberPos;    /* 80012C0C (CdlLOC *out, member) */
    AeStageHookFn FUN_80025E24;       /* 80025E24 (CdlLOC *); original symbol unresolved */
    AeStageHookFn CdMgrQueueCommand;  /* 80012360 (command, parameter*, status*, done*) */
    AeStageHookFn StageExitStart;     /* 8004D73C */
    AeStageHookFn IsTimeAttack;       /* 80097678 */
    AeStageHookFn GetLevelFirstArea;  /* 8009764C (level) */
    AeStageHookFn GimmickSaveStates;       /* 8005ADF8 */
    AeStageHookFn GimmickSaveApplyProgress;/* 8005B480 */
    AeStageHookFn ItemSaveCollectedMask;   /* 800847EC */
    AeStageHookFn ZakoSaveDefeatedMask;    /* 800509CC */
    AeStageHookFn AreaGetKind;          /* 80010CA4 */
    AeStageHookFn AreaGetLevel;         /* 80010CC0 */
    AeStageHookFn GimmickSaveTableReset;/* 8005AD90 */
    AeStageHookFn ItemsClearFlags;      /* 800847C4 */
    AeStageHookFn ZakoClearSlotTable;   /* 800509A4 */
    AeStageHookFn SoundSilence;         /* 80086148 */
    AeStageHookFn LevelMonkeyCountGet;  /* 800965EC (level) */
    AeStageHookFn AreaBumpAlloc;         /* 8008C1B8 (byte size) */
    AeStageHookFn MeshInit;              /* 80015C34 (AreaProp *, mesh entry) */
    AeStageHookFn GetZoneMask;           /* 8004626C (signed xyz, fallback mask) */
    AeStageHookFn FUN_80030890;          /* unresolved area-prop rotation helper */
    AeStageHookFn FUN_800A3700;          /* unresolved model-over warning */
    AeStageHookFn HeapInit;              /* 8008C01C */
    AeStageHookFn AreaBumpAllocSet;      /* 8008C058 */
    AeStageHookFn AreaSpecialFlagsClear; /* 8007AFB8 */
    AeStageHookFn AreaSetVramAnimList;   /* 800690E8 */
    AeStageHookFn AreaSetZoneBoxes;      /* 80043380 */
    AeStageHookFn IndicatorsReset;       /* 800715A0 */
    AeStageHookFn AreaSetFeature30;      /* 8009B7C8 */
    AeStageHookFn AreaSetSpecialActor;   /* 8009B3CC */
    AeStageHookFn AreaSetIndicatorModelA;/* 8007163C */
    AeStageHookFn AreaSetIndicatorModelB;/* 80071694 */
    AeStageHookFn AreaSetFeature31;      /* 8009B7E4 */
    AeStageHookFn AreaSetFeature32;      /* 8009B7F0 */
    AeStageHookFn SoundAreaKeepsMusic;   /* 80086F00 */
    AeStageHookFn BgmStart;              /* 80087388 */
    AeStageHookFn BgmUpdateVolume;       /* 8008745C */
    AeStageHookFn ApplyLevelFog;         /* 800323F8 */
    AeStageHookFn SetColorMatrixMode;    /* 80015D58 */
    AeStageHookFn SoundLoopsStopAll;     /* 80086540 */
    AeStageHookFn BuildTerrainBlockMask;  /* 800438F8 */
    AeStageHookFn TableClearDAT_800BEF60;/* 8007D0F0 */
    AeStageHookFn DebugMenuInitPlayerParams; /* 8005CB1C */
    AeStageHookFn DebugMenuInitCounters; /* 8005CC58 */
    AeStageHookFn DebugMenuInitCounters2;/* 8005CCDC */
    AeStageHookFn MessageBoxClose;       /* 8008DEE0 */
    AeStageHookFn PlayerSpawnAtEntry;    /* 80032BBC */
    AeStageHookFn TargetListClear;       /* 8007CD60 */
    AeStageHookFn ZakoInit;              /* 800505C8 */
    AeStageHookFn MonkeyLodModelsInit;   /* 8007BCB4 */
    AeStageHookFn FxSlotsReset;          /* 80084A60 */
    AeStageHookFn ProjectilesReset;      /* 800592E0 */
    AeStageHookFn EffectsReset;          /* 80069500 */
    AeStageHookFn ColliderZoneMasksUpdate; /* 80088670 */
    AeStageHookFn GimmickSpawnAll;       /* 8005B538 */
    AeStageHookFn BossInit;              /* 8005AB90 */
    AeStageHookFn ItemsLoadForArea;      /* 80083470 */
    AeStageHookFn AreaTexCopyInit;       /* 8005E7EC */
    AeStageHookFn RcCarDespawn;          /* 8009946C */
    AeStageHookFn PlayerSelectGadget;    /* 8007AC64 */
    AeStageHookFn HudFindGadgetIconPos;  /* 8008197C */
    AeStageHookFn MonkeyClearWords;      /* 80032708 */
    AeStageHookFn VSync;                 /* 8002E520 */
    AeStageHookFn Printf;                /* 800A3720 */
    AeStageHookFn CutsceneModeOff;        /* 8005DD28 */
    AeStageHookFn CutsceneModeOn;         /* 8005DD18 */
    AeStageHookFn PlayerInit;             /* 8003272C */
    AeStageHookFn PlayerSetMode;          /* 8007B374 */
    AeStageHookFn FUN_80028850;           /* unresolved context helper */
    AeStageHookFn FUN_800286CC;           /* unresolved context helper */
    AeStageHookFn WaitFrameFlip;          /* 8005DE8C */
    AeStageHookFn BgmFadeOut;             /* 8008760C */
    AeStageHookFn SoundStop;              /* 80086FD0 */
    AeStageHookFn PlaySfx;                /* 800877F8 */
    AeStageHookFn ProgressSetByte;        /* 80096360 */
    AeStageHookFn PlayTimeAdd;             /* 8005DD44 */
    AeStageHookFn StageGradeUpdate;        /* 80097BFC */
    AeStageHookFn StageVictoryStart;       /* 8009543C */
    AeStageHookFn StageTypeClearInit;      /* 80094F48 */
    AeStageHookFn StageTypeClearUpdate;    /* 80095030 */
    AeStageHookFn StageIntroStart;         /* 8009546C */
    AeStageHookFn PlayerEnterCutsceneMode; /* 80095588 */
    AeStageHookFn StageIntroUpdate;        /* 800954C4 */
    AeStageHookFn GimmickNopUpdate3;       /* 80094F40 */
    AeStageHookFn GimmickNopUpdate4;       /* 80095464 */
    AeStageHookFn XaPlay;                 /* 80011F6C */
    AeStageHookFn AreaCheckExits;          /* 80010D80 */
    AeStageHookFn CdMgrUpdate;             /* 800125B8 */
    AeStageHookFn FadeFrame;               /* 8007A14C */
    AeStageHookFn LoadListStep;            /* 800116C0 */
    AeStageHookFn FUN_80096638;            /* unresolved progress helper */
    AeStageHookFn FUN_8013EA24;            /* unresolved overlay helper */
    AeStageHookFn AimRequestClear;           /* 80048430 */
    AeStageHookFn AimModeLeave;              /* 8004B510 */
    const AeMonkeyHooks *monkey_hooks;
    const AeGadgetHooks *gadget_hooks;
} AeStageHooks;

/* Stage lifecycle and transition functions on the original PSX RAM image. */
int32_t ae_AreaLoadStartSeek(AeRam *r, const AeStageHooks *h, int32_t list_index); /* 80010D04 */
int32_t ae_AreaLoadQueueRead(AeRam *r, const AeStageHooks *h);                  /* 80011208 */
int32_t ae_AreaLoadStart(AeRam *r, const AeStageHooks *h, int32_t list_index); /* 800114D8 */
int32_t ae_AreaLoad(AeRam *r, const AeStageHooks *h, int32_t area, int32_t entry); /* 80031B60 */
void ae_AreaLoadGridAndProps(AeRam *r, const AeStageHooks *h);                 /* 8003244C */
int32_t ae_StageRequest(AeRam *r, const AeStageHooks *h, int32_t code, int32_t area, int32_t arg); /* 8005D100 */
int32_t ae_StageRequestUpdate(AeRam *r, const AeStageHooks *h);                /* 8005D890 */
int32_t ae_StageRequestExit(AeRam *r, const AeStageHooks *h);                  /* 80094F20 */
int32_t ae_StageRequestRetry(AeRam *r, const AeStageHooks *h);                 /* 800955C8 */
int32_t ae_StageProgressOnEnter(AeRam *r, const AeStageHooks *h, int32_t area); /* 800966B4 */
int32_t ae_StageProgressStoreArea(AeRam *r, const AeStageHooks *h, int32_t area); /* 80096E0C */
int32_t ae_StageProgressOnExit(AeRam *r, const AeStageHooks *h, int32_t area); /* 80096FAC */
int32_t ae_StageProgressRestore(AeRam *r, const AeStageHooks *h, int32_t area); /* 800972E0 */
void ae_StageProgressRestore_Nop(AeRam *r);       /* 80050AFC */
void ae_StageProgressRestore_Nop2(AeRam *r);      /* 800848B0 */

#endif
