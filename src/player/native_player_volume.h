#ifndef AE_NATIVE_PLAYER_VOLUME_H
#define AE_NATIVE_PLAYER_VOLUME_H
/*
 * Portable C99 reconstruction of the two world queries that native_player_body.c leaves to the host:
 *
 *   ae_volume_collider_at  ->  AePlayerBodyWorld.collider_at
 *       the collider scan of BodyRecordColliderContact (0x800792C8, BRC:56-320) incl. the shape
 *       helpers CollHalfPipeHeight (0x8004FFC8), ColliderTrenchHeight (0x800500D8),
 *       ColliderSlopeContactDir (0x8004F884), DiagonalSignedDistance (0x800774B8), SquareRoot0 (0x800303C0)
 *   ae_volume_ray_test     ->  AePlayerBodyWorld.ray_test
 *       CollisionRayTest (0x80078C9C) for mode 0 (the only mode PlayerTryLedgeGrab uses), over the
 *       probe points / collider mask built by CollisionBuildProbePoints (0x8007304C) with
 *       CollisionPointInGrid (0x8004F6E8), CollisionShiftProbePoints (0x800733DC), PointInBoxXYZ
 *       (0x80073AA0) and ColliderTypeActive (0x80079A28).
 *
 * Evidence: extract/cloud_evidence/player/body_dependencies/<addr>_<name>.asm.txt (line numbers in
 * native_player_volume.c are 1-based lines of those listings).  Where the Ghidra pseudocode and the
 * listing differ, the listing is followed (notes/70-native-player.md, "Volume queries").
 *
 * WIRING (one context is the `user` of every AePlayerBodyWorld callback):
 *     AeVolume v; ae_volume_init(&v);
 *     v.colliders = recs; v.collider_count = n;            // 28 x int16 records (0x38 bytes) as the Codex loader returns
 *     v.ground = my_ground; v.walls = my_walls; v.user = my_world;   // the host's real callbacks
 *     w.user = &v; w.ground = ae_volume_body_ground; w.walls = ae_volume_body_walls;
 *     w.ray_test = ae_volume_ray_test; w.collider_at = ae_volume_collider_at; w.collider_count = n;
 *     v.status = 0; ae_player_body_step(&p, &w);  -> also check v.status (AE_VOLUME_* bits)
 * ae_volume_body_walls records the body exactly as CollisionBuildProbePoints sees it (CU:160 runs
 * just before CollisionResolveProbes at CU:163 on the same body) and forwards to v.walls.
 *
 * Record layout (Collider, tools/structs.txt; int16 index = byte offset / 2):
 *   [0..2] pos (centre x, TOP y, z)   [3..5] halfSize   [8] geometry (+0x10)   [9] type (+0x12)
 *   [10] mode (+0x14, bit 15 solid)   [11] contact id (+0x16)   [12] flags (+0x18, 0x8000 disabled)
 *
 * Everything the listings do not contain is either a host callback below or fails closed: the query
 * returns its "nothing" value (collider_at -1, ray_test 0 = blocked, i.e. no ledge snap) and ORs an
 * AE_VOLUME_* bit into v.status.  Nothing here is validated against the original game.
 */
#include <stddef.h>
#include <stdint.h>
#include "native_player_body.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fail-closed bits (AeVolume.status, OR-accumulated; the caller clears it). */
enum {
    AE_VOLUME_NO_TABLE          = 1u << 0,  /* collider_count > 0 but colliders == NULL */
    AE_VOLUME_SLOPE_HANDLER     = 1u << 1,  /* class 0x2000: g_CollShapeHandlers[] (0x800AF180) not supplied */
    AE_VOLUME_SQRT_NEGATIVE     = 1u << 2,  /* SquareRoot0 of a value < -1: reads below the LUT at 0x800AE5C0 */
    AE_VOLUME_DIAG_ORIENTATION  = 1u << 3,  /* DiagonalSignedDistance with (geometry & 0xF00) not 0/0x400/0x800/0xC00: stale 0x1F8000F0 */
    AE_VOLUME_TRENCH_TRAP       = 1u << 4,  /* ColliderTrenchHeight div traps (break 0x1C00 / 0x1800) */
    AE_VOLUME_RAY_NOT_PREPARED  = 1u << 5,  /* ray_test before ae_volume_body_walls / ae_volume_build_probes ran */
    AE_VOLUME_RAY_MODE          = 1u << 6,  /* CollisionRayTest mode >> 16 != 0 (LoopSlot paths not modelled) */
    AE_VOLUME_GRID_PROBE        = 1u << 7,  /* grid cell shape reached but grid_probe (table 0x800B20E0) not supplied */
    AE_VOLUME_COLLIDER_PROBE    = 1u << 8,  /* near collider reached but collider_probe (table 0x800B2108) not supplied */
    AE_VOLUME_GRID_RANGE        = 1u << 9,  /* shape-grid / shape index outside the arrays supplied */
    AE_VOLUME_NO_GROUND         = 1u << 10, /* GetGroundHeight fallback reached without a ground callback */
    AE_VOLUME_WORD_3FA          = 1u << 11  /* fallback needs scratch 0x1F8003FA (GetSolidHeight 0x8004D8C0 may write it) */
};

/* Extra GetGroundHeight output used by CollisionRayTest's fallback (CRT:365-369). */
typedef struct {
    AePlayerBodyGroundHit hit;      /* as for AePlayerBodyWorld.ground */
    int16_t word_3fa;               /* scratch 0x1F8003FA after the call (GetGroundHeight zeroes it at GGH:21) */
    uint8_t word_3fa_known;         /* host sets 1 when word_3fa is exact (e.g. no solids / GetSolidHeight modelled) */
} AeVolumeGroundHit;

/* One CollisionBuildProbePoints entry (0x10 bytes at PTR_DAT_800B20B0 + 16 * i). */
typedef struct {
    int16_t x, y, z;                /* +0x0/+0x2/+0x4 */
    int16_t half[3];                /* +0x6/+0x8/+0xA, copied from the grid shape when one is found */
    int32_t shape;                  /* +0xC pointer -> index into AeVolume.shapes, -1 = NULL */
} AeVolumeProbe;

typedef struct AeVolume {
    /* ---- world data ---- */
    const int16_t (*colliders)[28]; /* g_Colliders (0x800F455C) */
    uint16_t collider_count;        /* g_ColliderCount (0x800F45D4) */
    /* collision shape grid (CollisionPointInGrid).  shape_grid == NULL = every cell index -1 (no shapes). */
    int32_t coll_half_x, coll_half_z;   /* g_CollHalfX / g_CollHalfZ (0x800F45F8 / 0x800F45FC) */
    int32_t coll_grid_w;                /* g_CollGridW (0x800F45F0) */
    const int16_t *shape_grid; size_t shape_grid_count;   /* g_CollShapeGrid (0x800F4560), s16 per cell */
    const int16_t (*shapes)[10]; size_t shape_count;      /* g_CollShapes (0x800F4558), 0x14-byte records */
    int16_t wall_probe;             /* g_CollWallProbe (0x800F45E6); PlayerTryLedgeGrab sets 1 (LG:96). init: 1 */

    /* ---- host callbacks (all receive `user`) ---- */
    void *user;
    /* GetGroundHeight 0x80043400. Fill hit->hit (and word_3fa if known). REQUIRED by ae_volume_body_ground. */
    int32_t (*ground)(void *user, int32_t x, int32_t y, int32_t z, AeVolumeGroundHit *hit);
    /* Forwarded CollisionResolveProbes equivalent (AePlayerBodyWorld.walls semantics). NULL = empty hit. */
    int (*walls)(void *user, const int32_t pos[3], const int32_t vel[3], int32_t radius, int32_t height,
                 AePlayerBodyWallHit *out);
    /* g_CollShapeHandlers[shape] (0x800AF180), as called by ColliderSlopeContactDir (CSD:7-38):
       local = { x - cx, y (absolute), z - cz }; io[0] in = cy, out = height (sp+0x28);
       io[1] = sp+0x2C, which the handler must write for shapes 0x40..0x43.  Return = handler v0. */
    int (*shape_handler)(void *user, int16_t shape, const int16_t half[3], const int32_t local[3], int32_t io[2]);
    /* Grid-cell probe handler PTR_CollProbe_Box_800B20E0[(u16)shape[9] >> 12] (CRT:44-71):
       params = { radius, (s16)shape[+0x12], height }; point = the ray probe; out[6] cleared. Return v0. */
    int32_t (*grid_probe)(void *user, const AeVolumeProbe *entry, const int16_t shape[10],
                          const int32_t params[3], const int32_t point[3], int32_t out[6]);
    /* Collider probe handler PTR_CollProbe_Box_800B2108[(geometry & 0xF000) >> 12] (CRT:191-218):
       record = the 0x38-byte copy; params = { radius, (u16)geometry, height }. Return v0. */
    int32_t (*collider_probe)(void *user, const int16_t record[28], const int32_t params[3],
                              const int32_t point[3], int32_t out[6]);

    /* ---- state ---- */
    int built;                      /* CollisionBuildProbePoints ran (ae_volume_build_probes) */
    int32_t build_pos[3], build_vel_y;  /* the body it ran on */
    AeVolumeProbe probes[8];        /* PTR_DAT_800B20B0 */
    uint32_t status;                /* AE_VOLUME_* bits */
    /* diagnostics of the last helper calls */
    int32_t contact_slope_x, contact_slope_z;   /* g_CollContactSlopeX / Z (0x800AF2EC / 0x800AF2F0) */
    int32_t last_collider, last_ray;
} AeVolume;

void ae_volume_init(AeVolume *v);

/* ---- AePlayerBodyWorld callbacks (user = AeVolume *) ---- */
int32_t ae_volume_collider_at(void *user, const int32_t pos[3]);          /* BRC scan: lowest index or -1 */
int32_t ae_volume_ray_test(void *user, uint32_t mode, const int32_t probe[5]);  /* CollisionRayTest */
int32_t ae_volume_body_ground(void *user, int32_t x, int32_t y, int32_t z, AePlayerBodyGroundHit *hit);
int ae_volume_body_walls(void *user, const int32_t pos[3], const int32_t vel[3], int32_t radius, int32_t height,
                         AePlayerBodyWallHit *out);

/* ---- lower-level pieces ---- */
/* BRC scan: writes *index (0xFFFF = none, the CU:154 reset value) and returns 1, or 0 + status bits. */
int ae_volume_collider_scan(AeVolume *v, const int32_t pos[3], uint16_t *index);
/* CollisionBuildProbePoints(1, body) on body pos and (post-BodyProbeGround) vel.y. */
void ae_volume_build_probes(AeVolume *v, const int32_t pos[3], int32_t vel_y);
/* CollisionRayTest(mode, probe{x, y, z, radius, height}); *ok = 0 when it failed closed. */
int32_t ae_volume_ray(AeVolume *v, uint32_t mode, const int32_t probe[5], int *ok);
/* PTR_DAT_800B20C0[i] as CollisionBuildProbePoints leaves it (1 = skipped by CollisionRayTest). */
int ae_volume_collider_masked(const AeVolume *v, uint16_t i);

/* Helpers (return 1/0 like the original; *ok = 0 when they failed closed).  point/local are int32. */
int32_t ae_volume_sqrt0(int32_t a, int *ok);                                        /* SquareRoot0 0x800303C0 */
int ae_volume_halfpipe_height(AeVolume *v, int16_t shape, const int16_t half[3], const int32_t d[3], int32_t *out, int *ok);
int ae_volume_trench_height(AeVolume *v, int16_t shape, const int16_t half[3], const int32_t d[3], int32_t *out, int *ok);
int32_t ae_volume_diagonal_distance(uint16_t geometry, const int32_t p[3], const int16_t rec[28], int32_t out[3], int *ok);
int ae_volume_halfspace_inside(const int16_t rec[28], const int32_t p[3]);         /* ColliderHalfSpaceInside 0x8004FF20 */
int ae_volume_type_active(int16_t wall_probe, uint32_t type, int16_t solid);         /* ColliderTypeActive 0x80079A28 */
int ae_volume_point_in_box(const int16_t rec[28], const int32_t p[3]);                /* PointInBoxXYZ: 0 near, -1 far */
int32_t ae_volume_point_in_grid(AeVolume *v, const AeVolumeProbe *e, int *ok);        /* CollisionPointInGrid: shape index or -1 */

#ifdef __cplusplus
}
#endif
#endif
