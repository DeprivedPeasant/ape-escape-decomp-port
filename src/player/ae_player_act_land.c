#include "ae_player_act.h"

#include <stdint.h>

#define AE_PA_LAND_SOFT_LIMIT 6
#define AE_PA_LAND_STATE_SPECIAL 0x45u
#define AE_PA_LAND_STATE_RECOVER 0x18u
#define AE_PA_LAND_STATE_HURT 0x4Bu
#define AE_PA_LAND_ATTACH_FLAG 0x04000000u

static int32_t pa_land_is_long_fall(AeRam *r)
{
    return ae_rds16(r, AE_PLAYER_ACT_PLAYER + AE_PA_PLAYER_FALL_FRAMES) >= AE_PA_LAND_SOFT_LIMIT;
}

static int pa_land_rumble(AeRam *r, const AePlayerActHooks *hooks,
                          int32_t motor, int32_t strength, int32_t frames)
{
    if (hooks == NULL || hooks->PadRumble == NULL) {
        ae_ram_fail(r, "PadRumble hook not provided");
        return 0;
    }
    hooks->PadRumble(hooks->user, r, motor, strength, frames);
    return 1;
}

static int pa_land_sound(AeRam *r, const AePlayerActHooks *hooks, int32_t sound_id)
{
    if (hooks == NULL || hooks->PlaySfx == NULL) {
        ae_ram_fail(r, "PlaySfx hook not provided");
        return 0;
    }
    hooks->PlaySfx(hooks->user, r, sound_id);
    return 1;
}

static int pa_land_voice(AeRam *r, const AePlayerActHooks *hooks, int32_t voice_id)
{
    if (hooks == NULL || hooks->PlayPlayerVoice == NULL) {
        ae_ram_fail(r, "PlayPlayerVoice hook not provided");
        return 0;
    }
    hooks->PlayPlayerVoice(hooks->user, r, voice_id);
    return 1;
}

static int pa_land_damage(AeRam *r, const AePlayerActHooks *hooks, int32_t amount)
{
    if (hooks == NULL || hooks->PlayerDamage == NULL) {
        ae_ram_fail(r, "PlayerDamage hook not provided");
        return 0;
    }
    hooks->PlayerDamage(hooks->user, r, amount);
    return 1;
}

static int pa_land_ground_impact(AeRam *r, const AePlayerActHooks *hooks)
{
    if (hooks == NULL || hooks->EffectGroundImpactAtPlayer == NULL) {
        ae_ram_fail(r, "EffectGroundImpactAtPlayer hook not provided");
        return 0;
    }
    hooks->EffectGroundImpactAtPlayer(hooks->user, r);
    return 1;
}

void ae_PlayerAct_Land(AeRam *r, const AePlayerActHooks *hooks)
{
    const uint32_t player = AE_PLAYER_ACT_PLAYER;
    const uint32_t state = ae_rd16(r, player + AE_PA_PLAYER_STATE);
    int32_t sound_id;

    if (state == AE_PA_LAND_STATE_SPECIAL) {
        if (!pa_land_ground_impact(r, hooks)) return;
        if (pa_land_is_long_fall(r)) {
            if (!pa_land_damage(r, hooks, 1)) return;
            if (!pa_land_rumble(r, hooks, 1, 0xF0, 8)) return;
            ae_PlayerSetState(r, hooks, AE_PA_LAND_STATE_RECOVER);
            if (ae_ram_failed(r)) return;
            if (!pa_land_voice(r, hooks, 9)) return;
            sound_id = 0x43;
        } else {
            if (!pa_land_rumble(r, hooks, 0, 1, 2)) return;
            sound_id = 0x42;
        }
    } else if (pa_land_is_long_fall(r)) {
        if (!pa_land_damage(r, hooks, 1)) return;
        if (!pa_land_rumble(r, hooks, 1, 0xF0, 8)) return;
        if (!pa_land_ground_impact(r, hooks)) return;
        ae_wr16(r, player + AE_PA_PLAYER_SQUASH_TIMER, 0x3C);
        ae_PlayerSetState(r, hooks, AE_PA_LAND_STATE_HURT);
        if (ae_ram_failed(r)) return;
        if (!pa_land_voice(r, hooks, 9)) return;
        sound_id = 0x43;
    } else if (ae_rds32(r, player + AE_PA_PLAYER_MOVE_VEL + AE_PA_VECTOR_Y) < 0x21) {
        if (!pa_land_rumble(r, hooks, 0, 1, 1)) return;
        sound_id = 0x13;
    } else {
        sound_id = 0x12;
    }

    if (!pa_land_sound(r, hooks, sound_id)) return;
    ae_wr32(r, player + AE_PA_PLAYER_ATTACH_FLAGS,
            ae_rd32(r, player + AE_PA_PLAYER_ATTACH_FLAGS) | AE_PA_LAND_ATTACH_FLAG);
}
