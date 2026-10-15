/* Test shim of tools/mips/check_game.py (NOT part of src/): creates an AeGame whose AeGameHost callbacks forward to
 * plain C function pointers (ctypes callbacks of the checker), so that the missing / render / debug / vblank / sound
 * hardware calls of src/game/ae_game.c are recorded and answered by the same Python model that stands in for them on the
 * MIPS side.  Built together with src/game/ae_game.c and every module it links. */
#include "../../src/game/ae_game.h"

#include <stdlib.h>
#include <string.h>

typedef struct ShimCallbacks {
    int32_t (*missing)(const char *name, uint32_t address, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
    int32_t (*render)(const char *name, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
    void (*debug)(const char *name, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
    int32_t (*vblank_count)(void);
    int32_t (*wait_idle)(void);
    void (*wait_loop)(uint32_t units);
    void (*set_voice_adsr)(uint32_t channel, uint32_t adsr1, uint32_t adsr2);
    void (*voice_key_off)(uint32_t half, uint32_t mask);
    void (*set_voice_volume)(uint32_t channel, uint32_t side, uint32_t volume);
    void (*printf_error)(uint32_t fmt, uint32_t argc, int32_t a, int32_t b, int32_t c);
} ShimCallbacks;

typedef struct Shim {
    ShimCallbacks cb;
    AeRam *ram;
    AeGame game;
    uint16_t padding[256];
    uint32_t padding_count, padding_index;
} Shim;

static uint16_t captured_padding(void *user, AeRam *ram, uint32_t function) {
    Shim *s=((AeGame *)user)->host.user;
    (void)ram; (void)function;
    return s->padding_index<s->padding_count ? s->padding[s->padding_index++] : 0;
}
void shim_set_spawn_padding(Shim *s, const uint16_t *values, uint32_t count) {
    if (count>256) count=256;
    memcpy(s->padding,values,count*sizeof(*values));
    s->padding_count=count; s->padding_index=0;
    s->game.spawn.position_padding=captured_padding;
}

static int32_t host_missing(void *user, AeRam *r, const char *name, uint32_t address,
                            uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return ((Shim *)user)->cb.missing(name, address, a0, a1, a2, a3);
}
static int32_t host_render(void *user, AeRam *r, const char *name, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    return ((Shim *)user)->cb.render(name, a0, a1, a2, a3);
}
static void host_debug(void *user, AeRam *r, const char *name, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)r;
    ((Shim *)user)->cb.debug(name, a0, a1, a2, a3);
}
static int32_t host_vblank(void *user) { return ((Shim *)user)->cb.vblank_count(); }
static int32_t hw_wait_idle(void *user, AeRam *r) { (void)r; return ((Shim *)user)->cb.wait_idle(); }
static void hw_wait_loop(void *user, AeRam *r, uint32_t units) { (void)r; ((Shim *)user)->cb.wait_loop(units); }
static void hw_adsr(void *user, AeRam *r, uint32_t channel, uint16_t adsr1, uint16_t adsr2) {
    (void)r;
    ((Shim *)user)->cb.set_voice_adsr(channel, adsr1, adsr2);
}
static void hw_key_off(void *user, AeRam *r, uint32_t half, uint16_t mask) {
    (void)r;
    ((Shim *)user)->cb.voice_key_off(half, mask);
}
static void hw_volume(void *user, AeRam *r, uint32_t channel, uint32_t side, uint16_t volume) {
    (void)r;
    ((Shim *)user)->cb.set_voice_volume(channel, side, volume);
}
static void hw_printf(void *user, AeRam *r, uint32_t fmt, uint32_t argc, int32_t a, int32_t b, int32_t c) {
    (void)r;
    ((Shim *)user)->cb.printf_error(fmt, argc, a, b, c);
}

Shim *shim_create(const ShimCallbacks *cb) {
    Shim *s = (Shim *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->cb = *cb;
    s->ram = ae_ram_create();
    if (!s->ram) { free(s); return NULL; }
    AeGameHost host;
    memset(&host, 0, sizeof(host));
    host.user = s;
    host.missing = host_missing;
    host.render = host_render;
    host.debug = host_debug;
    host.vblank_count = host_vblank;
    host.sound_hw.user = s;
    host.sound_hw.wait_idle = hw_wait_idle;
    host.sound_hw.wait_loop = hw_wait_loop;
    host.sound_hw.set_voice_adsr = hw_adsr;
    host.sound_hw.voice_key_off = hw_key_off;
    host.sound_hw.set_voice_volume = hw_volume;
    host.sound_hw.printf_error = hw_printf;
    ae_game_init_legacy(&s->game, s->ram, &host);
    return s;
}

AeRam *shim_ram(Shim *s) { return s->ram; }
int shim_frame(Shim *s) { return ae_game_logic_frame(&s->game); }
/* bit per MISSING entry that ae_game.c reported itself (only without a host.missing callback; unused here) */
uint32_t shim_missing_seen(const Shim *s, int word) { return s->game.missing_seen[word & 3]; }

/* ---- direct calls of the wired hook tables (the checker's per-hook layer) ------------------------------------------------ */
#include <stddef.h>

/* The hook tables are arrays of function pointers after their leading members; the checker indexes them in header order. */
int shim_monkey_hook_count(void) {
    return (int)((sizeof(AeMonkeyHooks) - offsetof(AeMonkeyHooks, Printf)) / sizeof(AeMonkeyHookFn));
}
int shim_world_hook_count(void) {
    return (int)((offsetof(AeWorldHooks, ground_stale_x) - offsetof(AeWorldHooks, GetSolidHeight)) / sizeof(AeWorldHookFn));
}
int shim_gimmick_hook_count(void) {
    return (int)((sizeof(AeGimmickHooks) - offsetof(AeGimmickHooks, PlaySfx)) / sizeof(AeGimmickHookFn));
}
int shim_spawn_hook_count(void) {
    return (int)((offsetof(AeSpawnHooks, position_padding) - offsetof(AeSpawnHooks, PlaySfx)) / sizeof(AeSpawnHookFn));
}

static int32_t null_hook(Shim *s, const char *what) {
    ae_ram_fail(s->ram, "shim: %s hook is NULL", what);
    return 0;
}
#define NULL_HOOK(s, what) null_hook((s), what)

int32_t shim_monkey_hook(Shim *s, int index, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    AeMonkeyHookFn *fns = (AeMonkeyHookFn *)((char *)&s->game.monkey + offsetof(AeMonkeyHooks, Printf));
    if (!fns[index]) return NULL_HOOK(s, "monkey");
    return fns[index](s->game.monkey.user, s->ram, a0, a1, a2, a3);
}
int32_t shim_world_hook(Shim *s, int index, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    AeWorldHookFn *fns = (AeWorldHookFn *)((char *)&s->game.world + offsetof(AeWorldHooks, GetSolidHeight));
    if (!fns[index]) return NULL_HOOK(s, "world");
    return fns[index](s->game.world.user, s->ram, a0, a1, a2, a3);
}
int32_t shim_gimmick_hook(Shim *s, int index, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    AeGimmickHookFn *fns = (AeGimmickHookFn *)((char *)&s->game.gimmick + offsetof(AeGimmickHooks, PlaySfx));
    if (!fns[index]) return NULL_HOOK(s, "gimmick");
    return fns[index](s->game.gimmick.user, s->ram, a0, a1, a2, a3);
}
int32_t shim_spawn_hook(Shim *s, int index, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    AeSpawnHookFn *fns = (AeSpawnHookFn *)((char *)&s->game.spawn + offsetof(AeSpawnHooks, PlaySfx));
    if (!fns[index]) return NULL_HOOK(s, "spawn");
    return fns[index](s->game.spawn.user, s->ram, a0, a1, a2, a3);
}
int shim_spawn_position_padding(Shim *s, uint32_t original_function) {
    if (!s->game.spawn.position_padding) return NULL_HOOK(s, "spawn position_padding");
    return s->game.spawn.position_padding(s->game.spawn.user, s->ram, original_function);
}
/* the spawn / world tables must point at each other */
int shim_tables_linked(Shim *s) {
    return s->game.spawn.world == &s->game.world && s->game.monkey.body == &s->game.body;
}

void shim_body_printf_error(Shim *s, uint32_t fmt, int32_t type) {
    if (!s->game.body.printf_error) { NULL_HOOK(s, "body.printf_error"); return; }
    s->game.body.printf_error(s->game.body.user, s->ram, fmt, type);
}
int32_t shim_body_sound_loop_start(Shim *s, int32_t a, int32_t id) {
    if (!s->game.body.sound_loop_start) return NULL_HOOK(s, "body.sound_loop_start");
    return s->game.body.sound_loop_start(s->game.body.user, s->ram, a, id);
}
void shim_body_sound_loop_stop(Shim *s, int32_t id, int32_t handle) {
    if (!s->game.body.sound_loop_stop) { NULL_HOOK(s, "body.sound_loop_stop"); return; }
    s->game.body.sound_loop_stop(s->game.body.user, s->ram, id, handle);
}
void shim_body_build_probe_points(Shim *s, int32_t type, uint32_t body) {
    if (!s->game.body.build_probe_points) { NULL_HOOK(s, "body.build_probe_points"); return; }
    s->game.body.build_probe_points(s->game.body.user, s->ram, type, body);
}
int32_t shim_body_collision_ray_test(Shim *s, uint32_t arg, uint32_t params, int16_t *out) {
    if (!s->game.body.collision_ray_test) return NULL_HOOK(s, "body.collision_ray_test");
    return s->game.body.collision_ray_test(s->game.body.user, s->ram, arg, params, out);
}
int32_t shim_body_gimmick_type_to_sfx(Shim *s, int32_t type) {
    if (!s->game.body.gimmick_type_to_sfx) return NULL_HOOK(s, "body.gimmick_type_to_sfx");
    return s->game.body.gimmick_type_to_sfx(s->game.body.user, s->ram, type);
}
void shim_body_play_sfx_at(Shim *s, int32_t sfx, const int16_t *pos) {
    if (!s->game.body.play_sfx_at) { NULL_HOOK(s, "body.play_sfx_at"); return; }
    s->game.body.play_sfx_at(s->game.body.user, s->ram, sfx, pos);
}
void shim_body_pickup_collect(Shim *s, int32_t type) {
    if (!s->game.body.pickup_collect) { NULL_HOOK(s, "body.pickup_collect"); return; }
    s->game.body.pickup_collect(s->game.body.user, s->ram, type);
}
void shim_body_camera_nudge(Shim *s, int32_t dx, int32_t dy, int32_t dz) {
    if (!s->game.body.camera_nudge) { NULL_HOOK(s, "body.camera_nudge"); return; }
    s->game.body.camera_nudge(s->game.body.user, s->ram, dx, dy, dz);
}
int32_t shim_body_stub_return_zero(Shim *s) {      /* NULL in the game: the port's default (the original returns 0) */
    return s->game.body.body_stub_return_zero ? s->game.body.body_stub_return_zero(s->game.body.user, s->ram) : 0;
}

void shim_anim_play_sfx(Shim *s, int32_t id) {
    if (!s->game.anim.play_sfx) { NULL_HOOK(s, "anim.play_sfx"); return; }
    s->game.anim.play_sfx(s->game.anim.user, s->ram, id);
}
void shim_anim_play_player_voice(Shim *s, int32_t id) {
    if (!s->game.anim.play_player_voice) { NULL_HOOK(s, "anim.play_player_voice"); return; }
    s->game.anim.play_player_voice(s->game.anim.user, s->ram, id);
}
void shim_anim_event_callback(Shim *s, uint32_t function) {
    if (!s->game.anim.event_callback) { NULL_HOOK(s, "anim.event_callback"); return; }
    s->game.anim.event_callback(s->game.anim.user, s->ram, function);
}

/* ---- trace of the monkey hooks the frames call (which wired hooks a scenario reaches) -------------------------------------- */
typedef void (*ShimTraceFn)(int index, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
static ShimTraceFn g_trace;
static AeMonkeyHookFn g_traced[64];
#define TRAMP(n)                                                                                                     \
    static int32_t tramp##n(void *u, AeRam *r, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {                  \
        g_trace(n, a0, a1, a2, a3);                                                                                  \
        return g_traced[n](u, r, a0, a1, a2, a3);                                                                    \
    }
TRAMP(0)
TRAMP(1)
TRAMP(2)
TRAMP(3)
TRAMP(4)
TRAMP(5)
TRAMP(6)
TRAMP(7)
TRAMP(8)
TRAMP(9)
TRAMP(10)
TRAMP(11)
TRAMP(12)
TRAMP(13)
TRAMP(14)
TRAMP(15)
TRAMP(16)
TRAMP(17)
TRAMP(18)
TRAMP(19)
TRAMP(20)
TRAMP(21)
TRAMP(22)
TRAMP(23)
TRAMP(24)
TRAMP(25)
TRAMP(26)
TRAMP(27)
TRAMP(28)
TRAMP(29)
TRAMP(30)
TRAMP(31)
TRAMP(32)
TRAMP(33)
TRAMP(34)
TRAMP(35)
TRAMP(36)
TRAMP(37)
TRAMP(38)
TRAMP(39)
TRAMP(40)
TRAMP(41)
TRAMP(42)
TRAMP(43)
TRAMP(44)
TRAMP(45)
TRAMP(46)
TRAMP(47)
TRAMP(48)
TRAMP(49)
TRAMP(50)
TRAMP(51)
TRAMP(52)
TRAMP(53)
TRAMP(54)
TRAMP(55)
TRAMP(56)
TRAMP(57)
TRAMP(58)
TRAMP(59)
TRAMP(60)
TRAMP(61)
TRAMP(62)
TRAMP(63)
void shim_trace_monkey_hooks(Shim *s, ShimTraceFn cb) {
    static AeMonkeyHookFn tramps[64] = {
        tramp0,
        tramp1,
        tramp2,
        tramp3,
        tramp4,
        tramp5,
        tramp6,
        tramp7,
        tramp8,
        tramp9,
        tramp10,
        tramp11,
        tramp12,
        tramp13,
        tramp14,
        tramp15,
        tramp16,
        tramp17,
        tramp18,
        tramp19,
        tramp20,
        tramp21,
        tramp22,
        tramp23,
        tramp24,
        tramp25,
        tramp26,
        tramp27,
        tramp28,
        tramp29,
        tramp30,
        tramp31,
        tramp32,
        tramp33,
        tramp34,
        tramp35,
        tramp36,
        tramp37,
        tramp38,
        tramp39,
        tramp40,
        tramp41,
        tramp42,
        tramp43,
        tramp44,
        tramp45,
        tramp46,
        tramp47,
        tramp48,
        tramp49,
        tramp50,
        tramp51,
        tramp52,
        tramp53,
        tramp54,
        tramp55,
        tramp56,
        tramp57,
        tramp58,
        tramp59,
        tramp60,
        tramp61,
        tramp62,
        tramp63,
    };
    AeMonkeyHookFn *fns = (AeMonkeyHookFn *)((char *)&s->game.monkey + offsetof(AeMonkeyHooks, Printf));
    int count = shim_monkey_hook_count();
    g_trace = cb;
    if (!cb) return;
    for (int i = 0; i < count && i < 64; ++i) {
        g_traced[i] = fns[i];
        fns[i] = tramps[i];
    }
}
