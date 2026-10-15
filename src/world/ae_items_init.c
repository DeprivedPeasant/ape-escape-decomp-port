#include "ae_items_init.h"

#define AE_ITEMS_G_STAGE_STATE 0x800F4350u
#define AE_ITEMS_STATE_LEVEL 0x124u
#define AE_ITEMS_STATE_AREA 0x126u
#define AE_ITEMS_STATE_ITEM_HEADER 0x248u
#define AE_ITEMS_STATE_LIGHT_MODE 0x27Cu

#define AE_ITEMS_G_COLLECTED_TABLE 0x800DFB70u
#define AE_ITEMS_COLLECTED_ENTRY_COUNT 128u
#define AE_ITEMS_COLLECTED_ENTRY_STRIDE 2u
#define AE_ITEMS_COLLECTED_EMPTY 0xFFu

#define AE_ITEMS_G_PICKUP_SLOTS 0x800BEFA0u
#define AE_ITEMS_PICKUP_SLOT_COUNT 32u
#define AE_ITEMS_PICKUP_SLOT_SIZE 24u
#define AE_ITEMS_PICKUP_RESERVED_WORD 0x00u
#define AE_ITEMS_PICKUP_KIND 0x0Au
#define AE_ITEMS_PICKUP_POSITION_X 0x04u
#define AE_ITEMS_PICKUP_POSITION_Y 0x06u
#define AE_ITEMS_PICKUP_POSITION_Z 0x08u
#define AE_ITEMS_PICKUP_ACTIVE 0x0Au
#define AE_ITEMS_PICKUP_PHASE 0x0Bu
#define AE_ITEMS_PICKUP_VELOCITY_X 0x0Cu
#define AE_ITEMS_PICKUP_VELOCITY_Y 0x0Eu
#define AE_ITEMS_PICKUP_VELOCITY_Z 0x10u
#define AE_ITEMS_PICKUP_MODE0_FLAG 0x14u
#define AE_ITEMS_PICKUP_MODE1_FLAG 0x15u
#define AE_ITEMS_PICKUP_MODE2_FLAG 0x17u
#define AE_ITEMS_POSITION_X 0x00u
#define AE_ITEMS_POSITION_Y 0x02u
#define AE_ITEMS_POSITION_Z 0x04u

#define AE_ITEMS_G_RANDOM_SEED_A 0x800AE8F0u
#define AE_ITEMS_G_RANDOM_SEED_B 0x800AE8F2u
#define AE_ITEMS_G_AREA_ITEM_FLAGS 0x800BF2D8u
#define AE_ITEMS_AREA_ITEM_FLAG_COUNT 32u
#define AE_ITEMS_G_ITEM_KIND 0x800BF2F8u
#define AE_ITEMS_G_ITEM_LOAD_STATE 0x800B28B0u
#define AE_ITEMS_G_ITEM_LOAD_BUSY (AE_ITEMS_G_ITEM_LOAD_STATE + 1u)
#define AE_ITEMS_G_ITEM_SCRATCH_FLAG 0x800B28B2u
#define AE_ITEMS_G_ITEM_GLOBAL_FLAG 0x800BF2B5u

#define AE_ITEMS_LEVEL_TABLE 0x800B2920u
#define AE_ITEMS_AREA_TABLE 0x800BF2B8u
#define AE_ITEMS_ITEM_HEADER_MAGIC0 0x00u
#define AE_ITEMS_ITEM_HEADER_MAGIC1 0x02u
#define AE_ITEMS_ITEM_HEADER_COUNT 0x04u
#define AE_ITEMS_ITEM_RECORDS 0x08u
#define AE_ITEMS_ITEM_RECORD_SIZE 0x10u
#define AE_ITEMS_ITEM_MAGIC0 0x492Eu
#define AE_ITEMS_ITEM_MAGIC1 0x4D54u
#define AE_ITEMS_ITEM_LOADED_MARKER 0x78u
#define AE_ITEMS_ITEM_END 0x00FFu
#define AE_ITEMS_ITEM_KIND 0x00u
#define AE_ITEMS_ITEM_POSITION_X 0x02u
#define AE_ITEMS_ITEM_KIND_SPECIAL 4u
#define AE_ITEMS_AREA_INDEX_LIMIT 8u

#define AE_ITEMS_LEVEL_GRID 0x800EBFA0u
#define AE_ITEMS_LEVEL_GRID_FAR_COLOR 0x1Cu
#define AE_ITEMS_LEVEL_GRID_FOG_PARAMETER 0x24u
#define AE_ITEMS_LEVEL_GRID_FOG_START 0x28u
#define AE_ITEMS_FOG_SCRATCH 0x1F8003C0u
#define AE_ITEMS_FOG_SCRATCH_START 0x1F8003C2u
#define AE_ITEMS_FOG_SCRATCH_ZERO_A 0x1F8003C4u
#define AE_ITEMS_FOG_SCRATCH_ZERO_B 0x1F8003C6u

#define AE_ITEMS_G_COLOR_MATRIX 0x800EBFF0u
#define AE_ITEMS_G_RESTORE_MATRIX 0x800E78E0u
#define AE_ITEMS_COLOR_MATRIX_UNIT 4096u
#define AE_ITEMS_COLOR_MATRIX_DIM_FACTOR 2730u
#define AE_ITEMS_MATRIX_R11 0x00u
#define AE_ITEMS_MATRIX_R12 0x02u
#define AE_ITEMS_MATRIX_R13 0x04u
#define AE_ITEMS_MATRIX_R21 0x06u
#define AE_ITEMS_MATRIX_R22 0x08u
#define AE_ITEMS_MATRIX_R23 0x0Au
#define AE_ITEMS_MATRIX_R31 0x0Cu
#define AE_ITEMS_MATRIX_R32 0x0Eu
#define AE_ITEMS_MATRIX_R33 0x10u
#define AE_ITEMS_COPY_WORD_0 0x00u
#define AE_ITEMS_COPY_WORD_1 0x04u
#define AE_ITEMS_COPY_WORD_2 0x08u
#define AE_ITEMS_COPY_WORD_3 0x0Cu
#define AE_ITEMS_COPY_WORD_4 0x10u
#define AE_ITEMS_COPY_WORD_5 0x14u
#define AE_ITEMS_COPY_WORD_6 0x18u
#define AE_ITEMS_COPY_WORD_7 0x1Cu
#define AE_ITEMS_COLOR_RED 0u
#define AE_ITEMS_COLOR_GREEN 1u
#define AE_ITEMS_COLOR_BLUE 2u

#define AE_ITEMS_AREA_OVERFLOW_MESSAGE 0x800A6160u

static int32_t items_call(AeRam *r, const AeItemsInitHooks *h,
                          AeItemsInitCallHook callback, const char *name,
                          uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    if (!h || !callback) {
        ae_ram_fail(r, "AeItemsInitHooks.%s not provided", name);
        return 0;
    }
    return callback(h->user, r, a0, a1, a2, a3);
}

static int items_color_call(AeRam *r, const AeItemsInitHooks *h,
                            AeItemsInitColorHook callback, const char *name,
                            uint32_t red, uint32_t green, uint32_t blue) {
    if (!h || !callback) {
        ae_ram_fail(r, "AeItemsInitHooks.%s not provided", name);
        return 0;
    }
    callback(h->user, r, red, green, blue);
    return !ae_ram_failed(r);
}

uint32_t ae_GetCollectedItemsTable(AeRam *r) {
    (void)r;
    return AE_ITEMS_G_COLLECTED_TABLE;
}

int32_t ae_CollectedItemsHas(AeRam *r, uint32_t item_id) {
    uint32_t table = ae_GetCollectedItemsTable(r);
    uint32_t id = item_id & 0xFFFFu;

    for (uint32_t index = 0; index < AE_ITEMS_COLLECTED_ENTRY_COUNT; ++index) {
        uint32_t value = ae_rd8(r, table + index * AE_ITEMS_COLLECTED_ENTRY_STRIDE);
        if (value == AE_ITEMS_COLLECTED_EMPTY) return 0;
        if (value == id) return 1;
    }
    return 0;
}

void ae_MatrixCopy8(AeRam *r, uint32_t destination, uint32_t source) {
    uint32_t first = ae_rd32(r, source + AE_ITEMS_COPY_WORD_0);
    uint32_t second = ae_rd32(r, source + AE_ITEMS_COPY_WORD_1);
    uint32_t third = ae_rd32(r, source + AE_ITEMS_COPY_WORD_2);
    ae_wr32(r, destination + AE_ITEMS_COPY_WORD_0, first);
    ae_wr32(r, destination + AE_ITEMS_COPY_WORD_1, second);
    ae_wr32(r, destination + AE_ITEMS_COPY_WORD_2, third);

    first = ae_rd32(r, source + AE_ITEMS_COPY_WORD_3);
    second = ae_rd32(r, source + AE_ITEMS_COPY_WORD_4);
    ae_wr32(r, destination + AE_ITEMS_COPY_WORD_3, first);
    ae_wr32(r, destination + AE_ITEMS_COPY_WORD_4, second);

    first = ae_rd32(r, source + AE_ITEMS_COPY_WORD_5);
    second = ae_rd32(r, source + AE_ITEMS_COPY_WORD_6);
    third = ae_rd32(r, source + AE_ITEMS_COPY_WORD_7);
    ae_wr32(r, destination + AE_ITEMS_COPY_WORD_5, first);
    ae_wr32(r, destination + AE_ITEMS_COPY_WORD_6, second);
    ae_wr32(r, destination + AE_ITEMS_COPY_WORD_7, third);
}

void ae_SetColorMatrixMode(AeRam *r, const AeItemsInitHooks *h,
                           uint32_t mode, uint32_t red, uint32_t green,
                           uint32_t blue) {
    if (mode == 0u) {
        (void)items_color_call(r, h, h ? h->SetBackColor : NULL, "SetBackColor", 0, 0, 0);
        if (ae_ram_failed(r)) return;

        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R11, AE_ITEMS_COLOR_MATRIX_UNIT);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R21, AE_ITEMS_COLOR_MATRIX_UNIT);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R31, AE_ITEMS_COLOR_MATRIX_UNIT);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R12, AE_ITEMS_COLOR_MATRIX_UNIT);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R22, AE_ITEMS_COLOR_MATRIX_UNIT);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R32, AE_ITEMS_COLOR_MATRIX_UNIT);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R13, 0);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R23, 0);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R33, 0);
    } else if (mode == 1u) {
        (void)items_color_call(r, h, h ? h->SetBackColor : NULL, "SetBackColor", 0, 0, 0);
        if (ae_ram_failed(r)) return;

        uint32_t darken = (ae_rd32(r, AE_ITEMS_G_STAGE_STATE +
                                   AE_ITEMS_STATE_LIGHT_MODE) >> 12) == mode;
        uint32_t first_column = darken ? AE_ITEMS_COLOR_MATRIX_DIM_FACTOR :
                                         AE_ITEMS_COLOR_MATRIX_UNIT;
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R11, first_column);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R21, first_column);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R31, AE_ITEMS_COLOR_MATRIX_UNIT);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R12, AE_ITEMS_COLOR_MATRIX_UNIT);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R22, AE_ITEMS_COLOR_MATRIX_UNIT);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R32, AE_ITEMS_COLOR_MATRIX_UNIT);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R13, 0);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R23, 0);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R33, 0);
    } else if (mode == 2u) {
        ae_MatrixCopy8(r, AE_ITEMS_G_RESTORE_MATRIX, AE_ITEMS_G_COLOR_MATRIX);
        (void)items_color_call(r, h, h ? h->SetBackColor : NULL, "SetBackColor", 0, 0, 0);
        if (ae_ram_failed(r)) return;

        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R11, red);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R21, green);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R31, blue);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R12, AE_ITEMS_COLOR_MATRIX_UNIT);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R22, AE_ITEMS_COLOR_MATRIX_UNIT);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R32, AE_ITEMS_COLOR_MATRIX_UNIT);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R13, 0);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R23, 0);
        ae_wr16(r, AE_ITEMS_G_COLOR_MATRIX + AE_ITEMS_MATRIX_R33, 0);
    } else if (mode == 3u) {
        ae_MatrixCopy8(r, AE_ITEMS_G_COLOR_MATRIX, AE_ITEMS_G_RESTORE_MATRIX);
    } else {
        return;
    }

    if (!ae_ram_failed(r)) {
        if (!h || !h->SetColorMatrix) { ae_ram_fail(r, "AeItemsInitHooks.SetColorMatrix not provided"); return; }
        h->SetColorMatrix(h->user, r, mode == 3u ? AE_ITEMS_G_RESTORE_MATRIX : AE_ITEMS_G_COLOR_MATRIX);
    }
}

void ae_ApplyLevelFog(AeRam *r, const AeItemsInitHooks *h) {
    uint32_t red = ae_rd8(r, AE_ITEMS_LEVEL_GRID + AE_ITEMS_LEVEL_GRID_FAR_COLOR + AE_ITEMS_COLOR_RED);
    uint32_t green = ae_rd8(r, AE_ITEMS_LEVEL_GRID + AE_ITEMS_LEVEL_GRID_FAR_COLOR + AE_ITEMS_COLOR_GREEN);
    uint32_t blue = ae_rd8(r, AE_ITEMS_LEVEL_GRID + AE_ITEMS_LEVEL_GRID_FAR_COLOR + AE_ITEMS_COLOR_BLUE);
    (void)items_color_call(r, h, h ? h->SetFarColor : NULL, "SetFarColor", red, green, blue);
    if (ae_ram_failed(r)) return;

    ae_wr16(r, AE_ITEMS_FOG_SCRATCH,
            ae_rd16(r, AE_ITEMS_LEVEL_GRID + AE_ITEMS_LEVEL_GRID_FOG_PARAMETER));
    ae_wr16(r, AE_ITEMS_FOG_SCRATCH_START,
            ae_rd16(r, AE_ITEMS_LEVEL_GRID + AE_ITEMS_LEVEL_GRID_FOG_START));
    ae_wr16(r, AE_ITEMS_FOG_SCRATCH_ZERO_A, 0);
    ae_wr16(r, AE_ITEMS_FOG_SCRATCH_ZERO_B, 0);
}

void ae_PickupSpawnSlot(AeRam *r, const AeItemsInitHooks *h, uint32_t kind, uint32_t position,
                        int32_t mode, int32_t slot) {
    uint32_t pickup = AE_ITEMS_G_PICKUP_SLOTS;
    int32_t remaining = (int32_t)(AE_ITEMS_PICKUP_SLOT_COUNT - 1u);

    if (slot < 0) {
        while (ae_rd8(r, pickup + AE_ITEMS_PICKUP_ACTIVE) != 0u) {
            --remaining;
            if (remaining < 0) return;
            pickup += AE_ITEMS_PICKUP_SLOT_SIZE;
        }
    } else {
        pickup += (uint32_t)slot * AE_ITEMS_PICKUP_SLOT_SIZE;
    }

    ae_wr8(r, pickup + AE_ITEMS_PICKUP_KIND, kind);
    ae_wr8(r, pickup + AE_ITEMS_PICKUP_MODE1_FLAG, 0);
    ae_wr32(r, pickup + AE_ITEMS_PICKUP_RESERVED_WORD, 0);

    uint32_t seed = (ae_rd16(r, AE_ITEMS_G_RANDOM_SEED_A) * 5u) + 1u;
    ae_wr16(r, AE_ITEMS_G_RANDOM_SEED_A, seed);
    ae_wr8(r, pickup + AE_ITEMS_PICKUP_PHASE, (seed >> 8) & 0x1Fu);
    ae_wr8(r, pickup + AE_ITEMS_PICKUP_MODE0_FLAG, 0);
    ae_wr8(r, pickup + AE_ITEMS_PICKUP_MODE2_FLAG, 0);

    uint32_t x = ae_rd16(r, position + AE_ITEMS_POSITION_X);
    uint32_t y = ae_rd16(r, position + AE_ITEMS_POSITION_Y);
    uint32_t z = ae_rd16(r, position + AE_ITEMS_POSITION_Z);

    if (mode == 1) {
        seed = (ae_rd16(r, AE_ITEMS_G_RANDOM_SEED_A) * 5u) + 1u;
        ae_wr16(r, AE_ITEMS_G_RANDOM_SEED_A, seed);
        uint32_t angle = ((seed >> 4) & 0xFF0u);
        if (!h || !h->rsin || !h->rcos) { ae_ram_fail(r, "AeItemsInitHooks.rsin/rcos not provided"); return; }
        int32_t x_velocity = h->rcos(h->user, r, (int32_t)angle);
        if (x_velocity < 0) x_velocity += 15;
        uint32_t x_position = x + (uint32_t)(x_velocity >> 4);

        seed = (ae_rd16(r, AE_ITEMS_G_RANDOM_SEED_B) * 5u) + 1u;
        ae_wr16(r, AE_ITEMS_G_RANDOM_SEED_B, seed);
        angle = ((seed >> 4) & 0xFF0u);
        int32_t z_velocity = h->rsin(h->user, r, (int32_t)angle);
        if (z_velocity < 0) z_velocity += 15;
        uint32_t z_position = z + (uint32_t)(z_velocity >> 4);

        ae_wr16(r, pickup + AE_ITEMS_PICKUP_POSITION_X, x_position);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_POSITION_Y, y);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_POSITION_Z, z_position);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_VELOCITY_Y, 12);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_VELOCITY_Z, 0);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_VELOCITY_X, 0);
    } else if (mode == 2) {
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_POSITION_X, x);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_POSITION_Y, y);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_POSITION_Z, z);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_VELOCITY_Z, 0);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_VELOCITY_X, 0);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_VELOCITY_Y, (uint32_t)-62);
    } else if (mode == 3) {
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_POSITION_X, x);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_POSITION_Y, y);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_VELOCITY_Z, 0);

        uint32_t seed_a = (ae_rd16(r, AE_ITEMS_G_RANDOM_SEED_A) * 5u) + 1u;
        ae_wr16(r, AE_ITEMS_G_RANDOM_SEED_A, seed_a);
        uint32_t velocity_x = ((seed_a >> 8) & 0x1Fu) - 16u;

        uint32_t seed_b = (ae_rd16(r, AE_ITEMS_G_RANDOM_SEED_B) * 5u) + 1u;
        ae_wr16(r, AE_ITEMS_G_RANDOM_SEED_B, seed_b);
        uint32_t velocity_z = ((seed_b >> 8) & 0x1Fu) - 16u;

        uint32_t final_seed_a = (ae_rd16(r, AE_ITEMS_G_RANDOM_SEED_A) * 5u) + 1u;
        ae_wr16(r, AE_ITEMS_G_RANDOM_SEED_A, final_seed_a);
        uint32_t velocity_y = (uint32_t)-31 - ((final_seed_a >> 8) & 0x1Fu);

        ae_wr16(r, pickup + AE_ITEMS_PICKUP_VELOCITY_X, velocity_x);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_VELOCITY_Z, velocity_z);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_VELOCITY_Y, velocity_y);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_POSITION_Z, z);
    } else {
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_POSITION_X, x);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_POSITION_Y, y);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_POSITION_Z, z);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_VELOCITY_Z, 0);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_VELOCITY_Y, 0);
        ae_wr16(r, pickup + AE_ITEMS_PICKUP_VELOCITY_X, 0);
        ae_wr8(r, pickup + AE_ITEMS_PICKUP_MODE0_FLAG, 1);
    }
}

void ae_ItemsLoadForArea(AeRam *r, const AeItemsInitHooks *h) {
    uint32_t item_header = ae_rd32(r, AE_ITEMS_G_STAGE_STATE + AE_ITEMS_STATE_ITEM_HEADER);
    for (uint32_t index = 0; index < AE_ITEMS_AREA_ITEM_FLAG_COUNT; ++index) {
        ae_wr8(r, AE_ITEMS_G_AREA_ITEM_FLAGS + index, 0);
    }

    uint32_t pickup_table_end = AE_ITEMS_G_PICKUP_SLOTS +
                                (AE_ITEMS_PICKUP_SLOT_COUNT - 1u) * AE_ITEMS_PICKUP_SLOT_SIZE;
    for (uint32_t pickup = pickup_table_end;; pickup -= AE_ITEMS_PICKUP_SLOT_SIZE) {
        ae_wr8(r, pickup + AE_ITEMS_PICKUP_ACTIVE, 0);
        if (pickup == AE_ITEMS_G_PICKUP_SLOTS) break;
    }

    if (ae_rd16(r, item_header + AE_ITEMS_ITEM_HEADER_MAGIC0) != AE_ITEMS_ITEM_MAGIC0 ||
        ae_rd16(r, item_header + AE_ITEMS_ITEM_HEADER_MAGIC1) != AE_ITEMS_ITEM_MAGIC1) {
        return;
    }
    ae_wr8(r, item_header + AE_ITEMS_ITEM_HEADER_MAGIC0,
           AE_ITEMS_ITEM_LOADED_MARKER);

    uint32_t level = ae_rd16(r, AE_ITEMS_G_STAGE_STATE + AE_ITEMS_STATE_LEVEL);
    int32_t first_area = items_call(r, h, h ? h->GetLevelFirstArea : NULL,
                                        "GetLevelFirstArea", level, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    uint32_t current_area = ae_rd16(r, AE_ITEMS_G_STAGE_STATE + AE_ITEMS_STATE_AREA);
    int32_t area_delta = (int32_t)(current_area - (uint32_t)first_area);
    uint32_t item_mask_index = (uint32_t)area_delta;

    if (area_delta >= (int32_t)AE_ITEMS_AREA_INDEX_LIMIT) {
        (void)items_call(r, h, h ? h->printf : NULL, "printf",
                         AE_ITEMS_AREA_OVERFLOW_MESSAGE, 0, 0, 0);
        if (ae_ram_failed(r)) return;
        item_mask_index = 0;
    }

    uint32_t item_mask = ae_rd32(r, AE_ITEMS_AREA_TABLE + item_mask_index * sizeof(uint32_t));
    uint32_t item_count = ae_rd16(r, item_header + AE_ITEMS_ITEM_HEADER_COUNT);
    uint32_t record = item_header + AE_ITEMS_ITEM_RECORDS;
    uint32_t area_item_flags = AE_ITEMS_G_AREA_ITEM_FLAGS;
    const uint32_t spawned_flag = 1u;
    ae_wr8(r, AE_ITEMS_G_ITEM_LOAD_BUSY, spawned_flag);

    for (uint32_t index = 0; index < item_count; ++index) {
        uint32_t kind = ae_rd16(r, record + AE_ITEMS_ITEM_KIND);
        if (kind == AE_ITEMS_ITEM_END) break;

        uint32_t position = record + AE_ITEMS_ITEM_POSITION_X;
        if ((item_mask & 1u) == 0u) {
            int collected = 0;
            if (kind == AE_ITEMS_ITEM_KIND_SPECIAL) {
                collected = ae_CollectedItemsHas(r, current_area);
                if (ae_ram_failed(r)) return;
            }
            if (!collected) {
                ae_PickupSpawnSlot(r, h, kind, position, 0, (int32_t)index);
                if (ae_ram_failed(r)) return;
                ae_wr8(r, area_item_flags + index, spawned_flag);
            }
        }

        item_mask >>= 1;
        record += AE_ITEMS_ITEM_RECORD_SIZE;
    }

    {
        uint32_t level_kind = ae_rd8(r, AE_ITEMS_LEVEL_TABLE + level);
        if (level_kind >= 5u) level_kind = 4u;
        ae_wr8(r, AE_ITEMS_G_ITEM_KIND, level_kind);
        ae_wr8(r, AE_ITEMS_G_ITEM_LOAD_STATE, 3u);
        ae_wr8(r, AE_ITEMS_G_ITEM_SCRATCH_FLAG, 0u);
        ae_wr8(r, AE_ITEMS_G_ITEM_GLOBAL_FLAG, 0u);
        ae_wr8(r, AE_ITEMS_G_ITEM_LOAD_BUSY, 0u);
    }
}
