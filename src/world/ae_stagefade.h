#ifndef AE_STAGEFADE_H
#define AE_STAGEFADE_H

#include "ae_stage.h"
#include "../sound/ae_sound.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AE_STAGEFADE_G_STATE             0x800F4350u
#define AE_STAGEFADE_G_FADE_STATE        0x800F4480u
#define AE_STAGEFADE_G_FADE_TIMER        0x800F4482u
#define AE_STAGEFADE_G_CONTEXT_SOURCE    0x800A5ED0u
#define AE_STAGEFADE_G_FRAME_COUNTER     0x800EC350u
#define AE_STAGEFADE_G_ORDER_TABLE_CURSOR (AE_STAGEFADE_G_FRAME_COUNTER + 0x04u)
#define AE_STAGEFADE_G_CD_STATE          0x800E3890u
#define AE_STAGEFADE_G_SOUND_BANKS       0x800BF670u
#define AE_STAGEFADE_G_LOOP_TABLE        0x800BF370u
#define AE_STAGEFADE_G_TIM_POINTERS      0x800DF7A0u
#define AE_STAGEFADE_G_TIM_IDS           0x800DF7C0u
#define AE_STAGEFADE_G_TIM_AGES          0x800DF7D0u
#define AE_STAGEFADE_G_TIM_COUNT         0x800B35C4u
#define AE_STAGEFADE_G_HUD_PANEL_A       0x800F4DB0u
#define AE_STAGEFADE_TIM_RESET_RETURN    0x800B0000u

enum AeStageFadeStateField {
    AE_STAGEFADE_STATE_LEVEL = 0x124,
    AE_STAGEFADE_STATE_NEXT_AREA = 0x126,
    AE_STAGEFADE_STATE_NEXT_ENTRY = 0x12A,
    AE_STAGEFADE_STATE_EXIT_KIND = 0x12C,
    AE_STAGEFADE_STATE_FADE_STATE = 0x130,
    AE_STAGEFADE_STATE_FADE_TIMER = 0x132,
    AE_STAGEFADE_STATE_FRAME_SIZE = 40,
    AE_STAGEFADE_STACK_LOCAL = 0x10
};

enum AeStageFadeSoundField {
    AE_STAGEFADE_SOUND_SLOT_EFFECTS = 0x00,
    AE_STAGEFADE_SOUND_SLOT_MENU = 0x02,
    AE_STAGEFADE_SOUND_SLOT_STAGE = 0x04,
    AE_STAGEFADE_SOUND_SLOT_BGM = 0x06,
    AE_STAGEFADE_SOUND_SLOT_BGM_MIRROR = 0x16,
    AE_STAGEFADE_SOUND_LOOP_COUNT = 24,
    AE_STAGEFADE_SOUND_LOOP_STRIDE = 0x1C,
    AE_STAGEFADE_SOUND_LOOP_ACTIVE = 0x12,
    AE_STAGEFADE_SOUND_LOOP_INDEX = 0x14,
    AE_STAGEFADE_SOUND_LOOP_FLAGS = 0x16
};

enum AeStageFadeTimField {
    AE_STAGEFADE_TIM_COUNT = 8,
    AE_STAGEFADE_TIM_POINTER_STRIDE = 4,
    AE_STAGEFADE_TIM_ID_STRIDE = 2,
    AE_STAGEFADE_TIM_MAX_AGE = 80
};

enum AeStageFadeHudField {
    AE_STAGEFADE_HUD_PANEL_A_TIMER = 0x08,
    AE_STAGEFADE_HUD_PANEL_A_STATE = 0x0A,
    AE_STAGEFADE_HUD_PANEL_A_FLAGS = 0x20
};

typedef int32_t (*AeStageFadeHookFn)(void *, AeRam *, uint32_t, uint32_t,
                                     uint32_t, uint32_t);

/* Frame entry metadata is used only to preserve original stack-backed and
 * caller-register arguments at named hooks. The host owns all hardware and
 * renderer work; callbacks receive the original a0-a3 words. */
typedef struct AeStageFadeHooks {
    void *user;
    uint32_t stack_pointer;
    uint32_t caller_a3;
    const AeStageHooks *stage;
    const AeSoundHw *sound_hw;
    AeStageFadeHookFn DebugTextPrintf;       /* 8008C6F0 */
    AeStageFadeHookFn SeqTrackClose;          /* 800A1F3C */
    AeStageFadeHookFn SeqTrackSetPlayState;   /* 800A2B6C */
    AeStageFadeHookFn SoundBankFree;           /* 800A1CCC */
    AeStageFadeHookFn SoundStopAllVoices;     /* 800A342C */
    AeStageFadeHookFn RenderScreenFlash;      /* native renderer replacement */
    AeStageFadeHookFn RenderFadeSpiral;       /* native renderer emits 32 packets and advances the OT cursor */
} AeStageFadeHooks;

int32_t ae_StageFadeUpdate(AeRam *r, const AeStageFadeHooks *h); /* 80079B10 */
void ae_ScreenFlashFadeDraw(AeRam *r, const AeStageFadeHooks *h); /* 80079E78 */
void ae_FadeSpiralDraw(AeRam *r, const AeStageFadeHooks *h); /* 80079ECC */
int32_t ae_SoundStop(AeRam *r, const AeStageFadeHooks *h,
                     int32_t mode); /* 80086FD0 */
int32_t ae_SoundLoopsReset(AeRam *r); /* 80088194 */
int32_t ae_TimListResidentUpdate(AeRam *r, const AeStageFadeHooks *h,
                                 int32_t mode, int32_t slot); /* 8008D064 */
int32_t ae_HudPanelA_Start(AeRam *r, int32_t mode); /* 80095244 */

#ifdef __cplusplus
}
#endif
#endif
