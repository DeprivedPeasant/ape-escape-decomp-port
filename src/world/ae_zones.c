/* Readable RAM-image ports of terrain/collider masks and debug-menu setup. */
#include "ae_zones.h"

#include <stdint.h>

#define AE_ZONES_DEBUG_COUNTER2_TABLE_GLOBAL AE_ZONES_DEBUG_COUNTER2_ITEM_GLOBAL
#define AE_ZONES_STACK_TEMP_VECTOR 0x801FF000u
#define AE_ZONES_MATRIX_DIAGONAL_X 0x00u
#define AE_ZONES_MATRIX_DIAGONAL_Y 0x08u
#define AE_ZONES_MATRIX_DIAGONAL_Z 0x10u
#define AE_ZONES_MATRIX_OFF_DIAGONAL_01 0x02u
#define AE_ZONES_MATRIX_OFF_DIAGONAL_02 0x04u
#define AE_ZONES_MATRIX_OFF_DIAGONAL_10 0x06u
#define AE_ZONES_MATRIX_OFF_DIAGONAL_12 0x0Au
#define AE_ZONES_MATRIX_OFF_DIAGONAL_20 0x0Cu
#define AE_ZONES_MATRIX_OFF_DIAGONAL_21 0x0Eu

static int32_t ae_zones_sra32(int32_t value, unsigned shift)
{
    uint32_t bits = (uint32_t)value;
    if (shift == 0u) {
        return value;
    }
    if (value >= 0) {
        return (int32_t)(bits >> shift);
    }
    return (int32_t)((bits >> shift) | (~UINT32_C(0) << (32u - shift)));
}

static int32_t ae_zones_signed_product_high(int32_t left, int32_t right)
{
    int64_t product = (int64_t)left * (int64_t)right;
    uint64_t bits = (uint64_t)product;
    return (int32_t)(uint32_t)(bits >> 32);
}

/* Exact signed-multiply sequences used by ColliderZoneMasksUpdate. */
static int32_t ae_zones_divide_by_360(int32_t value)
{
    int32_t high = ae_zones_signed_product_high(value, (int32_t)UINT32_C(0xB60B60B7));
    int32_t adjusted = (int32_t)((uint32_t)high + (uint32_t)value);
    return ae_zones_sra32(adjusted, 7u) - ae_zones_sra32(value, 31u);
}

static int32_t ae_zones_divide_by_5(int32_t value)
{
    int32_t high = ae_zones_signed_product_high(value, INT32_C(0x66666667));
    return ae_zones_sra32(high, 1u) - ae_zones_sra32(value, 31u);
}

static int32_t ae_zones_hook_missing(AeRam *r, const char *name)
{
    ae_ram_fail(r, "ae_zones: required hook %s is not provided", name);
    return 0;
}

void ae_SvecToDiagMatrix(AeRam *r, uint32_t vector, uint32_t matrix)
{
    uint32_t x = ae_rd16(r, vector + AE_ZONES_SVECTOR_X);
    ae_wr16(r, matrix + AE_ZONES_MATRIX_OFF_DIAGONAL_01, 0);
    ae_wr16(r, matrix + AE_ZONES_MATRIX_OFF_DIAGONAL_02, 0);
    ae_wr16(r, matrix + AE_ZONES_MATRIX_DIAGONAL_X, x);

    uint32_t y = ae_rd16(r, vector + AE_ZONES_SVECTOR_Y);
    ae_wr16(r, matrix + AE_ZONES_MATRIX_OFF_DIAGONAL_10, 0);
    ae_wr16(r, matrix + AE_ZONES_MATRIX_OFF_DIAGONAL_12, 0);
    ae_wr16(r, matrix + AE_ZONES_MATRIX_DIAGONAL_Y, y);

    uint32_t z = ae_rd16(r, vector + AE_ZONES_SVECTOR_Z);
    ae_wr16(r, matrix + AE_ZONES_MATRIX_OFF_DIAGONAL_20, 0);
    ae_wr16(r, matrix + AE_ZONES_MATRIX_OFF_DIAGONAL_21, 0);
    ae_wr16(r, matrix + AE_ZONES_MATRIX_DIAGONAL_Z, z);
}

uint32_t ae_BuildTerrainBlockMask(AeRam *r, const AeZonesHooks *h)
{
    uint32_t width = ae_rd16(r, AE_ZONES_G_LEVEL_GRID + AE_ZONES_GRID_WIDTH);
    uint32_t depth = ae_rd16(r, AE_ZONES_G_LEVEL_GRID + AE_ZONES_GRID_DEPTH);
    uint32_t mask_size = (width >> 1) * (depth >> 1);
    uint32_t mask;
    uint32_t block_z;

    if (h == NULL || h->area_bump_alloc == NULL) {
        return (uint32_t)ae_zones_hook_missing(r, "AreaBumpAlloc");
    }
    mask = h->area_bump_alloc(h->user, r, mask_size);
    block_z = 0;
    while (block_z < (ae_rd16(r, AE_ZONES_G_LEVEL_GRID + AE_ZONES_GRID_DEPTH) >> 1)) {
        uint32_t block_x;
        block_x = 0;
        while (block_x < (ae_rd16(r, AE_ZONES_G_LEVEL_GRID + AE_ZONES_GRID_WIDTH) >> 1)) {
            uint32_t row_width = ae_rd16(r, AE_ZONES_G_LEVEL_GRID + AE_ZONES_GRID_WIDTH);
            uint32_t cells = ae_rd32(r, AE_ZONES_G_LEVEL_GRID + AE_ZONES_GRID_CELLS);
            uint32_t cell_index = block_z * (row_width * 2u) + block_x * 2u;
            uint32_t lower_row_index = cell_index + row_width;
            uint32_t cell_address = cells + cell_index * 4u;
            uint32_t lower_row_address = cells + lower_row_index * 4u;
            int is_empty = ((ae_rd32(r, cell_address) & 1u) == 0u) &&
                           ((ae_rd32(r, cell_address + 4u) & 1u) == 0u) &&
                           ((ae_rd32(r, lower_row_address) & 1u) == 0u) &&
                           ((ae_rd32(r, lower_row_address + 4u) & 1u) == 0u);
            uint32_t row_product = block_z * row_width;
            int32_t output_row = (int32_t)row_product / 2;
            uint32_t output_index = (uint32_t)output_row + block_x;
            ae_wr8(r, mask + output_index, is_empty ? 0xFFu : 0u);
            ++block_x;
        }
        ++block_z;
    }
    return mask;
}

int32_t ae_ReadSizedValue(AeRam *r, uint32_t address, int32_t size)
{
    if (size == 0) {
        return ae_rds8(r, address);
    }
    if (size == 1) {
        return ae_rds16(r, address);
    }
    if (size == 2) {
        return ae_rds32(r, address);
    }
    if (size == 3) {
        return (int32_t)ae_rd8(r, address);
    }
    return size < 2 ? 1 : 3;
}

int32_t ae_MenuItemInitBound(AeRam *r, uint32_t item, uint32_t variable, int32_t size)
{
    int32_t value;
    ae_wr8(r, item + AE_ZONES_MENU_ITEM_VALUE_SIZE, (uint32_t)size);
    ae_wr32(r, item + AE_ZONES_MENU_ITEM_CALLBACK, AE_ZONES_MENU_ITEM_DEFAULT_CALLBACK);
    ae_wr32(r, item + AE_ZONES_MENU_ITEM_VARIABLE, variable);
    value = ae_ReadSizedValue(r, variable, size);
    ae_wr32(r, item + AE_ZONES_MENU_ITEM_VALUE, (uint32_t)value);
    ae_wr32(r, item + AE_ZONES_MENU_ITEM_MINIMUM, (uint32_t)-9999);
    ae_wr32(r, item + AE_ZONES_MENU_ITEM_MAXIMUM, 9999u);
    ae_wr32(r, item + AE_ZONES_MENU_ITEM_STEP, 1u);
    return 1;
}

void ae_MenuItemSetCallback(AeRam *r, uint32_t item, uint32_t callback)
{
    ae_wr32(r, item + AE_ZONES_MENU_ITEM_CALLBACK, callback);
}

void ae_DebugMenuInitFlags(AeRam *r)
{
    ae_wr32(r, AE_ZONES_DEBUG_FLAGS_ITEM_GLOBAL, AE_ZONES_DEBUG_FLAGS_ITEM);
    ae_MenuItemInitBound(r, AE_ZONES_DEBUG_FLAGS_ITEM, AE_ZONES_DEBUG_FLAGS_VARIABLE, 1);
    ae_MenuItemSetCallback(r, AE_ZONES_DEBUG_FLAGS_ITEM, AE_ZONES_DEBUG_FLAGS_CALLBACK);
}

void ae_DebugMenuInitPlayerParams(AeRam *r)
{
    uint32_t index;
    for (index = 0; index < AE_ZONES_DEBUG_PLAYER_ITEM_COUNT; ++index) {
        uint32_t item = AE_ZONES_DEBUG_PLAYER_ITEMS + index * AE_ZONES_MENU_ITEM_SIZE;
        uint32_t variable = AE_ZONES_DEBUG_PLAYER_VARIABLES + index * 4u;
        ae_MenuItemInitBound(r, item, variable, 2);
    }
    ae_MenuItemSetCallback(r, AE_ZONES_DEBUG_PLAYER_ITEMS, AE_ZONES_DEBUG_PLAYER_CALLBACK);
    ae_DebugMenuInitFlags(r);
}

void ae_DebugMenuInitCounters(AeRam *r)
{
    uint32_t index;
    ae_wr32(r, AE_ZONES_DEBUG_COUNTER_ITEM_GLOBAL, AE_ZONES_DEBUG_COUNTER_ITEMS);
    for (index = 0; index < AE_ZONES_DEBUG_COUNTER_COUNT; ++index) {
        uint32_t item = AE_ZONES_DEBUG_COUNTER_ITEMS + index * AE_ZONES_MENU_ITEM_SIZE;
        uint32_t variable = AE_ZONES_DEBUG_COUNTER_VARIABLES + index;
        ae_MenuItemInitBound(r, item, variable, 3);
        ae_MenuItemSetCallback(r, item, AE_ZONES_DEBUG_COUNTERS_CALLBACK);
    }
}

void ae_DebugMenuInitCounters2(AeRam *r)
{
    uint32_t index;
    ae_wr32(r, AE_ZONES_DEBUG_COUNTER2_TABLE_GLOBAL, AE_ZONES_DEBUG_COUNTER2_ITEMS);
    for (index = 0; index < AE_ZONES_DEBUG_COUNTER2_COUNT; ++index) {
        uint32_t item = AE_ZONES_DEBUG_COUNTER2_ITEMS + index * AE_ZONES_MENU_ITEM_SIZE;
        uint32_t variable = AE_ZONES_DEBUG_COUNTER2_VARIABLES + index;
        ae_MenuItemInitBound(r, item, variable, 3);
    }
}

void ae_FUN_8013fc00(AeRam *r, const AeZonesHooks *h, int32_t collider_index)
{
    if (h == NULL || h->FUN_8013fc00 == NULL) {
        ae_zones_hook_missing(r, "FUN_8013FC00");
        return;
    }
    h->FUN_8013fc00(h->user, r, collider_index);
}

int32_t ae_ColliderComputeQuadrantMask(AeRam *r, const AeZonesHooks *h, uint32_t collider)
{
    int16_t center_x = ae_rds16(r, collider + AE_ZONES_COLLIDER_POSITION);
    int16_t center_z = ae_rds16(r, collider + AE_ZONES_COLLIDER_POSITION_Z);
    int16_t half_x = ae_rds16(r, collider + AE_ZONES_COLLIDER_HALF_SIZE);
    int16_t half_z = ae_rds16(r, collider + AE_ZONES_COLLIDER_HALF_SIZE_Z);
    int32_t x_positive = (int16_t)((uint16_t)center_x + (uint16_t)half_x);
    int32_t x_negative = (int16_t)((uint16_t)center_x - (uint16_t)half_x);
    int32_t z_positive = (int16_t)((uint16_t)center_z + (uint16_t)half_z);
    int32_t z_negative = (int16_t)((uint16_t)center_z - (uint16_t)half_z);
    int32_t mask = 0;

    if (h == NULL || h->get_collider_quadrant_mask_xz == NULL) {
        return ae_zones_hook_missing(r, "GetColliderQuadrantMaskXZ");
    }
    mask |= h->get_collider_quadrant_mask_xz(h->user, r, x_positive, z_positive);
    mask |= h->get_collider_quadrant_mask_xz(h->user, r, x_negative, z_positive);
    mask |= h->get_collider_quadrant_mask_xz(h->user, r, x_negative, z_negative);
    mask |= h->get_collider_quadrant_mask_xz(h->user, r, x_positive, z_negative);
    return (int16_t)(uint16_t)mask;
}

static int32_t ae_zones_area_prop_index(AeRam *r, uint32_t collider)
{
    return ae_rds16(r, collider + AE_ZONES_COLLIDER_PROP_INDEX);
}

static uint32_t ae_zones_collider_address(AeRam *r, int32_t collider_index)
{
    uint32_t table = ae_rd32(r, AE_ZONES_G_COLLIDERS);
    return table + (uint32_t)collider_index * AE_ZONES_COLLIDER_SIZE;
}

static void ae_zones_copy_collider_position_to_prop(AeRam *r, uint32_t collider, int32_t prop_index)
{
    uint32_t props = ae_rd32(r, AE_ZONES_G_AREA_PROPS);
    uint32_t prop = props + (uint32_t)prop_index * AE_ZONES_AREA_PROP_SIZE;
    uint32_t axis;
    for (axis = 0; axis < 3u; ++axis) {
        uint32_t coordinate = ae_rd16(r, collider + AE_ZONES_COLLIDER_POSITION + axis * 2u);
        ae_wr16(r, prop + AE_ZONES_AREA_PROP_POSITION + axis * 2u, coordinate);
    }
}

int32_t ae_ColliderZoneMasksUpdate(AeRam *r, const AeZonesHooks *h)
{
    int32_t collider_index = ae_rds16(r, AE_ZONES_G_COLLIDER_COUNT) - 1;
    ae_wr32(r, AE_ZONES_G_SPECIAL_COLLIDER_RESET, 0);

    while (collider_index >= 0) {
        uint32_t collider = ae_zones_collider_address(r, collider_index);
        uint16_t flags = (uint16_t)ae_rd16(r, collider + AE_ZONES_COLLIDER_FLAGS);
        uint32_t type = ae_rd8(r, collider + AE_ZONES_COLLIDER_TYPE);
        uint16_t zone_mask;
        int32_t prop_index;

        if ((flags & AE_ZONES_COLLIDER_DISABLED) != 0u) {
            zone_mask = 0;
        } else if (type != 0u && type != 9u) {
            zone_mask = 0x1111u;
        } else {
            zone_mask = (uint16_t)ae_ColliderComputeQuadrantMask(r, h, collider);
        }

        collider = ae_zones_collider_address(r, collider_index);
        ae_wr16(r, collider + AE_ZONES_COLLIDER_ZONE_MASK, zone_mask);
        flags = (uint16_t)ae_rd16(r, collider + AE_ZONES_COLLIDER_FLAGS);
        if ((flags & AE_ZONES_COLLIDER_SKIP_MASK) == 3u) {
            --collider_index;
            continue;
        }

        collider = ae_zones_collider_address(r, collider_index);
        type = ae_rd8(r, collider + AE_ZONES_COLLIDER_TYPE);
        prop_index = ae_zones_area_prop_index(r, collider);
        if (prop_index >= 0) {
            ae_zones_copy_collider_position_to_prop(r, collider, prop_index);
        }

        if (type >= 3u && type <= 6u) {
            int32_t first_extent = ae_rds16(r, collider + AE_ZONES_COLLIDER_SHAPE_PRODUCT);
            int32_t second_extent = ae_rds16(r, collider + AE_ZONES_COLLIDER_SHAPE_EXTENT);
            int32_t product = first_extent * second_extent;
            int32_t divided_product = ae_zones_divide_by_360(product);
            int32_t doubled_extent = (int32_t)((uint32_t)second_extent << 1);

            ae_wr16(r, collider + AE_ZONES_COLLIDER_SHAPE_PRODUCT, (uint32_t)divided_product);
            ae_wr16(r, collider + AE_ZONES_COLLIDER_SHAPE_EXTENT, (uint32_t)doubled_extent);
            if (type == 5u) {
                ae_wr16(r, collider + AE_ZONES_COLLIDER_DERIVED_A,
                        (uint32_t)ae_zones_divide_by_5(divided_product));
                ae_wr16(r, collider + AE_ZONES_COLLIDER_DERIVED_B,
                        (uint32_t)ae_zones_divide_by_5(doubled_extent));
                --collider_index;
                continue;
            }
            if (type == 3u) {
                uint32_t flow_x = ae_rd16(r, collider + AE_ZONES_COLLIDER_FLOW_X);
                ae_wr16(r, collider + AE_ZONES_COLLIDER_FLOW_X, flow_x << 1);
                for (uint32_t flow_offset = AE_ZONES_COLLIDER_FLOW_Y;
                     flow_offset <= AE_ZONES_COLLIDER_FLOW_Z; flow_offset += 2u) {
                    ae_wr16(r, collider + flow_offset, ae_rd16(r, collider + flow_offset) << 1);
                }
                ae_wr16(r, collider + AE_ZONES_COLLIDER_FLOW_PAD, 0u);
            }
        } else if (type == 8u || type == 12u || type == 14u) {
            ae_wr16(r, collider + AE_ZONES_COLLIDER_DERIVED_A,
                    ae_rd16(r, collider + AE_ZONES_COLLIDER_SHAPE_PRODUCT));
            if (type == 12u) {
                uint32_t props = ae_rd32(r, AE_ZONES_G_AREA_PROPS);
                uint32_t prop = props + (uint32_t)prop_index * AE_ZONES_AREA_PROP_SIZE;
                uint32_t matrix = ae_rd32(r, prop + AE_ZONES_AREA_PROP_ROTATION);
                ae_wr16(r, AE_ZONES_STACK_TEMP_VECTOR, 0u);
                ae_wr16(r, AE_ZONES_STACK_TEMP_VECTOR + 2u, 0u);
                ae_wr16(r, AE_ZONES_STACK_TEMP_VECTOR + 4u, 0u);
                ae_SvecToDiagMatrix(r, AE_ZONES_STACK_TEMP_VECTOR, matrix);
            }
        } else if (type == 13u) {
            ae_FUN_8013fc00(r, h, collider_index);
            ae_wr32(r, AE_ZONES_G_SPECIAL_COLLIDER, (uint32_t)collider_index);
        } else if (type == 2u) {
            for (uint32_t flow_offset = AE_ZONES_COLLIDER_FLOW_X;
                 flow_offset <= AE_ZONES_COLLIDER_FLOW_Z; flow_offset += 2u) {
                ae_wr16(r, collider + flow_offset, ae_rd16(r, collider + flow_offset) << 1);
            }
            ae_wr16(r, collider + AE_ZONES_COLLIDER_FLOW_PAD, 0u);
        }

        --collider_index;
    }
    return 0;
}
