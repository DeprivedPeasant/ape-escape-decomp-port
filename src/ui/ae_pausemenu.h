#ifndef AE_PAUSEMENU_H
#define AE_PAUSEMENU_H

#include "../core/ae_ram.h"
#include "../core/ae_pad.h"
#include "../bodies/ae_bodies.h"
#include "../effects/ae_spawn.h"
#include "../player/ae_player2.h"
#include "../player/ae_player_act.h"
#include "../sound/ae_sound.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t (*AePauseMenuHookFn)(void *user, AeRam *ram,
                                     uint32_t a0, uint32_t a1,
                                     uint32_t a2, uint32_t a3);

/* Calls outside the pause-menu batch keep their original argument words.
   The host supplies the native input, sound, platform, text and renderer work. */
typedef struct AePauseMenuHooks {
    void *user;
    uint32_t stack_pointer; /* Original $sp at a top-level call; 0 means 0x801FFF00. */
    uint32_t caller_s5; /* Seeded caller value used by PauseMenuRun's mode-3 exit dispatch. */

    AePauseMenuHookFn WaitFrameFlip;                  /* 8005DE8C */
    AePauseMenuHookFn PauseMenuOverlay1B;              /* 8013FB20, overlay-only helper */
    AePauseMenuHookFn DrawSync;                       /* 800286CC, PsyQ */
    AePauseMenuHookFn VSync;                          /* 8002E520, PsyQ */
    AePauseMenuHookFn PutDispEnv;                     /* 80028EFC, PsyQ */
    AePauseMenuHookFn PutDrawEnv;                     /* 80028D30, PsyQ */
    AePauseMenuHookFn StoreImage;                     /* 800289D8, PsyQ */
    AePauseMenuHookFn LoadImage;                      /* 80028978, PsyQ */
    AePauseMenuHookFn ClearOTagR;                     /* 80028BB8, PsyQ */
    AePauseMenuHookFn DrawOTag;                       /* 80028CC0, PsyQ */
    AePauseMenuHookFn HeapAlloc;                      /* 8008C09C */
    AePauseMenuHookFn HeapFree;                       /* 8008C064 */
    AePauseMenuHookFn ColorFadeReset;                 /* 8008C390 */
    AePauseMenuHookFn PadDirectionCode;               /* 8007F40C */
    AePauseMenuHookFn TextSetCursor;                  /* 8008DDB0 */
    AePauseMenuHookFn PlaySfx;                        /* 800877F8 */
    AePauseMenuHookFn PauseMenuInitVolumes;            /* 80082D40 */
    AePauseMenuHookFn HudTextWidth;                   /* 80082230 */
    AePauseMenuHookFn DrawTextSprites;                /* 80082288 */
    AePauseMenuHookFn DrawMenuArrows;                 /* 8008211C */
    AePauseMenuHookFn IsTimeAttack;                   /* 80097678 */
    AePauseMenuHookFn HudPanelA_Start;                /* 80095244 */
    AePauseMenuHookFn HudPanelB_Start;                /* 80095600 */
    AePauseMenuHookFn HudCounterC_Start;              /* 800958C0 */
    AePauseMenuHookFn HudDrawGadgetSlots;              /* 80081664 */
    AePauseMenuHookFn GadgetSlotRemap;                 /* 80081144 */
    AePauseMenuHookFn HudDrawGadgetSlotsFrame;         /* 80081180 */
    AePauseMenuHookFn SetPriority;                     /* 80012C70 */
    AePauseMenuHookFn CutsceneModeOffCaller;           /* 8005DD28 */
    AePauseMenuHookFn CutsceneModeOn;                 /* 8005DD18 */
    AePauseMenuHookFn PauseDrawCounter;                /* 80082A04 */
    AePauseMenuHookFn PauseDrawCaptureGrid;            /* 80082398 */
    AePauseMenuHookFn PauseMenuDrawSubScreen;          /* 80082D6C */
    AePauseMenuHookFn HudDrawIcon16;                  /* 8005FCA4 */
    AePauseMenuHookFn HudDrawTime;                    /* 8005F978 */
    AePauseMenuHookFn GetDAT_800B01C2;                /* 8005DD34 */
    AePauseMenuHookFn HudDrawTimerGauge;              /* 8005FFD4 */
    AePauseMenuHookFn HudPanelA_DrawIcon;             /* 8005F2E0 */
    AePauseMenuHookFn HudDrawHealth;                  /* 8005E4A8 */
    AePauseMenuHookFn HudPanelB_DrawSprite;           /* 8005F5FC */
    AePauseMenuHookFn HudDrawDigit;                   /* 8005F770 */
    AePauseMenuHookFn HudDrawNumber;                  /* 8005F87C */
    AePauseMenuHookFn HudCounterC_Draw;               /* 8005FD80 */
    AePauseMenuHookFn HudDrawGadgetIcons;              /* 80046BC4 */
    AePauseMenuHookFn CdMgrUpdate;                    /* 800125B8, native archive boundary */
    AePauseMenuHookFn Printf;                          /* 800A3720 */
    AePauseMenuHookFn SoundLoopStop;                  /* 8008806C */
    AePauseMenuHookFn SfxVoiceSetPanVol;              /* 800A3570, sound hardware */
    AePauseMenuHookFn TextNextGlyph;                  /* 8008DD68 */
    AePauseMenuHookFn FontDrawGlyph;                  /* 8008D9D0 */
    AePauseMenuHookFn RotMatrixZ;                     /* 800306F0, PsyQ */
    AePauseMenuHookFn RotMatrixY;                     /* 80030890, PsyQ */
    AePauseMenuHookFn RotMatrixX;                     /* 80030A30, PsyQ */
    AePauseMenuHookFn SetRotMatrix;                   /* 8002FB00, PsyQ */
    AePauseMenuHookFn SetTransMatrix;                 /* 8002FB30, PsyQ */
    AePauseMenuHookFn RotTrans;                       /* 80031940, PsyQ */
    AePauseMenuHookFn RotTransSV;                     /* 80031230, PsyQ */
    AePauseMenuHookFn Memset;                          /* 800A3740 */
    AePauseMenuHookFn ModelTransformRigid;             /* 800161C0, renderer */
    AePauseMenuHookFn ModelDraw;                      /* 80017CD4, renderer */
    AePauseMenuHookFn ModelEmitPrimsScratchColors;     /* 8001C918, renderer */
    AePauseMenuHookFn ModelDrawScreenSpace;            /* 800207B4, renderer */
    AePauseMenuHookFn GadgetDrawSlot0Attachment;        /* 8009ABF4 */

    /* Replacements for inline GTE / GPU work in the listed routines. */
    AePauseMenuHookFn RendererIrgbToIr123;              /* IRGB input -> three IR values in a RAM out vector */
    AePauseMenuHookFn RendererPackIrgb;                 /* accum R,G,B,destination -> packed RGB555 */
    AePauseMenuHookFn RendererGetProjectionDistance;    /* 0 args -> H register */
    AePauseMenuHookFn RendererSetProjectionDistance;    /* H register value */
    AePauseMenuHookFn RendererHudDrawWindowFrame;        /* x,y,width,height */
    AePauseMenuHookFn RendererPauseMenuDrawButtons;      /* layout, enabled mask, mode, OT */
    AePauseMenuHookFn RendererPauseMenuDrawGadgetScreen;  /* cursor, active gadget, mask, buffer */

    /* Existing player port dependencies reached from PauseMenuRun. */
    const AePadHooks *pad;
    const AePlayer2Hooks *player2;
    const AePlayerActHooks *player_act;
    const AeSpawnHooks *spawn;
    const AeBodyHooks *bodies;
    const AeSoundHw *sound_hw;
} AePauseMenuHooks;

void ae_SoundLoopsResume(AeRam *ram, const AePauseMenuHooks *hooks); /* 800729CC */
void ae_PlayerStopGadgetSounds(AeRam *ram, const AePauseMenuHooks *hooks, int32_t gadget); /* 8007A180 */
void ae_DrawScaledModelAt(AeRam *ram, const AePauseMenuHooks *hooks,
                          uint32_t matrix, int32_t scale, int32_t update_player_matrix); /* 8007CAE0 */
uint32_t ae_PauseMenuDrawButton(AeRam *ram, const AePauseMenuHooks *hooks,
                                int32_t x_offset, int32_t y_offset, int32_t item,
                                int32_t selected, uint32_t ot_pointer); /* 8007F5E4 */
void ae_ScreenWipeReset(AeRam *ram); /* 8007F830 */
void ae_ScreenSnapshotRestore(AeRam *ram, const AePauseMenuHooks *hooks); /* 8007F848 */
void ae_ScreenWipeBlurBlock(AeRam *ram, const AePauseMenuHooks *hooks, uint32_t block); /* 8007F8F4 */
void ae_ScreenWipeStep(AeRam *ram, const AePauseMenuHooks *hooks); /* 8007F9D0 */
int32_t ae_PauseMenuRun(AeRam *ram, const AePauseMenuHooks *hooks); /* 8007FA4C */
void ae_PauseMenuSetLayout(AeRam *ram, uint32_t layout); /* 80081B9C */
void ae_PauseMenuDrawButtons(AeRam *ram, const AePauseMenuHooks *hooks); /* 80081D54 */
void ae_HudDrawWindowFrame(AeRam *ram, const AePauseMenuHooks *hooks,
                           int32_t x, int32_t y, int32_t width, int32_t height); /* 80081F18 */
int32_t ae_PauseItemEnabled(AeRam *ram, int32_t item); /* 80082030 */
int32_t ae_PauseMenuUpdate(AeRam *ram, const AePauseMenuHooks *hooks); /* 800821D4 */
void ae_SetDAT_800BEF80(AeRam *ram, uint32_t value); /* 8008227C */
int32_t ae_SoundLoopsSetPan(AeRam *ram, const AePauseMenuHooks *hooks, int32_t resume); /* 80086788 */
void ae_MessageBoxDrawString(AeRam *ram, const AePauseMenuHooks *hooks, uint32_t text); /* 8008DDC8 */
int32_t ae_TextWidthUntilGlyph(AeRam *ram, const AePauseMenuHooks *hooks,
                               uint32_t text, uint32_t glyph); /* 8008DE68 */
int32_t ae_RadarDrawModel(AeRam *ram, const AePauseMenuHooks *hooks,
                          uint32_t model, uint32_t matrix, uint32_t direction, uint32_t scale); /* 8009A574 */

#ifdef __cplusplus
}
#endif
#endif
