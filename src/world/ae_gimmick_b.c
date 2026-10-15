#include "ae_gimmick_b.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "ae_solid.h"

#define AE_B_STACK_QUOTA_RESULT_UPDATE 0x801FFEF0u
#define AE_B_STACK_QUOTA_RESULT_INIT 0x801FFED8u
#define AE_B_STACK_INDICATOR_POS 0x801FFEE8u
#define AE_B_STACK_BOX_VECTOR 0x801FFED8u
#define AE_B_STACK_BOX_POSITION 0x801FFEE0u
#define AE_B_STACK_PLAYER_BLAST_VECTOR 0x801FFEE8u
#define AE_B_STACK_GRAB_POINT 0x801FFEF0u
#define AE_B_STACK_OSCILLATE_ANGLES 0x801FFEA0u
#define AE_B_STACK_OSCILLATE_MATRIX 0x801FFEC0u
#define AE_B_STACK_OSCILLATE_RESULT 0x801FFEB8u
#define AE_B_STACK_OSCILLATE_BOX 0x801FFEE0u
#define AE_B_STACK_BOUNCE_FIRST 0x801FFEE0u
#define AE_B_STACK_BOUNCE_SECOND 0x801FFEE8u
#define AE_B_STACK_ROTATE_VECTOR 0x801FFED8u
#define AE_B_STACK_ROTATE_DISTANCE 0x801FFEE0u
#define AE_B_STACK_PROP_TILT_ANGLES 0x801FFED8u

#define AE_VECTOR3_X 0x00u
#define AE_VECTOR3_Y 0x02u
#define AE_VECTOR3_Z 0x04u

static int32_t narrow_s16(int32_t value) {
    return (int16_t)(uint16_t)value;
}

static int32_t half_toward_zero(int32_t value) {
    return value / 2;
}

static int32_t wrap_add32(int32_t left, int32_t right) {
    return (int32_t)((uint32_t)left + (uint32_t)right);
}

static int32_t divide_toward_zero(int32_t numerator, int32_t denominator) {
    if (denominator == 0) return 0;
    if (numerator == INT32_MIN && denominator == -1) return INT32_MIN;
    return numerator / denominator;
}

static uint32_t gimmick_record(uint32_t base, int32_t index) {
    return base + (uint32_t)index * AE_GIMMICK_SIZE;
}

static uint32_t collider_for_gimmick(AeRam *r, uint32_t gimmick) {
    uint32_t colliders = ae_rd32(r, AE_G_COLLIDERS);
    int32_t index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    return colliders + (uint32_t)index * AE_COLLIDER_SIZE;
}

static uint32_t prop_record(uint32_t props, int32_t index) {
    return props + (uint32_t)index * AE_AREA_PROP_SIZE;
}

static void play_sfx_at(AeRam *r, const AeGimmickBHooks *h, int32_t id, uint32_t position) {
    (void)ae_PlaySfxAt(r, h ? h->sound_hw : NULL, id, position);
}

static uint32_t prop_mesh_flags(AeRam *r, uint32_t prop) {
    uint32_t mesh = ae_rd32(r, prop + AE_AREA_PROP_MESH);
    return mesh + AE_PROP_MESH_FLAGS;
}

static void set_collider_ready(AeRam *r, uint32_t collider) {
    uint32_t flags = ae_rd16(r, collider + AE_COLLIDER_FLAGS);
    ae_wr16(r, collider + AE_COLLIDER_FLAGS,
            (flags & ~AE_COLLIDER_STATE_MASK) | 1u);
}

#define GIMMICK_B_CALL(h, member, a0, a1, a2, a3) do { \
    if (!(h) || !(h)->member) { \
        ae_ram_fail(r, #member " hook not provided"); \
        return 0; \
    } \
    (void)(h)->member((h)->user, r, (uint32_t)(a0), (uint32_t)(a1), \
                      (uint32_t)(a2), (uint32_t)(a3)); \
    if (ae_ram_failed(r)) return 0; \
} while (0)

#define GIMMICK_B_RESULT(h, member, a0, a1, a2, a3, result) do { \
    if (!(h) || !(h)->member) { \
        ae_ram_fail(r, #member " hook not provided"); \
        return 0; \
    } \
    (result) = (h)->member((h)->user, r, (uint32_t)(a0), (uint32_t)(a1), \
                           (uint32_t)(a2), (uint32_t)(a3)); \
    if (ae_ram_failed(r)) return 0; \
} while (0)

int32_t ae_Gimmick_NopInit(AeRam *r, uint32_t gimmick) {
    (void)r;
    (void)gimmick;
    return 0;
}

int32_t ae_Gimmick_NopUpdate(AeRam *r, uint32_t gimmick) {
    (void)r;
    (void)gimmick;
    return 0;
}

int32_t ae_Gimmick_Init_Closed(AeRam *r, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    set_collider_ready(r, collider);
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    return 0;
}

int32_t ae_Gimmick_Init_Closed2(AeRam *r, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    set_collider_ready(r, collider);
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);

    if (ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE) == 0) {
        int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
        if (prop_index >= 0) {
            uint32_t props = ae_rd32(r, AE_G_AREA_PROPS);
            uint32_t prop = prop_record(props, prop_index);
            ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0);
        }
    }
    return 0;
}

int32_t ae_Gimmick_NopUpdate2(AeRam *r, uint32_t gimmick) {
    (void)r;
    (void)gimmick;
    return 0;
}

int32_t ae_Gimmick_Init_PropHidden(AeRam *r, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    set_collider_ready(r, collider);
    int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
    if (prop_index >= 0) {
        uint32_t props = ae_rd32(r, AE_G_AREA_PROPS);
        uint32_t prop = prop_record(props, prop_index);
        ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0);
    }
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    return 0;
}

int32_t ae_Gimmick_Init_Closed3(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    set_collider_ready(r, collider);
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    ae_wr16(r, collider + AE_COLLIDER_AUX_HALF_X,
            ae_rd16(r, collider + AE_COLLIDER_HALF_X));
    ae_wr16(r, collider + AE_COLLIDER_AUX_HALF_Y,
            ae_rd16(r, collider + AE_COLLIDER_HALF_Y));
    ae_wr16(r, collider + AE_COLLIDER_AUX_HALF_Z,
            ae_rd16(r, collider + AE_COLLIDER_HALF_Z));

    if (ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX) >= 0) {
        GIMMICK_B_CALL(h, FUN_80091AF0, collider, gimmick, 0, 0);
    }
    return 0;
}

int32_t ae_Gimmick_Init_Disabled(AeRam *r, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    uint32_t flags = ae_rd16(r, collider + AE_COLLIDER_FLAGS);
    ae_wr16(r, collider + AE_COLLIDER_FLAGS, flags | AE_COLLIDER_DISABLED);
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    return 0;
}

int32_t ae_Gimmick_Init_PropFlag2000(AeRam *r, uint32_t gimmick) {
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    if (ae_rds16(r, gimmick + AE_GIMMICK_LINK) > 0) {
        uint32_t collider = collider_for_gimmick(r, gimmick);
        int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
        uint32_t props = ae_rd32(r, AE_G_AREA_PROPS);
        uint32_t prop = prop_record(props, prop_index);
        uint32_t flags_address = prop_mesh_flags(r, prop);
        ae_wr32(r, flags_address, ae_rd32(r, flags_address) | AE_PROP_MESH_HIDDEN);
    }
    return 0;
}

int32_t ae_Gimmick_Init_Hidden(AeRam *r, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    if (ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE) >= 4) {
        uint32_t flags = ae_rd16(r, collider + AE_COLLIDER_FLAGS);
        ae_wr16(r, collider + AE_COLLIDER_FLAGS, flags | AE_COLLIDER_DISABLED);
    }
    int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
    if (prop_index >= 0) {
        uint32_t props = ae_rd32(r, AE_G_AREA_PROPS);
        uint32_t prop = prop_record(props, prop_index);
        uint32_t flags_address = prop_mesh_flags(r, prop);
        ae_wr32(r, flags_address, ae_rd32(r, flags_address) | AE_PROP_MESH_HIDDEN);
    }
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    return 0;
}

int32_t ae_Gimmick_NopUpdate3(AeRam *r, uint32_t gimmick) {
    (void)r;
    (void)gimmick;
    return 0;
}

int32_t ae_Gimmick_NopUpdate4(AeRam *r, uint32_t gimmick) {
    (void)r;
    (void)gimmick;
    return 0;
}

int32_t ae_Gimmick_Init_CounterGate(AeRam *r, uint32_t gimmick) {
    ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    if (ae_rds16(r, gimmick + AE_GIMMICK_PARAM_14) != 0) {
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 2);
        return 0;
    }

    uint32_t gimmicks = ae_rd32(r, AE_G_GIMMICKS);
    int32_t linked_index = ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE);
    uint32_t linked = gimmick_record(gimmicks, linked_index);
    int32_t count = ae_rds16(r, linked + AE_GIMMICK_PARAM_16);
    int32_t target = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A);
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, count < target ? 0u : 2u);
    return 0;
}

uint32_t ae_GimmickStateTablePtr(AeRam *r) {
    (void)r;
    return AE_G_GIMMICK_STATE_TABLE;
}

static int32_t update_monkey_quota(AeRam *r, const AeGimmickBHooks *h,
                                   uint32_t gimmick, uint32_t result_address) {
    uint32_t result_count = 0;
    int activate = 0;
    if (ae_rds16(r, gimmick + AE_GIMMICK_TIMER) != 0) return 0;

    int32_t required = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A);
    if (ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE) == 0) {
        ae_wr32(r, result_address, 0);
        int32_t already_met;
        GIMMICK_B_RESULT(h, AreaMonkeyQuotaMet, result_address, 0, 0, 0, already_met);
        result_count = ae_rd32(r, result_address);
        activate = required < 0 && already_met != 0;
        if (!activate && (int32_t)result_count >= required && required >= 0) activate = 1;
    } else {
        int32_t total;
        GIMMICK_B_RESULT(h, FUN_80097504, 0, 0, 0, 0, total);
        result_count = (uint32_t)total;
        activate = total >= required;
    }

    if (activate) {
        (void)ae_GimmickActivate(r, h ? h->gimmick_hooks : NULL,
                                 ae_rds16(r, gimmick + AE_GIMMICK_LINK), gimmick);
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 1);
    }

    ae_wr16(r, gimmick + AE_GIMMICK_PARAM_16, result_count);
    return 0;
}

int32_t ae_Gimmick_Init_MonkeyQuota(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_PARAM_16, 0);
    (void)update_monkey_quota(r, h, gimmick, AE_B_STACK_QUOTA_RESULT_INIT);
    return 0;
}

int32_t ae_Gimmick_MonkeyQuota_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    return update_monkey_quota(r, h, gimmick, AE_B_STACK_QUOTA_RESULT_UPDATE);
}

int32_t ae_Gimmick_Init_ZoneMask(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    ae_wr16(r, collider + AE_COLLIDER_ZONE_MASK, 0x1111);
    set_collider_ready(r, collider);
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    GIMMICK_B_CALL(h, SpecialActorPlaceAtCollider,
                   ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE),
                   ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER),
                   ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A), 0);
    return 0;
}

int32_t ae_Gimmick_PlayerInCylinder_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    uint32_t flags = ae_rd16(r, collider + AE_COLLIDER_FLAGS);
    if (flags & AE_COLLIDER_DISABLED) return 0;

    int32_t dx = ae_rds16(r, collider + AE_COLLIDER_POS_X) - ae_rds32(r, AE_G_PLAYER_POS_X);
    int32_t dz = ae_rds16(r, collider + AE_COLLIDER_POS_Z) - ae_rds32(r, AE_G_PLAYER_POS_Z);
    uint32_t horizontal_sq = (uint32_t)(dx * dx) + (uint32_t)(dz * dz);
    int32_t radius = ae_rds16(r, collider + AE_COLLIDER_HALF_X);
    if ((int32_t)horizontal_sq >= radius * radius) return 0;

    int32_t player_y = ae_rds32(r, AE_G_PLAYER_POS_Y);
    int32_t top_y = ae_rds16(r, collider + AE_COLLIDER_POS_Y);
    int32_t bottom_y = top_y - ae_rds16(r, collider + AE_COLLIDER_HALF_Y);
    if (player_y < bottom_y || player_y > top_y) return 0;

    GIMMICK_B_CALL(h, FUN_8009B3B0, 0, 0, 0, 0);
    return 0;
}

int32_t ae_Gimmick_ProxTrigger_Init(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    set_collider_ready(r, collider);

    int32_t time_attack;
    GIMMICK_B_RESULT(h, IsTimeAttack, 0, 0, 0, 0, time_attack);
    if (time_attack != 0) {
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 2);
        ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
    } else {
        ae_wr16(r, collider + AE_COLLIDER_HALF_X, 80);
        ae_wr16(r, collider + AE_COLLIDER_HALF_Z, 80);
        ae_wr16(r, collider + AE_COLLIDER_HALF_Y, 256);
        ae_wr16(r, AE_B_STACK_INDICATOR_POS + 0, ae_rd16(r, collider + AE_COLLIDER_POS_X));
        ae_wr16(r, AE_B_STACK_INDICATOR_POS + 2, ae_rd16(r, collider + AE_COLLIDER_POS_Y));
        ae_wr16(r, AE_B_STACK_INDICATOR_POS + 4, ae_rd16(r, collider + AE_COLLIDER_POS_Z));
        int32_t indicator;
        GIMMICK_B_RESULT(h, Indicator_Register,
                         ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A), AE_B_STACK_INDICATOR_POS,
                         ae_rds16(r, gimmick + AE_GIMMICK_LINK), 0, indicator);
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_14, indicator);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);

        uint32_t area = ae_rd16(r, AE_G_CURRENT_AREA);
        int is_special_area = (uint32_t)(area - 92u) < 7u;
        ae_wr16(r, gimmick + AE_GIMMICK_STATE,
                is_special_area || ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE) == 0 ? 0u : 1u);
    }
    if (time_attack == 0)
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_10, 0);
    return 0;
}

static int32_t activate_proximity_effect(AeRam *r, const AeGimmickBHooks *h,
                                         uint32_t gimmick, uint32_t collider) {
    GIMMICK_B_CALL(h, Indicator_Activate,
                   ae_rds16(r, gimmick + AE_GIMMICK_PARAM_14),
                   ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12), 0, 0);
    int32_t y = (int16_t)(ae_rds16(r, collider + AE_COLLIDER_POS_Y) - 190);
    GIMMICK_B_CALL(h, SpawnEffectGroupAt, 274,
                   ae_rds16(r, collider + AE_COLLIDER_POS_X), y,
                   ae_rds16(r, collider + AE_COLLIDER_POS_Z));
    return 0;
}

int32_t ae_Gimmick_ProxTrigger_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    uint32_t area = ae_rd8(r, AE_G_STAGE_AREA);
    if (area == 23 || area == 7 || area == 21) return 0;

    uint32_t collider = collider_for_gimmick(r, gimmick);
    int32_t dx = ae_rds32(r, AE_G_PLAYER_POS_X) - ae_rds16(r, collider + AE_COLLIDER_POS_X);
    int32_t dy = ae_rds32(r, AE_G_PLAYER_POS_Y) - ae_rds16(r, collider + AE_COLLIDER_POS_Y);
    int32_t dz = ae_rds32(r, AE_G_PLAYER_POS_Z) - ae_rds16(r, collider + AE_COLLIDER_POS_Z);
    int32_t distance;
    distance = ae_player_vec_length3(dx, dy, dz);

    int32_t state = ae_rds16(r, gimmick + AE_GIMMICK_STATE);
    int32_t radius = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_E);
    if (state == 0) {
        if (distance >= radius) return 0;
        GIMMICK_B_CALL(h, Indicator_Activate,
                       ae_rds16(r, gimmick + AE_GIMMICK_PARAM_14),
                       ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12), 0, 0);
        ae_wr16(r, gimmick + AE_GIMMICK_STATE,
                (uint32_t)(area - 92u) < 7u ? 3u : 1u);
    } else if (state == 1) {
        if (distance >= radius || ae_rds16(r, collider + AE_COLLIDER_TOUCHER) < 0 ||
            ae_rds16(r, gimmick + AE_GIMMICK_PARAM_10) != 0) return 0;
        return activate_proximity_effect(r, h, gimmick, collider);
    } else if (state == 2) {
        return 0;
    } else if (state == 3) {
        if (distance > radius) {
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
            return 0;
        }
        if (ae_rds16(r, collider + AE_COLLIDER_TOUCHER) < 0) return 0;
    }

    if (state == 3) {
        return activate_proximity_effect(r, h, gimmick, collider);
    }
    return 0;
}

int32_t ae_Gimmick_Breakable_Init(AeRam *r, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    if (ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A) == 0) {
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 3);
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_A, 0);
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_16, 0);
        return 0;
    }

    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);
    int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
    if (prop_index >= 0) {
        uint32_t props = ae_rd32(r, AE_G_AREA_PROPS);
        ae_wr16(r, prop_record(props, prop_index) + AE_AREA_PROP_ZONE_MASK, 0);
    }
    ae_wr16(r, collider + AE_COLLIDER_FLAGS,
            ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
    return 0;
}

int32_t ae_Gimmick_Breakable_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    if (ae_rds16(r, gimmick + AE_GIMMICK_STATE) != 0) return 0;

    int32_t toucher = ae_rds16(r, collider + AE_COLLIDER_TOUCHER);
    if (toucher < 0) {
        GIMMICK_B_CALL(h, FUN_80091598, collider, gimmick, 0, 0);
        return 0;
    }
    if (ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE) == 1 &&
        ((uint32_t)toucher & 0xFF00u) != 0x0800u) return 0;
    ae_CameraShake(r, 0);

    int32_t hits = (int16_t)(ae_rd16(r, gimmick + AE_GIMMICK_PARAM_A) + 1u);
    ae_wr16(r, gimmick + AE_GIMMICK_PARAM_A, hits);
    if (hits < ae_rds16(r, gimmick + AE_GIMMICK_LINK)) {
        GIMMICK_B_CALL(h, FUN_80091598, collider, gimmick, 0, 0);
        return 0;
    }

    ae_wr16(r, gimmick + AE_GIMMICK_PARAM_A, 0);
    uint32_t timer = ae_rd8(r, gimmick + AE_GIMMICK_TIMER);
    int should_break = timer >= 8;
    if (!should_break) {
        uint32_t hit_state = gimmick + AE_GIMMICK_SUBTYPE + timer * 2u;
        should_break = ae_rds16(r, hit_state) < 0;
    }
    if (should_break) {
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);
        ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                ae_rd16(r, collider + AE_COLLIDER_FLAGS) | AE_COLLIDER_DISABLED);
        GIMMICK_B_CALL(h, PadRumble, 0, 1, 3, 0);
        int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
        if (prop_index >= 0) {
            uint32_t prop = prop_record(ae_rd32(r, AE_G_AREA_PROPS), prop_index);
            ae_wr16(r, prop + AE_AREA_PROP_ZONE_MASK, 0);
            uint32_t mode = ae_rd16(r, AE_G_CURRENT_AREA) == 29 ? 2u : 1u;
            GIMMICK_B_CALL(h, FUN_8006AA60, collider, prop_index, mode, 0);
        }
        return 0;
    }

    int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
    if (prop_index >= 0) {
        uint32_t prop = prop_record(ae_rd32(r, AE_G_AREA_PROPS), prop_index);
        GIMMICK_B_CALL(h, MeshInitFromAreaTable, prop, 0, 0, 0);
    }
    ae_wr16(r, gimmick + AE_GIMMICK_TIMER, timer + 1u);
    GIMMICK_B_CALL(h, FUN_80091598, collider, gimmick, 0, 0);
    return 0;
}

int32_t ae_Gimmick_Resize_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    if (ae_rds16(r, gimmick + AE_GIMMICK_STATE) != 1) return 0;
    uint32_t collider = collider_for_gimmick(r, gimmick);
    if (ae_rds16(r, collider + AE_COLLIDER_BREAKABLE_COUNT) == 1) {
        uint32_t level = ae_rd16(r, AE_G_CURRENT_LEVEL);
        uint32_t area = ae_rd16(r, AE_G_CURRENT_AREA);
        if (level != 6 && area != 0x65) play_sfx_at(r, h, 155, collider);
    }

    int32_t current = ae_rds16(r, collider + AE_COLLIDER_BREAKABLE_COUNT);
    if (current < ae_rds16(r, gimmick + AE_GIMMICK_LINK)) return 0;
    set_collider_ready(r, collider);
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 2);
    if (ae_rd16(r, AE_G_CURRENT_LEVEL) != 6 && ae_rd16(r, AE_G_CURRENT_AREA) != 0x65)
        play_sfx_at(r, h, 155, collider);
    return 0;
}

int32_t ae_Gimmick_Slider_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    int32_t state = ae_rds16(r, gimmick + AE_GIMMICK_STATE);
    if (state == 1) {
        if (ae_rds16(r, gimmick + AE_GIMMICK_TIMER) == 0)
            play_sfx_at(r, h, 155, collider);
        int32_t progress = (int16_t)(ae_rd16(r, gimmick + AE_GIMMICK_TIMER) +
                                     ae_rd16(r, gimmick + AE_GIMMICK_PARAM_A));
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, progress);
        if (progress > 4096) {
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 4096);
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 2);
            play_sfx_at(r, h, 155, collider);
        }
    } else if (state == 2) {
        int32_t progress = (int16_t)(ae_rd16(r, gimmick + AE_GIMMICK_TIMER) -
                                     ae_rd16(r, gimmick + AE_GIMMICK_PARAM_E));
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, progress);
        if (progress < 0) {
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
        }
    }
    if (ae_rds16(r, gimmick + AE_GIMMICK_STATE) != 0)
        GIMMICK_B_CALL(h, FUN_80091AF0, collider, gimmick, 0, 0);
    return 0;
}

int32_t ae_Gimmick_SliderQuiet_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    int32_t state = ae_rds16(r, gimmick + AE_GIMMICK_STATE);
    if (state == 1) {
        int32_t progress = (int16_t)(ae_rd16(r, gimmick + AE_GIMMICK_TIMER) +
                                     ae_rd16(r, gimmick + AE_GIMMICK_PARAM_A));
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, progress);
        if (progress > 4096) ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 4096);
    } else if (state == 2) {
        int32_t progress = (int16_t)(ae_rd16(r, gimmick + AE_GIMMICK_TIMER) -
                                     ae_rd16(r, gimmick + AE_GIMMICK_PARAM_E));
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, progress);
        if (progress < 0) ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    }
    if (ae_rds16(r, gimmick + AE_GIMMICK_STATE) != 0)
        GIMMICK_B_CALL(h, FUN_80091AF0, collider, gimmick, 0, 0);
    return 0;
}

int32_t ae_Gimmick_Init_RecordOrigin(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    ae_wr16(r, collider + AE_COLLIDER_ORIGIN_TAG, 0xFFFF);
    ae_wr16(r, collider + AE_COLLIDER_ORIGIN_X, ae_rd16(r, collider + AE_COLLIDER_POS_X));
    ae_wr16(r, collider + AE_COLLIDER_ORIGIN_Y, ae_rd16(r, collider + AE_COLLIDER_POS_Y));
    ae_wr16(r, collider + AE_COLLIDER_ORIGIN_Z, ae_rd16(r, collider + AE_COLLIDER_POS_Z));
    ae_wr16(r, collider + AE_COLLIDER_AUX_HALF_X, ae_rd16(r, collider + AE_COLLIDER_HALF_X));
    ae_wr16(r, collider + AE_COLLIDER_AUX_HALF_Y, ae_rd16(r, collider + AE_COLLIDER_HALF_Y));
    ae_wr16(r, collider + AE_COLLIDER_AUX_HALF_Z, ae_rd16(r, collider + AE_COLLIDER_HALF_Z));
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_PARAM_16, 0);

    uint32_t gimmicks = ae_rd32(r, AE_G_GIMMICKS);
    uint32_t packed_ids = ae_rd16(r, gimmick + AE_GIMMICK_PARAM_A);
    uint32_t low_id = packed_ids & 0xFFu;
    uint32_t high_id = (packed_ids >> 8) & 0xFFu;
    if (low_id != 0xFFu)
        ae_wr16(r, gimmicks + low_id * AE_GIMMICK_SIZE + AE_GIMMICK_TIMER,
                ae_rds16(r, gimmick + AE_GIMMICK_PARAM_E) * 2);
    if (high_id != 0xFFu)
        ae_wr16(r, gimmicks + high_id * AE_GIMMICK_SIZE + AE_GIMMICK_TIMER,
                ae_rds16(r, gimmick + AE_GIMMICK_PARAM_E) * 2);
    int32_t saved = 0;
    GIMMICK_B_RESULT(h, FUN_80092324, gimmick, collider, 0, 0, saved);
    ae_wr16(r, collider + AE_COLLIDER_GEOMETRY, saved);
    return 0;
}

int32_t ae_Gimmick_Init_PropTilt(AeRam *r, uint32_t gimmick) {
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
    ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    int32_t prop_index = ae_rds16(r, gimmick + AE_GIMMICK_LINK);
    if (prop_index < 0) return 0;

    uint32_t angles = AE_B_STACK_PROP_TILT_ANGLES;
    ae_wr16(r, angles, 0);
    ae_wr16(r, angles + 2, 0);
    ae_wr16(r, angles + 4, 0);
    int32_t amount = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_E);
    if (ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE) == 0)
        ae_wr16(r, angles, (int16_t)(-amount / 2));
    else
        ae_wr16(r, angles + 4, (int16_t)(amount / 2));
    uint32_t prop = prop_record(ae_rd32(r, AE_G_AREA_PROPS), prop_index);
    (void)ae_sdk_RotMatrixZYX(r, angles, ae_rd32(r, prop + AE_AREA_PROP_ROTATION));
    return 0;
}

int32_t ae_Gimmick_Init_PropMeshSet(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    int32_t set_index = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A);
    int32_t first_prop = ae_rds16(r, gimmick + AE_GIMMICK_LINK);
    int32_t mesh_index = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_E);
    if (set_index < 0 || first_prop < 0 || mesh_index < 0) return 0;

    uint32_t table = gimmick_record(ae_rd32(r, AE_G_GIMMICKS), set_index);
    int32_t mesh_count = ae_rds16(r, table + AE_GIMMICK_PARAM_16);
    int32_t requested_count = ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE);
    for (int32_t i = 0; i < mesh_count && i < requested_count; ++i) {
        uint32_t prop = prop_record(ae_rd32(r, AE_G_AREA_PROPS), first_prop + i);
        GIMMICK_B_CALL(h, MeshInitFromAreaTable, prop, mesh_index, 0, 0);
    }
    return 0;
}

int32_t ae_Gimmick_Counter_EventCam_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    int32_t state = ae_rds16(r, gimmick + AE_GIMMICK_STATE);
    int32_t event_index = ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE);
    if (event_index < 0) return 0;
    if (state == 0) {
        int32_t required = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A);
        if (required < 0) return 0;
        uint32_t record = gimmick_record(ae_rd32(r, AE_G_GIMMICKS), event_index);
        int32_t count = ae_rds16(r, record + AE_GIMMICK_PARAM_16);
        if (count < required || ae_rd16(r, AE_G_EVENT_CAM_BUSY) != 8) return 0;
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);
        return 0;
    }
    if (state == 1) {
        if (ae_rd16(r, AE_G_EVENT_CAM_BUSY) != 0) return 0;
        GIMMICK_B_CALL(h, EventCamStart, ae_rds16(r, gimmick + AE_GIMMICK_LINK) - 1, 0, 0, 0);
        ae_wr32(r, AE_G_STAGE_ENTRY_FLAGS, 7);
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 2);
    }
    return 0;
}

static int32_t wrap_mul32(int32_t left, int32_t right) {
    return (int32_t)(uint32_t)((int64_t)left * right);
}

static int32_t shift_q12_floor(int32_t value) {
    if (value < 0) value = (int32_t)((uint32_t)value + 4095u);
    return value >> 12;
}

int32_t ae_Gimmick_PlayerBox_Update(AeRam *r, uint32_t gimmick) {
    (void)ae_GimmickPlayerInBox(r, ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE),
                                gimmick + AE_GIMMICK_PARAM_A, 128);
    return 0;
}

int32_t ae_GimmickPlayerInBox(AeRam *r, int32_t kind_arg, uint32_t box, int32_t extent_arg) {
    int32_t kind = (int16_t)kind_arg;
    int32_t extent = (int16_t)extent_arg;
    int32_t player_x = ae_rds32(r, AE_G_PLAYER_POS_X);
    int32_t player_y = ae_rds32(r, AE_G_PLAYER_POS_Y);
    int32_t player_z = ae_rds32(r, AE_G_PLAYER_POS_Z);
    int32_t box_x = ae_rds16(r, box);
    int32_t box_y = ae_rds16(r, box + 2u);
    int32_t box_z = ae_rds16(r, box + 4u);
    int32_t half_x = ae_rds16(r, box + 6u);
    int32_t half_y = ae_rds16(r, box + 8u);
    int32_t half_z = ae_rds16(r, box + 10u);

    if (box_x - half_x >= player_x || player_x >= box_x + half_x) return 0;
    if (player_y >= box_y || player_y <= box_y - half_y) return 0;
    if (box_z - half_z >= player_z || player_z >= box_z + half_z) return 0;

    int32_t direction = kind;
    if (kind == 5) {
        direction = player_x < box_x;
    } else if (kind == 6) {
        direction = player_z < box_z ? 3 : 2;
    }

    ae_wr16(r, AE_B_STACK_BOX_VECTOR, 0);
    ae_wr16(r, AE_B_STACK_BOX_VECTOR + 2u, 0);
    ae_wr16(r, AE_B_STACK_BOX_VECTOR + 4u, 0);
    ae_wr32(r, AE_B_STACK_BOX_POSITION, player_x);
    ae_wr32(r, AE_B_STACK_BOX_POSITION + 4u, player_y);
    ae_wr32(r, AE_B_STACK_BOX_POSITION + 8u, player_z);

    if (direction == 0) {
        ae_wr16(r, AE_B_STACK_BOX_VECTOR, 4096);
        ae_wr32(r, AE_B_STACK_BOX_POSITION, player_x - 64);
    } else if (direction == 1) {
        ae_wr16(r, AE_B_STACK_BOX_VECTOR, -4096);
        ae_wr32(r, AE_B_STACK_BOX_POSITION, player_x + 64);
    } else if (direction == 2) {
        ae_wr16(r, AE_B_STACK_BOX_VECTOR + 4u, 4096);
        ae_wr32(r, AE_B_STACK_BOX_POSITION + 8u, player_z - 64);
    } else if (direction == 3) {
        ae_wr16(r, AE_B_STACK_BOX_VECTOR + 4u, -4096);
        ae_wr32(r, AE_B_STACK_BOX_POSITION + 8u, player_z + 64);
    } else if (direction == 4) {
        ae_wr16(r, AE_B_STACK_BOX_VECTOR, player_x - box_x);
        ae_wr16(r, AE_B_STACK_BOX_VECTOR + 2u, player_y - box_y);
        ae_wr16(r, AE_B_STACK_BOX_VECTOR + 4u, player_z - box_z);
        (void)ae_sdk_VectorNormalSS(r, AE_B_STACK_BOX_VECTOR, AE_B_STACK_BOX_VECTOR);
        ae_wr32(r, AE_B_STACK_BOX_POSITION, box_x);
        ae_wr32(r, AE_B_STACK_BOX_POSITION + 4u, box_y);
        ae_wr32(r, AE_B_STACK_BOX_POSITION + 8u, box_z);
    }

    uint32_t body = ae_CollisionBodyAlloc(r);
    if (body == 0 || ae_ram_failed(r)) return 0;
    ae_wr32(r, body + AE_BODY_FACING, ae_rd32(r, AE_B_STACK_BOX_VECTOR));
    ae_wr32(r, body + AE_BODY_FACING_Y, ae_rd32(r, AE_B_STACK_BOX_VECTOR + 4u));
    ae_wr32(r, body + AE_BODY_POSITION_X, ae_rd32(r, AE_B_STACK_BOX_POSITION));
    ae_wr32(r, body + AE_BODY_POSITION_Y, ae_rd32(r, AE_B_STACK_BOX_POSITION + 4u));
    ae_wr32(r, body + AE_BODY_POSITION_Z, ae_rd32(r, AE_B_STACK_BOX_POSITION + 8u));
    ae_wr16(r, body + AE_BODY_RADIUS, direction == 4 ? half_x : extent);
    ae_wr16(r, body + AE_BODY_PARAM, 1);
    ae_wr16(r, body + AE_BODY_FLAGS, 514);
    (void)ae_CollisionBodyRegister(r, NULL, 6, body, 0);
    return 0;
}

int32_t ae_Gimmick_LinkedLift_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    uint32_t gimmicks = ae_rd32(r, AE_G_GIMMICKS);
    uint32_t packed_meshes = ae_rd16(r, gimmick + AE_GIMMICK_PARAM_A);
    int32_t half_mesh_count = 0;

    for (int32_t shift = 0; shift <= 8; shift += 8) {
        uint32_t mesh_index = (packed_meshes >> shift) & 0xFFu;
        if (mesh_index != 0xFFu) {
            uint32_t mesh_gimmick = gimmick_record(gimmicks, (int32_t)mesh_index);
            int32_t mesh_count = ae_rds16(r, mesh_gimmick + AE_GIMMICK_PARAM_16);
            half_mesh_count += half_toward_zero(mesh_count);
        }
    }

    int32_t state = ae_rds16(r, gimmick + AE_GIMMICK_STATE);
    int32_t height = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_16);
    int32_t lower_bound = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_E);
    if (state == 0) {
        if (half_mesh_count >= height) {
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_16, half_mesh_count);
            if (half_mesh_count >= lower_bound) {
                ae_wr16(r, gimmick + AE_GIMMICK_PARAM_16, lower_bound);
                ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
                ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);
            }
        } else {
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_16,
                    height - ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12));
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 2);
        }
    } else if (state == 1) {
        int32_t timer = narrow_s16(ae_rd16(r, gimmick + AE_GIMMICK_TIMER) + 1);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, timer);
        if (timer > ae_rds16(r, gimmick + AE_GIMMICK_PARAM_14))
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 2);
    } else if (state == 2) {
        int32_t next_height = height - ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12);
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_16, next_height);
        if (next_height < half_mesh_count) {
            next_height = half_mesh_count;
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_16, next_height);
            if (lower_bound < half_mesh_count) {
                ae_wr16(r, gimmick + AE_GIMMICK_PARAM_16, lower_bound);
                ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
                ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);
            }
        } else if (next_height < 0) {
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_16, 0);
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
        }
    }

    height = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_16);
    int32_t sound_handle = ae_rds16(r, collider + AE_COLLIDER_ORIGIN_TAG);
    if (height > 0) {
        if (sound_handle < 0) {
            int32_t started;
            GIMMICK_B_RESULT(h, SoundLoopStart, 1, 127, 0, 0, started);
            ae_wr16(r, collider + AE_COLLIDER_ORIGIN_TAG, started);
        } else {
            GIMMICK_B_CALL(h, SoundEmitterPan, 127, sound_handle, 8192,
                           collider + AE_COLLIDER_ORIGIN_X);
        }
    } else if (sound_handle >= 0) {
        GIMMICK_B_CALL(h, SoundLoopStop, 127, sound_handle, 0, 0);
        ae_wr16(r, collider + AE_COLLIDER_ORIGIN_TAG, -1);
    }

    int32_t mesh_state;
    GIMMICK_B_RESULT(h, FUN_80092324, gimmick, collider, 0, 0, mesh_state);
    ae_wr16(r, collider + AE_COLLIDER_GEOMETRY, mesh_state);
    GIMMICK_B_CALL(h, FUN_80091C04,
                   ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE),
                   ae_rds16(r, gimmick + AE_GIMMICK_LINK),
                   ae_rds16(r, gimmick + AE_GIMMICK_PARAM_16), 0);
    return 0;
}

int32_t ae_Gimmick_Oscillate_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    int32_t state = ae_rds16(r, gimmick + AE_GIMMICK_STATE);
    int32_t duration = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A);
    int32_t timer = ae_rds16(r, gimmick + AE_GIMMICK_TIMER);
    int32_t amount = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_E);
    int32_t angle = 0;

    if (state == 0 || state == 1) {
        timer = narrow_s16(timer + 1);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, timer);
        if (timer > duration) {
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, state == 0 ? 1 : 0);
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
            angle = state == 0 ? half_toward_zero(amount) : half_toward_zero(-amount);
        } else {
            if (duration == 0) {
                ae_ram_fail(r, "Gimmick_Oscillate_Update division by zero");
                return 0;
            }
            int32_t phase = divide_toward_zero((int32_t)((uint32_t)timer << 11), duration);
            int32_t cosine = ae_sdk_rcos(r, phase);
            int32_t half_amount = state == 0 ? half_toward_zero(-amount)
                                              : half_toward_zero(amount);
            angle = shift_q12_floor(wrap_mul32(half_amount, cosine));
        }
    }

    if (ae_rds16(r, gimmick + AE_GIMMICK_LINK) >= 0) {
        uint32_t prop = prop_record(ae_rd32(r, AE_G_AREA_PROPS),
                                    ae_rds16(r, gimmick + AE_GIMMICK_LINK));
        uint32_t angles = AE_B_STACK_OSCILLATE_ANGLES;
        ae_wr16(r, angles + AE_VECTOR3_X, 0);
        ae_wr16(r, angles + AE_VECTOR3_Y, 0);
        ae_wr16(r, angles + AE_VECTOR3_Z, 0);
        uint32_t angle_axis = ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE) == 0
                                  ? AE_VECTOR3_X : AE_VECTOR3_Z;
        ae_wr16(r, angles + angle_axis, angle);
        (void)ae_sdk_RotMatrixZYX(r, angles, AE_B_STACK_OSCILLATE_MATRIX);

        ae_wr16(r, AE_B_STACK_OSCILLATE_ANGLES + AE_VECTOR3_X, 0);
        ae_wr16(r, AE_B_STACK_OSCILLATE_ANGLES + AE_VECTOR3_Y,
                ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12));
        ae_wr16(r, AE_B_STACK_OSCILLATE_ANGLES + AE_VECTOR3_Z, 0);
        (void)ae_sdk_ApplyMatrixSV(r, AE_B_STACK_OSCILLATE_MATRIX,
                                   AE_B_STACK_OSCILLATE_ANGLES,
                                   AE_B_STACK_OSCILLATE_RESULT);

        uint32_t box = AE_B_STACK_OSCILLATE_BOX;
        for (int32_t axis = 0; axis < 3; ++axis) {
            int32_t origin = ae_rds16(r, prop + AE_AREA_PROP_POSITION_X + (uint32_t)axis * 2u);
            int32_t moved = ae_rds16(r, AE_B_STACK_OSCILLATE_RESULT + (uint32_t)axis * 2u);
            ae_wr16(r, box + (uint32_t)axis * 2u, origin + moved);
            ae_wr16(r, box + 6u + (uint32_t)axis * 2u,
                    ae_rds16(r, gimmick + AE_GIMMICK_PARAM_10));
        }
        (void)ae_GimmickPlayerInBox(r, 4, box,
                                    ae_rds16(r, gimmick + AE_GIMMICK_PARAM_10));
        if (ae_ram_failed(r)) return 0;

        uint32_t prop_matrix_data = ae_rd32(r, prop + AE_AREA_PROP_ROTATION);
        for (uint32_t offset = 0; offset < 0x20u; offset += 4u)
            ae_wr32(r, prop_matrix_data + offset,
                    ae_rd32(r, AE_B_STACK_OSCILLATE_MATRIX + offset));

        if (ae_rds16(r, gimmick + AE_GIMMICK_TIMER) ==
            half_toward_zero(ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A))) {
            uint32_t area = ae_rd16(r, AE_G_CURRENT_AREA);
            if (area == 60 && ae_rds32(r, AE_G_PLAYER_POS_Y) < -2816)
                play_sfx_at(r, h, 180, AE_B_STACK_OSCILLATE_BOX);
            else if (area == 23)
                play_sfx_at(r, h, 178, AE_B_STACK_OSCILLATE_BOX);
        }
    }
    return 0;
}

int32_t ae_Gimmick_PlayerBlast_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    int32_t x = ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE);
    int32_t y = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A);
    int32_t z = ae_rds16(r, gimmick + AE_GIMMICK_LINK);
    int32_t dx = ae_rds32(r, AE_G_PLAYER_POS_X) - x;
    int32_t dy = ae_rds32(r, AE_G_PLAYER_POS_Y) - y;
    int32_t dz = ae_rds32(r, AE_G_PLAYER_POS_Z) - z;
    int32_t distance;
    distance = ae_player_vec_length3(dx, dy, dz);
    int32_t extent = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_E);
    if (extent < distance) return 0;

    ae_wr16(r, AE_B_STACK_PLAYER_BLAST_VECTOR + AE_VECTOR3_X, dx);
    ae_wr16(r, AE_B_STACK_PLAYER_BLAST_VECTOR + AE_VECTOR3_Y, dy);
    ae_wr16(r, AE_B_STACK_PLAYER_BLAST_VECTOR + AE_VECTOR3_Z, dz);
    (void)ae_sdk_VectorNormalSS(r, AE_B_STACK_PLAYER_BLAST_VECTOR,
                                AE_B_STACK_PLAYER_BLAST_VECTOR);
    uint32_t body = ae_CollisionBodyAlloc(r);
    if (body == 0 || ae_ram_failed(r)) return 0;
    ae_wr32(r, body + AE_BODY_FACING,
            ae_rd32(r, AE_B_STACK_PLAYER_BLAST_VECTOR));
    ae_wr32(r, body + AE_BODY_FACING + 4u,
            ae_rd32(r, AE_B_STACK_PLAYER_BLAST_VECTOR + 4u));
    ae_wr32(r, body + AE_BODY_POSITION_X, x);
    ae_wr32(r, body + AE_BODY_POSITION_Y, y);
    ae_wr32(r, body + AE_BODY_POSITION_Z, z);
    ae_wr16(r, body + AE_BODY_RADIUS, extent);
    int32_t kind = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_10);
    ae_wr16(r, body + AE_BODY_PARAM, kind == 1 ? 0x301 : kind == 2 ? 0x401 : 0x201);
    ae_wr16(r, body + AE_BODY_FLAGS, 0x202);
    (void)ae_CollisionBodyRegister(r, h ? h->body_hooks : NULL, 6, body, 0);
    return 0;
}

int32_t ae_Gimmick_Grab_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    int32_t state = ae_rds16(r, gimmick + AE_GIMMICK_STATE);
    if (state == 0) {
        uint32_t collider = collider_for_gimmick(r, gimmick);
        int32_t grabbed;
        GIMMICK_B_RESULT(h, FUN_80093848,
                         ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE), collider,
                         AE_B_STACK_GRAB_POINT, 0, grabbed);
        if (grabbed != 0) {
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 30);
            GIMMICK_B_CALL(h, PlayerAttachSet, AE_B_STACK_GRAB_POINT, 30,
                           AE_B_STACK_GRAB_POINT, 0);
        }
    } else if (state == 1) {
        int32_t timer = narrow_s16(ae_rd16(r, gimmick + AE_GIMMICK_TIMER) - 1);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, timer);
        if (timer < 0) ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
    }
    return 0;
}

int32_t ae_Gimmick_Bounce_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    int32_t state = ae_rds16(r, gimmick + AE_GIMMICK_STATE);
    int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
    uint32_t prop = prop_index >= 0 ? prop_record(ae_rd32(r, AE_G_AREA_PROPS), prop_index) : 0;

    if (state == 0) {
        int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
        if (ae_rds16(r, AE_G_PLAYER + AE_PLAYER_COLLIDER_INDEX) == collider_index) {
            ae_wr16(r, gimmick + AE_GIMMICK_STATE,
                    ae_rds16(r, gimmick + AE_GIMMICK_LINK) > 0 ? 3 : 1);
            ae_wr16(r, gimmick + AE_GIMMICK_SUBTYPE, 0x300);
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_A, 0);
            GIMMICK_B_CALL(h, PadRumble, 0, 1, 5, 0);
            uint32_t area = ae_rd16(r, AE_G_CURRENT_AREA);
            int32_t sound = 174;
            if (area == 7) sound = 181;
            else if (area == 81 || area == 82 || area == 84) sound = 179;
            else if (area == 50) sound = 182;
            GIMMICK_B_CALL(h, PlaySfx, sound, 0, 0, 0);
            GIMMICK_B_CALL(h, FUN_80066F34, 0, 0, 0, 0);
            return 0;
        }
        if (ae_rds16(r, gimmick + AE_GIMMICK_PARAM_12) > 0) return 0;

        int32_t dx = ae_rds32(r, AE_G_PLAYER_POS_X) - ae_rds16(r, collider + AE_COLLIDER_POS_X);
        int32_t dz = ae_rds32(r, AE_G_PLAYER_POS_Z) - ae_rds16(r, collider + AE_COLLIDER_POS_Z);
        int32_t vertical_edge = ae_rds16(r, collider + AE_COLLIDER_HALF_X) +
                                ae_rds16(r, AE_G_PLAYER + AE_PLAYER_VERTICAL_EXTENT) + 32;
        int32_t horizontal_squared = wrap_add32(wrap_mul32(dx, dx), wrap_mul32(dz, dz));
        int32_t vertical_squared = wrap_mul32(vertical_edge, vertical_edge);
        if (vertical_squared < horizontal_squared) return 0;
        int32_t player_y = ae_rds32(r, AE_G_PLAYER_POS_Y);
        int32_t collider_y = ae_rds16(r, collider + AE_COLLIDER_POS_Y);
        int32_t collider_bottom = wrap_add32(collider_y,
                                             -ae_rds16(r, collider + AE_COLLIDER_HALF_Y));
        if (player_y > collider_y || player_y < collider_bottom)
            return 0;

        ae_wr16(r, gimmick + AE_GIMMICK_STATE,
                ae_rds16(r, gimmick + AE_GIMMICK_LINK) > 0 ? 3 : 2);
        uint32_t area = ae_rd16(r, AE_G_CURRENT_AREA);
        if (area == 25) GIMMICK_B_CALL(h, PlaySfx, 187, 0, 0, 0);
        GIMMICK_B_CALL(h, PadRumble, 0, 1, 5, 0);
        ae_wr16(r, gimmick + AE_GIMMICK_SUBTYPE, 0x300);
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_A, 0);

        ae_wr16(r, AE_B_STACK_BOUNCE_FIRST + AE_VECTOR3_X,
                ae_rds32(r, AE_G_PLAYER_POS_X) + ae_rds16(r, AE_G_PLAYER + AE_PLAYER_ROTATE_X) -
                    ae_rds16(r, collider + AE_COLLIDER_POS_X));
        ae_wr16(r, AE_B_STACK_BOUNCE_FIRST + AE_VECTOR3_Y, 0);
        ae_wr16(r, AE_B_STACK_BOUNCE_FIRST + AE_VECTOR3_Z,
                ae_rds32(r, AE_G_PLAYER_POS_Z) + ae_rds16(r, AE_G_PLAYER + AE_PLAYER_ROTATE_Z) -
                    ae_rds16(r, collider + AE_COLLIDER_POS_Z));
        (void)ae_sdk_VectorNormalSS(r, AE_B_STACK_BOUNCE_FIRST, AE_B_STACK_BOUNCE_FIRST);
        ae_wr16(r, AE_B_STACK_BOUNCE_SECOND + AE_VECTOR3_X,
                ae_rds16(r, AE_G_PLAYER + AE_PLAYER_ROTATE_X));
        ae_wr16(r, AE_B_STACK_BOUNCE_SECOND + AE_VECTOR3_Y, 0);
        ae_wr16(r, AE_B_STACK_BOUNCE_SECOND + AE_VECTOR3_Z,
                ae_rds16(r, AE_G_PLAYER + AE_PLAYER_ROTATE_Z));
        (void)ae_sdk_VectorNormalSS(r, AE_B_STACK_BOUNCE_SECOND, AE_B_STACK_BOUNCE_SECOND);
        for (uint32_t axis = 0; axis < 3; ++axis) {
            int32_t sum = ae_rds16(r, AE_B_STACK_BOUNCE_FIRST + axis * 2u) +
                          ae_rds16(r, AE_B_STACK_BOUNCE_SECOND + axis * 2u);
            ae_wr16(r, AE_B_STACK_BOUNCE_SECOND + axis * 2u, sum);
        }
        (void)ae_sdk_VectorNormalSS(r, AE_B_STACK_BOUNCE_SECOND, AE_B_STACK_BOUNCE_SECOND);
        GIMMICK_B_CALL(h, PlayerAttachSet, AE_B_STACK_BOUNCE_SECOND, 30, 0, 0);
        ae_wr16(r, AE_G_PLAYER + AE_PLAYER_ATTACH_X,
                ae_rd16(r, AE_B_STACK_BOUNCE_SECOND + AE_VECTOR3_X));
        ae_wr16(r, AE_G_PLAYER + AE_PLAYER_ATTACH_Z,
                ae_rd16(r, AE_B_STACK_BOUNCE_SECOND + AE_VECTOR3_Z));
        return 0;
    }

    uint32_t angles = AE_B_STACK_BOUNCE_FIRST;
    ae_wr16(r, angles + AE_VECTOR3_X, 4096);
    ae_wr16(r, angles + AE_VECTOR3_Y, 4096);
    ae_wr16(r, angles + AE_VECTOR3_Z, 4096);
    int32_t amplitude = ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE);
    int32_t phase = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A);
    if (state == 1 || state == 2) {
        if (amplitude < 128) {
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
        } else {
            int32_t sine = ae_sdk_rsin(r, phase);
            int32_t vertical = 4096 - shift_q12_floor(wrap_mul32(amplitude, sine));
            ae_wr16(r, angles + AE_VECTOR3_Y, vertical);
            if (state == 1 || state == 2) {
                int32_t next_phase = narrow_s16(phase + 256);
                ae_wr16(r, gimmick + AE_GIMMICK_PARAM_A, next_phase);
                if (next_phase > 4096) {
                    ae_wr16(r, gimmick + AE_GIMMICK_PARAM_A, 0);
                    ae_wr16(r, gimmick + AE_GIMMICK_SUBTYPE, half_toward_zero(amplitude));
                }
            }
        }
    } else if (state == 3) {
        if (amplitude < 128) {
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
        } else {
            int32_t sine = ae_sdk_rsin(r, phase);
            int32_t scaled = shift_q12_floor(wrap_mul32(amplitude, sine));
            ae_wr16(r, angles + AE_VECTOR3_X, 4096 - scaled);
            ae_wr16(r, angles + AE_VECTOR3_Z, 4096 - scaled);
            int32_t next_phase = narrow_s16(phase +
                                            ae_rds16(r, gimmick + AE_GIMMICK_PARAM_E));
            ae_wr16(r, gimmick + AE_GIMMICK_PARAM_A, next_phase);
            int32_t phase_limit = (int32_t)((uint32_t)ae_rds16(
                r, gimmick + AE_GIMMICK_PARAM_10) << 12);
            if (next_phase >= phase_limit) {
                ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
                ae_wr16(r, gimmick + AE_GIMMICK_PARAM_A, 0);
            }
        }
    }

    if (prop_index >= 0 && (state == 1 || state == 2)) {
        uint32_t mesh = ae_rd32(r, prop + AE_AREA_PROP_MESH);
        if ((ae_rd32(r, mesh) & AE_PROP_MESH_TYPE_MASK) == AE_PROP_MESH_ROT_MATRIX_TYPE) {
            GIMMICK_B_CALL(h, SvecToDiagMatrix, angles,
                           ae_rd32(r, prop + AE_AREA_PROP_ROTATION), 0, 0);
        }
    } else if (prop_index >= 0 && state == 3) {
        ae_wr16(r, prop + AE_AREA_PROP_ROTATION_PROGRESS,
                ae_rd16(r, gimmick + AE_GIMMICK_PARAM_A));
    }
    return 0;
}

int32_t ae_Gimmick_Rotate_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    int32_t state = ae_rds16(r, gimmick + AE_GIMMICK_STATE);
    if (state == 0) {
        int32_t accepted;
        GIMMICK_B_RESULT(h, FUN_80094330, collider, AE_G_PLAYER_POS_X,
                         ae_rds16(r, gimmick + AE_GIMMICK_LINK),
                         ae_rds16(r, gimmick + AE_GIMMICK_PARAM_E), accepted);
        if (accepted == 0) return 0;

        int32_t variant = ae_rds16(r, gimmick + AE_GIMMICK_SUBTYPE);
        if (variant >= 4 && variant < 8) {
            uint32_t difference = AE_B_STACK_ROTATE_DISTANCE;
            int32_t diagonal_type = (variant & 1) != 0 ? 0xC00 : 0;
            (void)ae_DiagonalSignedDistance(r, diagonal_type,
                                            AE_G_PLAYER_POS_X, collider, difference);
            int32_t signed_distance = ae_rds32(r, difference);
            int32_t limit = ae_rds16(r, gimmick + AE_GIMMICK_LINK);
            if ((variant & 1) == 0 && ae_rd16(r, AE_G_CURRENT_AREA) == 84 &&
                limit < signed_distance) return 0;
            int32_t absolute_distance = signed_distance < 0 ? -signed_distance : signed_distance;
            if (limit < absolute_distance) return 0;
        }

        if (variant < 8) {
            int32_t rotate_x = ae_rds32(r, AE_G_PLAYER + AE_PLAYER_ROTATE_X);
            int32_t rotate_z = ae_rds32(r, AE_G_PLAYER + AE_PLAYER_ROTATE_Z);
            if (rotate_x == 0 || rotate_z == 0) rotate_x = rotate_z = 1;
            ae_wr32(r, AE_G_PLAYER + AE_PLAYER_ROTATE_X, rotate_x);
            ae_wr32(r, AE_G_PLAYER + AE_PLAYER_ROTATE_Z, rotate_z);

            int32_t vector_x;
            int32_t vector_z;
            if (variant == 0 || variant == 2) {
                vector_x = -rotate_x;
                vector_z = rotate_z;
            } else if (variant == 1 || variant == 3) {
                vector_x = rotate_x;
                vector_z = -rotate_z;
            } else if ((variant & 1) == 0) {
                vector_x = -rotate_z;
                vector_z = -rotate_x;
            } else {
                vector_x = rotate_z;
                vector_z = rotate_x;
            }
            ae_wr16(r, AE_B_STACK_ROTATE_VECTOR + AE_VECTOR3_X, vector_x);
            ae_wr16(r, AE_B_STACK_ROTATE_VECTOR + AE_VECTOR3_Y, 0);
            ae_wr16(r, AE_B_STACK_ROTATE_VECTOR + AE_VECTOR3_Z, vector_z);
            ae_wr32(r, AE_G_PLAYER + AE_PLAYER_ROTATE_X,
                    wrap_mul32(narrow_s16(vector_x), 5));
            ae_wr32(r, AE_G_PLAYER + AE_PLAYER_ROTATE_Z,
                    wrap_mul32(narrow_s16(vector_z), 5));
        }
        (void)ae_sdk_VectorNormalSS(r, AE_B_STACK_ROTATE_VECTOR, AE_B_STACK_ROTATE_VECTOR);
        ae_wr16(r, AE_G_PLAYER + AE_PLAYER_ATTACH_X,
                ae_rds16(r, AE_B_STACK_ROTATE_VECTOR + AE_VECTOR3_X));
        ae_wr16(r, AE_G_PLAYER + AE_PLAYER_ATTACH_Z,
                ae_rds16(r, AE_B_STACK_ROTATE_VECTOR + AE_VECTOR3_Z));
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);

        uint32_t area = ae_rd16(r, AE_G_CURRENT_AREA);
        if (area == 21) GIMMICK_B_CALL(h, PlaySfx, 179, 0, 0, 0);
        else if (area == 84) GIMMICK_B_CALL(h, PlaySfx, 180, 0, 0, 0);
        GIMMICK_B_CALL(h, PlayerAttachSet, AE_B_STACK_ROTATE_VECTOR,
                       ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A), 0, 0);
        return 0;
    }

    int32_t timer = ae_rds16(r, gimmick + AE_GIMMICK_TIMER);
    int32_t duration = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A);
    if (state == 1) {
        timer = narrow_s16(timer + 2);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, timer);
        if (timer >= duration) {
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 2);
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, duration - 1);
        }
    } else if (state == 2) {
        timer = narrow_s16(timer - 2);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, timer);
        if (timer < 0) {
            ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
        }
    } else {
        return 0;
    }

    int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
    if (prop_index < 0) return 0;
    int32_t numerator = (int32_t)((uint32_t)(ae_rds16(r, gimmick + AE_GIMMICK_PARAM_10) + 1) << 12);
    numerator = wrap_mul32(numerator, ae_rds16(r, gimmick + AE_GIMMICK_TIMER));
    int32_t divisor = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A);
    if (divisor == 0) {
        ae_ram_fail(r, "Gimmick_Rotate_Update division by zero");
        return 0;
    }
    if (numerator == INT32_MIN && divisor == -1) {
        ae_ram_fail(r, "Gimmick_Rotate_Update division overflow");
        return 0;
    }
    int32_t fraction = divide_toward_zero(numerator, divisor);
    uint32_t prop = prop_record(ae_rd32(r, AE_G_AREA_PROPS), prop_index);
    ae_wr16(r, prop + AE_AREA_PROP_ROTATION_PROGRESS, fraction);
    return 0;
}

int32_t ae_Gimmick_Falling_Update(AeRam *r, const AeGimmickBHooks *h, uint32_t gimmick) {
    uint32_t collider = collider_for_gimmick(r, gimmick);
    int32_t state = ae_rds16(r, gimmick + AE_GIMMICK_STATE);

    if (state == 0) {
        int32_t probe;
        GIMMICK_B_RESULT(h, FUN_80094A48,
                         ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A), 0, 0, 0, probe);
        if (ae_rds16(r, gimmick + AE_GIMMICK_TIMER) == 0 &&
            ae_rds16(r, AE_G_PLAYER_WALL_STATE) != ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER)) {
            int32_t collision;
            GIMMICK_B_RESULT(h, FUN_80094E5C, collider, 0, 0, 0, collision);
            if (collision == 0 && probe == 0) return 0;
        }
        if (ae_rds16(r, gimmick + AE_GIMMICK_TIMER) != 0 &&
            ae_rds16(r, AE_G_PLAYER_WALL_STATE) != ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER) &&
            probe == 0) {
            ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
            return 0;
        }

        uint32_t flags = ae_rd16(r, collider + AE_COLLIDER_FLAGS);
        ae_wr16(r, collider + AE_COLLIDER_FLAGS,
                (flags & ~AE_COLLIDER_STATE_MASK) | AE_COLLIDER_STATE_2);
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 1);
        ae_wr16(r, gimmick + AE_GIMMICK_STATE, 1);
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_E, ae_rd16(r, collider + AE_COLLIDER_POS_X));
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_10, ae_rd16(r, collider + AE_COLLIDER_POS_Y));
        ae_wr16(r, gimmick + AE_GIMMICK_PARAM_12, ae_rd16(r, collider + AE_COLLIDER_POS_Z));
        play_sfx_at(r, h, 156, collider);
        return 0;
    }

    if (state != 1) return 0;
    int32_t target = ae_rds16(r, gimmick + AE_GIMMICK_LINK);
    (void)ae_GimmickMoveColliderTo(r, h, ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A),
                                   collider, gimmick + AE_GIMMICK_PARAM_E);
    if (ae_ram_failed(r)) return 0;
    if (ae_rds16(r, collider + AE_COLLIDER_BREAKABLE_COUNT) < target * 2 + 1) return 0;

    uint32_t flags = ae_rd16(r, collider + AE_COLLIDER_FLAGS);
    ae_wr16(r, collider + AE_COLLIDER_FLAGS,
            (flags & ~AE_COLLIDER_STATE_MASK) | 1u);
    play_sfx_at(r, h, 156, collider);
    ae_wr16(r, gimmick + AE_GIMMICK_STATE, 0);
    int32_t index = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A);
    int32_t probe = 0;
    if (index >= 0)
        GIMMICK_B_RESULT(h, FUN_80094A48, index, 0, 0, 0, probe);
    if (ae_rds16(r, AE_G_PLAYER_WALL_STATE) != ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER) &&
        probe == 0)
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 0);
    else
        ae_wr16(r, gimmick + AE_GIMMICK_TIMER, 1);
    return 0;
}

int32_t ae_GimmickMoveColliderTo(AeRam *r, const AeGimmickBHooks *h, int32_t index_arg,
                                 uint32_t source, uint32_t destination) {
    int32_t index = narrow_s16(index_arg);
    if (index >= 0) {
        int32_t dx = narrow_s16(ae_rd16(r, source + AE_VECTOR3_X) -
                                ae_rd16(r, destination + AE_VECTOR3_X));
        int32_t dy = narrow_s16(ae_rd16(r, source + AE_VECTOR3_Y) -
                                ae_rd16(r, destination + AE_VECTOR3_Y));
        int32_t dz = narrow_s16(ae_rd16(r, source + AE_VECTOR3_Z) -
                                ae_rd16(r, destination + AE_VECTOR3_Z));
        (void)ae_GimmickMoveCollider(r, h, index, dx, dy, dz);
        ae_wr16(r, destination + AE_VECTOR3_X, ae_rd16(r, source + AE_VECTOR3_X));
        ae_wr16(r, destination + AE_VECTOR3_Y, ae_rd16(r, source + AE_VECTOR3_Y));
        ae_wr16(r, destination + AE_VECTOR3_Z, ae_rd16(r, source + AE_VECTOR3_Z));
    }
    return 0;
}

int32_t ae_GimmickMoveCollider(AeRam *r, const AeGimmickBHooks *h, int32_t index_arg,
                               int32_t dx_arg, int32_t dy_arg, int32_t dz_arg) {
    int32_t index = narrow_s16(index_arg);
    if (index < 0) return 0;
    int32_t dx = narrow_s16(dx_arg);
    int32_t dy = narrow_s16(dy_arg);
    int32_t dz = narrow_s16(dz_arg);
    uint32_t gimmick = gimmick_record(ae_rd32(r, AE_G_GIMMICKS), index);
    int32_t collider_index = ae_rds16(r, gimmick + AE_GIMMICK_COLLIDER);
    uint32_t collider = collider_for_gimmick(r, gimmick);
    int32_t player_x = ae_rds32(r, AE_G_PLAYER_POS_X);
    int32_t player_y = ae_rds32(r, AE_G_PLAYER_POS_Y);
    int32_t player_z = ae_rds32(r, AE_G_PLAYER_POS_Z);
    int32_t inside;
    GIMMICK_B_RESULT(h, ColliderBoxContainsPoint, collider, player_x, player_y, player_z, inside);
    if (inside != 0) {
        ae_wr32(r, AE_G_PLAYER_POS_X, player_x + dx);
        ae_wr32(r, AE_G_PLAYER_POS_Y, player_y + dy);
        ae_wr32(r, AE_G_PLAYER_POS_Z, player_z + dz);
        GIMMICK_B_CALL(h, CameraNudge, dx, dy, dz, 0);
    }

    uint32_t rc_car = ae_rd32(r, AE_G_RC_CAR_ACTOR);
    if (rc_car != 0) {
        int32_t car_x = ae_rds32(r, AE_G_RC_CAR + AE_RC_CAR_POSITION_X);
        int32_t car_y = ae_rds32(r, AE_G_RC_CAR + AE_RC_CAR_POSITION_Y);
        int32_t car_z = ae_rds32(r, AE_G_RC_CAR + AE_RC_CAR_POSITION_Z);
        GIMMICK_B_RESULT(h, ColliderBoxContainsPoint, collider, car_x, car_y, car_z, inside);
        if (inside != 0) {
            ae_wr32(r, AE_G_RC_CAR + AE_RC_CAR_POSITION_X, car_x + dx);
            ae_wr32(r, AE_G_RC_CAR + AE_RC_CAR_POSITION_Y, car_y + dy);
            ae_wr32(r, AE_G_RC_CAR + AE_RC_CAR_POSITION_Z, car_z + dz);
        }
    }

    ae_wr16(r, collider + AE_COLLIDER_POS_X, ae_rds16(r, collider + AE_COLLIDER_POS_X) + dx);
    ae_wr16(r, collider + AE_COLLIDER_POS_Y, ae_rds16(r, collider + AE_COLLIDER_POS_Y) + dy);
    ae_wr16(r, collider + AE_COLLIDER_POS_Z, ae_rds16(r, collider + AE_COLLIDER_POS_Z) + dz);
    int32_t prop_index = ae_rds16(r, collider + AE_COLLIDER_PROP_INDEX);
    if (prop_index >= 0) {
        uint32_t prop = prop_record(ae_rd32(r, AE_G_AREA_PROPS), prop_index);
        ae_wr16(r, prop + AE_AREA_PROP_POSITION_X,
                ae_rds16(r, prop + AE_AREA_PROP_POSITION_X) + dx);
        ae_wr16(r, prop + AE_AREA_PROP_POSITION_Y,
                ae_rds16(r, prop + AE_AREA_PROP_POSITION_Y) + dy);
        ae_wr16(r, prop + AE_AREA_PROP_POSITION_Z,
                ae_rds16(r, prop + AE_AREA_PROP_POSITION_Z) + dz);
    }

    if (ae_rds16(r, gimmick + AE_GIMMICK_TYPE) == 11) {
        int32_t linked = ae_rds16(r, gimmick + AE_GIMMICK_PARAM_A);
        if (linked >= 0) (void)ae_GimmickMoveCollider(r, h, linked, dx, dy, dz);
    }
    (void)collider_index;
    return 0;
}

