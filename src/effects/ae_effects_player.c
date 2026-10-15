/* Player-facing effects that compose existing particle and sound ports. */
#include "ae_effects.h"
#include "../player/ae_player_spawn.h"

#ifdef AE_EFFECTS_TEST
int32_t effects_test_play_sfx(AeRam *r, const AeSoundHw *hw, int32_t id);
#define EFFECTS_PLAY_SFX effects_test_play_sfx
#else
#define EFFECTS_PLAY_SFX ae_PlaySfx
#endif

static int player_effects_position_padding(AeRam *r, const AeSpawnHooks *spawn,
                                           uint32_t local, uint32_t original_function) {
    if (spawn == NULL || spawn->position_padding == NULL) {
        ae_ram_fail(r, "AeSpawnHooks.position_padding not provided");
        return 0;
    }
    uint16_t value = spawn->position_padding(spawn->user, r, original_function);
    ae_wr16(r, local + 6u, value);
    return !ae_ram_failed(r);
}

static uint32_t player_effects_next_seed(AeRam *r) {
    uint32_t seed = (ae_rd16(r, AE_SPAWN_RAND_SEED) * 5u + 1u) & 0xFFFFu;
    ae_wr16(r, AE_SPAWN_RAND_SEED, seed);
    return seed;
}

/* 8006B168: age the player's short trail and periodically place two effect
 * objects, followed by the two persistent trail particles. */
void ae_PlayerFxTrailUpdate(AeRam *r, const AeSpawnHooks *spawn) {
    uint32_t timer = ae_rd32(r, AE_G_PLAYER_FX_TRAIL + AE_PLAYER_FX_TRAIL_TIMER);
    if (timer == 0u) {
        return;
    }

    timer--;
    ae_wr32(r, AE_G_PLAYER_FX_TRAIL + AE_PLAYER_FX_TRAIL_TIMER, timer);

    uint32_t position = AE_G_SPLASH_TEMP_POS;
    for (uint32_t byte = 0; byte < AE_PLAYER_FX_TRAIL_POS_SIZE; ++byte) {
        ae_wr8(r, position + byte, ae_rd8(r, AE_G_PLAYER_FX_TRAIL + byte));
    }

    if ((timer & 3u) == 0u) {
        for (uint32_t which = 0; which < 2u; ++which) {
            uint32_t seed = player_effects_next_seed(r);
            uint32_t y = (ae_rd16(r, AE_G_PLAYER_FX_TRAIL + 2u) - ((seed & 0xFFFFu) >> 9)) & 0xFFFFu;
            ae_wr16(r, position + 2u, y);
            uint32_t definition = which == 0u ? AE_G_PLAYER_FX_DEF_A : AE_G_PLAYER_FX_DEF_B;
            ae_EffectSpawnFromDef(r, spawn, position, definition, 1, 0, 0);
            if (ae_ram_failed(r)) {
                return;
            }
        }
    }

    uint32_t seed = player_effects_next_seed(r);
    uint32_t x_offset = ((seed & 0xFFFFu) >> 8) - 128u;
    ae_wr16(r, position, ae_rd16(r, AE_G_PLAYER_FX_TRAIL) + x_offset);

    seed = player_effects_next_seed(r);
    uint32_t y_offset = ((seed & 0xFFFFu) >> 7) & 0x1FEu;
    ae_wr16(r, position + 2u, ae_rd16(r, AE_G_PLAYER_FX_TRAIL + 2u) - y_offset);

    seed = player_effects_next_seed(r);
    uint32_t z_offset = ((seed & 0xFFFFu) >> 8) - 128u;
    ae_wr16(r, position + 4u, ae_rd16(r, AE_G_PLAYER_FX_TRAIL + 4u) + z_offset);

    ae_SpawnParticle(r, spawn, 5, position);
    ae_SpawnParticle(r, spawn, 0x2B, position);
}

/* 8006B950: queue the splash particle at the player's x/z and the current
 * water surface, choose the shallow/deep impact sound, then close the splash
 * state window. */
void ae_PlayerSplash(AeRam *r, const AeSpawnHooks *spawn, const AeSoundHw *sound) {
    uint32_t position = AE_G_SPLASH_TEMP_POS;
    ae_wr16(r, position, ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POS_X));
    ae_wr16(r, position + 2u, ae_rd16(r, AE_G_SPLASH_WATER_Y));
    ae_wr16(r, position + 4u, ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POS_Z));
    if (!player_effects_position_padding(r, spawn, position, 0x8006B950u)) {
        return;
    }
    ae_SpawnParticle(r, spawn, 0x101, position);
    if (ae_ram_failed(r)) {
        return;
    }

    int32_t sound_id = 0x74;
    uint32_t velocity_y = ae_rd32(r, AE_G_PLAYER + AE_PLAYER_VEL_Y);
    if ((int32_t)velocity_y < 0) {
        velocity_y = 0u - velocity_y;
    }
    if (velocity_y < 0x80000000u && velocity_y >= 17u) {
        uint32_t ground = ae_rd32(r, AE_G_PLAYER + AE_PLAYER_GROUND_Y);
        uint32_t position_y = ae_rd32(r, AE_G_PLAYER + AE_PLAYER_POS_Y);
        uint32_t depth_delta = ground - position_y;
        if (depth_delta < 192u || depth_delta >= 0x80000000u) {
            sound_id = 0x9D;
        }
    }
    (void)EFFECTS_PLAY_SFX(r, sound, sound_id);
    ae_wr8(r, AE_G_SPLASH_STATE + AE_SPLASH_STATE_TIMER, 0x34);
    ae_wr8(r, AE_G_SPLASH_STATE + AE_SPLASH_STATE_ACTIVE, 0);
}

static int32_t player_effects_spawn_random_particle(AeRam *r,
                                                    const AeEffectsHooks *hooks,
                                                    uint32_t position,
                                                    uint32_t a1, uint32_t a2,
                                                    uint32_t scale) {
    if (hooks == NULL || hooks->SpawnRandomParticle == NULL) {
        ae_ram_fail(r, "AeEffectsHooks.SpawnRandomParticle not provided");
        return 0;
    }
    return hooks->SpawnRandomParticle(hooks->user, r, position, a1, a2, scale);
}

static void player_effects_tint_write_random_position(AeRam *r, uint32_t position,
                                                       uint32_t horizontal_radius,
                                                       uint32_t vertical_base,
                                                       uint32_t vertical_shift,
                                                       uint32_t vertical_mask) {
    uint32_t seed = player_effects_next_seed(r);
    uint32_t horizontal = ((seed >> 8) & (horizontal_radius * 2u - 1u)) - horizontal_radius;
    ae_wr16(r, position + AE_EFFECT_POSITION_X,
            ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POS_X) + horizontal);

    seed = player_effects_next_seed(r);
    uint32_t vertical = vertical_base + ((seed & 0xFFFFu) >> vertical_shift & vertical_mask);
    ae_wr16(r, position + AE_EFFECT_POSITION_Y,
            ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POS_Y) - vertical);

    seed = player_effects_next_seed(r);
    horizontal = ((seed >> 8) & (horizontal_radius * 2u - 1u)) - horizontal_radius;
    ae_wr16(r, position + AE_EFFECT_POSITION_Z,
            ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POS_Z) + horizontal);
}

/* 8006C334: age the active tint effect and emit its mode-specific particles. */
void ae_PlayerTintEffectUpdate(AeRam *r, const AeSpawnHooks *spawn,
                               const AeEffectsHooks *hooks) {
    uint32_t state = AE_G_SPLASH_STATE;
    uint32_t timer = ae_rd16(r, state + AE_SPLASH_STATE_TINT_TIMER);
    if (timer == 0u) {
        ae_wr32(r, AE_G_EFFECTS_TINT_CACHE, 0x00FFFFFFu);
        return;
    }

    timer--;
    ae_wr16(r, state + AE_SPLASH_STATE_TINT_TIMER, timer);
    uint32_t mode = ae_rd8(r, state + AE_SPLASH_STATE_MODE);
    uint32_t position = AE_G_SPLASH_TEMP_POS;

    if (mode == AE_PLAYER_TINT_MODE_TRAIL) {
        for (uint32_t particle = 0; particle < 2u; ++particle) {
            player_effects_tint_write_random_position(r, position, 64u, 64u, 10u, 0x3Fu);
            ae_SpawnParticle(r, spawn, 0x24, position);
            if (ae_ram_failed(r)) return;
            ae_SpawnParticle(r, spawn, 3, position);
            if (ae_ram_failed(r)) return;
        }
        return;
    }

    if (mode == AE_PLAYER_TINT_MODE_BUBBLES) {
        player_effects_tint_write_random_position(r, position, 32u, 32u, 8u, 0x3Fu);
        ae_SpawnParticle(r, spawn, 2, position);
        if (ae_ram_failed(r)) return;

        ae_SpawnParticle(r, spawn, (timer & 1u) != 0u ? 7 : 0x1C, position);
        if (ae_ram_failed(r)) return;

        if ((timer & 3u) == 0u) {
            ae_SpriteEffectQueue2(r, position);
        }
        return;
    }

    if (mode == AE_PLAYER_TINT_MODE_RANDOM && (timer & 3u) == 0u) {
        (void)player_effects_spawn_random_particle(r, hooks,
                                                   AE_G_TINT_RANDOM_PARTICLE_ORIGIN,
                                                   0, 0, 64);
    }
}
