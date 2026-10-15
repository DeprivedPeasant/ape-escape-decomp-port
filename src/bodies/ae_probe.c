/* Collision probe support, hand-ported from the original MIPS routines. */
#include "ae_probe.h"

#include <stdint.h>

#define AE_PROBE_GRID_CELL_SIZE 512u
#define AE_PROBE_GRID_HALF_CELL 256
#define AE_PROBE_BODY_GROUND_STEP 32
#define AE_PROBE_BODY_GROUND_MIN_VELOCITY 32
#define AE_PROBE_COLLIDER_DISABLED 0x8000u
#define AE_PROBE_COLLIDER_SKIP_TYPE 14

static int32_t probe_s16(uint32_t value) {
    value &= 0xFFFFu;
    return value < 0x8000u ? (int32_t)value : (int32_t)value - 0x10000;
}

static int32_t probe_wrap_add(int32_t left, int32_t right) {
    return (int32_t)((uint32_t)left + (uint32_t)right);
}

static int32_t probe_wrap_sub(int32_t left, int32_t right) {
    return (int32_t)((uint32_t)left - (uint32_t)right);
}

/* R3000 arithmetic right shift, expressed without relying on negative >> behavior. */
static int32_t probe_sra(int32_t value, uint32_t shift) {
    if (shift == 0) return value;
    uint32_t shifted = (uint32_t)value >> shift;
    if (value < 0) shifted |= 0xFFFFFFFFu << (32u - shift);
    return (int32_t)shifted;
}

static int32_t probe_round_cell(int32_t coordinate) {
    int32_t rounded = coordinate;
    if (coordinate < 0) rounded = probe_wrap_add(rounded, (int32_t)AE_PROBE_GRID_CELL_SIZE - 1);
    return probe_sra(rounded, 9);
}

static int probe_axis_inside(AeRam *r, uint32_t box, uint32_t point,
                             uint32_t box_center_offset, uint32_t box_half_offset,
                             uint32_t point_offset) {
    int32_t center = ae_rds16(r, box + box_center_offset);
    int32_t half_extent = ae_rds16(r, box + box_half_offset);
    int32_t position = ae_rds32(r, point + point_offset);
    int32_t difference = probe_wrap_sub(center, position);
    int32_t expanded_half = probe_wrap_add(half_extent, AE_PROBE_GRID_CELL_SIZE);
    if (difference < 0) difference = probe_wrap_sub(position, center);
    return difference < expanded_half;
}

/* 800733DC: shift four of the eight probe positions by the selected cell corner. */
int32_t ae_CollisionShiftProbePoints(AeRam *r, int32_t dx, int32_t dz) {
    uint32_t probes = ae_rd32(r, AE_G_COLL_PROBE_POINTS);

    ae_wr16(r, probes + AE_PROBE_SHIFT_Z_PROBE_1,
            ae_rd16(r, probes + AE_PROBE_SHIFT_Z_PROBE_1) + (uint32_t)dz);
    ae_wr16(r, probes + AE_PROBE_SHIFT_Z_PROBE_2,
            ae_rd16(r, probes + AE_PROBE_SHIFT_Z_PROBE_2) + (uint32_t)dz);
    ae_wr16(r, probes + AE_PROBE_SHIFT_X_PROBE_2,
            ae_rd16(r, probes + AE_PROBE_SHIFT_X_PROBE_2) + (uint32_t)dx);
    ae_wr16(r, probes + AE_PROBE_SHIFT_X_PROBE_3,
            ae_rd16(r, probes + AE_PROBE_SHIFT_X_PROBE_3) + (uint32_t)dx);
    ae_wr16(r, probes + AE_PROBE_SHIFT_Z_PROBE_4,
            ae_rd16(r, probes + AE_PROBE_SHIFT_Z_PROBE_4) + (uint32_t)dz);
    ae_wr16(r, probes + AE_PROBE_SHIFT_Z_PROBE_5,
            ae_rd16(r, probes + AE_PROBE_SHIFT_Z_PROBE_5) + (uint32_t)dz);
    ae_wr16(r, probes + AE_PROBE_SHIFT_X_PROBE_5,
            ae_rd16(r, probes + AE_PROBE_SHIFT_X_PROBE_5) + (uint32_t)dx);
    ae_wr16(r, probes + AE_PROBE_SHIFT_X_PROBE_6,
            ae_rd16(r, probes + AE_PROBE_SHIFT_X_PROBE_6) + (uint32_t)dx);
    return 0;
}

/* 80073AA0: strict three-axis overlap with each half extent expanded by 0x200. */
int32_t ae_PointInBoxXYZ(AeRam *r, uint32_t box, uint32_t point) {
    if (!probe_axis_inside(r, box, point, AE_PROBE_COLLIDER_X, AE_PROBE_COLLIDER_HALF_X,
                           AE_PROBE_BODY_X)) return -1;
    if (!probe_axis_inside(r, box, point, AE_PROBE_COLLIDER_Y, AE_PROBE_COLLIDER_HALF_Y,
                           AE_PROBE_BODY_Y)) return -1;
    if (!probe_axis_inside(r, box, point, AE_PROBE_COLLIDER_Z, AE_PROBE_COLLIDER_HALF_Z,
                           AE_PROBE_BODY_Z)) return -1;
    return 0;
}

/* 8004F6E8: choose the shape band intersecting the signed-halfword probe point. */
uint32_t ae_CollisionPointInGrid(AeRam *r, uint32_t point) {
    int32_t x = ae_rds16(r, point + AE_PROBE_X);
    int32_t y = ae_rds16(r, point + AE_PROBE_Y);
    int32_t z = ae_rds16(r, point + AE_PROBE_Z);
    int32_t half_x = ae_rds32(r, AE_G_COLL_HALF_X);
    int32_t half_z = ae_rds32(r, AE_G_COLL_HALF_Z);

    if (!(probe_wrap_sub(0, half_x) < x && x < half_x)) return 0;
    if (!(probe_wrap_sub(0, half_z) < z && z < half_z)) return 0;
    if (y > 0) return 0;

    uint32_t shape_base = ae_rd32(r, AE_G_COLL_SHAPES);
    uint32_t grid_base = ae_rd32(r, AE_G_COLL_SHAPE_GRID);
    ae_wr32(r, AE_G_COLL_CURRENT_SHAPES, shape_base);
    ae_wr32(r, AE_G_COLL_CURRENT_GRID, grid_base);

    int32_t cell_x = probe_round_cell(probe_wrap_add(x, half_x));
    int32_t cell_z = probe_round_cell(probe_wrap_add(z, half_z));
    int32_t height_from_origin = probe_wrap_sub(0, y);
    int32_t query_band = probe_round_cell(height_from_origin);
    uint32_t grid_width = ae_rd32(r, AE_G_COLL_GRID_WIDTH);
    uint32_t cell_index = grid_width * (uint32_t)cell_z + (uint32_t)cell_x;
    int32_t shape_index = ae_rds16(r, grid_base + cell_index * 2u);
    if (shape_index < 0) return 0;

    uint32_t shape = shape_base + (uint32_t)shape_index * AE_PROBE_SHAPE_SIZE;
    for (;;) {
        int32_t shape_band = ae_rds16(r, shape + AE_PROBE_SHAPE_Y_BAND);
        if (query_band < shape_band) return 0;

        int32_t shape_height = ae_rds16(r, shape + AE_PROBE_SHAPE_HALF_Y);
        int32_t rounded_height = shape_height;
        if (shape_height < 0) rounded_height = probe_wrap_add(rounded_height, 511);
        int32_t top_band = probe_wrap_sub(probe_wrap_add(shape_band, probe_sra(rounded_height, 9)), 1);
        if (top_band >= query_band) return shape;

        int32_t next_index = ae_rds16(r, shape + AE_PROBE_SHAPE_NEXT);
        if (next_index < 0) return 0;
        shape = shape_base + (uint32_t)next_index * AE_PROBE_SHAPE_SIZE;
        if (query_band == ae_rds16(r, shape + AE_PROBE_SHAPE_Y_BAND)) return shape;
    }
}

/* 8007304C: build the eight body probes, resolve each grid band, then mark overlapping colliders. */
int32_t ae_CollisionBuildProbePoints(AeRam *r, int32_t body_type, uint32_t body) {
    uint32_t probes = ae_rd32(r, AE_G_COLL_PROBE_POINTS);

    ae_wr16(r, probes + AE_PROBE_X, ae_rd16(r, body + AE_PROBE_BODY_X));
    ae_wr16(r, probes + AE_PROBE_Y, ae_rd16(r, body + AE_PROBE_BODY_Y));
    int32_t velocity_y = ae_rds32(r, body + AE_PROBE_BODY_VELOCITY_Y);
    if (velocity_y > 0) {
        ae_wr16(r, probes + AE_PROBE_Y,
                ae_rd16(r, probes + AE_PROBE_Y) + ae_rd16(r, body + AE_PROBE_BODY_VELOCITY_Y));
    }
    ae_wr16(r, probes + AE_PROBE_Z, ae_rd16(r, body + AE_PROBE_BODY_Z));

    for (int32_t index = 1; index < 8; ++index) {
        uint32_t probe = probes + (uint32_t)index * AE_PROBE_RECORD_SIZE;
        ae_wr16(r, probe + AE_PROBE_X, ae_rd16(r, probes + AE_PROBE_X));
        uint32_t probe_y = ae_rd16(r, probes + AE_PROBE_Y);
        if (index >= 4) probe_y -= AE_PROBE_GRID_CELL_SIZE;
        ae_wr16(r, probe + AE_PROBE_Y, probe_y);
        ae_wr16(r, probe + AE_PROBE_Z, ae_rd16(r, probes + AE_PROBE_Z));
    }

    int32_t x_from_grid_center = probe_wrap_add(ae_rds32(r, body + AE_PROBE_BODY_X),
                                                ae_rds32(r, AE_G_COLL_HALF_X));
    int32_t z_from_grid_center = probe_wrap_add(ae_rds32(r, body + AE_PROBE_BODY_Z),
                                                ae_rds32(r, AE_G_COLL_HALF_Z));
    int32_t x_cell = probe_round_cell(x_from_grid_center);
    int32_t z_cell = probe_round_cell(z_from_grid_center);
    int32_t x_remainder = probe_wrap_sub(x_from_grid_center,
                                         (int32_t)((uint32_t)x_cell << 9));
    int32_t z_remainder = probe_wrap_sub(z_from_grid_center,
                                         (int32_t)((uint32_t)z_cell << 9));
    uint32_t corner = ((uint32_t)probe_sra(probe_wrap_sub(x_remainder, AE_PROBE_GRID_HALF_CELL), 31) & 2u)
                    | ((uint32_t)probe_wrap_sub(z_remainder, AE_PROBE_GRID_HALF_CELL) >> 31);

    int32_t shift_x, shift_z;
    switch (corner) {
    case 0: shift_x = 512; shift_z = 512; break;
    case 1: shift_x = 512; shift_z = -512; break;
    case 2: shift_x = -512; shift_z = 512; break;
    default: shift_x = -512; shift_z = -512; break;
    }
    ae_CollisionShiftProbePoints(r, shift_x, shift_z);

    int32_t probe_index = 7;
    while (probe_index >= 0) {
        uint32_t probe = probes + (uint32_t)probe_index * AE_PROBE_RECORD_SIZE;
        uint32_t shape = ae_CollisionPointInGrid(r, probe);
        ae_wr32(r, probe + AE_PROBE_SHAPE, shape);
        if (shape != 0) {
            ae_wr16(r, probe + AE_PROBE_HALF_X, ae_rd16(r, shape + AE_PROBE_SHAPE_HALF_X));
            ae_wr16(r, probe + AE_PROBE_HALF_Y, ae_rd16(r, shape + AE_PROBE_SHAPE_HALF_Y));
            ae_wr16(r, probe + AE_PROBE_HALF_Z, ae_rd16(r, shape + AE_PROBE_SHAPE_HALF_Z));

            int32_t shape_x = ae_rds16(r, shape + AE_PROBE_SHAPE_X_CELL);
            int32_t shape_y = ae_rds16(r, shape + AE_PROBE_SHAPE_Y_BAND);
            int32_t shape_z = ae_rds16(r, shape + AE_PROBE_SHAPE_Z_CELL);
            int32_t world_x = probe_wrap_add(
                probe_wrap_sub((int32_t)((uint32_t)shape_x << 9), ae_rd16(r, AE_G_COLL_HALF_X)),
                AE_PROBE_GRID_HALF_CELL);
            int32_t world_y = probe_wrap_sub(0, (int32_t)((uint32_t)shape_y << 9));
            int32_t world_z = probe_wrap_add(
                probe_wrap_sub((int32_t)((uint32_t)shape_z << 9), ae_rd16(r, AE_G_COLL_HALF_Z)),
                AE_PROBE_GRID_HALF_CELL);
            ae_wr16(r, probe + AE_PROBE_X, (uint32_t)world_x);
            ae_wr16(r, probe + AE_PROBE_Y, (uint32_t)world_y);
            ae_wr16(r, probe + AE_PROBE_Z, (uint32_t)world_z);
        }
        --probe_index;
    }

    int32_t collider_index = probe_s16(ae_rd16(r, AE_G_COLLIDER_COUNT) - 1u);
    int32_t body_kind = probe_s16((uint32_t)body_type);
    while (collider_index >= 0) {
        uint32_t visited = ae_rd32(r, AE_G_COLL_PROBE_FLAGS);
        uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
        uint32_t collider = colliders + (uint32_t)collider_index * AE_PROBE_COLLIDER_SIZE;
        ae_wr8(r, visited + (uint32_t)collider_index, 0);

        if ((ae_rd16(r, collider + AE_PROBE_COLLIDER_FLAGS) & AE_PROBE_COLLIDER_DISABLED) != 0) {
            ae_wr8(r, visited + (uint32_t)collider_index, 1);
        } else {
            uint32_t collider_type = ae_rd16(r, collider + AE_PROBE_COLLIDER_TYPE);
            if ((int32_t)collider_type != AE_PROBE_COLLIDER_SKIP_TYPE) {
                if (body_kind == 1 && (int32_t)collider_type == body_kind)
                    ae_wr16(r, collider + AE_PROBE_COLLIDER_BODY_MARKER, 0xFFFFu);
                uint32_t current_colliders = ae_rd32(r, AE_G_COLLIDERS);
                uint32_t box = current_colliders + (uint32_t)collider_index * AE_PROBE_COLLIDER_SIZE;
                if (ae_PointInBoxXYZ(r, box, body) != 0) {
                    visited = ae_rd32(r, AE_G_COLL_PROBE_FLAGS);
                    ae_wr8(r, visited + (uint32_t)collider_index, 1);
                }
            }
        }
        collider_index = probe_s16((uint32_t)collider_index - 1u);
    }
    return 0;
}

/* 80072D28: step upward by 0x20 and stop at the first ground height below the probe. */
int32_t ae_BodyProbeGround(AeRam *r, const AeWorldHooks *world, uint32_t body) {
    int32_t probe_height = AE_PROBE_BODY_GROUND_STEP;
    if (probe_wrap_sub(ae_rds32(r, body + AE_PROBE_BODY_VELOCITY_Y),
                       AE_PROBE_BODY_GROUND_MIN_VELOCITY) < 0) return 0;

    for (;;) {
        int32_t x = probe_wrap_add(ae_rds32(r, body + AE_PROBE_BODY_X),
                                   ae_rds32(r, body + AE_PROBE_BODY_VELOCITY_X));
        int32_t y = probe_wrap_add(ae_rds32(r, body + AE_PROBE_BODY_Y), probe_height);
        int32_t z = probe_wrap_add(ae_rds32(r, body + AE_PROBE_BODY_Z),
                                   ae_rds32(r, body + AE_PROBE_BODY_VELOCITY_Z));
        int32_t ground = ae_GetGroundHeight(r, world, x, y, z);
        if (ground < y) {
            ae_wr32(r, body + AE_PROBE_BODY_VELOCITY_Y, (uint32_t)probe_height);
            return 0;
        }

        probe_height = probe_wrap_add(probe_height, AE_PROBE_BODY_GROUND_STEP);
        if (probe_wrap_sub(ae_rds32(r, body + AE_PROBE_BODY_VELOCITY_Y), probe_height) < 0) return 0;
    }
}
