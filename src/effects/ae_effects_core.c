/* Readable RAM-image ports of small, self-contained effect functions. */
#include "ae_effects.h"

/* 8005DC78: stage exits are allowed for exit codes 3..5 except the three locked
 * player states and the state class encoded in the high flag nibble. */
int32_t ae_PlayerCanExitStage(AeRam *r) {
    uint32_t exit_code = ae_rd8(r, AE_G_STAGE_EXIT_CODE);
    if (exit_code < 3u || exit_code > 5u) {
        return 0;
    }

    uint32_t player_state = ae_rd16(r, AE_G_PLAYER + AE_PLAYER_STATE);
    if (player_state == 0x23u || player_state == 0x4Au || player_state == 0x2Du) {
        return 0;
    }
    uint32_t state_class = ae_rd32(r, AE_G_PLAYER + AE_PLAYER_STATE_FLAGS) & 0xF0u;
    return state_class != 0xE0u;
}

/* 8006EF08: search backwards from point 7 and insert into the first inactive
 * point. The cursor is an LCG whose high bits select a life of 12..15 frames. */
void ae_TrailPointAdd(AeRam *r, uint32_t position) {
    for (int32_t index = AE_TRAIL_POINT_COUNT - 1; index >= 0; --index) {
        uint32_t point = AE_G_TRAIL_POINTS + (uint32_t)index * AE_TRAIL_POINT_SIZE;
        if (ae_rd8(r, point + AE_TRAIL_POINT_ACTIVE) != 0u) {
            continue;
        }

        uint32_t seed = (ae_rd16(r, AE_G_TRAIL_POINT_SEED) * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_G_TRAIL_POINT_SEED, seed);
        ae_wr8(r, point + AE_TRAIL_POINT_ACTIVE,
               AE_TRAIL_POINT_LIFE_BASE + ((seed >> 8) & 3u));
        for (uint32_t axis = 0; axis < 3u; ++axis) {
            ae_wr16(r, point + 2u * axis, ae_rd16(r, position + 2u * axis));
        }
        return;
    }
}

/* 8006D4C0: copy the eight-byte specification, then replace the slot's age and
 * kind bytes. The stored cursor wraps modulo the 32-slot ring. */
void ae_SpriteEffectQueue(AeRam *r, uint32_t spec, int32_t kind) {
    uint32_t cursor = ae_rd8(r, AE_G_SPRITE_FX_CURSOR);
    uint32_t slot = AE_G_SPRITE_FX_QUEUE + cursor * AE_SPRITE_FX_SIZE;

    for (uint32_t byte = 0; byte < AE_SPRITE_FX_SIZE; ++byte) {
        ae_wr8(r, slot + byte, ae_rd8(r, spec + byte));
    }
    ae_wr8(r, slot + AE_SPRITE_FX_AGE, AE_SPRITE_FX_INITIAL_AGE);
    ae_wr8(r, slot + AE_SPRITE_FX_KIND, (uint32_t)kind);
    ae_wr8(r, AE_G_SPRITE_FX_CURSOR, (cursor + 1u) & (AE_SPRITE_FX_COUNT - 1u));
}

/* 8006D588: queue a sprite effect in category 2. */
void ae_SpriteEffectQueue2(AeRam *r, uint32_t spec) {
    ae_SpriteEffectQueue(r, spec, 2);
}
