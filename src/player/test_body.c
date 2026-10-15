/*
 * Synthetic tests for native_player_body.c (the player pass of CollisionUpdate).  Expected values are
 * hand-computed from the instruction listings cited in native_player_body.c.  They are NOT
 * original-game differential tests: the worlds below are made-up callbacks.
 */
#include "native_player_body.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures, checks;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { long long x_ = (long long)(a), y_ = (long long)(b); ++checks; if (x_ != y_) { ++failures; printf("FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, x_, y_); } } while (0)

/* ---- synthetic world: terrain-like floor (height ignores the query y), optional step and wall ---- */
typedef struct {
    int32_t floor_y;                /* height for x < step_x */
    int32_t step_x, step_y;         /* height for x >= step_x (step_x = INT32_MAX: none) */
    int32_t wall_x;                 /* a wall plane: body x + radius may not pass it (INT32_MAX: none) */
    uint32_t wall_contact;
    int32_t ceiling;                /* top-probe ceiling term reported every frame */
    int16_t solid_kind; uint16_t solid_shape;
    int32_t collider;               /* collider_at result */
    int32_t ray;                    /* ray_test result */
    int wall_fail;
    int ground_calls, wall_calls;
    int32_t last_wall_pos[3], last_wall_vel[3];
} World;

static int32_t w_ground(void *u, int32_t x, int32_t y, int32_t z, AePlayerBodyGroundHit *hit) {
    World *w = (World *)u; (void)y; (void)z; w->ground_calls++;
    hit->solid_kind = w->solid_kind; hit->solid_shape = w->solid_shape;
    return x >= w->step_x ? w->step_y : w->floor_y;
}
static int w_walls(void *u, const int32_t pos[3], const int32_t vel[3], int32_t radius, int32_t height, AePlayerBodyWallHit *out) {
    World *w = (World *)u; (void)height; w->wall_calls++;
    for (int i = 0; i < 3; ++i) { w->last_wall_pos[i] = pos[i]; w->last_wall_vel[i] = vel[i]; }
    if (w->wall_fail) return 0;
    int32_t nx = pos[0] + vel[0];
    if (w->wall_x != INT32_MAX && nx + radius > w->wall_x) { out->push_x = w->wall_x - (nx + radius); out->contact = w->wall_contact; }
    if (w->ceiling) { out->vel_y_add = w->ceiling; out->ceiling = w->ceiling; }
    return 1;
}
static int32_t w_collider(void *u, const int32_t pos[3]) { (void)pos; return ((World *)u)->collider; }
static int32_t w_ray(void *u, uint32_t mode, const int32_t probe[5]) { (void)mode; (void)probe; return ((World *)u)->ray; }

static void world_init(World *w, int32_t floor_y) {
    memset(w, 0, sizeof(*w)); w->floor_y = floor_y; w->step_x = INT32_MAX; w->wall_x = INT32_MAX;
    w->solid_kind = -1; w->collider = -1; w->ray = 0; w->wall_contact = 0xA0000012u;
}
static void bw_init(AePlayerBodyWorld *bw, World *w, AePlayerBodyState *s, const AePlayerData *d) {
    memset(bw, 0, sizeof(*bw));
    bw->user = w; bw->ground = w_ground; bw->walls = w_walls; bw->ray_test = w_ray; bw->collider_at = w_collider;
    bw->data = d; bw->state = s; bw->grid_width = 64; bw->grid_depth = 64; bw->current_level = 1;
}
static void player_at(AePlayer *p, int32_t x, int32_t y, int32_t z) {
    int32_t pos[3] = { x, y, z }; int16_t face[3] = { 0, 0, 0x1000 }; ae_player_init(p, pos, face);
}

static void test_flat_floor(const AePlayerData *d) {
    World w; world_init(&w, 0); AePlayerBodyState s; ae_player_body_state_init(&s); AePlayerBodyWorld bw; bw_init(&bw, &w, &s, d);
    AePlayer p; player_at(&p, 0, 0, 0);
    p.vel[0] = 10; p.vel[1] = 5; p.vel[2] = -3; p.contact = 0x12345;
    CHECK_EQ(ae_player_body_step(&p, &bw), 0);
    CHECK_EQ(p.pos[0], 10); CHECK_EQ(p.pos[2], -3);
    CHECK_EQ(p.pos[1], 0); CHECK_EQ(p.vel[1], 0);            /* floor 0 < 0 + 5: vel.y = floor - y, y = floor */
    CHECK_EQ(p.vel[0], 10); CHECK_EQ(p.vel[2], -3);
    CHECK_EQ(p.contact, 0);                                   /* cleared at the start of CollisionUpdate */
    CHECK_EQ(s.collider_index, 0xFFFF);
    CHECK_EQ(s.land_ground_y, 0);
    /* airborne above the floor: y += vel.y (+ ceiling), no snap */
    player_at(&p, 0, -0x200, 0); p.vel[1] = 0x10;
    ae_player_body_step(&p, &bw);
    CHECK_EQ(p.pos[1], -0x1F0); CHECK_EQ(p.vel[1], 0x10);
    /* landing sample is at predicted y minus half the body height: floor between y+vel-h/2 and y+vel still clamps */
    player_at(&p, 0, -0x30, 0); p.vel[1] = 0x10;              /* y + vel = -0x20 > floor 0? no: floor 0 >= -0x20 -> no clamp */
    ae_player_body_step(&p, &bw);
    CHECK_EQ(p.pos[1], -0x20);
    /* state 0x2D bypass: pos += vel, contact kept, no world queries */
    player_at(&p, 0, -0x400, 0); p.state = 0x2D; p.vel[0] = 3; p.vel[1] = 0x500; p.contact = 0xA0000001u;
    w.ground_calls = 0; ae_player_body_step(&p, &bw);
    CHECK_EQ(p.pos[1], 0x100); CHECK_EQ(p.pos[0], 3); CHECK_EQ(p.contact, 0xA0000001u); CHECK_EQ(w.ground_calls, 0);
    /* missing ground callback fails closed without touching the player */
    AePlayerBodyWorld nb = bw; nb.ground = NULL; player_at(&p, 7, 0, 0); p.vel[0] = 9;
    CHECK_EQ(ae_player_body_step(&p, &nb), AE_PLAYER_BODY_NO_GROUND); CHECK_EQ(p.pos[0], 7);
}

static void test_probe_ground(const AePlayerData *d) {
    World w; world_init(&w, 0x30); AePlayerBodyState s; ae_player_body_state_init(&s); AePlayerBodyWorld bw; bw_init(&bw, &w, &s, d);
    AePlayer p; player_at(&p, 0, 0, 0); p.vel[1] = 0x50;      /* fast downward motion, floor 0x30 below */
    ae_player_body_step(&p, &bw);
    CHECK_EQ(s.probe_vel_y, 0x40);                            /* i = 0x20: 0x30 < 0x20 no; i = 0x40: 0x30 < 0x40 yes */
    CHECK_EQ(p.pos[1], 0x30); CHECK_EQ(p.vel[1], 0x30);
    player_at(&p, 0, 0, 0); p.vel[1] = 0x1F;                  /* below 0x20: no probing */
    w.floor_y = 0x400; ae_player_body_step(&p, &bw);
    CHECK_EQ(s.probe_vel_y, 0x1F); CHECK_EQ(p.pos[1], 0x1F);
}

static void test_steps(const AePlayerData *d) {
    World w; world_init(&w, 0); AePlayerBodyState s; ae_player_body_state_init(&s); AePlayerBodyWorld bw; bw_init(&bw, &w, &s, d);
    AePlayer p;
    /* step up 0x80 (inside 0xFF): snapped up, vel.y = -0x80 */
    w.step_x = 100; w.step_y = -0x80;
    player_at(&p, 90, 0, 0); p.vel[0] = 20;
    CHECK_EQ(ae_player_body_step(&p, &bw), 0);
    CHECK_EQ(p.pos[0], 110); CHECK_EQ(p.pos[1], -0x80); CHECK_EQ(p.vel[1], -0x80); CHECK_EQ(s.restored, 0);
    /* step up exactly 0xFF: still accepted (slti -0xff is strict) */
    w.step_y = -0xFF; player_at(&p, 90, 0, 0); p.vel[0] = 20; ae_player_body_step(&p, &bw);
    CHECK_EQ(p.pos[1], -0xFF); CHECK_EQ(s.restored, 0);
    /* step down 0x80: no snap down, the body keeps y + vel.y and falls under PlayerUpdate gravity */
    w.step_y = 0x80; player_at(&p, 90, 0, 0); p.vel[0] = 20; ae_player_body_step(&p, &bw);
    CHECK_EQ(p.pos[0], 110); CHECK_EQ(p.pos[1], 0); CHECK_EQ(p.vel[1], 0);
    /* a drop of 0x400 never restores (only upward moves are checked) */
    w.step_y = 0x400; player_at(&p, 90, 0, 0); p.vel[0] = 20; p.vel[1] = 0x10; ae_player_body_step(&p, &bw);
    CHECK_EQ(p.pos[1], 0x10); CHECK_EQ(s.restored, 0);
}

static void test_restore(const AePlayerData *d) {
    World w; world_init(&w, 0); AePlayerBodyState s; ae_player_body_state_init(&s); AePlayerBodyWorld bw; bw_init(&bw, &w, &s, d);
    AePlayer p;
    w.step_x = 100; w.step_y = -0x100;                        /* 0x100 above: > 0xFF upward step */
    player_at(&p, 90, 0, 5); p.vel[0] = 20; p.vel[1] = 7; p.vel[2] = 4;
    CHECK_EQ(ae_player_body_step(&p, &bw), 0);
    CHECK_EQ(s.restored, 1);
    CHECK_EQ(p.pos[0], 90); CHECK_EQ(p.pos[1], 0); CHECK_EQ(p.pos[2], 5);   /* snapshot position */
    CHECK_EQ(p.vel[0], 0); CHECK_EQ(p.vel[2], 0); CHECK_EQ(p.vel[1], 7);    /* x/z zeroed, y from the snapshot */
    CHECK_EQ(p.health, 5); CHECK_EQ(p.event_count, 0);                      /* no contact this frame: no damage */
    /* with a wall contact the restore damages (PlayerDamage(5)) */
    w.wall_x = 170; player_at(&p, 90, 0, 0); p.vel[0] = 20;   /* 110 + 0x40 > 170: pushed to 106, still on the step */
    ae_player_body_step(&p, &bw);
    CHECK_EQ(s.restored, 1); CHECK_EQ(p.health, 0); CHECK_EQ(p.invuln_timer, 0x3C); CHECK_EQ(p.hurt, 1);
    CHECK(p.event_count == 1 && p.events[0].kind == AE_EV_DAMAGE && p.events[0].a == 5);
    CHECK_EQ(p.contact, 0xA0000012u);
    /* exclusions: half-pipe shape 0x24 under a kind-1 solid, area 0x44, state 0x23 */
    w.solid_kind = 1; w.solid_shape = 0x1024; player_at(&p, 90, 0, 0); p.vel[0] = 20; ae_player_body_step(&p, &bw);
    CHECK_EQ(p.health, 5);
    w.solid_kind = 0; player_at(&p, 90, 0, 0); p.vel[0] = 20; ae_player_body_step(&p, &bw);
    CHECK_EQ(p.health, 0);                                    /* shape ignored when solid_kind != 1 */
    bw.current_area = 0x44; player_at(&p, 90, 0, 0); p.vel[0] = 20; ae_player_body_step(&p, &bw);
    CHECK_EQ(p.health, 5); bw.current_area = 0;
    player_at(&p, 90, 0, 0); p.state = 0x23; p.vel[0] = 20; ae_player_body_step(&p, &bw);
    CHECK_EQ(p.health, 5);
    /* level 0x20: death state 0x22 + XA instead of damage; fails closed without data */
    bw.current_level = 0x20; bw.frame_counter = 3; player_at(&p, 90, 0, 0); p.vel[0] = 20;
    ae_player_body_step(&p, &bw);
    CHECK_EQ(p.state, 0x22); CHECK_EQ(p.health, 5);
    CHECK(p.event_count >= 1 && p.events[p.event_count - 1].kind == AE_EV_XA && p.events[p.event_count - 1].a == 0xD);
    AePlayerBodyWorld nd = bw; nd.data = NULL; player_at(&p, 90, 0, 0); p.vel[0] = 20;
    CHECK(ae_player_body_step(&p, &nd) & AE_PLAYER_BODY_DEATH_NO_DATA); CHECK_EQ(p.state, 0);
}

static void test_walls_and_ceiling(const AePlayerData *d) {
    World w; world_init(&w, 0); AePlayerBodyState s; ae_player_body_state_init(&s); AePlayerBodyWorld bw; bw_init(&bw, &w, &s, d);
    AePlayer p; player_at(&p, 0, 0, 0);
    w.wall_x = 100; p.vel[0] = 50; p.vel[2] = 7;
    ae_player_body_step(&p, &bw);
    CHECK_EQ(w.last_wall_pos[0], 0); CHECK_EQ(w.last_wall_vel[0], 50);   /* callback gets the pre-motion position */
    CHECK_EQ(p.pos[0], 100 - 0x40);                           /* 50 + push (100 - (50 + 0x40)) */
    CHECK_EQ(p.vel[0], 50 + (100 - (50 + 0x40)));             /* push is added to vel.x */
    CHECK_EQ(p.pos[2], 7); CHECK_EQ(p.contact, 0xA0000012u);
    /* ceiling: vel.y gets the term, and BodyLandOnGround adds the returned max again */
    world_init(&w, 0); w.ceiling = 0x10; player_at(&p, 0, -0x300, 0); p.vel[1] = -0x20;
    ae_player_body_step(&p, &bw);
    CHECK_EQ(s.ceiling, 0x10); CHECK_EQ(p.vel[1], -0x10); CHECK_EQ(p.pos[1], -0x300 - 0x10 + 0x10);
    CHECK_EQ(p.contact, 0);                                   /* a ceiling-only hit sets no contact */
    /* failing wall callback is reported and treated as empty */
    world_init(&w, 0); w.wall_fail = 1; player_at(&p, 0, 0, 0); p.vel[0] = 5;
    CHECK(ae_player_body_step(&p, &bw) & AE_PLAYER_BODY_WALL_FAILED); CHECK_EQ(p.pos[0], 5);
    /* NULL walls = empty world */
    AePlayerBodyWorld nw = bw; nw.walls = NULL; world_init(&w, 0); player_at(&p, 0, 0, 0); p.vel[0] = 5;
    CHECK_EQ(ae_player_body_step(&p, &nw), 0); CHECK_EQ(p.pos[0], 5);
}

static void test_grid_modes_colliders(const AePlayerData *d) {
    World w; world_init(&w, 0); AePlayerBodyState s; ae_player_body_state_init(&s); AePlayerBodyWorld bw; bw_init(&bw, &w, &s, d);
    AePlayer p; player_at(&p, 0x3E00, 0, -0x3E00); p.vel[0] = 0x300; p.vel[2] = -0x300;
    bw.grid_width = 64; bw.grid_depth = 32;
    ae_player_body_step(&p, &bw);
    CHECK_EQ(p.pos[0], 63 * 0x100); CHECK_EQ(p.pos[2], -31 * 0x100);
    bw.grid_width = 0; player_at(&p, 0, 0, 0);
    CHECK(ae_player_body_step(&p, &bw) & AE_PLAYER_BODY_NO_GRID); bw.grid_width = 64;
    /* collider index: written when the scan finds one, needs a callback when colliders exist */
    bw.collider_count = 3; w.collider = 2; player_at(&p, 0, 0, 0);
    CHECK_EQ(ae_player_body_step(&p, &bw), 0); CHECK_EQ(s.collider_index, 2);
    AePlayerBodyWorld nc = bw; nc.collider_at = NULL;
    CHECK(ae_player_body_step(&p, &nc) & AE_PLAYER_BODY_COLLIDER_UNKNOWN); CHECK_EQ(s.collider_index, 0xFFFF);
    /* mode 5: previous index latched, vel x/z zeroed; mode 6: latched, no collider scan */
    w.collider = 1; ae_player_body_step(&p, &bw); CHECK_EQ(s.collider_index, 1);
    bw.player_mode = 5; p.vel[0] = 9; p.vel[2] = 9; ae_player_body_step(&p, &bw);
    CHECK_EQ(s.mode5_collider, 1); CHECK_EQ(s.mode6_collider, 0xFFFF); CHECK_EQ(p.pos[0], 0); CHECK_EQ(p.vel[0], 0);
    bw.player_mode = 6; w.collider = 0; ae_player_body_step(&p, &bw);
    CHECK_EQ(s.mode6_collider, 1); CHECK_EQ(s.collider_index, 0xFFFF);
    /* mode 2: no motion at all */
    bw.player_mode = 2; player_at(&p, 0, -0x100, 0); p.vel[0] = 9; p.vel[1] = 9; p.contact = 5;
    ae_player_body_step(&p, &bw); CHECK_EQ(p.pos[0], 0); CHECK_EQ(p.pos[1], -0x100); CHECK_EQ(p.contact, 0);
    bw.player_mode = 1; player_at(&p, 0, 0, 0);
    CHECK(ae_player_body_step(&p, &bw) & AE_PLAYER_BODY_MODE_SIDE_FX);
}

static void test_ledge(const AePlayerData *d) {
    World w; world_init(&w, 0x400); AePlayerBodyState s; ae_player_body_state_init(&s); AePlayerBodyWorld bw; bw_init(&bw, &w, &s, d);
    AePlayer p;
    /* falling past a ledge whose top is 0xC0 above the feet, 0x80 ahead (facing +z) */
    w.solid_kind = 0;
    /* the forward probe is z-based: make the step depend on x only, so use facing +x */
    w.step_x = 0x80; w.step_y = -0x20;
    int32_t pos[3] = { 0x40, 0xA0, 0 }; int16_t face[3] = { 0x1000, 0, 0 }; ae_player_init(&p, pos, face);
    p.vel[1] = 0x8; p.health = 5;
    w.ray = -1;
    /* after motion: y = 0xA8; ground here 0x400 (below); ahead x = 0x40 + 0x80: top -0x20; y - top = 0xC8 */
    CHECK_EQ(ae_player_body_step(&p, &bw), 0);
    CHECK_EQ(s.ledge_grabbed, 1); CHECK_EQ(p.state, 0x0E);
    CHECK_EQ(p.pos[0], 0xC0); CHECK_EQ(p.pos[1], -0x20); CHECK_EQ(p.vel[1], 0); CHECK_EQ(p.facing[0], 0x1000);
    /* ray blocked: no grab */
    ae_player_init(&p, pos, face); p.vel[1] = 8; w.ray = 0; ae_player_body_step(&p, &bw);
    CHECK_EQ(s.ledge_grabbed, 0); CHECK_EQ(p.pos[1], 0xA8);
    /* no ray callback: fails closed */
    AePlayerBodyWorld nr = bw; nr.ray_test = NULL; ae_player_init(&p, pos, face); p.vel[1] = 8;
    CHECK(ae_player_body_step(&p, &nr) & AE_PLAYER_BODY_LEDGE_UNKNOWN); CHECK_EQ(p.state, 0);
    /* rising (vel.y <= 0): never tested */
    ae_player_init(&p, pos, face); p.vel[1] = -8; CHECK_EQ(ae_player_body_step(&p, &nr), 0);
}

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    uint8_t *b = (uint8_t *)malloc((size_t)n); if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f); *size = (size_t)n; return b;
}

/* Synthetic surface table, same as test_player.c: ONLY for tests. */
static void synthetic_surface(AePlayerData *d) {
    for (int k = 0; k < 8; ++k) {
        uint8_t *r = d->surface[k].raw; memset(r, 0, 16);
        int32_t a = 0x3000, b = 0x6000; int16_t wall = 0x300, jump = 0x300, limit = 0x300;
        memcpy(r + 0, &a, 4); memcpy(r + 4, &b, 4); memcpy(r + 0xA, &wall, 2); memcpy(r + 0xC, &jump, 2); memcpy(r + 0xE, &limit, 2);
    }
    d->surface_loaded = 1;
}

/* Jump and land through ae_player_update + ae_player_body_step (replaces the test_player.c stand-in). */
static void test_jump_land(const AePlayerData *d) {
    World w; world_init(&w, 0); AePlayerBodyState s; ae_player_body_state_init(&s); AePlayerBodyWorld bw; bw_init(&bw, &w, &s, d);
    AePlayer p; player_at(&p, 0, 0, 0);
    uint32_t unsup = 0, body = 0; int32_t min_y = 0; int left = 0, landed = 0;
    for (int f = 0; f < 240 && !landed; ++f) {
        AePlayerInput in; memset(&in, 0, sizeof(in)); in.frame_counter = (uint32_t)f;
        if (f == 5) { in.held = 0x0A; in.changed = 0x0A; } else if (f > 5 && f < 20) in.held = 0x0A;
        ae_player_stick_cam(&p, d, in.cam_dir, in.stick_cam);
        AePlayerGround g; memset(&g, 0, sizeof(g));
        AePlayerBodyGroundHit h; memset(&h, 0, sizeof(h));
        g.ground_y = w_ground(&w, p.pos[0], p.pos[1], p.pos[2], &h); g.slope_x = h.slope_x; g.slope_z = h.slope_z;
        unsup |= ae_player_update(&p, d, &in, &g);
        body |= ae_player_body_step(&p, &bw);
        if (f == 5) CHECK_EQ(p.state, 8);
        if (p.pos[1] < min_y) min_y = p.pos[1];
        if (p.pos[1] < -0x10) left = 1;
        CHECK(p.pos[1] <= 0);                                 /* never sinks below the floor */
        if (left && f > 10 && p.pos[1] == 0 && p.state == 0) landed = 1;
    }
    CHECK(left); CHECK(landed); CHECK(min_y < -0x10);
    CHECK_EQ(p.pos[1], 0); CHECK_EQ(p.vel[1], 0);
    CHECK_EQ(unsup, 0); CHECK_EQ(body, 0);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s member_fixture.bin\n", argv[0]); return 2; }
    size_t size; uint8_t *blob = read_file(argv[1], &size);
    if (!blob) { fprintf(stderr, "cannot read fixture\n"); return 2; }
    AePlayerData d; ae_player_data_init(&d);
    if (!ae_player_data_load_member(&d, blob, size)) { fprintf(stderr, "load: %s\n", ae_player_error()); return 2; }
    synthetic_surface(&d);
    test_flat_floor(&d); test_probe_ground(&d); test_steps(&d); test_restore(&d);
    test_walls_and_ceiling(&d); test_grid_modes_colliders(&d); test_ledge(&d); test_jump_land(&d);
    ae_player_data_free(&d); free(blob);
    printf("body: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
