#include "../ui/ae_missb.h"

#include "ae_areaset.h"
#include "ae_zako_init.h"
#include "../core/ae_sdk_matrix.h"
#include "ae_zones.h"
#include "ae_world.h"

#define MBG_STAGE_STATE             0x800F4350u
#define MBG_STAGE_AREA              0x126u
#define MBG_STAGE_LEVEL             0x124u
#define MBG_STAGE_AREA_COUNT        0x128u
#define MBG_MONKEY_BASE             0x800E5500u
#define MBG_MONKEY_STRIDE           0x250u
#define MBG_MONKEY_ACTIVITY         0x7Au
#define MBG_AREA_PROPS              0x800B4D64u
#define MBG_AREA_PROP_STRIDE        0x20u
#define MBG_AREA_PROP_ROTATION      0x1Cu
#define MBG_AREA_PROP_POSITION_Y    0x12u
#define MBG_AREA_PROP_SCALE         0x06u
#define MBG_AREA_PROP_SCALE_AXIS    0x08u
#define MBG_PLAYER                  0x800EC1E0u
#define MBG_PLAYER_POSITION_X       0x24u
#define MBG_PLAYER_POSITION_Y       0x28u
#define MBG_PLAYER_POSITION_Z       0x2Cu
#define MBG_PLAYER_ATTACH_FLAGS     0xE0u
#define MBG_PLAYER_AREA             0x800EC238u
#define MBG_COLLIDER_LIST           0x800B4D64u
#define MBG_COLLIDER_POINTER        0x20Cu
#define MBG_SPECIAL_ACTOR_LIST      0x800B4D70u
#define MBG_STAGE_SPECIAL_ACTORS    0x220u
#define MBG_SPECIAL_ACTOR_STRIDE    24u
#define MBG_SPECIAL_LINK_AREA       0x04u
#define MBG_SPECIAL_LINK_KIND       0x00u
#define MBG_SPECIAL_LINK_NEXT       0x0Au
#define MBG_SPECIAL_LINKED_KIND     11u
#define MBG_COLLIDER_POS_X          0x00u
#define MBG_COLLIDER_POS_Y          0x02u
#define MBG_COLLIDER_POS_Z          0x04u
#define MBG_COLLIDER_HALF_X         0x06u
#define MBG_COLLIDER_HALF_Y         0x08u
#define MBG_COLLIDER_HALF_Z         0x0Au
#define MBG_COLLIDER_PROP_INDEX     0x0Eu
#define MBG_COLLIDER_SCALE_X        0x32u
#define MBG_COLLIDER_SCALE_Y        0x34u
#define MBG_COLLIDER_SCALE_Z        0x36u
#define MBG_COLLIDER_TYPE           0x12u
#define MBG_COLLIDER_STATE          0x18u
#define MBG_COLLIDER_RECORD_STRIDE  56u
#define MBG_GIMMICK_STAGE_SLOT      0x290u
#define MBG_GIMMICK_COLLIDER_INDEX  0x04u
#define MBG_GIMMICK_HEIGHT_OFFSET   0x16u
#define MBG_POINT_Y                 0x04u
#define MBG_POINT_Z                 0x08u
#define MBG_VECTOR_Y                0x02u
#define MBG_VECTOR_Z                0x04u
#define MBG_MONKEY_POSITION_X       0x24u
#define MBG_MONKEY_POSITION_Y       0x28u
#define MBG_MONKEY_POSITION_Z       0x2Cu
#define MBG_BOX_MONKEY_BASE         0x800E5500u
#define MBG_BOX_MONKEY_STRIDE       0x250u
#define MBG_BOX_MONKEY_ACTIVITY     0x7Au
#define MBG_DEFAULT_STACK           0x801FFF00u

static int32_t mbg_sra(int32_t value, uint32_t shift)
{
    if (shift == 0u) return value;
    uint32_t bits = (uint32_t)value;
    uint32_t result = bits >> shift;
    if ((bits & 0x80000000u) != 0u) result |= 0xFFFFFFFFu << (32u - shift);
    return (int32_t)result;
}

static int32_t mbg_fixed_mul(int32_t value, int32_t scale)
{
    int32_t product = (int32_t)((uint32_t)value * (uint32_t)scale);
    if (product < 0) product = (int32_t)((uint32_t)product + 4095u);
    return mbg_sra(product, 12u);
}

static uint32_t mbg_stack_vector(const AeMissbHooks *h)
{
    uint32_t stack = h != NULL && h->stack_pointer != 0u
        ? h->stack_pointer : MBG_DEFAULT_STACK;
    return stack - 32u + 16u;
}

static int32_t mbg_area_level(AeRam *r, const AeMissbHooks *h, int32_t area)
{
    if (h == NULL || h->area_set == NULL) {
        ae_ram_fail(r, "AeMissbHooks.area_set not provided for AreaGetLevel");
        return 0;
    }
    return ae_AreaGetLevel(r, h->area_set, area);
}

static int32_t mbg_active_monkeys(AeRam *r, uint32_t monkeys)
{
    int32_t count = 0;
    for (uint32_t i = 0; i < 8u; ++i) {
        uint8_t activity = ae_rd8(r, monkeys + i * MBG_MONKEY_STRIDE + MBG_MONKEY_ACTIVITY);
        if (activity == 2u || activity == 3u) ++count;
    }
    return count;
}

static int32_t mbg_active_monkey_records(AeRam *r, uint32_t records)
{
    int32_t count = 0;
    for (uint32_t i = 0; i < 8u; ++i) {
        uint8_t activity = ae_rd8(r, records + i);
        if (activity == 2u || activity == 3u) ++count;
    }
    return count;
}

/* 80097504: count active monkeys across the area's level rows, using the
 * live monkey slots for the currently loaded area and saved records elsewhere. */
int32_t ae_AreaAggregateActiveMonkeyCount(AeRam *r, const AeMissbHooks *h)
{
    uint32_t state = MBG_STAGE_STATE;
    int32_t current_area = ae_rds16(r, state + MBG_STAGE_AREA);
    int32_t level = mbg_area_level(r, h, current_area);
    int32_t first_area = ae_GetLevelFirstArea(r, level);
    uint32_t area_count = ae_rd16(r, state + MBG_STAGE_AREA_COUNT);
    int32_t total = 0;
    uint32_t stack = h != NULL && h->stack_pointer != 0u
        ? h->stack_pointer : MBG_DEFAULT_STACK;
    uint32_t out_count = stack - 48u + 16u;

    for (uint32_t i = 0; i < area_count; ++i) {
        int32_t area = first_area + (int32_t)i;
        if (current_area == area) {
            total += mbg_active_monkeys(r, MBG_MONKEY_BASE);
        } else {
            uint32_t records = (uint32_t)ae_missb_call(
                r, h, "AreaGetMonkeyRecord", 0x80097E88u, 2u,
                (uint32_t)area, out_count, 0u, 0u);
            total += mbg_active_monkey_records(r, records);
        }
    }
    return total;
}

/* 8009B3B0 */
void ae_GimmickCylinderSetPlayerFlag(AeRam *r, int32_t unused)
{
    (void)unused;
    ae_wr32(r, MBG_PLAYER + MBG_PLAYER_ATTACH_FLAGS,
            ae_rd32(r, MBG_PLAYER + MBG_PLAYER_ATTACH_FLAGS) | 0x10u);
}

/* 80091598 */
int32_t ae_GimmickBreakableFloorAdjust(AeRam *r, const AeMissbHooks *h,
                                       uint32_t collider, uint32_t gimmick)
{
    uint32_t state = MBG_STAGE_STATE;
    ae_wr16(r, state + MBG_GIMMICK_STAGE_SLOT,
            ae_rd16(r, gimmick + MBG_GIMMICK_COLLIDER_INDEX));

    int32_t height_y = ae_rds16(r, collider + MBG_COLLIDER_POS_Y) +
                       ae_rds16(r, gimmick + MBG_GIMMICK_HEIGHT_OFFSET) + 8;
    if (h == NULL || h->world == NULL) {
        ae_ram_fail(r, "AeMissbHooks.world not provided for GetGroundHeight");
        return 0;
    }
    int32_t height = ae_GetGroundHeight(
        r, h->world, ae_rds16(r, collider + MBG_COLLIDER_POS_X),
        height_y, ae_rds16(r, collider + MBG_COLLIDER_POS_Z));

    int32_t old_offset = ae_rds16(r, gimmick + MBG_GIMMICK_HEIGHT_OFFSET);
    int32_t target_y = ae_rds16(r, collider + MBG_COLLIDER_POS_Y) + old_offset + 8;
    int32_t prop_index = ae_rds16(r, collider + MBG_COLLIDER_PROP_INDEX);
    if (height < target_y) {
        ae_wr16(r, gimmick + MBG_GIMMICK_HEIGHT_OFFSET, 0u);
        ae_wr16(r, collider + MBG_COLLIDER_POS_Y, (uint32_t)height);
        if (prop_index >= 0) {
            uint32_t prop = ae_rd32(r, MBG_AREA_PROPS) +
                (uint32_t)prop_index * MBG_AREA_PROP_STRIDE;
            ae_wr16(r, prop + MBG_AREA_PROP_POSITION_Y, (uint32_t)height);
        }
    } else {
        int32_t next_offset = (int32_t)(uint16_t)(old_offset + 8);
        ae_wr16(r, gimmick + MBG_GIMMICK_HEIGHT_OFFSET, (uint32_t)next_offset);
        uint16_t y = (uint16_t)(ae_rd16(r, collider + MBG_COLLIDER_POS_Y) + next_offset);
        ae_wr16(r, collider + MBG_COLLIDER_POS_Y, y);
        if (prop_index >= 0) {
            uint32_t prop = ae_rd32(r, MBG_AREA_PROPS) +
                (uint32_t)prop_index * MBG_AREA_PROP_STRIDE;
            ae_wr16(r, prop + MBG_AREA_PROP_POSITION_Y,
                    ae_rd16(r, prop + MBG_AREA_PROP_POSITION_Y) + next_offset);
        }
    }
    return 0;
}

/* 80091AF0: apply the prop's one-axis scaling to the matching collider size,
 * then hand its diagonal rotation matrix to the existing SDK helper. */
int32_t ae_GimmickApplyPropScale(AeRam *r, const AeMissbHooks *h,
                                 uint32_t collider, uint32_t prop)
{
    uint32_t vector = mbg_stack_vector(h);
    int32_t scale = ae_rds16(r, prop + MBG_AREA_PROP_SCALE);
    uint16_t prop_axis = ae_rd16(r, prop + MBG_AREA_PROP_SCALE_AXIS);
    ae_wr16(r, vector, 0x1000u);
    ae_wr16(r, vector + MBG_VECTOR_Y, 0x1000u);
    ae_wr16(r, vector + MBG_VECTOR_Z, 0x1000u);
    if (prop_axis == 0u) {
        ae_wr16(r, vector, (uint32_t)scale);
        ae_wr16(r, collider + MBG_COLLIDER_HALF_X,
                (uint32_t)mbg_fixed_mul(ae_rds16(r, collider + MBG_COLLIDER_SCALE_X), scale));
    } else if (prop_axis == 1u) {
        ae_wr16(r, vector + MBG_VECTOR_Z, (uint32_t)scale);
        ae_wr16(r, collider + MBG_COLLIDER_HALF_Z,
                (uint32_t)mbg_fixed_mul(ae_rds16(r, collider + MBG_COLLIDER_SCALE_Z), scale));
    } else {
        ae_wr16(r, vector + MBG_VECTOR_Y, (uint32_t)scale);
        ae_wr16(r, collider + MBG_COLLIDER_HALF_Y,
                (uint32_t)mbg_fixed_mul(ae_rds16(r, collider + MBG_COLLIDER_SCALE_Y), scale));
    }

    int32_t prop_index = ae_rds16(r, collider + MBG_COLLIDER_PROP_INDEX);
    if (prop_index >= 0) {
        uint32_t model_prop = ae_rd32(r, MBG_AREA_PROPS) +
            (uint32_t)prop_index * MBG_AREA_PROP_STRIDE;
        ae_SvecToDiagMatrix(r, vector,
                            ae_rd32(r, model_prop + MBG_AREA_PROP_ROTATION));
    }
    return 0;
}

/* 80091C04: update a prop's rotation from one of the nine direction slots. */
int32_t ae_GimmickSetPropRotation(AeRam *r, uint32_t direction,
                                  int32_t prop_index, int32_t amount)
{
    int32_t index = (int16_t)(uint16_t)prop_index;
    if (index < 0) return 0;
    uint32_t prop = ae_rd32(r, MBG_AREA_PROPS) +
                    (uint32_t)index * MBG_AREA_PROP_STRIDE;
    uint32_t model = ae_rd32(r, prop);
    if ((ae_rd32(r, model) & 0xFu) != 3u) return 0;

    int32_t slot = (int16_t)(uint16_t)direction;
    if (slot < 0 || slot >= 9) return 0;
    uint32_t vector = MBG_DEFAULT_STACK - 32u + 16u;
    ae_wr16(r, vector, 0u);
    ae_wr16(r, vector + MBG_VECTOR_Y, 0u);
    ae_wr16(r, vector + MBG_VECTOR_Z, 0u);
    int32_t value = (int16_t)(uint16_t)amount;
    switch (slot) {
    case 0: case 3:
        ae_wr16(r, vector + MBG_VECTOR_Z, (uint32_t)value);
        break;
    case 1: case 2:
        ae_wr16(r, vector + MBG_VECTOR_Z, (uint32_t)-value);
        break;
    case 4: case 7:
        ae_wr16(r, vector, (uint32_t)value);
        break;
    case 5: case 6:
        ae_wr16(r, vector, (uint32_t)-value);
        break;
    case 8:
        ae_wr16(r, vector + MBG_VECTOR_Y, (uint32_t)value);
        break;
    }
    (void)ae_sdk_RotMatrixZYX(r, vector,
        ae_rd32(r, prop + MBG_AREA_PROP_ROTATION));
    return 0;
}

/* 80093848 */
int32_t ae_GimmickGetGrabOffset(AeRam *r, uint32_t mode,
                                uint32_t collider, uint32_t out_vector)
{
    uint32_t player = MBG_PLAYER;
    if (!ae_ColliderBoxContainsPoint(r, collider,
            ae_rds32(r, player + MBG_PLAYER_POSITION_X),
            ae_rds32(r, player + MBG_PLAYER_POSITION_Y),
            ae_rds32(r, player + MBG_PLAYER_POSITION_Z))) return 0;

    int32_t signed_mode = (int16_t)(uint16_t)mode;
    if (signed_mode == 1) {
        ae_wr16(r, out_vector, 0u);
        ae_wr16(r, out_vector + MBG_VECTOR_Y, 0u);
        int32_t dir = ae_rds16(r, collider + MBG_COLLIDER_POS_Z) <
            ae_rds32(r, player + MBG_PLAYER_POSITION_Z) ? -0x1000 : 0x1000;
        ae_wr16(r, out_vector + MBG_VECTOR_Z, (uint32_t)dir);
    } else {
        ae_wr16(r, out_vector + MBG_VECTOR_Y, 0u);
        ae_wr16(r, out_vector + MBG_VECTOR_Z, 0u);
        int32_t dir = ae_rds16(r, collider + MBG_COLLIDER_POS_X) <
            ae_rds32(r, player + MBG_PLAYER_POSITION_X) ? -0x1000 : 0x1000;
        ae_wr16(r, out_vector, (uint32_t)dir);
    }
    return 1;
}

/* 80094330: test a point against a box expanded by signed horizontal offsets. */
int32_t ae_GimmickPointWithinExpandedBox(AeRam *r, uint32_t collider,
                                         uint32_t point, int32_t x_offset,
                                         int32_t z_offset)
{
    int32_t center_x = ae_rds16(r, collider + MBG_COLLIDER_POS_X);
    int32_t half_x = ae_rds16(r, collider + MBG_COLLIDER_HALF_X);
    int32_t x = ae_rds32(r, point) - (int16_t)(uint16_t)x_offset;
    if (center_x + half_x < x || x < center_x - half_x) return 0;

    int32_t center_z = ae_rds16(r, collider + MBG_COLLIDER_POS_Z);
    int32_t half_z = ae_rds16(r, collider + MBG_COLLIDER_HALF_Z);
    int32_t z = ae_rds32(r, point + MBG_POINT_Z) - (int16_t)(uint16_t)z_offset;
    if (center_z + half_z < z || z < center_z - half_z) return 0;

    int32_t center_y = ae_rds16(r, collider + MBG_COLLIDER_POS_Y);
    int32_t half_y = ae_rds16(r, collider + MBG_COLLIDER_HALF_Y);
    int32_t y = ae_rds32(r, point + MBG_POINT_Y);
    return y < center_y - half_y || center_y < y ? 0 : 1;
}

/* 80094A48 */
int32_t ae_GimmickLinkedAreaActive(AeRam *r, int32_t index)
{
    if (index < 0) return 0;
    uint32_t state = MBG_STAGE_STATE;
    uint32_t records = ae_rd32(r, state + MBG_STAGE_SPECIAL_ACTORS);
    uint32_t record = records + (uint32_t)index * MBG_SPECIAL_ACTOR_STRIDE;
    if (ae_rds16(r, record + MBG_SPECIAL_LINK_AREA) == ae_rds16(r, MBG_PLAYER_AREA)) return 1;
    if (ae_rds16(r, record + MBG_SPECIAL_LINK_KIND) != MBG_SPECIAL_LINKED_KIND) return 0;
    return ae_GimmickLinkedAreaActive(r, ae_rds16(r, record + MBG_SPECIAL_LINK_NEXT));
}

/* 80094E5C: any captured monkey overlapping the collider counts as contact. */
int32_t ae_GimmickMonkeyTouchingBox(AeRam *r, uint32_t collider)
{
    int32_t center_x = ae_rds16(r, collider + MBG_COLLIDER_POS_X);
    int32_t center_y = ae_rds16(r, collider + MBG_COLLIDER_POS_Y);
    int32_t center_z = ae_rds16(r, collider + MBG_COLLIDER_POS_Z);
    int32_t half_x = ae_rds16(r, collider + MBG_COLLIDER_HALF_X);
    int32_t half_y = ae_rds16(r, collider + MBG_COLLIDER_HALF_Y);
    int32_t half_z = ae_rds16(r, collider + MBG_COLLIDER_HALF_Z);
    uint32_t monkey = MBG_BOX_MONKEY_BASE;
    for (uint32_t i = 0; i < 8u; ++i, monkey += MBG_BOX_MONKEY_STRIDE) {
        if (ae_rd8(r, monkey + MBG_BOX_MONKEY_ACTIVITY) != 4u) continue;
        int32_t x = ae_rds32(r, monkey + MBG_MONKEY_POSITION_X);
        int32_t y = ae_rds32(r, monkey + MBG_MONKEY_POSITION_Y);
        int32_t z = ae_rds32(r, monkey + MBG_MONKEY_POSITION_Z);
        int32_t dx = x - center_x;
        int32_t dz = z - center_z;
        if (dx < 0) dx = -dx;
        if (dz < 0) dz = -dz;
        if (dx >= half_x || dz >= half_z) continue;
        if (y >= center_y - half_y - 16 && y < center_y) return 1;
    }
    return 0;
}
