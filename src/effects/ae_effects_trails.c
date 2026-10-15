/* Trail aging and renderer dispatch. */
#include "ae_effects.h"

static int32_t effects_ring_draw(AeRam *r, const AeEffectsHooks *hooks,
                                 uint32_t trail, uint32_t step,
                                 uint32_t color, uint32_t intensity) {
    if (hooks == NULL || hooks->EffectRingDraw == NULL) {
        ae_ram_fail(r, "AeEffectsHooks.EffectRingDraw not provided");
        return 0;
    }
    return hooks->EffectRingDraw(hooks->user, r, trail, step, color, intensity);
}

/* 8006EE3C: countdown delayed trails; draw ready records with a fading
 * intensity, then smooth their step toward the configured width. */
void ae_TrailsUpdate(AeRam *r, const AeEffectsHooks *hooks) {
    for (uint32_t index = 0; index < AE_EFFECT_TRAIL_COUNT; ++index) {
        uint32_t trail = AE_TRAILS + index * AE_TRAIL_SIZE;
        uint32_t life = ae_rd8(r, trail + AE_T_LIFE);
        if (life == 0u) {
            continue;
        }

        uint32_t delay = ae_rd8(r, trail + AE_T_MODE);
        if (delay != 0u) {
            ae_wr8(r, trail + AE_T_MODE, delay - 1u);
            continue;
        }

        uint32_t step = ae_rd16(r, trail + AE_T_STEP);
        uint32_t color = ae_rd16(r, trail + AE_T_COLOR);
        uint32_t intensity = (life * 6u) & 0xFFu;
        ae_wr8(r, trail + AE_T_LIFE, life - 1u);
        (void)effects_ring_draw(r, hooks, trail, step, color,
                                intensity | (intensity << 8) | (intensity << 16));
        if (ae_ram_failed(r)) {
            return;
        }

        uint32_t width = ae_rd16(r, trail + AE_T_WIDTH);
        uint32_t adjusted = step * 7u + width;
        ae_wr16(r, trail + AE_T_STEP, adjusted >> 3);
    }
}
