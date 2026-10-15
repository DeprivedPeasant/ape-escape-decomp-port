/* Gadget/player attachment routines ported from the original EXE onto AeRam. */
#include "ae_gadget.h"

#include "../bodies/ae_bodies.h"
#include "../core/ae_sdk.h"
#include "../core/ae_sdk_matrix.h"
#include "../player/native_player.h"
#include "../world/ae_solid.h"
#include "../world/ae_world.h"

#include <stddef.h>

static int32_t gadget_call(AeRam *r, const AeGadgetHooks *h, AeGadgetHookFn fn,
                           const char *name, uint32_t a0, uint32_t a1,
                           uint32_t a2, uint32_t a3) {
    if (!fn) {
        ae_ram_fail(r, "gadget hook %s not provided", name);
        return 0;
    }
    return fn(h->user, r, a0, a1, a2, a3, 0);
}

static int32_t gadget_call5(AeRam *r, const AeGadgetHooks *h, AeGadgetHookFn fn,
                            const char *name, uint32_t a0, uint32_t a1,
                            uint32_t a2, uint32_t a3, uint32_t a4) {
    if (!fn) {
        ae_ram_fail(r, "gadget hook %s not provided", name);
        return 0;
    }
    return fn(h->user, r, a0, a1, a2, a3, a4);
}

static int32_t gadget_asr_s16_6(int32_t value) {
    /* MIPS first adds 63 only for negative inputs, then uses an arithmetic shift: truncation toward zero. */
    return value / 64;
}

static uint32_t gadget_wrap_add(uint32_t a, uint32_t b) {
    return a + b;
}

static void gadget_temp_svector(AeRam *r, int32_t x, int32_t y, int32_t z) {
    ae_wr16(r, AE_G_GADGET_TEMP_VECTOR, (uint32_t)x);
    ae_wr16(r, AE_G_GADGET_TEMP_VECTOR + 2u, (uint32_t)y);
    ae_wr16(r, AE_G_GADGET_TEMP_VECTOR + 4u, (uint32_t)z);
}

#define GADGET_CALL(r, h, hook, a0, a1, a2, a3) \
    gadget_call((r), (h), (h) ? (h)->hook : NULL, #hook, \
                (uint32_t)(a0), (uint32_t)(a1), (uint32_t)(a2), (uint32_t)(a3))
#define GADGET_CALL5(r, h, hook, a0, a1, a2, a3, a4) \
    gadget_call5((r), (h), (h) ? (h)->hook : NULL, #hook, \
                (uint32_t)(a0), (uint32_t)(a1), (uint32_t)(a2), (uint32_t)(a3), (uint32_t)(a4))

static int32_t gadget_world_GetSolidHeight(void *user, AeRam *r,
                                            uint32_t a0, uint32_t a1,
                                            uint32_t a2, uint32_t a3) {
    return GADGET_CALL(r, (const AeGadgetHooks *)user, GetSolidHeight, a0, a1, a2, a3);
}

static int32_t gadget_world_PlaySfxAt(void *user, AeRam *r,
                                      uint32_t a0, uint32_t a1,
                                      uint32_t a2, uint32_t a3) {
    return GADGET_CALL(r, (const AeGadgetHooks *)user, PlaySfxAt, a0, a1, a2, a3);
}

static int32_t gadget_world_GimmickActivate(void *user, AeRam *r,
                                             uint32_t a0, uint32_t a1,
                                             uint32_t a2, uint32_t a3) {
    return GADGET_CALL(r, (const AeGadgetHooks *)user, GimmickActivate, a0, a1, a2, a3);
}

static int32_t gadget_world_MeshInitFromAreaTable(void *user, AeRam *r,
                                                   uint32_t a0, uint32_t a1,
                                                   uint32_t a2, uint32_t a3) {
    return GADGET_CALL(r, (const AeGadgetHooks *)user, MeshInitFromAreaTable, a0, a1, a2, a3);
}

static int32_t gadget_world_AreaEffect(void *user, AeRam *r,
                                        uint32_t a0, uint32_t a1,
                                        uint32_t a2, uint32_t a3) {
    return GADGET_CALL(r, (const AeGadgetHooks *)user, AreaEffect, a0, a1, a2, a3);
}

static int32_t gadget_world_CameraShake(void *user, AeRam *r,
                                         uint32_t a0, uint32_t a1,
                                         uint32_t a2, uint32_t a3) {
    return GADGET_CALL(r, (const AeGadgetHooks *)user, CameraShake, a0, a1, a2, a3);
}

static int32_t gadget_world_EventCamStart(void *user, AeRam *r,
                                          uint32_t a0, uint32_t a1,
                                          uint32_t a2, uint32_t a3) {
    return GADGET_CALL(r, (const AeGadgetHooks *)user, EventCamStart, a0, a1, a2, a3);
}

static AeWorldHooks gadget_world_hooks(const AeGadgetHooks *h) {
    AeWorldHooks world = {0};
    world.user = (void *)h;
    world.GetSolidHeight = gadget_world_GetSolidHeight;
    world.PlaySfxAt = gadget_world_PlaySfxAt;
    world.GimmickActivate = gadget_world_GimmickActivate;
    world.MeshInitFromAreaTable = gadget_world_MeshInitFromAreaTable;
    world.FUN_8013d358 = gadget_world_AreaEffect;
    world.CameraShake = gadget_world_CameraShake;
    world.EventCamStart = gadget_world_EventCamStart;
    return world;
}

static void gadget_body_printf_error(void *user, AeRam *r, uint32_t format, int32_t type) {
    GADGET_CALL(r, (const AeGadgetHooks *)user, PrintfError, format, (uint32_t)type, 0, 0);
}

static int32_t gadget_body_sound_loop_start(void *user, AeRam *r, int32_t a, int32_t id) {
    return GADGET_CALL(r, (const AeGadgetHooks *)user, SoundLoopStart,
                       (uint32_t)a, (uint32_t)id, 0, 0);
}

static void gadget_body_sound_loop_stop(void *user, AeRam *r, int32_t id, int32_t handle) {
    GADGET_CALL(r, (const AeGadgetHooks *)user, SoundLoopStop,
                (uint32_t)id, (uint32_t)handle, 0, 0);
}

static int32_t gadget_body_gimmick_type_to_sfx(void *user, AeRam *r, int32_t type) {
    return GADGET_CALL(r, (const AeGadgetHooks *)user, GimmickTypeToSfx,
                       (uint32_t)type, 0, 0, 0);
}

static void gadget_body_play_sfx_at(void *user, AeRam *r, int32_t sfx, const int16_t position[3]) {
    uint32_t vector = AE_G_GADGET_TEMP_VECTOR;
    for (uint32_t axis = 0; axis < 3; ++axis) ae_wr16(r, vector + axis * 2u, (uint16_t)position[axis]);
    GADGET_CALL(r, (const AeGadgetHooks *)user, PlaySfxAt,
                (uint32_t)sfx, vector, 0, 0);
}

static void gadget_body_pickup_collect(void *user, AeRam *r, int32_t type) {
    GADGET_CALL(r, (const AeGadgetHooks *)user, PickupCollect, (uint32_t)type, 0, 0, 0);
}

static void gadget_body_camera_nudge(void *user, AeRam *r, int32_t dx, int32_t dy, int32_t dz) {
    GADGET_CALL(r, (const AeGadgetHooks *)user, CameraNudge,
                (uint32_t)dx, (uint32_t)dy, (uint32_t)dz, 0);
}

static AeBodyHooks gadget_body_hooks(const AeGadgetHooks *h) {
    AeBodyHooks body = {0};
    body.user = (void *)h;
    body.printf_error = gadget_body_printf_error;
    body.sound_loop_start = gadget_body_sound_loop_start;
    body.sound_loop_stop = gadget_body_sound_loop_stop;
    body.gimmick_type_to_sfx = gadget_body_gimmick_type_to_sfx;
    body.play_sfx_at = gadget_body_play_sfx_at;
    body.pickup_collect = gadget_body_pickup_collect;
    body.camera_nudge = gadget_body_camera_nudge;
    return body;
}

static int32_t gadget_s32(uint32_t bits) {
    if (bits <= INT32_MAX) return (int32_t)bits;
    return -1 - (int32_t)(UINT32_MAX - bits);
}

static int32_t gadget_sra(int32_t value, uint32_t shift) {
    uint32_t bits = (uint32_t)value;
    uint32_t amount = shift & 31u;
    if (amount == 0) return value;
    uint32_t result = bits >> amount;
    if (bits & 0x80000000u) result |= ~(0xFFFFFFFFu >> amount);
    return (int32_t)result;
}

static int32_t gadget_div_toward_zero(int32_t value, int32_t divisor) {
    if (divisor == 0) return 0;
    if (value == INT32_MIN && divisor == -1) return INT32_MIN;
    return value / divisor;
}

/* 80065694: gadgets 2, 5, and 7 use the alternate player stance. */
int32_t ae_GadgetHasAltStance(AeRam *r) {
    int32_t gadget = ae_rds8(r, AE_G_PLAYER_CURRENT_GADGET);
    return gadget == 2 || gadget == 5 || gadget == 7;
}

/* 8006C6A4: save the three signed-short coordinates and start a 30-frame flash. */
void ae_GadgetFlashSet(AeRam *r, uint32_t position) {
    uint32_t x = ae_rd16(r, position + AE_GADGET_POS_X);
    uint32_t y = ae_rd16(r, position + AE_GADGET_POS_Y);
    uint32_t z = ae_rd16(r, position + AE_GADGET_POS_Z);
    ae_wr16(r, AE_G_GADGET_FLASH_POS, x);
    ae_wr16(r, AE_G_GADGET_FLASH_POS + AE_GADGET_FLASH_Y, y - 64u);
    ae_wr8(r, AE_G_GADGET_FLASH_TIMER, 30);
    ae_wr16(r, AE_G_GADGET_FLASH_POS + AE_GADGET_FLASH_Z, z);
}

/* 8006C6D4: run the countdown effects for the currently selected gadget. */
void ae_GadgetFlashUpdate(AeRam *r, const AeGadgetHooks *h) {
    uint32_t timer = ae_rd8(r, AE_G_GADGET_FLASH_TIMER);
    if (timer == 0) return;

    /* The original copies two adjacent words so the stack vector also retains its fourth lane. */
    ae_wr32(r, AE_G_GADGET_FLASH_LOCAL_POSITION, ae_rd32(r, AE_G_GADGET_FLASH_POS));
    ae_wr32(r, AE_G_GADGET_FLASH_LOCAL_POSITION + 4u, ae_rd32(r, AE_G_GADGET_FLASH_POS + 4u));

    if (timer == 30u) {
        for (uint32_t particle = 0; particle < 3; ++particle) {
            for (uint32_t axis = 0; axis < 3; ++axis) {
                uint32_t seed = (ae_rd16(r, AE_G_GADGET6_SPARKLE_SEED) * 5u + 1u) & 0xFFFFu;
                ae_wr16(r, AE_G_GADGET6_SPARKLE_SEED, seed);
                uint32_t random_offset = AE_G_GADGET_RANDOM_VECTOR + axis * 2u;
                ae_wr16(r, random_offset, ((seed >> 4) & 0xFF0u));
            }
            GADGET_CALL5(r, h, EffectSpawnFromDef, AE_G_GADGET_FLASH_LOCAL_POSITION,
                         AE_G_FX_GADGET_FLASH_1, 1, particle, AE_G_GADGET_RANDOM_VECTOR);
            if (ae_ram_failed(r)) return;
        }
        GADGET_CALL5(r, h, EffectSpawnFromDef, AE_G_GADGET_FLASH_LOCAL_POSITION,
                     AE_G_FX_GADGET_FLASH_2, 1, 0, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL(r, h, SpawnParticle, 0x112u, AE_G_GADGET_FLASH_LOCAL_POSITION, 0, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL(r, h, SpawnParticle, 0x108u, AE_G_GADGET_FLASH_LOCAL_POSITION, 0, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL(r, h, TrailSpawn, AE_G_GADGET_FLASH_LOCAL_POSITION, 1, (uint32_t)-600, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL(r, h, PlaySfx, 0x36u, 0, 0, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL(r, h, PlaySfx, 0x37u, 0, 0, 0);
        if (ae_ram_failed(r)) return;
    }

    if (timer == 18u) {
        GADGET_CALL(r, h, PlaySfx, 0x38u, 0, 0, 0);
        if (ae_ram_failed(r)) return;
    } else if (timer == 14u) {
        GADGET_CALL(r, h, PlaySfx, 0x39u, 0, 0, 0);
        if (ae_ram_failed(r)) return;
    } else if (timer == 5u) {
        GADGET_CALL5(r, h, EffectSpawnFromDef, AE_G_GADGET_FLASH_LOCAL_POSITION,
                     AE_G_FX_GADGET_FLASH_3, 2, 3, 0);
        if (ae_ram_failed(r)) return;
    } else if (timer == 1u) {
        for (uint32_t particle = 0; particle < 20; ++particle) {
            GADGET_CALL(r, h, SpawnParticle, 41u, AE_G_GADGET_FLASH_LOCAL_POSITION, 0, 0);
            if (ae_ram_failed(r)) return;
        }
        GADGET_CALL(r, h, SpawnParticle, 0x112u, AE_G_GADGET_FLASH_LOCAL_POSITION, 0, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL(r, h, SpawnParticle, 0x109u, AE_G_GADGET_FLASH_LOCAL_POSITION, 0, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL(r, h, SpawnParticle, 0x108u, AE_G_GADGET_FLASH_LOCAL_POSITION, 0, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL5(r, h, EffectSpawnFromDef, AE_G_GADGET_FLASH_LOCAL_POSITION,
                     AE_G_FX_GADGET_FLASH_4, 1, 2, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL5(r, h, EffectSpawnFromDef, AE_G_GADGET_FLASH_LOCAL_POSITION,
                     AE_G_FX_GADGET_FLASH_5, 1, 0, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL(r, h, PlaySfx, 0x3Au, 0, 0, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL(r, h, PlaySfx, 0x3Bu, 0, 0, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL(r, h, PlaySfx, 0x3Cu, 0, 0, 0);
        if (ae_ram_failed(r)) return;
    }

    ae_wr8(r, AE_G_GADGET_FLASH_TIMER, timer - 1u);
}

/* 8006CDE0: place the gadget-selection sparkle just ahead of the player's facing vector. */
void ae_GadgetSelectFx(AeRam *r, const AeGadgetHooks *h) {
    if (ae_rd8(r, AE_G_GADGET_SELECT_FX_GUARD_A) == 1u ||
        ae_rd8(r, AE_G_GADGET_SELECT_FX_GUARD_B) == 1u) return;

    int32_t facing_x = ae_rds16(r, AE_G_PLAYER + AE_PLAYER_FACING + AE_PLAYER_FACING_X);
    int32_t facing_z = ae_rds16(r, AE_G_PLAYER + AE_PLAYER_FACING + AE_PLAYER_FACING_Z);
    uint32_t base_x = ae_rd16(r, AE_G_GADGET_SELECT_FX_ORIGIN);
    uint32_t base_y = ae_rd16(r, AE_G_GADGET_SELECT_FX_ORIGIN + 2u);
    uint32_t base_z = ae_rd16(r, AE_G_GADGET_SELECT_FX_ORIGIN + 4u);
    uint32_t vel_x = ae_rd32(r, AE_G_PLAYER + AE_PLAYER_VEL + AE_PLAYER_VEL_X);
    uint32_t vel_y = ae_rd32(r, AE_G_PLAYER + AE_PLAYER_VEL + AE_PLAYER_VEL_Y);
    uint32_t vel_z = ae_rd32(r, AE_G_PLAYER + AE_PLAYER_VEL + AE_PLAYER_VEL_Z);

    uint32_t x = gadget_wrap_add(base_x, (uint32_t)gadget_asr_s16_6(facing_x));
    uint32_t y = base_y;
    uint32_t z = gadget_wrap_add(base_z, (uint32_t)gadget_asr_s16_6(facing_z));
    x = gadget_wrap_add(x, vel_x << 2);
    y = gadget_wrap_add(y, vel_y << 2);
    z = gadget_wrap_add(z, vel_z << 2);
    gadget_temp_svector(r, (int32_t)x, (int32_t)y, (int32_t)z);
    GADGET_CALL(r, h, SpawnParticle, 0x115u, AE_G_GADGET_TEMP_VECTOR, 0, 0);
}

/* 8006CEF0: advance the shared sparkle seed and make one large, then sometimes one small, particle. */
void ae_Gadget6SparkleFx(AeRam *r, const AeGadgetHooks *h) {
    uint32_t seed = (uint16_t)ae_rd16(r, AE_G_GADGET6_SPARKLE_SEED);
    seed = (seed * 5u + 1u) & 0xFFFFu;
    ae_wr16(r, AE_G_GADGET6_SPARKLE_SEED, seed);
    uint32_t x_spread = seed >> 8;

    uint32_t x = ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POS + AE_GADGET_POS_X) + x_spread - 128u;
    uint32_t y = ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POS + AE_GADGET_POS_Y) - 256u;

    seed = (seed * 5u + 1u) & 0xFFFFu;
    ae_wr16(r, AE_G_GADGET6_SPARKLE_SEED, seed);
    uint32_t z_spread = seed >> 8;
    uint32_t z = ae_rd16(r, AE_G_PLAYER + AE_PLAYER_POS + AE_GADGET_POS_Z) + z_spread - 128u;
    gadget_temp_svector(r, (int32_t)x, (int32_t)y, (int32_t)z);
    GADGET_CALL(r, h, SpawnParticle, 48u, AE_G_GADGET_TEMP_VECTOR, 0, 0);
    if (ae_ram_failed(r)) return;
    if ((ae_rd32(r, AE_G_GADGET6_SPARKLE_GATE) & 3u) == 0) {
        GADGET_CALL(r, h, SpawnParticle, 4u, AE_G_GADGET_TEMP_VECTOR, 0, 0);
    }
}

static int32_t gadget7_interpolated_component(AeRam *r, uint32_t car_field,
                                             uint32_t angle_field, int32_t phase) {
    int32_t car_value = ae_rds32(r, car_field);
    int32_t angle_value = ae_rds16(r, angle_field);
    int32_t car_product = (int32_t)((uint32_t)car_value * (uint32_t)phase);
    int32_t angle_product = (int32_t)((uint32_t)angle_value * (uint32_t)(16 - phase));
    int32_t sum = (int32_t)((uint32_t)car_product + (uint32_t)angle_product);
    if (sum < 0) sum = (int32_t)((uint32_t)sum + 15u);
    return gadget_sra(sum, 4);
}

/* 8006D044: update the RC-car ribbon endpoints and select its inline trail renderer. */
void ae_Gadget7TrailUpdate(AeRam *r, const AeGadgetHooks *h) {
    if (ae_rds8(r, AE_G_PLAYER_CURRENT_GADGET) != 7) return;
    if (ae_rd16(r, AE_G_GADGET7_STAGE_GATE) == 10u ||
        ae_rd8(r, AE_G_GADGET_SELECT_FX_GUARD_A) == 1u) return;

    if (GADGET_CALL(r, h, PlayerCanExitStage, 0, 0, 0, 0) == 0 || ae_ram_failed(r)) return;
    if (GADGET_CALL(r, h, IsPlayerSwimming, 0, 0, 0, 0) != 0 || ae_ram_failed(r)) return;

    uint32_t state = AE_G_GADGET7_STATE;
    int32_t phase = ae_rd8(r, state + AE_GADGET7_COUNTER);
    ae_wr8(r, state + AE_GADGET7_COUNTER, (uint32_t)(phase + 1) % 12u);

    if ((ae_rd32(r, AE_G_GADGET4_DRAW_PHASE) & 1u) == 0) {
        GADGET_CALL(r, h, SdkVectorHelper_8002FB00, AE_G_CAMERA_LOOK_MATRIX, 0, 0, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL(r, h, SdkVectorHelper_8002FB30, AE_G_CAMERA_LOOK_MATRIX, 0, 0, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL(r, h, GpuDrawGadget7Trail, 0, 0, 0, 0);
        return;
    }

    int32_t ribbon_phase = phase;
    uint32_t endpoint_a = AE_G_GADGET_TEMP_VECTOR;
    uint32_t endpoint_b = AE_G_GADGET_TEMP_VECTOR + 8u;
    for (uint32_t endpoint = 0; endpoint < 2; ++endpoint) {
        uint32_t output = endpoint == 0 ? endpoint_a : endpoint_b;
        for (uint32_t axis = 0; axis < 3; ++axis) {
            uint32_t car_offset = AE_GADGET7_POS_X + axis * 4u;
            uint32_t angle_offset = AE_GADGET7_ANGLE_X + axis * 2u;
            int32_t value = gadget7_interpolated_component(
                r, AE_G_GADGET7_CAR + car_offset, state + angle_offset, ribbon_phase);
            ae_wr16(r, output + axis * 2u, (uint32_t)value);
        }

        uint32_t seed = (ae_rd16(r, AE_G_GADGET_TRAIL_SEED) * 5u + 1u) & 0xFFFFu;
        ae_wr16(r, AE_G_GADGET_TRAIL_SEED, seed);
        GADGET_CALL(r, h, RibbonDraw, endpoint_a, 2, AE_G_GADGET7_RIBBON_COLORS,
                    ((seed >> 8) & 3u) + 2u);
        if (ae_ram_failed(r)) return;
        ribbon_phase = (ribbon_phase + 5) % 35;
    }
}

/* 8006DAC8: clear the low five bits in each of the 40 trail records. */
void ae_Gadget4TrailReset(AeRam *r) {
    for (uint32_t i = 0; i < AE_GADGET4_TRAIL_COUNT; ++i) {
        uint32_t field = AE_G_GADGET_TRAIL4 + i * AE_GADGET4_TRAIL_STRIDE + AE_GADGET4_TRAIL_STATE;
        ae_wr16(r, field, ae_rd16(r, field) & (uint32_t)~AE_GADGET4_TRAIL_STATE_MASK);
    }
}

static uint32_t gadget_trail_lcg(uint32_t seed) {
    return seed * 5u + 1u;
}

static int32_t gadget_trail_offset(AeRam *r, uint32_t angle, int32_t radius, int cosine) {
    int32_t trig = cosine ? ae_sdk_rcos(r, (int32_t)angle) : ae_sdk_rsin(r, (int32_t)angle);
    int32_t product = (int32_t)((uint32_t)trig * (uint32_t)radius);
    return gadget_div_toward_zero(product, 4096);
}

/* 8006DAF8: age Super Hoop trail points and request one native draw per live point. */
void ae_Gadget4TrailUpdate(AeRam *r, const AeGadgetHooks *h) {
    if (ae_rds8(r, AE_G_PLAYER_CURRENT_GADGET) != 4) return;
    if (GADGET_CALL(r, h, PlayerCanExitStage, 0, 0, 0, 0) == 0 || ae_ram_failed(r)) return;

    uint32_t work = AE_G_GADGET_WORK;
    uint32_t player = AE_G_PLAYER;
    uint32_t emit_trails = ae_rd16(r, work + AE_GADGET4_TRAIL_MODE) == 1u ? 0u : 1u;
    ae_wr32(r, AE_GADGET_SCRATCH_TRAIL_EMIT, emit_trails);

    int32_t velocity_x_twice = (int32_t)((uint32_t)ae_rds32(r, player + AE_PLAYER_VEL + AE_PLAYER_VEL_X) << 1);
    int32_t velocity_z_twice = (int32_t)((uint32_t)ae_rds32(r, player + AE_PLAYER_VEL + AE_PLAYER_VEL_Z) << 1);
    int32_t velocity_x_third = gadget_div_toward_zero(velocity_x_twice, 3);
    int32_t velocity_z_third = gadget_div_toward_zero(velocity_z_twice, 3);
    int32_t trail_rate = ae_rds32(r, work + AE_GADGET4_TRAIL_RATE);
    int32_t radius = gadget_div_toward_zero(trail_rate, 6) + 90;

    ae_wr32(r, AE_GADGET_SCRATCH_SEED, ae_rd16(r, AE_G_GADGET_TRAIL_SEED));
    ae_wr32(r, AE_GADGET_SCRATCH_SPEED_X, (uint32_t)velocity_x_third);
    ae_wr32(r, AE_GADGET_SCRATCH_SPEED_Y,
            (uint32_t)ae_rds32(r, player + AE_PLAYER_VEL + AE_PLAYER_VEL_Y));
    ae_wr32(r, AE_GADGET_SCRATCH_SPEED_Z, (uint32_t)velocity_z_third);
    for (uint32_t byte = 0; byte < 8u; ++byte) {
        ae_wr8(r, AE_GADGET_SCRATCH_POS_SHORTS + byte,
               ae_rd8(r, player + AE_PLAYER_POS_SHORTS + byte));
    }
    ae_wr32(r, AE_GADGET_SCRATCH_ANGLE, (uint32_t)radius);

    GADGET_CALL(r, h, SdkVectorHelper_8002FB00, AE_G_CAMERA_LOOK_MATRIX, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    GADGET_CALL(r, h, SdkVectorHelper_8002FB30, AE_G_CAMERA_LOOK_MATRIX, 0, 0, 0);
    if (ae_ram_failed(r)) return;

    uint32_t record = AE_G_GADGET_TRAIL4;
    for (uint32_t index = 0; index < AE_GADGET4_TRAIL_COUNT; ++index, record += AE_GADGET4_TRAIL_STRIDE) {
        uint32_t state_field = record + AE_GADGET4_TRAIL_STATE;
        uint32_t trail_state = ae_rd16(r, state_field);
        uint32_t remaining = trail_state & AE_GADGET4_TRAIL_STATE_MASK;

        if (remaining == 0) {
            if (emit_trails != 0) continue;

            uint32_t seed = gadget_trail_lcg(ae_rd32(r, AE_GADGET_SCRATCH_SEED));
            ae_wr32(r, AE_GADGET_SCRATCH_SEED, seed);
            trail_state = (trail_state & ~AE_GADGET4_TRAIL_STATE_MASK) + ((seed & 7u) + 3u);
            ae_wr16(r, state_field, trail_state);

            seed = gadget_trail_lcg(seed);
            ae_wr32(r, AE_GADGET_SCRATCH_SEED, seed);
            uint32_t angle = seed & 0x0FFFu;
            int32_t x_offset = gadget_trail_offset(r, angle,
                                                   (int32_t)ae_rd32(r, AE_GADGET_SCRATCH_ANGLE), 1);
            int32_t z_offset = gadget_trail_offset(r, angle,
                                                   (int32_t)ae_rd32(r, AE_GADGET_SCRATCH_ANGLE), 0);
            ae_wr16(r, record + AE_GADGET4_TRAIL_X,
                    (uint32_t)((int32_t)ae_rds16(r, player + AE_PLAYER_POS) + x_offset));
            ae_wr16(r, record + AE_GADGET4_TRAIL_Y,
                    ae_rd16(r, AE_GADGET_SCRATCH_POS_SHORTS + 2u));
            ae_wr16(r, record + AE_GADGET4_TRAIL_Z,
                    (uint32_t)((int32_t)ae_rds16(r, player + AE_PLAYER_POS + 8u) + z_offset));

            trail_state = ae_rd16(r, state_field);
            seed = gadget_trail_lcg(seed);
            ae_wr32(r, AE_GADGET_SCRATCH_SEED, seed);
            uint32_t motion_bits = ((seed & 1u) + 1u) << 5;
            trail_state = (trail_state & 0xFF1Fu) | motion_bits;
            ae_wr16(r, state_field, trail_state);
        } else {
            uint32_t next_remaining = (remaining - 1u) & AE_GADGET4_TRAIL_STATE_MASK;
            trail_state = (trail_state & ~AE_GADGET4_TRAIL_STATE_MASK) | next_remaining;
            ae_wr16(r, state_field, trail_state);
            if (next_remaining < 3u) {
                trail_state = (trail_state & 0xFF1Fu) | 0x60u;
                ae_wr16(r, state_field, trail_state);
            }

            ae_wr16(r, record + AE_GADGET4_TRAIL_X,
                    ae_rd16(r, record + AE_GADGET4_TRAIL_X) +
                    ae_rd16(r, AE_GADGET_SCRATCH_SPEED_X));
            ae_wr16(r, record + AE_GADGET4_TRAIL_Y,
                    ae_rd16(r, AE_GADGET_SCRATCH_POS_SHORTS + 2u));
            ae_wr16(r, record + AE_GADGET4_TRAIL_Z,
                    ae_rd16(r, record + AE_GADGET4_TRAIL_Z) +
                    ae_rd16(r, AE_GADGET_SCRATCH_SPEED_Z));
        }

        GADGET_CALL(r, h, GpuDrawGadget4Trail, record, 0, 0, 0);
        if (ae_ram_failed(r)) return;
    }
}

/* 8007A48C: keep three distinct slingback targets and refresh their eight-frame lock timer. */
void ae_Gadget3LockOnAdd(AeRam *r, const AeGadgetHooks *h, uint32_t target) {
    for (int32_t i = (int32_t)AE_GADGET3_LOCK_COUNT - 1; i >= 0; --i) {
        uint32_t record = AE_G_GADGET3_LOCK_LIST + (uint32_t)i * AE_GADGET3_LOCK_STRIDE;
        if (ae_rd32(r, record + AE_GADGET3_LOCK_TARGET) == target) return;
    }

    for (uint32_t i = 0; i < AE_GADGET3_LOCK_COUNT; ++i) {
        uint32_t record = AE_G_GADGET3_LOCK_LIST + i * AE_GADGET3_LOCK_STRIDE;
        if (ae_rd32(r, record + AE_GADGET3_LOCK_TARGET) == 0) {
            ae_wr32(r, record + AE_GADGET3_LOCK_TARGET, target);
            ae_wr16(r, record + AE_GADGET3_LOCK_TIMER, 8);
            GADGET_CALL(r, h, PlaySfx, 24u, i + 1u, 0, 0);
            return;
        }
    }
}

/* 8007A32C: refresh the slingback lock candidate and draw one reticle per active lock. */
void ae_Gadget3FindAimTarget(AeRam *r, const AeGadgetHooks *h) {
    uint32_t direction = AE_G_GADGET_TEMP_VECTOR;

    if (ae_rd8(r, AE_G_GADGET3_AIM_MODE) == 1u) {
        GADGET_CALL(r, h, CameraGetSlotTarget, direction, 0, 0, 0);
        if (ae_ram_failed(r)) return;

        for (uint32_t axis = 0; axis < 3; ++axis) {
            uint32_t offset = axis * 2u;
            int32_t camera = ae_rds16(r, direction + offset);
            int32_t player = ae_rds16(r, AE_G_PLAYER + AE_PLAYER_POS + offset);
            ae_wr16(r, direction + offset, (uint32_t)(camera - player));
        }
        ae_sdk_VectorNormalSS(r, direction, direction);
    } else {
        uint32_t facing = AE_G_PLAYER + AE_PLAYER_FACING;
        ae_wr32(r, direction, ae_rd32(r, facing));
        ae_wr32(r, direction + 4u, ae_rd32(r, facing + 4u));
    }

    uint32_t candidate = (uint32_t)GADGET_CALL(
        r, h, TargetListFindBest, AE_G_GADGET3_AIM_ORIGIN, direction, 400, 0);
    if (ae_ram_failed(r)) return;
    if (candidate != 0) ae_Gadget3LockOnAdd(r, h, candidate);
    if (ae_ram_failed(r)) return;

    GADGET_CALL(r, h, SdkVectorHelper_8002FB00, AE_G_CAMERA_LOOK_MATRIX, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    GADGET_CALL(r, h, SdkVectorHelper_8002FB30, AE_G_CAMERA_LOOK_MATRIX, 0, 0, 0);
    if (ae_ram_failed(r)) return;

    for (uint32_t i = 0; i < AE_GADGET3_LOCK_COUNT; ++i) {
        uint32_t record = AE_G_GADGET3_LOCK_LIST + i * AE_GADGET3_LOCK_STRIDE;
        if (ae_rd32(r, record + AE_GADGET3_LOCK_TARGET) == 0) continue;

        int32_t timer = ae_rds16(r, record + AE_GADGET3_LOCK_TIMER);
        if (timer > 0) {
            --timer;
            ae_wr16(r, record + AE_GADGET3_LOCK_TIMER, (uint32_t)timer);
        }

        uint32_t vector_x = AE_G_GADGET_TEMP_VECTOR + 8u;
        uint32_t vector_yz = AE_G_GADGET_TEMP_VECTOR + 12u;
        GADGET_CALL(r, h, SdkVectorHelper_80031330,
                    ae_rd32(r, record + AE_GADGET3_LOCK_TARGET), vector_x, vector_yz, vector_yz);
        if (ae_ram_failed(r)) return;

        uint32_t packed = ae_rd32(r, vector_x);
        GADGET_CALL(r, h, AimReticleDrawCircle,
                    packed & 0xFFFFu, (uint32_t)(int32_t)(int16_t)(packed >> 16),
                    (uint32_t)ae_rds16(r, record + AE_GADGET3_LOCK_TIMER), 0);
        if (ae_ram_failed(r)) return;
    }
}

static void gadget4_write_hit_body(AeRam *r, const AeBodyHooks *body_hooks,
                                   uint32_t owner, uint32_t work, int32_t type,
                                   int32_t state, int32_t position_mode) {
    uint32_t body = ae_CollisionBodyAlloc(r);
    if (body == 0) return;

    ae_wr32(r, body + AE_COLLISION_BODY_FACING,
            ae_rd32(r, owner + AE_PLAYER_FACING));
    ae_wr32(r, body + AE_COLLISION_BODY_FACING_Z,
            ae_rd32(r, owner + AE_PLAYER_FACING + AE_PLAYER_FACING_Z));
    int32_t x = ae_rds16(r, work + AE_G_GADGET4_WORK_HIT_X);
    int32_t y = ae_rds16(r, work + AE_G_GADGET4_WORK_HIT_Y);
    int32_t z = ae_rds16(r, work + AE_G_GADGET4_WORK_HIT_Z);
    if (position_mode != 0) {
        x = gadget_sra(ae_rds32(r, owner + AE_PLAYER_POS + AE_GADGET_POS_X) + x, 1);
        if (state == 129) y -= 64;
        z = gadget_sra(ae_rds32(r, owner + AE_PLAYER_POS + AE_GADGET_POS_Z) + z, 1);
    } else if (state == 129) {
        y -= 64;
    }
    ae_wr32(r, body + AE_COLLISION_BODY_POS_X, (uint32_t)x);
    ae_wr32(r, body + AE_COLLISION_BODY_POS_Y, (uint32_t)y);
    ae_wr32(r, body + AE_COLLISION_BODY_POS_Z, (uint32_t)z);
    ae_wr16(r, body + AE_G_GADGET4_BODY_RADIUS, 150);
    ae_wr16(r, body + AE_G_GADGET4_BODY_HEIGHT, 2102);
    ae_wr16(r, body + AE_G_GADGET4_BODY_FLAGS, 514);
    uint32_t pointer_offset = position_mode == 0 ? AE_G_GADGET4_BODY_LEFT : AE_G_GADGET4_BODY_RIGHT;
    ae_wr32(r, work + pointer_offset, body);
    (void)ae_CollisionBodyRegister(r, body_hooks, type, body, owner);
}

/* 8007A514: update Super Hoop animation state, hit volumes, and rumble responses. */
void ae_Gadget4PlayerUpdate(AeRam *r, const AeGadgetHooks *h) {
    uint32_t work = AE_G_GADGET_WORK;
    uint32_t player = AE_G_PLAYER;
    uint32_t level = AE_G_GADGET4_LEVEL_DATA;
    uint32_t player_state = ae_rd16(r, player + AE_PLAYER_STATE);
    uint32_t mode = ae_rd16(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH);
    uint32_t level_frame = ae_rd16(r, level + AE_G_GADGET4_LEVEL_FRAME);
    uint32_t rumble_sfx = 0;

    ae_wr32(r, AE_G_GADGET4_SCRATCH_SAVED_STATE, ae_rd32(r, AE_G_GADGET4_SAVED_STATE));

    if (mode == 2u) {
        ae_wr16(r, work + AE_G_GADGET4_WORK_TARGET_SCALE, 1024);
        if (ae_rd16(r, AE_G_GADGET4_GESTURE_STATE) == 0) {
            ae_wr16(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH, 0);
        }
        if (player_state == 130u) {
            GADGET_CALL(r, h, PlayerSetState, 1, 0, 0, 0);
            if (ae_ram_failed(r)) return;
        }
    } else {
        uint32_t frame = ae_rd16(r, player + AE_PLAYER_ANIM_FRAME);
        switch (player_state) {
        case 128u:
            if (frame == 0) {
                if (level_frame < 121u) {
                    ae_wr16(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH, 0);
                    ae_wr16(r, work + AE_G_GADGET4_WORK_TARGET_SCALE, 1024);
                } else {
                    ae_wr16(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH, 1);
                    ae_wr16(r, work + AE_G_GADGET4_WORK_TARGET_SCALE, level_frame << 5);
                }
            } else if (frame == 13u && mode != 0) {
                ae_wr16(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH, 0);
                ae_wr16(r, work + AE_G_GADGET4_WORK_TARGET_SCALE, (level_frame << 3) + 1024u);
                if (ae_rd16(r, level + AE_G_GADGET4_LEVEL_ENABLE) != 0) {
                    ae_wr16(r, player + AE_PLAYER_ANIM_FRAME, frame - 1u);
                }
            }
            break;
        case 129u:
            if (frame == 19u) {
                ae_wr16(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH, 1);
                ae_wr16(r, work + AE_G_GADGET4_WORK_TARGET_SCALE, 4096);
            } else if ((int16_t)(frame - 19u) >= 14) {
                ae_wr16(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH, 0);
                ae_wr16(r, work + AE_G_GADGET4_WORK_TARGET_SCALE, 1024);
            }
            break;
        case 130u:
            ae_wr16(r, work + AE_G_GADGET4_WORK_TARGET_SCALE,
                    (ae_rd16(r, AE_G_GADGET4_GLOBAL_END_FRAME) << 3) + 1024u);
            ae_wr16(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH, 0);
            break;
        default:
            ae_wr16(r, work + AE_G_GADGET4_WORK_TARGET_SCALE, 1024);
            ae_wr16(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH, 0);
            ae_wr32(r, work + AE_G_GADGET4_BODY_LEFT, 0);
            break;
        }
    }

    for (uint32_t slot = 0; slot < 2; ++slot) {
        uint32_t gate_offset = AE_G_GADGET4_BODY_LEFT + slot * 4u;
        uint32_t body = ae_rd32(r, work + AE_G_GADGET4_BODY_LEFT);
        if (ae_rd32(r, work + gate_offset) != 0 && body != 0 &&
            (ae_rd32(r, body + AE_COLLISION_BODY_CONTACT) >> 28) == 10u) {
            ae_wr16(r, work + AE_G_GADGET4_WORK_TARGET_SCALE, 1024);
            ae_wr16(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH, 2);
            GADGET_CALL(r, h, PadRumble, 1, 200, 4, 0);
            if (ae_ram_failed(r)) return;
            ae_wr32(r, work + gate_offset, 0);
            rumble_sfx = 28;
        }
    }
    if (rumble_sfx != 0) {
        GADGET_CALL(r, h, PlaySfx, rumble_sfx, 0, 0, 0);
        if (ae_ram_failed(r)) return;
    }

    int32_t sum = ae_rds16(r, work + AE_G_GADGET4_WORK_SCALE) +
                  ae_rds16(r, work + AE_G_GADGET4_WORK_TARGET_SCALE);
    ae_wr16(r, work + AE_G_GADGET4_WORK_SCALE,
            (uint32_t)gadget_sra(sum + (sum < 0), 1));

    uint32_t dispatch_state = ae_rd16(r, player + AE_PLAYER_STATE);
    uint32_t level_index = dispatch_state - 14u;
    uint32_t destination = (level_index < 52u)
        ? ae_rd32(r, AE_G_GADGET4_AREA_DISPATCH + level_index * 4u) : 0;
    if (destination == 0x8007A804u) {
        ae_GadgetSetSelected(r, h, 0);
        return;
    }
    ae_GadgetSetSelected(r, h, 1);
    if (ae_ram_failed(r)) return;

    if ((ae_rd32(r, AE_G_GADGET4_AREA_MODE) & 0xF0u) == 0x80u) {
        uint32_t matrix = AE_G_GADGET4_SCRATCH_MATRIX;
        ae_sdk_MatrixIdentity(r, matrix);
        if (ae_ram_failed(r)) return;
        ae_wr32(r, matrix + AE_MATRIX_TRANSLATION_Y, 32);
        ae_wr32(r, matrix + AE_MATRIX_TRANSLATION_Z, (uint32_t)-48);
        ae_sdk_CompMatrix(r, AE_G_GADGET4_MATRIX_SOURCE, matrix, matrix);
        if (ae_ram_failed(r)) return;
        GADGET_CALL(r, h, DrawScaledModelAt, matrix,
                    (uint32_t)ae_rds16(r, work + AE_G_GADGET4_WORK_SCALE), 1, 0);
    } else {
        GADGET_CALL(r, h, DrawScaledModelAt, AE_G_GADGET4_MATRIX_SOURCE,
                    (uint32_t)ae_rds16(r, work + AE_G_GADGET4_WORK_SCALE), 1, 0);
    }
    if (ae_ram_failed(r)) return;

    mode = ae_rd16(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH);
    if (mode != 1u) {
        if (player_state != 130u || level_frame < 121u ||
            ae_rds16(r, level + AE_G_GADGET4_LEVEL_WINDOW) >= 3600) return;
    }

    AeBodyHooks body_hooks = gadget_body_hooks(h);
    gadget4_write_hit_body(r, &body_hooks, player, work, 13, (int32_t)player_state, 0);
    if (ae_ram_failed(r)) return;
    gadget4_write_hit_body(r, &body_hooks, player, work, 12, (int32_t)player_state, 1);
}

/* 8007AA1C: choose the swing impulse from the hard-flick event bit. */
void ae_GadgetSwingParamsInit(AeRam *r) {
    if (ae_rd16(r, AE_G_STICK_GESTURE_EVENTS) & 8u) {
        ae_wr16(r, AE_G_GADGET_SWING + AE_GADGET_SWING_ACCEL, 400);
        ae_wr16(r, AE_G_GADGET_SWING + AE_GADGET_SWING_SPEED, (uint32_t)-64);
    } else {
        ae_wr16(r, AE_G_GADGET_SWING + AE_GADGET_SWING_ACCEL, 800);
        ae_wr16(r, AE_G_GADGET_SWING + AE_GADGET_SWING_SPEED, (uint32_t)-128);
    }
    ae_wr16(r, AE_G_GADGET_SWING, 0);
}

/* 8007B358: OR the newly acquired gadget bits into the saved unlock mask. */
void ae_GadgetUnlockMask(AeRam *r, uint32_t mask) {
    ae_wr32(r, AE_G_GADGET_UNLOCK_MASK, ae_rd32(r, AE_G_GADGET_UNLOCK_MASK) | mask);
}

/* 8007C6CC: animate the gadget selector, query its ground contact, and register its sensor. */
void ae_GadgetSelectFxDraw(AeRam *r, const AeGadgetHooks *h) {
    uint32_t effect = AE_G_GADGET_SELECT_FX;
    uint32_t matrix_a = AE_G_GADGET_STACK_MATRIX_A;
    uint32_t matrix_b = AE_G_GADGET_STACK_MATRIX_B;
    uint32_t scale = AE_G_GADGET_STACK_SCALE;
    int32_t animation = ae_rds32(r, AE_G_GADGET_SELECT_FX_TIMER);
    int32_t turn = ae_rds16(r, effect + 6u) + 1024;
    ae_wr16(r, effect + AE_GADGET_SELECT_FX_TURN, (uint32_t)turn);
    for (uint32_t axis = 0; axis < 3; ++axis) ae_wr32(r, scale + axis * 4u, (uint32_t)turn);

    ae_sdk_MatrixIdentity(r, matrix_a);
    if (ae_ram_failed(r)) return;
    GADGET_CALL(r, h, SdkVectorHelper_80030A30, 1024, matrix_a, 0, 0);
    if (ae_ram_failed(r)) return;
    ae_sdk_MatrixLookDir(r, effect + AE_GADGET_SELECT_FX_DIR_X, matrix_b);
    if (ae_ram_failed(r)) return;
    ae_sdk_ScaleMatrix(r, matrix_b, scale);
    if (ae_ram_failed(r)) return;
    GADGET_CALL(r, h, SdkVectorHelper_80031830, matrix_b, matrix_a, matrix_b, 0);
    if (ae_ram_failed(r)) return;
    ae_sdk_CompMatrix(r, AE_G_CAMERA_LOOK_MATRIX, matrix_b, matrix_b);
    if (ae_ram_failed(r)) return;
    GADGET_CALL(r, h, ModelTransformRigid, AE_G_GADGET_SELECT_FX_MODEL,
                matrix_b, AE_G_GADGET_MODEL_ROOT_MATRIX, 0);
    if (ae_ram_failed(r)) return;

    int32_t alpha_q4 = 255;
    if (animation > 10) {
        alpha_q4 -= (animation - 10) * 51;
        if (alpha_q4 < 0) alpha_q4 = 0;
    }
    alpha_q4 <<= 4;
    GADGET_CALL(r, h, SetColorMatrixMode, 2, (uint32_t)alpha_q4,
                (uint32_t)alpha_q4, (uint32_t)alpha_q4);
    if (ae_ram_failed(r)) return;

    if (animation < 16 || ((uint32_t)animation & 1u) != 0) {
        GADGET_CALL(r, h, ModelDraw, ae_rd32(r, AE_G_GADGET_SELECT_FX_OT),
                    AE_G_GADGET_MODEL_ROOT_MATRIX, AE_G_GADGET_SELECT_FX_MODEL, 0);
        if (ae_ram_failed(r)) return;
    }
    GADGET_CALL(r, h, SetColorMatrixMode, 3, 0, 0, 0);
    if (ae_ram_failed(r)) return;

    if (animation < 16) {
        int32_t x = ae_rds16(r, effect + AE_GADGET_SELECT_FX_POS_X) +
                    gadget_sra((int32_t)(ae_rd16(r, effect + AE_GADGET_SELECT_FX_DIR_X) << 16), 22);
        int32_t y = ae_rds16(r, effect + AE_GADGET_SELECT_FX_POS_Y) +
                    gadget_sra((int32_t)(ae_rd16(r, effect + AE_GADGET_SELECT_FX_DIR_Y) << 16), 22);
        int32_t z = ae_rds16(r, effect + AE_GADGET_SELECT_FX_POS_Z) +
                    gadget_sra((int32_t)(ae_rd16(r, effect + AE_GADGET_SELECT_FX_DIR_Z) << 16), 22);
        ae_wr16(r, effect + AE_GADGET_SELECT_FX_POS_X, (uint32_t)x);
        ae_wr16(r, effect + AE_GADGET_SELECT_FX_POS_Y, (uint32_t)y);
        ae_wr16(r, effect + AE_GADGET_SELECT_FX_POS_Z, (uint32_t)z);

        uint32_t surface = (uint32_t)ae_GetSurfaceType(r, x, y + 32, z);
        if (ae_ram_failed(r)) return;
        if (((surface & 0xFFFFu) >> 12) == 1u) {
            AeWorldHooks world_hooks = gadget_world_hooks(h);
            int32_t ground = ae_GetGroundHeight(r, &world_hooks, x, y, z);
            if (ae_ram_failed(r)) return;
            if (ground < y) {
                animation = 16;
            }
        } else {
            animation = 16;
        }

        if (animation < 16 && ae_rd16(r, AE_G_GADGET_SELECT_FX_AREA_STATE) != 11u &&
            ae_rds16(r, AE_G_PLAYER + AE_PLAYER_HEALTH) > 0) {
            AeBodyHooks body_hooks = gadget_body_hooks(h);
            uint32_t body = ae_CollisionBodyAlloc(r);
            if (ae_ram_failed(r)) return;
            if (body != 0) {
                for (uint32_t byte = 0; byte < 8u; ++byte) {
                    ae_wr8(r, body + AE_COLLISION_BODY_FACING + byte,
                           ae_rd8(r, effect + AE_GADGET_SELECT_FX_DIR_X + byte));
                }
                ae_wr32(r, body + AE_COLLISION_BODY_POS_X,
                        (uint32_t)ae_rds16(r, effect + AE_GADGET_SELECT_FX_POS_X));
                ae_wr32(r, body + AE_COLLISION_BODY_POS_Y,
                        (uint32_t)ae_rds16(r, effect + AE_GADGET_SELECT_FX_POS_Y));
                ae_wr32(r, body + AE_COLLISION_BODY_POS_Z,
                        (uint32_t)ae_rds16(r, effect + AE_GADGET_SELECT_FX_POS_Z));
                ae_wr16(r, body + AE_G_GADGET4_BODY_RADIUS, (uint32_t)animation << 4);
                (void)ae_CollisionBodyRegister(r, &body_hooks, 8, body, AE_G_PLAYER);
                if (ae_ram_failed(r)) return;
                ae_wr16(r, body + AE_G_GADGET4_BODY_HEIGHT, 256);
                ae_wr16(r, body + AE_G_GADGET4_BODY_FLAGS, 0);
            }
        }
    }

    int32_t particle_animation = animation;
    if (animation < 21) {
        ++animation;
        particle_animation = animation;
    } else {
        animation = 0;
    }
    ae_wr32(r, AE_G_GADGET_SELECT_FX_TIMER, (uint32_t)animation);
    if (particle_animation < 16) {
        GADGET_CALL(r, h, SpawnParticle, 20, effect, 0, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL(r, h, SpawnParticle, 18, effect, 0, 0);
        if (ae_ram_failed(r)) return;
        GADGET_CALL(r, h, SpawnParticle, 21, effect, 0, 0);
    }
}

/* 8007CD0C: selecting a different gadget starts the selection effect before storing its id. */
void ae_GadgetSetSelected(AeRam *r, const AeGadgetHooks *h, int32_t gadget) {
    if (ae_rd8(r, AE_G_GADGET_SELECTED) != (uint8_t)gadget) {
        ae_GadgetSelectFx(r, h);
        if (ae_ram_failed(r)) return;
        ae_wr8(r, AE_G_GADGET_SELECTED, (uint32_t)gadget);
    }
}

/* 80081144: replace every slot entry equal to old_gadget, preserving the byte-width store. */
void ae_GadgetSlotRemap(AeRam *r, int32_t old_gadget, int32_t new_gadget) {
    for (uint32_t slot = 0; slot < 4; ++slot) {
        uint32_t field = AE_G_GADGET_SLOTS + slot;
        if (ae_rds8(r, field) == old_gadget) {
            ae_wr8(r, field, (uint32_t)new_gadget);
        }
    }
}

/* 80099C44: transform the selected gadget model, position its effect matrix, and draw it. */
void ae_GadgetDrawStaticModel(AeRam *r, const AeGadgetHooks *h) {
    int32_t gadget = ae_rds8(r, AE_G_PLAYER_CURRENT_GADGET);
    uint32_t instance = AE_G_PLAYER_STATIC_MODELS + (uint32_t)gadget * AE_STATIC_MODEL_SIZE;
    uint32_t model = ae_rd32(r, instance + AE_STATIC_MODEL_MODEL);
    uint32_t bone_index = ae_rd16(r, model + AE_STATIC_MODEL_BONES);
    uint32_t matrix_offset = bone_index * AE_G_GADGET_MODEL_MATRIX_STRIDE;
    uint32_t bone_matrix = AE_G_GADGET_MODEL_MATRIX_BASE + matrix_offset;

    GADGET_CALL(r, h, ModelTransformRigid, instance, bone_matrix,
                AE_G_GADGET_MODEL_ROOT_MATRIX, 0);
    if (ae_ram_failed(r)) return;
    GADGET_CALL(r, h, PlayerFxSetMatrixPos,
                AE_G_GADGET_MODEL_POSITION_BASE + matrix_offset, bone_matrix, 0, 0);
    if (ae_ram_failed(r)) return;

    uint32_t work = ae_rd32(r, AE_G_GADGET_MODEL_DRAW_INDEX);
    uint32_t draw_index = (((work << 7) + work) << 4) - work;
    uint32_t draw_matrix = AE_G_GADGET_MODEL_DRAW_BASE + (draw_index << 3);
    GADGET_CALL(r, h, ModelDraw, draw_matrix,
                AE_G_GADGET_MODEL_ROOT_MATRIX, instance, 0);
}

/* 8009A4E8: suppress the held model while climbing or hanging; otherwise draw the radar model. */
void ae_GadgetDrawWithStateCheck(AeRam *r, const AeGadgetHooks *h,
                                 uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t state = ae_rd16(r, AE_G_PLAYER + AE_PLAYER_STATE);
    if (state >= 56u && state < 61u) {
        ae_GadgetSetSelected(r, h, 0);
        return;
    }

    ae_GadgetSetSelected(r, h, 1);
    if (ae_ram_failed(r)) return;
    GADGET_CALL(r, h, RadarDrawModel, a0, a1, a2, a3);
}

/* 8009B1C0: manage the Sky Flyer loop voices, draw its model and emit flight sparkles. */
void ae_Gadget6UpdateSound(AeRam *r, const AeGadgetHooks *h) {
    uint32_t sound_state = ae_rd32(r, AE_G_GADGET6_SOUND_STATE);
    uint32_t player_state = ae_rd16(r, AE_G_PLAYER + AE_PLAYER_STATE);

    if (sound_state == 0) {
        if (player_state >= 128u && player_state < 131u) {
            int32_t voice = GADGET_CALL(r, h, SoundLoopStart, 0, 29, 0, 0);
            if (ae_ram_failed(r)) return;
            ae_wr16(r, AE_G_GADGET6_LOOP_VOICE_1, (uint32_t)voice);
            if ((int16_t)voice != -1) ae_wr32(r, AE_G_GADGET6_SOUND_STATE, 1);
        } else if (player_state >= 131u && player_state < 133u) {
            int32_t voice = GADGET_CALL(r, h, SoundLoopStart, 0, 30, 0, 0);
            if (ae_ram_failed(r)) return;
            ae_wr16(r, AE_G_GADGET6_LOOP_VOICE_2, (uint32_t)voice);
            if ((int16_t)voice != -1) ae_wr32(r, AE_G_GADGET6_SOUND_STATE, 2);
        }
    } else if (sound_state == 1) {
        if (player_state < 128u || player_state >= 131u) {
            int32_t voice = ae_rds16(r, AE_G_GADGET6_LOOP_VOICE_1);
            if (voice != -1) {
                GADGET_CALL(r, h, SoundLoopStop, 29, (uint32_t)voice, 0, 0);
                if (ae_ram_failed(r)) return;
            }
            ae_wr32(r, AE_G_GADGET6_SOUND_STATE, 0);
        }
    } else if (sound_state == 2) {
        if (player_state < 131u || player_state >= 133u) {
            int32_t voice = ae_rds16(r, AE_G_GADGET6_LOOP_VOICE_2);
            if (voice != -1) {
                GADGET_CALL(r, h, SoundLoopStop, 30, (uint32_t)voice, 0, 0);
                if (ae_ram_failed(r)) return;
            }
            ae_wr32(r, AE_G_GADGET6_SOUND_STATE, 0);
        }
    }

    player_state = ae_rd16(r, AE_G_PLAYER + AE_PLAYER_STATE);
    uint32_t model = (player_state >= 131u && player_state < 133u)
        ? AE_G_GADGET6_MODEL_BASE_LATE : AE_G_GADGET6_MODEL_BASE_EARLY;
    GADGET_CALL(r, h, ModelTransformRigid, model,
                AE_G_GADGET6_MODEL_BONE_MATRIX, AE_G_GADGET_MODEL_ROOT_MATRIX, 0);
    if (ae_ram_failed(r)) return;

    uint32_t work = ae_rd32(r, AE_G_GADGET_MODEL_DRAW_INDEX);
    uint32_t draw_index = (((work << 7) + work) << 4) - work;
    GADGET_CALL(r, h, ModelDraw,
                AE_G_GADGET_MODEL_DRAW_BASE + (draw_index << 3),
                AE_G_GADGET_MODEL_ROOT_MATRIX, model, 0);
    if (ae_ram_failed(r)) return;

    if (player_state == 131u || player_state == 132u) {
        ae_Gadget6SparkleFx(r, h);
    }
}

/* 80098DD0: build the Super Hoop draw matrix for its normal and spin modes. */
void ae_Gadget4BuildMatrix(AeRam *r, const AeGadgetHooks *h) {
    int32_t mode = ae_rds16(r, AE_G_GADGET4_MODE);
    uint32_t model_instance = AE_G_GADGET4_MODEL_BASE;
    uint32_t matrix = AE_G_GADGET4_MATRIX_TEMP;

    if (mode == 0) {
        ae_sdk_MatrixIdentity(r, matrix);
        uint32_t model = ae_rd32(r, model_instance);
        uint32_t bone_index = ae_rd16(r, model + AE_STATIC_MODEL_BONES);
        ae_wr32(r, matrix + AE_MATRIX_TRANSLATION_X,
                (uint32_t)ae_rds16(r, AE_G_GADGET_WORK + AE_GADGET4_ORIGIN_X));
        ae_wr32(r, matrix + AE_MATRIX_TRANSLATION_Y, 0);
        ae_wr32(r, matrix + AE_MATRIX_TRANSLATION_Z,
                (uint32_t)ae_rds16(r, AE_G_GADGET_WORK + AE_GADGET4_ORIGIN_Z));
        ae_sdk_CompMatrix(r, matrix, AE_G_GADGET4_HOOP_MATRIX_BASE +
                          bone_index * AE_G_GADGET_MODEL_MATRIX_STRIDE, matrix);
        ae_sdk_CompMatrix(r, AE_G_CAMERA_LOOK_MATRIX, matrix, matrix);
    } else if (mode == 1) {
        model_instance = AE_G_GADGET4_ALT_MODEL_BASE;
        uint32_t draw_state = ae_rd32(r, AE_G_GADGET4_DRAW_PHASE);
        int32_t angle = (int32_t)(draw_state * 250u);
        uint32_t model = ae_rd32(r, AE_G_GADGET4_MODEL_BASE);
        uint32_t bone_index = ae_rd16(r, model + AE_STATIC_MODEL_BONES);
        uint32_t source_matrix = AE_G_GADGET_MODEL_MATRIX_BASE +
                                 bone_index * AE_G_GADGET_MODEL_MATRIX_STRIDE;
        for (uint32_t offset = 0; offset < 32u; offset += 4u) {
            ae_wr32(r, matrix + offset, ae_rd32(r, source_matrix + offset));
        }
        GADGET_CALL(r, h, MatrixRotYMul, (uint32_t)angle, matrix, 0, 0);
        if (ae_ram_failed(r)) return;

        uint32_t scalar = (ae_rd32(r, AE_G_GADGET_WORK + 8u) << 2) + 4096u;
        ae_wr32(r, AE_G_GADGET4_VECTOR_TEMP, scalar);
        ae_wr32(r, AE_G_GADGET4_VECTOR_TEMP + 4u, scalar);
        ae_wr32(r, AE_G_GADGET4_VECTOR_TEMP + 8u, scalar);
        GADGET_CALL(r, h, SdkVectorHelper_800305B0, matrix, AE_G_GADGET4_VECTOR_TEMP, 0, 0);
        if (ae_ram_failed(r)) return;
    } else {
        if (!h) {
            ae_ram_fail(r, "Gadget4BuildMatrix stale $s1 value not provided");
            return;
        }
        model_instance = h->stale_s1;
    }

    GADGET_CALL(r, h, ModelTransformRigid, model_instance, matrix,
                AE_G_GADGET_MODEL_ROOT_MATRIX, 0);
    if (ae_ram_failed(r)) return;

    uint32_t work = ae_rd32(r, AE_G_GADGET_MODEL_DRAW_INDEX);
    uint32_t draw_index = (((work << 7) + work) << 4) - work;
    GADGET_CALL(r, h, ModelDraw,
                AE_G_GADGET_MODEL_DRAW_BASE + (draw_index << 3),
                AE_G_GADGET_MODEL_ROOT_MATRIX, model_instance, 0);
}

static int gadget4_apply_contact(AeRam *r, uint32_t player, uint32_t body_field) {
    uint32_t body = ae_rd32(r, body_field);
    if (body == 0 || (ae_rd32(r, body + AE_COLLISION_BODY_CONTACT) >> 28) == 0) return 0;

    uint32_t body_dx = (uint32_t)(int32_t)ae_rds16(r, body + AE_COLLISION_BODY_FACING_X) << 5;
    uint32_t body_dz = (uint32_t)(int32_t)ae_rds16(r, body + AE_COLLISION_BODY_FACING_Z) << 5;
    ae_wr32(r, player + AE_PLAYER_MOVE_VEL, ae_rd32(r, player + AE_PLAYER_MOVE_VEL) - body_dx);
    ae_wr32(r, AE_G_GADGET_WORK + AE_G_GADGET4_WORK_MODE_STRENGTH,
            ae_rd32(r, AE_G_GADGET_WORK + AE_G_GADGET4_WORK_MODE_STRENGTH) - 60u);
    ae_wr32(r, player + AE_PLAYER_MOVE_VEL + AE_PLAYER_VEL_Z,
            ae_rd32(r, player + AE_PLAYER_MOVE_VEL + AE_PLAYER_VEL_Z) - body_dz);
    return 1;
}

static void gadget4_register_ring_body(AeRam *r, const AeBodyHooks *body_hooks,
                                       uint32_t player, uint32_t work,
                                       uint32_t angle,
                                       uint32_t pointer_field) {
    uint32_t body = ae_CollisionBodyAlloc(r);
    ae_wr32(r, pointer_field, body);
    if (body == 0 || ae_ram_failed(r)) return;

    int32_t sine = ae_sdk_rsin(r, gadget_s32(angle));
    int32_t cosine = ae_sdk_rcos(r, gadget_s32(angle));
    int32_t radius_value = ae_rds32(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH);
    int32_t x_offset = gadget_sra((int32_t)((uint32_t)cosine * (uint32_t)radius_value), 14);
    int32_t z_offset = gadget_sra((int32_t)((uint32_t)sine * (uint32_t)radius_value), 14);

    ae_wr16(r, body + AE_COLLISION_BODY_FACING_X, (uint32_t)cosine);
    ae_wr16(r, body + AE_COLLISION_BODY_FACING_Y, 0);
    ae_wr16(r, body + AE_COLLISION_BODY_FACING_Z, (uint32_t)sine);
    ae_wr32(r, body + AE_COLLISION_BODY_POS_X,
            (uint32_t)((int64_t)ae_rds32(r, player + AE_PLAYER_POS) + x_offset));
    ae_wr32(r, body + AE_COLLISION_BODY_POS_Y,
            (uint32_t)(ae_rds32(r, player + AE_PLAYER_POS + AE_GADGET_POS_Y) - 128));
    ae_wr32(r, body + AE_COLLISION_BODY_POS_Z,
            (uint32_t)((int64_t)ae_rds32(r, player + AE_PLAYER_POS + AE_GADGET_POS_Z) + z_offset));
    ae_wr16(r, body + AE_G_GADGET4_BODY_RADIUS, 128);
    ae_wr16(r, body + AE_G_GADGET4_BODY_HEIGHT, 18);
    ae_wr16(r, body + AE_G_GADGET4_BODY_FLAGS, 513);
    (void)ae_CollisionBodyRegister(r, body_hooks, 6, body, player);
}

/* 800986E0: animate Super Hoop, respond to its three contact volumes, and draw its mode. */
void ae_Gadget4UpdateDraw(AeRam *r, const AeGadgetHooks *h) {
    uint32_t work = AE_G_GADGET_WORK;
    uint32_t player = AE_G_PLAYER;
    uint32_t level = AE_G_GADGET4_LEVEL_DATA;

    if ((ae_rd32(r, AE_G_GADGET_STAGE_FLAGS) & 1u) != 0) {
        ae_Gadget4BuildMatrix(r, h);
        return;
    }

    if (ae_rd16(r, AE_G_GADGET_SELECT_FX_AREA_STATE) == 11u ||
        ae_rds16(r, player + AE_PLAYER_HEALTH) == 0) {
        ae_wr16(r, AE_G_GADGET4_SCRATCH_ZOOM, 0);
        ae_wr16(r, AE_G_GADGET4_SCRATCH_RATE_X, 0);
        ae_wr16(r, AE_G_GADGET4_SCRATCH_RATE_Z, 0);
    } else {
        ae_wr16(r, AE_G_GADGET4_SCRATCH_ZOOM,
                ae_rd16(r, level + AE_G_GADGET4_LEVEL_FRAME));
        ae_wr16(r, AE_G_GADGET4_SCRATCH_RATE_X,
                ae_rd16(r, level + AE_G_GADGET4_LEVEL_TURN_RATE_X));
        ae_wr16(r, AE_G_GADGET4_SCRATCH_RATE_Z,
                ae_rd16(r, level + AE_G_GADGET4_LEVEL_TURN_RATE_Z));
    }

    uint32_t mode = ae_rd16(r, work + AE_G_GADGET4_WORK_TARGET_SCALE);
    if (mode != 1u) {
        for (uint32_t voice_index = 0; voice_index < 2; ++voice_index) {
            uint32_t voice_offset = AE_G_GADGET4_VOICE_LEFT + voice_index * 2u;
            int32_t voice = ae_rds16(r, work + voice_offset);
            if (voice != -1) {
                GADGET_CALL(r, h, SoundLoopStop, 44u + voice_index, (uint32_t)voice, 0, 0);
                if (ae_ram_failed(r)) return;
                ae_wr16(r, work + voice_offset, (uint32_t)-1);
            }
        }

        ae_sdk_MatrixIdentity(r, AE_G_GADGET4_SCRATCH_MATRIX);
        if (ae_ram_failed(r)) return;
        ae_wr16(r, work + AE_G_GADGET4_WORK_SCALE, 0);
        int can_activate = GADGET_CALL(r, h, PlayerCanActivateGadget, 4, 0, 0, 0);
        if (ae_ram_failed(r)) return;
        if (can_activate == 0) {
            ae_wr16(r, work + AE_G_GADGET4_WORK_SPEED_X, 0);
            ae_wr16(r, work + AE_G_GADGET4_WORK_SPEED_Z, 0);
        } else {
            int32_t scale = ae_rd16(r, AE_G_GADGET4_SCRATCH_ZOOM);
            int32_t level_speed_x = ae_rds16(r, level + AE_G_GADGET4_LEVEL_SPEED_X);
            int32_t level_speed_z = ae_rds16(r, level + AE_G_GADGET4_LEVEL_SPEED_Z);
            int32_t product_x = (int32_t)((uint32_t)level_speed_x * (uint32_t)scale);
            int32_t product_z = (int32_t)((uint32_t)level_speed_z * (uint32_t)scale);
            int32_t speed_x = gadget_sra(product_x, 13);
            int32_t speed_z = gadget_sra(product_z, 13);
            ae_wr16(r, work + AE_G_GADGET4_WORK_SPEED_X, speed_x);
            ae_wr32(r, AE_G_GADGET4_SCRATCH_SPEED_X, (uint32_t)(int16_t)speed_x);
            ae_wr16(r, work + AE_G_GADGET4_WORK_SPEED_Z, speed_z);
            ae_wr32(r, AE_G_GADGET4_SCRATCH_SPEED_Z, (uint32_t)(int16_t)speed_z);

            int32_t speed = gadget_s32(ae_rd32(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH));
            int32_t turn_rate = ae_rds16(r, AE_G_GADGET4_SCRATCH_RATE_X);
            int32_t turn_magnitude = turn_rate < 0 ? -turn_rate : turn_rate;
            if (speed >= 321) {
                ae_wr16(r, work + AE_G_GADGET4_WORK_TARGET_SCALE, 1);
                ae_wr16(r, work + AE_G_GADGET4_WORK_SCALE, 1);
                if (ae_rds16(r, work + AE_G_GADGET4_VOICE_LEFT) == -1) {
                    int32_t voice = GADGET_CALL(r, h, SoundLoopStart, 0, 44, 0, 0);
                    if (ae_ram_failed(r)) return;
                    ae_wr16(r, work + AE_G_GADGET4_VOICE_LEFT, (uint32_t)voice);
                }
            }
            speed = gadget_s32((uint32_t)speed + (uint32_t)turn_magnitude - 8u);
            if (speed < 0) speed = 0;
            ae_wr32(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH, (uint32_t)speed);
        }

        uint32_t model = ae_rd32(r, AE_G_GADGET4_MODEL_BASE);
        uint32_t bone = ae_rd16(r, model + AE_STATIC_MODEL_BONES);
        ae_sdk_CompMatrix(r, AE_G_GADGET4_SCRATCH_MATRIX,
                          AE_G_GADGET4_HOOP_MATRIX_BASE + bone * AE_G_GADGET_MODEL_MATRIX_STRIDE,
                          AE_G_GADGET4_SCRATCH_MATRIX);
        if (ae_ram_failed(r)) return;
        ae_wr16(r, work + AE_G_GADGET4_WORK_CLEAR_FIELD, 0);

        uint32_t level_number = ae_rd16(r, player + AE_PLAYER_STATE);
        if (level_number >= 49u && level_number < 52u) {
            ae_GadgetSetSelected(r, h, 0);
            return;
        }
        ae_GadgetSetSelected(r, h, 1);
        if (ae_ram_failed(r)) return;
        ae_Gadget4BuildMatrix(r, h);
        return;
    }

    if (ae_rd16(r, player + AE_PLAYER_STATE) == 141u &&
        ae_rds16(r, work + AE_G_GADGET4_VOICE_RIGHT) == -1) {
        int32_t voice = GADGET_CALL(r, h, SoundLoopStart, 0, 45, 0, 0);
        if (ae_ram_failed(r)) return;
        ae_wr16(r, work + AE_G_GADGET4_VOICE_RIGHT, (uint32_t)voice);
    }

    uint32_t particle_position = AE_G_GADGET_TEMP_VECTOR;
    for (uint32_t axis = 0; axis < 3; ++axis) {
        ae_wr16(r, particle_position + axis * 2u,
                ae_rd16(r, player + AE_PLAYER_POS + axis * 4u));
    }
    GADGET_CALL(r, h, SpawnParticle, 9, particle_position, 0, 0);
    if (ae_ram_failed(r)) return;

    int hit = 0;
    hit |= gadget4_apply_contact(r, player, work + AE_G_GADGET4_TRAIL_BODY_A);
    hit |= gadget4_apply_contact(r, player, work + AE_G_GADGET4_TRAIL_BODY_B);
    hit |= gadget4_apply_contact(r, player, work + AE_G_GADGET4_TRAIL_BODY_C);
    if (hit) {
        ae_wr32(r, player + AE_PLAYER_MOVE_VEL_Y,
                ae_rd32(r, player + AE_PLAYER_MOVE_VEL_Y) - 0x8000u);
        GADGET_CALL(r, h, PlayerSetState, 36, 0, 0, 0);
        if (ae_ram_failed(r)) return;
        ae_wr16(r, player + AE_PLAYER_ANIM_BLEND, 0);
    }

    uint32_t level_number = ae_rd16(r, player + AE_PLAYER_STATE);
    if (level_number >= 49u && level_number < 52u) {
        ae_GadgetSetSelected(r, h, 0);
        if (ae_ram_failed(r)) return;
        ae_wr32(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH, 0);
    } else {
        ae_GadgetSetSelected(r, h, 1);
        if (ae_ram_failed(r)) return;
        ae_Gadget4BuildMatrix(r, h);
        if (ae_ram_failed(r)) return;
    }

    int32_t strength = gadget_s32(ae_rd32(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH)) - 3;
    if (strength < 0) {
        ae_wr16(r, work + AE_G_GADGET4_WORK_TARGET_SCALE, 0);
    } else {
        int32_t voice = ae_rds16(r, work + AE_G_GADGET4_VOICE_LEFT);
        if (voice != -1) {
            GADGET_CALL(r, h, SoundEmitterVolume, 44, (uint32_t)voice,
                        (uint32_t)(strength * 8 + 512), 4096);
            if (ae_ram_failed(r)) return;
        }
        int32_t scaled_strength = gadget_s32((uint32_t)strength << 8);
        int64_t reciprocal_product = (int64_t)scaled_strength * 0x66666667;
        int32_t reciprocal_high = (int32_t)(reciprocal_product >> 32);
        int32_t rumble = gadget_sra(reciprocal_high, 7) - (scaled_strength < 0 ? -1 : 0);
        if (rumble >= 121) rumble = 120;
        GADGET_CALL(r, h, PadRumble, 1, (uint32_t)rumble, 2, 1);
        if (ae_ram_failed(r)) return;
    }
    ae_wr32(r, work + AE_G_GADGET4_WORK_MODE_STRENGTH, (uint32_t)strength);

    AeBodyHooks body_hooks = gadget_body_hooks(h);
    uint32_t first_angle = ae_rd32(r, AE_G_GADGET4_DRAW_PHASE) << 8;
    uint32_t body_fields[3] = {
        work + AE_G_GADGET4_TRAIL_BODY_A,
        work + AE_G_GADGET4_TRAIL_BODY_B,
        work + AE_G_GADGET4_TRAIL_BODY_C,
    };
    for (uint32_t index = 0; index < 3; ++index) {
        uint32_t angle = first_angle + index * 1300u;
        gadget4_register_ring_body(r, &body_hooks, player, work, angle,
                                   body_fields[index]);
        if (ae_ram_failed(r)) return;
    }
}

/* 8007AA98: consume the selected slingback ammo and spawn its guided or straight shot. */
void ae_Gadget3AimUpdateAmmo(AeRam *r, const AeGadgetHooks *h) {
    uint32_t shot_type = 1;
    uint32_t ammo_kind = ae_rd8(r, AE_G_GADGET3_AMMO_KIND);

    if (ammo_kind == 1u) {
        uint32_t ammo = ae_rd8(r, AE_G_GADGET3_AMMO_1);
        if (ammo != 0) {
            shot_type = 2;
            if (ae_rd16(r, AE_G_CURRENT_LEVEL) != 0x20u &&
                ae_rd16(r, AE_G_GAMEMODE) != 0x13u) {
                ae_wr8(r, AE_G_GADGET3_AMMO_1, ammo - 1u);
            }
        }
    } else if (ammo_kind == 2u) {
        uint32_t ammo = ae_rd8(r, AE_G_GADGET3_AMMO_2);
        if (ammo != 0) {
            shot_type = 3;
            if (ae_rd16(r, AE_G_CURRENT_LEVEL) != 0x20u &&
                ae_rd16(r, AE_G_GAMEMODE) != 0x13u) {
                ae_wr8(r, AE_G_GADGET3_AMMO_2, ammo - 1u);
            }
        }
    }

    uint32_t direction = AE_G_GADGET_TEMP_VECTOR;
    if (ae_rd8(r, AE_G_GADGET3_AIM_MODE) == 1u) {
        for (uint32_t axis = 0; axis < 3; ++axis) {
            uint32_t offset = axis * 2u;
            int32_t end = ae_rds16(r, AE_G_GADGET3_AIM_LINE_END + offset);
            int32_t start = ae_rds16(r, AE_G_GADGET3_AIM_LINE_START + offset);
            ae_wr16(r, direction + offset, (uint32_t)(end - start));
        }
        ae_sdk_VectorNormalSS(r, direction, direction);
        if (ae_ram_failed(r)) return;
    } else {
        uint32_t player_origin = AE_G_GADGET3_AIM_ORIGIN;
        uint32_t facing = AE_G_PLAYER + AE_PLAYER_FACING;
        uint32_t target = (uint32_t)GADGET_CALL(r, h, TargetListFindBest,
                                                player_origin, facing, 400, 0);
        if (ae_ram_failed(r)) return;
        if (target != 0) {
            for (uint32_t axis = 0; axis < 3; ++axis) {
                uint32_t offset = axis * 2u;
                int32_t point = ae_rds16(r, target + offset);
                int32_t player = ae_rds16(r, player_origin + offset);
                ae_wr16(r, direction + offset, (uint32_t)(point - player));
            }
            ae_sdk_VectorNormalSS(r, direction, direction);
            if (ae_ram_failed(r)) return;
            facing = direction;
        }
        direction = facing;
    }

    GADGET_CALL(r, h, ProjectileGroupSpawnSpread,
                AE_G_GADGET3_PROJECTILE_ORIGIN, direction, shot_type, 0);
}

/* 8009ABF4: draw both slot-0 attachments, then draw their ribbon between the bone matrices. */
void ae_GadgetDrawSlot0Attachment(AeRam *r, const AeGadgetHooks *h,
                                  uint32_t source_matrix, uint32_t output_matrix,
                                  int32_t angle) {
    uint32_t local = AE_G_GADGET_ATTACHMENT_LOCAL_MATRIX;
    ae_wr16(r, local, 4096);
    ae_wr16(r, local + 2u, 0);
    ae_wr16(r, local + 4u, 0);
    ae_wr16(r, local + 6u, 0);
    ae_wr16(r, local + 8u, (uint32_t)angle);
    ae_wr16(r, local + 10u, 0);
    ae_wr16(r, local + 12u, 0);
    ae_wr16(r, local + 14u, 0);
    ae_wr16(r, local + 16u, 4096);
    ae_wr16(r, local + 18u, 0);
    ae_wr32(r, local + 20u, 16);
    ae_wr32(r, local + 24u, 74);
    ae_wr32(r, local + 28u, (uint32_t)-8);

    uint32_t draw_index = ae_rd32(r, AE_G_GADGET_MODEL_DRAW_INDEX);
    uint32_t draw_matrix = AE_G_GADGET_MODEL_DRAW_BASE +
                           (((((draw_index << 7) + draw_index) << 4) - draw_index) << 3);
    GADGET_CALL(r, h, ModelTransformRigid, AE_G_GADGET_ATTACHMENT_MODEL_SLOT0,
                source_matrix, AE_G_GADGET_MODEL_ROOT_MATRIX, 0);
    if (ae_ram_failed(r)) return;
    GADGET_CALL(r, h, ModelDraw, draw_matrix,
                AE_G_GADGET_MODEL_ROOT_MATRIX, AE_G_GADGET_ATTACHMENT_MODEL_SLOT0, 0);
    if (ae_ram_failed(r)) return;

    if (output_matrix != 0) {
        ae_sdk_CompMatrix(r, output_matrix, local, AE_G_GADGET_ATTACHMENT_COMP_MATRIX);
        if (ae_ram_failed(r)) return;
    }
    ae_sdk_CompMatrix(r, source_matrix, local, local);
    if (ae_ram_failed(r)) return;

    GADGET_CALL(r, h, ModelTransformRigid, AE_G_GADGET_ATTACHMENT_MODEL_SLOT9,
                local, AE_G_GADGET_MODEL_ROOT_MATRIX, 0);
    if (ae_ram_failed(r)) return;
    GADGET_CALL(r, h, ModelDraw, draw_matrix,
                AE_G_GADGET_MODEL_ROOT_MATRIX, AE_G_GADGET_ATTACHMENT_MODEL_SLOT9, 0);
    if (ae_ram_failed(r)) return;

    uint32_t ribbon_angle = ae_rd32(r, AE_G_GADGET4_DRAW_PHASE) & 0x1Fu;
    if (ribbon_angle >= 17u) ribbon_angle = 32u - ribbon_angle;
    GADGET_CALL(r, h, DrawRibbonBetween,
                AE_G_GADGET_MODEL_ROOT_MATRIX + 64u,
                AE_G_GADGET_MODEL_ROOT_MATRIX + 72u,
                ribbon_angle + 24u, AE_G_GADGET_ATTACHMENT_RIBBON_COLOR);
}

/* 8009A800: update Super Hoop swing state, its looping voice, and its trail rings. */
void ae_GadgetSwingUpdate(AeRam *r, const AeGadgetHooks *h) {
    if (ae_rd32(r, AE_G_GADGET_STAGE_FLAGS) & 1u) {
        int32_t angle = ae_rds16(r, AE_G_GADGET_SWING) + 2048;
        ae_GadgetDrawSlot0Attachment(r, h, AE_G_GADGET_ATTACHMENT_START_MATRIX,
                                     AE_G_GADGET_ATTACHMENT_OUTPUT_MATRIX, angle);
        return;
    }

    uint32_t player_state = ae_rd16(r, AE_G_PLAYER + AE_PLAYER_STATE);
    if (player_state >= 138u && player_state < 140u &&
        ae_rds16(r, AE_G_GADGET_SWING) < 8192) {
        int32_t spin = ae_rds16(r, AE_G_STICK_SPIN);
        uint32_t magnitude = (uint32_t)spin;
        if (spin < 0) magnitude = 0u - magnitude;
        ae_wr16(r, AE_G_GADGET_SWING + AE_GADGET_SWING_SPEED, (uint32_t)-64);
        uint32_t acceleration = ae_rd16(r, AE_G_GADGET_SWING + AE_GADGET_SWING_ACCEL);
        ae_wr16(r, AE_G_GADGET_SWING + AE_GADGET_SWING_ACCEL, acceleration + (magnitude << 2));
    }

    uint32_t angle = ae_rd16(r, AE_G_GADGET_SWING);
    uint32_t acceleration = ae_rd16(r, AE_G_GADGET_SWING + AE_GADGET_SWING_ACCEL);
    uint32_t speed = ae_rd16(r, AE_G_GADGET_SWING + AE_GADGET_SWING_SPEED);
    uint32_t next_angle = angle + acceleration;
    ae_wr16(r, AE_G_GADGET_SWING, next_angle);
    ae_wr16(r, AE_G_GADGET_SWING + AE_GADGET_SWING_ACCEL, acceleration + speed);
    if ((int16_t)next_angle < 0) {
        ae_wr16(r, AE_G_GADGET_SWING, 0);
        ae_wr16(r, AE_G_GADGET_SWING + AE_GADGET_SWING_ACCEL, 0);
        ae_wr16(r, AE_G_GADGET_SWING + AE_GADGET_SWING_SPEED, 0);
    }

    int32_t trail_timer = ae_rds16(r, AE_G_GADGET_SWING_TIMER);
    if (trail_timer > 0) {
        int32_t next_timer = trail_timer < 6 ? trail_timer - 1 : 4;
        ae_wr16(r, AE_G_GADGET_SWING_TIMER, (uint32_t)next_timer);
    }

    int32_t draw_angle = ae_rds16(r, AE_G_GADGET_SWING);
    if (draw_angle >= 2049) draw_angle = 2048;
    ae_GadgetDrawSlot0Attachment(r, h, AE_G_GADGET_ATTACHMENT_START_MATRIX,
                                 AE_G_GADGET_ATTACHMENT_OUTPUT_MATRIX, draw_angle + 2048);
    if (ae_ram_failed(r)) return;

    uint32_t attachment_state = AE_G_GADGET_WORK + 4u;
    GADGET_CALL(r, h, SdkVectorHelper_8002FB00, attachment_state, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    GADGET_CALL(r, h, SdkVectorHelper_8002FB30, attachment_state, 0, 0, 0);
    if (ae_ram_failed(r)) return;

    uint32_t model = AE_G_PLAYER_STATIC_MODELS;
    uint32_t attachment_vector = AE_G_GADGET_TEMP_VECTOR + 24u;
    uint32_t attachment = ae_rd32(r, model + AE_G_GADGET_ATTACHMENT_MODEL_VECTOR);
    GADGET_CALL(r, h, SdkVectorHelper_80031230, attachment + AE_G_GADGET_ATTACHMENT_VECTOR_X,
                AE_G_GADGET_WORK + AE_G_GADGET_ATTACHMENT_POS_A, attachment_vector, 0);
    if (ae_ram_failed(r)) return;
    GADGET_CALL(r, h, SdkVectorHelper_80031230, attachment + AE_G_GADGET_ATTACHMENT_VECTOR_Z,
                AE_G_GADGET_WORK + AE_G_GADGET_ATTACHMENT_POS_B, attachment_vector, 0);
    if (ae_ram_failed(r)) return;

    player_state = ae_rd16(r, AE_G_PLAYER + AE_PLAYER_STATE);
    if (player_state >= 138u && player_state < 142u) {
        int32_t voice = ae_rds16(r, AE_G_GADGET_SWING_VOICE);
        if (voice == -1) {
            voice = GADGET_CALL(r, h, SoundLoopStart, 0, 61, 0, 0);
            if (ae_ram_failed(r)) return;
            ae_wr16(r, AE_G_GADGET_SWING_VOICE, (uint32_t)voice);
        }
        GADGET_CALL(r, h, TrailRingPush, 0, 0, 0, 0);
    } else {
        int32_t voice = ae_rds16(r, AE_G_GADGET_SWING_VOICE);
        if (voice != -1) {
            GADGET_CALL(r, h, SoundLoopStop, 61, (uint32_t)voice, 0, 0);
            if (ae_ram_failed(r)) return;
        }
        ae_wr16(r, AE_G_GADGET_SWING_VOICE, (uint32_t)-1);
    }
    GADGET_CALL(r, h, TrailRingDraw, 0, 0, 0, 0);
}

/* 8009AD60: select Slingback, maintain its aim locks, and delegate screen-space drawing. */
void ae_Gadget3UpdateDraw(AeRam *r, const AeGadgetHooks *h) {
    uint32_t stage_state = ae_rd16(r, AE_G_GADGET_SELECT_FX_AREA_STATE);
    if (stage_state != 11u &&
        (ae_rd32(r, AE_G_GADGET3_SELECT_FLAGS) & ae_rd32(r, AE_G_GADGET3_SELECT_MASK) & 0x400u) != 0) {
        GADGET_CALL(r, h, PlayerSelectGadget, 3, 3, 0, 0);
        if (ae_ram_failed(r)) return;
    }

    uint32_t player_state = ae_rd16(r, AE_G_PLAYER + AE_PLAYER_STATE);
    if (player_state == 131u) {
        if (ae_rd8(r, AE_G_GADGET3_AMMO_KIND) == 2u &&
            ae_rd8(r, AE_G_GADGET3_AMMO_2) != 0) {
            ae_Gadget3FindAimTarget(r, h);
            if (ae_ram_failed(r)) return;
        }
    } else {
        GADGET_CALL(r, h, AimTableClear3, 0, 0, 0, 0);
        if (ae_ram_failed(r)) return;
    }

    if (ae_rd8(r, AE_G_GADGET3_AIM_MODE) == 1u) return;

    uint32_t attachment_matrix = AE_G_GADGET_ATTACHMENT_OUTPUT_MATRIX;
    GADGET_CALL(r, h, ModelTransformRigid, AE_G_GADGET3_MODEL_INSTANCE,
                AE_G_GADGET_ATTACHMENT_START_MATRIX, AE_G_GADGET_MODEL_ROOT_MATRIX, 0);
    if (ae_ram_failed(r)) return;
    uint32_t model_draw_index = ae_rd32(r, AE_G_GADGET_MODEL_DRAW_INDEX);
    uint32_t draw_index = (((model_draw_index << 7) + model_draw_index) << 4) - model_draw_index;
    GADGET_CALL(r, h, ModelDraw, AE_G_GADGET_MODEL_DRAW_BASE + (draw_index << 3),
                AE_G_GADGET_MODEL_ROOT_MATRIX, AE_G_GADGET3_MODEL_INSTANCE, 0);
    if (ae_ram_failed(r)) return;

    GADGET_CALL(r, h, SdkVectorHelper_8002FB00, attachment_matrix, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    GADGET_CALL(r, h, SdkVectorHelper_8002FB30, attachment_matrix, 0, 0, 0);
    if (ae_ram_failed(r)) return;

    GADGET_CALL(r, h, GpuProjectGadget3Aim, 0, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    ae_wr16(r, AE_G_GADGET3_AIM_CACHE, 1);

    GADGET_CALL(r, h, SdkVectorHelper_8002FB00, AE_G_CAMERA_LOOK_MATRIX, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    GADGET_CALL(r, h, SdkVectorHelper_8002FB30, AE_G_CAMERA_LOOK_MATRIX, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    GADGET_CALL(r, h, GpuDrawGadget3AimLine, 0, 0, 0, 0);
}

#undef GADGET_CALL
#undef GADGET_CALL5
