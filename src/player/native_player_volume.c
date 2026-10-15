/*
 * Collider containment scan (BodyRecordColliderContact) and CollisionRayTest, portable C99.
 * Interface and wiring: native_player_volume.h.  Context: notes/70-native-player.md ("Volume queries").
 *
 * Citations are 1-based lines of extract/cloud_evidence/player/body_dependencies/<file>.asm.txt:
 *   BRC = 800792c8_BodyRecordColliderContact     CRT = 80078c9c_CollisionRayTest
 *   HPH = 8004ffc8_CollHalfPipeHeight             CTH = 800500d8_ColliderTrenchHeight
 *   CSD = 8004f884_ColliderSlopeContactDir        DSD = 800774b8_DiagonalSignedDistance
 *   CHS = 8004ff20_ColliderHalfSpaceInside        CTA = 80079a28_ColliderTypeActive
 *   SQ0 = 800303c0_SquareRoot0                    VL3 = 80046618_VecLength3
 *   CBP = 8007304c_CollisionBuildProbePoints      CSP = 800733dc_CollisionShiftProbePoints
 *   CPG = 8004f6e8_CollisionPointInGrid           PIB = 80073aa0_PointInBoxXYZ
 *   GGH = 80043400_GetGroundHeight                LG  = 80063ec4_PlayerTryLedgeGrab
 *
 * Collider fields read (record r[], int16 index = byte offset / 2; structs.txt "Collider"):
 *   +0x00..+0x04 pos  r[0..2]   lh  BRC:97-103, 126-132, 150-155, 174-198, 225-249, 283-306; DSD:23-139;
 *                                   CSD:7,15,29,43,52; CHS:18,26,30,34; PIB:2,17,32
 *   +0x06..+0x0A half r[3..5]   lh  BRC:94 (pointer), 162, 175, 186, 197, 226, 237, 248, 284, 295, 306;
 *                                   HPH:9,17; CTH:15,24,38,54,86,123,132,146,162,194; CSD:20-28; PIB:1,16,31
 *   +0x10 geometry    r[8]      lhu BRC:81 (class / shape), 216 (DSD a0), lbu BRC:267 (CSD a0); CHS:1;
 *                                   CRT:202,205 (lhu), 224 (lh, CTA a0)
 *   +0x12 type        r[9]      lbu BRC:77 (== 9 skipped); lhu CBP:186 (== 0xE never masked)
 *   +0x14 mode        r[10]     lhu CRT:223-228 (bit 15 -> CTA a1), CRT:236-238 (bit 13, modes 2 only)
 *   +0x16 contact id  r[11]     lhu CRT:243 (returned)
 *   +0x18 flags       r[12]     lhu BRC:69-76 ((flags & 0xF) == 3 or bit 15: skipped); CBP:178-181 (bit 15: masked)
 * Grid shape fields (g_CollShapes, 0x14 bytes, s[]): +0 cell x, +2 level, +4 cell z (CPG:66,74,97; CBP:131-147),
 *   +6..+A copied to the probe (CBP:122-130), +8 span (CPG:79), +0xC flags (CRT:82-97), +0xE id (CRT:105),
 *   +0x10 next (CPG:90), +0x12 type (CRT:51,56,93).
 *
 * MIPS arithmetic is reproduced with 32-bit wrap (addu/subu/mult-mflo) and signed slt compares.
 */
#include "native_player_volume.h"
#include <string.h>

static int32_t w32(uint32_t x) { return x <= 0x7FFFFFFFu ? (int32_t)x : (int32_t)(x - 0x80000000u) - 0x7FFFFFFF - 1; }
static int32_t add32(int32_t a, int32_t b) { return w32((uint32_t)a + (uint32_t)b); }
static int32_t sub32(int32_t a, int32_t b) { return w32((uint32_t)a - (uint32_t)b); }
static int32_t mul32(int32_t a, int32_t b) { return w32((uint32_t)a * (uint32_t)b); }
static int32_t neg32(int32_t a) { return sub32(0, a); }
static int32_t abs32(int32_t a) { return a < 0 ? neg32(a) : a; }          /* bgez / subu zero: abs(INT_MIN) = INT_MIN */
static int32_t sra32(int32_t a, int n) { return a >= 0 ? a >> n : ~((~a) >> n); }
static int32_t half_trunc(int32_t a) { return sra32(add32(a, (int32_t)((uint32_t)a >> 31)), 1); }   /* srl 31; addu; sra 1 */
static int32_t sdiv_round_floor9(int32_t a) { return sra32(a < 0 ? add32(a, 0x1FF) : a, 9); }       /* addiu 0x1ff if < 0; sra 9 */

void ae_volume_init(AeVolume *v) {
    memset(v, 0, sizeof(*v));
    v->wall_probe = 1;                      /* LG:94-96 sh 1, 0x296(g) = g_CollWallProbe right before the ray test */
    v->last_collider = -1; v->last_ray = -1;
    for (int i = 0; i < 8; ++i) v->probes[i].shape = -1;
}

/* ---------------------------------------------------------------- math */

static uint32_t isqrt_floor(uint32_t n) {
    uint32_t r = 0, bit = 1u << 30;
    while (bit > n) bit >>= 2;
    while (bit) { if (n >= r + bit) { n -= r + bit; r = (r >> 1) + bit; } else r >>= 1; bit >>= 2; }
    return r;
}

/* SquareRoot0 @ 800303c0 (SQ0:1-31).  LZCR counts leading sign bits; 32 (a = 0 or -1) returns 0 (SQ0:5-6,30-31).
   t2 = lz & ~1, t1 = (31 - t2) >> 1, t4 = a << (t2 - 24) or a >> (24 - t2) (the >> path at 0x80030408..0x80030410 is
   hidden by Ghidra's overlap warning; the pseudocode shows `a >> (0x18 - uVar1)`), value = (LUT[t4 - 0x40] << t1) >> 12
   (srl: logical).  LUT entries (0x800AE5C0, 192 x s16) are floor(sqrt(t4 << 18)): the Codex check of all 192 original
   entries (src/collision/native_collision.c line 132) - our own code, same fact.  Negative inputs other than -1 index
   below the table: fail closed. */
int32_t ae_volume_sqrt0(int32_t a, int *ok) {
    if (a == 0 || a == -1) return 0;
    if (a < 0) { *ok = 0; return 0; }
    uint32_t lz = 0, t = (uint32_t)a;
    while (!(t & 0x80000000u)) { ++lz; t <<= 1; }
    uint32_t t2 = lz & ~1u;
    int t1 = (int)(31 - t2) >> 1;
    uint32_t t4 = t2 >= 24 ? (uint32_t)a << (t2 - 24) : (uint32_t)a >> (24 - t2);   /* a > 0: sra == srl */
    uint32_t entry = isqrt_floor(t4 << 18);
    return (int32_t)((entry << t1) >> 12);
}

/* IntSqrt-based VecLength3 @ 80046618: GTE IR1..3 keep 16 bits, SQR0 (sf=0), MAC sum, IntSqrt (VL3:3-12).
   native_player.c already ports it instruction by instruction. */
static int32_t vec_length3(int32_t x, int32_t y, int32_t z) { return ae_player_vec_length3(x, y, z); }

/* ---------------------------------------------------------------- shape helpers */

/* CollHalfPipeHeight @ 8004ffc8 (HPH:1-68).  d = point - collider pos. */
int ae_volume_halfpipe_height(AeVolume *v, int16_t shape, const int16_t half[3], const int32_t d[3], int32_t *out, int *ok) {
    *out = 0;                                                   /* HPH:7 */
    int32_t hz = half[2], hx = half[0];
    if (hz < abs32(d[2])) return 0;                             /* HPH:8-14 */
    if (hx < abs32(d[0])) return 0;                             /* HPH:16-23 */
    int32_t s;
    if (shape == 0x24) {                                        /* HPH:24-27 (a0 sign-extended) */
        s = ae_volume_sqrt0(sub32(mul32(hz, hz), mul32(d[2], d[2])), ok);   /* HPH:29-36 */
        if (!*ok) { v->status |= AE_VOLUME_SQRT_NEGATIVE; return 0; }
        if (s < d[1]) return 0;                                 /* HPH:37-40 lw 0x4: dy */
        *out = s; v->contact_slope_x = 0;                       /* HPH:42-45 sw zero,-0xd14 = 0x800AF2EC */
    } else {
        s = ae_volume_sqrt0(sub32(mul32(hx, hx), mul32(d[0], d[0])), ok);   /* HPH:28 (delay slot mult a0,a0), 46-52 */
        if (!*ok) { v->status |= AE_VOLUME_SQRT_NEGATIVE; return 0; }
        if (s < d[1]) return 0;                                 /* HPH:53-56 */
        *out = s; v->contact_slope_z = 0;                       /* HPH:60-62 sw zero,-0xd10 = 0x800AF2F0 */
    }
    return 1;
}

/* ColliderTrenchHeight @ 800500d8 (CTH:1-238).  Shape 0x53 runs along x (radius in y/z, CTH:14-121); every other
   shape value takes the 0x54 path (along z, CTH:122-229).  VecLength3 gets a2 = dz (CTH:14) / dx (CTH:122) as its
   third argument: the pseudocode's VecLength3(dy, 0) drops it. */
int ae_volume_trench_height(AeVolume *v, int16_t shape, const int16_t half[3], const int32_t d[3], int32_t *out, int *ok) {
    *out = 0;                                                   /* CTH:13 */
    int a = shape == 0x53 ? 2 : 0, o = 2 - a;                   /* radial axis / along axis */
    int32_t r = half[a];
    if (add32(r, 0x100) < abs32(d[a])) return 0;               /* CTH:14-22 / 122-130 */
    if (half[o] < abs32(d[o])) return 0;                        /* CTH:23-30 / 131-138 */
    int32_t len = vec_length3(d[1], 0, d[a]);                   /* CTH:31-33 / 139-141 */
    int32_t sq;
    int neg;
    if (d[1] < 0) {                                             /* CTH:34-36 bgez dy */
        if (sub32(r, 0x100) < len) { sq = mul32(add32(r, 0x100), add32(r, 0x100)); neg = 0; }   /* CTH:38-53 */
        else { sq = mul32(sub32(r, 0x100), sub32(r, 0x100)); neg = 1; }                         /* CTH:67-76 */
    } else {
        if (add32(r, 0x100) < len) { sq = 0; neg = -1; }       /* CTH:54-61: out = 0 */
        else if (len < sub32(r, 0xC0)) { sq = mul32(sub32(r, 0x100), sub32(r, 0x100)); neg = 1; }   /* CTH:62-66 */
        else { sq = mul32(add32(r, 0x100), add32(r, 0x100)); neg = 1; }                        /* CTH:63, 67 */
    }
    if (neg >= 0) {
        int32_t s = ae_volume_sqrt0(sub32(sq, mul32(d[a], d[a])), ok);   /* CTH:68-74 / 176-182 */
        if (!*ok) { v->status |= AE_VOLUME_SQRT_NEGATIVE; return 0; }
        *out = neg ? neg32(s) : s;                              /* CTH:75-76 subu zero */
    }
    int32_t slope = 0;
    if (a == 2) v->contact_slope_x = 0; else v->contact_slope_z = 0;   /* CTH:77-78 / 185-186 */
    if (*out < 0) {                                             /* CTH:79-81 */
        int32_t m = abs32(*out);
        int32_t num = mul32(sub32(r, add32(m, 0x100)), 0xC00);  /* CTH:86-91: (3 * v) << 10 */
        int32_t den = sub32(r, 0x100);                          /* CTH:92 */
        if (den == 0 || (den == -1 && num == INT32_MIN)) {     /* CTH:94-102 break 0x1C00 / 0x1800 */
            v->status |= AE_VOLUME_TRENCH_TRAP; *ok = 0; return 0;
        }
        slope = num / den;                                      /* CTH:93,103 (C99 truncates like div) */
        if (d[a] > 0) slope = neg32(slope);                     /* CTH:104-109 blez */
        if (abs32(slope) < 0x80) slope = 0;                     /* CTH:110-119 */
    }
    if (a == 2) v->contact_slope_z = slope; else v->contact_slope_x = slope;
    return *out != 0;                                           /* CTH:230-232 sltu */
}

/* DiagonalSignedDistance @ 800774b8 (DSD:1-165).  p = point (int), rec = collider (cx = r[0], cz = r[2]).
   Result = s * SquareRoot0(|p - foot|^2), s = +1 on the side named below, else -1. */
int32_t ae_volume_diagonal_distance(uint16_t geometry, const int32_t p[3], const int16_t rec[28], int32_t out[3], int *ok) {
    int32_t px = p[0], pz = p[2], cx = rec[0], cz = rec[2];
    int s = -1, cond;                                           /* DSD:4 */
    switch (geometry & 0xF00) {                                 /* DSD:5-22 */
    case 0x000:                                                 /* DSD:23-51 */
        out[0] = half_trunc(sub32(add32(cx, cz), sub32(pz, px)));
        out[2] = half_trunc(add32(add32(cx, cz), sub32(pz, px)));
        cond = pz < add32(sub32(cx, px), cz); break;
    case 0x400:                                                 /* DSD:52-82 */
        out[0] = half_trunc(sub32(add32(px, pz), sub32(cz, cx)));
        out[2] = half_trunc(add32(add32(px, pz), sub32(cz, cx)));
        cond = sub32(add32(px, cz), cx) < pz; break;
    case 0x800:                                                 /* DSD:83-113 */
        out[0] = half_trunc(sub32(add32(cx, cz), sub32(pz, px)));
        out[2] = half_trunc(add32(add32(cx, cz), sub32(pz, px)));
        cond = add32(sub32(cx, px), cz) < pz; break;
    case 0xC00:                                                 /* DSD:114-142 */
        out[0] = half_trunc(sub32(add32(px, pz), sub32(cz, cx)));
        out[2] = half_trunc(add32(add32(px, pz), sub32(cz, cx)));
        cond = pz < sub32(add32(px, cz), cx); break;
    default:                                                    /* DSD:14,21: out (0x1F8000F0) left stale */
        *ok = 0; return 0;
    }
    if (cond) s = 1;                                            /* DSD:143-145 */
    int32_t ex = sub32(px, out[0]), ez = sub32(pz, out[2]);     /* DSD:146-159 */
    int32_t r = ae_volume_sqrt0(add32(mul32(ex, ex), mul32(ez, ez)), ok);
    if (!*ok) return 0;
    return mul32(s, r);                                         /* DSD:160,163 */
}

/* ColliderHalfSpaceInside @ 8004ff20 (CHS:1-42).  Not called by BRC or CRT; exported for collider_probe hosts. */
int ae_volume_halfspace_inside(const int16_t rec[28], const int32_t p[3]) {
    switch (rec[8] & 0xFF) {                                    /* CHS:1 lbu 0x10 */
    case 0x4E: return p[2] < rec[2];                            /* CHS:18-25 */
    case 0x4F: return p[0] < rec[0];                            /* CHS:26-29 */
    case 0x50: return rec[2] < p[2];                            /* CHS:30-33 */
    case 0x51: return rec[0] < p[0];                            /* CHS:34-40 */
    default: return 0;                                          /* CHS:9,16 */
    }
}

/* ColliderTypeActive @ 80079a28 (CTA:1-56). */
int ae_volume_type_active(int16_t wall_probe, uint32_t type, int16_t solid) {
    if (wall_probe == 1) return 1;                              /* CTA:5-8 */
    uint32_t cls = type & 0xF000, lo = type & 0xFF;
    int special = cls == 0x1000 || cls == 0x5000 || cls == 0x6000 || cls == 0x7000 || cls == 0x8000 ||
                  (cls == 0x3000 && (lo - 0x4E < 4u || lo == 0x36));   /* CTA:9-31 */
    if (wall_probe == 0) return special ? solid != 0 : 1;      /* CTA:36-45, 54 */
    return special ? solid != 0 : solid == 0;                   /* CTA:46-53 */
}

/* PointInBoxXYZ @ 80073aa0 (PIB:1-49): 0 when |c - p| < half + 0x200 on all three axes, else -1. */
int ae_volume_point_in_box(const int16_t rec[28], const int32_t p[3]) {
    for (int k = 0; k < 3; ++k) {
        int32_t lim = add32(rec[3 + k], 0x200);
        int32_t d = sub32(rec[k], p[k]);
        if (d < 0) d = sub32(p[k], rec[k]);
        if (!(d < lim)) return -1;
    }
    return 0;
}

/* ColliderSlopeContactDir @ 8004f884 (CSD:1-64): out[0] -> 0x1F800108, out[1] -> 0x1F80010C. */
static int slope_contact_dir(AeVolume *v, const int16_t rec[28], const int32_t p[3], int32_t out[2], int *ok) {
    int16_t shape = (int16_t)(rec[8] & 0xFF);                   /* BRC:267 lbu 0x10 */
    if (!v->shape_handler) { v->status |= AE_VOLUME_SLOPE_HANDLER; *ok = 0; return 0; }
    int32_t local[3] = { sub32(p[0], rec[0]), p[1], sub32(p[2], rec[2]) };   /* CSD:7-19: y stays absolute */
    int16_t half[3] = { rec[3], rec[4], rec[5] };               /* CSD:20-28 */
    int32_t io[2] = { rec[1], INT32_MIN };                      /* CSD:29-31 sp+0x28 = cy; sp+0x2C uninitialised */
    int r = v->shape_handler(v->user, shape, half, local, io);  /* CSD:32-38 g_CollShapeHandlers[shape] */
    if ((uint32_t)shape - 0x40u < 4u) {                         /* CSD:39-51 */
        /* A handler that returns 0 may leave sp+0x2C unwritten (CollShape_Ramp45 leaves it on the early range
           exits and on a failed underside test).  CSD then derives out[] from that stale word, but BRC:257-269
           (`beq v0,zero,0x800797b8` right after the jal) never reads out[] when the handler returned 0, so the
           stale value cannot influence the scan: no match, nothing unknown.  Only a non-zero return with the
           word unwritten is a genuine unknown. */
        if (!r) { out[0] = out[1] = 0; return 0; }
        if (io[1] == INT32_MIN) { v->status |= AE_VOLUME_SLOPE_HANDLER; *ok = 0; return 0; }
        out[0] = sub32(rec[1], io[1]); out[1] = sub32(io[0], io[1]);
    } else {
        out[0] = rec[1]; out[1] = io[0];                        /* CSD:52-57 */
    }
    return r;
}

/* ---------------------------------------------------------------- BodyRecordColliderContact scan */

int ae_volume_collider_scan(AeVolume *v, const int32_t pos[3], uint16_t *index) {
    *index = 0xFFFF;                                            /* body +0x34 after CU:154 */
    if (v->collider_count == 0) return 1;                       /* BRC:54-59 */
    if (!v->colliders) { v->status |= AE_VOLUME_NO_TABLE; return 0; }
    int unknown = 0;
    int32_t px = pos[0], py = pos[1], pz = pos[2];
    /* BRC:54-59, 316-320: i = count - 1 down to 0 as a signed 16-bit counter; every match stores i (sh s2,0x34),
       so the lowest matching index is the one left. */
    for (int16_t i = (int16_t)(uint16_t)(v->collider_count - 1u); i >= 0; --i) {
        const int16_t *c = v->colliders[i];                     /* BRC:62-68: g_Colliders + i * 0x38 */
        uint16_t flags = (uint16_t)c[12];
        if ((flags & 0xF) == 3 || (flags & 0x8000)) continue;   /* BRC:69-76 */
        if ((c[9] & 0xFF) == 9) continue;                       /* BRC:77-80 lbu 0x12 */
        uint16_t geom = (uint16_t)c[8];                         /* BRC:81 */
        int32_t cx = c[0], cy = c[1], cz = c[2], hx = c[3], hy = c[4], hz = c[5];
        int match = 0, ok = 1;
        uint32_t cls = geom & 0xF000u;
        int box_xz = (!(px < sub32(cx, hx)) && !(add32(cx, hx) < px) && !(pz < sub32(cz, hz)) && !(add32(cz, hz) < pz));
        int box_y = (!(py < sub32(cy, hy)) && !(cy < py));      /* BRC:305-313 (and 196-205, 247-256) */
        if (cls == 0x3000) {                                    /* BRC:82-171 */
            uint32_t sh = geom & 0xFFu;
            if (sh - 0x46u < 4u) continue;                      /* BRC:86-90 */
            int32_t d[3] = { sub32(px, cx), sub32(py, cy), sub32(pz, cz) };   /* BRC:97-109 / 126-138 -> 0x1F8000FC */
            int32_t h = 0;
            if (sh - 0x24u < 2u) {                              /* BRC:91-120 */
                if (ae_volume_halfpipe_height(v, (int16_t)sh, &c[3], d, &h, &ok) && ok)
                    match = !(py < sub32(cy, h)) && !(add32(cy, h) < py);
            } else if (sh - 0x53u < 2u) {                       /* BRC:121-149 */
                if (ae_volume_trench_height(v, (int16_t)sh, &c[3], d, &h, &ok) && ok) {
                    int32_t base = sub32(cy, h);
                    match = !(py < base) && !(add32(base, 0x40) < py);
                }
            } else {                                            /* BRC:150-171: vertical cylinder, then 0x80079788 */
                int32_t ex = sub32(cx, px), ez = sub32(cz, pz);
                if (add32(mul32(ex, ex), mul32(ez, ez)) < mul32(hx, hx)) match = box_y;
            }
        } else if (cls == 0x4000) {                             /* BRC:172-222: box, then diagonal half */
            if (box_xz && box_y) {
                int32_t foot[3] = { 0, 0, 0 };
                int32_t dist = ae_volume_diagonal_distance(geom, pos, c, foot, &ok);   /* BRC:206-218 a0 = lhu 0x10 */
                if (!ok) v->status |= (geom & 0xF00) == 0 || (geom & 0xF00) == 0x400 || (geom & 0xF00) == 0x800 ||
                                      (geom & 0xF00) == 0xC00 ? AE_VOLUME_SQRT_NEGATIVE : AE_VOLUME_DIAG_ORIENTATION;
                else match = dist < 1;                          /* BRC:219-222 bgtz -> skip */
            }
        } else if (cls == 0x2000) {                             /* BRC:223-280: box, then slope contact */
            if (box_xz && box_y) {
                int32_t o[2];
                int r = slope_contact_dir(v, c, pos, o, &ok);   /* BRC:257-269 */
                if (ok && r) match = !(py < sub32(cy, o[1])) && !(cy < py);   /* BRC:270-280 lw 0x4(s4) */
            }
        } else if (cls == 0) {                                  /* BRC:281-315 */
            match = box_xz && box_y;
        } else {
            continue;                                           /* BRC:281 bne v1,zero: other classes ignored */
        }
        if (!ok) unknown = 1;
        else if (match) { *index = (uint16_t)i; unknown = 0; } /* a lower index overwrites any earlier unknown */
    }
    if (unknown) { *index = 0xFFFF; return 0; }
    return 1;
}

int32_t ae_volume_collider_at(void *user, const int32_t pos[3]) {
    AeVolume *v = (AeVolume *)user;
    uint16_t idx;
    v->last_collider = -1;
    if (!ae_volume_collider_scan(v, pos, &idx) || idx == 0xFFFF) return -1;
    v->last_collider = idx;
    return idx;
}

/* ---------------------------------------------------------------- CollisionBuildProbePoints */

/* CollisionPointInGrid @ 8004f6e8 (CPG:1-103).  Returns the g_CollShapes index, -1 for NULL. */
int32_t ae_volume_point_in_grid(AeVolume *v, const AeVolumeProbe *e, int *ok) {
    int32_t hx = v->coll_half_x, hz = v->coll_half_z;           /* CPG:5,13 lw (32-bit) */
    int32_t x = e->x, y = e->y, z = e->z;                       /* lh */
    if (!(neg32(hx) < x) || !(x < hx)) return -1;               /* CPG:6-12 */
    if (!(neg32(hz) < z) || !(z < hz)) return -1;               /* CPG:13-20 */
    if (y > 0) return -1;                                       /* CPG:21-26 blez */
    if (!v->shape_grid) return -1;                              /* convention: no grid = every cell -1 */
    int32_t cx = sdiv_round_floor9(add32(x, hx));               /* CPG:32-42 */
    int32_t cz = sdiv_round_floor9(add32(z, hz));               /* CPG:38-48 */
    int32_t cy = sdiv_round_floor9(neg32(y));                   /* CPG:44-49, 60 */
    int32_t cell = add32(mul32(v->coll_grid_w, cz), cx);        /* CPG:50-57 */
    if (cell < 0 || (size_t)cell >= v->shape_grid_count) { v->status |= AE_VOLUME_GRID_RANGE; *ok = 0; return -1; }
    int32_t idx = v->shape_grid[cell];
    if (idx < 0) return -1;                                     /* CPG:59 */
    for (;;) {
        if (!v->shapes || (size_t)idx >= v->shape_count) { v->status |= AE_VOLUME_GRID_RANGE; *ok = 0; return -1; }
        const int16_t *s = v->shapes[idx];                      /* CPG:62-66, 94-97: shapes + idx * 0x14 */
        int32_t level = s[1];
        if (cy == level) return idx;                            /* CPG:99, 101 */
        if (cy < level) return -1;                              /* CPG:76-77 */
        int32_t span = sdiv_round_floor9(s[4]);                 /* CPG:79-84 lh 0x8 */
        if (!(add32(level, span) - 1 < cy)) return idx;         /* CPG:85-88 */
        idx = s[8];                                             /* CPG:90 lh 0x10 */
        if (idx < 0) return -1;                                 /* CPG:92 */
    }
}

/* CollisionBuildProbePoints(1, body) @ 8007304c (CBP:1-228) for the probe table; the collider mask
   (CBP:153-216) is evaluated on demand by ae_volume_collider_masked from build_pos. */
void ae_volume_build_probes(AeVolume *v, const int32_t pos[3], int32_t vel_y) {
    AeVolumeProbe *e = v->probes;
    v->built = 1;
    v->build_pos[0] = pos[0]; v->build_pos[1] = pos[1]; v->build_pos[2] = pos[2]; v->build_vel_y = vel_y;
    e[0].x = (int16_t)pos[0];                                   /* CBP:14-16 lhu/sh: low 16 bits */
    e[0].y = (int16_t)pos[1];                                   /* CBP:17-19 */
    if (vel_y > 0) e[0].y = (int16_t)(uint16_t)((uint16_t)pos[1] + (uint16_t)vel_y);   /* CBP:20-27 */
    e[0].z = (int16_t)pos[2];                                   /* CBP:28-31 */
    for (int i = 1; i < 8; ++i) {                               /* CBP:32-51 */
        e[i].x = e[0].x; e[i].z = e[0].z;
        e[i].y = i < 4 ? e[0].y : (int16_t)(uint16_t)((uint16_t)e[0].y - 0x200u);
    }
    /* CBP:52-81: quadrant of the body inside its 0x200 cell (32-bit pos + g_CollHalfX / Z) */
    int32_t ax = add32(pos[0], v->coll_half_x), az = add32(pos[2], v->coll_half_z);
    int32_t fx = sub32(sub32(ax, (int32_t)((uint32_t)sdiv_round_floor9(ax) << 9)), 0x100);
    int32_t fz = sub32(sub32(az, (int32_t)((uint32_t)sdiv_round_floor9(az) << 9)), 0x100);
    uint32_t q = (fz < 0 ? 1u : 0u) | (fx < 0 ? 2u : 0u);
    int16_t dx = (q & 2) ? -0x200 : 0x200, dz = (q & 1) ? -0x200 : 0x200;   /* CBP:82-106 */
    /* CollisionShiftProbePoints @ 800733dc (CSP:1-29): entries 1,2,4,5 get dz; 2,3,5,6 get dx */
    e[1].z = (int16_t)(e[1].z + dz); e[2].z = (int16_t)(e[2].z + dz); e[2].x = (int16_t)(e[2].x + dx);
    e[3].x = (int16_t)(e[3].x + dx); e[4].z = (int16_t)(e[4].z + dz); e[5].z = (int16_t)(e[5].z + dz);
    e[5].x = (int16_t)(e[5].x + dx); e[6].x = (int16_t)(e[6].x + dx);
    for (int i = 7; i >= 0; --i) {                              /* CBP:109-152 */
        int ok = 1;
        int32_t s = ae_volume_point_in_grid(v, &e[i], &ok);
        e[i].shape = ok ? s : -2;                               /* -2: unknown (fails closed in the ray test) */
        if (s >= 0) {
            const int16_t *sh = v->shapes[s];
            e[i].half[0] = sh[3]; e[i].half[1] = sh[4]; e[i].half[2] = sh[5];          /* CBP:122-130 */
            e[i].x = (int16_t)(uint16_t)((uint32_t)sh[0] * 0x200u - (uint16_t)v->coll_half_x + 0x100u);   /* CBP:131-136 */
            e[i].y = (int16_t)(uint16_t)(0u - (uint32_t)sh[1] * 0x200u);                 /* CBP:137-141 */
            e[i].z = (int16_t)(uint16_t)((uint32_t)sh[2] * 0x200u - (uint16_t)v->coll_half_z + 0x100u);   /* CBP:142-147 */
        }
    }
    /* Not modelled: CBP:190-196 writes collider +0x32 = -1 for type-1 colliders (body type 1); no reader here. */
}

/* PTR_DAT_800B20C0[i] (CBP:168-211): bit-15 disabled -> 1; (u16)type == 0xE -> 0; else PointInBoxXYZ != 0. */
int ae_volume_collider_masked(const AeVolume *v, uint16_t i) {
    const int16_t *c = v->colliders[i];
    if ((uint16_t)c[12] & 0x8000) return 1;                     /* CBP:178-185 */
    if ((uint16_t)c[9] == 0xE) return 0;                        /* CBP:186-189 lhu 0x12 */
    return ae_volume_point_in_box(c, v->build_pos) != 0;        /* CBP:198-211 */
}

/* ---------------------------------------------------------------- CollisionRayTest */

int32_t ae_volume_ray(AeVolume *v, uint32_t mode, const int32_t probe[5], int *ok) {
    uint32_t sub = mode >> 16;                                  /* CRT:13 srl s3,a0,0x10 */
    if (sub != 0) { v->status |= AE_VOLUME_RAY_MODE; *ok = 0; return 0; }
    if (!v->built) { v->status |= AE_VOLUME_RAY_NOT_PREPARED; *ok = 0; return 0; }
    if (v->collider_count && !v->colliders) { v->status |= AE_VOLUME_NO_TABLE; *ok = 0; return 0; }
    const int32_t point[3] = { probe[0], probe[1], probe[2] };  /* CRT:25-34 -> *PTR 0x800B20B4 */
    int32_t params[3] = { probe[3], 0, probe[4] };              /* CRT:38-42 -> 0x1F800114 / 0x1F80011C */
    int32_t s5 = -1;                                            /* CRT:26 */
    int grid_unknown = 0;
    /* CRT:43-126: the eight probe points of the body (PTR_DAT_800B20B0), index 7 down to 0 */
    for (int i = 7; i >= 0; --i) {
        const AeVolumeProbe *e = &v->probes[i];
        if (e->shape == -1) continue;                           /* CRT:47-49 */
        if (e->shape < 0 || !v->grid_probe) {
            grid_unknown = 1; v->status |= e->shape < 0 ? AE_VOLUME_GRID_RANGE : AE_VOLUME_GRID_PROBE; continue;
        }
        const int16_t *s = v->shapes[e->shape];
        params[1] = s[9];                                       /* CRT:51-53 lh 0x12 */
        int32_t out[6] = { 0, 0, 0, 0, 0, 0 };                  /* CRT:57-64 */
        if (v->grid_probe(v->user, e, s, params, point, out) == 0) continue;   /* CRT:65-73 */
        if (!ae_volume_type_active(v->wall_probe, (uint32_t)(int32_t)s[9], (int16_t)((uint16_t)s[6] & 0x8000u)))
            continue;                                           /* CRT:87-99 */
        s5 = s[7];                                              /* CRT:105 lh 0xe(a1): sign-extended */
        grid_unknown = 0;                                       /* a later (lower) hit overwrites s5 */
        /* CRT:112-115: *out = 1 (the caller's halfword; PlayerTryLedgeGrab never reads it) */
    }
    /* CRT:127-343: colliders, count - 1 down to 0; mode 0 returns the first hit (CRT:244-245, 320-321, 383) */
    for (int16_t i = (int16_t)(uint16_t)(v->collider_count - 1u); i >= 0; --i) {
        if (ae_volume_collider_masked(v, (uint16_t)i)) continue;   /* CRT:140-148 */
        if (!v->collider_probe) { v->status |= AE_VOLUME_COLLIDER_PROBE; *ok = 0; return 0; }
        int16_t copy[28];
        memcpy(copy, v->colliders[i], sizeof(copy));           /* CRT:149-199: 0x38-byte copy to PTR_DAT_800B20BC */
        params[1] = (uint16_t)copy[8];                          /* CRT:202-204 lhu 0x10 */
        int32_t out[6] = { 0, 0, 0, 0, 0, 0 };                  /* CRT:207-218 */
        if (v->collider_probe(v->user, copy, params, point, out) == 0) continue;   /* CRT:219-220 */
        if (!ae_volume_type_active(v->wall_probe, (uint32_t)(int32_t)copy[8], (int16_t)((uint16_t)copy[10] & 0x8000u)))
            continue;                                           /* CRT:221-230 */
        return (uint16_t)copy[11];                              /* CRT:241-245, 320-321, 383: lhu 0x16 */
    }
    if (grid_unknown) { *ok = 0; return 0; }
    if (s5 >= 0) return s5;                                     /* CRT:344-345 */
    /* CRT:346-383: GetGroundHeight(x, y - probe[3], z) */
    if (!v->ground) { v->status |= AE_VOLUME_NO_GROUND; *ok = 0; return 0; }
    int32_t qy = sub32(probe[1], probe[3]);
    AeVolumeGroundHit gh; memset(&gh, 0, sizeof(gh));
    gh.hit.solid_kind = -1;                                     /* GGH:16-24: 0x3F0 = -1, 0x3FA = 0 */
    int32_t g = v->ground(v->user, probe[0], qy, probe[2], &gh);
    if (!(g < qy)) return s5;                                   /* CRT:352-358 */
    if (gh.hit.solid_kind >= 0) return s5;                      /* CRT:359-364 lh 0x1F8003F0 */
    if (!gh.word_3fa_known) { v->status |= AE_VOLUME_WORD_3FA; *ok = 0; return 0; }
    return gh.word_3fa;                                         /* CRT:365-369, 383 (lhu, sign-extended) */
}

int32_t ae_volume_ray_test(void *user, uint32_t mode, const int32_t probe[5]) {
    AeVolume *v = (AeVolume *)user;
    int ok = 1;
    int32_t r = ae_volume_ray(v, mode, probe, &ok);
    v->last_ray = ok ? r : 0;
    return ok ? r : 0;                                          /* fail closed: >= 0 = blocked, no ledge snap */
}

/* ---------------------------------------------------------------- body-world adapters */

int32_t ae_volume_body_ground(void *user, int32_t x, int32_t y, int32_t z, AePlayerBodyGroundHit *hit) {
    AeVolume *v = (AeVolume *)user;
    if (!v->ground) { v->status |= AE_VOLUME_NO_GROUND; return y; }   /* invalid world: reported, no meaning */
    AeVolumeGroundHit gh; memset(&gh, 0, sizeof(gh));
    gh.hit = *hit;
    int32_t g = v->ground(v->user, x, y, z, &gh);
    *hit = gh.hit;
    return g;
}

/* CU:160 CollisionBuildProbePoints(1, body) then CU:163 CollisionResolveProbes(1, body): same body state. */
int ae_volume_body_walls(void *user, const int32_t pos[3], const int32_t vel[3], int32_t radius, int32_t height,
                         AePlayerBodyWallHit *out) {
    AeVolume *v = (AeVolume *)user;
    ae_volume_build_probes(v, pos, vel[1]);
    if (!v->walls) { memset(out, 0, sizeof(*out)); return 1; }
    return v->walls(v->user, pos, vel, radius, height, out);
}
