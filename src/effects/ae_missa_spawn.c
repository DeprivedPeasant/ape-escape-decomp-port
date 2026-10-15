#include "../core/ae_missa.h"
#include "../core/ae_sdk.h"
#include "../player/ae_player_spawn.h"

#define AE_MISSA_PLAYER_FX_PLAYER         0x800EC1E0u
#define AE_MISSA_PLAYER_POSITION_X        0x24u
#define AE_MISSA_PLAYER_POSITION_Y        0x28u
#define AE_MISSA_PLAYER_POSITION_Z        0x2Cu
#define AE_MISSA_PLAYER_VELOCITY_X        0x30u
#define AE_MISSA_PLAYER_VELOCITY_Y        0x34u
#define AE_MISSA_PLAYER_VELOCITY_Z        0x38u
#define AE_MISSA_FX_MATRIX_POINTER        0x800FC0DCu
#define AE_MISSA_FX_MATRIX_DIRECTION      0x60u
#define AE_MISSA_FX_MATRIX_OUTPUT         0x800BAD90u
#define AE_MISSA_FX_GROUP_TABLE_PARTICLES 0x800B175Cu
#define AE_MISSA_FX_GROUP_TABLE_GROUPS    0x800B1828u
#define AE_MISSA_FX116_POSITION           0x800B4D88u
#define AE_MISSA_FX116_DEFINITION         0x800B1C28u
#define AE_MISSA_FX116_DIRECTION          0x800EC240u
#define AE_MISSA_FX116_PARTICLE_TYPE       278
#define AE_MISSA_JUMP_BURST_DEFINITION    0x800B1E74u
#define AE_MISSA_TEMP_LOCAL_BASE          0x801FF000u
#define AE_MISSA_BREAK_PARTICLE_TYPE      265
#define AE_MISSA_BREAK_PARTICLE_ALT_A     261
#define AE_MISSA_BREAK_PARTICLE_ALT_B     259
#define AE_MISSA_SPAWN_EFFECT_ID_A        75
#define AE_MISSA_SPAWN_EFFECT_ID_B        95
#define AE_MISSA_SPAWN_EFFECT_ID_C        77
#define AE_MISSA_SPAWN_EFFECT_ID_D        185

static int32_t missa_spawn_clear_words(AeRam *r, const AeMissaHooks *hooks,
                                       uint32_t address, uint32_t value,
                                       uint32_t word_count)
{
    if (hooks == NULL || hooks->ClearWords == NULL) {
        ae_ram_fail(r, "player effect clear words hook missing");
        return 0;
    }
    return hooks->ClearWords(hooks->user, r, address, value, word_count, 0);
}

static int32_t missa_spawn_hook(AeRam *r, const AeMissaHooks *hooks,
                                AeMissaHookFn callback, const char *name,
                                uint32_t a0, uint32_t a1, uint32_t a2,
                                uint32_t a3)
{
    if (hooks == NULL || callback == NULL) {
        ae_ram_fail(r, name);
        return 0;
    }
    return callback(hooks->user, r, a0, a1, a2, a3);
}

static void missa_spawn_play_sfx(AeRam *r, const AeMissaHooks *hooks,
                                 int32_t sound_id)
{
    (void)ae_PlaySfx(r, hooks != NULL ? hooks->sound : NULL, sound_id);
}

void ae_SpawnEffectGroupAt(AeRam *r, const AeMissaHooks *hooks,
                           int32_t type, int32_t x, int32_t y, int32_t z)
{
    uint32_t entry_sp = hooks != NULL && hooks->stack_pointer != 0u ?
                        hooks->stack_pointer : 0x801FFF00u;
    uint32_t position = entry_sp - 16u;
    ae_wr16(r, position, (uint32_t)x);
    ae_wr16(r, position + 2u, (uint32_t)y);
    ae_wr16(r, position + 4u, (uint32_t)z);
    ae_SpawnEffectGroup(r, hooks != NULL ? hooks->spawn : NULL, type, position,
                        AE_MISSA_FX_GROUP_TABLE_PARTICLES,
                        AE_MISSA_FX_GROUP_TABLE_GROUPS);
}

void ae_BreakableFinalizeVisualUpdate(AeRam *r, const AeMissaHooks *hooks,
                                      uint32_t position, uint32_t model,
                                      int32_t kind)
{
    ae_SpawnParticle(r, hooks != NULL ? hooks->spawn : NULL,
                     AE_MISSA_BREAK_PARTICLE_TYPE, position);
    (void)missa_spawn_hook(r, hooks,
        hooks != NULL ? hooks->PropVisualUpdate : NULL,
        "breakable visual update hook missing", position, model, 0, 0);
    if (kind == 1) {
        missa_spawn_play_sfx(r, hooks, AE_MISSA_SPAWN_EFFECT_ID_C);
        missa_spawn_play_sfx(r, hooks, AE_MISSA_SPAWN_EFFECT_ID_B);
        return;
    }
    if (kind >= 2 && kind != 2) {
        ae_SpawnParticle(r, hooks != NULL ? hooks->spawn : NULL,
                         AE_MISSA_BREAK_PARTICLE_ALT_A, position);
        ae_SpawnParticle(r, hooks != NULL ? hooks->spawn : NULL,
                         AE_MISSA_BREAK_PARTICLE_ALT_B, position);
        missa_spawn_play_sfx(r, hooks, AE_MISSA_SPAWN_EFFECT_ID_A);
        missa_spawn_play_sfx(r, hooks, AE_MISSA_SPAWN_EFFECT_ID_B);
        return;
    }
    if (kind < 2) {
        ae_SpawnParticle(r, hooks != NULL ? hooks->spawn : NULL,
                         AE_MISSA_BREAK_PARTICLE_ALT_A, position);
        ae_SpawnParticle(r, hooks != NULL ? hooks->spawn : NULL,
                         AE_MISSA_BREAK_PARTICLE_ALT_B, position);
        missa_spawn_play_sfx(r, hooks, AE_MISSA_SPAWN_EFFECT_ID_A);
        missa_spawn_play_sfx(r, hooks, AE_MISSA_SPAWN_EFFECT_ID_B);
        return;
    }
    missa_spawn_play_sfx(r, hooks, AE_MISSA_SPAWN_EFFECT_ID_D);
}

void ae_PlayerFxSpawn116(AeRam *r, const AeSpawnHooks *hooks)
{
    ae_EffectSpawnFromDef(r, hooks, AE_MISSA_FX116_POSITION,
                          AE_MISSA_FX116_DEFINITION, 1, 0,
                          AE_MISSA_FX116_DIRECTION);
    ae_SpawnParticle(r, hooks, AE_MISSA_FX116_PARTICLE_TYPE,
                     AE_MISSA_FX116_POSITION);
}

void ae_PlayerFxJumpBurst(AeRam *r, const AeMissaHooks *hooks)
{
    uint32_t entry_sp = hooks != NULL && hooks->stack_pointer != 0u ?
                        hooks->stack_pointer : 0x801FFF00u;
    uint32_t frame_sp = entry_sp - 64u;
    uint32_t position = frame_sp + 24u;
    uint32_t direction = frame_sp + 32u;
    uint32_t velocity = frame_sp + 40u;
    uint32_t player = AE_MISSA_PLAYER_FX_PLAYER;

    (void)missa_spawn_clear_words(r, hooks, direction, 0u, 8u);
    (void)missa_spawn_clear_words(r, hooks, velocity, 0u, 8u);
    ae_wr16(r, direction, ae_rd16(r, player + AE_MISSA_PLAYER_POSITION_X));
    ae_wr16(r, direction + 2u,
            ae_rd16(r, player + AE_MISSA_PLAYER_POSITION_Y) - 80u);
    ae_wr16(r, direction + 4u, ae_rd16(r, player + AE_MISSA_PLAYER_POSITION_Z));
    for (uint32_t i = 0; i < 8u; ++i) {
        ae_wr8(r, position + i, ae_rd8(r, direction + i));
    }
    ae_wr16(r, velocity, ae_rd16(r, player + AE_MISSA_PLAYER_VELOCITY_X));
    ae_wr16(r, velocity + 2u, ae_rd16(r, player + AE_MISSA_PLAYER_VELOCITY_Y));
    ae_wr16(r, velocity + 4u, ae_rd16(r, player + AE_MISSA_PLAYER_VELOCITY_Z));
    for (uint32_t i = 0; i < 8u; ++i) {
        ae_wr8(r, direction + i, ae_rd8(r, velocity + i));
    }

    ae_EffectSpawnFromDef(r, hooks != NULL ? hooks->spawn : NULL, position,
                          AE_MISSA_JUMP_BURST_DEFINITION, 1, 0, direction);
    for (uint32_t burst = 0; burst < 2u; ++burst) {
        ae_SpawnParticle(r, hooks != NULL ? hooks->spawn : NULL, 4, position);
        ae_SpawnParticle(r, hooks != NULL ? hooks->spawn : NULL, 3, position);
    }
}

void ae_PlayerFxSetMatrixPos(AeRam *r, uint32_t source)
{
    uint32_t vector = ae_rd32(r, AE_MISSA_FX_MATRIX_POINTER) +
                      AE_MISSA_FX_MATRIX_DIRECTION;
    (void)ae_sdk_ApplyMatrixSV(r, source, vector,
                               AE_MISSA_FX_MATRIX_OUTPUT);
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        uint32_t component = 2u * axis;
        uint32_t value = ae_rd16(r, AE_MISSA_FX_MATRIX_OUTPUT + component) +
                         ae_rd16(r, source + 0x14u + component * 2u);
        ae_wr16(r, AE_MISSA_FX_MATRIX_OUTPUT + component, value);
    }
}
