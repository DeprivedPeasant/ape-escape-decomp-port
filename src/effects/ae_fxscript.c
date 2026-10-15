/* Readable RAM-image ports for the effect scripts, projectile systems and RC car. */
#include "ae_fxscript.h"

#include "ae_spawn.h"
#include "../bodies/ae_bodies.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "../player/ae_player_spawn.h"
#include "../sound/ae_bgm.h"
#include "../sound/ae_sound.h"
#include "../world/ae_areaset.h"
#include "../world/ae_solid.h"
#include "../world/ae_world.h"

#include <limits.h>

static int32_t fx_s32(uint32_t value) {
    if (value <= INT32_MAX) return (int32_t)value;
    return -1 - (int32_t)(~value);
}

static int32_t fx_s16(uint32_t value) {
    uint32_t low = value & 0xFFFFu;
    return low < 0x8000u ? (int32_t)low : (int32_t)low - 0x10000;
}

static int32_t fx_s8(uint32_t value) {
    uint32_t low = value & 0xFFu;
    return low < 0x80u ? (int32_t)low : (int32_t)low - 0x100;
}

static int32_t fx_asr32(int32_t value, uint32_t shift) {
    shift &= 31u;
    if (shift == 0u || value >= 0) return (int32_t)((uint32_t)value >> shift);
    return (int32_t)~((~(uint32_t)value) >> shift);
}

static int32_t fx_asr64(int64_t value, uint32_t shift) {
    int64_t divisor = (int64_t)1 << shift;
    if (value >= 0) return (int32_t)(value / divisor);
    return (int32_t)-((-value + divisor - 1) / divisor);
}

static int32_t fx_div(AeRam *r, int32_t numerator, int32_t denominator,
                      const char *operation) {
    if (denominator == 0 || (numerator == INT32_MIN && denominator == -1)) {
        ae_ram_fail(r, "%s: original MIPS division trap", operation);
        return 0;
    }
    return numerator / denominator;
}

static int32_t fx_call(AeRam *r, const AeFxscriptHooks *h, AeFxscriptHookFn fn,
                       const char *name, uint32_t a0, uint32_t a1,
                       uint32_t a2, uint32_t a3, uint32_t a4) {
    if (!h || !fn) {
        ae_ram_fail(r, "AeFxscriptHooks.%s not provided", name);
        return 0;
    }
    return fn(h->user, r, a0, a1, a2, a3, a4);
}

#define FX_CALL(r, h, field, a0, a1, a2, a3) \
    fx_call((r), (h), (h) ? (h)->field : NULL, #field, \
            (uint32_t)(a0), (uint32_t)(a1), (uint32_t)(a2), \
            (uint32_t)(a3), 0u)
#define FX_CALL5(r, h, field, a0, a1, a2, a3, a4) \
    fx_call((r), (h), (h) ? (h)->field : NULL, #field, \
            (uint32_t)(a0), (uint32_t)(a1), (uint32_t)(a2), \
            (uint32_t)(a3), (uint32_t)(a4))

static void fx_body_printf(void *user, AeRam *r, uint32_t format, int32_t type) {
    const AeFxscriptHooks *h = (const AeFxscriptHooks *)user;
    (void)FX_CALL(r, h, Printf, format, type, 0, 0);
}

static int32_t fx_body_sound_start(void *user, AeRam *r, int32_t a, int32_t id) {
    const AeFxscriptHooks *h = (const AeFxscriptHooks *)user;
    return ae_SoundLoopStart(r, h ? h->sound : NULL, a, id);
}

static void fx_body_sound_stop(void *user, AeRam *r, int32_t id, int32_t handle) {
    const AeFxscriptHooks *h = (const AeFxscriptHooks *)user;
    (void)FX_CALL(r, h, SoundLoopStop, id, handle, 0, 0);
}

static AeBodyHooks fx_body_hooks(const AeFxscriptHooks *h) {
    AeBodyHooks body = {0};
    body.user = (void *)h;
    body.printf_error = fx_body_printf;
    body.sound_loop_start = fx_body_sound_start;
    body.sound_loop_stop = fx_body_sound_stop;
    return body;
}

static int32_t fx_world_solid(void *user, AeRam *r, uint32_t a0, uint32_t a1,
                              uint32_t a2, uint32_t a3) {
    const AeFxscriptHooks *h = (const AeFxscriptHooks *)user;
    (void)a3;
    return FX_CALL(r, h, GetSolidHeight, a0, a1, a2, 0);
}

static int32_t fx_world_sfx_at(void *user, AeRam *r, uint32_t a0, uint32_t a1,
                               uint32_t a2, uint32_t a3) {
    const AeFxscriptHooks *h = (const AeFxscriptHooks *)user;
    (void)a2;
    (void)a3;
    return ae_PlaySfxAt(r, h ? h->sound : NULL, fx_s32(a0), a1);
}

static int32_t fx_world_gimmick_activate(void *user, AeRam *r, uint32_t a0,
                                         uint32_t a1, uint32_t a2, uint32_t a3) {
    const AeFxscriptHooks *h = (const AeFxscriptHooks *)user;
    return FX_CALL(r, h, GimmickActivate, a0, a1, a2, a3);
}

static int32_t fx_world_mesh_init(void *user, AeRam *r, uint32_t a0, uint32_t a1,
                                  uint32_t a2, uint32_t a3) {
    const AeFxscriptHooks *h = (const AeFxscriptHooks *)user;
    return FX_CALL(r, h, MeshInitFromAreaTable, a0, a1, a2, a3);
}

static int32_t fx_world_overlay(void *user, AeRam *r, uint32_t a0, uint32_t a1,
                                uint32_t a2, uint32_t a3) {
    const AeFxscriptHooks *h = (const AeFxscriptHooks *)user;
    return FX_CALL(r, h, FUN_8013d358, a0, a1, a2, a3);
}

static int32_t fx_world_camera_shake(void *user, AeRam *r, uint32_t a0, uint32_t a1,
                                     uint32_t a2, uint32_t a3) {
    const AeFxscriptHooks *h = (const AeFxscriptHooks *)user;
    (void)a1;
    (void)a2;
    (void)a3;
    return FX_CALL(r, h, CameraShake, a0, 0, 0, 0);
}

static int32_t fx_world_event_cam(void *user, AeRam *r, uint32_t a0, uint32_t a1,
                                  uint32_t a2, uint32_t a3) {
    const AeFxscriptHooks *h = (const AeFxscriptHooks *)user;
    return FX_CALL(r, h, EventCamStart, a0, a1, a2, a3);
}

static AeWorldHooks fx_world_hooks(const AeFxscriptHooks *h) {
    AeWorldHooks world = {0};
    world.user = (void *)h;
    world.GetSolidHeight = fx_world_solid;
    world.PlaySfxAt = fx_world_sfx_at;
    world.GimmickActivate = fx_world_gimmick_activate;
    world.MeshInitFromAreaTable = fx_world_mesh_init;
    world.FUN_8013d358 = fx_world_overlay;
    world.CameraShake = fx_world_camera_shake;
    world.EventCamStart = fx_world_event_cam;
    return world;
}

static int32_t fx_spawn_play_sfx(void *user, AeRam *r, uint32_t a0, uint32_t a1,
                                 uint32_t a2, uint32_t a3) {
    const AeFxscriptHooks *h = (const AeFxscriptHooks *)user;
    (void)a1;
    (void)a2;
    (void)a3;
    return ae_PlaySfx(r, h ? h->sound : NULL, fx_s32(a0));
}

static int32_t fx_play_sfx(AeRam *r, const AeFxscriptHooks *h, uint32_t id) {
    return ae_PlaySfx(r, h ? h->sound : NULL, fx_s32(id));
}

static int32_t fx_spawn_matrix_look(void *user, AeRam *r, uint32_t a0, uint32_t a1,
                                    uint32_t a2, uint32_t a3) {
    (void)user;
    (void)a2;
    (void)a3;
    return (int32_t)ae_sdk_MatrixLookDir(r, a0, a1);
}

static int32_t fx_spawn_matrix_xz(void *user, AeRam *r, uint32_t a0, uint32_t a1,
                                  uint32_t a2, uint32_t a3) {
    (void)user;
    (void)a2;
    (void)a3;
    return (int32_t)ae_sdk_MatrixFromDirXZ(r, a0, a1);
}

static uint16_t fx_spawn_padding(void *user, AeRam *r, uint32_t function) {
    const AeFxscriptHooks *h = (const AeFxscriptHooks *)user;
    if (h && h->position_padding) return h->position_padding(h->user, r, function);
    if (h) return h->stale_spawn_position_pad;
    ae_ram_fail(r, "AeFxscriptHooks.position_padding not provided");
    return 0;
}

static void fx_spawn_hooks(const AeFxscriptHooks *h, AeSpawnHooks *spawn,
                           AeWorldHooks *world) {
    *world = fx_world_hooks(h);
    *spawn = (AeSpawnHooks){0};
    spawn->user = (void *)h;
    spawn->world = world;
    spawn->PlaySfx = fx_spawn_play_sfx;
    spawn->MatrixLookDir = fx_spawn_matrix_look;
    spawn->MatrixFromDirXZ = fx_spawn_matrix_xz;
    spawn->position_padding = fx_spawn_padding;
}

static uint32_t fx_particle_rand16(uint16_t *seed) {
    *seed = (uint16_t)((uint32_t)*seed * 5u + 1u);
    return *seed;
}

static int32_t fx_particle_random_base(uint16_t *seed, int32_t base, int32_t spread) {
    int32_t random = (int32_t)(fx_particle_rand16(seed) >> 8);
    return base + fx_asr64((int64_t)random * spread, 8);
}

static int32_t fx_particle_random_offset(uint16_t *seed, int32_t spread, int32_t bias) {
    int32_t random = (int32_t)(fx_particle_rand16(seed) >> 8);
    return fx_asr64((int64_t)random * spread, 8) - bias;
}

static void fx_set_particle_s16(AeRam *r, uint32_t particle, uint32_t offset,
                                int32_t value) {
    ae_wr16(r, particle + offset, (uint32_t)value);
}

/* 80069920: leave projection and quad/OT packet work to the native renderer. */
void ae_ParticlesDraw(AeRam *r, const AeFxscriptHooks *h) {
    (void)FX_CALL(r, h, SetRotMatrix, AE_FX_VIEW_MATRIX, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    (void)FX_CALL(r, h, SetTransMatrix, AE_FX_VIEW_MATRIX, 0, 0, 0);
    if (ae_ram_failed(r)) return;

    for (uint32_t index = 0; index < AE_FX_PARTICLE_COUNT; ++index) {
        uint32_t particle = AE_FX_PARTICLES + index * AE_FX_PARTICLE_STRIDE;
        uint32_t width = ae_rd8(r, particle + AE_FX_PARTICLE_WIDTH);
        uint32_t height = ae_rd8(r, particle + AE_FX_PARTICLE_HEIGHT);
        if (ae_rd8(r, particle + AE_FX_PARTICLE_TYPE) == 0u || width == 0u) {
            continue;
        }

        uint32_t raw_depth = (uint32_t)FX_CALL(r, h, ProjectEffectPoint,
                      particle + AE_FX_PARTICLE_POSITION, AE_FX_RENDER_SCRATCH, 0, 0);
        if (ae_ram_failed(r)) return;
        uint32_t packed_screen = ae_rd32(r, AE_FX_RENDER_SCRATCH);
        uint32_t screen_x = packed_screen & 0xFFFFu;
        int32_t screen_y = fx_s16(packed_screen >> 16);

        if (((screen_x + 32u) & 0xFFFFu) >= 449u ||
            screen_y < -32 || screen_y >= 273) {
            continue;
        }
        int32_t depth = fx_asr32((int32_t)raw_depth, 2);
        ae_wr32(r, AE_FX_RENDER_SCRATCH + 4u, (uint32_t)depth);
        if (((uint32_t)depth - 1u) >= 4095u) continue;

        uint32_t scaled_width = (328u * width) / (uint32_t)depth;
        uint32_t height_numerator = (11685u * height) & 0x1FFFFFu;
        uint32_t fixed_height = (uint32_t)(((uint64_t)height_numerator * 0x05B05B33u) >> 32);
        uint32_t scaled_height = fixed_height / (uint32_t)depth;
        uint32_t size_pair = (scaled_width & 0xFFFFu) | ((scaled_height & 0xFFFFu) << 16);

        (void)FX_CALL5(r, h, DrawParticleQuad, particle, screen_x,
                       (uint32_t)screen_y, (uint32_t)depth, size_pair);
        if (ae_ram_failed(r)) return;
    }
}

static void fx_copy8(AeRam *r, uint32_t destination, uint32_t source) {
    uint8_t copy[8];
    uint8_t *input = ae_ptr(r, source, sizeof(copy));
    if (!input) return;
    for (uint32_t i = 0; i < sizeof(copy); ++i) copy[i] = input[i];
    uint8_t *output = ae_ptr(r, destination, sizeof(copy));
    if (!output) return;
    for (uint32_t i = 0; i < sizeof(copy); ++i) output[i] = copy[i];
}

static int32_t fx_script_printf(AeRam *r, const AeFxscriptHooks *h,
                                uint32_t format, uint32_t a1, uint32_t a2) {
    return FX_CALL(r, h, Printf, format, a1, a2, 0);
}

static void fx_report_bad_script(AeRam *r, const AeFxscriptHooks *h,
                                 uint32_t particle, uint32_t command_pc,
                                 int32_t loop_depth_error) {
    uint32_t command = ae_rd16(r, command_pc);
    (void)fx_script_printf(r, h,
        loop_depth_error ? AE_FX_ERROR_FORMAT_LOOP_DEPTH : AE_FX_ERROR_FORMAT_LOOP,
        ae_rd8(r, particle + AE_FX_PARTICLE_TYPE),
        loop_depth_error ? command : 0u);
    if (loop_depth_error) return;
    for (uint32_t word = 0; word < 6u; ++word) {
        (void)fx_script_printf(r, h, AE_FX_ERROR_FORMAT_COMMAND,
                               ae_rd16(r, command_pc + word * 2u), 0u);
    }
    (void)fx_script_printf(r, h, AE_FX_ERROR_FORMAT_END, 0u, 0u);
}

static void fx_clear_local(AeRam *r, const AeFxscriptHooks *h,
                           uint32_t address, uint32_t byte_count) {
    (void)FX_CALL(r, h, ClearMemory, address, 0, byte_count, 0);
}

static int fx_script_ground_or_water(AeRam *r, const AeFxscriptHooks *h,
                                     uint32_t particle, uint32_t *active) {
    AeSpawnHooks spawn;
    AeWorldHooks world;
    fx_spawn_hooks(h, &spawn, &world);

    uint32_t local_position = AE_FX_SCRIPT_LOCAL_GROUND;
    int32_t particle_y = ae_rds16(r, particle + AE_FX_PARTICLE_POSITION + 2u);
    int32_t ground_y = ae_rds32(r, AE_FX_PLAYER + AE_FX_PLAYER_GROUND_HEIGHT);
    if (ground_y < particle_y) {
        fx_clear_local(r, h, local_position + 8u, 8u);
        ae_wr16(r, local_position,
                ae_rd16(r, particle + AE_FX_PARTICLE_POSITION));
        ae_wr16(r, local_position + 2u, (uint32_t)ground_y);
        ae_wr16(r, local_position + 4u,
                ae_rd16(r, particle + AE_FX_PARTICLE_POSITION + 4u));
        if (ae_ram_failed(r)) return 0;
        ae_SpawnParticle(r, &spawn, 0x107, local_position);
        if (!ae_ram_failed(r)) {
            ae_wr8(r, particle + AE_FX_PARTICLE_TYPE, 0);
            *active = 0;
        }
        return 1;
    }

    int32_t water_y = ae_rds16(r, AE_FX_PARTICLE_WATER_HEIGHT);
    if (water_y < particle_y) {
        uint32_t seed = ae_rd16(r, AE_FX_WATER_RANDOM_SEED);
        seed = (seed * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_FX_WATER_RANDOM_SEED, seed);
        uint32_t burst_position = AE_FX_SCRIPT_LOCAL_WATER;
        fx_clear_local(r, h, burst_position + 8u, 8u);
        ae_wr16(r, burst_position,
                ae_rd16(r, particle + AE_FX_PARTICLE_POSITION));
        ae_wr16(r, burst_position + 2u, (uint32_t)water_y);
        ae_wr16(r, burst_position + 4u,
                ae_rd16(r, particle + AE_FX_PARTICLE_POSITION + 4u));
        if (ae_ram_failed(r)) return 0;
        ae_EffectSpawnFromDef(r, &spawn, burst_position,
                              0x800B1E20u, 1, 0, 0);
        if (!ae_ram_failed(r)) {
            ae_wr8(r, particle + AE_FX_PARTICLE_TYPE, 0);
            *active = 0;
        }
        return 1;
    }
    return 0;
}

static int32_t fx_script_command(AeRam *r, const AeFxscriptHooks *h,
                                 uint32_t particle, uint32_t command,
                                 uint32_t *pc, uint16_t *seed,
                                 uint32_t *active) {
    uint16_t opcode = (uint16_t)ae_rd16(r, command);
    uint32_t operand_pc = command + 2u;

#define U16_AT(n) ae_rd16(r, operand_pc + 2u * (n))
#define S16_AT(n) ae_rds16(r, operand_pc + 2u * (n))
#define U8_AT(n) ae_rd8(r, operand_pc + 2u * (n))
#define ADVANCE(bytes) (*pc = operand_pc + (bytes))

    switch (opcode) {
    case 1: case 2: {
        int32_t random = opcode == 2u ? (int32_t)(fx_particle_rand16(seed) >> 8) : 0;
        int32_t depth = fx_s8(ae_rd8(r, particle + AE_FX_PARTICLE_LOOP_DEPTH)) + 1;
        uint32_t body_pointer = operand_pc + (opcode == 1 ? 2u : 4u);
        uint32_t count;
        if (opcode == 1) {
            count = U8_AT(0);
            ADVANCE(2u);
        } else {
            int32_t base = (int32_t)U8_AT(0);
            int32_t spread = S16_AT(1);
            count = (uint32_t)(base + fx_asr64((int64_t)random * spread, 8)) & 0xFFu;
            ADVANCE(4u);
        }
        ae_wr8(r, particle + AE_FX_PARTICLE_LOOP_DEPTH, (uint32_t)depth);
        ae_wr32(r, particle + AE_FX_PARTICLE_LOOP_PC0 + (uint32_t)depth * 4u,
                body_pointer);
        ae_wr8(r, particle + AE_FX_PARTICLE_LOOP_COUNT0 + (uint32_t)depth, count);
        if (depth >= 2) fx_report_bad_script(r, h, particle, command, 1);
        return 0;
    }
    case 3: {
        int32_t depth = fx_s8(ae_rd8(r, particle + AE_FX_PARTICLE_LOOP_DEPTH));
        uint32_t counter = particle + AE_FX_PARTICLE_LOOP_COUNT0 + (uint32_t)depth;
        uint32_t remaining = ae_rd8(r, counter);
        if (remaining != 0u) {
            remaining = (remaining - 1u) & 0xFFu;
            ae_wr8(r, counter, remaining);
        }
        if (remaining != 0u) {
            *pc = ae_rd32(r, particle + AE_FX_PARTICLE_LOOP_PC0 +
                          (uint32_t)(depth * 4));
        } else {
            ae_wr8(r, particle + AE_FX_PARTICLE_LOOP_DEPTH,
                   (uint32_t)(depth - 1));
            ADVANCE(0u);
        }
        return 0;
    }
    case 4:
        ae_wr8(r, particle + AE_FX_PARTICLE_TYPE, 0);
        *active = 0;
        ADVANCE(0u);
        return 1;
    case 8:
        for (uint32_t axis = 0; axis < 3u; ++axis) {
            uint32_t offset = AE_FX_PARTICLE_POSITION + axis * 2u;
            fx_set_particle_s16(r, particle, offset,
                                ae_rds16(r, particle + offset) + S16_AT(axis));
        }
        ADVANCE(6u);
        return 0;
    case 9:
        for (uint32_t axis = 0; axis < 3u; ++axis) {
            int32_t spread = S16_AT(axis * 2u);
            int32_t bias = S16_AT(axis * 2u + 1u);
            int32_t delta = fx_particle_random_offset(seed, spread, bias);
            uint32_t offset = AE_FX_PARTICLE_POSITION + axis * 2u;
            fx_set_particle_s16(r, particle, offset,
                                ae_rds16(r, particle + offset) + delta);
        }
        ADVANCE(12u);
        return 0;
    case 10: case 11: case 13: case 14: {
        uint32_t fixed = opcode == 10u || opcode == 11u;
        int32_t wait;
        if (opcode == 10u || opcode == 13u) wait = S16_AT(0);
        else {
            int32_t base = S16_AT(0);
            int32_t spread = S16_AT(1);
            wait = fx_particle_random_base(seed, base, spread);
        }
        ae_wr8(r, particle + AE_FX_PARTICLE_FLAGS, fixed);
        fx_set_particle_s16(r, particle, AE_FX_PARTICLE_WAIT, wait - 1);
        ADVANCE((opcode == 10u || opcode == 13u) ? 2u : 4u);
        return 1;
    }
    case 12:
        ae_wr8(r, particle + AE_FX_PARTICLE_FLAGS, 0);
        ae_wr16(r, particle + AE_FX_PARTICLE_WAIT, 0);
        ADVANCE(0u);
        return 1;
    case 15: case 19:
        for (uint32_t axis = 0; axis < 3u; ++axis) {
            int32_t velocity;
            if (opcode == 15u) velocity = S16_AT(axis);
            else velocity = fx_particle_random_offset(seed,
                         S16_AT(axis * 2u), S16_AT(axis * 2u + 1u));
            fx_set_particle_s16(r, particle,
                                AE_FX_PARTICLE_VELOCITY + axis * 2u, velocity);
        }
        ADVANCE(opcode == 15u ? 6u : 12u);
        return 0;
    case 16: case 17: case 18:
        fx_set_particle_s16(r, particle,
            AE_FX_PARTICLE_VELOCITY + (uint32_t)(opcode - 16u) * 2u, S16_AT(0));
        ADVANCE(2u);
        return 0;
    case 20: case 21: case 22: {
        uint32_t axis = (uint32_t)(opcode - 20u);
        int32_t velocity = fx_particle_random_offset(seed, S16_AT(0), S16_AT(1));
        fx_set_particle_s16(r, particle, AE_FX_PARTICLE_VELOCITY + axis * 2u, velocity);
        ADVANCE(4u);
        return 0;
    }
    case 23:
        for (uint32_t axis = 0; axis < 3u; ++axis) {
            uint32_t offset = AE_FX_PARTICLE_VELOCITY + axis * 2u;
            fx_set_particle_s16(r, particle, offset,
                                ae_rds16(r, particle + offset) + S16_AT(axis));
        }
        ADVANCE(6u);
        return 0;
    case 24:
        for (uint32_t axis = 0; axis < 3u; ++axis) {
            uint32_t offset = AE_FX_PARTICLE_VELOCITY + axis * 2u;
            int64_t product = (int64_t)ae_rds16(r, particle + offset) * S16_AT(axis);
            fx_set_particle_s16(r, particle, offset, fx_asr64(product, 12));
        }
        ADVANCE(6u);
        return 0;
    case 25: case 26: case 27:
        ae_wr8(r, particle + AE_FX_PARTICLE_ANCHOR, opcode - 25u);
        ADVANCE(0u);
        return 0;
    case 30: {
        uint32_t base = U16_AT(0);
        uint32_t extent = U16_AT(1);
        ae_wr16(r, particle + AE_FX_PARTICLE_UV0, base);
        ae_wr16(r, particle + AE_FX_PARTICLE_UV1, base + (extent & 0xFF00u));
        ae_wr16(r, particle + AE_FX_PARTICLE_UV2, base + (extent & 0x00FFu));
        ae_wr16(r, particle + AE_FX_PARTICLE_UV3, base + extent);
        ADVANCE(4u);
        return 0;
    }
    case 31: case 32: {
        uint32_t component = opcode == 31u ? 0u : 1u;
        uint32_t delta = U8_AT(0);
        static const uint8_t uv_offsets[4] = {
            AE_FX_PARTICLE_UV0, AE_FX_PARTICLE_UV1,
            AE_FX_PARTICLE_UV2, AE_FX_PARTICLE_UV3
        };
        for (uint32_t corner = 0; corner < 4u; ++corner) {
            uint32_t offset = uv_offsets[corner] + component;
            ae_wr8(r, particle + offset,
                   ae_rd8(r, particle + offset) + delta);
        }
        ADVANCE(2u);
        return 0;
    }
    case 33:
        ae_wr8(r, particle + AE_FX_PARTICLE_WIDTH, U8_AT(0));
        ae_wr8(r, particle + AE_FX_PARTICLE_HEIGHT, U8_AT(1));
        ADVANCE(4u);
        return 0;
    case 34:
        ae_wr8(r, particle + AE_FX_PARTICLE_WIDTH,
               (uint32_t)fx_particle_random_base(seed, (int32_t)U8_AT(0), S16_AT(1)));
        ae_wr8(r, particle + AE_FX_PARTICLE_HEIGHT,
               (uint32_t)fx_particle_random_base(seed, (int32_t)U8_AT(2), S16_AT(3)));
        ADVANCE(8u);
        return 0;
    case 35: {
        uint32_t size = (uint32_t)fx_particle_random_base(seed,
                               (int32_t)U8_AT(0), S16_AT(1));
        ae_wr8(r, particle + AE_FX_PARTICLE_WIDTH, size);
        ae_wr8(r, particle + AE_FX_PARTICLE_HEIGHT, size);
        ADVANCE(4u);
        return 0;
    }
    case 36:
        ae_wr8(r, particle + AE_FX_PARTICLE_WIDTH,
               (uint32_t)fx_asr64((int64_t)ae_rd8(r, particle + AE_FX_PARTICLE_WIDTH) *
                                  S16_AT(0), 12));
        ae_wr8(r, particle + AE_FX_PARTICLE_HEIGHT,
               (uint32_t)fx_asr64((int64_t)ae_rd8(r, particle + AE_FX_PARTICLE_HEIGHT) *
                                  S16_AT(1), 12));
        ADVANCE(4u);
        return 0;
    case 37: {
        uint32_t color = ae_rd32(r, particle + AE_FX_PARTICLE_RGB) & 0xFF000000u;
        color |= (uint32_t)S16_AT(0) & 0xFFu;
        color |= ((uint32_t)S16_AT(1) & 0xFFu) << 8;
        color |= ((uint32_t)S16_AT(2) & 0xFFu) << 16;
        ae_wr32(r, particle + AE_FX_PARTICLE_RGB, color);
        ADVANCE(6u);
        return 0;
    }
    case 38: {
        int32_t value = fx_particle_random_base(seed, (int32_t)U8_AT(0), S16_AT(1));
        uint32_t color = ae_rd32(r, particle + AE_FX_PARTICLE_RGB);
        color = (color & 0xFFFF00FFu) | (((uint32_t)value & 0xFFu) << 8);
        ae_wr32(r, particle + AE_FX_PARTICLE_RGB, color);
        ADVANCE(4u);
        return 0;
    }
    case 39:
        ae_wr8(r, particle + AE_FX_PARTICLE_FADE, (uint32_t)S16_AT(0));
        ADVANCE(2u);
        return 0;
    case 40: {
        int32_t value = fx_particle_random_base(seed, (int32_t)U8_AT(0), S16_AT(1));
        ae_wr8(r, particle + AE_FX_PARTICLE_FADE, (uint32_t)value);
        ADVANCE(4u);
        return 0;
    }
    case 41:
        ae_wr8(r, particle + AE_FX_PARTICLE_PRIMITIVE,
               ae_rd8(r, particle + AE_FX_PARTICLE_PRIMITIVE) & 0xFDu);
        ADVANCE(0u);
        return 0;
    case 42: case 43: case 44: case 45:
        ae_wr16(r, particle + AE_FX_PARTICLE_TPAGE,
                28u + ((uint32_t)(opcode - 42u) * 32u));
        ae_wr8(r, particle + AE_FX_PARTICLE_PRIMITIVE,
               ae_rd8(r, particle + AE_FX_PARTICLE_PRIMITIVE) | 2u);
        ADVANCE(0u);
        return 0;
    case 46:
        ae_wr8(r, particle + AE_FX_PARTICLE_PRIMITIVE,
               ae_rd8(r, particle + AE_FX_PARTICLE_PRIMITIVE) & 0xFEu);
        ADVANCE(0u);
        return 0;
    case 47:
        ae_wr8(r, particle + AE_FX_PARTICLE_PRIMITIVE,
               ae_rd8(r, particle + AE_FX_PARTICLE_PRIMITIVE) | 1u);
        ADVANCE(0u);
        return 0;
    case 48: {
        int32_t row = (int32_t)U16_AT(0) + 496;
        ae_wr16(r, particle + AE_FX_PARTICLE_CLUT,
                ((uint32_t)row << 6) | 44u);
        ADVANCE(2u);
        return 0;
    }
    case 49: {
        int32_t row = fx_particle_random_base(seed, S16_AT(0), S16_AT(1)) + 496;
        ae_wr16(r, particle + AE_FX_PARTICLE_CLUT,
                ((uint32_t)row << 6) | 44u);
        ADVANCE(4u);
        return 0;
    }
    case 51:
        ADVANCE(0u);
        return fx_script_ground_or_water(r, h, particle, active);
    case 52: {
        uint32_t table_index = U16_AT(0);
        uint32_t local_points = 0x801FF100u;
        fx_copy8(r, local_points, particle + AE_FX_PARTICLE_POSITION);
        for (uint32_t axis = 0; axis < 3u; ++axis) {
            int32_t position = ae_rds16(r, particle + AE_FX_PARTICLE_POSITION + axis * 2u);
            int32_t velocity = ae_rds16(r, particle + AE_FX_PARTICLE_VELOCITY + axis * 2u);
            ae_wr16(r, local_points + 8u + axis * 2u,
                    (uint32_t)(position + velocity * 3));
        }
        ae_wr16(r, local_points + 14u,
                ae_rd16(r, particle + AE_FX_PARTICLE_POSITION + 6u));
        ADVANCE(2u);
        if (ae_ram_failed(r)) return 1;
        ae_RibbonDraw(r, h, local_points, 2u,
                      AE_FX_RIBBON_COLOR_TABLE + table_index * 8u,
                      ae_rd8(r, particle + AE_FX_PARTICLE_WIDTH));
        return 0;
    }
    default:
        fx_report_bad_script(r, h, particle, command, 0);
        ADVANCE(0u);
        return 1;
    }

#undef U16_AT
#undef S16_AT
#undef U8_AT
#undef ADVANCE
}

/* 80069BEC: draw existing slots, then run each u16 particle script for this frame. */
void ae_EffectScriptUpdate(AeRam *r, const AeFxscriptHooks *h) {
    uint16_t seed = (uint16_t)ae_rd16(r, AE_FX_RANDOM_SEED);
    ae_wr16(r, AE_FX_PARTICLE_ORIGIN,
            ae_rd16(r, AE_FX_PLAYER + AE_FX_PLAYER_POSITION));
    ae_wr16(r, AE_FX_PARTICLE_ORIGIN + 2u,
            ae_rd16(r, AE_FX_PLAYER + AE_FX_PLAYER_POSITION + 4u));
    ae_wr16(r, AE_FX_PARTICLE_ORIGIN + 4u,
            ae_rd16(r, AE_FX_PLAYER + AE_FX_PLAYER_POSITION + 8u));
    ae_wr16(r, AE_FX_PARTICLE_ORIGIN_LOW,
            ae_rd16(r, AE_FX_PLAYER + AE_FX_PLAYER_POSITION));
    ae_wr16(r, AE_FX_PARTICLE_ORIGIN_LOW + 2u,
            ae_rd16(r, AE_FX_PLAYER + AE_FX_PLAYER_POSITION + 4u) - 128u);
    ae_wr16(r, AE_FX_PARTICLE_ORIGIN_LOW + 4u,
            ae_rd16(r, AE_FX_PLAYER + AE_FX_PLAYER_POSITION + 8u));
    if (ae_ram_failed(r)) return;

    ae_ParticlesDraw(r, h);
    if (ae_ram_failed(r)) return;

    for (uint32_t index = 0; index < AE_FX_PARTICLE_COUNT; ++index) {
        uint32_t particle = AE_FX_PARTICLES + index * AE_FX_PARTICLE_STRIDE;
        if (ae_rd8(r, particle + AE_FX_PARTICLE_TYPE) == 0u) continue;

        uint32_t pc = ae_rd32(r, particle + AE_FX_PARTICLE_SCRIPT);
        uint32_t active = 1;
        int32_t wait = ae_rds16(r, particle + AE_FX_PARTICLE_WAIT);
        if (wait > 0) {
            ae_wr16(r, particle + AE_FX_PARTICLE_WAIT, (uint32_t)(wait - 1));
        } else {
            uint32_t command_budget = 0;
            while (active && !ae_ram_failed(r)) {
                if (++command_budget > 100000u) {
                    ae_ram_fail(r, "EffectScriptUpdate: script did not yield");
                    return;
                }
                uint32_t command = pc;
                uint32_t disposition = fx_script_command(r, h, particle, command,
                                                         &pc, &seed, &active);
                if (ae_ram_failed(r) || disposition != 0u) break;
            }
        }

        ae_wr32(r, particle + AE_FX_PARTICLE_SCRIPT, pc);
        if (ae_rd8(r, particle + AE_FX_PARTICLE_TYPE) == 0u || ae_ram_failed(r)) continue;

        if (ae_rd8(r, particle + AE_FX_PARTICLE_FLAGS) == 0u) {
            for (uint32_t axis = 0; axis < 3u; ++axis) {
                uint32_t position = particle + AE_FX_PARTICLE_POSITION + axis * 2u;
                uint32_t velocity = particle + AE_FX_PARTICLE_VELOCITY + axis * 2u;
                ae_wr16(r, position, ae_rd16(r, position) + ae_rd16(r, velocity));
            }
        }

        int32_t fade = fx_s8(ae_rd8(r, particle + AE_FX_PARTICLE_FADE));
        if (fade > 0) {
            for (uint32_t channel = 0; channel < 3u; ++channel) {
                uint32_t address = particle + AE_FX_PARTICLE_RGB + channel;
                int32_t color = (int32_t)ae_rd8(r, address) - fade;
                ae_wr8(r, address, color < 0 ? 0u : (uint32_t)color);
            }
        }
    }
    ae_wr16(r, AE_FX_RANDOM_SEED, seed);
}

/* 8006F3D0: project the strip, derive its screen-space edges, then hand the
 * packet/OT work to the renderer. ProjectEffectPoint writes packed SXY and
 * returns the raw SZ3 value consumed by MFC2 in the original routine. */
static void fx_ribbon_store_vertex(AeRam *r, uint32_t vertex,
                                   uint32_t point_x, uint32_t point_y,
                                   int32_t normal_x, int32_t normal_y,
                                   int32_t half_width) {
    int32_t offset_x = fx_asr64((int64_t)normal_y * half_width, 12);
    int32_t offset_y = fx_asr64((int64_t)normal_x * half_width, 12);
    ae_wr16(r, vertex, ae_rd16(r, point_x) - offset_x);
    ae_wr16(r, vertex + 2u, ae_rd16(r, point_y) + offset_y);
    ae_wr16(r, vertex + 4u, ae_rd16(r, point_x) + offset_x);
    ae_wr16(r, vertex + 6u, ae_rd16(r, point_y) - offset_y);
}

void ae_RibbonDraw(AeRam *r, const AeFxscriptHooks *h, uint32_t points,
                   uint32_t count, uint32_t color_table, uint32_t scale) {
    if ((int32_t)count < 2) return;
    if (count > 16u) count = 16u;

    (void)FX_CALL(r, h, SetRotMatrix, AE_FX_VIEW_MATRIX, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    (void)FX_CALL(r, h, SetTransMatrix, AE_FX_VIEW_MATRIX, 0, 0, 0);
    if (ae_ram_failed(r)) return;

    const uint32_t work = AE_FX_RENDER_SCRATCH;
    uint32_t input_count = count;
    uint32_t projected = 0;
    uint32_t effective_count = count;
    uint32_t previous_packed = 0;
    for (uint32_t i = 0; i < input_count; ++i) {
        uint32_t result = work + 8u;
        uint32_t raw_depth = (uint32_t)FX_CALL(r, h, ProjectEffectPoint,
                                               points + i * 8u, result, 0, 0);
        if (ae_ram_failed(r)) return;
        uint32_t packed = ae_rd32(r, result);
        if (packed == previous_packed) {
            --effective_count;
            if (effective_count < 2u) return;
            continue;
        }
        int32_t depth = fx_asr32((int32_t)raw_depth, 2);
        ae_wr32(r, result + 4u + projected * 4u, (uint32_t)depth);
        ae_wr16(r, AE_FX_RIBBON_PROJECTED_X + projected * 2u, packed);
        ae_wr16(r, AE_FX_RIBBON_PROJECTED_Y + projected * 2u, packed >> 16);
        ae_wr32(r, AE_FX_RIBBON_DEPTH + projected * 4u, (uint32_t)depth);
        previous_packed = packed;
        ++projected;
    }
    count = effective_count;
    if (count < 2u) return;

    const int32_t width_numerator = (int32_t)(164u * scale);
    for (uint32_t i = 0; i < count; ++i) {
        int32_t depth = ae_rds32(r, AE_FX_RIBBON_DEPTH + i * 4u);
        if (depth >= 4096) return;
        uint32_t x = ae_rd16(r, AE_FX_RIBBON_PROJECTED_X + i * 2u);
        int32_t y = ae_rds16(r, AE_FX_RIBBON_PROJECTED_Y + i * 2u);
        if (((x + 32u) & 0xFFFFu) >= 449u || y < -32 || y >= 273 || depth < 2) {
            ae_wr32(r, AE_FX_RIBBON_HALF_WIDTH + i * 4u, 0);
        } else {
            int32_t half_width = fx_div(r, width_numerator, depth, "RibbonDraw width");
            if (ae_ram_failed(r)) return;
            ae_wr32(r, AE_FX_RIBBON_HALF_WIDTH + i * 4u, (uint32_t)half_width);
        }
    }

    /* First endpoint uses the normalized direction of the first segment. */
    ae_wr16(r, work + 4u, 0);
    ae_wr16(r, work, ae_rd16(r, AE_FX_RIBBON_PROJECTED_X + 2u) -
                      ae_rd16(r, AE_FX_RIBBON_PROJECTED_X));
    ae_wr16(r, work + 2u, ae_rd16(r, AE_FX_RIBBON_PROJECTED_Y + 2u) -
                           ae_rd16(r, AE_FX_RIBBON_PROJECTED_Y));
    (void)ae_sdk_VectorNormalSS(r, work, work);
    if (ae_ram_failed(r)) return;
    int32_t nx = ae_rds16(r, work);
    int32_t ny = ae_rds16(r, work + 2u);
    int32_t half = ae_rds32(r, AE_FX_RIBBON_HALF_WIDTH);
    fx_ribbon_store_vertex(r, AE_FX_RIBBON_EDGE_A,
                           AE_FX_RIBBON_PROJECTED_X, AE_FX_RIBBON_PROJECTED_Y,
                           nx, ny, half);

    /* Interior vertices use the normalized sum of adjacent segment normals. */
    for (uint32_t i = 1; i + 1u < count; ++i) {
        int32_t tangent_x = fx_s16(ae_rd16(r, AE_FX_RIBBON_PROJECTED_X +
                                           (i + 1u) * 2u)) -
                                  fx_s16(ae_rd16(r, AE_FX_RIBBON_PROJECTED_X +
                                                 (i - 1u) * 2u));
        int32_t tangent_y = fx_s16(ae_rd16(r, AE_FX_RIBBON_PROJECTED_Y +
                                           (i + 1u) * 2u)) -
                                  fx_s16(ae_rd16(r, AE_FX_RIBBON_PROJECTED_Y +
                                                 (i - 1u) * 2u));
        ae_wr16(r, work, (uint32_t)tangent_x);
        ae_wr16(r, work + 2u, (uint32_t)tangent_y);
        ae_wr16(r, work + 4u, 0);
        (void)ae_sdk_VectorNormalSS(r, work, work);
        if (ae_ram_failed(r)) return;
        nx = ae_rds16(r, work);
        ny = ae_rds16(r, work + 2u);
        if (nx == 0 && ny == 0) {
            ae_wr16(r, work,
                    ae_rd16(r, AE_FX_RIBBON_PROJECTED_X + (i + 1u) * 2u) -
                    ae_rd16(r, AE_FX_RIBBON_PROJECTED_X + i * 2u));
            ae_wr16(r, work + 2u,
                    ae_rd16(r, AE_FX_RIBBON_PROJECTED_Y + (i + 1u) * 2u) -
                    ae_rd16(r, AE_FX_RIBBON_PROJECTED_Y + i * 2u));
            ae_wr16(r, work + 4u, 0);
            (void)ae_sdk_VectorNormalSS(r, work, work);
            if (ae_ram_failed(r)) return;
            nx = ae_rds16(r, work);
            ny = ae_rds16(r, work + 2u);
        }
        half = ae_rds32(r, AE_FX_RIBBON_HALF_WIDTH + i * 4u);
        fx_ribbon_store_vertex(r, AE_FX_RIBBON_EDGE_A + i * AE_FX_RIBBON_EDGE_STRIDE,
                               AE_FX_RIBBON_PROJECTED_X + i * 2u,
                               AE_FX_RIBBON_PROJECTED_Y + i * 2u,
                               nx, ny, half);
    }

    uint32_t last = count - 1u;
    int32_t dx = fx_s16(ae_rd16(r, AE_FX_RIBBON_PROJECTED_X + last * 2u)) -
                 fx_s16(ae_rd16(r, AE_FX_RIBBON_PROJECTED_X + (last - 1u) * 2u));
    int32_t dy = fx_s16(ae_rd16(r, AE_FX_RIBBON_PROJECTED_Y + last * 2u)) -
                 fx_s16(ae_rd16(r, AE_FX_RIBBON_PROJECTED_Y + (last - 1u) * 2u));
    ae_wr16(r, work, dx);
    ae_wr16(r, work + 2u, dy);
    ae_wr16(r, work + 4u, 0);
    (void)ae_sdk_VectorNormalSS(r, work, work);
    if (ae_ram_failed(r)) return;
    nx = ae_rds16(r, work);
    ny = ae_rds16(r, work + 2u);
    half = ae_rds32(r, AE_FX_RIBBON_HALF_WIDTH + last * 4u);
    fx_ribbon_store_vertex(r, AE_FX_RIBBON_EDGE_A + last * AE_FX_RIBBON_EDGE_STRIDE,
                           AE_FX_RIBBON_PROJECTED_X + last * 2u,
                           AE_FX_RIBBON_PROJECTED_Y + last * 2u,
                           nx, ny, half);

    (void)FX_CALL5(r, h, DrawRibbonStrip, points, count, color_table, scale, work);
}

/* 80084AA0: per-frame leaf dispatcher. */
void ae_ProjectileSystemsUpdate(AeRam *r, const AeFxscriptHooks *h) {
    ae_ProjectileRecordsUpdate(r, h);
    if (!ae_ram_failed(r)) ae_ProjectileGroupsUpdate(r, h);
}

/* 80084BC0: invoke the record's dynamic expiry callback before marking it dead. */
void ae_ProjectileExpire(AeRam *r, const AeFxscriptHooks *h,
                         uint32_t projectile, uint32_t reason) {
    uint32_t callback = ae_rd32(r, projectile + AE_FX_PROJECTILE_RECORD_EXPIRE);
    if (callback != 0u) {
        (void)FX_CALL(r, h, ProjectileExpireCallback, projectile, reason, callback, 0);
        if (ae_ram_failed(r)) return;
    }
    ae_wr8(r, projectile + AE_FX_PROJECTILE_RECORD_STATE, 1u);
    ae_wr32(r, projectile + AE_FX_PROJECTILE_RECORD_BODY, 0u);
}

/* These addresses are overlay RAM stubs, not functions in the main EXE. */
int32_t ae_FUN_8014022c(AeRam *r, const AeFxscriptHooks *h,
                        uint32_t position, uint32_t vector) {
    return FX_CALL(r, h, FUN_8014022c, position, vector, 0, 0);
}

int32_t ae_FUN_8014263c(AeRam *r, const AeFxscriptHooks *h,
                        uint32_t position, uint32_t vector) {
    return FX_CALL(r, h, FUN_8014263c, position, vector, 0, 0);
}

/* 80085630: mode 3 emits three groups after rotating the direction around Y. */
void ae_ProjectileGroupSpawnSpread(AeRam *r, const AeFxscriptHooks *h,
                                   uint32_t position, uint32_t direction,
                                   uint32_t mode) {
    if (mode != 3u) {
        (void)ae_ProjectileGroupSpawn(r, h, position, direction, mode, 0u);
        return;
    }
    uint32_t matrix = AE_FX_PROJECTILE_GROUP_MATRIX_LOCAL;
    uint32_t rotated = AE_FX_PROJECTILE_GROUP_MATRIX_TEMP;
    uint32_t angles = AE_FX_PROJECTILE_ROTATIONS;
    for (uint32_t index = 0; index < 3u; ++index) {
        (void)ae_sdk_MatrixIdentity(r, matrix);
        if (ae_ram_failed(r)) return;
        uint32_t angle_vector = AE_FX_PROJECTILE_GROUP_LOCAL_VECTOR;
        ae_wr16(r, angle_vector, 0u);
        ae_wr16(r, angle_vector + 2u,
                ae_rd16(r, AE_FX_PROJECTILE_SPREAD_TABLE + index * 4u));
        ae_wr16(r, angle_vector + 4u, 0u);
        (void)ae_sdk_RotMatrixZYX(r, angle_vector, matrix);
        if (ae_ram_failed(r)) return;
        (void)ae_sdk_ApplyMatrixSV(r, matrix, direction, rotated);
        if (ae_ram_failed(r)) return;
        (void)ae_ProjectileGroupSpawn(r, h, position, rotated, 3u,
                                     ae_rd32(r, angles + index * 8u));
        angles += 8u;
    }
}

/* 80085254: allocate one group and its update record, orient the normalized
 * direction, and configure the record from the requested group mode. */
uint32_t ae_ProjectileGroupSpawn(AeRam *r, const AeFxscriptHooks *h,
                                 uint32_t position, uint32_t direction,
                                 uint32_t count, uint32_t owner) {
    uint32_t group = 0;
    for (uint32_t i = 0; i < AE_FX_PROJECTILE_GROUP_COUNT; ++i) {
        uint32_t candidate = AE_FX_PROJECTILE_GROUPS + i * AE_FX_PROJECTILE_GROUP_STRIDE;
        if (ae_rd8(r, candidate + AE_FX_PROJECTILE_GROUP_ACTIVE) == 0u) {
            group = candidate;
            break;
        }
    }
    if (group == 0u) return 0u;

    uint32_t record = ae_ProjectileAllocSlot(r);
    if (record == 0u) return 0u;
    ae_wr32(r, group + AE_FX_PROJECTILE_GROUP_RECORD, record);
    ae_wr8(r, group + AE_FX_PROJECTILE_GROUP_ACTIVE, count);
    ae_wr32(r, group + AE_FX_PROJECTILE_GROUP_OWNER, owner);
    ae_ProjectileSetByte2(r, record, 30u);

    uint32_t record_origin = group + AE_FX_PROJECTILE_GROUP_DATA;
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        ae_wr16(r, record_origin + axis * 2u, ae_rd16(r, position + axis * 2u));
    }
    if (ae_rd8(r, AE_FX_PROJECTILE_AIM_MODE) == 1u) {
        ae_wr16(r, record_origin, ae_rd16(r, AE_FX_PROJECTILE_GROUP_STATE + AE_FX_PROJECTILE_STATE_AIM_TARGET_X));
        ae_wr16(r, record_origin + 2u, ae_rd16(r, AE_FX_PROJECTILE_GROUP_STATE + AE_FX_PROJECTILE_STATE_AIM_TARGET_Y));
        ae_wr16(r, record_origin + 4u, ae_rd16(r, AE_FX_PROJECTILE_GROUP_STATE + AE_FX_PROJECTILE_STATE_AIM_TARGET_Z));
        if (count == 5u) {
            ae_wr16(r, record_origin + 2u,
                    ae_rd16(r, AE_FX_PROJECTILE_GROUP_STATE + AE_FX_PROJECTILE_STATE_AIM_TARGET_Y) + 32u);
        }
    }

    const uint32_t normalized = AE_FX_PROJECTILE_GROUP_NORMALIZED;
    (void)ae_sdk_VectorNormalSS(r, direction, normalized);
    if (ae_ram_failed(r)) return 0u;
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        int32_t component = ae_rds16(r, normalized + axis * 2u);
        ae_wr16(r, normalized + axis * 2u, (uint32_t)fx_div(r, component, 20,
                                                             "ProjectileGroupSpawn /10"));
        if (ae_ram_failed(r)) return 0u;
    }
    ae_ProjectileInit(r, record, record_origin, normalized, 0u);

    uint32_t mode = count - 1u;
    uint32_t aim_mode = ae_rd8(r, AE_FX_PROJECTILE_AIM_MODE);
    switch (mode) {
    case 1u:
        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_TPAGE, 0x0E36u);
        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_CLUT, 515u);
        ae_ProjectileSetUpdate(r, record, 4u, 121u, 121u);
        (void)FX_CALL(r, h, PadRumble, 1u, 110u, 4u, 0);
        if (ae_ram_failed(r)) return 0u;
        break;
    case 2u:
        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_TPAGE, 0x0F24u);
        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_CLUT, 0x0202u);
        ae_ProjectileSetUpdate(r, record, 0x112u, 79u, 79u);
        if (owner != 0u) {
            ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_POSITION + 2u, (uint32_t)-60);
            ae_ProjectileSetByte2(r, record, 90u);
        }
        (void)FX_CALL(r, h, PadRumble, 1u, 110u, 4u, 0);
        if (ae_ram_failed(r)) return 0u;
        break;
    case 3u: {
        (void)fx_play_sfx(r, h, 0x76u);
        if (ae_ram_failed(r)) return 0u;
        if (aim_mode != 1u) {
            AeSpawnHooks spawn;
            AeWorldHooks world;
            fx_spawn_hooks(h, &spawn, &world);
            ae_SpawnParticle(r, &spawn, 0x10Bu, record_origin);
            if (ae_ram_failed(r)) return 0u;
        }
        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_TPAGE, 0x0D04u);
        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_CLUT, 0x0201u);
        ae_ProjectileSetUpdate(r, record, 0x111u, 95u, 34u);
        (void)FX_CALL(r, h, PadRumble, 1u, 110u, 4u, 0);
        break;
    }
    case 4u:
        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_TPAGE, 0x0836u);
        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_CLUT, 515u);
        (void)fx_play_sfx(r, h, 0x77u);
        if (ae_ram_failed(r)) return 0u;
        ae_CameraShake(r, 0);
        (void)FX_CALL(r, h, PadRumble, 1u, 240u, 5u, 0);
        if (ae_ram_failed(r)) return 0u;
        ae_ProjectileSetUpdate(r, record, 4u, 121u, 121u);
        ae_ProjectileSetByte2(r, record, 30u);
        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_POSITION + 2u,
                ae_rd16(r, record + AE_FX_PROJECTILE_RECORD_POSITION + 2u) - 32u);
        break;
    case 5u: {
        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_TPAGE, 0x0E36u);
        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_CLUT, 515u);
        ae_ProjectileSetUpdate(r, record, 0u, 255u, 255u);
        ae_TrailSpawn(r, position, 0, 470, 0);
        ae_wr8(r, group + AE_FX_PROJECTILE_GROUP_MOTION, 0u);
        break;
    }
    default:
        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_TPAGE, 0x0D02u);
        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_CLUT, 0x0201u);
        ae_ProjectileSetUpdate(r, record, 0x111u, 95u, 34u);
        (void)FX_CALL(r, h, PadRumble, 1u, 110u, 4u, 0);
        break;
    }

    if (ae_ram_failed(r)) return 0u;
    if (aim_mode != 1u) ae_ProjectileSetColor(r, record, 0u);
    ae_wr32(r, record + AE_FX_PROJECTILE_RECORD_OWNER, AE_FX_PROJECTILE_BODY_OWNER);
    ae_wr8(r, record + AE_FX_PROJECTILE_RECORD_AGE, 2u);
    return 1u;
}

static void fx_projectile_spawn_particle(AeRam *r, const AeFxscriptHooks *h,
                                         int32_t type, uint32_t position) {
    AeSpawnHooks spawn;
    AeWorldHooks world;
    fx_spawn_hooks(h, &spawn, &world);
    ae_SpawnParticle(r, &spawn, type, position);
}

static void fx_projectile_register_body(AeRam *r, const AeFxscriptHooks *h,
                                        int32_t type, uint32_t body, uint32_t owner) {
    AeBodyHooks body_hooks = fx_body_hooks(h);
    (void)ae_CollisionBodyRegister(r, &body_hooks, type, body, owner);
}

static void fx_projectile_half_signed(AeRam *r, uint32_t address) {
    int32_t value = ae_rds16(r, address);
    int32_t half = value / 2;
    ae_wr16(r, address, (uint32_t)half);
}

/* 80084C94: age and move active records, react to surface/body impacts, and
 * refresh their collision bodies and optional trail ribbons. */
void ae_ProjectileRecordsUpdate(AeRam *r, const AeFxscriptHooks *h) {
    for (uint32_t index = 0; index < AE_FX_PROJECTILE_RECORD_COUNT; ++index) {
        uint32_t record = AE_FX_PROJECTILE_RECORDS + index * AE_FX_PROJECTILE_RECORD_STRIDE;
        uint32_t state = ae_rd8(r, record + AE_FX_PROJECTILE_RECORD_STATE);
        if (state == 0u) continue;
        if (state == 1u || state == 2u) {
            ae_wr8(r, record + AE_FX_PROJECTILE_RECORD_STATE, 0u);
            continue;
        }

        uint32_t timer = (ae_rd8(r, record + AE_FX_PROJECTILE_RECORD_KIND + 1u) - 1u) & 0xFFu;
        ae_wr8(r, record + AE_FX_PROJECTILE_RECORD_KIND + 1u, timer);
        if (timer == 0u) {
            ae_ProjectileExpire(r, h, record, 2u);
            if (ae_ram_failed(r)) return;
            ae_wr8(r, record + AE_FX_PROJECTILE_RECORD_STATE, 2u);
            continue;
        }

        uint32_t body = ae_rd32(r, record + AE_FX_PROJECTILE_RECORD_BODY);
        if (body != 0u) {
            uint32_t body_kind = ae_rd32(r, body + AE_FX_COLLISION_BODY_KIND) >> 28;
            if (body_kind == 6u || body_kind == 11u) {
                ae_ProjectileExpire(r, h, record, 0u);
                if (ae_ram_failed(r)) return;
                continue;
            }
            if (body_kind == 10u) {
                uint32_t flags = ae_rd8(r, record + AE_FX_PROJECTILE_RECORD_KIND);
                uint32_t source = ae_rd32(r, record + AE_FX_PROJECTILE_RECORD_SOURCE);
                if ((flags & 0x60u) == 0u) {
                    for (uint32_t axis = 0; axis < 3u; ++axis) {
                        uint32_t component = record + AE_FX_PROJECTILE_RECORD_POSITION + axis * 2u;
                        ae_wr16(r, source + axis * 2u,
                                ae_rd16(r, source + axis * 2u) - ae_rd16(r, component));
                    }
                    ae_ProjectileExpire(r, h, record, 1u);
                    if (ae_ram_failed(r)) return;
                    continue;
                }

                int32_t vertical = ae_rds16(r, record + AE_FX_PROJECTILE_RECORD_HEIGHT);
                if (vertical > 0) {
                    if ((flags & 0x60u) == 0x20u) {
                        vertical = fx_asr32(-7 * vertical + 7, 3);
                        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_HEIGHT, (uint32_t)vertical);
                    } else {
                        fx_projectile_half_signed(r, record + AE_FX_PROJECTILE_RECORD_POSITION);
                        vertical = -(vertical / 2);
                        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_HEIGHT, (uint32_t)vertical);
                        fx_projectile_half_signed(r, record + AE_FX_PROJECTILE_RECORD_PARAM);
                    }
                }

                int32_t fade = fx_s8(ae_rd8(r, record + AE_FX_PROJECTILE_RECORD_FALL));
                if (-fade < ae_rds16(r, record + AE_FX_PROJECTILE_RECORD_HEIGHT)) {
                    ae_ProjectileExpire(r, h, record, 1u);
                    if (ae_ram_failed(r)) return;
                    continue;
                }
                if (ae_rds16(r, record + AE_FX_PROJECTILE_RECORD_HEIGHT) < -20) {
                    (void)fx_play_sfx(r, h, 0x51u);
                    if (ae_ram_failed(r)) return;
                    fx_projectile_spawn_particle(r, h, 4, source);
                    if (ae_ram_failed(r)) return;
                }
            }
        }

        uint32_t source = ae_rd32(r, record + AE_FX_PROJECTILE_RECORD_SOURCE);
        int32_t surface = ae_GetSurfaceType(r, ae_rds16(r, source),
                                            ae_rds16(r, source + 2u),
                                            ae_rds16(r, source + 4u));
        if (ae_ram_failed(r)) return;
        uint32_t surface_kind = ((uint32_t)surface & 0xFFFFu) >> 12;
        if (surface_kind == 1u &&
            (ae_rd16(r, record + AE_FX_PROJECTILE_RECORD_SURFACE) >> 12) != surface_kind) {
            uint32_t local = AE_FX_PROJECTILE_LOCAL_IMPACT;
            fx_clear_local(r, h, AE_FX_PROJECTILE_LOCAL_RIBBON, 8u);
            ae_wr16(r, local, ae_rd16(r, source));
            ae_wr16(r, local + 2u,
                    ae_rd16(r, AE_FX_PROJECTILE_BODY_HEIGHT) -
                    ae_rd16(r, AE_FX_PROJECTILE_BODY_BASE));
            ae_wr16(r, local + 4u, ae_rd16(r, source + 4u));
            if (ae_ram_failed(r)) return;
            fx_projectile_spawn_particle(r, h, 0x100, local);
            if (ae_ram_failed(r)) return;
            (void)fx_play_sfx(r, h, 0x72u);
            if (ae_ram_failed(r)) return;
            fx_projectile_half_signed(r, record + AE_FX_PROJECTILE_RECORD_POSITION);
            fx_projectile_half_signed(r, record + AE_FX_PROJECTILE_RECORD_HEIGHT);
            fx_projectile_half_signed(r, record + AE_FX_PROJECTILE_RECORD_PARAM);
        }
        ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_SURFACE, (uint32_t)surface);

        uint32_t motion_mode = ae_rd8(r, record + AE_FX_PROJECTILE_RECORD_KIND) & 0x0Fu;
        if (motion_mode == 0u || motion_mode == 1u) {
            for (uint32_t axis = 0; axis < 3u; ++axis) {
                uint32_t velocity = record + AE_FX_PROJECTILE_RECORD_POSITION + axis * 2u;
                ae_wr16(r, source + axis * 2u,
                        ae_rd16(r, source + axis * 2u) + ae_rd16(r, velocity));
            }
            if (motion_mode == 1u) {
                int32_t gravity = fx_s8(ae_rd8(r, record + AE_FX_PROJECTILE_RECORD_FALL));
                ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_HEIGHT,
                        ae_rd16(r, record + AE_FX_PROJECTILE_RECORD_HEIGHT) + gravity);
            }
        }

        for (uint32_t axis = 0; axis < 3u; ++axis) {
            ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_NORMAL + axis * 2u,
                    ae_rd16(r, record + AE_FX_PROJECTILE_RECORD_POSITION + axis * 2u));
        }
        (void)ae_sdk_VectorNormalSS(r, record + AE_FX_PROJECTILE_RECORD_NORMAL,
                                    record + AE_FX_PROJECTILE_RECORD_NORMAL);
        if (ae_ram_failed(r)) return;

        body = ae_CollisionBodyAlloc(r);
        if (body != 0u) {
            ae_wr32(r, record + AE_FX_PROJECTILE_RECORD_BODY, body);
            for (uint32_t axis = 0; axis < 3u; ++axis) {
                ae_wr32(r, body + axis * 4u, (uint32_t)ae_rds16(r, source + axis * 2u));
            }
            for (uint32_t axis = 0; axis < 3u; ++axis) {
                ae_wr16(r, body + AE_FX_COLLISION_BODY_VECTOR + axis * 2u,
                        ae_rd16(r, record + AE_FX_PROJECTILE_RECORD_NORMAL + axis * 2u));
            }
            ae_wr16(r, body + AE_FX_COLLISION_BODY_HEIGHT,
                    ae_rd16(r, record + AE_FX_PROJECTILE_RECORD_SIZE));
            ae_wr16(r, body + AE_FX_COLLISION_BODY_TPAGE,
                    ae_rd16(r, record + AE_FX_PROJECTILE_RECORD_TPAGE));
            ae_wr16(r, body + AE_FX_COLLISION_BODY_CLUT,
                    ae_rd16(r, record + AE_FX_PROJECTILE_RECORD_CLUT));
            fx_projectile_register_body(r, h, 7, body,
                ae_rd32(r, record + AE_FX_PROJECTILE_RECORD_OWNER));
            if (ae_ram_failed(r)) return;
        }

        if ((ae_rd8(r, record + AE_FX_PROJECTILE_RECORD_KIND) & 0x10u) != 0u) {
            uint32_t local = AE_FX_PROJECTILE_LOCAL_RIBBON;
            fx_copy8(r, local, source);
            (void)ae_sdk_VectorNormalSS(r, local, AE_FX_PROJECTILE_LOCAL_NORMAL);
            if (ae_ram_failed(r)) return;
            uint32_t width = ae_rd8(r, record + AE_FX_PROJECTILE_RECORD_WIDTH);
            for (uint32_t axis = 0; axis < 3u; ++axis) {
                int32_t delta = fx_asr32((uint32_t)(-
                    (int32_t)ae_rds16(r, AE_FX_PROJECTILE_LOCAL_NORMAL + axis * 2u) *
                    (int32_t)width), 8);
                ae_wr16(r, local + 8u + axis * 2u,
                        ae_rd16(r, source + axis * 2u) + delta);
            }
            ae_wr16(r, local + 14u, ae_rd16(r, source + 6u));
            ae_RibbonDraw(r, h, local, 2u,
                          AE_FX_RIBBON_COLOR_TABLE +
                              ae_rd8(r, record + AE_FX_PROJECTILE_RECORD_COLOR_INDEX) * 8u,
                          ae_rd8(r, record + AE_FX_PROJECTILE_RECORD_COLOR_MODE));
            if (ae_ram_failed(r)) return;
        }
    }
}

static void fx_rc_car_despawn(AeRam *r, const AeFxscriptHooks *h) {
    AeBodyHooks body = fx_body_hooks(h);
    AeAreaSetHooks area = {0};
    area.body_hooks = &body;
    ae_RcCarDespawn(r, &area);
}

static void fx_rc_car_copy_position(AeRam *r, uint32_t local) {
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        ae_wr16(r, local + axis * 2u,
                ae_rd16(r, AE_FX_RC_CAR + AE_FX_RC_POSITION + axis * 4u));
    }
}

static int32_t fx_rc_car_step_toward(AeRam *r, const AeFxscriptHooks *h,
                                     int32_t current, int32_t target) {
    return FX_CALL(r, h, StepToward, current, target, 4, 0);
}

/* 800994A8: update the RC car's ground response, aim steering, collision body,
 * surface effects, and velocity. */
void ae_RcCarUpdate(AeRam *r, const AeFxscriptHooks *h) {
    AeWorldHooks world = fx_world_hooks(h);
    const uint32_t car = AE_FX_RC_CAR;
    if (ae_rd32(r, car + AE_FX_RC_ACTIVE) == 0u) return;

    int32_t vertical_state = ae_rds32(r, car + AE_FX_RC_VERTICAL_STATE);
    if (vertical_state < -14) vertical_state = -14;
    ae_wr32(r, car + AE_FX_RC_VERTICAL_STATE, (uint32_t)(vertical_state + 7));

    int32_t aim_timer = ae_rds16(r, car + AE_FX_RC_UPDATE_TIMER);
    if (aim_timer > 0) {
        ae_wr16(r, car + AE_FX_RC_UPDATE_TIMER, (uint32_t)(aim_timer - 1));
        for (uint32_t axis = 0; axis < 2u; ++axis) {
            uint32_t player_value = AE_FX_PLAYER + AE_FX_PLAYER_FACING + axis * 4u;
            uint32_t car_value = car + AE_FX_RC_SPEED_X + axis * 8u;
            ae_wr32(r, car_value,
                    (uint32_t)fx_s8(ae_rd16(r, player_value) >> 8));
        }
        return;
    }

    int32_t ground = ae_GetGroundHeight(r, &world,
        ae_rds32(r, car + AE_FX_RC_POSITION),
        ae_rds32(r, car + AE_FX_RC_POSITION + 4u),
        ae_rds32(r, car + AE_FX_RC_POSITION + 8u));
    if (ae_ram_failed(r)) return;
    int32_t target_x = 0;
    int32_t target_z = 0;
    if (ae_rds32(r, car + AE_FX_RC_POSITION + 4u) >= ground) {
        uint32_t scratch = 0x1F8003F0u;
        ae_wr16(r, car + AE_FX_RC_SPIN, 512u);
        ae_wr16(r, car + AE_FX_RC_FACING_VECTOR,
                (uint32_t)-ae_rds16(r, scratch + AE_FX_RC_GTE_NORMAL_X));
        ae_wr16(r, car + AE_FX_RC_FACING_VECTOR + 4u,
                (uint32_t)-ae_rds16(r, scratch + AE_FX_RC_GTE_NORMAL_Z));
        (void)ae_sdk_VectorNormalSS(r, car + AE_FX_RC_FACING_VECTOR,
                                    car + AE_FX_RC_FACING_VECTOR);
        if (ae_ram_failed(r)) return;
        uint32_t collision = ae_rd16(r, scratch + AE_FX_RC_IMPACT_FLAGS);
        uint32_t collision_index = (collision >> 1) & 7u;
        if ((collision & 1u) != 0u && collision_index < 4u) {
            uint32_t position = AE_FX_RC_LOCAL_POSITION;
            fx_rc_car_copy_position(r, position);
            if (ae_ram_failed(r)) return;
            fx_projectile_spawn_particle(r, h, 0x102, position);
            if (ae_ram_failed(r)) return;
            fx_rc_car_despawn(r, h);
            if (ae_ram_failed(r)) return;
            (void)fx_play_sfx(r, h, 0x79u);
            if (ae_ram_failed(r)) return;
        }
    }

    int32_t can_gadget = FX_CALL(r, h, PlayerCanActivateGadget, 7, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    uint32_t steering_mode = ae_rd16(r, AE_FX_RC_STEERING_MODE);
    int gadget_steering = can_gadget != 0 &&
        fx_s8(ae_rd8(r, AE_FX_PLAYER + AE_FX_PLAYER_GADGET7_ALLOWED)) == 7 &&
        ae_rds16(r, AE_FX_PLAYER + AE_FX_PLAYER_GADGET7_TIMER) > 0;
    if (gadget_steering) {
        if (steering_mode == 0u || steering_mode >= 4u) {
            uint32_t aim_strength = ae_rd16(r, AE_FX_RC_AIM_STRENGTH);
            if (aim_strength != 0u) {
                (void)FX_CALL(r, h, TurnTowardsY, AE_FX_RC_TARGET_TURN,
                              AE_FX_RC_MATRIX, 0x155u, 0);
                if (ae_ram_failed(r)) return;
                int32_t facing_x = ae_rds16(r, car + AE_FX_RC_AIM_VECTOR_X);
                int32_t facing_z = ae_rds16(r, car + AE_FX_RC_AIM_VECTOR_Z);
                target_x = fx_div(r, facing_x * (int32_t)aim_strength, 0x3000,
                                  "RcCarUpdate aim x /0x3000");
                target_z = fx_div(r, facing_z * (int32_t)aim_strength, 0x3000,
                                  "RcCarUpdate aim z /0x3000");
                if (ae_ram_failed(r)) return;
            }
        }
        {
            uint32_t scratch = 0x1F8003F0u;
            int32_t sx = ae_rds16(r, scratch + AE_FX_RC_GTE_NORMAL_X);
            int32_t sz = ae_rds16(r, scratch + AE_FX_RC_GTE_NORMAL_Z);
            if ((uint32_t)(sx + 512) >= 1025u) {
                target_x = ae_rds32(r, car + AE_FX_RC_SPEED_X) +
                    fx_div(r, 0x0E00, sx, "RcCarUpdate target x");
            }
            if ((uint32_t)(sz + 512) >= 1025u) {
                target_z = ae_rds32(r, car + AE_FX_RC_SPEED_Z) +
                    fx_div(r, 0x0E00, sz, "RcCarUpdate target z");
            }
            if (ae_ram_failed(r)) return;
        }
    } else {
        target_x = 0;
        target_z = 0;
    }

    if (ae_rds32(r, car + AE_FX_RC_POSITION + 4u) < ground) {
        target_x = ae_rds32(r, car + AE_FX_RC_SPEED_X);
        target_z = ae_rds32(r, car + AE_FX_RC_SPEED_Z);
    }

    int32_t speed_x = ae_rds32(r, car + AE_FX_RC_SPEED_X);
    int32_t speed_z = ae_rds32(r, car + AE_FX_RC_SPEED_Z);
    uint32_t speed_squared_bits = (uint32_t)speed_x * (uint32_t)speed_x +
                                  (uint32_t)speed_z * (uint32_t)speed_z;
    int32_t speed_squared = fx_s32(speed_squared_bits);
    if (gadget_steering && speed_squared >= 901) {
        uint32_t body = ae_CollisionBodyAlloc(r);
        if (body != 0u) {
            ae_wr32(r, body, ae_rd32(r, car + AE_FX_RC_POSITION));
            int32_t size = ae_rds16(r, car + AE_FX_RC_CAR_SIZE);
            int32_t center_y = ae_rds32(r, car + AE_FX_RC_POSITION + 4u) - size / 2;
            ae_wr32(r, body + 4u, (uint32_t)center_y);
            ae_wr32(r, body + 8u, ae_rd32(r, car + AE_FX_RC_POSITION + 8u));
            ae_wr16(r, body + AE_FX_COLLISION_BODY_HEIGHT, 80u);
            for (uint32_t word = 0; word < 2u; ++word) {
                ae_wr32(r, body + AE_FX_COLLISION_BODY_VECTOR + word * 4u,
                        ae_rd32(r, car + AE_FX_PLAYER_FACING + word * 4u));
            }
            ae_wr16(r, body + AE_FX_COLLISION_BODY_TPAGE, 34u);
            ae_wr16(r, body + AE_FX_COLLISION_BODY_CLUT, 0x0201u);
            fx_projectile_register_body(r, h, 6, body, car);
            if (ae_ram_failed(r)) return;
        } else {
            target_x = 0;
            target_z = 0;
        }
    }

    uint32_t position = AE_FX_RC_LOCAL_POSITION;
    fx_rc_car_copy_position(r, position);
    if (ae_ram_failed(r)) return;
    uint32_t old_surface = ae_rd16(r, car + AE_FX_RC_SURFACE_CURRENT);
    ae_wr16(r, car + AE_FX_RC_SURFACE_PREVIOUS, old_surface);
    int32_t surface = ae_GetSurfaceType(r,
        ae_rds32(r, car + AE_FX_RC_POSITION),
        ae_rds32(r, car + AE_FX_RC_POSITION + 4u),
        ae_rds32(r, car + AE_FX_RC_POSITION + 8u));
    if (ae_ram_failed(r)) return;
    ae_wr16(r, car + AE_FX_RC_SURFACE_CURRENT, (uint32_t)surface);
    if (((uint32_t)surface >> 12) == 1u) {
        if ((old_surface >> 12) != 1u) {
            fx_projectile_spawn_particle(r, h, 0x100, position);
            if (ae_ram_failed(r)) return;
            (void)fx_play_sfx(r, h, 0x72u);
            if (ae_ram_failed(r)) return;
            ae_wr16(r, car + AE_FX_RC_IMPACT_COUNT, 0u);
        }
        int32_t impact_count = (int32_t)(int16_t)(ae_rd16(r, car + AE_FX_RC_IMPACT_COUNT) + 1u);
        ae_wr16(r, car + AE_FX_RC_IMPACT_COUNT, (uint32_t)impact_count);
        if (impact_count >= 31) {
            fx_projectile_spawn_particle(r, h, 0x102, position);
            if (ae_ram_failed(r)) return;
            (void)fx_play_sfx(r, h, 0x79u);
            if (ae_ram_failed(r)) return;
            fx_rc_car_despawn(r, h);
            if (ae_ram_failed(r)) return;
        } else if ((ae_rd32(r, AE_FX_RC_FRAME_COUNTER) & 7u) == 0u) {
            (void)FX_CALL(r, h, SpawnRandomParticle, car + AE_FX_RC_POSITION,
                          1u, 0u, 96u);
            if (ae_ram_failed(r)) return;
        }
    }

    uint32_t collision_kind = ae_rd32(r, car + AE_FX_RC_BODY_CONTACT) >> 28;
    if (collision_kind == 11u) {
        (void)fx_play_sfx(r, h, 0x79u);
        if (ae_ram_failed(r)) return;
        fx_projectile_spawn_particle(r, h, 0x102, position);
        if (ae_ram_failed(r)) return;
        uint32_t direction = AE_FX_RC_LOCAL_DIRECTION;
        for (uint32_t axis = 0; axis < 3u; ++axis) {
            ae_wr16(r, direction + axis * 2u, 0u);
        }
        ae_ProjectileGroupSpawnSpread(r, h, position, direction, 6u);
        if (ae_ram_failed(r)) return;
        fx_rc_car_despawn(r, h);
        if (ae_ram_failed(r)) return;
    }

    if (ae_rds32(r, car + AE_FX_RC_VERTICAL_STATE) < -7) {
        int32_t current_x = ae_rds32(r, car + AE_FX_RC_SPEED_X);
        int32_t current_z = ae_rds32(r, car + AE_FX_RC_SPEED_Z);
        ae_wr32(r, car + AE_FX_RC_SPEED_X,
                (uint32_t)fx_asr32((int32_t)((uint32_t)current_x + (uint32_t)target_x), 2));
        ae_wr32(r, car + AE_FX_RC_SPEED_Z,
                (uint32_t)fx_asr32((int32_t)((uint32_t)current_z + (uint32_t)target_z), 2));
        return;
    }

    int32_t moved_x = fx_rc_car_step_toward(r, h,
        ae_rds32(r, car + AE_FX_RC_SPEED_X), target_x);
    if (ae_ram_failed(r)) return;
    int32_t moved_z = fx_rc_car_step_toward(r, h,
        ae_rds32(r, car + AE_FX_RC_SPEED_Z), target_z);
    if (ae_ram_failed(r)) return;
    ae_wr32(r, car + AE_FX_RC_SPEED_X, (uint32_t)moved_x);
    ae_wr32(r, car + AE_FX_RC_SPEED_Z, (uint32_t)moved_z);
    if ((moved_x != target_x || moved_z != target_z) &&
        (ae_rd32(r, AE_FX_RC_FRAME_COUNTER) & 1u) == 0u) {
        uint32_t movement_position = AE_FX_RC_LOCAL_DIRECTION;
        fx_clear_local(r, h, movement_position, 8u);
        if (ae_ram_failed(r)) return;
        fx_rc_car_copy_position(r, movement_position);
        fx_projectile_spawn_particle(r, h, 4, position);
    }
}

static void fx_projectile_spawn_at_group(AeRam *r, const AeFxscriptHooks *h,
                                         int32_t type, uint32_t group) {
    fx_projectile_spawn_particle(r, h, type,
        group + AE_FX_PROJECTILE_GROUP_POSITION);
}

static int32_t fx_projectile_divide_three(AeRam *r, int32_t value) {
    return fx_div(r, value, 3, "ProjectileGroupsUpdate /3");
}

static void fx_projectile_group_body(AeRam *r, const AeFxscriptHooks *h,
                                     uint32_t group, int32_t type,
                                     uint32_t body_height, uint32_t body_flags,
                                     uint32_t body_owner) {
    uint32_t body = ae_CollisionBodyAlloc(r);
    if (body == 0u) return;
    ae_wr32(r, group, body);
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        ae_wr32(r, body + axis * 4u,
                (uint32_t)ae_rds16(r, group + AE_FX_PROJECTILE_GROUP_POSITION + axis * 2u));
    }
    ae_wr16(r, body + AE_FX_COLLISION_BODY_HEIGHT, body_height);
    ae_wr16(r, body + AE_FX_COLLISION_BODY_TPAGE, 0x0E36u);
    ae_wr16(r, body + AE_FX_COLLISION_BODY_CLUT, body_flags);
    fx_projectile_register_body(r, h, type, body, body_owner);
}

static void fx_projectile_group_ground_step(AeRam *r, const AeFxscriptHooks *h,
                                            uint32_t group) {
    AeWorldHooks world = fx_world_hooks(h);
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        uint32_t pos = group + AE_FX_PROJECTILE_GROUP_POSITION + axis * 2u;
        uint32_t velocity = group + AE_FX_PROJECTILE_GROUP_VELOCITY + axis * 2u;
        ae_wr16(r, pos, ae_rd16(r, pos) + ae_rd16(r, velocity));
    }
    ae_wr8(r, group + AE_FX_PROJECTILE_GROUP_PROGRESS,
           ae_rd8(r, group + AE_FX_PROJECTILE_GROUP_PROGRESS) + 1u);
    ae_wr16(r, group + AE_FX_PROJECTILE_GROUP_VELOCITY + 2u,
            ae_rd16(r, group + AE_FX_PROJECTILE_GROUP_VELOCITY + 2u) + 12u);

    int32_t ground = ae_GetGroundHeight(r, &world,
        ae_rds16(r, group + AE_FX_PROJECTILE_GROUP_POSITION),
        ae_rds16(r, group + AE_FX_PROJECTILE_GROUP_POSITION + 2u),
        ae_rds16(r, group + AE_FX_PROJECTILE_GROUP_POSITION + 4u));
    if (ae_ram_failed(r)) return;
    int32_t floor = ground - 32;
    int32_t position_y = ae_rds16(r, group + AE_FX_PROJECTILE_GROUP_POSITION + 2u);
    if (position_y >= floor) {
        ae_wr16(r, group + AE_FX_PROJECTILE_GROUP_POSITION + 2u, (uint32_t)floor);
        int32_t vx = ae_rds16(r, group + AE_FX_PROJECTILE_GROUP_VELOCITY);
        int32_t vy = ae_rds16(r, group + AE_FX_PROJECTILE_GROUP_VELOCITY + 2u);
        int32_t vz = ae_rds16(r, group + AE_FX_PROJECTILE_GROUP_VELOCITY + 4u);
        ae_wr16(r, group + AE_FX_PROJECTILE_GROUP_VELOCITY, (uint32_t)(vx / 2));
        ae_wr16(r, group + AE_FX_PROJECTILE_GROUP_VELOCITY + 2u,
                (uint32_t)fx_projectile_divide_three(r, -vy));
        ae_wr16(r, group + AE_FX_PROJECTILE_GROUP_VELOCITY + 4u, (uint32_t)(vz / 2));
        if (ae_rds16(r, group + AE_FX_PROJECTILE_GROUP_VELOCITY + 2u) < -19) {
            fx_projectile_spawn_at_group(r, h, 4, group);
        } else {
            ae_wr8(r, group + AE_FX_PROJECTILE_GROUP_ACTIVE, 0u);
        }
    }
    if (ae_rd8(r, group + AE_FX_PROJECTILE_GROUP_PROGRESS) >= 60u) {
        ae_wr8(r, group + AE_FX_PROJECTILE_GROUP_ACTIVE, 0u);
    }
}

static void fx_projectile_group_track_owner(AeRam *r, uint32_t group,
                                             uint32_t record) {
    uint32_t owner = ae_rd32(r, group + AE_FX_PROJECTILE_GROUP_OWNER);
    if (owner == 0u || record == 0u) return;

    for (uint32_t axis = 0; axis < 3u; ++axis) {
        uint32_t component = record + AE_FX_PROJECTILE_RECORD_POSITION + axis * 2u;
        int32_t value = ae_rds16(r, component);
        ae_wr16(r, component, (uint32_t)fx_asr32(value * 925, 10));
    }

    uint32_t direction = AE_FX_PROJECTILE_GROUP_LOCAL_VECTOR;
    ae_wr16(r, direction,
            ae_rd16(r, owner) - ae_rd16(r, group + AE_FX_PROJECTILE_GROUP_POSITION));
    ae_wr16(r, direction + 2u,
            ae_rd16(r, owner + 2u) -
            (ae_rd16(r, group + AE_FX_PROJECTILE_GROUP_POSITION + 2u) + 64u));
    ae_wr16(r, direction + 4u,
            ae_rd16(r, owner + 4u) -
            ae_rd16(r, group + AE_FX_PROJECTILE_GROUP_POSITION + 4u));
    (void)ae_sdk_VectorNormalSS(r, direction, direction);
    if (ae_ram_failed(r)) return;

    for (uint32_t axis = 0; axis < 3u; ++axis) {
        uint32_t component = record + AE_FX_PROJECTILE_RECORD_POSITION + axis * 2u;
        int32_t delta = fx_s8(ae_rd16(r, direction + axis * 2u) >> 8);
        int32_t value = ae_rds16(r, component) + delta;
        if (value < -96) value = -96;
        if (value > 96) value = 96;
        ae_wr16(r, component, (uint32_t)value);
    }
}

/* 800856FC: drive the four projectile-group state machines and their models. */
void ae_ProjectileGroupsUpdate(AeRam *r, const AeFxscriptHooks *h) {
    uint32_t local_matrix = AE_FX_PROJECTILE_GROUP_MATRIX_LOCAL;
    uint32_t local_compose = AE_FX_PROJECTILE_GROUP_MATRIX_TEMP;
    uint32_t local_points = AE_FX_PROJECTILE_GROUP_LOCAL_VECTOR;
    (void)ae_sdk_MatrixIdentity(r, local_matrix);
    if (ae_ram_failed(r)) return;
        uint32_t rotation = (ae_rd32(r, AE_FX_RIBBON_OT_TABLE) & 0x0Fu) << 8;
    ae_wr16(r, local_points, 0u);
    ae_wr16(r, local_points + 2u, 0u);
    ae_wr16(r, local_points + 4u, rotation);
    (void)ae_sdk_RotMatrixZYX(r, local_points, local_matrix);
    if (ae_ram_failed(r)) return;

    for (uint32_t index = 0; index < AE_FX_PROJECTILE_GROUP_COUNT; ++index) {
        uint32_t group = AE_FX_PROJECTILE_GROUPS + index * AE_FX_PROJECTILE_GROUP_STRIDE;
        uint32_t group_state = ae_rd8(r, group + AE_FX_PROJECTILE_GROUP_ACTIVE);
        if (group_state == 0u) continue;

        uint32_t record = ae_rd32(r, group + AE_FX_PROJECTILE_GROUP_RECORD);
        uint32_t record_state = record ? ae_rd8(r, record + AE_FX_PROJECTILE_RECORD_STATE) : 0u;
        if (record_state == 1u || record_state == 2u) {
            if (group_state == 2u) {
                fx_projectile_spawn_at_group(r, h, 0x102, group);
                if (ae_ram_failed(r)) return;
                for (uint32_t axis = 0; axis < 3u; ++axis) {
                    ae_wr16(r, group + AE_FX_PROJECTILE_GROUP_VELOCITY + axis * 2u, 0u);
                }
                ae_ProjectileGroupSpawnSpread(r, h,
                    group + AE_FX_PROJECTILE_GROUP_POSITION,
                    group + AE_FX_PROJECTILE_GROUP_KIND, 6u);
                if (ae_ram_failed(r)) return;
                fx_projectile_spawn_at_group(r, h, 0x113, group);
                if (ae_ram_failed(r)) return;
                ae_wr8(r, group + AE_FX_PROJECTILE_GROUP_ACTIVE, 0u);
                continue;
            }
            if (group_state == 5u) {
                fx_projectile_spawn_at_group(r, h, 0x10D, group);
                if (ae_ram_failed(r)) return;
                ae_wr8(r, group + AE_FX_PROJECTILE_GROUP_ACTIVE, 0u);
                ae_wr16(r, local_points,
                    ae_rd16(r, AE_FX_PLAYER + AE_FX_PLAYER_POSITION) -
                    ae_rd16(r, group + AE_FX_PROJECTILE_GROUP_POSITION));
                ae_wr16(r, local_points + 2u, 0u);
                ae_wr16(r, local_points + 4u,
                    ae_rd16(r, AE_FX_PLAYER + AE_FX_PLAYER_POSITION + 8u) -
                    ae_rd16(r, group + AE_FX_PROJECTILE_GROUP_POSITION + 4u));
                (void)ae_sdk_VectorNormalSS(r, local_points, local_points);
                if (ae_ram_failed(r)) return;
                uint32_t overlay_kind = ae_rd16(r, AE_FX_PROJECTILE_GROUP_OVERLAY_KIND);
                if (overlay_kind == 66u) {
                    (void)ae_FUN_8014263c(r, h,
                        group + AE_FX_PROJECTILE_GROUP_POSITION, local_points);
                } else if (overlay_kind == 56u) {
                    (void)ae_FUN_8014022c(r, h,
                        group + AE_FX_PROJECTILE_GROUP_POSITION, local_points);
                }
                if (ae_ram_failed(r)) return;
                continue;
            }
            if (group_state == 1u && record_state == 1u) {
                ae_wr8(r, group + AE_FX_PROJECTILE_GROUP_ACTIVE, 7u);
                ae_wr8(r, group + AE_FX_PROJECTILE_GROUP_PROGRESS, 0u);
                for (uint32_t axis = 0; axis < 3u; axis += 2u) {
                    int32_t speed = ae_rds16(r, record + AE_FX_PROJECTILE_RECORD_POSITION + axis * 2u);
                    ae_wr16(r, group + AE_FX_PROJECTILE_GROUP_VELOCITY + axis * 2u,
                        (uint32_t)fx_asr32(-speed + 7, 3));
                }
                uint32_t seed = (ae_rd16(r, AE_FX_RANDOM_SEED) * 5u + 1u) & 0xFFFFu;
                ae_wr16(r, AE_FX_RANDOM_SEED, seed);
                ae_wr16(r, group + AE_FX_PROJECTILE_GROUP_VELOCITY + 2u,
                        (uint32_t)-(int32_t)((seed >> 8) & 0x3Fu));
                continue;
            } else {
                ae_wr8(r, group + AE_FX_PROJECTILE_GROUP_ACTIVE, 0u);
                continue;
            }
        }

        group_state = ae_rd8(r, group + AE_FX_PROJECTILE_GROUP_ACTIVE);
        if (group_state == 5u) {
            fx_projectile_spawn_at_group(r, h, 4, group);
            if (ae_ram_failed(r)) return;
            if (record != 0u) {
                ae_wr16(r, record + AE_FX_PROJECTILE_RECORD_HEIGHT,
                    ae_rd16(r, record + AE_FX_PROJECTILE_RECORD_HEIGHT) + 4u);
            }
        } else if (group_state == 3u) {
            fx_projectile_group_track_owner(r, group, record);
            if (ae_ram_failed(r)) return;
        } else if (group_state == 7u) {
            fx_projectile_group_ground_step(r, h, group);
            if (ae_ram_failed(r)) return;
        } else if (group_state == 6u) {
            uint32_t progress = (ae_rd8(r, group + AE_FX_PROJECTILE_GROUP_PROGRESS) + 1u) & 0xFFu;
            ae_wr8(r, group + AE_FX_PROJECTILE_GROUP_PROGRESS, progress);
            if (progress >= 10u) {
                ae_wr8(r, group + AE_FX_PROJECTILE_GROUP_ACTIVE, 0u);
            } else {
                fx_projectile_group_body(r, h, group, 17,
                    (progress * 3u) << 4, 0x0203u, AE_FX_PROJECTILE_BODY_OWNER);
                if (ae_ram_failed(r)) return;
            }
            continue;
        }

        group_state = ae_rd8(r, group + AE_FX_PROJECTILE_GROUP_ACTIVE);
        if (group_state != 6u) {
            uint32_t direction = record + AE_FX_PROJECTILE_RECORD_POSITION;
            uint32_t model = 0x801FF0A0u;
            uint32_t model_id = ae_rd8(r, AE_FX_PROJECTILE_GROUP_MODEL_TABLE + group_state);
            (void)ae_sdk_MatrixLookDir(r, direction, model);
            if (ae_ram_failed(r)) return;
            for (uint32_t axis = 0; axis < 3u; ++axis) {
                ae_wr32(r, local_compose + AE_FX_MATRIX_TRANSLATION + axis * 4u,
                    (uint32_t)ae_rds16(r, group + AE_FX_PROJECTILE_GROUP_POSITION + axis * 2u));
            }
            (void)ae_sdk_CompMatrix(r, model, local_compose + AE_FX_MATRIX_TRANSLATION, model);
            if (ae_ram_failed(r)) return;
            (void)ae_sdk_CompMatrix(r, AE_FX_VIEW_MATRIX, model, model);
            if (ae_ram_failed(r)) return;
            (void)FX_CALL(r, h, ModelTransformRigid,
                AE_FX_PROJECTILE_GROUP_MODEL_SOURCE + model_id * 16u,
                model, AE_FX_PROJECTILE_GROUP_MODEL_INSTANCES, 0);
            if (ae_ram_failed(r)) return;
            uint32_t object_index = ae_rd32(r, AE_FX_RIBBON_OT_TABLE + AE_FX_RIBBON_OT_OBJECT_INDEX);
            uint32_t object = AE_FX_PROJECTILE_GROUP_MODEL_OBJECTS + object_index * 0x4078u;
            (void)FX_CALL(r, h, ModelDraw, object,
                AE_FX_PROJECTILE_GROUP_MODEL_INSTANCES,
                AE_FX_PROJECTILE_GROUP_MODEL_SOURCE + model_id * 16u, 0);
            if (ae_ram_failed(r)) return;
        }
    }
}

