#ifndef AE_MISC_H
#define AE_MISC_H

#include "../core/ae_ram.h"
#include "../collision/ae_collprobe.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A callback stands for one unported game subsystem or a public SDK / hardware
 * operation. Arguments are the original a0-a3 words, including PSX addresses. */
typedef int32_t (*AeMiscCall)(void *, AeRam *, uint32_t, uint32_t,
                             uint32_t, uint32_t);

typedef struct AeSoundHw AeSoundHw;

typedef struct AeMiscHooks {
    void *user;
    uint32_t stack_pointer; /* Shared MIPS stack base for pointer arguments. */

    /* SDK matrix / rendering entry points. */
    AeMiscCall MulMatrix;            /* 80031470 */
    AeMiscCall SetRotMatrix;         /* 8002FB00 */
    AeMiscCall SetTransMatrix;       /* 8002FB30 */
    AeMiscCall RotTransPers;         /* 80031330 */
    AeMiscCall ModelDraw;            /* 800845E0 */
    AeMiscCall ModelDrawWithOt;      /* 80017D94 */
    AeMiscCall ModelTransformRigid;  /* 800161C0 */
    AeMiscCall ModelTransformSkinned;/* 80016A60 */
    AeMiscCall ModelDrawEnvMap;      /* 8001CE54 */
    AeMiscCall DrawShadow;           /* 8001F604 */
    AeMiscCall RenderEnvUV;          /* named native renderer operation replacing inline GTE work */
    AeMiscCall RenderIndicator;      /* indicator pose / draw / shadow block */
    AeMiscCall RenderHudPopup;       /* HUD packet construction and ordering-table insertion */
    AeMiscCall CopyPlayerEyeTexture; /* 80067C08 */
    AeMiscCall HudShowHealth;        /* 8005E42C */
    AeMiscCall PlaySfx;              /* 800877F8 */
    AeMiscCall IndicatorDraw;        /* indicator public draw/activation routine */
    AeMiscCall IndicatorRegister;    /* indicator owner/slot registration */
    AeMiscCall IndicatorActivate;    /* indicator activation */
    AeMiscCall SpecialActorDraw;    /* area indicator draw helper */
    AeMiscCall SpawnParticle;        /* 800698B8 */
    AeMiscCall TrailSpawn;           /* 8006ED78 */
    AeMiscCall RotTransPersResult;   /* 80031330; renderer projection hook */

    /* Area-specific code-overlay updates. The source members are loaded at
     * 0x8013D000 by the checker; their entry code is not present in SCUS_944.23. */
    AeMiscCall FUN_8013f464;         /* area-code overlay entry */
    AeMiscCall FUN_8013fa2c;         /* area-code overlay entry */
    AeMiscCall FUN_8013fd74;         /* area-code overlay entry */
    AeMiscCall FUN_80140a60;         /* area-code overlay entry */
    AeMiscCall FUN_80140d60;         /* area-code overlay entry */

    /* Other original game callees outside this task's table. */
    AeMiscCall GetGroundHeight;      /* 80043400; used only if the checked port is unavailable */
    AeMiscCall ColliderTypeActive;   /* 80079A28; used only if the checked port is unavailable */
    AeMiscCall IsTimeAttack;        /* 80097678 */
    AeMiscCall HeapFree;            /* 8008C064 */
    AeMiscCall Printf;               /* 800A3720; only forwarded for collision-body table exhaustion */
    AeMiscCall SoundLoopStart;       /* 800878E0 */
    AeMiscCall SoundLoopStop;       /* 8008806C */
    AeMiscCall SpuWaitIdle;         /* 800A1398 */
    AeMiscCall SpuVolumeClamp;      /* 800A119C */
    AeMiscCall ColliderScriptMotion;/* 8008B980 */
    AeMiscCall ColliderScriptUpdate;/* runtime callback from the stage collision table */
    AeMiscCall PadRumbleReset;      /* PsyQ controller rumble reset */
    AeMiscCall ScreenBandDraw;      /* 8009B9C8; native GPU packet construction */
    AeMiscCall SetDrawMove;         /* 8002B380; PsyQ GPU move command */
    AeMiscCall exit;                /* libc exit veneer */
    AeMiscCall FUN_801402d8;        /* area-code overlay entry */
    AeMiscCall Area53_BossUpdate;   /* area-code overlay entry */
    AeMiscCall FUN_80143854;        /* area-code overlay entry */
    AeMiscCall Area46_BossUpdate;   /* area-code overlay entry */
    AeMiscCall Area57_BossUpdate;   /* area-code overlay entry */
    AeMiscCall FUN_80137248;        /* area-code overlay entry */
    AeMiscCall FUN_8013fc18;        /* area-code overlay entry */
    AeMiscCall FUN_8013fe4c;        /* area-code overlay entry */
    AeMiscCall FUN_80088d78;        /* collider-script table handler */
    AeMiscCall FUN_80088f2c;        /* collider-script table handler */
    AeMiscCall FUN_800895f0;        /* collider-script table handler */
    AeMiscCall FUN_80089b10;        /* collider-script table handler */
    AeMiscCall FUN_80089d64;        /* collider-script table handler */
    AeMiscCall FUN_80089d6c;        /* collider-script table handler */
    AeMiscCall FUN_8008aa64;        /* collider-script table handler */
    AeMiscCall GetSolidHeight;      /* 8004D8C0; called by verified GetGroundHeight */
    AeMiscCall GetColliderQuadrantMaskXZ; /* 800889B4 */
    AeMiscCall CdIntToPos;          /* PsyQ CdIntToPos used by ArcGetMemberPos */
    AeMiscCall CdControl;           /* PsyQ CD control used when draining a full queue */
    AeMiscCall CdReadyCallback;     /* PsyQ CD callback used when draining a full queue */
    AeMiscCall VSync;               /* PsyQ VSync used when draining a full queue */
    AeMiscCall AnimFramePtr;        /* player animation-frame address helper */
    AeMiscCall CollisionProbe0;
    AeMiscCall CollisionProbe1;
    AeMiscCall CollisionProbe2;
    AeMiscCall CollisionProbe3;
    AeMiscCall CollisionProbe4;
    AeMiscCall CollisionProbe5;
    AeMiscCall CollisionProbe6;
    AeMiscCall CollisionProbe7;
    AeMiscCall PointInBoxXZ;             /* 80073B64 */
    AeMiscCall HalfPipeRangeTest;         /* 80050490 */
    AeMiscCall ColliderSlopeContactDir;   /* 8004F884 */
    AeMiscCall DirIndexFromFlags;         /* 8007774C */
    AeCollProbeClimbFn PlayerSetClimbTarget;      /* 80066E28 */
    AeMiscCall PlayerSetRope;             /* 80066E8C */

    /* Same hooks used by already-ported audio requests. */
    const AeSoundHw *sound_hw;
} AeMiscHooks;

/* Stage, camera and area helpers. */
void ae_MatrixRotYMul(AeRam *r, const AeMiscHooks *h, int32_t angle,
                      uint32_t matrix); /* 80014750 */
void ae_MeshCalcEnvUVs(AeRam *r, const AeMiscHooks *h,
                       uint32_t instance); /* 80016D60 */
void ae_CameraNudge(AeRam *r, int32_t dx, int32_t dy, int32_t dz); /* 80047690 */
int32_t ae_AreaOverlayUpdate(AeRam *r, const AeMiscHooks *h); /* 8005ACE8 */
void ae_StageTimerAdd(AeRam *r, int32_t seconds); /* 8005DD78 */
void ae_PlayerHeal(AeRam *r, const AeMiscHooks *h, int32_t amount); /* 8005E794 */
void ae_PlayerEyeBlinkUpdate(AeRam *r, const AeMiscHooks *h); /* 80067AF0 */
int32_t ae_AreaFeature30GetState(AeRam *r); /* 8009B7A0 */
void ae_SpecialActorPlayerNear(AeRam *r); /* 8009B6C8 */

/* Indicators and pickups. */
void ae_IndicatorHintFree(AeRam *r, const AeMiscHooks *h); /* 800715B4 */
int32_t ae_IndicatorHintReady(AeRam *r, const AeMiscHooks *h); /* 800715FC */
void ae_IndicatorsUpdateDraw(AeRam *r, const AeMiscHooks *h); /* 800719F8 */
void ae_PickupCollect(AeRam *r, const AeMiscHooks *h, int32_t kind); /* 80078A18 */
int32_t ae_CollisionRayTest(AeRam *r, const AeMiscHooks *h, uint32_t mode,
                            uint32_t params, uint32_t out); /* 80078C9C */
void ae_PlayerClearDAT44A2(AeRam *r); /* 8007BC9C */
void ae_PickupSetPosition(AeRam *r, uint32_t record); /* 800836E0 */
void ae_PickupsUpdate(AeRam *r, const AeMiscHooks *h); /* 80083CB8 */
void ae_ItemsDraw(AeRam *r, const AeMiscHooks *h); /* 8008435C */
void ae_ItemsUpdateDraw(AeRam *r, const AeMiscHooks *h); /* 80084598 */
void ae_ItemDrawModelEnvMap(AeRam *r, const AeMiscHooks *h,
                            int32_t model_index, uint32_t item); /* 800846D8 */
void ae_CollectedItemsAdd(AeRam *r, uint32_t item_id); /* 80084964 */

/* Positional sound and collider scripts. */
int32_t ae_SoundEmitterUpdate(AeRam *r, const AeMiscHooks *h); /* 8008686C */
int32_t ae_BgmApplyPlayerEffect(AeRam *r); /* 80087654 */
int32_t ae_SoundEmitterAttenuation(AeRam *r, uint32_t emitter,
                                   uint32_t out); /* 80087EE0 */
int32_t ae_ColliderScriptsUpdate(AeRam *r, const AeMiscHooks *h); /* 80088AB8 */
int32_t ae_ColliderScriptRun(AeRam *r, const AeMiscHooks *h,
                             uint32_t collider, int32_t index); /* 8008B7F8 */

/* Message box and HUD. */
void ae_MessageBoxSetText(AeRam *r, uint32_t text); /* 8008DF20 */
int32_t ae_MessageBoxBusy(AeRam *r); /* 8008DF3C */
int32_t ae_HudPanelB_Start(AeRam *r, int32_t mode, int32_t argument); /* 80095600 */
int32_t ae_HudCounterC_Start(AeRam *r, int32_t argument); /* 800958C0 */
void ae_HudPopupStartPickup(AeRam *r); /* 80098398 */
void ae_HudPopupUpdate(AeRam *r, const AeMiscHooks *h); /* 800983E8 */

/* Sequencer and SPU voice helpers. */
int32_t ae_SeqFadeTrackProgram(AeRam *r, int32_t track, int32_t program,
                               int32_t volume, int32_t frames); /* 800A25CC */
int32_t ae_SeqGetChannelProgram(AeRam *r, int32_t track, int32_t channel,
                                int32_t force); /* 800A30D8 */
void ae_SfxVoiceSetPanVol(AeRam *r, const AeMiscHooks *h, int32_t voice,
                          int32_t left, int32_t right); /* 800A3570 */

#ifdef __cplusplus
}
#endif
#endif
