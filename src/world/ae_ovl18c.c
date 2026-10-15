#include "ae_ovl18c.h"
#include "ae_area02.h"
#include "ae_solid.h"

#include <limits.h>

/* Area 02 overlay helpers. Arithmetic helpers make the R3000's low-word and
 * arithmetic-shift behavior explicit where it affects stored values. */
static int32_t ovl_s16(uint32_t value) {
    uint32_t low = value & 0xFFFFu;
    return (low & 0x8000u) ? (int32_t)(low | 0xFFFF0000u) : (int32_t)low;
}

static int32_t ovl_asr(int32_t value, unsigned shift) {
    uint32_t bits = (uint32_t)value;
    if (shift == 0) return value;
    if (shift >= 32) return value < 0 ? -1 : 0;
    if (value >= 0) return (int32_t)(bits >> shift);
    return (int32_t)~((~bits) >> shift);
}

static int32_t ovl_div(AeRam *r, int32_t numerator, int32_t denominator) {
    if (denominator == 0 || (numerator == INT32_MIN && denominator == -1)) {
        ae_ram_fail(r, "Area02 overlay signed division trap");
        return 0;
    }
    return numerator / denominator;
}

static int32_t ovl_call(AeRam *r, const AeOvl18cHooks *h, const char *name,
                        uint32_t address, uint32_t count,
                        const uint32_t *args) {
    if (!h || !h->original_call) {
        ae_ram_fail(r, "Area02 overlay call hook missing: %s", name);
        return 0;
    }
    return h->original_call(h->user, r, name, address, count, args);
}

typedef struct OvlGroundAdapter {
    const AeWorldHooks *world;
    uint32_t solid_a3;
} OvlGroundAdapter;

static int32_t ovl_solid_height(void *user, AeRam *r, uint32_t x,
                                uint32_t y, uint32_t z, uint32_t a3) {
    OvlGroundAdapter *adapter = (OvlGroundAdapter *)user;
    (void)a3;
    if (!adapter->world || !adapter->world->GetSolidHeight) {
        ae_ram_fail(r, "Area02 overlay ground hook missing: GetSolidHeight");
        return 0;
    }
    return adapter->world->GetSolidHeight(adapter->world->user, r, x, y, z,
                                          adapter->solid_a3);
}

static int32_t ovl_ground_height(AeRam *r, const AeOvl18cHooks *h,
                                int32_t x, int32_t y, int32_t z,
                                uint32_t solid_a3) {
    if (!h || !h->world_hooks)
        return ae_GetGroundHeight(r, NULL, x, y, z);
    OvlGroundAdapter context = {h->world_hooks, solid_a3};
    AeWorldHooks world = {0};
    world.user = &context;
    world.GetSolidHeight = ovl_solid_height;
    world.ground_stale_x = h->world_hooks->ground_stale_x;
    world.ground_stale_z = h->world_hooks->ground_stale_z;
    return ae_GetGroundHeight(r, &world, x, y, z);
}

static int32_t ovl_render(AeRam *r, const AeOvl18cHooks *h, const char *name,
                          uint32_t address, uint32_t count,
                          const uint32_t *args) {
    if (!h || !h->renderer) {
        ae_ram_fail(r, "Area02 overlay renderer hook missing: %s", name);
        return 0;
    }
    return h->renderer(h->user, r, name, address, count, args);
}

static uint32_t ovl_effect_slot(uint32_t index) {
    return AE_OVL18C_EFFECT_SLOTS + index * AE_OVL18C_EFFECT_SLOT_SIZE;
}

static uint32_t ovl_surface_record(uint32_t index) {
    return AE_OVL18C_SURFACE_RECORDS + index * AE_OVL18C_SURFACE_RECORD_SIZE;
}

static int32_t ovl_anim_clip_field(AeRam *r, uint32_t obj, uint32_t field) {
    uint32_t clip_table = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE);
    uint32_t clip = clip_table + ae_rd8(r, obj + AE_OBJ_ANIM_SLOT) * AE_ANIM_CLIP_RECORD_SIZE;
    return (int32_t)ae_rd16(r, clip + field);
}

void ae_Area02_SpawnAmbientParticle(AeRam *r, const AeOvl18cHooks *h,
                                    uint32_t obj) {
    uint32_t seed = ae_rd16(r, AE_SPAWN_RAND_SEED);
    uint32_t next = seed * 5u + 1u;
    ae_wr16(r, AE_SPAWN_RAND_SEED, next);
    if (((next >> 8) & 0x3Fu) != 0) return;

    int32_t facing_x = ae_rds16(r, obj + AE_OBJ_FACING_X);
    int32_t facing_z = ae_rds16(r, obj + AE_OBJ_FACING_Z);
    uint32_t position = AE_OVL18C_STACK_POSITION;
    int32_t offset_x = ovl_asr(facing_x * AE_OVL18C_AMBIENT_OFFSET_SCALE,
                               AE_OVL18C_AMBIENT_OFFSET_SHIFT);
    int32_t offset_z = ovl_asr(facing_z * AE_OVL18C_AMBIENT_OFFSET_SCALE,
                               AE_OVL18C_AMBIENT_OFFSET_SHIFT);
    ae_wr16(r, position, ae_rd16(r, obj + AE_OBJ_POS_X) - (uint32_t)offset_x);
    ae_wr16(r, position + AE_OVL18C_SOURCE_POS_Y, ae_rd16(r, obj + AE_OBJ_POS_Y));
    ae_wr16(r, position + AE_OVL18C_SOURCE_POS_Z,
            ae_rd16(r, obj + AE_OBJ_POS_Z) - (uint32_t)offset_z);
    uint16_t padding = h && h->spawn_hooks && h->spawn_hooks->position_padding
        ? h->spawn_hooks->position_padding(h->spawn_hooks->user, r, 0x8013D000u) : 0;
    ae_wr16(r, position + AE_OVL18C_SOURCE_POS_PAD, padding);
    ae_EffectSpawnFromDef(r, h ? h->spawn_hooks : NULL, position,
                          AE_OVL18C_AMBIENT_EFFECT_DEF, 1, 0, 0);
}

int32_t ae_Area02_InterpolateAnimValue(AeRam *r, uint32_t obj,
                                       int32_t value_a, int32_t value_b) {
    int32_t clip_start = ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_FIRST_FRAME);
    int32_t clip_end = ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_LAST_FRAME);
    int32_t period = clip_end - clip_start;
    int32_t frame_delta = (int32_t)ae_rd16(r, obj + AE_OBJ_ANIM_FRAME) - clip_start;
    int32_t remainder = frame_delta;
    if (period != 0) remainder = frame_delta % period;
    else (void)ovl_div(r, frame_delta, period);
    if (ae_ram_failed(r)) return 0;

    int32_t phase_numerator = (int32_t)((uint32_t)remainder << 12);
    int32_t phase = ovl_div(r, phase_numerator, period);
    if (ae_ram_failed(r)) return 0;
    int32_t angle_with_bias = phase + 3072;
    int32_t rounded_cycles = ovl_asr(angle_with_bias < 0 ? phase + 7167 : angle_with_bias, 12);
    phase = ovl_s16((uint32_t)(angle_with_bias - rounded_cycles * 4096));

    int32_t sine = ae_sdk_rsin(r, phase);
    int32_t a = ovl_s16((uint32_t)value_a);
    int32_t b = ovl_s16((uint32_t)value_b);
    int32_t average_sum = a + b;
    int32_t average = ovl_asr(average_sum + (int32_t)((uint32_t)average_sum >> 31), 1);
    int32_t delta = a - b;
    int32_t product = (int32_t)((uint32_t)((int64_t)sine * delta));
    return ovl_s16((uint32_t)(average + ovl_asr(product, 13)));
}

int32_t ae_Area02_PlayerWithinHeightAndRadius(AeRam *r,
                                               const AeOvl18cHooks *h) {
    int32_t player_y = ae_rds16(r, AE_OVL18C_ENEMY_LOOK_Y);
    int32_t ground_y = ae_rds32(r, AE_OVL18C_PLAYER_GROUND_Y);
    int32_t delta_y = player_y - ground_y;
    if (delta_y < 0) delta_y = -delta_y;
    if (delta_y >= 50) return 0;
    uint32_t args[3] = {
        (uint32_t)ae_rds16(r, AE_OVL18C_ENEMY_LOOK_X),
        (uint32_t)ae_rds16(r, AE_OVL18C_ENEMY_LOOK_Z), 200u
    };
    return ovl_call(r, h, "PlayerWithinRadiusXZ", 0x800570F4u, 3, args) != 0;
}

int32_t ae_Area02_PlayerOutsideHeightAndRadius(AeRam *r,
                                                const AeOvl18cHooks *h) {
    int32_t player_y = ae_rds16(r, AE_OVL18C_ENEMY_LOOK_Y);
    int32_t ground_y = ae_rds32(r, AE_OVL18C_PLAYER_GROUND_Y);
    int32_t delta_y = player_y - ground_y;
    if (delta_y < 0) delta_y = -delta_y;
    if (delta_y >= 51) return 1;
    uint32_t args[3] = {
        (uint32_t)ae_rds16(r, AE_OVL18C_ENEMY_LOOK_X),
        (uint32_t)ae_rds16(r, AE_OVL18C_ENEMY_LOOK_Z), 250u
    };
    return ovl_call(r, h, "PlayerWithinRadiusXZ", 0x800570F4u, 3, args) == 0;
}

void ae_Area02_EnemyAnimationTriggerUpdate(AeRam *r,
                                           const AeOvl18cHooks *h,
                                           uint32_t obj) {
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
    uint32_t animation_slot = ae_rd8(r, obj + AE_OBJ_ANIM_SLOT);
    if ((flags & 1u) != 0) {
        if (ae_Area02_PlayerOutsideHeightAndRadius(r, h)) {
            if (animation_slot != 1u) {
                ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 1);
                ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
                        (uint32_t)ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_FIRST_FRAME));
            }
            flags = ae_rd32(r, obj + AE_OBJ_FLAGS) & ~1u;
            ae_wr32(r, obj + AE_OBJ_FLAGS, flags);
        }
    } else if (ae_Area02_PlayerWithinHeightAndRadius(r, h)) {
        if (animation_slot != 1u) {
            ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 1);
            ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
                    (uint32_t)ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_FIRST_FRAME));
        }
        flags = ae_rd32(r, obj + AE_OBJ_FLAGS) | 1u;
        ae_wr32(r, obj + AE_OBJ_FLAGS, flags);
    }

    if (ae_rd8(r, obj + AE_OBJ_ANIM_SLOT) != 1u) return;
    int32_t clip_start = ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_FIRST_FRAME);
    int32_t clip_end = ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_LAST_FRAME);
    uint32_t frame = ae_rd16(r, obj + AE_OBJ_ANIM_FRAME);
    if ((int32_t)frame == clip_start + 15) {
        ae_ObjPlaySfx(r, h ? h->zako1_hooks : NULL, obj, 0xAD);
    } else if ((int32_t)frame == clip_end) {
        ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 0);
        ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
                (uint32_t)ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_FIRST_FRAME));
    }
}

void ae_Area02_UpdateEnemyWakeOrigin(AeRam *r, uint32_t obj) {
    int32_t facing_x = ae_rds16(r, obj + AE_OBJ_FACING_X);
    int32_t facing_z = ae_rds16(r, obj + AE_OBJ_FACING_Z);
    int32_t offset_x = ovl_asr(-150 * facing_x, 12);
    int32_t offset_z = ovl_asr(-150 * facing_z, 12);
    ae_wr16(r, AE_OVL18C_ENEMY_LOOK_Y,
            (uint32_t)ae_rd16(r, obj + AE_OVL18C_OBJ_WAKE_ORIGIN_Y) -
                AE_OVL18C_LOOK_Y_OFFSET);
    ae_wr16(r, AE_OVL18C_ENEMY_LOOK_X,
            (uint32_t)ae_rd16(r, obj + AE_OBJ_POS_X) + (uint32_t)offset_x);
    ae_wr16(r, AE_OVL18C_ENEMY_LOOK_Z,
            (uint32_t)ae_rd16(r, obj + AE_OBJ_POS_Z) + (uint32_t)offset_z);
}

void ae_Area02_UpdateEnemyWake(AeRam *r, const AeOvl18cHooks *h,
                               uint32_t obj) {
    uint32_t remaining = ae_rd8(r, obj + AE_OVL18C_OBJ_WAKE_TIMER);
    if (remaining != 0) {
        ae_wr8(r, obj + AE_OVL18C_OBJ_WAKE_TIMER, remaining - 1u);
        return;
    }
    uint32_t linked_args[2] = {obj, 0};
    int32_t linked_state = ovl_call(r, h, "ObjGetLinkedObjState", 0x800582ECu,
                                    2, linked_args);
    if (ae_ram_failed(r)) return;
    if (linked_state == 6) {
        ae_wr8(r, obj + AE_OVL18C_OBJ_WAKE_TIMER, 10);
        return;
    }

    int32_t facing_x = ae_rds16(r, obj + AE_OBJ_FACING_X);
    int32_t facing_z = ae_rds16(r, obj + AE_OBJ_FACING_Z);
    int32_t offset_x = ovl_asr(175 * facing_x, 11);
    int32_t offset_z = ovl_asr(175 * facing_z, 11);
    uint32_t x = (uint32_t)ae_rd16(r, obj + AE_OBJ_POS_X) + (uint32_t)offset_x;
    uint32_t y = (uint32_t)ae_rd16(r, obj + AE_OBJ_POS_Y) -
                 AE_OVL18C_WAKE_HIT_Y_OFFSET;
    uint32_t z = (uint32_t)ae_rd16(r, obj + AE_OBJ_POS_Z) + (uint32_t)offset_z;
    uint32_t args[7] = {obj, 200u, x, y, z, 0u, 0x101u};
    ae_wr32(r, obj + AE_OBJ_HIT_TARGET,
            (uint32_t)ovl_call(r, h, "ObjAddHitBody", 0x8005813Cu, 7, args));
}

void ae_Area02_EnemyType0B_Update(AeRam *r, const AeOvl18cHooks *h,
                                  uint32_t obj) {
    uint32_t flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
    if ((flags & 8u) == 0) {
        ae_wr16(r, obj + AE_OBJ_BODY_FLAGS, 0xC000u);
        flags = ae_rd32(r, obj + AE_OBJ_FLAGS);
        ae_wr8(r, obj + AE_OBJ_PATH_INDEX, 0);
        ae_wr16(r, obj + AE_OBJ_BODY_RADIUS, 0);
        ae_wr16(r, obj + AE_OBJ_BODY_PARAM, 0);
        flags &= ~0x1002u;
        ae_wr32(r, obj + AE_OBJ_FLAGS, flags);
        if (ae_rd16(r, obj + AE_OBJ_PATH_POINT) != 0) {
            ae_ObjSetPosToPathPoint(r, obj, 0);
            if (ae_ram_failed(r)) return;
            ae_ObjFacePathPoint(r, obj, 1);
            if (ae_ram_failed(r)) return;
        }
        flags = ae_rd32(r, obj + AE_OBJ_FLAGS) | 0x40u;
        ae_wr32(r, obj + AE_OBJ_FLAGS, flags);
        uint32_t record = ae_rd32(r, AE_OVL18C_ENEMY_DISTANCE_RECORD_POINTER) +
            ae_rd16(r, obj + AE_OBJ_OWN_COLLIDER) * AE_OVL18C_ENEMY_DISTANCE_RECORD_SIZE;
        ae_wr16(r, record + AE_OVL18C_DISTANCE_MIN, 250);
        ae_wr16(r, record + AE_OVL18C_DISTANCE_MAX, 250);
        ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 0);
        ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
                (uint32_t)ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_FIRST_FRAME));
        flags = ae_rd32(r, obj + AE_OBJ_FLAGS) | 8u;
        ae_wr32(r, obj + AE_OBJ_FLAGS, flags);
    }

    ae_Area02_UpdateEnemyWakeOrigin(r, obj);
    ae_Area02_SpawnAmbientParticle(r, h, obj);
    int32_t speed = ae_Area02_InterpolateAnimValue(r, obj, 20, 8);
    if (ae_ram_failed(r)) return;
    ae_ObjFollowPath(r, h ? h->zako1_hooks : NULL, obj, speed, 0, 0);
    if (ae_ram_failed(r)) return;
    ae_ObjMove(r, obj);
    if (ae_ram_failed(r)) return;
    ae_Area02_EnemyAnimationTriggerUpdate(r, h, obj);
    ae_Area02_UpdateEnemyWake(r, h, obj);
}

void ae_Area02_CopyEnemyModel(AeRam *r, uint32_t source,
                              uint32_t destination, uint32_t type) {
    ae_ObjInitModel(r, destination, type);
    (void)ae_ZakoGetAnimTable(r, destination, type);
    ae_wr16(r, destination + AE_OBJ_ANIM_FRAME, 0);
    ae_wr16(r, destination + AE_OVL18C_OBJ_FRAME_PARAM, 3);
    for (uint32_t offset = AE_OBJ_FACING_X; offset < AE_OBJ_FACING_X + 8u; ++offset)
        ae_wr8(r, destination + offset, ae_rd8(r, source + offset));
    for (uint32_t offset = AE_OBJ_POS_X; offset <= AE_OBJ_POS_Z; offset += 4u)
        ae_wr32(r, destination + offset, ae_rd32(r, source + offset));
}

void ae_Area02_InitializeEffects(AeRam *r, const AeOvl18cHooks *h,
                                 uint32_t obj) {
    (void)h;
    ae_Area02_CopyEnemyModel(r, obj, AE_OVL18C_ENEMY_MODEL_INSTANCE, 0x83u);
    for (uint32_t i = 0; i < AE_OVL18C_EFFECT_SLOT_COUNT; ++i)
        ae_wr16(r, ovl_effect_slot(i) + AE_OVL18C_EFFECT_AGE, 0);
    (void)ae_ZakoHooksAdd(r, 0x8013DA94u);
}

int32_t ae_Area02_FindFreeEffectSlot(AeRam *r) {
    for (int32_t index = (int32_t)AE_OVL18C_EFFECT_SLOT_COUNT - 1; index >= 0; --index) {
        if (ae_rd16(r, ovl_effect_slot((uint32_t)index) + AE_OVL18C_EFFECT_AGE) == 0)
            return index;
    }
    return -1;
}

void ae_Area02_SpawnRadialEffect(AeRam *r, const AeOvl18cHooks *h,
                                uint32_t obj, int32_t y_offset,
                                int32_t radius) {
    int32_t slot_index = ae_Area02_FindFreeEffectSlot(r);
    if (slot_index < 0) return;
    y_offset = ovl_s16((uint32_t)y_offset);
    radius = ovl_s16((uint32_t)radius);
    int32_t angle = ae_rds16(r, obj + AE_OBJ_HEADING);
    int32_t cosine = ae_sdk_rcos(r, angle);
    int32_t sine = ae_sdk_rsin(r, angle);
    uint32_t slot = ovl_effect_slot((uint32_t)slot_index);
    int32_t x = ae_rds16(r, obj + AE_OBJ_POS_X) + ovl_asr(cosine * radius, 12);
    int32_t y = ae_rds16(r, obj + AE_OBJ_POS_Y) - y_offset;
    int32_t z = ae_rds16(r, obj + AE_OBJ_POS_Z) + ovl_asr(sine * radius, 12);
    ae_wr16(r, slot + AE_OVL18C_EFFECT_POS_X, x);
    ae_wr16(r, slot + AE_OVL18C_EFFECT_POS_Y, y);
    ae_wr16(r, slot + AE_OVL18C_EFFECT_POS_Z, z);
    ae_wr16(r, slot + AE_OVL18C_EFFECT_VEL_X, cosine / 48);
    ae_wr16(r, slot + AE_OVL18C_EFFECT_VEL_Y, AE_OVL18C_RADIAL_Y_VELOCITY);
    ae_wr16(r, slot + AE_OVL18C_EFFECT_VEL_Z, sine / 48);
    ae_wr16(r, slot + AE_OVL18C_EFFECT_AGE,
            ae_rd16(r, slot + AE_OVL18C_EFFECT_AGE) + 1u);
    ae_wr16(r, slot + AE_OVL18C_EFFECT_OWNER,
            (uint32_t)(int32_t)ae_rds8(r, obj + AE_OVL18C_OBJ_EFFECT_INDEX));
    (void)h;
}

int32_t ae_Area02_EffectBelowGround(AeRam *r,
                                    const AeOvl18cHooks *h,
                                    uint32_t effect) {
    int32_t ground = ovl_ground_height(r, h,
        ae_rds16(r, effect + AE_OVL18C_EFFECT_POS_X),
        ae_rds16(r, effect + AE_OVL18C_EFFECT_POS_Y),
        ae_rds16(r, effect + AE_OVL18C_EFFECT_POS_Z),
        ae_rd16(r, effect + AE_OVL18C_EFFECT_VEL_Z));
    return ground < ae_rds16(r, effect + AE_OVL18C_EFFECT_POS_Y);
}

void ae_Area02_SpawnEffectParticle(AeRam *r, const AeOvl18cHooks *h,
                                   uint32_t position) {
    ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 4, position);
}

void ae_Area02_SpawnEffectBurst(AeRam *r, const AeOvl18cHooks *h,
                                uint32_t position) {
    for (int32_t i = 0; i < 16; ++i) {
        ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 10, position);
        if ((i & 1) != 0) ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 22, position);
    }
    ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 5, position);
    ae_SpawnParticle(r, h ? h->spawn_hooks : NULL, 264, position);
}

void ae_Area02_UpdateEffects(AeRam *r, const AeOvl18cHooks *h) {
    for (int32_t index = (int32_t)AE_OVL18C_EFFECT_SLOT_COUNT - 1; index >= 0; --index) {
        uint32_t slot = ovl_effect_slot((uint32_t)index);
        uint32_t age = ae_rd16(r, slot + AE_OVL18C_EFFECT_AGE);
        if (age == 0) continue;
        ae_wr16(r, slot + AE_OVL18C_EFFECT_AGE, age + 1u);
        ae_Area02_SpawnEffectParticle(r, h, slot);
        int32_t velocity_y = ae_rds16(r, slot + AE_OVL18C_EFFECT_VEL_Y) +
                             AE_OVL18C_EFFECT_GRAVITY_STEP;
        int32_t x = ae_rds16(r, slot + AE_OVL18C_EFFECT_POS_X) +
                    ae_rds16(r, slot + AE_OVL18C_EFFECT_VEL_X);
        int32_t y = ae_rds16(r, slot + AE_OVL18C_EFFECT_POS_Y) + velocity_y;
        int32_t z = ae_rds16(r, slot + AE_OVL18C_EFFECT_POS_Z) +
                    ae_rds16(r, slot + AE_OVL18C_EFFECT_VEL_Z);
        ae_wr16(r, slot + AE_OVL18C_EFFECT_VEL_Y, velocity_y);
        ae_wr16(r, slot + AE_OVL18C_EFFECT_POS_X, x);
        ae_wr16(r, slot + AE_OVL18C_EFFECT_POS_Y, y);
        ae_wr16(r, slot + AE_OVL18C_EFFECT_POS_Z, z);
        int32_t owner_index = ae_rds16(r, slot + AE_OVL18C_EFFECT_OWNER);
        uint32_t owner = AE_OVL18C_EFFECT_OBJECTS +
                         (uint32_t)owner_index * AE_OVL18C_EFFECT_OBJECT_SIZE;
        uint32_t linked_args[2] = {owner, 0};
        int32_t linked_state = ovl_call(r, h, "ObjGetLinkedObjState", 0x800582ECu,
                                        2, linked_args);
        if (ae_ram_failed(r)) return;
        if (linked_state == 6) {
            ae_wr16(r, slot + AE_OVL18C_EFFECT_AGE, 0);
            ae_Area02_SpawnEffectBurst(r, h, slot);
            ae_ObjPlaySfx(r, h ? h->zako1_hooks : NULL, owner, 0x6B);
            continue;
        }
        if (ae_Area02_EffectBelowGround(r, h, slot) ||
            ae_rds32(r, owner + AE_OBJ_POS_Y) + AE_OVL18C_EFFECT_GROUND_CLEARANCE <
                ae_rds16(r, slot + AE_OVL18C_EFFECT_POS_Y)) {
            ae_wr16(r, slot + AE_OVL18C_EFFECT_AGE, 0);
            ae_Area02_SpawnEffectBurst(r, h, slot);
            continue;
        }
        uint32_t enemy = AE_OVL18C_ENEMY_MODEL_INSTANCE;
        ae_wr16(r, enemy + AE_OBJ_POS_X, x);
        ae_wr16(r, enemy + AE_OBJ_POS_Y, y);
        ae_wr16(r, enemy + AE_OBJ_POS_Z, z);
        ae_wr16(r, enemy + AE_OBJ_FACING_X, ae_rd16(r, slot + AE_OVL18C_EFFECT_VEL_X));
        ae_wr16(r, enemy + AE_OBJ_FACING_Y, ae_rd16(r, slot + AE_OVL18C_EFFECT_VEL_Y));
        ae_wr16(r, enemy + AE_OBJ_FACING_Z, ae_rd16(r, slot + AE_OVL18C_EFFECT_VEL_Z));
        (void)ae_sdk_VectorNormalSS(r, enemy + AE_OBJ_FACING_X,
                                    enemy + AE_OBJ_FACING_X);
        uint32_t draw_args[2] = {0, 3};
        (void)ovl_render(r, h, "Area02_EnemyModelDraw", 0x8013D700u, 2, draw_args);
        uint32_t hit_args[7] = {owner, 30u,
            (uint32_t)ae_rds16(r, slot + AE_OVL18C_EFFECT_POS_X),
            (uint32_t)ae_rds16(r, slot + AE_OVL18C_EFFECT_POS_Y),
            (uint32_t)ae_rds16(r, slot + AE_OVL18C_EFFECT_POS_Z), 1u, 0x203u};
        uint32_t hit_target = (uint32_t)ovl_call(r, h, "ObjAddHitBody", 0x8005813Cu,
                                                 7, hit_args);
        ae_wr32(r, owner + AE_OBJ_HIT_TARGET, hit_target);
    }
}

void ae_Area02_SpawnEnemyHitParticle(AeRam *r, const AeOvl18cHooks *h,
                                     uint32_t obj) {
    uint32_t args[5] = {obj, 262u, 0, 0, 0};
    (void)ovl_call(r, h, "ObjSpawnParticleAt", 0x80057504u, 5, args);
}

void ae_Area02_EnemyHitUpdate(AeRam *r, const AeOvl18cHooks *h,
                              uint32_t obj) {
    if ((ae_rd32(r, obj + AE_OVL18C_OBJ_LINKED_BODY) >> 28) != 0x0Bu) return;
    if ((ae_rd32(r, obj + AE_OBJ_FLAGS) & 2u) != 0) return;
    uint32_t animation_slot = ae_rd8(r, obj + AE_OBJ_ANIM_SLOT);
    ae_wr8(r, obj + AE_OVL18C_OBJ_PREV_EFFECT_ANIM, animation_slot);
    ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, ae_rd8(r, obj + AE_OVL18C_OBJ_EFFECT_ANIM));
    ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
            (uint32_t)ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_FIRST_FRAME));
    ae_ObjPuffAt(r, h ? h->zako1_hooks : NULL, obj, 0, 0, 0);
    ae_ZakoOnHitByGadget(r, h ? h->zako1_hooks : NULL, obj);
    ae_ObjDeathSfx(r, h ? h->zako1_hooks : NULL, obj);
}

void ae_Area02_EnemyEnterHitReaction(AeRam *r, const AeOvl18cHooks *h,
                                     uint32_t obj) {
    ae_wr8(r, obj + AE_OBJ_AI_STATE, 3);
    ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 1);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_X, 0);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Y, 0);
    ae_wr16(r, obj + AE_OBJ_VELOCITY_Z, 0);
    ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
            (uint32_t)ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_FIRST_FRAME));
    ae_Area02_SpawnEnemyHitParticle(r, h, obj);
    ae_ObjTurnTowardsXZ(r, h ? h->zako1_hooks : NULL, obj,
                        ae_rds16(r, AE_OVL18C_PLAYER_POSITION + AE_OBJ_POS_X) -
                            ae_rds16(r, obj + AE_OBJ_POS_X),
                        ae_rds16(r, AE_OVL18C_PLAYER_POSITION + AE_OBJ_POS_Z) -
                            ae_rds16(r, obj + AE_OBJ_POS_Z), 0, 4096);
}

void ae_Area02_EnemySplashUpdate(AeRam *r, const AeOvl18cHooks *h,
                                 uint32_t obj) {
    uint32_t timer = (ae_rd8(r, obj + AE_OVL18C_OBJ_WAKE_TIMER) + 1u) & 0xFFu;
    ae_wr8(r, obj + AE_OVL18C_OBJ_WAKE_TIMER, timer);
    if (timer < 2u) ae_Area02_SpawnRadialEffect(r, h, obj,
        AE_OVL18C_RADIAL_Y_OFFSET, AE_OVL18C_RADIAL_RADIUS);
    int32_t sound = timer < 2u ? 75 : 72;
    ae_ObjPlaySfx(r, h ? h->zako1_hooks : NULL, obj, sound);
}

void ae_Area02_EnemyEnterDefeat(AeRam *r, const AeOvl18cHooks *h,
                                uint32_t obj) {
    ae_wr16(r, obj + AE_OBJ_BODY_RADIUS, 200);
    ae_wr16(r, obj + AE_OVL18C_OBJ_DEFEAT_FIELD, 91);
    ae_wr16(r, obj + AE_OBJ_BODY_FLAGS, 0x8103u);
    ae_wr8(r, obj + AE_OVL18C_OBJ_EFFECT_ANIM, 5);
    ae_wr8(r, obj + AE_OBJ_DEFENSE, 8);
    ae_wr8(r, obj + AE_OBJ_SCALE, 14);
    ae_wr8(r, obj + AE_OVL18C_OBJ_DEFEAT_FLAGS, 1);
    ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 1);
    ae_wr32(r, obj + AE_OBJ_FLAGS, ae_rd32(r, obj + AE_OBJ_FLAGS) & ~0x1000u);
    ae_wr16(r, obj + AE_OVL18C_OBJ_DRAW_SCALE, 0);
    ae_wr16(r, obj + AE_OVL18C_OBJ_RESET_FIELD, 0);
    uint32_t animation_table = ae_rd32(r, obj + AE_OBJ_ANIM_TABLE);
    ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
            ae_rd16(r, animation_table + AE_ANIM_CLIP_RECORD_SIZE));
    int32_t random = ae_rds16(r, obj + AE_OVL18C_OBJ_HIT_REACT_PARAM);
    int32_t cooldown = (int32_t)((uint32_t)(random % 15) + 60u);
    uint32_t args[2] = {obj, (uint32_t)cooldown};
    (void)ovl_call(r, h, "ObjCooldown", 0x8005588Cu, 2, args);
    ae_ObjUnregisterBody(r, h ? h->zako_hooks : NULL, obj);
}

static int32_t ovl_animation_finished(AeRam *r, uint32_t obj) {
    uint32_t end = (uint32_t)ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_LAST_FRAME);
    return ae_rd16(r, obj + AE_OBJ_ANIM_FRAME) >= end;
}

int32_t ae_Area02_EnemyPhase6Update(AeRam *r,
                                    const AeOvl18cHooks *h,
                                    uint32_t obj) {
    uint32_t turn_args[1] = {obj};
    (void)ovl_call(r, h, "ObjTurnToPlayer", 0x8005696Cu, 1, turn_args);
    if ((ae_rd32(r, obj + AE_OVL18C_OBJ_LINKED_BODY) >> 28) != 6u)
        return 0;
    ae_wr8(r, obj + AE_OBJ_AI_STATE, 6);
    ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 0);
    ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
            (uint32_t)ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_FIRST_FRAME));
    ae_Area02_SpawnEnemyHitParticle(r, h, obj);
    return 1;
}

void ae_Area02_StageSpecificEnemyUpdate(AeRam *r,
                                        const AeOvl18cHooks *h,
                                        uint32_t obj) {
    if ((ae_rd32(r, obj + AE_OBJ_FLAGS) & 8u) == 0) {
        ae_Area02_EnemyEnterDefeat(r, h, obj);
        ae_wr16(r, obj + AE_OVL18C_OBJ_DRAW_SCALE, 100);
        ae_wr8(r, obj + AE_OVL18C_OBJ_RENDER_MODE, 0);
        ae_wr8(r, obj + AE_OVL18C_OBJ_RENDER_COOLDOWN, 0);
        ae_wr32(r, obj + AE_OBJ_FLAGS, ae_rd32(r, obj + AE_OBJ_FLAGS) & ~1u);
        ae_Area02_InitializeEffects(r, h, obj);
        ae_wr32(r, obj + AE_OBJ_FLAGS, ae_rd32(r, obj + AE_OBJ_FLAGS) | 8u);
    }

    uint32_t nop_args[2] = {obj, ae_rd16(r, obj + AE_OBJ_BODY_RADIUS)};
    (void)ovl_call(r, h, "Zako_Nop", 0x80058134u, 2, nop_args);
    uint32_t state = ae_rd8(r, obj + AE_OBJ_AI_STATE);
    switch (state) {
    case 1: {
        ae_wr8(r, obj + AE_OVL18C_OBJ_RENDER_MODE, 0);
        int32_t ground_y = ae_ObjGroundHeightCached(r, h ? h->zako1_hooks : NULL,
                                                    obj, 0, 0, 0);
        uint32_t shadow_args[4] = {obj + AE_OBJ_POS_X, (uint32_t)ground_y,
                                   (uint32_t)ae_rds16(r, obj + AE_OVL18C_OBJ_DRAW_SCALE),
                                   ae_rd32(r, obj + AE_OBJ_GROUND_HEIGHT)};
        (void)ovl_render(r, h, "DrawShadow", 0x8001F604u, 4, shadow_args);
        uint32_t cooldown_args[2] = {obj, 0xFFFFFFFFu};
        if (ovl_call(r, h, "ObjCooldown", 0x8005588Cu, 2, cooldown_args) != 0)
            break;
        uint32_t near_args[5] = {obj,
            (uint32_t)ae_rds16(r, obj + AE_OVL18C_OBJ_HIT_REACT_PARAM),
            ae_rd32(r, obj + AE_OBJ_POS_Y),
            (uint32_t)ae_rds16(r, obj + AE_OVL18C_OBJ_HIT_REACT_PARAM_Z), 1500u};
        if (ovl_call(r, h, "ObjPlayerNearPointXZ", 0x8005654Cu, 5, near_args) != 0)
            break;
        ae_Area02_EnemyEnterHitReaction(r, h, obj);
        ae_wr8(r, obj + AE_OVL18C_OBJ_RENDER_MODE, 14);
        ae_ObjReenableBody(r, h ? h->zako1_hooks : NULL, obj);
        break;
    }
    case 3: {
        uint32_t turn_args[1] = {obj};
        (void)ovl_call(r, h, "ObjTurnToPlayer", 0x8005696Cu, 1, turn_args);
        if (ovl_animation_finished(r, obj)) {
            if (ae_rd8(r, obj + AE_OBJ_ANIM_SLOT) == 1u) {
                ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 4);
                ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
                        (uint32_t)ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_FIRST_FRAME));
            } else {
                ae_wr8(r, obj + AE_OBJ_AI_STATE, 4);
                ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 3);
                ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
                        (uint32_t)ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_FIRST_FRAME));
                ae_wr8(r, obj + AE_OVL18C_OBJ_WAKE_TIMER, 0);
            }
        }
        break;
    }
    case 4:
        if (ae_Area02_EnemyPhase6Update(r, h, obj) == 0) {
            if (ae_rd16(r, obj + AE_OBJ_ANIM_FRAME) ==
                (uint32_t)ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_FIRST_FRAME) + 20u)
                ae_Area02_EnemySplashUpdate(r, h, obj);
            if (ae_rd8(r, obj + AE_OVL18C_OBJ_WAKE_TIMER) < 3u) break;
            ae_wr8(r, obj + AE_OBJ_AI_STATE, 6);
            ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 0);
            ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
                    (uint32_t)ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_FIRST_FRAME));
            ae_Area02_SpawnEnemyHitParticle(r, h, obj);
        }
        break;
    case 6:
        ae_ObjUnregisterBody(r, h ? h->zako_hooks : NULL, obj);
        if (ovl_animation_finished(r, obj)) {
            ae_wr8(r, obj + AE_OBJ_AI_STATE, 1);
            ae_wr8(r, obj + AE_OBJ_ANIM_SLOT, 1);
            ae_wr16(r, obj + AE_OBJ_ANIM_FRAME,
                    (uint32_t)ovl_anim_clip_field(r, obj, AE_ANIM_CLIP_FIRST_FRAME));
            ae_Area02_SpawnEnemyHitParticle(r, h, obj);
            uint32_t cooldown_args[2] = {obj, 60u};
            (void)ovl_call(r, h, "ObjCooldown", 0x8005588Cu, 2, cooldown_args);
        }
        break;
    default:
        break;
    }
    ae_Area02_EnemyHitUpdate(r, h, obj);
}

int32_t ae_Area02_SurfaceEffectsInit(AeRam *r,
                                     const AeOvl18cHooks *h) {
    uint32_t source = ae_rd32(r, AE_OVL18C_SURFACE_SOURCE_POINTER);
    ae_wr8(r, AE_OVL18C_SURFACE_RECORD_COUNT, 0);
    if (ae_rd16(r, source + AE_OVL18C_SURFACE_HEADER_MAGIC) != AE_OVL18C_SURFACE_MAGIC ||
        ae_rd8(r, source + AE_OVL18C_SURFACE_HEADER_KIND) != AE_OVL18C_SURFACE_VALID_KIND)
        return 1;
    uint32_t count = ae_rd8(r, source + AE_OVL18C_SURFACE_HEADER_COUNT);
    if (count > AE_OVL18C_SURFACE_COUNT_MAX) count = AE_OVL18C_SURFACE_COUNT_MAX;
    ae_wr8(r, AE_OVL18C_SURFACE_RECORD_COUNT, count);
    source += AE_OVL18C_SURFACE_HEADER_DATA;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t source_record = source + i * AE_OVL18C_SURFACE_SOURCE_STRIDE;
        uint32_t record = ovl_surface_record(i);
        ae_wr8(r, record + AE_OVL18C_SURFACE_TIMER, 0);
        int32_t x = ae_rds16(r, source_record + AE_OVL18C_SURFACE_SOURCE_X);
        int32_t y = ae_rds16(r, source_record + AE_OVL18C_SURFACE_SOURCE_Y);
        int32_t z = ae_rds16(r, source_record + AE_OVL18C_SURFACE_SOURCE_Z);
        ae_wr16(r, record + AE_OVL18C_SURFACE_POS_X, x);
        ae_wr16(r, record + AE_OVL18C_SURFACE_POS_Y, y);
        ae_wr16(r, record + AE_OVL18C_SURFACE_POS_Z, z);
        ae_wr16(r, record + AE_OVL18C_SURFACE_PREV_X, x);
        ae_wr16(r, record + AE_OVL18C_SURFACE_PREV_Y, y);
        ae_wr16(r, record + AE_OVL18C_SURFACE_PREV_Z, z);
        int32_t ground = ovl_ground_height(r, h, x,
            y + AE_OVL18C_SURFACE_GROUND_OFFSET_1, z, 0);
        ae_wr16(r, record + AE_OVL18C_SURFACE_GROUND_Y, ground);
        if (ground < y) {
            ground = ovl_ground_height(r, h, x,
                y + AE_OVL18C_SURFACE_GROUND_OFFSET_2, z, 0);
            ae_wr16(r, record + AE_OVL18C_SURFACE_GROUND_Y, ground);
        }
        ae_wr16(r, record + AE_OVL18C_SURFACE_FLAG, 1);
        ae_wr16(r, record + AE_OVL18C_SURFACE_PARAM,
                ae_rd16(r, source_record + AE_OVL18C_SURFACE_SOURCE_PARAM));
    }
    return 0;
}

static uint32_t ovl_draw_record(uint32_t index) {
    return AE_OVL18C_SURFACE_RECORDS +
           index * AE_OVL18C_SURFACE_RECORD_SIZE;
}

static int32_t ovl_surface_phase_step(int32_t phase) {
    return (int32_t)((uint32_t)phase * AE_OVL18C_SURFACE_PHASE_STEP +
                     AE_OVL18C_SURFACE_PHASE_BIAS);
}

static int32_t ovl_surface_phase_coordinate(uint16_t phase_u,
                                            uint16_t phase_v) {
    int32_t u = (int32_t)(phase_u >> 8) - AE_OVL18C_SURFACE_PHASE_ORIGIN;
    int32_t v = (int32_t)(phase_v >> 8) - AE_OVL18C_SURFACE_PHASE_ORIGIN;
    return u * AE_OVL18C_SURFACE_PHASE_U_SCALE +
           v * AE_OVL18C_SURFACE_PHASE_V_SCALE;
}

static uint32_t ovl_surface_projection_args(AeRam *r,
                                            const AeOvl18cHooks *h,
                                            const char *name,
                                            uint32_t address,
                                            uint32_t output_xy,
                                            uint32_t output_depth) {
    uint32_t point = AE_OVL18C_SURFACE_SCRATCH +
                     AE_OVL18C_SURFACE_SCRATCH_POINT_X;
    uint32_t args[6] = {
        (uint32_t)ae_rds16(r, point),
        (uint32_t)ae_rds16(r, point + 2u),
        (uint32_t)ae_rds16(r, point + 4u),
        output_xy,
        output_depth,
        AE_OVL18C_SURFACE_VIEW_MATRIX
    };
    (void)ovl_render(r, h, name, address, 6, args);
    return ae_rd32(r, output_xy);
}

static void ovl_surface_setup_projection(AeRam *r,
                                         const AeOvl18cHooks *h) {
    const AeZako1Hooks *hooks = h ? h->zako1_hooks : NULL;
    if (!hooks || !hooks->FUN_8002FB00 || !hooks->FUN_8002FB30) {
        ae_ram_fail(r, "Area02 surface projection setup hooks not provided");
        return;
    }
    (void)hooks->FUN_8002FB00(hooks->user, r,
                              AE_OVL18C_SURFACE_VIEW_MATRIX, 0, 0, 0);
    if (ae_ram_failed(r)) return;
    (void)hooks->FUN_8002FB30(hooks->user, r,
                              AE_OVL18C_SURFACE_VIEW_MATRIX, 0, 0, 0);
}

/* 8013E33C: advance the overlay's effect records, then submit the resulting
 * positions and animation fields to the native renderer. */
int32_t ae_Area02_EffectDraw(AeRam *r, const AeOvl18cHooks *h) {
    uint32_t count = ae_rd8(r, AE_OVL18C_SURFACE_RECORD_COUNT);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t record = ovl_draw_record(i);
        if (ae_rd8(r, record + AE_OVL18C_DRAW_RECORD_TIMER) != 0u) continue;

        uint32_t lifetime = (ae_rd16(r, record + AE_OVL18C_DRAW_RECORD_LIFETIME) - 1u) & 0xFFFFu;
        ae_wr16(r, record + AE_OVL18C_DRAW_RECORD_LIFETIME, lifetime);
        if (lifetime != 0u) continue;

        ae_wr8(r, record + AE_OVL18C_DRAW_RECORD_FRAME, 0);
        ae_wr8(r, record + AE_OVL18C_DRAW_RECORD_TIMER, 1);
        ae_wr16(r, record + AE_OVL18C_DRAW_RECORD_LIFETIME,
                ae_rd16(r, record + AE_OVL18C_DRAW_RECORD_RELOAD));
        ae_wr16(r, record + AE_OVL18C_SURFACE_POS_X,
                ae_rd16(r, record + AE_OVL18C_DRAW_RECORD_ORIGIN_X));
        ae_wr16(r, record + AE_OVL18C_SURFACE_POS_Y,
                ae_rd16(r, record + AE_OVL18C_DRAW_RECORD_ORIGIN_Y));
        ae_wr16(r, record + AE_OVL18C_SURFACE_POS_Z,
                ae_rd16(r, record + AE_OVL18C_DRAW_RECORD_ORIGIN_Z));
        ae_wr16(r, record + AE_OVL18C_DRAW_RECORD_DELTA_X, 0);
        ae_wr16(r, record + AE_OVL18C_DRAW_RECORD_DELTA_Y, 0);
        ae_wr16(r, record + AE_OVL18C_DRAW_RECORD_DELTA_Z, 0);
        ae_wr8(r, record + AE_OVL18C_DRAW_RECORD_TYPE, 0);
        ae_wr32(r, record + AE_OVL18C_DRAW_RECORD_TEXTURE, 0x170F60C0u);
    }

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t record = ovl_draw_record(i);
        if (ae_rd8(r, record + AE_OVL18C_DRAW_RECORD_TIMER) == 0u) continue;

        uint32_t kind = ae_rd8(r, record + AE_OVL18C_DRAW_RECORD_TYPE);
        if (kind == 0u) {
            uint32_t frame = ae_rd8(r, record + AE_OVL18C_DRAW_RECORD_FRAME);
            if (frame == 10u || frame == 34u) {
                uint32_t frame_phase = (ae_rd8(r, record + AE_OVL18C_DRAW_RECORD_TEXTURE) + 16u) & 0xFFu;
                ae_wr8(r, record + AE_OVL18C_DRAW_RECORD_TEXTURE, frame_phase);
            }

            if (frame >= 34u) {
                uint32_t next_height = (uint32_t)(uint16_t)ae_rd16(
                    r, record + AE_OVL18C_DRAW_RECORD_HEIGHT) + 15u;
                next_height &= 0xFFFFu;
                ae_wr16(r, record + AE_OVL18C_DRAW_RECORD_HEIGHT, next_height);
                if (ovl_s16(next_height) >= 97) {
                    next_height = 96u;
                    ae_wr16(r, record + AE_OVL18C_DRAW_RECORD_HEIGHT, next_height);
                }

                int32_t moving_y = ovl_s16(ae_rd16(r, record + AE_OVL18C_SURFACE_POS_Y));
                moving_y = ovl_s16((uint32_t)moving_y + next_height);
                ae_wr16(r, record + AE_OVL18C_SURFACE_POS_Y, (uint32_t)moving_y);
                int32_t stop_y = ovl_s16(ae_rd16(
                    r, record + AE_OVL18C_DRAW_RECORD_STOP_Y));
                if (stop_y < moving_y) {
                    uint32_t old_kind = ae_rd8(r, record + AE_OVL18C_DRAW_RECORD_TYPE);
                    ae_wr16(r, record + AE_OVL18C_SURFACE_POS_Y,
                            ae_rd16(r, record + AE_OVL18C_DRAW_RECORD_STOP_Y));
                    ae_wr8(r, record + AE_OVL18C_DRAW_RECORD_FRAME, 0);
                    ae_wr32(r, record + AE_OVL18C_DRAW_RECORD_TEXTURE, 0x070F20B0u);
                    ae_wr8(r, record + AE_OVL18C_DRAW_RECORD_TYPE,
                           old_kind + 1u);
                }
            }
        } else if (kind == 1u) {
            uint32_t phase = (ae_rd8(r, record + AE_OVL18C_DRAW_RECORD_TEXTURE_PHASE) + 8u) & 0xFFu;
            ae_wr8(r, record + AE_OVL18C_DRAW_RECORD_TEXTURE_PHASE, phase);
            if (ae_rd8(r, record + AE_OVL18C_DRAW_RECORD_FRAME) >= 3u)
                ae_wr8(r, record + AE_OVL18C_DRAW_RECORD_TIMER, 0);
        }

        ae_wr8(r, record + AE_OVL18C_DRAW_RECORD_FRAME,
               ae_rd8(r, record + AE_OVL18C_DRAW_RECORD_FRAME) + 1u);
    }

    uint32_t cursor = ae_rd32(r, AE_OVL18C_SURFACE_OT_CURSOR);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t record = ovl_draw_record(i);
        if (ae_rd8(r, record + AE_OVL18C_DRAW_RECORD_TIMER) == 0u) continue;
        uint32_t args[8] = {
            record,
            (uint32_t)ae_rds16(r, record + AE_OVL18C_SURFACE_POS_X),
            (uint32_t)ae_rds16(r, record + AE_OVL18C_SURFACE_POS_Y),
            (uint32_t)ae_rds16(r, record + AE_OVL18C_SURFACE_POS_Z),
            ae_rd8(r, record + AE_OVL18C_DRAW_RECORD_FRAME),
            ae_rd8(r, record + AE_OVL18C_DRAW_RECORD_TYPE),
            ae_rd16(r, record + AE_OVL18C_DRAW_RECORD_LIFETIME),
            cursor
        };
        cursor = (uint32_t)ovl_render(r, h, "Area02_EffectDrawRecord",
                                      AE_OVL18C_EFFECT_DRAW_RENDER_HOOK, 8, args);
        if (ae_ram_failed(r)) return 0;
    }
    ae_wr32(r, AE_OVL18C_SURFACE_OT_CURSOR, cursor);
    return 0;
}

/* 8013E950: animate the 230 surface texture records in game RAM. Projection
 * and packet emission are renderer boundaries; their outputs feed the same
 * visibility and animation branches as the original GTE results. */
int32_t ae_Area02_SurfaceDraw(AeRam *r, const AeOvl18cHooks *h) {
    uint32_t scratch = AE_OVL18C_SURFACE_SCRATCH;
    uint32_t source = AE_OVL18C_SURFACE_MATRIX_SOURCE;
    for (uint32_t i = 0; i < 8u; ++i)
        ae_wr8(r, scratch + AE_OVL18C_SURFACE_SCRATCH_ORIGIN_X + i,
               ae_rd8(r, source + i));

    for (uint32_t axis = 0; axis < 3u; ++axis) {
        uint32_t origin = scratch + AE_OVL18C_SURFACE_SCRATCH_ORIGIN_X + axis * 2u;
        uint32_t position = AE_OVL18C_SURFACE_MATRIX_POSITION_X + axis * 2u;
        ae_wr16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_POINT_X + axis * 2u,
                ae_rd16(r, position) - ae_rd16(r, origin));
    }

    (void)ae_sdk_VectorNormalSS(r, scratch + AE_OVL18C_SURFACE_SCRATCH_POINT_X,
                               scratch + AE_OVL18C_SURFACE_SCRATCH_POINT_X);
    for (uint32_t axis = 0; axis < 3u; ++axis) {
        uint32_t point = scratch + AE_OVL18C_SURFACE_SCRATCH_POINT_X + axis * 2u;
        uint32_t origin = scratch + AE_OVL18C_SURFACE_SCRATCH_ORIGIN_X + axis * 2u;
        int32_t step = ovl_asr(ovl_s16(ae_rd16(r, point)), 2);
        ae_wr16(r, origin, ae_rd16(r, origin) + (uint32_t)step);
    }

    int32_t origin_x = ovl_s16(ae_rd16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_ORIGIN_X));
    int32_t origin_y = ovl_s16(ae_rd16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_ORIGIN_Y));
    int32_t origin_z = ovl_s16(ae_rd16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_ORIGIN_Z));
    int32_t surface_type = ae_GetSurfaceType(
        r, origin_x, origin_y + AE_OVL18C_SURFACE_TYPE_Y_OFFSET, origin_z);
    if (ae_ram_failed(r)) return 0;

    int32_t surface_style = 0;
    uint32_t mode = ae_rd32(r, AE_OVL18C_SURFACE_MODE_FLAGS) & 0xF0u;
    int has_surface_height = ((uint32_t)surface_type & 0xF000u) != 0u;
    if ((ae_rd8(r, AE_OVL18C_SURFACE_GAME_MODE) == 1u && has_surface_height) ||
        mode == 0xC0u || (mode == 0x40u && has_surface_height)) {
        surface_style = -2061;
    } else {
        surface_style = -2573;
    }

    int32_t vertical_gate = (int16_t)surface_style;
    uint16_t scroll_u = ae_rd16(r, AE_OVL18C_SURFACE_OVERLAY_PHASE_U);
    uint16_t scroll_v = ae_rd16(r, AE_OVL18C_SURFACE_OVERLAY_PHASE_V);
    ae_wr16(r, AE_OVL18C_SURFACE_OVERLAY_PHASE_U, 0xFFFDu);
    ae_wr16(r, AE_OVL18C_SURFACE_OVERLAY_PHASE_V, 0xFFFDu);
    uint16_t phase_u = ae_rd16(r, AE_OVL18C_SURFACE_GLOBAL_PHASE_U);
    uint16_t phase_v = ae_rd16(r, AE_OVL18C_SURFACE_GLOBAL_PHASE_V);
    ae_wr16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_PHASE_U, phase_u);
    ae_wr16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_PHASE_V, phase_v);
    ovl_surface_setup_projection(r, h);
    if (ae_ram_failed(r)) return 0;

    uint32_t cursor = ae_rd32(r, AE_OVL18C_SURFACE_OT_CURSOR);
    for (uint32_t index = 0; index < AE_OVL18C_SURFACE_ANIM_RECORD_COUNT; ++index) {
        uint32_t record = AE_OVL18C_SURFACE_ANIM_RECORDS +
                          index * AE_OVL18C_SURFACE_ANIM_RECORD_SIZE;
        uint32_t state_address = record + AE_OVL18C_SURFACE_ANIM_STATE;
        int32_t state = (int8_t)ae_rd8(r, state_address);

        if (state == 0) {
            ae_wr8(r, state_address, 1);
            phase_u = (uint16_t)ovl_surface_phase_step(phase_u);
            phase_v = (uint16_t)ovl_surface_phase_step(phase_v);
            ae_wr16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_PHASE_U, phase_u);
            ae_wr16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_PHASE_V, phase_v);
            ae_wr16(r, record + AE_OVL18C_SURFACE_ANIM_POS_U,
                    (uint32_t)ovl_surface_phase_coordinate(phase_u, phase_v));

            phase_u = (uint16_t)ovl_surface_phase_step(phase_u);
            phase_v = (uint16_t)ovl_surface_phase_step(phase_v);
            ae_wr16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_PHASE_U, phase_u);
            ae_wr16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_PHASE_V, phase_v);
            ae_wr16(r, record + AE_OVL18C_SURFACE_ANIM_POS_V,
                    AE_OVL18C_SURFACE_ANIM_INITIAL_V);
            ae_wr16(r, record + AE_OVL18C_SURFACE_ANIM_POS_W,
                    (uint32_t)ovl_surface_phase_coordinate(phase_u, phase_v));

            phase_u = (uint16_t)ovl_surface_phase_step(phase_u);
            ae_wr16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_PHASE_U, phase_u);
            ae_wr8(r, record + AE_OVL18C_SURFACE_ANIM_INTENSITY,
                   ((phase_u >> 8) & 0x3Fu) + AE_OVL18C_SURFACE_PHASE_ORIGIN);
            state = 1;
        }

        if (state == 1) {
            uint16_t pos_u = (uint16_t)(ae_rd16(r, record + AE_OVL18C_SURFACE_ANIM_POS_U) + scroll_u);
            uint16_t pos_v = (uint16_t)(ae_rd16(r, record + AE_OVL18C_SURFACE_ANIM_POS_V) +
                                        ae_rd8(r, record + AE_OVL18C_SURFACE_ANIM_INTENSITY));
            uint16_t pos_w = (uint16_t)(ae_rd16(r, record + AE_OVL18C_SURFACE_ANIM_POS_W) + scroll_v);
            ae_wr16(r, record + AE_OVL18C_SURFACE_ANIM_POS_U, pos_u);
            ae_wr16(r, record + AE_OVL18C_SURFACE_ANIM_POS_V, pos_v);
            ae_wr16(r, record + AE_OVL18C_SURFACE_ANIM_POS_W, pos_w);

            int32_t candidate_y = ovl_s16(pos_v) +
                ovl_s16(ae_rd16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_ORIGIN_Y));
            if (vertical_gate < candidate_y) {
                uint16_t wrapped_u = (uint16_t)(pos_u + 1023u);
                int32_t signed_w = ovl_s16(pos_w);
                if (wrapped_u >= 2047u || signed_w < -1023 || signed_w >= 1024) {
                    ae_wr8(r, state_address, 0);
                    continue;
                }
                int32_t reset_v = surface_style -
                    ovl_s16(ae_rd16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_ORIGIN_Y));
                ae_wr16(r, record + AE_OVL18C_SURFACE_ANIM_POS_V,
                        (uint32_t)reset_v);
                ae_wr8(r, state_address, 2);
                phase_u = (uint16_t)ovl_surface_phase_step(
                    ae_rd16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_PHASE_U));
                phase_v = (uint16_t)ovl_surface_phase_step(
                    ae_rd16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_PHASE_V));
                ae_wr16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_PHASE_U, phase_u);
                ae_wr16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_PHASE_V, phase_v);
                state = 2;
            }
        }

        uint32_t point = scratch + AE_OVL18C_SURFACE_SCRATCH_POINT_X;
        uint32_t origin = scratch + AE_OVL18C_SURFACE_SCRATCH_ORIGIN_X;
        ae_wr16(r, point,
                ae_rd16(r, origin) + ae_rd16(r, record + AE_OVL18C_SURFACE_ANIM_POS_U));
        ae_wr16(r, point + 2u,
                ae_rd16(r, origin + 2u) + ae_rd16(r, record + AE_OVL18C_SURFACE_ANIM_POS_V));
        ae_wr16(r, point + 4u,
                ae_rd16(r, origin + 4u) + ae_rd16(r, record + AE_OVL18C_SURFACE_ANIM_POS_W));

        uint32_t packed_xy = ovl_surface_projection_args(
            r, h, "Area02_SurfaceProjectPoint",
            AE_OVL18C_SURFACE_HOOK_PROJECT_POINT,
            scratch + AE_OVL18C_SURFACE_SCRATCH_PROJECTED_XY,
            scratch + AE_OVL18C_SURFACE_SCRATCH_DEPTH);
        if (ae_ram_failed(r)) return 0;

        uint32_t screen_x = packed_xy & 0xFFFFu;
        int32_t screen_y = ovl_s16(packed_xy >> 16);
        int32_t depth = (int32_t)ae_rd32(r, scratch + AE_OVL18C_SURFACE_SCRATCH_DEPTH);
        ae_wr32(r, scratch + AE_OVL18C_SURFACE_SCRATCH_SCREEN_XY, screen_x);
        ae_wr32(r, scratch + AE_OVL18C_SURFACE_SCRATCH_SCREEN_Y, (uint32_t)screen_y);
        if (((screen_x + 192u) & 0xFFFFu) >= 769u || depth <= 0 || depth >= 2048) {
            ae_wr8(r, state_address, 0);
            continue;
        }

        state = (int8_t)ae_rd8(r, state_address);
        if (state == 1) {
            uint32_t point_x = scratch + AE_OVL18C_SURFACE_SCRATCH_POINT_X;
            int32_t intensity = ae_rd8(r, record + AE_OVL18C_SURFACE_ANIM_INTENSITY);
            ae_wr16(r, point_x,
                    ae_rd16(r, point_x) - scroll_u);
            ae_wr16(r, point_x + 2u,
                    ae_rd16(r, point_x + 2u) - (uint32_t)((intensity * 3) / 2));
            ae_wr16(r, point_x + 4u,
                    ae_rd16(r, point_x + 4u) - scroll_v);

            (void)ovl_surface_projection_args(
                r, h, "Area02_SurfaceProjectVertex",
                AE_OVL18C_SURFACE_HOOK_PROJECT_VERTEX,
                scratch + AE_OVL18C_SURFACE_SCRATCH_VERTEX_XY, 0);
            if (ae_ram_failed(r)) return 0;
            uint32_t args[8] = {
                record,
                (uint32_t)ovl_s16(ae_rd16(r, record + AE_OVL18C_SURFACE_ANIM_POS_U)),
                (uint32_t)ovl_s16(ae_rd16(r, record + AE_OVL18C_SURFACE_ANIM_POS_V)),
                (uint32_t)ovl_s16(ae_rd16(r, record + AE_OVL18C_SURFACE_ANIM_POS_W)),
                ae_rd32(r, scratch + AE_OVL18C_SURFACE_SCRATCH_DEPTH),
                ae_rd32(r, scratch + AE_OVL18C_SURFACE_SCRATCH_PROJECTED_XY),
                ae_rd32(r, scratch + AE_OVL18C_SURFACE_SCRATCH_VERTEX_XY),
                cursor
            };
            (void)ovl_render(r, h, "Area02_SurfaceSubmitPrimary",
                             AE_OVL18C_SURFACE_HOOK_SUBMIT_PRIMARY, 8, args);
            if (ae_ram_failed(r)) return 0;
            cursor += 24u;
        } else if (state < 5) {
            ae_wr8(r, state_address, state + 1);
            uint32_t width = (uint32_t)AE_OVL18C_SURFACE_RENDER_PACKET_WIDTH /
                             (uint32_t)depth;
            uint32_t height = (uint32_t)AE_OVL18C_SURFACE_RENDER_PACKET_HEIGHT /
                              (uint32_t)depth;
            uint32_t args[11] = {
                record,
                (uint32_t)ovl_s16(ae_rd16(r, record + AE_OVL18C_SURFACE_ANIM_POS_U)),
                (uint32_t)ovl_s16(ae_rd16(r, record + AE_OVL18C_SURFACE_ANIM_POS_V)),
                (uint32_t)ovl_s16(ae_rd16(r, record + AE_OVL18C_SURFACE_ANIM_POS_W)),
                ae_rd32(r, scratch + AE_OVL18C_SURFACE_SCRATCH_PROJECTED_XY),
                (uint32_t)depth,
                width,
                height,
                cursor,
                ae_rd8(r, state_address),
                ae_rd8(r, record + AE_OVL18C_SURFACE_ANIM_INTENSITY)
            };
            (void)ovl_render(r, h, "Area02_SurfaceSubmitSecondary",
                             AE_OVL18C_SURFACE_HOOK_SUBMIT_SECONDARY, 11, args);
            if (ae_ram_failed(r)) return 0;
            cursor += 40u;
        } else {
            ae_wr8(r, state_address, 0);
        }
    }

    ae_wr32(r, AE_OVL18C_SURFACE_OT_CURSOR, cursor);
    ae_wr16(r, AE_OVL18C_SURFACE_GLOBAL_PHASE_U,
            ae_rd16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_PHASE_U));
    ae_wr16(r, AE_OVL18C_SURFACE_GLOBAL_PHASE_V,
            ae_rd16(r, scratch + AE_OVL18C_SURFACE_SCRATCH_PHASE_V));
    return 0;
}
