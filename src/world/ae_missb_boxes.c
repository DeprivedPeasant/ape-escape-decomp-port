#include "../ui/ae_missb.h"
#include "../core/ae_sdk_matrix.h"

#define AE_MISSB_COLLIDER_CENTER_X 0x00u
#define AE_MISSB_COLLIDER_CENTER_Y 0x02u
#define AE_MISSB_COLLIDER_CENTER_Z 0x04u
#define AE_MISSB_COLLIDER_HALF_X 0x06u
#define AE_MISSB_COLLIDER_HALF_Z 0x0Au
#define AE_MISSB_COLLIDER_HALF_Y 0x08u
#define AE_MISSB_BODY_POSITION_X 0x24u
#define AE_MISSB_BODY_POSITION_Y 0x26u
#define AE_MISSB_BODY_POSITION_Z 0x28u
#define AE_MISSB_BODY_HEIGHT 0x08u
#define AE_MISSB_HALF_SIZE_X 0x00u
#define AE_MISSB_HALF_SIZE_Z 0x02u
#define AE_MISSB_POINT_X 0x00u
#define AE_MISSB_POINT_Y 0x04u
#define AE_MISSB_POINT_Z 0x08u
#define AE_MISSB_PLAYER 0x800EC1E0u
#define AE_MISSB_ACTOR_POSITION 0x24u
#define AE_MISSB_ACTOR_POSITION_X 0x24u
#define AE_MISSB_ACTOR_POSITION_Y 0x28u
#define AE_MISSB_ACTOR_POSITION_Z 0x2Cu
#define AE_MISSB_SVECTOR_Y 0x02u
#define AE_MISSB_SVECTOR_Z 0x04u
#define AE_MISSB_RCCAR 0x800F5A20u
#define AE_MISSB_RCCAR_STATE 0x70u
#define AE_MISSB_GAME_STATE 0x800F4350u
#define AE_MISSB_COLLIDER_COUNT 0x284u
#define AE_MISSB_COLLIDER_POINTER 0x20Cu
#define AE_MISSB_COLLIDER_STRIDE 56u
#define AE_MISSB_COLLIDER_KIND 0x12u
#define AE_MISSB_MONKEYS 0x800E5500u
#define AE_MISSB_MONKEY_STRIDE 0x250u
#define AE_MISSB_MONKEY_ACTIVITY 0x7Au
#define AE_MISSB_MONKEY_POSITION 0x24u
#define AE_MISSB_MONKEY_RADIUS 0x06u
#define AE_MISSB_AREA_PROPS 0x800B4D64u
#define AE_MISSB_AREA_PROP_STRIDE 0x20u
#define AE_MISSB_AREA_PROP_ROTATION 0x1Cu
#define AE_MISSB_AREA_PROP_POSITION_X 0x10u
#define AE_MISSB_AREA_PROP_POSITION_Y 0x12u
#define AE_MISSB_AREA_PROP_POSITION_Z 0x14u
#define AE_MISSB_DOOR_PROP_INDEX 0x0Eu
#define AE_MISSB_DOOR_STATE 0x00u
#define AE_MISSB_DOOR_DISTANCE 0x06u
#define AE_MISSB_DOOR_AXIS 0x08u
#define AE_MISSB_DOOR_Y 0x02u
#define AE_MISSB_DOOR_Z 0x04u
#define AE_MISSB_DEFAULT_STACK 0x801FFF00u

static int32_t missb_boxes_s32(uint32_t value)
{
    return (value & 0x80000000u) != 0u
        ? (int32_t)((int64_t)value - 0x100000000ll)
        : (int32_t)value;
}

static int32_t missb_boxes_abs_delta(uint32_t point, int32_t center)
{
    uint32_t difference = point - (uint32_t)center;
    if ((difference & 0x80000000u) != 0u) difference = 0u - difference;
    return missb_boxes_s32(difference);
}

/* 8008EC70: the vertical interval is strict at both ends; X and Z use the
 * supplied half sizes and are strict as well. */
int32_t ae_BodyInsideBox(AeRam *r, uint32_t point, uint32_t collider,
                         int32_t unused, uint32_t half_sizes)
{
    (void)unused;
    int32_t center_z = ae_rds16(r, collider + AE_MISSB_BODY_POSITION_Z);
    int32_t dz = missb_boxes_abs_delta(ae_rd32(r, point + AE_MISSB_POINT_Z), center_z);
    int32_t half_z = ae_rds16(r, half_sizes + AE_MISSB_HALF_SIZE_Z);
    if (dz >= half_z) return 0;

    int32_t center_x = ae_rds16(r, collider + AE_MISSB_BODY_POSITION_X);
    int32_t dx = missb_boxes_abs_delta(ae_rd32(r, point + AE_MISSB_POINT_X), center_x);
    int32_t half_x = ae_rds16(r, half_sizes + AE_MISSB_HALF_SIZE_X);
    if (dx >= half_x) return 0;

    int32_t center_y = ae_rds16(r, collider + AE_MISSB_BODY_POSITION_Y);
    int32_t half_y = ae_rds16(r, collider + AE_MISSB_BODY_HEIGHT);
    int32_t point_y = ae_rds32(r, point + AE_MISSB_POINT_Y);
    uint32_t upper = (uint32_t)center_y + (uint32_t)half_y;
    uint32_t lower = (uint32_t)center_y - (uint32_t)half_y;
    if (point_y >= missb_boxes_s32(upper)) return 0;
    if (point_y <= missb_boxes_s32(lower)) return 0;
    return 1;
}

/* 8009091C: horizontal edges are inclusive.  The vertical range runs from
 * centerY - halfHeight through centerY. */
int32_t ae_ColliderBoxContainsPoint(AeRam *r, uint32_t collider,
                                    int32_t x, int32_t y, int32_t z)
{
    int32_t center_x = ae_rds16(r, collider + AE_MISSB_COLLIDER_CENTER_X);
    int32_t half_x = ae_rds16(r, collider + AE_MISSB_COLLIDER_HALF_X);
    int32_t upper_x = missb_boxes_s32((uint32_t)center_x + (uint32_t)half_x);
    int32_t lower_x = missb_boxes_s32((uint32_t)center_x - (uint32_t)half_x);
    if (upper_x < x || x < lower_x) return 0;

    int32_t center_z = ae_rds16(r, collider + AE_MISSB_COLLIDER_CENTER_Z);
    int32_t half_z = ae_rds16(r, collider + AE_MISSB_COLLIDER_HALF_Z);
    int32_t upper_z = missb_boxes_s32((uint32_t)center_z + (uint32_t)half_z);
    int32_t lower_z = missb_boxes_s32((uint32_t)center_z - (uint32_t)half_z);
    if (upper_z < z || z < lower_z) return 0;

    int32_t center_y = ae_rds16(r, collider + AE_MISSB_COLLIDER_CENTER_Y);
    int32_t half_y = ae_rds16(r, collider + AE_MISSB_COLLIDER_HALF_Y);
    int32_t lower_y = missb_boxes_s32((uint32_t)center_y - (uint32_t)half_y);
    return y < lower_y || y > center_y ? 0 : 1;
}

/* 80090754: test the player, RC car, collision bodies, then optional captured
 * monkeys against the supplied collision box. */
int32_t ae_GimmickTouchCheck(AeRam *r, int32_t include_monkeys,
                             uint32_t collider, int32_t unused2,
                             int32_t unused3)
{
    (void)unused2;
    (void)unused3;
    uint32_t player = AE_MISSB_PLAYER;
    int32_t player_x = ae_rds32(r, player + AE_MISSB_ACTOR_POSITION);
    int32_t player_y = ae_rds32(r, player + AE_MISSB_ACTOR_POSITION_Y);
    int32_t player_z = ae_rds32(r, player + AE_MISSB_ACTOR_POSITION_Z);
    if (ae_ColliderBoxContainsPoint(r, collider, player_x, player_y, player_z)) return 1;

    uint32_t rc_car = AE_MISSB_RCCAR;
    if (ae_rd32(r, rc_car + AE_MISSB_RCCAR_STATE) != 0u) {
        if (ae_ColliderBoxContainsPoint(r, collider,
                ae_rds32(r, rc_car + AE_MISSB_ACTOR_POSITION),
                ae_rds32(r, rc_car + AE_MISSB_ACTOR_POSITION_Y),
                ae_rds32(r, rc_car + AE_MISSB_ACTOR_POSITION_Z))) return 1;
    }

    uint32_t state = AE_MISSB_GAME_STATE;
    int32_t signed_body_count = ae_rds16(r, state + AE_MISSB_COLLIDER_COUNT);
    uint32_t body_count = signed_body_count > 0 ? (uint32_t)signed_body_count : 0u;
    uint32_t bodies = ae_rd32(r, state + AE_MISSB_COLLIDER_POINTER);
    int32_t box_x = ae_rds16(r, collider + AE_MISSB_COLLIDER_CENTER_X);
    int32_t box_y = ae_rds16(r, collider + AE_MISSB_COLLIDER_CENTER_Y);
    int32_t box_z = ae_rds16(r, collider + AE_MISSB_COLLIDER_CENTER_Z);
    int32_t box_half_x = ae_rds16(r, collider + AE_MISSB_COLLIDER_HALF_X);
    int32_t box_half_y = ae_rds16(r, collider + AE_MISSB_COLLIDER_HALF_Y);
    int32_t box_half_z = ae_rds16(r, collider + AE_MISSB_COLLIDER_HALF_Z);
    for (uint32_t i = body_count; i != 0u; --i) {
        uint32_t body = bodies + (i - 1u) * AE_MISSB_COLLIDER_STRIDE;
        if (ae_rd8(r, body + AE_MISSB_COLLIDER_KIND) != 1u) continue;

        int32_t dx = missb_boxes_abs_delta((uint32_t)(int32_t)ae_rds16(r, body), box_x);
        int32_t dz = missb_boxes_abs_delta(
            (uint32_t)(int32_t)ae_rds16(r, body + AE_MISSB_COLLIDER_CENTER_Z), box_z);
        int32_t body_half_x = ae_rds16(r, body + AE_MISSB_COLLIDER_HALF_X);
        int32_t body_half_z = ae_rds16(r, body + AE_MISSB_COLLIDER_HALF_Z);
        if (dx >= body_half_x + box_half_x || dz >= body_half_z + box_half_z) continue;

        int32_t body_y = ae_rds16(r, body + AE_MISSB_COLLIDER_CENTER_Y);
        if (body_y >= box_y - box_half_y && body_y <= box_y) return 1;
    }

    if (include_monkeys == 0) return 0;
    uint32_t monkey = AE_MISSB_MONKEYS;
    for (uint32_t i = 0; i < 8u; ++i, monkey += AE_MISSB_MONKEY_STRIDE) {
        if (ae_rd8(r, monkey + AE_MISSB_MONKEY_ACTIVITY) != 4u) continue;
        if (ae_ColliderBoxContainsPoint(r, collider,
                ae_rds32(r, monkey + AE_MISSB_MONKEY_POSITION),
                ae_rds32(r, monkey + AE_MISSB_ACTOR_POSITION_Y),
                ae_rds32(r, monkey + AE_MISSB_ACTOR_POSITION_Z))) return 1;
    }
    return 0;
}

/* 8008F308: build the local rotation vector for a swinging prop and write its
 * rotation matrix through the already ported LIBGTE routine. */
int32_t ae_SwingDoorBodyDelta(AeRam *r, uint32_t door, int32_t prop_index)
{
    uint32_t props = ae_rd32(r, AE_MISSB_AREA_PROPS);
    uint32_t prop = props + (uint32_t)prop_index * AE_MISSB_AREA_PROP_STRIDE;
    uint32_t model = ae_rd32(r, prop);
    uint32_t prop_type = ae_rd32(r, model) & 0xFu;
    if (prop_type != 3u) return 0;

    uint32_t stack = AE_MISSB_DEFAULT_STACK - 32u;
    uint32_t angles = stack + 16u;
    ae_wr16(r, angles, 0u);
    ae_wr16(r, angles + AE_MISSB_SVECTOR_Y, 0u);
    ae_wr16(r, angles + AE_MISSB_SVECTOR_Z, 0u);

    int32_t state = ae_rds16(r, door + AE_MISSB_DOOR_STATE);
    int32_t axis = ae_rds16(r, door + AE_MISSB_DOOR_AXIS);
    int32_t distance = ae_rds16(r, door + AE_MISSB_DOOR_DISTANCE);
    if (state == 1 || state == 101) {
        int32_t y_rotation = (axis & 1) != 0 ? distance : -distance;
        ae_wr16(r, angles + AE_MISSB_SVECTOR_Y, (uint32_t)y_rotation);
    } else if (state == 3 || state == 20) {
        if (((uint32_t)(axis - 60) & 1u) == 0u) {
            int32_t x = axis == 60 ? distance : -distance;
            ae_wr16(r, angles, (uint32_t)x);
        } else {
            int32_t z = axis == 61 ? -distance : distance;
            ae_wr16(r, angles + AE_MISSB_SVECTOR_Z, (uint32_t)z);
        }
    } else {
        return 0;
    }

    (void)ae_sdk_RotMatrixZYX(r, angles,
        ae_rd32(r, prop + AE_MISSB_AREA_PROP_ROTATION));
    return 0;
}

/* 8008FAD0: move the collider and its area prop along the direction table. */
int32_t ae_SlideDoorMove(AeRam *r, uint32_t door, int32_t direction,
                         int32_t delta)
{
    int32_t prop_index = ae_rds16(r, door + AE_MISSB_DOOR_PROP_INDEX);
    if (prop_index < 0) return 0;

    uint32_t prop = ae_rd32(r, AE_MISSB_AREA_PROPS) +
                    (uint32_t)prop_index * AE_MISSB_AREA_PROP_STRIDE;
    uint32_t direction_index = (uint32_t)(int16_t)direction;
    uint32_t step = (uint32_t)delta;
    if (direction_index >= 8u && direction_index < 16u) {
        ae_wr16(r, door + AE_MISSB_DOOR_Y,
                ae_rd16(r, door + AE_MISSB_DOOR_Y) + step);
        ae_wr16(r, prop + AE_MISSB_AREA_PROP_POSITION_Y,
                ae_rd16(r, prop + AE_MISSB_AREA_PROP_POSITION_Y) + step);
    }

    switch (direction_index) {
    case 0: case 5: case 8: case 13:
        ae_wr16(r, door, ae_rd16(r, door) + step);
        ae_wr16(r, prop + AE_MISSB_AREA_PROP_POSITION_X,
                ae_rd16(r, prop + AE_MISSB_AREA_PROP_POSITION_X) + step);
        break;
    case 1: case 4: case 9: case 12:
        ae_wr16(r, door, ae_rd16(r, door) - step);
        ae_wr16(r, prop + AE_MISSB_AREA_PROP_POSITION_X,
                ae_rd16(r, prop + AE_MISSB_AREA_PROP_POSITION_X) - step);
        break;
    case 2: case 7: case 10: case 15:
        ae_wr16(r, door + AE_MISSB_DOOR_Z,
                ae_rd16(r, door + AE_MISSB_DOOR_Z) - step);
        ae_wr16(r, prop + AE_MISSB_AREA_PROP_POSITION_Z,
                ae_rd16(r, prop + AE_MISSB_AREA_PROP_POSITION_Z) - step);
        break;
    case 3: case 6: case 11: case 14:
        ae_wr16(r, door + AE_MISSB_DOOR_Z,
                ae_rd16(r, door + AE_MISSB_DOOR_Z) + step);
        ae_wr16(r, prop + AE_MISSB_AREA_PROP_POSITION_Z,
                ae_rd16(r, prop + AE_MISSB_AREA_PROP_POSITION_Z) + step);
        break;
    default:
        break;
    }
    return 0;
}
