#ifndef AE_MISSA_H
#define AE_MISSA_H

#include "ae_ram.h"
#include "../effects/ae_spawn.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Callback boundary used by the functions in this task. The callback receives
 * the original MIPS argument words in a0-a3 order. */
typedef int32_t (*AeMissaHookFn)(void *, AeRam *, uint32_t, uint32_t,
                                uint32_t, uint32_t);
typedef int32_t (*AeMissaIndirectFn)(void *, AeRam *, uint32_t target,
                                     uint32_t, uint32_t, uint32_t, uint32_t);
typedef int32_t (*AeMissaHook5Fn)(void *, AeRam *, uint32_t, uint32_t,
                                  uint32_t, uint32_t, uint32_t);
struct AeStageHooks;
struct AeWorldHooks;
struct AeAreaSetHooks;
struct AeCameraHooks;
struct AeCamupdHooks;
struct AeSoundHw;
struct AeProgressHooks;
struct AePlayer1Hooks;
struct AePlayerActHooks;
struct AeZako2Hooks;
struct AeStageFadeHooks;

typedef struct AeMissaHooks {
    void *user;
    uint32_t stack_pointer;               /* MIPS entry $sp for local pointer arguments */
    const AeSpawnHooks *spawn;
    const struct AeStageHooks *stage;       /* direct call to the verified StageRequest port */
    const struct AeWorldHooks *world;        /* direct call to GetGroundHeight */
    const struct AeAreaSetHooks *area_set;   /* direct call to StaticModelInit */
    const struct AeCameraHooks *camera;      /* direct call to CameraFollowUpdate */
    const struct AeProgressHooks *progress;  /* direct call to DrawTextSprites */
    const struct AePlayer1Hooks *player1;    /* direct calls to archive/CD queue ports */
    const struct AePlayerActHooks *player_act; /* direct call to PlayerSetMode */
    const struct AeZako2Hooks *zako2;        /* direct call to SpawnPickups */
    const struct AeStageFadeHooks *stage_fade; /* direct call to FadeSpiralDraw */
    const struct AeCamupdHooks *camupd;       /* direct calls to event/exit camera helpers */
    const struct AeSoundHw *sound;            /* direct calls to PlaySfx */
    AeMissaHookFn BeginFrame;                 /* 8005DF48 */
    AeMissaHookFn PlaySfx;                    /* 800877F8 */
    AeMissaHookFn ClearWords;                 /* 800A3740 */
    AeMissaHookFn PropVisualUpdate;           /* 80071120 */
    AeMissaHookFn Printf;                       /* 800A3720 */
    AeMissaHookFn PlayerAuxModelInit;           /* 80098FB0 */
    AeMissaHookFn HudDrawTimeRankIcon;           /* renderer packet boundary at 8005F70C */
    AeMissaHookFn HudDrawStatPips;               /* 80060224 renderer packet builder */
    AeMissaHookFn HudDrawFrameBox;               /* 80060334 renderer packet builder */
    AeMissaHookFn HudTextWidth;                  /* 80082230 */
    AeMissaHookFn MessageBoxOpen;                 /* 8008DEEC */
    AeMissaHookFn DrawMenuChrome;                /* inline GPU block at 8007D71C */
    AeMissaHookFn DrawMenuRows;                  /* inline GPU block at 8007D9F0 */
    AeMissaHookFn DrawMenuHighlight;             /* inline GPU block at 8007E030 */
    AeMissaHookFn DrawMenuItem;                  /* inline GPU block at 8007E1B8 */
    AeMissaHookFn DrawMenuSelection;             /* inline GPU block at 8007E3F8 */
    AeMissaHookFn DrawMenuCursor;                /* inline GPU block at 8007F148 */
    AeMissaHookFn SetRotMatrix;                  /* 8002FB00, native renderer matrix state */
    AeMissaHookFn SetTransMatrix;                /* 8002FB30, native renderer matrix state */
    AeMissaHookFn RendererHudMonkeyBadge;        /* inline GPU block in HudDrawMonkeyInfo */
    AeMissaHook5Fn TrailRingDraw;                 /* one native draw callback per live ring segment */
    AeMissaIndirectFn CallIndirect;
} AeMissaHooks;

/* Stick / frame helpers. */
int32_t ae_AreaCheckExits(AeRam *r, const AeMissaHooks *hooks, uint32_t mode); /* 80010D80 */
int32_t ae_PlayerPlaceAtEntry(AeRam *r, const AeMissaHooks *hooks,
                              int32_t speed, int32_t area, int32_t entry); /* 80032D38 */
void ae_PlayerInit(AeRam *r, const AeMissaHooks *hooks); /* 8003272C */
void ae_TrailRingPush(AeRam *r); /* 8001F8B0 */
void ae_TrailRingDraw(AeRam *r, const AeMissaHooks *hooks,
                      uint32_t a0, uint32_t a1, uint32_t a2,
                      uint32_t a3, uint32_t a4); /* 8001F954 ring lifetime + renderer segments */
int32_t ae_AreaUsesAltMember(AeRam *r, int32_t area, int32_t list_index); /* 800115F4 */
int32_t ae_PadDemoPlaybackStep(AeRam *r);                                  /* 80013128 */
uint32_t ae_StickToButtons(AeRam *r, uint32_t buttons);                     /* 800140B4 */
uint32_t ae_PlayerInput_StickGesture(AeRam *r);                             /* 800650D8 */

/* Model / stage helpers. */
void ae_ModelInitNormals(AeRam *r, uint32_t model_state, uint32_t model);   /* 80015B30 */
void ae_ScreenFadeSet(AeRam *r, uint32_t count, uint32_t start, uint32_t step); /* 80046978 */
void ae_PlayTimeAdd(AeRam *r, int32_t seconds);                             /* 8005DD44 */
int32_t ae_PointInBoxXZ(AeRam *r, uint32_t position, uint32_t box);         /* 80073B64 */
int32_t ae_DirIndexFromFlags(AeRam *r, uint32_t flags);                    /* 8007774C */
int32_t ae_FadeFrame(AeRam *r, const AeMissaHooks *hooks);                 /* 8007A14C */
void ae_ZakoClearDefeatedBits(AeRam *r, uint32_t defeated_mask);          /* 80054A08 */
void ae_ZakoSaveDefeatedMask(AeRam *r, const AeMissaHooks *hooks);       /* 800509CC */
void ae_CameraRadarFocusSelect(AeRam *r, const AeMissaHooks *hooks);       /* 80047E8C */
void ae_CameraRadarFocusUpdate(AeRam *r);                                  /* 80047EE0 */
void ae_CameraRcCarFollow(AeRam *r, const AeMissaHooks *hooks,
                          uint32_t camera_slot);                            /* 8004AA58 */
void ae_CameraGetSlotTarget(AeRam *r, uint32_t out);                        /* 8004B54C */
void ae_BossAimSfx(AeRam *r, const AeMissaHooks *hooks);                    /* 8004898C */
void ae_AimRequestStart(AeRam *r, const AeMissaHooks *hooks);               /* 80048398 */
int32_t ae_CameraAreaScriptDispatch(AeRam *r, const AeMissaHooks *hooks,
                                    uint32_t script_index, uint32_t argument,
                                    uint32_t arg2, uint32_t arg3);
int32_t ae_CallIndexedHandler(AeRam *r, const AeMissaHooks *hooks,
                              uint32_t argument, uint32_t handler_index,
                              uint32_t arg2, uint32_t arg3);
void ae_StageExitStart(AeRam *r, const AeMissaHooks *hooks);                /* 8004D73C */
void ae_EventCamStartVictory(AeRam *r, const AeMissaHooks *hooks);          /* 8004BBF0 */
void ae_EventCamStartDefault(AeRam *r, const AeMissaHooks *hooks);          /* 8004BC5C */
void ae_PlayerAttachSet(AeRam *r, const AeMissaHooks *hooks,
                        uint32_t attach_record, uint32_t duration,
                        uint32_t arg2, uint32_t arg3);                    /* 80066EEC */
void ae_PlayerSetBounceFlag(AeRam *r);                                     /* 80066F34 */
void ae_SpawnEffectGroupAt(AeRam *r, const AeMissaHooks *hooks,
                           int32_t type, int32_t x, int32_t y, int32_t z); /* 800698E4 */
void ae_BreakableFinalizeVisualUpdate(AeRam *r, const AeMissaHooks *hooks,
                                      uint32_t position, uint32_t model,
                                      int32_t kind);                       /* 8006AA60 */
void ae_PlayerFxJumpBurst(AeRam *r, const AeMissaHooks *hooks);            /* 8006C58C */
void ae_PlayerFxSpawn116(AeRam *r, const AeSpawnHooks *hooks);             /* 8006CE9C */
void ae_PlayerFxSetMatrixPos(AeRam *r, uint32_t source);                    /* 8006CFBC */
void ae_PadDirectionCode(AeRam *r, const AeMissaHooks *hooks);             /* 8007F40C */
void ae_HudDrawTimeRankIcon(AeRam *r, const AeMissaHooks *hooks,
                            int32_t x, int32_t y, uint32_t time,
                            uint32_t thresholds); /* 8005F68C */
void ae_HudDrawMonkeyInfo(AeRam *r, const AeMissaHooks *hooks,
                          uint32_t monkey); /* 80060448 */
void ae_Indicator_Activate(AeRam *r, const AeMissaHooks *hooks,
                           int32_t index, int32_t sector); /* 800717B4 */
uint32_t ae_TargetListFindBest(AeRam *r, const AeMissaHooks *hooks,
                               uint32_t position, uint32_t direction,
                               int32_t max_angle);                          /* 8007CDFC */

#ifdef __cplusplus
}
#endif
#endif
