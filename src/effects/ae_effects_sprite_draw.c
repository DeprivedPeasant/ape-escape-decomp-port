/* Sprite queue aging and native renderer dispatch. */
#include "ae_effects.h"

enum {
    AE_SPRITE_FX_X = 0,
    AE_SPRITE_FX_Y = 2,
    AE_SPRITE_FX_Z = 4,
    AE_SPRITE_FX_RENDER_SEED = 0x1F800000u
};

static uint32_t sprite_draw_next_seed(uint32_t *seed) {
    *seed = *seed * 5u + 1u;
    return *seed;
}

static void sprite_draw_set_matrix(AeRam *r, const AeEffectsHooks *hooks,
                                   AeEffectsCallFn callback, const char *name) {
    if (hooks == NULL || callback == NULL) {
        ae_ram_fail(r, "AeEffectsHooks.%s not provided", name);
        return;
    }
    (void)callback(hooks->user, r, AE_G_EFFECTS_CAMERA_MATRIX, 0, 0, 0);
}

static void sprite_draw_dispatch(AeRam *r, const AeEffectsHooks *hooks,
                                 uint32_t sprite, uint32_t age, uint32_t kind) {
    if (hooks == NULL || hooks->GpuDrawSprite == NULL) {
        ae_ram_fail(r, "AeEffectsHooks.GpuDrawSprite not provided");
        return;
    }
    (void)hooks->GpuDrawSprite(hooks->user, r, sprite, age, kind, 0);
}

/* SpriteEffectsDraw @8006D5A8. Preserve the effect's random drift and life
 * counter in RAM; the native renderer expands the queued sprite into a quad. */
void ae_SpriteEffectsDraw(AeRam *r, const AeEffectsHooks *hooks) {
    sprite_draw_set_matrix(r, hooks, hooks == NULL ? NULL : hooks->SetRotMatrix,
                           "SetRotMatrix");
    if (ae_ram_failed(r)) return;
    sprite_draw_set_matrix(r, hooks, hooks == NULL ? NULL : hooks->SetTransMatrix,
                           "SetTransMatrix");
    if (ae_ram_failed(r)) return;

    uint32_t seed = ae_rd16(r, AE_G_TRAIL_POINT_SEED);
    ae_wr32(r, AE_SPRITE_FX_RENDER_SEED, seed);

    for (uint32_t index = 0; index < AE_SPRITE_FX_COUNT; ++index) {
        uint32_t sprite = AE_G_SPRITE_FX_QUEUE + index * AE_SPRITE_FX_SIZE;
        uint32_t age = ae_rd8(r, sprite + AE_SPRITE_FX_AGE);
        if (age == 0u) continue;

        uint32_t kind = ae_rd8(r, sprite + AE_SPRITE_FX_KIND);
        if (kind != 1u) {
            seed = sprite_draw_next_seed(&seed);
            ae_wr32(r, AE_SPRITE_FX_RENDER_SEED, seed);
            ae_wr16(r, sprite + AE_SPRITE_FX_X,
                    ae_rd16(r, sprite + AE_SPRITE_FX_X) + (seed & 0xFu) - 7u);

            seed = sprite_draw_next_seed(&seed);
            ae_wr32(r, AE_SPRITE_FX_RENDER_SEED, seed);
            ae_wr16(r, sprite + AE_SPRITE_FX_Y,
                    ae_rd16(r, sprite + AE_SPRITE_FX_Y) + (seed & 0xFu) - 28u);

            seed = sprite_draw_next_seed(&seed);
            ae_wr32(r, AE_SPRITE_FX_RENDER_SEED, seed);
            ae_wr16(r, sprite + AE_SPRITE_FX_Z,
                    ae_rd16(r, sprite + AE_SPRITE_FX_Z) + (seed & 0xFu) - 7u);
        }

        for (uint32_t vertex = 0; vertex < 3u; ++vertex) {
            seed = sprite_draw_next_seed(&seed);
            ae_wr32(r, AE_SPRITE_FX_RENDER_SEED, seed);
        }
        sprite_draw_dispatch(r, hooks, sprite, age, kind);
        if (ae_ram_failed(r)) return;

        ae_wr8(r, sprite + AE_SPRITE_FX_AGE, age - 1u);
    }
}
