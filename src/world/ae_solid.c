/* Solid surface queries, hand ported from SCUS-94423; see notes/port/solid.md. */
#include "ae_solid.h"
#include "../core/ae_sdk.h"
#include "../player/native_player.h"
#include <limits.h>

static int32_t add32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static int32_t sub32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static int32_t abs32(int32_t value) { return value < 0 ? sub32(0, value) : value; }
static int32_t mul32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
static int32_t divide(AeRam *r, int32_t numerator, int32_t denominator) {
    if (!denominator || (numerator == INT_MIN && denominator == -1)) {
        ae_ram_fail(r, "solid height division trap");
        return 0;
    }
    return numerator / denominator;
}
static void slopes(AeRam *r, int32_t x, int32_t z) {
    ae_wr32(r, AE_COLL_SLOPE_X, x);
    ae_wr32(r, AE_COLL_SLOPE_Z, z);
}
static int inside_shape(AeRam *r, uint32_t shape, uint32_t point) {
    int32_t ex = ae_rds16(r, shape + AE_SHAPE_EXTENT_X);
    int32_t x = ae_rds32(r, point);
    if (x < -ex || ex < x) return 0;
    int32_t ez = ae_rds16(r, shape + AE_SHAPE_EXTENT_Z);
    int32_t z = ae_rds32(r, point + 8);
    return z >= -ez && z <= ez;
}

/* 80071D20: the counter wraps, and the function always returns zero. */
int32_t ae_CollProbeCounterInc(AeRam *r) {
    ae_wr32(r, AE_COLL_PROBE_COUNT, ae_rd32(r, AE_COLL_PROBE_COUNT) + 1u);
    return 0;
}

/* 800889B4: quadrant of the signed low halfwords, including zero on the positive side. */
int32_t ae_GetColliderQuadrantMaskXZ(AeRam *r, int32_t x, int32_t z) {
    (void)r;
    if ((int16_t)x < 0) return (int16_t)z < 0 ? 1 : 4096;
    return (int16_t)z < 0 ? 16 : 256;
}

/* 8004FE48: outer corner of a slope; other types leave the output untouched. */
int32_t ae_ColliderSlopeCorner(AeRam *r, uint32_t collider, uint32_t out) {
    uint32_t type = ae_rd8(r, collider + AE_COL_TYPE);
    if (type < 0x46 || type > 0x49) return 0;
    int32_t x = ae_rds16(r, collider + AE_COL_X);
    int32_t z = ae_rds16(r, collider + AE_COL_Z);
    int32_t extent_x = ae_rds16(r, collider + AE_COL_EXTENT_X);
    int32_t extent_z = ae_rds16(r, collider + AE_COL_EXTENT_Z);
    ae_wr32(r, out, type < 0x48 ? x + extent_x : x - extent_x);
    ae_wr32(r, out + 4, type == 0x46 || type == 0x49 ? z + extent_z : z - extent_z);
    return 0;
}

/* 8004FC8C: rectangle expanded only on the two sides adjoining the sloping edge. */
int32_t ae_ColliderSlopeSide(AeRam *r, uint32_t collider, uint32_t point, int32_t radius, uint32_t angles) {
    int32_t extent_x = ae_rds16(r, collider + AE_COL_EXTENT_X);
    int32_t extent_z = ae_rds16(r, collider + AE_COL_EXTENT_Z);
    int32_t expanded_x = add32(extent_x, radius);
    int32_t expanded_z = add32(extent_z, radius);
    uint32_t type = ae_rd8(r, collider + AE_COL_TYPE);
    if (type < 0x46 || type > 0x49) return 0;
    int32_t x = ae_rds16(r, collider + AE_COL_X);
    int32_t z = ae_rds16(r, collider + AE_COL_Z);
    int32_t px = ae_rds32(r, point);
    int32_t pz = ae_rds32(r, point + 8);
    int32_t min_x = sub32(x, type < 0x48 ? expanded_x : extent_x);
    int32_t max_x = add32(x, type < 0x48 ? extent_x : expanded_x);
    int32_t min_z = sub32(z, type == 0x46 || type == 0x49 ? expanded_z : extent_z);
    int32_t max_z = add32(z, type == 0x46 || type == 0x49 ? extent_z : expanded_z);
    if (max_x < px || px < min_x || max_z < pz || pz < min_z) return -1;
    switch (type) {
    case 0x46: ae_wr16(r, angles, 2048); ae_wr16(r, angles + 2, 3072); break;
    case 0x47: ae_wr16(r, angles, 1024); ae_wr16(r, angles + 2, 2048); break;
    case 0x48: ae_wr16(r, angles, 0); ae_wr16(r, angles + 2, 1024); break;
    case 0x49: ae_wr16(r, angles, 3072); ae_wr16(r, angles + 2, 0); break;
    }
    return 0;
}

/* 8004FF20: strict half spaces; points on the plane are outside. */
int32_t ae_ColliderHalfSpaceInside(AeRam *r, uint32_t collider, uint32_t point) {
    switch (ae_rd8(r, collider + AE_COL_TYPE)) {
    case 0x4E: return ae_rds32(r, point + 8) < ae_rds16(r, collider + AE_COL_Z);
    case 0x4F: return ae_rds32(r, point) < ae_rds16(r, collider + AE_COL_X);
    case 0x50: return ae_rds16(r, collider + AE_COL_Z) < ae_rds32(r, point + 8);
    case 0x51: return ae_rds16(r, collider + AE_COL_X) < ae_rds32(r, point);
    default: return 0;
    }
}

/* 80079A28: select solid versus wall queries using the record's material class. */
int32_t ae_ColliderTypeActive(AeRam *r, uint32_t type, int32_t enabled) {
    int32_t probe = ae_rds16(r, AE_COLL_WALL_PROBE);
    if (probe == 1) return 1;
    uint32_t material = type & 0xF000u;
    uint32_t shape = type & 0xFFu;
    int special = material == 0x1000 || material == 0x5000 || material == 0x6000 ||
                  material == 0x7000 || material == 0x8000 ||
                  (material == 0x3000 && ((shape >= 78 && shape < 82) || shape == 54));
    if (probe == 0 && !special) return 1;
    if (special) return (int16_t)enabled != 0;
    return (int16_t)enabled == 0;
}

/* 8004F548: a narrow strip with linear height along its long axis. */
int32_t ae_ColliderStripHeight(AeRam *r, uint32_t collider, uint32_t point) {
    int32_t center_x = ae_rds16(r, collider + AE_COL_X);
    int32_t x = ae_rds32(r, point);
    int32_t center_z = ae_rds16(r, collider + AE_COL_Z);
    int32_t z = ae_rds32(r, point + 8);
    int32_t dx = sub32(x, center_x), dz = sub32(z, center_z);
    uint32_t type = ae_rd8(r, collider + AE_COL_TYPE);
    int32_t extent = ae_rds16(r, collider + AE_COL_EXTENT_X);
    int32_t distance;
    if (type == 0x4A || type == 0x4C) {
        if (extent < abs32(dx) || 72 < abs32(dz)) return 0;
        distance = ae_rds16(r, collider + AE_COL_STRIP_DIRECTION) == 0 ?
            sub32(add32(center_x, extent), x) : sub32(x, sub32(center_x, extent));
    } else {
        if (extent < abs32(dz) || 72 < abs32(dx)) return 0;
        distance = ae_rds16(r, collider + AE_COL_STRIP_DIRECTION) == 90 ?
            sub32(add32(center_z, extent), z) : sub32(z, sub32(center_z, extent));
    }
    int32_t base = 2 * ae_rds16(r, collider + AE_COL_EXTENT_Z);
    int32_t rise = ae_rds16(r, collider + AE_COL_EXTENT_Y) - base;
    int32_t interpolated = divide(r, mul32(rise, distance), 2 * extent);
    return ae_ram_failed(r) ? 0 : add32(base, interpolated);
}

/* 8004F514: clearing out before the query also matters when out aliases input. */
int32_t ae_ColliderStripHeightOut(AeRam *r, uint32_t collider, uint32_t point, uint32_t out) {
    ae_wr32(r, out, 0);
    int32_t height = ae_ColliderStripHeight(r, collider, point);
    if (ae_ram_failed(r)) return 0;
    ae_wr32(r, out, height);
    return height != 0;
}

/* 8004E33C: the four offset boxes use narrowed signed offsets. */
int32_t ae_CollShape_Box(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out) {
    type = (int16_t)type;
    ae_wr32(r, out, 0);
    int32_t offset_x = 0, offset_z = 0;
    switch (type) {
    case 60: offset_z = (int16_t)(-ae_rd16(r, shape + AE_SHAPE_EXTENT_Z)); break;
    case 61: offset_x = (int16_t)(-ae_rd16(r, shape + AE_SHAPE_EXTENT_X)); break;
    case 62: offset_z = ae_rds16(r, shape + AE_SHAPE_EXTENT_Z); break;
    case 63: offset_x = ae_rds16(r, shape + AE_SHAPE_EXTENT_X); break;
    }
    int32_t ex = ae_rds16(r, shape + AE_SHAPE_EXTENT_X);
    int32_t x = ae_rds32(r, point);
    if (x < offset_x - ex || offset_x + ex < x) return 0;
    int32_t ez = ae_rds16(r, shape + AE_SHAPE_EXTENT_Z);
    int32_t z = ae_rds32(r, point + 8);
    if (z < offset_z - ez || offset_z + ez < z || !ex || !ez) return 0;
    ae_wr32(r, out, ae_rds16(r, shape + AE_SHAPE_HEIGHT));
    slopes(r, 0, 0);
    return 1;
}

/* 8004ED30: 1:2 ramps. Outside the rectangle the output remains untouched. */
int32_t ae_CollShape_Ramp2(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out) {
    if (!inside_shape(r, shape, point)) return 0;
    ae_wr32(r, out, 0);
    int32_t index = (int16_t)sub32(type, 11);
    if ((uint32_t)index >= 8) return 1;
    int32_t height, x_slope = 0, z_slope = 0;
    int32_t scaled_type = (int32_t)((uint32_t)type << 16) >> 8;
    if (index < 2) {
        height = add32(ae_rds32(r, point + 8) / 2, scaled_type - 2688); z_slope = -256;
    } else if (index < 4) {
        height = sub32(scaled_type - 3200, ae_rds32(r, point) / 2); x_slope = 256;
    } else if (index < 6) {
        height = sub32(scaled_type - 3712, ae_rds32(r, point + 8) / 2); z_slope = 256;
    } else {
        height = add32(ae_rds32(r, point) / 2, scaled_type - 4224); x_slope = -256;
    }
    ae_wr32(r, out, height); slopes(r, x_slope, z_slope);
    return 1;
}

/* 8004EE94: 1:4 ramps clear the output even outside the rectangle. */
int32_t ae_CollShape_Ramp4(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out) {
    ae_wr32(r, out, 0);
    if (!inside_shape(r, shape, point)) return 0;
    int32_t index = (int16_t)sub32(type, 19);
    if ((uint32_t)index >= 16) return 1;
    int32_t height, x_slope = 0, z_slope = 0;
    int32_t scaled_type = (int32_t)((uint32_t)type << 16) >> 9;
    if (index < 4) {
        height = add32(ae_rds32(r, point + 8) / 4, scaled_type - 2368); z_slope = -128;
    } else if (index < 8) {
        height = sub32(scaled_type - 2880, ae_rds32(r, point) / 4); x_slope = 128;
    } else if (index < 12) {
        height = sub32(scaled_type - 3392, ae_rds32(r, point + 8) / 4); z_slope = 128;
    } else {
        height = add32(ae_rds32(r, point) / 4, scaled_type - 3904); x_slope = -128;
    }
    ae_wr32(r, out, height); slopes(r, x_slope, z_slope);
    return 1;
}

/* 8004EA1C: ordinary 45-degree ramps and the four underside wall variants. */
int32_t ae_CollShape_Ramp45(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out) {
    int32_t previous_height = ae_rds32(r, out);
    ae_wr32(r, out, 0);
    if (!inside_shape(r, shape, point)) return 0;
    int32_t index = (int16_t)sub32(type, 7);
    if ((uint32_t)index >= 61) return 1;
    int32_t along;
    switch (index) {
    case 0: along = ae_rds32(r, point + 8); break;
    case 1: along = sub32(0, ae_rds32(r, point)); break;
    case 2: along = sub32(0, ae_rds32(r, point + 8)); break;
    case 3: along = ae_rds32(r, point); break;
    case 57: case 58: case 59: case 60:
        along = ae_rds32(r, point + (index == 57 || index == 59 ? 8 : 0));
        if (index == 58 || index == 59) along = sub32(0, along);
        if (ae_rds32(r, point + 4) >= sub32(sub32(previous_height, 256), along)) return 0;
        ae_wr32(r, out, ae_rds16(r, shape + AE_SHAPE_HEIGHT));
        slopes(r, 0, 0);
        ae_wr32(r, out + 4, add32(along, 256));
        return 1;
    default: return 1;
    }
    ae_wr32(r, out, add32(along, 256));
    slopes(r, index == 1 ? 512 : index == 3 ? -512 : 0,
              index == 0 ? -512 : index == 2 ? 512 : 0);
    return 1;
}

/* 8004F000: a wedge rises from zero to its height across a full box width. */
int32_t ae_CollShape_Wedge(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out) {
    if (!inside_shape(r, shape, point)) return 0;
    ae_wr32(r, out, 0);
    int32_t ex = ae_rds16(r, shape), ez = ae_rds16(r, shape + AE_SHAPE_EXTENT_Z);
    if (ex <= 64 || ez <= 64) {
        ae_wr32(r, out, ae_rds16(r, shape + AE_SHAPE_HEIGHT)); slopes(r, 0, 0); return 1;
    }
    type = (int16_t)type;
    if (type < 85 || type > 88) return 1;
    int along_x = type == 86 || type == 88;
    int descending = type == 86 || type == 87;
    int32_t height = ae_rds16(r, shape + AE_SHAPE_HEIGHT);
    /* Half the height rounds toward zero: (height - sign) >> 1. */
    int32_t midpoint = (height - (height < 0 ? -1 : 0)) >> 1;
    int32_t extent = along_x ? ex : ez;
    int32_t along = ae_rds32(r, point + (along_x ? 0 : 8));
    int32_t rise = divide(r, mul32(midpoint, along), extent);
    ae_wr32(r, out, descending ? sub32(midpoint, rise) : add32(midpoint, rise));
    int32_t slope = divide(r, mul32(descending ? height : -height, 512), extent * 2);
    slopes(r, along_x ? slope : 0, along_x ? 0 : slope);
    return 1;
}

/* 8004FFC8: square-root arc, retaining the slope of its axial direction. */
int32_t ae_CollHalfPipeHeight(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out) {
    ae_wr32(r, out, 0);
    int32_t z = ae_rds32(r, point + 8), ez = ae_rds16(r, shape + AE_SHAPE_EXTENT_Z);
    if (ez < abs32(z)) return 0;
    int32_t x = ae_rds32(r, point), ex = ae_rds16(r, shape);
    if (ex < abs32(x)) return 0;
    int along_z = (int16_t)type == 36;
    int32_t radius = along_z ? ez : ex, offset = along_z ? z : x;
    int32_t height = ae_sdk_SquareRoot0(r, sub32(mul32(radius, radius), mul32(offset, offset)));
    if (height < ae_rds32(r, point + 4)) return 0;
    ae_wr32(r, out, height);
    ae_wr32(r, along_z ? AE_COLL_SLOPE_X : AE_COLL_SLOPE_Z, 0);
    return 1;
}

/* 8004EC3C: some cylinder codes are intentionally disabled; failed half-pipes fall back to a disc. */
int32_t ae_CollShape_Cylinder(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out) {
    ae_wr32(r, out, 0);
    if ((uint16_t)sub32(type, 70) < 4) return 0;
    if ((uint16_t)sub32(type, 36) < 2) {
        /* An isolated RAM local provides the original pointer API without exposing a host pointer. */
        uint32_t local = 0x801FF010u;
        if (ae_CollHalfPipeHeight(r, (int16_t)type, shape, point, local)) {
            ae_wr32(r, out, ae_rd32(r, local)); return 1;
        }
    } else if ((uint16_t)sub32(type, 83) < 2 || (int16_t)type == 82) return 0;
    int32_t distance = ae_player_vec_length3(ae_rds32(r, point), 0, ae_rds32(r, point + 8));
    if (distance >= ae_rds16(r, shape)) return 0;
    int32_t height = ae_rds16(r, shape + AE_SHAPE_HEIGHT);
    slopes(r, 0, 0); ae_wr32(r, out, height);
    return 1;
}

/* 8004F438: the annulus excludes both edges. Width is 7.5% of radius, at least 64. */
int32_t ae_ColliderRingHeight(AeRam *r, uint32_t collider, uint32_t point, uint32_t out) {
    ae_wr32(r, out, 0);
    int32_t radius = ae_rds16(r, collider + AE_COL_EXTENT_X);
    int32_t width = radius * 75 / 1000;
    if (width < 64) width = 64;
    int32_t distance = ae_player_vec_length3(sub32(ae_rds16(r, collider), ae_rds32(r, point)), 0,
                              sub32(ae_rds16(r, collider + AE_COL_Z), ae_rds32(r, point + 8)));
    radius = ae_rds16(r, collider + AE_COL_EXTENT_X);
    if (distance <= radius - width / 2 || distance >= radius + width / 2) return 0;
    ae_wr32(r, out, ae_rds16(r, collider + AE_COL_EXTENT_Y));
    return 1;
}

/* 800774B8: project onto either diagonal, then return a signed Euclidean distance. */
int32_t ae_DiagonalSignedDistance(AeRam *r, uint32_t type, uint32_t point, uint32_t center, uint32_t out) {
    type &= 0xF00;
    int32_t sign = -1;
    if (type == 0 || type == 0x400 || type == 0x800 || type == 0xC00) {
        int32_t x = ae_rds32(r, point), z = ae_rds32(r, point + 8);
        int32_t cx = ae_rds16(r, center), cz = ae_rds16(r, center + 4);
        int32_t sum, difference;
        if (type == 0 || type == 0x800) {
            sum = cx + cz; difference = sub32(z, x);
        } else {
            sum = add32(x, z); difference = cz - cx;
        }
        ae_wr32(r, out, sub32(sum, difference) / 2);
        /* Reload inputs after the store, preserving the original alias behavior. */
        x = ae_rds32(r, point); z = ae_rds32(r, point + 8);
        cx = ae_rds16(r, center); cz = ae_rds16(r, center + 4);
        if (type == 0 || type == 0x800) {
            sum = cx + cz; difference = sub32(z, x);
        } else {
            sum = add32(x, z); difference = cz - cx;
        }
        ae_wr32(r, out + 8, add32(sum, difference) / 2);
        x = ae_rds32(r, point); z = ae_rds32(r, point + 8);
        cx = ae_rds16(r, center); cz = ae_rds16(r, center + 4);
        int32_t edge = type == 0 || type == 0x800 ? add32(sub32(cx, x), cz) : sub32(add32(x, cz), cx);
        if ((type == 0 || type == 0xC00) ? z < edge : edge < z) sign = 1;
    }
    int32_t dx = sub32(ae_rds32(r, point), ae_rds32(r, out));
    int32_t dz = sub32(ae_rds32(r, point + 8), ae_rds32(r, out + 8));
    return mul32(sign, ae_sdk_SquareRoot0(r, add32(mul32(dx, dx), mul32(dz, dz))));
}

/* 800500D8: trench arcs, with the original sign-dependent inner/outer radius choice. */
int32_t ae_ColliderTrenchHeight(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out) {
    ae_wr32(r, out, 0);
    int along_z = (int16_t)type == 83;
    int32_t along = ae_rds32(r, point + (along_z ? 8 : 0));
    int32_t radius = ae_rds16(r, shape + (along_z ? 4 : 0));
    int32_t across = ae_rds32(r, point + (along_z ? 0 : 8));
    int32_t extent = ae_rds16(r, shape + (along_z ? 0 : 4));
    if (radius + 256 < abs32(along) || extent < abs32(across)) return 0;
    int32_t y = ae_rds32(r, point + 4);
    int32_t distance = ae_player_vec_length3(y, 0, along);
    int32_t arc_radius = radius - 256;
    int positive_arc = y < 0 && arc_radius < distance;
    if (positive_arc) arc_radius = radius + 256;
    int outside = y >= 0 && radius + 256 < distance;
    if (outside) arc_radius = 0;
    else if (y >= 0 && radius - 192 <= distance) arc_radius = radius + 256;
    int32_t height = 0;
    if (!outside) {
        height = ae_sdk_SquareRoot0(r, sub32(mul32(arc_radius, arc_radius), mul32(along, along)));
        if (!positive_arc) height = sub32(0, height);
    }
    ae_wr32(r, out, height);
    ae_wr32(r, along_z ? AE_COLL_SLOPE_X : AE_COLL_SLOPE_Z, 0);
    int32_t slope = 0;
    if (height < 0) {
        radius = ae_rds16(r, shape + (along_z ? 4 : 0));
        slope = divide(r, mul32(sub32(radius, add32(abs32(height), 256)), 3072), radius - 256);
        if (ae_ram_failed(r)) return 0;
        if (ae_rds32(r, point + (along_z ? 8 : 0)) > 0) slope = sub32(0, slope);
        if (abs32(slope) < 128) slope = 0;
    }
    ae_wr32(r, along_z ? AE_COLL_SLOPE_Z : AE_COLL_SLOPE_X, slope);
    return ae_rds32(r, out) != 0;
}

static int32_t missing_stale(AeRam *r) {
    ae_ram_fail(r, "AeSolidHooks stale caller value not provided"); return 0;
}

/* 8004E444: intersecting thin wall strips. Axis selection for unsupported types
 * outside their intersection reads an undefined caller value in the original. */
int32_t ae_CollShape_WallThin(AeRam *r, const AeSolidHooks *h, int32_t type, uint32_t shape, uint32_t point, uint32_t out) {
    ae_wr32(r, out, 0);
    if (ae_rds16(r, AE_COLL_WALL_PROBE) != 1) return 0;
    type = (int16_t)type;
    int32_t x = ae_rds32(r, point), z = ae_rds32(r, point + 8);
    int32_t axis;
    if (type == 3 || type == 5) axis = 2;
    else if (type == 4 || type == 6) axis = 1;
    else if (abs32(x) < 65 && abs32(z) < 65) axis = 3;
    else {
        switch (type) {
        case 42: axis = x < z ? 1 : 2; break;
        case 43: axis = sub32(0, x) < z ? 2 : 1; break;
        case 44: axis = x < z ? 2 : 1; break;
        case 45: axis = sub32(0, x) < z ? 1 : 2; break;
        default: if (!h) return missing_stale(r); axis = (int16_t)h->stale_wall_axis; break;
        }
    }
    if (axis == 1) {
        if ((uint32_t)x + 64u >= 129u) return 0;
        int32_t extent = ae_rds16(r, shape + AE_SHAPE_EXTENT_Z);
        if (extent < z || z < -extent) return 0;
    } else if (axis == 2) {
        if ((uint32_t)z + 64u >= 129u) return 0;
        int32_t extent = ae_rds16(r, shape);
        if (extent < x || x < -extent) return 0;
    }
    ae_wr32(r, out, ae_rds16(r, shape + AE_SHAPE_HEIGHT)); return 1;
}

/* 8004E63C: bands at the cell edges, choosing the nearer edge at corners. */
int32_t ae_CollShape_WallEdge(AeRam *r, const AeSolidHooks *h, int32_t type, uint32_t shape, uint32_t point, uint32_t out) {
    ae_wr32(r, out, 0);
    if (ae_rds16(r, AE_COLL_WALL_PROBE) != 1) return 0;
    int32_t x = ae_rds32(r, point), z = ae_rds32(r, point + 8);
    int32_t axis, edge_x = 0, edge_z = 0;
    switch ((int16_t)sub32(type, 46)) {
    case 0: edge_z = -256; axis = 2; break;
    case 1: edge_x = -256; axis = 1; break;
    case 2: edge_z = 256; axis = 2; break;
    case 3: edge_x = 256; axis = 1; break;
    case 4: case 5: case 6: case 7: {
        int index = (int16_t)sub32(type, 46);
        edge_x = index < 6 ? -256 : 256;
        edge_z = index == 4 || index == 7 ? -256 : 256;
        if (x >= edge_x - 64 && x <= edge_x + 64 && z >= edge_z - 64 && z <= edge_z + 64) axis = 3;
        else if (index == 4) axis = x < z ? 1 : 2;
        else if (index == 5) axis = sub32(0, x) < z ? 2 : 1;
        else if (index == 6) axis = x < z ? 2 : 1;
        else axis = sub32(0, x) < z ? 1 : 2;
        break;
    }
    default:
        if (!h) return missing_stale(r);
        axis = (int16_t)h->stale_wall_axis; edge_x = h->stale_edge_x; edge_z = h->stale_edge_z; break;
    }
    if (axis == 1) {
        if (edge_x + 64 < x || x < edge_x - 64) return 0;
        int32_t extent = ae_rds16(r, shape + AE_SHAPE_EXTENT_Z);
        if (extent < z || z < -extent) return 0;
    } else if (axis == 2) {
        if (edge_z + 64 < z || z < edge_z - 64) return 0;
        int32_t extent = ae_rds16(r, shape);
        if (extent < x || x < -extent) return 0;
    }
    ae_wr32(r, out, ae_rds16(r, shape + AE_SHAPE_HEIGHT)); return 1;
}

static int32_t shape_diagonal_distance(AeRam *r, int32_t type, int base_type, uint32_t point) {
    uint32_t center = 0x801FF030u, projection = 0x801FF040u;
    for (uint32_t i = 0; i < 3; ++i) ae_wr16(r, center + 2 * i, 0);
    uint32_t flags = ae_rd32(r, 0x800A4DF0u + 4u * (uint32_t)((int16_t)type - base_type));
    return ae_DiagonalSignedDistance(r, (uint32_t)(int32_t)(int16_t)type | flags, point, center, projection);
}

/* 8004E8F8: diagonal wall band; it does not clear the contact slopes. */
int32_t ae_CollShape_WallDiagonal(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out) {
    ae_wr32(r, out, 0);
    if (ae_rds16(r, AE_COLL_WALL_PROBE) != 1 || !inside_shape(r, shape, point)) return 0;
    if (abs32(shape_diagonal_distance(r, type, 55, point)) >= 65) return 0;
    ae_wr32(r, out, ae_rds16(r, shape + AE_SHAPE_HEIGHT)); return 1;
}

/* 8004F330: flat top clipped to one side of the diagonal, including its edge. */
int32_t ae_CollShape_DiagonalBox(AeRam *r, int32_t type, uint32_t shape, uint32_t point, uint32_t out) {
    ae_wr32(r, out, 0);
    if (!inside_shape(r, shape, point)) return 0;
    if (shape_diagonal_distance(r, type, 38, point) > 0) return 0;
    slopes(r, 0, 0); ae_wr32(r, out, ae_rds16(r, shape + AE_SHAPE_HEIGHT)); return 1;
}

/* 8004F984: radial slope interpolation and its two contact gradients. Only
 * slope types 0x46..0x49 initialize their corner and side angles. */
int32_t ae_ColliderSlopeHeight(AeRam *r, const AeSolidHooks *h, uint32_t collider, uint32_t point, uint32_t vector, uint32_t out) {
    int32_t radius = ae_rds32(r, vector);
    if (ae_rds16(r, collider + AE_COL_Y) < sub32(ae_rds32(r, point + 4), ae_rds32(r, vector + 8))) return -1;
    uint32_t type = ae_rd8(r, collider + AE_COL_TYPE);
    uint32_t corner = 0x801FF080u, angles = 0x801FF090u;
    uint32_t input = 0x801FF0A0u, normal = 0x801FF0B0u;
    if (type < 70 || type > 73) {
        if (!h) return missing_stale(r);
        ae_wr32(r, corner, h->stale_slope_corner_x); ae_wr32(r, corner + 4, h->stale_slope_corner_z);
        ae_wr16(r, angles, h->stale_slope_angle_low); ae_wr16(r, angles + 2, h->stale_slope_angle_high);
    }
    ae_ColliderSlopeCorner(r, collider, corner);
    int32_t outer = ae_rds16(r, collider + AE_COL_EXTENT_X), inner = ae_rds16(r, collider + AE_COL_INNER_RADIUS);
    int32_t arc_radius = outer + inner;
    if (!arc_radius) arc_radius = 1;
    int32_t cx = ae_rds32(r, corner), cz = ae_rds32(r, corner + 4);
    int32_t distance = ae_player_vec_length3(sub32(cx, ae_rds32(r, point)), 0, sub32(cz, ae_rds32(r, point + 8)));
    if (abs32(sub32(arc_radius, distance)) > add32(outer - inner, radius)) return -2;
    if (ae_ColliderSlopeSide(r, collider, point, radius, angles) < 0) return -3;
    ae_wr32(r, input, sub32(ae_rds32(r, point), cx)); ae_wr32(r, input + 4, 0);
    ae_wr32(r, input + 8, sub32(ae_rds32(r, point + 8), cz));
    ae_sdk_VectorNormalS(r, input, normal);
    if (ae_ram_failed(r)) return 0;
    int32_t angle = ae_sdk_ratan2(r, ae_rds16(r, normal + 4), ae_rds16(r, normal));
    if (angle < 0) angle += 4096;
    if (!(angle & 1023)) {
        if (angle == ae_rds16(r, angles)) ++angle;
        else if (angle == ae_rds16(r, angles + 2)) --angle;
        if (angle < 0) angle += 4096;
    }
    int32_t low_height = ae_rds16(r, collider + AE_COL_LOW_HEIGHT);
    int32_t delta = ae_rds16(r, collider + AE_COL_EXTENT_Y) - low_height;
    int32_t height = add32(low_height, mul32(delta, angle - (angle / 1024) * 1024) / 1024);
    ae_wr32(r, input, sub32(ae_rds32(r, point + 8), cz)); ae_wr32(r, input + 4, 0);
    ae_wr32(r, input + 8, sub32(ae_rds32(r, point), cx));
    ae_sdk_VectorNormalS(r, input, normal);
    if (ae_ram_failed(r)) return 0;
    int32_t denominator = mul32(arc_radius, 628);
    int32_t gradient = mul32(mul32(delta, ae_rds16(r, normal)) / 4096, 204800);
    ae_wr16(r, out, divide(r, gradient, denominator));
    delta = ae_rds16(r, collider + AE_COL_LOW_HEIGHT) - ae_rds16(r, collider + AE_COL_EXTENT_Y);
    gradient = mul32(mul32(delta, ae_rds16(r, normal + 4)) / 4096, 204800);
    ae_wr16(r, out + 2, divide(r, gradient, denominator));
    return height;
}

/* 800797F8: first matching volume, scanning the collider array backwards. */
int32_t ae_GetSurfaceType(AeRam *r, int32_t x, int32_t y, int32_t z) {
    uint32_t scratch = AE_COLL_SURFACE_SCRATCH;
    for (uint32_t off = 0; off <= 24; off += 2) ae_wr16(r, scratch + off, 0);
    int32_t index = (int16_t)(ae_rd16(r, AE_COLLIDER_COUNT) - 1);
    ae_wr16(r, AE_COLL_SURFACE_INDEX, -1);
    if (index < 0) return 0;
    uint32_t colliders = ae_rd32(r, AE_COLLIDERS);
    int32_t near_y = sub32(y, 64);
    for (; index >= 0; --index) {
        uint32_t collider = colliders + (uint32_t)index * AE_COL_SIZE;
        uint32_t surface = ae_rd16(r, collider + AE_COL_SURFACE_TYPE);
        if ((surface & 255) != 9) continue;
        int32_t center = ae_rds16(r, collider), extent = ae_rds16(r, collider + AE_COL_EXTENT_X);
        if (x < center - extent || center + extent < x) continue;
        center = ae_rds16(r, collider + AE_COL_Z); extent = ae_rds16(r, collider + AE_COL_EXTENT_Z);
        if (z < center - extent || center + extent < z) continue;
        center = ae_rds16(r, collider + AE_COL_Y); extent = ae_rds16(r, collider + AE_COL_EXTENT_Y);
        if (y < center - extent || center < y) continue;
        uint32_t raw_center = ae_rd16(r, collider + AE_COL_Y), raw_extent = ae_rd16(r, collider + AE_COL_EXTENT_Y);
        ae_wr16(r, scratch + 16, raw_extent);
        int32_t bottom = (int16_t)(raw_center - raw_extent);
        uint32_t proximity = y >= bottom && near_y < bottom ? 2 :
                             near_y >= bottom && sub32(y, 192) < bottom ? 1 : 0;
        ae_wr16(r, scratch, ((surface & 0xF00) << 4) | proximity);
        ae_wr16(r, scratch + 2, ae_rd16(r, collider + AE_COL_SURFACE_PARAMETER));
        ae_wr16(r, scratch + 4, ae_rd16(r, collider + AE_COL_MODE));
        ae_wr16(r, scratch + 6, ae_rd16(r, collider + AE_COL_CONTACT_PARAMETER));
        for (uint32_t k = 0; k < 4; ++k) ae_wr16(r, scratch + 8 + 2 * k, ae_rd16(r, collider + 2 * k));
        ae_wr16(r, scratch + 18, ae_rd16(r, collider + AE_COL_EXTENT_Z));
        ae_wr16(r, scratch + 20, (uint32_t)ae_rds16(r, collider + AE_COL_FLOW_X) << 12);
        ae_wr16(r, scratch + 22, (uint32_t)ae_rds16(r, collider + AE_COL_FLOW_Y) << 12);
        ae_wr16(r, AE_COLL_SURFACE_INDEX, index);
        ae_wr16(r, scratch + 24, (uint32_t)ae_rds16(r, collider + AE_COL_FLOW_Z) << 12);
        return ae_rd16(r, scratch);
    }
    return 0;
}

/* Dispatch by the actual EXE function pointer, so patched tables are observed. */
static int32_t dispatch_shape(AeRam *r, const AeSolidHooks *h, int32_t type, uint32_t shape, uint32_t point, uint32_t out) {
    switch (ae_rd32(r, AE_COLL_SHAPE_HANDLERS + (uint32_t)type * 4)) {
    case 0x8004E33C: return ae_CollShape_Box(r, type, shape, point, out);
    case 0x8004E444: return ae_CollShape_WallThin(r, h, type, shape, point, out);
    case 0x8004E63C: return ae_CollShape_WallEdge(r, h, type, shape, point, out);
    case 0x8004E8F8: return ae_CollShape_WallDiagonal(r, type, shape, point, out);
    case 0x8004EA1C: return ae_CollShape_Ramp45(r, type, shape, point, out);
    case 0x8004EC3C: return ae_CollShape_Cylinder(r, type, shape, point, out);
    case 0x8004ED30: return ae_CollShape_Ramp2(r, type, shape, point, out);
    case 0x8004EE94: return ae_CollShape_Ramp4(r, type, shape, point, out);
    case 0x8004F000: return ae_CollShape_Wedge(r, type, shape, point, out);
    case 0x8004F330: return ae_CollShape_DiagonalBox(r, type, shape, point, out);
    default: ae_ram_fail(r, "unknown solid shape handler for type %d", type); return 0;
    }
}

/* 8004DF38: geometry's high nibble selects the collider query family. */
int32_t ae_ColliderHeightAt(AeRam *r, const AeSolidHooks *h, uint32_t collider, int32_t x, int32_t y, int32_t z) {
    uint32_t geometry = ae_rd16(r, collider + AE_COL_TYPE);
    if (!ae_ColliderTypeActive(r, (int16_t)geometry, (int16_t)(ae_rd16(r, collider + AE_COL_MODE) & 0x8000))) return 0;
    uint32_t family = geometry & 0xF000;
    int32_t type = ae_rd8(r, collider + AE_COL_TYPE);
    uint32_t point = 0x801FF100u, out = 0x801FF110u;
    int32_t height;
    if (family == 0x3000 || family == 0x9000) {
        if (type == 54 || (type >= 78 && type <= 81)) return 0;
        if (type == 68) {
            ae_wr32(r, point, x); ae_wr32(r, point + 4, y); ae_wr32(r, point + 8, z);
            if (!ae_ColliderRingHeight(r, collider, point, out)) return 0;
            height = ae_rds32(r, out); slopes(r, 0, 0); return height;
        }
        if (type >= 70 && type <= 73) {
            uint32_t vector = 0x801FF120u, slope = 0x801FF130u;
            ae_wr32(r, point, x); ae_wr32(r, point + 4, y); ae_wr32(r, point + 8, z);
            for (uint32_t i = 0; i < 3; ++i) ae_wr32(r, vector + 4 * i, 0);
            height = ae_ColliderSlopeHeight(r, h, collider, point, vector, slope);
            if (ae_ram_failed(r)) return 0;
            if (height < 0) return 0;
            slopes(r, ae_rds16(r, slope), ae_rds16(r, slope + 2)); return height;
        }
        if (type == 36 || type == 37 || type == 83 || type == 84) {
            ae_wr32(r, point, sub32(x, ae_rds16(r, collider)));
            ae_wr32(r, point + 8, sub32(z, ae_rds16(r, collider + AE_COL_Z)));
            ae_wr32(r, point + 4, sub32(y, ae_rds16(r, collider + AE_COL_Y)));
            int hit = type < 38 ? ae_CollHalfPipeHeight(r, type, collider + AE_COL_EXTENT_X, point, out) :
                                  ae_ColliderTrenchHeight(r, type, collider + AE_COL_EXTENT_X, point, out);
            return hit ? ae_rds32(r, out) : 0;
        }
        if (type == 82) return 0;
        int32_t distance = ae_player_vec_length3(sub32(ae_rds16(r, collider), x), 0,
                                                sub32(ae_rds16(r, collider + AE_COL_Z), z));
        if (distance >= ae_rds16(r, collider + AE_COL_EXTENT_X)) return 0;
    } else if (family == 0x1000) {
        if (type < 74 || type > 77) return 0;
        ae_wr32(r, point, x); ae_wr32(r, point + 4, y); ae_wr32(r, point + 8, z);
        if (!ae_ColliderStripHeightOut(r, collider, point, out)) return 0;
        height = ae_rds32(r, out); slopes(r, 0, 0); return height;
    } else {
        int32_t center_x = ae_rds16(r, collider), ex = ae_rds16(r, collider + AE_COL_EXTENT_X);
        if (center_x + ex < x || x < center_x - ex) return 0;
        int32_t center_z = ae_rds16(r, collider + AE_COL_Z), ez = ae_rds16(r, collider + AE_COL_EXTENT_Z);
        if (center_z + ez < z || z < center_z - ez) return 0;
        if (family == 0x2000 || family == 0x4000) {
            if (ae_rds16(r, collider + AE_COL_Y) < y) return 0;
            ae_wr32(r, point, sub32(x, center_x)); ae_wr32(r, point + 4, y);
            ae_wr32(r, point + 8, sub32(z, center_z));
            ae_wr32(r, out, ae_rds16(r, collider + AE_COL_Y));
            return dispatch_shape(r, h, type, collider + AE_COL_EXTENT_X, point, out) ? ae_rds32(r, out) : 0;
        }
    }
    if (ae_rds16(r, collider + AE_COL_Y) < y) return 0;
    height = ae_rds16(r, collider + AE_COL_EXTENT_Y); slopes(r, 0, 0); return height;
}

/* 8004D8C0: shape-column query followed by repeated collider passes. */
int32_t ae_GetSolidHeight(AeRam *r, const AeSolidHooks *h, int32_t x, int32_t y, int32_t z) {
    ae_CollProbeCounterInc(r);
    int32_t half_x = ae_rds32(r, AE_COLL_HALF_X), half_z = ae_rds32(r, AE_COLL_HALF_Z);
    if (x <= sub32(0, half_x) || half_x <= x || z <= sub32(0, half_z) || half_z <= z) {
        ae_wr16(r, AE_COLL_WALL_PROBE, 0); ae_wr16(r, AE_COLL_SKIP_SURFACE, 0); return 32767;
    }
    int32_t grid_height = 32767, shape_index = -1;
    int32_t grid_slope_x = 0, grid_slope_z = 0;
    int32_t collider_slope_x = 0, collider_slope_z = 0;
    int32_t layer = -1;
    if (y <= 0) {
        slopes(r, 0, 0);
        ae_wr32(r, AE_COLL_CURRENT_SHAPES, ae_rd32(r, AE_COLL_SHAPES));
        ae_wr32(r, AE_COLL_CURRENT_GRID, ae_rd32(r, AE_COLL_GRID));
        int32_t gx = add32(x, half_x), gz = add32(z, half_z);
        int32_t cell_x = gx / 512, cell_z = gz / 512;
        int32_t query_layer = sub32(0, y) / 512;
        uint32_t point = 0x801FF180u, out = 0x801FF190u;
        ae_wr32(r, point, sub32(sub32(gx, mul32(cell_x, 512)), 256));
        ae_wr32(r, point + 4, y);
        ae_wr32(r, point + 8, sub32(sub32(gz, mul32(cell_z, 512)), 256));
        int32_t index = ae_rds16(r, ae_rd32(r, AE_COLL_GRID) +
                                2u * (uint32_t)add32(mul32(ae_rds32(r, AE_COLL_GRID_WIDTH), cell_z), cell_x));
        int32_t last_height = 512;
        if (index >= 0) {
            uint32_t shape = ae_rd32(r, AE_COLL_SHAPES) + (uint32_t)index * AE_SHAPE_SIZE;
            if (ae_rds16(r, shape + AE_SHAPE_BAND) <= query_layer) {
                for (uint32_t steps = 0; index >= 0; ++steps) {
                    if (steps == 65536) { ae_ram_fail(r, "cyclic solid shape column"); return 0; }
                    shape = ae_rd32(r, AE_COLL_CURRENT_SHAPES) + (uint32_t)index * AE_SHAPE_SIZE;
                    ae_wr32(r, out, -512 * ae_rds16(r, shape + AE_SHAPE_BAND));
                    int32_t hit = dispatch_shape(r, h, ae_rd8(r, shape + AE_SHAPE_TYPE), shape + AE_SHAPE_PARAMETERS, point, out);
                    if (ae_ram_failed(r)) return 0;
                    shape = ae_rd32(r, AE_COLL_CURRENT_SHAPES) + (uint32_t)index * AE_SHAPE_SIZE;
                    int32_t band = ae_rds16(r, shape + AE_SHAPE_BAND);
                    if (!hit && query_layer <= band) break;
                    if (hit) {
                        shape_index = (int16_t)index; layer = band; last_height = ae_rds32(r, out);
                        int32_t next = ae_rds16(r, shape + AE_SHAPE_NEXT);
                        if (next >= 0) {
                            int32_t rise_bands = last_height < 512 ? 1 : (int16_t)((uint32_t)last_height >> 9);
                            uint32_t next_shape = ae_rd32(r, AE_COLL_CURRENT_SHAPES) + (uint32_t)next * AE_SHAPE_SIZE;
                            if (layer + rise_bands == ae_rds16(r, next_shape + AE_SHAPE_BAND) && last_height > 256) {
                                index = next; continue;
                            }
                        }
                    }
                    index = ae_rds16(r, shape + AE_SHAPE_NEXT);
                    if (index < 0) break;
                    uint32_t next_shape = ae_rd32(r, AE_COLL_CURRENT_SHAPES) + (uint32_t)index * AE_SHAPE_SIZE;
                    if (query_layer < ae_rds16(r, next_shape + AE_SHAPE_BAND)) break;
                }
            }
            if (layer >= 0) {
                grid_height = sub32(-512 * layer, last_height);
                grid_slope_x = ae_rds32(r, AE_COLL_SLOPE_X); grid_slope_z = ae_rds32(r, AE_COLL_SLOPE_Z);
            }
        }
    }
    /* The uninitialized layer is only observable here when y exceeds 32767. */
    if (y > 32767) { if (!h) return missing_stale(r); layer = h->stale_initial_layer; }
    if (layer >= 0 && y > grid_height) y = grid_height;
    int32_t mask = ae_GetColliderQuadrantMaskXZ(r, x, z);
    int32_t best_height = 32767, best_index = -1, previous;
    do {
        previous = best_height;
        for (int32_t i = (int16_t)(ae_rd16(r, AE_COLLIDER_COUNT) - 1); i >= 0; --i) {
            uint32_t collider = ae_rd32(r, AE_COLLIDERS) + (uint32_t)i * AE_COL_SIZE;
            if (!(ae_rd16(r, collider + AE_COL_ZONE_MASK) & mask) || ae_rds16(r, AE_COLL_IGNORE) == i ||
                (ae_rd16(r, collider + AE_COL_FLAGS) & 0x8000)) continue;
            int32_t height = ae_ColliderHeightAt(r, h, collider, x, y, z);
            if (ae_ram_failed(r)) return 0;
            if (!height) continue;
            height = sub32(ae_rds16(r, ae_rd32(r, AE_COLLIDERS) + (uint32_t)i * AE_COL_SIZE + AE_COL_Y), height);
            if (height >= best_height) continue;
            collider_slope_x = ae_rds32(r, AE_COLL_SLOPE_X); collider_slope_z = ae_rds32(r, AE_COLL_SLOPE_Z);
            best_height = height; best_index = (int16_t)i;
            if (height < y) y = sub32(height, 32);
            break;
        }
    } while (best_height != previous);
    if (!ae_rds16(r, AE_COLL_SKIP_SURFACE)) {
        ae_GetSurfaceType(r, x, y, z);
        int32_t i = ae_rds16(r, AE_COLL_SURFACE_INDEX);
        if (i >= 0) {
            uint32_t collider = ae_rd32(r, AE_COLLIDERS) + (uint32_t)i * AE_COL_SIZE;
            uint32_t surface = ae_rd16(r, collider + AE_COL_SURFACE_TYPE) & 0xF00;
            if (surface == 0x400 || surface == 0x500) {
                uint32_t scratch = ae_rd32(r, AE_COLL_SURFACE_PTR);
                int32_t height = ae_rds16(r, scratch + 10) - ae_rds16(r, scratch + 16);
                if (height < best_height) { best_height = height; best_index = i; collider_slope_x = collider_slope_z = 0; }
            }
        }
    }
    ae_wr16(r, AE_COLL_WALL_PROBE, 0); ae_wr16(r, AE_COLL_SKIP_SURFACE, 0);
    uint32_t contact = ae_rd32(r, AE_COLL_CONTACT_PTR);
    if (best_height < grid_height) {
        if (best_index < 0) return 32767;
        ae_wr16(r, contact, 1); ae_wr16(r, contact + 2, best_index);
        uint32_t collider = ae_rd32(r, AE_COLLIDERS) + (uint32_t)best_index * AE_COL_SIZE;
        ae_wr16(r, contact + 4, ae_rd16(r, collider + AE_COL_TYPE));
        ae_wr16(r, contact + 6, ae_rd16(r, collider + AE_COL_SURFACE_TYPE));
        ae_wr16(r, contact + 8, ae_rd16(r, collider + AE_COL_MODE));
        uint32_t parameter = ae_rd16(r, collider + AE_COL_CONTACT_PARAMETER);
        ae_wr16(r, contact + 12, collider_slope_x);
        ae_wr16(r, contact + 10, parameter); ae_wr16(r, contact + 14, collider_slope_z);
        return best_height;
    }
    if (shape_index < 0) return 32767;
    uint32_t shape = ae_rd32(r, AE_COLL_CURRENT_SHAPES) + (uint32_t)shape_index * AE_SHAPE_SIZE;
    ae_wr16(r, contact, 0); ae_wr16(r, contact + 2, shape_index);
    uint32_t type = ae_rd16(r, shape + AE_SHAPE_TYPE);
    ae_wr16(r, contact + 6, 0); ae_wr16(r, contact + 4, type);
    ae_wr16(r, contact + 8, ae_rd16(r, shape + AE_SHAPE_CONTACT_MODE));
    uint32_t parameter = ae_rd16(r, shape + AE_SHAPE_CONTACT_PARAMETER);
    ae_wr16(r, contact + 12, grid_slope_x);
    ae_wr16(r, contact + 10, parameter); ae_wr16(r, contact + 14, grid_slope_z);
    return grid_height;
}
