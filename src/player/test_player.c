/*
 * Synthetic tests for the portable player module.  They check the ported routines against the
 * evidence-derived expectations (hand-computed from the decompiled constants) and, when built with
 * -DWITH_CODEX_MATH, against the instruction-validated routines in src/collision/native_collision.c.
 * They are NOT original-game differential tests.  The integrator below is a stand-in for
 * CollisionUpdate (not in the evidence package): pos += vel with a flat floor.
 */
#include "native_player.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WITH_CODEX_MATH
#include "../collision/native_collision.h"
#endif

static int failures, checks;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { long long x_ = (long long)(a), y_ = (long long)(b); ++checks; if (x_ != y_) { ++failures; printf("FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, x_, y_); } } while (0)

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    uint8_t *b = (uint8_t *)malloc((size_t)n); if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f); *size = (size_t)n; return b;
}

/* Synthetic surface table: ONLY for tests.  Real values live at 0x800AEF60 in the EXE. */
static void synthetic_surface(AePlayerData *d) {
    for (int k = 0; k < 8; ++k) {
        uint8_t *r = d->surface[k].raw; memset(r, 0, 16);
        int32_t a = 0x3000, b = 0x6000; int16_t wall = 0x300, jump = 0x300, limit = 0x300;
        memcpy(r + 0, &a, 4); memcpy(r + 4, &b, 4); memcpy(r + 0xA, &wall, 2); memcpy(r + 0xC, &jump, 2); memcpy(r + 0xE, &limit, 2);
    }
    d->surface_loaded = 1;
}

typedef struct { AePlayer p; AePlayerData *d; int32_t floor_y; unsigned unsup; } Sim;

static void sim_init(Sim *s, AePlayerData *d, int32_t x, int32_t y, int32_t z) {
    int32_t pos[3] = { x, y, z }; int16_t face[3] = { 0, 0, 0x1000 };
    memset(s, 0, sizeof(*s)); ae_player_init(&s->p, pos, face); s->d = d; s->floor_y = y;
}
/* one frame: update, then the stand-in integrator (flat floor, no walls) */
static void sim_step(Sim *s, uint32_t held, uint32_t changed, uint16_t mag, int16_t dx, int16_t dz) {
    static uint32_t frame;
    AePlayerInput in; memset(&in, 0, sizeof(in));
    in.held = held; in.changed = changed; in.stick_mag = mag; in.stick_class = ae_player_stick_class(mag);
    in.cam_dir[0] = dx; in.cam_dir[2] = dz; in.frame_counter = frame++;
    ae_player_stick_cam(&s->p, s->d, in.cam_dir, in.stick_cam);
    AePlayerGround g; memset(&g, 0, sizeof(g)); g.ground_y = s->floor_y; g.surface_flags = 0; /* kind 0, no hazard */
    s->unsup |= ae_player_update(&s->p, s->d, &in, &g);
    int32_t pos[3], vel[3];
    for (int i = 0; i < 3; ++i) { pos[i] = s->p.pos[i] + s->p.vel[i]; vel[i] = s->p.vel[i]; }
    if (pos[1] >= s->floor_y) { pos[1] = s->floor_y; vel[1] = 0; }
    ae_player_apply_motion(&s->p, pos, vel, 0);
}

static void test_math(void) {
    AePlayerData d; ae_player_data_init(&d);
    CHECK_EQ(ae_player_rcos(&d, 0), 0x1000); CHECK_EQ(ae_player_rsin(&d, 0x400), 0x1000);
    CHECK_EQ(ae_player_rcos(&d, 0x800), -0x1000); CHECK_EQ(ae_player_rsin(&d, 0xC00), -0x1000);
    CHECK_EQ(ae_player_rcos(&d, 0x400), 0); CHECK_EQ(ae_player_rsin(&d, 0), 0);
    for (int a = -0x2000; a < 0x2000; a += 7) {
        CHECK_EQ(ae_player_rsin(&d, -a), -ae_player_rsin(&d, a));
        CHECK_EQ(ae_player_rcos(&d, -a), ae_player_rcos(&d, a));
        CHECK_EQ(ae_player_rcos(&d, a), ae_player_rsin(&d, a + 0x400));
    }
    CHECK_EQ(ae_player_int_sqrt(0), 0); CHECK_EQ(ae_player_int_sqrt(1), 1); CHECK_EQ(ae_player_int_sqrt(2), 1);
    CHECK_EQ(ae_player_int_sqrt(3), 2); CHECK_EQ(ae_player_int_sqrt(16), 4); CHECK_EQ(ae_player_int_sqrt(0x7FFFFFFF), 46341);
    CHECK_EQ(ae_player_stick_class(0), 0); CHECK_EQ(ae_player_stick_class(1), 2); CHECK_EQ(ae_player_stick_class(0x46), 2);
    CHECK_EQ(ae_player_stick_class(0x47), 4); CHECK_EQ(ae_player_stick_class(0x7C), 4); CHECK_EQ(ae_player_stick_class(0x7D), 8);
    int16_t in3[3] = { 0, 0, 0x1000 }, out3[3];
    ae_player_normalize(&d, in3, out3); CHECK_EQ(out3[2], 0x1000);
    int16_t in4[3] = { 3, 0, 4 }; ae_player_normalize(&d, in4, out3);
    CHECK(abs(out3[0] - 0x999) <= 2 && abs(out3[2] - 0xCCC) <= 2);
#ifdef WITH_CODEX_MATH
    /* cross-check with the routines Codex validated against the original instructions */
    unsigned seed = 12345; int bad_sqrt = 0, bad_norm = 0;
    for (int i = 0; i < 300000; ++i) {
        seed = seed * 1664525u + 1013904223u;
        int32_t n = (int32_t)(seed >> 1);
        if (i % 3 == 0) n >>= 12;
        if (ae_player_int_sqrt(n) != ae_collision_sqrt(n, 0)) ++bad_sqrt;
        seed = seed * 1664525u + 1013904223u; int32_t x = (int16_t)(seed >> 16), z = (int16_t)(seed & 0xFFFF);
        if (i % 2) { x >>= 6; z >>= 6; }
        int16_t v[3] = { (int16_t)x, 0, (int16_t)z }, o[3]; int32_t ref[2];
        ae_player_normalize(&d, v, o); ae_collision_normalize(x, z, ref);
        if (o[0] != ref[0] || o[2] != ref[1]) ++bad_norm;
    }
    CHECK_EQ(bad_sqrt, 0); CHECK_EQ(bad_norm, 0);
#endif
    ae_player_data_free(&d);
}

static void test_loader(const char *member_path) {
    size_t size; uint8_t *blob = read_file(member_path, &size); CHECK(blob != NULL); if (!blob) return;
    AePlayerData d; ae_player_data_init(&d);
    CHECK(ae_player_data_load_member(&d, blob, size));
    CHECK_EQ(d.set0.state_count, 78); CHECK_EQ(d.set0.transition_count, 192);
    CHECK_EQ(d.set0.states[0].first_frame, 0); CHECK_EQ(d.set0.states[0].last_frame, 28); CHECK_EQ(d.set0.states[0].flags, 0x13);
    CHECK_EQ(d.set0.states[5].move_mode, 4); CHECK_EQ(d.set0.states[2].turn_tilt & 0xF, 2);
    CHECK_EQ(d.set0.states[77].first_frame, 511); CHECK_EQ(d.set0.states[77].last_frame, 492);   /* reversed range preserved */
    CHECK_EQ(d.set0.transitions[0].state, 0); CHECK_EQ(d.set0.transitions[0].new_state, 0x24); CHECK_EQ(d.set0.transitions[0].cond, 24);
    CHECK_EQ(d.set0.transitions[d.set0.transition_count].state, 0xFF);
    uint8_t truncated[0x30]; memset(truncated, 0, sizeof(truncated));
    CHECK(!ae_player_data_load_member(&d, truncated, sizeof(truncated)));
    ae_player_data_free(&d); free(blob);
}

static void test_exe_loader(void) {
    size_t size = 0x800 + 0xA0000; uint8_t *exe = (uint8_t *)calloc(1, size);
    AePlayerData d; ae_player_data_init(&d);
    CHECK(!ae_player_data_load_exe(&d, exe, size));                      /* zero image: sine endpoints invalid */
    for (int i = 0; i <= 0x400; ++i) { exe[0x800 + 0x800A9DB0u - 0x80010000u + i * 2] = (uint8_t)((i * 4) & 0xFF); exe[0x800 + 0x800A9DB0u - 0x80010000u + i * 2 + 1] = (uint8_t)((i * 4) >> 8); }
    exe[0x800 + 0x800A9DB0u - 0x80010000u + 0x800] = 0x00; exe[0x800 + 0x800A9DB0u - 0x80010000u + 0x801] = 0x10;
    exe[0x800 + 0x800AEF60u - 0x80010000u + 4] = 0x34; exe[0x800 + 0x800AEF60u - 0x80010000u + 5] = 0x12;
    CHECK(ae_player_data_load_exe(&d, exe, size)); CHECK(d.trig_exact && d.surface_loaded);
    CHECK_EQ(d.surface[0].raw[4], 0x34); CHECK_EQ(d.sine_quarter[0x400], 0x1000);
    ae_player_data_free(&d); free(exe);
}

static void test_idle_walk_run(AePlayerData *d) {
    Sim s; sim_init(&s, d, 0, 0, 0);
    for (int i = 0; i < 70; ++i) sim_step(&s, 0, 0, 0, 0, 0);       /* idle loops through state 0 */
    CHECK_EQ(s.p.state, 0); CHECK_EQ(s.p.pos[0], 0); CHECK_EQ(s.p.pos[2], 0);
    CHECK_EQ(s.unsup, 0);
    /* tilt classes (PadUpdate): mag < 0x47 -> class 2 -> creep (state 2); < 0x7D -> class 4 -> walk (state 3) */
    sim_step(&s, 0, 0, 0x30, 0, 0x1000);
    CHECK_EQ(s.p.state, 2);
    sim_step(&s, 0, 0, 0x30, 0, 0x1000);
    CHECK_EQ(s.p.state, 2);
    CHECK(s.p.pos[2] > 0 && s.p.pos[0] == 0);
    sim_step(&s, 0, 0, 0x60, 0, 0x1000);
    CHECK_EQ(s.p.state, 3);
    sim_step(&s, 0, 0, 0x60, 0, 0x1000);
    CHECK_EQ(s.p.state, 3);
    CHECK_EQ(s.p.move_vel[2] > 0, 1);
    /* full tilt -> class 8 -> run (state 5): ((0x1000 * 0x7F) >> 7) * 0x3F2 >> 5 per frame, limited by the surface accel */
    for (int i = 0; i < 40; ++i) sim_step(&s, 0, 0, 0x7F, 0, 0x1000);
    CHECK_EQ(s.p.state, 5);
    CHECK_EQ(s.p.move_vel[2], ((0x1000 * 0x7F >> 7) * 0x3F2) >> 5);
    int32_t before = s.p.pos[2];
    sim_step(&s, 0, 0, 0x7F, 0, 0x1000);
    CHECK_EQ(s.p.pos[2] - before, (((0x1000 * 0x7F >> 7) * 0x3F2) >> 5) / 0x1000);
    /* release: back to stand (state 0) and decelerate to rest */
    for (int i = 0; i < 80; ++i) sim_step(&s, 0, 0, 0, 0, 0);
    CHECK_EQ(s.p.state, 0); CHECK_EQ(s.p.vel[2], 0);
    CHECK_EQ(s.unsup & ~AE_UNSUP_TILT, 0);
}

static void test_turn(AePlayerData *d) {
    Sim s; sim_init(&s, d, 0, 0, 0);
    sim_step(&s, 0, 0, 0x7F, 0x1000, 0);                                       /* state 0 has no turn mode: facing unchanged */
    CHECK_EQ(s.p.state, 5); CHECK_EQ(s.p.facing[0], 0);
    sim_step(&s, 0, 0, 0x7F, 0x1000, 0);                                       /* state 5 turns by at most 0x155 (30 deg) */
    CHECK(s.p.facing[0] > 0 && s.p.facing[2] > 0);                           /* partly turned, not snapped */
    for (int i = 0; i < 60; ++i) sim_step(&s, 0, 0, 0x7F, 0x1000, 0);
    CHECK_EQ(s.p.facing[0], 0x1000); CHECK_EQ(s.p.facing[2], 0);
    /* turn step is bounded by rcos(0x155) */
    Sim t; sim_init(&t, d, 0, 0, 0);
    sim_step(&t, 0, 0, 0x7F, -0x1000, 0); sim_step(&t, 0, 0, 0x7F, -0x1000, 0);
    int32_t step_dot = (int32_t)t.p.facing[2];                               /* new facing . old facing (0,0,0x1000) */
    CHECK(step_dot >= ae_player_rcos(d, 0x155) - 4);
}

static void test_jump_fall_land(AePlayerData *d) {
    Sim s; sim_init(&s, d, 0, 0, 0);
    for (int i = 0; i < 5; ++i) sim_step(&s, 0, 0, 0, 0, 0);
    sim_step(&s, 0x0A, 0x0A, 0, 0, 0);                                        /* jump pressed */
    CHECK_EQ(s.p.state, 8);
    int32_t jump_move_y = (d->params.gravity + d->params.jump_impulse) * 0x80;
    CHECK(s.p.vel[1] < 0);
    CHECK_EQ(s.p.move_vel[1] > jump_move_y - 0x1000 && s.p.move_vel[1] <= jump_move_y + 0x1000, 1);
    int32_t min_y = 0, apex_frame = 0; int visited_air = 0, landed = 0, saw_fx = 0;
    for (int i = 0; i < 200; ++i) {
        sim_step(&s, 0x0A, 0, 0, 0, 0);                                       /* jump held */
        if (s.p.pos[1] < min_y) { min_y = s.p.pos[1]; apex_frame = i; }
        if (s.p.state == 9 || s.p.state == 0xA) visited_air = 1;
        for (int e = 0; e < s.p.event_count; ++e) if (s.p.events[e].kind == AE_EV_SFX && (s.p.events[e].a == 0x12 || s.p.events[e].a == 0x13)) saw_fx = 1;
        if (s.p.state == 0 && s.p.pos[1] == 0 && i > 5) { landed = 1; break; }
    }
    CHECK(visited_air); CHECK(landed); CHECK(saw_fx);
    CHECK(min_y < -0x10 && apex_frame > 5);                                   /* it left the ground and came back */
    CHECK_EQ(s.p.pos[1], 0);
    /* a short hop (jump released at once) peaks lower than the held jump */
    Sim h; sim_init(&h, d, 0, 0, 0); int32_t min_hop = 0;
    for (int i = 0; i < 3; ++i) sim_step(&h, 0, 0, 0, 0, 0);
    sim_step(&h, 0x0A, 0x0A, 0, 0, 0);
    for (int i = 0; i < 120; ++i) { sim_step(&h, 0, 0, 0, 0, 0); if (h.p.pos[1] < min_hop) min_hop = h.p.pos[1]; }
    CHECK(min_hop > min_y);
}

static void test_fall_off_ledge(AePlayerData *d) {
    Sim s; sim_init(&s, d, 0, -0x400, 0); s.floor_y = 0;                       /* 0x400 above the floor */
    s.p.vel[1] = 0x20; s.p.ground_y = 0;
    for (int i = 0; i < 3; ++i) sim_step(&s, 0, 0, 0, 0, 0);
    CHECK_EQ(s.p.state, 0x24);                                                 /* PlayerCond_FallingHigh */
    int landed = 0;
    for (int i = 0; i < 200 && !landed; ++i) { sim_step(&s, 0, 0, 0, 0, 0); landed = s.p.pos[1] == 0 && s.p.state != 0x24; }
    CHECK(landed);
    CHECK(s.p.state == 0x25 || s.p.state == 0x4B || s.p.state == 0x0 || s.p.state == 0x18);
    /* a long fall (>= 6 frames at terminal speed) hurts and enters the launch-recovery state 0x4B */
    Sim h; sim_init(&h, d, 0, -0x7FFF, 0); h.floor_y = 0; h.p.vel[1] = 0x60;
    int hurt = 0;
    for (int i = 0; i < 600; ++i) { sim_step(&h, 0, 0, 0, 0, 0); for (int e = 0; e < h.p.event_count; ++e) if (h.p.events[e].kind == AE_EV_DAMAGE) hurt = 1; if (hurt) break; }
    CHECK(hurt); CHECK_EQ(h.p.health, 4); CHECK_EQ(h.p.state, 0x4B);
    d->jump_loaded = 0; h.unsup = 0; sim_step(&h, 0, 0, 0, 0, 0);
    CHECK(h.unsup & AE_UNSUP_WATER);                                           /* 0x4B dispatches through the EXE jump table: fails closed without it */
    d->jump_loaded = 1; d->jump_table[0x4B - 0x46] = 0x8006163Cu;            /* synthetic: normal-gravity handler */
    h.unsup = 0; for (int i = 0; i < 100 && h.p.state == 0x4B; ++i) sim_step(&h, 0, 0, 0, 0, 0);
    CHECK_EQ(h.p.state, 0); CHECK_EQ(h.unsup & AE_UNSUP_WATER, 0);          /* 0x4B -> AnimDone -> stand */
    d->jump_loaded = 1;
}

static void test_unsupported_reporting(AePlayerData *d) {
    Sim s; sim_init(&s, d, 0, 0, 0);
    s.p.state = 0x80;
    uint32_t u = 0; AePlayerInput in; memset(&in, 0, sizeof(in)); AePlayerGround g; memset(&g, 0, sizeof(g));
    u = ae_player_update(&s.p, d, &in, &g); CHECK(u & AE_UNSUP_GADGET_STATE);
    s.p.state = 0x46; u = ae_player_update(&s.p, d, &in, &g); CHECK(u & (AE_UNSUP_MOVE_MODE | AE_UNSUP_TURN_MODE));
    s.p.state = 0; g.ground_type = 0x1000; u = ae_player_update(&s.p, d, &in, &g); CHECK(u & AE_UNSUP_WATER);
}

static void test_timed_launch(AePlayerData *d) {
    Sim s; sim_init(&s, d, 0, 0, 0);
    d->jump_loaded = 1; d->jump_table[0x4D - 0x46] = 0x8006163Cu;           /* synthetic handler assignment */
    for (int i = 0; i < 3; ++i) sim_step(&s, 0, 0, 0, 0, 0);
    ae_player_set_state(&s.p, d, 0x4D); s.p.contact_cooldown = 0xF;
    int frames = 0;
    while (s.p.state == 0x4D && frames < 40) { sim_step(&s, 0, 0, 0, 0, 0); ++frames; }
    CHECK_EQ(frames, 15); CHECK_EQ(s.p.state, 0x2B);                         /* OnContact counts the cooldown down, then launches */
    d->jump_loaded = 1;
}

static void test_events_and_blend(AePlayerData *d) {
    Sim s; sim_init(&s, d, 0, 0, 0);
    for (int i = 0; i < 3; ++i) sim_step(&s, 0, 0, 0, 0, 0);
    sim_step(&s, 0x0A, 0x0A, 0, 0, 0);
    /* entering state 8 plays the jump SFX 0x11 on relative frame 0 (events table, kind 1) */
    int found = 0;
    for (int i = 0; i < 4; ++i) { for (int e = 0; e < s.p.event_count; ++e) if (s.p.events[e].kind == AE_EV_SFX && s.p.events[e].a == 0x11) found = 1; sim_step(&s, 0x0A, 0, 0, 0, 0); }
    CHECK(found);
    /* SetState(0) from a state that did not finish its animation starts a 5-frame blend */
    Sim b; sim_init(&b, d, 0, 0, 0);
    ae_player_set_state(&b.p, d, 5); b.p.anim_frame = 108; ae_player_set_state(&b.p, d, 0);
    CHECK_EQ(b.p.anim_blend, 5); CHECK_EQ(b.p.anim_blend_from, 108);
}

/* PlayerAnimateAndDraw scalar side effect: next_frame = anim_frame + 1 for state-flag class 0x20 or blend 0xFFFF. */
static void test_draw_next_frame(AePlayerData *d) {
    AePlayerInput in; memset(&in, 0, sizeof(in)); AePlayerGround g; memset(&g, 0, sizeof(g));
    Sim s; sim_init(&s, d, 0, 0, 0);
    /* class 0x20 (state 2, flags 0x23), no blend: written */
    ae_player_set_state(&s.p, d, 2); s.p.anim_frame = 62; s.p.anim_blend = 0; s.p.next_frame = 0x1234;
    sim_step(&s, 0, 0, 0x30, 0, 0x1000);                                    /* creep input keeps state 2 */
    CHECK_EQ(s.p.state, 2); CHECK_EQ(s.p.state_flags & 0xF0, 0x20); CHECK_EQ(s.p.next_frame, s.p.anim_frame + 1);
    /* blend == 0xFFFF in a non-0x20 state (state 0, flags 0x13): written, blend then cleared by the decrement stage */
    Sim b; sim_init(&b, d, 0, 0, 0);
    b.p.anim_frame = 10; b.p.anim_blend = 0xFFFF; b.p.next_frame = 0x1234;
    ae_player_update(&b.p, d, &in, &g);
    CHECK_EQ(b.p.state, 0); CHECK_EQ(b.p.state_flags & 0xF0, 0x10); CHECK_EQ(b.p.anim_blend, 0);
    CHECK_EQ(b.p.next_frame, b.p.anim_frame + 1);
    /* neither condition (blend 0, 0xFFFE or counting): next_frame untouched */
    uint16_t blends[3] = { 0, 0xFFFE, 3 };
    for (int i = 0; i < 3; ++i) {
        Sim n; sim_init(&n, d, 0, 0, 0);
        n.p.anim_frame = 10; n.p.anim_blend = blends[i]; n.p.next_frame = 0x1234;
        ae_player_update(&n.p, d, &in, &g);
        CHECK_EQ(n.p.state, 0); CHECK_EQ(n.p.next_frame, 0x1234);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s member_fixture.bin\n", argv[0]); return 2; }
    test_math(); test_loader(argv[1]); test_exe_loader();
    size_t size; uint8_t *blob = read_file(argv[1], &size);
    if (!blob) { fprintf(stderr, "cannot read fixture\n"); return 2; }
    AePlayerData d; ae_player_data_init(&d);
    if (!ae_player_data_load_member(&d, blob, size)) { fprintf(stderr, "load: %s\n", ae_player_error()); return 2; }
    synthetic_surface(&d);
    test_idle_walk_run(&d); test_turn(&d); test_jump_fall_land(&d); test_fall_off_ledge(&d);
    test_unsupported_reporting(&d); test_timed_launch(&d); test_events_and_blend(&d); test_draw_next_frame(&d);
    ae_player_data_free(&d); free(blob);
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
