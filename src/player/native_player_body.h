#ifndef AE_NATIVE_PLAYER_BODY_H
#define AE_NATIVE_PLAYER_BODY_H
/*
 * Portable C99 reconstruction of what CollisionUpdate (0x8007247C) does to the PLAYER collision
 * body (type 1) each frame, after PlayerUpdate has produced p->vel.  It replaces the host
 * "pos += vel + floor clamp" stand-in described in notes/70-native-player.md ("Interface").
 *
 * Evidence (all under extract/cloud_evidence/player/local_handoff/; ".asm" = the
 * .asm.txt listing, line numbers are 1-based lines of that file):
 *   CollisionUpdate          8007247c_CollisionUpdate.{c,asm.txt}
 *   BodyProbeGround          80072d28_BodyProbeGround.{c,asm.txt}
 *   BodyLandOnGround         80072dcc_BodyLandOnGround.{c,asm.txt}
 *   CollisionResolveProbes   80073450_CollisionResolveProbes.{c,asm.txt}
 *   PlayerTryLedgeGrab       80063ec4_PlayerTryLedgeGrab.{c,asm.txt}
 *   BodyRecordColliderContact 800792c8_BodyRecordColliderContact.{c,asm.txt}
 *   PlayerDamage             8005e6f0_PlayerDamage.c
 * The per-line field map is in native_player_body.c.
 *
 * The world (terrain grid, colliders, probe-shape handlers) is NOT in this module: the host
 * supplies it through AePlayerBodyWorld callbacks, so the Codex collision engine
 * (src/collision/native_collision.h) can be plugged in without this module linking it.
 *
 * Collision body = Player + 0x24 (structs.txt CollisionBody):
 *   +0x00 pos[3]  = Player+0x24  -> AePlayer.pos
 *   +0x0C vel[3]  = Player+0x30  -> AePlayer.vel
 *   +0x20 radius  = Player+0x44  -> AePlayer.body_radius
 *   +0x22 height  = Player+0x46  -> AePlayer.body_height
 *   +0x2C contact = Player+0x50  -> AePlayer.contact
 *   +0x34 collider index (u16)   = Player+0x58 -> AePlayerBodyState.collider_index (not in AePlayer)
 */
#include <stdint.h>
#include "native_player.h"

#ifdef __cplusplus
extern "C" {
#endif

/* GetGroundHeight (0x80043400) result + the scratchpad words it leaves behind. */
typedef struct {
    int16_t slope_x, slope_z;       /* 0x1F8003FC / 0x1F8003FE (what GetGroundHeightEx copies out) */
    uint16_t surface_flags;         /* 0x1F8003F8 cell attribute word */
    int16_t solid_kind;             /* 0x1F8003F0: -1 none (GetGroundHeight init 0xFFFF); compared == 1 and >= 0 */
    uint16_t solid_shape;           /* 0x1F8003F4: low byte = shape code (0x24/0x25 half pipe, 0x53/0x54 trench) */
    uint16_t solid_type;            /* 0x1F8003F6: low byte compared with 10 by PlayerTryLedgeGrab */
} AePlayerBodyGroundHit;

/* Result of the host's CollisionResolveProbes equivalent (grid probes + collider probes). */
typedef struct {
    int32_t push_x, push_z;         /* DAT_1F800118 / DAT_1F800120: summed horizontal push-out */
    int32_t vel_y_add;              /* sum of every ceiling term added to body vel.y (+0x10) */
    int32_t ceiling;                /* local_38: max positive top-probe (index 7) ceiling term, >= 0 */
    uint32_t contact;               /* uVar9 / s6: 0xA0000000 | id (0xA8000000 for collider type&0xFF == 1), 0 = none */
} AePlayerBodyWallHit;

/* Persistent per-player state that AePlayer does not carry. Initialise with ae_player_body_state_init. */
typedef struct {
    uint16_t collider_index;        /* body +0x34 (Player +0x58), 0xFFFF = none */
    uint16_t mode6_collider;        /* DAT_800B20C8: previous body+0x34 latched in g_PlayerMode 6 (0xFFFF otherwise) */
    uint16_t mode5_collider;        /* DAT_800B20CA: previous body+0x34 latched in g_PlayerMode 5 (0xFFFF otherwise) */
    /* diagnostics of the last step */
    int32_t snapshot_pos[3], snapshot_vel[3];   /* PTR_DAT_800B20B8 copy taken at the start of the body pass */
    int32_t land_ground_y;          /* DAT_1F8000FC: ground height sampled by BodyLandOnGround */
    int32_t probe_vel_y;            /* body vel.y after BodyProbeGround */
    int32_t ceiling;                /* CollisionResolveProbes return value passed to BodyLandOnGround */
    int restored;                   /* 1 when the > 0xFF upward step restored the snapshot */
    int ledge_grabbed;              /* 1 when PlayerTryLedgeGrab snapped onto a ledge */
} AePlayerBodyState;

typedef struct AePlayerBodyWorld {
    void *user;
    /* GetGroundHeight(x, y, z): int height (may be a solid's top); must fill *hit. REQUIRED. */
    int32_t (*ground)(void *user, int32_t x, int32_t y, int32_t z, AePlayerBodyGroundHit *hit);
    /* CollisionResolveProbes minus the arithmetic this module performs itself.  Inputs: body pos
       BEFORE the x/z motion, body vel AFTER BodyProbeGround, radius, height.  The host evaluates the
       eight grid probes and the colliders at (x + vel.x + push, y + vel.y, z + vel.z + push) and
       returns the sums.  Return 0 on failure (the module then reports AE_PLAYER_BODY_WALL_FAILED and
       uses an empty hit).  NULL = a world with no wall shapes and no colliders (empty hit, exact).
       Contact word: grid probe -> 0xA0000000 | (int32)(int16)cell[+0xE] (CRP:121-124, lh: sign-extended);
       collider -> 0xA0000000 | (uint16)collider[+0x16], | 0x08000000 when (type & 0xFF) == 1 (CRP:253-263).
       The last probe that pushed wins; a top-probe (index 7) ceiling hit does not set it.
       Codex adapter: AeMoveHit m; ae_collision_resolve(c, pos, vel, radius, height, &m);
         push_x = m.push_x; push_z = m.push_z; vel_y_add = m.velocity[1] - vel[1];
         ceiling = m.ceiling; contact = (uint32_t)m.contact. */
    int (*walls)(void *user, const int32_t pos[3], const int32_t vel[3], int32_t radius, int32_t height,
                 AePlayerBodyWallHit *out);
    /* CollisionRayTest(mode, probe{x, y, z, radius, height}, out): < 0 = clear. Only PlayerTryLedgeGrab uses it. */
    int32_t (*ray_test)(void *user, uint32_t mode, const int32_t probe[5]);
    /* BodyRecordColliderContact scan: the LOWEST collider index whose volume contains pos (the
       original scans downwards and overwrites), or -1.  Needed only when collider_count > 0. */
    int32_t (*collider_at)(void *user, const int32_t pos[3]);
    const AePlayerData *data;       /* needed for PlayerSetState (0x0E ledge, 0x22 death) and VectorNormalSS */
    AePlayerBodyState *state;       /* optional; NULL = a temporary state (collider index not kept) */
    int16_t grid_width, grid_depth; /* g_LevelGrid.width / depth (cells of 0x200) */
    uint16_t collider_count;        /* g_ColliderCount */
    uint16_t player_mode;           /* g_PlayerMode (0x800F44A0) */
    uint16_t current_level;         /* g_CurrentLevel (0x800F4474) */
    uint16_t current_area;          /* g_CurrentArea  (0x800F4476) */
    uint32_t frame_counter;         /* g_FrameCounter (XaPlay parity) */
} AePlayerBodyWorld;

/* Bits returned by ae_player_body_step: behaviour reached that needs something the module does not have. */
enum {
    AE_PLAYER_BODY_NO_GROUND      = 1u << 0,  /* w->ground missing: nothing was changed */
    AE_PLAYER_BODY_WALL_FAILED    = 1u << 1,  /* walls callback reported failure; empty hit used */
    AE_PLAYER_BODY_LEDGE_UNKNOWN  = 1u << 2,  /* PlayerTryLedgeGrab reached a test needing ray_test / data: no snap applied */
    AE_PLAYER_BODY_COLLIDER_UNKNOWN = 1u << 3,/* collider_count > 0 but no collider_at: index left 0xFFFF */
    AE_PLAYER_BODY_DEATH_NO_DATA  = 1u << 4,  /* level 0x20 restore wants PlayerSetState(0x22) but w->data is NULL */
    AE_PLAYER_BODY_MODE_SIDE_FX   = 1u << 5,  /* g_PlayerMode 1: PlayerMoveRumbleSoundUpdate sound loop / rumble not modelled */
    AE_PLAYER_BODY_NO_GRID        = 1u << 6,  /* grid_width or grid_depth <= 0: grid clamp skipped */
    AE_PLAYER_BODY_LEDGE_ZERO_DIR = 1u << 7   /* ledge octant vector was (0,0,0): VectorNormalSS of zero not modelled */
};

void ae_player_body_state_init(AePlayerBodyState *s);
/* One CollisionUpdate pass for the player body.  Call after ae_player_update.  Writes p->pos, p->vel,
   p->contact (via ae_player_apply_motion), may write p->state / facing / slope / health / events. */
uint32_t ae_player_body_step(AePlayer *p, const AePlayerBodyWorld *w);

#ifdef __cplusplus
}
#endif
#endif
