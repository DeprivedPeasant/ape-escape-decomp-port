/*
 * Player collision-body pass of CollisionUpdate (0x8007247C), portable C99.
 * See native_player_body.h for the interface and notes/70-native-player.md for the context.
 *
 * Citations: "CU:n"  = 8007247c_CollisionUpdate.asm.txt line n
 *            "BPG:n" = 80072d28_BodyProbeGround.asm.txt line n
 *            "BLG:n" = 80072dcc_BodyLandOnGround.asm.txt line n
 *            "CRP:n" = 80073450_CollisionResolveProbes.asm.txt line n
 *            "LG:n"  = 80063ec4_PlayerTryLedgeGrab.asm.txt line n
 *            "BRC:n" = 800792c8_BodyRecordColliderContact.asm.txt line n
 * (all in extract/cloud_evidence/player/local_handoff/).  Where the listing and the Ghidra
 * pseudocode disagree, the listing is followed and the difference is noted inline.
 *
 * Fields read / written (body = Player + 0x24):
 *   Player.state (+0x5E)          read  CU:14, BLG:92, LG via stateFlags, BRC:39-55
 *   Player.stateFlags (+0x70)     read  BLG:87-90, LG:85-88, BRC:27-30
 *   Player.health (+0xE8)         read  LG:9-11; written by PlayerDamage (8005e6f0) on the restore path
 *   Player.facing (+0x60..+0x67)  read  LG:37-64; written LG:196-203 (ledge snap)
 *   Player.slopeX/Z (+0x104)      written by GetGroundHeightEx LG:108-109; read as one word LG:139-141
 *   body pos +0x00..+0x08         read/written CU:18-28 (state 0x2D), CRP:26-34, CRP:277-287, BLG:42-49, BLG:59-68,
 *                                 LG:180-195, CU:253-283 (grid clamp)
 *   body vel +0x0C..+0x14         read/written CU:148-150 (mode 5), BPG:39-82, CRP:101-105, CRP:234-239,
 *                                 CRP:330-345, BLG:42-43, BLG:66-71, LG:190-192
 *   body radius/height +0x20/+0x22 read CRP:37-38, BLG:23-30, LG:129-130
 *   body contact +0x2C            cleared CU:81; written CRP:271, CRP:333; read BLG:82-84, LG:13-16
 *   body +0x34 collider index     latched CU:143-151, reset CU:154, written BRC:222/315
 *   snapshot PTR_DAT_800B20B8     copied CU:112-129 (0x38 bytes), read BLG:50-71
 */
#include "native_player_body.h"
#include <string.h>

static void body_emit(AePlayer *p, uint8_t kind, int32_t a) {
    if (p->event_count < AE_PLAYER_EVENT_MAX) {
        AePlayerEvent *e = &p->events[p->event_count++];
        e->kind = kind; e->a = a; e->b = 0; e->c = 0;
    }
}

/* PlayerDamage @ 8005e6f0: no-op on levels 0x1F / 0x20 (AePlayer.damage_enabled carries that), else
   health -= amount (floor 0), invulnTimer = 0x3C, hurt = 1.  HudShowHealth is reported as an event. */
static void body_player_damage(AePlayer *p, const AePlayerBodyWorld *w, int amount) {
    if (amount == 0 || !p->damage_enabled || w->current_level == 0x20 || w->current_level == 0x1F) return;
    p->health = (int16_t)(p->health - amount < 0 ? 0 : p->health - amount);
    p->invuln_timer = 0x3C; p->hurt = 1;
    body_emit(p, AE_EV_DAMAGE, amount);
}

static int32_t ground(const AePlayerBodyWorld *w, int32_t x, int32_t y, int32_t z, AePlayerBodyGroundHit *hit) {
    memset(hit, 0, sizeof(*hit));
    hit->solid_kind = -1;                       /* GetGroundHeight initialises 0x1F8003F0 to 0xFFFF */
    return w->ground(w->user, x, y, z, hit);
}

void ae_player_body_state_init(AePlayerBodyState *s) {
    memset(s, 0, sizeof(*s));
    s->collider_index = 0xFFFF; s->mode5_collider = 0xFFFF; s->mode6_collider = 0xFFFF;
}

/* BodyProbeGround @ 80072d28 (BPG:5-38): anti-tunnelling for fast downward motion.  For vel.y >= 0x20
   it samples the ground at (x + vel.x, y + i, z + vel.z) for i = 0x20, 0x40, ... while vel.y - i >= 0
   and cuts vel.y to the first i whose sample lies below the ground (signed compares, full int result). */
static void body_probe_ground(const AePlayerBodyWorld *w, int32_t pos[3], int32_t vel[3]) {
    if (vel[1] < 0x20) return;                                              /* BPG:5-7 slti 0x20 */
    for (int32_t i = 0x20; vel[1] - i >= 0; i += 0x20) {                    /* BPG:12-14 */
        AePlayerBodyGroundHit h;
        int32_t g = ground(w, pos[0] + vel[0], pos[1] + i, pos[2] + vel[2], &h);   /* BPG:15-23 */
        if (g < pos[1] + i) { vel[1] = i; return; }                         /* BPG:24-30 sw s0,0x10 */
    }
}

/* PlayerTryLedgeGrab @ 80063ec4, acting on the body copy (g_Player.pos/vel are the body). */
static uint32_t body_try_ledge_grab(AePlayer *p, const AePlayerBodyWorld *w, AePlayerBodyState *s,
                                    int32_t pos[3], int32_t vel[3], uint32_t contact) {
    if (p->health <= 0) return 0;                                           /* LG:9-11 blez */
    if ((contact >> 28) == 8) return 0;                                     /* LG:13-16 srl 0x1c */
    uint16_t m = w->player_mode;                                            /* LG:21-35 */
    if (m == 3 || m == 1 || m == 5 || m == 6) return 0;
    int16_t dir[3] = { 0, 0, 0 };                                           /* LG:37-64: octant of facing */
    dir[0] = (int16_t)(p->facing[0] > 0x800 ? 1 : p->facing[0] < -0x800 ? -1 : 0);
    dir[2] = (int16_t)(p->facing[2] > 0x800 ? 1 : p->facing[2] < -0x800 ? -1 : 0);
    /* LG:65-67 VectorNormalSS(dir, dir) is called before the vel.y test (no side effects other than dir). */
    if (vel[1] <= 0) return 0;                                              /* LG:70-72 lw 0x34 (vel.y), blez */
    AePlayerBodyGroundHit h;
    int16_t here = (int16_t)ground(w, pos[0], pos[1], pos[2], &h);          /* LG:74-81: result truncated to s16 */
    if (!(pos[1] < here)) return 0;                                         /* LG:82-83 */
    if ((p->state_flags & 0xF0) == 0xC0) return 0;                          /* LG:85-88 */
    /* LG:96 g_CollWallProbe (0x800F45E6) = 1: global flag, not represented. */
    if (dir[0] == 0 && dir[2] == 0) return AE_PLAYER_BODY_LEDGE_ZERO_DIR;
    if (!w->data) return AE_PLAYER_BODY_LEDGE_UNKNOWN;                      /* VectorNormalSS needs the module's sine/sqrt data */
    int16_t n[3];
    ae_player_normalize(w->data, dir, n);
    int32_t dx = n[0] / 32, dz = n[2] / 32;                                 /* LG:93-98: addiu 0x1f if < 0, sra 5 */
    /* LG:99-110 GetGroundHeightEx(x+dx, y-0x100, z+dz, &Player.slopeX): writes the slopes into the player */
    int16_t top = (int16_t)ground(w, pos[0] + dx, pos[1] - 0x100, pos[2] + dz, &h);   /* LG:111,115 s16 */
    p->slope_x = h.slope_x; p->slope_z = h.slope_z;
    int32_t probe[5] = { pos[0] + dx, pos[1] - 0x100, pos[2] + dz, p->body_radius, p->body_height };  /* LG:117-138 */
    if ((uint32_t)pos[1] - (uint32_t)(int32_t)top - 0x81u >= 0x7Fu) return 0;               /* LG:133-137 sltiu: 0x81 <= y-top <= 0xFF */
    if (p->slope_x != 0 || p->slope_z != 0) return 0;                       /* LG:139-141 lw 0x104 (both halves) */
    if (h.solid_kind < 0) return 0;                                         /* LG:143-145 lh 0x1F8003F0, bltz */
    if (h.surface_flags & 0x4000) return 0;                                 /* LG:147-150 */
    uint16_t shape = (uint16_t)(h.solid_shape & 0xFF);                      /* LG:152-162 */
    if (shape == 0x24 || shape == 0x25 || shape == 0x53 || shape == 0x54) return 0;
    if ((h.solid_type & 0xFF) == 10) return 0;                              /* LG:164-167 */
    if (!w->ray_test) return AE_PLAYER_BODY_LEDGE_UNKNOWN;                  /* CollisionRayTest 0x80078C9C */
    if (w->ray_test(w->user, 0, probe) >= 0) return 0;                      /* LG:168-173 bgez -> no grab */
    pos[0] += dx; pos[1] = top; pos[2] += dz;                               /* LG:174-195 */
    vel[0] = vel[1] = vel[2] = 0;
    /* PlayerSetState writes nothing in pos/vel; the body copy is written back by the caller. */
    ae_player_set_state(p, w->data, 0x0E);                                  /* LG:186,194 */
    p->facing[0] = n[0]; p->facing[1] = n[1]; p->facing[2] = n[2];          /* LG:196-203 */
    /* facing.pad (+0x66) receives sp+0x16, which the listing never initialises: left unchanged here. */
    s->ledge_grabbed = 1;
    return 0;
}

/* BodyRecordColliderContact @ 800792c8 for body type 1: gating BRC:18-55, scan BRC:56.. */
static uint32_t body_record_collider(const AePlayer *p, const AePlayerBodyWorld *w, AePlayerBodyState *s,
                                     const int32_t pos[3]) {
    if ((p->state_flags & 0xF0) == 0xE0 || p->state == 0x23 || p->state == 0x4A || p->state == 0x2D) return 0;
    if (w->player_mode == 6) return 0;                                      /* BRC:40-49 */
    if (w->collider_count == 0) return 0;                                   /* BRC:50-56 loop does not run */
    if (!w->collider_at) return AE_PLAYER_BODY_COLLIDER_UNKNOWN;
    int32_t idx = w->collider_at(w->user, pos);
    if (idx >= 0) s->collider_index = (uint16_t)idx;                        /* BRC:222 / BRC:315 sh s2,0x34 */
    return 0;
}

uint32_t ae_player_body_step(AePlayer *p, const AePlayerBodyWorld *w) {
    uint32_t bits = 0;
    AePlayerBodyState tmp, *s = w->state;
    if (!s) { ae_player_body_state_init(&tmp); s = &tmp; }
    if (!w->ground) return AE_PLAYER_BODY_NO_GROUND;
    s->restored = 0; s->ledge_grabbed = 0; s->ceiling = 0;

    int32_t pos[3], vel[3];
    for (int i = 0; i < 3; ++i) { pos[i] = p->pos[i]; vel[i] = p->vel[i]; }

    /* CU:14-28: state 0x2D skips the whole collision pass: pos += vel, nothing else (contact kept). */
    if (p->state == 0x2D) {
        for (int i = 0; i < 3; ++i) pos[i] += vel[i];
        ae_player_apply_motion(p, pos, vel, p->contact);
        return 0;
    }

    uint32_t contact = 0;                                                   /* CU:81 sw zero,0x2c(s0) for every body */

    /* CU:112-129: snapshot of the 0x38-byte body before anything moves. */
    for (int i = 0; i < 3; ++i) { s->snapshot_pos[i] = pos[i]; s->snapshot_vel[i] = vel[i]; }

    s->mode6_collider = 0xFFFF; s->mode5_collider = 0xFFFF;                 /* CU:132,134 sh s3 (-1), before the mode test */
    /* CU:130-138: g_PlayerMode 2 only builds the probe points (CollisionBuildProbePoints); the player body
       is not moved and +0x34 is not touched.  It then falls through to the grid clamp. */
    if (w->player_mode != 2) {
        if (w->player_mode == 6) s->mode6_collider = s->collider_index;     /* CU:140-144 */
        else if (w->player_mode == 5) {                                      /* CU:146-151 */
            s->mode5_collider = s->collider_index;
            vel[2] = 0; vel[0] = 0;
        }
        s->collider_index = 0xFFFF;                                          /* CU:154 sh s3,0x34(s0) */
        if (w->player_mode == 1) bits |= AE_PLAYER_BODY_MODE_SIDE_FX;        /* CU:153 PlayerMoveRumbleSoundUpdate */

        body_probe_ground(w, pos, vel);                                      /* CU:155 */
        s->probe_vel_y = vel[1];
        /* CU:157 CollisionStub_ReturnZero: returns 0, no effect.  CU:160 CollisionBuildProbePoints: host side. */

        /* CU:163 CollisionResolveProbes(1, body) */
        AePlayerBodyWallHit wh; memset(&wh, 0, sizeof(wh));
        if (w->walls) {
            int32_t vin[3] = { vel[0], vel[1], vel[2] };
            if (!w->walls(w->user, pos, vin, p->body_radius, p->body_height, &wh)) {
                bits |= AE_PLAYER_BODY_WALL_FAILED; memset(&wh, 0, sizeof(wh));
            }
        }
        pos[2] += vel[2]; pos[0] += vel[0];                                  /* CRP:26-34: z and x move before probing */
        vel[1] += wh.vel_y_add;                                              /* CRP:101-105 (probe 7), CRP:234-239 (colliders) */
        pos[0] += wh.push_x; pos[2] += wh.push_z;                            /* CRP:277-287 */
        if (wh.push_x != 0 || wh.push_z != 0 || wh.contact != 0) {           /* CRP:284-293 */
            /* CRP:294-329 type-2 (RC car) despawn branch does not apply to type 1. */
            contact = wh.contact;                                            /* CRP:333 sw s6,0x2c(s3) */
            if (wh.push_x != 0) vel[0] += wh.push_x;                         /* CRP:330-337 */
            if (wh.push_z != 0) vel[2] += wh.push_z;                         /* CRP:338-345 */
        }
        int32_t ceiling = wh.ceiling < 0 ? 0 : wh.ceiling;                   /* CRP:346-351: max(local_38, 0) */
        s->ceiling = ceiling;

        /* CU:167 BodyLandOnGround(body, 1, ceiling).  BLG:19 DAT_800F45E8 = 1 (global flag, not represented). */
        {
            int32_t h = p->body_height;                                      /* BLG:23,26-27 lhu + sll/sra: s16 */
            int32_t half = h / 2;                                            /* BLG:28-30 (h + sign) >> 1 = truncating /2 */
            AePlayerBodyGroundHit gh;
            int32_t g = ground(w, pos[0], pos[1] + vel[1] - half, pos[2], &gh);   /* BLG:20-32, full int result */
            s->land_ground_y = g;                                            /* BLG:34 sw v1,0xc(s1) = 0x1F8000FC */
            if (g < pos[1] + vel[1]) {                                       /* BLG:35-40 slt (signed) */
                vel[1] = g - pos[1];                                         /* BLG:42-43 */
                pos[1] = g;                                                  /* BLG:44-46 */
            } else {
                pos[1] = pos[1] + vel[1] + ceiling;                          /* BLG:47-49 */
            }
            if (pos[1] - s->snapshot_pos[1] < -0xFF) {                       /* BLG:50-57 slti -0xff: moved UP by more than 0xFF */
                pos[0] = s->snapshot_pos[0]; pos[1] = s->snapshot_pos[1];    /* BLG:59-71 */
                vel[2] = 0; vel[0] = 0;
                pos[2] = s->snapshot_pos[2];
                vel[1] = s->snapshot_vel[1];
                s->restored = 1;
                /* BLG:72-134 type 1 */
                uint16_t shape = 0;
                if (gh.solid_kind == 1) shape = (uint16_t)(gh.solid_shape & 0xFF);   /* BLG:75-81 */
                if (contact != 0 && (p->state_flags & 0xF0) != 0xE0 && p->state != 0x23 && p->state != 0x4A &&
                    p->state != 0x2D && shape != 0x24 && shape != 0x25 && w->current_area != 0x44) {   /* BLG:82-110 */
                    if (w->current_level == 0x20) {                          /* BLG:111-114 */
                        if (!w->data) bits |= AE_PLAYER_BODY_DEATH_NO_DATA;
                        else ae_player_set_state(p, w->data, 0x22);          /* BLG:120-121 */
                        body_emit(p, AE_EV_XA, (w->frame_counter & 1) ? 0xD : 0xF);   /* BLG:122-132 */
                    } else {
                        body_player_damage(p, w, 5);                         /* BLG:116-117 */
                    }
                }
            }
        }

        /* CU:169 PlayerTryLedgeGrab() */
        bits |= body_try_ledge_grab(p, w, s, pos, vel, contact);

        /* CU:172 BodyRecordColliderContact(body, 1) */
        bits |= body_record_collider(p, w, s, pos);
    }

    /* CU:253-283: clamp x and z to +-(dim - 1) * 0x100 (dims read with lh: signed). */
    if (w->grid_width <= 0 || w->grid_depth <= 0) bits |= AE_PLAYER_BODY_NO_GRID;
    else {
        int32_t lx = ((int32_t)w->grid_width - 1) * 0x100, lz = ((int32_t)w->grid_depth - 1) * 0x100;
        if (pos[0] < -lx) pos[0] = -lx; else if (pos[0] > lx) pos[0] = lx;
        if (pos[2] < -lz) pos[2] = -lz; else if (pos[2] > lz) pos[2] = lz;
    }

    ae_player_apply_motion(p, pos, vel, contact);
    return bits;
}
