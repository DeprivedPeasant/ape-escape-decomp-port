/* Test-only adapter for the missb RAM-image ports. */
#include "../../src/game/ae_game.h"
#include "../../src/ui/ae_missb.h"

enum {
    MISSB_EVENT_WAIT_IDLE = 1,
    MISSB_EVENT_WAIT_LOOP,
    MISSB_EVENT_SET_ADSR,
    MISSB_EVENT_KEY_OFF,
    MISSB_EVENT_PRINTF,
    MISSB_EVENT_SET_VOLUME,
    MISSB_EVENT_SET_DRAW_TPAGE,
    MISSB_EVENT_SOUND_LOOPS_SET_PAN,
    MISSB_EVENT_PROGRESS_OVERLAY
};

typedef struct AeMissbTestEvent {
    uint32_t kind;
    uint32_t values[6];
} AeMissbTestEvent;

static AeGame missb_test_game;
static AeMissbHooks missb_test_hooks;
static AeMissbTestEvent missb_test_events[4096];
static uint32_t missb_test_event_count;

static void missb_test_record(uint32_t kind, uint32_t a, uint32_t b,
                              uint32_t c, uint32_t d, uint32_t e,
                              uint32_t f)
{
    if (missb_test_event_count < 4096u) {
        AeMissbTestEvent *event = &missb_test_events[missb_test_event_count++];
        event->kind = kind;
        event->values[0] = a;
        event->values[1] = b;
        event->values[2] = c;
        event->values[3] = d;
        event->values[4] = e;
        event->values[5] = f;
    }
}

static int32_t missb_test_wait_idle(void *user, AeRam *ram)
{
    (void)user;
    (void)ram;
    missb_test_record(MISSB_EVENT_WAIT_IDLE, 0u, 0u, 0u, 0u, 0u, 0u);
    return 0;
}

static void missb_test_wait_loop(void *user, AeRam *ram, uint32_t units)
{
    (void)user;
    (void)ram;
    missb_test_record(MISSB_EVENT_WAIT_LOOP, units, 0u, 0u, 0u, 0u, 0u);
}

static void missb_test_set_voice_adsr(void *user, AeRam *ram,
                                      uint32_t channel, uint16_t adsr1,
                                      uint16_t adsr2)
{
    (void)user;
    (void)ram;
    missb_test_record(MISSB_EVENT_SET_ADSR, channel, adsr1, adsr2, 0u, 0u, 0u);
}

static void missb_test_voice_key_off(void *user, AeRam *ram, uint32_t half,
                                     uint16_t mask)
{
    uint32_t shadow = ae_rd32(ram, 0x800E1874u);
    (void)user;
    ae_wr16(ram, shadow + (half == 0u ? 0x18Cu : 0x18Eu), mask);
    missb_test_record(MISSB_EVENT_KEY_OFF, half, mask, 0u, 0u, 0u, 0u);
}

static void missb_test_printf_error(void *user, AeRam *ram, uint32_t format,
                                    uint32_t argc, int32_t a, int32_t b,
                                    int32_t c)
{
    (void)user;
    (void)ram;
    missb_test_record(MISSB_EVENT_PRINTF, format, argc, (uint32_t)a,
                      (uint32_t)b, (uint32_t)c, 0u);
}

static void missb_test_set_voice_volume(void *user, AeRam *ram,
                                        uint32_t channel, uint32_t side,
                                        uint16_t volume)
{
    uint32_t shadow = ae_rd32(ram, 0x800E1874u) + channel * 16u + side * 2u;
    (void)user;
    ae_wr16(ram, shadow, volume);
    missb_test_record(MISSB_EVENT_SET_VOLUME, channel, side, volume, 0u, 0u, 0u);
}

static int32_t missb_test_progress_set_tpage(void *user, AeRam *ram,
                                             uint32_t packet,
                                             uint32_t semi_transparent,
                                             uint32_t draw_page,
                                             uint32_t tpage)
{
    uint32_t value = draw_page != 0u ? 0xE1000200u : 0u;
    (void)user;
    missb_test_record(MISSB_EVENT_SET_DRAW_TPAGE, packet, semi_transparent,
                      draw_page, tpage, 0u, 0u);
    value |= tpage & 0x9FFu;
    if (semi_transparent != 0u) value |= 0x400u;
    ae_wr8(ram, packet + 3u, 1u);
    ae_wr32(ram, packet + 4u, value);
    return 0;
}

static int32_t missb_test_progress_sound_loops(void *user, AeRam *ram,
                                               uint32_t a0, uint32_t a1,
                                               uint32_t a2, uint32_t a3)
{
    (void)user;
    (void)ram;
    missb_test_record(MISSB_EVENT_SOUND_LOOPS_SET_PAN, a0, a1, a2, a3, 0u, 0u);
    return 0;
}

static int32_t missb_test_progress_overlay(void *user, AeRam *ram,
                                           uint32_t a0, uint32_t a1,
                                           uint32_t a2, uint32_t a3)
{
    (void)user;
    (void)ram;
    missb_test_record(MISSB_EVENT_PROGRESS_OVERLAY, a0, a1, a2, a3, 0u, 0u);
    return 0;
}

static int32_t missb_test_progress_sound_bank(void *user, AeRam *ram,
                                             uint32_t bank, uint32_t volume,
                                             uint32_t a2, uint32_t a3)
{
    (void)user;
    (void)a2;
    (void)a3;
    return ae_SoundBankSetVolume(ram, &missb_test_hooks, (int32_t)bank,
                                 (int32_t)volume);
}

static int32_t missb_test_player_enter_cutscene(void *user, AeRam *ram,
                                                uint32_t a0, uint32_t a1,
                                                uint32_t a2, uint32_t a3)
{
    (void)user;
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    return ae_PlayerEnterCutsceneMode(ram, &missb_test_hooks);
}

static int32_t missb_test_missing(void *user, AeRam *ram, const char *name,
                                  uint32_t address, uint32_t a0, uint32_t a1,
                                  uint32_t a2, uint32_t a3)
{
    (void)user;
    (void)ram;
    (void)name;
    missb_test_record(10u, address, 4u, a0, a1, a2, a3);
    return 0;
}

void ae_missb_test_prepare(AeRam *ram, AeMissbCallFn call, void *call_user,
                           uint32_t stack_pointer, uint32_t caller_a3,
                           int32_t caller_s1, int32_t caller_s2)
{
    AeGameHost host = {0};
    host.missing = missb_test_missing;
    host.sound_hw.wait_idle = missb_test_wait_idle;
    host.platform.SpuWaitIdle = missb_test_wait_idle;
    host.sound_hw.wait_loop = missb_test_wait_loop;
    host.sound_hw.set_voice_adsr = missb_test_set_voice_adsr;
    host.sound_hw.voice_key_off = missb_test_voice_key_off;
    host.sound_hw.printf_error = missb_test_printf_error;
    host.sound_hw.set_voice_volume = missb_test_set_voice_volume;

    ae_game_init(&missb_test_game, ram, &host);
    missb_test_game.stage.PlayerEnterCutsceneMode = missb_test_player_enter_cutscene;

    missb_test_hooks.user = call_user;
    missb_test_hooks.stack_pointer = stack_pointer;
    missb_test_hooks.caller_a3 = caller_a3;
    missb_test_hooks.caller_s1 = caller_s1;
    missb_test_hooks.caller_s2 = caller_s2;
    missb_test_hooks.call = call;
    missb_test_hooks.misc = &missb_test_game.misc;
    missb_test_hooks.progress = &missb_test_game.progress;
    missb_test_hooks.pause_menu = &missb_test_game.pause_menu;
    missb_test_hooks.hud = &missb_test_game.hud;
    missb_test_hooks.sound_hw = &missb_test_game.host.sound_hw;
    missb_test_hooks.stage_fade = &missb_test_game.stage_fade;
    missb_test_hooks.camupd = &missb_test_game.camupd;
    missb_test_hooks.player_act = &missb_test_game.player_act;
    missb_test_hooks.area_set = &missb_test_game.area_set;
    missb_test_hooks.gimmick_a = &missb_test_game.gimmick_a;
    missb_test_hooks.zones = &missb_test_game.zones;
    missb_test_hooks.world = &missb_test_game.world;
    missb_test_hooks.player1 = &missb_test_game.player1;
    missb_test_hooks.stage = &missb_test_game.stage;
    missb_test_game.progress.SetDrawTPage = missb_test_progress_set_tpage;
    missb_test_game.progress.SoundLoopsSetPan = missb_test_progress_sound_loops;
    missb_test_game.progress.FUN_8013FB20 = missb_test_progress_overlay;
    missb_test_game.progress.SoundBankSetVolume = missb_test_progress_sound_bank;
}

const AeMissbHooks *ae_missb_test_hooks(void)
{
    return &missb_test_hooks;
}

void ae_missb_test_record_generic(uint32_t address, uint32_t argc,
                                  const uint32_t args[4])
{
    missb_test_record(10u, address, argc,
                      argc > 0u ? args[0] : 0u,
                      argc > 1u ? args[1] : 0u,
                      argc > 2u ? args[2] : 0u,
                      argc > 3u ? args[3] : 0u);
}

void ae_missb_test_clear_events(void)
{
    missb_test_event_count = 0u;
}

uint32_t ae_missb_test_event_count_get(void)
{
    return missb_test_event_count;
}

int32_t ae_missb_test_event_get(uint32_t index, AeMissbTestEvent *out)
{
    if (index >= missb_test_event_count || out == NULL) return 0;
    *out = missb_test_events[index];
    return 1;
}
