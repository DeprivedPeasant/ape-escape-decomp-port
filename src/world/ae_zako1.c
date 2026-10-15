#include "ae_zako1.h"

#include <stdint.h>

#define AE_ZAKO1_STACK_BASE       0x801FFE00u
#define AE_ZAKO1_STACK_ObjSfx     (AE_ZAKO1_STACK_BASE + 0x00u)
#define AE_ZAKO1_STACK_Puff       (AE_ZAKO1_STACK_BASE + 0x10u)
#define AE_ZAKO1_STACK_Defeat     (AE_ZAKO1_STACK_BASE + 0x20u)
#define AE_ZAKO1_STACK_Drop       (AE_ZAKO1_STACK_BASE + 0x30u)
#define AE_ZAKO1_STACK_Knockback  (AE_ZAKO1_STACK_BASE + 0x40u)
#define AE_ZAKO1_STACK_Point      (AE_ZAKO1_STACK_BASE + 0x60u)
#define AE_ZAKO1_STACK_MatrixAngles (AE_ZAKO1_STACK_BASE + 0x80u)
#define AE_ZAKO1_STACK_MatrixScale  (AE_ZAKO1_STACK_BASE + 0x90u)
#define AE_ZAKO1_STACK_MatrixShrink (AE_ZAKO1_STACK_BASE + 0xA0u)

static int32_t signed16(uint32_t bits) {
    return (int16_t)(bits & 0xFFFFu);
}

static int32_t signed32(uint32_t bits) {
    return (int32_t)bits;
}

static int32_t mulhi_signed(int32_t left, int32_t right) {
    int64_t product = (int64_t)left * right;
    return (int32_t)(product >> 32);
}

static int32_t call_hook(AeRam *r, const AeZako1Hooks *h, AeZako1HookFn fn,
                         const char *name, uint32_t a0, uint32_t a1,
                         uint32_t a2, uint32_t a3) {
    if (h == NULL || fn == NULL) {
        ae_ram_fail(r, name);
        return 0;
    }
    int32_t result = fn(h->user, r, a0, a1, a2, a3);
    return ae_ram_failed(r) ? 0 : result;
}

static int32_t call_dispatch(AeRam *r, const AeZako1Hooks *h,
                             AeZako1DispatchFn fn, const char *name,
                             uint32_t target, uint32_t a0, uint32_t a1,
                             uint32_t a2, uint32_t a3) {
    if (h == NULL || fn == NULL) {
        ae_ram_fail(r, name);
        return 0;
    }
    int32_t result = fn(h->user, r, target, a0, a1, a2, a3);
    return ae_ram_failed(r) ? 0 : result;
}

static int32_t add_signed32(int32_t left, int32_t right) {
    return signed32((uint32_t)left + (uint32_t)right);
}

static int32_t floor_shift(int32_t value, unsigned shift) {
    if (shift == 0) return value;
    uint32_t shifted = (uint32_t)value >> shift;
    if (value < 0) shifted |= UINT32_MAX << (32u - shift);
    return signed32(shifted);
}

static int32_t signed_divide(AeRam *r, int32_t numerator, int32_t denominator,
                             const char *name) {
    if (denominator == 0 || (numerator == INT32_MIN && denominator == -1)) {
        ae_ram_fail(r, name);
        return 0;
    }
    return numerator / denominator;
}

int32_t ae_ObjApplyDamage(AeRam *r, uint32_t obj, uint32_t attacker_body) {
    if (ae_rds8(r, AE_G_ZAKO_NO_HIT_DAMAGE) == 0) return 0;

    uint32_t damage = ae_rd16(r, attacker_body + AE_ZAKO_HIT_BODY_DAMAGE) & 0x0Fu;
    int32_t hp = (int32_t)ae_rd8(r, obj + AE_OBJ_HP) - (int32_t)damage;
    if (hp > 0) {
        ae_wr8(r, obj + AE_OBJ_HP, (uint32_t)hp);
        return 0;
    }
    ae_wr8(r, obj + AE_OBJ_HP, 0);
    return 1;
}

int32_t ae_ObjIsOwnerType0B(AeRam *r, uint32_t obj) {
    return (ae_rd32(r, obj + AE_OBJ_TARGET_BODY) >> 28) == 0xBu;
}

void ae_ObjAnimClampLastFrame(AeRam *r, uint32_t obj) {
    uint32_t slot = ae_rd8(r, obj + AE_OBJ_ANIM_SLOT);
    uint32_t entry = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE) +
                     slot * AE_ANIM_CLIP_RECORD_SIZE;
    uint32_t last_frame = ae_rd16(r, entry + AE_ANIM_CLIP_LAST_FRAME);
    if (ae_rd16(r, obj + AE_OBJ_ANIM_FRAME) == last_frame)
        ae_wr16(r, obj + AE_OBJ_ANIM_FRAME, last_frame - 1u);
}

int32_t ae_ObjBounceOnGround(AeRam *r, uint32_t obj, int32_t ground_y) {
    int32_t vertical_velocity = (int16_t)ae_rd16(r, obj + AE_OBJ_VELOCITY_Y);
    int32_t min_velocity = vertical_velocity < 64 ? 64 : (int16_t)ae_rd16(r, obj + AE_OBJ_VELOCITY_Y);
    int32_t low_y = signed16((uint32_t)ground_y) + min_velocity;
    int32_t obj_y = signed32(ae_rd32(r, obj + AE_OBJ_POS_Y));
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);

    if (low_y < obj_y && (flags & 0x20u) == 0) {
        int32_t velocity_x = (int16_t)ae_rd16(r, obj + AE_OBJ_VELOCITY_X);
        int32_t velocity_z = (int16_t)ae_rd16(r, obj + AE_OBJ_VELOCITY_Z);
        ae_wr16(r, obj + AE_OBJ_VELOCITY_X, (uint32_t)(0u - (uint32_t)velocity_x));
        ae_wr32(r, obj + AE_OBJ_FLAGS, flags | 0x20u);
        ae_wr16(r, obj + AE_OBJ_VELOCITY_Z, (uint32_t)(0u - (uint32_t)velocity_z));
        return 0;
    }

    int32_t high_y = signed32(ae_rd32(r, obj + AE_OBJ_POS_Y));
    int32_t input_ground_y = signed16((uint32_t)ground_y);
    if (high_y < input_ground_y) return 0;

    ae_wr32(r, obj + AE_OBJ_POS_Y, (uint32_t)input_ground_y);
    if ((flags & 0x400u) != 0) return 1;

    vertical_velocity = (int16_t)ae_rd16(r, obj + AE_OBJ_VELOCITY_Y);
    int32_t reversed = signed32(0u - (uint32_t)vertical_velocity);
    uint32_t half_negative_bits = (uint32_t)reversed;
    int32_t half_negative = signed32(half_negative_bits + (half_negative_bits >> 31)) >> 1;
    int32_t velocity_x = floor_shift((int16_t)ae_rd16(r, obj + AE_OBJ_VELOCITY_X), 1);
    int32_t velocity_z = floor_shift((int16_t)ae_rd16(r, obj + AE_OBJ_VELOCITY_Z), 1);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Y, (uint32_t)half_negative);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_X, (uint32_t)velocity_x);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Z, (uint32_t)velocity_z);
    ae_wr32(r, obj + AE_OBJ_FLAGS, flags | 0x400u);
    return 0;
}

int32_t ae_ObjKnockbackDecay(AeRam *r, uint32_t obj) {
    int32_t strength = signed16(ae_rd16(r, obj + AE_OBJ_KNOCKBACK));
    int32_t scale = floor_shift(strength, 3);
    int32_t facing_x = (int16_t)ae_rd16(r, obj + AE_OBJ_FACING_X);
    int32_t facing_z = (int16_t)ae_rd16(r, obj + AE_OBJ_FACING_Z);
    int32_t velocity_x = floor_shift(signed32((uint32_t)(-facing_x) * (uint32_t)scale), 12);
    int32_t velocity_z = floor_shift(signed32((uint32_t)(-facing_z) * (uint32_t)scale), 12);
    int32_t decay = ae_rd8(r, obj + AE_OBJ_KNOCK_DECAY);

    strength = signed16((uint32_t)strength - (uint32_t)decay);
    ae_wr16(r, obj + AE_OBJ_KNOCKBACK, (uint32_t)strength);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_X, (uint32_t)velocity_x);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Z, (uint32_t)velocity_z);
    return strength < 11;
}

void ae_ObjSyncBodyHitFlags(AeRam *r, uint32_t obj) {
    uint32_t owner_type = ae_rd32(r, obj + AE_OBJ_TARGET_BODY) >> 28;
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
    if (owner_type == 0xBu) {
        ae_wr16(r, obj + AE_OBJ_HIT_REACT,
                (flags & 0x00028000u) == 0x00008000u ? 2u : 0u);
        if ((flags & 0x00020000u) != 0)
            ae_wr32(r, obj + AE_OBJ_TARGET_BODY, 0);
        ae_wr32(r, obj + AE_OBJ_FLAGS, flags | 0x00020000u);
        return;
    }
    ae_wr16(r, obj + AE_OBJ_HIT_REACT, 0);
    ae_wr32(r, obj + AE_OBJ_FLAGS, flags & ~0x00020000u);
}

int32_t ae_ObjGroundHeightCached(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                                 int32_t dx, int32_t dy, int32_t dz) {
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
    if ((flags & 0x100u) == 0) {
        int32_t x = add_signed32(signed32(ae_rd32(r, obj + AE_OBJ_POS_X)), (int16_t)dx);
        int32_t y = add_signed32(signed32(ae_rd32(r, obj + AE_OBJ_POS_Y)), (int16_t)dy);
        int32_t z = add_signed32(signed32(ae_rd32(r, obj + AE_OBJ_POS_Z)), (int16_t)dz);

        int32_t ground = ae_GetGroundHeight(r, h ? h->world_hooks : NULL, x, y, z);
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, obj + AE_OBJ_CACHED_GROUND_Y, (uint32_t)ground);
        ae_wr32(r, obj + AE_OBJ_GROUND_HEIGHT, ae_rd32(r, AE_GROUND_QUERY_RESULT));
        ae_wr32(r, obj + AE_OBJ_FLAGS, flags | 0x100u);
    }
    return (int16_t)ae_rd16(r, obj + AE_OBJ_CACHED_GROUND_Y);
}

int32_t ae_ObjApplyVelocity(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    int32_t vx = (int16_t)ae_rd16(r, obj + AE_OBJ_VELOCITY_X);
    int32_t vy = (int16_t)ae_rd16(r, obj + AE_OBJ_VELOCITY_Y);
    int32_t vz = (int16_t)ae_rd16(r, obj + AE_OBJ_VELOCITY_Z);
    ae_wr32(r, obj + AE_OBJ_POS_X, ae_rd32(r, obj + AE_OBJ_POS_X) + (uint32_t)vx);
    ae_wr32(r, obj + AE_OBJ_POS_Y, ae_rd32(r, obj + AE_OBJ_POS_Y) + (uint32_t)vy);
    ae_wr32(r, obj + AE_OBJ_POS_Z, ae_rd32(r, obj + AE_OBJ_POS_Z) + (uint32_t)vz);
    ae_wr16(r, AE_G_ZAKO_GROUND_QUERY_MODE, 1u);
    return ae_ObjGroundHeightCached(r, h, obj, 0, 0, 0);
}

void ae_ObjTurnTowardsXZ(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                         int32_t dx, int32_t dz, int32_t offset, int32_t steps) {
    /* The generic hook slot has four ABI words, but Atan2Safe consumes only
     * dx and dz; do not forward the caller's offset or turn-step values. */
    int32_t target = call_hook(r, h, h ? h->Atan2Safe : NULL, "Atan2Safe hook not provided",
                               (uint32_t)(int32_t)(int16_t)dx,
                               (uint32_t)(int32_t)(int16_t)dz,
                               0, 0);
    if (ae_ram_failed(r)) return;

    int32_t sum = add_signed32(target, (int16_t)offset);
    int32_t unrounded = add_signed32(sum, 8192);
    int32_t rounded_for_quotient = unrounded < 0
        ? add_signed32(sum, 12287) : unrounded;
    int32_t quotient = floor_shift(rounded_for_quotient, 12);
    int32_t snapped = signed32((uint32_t)unrounded -
                               ((uint32_t)quotient << 12));
    int32_t difference = add_signed32(snapped,
                                     -(int32_t)(int16_t)ae_rd16(r, obj + AE_OBJ_HEADING));
    if (difference == 0) return;

    int32_t direction = difference > 0 ? 1 : -1;
    int32_t magnitude = difference < 0 ? -difference : difference;
    if (magnitude > 2048) {
        magnitude = 4096 - magnitude;
        direction = -direction;
    }

    int32_t max_step = (int16_t)steps;
    int32_t denominator = magnitude < max_step ? magnitude : max_step;
    int32_t turn_steps = signed_divide(r, magnitude, denominator,
                                       "ObjTurnTowardsXZ division trap");
    if (ae_ram_failed(r)) return;
    int32_t turn_speed = signed32((uint32_t)direction * (uint32_t)denominator);
    ae_wr16(r, obj + AE_OBJ_TURN_SPEED, (uint32_t)turn_speed);
    ae_wr16(r, obj + AE_OBJ_TIMER, (uint32_t)turn_steps);
}

void ae_ZakoSetKnockback(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                         int32_t direction, int32_t strength, int32_t angle) {
    uint32_t motion = ae_rd8(r, obj + AE_OBJ_MOTION_STATE);
    if (motion == 4u) {
        int32_t signed_angle = (int16_t)angle;
        int32_t signed_strength = (int16_t)strength;
        int32_t sine = ae_sdk_rsin(r, signed_angle);
        ae_wr16(r, obj + AE_OBJ_VELOCITY_X, 0);
        int32_t sine_product = signed32((uint32_t)sine * (uint32_t)signed_strength);
        int32_t vertical_velocity = floor_shift(signed32((uint32_t)sine_product << 1), 12);
        ae_wr16(r, obj + AE_OBJ_VELOCITY_Y,
                (uint32_t)(0u - (uint32_t)vertical_velocity));
        int32_t cosine = ae_sdk_rcos(r, signed_angle);
        int32_t cosine_product = signed32((uint32_t)cosine * (uint32_t)signed_strength);
        int32_t forward_velocity = floor_shift(signed32((uint32_t)cosine_product << 1), 12);
        ae_wr16(r, obj + AE_OBJ_VELOCITY_Z,
                (uint32_t)(0u - (uint32_t)forward_velocity));
    } else if (motion == 5u) {
        ae_wr16(r, obj + AE_OBJ_VELOCITY_Z, (uint32_t)(0u - (uint32_t)strength));
        ae_wr16(r, obj + AE_OBJ_VELOCITY_Y, 0);
        ae_wr16(r, obj + AE_OBJ_VELOCITY_X, 0);
        ae_wr8(r, obj + AE_OBJ_KNOCK_DECAY, (uint32_t)angle);
        ae_wr16(r, obj + AE_OBJ_KNOCKBACK, (uint32_t)((uint32_t)strength << 3));
    }

    uint32_t matrix = AE_ZAKO1_STACK_Knockback;
    (void)ae_sdk_MatrixFromDirXZ(r, (uint32_t)direction, matrix);
    (void)ae_sdk_ApplyMatrixSV(r, matrix, obj + AE_OBJ_VELOCITY_X,
                               obj + AE_OBJ_VELOCITY_X);
    if (ae_ram_failed(r)) return;

    int32_t player_dx = (int16_t)ae_rd16(r, AE_G_PLAYER_OBJECT + AE_OBJ_POS_X) -
                        (int16_t)ae_rd16(r, obj + AE_OBJ_POS_X);
    int32_t player_dz = (int16_t)ae_rd16(r, AE_G_PLAYER_OBJECT + AE_OBJ_POS_Z) -
                        (int16_t)ae_rd16(r, obj + AE_OBJ_POS_Z);
    ae_ObjTurnTowardsXZ(r, h, obj, player_dx, player_dz, 0, 0x1000);
    if (ae_ram_failed(r)) return;

    ae_wr8(r, obj + AE_OBJ_FLASH_TIMER, 20u);
    ae_wr8(r, obj + AE_OBJ_MOTION_TIMER, 0);
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
    flags &= ~(0x400u | 0x20u | 0x00100000u);
    ae_wr32(r, obj + AE_OBJ_FLAGS, flags);
}

static int32_t scale_hit_value(int32_t value) {
    int32_t high = mulhi_signed(value, (int32_t)0x66666667u);
    return floor_shift(high, 2) - (value >> 31);
}

static int32_t scaled_knockback(int32_t value) {
    const int32_t reciprocal = (int32_t)0xB60B60B7u;
    int32_t high = mulhi_signed(value, reciprocal);
    return floor_shift(add_signed32(high, value), 8) - floor_shift(value, 31);
}

void ae_ZakoApplyHit(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                     uint32_t hitter_body) {
    int32_t mode = ae_rds8(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_HIT_MODE);
    int32_t strength = 35;
    int32_t angle = 20;
    uint32_t motion;

    if (mode == 1) {
        motion = 4;
        ae_wr8(r, obj + AE_OBJ_MOTION_STATE, motion);
        strength = ae_rds8(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_HIT_STRENGTH);
        int32_t raw_angle = (int32_t)ae_rds8(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_HIT_ANGLE) * 4096;
        angle = scaled_knockback(raw_angle);
    } else if (mode == 2) {
        motion = 5;
        ae_wr8(r, obj + AE_OBJ_MOTION_STATE, motion);
        strength = ae_rds8(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_HIT_STRENGTH);
        angle = ae_rds8(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_HIT_ANGLE);
    } else {
        motion = ae_rd8(r, obj + AE_OBJ_HIT_COUNT);
        if (motion == 0) {
            uint32_t hit_kind = ae_rd8(r, hitter_body + AE_ZAKO_HIT_BODY_KIND);
            ae_wr8(r, obj + AE_OBJ_LAST_HIT_TYPE, hit_kind);
            uint32_t hit_bits = ae_rd16(r, hitter_body + AE_ZAKO_HIT_BODY_KIND) & 0x3F00u;
            motion = hit_bits == 0x0200u ? 4u : 5u;
        }
        ae_wr8(r, obj + AE_OBJ_MOTION_STATE, motion);

        if (motion == 4u) {
            uint32_t type = ae_rd8(r, obj + AE_OBJ_TYPE);
            uint32_t record = AE_G_ZAKO_MOTION_TABLE +
                              type * AE_ZAKO_MOTION_RECORD_SIZE;
            int32_t raw_angle = (int32_t)ae_rds8(r, record + AE_ZAKO_MOTION_ANGLE) * 4096;
            strength = ae_rds8(r, record + AE_ZAKO_MOTION_STRENGTH);
            angle = scaled_knockback(raw_angle);
        }
    }

    if (motion != 4u) {
        int32_t hit_type = (int8_t)ae_rd8(r, obj + AE_OBJ_DEFENSE);
        if (hit_type == 10) {
            strength = 0;
        } else {
            int32_t defense_scale = ae_rds8(r, AE_G_ZAKO_DEFENSE_SCALE);
            int32_t multiplier = 10 - hit_type - defense_scale;
            strength = scale_hit_value(signed32((uint32_t)strength *
                                                 (uint32_t)multiplier));
        }
    }

    ae_ZakoSetKnockback(r, h, obj,
                        hitter_body + AE_ZAKO_HIT_BODY_DIRECTION,
                        (int16_t)strength, (int16_t)angle);
}

void ae_ZakoOnHitByGadget(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    uint32_t index = ae_rd16(r, obj + AE_OBJ_TARGET_BODY) & 0x0FFFu;
    uint32_t hitter = ae_GetObjectByIndex(r, index);
    ae_ZakoApplyHit(r, h, obj, hitter);
    if (ae_ram_failed(r)) return;
    if (ae_ObjApplyDamage(r, obj, hitter) == 0) return;
    ae_ObjUnregisterBody(r, h ? h->init_hooks : NULL, obj);
}

uint32_t ae_ZakoAllocSlot(AeRam *r) {
    int32_t count = ae_rds16(r, AE_G_ZAKO_SPAWN_COUNT);
    if (count <= 0) return 0;
    for (int32_t i = 0; i < count; ++i) {
        uint32_t obj = AE_G_ZAKO_OBJECTS + (uint32_t)i * AE_ZAKO_OBJECT_SIZE;
        if ((ae_rd32(r, obj + AE_OBJ_FLAGS) & AE_OBJ_FLAG_SPAWNED) == 0)
            return obj;
    }
    return 0;
}

void ae_ZakoSpawnNearby(AeRam *r, const AeZako1Hooks *h) {
    uint32_t state = AE_G_ZAKO_STATE;
    uint32_t entries = ae_rd32(r, state + AE_ZAKO_STATE_ENTRY_TABLE);
    int32_t count = ae_rds16(r, state + AE_ZAKO_STATE_ENTRY_COUNT);
    int32_t player_x = (int16_t)ae_rd16(r, AE_G_PLAYER_OBJECT + AE_OBJ_POS_X);
    int32_t player_z = (int16_t)ae_rd16(r, AE_G_PLAYER_OBJECT + AE_OBJ_POS_Z);

    if (count <= 0) return;
    for (int32_t index = 0; index < count; ++index) {
        uint32_t entry = entries + (uint32_t)index * AE_ZAKO_ENTRY_RECORD_SIZE;
        if (ae_rd8(r, entry + AE_ZAKO_ENTRY_ACTIVE) != 0 ||
            ae_rd16(r, entry + AE_ZAKO_ENTRY_REMAINING) == 0)
            continue;

        int32_t dx = (int16_t)ae_rd16(r, entry + AE_ZAKO_ENTRY_X) - player_x;
        int32_t dz = (int16_t)ae_rd16(r, entry + AE_ZAKO_ENTRY_Z) - player_z;
        uint32_t x_squared = (uint32_t)dx * (uint32_t)dx;
        uint32_t z_squared = (uint32_t)dz * (uint32_t)dz;
        uint32_t radius = ae_rd16(r, entry + AE_ZAKO_ENTRY_SPAWN_RADIUS);
        uint32_t radius_squared = radius * radius;
        int32_t distance_squared = signed32(x_squared + z_squared);
        if (signed32(radius_squared) >= distance_squared) continue;

        uint32_t obj = ae_ZakoAllocSlot(r);
        if (obj == 0) continue;
        uint32_t remaining = ae_rd16(r, entry + AE_ZAKO_ENTRY_REMAINING);
        ae_wr8(r, entry + AE_ZAKO_ENTRY_ACTIVE, 1u);
        ae_wr16(r, entry + AE_ZAKO_ENTRY_REMAINING, remaining - 1u);
        ae_ZakoSpawnFromEntry(r, h ? h->init_hooks : NULL, obj, entry, index);
        if (ae_ram_failed(r)) return;
    }
}

void ae_ZakoDefeat(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    uint32_t position = AE_ZAKO1_STACK_Defeat;
    (void)call_hook(r, h, h ? h->FUN_800A3740 : NULL, "FUN_800A3740 hook not provided",
                    position, 0, 8, 0);
    if (ae_ram_failed(r)) return;
    ae_wr16(r, position, ae_rd16(r, obj + AE_OBJ_POS_X));
    ae_wr16(r, position + AE_ZAKO_SVECTOR_Y, ae_rd16(r, obj + AE_OBJ_POS_Y));
    ae_wr16(r, position + AE_ZAKO_SVECTOR_Z, ae_rd16(r, obj + AE_OBJ_POS_Z));

    ae_ObjKill(r, h ? h->init_hooks : NULL, obj, 1);
    if (ae_ram_failed(r)) return;
    if ((ae_rd8(r, obj + AE_OBJ_HIT_EFFECT_COUNT) & 3u) == 2u) {
        ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 0x102, position);
        if (ae_ram_failed(r)) return;
        ae_ObjPlaySfx(r, h, obj, 0x6E);
    } else {
        ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 0x106, position);
    }
    if (ae_ram_failed(r)) return;
    ae_ZakoDropPickups(r, h, obj);
}

void ae_ZakoDropPickups(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    uint32_t position = AE_ZAKO1_STACK_Drop;
    (void)call_hook(r, h, h ? h->FUN_800A3740 : NULL, "FUN_800A3740 hook not provided",
                    position, 0, 8, 0);
    if (ae_ram_failed(r)) return;
    ae_wr16(r, position, ae_rd16(r, obj + AE_OBJ_POS_X));
    ae_wr16(r, position + AE_ZAKO_SVECTOR_Y,
            (uint32_t)ae_rd16(r, obj + AE_OBJ_POS_Y) - 50u);
    ae_wr16(r, position + AE_ZAKO_SVECTOR_Z, ae_rd16(r, obj + AE_OBJ_POS_Z));

    uint32_t table_index = ae_rd8(r, obj + AE_OBJ_DROP_TABLE);
    if (table_index == 99u) return;
    for (uint32_t i = 0; i < AE_ZAKO_DROP_TABLE_COUNT; ++i) {
        int32_t kind = ae_rds8(r, AE_G_ZAKO_DROP_TABLE +
                               table_index * AE_ZAKO_DROP_TABLE_COUNT + i);
        if (kind == -1) return;
        if (kind == 0) {
            (void)call_hook(r, h, h ? h->SpawnPickups : NULL,
                            "SpawnPickups hook not provided", 3u, position, 3u, 1u);
        } else {
            (void)call_hook(r, h, h ? h->SpawnPickups : NULL,
                            "SpawnPickups hook not provided", (uint32_t)kind,
                            position, 3u, 0u);
            if (kind == 3)
                (void)call_hook(r, h, h ? h->SpawnPickups : NULL,
                                "SpawnPickups hook not provided", 3u, position, 3u, 0u);
        }
        if (ae_ram_failed(r)) return;
    }
}

void ae_ZakoHooksRun(AeRam *r, const AeZako1Hooks *h) {
    uint32_t count = ae_rd8(r, AE_G_ZAKO_HOOK_COUNT);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t callback = ae_rd32(r, AE_G_ZAKO_HOOKS +
                                    i * AE_ZAKO_HOOK_POINTER_SIZE);
        (void)call_dispatch(r, h, h ? h->ZakoFrameHook : NULL,
                            "ZakoFrameHook hook not provided", callback,
                            0, 0, 0, 0);
        if (ae_ram_failed(r)) return;
    }
}

void ae_ZakoUpdateLoop(AeRam *r, const AeZako1Hooks *h) {
    uint32_t mode = ae_rd8(r, AE_G_ZAKO_MODE_KIND);
    uint32_t mode_update = ae_rd32(r, AE_G_ZAKO_MODE_HANDLER_TABLE + mode * 4u);
    (void)call_dispatch(r, h, h ? h->AreaModeUpdate : NULL,
                        "AreaModeUpdate hook not provided", mode_update,
                        0, 0, 0, 0);
    if (ae_ram_failed(r)) return;

    int32_t count = ae_rds16(r, AE_G_ZAKO_SPAWN_COUNT);
    for (int32_t index = 0; index < count; ++index) {
        uint32_t obj = AE_G_ZAKO_OBJECTS + (uint32_t)index * AE_ZAKO_OBJECT_SIZE;
        uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
        if ((flags & AE_OBJ_FLAG_SPAWNED) == 0) continue;

        uint32_t collider_index = ae_rd16(r, obj + AE_OBJ_OWN_COLLIDER);
        if (collider_index != 0xFFFFu)
            ae_wr16(r, AE_G_ZAKO_CURRENT_COLLIDER, collider_index);

        flags &= ~AE_ZAKO1_OBJ_FLAG_UNK_8;
        ae_wr32(r, obj + AE_OBJ_FLAGS, flags);

        uint32_t slot = ae_rd8(r, obj + AE_OBJ_ANIM_SLOT);
        uint32_t clip = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE) +
                        slot * AE_ANIM_CLIP_RECORD_SIZE;
        uint32_t frame = (ae_rd16(r, obj + AE_OBJ_ANIM_FRAME) + 1u) & 0xFFFFu;
        ae_wr16(r, obj + AE_OBJ_ANIM_FRAME, frame);
        if (ae_rd16(r, clip + AE_ANIM_CLIP_LAST_FRAME) < frame)
            ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
                    ae_rd16(r, clip + AE_ANIM_CLIP_FIRST_FRAME));

        int32_t turn_timer = (int16_t)ae_rd16(r, obj + AE_OBJ_TIMER);
        if (turn_timer != 0) {
            ae_wr16(r, obj + AE_OBJ_TIMER, (uint32_t)(turn_timer - 1));
            int32_t angle = (int16_t)ae_rd16(r, obj + AE_OBJ_HEADING);
            int32_t turn_speed = (int16_t)ae_rd16(r, obj + AE_OBJ_TURN_SPEED);
            int32_t unwrapped = add_signed32(add_signed32(angle, turn_speed), 4096);
            int32_t rounded = unwrapped < 0 ? add_signed32(unwrapped, 4095) : unwrapped;
            int32_t turns = floor_shift(rounded, 12);
            int32_t facing_angle = signed32((uint32_t)unwrapped -
                                              ((uint32_t)turns << 12));
            ae_wr16(r, obj + AE_OBJ_HEADING, (uint32_t)facing_angle);
            ae_wr16(r, obj + AE_OBJ_FACING_X,
                    (uint32_t)ae_sdk_rcos(r, facing_angle));
            ae_wr16(r, obj + AE_OBJ_FACING_Z,
                    (uint32_t)ae_sdk_rsin(r, facing_angle));
            if (ae_ram_failed(r)) return;
        }

        ae_ObjSyncBodyHitFlags(r, obj);
        int32_t update_result = ae_ObjUpdateMotionState(r, h, obj);
        if (ae_ram_failed(r)) return;
        if (update_result == 2) {
            ae_ObjResetMotion(r, h, obj);
            if (ae_ram_failed(r)) return;
            continue;
        }

        flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
        if ((flags & AE_OBJ_FLAG_AREA_MODE) == 0) {
            int32_t death_y = (int16_t)ae_rd16(r, obj + AE_OBJ_HOME_Y) + 2048;
            int32_t y = signed32(ae_rd32(r, obj + AE_OBJ_POS_Y));
            if (death_y < y) {
                if (ae_rd8(r, obj + AE_OBJ_TYPE) == 0x3Fu) {
                    (void)call_hook(r, h, h ? h->FUN_8013F660 : NULL,
                                    "FUN_8013F660 hook not provided", obj, 0, 0, 0);
                } else {
                    ae_ZakoDefeat(r, h, obj);
                }
                if (ae_ram_failed(r)) return;
                continue;
            }
        }

        if (ae_rd8(r, obj + AE_OBJ_MOTION_STATE) != 0 &&
            (ae_rd32(r, obj + AE_OBJ_FLAGS) & 0x00200000u) == 0)
            continue;

        uint32_t type = ae_rd8(r, obj + AE_OBJ_TYPE);
        uint32_t type_update = ae_rd32(r, AE_G_ZAKO_TYPE_HANDLER_TABLE + type * 4u);
        (void)call_dispatch(r, h, h ? h->ZakoTypeUpdate : NULL,
                            "ZakoTypeUpdate hook not provided", type_update,
                            obj, 0, 0, 0);
        if (ae_ram_failed(r)) return;
    }

    ae_ZakoHooksRun(r, h);
    if (ae_ram_failed(r)) return;
    ae_ZakoDebugMenuDraw(r, h);
}

void ae_ZakoUpdateAll(AeRam *r, const AeZako1Hooks *h) {
    (void)call_hook(r, h, h ? h->ProjectilesUpdateDraw : NULL,
                    "ProjectilesUpdateDraw hook not provided", 0, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    if (ae_rd8(r, AE_G_ZAKO_FRAME_ENABLE) != 0) return;

    if ((ae_rd32(r, AE_G_ZAKO_FRAME_STATE) & 4u) == 0) {
        ae_ZakoUpdateLoop(r, h);
        if (ae_ram_failed(r)) return;
    }
    (void)call_hook(r, h, h ? h->Draw_Nop : NULL,
                    "Draw_Nop hook not provided", 1u, 0x0010A0F0u, 0, 0);
    if (ae_ram_failed(r)) return;
    ae_ZakoCullAndDraw(r, h);
    if (ae_ram_failed(r)) return;
    ae_ZakoSpawnNearby(r, h);
}




void ae_ObjPlaySfx(AeRam *r, const AeZako1Hooks *h, uint32_t obj, int32_t id) {
    uint32_t position = AE_ZAKO1_STACK_ObjSfx;
    uint32_t z = ae_rd16(r, obj + AE_OBJ_POS_Z);
    ae_wr16(r, position, ae_rd16(r, obj + AE_OBJ_POS_X));
    ae_wr16(r, position + AE_ZAKO_SVECTOR_Y, ae_rd16(r, obj + AE_OBJ_POS_Y));
    ae_wr16(r, position + AE_ZAKO_SVECTOR_Z, z);
    ae_wr16(r, position + AE_ZAKO_SVECTOR_PAD, (uint32_t)(int32_t)(int16_t)z);
    (void)ae_PlaySfxAt(r, h ? h->sound_hw : NULL, id, position);
}

void ae_ObjPuffAt(AeRam *r, const AeZako1Hooks *h, uint32_t obj,
                  int32_t dx, int32_t dy, int32_t dz) {
    uint32_t position = AE_ZAKO1_STACK_Puff;
    uint32_t x = (uint32_t)ae_rd16(r, obj + AE_OBJ_POS_X) + (uint32_t)dx;
    uint32_t y = (uint32_t)ae_rd16(r, obj + AE_OBJ_POS_Y) - ((uint32_t)dy + 0x80u);
    uint32_t z = (uint32_t)ae_rd16(r, obj + AE_OBJ_POS_Z) + (uint32_t)dz;
    ae_wr16(r, position, x);
    ae_wr16(r, position + AE_ZAKO_SVECTOR_Y, y);
    ae_wr16(r, position + AE_ZAKO_SVECTOR_Z, z);
    ae_SpawnPuff(r, h ? h->spawn_hooks : NULL, position);
}

void ae_ObjDeathSfx(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    ae_ObjPuffAt(r, h, obj, 0, 0, 0);
    uint32_t kind = ae_rd8(r, obj + AE_OBJ_HIT_EFFECT_COUNT) & 3u;
    int32_t sound = kind == 1u ? 0x60 : (kind == 2u ? 0x6A : 0x5F);
    ae_ObjPlaySfx(r, h, obj, sound);
    ae_ObjPlaySfx(r, h, obj, 0x15);
}

void ae_ObjReenableBody(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
    if ((flags & 0x4002u) != 0x4002u) return;

    (void)ae_CollisionBodyRegister(r, h ? h->body_hooks : NULL, 4,
                                   obj + AE_OBJ_POS_X, obj);
    if (ae_ram_failed(r)) return;
    flags = ae_rd32(r, obj + AE_OBJ_FLAGS) & ~2u;
    ae_wr32(r, obj + AE_OBJ_FLAGS, flags);
    if ((flags & AE_OBJ_FLAG_TARGET_TRACKED) != 0)
        (void)ae_TargetListAdd(r, obj + AE_OBJ_TARGET_NODE);
}

void ae_ObjResetMotion(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    uint32_t hp = ae_rd8(r, obj + AE_OBJ_HP);
    ae_wr8(r, obj + AE_OBJ_MOTION_STATE, 0);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Z, 0);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Y, 0);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_X, 0);

    if (hp == 0) {
        uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
        if ((flags & 0x00200000u) != 0) {
            ae_ObjReenableBody(r, h, obj);
        } else {
            ae_ObjPlaySfx(r, h, obj, 0x15);
            if (ae_ram_failed(r)) return;
            if (ae_rds8(r, AE_G_ZAKO_NO_HIT_DAMAGE) != 0)
                ae_ZakoDefeat(r, h, obj);
        }
        return;
    }

    uint32_t old_slot = ae_rd8(r, obj + AE_OBJ_PREV_ANIM_SLOT);
    uint32_t current_slot = ae_rd8(r, obj + AE_OBJ_ANIM_SLOT);
    if (old_slot == current_slot) return;
    ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, old_slot);
    uint32_t anim = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE) +
                    old_slot * AE_ANIM_CLIP_RECORD_SIZE;
    ae_wr16(r, obj + AE_OBJ_ANIM_FRAME, ae_rd16(r, anim));
}

void ae_ObjSwapToReactAnim(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    if ((ae_rd32(r, obj + AE_OBJ_FLAGS) & 0x1000u) == 0) return;
    if (!ae_ObjIsOwnerType0B(r, obj)) return;

    uint32_t current = ae_rd8(r, obj + AE_OBJ_ANIM_SLOT);
    uint32_t reaction = ae_rd8(r, obj + AE_OBJ_REACT_ANIM);
    ae_wr8(r, obj + AE_OBJ_PREV_ANIM_SLOT, current);
    ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, reaction);
    uint32_t anim = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE) +
                    reaction * AE_ANIM_CLIP_RECORD_SIZE;
    ae_wr16(r, obj + AE_OBJ_ANIM_FRAME, ae_rd16(r, anim));
    ae_ZakoOnHitByGadget(r, h, obj);
    if (ae_ram_failed(r)) return;
    ae_ObjDeathSfx(r, h, obj);
}

int32_t ae_ObjUpdateMotionState(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    uint32_t motion = ae_rd8(r, obj + AE_OBJ_MOTION_STATE);
    if (motion == 0) {
        ae_ObjSwapToReactAnim(r, h, obj);
        return 0;
    }

    uint8_t timer = (uint8_t)(ae_rd8(r, obj + AE_OBJ_MOTION_TIMER) + 1u);
    ae_wr8(r, obj + AE_OBJ_MOTION_TIMER, timer);
    if ((ae_rd32(r, obj + AE_OBJ_FLAGS) & 0x00100000u) != 0) {
        uint32_t slot = ae_rd8(r, obj + AE_OBJ_ANIM_SLOT);
        uint32_t anim = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE) +
                        slot * AE_ANIM_CLIP_RECORD_SIZE;
        uint32_t last_frame = ae_rd16(r, anim + AE_ANIM_CLIP_LAST_FRAME);
        return ae_rd16(r, obj + AE_OBJ_ANIM_FRAME) == last_frame
            ? 2 : (int32_t)last_frame;
    }

    int32_t ground_y = ae_ObjApplyVelocity(r, h, obj);
    if (ae_ram_failed(r)) return 0;
    ae_ObjAnimClampLastFrame(r, obj);
    motion = ae_rd8(r, obj + AE_OBJ_MOTION_STATE);

    if (motion == 4u) {
        if (ae_ObjBounceOnGround(r, obj, (int16_t)ground_y) != 0) {
            uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
            ae_wr32(r, obj + AE_OBJ_FLAGS, flags | 0x00100000u);
        }

        int32_t gravity;
        if (ae_rds8(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_HIT_MODE) == 1) {
            gravity = ae_rds8(r, AE_G_ZAKO_STATE + AE_ZAKO_STATE_GRAVITY);
        } else {
            uint32_t type = ae_rd8(r, obj + AE_OBJ_LAST_HIT_TYPE);
            gravity = ae_rds8(r, AE_G_ZAKO_MOTION_TABLE +
                              type * AE_ZAKO_MOTION_RECORD_SIZE + AE_ZAKO_MOTION_GRAVITY);
        }
        int32_t velocity_y = (int16_t)ae_rd16(r, obj + AE_OBJ_VELOCITY_Y);
        ae_wr16(r, obj + AE_OBJ_VELOCITY_Y,
                (uint32_t)add_signed32(velocity_y, gravity));
        return 1;
    }

    if (motion == 5u) {
        int32_t knockback_done = ae_ObjKnockbackDecay(r, obj);
        if (knockback_done != 0) {
            uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
            ae_wr32(r, obj + AE_OBJ_FLAGS, flags | 0x00100000u);
        } else {
            int32_t y = signed32(ae_rd32(r, obj + AE_OBJ_POS_Y));
            int32_t ground = (int16_t)ground_y;
            int32_t difference = add_signed32(y, (int32_t)(0u - (uint32_t)ground));
            if (difference < 0)
                difference = add_signed32(ground, (int32_t)(0u - (uint32_t)y));
            if (difference >= 129) {
                uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
                ae_wr32(r, obj + AE_OBJ_FLAGS, flags | 0x00100000u);
            }
        }
        ae_wr32(r, obj + AE_OBJ_POS_Y, (uint32_t)(int16_t)ground_y);
        return 1;
    }
    return 1;
}

int32_t ae_PointOffscreen(AeRam *r, const AeZako1Hooks *h, uint32_t pos,
                          int32_t y_offset, int32_t use_margin) {
    uint32_t projection = AE_G_ZAKO_PROJECTION_SETTINGS;
    (void)call_hook(r, h, h ? h->FUN_8002FB00 : NULL,
                    "FUN_8002FB00 projection hook not provided", projection, 0, 0, 0);
    (void)call_hook(r, h, h ? h->FUN_8002FB30 : NULL,
                    "FUN_8002FB30 projection hook not provided", projection, 0, 0, 0);
    if (ae_ram_failed(r)) return 1;

    uint32_t input = AE_ZAKO1_STACK_Point;
    uint32_t screen_x = input + AE_ZAKO_PROJECTION_SCREEN;
    uint32_t screen_y = screen_x + AE_ZAKO_SCREEN_Y;
    uint32_t depth_flags = input + AE_ZAKO_PROJECTION_FLAGS;
    ae_wr16(r, input, ae_rd16(r, pos));
    ae_wr16(r, input + AE_ZAKO_SVECTOR_Y,
            (uint32_t)ae_rd16(r, pos + AE_ZAKO_VECTOR_Y) - (uint32_t)y_offset);
    ae_wr16(r, input + AE_ZAKO_SVECTOR_Z, ae_rd16(r, pos + AE_ZAKO_VECTOR_Z));
    int32_t depth = call_hook(r, h, h ? h->FUN_80031330 : NULL,
                              "FUN_80031330 projection hook not provided",
                              input, screen_x, depth_flags, depth_flags);
    if (ae_ram_failed(r)) return 1;

    int32_t margin = use_margin != 0 ? 500 : 0;
    int32_t min_screen = -100 - margin;
    int32_t max_x = margin + 484;
    int32_t max_y = margin + 340;
    int32_t x = (int16_t)ae_rd16(r, screen_x);
    int32_t y = (int16_t)ae_rd16(r, screen_y);
    if (x < min_screen || x > max_x || y < min_screen || y > max_y) return 1;

    uint32_t near_addr = use_margin != 0 ? AE_G_ZAKO_NEAR_MARGIN : AE_G_ZAKO_NEAR_NO_MARGIN;
    if ((int16_t)ae_rd16(r, near_addr) < depth) return 1;
    return depth <= 0;
}

void ae_ZakoCullAndDraw(AeRam *r, const AeZako1Hooks *h) {
    int32_t count = ae_rds16(r, AE_G_ZAKO_SPAWN_COUNT);
    for (int32_t i = 0; i < count; ++i) {
        uint32_t obj = AE_G_ZAKO_OBJECTS + (uint32_t)i * AE_ZAKO_OBJECT_SIZE;
        uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
        flags |= 0x4u;
        ae_wr32(r, obj + AE_OBJ_FLAGS, flags);
        if ((flags & AE_OBJ_FLAG_SPAWNED) == 0) {
            ae_ZakoPoseHidden(r, h, obj);
            if (ae_ram_failed(r)) return;
            continue;
        }

        int32_t shadow_height = (int16_t)ae_rd16(r, obj + AE_OBJ_SHADOW_SIZE);
        int32_t y_offset = floor_shift(shadow_height + (shadow_height >> 31), 1);
        int32_t use_margin = (int32_t)((flags >> 9) & 1u);
        int32_t offscreen = ae_PointOffscreen(r, h, obj + AE_OBJ_POS_X,
                                              y_offset, use_margin);
        if (ae_ram_failed(r)) return;
        if (offscreen == 0) {
            flags = ae_rd32(r, obj + AE_OBJ_FLAGS) & ~0x5u;
            ae_wr32(r, obj + AE_OBJ_FLAGS, flags);
            if (ae_rd8(r, obj + AE_OBJ_DRAW_SCALE) != 0)
                ae_ZakoDraw(r, h, obj);
            else
                ae_ZakoPoseHidden(r, h, obj);
        } else {
            ae_ZakoPoseHidden(r, h, obj);
        }
        if (ae_ram_failed(r)) return;
    }
}

void ae_ZakoGetBonePoint(AeRam *r, uint32_t obj, int32_t bone, int32_t vertex,
                         uint32_t out) {
    uint32_t matrix = AE_G_ZAKO_MODEL_BONE_MATRICES + (uint32_t)bone * AE_ZAKO_MATRIX_RECORD_SIZE;
    if (vertex == -1) {
        ae_wr16(r, out, ae_rd16(r, matrix + AE_ZAKO_MATRIX_TRANSLATE_X));
        ae_wr16(r, out + AE_ZAKO_SVECTOR_Y,
                ae_rd16(r, matrix + AE_ZAKO_MATRIX_TRANSLATE_Y));
        ae_wr16(r, out + AE_ZAKO_SVECTOR_Z,
                ae_rd16(r, matrix + AE_ZAKO_MATRIX_TRANSLATE_Z));
        return;
    }
    uint32_t verts = ae_rd32(r, obj + AE_OBJ_VERTS) + (uint32_t)vertex * 8u;
    (void)ae_sdk_ApplyMatrixSV(r, matrix, verts, out);
    ae_wr16(r, out, ae_rd16(r, out) + ae_rd16(r, matrix + AE_ZAKO_MATRIX_TRANSLATE_X));
    ae_wr16(r, out + AE_ZAKO_SVECTOR_Y,
            ae_rd16(r, out + AE_ZAKO_SVECTOR_Y) +
            ae_rd16(r, matrix + AE_ZAKO_MATRIX_TRANSLATE_Y));
    ae_wr16(r, out + AE_ZAKO_SVECTOR_Z,
            ae_rd16(r, out + AE_ZAKO_SVECTOR_Z) +
            ae_rd16(r, matrix + AE_ZAKO_MATRIX_TRANSLATE_Z));
}

void ae_ZakoUpdateLinkPoints(AeRam *r, uint32_t obj) {
    for (uint32_t i = 0; i < 2u; ++i) {
        uint32_t link = ae_rd32(r, obj + AE_OBJ_LINK0 + i * 4u);
        if (link == 0) continue;
        int32_t bone = (int16_t)ae_rd16(r, link + AE_ZAKO_LINK_POINT_BONE);
        int32_t vertex = (int16_t)ae_rd16(r, link + AE_ZAKO_LINK_POINT_VERTEX);
        ae_ZakoGetBonePoint(r, obj, bone, vertex, link);
    }
}

void ae_ZakoUpdateColliderPos(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    int32_t dx = (int16_t)ae_rd16(r, AE_G_PLAYER_OBJECT + AE_OBJ_FACING_X);
    int32_t dz = (int16_t)ae_rd16(r, AE_G_PLAYER_OBJECT + AE_OBJ_FACING_Z);
    int32_t facing = call_hook(r, h, h ? h->Atan2Safe : NULL,
                               "Atan2Safe hook not provided", (uint32_t)dx,
                               (uint32_t)dz, 0, 0);
    if (ae_ram_failed(r)) return;
    int32_t timer = (int16_t)ae_rd16(r, obj + AE_OBJ_TIMER);
    if (timer != 0) {
        int32_t turn_speed = (int16_t)ae_rd16(r, obj + AE_OBJ_TURN_SPEED);
        int32_t sum = add_signed32(facing, turn_speed);
        int32_t angle = add_signed32(sum, 4096);
        int32_t rounded = angle < 0 ? add_signed32(sum, 8191) : angle;
        int32_t quotient = floor_shift(rounded, 12);
        facing = signed32((uint32_t)angle - ((uint32_t)quotient << 12));
    }
    int32_t cosine = ae_sdk_rcos(r, facing);
    uint32_t collider_index = ae_rd16(r, obj + AE_OBJ_OWN_COLLIDER);
    uint32_t table = ae_rd32(r, AE_G_ZAKO_COLLIDER_TABLE_PTR);
    uint32_t collider = table + collider_index * AE_ZAKO_COLLIDER_RECORD_SIZE;
    ae_wr16(r, collider + AE_ZAKO_COLLIDER_FLOW_ANGLE, (uint32_t)cosine);
    int32_t sine = ae_sdk_rsin(r, facing);
    ae_wr16(r, collider + AE_ZAKO_COLLIDER_FLOW_X, 0);
    ae_wr16(r, collider + AE_ZAKO_COLLIDER_FLOW_Z, (uint32_t)sine);
    ae_wr16(r, collider + AE_ZAKO_COLLIDER_POS_X,
            ae_rd16(r, AE_G_ZAKO_COLLIDER_CONFIG + AE_G_ZAKO_COLLIDER_CONFIG_X));
    ae_wr16(r, collider + AE_ZAKO_COLLIDER_POS_Y,
            ae_rd16(r, AE_G_ZAKO_COLLIDER_CONFIG + AE_G_ZAKO_COLLIDER_CONFIG_Y) +
            ae_rd16(r, collider + AE_ZAKO_COLLIDER_HEIGHT_INPUT));
    ae_wr16(r, collider + AE_ZAKO_COLLIDER_POS_Z,
            ae_rd16(r, AE_G_ZAKO_COLLIDER_CONFIG + AE_G_ZAKO_COLLIDER_CONFIG_Z));
}

void ae_ZakoBuildMatrix(AeRam *r, uint32_t obj) {
    uint32_t matrix = ae_rd32(r, obj + AE_OBJ_BONE_MATRICES);
    uint32_t direction = obj + AE_OBJ_FACING_X;
    uint32_t mode = (ae_rd32(r, obj + AE_OBJ_FLAGS) >> 22) & 3u;
    if (mode == 0) {
        (void)ae_sdk_MatrixFromDirXZ(r, direction, matrix);
    } else if (mode == 1) {
        uint32_t angles = AE_ZAKO1_STACK_MatrixAngles;
        ae_wr16(r, angles, ae_rd16(r, obj + AE_OBJ_UNK_D4));
        int32_t yaw = 0x1C00 - (int16_t)ae_rd16(r, obj + AE_OBJ_HEADING);
        int32_t rounded = yaw < 0 ? add_signed32(yaw, 4095) : yaw;
        int32_t turns = floor_shift(rounded, 12);
        ae_wr16(r, angles + AE_ZAKO_SVECTOR_Y,
                (uint32_t)signed32((uint32_t)yaw - ((uint32_t)turns << 12)));
        ae_wr16(r, angles + AE_ZAKO_SVECTOR_Z, ae_rd16(r, obj + AE_OBJ_UNK_D6));
        (void)ae_sdk_RotMatrixYXZ(r, angles, matrix);
    } else {
        (void)ae_sdk_MatrixLookDir(r, direction, matrix);
    }
    if (ae_ram_failed(r)) return;

    uint32_t scale = ae_rd8(r, obj + AE_OBJ_DRAW_SCALE);
    int32_t draw_scale_control = ae_rds8(r, AE_G_ZAKO_DRAW_SCALE_CONTROL);
    uint32_t scale_vector = AE_ZAKO1_STACK_MatrixScale;
    if (scale == 10u && draw_scale_control == 0) {
        /* Scale ten is the unscaled default when the draw-scale option is off. */
    } else if (scale == 255u) {
        ae_wr32(r, scale_vector, (uint32_t)(int16_t)ae_rd16(r, obj + AE_OBJ_UNK_C2));
        ae_wr32(r, scale_vector + AE_ZAKO_VECTOR_Y,
                (uint32_t)(int16_t)ae_rd16(r, obj + AE_OBJ_SCALE_COMPONENT_Y));
        ae_wr32(r, scale_vector + AE_ZAKO_VECTOR_Z,
                (uint32_t)(int16_t)ae_rd16(r, obj + AE_OBJ_UNK_C6));
        (void)ae_sdk_ScaleMatrix(r, matrix, scale_vector);
    } else {
        uint32_t scale_factor = draw_scale_control != 0
            ? (uint32_t)draw_scale_control : scale;
        int32_t scaled = scale_hit_value((int32_t)(scale_factor << 12));
        ae_wr32(r, scale_vector, (uint32_t)scaled);
        ae_wr32(r, scale_vector + AE_ZAKO_VECTOR_Y, (uint32_t)scaled);
        ae_wr32(r, scale_vector + AE_ZAKO_VECTOR_Z, (uint32_t)scaled);
        (void)ae_sdk_ScaleMatrix(r, matrix, scale_vector);
    }
    if ((ae_rd32(r, obj + AE_OBJ_FLAGS) & AE_OBJ_FLAG_UNK_7) != 0) {
        uint32_t shrink = AE_ZAKO1_STACK_MatrixShrink;
        ae_wr32(r, shrink, 0x14CCu);
        ae_wr32(r, shrink + AE_ZAKO_VECTOR_Y, 0x0600u);
        ae_wr32(r, shrink + AE_ZAKO_VECTOR_Z, 0x14CCu);
        (void)ae_sdk_ScaleMatrix(r, matrix, shrink);
    }
}

void ae_ZakoPoseHidden(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
    if ((flags & AE_OBJ_FLAG_UNK_13) != 0 && ae_rd8(r, obj + AE_OBJ_DRAW_SCALE) != 0) {
        ae_ZakoBuildMatrix(r, obj);
        ae_ModelPoseBones(r, obj, 3u);
        if ((flags & AE_OBJ_FLAG_UNK_13) != 0) ae_ZakoUpdateLinkPoints(r, obj);
    }
    if (ae_rd16(r, obj + AE_OBJ_OWN_COLLIDER) != 0xFFFFu)
        ae_ZakoUpdateColliderPos(r, h, obj);
}

void ae_ModelLightColorsToScratch(AeRam *r, const AeZako1Hooks *h,
                                  uint32_t ot, uint32_t vertices,
                                  uint32_t model, int32_t flash) {
    uint32_t model_header = ae_rd32(r, model + AE_OBJ_MODEL);
    uint32_t primitives = ae_rd32(r, model + AE_OBJ_PRIMS);
    (void)call_hook(r, h, h ? h->ZakoDebugDrawPrimitive : NULL,
                    "ZakoDebugDrawPrimitive lighting hook not provided",
                    ot, vertices, model, (uint32_t)flash);
    if (ae_ram_failed(r)) return;
    (void)call_hook(r, h, h ? h->ModelEmitPrims : NULL,
                    "ModelEmitPrims hook not provided", ot, primitives,
                    vertices, 0u);
    (void)model_header;
}

void ae_ZakoDraw(AeRam *r, const AeZako1Hooks *h, uint32_t obj) {
    (void)ae_sdk_VectorNormalSS(r, obj + AE_OBJ_FACING_X, obj + AE_OBJ_FACING_X);
    int32_t shadow_size = (int16_t)ae_rd16(r, obj + AE_OBJ_SHADOW_SIZE);
    if (shadow_size > 0) {
        int32_t ground = ae_ObjGroundHeightCached(r, h, obj, 0, 0, 0);
        uint32_t ground_height = ae_rd32(r, obj + AE_OBJ_GROUND_HEIGHT);
        (void)call_hook(r, h, h ? h->DrawShadow : NULL,
                        "DrawShadow hook not provided", obj + AE_OBJ_POS_X,
                        (uint32_t)(int32_t)(int16_t)ground,
                        (uint32_t)shadow_size, ground_height);
        if (ae_ram_failed(r)) return;
    }

    ae_ZakoBuildMatrix(r, obj);
    ae_ModelPoseBones(r, obj, 3u);
    if ((ae_rd32(r, obj + AE_OBJ_FLAGS) & AE_OBJ_FLAG_UNK_13) != 0)
        ae_ZakoUpdateLinkPoints(r, obj);
    if (ae_rd16(r, obj + AE_OBJ_OWN_COLLIDER) != 0xFFFFu)
        ae_ZakoUpdateColliderPos(r, h, obj);
    if (ae_ram_failed(r)) return;

    (void)call_hook(r, h, h ? h->ModelTransformSkinned : NULL,
                    "ModelTransformSkinned hook not provided", obj, 0, 0, 0);
    if (ae_ram_failed(r)) return;

    uint32_t model_header = ae_rd32(r, obj + AE_OBJ_MODEL);
    uint32_t draw_index = ae_rd32(r, AE_G_ZAKO_DRAW_CONTEXT + AE_ZAKO_DRAW_CONTEXT_COUNTER);
    uint32_t ot = AE_G_ZAKO_DRAW_CONTEXT + AE_ZAKO_OT_BUFFER_FIRST +
                  draw_index * AE_ZAKO_OT_DRAW_STRIDE;
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
    if ((flags & 0x04000000u) != 0) {
        (void)call_hook(r, h, h ? h->ModelPrepCycleColors : NULL,
                        "ModelPrepCycleColors hook not provided",
                        AE_G_ZAKO_RENDER_CONTEXT,
                        ae_rd16(r, model_header + AE_MODEL_HEADER_VERTEX_COUNT), 0, 0);
        if (ae_ram_failed(r)) return;
        (void)call_hook(r, h, h ? h->ModelEmitPrimsCycleColors : NULL,
                        "ModelEmitPrimsCycleColors hook not provided", ot,
                        AE_G_ZAKO_RENDER_CONTEXT, obj, 0);
        return;
    }

    uint32_t flash = ae_rd8(r, obj + AE_OBJ_DRAW_FLASH_TIMER);
    int32_t flash_color = 0;
    if (flash != 0) {
        uint32_t frame_count = ae_rd32(r, AE_G_ZAKO_DEBUG_DRAW_COUNT);
        uint32_t quotient = (uint32_t)(((uint64_t)frame_count * 0xCCCCCCCDull) >> 34);
        uint32_t remainder = frame_count - quotient * 5u;
        if (remainder >= 3u) remainder = 5u - remainder;
        int32_t value = (int32_t)(remainder * 0x100u - remainder);
        flash_color = floor_shift(value + (value >> 31), 1);
        ae_wr8(r, obj + AE_OBJ_DRAW_FLASH_TIMER, flash - 1u);
    }
    ae_ModelLightColorsToScratch(r, h, ot, AE_G_ZAKO_RENDER_CONTEXT,
                                 obj, flash_color);
}

void ae_ZakoDebugMenuDraw(AeRam *r, const AeZako1Hooks *h) {
    if (ae_rd8(r, AE_G_ZAKO_DEBUG_ENABLED) == 0) return;

    uint32_t entry_table = ae_rd32(r, AE_G_ZAKO_DEBUG_ENTRY_TABLE);
    int32_t entry_count = ae_rds16(r, AE_G_ZAKO_DEBUG_ENTRY_COUNT);
    for (int32_t i = 0; i < entry_count; ++i) {
        uint32_t entry = entry_table + (uint32_t)i * AE_ZAKO_DEBUG_ENTRY_SIZE;
        (void)call_hook(r, h, h ? h->ColorFadeSet : NULL,
                        "ColorFadeSet hook not provided", 16u, 100u, 0, 0);
        (void)call_hook(r, h, h ? h->DebugTextPrintfWhite : NULL,
                        "DebugTextPrintfWhite hook not provided",
                        AE_G_ZAKO_DEBUG_STATUS_LABELS, (uint32_t)i, 0, 0);
        (void)call_hook(r, h, h ? h->ColorFadeSet : NULL,
                        "ColorFadeSet hook not provided", 60u, 100u, 0, 0);
        uint32_t enabled_text = ae_rd8(r, entry + AE_ZAKO_DEBUG_ENTRY_ENABLED) == 1u
            ? AE_G_ZAKO_DEBUG_ENTRY_LABELS - 8u
            : AE_G_ZAKO_DEBUG_ENTRY_LABELS - 4u;
        (void)call_hook(r, h, h ? h->DebugTextPrintfWhite : NULL,
                        "DebugTextPrintfWhite hook not provided", enabled_text, 0, 0, 0);
        (void)call_hook(r, h, h ? h->ColorFadeSet : NULL,
                        "ColorFadeSet hook not provided", 76u, 100u, 0, 0);
        (void)call_hook(r, h, h ? h->DebugTextPrintfWhite : NULL,
                        "DebugTextPrintfWhite hook not provided",
                        AE_G_ZAKO_DEBUG_ENTRY_LABELS,
                        ae_rd16(r, entry + AE_ZAKO_DEBUG_ENTRY_X), 0, 0);
        (void)call_hook(r, h, h ? h->ColorFadeSet : NULL,
                        "ColorFadeSet hook not provided", 100u, 100u, 0, 0);
        (void)call_hook(r, h, h ? h->DebugTextPrintfWhite : NULL,
                        "DebugTextPrintfWhite hook not provided",
                        AE_G_ZAKO_DEBUG_ENTRY_LABELS,
                        ae_rd16(r, entry + AE_ZAKO_DEBUG_ENTRY_Z), 0, 0);
        if (ae_ram_failed(r)) return;
    }

    uint32_t projection = AE_G_ZAKO_PROJECTION_SETTINGS;
    (void)call_hook(r, h, h ? h->FUN_8002FB00 : NULL,
                    "FUN_8002FB00 projection hook not provided", projection, 0, 0, 0);
    (void)call_hook(r, h, h ? h->FUN_8002FB30 : NULL,
                    "FUN_8002FB30 projection hook not provided", projection, 0, 0, 0);
    if (ae_ram_failed(r)) return;

    int32_t object_count = ae_rds16(r, AE_G_ZAKO_SPAWN_COUNT);
    uint32_t ot = ae_rd32(r, AE_G_ZAKO_OT_HEAD);
    uint32_t draw_index = ae_rd32(r, AE_G_ZAKO_DEBUG_DRAW_INDEX);
    uint32_t draw_state = AE_G_ZAKO_DEBUG_CONTEXT_BASE +
                          draw_index * AE_ZAKO_OT_DRAW_STRIDE;
    for (int32_t i = 0; i < object_count; ++i) {
        uint32_t obj = AE_G_ZAKO_OBJECTS + (uint32_t)i * AE_ZAKO_OBJECT_SIZE;
        uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
        if ((flags & AE_OBJ_FLAG_SPAWNED) != 0) {
            uint32_t result = (uint32_t)call_hook(
                r, h, h ? h->ZakoDebugDrawPrimitive : NULL,
                "ZakoDebugDrawPrimitive hook not provided", obj,
                obj + AE_OBJ_POS_X, ot, draw_state);
            if (ae_ram_failed(r)) return;
            if (result != 0) ot = result;
        }
    }
    ae_wr32(r, AE_G_ZAKO_OT_HEAD, ot);
}


