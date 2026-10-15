#include "ae_zako2.h"

#include "../bodies/ae_bodies.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"

#include <stdint.h>

static int32_t zako2_hook(AeRam *r, const AeZako2Hooks *h, AeZako2CallHook hook,
                          const char *name, uint32_t a0, uint32_t a1,
                          uint32_t a2, uint32_t a3);

static uint32_t zako2_projectile(uint32_t index) {
    return AE_G_ZAKO2_PROJECTILES + index * AE_ZAKO2_PROJECTILE_SIZE;
}

static int32_t zako2_s32(uint32_t value) {
    if (value <= 0x7FFFFFFFu) return (int32_t)value;
    return (int32_t)((int64_t)value - 0x100000000LL);
}

static int32_t zako2_s16(uint32_t value) {
    value &= 0xFFFFu;
    return value < 0x8000u ? (int32_t)value : (int32_t)value - 0x10000;
}

static int32_t zako2_asr64(int64_t value, uint32_t shift) {
    int64_t divisor = (int64_t)1 << shift;
    if (value >= 0) return (int32_t)(value / divisor);
    return (int32_t)-(((-value) + divisor - 1) / divisor);
}

static int32_t zako2_s8(uint32_t value) {
    value &= 0xFFu;
    return value < 0x80u ? (int32_t)value : (int32_t)value - 0x100;
}

static int32_t zako2_difference(int32_t left, int32_t right) {
    return zako2_s32((uint32_t)left - (uint32_t)right);
}

static uint32_t zako2_square(int32_t value) {
    int64_t wide = value;
    return (uint32_t)(wide * wide);
}

static uint32_t zako2_distance3_squared(AeRam *r, uint32_t projectile,
                                        uint32_t player_x_offset,
                                        uint32_t player_y_offset,
                                        uint32_t player_z_offset,
                                        int32_t y_bias) {
    int32_t dx = zako2_difference(ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_X),
                                  zako2_s32(ae_rd32(r, AE_G_ZAKO2_PLAYER + player_x_offset)));
    int32_t dy = zako2_difference((int32_t)ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y) + y_bias,
                                  zako2_s32(ae_rd32(r, AE_G_ZAKO2_PLAYER + player_y_offset)));
    int32_t dz = zako2_difference(ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Z),
                                  zako2_s32(ae_rd32(r, AE_G_ZAKO2_PLAYER + player_z_offset)));
    return zako2_square(dx) + zako2_square(dy) + zako2_square(dz);
}

static uint32_t zako2_distance_xz_squared(AeRam *r, uint32_t projectile) {
    int32_t dx = zako2_difference(ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_X),
                                  zako2_s32(ae_rd32(r, AE_G_ZAKO2_PLAYER + AE_ZAKO2_PLAYER_POS_X)));
    int32_t dz = zako2_difference(ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Z),
                                  zako2_s32(ae_rd32(r, AE_G_ZAKO2_PLAYER + AE_ZAKO2_PLAYER_POS_Z)));
    return zako2_square(dx) + zako2_square(dz);
}

static void zako2_spawn_particle(AeRam *r, const AeZako2Hooks *h,
                                 int32_t type, uint32_t position) {
    if (!h || !h->spawn_hooks) {
        ae_ram_fail(r, "zako2 spawn hooks not provided");
        return;
    }
    ae_SpawnParticle(r, h->spawn_hooks, type, position);
}

static int32_t zako2_ground_height(AeRam *r, const AeZako2Hooks *h,
                                   uint32_t projectile) {
    if (!h || !h->world_hooks) {
        ae_ram_fail(r, "zako2 world hooks not provided");
        return 0;
    }
    return ae_GetGroundHeight(r, h->world_hooks,
                              ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_X),
                              ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y),
                              ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Z));
}

static uint32_t zako2_next_random(AeRam *r, uint32_t seed_address) {
    uint32_t next = (uint16_t)(ae_rd16(r, seed_address) * AE_ZAKO2_PICKUP_LCG_MULTIPLIER +
                               AE_ZAKO2_PICKUP_LCG_INCREMENT);
    ae_wr16(r, seed_address, next);
    return next;
}

static int32_t zako2_items_rsin(void *user, AeRam *r, int32_t angle) {
    (void)user;
    return ae_sdk_rsin(r, angle);
}

static int32_t zako2_items_rcos(void *user, AeRam *r, int32_t angle) {
    (void)user;
    return ae_sdk_rcos(r, angle);
}

static int32_t zako2_body_contact_kind(AeRam *r, uint32_t projectile) {
    uint32_t body = ae_rd32(r, projectile + AE_ZAKO2_PROJECTILE_BODY);
    if (body == 0) return 0;
    return (int32_t)((ae_rd32(r, body + AE_ZAKO2_PLAYER_BODY_CONTACT) >>
                      AE_ZAKO2_BODY_CONTACT_KIND_SHIFT) & AE_ZAKO2_PROJECTILE_CONTACT_MASK);
}

static void zako2_move_projectile(AeRam *r, uint32_t projectile) {
    ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_POS_X,
            ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_POS_X) +
            ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_X));
    ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y,
            ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y) +
            ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y));
    ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Z,
            ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Z) +
            ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Z));
}

static void zako2_projectile_update_type5(AeRam *r, const AeZako2Hooks *h,
                                          uint32_t projectile) {
    uint32_t countdown = projectile + AE_ZAKO2_PROJECTILE_VARIANT;
    uint32_t bounce_phase = projectile + AE_ZAKO2_PROJECTILE_STATE;
    uint32_t next_countdown = (ae_rd8(r, countdown) - 1u) & 0xFFu;
    ae_wr8(r, countdown, next_countdown);

    if (ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y) <
        ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_GROUND_Y)) {
        zako2_move_projectile(r, projectile);
        ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y,
                ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y) +
                AE_ZAKO2_PROJECTILE_GRAVITY_TYPE5);
        if (next_countdown == 0) {
            ae_wr8(r, countdown, 2);
            if (ae_rd8(r, bounce_phase) < 4u)
                ae_wr8(r, bounce_phase, ae_rd8(r, bounce_phase) + 1u);
        }
        if (ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y) >=
            ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_GROUND_Y)) {
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y,
                    ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_GROUND_Y));
            zako2_spawn_particle(r, h, AE_ZAKO2_PARTICLE_SPARK,
                                 projectile + AE_ZAKO2_PROJECTILE_POS_X);
            if (ae_ram_failed(r)) return;
            ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_FIELD_15, 64);
            ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_FIELD_14,
                   (ae_rd8(r, bounce_phase) & 7u) << 5);
            return;
        }
    } else if (next_countdown == 0) {
        ae_wr8(r, countdown, 2);
        if (ae_rd8(r, bounce_phase) < 7u)
            ae_wr8(r, bounce_phase, ae_rd8(r, bounce_phase) + 1u);
    }
    if (ae_ram_failed(r)) return;

    if (ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y) <
        ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_GROUND_Y)) {
        ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_FIELD_15, 64);
        ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_FIELD_14,
               (ae_rd8(r, bounce_phase) & 7u) << 5);
        return;
    }

    uint32_t timer = (uint16_t)(ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_TIMER) + 1u);
    ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_TIMER, timer);
    if (timer >= AE_ZAKO2_PROJECTILE_TYPE5_LIFETIME) {
        ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_KIND, 0);
    } else if (timer >= AE_ZAKO2_PROJECTILE_PHASE_LIMIT) {
        if ((timer & 1u) != 0)
            ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_AGE, 1);
    } else {
        uint32_t distance = zako2_distance3_squared(r, projectile,
            AE_ZAKO2_PLAYER_POS_X, AE_ZAKO2_PLAYER_POS_Y, AE_ZAKO2_PLAYER_POS_Z, 0);
        if (zako2_s32(distance) < (int32_t)AE_ZAKO2_PROJECTILE_TYPE5_HIT_RANGE) {
            ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_STATE, 0);
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y, (uint32_t)-32);
            ae_wr8(r, countdown, 4);
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_TIMER, 60);
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y,
                    ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y) +
                    ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y));
            uint32_t first_random = zako2_next_random(r, AE_G_ZAKO2_RANDOM_SEED);
            int32_t first_velocity = (int32_t)((first_random >> 8) & 0x1Fu) - 16;
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_X,
                    ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_X) + (uint32_t)first_velocity);
            uint32_t second_random = zako2_next_random(r, AE_G_ZAKO2_RANDOM_SEED);
            int32_t second_velocity = (int32_t)((second_random >> 8) & 0x1Fu) - 16;
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Z,
                    ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Z) + (uint32_t)second_velocity);
            ae_ProjectileAddHitBody(r, h, projectile,
                AE_ZAKO2_PROJECTILE_TYPE5_HIT_RADIUS,
                AE_ZAKO2_PROJECTILE_TYPE5_HIT_PARAM,
                AE_ZAKO2_PROJECTILE_TYPE5_HIT_FLAGS);
        }
    }
    ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_FIELD_15, 64);
    ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_FIELD_14,
            (ae_rd8(r, bounce_phase) & 7u) << 5);
}

static int32_t zako2_projectile_update_type6(AeRam *r, const AeZako2Hooks *h,
                                             uint32_t projectile) {
    uint32_t timer = (uint16_t)(ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_TIMER) + 1u);
    ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_TIMER, timer);
    ae_wr16(r, AE_ZAKO2_COLLISION_WALL_PROBE, 2);
    if ((timer & 1u) != 0) {
        int32_t height = zako2_ground_height(r, h, projectile);
        if (ae_ram_failed(r)) return 0;
        ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_GROUND_Y, (uint32_t)height);
    }

    int32_t ground = ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_GROUND_Y);
    int32_t y = ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y);
    int32_t should_end = y > ground - AE_ZAKO2_PROJECTILE_TYPE5_GROUND_MARGIN ||
                         timer >= AE_ZAKO2_PROJECTILE_AIRBORNE_LIMIT;
    int32_t contact_kind = zako2_body_contact_kind(r, projectile);
    if (should_end || contact_kind == AE_ZAKO2_PROJECTILE_CONTACT_KIND) {
        int32_t difference = y - ground;
        if (difference >= 0 && difference < (int32_t)AE_ZAKO2_PROJECTILE_TYPE5_Y_TOLERANCE)
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y, (uint32_t)ground);
        else if (difference < 0 && -difference < (int32_t)AE_ZAKO2_PROJECTILE_TYPE5_Y_TOLERANCE)
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y, (uint32_t)ground);
        ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_KIND, 0);
        zako2_spawn_particle(r, h, AE_ZAKO2_PARTICLE_BURST,
                             projectile + AE_ZAKO2_PROJECTILE_POS_X);
        if (ae_ram_failed(r)) return 1;
        (void)zako2_hook(r, h, h ? h->PlaySfx : NULL, "PlaySfx",
                         AE_ZAKO2_PROJECTILE_DEFAULT_SOUND, 0, 0, 0);
        if (ae_ram_failed(r)) return 1;
        ae_CameraShake(r, 1);
        return 1;
    }

    zako2_move_projectile(r, projectile);
    zako2_spawn_particle(r, h, AE_ZAKO2_PARTICLE_SMALL,
                         projectile + AE_ZAKO2_PROJECTILE_POS_X);
    if (ae_ram_failed(r)) return 0;
    if ((timer & 3u) == 0)
        zako2_spawn_particle(r, h, AE_ZAKO2_PARTICLE_SPARK,
                             projectile + AE_ZAKO2_PROJECTILE_POS_X);
    uint32_t trail_countdown = projectile + AE_ZAKO2_PROJECTILE_TIMER_BYTE;
    if (ae_rd8(r, trail_countdown) != 0) {
        uint32_t remaining = (ae_rd8(r, trail_countdown) - 1u) & 0xFFu;
        ae_wr8(r, trail_countdown, remaining);
        if (remaining == 0)
            zako2_spawn_particle(r, h, AE_ZAKO2_PARTICLE_TRAIL,
                                 projectile + AE_ZAKO2_PROJECTILE_POS_X);
    }
    if (ae_ram_failed(r)) return 0;

    uint32_t target_flag = ae_rd8(r, projectile + AE_ZAKO2_PROJECTILE_FIELD_0F);
    if (target_flag != 0) {
        ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y,
                ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y) + 2u);
    } else if (
        ae_rd8(r, trail_countdown) == 0) {
        uint32_t input = AE_ZAKO2_TEMP_VECTOR;
        uint32_t output = AE_ZAKO2_TEMP_VECTOR + AE_ZAKO2_TEMP_VECTOR_OUTPUT_OFFSET;
        int32_t px = ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_X);
        int32_t py = ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y);
        int32_t pz = ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Z);
        uint32_t direction_x = (ae_rd32(r, AE_G_ZAKO2_PLAYER + AE_ZAKO2_PLAYER_VECTOR_X) << 2) +
            ae_rd16(r, AE_G_ZAKO2_PLAYER + AE_ZAKO2_PLAYER_TARGET_X) - (uint32_t)px;
        uint32_t direction_y = (ae_rd32(r, AE_G_ZAKO2_PLAYER + AE_ZAKO2_PLAYER_VECTOR_Y) << 1) - 128u +
            ae_rd16(r, AE_G_ZAKO2_PLAYER + AE_ZAKO2_PLAYER_TARGET_Y) - (uint32_t)py;
        uint32_t direction_z = ae_rd16(r, AE_G_ZAKO2_PLAYER + AE_ZAKO2_PLAYER_TARGET_Z) +
            (ae_rd32(r, AE_G_ZAKO2_PLAYER + AE_ZAKO2_PLAYER_VECTOR_Z) << 2) - (uint32_t)pz;
        ae_wr16(r, input + AE_ZAKO2_VECTOR_COMPONENT_X_OFFSET, direction_x);
        ae_wr16(r, input + AE_ZAKO2_VECTOR_COMPONENT_Y_OFFSET, direction_y);
        ae_wr16(r, input + AE_ZAKO2_VECTOR_COMPONENT_Z_OFFSET, direction_z);
        (void)ae_sdk_VectorNormalSS(r, input, output);
        if (ae_ram_failed(r)) return 0;
        int32_t velocity_x = zako2_asr64((int64_t)AE_ZAKO2_PROJECTILE_VECTOR_DAMPING *
            ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_X), AE_ZAKO2_PROJECTILE_VECTOR_DAMPING_SHIFT);
        int32_t velocity_y = zako2_asr64((int64_t)AE_ZAKO2_PROJECTILE_VECTOR_DAMPING *
            ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y), AE_ZAKO2_PROJECTILE_VECTOR_DAMPING_SHIFT);
        int32_t velocity_z = zako2_asr64((int64_t)AE_ZAKO2_PROJECTILE_VECTOR_DAMPING *
            ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Z), AE_ZAKO2_PROJECTILE_VECTOR_DAMPING_SHIFT);
        velocity_x += zako2_s8(ae_rd16(r, output + AE_ZAKO2_VECTOR_COMPONENT_X_OFFSET) >> 8);
        velocity_y += zako2_s8(ae_rd16(r, output + AE_ZAKO2_VECTOR_COMPONENT_Y_OFFSET) >> 8);
        velocity_z += zako2_s8(ae_rd16(r, output + AE_ZAKO2_VECTOR_COMPONENT_Z_OFFSET) >> 8);
        if (velocity_x < -28) velocity_x = -28;
        else if (velocity_x > 28) velocity_x = 28;
        if (velocity_y < -26) velocity_y = -26;
        else if (velocity_y > 26) velocity_y = 26;
        if (velocity_z < -28) velocity_z = -28;
        else if (velocity_z > 28) velocity_z = 28;
        ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_X, (uint32_t)velocity_x);
        ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y, (uint32_t)velocity_y);
        ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Z, (uint32_t)velocity_z);

        uint32_t distance = zako2_distance3_squared(r, projectile,
            AE_ZAKO2_PLAYER_POS_X, AE_ZAKO2_PLAYER_POS_Y, AE_ZAKO2_PLAYER_POS_Z, 128);
        if (zako2_s32(distance) <= (int32_t)AE_ZAKO2_PROJECTILE_HIT_RANGE2)
            ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_FIELD_0F, 1);
        if (zako2_s32(distance) < (int32_t)AE_ZAKO2_PROJECTILE_NEAR_RANGE2)
            ae_ProjectileAddHitBody(r, h, projectile,
                                    AE_ZAKO2_PROJECTILE_TYPE6_BODY_RADIUS,
                                    AE_ZAKO2_PROJECTILE_TYPE6_BODY_PARAM,
                                    AE_ZAKO2_PROJECTILE_TYPE6_BODY_FLAGS);
    }
    return 0;
}

static int32_t zako2_div4_round_negative(int32_t value) {
    if (value < 0) value += 3;
    return zako2_asr64(value, 2);
}

static void zako2_projectile_bounce_vertical(AeRam *r, const AeZako2Hooks *h,
                                             uint32_t projectile, int32_t velocity_y) {
    int32_t bounced_velocity = -velocity_y / 2;
    ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y, (uint32_t)bounced_velocity);
    if (bounced_velocity < -10)
        zako2_spawn_particle(r, h, AE_ZAKO2_PARTICLE_SPARK,
                             projectile + AE_ZAKO2_PROJECTILE_POS_X);
}

static void zako2_projectile_update_type7(AeRam *r, const AeZako2Hooks *h,
                                          uint32_t projectile) {
    zako2_move_projectile(r, projectile);
    uint32_t timer = ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_TIMER);
    if (timer >= AE_ZAKO2_PROJECTILE_TYPE7_BOUNCE_TIME) {
        uint32_t velocity_bits = (ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y) + 8u) & 0xFFFFu;
        int32_t velocity_y = zako2_s16(velocity_bits);
        ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y, (uint32_t)velocity_y);
        if (ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y) >=
            ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_GROUND_Y))
            zako2_projectile_bounce_vertical(r, h, projectile, velocity_y);
        return;
    }

    uint32_t velocity_bits = (ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y) +
                              AE_ZAKO2_PROJECTILE_GRAVITY_TYPE7) & 0xFFFFu;
    int32_t velocity_y = zako2_s16(velocity_bits);
    if (velocity_y > AE_ZAKO2_PROJECTILE_TERMINAL_TYPE7)
        velocity_y = AE_ZAKO2_PROJECTILE_TERMINAL_TYPE7;
    ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y, (uint32_t)velocity_y);
    if (velocity_y > 0) {
        int32_t ground = zako2_ground_height(r, h, projectile);
        if (ae_ram_failed(r)) return;
        ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_GROUND_Y, (uint32_t)ground);
        if (ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y) >= ground) {
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y, (uint32_t)ground);
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y,
                    (uint32_t)AE_ZAKO2_PROJECTILE_BOUNCE_TYPE7);
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_X,
                    (uint32_t)zako2_div4_round_negative(ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_X)));
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Z,
                    (uint32_t)zako2_div4_round_negative(ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Z)));
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_TIMER,
                    AE_ZAKO2_PROJECTILE_TYPE7_BOUNCE_TIME);
            zako2_spawn_particle(r, h, AE_ZAKO2_PARTICLE_SPARK,
                                 projectile + AE_ZAKO2_PROJECTILE_POS_X);
        }
    }
    if (ae_ram_failed(r)) return;

    if (zako2_body_contact_kind(r, projectile) == AE_ZAKO2_PROJECTILE_CONTACT_KIND) {
        ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_X,
                (uint32_t)zako2_div4_round_negative(-ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_X)));
        ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y,
                (uint32_t)AE_ZAKO2_PROJECTILE_HIT_BOUNCE_Y);
        ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Z,
                (uint32_t)zako2_div4_round_negative(-ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Z)));
        ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_TIMER,
                AE_ZAKO2_PROJECTILE_TYPE7_BOUNCE_TIME);
        zako2_spawn_particle(r, h, AE_ZAKO2_PARTICLE_SPARK,
                             projectile + AE_ZAKO2_PROJECTILE_POS_X);
        if (ae_ram_failed(r)) return;
        int32_t ground = zako2_ground_height(r, h, projectile);
        if (ae_ram_failed(r)) return;
        ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_GROUND_Y, (uint32_t)ground);
    }
    if (ae_ram_failed(r)) return;
    uint32_t horizontal_distance = zako2_distance_xz_squared(r, projectile);
    int32_t distance = ae_sdk_SquareRoot0(r, zako2_s32(horizontal_distance));
    if (ae_ram_failed(r) || distance >= (int32_t)AE_ZAKO2_PROJECTILE_NEAR_PLAYER_RANGE) return;
    ae_ProjectileAddHitBody(r, h, projectile,
                            AE_ZAKO2_PROJECTILE_TYPE7_BODY_RADIUS,
                            AE_ZAKO2_PROJECTILE_TYPE7_BODY_PARAM,
                            AE_ZAKO2_PROJECTILE_TYPE7_BODY_FLAGS);
}

static void zako2_projectile_update_type89(AeRam *r, const AeZako2Hooks *h,
                                           uint32_t projectile, uint32_t kind) {
    zako2_move_projectile(r, projectile);
    uint32_t timer = ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_TIMER);
    uint32_t velocity_bits = (ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y) +
                              AE_ZAKO2_PROJECTILE_GRAVITY_TYPE89) & 0xFFFFu;
    int32_t velocity_y = zako2_s16(velocity_bits);
    ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y, (uint32_t)velocity_y);
    if (timer >= AE_ZAKO2_PROJECTILE_TYPE89_BOUNCE_TIME) {
        if (ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y) >=
            ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_GROUND_Y))
            zako2_projectile_bounce_vertical(r, h, projectile, velocity_y);
        return;
    }

    if (velocity_y > 0) {
        int32_t ground = zako2_ground_height(r, h, projectile);
        if (ae_ram_failed(r)) return;
        ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_GROUND_Y, (uint32_t)ground);
        if (ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y) >= ground) {
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y, (uint32_t)ground);
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Y,
                    (uint32_t)AE_ZAKO2_PROJECTILE_BOUNCE_TYPE89);
            if (kind == 8u) {
                ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_X,
                        ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_X) << 2);
                ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Z,
                        ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_VEL_Z) << 2);
            }
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_TIMER,
                    AE_ZAKO2_PROJECTILE_TYPE89_BOUNCE_TIME);
            zako2_spawn_particle(r, h, AE_ZAKO2_PARTICLE_BOUNCE,
                                 projectile + AE_ZAKO2_PROJECTILE_POS_X);
        }
    } else {
        return;
    }
    if (ae_ram_failed(r)) return;

    uint32_t horizontal_distance = zako2_distance_xz_squared(r, projectile);
    int32_t distance = ae_sdk_SquareRoot0(r, zako2_s32(horizontal_distance));
    if (ae_ram_failed(r) || distance >= (int32_t)AE_ZAKO2_PROJECTILE_NEAR_PLAYER_RANGE) return;

    ae_ProjectileAddHitBody(r, h, projectile,
                            AE_ZAKO2_PROJECTILE_TYPE89_BODY_RADIUS,
                            AE_ZAKO2_PROJECTILE_TYPE89_BODY_PARAM,
                            AE_ZAKO2_PROJECTILE_TYPE89_BODY_FLAGS);
}

void ae_ProjectilesUpdate(AeRam *r, const AeZako2Hooks *h) {
    for (uint32_t index = 0; index < AE_ZAKO2_PROJECTILE_COUNT; ++index) {
        uint32_t projectile = zako2_projectile(index);
        uint32_t kind = ae_rd8(r, projectile + AE_ZAKO2_PROJECTILE_KIND);
        if (kind == 0) continue;

        uint32_t cooldown = ae_rd8(r, projectile + AE_ZAKO2_PROJECTILE_COOLDOWN);
        if (cooldown != 0) {
            ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_COOLDOWN, cooldown - 1u);
            ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_AGE, 1);
            continue;
        }
        ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_AGE, 0);
        if (kind > 10u) continue;

        if (kind <= 2u) {
            uint32_t source = ae_rd32(r, projectile + AE_ZAKO2_PROJECTILE_SOURCE);
            uint32_t source_kind = ae_rd8(r, source);
            if (source_kind == 1u || source_kind == 2u)
                ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_KIND, 0);
            continue;
        }

        if (kind == 3u || kind == 4u) {
            uint32_t timer = ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_TIMER);
            if (timer == 0) {
                ae_ProjectileAddHitBody(r, h, projectile, 200,
                    kind == 3u ? 0 : 1, kind == 3u ? 258 : 513);
            }
            if (timer == 1)
                ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_KIND, 0);
        } else if (kind == 5u) {
            zako2_projectile_update_type5(r, h, projectile);
        } else if (kind == 6u) {
            int32_t exploded = zako2_projectile_update_type6(r, h, projectile);
            if (ae_ram_failed(r) || exploded) continue;
        } else if (kind == 7u) {
            zako2_projectile_update_type7(r, h, projectile);
        } else if (kind == 8u || kind == 9u) {
            zako2_projectile_update_type89(r, h, projectile, kind);
        } else {
            uint32_t source = ae_rd32(r, projectile + AE_ZAKO2_PROJECTILE_SOURCE);
            uint32_t source_kind = ae_rd8(r, source);
            if (source_kind == 1u || source_kind == 2u) {
                ae_wr8(r, projectile + AE_ZAKO2_PROJECTILE_KIND, 0);
                zako2_spawn_particle(r, h, AE_ZAKO2_PARTICLE_BURST,
                                     projectile + AE_ZAKO2_PROJECTILE_POS_X);
                if (ae_ram_failed(r)) return;
                ae_CameraShake(r, 0);
                (void)zako2_hook(r, h, h ? h->PlaySfx : NULL, "PlaySfx",
                                 AE_ZAKO2_PROJECTILE_DEFAULT_SOUND, 0, 0, 0);
            }
            continue;
        }
        if (ae_ram_failed(r)) return;

        if (kind != 5u && kind != 6u)
            ae_wr16(r, projectile + AE_ZAKO2_PROJECTILE_TIMER,
                    ae_rd16(r, projectile + AE_ZAKO2_PROJECTILE_TIMER) + 1u);
    }
}

static int32_t zako2_hook(AeRam *r, const AeZako2Hooks *h, AeZako2CallHook hook,
                          const char *name, uint32_t a0, uint32_t a1,
                          uint32_t a2, uint32_t a3) {
    if (!h || !hook) {
        ae_ram_fail(r, "zako2 hook %s not provided", name);
        return 0;
    }
    int32_t result = hook(h->user, r, a0, a1, a2, a3);
    return ae_ram_failed(r) ? 0 : result;
}

void ae_FUN_8013f660(AeRam *r, const AeZako2Hooks *h, uint32_t object) {
    (void)zako2_hook(r, h, h ? h->FUN_8013f660 : NULL, "FUN_8013F660",
                     object, 0, 0, 0);
}

static void zako2_render_hook(AeRam *r, const AeZako2Hooks *h,
                              AeZako2RenderHook hook, const char *name,
                              uint32_t a0, uint32_t a1, uint32_t a2,
                              uint32_t a3, uint32_t a4, uint32_t a5) {
    if (!h || !hook) {
        ae_ram_fail(r, "zako2 renderer hook %s not provided", name);
        return;
    }
    (void)hook(h->user, r, a0, a1, a2, a3, a4, a5);
}

void ae_ProjectileAddHitBody(AeRam *r, const AeZako2Hooks *h,
                             uint32_t projectile, int32_t dx,
                             int32_t dy, int32_t dz) {
    uint32_t body = ae_CollisionBodyAlloc(r);
    if (ae_ram_failed(r)) return;

    ae_wr32(r, projectile + AE_ZAKO2_PROJECTILE_BODY, body);
    if (body == 0) return;

    ae_wr32(r, body + AE_ZAKO2_BODY_POS_X,
            (uint32_t)ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_X));
    ae_wr32(r, body + AE_ZAKO2_BODY_POS_Y,
            (uint32_t)(ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Y) - 64));
    ae_wr32(r, body + AE_ZAKO2_BODY_POS_Z,
            (uint32_t)ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_POS_Z));
    ae_wr16(r, body + AE_ZAKO2_BODY_RADIUS, (uint32_t)dx);
    ae_wr16(r, body + AE_ZAKO2_BODY_PARAM, (uint32_t)dy);
    ae_wr16(r, body + AE_ZAKO2_BODY_FLAGS, (uint32_t)dz);

    if (!h || !h->body_hooks) {
        ae_ram_fail(r, "zako2 body hooks not provided");
        return;
    }
    (void)ae_CollisionBodyRegister(r, h->body_hooks, 6, body,
                                   ae_rd32(r, projectile + AE_ZAKO2_PROJECTILE_OWNER));
}

void ae_ProjectilesUpdateDraw(AeRam *r, const AeZako2Hooks *h) {
    if ((ae_rd32(r, AE_ZAKO2_STAGE_RENDER_FLAGS) & AE_ZAKO2_UPDATE_RENDER_FLAG_MASK) == 0)
        ae_ProjectilesUpdate(r, h);
    if (ae_ram_failed(r)) return;
    ae_ProjectilesDraw(r, h);
}

void ae_ProjectilesDraw(AeRam *r, const AeZako2Hooks *h) {
    uint32_t render_state = AE_G_ZAKO2_RENDER_STATE;
    uint32_t model_matrix = AE_ZAKO2_TEMP_MATRIX;
    uint32_t model_vertices = AE_G_ZAKO2_SCREEN_VERTICES;
    uint32_t camera_matrix = AE_G_ZAKO2_GTE_MATRIX;
    uint32_t ot_index = ae_rd32(r, render_state + AE_ZAKO2_RENDER_OT_INDEX);
    uint32_t model_draw_context = render_state + AE_ZAKO2_RENDER_MODEL_OT +
                                  ot_index * AE_ZAKO2_MODEL_DRAW_CONTEXT_SIZE;

    for (uint32_t index = 0; index < AE_ZAKO2_PROJECTILE_COUNT; ++index) {
        uint32_t projectile = zako2_projectile(index);
        uint32_t kind = ae_rd8(r, projectile + AE_ZAKO2_PROJECTILE_KIND);
        if (kind == 0 || ae_rd8(r, projectile + AE_ZAKO2_PROJECTILE_AGE) != 0 || kind < 5u)
            continue;

        uint32_t position = projectile + AE_ZAKO2_PROJECTILE_POS_X;
        if (kind == 5u) {
            uint32_t primitive = ae_rd32(r, render_state + AE_ZAKO2_RENDER_PRIMITIVE_PTR);
            zako2_render_hook(r, h, h ? h->DrawProjectileSprite : NULL,
                              "DrawProjectileSprite", projectile, position, kind,
                              render_state, primitive, 0);
            if (ae_ram_failed(r)) return;
            continue;
        }

        /* DrawShadow receives the same four-word position copy the original makes on its stack. */
        ae_wr32(r, AE_ZAKO2_TEMP_VECTOR + AE_ZAKO2_STACK_VECTOR_X_OFFSET,
                (uint32_t)ae_rds16(r, position));
        ae_wr32(r, AE_ZAKO2_TEMP_VECTOR + AE_ZAKO2_STACK_VECTOR_Y_OFFSET,
                (uint32_t)ae_rds16(r, position + AE_ZAKO2_VECTOR_Y_OFFSET));
        ae_wr32(r, AE_ZAKO2_TEMP_VECTOR + AE_ZAKO2_STACK_VECTOR_Z_OFFSET,
                (uint32_t)ae_rds16(r, position + AE_ZAKO2_VECTOR_Z_OFFSET));
        ae_wr32(r, AE_ZAKO2_TEMP_VECTOR + AE_ZAKO2_STACK_VECTOR_W_OFFSET, 0);
        (void)zako2_hook(r, h, h ? h->DrawShadow : NULL, "DrawShadow",
                         AE_ZAKO2_TEMP_VECTOR, (uint32_t)ae_rds16(r, projectile + AE_ZAKO2_PROJECTILE_GROUND_Y),
                         32u, 0);
        if (ae_ram_failed(r)) return;

        ae_sdk_MatrixLookDir(r, projectile + AE_ZAKO2_PROJECTILE_VEL_X, model_matrix);
        if (ae_ram_failed(r)) return;
        ae_sdk_CompMatrix(r, camera_matrix, model_matrix, model_matrix);
        if (ae_ram_failed(r)) return;

        uint32_t model_index = ae_rd8(r, projectile + AE_ZAKO2_PROJECTILE_MODEL_INDEX);
        uint32_t model = AE_G_ZAKO2_PROJECTILE_MODEL_TABLE +
                         model_index * AE_ZAKO2_PROJECTILE_MODEL_STRIDE;
        (void)zako2_hook(r, h, h ? h->ModelTransformRigid : NULL,
                         "ModelTransformRigid", model, model_matrix,
                         model_vertices, 0);
        if (ae_ram_failed(r)) return;
        (void)zako2_hook(r, h, h ? h->ModelDraw : NULL, "ModelDraw",
                         model_draw_context, model_vertices, model, 0);
        if (ae_ram_failed(r)) return;
    }
}

int32_t ae_PickupPickKindA(AeRam *r) {
    uint32_t next = (uint16_t)(ae_rd16(r, AE_G_ZAKO2_PICKUP_RANDOM_A) *
                               AE_ZAKO2_PICKUP_LCG_MULTIPLIER + AE_ZAKO2_PICKUP_LCG_INCREMENT);
    ae_wr16(r, AE_G_ZAKO2_PICKUP_RANDOM_A, next);
    if ((ae_rd32(r, AE_G_ZAKO2_GADGET_UNLOCK_MASK) & 8u) == 0) return 3;
    return ((next >> 8) & 1u) != 0 ? 6 : 7;
}

int32_t ae_PickupPickKindB(AeRam *r) {
    uint32_t next = (uint16_t)(ae_rd16(r, AE_G_ZAKO2_PICKUP_RANDOM_B) *
                               AE_ZAKO2_PICKUP_LCG_MULTIPLIER + AE_ZAKO2_PICKUP_LCG_INCREMENT);
    ae_wr16(r, AE_G_ZAKO2_PICKUP_RANDOM_B, next);

    uint32_t random_percent = ((((next >> 8) * 3u) << 3) + (next >> 8)) >>
                              AE_ZAKO2_PICKUP_PERCENT_SHIFT;
    random_percent += 1u;
    if (ae_rd8(r, AE_G_ZAKO2_PICKUP_STATE) != 0) return 5;
    ae_wr8(r, AE_G_ZAKO2_PICKUP_STATE, 9);

    int32_t collected = ae_rd16(r, AE_G_ZAKO2_COLLECTED_PICKUP_COUNT);
    int32_t tier;
    if (collected <= 0) tier = 1;
    else if (collected >= (int32_t)AE_ZAKO2_PICKUP_TIER_THRESHOLD + 1)
        tier = AE_ZAKO2_PICKUP_TIER_THRESHOLD;
    else tier = collected;

    return random_percent < (uint32_t)((tier - 1) * 10) ? 5 : 2;
}

static uint32_t zako2_random_percent(AeRam *r, uint32_t seed_address) {
    uint32_t next = (uint16_t)(ae_rd16(r, seed_address) *
                               AE_ZAKO2_PICKUP_LCG_MULTIPLIER + AE_ZAKO2_PICKUP_LCG_INCREMENT);
    ae_wr16(r, seed_address, next);
    return (((((next >> 8) * 3u) << 3) + (next >> 8)) >>
            AE_ZAKO2_PICKUP_PERCENT_SHIFT) + 1u;
}

void ae_SpawnPickups(AeRam *r, const AeZako2Hooks *h, uint32_t kind,
                     uint32_t position, int32_t mode, uint32_t choose_kind) {
    uint32_t spawn_count = 1;
    uint32_t selected_kind = kind;

    if (choose_kind == 0) {
        if (ae_rd8(r, AE_G_ZAKO2_PICKUP_STATE + AE_ZAKO2_PICKUP_STATE_GATE_OFFSET) == 0 &&
            ae_IsTimeAttack(r))
            selected_kind = 3;
    } else {
        selected_kind = 1;
        uint32_t difficulty = choose_kind == 1
            ? ae_rd8(r, AE_G_ZAKO2_PICKUP_DIFFICULTY) : choose_kind;
        uint32_t weights = AE_G_ZAKO2_PICKUP_WEIGHT_TABLE +
                           difficulty * AE_ZAKO2_PICKUP_WEIGHT_ROW_STRIDE +
                           (uint32_t)ae_rds16(r, AE_G_ZAKO2_PICKUP_HEALTH_WEIGHT);
        uint8_t cumulative[AE_ZAKO2_PICKUP_WEIGHT_COUNT];
        uint32_t total = 0;
        for (uint32_t i = 0; i < AE_ZAKO2_PICKUP_WEIGHT_COUNT; ++i) {
            total += ae_rd8(r, weights + i * AE_ZAKO2_PICKUP_WEIGHT_ENTRY_STRIDE);
            cumulative[i] = (uint8_t)total;
        }

        uint32_t roll = zako2_random_percent(r, AE_G_ZAKO2_PICKUP_RANDOM_B);
        uint32_t bucket = 0;
        while (bucket < AE_ZAKO2_PICKUP_WEIGHT_COUNT && roll >= cumulative[bucket]) ++bucket;
        if (bucket < AE_ZAKO2_PICKUP_WEIGHT_COUNT) {
            selected_kind = ae_rd8(r, AE_G_ZAKO2_PICKUP_KIND_TABLE + bucket);
        }

        if (selected_kind == 7u) {
            selected_kind = (uint32_t)ae_PickupPickKindA(r);
        } else if (selected_kind == 2u) {
            selected_kind = (uint32_t)ae_PickupPickKindB(r);
        } else if (selected_kind == 3u) {
            uint32_t next = (uint16_t)(ae_rd16(r, AE_G_ZAKO2_PICKUP_RANDOM_A) *
                                       AE_ZAKO2_PICKUP_LCG_MULTIPLIER +
                                       AE_ZAKO2_PICKUP_LCG_INCREMENT);
            ae_wr16(r, AE_G_ZAKO2_PICKUP_RANDOM_A, next);
            spawn_count = 2u + ((next >> 8) & 1u);
        }

        uint32_t active = ae_rd8(r, AE_G_ZAKO2_PICKUP_STATE);
        if (active != 0) ae_wr8(r, AE_G_ZAKO2_PICKUP_STATE, active - 1u);
        if (ae_IsTimeAttack(r)) {
            selected_kind = 3;
            spawn_count = 2;
        }
    }

    if (!h || !h->items_hooks) {
        ae_ram_fail(r, "zako2 item hooks not provided");
        return;
    }
    AeItemsInitHooks items = *h->items_hooks;
    items.rsin = zako2_items_rsin;
    items.rcos = zako2_items_rcos;
    while (spawn_count != 0) {
        ae_PickupSpawnSlot(r, &items, selected_kind, position, mode, -1);
        if (ae_ram_failed(r)) return;
        --spawn_count;
    }
}

void ae_ColorFadeSet(AeRam *r, int32_t color, int32_t level) {
    ae_wr32(r, AE_G_ZAKO2_DEBUG_FADE + AE_ZAKO2_DEBUG_COLOR, (uint32_t)color);
    ae_wr32(r, AE_G_ZAKO2_DEBUG_FADE + AE_ZAKO2_DEBUG_FADE_LEVEL, (uint32_t)level);
    ae_wr32(r, AE_G_ZAKO2_DEBUG_FADE + AE_ZAKO2_DEBUG_CURSOR_X, 0);
    ae_wr32(r, AE_G_ZAKO2_DEBUG_FADE + AE_ZAKO2_DEBUG_CURSOR_Y, 0);
}

int32_t ae_IsTimeAttack(AeRam *r) {
    return ae_rd16(r, AE_ZAKO2_STAGE_MODE) == 0x13u ||
           ae_rd16(r, AE_ZAKO2_STAGE_AREA_KIND) == 3u;
}

static uint32_t zako2_vararg(AeRam *r, uint32_t argument_frame, uint32_t index) {
    return ae_rd32(r, argument_frame + AE_ZAKO2_VARARG_FIRST_STACK_OFFSET +
                   index * AE_ZAKO2_VARARG_WORD_SIZE);
}

static int32_t zako2_signed(uint32_t value) {
    if (value <= 0x7FFFFFFFu) return (int32_t)value;
    return (int32_t)((int64_t)value - 0x100000000LL);
}

static void zako2_print_glyph(AeRam *r, const AeZako2Hooks *h, int32_t glyph) {
    ae_DebugTextDrawGlyph(r, h, glyph);
}

void ae_DebugTextDrawGlyph(AeRam *r, const AeZako2Hooks *h, int32_t glyph) {
    uint32_t cursor_x_address = AE_G_ZAKO2_DEBUG_FADE + AE_ZAKO2_DEBUG_CURSOR_X;
    uint32_t cursor_y_address = AE_G_ZAKO2_DEBUG_FADE + AE_ZAKO2_DEBUG_CURSOR_Y;
    uint32_t cursor_x = ae_rd32(r, cursor_x_address);
    uint32_t cursor_y = ae_rd32(r, cursor_y_address);
    uint32_t color = ae_rd32(r, AE_G_ZAKO2_DEBUG_DRAW_COLOR);
    uint32_t packet = ae_rd32(r, AE_G_ZAKO2_RENDER_STATE + AE_ZAKO2_RENDER_PRIMITIVE_PTR);
    uint32_t glyph_data = AE_G_ZAKO2_DEBUG_FONT_GLYPH_TABLE +
                          (uint32_t)glyph * AE_ZAKO2_DEBUG_FONT_GLYPH_STRIDE;
    uint32_t primitive_count = ae_rd32(r, AE_G_ZAKO2_DEBUG_PRIMITIVE_COUNT);

    if (primitive_count < 4u) {
        if (glyph == 0) {
            ae_wr32(r, cursor_x_address, cursor_x + 8u);
            return;
        }
        uint32_t ot_index = ae_rd32(r, AE_G_ZAKO2_RENDER_STATE + AE_ZAKO2_RENDER_OT_INDEX);
        uint32_t ot = AE_G_ZAKO2_DEBUG_OT + ot_index * AE_ZAKO2_MODEL_DRAW_CONTEXT_SIZE;
        zako2_render_hook(r, h, h ? h->DrawDebugGlyph : NULL, "DrawDebugGlyph",
                          (uint32_t)glyph, cursor_x, cursor_y, color,
                          ot, packet);
        if (ae_ram_failed(r)) return;
        ae_wr32(r, cursor_x_address, cursor_x + 8u);
        ae_wr32(r, AE_G_ZAKO2_RENDER_STATE + AE_ZAKO2_RENDER_PRIMITIVE_PTR,
                packet + AE_ZAKO2_GLYPH_PACKET_SIZE_A);
    } else {
        uint32_t advance = ae_rd8(r, glyph_data + AE_ZAKO2_DEBUG_FONT_GLYPH_WIDTH_OFFSET);
        zako2_render_hook(r, h, h ? h->DrawDebugGlyph : NULL, "DrawDebugGlyph",
                          (uint32_t)glyph, cursor_x, cursor_y, color,
                          glyph_data, packet);
        if (ae_ram_failed(r)) return;
        ae_wr32(r, cursor_x_address, cursor_x + advance);
        ae_wr32(r, AE_G_ZAKO2_RENDER_STATE + AE_ZAKO2_RENDER_PRIMITIVE_PTR,
                packet + AE_ZAKO2_GLYPH_PACKET_SIZE_B);
    }
}

void ae_DebugTextPrintf(AeRam *r, const AeZako2Hooks *h,
                        uint32_t format, uint32_t argument_frame) {
    if (ae_rds16(r, AE_G_ZAKO2_DEBUG_TEXT_ENABLED) == 0) return;

    ae_wr32(r, AE_G_ZAKO2_DEBUG_TEXT_MAX_WIDTH, 31u);
    ae_wr32(r, AE_G_ZAKO2_DEBUG_PRIMITIVE_COUNT, 0);

    uint32_t format_index = 0;
    uint32_t argument_index = 0;
    uint32_t width = 0;
    uint32_t format_steps = 0;
    while (format_steps++ < 0x10000u) {
        int32_t ch = ae_rds8(r, format + format_index);
        if (ae_ram_failed(r) || ch == 0) break;
        ++format_index;

        if (ch == '\n') {
            ae_wr32(r, AE_G_ZAKO2_DEBUG_FADE + AE_ZAKO2_DEBUG_CURSOR_X, 0);
            ae_wr32(r, AE_G_ZAKO2_DEBUG_FADE + AE_ZAKO2_DEBUG_CURSOR_Y,
                    ae_rd32(r, AE_G_ZAKO2_DEBUG_FADE + AE_ZAKO2_DEBUG_CURSOR_Y) + 1u);
            continue;
        }

        if (ch == '`') {
            int32_t color_code = ae_rds8(r, format + format_index);
            if (ae_ram_failed(r)) break;
            ae_wr32(r, AE_G_ZAKO2_DEBUG_PRIMITIVE_COUNT,
                    (uint32_t)(color_code - '0'));
            ++format_index;
            continue;
        }

        if (ch != '%') {
            zako2_print_glyph(r, h, ch - 32);
            if (ae_ram_failed(r)) return;
            continue;
        }

        int32_t specifier = ae_rds8(r, format + format_index);
        if (specifier == 0) break;
        if (specifier >= '1' && specifier <= '9') {
            width = (uint32_t)(specifier - '0');
            ++format_index;
            specifier = ae_rds8(r, format + format_index);
            if (specifier == 0) break;
        }
        ++format_index;

        if (specifier == 'd') {
            uint32_t value_bits = zako2_vararg(r, argument_frame, argument_index++);
            int32_t signed_value = zako2_signed(value_bits);
            int32_t quotient = signed_value < 0
                ? (int32_t)(0u - value_bits) : signed_value;
            uint8_t digits[12];
            uint32_t count = 0;
            do {
                int32_t digit = quotient % 10;
                quotient /= 10;
                digits[count++] = (uint8_t)(digit + 16);
            } while (quotient != 0 && count < sizeof(digits));
            if (signed_value < 0) digits[count++] = 13u;

            if (width != 0 && width > count) {
                uint32_t pad = (width - count) * 8u;
                ae_wr32(r, AE_G_ZAKO2_DEBUG_FADE + AE_ZAKO2_DEBUG_CURSOR_X,
                        ae_rd32(r, AE_G_ZAKO2_DEBUG_FADE + AE_ZAKO2_DEBUG_CURSOR_X) + pad);
            }
            while (count != 0) {
                zako2_print_glyph(r, h, digits[--count]);
                if (ae_ram_failed(r)) return;
            }
            width = 0;
        } else if (specifier == 'c') {
            uint32_t value = zako2_vararg(r, argument_frame, argument_index++);
            zako2_print_glyph(r, h, zako2_signed(value - 32u));
            if (ae_ram_failed(r)) return;
        } else if (specifier == 'x') {
            uint32_t value = zako2_vararg(r, argument_frame, argument_index++);
            uint8_t digits[8];
            uint32_t count = 0;
            do {
                uint32_t nibble = value & 0xFu;
                digits[count++] = (uint8_t)(nibble < 10u ? nibble + 16u : nibble + 23u);
                value >>= 4;
            } while (value != 0 && count < sizeof(digits));
            while (count != 0) {
                zako2_print_glyph(r, h, digits[--count]);
                if (ae_ram_failed(r)) return;
            }
        } else if (specifier == 's') {
            uint32_t string = zako2_vararg(r, argument_frame, argument_index++);
            uint32_t string_steps = 0;
            for (;;) {
                int32_t item = ae_rds8(r, string);
                if (ae_ram_failed(r) || item == 0 || string_steps++ >= 0x10000u) break;
                if (item == '`') {
                    ++string;
                    int32_t color_code = ae_rds8(r, string);
                    if (ae_ram_failed(r)) break;
                    ae_wr32(r, AE_G_ZAKO2_DEBUG_PRIMITIVE_COUNT,
                            (uint32_t)(color_code - '0'));
                    ++string;
                    item = ae_rds8(r, string);
                    if (ae_ram_failed(r)) break;
                }
                ++string;
                zako2_print_glyph(r, h, item - 32);
                if (ae_ram_failed(r)) return;
            }
        }
    }

    uint32_t ot_index = ae_rd32(r, AE_G_ZAKO2_RENDER_STATE + AE_ZAKO2_RENDER_OT_INDEX);
    uint32_t ot = AE_G_ZAKO2_DEBUG_FINAL_OT + ot_index * AE_ZAKO2_MODEL_DRAW_CONTEXT_SIZE;
    uint32_t packet = ae_rd32(r, AE_G_ZAKO2_RENDER_STATE + AE_ZAKO2_RENDER_PRIMITIVE_PTR);
    zako2_render_hook(r, h, h ? h->FinishDebugText : NULL, "FinishDebugText",
                      ot, packet,
                      ae_rd32(r, AE_G_ZAKO2_DEBUG_FADE + AE_ZAKO2_DEBUG_COLOR),
                      ae_rd32(r, AE_G_ZAKO2_DEBUG_FADE + AE_ZAKO2_DEBUG_FADE_LEVEL),
                      ae_rd32(r, AE_G_ZAKO2_DEBUG_DRAW_COLOR),
                      ae_rd32(r, AE_G_ZAKO2_DEBUG_PRIMITIVE_COUNT));
    if (ae_ram_failed(r)) return;
    ae_wr32(r, AE_G_ZAKO2_RENDER_STATE + AE_ZAKO2_RENDER_PRIMITIVE_PTR,
            packet + AE_ZAKO2_TEXT_FINAL_PACKET_SIZE);
}

void ae_DebugTextPrintfWhite(AeRam *r, const AeZako2Hooks *h,
                             uint32_t format, uint32_t argument_frame) {
    int32_t previous_color = ae_rds16(r, AE_G_ZAKO2_DEBUG_TEXT_ENABLED);
    ae_wr16(r, AE_G_ZAKO2_DEBUG_TEXT_ENABLED, 1u);
    ae_DebugTextPrintf(r, h, format, argument_frame);
    if (ae_ram_failed(r)) return;
    ae_ColorFadeSet(r, 0, 0);
    ae_wr16(r, AE_G_ZAKO2_DEBUG_TEXT_ENABLED, (uint32_t)previous_color);
}
