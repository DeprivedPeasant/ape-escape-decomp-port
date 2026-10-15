/* Integration-only instrumentation; no game data or game logic. */
#include "../../src/game/ae_calls.h"
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

typedef int32_t (*Trace)(void *, const char *, uint32_t, uint32_t, const uint32_t *);
typedef struct Integration {
    AeGame game;
    Trace trace;
    void *user;
    const uint8_t *member;
    size_t size;
    const uint8_t *overlay;
    size_t overlay_size;
    uint32_t overlay_member, overlay_address;
    uint16_t direction_padding;
    uint16_t spawn_padding[64];
    uint32_t spawn_padding_count, spawn_padding_index;
    uint32_t renderer_words_calls;
    char missing_labels[1024][128];
    jmp_buf stop;
    int bounded, stop_requested;
    uint32_t bounded_callbacks;
    uint32_t xa_padding[64][2], xa_count, xa_index;
} Integration;

void integration_set_xa_padding(Integration *s, const uint32_t *words, uint32_t count) {
    s->xa_count = count > 64u ? 64u : count;
    s->xa_index = 0;
    if (s->xa_count) memcpy(s->xa_padding, words, s->xa_count * sizeof(s->xa_padding[0]));
}
static uint32_t xa_caller_padding(void *user, AeRam *ram, uint32_t local) {
    (void)ram;
    Integration *s = user;
    if (local > 1u || s->xa_index >= s->xa_count) return 0;
    uint32_t value = s->xa_padding[s->xa_index][local];
    if (local == 1u) ++s->xa_index;
    return value;
}

void integration_stop(Integration *s) { s->stop_requested = 1; }
static int32_t integration_trace(Integration *s, const char *name, uint32_t address,
                                 uint32_t count, const uint32_t *words) {
    if (s->bounded && ++s->bounded_callbacks > 20000u) {
        ae_ram_fail(s->game.ram, "integration native callback budget exhausted");
        longjmp(s->stop, 1);
    }
    int32_t result = s->trace(s->user, name, address, count, words);
    if (s->bounded && s->stop_requested) longjmp(s->stop, 1);
    return result;
}

void integration_set_direction_padding(Integration *s, uint32_t padding) { s->direction_padding = (uint16_t)padding; }
int32_t integration_missa_trail_words(Integration *s, uint32_t a0, uint32_t a1,
                                     uint32_t a2, uint32_t a3, uint32_t a4) {
    AeGameRenderer saved = s->game.host.renderer;
    s->game.host.renderer.RenderTrailRingSegment = NULL;
    int32_t result = s->game.missa.TrailRingDraw(s->game.missa.user,
        s->game.ram, a0, a1, a2, a3, a4);
    s->game.host.renderer = saved;
    return result;
}
static uint32_t direction_padding(void *user, AeRam *ram) {
    (void)ram; return ((Integration *)user)->direction_padding;
}
static Integration *spawn_padding_owner;
static uint16_t integration_spawn_position_padding(void *user, AeRam *ram, uint32_t function) {
    (void)user; (void)ram; (void)function;
    Integration *s = spawn_padding_owner;
    if (s == NULL || s->spawn_padding_index >= s->spawn_padding_count) return 0;
    return s->spawn_padding[s->spawn_padding_index++];
}

static int32_t trace_call(Integration *s, const char *name, uint32_t address, uint32_t argc,
                           uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5) {
    const uint32_t args[6] = {a0, a1, a2, a3, a4, a5};
    return integration_trace(s, name, address, argc, args);
}
static void record_missing(Integration *s, const char *name, uint32_t address) {
    AeGame *g = &s->game;
    for (size_t i = 0; i < g->missing_address_count; ++i)
        if (g->missing_addresses[i] == address && !strcmp(g->missing_names[i], name)) return;
    if (g->missing_address_count < 1024) {
        size_t index = g->missing_address_count++;
        strncpy(s->missing_labels[index], name, 127);
        s->missing_labels[index][127] = 0;
        g->missing_names[index] = s->missing_labels[index];
        g->missing_addresses[index] = address;
    }
}
static int32_t missing_call(void *user, AeRam *r, const char *name, uint32_t address,
                              uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    record_missing(user, name, address);
    return trace_call(user, name, address, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t missing_words(void *user, AeRam *r, const char *name, uint32_t address,
                             uint32_t count, const uint32_t *words) {
    Integration *s = user;
    (void)r;
    record_missing(s, name, address);
    return integration_trace(s, name, address, count, words);
}
static int32_t render_call(void *user, AeRam *r, const char *name,
                             uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return trace_call(user, name, 0, 4, a0, a1, a2, a3, 0, 0);
}
static int32_t render_words_call(void *user, AeRam *r, const char *name, uint32_t address,
                                 uint32_t count, const uint32_t *words) {
    ++((Integration *)user)->renderer_words_calls;
    (void)r;
    Integration *s = user;
    return integration_trace(s, name, address, count, words);
}
static int32_t card_call(void *user, AeRam *r, const char *name, uint32_t address,
                         uint32_t count, const uint32_t *words) {
    (void)r;
    Integration *s = user;
    return integration_trace(s, name, address, count, words);
}
static void debug_call(void *user, AeRam *r, const char *name,
                        uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    (void)trace_call(user, name, 0x800A3720u, 4, a0, a1, a2, a3, 0, 0);
}
static int area_member(void *user, int32_t area, const uint8_t **bytes, size_t *size) {
    Integration *s = user;
    (void)area;
    *bytes = s->member; *size = s->size;
    return s->member != NULL;
}
static int area_overlay(void *user, int32_t area, uint32_t *member, uint32_t *address,
                          const uint8_t **bytes, size_t *size) {
    Integration *s = user;
    (void)area;
    *member = s->overlay_member; *address = s->overlay_address;
    *bytes = s->overlay; *size = s->overlay_size;
    return s->overlay != NULL;
}

/* Named platform and renderer callbacks generated for the test trace. */
#include "game_integration_callbacks.inc"

static void sound_wait(void *user, AeRam *r, uint32_t units) {
    (void)r; trace_call(user, "SpuWaitLoop", 0x8009D79Cu, 1, units, 0, 0, 0, 0, 0);
}
static void sound_adsr(void *user, AeRam *r, uint32_t ch, uint16_t a, uint16_t b) {
    (void)r; trace_call(user, "SoundAdsr", 0, 3, ch, a, b, 0, 0, 0);
}
static void sound_off(void *user, AeRam *r, uint32_t half, uint16_t mask) {
    (void)r; trace_call(user, "SoundKeyOff", 0, 2, half, mask, 0, 0, 0, 0);
}
static void sound_volume(void *user, AeRam *r, uint32_t ch, uint32_t side, uint16_t volume) {
    (void)r; trace_call(user, "SoundVolume", 0, 3, ch, side, volume, 0, 0, 0);
}
static void sound_printf(void *user, AeRam *r, uint32_t fmt, uint32_t count, int32_t a, int32_t b, int32_t c) {
    (void)r; (void)count; trace_call(user, "Printf", 0x800A3720u, 4, fmt, (uint32_t)a, (uint32_t)b, (uint32_t)c, 0, 0);
}

Integration *integration_create(Trace trace, void *user) {
    Integration *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    AeRam *r = ae_ram_create();
    if (!r) { free(s); return NULL; }
    s->trace = trace; s->user = user;
    AeGameHost host;
    memset(&host, 0, sizeof(host));
    host.user = s;
    host.card_call = card_call;
    host.xa_caller_padding = xa_caller_padding;
    host.missing = missing_call; host.missing_words = missing_words; host.render = render_call; host.render_words = render_words_call; host.debug = debug_call;
    host.body_direction_padding = direction_padding;
    host.area_member = area_member;
    host.area_overlay = area_overlay;
    integration_callbacks(&host);
    host.sound_hw.user = s;
    host.sound_hw.wait_idle = test_platform_SpuWaitIdle;
    host.sound_hw.wait_loop = sound_wait;
    host.sound_hw.set_voice_adsr = sound_adsr;
    host.sound_hw.voice_key_off = sound_off;
    host.sound_hw.set_voice_volume = sound_volume;
    host.sound_hw.printf_error = sound_printf;
    ae_game_init(&s->game, r, &host);
    return s;
}
void integration_destroy(Integration *s) {
    if (s) { ae_ram_destroy(s->game.ram); free(s); }
}
AeRam *integration_ram(Integration *s) { return s->game.ram; }
void integration_set_loaded_overlay(Integration *s, uint32_t member) { s->game.loaded_overlay = member; }
uint32_t integration_renderer_words_calls(Integration *s) { return s->renderer_words_calls; }
int32_t integration_overlay_words(Integration *s, uint32_t render, uint32_t address,
                                 uint32_t count, const uint32_t *words) {
    const AeOvl18cHooks *h = &s->game.ovl18c;
    return (render ? h->renderer : h->original_call)(h->user, s->game.ram,
             render ? "OverlayRenderProbe" : "OverlayCallProbe", address, count, words);
}
void integration_set_hud_draw(Integration *s, void (*draw)(void *, AeRam *, const AeHudDraw *)) {
    s->game.host.hud_draw = draw;
}
AeBodyHooks *integration_body_hooks(Integration *s) { return &s->game.body; }
AeBodyMoveHooks *integration_bodymove_hooks(Integration *s) { return &s->game.body.move; }
AeSoundHw *integration_sound_hw(Integration *s) { return &s->game.host.sound_hw; }
void integration_set_bodymove_sp(Integration *s, uint32_t sp) { s->game.body.move.stack_pointer = sp; }
void integration_set_spawn_padding(Integration *s, const uint16_t *values, uint32_t count) {
    if (count > 64u) count = 64u;
    if (count != 0u) memcpy(s->spawn_padding, values, count * sizeof(*values));
    s->spawn_padding_count = count;
    s->spawn_padding_index = 0;
    spawn_padding_owner = s;
    s->game.spawn.position_padding = integration_spawn_position_padding;
}
uint32_t integration_spawn_padding_consumed(Integration *s) { return s->spawn_padding_index; }
const AePlayerUpdateFrameHooks *integration_playerupdate_hooks(Integration *s) {
    return &s->game.player_update_frame;
}
int32_t integration_call(Integration *s, uint32_t address, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    return ae_game_call(&s->game, address, a0, a1, a2, a3);
}
int32_t integration_wrapper(Integration *s, uint32_t index, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeGameCall *call = ae_game_call_at(index);
    return call ? call->invoke(&s->game, a0, a1, a2, a3) : 0;
}
int32_t integration_load_area(Integration *s, int32_t area, int32_t entry, const uint8_t *member, size_t size) {
    s->member = member; s->size = size;
    int32_t result = ae_game_load_area(&s->game, area, entry);
    s->member = NULL; s->size = 0;
    s->overlay = NULL; s->overlay_size = 0;
    return result;
}
void integration_set_overlay(Integration *s, uint32_t member, uint32_t address, const uint8_t *bytes, size_t size) {
    s->overlay_member = member; s->overlay_address = address;
    s->overlay = bytes; s->overlay_size = size;
}
int integration_frame(Integration *s) { return ae_game_frame(&s->game); }
int32_t integration_hook(Integration *s, uint32_t index, uint32_t a0, uint32_t a1,
                         uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6) {
    AeGame *g = &s->game;
    switch (index) {
#include "game_integration_hooks.inc"
    default: return 0;
    }
}

/* Observe the callback's o32 stack while the central helper is active. */
static int32_t stack_probe(void *user, AeRam *r, const char *name, uint32_t address,
                           uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    Integration *s = user;
    uint32_t sp = s->game.call_stack_pointer;
    const uint32_t words[7] = {a0, a1, a2, a3, ae_rd32(r, sp + 16u),
                               ae_rd32(r, sp + 20u), ae_rd32(r, sp + 24u)};
    (void)name; (void)address;
    return s->trace(s->user, "TestStackWords", 0, 7, words);
}
int32_t integration_stack7(Integration *s, uint32_t a0, uint32_t a1, uint32_t a2,
                            uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6) {
    AeGameHost saved = s->game.host;
    s->game.host.missing = stack_probe;
    int32_t result = ae_game_call7(&s->game, 0, a0, a1, a2, a3, a4, a5, a6);
    s->game.host = saved;
    return result;
}

static int mode_continue(void *user, AeRam *r) {
    Integration *s = user;
    return trace_call(s, "ModeContinue", 0, 1, ae_rd16(r, 0x800F4470u), 0, 0, 0, 0, 0);
}
int32_t integration_run(Integration *s) {
    s->game.host.mode_continue = mode_continue;
    s->stop_requested = 0;
    s->bounded_callbacks = 0;
    s->bounded = 1;
    int32_t result = 0;
    if (!setjmp(s->stop)) result = ae_game_run(&s->game);
    s->bounded = 0;
    return result;
}

int32_t integration_bounded_call(Integration *s, uint32_t address) {
    s->stop_requested = 0;
    s->bounded_callbacks = 0;
    s->bounded = 1;
    int32_t result = 0;
    if (!setjmp(s->stop)) result = ae_game_call(&s->game, address, 0, 0, 0, 0);
    s->bounded = 0;
    return result;
}

size_t integration_missing_count(Integration *s) { return s->game.missing_address_count; }
const char *integration_missing_name(Integration *s, size_t index) {
    return index < s->game.missing_address_count ? s->game.missing_names[index] : NULL;
}
uint32_t integration_missing_address(Integration *s, size_t index) {
    return index < s->game.missing_address_count ? s->game.missing_addresses[index] : 0;
}
int32_t integration_mode_words(Integration *s, const char *name, uint32_t address,
                                uint32_t count, const uint32_t *words) {
    s->stop_requested = 0;
    s->bounded_callbacks = 0;
    s->bounded = 1;
    int32_t result = 0;
    if (!setjmp(s->stop)) result = ae_game_mode_call(&s->game, s->game.ram, name, address, count, words);
    s->bounded = 0;
    return result;
}

int32_t integration_missb_words(Integration *s, const char *name, uint32_t address,
                                uint32_t count, const uint32_t *words) {
    uint32_t a[4] = {0};
    if (count > 4u) { ae_ram_fail(s->game.ram, "MissB test exceeds four words"); return 0; }
    for (uint32_t i = 0; i < count; ++i) a[i] = words[i];
    return ae_missb_call(s->game.ram, &s->game.missb, name, address,
                         count, a[0], a[1], a[2], a[3]);
}
