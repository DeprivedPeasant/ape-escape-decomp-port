#include "ae_ovl183.h"

#include "../player/ae_player_spawn.h"
#include "../world/ae_misc_batch.h"

#define OVL183_API_INIT_DISPLAY 0x8005CFECu
#define OVL183_API_RESET_GRAPH 0x80028350u
#define OVL183_API_CLEAR_IMAGE 0x80028850u
#define OVL183_API_DRAW_SYNC 0x800286CCu
#define OVL183_API_BEGIN_FRAME 0x8005DF48u
#define OVL183_API_VSYNC 0x8002E520u
#define OVL183_API_PUT_DISPLAY_ENV 0x80028EFCu
#define OVL183_API_PUT_DRAW_ENV 0x80028D30u
#define OVL183_API_DRAW_OTAG 0x80028CC0u
#define OVL183_API_DEBUG_TEXT_SET_CURSOR 0x8008CC60u
#define OVL183_API_ARC_LOAD_ENTRY 0x80011194u
#define OVL183_API_LOAD_LIST_STEP 0x800116C0u
#define OVL183_API_CD_MANAGER_UPDATE 0x800125B8u
#define OVL183_API_SOUND_STOP_AND_RESTORE 0x80086170u
#define OVL183_API_SOUND_STOP_ALL_VOICES 0x800A342Cu
#define OVL183_API_SOUND_LOOP_TABLE_GET_PARAMS 0x80088548u

#define OVL183_ENTRY_FRAME_BYTES 48u
#define OVL183_UPDATE_FRAME_BYTES 24u
#define OVL183_MAIN_FRAME_BYTES 40u
#define OVL183_COMMON_FRAME_BYTES 40u
#define OVL183_SIGHT_FRAME_BYTES 40u
#define OVL183_CLEAR_RECT 0x80136A80u
#define OVL183_MENU_TITLE 0x80136AA0u
#define OVL183_MENU_FORMAT 0x80136AB8u
#define OVL183_COMMON_LABEL_A 0x80136ABC
#define OVL183_COMMON_LABEL_B 0x80136ACC
#define OVL183_COMMON_LABEL_C 0x80136AE4u
#define OVL183_COMMON_FORMAT 0x80136AF8u
#define OVL183_SIGHT_LABEL_A 0x80136B18u
#define OVL183_SIGHT_LABEL_B 0x80136B28u
#define OVL183_SIGHT_LABEL_C 0x80136ACC
#define OVL183_SIGHT_FORMAT 0x80136AF8u
#define OVL183_FADE_COLOR 130
#define OVL183_BUTTON_START 0x0800u
#define OVL183_BUTTON_MENU_UP 0x1000u
#define OVL183_BUTTON_MENU_DOWN 0x4000u
#define OVL183_BUTTON_SOUND_PREVIOUS 0x8000u
#define OVL183_BUTTON_SOUND_NEXT 0x2000u
#define OVL183_BUTTON_PREVIOUS 0x1000u
#define OVL183_BUTTON_NEXT 0x4000u
#define OVL183_BUTTON_PLAY_A 0x0080u
#define OVL183_BUTTON_PLAY_B 0x0020u
#define OVL183_BUTTON_STOP 0x0100u
#define OVL183_BUTTON_MUTE 0x0008u

static uint32_t ovl183_stack_base(const AeOvl183Hooks *h)
{
    return h != NULL && h->stack_pointer != 0u ? h->stack_pointer : 0x801FFF00u;
}

static const AeOvl183Hooks *ovl183_nested_hooks(AeOvl183Hooks *nested,
                                                const AeOvl183Hooks *h,
                                                uint32_t frame_bytes)
{
    if (h == NULL) return NULL;
    *nested = *h;
    nested->stack_pointer = ovl183_stack_base(h) - frame_bytes;
    return nested;
}

static int32_t ovl183_named_call(AeRam *r, const AeOvl183Hooks *h,
                                 AeOvl183NamedHook callback,
                                 const char *name, uint32_t address,
                                 uint32_t argc, const uint32_t *argv)
{
    if (callback == NULL) {
        ae_ram_fail(r, "AeOvl183 %s hook not provided", name);
        return 0;
    }
    return callback(h != NULL ? h->user : NULL, r, name, address, argc, argv);
}

static int32_t ovl183_native(AeRam *r, const AeOvl183Hooks *h,
                             const char *name, uint32_t address,
                             uint32_t argc, const uint32_t *argv)
{
    return ovl183_named_call(r, h, h != NULL ? h->native_call : NULL,
                             name, address, argc, argv);
}

static int32_t ovl183_renderer(AeRam *r, const AeOvl183Hooks *h,
                               const char *name, uint32_t address,
                               uint32_t argc, const uint32_t *argv)
{
    return ovl183_named_call(r, h, h != NULL ? h->renderer : NULL,
                             name, address, argc, argv);
}

#define OVL183_NATIVE0(r,h,n,a) ovl183_native((r),(h),(n),(a),0u,NULL)
#define OVL183_NATIVE1(r,h,n,a,x) \
    ovl183_native((r),(h),(n),(a),1u,(uint32_t[]){(uint32_t)(x)})
#define OVL183_NATIVE2(r,h,n,a,x,y) \
    ovl183_native((r),(h),(n),(a),2u,(uint32_t[]){(uint32_t)(x),(uint32_t)(y)})
#define OVL183_NATIVE4(r,h,n,a,x,y,z,w) \
    ovl183_native((r),(h),(n),(a),4u,(uint32_t[]){(uint32_t)(x),(uint32_t)(y),(uint32_t)(z),(uint32_t)(w)})

static uint32_t ovl183_frame_offset(uint32_t index)
{
    return index * AE_OVL183_FRAME_STRIDE;
}

static void ovl183_debug_set_cursor(AeRam *r, const AeOvl183Hooks *h,
                                    int32_t x, int32_t y)
{
    (void)OVL183_NATIVE2(r, h, "DebugTextSetCursor",
                         OVL183_API_DEBUG_TEXT_SET_CURSOR,
                         (uint32_t)x, (uint32_t)y);
}

static void ovl183_debug_printf(AeRam *r, const AeOvl183Hooks *h,
                                uint32_t frame_bytes, uint32_t format,
                                const uint32_t *arguments,
                                uint32_t argument_count, int white)
{
    uint32_t caller_sp = ovl183_stack_base(h) - frame_bytes;
    for (uint32_t i = 0; i < argument_count; ++i)
        ae_wr32(r, caller_sp + 4u + i * 4u, arguments[i]);
    if (h == NULL || h->text == NULL) {
        ae_ram_fail(r, "AeOvl183 debug text hook table not provided");
        return;
    }
    if (white)
        ae_DebugTextPrintfWhite(r, h->text, format, caller_sp);
    else
        ae_DebugTextPrintf(r, h->text, format, caller_sp);
}

static int32_t ovl183_play_sfx(AeRam *r, const AeOvl183Hooks *h, int32_t id)
{
    if (h == NULL || h->sound_hw == NULL) {
        ae_ram_fail(r, "AeOvl183 PlaySfx sound hook table not provided");
        return 0;
    }
    return ae_PlaySfx(r, h->sound_hw, id);
}

static uint32_t ovl183_record(uint32_t selection)
{
    return AE_OVL183_SOUND_RECORDS +
           selection * AE_OVL183_SOUND_RECORD_STRIDE;
}

static int32_t ovl183_load_archive(AeRam *r, const AeOvl183Hooks *h)
{
    int32_t result;
    (void)OVL183_NATIVE1(r, h, "ArcLoadEntry", OVL183_API_ARC_LOAD_ENTRY,
                         AE_OVL183_G_ARCHIVE_ENTRY);
    do {
        result = OVL183_NATIVE0(r, h, "LoadListStep",
                                OVL183_API_LOAD_LIST_STEP);
        if (result >= 0)
            (void)OVL183_NATIVE0(r, h, "CdMgrUpdate",
                                 OVL183_API_CD_MANAGER_UPDATE);
    } while (result >= 0 && !ae_ram_failed(r));
    return result;
}

/* Overlay entry @80136000. */
int32_t ae_ovl183_entry_80136000(AeRam *r, const AeOvl183Hooks *h)
{
    uint32_t stack_pointer = ovl183_stack_base(h) - OVL183_ENTRY_FRAME_BYTES;
    uint32_t rectangle = stack_pointer + 0x10u;
    uint32_t render_state = AE_OVL183_G_FRAME;
    uint32_t stage_mode = AE_OVL183_G_STAGE_STATE + AE_OVL183_STAGE_MODE;
    uint32_t debug_enabled = 0x800F460Au;

    ae_wr32(r, rectangle, ae_rd32(r, OVL183_CLEAR_RECT));
    ae_wr32(r, rectangle + 4u, ae_rd32(r, OVL183_CLEAR_RECT + 4u));
    (void)OVL183_NATIVE0(r, h, "InitDisplay", OVL183_API_INIT_DISPLAY);
    (void)OVL183_NATIVE1(r, h, "ResetGraph", OVL183_API_RESET_GRAPH, 1u);
    (void)OVL183_NATIVE4(r, h, "ClearImage", OVL183_API_CLEAR_IMAGE,
                         rectangle, 0u, 0u, 0u);
    (void)OVL183_NATIVE1(r, h, "DrawSync", OVL183_API_DRAW_SYNC, 0u);

    ae_wr8(r, AE_OVL183_G_PACKET_FLAGS, 1u);
    ae_wr8(r, AE_OVL183_G_RENDER_FLAGS, 1u);
    ae_wr32(r, AE_OVL183_G_OT_INDEX, 0u);
    ae_wr32(r, AE_OVL183_G_SOUND_SELECTION, 0u);
    ae_wr32(r, AE_OVL183_G_PAGE, 0u);
    ae_wr32(r, AE_OVL183_G_MENU_SELECTION, 0u);
    ae_wr16(r, debug_enabled, ae_rd16(r, debug_enabled) | 2u);
    for (;;) {
        int32_t result;
        uint32_t index;
        uint32_t frame_offset;
        AeOvl183Hooks update_hooks;

        ae_pad_PadUpdate(r, h != NULL ? h->pad : NULL, 2u);
        if (ae_ram_failed(r)) return 0;
        (void)OVL183_NATIVE0(r, h, "BeginFrame", OVL183_API_BEGIN_FRAME);
        ae_ColorFadeReset(r);
        result = ae_SoundTest_Update(
            r, ovl183_nested_hooks(&update_hooks, h, OVL183_ENTRY_FRAME_BYTES));
        if (ae_ram_failed(r)) return 0;
        if (result < 0) break;

        (void)OVL183_NATIVE1(r, h, "DrawSync", OVL183_API_DRAW_SYNC, 0u);
        (void)OVL183_NATIVE1(r, h, "VSync", OVL183_API_VSYNC, 0u);
        index = ae_rd32(r, AE_OVL183_G_OT_INDEX);
        frame_offset = ovl183_frame_offset(index);
        ae_wr32(r, render_state, ae_rd32(r, render_state) + 1u);
        (void)OVL183_NATIVE1(r, h, "PutDispEnv",
                             OVL183_API_PUT_DISPLAY_ENV,
                             AE_OVL183_G_DISPLAY_ENV + frame_offset);
        (void)OVL183_NATIVE1(r, h, "PutDrawEnv",
                             OVL183_API_PUT_DRAW_ENV,
                             AE_OVL183_G_DRAW_ENV + frame_offset);
        (void)OVL183_NATIVE1(r, h, "DrawOTag", OVL183_API_DRAW_OTAG,
                             AE_OVL183_G_ORDER_TABLES + frame_offset +
                                 AE_OVL183_OT_END);
        if (ae_ram_failed(r)) return 0;
    }

    ae_wr16(r, stage_mode, 3u);
    return 0;
}

/* Page dispatch @801361BC. */
int32_t ae_SoundTest_Update(AeRam *r, const AeOvl183Hooks *h)
{
    uint32_t page = ae_rd32(r, AE_OVL183_G_PAGE);
    AeOvl183Hooks nested;
    const AeOvl183Hooks *child =
        ovl183_nested_hooks(&nested, h, OVL183_UPDATE_FRAME_BYTES);
    if (page == 1u) return ae_SoundTest_DrawCommonTest(r, child);
    if (page == 2u) return ae_SoundTest_DrawSightTest(r, child);
    if (page == 0u) return ae_SoundTest_DrawMainMenu(r, child);
    return 0;
}

/* Main menu, selection controls and archive transition @80136244. */
int32_t ae_SoundTest_DrawMainMenu(AeRam *r, const AeOvl183Hooks *h)
{
    uint32_t current_sound = ae_rd32(r, AE_OVL183_G_SOUND_SELECTION);
    uint32_t selected_row = ae_rd32(r, AE_OVL183_G_MENU_SELECTION);
    uint32_t held = ae_rd32(r, AE_OVL183_PAD_STATE + AE_OVL183_PAD_HELD);
    uint32_t changed = ae_rd32(r, AE_OVL183_PAD_STATE + AE_OVL183_PAD_CHANGED);
    uint32_t repeated = ae_rd32(r, AE_OVL183_PAD_STATE + AE_OVL183_PAD_REPEAT);

    ovl183_debug_set_cursor(r, h, 100, 40);
    ovl183_debug_printf(r, h, OVL183_MAIN_FRAME_BYTES,
                        OVL183_MENU_TITLE, NULL, 0u, 1);
    for (uint32_t row = 0; row < 3u; ++row) {
        uint32_t label = ae_rd32(r, AE_OVL183_MENU_LABELS + row * 4u);
        uint32_t argument = label;
        ovl183_debug_set_cursor(r, h, 160, 100 + (int32_t)row * 20);
        ovl183_debug_printf(r, h, OVL183_MAIN_FRAME_BYTES,
                            OVL183_MENU_FORMAT, &argument, 1u, 1);
    }
    ovl183_debug_set_cursor(r, h, 190, 150);
    {
        uint32_t argument = ovl183_record(current_sound);
        ovl183_debug_printf(r, h, OVL183_MAIN_FRAME_BYTES,
                            OVL183_MENU_FORMAT, &argument, 1u, 1);
    }
    ovl183_debug_set_cursor(r, h, 260, 150);
    {
        uint32_t argument = ovl183_record(current_sound) + 0x10u;
        ovl183_debug_printf(r, h, OVL183_MAIN_FRAME_BYTES,
                            OVL183_MENU_FORMAT, &argument, 1u, 1);
    }

    {
        uint32_t cursor = ae_rd32(r, AE_OVL183_G_FRAME + 4u);
        uint32_t args[4] = {cursor, ae_rd32(r, AE_OVL183_G_OT_INDEX),
                            selected_row, current_sound};
        (void)ovl183_renderer(r, h, "SoundTest_DrawMainMenuSelection",
                              AE_OVL183_BASE + 0x0330u, 4u, args);
        ae_wr32(r, AE_OVL183_G_FRAME + 4u, cursor + 36u);
    }
    if (ae_ram_failed(r)) return 0;

    if ((held & changed & OVL183_BUTTON_START) != 0u) {
        ovl183_play_sfx(r, h, 2);
        if (ae_ram_failed(r)) return 0;
        if (selected_row == 0u) return -1;
        if (selected_row == 1u) {
            ae_wr32(r, AE_OVL183_G_PAGE, 1u);
            ae_wr32(r, AE_OVL183_G_COMMON_SELECTION, 0u);
            return 0;
        }
        ae_wr32(r, AE_OVL183_G_PAGE, 2u);
        ae_wr32(r, AE_OVL183_G_COMMON_SELECTION, 172u);
        ae_wr32(r, AE_OVL183_G_SIGHT_MUTE, 0u);
        ae_wr16(r, AE_OVL183_G_ARCHIVE_ENTRY,
                ae_rd16(r, ovl183_record(current_sound) + 24u));
        (void)ovl183_load_archive(r, h);
        return 0;
    }

    if ((repeated & OVL183_BUTTON_MENU_UP) != 0u) {
        ovl183_play_sfx(r, h, 0);
        selected_row = selected_row == 0u ? 2u : selected_row - 1u;
        ae_wr32(r, AE_OVL183_G_MENU_SELECTION, selected_row);
    } else if ((repeated & OVL183_BUTTON_MENU_DOWN) != 0u) {
        ovl183_play_sfx(r, h, 0);
        selected_row += 1u;
        if ((int32_t)selected_row >= 3) selected_row = 0u;
        ae_wr32(r, AE_OVL183_G_MENU_SELECTION, selected_row);
    } else if ((repeated & OVL183_BUTTON_SOUND_PREVIOUS) != 0u) {
        ovl183_play_sfx(r, h, 0);
        current_sound -= 1u;
        if ((int32_t)current_sound < 0) current_sound = 42u;
        ae_wr32(r, AE_OVL183_G_SOUND_SELECTION, current_sound);
    } else if ((repeated & OVL183_BUTTON_SOUND_NEXT) != 0u) {
        ovl183_play_sfx(r, h, 0);
        current_sound += 1u;
        if ((int32_t)current_sound >= 43) current_sound = 0u;
        ae_wr32(r, AE_OVL183_G_SOUND_SELECTION, current_sound);
    }
    return 0;
}

/* Common sound-effect page @80136698. */
int32_t ae_SoundTest_DrawCommonTest(AeRam *r, const AeOvl183Hooks *h)
{
    uint32_t selection = ae_rd32(r, AE_OVL183_G_COMMON_SELECTION);
    uint32_t repeated = ae_rd32(r, AE_OVL183_PAD_STATE + AE_OVL183_PAD_REPEAT);
    uint32_t held = ae_rd32(r, AE_OVL183_PAD_STATE + AE_OVL183_PAD_HELD);
    uint32_t changed = ae_rd32(r, AE_OVL183_PAD_STATE + AE_OVL183_PAD_CHANGED);
    uint32_t frame_bytes = OVL183_COMMON_FRAME_BYTES;

    ae_ColorFadeSet(r, OVL183_FADE_COLOR, 70);
    ovl183_debug_printf(r, h, frame_bytes, OVL183_COMMON_LABEL_A, NULL, 0u, 0);
    ae_ColorFadeSet(r, OVL183_FADE_COLOR, 196);
    ovl183_debug_printf(r, h, frame_bytes, OVL183_COMMON_LABEL_B, NULL, 0u, 0);
    ae_ColorFadeSet(r, OVL183_FADE_COLOR, 210);
    ovl183_debug_printf(r, h, frame_bytes, OVL183_COMMON_LABEL_C, NULL, 0u, 0);

    if ((held & changed & OVL183_BUTTON_START) != 0u) {
        ovl183_play_sfx(r, h, 2);
        ae_wr32(r, AE_OVL183_G_PAGE, 0u);
        return 0;
    }
    if ((repeated & OVL183_BUTTON_PREVIOUS) != 0u) {
        ovl183_play_sfx(r, h, 0);
        selection -= 1u;
        if ((int32_t)selection < 0) selection = 171u;
        ae_wr32(r, AE_OVL183_G_COMMON_SELECTION, selection);
    } else if ((repeated & OVL183_BUTTON_NEXT) != 0u) {
        ovl183_play_sfx(r, h, 0);
        selection += 1u;
        if ((int32_t)selection >= 172) selection = 0u;
        ae_wr32(r, AE_OVL183_G_COMMON_SELECTION, selection);
    } else if ((repeated & (OVL183_BUTTON_PLAY_A | OVL183_BUTTON_PLAY_B)) != 0u) {
        ae_wr32(r, AE_OVL183_G_COMMON_VOICE,
                (uint32_t)ovl183_play_sfx(r, h, (int32_t)selection));
    } else if ((repeated & OVL183_BUTTON_STOP) != 0u) {
        (void)OVL183_NATIVE1(r, h, "SoundStopAllVoices",
                             OVL183_API_SOUND_STOP_ALL_VOICES, 0u);
    }

    {
        uint32_t args[3] = {selection, AE_OVL183_G_COMMON_LABEL,
                            AE_OVL183_G_COMMON_LABEL + 4u};
        (void)ovl183_native(r, h, "SoundLoopTableGetParams",
                            OVL183_API_SOUND_LOOP_TABLE_GET_PARAMS, 3u, args);
    }
    ae_ColorFadeSet(r, OVL183_FADE_COLOR, OVL183_FADE_COLOR);
    {
        uint32_t args[4] = {selection,
                            ae_rd32(r, AE_OVL183_G_COMMON_LABEL),
                            ae_rd32(r, AE_OVL183_G_COMMON_LABEL + 4u),
                            ae_rd32(r, AE_OVL183_G_COMMON_VOICE)};
        ovl183_debug_printf(r, h, frame_bytes, OVL183_COMMON_FORMAT,
                            args, 4u, 0);
    }
    return 0;
}

/* XA/BGM audition page @8013684C. */
int32_t ae_SoundTest_DrawSightTest(AeRam *r, const AeOvl183Hooks *h)
{
    uint32_t selection = ae_rd32(r, AE_OVL183_G_COMMON_SELECTION);
    uint32_t repeated = ae_rd32(r, AE_OVL183_PAD_STATE + AE_OVL183_PAD_REPEAT);
    uint32_t held = ae_rd32(r, AE_OVL183_PAD_STATE + AE_OVL183_PAD_HELD);
    uint32_t changed = ae_rd32(r, AE_OVL183_PAD_STATE + AE_OVL183_PAD_CHANGED);
    uint32_t frame_bytes = OVL183_SIGHT_FRAME_BYTES;

    ae_ColorFadeSet(r, OVL183_FADE_COLOR, 70);
    ovl183_debug_printf(r, h, frame_bytes, OVL183_SIGHT_LABEL_A, NULL, 0u, 0);
    ae_ColorFadeSet(r, OVL183_FADE_COLOR, 180);
    ovl183_debug_printf(r, h, frame_bytes, OVL183_SIGHT_LABEL_B, NULL, 0u, 0);
    ae_ColorFadeSet(r, OVL183_FADE_COLOR, 196);
    ovl183_debug_printf(r, h, frame_bytes, OVL183_SIGHT_LABEL_C, NULL, 0u, 0);
    ae_ColorFadeSet(r, OVL183_FADE_COLOR, 210);
    ovl183_debug_printf(r, h, frame_bytes, OVL183_COMMON_LABEL_C, NULL, 0u, 0);

    if ((held & changed & OVL183_BUTTON_START) != 0u) {
        ovl183_play_sfx(r, h, 2);
        ae_wr32(r, AE_OVL183_G_PAGE, 0u);
        (void)ae_SoundStop(r, h != NULL ? h->stage_fade : NULL, 1);
        return 0;
    }
    if ((repeated & OVL183_BUTTON_PREVIOUS) != 0u) {
        ovl183_play_sfx(r, h, 0);
        selection -= 1u;
        if ((int32_t)selection < 172) selection = 196u;
        ae_wr32(r, AE_OVL183_G_COMMON_SELECTION, selection);
    } else if ((repeated & OVL183_BUTTON_NEXT) != 0u) {
        ovl183_play_sfx(r, h, 0);
        selection += 1u;
        if ((int32_t)selection >= 197) selection = 172u;
        ae_wr32(r, AE_OVL183_G_COMMON_SELECTION, selection);
    } else if ((repeated & OVL183_BUTTON_MUTE) != 0u) {
        int32_t mute = (int32_t)ae_rd32(r, AE_OVL183_G_SIGHT_MUTE);
        (void)ae_SoundSetVolumes(r, h != NULL ? h->progress : NULL,
                                 127, mute != 0 ? 48 : 0);
        ae_wr32(r, AE_OVL183_G_SIGHT_MUTE, mute == 0 ? 1u : 0u);
    } else if ((repeated & (OVL183_BUTTON_PLAY_A | OVL183_BUTTON_PLAY_B)) != 0u) {
        ae_wr32(r, AE_OVL183_G_SIGHT_VOICE,
                (uint32_t)ovl183_play_sfx(r, h, (int32_t)selection));
    } else if ((repeated & OVL183_BUTTON_STOP) != 0u) {
        (void)OVL183_NATIVE1(r, h, "SoundStopAllVoices",
                             OVL183_API_SOUND_STOP_ALL_VOICES, 0u);
    }

    {
        uint32_t args[3] = {selection, AE_OVL183_G_SIGHT_LABEL,
                            AE_OVL183_G_SIGHT_LABEL + 4u};
        (void)ovl183_native(r, h, "SoundLoopTableGetParams",
                            OVL183_API_SOUND_LOOP_TABLE_GET_PARAMS, 3u, args);
    }
    ae_ColorFadeSet(r, OVL183_FADE_COLOR, OVL183_FADE_COLOR);
    {
        uint32_t args[4] = {selection,
                            ae_rd32(r, AE_OVL183_G_SIGHT_LABEL),
                            ae_rd32(r, AE_OVL183_G_SIGHT_LABEL + 4u),
                            ae_rd32(r, AE_OVL183_G_SIGHT_VOICE)};
        ovl183_debug_printf(r, h, frame_bytes, OVL183_SIGHT_FORMAT,
                            args, 4u, 0);
    }
    return 0;
}
