#include "ae_ovl182.h"
#include "ae_overlay_layout.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"

enum {
    AE_OVL182_VSYNC = 0x8002E520u,
    AE_OVL182_DRAW_SYNC = 0x800286CCu,
    AE_OVL182_PAD_UPDATE = 0x8001325Cu,
    AE_OVL182_BEGIN_FRAME = 0x8005DF48u,
    AE_OVL182_PUT_DISP_ENV = 0x80028EFCu,
    AE_OVL182_PUT_DRAW_ENV = 0x80028D30u,
    AE_OVL182_DRAW_OTAG = 0x80028CC0u,
    AE_OVL182_CLEAR_IMAGE = 0x80028850u,
    AE_OVL182_CLEAR_IMAGE2 = 0x800288E0u,
    AE_OVL182_INIT_DISPLAY = 0x8005CFECu,
    AE_OVL182_GAME_RESET = 0x8005CE04u,
    AE_OVL182_SET_GEOM_SCREEN = 0x8002FAF0u,
    AE_OVL182_ARC_LOAD_ENTRY = 0x80011194u,
    AE_OVL182_LOAD_LIST_START = 0x80011104u,
    AE_OVL182_LOAD_LIST_STEP = 0x800116C0u,
    AE_OVL182_CD_UPDATE = 0x800125B8u,
    AE_OVL182_COLOR_FADE_RESET = 0x8008C390u,
    AE_OVL182_EFFECTS_RESET = 0x80069500u,
    AE_OVL182_FOG_APPLY = 0x800323F8u,
    AE_OVL182_HEAP_INIT = 0x8008C01Cu,
    AE_OVL182_ROT_MATRIX = 0x8002FB00u,
    AE_OVL182_TRANS_MATRIX = 0x8002FB30u,
    AE_OVL182_MESH_ROTATE = 0x80016294u,
    AE_OVL182_MESH_DRAW = 0x80017EFCu,
    AE_OVL182_MODEL_POSE = 0x80015780u,
    AE_OVL182_MODEL_TRANSFORM = 0x800161C0u,
    AE_OVL182_MODEL_DRAW = 0x80017CD4u,
    AE_OVL182_MODEL_DRAW_OT = 0x80017D94u,
    AE_OVL182_DRAW_SHADOW = 0x8001F604u,
    AE_OVL182_ALERT_DRAW = 0x8006ABF0u,
    AE_OVL182_LOOK_AT = 0x80046670u,
    AE_OVL182_MATRIX_FROM_DIR_XZ = 0x80014804u,
    AE_OVL182_MATRIX_RIGID = 0x800161C0u,
    AE_OVL182_MATRIX_SKINNED = 0x80016A60u,
    AE_OVL182_STATIC_MODEL_INIT = 0x80015D10u,
    AE_OVL182_MODEL_INIT_NORMALS = 0x80015B30u,
    AE_OVL182_MODEL_INIT = 0x80015AC0u,
    AE_OVL182_MESH_INIT = 0x80015C34u,
    AE_OVL182_RESET_SOUND = 0x80086FD0u,
    AE_OVL182_SOUND_FADE = 0x8008760Cu,
    AE_OVL182_TRANSITION_ENTRY = 0x80010B50u
};
enum {
    AE_OVL182_INPUT_CONFIRM = 0x0860u,
    AE_OVL182_INPUT_EXIT = 0x0900u,
    AE_OVL182_INPUT_LEFT = 0x1000u,
    AE_OVL182_INPUT_RIGHT = 0x4000u,
    AE_OVL182_INPUT_MENU_HORIZONTAL = 0xA000u,
    AE_OVL182_INPUT_OPTIONS = 0xA000u,
    AE_OVL182_MAX_IDLE_FRAMES = 1800,
    AE_OVL182_INTRO_FADE = 30,
    AE_OVL182_FADE_COMPLETE = 31
};

static int32_t ae_ovl182_call(AeRam *ram, const AeOverlayHooks *hooks,
                              const char *name, uint32_t address, uint32_t count,
                              uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3,
                              uint32_t a4, uint32_t a5, uint32_t a6)
{
    const uint32_t arguments[7] = {a0, a1, a2, a3, a4, a5, a6};
    return ae_overlay_call(ram, hooks, name, address, count, arguments);
}

static int32_t ae_ovl182_native_call(AeRam *ram, const AeOverlayHooks *hooks,
                                     const char *name, uint32_t address,
                                     uint32_t count, uint32_t a0,
                                     uint32_t a1, uint32_t a2)
{
    const uint32_t arguments[3] = {a0, a1, a2};
    return ae_overlay_native_call(ram, hooks, name, address, count, arguments);
}

static int32_t ae_ovl182_render(AeRam *ram, const AeOverlayHooks *hooks,
                                const char *name, uint32_t count,
                                uint32_t a0, uint32_t a1, uint32_t a2,
                                uint32_t a3, uint32_t a4)
{
    const uint32_t values[5] = {a0, a1, a2, a3, a4};
    return ae_overlay_render(ram, hooks, name, count, values);
}

#define OVL0(r,h,n,a) ae_ovl182_call((r),(h),(n),(a),0u,0u,0u,0u,0u,0u,0u,0u)
#define OVL1(r,h,n,a,x) ae_ovl182_call((r),(h),(n),(a),1u,(uint32_t)(x),0u,0u,0u,0u,0u,0u)
#define OVL2(r,h,n,a,x,y) ae_ovl182_call((r),(h),(n),(a),2u,(uint32_t)(x),(uint32_t)(y),0u,0u,0u,0u,0u)
#define OVL3(r,h,n,a,x,y,z) ae_ovl182_call((r),(h),(n),(a),3u,(uint32_t)(x),(uint32_t)(y),(uint32_t)(z),0u,0u,0u,0u)
#define OVL4(r,h,n,a,x,y,z,w) ae_ovl182_call((r),(h),(n),(a),4u,(uint32_t)(x),(uint32_t)(y),(uint32_t)(z),(uint32_t)(w),0u,0u,0u)
#define OVL5(r,h,n,a,x,y,z,w,q) ae_ovl182_call((r),(h),(n),(a),5u,(uint32_t)(x),(uint32_t)(y),(uint32_t)(z),(uint32_t)(w),(uint32_t)(q),0u,0u)

static uint32_t ae_ovl182_display_offset(uint32_t index)
{
    return index * AE_DISPLAY_BUFFER_STRIDE;
}

static void ae_ovl182_rect(AeRam *ram, uint32_t x, uint32_t y,
                           uint32_t width, uint32_t height)
{
    ae_wr16(ram, AE_OVL182_LOCAL_RECT + AE_RECT_X_OFFSET, x);
    ae_wr16(ram, AE_OVL182_LOCAL_RECT + AE_RECT_Y_OFFSET, y);
    ae_wr16(ram, AE_OVL182_LOCAL_RECT + AE_RECT_WIDTH_OFFSET, width);
    ae_wr16(ram, AE_OVL182_LOCAL_RECT + AE_RECT_HEIGHT_OFFSET, height);
}

int32_t ae_Ovl182_DrawFade(AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t timer = ae_rds16(ram, AE_OVL182_FADE_TIMER);
    int32_t alpha = (timer * 255) / AE_OVL182_INTRO_FADE;
    uint32_t packet_alpha = (uint8_t)alpha;
    uint32_t draw_environment = ae_rd32(ram, AE_DISPLAY_CURRENT_ENV);
    int32_t fade_state = ae_rds16(ram, AE_OVL182_FADE_STATE);
    uint32_t fade_mode = (fade_state == 0 || fade_state == 3) ?
                         0xE1000220u : 0xE1000240u;
    (void)ae_ovl182_render(ram, hooks, "OverlayFadeQuad", 5u,
                           draw_environment, (uint32_t)fade_state,
                           (uint32_t)timer, packet_alpha, fade_mode);
    ae_wr32(ram, AE_DISPLAY_CURRENT_ENV, draw_environment + AE_OVL182_FADE_PACKET_SIZE);
    return 0;
}

int32_t ae_Ovl182_FadeTick(AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t state = ae_rds16(ram, AE_OVL182_FADE_STATE);
    int32_t timer;
    if (state == 1) return 0;
    if (state < 2) {
        if (state != 0) return 0;
        ae_Ovl182_DrawFade(ram, hooks);
        timer = (int16_t)(ae_rds16(ram, AE_OVL182_FADE_TIMER) - 1);
        ae_wr16(ram, AE_OVL182_FADE_TIMER, (uint16_t)timer);
        if (timer < 0) {
            ae_wr16(ram, AE_OVL182_FADE_STATE, 1u);
            ae_wr16(ram, AE_OVL182_FADE_TIMER, 0u);
        }
        return 0;
    }
    if (state >= 4) return 0;
    ae_Ovl182_DrawFade(ram, hooks);
    timer = (int16_t)(ae_rds16(ram, AE_OVL182_FADE_TIMER) + 1);
    ae_wr16(ram, AE_OVL182_FADE_TIMER, (uint16_t)timer);
    if (timer >= AE_OVL182_FADE_COMPLETE) {
        ae_wr16(ram, AE_OVL182_FADE_STATE, 1u);
        ae_wr16(ram, AE_OVL182_FADE_TIMER, 0u);
        return -1;
    }
    return 0;
}

static void ae_ovl182_debug_set(AeRam *ram, const AeOverlayHooks *hooks,
                                uint32_t x, uint32_t y)
{
    (void)ae_ovl182_native_call(ram, hooks, "DebugTextSetColorWrap",
                                AE_OVL182_DEBUG_SET, 2u, x, y, 0u);
}

static void ae_ovl182_debug_text(AeRam *ram, const AeOverlayHooks *hooks,
                                 uint32_t format, uint32_t value)
{
    (void)ae_ovl182_native_call(ram, hooks, "DebugTextPrintfWhite",
                                AE_OVL182_DEBUG_TEXT, 2u, format, value, 0u);
}

static uint32_t ae_ovl182_input(AeRam *ram)
{
    return ae_rd32(ram, AE_OVERLAY_PAD_HELD) & ae_rd32(ram, AE_OVERLAY_PAD_CHANGED);
}

static void ae_ovl182_draw_menu(AeRam *ram, const AeOverlayHooks *hooks)
{
    uint32_t selection = ae_rd32(ram, AE_OVL182_SELECTION);
    ae_ovl182_debug_set(ram, hooks, 100u, 60u);
    ae_ovl182_debug_text(ram, hooks, AE_OVL182_MENU_LABEL_HEADER, 60u);

    ae_ovl182_debug_set(ram, hooks, 100u, 90u);
    ae_ovl182_debug_text(ram, hooks, AE_OVL182_MENU_LABEL_PAGE,
                         selection == 0u ? 62u : 32u);
    ae_ovl182_debug_set(ram, hooks, 250u, 90u);
    ae_ovl182_debug_text(ram, hooks, AE_OVL182_MENU_LABEL_MODE,
                         ae_rd32(ram, AE_OVL182_MENU_FLAG) != 0u ?
                         AE_OVL182_MENU_LABEL_MODE_STEREO : AE_OVL182_MENU_LABEL_MODE_MONO);

    ae_ovl182_debug_set(ram, hooks, 100u, 105u);
    ae_ovl182_debug_text(ram, hooks, AE_OVL182_MENU_LABEL_STEREO,
                         selection == 1u ? 62u : 32u);
    ae_ovl182_debug_set(ram, hooks, 250u, 105u);
    ae_ovl182_debug_text(ram, hooks,
                         ae_rd16(ram, AE_OVL182_STEREO_SETTING) != 0u ?
                         AE_OVL182_MENU_LABEL_GAUGE : AE_OVL182_MENU_LABEL_RUMBLE, 105u);

    ae_ovl182_debug_set(ram, hooks, 100u, 120u);
    ae_ovl182_debug_text(ram, hooks, AE_OVL182_MENU_LABEL_HISTORY,
                         selection == 2u ? 62u : 32u);
    ae_ovl182_debug_set(ram, hooks, 250u, 120u);
    ae_ovl182_debug_text(ram, hooks,
                         ae_rd8(ram, AE_OVL182_VIBRATION_SETTING) != 0u ?
                         AE_OVL182_MENU_LABEL_GAUGE : AE_OVL182_MENU_LABEL_RUMBLE, 120u);

    ae_ovl182_debug_set(ram, hooks, 100u, 135u);
    ae_ovl182_debug_text(ram, hooks, AE_OVL182_MENU_LABEL_OPTIONS,
                         selection == 3u ? 62u : 32u);
    ae_ovl182_debug_set(ram, hooks, 250u, 135u);
    ae_ovl182_debug_set(ram, hooks, 100u, 150u);
    ae_ovl182_debug_text(ram, hooks, AE_OVL182_MENU_LABEL_VIBRATION,
                         selection == 4u ? 62u : 32u);
    ae_ovl182_debug_set(ram, hooks, 250u, 150u);
    ae_ovl182_debug_text(ram, hooks,
                         ae_rd8(ram, AE_GAME_VIBRATION_STICK_MODE) != 0u ?
                         AE_OVL182_MENU_LABEL_GAUGE : AE_OVL182_MENU_LABEL_RUMBLE,
                         150u);
    (void)ae_ovl182_native_call(ram, hooks, "DebugDrawStickGauge",
                                AE_OVL182_DEBUG_GAUGE, 0u, 0u, 0u, 0u);
    (void)ae_ovl182_native_call(ram, hooks, "DebugDrawStickHistory",
                                AE_OVL182_DEBUG_HISTORY, 0u, 0u, 0u, 0u);
}

int32_t ae_Ovl182_MenuInput(AeRam *ram, const AeOverlayHooks *hooks)
{
    uint32_t input = ae_ovl182_input(ram);
    int32_t selection;
    if ((input & AE_OVL182_INPUT_EXIT) != 0u) {
        OVL1(ram, hooks, "PlaySfx", AE_OVL182_AUDIO_SFX, 2u);
        ae_wr32(ram, AE_OVL182_STATE, 0u);
        return 0;
    }
    if ((input & AE_OVL182_INPUT_LEFT) != 0u) {
        OVL1(ram, hooks, "PlaySfx", AE_OVL182_AUDIO_SFX, 0u);
        ae_wr32(ram, AE_OVL182_SELECTION, ae_rd32(ram, AE_OVL182_SELECTION) - 1u);
    }
    if ((input & AE_OVL182_INPUT_RIGHT) != 0u) {
        OVL1(ram, hooks, "PlaySfx", AE_OVL182_AUDIO_SFX, 0u);
        ae_wr32(ram, AE_OVL182_SELECTION, ae_rd32(ram, AE_OVL182_SELECTION) + 1u);
    }

    selection = (int32_t)ae_rd32(ram, AE_OVL182_SELECTION);
    int32_t wrapped = (selection + 5) % 5;
    if (wrapped >= 0 && wrapped < 5)
        ae_wr32(ram, AE_OVL182_SELECTION, (uint32_t)wrapped);
    selection = (int32_t)ae_rd32(ram, AE_OVL182_SELECTION);

    switch (selection) {
    case 0:
        if ((input & AE_OVL182_INPUT_OPTIONS) != 0u) {
            OVL1(ram, hooks, "PlaySfx", AE_OVL182_AUDIO_SFX, 1u);
            uint32_t value = ae_rd32(ram, AE_OVL182_MENU_FLAG);
            ae_wr32(ram, AE_OVL182_MENU_FLAG, value == 0u ? 1u : 0u);
        }
        break;
    case 1:
        if ((input & AE_OVL182_INPUT_OPTIONS) != 0u) {
            OVL1(ram, hooks, "PlaySfx", AE_OVL182_AUDIO_SFX, 1u);
            uint32_t value = ae_rd16(ram, AE_OVL182_STEREO_SETTING) ^ 1u;
            ae_wr16(ram, AE_OVL182_STEREO_SETTING, value);
            OVL1(ram, hooks, "SoundSetStereo", AE_OVL182_AUDIO_STEREO, value);
        }
        break;
    case 2:
        if ((input & AE_OVL182_INPUT_OPTIONS) != 0u) {
            OVL1(ram, hooks, "PlaySfx", AE_OVL182_AUDIO_SFX, 1u);
            uint32_t value = ae_rd8(ram, AE_OVL182_VIBRATION_SETTING) == 0u ? 1u : 0u;
            ae_wr8(ram, AE_OVL182_VIBRATION_SETTING, value);
        }
        break;
    case 3: {
        uint32_t stick_index = ae_rd32(ram, AE_OVL182_STICK_HISTORY_INDEX);
        uint32_t history_record = AE_OVL182_STICK_HISTORY + stick_index * 4u;
        if (ae_rds8(ram, history_record + AE_OVL182_STICK_RECORD_Y_OFFSET) >= 41)
            OVL3(ram, hooks, "PadRumble", AE_OVL182_RUMBLE, 0u, 1u, 2u);
        if (ae_rd8(ram, AE_OVL182_VIBRATION_SETTING) != 0u) {
            int32_t rumble_strength = ae_rds8(
                ram, history_record + AE_OVL182_STICK_RECORD_RUMBLE_OFFSET);
            if (rumble_strength >= 11) {
                uint32_t motor_strength = (uint32_t)rumble_strength << 1;
                OVL3(ram, hooks, "PadRumble", AE_OVL182_RUMBLE, 0u, motor_strength, 2u);
                ae_ovl182_debug_set(ram, hooks, 200u, 200u);
                ae_ovl182_debug_text(ram, hooks, AE_OVL182_MENU_LABEL_CONFIRM,
                                     motor_strength);
            }
        }
        break;
    }
    case 4:
        if ((input & AE_OVL182_INPUT_OPTIONS) != 0u) {
            OVL1(ram, hooks, "PlaySfx", AE_OVL182_AUDIO_SFX, 1u);
            uint32_t value = ae_rd8(ram, AE_GAME_VIBRATION_STICK_MODE) ^ 1u;
            ae_wr8(ram, AE_GAME_VIBRATION_STICK_MODE, value);
        }
        break;
    default:
        break;
    }
    ae_ovl182_draw_menu(ram, hooks);
    return 0;
}

int32_t ae_Ovl182_StateMachine(AeRam *ram, const AeOverlayHooks *hooks)
{
    switch (ae_rd32(ram, AE_OVL182_STATE)) {
    case 0: {
        int32_t result = ae_Ovl182_DrawIntroState(ram, hooks);
        return result < 0 ? -1 : 0;
    }
    case 1: return -3;
    case 2: return -6;
    case 3: return 0;
    case 4:
        (void)ae_Ovl182_MenuInput(ram, hooks);
        return 0;
    case 5: return -4;
    case 6: return -5;
    case 7: return 0;
    case 8: return -8;
    case 9: return -10;
    default: return 0;
    }
}

int32_t ae_Ovl182_DrawIntroState(AeRam *ram, const AeOverlayHooks *hooks)
{
    uint32_t draw_index = ae_rd32(ram, AE_OVL182_DRAW_INDEX);
    uint32_t packet_pointer = ae_rd32(ram, AE_OVL182_PACKET_CURSOR);
    uint32_t ordering_table = ae_rd32(ram, AE_OVL182_OT_HEAD);
    uint32_t frame_phase = ae_rd32(ram, AE_OVL182_PERSISTENT_FRAME);
    int32_t fade_state = ae_rds16(ram, AE_OVL182_FADE_STATE);
    (void)ae_ovl182_render(ram, hooks, "OverlayIntroTitle", 5u,
                           draw_index, packet_pointer, ordering_table,
                           frame_phase, (uint32_t)fade_state);
    ae_wr32(ram, AE_OVL182_PACKET_CURSOR,
            packet_pointer + AE_OVL182_INTRO_TITLE_PACKET_SIZE);
    if (fade_state != 1) return 0;

    uint32_t input = ae_ovl182_input(ram);
    if ((input & AE_OVL182_INPUT_CONFIRM) != 0u) {
        OVL1(ram, hooks, "PlaySfx", AE_OVL182_AUDIO_SFX, 2u);
        if (draw_index == 0u) ae_wr32(ram, AE_OVL182_STATE, 2u);
        else if (draw_index == 1u) ae_wr32(ram, AE_OVL182_STATE, 1u);
        return 0;
    }
    if ((input & AE_OVL182_INPUT_RIGHT) != 0u) {
        OVL1(ram, hooks, "PlaySfx", AE_OVL182_AUDIO_SFX, 0u);
        int32_t next = (int32_t)draw_index - 1;
        ae_wr32(ram, AE_OVL182_DRAW_INDEX, (uint32_t)(next < 0 ? fade_state : next));
        ae_wr16(ram, AE_OVL182_FRAME, 0u);
        return 0;
    }
    if ((input & AE_OVL182_INPUT_LEFT) != 0u) {
        OVL1(ram, hooks, "PlaySfx", AE_OVL182_AUDIO_SFX, 0u);
        uint32_t next = draw_index + 1u;
        ae_wr32(ram, AE_OVL182_DRAW_INDEX, next < 2u ? next : 0u);
        ae_wr16(ram, AE_OVL182_FRAME, 0u);
    }
    return 0;
}

int32_t ae_Ovl182_DrawScene3D(AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t angle = ae_rds16(ram, AE_OVL182_SCROLL_ANGLE);
    uint32_t old_scroll = ae_rd32(ram, AE_OVL182_SCROLL);
    uint32_t scroll = old_scroll + 64u;
    if (scroll >= 1025u) scroll = old_scroll - 960u;
    ae_wr32(ram, AE_OVL182_SCROLL, scroll);

    OVL1(ram, hooks, "SetRotMatrix", AE_OVL182_ROT_MATRIX, AE_OVL182_VIEW_MATRIX);
    OVL1(ram, hooks, "SetTransMatrix", AE_OVL182_TRANS_MATRIX, AE_OVL182_VIEW_MATRIX);
    uint32_t low_scroll = ae_rd16(ram, AE_OVL182_SCROLL);
    for (uint32_t column = 0; column < 16u; ++column) {
        int32_t tile_x = -8192;
        int32_t tile_scroll = (int32_t)(int16_t)(uint16_t)(low_scroll - 8192u + column * 0x400u);
        for (uint32_t row = 0; row < 16u; ++row) {
            uint32_t scratch_vertex = AE_SCRATCH_BASE + AE_OVL182_SCRATCH_VERTEX_OFFSET;
            ae_wr16(ram, scratch_vertex + AE_OVL182_SVECTOR_X_OFFSET,
                    (uint16_t)tile_x);
            ae_wr16(ram, scratch_vertex + AE_OVL182_SVECTOR_Y_OFFSET, 0u);
            ae_wr16(ram, scratch_vertex + AE_OVL182_SVECTOR_Z_OFFSET,
                    (uint16_t)tile_scroll);
            OVL2(ram, hooks, "MeshTransformRot90", AE_OVL182_MESH_ROTATE,
                 AE_OVL182_ACTOR_A, AE_OVL182_SCENE_VERTICES);
            OVL4(ram, hooks, "MeshDrawAnimColors", AE_OVL182_MESH_DRAW,
                 ae_rd32(ram, AE_OVL182_OT_HEAD) + AE_OVL182_SCENE_OT_SUBTABLE_OFFSET,
                 AE_OVL182_SCENE_VERTICES,
                 AE_OVL182_ACTOR_A, 0u);
            tile_x += 0x400;
        }
        (void)column;
    }

    int32_t scaled_angle = angle / 72;
    int32_t overlay_scroll = 8 - scaled_angle;
    if (overlay_scroll < -128) overlay_scroll = -128;
    if (overlay_scroll > 0) overlay_scroll = 0;
    uint32_t tint = 255u - ae_rd8(ram, AE_DISPLAY_GLOBAL_BASE);
    (void)ae_ovl182_render(ram, hooks, "OverlayWaterQuad", 5u,
                           ae_rd32(ram, AE_OVL182_PACKET_CURSOR), scroll,
                           (uint32_t)angle, (uint32_t)overlay_scroll, tint);
    ae_wr32(ram, AE_OVL182_PACKET_CURSOR,
            ae_rd32(ram, AE_OVL182_PACKET_CURSOR) +
                AE_OVL182_WATER_QUAD_PACKET_SIZE);
    return 0;
}

int32_t ae_Ovl182_Init(AeRam *ram, const AeOverlayHooks *hooks, uint32_t descriptor)
{
    OVL1(ram, hooks, "HeapInit", AE_OVL182_HEAP_INIT, descriptor);
    for (uint32_t actor = 0; actor < 4u; actor += 2u) {
        uint32_t output = AE_OVL182_CAMERA + (actor / 2u) * AE_OVL182_MODEL_SLOT_STRIDE;
        uint32_t model_index = ae_rd16(
            ram, descriptor + actor * AE_OVL182_DESCRIPTOR_ENTRY_STRIDE +
                     AE_OVL182_DESCRIPTOR_ACTOR_MODEL_OFFSET);
        uint32_t model_header = descriptor +
                                model_index * AE_OVL182_DESCRIPTOR_ENTRY_STRIDE;
        if (actor == 0u)
            OVL2(ram, hooks, "ModelInitNormals", AE_OVL182_MODEL_INIT_NORMALS,
                 output, model_header);
        else
            OVL2(ram, hooks, "ModelInit", AE_OVL182_MODEL_INIT, output, model_header);

        uint32_t animation_index = ae_rd16(
            ram, descriptor + actor * AE_OVL182_DESCRIPTOR_ENTRY_STRIDE +
                     AE_OVL182_DESCRIPTOR_ACTOR_ANIM_OFFSET);
        uint32_t animation_table = descriptor +
                                   animation_index * AE_OVL182_DESCRIPTOR_ENTRY_STRIDE;
        uint32_t animation_count = ae_rd32(ram, animation_table);
        ae_wr32(ram, output + AE_OVL182_MODEL_DATA_POINTER_OFFSET,
                animation_table +
                    animation_count * AE_OVL182_ANIMATION_RECORD_STRIDE +
                    AE_OVL182_ANIMATION_DATA_OFFSET);
        ae_wr16(ram, output + AE_OVL182_MODEL_ANIM_FRAME_OFFSET, 0u);
        ae_wr16(ram, output + AE_OVL182_MODEL_ANIM_INDEX_OFFSET, 0u);
        ae_wr16(ram, output + AE_OVL182_MODEL_ANIM_NEXT_INDEX_OFFSET, 0u);
        ae_wr16(ram, output + AE_OVL182_MODEL_ANIM_FLAGS_OFFSET, 0u);
        ae_wr16(ram, output + AE_OVL182_MODEL_INIT_AUX_OFFSET, 0u);
        ae_wr16(ram, output + AE_OVL182_MODEL_INIT_AUX_SECOND_OFFSET, 0u);
        ae_wr16(ram, output + AE_OVL182_MODEL_INIT_BOUNDS_OFFSET, 0xF000u);
        ae_wr32(ram, output + AE_OVL182_MODEL_ANIM_TABLE_OFFSET,
                animation_table + AE_OVL182_ANIMATION_DATA_OFFSET);
    }

    for (uint32_t mesh = 0; mesh < 2u; ++mesh) {
        uint32_t output = AE_OVL182_STATIC_MODEL_BASE + mesh * AE_OVL182_MESH_SLOT_STRIDE;
        uint32_t model_index = ae_rd16(
            ram, descriptor + AE_OVL182_DESCRIPTOR_MESH_ENTRY_OFFSET +
                     mesh * AE_OVL182_DESCRIPTOR_ENTRY_STRIDE);
        OVL2(ram, hooks, "StaticModelInit", AE_OVL182_STATIC_MODEL_INIT,
             output, descriptor + model_index * AE_OVL182_DESCRIPTOR_ENTRY_STRIDE);
    }

    uint32_t prop_descriptor = descriptor + AE_OVL182_DESCRIPTOR_PROP_OFFSET;
    uint32_t prop_output = AE_OVL182_CAMERA + AE_OVL182_PROP_MODEL_SLOT_OFFSET;
    uint32_t prop_index = ae_rd16(ram, prop_descriptor);
    OVL2(ram, hooks, "MeshInit", AE_OVL182_MESH_INIT,
         prop_output, descriptor + prop_index * AE_OVL182_DESCRIPTOR_ENTRY_STRIDE);
    ae_wr32(ram, prop_output + AE_OVL182_PROP_OUTPUT_FLAGS_OFFSET, 0u);
    return 0;
}

int32_t ae_Ovl182_UpdateCamera(AeRam *ram, const AeOverlayHooks *hooks)
{
    uint32_t camera = AE_OVL182_CAMERA;
    uint32_t phase = ae_rd32(ram, AE_OVL182_PERSISTENT_FRAME);
    int32_t first_sine = ae_sdk_rsin(ram, (int32_t)(phase << 3));
    int32_t second_sine = ae_sdk_rsin(ram, (int32_t)(phase << 2));
    int32_t third_cosine = ae_sdk_rcos(ram, (int32_t)(phase << 3));

    ae_wr16(ram, AE_OVL182_GAME_LOOK_AT_TARGET + AE_OVL182_SVECTOR_X_OFFSET, 0u);
    ae_wr16(ram, AE_OVL182_GAME_LOOK_AT_TARGET + AE_OVL182_SVECTOR_Y_OFFSET,
            (uint16_t)-250);
    ae_wr16(ram, AE_OVL182_GAME_LOOK_AT_TARGET + AE_OVL182_SVECTOR_Z_OFFSET, 0u);
    ae_wr16(ram, AE_OVL182_GAME_LOOK_AT_EYE + AE_OVL182_SVECTOR_X_OFFSET,
            (uint16_t)(first_sine / 5));
    ae_wr16(ram, AE_OVL182_GAME_LOOK_AT_EYE + AE_OVL182_SVECTOR_Y_OFFSET,
            (uint16_t)((second_sine >> 2) - 1280));
    ae_wr16(ram, AE_OVL182_GAME_LOOK_AT_EYE + AE_OVL182_SVECTOR_Z_OFFSET,
            (uint16_t)(third_cosine / 5));

    OVL5(ram, hooks, "CameraLookAt", AE_OVL182_LOOK_AT,
         AE_OVL182_GAME_LOOK_AT_EYE, AE_OVL182_GAME_LOOK_AT_TARGET,
         AE_OVL182_VIEW_MATRIX,
         AE_OVL182_CAMERA_ROT_MATRIX, 0u);
    ae_wr32(ram, camera + AE_OVL182_MODEL_POSITION_X_OFFSET, 0u);
    ae_wr32(ram, camera + AE_OVL182_MODEL_POSITION_Y_OFFSET, 0u);
    ae_wr32(ram, camera + AE_OVL182_MODEL_POSITION_Z_OFFSET, 512u);
    ae_sdk_MatrixFromDirXZ(ram,
                           camera + AE_OVL182_MODEL_DIRECTION_MATRIX_OFFSET,
                           ae_rd32(ram, camera + AE_OVL182_MODEL_MESH_POINTER_OFFSET));
    ae_wr16(ram, camera + AE_OVL182_MODEL_ANIM_FRAME_OFFSET,
            (uint16_t)(ae_rd16(ram, camera + AE_OVL182_MODEL_ANIM_FRAME_OFFSET) + 1u));
    uint32_t animation_table = ae_rd32(ram, camera + AE_OVL182_MODEL_ANIM_TABLE_OFFSET);
    uint32_t animation_index = ae_rd16(ram, camera + AE_OVL182_MODEL_ANIM_INDEX_OFFSET);
    uint32_t animation_end = (uint32_t)(uint16_t)ae_rds16(
        ram, camera + AE_OVL182_MODEL_ANIM_FRAME_OFFSET);
    uint32_t animation_record = animation_table +
                                animation_index * AE_OVL182_ANIMATION_RECORD_STRIDE;
    if ((int32_t)ae_rds16(ram, animation_record + AE_OVL182_ANIMATION_END_OFFSET) <
        (int32_t)animation_end) {
        uint32_t next = ae_rd16(ram, camera + AE_OVL182_MODEL_ANIM_NEXT_INDEX_OFFSET);
        ae_wr16(ram, camera + AE_OVL182_MODEL_ANIM_INDEX_OFFSET, (uint16_t)next);
        ae_wr16(ram, camera + AE_OVL182_MODEL_ANIM_FRAME_OFFSET,
                (uint16_t)ae_rd16(ram, animation_table +
                                  next * AE_OVL182_ANIMATION_RECORD_STRIDE +
                                  AE_OVL182_ANIMATION_FRAME_OFFSET));
    }
    OVL2(ram, hooks, "ModelPoseBones", AE_OVL182_MODEL_POSE, camera, 1u);
    OVL1(ram, hooks, "ModelTransformSkinned", AE_OVL182_MATRIX_SKINNED, camera);
    OVL3(ram, hooks, "ModelDraw", AE_OVL182_MODEL_DRAW,
         ae_rd32(ram, AE_OVL182_MODEL_DRAW_CONTEXT), AE_OVL182_SCENE_VERTICES, camera);
    OVL4(ram, hooks, "DrawShadow", AE_OVL182_DRAW_SHADOW,
         camera + AE_OVL182_MODEL_POSITION_X_OFFSET,
         ae_rd32(ram, camera + AE_OVL182_MODEL_POSITION_Y_OFFSET), 96u, 0u);
    ae_wr16(ram, AE_OVL182_LOCAL_ANGLES + AE_OVL182_SVECTOR_X_OFFSET, 1024u);
    ae_wr16(ram, AE_OVL182_LOCAL_ANGLES + AE_OVL182_SVECTOR_Y_OFFSET, 1024u);
    ae_wr16(ram, AE_OVL182_LOCAL_ANGLES + AE_OVL182_SVECTOR_Z_OFFSET, 0u);
    ae_sdk_RotMatrixZYX(ram, AE_OVL182_LOCAL_ANGLES, AE_OVL182_LOCAL_MATRIX);
    uint32_t first_model_header = ae_rd32(ram, AE_OVL182_STATIC_MODEL_BASE);
    uint32_t model_index = ae_rd16(
        ram, first_model_header + AE_OVL182_MODEL_HEADER_MATRIX_OFFSET);
    uint32_t actor_matrix = AE_OVL182_MODEL_INSTANCE_BASE +
                            model_index * AE_OVL182_MATRIX_STRIDE;
    ae_sdk_CompMatrix(ram, actor_matrix, AE_OVL182_LOCAL_MATRIX,
                      AE_OVL182_LOCAL_MATRIX);
    OVL3(ram, hooks, "ModelTransformRigid", AE_OVL182_MATRIX_RIGID,
         AE_OVL182_STATIC_MODEL_BASE, AE_OVL182_LOCAL_MATRIX, AE_OVL182_SCENE_VERTICES);
    OVL3(ram, hooks, "ModelDraw", AE_OVL182_MODEL_DRAW,
         ae_rd32(ram, AE_OVL182_OT_HEAD), AE_OVL182_SCENE_VERTICES,
         AE_OVL182_STATIC_MODEL_BASE);

    uint32_t actor_b = camera + AE_OVL182_MODEL_SLOT_STRIDE;
    OVL2(ram, hooks, "MatrixFromDirXZ", AE_OVL182_MATRIX_FROM_DIR_XZ,
         actor_b + AE_OVL182_MODEL_DIRECTION_MATRIX_OFFSET,
         ae_rd32(ram, actor_b + AE_OVL182_MODEL_MESH_POINTER_OFFSET));
    ae_wr32(ram, actor_b + AE_OVL182_MODEL_POSITION_X_OFFSET, 0u);
    ae_wr32(ram, actor_b + AE_OVL182_MODEL_POSITION_Y_OFFSET, 0u);
    ae_wr32(ram, actor_b + AE_OVL182_MODEL_POSITION_Z_OFFSET, (uint32_t)-512);
    animation_table = ae_rd32(ram, actor_b + AE_OVL182_MODEL_ANIM_TABLE_OFFSET);
    animation_index = ae_rd16(ram, actor_b + AE_OVL182_MODEL_ANIM_INDEX_OFFSET);
    animation_end = ae_rd16(ram, actor_b + AE_OVL182_MODEL_ANIM_FRAME_OFFSET);
    uint32_t actor_b_frame = animation_end + 1u;
    ae_wr16(ram, actor_b + AE_OVL182_MODEL_ANIM_FRAME_OFFSET,
            (uint16_t)actor_b_frame);
    animation_record = animation_table + animation_index *
                       AE_OVL182_ANIMATION_RECORD_STRIDE;
    if (ae_rds16(ram, animation_record + AE_OVL182_ANIMATION_END_OFFSET) <
        (int32_t)actor_b_frame) {
        uint32_t next = ae_rd16(ram, actor_b + AE_OVL182_MODEL_ANIM_NEXT_INDEX_OFFSET);
        ae_wr16(ram, actor_b + AE_OVL182_MODEL_ANIM_INDEX_OFFSET, (uint16_t)next);
        ae_wr16(ram, actor_b + AE_OVL182_MODEL_ANIM_FRAME_OFFSET,
                ae_rd16(ram, animation_table + next *
                        AE_OVL182_ANIMATION_RECORD_STRIDE +
                        AE_OVL182_ANIMATION_FRAME_OFFSET));
    }
    OVL2(ram, hooks, "ModelPoseBones", AE_OVL182_MODEL_POSE, actor_b, 3u);
    OVL1(ram, hooks, "ModelTransformSkinned", AE_OVL182_MATRIX_SKINNED, actor_b);
    OVL4(ram, hooks, "ModelDrawWithOt", AE_OVL182_MODEL_DRAW_OT,
         ae_rd32(ram, AE_OVL182_MODEL_DRAW_CONTEXT), AE_OVL182_SCENE_VERTICES,
         actor_b, 1024u);

    uint32_t static_model_b = AE_OVL182_STATIC_MODEL_BASE +
                              AE_OVL182_MESH_SLOT_STRIDE;
    uint32_t static_model_header = ae_rd32(ram, static_model_b);
    uint32_t static_matrix_index = ae_rd16(
        ram, static_model_header + AE_OVL182_MODEL_HEADER_MATRIX_OFFSET);
    uint32_t static_matrix = AE_OVL182_MODEL_INSTANCE_BASE +
                             static_matrix_index * AE_OVL182_MATRIX_STRIDE;
    OVL3(ram, hooks, "ModelTransformRigid", AE_OVL182_MATRIX_RIGID,
         static_model_b, static_matrix, AE_OVL182_SCENE_VERTICES);
    OVL3(ram, hooks, "ModelDraw", AE_OVL182_MODEL_DRAW,
         ae_rd32(ram, AE_OVL182_MODEL_DRAW_CONTEXT), AE_OVL182_SCENE_VERTICES,
         static_model_b);
    OVL4(ram, hooks, "DrawShadow", AE_OVL182_DRAW_SHADOW,
         actor_b + AE_OVL182_MODEL_POSITION_X_OFFSET,
         ae_rd32(ram, actor_b + AE_OVL182_MODEL_POSITION_Y_OFFSET), 64u, 0u);

    ae_wr16(ram, AE_OVL182_LOCAL_POSITION + AE_OVL182_SVECTOR_X_OFFSET, 0u);
    ae_wr16(ram, AE_OVL182_LOCAL_POSITION + AE_OVL182_SVECTOR_Y_OFFSET, 100u);
    ae_wr16(ram, AE_OVL182_LOCAL_POSITION + AE_OVL182_SVECTOR_Z_OFFSET, 0u);
    ae_sdk_ApplyMatrixSV(ram, AE_OVL182_WORLD_MATRIX,
                         AE_OVL182_LOCAL_POSITION, AE_OVL182_LOCAL_POSITION);
    uint32_t world_x = ae_rd16(ram, AE_OVL182_WORLD_X) +
                       ae_rd16(ram, AE_OVL182_LOCAL_POSITION +
                                   AE_OVL182_SVECTOR_X_OFFSET);
    uint32_t world_y = ae_rd16(ram, AE_OVL182_WORLD_Y) +
                       ae_rd16(ram, AE_OVL182_LOCAL_POSITION +
                                   AE_OVL182_SVECTOR_Y_OFFSET) + 220u;
    uint32_t world_z = ae_rd16(ram, AE_OVL182_WORLD_Z) +
                       ae_rd16(ram, AE_OVL182_LOCAL_POSITION +
                                   AE_OVL182_SVECTOR_Z_OFFSET);
    ae_wr16(ram, AE_OVL182_LOCAL_POSITION, (uint16_t)world_x);
    ae_wr16(ram, AE_OVL182_LOCAL_POSITION + AE_OVL182_SVECTOR_Y_OFFSET,
            (uint16_t)world_y);
    ae_wr16(ram, AE_OVL182_LOCAL_POSITION + AE_OVL182_SVECTOR_Z_OFFSET,
            (uint16_t)world_z);
    OVL4(ram, hooks, "AlertIconDraw", AE_OVL182_ALERT_DRAW,
         AE_OVL182_LOCAL_POSITION, 2u, 1u, world_y);
    (void)ae_Ovl182_DrawScene3D(ram, hooks);
    return 0;
}

static int32_t ae_ovl182_mode_frame_render(AeRam *ram, const AeOverlayHooks *hooks)
{
    uint32_t packet_cursor = ae_rd32(ram, AE_OVL182_PACKET_CURSOR);
    uint32_t display_index = ae_rd32(ram, AE_DISPLAY_INDEX);
    (void)ae_ovl182_render(ram, hooks, "OverlayModeFrame", 5u,
                           packet_cursor, display_index,
                           ae_rd32(ram, AE_OVL182_STATE),
                           (uint32_t)ae_rds16(ram, AE_OVL182_FADE_STATE),
                           ae_rd16(ram, AE_OVL182_FRAME));
    ae_wr32(ram, AE_OVL182_PACKET_CURSOR,
            packet_cursor + AE_OVL182_MODE_FRAME_PACKET_SIZE);
    return 0;
}

static int32_t ae_ovl182_finish_mode(AeRam *ram, const AeOverlayHooks *hooks,
                                    int32_t state_result)
{
    uint32_t game_mode = AE_GAME_MODE;
    switch (state_result) {
    case -10: ae_wr16(ram, AE_GAME_MODE, 34u); break;
    case -9: {
        uint32_t next_list = ((uint32_t)ae_rd16(ram, AE_GAME_BOOT_FIELD_15C) + 1u) % 5u;
        uint32_t list_entry = next_list + 99u;
        ae_wr16(ram, AE_GAME_BOOT_FIELD_15C, (uint16_t)next_list);
        ae_wr16(ram, AE_OVL182_NEXT_LIST, (uint16_t)list_entry);
        ae_wr16(ram, AE_GAME_MODE, 36u);
        OVL1(ram, hooks, "StageInitMemory", AE_OVL182_TRANSITION_ENTRY, list_entry);
        OVL2(ram, hooks, "LoadListStart", AE_OVL182_LOAD_LIST_START, 1u,
             ae_rd16(ram, AE_OVL182_NEXT_LIST));
        while (OVL0(ram, hooks, "LoadListStep", AE_OVL182_LOAD_LIST_STEP) == 0) {
            OVL0(ram, hooks, "CdMgrUpdate", AE_OVL182_CD_UPDATE);
            OVL1(ram, hooks, "VSync", AE_OVL182_VSYNC, 2u);
        }
        ae_wr16(ram, game_mode, 34u);
        break;
    }
    case -8: ae_wr16(ram, AE_GAME_MODE, 27u); break;
    case -7: break;
    case -6: ae_wr16(ram, AE_GAME_MODE, 5u); break;
    case -5: ae_wr16(ram, AE_GAME_MODE, 5u); break;
    case -4: ae_wr16(ram, AE_GAME_MODE, 6u); break;
    case -3:
        ae_wr16(ram, game_mode, 26u);
        ae_wr16(ram, AE_GAME_BOOT_FIELD_15A, 0u);
        break;
    case -2: ae_wr16(ram, AE_GAME_MODE, 11u); break;
    case -1: ae_wr16(ram, AE_GAME_MODE, 13u); break;
    default:
        break;
    }
    return 0;
}

int32_t ae_Mode_03(AeRam *ram, const AeOverlayHooks *hooks)
{
    int32_t step;
    ae_ovl182_rect(ram, 0u, 0u, 384u, 512u);
    OVL0(ram, hooks, "InitDisplay", AE_OVL182_INIT_DISPLAY);
    OVL0(ram, hooks, "GameStateReset", AE_OVL182_GAME_RESET);
    OVL1(ram, hooks, "SetGeomScreen", AE_OVL182_SET_GEOM_SCREEN, 328u);
    OVL4(ram, hooks, "ClearImage", AE_OVL182_CLEAR_IMAGE,
         AE_OVL182_LOCAL_RECT, 255u, 255u, 255u);
    OVL1(ram, hooks, "DrawSync", AE_OVL182_DRAW_SYNC, 0u);
    OVL1(ram, hooks, "ArcLoadEntry", AE_OVL182_ARC_LOAD_ENTRY, AE_OVL182_LOAD_ENTRY);
    ae_wr8(ram, AE_DISPLAY_RENDER_FLAGS, 1u);
    ae_wr8(ram, AE_DISPLAY_MODE_FLAGS, 1u);
    ae_wr32(ram, AE_DISPLAY_INDEX, 0u);
    do {
        step = OVL0(ram, hooks, "LoadListStep", AE_OVL182_LOAD_LIST_STEP);
        if (step >= 0) {
            OVL0(ram, hooks, "CdMgrUpdate", AE_OVL182_CD_UPDATE);
            OVL1(ram, hooks, "VSync", AE_OVL182_VSYNC, 2u);
        }
    } while (step >= 0);

    ae_Ovl182_Init(ram, hooks, AE_OVL182_LOAD_MEMBER);
    OVL0(ram, hooks, "EffectsReset", AE_OVL182_EFFECTS_RESET);
    ae_wr32(ram, AE_OVL182_MODE_GLOBALS + AE_OVL182_MODE_GLOBAL_FIELD_36_OFFSET,
            0xF000u);
    ae_wr32(ram, AE_OVL182_MODE_GLOBALS + AE_OVL182_MODE_GLOBAL_FIELD_40_OFFSET,
            1536u);
    ae_wr32(ram, AE_OVL182_MODE_GLOBALS + AE_OVL182_MODE_GLOBAL_FIELD_44_OFFSET,
            16384u);
    ae_wr32(ram, AE_OVL182_MODE_GLOBALS + AE_OVL182_MODE_GLOBAL_FIELD_28_OFFSET,
            0u);
    OVL1(ram, hooks, "ApplyLevelFog", AE_OVL182_FOG_APPLY, 0u);
    OVL4(ram, hooks, "SetColorMatrixMode", 0x80015D58u, 0u, 0u, 0u, 0u);

    ae_wr32(ram, AE_OVL182_STATE, 0u);
    ae_wr16(ram, AE_OVL182_FADE_TIMER, AE_OVL182_INTRO_FADE);
    ae_wr16(ram, AE_OVL182_FRAME, 0u);
    ae_wr16(ram, AE_OVL182_FADE_STATE, 0u);
    ae_wr32(ram, AE_OVL182_MODE_INPUT_SKIP, 0u);

    int32_t state_result = 0;
    for (;;) {
        OVL1(ram, hooks, "PadUpdate", AE_OVL182_PAD_UPDATE, 2u);
        OVL0(ram, hooks, "BeginFrame", AE_OVL182_BEGIN_FRAME);
        OVL0(ram, hooks, "ColorFadeReset", AE_OVL182_COLOR_FADE_RESET);
        int32_t fade_result = ae_Ovl182_FadeTick(ram, hooks);
        if (fade_result < 0) break;
        int32_t step_result = ae_Ovl182_StateMachine(ram, hooks);
        if (step_result < 0 && ae_rds16(ram, AE_OVL182_FADE_STATE) == 1) {
            state_result = step_result;
            ae_wr16(ram, AE_OVL182_FADE_STATE, step_result == -3 ? 3u : 2u);
        }
        int32_t frame = (int16_t)(ae_rd16(ram, AE_OVL182_FRAME) + 1u);
        ae_wr16(ram, AE_OVL182_FRAME, (uint16_t)frame);
        if (ae_rds16(ram, AE_OVL182_FADE_STATE) == 1 && frame > AE_OVL182_MAX_IDLE_FRAMES) {
            OVL1(ram, hooks, "SoundFade", AE_OVL182_SOUND_FADE,
                 0xE1000000u);
            ae_wr16(ram, AE_OVL182_FADE_STATE, 2u);
            state_result = -9;
        }

        ae_ovl182_mode_frame_render(ram, hooks);
        ae_Ovl182_UpdateCamera(ram, hooks);
        OVL1(ram, hooks, "DrawSync", AE_OVL182_DRAW_SYNC, 0u);
        OVL1(ram, hooks, "VSync", AE_OVL182_VSYNC, 2u);
        uint32_t display_index = ae_rd32(ram, AE_DISPLAY_INDEX);
        uint32_t persistent_frame = ae_rd32(ram, AE_OVL182_PERSISTENT_FRAME);
        ae_wr32(ram, AE_OVL182_PERSISTENT_FRAME, persistent_frame + 1u);
        OVL1(ram, hooks, "PutDispEnv", AE_OVL182_PUT_DISP_ENV,
             AE_DISPLAY_ENV_TABLE + ae_ovl182_display_offset(display_index));
        OVL1(ram, hooks, "PutDrawEnv", AE_OVL182_PUT_DRAW_ENV,
             AE_DRAW_ENV_TABLE + ae_ovl182_display_offset(display_index));
        OVL1(ram, hooks, "DrawOTag", AE_OVL182_DRAW_OTAG,
             AE_ORDERING_TABLE + ae_ovl182_display_offset(display_index) +
                 AE_DISPLAY_OT_END_OFFSET);
    }

    OVL1(ram, hooks, "DrawSync", AE_OVL182_DRAW_SYNC, 0u);
    OVL1(ram, hooks, "VSync", AE_OVL182_VSYNC, 0u);
    OVL1(ram, hooks, "SoundStop", AE_OVL182_RESET_SOUND, 1u);
    ae_ovl182_rect(ram, 0u, 0u, 384u, 512u);
    uint32_t clear_color = state_result == -3 ? 255u : 0u;
    OVL4(ram, hooks, "ClearImage", AE_OVL182_CLEAR_IMAGE,
         AE_OVL182_LOCAL_RECT, clear_color, clear_color, clear_color);
    OVL1(ram, hooks, "DrawSync", AE_OVL182_DRAW_SYNC, 0u);
    return ae_ovl182_finish_mode(ram, hooks, state_result);
}

