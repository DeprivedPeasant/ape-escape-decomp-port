#include "ae_area02.h"

/* R3000 arithmetic wraps on the subu operations in this helper. */
static int32_t area02_range_s32(uint32_t bits) {
    int64_t value = bits;
    if ((bits & 0x80000000u) != 0u) value -= 0x100000000LL;
    return (int32_t)value;
}

static int32_t area02_range_sub32(int32_t left, int32_t right) {
    return area02_range_s32((uint32_t)left - (uint32_t)right);
}

int32_t ae_HalfPipeRangeTest(AeRam *r, int32_t y, int32_t body_height,
                             int32_t lower_y, int32_t extent) {
    (void)r;
    int32_t clearance_bottom = area02_range_sub32(lower_y, extent);
    int32_t y_below_margin = area02_range_sub32(y, 96);

    if (clearance_bottom < y_below_margin && y_below_margin < lower_y)
        return 1;
    if (body_height < 96) return 0;

    int32_t half_body_height = body_height / 2;
    int32_t body_midpoint = area02_range_sub32(y, half_body_height);
    if (clearance_bottom < body_midpoint && body_midpoint < lower_y)
        return 1;

    int32_t player_bottom = area02_range_sub32(y, body_height);
    if (clearance_bottom < player_bottom && player_bottom < lower_y)
        return 1;
    if (clearance_bottom < y_below_margin && player_bottom < clearance_bottom)
        return 1;

    int32_t half_extent = extent / 2;
    int32_t extent_midpoint = area02_range_sub32(lower_y, half_extent);
    if (extent_midpoint < y_below_margin && player_bottom < extent_midpoint)
        return 1;
    if (lower_y < y_below_margin && player_bottom < lower_y)
        return 1;
    return 0;
}
