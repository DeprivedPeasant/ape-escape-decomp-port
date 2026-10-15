#include "ae_gimmick.h"

static int32_t gimmick_s16(uint32_t value) {
    return (int16_t)value;
}

static uint32_t gimmick_record(uint32_t base, int32_t index) {
    return base + (uint32_t)index * AE_GIMMICK_SIZE;
}

static uint32_t collider_record(uint32_t base, int32_t index) {
    return base + (uint32_t)index * AE_COLLIDER_SIZE;
}

static uint32_t prop_record(uint32_t base, int32_t index) {
    return base + (uint32_t)index * AE_AREA_PROP_SIZE;
}

static int32_t call_hook(AeRam *r, const AeGimmickHooks *h, AeGimmickHookFn fn,
                         const char *name, uint32_t a0, uint32_t a1,
                         uint32_t a2, uint32_t a3) {
    if (!h || !fn) {
        ae_ram_fail(r, "gimmick hook %s not provided", name);
        return 0;
    }
    return fn(h->user, r, a0, a1, a2, a3);
}

#define GIMMICK_CALL(name, a0, a1, a2, a3) \
    call_hook(r, h, h ? h->name : NULL, #name, (uint32_t)(a0), (uint32_t)(a1), \
              (uint32_t)(a2), (uint32_t)(a3))

int32_t ae_GimmickDeactivateChain(AeRam *r, uint32_t gimmick) {
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    if (collider_index < 0) return 0;

    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    uint32_t collider = collider_record(colliders, collider_index);
    uint32_t gimmicks = ae_rd32(r, AE_G_GIMMICKS);
    int32_t prop_index;

    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);

    prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
    if (prop_index >= 0) {
        uint32_t props = ae_rd32(r, AE_G_AREA_PROPS);
        uint32_t prop = prop_record(props, prop_index);
        ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0xFFFFu);
    }

    ae_wr16(r, collider + AE_COLLIDER_FLAGS,
            (ae_rd16(r, collider + AE_COLLIDER_FLAGS) & ~AE_COLLIDER_STATE_MASK) |
                AE_COLLIDER_STATE_2);

    int32_t linked_index = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_E);
    if (linked_index >= 0) ae_GimmickDeactivateChain(r, gimmick_record(gimmicks, linked_index));

    linked_index = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_10);
    if (linked_index >= 0) ae_GimmickDeactivateChain(r, gimmick_record(gimmicks, linked_index));

    linked_index = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12);
    if (linked_index >= 0) ae_GimmickDeactivateChain(r, gimmick_record(gimmicks, linked_index));

    ae_wr16(r, gimmick + AE_GIMMICK_PARAM_E, ae_rd16(r, collider + AE_COLLIDER_POS_X));
    ae_wr16(r, gimmick + AE_GIMMICK_PARAM_10, ae_rd16(r, collider + AE_COLLIDER_POS_Y));
    ae_wr16(r, gimmick + AE_GIMMICK_PARAM_12, ae_rd16(r, collider + AE_COLLIDER_POS_Z));
    return 0;
}

int32_t ae_SwingDoorSetPivot(AeRam *r, uint32_t collider, int32_t direction, int32_t pivot_mode) {
    int32_t dir = gimmick_s16((uint32_t)direction);
    int32_t mode = gimmick_s16((uint32_t)pivot_mode);
    int32_t pos_x = ae_rds16(r, collider + AE_COLLIDER_POS_X);
    int32_t pos_z = ae_rds16(r, collider + AE_COLLIDER_POS_Z);
    int32_t half_x = ae_rds16(r, collider + AE_COLLIDER_HALF_X);
    int32_t half_z = ae_rds16(r, collider + AE_COLLIDER_HALF_Z);
    int32_t pivot_x = pos_x;
    int32_t pivot_z = pos_z;

    ae_wr16(r, collider + AE_COLLIDER_FLAGS,
            ae_rd16(r, collider + AE_COLLIDER_FLAGS) & ~AE_COLLIDER_DISABLED);

    if (dir >= 0 && dir < 8) {
        if (mode == 2) {
            switch (dir) {
            case 0: pivot_x = pos_x + half_x - half_z; pivot_z = pos_z - half_x - half_z; break;
            case 1: pivot_x = pos_x - half_x + half_z; pivot_z = pos_z - half_x - half_z; break;
            case 2: pivot_x = pos_x - half_z - half_x; pivot_z = pos_z - half_z + half_x; break;
            case 3: pivot_x = pos_x - half_z - half_x; pivot_z = pos_z + half_z - half_x; break;
            case 4: pivot_x = pos_x - half_x + half_z; pivot_z = pos_z + half_x + half_z; break;
            case 5: pivot_x = pos_x + half_x - half_z; pivot_z = pos_z + half_x + half_z; break;
            case 6: pivot_x = pos_x + half_z + half_x; pivot_z = pos_z + half_z - half_x; break;
            case 7: pivot_x = pos_x + half_z + half_x; pivot_z = pos_z - half_z + half_x; break;
            }
        } else {
            switch (dir) {
            case 0: pivot_x = pos_x - half_z + half_x; pivot_z = pos_z + half_z + half_x; break;
            case 1: pivot_x = pos_x + half_z - half_x; pivot_z = pos_z + half_z + half_x; break;
            case 2: pivot_x = pos_x + half_x + half_z; pivot_z = pos_z + half_x - half_z; break;
            case 3: pivot_x = pos_x + half_x + half_z; pivot_z = pos_z - half_x + half_z; break;
            case 4: pivot_x = pos_x + half_z - half_x; pivot_z = pos_z - half_z - half_x; break;
            case 5: pivot_x = pos_x - half_z + half_x; pivot_z = pos_z - half_z - half_x; break;
            case 6: pivot_x = pos_x - half_x - half_z; pivot_z = pos_z - half_x + half_z; break;
            case 7: pivot_x = pos_x - half_x - half_z; pivot_z = pos_z + half_x - half_z; break;
            }
        }
        ae_wr16(r, collider + AE_COLLIDER_POS_X, (uint32_t)pivot_x);
        ae_wr16(r, collider + AE_COLLIDER_POS_Z, (uint32_t)pivot_z);
    }

    /* The original swaps the X/Z half extents after choosing the pivot. */
    ae_wr16(r, collider + AE_COLLIDER_HALF_X, (uint32_t)half_z);
    ae_wr16(r, collider + AE_COLLIDER_HALF_Z, (uint32_t)half_x);
    return 0;
}

int32_t ae_GimmickActivate(AeRam *r, const AeGimmickHooks *h, int32_t index, uint32_t source_gimmick) {
    int32_t target_index = gimmick_s16((uint32_t)index);
    if (target_index < 0) return 0;

    uint32_t gimmicks = ae_rd32(r, AE_G_GIMMICKS);
    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    uint32_t target = gimmick_record(gimmicks, target_index);
    int32_t collider_index = ae_rds16(r, target + AE_GIMMICK_COLLIDER);
    uint32_t collider = collider_record(colliders, collider_index);
    int32_t type = gimmick_s16(ae_rd16(r, target + AE_GIMMICK_TYPE) - 2u) + 2;

    if (type < 2 || type > 101) return 0;

    switch (type) {
    case 2: {
        uint32_t subtype = ae_rd16(r, target + AE_GIMMICK_SUBTYPE);
        if (subtype < 46 || subtype > 49) return 0;

        GIMMICK_CALL(PlaySfxAt, 0x75u, collider, 0, 0);
        if (ae_ram_failed(r)) return 0;

        int32_t state = ae_rds16(r, target + AE_GIMMICK_STATE);
        int32_t mesh_index;
        if (state == 2) {
            mesh_index = ae_rds16(r, target + AE_GIMMICK_PARAM_E);
            ae_wr16(r, target + AE_GIMMICK_STATE, 0);
        } else if (state == 0) {
            mesh_index = ae_rds16(r, target + AE_GIMMICK_PARAM_12);
            ae_wr16(r, target + AE_GIMMICK_STATE, 2);
        } else {
            return 0;
        }

        int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
        GIMMICK_CALL(PropSetMesh, prop_index, mesh_index, mesh_index, 0);
        return 0;
    }

    case 3:
    case 20: {
        int32_t state = ae_rds16(r, target + AE_GIMMICK_STATE);
        if (state == 0) ae_wr16(r, target + AE_GIMMICK_STATE, 1);
        else if (state == 2) ae_wr16(r, target + AE_GIMMICK_STATE, 3);
        else return 0;
        ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
        return 0;
    }

    case 5: {
        uint32_t collider_type = ae_rd16(r, collider + AE_COLLIDER_TYPE) & 0xF0FFu;
        int32_t mode = ae_rds16(r, target + AE_GIMMICK_PARAM_A);
        ae_wr16(r, collider + AE_COLLIDER_MODE, (uint32_t)mode);
        ae_wr16(r, collider + AE_COLLIDER_TYPE,
                collider_type | ((ae_rd16(r, target + AE_GIMMICK_PARAM_A) & 0x000Eu) ? 0x0400u : 0x0500u));

        int32_t link = ae_rds16(r, target + AE_GIMMICK_LINK);
        ae_GimmickActivate(r, h, link, target);
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, target + AE_GIMMICK_STATE, 2);

        if (ae_rd16(r, AE_G_CURRENT_AREA) == 0x10u) {
            GIMMICK_CALL(PlaySfx, 0xB0u, 0, 0, 0);
        }
        return 0;
    }

    case 6: {
        ae_wr16(r, target + AE_GIMMICK_STATE, 1);
        int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
        if (prop_index >= 0) {
            uint32_t props = ae_rd32(r, AE_G_AREA_PROPS);
            ae_wr16(r, prop_record(props, prop_index) + AE_AREA_PROP_ZONE_MASK, 0xFFFFu);
        }
        {
            uint32_t flags = ae_rd16(r, collider + AE_COLLIDER_FLAGS);
            ae_wr16(r, collider + AE_COLLIDER_FLAGS, (flags & 0xFFF0u) | AE_COLLIDER_STATE_2);
        }
        return 0;
    }

    case 10:
        ae_GimmickDeactivateChain(r, target);
        return 0;

    case 7:
        ae_wr16(r, target + AE_GIMMICK_STATE, 1);
        return 0;

    case 22: {
        if (ae_rd16(r, AE_G_CURRENT_LEVEL) != 6u && ae_rd16(r, AE_G_CURRENT_AREA) != 0x65u) {
            ae_wr16(r, target + AE_GIMMICK_STATE, 1);
        } else {
            GIMMICK_CALL(PlaySfxAt, 0xB0u, collider, 0, 0);
            if (ae_ram_failed(r)) return 0;
            ae_wr16(r, target + AE_GIMMICK_STATE, 1);
        }

        int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
        if (prop_index >= 0) {
            uint32_t props = ae_rd32(r, AE_G_AREA_PROPS);
            uint32_t prop = prop_record(props, prop_index);
            ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0xFFFFu);
        }
        {
            uint32_t flags = ae_rd16(r, collider + AE_COLLIDER_FLAGS);
            ae_wr16(r, collider + AE_COLLIDER_FLAGS, (flags & 0xFFF0u) | AE_COLLIDER_STATE_2);
        }
        return 0;
    }

    case 25: {
        ae_wr16(r, target + AE_GIMMICK_STATE, 1);
        ae_wr16(r, target + AE_GIMMICK_PARAM_E, ae_rd16(r, collider + AE_COLLIDER_POS_X));
        ae_wr16(r, target + AE_GIMMICK_PARAM_10, ae_rd16(r, collider + AE_COLLIDER_POS_Y));
        ae_wr16(r, target + AE_GIMMICK_PARAM_12, ae_rd16(r, collider + AE_COLLIDER_POS_Z));
        uint32_t flags = ae_rd16(r, collider + AE_COLLIDER_FLAGS);
        ae_wr16(r, collider + AE_COLLIDER_FLAGS, (flags & 0xFFF0u) | AE_COLLIDER_STATE_2);
        return 0;
    }

    case 26:
    case 29: {
        int32_t source_state = ae_rds16(r, source_gimmick + AE_GIMMICK_STATE);
        ae_wr16(r, target + AE_GIMMICK_STATE, source_state == 0 ? 1u : 2u);
        if (ae_rd16(r, AE_G_CURRENT_AREA) == 0x15u) {
            GIMMICK_CALL(PlaySfxAt, 0x9Bu, collider, 0, 0);
        }
        return 0;
    }

    case 27: {
        int32_t source_state = ae_rds16(r, source_gimmick + AE_GIMMICK_STATE);
        int32_t source_type = ae_rds16(r, source_gimmick + AE_GIMMICK_TYPE);
        if ((source_state == 0 && source_type == 2) || source_type == 0x1B) {
            int32_t link = ae_rds16(r, target + AE_GIMMICK_LINK);
            ae_GimmickActivate(r, h, link, target);
            if (ae_ram_failed(r)) return 0;
            ae_wr16(r, target + AE_GIMMICK_STATE, 1);
        }
        return 0;
    }

    case 28: {
        int32_t source_state = ae_rds16(r, source_gimmick + AE_GIMMICK_STATE);
        int32_t mesh_index = ae_rds16(r, target +
            (source_state == 0 ? AE_GIMMICK_PARAM_10 : AE_GIMMICK_PARAM_E));
        int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
        GIMMICK_CALL(PropSetMesh, prop_index, mesh_index, mesh_index, 0);
        return 0;
    }

    case 30: {
        int32_t first_index = ae_rds16(r, target + AE_GIMMICK_SUBTYPE);
        int32_t second_index = ae_rds16(r, target + AE_GIMMICK_PARAM_A);
        ae_wr16(r, gimmick_record(gimmicks, first_index) + AE_GIMMICK_STATE, 5);
        ae_wr16(r, gimmick_record(gimmicks, second_index) + AE_GIMMICK_STATE, 5);
        return 0;
    }

    case 31: {
        if (ae_rds16(r, source_gimmick + AE_GIMMICK_STATE) != 0 ||
            ae_rds16(r, target + AE_GIMMICK_STATE) != 0) return 0;
        ae_wr16(r, target + AE_GIMMICK_STATE, 1);
        return 0;
    }

    case 36: {
        if (ae_rds16(r, source_gimmick + AE_GIMMICK_STATE) != 0) return 0;
        if (ae_rds16(r, target + AE_GIMMICK_STATE) == 0) {
            ae_wr16(r, target + AE_GIMMICK_STATE, 1);
        }
        uint32_t following = gimmick_record(gimmicks, target_index + 1);
        if (ae_rds16(r, following + AE_GIMMICK_STATE) == 0) {
            ae_wr16(r, following + AE_GIMMICK_STATE, 1);
        }
        return 0;
    }

    case 38:
    case 39: {
        int32_t state = ae_rds16(r, target + AE_GIMMICK_STATE);
        if (state == 4) ae_wr16(r, target + AE_GIMMICK_STATE, 5);
        else return 0;
        ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
        return 0;
    }

    case 40: {
        if (ae_rds16(r, source_gimmick + AE_GIMMICK_STATE) != 0 ||
            ae_rds16(r, target + AE_GIMMICK_STATE) != 0) return 0;
        int32_t link = ae_rds16(r, target + AE_GIMMICK_LINK);
        ae_GimmickActivate(r, h, link, target);
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, target + AE_GIMMICK_STATE, 1);
        {
            uint32_t flags = ae_rd16(r, collider + AE_COLLIDER_FLAGS);
            ae_wr16(r, collider + AE_COLLIDER_FLAGS, (flags & 0xFFF0u) | AE_COLLIDER_STATE_2);
        }
        return 0;
    }

    case 41:
        if (ae_rds16(r, target + AE_GIMMICK_STATE) != 0) return 0;
        GIMMICK_CALL(FUN_801414a0, target, 0, 0, 0);
        return 0;

    case 44:
        ae_wr16(r, target + AE_GIMMICK_STATE, 2);
        {
            uint32_t flags = ae_rd16(r, collider + AE_COLLIDER_FLAGS);
            ae_wr16(r, collider + AE_COLLIDER_FLAGS, (flags & 0xFFF0u) | AE_COLLIDER_STATE_2);
        }
        return 0;

    case 101: {
        int32_t state = ae_rds16(r, target + AE_GIMMICK_STATE);
        uint32_t direction = ae_rd16(r, target + AE_GIMMICK_SUBTYPE);
        int32_t source_state = ae_rds16(r, source_gimmick + AE_GIMMICK_STATE);
        int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);

        if (state == 1) {
            if (direction < 8) {
                ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                        ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
                ae_wr16(r, target + AE_GIMMICK_STATE, 2);
                return 0;
            }
            if (source_state != 1) return 0;
            if (prop_index >= 0) {
                uint32_t props = ae_rd32(r, AE_G_AREA_PROPS);
                ae_wr16(r, prop_record(props, prop_index) + AE_AREA_PROP_ZONE_MASK, 0xFFFFu);
            }
            ae_wr16(r, target + AE_GIMMICK_STATE, 2);
            return 0;
        }

        if (state == 2) {
            if (direction < 8) {
                ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                        ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
                ae_wr16(r, target + AE_GIMMICK_STATE, 5);
                ae_SwingDoorSetPivot(r, collider, (int32_t)direction, 5);
                return 0;
            }
            if (source_state == 0) ae_wr16(r, target + AE_GIMMICK_STATE, 5);
            return 0;
        }

        if (state == 4) {
            if (direction < 8) {
                ae_wr16(r, target + AE_GIMMICK_STATE, 5);
                ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                        ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
                return 0;
            }
            if (source_state == 0) ae_wr16(r, target + AE_GIMMICK_STATE, 5);
            return 0;
        }

        if (state == 5) {
            if (direction < 8) {
                ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                        ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
                ae_wr16(r, target + AE_GIMMICK_STATE, 2);
                ae_SwingDoorSetPivot(r, collider, (int32_t)direction, 2);
                return 0;
            }
            if (source_state != 1) return 0;
            ae_wr16(r, target + AE_GIMMICK_STATE, 2);
            return 0;
        }

        return 0;
    }

    default:
        return 0;
    }
}

#undef GIMMICK_CALL
