/*
 * Synthetic tests for native_player_volume.c (BodyRecordColliderContact scan, CollisionRayTest).
 * Expected values are hand-computed from the instruction listings cited in native_player_volume.c.
 * All collider records, grids and callbacks are made up: NOT original-game differential tests.
 * Optional -DWITH_CODEX_MATH cross-checks SquareRoot0 against src/collision/native_collision.c.
 */
#include "native_player_volume.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WITH_CODEX_MATH
#include "../collision/native_collision.h"
#endif

static int failures, checks;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { long long x_ = (long long)(a), y_ = (long long)(b); ++checks; if (x_ != y_) { ++failures; printf("FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, x_, y_); } } while (0)

static void rec(int16_t r[28], int cx, int cy, int cz, int hx, int hy, int hz, unsigned geom, unsigned type, unsigned mode,
                unsigned id, unsigned flags) {
    memset(r, 0, 28 * sizeof(int16_t));
    r[0] = (int16_t)cx; r[1] = (int16_t)cy; r[2] = (int16_t)cz; r[3] = (int16_t)hx; r[4] = (int16_t)hy; r[5] = (int16_t)hz;
    r[8] = (int16_t)(uint16_t)geom; r[9] = (int16_t)(uint16_t)type; r[10] = (int16_t)(uint16_t)mode;
    r[11] = (int16_t)(uint16_t)id; r[12] = (int16_t)(uint16_t)flags;
}
static int32_t at(AeVolume *v, int32_t x, int32_t y, int32_t z) { int32_t p[3] = { x, y, z }; return ae_volume_collider_at(v, p); }

/* ---------------------------------------------------------------- math */
static void test_sqrt(void) {
    int ok = 1;
    CHECK_EQ(ae_volume_sqrt0(0, &ok), 0); CHECK_EQ(ae_volume_sqrt0(-1, &ok), 0); CHECK(ok);
    CHECK_EQ(ae_volume_sqrt0(1, &ok), 1); CHECK_EQ(ae_volume_sqrt0(2, &ok), 1); CHECK_EQ(ae_volume_sqrt0(4, &ok), 2);
    CHECK_EQ(ae_volume_sqrt0(0x10000, &ok), 0x100); CHECK_EQ(ae_volume_sqrt0(0xC000, &ok), 221);
    CHECK_EQ(ae_volume_sqrt0(0x7FFFFFFF, &ok), 46152);       /* t2 = 0, t4 = 0x7F, LUT isqrt(127 << 18) = 5769, << 15 >> 12 */
    CHECK(ok);
    CHECK_EQ(ae_volume_sqrt0(-2, &ok), 0); CHECK(!ok);        /* below the LUT: fails closed */
#ifdef WITH_CODEX_MATH
    unsigned seed = 777; int bad = 0;
    for (int i = 0; i < 300000; ++i) {
        seed = seed * 1664525u + 1013904223u;
        int32_t n = (int32_t)(seed >> 1); if (i % 3 == 0) n >>= 12; if (i % 7 == 0) n >>= 20;
        if (n <= 0) continue;
        ok = 1; if (ae_volume_sqrt0(n, &ok) != ae_collision_sqrt(n, 1)) ++bad;
    }
    CHECK_EQ(bad, 0);
#endif
}

/* ---------------------------------------------------------------- BRC scan */
static int16_t T[8][28];

static void test_box_and_priority(void) {
    AeVolume v; ae_volume_init(&v); v.colliders = (const int16_t (*)[28])T; v.collider_count = 4;
    for (int i = 0; i < 4; ++i) rec(T[i], 0x4000, 0, 0x4000, 1, 1, 1, 0, 0, 0, 0, 0);   /* far away */
    rec(T[2], 0x100, 0x40, -0x100, 0x80, 0x100, 0x20, 0x0000, 0, 0, 0x55, 0);           /* class 0 box */
    /* x in [0x80, 0x180], z in [-0x120, -0xE0], y in [0x40 - 0x100, 0x40] (top y = pos.y, BRC:305-313) */
    CHECK_EQ(at(&v, 0x100, 0, -0x100), 2);
    CHECK_EQ(at(&v, 0x80, 0x40, -0x120), 2); CHECK_EQ(at(&v, 0x180, -0xC0, -0xE0), 2);      /* inclusive corners */
    CHECK_EQ(at(&v, 0x7F, 0, -0x100), -1); CHECK_EQ(at(&v, 0x181, 0, -0x100), -1);
    CHECK_EQ(at(&v, 0x100, 0x41, -0x100), -1); CHECK_EQ(at(&v, 0x100, -0xC1, -0x100), -1);
    CHECK_EQ(at(&v, 0x100, 0, -0x121), -1); CHECK_EQ(at(&v, 0x100, 0, -0xDF), -1);
    /* index order: the scan runs count-1 .. 0 and every hit overwrites, so the lowest index wins */
    memcpy(T[3], T[2], sizeof(T[2])); memcpy(T[1], T[2], sizeof(T[2]));
    CHECK_EQ(at(&v, 0x100, 0, -0x100), 1);
    /* disabled (flags 0x8000), (flags & 0xF) == 3, type low byte 9 (water), other classes: skipped */
    T[1][12] = (int16_t)0x8000; CHECK_EQ(at(&v, 0x100, 0, -0x100), 2);
    T[2][12] = 0x0013; CHECK_EQ(at(&v, 0x100, 0, -0x100), 3);
    T[2][12] = 0x0012; CHECK_EQ(at(&v, 0x100, 0, -0x100), 2);                           /* 2 != 3: active */
    T[2][9] = 0x0109; CHECK_EQ(at(&v, 0x100, 0, -0x100), 3);                            /* lbu: low byte 9 */
    T[2][9] = 0x0900; CHECK_EQ(at(&v, 0x100, 0, -0x100), 2);
    T[2][8] = 0x1000; CHECK_EQ(at(&v, 0x100, 0, -0x100), 3);                            /* class 0x1000 ignored */
    T[2][8] = 0x5000; CHECK_EQ(at(&v, 0x100, 0, -0x100), 3);
    CHECK_EQ(v.status, 0);
    /* empty / missing table */
    v.collider_count = 0; CHECK_EQ(at(&v, 0, 0, 0), -1); CHECK_EQ(v.status, 0);
    v.collider_count = 1; v.colliders = NULL; CHECK_EQ(at(&v, 0, 0, 0), -1); CHECK(v.status & AE_VOLUME_NO_TABLE);
    /* a signed 16-bit counter: count > 0x8000 never enters the loop (BRC:56-59) */
    v.colliders = (const int16_t (*)[28])T; v.collider_count = 0x8001; v.status = 0;
    CHECK_EQ(at(&v, 0x100, 0, -0x100), -1); CHECK_EQ(v.status, 0);
}

static void test_cylinder(void) {
    AeVolume v; ae_volume_init(&v); v.colliders = (const int16_t (*)[28])T; v.collider_count = 1;
    rec(T[0], 0, 0, 0, 0x100, 0x80, 0x7FFF, 0x3010, 0, 0, 0, 0);   /* class 0x3000 other shape: radius = half x */
    CHECK_EQ(at(&v, 0xFF, 0, 0), 0); CHECK_EQ(at(&v, 0x100, 0, 0), -1);   /* d^2 < r^2 strict */
    CHECK_EQ(at(&v, 0xB4, -0x80, 0xB4), 0);                            /* 0xB4^2 * 2 = 0xFD20 < 0x10000 */
    CHECK_EQ(at(&v, 0xB5, 0, 0xB5), 0); CHECK_EQ(at(&v, 0xB6, 0, 0xB6), -1);   /* 0xFFF2 < 0x10000; 0x102C8 */
    CHECK_EQ(at(&v, 0, -0x81, 0), -1); CHECK_EQ(at(&v, 0, 1, 0), -1);   /* y in [cy - hy, cy] */
    for (unsigned s = 0x46; s <= 0x49; ++s) { T[0][8] = (int16_t)(0x3000 | s); CHECK_EQ(at(&v, 0, 0, 0), -1); }
    T[0][8] = 0x304A; CHECK_EQ(at(&v, 0, 0, 0), 0);
    T[0][8] = 0x304E; CHECK_EQ(at(&v, 0, 0, 0), 0);                   /* half-space types use the cylinder path here */
}

static void test_halfpipe(void) {
    AeVolume v; ae_volume_init(&v); v.colliders = (const int16_t (*)[28])T; v.collider_count = 1;
    rec(T[0], 0, 0, 0, 0x200, 0x100, 0x100, 0x3024, 0, 0, 0, 0);
    v.contact_slope_x = 99;
    CHECK_EQ(at(&v, 0, -0x80, 0), 0); CHECK_EQ(v.contact_slope_x, 0); /* sqrt(0x100^2) = 0x100 >= dy */
    CHECK_EQ(at(&v, 0, 0x100, 0), 0); CHECK_EQ(at(&v, 0, 0x101, 0), -1);   /* helper: s < dy fails */
    CHECK_EQ(at(&v, 0, -0x100, 0), 0); CHECK_EQ(at(&v, 0, -0x101, 0), -1);  /* BRC: y < cy - h */
    CHECK_EQ(at(&v, 0, 0, 0x100), 0); CHECK_EQ(at(&v, 0, -1, 0x100), -1);   /* at the rim h = 0 */
    CHECK_EQ(at(&v, 0, 0, 0x101), -1); CHECK_EQ(at(&v, 0x201, 0, 0), -1);
    /* 0x25 uses half x as the radius (HPH:46-52) */
    T[0][8] = 0x3025; T[0][3] = 0x80; T[0][5] = 0x300;
    CHECK_EQ(at(&v, 0, -0x80, 0x2FF), 0); CHECK_EQ(at(&v, 0, -0x81, 0x2FF), -1);
    int32_t d[3] = { 0, 0, 0 }, h; int ok = 1; int16_t half[3] = { 0x80, 0, 0x300 };
    v.contact_slope_z = 7; CHECK_EQ(ae_volume_halfpipe_height(&v, 0x25, half, d, &h, &ok), 1);
    CHECK_EQ(h, 0x80); CHECK_EQ(v.contact_slope_z, 0);
}

static void test_trench(void) {
    AeVolume v; ae_volume_init(&v); v.colliders = (const int16_t (*)[28])T; v.collider_count = 1;
    rec(T[0], 0, 0, 0, 0x200, 0, 0x200, 0x3053, 0, 0, 0, 0);   /* along x, radius half z */
    int16_t half[3] = { 0x200, 0, 0x200 }; int32_t h; int ok = 1;
    int32_t d1[3] = { 0, -0x50, 0 };
    CHECK_EQ(ae_volume_trench_height(&v, 0x53, half, d1, &h, &ok), 1); CHECK_EQ(h, -0x100);   /* dy < 0, r - 0x100 >= L */
    CHECK_EQ(at(&v, 0, -0x50, 0), -1);                                   /* y must be in [cy - h, cy - h + 0x40] */
    CHECK_EQ(at(&v, 0, 0x120, 0), 0); CHECK_EQ(at(&v, 0, 0x100, 0), 0); CHECK_EQ(at(&v, 0, 0x13F, 0), 0);
    CHECK_EQ(at(&v, 0, 0xFF, 0), -1);
    int32_t d2[3] = { 0, 0x140, 0 };                                     /* L = 0x140, not < r - 0xC0: (r + 0x100)^2 */
    CHECK_EQ(ae_volume_trench_height(&v, 0x53, half, d2, &h, &ok), 1); CHECK_EQ(h, -0x300);
    CHECK_EQ(at(&v, 0, 0x140, 0), -1);
    int32_t d3[3] = { 0, 0x110, 0x80 };                                  /* L = IntSqrt(0x16100) = 301 < 0x140 */
    CHECK_EQ(ae_volume_trench_height(&v, 0x53, half, d3, &h, &ok), 1); CHECK_EQ(h, -221);
    CHECK_EQ(v.contact_slope_x, 0); CHECK_EQ(v.contact_slope_z, -420); /* (0x200 - 477) * 0xC00 / 0x100, dz > 0 */
    CHECK_EQ(at(&v, 0, 0x110, 0x80), 0);                                 /* base 221, 272 <= 285 */
    /* VecLength3's third argument is dz (CTH:14): with dy = 0x130 alone L = 304 (< 320), with dz = 0x80 L = 330 */
    int32_t d4[3] = { 0, 0x130, 0x80 };
    CHECK_EQ(ae_volume_trench_height(&v, 0x53, half, d4, &h, &ok), 1); CHECK_EQ(h, -ae_volume_sqrt0(0x90000 - 0x4000, &ok));
    CHECK(h != -221);
    /* outside the trench box */
    int32_t d5[3] = { 0, 0x120, 0x301 }; CHECK_EQ(ae_volume_trench_height(&v, 0x53, half, d5, &h, &ok), 0);
    int32_t d6[3] = { 0x201, 0x120, 0 }; CHECK_EQ(ae_volume_trench_height(&v, 0x53, half, d6, &h, &ok), 0);
    /* dy >= 0 beyond r + 0x100: out = 0, returns 0 */
    int32_t d7[3] = { 0, 0x301, 0 }; CHECK_EQ(ae_volume_trench_height(&v, 0x53, half, d7, &h, &ok), 0); CHECK_EQ(h, 0); CHECK(ok);
    /* 0x54: along z, radius half x, slope goes to X */
    int32_t d8[3] = { 0x80, 0x110, 0 };
    CHECK_EQ(ae_volume_trench_height(&v, 0x54, half, d8, &h, &ok), 1); CHECK_EQ(h, -221);
    CHECK_EQ(v.contact_slope_z, 0); CHECK_EQ(v.contact_slope_x, -420);
    int32_t d9[3] = { -0x80, 0x110, 0 };
    ae_volume_trench_height(&v, 0x54, half, d9, &h, &ok); CHECK_EQ(v.contact_slope_x, 420);
    /* small slopes are zeroed (|s| < 0x80) */
    int32_t d10[3] = { 0x10, 0x110, 0 };
    ae_volume_trench_height(&v, 0x54, half, d10, &h, &ok); CHECK_EQ(v.contact_slope_x, 0);
    CHECK(ok); CHECK_EQ(v.status, 0);
    /* r = 0x100 and out < 0: div by zero trap -> fails closed */
    int16_t h100[3] = { 0x200, 0, 0x100 }; int32_t dt[3] = { 0, 0x50, 0 };
    ok = 1; CHECK_EQ(ae_volume_trench_height(&v, 0x53, h100, dt, &h, &ok), 0); CHECK(!ok); CHECK(v.status & AE_VOLUME_TRENCH_TRAP);
    v.status = 0; T[0][5] = 0x100; CHECK_EQ(at(&v, 0, 0x50, 0), -1); CHECK(v.status & AE_VOLUME_TRENCH_TRAP);
}

static void test_diagonal(void) {
    AeVolume v; ae_volume_init(&v); v.colliders = (const int16_t (*)[28])T; v.collider_count = 1;
    rec(T[0], 0, 0, 0, 0x100, 0x100, 0x100, 0x4000, 0, 0, 0, 0);   /* orientation 0: solid where px + pz >= 0 */
    CHECK_EQ(at(&v, 0x80, -0x10, 0x80), 0); CHECK_EQ(at(&v, -0x80, -0x10, -0x80), -1);
    CHECK_EQ(at(&v, 0x40, 0, -0x40), 0);                                 /* on the diagonal: distance 0 */
    CHECK_EQ(at(&v, 0x101, 0, 0x80), -1);                                /* box test first */
    T[0][8] = 0x4400;                                                    /* 0x400: solid where pz <= px */
    CHECK_EQ(at(&v, 0x80, 0, -0x80), 0); CHECK_EQ(at(&v, -0x80, 0, 0x80), -1);
    T[0][8] = 0x4800;                                                    /* 0x800: s = +1 when cx - px + cz < pz */
    CHECK_EQ(at(&v, 0x80, 0, 0x80), -1); CHECK_EQ(at(&v, -0x80, 0, -0x80), 0);
    T[0][8] = 0x4C00;                                                    /* 0xC00: s = +1 when pz < px + cz - cx */
    CHECK_EQ(at(&v, 0x80, 0, -0x80), -1); CHECK_EQ(at(&v, -0x80, 0, 0x80), 0);
    int32_t p[3] = { 0x30, 0, -0x10 }, foot[3]; int ok = 1;
    CHECK_EQ(ae_volume_diagonal_distance(0x400, p, T[0], foot, &ok), -ae_volume_sqrt0(0x20 * 0x20 * 2, &ok));   /* solid side: -1 */
    CHECK_EQ(foot[0], 0x10); CHECK_EQ(foot[2], 0x10);
    CHECK_EQ(v.status, 0);
    /* orientation 0x300 reads a stale scratch result: fails closed, unless a lower index matches afterwards */
    T[0][8] = 0x4300; CHECK_EQ(at(&v, 0x80, 0, 0x80), -1); CHECK(v.status & AE_VOLUME_DIAG_ORIENTATION);
    v.collider_count = 2; memcpy(T[1], T[0], sizeof(T[0])); rec(T[0], 0x80, 0, 0x80, 4, 4, 4, 0, 0, 0, 0, 0);
    v.status = 0; CHECK_EQ(at(&v, 0x80, 0, 0x80), 0); CHECK_EQ(v.status & AE_VOLUME_DIAG_ORIENTATION, AE_VOLUME_DIAG_ORIENTATION);
    /* status records the unknown, the answer is still exact because index 0 overwrote it */
    memcpy(T[0], T[1], sizeof(T[0])); rec(T[1], 0x80, 0, 0x80, 4, 4, 4, 0, 0, 0, 0, 0);   /* unknown at the LOWER index */
    v.status = 0; CHECK_EQ(at(&v, 0x80, 0, 0x80), -1);                  /* index 1 matched first, 0 unknown: fail closed */
}

/* class 0x2000: shape handler stub (g_CollShapeHandlers is not in the evidence) */
typedef struct { int calls; int16_t shape; int32_t local[3], io_in; int32_t height, extra; int ret, write_extra; } Slope;
static int slope_handler(void *u, int16_t shape, const int16_t half[3], const int32_t local[3], int32_t io[2]) {
    Slope *s = (Slope *)u; (void)half; s->calls++; s->shape = shape;
    for (int i = 0; i < 3; ++i) s->local[i] = local[i];
    s->io_in = io[0]; io[0] = s->height; if (s->write_extra) io[1] = s->extra;
    return s->ret;
}
static void test_slope(void) {
    AeVolume v; ae_volume_init(&v); v.colliders = (const int16_t (*)[28])T; v.collider_count = 1;
    rec(T[0], 0x100, 0x20, 0x100, 0x80, 0x100, 0x80, 0x2012, 0, 0, 0, 0);
    CHECK_EQ(at(&v, 0x100, 0, 0x100), -1); CHECK(v.status & AE_VOLUME_SLOPE_HANDLER);   /* fails closed */
    CHECK_EQ(at(&v, 0x300, 0, 0x100), -1);                               /* outside the box: no handler needed */
    Slope s; memset(&s, 0, sizeof(s)); s.height = 0x40; s.ret = 1;
    v.user = &s; v.shape_handler = slope_handler; v.status = 0;
    CHECK_EQ(at(&v, 0x110, 0x10, 0xF0), 0);
    CHECK_EQ(s.shape, 0x12); CHECK_EQ(s.local[0], 0x10); CHECK_EQ(s.local[1], 0x10); CHECK_EQ(s.local[2], -0x10);
    CHECK_EQ(s.io_in, 0x20);                                             /* sp+0x28 pre-set to cy */
    CHECK_EQ(at(&v, 0x110, 0x20 - 0x40, 0xF0), 0); CHECK_EQ(at(&v, 0x110, 0x20 - 0x41, 0xF0), -1);   /* y >= cy - out[1] */
    CHECK_EQ(at(&v, 0x110, 0x21, 0xF0), -1);
    s.ret = 0; CHECK_EQ(at(&v, 0x110, 0x10, 0xF0), -1); s.ret = 1;
    /* shapes 0x40..0x43: out[1] = height - extra; the handler must write sp+0x2C */
    T[0][8] = 0x2041; s.write_extra = 1; s.extra = 0x10;
    CHECK_EQ(at(&v, 0x110, 0x20 - 0x30, 0xF0), 0); CHECK_EQ(at(&v, 0x110, 0x20 - 0x31, 0xF0), -1);
    CHECK_EQ(v.status, 0);
    s.write_extra = 0; CHECK_EQ(at(&v, 0x110, 0x10, 0xF0), -1); CHECK(v.status & AE_VOLUME_SLOPE_HANDLER);   /* returned 1, extra unwritten */
    /* handler returns 0 without writing sp+0x2C (CollShape_Ramp45 failed underside test, as for the eight area-78
       colliders 14..17 of tools/codex/check_cloud_volumes.py): BRC never reads out[], so it is a clean miss. */
    v.status = 0; s.ret = 0; CHECK_EQ(at(&v, 0x110, 0x10, 0xF0), -1); CHECK_EQ(v.status, 0);
    /* ... and a miss at the higher index must not hide a match at a lower index */
    v.collider_count = 2; memcpy(T[1], T[0], sizeof(T[0])); rec(T[0], 0x110, 0x10, 0xF0, 4, 4, 4, 0, 0, 0, 0, 0);
    CHECK_EQ(at(&v, 0x110, 0x10, 0xF0), 0); CHECK_EQ(v.status, 0);
    /* shape 0x42/0x43 and a non-0x40 shape returning 0 behave the same */
    v.collider_count = 1; rec(T[0], 0x100, 0x20, 0x100, 0x80, 0x100, 0x80, 0x2043, 0, 0, 0, 0); s.ret = 0;
    CHECK_EQ(at(&v, 0x110, 0x10, 0xF0), -1); CHECK_EQ(v.status, 0);
}

static void test_halfspace_typeactive_box(void) {
    int16_t r[28]; rec(r, 0x10, 0, -0x10, 0, 0, 0, 0x304E, 0, 0, 0, 0);
    int32_t a[3] = { 0, 0, -0x11 }, b[3] = { 0x11, 0, -0x10 };
    CHECK_EQ(ae_volume_halfspace_inside(r, a), 1); CHECK_EQ(ae_volume_halfspace_inside(r, b), 0);
    r[8] = 0x304F; CHECK_EQ(ae_volume_halfspace_inside(r, a), 1); CHECK_EQ(ae_volume_halfspace_inside(r, b), 0);
    r[8] = 0x3050; CHECK_EQ(ae_volume_halfspace_inside(r, a), 0); CHECK_EQ(ae_volume_halfspace_inside(r, b), 0);
    r[8] = 0x3051; CHECK_EQ(ae_volume_halfspace_inside(r, b), 1); CHECK_EQ(ae_volume_halfspace_inside(r, a), 0);
    r[8] = 0x3052; CHECK_EQ(ae_volume_halfspace_inside(r, a), 0);
    /* ColliderTypeActive */
    CHECK_EQ(ae_volume_type_active(1, 0x1000, 0), 1);
    CHECK_EQ(ae_volume_type_active(0, 0x1000, 0), 0); CHECK_EQ(ae_volume_type_active(0, 0x1000, -0x8000), 1);
    CHECK_EQ(ae_volume_type_active(0, 0x0000, 0), 1); CHECK_EQ(ae_volume_type_active(0, 0x2000, -0x8000), 1);
    CHECK_EQ(ae_volume_type_active(2, 0x0000, 0), 1); CHECK_EQ(ae_volume_type_active(2, 0x0000, -0x8000), 0);
    CHECK_EQ(ae_volume_type_active(2, 0x8000, -0x8000), 1); CHECK_EQ(ae_volume_type_active(2, 0x3051, 0), 0);
    CHECK_EQ(ae_volume_type_active(2, 0x3052, 0), 1); CHECK_EQ(ae_volume_type_active(0, 0x3036, 0), 0);
    /* PointInBoxXYZ: strict |d| < half + 0x200 */
    rec(r, 0, 0, 0, 0x10, 0x20, 0x30, 0, 0, 0, 0, 0);
    int32_t p1[3] = { 0x20F, -0x21F, 0x22F }, p2[3] = { -0x210, 0, 0 }, p3[3] = { 0, 0x220, 0 };
    CHECK_EQ(ae_volume_point_in_box(r, p1), 0); CHECK_EQ(ae_volume_point_in_box(r, p2), -1); CHECK_EQ(ae_volume_point_in_box(r, p3), -1);
}

/* ---------------------------------------------------------------- grid, probe points, ray test */
static int16_t G[256];
static int16_t S[2][10];
static void grid_setup(AeVolume *v) {
    for (int i = 0; i < 256; ++i) G[i] = -1;
    G[8 * 16 + 8] = 0;
    memset(S, 0, sizeof(S));
    S[0][0] = 8; S[0][1] = 0; S[0][2] = 8; S[0][3] = 0x100; S[0][4] = 0x200; S[0][5] = 0x100; S[0][7] = 0x21; S[0][8] = 1; S[0][9] = 0x0005;
    S[1][0] = 8; S[1][1] = 2; S[1][2] = 8; S[1][3] = 0x100; S[1][4] = 0x400; S[1][5] = 0x100; S[1][7] = 0x22; S[1][8] = -1; S[1][9] = 0x1005;
    v->coll_half_x = 0x1000; v->coll_half_z = 0x1000; v->coll_grid_w = 16;
    v->shape_grid = G; v->shape_grid_count = 256; v->shapes = (const int16_t (*)[10])S; v->shape_count = 2;
}
static int32_t pig(AeVolume *v, int x, int y, int z) { AeVolumeProbe e; memset(&e, 0, sizeof(e)); e.x = (int16_t)x; e.y = (int16_t)y; e.z = (int16_t)z; int ok = 1; return ae_volume_point_in_grid(v, &e, &ok); }

static void test_point_in_grid(void) {
    AeVolume v; ae_volume_init(&v); grid_setup(&v);
    CHECK_EQ(pig(&v, 0, 0, 0), 0);                                       /* cell (8, 8), level 0 */
    CHECK_EQ(pig(&v, 0, -0x200, 0), -1);                                 /* level 1: span 1 ends, next level 2 > 1 */
    CHECK_EQ(pig(&v, 0, -0x400, 0), 1); CHECK_EQ(pig(&v, 0x1FF, -0x600, 0x1FF), 1);   /* levels 2..3 */
    CHECK_EQ(pig(&v, 0, -0x800, 0), -1);
    CHECK_EQ(pig(&v, 0, 1, 0), -1);                                      /* y > 0 */
    CHECK_EQ(pig(&v, 0x200, 0, 0), -1); CHECK_EQ(pig(&v, -1, 0, 0), -1); /* neighbouring cells empty */
    CHECK_EQ(pig(&v, 0x1000, 0, 0), -1); CHECK_EQ(pig(&v, -0x1000, 0, 0), -1);   /* strict bounds */
    G[8 * 16 + 8] = 5; int ok = 1; AeVolumeProbe e; memset(&e, 0, sizeof(e));
    CHECK_EQ(ae_volume_point_in_grid(&v, &e, &ok), -1); CHECK(!ok); CHECK(v.status & AE_VOLUME_GRID_RANGE);
}

static void test_build_probes(void) {
    AeVolume v; ae_volume_init(&v); grid_setup(&v);
    int32_t pos[3] = { 0x10, -0x430, 0x180 };
    ae_volume_build_probes(&v, pos, 0x20);
    /* e0 y = -0x410 (vel.y > 0 added); quadrant: x rem 0x10 < 0x100 -> dx = -0x200, z rem 0x180 -> dz = +0x200 */
    const AeVolumeProbe *e = v.probes;
    CHECK_EQ(e[0].shape, 1); CHECK_EQ(e[0].x, 0x100); CHECK_EQ(e[0].y, -0x400); CHECK_EQ(e[0].z, 0x100);   /* snapped to the shape */
    CHECK_EQ(e[0].half[1], 0x400);
    CHECK_EQ(e[7].shape, 1);                                             /* top centre: y - 0x200 -> level 3 */
    CHECK_EQ(e[1].shape, -1); CHECK_EQ(e[1].x, 0x10); CHECK_EQ(e[1].y, -0x410); CHECK_EQ(e[1].z, 0x380);
    CHECK_EQ(e[2].x, -0x1F0); CHECK_EQ(e[2].z, 0x380); CHECK_EQ(e[3].x, -0x1F0); CHECK_EQ(e[3].z, 0x180);
    CHECK_EQ(e[4].x, 0x10); CHECK_EQ(e[4].y, -0x610); CHECK_EQ(e[4].z, 0x380);
    CHECK_EQ(e[5].x, -0x1F0); CHECK_EQ(e[5].z, 0x380); CHECK_EQ(e[6].x, -0x1F0); CHECK_EQ(e[6].z, 0x180);
    /* vel.y <= 0 is not added */
    int32_t pos2[3] = { 0x110, -0x430, 0x110 }; ae_volume_build_probes(&v, pos2, -0x20);
    CHECK_EQ(v.probes[1].z, 0x110 + 0x200); CHECK_EQ(v.probes[3].x, 0x110 + 0x200);   /* rem 0x110 >= 0x100: +0x200 */
    CHECK_EQ(v.probes[0].shape, 1);                                      /* y -0x430: level 2 */
}

typedef struct {
    int grid_calls, coll_calls; int grid_order[16], coll_order[16];
    int grid_hit_mask; int coll_hit_mask;
    int32_t ground_y; int16_t solid_kind, word; int word_known; int ground_calls; int32_t gq[3];
    const AeVolume *v;
    /* body world for the integration tests */
    int32_t floor_y, step_x, step_y; int16_t step_solid;
} Host;
static int32_t h_grid(void *u, const AeVolumeProbe *e, const int16_t s[10], const int32_t params[3], const int32_t point[3], int32_t out[6]) {
    Host *h = (Host *)u; (void)s; (void)params; (void)point; (void)out;
    int i = (int)(e - h->v->probes); h->grid_order[h->grid_calls++ & 15] = i;
    return (h->grid_hit_mask >> i) & 1;
}
static int32_t h_coll(void *u, const int16_t r[28], const int32_t params[3], const int32_t point[3], int32_t out[6]) {
    Host *h = (Host *)u; (void)point; (void)out;
    int i = r[13];                                                       /* test tag stored in an unused word */
    h->coll_order[h->coll_calls++ & 15] = i;
    CHECK_EQ(params[1], (uint16_t)r[8]);
    return (h->coll_hit_mask >> i) & 1;
}
static int32_t h_ground(void *u, int32_t x, int32_t y, int32_t z, AeVolumeGroundHit *hit) {
    Host *h = (Host *)u; h->ground_calls++; h->gq[0] = x; h->gq[1] = y; h->gq[2] = z;
    if (h->step_x) {                                                     /* integration world: x-step terrain */
        int ledge = x >= h->step_x;
        hit->hit.solid_kind = ledge ? h->step_solid : -1;
        hit->word_3fa_known = 1; hit->word_3fa = 0;
        return ledge ? h->step_y : h->floor_y;
    }
    hit->hit.solid_kind = h->solid_kind; hit->word_3fa = h->word; hit->word_3fa_known = (uint8_t)h->word_known;
    return h->ground_y;
}

static void test_ray(void) {
    AeVolume v; ae_volume_init(&v); Host h; memset(&h, 0, sizeof(h)); h.v = &v;
    v.user = &h; v.colliders = (const int16_t (*)[28])T; v.collider_count = 5;
    int32_t probe[5] = { 0x80, -0x100, 0, 0x40, 0x100 };
    int ok = 1;
    CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), 0); CHECK(!ok); CHECK(v.status & AE_VOLUME_RAY_NOT_PREPARED);
    int32_t body[3] = { 0, 0, 0 }; ae_volume_build_probes(&v, body, 0);
    ok = 1; ae_volume_ray(&v, 0x10000, probe, &ok); CHECK(!ok); CHECK(v.status & AE_VOLUME_RAY_MODE);
    /* colliders: 0 near, 1 far (masked), 2 near but disabled (masked), 3 near, 4 far but type 0xE (never masked) */
    rec(T[0], 0x100, 0, 0, 0x10, 0x10, 0x10, 0x0000, 0, 0, 0x8001, 0);
    rec(T[1], 0x2000, 0, 0, 0x10, 0x10, 0x10, 0x0000, 0, 0, 0x11, 0);
    rec(T[2], 0, 0, 0, 0x10, 0x10, 0x10, 0x0000, 0, 0, 0x12, 0x8000);
    rec(T[3], 0x1FF, -0x1FF, 0x1FF, 0, 0, 0, 0x1000, 0, 0x8000, 0x13, 0);
    rec(T[4], 0x3000, 0, 0, 0x10, 0x10, 0x10, 0x3000, 0x000E, 0, 0x14, 0);
    for (int i = 0; i < 5; ++i) T[i][13] = (int16_t)i;
    CHECK_EQ(ae_volume_collider_masked(&v, 0), 0); CHECK_EQ(ae_volume_collider_masked(&v, 1), 1);
    CHECK_EQ(ae_volume_collider_masked(&v, 2), 1); CHECK_EQ(ae_volume_collider_masked(&v, 3), 0);
    CHECK_EQ(ae_volume_collider_masked(&v, 4), 0);
    T[3][0] = 0x200; CHECK_EQ(ae_volume_collider_masked(&v, 3), 1); T[3][0] = 0x1FF;   /* strict |d| < half + 0x200 */
    v.status = 0; ok = 1;
    CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), 0); CHECK(!ok); CHECK(v.status & AE_VOLUME_COLLIDER_PROBE);
    v.collider_probe = h_coll; v.ground = h_ground; h.ground_y = 0x400; h.solid_kind = -1;
    /* no hit anywhere: GetGroundHeight(x, y - radius, z) below the point -> -1 (clear) */
    v.status = 0; ok = 1;
    CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), -1); CHECK(ok);
    CHECK_EQ(h.coll_calls, 3); CHECK_EQ(h.coll_order[0], 4); CHECK_EQ(h.coll_order[1], 3); CHECK_EQ(h.coll_order[2], 0);
    CHECK_EQ(h.gq[0], 0x80); CHECK_EQ(h.gq[1], -0x140); CHECK_EQ(h.gq[2], 0);
    /* highest unmasked index hit returns its +0x16 id (lhu): 3 before 0 */
    h.coll_hit_mask = (1 << 3) | (1 << 0); h.coll_calls = 0;
    CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), 0x13); CHECK_EQ(h.coll_calls, 2);
    h.coll_hit_mask = 1 << 0; CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), 0x8001);   /* zero-extended */
    /* ColliderTypeActive filter with g_CollWallProbe != 1: class 0x1000 needs mode bit 15 */
    h.coll_hit_mask = (1 << 3); v.wall_probe = 0; CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), 0x13);
    T[3][10] = 0; CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), -1); v.wall_probe = 1;
    CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), 0x13); T[3][10] = (int16_t)0x8000;
    h.coll_hit_mask = 0;
    /* ground fallback: point under the terrain (g < y - r) */
    h.ground_y = -0x200; h.solid_kind = 0;
    CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), -1);                      /* a solid: returns -1 */
    h.solid_kind = -1; h.word = 7; h.word_known = 1;
    CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), 7);
    h.word = -3; CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), -3);
    h.ground_y = -0x140; CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), -1);   /* not strictly above */
    h.ground_y = -0x141; h.word_known = 0; v.status = 0; ok = 1;
    CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), 0); CHECK(!ok); CHECK(v.status & AE_VOLUME_WORD_3FA);
    CHECK_EQ(ae_volume_ray_test(&v, 0, probe), 0);                       /* callback: fail closed = blocked */
    AeVolume nv = v; nv.ground = NULL; ok = 1; ae_volume_ray(&nv, 0, probe, &ok); CHECK(!ok); CHECK(nv.status & AE_VOLUME_NO_GROUND);
}

static void test_ray_grid(void) {
    AeVolume v; ae_volume_init(&v); grid_setup(&v); Host h; memset(&h, 0, sizeof(h)); h.v = &v; v.user = &h;
    v.ground = h_ground; h.ground_y = 0x400; h.solid_kind = -1;
    int32_t body[3] = { 0x10, -0x430, 0x180 }; ae_volume_build_probes(&v, body, 0x20);   /* entries 7 and 0 -> shape 1 */
    int32_t probe[5] = { 0, -0x500, 0, 0x40, 0x100 };
    int ok = 1;
    CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), 0); CHECK(!ok); CHECK(v.status & AE_VOLUME_GRID_PROBE);
    v.grid_probe = h_grid; v.status = 0; ok = 1;
    CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), -1); CHECK(ok);
    CHECK_EQ(h.grid_calls, 2); CHECK_EQ(h.grid_order[0], 7); CHECK_EQ(h.grid_order[1], 0);
    h.grid_hit_mask = 0xFF; CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), 0x22);
    S[1][7] = -5; h.ground_calls = 0;                                    /* negative grid id (lh): ground test still runs */
    CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), -5); CHECK_EQ(h.ground_calls, 1);   /* not underground: returns s5 = -5 */
    S[1][7] = 0x22;
    v.wall_probe = 0; CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), -1);   /* type 0x1005, flags bit 15 clear: inactive */
    S[1][6] = (int16_t)0x8000; CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), 0x22); S[1][6] = 0; v.wall_probe = 1;
    /* missing grid handler: a collider hit still decides (returns before the grid result is used) */
    v.grid_probe = NULL; v.colliders = (const int16_t (*)[28])T; v.collider_count = 1; v.collider_probe = h_coll;
    rec(T[0], 0x10, -0x430, 0x180, 8, 8, 8, 0, 0, 0, 0x30, 0); T[0][13] = 0; h.coll_hit_mask = 1;
    ok = 1; CHECK_EQ(ae_volume_ray(&v, 0, probe, &ok), 0x30); CHECK(ok);
}

/* ---------------------------------------------------------------- integration with the body pass */
static uint8_t *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    uint8_t *b = (uint8_t *)malloc((size_t)n); if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f); *size = (size_t)n; return b;
}
static void synthetic_surface(AePlayerData *d) {                         /* same synthetic table as test_body.c */
    for (int k = 0; k < 8; ++k) {
        uint8_t *r = d->surface[k].raw; memset(r, 0, 16);
        int32_t a = 0x3000, b = 0x6000; int16_t wall = 0x300, jump = 0x300, limit = 0x300;
        memcpy(r + 0, &a, 4); memcpy(r + 4, &b, 4); memcpy(r + 0xA, &wall, 2); memcpy(r + 0xC, &jump, 2); memcpy(r + 0xE, &limit, 2);
    }
    d->surface_loaded = 1;
}

/* One frame: ae_player_update fed from the same ground, then ae_player_body_step through the volume callbacks. */
static uint32_t frame(AePlayer *p, const AePlayerData *d, AePlayerBodyWorld *w, AeVolume *v, uint32_t f) {
    AePlayerInput in; memset(&in, 0, sizeof(in)); in.frame_counter = f;
    ae_player_stick_cam(p, d, in.cam_dir, in.stick_cam);
    AePlayerBodyGroundHit gh; memset(&gh, 0, sizeof(gh)); gh.solid_kind = -1;
    AePlayerGround g; memset(&g, 0, sizeof(g));
    g.ground_y = ae_volume_body_ground(v, p->pos[0], p->pos[1], p->pos[2], &gh);
    uint32_t u = ae_player_update(p, d, &in, &g);
    return u | (ae_player_body_step(p, w) << 16);
}

static void test_wired_ledge(const AePlayerData *d) {
    Host h; memset(&h, 0, sizeof(h)); h.floor_y = 0x400; h.step_x = 0x80; h.step_y = -0x20; h.step_solid = 0;
    AeVolume v; ae_volume_init(&v); h.v = &v;
    v.user = &h; v.ground = h_ground; v.collider_probe = h_coll;
    rec(T[0], 0x40, 0x60, 0x100, 0x40, 0x40, 0x40, 0x0000, 0, 0, 0x40, 0); T[0][13] = 0;   /* near, probe misses */
    rec(T[1], 0x3000, 0, 0x3000, 0x40, 0x40, 0x40, 0x0000, 0, 0, 0x41, 0); T[1][13] = 1;   /* far: masked */
    v.colliders = (const int16_t (*)[28])T; v.collider_count = 2;
    AePlayerBodyState s; ae_player_body_state_init(&s);
    AePlayerBodyWorld w; memset(&w, 0, sizeof(w));
    w.user = &v; w.ground = ae_volume_body_ground; w.walls = ae_volume_body_walls;
    w.ray_test = ae_volume_ray_test; w.collider_at = ae_volume_collider_at;
    w.data = d; w.state = &s; w.grid_width = 64; w.grid_depth = 64; w.current_level = 1; w.collider_count = v.collider_count;
    AePlayer p; int32_t pos[3] = { 0x40, 0xA0, 0 }; int16_t face[3] = { 0x1000, 0, 0 }; ae_player_init(&p, pos, face);
    uint32_t bits = 0; int f;
    for (f = 0; f < 12 && !s.ledge_grabbed; ++f) bits |= frame(&p, d, &w, &v, (uint32_t)f);
    CHECK_EQ(s.ledge_grabbed, 1); CHECK_EQ(p.state, 0x0E);
    CHECK_EQ(p.pos[0], 0xC0); CHECK_EQ(p.pos[1], -0x20); CHECK_EQ(p.vel[1], 0);
    CHECK_EQ(v.status, 0); CHECK_EQ(bits >> 16, 0);
    CHECK_EQ(v.last_ray, -1);                                            /* fallback: ledge-top terrain is a solid */
    CHECK(h.coll_calls >= 1); CHECK_EQ(h.coll_order[0], 0);              /* only the near collider was probed */
    /* the same frame with the near collider reporting a hit: the ray is blocked, no grab */
    ae_player_init(&p, pos, face); ae_player_body_state_init(&s); h.coll_hit_mask = 1;
    for (f = 0; f < 12; ++f) frame(&p, d, &w, &v, (uint32_t)f);
    CHECK_EQ(s.ledge_grabbed, 0); CHECK(p.state != 0x0E);
}

static void test_wired_collider(const AePlayerData *d) {
    Host h; memset(&h, 0, sizeof(h)); h.floor_y = 0; h.step_x = 0x7FFFFFFF; h.step_y = 0;
    AeVolume v; ae_volume_init(&v); h.v = &v; v.user = &h; v.ground = h_ground;
    rec(T[0], 0, 0x10, 0, 0x80, 0x100, 0x80, 0x0000, 0, 0, 0x50, 0x8000);       /* contains, disabled */
    rec(T[1], 0x400, 0, 0, 0x80, 0x100, 0x80, 0x3010, 0, 0, 0x51, 0);           /* cylinder elsewhere */
    rec(T[2], 0, 0x10, 0, 0x80, 0x100, 0x80, 0x0000, 0, 0, 0x52, 0);            /* contains */
    rec(T[3], 0, 0x10, 0, 0x80, 0x100, 0x80, 0x0000, 0x0009, 0, 0x53, 0);       /* water volume: skipped */
    v.colliders = (const int16_t (*)[28])T; v.collider_count = 4;
    AePlayerBodyState s; ae_player_body_state_init(&s);
    AePlayerBodyWorld w; memset(&w, 0, sizeof(w));
    w.user = &v; w.ground = ae_volume_body_ground; w.walls = ae_volume_body_walls;
    w.ray_test = ae_volume_ray_test; w.collider_at = ae_volume_collider_at;
    w.data = d; w.state = &s; w.grid_width = 64; w.grid_depth = 64; w.current_level = 1; w.collider_count = v.collider_count;
    AePlayer p; int32_t pos[3] = { 0, 0, 0 }; int16_t face[3] = { 0, 0, 0x1000 }; ae_player_init(&p, pos, face);
    uint32_t bits = frame(&p, d, &w, &v, 0);
    CHECK_EQ(bits, 0); CHECK_EQ(v.status, 0);
    CHECK_EQ(p.pos[1], 0); CHECK_EQ(s.collider_index, 2);
    CHECK(v.built); CHECK_EQ(v.build_pos[0], 0);                        /* walls adapter recorded CU:160's body */
    /* walk out of the box: index back to 0xFFFF on the next frame */
    p.pos[0] = 0x200; frame(&p, d, &w, &v, 1); CHECK_EQ(s.collider_index, 0xFFFF);
}

int main(int argc, char **argv) {
    test_sqrt(); test_box_and_priority(); test_cylinder(); test_halfpipe(); test_trench(); test_diagonal(); test_slope();
    test_halfspace_typeactive_box(); test_point_in_grid(); test_build_probes(); test_ray(); test_ray_grid();
    if (argc >= 2) {
        size_t size; uint8_t *blob = read_file(argv[1], &size);
        AePlayerData d; ae_player_data_init(&d);
        if (!blob || !ae_player_data_load_member(&d, blob, size)) { fprintf(stderr, "cannot load fixture\n"); return 2; }
        synthetic_surface(&d);
        test_wired_ledge(&d); test_wired_collider(&d);
        ae_player_data_free(&d); free(blob);
    } else {
        printf("volume: no member fixture given, integration tests skipped\n");
    }
    printf("volume: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
