#include "../../src/game/ae_calls.h"

static int32_t items_get_level_first_area(void *user, AeRam *ram,
                                          uint32_t a0, uint32_t a1,
                                          uint32_t a2, uint32_t a3) {
    AeGame *game = user;
    (void)ram; (void)a1; (void)a2; (void)a3;
    return ae_game_call(game, 0x8009764Cu, a0, 0u, 0u, 0u);
}

static int32_t items_printf(void *user, AeRam *ram, uint32_t a0,
                            uint32_t a1, uint32_t a2, uint32_t a3) {
    AeGame *game = user;
    return ae_game_missing_call(game, "printf", 0x800A3720u,
                                a0, a1, a2, a3);
}

static void items_set_back_color(void *user, AeRam *ram, uint32_t a0,
                                 uint32_t a1, uint32_t a2) {
    AeGame *game = user;
    if (game->host.platform.SetBackColor != NULL)
        (void)game->host.platform.SetBackColor(game->host.user, ram,
                                               a0, a1, a2);
}

static void items_set_far_color(void *user, AeRam *ram, uint32_t a0,
                                uint32_t a1, uint32_t a2) {
    AeGame *game = user;
    if (game->host.platform.SetFarColor != NULL)
        (void)game->host.platform.SetFarColor(game->host.user, ram,
                                              a0, a1, a2);
}

static void items_set_color_matrix(void *user, AeRam *ram, uint32_t matrix) {
    AeGame *game = user;
    if (game->host.platform.SetColorMatrix != NULL)
        (void)game->host.platform.SetColorMatrix(game->host.user, ram,
                                                 matrix);
}

static int32_t items_rsin(void *user, AeRam *ram, int32_t angle) {
    AeGame *game = user;
    (void)ram;
    return ae_game_call(game, 0x8002FB50u, (uint32_t)angle, 0u, 0u, 0u);
}

static int32_t items_rcos(void *user, AeRam *ram, int32_t angle) {
    AeGame *game = user;
    (void)ram;
    return ae_game_call(game, 0x8002FC20u, (uint32_t)angle, 0u, 0u, 0u);
}

static uint32_t player1_model_emit_prims(void *user, AeRam *ram,
                                         uint32_t a0, uint32_t a1,
                                         uint32_t a2, uint32_t a3) {
    AeGame *game = user;
    (void)ram;
    return (uint32_t)ae_game_missing_call(game, "ModelEmitPrims",
                                          0x800180B8u, a0, a1, a2, a3);
}

static uint32_t player1_model_emit_prims_cycle_colors(
    void *user, AeRam *ram, uint32_t a0, uint32_t a1,
    uint32_t a2, uint32_t a3) {
    AeGame *game = user;
    if (game->host.renderer.ModelEmitPrimsCycleColors != NULL)
        return (uint32_t)game->host.renderer.ModelEmitPrimsCycleColors(
            game->host.user, ram, a0, a1, a2, a3);
    if (game->host.render != NULL)
        return (uint32_t)game->host.render(game->host.user, ram,
            "ModelEmitPrimsCycleColors", a0, a1, a2, a3);
    return 0u;
}

static int32_t stage_fade_vsync(void *user, AeRam *ram, uint32_t a0,
                                uint32_t a1, uint32_t a2, uint32_t a3) {
    AeGame *game = user;
    (void)a1; (void)a2; (void)a3;
    if (game->host.platform.VSync == NULL) return 0;
    return game->host.platform.VSync(game->host.user, ram, a0);
}

static int32_t stage_fade_stop_voices(void *user, AeRam *ram, uint32_t a0,
                                      uint32_t a1, uint32_t a2,
                                      uint32_t a3) {
    AeGame *game = user;
    uint32_t args[1] = {a0};
    (void)a1; (void)a2; (void)a3;
    if (game->host.missing_words == NULL) return 0;
    return game->host.missing_words(game->host.user, ram,
                                    "SoundStopAllVoices", 0x800A342Cu,
                                    1u, args);
}

static int32_t pad_get_state(void *user, AeRam *ram, int32_t port) {
    AeGame *game = user;
    if (game->host.platform.PadGetState == NULL) return 0;
    return game->host.platform.PadGetState(game->host.user, ram,
                                           (uint32_t)port);
}

static int32_t camupd_pad_get_state(void *user, AeRam *ram, uint32_t port,
                                    uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a1; (void)a2; (void)a3;
    return pad_get_state(user, ram, (int32_t)port);
}

static int32_t pad_info_mode(void *user, AeRam *ram, int32_t port,
                             int32_t mode, int32_t lock) {
    AeGame *game = user;
    if (game->host.platform.PadInfoMode == NULL) return 0;
    return game->host.platform.PadInfoMode(game->host.user, ram,
                                           (uint32_t)port, (uint32_t)mode,
                                           (uint32_t)lock);
}

static void pad_setup_dualshock(void *user, AeRam *ram, int32_t mode) {
    AeGame *game = user;
    uint32_t arg = (uint32_t)mode;
    if (game->host.missing_words != NULL)
        (void)game->host.missing_words(game->host.user, ram, "PadSetupDualShock",
                                       0x80012CA0u, 1u, &arg);
}

static int32_t pad_demo_step(void *user, AeRam *ram) {
    AeGame *game = user;
    return ae_game_call(game, 0x80013128u, 0u, 0u, 0u, 0u);
}

static void pad_stage_request(void *user, AeRam *ram, int32_t request,
                              int32_t area, int32_t entry) {
    AeGame *game = user;
    (void)ram;
    (void)ae_game_call(game, 0x8005D100u, (uint32_t)request,
                       (uint32_t)area, (uint32_t)entry, 0u);
}

static uint32_t pad_stick_to_buttons(void *user, AeRam *ram) {
    AeGame *game = user;
    uint32_t args[1] = {
        ~(uint32_t)(ae_rd8(ram, 0x800E52D3u) |
                    (ae_rd8(ram, 0x800E52D2u) << 8))
    };
    if (game->host.missing_words == NULL) return args[0];
    return (uint32_t)game->host.missing_words(game->host.user, ram,
                                              "StickToButtons", 0x800140B4u,
                                              1u, args);
}

static void pad_rumble_reset(void *user, AeRam *ram, int32_t port) {
    AeGame *game = user;
    uint32_t arg = (uint32_t)port;
    if (game->host.missing_words != NULL)
        (void)game->host.missing_words(game->host.user, ram, "PadRumbleReset",
                                       0x80014208u, 1u, &arg);
}

void ae_game_wire_modules(AeGame *game) {
    game->camupd.user = game;
    game->camupd.PadGetState = camupd_pad_get_state;
    game->items_init.user = game;
    game->items_init.GetLevelFirstArea = items_get_level_first_area;
    game->items_init.printf = items_printf;
    game->items_init.SetBackColor = items_set_back_color;
    game->items_init.SetFarColor = items_set_far_color;
    game->items_init.SetColorMatrix = items_set_color_matrix;
    game->items_init.rsin = items_rsin;
    game->items_init.rcos = items_rcos;
    game->player1.user = game;
    game->player1.ModelEmitPrims = player1_model_emit_prims;
    game->player1.ModelEmitPrimsCycleColors =
        player1_model_emit_prims_cycle_colors;
    game->player1.sound_hw = &game->host.sound_hw;
    game->stage.user = game;
    game->stage.VSync = stage_fade_vsync;
    game->stage_fade.user = game;
    game->stage_fade.stage = &game->stage;
    game->stage_fade.sound_hw = &game->host.sound_hw;
    game->stage_fade.SoundStopAllVoices = stage_fade_stop_voices;
    game->pad.user = game;
    game->pad.pad_get_state = pad_get_state;
    game->pad.pad_info_mode = pad_info_mode;
    game->pad.pad_setup_dualshock = pad_setup_dualshock;
    game->pad.pad_demo_step = pad_demo_step;
    game->pad.stage_request = pad_stage_request;
    game->pad.stick_to_buttons = pad_stick_to_buttons;
    game->pad.pad_rumble_reset = pad_rumble_reset;
}
