#include "ae_zako_init.h"
#include "../bodies/ae_bodies.h"
#include "ae_world.h"

#include <stddef.h>

#define ZAKO_RESULT(hooks, member, a0, a1, a2, a3, result) do { \
    if (!(hooks) || !(hooks)->member) { \
        ae_ram_fail(r, #member " hook not provided"); \
        return; \
    } \
    (result) = (hooks)->member((hooks)->user, r, (uint32_t)(a0), (uint32_t)(a1), \
                               (uint32_t)(a2), (uint32_t)(a3)); \
    if (ae_ram_failed(r)) return; \
} while (0)

#define ZAKO_CALL(hooks, member, a0, a1, a2, a3) do { \
    if (!(hooks) || !(hooks)->member) { \
        ae_ram_fail(r, #member " hook not provided"); \
        return; \
    } \
    (void)(hooks)->member((hooks)->user, r, (uint32_t)(a0), (uint32_t)(a1), \
                          (uint32_t)(a2), (uint32_t)(a3)); \
    if (ae_ram_failed(r)) return; \
} while (0)

static uint32_t model_header_word(uint32_t model_header, uint32_t offset) {
    return model_header + offset;
}

static int type_is_one_of(uint32_t type, const uint8_t *types, size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (types[i] == type) return 1;
    return 0;
}

void ae_ModelInitAuto(AeRam *r, uint32_t instance, uint32_t model_header) {
    uint32_t colors = model_header + AE_MODEL_HEADER_DATA;
    uint32_t bone_count = ae_rd16(r, model_header_word(model_header, AE_MODEL_HEADER_BONE_COUNT));
    uint32_t vertex_count = ae_rd16(r, model_header_word(model_header, AE_MODEL_HEADER_VERTEX_COUNT));
    uint32_t color_count = ae_rd16(r, model_header_word(model_header, AE_MODEL_HEADER_COLOR_COUNT));
    uint32_t has_normals = ae_rd16(r, model_header_word(model_header, AE_MODEL_HEADER_HAS_NORMALS));
    uint32_t bones = colors + color_count * 4u;
    uint32_t vertices = bones + bone_count * 12u;
    uint32_t vertex_stride = has_normals != 0 ? AE_MODEL_NORMAL_VERTEX_STRIDE : AE_MODEL_VERTEX_STRIDE;
    uint32_t primitives = vertices + vertex_count * vertex_stride;

    ae_wr32(r, instance + AE_OBJ_MODEL, model_header);
    ae_wr32(r, instance + AE_OBJ_COLORS, colors);
    ae_wr32(r, instance + AE_OBJ_PRIMS, primitives);
    ae_wr32(r, instance + AE_OBJ_BONES, bones);
    ae_wr32(r, instance + AE_OBJ_VERTS, vertices);
    ae_wr32(r, instance + AE_OBJ_BONE_MATRICES, AE_G_MODEL_BONE_MATRICES);
}

int32_t ae_ZakoTypeIsIgnored(AeRam *r, int32_t type) {
    (void)r;
    switch ((uint32_t)type) {
    case 0x61u:
    case 0x63u:
    case 0x6Du:
    case 0x6Eu:
    case 0x84u:
        return 1;
    default:
        return (uint32_t)((uint32_t)type - 0x72u) < 0x44u;
    }
}

void ae_ZakoInit(AeRam *r, const AeZakoHooks *h) {
    int32_t grid_limit_source = ae_rds32(r, AE_ZAKO_GRID_LIMIT_SOURCE);
    int32_t grid_limit_x = grid_limit_source < AE_ZAKO_GRID_LIMIT_X_CLAMP
        ? grid_limit_source : AE_ZAKO_GRID_LIMIT_X_MAX;
    uint32_t grid_limit_z_bits = (uint32_t)grid_limit_source +
                                 AE_ZAKO_GRID_LIMIT_Z_OFFSET;
    int32_t grid_limit_z = grid_limit_z_bits <= INT32_MAX
        ? (int32_t)grid_limit_z_bits
        : (int32_t)((int64_t)grid_limit_z_bits - 0x100000000LL);
    if (grid_limit_z >= AE_ZAKO_GRID_LIMIT_Z_CLAMP)
        grid_limit_z = AE_ZAKO_GRID_LIMIT_Z_MAX;
    ae_wr16(r, AE_ZAKO_GRID_LIMIT_X, (uint32_t)grid_limit_x);
    ae_wr16(r, AE_ZAKO_GRID_LIMIT_Z, (uint32_t)grid_limit_z);

    ae_wr8(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_RESET_BYTE_14, 0);
    ae_wr8(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_RESET_BYTE_1F, 0);
    ae_wr8(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_RESET_BYTE_18, 1);
    ae_wr8(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_RESET_BYTE_12, 0);
    ae_wr8(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_RESET_BYTE_13, 0);
    ae_wr16(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_ENTRY_COUNT, 0);
    ae_wr16(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_PATH_COUNT, 0);
    ae_wr16(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_SPAWN_COUNT, 0);
    ae_wr8(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_RESET_BYTE_23, 0);

    ae_wr8(r, AE_G_ZAKO_MODE_KIND, 0);
    ae_wr8(r, AE_G_ZAKO_MODE_COUNTER, 0);
    ae_wr8(r, AE_G_ZAKO_MODE_KIND + AE_ZAKO_MODE_RESET_BYTE_03, 0);
    ae_wr8(r, AE_G_ZAKO_MODE_KIND + AE_ZAKO_MODE_RESET_BYTE_04, 0);
    ae_wr16(r, AE_G_ZAKO_MODE_KIND + AE_ZAKO_MODE_RESET_HALF_06, 0);
    ae_ZakoHooksReset(r);

    for (uint32_t i = 0; i < AE_ZAKO_OBJECT_COUNT; ++i) {
        uint32_t obj = AE_G_ZAKO_OBJECTS + i * AE_ZAKO_OBJECT_SIZE;
        ae_wr8(r, obj + AE_OBJ_SLOT, i);
        ae_wr32(r, obj + AE_OBJ_FLAGS,
                ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_SPAWNED);
        ae_wr8(r, AE_G_ZAKO_SLOT_BY_ENTRY + i, AE_ZAKO_NO_SPAWN_INDEX);
    }

    ae_ZakoDebugMenuInit(r, h);
    if (ae_ram_failed(r)) return;

    uint32_t area_zed = ae_rd32(r, AE_G_AREA_ZED);
    if (ae_rd32(r, area_zed) != AE_ZED_MAGIC) return;

    uint32_t entry_count_raw = ae_rd16(r, area_zed + AE_ZED_ENTRY_COUNT);
    int32_t entry_count = (int16_t)entry_count_raw;
    uint32_t entry_table = area_zed + ae_rd32(r, area_zed + AE_ZED_ENTRY_OFFSET);
    int32_t path_count = ae_rd16(r, area_zed + AE_ZED_PATH_COUNT);
    uint32_t path_table = area_zed + ae_rd32(r, area_zed + AE_ZED_PATH_OFFSET);
    ae_wr32(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_AREA_ZED, area_zed);
    ae_wr16(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_ENTRY_COUNT, entry_count_raw);
    ae_wr32(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_ENTRY_TABLE, entry_table);
    ae_wr16(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_PATH_COUNT, (uint32_t)path_count);
    ae_wr32(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_PATH_TABLE, path_table);
    ae_wr8(r, area_zed, 120u);

    uint32_t obj = AE_G_ZAKO_OBJECTS;
    for (int32_t entry_index = 0; entry_index < entry_count; ++entry_index) {
        uint32_t entry = entry_table + (uint32_t)entry_index * AE_ZED_ENTRY_SIZE;
        uint32_t type = ae_rd16(r, entry + AE_ZED_ENTRY_TYPE);
        if (type < AE_ZAKO_FIRST_SPAWN_TYPE &&
            !ae_ZakoTypeIsIgnored(r, (int32_t)type)) {
            ae_ZakoSpawnFromEntry(r, h, obj, entry, entry_index);
            if (ae_ram_failed(r)) return;
            obj += AE_ZAKO_OBJECT_SIZE;
            uint32_t spawn_count = ae_rd16(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_SPAWN_COUNT);
            ae_wr16(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_SPAWN_COUNT, spawn_count + 1u);
        }
    }

    uint32_t stage_state = AE_G_STAGE_STATE;
    if (ae_rd8(r, stage_state + AE_ZAKO_STAGE_AREA_KIND) == AE_ZAKO_AREA_MODE_CLEAR) {
        ae_ZakoClearSlotTable(r);
        return;
    }

    int32_t level = ae_rd16(r, stage_state + AE_ZAKO_STAGE_LEVEL);
    int32_t first_area = ae_GetLevelFirstArea(r, level);
    uint32_t current_area = ae_rd16(r, stage_state + AE_ZAKO_STAGE_AREA);
    int32_t area_delta = (int32_t)(current_area - (uint32_t)first_area);
    if (area_delta >= AE_ZAKO_AREA_INDEX_LIMIT) {
        ZAKO_CALL(h, Printf, AE_ZAKO_FMT_SIGHT_OVERFLOW, 0, 0, 0);
        area_delta = 0;
    }

    uint32_t progress_word = AE_G_ZAKO_PROGRESS + ((uint32_t)area_delta << 2);
    uint32_t defeated_mask = ae_rd32(r, progress_word);
    int32_t active_object_count = ae_rds16(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_ENTRY_COUNT);
    obj = AE_G_ZAKO_OBJECTS;
    for (int32_t i = 0; i < active_object_count; ++i) {
        if ((defeated_mask & 1u) != 0) {
            uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
            uint32_t type = ae_rd8(r, obj + AE_OBJ_TYPE);
            if ((flags & AE_OBJ_FLAG_SPAWNED) != 0 && type != AE_ZAKO_TYPE_EXEMPT_KILL) {
                ae_ObjKill(r, h, obj, 0);
                if (ae_ram_failed(r)) return;
            }
        }
        defeated_mask >>= 1;
        obj += AE_ZAKO_OBJECT_SIZE;
    }
}

void ae_ZakoSetupType(AeRam *r, const AeZakoHooks *h, uint32_t obj, uint32_t type) {
    static const uint8_t no_spawn_flag[] = {
        0xAD, 0x18, 0x30, 0x3B, 0x70, 0x8E, 0x69, 0x3D, 0x23, 0x2E,
        0x45, 0x50, 0x57, 0x0C, 0x1A, 0x1B, 0x4F, 0x39, 0x40
    };
    static const uint8_t no_body_flag[] = {
        0x30, 0x18, 0x3B, 0x12, 0x17, 0x3A, 0x32, 0x33, 0x24, 0x70,
        0x2C, 0x71, 0x3D, 0x2E, 0x45, 0x29, 0x15, 0x00, 0x55, 0x57,
        0x40, 0x62, 0x35, 0x64, 0x65, 0x51, 0x1B
    };
    static const uint8_t no_target_flag[] = {
        0x37, 0x44, 0x54, 0x3D, 0x20, 0x07, 0x22, 0x56, 0x19,
        0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E
    };

    ae_wr16(r, obj + AE_OBJ_VELOCITY_X, 0);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Y, 0);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Z, 0);
    ae_wr16(r, obj + AE_OBJ_TIMER, 0);
    ae_wr16(r, obj + AE_OBJ_TARGET_X, ae_rds32(r, obj + AE_OBJ_POS_X));
    ae_wr16(r, obj + AE_OBJ_TARGET_Z, ae_rds32(r, obj + AE_OBJ_POS_Z));
    ae_wr16(r, obj + AE_OBJ_HOME_Y, ae_rds32(r, obj + AE_OBJ_POS_Y));
    ae_ZakoSetAnimFromEntry(r, obj, type);
    ae_ObjInitModel(r, obj, type);

    uint32_t object_type = ae_rd8(r, obj + AE_OBJ_TYPE);
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
    if (type_is_one_of(object_type, no_spawn_flag, sizeof(no_spawn_flag)))
        flags &= ~AE_OBJ_FLAG_SPAWNED;
    else
        flags |= AE_OBJ_FLAG_SPAWNED;
    ae_wr32(r, obj + AE_OBJ_FLAGS, flags);

    if (type_is_one_of(object_type, no_body_flag, sizeof(no_body_flag)))
        flags &= ~AE_OBJ_FLAG_BODY_ACTIVE;
    else
        flags |= AE_OBJ_FLAG_BODY_ACTIVE;
    ae_wr32(r, obj + AE_OBJ_FLAGS, flags);

    if (type_is_one_of(object_type, no_target_flag, sizeof(no_target_flag)) ||
        (object_type >= 0x59u && object_type <= 0x5Eu))
        flags &= ~AE_OBJ_FLAG_TARGET_LISTED;
    else
        flags |= AE_OBJ_FLAG_TARGET_LISTED;
    ae_wr32(r, obj + AE_OBJ_FLAGS, flags);

    if ((flags & (AE_OBJ_FLAG_BODY_ACTIVE | AE_OBJ_FLAG_SPAWNED)) ==
        (AE_OBJ_FLAG_BODY_ACTIVE | AE_OBJ_FLAG_SPAWNED)) {
        int32_t body_type = (object_type == 1u || object_type == 4u ||
                             object_type == 9u || object_type == 0x3Fu) ? 5 : 4;
        (void)ae_CollisionBodyRegister(r, h ? h->body_hooks : NULL, body_type,
                                       obj + AE_OBJ_POS_X, obj);
        if (ae_ram_failed(r)) return;

        flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
        if ((flags & AE_OBJ_FLAG_TARGET_LISTED) != 0)
            (void)ae_TargetListAdd(r, obj + AE_OBJ_TARGET_NODE);
    }

    ae_wr16(r, obj + AE_OBJ_FACING_X, 0x1000u);
    ae_wr16(r, obj + AE_OBJ_UNK_66, 3u);
    ae_wr16(r, obj + AE_OBJ_FACING_Z, 0x1000u);
    ae_wr16(r, obj + AE_OBJ_BODY_FLAGS, 0x8103u);
    ae_wr16(r, obj + AE_OBJ_BODY_RADIUS, 0x30u);
    ae_wr16(r, obj + AE_OBJ_SHADOW_SIZE, 0x30u);
    ae_wr16(r, obj + AE_OBJ_BODY_HEIGHT, 0x80u);
    ae_wr16(r, obj + AE_OBJ_FACING_Y, 0);
    ae_wr16(r, obj + AE_OBJ_BODY_PARAM, 1u);
    ae_wr8(r, obj + AE_OBJ_MOTION_STATE, 0);
    ae_wr8(r, obj + AE_OBJ_LAST_HIT_TYPE, 0);
    ae_wr8(r, obj + AE_OBJ_HIT_COUNT, 0);
    ae_wr8(r, obj + AE_OBJ_ANIM_INDEX, 0);
    ae_wr8(r, obj + AE_OBJ_AI_STATE, 1u);
    ae_wr8(r, obj + AE_OBJ_COOLDOWN, 0);
    ae_wr8(r, obj + AE_OBJ_SUB_STATE, 0);
    ae_wr8(r, obj + AE_OBJ_PATH_INDEX, 0);
    ae_wr16(r, obj + AE_OBJ_UNK_D4, 0);
    ae_wr16(r, obj + AE_OBJ_UNK_D6, 0);
    ae_wr32(r, obj + AE_OBJ_TARGET_BODY, 0);
    ae_wr8(r, obj + AE_OBJ_FLASH_TIMER, 0);
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_UNK_17);
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_UNK_19);
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_UNK_6);
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_UNK_22_23);
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_UNK_20);
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_UNK_21);
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_BODY_UNREGISTERED);
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_UNK_7);
    ae_wr16(r, obj + AE_OBJ_UNK_B0, 22u);
    ae_wr8(r, obj + AE_OBJ_HP, 2u);
    ae_wr8(r, obj + AE_OBJ_SCALE, 10u);
    ae_wr8(r, obj + AE_OBJ_SPAWN_SLOT, ae_rd8(r, obj + AE_OBJ_SLOT));
    ae_wr8(r, obj + AE_OBJ_SPAWN_INDEX, AE_ZAKO_NO_SPAWN_INDEX);
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_UNK_26);
    ae_wr8(r, obj + AE_OBJ_LOOP_SLOT, 0xFFu);
    ae_wr8(r, obj + AE_OBJ_DEFENSE, 5u);
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_UNK_9);
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_UNK_24_25);
    ae_wr8(r, obj + AE_OBJ_DROP_TABLE, 99u);
    ae_wr8(r, obj + AE_OBJ_IDLE_PHASE, 0);
    ae_wr16(r, obj + AE_OBJ_UNK_BC, 0);
    ae_wr16(r, obj + AE_OBJ_UNK_C0, 0);
    ae_wr16(r, obj + AE_OBJ_UNK_C2, 0);
    ae_wr16(r, obj + AE_OBJ_UNK_C6, 0);
    ae_wr16(r, obj + AE_OBJ_ANIM_INDEX, 0);
    ae_wr8(r, obj + AE_OBJ_REACT_ANIM, 0);
    ae_wr8(r, obj + AE_OBJ_PREV_ANIM_SLOT, 0);
    ae_wr32(r, obj + AE_OBJ_HIT_TARGET, 0);
    ae_wr32(r, obj + AE_OBJ_HIT_TARGET_AUX, 0);
    ae_wr32(r, obj + AE_OBJ_LINK0, 0);
    ae_wr32(r, obj + AE_OBJ_LINK1, 0);

    flags = ae_rd32(r, obj + AE_OBJ_FLAGS) | AE_OBJ_FLAG_UNK_12;
    ae_wr8(r, obj + AE_OBJ_DEFENSE, 5u);
    ae_wr32(r, obj + AE_OBJ_FLAGS, flags);
    flags |= AE_OBJ_FLAG_UNK_15;
    flags &= ~AE_OBJ_FLAG_UNK_13;
    ae_wr32(r, obj + AE_OBJ_FLAGS, flags);
}

void ae_ZakoSpawnFromEntry(AeRam *r, const AeZakoHooks *h, uint32_t obj,
                           uint32_t entry, int32_t entry_index) {
    uint32_t type = ae_rd8(r, entry + AE_ZED_ENTRY_TYPE);
    ae_wr8(r, obj + AE_OBJ_TYPE, type);
    ae_wr32(r, obj + AE_OBJ_POS_X,
            (uint32_t)ae_rds16(r, entry + AE_ZED_ENTRY_X));
    ae_wr32(r, obj + AE_OBJ_POS_Y,
            (uint32_t)ae_rds16(r, entry + AE_ZED_ENTRY_Y));
    ae_wr32(r, obj + AE_OBJ_POS_Z,
            (uint32_t)ae_rds16(r, entry + AE_ZED_ENTRY_Z));
    ae_wr16(r, obj + AE_OBJ_SENSE_RADIUS,
            ae_rd16(r, entry + AE_ZED_ENTRY_SENSE_RADIUS));
    ae_wr16(r, obj + AE_OBJ_RADIUS,
            ae_rd16(r, entry + AE_ZED_ENTRY_RADIUS));
    uint32_t collider_index = ae_rd16(r, entry + AE_ZED_ENTRY_COLLIDER);
    ae_wr16(r, obj + AE_OBJ_PATH_POINT, 0);
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_UNK_3);
    ae_wr16(r, obj + AE_OBJ_OWN_COLLIDER, collider_index);

    uint32_t path_index = ae_rd16(r, entry + AE_ZED_ENTRY_PATH_INDEX);
    if (path_index != 0xFFFFu)
        ae_ObjSetPathFromTable(r, obj, (int32_t)path_index);

    ae_ZakoSetupType(r, h, obj, type);
    if (ae_ram_failed(r)) return;

    uint32_t obj_slot = ae_rd8(r, obj + AE_OBJ_SLOT);
    ae_wr8(r, AE_G_ZAKO_SLOT_BY_ENTRY + (uint32_t)entry_index, obj_slot);
    type = ae_rd8(r, obj + AE_OBJ_TYPE);
    int32_t signed_slot = (int8_t)ae_rd8(r, obj + AE_OBJ_SLOT);
    ae_ZakoSetAreaMode(r, h, obj, type, signed_slot);
    if (ae_ram_failed(r)) return;

    ae_wr8(r, entry + AE_ZED_ENTRY_ACTIVE, 1u);
    ae_wr8(r, obj + AE_OBJ_SPAWN_INDEX, (uint32_t)entry_index);
}

void ae_ZakoClearSlotTable(AeRam *r) {
    for (uint32_t i = 0; i < AE_ZAKO_PROGRESS_WORD_COUNT; ++i)
        ae_wr32(r, AE_G_ZAKO_PROGRESS + i * AE_ZAKO_PROGRESS_WORD_SIZE, 0);
}

void ae_ObjUnregisterBody(AeRam *r, const AeZakoHooks *h, uint32_t obj) {
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
    if ((flags & (AE_OBJ_FLAG_BODY_ACTIVE | AE_OBJ_FLAG_BODY_UNREGISTERED)) !=
        AE_OBJ_FLAG_BODY_ACTIVE)
        return;

    (void)ae_CollisionBodyUnregister(r, h ? h->body_hooks : NULL,
                                     obj + AE_OBJ_POS_X);
    flags = ae_rd32(r, obj + AE_OBJ_FLAGS) | AE_OBJ_FLAG_BODY_UNREGISTERED;
    ae_wr32(r, obj + AE_OBJ_FLAGS, flags);
    if ((flags & AE_OBJ_FLAG_TARGET_TRACKED) != 0)
        ae_TargetListRemove(r, obj + AE_OBJ_TARGET_NODE);
}

void ae_ObjKill(AeRam *r, const AeZakoHooks *h, uint32_t obj, int32_t count_as_kill) {
    if (count_as_kill != 0) {
        uint32_t kills = ae_rd16(r, AE_G_OBJ_KILL_COUNT);
        ae_wr16(r, AE_G_OBJ_KILL_COUNT, kills + 1u);
    }

    ae_wr16(r, obj + AE_OBJ_ANIM_INDEX, 1u);
    ae_wr32(r, obj + AE_OBJ_FLAGS,
            ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_SPAWNED);
    ae_ObjUnregisterBody(r, h, obj);

    uint32_t spawn_index = ae_rd8(r, obj + AE_OBJ_SPAWN_INDEX);
    if (spawn_index != AE_ZAKO_NO_SPAWN_INDEX) {
        uint32_t entry = ae_rd32(r, AE_G_ZAKO_ENTRY_TABLE) +
                         spawn_index * AE_ZED_ENTRY_SIZE;
        ae_wr8(r, entry + AE_ZED_ENTRY_ACTIVE, 0);
    }
}

void ae_ZakoSetAreaMode(AeRam *r, const AeZakoHooks *h, uint32_t obj,
                        uint32_t type, int32_t entry_index) {
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS) | AE_OBJ_FLAG_AREA_MODE;
    ae_wr32(r, obj + AE_OBJ_FLAGS, flags);

    switch (type) {
    case 0x3Du:
        ae_wr8(r, AE_G_ZAKO_MODE_KIND, 3u);
        ae_wr8(r, AE_G_ZAKO_MODE_ENTRY_INDEX, (uint32_t)entry_index);
        break;
    case 0x23u:
        ae_wr8(r, AE_G_ZAKO_MODE_KIND, 1u);
        ae_wr8(r, AE_G_ZAKO_MODE_ENTRY_INDEX, (uint32_t)entry_index);
        break;
    case 0x57u:
        ae_wr8(r, AE_G_ZAKO_MODE_KIND, 4u);
        ae_wr8(r, AE_G_ZAKO_MODE_ENTRY_INDEX, (uint32_t)entry_index);
        break;
    case 0x2Eu:
    case 0x45u:
    case 0x40u:
        ae_wr8(r, AE_G_ZAKO_MODE_KIND, 2u);
        ae_wr8(r, AE_G_ZAKO_MODE_ENTRY_INDEX, (uint32_t)entry_index);
        break;
    case 0x1Au:
        ae_wr8(r, AE_G_ZAKO_MODE_COUNTER,
               ae_rd8(r, AE_G_ZAKO_MODE_COUNTER) + 1u);
        break;
    case 0x6Fu:
        if (ae_rd16(r, AE_G_STAGE_STATE + AE_ZAKO_STAGE_AREA) == 0x4Cu) {
            ae_ObjKill(r, h, obj, 0);
        }
        break;
    default:
        ae_wr32(r, obj + AE_OBJ_FLAGS,
                ae_rd32(r, obj + AE_OBJ_FLAGS) & ~AE_OBJ_FLAG_AREA_MODE);
        break;
    }
}

int32_t ae_ZakoFindEntryByType(AeRam *r, uint32_t type) {
    int32_t entry_count = ae_rds16(r, AE_G_ZAKO_ENTRY_COUNT);
    uint32_t entries = ae_rd32(r, AE_G_ZAKO_ENTRY_TABLE);
    if (entry_count <= 0) return -1;

    for (int32_t i = 0; i < entry_count; ++i) {
        uint32_t entry = entries + (uint32_t)i * AE_ZED_ENTRY_SIZE;
        if (ae_rd16(r, entry + AE_ZED_ENTRY_TYPE) == type) return i;
    }
    return -1;
}

void ae_ObjInitModel(AeRam *r, uint32_t obj, uint32_t type) {
    int32_t entry_index = ae_ZakoFindEntryByType(r, type);
    if (entry_index < 0) return;

    uint32_t entry = ae_rd32(r, AE_G_ZAKO_ENTRY_TABLE) +
                     (uint32_t)entry_index * AE_ZED_ENTRY_SIZE;
    uint32_t model_header = ae_rd32(r, AE_G_AREA_ZED) +
                            ae_rd32(r, entry + AE_ZED_ENTRY_MODEL_OFFSET);
    ae_ModelInitAuto(r, obj, model_header);
}

uint32_t ae_ZakoGetAnimTable(AeRam *r, uint32_t obj, uint32_t type) {
    int32_t entry_index = ae_ZakoFindEntryByType(r, type);
    if (entry_index < 0) return 0;

    uint32_t entry = ae_rd32(r, AE_G_ZAKO_ENTRY_TABLE) +
                     (uint32_t)entry_index * AE_ZED_ENTRY_SIZE;
    uint32_t animation_header = ae_rd32(r, AE_G_AREA_ZED) +
                                ae_rd32(r, entry + AE_ZED_ENTRY_ANIM_OFFSET);
    uint32_t clip_count = ae_rd16(r, animation_header);
    uint32_t anim_table = animation_header + AE_ANIM_HEADER_CLIP_TABLE +
                          clip_count * AE_ANIM_CLIP_RECORD_SIZE;
    ae_wr32(r, obj + AE_OBJ_ANIM_SET, anim_table);
    return animation_header + AE_ANIM_HEADER_CLIP_TABLE;
}

void ae_ZakoSetAnimFromEntry(AeRam *r, uint32_t obj, uint32_t type) {
    uint32_t anim_table = ae_ZakoGetAnimTable(r, obj, type);
    if (anim_table == 0) return;

    ae_wr32(r, obj + AE_OBJ_ANIM_TABLE, anim_table);
    ae_wr16(r, obj + AE_OBJ_ANIM_FRAME, 0);
    ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 0);
}

void ae_ZakoHooksReset(AeRam *r) {
    ae_wr8(r, AE_G_ZAKO_HOOK_COUNT, 0);
    for (uint32_t i = 0; i < AE_ZAKO_HOOK_ENTRY_COUNT; ++i)
        ae_wr32(r, AE_G_ZAKO_HOOKS + i * AE_ZAKO_HOOK_POINTER_SIZE, 0);
}

void ae_ObjSetPathFromTable(AeRam *r, uint32_t obj, int32_t table_index) {
    uint32_t index = (uint32_t)table_index & 0xFFFFu;
    uint32_t entry = ae_rd32(r, AE_G_ZAKO_PATH_TABLE) + index * AE_ZED_PATH_ENTRY_SIZE;
    uint32_t area_zed = ae_rd32(r, AE_G_AREA_ZED);
    uint32_t path_offset = ae_rd32(r, entry + AE_ZED_PATH_DATA_OFFSET);

    ae_wr16(r, obj + AE_OBJ_PATH_POINT,
            ae_rd16(r, entry + AE_ZED_PATH_POINT_COUNT));
    ae_wr32(r, obj + AE_OBJ_PATH, area_zed + path_offset);
}

int32_t ae_GetLevelFirstArea(AeRam *r, int32_t level) {
    uint32_t entry = AE_G_LEVEL_FIRST_AREAS +
                     (uint32_t)level * AE_LEVEL_FIRST_AREA_ENTRY_SIZE;
    return ae_rds16(r, entry);
}

void ae_MenuItemInitToggle(AeRam *r, const AeZakoHooks *h, uint32_t item,
                           uint32_t variable, uint32_t size) {
    int32_t initial_value;
    ae_wr8(r, item + AE_MENU_ITEM_SIZE_KIND, size);
    ae_wr32(r, item + AE_MENU_ITEM_READ_CALLBACK, AE_MENU_ITEM_TOGGLE_CALLBACK);
    ae_wr32(r, item + AE_MENU_ITEM_VARIABLE, variable);
    ZAKO_RESULT(h, ReadSizedValue, variable, size, 0, 0, initial_value);
    ae_wr32(r, item + AE_MENU_ITEM_VALUE, (uint32_t)initial_value);
    ae_wr32(r, item + AE_MENU_ITEM_RANGE, 1u);
    ae_wr32(r, item + AE_MENU_ITEM_FIRST_RANGE, 0u);
    ae_wr32(r, item + AE_MENU_ITEM_SECOND_RANGE, 1u);
}

void ae_ZakoDebugMenuInit(AeRam *r, const AeZakoHooks *h) {
    static const struct {
        uint8_t item_index;
        uint8_t value_offset;
        uint8_t size_kind;
        uint8_t uses_callback;
    } items[] = {
        {1, 1, 0, 0}, {3, 2, 0, 0}, {4, 3, 0, 0}, {5, 7, 0, 0},
        {6, 13, 0, 0}, {7, 4, 0, 0}, {7, 0, 0, 1}, {8, 10, 0, 0},
        {9, 12, 3, 0}, {10, 11, 0, 0}, {12, 5, 0, 0}, {13, 6, 0, 0},
        {14, 8, 0, 0}, {15, 9, 0, 0},
    };

    ae_wr32(r, AE_G_DEBUG_MENU_ITEM_HEAD, AE_G_ZAKO_DEBUG_ITEMS);
    ae_MenuItemInitToggle(r, h, AE_G_ZAKO_DEBUG_ITEMS,
                          AE_G_ZAKO_DEBUG_VALUES, 0);
    if (ae_ram_failed(r)) return;

    for (uint32_t i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        uint32_t item = AE_G_ZAKO_DEBUG_ITEMS +
                        (uint32_t)items[i].item_index * AE_MENU_ITEM_SIZE;
        uint32_t variable = AE_G_ZAKO_DEBUG_VALUES + items[i].value_offset;
        if (items[i].uses_callback) {
            ZAKO_CALL(h, MenuItemSetCallback, item, AE_G_ZAKO_DEBUG_CALLBACK, 0, 0);
        } else {
            ZAKO_CALL(h, MenuItemInitBound, item, variable, items[i].size_kind, 0);
        }
    }
}
