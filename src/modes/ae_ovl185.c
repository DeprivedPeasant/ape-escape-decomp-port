#include "ae_ovl185.h"
#include "../player/ae_player_spawn.h"
#include "../world/ae_misc_batch.h"

#define OVL185_API_INIT_DISPLAY 0x8005CFECu
#define OVL185_API_RESET_GRAPH 0x80028350u
#define OVL185_API_CLEAR_IMAGE 0x80028850u
#define OVL185_API_DRAW_SYNC 0x800286CCu
#define OVL185_API_BEGIN_FRAME 0x8005DF48u
#define OVL185_API_VSYNC 0x8002E520u
#define OVL185_API_PUT_DISP_ENV 0x80028EFCu
#define OVL185_API_PUT_DRAW_ENV 0x80028D30u
#define OVL185_API_DRAW_OTAG 0x80028CC0u
#define OVL185_API_ARC_LOAD_ENTRY 0x80011194u
#define OVL185_API_LOAD_LIST_STEP 0x800116C0u
#define OVL185_API_CD_UPDATE 0x800125B8u
#define OVL185_API_STAGE_RENDER_INIT 0x8005BEF0u
#define OVL185_API_QUEUE_FRAME_FLIP 0x8005DEE0u
#define OVL185_API_AREA_FEATURE_REQUEST 0x8009B7B0u

#define OVL185_RUN_FRAME_BYTES 48u
#define OVL185_PROMPT_PACKET_BYTES 36u
#define OVL185_FADE_COLOR 130
#define OVL185_RUN_RECT_OFFSET 0x10u
#define OVL185_RUN_RECT_FRAME_BYTES 48u
#define OVL185_TEXT_COLOR_TITLE 0
#define OVL185_TEXT_COLOR_ROW 1

static uint32_t ovl185_stack_base(const AeOvl185Hooks *h)
{
    return h != NULL && h->stack_pointer != 0u ? h->stack_pointer : 0x801FFF00u;
}

static int32_t ovl185_call(AeRam *r, const AeOvl185Hooks *h,
                           AeOvl185NamedHook callback, const char *name,
                           uint32_t address, uint32_t argc,
                           const uint32_t *argv)
{
    if (callback == NULL) {
        ae_ram_fail(r, "AeOvl185 %s hook not provided", name);
        return 0;
    }
    return callback(h != NULL ? h->user : NULL, r, name, address, argc, argv);
}

static int32_t ovl185_native(AeRam *r, const AeOvl185Hooks *h,
                             const char *name, uint32_t address,
                             uint32_t argc, const uint32_t *argv)
{
    return ovl185_call(r, h, h != NULL ? h->native_call : NULL,
                       name, address, argc, argv);
}

static int32_t ovl185_renderer(AeRam *r, const AeOvl185Hooks *h,
                               const char *name, uint32_t address,
                               uint32_t argc, const uint32_t *argv)
{
    return ovl185_call(r, h, h != NULL ? h->renderer : NULL,
                       name, address, argc, argv);
}

#define OVL185_NATIVE0(r, h, name, address) \
    ovl185_native((r), (h), (name), (address), 0u, NULL)
#define OVL185_NATIVE1(r, h, name, address, a0) \
    ovl185_native((r), (h), (name), (address), 1u, \
                  (uint32_t[]){(uint32_t)(a0)})
#define OVL185_NATIVE2(r, h, name, address, a0, a1) \
    ovl185_native((r), (h), (name), (address), 2u, \
                  (uint32_t[]){(uint32_t)(a0), (uint32_t)(a1)})
#define OVL185_NATIVE4(r, h, name, address, a0, a1, a2, a3) \
    ovl185_native((r), (h), (name), (address), 4u, \
                  (uint32_t[]){(uint32_t)(a0), (uint32_t)(a1), \
                               (uint32_t)(a2), (uint32_t)(a3)})
#define OVL185_RENDER4(r, h, name, address, a0, a1, a2, a3) \
    ovl185_renderer((r), (h), (name), (address), 4u, \
                    (uint32_t[]){(uint32_t)(a0), (uint32_t)(a1), \
                                 (uint32_t)(a2), (uint32_t)(a3)})
#define OVL185_RENDER5(r, h, name, address, a0, a1, a2, a3, a4) \
    ovl185_renderer((r), (h), (name), (address), 5u, \
                    (uint32_t[]){(uint32_t)(a0), (uint32_t)(a1), \
                                 (uint32_t)(a2), (uint32_t)(a3), \
                                 (uint32_t)(a4)})

static uint32_t ovl185_frame_offset(uint32_t index)
{
    return index * AE_OVL185_FRAME_STRIDE;
}

static void ovl185_draw_label(AeRam *r, const AeOvl185Hooks *h,
                              uint32_t text, int32_t x, int32_t y,
                              int32_t color)
{
    (void)OVL185_RENDER4(r, h, "Ovl185_DrawText", AE_OVL185_DRAW_TEXT,
                         text, (uint32_t)x, (uint32_t)y, (uint32_t)color);
}

/* Entry @80146000 selects between the soundless scene viewer and its scene-load phase. */
int32_t ae_ovl185_entry_80146000(AeRam *r, const AeOvl185Hooks *h)
{
    ae_wr32(r, AE_OVL185_G_MODE, 0u);
    for (;;) {
        uint32_t mode = ae_rd32(r, AE_OVL185_G_MODE);
        if (mode == 0u) {
            if (ae_Ovl185_Run(r, h) < 0) return 0;
        } else if (mode == 1u) {
            (void)ae_Ovl185_LoadScene(r, h);
        }
        if (ae_ram_failed(r)) return 0;
    }
}

/* Display and frame loop @8014607C. Scene and input transitions remain in Ovl185_Update. */
int32_t ae_Ovl185_Run(AeRam *r, const AeOvl185Hooks *h)
{
    uint32_t stack_pointer = ovl185_stack_base(h) - OVL185_RUN_RECT_FRAME_BYTES;
    uint32_t rect = stack_pointer + OVL185_RUN_RECT_OFFSET;
    uint32_t frame = AE_OVL185_G_FRAME_COUNTER;

    ae_wr32(r, rect, ae_rd32(r, AE_OVL185_G_CLEAR_RECT));
    ae_wr32(r, rect + 4u, ae_rd32(r, AE_OVL185_G_CLEAR_RECT + 4u));
    (void)OVL185_NATIVE0(r, h, "InitDisplay", OVL185_API_INIT_DISPLAY);
    (void)OVL185_NATIVE1(r, h, "ResetGraph", OVL185_API_RESET_GRAPH, 1u);
    (void)OVL185_NATIVE4(r, h, "ClearImage", OVL185_API_CLEAR_IMAGE,
                         rect, 0u, 0u, 0x7Fu);
    (void)OVL185_NATIVE1(r, h, "DrawSync", OVL185_API_DRAW_SYNC, 0u);

    ae_wr8(r, AE_OVL185_PACKET_FLAGS, 1u);
    ae_wr8(r, AE_OVL185_STATE_FLAGS, 1u);
    ae_wr8(r, AE_OVL185_G_DRAW_ENV + AE_OVL185_ENV_RED_OFFSET, 0u);
    ae_wr8(r, AE_OVL185_G_DRAW_ENV + AE_OVL185_ENV_GREEN_OFFSET, 0u);
    ae_wr8(r, AE_OVL185_G_DRAW_ENV + AE_OVL185_ENV_BLUE_OFFSET,
           AE_OVL185_ENV_BLUE);
    ae_wr8(r, AE_OVL185_G_DRAW_ENV_SECOND + AE_OVL185_ENV_RED_OFFSET, 0u);
    ae_wr8(r, AE_OVL185_G_DRAW_ENV_SECOND + AE_OVL185_ENV_GREEN_OFFSET, 0u);
    ae_wr8(r, AE_OVL185_G_DRAW_ENV_SECOND + AE_OVL185_ENV_BLUE_OFFSET,
           AE_OVL185_ENV_BLUE);
    ae_wr32(r, AE_OVL185_G_OT_INDEX, 0u);
    ae_wr32(r, AE_OVL185_G_SCENE, 0u);

    for (;;) {
        uint32_t index;
        uint32_t frame_offset;
        int32_t update_result;

        ae_pad_PadUpdate(r, h != NULL ? h->pad : NULL, 0u);
        (void)OVL185_NATIVE0(r, h, "BeginFrame", OVL185_API_BEGIN_FRAME);
        update_result = ae_Ovl185_Update(r, h);
        if (ae_ram_failed(r)) return 0;
        if (update_result != 0) return update_result < 0 ? -1 : 0;

        (void)OVL185_NATIVE1(r, h, "DrawSync", OVL185_API_DRAW_SYNC, 0u);
        (void)OVL185_NATIVE1(r, h, "VSync", OVL185_API_VSYNC, 0u);
        index = ae_rd32(r, AE_OVL185_G_OT_INDEX);
        frame_offset = ovl185_frame_offset(index);
        ae_wr32(r, frame, ae_rd32(r, frame) + 1u);
        (void)OVL185_NATIVE1(r, h, "PutDispEnv", OVL185_API_PUT_DISP_ENV,
                             AE_OVL185_G_DISP_ENV + frame_offset);
        (void)OVL185_NATIVE1(r, h, "PutDrawEnv", OVL185_API_PUT_DRAW_ENV,
                             AE_OVL185_G_DRAW_ENV + frame_offset);
        (void)OVL185_NATIVE1(r, h, "DrawOTag", OVL185_API_DRAW_OTAG,
                             AE_OVL185_G_ORDER_TABLES + frame_offset +
                                 AE_OVL185_OT_END_OFFSET);
        (void)OVL185_NATIVE0(r, h, "CdMgrUpdate", OVL185_API_CD_UPDATE);
        if (ae_ram_failed(r)) return 0;
    }
}

/* Input, scene selection and native text/selection drawing @80146230. */
int32_t ae_Ovl185_Update(AeRam *r, const AeOvl185Hooks *h)
{
    uint32_t input = ae_rd32(r, AE_OVL185_PAD_STATE + AE_OVL185_PAD_HELD) &
                     ae_rd32(r, AE_OVL185_PAD_STATE + AE_OVL185_PAD_CHANGED);
    uint32_t scene = ae_rd32(r, AE_OVL185_G_SCENE);
    uint32_t cursor_address = AE_OVL185_G_FRAME_CURSOR;

    if ((input & AE_OVL185_START_BITS) != 0u) {
        if (h == NULL || h->sound_hw == NULL) {
            ae_ram_fail(r, "AeOvl185 PlaySfx sound hook table not provided");
            return 0;
        }
        (void)ae_PlaySfx(r, h->sound_hw, 2);
        ae_wr32(r, AE_OVL185_G_MODE, 1u);
        return 1;
    }
    if ((input & AE_OVL185_BUTTON_CANCEL) != 0u) return -1;

    if ((input & AE_OVL185_BUTTON_LEFT) != 0u) {
        if (h == NULL || h->sound_hw == NULL) {
            ae_ram_fail(r, "AeOvl185 PlaySfx sound hook table not provided");
            return 0;
        }
        (void)ae_PlaySfx(r, h->sound_hw, 0);
        scene--;
        if ((int32_t)scene < 0) scene = AE_OVL185_SCENE_COUNT - 1u;
        ae_wr32(r, AE_OVL185_G_SCENE, scene);
    }
    if ((input & AE_OVL185_BUTTON_RIGHT) != 0u) {
        if (h == NULL || h->sound_hw == NULL) {
            ae_ram_fail(r, "AeOvl185 PlaySfx sound hook table not provided");
            return 0;
        }
        (void)ae_PlaySfx(r, h->sound_hw, 0);
        scene = ae_rd32(r, AE_OVL185_G_SCENE) + 1u;
        if (scene >= AE_OVL185_SCENE_COUNT) scene = 0u;
        ae_wr32(r, AE_OVL185_G_SCENE, scene);
    }

    ovl185_draw_label(r, h, AE_OVL185_G_DRAW_STRINGS,
                      160, 40, OVL185_TEXT_COLOR_TITLE);
    for (uint32_t row = 0; row < AE_OVL185_SCENE_COUNT; ++row) {
        ovl185_draw_label(r, h, AE_OVL185_G_DRAW_ROWS + row * 0x10u,
                          110, 70 + (int32_t)row * 20,
                          OVL185_TEXT_COLOR_ROW);
    }

    /* The native renderer owns the inline SP2 packet and ordering-table link. */
    uint32_t packet_cursor = ae_rd32(r, cursor_address);
    (void)OVL185_RENDER5(r, h, "Ovl185_DrawSelection",
                         AE_OVL185_DRAW_SELECTION,
                         packet_cursor,
                         ae_rd32(r, AE_OVL185_G_OT_INDEX),
                         ae_rd32(r, AE_OVL185_G_SCENE),
                         0x800F03CCu,
                         AE_OVL185_FRAME_STRIDE);
    if (!ae_ram_failed(r))
        ae_wr32(r, cursor_address, packet_cursor + AE_OVL185_SELECTION_PACKET_BYTES);
    return 0;
}

/* Archive load and area-feature loop @80146500. */
int32_t ae_Ovl185_LoadScene(AeRam *r, const AeOvl185Hooks *h)
{
    uint32_t scene = ae_rd32(r, AE_OVL185_G_SCENE);
    uint32_t record = AE_OVL185_G_SCENE_LIST +
                      scene * AE_OVL185_SCENE_RECORD_STRIDE;
    uint32_t frame = AE_OVL185_G_FRAME_COUNTER;
    int32_t load_result;

    (void)OVL185_NATIVE1(r, h, "ArcLoadEntry", OVL185_API_ARC_LOAD_ENTRY,
                         ae_rd32(r, record + AE_OVL185_SCENE_RECORD_ARCHIVE));
    do {
        load_result = OVL185_NATIVE0(r, h, "LoadListStep",
                                     OVL185_API_LOAD_LIST_STEP);
        if (load_result >= 0) {
            (void)OVL185_NATIVE0(r, h, "CdMgrUpdate", OVL185_API_CD_UPDATE);
        }
    } while (load_result >= 0 && !ae_ram_failed(r));
    if (ae_ram_failed(r)) return 0;

    (void)OVL185_NATIVE0(r, h, "StageRenderInit", OVL185_API_STAGE_RENDER_INIT);
    (void)ae_AreaLoad(r, h != NULL ? h->stage : NULL,
                      (int32_t)ae_rd32(r, record + AE_OVL185_SCENE_RECORD_AREA),
                      0);
    if (ae_ram_failed(r)) return 0;

    (void)OVL185_NATIVE1(r, h, "AreaFeature30Request",
                         OVL185_API_AREA_FEATURE_REQUEST,
                         ae_rd32(r, record + AE_OVL185_SCENE_RECORD_ENTRY));

    for (;;) {
        uint32_t index;
        uint32_t frame_offset;
        ae_pad_PadUpdate(r, h != NULL ? h->pad : NULL, 1u);
        ae_AreaFeature30Update(r, h != NULL ? h->misc : NULL);
        if (ae_ram_failed(r)) return 0;
        ae_wr32(r, frame, ae_rd32(r, frame) + 1u);
        (void)OVL185_NATIVE0(r, h, "BeginFrame", OVL185_API_BEGIN_FRAME);
        if (ae_ram_failed(r)) return 0;
        if (ae_AreaFeature30GetState(r) == 0) break;
        index = ae_rd32(r, AE_OVL185_G_OT_INDEX);
        frame_offset = ovl185_frame_offset(index);
        (void)OVL185_NATIVE0(r, h, "CdMgrUpdate", OVL185_API_CD_UPDATE);
        (void)OVL185_NATIVE1(r, h, "DrawSync", OVL185_API_DRAW_SYNC, 0u);
        (void)OVL185_NATIVE4(r, h, "QueueFrameFlip",
                             OVL185_API_QUEUE_FRAME_FLIP,
                             AE_OVL185_G_DRAW_ENV + frame_offset,
                             AE_OVL185_G_DISP_ENV + frame_offset,
                             ae_rd32(r, AE_OVL185_G_FRAME_COUNTER + 8u) +
                                 AE_OVL185_OT_END_OFFSET,
                             AE_OVL185_G_OT_BUFFER + frame_offset);
        if (ae_ram_failed(r)) return 0;
    }
    ae_wr32(r, AE_OVL185_G_MODE, 0u);
    return 0;
}
