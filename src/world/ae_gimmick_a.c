#include "ae_gimmick_a.h"

#include <stdbool.h>
#include <stdint.h>

static int32_t ga_s16(uint32_t value)
{
    return (int16_t)value;
}

static uint32_t ga_gimmick(uint32_t base, int32_t index)
{
    return base + (uint32_t)index * AE_GIMMICK_SIZE;
}

static uint32_t ga_collider(uint32_t base, int32_t index)
{
    return base + (uint32_t)index * AE_COLLIDER_SIZE;
}

static int32_t ga_hook(AeRam *r, const AeGimmickAHooks *h, AeGimmickAHookFn fn,
                       const char *name, uint32_t a0, uint32_t a1,
                       uint32_t a2, uint32_t a3)
{
    if (!fn) {
        ae_ram_fail(r, "gimmick hook %s not provided", name);
        return 0;
    }
    return fn(h->user, r, a0, a1, a2, a3);
}

static int32_t ga_handler(AeRam *r, const AeGimmickAHooks *h, uint32_t function,
                          uint32_t a0, uint32_t a1)
{
    if (!h || !h->GimmickHandler) {
        ae_ram_fail(r, "gimmick handler hook not provided for %08X", function);
        return 0;
    }
    return h->GimmickHandler(h->user, r, function, a0, a1);
}

static int32_t ga_ground_height(AeRam *r, const AeGimmickAHooks *h,
                                int32_t x, int32_t y, int32_t z)
{
    AeWorldHooks world = {0};
    if (h) {
        world.user = h->user;
        world.GetSolidHeight = h->GetSolidHeight;
        world.ground_stale_x = h->ground_stale_x;
        world.ground_stale_z = h->ground_stale_z;
    }
    return ae_GetGroundHeight(r, &world, x, y, z);
}

#define GA_CALL(name, a0, a1, a2, a3) \
    ga_hook(r, h, h ? h->name : 0, #name, (uint32_t)(a0), (uint32_t)(a1), \
            (uint32_t)(a2), (uint32_t)(a3))

/* 8005AD90 */
int32_t ae_GimmickSaveTableReset(AeRam *r)
{
    for (uint32_t i = 0; i < AE_GA_SAVE_ENTRY_COUNT; ++i) {
        uint32_t entry = AE_GA_SAVE_TABLE + i * AE_GA_SAVE_ENTRY_SIZE;
        ae_wr16(r, entry + AE_GA_SAVE_ENTRY_USED, 0xFFFFu);
        ae_wr16(r, entry + AE_GA_SAVE_ENTRY_STAGE, 0xFFFFu);
        ae_wr16(r, entry + AE_GA_SAVE_ENTRY_GIMMICK_INDEX, 0xFFFFu);
    }

    if (ae_rd8(r, AE_GA_AREA_EXIT_CODE) != 2u) {
        ae_wr16(r, AE_GA_CARRY_OVER_X, 0);
        ae_wr16(r, AE_GA_CARRY_OVER_Y, 0);
        ae_wr16(r, AE_GA_CARRY_OVER_Z, 0);
    }
    return 0;
}

/* 8005BB5C */
int32_t ae_GimmickGetState(AeRam *r, int32_t index)
{
    if (index < 0) return -1;

    uint32_t gimmicks = ae_rd32(r, AE_G_GIMMICKS);
    uint32_t record = ga_gimmick(gimmicks, index);
    return ae_rds16(r, record + AE_GIMMICK_STATE);
}

/* 8005B318 */
int32_t ae_GimmickInitFromProgress(AeRam *r, const AeGimmickAHooks *h)
{
    if (ae_rd16(r, AE_G_CURRENT_LEVEL) == 0x20u) return 0;

    uint32_t area = ae_rd16(r, AE_G_CURRENT_AREA);
    uint32_t handlers = ae_rd32(r, AE_GA_HANDLER_TABLE + area * 4u);
    if (handlers == 0 || ae_rd32(r, handlers + AE_GA_HANDLER_INIT) == 0) return 0;

    uint32_t groups = ae_rd32(r, AE_GA_GIMMICK_GROUPS);
    uint32_t gimmicks = ae_rd32(r, AE_G_GIMMICKS);

    for (uint32_t group_index = 0;
         ae_rd32(r, handlers + group_index * AE_GA_HANDLER_RECORD_SIZE + AE_GA_HANDLER_INIT) != 0;
         ++group_index) {
        uint32_t group = groups + (uint32_t)group_index * AE_GA_GROUP_RECORD_SIZE;
        int32_t gimmick_count = ae_rds16(r, group + AE_GA_GROUP_COUNT_FIELD);
        int32_t first_index = ae_rds16(r, group + AE_GA_GROUP_FIRST_GIMMICK);
        for (int32_t i = 0; i < gimmick_count; ++i) {
            uint32_t gimmick = ga_gimmick(gimmicks, first_index + i);
            int32_t type = ae_rds16(r, gimmick + AE_GIMMICK_TYPE);

            if (type == 0x22) {
                int32_t progress_id = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12);
                int32_t enabled = GA_CALL(ProgressFlagGet, (uint32_t)progress_id, 0, 0, 0);
                if (ae_ram_failed(r)) return 0;
                if (enabled != 0) ae_wr16(r, gimmick + AE_GIMMICK_SUBTYPE, 1);
            } else if (type == 0x32) {
                int32_t progress_id = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A);
                int32_t value = GA_CALL(ProgressGetByte, (uint32_t)progress_id, 0, 0, 0);
                if (ae_ram_failed(r)) return 0;
                ae_wr16(r, gimmick + AE_GIMMICK_SUBTYPE, (uint32_t)value);
            }
        }
    }
    return 0;
}

/* 8005B130 */
int32_t ae_GimmickSaveEntryAdd(AeRam *r, int32_t index, uint32_t gimmick)
{
    uint32_t current_area = ae_rd16(r, AE_G_CURRENT_AREA);

    for (uint32_t slot = 0; slot < AE_GA_SAVE_ENTRY_COUNT; ++slot) {
        uint32_t entry = AE_GA_SAVE_TABLE + slot * AE_GA_SAVE_ENTRY_SIZE;
        if (ae_rds16(r, entry + AE_GA_SAVE_ENTRY_USED) >= 0) continue;

        ae_wr16(r, entry + AE_GA_SAVE_ENTRY_USED, 1);
        ae_wr16(r, entry + AE_GA_SAVE_ENTRY_STAGE, current_area);
        ae_wr16(r, entry + AE_GA_SAVE_ENTRY_GIMMICK_INDEX, (uint32_t)index);
        for (uint32_t offset = 0; offset < AE_GIMMICK_SIZE; offset += 2) {
            ae_wr16(r, entry + AE_GA_SAVE_ENTRY_DATA + offset, ae_rd16(r, gimmick + offset));
        }

        int32_t type = ae_rds16(r, entry + AE_GA_SAVE_ENTRY_TYPE);
        uint32_t state = ae_rd16(r, entry + AE_GA_SAVE_ENTRY_DATA + AE_GIMMICK_STATE);
        uint32_t subtype = ae_rd16(r, entry + AE_GA_SAVE_ENTRY_DATA + AE_GIMMICK_SUBTYPE);
        switch (type) {
        case 0x02:
        case 0x03:
        case 0x05:
        case 0x07:
        case 0x09:
        case 0x0C:
        case 0x0D:
        case 0x17:
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
            ae_wr16(r, entry + AE_GA_SAVE_ENTRY_FIELD_10, state);
            break;
        case 0x06:
            ae_wr16(r, entry + AE_GA_SAVE_ENTRY_FIELD_10, state == 3 ? 1u : 0u);
            break;
        case 0x18: {
            uint32_t packed = (state << 8) | subtype;
            ae_wr16(r, entry + AE_GA_SAVE_ENTRY_FIELD_10, packed);
            break;
        }
        case 0x0A:
        case 0x29:
        case 0x2E:
        case 0x30:
            ae_wr16(r, entry + AE_GA_SAVE_ENTRY_FIELD_1A, state != 0 ? 1u : 0u);
            break;
        case 0x2A:
        case 0x2D:
        case 0x2F:
            ae_wr16(r, entry + AE_GA_SAVE_ENTRY_FIELD_1A, state != 0 ? 2u : 0u);
            break;
        case 0x25:
            ae_wr16(r, entry + AE_GA_SAVE_ENTRY_FIELD_0E, state);
            break;
        case 0x27:
            ae_wr16(r, entry + AE_GA_SAVE_ENTRY_FIELD_1A, state == 4 ? state : 1u);
            break;
        case 0x32:
            if (state < 6) {
                static const uint16_t saved_modes[6] = { 0, 2, 2, 2, 5, 5 };
                ae_wr16(r, entry + AE_GA_SAVE_ENTRY_FIELD_0E, saved_modes[state]);
            }
            break;
        default:
            break;
        }
        return 0;
    }
    return -1;
}

/* 8005ADF8 */
int32_t ae_GimmickSaveStates(AeRam *r)
{
    if (ae_rd16(r, AE_G_CURRENT_LEVEL) == 0x20u) return 0;

    uint32_t groups = ae_rd32(r, AE_GA_GIMMICK_GROUPS);
    int32_t group_count = ae_rds32(r, AE_GA_GIMMICK_GROUP_COUNT);
    uint32_t gimmicks = ae_rd32(r, AE_G_GIMMICKS);
    for (int32_t group_index = 0; group_index < group_count; ++group_index) {
        uint32_t group = groups + (uint32_t)group_index * AE_GA_GROUP_RECORD_SIZE;
        int32_t gimmick_count = ae_rds16(r, group + AE_GA_GROUP_COUNT_FIELD);
        int32_t first_index = ae_rds16(r, group + AE_GA_GROUP_FIRST_GIMMICK);
        for (int32_t i = 0; i < gimmick_count; ++i) {
            int32_t index = first_index + i;
            uint32_t gimmick = ga_gimmick(gimmicks, index);
            int32_t type = ae_rds16(r, gimmick + AE_GIMMICK_TYPE);
            bool persistent = false;
            switch (type) {
            case 0x02: {
                int32_t subtype = ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE);
                persistent = subtype < 0x37 || subtype > 0x3A;
                break;
            }
            case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
            case 0x09: case 0x0A: case 0x0C: case 0x0D: case 0x0E:
            case 0x17: case 0x18: case 0x20: case 0x21: case 0x22:
            case 0x23: case 0x25: case 0x27: case 0x29: case 0x2A:
            case 0x2D: case 0x2E: case 0x2F: case 0x30: case 0x32:
                persistent = true;
                break;
            default:
                break;
            }
            if (persistent) (void)ae_GimmickSaveEntryAdd(r, index, gimmick);
        }
    }
    return 0;
}

/* 8005AF4C */
int32_t ae_GimmickRestoreStates(AeRam *r)
{
    uint32_t current_area = ae_rd16(r, AE_G_CURRENT_AREA);
    uint32_t gimmicks = ae_rd32(r, AE_G_GIMMICKS);
    for (uint32_t slot = 0; slot < AE_GA_SAVE_ENTRY_COUNT; ++slot) {
        uint32_t entry = AE_GA_SAVE_TABLE + slot * AE_GA_SAVE_ENTRY_SIZE;
        if (ae_rds16(r, entry + AE_GA_SAVE_ENTRY_USED) < 0) continue;
        if (ae_rd16(r, entry + AE_GA_SAVE_ENTRY_STAGE) != current_area) continue;

        int32_t index = ae_rds16(r, entry + AE_GA_SAVE_ENTRY_GIMMICK_INDEX);
        uint32_t gimmick = ga_gimmick(gimmicks, index);
        int32_t type = ae_rds16(r, entry + AE_GA_SAVE_ENTRY_TYPE);
        switch (type) {
        case 0x02: case 0x03: case 0x09: case 0x0C: case 0x0D: case 0x17:
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_A,
                    ae_rd16(r, entry + AE_GA_SAVE_ENTRY_FIELD_10));
            break;
        case 0x04:
            ae_wr16(r, gimmick + AE_GIMMICK_LINK,
                    ae_rd16(r, entry + AE_GA_SAVE_ENTRY_FIELD_12));
            break;
        case 0x05: case 0x07: case 0x20: case 0x21: case 0x22: case 0x23:
            ae_wr16(r, gimmick + AE_GIMMICK_SUBTYPE,
                    ae_rd16(r, entry + AE_GA_SAVE_ENTRY_FIELD_10));
            break;
        case 0x06:
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_14,
                    ae_rd16(r, entry + AE_GA_SAVE_ENTRY_FIELD_10));
            break;
        case 0x0A: case 0x27: case 0x29: case 0x2A: case 0x2D: case 0x2E:
        case 0x2F: case 0x30:
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_14,
                    ae_rd16(r, entry + AE_GA_SAVE_ENTRY_FIELD_1A));
            break;
        case 0x0E:
            ae_wr16(r, gimmick + AE_GIMMICK_SUBTYPE,
                    ae_rd16(r, entry + AE_GA_SAVE_ENTRY_FIELD_0E));
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_E,
                    ae_rd16(r, entry + AE_GA_SAVE_ENTRY_FIELD_14));
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_10,
                    ae_rd16(r, entry + AE_GA_SAVE_ENTRY_FIELD_16));
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_12,
                    ae_rd16(r, entry + AE_GA_SAVE_ENTRY_FIELD_18));
            break;
        case 0x18:
            ae_wr16(r, gimmick + AE_GIMMICK_SUBTYPE,
                    ae_rd16(r, entry + AE_GA_SAVE_ENTRY_FIELD_10));
            break;
        case 0x25:
            ae_wr16(r, gimmick + AE_GIMMICK_SUBTYPE,
                    ae_rd16(r, entry + AE_GA_SAVE_ENTRY_FIELD_0E));
            ae_wr16(r, gimmick + AE_GIMMICK_LINK,
                    ae_rd16(r, entry + AE_GA_SAVE_ENTRY_FIELD_12));
            break;
        case 0x32:
            ae_wr16(r, gimmick + AE_GIMMICK_SUBTYPE,
                    ae_rd16(r, entry + AE_GA_SAVE_ENTRY_FIELD_0E));
            break;
        default:
            break;
        }
        ae_wr16(r, entry + AE_GA_SAVE_ENTRY_USED, 0xFFFFu);
        ae_wr16(r, entry + AE_GA_SAVE_ENTRY_STAGE, 0xFFFFu);
        ae_wr16(r, entry + AE_GA_SAVE_ENTRY_GIMMICK_INDEX, 0xFFFFu);
    }
    return 0;
}

/* 8005B480 */
int32_t ae_GimmickSaveApplyProgress(AeRam *r, const AeGimmickAHooks *h)
{
    for (uint32_t i = 0; i < AE_GA_SAVE_ENTRY_COUNT; ++i) {
        uint32_t entry = AE_GA_SAVE_TABLE + i * AE_GA_SAVE_ENTRY_SIZE;
        if (ae_rds16(r, entry + AE_GA_SAVE_ENTRY_USED) < 0) continue;

        int32_t type = ae_rds16(r, entry + AE_GA_SAVE_ENTRY_DATA + AE_GIMMICK_TYPE);
        if (type == 0x22 && ae_rds16(r, entry + AE_GA_SAVE_ENTRY_FIELD_10) != 0) {
            int32_t progress_id = ae_rds16(r, entry + AE_GA_SAVE_ENTRY_FIELD_18);
            GA_CALL(ProgressFlagSet, (uint32_t)progress_id, 0, 0, 0);
            if (ae_ram_failed(r)) return 0;
        }
        if (type == 0x32) {
            int32_t value = ae_rds16(r, entry + AE_GA_SAVE_ENTRY_FIELD_10);
            int32_t progress_id = ae_rds16(r, entry + AE_GA_SAVE_ENTRY_FIELD_0E);
            GA_CALL(ProgressSetByte, (uint32_t)value, (uint32_t)progress_id, 0, 0);
            if (ae_ram_failed(r)) return 0;
        }
    }
    return 0;
}

/* 8005B538 */
int32_t ae_GimmickSpawnAll(AeRam *r, const AeGimmickAHooks *h)
{
    uint32_t area = ae_rd16(r, AE_G_CURRENT_AREA);
    uint32_t handlers = ae_rd32(r, AE_GA_HANDLER_TABLE + area * 4u);
    if (handlers == 0) {
        ae_wr32(r, AE_GA_GIMMICK_GROUP_COUNT, 0);
        return 0;
    }

    ae_GimmickRestoreStates(r);
    ae_GimmickInitFromProgress(r, h);
    if (ae_ram_failed(r)) return 0;

    uint32_t groups = ae_rd32(r, AE_GA_GIMMICK_GROUPS);
    uint32_t gimmicks = ae_rd32(r, AE_G_GIMMICKS);
    int32_t group_count = ae_rds32(r, AE_GA_GIMMICK_GROUP_COUNT);
    for (int32_t group_index = 0; ; ++group_index) {
        uint32_t handler = handlers + (uint32_t)group_index * AE_GA_HANDLER_RECORD_SIZE;
        uint32_t init_function = ae_rd32(r, handler + AE_GA_HANDLER_INIT);
        if (init_function == 0) break;

        if (group_index < group_count) {
            uint32_t group = groups + (uint32_t)group_index * AE_GA_GROUP_RECORD_SIZE;
            ae_wr32(r, handler + AE_GA_HANDLER_GROUP_STATE, group);
            ae_wr16(r, group + AE_GA_GROUP_FLAGS, 0);
            int32_t count = ae_rds16(r, group + AE_GA_GROUP_COUNT_FIELD);
            int32_t first = ae_rds16(r, group + AE_GA_GROUP_FIRST_GIMMICK);
            for (int32_t i = 0; i < count; ++i) {
                uint32_t gimmick = ga_gimmick(gimmicks, first + i);
                (void)ga_handler(r, h, init_function, gimmick, 0);
                if (ae_ram_failed(r)) return 0;
            }
        } else {
            (void)ga_handler(r, h, init_function, 0, 0);
            if (ae_ram_failed(r)) return 0;
        }
    }
    return 0;
}

/* 8005B68C */
int32_t ae_GimmickUpdateAll(AeRam *r, const AeGimmickAHooks *h)
{
    uint32_t area = ae_rd16(r, AE_G_CURRENT_AREA);
    uint32_t handlers = ae_rd32(r, AE_GA_HANDLER_TABLE + area * 4u);
    if (handlers == 0) return 0;

    ae_wr16(r, AE_GA_UPDATE_GROUP_STATE, 0xFFFFu);
    if (ae_rd32(r, handlers + AE_GA_HANDLER_UPDATE) == 0) return 0;

    uint32_t gimmicks = ae_rd32(r, AE_G_GIMMICKS);
    for (int32_t group_index = 0; ; ++group_index) {
        uint32_t handler = handlers + (uint32_t)group_index * AE_GA_HANDLER_RECORD_SIZE;
        uint32_t update_function = ae_rd32(r, handler + AE_GA_HANDLER_UPDATE);
        if (update_function == 0) break;

        uint32_t group = ae_rd32(r, handler + AE_GA_HANDLER_GROUP_STATE);
        if (group == 0) {
            (void)ga_handler(r, h, update_function, 0, 0);
            if (ae_ram_failed(r)) return 0;
            continue;
        }
        if ((ae_rd32(r, AE_GA_STAGE_ENTRY_FLAGS) & 8u) != 0) continue;

        int32_t count = ae_rds16(r, group + AE_GA_GROUP_COUNT_FIELD);
        int32_t first = ae_rds16(r, group + AE_GA_GROUP_FIRST_GIMMICK);
        for (int32_t i = 0; i < count; ++i) {
            uint32_t gimmick = ga_gimmick(gimmicks, first + i);
            (void)ga_handler(r, h, update_function, gimmick, 0);
            if (ae_ram_failed(r)) return 0;
        }
    }
    return 0;
}

/* 8005BB98: the low five bits select the original 16-entry sound table. */
int32_t ae_GimmickTypeToSfx(AeRam *r, int32_t type)
{
    static const int16_t sounds[16] = {
        130, 130, 129, 129, 128, 128, -1, 42,
        117, -1, -1, -1, -1, 126, -1, 177
    };
    int32_t table_index = ga_s16(((uint32_t)type & 0x1Fu) - 2u);
    if ((uint32_t)table_index >= 16u) return -1;
    (void)r;
    return sounds[table_index];
}

/* 8008EAE4 */
int32_t ae_GimmickPlayTypeSfx(AeRam *r, const AeGimmickAHooks *h, uint32_t collider)
{
    int32_t sfx = ae_GimmickTypeToSfx(r, ae_rd16(r, collider + AE_GA_COLLIDER_SFX_TYPE));
    if (sfx < 0) return 0;

    if ((ae_rd32(r, AE_GA_STAGE_ENTRY_FLAGS) & 1u) != 0) {
        GA_CALL(PlaySfx, (uint32_t)sfx, 0, 0, 0);
    } else {
        GA_CALL(PlaySfxAt, (uint32_t)sfx, collider, 0, 0);
    }
    return 0;
}

/* 8008EB58 */
int32_t ae_GimmickAnyBodyInside(AeRam *r, const AeGimmickAHooks *h,
                                uint32_t collider, int32_t radius_arg, uint32_t arg)
{
    int32_t radius = ga_s16((uint32_t)radius_arg);
    if (GA_CALL(BodyInsideBox, AE_GA_PLAYER + AE_GA_ACTOR_POSITION,
                collider, (uint32_t)radius, arg) != 0) return 1;
    if (ae_ram_failed(r)) return 0;

    if (ae_rd32(r, AE_GA_RC_CAR + AE_GA_RC_CAR_ACTIVE) != 0) {
        if (GA_CALL(BodyInsideBox, AE_GA_RC_CAR + AE_GA_ACTOR_POSITION,
                    collider, (uint32_t)radius, arg) != 0) return 1;
        if (ae_ram_failed(r)) return 0;
    }

    for (uint32_t slot = 0; slot < 8; ++slot) {
        uint32_t monkey = AE_GA_MONKEYS + slot * AE_GA_MONKEY_SIZE;
        if (ae_rd8(r, monkey + AE_GA_MONKEY_ACTIVITY) != 4u) continue;
        if (GA_CALL(BodyInsideBox, monkey + AE_GA_ACTOR_POSITION,
                    collider, (uint32_t)radius, arg) != 0) return 1;
        if (ae_ram_failed(r)) return 0;
    }
    return 0;
}

/* 8008AA6C */
int32_t ae_GimmickCarryBodiesByDelta(AeRam *r, const AeGimmickAHooks *h,
                                     uint32_t gimmick, uint32_t delta, int32_t collider_index_arg)
{
    int32_t dx = ae_rds16(r, delta + AE_GA_DELTA_X);
    int32_t dy = ae_rds16(r, delta + AE_GA_DELTA_Y);
    int32_t dz = ae_rds16(r, delta + AE_GA_DELTA_Z);
    int32_t collider_index = ga_s16((uint32_t)collider_index_arg);
    int32_t abs_dx = dx < 0 ? -dx : dx;
    int32_t abs_dy = dy < 0 ? -dy : dy;
    int32_t abs_dz = dz < 0 ? -dz : dz;
    (void)gimmick;
    if (abs_dx >= 513 || abs_dy >= 1025 || abs_dz >= 513) return 0;

    uint32_t player = AE_GA_PLAYER;
    if (ae_rds16(r, player + AE_GA_PLAYER_BODY_COLLIDER) == collider_index) {
        uint32_t position = player + AE_GA_ACTOR_POSITION;
        ae_wr32(r, position + AE_GA_POSITION_X,
                ae_rd32(r, position + AE_GA_POSITION_X) + (uint32_t)dx);
        ae_wr32(r, position + AE_GA_POSITION_Y,
                ae_rd32(r, position + AE_GA_POSITION_Y) + (uint32_t)dy);
        ae_wr32(r, position + AE_GA_POSITION_Z,
                ae_rd32(r, position + AE_GA_POSITION_Z) + (uint32_t)dz);
        GA_CALL(CameraNudge, (uint32_t)dx, (uint32_t)dy, (uint32_t)dz, 0);
        if (ae_ram_failed(r)) return 0;
    }

    uint32_t rc_car = AE_GA_RC_CAR;
    if (ae_rds16(r, rc_car + AE_GA_PLAYER_BODY_COLLIDER) == collider_index) {
        uint32_t position = rc_car + AE_GA_ACTOR_POSITION;
        ae_wr32(r, position + AE_GA_POSITION_X,
                ae_rd32(r, position + AE_GA_POSITION_X) + (uint32_t)dx);
        ae_wr32(r, position + AE_GA_POSITION_Y,
                ae_rd32(r, position + AE_GA_POSITION_Y) + (uint32_t)dy);
        ae_wr32(r, position + AE_GA_POSITION_Z,
                ae_rd32(r, position + AE_GA_POSITION_Z) + (uint32_t)dz);
    }
    return 0;
}

/* 8008ABB0: carry the player and RC car around the selected swing pivot. */
int32_t ae_GimmickCarryBodiesSwing(AeRam *r, const AeGimmickAHooks *h,
                                   uint32_t collider, uint32_t pivot, int32_t collider_index_arg)
{
    if ((ae_rd16(r, collider + AE_GA_COLLIDER_GEOMETRY) & 0xF000u) != 0x3000u) return 0;

    int32_t collider_index = ga_s16((uint32_t)collider_index_arg);
    uint32_t player = AE_GA_PLAYER;
    if (ae_rds16(r, player + AE_GA_PLAYER_BODY_COLLIDER) == collider_index) {
        uint32_t position = player + AE_GA_ACTOR_POSITION;
        uint16_t old_x = ae_rd16(r, position + AE_GA_POSITION_X);
        uint16_t old_y = ae_rd16(r, position + AE_GA_POSITION_Y);
        uint16_t old_z = ae_rd16(r, position + AE_GA_POSITION_Z);

        ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_X,
                (uint32_t)(old_x - ae_rd16(r, pivot + AE_GA_VECTOR_X)));
        ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_Y,
                (uint32_t)(old_y - ae_rd16(r, pivot + AE_GA_VECTOR_Y)));
        ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_Z,
                (uint32_t)(old_z - ae_rd16(r, pivot + AE_GA_VECTOR_Z)));
        ae_GimmickSwingCarryPoint(r, collider, pivot, AE_GA_SWING_TEMP_VECTOR, position);
        if (ae_ram_failed(r)) return 0;

        int32_t moved_x = ga_s16(ae_rd16(r, position + AE_GA_POSITION_X) - old_x);
        int32_t moved_y = ga_s16(ae_rd16(r, position + AE_GA_POSITION_Y) - old_y);
        int32_t moved_z = ga_s16(ae_rd16(r, position + AE_GA_POSITION_Z) - old_z);
        GA_CALL(CameraNudge, (uint32_t)moved_x, (uint32_t)moved_y, (uint32_t)moved_z, 0);
        if (ae_ram_failed(r)) return 0;
    }

    uint32_t rc_car = AE_GA_RC_CAR;
    if (ae_rds16(r, rc_car + AE_GA_PLAYER_BODY_COLLIDER) == collider_index) {
        uint32_t position = rc_car + AE_GA_ACTOR_POSITION;
        uint16_t old_x = ae_rd16(r, position + AE_GA_POSITION_X);
        uint16_t old_y = ae_rd16(r, position + AE_GA_POSITION_Y);
        uint16_t old_z = ae_rd16(r, position + AE_GA_POSITION_Z);

        ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_X,
                (uint32_t)(old_x - ae_rd16(r, pivot + AE_GA_VECTOR_X)));
        ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_Y,
                (uint32_t)(old_y - ae_rd16(r, pivot + AE_GA_VECTOR_Y)));
        ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_Z,
                (uint32_t)(old_z - ae_rd16(r, pivot + AE_GA_VECTOR_Z)));
        ae_GimmickSwingCarryPoint(r, collider, pivot, AE_GA_SWING_TEMP_VECTOR, position);
        if (ae_ram_failed(r)) return 0;
    }
    return 0;
}

static int32_t ga_shift12_toward_zero(int32_t value)
{
    return value / 4096;
}

/* 8008B060: seed the rotation matrix from the current signed swing counters. */
int32_t ae_GimmickSwingFirstStep(AeRam *r, uint32_t collider)
{
    int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
    uint32_t props = ae_rd32(r, AE_G_AREA_PROPS);
    uint32_t prop = props + (uint32_t)prop_index * AE_GA_PROP_RECORD_SIZE;
    uint32_t prop_object = ae_rd32(r, prop + AE_GA_PROP_OBJECT_POINTER);
    if ((ae_rd32(r, prop_object + AE_GA_PROP_OBJECT_KIND) & 0xFu) != 3u) return -1;

    if ((ae_rd16(r, collider + AE_GA_COLLIDER_GEOMETRY) & 0xF000u) != 0x3000u) return 0;

    int32_t angle = 0;
    int32_t range = ae_rds16(r, collider + AE_GA_COLLIDER_SWING_RANGE_Y);
    if (range != 0) {
        int32_t step = ae_rds16(r, collider + AE_GA_COLLIDER_SWING_STEP);
        int32_t quotient = (step * 4096) / range;
        angle = range > 0 ? -quotient : quotient;
        ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_X, 0);
        ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_Y, (uint32_t)angle);
        ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_Z, 0);
    } else {
        range = ae_rds16(r, collider + AE_GA_COLLIDER_SWING_RANGE_X);
        if (range != 0) {
            int32_t step = ae_rds16(r, collider + AE_GA_COLLIDER_SWING_STEP);
            int32_t quotient = (step * 4096) / range;
            angle = range > 0 ? -quotient : quotient;
            ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_X, (uint32_t)angle);
            ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_Y, 0);
            ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_Z, 0);
        } else {
            range = ae_rds16(r, collider + AE_GA_COLLIDER_SWING_RANGE_Z);
            if (range == 0) return 0;

            int32_t step = ae_rds16(r, collider + AE_GA_COLLIDER_SWING_STEP);
            int32_t quotient = (step * 4096) / range;
            angle = range < 0 ? -quotient : quotient;
            ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_X, 0);
            ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_Y, 0);
            ae_wr16(r, AE_GA_SWING_TEMP_VECTOR + AE_GA_VECTOR_Z, (uint32_t)angle);
        }
    }

    uint32_t matrix = ae_rd32(r, prop + AE_GA_PROP_MATRIX_POINTER);
    ae_sdk_RotMatrixZYX(r, AE_GA_SWING_TEMP_VECTOR, matrix);
    return ae_ram_failed(r) ? 0 : 1;
}

/* 8008AD14 */
int32_t ae_GimmickSwingCarryPoint(AeRam *r, uint32_t collider, uint32_t position,
                                  uint32_t offset, uint32_t out)
{
    int32_t range = ae_rds16(r, collider + AE_GA_COLLIDER_SWING_RANGE_Y);
    int32_t step = ae_rds16(r, collider + AE_GA_COLLIDER_SWING_STEP);
    if (range != 0) {
        int32_t angle = ae_GimmickSwingAngle(r, range, step);
        int32_t cosine = ae_sdk_rcos(r, angle);
        int32_t sine = ae_sdk_rsin(r, angle);
        if (ae_ram_failed(r)) return 0;

        int32_t offset_x = ae_rds16(r, offset + AE_GA_VECTOR_X);
        int32_t offset_z = ae_rds16(r, offset + AE_GA_VECTOR_Z);
        int32_t rotated_x = offset_x * cosine - offset_z * sine;
        int32_t rotated_z = offset_x * sine + offset_z * cosine;
        int32_t result_x = ae_rds16(r, position + AE_GA_VECTOR_X) +
                           ga_shift12_toward_zero(rotated_x);
        int32_t result_z = ae_rds16(r, position + AE_GA_VECTOR_Z) +
                           ga_shift12_toward_zero(rotated_z);
        ae_wr32(r, out + AE_GA_POSITION_X, (uint32_t)result_x);
        ae_wr32(r, out + AE_GA_POSITION_Z, (uint32_t)result_z);
        return 0;
    }

    range = ae_rds16(r, collider + AE_GA_COLLIDER_SWING_RANGE_X);
    if (range != 0) {
        int32_t angle = ae_GimmickSwingAngle(r, range, step);
        int32_t cosine = ae_sdk_rcos(r, angle);
        int32_t sine = ae_sdk_rsin(r, angle);
        if (ae_ram_failed(r)) return 0;

        int32_t offset_z = ae_rds16(r, offset + AE_GA_VECTOR_Z);
        int32_t offset_y = ae_rds16(r, offset + AE_GA_VECTOR_Y);
        int32_t rotated_z = offset_z * cosine - offset_y * sine;
        int32_t result_z = ae_rds16(r, position + AE_GA_VECTOR_Z) +
                           ga_shift12_toward_zero(rotated_z);
        ae_wr32(r, out + AE_GA_POSITION_Z, (uint32_t)result_z);

        int32_t endpoint_z = ae_rds16(r, position + AE_GA_VECTOR_Z) + offset_z;
        if (result_z == endpoint_z) {
            result_z += offset_z > 0 ? 1 : -1;
            ae_wr32(r, out + AE_GA_POSITION_Z, (uint32_t)result_z);
        }

        int32_t rotated_y = offset_z * sine + offset_y * cosine;
        int32_t result_y = ae_rds16(r, position + AE_GA_VECTOR_Y) +
                           ga_shift12_toward_zero(rotated_y);
        ae_wr32(r, out + AE_GA_POSITION_Y, (uint32_t)result_y);
        return 0;
    }

    range = ae_rds16(r, collider + AE_GA_COLLIDER_SWING_RANGE_Z);
    if (range != 0) {
        int32_t angle = ae_GimmickSwingAngle(r, range, step);
        int32_t cosine = ae_sdk_rcos(r, angle);
        int32_t sine = ae_sdk_rsin(r, angle);
        if (ae_ram_failed(r)) return 0;

        int32_t offset_x = ae_rds16(r, offset + AE_GA_VECTOR_X);
        int32_t offset_y = ae_rds16(r, offset + AE_GA_VECTOR_Y);
        int32_t rotated_x = offset_x * cosine - offset_y * sine;
        int32_t result_x = ae_rds16(r, position + AE_GA_VECTOR_X) +
                           ga_shift12_toward_zero(rotated_x);
        ae_wr32(r, out + AE_GA_POSITION_X, (uint32_t)result_x);

        int32_t endpoint_x = ae_rds16(r, position + AE_GA_VECTOR_X) + offset_x;
        if (result_x == endpoint_x) {
            result_x += offset_x > 0 ? 1 : -1;
            ae_wr32(r, out + AE_GA_POSITION_X, (uint32_t)result_x);
        }

        int32_t rotated_y = offset_x * sine + offset_y * cosine;
        int32_t result_y = ae_rds16(r, position + AE_GA_VECTOR_Y) +
                           ga_shift12_toward_zero(rotated_y);
        ae_wr32(r, out + AE_GA_POSITION_Y, (uint32_t)result_y);
    }
    return 0;
}

/* 8008AF98: both the progress counter and range are signed halfwords at the call site. */
int32_t ae_GimmickSwingAngle(AeRam *r, int32_t range_arg, int32_t step_arg)
{
    int32_t range = ga_s16((uint32_t)range_arg);
    int32_t step = ga_s16((uint32_t)step_arg);
    if (range == 0) {
        ae_ram_fail(r, "GimmickSwingAngle: original division by zero");
        return 0;
    }

    int32_t scaled_step = step * 4096;
    int32_t first_quotient = scaled_step / range;
    int32_t adjusted_step;

    if (range > 0) {
        adjusted_step = step - 1;
        if (adjusted_step < 0) adjusted_step += range;
    } else {
        adjusted_step = step + 1;
        if (range < adjusted_step) adjusted_step -= range;
    }

    int32_t second_quotient = (adjusted_step * 4096) / range;
    if (range < 0) {
        first_quotient = -first_quotient;
        second_quotient = -second_quotient;
    }
    return (int32_t)((uint32_t)first_quotient - (uint32_t)second_quotient);
}

/* 8008B228: advance the three possible swing axes in Y, X, Z order. */
int32_t ae_GimmickSwingAdvance(AeRam *r, uint32_t collider)
{
    if ((ae_rd16(r, collider + AE_GA_COLLIDER_GEOMETRY) & 0xF000u) != 0x3000u) return 0;

    int32_t range = ae_rds16(r, collider + AE_GA_COLLIDER_SWING_RANGE_Y);
    if (range == 0) range = ae_rds16(r, collider + AE_GA_COLLIDER_SWING_RANGE_X);
    if (range == 0) range = ae_rds16(r, collider + AE_GA_COLLIDER_SWING_RANGE_Z);
    if (range == 0) return 0;

    int32_t step = ga_s16((uint32_t)(ae_rd16(r, collider + AE_GA_COLLIDER_SWING_STEP) +
                                     (range > 0 ? 1u : (uint32_t)-1)));
    ae_wr16(r, collider + AE_GA_COLLIDER_SWING_STEP, (uint32_t)step);
    if (range > 0 && range < step) {
        step = ga_s16((uint32_t)(step - range));
        ae_wr16(r, collider + AE_GA_COLLIDER_SWING_STEP, (uint32_t)step);
    } else if (range < 0 && step < range) {
        step = ga_s16((uint32_t)(step - range));
        ae_wr16(r, collider + AE_GA_COLLIDER_SWING_STEP, (uint32_t)step);
    }
    return 0;
}

static int32_t ga_swing_probe_ground(AeRam *r, const AeGimmickAHooks *h,
                                     int32_t x, int32_t y, int32_t z)
{
    ae_wr16(r, AE_GA_WALL_PROBE_FLAG, 1);
    int32_t ground = ga_ground_height(r, h, x, y, z);
    return ga_s16((uint32_t)ground);
}

static int32_t ga_swing_probe_adjust_surface(AeRam *r, int32_t ground)
{
    if (ae_rd16(r, AE_GA_GROUND_QUERY_SCRATCH + AE_GA_GROUND_QUERY_SURFACE) == 1u &&
        ae_rds16(r, AE_GA_GROUND_QUERY_SCRATCH + AE_GA_GROUND_QUERY_SENTINEL) < 0) {
        return INT16_MAX;
    }
    return ground;
}

/* 8008B3A4: sample the four swept corners and center, then pick the lowest height. */
int32_t ae_GimmickSwingProbeWall(AeRam *r, const AeGimmickAHooks *h,
                                 uint32_t collider, int32_t dx_arg, int32_t dz_arg,
                                 uint32_t out)
{
    int32_t dx = ga_s16((uint32_t)dx_arg);
    int32_t dz = ga_s16((uint32_t)dz_arg);
    int32_t half_x = ga_s16(ae_rd16(r, collider + AE_COLLIDER_HALF_X) - 2u);
    int32_t half_z = ga_s16(ae_rd16(r, collider + AE_COLLIDER_HALF_Z) - 2u);
    int32_t origin_x = ae_rds16(r, collider + AE_COLLIDER_POS_X);
    int32_t origin_y = ae_rds16(r, collider + AE_COLLIDER_POS_Y);
    int32_t origin_z = ae_rds16(r, collider + AE_COLLIDER_POS_Z);
    int32_t wall_y = origin_y;
    int32_t probe_x[5] = {
        origin_x + dx - half_x,
        origin_x + dx + half_x,
        origin_x + dx - half_x,
        origin_x + dx + half_x,
        origin_x + dx
    };
    int32_t probe_z[5] = {
        origin_z + dz + half_z,
        origin_z + dz + half_z,
        origin_z + dz - half_z,
        origin_z + dz - half_z,
        origin_z + dz
    };

    for (uint32_t i = 0; i < 5; ++i) {
        int32_t ground = ga_swing_probe_ground(r, h, probe_x[i], wall_y, probe_z[i]);
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, AE_GA_PROBE_HEIGHTS + i * 2u, (uint32_t)ground);
        if (i < 4 && ground < wall_y) {
            ae_wr16(r, out, (uint32_t)wall_y);
            return 1;
        }
        if (i == 4 && ae_rds16(r, AE_GA_PROBE_HEIGHTS + AE_GA_PROBE_HEIGHT_FOURTH) < wall_y) {
            ae_wr16(r, out, (uint32_t)wall_y);
            return 1;
        }
        ground = ga_swing_probe_adjust_surface(r, ground);
        ae_wr16(r, AE_GA_PROBE_HEIGHTS + i * 2u, (uint32_t)ground);
    }

    int32_t lowest = ae_rds16(r, AE_GA_PROBE_HEIGHTS);
    for (uint32_t i = 1; i < 5; ++i) {
        int32_t height = ae_rds16(r, AE_GA_PROBE_HEIGHTS + i * 2u);
        if (height < lowest) lowest = height;
    }
    ae_wr16(r, out, (uint32_t)lowest);
    return 0;
}

/* 800909A8 */
int32_t ae_Gimmick_Init_Counter(AeRam *r, uint32_t gimmick)
{
    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    uint32_t collider = ga_collider(colliders, collider_index);

    ae_wr16(r, gimmick + AE_GIMMICK_PARAM_12, 0xFFFFu);
    ae_wr16(r, gimmick + AE_GIMMICK_PARAM_14, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_PARAM_16, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_TIMER,
            (uint32_t)(ae_rds16(r, collider + AE_COLLIDER_HALF_Y) * 2));
    return 0;
}

static int32_t ga_activate(AeRam *r, const AeGimmickAHooks *h,
                           int32_t target_index, uint32_t source)
{
    AeGimmickHooks hooks = {0};
    if (h) {
        hooks.user = h->user;
        hooks.PlaySfx = h->PlaySfx;
        hooks.PlaySfxAt = h->PlaySfxAt;
        hooks.PropSetMesh = h->PropSetMesh;
    }
    return ae_GimmickActivate(r, &hooks, target_index, source);
}

static uint32_t ga_area_prop(uint32_t props, int32_t index)
{
    return props + (uint32_t)index * AE_GA_PROP_RECORD_SIZE;
}

static int32_t ga_gimmick_prop_index(AeRam *r, uint32_t collider)
{
    return ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
}

static int32_t ga_half_s16(int32_t value)
{
    return value < 0 ? (value + 1) / 2 : value / 2;
}

static void ga_copy_collider_origin(AeRam *r, uint32_t collider)
{
    ae_wr16(r, collider + AE_GA_COLLIDER_ORIGIN_X,
            ae_rd16(r, collider + AE_COLLIDER_POS_X));
    ae_wr16(r, collider + AE_GA_COLLIDER_ORIGIN_Y,
            ae_rd16(r, collider + AE_COLLIDER_POS_Y));
    ae_wr16(r, collider + AE_GA_COLLIDER_ORIGIN_Z,
            ae_rd16(r, collider + AE_COLLIDER_POS_Z));
}

/* 80090500 */
int32_t ae_Gimmick_Switch_Update(AeRam *r, const AeGimmickAHooks *h, uint32_t gimmick)
{
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    uint32_t collider = ga_collider(colliders, collider_index);

    if (ae_rds16(r, gimmick + AE_GIMMICK_STATE) != 0 ||
        ae_rds16(r, collider + AE_GA_COLLIDER_TOUCHED_BY) < 0) {
        return 0;
    }

    ga_activate(r, h, ae_rds16(r, gimmick + AE_GIMMICK_LINK), gimmick);
    if (ae_ram_failed(r)) return 0;
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);
    ae_wr16(r, collider + AE_COLLIDER_FLAGS,
            ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
    GA_CALL(PropSetMesh, (uint32_t)ga_gimmick_prop_index(r, collider),
            ae_rd16(r, gimmick + AE_GIMMICK_PARAM_10),
            ae_rd16(r, gimmick + AE_GIMMICK_PARAM_10), 0);
    return 0;
}

/* 800905A4 */
int32_t ae_Gimmick_Init_PairToggle(AeRam *r, const AeGimmickAHooks *h,
                                   uint32_t gimmick)
{
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    uint32_t collider = ga_collider(colliders, collider_index);
    int32_t mesh_a = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_E);
    int32_t mesh_b = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_10);

    if (ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A) == 0) {
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
        ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
        GA_CALL(PropSetMesh, (uint32_t)ga_gimmick_prop_index(r, collider),
                (uint32_t)mesh_a, (uint32_t)mesh_a, (uint32_t)mesh_b);
    } else {
        ga_activate(r, h, ae_rds16(r, gimmick + AE_GIMMICK_LINK), gimmick);
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);
        ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
        GA_CALL(PropSetMesh, (uint32_t)ga_gimmick_prop_index(r, collider),
                (uint32_t)mesh_b, (uint32_t)mesh_a, (uint32_t)mesh_b);
    }
    return 0;
}

/* 80090660 */
int32_t ae_Gimmick_Hit_Update(AeRam *r, const AeGimmickAHooks *h, uint32_t gimmick)
{
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    uint32_t collider = ga_collider(colliders, collider_index);
    int32_t state = ae_rds16(r, gimmick + AE_GIMMICK_STATE);
    if (state != 0 && state != 1) return 0;

    int32_t touched = GA_CALL(GimmickTouchCheck, 1, collider, 0, 0);
    if (ae_ram_failed(r)) return 0;
    if ((state == 0 && touched == 0) || (state == 1 && touched != 0)) return 0;

    ga_activate(r, h, ae_rds16(r, gimmick + AE_GIMMICK_LINK), gimmick);
    if (ae_ram_failed(r)) return 0;
    if (state == 0) {
        GA_CALL(PlaySfxAt, 0x75, collider, 0, 0);
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);
        GA_CALL(PropSetMesh, (uint32_t)ga_gimmick_prop_index(r, collider),
                ae_rd16(r, gimmick + AE_GIMMICK_PARAM_10),
                ae_rd16(r, gimmick + AE_GIMMICK_PARAM_E),
                ae_rd16(r, gimmick + AE_GIMMICK_PARAM_10));
    } else {
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
        GA_CALL(PropSetMesh, (uint32_t)ga_gimmick_prop_index(r, collider),
                ae_rd16(r, gimmick + AE_GIMMICK_PARAM_E),
                ae_rd16(r, gimmick + AE_GIMMICK_PARAM_E),
                ae_rd16(r, gimmick + AE_GIMMICK_PARAM_10));
    }
    return 0;
}

/* 8008E90C */
int32_t ae_Gimmick_SwingDoor_Update(AeRam *r, const AeGimmickAHooks *h,
                                    uint32_t gimmick)
{
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    uint32_t collider = ga_collider(colliders, collider_index);
    int32_t state = ae_rds16(r, gimmick + AE_GIMMICK_STATE);

    if (state == 0 || state == 3) {
        int32_t inside = ae_GimmickAnyBodyInside(
            r, h, collider, 0, gimmick + AE_GIMMICK_PARAM_E);
        if (ae_ram_failed(r)) return 0;
        bool start_close = state == 0 && inside == 0;
        bool start_open = state == 3 && inside != 0;
        if (start_close || start_open) {
            ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                    ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, start_close ? 2u : 5u);
        }
        return 0;
    }

    if (state == 5 || state == 2) {
        GA_CALL(SwingDoorBodyDelta, gimmick,
                ae_rd16(r, collider + AE_COLLIDER_PROP_INDEX), 0, 0);
        if (ae_ram_failed(r)) return 0;

        int32_t timer = ae_rds16(r, gimmick + AE_GIMMICK_TIMER);
        if (state == 5) {
            timer = ga_s16((uint32_t)(timer + 32));
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, (uint32_t)timer);
            if (timer == 192) ae_GimmickPlayTypeSfx(r, h, collider);
            if (timer < 1025) return 0;

            ae_SwingDoorSetPivot(r, collider,
                                 ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE), 2);
            ae_wr16(r, gimmick + AE_GIMMICK_STATE,
                    ae_rds16(r, gimmick + AE_GIMMICK_TYPE) == 1 ? 0u : 1u);
        } else {
            timer = ga_s16((uint32_t)(timer - 32));
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, (uint32_t)timer);
            if (timer == 832) ae_GimmickPlayTypeSfx(r, h, collider);
            if (timer >= 0) return 0;

            ae_SwingDoorSetPivot(r, collider,
                                 ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE), 5);
            ae_wr16(r, gimmick + AE_GIMMICK_STATE,
                    ae_rds16(r, gimmick + AE_GIMMICK_TYPE) == 1 ? 3u : 4u);
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
            ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                    ae_rd16(r, collider + AE_COLLIDER_FLAGS) & 0x7FFFu);
        }
    }
    return 0;
}

/* 8008F0BC */
int32_t ae_Gimmick_LiftDoor_Update(AeRam *r, const AeGimmickAHooks *h,
                                   uint32_t gimmick)
{
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    uint32_t collider = ga_collider(colliders, collider_index);
    int32_t state = ae_rds16(r, gimmick + AE_GIMMICK_STATE);

    if (state == 0 || state == 3) {
        int32_t inside = ae_GimmickAnyBodyInside(
            r, h, collider, 1, gimmick + AE_GIMMICK_PARAM_E);
        if (ae_ram_failed(r)) return 0;
        if ((state == 0 && inside == 0) || (state == 3 && inside != 0)) {
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, state == 0 ? 2u : 5u);
            if (state == 0) {
                int32_t prop_index = ga_gimmick_prop_index(r, collider);
                if (prop_index >= 0) {
                    uint32_t prop = ga_area_prop(ae_rd32(r, AE_G_AREA_PROPS), prop_index);
                    ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0xFFFFu);
                }
            }
        }
        return 0;
    }

    int32_t timer = ae_rds16(r, gimmick + AE_GIMMICK_TIMER);
    int32_t half_y = ae_rds16(r, collider + AE_COLLIDER_HALF_Y);
    int32_t prop_index = ga_gimmick_prop_index(r, collider);
    uint32_t prop = prop_index >= 0 ?
        ga_area_prop(ae_rd32(r, AE_G_AREA_PROPS), prop_index) : 0;

    if (state == 5) {
        timer = ga_s16((uint32_t)(timer + 32));
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, (uint32_t)timer);
        if (timer == 128) ae_GimmickPlayTypeSfx(r, h, collider);
        if (timer > half_y) {
            timer = half_y;
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, (uint32_t)timer);
            ae_wr16(r, gimmick + AE_GIMMICK_STATE,
                    ae_rds16(r, gimmick + AE_GIMMICK_TYPE) == 1 ? 0u : 1u);
            if (prop_index >= 0) ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0);
        } else {
            ae_wr16(r, collider + AE_COLLIDER_POS_Y,
                    ae_rd16(r, collider + AE_COLLIDER_POS_Y) - 32u);
            if (prop_index >= 0) {
                ae_wr16(r, prop + AE_GA_PROP_POSITION_Y,
                        ae_rd16(r, prop + AE_GA_PROP_POSITION_Y) - 32u);
            }
        }
    } else if (state == 2) {
        timer = ga_s16((uint32_t)(timer - 32));
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, (uint32_t)timer);
        int32_t sound_boundary = ((half_y < 0 ? half_y + 31 : half_y) >> 5) - 4;
        if (timer == sound_boundary * 32) ae_GimmickPlayTypeSfx(r, h, collider);
        if (timer < 0) {
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
            ae_wr16(r, gimmick + AE_GIMMICK_STATE,
                    ae_rds16(r, gimmick + AE_GIMMICK_TYPE) == 1 ? 3u : 4u);
        } else {
            ae_wr16(r, collider + AE_COLLIDER_POS_Y,
                    ae_rd16(r, collider + AE_COLLIDER_POS_Y) + 32u);
            if (prop_index >= 0) {
                ae_wr16(r, prop + AE_GA_PROP_POSITION_Y,
                        ae_rd16(r, prop + AE_GA_PROP_POSITION_Y) + 32u);
            }
        }
    }
    return 0;
}

/* 8008F8D4 */
int32_t ae_Gimmick_SlideDoor_Update(AeRam *r, const AeGimmickAHooks *h,
                                    uint32_t gimmick)
{
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    uint32_t collider = ga_collider(colliders, collider_index);
    int32_t state = ae_rds16(r, gimmick + AE_GIMMICK_STATE);

    if (state == 0 || state == 3) {
        int32_t inside = ae_GimmickAnyBodyInside(
            r, h, collider, 2, gimmick + AE_GIMMICK_PARAM_E);
        if (ae_ram_failed(r)) return 0;
        if ((state == 0 && inside == 0) || (state == 3 && inside != 0)) {
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, state == 0 ? 2u : 5u);
            if (state == 0) {
                int32_t prop_index = ga_gimmick_prop_index(r, collider);
                if (prop_index >= 0) {
                    uint32_t prop = ga_area_prop(ae_rd32(r, AE_G_AREA_PROPS), prop_index);
                    ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0xFFFFu);
                }
            }
        }
        return 0;
    }

    int32_t timer = ae_rds16(r, gimmick + AE_GIMMICK_TIMER);
    int32_t prop_index = ga_gimmick_prop_index(r, collider);
    uint32_t prop = prop_index >= 0 ?
        ga_area_prop(ae_rd32(r, AE_G_AREA_PROPS), prop_index) : 0;

    if (state == 5) {
        timer = ga_s16((uint32_t)(timer + 32));
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, (uint32_t)timer);
        if (timer == 64) ae_GimmickPlayTypeSfx(r, h, collider);
        int32_t distance = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12);
        bool reached_end = timer > distance;
        if (reached_end) {
            timer = distance;
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, (uint32_t)timer);
            ae_wr16(r, gimmick + AE_GIMMICK_STATE,
                    ae_rds16(r, gimmick + AE_GIMMICK_TYPE) == 1 ? 0u : 1u);
            if (prop_index >= 0) ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0);
        }
        if (!reached_end) {
            GA_CALL(SlideDoorMove, collider,
                    ae_rd16(r, gimmick + AE_GIMMICK_SUBTYPE), 32, 0);
        }
    } else if (state == 2) {
        timer = ga_s16((uint32_t)(timer - 32));
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, (uint32_t)timer);
        int32_t distance = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12);
        int32_t boundary = ((distance < 0 ? distance + 31 : distance) >> 5) - 2;
        if (timer == boundary * 32) ae_GimmickPlayTypeSfx(r, h, collider);
        if (timer < 0) {
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
            ae_wr16(r, gimmick + AE_GIMMICK_STATE,
                    ae_rds16(r, gimmick + AE_GIMMICK_TYPE) == 1 ? 3u : 4u);
        } else {
            GA_CALL(SlideDoorMove, collider,
                    ae_rd16(r, gimmick + AE_GIMMICK_SUBTYPE), (uint32_t)-32, 0);
        }
    }
    return 0;
}

/* 8008F53C */
int32_t ae_Gimmick_ThresholdDoor_Update(AeRam *r, const AeGimmickAHooks *h,
                                        uint32_t gimmick)
{
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    uint32_t collider = ga_collider(colliders, collider_index);
    int32_t threshold = 0;
    const int32_t linked[] = {
        ae_rds16(r, gimmick + AE_GIMMICK_PARAM_E),
        ae_rds16(r, gimmick + AE_GIMMICK_PARAM_10)
    };
    uint32_t gimmicks = ae_rd32(r, AE_G_GIMMICKS);
    for (uint32_t i = 0; i < 2; ++i) {
        if (linked[i] >= 0) {
            uint32_t record = ga_gimmick(gimmicks, linked[i]);
            threshold += ae_rds16(r, record + AE_GIMMICK_PARAM_16);
        }
    }

    int32_t prop_index = ga_gimmick_prop_index(r, collider);
    uint32_t prop = prop_index >= 0 ?
        ga_area_prop(ae_rd32(r, AE_G_AREA_PROPS), prop_index) : 0;
    int32_t below_threshold = ae_rds16(r, collider + AE_COLLIDER_HALF_Y) < threshold;
    int32_t original_state = ae_rds16(r, gimmick + AE_GIMMICK_STATE);
    if (below_threshold) {
        if (prop_index >= 0) ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0);
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
    } else if (threshold > 0) {
        if (prop_index >= 0) ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0xFFFFu);
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 5);
    } else {
        if (original_state == 5) {
            int32_t sound = ae_GimmickTypeToSfx(
                r, ae_rds16(r, collider + AE_GA_COLLIDER_SFX_TYPE));
            if (sound >= 0) GA_CALL(PlaySfxAt, (uint32_t)sound, collider, 0, 0);
        }
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 3);
    }

    if (threshold > 0) {
        int32_t loop = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12);
        if (loop < 0) {
            loop = GA_CALL(SoundLoopStart, 1, 127, 0, 0);
            if (ae_ram_failed(r)) return 0;
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_12, (uint32_t)loop);
        } else {
            GA_CALL(SoundEmitterPan, 127, (uint32_t)loop, 6144,
                    collider + AE_GA_COLLIDER_ORIGIN_X);
        }
    } else {
        int32_t loop = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12);
        if (loop >= 0) {
            GA_CALL(SoundLoopStop, 127, (uint32_t)loop, 0, 0);
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_12, 0xFFFFu);
        }
    }

    if (prop_index >= 0) {
        ae_wr16(r, prop + AE_GA_PROP_POSITION_Y,
                ae_rd16(r, collider + AE_GA_COLLIDER_ORIGIN_Y) - (uint32_t)threshold);
    }
    ae_wr16(r, collider + AE_COLLIDER_POS_Y,
            ae_rd16(r, collider + AE_GA_COLLIDER_ORIGIN_Y) - (uint32_t)threshold);
    return 0;
}

/* 8008E5C0 */
int32_t ae_Gimmick_SwingDoor_Init(AeRam *r, const AeGimmickAHooks *h,
                                  uint32_t gimmick)
{
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    uint32_t collider = ga_collider(colliders, collider_index);
    int32_t half_x = ga_half_s16(ae_rds16(r, collider + AE_COLLIDER_HALF_X));
    int32_t half_z = ga_half_s16(ae_rds16(r, collider + AE_COLLIDER_HALF_Z));
    ae_wr16(r, collider + AE_COLLIDER_HALF_X, (uint32_t)half_x);
    ae_wr16(r, collider + AE_COLLIDER_HALF_Z, (uint32_t)half_z);

    uint16_t direction = ae_rd16(r, gimmick + AE_GIMMICK_SUBTYPE);
    int32_t x = ae_rds16(r, collider + AE_COLLIDER_POS_X);
    int32_t z = ae_rds16(r, collider + AE_COLLIDER_POS_Z);
    uint16_t new_x = (uint16_t)x;
    uint16_t new_z = (uint16_t)z;
    uint16_t origin_x = (uint16_t)x;
    uint16_t origin_z = (uint16_t)z;
    uint16_t travel_x = 0;
    uint16_t travel_z = 0;

    switch (direction) {
    case 0:
        new_x = (uint16_t)(x - half_x); new_z = (uint16_t)(z + half_z);
        origin_x = (uint16_t)(x - 2 * half_x); origin_z = new_z;
        travel_x = (uint16_t)(4 * half_x);
        travel_z = (uint16_t)(half_z + 0x600);
        break;
    case 1:
        new_x = (uint16_t)(x + half_x); new_z = (uint16_t)(z + half_z);
        origin_x = (uint16_t)(x + 2 * half_x); origin_z = new_z;
        travel_x = (uint16_t)(4 * half_x);
        travel_z = (uint16_t)(half_z + 0x600);
        break;
    case 2:
        new_x = (uint16_t)(x + half_x); new_z = (uint16_t)(z + half_z);
        origin_x = new_x; origin_z = (uint16_t)(z + 2 * half_z);
        travel_x = (uint16_t)(half_x + 0x600);
        travel_z = (uint16_t)(4 * half_z);
        break;
    case 3:
        new_x = (uint16_t)(x + half_x); new_z = (uint16_t)(z - half_z);
        origin_x = new_x; origin_z = (uint16_t)(z - 2 * half_z);
        travel_x = (uint16_t)(half_x + 0x600);
        travel_z = (uint16_t)(4 * half_z);
        break;
    case 4:
        new_x = (uint16_t)(x + half_x); new_z = (uint16_t)(z - half_z);
        origin_x = (uint16_t)(x + 2 * half_x); origin_z = new_z;
        travel_x = (uint16_t)(4 * half_x);
        travel_z = (uint16_t)(half_z + 0x600);
        break;
    case 5:
        new_x = (uint16_t)(x - half_x); new_z = (uint16_t)(z - half_z);
        origin_x = (uint16_t)(x - 2 * half_x); origin_z = new_z;
        travel_x = (uint16_t)(4 * half_x);
        travel_z = (uint16_t)(half_z + 0x600);
        break;
    case 6:
        new_x = (uint16_t)(x - half_x); new_z = (uint16_t)(z - half_z);
        origin_x = new_x; origin_z = (uint16_t)(z - 2 * half_z);
        travel_x = (uint16_t)(half_x + 0x600);
        travel_z = (uint16_t)(4 * half_z);
        break;
    case 7:
        new_x = (uint16_t)(x - half_x); new_z = (uint16_t)(z + half_z);
        origin_x = new_x; origin_z = (uint16_t)(z + 2 * half_z);
        travel_x = (uint16_t)(half_x + 0x600);
        travel_z = (uint16_t)(4 * half_z);
        break;
    default:
        break;
    }

    ae_wr16(r, collider + AE_COLLIDER_POS_X, new_x);
    ae_wr16(r, collider + AE_COLLIDER_POS_Z, new_z);
    ae_wr16(r, collider + AE_GA_COLLIDER_ORIGIN_X, origin_x);
    ae_wr16(r, collider + AE_GA_COLLIDER_ORIGIN_Z, origin_z);
    if (direction < 8) {
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_E, travel_x);
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_10, travel_z);
    }
    ae_wr16(r, collider + AE_GA_COLLIDER_ORIGIN_Y,
            ae_rd16(r, collider + AE_COLLIDER_POS_Y));

    if (ae_rds16(r, gimmick + AE_GIMMICK_TYPE) == 1) {
        if (ae_rds16(r, gimmick + AE_GIMMICK_LINK) == 0) {
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 3);
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
            return 0;
        }
        ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0x400);
    } else if (ae_rds16(r, gimmick + AE_GIMMICK_LINK) == 0) {
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 4);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
        return 0;
    } else {
        ae_SwingDoorSetPivot(r, collider,
                             ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE), 2);
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0x400);
    }

    GA_CALL(SwingDoorBodyDelta, gimmick,
            ae_rd16(r, collider + AE_COLLIDER_PROP_INDEX), 0, 0);
    return 0;
}

/* 8008EF60 */
int32_t ae_Gimmick_LiftDoor_Init(AeRam *r, uint32_t gimmick)
{
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    uint32_t collider = ga_collider(colliders, collider_index);
    ga_copy_collider_origin(r, collider);

    int32_t type = ae_rds16(r, gimmick + AE_GIMMICK_TYPE);
    int32_t subtype = ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE);
    int32_t linked = ae_rds16(r, gimmick + AE_GIMMICK_LINK);
    if (type == 1) {
        if (subtype == 10 || subtype == 12) {
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_E,
                    ae_rd16(r, collider + AE_COLLIDER_HALF_X) * 2u);
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_10,
                    ae_rd16(r, collider + AE_COLLIDER_HALF_Z) + 0x400u);
        } else {
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_E,
                    ae_rd16(r, collider + AE_COLLIDER_HALF_X) + 0x400u);
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_10,
                    ae_rd16(r, collider + AE_COLLIDER_HALF_Z) * 2u);
        }

        if (linked != 0) {
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER,
                    ae_rd16(r, collider + AE_COLLIDER_HALF_Y));
        } else {
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 3);
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
        }
    } else if (linked != 0) {
        int32_t height = ae_rds16(r, collider + AE_COLLIDER_HALF_Y);
        int32_t prop_index = ga_gimmick_prop_index(r, collider);
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, (uint32_t)height);
        ae_wr16(r, collider + AE_COLLIDER_POS_Y,
                ae_rd16(r, collider + AE_COLLIDER_POS_Y) - (uint32_t)height);
        if (prop_index >= 0) {
            uint32_t prop = ga_area_prop(ae_rd32(r, AE_G_AREA_PROPS), prop_index);
            ae_wr16(r, prop + AE_GA_PROP_POSITION_Y,
                    ae_rd16(r, prop + AE_GA_PROP_POSITION_Y) - (uint32_t)height);
            ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0);
        }
    } else {
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 4);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    }
    return 0;
}

/* 8008F468 */
int32_t ae_Gimmick_Init_ThresholdDoor(AeRam *r, uint32_t gimmick)
{
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    uint32_t collider = ga_collider(colliders, collider_index);
    ga_copy_collider_origin(r, collider);

    int32_t height = ae_rds16(r, collider + AE_COLLIDER_HALF_Y);
    const uint32_t link_offsets[] = {AE_GIMMICK_PARAM_E, AE_GIMMICK_PARAM_10};
    for (uint32_t i = 0; i < 2; ++i) {
        int32_t index = ae_rds16(r, gimmick + link_offsets[i]);
        if (index >= 0) {
            uint32_t record = ga_gimmick(ae_rd32(r, AE_G_GIMMICKS), index);
            ae_wr16(r, record + AE_GIMMICK_TIMER, (uint32_t)height);
        }
    }
    if (ae_rds16(r, gimmick + AE_GIMMICK_LINK) == 0) {
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 3);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    } else {
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, (uint32_t)height);
    }
    ae_wr16(r, gimmick + AE_GIMMICK_PARAM_12, 0xFFFFu);
    return 0;
}

/* 8008F720 */
int32_t ae_Gimmick_SlideDoor_Init(AeRam *r, const AeGimmickAHooks *h,
                                  uint32_t gimmick)
{
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    uint32_t collider = ga_collider(colliders, collider_index);
    ga_copy_collider_origin(r, collider);

    int32_t direction = ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE);
    int32_t half_x = ae_rds16(r, collider + AE_COLLIDER_HALF_X);
    int32_t half_z = ae_rds16(r, collider + AE_COLLIDER_HALF_Z);
    switch (direction) {
    case 0: case 5:
        ae_wr16(r, collider + AE_GA_COLLIDER_ORIGIN_X,
                (uint32_t)(ae_rds16(r, collider + AE_GA_COLLIDER_ORIGIN_X) - half_x));
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_E, (uint32_t)(half_x * 2));
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_10, (uint32_t)(half_z + 0x400));
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_12, (uint32_t)(half_x * 2));
        break;
    case 1: case 4:
        ae_wr16(r, collider + AE_GA_COLLIDER_ORIGIN_X,
                (uint32_t)(ae_rds16(r, collider + AE_GA_COLLIDER_ORIGIN_X) + half_x));
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_E, (uint32_t)(half_x * 2));
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_10, (uint32_t)(half_z + 0x400));
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_12, (uint32_t)(half_x * 2));
        break;
    case 2: case 7:
        ae_wr16(r, collider + AE_GA_COLLIDER_ORIGIN_Z,
                (uint32_t)(ae_rds16(r, collider + AE_GA_COLLIDER_ORIGIN_Z) + half_z));
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_E, (uint32_t)(half_x + 0x400));
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_10, (uint32_t)(half_z * 2));
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_12, (uint32_t)(half_z * 2));
        break;
    case 3: case 6:
        ae_wr16(r, collider + AE_GA_COLLIDER_ORIGIN_Z,
                (uint32_t)(ae_rds16(r, collider + AE_GA_COLLIDER_ORIGIN_Z) - half_z));
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_E, (uint32_t)(half_x + 0x400));
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_10, (uint32_t)(half_z * 2));
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_12, (uint32_t)(half_z * 2));
        break;
    default:
        break;
    }

    if (ae_rds16(r, gimmick + AE_GIMMICK_TYPE) == 1) {
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 3);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    } else if (ae_rds16(r, gimmick + AE_GIMMICK_LINK) == 0) {
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 4);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    } else {
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER,
                ae_rd16(r, gimmick + AE_GIMMICK_PARAM_12));
        int32_t prop_index = ga_gimmick_prop_index(r, collider);
        if (prop_index >= 0) {
            uint32_t prop = ga_area_prop(ae_rd32(r, AE_G_AREA_PROPS), prop_index);
            ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0);
        }
        GA_CALL(SlideDoorMove, collider,
                ae_rd16(r, gimmick + AE_GIMMICK_SUBTYPE),
                ae_rd16(r, gimmick + AE_GIMMICK_PARAM_12), 0);
    }
    return 0;
}

/* 80090370 */
int32_t ae_Gimmick_Switch_Init(AeRam *r, const AeGimmickAHooks *h,
                               uint32_t gimmick)
{
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    uint32_t collider = ga_collider(colliders, collider_index);
    int32_t subtype = ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE);
    int32_t mode = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A);
    int32_t mesh = 0;

    if (subtype >= 0x2A && subtype <= 0x2D) {
        mesh = ae_rds16(r, gimmick +
                        (mode == 0 ? AE_GIMMICK_PARAM_E : AE_GIMMICK_PARAM_10));
        if (mode == 0) {
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
        } else {
            ga_activate(r, h, ae_rds16(r, gimmick + AE_GIMMICK_LINK), gimmick);
            if (ae_ram_failed(r)) return 0;
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);
            ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                    ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
        }
        GA_CALL(PropSetMesh, (uint32_t)ga_gimmick_prop_index(r, collider),
                (uint32_t)mesh, ae_rd16(r, gimmick + AE_GIMMICK_PARAM_E),
                ae_rd16(r, gimmick + AE_GIMMICK_PARAM_10));
    } else if (subtype >= 0x2E && subtype <= 0x31) {
        if (mode == 0) {
            mesh = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_E);
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
        } else if (mode == 1) {
            mesh = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_10);
            ga_activate(r, h, ae_rds16(r, gimmick + AE_GIMMICK_LINK), gimmick);
            if (ae_ram_failed(r)) return 0;
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);
            ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                    ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
        } else {
            mesh = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12);
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, (uint32_t)mode);
        }

        int32_t prop_index = ga_gimmick_prop_index(r, collider);
        int32_t mesh_a = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_E);
        int32_t mesh_b = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_10);
        int32_t mesh_c = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12);
        if (mesh_a >= 0 && mesh_b >= 0 && mesh_c >= 0 && prop_index >= 0) {
            uint32_t prop = ga_area_prop(ae_rd32(r, AE_G_AREA_PROPS), prop_index);
            GA_CALL(MeshInitFromAreaTable, prop, (uint32_t)mesh,
                    (uint32_t)mesh_a, (uint32_t)mesh_b);
        }
    }
    return 0;
}

static int32_t ga_wrap_spinner_angle(int32_t angle, bool negative_direction)
{
    if (negative_direction) {
        while (angle < -0x400) angle += 0x400;
        while (angle > 0) angle -= 0x400;
    } else {
        while (angle > 0x400) angle -= 0x400;
        while (angle < 0) angle += 0x400;
    }
    return angle;
}

/* 800909FC */
int32_t ae_Gimmick_SpinRoller_Update(AeRam *r, uint32_t gimmick)
{
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    uint32_t collider = ga_collider(ae_rd32(r, AE_G_COLLIDERS), collider_index);
    int32_t speed = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_14);
    int32_t angle = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_16);

    if (ae_rds16(r, collider + AE_GA_COLLIDER_TOUCHED_BY) >= 0) {
        speed = ga_s16((uint32_t)(speed + 0x60));
        if (speed >= 0xB5) speed = 0xB4;
    } else if (speed > 0) {
        speed = ga_s16((uint32_t)(speed - ((speed / 100) * 8 + 8)));
    } else if (angle > 0) {
        speed = -16;
    } else {
        speed = 0;
        angle = 0;
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_16, 0);
    }

    ae_wr16(r, gimmick + AE_GIMMICK_PARAM_14, (uint32_t)speed);
    angle = ga_s16((uint32_t)(angle + speed));
    int32_t limit = ae_rds16(r, gimmick + AE_GIMMICK_TIMER);
    if (limit < angle) angle = limit;
    ae_wr16(r, gimmick + AE_GIMMICK_PARAM_16, (uint32_t)angle);

    int32_t subtype = ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE);
    for (int32_t i = 0; i < 4; ++i) {
        int32_t prop_index = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A +
                                      (uint32_t)i * 2u);
        if (prop_index < 0) continue;

        int32_t phase = (i << 8) + 0x80;
        int32_t rotation = 0;
        if (subtype == 0x37 || subtype == 0x3A) {
            rotation = ga_wrap_spinner_angle(angle + phase, false);
        } else if (subtype == 0x38 || subtype == 0x39) {
            rotation = ga_wrap_spinner_angle(-phase - angle, true);
        }

        ae_wr16(r, AE_GA_SPIN_TEMP_VECTOR, 0);
        ae_wr16(r, AE_GA_SPIN_TEMP_VECTOR + AE_GA_VECTOR_Y, 0);
        ae_wr16(r, AE_GA_SPIN_TEMP_VECTOR + AE_GA_VECTOR_Z, 0);
        uint32_t axis_offset = (subtype == 0x37 || subtype == 0x38) ?
            AE_GA_VECTOR_X : AE_GA_VECTOR_Z;
        if (subtype >= 0x37 && subtype <= 0x3A) {
            ae_wr16(r, AE_GA_SPIN_TEMP_VECTOR + axis_offset, (uint32_t)rotation);
        }

        uint32_t prop = ga_area_prop(ae_rd32(r, AE_G_AREA_PROPS), prop_index);
        uint32_t matrix = ae_rd32(r, prop + AE_GA_PROP_MATRIX_POINTER);
        ae_sdk_RotMatrixZYX(r, AE_GA_SPIN_TEMP_VECTOR, matrix);
    }
    return 0;
}

#undef GA_CALL
