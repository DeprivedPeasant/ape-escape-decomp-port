/* Trail point state aging and native quad-renderer boundary. */
#include "ae_effects.h"

enum {
    AE_TRAIL_DRAW_POINT_COUNT = 8,
    AE_TRAIL_DRAW_POINT_SIZE = 8,
    AE_TRAIL_DRAW_POSITION_X = 0,
    AE_TRAIL_DRAW_POSITION_Y = 2,
    AE_TRAIL_DRAW_POSITION_Z = 4,
    AE_TRAIL_DRAW_LIFE = 6,
    AE_TRAIL_DRAW_VERTEX_STRIDE = 8,
    AE_TRAIL_DRAW_VERTEX_COUNT = 4,
    AE_TRAIL_DRAW_VERTEX_X = 0,
    AE_TRAIL_DRAW_VERTEX_Y = 2,
    AE_TRAIL_DRAW_VERTEX_Z = 4,
    AE_TRAIL_DRAW_RADIUS_BASE = 24,
    AE_TRAIL_DRAW_RADIUS_PER_LIFE = 12
};

static void trail_points_call(AeRam *r, const AeEffectsHooks *hooks,
                              AeEffectsCallFn callback, const char *name,
                              uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    if (hooks == NULL || callback == NULL) {
        ae_ram_fail(r, "AeEffectsHooks.%s not provided", name);
        return;
    }
    (void)callback(hooks->user, r, a0, a1, a2, a3);
}

static void trail_points_write_vertex(AeRam *r, uint32_t vertex,
                                      uint32_t x, uint32_t y, uint32_t z) {
    ae_wr16(r, vertex + AE_TRAIL_DRAW_VERTEX_X, x);
    ae_wr16(r, vertex + AE_TRAIL_DRAW_VERTEX_Y, y);
    ae_wr16(r, vertex + AE_TRAIL_DRAW_VERTEX_Z, z);
}

static void trail_points_make_quad(AeRam *r, uint32_t point,
                                   uint32_t age, uint32_t geometry) {
    uint32_t radius = age * AE_TRAIL_DRAW_RADIUS_PER_LIFE + AE_TRAIL_DRAW_RADIUS_BASE;
    uint32_t x = ae_rd16(r, point + AE_TRAIL_DRAW_POSITION_X);
    uint32_t y = ae_rd16(r, point + AE_TRAIL_DRAW_POSITION_Y);
    uint32_t z = ae_rd16(r, point + AE_TRAIL_DRAW_POSITION_Z);

    trail_points_write_vertex(r, geometry,
                              x - radius, y, z - radius);
    trail_points_write_vertex(r, geometry + AE_TRAIL_DRAW_VERTEX_STRIDE,
                              x + radius, y, z - radius);
    trail_points_write_vertex(r, geometry + 2u * AE_TRAIL_DRAW_VERTEX_STRIDE,
                              x - radius, y, z + radius);
    trail_points_write_vertex(r, geometry + 3u * AE_TRAIL_DRAW_VERTEX_STRIDE,
                              x + radius, y, z + radius);
}

/* TrailPointsDraw @8006EF88 ages each active trail point, builds four input
 * vertices in scratchpad, then leaves projection, clipping, and packet writes
 * to the native renderer. */
void ae_TrailPointsDraw(AeRam *r, const AeEffectsHooks *hooks) {
    trail_points_call(r, hooks, hooks == NULL ? NULL : hooks->SetRotMatrix,
                      "SetRotMatrix", AE_G_EFFECTS_CAMERA_MATRIX, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    trail_points_call(r, hooks, hooks == NULL ? NULL : hooks->SetTransMatrix,
                      "SetTransMatrix", AE_G_EFFECTS_CAMERA_MATRIX, 0, 0, 0);
    if (ae_ram_failed(r)) return;

    for (int32_t index = AE_TRAIL_DRAW_POINT_COUNT - 1; index >= 0; --index) {
        uint32_t point = AE_G_TRAIL_POINTS + (uint32_t)index * AE_TRAIL_DRAW_POINT_SIZE;
        uint32_t age = ae_rd8(r, point + AE_TRAIL_DRAW_LIFE);
        if (age == 0u) continue;

        ae_wr8(r, point + AE_TRAIL_DRAW_LIFE, age - 1u);
        trail_points_make_quad(r, point, age, AE_G_TRAIL_POINT_RENDER_VERTICES);
        trail_points_call(r, hooks,
                          hooks == NULL ? NULL : hooks->GpuDrawTrailPoint,
                          "GpuDrawTrailPoint", point, age,
                          AE_G_TRAIL_POINT_RENDER_VERTICES, (uint32_t)index);
        if (ae_ram_failed(r)) return;
    }
}
