#include "ae_ovl184.h"
#include "../player/ae_player_spawn.h"
#include "../world/ae_misc_batch.h"
#include "../world/ae_zako2.h"

#define OVL184_API_INIT_DISPLAY 0x8005CFECu
#define OVL184_API_RESET_GRAPH 0x80028350u
#define OVL184_API_CLEAR_IMAGE 0x80028850u
#define OVL184_API_DRAW_SYNC 0x800286CCu
#define OVL184_API_BEGIN_FRAME 0x8005DF48u
#define OVL184_API_VSYNC 0x8002E520u
#define OVL184_API_PUT_DISP_ENV 0x80028EFCu
#define OVL184_API_PUT_DRAW_ENV 0x80028D30u
#define OVL184_API_DRAW_OTAG 0x80028CC0u
#define OVL184_API_LOAD_LIST_START 0x80011104u
#define OVL184_API_LOAD_LIST_STEP 0x800116C0u

#define OVL184_ENTRY_FRAME_BYTES 24u
#define OVL184_LOOP_FRAME_BYTES 56u
#define OVL184_LOAD_LOOP_FRAME_BYTES 48u
#define OVL184_PROMPT_FRAME_BYTES 24u
#define OVL184_TEXT_DEMO 0x801363E8u
#define OVL184_TEXT_PUSH_START 0x801363FCu
#define OVL184_FADE_COLOR 130

static uint32_t ovl184_stack_base(const AeOvl184Hooks *h)
{
    return h != NULL && h->stack_pointer != 0u ? h->stack_pointer : 0x801FFF00u;
}

static int32_t ovl184_native(AeRam *r, const AeOvl184Hooks *h,
                             const char *name, uint32_t address,
                             uint32_t argc, const uint32_t *argv)
{
    if (h == NULL || h->native_call == NULL) {
        ae_ram_fail(r, "AeOvl184 %s hook not provided", name);
        return 0;
    }
    return h->native_call(h->user, r, name, address, argc, argv);
}

#define OVL184_API0(r, h, name, address) \
    ovl184_native((r), (h), (name), (address), 0u, NULL)
#define OVL184_API1(r, h, name, address, a0) \
    ovl184_native((r), (h), (name), (address), 1u, \
                  (uint32_t[]){(uint32_t)(a0)})
#define OVL184_API2(r, h, name, address, a0, a1) \
    ovl184_native((r), (h), (name), (address), 2u, \
                  (uint32_t[]){(uint32_t)(a0), (uint32_t)(a1)})
#define OVL184_API4(r, h, name, address, a0, a1, a2, a3) \
    ovl184_native((r), (h), (name), (address), 4u, \
                  (uint32_t[]){(uint32_t)(a0), (uint32_t)(a1), \
                               (uint32_t)(a2), (uint32_t)(a3)})

static void ovl184_debug_text(AeRam *r, const AeOvl184Hooks *h,
                              uint32_t call_stack, uint32_t format,
                              int32_t prompt_index)
{
    /* DebugTextPrintf reads its first vararg from the caller's argument home area. */
    ae_wr32(r, call_stack + 4u, (uint32_t)prompt_index);
    if (h == NULL || h->text == NULL) {
        ae_ram_fail(r, "AeOvl184 DebugTextPrintf hook table not provided");
        return;
    }
    ae_DebugTextPrintf(r, h->text, format, call_stack);
}

/* Entry @80136000 initializes the mode's stage-state discriminator after its loop. */
int32_t ae_ovl184_entry_80136000(AeRam *r, const AeOvl184Hooks *h)
{
    uint32_t state = AE_OVL184_G_STAGE_STATE + AE_OVL184_STAGE_STATE_MODE;
    AeOvl184Hooks nested_hooks = h != NULL ? *h : (AeOvl184Hooks){0};

    nested_hooks.stack_pointer = ovl184_stack_base(h) - OVL184_ENTRY_FRAME_BYTES;
    (void)ae_Ovl184_DemoLoop(r, &nested_hooks, 0);
    ae_wr16(r, state, 9u);
    return 0;
}

/* One-time TIM setup followed by the resident-list load and frame loop @80136038. */
int32_t ae_Ovl184_LoadListLoop(AeRam *r, const AeOvl184Hooks *h)
{
    uint32_t stack_pointer = ovl184_stack_base(h) - OVL184_LOAD_LOOP_FRAME_BYTES;
    uint32_t rect = stack_pointer + AE_OVL184_LOOP_RECT_OFFSET;
    uint32_t frame = AE_OVL184_G_FRAME_COUNTER;
    int32_t load_result;

    ae_wr32(r, rect, ae_rd32(r, AE_OVL184_G_PAGE_STATE));
    ae_wr32(r, rect + AE_OVL184_CLEAR_RECT_SECOND,
            ae_rd32(r, AE_OVL184_G_PAGE_STATE + AE_OVL184_CLEAR_RECT_SECOND));
    (void)OVL184_API0(r, h, "InitDisplay", OVL184_API_INIT_DISPLAY);
    (void)OVL184_API1(r, h, "ResetGraph", OVL184_API_RESET_GRAPH, 1u);
    (void)OVL184_API4(r, h, "ClearImage", OVL184_API_CLEAR_IMAGE,
                      rect, 0u, 0u, 0u);
    (void)OVL184_API1(r, h, "DrawSync", OVL184_API_DRAW_SYNC, 0u);
    ae_wr8(r, AE_OVL184_PACKET_FLAGS, 1u);
    ae_wr8(r, AE_OVL184_STATE_FLAGS, 1u);
    (void)OVL184_API2(r, h, "LoadListStart", OVL184_API_LOAD_LIST_START,
                      1u, 0u);
    ae_wr32(r, AE_OVL184_G_OT_INDEX, 0u);
    (void)ae_TimListResidentUpdate(r, h != NULL ? h->stage_fade : NULL,
                                   0, -1);
    if (ae_ram_failed(r)) return 0;

    for (;;) {
        uint32_t index;
        uint32_t frame_offset;

        ae_pad_PadUpdate(r, h != NULL ? h->pad : NULL, 0u);
        (void)OVL184_API0(r, h, "BeginFrame", OVL184_API_BEGIN_FRAME);
        ae_ColorFadeReset(r);
        load_result = OVL184_API1(r, h, "LoadListStep",
                                  OVL184_API_LOAD_LIST_STEP, 0u);
        if (ae_ram_failed(r)) return 0;
        if (load_result < 0) break;

        (void)OVL184_API1(r, h, "DrawSync", OVL184_API_DRAW_SYNC, 0u);
        (void)OVL184_API1(r, h, "VSync", OVL184_API_VSYNC, 0u);
        index = ae_rd32(r, AE_OVL184_G_OT_INDEX);
        frame_offset = index * AE_OVL184_FRAME_STRIDE;
        ae_wr32(r, frame, ae_rd32(r, frame) + 1u);
        (void)OVL184_API1(r, h, "PutDispEnv", OVL184_API_PUT_DISP_ENV,
                          AE_OVL184_G_DISP_ENV + frame_offset);
        (void)OVL184_API1(r, h, "PutDrawEnv", OVL184_API_PUT_DRAW_ENV,
                          AE_OVL184_G_DRAW_ENV + frame_offset);
        (void)OVL184_API1(r, h, "DrawOTag", OVL184_API_DRAW_OTAG,
                          AE_OVL184_G_ORDER_TABLES + frame_offset +
                              AE_OVL184_OT_END_OFFSET);
        if (ae_ram_failed(r)) return 0;
    }
    ae_wr16(r, AE_OVL184_G_STAGE_STATE + AE_OVL184_STAGE_STATE_ACTIVE_AREA, 0u);
    return 0;
}

/* The loop owns display setup and frame presentation; controller processing remains PadUpdate. */
int32_t ae_Ovl184_DemoLoop(AeRam *r, const AeOvl184Hooks *h,
                           int32_t prompt_index)
{
    uint32_t stack_pointer = ovl184_stack_base(h) - OVL184_LOOP_FRAME_BYTES;
    uint32_t rect = stack_pointer + AE_OVL184_LOOP_RECT_OFFSET;
    uint32_t frame = AE_OVL184_G_FRAME_COUNTER;

    ae_wr32(r, rect, ae_rd32(r, AE_OVL184_G_PAGE_STATE));
    ae_wr32(r, rect + AE_OVL184_CLEAR_RECT_SECOND,
            ae_rd32(r, AE_OVL184_G_PAGE_STATE + AE_OVL184_CLEAR_RECT_SECOND));
    (void)OVL184_API0(r, h, "InitDisplay", OVL184_API_INIT_DISPLAY);
    (void)OVL184_API1(r, h, "ResetGraph", OVL184_API_RESET_GRAPH, 1u);
    (void)OVL184_API4(r, h, "ClearImage", OVL184_API_CLEAR_IMAGE,
                      rect, 0u, 0u, 0u);
    (void)OVL184_API1(r, h, "DrawSync", OVL184_API_DRAW_SYNC, 0u);

    ae_wr8(r, AE_OVL184_PACKET_FLAGS, 1u);
    ae_wr8(r, AE_OVL184_STATE_FLAGS, 1u);
    ae_wr32(r, AE_OVL184_G_OT_INDEX, 0u);

    for (;;) {
        uint32_t index;
        uint32_t frame_offset;
        int32_t result;
        AeOvl184Hooks prompt_hooks;

        ae_pad_PadUpdate(r, h != NULL ? h->pad : NULL, 0u);
        (void)OVL184_API0(r, h, "BeginFrame", OVL184_API_BEGIN_FRAME);
        ae_ColorFadeReset(r);
        if (ae_ram_failed(r)) return 0;

        /* The prompt routine's stack frame is below this loop's frame. */
        prompt_hooks = h != NULL ? *h : (AeOvl184Hooks){0};
        prompt_hooks.stack_pointer = stack_pointer;
        result = ae_Ovl184_DrawDemoPrompt(r, &prompt_hooks, prompt_index);
        if (ae_ram_failed(r)) return 0;
        if (result < 0) {
            break;
        }

        (void)OVL184_API1(r, h, "DrawSync", OVL184_API_DRAW_SYNC, 0u);
        (void)OVL184_API1(r, h, "VSync", OVL184_API_VSYNC, 0u);
        index = ae_rd32(r, AE_OVL184_G_OT_INDEX);
        frame_offset = index * AE_OVL184_FRAME_STRIDE;
        ae_wr32(r, frame, ae_rd32(r, frame) + 1u);
        (void)OVL184_API1(r, h, "PutDispEnv", OVL184_API_PUT_DISP_ENV,
                          AE_OVL184_G_DISP_ENV + frame_offset);
        (void)OVL184_API1(r, h, "PutDrawEnv", OVL184_API_PUT_DRAW_ENV,
                          AE_OVL184_G_DRAW_ENV + frame_offset);
        (void)OVL184_API1(r, h, "DrawOTag", OVL184_API_DRAW_OTAG,
                          AE_OVL184_G_ORDER_TABLES + frame_offset +
                              AE_OVL184_OT_END_OFFSET);
        if (ae_ram_failed(r)) return 0;
    }
    return 0;
}

/* Native text layout stays in the validated debug-text port; this routine owns its page and input state. */
int32_t ae_Ovl184_DrawDemoPrompt(AeRam *r, const AeOvl184Hooks *h,
                                 int32_t prompt_index)
{
    uint32_t call_stack = ovl184_stack_base(h) - OVL184_PROMPT_FRAME_BYTES;
    uint32_t held = ae_rd32(r, AE_OVL184_PAD_STATE + AE_OVL184_PAD_HELD);
    uint32_t changed = ae_rd32(r, AE_OVL184_PAD_STATE + AE_OVL184_PAD_CHANGED);

    ae_ColorFadeSet(r, OVL184_FADE_COLOR, 70);
    ovl184_debug_text(r, h, call_stack, OVL184_TEXT_DEMO, prompt_index);
    ae_ColorFadeSet(r, OVL184_FADE_COLOR, 210);
    ovl184_debug_text(r, h, call_stack, OVL184_TEXT_PUSH_START, prompt_index);
    if (ae_ram_failed(r)) return 0;

    if ((held & changed & AE_OVL184_BUTTON_START) == 0u) return 0;
    if (h == NULL || h->sound_hw == NULL) {
        ae_ram_fail(r, "AeOvl184 PlaySfx sound hook table not provided");
        return 0;
    }
    (void)ae_PlaySfx(r, h->sound_hw, 2);
    return -1;
}
