/* Player collision response probes from SCUS-94423, ported as readable AeRam code. */
#include "ae_collprobe.h"
#include "../bodies/ae_probe.h"
#include "../core/ae_sdk.h"
#include "../world/ae_area02.h"
#include "../world/ae_solid.h"

static int32_t add32(int32_t left, int32_t right) {
    return (int32_t)((uint32_t)left + (uint32_t)right);
}

static int32_t sub32(int32_t left, int32_t right) {
    return (int32_t)((uint32_t)left - (uint32_t)right);
}

static int32_t mul32(int32_t left, int32_t right) {
    return (int32_t)((uint32_t)left * (uint32_t)right);
}

static int32_t collprobe_s32(uint32_t bits) {
    int64_t value = bits;
    if ((bits & 0x80000000u) != 0u) value -= 0x100000000LL;
    return (int32_t)value;
}

static int32_t halfpipe_range_test(AeRam *r, uint32_t y,
                                   uint32_t body_height,
                                   uint32_t lower_y, uint32_t extent) {
    return ae_HalfPipeRangeTest(r, collprobe_s32(y),
                                collprobe_s32(body_height),
                                collprobe_s32(lower_y),
                                collprobe_s32(extent));
}

static int32_t divide32(AeRam *r, int32_t numerator, int32_t denominator) {
    if (denominator == 0 || (numerator == INT32_MIN && denominator == -1)) {
        ae_ram_fail(r, "signed divide trap in collision probe");
        return 0;
    }
    return numerator / denominator;
}

static int32_t abs32_wrapped(int32_t value) {
    return value < 0 ? sub32(0, value) : value;
}

static int32_t divide100_magic(int32_t value) {
    int64_t product = (int64_t)value * INT64_C(0x51EB851F);
    int32_t high = (int32_t)(product >> 32);
    return sub32(high >> 5, value >> 31);
}

static int32_t divide1000_magic(int32_t value) {
    int64_t product = (int64_t)value * INT64_C(0x10624DD3);
    int32_t high = (int32_t)(product >> 32);
    return sub32(high >> 6, value >> 31);
}

static int32_t radial_scale(AeRam *r, int32_t target, int32_t delta,
                            int32_t distance, int keep_precision) {
    int32_t numerator = mul32(target, delta);
    if (keep_precision) numerator = mul32(numerator, 100);
    int32_t quotient = divide32(r, numerator, distance);
    return keep_precision ? divide100_magic(quotient) : quotient;
}

static int32_t half_toward_zero(int32_t value) {
    return (value + (value < 0 ? 1 : 0)) >> 1;
}

static int32_t signed_midpoint(int32_t first, int32_t second) {
    int32_t half_difference = half_toward_zero(sub32(first, second));
    return (int16_t)add32(second, half_difference);
}

static int32_t unsigned_magnitude(int32_t value) {
    return value < 0 ? sub32(0, value) : value;
}

static int32_t rope_distance(AeRam *r, int32_t dx, int32_t dz) {
    return ae_sdk_SquareRoot0(r, add32(mul32(dx, dx), mul32(dz, dz)));
}

static void rope_attach(AeRam *r, const AeCollProbeHooks *h, uint32_t shape,
                        int write_rope_index_first) {
    if (write_rope_index_first)
        ae_wr16(r, AE_COLLPROBE_ROPE_INDEX, ae_rd16(r, AE_COLLPROBE_PLAYER_ACTION_A));
    if (h == NULL || h->PlayerSetRope == NULL) {
        ae_ram_fail(r, "CollProbe_Rope requires the PlayerSetRope hook");
        return;
    }
    h->PlayerSetRope(h->user, r, shape,
                     shape + AE_COLLPROBE_ROPE_END_B_X, 0, 0);
    if (!write_rope_index_first)
        ae_wr16(r, AE_COLLPROBE_ROPE_INDEX, ae_rd16(r, AE_COLLPROBE_PLAYER_ACTION_A));
}

int32_t ae_CollProbe_Rope(AeRam *r, const AeCollProbeHooks *h,
                          uint32_t shape, uint32_t probe, uint32_t out) {
    (void)out;
    if (ae_rds16(r, AE_COLLPROBE_BODY_CUR_TYPE) != 1) return 0;
    if (ae_rd16(r, AE_COLLPROBE_ATTACH_KIND) == 6 &&
        ae_rds16(r, AE_COLLPROBE_ROPE_ACTION) ==
            ae_rds16(r, AE_COLLPROBE_PLAYER_ACTION_A)) {
        rope_attach(r, h, shape, 1);
        return 0;
    }

    int32_t x1 = ae_rds16(r, shape + AE_COLLPROBE_ROPE_END_A_X);
    int32_t y1 = ae_rds16(r, shape + AE_COLLPROBE_ROPE_END_A_Y);
    int32_t z1 = ae_rds16(r, shape + AE_COLLPROBE_ROPE_END_A_Z);
    int32_t x2 = ae_rds16(r, shape + AE_COLLPROBE_ROPE_END_B_X);
    int32_t y2 = ae_rds16(r, shape + AE_COLLPROBE_ROPE_END_B_Y);
    int32_t z2 = ae_rds16(r, shape + AE_COLLPROBE_ROPE_END_B_Z);
    int32_t mid_x = signed_midpoint(x1, x2);
    int32_t mid_y = signed_midpoint(y1, y2);
    int32_t mid_z = signed_midpoint(z1, z2);
    uint32_t position = ae_rd32(r, AE_COLLPROBE_CURRENT_POSITION);
    int32_t x = ae_rds32(r, position + AE_COLLPROBE_POSITION_X);
    int32_t y = ae_rds32(r, position + AE_COLLPROBE_POSITION_Y);
    int32_t z = ae_rds32(r, position + AE_COLLPROBE_POSITION_Z);
    int32_t body_height = ae_rds32(r, probe + AE_COLLPROBE_INPUT_HEIGHT);
    int32_t feet_plus_margin = add32(sub32(y, body_height), 64);
    if (feet_plus_margin < sub32(mid_y, 64) ||
        add32(mid_y, 64) < feet_plus_margin) return 0;

    int32_t half_x_span = half_toward_zero(unsigned_magnitude(sub32(x1, x2)));
    int32_t half_z_span = half_toward_zero(unsigned_magnitude(sub32(z1, z2)));
    int32_t dx_from_mid = unsigned_magnitude(sub32(x, mid_x));
    int32_t dz_from_mid = unsigned_magnitude(sub32(z, mid_z));
    int32_t contact = 0;

    if (x1 == x2) {
        contact = dx_from_mid <= 64 && dz_from_mid <= half_z_span;
    } else if (z1 == z2) {
        contact = dz_from_mid <= 64 && dx_from_mid <= half_x_span;
    } else {
        int32_t projected_x;
        int32_t projected_z;
        int32_t same_direction = ((x1 - x2) > 0) == ((z1 - z2) > 0);
        if (same_direction) {
            int32_t sum = add32(x, z);
            projected_x = half_toward_zero(add32(sum, sub32(mid_x, mid_z)));
            projected_z = half_toward_zero(add32(sum, sub32(mid_z, mid_x)));
        } else {
            int32_t mid_sum = add32(mid_x, mid_z);
            int32_t current_difference = sub32(z, x);
            projected_x = half_toward_zero(sub32(mid_sum, current_difference));
            projected_z = half_toward_zero(add32(mid_sum, current_difference));
        }
        ae_wr32(r, AE_COLLPROBE_ROPE_PROJECTED_X, (uint32_t)projected_x);
        ae_wr32(r, AE_COLLPROBE_ROPE_PROJECTED_Z, (uint32_t)projected_z);

        int32_t player_distance = rope_distance(r, sub32(x, projected_x),
                                                sub32(z, projected_z));
        ae_wr32(r, AE_COLLPROBE_ROPE_DISTANCE_FROM_PLAYER,
                (uint32_t)player_distance);
        int32_t midpoint_distance = rope_distance(r, sub32(mid_x, projected_x),
                                                  sub32(mid_z, projected_z));
        ae_wr32(r, AE_COLLPROBE_ROPE_DISTANCE_FROM_MIDPOINT,
                (uint32_t)midpoint_distance);
        if (ae_ram_failed(r)) return 0;
        contact = player_distance < 65 && midpoint_distance <= half_x_span;
    }

    if (contact) rope_attach(r, h, shape, 0);
    return 0;
}

static int32_t wall_range_test(AeRam *r, uint32_t shape, uint32_t probe,
                               uint32_t position) {
    return halfpipe_range_test(r,
                               ae_rd32(r, position + AE_COLLPROBE_POSITION_Y),
                               ae_rd32(r, probe + AE_COLLPROBE_INPUT_HEIGHT),
                               (uint32_t)ae_rds16(r, shape + AE_COL_Y),
                               (uint32_t)ae_rds16(r, shape + AE_COL_EXTENT_Y));
}

static int32_t wall_band_axis(uint32_t type, int32_t dx, int32_t dz, int32_t stale_axis) {
    uint32_t orientation = type & 0x0F00u;
    switch (type & 0xF000u) {
    case 0x1000u:
        return (type & 0x0C00u) != 0 ? 1 : 2;
    case 0x5000u:
    case 0x7000u:
        switch (orientation) {
        case 0x000u: return dx < dz ? 1 : 2;
        case 0x400u: return sub32(0, dx) < dz ? 2 : 1;
        case 0x800u: return dx < dz ? 2 : 1;
        case 0xC00u: return sub32(0, dx) < dz ? 1 : 2;
        default: return stale_axis;
        }
    case 0x6000u:
        switch (orientation) {
        case 0x000u: return 2;
        case 0x400u: return 1;
        case 0x800u: return 2;
        case 0xC00u: return 1;
        default: return stale_axis;
        }
    default:
        return 0;
    }
}

static int32_t wall_band_response(AeRam *r, const AeCollProbeHooks *h,
                                  uint32_t shape, uint32_t probe, uint32_t out,
                                  int edge_wall) {
    uint32_t position = ae_rd32(r, AE_COLLPROBE_CURRENT_POSITION);
    uint32_t type = ae_rd32(r, probe + AE_COLLPROBE_INPUT_TYPE);
    uint32_t material = type & 0xF000u;
    int32_t dx = sub32(ae_rds32(r, position + AE_COLLPROBE_POSITION_X),
                       ae_rds16(r, shape + AE_COL_X));
    int32_t dz = sub32(ae_rds32(r, position + AE_COLLPROBE_POSITION_Z),
                       ae_rds16(r, shape + AE_COL_Z));
    int32_t radius = add32(ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS), 12);
    int32_t center_x = 0;
    int32_t center_z = 0;
    int32_t axis = wall_band_axis(type, dx, dz, h != NULL ? h->stale_wall_axis : 0);

    if (edge_wall) {
        if (material == 0x6000u || material == 0x7000u) {
            uint32_t orientation = type & 0x0F00u;
            if (material == 0x6000u) {
                if (orientation == 0x000u) center_z = -256;
                else if (orientation == 0x400u) center_x = -256;
                else if (orientation == 0x800u) center_z = 256;
                else if (orientation == 0xC00u) center_x = 256;
                else {
                    center_x = h != NULL ? h->stale_edge_x : 0;
                    center_z = h != NULL ? h->stale_edge_z : 0;
                }
            } else {
                if (orientation == 0x000u || orientation == 0x400u ||
                    orientation == 0x800u || orientation == 0xC00u) {
                    center_x = orientation < 0x800u ? -256 : 256;
                    center_z = (orientation == 0x000u || orientation == 0xC00u) ? -256 : 256;
                } else {
                    center_x = h != NULL ? h->stale_edge_x : 0;
                    center_z = h != NULL ? h->stale_edge_z : 0;
                }
            }
        } else {
            return 0;
        }
    } else if (material != 0x1000u && material != 0x5000u) {
        return 0;
    }

    if (wall_range_test(r, shape, probe, position) == 0) return 0;
    if (ae_ram_failed(r)) return 0;

    if (axis == 1) {
        int32_t extent_z = ae_rds16(r, shape + AE_COL_EXTENT_Z);
        int32_t extent_margin = (material == 0x6000u || material == 0x7000u)
            ? radius : 0;
        if (abs32_wrapped(sub32(dx, center_x)) > radius ||
            abs32_wrapped(dz) > add32(extent_z, extent_margin)) return 0;
        int32_t edge = dx <= center_x ? -radius : radius;
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_X,
                (uint32_t)sub32(add32(center_x, edge), dx));
        return 1;
    }
    if (axis == 2) {
        int32_t extent_x = ae_rds16(r, shape + AE_COL_EXTENT_X);
        int32_t extent_margin = (material == 0x6000u || material == 0x7000u)
            ? radius : 0;
        if (abs32_wrapped(sub32(dz, center_z)) > radius ||
            abs32_wrapped(dx) > add32(extent_x, extent_margin)) return 0;
        int32_t edge = dz <= center_z ? -radius : radius;
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Z,
                (uint32_t)sub32(add32(center_z, edge), dz));
        return 2;
    }
    if (axis == 3) {
        int32_t relative_x = sub32(dx, center_x);
        int32_t relative_z = sub32(dz, center_z);
        int32_t push_x = relative_x <= 0
            ? sub32(sub32(0, radius), relative_x)
            : sub32(radius, relative_x);
        int32_t push_z = relative_z <= 0
            ? sub32(sub32(0, radius), relative_z)
            : sub32(radius, relative_z);
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_X, (uint32_t)push_x);
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Z, (uint32_t)push_z);
        return 3;
    }
    return 0;
}

int32_t ae_CollProbe_NoContact(AeRam *r, const AeCollProbeHooks *h,
                               uint32_t collider, uint32_t probe, uint32_t out) {
    (void)collider;
    if (h == NULL || h->printf == NULL) {
        ae_ram_fail(r, "CollProbe_NoContact requires the printf hook");
        return 0;
    }

    /* The original logs the probe type and its adjacent halfword as one word. */
    uint32_t probe_type_word = ae_rd32(r, probe + AE_COLLPROBE_INPUT_TYPE);
    h->printf(h->user, r, AE_COLLPROBE_NO_CONTACT_FORMAT, probe_type_word, out, 0);
    return 0;
}

int32_t ae_CollProbe_Strip(AeRam *r, const AeCollProbeHooks *h,
                           uint32_t collider, uint32_t probe, uint32_t out) {
    (void)h;
    uint32_t position = ae_rd32(r, AE_COLLPROBE_CURRENT_POSITION);
    uint32_t geometry = ae_rd8(r, collider + AE_COL_TYPE);
    int32_t radius_with_margin = add32(ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS), 12);
    int32_t offset_x = sub32(ae_rds32(r, position + AE_COLLPROBE_POSITION_X), ae_rds16(r, collider + AE_COL_X));
    int32_t offset_z = sub32(ae_rds32(r, position + AE_COLLPROBE_POSITION_Z), ae_rds16(r, collider + AE_COL_Z));
    int32_t result;
    int32_t response;

    if (geometry == AE_COLLPROBE_STRIP_Z_A || geometry == AE_COLLPROBE_STRIP_Z_B) {
        response = 2;
    } else {
        response = 1;
    }

    result = ae_ColliderStripHeight(r, collider, position);
    if (result == 0) return 0;
    if (halfpipe_range_test(r,
                            ae_rd32(r, position + AE_COLLPROBE_POSITION_Y),
                            ae_rd32(r, probe + AE_COLLPROBE_INPUT_HEIGHT),
                            (uint32_t)ae_rds16(r, collider + AE_COL_Y),
                            (uint32_t)result) == 0) {
        return 0;
    }

    if (response == 2) {
        int32_t push_z = offset_z > 0
            ? sub32(radius_with_margin, offset_z)
            : sub32(sub32(0, radius_with_margin), offset_z);
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Z, (uint32_t)push_z);
    } else {
        int32_t push_x = offset_x > 0
            ? sub32(radius_with_margin, offset_x)
            : sub32(sub32(0, radius_with_margin), offset_x);
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_X, (uint32_t)push_x);
    }
    return response;
}

int32_t ae_CollProbe_ThinWall(AeRam *r, const AeCollProbeHooks *h,
                              uint32_t shape, uint32_t probe, uint32_t out) {
    return wall_band_response(r, h, shape, probe, out, 0);
}

int32_t ae_CollProbe_EdgeWall(AeRam *r, const AeCollProbeHooks *h,
                              uint32_t shape, uint32_t probe, uint32_t out) {
    return wall_band_response(r, h, shape, probe, out, 1);
}

static int32_t point_in_box_xz(AeRam *r, const AeCollProbeHooks *h,
                               uint32_t position, uint32_t shape) {
    if (h == NULL || h->PointInBoxXZ == NULL) {
        ae_ram_fail(r, "CollProbe_Box requires the PointInBoxXZ hook");
        return 0;
    }
    return h->PointInBoxXZ(h->user, r, position, shape, 0, 0);
}

int32_t ae_CollProbe_Box(AeRam *r, const AeCollProbeHooks *h,
                         uint32_t shape, uint32_t probe, uint32_t out) {
    uint32_t type = ae_rd8(r, probe + AE_COLLPROBE_INPUT_TYPE);
    switch (type) {
    case 60:
        ae_wr16(r, shape + AE_COL_Z,
                ae_rd16(r, shape + AE_COL_Z) - ae_rd16(r, shape + AE_COL_EXTENT_Z));
        break;
    case 61:
        ae_wr16(r, shape + AE_COL_X,
                ae_rd16(r, shape + AE_COL_X) - ae_rd16(r, shape + AE_COL_EXTENT_X));
        break;
    case 62:
        ae_wr16(r, shape + AE_COL_Z,
                ae_rd16(r, shape + AE_COL_Z) + ae_rd16(r, shape + AE_COL_EXTENT_Z));
        break;
    case 63:
        ae_wr16(r, shape + AE_COL_X,
                ae_rd16(r, shape + AE_COL_X) + ae_rd16(r, shape + AE_COL_EXTENT_X));
        break;
    }

    int32_t half_x = ae_rds16(r, shape + AE_COL_EXTENT_X);
    int32_t half_z = ae_rds16(r, shape + AE_COL_EXTENT_Z);
    if (half_x == 0) {
        ae_wr16(r, shape + AE_COL_EXTENT_X, 1);
        half_x = 1;
    }

    int32_t radius_u16 = (int32_t)ae_rd16(r, probe + AE_COLLPROBE_INPUT_RADIUS);
    int32_t expanded_x = half_x + radius_u16;
    int32_t expanded_z = (int32_t)ae_rd16(r, shape + AE_COL_EXTENT_Z) + radius_u16;
    int32_t bounds_x = (int16_t)expanded_x;
    int32_t bounds_z = (int16_t)expanded_z;
    uint32_t position = ae_rd32(r, AE_COLLPROBE_CURRENT_POSITION);
    int32_t center_x = ae_rds16(r, shape + AE_COL_X);
    int32_t center_z = ae_rds16(r, shape + AE_COL_Z);
    int32_t x = ae_rds32(r, position + AE_COLLPROBE_POSITION_X);
    int32_t z = ae_rds32(r, position + AE_COLLPROBE_POSITION_Z);

    if (add32(center_x, bounds_x) < x || x < sub32(center_x, bounds_x)) return 0;
    if (add32(center_z, bounds_z) < z || z < sub32(center_z, bounds_z)) return 0;

    if (halfpipe_range_test(r,
                            ae_rd32(r, position + AE_COLLPROBE_POSITION_Y),
                            ae_rd32(r, probe + AE_COLLPROBE_INPUT_HEIGHT),
                            (uint32_t)ae_rds16(r, shape + AE_COL_Y),
                            (uint32_t)ae_rds16(r, shape + AE_COL_EXTENT_Y)) == 0) return 0;

    int32_t dx = sub32(x, center_x);
    int32_t dz = sub32(z, center_z);
    int32_t sector;
    if (abs32_wrapped(dx) > half_x && abs32_wrapped(dz) > half_z) {
        int32_t corner_x = dx > half_x ? sub32(dx, half_x) : add32(dx, half_x);
        int32_t corner_z = dz > half_z ? sub32(dz, half_z) : add32(dz, half_z);
        sector = (corner_x < corner_z ? 2 : 0) |
                 (sub32(0, corner_x) < corner_z ? 1 : 0);
    } else {
        int32_t slope_z = ae_rds16(r, shape + AE_COL_EXTENT_Z);
        int32_t slope_x = ae_rds16(r, shape + AE_COL_EXTENT_X);
        int32_t ratio_x = divide32(r, mul32(dx, slope_z), slope_x);
        int32_t ratio_neg_x = divide32(r, mul32(sub32(0, dx), slope_z), slope_x);
        if (ae_ram_failed(r)) return 0;
        sector = (ratio_x < dz ? 2 : 0) | (ratio_neg_x < dz ? 1 : 0);
    }

    if (sector == 0 || sector == 3) {
        int32_t offset = sub32(center_z, z);
        int32_t extent = (int16_t)expanded_z;
        int32_t push_z = z < center_z ? sub32(offset, extent) : add32(offset, extent);
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Z, (uint32_t)push_z);
    } else {
        int32_t offset = sub32(center_x, x);
        int32_t extent = (int16_t)expanded_x;
        int32_t push_x = x < center_x ? sub32(offset, extent) : add32(offset, extent);
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_X, (uint32_t)push_x);
    }

    int32_t shape_top = ae_rds16(r, shape + AE_COL_Y);
    int32_t player_y = ae_rds32(r, position + AE_COLLPROBE_POSITION_Y);
    int32_t body_height = ae_rds32(r, probe + AE_COLLPROBE_INPUT_HEIGHT);
    int32_t shape_bottom = sub32(shape_top, ae_rds16(r, shape + AE_COL_EXTENT_Y));
    if (sub32(player_y, body_height) >= shape_top || shape_bottom >= player_y) return 1;

    if (point_in_box_xz(r, h, position, shape) == 0) return 1;
    if (point_in_box_xz(r, h, ae_rd32(r, AE_COLLPROBE_PREVIOUS_POSITION), shape) == 0) return 1;
    int32_t previous_bottom = sub32(player_y, body_height);
    int32_t vertical_response = sub32(shape_top, previous_bottom);
    ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Y, (uint32_t)vertical_response);
    return 1;
}

int32_t ae_CollProbe_DiagonalWall(AeRam *r, const AeCollProbeHooks *h,
                                 uint32_t shape, uint32_t probe, uint32_t out) {
    (void)h;
    uint32_t position = ae_rd32(r, AE_COLLPROBE_CURRENT_POSITION);
    uint32_t type = ae_rd32(r, probe + AE_COLLPROBE_INPUT_TYPE);
    int32_t radius = add32(ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS), 12);
    int32_t half_x = ae_rds16(r, shape + AE_COL_EXTENT_X);
    int32_t half_z = ae_rds16(r, shape + AE_COL_EXTENT_Z);
    int32_t dx = sub32(ae_rds32(r, position + AE_COLLPROBE_POSITION_X),
                       ae_rds16(r, shape + AE_COL_X));
    int32_t dz = sub32(ae_rds32(r, position + AE_COLLPROBE_POSITION_Z),
                       ae_rds16(r, shape + AE_COL_Z));
    if (add32(half_x, ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS)) < dx ||
        dx < sub32(0, add32(half_x, ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS))) ||
        add32(half_z, ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS)) < dz ||
        dz < sub32(0, add32(half_z, ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS)))) return 0;

    if (halfpipe_range_test(r,
                            ae_rd32(r, position + AE_COLLPROBE_POSITION_Y),
                            ae_rd32(r, probe + AE_COLLPROBE_INPUT_HEIGHT),
                            (uint32_t)ae_rds16(r, shape + AE_COL_Y),
                            (uint32_t)ae_rds16(r, shape + AE_COL_EXTENT_Y)) == 0) return 0;

    int32_t distance = ae_DiagonalSignedDistance(r, type, position, shape,
                                                  AE_COLLPROBE_DIAGONAL_PROJECTION);
    if (ae_ram_failed(r)) return 0;
    if (abs32_wrapped(distance) > radius) return 0;
    if (distance == 0) distance = 1;

    int32_t projection_dx = abs32_wrapped(sub32(
        ae_rds32(r, AE_COLLPROBE_DIAGONAL_PROJECTION),
        ae_rds32(r, position + AE_COLLPROBE_POSITION_X)));
    int32_t projection_dz = abs32_wrapped(sub32(
        ae_rds32(r, AE_COLLPROBE_DIAGONAL_PROJECTION + 8u),
        ae_rds32(r, position + AE_COLLPROBE_POSITION_Z)));
    int32_t scaled_x = divide32(r, mul32(projection_dx, radius), distance);
    int32_t scaled_z = divide32(r, mul32(projection_dz, radius), distance);
    if (ae_ram_failed(r)) return 0;

    uint32_t orientation = type & 0x0F00u;
    int32_t sign_x, sign_z;
    switch (orientation) {
    case 0x000u: sign_x = -1; sign_z = -1; break;
    case 0x400u: sign_x = -1; sign_z = 1; break;
    case 0x800u: sign_x = 1; sign_z = 1; break;
    case 0xC00u: sign_x = 1; sign_z = -1; break;
    default: return 1;
    }
    int32_t side_adjustment = distance < 0 ? 1 : -1;
    int32_t push_x = mul32(sign_x, add32(scaled_x, mul32(side_adjustment, projection_dx)));
    int32_t push_z = mul32(sign_z, add32(scaled_z, mul32(side_adjustment, projection_dz)));
    ae_wr32(r, out + AE_COLLPROBE_OUTPUT_X, (uint32_t)push_x);
    ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Z, (uint32_t)push_z);
    return 1;
}

int32_t ae_CollProbe_HalfPipe(AeRam *r, const AeCollProbeHooks *h,
                              uint32_t shape, uint32_t probe, uint32_t out) {
    uint32_t position = ae_rd32(r, AE_COLLPROBE_CURRENT_POSITION);
    uint32_t type = ae_rd8(r, probe + AE_COLLPROBE_INPUT_TYPE);
    int32_t radius = ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS);
    int32_t half_x_unsigned = (int32_t)ae_rd16(r, shape + AE_COL_EXTENT_X);
    int32_t half_z_unsigned = (int32_t)ae_rd16(r, shape + AE_COL_EXTENT_Z);
    int32_t expanded_x = half_x_unsigned + radius;
    int32_t expanded_z = half_z_unsigned + radius;
    int32_t arc_radius;
    int32_t along;
    int check_axis_extent = 0;

    if (type == 36 || type == 83) {
        arc_radius = ae_rds16(r, shape + AE_COL_EXTENT_Z);
        along = sub32(ae_rds32(r, position + AE_COLLPROBE_POSITION_Z),
                      ae_rds16(r, shape + AE_COL_Z));
        check_axis_extent = type == 36;
    } else {
        arc_radius = ae_rds16(r, shape + AE_COL_EXTENT_X);
        along = sub32(ae_rds32(r, position + AE_COLLPROBE_POSITION_X),
                      ae_rds16(r, shape + AE_COL_X));
        check_axis_extent = type == 37;
    }
    if (check_axis_extent && abs32_wrapped(along) > arc_radius) return 0;

    int32_t square = sub32(mul32(arc_radius, arc_radius), mul32(along, along));
    int32_t arc_height = ae_sdk_SquareRoot0(r, square);
    if (ae_ram_failed(r)) return 0;
    int32_t shape_y = ae_rds16(r, shape + AE_COL_Y);
    if (halfpipe_range_test(r,
                            ae_rd32(r, position + AE_COLLPROBE_POSITION_Y),
                            ae_rd32(r, probe + AE_COLLPROBE_INPUT_HEIGHT),
                            (uint32_t)add32(shape_y, arc_height),
                            (uint32_t)mul32(arc_height, 2)) == 0) return 0;

    int32_t half_x = ae_rds16(r, shape + AE_COL_EXTENT_X);
    int32_t half_z = ae_rds16(r, shape + AE_COL_EXTENT_Z);
    int32_t x = ae_rds32(r, position + AE_COLLPROBE_POSITION_X);
    int32_t z = ae_rds32(r, position + AE_COLLPROBE_POSITION_Z);
    int32_t dx = sub32(x, ae_rds16(r, shape + AE_COL_X));
    int32_t dz = sub32(z, ae_rds16(r, shape + AE_COL_Z));
    int32_t sector;
    if (abs32_wrapped(dx) > half_x && abs32_wrapped(dz) > half_z) {
        int32_t corner_x = dx > half_x ? sub32(dx, half_x) : add32(dx, half_x);
        int32_t corner_z = dz > half_z ? sub32(dz, half_z) : add32(dz, half_z);
        sector = (corner_x < corner_z ? 2 : 0) |
                 (sub32(0, corner_x) < corner_z ? 1 : 0);
    } else {
        int32_t ratio_x = divide32(r, mul32(dx, half_z), half_x);
        int32_t ratio_neg_x = divide32(r, mul32(sub32(0, dx), half_z), half_x);
        if (ae_ram_failed(r)) return 0;
        sector = (ratio_x < dz ? 2 : 0) | (ratio_neg_x < dz ? 1 : 0);
    }

    int32_t player_y = ae_rds32(r, position + AE_COLLPROBE_POSITION_Y);
    int32_t body_height = ae_rds32(r, probe + AE_COLLPROBE_INPUT_HEIGHT);
    int32_t expanded_x_signed = (int16_t)expanded_x;
    int32_t expanded_z_signed = (int16_t)expanded_z;
    if (sector == 0 || sector == 3) {
        int32_t offset = sub32(ae_rds16(r, shape + AE_COL_Z), z);
        int32_t push_z = z < ae_rds16(r, shape + AE_COL_Z)
            ? sub32(offset, expanded_z_signed) : add32(offset, expanded_z_signed);
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Z, (uint32_t)push_z);
    } else {
        int32_t offset = sub32(ae_rds16(r, shape + AE_COL_X), x);
        int32_t push_x = x < ae_rds16(r, shape + AE_COL_X)
            ? sub32(offset, expanded_x_signed) : add32(offset, expanded_x_signed);
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_X, (uint32_t)push_x);
    }

    int32_t top = add32(shape_y, arc_height);
    int32_t bottom = sub32(shape_y, arc_height);
    int32_t player_bottom = sub32(player_y, body_height);
    if (player_bottom >= top || bottom >= player_y) return 1;
    if (point_in_box_xz(r, h, position, shape) == 0) return 1;
    if (point_in_box_xz(r, h, ae_rd32(r, AE_COLLPROBE_PREVIOUS_POSITION), shape) == 0) return 1;

    ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Y, (uint32_t)sub32(top, player_bottom));
    ae_wr32(r, out + AE_COLLPROBE_OUTPUT_SLOPE_X, (uint32_t)arc_height);
    ae_wr32(r, out + AE_COLLPROBE_OUTPUT_SLOPE_Z, (uint32_t)arc_height);
    return 1;
}

int32_t ae_CollProbe_Slope(AeRam *r, const AeCollProbeHooks *h,
                           uint32_t shape, uint32_t probe, uint32_t out) {
    (void)h;
    uint32_t position = ae_rd32(r, AE_COLLPROBE_CURRENT_POSITION);
    int32_t height = ae_ColliderSlopeHeight(r, NULL, shape, position, probe,
                                             AE_COLLPROBE_SLOPE_GRADIENT);
    if (ae_ram_failed(r) || height < 0) return 0;

    int32_t shape_top = ae_rds16(r, shape + AE_COL_Y);
    int32_t player_y = ae_rds32(r, position + AE_COLLPROBE_POSITION_Y);
    if (player_y < add32(sub32(shape_top, height), 64)) return 0;

    ae_ColliderSlopeCorner(r, shape, AE_COLLPROBE_SLOPE_CONTACT);
    int32_t outer_radius = ae_rds16(r, shape + AE_COL_EXTENT_X);
    int32_t inner_radius = ae_rds16(r, shape + AE_COL_INNER_RADIUS);
    int32_t outer_edge = add32(outer_radius, inner_radius);
    int32_t inner_edge = sub32(outer_radius, inner_radius);
    int32_t corner_x = ae_rds32(r, AE_COLLPROBE_SLOPE_CONTACT);
    int32_t corner_z = ae_rds32(r, AE_COLLPROBE_SLOPE_CONTACT + 4u);
    int32_t px = ae_rds32(r, position + AE_COLLPROBE_POSITION_X);
    int32_t pz = ae_rds32(r, position + AE_COLLPROBE_POSITION_Z);
    int32_t from_corner_x = sub32(px, corner_x);
    int32_t from_corner_z = sub32(pz, corner_z);
    int32_t distance_squared = add32(mul32(from_corner_x, from_corner_x),
                                     mul32(from_corner_z, from_corner_z));
    int32_t distance = ae_sdk_SquareRoot0(r, distance_squared);
    if (ae_ram_failed(r)) return 0;
    if (distance == 0) distance = 1;

    int32_t outer_gap = sub32(outer_edge, distance);
    int32_t in_vertical_band;
    if (outer_gap < 0) {
        in_vertical_band = sub32(distance, outer_edge) <= inner_edge;
    } else {
        in_vertical_band = outer_gap <= inner_edge;
    }
    if (in_vertical_band) {
        int32_t low_height = ae_rds16(r, shape + AE_COL_LOW_HEIGHT);
        int32_t min_height = ae_rds16(r, shape + AE_COL_EXTENT_Y);
        if (low_height < min_height) min_height = low_height;
        int32_t player_bottom = sub32(player_y, ae_rds32(r, probe + AE_COLLPROBE_INPUT_HEIGHT));
        if (player_bottom < shape_top && sub32(shape_top, min_height) < player_y) {
            ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Y,
                    (uint32_t)sub32(shape_top, player_bottom));
            return 1;
        }
        return 0;
    }

    int32_t probe_radius = ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS);
    int32_t radial_width = distance < outer_edge
        ? sub32(sub32(outer_edge, inner_edge), probe_radius)
        : add32(add32(outer_edge, inner_edge), probe_radius);
    int32_t offset_x = divide32(r, mul32(radial_width, from_corner_x), distance);
    int32_t offset_z = divide32(r, mul32(radial_width, from_corner_z), distance);
    if (ae_ram_failed(r)) return 0;
    if (offset_x == from_corner_x && offset_z == from_corner_z) {
        if (corner_x >= px) offset_x = add32(offset_x, 1);
        else offset_x = sub32(offset_x, 1);
        if (corner_z >= pz) offset_z = add32(offset_z, 1);
        else offset_z = sub32(offset_z, 1);
    }
    ae_wr32(r, out + AE_COLLPROBE_OUTPUT_X, (uint32_t)sub32(add32(corner_x, offset_x), px));
    ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Z, (uint32_t)sub32(add32(corner_z, offset_z), pz));
    return 1;
}

int32_t ae_CollProbe_Ramp(AeRam *r, const AeCollProbeHooks *h,
                          uint32_t shape, uint32_t probe, uint32_t out) {
    uint32_t position = ae_rd32(r, AE_COLLPROBE_CURRENT_POSITION);
    uint32_t type_word = ae_rd32(r, probe + AE_COLLPROBE_INPUT_TYPE);
    uint32_t type = type_word & 0xFFu;
    int32_t radius = (int32_t)ae_rd16(r, probe + AE_COLLPROBE_INPUT_RADIUS);
    int32_t expanded_x = add32(ae_rd16(r, shape + AE_COL_EXTENT_X), radius);
    int32_t expanded_z = add32(ae_rd16(r, shape + AE_COL_EXTENT_Z), radius);
    int32_t center_x = ae_rds16(r, shape + AE_COL_X);
    int32_t center_z = ae_rds16(r, shape + AE_COL_Z);
    int32_t player_x = ae_rds32(r, position + AE_COLLPROBE_POSITION_X);
    int32_t player_z = ae_rds32(r, position + AE_COLLPROBE_POSITION_Z);
    if (add32(center_x, (int16_t)expanded_x) < player_x ||
        player_x < sub32(center_x, (int16_t)expanded_x) ||
        add32(center_z, (int16_t)expanded_z) < player_z ||
        player_z < sub32(center_z, (int16_t)expanded_z)) return 0;

    for (uint32_t offset = 0; offset < 12; offset += 4) {
        ae_wr32(r, AE_COLLPROBE_RAMP_POINT + offset,
                ae_rd32(r, position + offset));
    }
    int32_t shape_right = add32(center_x, ae_rds16(r, shape + AE_COL_EXTENT_X));
    int32_t shape_left = sub32(center_x, ae_rds16(r, shape + AE_COL_EXTENT_X));
    int32_t shape_front = add32(center_z, ae_rds16(r, shape + AE_COL_EXTENT_Z));
    int32_t shape_back = sub32(center_z, ae_rds16(r, shape + AE_COL_EXTENT_Z));
    if (player_x >= shape_right)
        ae_wr32(r, AE_COLLPROBE_RAMP_POINT, (uint32_t)sub32(shape_right, 1));
    else if (player_x <= shape_left)
        ae_wr32(r, AE_COLLPROBE_RAMP_POINT, (uint32_t)add32(shape_left, 1));
    if (player_z >= shape_front)
        ae_wr32(r, AE_COLLPROBE_RAMP_POINT + AE_COLLPROBE_POSITION_Z,
                (uint32_t)sub32(shape_front, 1));
    else if (player_z <= shape_back)
        ae_wr32(r, AE_COLLPROBE_RAMP_POINT + AE_COLLPROBE_POSITION_Z,
                (uint32_t)add32(shape_back, 1));

    if (h == NULL || h->ColliderSlopeContactDir == NULL) {
        ae_ram_fail(r, "CollProbe_Ramp requires the ColliderSlopeContactDir hook");
        return 0;
    }
    if (h->ColliderSlopeContactDir(h->user, r, type, shape,
                                   AE_COLLPROBE_RAMP_POINT,
                                   AE_COLLPROBE_RAMP_HEIGHT) == 0) return 0;
    int32_t top = ae_rds32(r, AE_COLLPROBE_RAMP_HEIGHT);
    int32_t height = ae_rds32(r, AE_COLLPROBE_RAMP_HEIGHT + 4u);
    int32_t player_y = ae_rds32(r, position + AE_COLLPROBE_POSITION_Y);
    int32_t body_height = ae_rds32(r, probe + AE_COLLPROBE_INPUT_HEIGHT);
    if (halfpipe_range_test(r, (uint32_t)player_y,
                            (uint32_t)body_height, (uint32_t)top,
                            (uint32_t)height) == 0) return 0;

    int32_t dx = sub32(player_x, center_x);
    int32_t dz = sub32(player_z, center_z);
    int32_t half_x = ae_rds16(r, shape + AE_COL_EXTENT_X);
    int32_t half_z = ae_rds16(r, shape + AE_COL_EXTENT_Z);
    int32_t ratio_x = divide32(r, mul32(dx, half_z), half_x);
    int32_t ratio_neg_x = divide32(r, mul32(sub32(0, dx), half_z), half_x);
    if (ae_ram_failed(r)) return 0;
    int32_t sector = (ratio_x < dz ? 2 : 0) | (ratio_neg_x < dz ? 1 : 0);
    if (sector == 0 || sector == 3) {
        int32_t side = player_z < center_z ? -(int16_t)expanded_z : (int16_t)expanded_z;
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Z,
                (uint32_t)add32(sub32(center_z, player_z), side));
    } else {
        int32_t side = player_x < center_x ? -(int16_t)expanded_x : (int16_t)expanded_x;
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_X,
                (uint32_t)add32(sub32(center_x, player_x), side));
    }

    int32_t player_bottom = sub32(player_y, body_height);
    int wedge = type >= 64 && type <= 67;
    int32_t lower = sub32(top, height);
    int vertical_overlap = wedge
        ? player_bottom < top && lower < player_y
        : player_bottom < top && top < player_y && player_bottom > lower;
    if (vertical_overlap &&
        point_in_box_xz(r, h, position, shape) != 0 &&
        point_in_box_xz(r, h, ae_rd32(r, AE_COLLPROBE_PREVIOUS_POSITION), shape) != 0) {
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Y,
                (uint32_t)sub32(top, player_bottom));
    }

    int32_t floor_edge = (type >= 7 && type <= 10) ? top : lower;
    if (sub32(player_y, 96) < floor_edge) {
        if (wedge) return 0;
        uint32_t orientation = type_word & 0x0F00u;
        if ((orientation == 0x800u && sector == 3) ||
            (orientation == 0xC00u && sector == 2) ||
            (orientation == 0x400u && sector == 1) ||
            (orientation != 0x800u && orientation != 0xC00u &&
             orientation != 0x400u && sector == 0)) return 0;
    }
    return 1;
}

int32_t ae_CollProbe_Trench(AeRam *r, const AeCollProbeHooks *h,
                            uint32_t shape, uint32_t probe, uint32_t out) {
    uint32_t position = ae_rd32(r, AE_COLLPROBE_CURRENT_POSITION);
    int32_t type = ae_rd8(r, probe + AE_COLLPROBE_INPUT_TYPE);
    int32_t player_x = ae_rds32(r, position + AE_COLLPROBE_POSITION_X);
    int32_t player_y = ae_rds32(r, position + AE_COLLPROBE_POSITION_Y);
    int32_t player_z = ae_rds32(r, position + AE_COLLPROBE_POSITION_Z);
    int32_t center_x = ae_rds16(r, shape + AE_COL_X);
    int32_t center_y = ae_rds16(r, shape + AE_COL_Y);
    int32_t center_z = ae_rds16(r, shape + AE_COL_Z);
    int32_t radius = type == 83
        ? ae_rds16(r, shape + AE_COL_EXTENT_Z)
        : ae_rds16(r, shape + AE_COL_EXTENT_X);
    int32_t body_height = ae_rds32(r, probe + AE_COLLPROBE_INPUT_HEIGHT);

    if (type == 83 || type == 84) {
        int along_z = type == 83;
        int32_t along = sub32(along_z ? player_z : player_x,
                              along_z ? center_z : center_x);
        int32_t relative_x = sub32(player_x, center_x);
        int32_t relative_y = sub32(player_y, center_y);
        int32_t relative_z = sub32(player_z, center_z);
        ae_wr32(r, AE_COLLPROBE_TRENCH_POINT, (uint32_t)relative_x);
        ae_wr32(r, AE_COLLPROBE_TRENCH_POINT + 4u,
                (uint32_t)sub32(relative_y, body_height));
        ae_wr32(r, AE_COLLPROBE_TRENCH_POINT + 8u, (uint32_t)relative_z);
        ae_ColliderTrenchHeight(r, type, shape + AE_COL_EXTENT_X,
                                AE_COLLPROBE_TRENCH_POINT,
                                AE_COLLPROBE_TRENCH_FOOT_HEIGHT);
        int32_t foot_height = ae_rds32(r, AE_COLLPROBE_TRENCH_FOOT_HEIGHT);

        ae_wr32(r, AE_COLLPROBE_TRENCH_POINT + 4u, (uint32_t)relative_y);
        ae_ColliderTrenchHeight(r, type, shape + AE_COL_EXTENT_X,
                                AE_COLLPROBE_TRENCH_POINT,
                                AE_COLLPROBE_TRENCH_TOP_HEIGHT);
        int32_t top_height = ae_rds32(r, AE_COLLPROBE_TRENCH_TOP_HEIGHT);
        if (ae_ram_failed(r)) return 0;

        int32_t bottom_distance = ae_sdk_SquareRoot0(r,
            add32(mul32(sub32(relative_y, body_height), sub32(relative_y, body_height)),
                 mul32(along, along)));
        int32_t top_distance = ae_sdk_SquareRoot0(r,
            add32(mul32(relative_y, relative_y), mul32(along, along)));
        int32_t inner_limit = sub32(radius, 192);
        if (ae_ram_failed(r)) return 0;
        if (bottom_distance <= inner_limit && top_distance <= inner_limit) {
            int32_t edge_square = sub32(mul32(sub32(radius, 256), sub32(radius, 256)),
                                        mul32(body_height, body_height));
            int32_t edge = sub32(ae_sdk_SquareRoot0(r, edge_square), 64);
            if (ae_ram_failed(r)) return 0;
            if (foot_height == top_height && sub32(0, edge) < along && along < edge)
                return 0;
            if (along < sub32(0, edge)) {
                ae_wr32(r, along_z ? out + AE_COLLPROBE_OUTPUT_Z : out + AE_COLLPROBE_OUTPUT_X,
                        (uint32_t)sub32(sub32(0, edge), along));
            } else if (along > edge) {
                ae_wr32(r, along_z ? out + AE_COLLPROBE_OUTPUT_Z : out + AE_COLLPROBE_OUTPUT_X,
                        (uint32_t)sub32(edge, along));
            }
            if (top_height < 0 && foot_height > 0) {
                ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Y,
                        (uint32_t)sub32(add32(center_y, top_height),
                                        sub32(player_y, body_height)));
                ae_wr32(r, out + AE_COLLPROBE_OUTPUT_SLOPE_X, (uint32_t)top_height);
                ae_wr32(r, out + AE_COLLPROBE_OUTPUT_SLOPE_Z,
                        (uint32_t)abs32_wrapped(top_height));
            }
            return 1;
        }
    }

    for (uint32_t offset = 0; offset < 12; offset += 2)
        ae_wr16(r, AE_COLLPROBE_TRENCH_HALFPIPE_SHAPE + offset,
                ae_rd16(r, shape + offset));
    if (type == 83) {
        ae_wr16(r, AE_COLLPROBE_TRENCH_HALFPIPE_SHAPE + AE_COL_EXTENT_Z,
                ae_rd16(r, AE_COLLPROBE_TRENCH_HALFPIPE_SHAPE + AE_COL_EXTENT_Z) + 256u);
    } else if (type == 84) {
        ae_wr16(r, AE_COLLPROBE_TRENCH_HALFPIPE_SHAPE + AE_COL_EXTENT_X,
                ae_rd16(r, AE_COLLPROBE_TRENCH_HALFPIPE_SHAPE + AE_COL_EXTENT_X) + 256u);
    }
    return ae_CollProbe_HalfPipe(r, h, AE_COLLPROBE_TRENCH_HALFPIPE_SHAPE, probe, out);
}

int32_t ae_CollProbe_Triangle(AeRam *r, const AeCollProbeHooks *h,
                              uint32_t shape, uint32_t probe, uint32_t out) {
    uint32_t position = ae_rd32(r, AE_COLLPROBE_CURRENT_POSITION);
    uint32_t previous_position = ae_rd32(r, AE_COLLPROBE_PREVIOUS_POSITION);
    uint32_t type = ae_rd32(r, probe + AE_COLLPROBE_INPUT_TYPE);
    int32_t radius = ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS);
    int32_t body_height = ae_rds32(r, probe + AE_COLLPROBE_INPUT_HEIGHT);
    int32_t center_x = ae_rds16(r, shape + AE_COL_X);
    int32_t center_y = ae_rds16(r, shape + AE_COL_Y);
    int32_t center_z = ae_rds16(r, shape + AE_COL_Z);
    int32_t expanded_x = add32(ae_rd16(r, shape + AE_COL_EXTENT_X),
                               ae_rd16(r, probe + AE_COLLPROBE_INPUT_RADIUS));
    int32_t expanded_z = add32(ae_rd16(r, shape + AE_COL_EXTENT_Z),
                               ae_rd16(r, probe + AE_COLLPROBE_INPUT_RADIUS));
    int32_t half_x = ae_rds16(r, shape + AE_COL_EXTENT_X);
    int32_t half_y = ae_rds16(r, shape + AE_COL_EXTENT_Y);
    int32_t half_z = ae_rds16(r, shape + AE_COL_EXTENT_Z);
    int32_t x = ae_rds32(r, position + AE_COLLPROBE_POSITION_X);
    int32_t y = ae_rds32(r, position + AE_COLLPROBE_POSITION_Y);
    int32_t z = ae_rds32(r, position + AE_COLLPROBE_POSITION_Z);
    if (x < sub32(center_x, (int16_t)expanded_x) ||
        add32(center_x, (int16_t)expanded_x) < x ||
        z < sub32(center_z, (int16_t)expanded_z) ||
        add32(center_z, (int16_t)expanded_z) < z) return 0;

    if (halfpipe_range_test(r, (uint32_t)y, (uint32_t)body_height,
                            (uint32_t)center_y, (uint32_t)half_y) == 0) return 0;

    int32_t distance = ae_DiagonalSignedDistance(r, type, position, shape,
                                                  AE_COLLPROBE_DIAGONAL_PROJECTION);
    if (ae_ram_failed(r)) return 0;
    if (add32(radius, 32) < distance) return 0;
    int32_t projected_dx = abs32_wrapped(sub32(
        ae_rds32(r, AE_COLLPROBE_DIAGONAL_PROJECTION), x));
    int32_t projected_dz = abs32_wrapped(sub32(
        ae_rds32(r, AE_COLLPROBE_DIAGONAL_PROJECTION + 8u), z));
    if (add32(projected_dx, projected_dz) > add32(half_x, half_z)) return 0;

    if (h == NULL || h->DirIndexFromFlags == NULL) {
        ae_ram_fail(r, "CollProbe_Triangle requires the DirIndexFromFlags hook");
        return 0;
    }
    int32_t direction_index = h->DirIndexFromFlags(h->user, r, type, 0, 0, 0);
    int32_t dx = sub32(x, center_x);
    int32_t dz = sub32(z, center_z);
    int32_t ratio_x = divide32(r, mul32(dx, half_z), half_x);
    int32_t ratio_neg_x = divide32(r, mul32(sub32(0, dx), half_z), half_x);
    if (ae_ram_failed(r)) return 0;
    int32_t sector = (ratio_x < dz ? 2 : 0) | (ratio_neg_x < dz ? 1 : 0);
    uint32_t orientation = (type >> 10) & 3u;
    int corner_sector =
        (orientation == 0 && (sector == 0 || sector == 2)) ||
        (orientation == 1 && sector >= 2) ||
        (orientation == 2 && (sector == 1 || sector == 3)) ||
        (orientation == 3 && sector < 2);
    if (corner_sector || distance == 0) sector = 4;

    if (sector == 0 || sector == 3) {
        int32_t offset = sub32(center_z, z);
        int32_t side = z < center_z ? -(int16_t)expanded_z : (int16_t)expanded_z;
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Z, (uint32_t)add32(offset, side));
    } else if (sector == 1 || sector == 2) {
        int32_t offset = sub32(center_x, x);
        int32_t side = x < center_x ? -(int16_t)expanded_x : (int16_t)expanded_x;
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_X, (uint32_t)add32(offset, side));
    } else {
        for (uint32_t offset = 0; offset < 32; offset += 4)
            ae_wr32(r, AE_COLLPROBE_TRIANGLE_DIRECTION_COPY + offset,
                    ae_rd32(r, AE_COLLPROBE_TRIANGLE_DIRECTION_TABLE + offset));
        uint32_t direction_address = AE_COLLPROBE_TRIANGLE_DIRECTION_COPY +
                                     (uint32_t)direction_index * 8u;
        int32_t direction_x = ae_rds16(r, direction_address);
        int32_t direction_z = ae_rds16(r, direction_address + 4u);
        if (direction_x < 0) direction_x = add32(direction_x, 2047);
        if (direction_z < 0) direction_z = add32(direction_z, 2047);
        direction_x >>= 11;
        direction_z >>= 11;
        int32_t response_x, response_z;
        if (distance > 0) {
            int32_t radius_margin = add32(radius, 32);
            int32_t offset_x = sub32(
                divide32(r, mul32(projected_dx, radius_margin), distance), projected_dx);
            int32_t offset_z = sub32(
                divide32(r, mul32(projected_dz, radius_margin), distance), projected_dz);
            response_x = mul32(direction_x, offset_x);
            response_z = mul32(direction_z, offset_z);
        } else {
            response_x = divide100_magic(mul32(mul32(direction_x, radius), 71));
            response_z = divide100_magic(mul32(mul32(direction_z, body_height), 71));
        }
        if (ae_ram_failed(r)) return 0;
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_X, (uint32_t)response_x);
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Z, (uint32_t)response_z);
    }

    int32_t player_bottom = sub32(y, body_height);
    if (player_bottom < center_y && sub32(center_y, half_y) < y &&
        point_in_box_xz(r, h, position, shape) != 0 &&
        point_in_box_xz(r, h, previous_position, shape) != 0 &&
        distance < 0) {
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Y,
                (uint32_t)sub32(center_y, player_bottom));
    }
    return 1;
}

static int32_t radial_ring_response(AeRam *r, const AeCollProbeHooks *h,
                                    uint32_t shape, uint32_t probe,
                                    uint32_t out, uint32_t type) {
    (void)h;
    uint32_t position = ae_rd32(r, AE_COLLPROBE_CURRENT_POSITION);
    int32_t x = ae_rds32(r, position + AE_COLLPROBE_POSITION_X);
    int32_t y = ae_rds32(r, position + AE_COLLPROBE_POSITION_Y);
    int32_t z = ae_rds32(r, position + AE_COLLPROBE_POSITION_Z);
    int32_t center_x = ae_rds16(r, shape + AE_COL_X);
    int32_t center_y = ae_rds16(r, shape + AE_COL_Y);
    int32_t center_z = ae_rds16(r, shape + AE_COL_Z);
    int32_t half_x = ae_rds16(r, shape + AE_COL_EXTENT_X);
    int32_t half_y = ae_rds16(r, shape + AE_COL_EXTENT_Y);
    int32_t body_height = ae_rds32(r, probe + AE_COLLPROBE_INPUT_HEIGHT);
    int32_t dx = sub32(x, center_x);
    int32_t dz = sub32(z, center_z);
    int32_t distance = ae_sdk_SquareRoot0(r, add32(mul32(dx, dx), mul32(dz, dz)));
    if (ae_ram_failed(r)) return 0;
    if (distance == 0) distance = 1;

    int32_t bottom = sub32(center_y, half_y);
    int32_t player_bottom = sub32(y, body_height);
    if (y < add32(bottom, 64) || center_y < player_bottom) return 0;
    int32_t width = divide1000_magic(mul32(half_x, 75));
    if (width < 64) width = 64;
    if (abs32_wrapped(sub32(distance, half_x)) > width) return 0;
    if (type >= 78 && type <= 81 && !ae_ColliderHalfSpaceInside(r, shape, position)) return 0;

    int32_t target = distance < half_x ? sub32(half_x, width) : add32(half_x, width);
    int32_t push_x = sub32(add32(center_x, radial_scale(r, target, sub32(x, center_x),
                                                       distance, 1)), x);
    int32_t push_z = sub32(add32(center_z, radial_scale(r, target, sub32(z, center_z),
                                                       distance, 1)), z);
    if (ae_ram_failed(r)) return 0;
    ae_wr32(r, out + AE_COLLPROBE_OUTPUT_X, (uint32_t)push_x);
    ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Z, (uint32_t)push_z);

    return 1;
}

static int32_t radial_round_response(AeRam *r, const AeCollProbeHooks *h,
                                     uint32_t shape, uint32_t probe,
                                     uint32_t out, uint32_t type) {
    uint32_t position = ae_rd32(r, AE_COLLPROBE_CURRENT_POSITION);
    uint32_t previous = ae_rd32(r, AE_COLLPROBE_PREVIOUS_POSITION);
    int32_t x = ae_rds32(r, position + AE_COLLPROBE_POSITION_X);
    int32_t y = ae_rds32(r, position + AE_COLLPROBE_POSITION_Y);
    int32_t z = ae_rds32(r, position + AE_COLLPROBE_POSITION_Z);
    int32_t center_x = ae_rds16(r, shape + AE_COL_X);
    int32_t center_y = ae_rds16(r, shape + AE_COL_Y);
    int32_t center_z = ae_rds16(r, shape + AE_COL_Z);
    int32_t half_x = ae_rds16(r, shape + AE_COL_EXTENT_X);
    int32_t half_y = ae_rds16(r, shape + AE_COL_EXTENT_Y);
    int32_t radius = ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS);
    int32_t body_height = ae_rds32(r, probe + AE_COLLPROBE_INPUT_HEIGHT);
    int32_t dx = sub32(x, center_x);
    int32_t dz = sub32(z, center_z);
    int32_t distance = ae_sdk_SquareRoot0(r, add32(mul32(dx, dx), mul32(dz, dz)));
    if (ae_ram_failed(r)) return 0;
    if (distance == 0) distance = 1;

    int32_t expanded_radius = add32(radius, half_x);
    if (distance >= expanded_radius) return 0;
    if (halfpipe_range_test(r, (uint32_t)y, (uint32_t)body_height,
                            (uint32_t)center_y, (uint32_t)half_y) == 0) return 0;

    if ((type == 59 || type == 89) &&
        ae_rds16(r, AE_COLLPROBE_BODY_CUR_TYPE) == 1) {
        if (h->PlayerSetClimbTarget == NULL) {
            ae_ram_fail(r, "CollProbe_Radial requires the PlayerSetClimbTarget hook");
            return 0;
        }
        h->PlayerSetClimbTarget(h->user, r,
                                (uint32_t)center_x, (uint32_t)center_y,
                                (uint32_t)center_z,
                                (uint32_t)ae_rds16(r, shape + AE_COL_EXTENT_X),
                                (uint32_t)half_y, type == 59 ? 1u : 0u);
        ae_wr16(r, AE_COLLPROBE_ROPE_INDEX,
                ae_rd16(r, AE_COLLPROBE_PLAYER_ACTION_A));
    }

    int32_t keep_precision = expanded_radius < 4097;
    int32_t push_x = sub32(add32(center_x,
        radial_scale(r, expanded_radius, sub32(x, center_x), distance, keep_precision)), x);
    int32_t push_z = sub32(add32(center_z,
        radial_scale(r, expanded_radius, sub32(z, center_z), distance, keep_precision)), z);
    if (ae_ram_failed(r)) return 0;
    ae_wr32(r, out + AE_COLLPROBE_OUTPUT_X, (uint32_t)push_x);
    ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Z, (uint32_t)push_z);

    int32_t player_bottom = sub32(y, body_height);
    if (player_bottom < center_y && sub32(center_y, half_y) < y &&
        point_in_box_xz(r, h, previous, shape) != 0 && distance < half_x) {
        ae_wr32(r, out + AE_COLLPROBE_OUTPUT_Y,
                (uint32_t)sub32(center_y, player_bottom));
    }
    return (type == 54 || type == 59 || type == 68 || type == 89 ||
            (type >= 78 && type <= 81)) ? 2 : 1;
}

int32_t ae_CollProbe_Radial(AeRam *r, const AeCollProbeHooks *h,
                            uint32_t shape, uint32_t probe, uint32_t out) {
    uint32_t type = ae_rd8(r, probe + AE_COLLPROBE_INPUT_TYPE);
    int32_t radius = ae_rds32(r, probe + AE_COLLPROBE_INPUT_RADIUS);
    uint32_t position = ae_rd32(r, AE_COLLPROBE_CURRENT_POSITION);
    int32_t x = ae_rds32(r, position + AE_COLLPROBE_POSITION_X);
    int32_t z = ae_rds32(r, position + AE_COLLPROBE_POSITION_Z);
    int32_t center_x = ae_rds16(r, shape + AE_COL_X);
    int32_t center_z = ae_rds16(r, shape + AE_COL_Z);

    if (type == 36 || type == 37) {
        int32_t expanded_x = add32(ae_rd16(r, shape + AE_COL_EXTENT_X), radius);
        int32_t expanded_z = add32(ae_rd16(r, shape + AE_COL_EXTENT_Z), radius);
        if (x < sub32(center_x, expanded_x) || add32(center_x, expanded_x) < x ||
            z < sub32(center_z, expanded_z) || add32(center_z, expanded_z) < z) return 0;
        return ae_CollProbe_HalfPipe(r, h, shape, probe, out);
    }
    if (type >= 70 && type <= 73) return ae_CollProbe_Slope(r, h, shape, probe, out);
    if (type == 83 || type == 84) {
        int32_t expanded_x = add32(ae_rd16(r, shape + AE_COL_EXTENT_X), radius);
        int32_t expanded_z = add32(ae_rd16(r, shape + AE_COL_EXTENT_Z), radius);
        if (type == 83) expanded_z = add32(expanded_z, 256);
        else expanded_x = add32(expanded_x, 256);
        if (x < sub32(center_x, expanded_x) || add32(center_x, expanded_x) < x ||
            z < sub32(center_z, expanded_z) || add32(center_z, expanded_z) < z) return 0;
        return ae_CollProbe_Trench(r, h, shape, probe, out);
    }
    if (type == 82) return ae_CollProbe_Rope(r, h, shape, probe, out);
    if ((type == 59 || type == 89) && ae_rd16(r, AE_COLLPROBE_ATTACH_KIND) == 5) {
        if (ae_rds16(r, AE_COLLPROBE_PLAYER_ACTION_B) ==
                ae_rds16(r, AE_COLLPROBE_PLAYER_ACTION_A) &&
            ae_rds16(r, AE_COLLPROBE_BODY_CUR_TYPE) == 1) {
            ae_wr16(r, AE_COLLPROBE_ROPE_INDEX,
                    ae_rd16(r, AE_COLLPROBE_PLAYER_ACTION_A));
        }
        return 0;
    }
    if (type == 54 || type == 68 || (type >= 78 && type <= 81))
        return radial_ring_response(r, h, shape, probe, out, type);
    return radial_round_response(r, h, shape, probe, out, type);
}
