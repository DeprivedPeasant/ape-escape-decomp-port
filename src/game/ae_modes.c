/* Resident game modes, hand ported from the SCUS-94423 instructions. */
#include "ae_modes.h"

#define MODE_STATE 0x800F4350u
#define CURRENT_MODE (MODE_STATE + 0x120u)
#define AREA_KIND (MODE_STATE + 0x122u)
#define CURRENT_LEVEL (MODE_STATE + 0x124u)
#define CURRENT_AREA (MODE_STATE + 0x126u)
#define REQUEST (MODE_STATE + 0x12Cu)
#define STAGE_TRANSITION_COUNT (MODE_STATE + 0x13Cu)
#define MODE_ARGUMENT_A (MODE_STATE + 0x15Au)
#define MODE_ARGUMENT_B (MODE_STATE + 0x15Cu)
#define MINIGAME_SELECTION (MODE_STATE + 0x15Eu)
#define FRAME_START_VBLANK (MODE_STATE + 0x2B0u)
#define DISPLAY_BUFFER_INDEX (MODE_STATE + 0x134u)
#define DISPLAY_BUFFER_PENDING (MODE_STATE + 0x136u)
#define DRAW_STATE 0x800EC350u
#define RENDER_ORDER_TABLE (DRAW_STATE + 8u)
#define RENDER_BUFFER_INDEX (DRAW_STATE + 12u)
#define DRAW_ENV_OFFSET 16u
#define DISPLAY_ENV_OFFSET 108u
#define RENDER_BUFFER_BYTES 0x4078u
#define MODE_TABLE 0x800AFA30u
#define AREA_LOAD_TABLE 0x800A81B0u
#define DEMO_BUFFER 0x800A8A50u
#define MONKEYS 0x800E5500u
#define PLAYER 0x800EC1E0u
#define OBJECTS 0x800E6960u
#define OBJECT_AUX 0x800E68D0u
#define OBJECT_HEADER 0x800E68C0u
#define SPECIAL_ACTOR 0x800F5120u
#define FLIP_STATE 0x800B97C0u
#define DEBUG_SELECTION 0x800B3618u
#define DEBUG_AREA_SELECTION 0x800B361Cu

static int32_t mode_call(AeRam *r, const AeModeHooks *h, const char *name,
                         uint32_t address, uint32_t count, const uint32_t *args) {
    if (!h || !h->original_call) {
        ae_ram_fail(r, "Mode: missing %s boundary", name);
        return 0;
    }
    return h->original_call(h->user, r, name, address, count, args);
}
#define CALL0(name, address) mode_call(r,h,#name,address,0,NULL)
#define CALL1(name, address, a) mode_call(r,h,#name,address,1,(const uint32_t[]){(uint32_t)(a)})
#define CALL2(name, address, a, b) mode_call(r,h,#name,address,2,(const uint32_t[]){(uint32_t)(a),(uint32_t)(b)})
#define CALL3(name, address, a, b, c) mode_call(r,h,#name,address,3,(const uint32_t[]){(uint32_t)(a),(uint32_t)(b),(uint32_t)(c)})
#define CALL4(name, address, a, b, c, d) mode_call(r,h,#name,address,4,(const uint32_t[]){(uint32_t)(a),(uint32_t)(b),(uint32_t)(c),(uint32_t)(d)})

static uint32_t local_buffer(const AeModeHooks *h, uint32_t frame_size) {
    return (h && h->stack_pointer ? h->stack_pointer : 0x801FFF00u) - frame_size + 16u;
}

static void clear_vram(AeRam *r, const AeModeHooks *h, uint32_t template,
                       uint32_t frame_size) {
    uint32_t rectangle = local_buffer(h, frame_size);
    ae_wr32(r, rectangle, ae_rd32(r, template));
    ae_wr32(r, rectangle + 4u, ae_rd32(r, template + 4u));
    CALL0(InitDisplay, 0x8005CFECu);
    if (frame_size == 64u) CALL1(ResetGraph, 0x80028350u, 1);
    CALL4(ClearImage, 0x80028850u, rectangle, 0, 0, 0);
    CALL1(DrawSync, 0x800286CCu, 0);
}

/* 800143B0: a void leaf; the original does not define a return value. */
void ae_NullInit(AeRam *r) { (void)r; }

/* 8008C000: align the low word, then subtract from the fixed heap ceiling.
 * The arithmetic right shift followed by <<2 is exactly this bit mask. */
uint32_t ae_BumpAllocPeek(AeRam *r, uint32_t size) {
    (void)r;
    return 0x800DF798u - ((size + 3u) & ~3u);
}

/* 8005DDE0: game-owned pending flags; presentation stays a native boundary. */
void ae_ResetFrameFlip(AeRam *r) {
    ae_wr16(r, FLIP_STATE + 2u, 0);
    ae_wr16(r, FLIP_STATE, 0);
}

/* 80010B50 */
int32_t ae_StageInitMemory(AeRam *r, const AeModeHooks *h, uint32_t list_index) {
    uint32_t buffer = ae_BumpAllocPeek(r, 0x7800u);
    uint32_t list = ae_rd32(r, AREA_LOAD_TABLE + (list_index << 3));
    ae_wr32(r, DEMO_BUFFER, buffer);
    ae_wr32(r, list + 28u, buffer);
    for (uint32_t slot = 0; slot < 8u; ++slot)
        CALL2(ClearWords, 0x80032708u, MONKEYS + slot * 0x250u, 0x94u);
    CALL2(ClearWords, 0x80032708u, PLAYER, 0x53u);
    for (uint32_t slot = 0; slot < 16u; ++slot)
        CALL2(ClearWords, 0x80032708u, OBJECTS + slot * 0xE4u, 0x39u);
    CALL2(ClearWords, 0x80032708u, OBJECT_AUX, 0x1Fu);
    CALL2(ClearWords, 0x80032708u, OBJECT_HEADER, 4u);
    CALL2(ClearWords, 0x80032708u, SPECIAL_ACTOR, 0x2Au);
    return 0;
}

/* 8005BEE4 */
int32_t ae_Mode_04(AeRam *r) { (void)r; return 0; }

/* 8005BEF0 */
int32_t ae_StageRenderInit(AeRam *r, const AeModeHooks *h) {
    clear_vram(r, h, 0x800A5740u, 32u);
    CALL1(MatrixIdentity, 0x8004637Cu, 0x800E78C0u);
    ae_wr32(r, 0x800EBFC4u, 0);
    ae_wr32(r, RENDER_BUFFER_INDEX, 0);
    return 0;
}

static void wait_load_list(AeRam *r, const AeModeHooks *h) {
    while (!CALL0(LoadListStep, 0x800116C0u) && !ae_ram_failed(r)) {
        CALL0(CdMgrUpdate, 0x800125B8u);
        CALL1(VSync, 0x8002E520u, 2);
    }
}

/* 8005BDC0: the resident dispatcher owns overlay selection and the mode loop. */
int32_t ae_main(AeRam *r, const AeModeHooks *h) {
    ae_NullInit(r);
    CALL1(SetVideoMode, 0x8002EDF0u, 0);
    CALL0(InitSystem, 0x8005CD50u);
    ae_wr16(r, CURRENT_MODE, 0);
    while (!ae_ram_failed(r)) {
        uint32_t entry = MODE_TABLE + ae_rd16(r, CURRENT_MODE) * 8u;
        int32_t member = ae_rds32(r, entry + 4u);
        if (member >= 0) {
            CALL2(ArcLoadMember, 0x800112E4u, member, 0x80136000u);
            CALL1(CdMgrWaitIdle, 0x8001256Cu, 0);
        }
        if (ae_rd16(r, CURRENT_MODE) == 13u) {
            uint32_t request = ae_rd16(r, REQUEST);
            ae_wr16(r, CURRENT_AREA, request == 21u || request == 30u ? 91u : 88u);
            CALL2(LoadListInit, 0x80011104u, 1, ae_rd16(r, CURRENT_AREA));
            wait_load_list(r, h);
        }
        entry = MODE_TABLE + ae_rd16(r, CURRENT_MODE) * 8u;
        if (CALL0(ModeEntry, ae_rd32(r, entry)) < 0) break;
        CALL0(CdMgrUpdate, 0x800125B8u);
    }
    return 0;
}

/* 8008D1F0: resident debug selector / executable launcher. */
int32_t ae_Mode_06(AeRam *r, const AeModeHooks *h) {
    clear_vram(r, h, 0x800A6464u, 64u);
    ae_wr8(r, DRAW_STATE + 0x40A0u, 1);
    ae_wr8(r, DRAW_STATE + 0x28u, 1);
    ae_wr32(r, RENDER_BUFFER_INDEX, 0);
    ae_wr32(r, 0x800B362Cu, ae_rd32(r, 0x800B35D4u + (ae_rd32(r, DEBUG_SELECTION) << 2)));
    uint32_t selected_area_name = ae_rd32(r, 0x800B3600u + (ae_rd32(r, DEBUG_AREA_SELECTION) << 2));
    ae_wr32(r, 0x800B35D0u, 0);
    ae_wr32(r, 0x800B3630u, selected_area_name);
    int32_t selection;
    for (;;) {
        CALL1(PadUpdate, 0x8001325Cu, 0);
        CALL0(BeginFrame, 0x8005DF48u);
        CALL0(ColorFadeReset, 0x8008C390u);
        selection = CALL0(DebugMenuUpdate, 0x8008D50Cu);
        if (selection >= 0 || ae_ram_failed(r)) break;
        CALL1(DrawSync, 0x800286CCu, 0);
        CALL1(VSync, 0x8002E520u, 0);
        uint32_t buffer = ae_rd32(r, RENDER_BUFFER_INDEX) * RENDER_BUFFER_BYTES;
        ae_wr32(r, DRAW_STATE, ae_rd32(r, DRAW_STATE) + 1u);
        CALL1(PutDispEnv, 0x80028EFCu, DRAW_STATE + DISPLAY_ENV_OFFSET + buffer);
        buffer = ae_rd32(r, RENDER_BUFFER_INDEX) * RENDER_BUFFER_BYTES;
        CALL1(PutDrawEnv, 0x80028D30u, DRAW_STATE + DRAW_ENV_OFFSET + buffer);
        buffer = ae_rd32(r, RENDER_BUFFER_INDEX) * RENDER_BUFFER_BYTES;
        CALL1(DrawOTag, 0x80028CC0u, DRAW_STATE + 128u + buffer + 0x3FFCu);
    }
    if (ae_ram_failed(r)) return 0;
    if (selection == 999) {
        ae_wr16(r, CURRENT_MODE, 3);
    } else if (selection < 2) {
        CALL1(RunMinigameExe, 0x80011A2Cu, selection);
    } else if (selection == 2) {
        CALL2(ArcLoadMember, 0x800112E4u, 0x1ABu, 0x80136000u);
        CALL1(CdMgrWaitIdle, 0x8001256Cu, 0);
        CALL0(GalaxyEntry, 0x80137654u);
        ae_wr16(r, CURRENT_MODE, 3);
    } else if (selection == 3) {
        CALL2(ArcLoadMember, 0x800112E4u, 0x180u, 0x80136000u);
        CALL1(CdMgrWaitIdle, 0x8001256Cu, 0);
        CALL1(TitleSequence, 0x80136074u, ae_rd32(r, DEBUG_SELECTION));
        ae_wr16(r, CURRENT_MODE, 3);
    } else if (selection == 4) {
        uint32_t selected_area = ae_rd16(r, DEBUG_AREA_SELECTION);
        ae_wr16(r, CURRENT_AREA, selected_area + 99u);
        ae_wr16(r, CURRENT_MODE, 36);
        ae_wr16(r, MODE_ARGUMENT_B, selected_area);
        ae_StageInitMemory(r, h, ae_rd16(r, CURRENT_AREA));
        CALL2(LoadListInit, 0x80011104u, 1, ae_rd16(r, CURRENT_AREA));
        wait_load_list(r, h);
    }
    return 0;
}

#define STAGE 0x800F4350u
#define STAGE_FLAGS (STAGE + 0x100u)
#define STAGE_LEVEL (STAGE + 0x124u)
#define STAGE_AREA (STAGE + 0x126u)
#define STAGE_ENTRY (STAGE + 0x12Au)
#define STAGE_REQUEST (STAGE + 0x12Cu)
#define PLAYER_MODE (STAGE + 0x150u)
#define PLAYER_LOCK (STAGE + 0x152u)


static int stage_frame_body(AeRam *r, const AeModeHooks *h) {
    CALL1(PadUpdate, 0x8001325Cu, 1); /* PadUpdate */
    ae_wr16(r, 0x800AE8F0u, ae_rd16(r, 0x800AE8F0u) * 5u + 1u);
    ae_wr16(r, 0x800AE8F2u, ae_rd16(r, 0x800AE8F2u) * 5u + 1u);
    if (ae_rd8(r, STAGE_REQUEST) == 6u) {
        CALL0(StageRequestUpdate, 0x8005D890u); /* StageRequestUpdate */
    } else {
        int32_t pause = CALL0(PauseMenuUpdate, 0x800821D4u);
        if (pause == 2 || pause == 3)
            CALL3(StageRequest, 0x8005D100u, pause == 2 ? 28u : 7u,
                         ae_rd16(r, STAGE_AREA), ae_rd16(r, STAGE_ENTRY));
        CALL0(BeginFrame, 0x8005DF48u);
        CALL0(MenuWindowsRun, 0x8007D530u); /* MenuWindowsRun */
        CALL0(MessageBoxUpdateDraw, 0x8008DF4Cu); /* MessageBoxUpdateDraw */
        CALL0(AreaFeature30Update, 0x8009B7FCu);
        ae_wr32(r, DRAW_STATE, ae_rd32(r, DRAW_STATE) + 1u);
        CALL0(ColorFadeReset, 0x8008C390u);
        if (CALL0(StageFadeUpdate, 0x80079B10u) < 0) {
            CALL0(WaitFrameFlip, 0x8005DE8Cu);
            if (ae_rd16(r, AREA_KIND) != 3u)
                CALL1(SoundStop, 0x80086FD0u, 1);
            return 0;
        }
        if (ae_rd32(r, 0x800A8A58u) && (ae_rd32(r, DRAW_STATE) & 0x10u))
            CALL4(DrawTextSprites, 0x80082288u, 0x800AFB60u, 300, 210, 2);
        CALL0(CameraUpdate, 0x80047BD0u); /* CameraUpdate */
        CALL4(SetColorMatrixMode, 0x80015D58u, 1, 0, 0, 0); /* SetColorMatrixMode */
        if (ae_rd32(r, STAGE_FLAGS) & 1u)
            CALL0(PlayerAnimateAndDraw, 0x80063628u); /* PlayerAnimateAndDraw */
        else CALL0(PlayerUpdate, 0x80060780u); /* complete PlayerUpdate port */
        uint32_t special = ae_rd32(r, 0x800F5120u);
        if (special & 0xCu) {
            int variant_b = (special & 8u) != 0;
            if (ae_rd16(r, PLAYER_MODE) != (variant_b ? 3u : 1u)) {
                uint32_t function = ae_rd32(r, variant_b ? 0x800B4DCCu : 0x800B4DD0u);
                CALL1(SpecialActorUpdate, function, 0);
                CALL0(SpecialActorPlayerNear, 0x8009B6C8u); /* SpecialActorPlayerNear */
            }
        }
        if (ae_rd32(r, 0x800F5A90u)) {
            if (!(ae_rd32(r, STAGE_FLAGS) & 1u)) CALL0(RcCarUpdate, 0x800994A8u); /* RcCarUpdate */
            CALL0(RcCarDraw, 0x80099AE0u); /* RcCarDraw */
        }
        CALL0(ProjectileSystemsUpdate, 0x80084AA0u); /* ProjectileSystemsUpdate */
        CALL0(ZakoUpdateAll, 0x80050C34u); /* ZakoUpdateAll */
        CALL0(MonkeyUpdateAll, 0x80042E18u); /* MonkeyUpdateAll */
        CALL0(AreaOverlayUpdate, 0x8005ACE8u); /* AreaOverlayUpdate */
        CALL0(DrawTerrain, 0x80043A8Cu);
        CALL0(ItemsUpdateDraw, 0x80084598u); /* ItemsUpdateDraw */
        CALL0(IndicatorsUpdateDraw, 0x800719F8u); /* IndicatorsUpdateDraw */
        CALL0(DrawAreaProps, 0x80016F34u);
        CALL0(EffectScriptUpdate, 0x80069BECu); /* EffectScriptUpdate */
        CALL0(EffectsUpdateAll, 0x8006A9F8u); /* EffectsUpdateAll */
        CALL0(HudPopupUpdate, 0x800983E8u); /* HudPopupUpdate */
        CALL0(ColliderScriptsUpdate, 0x80088AB8u); /* ColliderScriptsUpdate */
        CALL0(GimmickUpdateAll, 0x8005B68Cu); /* GimmickUpdateAll */
        CALL0(PlayerEyeBlinkUpdate, 0x80067AF0u); /* PlayerEyeBlinkUpdate */
        uint32_t mode = ae_rd16(r, PLAYER_MODE);
        if (mode != 8u && mode != 13u && !(ae_rd32(r, STAGE_FLAGS) & 1u)) CALL0(CollisionUpdate, 0x8007247Cu);
        CALL0(SoundEmitterUpdate, 0x8008686Cu); /* SoundEmitterUpdate */
        CALL0(StageRequestUpdate, 0x8005D890u); /* StageRequestUpdate */
        CALL0(StageDrawScreenOverlay, 0x8005E0F0u); /* StageDrawScreenOverlay */
        CALL0(VramScrollUpdate, 0x800691F8u);
        int32_t vblank = CALL1(VSync, 0x8002E520u, 1);
        uint32_t elapsed = (uint32_t)vblank - ae_rd32(r, FRAME_START_VBLANK);
        if ((int32_t)elapsed <= 0) elapsed += 0xFFFFu;
        ae_wr32(r, 0x800B97B0u, elapsed);
    }
    if (CALL0(PlayerCanExitStage, 0x8005DC78u) &&
        !CALL0(StageExitPending, 0x8004D104u) && ae_rd16(r, PLAYER_LOCK) != 11u &&
        (ae_rd32(r, 0x800EC230u) >> 28) != 8u) {
        uint32_t buttons = ae_rd32(r, 0x800E5478u) & ae_rd32(r, 0x800E5480u);
        uint32_t menu = 0;
        if (buttons & 0x800u) menu = 1;
        else if ((buttons & 0x100u) && ae_rd16(r, STAGE_LEVEL) != 7u && ae_rd16(r, STAGE_LEVEL) != 14u) menu = 10;
        if (menu) {
            CALL0(SoundLoopsPause, 0x8007292Cu); /* SoundLoopsPause */
            CALL1(OpenPauseMenu, 0x8007F56Cu, menu);
            CALL1(PlaySfx, 0x800877F8u, 2);
        }
    }
    /* Presentation is a native boundary, including the transition-only path. */
    if (ae_rd8(r, STAGE_REQUEST) == 6u) {
        CALL1(DrawSync, 0x800286CCu, 0);
        CALL1(VSync, 0x8002E520u, 2);
        uint32_t display_buffer, draw_buffer;
        int pending = ae_rd16(r, DISPLAY_BUFFER_PENDING) != 0;
        if (pending) {
            display_buffer = draw_buffer = ae_rd32(r, RENDER_BUFFER_INDEX) * RENDER_BUFFER_BYTES;
        } else {
            display_buffer = ae_rd16(r, DISPLAY_BUFFER_INDEX) * RENDER_BUFFER_BYTES;
            draw_buffer = (ae_rd16(r, DISPLAY_BUFFER_INDEX) ^ 1u) * RENDER_BUFFER_BYTES;
        }
        CALL1(PutDispEnv, 0x80028EFCu, DRAW_STATE + DISPLAY_ENV_OFFSET + display_buffer);
        draw_buffer = pending ? ae_rd32(r, RENDER_BUFFER_INDEX) * RENDER_BUFFER_BYTES :
                      (ae_rd16(r, DISPLAY_BUFFER_INDEX) ^ 1u) * RENDER_BUFFER_BYTES;
        CALL1(PutDrawEnv, 0x80028D30u, DRAW_STATE + DRAW_ENV_OFFSET + draw_buffer);
        if (pending) ae_wr16(r, DISPLAY_BUFFER_PENDING, 0);
        CALL1(DrawOTag, 0x80028CC0u, ae_rd32(r, RENDER_ORDER_TABLE) + 0x3FFCu);
    } else {
        if (CALL0(IsPauseMenuOpen, 0x8007F5D4u)) {
            uint32_t order_table = ae_rd32(r, RENDER_ORDER_TABLE);
            ae_wr32(r, order_table, ae_rd32(r, order_table) | 0x00FFFFFFu);
        }
        uint32_t buffer = ae_rd32(r, RENDER_BUFFER_INDEX) * RENDER_BUFFER_BYTES;
        CALL4(QueueFrameFlip, 0x8005DEE0u, DRAW_STATE + DRAW_ENV_OFFSET + buffer,
                   DRAW_STATE + DISPLAY_ENV_OFFSET + buffer, ae_rd32(r, RENDER_ORDER_TABLE) + 0x3FFCu, 0);
    }
    return !ae_ram_failed(r);
}

/* The fade exit belongs to Mode_Stage, including the post-loop mode changes. */
static void stage_exit(AeRam *r, const AeModeHooks *h) {
    uint32_t area = ae_rd16(r, CURRENT_AREA);
    if (area == 19u || area == 36u || area - 92u < 7u)
        CALL0(GadgetSlotsRestore, 0x8007CA10u);
    switch (ae_rd16(r, REQUEST)) {
    case 8:
        if (ae_rd16(r, AREA_KIND) == 3u) {
            CALL2(ProgressMarkStageResult, 0x800963DCu, ae_rd16(r, CURRENT_AREA), 8);
            CALL0(BgmUpdateVolume, 0x8008745Cu);
        }
        CALL0(RecordBestTime, 0x800961D0u);
        CALL0(SaveBufferFill, 0x80095B3Cu);
        if (CALL0(IsTimeAttack, 0x80097678u)) {
            uint32_t next = 25u;
            if (ae_rd16(r, CURRENT_MODE) == 19u && ae_rd16(r, AREA_KIND) != 3u)
                next = 20u;
            ae_wr16(r, CURRENT_MODE, next);
        } else ae_wr16(r, CURRENT_MODE, 12);
        break;
    case 29:
        CALL0(SaveBufferFill, 0x80095B3Cu);
        ae_wr16(r, CURRENT_MODE, 30);
        break;
    case 9:
        ae_wr16(r, STAGE_TRANSITION_COUNT, 5);
        CALL0(SaveBufferFill, 0x80095B3Cu);
        ae_wr16(r, CURRENT_MODE, 15);
        break;
    case 21:
        CALL0(SaveCaptureState, 0x80095CECu);
        CALL1(RunMinigameExe, 0x80011A2Cu, ae_rd16(r, MINIGAME_SELECTION));
        ae_wr16(r, CURRENT_MODE, 13);
        break;
    case 30:
        CALL0(SaveCaptureState, 0x80095CECu);
        ae_wr16(r, CURRENT_MODE, 33);
        break;
    case 23:
        CALL0(SaveCaptureState, 0x80095CECu);
        CALL0(SaveHeaderFill, 0x80095D44u);
        ae_wr16(r, CURRENT_MODE, 21);
        break;
    case 22:
        CALL0(UnlockSpecialArea, 0x80097820u);
        CALL0(SaveBufferFill, 0x80095B3Cu);
        CALL0(ExitSpecialArea, 0x800978FCu);
        break;
    case 26:
        CALL2(ProgressMarkStageResult, 0x800963DCu, ae_rd16(r, CURRENT_AREA), 26);
        CALL0(SaveBufferFill, 0x80095B3Cu);
        CALL0(BgmUpdateVolume, 0x8008745Cu);
        ae_wr16(r, CURRENT_MODE, 25);
        break;
    case 7: {
        uint32_t mode = ae_rd16(r, CURRENT_MODE);
        if (mode == 11u) CALL0(SaveBufferFill, 0x80095B3Cu);
        else if (mode == 19u) CALL0(SaveHeaderFill, 0x80095D44u);
        else if (mode == 24u) {
            CALL2(ProgressMarkStageResult, 0x800963DCu, ae_rd16(r, CURRENT_AREA), 7);
            CALL0(SaveBufferFill, 0x80095B3Cu);
            ae_wr16(r, CURRENT_MODE, 13);
            break;
        } else if (mode == 13u) {
            if (ae_rd16(r, CURRENT_AREA) - 92u < 7u) {
                ae_wr16(r, CURRENT_MODE, mode);
                break;
            }
            CALL0(SaveCaptureState, 0x80095CECu);
        }
        ae_wr16(r, CURRENT_MODE, 9);
        break;
    }
    case 27:
        if (ae_rd16(r, CURRENT_MODE) == 11u) CALL0(SaveBufferFill, 0x80095B3Cu);
        else if (ae_rd16(r, CURRENT_MODE) == 19u) CALL0(SaveHeaderFill, 0x80095D44u);
        ae_wr16(r, CURRENT_MODE, 26);
        ae_wr16(r, MODE_ARGUMENT_A, 3);
        break;
    case 31:
        CALL0(SaveCaptureState, 0x80095CECu);
        ae_wr16(r, CURRENT_MODE, 26);
        ae_wr16(r, MODE_ARGUMENT_A, 8);
        break;
    case 28: {
        CALL0(SaveHeaderFill, 0x80095D44u);
        uint32_t level = ae_rd16(r, CURRENT_LEVEL);
        if (level - 24u < 6u) ae_wr16(r, CURRENT_LEVEL, 24);
        uint32_t first = (uint32_t)CALL1(GetLevelFirstArea, 0x8009764Cu, ae_rd16(r, CURRENT_LEVEL));
        ae_wr16(r, CURRENT_AREA, first);
        if ((first & 0xFFFFu) == 19u || (first & 0xFFFFu) == 36u) {
            CALL1(SoundStop, 0x80086FD0u, 1);
            CALL1(VSync, 0x8002E520u, 0);
            CALL2(ArcLoadMember, 0x800112E4u, 27, 0x80146000u);
            CALL1(CdMgrWaitIdle, 0x8001256Cu, 0);
            CALL1(SoundBankUpload, 0x80086CACu, 2);
        }
        CALL2(LoadListInit, 0x80011104u, 1, ae_rd16(r, CURRENT_AREA));
        wait_load_list(r, h);
        break;
    }
    case 32:
        ae_wr16(r, CURRENT_MODE, 26);
        ae_wr16(r, MODE_ARGUMENT_A, 2);
        break;
    case 34:
        ae_wr16(r, CURRENT_MODE, 3);
        break;
    default:
        CALL0(SaveBufferFill, 0x80095B3Cu);
        ae_wr16(r, CURRENT_MODE, 13);
        break;
    }
    CALL1(DrawSync, 0x800286CCu, 0);
    CALL1(VSync, 0x8002E520u, 0);
}

void ae_mode_stage_begin(AeRam *r, const AeModeHooks *h) {
    AeModeHooks nested = {0};
    if (h) nested = *h;
    nested.stack_pointer = local_buffer(h, 40u) - 16u;
    ae_StageRenderInit(r, &nested);
    CALL3(StageRequest, 0x8005D100u, 1, ae_rd16(r, CURRENT_AREA), 0);
    ae_ResetFrameFlip(r);
}

int ae_mode_stage_frame(AeRam *r, const AeModeHooks *h) {
    if (stage_frame_body(r, h)) return 1;
    if (!ae_ram_failed(r)) stage_exit(r, h);
    return 0;
}

/* 8005BF70: the original loop remains here; the host shares the same body. */
int32_t ae_Mode_Stage(AeRam *r, const AeModeHooks *h) {
    ae_mode_stage_begin(r, h);
    while (!ae_ram_failed(r) && ae_mode_stage_frame(r, h)) { }
    return 0;
}
