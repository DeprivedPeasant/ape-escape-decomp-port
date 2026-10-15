/* Continuous-replay harness for replay_check.py (shared library, ctypes).
 *
 * Links the unchanged player module, body pass and volume callbacks to Codex's host adapter and
 * collision engine (src/app/native_player_host.c, src/collision/native_collision.c, native_ground.c) which are
 * compiled from their own path, read-only.  One handle = one area.
 *
 * replay_seed()  : copy the represented fields of the 0x150-byte Player RAM image into native state.
 * replay_frame() : ONE PlayerUpdate + body step.  The ground query is computed natively from the real
 *                  area data (host ground_cb + GetSurfaceType) unless the caller passes an override.
 * Nothing in here reads the captured post-update / post-collision state.
 */
#include "../../app/native_player_host.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define API __declspec(dllexport)
#else
#define API
#endif

typedef struct { const char *name; size_t native; int ram, width, count, sign; } Field;
#define F(n,r,w,c,s) {#n,offsetof(AePlayer,n),r,w,c,s}
static const Field fields[] = {
 F(pos,0x24,4,3,1),F(vel,0x30,4,3,1),F(prev_facing,0x3c,2,4,1),
 F(body_radius,0x44,2,1,1),F(body_height,0x46,2,1,1),F(contact,0x50,4,1,0),
 F(anim_frame,0x5c,2,1,0),F(state,0x5e,2,1,0),F(facing,0x60,2,4,1),
 F(state_flags,0x70,4,1,0),F(desired_dir,0x74,2,4,1),F(saved_facing,0x80,2,4,1),
 F(next_frame,0x88,2,1,1),F(anim_lerp,0x8a,2,1,0),F(anim_blend,0x8c,2,1,0),
 F(anim_speed,0x8e,2,1,0),F(attach_dir,0xd8,2,3,1),F(attach_timer,0xde,2,1,0),
 F(attach_flags,0xe0,4,1,0),F(prev_attach_flags,0xe4,4,1,0),F(health,0xe8,2,1,1),
 F(invuln_timer,0xea,2,1,1),F(surface_kind,0xec,2,1,1),F(unk_ee,0xee,1,1,0),
 F(contact_frames,0xef,1,1,0),F(hurt,0xf0,1,1,0),F(contact_cooldown,0xf1,1,1,0),
 F(current_gadget,0xf2,1,1,1),F(fall_frames,0xfc,2,1,1),F(squash_timer,0xfe,2,1,1),
 F(slope_x,0x104,2,1,1),F(slope_z,0x106,2,1,1),F(vel_frac,0x108,4,3,1),
 F(target_move_vel,0x114,4,3,1),F(move_vel,0x120,4,3,1),F(ground_y,0x12c,4,1,1),
 F(has_ground,0x136,2,1,1),F(ground_type,0x138,2,1,0),F(prev_ground_type,0x13a,2,1,0)
};
#define NF ((int)(sizeof(fields)/sizeof(fields[0])))
/* One extra compared word that lives in the body state, not in AePlayer: Player+0x58 collider index. */
#define COLLIDER_RAM 0x58

typedef struct { AePlayerHost host; AeGround *ground; AeCollision *coll; } Replay;

static int64_t value(const uint8_t *p, int width, int sign) {
    uint32_t v = 0; for (int i = 0; i < width; i++) v |= (uint32_t)p[i] << (8*i);
    if (sign && width == 1) return (int8_t)v;
    if (sign && width == 2) return (int16_t)v;
    if (sign && width == 4) return (int32_t)v;
    return v;
}

API int replay_field_count(void) { return NF + 1; }
API const char *replay_field_name(int i) { return i < NF ? fields[i].name : "collider_index"; }
API int replay_field_meta(int i, int which) {
    if (i == NF) return which == 0 ? COLLIDER_RAM : which == 1 ? 2 : which == 2 ? 1 : 0;
    const Field *f = &fields[i]; return which == 0 ? f->ram : which == 1 ? f->width : which == 2 ? f->count : f->sign;
}

API Replay *replay_open(const char *area_member, const char *exe, const char *player_member, unsigned area) {
    Replay *r = calloc(1, sizeof(*r)); int32_t pos[3] = {0}; int16_t dir[3] = {0, 0, 4096};
    if (!r) return NULL;
    r->ground = ae_ground_load(area_member); r->coll = ae_collision_load(area_member);
    if (!r->ground || !r->coll || !ae_collision_math_load(exe) ||
        !ae_player_host_init(&r->host, r->ground, r->coll, player_member, exe, area, pos, dir)) {
        free(r); return NULL;
    }
    return r;
}
API void replay_close(Replay *r) { ae_player_host_free(&r->host); ae_ground_free(r->ground); ae_collision_free(r->coll); free(r); }

/* Seed from the 0x150-byte Player image.  hurt_blink / dash_boost / damage_enabled are not in that image
 * (g_PlayerHurtBlink, DAT_800F5126, level flag): the caller passes the first captured values. */
API void replay_seed(Replay *r, const uint8_t *ram, int32_t damage_enabled, int32_t hurt_blink, int32_t dash_boost) {
    AePlayerHost *h = &r->host; int32_t pos[3] = {0}; int16_t dir[4] = {0, 0, 0x1000, 1};
    ae_player_init(&h->player, pos, dir);
    for (int i = 0; i < NF; i++) { const Field *f = &fields[i]; memcpy((uint8_t *)&h->player + f->native, ram + f->ram, (size_t)(f->width * f->count)); }
    h->player.damage_enabled = damage_enabled; h->player.hurt_blink = (int16_t)hurt_blink; h->player.dash_boost = (int16_t)dash_boost;
    ae_player_body_state_init(&h->body);
    h->body.collider_index = (uint16_t)(ram[COLLIDER_RAM] | ram[COLLIDER_RAM + 1] << 8);
    h->update_flags = h->body_flags = 0; h->failed = 0;
}

/* input[16] = captured input record (see check_native_player.py / bridge).  derive: bit0 = derive `changed`
 * from the previous frame's held word, bit1 = derive stick_class from stick_mag, bit2 = derive stick_cam from
 * the native facing, bit3 = feed hurt/dash/damage externals from `input` instead of native state.
 * ground_override: NULL = native world query; else 6 values {y, slope_x, slope_z, flags, type, water_y}.
 * Outputs: out[replay_field_count()]; aux[0..5] = native ground query actually computed from native pos,
 * aux[6..7] = native hurt_blink, dash_boost after the frame. */
API void replay_frame(Replay *r, const int32_t *input, uint32_t mode, uint32_t frame, uint32_t derive, uint32_t prev_held,
                      const int32_t *ground_override, int64_t *out, int64_t *aux, uint32_t *bits) {
    AePlayerHost *h = &r->host; AePlayerInput in; AePlayerGround g; AePlayerBodyGroundHit hit;
    memset(&in, 0, sizeof(in)); memset(&g, 0, sizeof(g)); memset(&hit, 0, sizeof(hit));
    in.held = (uint32_t)input[0];
    in.changed = (derive & 1) ? (prev_held ^ in.held) : (uint32_t)input[1];
    in.stick_mag = (uint16_t)input[3];
    in.stick_class = (derive & 2) ? ae_player_stick_class(in.stick_mag) : (uint16_t)input[2];
    for (int i = 0; i < 4; i++) in.cam_dir[i] = (int16_t)input[4 + i];
    if (derive & 4) ae_player_stick_cam(&h->player, &h->data, in.cam_dir, in.stick_cam);
    else { in.stick_cam[0] = (int16_t)input[8]; in.stick_cam[1] = (int16_t)input[9]; }
    in.right_stick_gesture = (uint16_t)input[10]; in.locked = input[11]; in.frame_counter = (uint32_t)input[12];
    if (derive & 8) { h->player.damage_enabled = input[13]; h->player.hurt_blink = (int16_t)input[14]; h->player.dash_boost = (int16_t)input[15]; }
    /* ---- ground query ---- */
    int16_t water_y = 0;
    h->volume.status = 0;
    int32_t gy = h->world.ground(h->world.user, h->player.pos[0], h->player.pos[1], h->player.pos[2], &hit);
    uint16_t gt = ae_collision_surface_type(h->collision, h->player.pos, &water_y);
    aux[0] = gy; aux[1] = hit.slope_x; aux[2] = hit.slope_z; aux[3] = hit.surface_flags; aux[4] = gt; aux[5] = water_y;
    if (ground_override) { g.ground_y = ground_override[0]; g.slope_x = (int16_t)ground_override[1]; g.slope_z = (int16_t)ground_override[2];
        g.surface_flags = (uint16_t)ground_override[3]; g.ground_type = (uint16_t)ground_override[4]; g.water_y = (int16_t)ground_override[5]; }
    else { g.ground_y = gy; g.slope_x = hit.slope_x; g.slope_z = hit.slope_z; g.surface_flags = hit.surface_flags; g.ground_type = gt; g.water_y = water_y; }
    /* ---- world globals ---- */
    h->world.player_mode = (uint16_t)mode; h->world.frame_counter = frame;
    h->volume.status = 0; h->failed = 0;
    bits[0] = ae_player_update(&h->player, &h->data, &in, &g);
    bits[1] = ae_player_body_step(&h->player, &h->world);
    bits[2] = h->volume.status; bits[3] = (uint32_t)h->failed;
    aux[6] = h->player.hurt_blink; aux[7] = h->player.dash_boost;
    int n = 0;
    for (int i = 0; i < NF; i++) { const Field *f = &fields[i];
        for (int k = 0; k < f->count; k++) out[n++] = value((const uint8_t *)&h->player + f->native + k * f->width, f->width, f->sign); }
    out[n++] = h->body.collider_index;
}

/* ---- diagnostics: query the world exactly as the body step does ---- */
API int replay_debug_walls(Replay *r, const int32_t pos[3], const int32_t vel[3], int32_t radius, int32_t height, int32_t out[6]) {
    AePlayerBodyWallHit w; memset(&w, 0, sizeof(w));
    int ok = r->host.world.walls(r->host.world.user, pos, vel, radius, height, &w);
    out[0] = w.push_x; out[1] = w.push_z; out[2] = w.vel_y_add; out[3] = w.ceiling; out[4] = (int32_t)w.contact; out[5] = (int32_t)r->host.volume.status;
    return ok;
}
API int32_t replay_debug_ground(Replay *r, int32_t x, int32_t y, int32_t z, int32_t out[6]) {
    AePlayerBodyGroundHit h; memset(&h, 0, sizeof(h)); h.solid_kind = -1;
    int32_t g = r->host.world.ground(r->host.world.user, x, y, z, &h);
    out[0] = h.slope_x; out[1] = h.slope_z; out[2] = h.surface_flags; out[3] = h.solid_kind; out[4] = h.solid_shape; out[5] = h.solid_type;
    return g;
}

/* Hypothesis testing only: overwrite the position words (record words 0..2 = centre x, top y, z) of one collider
 * in the area's collision data held by this handle (the captured trace does not contain the live collider table). */
API int replay_debug_set_collider_pos(Replay *r, int index, int x, int y, int z) {
    const int16_t (*cols)[28]; const int16_t *grid; const int16_t (*shapes)[10];
    ae_collision_arrays(r->coll, &grid, &shapes, &cols);
    if (index < 0 || index >= r->host.volume.collider_count) return 0;
    int16_t *c = (int16_t *)(void *)cols[index];
    c[0] = (int16_t)x; c[1] = (int16_t)y; c[2] = (int16_t)z;
    return 1;
}
