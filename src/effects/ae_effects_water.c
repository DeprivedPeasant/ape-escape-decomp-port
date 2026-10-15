/* Water entry, bubbles, wakes, and splash timing. */
#include "ae_effects.h"
#include "../player/ae_player_spawn.h"
#include "../world/ae_solid.h"

#ifdef AE_EFFECTS_TEST
int32_t effects_test_play_sfx(AeRam *r, const AeSoundHw *sound, int32_t id);
#define EFFECTS_WATER_PLAY_SFX effects_test_play_sfx
#else
#define EFFECTS_WATER_PLAY_SFX ae_PlaySfx
#endif

enum {
    WATER_PLAYER_VELOCITY_X = 0x030,
    WATER_PLAYER_GROUND_Y = 0x13C,
    WATER_PLAYER_GROUND_HEIGHT = 0x12C,
    WATER_PLAYER_SURFACE = 0x138,
    WATER_SPLASH_ACTIVE = 0,
    WATER_SPLASH_TIMER = 1,
    WATER_SPLASH_IN_WATER = 3,
    WATER_SPLASH_SURFACE_Y = 4,
    WATER_SPLASH_TIMER_WINDOW = 6,
    WATER_WATER_HEIGHT_SCRATCH_Y = 10,
    WATER_WATER_HEIGHT_SCRATCH_EXTENT = 16,
    WATER_SPLASH_LOCAL_PADDING = -50,
    WATER_TIMER_LOCAL_PADDING = -42,
    WATER_TRAIL_LOCAL_PADDING = -34
};

static uint32_t water_next_seed(AeRam *r) {
    uint32_t seed = (ae_rd16(r, AE_G_TRAIL_POINT_SEED) * 5u + 1u) & 0xFFFFu;
    ae_wr16(r, AE_G_TRAIL_POINT_SEED, seed);
    return seed;
}

static uint32_t water_next_surface_seed(AeRam *r) {
    uint32_t seed = (ae_rd16(r, AE_G_WATER_RANDOM_SEED) * 5u + 1u) & 0xFFFFu;
    ae_wr16(r, AE_G_WATER_RANDOM_SEED, seed);
    return seed;
}

static int32_t water_signed32(uint32_t value) {
    return value <= 0x7FFFFFFFu ? (int32_t)value : -1 - (int32_t)(~value);
}

static void water_position(AeRam *r, const AeEffectsHooks *hooks, uint32_t position,
                           int32_t stack_offset, uint32_t x, uint32_t y, uint32_t z) {
    ae_wr16(r, position, x);
    ae_wr16(r, position + 2u, y);
    ae_wr16(r, position + 4u, z);
    if (hooks == NULL || hooks->LocalPositionPadding == NULL) {
        ae_ram_fail(r, "AeEffectsHooks.LocalPositionPadding not provided");
        return;
    }
    ae_wr16(r, position + 6u,
            hooks->LocalPositionPadding(hooks->user, r, 0x8006BA2Cu, stack_offset));
}

static void water_spawn_particle(AeRam *r, const AeSpawnHooks *spawn,
                                 const AeEffectsHooks *hooks, int32_t type,
                                 int32_t stack_offset, uint32_t x, uint32_t y, uint32_t z) {
    water_position(r, hooks, AE_G_WATER_TEMP_POSITION, stack_offset, x, y, z);
    if (ae_ram_failed(r)) return;
    ae_SpawnParticle(r, spawn, type, AE_G_WATER_TEMP_POSITION);
}

/* PlayerWaterEffectsUpdate @8006BA2C. The player surface nibble tracks entry
 * and exit, while the splash timers and random seeds drive wakes and bubbles. */
void ae_PlayerWaterEffectsUpdate(AeRam *r, const AeSpawnHooks *spawn,
                                 const AeEffectsHooks *hooks) {
    uint32_t player = AE_G_PLAYER;
    uint32_t splash = AE_G_SPLASH_STATE;
    uint32_t player_surface = (ae_rd16(r, player + WATER_PLAYER_SURFACE) >> 12) ^ 1u;
    uint32_t in_water = player_surface == 0u;
    uint32_t previous_water = ae_rd8(r, splash + WATER_SPLASH_IN_WATER);

    if (in_water != previous_water && in_water != 0u) {
        (void)ae_GetSurfaceType(r, ae_rds32(r, player + AE_PLAYER_POS_X),
                                ae_rds32(r, player + AE_PLAYER_POS_Y),
                                ae_rds32(r, player + AE_PLAYER_POS_Z));
        uint32_t surface_y = ae_rd16(r, AE_G_WATER_SURFACE_SCRATCH + WATER_WATER_HEIGHT_SCRATCH_Y) -
                             ae_rd16(r, AE_G_WATER_SURFACE_SCRATCH + WATER_WATER_HEIGHT_SCRATCH_EXTENT);
        ae_wr16(r, splash + WATER_SPLASH_SURFACE_Y, surface_y);

        uint32_t velocity_y = ae_rd32(r, player + AE_PLAYER_VEL_Y);
        if (water_signed32(velocity_y) < 0) velocity_y = 0u - velocity_y;
        /* 8006BAC0 is the velocity branch's delay slot: update the shared
         * water height even when the entry is too slow to emit a splash. */
        ae_wr16(r, AE_G_WATER_ENTRY_EFFECT_POSITION, surface_y);
        if (water_signed32(velocity_y) >= 17) {
            uint32_t depth_delta = ae_rd32(r, player + WATER_PLAYER_GROUND_HEIGHT) -
                                   ae_rd32(r, player + AE_PLAYER_POS_Y);
            if (water_signed32(depth_delta) < 144 && water_signed32(depth_delta) >= 49) {
                water_position(r, hooks, AE_G_WATER_TEMP_POSITION, WATER_SPLASH_LOCAL_PADDING,
                               ae_rd16(r, player + AE_PLAYER_POS_X), surface_y,
                               ae_rd16(r, player + AE_PLAYER_POS_Z));
                if (ae_ram_failed(r)) return;
                ae_SpawnParticle(r, spawn, 0x101, AE_G_WATER_TEMP_POSITION);
                if (ae_ram_failed(r)) return;
                (void)EFFECTS_WATER_PLAY_SFX(r, hooks != NULL ? hooks->sound : NULL, 0x9D);
                ae_wr8(r, splash + WATER_SPLASH_TIMER, 0);
            }
        }
    }
    ae_wr8(r, splash + WATER_SPLASH_IN_WATER, in_water);

    if (ae_rd8(r, AE_G_WATER_PLAYER_SPECIAL_STATE) == 1u) return;

    uint32_t splash_active = ae_rd8(r, splash + WATER_SPLASH_ACTIVE);
    if (splash_active != 0u) {
        ae_wr8(r, splash + WATER_SPLASH_ACTIVE, splash_active - 1u);
        water_spawn_particle(r, spawn, hooks, 18, WATER_SPLASH_LOCAL_PADDING,
                             ae_rd16(r, player + AE_PLAYER_POS_X),
                             ae_rd16(r, player + AE_PLAYER_POS_Y) - 128u,
                             ae_rd16(r, player + AE_PLAYER_POS_Z));
        if (ae_ram_failed(r)) return;
        water_spawn_particle(r, spawn, hooks, 21, WATER_SPLASH_LOCAL_PADDING,
                             ae_rd16(r, player + AE_PLAYER_POS_X),
                             ae_rd16(r, player + AE_PLAYER_POS_Y) - 128u,
                             ae_rd16(r, player + AE_PLAYER_POS_Z));
        if (ae_ram_failed(r)) return;
    }

    uint32_t splash_timer = ae_rd8(r, splash + WATER_SPLASH_TIMER);
    if (splash_timer != 0u) {
        uint32_t player_class = ae_rd32(r, AE_G_WATER_PLAYER_FLAGS) & 0xF0u;
        uint32_t particle_count = player_class != 0x40u && splash_timer >= 10u ? 3u : 1u;
        while (particle_count-- != 0u) {
            uint32_t seed = water_next_seed(r);
            uint32_t x = ae_rd16(r, player + AE_PLAYER_POS_X) + ((seed >> 8) & 0x7Fu) - 63u;
            seed = water_next_seed(r);
            uint32_t y = ae_rd16(r, player + AE_PLAYER_POS_Y) - ((seed >> 8) & 0x7Fu);
            seed = water_next_seed(r);
            uint32_t z = ae_rd16(r, player + AE_PLAYER_POS_Z) + ((seed >> 8) & 0x7Fu) - 63u;
            water_spawn_particle(r, spawn, hooks, 17, WATER_TIMER_LOCAL_PADDING, x, y, z);
            if (ae_ram_failed(r)) return;
        }
        ae_wr8(r, splash + WATER_SPLASH_TIMER, splash_timer - 1u);
    }

    if (ae_rd8(r, splash + WATER_SPLASH_IN_WATER) == 0u) return;

    uint32_t horizontal_motion = ae_rd32(r, player + WATER_PLAYER_VELOCITY_X) != 0u ||
                                 ae_rd32(r, player + WATER_PLAYER_VELOCITY_X + 8u) != 0u;
    int32_t player_y = ae_rds32(r, player + AE_PLAYER_POS_Y);
    int32_t ground_y = ae_rds16(r, player + WATER_PLAYER_GROUND_Y);
    if (water_signed32((uint32_t)player_y - 256u) < ground_y &&
        ground_y < water_signed32((uint32_t)player_y - 64u)) {
        uint32_t player_class = ae_rd32(r, AE_G_WATER_PLAYER_FLAGS) & 0xF0u;
        int32_t velocity_y = ae_rds32(r, player + AE_PLAYER_VEL_Y);
        if (player_class == 0x40u && velocity_y < 0 &&
            ae_rd8(r, splash + WATER_SPLASH_TIMER) == 0u &&
            water_signed32((uint32_t)player_y - (uint32_t)ground_y) < 200) {
            ae_PlayerSplash(r, spawn, hooks != NULL ? hooks->sound : NULL);
            if (ae_ram_failed(r)) return;
        }

        if ((ae_rd32(r, AE_G_WATER_FRAME_FLAGS) & 1u) != 0u) return;
        water_position(r, hooks, AE_G_WATER_TEMP_POSITION, WATER_TRAIL_LOCAL_PADDING,
                       ae_rd16(r, player + AE_PLAYER_POS_X),
                       ae_rd16(r, player + WATER_PLAYER_GROUND_Y),
                       ae_rd16(r, player + AE_PLAYER_POS_Z));
        if (ae_ram_failed(r)) return;
        ae_TrailPointAdd(r, AE_G_WATER_TEMP_POSITION);

        uint32_t seed = water_next_seed(r);
        if (((seed >> 8) & 3u) == 0u) {
            ae_EffectSpawnFromDef(r, spawn, AE_G_WATER_TEMP_POSITION,
                                  0x800B1D24u, 1, 0, 0);
            if (ae_ram_failed(r)) return;
        }
        if (horizontal_motion != 0u || ae_rd8(r, splash + WATER_SPLASH_ACTIVE) != 0u) {
            ae_SpawnParticle(r, spawn, 20, AE_G_TINT_RANDOM_PARTICLE_ORIGIN);
            if (ae_ram_failed(r)) return;
        }
    }

    if (water_signed32((uint32_t)player_y - 128u) < ground_y) {
        uint32_t seed = water_next_surface_seed(r);
        if (((seed >> 8) & 0x1Fu) == 0u) {
            water_position(r, hooks, AE_G_WATER_TEMP_POSITION, WATER_TRAIL_LOCAL_PADDING,
                           ae_rd16(r, player + AE_PLAYER_POS_X),
                           ae_rd16(r, splash + WATER_SPLASH_SURFACE_Y),
                           ae_rd16(r, player + AE_PLAYER_POS_Z));
            if (ae_ram_failed(r)) return;
            ae_EffectSpawnFromDef(r, spawn, AE_G_WATER_TEMP_POSITION,
                                  0x800B1D24u, 1, 0, 0);
            if (ae_ram_failed(r)) return;
        }
    }

    if (ground_y < water_signed32((uint32_t)player_y - 128u) &&
        (horizontal_motion != 0u || ae_rd8(r, splash + WATER_SPLASH_ACTIVE) != 0u)) {
        ae_SpawnParticle(r, spawn, 20, AE_G_TINT_RANDOM_PARTICLE_ORIGIN);
    }
}
