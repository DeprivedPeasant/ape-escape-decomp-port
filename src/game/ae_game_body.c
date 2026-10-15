/* Typed body / animation adapters: their interfaces predate the word hooks. */
#include "ae_calls.h"
#include "../bodies/ae_bodymove.h"

#define ARG_VECTOR 0x801FFE00u
int32_t ae_game_call5(AeGame *g, uint32_t address, uint32_t a0, uint32_t a1,
                     uint32_t a2, uint32_t a3, uint32_t a4) {
    uint32_t stack_argument = g->call_stack_pointer + 16u;
    uint32_t saved = ae_rd32(g->ram, stack_argument);
    ae_wr32(g->ram, stack_argument, a4);
    int32_t result = ae_game_call(g, address, a0, a1, a2, a3);
    ae_wr32(g->ram, stack_argument, saved);
    return result;
}

int32_t ae_game_call6(AeGame *g, uint32_t address, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5) {
    uint32_t sp = g->call_stack_pointer;
    uint32_t saved4 = ae_rd32(g->ram, sp + 16u), saved5 = ae_rd32(g->ram, sp + 20u);
    ae_wr32(g->ram, sp + 16u, a4);
    ae_wr32(g->ram, sp + 20u, a5);
    int32_t result = ae_game_call(g, address, a0, a1, a2, a3);
    ae_wr32(g->ram, sp + 16u, saved4);
    ae_wr32(g->ram, sp + 20u, saved5);
    return result;
}

/* Seven-word callbacks carry the third stack argument at sp + 0x18. */
int32_t ae_game_call7(AeGame *g, uint32_t address, uint32_t a0, uint32_t a1,
                     uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6) {
    uint32_t stack_argument = g->call_stack_pointer + 24u;
    uint32_t saved = ae_rd32(g->ram, stack_argument);
    ae_wr32(g->ram, stack_argument, a6);
    int32_t result = ae_game_call6(g, address, a0, a1, a2, a3, a4, a5);
    ae_wr32(g->ram, stack_argument, saved);
    return result;
}

/* CollisionRayTest copies probe callbacks and preserves h->user. All game
 * callbacks therefore receive AeGame directly, including copied probe hooks. */
int32_t ae_game_collision_ray(AeGame *g, uint32_t mode, uint32_t params, uint32_t out) {
    return ae_CollisionRayTest(g->ram, &g->misc, mode, params, out);
}
static int32_t call(void *user, uint32_t address, uint32_t a0, uint32_t a1, uint32_t a2) {
    return ae_game_call(user, address, a0, a1, a2, 0);
}
static void printf_error(void *user, AeRam *r, uint32_t format, int32_t type) {
    AeGame *g = user;
    if (g->host.platform.Printf)
        g->host.platform.Printf(g->host.user, r, format, (uint32_t)type, 0, 0);
}
static void build_probe_points(void *user, AeRam *r, int32_t type, uint32_t body) {
    (void)r; call(user, 0x8007304Cu, (uint32_t)type, body, 0);
}
static void body_sfx_at(void *user, AeRam *r, int32_t id, const int16_t position[3]) {
    for (uint32_t axis = 0; axis < 3; ++axis)
        ae_wr16(r, ARG_VECTOR + axis * 2u, (uint16_t)position[axis]);
    call(user, 0x80087A20u, (uint32_t)id, ARG_VECTOR, 0);
}
static int32_t sound_start(void *user, AeRam *r, int32_t mode, int32_t id) {
    (void)r; return call(user, 0x800878E0u, (uint32_t)mode, (uint32_t)id, 0);
}
static void sound_stop(void *user, AeRam *r, int32_t id, int32_t handle) {
    AeGame *g = user;
    (void)ae_SoundLoopStop(r, &g->host.sound_hw, id, handle);
}
static int32_t ray(void *user, AeRam *r, uint32_t mode, uint32_t params, int16_t out[2]) {
    ae_wr32(r, ARG_VECTOR, 0);
    int32_t result = call(user, 0x80078C9Cu, mode, params, ARG_VECTOR);
    out[0] = (int16_t)ae_rds16(r, ARG_VECTOR);
    out[1] = (int16_t)ae_rds16(r, ARG_VECTOR + 2u);
    return result;
}
static int32_t type_sfx(void *user, AeRam *r, int32_t type) {
    (void)r; return call(user, 0x8005BB98u, (uint32_t)type, 0, 0);
}
static void pickup(void *user, AeRam *r, int32_t type) {
    (void)r; call(user, 0x80078A18u, (uint32_t)type, 0, 0);
}
static void camera_nudge(void *user, AeRam *r, int32_t x, int32_t y, int32_t z) {
    (void)r; call(user, 0x80047690u, (uint32_t)x, (uint32_t)y, (uint32_t)z);
}
static void play_sfx(void *user, AeRam *r, int32_t id) {
    (void)r; call(user, 0x800877F8u, (uint32_t)id, 0, 0);
}
static void voice(void *user, AeRam *r, int32_t id) {
    (void)r; call(user, 0x8006896Cu, (uint32_t)id, 0, 0);
}
static void event(void *user, AeRam *r, uint32_t function) {
    (void)r; call(user, function, 0, 0, 0);
}
void ae_game_wire_body(AeGame *g) {
    g->body.printf_error = printf_error;
    g->body.build_probe_points = build_probe_points;
    g->body.play_sfx_at = body_sfx_at;
    g->body.sound_loop_start = sound_start;
    g->body.sound_loop_stop = sound_stop;
    g->body.collision_ray_test = ray;
    g->body.gimmick_type_to_sfx = type_sfx;
    g->body.pickup_collect = pickup;
    g->body.camera_nudge = camera_nudge;
    g->anim.play_sfx = play_sfx;
    g->anim.play_player_voice = voice;
    g->anim.event_callback = event;
}
