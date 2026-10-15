#include "ae_stagefade.h"

#include "ae_misc.h"
#include "../player/ae_player1.h"
#include "../sound/ae_bgm.h"

#include <stddef.h>

#define AE_STAGEFADE_DEFAULT_SP 0x801FFF00u

static int32_t stagefade_s16(uint32_t value)
{
    uint16_t low = (uint16_t)value;
    return (low & 0x8000u) != 0u ? (int32_t)low - 0x10000 : (int32_t)low;
}

static uint32_t stagefade_entry_sp(const AeStageFadeHooks *h)
{
    return h != NULL && h->stack_pointer != 0u ? h->stack_pointer :
                                                  AE_STAGEFADE_DEFAULT_SP;
}

static uint32_t stagefade_frame_sp(const AeStageFadeHooks *h)
{
    return stagefade_entry_sp(h) - AE_STAGEFADE_STATE_FRAME_SIZE;
}

static int32_t stagefade_hook(AeRam *r, const AeStageFadeHooks *h,
                              AeStageFadeHookFn fn, const char *name,
                              uint32_t a0, uint32_t a1,
                              uint32_t a2, uint32_t a3)
{
    if (h == NULL || fn == NULL) {
        ae_ram_fail(r, "AeStageFadeHooks.%s not provided", name);
        return 0;
    }
    return fn(h->user, r, a0, a1, a2, a3);
}

static int32_t stagefade_stage_hook(AeRam *r, const AeStageFadeHooks *h,
                                    AeStageHookFn fn, const char *name,
                                    uint32_t a0, uint32_t a1,
                                    uint32_t a2, uint32_t a3)
{
    if (h == NULL || h->stage == NULL || fn == NULL) {
        ae_ram_fail(r, "AeStageFadeHooks.stage.%s not provided", name);
        return 0;
    }
    return fn(h->stage->user, r, a0, a1, a2, a3);
}

static const AeStageHooks *stagefade_callee_hooks(
    const AeStageFadeHooks *h, AeStageHooks *storage)
{
    if (h == NULL || h->stage == NULL) return NULL;
    *storage = *h->stage;
    storage->stack_pointer = stagefade_frame_sp(h);
    return storage;
}

static void stagefade_copy_context(AeRam *r, uint32_t destination)
{
    for (uint32_t word = 0; word < 2u; ++word) {
        ae_wr32(r, destination + word * 4u,
                ae_rd32(r, AE_STAGEFADE_G_CONTEXT_SOURCE + word * 4u));
    }
}

static void stagefade_context_hooks(AeRam *r, const AeStageFadeHooks *h)
{
    uint32_t context = stagefade_frame_sp(h) + AE_STAGEFADE_STACK_LOCAL;
    stagefade_copy_context(r, context);
    (void)stagefade_stage_hook(r, h,
            h != NULL && h->stage != NULL ? h->stage->FUN_80028850 : NULL,
            "FUN_80028850", context, 0u, 0u, 0u);
    if (ae_ram_failed(r)) return;
    (void)stagefade_stage_hook(r, h,
            h != NULL && h->stage != NULL ? h->stage->FUN_800286CC : NULL,
            "FUN_800286CC", 0u, 0u, 0u, 0u);
}

static int32_t stagefade_request(AeRam *r, const AeStageFadeHooks *h,
                                 int32_t action, uint32_t area,
                                 uint32_t entry)
{
    AeStageHooks storage;
    const AeStageHooks *callee = stagefade_callee_hooks(h, &storage);
    return ae_StageRequest(r, callee, action, (int32_t)area, (int32_t)entry);
}

static int32_t stagefade_queue_read(AeRam *r, const AeStageFadeHooks *h)
{
    AeStageHooks storage;
    const AeStageHooks *callee = stagefade_callee_hooks(h, &storage);
    return ae_AreaLoadQueueRead(r, callee);
}

/* Stagefade reaches the shared ScreenFlashDraw renderer boundary through the
 * same host callback without defining a second public 80046990 symbol. */
static void stagefade_render_screen_flash(AeRam *r,
                                          const AeStageFadeHooks *h,
                                          int32_t shade)
{
    (void)stagefade_hook(r, h, h != NULL ? h->RenderScreenFlash : NULL,
                         "RenderScreenFlash", (uint32_t)shade, 0u, 0u, 0u);
}

/* 80079E78: draw the frame-scaled flash color. Division truncates toward zero. */
void ae_ScreenFlashFadeDraw(AeRam *r, const AeStageFadeHooks *h)
{
    int32_t timer = ae_rds16(r, AE_STAGEFADE_G_FADE_TIMER);
    int32_t shade = (timer * 255) / 48;
    stagefade_render_screen_flash(r, h, shade);
}

/* 80079ECC: clamp the game timer, then let the native renderer draw the wipe. */
void ae_FadeSpiralDraw(AeRam *r, const AeStageFadeHooks *h)
{
    int32_t timer = ae_rds16(r, AE_STAGEFADE_G_FADE_TIMER);
    if (timer >= 25) {
        timer = 24;
        ae_wr16(r, AE_STAGEFADE_G_FADE_TIMER, (uint32_t)timer);
    }
    (void)stagefade_hook(r, h, h != NULL ? h->RenderFadeSpiral : NULL,
                         "RenderFadeSpiral", (uint32_t)timer, 0u, 0u, 0u);
}

/* 80088194: reset only the loop fields cleared by the original. */
int32_t ae_SoundLoopsReset(AeRam *r)
{
    for (uint32_t slot = 0; slot < AE_STAGEFADE_SOUND_LOOP_COUNT; ++slot) {
        uint32_t loop = AE_STAGEFADE_G_LOOP_TABLE +
                        slot * AE_STAGEFADE_SOUND_LOOP_STRIDE;
        ae_wr16(r, loop, 0xFFFFu);
        ae_wr16(r, loop + 2u, 0xFFFFu);
        ae_wr16(r, loop + AE_STAGEFADE_SOUND_LOOP_ACTIVE, 0u);
        ae_wr16(r, loop + AE_STAGEFADE_SOUND_LOOP_INDEX, 0xFFFFu);
        ae_wr16(r, loop + AE_STAGEFADE_SOUND_LOOP_FLAGS, 0u);
    }
    return 0;
}

static int32_t sound_stop_hook(AeRam *r, const AeStageFadeHooks *h,
                               AeStageFadeHookFn fn, const char *name,
                               uint32_t a0, uint32_t a1)
{
    return stagefade_hook(r, h, fn, name, a0, a1, 0u, 0u);
}

static void sound_stop_wait_track(AeRam *r, const AeStageFadeHooks *h,
                                  int32_t track)
{
    if (track < 0) return;
    int32_t result;
    do {
        result = sound_stop_hook(r, h,
                    h != NULL ? h->SeqTrackClose : NULL,
                    "SeqTrackClose", (uint32_t)track, 0u);
    } while (!ae_ram_failed(r) && stagefade_s16((uint32_t)result) < 0);
}

static void sound_stop_wait_bank(AeRam *r, const AeStageFadeHooks *h,
                                 int32_t bank)
{
    if (bank < 0) return;
    int32_t result;
    do {
        result = sound_stop_hook(r, h,
                    h != NULL ? h->SoundBankFree : NULL,
                    "SoundBankFree", (uint32_t)bank, 0u);
    } while (!ae_ram_failed(r) && stagefade_s16((uint32_t)result) < 0);
}

static void sound_stop_bank_slot(AeRam *r, const AeStageFadeHooks *h,
                                 uint32_t offset)
{
    int32_t bank = ae_rds16(r, AE_STAGEFADE_G_SOUND_BANKS + offset);
    sound_stop_wait_bank(r, h, bank);
}

/* 80086FD0: game-side sound state and ordering stay in RAM; bank/voice
 * operations cross the sequencer or SPU host boundary. */
int32_t ae_SoundStop(AeRam *r, const AeStageFadeHooks *h, int32_t mode)
{
    int32_t bgm_track = ae_rds16(r, AE_STAGEFADE_G_SOUND_BANKS +
                                   AE_STAGEFADE_SOUND_SLOT_BGM);
    if (bgm_track >= 0) {
        if (h == NULL || h->sound_hw == NULL) {
            ae_ram_fail(r, "AeStageFadeHooks.sound_hw not provided");
            return 0;
        }
        (void)ae_BgmSeqSetVolume(r, h->sound_hw, bgm_track, 0);
        if (ae_ram_failed(r)) return 0;
        (void)sound_stop_hook(r, h,
                h != NULL ? h->SeqTrackSetPlayState : NULL,
                "SeqTrackSetPlayState", (uint32_t)bgm_track, 1u);
        if (ae_ram_failed(r)) return 0;
    }

    if (mode != 0 && mode != 1 && mode != 3) return 0;

    if (bgm_track >= 0) {
        sound_stop_wait_track(r, h, bgm_track);
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, AE_STAGEFADE_G_SOUND_BANKS + AE_STAGEFADE_SOUND_SLOT_BGM,
                0xFFFFu);
        ae_wr16(r, AE_STAGEFADE_G_SOUND_BANKS +
                   AE_STAGEFADE_SOUND_SLOT_BGM_MIRROR, 0xFFFFu);
    }

    if (mode == 3) return 0;

    (void)ae_SoundLoopsReset(r);
    (void)sound_stop_hook(r, h,
            h != NULL ? h->SoundStopAllVoices : NULL,
            "SoundStopAllVoices", 1u, 0u);
    if (ae_ram_failed(r)) return 0;
    (void)stagefade_stage_hook(r, h,
            h != NULL && h->stage != NULL ? h->stage->VSync : NULL,
            "VSync", 0u, 0u, 0u, 0u);
    if (ae_ram_failed(r)) return 0;

    sound_stop_bank_slot(r, h, AE_STAGEFADE_SOUND_SLOT_STAGE);
    if (ae_ram_failed(r)) return 0;
    sound_stop_bank_slot(r, h, AE_STAGEFADE_SOUND_SLOT_MENU);
    if (ae_ram_failed(r)) return 0;
    if (mode != 1) {
        sound_stop_bank_slot(r, h, AE_STAGEFADE_SOUND_SLOT_EFFECTS);
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, AE_STAGEFADE_G_SOUND_BANKS + AE_STAGEFADE_SOUND_SLOT_STAGE,
                0xFFFFu);
        ae_wr16(r, AE_STAGEFADE_G_SOUND_BANKS + AE_STAGEFADE_SOUND_SLOT_MENU,
                0xFFFFu);
        ae_wr16(r, AE_STAGEFADE_G_SOUND_BANKS + AE_STAGEFADE_SOUND_SLOT_EFFECTS,
                0xFFFFu);
    } else {
        ae_wr16(r, AE_STAGEFADE_G_SOUND_BANKS + AE_STAGEFADE_SOUND_SLOT_STAGE,
                0xFFFFu);
        ae_wr16(r, AE_STAGEFADE_G_SOUND_BANKS + AE_STAGEFADE_SOUND_SLOT_MENU,
                0xFFFFu);
    }
    return 0;
}

/* 8008D064: maintain the eight resident TIM records and their aging bytes. */
int32_t ae_TimListResidentUpdate(AeRam *r, const AeStageFadeHooks *h,
                                 int32_t mode, int32_t slot)
{
    if (slot == -1) {
        for (uint32_t i = 0; i < AE_STAGEFADE_TIM_COUNT; ++i) {
            ae_wr32(r, AE_STAGEFADE_G_TIM_POINTERS +
                       i * AE_STAGEFADE_TIM_POINTER_STRIDE, 0u);
        }
        ae_wr8(r, AE_STAGEFADE_G_TIM_COUNT, 0u);
        return (int32_t)AE_STAGEFADE_TIM_RESET_RETURN;
    }

    if (mode == 0) {
        uint32_t count = ae_rd8(r, AE_STAGEFADE_G_TIM_COUNT);
        if (mode >= (int32_t)count) return 0;
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t resource = ae_rd32(r, AE_STAGEFADE_G_TIM_POINTERS +
                                           i * AE_STAGEFADE_TIM_POINTER_STRIDE);
            if (resource == 0u) continue;

            uint32_t age_address = AE_STAGEFADE_G_TIM_AGES + i;
            uint32_t age = ae_rd8(r, age_address);
            if (age != 0u && age != 0xFFu) {
                age++;
                ae_wr8(r, age_address, age);
            }
            if (age < AE_STAGEFADE_TIM_MAX_AGE) {
                int32_t resource_id = ae_rds16(r, AE_STAGEFADE_G_TIM_IDS +
                                                  i * AE_STAGEFADE_TIM_ID_STRIDE);
                (void)stagefade_hook(r, h,
                        h != NULL ? h->DebugTextPrintf : NULL,
                        "DebugTextPrintf", resource, (uint32_t)resource_id,
                        (uint32_t)mode, h != NULL ? h->caller_a3 : 0u);
                if (ae_ram_failed(r)) return 0;
            }
        }
        return 0;
    }

    uint32_t count = ae_rd8(r, AE_STAGEFADE_G_TIM_COUNT);
    uint32_t pointer = AE_STAGEFADE_G_TIM_POINTERS +
                       count * AE_STAGEFADE_TIM_POINTER_STRIDE;
    uint32_t id = AE_STAGEFADE_G_TIM_IDS + count * AE_STAGEFADE_TIM_ID_STRIDE;
    uint32_t age = AE_STAGEFADE_G_TIM_AGES + count;
    ae_wr32(r, pointer, (uint32_t)mode);
    ae_wr16(r, id, (uint32_t)slot);
    ae_wr8(r, age, 1u);

    uint32_t next_count = count + 1u;
    next_count -= next_count & 0x1F8u;
    ae_wr8(r, AE_STAGEFADE_G_TIM_COUNT,
           next_count);
    return (int32_t)next_count;
}

/* 80095244: start HUD panel A and set its visibility flag. */
int32_t ae_HudPanelA_Start(AeRam *r, int32_t mode)
{
    uint32_t panel = AE_STAGEFADE_G_HUD_PANEL_A;
    ae_wr16(r, panel + AE_STAGEFADE_HUD_PANEL_A_TIMER,
            mode == 0 ? 40u : 0u);
    ae_wr16(r, panel + AE_STAGEFADE_HUD_PANEL_A_STATE,
            mode == 0 ? 0u : 2u);
    ae_wr32(r, panel + AE_STAGEFADE_HUD_PANEL_A_FLAGS,
            ae_rd32(r, panel + AE_STAGEFADE_HUD_PANEL_A_FLAGS) | 4u);
    return 0;
}

/* 80079B10: mode-stage fade transition state machine. */
int32_t ae_StageFadeUpdate(AeRam *r, const AeStageFadeHooks *h)
{
    uint32_t fade_state = ae_rd16(r, AE_STAGEFADE_G_STATE +
                                     AE_STAGEFADE_STATE_FADE_STATE);

    switch (fade_state) {
    case 1: {
        uint32_t state = AE_STAGEFADE_G_STATE;
        uint32_t initial_exit_kind = ae_rd8(r, state + AE_STAGEFADE_STATE_EXIT_KIND);
        if (initial_exit_kind != 1u && initial_exit_kind != 13u &&
            initial_exit_kind != 2u) {
            ae_wr16(r, state + AE_STAGEFADE_STATE_FADE_STATE, 0u);
            return 0;
        }

        ae_ScreenFlashFadeDraw(r, h);
        if (ae_ram_failed(r)) return 0;
        int32_t timer = stagefade_s16(ae_rd16(r, state +
                                                  AE_STAGEFADE_STATE_FADE_TIMER) - 1u);
        ae_wr16(r, state + AE_STAGEFADE_STATE_FADE_TIMER, (uint32_t)timer);
        if (timer > 0) return 0;

        uint32_t exit_kind = ae_rd8(r, state + AE_STAGEFADE_STATE_EXIT_KIND);
        ae_wr16(r, state + AE_STAGEFADE_STATE_FADE_TIMER, 0u);
        ae_wr16(r, state + AE_STAGEFADE_STATE_FADE_STATE, 0u);
        uint32_t next_area = ae_rd16(r, state + AE_STAGEFADE_STATE_NEXT_AREA);
        uint32_t next_entry = ae_rd16(r, state + AE_STAGEFADE_STATE_NEXT_ENTRY);

        if (exit_kind == 1u) {
            int32_t time_attack = stagefade_stage_hook(r, h,
                    h != NULL && h->stage != NULL ? h->stage->IsTimeAttack : NULL,
                    "IsTimeAttack", 0u, 0u, 0u, 0u);
            if (ae_ram_failed(r)) return 0;
            int32_t action = time_attack != 0 ||
                ae_rd16(r, state + AE_STAGEFADE_STATE_LEVEL) == 33u ? 25 : 24;
            (void)stagefade_request(r, h, action, next_area, next_entry);
            return 0;
        }

        (void)stagefade_request(r, h, 3, next_area, next_entry);
        if (ae_ram_failed(r)) return 0;
        if (exit_kind == 13u) {
            (void)ae_HudPanelB_Start(r, 0, 0);
            (void)ae_HudPanelA_Start(r, 0);
            ae_HudShowHealth(r, 0);
        }
        return 0;
    }

    case 2: {
        ae_ScreenFlashFadeDraw(r, h);
        if (ae_ram_failed(r)) return 0;
        uint32_t state = AE_STAGEFADE_G_STATE;
        int32_t timer = stagefade_s16(ae_rd16(r, state +
                                                  AE_STAGEFADE_STATE_FADE_TIMER) + 1u);
        ae_wr16(r, state + AE_STAGEFADE_STATE_FADE_TIMER, (uint32_t)timer);
        if (timer < 48) return 0;

        ae_wr16(r, state + AE_STAGEFADE_STATE_FADE_STATE, 0u);
        ae_wr16(r, state + AE_STAGEFADE_STATE_FADE_TIMER, 0u);
        stagefade_context_hooks(r, h);
        return ae_ram_failed(r) ? 0 : -1;
    }

    case 3: {
        ae_ScreenFlashFadeDraw(r, h);
        if (ae_ram_failed(r)) return 0;
        uint32_t state = AE_STAGEFADE_G_STATE;
        int32_t timer = stagefade_s16(ae_rd16(r, state +
                                                  AE_STAGEFADE_STATE_FADE_TIMER) + 1u);
        ae_wr16(r, state + AE_STAGEFADE_STATE_FADE_TIMER, (uint32_t)timer);
        if (timer < 48) return 0;

        ae_wr16(r, state + AE_STAGEFADE_STATE_FADE_STATE, 0u);
        ae_wr16(r, state + AE_STAGEFADE_STATE_FADE_TIMER, 0u);
        (void)ae_SoundStop(r, h, 2);
        if (ae_ram_failed(r)) return 0;
        stagefade_context_hooks(r, h);
        if (ae_ram_failed(r)) return 0;

        while (ae_rd32(r, AE_STAGEFADE_G_CD_STATE) != 0u) {
            ae_wr32(r, AE_STAGEFADE_G_FRAME_COUNTER,
                    ae_rd32(r, AE_STAGEFADE_G_FRAME_COUNTER) + 1u);
            (void)stagefade_stage_hook(r, h,
                    h != NULL && h->stage != NULL ? h->stage->CdMgrUpdate : NULL,
                    "CdMgrUpdate", 0u, 0u, 0u, 0u);
            if (ae_ram_failed(r)) return 0;
            (void)stagefade_stage_hook(r, h,
                    h != NULL && h->stage != NULL ? h->stage->VSync : NULL,
                    "VSync", 2u, 0u, 0u, 0u);
            if (ae_ram_failed(r)) return 0;
        }

        (void)stagefade_queue_read(r, h);
        if (ae_ram_failed(r)) return 0;
        int32_t load_result;
        do {
            load_result = stagefade_stage_hook(r, h,
                    h != NULL && h->stage != NULL ? h->stage->LoadListStep : NULL,
                    "LoadListStep", 0u, 0u, 0u, 0u);
            if (ae_ram_failed(r)) return 0;
            if (load_result >= 0) {
                (void)stagefade_stage_hook(r, h,
                        h != NULL && h->stage != NULL ? h->stage->CdMgrUpdate : NULL,
                        "CdMgrUpdate", 0u, 0u, 0u, 0u);
                if (ae_ram_failed(r)) return 0;
                (void)stagefade_stage_hook(r, h,
                        h != NULL && h->stage != NULL ? h->stage->VSync : NULL,
                        "VSync", 2u, 0u, 0u, 0u);
                if (ae_ram_failed(r)) return 0;
            }
        } while (load_result >= 0);

        (void)stagefade_request(r, h, 13,
                ae_rd16(r, state + AE_STAGEFADE_STATE_NEXT_AREA),
                ae_rd16(r, state + AE_STAGEFADE_STATE_NEXT_ENTRY));
        return 0;
    }

    case 4: {
        ae_FadeSpiralDraw(r, h);
        if (ae_ram_failed(r)) return 0;
        uint32_t state = AE_STAGEFADE_G_STATE;
        int32_t timer = stagefade_s16(ae_rd16(r, state +
                                                  AE_STAGEFADE_STATE_FADE_TIMER) - 1u);
        ae_wr16(r, state + AE_STAGEFADE_STATE_FADE_TIMER, (uint32_t)timer);
        if (timer > 0) return 0;

        uint32_t next_area = ae_rd16(r, state + AE_STAGEFADE_STATE_NEXT_AREA);
        uint32_t next_entry = ae_rd16(r, state + AE_STAGEFADE_STATE_NEXT_ENTRY);
        ae_wr16(r, state + AE_STAGEFADE_STATE_FADE_TIMER, 0u);
        ae_wr16(r, state + AE_STAGEFADE_STATE_FADE_STATE, 0u);
        (void)stagefade_request(r, h, 3, next_area, next_entry);
        return 0;
    }

    default:
        return 0;
    }
}
