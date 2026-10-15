#include "ae_ovl181.h"
#include "ae_overlay_layout.h"

enum {
    AE_ADDR_SET_DISP_MASK = 0x80028634u,
    AE_ADDR_RESET_GRAPH = 0x80028350u,
    AE_ADDR_STOP_CALLBACK = 0x8002E824u,
    AE_ADDR_SET_GRAPH_DEBUG = 0x800284C4u,
    AE_ADDR_CLEAR_IMAGE = 0x80028850u,
    AE_ADDR_CLEAR_IMAGE2 = 0x800288E0u,
    AE_ADDR_DRAW_SYNC = 0x800286CCu,
    AE_ADDR_VSYNC = 0x8002E520u,
    AE_ADDR_ARC_LOAD_ENTRY = 0x80011194u,
    AE_ADDR_ARC_LOAD_MEMBER = 0x800112E4u,
    AE_ADDR_LOAD_LIST_START = 0x80011104u,
    AE_ADDR_LOAD_LIST_STEP = 0x800116C0u,
    AE_ADDR_CD_UPDATE = 0x800125B8u,
    AE_ADDR_CD_WAIT_IDLE = 0x8001256Cu,
    AE_ADDR_LOAD_TIM_LIST = 0x80068D10u,
    AE_ADDR_INIT_DISPLAY = 0x8005CFECu,
    AE_ADDR_BEGIN_FRAME = 0x8005DF48u,
    AE_ADDR_PUT_DRAW_ENV = 0x80028D30u,
    AE_ADDR_PUT_DISP_ENV = 0x80028EFCu,
    AE_ADDR_DRAW_OTAG = 0x80028CC0u,
    AE_ADDR_SET_GEOM_OFFSET = 0x8002FAD0u,
    AE_ADDR_SET_DEF_DRAW_ENV = 0x8002B280u,
    AE_ADDR_SET_DEF_DISP_ENV = 0x8002B340u,
    AE_ADDR_GET_VIDEO_MODE = 0x8002EE04u,
    AE_ADDR_EXIT = 0x800A3700u,
    AE_ADDR_RUN_MINIGAME = 0x80011A2Cu,
    AE_ADDR_SOUND_STOP = 0x80086FD0u
};

enum {
    AE_BOOT_READY_LIMIT = 240,
    AE_LOAD_WAIT_LIMIT = 210,
    AE_LOGO_A_FRAMES = 361,
    AE_LOGO_B_FRAMES = 721
};

static uint32_t ae_overlay_words(uint32_t *words, uint32_t a0, uint32_t a1,
                                 uint32_t a2, uint32_t a3, uint32_t a4,
                                 uint32_t a5, uint32_t a6)
{
    words[0] = a0; words[1] = a1; words[2] = a2; words[3] = a3;
    words[4] = a4; words[5] = a5; words[6] = a6;
    return 0u;
}

static int32_t ae_ovl181_call(AeRam *ram, const AeOverlayHooks *hooks,
                             const char *name, uint32_t address, uint32_t count,
                             uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3,
                             uint32_t a4, uint32_t a5, uint32_t a6)
{
    uint32_t words[7];
    (void)ae_overlay_words(words, a0, a1, a2, a3, a4, a5, a6);
    return ae_overlay_call(ram, hooks, name, address, count, words);
}

static int32_t ae_ovl181_native_call(AeRam *ram, const AeOverlayHooks *hooks,
                                    const char *name, uint32_t address,
                                    uint32_t count, uint32_t a0, uint32_t a1,
                                    uint32_t a2, uint32_t a3, uint32_t a4,
                                    uint32_t a5, uint32_t a6)
{
    uint32_t words[7];
    (void)ae_overlay_words(words, a0, a1, a2, a3, a4, a5, a6);
    return ae_overlay_native_call(ram, hooks, name, address, count, words);
}

#define CALL0(r,h,n,a) ae_ovl181_call((r),(h),(n),(a),0u,0u,0u,0u,0u,0u,0u,0u)
#define CALL1(r,h,n,a,x) ae_ovl181_call((r),(h),(n),(a),1u,(uint32_t)(x),0u,0u,0u,0u,0u,0u)
#define CALL2(r,h,n,a,x,y) ae_ovl181_call((r),(h),(n),(a),2u,(uint32_t)(x),(uint32_t)(y),0u,0u,0u,0u,0u)
#define CALL3(r,h,n,a,x,y,z) ae_ovl181_call((r),(h),(n),(a),3u,(uint32_t)(x),(uint32_t)(y),(uint32_t)(z),0u,0u,0u,0u)
#define CALL4(r,h,n,a,x,y,z,w) ae_ovl181_call((r),(h),(n),(a),4u,(uint32_t)(x),(uint32_t)(y),(uint32_t)(z),(uint32_t)(w),0u,0u,0u)
#define CALL5(r,h,n,a,x,y,z,w,q) ae_ovl181_call((r),(h),(n),(a),5u,(uint32_t)(x),(uint32_t)(y),(uint32_t)(z),(uint32_t)(w),(uint32_t)(q),0u,0u)
#define NATIVE0(r,h,n,a) ae_ovl181_native_call((r),(h),(n),(a),0u,0u,0u,0u,0u,0u,0u,0u)
#define NATIVE1(r,h,n,a,x) ae_ovl181_native_call((r),(h),(n),(a),1u,(uint32_t)(x),0u,0u,0u,0u,0u,0u)
#define NATIVE2(r,h,n,a,x,y) ae_ovl181_native_call((r),(h),(n),(a),2u,(uint32_t)(x),(uint32_t)(y),0u,0u,0u,0u,0u)
#define NATIVE3(r,h,n,a,x,y,z) ae_ovl181_native_call((r),(h),(n),(a),3u,(uint32_t)(x),(uint32_t)(y),(uint32_t)(z),0u,0u,0u,0u)
#define NATIVE4(r,h,n,a,x,y,z,w) ae_ovl181_native_call((r),(h),(n),(a),4u,(uint32_t)(x),(uint32_t)(y),(uint32_t)(z),(uint32_t)(w),0u,0u,0u)
#define NATIVE5(r,h,n,a,x,y,z,w,q) ae_ovl181_native_call((r),(h),(n),(a),5u,(uint32_t)(x),(uint32_t)(y),(uint32_t)(z),(uint32_t)(w),(uint32_t)(q),0u,0u)

static void ae_ovl181_rect_at(AeRam *ram, uint32_t address, uint32_t x,
                              uint32_t y, uint32_t width, uint32_t height)
{
    ae_wr16(ram, address + AE_RECT_X_OFFSET, x);
    ae_wr16(ram, address + AE_RECT_Y_OFFSET, y);
    ae_wr16(ram, address + AE_RECT_WIDTH_OFFSET, width);
    ae_wr16(ram, address + AE_RECT_HEIGHT_OFFSET, height);
}

static void ae_ovl181_rect(AeRam *ram, uint32_t x, uint32_t y,
                           uint32_t width, uint32_t height)
{
    ae_ovl181_rect_at(ram, AE_OVERLAY_LOCAL_RECT, x, y, width, height);
}

static int32_t ae_ovl181_clear_screen_rect(AeRam *ram, const AeOverlayHooks *hooks,
                                          uint32_t y)
{
    ae_ovl181_rect(ram, 0u, y, 640u, 256u);
    return CALL4(ram, hooks, "ClearImage2", AE_ADDR_CLEAR_IMAGE2,
                 AE_OVERLAY_LOCAL_RECT, 0u, 0u, 0u);
}

static int32_t ae_ovl181_load_until_done(AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t status;
    do {
        status = CALL0(ram, hooks, "LoadListStep", AE_ADDR_LOAD_LIST_STEP);
        if (status == 0) {
            CALL0(ram, hooks, "CdMgrUpdate", AE_ADDR_CD_UPDATE);
            CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 2u);
        }
    } while (status == 0);
    return status;
}

int32_t ae_Boot_SetupDisplay(AeRam *ram, const AeOverlayHooks *hooks)
{
    CALL2(ram, hooks, "SetGeomOffset", AE_ADDR_SET_GEOM_OFFSET, 192u, 120u);
    for (uint32_t buffer = 0; buffer < 2u; ++buffer) {
        uint32_t draw_env = AE_DISPLAY_SETUP_STORAGE + buffer * AE_DISPLAY_BUFFER_STRIDE;
        CALL5(ram, hooks, "SetDefDrawEnv", AE_ADDR_SET_DEF_DRAW_ENV,
              draw_env, 0u, 0u, 640u, 480u);
    }
    for (uint32_t buffer = 0; buffer < 2u; ++buffer) {
        uint32_t draw_env = AE_DISPLAY_SETUP_STORAGE + buffer * AE_DISPLAY_BUFFER_STRIDE;
        uint32_t display_env = draw_env + AE_DISPLAY_ENV_SIZE;
        CALL5(ram, hooks, "SetDefDispEnv", AE_ADDR_SET_DEF_DISP_ENV,
              display_env, 0u, 0u, 640u, 480u);
    }

    ae_wr8(ram, AE_BOOT_GFX_FLAG_A, 1u);
    ae_wr8(ram, AE_BOOT_GFX_FLAG_B, 1u);
    ae_wr8(ram, AE_BOOT_GFX_FLAG_C, 1u);
    ae_wr8(ram, AE_BOOT_GFX_FLAG_D, 1u);
    ae_wr8(ram, AE_BOOT_GFX_FLAG_E, 0u);
    ae_wr8(ram, AE_BOOT_GFX_FLAG_F, 0u);
    ae_wr32(ram, AE_DISPLAY_TIM_POINTER, 0x801A8000u);
    ae_wr32(ram, AE_DISPLAY_OT_POINTER, 0x801D0000u);
    for (uint32_t offset = 0; offset < 3u; ++offset) {
        ae_wr8(ram, AE_DISPLAY_SETUP_STORAGE + AE_DISPLAY_ENV_RESERVED_START + offset, 0u);
        ae_wr8(ram, AE_DISPLAY_SETUP_STORAGE + AE_DISPLAY_BUFFER_STRIDE +
                     AE_DISPLAY_ENV_RESERVED_START + offset, 0u);
    }
    CALL1(ram, hooks, "SetDispMask", AE_ADDR_SET_DISP_MASK, 1u);
    return 0;
}

int32_t ae_Boot_ClearScreen(AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t result;
    ae_ovl181_rect(ram, 0u, 0u, 640u, 256u);
    CALL4(ram, hooks, "ClearImage2", AE_ADDR_CLEAR_IMAGE2,
          AE_OVERLAY_LOCAL_RECT, 0u, 0u, 0u);
    CALL1(ram, hooks, "DrawSync", AE_ADDR_DRAW_SYNC, 0u);
    ae_ovl181_rect(ram, 0u, 256u, 640u, 256u);
    CALL4(ram, hooks, "ClearImage2", AE_ADDR_CLEAR_IMAGE2,
          AE_OVERLAY_LOCAL_RECT, 0u, 0u, 0u);
    CALL1(ram, hooks, "DrawSync", AE_ADDR_DRAW_SYNC, 0u);
    CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0u);
    CALL2(ram, hooks, "LoadListStart", AE_ADDR_LOAD_LIST_START, 0u, 0u);

    uint32_t display_pointer = ae_rd32(ram, AE_DISPLAY_OT_POINTER);
    ae_wr32(ram, AE_DISPLAY_INDEX, 1u);
    ae_wr32(ram, AE_DISPLAY_CURRENT_ENV, display_pointer);
    CALL1(ram, hooks, "PutDispEnv", AE_ADDR_PUT_DISP_ENV,
          AE_DISPLAY_ENV_BUFFER_B);
    uint32_t draw_index = ae_rd32(ram, AE_DISPLAY_INDEX);
    CALL1(ram, hooks, "PutDrawEnv", AE_ADDR_PUT_DRAW_ENV,
          AE_DRAW_ENV_TABLE + draw_index * AE_DISPLAY_BUFFER_STRIDE);

    uint32_t load_done = 1u;
    int32_t wait_frames = -1;
    for (;;) {
        CALL1(ram, hooks, "PadUpdate", 0x8001325Cu, 0u);
        CALL0(ram, hooks, "BeginFrame", AE_ADDR_BEGIN_FRAME);
        if (load_done != 0u) {
            result = CALL0(ram, hooks, "LoadListStep", AE_ADDR_LOAD_LIST_STEP);
            if (result < 0) load_done = 0u;
        }
        if (ae_rd32(ram, AE_BOOT_DISPLAY_STATE) != 0u) {
            if (wait_frames >= 0) ++wait_frames;
            else wait_frames = 1;
        }
        if (wait_frames >= AE_LOAD_WAIT_LIMIT + 1 && load_done == 0u) break;
        CALL0(ram, hooks, "CdMgrUpdate", AE_ADDR_CD_UPDATE);
        CALL1(ram, hooks, "DrawSync", AE_ADDR_DRAW_SYNC, 0u);
        CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0u);
    }
    return 0;
}

int32_t ae_Boot_DrawLogoA(AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t status;
    ae_ovl181_clear_screen_rect(ram, hooks, 0u);
    CALL1(ram, hooks, "DrawSync", AE_ADDR_DRAW_SYNC, 0u);
    ae_ovl181_clear_screen_rect(ram, hooks, 256u);
    CALL1(ram, hooks, "DrawSync", AE_ADDR_DRAW_SYNC, 0u);
    CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0u);
    CALL2(ram, hooks, "LoadListStart", AE_ADDR_LOAD_LIST_START, 0u, 6u);

    int32_t frame = -1;
    uint32_t load_done = 1u;
    do {
        CALL1(ram, hooks, "PadUpdate", 0x8001325Cu, 0u);
        CALL0(ram, hooks, "BeginFrame", AE_ADDR_BEGIN_FRAME);
        if (load_done != 0u) {
            status = CALL0(ram, hooks, "LoadListStep", AE_ADDR_LOAD_LIST_STEP);
            if (status < 0) load_done = 0u;
        }
        uint32_t screen_state = ae_rd32(ram, AE_BOOT_DISPLAY_STATE);
        if (screen_state != 6u) {
            if (frame >= 0) ++frame;
            else frame = 1;
        }
        if (frame >= AE_LOGO_A_FRAMES) break;

        CALL0(ram, hooks, "CdMgrUpdate", AE_ADDR_CD_UPDATE);
        CALL1(ram, hooks, "DrawSync", AE_ADDR_DRAW_SYNC, 0u);
        CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0u);
        uint32_t index = ae_rd32(ram, AE_DISPLAY_INDEX);
        CALL1(ram, hooks, "PutDispEnv", AE_ADDR_PUT_DISP_ENV,
              AE_DISPLAY_ENV_TABLE + index * AE_DISPLAY_BUFFER_STRIDE);
        CALL1(ram, hooks, "PutDrawEnv", AE_ADDR_PUT_DRAW_ENV,
              AE_DRAW_ENV_TABLE + index * AE_DISPLAY_BUFFER_STRIDE);
        CALL1(ram, hooks, "DrawOTag", AE_ADDR_DRAW_OTAG,
              AE_ORDERING_TABLE + index * AE_DISPLAY_BUFFER_STRIDE + AE_DISPLAY_OT_END_OFFSET);
    } while (1);
    return 0;
}

int32_t ae_Boot_DrawLogoB(AeRam *ram, const AeOverlayHooks *hooks)
{
    ae_ovl181_clear_screen_rect(ram, hooks, 0u);
    CALL1(ram, hooks, "DrawSync", AE_ADDR_DRAW_SYNC, 0u);
    ae_ovl181_clear_screen_rect(ram, hooks, 256u);
    CALL1(ram, hooks, "DrawSync", AE_ADDR_DRAW_SYNC, 0u);
    CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0u);
    CALL2(ram, hooks, "ArcLoadMember", AE_ADDR_ARC_LOAD_MEMBER, 34u,
          AE_LOGO_B_TIM_DESTINATION);
    CALL1(ram, hooks, "CdMgrWaitIdle", AE_ADDR_CD_WAIT_IDLE, 0u);
    CALL1(ram, hooks, "LoadTimList", AE_ADDR_LOAD_TIM_LIST, AE_LOGO_B_TIM_DESTINATION);
    CALL1(ram, hooks, "DrawSync", AE_ADDR_DRAW_SYNC, 0u);
    CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 2u);

    int32_t frame = 0;
    do {
        CALL1(ram, hooks, "PadUpdate", 0x8001325Cu, 0u);
        CALL0(ram, hooks, "BeginFrame", AE_ADDR_BEGIN_FRAME);
        ++frame;
        if (frame >= AE_LOGO_B_FRAMES) break;
        CALL0(ram, hooks, "CdMgrUpdate", AE_ADDR_CD_UPDATE);
        CALL1(ram, hooks, "DrawSync", AE_ADDR_DRAW_SYNC, 0u);
        CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0u);
        uint32_t index = ae_rd32(ram, AE_DISPLAY_INDEX);
        CALL1(ram, hooks, "PutDispEnv", AE_ADDR_PUT_DISP_ENV,
              AE_DISPLAY_ENV_TABLE + index * AE_DISPLAY_BUFFER_STRIDE);
        CALL1(ram, hooks, "PutDrawEnv", AE_ADDR_PUT_DRAW_ENV,
              AE_DRAW_ENV_TABLE + index * AE_DISPLAY_BUFFER_STRIDE);
        CALL1(ram, hooks, "DrawOTag", AE_ADDR_DRAW_OTAG,
              AE_ORDERING_TABLE + index * AE_DISPLAY_BUFFER_STRIDE + AE_DISPLAY_OT_END_OFFSET);
    } while (1);
    return 0;
}

int32_t ae_Mode_Boot(AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t status;
    CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0u);
    int32_t start_vblank = CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0xFFFFFFFFu);
    (void)CALL1(ram, hooks, "ArcLoadEntry", AE_ADDR_ARC_LOAD_ENTRY, AE_BOOT_LOAD_ENTRY);
    (void)ae_ovl181_load_until_done(ram, hooks);

    do {
        int32_t current = CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0xFFFFFFFFu);
        if ((int32_t)(current - start_vblank) >= AE_BOOT_READY_LIMIT) break;
        CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0u);
    } while (1);

    CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0u);
    CALL1(ram, hooks, "SetDispMask", AE_ADDR_SET_DISP_MASK, 0u);
    CALL1(ram, hooks, "ResetGraph", AE_ADDR_RESET_GRAPH, 0u);
    CALL1(ram, hooks, "SetGraphDebug", AE_ADDR_SET_GRAPH_DEBUG, 0u);
    ae_ovl181_rect(ram, 0u, 0u, 640u, 256u);
    CALL4(ram, hooks, "ClearImage", AE_ADDR_CLEAR_IMAGE, AE_OVERLAY_LOCAL_RECT, 0u, 0u, 0u);
    ae_ovl181_rect(ram, 0u, 256u, 640u, 256u);
    CALL4(ram, hooks, "ClearImage", AE_ADDR_CLEAR_IMAGE, AE_OVERLAY_LOCAL_RECT, 0u, 0u, 0u);
    CALL1(ram, hooks, "DrawSync", AE_ADDR_DRAW_SYNC, 0u);
    CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 0u);
    ae_wr16(ram, AE_GAME_BOOT_FIELD_164, 0u);
    ae_ovl181_rect(ram, 0u, 0u, 640u, 512u);
    ae_Boot_SetupDisplay(ram, hooks);
    CALL0(ram, hooks, "SoundStopAndRestoreVolume", 0x80086170u);
    status = ae_Boot_ClearScreen(ram, hooks);
    if (status < 0) return 0;
    status = ae_Boot_DrawLogoA(ram, hooks);
    if (status < 0) return 0;
    if (ae_rd8(ram, AE_BOOT_INPUT) == 0u &&
        (ae_rd8(ram, AE_BOOT_INPUT_STATUS) >> 4) == 7u) {
        /* The boot ROM skips the second logo for this controller status. */
    } else {
        status = ae_Boot_DrawLogoB(ram, hooks);
        if (status < 0) return 0;
    }
    CALL1(ram, hooks, "SoundStop", AE_ADDR_SOUND_STOP, 1u);
    ae_ovl181_rect(ram, 0u, 0u, 640u, 256u);
    CALL4(ram, hooks, "ClearImage2", AE_ADDR_CLEAR_IMAGE2, AE_OVERLAY_LOCAL_RECT, 0u, 0u, 0u);
    CALL1(ram, hooks, "DrawSync", AE_ADDR_DRAW_SYNC, 0u);
    CALL0(ram, hooks, "InitDisplay", AE_ADDR_INIT_DISPLAY);
    uint32_t display_index = ae_rd32(ram, AE_DISPLAY_INDEX);
    CALL1(ram, hooks, "PutDispEnv", AE_ADDR_PUT_DISP_ENV,
          AE_DISPLAY_ENV_TABLE + display_index * AE_DISPLAY_BUFFER_STRIDE);
    CALL1(ram, hooks, "PutDrawEnv", AE_ADDR_PUT_DRAW_ENV,
          AE_DRAW_ENV_TABLE + display_index * AE_DISPLAY_BUFFER_STRIDE);
    CALL2(ram, hooks, "LoadListStart", AE_ADDR_LOAD_LIST_START, 0u, 12u);
    (void)ae_ovl181_load_until_done(ram, hooks);
    ae_wr16(ram, AE_GAME_MODE, 26u);
    ae_wr16(ram, AE_GAME_BOOT_FIELD_15A, 2u);
    ae_wr16(ram, AE_GAME_BOOT_FIELD_15C, 4u);
    return 0;
}

int32_t ae_Mode_02(AeRam *ram, const AeOverlayHooks *hooks)
{
    ae_ovl181_rect(ram, 0u, 0u, 640u, 256u);
    CALL4(ram, hooks, "ClearImage", AE_ADDR_CLEAR_IMAGE, AE_OVERLAY_LOCAL_RECT, 0u, 0u, 0u);
    CALL1(ram, hooks, "DrawSync", AE_ADDR_DRAW_SYNC, 0u);
    CALL1(ram, hooks, "RunMinigameExe", AE_ADDR_RUN_MINIGAME, 1u);
    ae_wr16(ram, AE_GAME_MODE, 3u);
    return 0;
}

int32_t ae_Ovl181_DrawIntegrityWarning(AeRam *ram,
                                       const AeOverlayHooks *hooks)
{
    uint32_t draw_env = AE_OVL181_WARNING_DRAW_ENV;
    uint32_t display_env = AE_OVL181_WARNING_DISPLAY_ENV;
    uint32_t packet = AE_OVL181_WARNING_PACKET;
    uint32_t text_rect = AE_OVL181_WARNING_TEXT_RECT;

    (void)NATIVE1(ram, hooks, "ResetGraph", AE_ADDR_RESET_GRAPH, 3u);
    (void)NATIVE1(ram, hooks, "StopCallback", AE_ADDR_STOP_CALLBACK, 3u);
    (void)NATIVE1(ram, hooks, "ResetGraph", AE_ADDR_RESET_GRAPH, 0u);
    (void)NATIVE5(ram, hooks, "SetDefDrawEnv", AE_ADDR_SET_DEF_DRAW_ENV,
                  draw_env, 0u, 0u, 320u, 240u);
    (void)NATIVE5(ram, hooks, "SetDefDispEnv", AE_ADDR_SET_DEF_DISP_ENV,
                  display_env, 0u, 0u, 320u, 240u);

    if (NATIVE0(ram, hooks, "GetVideoMode", AE_ADDR_GET_VIDEO_MODE) == 1) {
        ae_wr16(ram, display_env + AE_OVL181_WARNING_DISPLAY_MODE_OFFSET,
                17u);
    }
    ae_wr32(ram, draw_env + AE_OVL181_WARNING_DRAW_ENV_FLAGS_OFFSET, 1u);
    (void)NATIVE1(ram, hooks, "PutDrawEnv", AE_ADDR_PUT_DRAW_ENV, draw_env);
    (void)NATIVE1(ram, hooks, "PutDispEnv", AE_ADDR_PUT_DISP_ENV, display_env);

    const uint32_t packet_args[] = {packet};
    (void)ae_overlay_render(ram, hooks, "Ovl181_IntegrityWarningClearPacket",
                            1u, packet_args);

    ae_wr32(ram, AE_OVL181_WARNING_COLOR, 0xFFFFu);
    for (int32_t line = 0; line < 2; ++line) {
        ae_ovl181_rect_at(ram, text_rect, (uint32_t)(line + 80),
                          (uint32_t)(line + 92), 16u, 15u);
        const uint32_t text_args[] = {AE_OVL181_WARNING_TEXT, text_rect};
        (void)ae_overlay_render(ram, hooks, "Ovl181_DrawEncodedWarningText",
                                2u, text_args);
        ae_wr32(ram, AE_OVL181_WARNING_COLOR, 0x8000u);
    }

    (void)ae_overlay_render(ram, hooks, "Ovl181_BuildWarningBackdrop", 0u,
                            NULL);
    (void)NATIVE1(ram, hooks, "SetDispMask", AE_ADDR_SET_DISP_MASK, 1u);
    return NATIVE0(ram, hooks, "Exit", AE_ADDR_EXIT);
}

int32_t ae_Ovl181_CheckCdHardwareIntegrity(AeRam *ram,
                                           const AeOverlayHooks *hooks,
                                           uint32_t arg0, uint32_t arg1,
                                           uint32_t arg2)
{
    ae_wr8(ram, AE_OVL181_CD_PHASE, 32u);
    int32_t failed = NATIVE1(ram, hooks, "Ovl181_CdCommand",
                             AE_OVL181_CD_COMMAND, 5u) < 0;
    if (!failed) {
        failed = NATIVE1(ram, hooks, "Ovl181_CdCommand",
                         AE_OVL181_CD_COMMAND, 0u) < 0;
    }

    uint32_t status = ae_rd8(ram, AE_OVL181_CD_STATUS);
    if (!failed && (status & 0xF0u) != 0u) {
        failed = 1;
    }

    if (!failed) {
        if ((status & 0x02u) == 0u) {
            failed = NATIVE1(ram, hooks, "Ovl181_CdCommand",
                             AE_OVL181_CD_COMMAND, 1u) < 0;
        }
    }

    if (!failed) {
        ae_wr8(ram, AE_OVL181_CD_ARGUMENT0, (uint8_t)arg0);
        ae_wr8(ram, AE_OVL181_CD_ARGUMENT1, (uint8_t)arg1);
        ae_wr8(ram, AE_OVL181_CD_ARGUMENT2, (uint8_t)arg2);
        failed = NATIVE1(ram, hooks, "Ovl181_CdCommand",
                         AE_OVL181_CD_COMMAND, 2u) < 0;
    }

    if (!failed) {
        ae_wr8(ram, AE_OVL181_CD_PHASE, 1u);
        failed = NATIVE1(ram, hooks, "Ovl181_CdCommand",
                         AE_OVL181_CD_COMMAND, 4u) < 0;
    }
    if (!failed) {
        (void)CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 3u);
        failed = NATIVE1(ram, hooks, "Ovl181_CdCommand",
                         AE_OVL181_CD_COMMAND, 3u) < 0;
    }
    if (!failed) {
        failed = NATIVE1(ram, hooks, "Ovl181_CdCommand",
                         AE_OVL181_CD_COMMAND, 6u) < 0;
    }
    if (!failed) {
        failed = NATIVE1(ram, hooks, "Ovl181_CdCommand",
                         AE_OVL181_CD_COMMAND, 7u) < 0;
    }
    if (!failed) {
        ae_wr8(ram, AE_OVL181_CD_PHASE, 4u);
        failed = NATIVE1(ram, hooks, "Ovl181_CdCommand",
                         AE_OVL181_CD_COMMAND, 8u) < 0;
    }
    if (!failed) {
        (void)CALL1(ram, hooks, "VSync", AE_ADDR_VSYNC, 200u);
        ae_wr8(ram, AE_OVL181_CD_PHASE, 5u);
        failed = NATIVE1(ram, hooks, "Ovl181_CdCommand",
                         AE_OVL181_CD_COMMAND, 8u) < 0;
    }

    if (!failed && ae_rd8(ram, AE_OVL181_CD_STATUS_GATE) != 0u) {
        failed = NATIVE1(ram, hooks, "Ovl181_CdCommand",
                         AE_OVL181_CD_COMMAND, 0u) < 0;
        if (!failed && (ae_rd8(ram, AE_OVL181_CD_STATUS) & 0x10u) == 0u) {
            (void)ae_Ovl181_DrawIntegrityWarning(ram, hooks);
        }
    }

    int32_t final_status = NATIVE1(ram, hooks, "Ovl181_CdCommand",
                                   AE_OVL181_CD_COMMAND, 9u);
    return failed || final_status < 0 ? -1 : 0;
}

